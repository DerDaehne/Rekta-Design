/* anim.c: implicit animations. Setters change the model value; inside an rk_animate_begin … rk_animate_commit block
   the presented value travels to it with the block's motion (docs/motion.md). Layout changes in a block animate
   too: every view the layout moves or resizes glides from where it was drawn (FLIP). */
#include "rk_internal.h"

typedef struct group { void (*fn)(void *); void *user; int pending; bool committed; } group;

typedef struct anim {
  rk_view *view; int prop, n;
  float from[10], to[10];
  double start, delay, dur;
  const float *ease;
  void (*done)(rk_view *v, void *user); void *user;
  group *grp;
  bool spring; rk_glide g;                                 /* a glide instead of the eased line */
} anim;

static anim *anims; static int nanims, acap;
static rk_anim_ctx ctx[16]; static int depth;
static group *ctx_group[16];

bool rk__animating_block(void) { return depth > 0; }
rk_motion rk__current_motion(void) { return depth ? ctx[depth - 1].motion : RK_MOTION_STATE; }
bool rk__anim_running(void) { return nanims > 0; }
uint32_t rk__anim_count(void) { return (uint32_t)nanims; }

double rk__motion_duration(rk_motion m, bool tile) {
  switch (m) {
    case RK_MOTION_REFLOW: return rk_duration_of(RK_DURATION_MODERATE);
    case RK_MOTION_ENTER: return rk_duration_of(tile ? RK_DURATION_MODERATE : RK_DURATION_SHORT);
    case RK_MOTION_LEAVE: return 0.6 * rk_duration_of(RK_DURATION_SHORT);
    case RK_MOTION_MORPH: return rk_duration_of(RK_DURATION_LONG);
    default: return rk_duration_of(RK_DURATION_SHORT);
  }
}

const float *rk__motion_ease(rk_motion m) {
  return m == RK_MOTION_ENTER ? rk__ease_in : m == RK_MOTION_LEAVE ? rk__ease_out : rk__ease_base;
}

static void apply(anim *a, float t) {
  rk_view *v = a->view;
  float k = a->prop == RK_PROP_CONTENT || a->prop == RK_PROP_TRANSITION ? t : rk__ease(a->ease, t), x[10];
  for (int i = 0; i < a->n; i++) x[i] = a->from[i] + (a->to[i] - a->from[i]) * k;
  if (a->spring) x[0] = rk__glide_at(&a->g, a->start + t * a->dur, NULL, NULL);
  switch (a->prop) {
    case RK_PROP_FRAME: v->pframe = (rk_rect){ x[0], x[1], x[2], x[3] }; break;
    case RK_PROP_OPACITY: v->popacity = x[0]; break;
    case RK_PROP_TRANSFORM: memcpy(&v->ptransform, x, sizeof v->ptransform); break;
    case RK_PROP_FILL: {
      rk_color c = 0;
      for (int i = 0; i < 4; i++) c |= (rk_color)RK_CLAMP((int)(x[i] + 0.5f), 0, 255) << (24 - 8 * i);
      v->pfill = c;
      break;
    }
    case RK_PROP_CONTENT: v->content_alpha = t < 0.55f ? 0 : (t - 0.55f) / 0.45f; break;
    case RK_PROP_TRANSITION: v->transition_t = t; break;
    case RK_PROP_VALUE: v->pvalue = x[0]; break;
    default: break;
  }
}

static void group_done(group *g) {
  if (!g || --g->pending > 0 || !g->committed) return;
  void (*fn)(void *) = g->fn; void *user = g->user;
  free(g);
  if (fn) fn(user);
}

void rk__anim_cancel(rk_view *v, int prop) {
  for (int i = 0; i < nanims; i++) if (anims[i].view == v && anims[i].prop == prop) {
    anim a = anims[i];
    anims[i] = anims[--nanims];
    v->anims--;
    if (a.prop == RK_PROP_FILL) v->fill_anim = false;
    if (a.done) a.done(v, a.user);
    group_done(a.grp);
    return;
  }
}

void rk__animate_cb(rk_view *v, int prop, const float *from, const float *to, int n, double dur, double delay, const float *ease,
                    void (*done)(rk_view *, void *), void *user) {
  rk__anim_cancel(v, prop);
  anim a;
  memset(&a, 0, sizeof a);
  a.view = v; a.prop = prop; a.n = n; a.dur = dur; a.delay = delay; a.ease = ease ? ease : rk__ease_base;
  memcpy(a.from, from, (size_t)n * sizeof(float)); memcpy(a.to, to, (size_t)n * sizeof(float));
  a.done = done; a.user = user;
  if (prop == RK_PROP_FILL) v->fill_anim = dur > 0 || delay > 0;
  if (dur <= 0 && delay <= 0) { apply(&a, 1); if (done) done(v, user); return; }
  a.start = rk_app_now();
  a.grp = depth ? ctx_group[depth - 1] : NULL;
  if (a.grp) a.grp->pending++;
  apply(&a, 0);
  anims = rk__grow(anims, &acap, nanims + 1, sizeof *anims);
  anims[nanims++] = a;
  v->anims++;
  rk__view_dirty(v);
  rk__app_request_frame();
}

void rk__animate(rk_view *v, int prop, const float *from, const float *to, int n, double dur, double delay, const float *ease) {
  rk__animate_cb(v, prop, from, to, n, dur, delay, ease, NULL, NULL);
}

void rk__anim_view_gone(rk_view *v) {
  for (int i = 0; i < nanims;) {
    if (anims[i].view == v) { group *g = anims[i].grp; anims[i] = anims[--nanims]; group_done(g); }
    else i++;
  }
}

/* A glide: a quintic from where the value is, with the speed and acceleration it has, to the target at rest. It
   starts and ends without a jolt (acceleration zero at both ends), and a new target picks it up mid-way with no break
   in speed or acceleration. */
float rk__glide_at(const rk_glide *g, double now, float *vel, float *acc) {
  if (g->dur <= 0 || now >= g->t0 + g->dur) { if (vel) *vel = 0; if (acc) *acc = 0; return g->p1; }
  double T = g->dur / 1000.0, s = RK_MAX(0.0, (now - g->t0) / g->dur), s2 = s * s, s3 = s2 * s, s4 = s3 * s, s5 = s4 * s;
  double p0 = g->p0, p1 = g->p1, v0 = g->v0, a0 = g->a0;
  double H0 = 1 - 10 * s3 + 15 * s4 - 6 * s5, H1 = s - 6 * s3 + 8 * s4 - 3 * s5, H2 = 0.5 * s2 - 1.5 * s3 + 1.5 * s4 - 0.5 * s5, H5 = 10 * s3 - 15 * s4 + 6 * s5;
  if (vel) {
    double D0 = -30 * s2 + 60 * s3 - 30 * s4, D1 = 1 - 18 * s2 + 32 * s3 - 15 * s4, D2 = s - 4.5 * s2 + 6 * s3 - 2.5 * s4;
    *vel = (float)((p1 - p0) * -D0 / T + v0 * D1 + a0 * T * D2);
  }
  if (acc) {
    double E0 = -60 * s + 180 * s2 - 120 * s3, E1 = -36 * s + 96 * s2 - 60 * s3, E2 = 1 - 9 * s + 18 * s2 - 10 * s3;
    *acc = (float)((p1 - p0) * -E0 / (T * T) + v0 * E1 / T + a0 * E2);
  }
  return (float)(p0 * H0 + v0 * T * H1 + a0 * T * T * H2 + p1 * H5);
}

void rk__glide_to(rk_glide *g, double now, float target, double dur) {
  float v, a, p = rk__glide_at(g, now, &v, &a);
  g->p0 = p; g->v0 = v; g->a0 = a; g->p1 = target; g->t0 = now; g->dur = rk__reduced() ? 0 : dur;
}

void rk__glide_set(rk_glide *g, float value) { memset(g, 0, sizeof *g); g->p0 = g->p1 = value; }
bool rk__glide_moving(const rk_glide *g, double now) { return g->dur > 0 && now < g->t0 + g->dur; }

/* Moves the value to target in about settle ms (a little longer the further it goes); a glide already on it is
   picked up where it is. */
void rk__animate_spring(rk_view *v, int prop, float target, double settle) {
  double now = rk_app_now();
  anim *a = NULL;
  for (int i = 0; i < nanims; i++) if (anims[i].view == v && anims[i].prop == prop && anims[i].spring) a = &anims[i];
  float p = a ? rk__glide_at(&a->g, now, NULL, NULL) : v->pvalue;
  if (settle <= 0 || rk__reduced() || (!a && fabsf(p - target) < 0.2f)) { rk__anim_cancel(v, prop); v->pvalue = target; rk__view_dirty(v); return; }
  double dur = settle * RK_CLAMP(sqrt(fabs(target - p) / 600.0), 0.8, 1.5);
  if (!a) {
    rk__animate(v, prop, &p, &target, 1, dur, 0, NULL);
    a = &anims[nanims - 1];
    a->spring = true;
    rk__glide_set(&a->g, p);
  }
  rk__glide_to(&a->g, now, target, dur);
  a->start = now; a->delay = 0; a->dur = dur;
  rk__app_request_frame();
}

bool rk__anim_tick(double now) {
  static anim *finished; static int fcap;
  int nf = 0;
  for (int i = 0; i < nanims;) {
    anim *a = &anims[i];
    double t = a->dur > 0 ? (now - a->start - a->delay) / a->dur : 1;
    if (now < a->start + a->delay) t = 0;
    /* where it was drawn, and where it is now; a move, a fade or a turn leaves the view's own layer as it is */
    bool moves = a->prop == RK_PROP_FRAME || a->prop == RK_PROP_OPACITY || a->prop == RK_PROP_TRANSFORM;
    if (moves) rk__view_moved(a->view); else rk__view_dirty(a->view);
    apply(a, (float)RK_CLAMP(t, 0.0, 1.0));
    if (moves) rk__view_moved(a->view); else rk__view_dirty(a->view);
    if (t >= 1) {
      finished = rk__grow(finished, &fcap, nf + 1, sizeof *finished);
      finished[nf++] = *a;
      rk_retain(a->view);                                      /* a callback may let it go; hold it until all ran */
      a->view->anims--;
      anims[i] = anims[--nanims];
    } else i++;
  }
  for (int i = 0; i < nf; i++) {
    anim *a = &finished[i];
    if (a->prop == RK_PROP_CONTENT) a->view->content_alpha = 1;
    if (a->prop == RK_PROP_FILL) a->view->fill_anim = false;
    if (a->view->anims == 0 && a->view->layer) { rk__bitmap_free(a->view->layer); a->view->layer = NULL; }
    if (a->done) a->done(a->view, a->user);
    group_done(a->grp);
  }
  for (int i = 0; i < nf; i++) rk_release(finished[i].view);
  return nanims > 0;
}

/* Blocks ---------------------------------------------------------------------------------------------------------- */

void rk_animate_begin(rk_motion motion) {
  if (depth >= RK_COUNT(ctx)) return;
  ctx[depth].motion = motion;
  ctx_group[depth] = NULL;
  depth++;
}

void rk_animate_on_done(void (*fn)(void *), void *user) {
  if (!depth) { if (fn) fn(user); return; }
  group *g = ctx_group[depth - 1];
  if (!g) { g = rk__alloc(sizeof *g); ctx_group[depth - 1] = g; }
  g->fn = fn; g->user = user;
}

void rk_animate_commit(void) {
  if (!depth) return;
  if (depth == 1) rk__layout_pending();                    /* the layout runs inside the block, so it animates */
  group *g = ctx_group[depth - 1];
  depth--;
  if (g) { g->committed = true; g->pending++; group_done(g); }
}

/* Frames: the layout's way of placing a view ---------------------------------------------------------------------- */

static void entered(rk_view *v, void *user) { (void)user; v->entering = false; }

void rk__set_frame(rk_view *v, rk_rect f) {
  bool first = !v->placed;
  v->frame = f;
  v->placed = true;
  bool animate = depth > 0 && rk__view_window(v) && !rk__reduced();
  if (v->entering) {
    rk__anim_cancel(v, RK_PROP_FRAME);
    v->pframe = f;
    if (!animate) { v->entering = false; return; }
    bool tile = rk__is_tile(v);
    double d = rk__motion_duration(RK_MOTION_ENTER, tile);
    float o0 = 0, o1 = v->opacity;
    rk__animate(v, RK_PROP_OPACITY, &o0, &o1, 1, d, 0, rk__ease_in);
    rk_transform t0 = v->transform, t1 = v->transform;
    if (tile) { t0.translate_y += 6; t0.scale_x *= 0.96f; t0.scale_y *= 0.96f; } else t0.translate_y -= 6;
    rk__animate_cb(v, RK_PROP_TRANSFORM, &t0.translate_x, &t1.translate_x, 10, d, 0, rk__ease_in, entered, NULL);
    return;
  }
  if (first || !animate) { rk__anim_cancel(v, RK_PROP_FRAME); v->pframe = f; return; }
  rk_rect p = v->pframe;
  if (fabsf(p.x - f.x) < 0.25f && fabsf(p.y - f.y) < 0.25f && fabsf(p.w - f.w) < 0.25f && fabsf(p.h - f.h) < 0.25f) {
    rk__anim_cancel(v, RK_PROP_FRAME);
    v->pframe = f;
    return;
  }
  rk_motion m = rk__current_motion();
  double d = rk__motion_duration(RK_MOTION_REFLOW, false);
  double delay = m == RK_MOTION_LEAVE ? rk__motion_duration(RK_MOTION_LEAVE, false) : 0;
  rk__animate(v, RK_PROP_FRAME, &p.x, &f.x, 4, d, delay, rk__ease_base);
  /* a box that changes size fades its content back in near the end, so text is never seen stretched; a container
     only makes room, its children move on their own */
  if (v->cls && (fabsf(p.w - f.w) > 1 || fabsf(p.h - f.h) > 1)) {
    float a = 0, b = 1;
    rk__animate(v, RK_PROP_CONTENT, &a, &b, 1, d, delay, NULL);
  }
}

/* Transitions: the old look is kept as a picture and the change animates from it ------------------------------------ */

static void transition_done(rk_view *v, void *user) {
  (void)user;
  v->transition = 0; v->transition_t = 0;
  rk__bitmap_free(v->snapshot); v->snapshot = NULL;
}

void rk_view_transition(rk_view *v, rk_transition kind) {
  if (!rk__view_visible(v) || rk__reduced()) return;
  rk__bitmap_free(v->snapshot);
  v->snapshot = rk__snapshot(v, kind == RK_TRANSITION_FLIP);
  if (!v->snapshot) return;
  v->transition = (int)kind + 1;
  double d = kind == RK_TRANSITION_CROSSFADE ? rk_duration_of(RK_DURATION_SHORT) : rk_duration_of(RK_DURATION_MODERATE);
  float a = 0, b = 1;
  rk__animate_cb(v, RK_PROP_TRANSITION, &a, &b, 1, d, 0, NULL, transition_done, NULL);
}

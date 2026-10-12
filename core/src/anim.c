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
} anim;

static anim *anims; static int nanims, acap;
static rk_anim_ctx ctx[16]; static int depth;
static group *ctx_group[16];

bool rk__animating_block(void) { return depth > 0; }
rk_motion rk__current_motion(void) { return depth ? ctx[depth - 1].motion : RK_MOTION_STATE; }
bool rk__anim_running(void) { return nanims > 0; }

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

bool rk__anim_tick(double now) {
  static anim *finished; static int fcap;
  int nf = 0;
  for (int i = 0; i < nanims;) {
    anim *a = &anims[i];
    double t = a->dur > 0 ? (now - a->start - a->delay) / a->dur : 1;
    if (now < a->start + a->delay) t = 0;
    apply(a, (float)RK_CLAMP(t, 0.0, 1.0));
    rk__view_dirty(a->view);
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

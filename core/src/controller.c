/* controller.c: controllers own views and come and go; places, panoramas and pivots contain them; sheets are
   presented over them. The motions follow motion.md: a place change turns, a facet change slides, a sheet comes
   from the top edge. */
#include "rk_internal.h"
#include "rk_tokens.h"

enum { K_PLAIN, K_PLACES, K_PANORAMA, K_PIVOT };

struct rk_controller {
  rk_object obj;
  rk_controller_class cls; void *state;
  int kind; void *impl; void (*impl_free)(void *);
  rk_view *view; char *title;
  rk_controller *parent;
  rk_window *window;
  rk_controller *sheet, *presenter;                        /* what it presents, what presented it */
  rk_view *sheet_shade, *sheet_box, *focus_before;
  bool appeared;
};

static void controller_destroy(void *self) {
  rk_controller *c = self;
  if (c->sheet) rk_release(c->sheet);
  if (c->impl_free) c->impl_free(c->impl);
  if (c->view) { c->view->controller = NULL; rk_release(c->view); }
  if (c->cls.destroy) c->cls.destroy(c->state);
  free(c->title);
}
static const rk_type controller_type = { "rk_controller", controller_destroy };

rk_controller *rk_controller_create(const rk_controller_class *cls, void *state) {
  rk_controller *c = rk__new(&controller_type, sizeof *c);
  if (cls) memcpy(&c->cls, cls, RK_MIN((size_t)cls->size, sizeof c->cls));
  c->cls.size = sizeof c->cls;
  c->state = state;
  return c;
}

rk_controller *rk_controller_create_with_view(rk_view *view, const char *title) {
  rk_controller *c = rk_controller_create(NULL, NULL);
  c->view = view;                                              /* takes the caller's reference */
  view->controller = c;
  rk__setstr(&c->title, title);
  return c;
}

void *rk_controller_state(rk_controller *c) { return c->state; }
void rk_controller_set_title(rk_controller *c, const char *t) { rk__setstr(&c->title, t); if (c->window && c->window->titlebar) rk__titlebar_update(c->window->titlebar); }
rk_controller *rk_controller_parent(rk_controller *c) { return c->parent; }
rk_window *rk_controller_window(rk_controller *c) { return c->window; }

rk_view *rk_controller_view(rk_controller *c) {
  if (!c->view) {
    c->view = c->cls.load ? c->cls.load(c, c->state) : NULL;
    if (!c->view) c->view = rk_view_create();
    c->view->controller = c;
  }
  return c->view;
}

static rk_controller *visible_child(rk_controller *c);
static rk_controller *pivot_visible(rk_controller *c);

const char *rk_controller_title(rk_controller *c) {
  rk_controller *v = visible_child(c);
  if (v && v != c) { const char *t = rk_controller_title(v); if (t && *t) return t; }
  return c->title;
}

void rk__controller_attach(rk_controller *c, rk_window *w) {
  c->window = w;
  rk_controller *v = visible_child(c);
  if (v && v != c) rk__controller_attach(v, w);
}

void rk__controller_appear(rk_controller *c, bool appear) {
  if (c->appeared == appear) return;
  c->appeared = appear;
  rk_controller *v = visible_child(c);
  if (!appear && v && v != c) rk__controller_appear(v, false);
  if (appear && c->cls.appear) c->cls.appear(c, c->state);
  if (!appear && c->cls.disappear) c->cls.disappear(c, c->state);
  if (appear && v && v != c) rk__controller_appear(v, true);
}

/* Places ---------------------------------------------------------------------------------------------------------- */

typedef struct turn { rk_view **units; int n, cap; } turn;   /* the units of a place turning */
static void turn_release(turn *t) { for (int i = 0; i < t->n; i++) rk_release(t->units[i]); t->n = 0; }

typedef struct places {
  rk_controller *self;
  rk_controller **stack; int n, cap;
  rk_view *focus_return[64];
  turn out;                                                /* the units of the place leaving */
  rk_controller *leaving;                                  /* owned while it turns away after back */
  bool leaving_owned;
  uint32_t timer;
  struct swap *pending;
} places;

static void places_free(void *p) {
  places *pl = p;
  if (pl->timer) rk_app_cancel(pl->timer);
  free(pl->pending);
  if (pl->leaving_owned) rk_release(pl->leaving);
  for (int i = 0; i < 64; i++) rk_release(pl->focus_return[i]);
  for (int i = 0; i < pl->n; i++) rk_release(pl->stack[i]);
  turn_release(&pl->out);
  free(pl->stack); free(pl->out.units); free(pl);
}

static void fill_layout(rk_view *v, void *st) {
  (void)st;
  rk_insets p = v->padding;
  for (int i = 0; i < v->nkids; i++) rk__set_frame(v->kids[i], (rk_rect){ p.left, p.top, RK_MAX(0.0f, v->frame.w - p.left - p.right), RK_MAX(0.0f, v->frame.h - p.top - p.bottom) });
}
static const rk_class fill_class = { { sizeof(rk_view_class), "rk_fill", NULL, NULL, fill_layout, NULL, NULL }, NULL, NULL };

static rk_controller *visible_child(rk_controller *c) {
  if (c->kind == K_PLACES) { places *p = c->impl; return p->n ? p->stack[p->n - 1] : NULL; }
  if (c->kind == K_PIVOT) return pivot_visible(c);
  return c;
}

rk_controller *rk_places(rk_controller *home) {
  rk_controller *c = rk_controller_create(NULL, NULL);
  places *p = rk__alloc(sizeof *p);
  p->self = c;
  c->kind = K_PLACES; c->impl = p; c->impl_free = places_free;
  c->view = rk__view_create(&fill_class, NULL);
  c->view->controller = c;
  if (home) {
    p->stack = rk__grow(p->stack, &p->cap, 1, sizeof *p->stack);
    p->stack[p->n++] = home;                                   /* takes the caller's reference */
    home->parent = c;
    rk__view_add(c->view, rk_controller_view(home));
  }
  return c;
}

int rk_places_depth(rk_controller *c) { return c->kind == K_PLACES ? ((places *)c->impl)->n : 0; }

/* The turn: a place change (motion.md). Its units are the parts a place shows, top to bottom, a row at a time. Each turns around the leading edge of the box that clips it, in a time that follows its size: a
   small unit turns quicker than a large one, as something light starts and stops sooner than something heavy. */

/* A view turns as one when it is a tile or shows something of its own (a class that draws, a fill, a rule); a plain
   layout box lets its children turn one by one, so each row, label and button keeps its heft, and so does one that
   clips: its parts turn from its edge. Parts out of the window's sight stand still. */
static bool turns_whole(rk_view *k) {
  if (rk__is_tile(k) || !k->nkids || k->rule_top || k->rule_bottom || k->fill_role || (k->fill[0] & 0xff)) return true;
  return k->cls && k->cls->pub.draw;
}

static void collect_units(rk_view *v, turn *t) {
  for (int i = 0; i < v->nkids; i++) {
    rk_view *k = v->kids[i];
    if (k->hidden || !rk__view_visible(k)) continue;
    rk_window *w = rk__view_window(k);
    rk_rect r = rk__window_rect(k);
    if (w && (r.x >= w->size.w || r.y >= w->size.h || r.x + r.w <= 0 || r.y + r.h <= 0)) continue;
    if (turns_whole(k)) {
      t->units = rk__grow(t->units, &t->cap, t->n + 1, sizeof *t->units);
      t->units[t->n++] = rk_retain(k);
    } else collect_units(k, t);
  }
}

static int unit_order(const void *a, const void *b) {
  rk_rect p = rk__window_rect(*(rk_view *const *)a), q = rk__window_rect(*(rk_view *const *)b);
  int rp = (int)lroundf(p.y / 8), rq = (int)lroundf(q.y / 8);
  if (rp != rq) return rp - rq;
  return p.x < q.x ? -1 : p.x > q.x;
}

static rk_view *clip_box(rk_view *u, rk_view *place) {
  for (rk_view *p = u->parent; p; p = p->parent) if (p->clip || p == place) return p;
  return place;
}

static float turn_perspective(float reach) { return roundf(RK_TOK_TURNSTILE_PERSPECTIVE + 1.2f * RK_MAX(0.0f, reach)); }

/* A unit's time against the token's grows with its area, against a tile's (150 x 150): 0.8 x (area / tile)^0.35,
   from 0.5 (a line of text, about 0.6) through 0.8 (a tile) and 1.0 (a wide tile) to 1.4 (a table). */
static double heft(rk_rect r) { float k = 0.8f * powf(RK_MAX(1.0f, r.w * r.h) / 22500.0f, 0.35f); return RK_CLAMP(k, 0.5f, 1.4f); }

/* The turn as rekta.css has it: leaving parts swing 60 degrees, gathering speed, whole for 40% of their time;
   arriving parts swing in from 70 degrees, quick at first, and land with a trace of speed left, so the motion ends
   when the animation does. Heft also sets when a part starts: (heft - 0.5) x 6 x duration-stagger after the turn
   begins, light parts first, heavy ones later. Parts of one size (heft within 0.05) follow one another in reading
   order, half a duration-stagger apart, six steps at most: the size leads, the order only sorts what weighs the
   same. */
#define TURN_OUT_ANGLE 60.0f
#define TURN_IN_ANGLE 70.0f
#define TURN_CLASSES 32                                    /* heft x 20, up to 1.4 x 20 = 28 */
static void turn_starts(turn *t, int n, rk_view *pressed, double *hs, double *at) {
  int seen[TURN_CLASSES] = { 0 };
  double step = rk_duration_of(RK_DURATION_STAGGER);
  for (int i = 0; i < n; i++) {
    hs[i] = heft(rk__window_rect(t->units[i]));
    if (t->units[i] == pressed) { at[i] = 0; continue; }
    int key = RK_CLAMP((int)lround(hs[i] * 20), 0, TURN_CLASSES - 1), k = seen[key]++;
    at[i] = (hs[i] - 0.5) * 6 * step + RK_MIN(k, 6) * step / 2;
  }
}
static const float turn_ease_out[4] = { 0.35f, 0.0f, 0.65f, 0.6f }, turn_ease_in[4] = { 0.22f, 0.9f, 0.6f, 0.98f };

/* The units of a place in turning order, the pressed one last; each one's row, and how far each row reaches from
   its clip box's leading edge (the farther, the flatter the perspective). */
static int turn_gather(turn *t, rk_view *place, rk_view *pressed, int *slots, float *reach) {
  turn_release(t);
  collect_units(place, t);
  qsort(t->units, (size_t)t->n, sizeof *t->units, unit_order);
  for (int i = 0; i < t->n; i++) if (t->units[i] == pressed) {
    memmove(t->units + i, t->units + i + 1, (size_t)(t->n - i - 1) * sizeof *t->units);
    t->units[t->n - 1] = pressed;
    break;
  }
  int slot = -1, key = INT32_MIN, n = RK_MIN(t->n, 1024);
  for (int i = 0; i < n; i++) {
    rk_view *u = t->units[i];
    rk_rect r = rk__window_rect(u), cr = rk__window_rect(clip_box(u, place));
    int k = u == pressed ? INT32_MAX : (int)lroundf(r.y / 8);
    if (k != key) { slot++; key = k; if (slot < 256) reach[slot] = 0; }
    slots[i] = slot;
    if (slot < 256) reach[slot] = RK_MAX(reach[slot], r.x + r.w - cr.x);
  }
  return n;
}

/* where a unit turns from: the leading edge of its clip box, at its own middle, or its top when flush with the box's */
static void turn_pivot(rk_transform *t, rk_view *u, rk_view *place, float reach) {
  rk_rect r = rk__window_rect(u), cr = rk__window_rect(clip_box(u, place));
  t->origin_x = r.w > 0 ? (cr.x - r.x) / r.w : 0;
  t->origin_y = fabsf(r.y - cr.y) < 2 ? 0 : 0.5f;
  t->perspective = turn_perspective(reach);
}

/* Leaving: each unit swings toward the viewer, then fades, light ones first, over
   duration-short by its heft; the pressed tile keeps its press and goes last. Returns when the place can go: its last
   unit 75% through, fading, so the next swings in without a gap. */
static double turn_out(turn *t, rk_view *place, rk_view *pressed, bool back) {
  static int slots[1024]; static float reach[256];
  int n = turn_gather(t, place, pressed, slots, reach);
  double out = rk_duration_of(RK_DURATION_SHORT), end = 0;
  float angle = back ? TURN_OUT_ANGLE : -TURN_OUT_ANGLE;
  static double hs[1024], starts[1024];
  double last = 0;                                         /* the pressed tile goes when the last other does */
  turn_starts(t, n, pressed, hs, starts);
  for (int i = 0; i < n; i++) if (t->units[i] != pressed) last = RK_MAX(last, starts[i]);
  for (int i = 0; i < n; i++) {
    rk_view *u = t->units[i];
    double h = hs[i], at = u == pressed ? last : starts[i], d = out * h;
    if (u == pressed) at = RK_MAX(at, rk_duration_of(RK_DURATION_PRESS) - (rk_app_now() - u->press_since));
    rk_transform from = u->ptransform, to = u->transform;
    turn_pivot(&to, u, place, slots[i] < 256 ? reach[slots[i]] : rk__window_rect(u).w);
    to.rotate_y = angle; to.rotate_x = 0; to.scale_x = to.scale_y = 1;
    if (u != pressed) { from = u->transform; from.origin_x = to.origin_x; from.origin_y = to.origin_y; from.perspective = to.perspective; }
    float o0 = u->popacity, o1 = 0;
    rk__animate(u, RK_PROP_TRANSFORM, &from.translate_x, &to.translate_x, 10, d, at, turn_ease_out);
    rk__animate(u, RK_PROP_OPACITY, &o0, &o1, 1, 0.6 * d, at + 0.4 * d, rk__ease_base);
    u->pressed = 0;
    end = RK_MAX(end, at + 0.75 * d);
  }
  return end;
}

/* The place is gone: its units stand as they were, for when it comes back. */
static void turn_reset(turn *t) {
  for (int i = 0; i < t->n; i++) {
    rk_view *u = t->units[i];
    rk__anim_cancel(u, RK_PROP_TRANSFORM); rk__anim_cancel(u, RK_PROP_OPACITY);
    u->ptransform = u->transform; u->popacity = u->opacity;
  }
  turn_release(t);
}

/* Arriving: each unit swings in from the other side, fading in over the first third, light ones first, over
   duration-moderate by its heft. The place assembles itself; nothing builds in after it. Back runs it the other
   way round. */
static void turn_in(rk_view *place, bool back) {
  static int slots[1024]; static float reach[256];
  turn t = { 0 };
  rk__layout_pending();
  int n = turn_gather(&t, place, NULL, slots, reach);
  double in = rk_duration_of(RK_DURATION_MODERATE);
  static double hs[1024], starts[1024];
  turn_starts(&t, n, NULL, hs, starts);
  for (int i = 0; i < n; i++) {
    rk_view *u = t.units[i];
    double h = hs[i], at = starts[i], d = in * h;
    rk_transform from = u->transform, to = u->transform;
    turn_pivot(&from, u, place, slots[i] < 256 ? reach[slots[i]] : rk__window_rect(u).w);
    to.origin_x = from.origin_x; to.origin_y = from.origin_y; to.perspective = from.perspective;
    from.rotate_y = back ? -TURN_IN_ANGLE : TURN_IN_ANGLE;
    float o0 = 0, o1 = u->opacity;
    rk__animate(u, RK_PROP_TRANSFORM, &from.translate_x, &to.translate_x, 10, d, at, turn_ease_in);
    rk__animate(u, RK_PROP_OPACITY, &o0, &o1, 1, 0.35 * d, at, rk__ease_base);
  }
  turn_release(&t);
  free(t.units);
}

typedef struct swap { rk_controller *places, *to; bool back; } swap;

/* The leaving place is gone: the next one turns in. */
static void swap_now(void *user) {
  swap *s = user;
  rk_controller *c = s->places;
  places *pl = c->impl;
  pl->timer = 0; pl->pending = NULL;
  rk_controller *old = pl->leaving;
  bool owned = pl->leaving_owned;
  pl->leaving = NULL; pl->leaving_owned = false;
  turn_reset(&pl->out);
  if (old) { rk__view_detach(rk_controller_view(old)); rk__controller_appear(old, false); }
  rk_view *nv = rk_controller_view(s->to);
  nv->ptransform = nv->transform; nv->popacity = nv->opacity;
  rk__view_add(c->view, nv);
  rk__controller_attach(s->to, c->window);
  turn_in(nv, s->back);
  if (c->appeared) rk__controller_appear(s->to, true);
  rk_window *w = c->window;
  if (w) {
    int at = pl->n - 1;
    rk_view *f = s->back && at >= 0 && at < 64 ? pl->focus_return[at] : NULL;
    if (f && rk__view_window(f) == w) rk_window_focus(w, f); else rk_window_focus(w, NULL);
    if (w->titlebar) rk__titlebar_update(w->titlebar);
  }
  if (owned) rk_release(old);
  free(s);
}

static void start_turn(rk_controller *c, rk_controller *old, bool owned, rk_controller *to, bool back, rk_view *pressed) {
  places *pl = c->impl;
  if (pl->pending) { rk_app_cancel(pl->timer); swap_now(pl->pending); }   /* a turn still running finishes at once */
  pl->leaving = old; pl->leaving_owned = owned;
  swap *s = rk__alloc(sizeof *s);
  s->places = c; s->to = to; s->back = back;
  double wait = old && old->view && rk__view_visible(old->view) ? turn_out(&pl->out, old->view, pressed, back) : 0;
  if (wait <= 0) { swap_now(s); return; }
  pl->pending = s;
  pl->timer = rk_app_after(wait, false, swap_now, s);
}

void rk_places_go(rk_controller *c, rk_controller *place) {
  if (c->kind != K_PLACES || !place) { rk_release(place); return; }
  places *pl = c->impl;
  rk_controller *old = pl->n ? pl->stack[pl->n - 1] : NULL;
  if (old == place) { rk_release(place); return; }
  rk_window *w = c->window;
  if (pl->n - 1 >= 0 && pl->n - 1 < 64) {                  /* back will put focus here again */
    rk_release(pl->focus_return[pl->n - 1]);
    pl->focus_return[pl->n - 1] = w && w->focus ? rk_retain(w->focus) : NULL;
  }
  pl->stack = rk__grow(pl->stack, &pl->cap, pl->n + 1, sizeof *pl->stack);
  pl->stack[pl->n++] = place;                                  /* takes the caller's reference */
  place->parent = c;
  start_turn(c, old, false, place, false, w ? w->activated : NULL);
}

void rk_places_back(rk_controller *c) {
  if (c->kind != K_PLACES) return;
  places *pl = c->impl;
  if (pl->n <= 1) return;
  rk_controller *old = pl->stack[--pl->n];               /* the reference moves to the turn */
  start_turn(c, old, true, pl->stack[pl->n - 1], true, NULL);
}

/* The strip: a pivot's or a panorama's titles. The active one is large; the others are scaled to 0.6 of it and
   change size over pivot-size. The strip glides by the active title's place in the order, so the title before it
   stays whole and in reach. */

typedef struct strip {
  char **titles; int n, active;
  bool nav, side, level_set;                                /* side: a pivot's titles as a list down the leading side */
  bool want_side;                                          /* asked for down the side; a narrow window keeps them on top */
  rk_glide *in;                                            /* down the side: each title's step in, 1 for the place in sight */
  rk_controller *pivot;                                    /* a pivot's strip: the nav level when nothing holds the pivot */
  rk_glide *scale;                                         /* each title's size, 1 active, INACTIVE otherwise */
  rk_glide glide;                                          /* how far the strip has slid */
  void (*select)(void *owner, int index); void *owner;
} strip;

#define INACTIVE 0.6f

static rk_textfmt strip_fmt(rk_view *v, strip *s) {
  return rk__textfmt(s->nav && !s->side ? RK_TEXT_NAV : RK_TEXT_PIVOT, rk__host_width(v));
}
static float strip_rest(strip *s, int i) { return i == s->active || s->side ? 1 : INACTIVE; }

/* Sizes and the slide glide: a change of section while one is under way carries on from its speed. */
static float strip_scale(strip *s, int i, double now) { return s->scale ? rk__glide_at(&s->scale[i], now, NULL, NULL) : (i == s->active ? 1 : INACTIVE); }
static float strip_glide(strip *s, double now) { return rk__glide_at(&s->glide, now, NULL, NULL); }

static bool strip_moving(strip *s, double now) {
  if (rk__glide_moving(&s->glide, now)) return true;
  for (int i = 0; s->scale && i < s->n; i++) if (rk__glide_moving(&s->scale[i], now) || rk__glide_moving(&s->in[i], now)) return true;
  return false;
}

static float strip_pad(strip *s) { return s->nav ? rk_metric_of(RK_SPACE_XL) : 0; }

/* where each title starts at its final size, and the glide that keeps the title before the active one whole */
static float strip_target(rk_view *v, strip *s) {
  rk_textfmt f = strip_fmt(v, s);
  float gap = rk_metric_of(RK_SPACE_L), x = strip_pad(s), prev = 0, total;
  for (int i = 0; i < s->n; i++) {
    if (i == s->active - 1) prev = x - strip_pad(s);
    x += rk__text_width(s->titles[i], -1, &f) * (i == s->active ? 1 : INACTIVE) + gap;
  }
  total = x - gap + strip_pad(s);
  float over = RK_MAX(0.0f, total - v->frame.w);
  return s->active > 0 ? RK_CLAMP(prev, 0.0f, over) : 0;
}

static void strip_level(strip *s);

/* Down the side: one title a line from the top, flush with the facets, all at one size. The one in sight steps in
   by space-m and darkens over pivot-size; the one before steps back: the motion says which was picked. */
static float side_line(rk_textfmt *f, float k) { return f->size * k * f->line + rk_metric_of(RK_SPACE_XS); }

static rk_size side_measure(rk_view *v, strip *s) {
  rk_textfmt f = strip_fmt(v, s);
  float w = 0, h = 0;
  for (int i = 0; i < s->n; i++) { w = RK_MAX(w, rk__text_width(s->titles[i], -1, &f)); h += side_line(&f, 1); }
  return (rk_size){ strip_pad(s) + rk_metric_of(RK_SPACE_M) + w + rk_metric_of(RK_SPACE_XL), h };
}

static void side_draw(rk_view *v, rk_canvas *c, strip *s, double now) {
  rk_textfmt f = strip_fmt(v, s);
  float y = 0, step = rk_metric_of(RK_SPACE_M);
  for (int i = 0; i < s->n; i++) {
    float in = rk__glide_at(&s->in[i], now, NULL, NULL);
    rk__text_draw(c, s->titles[i], -1, strip_pad(s) + in * step, y, &f, rk__mix(rk_color_of(RK_ROLE_TEXT_MEDIUM), rk_color_of(RK_ROLE_TEXT), in));
    y += side_line(&f, 1);
  }
}

static int side_hit(rk_view *v, strip *s, float py) {
  rk_textfmt f = strip_fmt(v, s);
  float y = 0;
  for (int i = 0; i < s->n; i++) { float h = side_line(&f, 1); if (py >= y && py < y + h) return i; y += h; }
  return -1;
}

static rk_size strip_measure(rk_view *v, rk_size avail, void *st) {
  strip *s = st;
  strip_level(s);
  if (s->side) return side_measure(v, s);
  rk_textfmt f = strip_fmt(v, s);
  return (rk_size){ isfinite(avail.w) ? avail.w : 600, RK_MAX(rk_metric_of(RK_TOUCH_TARGET), f.size * f.line) };
}

static void strip_draw(rk_view *v, rk_canvas *c, void *st) {
  strip *s = st;
  double now = rk_app_now();
  if (s->side) {
    side_draw(v, c, s, now);
    if (strip_moving(s, now)) { rk__view_dirty(v); rk__app_request_frame(); }
    return;
  }
  rk_textfmt f = strip_fmt(v, s);
  float gap = rk_metric_of(RK_SPACE_L), x = strip_pad(s) - strip_glide(s, now), H = v->pframe.h;
  float base = H - (f.size * f.line - rk__text_ascent(&f));      /* titles share the baseline, as transform-origin left bottom */
  rk_canvas_push_clip(c, (rk_rect){ 0, 0, v->pframe.w, H + 8 });
  for (int i = 0; i < s->n; i++) {
    float k = strip_scale(s, i, now);
    rk_textfmt g = f; g.size = f.size * k;
    rk__text_draw(c, s->titles[i], -1, x, base - rk__text_ascent(&g), &g, rk_color_of(i == s->active ? RK_ROLE_TEXT : RK_ROLE_TEXT_MEDIUM));
    x += rk__text_width(s->titles[i], -1, &f) * k + gap;
  }
  rk_canvas_pop_clip(c);
  if (strip_moving(s, now)) { rk__view_dirty(v); rk__app_request_frame(); }
}

static bool strip_event(rk_view *v, const rk_event *e, void *st) {
  strip *s = st;
  if (e->type == RK_EVENT_POINTER_UP && e->button == 0 && s->side) {
    int i = side_hit(v, s, e->position.y);
    if (i >= 0 && s->select) s->select(s->owner, i);
    return true;
  }
  if (e->type == RK_EVENT_POINTER_UP && e->button == 0) {
    double now = rk_app_now();
    rk_textfmt f = strip_fmt(v, s);
    float gap = rk_metric_of(RK_SPACE_L), x = strip_pad(s) - strip_glide(s, now);
    for (int i = 0; i < s->n; i++) {
      float w = rk__text_width(s->titles[i], -1, &f) * strip_scale(s, i, now);
      if (e->position.x >= x - gap / 2 && e->position.x < x + w + gap / 2) { if (s->select) s->select(s->owner, i); return true; }
      x += w + gap;
    }
    return true;
  }
  if (e->type == RK_EVENT_KEY_DOWN && (e->key == RK_KEY_HOME || e->key == RK_KEY_END) && !e->mods && s->n) {
    if (s->select) s->select(s->owner, e->key == RK_KEY_HOME ? 0 : s->n - 1);
    return true;
  }
  uint32_t back = s->side ? RK_KEY_UP : RK_KEY_LEFT, on = s->side ? RK_KEY_DOWN : RK_KEY_RIGHT;
  if (e->type == RK_EVENT_KEY_DOWN && (e->key == back || e->key == on) && s->n) {
    int i = (s->active + (e->key == on ? 1 : -1) + s->n) % s->n;
    if (s->select) s->select(s->owner, i);
    return true;
  }
  return false;
}

static void strip_destroy(void *st) {
  strip *s = st;
  for (int i = 0; i < s->n; i++) free(s->titles[i]);
  free(s->titles); free(s->scale); free(s->in); free(s);
}

static const rk_class strip_class = { { sizeof(rk_view_class), "rk_strip", strip_draw, strip_measure, NULL, strip_event, strip_destroy }, NULL, NULL };

static rk_view *strip_create(bool nav, void (*select)(void *, int), void *owner) {
  strip *s = rk__alloc(sizeof *s);
  s->nav = nav; s->select = select; s->owner = owner;
  rk_view *v = rk__view_create(&strip_class, s);
  v->focusable = true;
  v->access = RK_ACCESS_TAB_LIST;
  return v;
}

static void strip_add(rk_view *v, const char *title) {
  strip *s = v->state;
  s->titles = realloc(s->titles, (size_t)(s->n + 1) * sizeof *s->titles);
  s->titles[s->n++] = rk__strdup(title ? title : "");
  s->scale = realloc(s->scale, (size_t)s->n * sizeof *s->scale);
  s->in = realloc(s->in, (size_t)s->n * sizeof *s->in);
  rk__glide_set(&s->scale[s->n - 1], strip_rest(s, s->n - 1));
  rk__glide_set(&s->in[s->n - 1], s->n - 1 == s->active);
  rk__view_relayout_up(v);
}

static void strip_activate(rk_view *v, int index) {
  strip *s = v->state;
  if (index == s->active || index < 0 || index >= s->n) return;
  double now = rk_app_now();
  s->active = index;
  for (int i = 0; i < s->n; i++) {
    rk__glide_to(&s->scale[i], now, strip_rest(s, i), rk_duration_of(RK_DURATION_PIVOT_SIZE));
    rk__glide_to(&s->in[i], now, i == index, rk_duration_of(RK_DURATION_PIVOT_SIZE));
  }
  if (!s->side) rk__glide_to(&s->glide, now, strip_target(v, s), rk_duration_of(RK_DURATION_PIVOT_GLIDE));
  rk__view_dirty(v);
  rk__app_request_frame();
}

/* Panorama -------------------------------------------------------------------------------------------------------- */

typedef struct section { char *title; rk_view *view, *box; float width, x, max; } section;
typedef struct panorama {
  rk_controller *self; section *secs; int n, cap; rk_view *strip, *track; float max_scroll;
  int latch; double latch_at;                                  /* what the current wheel gesture moves */
  float boost;                                                 /* wheel acceleration */
} panorama;

static float section_width(section *s) {
  if (s->width > 0) return s->width;
  return 3 * rk_metric_of(RK_TILE_UNIT) + 2 * rk_metric_of(RK_SPACE_S) + 2 * rk_metric_of(RK_SPACE_XL);
}

/* A section's box clips it and scrolls it up and down when it is taller than the panorama. */
static void secbox_draw(rk_view *v, rk_canvas *c, void *st) {
  (void)c; (void)st;
  if (v->nkids) v->kids[0]->ptransform.translate_y = -v->pvalue;
}
static const rk_class secbox_class = { { sizeof(rk_view_class), "rk_section", secbox_draw, NULL, NULL, NULL, NULL }, NULL, NULL };

static void pano_layout(rk_view *v, void *st) {
  panorama *p = st;
  float W = v->frame.w, H = v->frame.h, xl = rk_metric_of(RK_SPACE_XL), l = rk_metric_of(RK_SPACE_L), sp = rk_metric_of(RK_SPACE_S);
  rk_size ss = rk__measure(p->strip, (rk_size){ W, INFINITY });
  float top = rk_metric_of(RK_SPACE_S);
  rk__set_frame(p->strip, (rk_rect){ 0, top, W, ss.h });
  float ty = top + ss.h, th = H - ty;
  float x = xl;
  for (int i = 0; i < p->n; i++) {
    section *s = &p->secs[i];
    float w = section_width(s);
    s->x = x;
    rk__set_frame(s->box, (rk_rect){ x, 0, w, th });
    float inner = w - sp;                                       /* room for the scroll edge, as padding-right */
    rk_size n = rk__measure(s->view, (rk_size){ inner, INFINITY });
    float h = RK_MAX(n.h, th - 2 * l);
    rk__set_frame(s->view, (rk_rect){ 0, l, inner, h });
    s->max = RK_MAX(0.0f, h + 2 * l - th);
    if (s->box->value > s->max) s->box->value = s->box->pvalue = s->max;
    x += w + xl;
  }
  /* the last section may reach the left edge */
  p->max_scroll = p->n ? RK_MAX(0.0f, RK_MIN(p->secs[p->n - 1].x - xl, x - W)) : 0;
  if (p->n && x - xl <= W) p->max_scroll = 0;
  rk__set_frame(p->track, (rk_rect){ 0, ty, RK_MAX(W, x), th });
  if (v->value > p->max_scroll) { v->value = v->pvalue = p->max_scroll; }
}

static int pano_active(panorama *p, float scroll) {
  int a = 0;
  float xl = rk_metric_of(RK_SPACE_XL);
  for (int i = 0; i < p->n; i++) if (p->secs[i].x - xl <= scroll + section_width(&p->secs[i]) / 2) a = i;
  return a;
}

static void pano_draw(rk_view *v, rk_canvas *c, void *st) {
  (void)c;
  panorama *p = st;
  p->track->ptransform.translate_x = -v->pvalue;
  strip_activate(p->strip, pano_active(p, v->value));
}

/* Scrolling rides a spring: it sets off gently, a new wheel step adds to where it is going without stopping it,
   and it comes to rest without a bump. */
static void scroll_value(rk_view *v, float to, float max, double settle) {
  to = RK_CLAMP(to, 0.0f, max);
  v->value = to;
  rk__animate_spring(v, RK_PROP_VALUE, to, settle);
  rk__view_dirty(v);
}

#define SETTLE_WHEEL 260.0                                     /* ms a wheel step takes to come to rest */
#define SETTLE_GO 520.0                                        /* ms to bring a section in */

/* Scroll ---------------------------------------------------------------------------------------------------------- */
/* A view that clips its content and scrolls it up and down on the panorama's spring. The wheel moves it until an end
   (then what holds it moves); keys a focused view inside leaves (Up, Down, Page Up, Page Down, Home, End) move it;
   focus landing inside brings the view in sight. A 4px bar shows while it moves and a moment after, and drags. */

typedef struct scroller { float max; bool bar, dragging; uint32_t hide; float drag_y, drag_value; rk_view *view, *rail; } scroller;

#define BAR_W 4.0f
#define BAR_MIN 24.0f

static void scroller_hide(void *user) { scroller *s = user; s->hide = 0; if (s->dragging) return; s->bar = false; s->rail->passthrough = true; rk__view_dirty(s->rail); }
static void scroller_show(scroller *s) {   /* the bar shows, and takes the pointer while it does */
  s->bar = true;
  s->rail->passthrough = false;
  if (s->hide) rk_app_cancel(s->hide);
  s->hide = rk_app_after(900, false, scroller_hide, s);
  rk__view_dirty(s->rail);
}
static void scroller_to(scroller *s, float to, double settle) {
  if (s->max <= 0) return;
  scroll_value(s->view, to, s->max, settle);
  scroller_show(s);
}

static void scroller_layout(rk_view *v, void *st) {
  scroller *s = st;
  if (v->nkids < 2) return;
  rk_view *c = v->kids[0];
  rk_size n = rk__measure(c, (rk_size){ v->frame.w, INFINITY });
  float h = RK_MAX(n.h, v->frame.h);
  rk__set_frame(c, (rk_rect){ 0, 0, v->frame.w, h });
  rk__set_frame(s->rail, (rk_rect){ v->frame.w - BAR_W - 4, 0, BAR_W + 4, v->frame.h });
  s->max = RK_MAX(0.0f, h - v->frame.h);
  if (v->value > s->max) v->value = v->pvalue = s->max;
}

static rk_size scroller_measure(rk_view *v, rk_size avail, void *st) {
  (void)st;
  if (!v->nkids) return (rk_size){ 0, 0 };
  rk_size n = rk__measure(v->kids[0], (rk_size){ avail.w, INFINITY });
  return (rk_size){ isfinite(avail.w) ? avail.w : n.w, n.h };
}

static void scroller_draw(rk_view *v, rk_canvas *c, void *st) {
  (void)c; (void)st;
  if (v->nkids) v->kids[0]->ptransform.translate_y = -v->pvalue;
}

static bool scroller_event(rk_view *v, const rk_event *e, void *st) {
  scroller *s = st;
  if (s->max <= 0) return false;
  if (e->type == RK_EVENT_WHEEL) {
    float to = RK_CLAMP(v->value + e->wheel_y, 0.0f, s->max);
    if (to == v->value) return false;                          /* at its end: what holds it moves */
    scroller_to(s, to, SETTLE_WHEEL);
    return true;
  }
  if (e->type != RK_EVENT_KEY_DOWN || (e->mods & (RK_MOD_CTRL | RK_MOD_ALT | RK_MOD_SUPER))) return false;
  float page = RK_MAX(40.0f, v->frame.h - 40), to = v->value;
  switch (e->key) {
    case RK_KEY_UP: to -= 40; break;
    case RK_KEY_DOWN: to += 40; break;
    case RK_KEY_PAGE_UP: to -= page; break;
    case RK_KEY_PAGE_DOWN: to += page; break;
    case RK_KEY_HOME: to = 0; break;
    case RK_KEY_END: to = s->max; break;
    default: return false;
  }
  to = RK_CLAMP(to, 0.0f, s->max);
  if (to == v->value) return false;
  scroller_to(s, to, SETTLE_WHEEL);
  return true;
}

static void scroller_free(void *st) { scroller *s = st; if (s->hide) rk_app_cancel(s->hide); free(s); }
static const rk_class scroll_class = { { sizeof(rk_view_class), "rk_scroll", scroller_draw, scroller_measure, scroller_layout, scroller_event, scroller_free }, NULL, NULL };

/* The bar: a rail along the right edge that draws the thumb while it shows and drags it. */
static void thumb_of(scroller *s, float H, float *y, float *h) {
  *h = RK_MAX(BAR_MIN, H * H / (H + s->max));
  *y = s->max > 0 ? (H - *h) * s->view->pvalue / s->max : 0;
}
static void rail_draw(rk_view *v, rk_canvas *c, void *st) {
  scroller *s = st;
  if (s->max <= 0 || !(s->bar || s->dragging)) return;
  float y, h;
  thumb_of(s, v->pframe.h, &y, &h);
  rk_canvas_fill_rect(c, (rk_rect){ v->pframe.w - BAR_W, y, BAR_W, h }, rk_color_of(s->dragging ? RK_ROLE_TEXT : RK_ROLE_TEXT_MEDIUM));
  if (s->view->pvalue != s->view->value) rk__view_dirty(v);   /* it follows the glide */
}
static bool rail_event(rk_view *v, const rk_event *e, void *st) {
  scroller *s = st;
  if (s->max <= 0) return false;
  float H = v->frame.h, y, h;
  thumb_of(s, H, &y, &h);
  switch (e->type) {
    case RK_EVENT_POINTER_DOWN:
      if (e->button || !s->bar) return false;                  /* only a bar in sight takes a press */
      if (e->position.y < y || e->position.y > y + h) { scroller_to(s, s->max * (e->position.y - h / 2) / (H - h), SETTLE_GO); return true; }
      s->dragging = true; s->drag_y = e->position.y; s->drag_value = s->view->value;
      rk__view_dirty(v);
      return true;
    case RK_EVENT_POINTER_MOVE:
      if (!s->dragging) return false;
      s->view->value = s->view->pvalue = RK_CLAMP(s->drag_value + (e->position.y - s->drag_y) * s->max / RK_MAX(1.0f, H - h), 0.0f, s->max);
      rk__anim_cancel(s->view, RK_PROP_VALUE);
      scroller_show(s);
      rk__view_dirty(s->view);
      return true;
    case RK_EVENT_POINTER_UP: if (!s->dragging) return false; s->dragging = false; rk__view_dirty(v); scroller_show(s); return true;
    default: return false;
  }
}
static const rk_class rail_class = { { sizeof(rk_view_class), "rk_scroll_bar", rail_draw, NULL, NULL, rail_event, NULL }, NULL, NULL };

rk_view *rk_scroll(rk_view *content) {
  scroller *s = rk__alloc(sizeof *s);
  rk_view *v = rk__view_create(&scroll_class, s);
  s->view = v;
  v->clip = true;
  v->access = RK_ACCESS_GROUP;
  if (content) { rk__view_add(v, content); rk_release(content); }
  else { rk_view *empty = rk_view_create(); rk__view_add(v, empty); rk_release(empty); }
  s->rail = rk__view_create(&rail_class, s);
  s->rail->passthrough = true;
  rk__view_add(v, s->rail); rk_release(s->rail);
  return v;
}

void rk_view_reveal(rk_view *v) { if (v) rk__reveal(v); }

static void pano_scroll_to(rk_view *v, panorama *p, float to, double settle) {
  scroll_value(v, to, p->max_scroll, settle);
  strip_activate(p->strip, pano_active(p, v->value));
}

static void pano_select(void *owner, int i) {
  panorama *p = owner;
  rk_view *v = p->self->view;
  if (i < 0 || i >= p->n) return;
  pano_scroll_to(v, p, p->secs[i].x - rk_metric_of(RK_SPACE_XL), SETTLE_GO);
}

/* One wheel gesture moves one thing: started over a section that can still scroll that way, it scrolls the
   section; otherwise the panorama goes sideways. The choice holds for the gesture (wheel-latch). */
static bool pano_event(rk_view *v, const rk_event *e, void *st) {
  panorama *p = st;
  if (e->type == RK_EVENT_WHEEL) {
    bool sideways = fabsf(e->wheel_x) > fabsf(e->wheel_y);
    float d = sideways ? e->wheel_x : e->wheel_y;
    if (d == 0) return false;
    double now = rk_app_now();
    int target = -1;                                            /* -1: the panorama */
    if (now - p->latch_at < RK_TOK_TIMING_WHEEL_LATCH && p->latch >= -1) target = p->latch;
    else if (!sideways) {
      float x = e->position.x + v->pvalue;
      for (int i = 0; i < p->n; i++) {
        section *s = &p->secs[i];
        if (x >= s->x && x < s->x + section_width(s) && s->max > 0 && ((d > 0 && s->box->value < s->max) || (d < 0 && s->box->value > 0))) target = i;
      }
    }
    /* wheel steps in quick succession go further each time (a touchpad's small, continuous steps as they are) */
    if (fabsf(d) >= 40 && now - p->latch_at < 90) p->boost = RK_MIN(p->boost * 1.25f, 4.0f); else p->boost = 1;
    d *= p->boost;
    p->latch = target; p->latch_at = now;
    if (target >= 0) scroll_value(p->secs[target].box, p->secs[target].box->value + d, p->secs[target].max, SETTLE_WHEEL);
    else pano_scroll_to(v, p, v->value + d, SETTLE_WHEEL);
    return true;
  }
  if (e->type == RK_EVENT_KEY_DOWN) {
    int a = pano_active(p, v->value);
    if (e->key == RK_KEY_PAGE_DOWN) { pano_select(p, RK_MIN(a + 1, p->n - 1)); return true; }
    if (e->key == RK_KEY_PAGE_UP) { pano_select(p, RK_MAX(a - 1, 0)); return true; }
    if (e->key == RK_KEY_HOME && !(e->mods & RK_MOD_SHIFT)) { pano_select(p, 0); return true; }
    if (e->key == RK_KEY_END && !(e->mods & RK_MOD_SHIFT)) { pano_select(p, p->n - 1); return true; }
    if ((e->key == RK_KEY_UP || e->key == RK_KEY_DOWN) && !e->mods && p->n && p->secs[a].max > 0) {   /* the section in sight scrolls, as a page does */
      section *s = &p->secs[a];
      float to = RK_CLAMP(s->box->value + (e->key == RK_KEY_DOWN ? 40.0f : -40.0f), 0.0f, s->max);
      if (to == s->box->value) return false;
      scroll_value(s->box, to, s->max, SETTLE_WHEEL);
      return true;
    }
  }
  return false;
}

static void pano_free(void *st) {
  panorama *p = st;
  for (int i = 0; i < p->n; i++) free(p->secs[i].title);
  free(p->secs); free(p);
}
static const rk_class pano_class = { { sizeof(rk_view_class), "rk_panorama", pano_draw, NULL, pano_layout, pano_event, NULL }, NULL, NULL };

rk_controller *rk_panorama(void) {
  rk_controller *c = rk_controller_create(NULL, NULL);
  panorama *p = rk__alloc(sizeof *p);
  p->self = c;
  p->latch = -2;
  c->kind = K_PANORAMA; c->impl = p; c->impl_free = pano_free;
  c->view = rk__view_create(&pano_class, p);
  c->view->controller = c;
  c->view->clip = true;
  p->strip = strip_create(true, pano_select, p);
  p->track = rk_view_create();
  rk__view_add(c->view, p->track); rk_release(p->track);
  rk__view_add(c->view, p->strip); rk_release(p->strip);
  return c;
}

void rk_panorama_add(rk_controller *c, const char *title, rk_view *section_view, float width) {
  if (c->kind != K_PANORAMA || !section_view) { rk_release(section_view); return; }
  panorama *p = c->impl;
  p->secs = rk__grow(p->secs, &p->cap, p->n + 1, sizeof *p->secs);
  section *s = &p->secs[p->n++];
  s->title = rk__strdup(title); s->view = section_view; s->width = width;
  s->box = rk__view_create(&secbox_class, NULL);
  s->box->clip = true;
  s->box->access = RK_ACCESS_GROUP;
  rk__setstr(&s->box->access_label, title);
  rk__view_add(s->box, section_view);
  rk_release(section_view);                                    /* the section box holds it now */
  rk__view_add(p->track, s->box);
  rk_release(s->box);
  strip_add(p->strip, title);
}

/* Keyboard focus never lands out of sight: the section scrolls it into view, the panorama brings the section. */
void rk__reveal(rk_view *v) { rk__reveal_rect(v, (rk_rect){ 0, 0, v->frame.w, v->frame.h }); }

void rk__reveal_rect(rk_view *v, rk_rect r) {
  for (rk_view *a = v->parent; a; a = a->parent) {
    if (a->cls == &secbox_class && a->nkids) {
      rk_view *content = a->kids[0];
      float y = 0;
      for (rk_view *x = v; x && x != content; x = x->parent) y += x->frame.y;
      float top = y + r.y + content->frame.y, bottom = top + r.h, view_h = a->frame.h, off = a->value;
      float max = RK_MAX(0.0f, content->frame.h + 2 * content->frame.y - view_h);
      if (top - off < 0) scroll_value(a, top - rk_metric_of(RK_SPACE_M), max, SETTLE_WHEEL);
      else if (bottom - off > view_h) scroll_value(a, bottom - view_h + rk_metric_of(RK_SPACE_M), max, SETTLE_WHEEL);
    }
    if (a->cls == &scroll_class && a->nkids) {
      scroller *sc = a->state;
      rk_view *content = a->kids[0];
      float y = 0;
      for (rk_view *x = v; x && x != content; x = x->parent) y += x->frame.y;
      float top = y + r.y, bottom = top + r.h, view_h = a->frame.h, off = a->value, m = rk_metric_of(RK_SPACE_S);
      if (top - off < 0) scroller_to(sc, top - m, SETTLE_WHEEL);
      else if (bottom - off > view_h) scroller_to(sc, bottom - view_h + m, SETTLE_WHEEL);
    }
    if (a->cls == &pano_class) {
      panorama *p = a->state;
      for (int i = 0; i < p->n; i++) {
        for (rk_view *x = v; x; x = x->parent) if (x == p->secs[i].box) {
          float left = p->secs[i].x - a->value, W = a->frame.w;
          if (left < 0 || left + section_width(&p->secs[i]) > W) pano_select(p, i);
          return;
        }
      }
    }
  }
}

/* Pivot ----------------------------------------------------------------------------------------------------------- */

typedef struct pivot {
  rk_controller *self; rk_controller **facets; int n, cap, sel; rk_view *strip, *body;
  turn out; rk_controller *leaving; uint32_t timer; bool back;   /* a place change: the facet turning away */
} pivot;

static void pivot_select_cb(void *owner, int i) { rk_pivot_select(((pivot *)owner)->self, i); }
static void pivot_free(void *st) {
  pivot *p = st;
  if (p->timer) rk_app_cancel(p->timer);
  turn_release(&p->out);
  for (int i = 0; i < p->n; i++) rk_release(p->facets[i]);
  free(p->out.units); free(p->facets); free(p);
}

static rk_controller *pivot_visible(rk_controller *c) { pivot *p = c->impl; return p->n ? p->facets[p->sel] : NULL; }

/* A pivot nothing else holds picks the place (navigation.md): its strip takes the nav level, as the panorama's does,
   and its facets start where the strip's first title does. Inside a panorama or another pivot it picks a facet. */
static void strip_level(strip *s) {
  if (!s->pivot) return;
  bool top = true;
  for (rk_controller *a = s->pivot->parent; a; a = a->parent) if (a->kind == K_PIVOT || a->kind == K_PANORAMA) top = false;
  if (top == s->nav && s->level_set) return;
  s->nav = top; s->level_set = true;
  pivot *p = s->pivot->impl;
  float xl = top ? rk_metric_of(RK_SPACE_XL) : 0;
  rk_view_set_padding(p->body, (rk_insets){ 0, xl, 0, s->side ? 0 : xl });
}

/* Along the top the titles are the facets' heading; down the side they are a list beside them, as tabs are in a
   browser either way. Same facets, same keys turned a quarter, same motion. A window narrower than 760 px or lower
   than 481 px (the web's compact class) keeps them along the top, where a small screen has room for them. */
static void pivot_mode(pivot *p) {
  strip *s = p->strip->state;
  rk_view *v = p->self->view;
  rk_window *w = rk__view_window(v);
  bool narrow = w && (w->size.w < 760 || w->size.h < 481), side = s->want_side && !narrow;
  if (side == s->side) return;
  s->side = side; s->level_set = false;
  for (int i = 0; i < s->n; i++) rk__glide_set(&s->scale[i], strip_rest(s, i));
  rk__glide_set(&s->glide, 0);
  v->layout = side ? RK_LAYOUT_ROW : RK_LAYOUT_COLUMN;
  v->gap = side ? 0 : rk_metric_of(RK_SPACE_M);
  v->padding = (rk_insets){ side ? rk_metric_of(RK_SPACE_L) : 0, 0, 0, 0 };   /* the list and its facet start together, clear of the bar */
  strip_level(s);
}

static void pivot_layout(rk_view *v, void *st) {
  pivot *p = st;
  pivot_mode(p);
  rk__stack(v, !((strip *)p->strip->state)->side);
}

static const rk_class pivot_class = { { sizeof(rk_view_class), "rk_pivot", NULL, NULL, pivot_layout, NULL, NULL }, NULL, NULL };

void rk_pivot_set_layout(rk_controller *c, rk_pivot_layout layout) {
  if (c->kind != K_PIVOT) return;
  pivot *p = c->impl;
  strip *s = p->strip->state;
  s->want_side = layout == RK_PIVOT_SIDE;
  pivot_mode(p);
  rk__view_relayout_up(c->view);
}

rk_controller *rk_pivot(void) {
  rk_controller *c = rk_controller_create(NULL, NULL);
  pivot *p = rk__alloc(sizeof *p);
  p->self = c;
  c->kind = K_PIVOT; c->impl = p; c->impl_free = pivot_free;
  rk_view *v = rk__view_create(&pivot_class, p);
  rk_view_set_layout(v, RK_LAYOUT_COLUMN);                 /* how it measures; pivot_layout lays it out either way */
  rk_view_set_gap(v, rk_metric_of(RK_SPACE_M));
  rk_view_set_align(v, RK_ALIGN_START, RK_ALIGN_STRETCH);
  c->view = v; v->controller = c;
  p->strip = strip_create(false, pivot_select_cb, p);
  ((strip *)p->strip->state)->pivot = c;
  p->body = rk__view_create(&fill_class, NULL);
  p->body->grow = 1;
  p->body->clip = true;
  rk__view_add(v, p->strip); rk_release(p->strip);
  rk__view_add(v, p->body); rk_release(p->body);
  return c;
}

void rk_pivot_add(rk_controller *c, rk_controller *facet) {
  if (c->kind != K_PIVOT || !facet) { rk_release(facet); return; }
  pivot *p = c->impl;
  p->facets = rk__grow(p->facets, &p->cap, p->n + 1, sizeof *p->facets);
  p->facets[p->n++] = facet;                                   /* takes the caller's reference */
  facet->parent = c;
  strip_add(p->strip, facet->title);
  if (p->n == 1) {
    rk__view_add(p->body, rk_controller_view(facet));
    rk__controller_attach(facet, c->window);
    if (c->appeared) rk__controller_appear(facet, true);
  }
}

/* A pivot that picks the place turns it: the facet in sight turns away, then the next one turns in. One later in
   the order turns forward, one earlier the way back. */
static void pivot_arrive(void *user) {
  pivot *p = user;
  rk_controller *c = p->self, *old = p->leaving, *to = p->facets[p->sel];
  p->timer = 0; p->leaving = NULL;
  turn_reset(&p->out);
  rk__view_detach(rk_controller_view(old));
  rk__controller_appear(old, false);
  rk_view *nv = rk_controller_view(to);
  nv->ptransform = nv->transform; nv->popacity = nv->opacity;
  rk__view_add(p->body, nv);
  rk__controller_attach(to, c->window);
  turn_in(nv, p->back);
  if (c->appeared) rk__controller_appear(to, true);
  if (c->window && c->window->titlebar) rk__titlebar_update(c->window->titlebar);
}

/* A facet change slides the new facet in from its side, slide-distance, with a fade, over 0.75 × duration-moderate. */
void rk_pivot_select(rk_controller *c, int index) {
  if (c->kind != K_PIVOT) return;
  pivot *p = c->impl;
  if (index < 0 || index >= p->n || index == p->sel) return;
  if (p->timer) { rk_app_cancel(p->timer); pivot_arrive(p); }      /* a turn still running finishes at once */
  rk_controller *old = p->facets[p->sel], *to = p->facets[index];
  int dir = index > p->sel ? 1 : -1;
  p->sel = index;
  strip_activate(p->strip, index);
  if (((strip *)p->strip->state)->nav) {
    p->leaving = old; p->back = dir < 0;
    double wait = rk__view_visible(rk_controller_view(old)) ? turn_out(&p->out, rk_controller_view(old), NULL, p->back) : 0;
    if (wait > 0) p->timer = rk_app_after(wait, false, pivot_arrive, p); else pivot_arrive(p);
    return;
  }
  rk__view_detach(rk_controller_view(old));               /* the controller keeps its view */
  rk__controller_appear(old, false);
  rk_view *nv = rk_controller_view(to);
  rk__view_add(p->body, nv);
  rk__controller_attach(to, c->window);
  rk__layout_pending();
  rk_transform t0 = nv->transform, t1 = nv->transform;
  if (((strip *)p->strip->state)->side) t0.translate_y += dir * RK_TOK_SLIDE_DISTANCE;   /* from below or above, where its title is */
  else t0.translate_x += dir * RK_TOK_SLIDE_DISTANCE;
  double d = 0.75 * rk_duration_of(RK_DURATION_MODERATE);
  float a0 = 0, a1 = nv->opacity;
  rk__animate(nv, RK_PROP_TRANSFORM, &t0.translate_x, &t1.translate_x, 10, d, 0, rk__ease_out);
  rk__animate(nv, RK_PROP_OPACITY, &a0, &a1, 1, d, 0, rk__ease_out);
  if (c->appeared) rk__controller_appear(to, true);
}

int rk_pivot_selected(rk_controller *c) { return c->kind == K_PIVOT ? ((pivot *)c->impl)->sel : -1; }

/* Sheets ---------------------------------------------------------------------------------------------------------- */

/* The sheet layer: the shading fills the window; each sheet hangs from the top edge, up to 32rem wide. Its top
   corners sit above the window, so only the bottom ones show their radius. */
static void sheets_layout(rk_view *v, void *st) {
  (void)st;
  float W = v->frame.w, H = v->frame.h, r = rk_metric_of(RK_RADIUS_M);
  for (int i = 0; i < v->nkids; i++) {
    rk_view *k = v->kids[i];
    if (k->user) {                                          /* a sheet box */
      float w = RK_MIN(512.0f, W);
      rk_size s = rk__measure(k, (rk_size){ w, INFINITY });
      rk__set_frame(k, (rk_rect){ (W - w) / 2, -r, w, RK_MIN(s.h, H - rk_metric_of(RK_SPACE_XL) + r) });
    } else rk__set_frame(k, (rk_rect){ 0, 0, W, H });
  }
}
static const rk_class sheets_class = { { sizeof(rk_view_class), "rk_sheets", NULL, NULL, sheets_layout, NULL, NULL }, NULL, NULL };

static void shade_tap(rk_view *v, void *user) { (void)v; rk_controller_dismiss(user); }

static rk_view *first_focusable(rk_view *v) {
  if (v->hidden) return NULL;
  if (v->focusable) return v;
  for (int i = 0; i < v->nkids; i++) { rk_view *f = first_focusable(v->kids[i]); if (f) return f; }
  return NULL;
}

void rk_controller_present(rk_controller *presenter, rk_controller *sheet) {
  rk_window *w = presenter->window;
  if (!w || !sheet) { rk_release(sheet); return; }
  if (presenter->sheet) rk_controller_dismiss(presenter->sheet);
  if (w->sheets->cls != &sheets_class) w->sheets->cls = &sheets_class;
  presenter->sheet = sheet;                                    /* takes the caller's reference */
  sheet->presenter = presenter;
  sheet->window = w;
  rk_view *shade = rk_view_create();
  rk_view_set_fill(shade, RK_ROLE_SHADING);
  rk_view_on_activate(shade, shade_tap, sheet);
  shade->press = RK_PRESS_FLAT; shade->focusable = false;
  rk_view *box = rk_view_create();
  float m = rk_metric_of(RK_SPACE_M), l = rk_metric_of(RK_SPACE_L), r = rk_metric_of(RK_RADIUS_M);
  rk_view_set_layout(box, RK_LAYOUT_COLUMN);
  rk_view_set_padding(box, (rk_insets){ l + r, w->size.w > 560 ? rk_metric_of(RK_SPACE_XL) : m, l, w->size.w > 560 ? rk_metric_of(RK_SPACE_XL) : m });
  rk_view_set_fill(box, RK_ROLE_BG);
  box->radius = r;
  box->user = sheet;                                        /* marks it as a box; borrowed */
  box->access = RK_ACCESS_DIALOG;
  rk__setstr(&box->access_label, sheet->title);
  rk__view_add(box, rk_controller_view(sheet));
  sheet->sheet_shade = shade; sheet->sheet_box = box;
  rk__view_add(w->sheets, shade); rk_release(shade);
  rk__view_add(w->sheets, box); rk_release(box);
  rk__layout_pending();
  double d = rk_duration_of(RK_DURATION_SHORT);
  float a0 = 0, a1 = 1;
  rk__animate(shade, RK_PROP_OPACITY, &a0, &a1, 1, d, 0, rk__ease_in);
  rk__animate(box, RK_PROP_OPACITY, &a0, &a1, 1, d, 0, rk__ease_in);
  rk_transform t0 = box->transform, t1 = box->transform;
  t0.translate_y -= 16;
  rk__animate(box, RK_PROP_TRANSFORM, &t0.translate_x, &t1.translate_x, 10, d, 0, rk__ease_in);
  sheet->focus_before = w->focus ? rk_retain(w->focus) : NULL;
  rk_view *f = first_focusable(box);
  w->focus = NULL;
  if (f) rk_window_focus(w, f);
  rk__controller_appear(sheet, true);
}

static void sheet_gone(rk_view *box, void *user) {
  rk_controller *sheet = user;
  rk_view *shade = sheet->sheet_shade;
  sheet->sheet_box = sheet->sheet_shade = NULL;
  if (shade) rk__view_detach(shade);
  if (box->parent) rk__view_detach(box);
  rk_release(sheet);
}

void rk_controller_dismiss(rk_controller *sheet) {
  rk_controller *p = sheet ? sheet->presenter : NULL;
  if (!p || p->sheet != sheet) return;
  rk_window *w = sheet->window;
  p->sheet = NULL; sheet->presenter = NULL;
  rk__controller_appear(sheet, false);
  rk_view *box = sheet->sheet_box, *shade = sheet->sheet_shade;
  if (w) {
    rk_view *f = sheet->focus_before;
    w->focus = NULL;
    if (f && rk__view_window(f) == w) rk_window_focus(w, f);
  }
  rk_release(sheet->focus_before); sheet->focus_before = NULL;
  if (!box) { rk_release(sheet); return; }
  double d = 0.8 * rk_duration_of(RK_DURATION_SHORT);
  float a0 = box->popacity, a1 = 0, s0 = shade ? shade->popacity : 0;
  rk_transform t0 = box->ptransform, t1 = box->transform;
  t1.translate_y -= 16;
  if (shade) rk__animate(shade, RK_PROP_OPACITY, &s0, &a1, 1, d, 0, rk__ease_out);
  rk__animate(box, RK_PROP_OPACITY, &a0, &a1, 1, d, 0, rk__ease_out);
  if (shade) shade->leaving = true;
  box->leaving = true;
  rk__animate_cb(box, RK_PROP_TRANSFORM, &t0.translate_x, &t1.translate_x, 10, d, 0, rk__ease_out, sheet_gone, sheet);
}

/* Window keys and the chain --------------------------------------------------------------------------------------- */

static rk_controller *find_places(rk_controller *c) {
  for (int guard = 0; c && guard < 16; guard++) {
    if (c->kind == K_PLACES) return c;
    if (c->kind == K_PIVOT) c = pivot_visible(c); else return NULL;
  }
  return NULL;
}

bool rk__window_key(rk_window *w, const rk_event *e) {
  if (e->type != RK_EVENT_KEY_DOWN) return false;
  if (e->mods & (RK_MOD_CTRL | RK_MOD_SUPER)) {                 /* the app's history, outside text fields */
    if (e->key == 'Z') return e->mods & RK_MOD_SHIFT ? rk_window_redo(w) : rk_window_undo(w);
    if (e->key == 'Y') return rk_window_redo(w);
  }
  bool back = e->key == RK_KEY_BACK || (e->key == RK_KEY_LEFT && (e->mods & RK_MOD_ALT));
  if (e->key == RK_KEY_ESCAPE) {
    for (int i = w->sheets->nkids - 1; i >= 0; i--) {
      rk_view *k = w->sheets->kids[i];
      if (k->user && !k->leaving) { rk_controller_dismiss(k->user); return true; }
    }
    back = true;
  }
  if (back) {
    rk_controller *pl = find_places(w->controller);
    if (pl && rk_places_depth(pl) > 1) { rk_places_back(pl); return true; }
  }
  return false;
}

rk_view *rk__controller_key_view(rk_controller *c) {
  for (int guard = 0; c && guard < 32; guard++) {
    rk_controller *v = visible_child(c);
    if (!v || v == c) return rk_controller_view(c);
    c = v;
  }
  return c ? rk_controller_view(c) : NULL;
}

bool rk__controller_event(rk_controller *c, const rk_event *e) {
  return c->cls.event ? c->cls.event(c, e, c->state) : false;
}

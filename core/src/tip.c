/* tip.c: tooltips, as the web's tips(). A short ink label for a view that has one (rk_view_set_tooltip): after
   tip-delay under the pointer, at once after another tip or on keyboard focus. Escape, a press, the wheel or leaving
   hide it. It repeats or completes what the view says - never the only way to learn what it does; a disabled view's
   reason is the common case, and such a view stays reachable by Tab so the keyboard finds its reason too.
   The label floats in the window's top layer, centred above its view (below where there is no room), and comes in
   with a fade and a 4px shift over duration-float, as a menu does. */
#include "rk_internal.h"

#define TIP_MAX_W 320                                      /* 20rem */
#define TIP_GAP 6

static rk_textfmt tip_fmt(void) {
  rk_textfmt f = rk__textfmt(RK_TEXT_LABEL, 0);           /* xs, semibold, line-height compact */
  f.color = RK_ROLE_BG;
  return f;
}

static rk_size tip_measure(rk_view *v, rk_size avail, void *st) {
  (void)v; (void)avail;
  const char *text = st;
  rk_textfmt f = tip_fmt();
  float pad = rk_metric_of(RK_SPACE_S);
  float w;
  int n = rk__text_wrap_size(text, &f, TIP_MAX_W - 2 * pad, &w);
  return (rk_size){ ceilf(w + 2 * pad), ceilf(RK_MAX(n, 1) * f.size * f.line + 8) };
}

static void tip_draw(rk_view *v, rk_canvas *c, void *st) {
  const char *text = st;
  rk_textfmt f = tip_fmt();
  float pad = rk_metric_of(RK_SPACE_S);
  rk_canvas_fill_round_rect(c, (rk_rect){ 0, 0, v->pframe.w, v->pframe.h }, rk_metric_of(RK_RADIUS_S), rk_color_of(RK_ROLE_TEXT));
  rk__text_draw_lines(c, text, &f, pad, 4, TIP_MAX_W - 2 * pad, 0, rk_color_of(RK_ROLE_BG));
}

static void tip_destroy(void *st) { free(st); }
static const rk_class tip_class = { { sizeof(rk_view_class), "rk_tip", tip_draw, tip_measure, NULL, NULL, tip_destroy }, NULL, NULL };

/* The nearest view up the chain that has a tooltip. */
rk_view *rk__tip_owner(rk_view *v) {
  for (; v; v = v->parent) if (v->tooltip) return v;
  return NULL;
}

void rk__tip_hide(rk_window *w, bool remember) {
  if (w->tip_timer) { rk_app_cancel(w->tip_timer); w->tip_timer = 0; }
  if (!w->tip) return;
  if (w->tip->parent) { rk__view_detach(w->tip); if (remember) w->tip_hid = rk_app_now(); }
  rk_release(w->tip); w->tip = NULL;
  rk_release(w->tip_for); w->tip_for = NULL;
}

static void tip_show(rk_window *w, rk_view *owner) {
  rk__tip_hide(w, false);
  if (!owner->tooltip || !rk__view_window(owner)) return;
  rk__layout_pending();
  rk_view *t = rk__view_create(&tip_class, rk__strdup(owner->tooltip));
  t->access = RK_ACCESS_STATUS;
  rk__setstr(&t->access_label, owner->tooltip);
  rk_size s = tip_measure(t, (rk_size){ INFINITY, INFINITY }, t->state);
  rk_rect a = rk__window_rect(owner);
  float edge = rk_metric_of(RK_SPACE_S);
  bool above = a.y - TIP_GAP - s.h >= edge;
  float x = RK_CLAMP(roundf(a.x + a.w / 2 - s.w / 2), edge, RK_MAX(edge, w->size.w - s.w - edge));
  float y = roundf(above ? a.y - TIP_GAP - s.h : a.y + a.h + TIP_GAP);
  rk__view_add(w->floats, t);
  rk_view_set_frame(t, (rk_rect){ x, y, s.w, s.h });
  w->tip = t;                                              /* the floats layer holds it too; this reference is ours */
  w->tip_for = rk_retain(owner);
  float o0 = 0, o1 = 1;
  rk_transform t0 = t->transform, t1 = t->transform;
  t0.translate_y = above ? 4 : -4;
  double d = rk_duration_of(RK_DURATION_FLOAT);
  rk__animate(t, RK_PROP_OPACITY, &o0, &o1, 1, d, 0, rk__ease_in);
  rk__animate(t, RK_PROP_TRANSFORM, &t0.translate_x, &t1.translate_x, 10, d, 0, rk__ease_in);
}

typedef struct tip_wait { rk_window *w; rk_view *owner; } tip_wait;
static void waited(void *user) {
  tip_wait *tw = user;
  rk_window *w = tw->w;
  w->tip_timer = 0;
  if (!w->closed && tw->owner->parent && rk__tip_owner(w->hover) == tw->owner) tip_show(w, tw->owner);
  rk_release(tw->owner);
  free(tw);
}

/* The pointer moved onto v: its tooltip after tip-delay, or at once right after another one. */
void rk__tip_hover(rk_window *w, rk_view *v) {
  rk_view *owner = rk__tip_owner(v);
  if (owner && owner == w->tip_for) return;
  if (!owner) { if (w->tip_timer || (w->tip && !rk__view_within(w->tip_for, w->focus))) rk__tip_hide(w, true); return; }
  double wait = rk_duration_of(RK_TIMING_TIP_DELAY);
  if (w->tip || rk_app_now() - w->tip_hid < wait) { tip_show(w, owner); return; }
  if (w->tip_timer) rk_app_cancel(w->tip_timer);
  tip_wait *tw = rk__alloc(sizeof *tw);
  tw->w = w; tw->owner = rk_retain(owner);
  w->tip_timer = rk_app_after(wait, false, waited, tw);
}

/* Focus came to v from the keyboard: its tooltip at once. */
void rk__tip_focus(rk_window *w, rk_view *v) {
  rk_view *owner = v && w->focus_visible ? rk__tip_owner(v) : NULL;
  if (owner) tip_show(w, owner);
  else if (w->tip) rk__tip_hide(w, false);
}

void rk_view_set_tooltip(rk_view *v, const char *text) {
  if (!v) return;
  rk__setstr(&v->tooltip, text && *text ? text : NULL);
  rk_window *w = rk__view_window(v);
  if (w && w->tip_for == v) { if (v->tooltip) tip_show(w, v); else rk__tip_hide(w, false); }
}

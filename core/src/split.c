/* split.c: a list beside its detail, as the web's .rk-split. Sideways (the split at least 760 px wide in a window at
   least 481 px tall, the web's window classes) the two stand side by side with a hairline between them; a 9 px handle
   lies over it, its line 2 px and text-medium while focused or dragged; under the pointer it shows the resize shape. The list stops space-m short of the
   line and the detail starts space-l after it; compact the pane is inset by space-m each side. The list starts four tiles wide and
   no pane goes narrower than pane-min; a drag changes the width at once, Left and Right by space-m (with Shift, four
   times that), Home and End to the narrowest and widest, a double click gives the list its own width back. The app
   remembers the width (on_resize). A new item's detail slides in as a facet does.

   Compact one pane shows at a time and the change turns as a place does: to the detail, forward; back to the list,
   the other way. The detail starts with a crumb back to the list; Escape goes back too. Included after controller.c,
   whose turn it uses. */
#include "rk_internal.h"
#include "rk_tokens.h"

typedef struct split {
  rk_view *self, *list, *detail, *handle, *crumb;
  char *title;                                             /* the crumb's; NULL: the word for back */
  float set;                                               /* the list's width as asked; 0: its own */
  void (*back)(rk_view *split, void *user); void *back_user;   /* the user went back to the list */
  bool detail_shown, sideways, laid;                        /* compact: which pane; how it was laid out last */
  struct turn *turn; uint32_t timer; bool going;           /* compact: a turn under way, to the detail or back */
  float drag_x, drag_w; bool dragging; double down_at;
  rk_tween strong;                                         /* the handle's line: 0 a hairline, 1 focused or held */
  void (*resize)(rk_view *split, float width, void *user); void *resize_user;
} split;

static const char *crumb_title(split *s) { return s->title ? s->title : rk__words()->back; }
static float split_own(void) { return 4 * rk_metric_of(RK_TILE_UNIT) + 3 * rk_metric_of(RK_SPACE_S); }
static float split_min(void) { return rk_metric_of(RK_PANE_MIN); }
static float split_max(split *s) { return RK_MAX(split_min(), s->self->frame.w - split_min() - 1); }
static float split_list_w(split *s) { return RK_CLAMP(s->set > 0 ? s->set : split_own(), split_min(), split_max(s)); }

static bool split_wide(split *s) {
  rk_window *w = rk__view_window(s->self);
  return s->self->frame.w >= 760 && (!w || w->size.h >= 481);
}

/* The handle ------------------------------------------------------------------------------------------------------ */

static bool handle_focused(split *s) { rk_window *w = rk__view_window(s->handle); return w && w->focus == s->handle; }

static void handle_emphasis(split *s, bool focused) {
  bool on = s->dragging || focused;   /* as everywhere in the core, nothing changes under the pointer but its shape */
  float to = on ? 1 : 0;
  if (s->strong.to == to) return;
  rk__tween_to(&s->strong, rk_app_now(), to, rk_duration_of(RK_DURATION_SHORT), rk__ease_base);
  rk__view_dirty(s->handle);
  rk__app_request_frame();
}

static void handle_draw(rk_view *v, rk_canvas *c, void *st) {
  split *s = st;
  double now = rk_app_now();
  float k = rk__tween_at(&s->strong, now), lw = 1 + k, H = v->pframe.h;
  rk_color ink = rk__mix(rk_color_of(RK_ROLE_TEXT_LOW), rk_color_of(RK_ROLE_TEXT_MEDIUM), k);
  rk_canvas_fill_rect(c, (rk_rect){ v->pframe.w / 2 - lw / 2, 0, lw, H }, ink);
  if (rk__shows_focus(v)) rk__focus_ring(c, (rk_rect){ 0, 0, v->pframe.w, H }, 0);
  if (rk__tween_moving(&s->strong, now)) { rk__view_dirty(v); rk__app_request_frame(); }
}

static void split_set(split *s, float w, bool tell) {
  s->set = w <= 0 ? 0 : s->self->frame.w > 0 ? RK_CLAMP(roundf(w), split_min(), split_max(s)) : roundf(w);   /* laid out: clamped now */
  rk__view_relayout_up(s->self);
  rk__view_dirty(s->self);
  if (tell && s->resize) s->resize(s->self, s->set, s->resize_user);
}

static bool handle_event(rk_view *v, const rk_event *e, void *st) {
  split *s = st;
  switch (e->type) {
    case RK_EVENT_POINTER_DOWN:
      if (e->button) return false;
      if (e->time - s->down_at < 400) { s->down_at = 0; split_set(s, 0, true); return true; }   /* twice: its own width */
      s->down_at = e->time;
      s->dragging = true; s->drag_x = e->position.x + v->frame.x; s->drag_w = split_list_w(s);
      handle_emphasis(s, handle_focused(s));
      return true;
    case RK_EVENT_POINTER_MOVE:
      v->cursor = RK__CURSOR_COL_RESIZE;
      if (!s->dragging) return false;
      if (fabsf(e->position.x + v->frame.x - s->drag_x) > 2) s->down_at = 0;   /* a drag is not half of a double click */
      split_set(s, s->drag_w + e->position.x + v->frame.x - s->drag_x, false);
      return true;
    case RK_EVENT_POINTER_UP:
      if (!s->dragging) return false;
      s->dragging = false;
      handle_emphasis(s, handle_focused(s));
      if (s->resize) s->resize(s->self, s->set, s->resize_user);
      return true;
    case RK_EVENT_KEY_DOWN: {
      float step = rk_metric_of(RK_SPACE_M) * (e->mods & RK_MOD_SHIFT ? 4 : 1), now = split_list_w(s);
      switch (e->key) {
        case RK_KEY_LEFT: split_set(s, now - step, true); return true;
        case RK_KEY_RIGHT: split_set(s, now + step, true); return true;
        case RK_KEY_HOME: split_set(s, split_min(), true); return true;
        case RK_KEY_END: split_set(s, split_max(s), true); return true;
        default: return false;
      }
    }
    case RK_EVENT_FOCUS: case RK_EVENT_BLUR: handle_emphasis(s, e->type == RK_EVENT_FOCUS); rk__view_dirty(v); return false;   /* on blur the window still names it */
    default: return false;
  }
}

static const rk_class handle_class = { { sizeof(rk_view_class), "rk_split_handle", handle_draw, NULL, NULL, handle_event, NULL }, NULL, NULL };

/* The crumb: back to the list, compact --------------------------------------------------------------------------- */

static void crumb_draw(rk_view *v, rk_canvas *c, void *st) {
  split *s = st;
  rk_textfmt f = rk__textfmt(RK_TEXT_LABEL, 0);
  float H = v->pframe.h, gap = rk_metric_of(RK_SPACE_XS);
  rk_color ink = rk_color_of(v->pressed ? RK_ROLE_TEXT : RK_ROLE_TEXT_HIGH);
  rk_canvas_icon(c, "back", (rk_rect){ 0, (H - 16) / 2, 16, 16 }, ink);
  rk__text_draw(c, crumb_title(s), -1, 16 + gap, (H - f.size * f.line) / 2, &f, ink);
}

static rk_size crumb_measure(rk_view *v, rk_size avail, void *st) {
  (void)v; (void)avail;
  split *s = st;
  rk_textfmt f = rk__textfmt(RK_TEXT_LABEL, 0);
  return (rk_size){ 16 + rk_metric_of(RK_SPACE_XS) + rk__text_width(crumb_title(s), -1, &f), rk_metric_of(RK_CONTROL_HEIGHT) };
}

static const rk_class crumb_class = { { sizeof(rk_view_class), "rk_split_crumb", crumb_draw, crumb_measure, NULL, NULL, NULL }, NULL, NULL };

/* The split -------------------------------------------------------------------------------------------------------- */

static void split_place(split *s) {
  rk_view *v = s->self;
  float W = v->frame.w, H = v->frame.h;
  bool wide = split_wide(s);
  if (s->laid && wide != s->sideways && s->timer) { rk_app_cancel(s->timer); s->timer = 0; rk__turn_settle(s->turn); s->detail_shown = s->going; }
  s->sideways = wide; s->laid = true;
  if (wide) {
    float lw = split_list_w(s);
    rk_view_set_hidden(s->list, false); rk_view_set_hidden(s->detail, false); rk_view_set_hidden(s->handle, false);
    rk_view_set_hidden(s->crumb, true);
    /* the list stops space-m short of the line, the detail starts space-l after it, as the web's panes are padded */
    float m = rk_metric_of(RK_SPACE_M), l = rk_metric_of(RK_SPACE_L);
    rk__set_frame(s->list, (rk_rect){ 0, 0, RK_MAX(0.0f, lw - m), H });
    rk__set_frame(s->handle, (rk_rect){ lw - 4, 0, 9, H });
    rk__set_frame(s->detail, (rk_rect){ lw + 1 + l, 0, RK_MAX(0.0f, W - lw - 1 - l - m), H });
    return;
  }
  rk_view_set_hidden(s->handle, true);
  rk_view_set_hidden(s->list, s->detail_shown);
  rk_view_set_hidden(s->detail, !s->detail_shown);
  rk_view_set_hidden(s->crumb, !s->detail_shown);
  if (!s->title) rk__setstr(&s->crumb->access_label, crumb_title(s));   /* the word for back, as the app has it now */
  float ch = rk_metric_of(RK_CONTROL_HEIGHT), m = rk_metric_of(RK_SPACE_M), iw = RK_MAX(0.0f, W - 2 * m);   /* space-m each side */
  rk__set_frame(s->list, (rk_rect){ m, 0, iw, H });
  rk__set_frame(s->crumb, (rk_rect){ m, 0, crumb_measure(s->crumb, (rk_size){ iw, H }, s).w, ch });
  rk__set_frame(s->detail, (rk_rect){ m, ch, iw, RK_MAX(0.0f, H - ch) });
}

static void split_layout(rk_view *v, void *st) { (void)v; split_place(st); }

static rk_size split_measure(rk_view *v, rk_size avail, void *st) {
  (void)v; (void)st;
  return (rk_size){ isfinite(avail.w) ? avail.w : 960, isfinite(avail.h) ? avail.h : 600 };
}

/* the leaving pane has turned away: the other one turns in and takes the focus */
static void split_arrive(void *user) {
  split *s = user;
  s->timer = 0;
  rk__turn_settle(s->turn);
  s->detail_shown = s->going;
  split_place(s);
  rk__layout_pending();
  rk__turn_arrive(s->self, !s->going);
  rk_window *w = rk__view_window(s->self);
  rk_view *f = rk__first_focusable(s->going ? s->crumb : s->list);
  if (w && f) rk_window_focus(w, f);
}

static bool split_event(rk_view *v, const rk_event *e, void *st) {
  (void)v;
  split *s = st;
  if (e->type == RK_EVENT_KEY_DOWN && e->key == RK_KEY_ESCAPE && !e->mods && !s->sideways && s->going) {
    rk_split_set_detail_shown(s->self, false);                          /* compact, in the detail: back to the list */
    if (s->back) s->back(s->self, s->back_user);
    return true;
  }
  return false;
}

static void split_destroy(void *st) {
  split *s = st;
  if (s->timer) rk_app_cancel(s->timer);
  rk__turn_free(s->turn);
  free(s->title); free(s);
}

static const rk_class split_class = { { sizeof(rk_view_class), "rk_split", NULL, split_measure, split_layout, split_event, split_destroy }, NULL, NULL };

static split *split_of(rk_view *v) { return v && v->cls == &split_class ? v->state : NULL; }

static void crumb_back(rk_view *crumb, void *user) {
  (void)crumb;
  rk_split_set_detail_shown(user, false);
  split *s = split_of(user);
  if (s && s->back) s->back(s->self, s->back_user);
}
void rk_split_on_back(rk_view *v, void (*fn)(rk_view *split, void *user), void *user) {
  split *s = RK__EXPECT(split_of, v, "a split");
  if (s) { s->back = fn; s->back_user = user; }
}

rk_view *rk_split(rk_view *list, rk_view *detail, const char *title) {
  split *s = rk__alloc(sizeof *s);
  s->title = title && *title ? rk__strdup(title) : NULL;
  s->turn = rk__turn_new();
  rk_view *v = rk__view_create(&split_class, s);
  s->self = v;
  s->list = list ? list : rk_view_create();
  s->detail = detail ? detail : rk_view_create();
  s->crumb = rk__view_create(&crumb_class, s);
  s->crumb->focusable = true;
  s->crumb->access = RK_ACCESS_LINK;
  rk__setstr(&s->crumb->access_label, crumb_title(s));
  rk_view_on_activate(s->crumb, crumb_back, v);
  s->handle = rk__view_create(&handle_class, s);
  s->handle->focusable = true; s->handle->draws_focus = true;
  s->handle->access = RK_ACCESS_SEPARATOR;
  rk__view_add(v, s->list); rk_release(s->list);           /* takes the caller's references */
  rk__view_add(v, s->detail); rk_release(s->detail);
  rk__view_add(v, s->crumb); rk_release(s->crumb);
  rk__view_add(v, s->handle); rk_release(s->handle);
  return v;
}

/* Sideways a new item's detail slides in from the trailing side, slide-distance, with a fade, over 0.75 x
   duration-moderate; compact the panes turn. */
void rk_split_set_detail_shown(rk_view *v, bool detail) {
  split *s = RK__EXPECT(split_of, v, "a split");
  if (!s) return;
  if (!s->laid || s->sideways) {
    s->detail_shown = s->going = detail;
    if (!detail || !s->laid) return;
    rk_view *d = s->detail;
    rk_transform t0 = d->transform, t1 = d->transform;
    t0.translate_x += RK_TOK_SLIDE_DISTANCE;
    double dur = 0.75 * rk_duration_of(RK_DURATION_MODERATE);
    float a0 = 0, a1 = d->opacity;
    rk__animate_transform(d, &t0, &t1, dur, 0, rk__ease_out);
    rk__animate(d, RK_PROP_OPACITY, &a0, &a1, 1, dur, 0, rk__ease_out);
    return;
  }
  if (s->timer) { rk_app_cancel(s->timer); split_arrive(s); }  /* a turn still running finishes at once */
  if (detail == s->detail_shown) return;
  s->going = detail;
  double wait = rk__turn_away(s->turn, v, !detail);
  if (wait > 0) s->timer = rk__after_quiet(wait, false, split_arrive, s); else split_arrive(s);
}

bool rk_split_is_sideways(rk_view *v) { split *s = RK__EXPECT(split_of, v, "a split"); return s && s->sideways; }
bool rk_split_is_detail_shown(rk_view *v) { split *s = RK__EXPECT(split_of, v, "a split"); return s && s->detail_shown; }
void rk_split_set_width(rk_view *v, float width) { split *s = RK__EXPECT(split_of, v, "a split"); if (s) split_set(s, width, false); }
float rk_split_width(rk_view *v) { split *s = RK__EXPECT(split_of, v, "a split"); return s ? split_list_w(s) : 0; }
void rk_split_on_resize(rk_view *v, void (*fn)(rk_view *, float, void *), void *user) { split *s = split_of(v); if (s) { s->resize = fn; s->resize_user = user; } }

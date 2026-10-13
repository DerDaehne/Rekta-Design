/* controls.c: the controls that hold a value, as the web's: a checkbox, a group of options, the Metro toggle and
   segments. Every one holds a number - 0 or 1, the chosen index - read and set with rk_control_value and
   rk_control_set_value; rk_control_on_change hears what the user changes (not what the app sets).

   Checkbox: a 20px square with a 2px ink border, filled with the accent and checked when on; Space toggles.
   Options: a 20px circle each, the chosen one ringed and dotted in the accent; the group is one stop for Tab, the
   arrows choose. Toggle: a 46 x 22 square-cornered frame, the accent growing from the leading edge, an ink bar as the
   thumb, the state word beside it; the thumb moves over duration-short. Segments: two to five options side by side
   in joined 2px frames, the chosen one in the accent; the accent block slides to a new choice, and the text turns
   light halfway.

   Slider: a 6px track filled with the accent up to the value, a 10 x 24 ink bar as the thumb, the value beside it in
   tabular figures; a press anywhere moves it there and a drag follows. Stepper: minus, the number, plus, joined in
   1px frames; a press steps, a held one repeats; typing a number and Enter (or leaving) takes it, Escape puts it
   back. Both snap to their step between their minimum and maximum, and know Up, Down, Page Up, Page Down, Home and
   End. Included after widgets.c. */
#include "rk_internal.h"

typedef struct control {
  double value;
  bool disabled;
  void (*change)(rk_view *control, void *user); void *change_user;
  void (*apply)(rk_view *v, double from, double to);       /* the control takes a value: the motion, if any */
  bool (*valid)(rk_view *v, double value);                 /* NULL: any */
  double (*fit)(rk_view *v, double value);                 /* NULL: as given; else the nearest value it can take */
} control;

static float box_size(void) { return 20; }
static rk_textfmt strong_fmt(void) { rk_textfmt f = rk__textfmt(RK_TEXT_STRONG, 0); f.line = 1; return f; }
static rk_textfmt word_fmt(void) { rk_textfmt f = rk__textfmt(RK_TEXT_BODY, 0); f.line = 1; return f; }
static bool shows_focus(rk_view *v) { rk_window *w = rk__view_window(v); return w && w->focus == v && w->focus_visible; }
static void focus_ring(rk_canvas *c, rk_rect r) { rk_canvas_stroke_rect(c, (rk_rect){ r.x - 4, r.y - 4, r.w + 8, r.h + 8 }, 2, rk_color_of(RK_ROLE_FOCUS)); }
static double short_motion(void) { return rk__reduced() ? 0 : rk_duration_of(RK_DURATION_SHORT); }

/* A value the user chose: drawn (with its motion) and told. */
static void user_set(rk_view *v, control *c, double value) {
  if (c->fit) value = c->fit(v, value);
  if (c->disabled || value == c->value || (c->valid && !c->valid(v, value))) return;
  double was = c->value;
  c->value = value;
  if (c->apply) c->apply(v, was, value);
  rk__view_dirty(v);
  if (c->change) c->change(v, c->change_user);
}

/* Checkbox ---------------------------------------------------------------------------------------------------- */

typedef struct check { control c; char *label; bool down; } check;

static rk_size check_measure(rk_view *v, rk_size avail, void *st) {
  (void)v; (void)avail;
  check *k = st;
  rk_textfmt f = strong_fmt();
  float tw = k->label && *k->label ? rk__text_width(k->label, -1, &f) + rk_metric_of(RK_SPACE_S) : 0;
  return (rk_size){ ceilf(box_size() + tw), rk_metric_of(RK_CONTROL_HEIGHT) };
}

static void draw_box(rk_canvas *c, rk_rect b, bool on, bool off) {
  rk_color ink = rk_color_of(off ? RK_ROLE_DISABLED : RK_ROLE_TEXT);
  if (on) {
    rk_canvas_fill_rect(c, b, off ? ink : rk_color_of(RK_ROLE_ACCENT));
    rk_point mark[3] = { { b.x + 5, b.y + 10 }, { b.x + 8.5f, b.y + 13.5f }, { b.x + 15, b.y + 6.5f } };
    rk_canvas_polyline(c, mark, 3, 2, rk_color_of(RK_ROLE_ON_ACCENT));
  } else rk_canvas_stroke_rect(c, b, 2, ink);
}

static void check_draw(rk_view *v, rk_canvas *c, void *st) {
  check *k = st;
  float s = box_size(), h = v->pframe.h;
  rk_rect b = { 0, (h - s) / 2, s, s };
  if (k->down && !k->c.disabled) rk_canvas_fill_rect(c, (rk_rect){ b.x - 4, b.y - 4, s + 8, s + 8 }, rk_color_of(RK_ROLE_HOVER));
  draw_box(c, b, k->c.value != 0, k->c.disabled);
  if (k->label && *k->label) {
    rk_textfmt f = strong_fmt();
    rk__text_draw(c, k->label, -1, s + rk_metric_of(RK_SPACE_S), (h - f.size) / 2, &f, rk_color_of(k->c.disabled ? RK_ROLE_DISABLED : RK_ROLE_TEXT));
  }
  if (shows_focus(v)) focus_ring(c, b);
}

static bool check_event(rk_view *v, const rk_event *e, void *st) {
  check *k = st;
  switch (e->type) {
    case RK_EVENT_POINTER_DOWN: if (e->button) return false; k->down = true; rk__view_dirty(v); return true;
    case RK_EVENT_POINTER_UP:
      if (!k->down) return false;
      k->down = false;
      rk__view_dirty(v);
      if (e->position.x >= 0 && e->position.y >= 0 && e->position.x < v->pframe.w && e->position.y < v->pframe.h) user_set(v, &k->c, k->c.value ? 0 : 1);
      return true;
    case RK_EVENT_KEY_DOWN:
      if (e->key != RK_KEY_SPACE) return false;
      user_set(v, &k->c, k->c.value ? 0 : 1);
      return true;
    case RK_EVENT_FOCUS: case RK_EVENT_BLUR: k->down = false; rk__view_dirty(v); return false;
    default: return false;
  }
}

static void check_destroy(void *st) { check *k = st; free(k->label); free(k); }
static const rk_class check_class = { { sizeof(rk_view_class), "rk_check", check_draw, check_measure, NULL, check_event, check_destroy }, NULL, NULL };

rk_view *rk_check(const char *label, bool on) {
  check *k = rk__alloc(sizeof *k);
  k->label = rk__strdup(label); k->c.value = on ? 1 : 0;
  rk_view *v = rk__view_create(&check_class, k);
  v->focusable = true; v->draws_focus = true; v->hug = true;
  v->access = RK_ACCESS_CHECKBOX;
  rk__setstr(&v->access_label, label);
  return v;
}

/* Options ------------------------------------------------------------------------------------------------------ */

typedef struct choices { control c; char **labels; int n, down; } choices;

static float row_h(void) { return rk_metric_of(RK_CONTROL_HEIGHT); }

static rk_size choices_measure(rk_view *v, rk_size avail, void *st) {
  (void)v; (void)avail;
  choices *o = st;
  rk_textfmt f = strong_fmt();
  float w = 0;
  for (int i = 0; i < o->n; i++) w = RK_MAX(w, rk__text_width(o->labels[i], -1, &f));
  return (rk_size){ ceilf(box_size() + rk_metric_of(RK_SPACE_S) + w), o->n * row_h() };
}

static void choices_draw(rk_view *v, rk_canvas *c, void *st) {
  choices *o = st;
  float s = box_size(), H = row_h();
  rk_textfmt f = strong_fmt();
  rk_color ink = rk_color_of(o->c.disabled ? RK_ROLE_DISABLED : RK_ROLE_TEXT), accent = o->c.disabled ? ink : rk_color_of(RK_ROLE_ACCENT);
  for (int i = 0; i < o->n; i++) {
    rk_rect b = { 0, i * H + (H - s) / 2, s, s };
    bool on = i == (int)o->c.value;
    if (i == o->down && !o->c.disabled) rk_canvas_fill_round_rect(c, (rk_rect){ b.x - 4, b.y - 4, s + 8, s + 8 }, s / 2 + 4, rk_color_of(RK_ROLE_HOVER));
    rk_canvas_fill_round_rect(c, b, s / 2, on ? accent : ink);
    rk_canvas_fill_round_rect(c, (rk_rect){ b.x + 2, b.y + 2, s - 4, s - 4 }, s / 2 - 2, rk_color_of(RK_ROLE_BG));
    if (on) rk_canvas_fill_round_rect(c, (rk_rect){ b.x + 5, b.y + 5, s - 10, s - 10 }, s / 2 - 5, accent);
    rk__text_draw(c, o->labels[i], -1, s + rk_metric_of(RK_SPACE_S), i * H + (H - f.size) / 2, &f, ink);
  }
  if (shows_focus(v)) {
    int at = o->c.value >= 0 ? (int)o->c.value : 0;
    focus_ring(c, (rk_rect){ 0, at * H + (H - s) / 2, s, s });
  }
}

static int option_at(rk_view *v, const choices *o, float y) { (void)v; int i = (int)floorf(y / row_h()); return i >= 0 && i < o->n ? i : -1; }

static bool choices_event(rk_view *v, const rk_event *e, void *st) {
  choices *o = st;
  switch (e->type) {
    case RK_EVENT_POINTER_DOWN: if (e->button) return false; o->down = option_at(v, o, e->position.y); rk__view_dirty(v); return true;
    case RK_EVENT_POINTER_UP: {
      int i = option_at(v, o, e->position.y), was = o->down;
      o->down = -1;
      rk__view_dirty(v);
      if (i >= 0 && i == was) user_set(v, &o->c, i);
      return true;
    }
    case RK_EVENT_KEY_DOWN: {
      int at = (int)o->c.value;
      switch (e->key) {
        case RK_KEY_DOWN: case RK_KEY_RIGHT: user_set(v, &o->c, at < 0 ? 0 : (at + 1) % o->n); return true;
        case RK_KEY_UP: case RK_KEY_LEFT: user_set(v, &o->c, at < 0 ? o->n - 1 : (at - 1 + o->n) % o->n); return true;
        case RK_KEY_SPACE: if (at < 0) user_set(v, &o->c, 0); return true;
        default: return false;
      }
    }
    case RK_EVENT_FOCUS: case RK_EVENT_BLUR: o->down = -1; rk__view_dirty(v); return false;
    default: return false;
  }
}

static bool choices_valid(rk_view *v, double value) { choices *o = v->state; return value == floor(value) && value >= -1 && value < o->n; }
static void choices_destroy(void *st) { choices *o = st; for (int i = 0; i < o->n; i++) free(o->labels[i]); free(o->labels); free(o); }
static const rk_class choices_class = { { sizeof(rk_view_class), "rk_choices", choices_draw, choices_measure, NULL, choices_event, choices_destroy }, NULL, NULL };

static char **copy_labels(const char *const *labels, int n) {
  char **out = rk__alloc((size_t)RK_MAX(n, 1) * sizeof *out);
  for (int i = 0; i < n; i++) out[i] = rk__strdup(labels[i] ? labels[i] : "");
  return out;
}

rk_view *rk_choices(const char *const *options, int count, int chosen) {
  choices *o = rk__alloc(sizeof *o);
  o->n = RK_MAX(0, count); o->labels = copy_labels(options, o->n); o->down = -1;
  o->c.value = chosen >= 0 && chosen < o->n ? chosen : -1;
  o->c.valid = choices_valid;
  rk_view *v = rk__view_create(&choices_class, o);
  v->focusable = true; v->draws_focus = true; v->hug = true;
  v->access = RK_ACCESS_RADIO_GROUP;
  return v;
}

/* Toggle -------------------------------------------------------------------------------------------------------- */

typedef struct toggle { control c; char *label; bool down; } toggle;

#define TRACK_W 46.0f
#define TRACK_H 22.0f
static float toggle_track_x(const toggle *t) {
  rk_textfmt f = strong_fmt();
  return t->label && *t->label ? rk__text_width(t->label, -1, &f) + 2 * rk_metric_of(RK_SPACE_S) : 0;
}
static float word_room(void) { rk_textfmt f = word_fmt(); return RK_MAX(2.2f * f.size, RK_MAX(rk__text_width("on", -1, &f), rk__text_width("off", -1, &f))); }

static rk_size toggle_measure(rk_view *v, rk_size avail, void *st) {
  (void)v; (void)avail;
  return (rk_size){ ceilf(toggle_track_x(st) + TRACK_W + rk_metric_of(RK_SPACE_S) + word_room()), rk_metric_of(RK_CONTROL_HEIGHT) };
}

static void toggle_draw(rk_view *v, rk_canvas *c, void *st) {
  toggle *t = st;
  float h = v->pframe.h, x = toggle_track_x(t), y = (h - TRACK_H) / 2, p = RK_CLAMP(v->pvalue, 0.0f, 1.0f);
  bool off = t->c.disabled;
  rk_color ink = rk_color_of(off ? RK_ROLE_DISABLED : RK_ROLE_TEXT);
  rk_textfmt f = strong_fmt(), wf = word_fmt();
  if (t->label && *t->label) rk__text_draw(c, t->label, -1, 0, (h - f.size) / 2, &f, ink);
  rk_rect track = { x, y, TRACK_W, TRACK_H };
  if (t->down && !off) rk_canvas_fill_rect(c, track, rk_color_of(RK_ROLE_HOVER));
  rk_canvas_stroke_rect(c, track, 2, ink);
  if (p > 0 && !off) rk_canvas_fill_rect(c, (rk_rect){ x + 4, y + 4, p * (TRACK_W - 8 - 14), TRACK_H - 8 }, rk_color_of(RK_ROLE_ACCENT));
  rk_canvas_fill_rect(c, (rk_rect){ x + p * (TRACK_W - 12), y, 12, TRACK_H }, ink);
  rk__text_draw(c, t->c.value ? "on" : "off", -1, x + TRACK_W + rk_metric_of(RK_SPACE_S), (h - wf.size) / 2, &wf, rk_color_of(off ? RK_ROLE_DISABLED : RK_ROLE_TEXT_HIGH));
  if (shows_focus(v)) rk_canvas_stroke_rect(c, (rk_rect){ x - 4, y - 4, TRACK_W + 8, TRACK_H + 8 }, 2, rk_color_of(RK_ROLE_FOCUS));
}

static void toggle_apply(rk_view *v, double from, double to) {
  float a = (float)from, b = (float)to;
  v->value = b;
  rk__animate(v, RK_PROP_VALUE, &a, &b, 1, short_motion(), 0, rk__ease_base);
}

static bool toggle_event(rk_view *v, const rk_event *e, void *st) {
  toggle *t = st;
  switch (e->type) {
    case RK_EVENT_POINTER_DOWN: if (e->button) return false; t->down = true; rk__view_dirty(v); return true;
    case RK_EVENT_POINTER_UP:
      if (!t->down) return false;
      t->down = false;
      rk__view_dirty(v);
      if (e->position.x >= 0 && e->position.y >= 0 && e->position.x < v->pframe.w && e->position.y < v->pframe.h) user_set(v, &t->c, t->c.value ? 0 : 1);
      return true;
    case RK_EVENT_KEY_DOWN:
      if (e->key != RK_KEY_SPACE && e->key != RK_KEY_ENTER) return false;
      user_set(v, &t->c, t->c.value ? 0 : 1);
      return true;
    case RK_EVENT_FOCUS: case RK_EVENT_BLUR: t->down = false; rk__view_dirty(v); return false;
    default: return false;
  }
}

static void toggle_destroy(void *st) { toggle *t = st; free(t->label); free(t); }
static const rk_class toggle_class = { { sizeof(rk_view_class), "rk_toggle", toggle_draw, toggle_measure, NULL, toggle_event, toggle_destroy }, NULL, NULL };

rk_view *rk_toggle(const char *label, bool on) {
  toggle *t = rk__alloc(sizeof *t);
  t->label = rk__strdup(label); t->c.value = on ? 1 : 0; t->c.apply = toggle_apply;
  rk_view *v = rk__view_create(&toggle_class, t);
  v->value = v->pvalue = on ? 1 : 0;
  v->focusable = true; v->draws_focus = true; v->hug = true;
  v->access = RK_ACCESS_SWITCH;
  rk__setstr(&v->access_label, label);
  return v;
}

/* Segments ------------------------------------------------------------------------------------------------------ */

typedef struct segments { control c; char **labels; int n, down, from; float *x; } segments;   /* x: n + 1 edges */

static void segments_place(segments *s) {
  rk_textfmt f = strong_fmt();
  float pad = rk_metric_of(RK_CONTROL_PADDING), x = 0;
  for (int i = 0; i < s->n; i++) { s->x[i] = x; x += ceilf(rk__text_width(s->labels[i], -1, &f) + 2 * pad) - 2; }   /* joined: the frames overlap */
  s->x[s->n] = x + 2;
}

static rk_size segments_measure(rk_view *v, rk_size avail, void *st) {
  (void)v; (void)avail;
  segments *s = st;
  segments_place(s);
  return (rk_size){ s->x[s->n], rk_metric_of(RK_CONTROL_HEIGHT) };
}

/* A segment's box, frames included: they share the 2px between them. */
static rk_rect seg_rect(const segments *s, int i, float h) { return (rk_rect){ s->x[i], 0, (i + 1 < s->n ? s->x[i + 1] + 2 : s->x[s->n]) - s->x[i], h }; }

static void segments_draw(rk_view *v, rk_canvas *c, void *st) {
  segments *s = st;
  segments_place(s);
  float h = v->pframe.h, p = RK_CLAMP(v->pvalue, 0.0f, 1.0f);
  int to = (int)s->c.value;
  bool off = s->c.disabled;
  rk_color ink = rk_color_of(off ? RK_ROLE_DISABLED : RK_ROLE_TEXT);
  if (to >= 0) {                                           /* the accent block, sliding from where it was */
    rk_rect b = seg_rect(s, to, h), a = s->from >= 0 && s->from < s->n ? seg_rect(s, s->from, h) : b;
    rk_rect m = { a.x + (b.x - a.x) * p, 0, a.w + (b.w - a.w) * p, h };
    rk_canvas_fill_rect(c, m, off ? ink : rk_color_of(RK_ROLE_ACCENT));
  }
  for (int i = 0; i < s->n; i++) {
    rk_rect b = seg_rect(s, i, h);
    if (i == s->down && i != to && !off) rk_canvas_fill_rect(c, b, rk_color_of(RK_ROLE_HOVER));
    rk_canvas_stroke_rect(c, b, 2, ink);
    rk_textfmt f = strong_fmt();
    bool lit = i == to && p >= 0.5f;
    float tw = rk__text_width(s->labels[i], -1, &f);
    rk__text_draw(c, s->labels[i], -1, b.x + (b.w - tw) / 2, (h - f.size) / 2, &f, lit ? rk_color_of(RK_ROLE_ON_ACCENT) : ink);
  }
  if (shows_focus(v)) focus_ring(c, seg_rect(s, to >= 0 ? to : 0, h));
}

static void segments_apply(rk_view *v, double from, double to) {
  segments *s = v->state;
  (void)to;
  s->from = (int)from;
  float a = 0, b = 1;
  v->value = 1;
  rk__animate(v, RK_PROP_VALUE, &a, &b, 1, short_motion(), 0, rk__ease_base);
}

static int segment_at(segments *s, float x) { for (int i = 0; i < s->n; i++) if (x >= s->x[i] && x < s->x[i + 1]) return i; return -1; }

static bool segments_event(rk_view *v, const rk_event *e, void *st) {
  segments *s = st;
  switch (e->type) {
    case RK_EVENT_POINTER_DOWN: if (e->button) return false; s->down = segment_at(s, e->position.x); rk__view_dirty(v); return true;
    case RK_EVENT_POINTER_UP: {
      int i = segment_at(s, e->position.x), was = s->down;
      s->down = -1;
      rk__view_dirty(v);
      if (i >= 0 && i == was) user_set(v, &s->c, i);
      return true;
    }
    case RK_EVENT_KEY_DOWN: {
      int at = (int)s->c.value;
      switch (e->key) {
        case RK_KEY_RIGHT: case RK_KEY_DOWN: user_set(v, &s->c, at < 0 ? 0 : (at + 1) % s->n); return true;
        case RK_KEY_LEFT: case RK_KEY_UP: user_set(v, &s->c, at < 0 ? s->n - 1 : (at - 1 + s->n) % s->n); return true;
        case RK_KEY_HOME: user_set(v, &s->c, 0); return true;
        case RK_KEY_END: user_set(v, &s->c, s->n - 1); return true;
        default: return false;
      }
    }
    case RK_EVENT_FOCUS: case RK_EVENT_BLUR: s->down = -1; rk__view_dirty(v); return false;
    default: return false;
  }
}

static bool segments_valid(rk_view *v, double value) { segments *s = v->state; return value == floor(value) && value >= 0 && value < s->n; }
static void segments_destroy(void *st) { segments *s = st; for (int i = 0; i < s->n; i++) free(s->labels[i]); free(s->labels); free(s->x); free(s); }
static const rk_class segments_class = { { sizeof(rk_view_class), "rk_segments", segments_draw, segments_measure, NULL, segments_event, segments_destroy }, NULL, NULL };

rk_view *rk_segments(const char *const *options, int count, int chosen) {
  segments *s = rk__alloc(sizeof *s);
  s->n = RK_CLAMP(count, 0, 5); s->labels = copy_labels(options, s->n); s->down = -1;
  s->x = rk__alloc((size_t)(s->n + 1) * sizeof *s->x);
  s->c.value = chosen >= 0 && chosen < s->n ? chosen : 0; s->from = (int)s->c.value;
  s->c.apply = segments_apply; s->c.valid = segments_valid;
  rk_view *v = rk__view_create(&segments_class, s);
  v->value = v->pvalue = 1;
  v->focusable = true; v->draws_focus = true; v->hug = true;
  v->access = RK_ACCESS_RADIO_GROUP;
  return v;
}

/* Numbers: a range and a step -------------------------------------------------------------------------------------- */

typedef struct range { double min, max, step; } range;

static double range_fit(const range *r, double v) {
  if (v != v) v = r->min;                                  /* NaN */
  if (r->step > 0) v = r->min + round((v - r->min) / r->step) * r->step;
  v = RK_CLAMP(v, r->min, r->max);
  return round(v * 1e6) / 1e6;                             /* no 0.30000000000000004 */
}
static int range_digits(const range *r) {
  int d = 0;
  for (double s = r->step; d < 6 && s > 0 && fabs(s - round(s)) > 1e-9; s *= 10) d++;
  return d;
}
static void range_format(const range *r, double v, char *out, size_t size) { snprintf(out, size, "%.*f", range_digits(r), v); }
static double range_page(const range *r) { double p = (r->max - r->min) / 10; return r->step > 0 ? RK_MAX(r->step, round(p / r->step) * r->step) : p; }

/* The keys both know: a step, a page, the ends. True when the key was one of them. */
static bool range_key(rk_view *v, control *c, const range *r, uint32_t key) {
  double s = r->step > 0 ? r->step : (r->max - r->min) / 100;
  switch (key) {
    case RK_KEY_RIGHT: case RK_KEY_UP: user_set(v, c, c->value + s); return true;
    case RK_KEY_LEFT: case RK_KEY_DOWN: user_set(v, c, c->value - s); return true;
    case RK_KEY_PAGE_UP: user_set(v, c, c->value + range_page(r)); return true;
    case RK_KEY_PAGE_DOWN: user_set(v, c, c->value - range_page(r)); return true;
    case RK_KEY_HOME: user_set(v, c, r->min); return true;
    case RK_KEY_END: user_set(v, c, r->max); return true;
    default: return false;
  }
}

/* Slider -------------------------------------------------------------------------------------------------------- */

typedef struct slider { control c; range r; bool dragging; } slider;

static rk_textfmt value_fmt(void) { rk_textfmt f = rk__textfmt(RK_TEXT_BODY, 0); f.weight = RK_TOK_WEIGHT_MEDIUM; f.tabular = true; f.line = 1; return f; }

static float slider_out_w(const slider *s) {
  rk_textfmt f = value_fmt();
  char a[32], b[32];
  range_format(&s->r, s->r.min, a, sizeof a); range_format(&s->r, s->r.max, b, sizeof b);
  return ceilf(RK_MAX(3.5f * f.size, RK_MAX(rk__text_width(a, -1, &f), rk__text_width(b, -1, &f))));
}
static void slider_track(rk_view *v, const slider *s, float *x0, float *x1) { *x0 = 5; *x1 = RK_MAX(*x0 + 1, v->pframe.w - slider_out_w(s) - rk_metric_of(RK_SPACE_M) - 5); }
static float slider_x(rk_view *v, const slider *s, double value) {
  float x0, x1;
  slider_track(v, s, &x0, &x1);
  double t = s->r.max > s->r.min ? (value - s->r.min) / (s->r.max - s->r.min) : 0;
  return x0 + (float)RK_CLAMP(t, 0.0, 1.0) * (x1 - x0);
}
static double slider_value_at(rk_view *v, const slider *s, float x) {
  float x0, x1;
  slider_track(v, s, &x0, &x1);
  return s->r.min + RK_CLAMP((x - x0) / (x1 - x0), 0.0f, 1.0f) * (s->r.max - s->r.min);
}

static rk_size slider_measure(rk_view *v, rk_size avail, void *st) {
  (void)v;
  return (rk_size){ isfinite(avail.w) ? avail.w : 240 + slider_out_w(st), rk_metric_of(RK_CONTROL_HEIGHT) };
}

static void slider_draw(rk_view *v, rk_canvas *c, void *st) {
  slider *s = st;
  float x0, x1, h = v->pframe.h, cy = h / 2, vx = slider_x(v, s, s->c.value);
  slider_track(v, s, &x0, &x1);
  bool off = s->c.disabled;
  rk_color ink = rk_color_of(off ? RK_ROLE_DISABLED : RK_ROLE_TEXT);
  rk_canvas_fill_rect(c, (rk_rect){ x0 - 5, cy - 3, x1 - x0 + 10, 6 }, rk_color_of(RK_ROLE_TEXT_LOW));
  rk_canvas_fill_rect(c, (rk_rect){ x0 - 5, cy - 3, vx - x0 + 5, 6 }, off ? ink : rk_color_of(RK_ROLE_ACCENT));
  rk_rect thumb = { vx - 5, cy - 12, 10, 24 };
  rk_canvas_fill_rect(c, thumb, ink);
  char text[32];
  range_format(&s->r, s->c.value, text, sizeof text);
  rk_textfmt f = value_fmt();
  rk__text_draw(c, text, -1, v->pframe.w - rk__text_width(text, -1, &f), (h - f.size) / 2, &f, ink);
  if (shows_focus(v)) focus_ring(c, thumb);
}

static bool slider_event(rk_view *v, const rk_event *e, void *st) {
  slider *s = st;
  switch (e->type) {
    case RK_EVENT_POINTER_DOWN:
      if (e->button) return false;
      s->dragging = true;
      user_set(v, &s->c, slider_value_at(v, s, e->position.x));
      return true;
    case RK_EVENT_POINTER_MOVE:
      if (!s->dragging) return false;
      user_set(v, &s->c, slider_value_at(v, s, e->position.x));
      return true;
    case RK_EVENT_POINTER_UP: if (!s->dragging) return false; s->dragging = false; return true;
    case RK_EVENT_KEY_DOWN: return range_key(v, &s->c, &s->r, e->key);
    case RK_EVENT_FOCUS: case RK_EVENT_BLUR: rk__view_dirty(v); return false;
    default: return false;
  }
}

static double slider_fit(rk_view *v, double value) { return range_fit(&((slider *)v->state)->r, value); }
static const rk_class slider_class = { { sizeof(rk_view_class), "rk_slider", slider_draw, slider_measure, NULL, slider_event, free }, NULL, NULL };

rk_view *rk_slider(double min, double max, double step, double value) {
  slider *s = rk__alloc(sizeof *s);
  s->r = (range){ min, RK_MAX(min, max), RK_MAX(0.0, step) };
  s->c.fit = slider_fit;
  s->c.value = range_fit(&s->r, value);
  rk_view *v = rk__view_create(&slider_class, s);
  v->focusable = true; v->draws_focus = true;
  v->access = RK_ACCESS_SLIDER;
  return v;
}

/* Stepper ------------------------------------------------------------------------------------------------------- */

typedef struct stepper { control c; range r; int down; uint32_t repeat; char typed[32]; int ntyped; rk_view *view; } stepper;

static float stepper_bw(void) { return rk_metric_of(RK_CONTROL_HEIGHT); }
static float stepper_fw(void) { rk_textfmt f = rk__textfmt(RK_TEXT_BODY, 0); return ceilf(5.5f * f.size); }

static rk_size stepper_measure(rk_view *v, rk_size avail, void *st) { (void)v; (void)avail; (void)st; return (rk_size){ 2 * stepper_bw() + stepper_fw(), rk_metric_of(RK_CONTROL_HEIGHT) }; }

static void stepper_draw(rk_view *v, rk_canvas *c, void *st) {
  stepper *s = st;
  float bw = stepper_bw(), fw = stepper_fw(), h = v->pframe.h, rad = rk_metric_of(RK_RADIUS_S);
  bool off = s->c.disabled;
  rk_color line = rk_color_of(off ? RK_ROLE_DISABLED : RK_ROLE_TEXT_MEDIUM), ink = rk_color_of(off ? RK_ROLE_DISABLED : RK_ROLE_TEXT);
  rk_rect all = { 0, 0, 2 * bw + fw, h };
  rk_canvas_fill_round_rect(c, all, rad, line);
  rk_canvas_fill_round_rect(c, (rk_rect){ 1, 1, all.w - 2, h - 2 }, RK_MAX(0.0f, rad - 1), rk_color_of(RK_ROLE_BG));
  rk_canvas_fill_rect(c, (rk_rect){ bw, 0, 1, h }, line);
  rk_canvas_fill_rect(c, (rk_rect){ bw + fw - 1, 0, 1, h }, line);
  for (int b = 0; b < 2; b++) {                            /* minus, plus: dimmed at their end */
    float x = b ? bw + fw : 0, cx = x + bw / 2, cy = h / 2;
    bool end = b ? s->c.value >= s->r.max : s->c.value <= s->r.min;
    if (s->down == b && !end && !off) rk_canvas_fill_rect(c, (rk_rect){ x + 1, 1, bw - 2, h - 2 }, rk_color_of(RK_ROLE_HOVER));
    rk_color k = end || off ? rk_color_of(RK_ROLE_DISABLED) : ink;
    rk_canvas_fill_rect(c, (rk_rect){ cx - 6, cy - 1, 12, 2 }, k);
    if (b) rk_canvas_fill_rect(c, (rk_rect){ cx - 1, cy - 6, 2, 12 }, k);
  }
  char text[40];
  if (s->ntyped) snprintf(text, sizeof text, "%s", s->typed); else range_format(&s->r, s->c.value, text, sizeof text);
  rk_textfmt f = rk__textfmt(RK_TEXT_BODY, 0);
  f.tabular = true; f.line = 1;
  float tw = rk__text_width(text, -1, &f);
  rk__text_draw(c, text, -1, bw + (fw - tw) / 2, (h - f.size) / 2, &f, ink);
  if (s->ntyped && shows_focus(v)) rk_canvas_fill_rect(c, (rk_rect){ bw + (fw + tw) / 2 + 1, (h - f.size) / 2 - 2, 1, f.size + 4 }, ink);   /* typing: the caret */
  if (shows_focus(v)) focus_ring(c, all);
}

static void stepper_step(stepper *s, int dir) { user_set(s->view, &s->c, s->c.value + dir * (s->r.step > 0 ? s->r.step : 1)); }

static void stepper_repeat(void *user) {
  stepper *s = user;
  if (s->down < 0) { rk_app_cancel(s->repeat); s->repeat = 0; return; }
  stepper_step(s, s->down ? 1 : -1);
}
static void stepper_hold(void *user) {                     /* held: from now on it repeats */
  stepper *s = user;
  s->repeat = s->down >= 0 ? rk_app_after(80, true, stepper_repeat, s) : 0;
}

static void stepper_take_typed(stepper *s) {
  if (!s->ntyped) return;
  char buf[32];
  snprintf(buf, sizeof buf, "%s", s->typed);
  for (char *p = buf; *p; p++) if (*p == ',') *p = '.';
  char *end = NULL;
  double x = strtod(buf, &end);
  s->ntyped = 0; s->typed[0] = 0;
  if (end && end != buf) user_set(s->view, &s->c, x);
  rk__view_dirty(s->view);
}

static bool stepper_event(rk_view *v, const rk_event *e, void *st) {
  stepper *s = st;
  float bw = stepper_bw(), fw = stepper_fw();
  switch (e->type) {
    case RK_EVENT_POINTER_DOWN: {
      if (e->button) return false;
      stepper_take_typed(s);
      s->down = e->position.x < bw ? 0 : e->position.x >= bw + fw ? 1 : -1;
      if (s->down >= 0) { stepper_step(s, s->down ? 1 : -1); s->repeat = rk_app_after(400, false, stepper_hold, s); }
      rk__view_dirty(v);
      return true;
    }
    case RK_EVENT_POINTER_UP:
      s->down = -1;
      if (s->repeat) { rk_app_cancel(s->repeat); s->repeat = 0; }
      rk__view_dirty(v);
      return true;
    case RK_EVENT_TEXT: {
      if (!e->text || s->c.disabled) return false;
      for (const char *p = e->text; *p; p++) {
        bool ok = (*p >= '0' && *p <= '9') || *p == '.' || *p == ',' || (*p == '-' && !s->ntyped && s->r.min < 0);
        if (ok && s->ntyped < (int)sizeof s->typed - 1) { s->typed[s->ntyped++] = *p; s->typed[s->ntyped] = 0; }
      }
      rk__view_dirty(v);
      return true;
    }
    case RK_EVENT_KEY_DOWN:
      if (s->ntyped) {
        if (e->key == RK_KEY_BACKSPACE) { s->typed[--s->ntyped] = 0; rk__view_dirty(v); return true; }
        if (e->key == RK_KEY_ESCAPE) { s->ntyped = 0; s->typed[0] = 0; rk__view_dirty(v); return true; }
        if (e->key == RK_KEY_ENTER) { stepper_take_typed(s); return true; }
        stepper_take_typed(s);
      }
      return range_key(v, &s->c, &s->r, e->key);
    case RK_EVENT_BLUR: stepper_take_typed(s); s->down = -1; rk__view_dirty(v); return false;
    case RK_EVENT_FOCUS: rk__view_dirty(v); return false;
    default: return false;
  }
}

static double stepper_fit(rk_view *v, double value) { return range_fit(&((stepper *)v->state)->r, value); }
static void stepper_destroy(void *st) { stepper *s = st; if (s->repeat) rk_app_cancel(s->repeat); free(s); }
static const rk_class stepper_class = { { sizeof(rk_view_class), "rk_stepper", stepper_draw, stepper_measure, NULL, stepper_event, stepper_destroy }, NULL, NULL };

rk_view *rk_stepper(double min, double max, double step, double value) {
  stepper *s = rk__alloc(sizeof *s);
  s->r = (range){ min, RK_MAX(min, max), RK_MAX(0.0, step) };
  s->c.fit = stepper_fit;
  s->c.value = range_fit(&s->r, value);
  s->down = -1;
  rk_view *v = rk__view_create(&stepper_class, s);
  s->view = v;
  v->focusable = true; v->draws_focus = true; v->hug = true;
  v->access = RK_ACCESS_SPIN_BUTTON;
  return v;
}

/* The value, for every control ------------------------------------------------------------------------------------ */

static control *control_of(rk_view *v) {
  if (!v) return NULL;
  if (v->cls == &check_class || v->cls == &choices_class || v->cls == &toggle_class || v->cls == &segments_class || v->cls == &slider_class ||
      v->cls == &stepper_class) return v->state;
  return NULL;
}

double rk_control_value(rk_view *v) { control *c = control_of(v); return c ? c->value : 0; }

void rk_control_set_value(rk_view *v, double value) {
  control *c = control_of(v);
  if (c && c->fit) value = c->fit(v, value);
  if (!c || value == c->value || (c->valid && !c->valid(v, value))) return;
  double was = c->value;
  c->value = value;
  if (c->apply) c->apply(v, was, value);
  rk__view_dirty(v);
}

void rk_control_on_change(rk_view *v, void (*fn)(rk_view *, void *), void *user) { control *c = control_of(v); if (c) { c->change = fn; c->change_user = user; } }

void rk_control_set_disabled(rk_view *v, bool disabled) {
  control *c = control_of(v);
  if (!c) return;
  c->disabled = disabled;
  v->focusable = !disabled;
  rk_window *w = rk__view_window(v);
  if (disabled && w && w->focus == v) rk_window_focus(w, NULL);
  rk__view_dirty(v);
}

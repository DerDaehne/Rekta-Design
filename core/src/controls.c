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
   End.

   Select: a box like a field's input with the chosen option and a chevron; a press, Enter, Space or Alt+Down opens a
   menu of the options below it, the chosen one marked; Up and Down change it in place, a letter jumps to the next
   option starting with it.

   Date: a box with the date as 2026-10-13 in tabular figures and a calendar; its value is the number 20261013 (0: no
   date yet). Left and Right go between year, month and day, Up and Down change the one in focus (the day stays in
   its month), digits type it. The calendar, Alt+Down or Enter opens a month below it, Monday first: today ringed, the
   chosen day in the accent; the arrows go by day and week, Page Up and Page Down by month (with Shift, by year),
   Home and End to the month's ends, Enter or a click takes a day, Escape closes. Included after widgets.c. */
#include "rk_internal.h"
#include "rk_tokens.h"

typedef struct control {
  double value;
  void (*change)(rk_view *control, void *user); void *change_user;
  void (*apply)(rk_view *v, double from, double to);       /* the control takes a value: the motion, if any */
  bool (*valid)(rk_view *v, double value);                 /* NULL: any */
  double (*fit)(rk_view *v, double value);                 /* NULL: as given; else the nearest value it can take */
} control;

static float box_size(void) { return 20; }
static rk_textfmt strong_fmt(void) { rk_textfmt f = rk__textfmt(RK_TEXT_STRONG, 0); f.line = 1; return f; }
static rk_textfmt word_fmt(void) { rk_textfmt f = rk__textfmt(RK_TEXT_BODY, 0); f.line = 1; return f; }
static double short_motion(void) { return rk__reduced() ? 0 : rk_duration_of(RK_DURATION_SHORT); }

/* A value the user chose: drawn (with its motion) and told. */
static void user_set(rk_view *v, control *c, double value) {
  if (c->fit) value = c->fit(v, value);
  if (rk__view_disabled(v) || value == c->value || (c->valid && !c->valid(v, value))) return;
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
  if (k->down && !rk__view_disabled(v)) rk_canvas_fill_rect(c, (rk_rect){ b.x - 4, b.y - 4, s + 8, s + 8 }, rk_color_of(RK_ROLE_HOVER));
  draw_box(c, b, k->c.value != 0, rk__view_disabled(v));
  if (k->label && *k->label) {
    rk_textfmt f = strong_fmt();
    rk__text_draw(c, k->label, -1, s + rk_metric_of(RK_SPACE_S), (h - f.size) / 2, &f, rk_color_of(rk__view_disabled(v) ? RK_ROLE_DISABLED : RK_ROLE_TEXT));
  }
  if (rk__shows_focus(v)) rk__focus_ring(c, b, 4);
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
  rk_color ink = rk_color_of(rk__view_disabled(v) ? RK_ROLE_DISABLED : RK_ROLE_TEXT), accent = rk__view_disabled(v) ? ink : rk_color_of(RK_ROLE_ACCENT);
  for (int i = 0; i < o->n; i++) {
    rk_rect b = { 0, i * H + (H - s) / 2, s, s };
    bool on = i == (int)o->c.value;
    if (i == o->down && !rk__view_disabled(v)) rk_canvas_fill_round_rect(c, (rk_rect){ b.x - 4, b.y - 4, s + 8, s + 8 }, s / 2 + 4, rk_color_of(RK_ROLE_HOVER));
    rk_canvas_fill_round_rect(c, b, s / 2, on ? accent : ink);
    rk_canvas_fill_round_rect(c, (rk_rect){ b.x + 2, b.y + 2, s - 4, s - 4 }, s / 2 - 2, rk_color_of(RK_ROLE_BG));
    if (on) rk_canvas_fill_round_rect(c, (rk_rect){ b.x + 5, b.y + 5, s - 10, s - 10 }, s / 2 - 5, accent);
    rk__text_draw(c, o->labels[i], -1, s + rk_metric_of(RK_SPACE_S), i * H + (H - f.size) / 2, &f, ink);
  }
  if (rk__shows_focus(v)) {
    int at = o->c.value >= 0 ? (int)o->c.value : 0;
    rk__focus_ring(c, (rk_rect){ 0, at * H + (H - s) / 2, s, s }, 4);
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
  bool off = rk__view_disabled(v);
  rk_color ink = rk_color_of(off ? RK_ROLE_DISABLED : RK_ROLE_TEXT);
  rk_textfmt f = strong_fmt(), wf = word_fmt();
  if (t->label && *t->label) rk__text_draw(c, t->label, -1, 0, (h - f.size) / 2, &f, ink);
  rk_rect track = { x, y, TRACK_W, TRACK_H };
  if (t->down && !off) rk_canvas_fill_rect(c, track, rk_color_of(RK_ROLE_HOVER));
  rk_canvas_stroke_rect(c, track, 2, ink);
  if (p > 0 && !off) rk_canvas_fill_rect(c, (rk_rect){ x + 4, y + 4, p * (TRACK_W - 8 - 14), TRACK_H - 8 }, rk_color_of(RK_ROLE_ACCENT));
  rk_canvas_fill_rect(c, (rk_rect){ x + p * (TRACK_W - 12), y, 12, TRACK_H }, ink);
  rk__text_draw(c, t->c.value ? "on" : "off", -1, x + TRACK_W + rk_metric_of(RK_SPACE_S), (h - wf.size) / 2, &wf, rk_color_of(off ? RK_ROLE_DISABLED : RK_ROLE_TEXT_HIGH));
  if (rk__shows_focus(v)) rk__focus_ring(c, (rk_rect){ x, y, TRACK_W, TRACK_H }, 4);
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
  for (int i = 0; i < s->n; i++) { s->x[i] = x; x += ceilf(rk__text_width(s->labels[i], -1, &f) + 2 * pad + 4) - 2; }   /* joined: the frames overlap */
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
  bool off = rk__view_disabled(v);
  rk_color ink = rk_color_of(off ? RK_ROLE_DISABLED : RK_ROLE_TEXT), accent = off ? ink : rk_color_of(RK_ROLE_ACCENT);
  float rad = rk_metric_of(RK_RADIUS_S);
  rk_rect all = { 0, 0, s->x[s->n], h };                   /* one frame, rounded at its ends, with lines between */
  if (s->down >= 0 && s->down != to && !off) { rk_canvas_push_clip(c, seg_rect(s, s->down, h)); rk_canvas_fill_round_rect(c, all, rad, rk_color_of(RK_ROLE_HOVER)); rk_canvas_pop_clip(c); }
  if (to >= 0) {                                           /* the accent block, sliding from where it was */
    rk_rect b = seg_rect(s, to, h), a = s->from >= 0 && s->from < s->n ? seg_rect(s, s->from, h) : b;
    float x0 = a.x + (b.x - a.x) * p, x1 = x0 + a.w + (b.w - a.w) * p, W = all.w;
    float l = RK_MAX(x0, rad), r = RK_MIN(x1, W - rad);   /* the middle exact to the subpixel while it slides */
    if (r > l) rk_canvas_fill_rect(c, (rk_rect){ l, 0, r - l, h }, accent);
    if (x0 < rad) { rk_canvas_push_clip(c, (rk_rect){ x0, 0, RK_MIN(x1, rad) - x0, h }); rk_canvas_fill_round_rect(c, all, rad, accent); rk_canvas_pop_clip(c); }
    if (x1 > W - rad) { float e = RK_MAX(x0, W - rad); rk_canvas_push_clip(c, (rk_rect){ e, 0, x1 - e, h }); rk_canvas_fill_round_rect(c, all, rad, accent); rk_canvas_pop_clip(c); }
  }
  rk__canvas_ring(c, all, rad, 2, ink);
  for (int i = 1; i < s->n; i++) rk_canvas_fill_rect(c, (rk_rect){ s->x[i], 0, 2, h }, ink);
  if (to >= 0) {                                           /* the chosen one's frame is the accent's, over its neighbours */
    rk_rect b = seg_rect(s, to, h);
    rk_canvas_push_clip(c, b);
    rk__canvas_ring(c, all, rad, 2, accent);
    rk_canvas_fill_rect(c, (rk_rect){ b.x, 0, 2, h }, accent);
    rk_canvas_fill_rect(c, (rk_rect){ b.x + b.w - 2, 0, 2, h }, accent);
    rk_canvas_pop_clip(c);
  }
  for (int i = 0; i < s->n; i++) {
    rk_rect b = seg_rect(s, i, h);
    rk_textfmt f = strong_fmt();
    bool lit = i == to && p >= 0.5f;
    float tw = rk__text_width(s->labels[i], -1, &f);
    rk__text_draw(c, s->labels[i], -1, b.x + (b.w - tw) / 2, (h - f.size) / 2, &f, lit ? rk_color_of(RK_ROLE_ON_ACCENT) : ink);
  }
  if (rk__shows_focus(v)) rk__focus_ring(c, seg_rect(s, to >= 0 ? to : 0, h), 4);
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

static bool segments_valid(rk_view *v, double value) { segments *s = v->state; return value == floor(value) && value >= -1 && value < s->n; }   /* -1: none yet, as choices and select */
static void segments_destroy(void *st) { segments *s = st; for (int i = 0; i < s->n; i++) free(s->labels[i]); free(s->labels); free(s->x); free(s); }
static const rk_class segments_class = { { sizeof(rk_view_class), "rk_segments", segments_draw, segments_measure, NULL, segments_event, segments_destroy }, NULL, NULL };

rk_view *rk_segments(const char *const *options, int count, int chosen) {
  segments *s = rk__alloc(sizeof *s);
  s->n = RK_CLAMP(count, 0, 5); s->labels = copy_labels(options, s->n); s->down = -1;
  s->x = rk__alloc((size_t)(s->n + 1) * sizeof *s->x);
  s->c.value = chosen >= 0 && chosen < s->n ? chosen : -1; s->from = (int)s->c.value;   /* -1: none yet */
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
  bool off = rk__view_disabled(v);
  rk_color ink = rk_color_of(off ? RK_ROLE_DISABLED : RK_ROLE_TEXT);
  rk_canvas_fill_rect(c, (rk_rect){ x0 - 5, cy - 3, x1 - x0 + 10, 6 }, rk_color_of(RK_ROLE_TEXT_LOW));
  rk_canvas_fill_rect(c, (rk_rect){ x0 - 5, cy - 3, vx - x0 + 5, 6 }, off ? ink : rk_color_of(RK_ROLE_ACCENT));
  rk_rect thumb = { vx - 5, cy - 12, 10, 24 };
  rk_canvas_fill_rect(c, thumb, ink);
  char text[32];
  range_format(&s->r, s->c.value, text, sizeof text);
  rk_textfmt f = value_fmt();
  rk__text_draw(c, text, -1, v->pframe.w - rk__text_width(text, -1, &f), (h - f.size) / 2, &f, ink);
  if (rk__shows_focus(v)) rk__focus_ring(c, thumb, 4);
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
  bool off = rk__view_disabled(v);
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
  if (s->ntyped && rk__shows_focus(v)) rk_canvas_fill_rect(c, (rk_rect){ bw + (fw + tw) / 2 + 1, (h - f.size) / 2 - 2, 1, f.size + 4 }, ink);   /* typing: the caret */
  if (rk__shows_focus(v)) rk__focus_ring(c, all, 4);
}

static void stepper_step(stepper *s, int dir) { user_set(s->view, &s->c, s->c.value + dir * (s->r.step > 0 ? s->r.step : 1)); }

static void stepper_repeat(void *user) {
  stepper *s = user;
  if (s->down < 0) { rk_app_cancel(s->repeat); s->repeat = 0; return; }
  stepper_step(s, s->down ? 1 : -1);
}
static void stepper_hold(void *user) {                     /* held: from now on it repeats */
  stepper *s = user;
  s->repeat = s->down >= 0 ? rk_app_every(80, stepper_repeat, s) : 0;
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
      if (s->down >= 0) { stepper_step(s, s->down ? 1 : -1); s->repeat = rk_app_after(400, stepper_hold, s); }
      rk__view_dirty(v);
      return true;
    }
    case RK_EVENT_POINTER_UP:
      s->down = -1;
      if (s->repeat) { rk_app_cancel(s->repeat); s->repeat = 0; }
      rk__view_dirty(v);
      return true;
    case RK_EVENT_TEXT: {
      if (!e->text || rk__view_disabled(v)) return false;
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

/* Select -------------------------------------------------------------------------------------------------------- */

typedef struct select_box { control c; char **labels; int n; rk_view *menu, *view; bool down; } select_box;

#define SELECT_PAD 12.0f
#define SELECT_CHEVRON 40.0f

static rk_size select_measure(rk_view *v, rk_size avail, void *st) {
  (void)v; (void)avail;
  select_box *s = st;
  rk_textfmt f = rk__textfmt(RK_TEXT_BODY, 0);
  float w = rk__text_width("choose one", -1, &f);
  for (int i = 0; i < s->n; i++) w = RK_MAX(w, rk__text_width(s->labels[i], -1, &f));
  return (rk_size){ RK_MAX(160.0f, ceilf(w + SELECT_PAD + SELECT_CHEVRON)), rk_metric_of(RK_CONTROL_HEIGHT) };
}

static void select_draw(rk_view *v, rk_canvas *c, void *st) {
  select_box *s = st;
  rk_window *w = rk__view_window(v);
  bool focused = w && w->focus == v, off = rk__view_disabled(v), open = rk__menu_open(s->menu);
  float W = v->pframe.w, h = v->pframe.h, r = rk_metric_of(RK_RADIUS_S);
  rk_color edge = rk_color_of(off ? RK_ROLE_DISABLED : focused || open ? RK_ROLE_TEXT : RK_ROLE_TEXT_MEDIUM);
  rk_canvas_fill_round_rect(c, (rk_rect){ 0, 0, W, h }, r, edge);
  rk_canvas_fill_round_rect(c, (rk_rect){ 1, 1, W - 2, h - 2 }, r - 1, rk_color_of(RK_ROLE_BG));
  if (s->down || open) rk_canvas_fill_round_rect(c, (rk_rect){ 1, 1, W - 2, h - 2 }, r - 1, rk_color_of(RK_ROLE_HOVER));
  rk_textfmt f = rk__textfmt(RK_TEXT_BODY, 0);
  f.line = 1;
  int at = (int)s->c.value;
  const char *text = at >= 0 && at < s->n ? s->labels[at] : "choose one";
  rk_canvas_push_clip(c, (rk_rect){ 1, 0, W - SELECT_CHEVRON, h });
  rk__text_draw(c, text, -1, SELECT_PAD, (h - f.size) / 2, &f, rk_color_of(off ? RK_ROLE_DISABLED : at >= 0 ? RK_ROLE_TEXT : RK_ROLE_TEXT_MEDIUM));
  rk_canvas_pop_clip(c);
  float cx = W - SELECT_CHEVRON / 2, cy = h / 2 + (open ? 2 : -2), d = open ? -4 : 4;   /* the chevron: down, up while open */
  rk_point chev[3] = { { cx - 4, cy - d / 2 }, { cx, cy + d / 2 }, { cx + 4, cy - d / 2 } };
  rk_canvas_polyline(c, chev, 3, 1.6f, rk_color_of(off ? RK_ROLE_DISABLED : RK_ROLE_TEXT));
  if (rk__shows_focus(v)) rk__focus_ring(c, (rk_rect){ 0, 0, W, h }, 4);
}

static void select_chosen(rk_view *menu, int item, void *user) { (void)menu; select_box *s = user; user_set(s->view, &s->c, item); }

static void select_open(rk_view *v, select_box *s) {
  if (rk__view_disabled(v) || !s->n) return;
  rk__menu_mark(s->menu, (int)s->c.value);
  rk__menu_shape(s->menu, v->pframe.w, (int)s->c.value);
  rk_menu_open(s->menu, v);
}

static bool select_event(rk_view *v, const rk_event *e, void *st) {
  select_box *s = st;
  switch (e->type) {
    case RK_EVENT_POINTER_DOWN: if (e->button) return false; s->down = true; rk__view_dirty(v); return true;
    case RK_EVENT_POINTER_UP:
      if (!s->down) return false;
      s->down = false;
      rk__view_dirty(v);
      if (e->position.x >= 0 && e->position.y >= 0 && e->position.x < v->pframe.w && e->position.y < v->pframe.h) select_open(v, s);
      return true;
    case RK_EVENT_KEY_DOWN: {
      int at = (int)s->c.value;
      if (e->key == RK_KEY_ENTER || e->key == RK_KEY_SPACE || (e->key == RK_KEY_DOWN && (e->mods & RK_MOD_ALT))) { select_open(v, s); return true; }
      if (e->key == RK_KEY_DOWN) { user_set(v, &s->c, RK_MIN(at + 1, s->n - 1)); return true; }
      if (e->key == RK_KEY_UP) { user_set(v, &s->c, RK_MAX(at - 1, 0)); return true; }
      if (e->key == RK_KEY_HOME) { user_set(v, &s->c, 0); return true; }
      if (e->key == RK_KEY_END) { user_set(v, &s->c, s->n - 1); return true; }
      if (e->key >= 'A' && e->key <= 'Z' && !(e->mods & (RK_MOD_CTRL | RK_MOD_ALT | RK_MOD_SUPER))) {   /* the next option with that letter */
        for (int k = 1; k <= s->n; k++) {
          int i = (RK_MAX(at, -1) + k) % s->n;
          if ((s->labels[i][0] | 32) == (char)(e->key | 32)) { user_set(v, &s->c, i); break; }
        }
        return true;
      }
      return false;
    }
    case RK_EVENT_FOCUS: case RK_EVENT_BLUR: s->down = false; rk__view_dirty(v); return false;
    default: return false;
  }
}

static bool select_valid(rk_view *v, double value) { select_box *s = v->state; return value == floor(value) && value >= -1 && value < s->n; }
static void select_destroy(void *st) {
  select_box *s = st;
  rk_menu_close(s->menu);
  rk_release(s->menu);
  for (int i = 0; i < s->n; i++) free(s->labels[i]);
  free(s->labels); free(s);
}
static const rk_class select_class = { { sizeof(rk_view_class), "rk_select", select_draw, select_measure, NULL, select_event, select_destroy }, NULL, NULL };

rk_view *rk_select(const char *const *options, int count, int chosen) {
  select_box *s = rk__alloc(sizeof *s);
  s->n = RK_MAX(0, count); s->labels = copy_labels(options, s->n);
  s->c.value = chosen >= 0 && chosen < s->n ? chosen : -1;
  s->c.valid = select_valid;
  s->menu = rk_menu();
  for (int i = 0; i < s->n; i++) rk_menu_add(s->menu, s->labels[i], NULL, select_chosen, s);
  rk_view *v = rk__view_create(&select_class, s);
  s->view = v;
  v->focusable = true; v->draws_focus = true; v->hug = true;
  v->access = RK_ACCESS_COMBO_BOX;
  return v;
}

/* Date ---------------------------------------------------------------------------------------------------------- */

static int days_in(int y, int m) {
  static const int n[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  return m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0) ? 29 : n[RK_CLAMP(m, 1, 12) - 1];
}
static int weekday(int y, int m, int d) {                  /* Monday 0 */
  static const int t[12] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
  if (m < 3) y--;
  return ((y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7 + 6) % 7;
}
static int today(void) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  return t ? (t->tm_year + 1900) * 10000 + (t->tm_mon + 1) * 100 + t->tm_mday : 20000101;
}
static int ymd(int y, int m, int d) { y = RK_CLAMP(y, 1, 9999); m = RK_CLAMP(m, 1, 12); return y * 10000 + m * 100 + RK_CLAMP(d, 1, days_in(y, m)); }
static int add_days(int value, int n) {                    /* a day at a time: a month's worth at most */
  int y = value / 10000, m = value / 100 % 100, d = value % 100 + n;
  while (d < 1) { if (--m < 1) { m = 12; y--; } d += days_in(y, m); }
  while (d > days_in(y, m)) { d -= days_in(y, m); if (++m > 12) { m = 1; y++; } }
  return ymd(y, m, d);
}
static int add_months(int value, int n) {
  int y = value / 10000, m = value / 100 % 100 - 1 + n;
  y += m >= 0 ? m / 12 : -((11 - m) / 12);
  m = ((m % 12) + 12) % 12 + 1;
  return ymd(y, m, value % 100);
}

typedef struct date_box { control c; int seg; char typed[5]; int ntyped; bool down; rk_view *view, *sheet; } date_box;
typedef struct date_sheet { date_box *box; int cur; } date_sheet;   /* cur: the day the keys are on */

#define DATE_ICON 40.0f
#define CELL 36.0f

static rk_textfmt date_fmt(void) { rk_textfmt f = rk__textfmt(RK_TEXT_BODY, 0); f.tabular = true; f.line = 1; return f; }

/* Where each of year, month and day starts and ends in the box. */
static void date_segments(float *x0, float *x1) {
  rk_textfmt f = date_fmt();
  float x = SELECT_PAD, dash = rk__text_width("-", -1, &f), w4 = rk__text_width("0000", -1, &f), w2 = rk__text_width("00", -1, &f);
  x0[0] = x; x1[0] = x + w4; x += w4 + dash;
  x0[1] = x; x1[1] = x + w2; x += w2 + dash;
  x0[2] = x; x1[2] = x + w2;
}

static rk_size date_measure(rk_view *v, rk_size avail, void *st) {
  (void)v; (void)avail; (void)st;
  float x0[3], x1[3];
  date_segments(x0, x1);
  return (rk_size){ ceilf(x1[2] + SELECT_PAD + DATE_ICON), rk_metric_of(RK_CONTROL_HEIGHT) };
}

static void date_draw(rk_view *v, rk_canvas *c, void *st) {
  date_box *b = st;
  rk_window *w = rk__view_window(v);
  bool focused = w && w->focus == v, off = rk__view_disabled(v), open = b->sheet && b->sheet->parent;
  float W = v->pframe.w, h = v->pframe.h, r = rk_metric_of(RK_RADIUS_S);
  rk_canvas_fill_round_rect(c, (rk_rect){ 0, 0, W, h }, r, rk_color_of(off ? RK_ROLE_DISABLED : focused || open ? RK_ROLE_TEXT : RK_ROLE_TEXT_MEDIUM));
  rk_canvas_fill_round_rect(c, (rk_rect){ 1, 1, W - 2, h - 2 }, r - 1, rk_color_of(RK_ROLE_BG));
  int value = (int)b->c.value, part[3] = { value / 10000, value / 100 % 100, value % 100 };
  static const char *const empty[3] = { "yyyy", "mm", "dd" };
  float x0[3], x1[3];
  date_segments(x0, x1);
  rk_textfmt f = date_fmt();
  float ty = (h - f.size) / 2;
  for (int i = 0; i < 3; i++) {
    char text[8];
    bool here = focused && i == b->seg && !off;
    if (here && b->ntyped) snprintf(text, sizeof text, "%s", b->typed);
    else if (value) snprintf(text, sizeof text, i ? "%02d" : "%04d", part[i]);
    else snprintf(text, sizeof text, "%s", empty[i]);
    float tw = rk__text_width(text, -1, &f), x = x0[i] + (x1[i] - x0[i] - tw) / 2;
    rk_color ink = rk_color_of(off ? RK_ROLE_DISABLED : value || (here && b->ntyped) ? RK_ROLE_TEXT : RK_ROLE_TEXT_MEDIUM);
    if (here) { rk_canvas_fill_rect(c, (rk_rect){ x0[i] - 2, ty - 3, x1[i] - x0[i] + 4, f.size + 6 }, rk_color_of(RK_ROLE_ACCENT)); ink = rk_color_of(RK_ROLE_ON_ACCENT); }
    rk__text_draw(c, text, -1, x, ty, &f, ink);
    if (i < 2) rk__text_draw(c, "-", -1, x1[i], ty, &f, rk_color_of(off ? RK_ROLE_DISABLED : value ? RK_ROLE_TEXT : RK_ROLE_TEXT_MEDIUM));
  }
  rk_canvas_icon(c, "calendar", (rk_rect){ W - DATE_ICON + 10, (h - 20) / 2, 20, 20 }, rk_color_of(off ? RK_ROLE_DISABLED : RK_ROLE_TEXT));
  if (rk__shows_focus(v)) rk__focus_ring(c, (rk_rect){ 0, 0, W, h }, 4);
}

/* What is typed into the segment in focus, taken. */
static void date_take_typed(date_box *b) {
  if (!b->ntyped) return;
  int n = atoi(b->typed), value = b->c.value ? (int)b->c.value : today();
  int y = value / 10000, m = value / 100 % 100, d = value % 100;
  if (b->seg == 0) y = b->ntyped < 4 ? 2000 + n % 100 : n;   /* two digits: this century */
  else if (b->seg == 1) m = n;
  else d = n;
  b->ntyped = 0; b->typed[0] = 0;
  if ((b->seg == 1 && (m < 1 || m > 12)) || (b->seg == 2 && d < 1) || (b->seg == 0 && y < 1)) { rk__view_dirty(b->view); return; }
  user_set(b->view, &b->c, ymd(y, m, RK_MIN(d, days_in(y, m))));
  rk__view_dirty(b->view);
}

static void date_close(date_box *b, bool give_back) {
  if (!b->sheet || !b->sheet->parent) return;
  rk_window *w = rk__view_window(b->sheet);
  bool had = w && w->focus == b->sheet;
  rk__view_detach(b->sheet);
  if (give_back && had && rk__view_window(b->view) == w) rk_window_focus(w, b->view);
  rk__view_dirty(b->view);
}

static void date_open(date_box *b);

static bool date_event(rk_view *v, const rk_event *e, void *st) {
  date_box *b = st;
  switch (e->type) {
    case RK_EVENT_POINTER_DOWN: {
      if (e->button || rk__view_disabled(v)) return false;
      date_take_typed(b);
      if (e->position.x >= v->pframe.w - DATE_ICON) { b->down = true; return true; }
      float x0[3], x1[3];
      date_segments(x0, x1);
      for (int i = 0; i < 3; i++) if (e->position.x < x1[i] + 4 || i == 2) { b->seg = i; break; }
      rk__view_dirty(v);
      return true;
    }
    case RK_EVENT_POINTER_UP:
      if (b->down && e->position.x >= v->pframe.w - DATE_ICON) date_open(b);
      b->down = false;
      return true;
    case RK_EVENT_TEXT: {
      if (!e->text || rk__view_disabled(v)) return false;
      for (const char *p = e->text; *p; p++) {
        if (*p == '-' || *p == '/' || *p == '.') { date_take_typed(b); b->seg = RK_MIN(b->seg + 1, 2); continue; }
        if (*p < '0' || *p > '9') continue;
        int max = b->seg ? 2 : 4;
        if (b->ntyped < max) { b->typed[b->ntyped++] = *p; b->typed[b->ntyped] = 0; }
        int first = b->typed[0] - '0';
        bool full = b->ntyped == max || (b->seg == 1 && b->ntyped == 1 && first > 1) || (b->seg == 2 && b->ntyped == 1 && first > 3);
        if (full) { int seg = b->seg; date_take_typed(b); b->seg = RK_MIN(seg + 1, 2); }
      }
      rk__view_dirty(v);
      return true;
    }
    case RK_EVENT_KEY_DOWN: {
      if (rk__view_disabled(v)) return false;
      if (b->ntyped) {
        if (e->key == RK_KEY_BACKSPACE) { b->typed[--b->ntyped] = 0; rk__view_dirty(v); return true; }
        if (e->key == RK_KEY_ESCAPE) { b->ntyped = 0; b->typed[0] = 0; rk__view_dirty(v); return true; }
        date_take_typed(b);
      }
      if ((e->key == RK_KEY_DOWN && (e->mods & RK_MOD_ALT)) || e->key == RK_KEY_ENTER || e->key == RK_KEY_F4) { date_open(b); return true; }
      if (e->key == RK_KEY_LEFT && b->seg > 0) { b->seg--; rk__view_dirty(v); return true; }
      if (e->key == RK_KEY_RIGHT && b->seg < 2) { b->seg++; rk__view_dirty(v); return true; }
      if (e->key == RK_KEY_UP || e->key == RK_KEY_DOWN) {
        int dir = e->key == RK_KEY_UP ? 1 : -1, value = b->c.value ? (int)b->c.value : today();
        if (!b->c.value) dir = 0;                          /* the first press: today */
        int y = value / 10000, m = value / 100 % 100, d = value % 100;
        if (b->seg == 0) value = ymd(y + dir, m, RK_MIN(d, days_in(y + dir, m)));
        else if (b->seg == 1) { m = (m - 1 + dir + 12) % 12 + 1; value = ymd(y, m, RK_MIN(d, days_in(y, m))); }
        else { int n = days_in(y, m); value = ymd(y, m, (d - 1 + dir + n) % n + 1); }
        user_set(v, &b->c, value);
        return true;
      }
      return false;
    }
    case RK_EVENT_BLUR: date_take_typed(b); rk__view_dirty(v); return false;
    case RK_EVENT_FOCUS: rk__view_dirty(v); return false;
    default: return false;
  }
}

/* The month ------------------------------------------------------------------------------------------------------ */

static float sheet_pad(void) { return rk_metric_of(RK_SPACE_S); }
static float sheet_head(void) { return rk_metric_of(RK_CONTROL_HEIGHT); }
static float sheet_days_y(void) { rk_textfmt l = rk__textfmt(RK_TEXT_LABEL, 0); return sheet_pad() + sheet_head() + l.size * l.line + rk_metric_of(RK_SPACE_XS); }
static rk_size sheet_size(void) { return (rk_size){ 7 * CELL + 2 * sheet_pad(), sheet_days_y() + 6 * CELL + sheet_pad() }; }

static rk_rect day_cell(int cur, int day) {
  int y = cur / 10000, m = cur / 100 % 100, at = weekday(y, m, 1) + day - 1;
  return (rk_rect){ sheet_pad() + (at % 7) * CELL, sheet_days_y() + (at / 7) * CELL, CELL, CELL };
}

static void sheet_draw(rk_view *v, rk_canvas *c, void *st) {
  date_sheet *s = st;
  static const char *const months[12] = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };
  static const char *const days[7] = { "Mo", "Tu", "We", "Th", "Fr", "Sa", "Su" };
  float W = v->pframe.w, H = v->pframe.h, r = rk_metric_of(RK_RADIUS_S), pad = sheet_pad(), head = sheet_head();
  rk_canvas_fill_round_rect(c, (rk_rect){ 0, 0, W, H }, r, rk_color_of(RK_ROLE_TEXT_MEDIUM));
  rk_canvas_fill_round_rect(c, (rk_rect){ 1, 1, W - 2, H - 2 }, r - 1, rk_color_of(RK_ROLE_BG));
  int y = s->cur / 10000, m = s->cur / 100 % 100;
  rk_color ink = rk_color_of(RK_ROLE_TEXT);
  char title[32];
  snprintf(title, sizeof title, "%s %d", months[m - 1], y);
  rk_textfmt t = strong_fmt();
  rk__text_draw(c, title, -1, (W - rk__text_width(title, -1, &t)) / 2, pad + (head - t.size) / 2, &t, ink);
  for (int k = 0; k < 2; k++) {                            /* the month before and after */
    float cx = k ? W - pad - CELL / 2 : pad + CELL / 2, cy = pad + head / 2, d = k ? 3 : -3;
    rk_point chev[3] = { { cx - d, cy - 6 }, { cx + d, cy }, { cx - d, cy + 6 } };
    rk_canvas_polyline(c, chev, 3, 2, ink);
  }
  rk_textfmt l = rk__textfmt(RK_TEXT_LABEL, 0);
  for (int i = 0; i < 7; i++) rk__text_draw(c, days[i], -1, pad + i * CELL + (CELL - rk__text_width(days[i], -1, &l)) / 2, pad + head, &l, rk_color_of(RK_ROLE_TEXT_HIGH));
  rk_textfmt f = date_fmt();
  int chosen = (int)s->box->c.value, now = today();
  rk_window *w = rk__view_window(v);
  bool keys = w && w->focus == v && w->focus_visible;
  for (int d = 1; d <= days_in(y, m); d++) {
    rk_rect cell = day_cell(s->cur, d);
    int value = y * 10000 + m * 100 + d;
    rk_rect in = { cell.x + 2, cell.y + 2, cell.w - 4, cell.h - 4 };
    rk_color k = ink;
    if (value == chosen) { rk_canvas_fill_rect(c, in, rk_color_of(RK_ROLE_ACCENT)); k = rk_color_of(RK_ROLE_ON_ACCENT); }
    else if (value == now) rk_canvas_stroke_rect(c, in, 1, ink);
    if (value == s->cur && keys) rk__focus_ring(c, cell, 0);
    char text[12];
    snprintf(text, sizeof text, "%d", d);
    rk__text_draw(c, text, -1, cell.x + (cell.w - rk__text_width(text, -1, &f)) / 2, cell.y + (cell.h - f.size) / 2, &f, k);
  }
}

static void sheet_set(rk_view *v, date_sheet *s, int value) { s->cur = value; rk__view_dirty(v); }

static void sheet_take(date_sheet *s, int value) {
  date_box *b = s->box;
  rk_retain(b->view);
  date_close(b, true);
  user_set(b->view, &b->c, value);
  rk_release(b->view);
}

static bool sheet_event(rk_view *v, const rk_event *e, void *st) {
  date_sheet *s = st;
  switch (e->type) {
    case RK_EVENT_POINTER_DOWN: return true;
    case RK_EVENT_POINTER_UP: {
      float pad = sheet_pad(), head = sheet_head(), x = e->position.x, y = e->position.y;
      if (y >= pad && y < pad + head) {
        if (x < pad + CELL) sheet_set(v, s, add_months(s->cur, -1));
        else if (x >= v->pframe.w - pad - CELL) sheet_set(v, s, add_months(s->cur, 1));
        return true;
      }
      int yy = s->cur / 10000, m = s->cur / 100 % 100;
      for (int d = 1; d <= days_in(yy, m); d++) {
        rk_rect c = day_cell(s->cur, d);
        if (x >= c.x && x < c.x + c.w && y >= c.y && y < c.y + c.h) { sheet_take(s, yy * 10000 + m * 100 + d); break; }
      }
      return true;
    }
    case RK_EVENT_KEY_DOWN: {
      bool shift = (e->mods & RK_MOD_SHIFT) != 0;
      int y = s->cur / 10000, m = s->cur / 100 % 100;
      switch (e->key) {
        case RK_KEY_LEFT: sheet_set(v, s, add_days(s->cur, -1)); return true;
        case RK_KEY_RIGHT: sheet_set(v, s, add_days(s->cur, 1)); return true;
        case RK_KEY_UP: sheet_set(v, s, add_days(s->cur, -7)); return true;
        case RK_KEY_DOWN: sheet_set(v, s, add_days(s->cur, 7)); return true;
        case RK_KEY_PAGE_UP: sheet_set(v, s, add_months(s->cur, shift ? -12 : -1)); return true;
        case RK_KEY_PAGE_DOWN: sheet_set(v, s, add_months(s->cur, shift ? 12 : 1)); return true;
        case RK_KEY_HOME: sheet_set(v, s, ymd(y, m, 1)); return true;
        case RK_KEY_END: sheet_set(v, s, ymd(y, m, days_in(y, m))); return true;
        case RK_KEY_ENTER: case RK_KEY_SPACE: sheet_take(s, s->cur); return true;
        case RK_KEY_ESCAPE: case RK_KEY_TAB: date_close(s->box, true); return true;
        default: return false;
      }
    }
    case RK_EVENT_BLUR: date_close(s->box, false); return false;
    default: return false;
  }
}

static const rk_class sheet_class = { { sizeof(rk_view_class), "rk_date_month", sheet_draw, NULL, NULL, sheet_event, free }, NULL, NULL };

/* It opens below the box (above where there is no room), in the window's top layer, with the keys on the chosen day. */
static void date_open(date_box *b) {
  rk_window *w = rk__view_window(b->view);
  if (!w || rk__view_disabled(b->view)) return;
  if (!b->sheet) {
    date_sheet *s = rk__alloc(sizeof *s);
    s->box = b;
    b->sheet = rk__view_create(&sheet_class, s);
    b->sheet->focusable = true; b->sheet->draws_focus = true;
    b->sheet->access = RK_ACCESS_DIALOG;
  }
  if (b->sheet->parent) return;
  rk__menus_close(w);
  ((date_sheet *)b->sheet->state)->cur = b->c.value ? (int)b->c.value : today();
  rk_rect at = rk__window_rect(b->view);
  rk_size sz = sheet_size();
  float gap = rk_metric_of(RK_SPACE_XS), edge = rk_metric_of(RK_SPACE_M);
  float x = RK_CLAMP(at.x, edge, RK_MAX(edge, w->size.w - edge - sz.w)), y = at.y + at.h + gap;
  if (y + sz.h > w->size.h - edge && at.y - gap - sz.h >= edge) y = at.y - gap - sz.h;
  y = RK_CLAMP(y, edge, RK_MAX(edge, w->size.h - edge - sz.h));
  rk__view_add(w->floats, b->sheet);
  rk_view_set_frame(b->sheet, (rk_rect){ x, y, sz.w, sz.h });
  rk__layout_pending();
  rk_window_focus(w, b->sheet);
  float o0 = 0, o1 = 1;
  rk__animate(b->sheet, RK_PROP_OPACITY, &o0, &o1, 1, rk__reduced() ? 0 : rk_duration_of(RK_DURATION_FLOAT), 0, rk__ease_in);
  rk__view_dirty(b->view);
}

static bool date_valid(rk_view *v, double value) {
  (void)v;
  int n = (int)value, y = n / 10000, m = n / 100 % 100, d = n % 100;
  return value == floor(value) && (n == 0 || (y >= 1 && y <= 9999 && m >= 1 && m <= 12 && d >= 1 && d <= days_in(y, m)));
}
static void date_destroy(void *st) {
  date_box *b = st;
  if (b->sheet) { if (b->sheet->parent) rk__view_detach(b->sheet); ((date_sheet *)b->sheet->state)->box = NULL; rk_release(b->sheet); }
  free(b);
}
static const rk_class date_class = { { sizeof(rk_view_class), "rk_date", date_draw, date_measure, NULL, date_event, date_destroy }, NULL, NULL };
static date_box *date_of(rk_view *v) { return v && v->cls == &date_class ? v->state : NULL; }

rk_view *rk_date(int year, int month, int day) {
  date_box *b = rk__alloc(sizeof *b);
  b->c.valid = date_valid;
  int value = year * 10000 + month * 100 + day;
  b->c.value = year && date_valid(NULL, value) ? value : 0;
  rk_view *v = rk__view_create(&date_class, b);
  b->view = v;
  v->focusable = true; v->draws_focus = true; v->hug = true;
  v->access = RK_ACCESS_COMBO_BOX;
  return v;
}

/* A date as its parts: false when none is set yet. */
bool rk_date_value(rk_view *v, int *year, int *month, int *day) {
  date_box *b = RK__EXPECT(date_of, v, "a date");
  int n = b ? (int)b->c.value : 0;
  if (year) *year = n / 10000;
  if (month) *month = n / 100 % 100;
  if (day) *day = n % 100;
  return n != 0;
}
void rk_date_set_value(rk_view *v, int year, int month, int day) {
  if (!RK__EXPECT(date_of, v, "a date")) return;
  rk_control_set_value(v, year ? year * 10000 + month * 100 + day : 0);
}

/* The value, for every control ------------------------------------------------------------------------------------ */

static control *control_of(rk_view *v) {
  if (!v) return NULL;
  if (v->cls == &check_class || v->cls == &choices_class || v->cls == &toggle_class || v->cls == &segments_class || v->cls == &slider_class ||
      v->cls == &stepper_class || v->cls == &select_class || v->cls == &date_class) return v->state;
  return NULL;
}

double rk_control_value(rk_view *v) { control *c = RK__EXPECT(control_of, v, "a control (check, choices, toggle, segments, slider, stepper, select or date)"); return c ? c->value : 0; }

void rk_control_set_value(rk_view *v, double value) {
  control *c = RK__EXPECT(control_of, v, "a control (check, choices, toggle, segments, slider, stepper, select or date)");
  if (c && c->fit) value = c->fit(v, value);
  if (!c || value == c->value || (c->valid && !c->valid(v, value))) return;
  double was = c->value;
  c->value = value;
  if (c->apply) c->apply(v, was, value);
  rk__view_dirty(v);
}

/* What a control says and offers, changed after it is made. A choice the new options lack is let go (-1); a number
   is fitted to the new range. Neither is told to the change handler: the app made the change. */
void rk_control_set_label(rk_view *v, const char *label) {
  char **slot = v && v->cls == &check_class ? &((check *)v->state)->label : v && v->cls == &toggle_class ? &((toggle *)v->state)->label : NULL;
  if (!rk__expect(slot, v, __func__, "a check or a toggle")) return;
  rk__setstr(slot, label);
  rk__setstr(&v->access_label, label);
  rk__view_relayout_up(v);
}

static void swap_labels(char ***labels, int *n, const char *const *options, int count) {
  for (int i = 0; i < *n; i++) free((*labels)[i]);
  free(*labels);
  *n = count; *labels = copy_labels(options, count);
}

void rk_control_set_options(rk_view *v, const char *const *options, int count) {
  if (!rk__expect(v && (v->cls == &choices_class || v->cls == &segments_class || v->cls == &select_class) ? v : NULL, v, __func__,
                  "choices, segments or a select")) return;
  count = RK_MAX(0, count);
  control *c = v->state;
  if (v->cls == &choices_class) { choices *o = v->state; swap_labels(&o->labels, &o->n, options, count); o->down = -1; }
  else if (v->cls == &segments_class) {
    segments *g = v->state;
    swap_labels(&g->labels, &g->n, options, RK_MIN(count, 5));
    free(g->x); g->x = rk__alloc((size_t)(g->n + 1) * sizeof *g->x);
    g->down = -1;
  } else {
    select_box *b = v->state;
    swap_labels(&b->labels, &b->n, options, count);
    rk_menu_close(b->menu);
    rk_menu_clear(b->menu);
    for (int i = 0; i < b->n; i++) rk_menu_add(b->menu, b->labels[i], NULL, select_chosen, b);
  }
  int n = v->cls == &choices_class ? ((choices *)c)->n : v->cls == &segments_class ? ((segments *)c)->n : ((select_box *)c)->n;
  if (c->value >= n) { c->value = -1; if (v->cls == &segments_class) ((segments *)c)->from = -1; }
  rk__view_relayout_up(v);
}

void rk_control_set_range(rk_view *v, double min, double max, double step) {
  range *r = v && v->cls == &slider_class ? &((slider *)v->state)->r : v && v->cls == &stepper_class ? &((stepper *)v->state)->r : NULL;
  if (!rk__expect(r, v, __func__, "a slider or a stepper")) return;
  r->min = min; r->max = RK_MAX(min, max); r->step = step > 0 ? step : 0;
  control *c = v->state;
  double was = c->value;
  c->value = range_fit(r, c->value);
  if (c->apply && was != c->value) c->apply(v, was, c->value);
  rk__view_relayout_up(v);
}

void rk_control_on_change(rk_view *v, void (*fn)(rk_view *, void *), void *user) { control *c = control_of(v); if (c) { c->change = fn; c->change_user = user; } }


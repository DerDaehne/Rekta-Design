/* field.c: a text field, as the web's .rk-field: a label above, the input with a 1px border, a hint or an error
   below. One line of text, edited with the keys a desktop knows (arrows by character, Ctrl by word, Shift to select,
   Home, End, Backspace, Delete, Ctrl+A, Ctrl+C, Ctrl+X, Ctrl+V) and the pointer (a click places the caret, a drag
   selects, a double click takes a word). The text scrolls inside the input to keep the caret in sight.

   The caret blinks for ten seconds after the last edit and then stays lit, so a field left alone wakes nothing.
   Included after widgets.c. */
#include "rk_internal.h"

#define BLINK_MS 530
#define BLINK_FOR_MS 10000
#define PAD 12                                             /* the input's padding on either side, as the web */

typedef struct field {
  char *text; int len, cap;                                /* UTF-8 */
  int caret, anchor;                                       /* byte offsets; the selection is between them */
  float scroll;                                            /* how far the text has moved left to keep the caret in sight */
  char *label, *placeholder, *hint; bool error;
  char *preedit;                                           /* what an input method is composing, shown at the caret */
  bool lit, dragging;                                      /* the caret shows; a drag selects */
  uint32_t blink; double edited, last_click;
  rk_view *view;
  void (*change)(rk_view *, void *); void *change_user;
  void (*submit)(rk_view *, void *); void *submit_user;
} field;

static char *clipboard;                                    /* the app's own, where the platform has none */

static void clip_set(const char *s) {
  if (rk__platform()->clip_set) { rk__platform()->clip_set(s); return; }
  free(clipboard);
  clipboard = rk__strdup(s);
}

static char *clip_get(void) { return rk__platform()->clip_get ? rk__platform()->clip_get() : rk__strdup(clipboard); }

static rk_textfmt input_fmt(void) { rk_textfmt f = rk__textfmt(RK_TEXT_BODY, 0); f.line = 1; return f; }
static rk_textfmt label_fmt(void) { return rk__textfmt(RK_TEXT_LABEL, 0); }
static rk_textfmt hint_fmt(void) { return rk__textfmt(RK_TEXT_SMALL, 0); }

static float label_h(const field *f) { if (!f->label) return 0; rk_textfmt l = label_fmt(); return l.size * l.line + rk_metric_of(RK_SPACE_XS); }
static rk_rect input_rect(rk_view *v, const field *f) { return (rk_rect){ 0, label_h(f), v->pframe.w, rk_metric_of(RK_CONTROL_HEIGHT) }; }

static int sel_lo(const field *f) { return RK_MIN(f->caret, f->anchor); }
static int sel_hi(const field *f) { return RK_MAX(f->caret, f->anchor); }

/* Code points: the caret never stops inside one. */
static int next_char(const field *f, int i) { if (i >= f->len) return f->len; i++; while (i < f->len && ((unsigned char)f->text[i] & 0xc0) == 0x80) i++; return i; }
static int prev_char(const field *f, int i) { if (i <= 0) return 0; i--; while (i > 0 && ((unsigned char)f->text[i] & 0xc0) == 0x80) i--; return i; }
static bool word_char(char c) { return ((unsigned char)c & 0x80) || (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_'; }
static int next_word(const field *f, int i) { while (i < f->len && !word_char(f->text[i])) i++; while (i < f->len && word_char(f->text[i])) i++; return i; }
static int prev_word(const field *f, int i) { while (i > 0 && !word_char(f->text[i - 1])) i--; while (i > 0 && word_char(f->text[i - 1])) i--; return i; }

static float x_at(const field *f, int i) { rk_textfmt t = input_fmt(); return rk__text_width(f->text, i, &t); }

/* The byte offset nearest to x in the text. */
static int offset_at(const field *f, float x) {
  int best = 0;
  float bd = INFINITY;
  for (int i = 0;; i = next_char(f, i)) {
    float d = fabsf(x_at(f, i) - x);
    if (d < bd) { bd = d; best = i; }
    if (i >= f->len) break;
  }
  return best;
}

static void keep_caret_in_sight(rk_view *v, field *f) {
  float inner = v->pframe.w - 2 * PAD, cx = x_at(f, f->caret), w = x_at(f, f->len);
  if (cx - f->scroll > inner) f->scroll = cx - inner;
  if (cx - f->scroll < 0) f->scroll = cx;
  f->scroll = RK_CLAMP(f->scroll, 0.0f, RK_MAX(0.0f, w - inner + 1));
}

static void blink(void *user);

/* An input method places its window by the caret, in window coordinates. */
static void tell_input_method(rk_view *v, field *f, bool on) {
  rk_window *w = rk__view_window(v);
  if (!w || !rk__platform()->text_input) return;
  rk_rect r = rk__window_rect(v), box = input_rect(v, f);
  rk_textfmt t = input_fmt();
  rk_rect caret = { r.x + box.x + PAD - f->scroll + x_at(f, f->caret), r.y + box.y + (box.h - t.size) / 2 - 2, 1, t.size + 4 };
  rk__platform()->text_input(w->native, on, caret);
}

/* An edit or a move: the caret shows at once and blinks a while, then stays lit. */
static void touched(rk_view *v, field *f) {
  f->lit = true;
  f->edited = rk_app_now();
  rk_window *w = rk__view_window(v);
  if (!f->blink && w && w->focus == v && !rk__reduced()) f->blink = rk_app_after(BLINK_MS, true, blink, f);
  keep_caret_in_sight(v, f);
  if (w && w->focus == v) tell_input_method(v, f, true);
  rk__view_dirty(v);
}

static void blink(void *user) {
  field *f = user;
  rk_window *w = rk__view_window(f->view);
  if (!w || w->focus != f->view || rk_app_now() - f->edited > BLINK_FOR_MS) {
    rk_app_cancel(f->blink); f->blink = 0;
    f->lit = true;
  } else f->lit = !f->lit;
  rk__view_dirty(f->view);
}

static void field_changed(rk_view *v, field *f) {
  touched(v, f);
  if (f->change) f->change(v, f->change_user);
}

/* Replaces the selection with s (n bytes): typing, pasting, deleting. */
static void replace(rk_view *v, field *f, const char *s, int n) {
  int lo = sel_lo(f), hi = sel_hi(f);
  if (!n && lo == hi) return;
  f->text = rk__grow(f->text, &f->cap, f->len - (hi - lo) + n + 1, 1);
  memmove(f->text + lo + n, f->text + hi, (size_t)(f->len - hi));
  for (int i = 0; i < n; i++) f->text[lo + i] = (unsigned char)s[i] < 0x20 || s[i] == 0x7f ? ' ' : s[i];   /* one line: breaks become spaces */
  f->len += n - (hi - lo);
  f->text[f->len] = 0;
  f->caret = f->anchor = lo + n;
  rk__setstr(&v->access_value, f->text);
  field_changed(v, f);
}

static void move_to(rk_view *v, field *f, int i, bool extend) {
  f->caret = RK_CLAMP(i, 0, f->len);
  if (!extend) f->anchor = f->caret;
  touched(v, f);
}

static void copy_selection(const field *f) {
  int lo = sel_lo(f), hi = sel_hi(f);
  if (lo == hi) return;
  char *s = rk__alloc((size_t)(hi - lo) + 1);
  memcpy(s, f->text + lo, (size_t)(hi - lo));
  clip_set(s);
  free(s);
}

static bool field_key(rk_view *v, field *f, const rk_event *e) {
  bool shift = (e->mods & RK_MOD_SHIFT) != 0, word = (e->mods & (RK_MOD_CTRL | RK_MOD_ALT)) != 0, cmd = (e->mods & (RK_MOD_CTRL | RK_MOD_SUPER)) != 0;
  bool sel = f->caret != f->anchor;
  switch (e->key) {
    case RK_KEY_LEFT:
      if (sel && !shift) move_to(v, f, sel_lo(f), false);
      else move_to(v, f, word ? prev_word(f, f->caret) : prev_char(f, f->caret), shift);
      return true;
    case RK_KEY_RIGHT:
      if (sel && !shift) move_to(v, f, sel_hi(f), false);
      else move_to(v, f, word ? next_word(f, f->caret) : next_char(f, f->caret), shift);
      return true;
    case RK_KEY_HOME: case RK_KEY_UP: move_to(v, f, 0, shift); return true;
    case RK_KEY_END: case RK_KEY_DOWN: move_to(v, f, f->len, shift); return true;
    case RK_KEY_BACKSPACE:
      if (!sel) f->anchor = word ? prev_word(f, f->caret) : prev_char(f, f->caret);
      replace(v, f, "", 0);
      return true;
    case RK_KEY_DELETE:
      if (!sel) f->anchor = word ? next_word(f, f->caret) : next_char(f, f->caret);
      replace(v, f, "", 0);
      return true;
    case RK_KEY_ENTER:
      if (f->submit) { f->submit(v, f->submit_user); return true; }
      return false;
    case 'A': if (!cmd) return false; f->anchor = 0; f->caret = f->len; touched(v, f); return true;
    case 'C': if (!cmd) return false; copy_selection(f); return true;
    case 'X': if (!cmd) return false; copy_selection(f); replace(v, f, "", 0); return true;
    case 'V': {
      if (!cmd) return false;
      char *s = clip_get();
      if (s) { replace(v, f, s, (int)strlen(s)); free(s); }
      return true;
    }
    default: return false;
  }
}

static bool field_event(rk_view *v, const rk_event *e, void *st) {
  field *f = st;
  rk_rect box = input_rect(v, f);
  float tx = e->position.x - PAD + f->scroll;
  switch (e->type) {
    case RK_EVENT_TEXT:
      if (!e->text) return false;
      replace(v, f, e->text, (int)strlen(e->text));
      return true;
    case RK_EVENT_COMPOSE:
      rk__setstr(&f->preedit, e->text && *e->text ? e->text : NULL);
      rk__view_dirty(v);
      return true;
    case RK_EVENT_KEY_DOWN: return field_key(v, f, e);
    case RK_EVENT_POINTER_DOWN: {
      if (e->button || e->position.y < box.y || e->position.y > box.y + box.h) return e->button == 0;
      int i = offset_at(f, tx);
      if (e->time - f->last_click < 400 && i == f->caret) {    /* a double click: the word */
        f->anchor = prev_word(f, i < f->len && word_char(f->text[i]) ? next_char(f, i) : i);
        f->caret = next_word(f, f->anchor);
        f->last_click = 0;
        touched(v, f);
        return true;
      }
      f->last_click = e->time;
      f->dragging = true;
      move_to(v, f, i, (e->mods & RK_MOD_SHIFT) != 0);
      return true;
    }
    case RK_EVENT_POINTER_MOVE:
      if (!f->dragging) return false;
      move_to(v, f, offset_at(f, tx), true);
      return true;
    case RK_EVENT_POINTER_UP: f->dragging = false; return true;
    case RK_EVENT_FOCUS: touched(v, f); return false;
    case RK_EVENT_BLUR:
      if (f->blink) { rk_app_cancel(f->blink); f->blink = 0; }
      rk__setstr(&f->preedit, NULL);
      tell_input_method(v, f, false);
      f->dragging = false;
      rk__view_dirty(v);
      return false;
    default: return false;
  }
}

static void field_draw(rk_view *v, rk_canvas *c, void *st) {
  field *f = st;
  rk_window *w = rk__view_window(v);
  bool focused = w && w->focus == v;
  rk_rect box = input_rect(v, f);
  if (f->label) { rk_textfmt l = label_fmt(); rk__text_draw(c, f->label, -1, 0, 0, &l, rk_color_of(RK_ROLE_TEXT_HIGH)); }
  /* the input: 1px border in text-medium, the text colour while focused, 2px in error-full for an error */
  float bw = f->error ? 2 : 1, r = rk_metric_of(RK_RADIUS_S);
  rk_color border = rk_color_of(f->error ? RK_ROLE_ERROR_FULL : focused ? RK_ROLE_TEXT : RK_ROLE_TEXT_MEDIUM);
  rk_canvas_fill_round_rect(c, box, r, border);
  rk_canvas_fill_round_rect(c, (rk_rect){ box.x + bw, box.y + bw, box.w - 2 * bw, box.h - 2 * bw }, RK_MAX(0.0f, r - bw), rk_color_of(RK_ROLE_BG));
  if (focused) rk_canvas_stroke_rect(c, (rk_rect){ box.x - 4, box.y - 4, box.w + 8, box.h + 8 }, 2, rk_color_of(RK_ROLE_FOCUS));
  rk_textfmt t = input_fmt();
  float ty = box.y + (box.h - t.size) / 2, x0 = box.x + PAD - f->scroll;
  rk_canvas_push_clip(c, (rk_rect){ box.x + bw, box.y, box.w - 2 * bw, box.h });
  if (!f->len && f->placeholder) rk__text_draw(c, f->placeholder, -1, box.x + PAD, ty, &t, rk_color_of(RK_ROLE_TEXT_MEDIUM));
  if (focused && f->preedit) {                               /* composing: the text so far, underlined, at the caret */
    float cx = x0 + x_at(f, f->caret), pw = rk__text_width(f->preedit, -1, &t);
    rk__text_draw(c, f->text, f->caret, x0, ty, &t, rk_color_of(RK_ROLE_TEXT));
    rk__text_draw(c, f->preedit, -1, cx, ty, &t, rk_color_of(RK_ROLE_TEXT));
    rk_canvas_fill_rect(c, (rk_rect){ cx, ty + t.size + 1, pw, 1 }, rk_color_of(RK_ROLE_TEXT));
    rk__text_draw(c, f->text + f->caret, f->len - f->caret, cx + pw, ty, &t, rk_color_of(RK_ROLE_TEXT));
    rk_canvas_fill_rect(c, (rk_rect){ cx + pw, ty - 2, 1, t.size + 4 }, rk_color_of(RK_ROLE_TEXT));
    rk_canvas_pop_clip(c);
    goto hint;
  }
  rk__text_draw(c, f->text, f->len, x0, ty, &t, rk_color_of(RK_ROLE_TEXT));
  int lo = sel_lo(f), hi = sel_hi(f);
  if (focused && lo != hi) {                                 /* the selection: the accent, its text on it */
    float a = x0 + x_at(f, lo), b = x0 + x_at(f, hi);
    rk_canvas_push_clip(c, (rk_rect){ a, box.y + 4, b - a, box.h - 8 });
    rk_canvas_fill_rect(c, (rk_rect){ a, box.y + 4, b - a, box.h - 8 }, rk_color_of(RK_ROLE_ACCENT));
    rk__text_draw(c, f->text, f->len, x0, ty, &t, rk_color_of(RK_ROLE_ON_ACCENT));
    rk_canvas_pop_clip(c);
  }
  if (focused && lo == hi && f->lit) rk_canvas_fill_rect(c, (rk_rect){ x0 + x_at(f, f->caret), ty - 2, 1, t.size + 4 }, rk_color_of(RK_ROLE_TEXT));
  rk_canvas_pop_clip(c);
hint:
  if (f->hint) {
    rk_textfmt h = hint_fmt();
    float hy = box.y + box.h + rk_metric_of(RK_SPACE_XS), hx = 0;
    if (f->error) { rk_canvas_fill_rect(c, (rk_rect){ 0, hy + (h.size * h.line - 8) / 2, 8, 8 }, rk_color_of(RK_ROLE_ERROR_FULL)); hx = 14; }
    rk__text_draw(c, f->hint, -1, hx, hy, &h, rk_color_of(f->error ? RK_ROLE_TEXT : RK_ROLE_TEXT_HIGH));
  }
}

static rk_size field_measure(rk_view *v, rk_size avail, void *st) {
  (void)v;
  field *f = st;
  float h = label_h(f) + rk_metric_of(RK_CONTROL_HEIGHT);
  if (f->hint) { rk_textfmt t = hint_fmt(); h += rk_metric_of(RK_SPACE_XS) + t.size * t.line; }
  return (rk_size){ isfinite(avail.w) ? avail.w : 320, h };
}

static void field_destroy(void *st) {
  field *f = st;
  if (f->blink) rk_app_cancel(f->blink);
  free(f->text); free(f->label); free(f->placeholder); free(f->hint); free(f->preedit); free(f);
}

static const rk_class field_class = { { sizeof(rk_view_class), "rk_field", field_draw, field_measure, NULL, field_event, field_destroy }, NULL, NULL };

static field *field_of(rk_view *v) { return v && v->cls == &field_class ? v->state : NULL; }

rk_view *rk_field(const char *label, const char *placeholder) {
  field *f = rk__alloc(sizeof *f);
  f->text = rk__alloc(1); f->cap = 1;
  f->label = rk__strdup(label); f->placeholder = rk__strdup(placeholder);
  rk_view *v = rk__view_create(&field_class, f);
  f->view = v;
  v->focusable = true; v->draws_focus = true;
  v->access = RK_ACCESS_TEXT_FIELD;
  rk__setstr(&v->access_label, label ? label : placeholder);
  return v;
}

const char *rk_field_text(rk_view *v) { field *f = field_of(v); return f ? f->text : ""; }

void rk_field_set_text(rk_view *v, const char *text) {
  field *f = field_of(v);
  if (!f) return;
  f->anchor = 0; f->caret = f->len;
  replace(v, f, text ? text : "", text ? (int)strlen(text) : 0);
}

void rk_field_set_hint(rk_view *v, const char *hint, bool error) {
  field *f = field_of(v);
  if (!f) return;
  rk__setstr(&f->hint, hint);
  f->error = hint && error;
  rk__view_relayout_up(v);
  rk__view_dirty(v);
}

void rk_field_on_change(rk_view *v, void (*fn)(rk_view *, void *), void *user) { field *f = field_of(v); if (f) { f->change = fn; f->change_user = user; } }
void rk_field_on_submit(rk_view *v, void (*fn)(rk_view *, void *), void *user) { field *f = field_of(v); if (f) { f->submit = fn; f->submit_user = user; } }

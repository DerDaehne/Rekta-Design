/* field.c: a text field, as the web's .rk-field: a label above, the input with a 1px border, a hint or an error
   below. Edited with the keys a desktop knows (arrows by character, Ctrl by word, Shift to select, Home, End,
   Backspace, Delete, Ctrl+A, Ctrl+C, Ctrl+X, Ctrl+V) and the pointer (a click places the caret, a drag selects, a
   double click takes a word). The text scrolls inside the input to keep the caret in sight.

   One line by default; rk_field_set_lines makes it a text area, as the web's textarea rows: the text wraps at the
   input's width (at spaces, inside a word only when the word is wider than the line), Enter breaks a line, Ctrl+Enter
   submits, Up and Down keep to the column they started from, Home and End keep to the line. Both are the same
   code: a one-line field is a text area with one line that never wraps.

   rk_field_set_options makes it a combobox: a list of the options that contain what is typed floats below the
   input without taking focus; Down opens it or moves down it, Up moves up, Enter takes the active option, Escape
   closes it, the pointer chooses too. What is typed need not be an option.

   The caret blinks for ten seconds after the last edit and then stays lit, so a field left alone wakes nothing.
   Included after widgets.c. */
#include "rk_internal.h"

#define BLINK_MS 530
#define BLINK_FOR_MS 10000
#define PAD 12                                             /* the input's padding on either side, as the web */

typedef struct field {
  char *text; int len, cap;                                /* UTF-8 */
  int caret, anchor;                                       /* byte offsets; the selection is between them */
  float scroll, scroll_y;                                  /* how far the text has moved left (one line) or up (several) */
  int rows;                                                /* the lines the input shows; 1 is a one-line field */
  float box_h;                                             /* the input's height for one line; 0: control-height */
  int *starts, nlines, lcap;                               /* where each line begins, as laid out for wrap_w */
  float wrap_w, goal, drawn_w;                             /* the width laid out for (and last drawn at); Up and Down keep to the goal's x */
  bool stale;                                              /* the text changed since the lines were laid out */
  char *label, *placeholder, *hint; bool error;
  bool secret;                                             /* a password: dots drawn, nothing copied or read aloud */
  char *preedit;                                           /* what an input method is composing, shown at the caret */
  struct snap { char *text; int caret, anchor; } *undo, *redo; int nundo, ucap, nredo, rcap;   /* the field's own history */
  double typed_at; bool typing;                            /* typing runs together into one step, word by word */
  bool lit, dragging;                                      /* the caret shows; a drag selects */
  uint32_t blink; double edited, last_click;
  rk_view *view;
  char **options; int noptions; rk_view *suggest; int *shown, nshown;   /* a combobox: the options, the list, which it shows */
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

static bool multi(const field *f) { return f->rows > 1; }
static rk_textfmt input_fmt(const field *f) { rk_textfmt t = rk__textfmt(RK_TEXT_BODY, 0); if (!multi(f)) t.line = 1; return t; }
static rk_textfmt label_fmt(void) { return rk__textfmt(RK_TEXT_LABEL, 0); }
static rk_textfmt hint_fmt(void) { return rk__textfmt(RK_TEXT_SMALL, 0); }

static float line_h(const field *f) { rk_textfmt t = input_fmt(f); return t.size * t.line; }
static float control_h(const field *f) { return f->box_h > 0 ? f->box_h : rk_metric_of(RK_CONTROL_HEIGHT); }
static float input_pad_y(const field *f) { return (control_h(f) - line_h(f)) / 2; }   /* one line sits as in a field */
static float label_h(const field *f) { if (!f->label) return 0; rk_textfmt l = label_fmt(); return l.size * l.line + rk_metric_of(RK_SPACE_XS); }
static rk_rect input_rect(rk_view *v, const field *f) {
  return (rk_rect){ 0, label_h(f), v->pframe.w, control_h(f) + (f->rows - 1) * line_h(f) };
}

static int sel_lo(const field *f) { return RK_MIN(f->caret, f->anchor); }
static int sel_hi(const field *f) { return RK_MAX(f->caret, f->anchor); }

/* Code points: the caret never stops inside one. */
static int next_char(const field *f, int i) { if (i >= f->len) return f->len; i++; while (i < f->len && ((unsigned char)f->text[i] & 0xc0) == 0x80) i++; return i; }
static int prev_char(const field *f, int i) { if (i <= 0) return 0; i--; while (i > 0 && ((unsigned char)f->text[i] & 0xc0) == 0x80) i--; return i; }
static bool word_char(char c) { return ((unsigned char)c & 0x80) || (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_'; }
static int next_word(const field *f, int i) { while (i < f->len && !word_char(f->text[i])) i++; while (i < f->len && word_char(f->text[i])) i++; return i; }
static int prev_word(const field *f, int i) { while (i > 0 && !word_char(f->text[i - 1])) i--; while (i > 0 && word_char(f->text[i - 1])) i--; return i; }

#define DOT "\xe2\x80\xa2"                                 /* what a secret field shows for each character */
static int chars_in(const field *f, int a, int b) { int n = 0; for (int i = a; i < b; i++) n += ((unsigned char)f->text[i] & 0xc0) != 0x80; return n; }
static float width(const field *f, int a, int b) {
  rk_textfmt t = input_fmt(f);
  if (f->secret) return chars_in(f, a, b) * rk__text_width(DOT, -1, &t);
  return rk__text_width(f->text + a, b - a, &t);
}
/* The text from a to b, or as many dots */
static void draw_run(rk_canvas *c, const field *f, int a, int b, float x, float y, const rk_textfmt *t, rk_color ink) {
  if (!f->secret) { rk__text_draw(c, f->text + a, b - a, x, y, t, ink); return; }
  int n = chars_in(f, a, b);
  char *dots = rk__alloc((size_t)n * 3 + 1);
  for (int i = 0; i < n; i++) memcpy(dots + i * 3, DOT, 3);
  rk__text_draw(c, dots, n * 3, x, y, t, ink);
  free(dots);
}
static void tell_access(rk_view *v, const field *f) { rk__setstr(&v->access_value, f->secret ? "" : f->text); }

static void add_line(field *f, int start) { f->starts = rk__grow(f->starts, &f->lcap, f->nlines + 1, sizeof *f->starts); f->starts[f->nlines++] = start; }

/* Lays the text out in lines: at each break, and in a text area where the next word would not fit. Spaces after a
   wrap stay at the end of the line they follow, as a browser hangs them. Again only when the text or width changed. */
static void wrap(rk_view *v, field *f) {
  float inner = v->pframe.w - 2 * PAD;
  if (!f->stale && f->nlines && inner == f->wrap_w) return;
  f->stale = false; f->wrap_w = inner; f->nlines = 0;
  for (int start = 0;;) {
    add_line(f, start);
    if (!multi(f)) return;
    const char *nl = memchr(f->text + start, '\n', (size_t)(f->len - start));
    int hard = nl ? (int)(nl - f->text) : f->len, fit = hard;
    if (inner > 0 && width(f, start, hard) > inner) {
      fit = start;
      for (int j = start; j < hard;) {                       /* whole words (CJK: characters) while they fit */
        int k = rk__text_next_break(f->text, j, hard), vis = k;
        while (vis > start && f->text[vis - 1] == ' ') vis--;
        if (width(f, start, vis) > inner) break;
        fit = j = k;
      }
      if (fit == start) {                                    /* a word wider than the line breaks where it must */
        fit = next_char(f, start);
        while (fit < hard && width(f, start, next_char(f, fit)) <= inner) fit = next_char(f, fit);
      }
      while (fit < hard && f->text[fit] == ' ') fit++;
    }
    if (fit < hard) { start = fit; continue; }
    if (hard == f->len) return;
    start = hard + 1;
  }
}

static int line_of(const field *f, int i) {
  int lo = 0, hi = f->nlines - 1;
  while (lo < hi) { int m = (lo + hi + 1) / 2; if (f->starts[m] <= i) lo = m; else hi = m - 1; }
  return lo;
}
/* Where line k's text stops (before its break), and the last place the caret can stand on it. */
static int line_stop(const field *f, int k) { if (k + 1 >= f->nlines) return f->len; int e = f->starts[k + 1]; return f->text[e - 1] == '\n' ? e - 1 : e; }
static int line_last(const field *f, int k) { int e = line_stop(f, k); return k + 1 < f->nlines && e == f->starts[k + 1] ? prev_char(f, e) : e; }

static float x_at(const field *f, int i) { return width(f, f->starts[line_of(f, i)], i); }

/* The byte offset on line k nearest to x. */
static int offset_in_line(const field *f, int k, float x) {
  int a = f->starts[k], z = line_last(f, k), best = a;
  float bd = INFINITY;
  for (int i = a;; i = next_char(f, i)) {
    float d = fabsf(width(f, a, i) - x);
    if (d < bd) { bd = d; best = i; }
    if (i >= z) break;
  }
  return best;
}

/* The offset under a point in the view. */
static int offset_at(rk_view *v, const field *f, rk_point p) {
  rk_rect box = input_rect(v, f);
  int k = RK_CLAMP((int)floorf((p.y - box.y - input_pad_y(f) + f->scroll_y) / line_h(f)), 0, f->nlines - 1);
  return offset_in_line(f, k, p.x - box.x - PAD + f->scroll);
}

static void keep_caret_in_sight(rk_view *v, field *f) {
  if (v->pframe.w <= 2 * PAD) return;                      /* not laid out yet: the draw places it */
  wrap(v, f);
  if (multi(f)) {
    float lh = line_h(f), inner = f->rows * lh, y = line_of(f, f->caret) * lh;
    if (y + lh - f->scroll_y > inner) f->scroll_y = y + lh - inner;
    if (y < f->scroll_y) f->scroll_y = y;
    f->scroll_y = RK_CLAMP(f->scroll_y, 0.0f, RK_MAX(0.0f, f->nlines * lh - inner));
    return;
  }
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
  rk_textfmt t = input_fmt(f);
  float lh = line_h(f), top = box.y + input_pad_y(f) + line_of(f, f->caret) * lh - f->scroll_y + (lh - t.size) / 2;
  rk_rect caret = { r.x + box.x + PAD - f->scroll + x_at(f, f->caret), r.y + top - 2, 1, t.size + 4 };
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

/* The options: those that contain what is typed (all, when nothing is), the one it is marked. */
static char fold(char c) { return c >= 'A' && c <= 'Z' ? (char)(c + 32) : c; }   /* ASCII; other letters as they are */
static bool contains(const char *hay, const char *needle) {
  size_t n = strlen(needle);
  for (const char *h = hay; *h; h++) {
    size_t i = 0;
    while (i < n && h[i] && fold(h[i]) == fold(needle[i])) i++;
    if (i == n) return true;
  }
  return n == 0;
}
static bool same(const char *a, const char *b) { while (*a && fold(*a) == fold(*b)) { a++; b++; } return !*a && !*b; }

static void suggest_chosen(rk_view *menu, int item, void *user);

static void suggest(rk_view *v, field *f, bool open) {
  if (!f->suggest) return;
  rk_menu_clear(f->suggest);
  f->nshown = 0;
  int mark = -1;
  for (int i = 0; i < f->noptions; i++) {
    if (!contains(f->options[i], f->text)) continue;
    if (same(f->options[i], f->text)) mark = f->nshown;
    rk_menu_add(f->suggest, f->options[i], NULL, suggest_chosen, f);
    f->shown[f->nshown++] = i;
  }
  rk__menu_mark(f->suggest, mark);
  rk_window *w = rk__view_window(v);
  if (!f->nshown || !w || w->focus != v || (!open && !rk__menu_open(f->suggest))) { rk_menu_close(f->suggest); return; }
  rk_rect r = rk__window_rect(v), box = input_rect(v, f);
  rk__menu_shape(f->suggest, box.w, -1);
  rk__menu_show_passive(f->suggest, v, (rk_rect){ r.x + box.x, r.y + box.y, box.w, box.h });
}

static struct snap snap_now(const field *f) { struct snap s = { rk__strdup(f->text), f->caret, f->anchor }; return s; }
static void snaps_drop(struct snap *s, int *n) { while (*n) free(s[--*n].text); }

/* Remembers the text before an edit - unless it continues the typing before it: one step a word, as a desktop does. */
static void remember(field *f, bool typing, const char *s, int n) {
  double now = rk_app_now();
  bool joins = typing && f->typing && now - f->typed_at < 1000 && f->caret == f->anchor && !(n == 1 && (s[0] == ' ' || s[0] == '\n'));
  f->typing = typing; f->typed_at = now;
  if (joins) return;
  if (f->nundo == 100) { free(f->undo[0].text); memmove(f->undo, f->undo + 1, 99 * sizeof *f->undo); f->nundo--; }
  f->undo = rk__grow(f->undo, &f->ucap, f->nundo + 1, sizeof *f->undo);
  f->undo[f->nundo++] = snap_now(f);
  snaps_drop(f->redo, &f->nredo);
}

static void restore(rk_view *v, field *f, struct snap *from, int *nfrom, struct snap **to, int *nto, int *tocap) {
  if (!*nfrom) return;
  *to = rk__grow(*to, tocap, *nto + 1, sizeof **to);
  (*to)[(*nto)++] = snap_now(f);
  struct snap s = from[--*nfrom];
  free(f->text);
  f->text = s.text; f->len = (int)strlen(s.text); f->cap = f->len + 1;
  f->caret = RK_MIN(s.caret, f->len); f->anchor = RK_MIN(s.anchor, f->len);
  f->typing = false; f->stale = true;
  tell_access(v, f);
  field_changed(v, f);
}

/* Replaces the selection with s (n bytes): typing, pasting, deleting. */
static void replace_as(rk_view *v, field *f, const char *s, int n, bool typing);
static void replace(rk_view *v, field *f, const char *s, int n) { replace_as(v, f, s, n, false); }

static void suggest_chosen(rk_view *menu, int item, void *user) {   /* an option taken: it is the text now, as an edit */
  (void)menu;
  field *f = user;
  if (item < 0 || item >= f->nshown) return;
  const char *o = f->options[f->shown[item]];
  int caret = f->caret, anchor = f->anchor;
  f->anchor = 0; f->caret = f->len;
  replace(f->view, f, o, (int)strlen(o));
  if (f->nundo) { f->undo[f->nundo - 1].caret = caret; f->undo[f->nundo - 1].anchor = anchor; }   /* undone: the caret where it was */
}
static void replace_as(rk_view *v, field *f, const char *s, int n, bool typing) {
  int lo = sel_lo(f), hi = sel_hi(f);
  if (!n && lo == hi) return;
  char *in = rk__alloc((size_t)n + 1);                      /* breaks as \n in a text area, spaces in one line; no other controls */
  int m = 0;
  for (int i = 0; i < n; i++) {
    unsigned char ch = (unsigned char)s[i];
    if (ch == '\r') { if (i + 1 < n && s[i + 1] == '\n') continue; ch = '\n'; }
    in[m++] = ch == '\n' && multi(f) ? '\n' : ch < 0x20 || ch == 0x7f ? ' ' : (char)ch;
  }
  remember(f, typing, in, m);
  f->text = rk__grow(f->text, &f->cap, f->len - (hi - lo) + m + 1, 1);
  memmove(f->text + lo + m, f->text + hi, (size_t)(f->len - hi));
  memcpy(f->text + lo, in, (size_t)m);
  free(in);
  f->len += m - (hi - lo);
  f->text[f->len] = 0;
  f->caret = f->anchor = lo + m;
  f->stale = true; f->goal = -1;
  tell_access(v, f);
  field_changed(v, f);
}

static void move_to(rk_view *v, field *f, int i, bool extend) {
  f->caret = RK_CLAMP(i, 0, f->len);
  if (!extend) f->anchor = f->caret;
  f->goal = -1;
  touched(v, f);
}

/* Up and Down in a text area: dk lines on, at the x the first of them started from; past the ends, to the ends. */
static void move_lines(rk_view *v, field *f, int dk, bool extend) {
  int k = line_of(f, f->caret) + dk;
  if (k < 0 || k >= f->nlines) { move_to(v, f, k < 0 ? 0 : f->len, extend); return; }
  float goal = f->goal >= 0 ? f->goal : x_at(f, f->caret);
  move_to(v, f, offset_in_line(f, k, goal), extend);
  f->goal = goal;
}

static void copy_selection(const field *f) {
  int lo = sel_lo(f), hi = sel_hi(f);
  if (lo == hi || f->secret) return;                       /* a password never reaches the clipboard */
  char *s = rk__alloc((size_t)(hi - lo) + 1);
  memcpy(s, f->text + lo, (size_t)(hi - lo));
  clip_set(s);
  free(s);
}

static bool field_key(rk_view *v, field *f, const rk_event *e) {
  bool shift = (e->mods & RK_MOD_SHIFT) != 0, word = (e->mods & (RK_MOD_CTRL | RK_MOD_ALT)) != 0, cmd = (e->mods & (RK_MOD_CTRL | RK_MOD_SUPER)) != 0;
  bool sel = f->caret != f->anchor;
  if (f->suggest && !multi(f)) {                           /* a combobox: the list takes Up, Down, Enter and Escape */
    bool open = rk__menu_open(f->suggest);
    if (e->key == RK_KEY_DOWN) { if (open) rk__menu_move(f->suggest, 1); else suggest(v, f, true); return true; }
    if (e->key == RK_KEY_UP && open) { rk__menu_move(f->suggest, -1); return true; }
    if (e->key == RK_KEY_ENTER && open && rk__menu_active(f->suggest) >= 0) { rk__menu_choose(f->suggest, rk__menu_active(f->suggest)); return true; }
    if (e->key == RK_KEY_ESCAPE && open) { rk_menu_close(f->suggest); return true; }
  }
  switch (e->key) {
    case RK_KEY_LEFT:
      if (sel && !shift) move_to(v, f, sel_lo(f), false);
      else move_to(v, f, word ? prev_word(f, f->caret) : prev_char(f, f->caret), shift);
      return true;
    case RK_KEY_RIGHT:
      if (sel && !shift) move_to(v, f, sel_hi(f), false);
      else move_to(v, f, word ? next_word(f, f->caret) : next_char(f, f->caret), shift);
      return true;
    case RK_KEY_HOME: move_to(v, f, multi(f) && !cmd ? f->starts[line_of(f, f->caret)] : 0, shift); return true;
    case RK_KEY_END: move_to(v, f, multi(f) && !cmd ? line_last(f, line_of(f, f->caret)) : f->len, shift); return true;
    case RK_KEY_UP: case RK_KEY_DOWN: case RK_KEY_PAGE_UP: case RK_KEY_PAGE_DOWN: {
      int dk = e->key == RK_KEY_UP || e->key == RK_KEY_DOWN ? 1 : RK_MAX(1, f->rows - 1);
      if (e->key == RK_KEY_UP || e->key == RK_KEY_PAGE_UP) dk = -dk;
      if (multi(f)) move_lines(v, f, dk, shift);
      else move_to(v, f, dk < 0 ? 0 : f->len, shift);
      return true;
    }
    case RK_KEY_BACKSPACE:
      if (!sel) f->anchor = word ? prev_word(f, f->caret) : prev_char(f, f->caret);
      replace(v, f, "", 0);
      return true;
    case RK_KEY_DELETE:
      if (!sel) f->anchor = word ? next_word(f, f->caret) : next_char(f, f->caret);
      replace(v, f, "", 0);
      return true;
    case RK_KEY_ENTER:
      if (multi(f) && !cmd) { replace(v, f, "\n", 1); return true; }
      if (f->submit) { f->submit(v, f->submit_user); return true; }
      return false;
    case 'A': if (!cmd) return false; f->anchor = 0; f->caret = f->len; touched(v, f); return true;
    case 'C': if (!cmd) return false; copy_selection(f); return true;
    case 'X': if (!cmd) return false; copy_selection(f); replace(v, f, "", 0); return true;
    case 'Z':
      if (!cmd) return false;
      if (shift) restore(v, f, f->redo, &f->nredo, &f->undo, &f->nundo, &f->ucap);
      else restore(v, f, f->undo, &f->nundo, &f->redo, &f->nredo, &f->rcap);
      return true;
    case 'Y': if (!cmd) return false; restore(v, f, f->redo, &f->nredo, &f->undo, &f->nundo, &f->ucap); return true;
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
  wrap(v, f);
  rk_rect box = input_rect(v, f);
  switch (e->type) {
    case RK_EVENT_TEXT:
      if (!e->text) return false;
      replace_as(v, f, e->text, (int)strlen(e->text), true);
      suggest(v, f, true);
      return true;
    case RK_EVENT_COMPOSE:
      rk__setstr(&f->preedit, e->text && *e->text ? e->text : NULL);
      rk__view_dirty(v);
      return true;
    case RK_EVENT_KEY_DOWN: return field_key(v, f, e);
    case RK_EVENT_POINTER_DOWN: {
      if (e->button || e->position.y < box.y || e->position.y > box.y + box.h) return e->button == 0;
      int i = offset_at(v, f, e->position);
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
      v->cursor = f->dragging || (e->position.y >= box.y && e->position.y < box.y + box.h) ? RK__CURSOR_TEXT : RK__CURSOR_ARROW;
      if (!f->dragging) return false;
      move_to(v, f, offset_at(v, f, e->position), true);
      return true;
    case RK_EVENT_POINTER_UP: f->dragging = false; return true;
    case RK_EVENT_WHEEL: {                                   /* a text area scrolls its lines; at an end, what holds it does */
      if (!multi(f)) return false;
      float lh = line_h(f), to = RK_CLAMP(f->scroll_y + e->wheel_y, 0.0f, RK_MAX(0.0f, f->nlines * lh - f->rows * lh));
      if (to == f->scroll_y) return false;
      f->scroll_y = to;
      rk__view_dirty(v);
      return true;
    }
    case RK_EVENT_FOCUS: touched(v, f); return false;
    case RK_EVENT_BLUR:
      if (f->suggest) rk_menu_close(f->suggest);
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
  bool off = rk__view_disabled(v);
  rk_color border = rk_color_of(off ? RK_ROLE_DISABLED : f->error ? RK_ROLE_ERROR_FULL : focused ? RK_ROLE_TEXT : RK_ROLE_TEXT_MEDIUM);
  rk_canvas_fill_round_rect(c, box, r, border);
  rk_canvas_fill_round_rect(c, (rk_rect){ box.x + bw, box.y + bw, box.w - 2 * bw, box.h - 2 * bw }, RK_MAX(0.0f, r - bw), rk_color_of(RK_ROLE_BG));
  if (focused) rk_canvas_stroke_rect(c, (rk_rect){ box.x - 4, box.y - 4, box.w + 8, box.h + 8 }, 2, rk_color_of(RK_ROLE_FOCUS));
  wrap(v, f);
  if (f->wrap_w != f->drawn_w) { f->drawn_w = f->wrap_w; keep_caret_in_sight(v, f); }   /* a new width: the caret in sight of it */
  rk_textfmt t = input_fmt(f);
  float lh = line_h(f), x0 = box.x + PAD - f->scroll, y0 = box.y + input_pad_y(f) - f->scroll_y;
  rk_color ink = rk_color_of(off ? RK_ROLE_DISABLED : RK_ROLE_TEXT);
  rk_canvas_push_clip(c, multi(f) ? (rk_rect){ box.x + bw, box.y + bw, box.w - 2 * bw, box.h - 2 * bw } : (rk_rect){ box.x + bw, box.y, box.w - 2 * bw, box.h });
  if (!f->len && f->placeholder) rk__text_draw(c, f->placeholder, -1, box.x + PAD, y0, &t, rk_color_of(RK_ROLE_TEXT_MEDIUM));
  int lo = sel_lo(f), hi = sel_hi(f), ck = line_of(f, f->caret);
  bool composing = focused && f->preedit;
  int first = RK_MAX(0, (int)floorf(f->scroll_y / lh)), last = RK_MIN(f->nlines - 1, (int)floorf((f->scroll_y + box.h) / lh));
  for (int k = first; k <= last; k++) {
    int a = f->starts[k], z = line_stop(f, k);
    float y = y0 + k * lh;
    if (composing && k == ck) {                              /* composing: the text so far, underlined, at the caret */
      float cx = x0 + width(f, a, f->caret), pw = rk__text_width(f->preedit, -1, &t), ty = y + (lh - t.size) / 2;
      draw_run(c, f, a, f->caret, x0, y, &t, ink);
      rk__text_draw(c, f->preedit, -1, cx, y, &t, ink);
      rk_canvas_fill_rect(c, (rk_rect){ cx, ty + t.size + 1, pw, 1 }, ink);
      draw_run(c, f, f->caret, z, cx + pw, y, &t, ink);
      rk_canvas_fill_rect(c, (rk_rect){ cx + pw, ty - 2, 1, t.size + 4 }, ink);
      continue;
    }
    draw_run(c, f, a, z, x0, y, &t, ink);
    int s0 = RK_MAX(lo, a), s1 = RK_MIN(hi, z);
    bool through = hi > z && z < f->len && f->text[z] == '\n';   /* the selection takes the break: a space's width shows it */
    if (focused && !composing && lo != hi && lo <= z && hi >= a && (s0 < s1 || through)) {
      float xa = x0 + width(f, a, s0), xb = x0 + width(f, a, RK_MAX(s0, s1)) + (through ? t.size / 3 : 0);
      rk_rect band = multi(f) ? (rk_rect){ xa, y, xb - xa, lh } : (rk_rect){ xa, box.y + 4, xb - xa, box.h - 8 };
      rk_canvas_push_clip(c, band);
      rk_canvas_fill_rect(c, band, rk_color_of(RK_ROLE_ACCENT));
      draw_run(c, f, a, z, x0, y, &t, rk_color_of(RK_ROLE_ON_ACCENT));
      rk_canvas_pop_clip(c);
    }
  }
  if (focused && !composing && lo == hi && f->lit)
    rk_canvas_fill_rect(c, (rk_rect){ x0 + x_at(f, f->caret), y0 + ck * lh + (lh - t.size) / 2 - 2, 1, t.size + 4 }, ink);
  rk_canvas_pop_clip(c);
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
  float h = label_h(f) + control_h(f) + (f->rows - 1) * line_h(f);
  if (f->hint) { rk_textfmt t = hint_fmt(); h += rk_metric_of(RK_SPACE_XS) + t.size * t.line; }
  return (rk_size){ isfinite(avail.w) ? avail.w : 320, h };
}

static void field_destroy(void *st) {
  field *f = st;
  if (f->blink) rk_app_cancel(f->blink);
  snaps_drop(f->undo, &f->nundo); snaps_drop(f->redo, &f->nredo); free(f->undo); free(f->redo);
  if (f->suggest) { rk_menu_close(f->suggest); rk_release(f->suggest); }
  for (int i = 0; i < f->noptions; i++) free(f->options[i]);
  free(f->options); free(f->shown);
  free(f->starts); free(f->text); free(f->label); free(f->placeholder); free(f->hint); free(f->preedit); free(f);
}

static const rk_class field_class = { { sizeof(rk_view_class), "rk_field", field_draw, field_measure, NULL, field_event, field_destroy }, NULL, NULL };

static field *field_of(rk_view *v) { return v && v->cls == &field_class ? v->state : NULL; }

rk_view *rk_field(const char *label, const char *placeholder) {
  field *f = rk__alloc(sizeof *f);
  f->text = rk__alloc(1); f->cap = 1;
  f->rows = 1; f->goal = -1; f->stale = true;
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
  snaps_drop(f->undo, &f->nundo); snaps_drop(f->redo, &f->nredo);   /* text from the app starts a new history */
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

void rk_field_set_secret(rk_view *v, bool secret) {
  field *f = field_of(v);
  if (!f) return;
  f->secret = secret;
  if (secret) rk_field_set_lines(v, 1);                    /* a password is one line */
  tell_access(v, f);
  rk__view_relayout_up(v);
}

void rk_field_set_lines(rk_view *v, int lines) {
  field *f = field_of(v);
  if (!f) return;
  bool was = multi(f);
  f->rows = RK_MAX(1, lines);
  if (was && !multi(f)) for (int i = 0; i < f->len; i++) if (f->text[i] == '\n') f->text[i] = ' ';   /* one line again */
  f->stale = true; f->scroll = f->scroll_y = 0;
  rk__view_relayout_up(v);
  rk__view_dirty(v);
}

/* A table's cell editor: the input as tall as a row. */
void rk__field_select_all(rk_view *v) { field *f = field_of(v); if (f) { f->anchor = 0; f->caret = f->len; touched(v, f); } }

void rk__field_set_box_height(rk_view *v, float h) {
  field *f = field_of(v);
  if (!f) return;
  f->box_h = h;
  rk__view_relayout_up(v);
}

void rk_field_set_options(rk_view *v, const char *const *options, int count) {
  field *f = field_of(v);
  if (!f) return;
  if (f->suggest) rk_menu_close(f->suggest);
  for (int i = 0; i < f->noptions; i++) free(f->options[i]);
  free(f->options); free(f->shown);
  f->options = NULL; f->shown = NULL; f->noptions = f->nshown = 0;
  if (count <= 0 || !options) { if (f->suggest) { rk_release(f->suggest); f->suggest = NULL; } v->access = RK_ACCESS_TEXT_FIELD; return; }
  f->options = rk__alloc((size_t)count * sizeof *f->options);
  f->shown = rk__alloc((size_t)count * sizeof *f->shown);
  for (int i = 0; i < count; i++) f->options[i] = rk__strdup(options[i] ? options[i] : "");
  f->noptions = count;
  if (!f->suggest) f->suggest = rk_menu();
  v->access = RK_ACCESS_COMBO_BOX;
}

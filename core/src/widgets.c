/* widgets.c: the built-in views, drawn as the web draws them (rekta.css): label, button, tile, rows, icon,
   progress, toast and the title bar. */
#include "rk_internal.h"
#include "rk_tokens.h"

static const char *status_word(rk_status s) {
  switch (s) { case RK_STATUS_INFO: return "active"; case RK_STATUS_SUCCESS: return "done"; case RK_STATUS_WARNING: return "waiting"; case RK_STATUS_ERROR: return "failed"; default: return "idle"; }
}
static rk_role status_tile(rk_status s) {
  switch (s) { case RK_STATUS_INFO: return RK_ROLE_INFO_TILE; case RK_STATUS_SUCCESS: return RK_ROLE_SUCCESS_TILE; case RK_STATUS_WARNING: return RK_ROLE_WARNING_TILE; case RK_STATUS_ERROR: return RK_ROLE_ERROR_TILE; default: return RK_ROLE_SURFACE; }
}
static rk_role status_full(rk_status s) {
  switch (s) { case RK_STATUS_INFO: return RK_ROLE_INFO_FULL; case RK_STATUS_SUCCESS: return RK_ROLE_SUCCESS_FULL; case RK_STATUS_WARNING: return RK_ROLE_WARNING_FULL; case RK_STATUS_ERROR: return RK_ROLE_ERROR_FULL; default: return RK_ROLE_TEXT_MEDIUM; }
}
static rk_color with_alpha(rk_color c, float a) { return (c & 0xffffff00u) | (rk_color)((c & 255) * a + 0.5f); }

/* Wrapped text in a box, as a paragraph: the lines from the top. */
static float draw_lines(rk_canvas *c, const char *s, const rk_textfmt *f, float x, float y, float width, rk_color color) {
  rk_line lines[128];
  int n = rk__text_wrap(s, f, width, lines, 128);
  for (int i = 0; i < n; i++) rk__text_draw(c, s + lines[i].start, lines[i].len, x, y + i * f->size * f->line, f, color);
  return n * f->size * f->line;
}

static float text_height(const char *s, const rk_textfmt *f, float width) {
  rk_line lines[128];
  return rk__text_wrap(s, f, width, lines, 128) * f->size * f->line;
}

/* Label ----------------------------------------------------------------------------------------------------------- */

typedef struct label { char *text; rk_text_style style; rk_role color; } label;

static rk_size label_measure(rk_view *v, rk_size avail, void *st) {
  label *l = st;
  rk_textfmt f = rk__textfmt(l->style, rk__host_width(v));
  rk_line lines[128];
  int n = rk__text_wrap(l->text, &f, isfinite(avail.w) ? avail.w : 0, lines, 128);
  float w = 0;
  for (int i = 0; i < n; i++) w = RK_MAX(w, lines[i].width);
  return (rk_size){ ceilf(w), RK_MAX(n, 1) * f.size * f.line };
}
static void label_draw(rk_view *v, rk_canvas *c, void *st) {
  label *l = st;
  rk_textfmt f = rk__textfmt(l->style, rk__host_width(v));
  draw_lines(c, l->text, &f, 0, 0, v->pframe.w + 0.5f, rk_color_of(l->color ? l->color : f.color));
}
static void label_destroy(void *st) { label *l = st; free(l->text); free(l); }
static const rk_class label_class = { { sizeof(rk_view_class), "rk_label", label_draw, label_measure, NULL, NULL, label_destroy }, NULL, NULL };

rk_view *rk_label(const char *text, rk_text_style style) {
  label *l = rk__alloc(sizeof *l);
  l->text = rk__strdup(text ? text : ""); l->style = style;
  rk_view *v = rk__view_create(&label_class, l);
  v->access = style == RK_TEXT_TITLE || style == RK_TEXT_GROUP ? RK_ACCESS_HEADING : RK_ACCESS_TEXT;
  rk__setstr(&v->access_label, l->text);
  return v;
}
void rk_label_set_text(rk_view *v, const char *text) {
  if (v->cls != &label_class) return;
  label *l = v->state;
  if (!strcmp(l->text, text ? text : "")) return;
  rk__setstr(&l->text, text ? text : "");
  rk__setstr(&v->access_label, l->text);
  rk__view_relayout_up(v);
}
void rk_label_set_color(rk_view *v, rk_role role) { if (v->cls == &label_class) { ((label *)v->state)->color = role; rk__view_dirty(v); } }

/* Button ---------------------------------------------------------------------------------------------------------- */

enum { BUTTON_TOAST = 100 };
typedef struct button { char *label, *icon, *reason; int kind; } button;

static rk_textfmt button_fmt(button *b) {
  rk_textfmt f = rk__textfmt(b->kind == RK_BUTTON_BAR ? RK_TEXT_LABEL : RK_TEXT_STRONG, 0);
  f.line = 1;
  return f;
}
static rk_size button_measure(rk_view *v, rk_size avail, void *st) {
  (void)v; (void)avail;
  button *b = st;
  rk_textfmt f = button_fmt(b);
  float tw = b->label && *b->label ? rk__text_width(b->label, -1, &f) : 0, iw = b->icon ? 20 : 0;
  if (b->kind == RK_BUTTON_BAR) return (rk_size){ RK_MAX(48.0f, 20 + iw + (tw && iw ? 6 : 0) + tw), 48 };
  if (b->kind == BUTTON_TOAST) return (rk_size){ RK_MAX(rk_metric_of(RK_CONTROL_HEIGHT), tw + 2 * rk_metric_of(RK_SPACE_M)), rk_metric_of(RK_CONTROL_HEIGHT) + 4 };
  float pad = rk_metric_of(RK_CONTROL_PADDING);
  return (rk_size){ ceilf(tw + iw + (tw && iw ? 8 : 0) + 2 * pad + 4), rk_metric_of(RK_CONTROL_HEIGHT) };   /* the 2 px frame outside the padding */
}
static void button_draw(rk_view *v, rk_canvas *c, void *st) {
  button *b = st;
  rk_rect r = { 0, 0, v->pframe.w, v->pframe.h };
  bool off = b->reason != NULL;
  rk_color ink = rk_color_of(RK_ROLE_TEXT), bg = 0;
  if (b->kind == RK_BUTTON_PRIMARY && !off) { bg = rk_color_of(RK_ROLE_ACCENT); ink = rk_color_of(RK_ROLE_ON_ACCENT); }
  if (b->kind == RK_BUTTON_QUIET) ink = rk_color_of(RK_ROLE_TEXT_HIGH);
  if (b->kind == BUTTON_TOAST) ink = rk_color_of(RK_ROLE_BG);
  if (off) ink = rk_color_of(RK_ROLE_DISABLED);
  if (bg) rk_canvas_fill_round_rect(c, r, rk_metric_of(RK_RADIUS_S), bg);
  if (v->pressed && !off) rk_canvas_fill_round_rect(c, r, b->kind == BUTTON_TOAST ? 0 : rk_metric_of(RK_RADIUS_S), rk_color_of(RK_ROLE_HOVER));
  if (b->kind == RK_BUTTON_DEFAULT || b->kind == RK_BUTTON_PRIMARY) {
    rk_color border = b->kind == RK_BUTTON_PRIMARY && !off ? bg : ink;
    rk__canvas_ring(c, r, rk_metric_of(RK_RADIUS_S), 2, border);
  }
  if (b->kind == BUTTON_TOAST) rk_canvas_fill_rect(c, (rk_rect){ 0, 12, 1, r.h - 24 }, with_alpha(ink, 0.2f));
  rk_textfmt f = button_fmt(b);
  float tw = b->label && *b->label ? rk__text_width(b->label, -1, &f) : 0, iw = b->icon ? 20 : 0;
  float gap = tw && iw ? (b->kind == RK_BUTTON_BAR ? 6 : 8) : 0, x = (r.w - tw - iw - gap) / 2;
  if (b->icon) rk_canvas_icon(c, b->icon, (rk_rect){ x, (r.h - 20) / 2, 20, 20 }, ink);
  if (tw) rk__text_draw(c, b->label, -1, x + iw + gap, (r.h - f.size) / 2, &f, ink);
}
static void button_destroy(void *st) { button *b = st; free(b->label); free(b->icon); free(b->reason); free(b); }
static const rk_class button_class = { { sizeof(rk_view_class), "rk_button", button_draw, button_measure, NULL, NULL, button_destroy }, NULL, NULL };

static rk_view *button_make(const char *text, const char *icon, int kind) {
  button *b = rk__alloc(sizeof *b);
  b->label = rk__strdup(text ? text : ""); b->icon = rk__strdup(icon); b->kind = kind;
  rk_view *v = rk__view_create(&button_class, b);
  v->press = RK_PRESS_FLAT; v->focusable = true; v->hug = true;
  v->access = RK_ACCESS_BUTTON;
  rk__setstr(&v->access_label, text);
  return v;
}
rk_view *rk_button(const char *text, void (*on_press)(rk_view *button, void *user), void *user) {
  rk_view *v = button_make(text, NULL, RK_BUTTON_DEFAULT);
  if (on_press) rk_view_on_activate(v, on_press, user);
  return v;
}

void rk_button_set_kind(rk_view *v, rk_button_kind kind) {
  if (v->cls != &button_class) return;
  ((button *)v->state)->kind = kind;
  rk__view_relayout_up(v);
}

void rk_button_set_icon(rk_view *v, const char *icon) {
  if (v->cls != &button_class) return;
  button *b = v->state;
  rk__setstr(&b->icon, icon);
  if (!b->label || !*b->label) rk__setstr(&v->access_label, icon);   /* an icon alone is still named */
  rk__view_relayout_up(v);
}
void rk_button_set_disabled(rk_view *v, const char *reason) {
  if (v->cls != &button_class) return;
  button *b = v->state;
  rk__setstr(&b->reason, reason);
  v->press = reason ? RK_PRESS_NONE : RK_PRESS_FLAT;
  rk__view_dirty(v);
}

/* Tile ------------------------------------------------------------------------------------------------------------ */

typedef struct tile {
  rk_status status; char *word, *value, *label, *message, *old_value, *old_label;
  float *series; int nseries; float progress;
} tile;

static float tile_stat_height(rk_view *v, tile *t, rk_textfmt *num, rk_textfmt *lab) {
  *num = rk__textfmt(RK_TEXT_STAT, rk__host_width(v));
  *lab = rk__textfmt(RK_TEXT_SMALL, 0);
  return (t->value ? num->size * num->line : 0) + (t->label ? lab->size * lab->line : 0);
}

static void draw_stat(rk_canvas *c, const char *value, const char *lab_text, float x, float y, rk_textfmt *num, rk_textfmt *lab, rk_color ink) {
  if (value) { rk__text_draw(c, value, -1, x, y, num, ink); y += num->size * num->line; }
  if (lab_text) rk__text_draw(c, lab_text, -1, x, y, lab, ink);
}

static void tile_draw(rk_view *v, rk_canvas *c, void *st) {
  tile *t = st;
  float W = v->pframe.w, H = v->pframe.h, pad = 12, gap = rk_metric_of(RK_SPACE_S), inner = W - 2 * pad;
  rk_color ink = rk_color_of(RK_ROLE_TEXT);
  rk_textfmt word = rk__textfmt(RK_TEXT_LABEL, 0);
  word.color = RK_ROLE_TEXT;
  const char *wd = t->word ? t->word : status_word(t->status);
  rk__text_draw(c, wd, -1, W - 12 - rk__text_width(wd, -1, &word), 10 - (word.size * word.line - word.size) / 2, &word, ink);
  /* from the bottom up: progress, sparkline, message, stat (a column justified to the end) */
  float y = H - pad;
  if (t->progress >= 0) {
    y -= 4;
    rk_canvas_fill_rect(c, (rk_rect){ pad, y, inner, 4 }, with_alpha(ink, 0.2f));
    rk_canvas_fill_rect(c, (rk_rect){ pad, y, inner * RK_CLAMP(v->pvalue, 0.0f, 1.0f), 4 }, ink);
    y -= gap;
  }
  if (t->nseries > 1 && t->status != RK_STATUS_SUCCESS) {          /* stillness means done: no line when finished */
    float sh = inner * 28 / 200;
    y -= sh;
    float lo = t->series[0], hi = t->series[0];
    for (int i = 1; i < t->nseries; i++) { lo = RK_MIN(lo, t->series[i]); hi = RK_MAX(hi, t->series[i]); }
    float span = hi - lo > 0 ? hi - lo : 1;
    rk_point pts[256];
    int n = RK_MIN(t->nseries, 256);
    for (int i = 0; i < n; i++) pts[i] = (rk_point){ pad + inner * i / (n - 1), y + sh * (26 - (t->series[i] - lo) / span * 24) / 28 };
    rk_canvas_polyline(c, pts, n, 2, ink);
    y -= gap;
  }
  if (t->message) {
    rk_textfmt mf = rk__textfmt(RK_TEXT_BODY, 0);
    mf.line = RK_TOK_LINE_COMPACT;
    float mh = text_height(t->message, &mf, inner);
    y -= mh;
    draw_lines(c, t->message, &mf, pad, y, inner, ink);
    y -= gap;
  }
  rk_textfmt num, lab;
  float sh = tile_stat_height(v, t, &num, &lab);
  y -= sh;
  if (v->transition == RK_TRANSITION_PEEK + 1) {
    /* a new number peeks in: the old stat leaves upwards as the new one rises, inside the tile */
    float tt = v->transition_t, e0 = rk__ease(rk__ease_out, RK_MIN(1.0f, tt / 0.6f)), e1 = rk__ease(rk__ease_in, RK_CLAMP((tt - 0.2f) / 0.8f, 0.0f, 1.0f));
    float a = c->alpha;
    rk_canvas_push_clip(c, (rk_rect){ 0, 0, W, H });
    c->alpha = a * (1 - e0);
    draw_stat(c, t->old_value, t->old_label, pad, y - 0.4f * sh * e0, &num, &lab, ink);
    c->alpha = a * e1;
    draw_stat(c, t->value, t->label, pad, y + 0.4f * sh * (1 - e1), &num, &lab, ink);
    c->alpha = a;
    rk_canvas_pop_clip(c);
  } else draw_stat(c, t->value, t->label, pad, y, &num, &lab, ink);
}

static void *tile_peek(rk_view *v, void *st, rk_rect *area) { (void)v; (void)area; return st; }
static void tile_destroy(void *st) {
  tile *t = st;
  free(t->word); free(t->value); free(t->label); free(t->message); free(t->old_value); free(t->old_label); free(t->series); free(t);
}
static const rk_class tile_class = { { sizeof(rk_view_class), "rk_tile", tile_draw, NULL, NULL, NULL, tile_destroy }, tile_peek, NULL };

bool rk__is_tile(rk_view *v) { return v && v->cls == &tile_class; }

static void tile_access(rk_view *v) {
  tile *t = v->state;
  char buf[512];
  snprintf(buf, sizeof buf, "%s%s%s%s%s", t->value ? t->value : "", t->value && t->label ? " " : "", t->label ? t->label : "", ", ", t->word ? t->word : status_word(t->status));
  rk__setstr(&v->access_label, buf);
}

rk_view *rk_tile(const char *value, const char *label) {
  tile *t = rk__alloc(sizeof *t);
  t->progress = -1;
  rk_view *v = rk__view_create(&tile_class, t);
  v->radius = rk_metric_of(RK_RADIUS_TILE);
  v->access = RK_ACCESS_GROUP;
  rk_tile_set_status(v, RK_STATUS_NONE, NULL);
  rk_tile_set_stat(v, value, label);
  return v;
}

/* Containers: a plain view with a layout and the spacing the design gives it. */
static rk_view *container(rk_layout layout, float gap) {
  rk_view *v = rk_view_create();
  rk_view_set_layout(v, layout);
  rk_view_set_gap(v, gap);
  return v;
}

rk_view *rk_column(void) { return container(RK_LAYOUT_COLUMN, rk_metric_of(RK_SPACE_M)); }
rk_view *rk_tile_grid(void) { return container(RK_LAYOUT_TILES, rk_metric_of(RK_SPACE_S)); }

rk_view *rk_row(void) {
  rk_view *v = container(RK_LAYOUT_ROW, rk_metric_of(RK_SPACE_S));
  rk_view_set_align(v, RK_ALIGN_START, RK_ALIGN_START);
  return v;
}

void rk_tile_set_status(rk_view *v, rk_status status, const char *word) {
  if (v->cls != &tile_class) return;
  tile *t = v->state;
  t->status = status;
  rk__setstr(&t->word, word);
  rk_view_set_fill(v, status_tile(status));
  tile_access(v);
  rk__view_dirty(v);
}

void rk_tile_set_stat(rk_view *v, const char *value, const char *lab) {
  if (v->cls != &tile_class) return;
  tile *t = v->state;
  free(t->old_value); free(t->old_label);
  t->old_value = t->value; t->old_label = t->label;
  t->value = rk__strdup(value); t->label = rk__strdup(lab);
  tile_access(v);
  rk__view_dirty(v);
}

void rk_tile_set_message(rk_view *v, const char *message) {
  if (v->cls != &tile_class) return;
  rk__setstr(&((tile *)v->state)->message, message);
  rk__view_dirty(v);
}

void rk_tile_set_series(rk_view *v, const float *values, int count) {
  if (v->cls != &tile_class) return;
  tile *t = v->state;
  free(t->series); t->series = NULL; t->nseries = 0;
  if (count > 0 && values) { t->series = rk__alloc((size_t)count * sizeof(float)); memcpy(t->series, values, (size_t)count * sizeof(float)); t->nseries = count; }
  rk__view_dirty(v);
}

static void set_value(rk_view *v, float value) {
  v->value = value;
  if (rk__view_visible(v) && !rk__reduced()) {
    float from = v->pvalue;
    rk__animate(v, RK_PROP_VALUE, &from, &value, 1, rk_duration_of(RK_DURATION_MODERATE), 0, rk__ease_base);
  } else { rk__anim_cancel(v, RK_PROP_VALUE); v->pvalue = value; }
  rk__view_dirty(v);
}

void rk_tile_set_progress(rk_view *v, float progress) {
  if (v->cls != &tile_class) return;
  tile *t = v->state;
  bool had = t->progress >= 0;
  t->progress = progress;
  if (progress >= 0) { if (!had) v->pvalue = progress; set_value(v, progress); }
  rk__view_dirty(v);
}

/* Rows ------------------------------------------------------------------------------------------------------------ */

typedef struct row { char *title, *meta; rk_status status; bool lifted; rk_view *grip; } row;   /* grip: sortable (sort.c) */
static bool rows_last(rk_view *v);
static void rows_added(rk_view *rows, rk_view *row);

/* A row's rule above it is its own, outside the padding, and the last row has one below as well, as on the web. */
static float row_rules(rk_view *v, float *above) { *above = 1; return rows_last(v) ? 2 : 1; }
static rk_size row_measure(rk_view *v, rk_size avail, void *st) {
  (void)st;
  rk_textfmt f = rk__textfmt(RK_TEXT_BODY, 0);
  float above, rules = row_rules(v, &above);
  return (rk_size){ isfinite(avail.w) ? avail.w : 320, RK_MAX(rk_metric_of(RK_ROW_HEIGHT), f.size * f.line + 2 * rk_metric_of(RK_SPACE_S)) + rules };
}
static void row_draw(rk_view *v, rk_canvas *c, void *st) {
  row *r = st;
  float W = v->pframe.w, H = v->pframe.h;
  rk_color rule = rk_color_of(RK_ROLE_TEXT_LOW);
  if (r->lifted) {                                         /* picked up: a floating surface, bg and a 1 px edge, no shadow */
    rk_canvas_fill_rect(c, (rk_rect){ 0, 0, W, H }, rk_color_of(RK_ROLE_BG));
    rk_canvas_stroke_rect(c, (rk_rect){ 0, 0, W, H }, 1, rk_color_of(RK_ROLE_TEXT_MEDIUM));
  } else {
    rk_canvas_fill_rect(c, (rk_rect){ 0, 0, W, 1 }, rule);
    if (rows_last(v)) rk_canvas_fill_rect(c, (rk_rect){ 0, H - 1, W, 1 }, rule);
  }
  if (v->pressed) rk_canvas_fill_rect(c, (rk_rect){ 0, 1, W, H - 1 }, rk_color_of(RK_ROLE_HOVER));
  rk_textfmt f = rk__textfmt(RK_TEXT_BODY, 0);
  float x = r->grip ? 24 + rk_metric_of(RK_SPACE_S) : 0;   /* after the grip */
  float above, inner = H - row_rules(v, &above), top = above + (inner - f.size * f.line) / 2;   /* between the rules */
  float base = top + rk__text_ascent(&f);                  /* the meta and the word stand on the title's baseline */
  rk__text_draw(c, r->title, -1, x, top, &f, rk_color_of(RK_ROLE_TEXT));
  float right = W;
  if (r->status != RK_STATUS_NONE) {
    rk_textfmt tf = rk__textfmt(RK_TEXT_LABEL, 0);
    const char *w = r->meta ? r->meta : status_word(r->status);
    float tw = rk__text_width(w, -1, &tf);
    rk__text_draw(c, w, -1, right - tw, base - rk__text_ascent(&tf), &tf, rk_color_of(RK_ROLE_TEXT));
    rk_canvas_fill_rect(c, (rk_rect){ right - tw - 14, above + inner / 2 - 4, 8, 8 }, rk_color_of(status_full(r->status)));
  } else if (r->meta) {
    rk_textfmt mf = rk__textfmt(RK_TEXT_META, 0);
    float mw = rk__text_width(r->meta, -1, &mf);
    rk__text_draw(c, r->meta, -1, right - mw, base - rk__text_ascent(&mf), &mf, rk_color_of(RK_ROLE_TEXT_HIGH));
  }
}
static void row_layout(rk_view *v, void *st) { row *r = st; if (r->grip) rk__set_frame(r->grip, (rk_rect){ 0, 0, 24, v->frame.h }); }
static void row_destroy(void *st) { row *r = st; free(r->title); free(r->meta); free(r); }
static const rk_class row_class = { { sizeof(rk_view_class), "rk_rows_row", row_draw, row_measure, row_layout, NULL, row_destroy }, NULL, NULL };

rk_view *rk_rows(void) {
  rk_view *v = rk_view_create();
  rk_view_set_layout(v, RK_LAYOUT_COLUMN);
  v->access = RK_ACCESS_LIST;
  return v;
}

rk_view *rk_rows_add(rk_view *rows, const char *title, const char *meta, rk_status status) {
  row *r = rk__alloc(sizeof *r);
  r->title = rk__strdup(title ? title : ""); r->meta = rk__strdup(meta); r->status = status;
  rk_view *v = rk__view_create(&row_class, r);
  v->access = RK_ACCESS_LIST_ITEM;
  rk__setstr(&v->access_label, r->title);
  rk__view_add(rows, v);
  rk_release(v);
  rows_added(rows, v);
  return v;
}

/* Icon and progress ----------------------------------------------------------------------------------------------- */

typedef struct icon { char *name; rk_role color; } icon;
static rk_size icon_measure(rk_view *v, rk_size a, void *st) { (void)v; (void)a; (void)st; return (rk_size){ 20, 20 }; }
static void icon_draw(rk_view *v, rk_canvas *c, void *st) {
  icon *i = st;
  rk_canvas_icon(c, i->name, (rk_rect){ 0, 0, v->pframe.w, v->pframe.h }, rk_color_of(i->color ? i->color : RK_ROLE_TEXT));
}
static void icon_destroy(void *st) { icon *i = st; free(i->name); free(i); }
static const rk_class icon_class = { { sizeof(rk_view_class), "rk_icon", icon_draw, icon_measure, NULL, NULL, icon_destroy }, NULL, NULL };

rk_view *rk_icon(const char *name, rk_role color) {
  icon *i = rk__alloc(sizeof *i);
  i->name = rk__strdup(name); i->color = color;
  rk_view *v = rk__view_create(&icon_class, i);
  v->access = RK_ACCESS_IMAGE;
  return v;
}

static rk_size progress_measure(rk_view *v, rk_size a, void *st) { (void)v; (void)st; return (rk_size){ isfinite(a.w) ? a.w : 150, 4 }; }
static void progress_draw(rk_view *v, rk_canvas *c, void *st) {
  (void)st;
  float W = v->pframe.w;
  rk_color ink = rk_color_of(RK_ROLE_TEXT);
  rk_canvas_fill_rect(c, (rk_rect){ 0, 0, W, 4 }, with_alpha(ink, 0.2f));
  if (v->value < 0) {
    /* working on it: a 30% piece passes every duration-busy; under reduced motion it stands at full width */
    double busy = rk_duration_of(RK_DURATION_LONG) > 0 ? 1200.0 : 0;
    if (busy <= 0) { rk_canvas_fill_rect(c, (rk_rect){ 0, 0, W, 4 }, ink); return; }
    float t = (float)fmod(rk_app_now(), busy) / (float)busy, x = (-0.3f + 1.3f * t) * W;
    rk_canvas_fill_rect(c, (rk_rect){ RK_MAX(0.0f, x), 0, RK_MIN(W, x + 0.3f * W) - RK_MAX(0.0f, x), 4 }, ink);
    rk__view_dirty(v);
    rk__app_request_frame();
    return;
  }
  rk_canvas_fill_rect(c, (rk_rect){ 0, 0, W * RK_CLAMP(v->pvalue, 0.0f, 1.0f), 4 }, ink);
}
static const rk_class progress_class = { { sizeof(rk_view_class), "rk_progress", progress_draw, progress_measure, NULL, NULL, NULL }, NULL, NULL };

rk_view *rk_progress(float progress) {
  rk_view *v = rk__view_create(&progress_class, NULL);
  v->value = v->pvalue = progress;
  v->access = RK_ACCESS_PROGRESS;
  return v;
}
void rk_progress_set(rk_view *v, float progress) { if (v->cls == &progress_class) set_value(v, progress); }

/* Focus ----------------------------------------------------------------------------------------------------------- */

/* Keyboard focus: a 2px ring in the focus colour 2px outside; on a tile, inset in the tile's own text colour. */
void rk__draw_focus(rk_canvas *c, rk_view *v, rk_rect r) {
  if (rk__is_tile(v)) { rk_canvas_stroke_rect(c, (rk_rect){ r.x + 5, r.y + 5, r.w - 10, r.h - 10 }, 2, rk_color_of(RK_ROLE_TEXT)); return; }
  rk_canvas_stroke_rect(c, (rk_rect){ r.x - 4, r.y - 4, r.w + 8, r.h + 8 }, 2, rk_color_of(RK_ROLE_FOCUS));
}

/* Toast ----------------------------------------------------------------------------------------------------------- */

typedef struct toast { void (*fn)(void *); void *user; uint32_t timer; rk_view *view; } toast;

#define TOAST_HOLD_MS 1000

static void go(toast *t) {
  if (t->timer) { rk_app_cancel(t->timer); t->timer = 0; }
  if (!t->view->parent || t->view->leaving) return;
  rk__toast_leaving(t->view);
  rk_animate_begin(RK_MOTION_LEAVE); rk_view_remove(t->view); rk_animate_commit();
}

/* Its time is up - unless focus or the pointer is on it: then it holds, and looks again a second later. */
static void toast_leave(void *st) {
  toast *t = st;
  t->timer = 0;
  rk_window *w = rk__view_window(t->view);
  if (w && (rk__view_within(w->focus, t->view) || rk__view_within(w->hover, t->view))) { t->timer = rk_app_after(TOAST_HOLD_MS, false, toast_leave, t); return; }
  go(t);
}

static bool toast_event(rk_view *v, const rk_event *e, void *st) {
  (void)v;
  if (e->type != RK_EVENT_KEY_DOWN || e->key != RK_KEY_ESCAPE) return false;
  go(st);                                                  /* Escape inside a toast dismisses it */
  return true;
}
static void toast_act(rk_view *b, void *st) {
  (void)b;
  toast *t = st;
  void (*fn)(void *) = t->fn; void *user = t->user;
  t->fn = NULL;
  go(t);
  if (fn) fn(user);
}
static void toast_destroy(void *st) { toast *t = st; if (t->timer) rk_app_cancel(t->timer); free(t); }
static const rk_class toast_class = { { sizeof(rk_view_class), "rk_toast", NULL, NULL, NULL, toast_event, toast_destroy }, NULL, NULL };

rk_view *rk__toast_create(const char *message, const char *action, void (*on_action)(void *), void *user) {
  toast *t = rk__alloc(sizeof *t);
  t->fn = on_action; t->user = user;
  rk_view *v = rk__view_create(&toast_class, t);
  t->view = v;
  v->layout = RK_LAYOUT_ROW; v->cross = RK_ALIGN_STRETCH;
  v->fill_role = RK_ROLE_TEXT; v->pfill = rk_color_of(RK_ROLE_TEXT);
  v->radius = rk_metric_of(RK_RADIUS_S);
  v->clip = true;
  v->min_h = rk_metric_of(RK_CONTROL_HEIGHT) + 4;
  v->access = RK_ACCESS_STATUS;
  rk__setstr(&v->access_label, message);
  rk_view *msg = rk_label(message, RK_TEXT_BODY);
  rk_label_set_color(msg, RK_ROLE_BG);
  float m = rk_metric_of(RK_SPACE_M), s = rk_metric_of(RK_SPACE_S) + rk_metric_of(RK_SPACE_XS);
  rk_view *wrap = rk_view_create();
  rk_view_set_layout(wrap, RK_LAYOUT_COLUMN);
  rk_view_set_align(wrap, RK_ALIGN_CENTER, RK_ALIGN_STRETCH);
  rk_view_set_padding(wrap, (rk_insets){ s, m, s, m });
  rk_view_set_grow(wrap, 1);
  rk__view_add(wrap, msg); rk_release(msg);
  rk__view_add(v, wrap); rk_release(wrap);
  if (action) {
    rk_view *b = button_make(action, NULL, BUTTON_TOAST);
    rk_view_on_activate(b, toast_act, t);
    rk__view_add(v, b); rk_release(b);
  }
  t->timer = rk_app_after(rk_duration_of(RK_TIMING_TOAST), false, toast_leave, t);
  return v;
}

/* Title bar ------------------------------------------------------------------------------------------------------- */

typedef struct titlebar { rk_window *w; float inset; } titlebar;   /* inset: room for the system's buttons (macOS) */

static void titlebar_draw(rk_view *v, rk_canvas *c, void *st) {
  titlebar *tb = st;
  float H = v->pframe.h;
  rk_canvas_fill_rect(c, (rk_rect){ 0, H - 1, v->pframe.w, 1 }, rk_color_of(RK_ROLE_TEXT_LOW));
  rk_textfmt name = rk__textfmt(RK_TEXT_LABEL, 0), place = rk__textfmt(RK_TEXT_SMALL, 0);
  name.color = RK_ROLE_TEXT; place.weight = RK_TOK_WEIGHT_LIGHT; place.line = name.line;
  float x = tb->inset + rk_metric_of(RK_SPACE_S) + 4, ty = (H - name.size * name.line) / 2;
  const char *title = tb->w->title && *tb->w->title ? tb->w->title : "Rekta";
  rk__text_draw(c, title, -1, x, ty, &name, rk_color_of(RK_ROLE_TEXT));
  x += rk__text_width(title, -1, &name) + rk_metric_of(RK_SPACE_S);
  rk_controller *root = tb->w->controller;
  const char *pl = root ? rk_controller_title(root) : NULL;
  if (pl && *pl) {
    rk__text_draw(c, "/", -1, x, ty, &place, rk_color_of(RK_ROLE_TEXT_MEDIUM));
    x += rk__text_width("/", -1, &place) + rk_metric_of(RK_SPACE_S);
    rk__text_draw(c, pl, -1, x, ty, &place, rk_color_of(RK_ROLE_TEXT_HIGH));
  }
}

static bool titlebar_event(rk_view *v, const rk_event *e, void *st) {
  titlebar *tb = st;
  /* a press on the bar itself moves the window (twice: maximizes); its buttons press as buttons */
  if (e->type == RK_EVENT_POINTER_DOWN && e->button == 0 && (!tb->w->pressed || tb->w->pressed == v)) {
    static double last;
    rk__window_pointer_cancel(tb->w);                          /* the system takes the pointer: no release comes */
    if (e->time - last < 400) rk__platform()->zoom(tb->w->native); else rk__platform()->move(tb->w->native);
    last = e->time;
    return true;
  }
  return false;
}

static void titlebar_layout(rk_view *v, void *st) {
  (void)st;
  float x = v->frame.w;
  for (int i = v->nkids - 1; i >= 0; i--) { x -= 46; rk__set_frame(v->kids[i], (rk_rect){ x, 0, 46, v->frame.h - 1 }); }
}
static void titlebar_destroy(void *st) { free(st); }
static const rk_class titlebar_class = { { sizeof(rk_view_class), "rk_titlebar", titlebar_draw, NULL, titlebar_layout, titlebar_event, titlebar_destroy }, NULL, NULL };

static void tb_min(rk_view *b, void *u) { (void)b; rk__platform()->minimize(((rk_window *)u)->native); }
static void tb_zoom(rk_view *b, void *u) { (void)b; rk__platform()->zoom(((rk_window *)u)->native); }
static void tb_close(rk_view *b, void *u) { (void)b; rk__window_close_request(u); }

rk_view *rk__titlebar_create(rk_window *w) {
  titlebar *tb = rk__alloc(sizeof *tb);
  tb->w = w;
  rk_view *v = rk__view_create(&titlebar_class, tb);
  v->fill_role = RK_ROLE_BG; v->pfill = rk_color_of(RK_ROLE_BG);
  /* macOS keeps its traffic lights on the left; elsewhere Rekta draws minimize, maximize and close on the right */
  if (!strcmp(rk__platform()->name, "cocoa")) { tb->inset = 70; return v; }
  static const char *icons[3] = { "minus", "window", "close" }, *names[3] = { "minimize", "maximize", "close" };
  void (*fns[3])(rk_view *, void *) = { tb_min, tb_zoom, tb_close };
  for (int i = 0; i < 3; i++) {
    rk_view *b = button_make(NULL, icons[i], RK_BUTTON_BAR);
    rk__setstr(&b->access_label, names[i]);
    b->focusable = false;
    rk_view_on_activate(b, fns[i], w);
    b->focusable = false;
    rk__view_add(v, b); rk_release(b);
  }
  return v;
}

void rk__titlebar_update(rk_view *bar) { rk__view_dirty(bar); }

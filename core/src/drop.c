/* drop.c: a place to drop files, as the web's .rk-drop. A 2 px dashed text-medium edge with radius-s around a title
   in the text colour and a hint in text-high, 120 px tall at least; files dragged over it make the edge solid and
   the text colour, over the hover fill, both glide over duration-short. Files dropped anywhere else are refused (no
   other view takes them). What it accepts is a list such as ".png,.jpg" or "image/" with a star: an extension, or a family of
   them by its media type; empty takes every file. The app hears the files it took and those it refused, only when it
   took one; a click or Enter opens the system's file chooser with the same filter (rk_window_choose_files). */
#include "rk_internal.h"

typedef struct drop_zone {
  char *title, *hint, *accept;
  bool over;
  rk_tween k;                                              /* 0 at rest, 1 with files over it */
  void (*files)(rk_view *drop, const char *const *taken, int ntaken, const char *const *refused, int nrefused, void *user);
  void *user;
} drop_zone;

static int lower_ascii(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

static bool ends_with(const char *s, const char *tail, size_t n) {
  size_t ls = strlen(s);
  if (n > ls) return false;
  for (size_t i = 0; i < n; i++) if (lower_ascii((unsigned char)s[ls - n + i]) != lower_ascii((unsigned char)tail[i])) return false;
  return true;
}

/* the extensions a media type stands for: a path has no type, only its name */
static const char *const family_image[] = { ".png", ".jpg", ".jpeg", ".gif", ".webp", ".bmp", ".svg", ".tif", ".tiff", ".heic", ".avif", NULL };
static const char *const family_text[] = { ".txt", ".md", ".csv", ".tsv", ".json", ".xml", ".html", ".htm", ".css", ".js", ".log", ".c", ".h", NULL };
static const char *const family_audio[] = { ".mp3", ".wav", ".ogg", ".flac", ".m4a", ".aac", ".opus", NULL };
static const char *const family_video[] = { ".mp4", ".mov", ".webm", ".mkv", ".avi", ".m4v", NULL };
static const struct { const char *type, *ext; } exact[] = {
  { "application/pdf", ".pdf" }, { "application/json", ".json" }, { "application/zip", ".zip" }, { "text/csv", ".csv" },
  { "text/plain", ".txt" }, { "image/png", ".png" }, { "image/jpeg", ".jpg" }, { "image/svg+xml", ".svg" },
};

static bool matches_one(const char *path, const char *pat, size_t n) {
  if (!n) return false;
  if (pat[0] == '.') return ends_with(path, pat, n);
  if (n == 1 && pat[0] == '*') return true;
  if (n > 2 && pat[n - 2] == '/' && pat[n - 1] == '*') {
    const char *const *fam = !strncmp(pat, "image/", 6) ? family_image : !strncmp(pat, "text/", 5) ? family_text
                           : !strncmp(pat, "audio/", 6) ? family_audio : !strncmp(pat, "video/", 6) ? family_video : NULL;
    for (int i = 0; fam && fam[i]; i++) if (ends_with(path, fam[i], strlen(fam[i]))) return true;
    return false;
  }
  for (size_t i = 0; i < sizeof exact / sizeof *exact; i++)
    if (strlen(exact[i].type) == n && !strncmp(exact[i].type, pat, n)) {
      if (ends_with(path, exact[i].ext, strlen(exact[i].ext))) return true;
      if (!strcmp(exact[i].ext, ".jpg") && ends_with(path, ".jpeg", 5)) return true;
    }
  return false;
}

bool rk__accepts(const char *accept, const char *path) {
  if (!accept || !*accept) return true;
  for (const char *p = accept; *p;) {
    while (*p == ' ' || *p == ',') p++;
    const char *e = p;
    while (*e && *e != ',') e++;
    size_t n = (size_t)(e - p);
    while (n && p[n - 1] == ' ') n--;
    if (matches_one(path, p, n)) return true;
    p = e;
  }
  return false;
}

/* The filter as glob patterns ("*.png") for the system's choosers: every extension it names or stands for. NULL and
   0 when it takes any file. The strings and the array are malloc'd (rk__free_paths). */
int rk__accept_globs(const char *accept, char ***out) {
  char **globs = NULL;
  int n = 0, cap = 0;
  *out = NULL;
  if (!accept || !*accept) return 0;
  for (const char *p = accept; *p;) {
    while (*p == ' ' || *p == ',') p++;
    const char *e = p;
    while (*e && *e != ',') e++;
    size_t len = (size_t)(e - p);
    while (len && p[len - 1] == ' ') len--;
    const char *one[2] = { NULL, NULL };
    const char *const *list = one;
    char ext[64] = "";
    if (len == 1 && p[0] == '*') { rk__free_paths(globs, n); return 0; }   /* any file */
    if (len && p[0] == '.' && len < sizeof ext) { memcpy(ext, p, len); ext[len] = 0; one[0] = ext; }
    else if (len > 2 && p[len - 2] == '/' && p[len - 1] == '*')
      list = !strncmp(p, "image/", 6) ? family_image : !strncmp(p, "text/", 5) ? family_text : !strncmp(p, "audio/", 6) ? family_audio : !strncmp(p, "video/", 6) ? family_video : one;
    else for (size_t i = 0; i < sizeof exact / sizeof *exact; i++) if (strlen(exact[i].type) == len && !strncmp(exact[i].type, p, len)) one[0] = exact[i].ext;
    for (int i = 0; list[i]; i++) {
      size_t l = strlen(list[i]) + 2;
      char *g = rk__alloc(l);
      snprintf(g, l, "*%s", list[i]);
      globs = rk__grow(globs, &cap, n + 1, sizeof *globs);
      globs[n++] = g;
    }
    p = e;
  }
  *out = globs;
  return n;
}

/* An edge around a rounded rectangle, dashed along each side or solid; the corners drawn whole. */
static void edge_rect(rk_canvas *c, rk_rect r, float rad, float width, rk_color ink, bool solid) {
  float dash = 6, gap = 4, half = width / 2;
  float x0 = r.x + half, y0 = r.y + half, x1 = r.x + r.w - half, y1 = r.y + r.h - half;
  struct { float ax, ay, bx, by; } side[4] = { { x0 + rad, y0, x1 - rad, y0 }, { x1, y0 + rad, x1, y1 - rad }, { x1 - rad, y1, x0 + rad, y1 }, { x0, y1 - rad, x0, y0 + rad } };
  for (int s = 0; s < 4; s++) {
    float dx = side[s].bx - side[s].ax, dy = side[s].by - side[s].ay, len = sqrtf(dx * dx + dy * dy);
    if (len <= 0) continue;
    int n = solid ? 1 : (int)floorf((len + gap) / (dash + gap));
    float step = n > 1 ? (len - dash) / (n - 1) : 0, ux = dx / len, uy = dy / len;
    for (int i = 0; i < RK_MAX(n, 1); i++) {
      float l = solid ? len : RK_MIN(dash, len), t = n > 1 ? i * step : (len - l) / 2;
      rk_point p[2] = { { side[s].ax + ux * t, side[s].ay + uy * t }, { side[s].ax + ux * (t + l), side[s].ay + uy * (t + l) } };
      rk_canvas_polyline(c, p, 2, width, ink);
    }
  }
  float cx[4] = { x1 - rad, x1 - rad, x0 + rad, x0 + rad }, cy[4] = { y0 + rad, y1 - rad, y1 - rad, y0 + rad };
  for (int k = 0; k < 4; k++) {                            /* a quarter circle at each corner */
    rk_point p[5];
    for (int i = 0; i < 5; i++) {
      float a = (float)M_PI / 2 * (k - 1) + (float)M_PI / 2 * i / 4;
      p[i] = (rk_point){ cx[k] + cosf(a) * rad, cy[k] + sinf(a) * rad };
    }
    rk_canvas_polyline(c, p, 5, width, ink);
  }
}

/* the hint, wrapped and each line centred */
static float hint_height(const char *s, float w, const rk_textfmt *f) { return rk__text_wrap_size(s, f, w, NULL) * f->size * f->line; }
static void hint_draw(rk_canvas *c, const char *s, float x, float w, float y, const rk_textfmt *f, rk_color ink) {
  rk__text_draw_lines(c, s, f, x, y, w, 0.5f, ink);
}

static void drop_draw(rk_view *v, rk_canvas *c, void *st) {
  drop_zone *d = st;
  double now = rk_app_now();
  float k = rk__tween_at(&d->k, now), W = v->pframe.w, H = v->pframe.h, rad = rk_metric_of(RK_RADIUS_S);
  rk_rect b = { 0, 0, W, H };
  if (k > 0) {
    rk_color fill = rk_color_of(RK_ROLE_HOVER);
    fill = (fill & 0xffffff00u) | (rk_color)lroundf((fill & 0xff) * k);
    rk_canvas_fill_round_rect(c, b, rad, fill);
  }
  rk_color edge = rk__mix(rk_color_of(RK_ROLE_TEXT_MEDIUM), rk_color_of(RK_ROLE_TEXT), k);
  edge_rect(c, b, rad, 2, edge, d->over);                  /* solid while files are over it, as the web's border-style */
  rk_textfmt tf = rk__textfmt(RK_TEXT_STRONG, 0), hf = rk__textfmt(RK_TEXT_BODY, 0);
  float pad = rk_metric_of(RK_SPACE_M), iw = RK_MAX(0.0f, W - 2 * pad), gap = rk_metric_of(RK_SPACE_XS);
  float th = d->title ? tf.size * tf.line : 0, hh = d->hint ? hint_height(d->hint, iw, &hf) : 0;
  float y = (H - th - (d->hint ? gap + hh : 0)) / 2;
  if (d->title) {
    float tw = rk__text_width(d->title, -1, &tf);
    rk__text_draw(c, d->title, -1, (W - tw) / 2, y, &tf, rk_color_of(RK_ROLE_TEXT));
    y += th + gap;
  }
  if (d->hint) hint_draw(c, d->hint, pad, iw, y, &hf, rk__mix(rk_color_of(RK_ROLE_TEXT_HIGH), rk_color_of(RK_ROLE_TEXT), k));
  if (rk__tween_moving(&d->k, now)) { rk__view_dirty(v); rk__app_request_frame(); }
}

static rk_size drop_measure(rk_view *v, rk_size avail, void *st) {
  (void)v;
  drop_zone *d = st;
  rk_textfmt tf = rk__textfmt(RK_TEXT_STRONG, 0), hf = rk__textfmt(RK_TEXT_BODY, 0);
  float w = isfinite(avail.w) ? avail.w : 360, iw = RK_MAX(0.0f, w - 2 * rk_metric_of(RK_SPACE_M));
  float h = 2 * rk_metric_of(RK_SPACE_L) + (d->title ? tf.size * tf.line : 0) + (d->hint ? rk_metric_of(RK_SPACE_XS) + hint_height(d->hint, iw, &hf) : 0);
  return (rk_size){ w, RK_MAX(120.0f, h) };
}

static void drop_set_over(rk_view *v, drop_zone *d, bool over) {
  if (d->over == over) return;
  d->over = over;
  rk__tween_to(&d->k, rk_app_now(), over ? 1 : 0, rk_duration_of(RK_DURATION_SHORT), rk__ease_base);
  rk__view_dirty(v);
  rk__app_request_frame();
}

static void drop_take(rk_view *v, drop_zone *d, const char *const *files, int n) {
  if (n <= 0) return;
  const char **taken = rk__alloc((size_t)n * sizeof *taken), **refused = rk__alloc((size_t)n * sizeof *refused);
  int nt = 0, nr = 0;
  for (int i = 0; i < n; i++) { if (rk__accepts(d->accept, files[i])) taken[nt++] = files[i]; else refused[nr++] = files[i]; }
  if (nt && d->files) d->files(v, taken, nt, refused, nr, d->user);
  free(taken); free(refused);
}

static bool drop_event(rk_view *v, const rk_event *e, void *st) {
  drop_zone *d = st;
  switch (e->type) {
    case RK_EVENT_DRAG_MOVE: drop_set_over(v, d, true); return true;
    case RK_EVENT_DRAG_LEAVE: drop_set_over(v, d, false); return true;
    case RK_EVENT_DROP: drop_set_over(v, d, false); drop_take(v, d, e->files, e->nfiles); return true;
    default: return false;
  }
}

static void drop_chosen(rk_window *w, const char *const *files, int n, void *user) {
  (void)w;
  rk_view *v = user;
  if (v->cls && v->state && n > 0) drop_take(v, v->state, files, n);
  rk_release(v);
}

static void drop_activate(rk_view *v, void *user) {
  (void)user;
  drop_zone *d = v->state;
  rk_window *w = rk__view_window(v);
  if (!w) return;
  rk_retain(v);                                            /* kept until the chooser answers */
  if (!rk_window_choose_files(w, d->accept, true, drop_chosen, v)) rk_release(v);
}

static void drop_destroy(void *st) { drop_zone *d = st; free(d->title); free(d->hint); free(d->accept); free(d); }

/* read aloud: its title, or the app's word for files as it is now */
static void drop_name(rk_view *v, drop_zone *d) { rk__setstr(&v->access_label, d->title ? d->title : rk__words()->files); }
static void drop_layout(rk_view *v, void *st) { drop_name(v, st); }

static const rk_class drop_class = { { sizeof(rk_view_class), "rk_drop", drop_draw, drop_measure, drop_layout, drop_event, drop_destroy }, NULL, NULL };

rk_view *rk_drop(const char *title, const char *hint, const char *accept,
                 void (*files)(rk_view *drop, const char *const *taken, int ntaken, const char *const *refused, int nrefused, void *user), void *user) {
  drop_zone *d = rk__alloc(sizeof *d);
  d->title = title ? rk__strdup(title) : NULL; d->hint = hint ? rk__strdup(hint) : NULL; d->accept = accept ? rk__strdup(accept) : NULL;
  d->files = files; d->user = user;
  rk_view *v = rk__view_create(&drop_class, d);
  v->focusable = true;
  v->access = RK_ACCESS_BUTTON;
  drop_name(v, d);
  v->own_activate = drop_activate; v->press = RK_PRESS_FLAT;   /* its own: an app's handler may come too */
  return v;
}

void rk__drop_detach(rk_view *v) { if (v && v->cls == &drop_class) ((drop_zone *)v->state)->files = NULL; }   /* declare.c */

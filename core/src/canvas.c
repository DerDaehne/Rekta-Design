/* canvas.c: drawing in a view's logical coordinates. The canvas maps them to device pixels (offset and scale; a view
   that turns in depth draws flat into a layer first), clips, and applies the opacity it carries. */
#include "rk_internal.h"
#include "rk_icons.h"

void rk__canvas_init(rk_canvas *c, rk_bitmap *target, float scale) {
  c->target = target;
  c->nclip = 1;
  c->clips[0] = (rk_irect){ 0, 0, target->w, target->h };
  c->ox = c->oy = 0;
  c->scale = scale;
  c->alpha = 1;
}

#define DX(c, x) ((c)->ox + (x) * (c)->scale)
#define DY(c, y) ((c)->oy + (y) * (c)->scale)
/* Fills land on whole device pixels, so an edge is one crisp step rather than a half-covered blur. */
#define SX(c, x) roundf(DX(c, x))
#define SY(c, y) roundf(DY(c, y))

void rk_canvas_fill_rect(rk_canvas *c, rk_rect r, rk_color color) {
  float x0 = SX(c, r.x), y0 = SY(c, r.y), x1 = SX(c, r.x + r.w), y1 = SY(c, r.y + r.h);
  if (r.w > 0 && x1 <= x0) x1 = x0 + 1;                        /* a hairline stays one pixel */
  if (r.h > 0 && y1 <= y0) y1 = y0 + 1;
  rk__fill_rect(c->target, rk__canvas_clip(c), x0, y0, x1, y1, rk__pm(color, c->alpha));
}

void rk_canvas_fill_round_rect(rk_canvas *c, rk_rect r, float radius, rk_color color) {
  if (radius * c->scale < 0.25f) { rk_canvas_fill_rect(c, r, color); return; }
  rk__path_reset(&c->path);
  rk__path_round_rect(&c->path, SX(c, r.x), SY(c, r.y), SX(c, r.x + r.w), SY(c, r.y + r.h), radius * c->scale);
  rk__path_fill(c->target, rk__canvas_clip(c), &c->path, rk__pm(color, c->alpha));
}

/* A frame inside a rounded rect: the outer contour less the inner one, wound the other way, as one path. */
void rk__canvas_ring(rk_canvas *c, rk_rect r, float radius, float width, rk_color color) {
  rk__path_reset(&c->path);
  rk__path_round_rect(&c->path, SX(c, r.x), SY(c, r.y), SX(c, r.x + r.w), SY(c, r.y + r.h), radius * c->scale);
  rk_path inner = { 0 };
  rk__path_round_rect(&inner, SX(c, r.x + width), SY(c, r.y + width), SX(c, r.x + r.w - width), SY(c, r.y + r.h - width),
                      RK_MAX(0.0f, radius - width) * c->scale);
  for (int i = inner.ends[0] - 1; i >= 0; i--) {
    if (i == inner.ends[0] - 1) rk__path_move(&c->path, inner.pts[i * 2], inner.pts[i * 2 + 1]);
    else rk__path_line(&c->path, inner.pts[i * 2], inner.pts[i * 2 + 1]);
  }
  rk__path_close(&c->path);
  rk__path_free(&inner);
  rk__path_fill(c->target, rk__canvas_clip(c), &c->path, rk__pm(color, c->alpha));
}

void rk_canvas_stroke_rect(rk_canvas *c, rk_rect r, float width, rk_color color) {
  rk_canvas_fill_rect(c, (rk_rect){ r.x, r.y, r.w, width }, color);
  rk_canvas_fill_rect(c, (rk_rect){ r.x, r.y + r.h - width, r.w, width }, color);
  rk_canvas_fill_rect(c, (rk_rect){ r.x, r.y + width, width, r.h - 2 * width }, color);
  rk_canvas_fill_rect(c, (rk_rect){ r.x + r.w - width, r.y + width, width, r.h - 2 * width }, color);
}

void rk_canvas_fill_polygon(rk_canvas *c, const rk_point *pts, int n, rk_color color) {
  if (n < 3) return;
  rk__path_reset(&c->path);
  rk__path_move(&c->path, DX(c, pts[0].x), DY(c, pts[0].y));
  for (int i = 1; i < n; i++) rk__path_line(&c->path, DX(c, pts[i].x), DY(c, pts[i].y));
  rk__path_close(&c->path);
  rk__path_fill(c->target, rk__canvas_clip(c), &c->path, rk__pm(color, c->alpha));
}

static float *xy_buf;
static int xy_cap;

void rk_canvas_polyline(rk_canvas *c, const rk_point *pts, int n, float width, rk_color color) {
  if (n < 2) return;
  xy_buf = rk__grow(xy_buf, &xy_cap, n * 2, sizeof(float));
  for (int i = 0; i < n; i++) { xy_buf[i * 2] = DX(c, pts[i].x); xy_buf[i * 2 + 1] = DY(c, pts[i].y); }
  rk__path_reset(&c->path);
  rk__path_stroke(&c->path, xy_buf, n, false, width * c->scale, true);
  rk__path_fill(c->target, rk__canvas_clip(c), &c->path, rk__pm(color, c->alpha));
}

/* A name not in the icon set is said once - the same one again, every frame, would only be noise. */
bool rk__icon_check(const char *name, const char *fn) {
  int first, count;
  static char said[64];
  if (!name || rk__icon_find(name, &first, &count)) return true;
  if (strncmp(said, name, sizeof said - 1)) { snprintf(said, sizeof said, "%s", name); rk__warn("%s: no icon is called \"%s\" (rekta(3) lists them)", fn, name); }
  return false;
}

bool rk__icon_find(const char *name, int *first, int *count) {
  int lo = 0, hi = RK_ICON_COUNT - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2, d = strcmp(name, rk_icon_defs[mid].name);
    if (!d) { *first = rk_icon_defs[mid].first; *count = rk_icon_defs[mid].count; return true; }
    if (d < 0) hi = mid - 1; else lo = mid + 1;
  }
  return false;
}

/* An icon: its shapes from the 20-unit grid, scaled to the rect, stroked 1.6 units (mitred, butt ends) or filled. */
void rk_canvas_icon(rk_canvas *c, const char *name, rk_rect r, rk_color color) {
  int first, count;
  if (!name) return;
  if (!rk__icon_find(name, &first, &count)) { rk__icon_check(name, "rk_canvas_icon"); return; }
  float k = RK_MIN(r.w, r.h) / RK_ICON_GRID, ox = r.x + (r.w - RK_ICON_GRID * k) / 2, oy = r.y + (r.h - RK_ICON_GRID * k) / 2;
  uint32_t pm = rk__pm(color, c->alpha);
  rk_irect clip = rk__canvas_clip(c);
  rk__path_reset(&c->path);
  for (int i = first; i < first + count; i++) {
    const rk_icon_contour *ct = &rk_icon_contours[i];
    xy_buf = rk__grow(xy_buf, &xy_cap, ct->count * 2, sizeof(float));
    for (int j = 0; j < ct->count; j++) {
      xy_buf[j * 2] = DX(c, ox + rk_icon_points[(ct->start + j) * 2] * k);
      xy_buf[j * 2 + 1] = DY(c, oy + rk_icon_points[(ct->start + j) * 2 + 1] * k);
    }
    if (ct->fill) {
      /* wound like the strokes, so where a fill meets a stroke they add up instead of cancelling */
      float area = 0;
      for (int j = 0; j < ct->count; j++) {
        int n = (j + 1) % ct->count;
        area += xy_buf[j * 2] * xy_buf[n * 2 + 1] - xy_buf[n * 2] * xy_buf[j * 2 + 1];
      }
      for (int j = 0; j < ct->count; j++) {
        int at = area >= 0 ? j : ct->count - 1 - j;
        if (!j) rk__path_move(&c->path, xy_buf[at * 2], xy_buf[at * 2 + 1]);
        else rk__path_line(&c->path, xy_buf[at * 2], xy_buf[at * 2 + 1]);
      }
      rk__path_close(&c->path);
    } else rk__path_stroke(&c->path, xy_buf, ct->count, ct->closed, RK_ICON_STROKE * k * c->scale, false);
  }
  rk__path_fill(c->target, clip, &c->path, pm);
}

float rk_canvas_text(rk_canvas *c, const char *text, rk_point origin, rk_text_style style, rk_color color) {
  rk_textfmt f = rk__textfmt(style, 0);
  rk__text_draw(c, text, -1, origin.x, origin.y, &f, color);
  return rk__text_width(text, -1, &f);
}

void rk_canvas_push_clip(rk_canvas *c, rk_rect r) {
  if (c->nclip >= RK_COUNT(c->clips)) return;
  rk_irect d = { (int)floorf(DX(c, r.x)), (int)floorf(DY(c, r.y)), (int)ceilf(DX(c, r.x + r.w)), (int)ceilf(DY(c, r.y + r.h)) };
  c->clips[c->nclip] = rk__irect_meet(rk__canvas_clip(c), d);
  c->nclip++;
}

void rk_canvas_pop_clip(rk_canvas *c) { if (c->nclip > 1) c->nclip--; }

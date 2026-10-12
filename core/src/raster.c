/* raster.c: the software rasterizer. Premultiplied 32-bit pixels; shapes with exact area coverage (the signed-area
   accumulation of font-rs), so edges are anti-aliased without supersampling; layers composited through a projective
   map with bilinear sampling, for views that turn in depth. */
#include "rk_internal.h"

/* Pixels ---------------------------------------------------------------------------------------------------------- */

rk_bitmap *rk__bitmap_create(int w, int h) {
  rk_bitmap *b = rk__alloc(sizeof *b);
  b->w = RK_MAX(w, 1); b->h = RK_MAX(h, 1); b->stride = b->w;
  b->cap = (size_t)b->stride * b->h;
  b->px = rk__alloc(b->cap * 4);
  return b;
}

/* A window being resized changes size every frame: it keeps its memory, with a quarter to spare when it grows. */
void rk__bitmap_fit(rk_bitmap **slot, int w, int h) {
  rk_bitmap *b = *slot;
  w = RK_MAX(w, 1); h = RK_MAX(h, 1);
  if (b && b->w == w && b->h == h) return;
  size_t need = (size_t)w * h;
  if (b && need <= b->cap) { b->w = w; b->h = h; b->stride = w; return; }
  rk__bitmap_free(b);
  *slot = b = rk__bitmap_create(w, h);
  if (need > 4096) { free(b->px); b->cap = need + need / 4; b->px = rk__alloc(b->cap * 4); }
}

void rk__bitmap_free(rk_bitmap *b) { if (b) { free(b->px); free(b); } }

void rk__bitmap_clear(rk_bitmap *b, uint32_t pm) {
  for (int y = 0; y < b->h; y++) {
    uint32_t *row = b->px + (size_t)y * b->stride;
    for (int x = 0; x < b->w; x++) row[x] = pm;
  }
}

uint32_t rk__pm(rk_color c, float alpha) {
  float a = ((c & 0xff) / 255.0f) * RK_CLAMP(alpha, 0.0f, 1.0f);
  uint32_t A = (uint32_t)(a * 255.0f + 0.5f);
  uint32_t R = (uint32_t)(((c >> 24) & 0xff) * a + 0.5f), G = (uint32_t)(((c >> 16) & 0xff) * a + 0.5f), B = (uint32_t)(((c >> 8) & 0xff) * a + 0.5f);
  return (A << 24) | (R << 16) | (G << 8) | B;
}

rk_irect rk__irect_meet(rk_irect a, rk_irect b) {
  rk_irect r = { RK_MAX(a.x0, b.x0), RK_MAX(a.y0, b.y0), RK_MIN(a.x1, b.x1), RK_MIN(a.y1, b.y1) };
  return r;
}

/* s over d, s scaled by cov (0..256). */
static inline uint32_t over(uint32_t d, uint32_t s, uint32_t cov) {
  if (cov >= 256) {
    if ((s >> 24) == 255) return s;
  } else {
    uint32_t rb = ((s & 0x00ff00ffu) * cov >> 8) & 0x00ff00ffu, ag = (((s >> 8) & 0x00ff00ffu) * cov >> 8) & 0x00ff00ffu;
    s = rb | (ag << 8);
  }
  uint32_t inv = 256 - (s >> 24);
  uint32_t rb = ((d & 0x00ff00ffu) * inv >> 8) & 0x00ff00ffu, ag = (((d >> 8) & 0x00ff00ffu) * inv >> 8) & 0x00ff00ffu;
  return s + (rb | (ag << 8));
}

static void span(rk_bitmap *b, int y, int x0, int x1, uint32_t pm, uint32_t cov) {
  uint32_t *row = b->px + (size_t)y * b->stride;
  if (cov >= 256 && (pm >> 24) == 255) { for (int x = x0; x < x1; x++) row[x] = pm; return; }
  for (int x = x0; x < x1; x++) row[x] = over(row[x], pm, cov);
}

/* Rectangles: solid inside, the fraction of each edge pixel covered at the border. */
void rk__fill_rect(rk_bitmap *b, rk_irect clip, float x0, float y0, float x1, float y1, uint32_t pm) {
  if (!(x1 > x0 && y1 > y0) || !(pm >> 24)) return;
  rk_irect box = { (int)floorf(x0), (int)floorf(y0), (int)ceilf(x1), (int)ceilf(y1) };
  rk_irect r = rk__irect_meet(rk__irect_meet(box, clip), (rk_irect){ 0, 0, b->w, b->h });
  if (rk__irect_empty(r)) return;
  for (int y = r.y0; y < r.y1; y++) {
    float cy = RK_MIN(y1, (float)y + 1) - RK_MAX(y0, (float)y);
    if (cy <= 0) continue;
    int ix0 = (int)floorf(x0), ix1 = (int)ceilf(x1) - 1;
    for (int x = r.x0; x < r.x1;) {
      if (x > ix0 && x < ix1) {
        int end = RK_MIN(ix1, r.x1);
        span(b, y, x, end, pm, (uint32_t)(cy * 256.0f + 0.5f));
        x = end;
        continue;
      }
      float cx = RK_MIN(x1, (float)x + 1) - RK_MAX(x0, (float)x);
      if (cx > 0) span(b, y, x, x + 1, pm, (uint32_t)(cx * cy * 256.0f + 0.5f));
      x++;
    }
  }
}

/* Paths ----------------------------------------------------------------------------------------------------------- */

void rk__path_reset(rk_path *p) { p->npts = 0; p->nends = 0; }
void rk__path_free(rk_path *p) { free(p->pts); free(p->ends); memset(p, 0, sizeof *p); }

void rk__path_line(rk_path *p, float x, float y) {
  p->pts = rk__grow(p->pts, &p->cap, p->npts * 2 + 2, sizeof(float));
  p->pts[p->npts * 2] = x; p->pts[p->npts * 2 + 1] = y; p->npts++;
}

void rk__path_close(rk_path *p) {
  int start = p->nends ? p->ends[p->nends - 1] : 0;
  if (p->npts - start < 3) { p->npts = start; return; }
  p->ends = rk__grow(p->ends, &p->ecap, p->nends + 1, sizeof(int));
  p->ends[p->nends++] = p->npts;
}

void rk__path_move(rk_path *p, float x, float y) {
  int start = p->nends ? p->ends[p->nends - 1] : 0;
  if (p->npts > start) rk__path_close(p);
  rk__path_line(p, x, y);
}

/* Corners as arcs of up to 24 segments a quarter, from the radius in device pixels. */
void rk__path_round_rect(rk_path *p, float x0, float y0, float x1, float y1, float r) {
  r = RK_MIN(r, RK_MIN(x1 - x0, y1 - y0) / 2);
  if (r < 0.25f) {
    rk__path_move(p, x0, y0); rk__path_line(p, x1, y0); rk__path_line(p, x1, y1); rk__path_line(p, x0, y1); rk__path_close(p);
    return;
  }
  int n = RK_CLAMP((int)ceilf(r * 0.8f), 3, 24);
  const float cx[4] = { x1 - r, x1 - r, x0 + r, x0 + r }, cy[4] = { y0 + r, y1 - r, y1 - r, y0 + r };
  bool first = true;
  for (int k = 0; k < 4; k++) {
    float a0 = (float)M_PI * (-0.5f + 0.5f * k);
    for (int i = 0; i <= n; i++) {
      float a = a0 + (float)M_PI * 0.5f * i / n, x = cx[k] + r * cosf(a), y = cy[k] + r * sinf(a);
      if (first) { rk__path_move(p, x, y); first = false; } else rk__path_line(p, x, y);
    }
  }
  rk__path_close(p);
}

static void quad(rk_path *p, float ax, float ay, float bx, float by, float cx, float cy, float dx, float dy) {
  /* every piece of a stroke winds the same way, so overlaps add up and clamp instead of cancelling */
  float area = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax) + (cx - ax) * (dy - ay) - (cy - ay) * (dx - ax);
  rk__path_move(p, ax, ay);
  if (area >= 0) { rk__path_line(p, bx, by); rk__path_line(p, cx, cy); rk__path_line(p, dx, dy); }
  else { rk__path_line(p, dx, dy); rk__path_line(p, cx, cy); rk__path_line(p, bx, by); }
  rk__path_close(p);
}

static void tri(rk_path *p, float ax, float ay, float bx, float by, float cx, float cy) {
  float area = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
  if (fabsf(area) < 1e-6f) return;
  rk__path_move(p, ax, ay);
  if (area > 0) { rk__path_line(p, bx, by); rk__path_line(p, cx, cy); } else { rk__path_line(p, cx, cy); rk__path_line(p, bx, by); }
  rk__path_close(p);
}

/* A stroke as polygons: a rectangle a segment (butt ends), a mitre at each corner (limited to 4 × the half width,
   then bevelled) or a round join. */
void rk__path_stroke(rk_path *p, const float *xy, int n, bool closed, float width, bool round_joins) {
  float h = width / 2;
  if (n < 2) return;
  int segs = closed ? n : n - 1;
  for (int i = 0; i < segs; i++) {
    float ax = xy[i * 2], ay = xy[i * 2 + 1], bx = xy[((i + 1) % n) * 2], by = xy[((i + 1) % n) * 2 + 1];
    float dx = bx - ax, dy = by - ay, len = sqrtf(dx * dx + dy * dy);
    if (len < 1e-6f) continue;
    float nx = -dy / len * h, ny = dx / len * h;
    quad(p, ax + nx, ay + ny, bx + nx, by + ny, bx - nx, by - ny, ax - nx, ay - ny);
  }
  for (int i = closed ? 0 : 1; i < (closed ? n : n - 1); i++) {
    int ia = (i - 1 + n) % n, ib = (i + 1) % n;
    float px = xy[i * 2], py = xy[i * 2 + 1];
    float d0x = px - xy[ia * 2], d0y = py - xy[ia * 2 + 1], d1x = xy[ib * 2] - px, d1y = xy[ib * 2 + 1] - py;
    float l0 = sqrtf(d0x * d0x + d0y * d0y), l1 = sqrtf(d1x * d1x + d1y * d1y);
    if (l0 < 1e-6f || l1 < 1e-6f) continue;
    d0x /= l0; d0y /= l0; d1x /= l1; d1y /= l1;
    float cross = d0x * d1y - d0y * d1x;
    if (fabsf(cross) < 1e-4f && d0x * d1x + d0y * d1y > 0) continue;   /* straight on */
    if (round_joins) {
      int k = RK_CLAMP((int)ceilf(h * 2), 6, 24);
      rk__path_move(p, px + h, py);
      for (int j = 1; j < k; j++) { float a = 2 * (float)M_PI * j / k; rk__path_line(p, px + h * cosf(a), py + h * sinf(a)); }
      rk__path_close(p);
      continue;
    }
    /* outer side: the side away from the turn */
    float s = cross > 0 ? -1.0f : 1.0f;
    float o0x = -d0y * h * s, o0y = d0x * h * s, o1x = -d1y * h * s, o1y = d1x * h * s;
    float mx = o0x + o1x, my = o0y + o1y, ml = sqrtf(mx * mx + my * my);
    float cosh = (o0x * mx + o0y * my) / (h * ml);                    /* cos of half the outer angle */
    tri(p, px, py, px + o0x, py + o0y, px + o1x, py + o1y);
    if (cosh > 0.25f) {
      float miter = h / cosh;
      tri(p, px + o0x, py + o0y, px + mx / ml * miter, py + my / ml * miter, px + o1x, py + o1y);
    }
  }
}

/* The accumulation rasterizer: every edge adds its signed area to the cells it crosses; a running sum along each row
   gives the exact coverage of each pixel. */
static float *acc_buf;
static int acc_cap;

void rk__path_fill(rk_bitmap *b, rk_irect clip, const rk_path *p, uint32_t pm) {
  if (!p->nends || !(pm >> 24)) return;
  clip = rk__irect_meet(clip, (rk_irect){ 0, 0, b->w, b->h });
  float minx = 1e30f, miny = 1e30f, maxx = -1e30f, maxy = -1e30f;
  for (int i = 0; i < p->ends[p->nends - 1]; i++) {
    float x = p->pts[i * 2], y = p->pts[i * 2 + 1];
    minx = RK_MIN(minx, x); maxx = RK_MAX(maxx, x); miny = RK_MIN(miny, y); maxy = RK_MAX(maxy, y);
  }
  rk_irect box = { (int)floorf(minx), (int)floorf(miny), (int)ceilf(maxx), (int)ceilf(maxy) };
  rk_irect r = rk__irect_meet(box, clip);
  if (rk__irect_empty(r)) return;
  /* columns from the clip's left edge (points further left are pulled onto it, which keeps the coverage to the
     right), rows only where drawn */
  int ox = r.x0, oy = r.y0, w = r.x1 - r.x0 + 2, h = r.y1 - r.y0;
  int need = w * h;
  if (need > acc_cap) { free(acc_buf); acc_cap = need; acc_buf = rk__alloc((size_t)acc_cap * sizeof(float)); }
  memset(acc_buf, 0, (size_t)need * sizeof(float));
  float xmax = (float)(w - 1);
  int start = 0;
  for (int e = 0; e < p->nends; e++) {
    int end = p->ends[e];
    for (int i = start; i < end; i++) {
      int j = i + 1 < end ? i + 1 : start;
      float x0 = p->pts[i * 2] - ox, y0 = p->pts[i * 2 + 1] - oy, x1 = p->pts[j * 2] - ox, y1 = p->pts[j * 2 + 1] - oy;
      if (y0 == y1) continue;
      float dir = 1;
      if (y0 > y1) { float t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; dir = -1; }
      float dxdy = (x1 - x0) / (y1 - y0), x = x0;
      if (y0 < 0) { x -= y0 * dxdy; y0 = 0; }
      if (y1 > h) y1 = (float)h;
      if (y0 >= y1) continue;
      for (int y = (int)y0; y < (int)ceilf(y1) && y < h; y++) {
        float *row = acc_buf + (size_t)y * w;
        float dy = RK_MIN((float)y + 1, y1) - RK_MAX((float)y, y0), xn = x + dxdy * dy, d = dy * dir;
        float a = RK_CLAMP(RK_MIN(x, xn), 0.0f, xmax), c = RK_CLAMP(RK_MAX(x, xn), 0.0f, xmax);
        int a0 = (int)a, c1 = (int)ceilf(c);
        if (c1 <= a0 + 1) {
          float xm = 0.5f * (a + c) - a0;
          row[a0] += d - d * xm;
          if (a0 + 1 < w) row[a0 + 1] += d * xm;
        } else {
          float s = 1.0f / (c - a), af = a - a0, a0v = 0.5f * s * (1 - af) * (1 - af);
          float cf = c - c1 + 1, amv = 0.5f * s * cf * cf;
          row[a0] += d * a0v;
          if (c1 == a0 + 2) row[a0 + 1] += d * (1 - a0v - amv);
          else {
            float a1 = s * (1.5f - af);
            row[a0 + 1] += d * (a1 - a0v);
            for (int k = a0 + 2; k < c1 - 1; k++) row[k] += d * s;
            float a2 = a1 + (c1 - a0 - 3) * s;
            row[c1 - 1] += d * (1 - a2 - amv);
          }
          if (c1 < w) row[c1] += d * amv;
        }
        x = xn;
      }
    }
    start = end;
  }
  for (int y = 0; y < h; y++) {
    const float *row = acc_buf + (size_t)y * w;
    uint32_t *out = b->px + (size_t)(y + oy) * b->stride + ox;
    float sum = 0;
    for (int x = 0; x < r.x1 - ox; x++) {
      sum += row[x];
      float cov = fabsf(sum);
      if (cov < 0.002f) continue;
      out[x] = over(out[x], pm, cov >= 1 ? 256u : (uint32_t)(cov * 256.0f + 0.5f));
    }
  }
}

/* Masks (glyphs) -------------------------------------------------------------------------------------------------- */

void rk__blit_mask(rk_bitmap *b, rk_irect clip, int x, int y, const uint8_t *mask, int w, int h, int stride, uint32_t pm) {
  rk_irect r = rk__irect_meet(rk__irect_meet(clip, (rk_irect){ 0, 0, b->w, b->h }), (rk_irect){ x, y, x + w, y + h });
  for (int py = r.y0; py < r.y1; py++) {
    const uint8_t *m = mask + (size_t)(py - y) * stride - x;
    uint32_t *row = b->px + (size_t)py * b->stride;
    for (int px = r.x0; px < r.x1; px++) if (m[px]) row[px] = over(row[px], pm, m[px] + (m[px] >> 7));
  }
}

/* Layers ---------------------------------------------------------------------------------------------------------- */

void rk__composite_at(rk_bitmap *dst, rk_irect clip, const rk_bitmap *src, int x, int y, float opacity) {
  uint32_t a = (uint32_t)(RK_CLAMP(opacity, 0.0f, 1.0f) * 256.0f + 0.5f);
  if (!a) return;
  rk_irect r = rk__irect_meet(rk__irect_meet(clip, (rk_irect){ 0, 0, dst->w, dst->h }), (rk_irect){ x, y, x + src->w, y + src->h });
  for (int py = r.y0; py < r.y1; py++) {
    const uint32_t *s = src->px + (size_t)(py - y) * src->stride - x;
    uint32_t *d = dst->px + (size_t)py * dst->stride;
    for (int px = r.x0; px < r.x1; px++) if (s[px]) d[px] = over(d[px], s[px], a);
  }
}

/* The projective map of the unit square onto a quad (Heckbert), and its inverse. */
static void square_to_quad(const float q[8], double m[9]) {
  double x0 = q[0], y0 = q[1], x1 = q[2], y1 = q[3], x2 = q[4], y2 = q[5], x3 = q[6], y3 = q[7];
  double sx = x0 - x1 + x2 - x3, sy = y0 - y1 + y2 - y3;
  if (fabs(sx) < 1e-9 && fabs(sy) < 1e-9) {
    m[0] = x1 - x0; m[1] = x2 - x1; m[2] = x0; m[3] = y1 - y0; m[4] = y2 - y1; m[5] = y0; m[6] = 0; m[7] = 0; m[8] = 1;
    return;
  }
  double dx1 = x1 - x2, dx2 = x3 - x2, dy1 = y1 - y2, dy2 = y3 - y2, den = dx1 * dy2 - dx2 * dy1;
  double g = (sx * dy2 - dx2 * sy) / den, h = (dx1 * sy - sx * dy1) / den;
  m[0] = x1 - x0 + g * x1; m[1] = x3 - x0 + h * x3; m[2] = x0;
  m[3] = y1 - y0 + g * y1; m[4] = y3 - y0 + h * y3; m[5] = y0;
  m[6] = g; m[7] = h; m[8] = 1;
}

static bool invert3(const double m[9], double r[9]) {
  double a = m[4] * m[8] - m[5] * m[7], b = m[5] * m[6] - m[3] * m[8], c = m[3] * m[7] - m[4] * m[6];
  double det = m[0] * a + m[1] * b + m[2] * c;
  if (fabs(det) < 1e-12) return false;
  r[0] = a / det; r[1] = (m[2] * m[7] - m[1] * m[8]) / det; r[2] = (m[1] * m[5] - m[2] * m[4]) / det;
  r[3] = b / det; r[4] = (m[0] * m[8] - m[2] * m[6]) / det; r[5] = (m[2] * m[3] - m[0] * m[5]) / det;
  r[6] = c / det; r[7] = (m[1] * m[6] - m[0] * m[7]) / det; r[8] = (m[0] * m[4] - m[1] * m[3]) / det;
  return true;
}

static inline uint32_t lerp_px(uint32_t a, uint32_t b, uint32_t t) {   /* t 0..256 */
  uint32_t rb = (((a & 0x00ff00ffu) * (256 - t) + (b & 0x00ff00ffu) * t) >> 8) & 0x00ff00ffu;
  uint32_t ag = ((((a >> 8) & 0x00ff00ffu) * (256 - t) + ((b >> 8) & 0x00ff00ffu) * t) >> 8) & 0x00ff00ffu;
  return rb | (ag << 8);
}

static inline uint32_t sample(const rk_bitmap *s, float u, float v) {
  u -= 0.5f; v -= 0.5f;
  int x = (int)floorf(u), y = (int)floorf(v);
  uint32_t fx = (uint32_t)((u - x) * 256.0f), fy = (uint32_t)((v - y) * 256.0f);
  if (x >= 0 && y >= 0 && x + 1 < s->w && y + 1 < s->h) {      /* inside: no edge checks */
    const uint32_t *p = s->px + (size_t)y * s->stride + x;
    return lerp_px(lerp_px(p[0], p[1], fx), lerp_px(p[s->stride], p[s->stride + 1], fx), fy);
  }
  #define PX(X, Y) (((X) < 0 || (Y) < 0 || (X) >= s->w || (Y) >= s->h) ? 0u : s->px[(size_t)(Y) * s->stride + (X)])
  uint32_t top = lerp_px(PX(x, y), PX(x + 1, y), fx), bot = lerp_px(PX(x, y + 1), PX(x + 1, y + 1), fx);
  #undef PX
  return lerp_px(top, bot, fy);
}

static rk_path quad_path;

void rk__composite(rk_bitmap *dst, rk_irect clip, const rk_bitmap *src, const float q[8], float opacity) {
  if (opacity <= 0.002f) return;
  double m[9], inv[9];
  square_to_quad(q, m);
  if (!invert3(m, inv)) return;
  /* the quad's coverage first, as a mask in a scratch layer the size of the quad's box */
  float minx = q[0], maxx = q[0], miny = q[1], maxy = q[1];
  for (int i = 1; i < 4; i++) { minx = RK_MIN(minx, q[i * 2]); maxx = RK_MAX(maxx, q[i * 2]); miny = RK_MIN(miny, q[i * 2 + 1]); maxy = RK_MAX(maxy, q[i * 2 + 1]); }
  rk_irect r = rk__irect_meet(rk__irect_meet(clip, (rk_irect){ 0, 0, dst->w, dst->h }), (rk_irect){ (int)floorf(minx), (int)floorf(miny), (int)ceilf(maxx), (int)ceilf(maxy) });
  if (rk__irect_empty(r)) return;
  static rk_bitmap cover = { 0, 0, 0, NULL, 0 };
  int w = r.x1 - r.x0, h = r.y1 - r.y0;
  if ((size_t)w * h > cover.cap) { free(cover.px); cover.cap = (size_t)w * h; cover.px = rk__alloc(cover.cap * 4); }
  cover.w = w; cover.h = h; cover.stride = w;
  memset(cover.px, 0, (size_t)w * h * 4);
  rk__path_reset(&quad_path);
  rk__path_move(&quad_path, q[0] - r.x0, q[1] - r.y0);
  for (int i = 1; i < 4; i++) rk__path_line(&quad_path, q[i * 2] - r.x0, q[i * 2 + 1] - r.y0);
  rk__path_close(&quad_path);
  rk__path_fill(&cover, (rk_irect){ 0, 0, w, h }, &quad_path, 0xff000000u);
  uint32_t a = (uint32_t)(RK_CLAMP(opacity, 0.0f, 1.0f) * 256.0f + 0.5f);
  /* the inverse map in source pixels, stepped along each row: one division a pixel, none when it is affine */
  float a0 = (float)(inv[0] * src->w), a1 = (float)(inv[1] * src->w), a2 = (float)(inv[2] * src->w);
  float b0 = (float)(inv[3] * src->h), b1 = (float)(inv[4] * src->h), b2 = (float)(inv[5] * src->h);
  float c0 = (float)inv[6], c1 = (float)inv[7], c2 = (float)inv[8];
  bool affine = fabsf(c0) < 1e-9f && fabsf(c1) < 1e-9f;
  for (int y = 0; y < h; y++) {
    uint32_t *d = dst->px + (size_t)(y + r.y0) * dst->stride + r.x0;
    const uint32_t *cv = cover.px + (size_t)y * w;
    float X = r.x0 + 0.5f, Y = y + r.y0 + 0.5f;
    float nu = a0 * X + a1 * Y + a2, nv = b0 * X + b1 * Y + b2, nz = c0 * X + c1 * Y + c2;
    for (int x = 0; x < w; x++, nu += a0, nv += b0, nz += c0) {
      uint32_t c = cv[x] >> 24;
      if (!c) continue;
      float z = affine ? 1.0f / c2 : 1.0f / nz;
      uint32_t s = sample(src, nu * z, nv * z);
      if (!s) continue;
      d[x] = over(d[x], s, (c + (c >> 7)) * a >> 8);
    }
  }
}

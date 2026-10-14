/* image.c: a picture from a PNG or JPEG file or from memory, through stb_image. Its size is known at once (from the
   file's header); the pixels are decoded by the background worker, away from the UI thread, and fade in over
   duration-short where a quiet placeholder stood. It fits its view whole (contain) or fills it (cover), keeping its
   shape. Drawn at a size, it is scaled once with a tent filter that averages every source pixel when it shrinks,
   and that copy is reused until the size changes. A big picture (over a million pixels) is scaled by the worker
   too: until the copy comes, the last one stands in, stretched, and only the newest size is kept - a frame never
   waits for it. Included after controls.c. */
#include "rk_internal.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO                                      /* files are mapped here, not opened by stb */
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STBI_NO_GIF                                         /* (its declarations stay otherwise) */
#define STBI_MAX_DIMENSIONS 16384                          /* a 16k picture is 1 GB of pixels: enough */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif
#include "../third_party/stb_image.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

typedef struct image_job image_job;
typedef struct image {
  int w, h;                                                /* pixels, from the header; 0 when it cannot be read */
  bool ready, failed;
  uint32_t *px;                                            /* decoded, premultiplied ARGB, w x h */
  rk_bitmap fitted;                                        /* scaled to the size last drawn */
  rk_fit fit;
  image_job *job;                                          /* decoding; NULL once done */
  struct fit_job *fitting; int want_w, want_h;             /* a big picture being scaled by the worker, to this size */
  bool shown;                                              /* the first copy is in: it has faded in */
  rk_view *view;
} image;

struct image_job {
  image *img;                                              /* NULL if the view went before the job was done */
  unsigned char *data; size_t size; bool mapped;           /* the job's own: freed when it is done */
  uint32_t *px; int w, h;
};

/* Decoding: on the worker; the pixels come back to the UI thread -------------------------------------------------- */

static void image_done(void *user) {
  image_job *j = user;
  image *img = j->img;
  if (img) {
    img->job = NULL;
    if (j->px) { img->px = j->px; j->px = NULL; img->ready = true; }
    else img->failed = true;
    rk__view_dirty(img->view);
    rk_release(img->view);
  }
  free(j->px);
  free(j);
}

static void image_decode(void *user) {
  image_job *j = user;
  int w = 0, h = 0, n = 0;
  unsigned char *rgba = stbi_load_from_memory(j->data, (int)j->size, &w, &h, &n, 4);
  if (j->mapped) rk__file_unmap(j->data, j->size); else free(j->data);
  j->data = NULL;
  if (rgba && w > 0 && h > 0) {
    j->px = malloc((size_t)w * h * sizeof *j->px);
    if (j->px) {
      for (size_t i = 0, n2 = (size_t)w * h; i < n2; i++) {   /* straight RGBA to premultiplied ARGB */
        unsigned r = rgba[i * 4], g = rgba[i * 4 + 1], b = rgba[i * 4 + 2], a = rgba[i * 4 + 3];
        j->px[i] = (a << 24) | ((r * a + 127) / 255 << 16) | ((g * a + 127) / 255 << 8) | ((b * a + 127) / 255);
      }
      j->w = w; j->h = h;
    }
  }
  stbi_image_free(rgba);
  rk_app_post(image_done, j);
}

/* Scaling: separable, a tent as wide as one source pixel per destination pixel when shrinking --------------------- */

typedef struct tap { int first, n; float *w; } tap;

static tap *taps_for(int src, int dst) {
  tap *t = rk__alloc((size_t)dst * sizeof *t);
  float ratio = (float)src / dst, radius = RK_MAX(1.0f, ratio);
  for (int i = 0; i < dst; i++) {
    float c = (i + 0.5f) * ratio - 0.5f;
    int a = (int)floorf(c - radius) + 1, b = (int)ceilf(c + radius) - 1;
    a = RK_MAX(a, 0); b = RK_MIN(b, src - 1);
    if (b < a) b = a = RK_CLAMP((int)floorf(c + 0.5f), 0, src - 1);
    t[i].first = a; t[i].n = b - a + 1;
    t[i].w = rk__alloc((size_t)t[i].n * sizeof(float));
    float sum = 0;
    for (int k = 0; k < t[i].n; k++) { float d = fabsf(a + k - c) / radius; t[i].w[k] = d < 1 ? 1 - d : 0; sum += t[i].w[k]; }
    for (int k = 0; k < t[i].n; k++) t[i].w[k] = sum > 0 ? t[i].w[k] / sum : 1.0f / t[i].n;
  }
  return t;
}
static void taps_free(tap *t, int n) { for (int i = 0; i < n; i++) free(t[i].w); free(t); }

static void resample(const uint32_t *src, int sw, int sh, uint32_t *dst, int dw, int dh) {
  tap *tx = taps_for(sw, dw), *ty = taps_for(sh, dh);
  float *mid = rk__alloc((size_t)dw * sh * 4 * sizeof(float));   /* rows scaled across, four channels */
  for (int y = 0; y < sh; y++) {
    const uint32_t *row = src + (size_t)y * sw;
    for (int x = 0; x < dw; x++) {
      float acc[4] = { 0, 0, 0, 0 };
      for (int k = 0; k < tx[x].n; k++) {
        uint32_t p = row[tx[x].first + k];
        float w = tx[x].w[k];
        acc[0] += (p >> 24) * w; acc[1] += (p >> 16 & 255) * w; acc[2] += (p >> 8 & 255) * w; acc[3] += (p & 255) * w;
      }
      memcpy(mid + ((size_t)y * dw + x) * 4, acc, sizeof acc);
    }
  }
  for (int y = 0; y < dh; y++) {
    for (int x = 0; x < dw; x++) {
      float acc[4] = { 0, 0, 0, 0 };
      for (int k = 0; k < ty[y].n; k++) {
        const float *m = mid + ((size_t)(ty[y].first + k) * dw + x) * 4;
        float w = ty[y].w[k];
        for (int c = 0; c < 4; c++) acc[c] += m[c] * w;
      }
      uint32_t a = (uint32_t)RK_CLAMP(acc[0] + 0.5f, 0.0f, 255.0f), out[3];
      for (int c = 0; c < 3; c++) out[c] = (uint32_t)RK_CLAMP(acc[c + 1] + 0.5f, 0.0f, (float)a);   /* premultiplied: never above alpha */
      dst[(size_t)y * dw + x] = a << 24 | out[0] << 16 | out[1] << 8 | out[2];
    }
  }
  free(mid);
  taps_free(tx, dw); taps_free(ty, dh);
}

/* A copy at a size: at once for a small picture, by the worker for a big one ------------------------------------- */

typedef struct fit_job { image *img; const uint32_t *src; int sw, sh, dw, dh; uint32_t *out; } fit_job;

static void image_show(image *img) {                       /* the first copy is in: it fades in where the placeholder was */
  if (img->shown) return;
  img->shown = true;
  float a = 0, b = 1;
  img->view->value = 1;
  rk__animate(img->view, RK_PROP_VALUE, &a, &b, 1, rk__reduced() ? 0 : rk_duration_of(RK_DURATION_SHORT), 0, rk__ease_base);
}

static void fitted_take(image *img, uint32_t *px, int w, int h) {
  free(img->fitted.px);
  img->fitted.px = px; img->fitted.w = img->fitted.stride = w; img->fitted.h = h; img->fitted.cap = (size_t)w * h;
  image_show(img);
}

static void fit_done(void *user) {
  fit_job *j = user;
  image *img = j->img;
  if (img && img->fitting == j) {                          /* the newest size: kept */
    img->fitting = NULL;
    fitted_take(img, j->out, j->dw, j->dh); j->out = NULL;
    rk__view_dirty(img->view);
  } else if (img && !img->fitting) rk__view_dirty(img->view);
  if (img) rk_release(img->view);
  free(j->out);
  free(j);
}

static void fit_work(void *user) {
  fit_job *j = user;
  j->out = malloc((size_t)j->dw * j->dh * sizeof *j->out);
  if (j->out) resample(j->src, j->sw, j->sh, j->out, j->dw, j->dh);
  rk_app_post(fit_done, j);
}

/* Asks for a copy at dw x dh; true when it is there to draw. */
static bool image_fit_to(image *img, int dw, int dh) {
  img->want_w = dw; img->want_h = dh;
  if (img->fitted.w == dw && img->fitted.h == dh) return true;
  if ((long)img->w * img->h <= 1000000) {                  /* small: now */
    uint32_t *px = rk__alloc((size_t)dw * dh * sizeof *px);
    resample(img->px, img->w, img->h, px, dw, dh);
    fitted_take(img, px, dw, dh);
    return true;
  }
  if (img->fitting) return false;                          /* one at a time; its end asks again if the size moved on */
  fit_job *j = rk__alloc(sizeof *j);
  j->img = img; j->src = img->px; j->sw = img->w; j->sh = img->h; j->dw = dw; j->dh = dh;
  img->fitting = j;
  rk_retain(img->view);                                    /* its pixels stay while the worker reads them */
  rk__background(fit_work, j);
  return img->fitted.w == dw && img->fitted.h == dh;       /* (a headless run did it at once) */
}

/* The last copy, stretched over a new size while the right one is made: nearest pixels, cheap. */
static void stretch_at(rk_bitmap *dst, rk_irect clip, const rk_bitmap *src, int x, int y, int w, int h, float opacity) {
  uint32_t a = (uint32_t)(RK_CLAMP(opacity, 0.0f, 1.0f) * 256.0f + 0.5f);
  rk_irect r = rk__irect_meet(rk__irect_meet(clip, (rk_irect){ 0, 0, dst->w, dst->h }), (rk_irect){ x, y, x + w, y + h });
  if (!a || src->w <= 0 || src->h <= 0) return;
  for (int py = r.y0; py < r.y1; py++) {
    const uint32_t *s = src->px + (size_t)((long)(py - y) * src->h / h) * src->stride;
    uint32_t *d = dst->px + (size_t)py * dst->stride;
    for (int px = r.x0; px < r.x1; px++) { uint32_t p = s[(long)(px - x) * src->w / w]; if (p) d[px] = rk__over(d[px], p, a); }
  }
}

/* The view -------------------------------------------------------------------------------------------------------- */

/* Where the picture goes in a box: whole (contain) or filling it (cover), centred, its shape kept. */
static rk_rect image_place(const image *img, rk_rect box) {
  if (img->w <= 0 || img->h <= 0) return box;
  float sx = box.w / img->w, sy = box.h / img->h, s = img->fit == RK_FIT_COVER ? RK_MAX(sx, sy) : RK_MIN(sx, sy);
  float w = img->w * s, h = img->h * s;
  return (rk_rect){ box.x + (box.w - w) / 2, box.y + (box.h - h) / 2, w, h };
}

static rk_size image_measure(rk_view *v, rk_size avail, void *st) {
  (void)v;
  image *img = st;
  float w = (float)img->w, h = (float)img->h;
  if (isfinite(avail.w) && avail.w > 0 && w > avail.w) { h *= avail.w / w; w = avail.w; }   /* narrower than it is: smaller, its shape kept */
  return (rk_size){ w, h };
}

static void image_draw(rk_view *v, rk_canvas *c, void *st) {
  image *img = st;
  rk_rect box = { 0, 0, v->pframe.w, v->pframe.h }, at = image_place(img, box);
  bool cover = img->fit == RK_FIT_COVER;
  if (cover) rk_canvas_push_clip(c, box);
  int x0 = (int)floorf(c->ox + at.x * c->scale + 0.5f), y0 = (int)floorf(c->oy + at.y * c->scale + 0.5f);
  int dw = (int)floorf(c->ox + (at.x + at.w) * c->scale + 0.5f) - x0, dh = (int)floorf(c->oy + (at.y + at.h) * c->scale + 0.5f) - y0;
  bool sized = img->ready && dw > 0 && dh > 0 && dw <= 16384 && dh <= 16384;
  bool exact = sized && image_fit_to(img, dw, dh);
  float fade = img->shown ? RK_CLAMP(v->pvalue, 0.0f, 1.0f) : 0;
  if (fade < 1) rk_canvas_fill_rect(c, cover ? box : at, rk_color_of(RK_ROLE_TEXT_LOW));   /* the placeholder, until it is in */
  if (sized && img->shown) {
    if (exact) rk__composite_at(c->target, rk__canvas_clip(c), &img->fitted, x0, y0, c->alpha * fade);
    else stretch_at(c->target, rk__canvas_clip(c), &img->fitted, x0, y0, dw, dh, c->alpha * fade);   /* the right copy is coming */
  }
  if (cover) rk_canvas_pop_clip(c);
}

static void image_destroy(void *st) {
  image *img = st;
  if (img->job) img->job->img = NULL;                      /* the job finishes on its own and frees what it made */
  if (img->fitting) img->fitting->img = NULL;
  free(img->px); free(img->fitted.px); free(img->fitted.occ); free(img);
}

static const rk_class image_class = { { sizeof(rk_view_class), "rk_image", image_draw, image_measure, NULL, NULL, image_destroy }, NULL, NULL };

static rk_view *image_make(unsigned char *data, size_t size, bool mapped) {
  image *img = rk__alloc(sizeof *img);
  rk_view *v = rk__view_create(&image_class, img);
  img->view = v;
  v->access = RK_ACCESS_IMAGE;
  int n = 0;
  if (!data || !size || size > INT_MAX || !stbi_info_from_memory(data, (int)size, &img->w, &img->h, &n)) {
    img->w = img->h = 0; img->failed = true;
    if (data) { if (mapped) rk__file_unmap(data, size); else free(data); }
    return v;
  }
  image_job *j = rk__alloc(sizeof *j);
  j->img = img; j->data = data; j->size = size; j->mapped = mapped;
  img->job = j;
  rk_retain(v);                                            /* drawn again when the pixels come; released then */
  rk__background(image_decode, j);
  return v;
}

rk_view *rk_image(const char *path) {
  size_t size = 0;
  const unsigned char *data = path ? rk__file_map(path, &size) : NULL;
  return image_make((unsigned char *)data, size, true);
}

rk_view *rk_image_from_memory(const void *data, size_t size) {
  unsigned char *copy = data && size ? malloc(size) : NULL;   /* the caller's bytes may go before the decoding is done */
  if (copy) memcpy(copy, data, size);
  return image_make(copy, copy ? size : 0, false);
}

static image *image_of(rk_view *v) { return v && v->cls == &image_class ? v->state : NULL; }

void rk_image_set_fit(rk_view *v, rk_fit fit) { image *img = RK__EXPECT(image_of, v, "an image"); if (img && img->fit != fit) { img->fit = fit; rk__view_dirty(v); } }

bool rk_image_size(rk_view *v, int *width, int *height) {
  image *img = RK__EXPECT(image_of, v, "an image");
  if (width) *width = img ? img->w : 0;
  if (height) *height = img ? img->h : 0;
  return img && !img->failed;
}

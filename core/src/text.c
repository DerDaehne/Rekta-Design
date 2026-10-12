/* text.c: Barlow through stb_truetype. Glyphs are rasterized at the device size with quarter-pixel horizontal
   positions and cached; kerning comes from the font's GPOS pairs; text styles follow typography.md. */
#include "rk_internal.h"
#include "rk_tokens.h"
#include "rk_fonts.h"

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif
#include "../third_party/stb_truetype.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

typedef struct font { stbtt_fontinfo info; int weight; float ascent, descent, upem; int zero_adv; bool ok; } font;
static font fonts[RK_FONT_FILE_COUNT];
static bool fonts_ready;

static void fonts_init(void) {
  if (fonts_ready) return;
  fonts_ready = true;
  for (int i = 0; i < RK_FONT_FILE_COUNT; i++) {
    font *f = &fonts[i];
    f->weight = rk_font_files[i].weight;
    if (!stbtt_InitFont(&f->info, rk_font_files[i].data, stbtt_GetFontOffsetForIndex(rk_font_files[i].data, 0))) continue;
    int a, d, g;
    stbtt_GetFontVMetrics(&f->info, &a, &d, &g);
    f->upem = (float)ttUSHORT(f->info.data + f->info.head + 18);
    f->ascent = a / f->upem; f->descent = -d / f->upem;
    int adv, lsb;
    stbtt_GetCodepointHMetrics(&f->info, '0', &adv, &lsb);
    f->zero_adv = adv;
    f->ok = true;
  }
}

static font *font_for(int weight) {
  fonts_init();
  font *best = NULL;
  for (int i = 0; i < RK_FONT_FILE_COUNT; i++)
    if (fonts[i].ok && (!best || abs(fonts[i].weight - weight) < abs(best->weight - weight))) best = &fonts[i];
  return best;
}

/* Styles ---------------------------------------------------------------------------------------------------------- */

static float fluid(float lo, float hi, float vw, float host) { return vw > 0 && host > 0 ? RK_CLAMP(vw * host, lo, hi) : lo; }
#define SIZE(N, host) fluid(RK_TOK_SIZE_##N, RK_TOK_SIZE_##N##_MAX, RK_TOK_SIZE_##N##_VW, host)

rk_textfmt rk__textfmt(rk_text_style style, float host) {
  rk_textfmt f = { RK_TOK_WEIGHT_REGULAR, RK_TOK_SIZE_S, RK_TOK_LINE_BODY, 0, false, RK_ROLE_TEXT };
  switch (style) {
    case RK_TEXT_BODY: break;
    case RK_TEXT_STRONG: f.weight = RK_TOK_WEIGHT_SEMIBOLD; break;
    case RK_TEXT_SMALL: f.size = RK_TOK_SIZE_XS; f.line = RK_TOK_LINE_COMPACT; break;
    case RK_TEXT_META: f.size = RK_TOK_SIZE_XXS; f.line = RK_TOK_LINE_COMPACT; f.tabular = true; f.color = RK_ROLE_TEXT_HIGH; break;
    case RK_TEXT_LABEL: f.size = RK_TOK_SIZE_XS; f.line = RK_TOK_LINE_COMPACT; f.weight = RK_TOK_WEIGHT_SEMIBOLD; f.color = RK_ROLE_TEXT_HIGH; break;
    case RK_TEXT_KICKER: f.size = RK_TOK_SIZE_XS; f.line = RK_TOK_LINE_COMPACT; f.weight = RK_TOK_WEIGHT_SEMIBOLD; f.tracking = 0.02f; f.color = RK_ROLE_TEXT_HIGH; break;
    case RK_TEXT_GROUP: f.size = SIZE(GROUP, host); f.line = RK_TOK_LINE_TIGHT; break;
    case RK_TEXT_TITLE: f.size = SIZE(TITLE, host); f.line = RK_TOK_LINE_TITLE; f.weight = RK_TOK_WEIGHT_SEMIBOLD; break;
    case RK_TEXT_STAT: f.size = SIZE(XXL, host); f.line = 1.0f; f.weight = RK_TOK_WEIGHT_MEDIUM; f.tabular = true; break;
    case RK_TEXT_PIVOT: f.size = SIZE(PIVOT_ACTIVE, host); f.line = RK_TOK_LINE_NAV; f.weight = RK_TOK_WEIGHT_LIGHT; break;
    case RK_TEXT_NAV: f.size = SIZE(NAV_ACTIVE, host); f.line = RK_TOK_LINE_NAV; f.weight = RK_TOK_WEIGHT_LIGHT; break;
    default: break;
  }
  return f;
}

/* UTF-8 ----------------------------------------------------------------------------------------------------------- */

static int utf8(const char *s, int len, int *i) {
  const unsigned char *p = (const unsigned char *)s + *i;
  int n = len - *i, c = p[0];
  if (c < 0x80) { *i += 1; return c; }
  if ((c & 0xe0) == 0xc0 && n >= 2) { *i += 2; return ((c & 0x1f) << 6) | (p[1] & 0x3f); }
  if ((c & 0xf0) == 0xe0 && n >= 3) { *i += 3; return ((c & 0x0f) << 12) | ((p[1] & 0x3f) << 6) | (p[2] & 0x3f); }
  if ((c & 0xf8) == 0xf0 && n >= 4) { *i += 4; return ((c & 0x07) << 18) | ((p[1] & 0x3f) << 12) | ((p[2] & 0x3f) << 6) | (p[3] & 0x3f); }
  *i += 1;
  return 0xfffd;
}

/* Measuring ------------------------------------------------------------------------------------------------------- */

typedef struct pen { font *f; float scale; float x; int prev; const rk_textfmt *fmt; } pen;

/* Advances the pen over one code point; returns the glyph and where it starts (in font units × scale). */
static int pen_step(pen *p, int cp, float *gx, float *center) {
  int g = stbtt_FindGlyphIndex(&p->f->info, cp);
  if (p->prev >= 0 && g) p->x += stbtt_GetGlyphKernAdvance(&p->f->info, p->prev, g) * p->scale;
  int adv, lsb;
  stbtt_GetGlyphHMetrics(&p->f->info, g, &adv, &lsb);
  *center = 0;
  float a = adv * p->scale;
  if (p->fmt->tabular && cp >= '0' && cp <= '9') { float t = p->f->zero_adv * p->scale; *center = (t - a) / 2; a = t; }
  *gx = p->x + *center;
  p->x += a + p->fmt->tracking * p->fmt->size;
  p->prev = g;
  return g;
}

float rk__text_width(const char *s, int len, const rk_textfmt *f) {
  font *fo = font_for(f->weight);
  if (!fo || !s) return 0;
  if (len < 0) len = (int)strlen(s);
  pen p = { fo, f->size / fo->upem, 0, -1, f };
  for (int i = 0; i < len;) { float gx, c; pen_step(&p, utf8(s, len, &i), &gx, &c); }
  if (len) p.x -= f->tracking * f->size;
  return p.x;
}

/* The CSS line box: the content (ascent + descent) centred in size × line height, half the leading above. */
float rk__text_ascent(const rk_textfmt *f) {
  font *fo = font_for(f->weight);
  if (!fo) return f->size * 0.8f;
  float content = (fo->ascent + fo->descent) * f->size;
  return (f->size * f->line - content) / 2 + fo->ascent * f->size;
}

/* Breaks at spaces (and after hyphens) to fit max_width; a word longer than the line stands on its own. */
int rk__text_wrap(const char *s, const rk_textfmt *f, float max_width, rk_line *lines, int max) {
  int len = (int)strlen(s), n = 0, start = 0;
  if (!len) return 0;
  while (start < len && n < max) {
    int nl = start;
    while (nl < len && s[nl] != '\n') nl++;
    if (max_width <= 0 || rk__text_width(s + start, nl - start, f) <= max_width) {
      lines[n].start = start; lines[n].len = nl - start; lines[n].width = rk__text_width(s + start, nl - start, f); n++;
      start = nl + 1;
      continue;
    }
    int brk = -1, i = start;
    while (i < nl) {
      int j = i;
      while (j < nl && s[j] != ' ' && s[j] != '-') j++;
      int end = j < nl && s[j] == '-' ? j + 1 : j;
      if (rk__text_width(s + start, end - start, f) > max_width && brk > 0) break;
      brk = end;
      i = j + 1;
    }
    if (brk <= start) brk = nl;
    int e = brk;
    while (e > start && s[e - 1] == ' ') e--;
    lines[n].start = start; lines[n].len = e - start; lines[n].width = rk__text_width(s + start, e - start, f); n++;
    start = brk;
    while (start < len && s[start] == ' ') start++;
  }
  return n;
}

/* Glyph cache ----------------------------------------------------------------------------------------------------- */

typedef struct glyph { uint32_t key0, key1; int w, h, x, y; uint8_t *px; } glyph;
#define CACHE 4096
static glyph cache[CACHE];
static int cached;

static void cache_clear(void) {
  for (int i = 0; i < CACHE; i++) { free(cache[i].px); cache[i].px = NULL; cache[i].key0 = cache[i].key1 = 0; }
  cached = 0;
}

void rk__text_shutdown(void) { cache_clear(); }

/* Coverage is lifted slightly, as browsers do, so thin strokes keep their weight on a light ground. */
static uint8_t gamma_lift[256];

static glyph *glyph_get(font *f, int g, float px_size, int sub) {
  uint32_t size_q = (uint32_t)(px_size * 4 + 0.5f);
  uint32_t k0 = ((uint32_t)(f - fonts) << 28) | (size_q << 2) | (uint32_t)sub | 0x08000000u, k1 = (uint32_t)g + 1;
  uint32_t h = (k0 * 2654435761u) ^ (k1 * 40503u);
  for (int probe = 0; probe < CACHE; probe++) {
    glyph *e = &cache[(h + probe) & (CACHE - 1)];
    if (e->key0 == k0 && e->key1 == k1) return e;
    if (!e->key0) {
      if (cached > CACHE * 3 / 4) { cache_clear(); return glyph_get(f, g, px_size, sub); }
      if (!gamma_lift[255]) for (int i = 0; i < 256; i++) gamma_lift[i] = (uint8_t)(255.0 * pow(i / 255.0, 0.82) + 0.5);
      float scale = px_size / f->upem, shift = sub / 4.0f;
      int x0, y0, x1, y1;
      stbtt_GetGlyphBitmapBoxSubpixel(&f->info, g, scale, scale, shift, 0, &x0, &y0, &x1, &y1);
      e->key0 = k0; e->key1 = k1; e->x = x0; e->y = y0; e->w = x1 - x0; e->h = y1 - y0;
      if (e->w > 0 && e->h > 0) {
        e->px = rk__alloc((size_t)e->w * e->h);
        stbtt_MakeGlyphBitmapSubpixel(&f->info, e->px, e->w, e->h, e->w, scale, scale, shift, 0, g);
        for (int i = 0; i < e->w * e->h; i++) e->px[i] = gamma_lift[e->px[i]];
      }
      cached++;
      return e;
    }
  }
  return NULL;
}

/* Drawing --------------------------------------------------------------------------------------------------------- */

void rk__text_draw(rk_canvas *c, const char *s, int len, float x, float top, const rk_textfmt *f, rk_color color) {
  font *fo = font_for(f->weight);
  if (!fo || !s) return;
  if (len < 0) len = (int)strlen(s);
  float k = c->scale, px_size = f->size * k;
  uint32_t pm = rk__pm(color, c->alpha);
  rk_irect clip = rk__canvas_clip(c);
  float base = c->oy + (top + rk__text_ascent(f)) * k;
  int by = (int)floorf(base + 0.5f);
  pen p = { fo, px_size / fo->upem, 0, -1, f };
  rk_textfmt dev = *f; dev.size = px_size;
  p.fmt = &dev;
  float x0 = c->ox + x * k;
  for (int i = 0; i < len;) {
    int cp = utf8(s, len, &i);
    float gx, center;
    int g = pen_step(&p, cp, &gx, &center);
    if (cp == ' ' || !g) continue;
    float at = x0 + gx;
    int ix = (int)floorf(at), sub = (int)((at - ix) * 4) & 3;
    glyph *e = glyph_get(fo, g, px_size, sub);
    if (e && e->px) rk__blit_mask(c->target, clip, ix + e->x, by + e->y, e->px, e->w, e->h, e->w, pm);
  }
}

rk_size rk_text_measure(const char *text, rk_text_style style, float max_width) {
  rk_textfmt f = rk__textfmt(style, 0);
  rk_line lines[256];
  int n = rk__text_wrap(text ? text : "", &f, max_width, lines, 256);
  float w = 0;
  for (int i = 0; i < n; i++) w = RK_MAX(w, lines[i].width);
  rk_size s = { w, RK_MAX(n, 1) * f.size * f.line };
  return s;
}

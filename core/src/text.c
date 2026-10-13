/* text.c: Barlow through stb_truetype. Glyphs are rasterized at the device size with quarter-pixel horizontal
   positions and cached; kerning comes from the font's GPOS pairs; text styles follow typography.md.

   A character Barlow lacks comes from another face: one the app added (rk_app_add_font), or one the system offers
   for it (fontfind.c), mapped once and kept. Which face draws a character is remembered, so the search runs once per
   character. Glyphs from another face sit on Barlow's baseline at the same em size. Lines break at spaces, after a
   hyphen, and between CJK characters - never before closing punctuation or after opening (kinsoku). Text is set
   left to right, a character at a time: scripts that join or reorder (Arabic, Hebrew, the Indic scripts) wait for
   shaping. */
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

typedef struct font {
  stbtt_fontinfo info; int weight; float ascent, descent, upem; int zero_adv; bool ok;
  const unsigned char *map; size_t map_size; char path[512]; int index;   /* another face: its mapped file */
} font;
#define FACES 32                                           /* Barlow's weights first, then the faces found or added */
static font fonts[FACES];
static int nfonts;
static bool fonts_ready;

static bool face_init(font *f, const unsigned char *data, int offset) {
  if (offset < 0 || !stbtt_InitFont(&f->info, data, offset)) return false;
  if (!stbtt__find_table((unsigned char *)data, (stbtt_uint32)offset, "glyf") && !stbtt__find_table((unsigned char *)data, (stbtt_uint32)offset, "CFF ")) return false;   /* bitmaps only (colour emoji): nothing to draw */
  int a, d, g;
  stbtt_GetFontVMetrics(&f->info, &a, &d, &g);
  f->upem = (float)ttUSHORT(f->info.data + f->info.head + 18);
  f->ascent = a / f->upem; f->descent = -d / f->upem;
  int adv, lsb;
  stbtt_GetCodepointHMetrics(&f->info, '0', &adv, &lsb);
  f->zero_adv = adv;
  f->ok = true;
  return true;
}

static void fonts_init(void) {
  if (fonts_ready) return;
  fonts_ready = true;
  for (int i = 0; i < RK_FONT_FILE_COUNT; i++) {
    fonts[i].weight = rk_font_files[i].weight;
    face_init(&fonts[i], rk_font_files[i].data, stbtt_GetFontOffsetForIndex(rk_font_files[i].data, 0));
  }
  nfonts = RK_FONT_FILE_COUNT;
}

static font *font_for(int weight) {
  fonts_init();
  font *best = NULL;
  for (int i = 0; i < RK_FONT_FILE_COUNT; i++)
    if (fonts[i].ok && (!best || abs(fonts[i].weight - weight) < abs(best->weight - weight))) best = &fonts[i];
  return best;
}

/* The face in a file: by index, or in a collection by its PostScript name. NULL when it cannot be drawn with. */
static font *face_open(const char *path, int index, const char *psname, int weight) {
  fonts_init();
  for (int i = RK_FONT_FILE_COUNT; i < nfonts; i++) if (!strcmp(fonts[i].path, path) && fonts[i].index == index) return fonts[i].ok ? &fonts[i] : NULL;
  if (nfonts == FACES) return NULL;
  font *f = &fonts[nfonts];
  memset(f, 0, sizeof *f);
  snprintf(f->path, sizeof f->path, "%s", path);
  f->index = index; f->weight = weight;
  f->map = rk__file_map(path, &f->map_size);
  nfonts++;                                                /* tried, drawable or not: never opened twice */
  if (!f->map) return NULL;
  if (psname && *psname) {
    for (int k = 0, n = stbtt_GetNumberOfFonts(f->map); k < n; k++) {
      stbtt_fontinfo probe;
      int len = 0;
      if (!stbtt_InitFont(&probe, f->map, stbtt_GetFontOffsetForIndex(f->map, k))) continue;
      const char *name = stbtt_GetFontNameString(&probe, &len, 1, 0, 0, 6);   /* Macintosh Roman: plain bytes */
      if (name && len == (int)strlen(psname) && !memcmp(name, psname, (size_t)len)) { index = f->index = k; break; }
    }
  }
  if (!face_init(f, f->map, stbtt_GetFontOffsetForIndex(f->map, index))) { rk__file_unmap(f->map, f->map_size); f->map = NULL; return NULL; }
  return f;
}

bool rk_app_add_font(const char *path) {
  if (!path) return false;
  int at = nfonts;
  font *f = face_open(path, 0, NULL, 0);                 /* weight 0: for every weight */
  if (f && f - fonts == at) {                              /* new: before the system's, so the app's font wins */
    font mine = *f;
    memmove(&fonts[RK_FONT_FILE_COUNT + 1], &fonts[RK_FONT_FILE_COUNT], (size_t)(at - RK_FONT_FILE_COUNT) * sizeof *fonts);
    fonts[RK_FONT_FILE_COUNT] = mine;                      /* each info points into its own map, which does not move */
  }
  rk__text_forget();
  return f != NULL;
}

/* Which face draws a code point at a weight: remembered (cp, weight bucket) -> face, or -1 for none. */
typedef struct cp_face { uint32_t key; int16_t face; } cp_face;
#define CP_CACHE 4096
static cp_face cp_cache[CP_CACHE];
static int cp_cached;

static int face_has(const font *f, int cp) { return f && f->ok ? stbtt_FindGlyphIndex(&f->info, cp) : 0; }

static font *fallback_for(int cp, int weight, int *glyph) {
  uint32_t key = ((uint32_t)cp << 3 | (uint32_t)(RK_CLAMP(weight / 100, 1, 9) - 1)) + 1, h = key * 2654435761u;
  for (int probe = 0; probe < CP_CACHE; probe++) {
    cp_face *e = &cp_cache[(h + (uint32_t)probe) & (CP_CACHE - 1)];
    if (e->key == key) { *glyph = e->face >= 0 ? face_has(&fonts[e->face], cp) : 0; return e->face >= 0 ? &fonts[e->face] : NULL; }
    if (e->key) continue;
    if (cp_cached > CP_CACHE * 3 / 4) { memset(cp_cache, 0, sizeof cp_cache); cp_cached = 0; return fallback_for(cp, weight, glyph); }
    font *found = NULL;
    for (int i = RK_FONT_FILE_COUNT; i < nfonts && !found; i++) if (face_has(&fonts[i], cp) && (!fonts[i].weight || abs(fonts[i].weight - weight) < 150)) found = &fonts[i];
    if (!found) {                                          /* the system's candidates, opened in turn */
      rk_font_ref refs[12];
      int n = rk__font_candidates(cp, weight, refs, 12);
      for (int i = 0; i < n && !found; i++) { font *f = face_open(refs[i].path, refs[i].index, refs[i].psname, weight); if (face_has(f, cp)) found = f; }
    }
    for (int i = RK_FONT_FILE_COUNT; i < nfonts && !found; i++) if (face_has(&fonts[i], cp)) found = &fonts[i];   /* any weight */
    e->key = key; e->face = found ? (int16_t)(found - fonts) : -1;
    cp_cached++;
    *glyph = face_has(found, cp);
    return found;
  }
  *glyph = 0;
  return NULL;
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

typedef struct pen { font *f; float size; float x; int prev; font *prev_face; const rk_textfmt *fmt; } pen;

/* Advances the pen over one code point; returns the glyph, the face that has it, and where it starts. */
static int pen_step(pen *p, int cp, float *gx, float *center, font **face) {
  font *f = p->f;
  int g = stbtt_FindGlyphIndex(&f->info, cp);
  if (!g && cp >= 0x20) { int fg = 0; font *other = fallback_for(cp, p->fmt->weight, &fg); if (other) { f = other; g = fg; } }
  float scale = p->size / f->upem;
  if (p->prev >= 0 && g && p->prev_face == f) p->x += stbtt_GetGlyphKernAdvance(&f->info, p->prev, g) * scale;
  int adv, lsb;
  stbtt_GetGlyphHMetrics(&f->info, g, &adv, &lsb);
  *center = 0;
  float a = adv * scale;
  if (p->fmt->tabular && cp >= '0' && cp <= '9' && f == p->f) { float t = f->zero_adv * scale; *center = (t - a) / 2; a = t; }
  *gx = p->x + *center;
  p->x += a + p->fmt->tracking * p->fmt->size;
  p->prev = g; p->prev_face = f;
  *face = f;
  return g;
}

float rk__text_width(const char *s, int len, const rk_textfmt *f) {
  font *fo = font_for(f->weight);
  if (!fo || !s) return 0;
  if (len < 0) len = (int)strlen(s);
  pen p = { fo, f->size, 0, -1, NULL, f };
  for (int i = 0; i < len;) { float gx, c; font *face; pen_step(&p, utf8(s, len, &i), &gx, &c, &face); }
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

/* Where a line may break. CJK characters break between each other, but not before closing punctuation (and the small
   kana) or after opening; elsewhere a line breaks after spaces (which stay on the line they end) or a hyphen. */
static bool cjk_wide(int c) {
  return (c >= 0x1100 && c <= 0x11ff) || (c >= 0x2e80 && c <= 0x9fff) || (c >= 0xac00 && c <= 0xd7af) || (c >= 0xf900 && c <= 0xfaff) ||
         (c >= 0xfe30 && c <= 0xfe4f) || (c >= 0xff00 && c <= 0xff60) || (c >= 0x20000 && c <= 0x3ffff);
}
static bool in_list(int c, const int *list, int n) { for (int i = 0; i < n; i++) if (list[i] == c) return true; return false; }
static bool closes(int c) {
  static const int list[] = { ')', ']', '}', ',', '.', ':', ';', '!', '?', 0x3001, 0x3002, 0xff0c, 0xff0e, 0xff1a, 0xff1b, 0xff1f, 0xff01, 0x30fb, 0x30fc,
                              0xff09, 0x300d, 0x300f, 0x3011, 0x3015, 0x3009, 0x300b, 0x3019, 0x3017, 0xff5d, 0xff3d, 0x3041, 0x3043, 0x3045, 0x3047,
                              0x3049, 0x3063, 0x3083, 0x3085, 0x3087, 0x308e, 0x30a1, 0x30a3, 0x30a5, 0x30a7, 0x30a9, 0x30c3, 0x30e3, 0x30e5, 0x30e7,
                              0x30ee, 0x30f5, 0x30f6, 0x309d, 0x309e, 0x30fd, 0x30fe, 0x3005 };
  return in_list(c, list, (int)(sizeof list / sizeof *list));
}
static bool opens(int c) {
  static const int list[] = { '(', '[', '{', 0xff08, 0x300c, 0x300e, 0x3010, 0x3014, 0x3008, 0x300a, 0x3018, 0x3016, 0xff5b, 0xff3b };
  return in_list(c, list, (int)(sizeof list / sizeof *list));
}

int rk__text_next_break(const char *s, int from, int end) {
  int i = from, prev = 0;
  while (i < end) {
    if (s[i] == ' ') { while (i < end && s[i] == ' ') i++; return i; }
    int k = i, c = utf8(s, end, &k);
    if (i > from && (cjk_wide(c) || cjk_wide(prev)) && !closes(c) && !opens(prev)) return i;
    if (c == '-' && i > from) return k;
    prev = c; i = k;
  }
  return end;
}

/* Breaks where rk__text_next_break allows to fit max_width; a word longer than the line stands on its own. */
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
    int brk = -1;
    for (int i = start; i < nl;) {
      int end = rk__text_next_break(s, i, nl), vis = end;
      while (vis > start && s[vis - 1] == ' ') vis--;
      if (brk > 0 && rk__text_width(s + start, vis - start, f) > max_width) break;
      brk = end; i = end;
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
#define CACHE 16384                                        /* titles that change size in motion need many sizes */
static glyph cache[CACHE];
static int cached;

static void cache_clear(void) {
  for (int i = 0; i < CACHE; i++) { free(cache[i].px); cache[i].px = NULL; cache[i].key0 = cache[i].key1 = 0; }
  cached = 0;
}

/* The faces moved or changed: what was remembered about them goes. */
void rk__text_forget(void) { cache_clear(); memset(cp_cache, 0, sizeof cp_cache); cp_cached = 0; }

void rk__text_shutdown(void) {
  rk__text_forget();
  for (int i = RK_FONT_FILE_COUNT; i < nfonts; i++) rk__file_unmap(fonts[i].map, fonts[i].map_size);
  if (fonts_ready) nfonts = RK_FONT_FILE_COUNT;
}

/* Coverage is lifted slightly, as browsers do, so thin strokes keep their weight on a light ground. */
static uint8_t gamma_lift[256];

static glyph *glyph_get(font *f, int g, float px_size, int sub) {
  uint32_t size_q = (uint32_t)(px_size * 4 + 0.5f);
  uint32_t k0 = 0x80000000u | (size_q << 7) | ((uint32_t)(f - fonts) << 2) | (uint32_t)sub, k1 = (uint32_t)g + 1;
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
      rk__meter.glyphs++;
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
  rk_textfmt dev = *f; dev.size = px_size;
  pen p = { fo, px_size, 0, -1, NULL, &dev };
  float x0 = c->ox + x * k;
  for (int i = 0; i < len;) {
    int cp = utf8(s, len, &i);
    float gx, center;
    font *face;
    int g = pen_step(&p, cp, &gx, &center, &face);
    if (cp == ' ' || !g) continue;
    float at = x0 + gx;
    int ix = (int)floorf(at), sub = (int)((at - ix) * 4) & 3;
    glyph *e = glyph_get(face, g, px_size, sub);
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

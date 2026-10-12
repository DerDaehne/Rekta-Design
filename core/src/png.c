/* png.c: writes a bitmap as a PNG (straight RGBA). Deflate with fixed Huffman codes and a small LZ77 matcher: flat
   UI images shrink well, with no zlib. */
#include "rk_internal.h"

typedef struct bits { uint8_t *buf; int len, cap; uint32_t acc; int n; } bits;

static void put_byte(bits *b, uint8_t v) { b->buf = rk__grow(b->buf, &b->cap, b->len + 1, 1); b->buf[b->len++] = v; }
static void put_bits(bits *b, uint32_t v, int n) {               /* LSB first */
  b->acc |= v << b->n; b->n += n;
  while (b->n >= 8) { put_byte(b, (uint8_t)b->acc); b->acc >>= 8; b->n -= 8; }
}
static void put_rev(bits *b, uint32_t code, int n) {             /* Huffman codes go MSB first */
  uint32_t r = 0;
  for (int i = 0; i < n; i++) r |= ((code >> i) & 1) << (n - 1 - i);
  put_bits(b, r, n);
}
static void lit(bits *b, int v) {
  if (v < 144) put_rev(b, 0x30 + v, 8);
  else if (v < 256) put_rev(b, 0x190 + v - 144, 9);
  else if (v < 280) put_rev(b, v - 256, 7);
  else put_rev(b, 0xc0 + v - 280, 8);
}
static const int len_base[] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
static const int len_extra[] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
static const int dist_base[] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
static const int dist_extra[] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };

static void match(bits *b, int len, int dist) {
  int i = 28;
  while (len_base[i] > len) i--;
  lit(b, 257 + i); put_bits(b, (uint32_t)(len - len_base[i]), len_extra[i]);
  int d = 29;
  while (dist_base[d] > dist) d--;
  put_rev(b, (uint32_t)d, 5); put_bits(b, (uint32_t)(dist - dist_base[d]), dist_extra[d]);
}

static void deflate(bits *b, const uint8_t *in, int n) {
  enum { HASH = 1 << 15, WIN = 32768 };
  int *head = malloc(HASH * sizeof(int)), *prev = malloc((size_t)n * sizeof(int));
  if (!head || !prev) { free(head); free(prev); abort(); }
  for (int i = 0; i < HASH; i++) head[i] = -1;
  put_bits(b, 1, 1); put_bits(b, 1, 2);                         /* one final block, fixed codes */
  int i = 0;
  while (i < n) {
    int best = 0, bd = 0;
    if (i + 3 <= n) {
      uint32_t h = ((uint32_t)in[i] << 10 ^ (uint32_t)in[i + 1] << 5 ^ in[i + 2]) & (HASH - 1);
      int c = head[h], tries = 32;
      while (c >= 0 && i - c <= WIN && tries--) {
        int l = 0, max = RK_MIN(258, n - i);
        while (l < max && in[c + l] == in[i + l]) l++;
        if (l > best) { best = l; bd = i - c; if (l == max) break; }
        c = prev[c];
      }
      prev[i] = head[h]; head[h] = i;
    }
    if (best >= 3) {
      match(b, best, bd);
      for (int k = 1; k < best && i + k + 3 <= n; k++) {
        uint32_t h = ((uint32_t)in[i + k] << 10 ^ (uint32_t)in[i + k + 1] << 5 ^ in[i + k + 2]) & (HASH - 1);
        prev[i + k] = head[h]; head[h] = i + k;
      }
      i += best;
    } else { lit(b, in[i]); i++; }
  }
  lit(b, 256);
  if (b->n) put_bits(b, 0, 8 - b->n);
  free(head); free(prev);
}

static uint32_t crc_table[256];
static uint32_t crc(const uint8_t *p, int n, uint32_t c) {
  if (!crc_table[1]) for (uint32_t i = 0; i < 256; i++) { uint32_t v = i; for (int k = 0; k < 8; k++) v = v & 1 ? 0xedb88320u ^ (v >> 1) : v >> 1; crc_table[i] = v; }
  c = ~c;
  for (int i = 0; i < n; i++) c = crc_table[(c ^ p[i]) & 255] ^ (c >> 8);
  return ~c;
}

static void be32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }

static void chunk(FILE *f, const char *type, const uint8_t *data, int n) {
  uint8_t h[8];
  be32(h, (uint32_t)n); memcpy(h + 4, type, 4);
  fwrite(h, 1, 8, f);
  if (n) fwrite(data, 1, (size_t)n, f);
  uint32_t c = crc((const uint8_t *)type, 4, 0);
  c = crc(data, n, c);
  be32(h, c); fwrite(h, 1, 4, f);
}

bool rk__png_write(const rk_bitmap *bmp, const char *path) {
  int w = bmp->w, h = bmp->h, rowlen = w * 4 + 1;
  uint8_t *raw = rk__alloc((size_t)rowlen * h);
  for (int y = 0; y < h; y++) {
    uint8_t *o = raw + (size_t)y * rowlen;
    const uint32_t *s = bmp->px + (size_t)y * bmp->stride;
    o[0] = 1;                                                   /* Sub filter: flat runs become zeros */
    uint8_t last[4] = { 0, 0, 0, 0 };
    for (int x = 0; x < w; x++) {
      uint32_t p = s[x], a = p >> 24;
      uint8_t px[4] = { 0, 0, 0, (uint8_t)a };
      if (a) {
        px[0] = (uint8_t)RK_MIN(255u, (((p >> 16) & 255) * 255 + a / 2) / a);
        px[1] = (uint8_t)RK_MIN(255u, (((p >> 8) & 255) * 255 + a / 2) / a);
        px[2] = (uint8_t)RK_MIN(255u, ((p & 255) * 255 + a / 2) / a);
      }
      for (int k = 0; k < 4; k++) { o[1 + x * 4 + k] = (uint8_t)(px[k] - last[k]); last[k] = px[k]; }
    }
  }
  bits z = { 0 };
  put_byte(&z, 0x78); put_byte(&z, 0x01);
  deflate(&z, raw, rowlen * h);
  uint32_t s1 = 1, s2 = 0;
  for (int i = 0; i < rowlen * h; i++) { s1 = (s1 + raw[i]) % 65521; s2 = (s2 + s1) % 65521; }
  uint8_t ad[4]; be32(ad, (s2 << 16) | s1);
  for (int k = 0; k < 4; k++) put_byte(&z, ad[k]);
  free(raw);
  FILE *f = fopen(path, "wb");
  if (!f) { free(z.buf); return false; }
  static const uint8_t sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
  fwrite(sig, 1, 8, f);
  uint8_t ihdr[13];
  be32(ihdr, (uint32_t)w); be32(ihdr + 4, (uint32_t)h);
  ihdr[8] = 8; ihdr[9] = 6; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
  chunk(f, "IHDR", ihdr, 13);
  chunk(f, "IDAT", z.buf, z.len);
  chunk(f, "IEND", NULL, 0);
  free(z.buf);
  return fclose(f) == 0;
}

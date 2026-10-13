/* fontfind.c: where the system keeps a font for a character Barlow lacks, and mapping font files into memory.

   Linux asks fontconfig (opened at run time, so nothing more is linked; without it, the usual places are tried),
   macOS asks CoreText (opened the same way) for the font it would fall back to, Windows offers its fonts in an
   order that suits the user's language. Each answer is a list of candidates: text.c opens them in turn and takes the
   first that has the character. A file is mapped, not read, so a 20 MB CJK font costs the pages its glyphs touch.
   Included before text.c. */
#include "rk_internal.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#if !defined(RK_HEADLESS_ONLY)
#include <dlfcn.h>
#endif
#endif

int rk__utf8_put(unsigned char *out, int c) {
  if (c < 0x80) { out[0] = (unsigned char)c; return 1; }
  if (c < 0x800) { out[0] = (unsigned char)(0xc0 | c >> 6); out[1] = (unsigned char)(0x80 | (c & 63)); return 2; }
  if (c < 0x10000) { out[0] = (unsigned char)(0xe0 | c >> 12); out[1] = (unsigned char)(0x80 | (c >> 6 & 63)); out[2] = (unsigned char)(0x80 | (c & 63)); return 3; }
  out[0] = (unsigned char)(0xf0 | c >> 18); out[1] = (unsigned char)(0x80 | (c >> 12 & 63)); out[2] = (unsigned char)(0x80 | (c >> 6 & 63)); out[3] = (unsigned char)(0x80 | (c & 63));
  return 4;
}

/* Mapping ---------------------------------------------------------------------------------------------------------- */

const unsigned char *rk__font_map(const char *path, size_t *size) {
  *size = 0;
#if defined(_WIN32)
  wchar_t wpath[512];
  if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, 512)) return NULL;
  HANDLE f = CreateFileW(wpath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (f == INVALID_HANDLE_VALUE) return NULL;
  LARGE_INTEGER n;
  HANDLE m = GetFileSizeEx(f, &n) && n.QuadPart > 0 ? CreateFileMappingW(f, NULL, PAGE_READONLY, 0, 0, NULL) : NULL;
  const unsigned char *p = m ? MapViewOfFile(m, FILE_MAP_READ, 0, 0, 0) : NULL;
  if (m) CloseHandle(m);                                       /* the view keeps the mapping */
  CloseHandle(f);
  if (p) *size = (size_t)n.QuadPart;
  return p;
#else
  int fd = open(path, O_RDONLY);
  if (fd < 0) return NULL;
  struct stat st;
  void *p = fstat(fd, &st) == 0 && st.st_size > 0 ? mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0) : MAP_FAILED;
  close(fd);
  if (p == MAP_FAILED) return NULL;
  *size = (size_t)st.st_size;
  return p;
#endif
}

void rk__font_unmap(const unsigned char *p, size_t size) {
  if (!p) return;
#if defined(_WIN32)
  (void)size;
  UnmapViewOfFile(p);
#else
  munmap((void *)p, size);
#endif
}

/* Candidates ------------------------------------------------------------------------------------------------------- */

static int add_ref(rk_font_ref *out, int n, int max, const char *path, int index, const char *psname) {
  if (n >= max || !path || strlen(path) >= sizeof out->path) return n;
  for (int i = 0; i < n; i++) if (!strcmp(out[i].path, path) && out[i].index == index) return n;
  snprintf(out[n].path, sizeof out[n].path, "%s", path);
  out[n].index = index;
  snprintf(out[n].psname, sizeof out[n].psname, "%s", psname ? psname : "");
  return n + 1;
}

#if defined(_WIN32)

/* The fonts every Windows since 7 or 10 has, the user's own CJK first: Han characters look different in each. */
static int candidates(int cp, int weight, rk_font_ref *out, int max) {
  (void)cp; (void)weight;
  wchar_t wdir[MAX_PATH];
  char dir[MAX_PATH * 3];
  UINT len = GetWindowsDirectoryW(wdir, MAX_PATH);
  if (!len || !WideCharToMultiByte(CP_UTF8, 0, wdir, -1, dir, (int)sizeof dir, NULL, NULL)) return 0;
  wchar_t wlocale[LOCALE_NAME_MAX_LENGTH] = L"";
  GetUserDefaultLocaleName(wlocale, LOCALE_NAME_MAX_LENGTH);
  const char *cjk_ja[] = { "YuGothM.ttc", "meiryo.ttc", "msgothic.ttc" }, *cjk_ko[] = { "malgun.ttf" }, *cjk_tc[] = { "msjh.ttc" }, *cjk_sc[] = { "msyh.ttc", "simsun.ttc" };
  const char **first = cjk_sc;
  int nfirst = 2;
  if (!wcsncmp(wlocale, L"ja", 2)) { first = cjk_ja; nfirst = 3; }
  else if (!wcsncmp(wlocale, L"ko", 2)) { first = cjk_ko; nfirst = 1; }
  else if (!wcsncmp(wlocale, L"zh-TW", 5) || !wcsncmp(wlocale, L"zh-HK", 5)) { first = cjk_tc; nfirst = 1; }
  const char *rest[] = { "segoeui.ttf", "seguisym.ttf", "Nirmala.ttf", "malgun.ttf", "msyh.ttc", "YuGothM.ttc", "meiryo.ttc", "msjh.ttc",
                         "simsun.ttc", "arial.ttf", "seguiemj.ttf", "ebrima.ttf", "gadugi.ttf", "himalaya.ttf", "mmrtext.ttf", "taile.ttf" };
  char path[MAX_PATH * 3 + 32];
  int n = 0;
  for (int i = 0; i < nfirst; i++) { snprintf(path, sizeof path, "%s\\Fonts\\%s", dir, first[i]); n = add_ref(out, n, max, path, 0, NULL); }
  for (size_t i = 0; i < sizeof rest / sizeof *rest; i++) { snprintf(path, sizeof path, "%s\\Fonts\\%s", dir, rest[i]); n = add_ref(out, n, max, path, 0, NULL); }
  return n;
}

#elif defined(__APPLE__) && !defined(RK_HEADLESS_ONLY)

/* CoreText's own fallback: the font it would draw the character with, by its file and PostScript name. */
typedef struct { long location, length; } cf_range;
static void *ct, *cf;
static void *(*pCTFontCreateWithName)(void *, double, const void *);
static void *(*pCTFontCreateForString)(void *, void *, cf_range);
static void *(*pCTFontCopyAttribute)(void *, void *);
static void *(*pCTFontCopyPostScriptName)(void *);
static void **pkCTFontURLAttribute;
static void *(*pCFStringCreateWithBytes)(void *, const unsigned char *, long, unsigned, unsigned char);
static unsigned char (*pCFStringGetCString)(void *, char *, long, unsigned);
static unsigned char (*pCFURLGetFileSystemRepresentation)(void *, unsigned char, unsigned char *, long);
static void (*pCFRelease)(const void *);
#define UTF8_ENCODING 0x08000100u

static bool coretext(void) {
  static int tried;
  if (tried) return ct && cf;
  tried = 1;
  ct = dlopen("/System/Library/Frameworks/CoreText.framework/CoreText", RTLD_LAZY);
  cf = dlopen("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation", RTLD_LAZY);
  if (!ct || !cf) return false;
  *(void **)&pCTFontCreateWithName = dlsym(ct, "CTFontCreateWithName");
  *(void **)&pCTFontCreateForString = dlsym(ct, "CTFontCreateForString");
  *(void **)&pCTFontCopyAttribute = dlsym(ct, "CTFontCopyAttribute");
  *(void **)&pCTFontCopyPostScriptName = dlsym(ct, "CTFontCopyPostScriptName");
  pkCTFontURLAttribute = dlsym(ct, "kCTFontURLAttribute");
  *(void **)&pCFStringCreateWithBytes = dlsym(cf, "CFStringCreateWithBytes");
  *(void **)&pCFStringGetCString = dlsym(cf, "CFStringGetCString");
  *(void **)&pCFURLGetFileSystemRepresentation = dlsym(cf, "CFURLGetFileSystemRepresentation");
  *(void **)&pCFRelease = dlsym(cf, "CFRelease");
  if (!pCTFontCreateWithName || !pCTFontCreateForString || !pCTFontCopyAttribute || !pCTFontCopyPostScriptName || !pkCTFontURLAttribute ||
      !pCFStringCreateWithBytes || !pCFStringGetCString || !pCFURLGetFileSystemRepresentation || !pCFRelease) ct = NULL;
  return ct && cf;
}

static int candidates(int cp, int weight, rk_font_ref *out, int max) {
  int n = 0;
  if (coretext()) {
    unsigned char u[4];
    int len = rk__utf8_put(u, cp);
    const char *base_name = weight >= 600 ? "Helvetica-Bold" : "Helvetica";
    void *base_s = pCFStringCreateWithBytes(NULL, (const unsigned char *)base_name, (long)strlen(base_name), UTF8_ENCODING, 0);
    void *s = pCFStringCreateWithBytes(NULL, u, len, UTF8_ENCODING, 0);
    void *base = base_s ? pCTFontCreateWithName(base_s, 16, NULL) : NULL;
    if (base && s) {
      cf_range all = { 0, len == 4 ? 2 : 1 };                   /* UTF-16 units */
      void *font = pCTFontCreateForString(base, s, all);
      if (font) {
        void *url = pCTFontCopyAttribute(font, *pkCTFontURLAttribute), *ps = pCTFontCopyPostScriptName(font);
        unsigned char path[1024];
        char psname[128] = "";
        if (ps) pCFStringGetCString(ps, psname, sizeof psname, UTF8_ENCODING);
        if (url && pCFURLGetFileSystemRepresentation(url, 1, path, sizeof path)) n = add_ref(out, n, max, (const char *)path, 0, psname);
        if (url) pCFRelease(url);
        if (ps) pCFRelease(ps);
        pCFRelease(font);
      }
    }
    if (base) pCFRelease(base);
    if (base_s) pCFRelease(base_s);
    if (s) pCFRelease(s);
  }
  static const char *const known[] = { "/System/Library/Fonts/Supplemental/Arial Unicode.ttf", "/System/Library/Fonts/Apple Symbols.ttf",
                                       "/System/Library/Fonts/Hiragino Sans GB.ttc", "/System/Library/Fonts/AppleSDGothicNeo.ttc" };
  for (size_t i = 0; i < sizeof known / sizeof *known; i++) n = add_ref(out, n, max, known[i], 0, NULL);
  return n;
}

#elif !defined(__APPLE__) && !defined(RK_HEADLESS_ONLY)

/* fontconfig: the fonts it would substitute for sans-serif at this weight, the user's language first, that have it. */
typedef void FcPattern, FcCharSet, FcConfig;
typedef struct { int nfont, sfont; FcPattern **fonts; } FcFontSet;
static void *fc;
static FcPattern *(*pFcPatternCreate)(void);
static FcCharSet *(*pFcCharSetCreate)(void);
static int (*pFcCharSetAddChar)(FcCharSet *, unsigned);
static int (*pFcCharSetHasChar)(const FcCharSet *, unsigned);
static void (*pFcCharSetDestroy)(FcCharSet *);
static int (*pFcPatternAddCharSet)(FcPattern *, const char *, const FcCharSet *);
static int (*pFcPatternAddInteger)(FcPattern *, const char *, int);
static int (*pFcPatternAddString)(FcPattern *, const char *, const unsigned char *);
static int (*pFcConfigSubstitute)(FcConfig *, FcPattern *, int);
static void (*pFcDefaultSubstitute)(FcPattern *);
static FcFontSet *(*pFcFontSort)(FcConfig *, FcPattern *, int, FcCharSet **, int *);
static int (*pFcPatternGetString)(const FcPattern *, const char *, int, unsigned char **);
static int (*pFcPatternGetInteger)(const FcPattern *, const char *, int, int *);
static int (*pFcPatternGetCharSet)(const FcPattern *, const char *, int, FcCharSet **);
static void (*pFcFontSetDestroy)(FcFontSet *);
static void (*pFcPatternDestroy)(FcPattern *);

static bool fontconfig(void) {
  static int tried;
  if (tried) return fc != NULL;
  tried = 1;
  fc = dlopen("libfontconfig.so.1", RTLD_LAZY);
  if (!fc) return false;
  struct { void **fn; const char *name; } syms[] = {
    { (void **)&pFcPatternCreate, "FcPatternCreate" }, { (void **)&pFcCharSetCreate, "FcCharSetCreate" },
    { (void **)&pFcCharSetAddChar, "FcCharSetAddChar" }, { (void **)&pFcCharSetHasChar, "FcCharSetHasChar" },
    { (void **)&pFcCharSetDestroy, "FcCharSetDestroy" }, { (void **)&pFcPatternAddCharSet, "FcPatternAddCharSet" },
    { (void **)&pFcPatternAddInteger, "FcPatternAddInteger" }, { (void **)&pFcPatternAddString, "FcPatternAddString" },
    { (void **)&pFcConfigSubstitute, "FcConfigSubstitute" }, { (void **)&pFcDefaultSubstitute, "FcDefaultSubstitute" },
    { (void **)&pFcFontSort, "FcFontSort" }, { (void **)&pFcPatternGetString, "FcPatternGetString" },
    { (void **)&pFcPatternGetInteger, "FcPatternGetInteger" }, { (void **)&pFcPatternGetCharSet, "FcPatternGetCharSet" },
    { (void **)&pFcFontSetDestroy, "FcFontSetDestroy" }, { (void **)&pFcPatternDestroy, "FcPatternDestroy" },
  };
  for (size_t i = 0; i < sizeof syms / sizeof *syms; i++) if (!(*syms[i].fn = dlsym(fc, syms[i].name))) { fc = NULL; return false; }
  return true;
}

static int candidates(int cp, int weight, rk_font_ref *out, int max) {
  int n = 0;
  if (fontconfig()) {
    FcPattern *pat = pFcPatternCreate();
    FcCharSet *cs = pFcCharSetCreate();
    pFcCharSetAddChar(cs, (unsigned)cp);
    pFcPatternAddString(pat, "family", (const unsigned char *)"sans-serif");
    pFcPatternAddCharSet(pat, "charset", cs);
    pFcPatternAddInteger(pat, "weight", weight >= 700 ? 200 : weight >= 600 ? 180 : weight >= 500 ? 100 : weight <= 300 ? 50 : 80);   /* fontconfig's scale */
    pFcConfigSubstitute(NULL, pat, 0 /* FcMatchPattern */);
    pFcDefaultSubstitute(pat);
    int result = 0;
    FcFontSet *set = pFcFontSort(NULL, pat, 1, NULL, &result);
    for (int i = 0; set && i < set->nfont && n < max; i++) {
      unsigned char *file = NULL;
      FcCharSet *has = NULL;
      int index = 0;
      if (pFcPatternGetString(set->fonts[i], "file", 0, &file) || !file) continue;
      if (!pFcPatternGetCharSet(set->fonts[i], "charset", 0, &has) && has && !pFcCharSetHasChar(has, (unsigned)cp)) continue;
      pFcPatternGetInteger(set->fonts[i], "index", 0, &index);
      n = add_ref(out, n, max, (const char *)file, index & 0xffff, NULL);   /* the high bits name a variable instance */
    }
    if (set) pFcFontSetDestroy(set);
    pFcPatternDestroy(pat);
    pFcCharSetDestroy(cs);
  }
  static const char *const known[] = {
    "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf", "/usr/share/fonts/noto/NotoSans-Regular.ttf",
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc", "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/google-noto-cjk/NotoSansCJK-Regular.ttc", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/TTF/DejaVuSans.ttf", "/usr/share/fonts/dejavu/DejaVuSans.ttf", "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",
  };
  for (size_t i = 0; i < sizeof known / sizeof *known; i++) n = add_ref(out, n, max, known[i], 0, NULL);
  return n;
}

#else

static int candidates(int cp, int weight, rk_font_ref *out, int max) { (void)cp; (void)weight; (void)out; (void)max; (void)add_ref; return 0; }

#endif

/* The system's candidates for a code point; none in a headless run, which draws the same everywhere. */
int rk__font_candidates(int cp, int weight, rk_font_ref *out, int max) {
  return rk__headless() ? 0 : candidates(cp, weight, out, max);
}

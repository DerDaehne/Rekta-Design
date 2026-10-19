/* app.c: the run loop, timers, posting from other threads, and the app's settings (theme, motion, density, accent).
   The loop sleeps until an event, a timer or a frame is due; while something moves it draws at 60 frames a second. */
#include "rk_internal.h"
#include "rk_tokens.h"

#if defined(_WIN32)
#include <windows.h>
static CRITICAL_SECTION post_lock;
#define LOCK_INIT() InitializeCriticalSection(&post_lock)
#define LOCK() EnterCriticalSection(&post_lock)
#define UNLOCK() LeaveCriticalSection(&post_lock)
#else
#include <pthread.h>
static pthread_mutex_t post_lock = PTHREAD_MUTEX_INITIALIZER;
#define LOCK_INIT() ((void)0)
#define LOCK() pthread_mutex_lock(&post_lock)
#define UNLOCK() pthread_mutex_unlock(&post_lock)
#endif

/* The main thread: the one rk_app_main runs on. Everything but rk_app_post is called there; a call from another
   thread is said (once) instead of corrupting the tree quietly. */
#if defined(_WIN32)
static DWORD main_thread;
static bool on_main(void) { return !main_thread || GetCurrentThreadId() == main_thread; }
static void set_main(void) { main_thread = GetCurrentThreadId(); }
#else
static pthread_t main_thread; static bool have_main;
static bool on_main(void) { return !have_main || pthread_equal(pthread_self(), main_thread); }
static void set_main(void) { main_thread = pthread_self(); have_main = true; }
#endif
void rk__main_thread_only(const char *fn) {
  static bool said;
  if (on_main() || said) return;
  said = true;
  rk__error("%s called from another thread: only rk_app_post may be (the main thread runs everything else)", fn);
}

typedef struct timer { uint32_t id; double due, every; void (*fn)(void *); void *user; bool quiet; } timer;   /* quiet: reaches no app code */
typedef struct post { void (*fn)(void *); void *user; } post;
typedef struct listener { uint32_t id; void (*fn)(void *); void *user; } listener;

static const rk_platform *plat;
static rk_app_desc app;
static bool running, launched, frame_wanted;
static double next_frame;                                   /* when the next frame is due while something moves */
static int exit_code;
static timer *timers; static int ntimers, tcap; static uint32_t next_id = 1;
static post *posts; static int nposts, pcap;
static listener *listeners; static int nlisteners, lcap;

const rk_platform *rk__platform(void) { return plat ? plat : &rk__platform_headless; }
bool rk__headless(void) { return !strcmp(rk__platform()->name, "headless"); }   /* also a build without a windowing system */
void rk__app_request_frame(void) { frame_wanted = true; }
double rk_app_now(void) { return rk__platform()->now(); }

/* Timers ---------------------------------------------------------------------------------------------------------- */

/* Timers and change listeners share one run of ids, so rk_app_cancel ends either. */
static uint32_t new_id(void) { uint32_t id = next_id++; if (!next_id) next_id = 1; return id; }

static uint32_t add_timer(double ms, bool repeat, bool quiet, void (*fn)(void *), void *user) {
  timers = rk__grow(timers, &tcap, ntimers + 1, sizeof *timers);
  timer *t = &timers[ntimers++];
  t->id = new_id();
  t->due = rk_app_now() + RK_MAX(0.0, ms); t->every = repeat ? RK_MAX(1.0, ms) : 0;
  t->fn = fn; t->user = user; t->quiet = quiet;
  return t->id;
}
uint32_t rk_app_after(double ms, void (*fn)(void *), void *user) { return add_timer(ms, false, false, fn, user); }
uint32_t rk_app_every(double ms, void (*fn)(void *), void *user) { return add_timer(ms, true, false, fn, user); }
/* the library's own timers that reach no app code (a caret's blink, a tooltip's wait): declared views stay as built */
uint32_t rk__after_quiet(double ms, bool repeat, void (*fn)(void *), void *user) { return add_timer(ms, repeat, true, fn, user); }

bool rk_app_notify(const char *title, const char *body) {
  const rk_platform *p = rk__platform();
  return title && *title && p->notify && p->notify(&app, title, body ? body : "");
}

void rk_app_cancel(uint32_t id) {
  for (int i = 0; i < ntimers; i++) if (timers[i].id == id) { timers[i] = timers[--ntimers]; return; }
  for (int i = 0; i < nlisteners; i++) if (listeners[i].id == id) { memmove(listeners + i, listeners + i + 1, (size_t)(nlisteners - i - 1) * sizeof *listeners); nlisteners--; return; }
}

static void run_timers(double now) {
  for (;;) {
    int at = -1;
    for (int i = 0; i < ntimers; i++) if (timers[i].due <= now && (at < 0 || timers[i].due < timers[at].due)) at = i;
    if (at < 0) return;
    timer t = timers[at];
    if (t.every > 0) timers[at].due = RK_MAX(t.due + t.every, now);
    else timers[at] = timers[--ntimers];
    t.fn(t.user);
    if (!t.quiet) { rk__ui_dirty(); rk__ui_settle(); }     /* what is declared follows what the timer changed */
  }
}

static double next_due(void) {
  double d = INFINITY;
  for (int i = 0; i < ntimers; i++) d = RK_MIN(d, timers[i].due);
  return d;
}

/* Posting --------------------------------------------------------------------------------------------------------- */

void rk_app_post(void (*fn)(void *), void *user) {
  LOCK();
  posts = rk__grow(posts, &pcap, nposts + 1, sizeof *posts);
  posts[nposts].fn = fn; posts[nposts].user = user; nposts++;
  UNLOCK();
  rk__platform()->wake();
}

static void run_posts(void) {
  for (;;) {
    LOCK();
    if (!nposts) { UNLOCK(); return; }
    post p = posts[0];
    memmove(posts, posts + 1, (size_t)(nposts - 1) * sizeof *posts);
    nposts--;
    UNLOCK();
    p.fn(p.user);
    rk__ui_dirty(); rk__ui_settle();
  }
}

/* Settings -------------------------------------------------------------------------------------------------------- */

static void changed(bool crossfade) {
  for (rk_window *w = rk__windows(); w; w = w->next) {
    w->needs_layout = true;
    rk__view_relayout_up(w->root);
    rk__window_invalidate(w);
    (void)crossfade;
  }
  for (int i = 0; i < nlisteners; i++) listeners[i].fn(listeners[i].user);
  rk__ui_dirty(); rk__ui_settle();                         /* what is declared may read the look (rk_app_is_dark) */
}

/* A change of the whole look cross-fades over duration-short, as the web's view transition. */
static void look_changes(void) {
  for (rk_window *w = rk__windows(); w; w = w->next) rk_view_transition(w->root, RK_TRANSITION_CROSSFADE);
}

void rk_app_set_theme(rk_theme theme) {
  if (rk__prefs.theme == theme) return;
  bool dark = rk__dark();
  rk__prefs.theme = theme;
  if (dark != rk__dark()) look_changes();
  changed(true);
}
bool rk_app_is_dark(void) { return rk__dark(); }
void rk_app_set_motion(rk_motion_pref m) { rk__prefs.motion = m; changed(false); }
bool rk_app_is_reduced_motion(void) { return rk__reduced(); }
void rk_app_set_density(rk_density d) { rk__prefs.density = d; changed(false); }
void rk_app_set_accent(rk_color light, rk_color dark) {
  look_changes();
  rk__prefs.accent[0] = light; rk__prefs.accent[1] = dark ? dark : light;
  changed(true);
}

/* A domain's colour code: set as asked, and held to the rules the web's build.mjs --check-product holds a product's
   CSS to - 3:1 on bg and surface, 0.09 apart under protanopia and deuteranopia from every other code. What breaks
   one is said, not refused: the product sees it the first time it runs. */
static void series_check(int code) {
  static const char *const theme[2] = { "light", "dark" }, *const kind[3] = { "", "protanopia", "deuteranopia" };
  static const rk_role ground[2] = { RK_ROLE_BG, RK_ROLE_SURFACE };
  for (int t = 0; t < 2; t++) {
    rk_color c = rk__prefs.series[code - 1][t];
    if (!c) continue;
    for (int g = 0; g < 2; g++) {
      double r = rk__contrast(c, rk_tok_colors[t][ground[g]]);
      if (r < 3) rk__warn("rk_app_set_series: code %d (#%06x) on %s is %.2f:1 in the %s theme, needs 3:1", code, (unsigned)(c >> 8), g ? "surface" : "bg", r, theme[t]);
    }
    for (int o = 1; o <= 8; o++) {
      rk_color d = rk__prefs.series[o - 1][t];
      if (o == code || !d) continue;
      for (int k = 1; k <= 2; k++) {
        double apart = rk__distance(c, d, k);
        if (apart < 0.09) rk__warn("rk_app_set_series: codes %d and %d are %.3f apart under %s in the %s theme, need 0.09", code, o, apart, kind[k], theme[t]);
      }
    }
  }
}
void rk_app_set_series(int code, rk_color light, rk_color dark) {
  if (code < 1 || code > 8) { rk__error("rk_app_set_series: code %d is outside 1 to 8", code); return; }
  rk__prefs.series[code - 1][0] = light; rk__prefs.series[code - 1][1] = dark ? dark : light;
  series_check(code);
  changed(false);
}

/* What a product may set (docs/platforms.md): the rest is the design's, said instead of set. */
void rk_app_set_metric(rk_metric metric, float px) {
  if (metric != RK_TILE_UNIT && metric != RK_PIVOT_WIDE) { rk__error("rk_app_set_metric: only RK_TILE_UNIT and RK_PIVOT_WIDE are a product's to set"); return; }
  if (metric == RK_TILE_UNIT && px == 0) { rk__error("rk_app_set_metric: a tile unit of 0"); return; }
  rk__prefs.metric_set[metric] = px >= 0; rk__prefs.metric[metric] = px;
  changed(false);
}
void rk_app_set_timing(rk_duration timing, double ms) {
  if (timing != RK_TIMING_TOAST && timing != RK_TIMING_STALE) { rk__error("rk_app_set_timing: only RK_TIMING_TOAST and RK_TIMING_STALE are a product's to set"); return; }
  rk__prefs.timing_set[timing] = ms >= 0; rk__prefs.timing[timing] = ms;
  changed(false);
}

/* The words: the English ones, and the app's over them. Every word the library says comes from here; test/words.sh
   fails the tests when one is written anywhere else. */
static int english_plural(long n) { return n == 1 ? RK_PLURAL_ONE : RK_PLURAL_OTHER; }
/* rk-strings-begin */
static const rk_strings english = {
  sizeof(rk_strings), "undo", "redo", "Undone: %s", "ok", "cancel", "choose one", "back", "move %s", "on", "off",
  { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" },
  { "Mo", "Tu", "We", "Th", "Fr", "Sa", "Su" },
  { "idle", "active", "done", "waiting", "failed" },
  { NULL, "%s item", NULL, NULL, NULL, "%s items" },
  "minimize", "maximize", "close", "files", ".", ",", "%sK", "%sM", "%sB", "%sT", 1, english_plural,
};
/* rk-strings-end */
static rk_strings words;                                   /* the app's, copied; NULL fields: the English word */
/* the words are every field from undo to group_min (a word added after plural, as rk_strings grows, is a second run) */
#define WORD_COUNT ((offsetof(rk_strings, group_min) - offsetof(rk_strings, undo)) / sizeof(const char *))
static const char **word_at(rk_strings *s, size_t i) { return (const char **)((char *)s + offsetof(rk_strings, undo)) + i; }
#define RK__PLURAL_COUNT (RK_PLURAL_OTHER + 1)

/* A format takes exactly one %s and no other conversion ("%%" is a percent sign). */
static bool one_s(const char *f) {
  int n = 0;
  for (; *f; f++) if (*f == '%') { if (f[1] == '%') f++; else if (f[1] == 's') { n++; f++; } else return false; }
  return n == 1;
}

const rk_strings *rk__words(void) {
  static rk_strings now;
  now = english;
  for (size_t i = 0; i < WORD_COUNT; i++) if (*word_at(&words, i)) *word_at(&now, i) = *word_at(&words, i);
  bool forms = false;                                      /* a counting word the app gives is its own in every form */
  for (int f = 0; f < RK__PLURAL_COUNT; f++) forms |= words.items[f] != NULL;
  if (forms) memcpy(now.items, words.items, sizeof now.items);
  if (!now.items[RK_PLURAL_OTHER]) now.items[RK_PLURAL_OTHER] = english.items[RK_PLURAL_OTHER];
  if (words.group_min > 0) now.group_min = words.group_min;
  if (words.plural) now.plural = words.plural;
  return &now;
}

/* Bytes into out while they fit, counted either way: what snprintf returns. */
static void put(char *out, size_t size, size_t *n, const char *s, size_t len) {
  for (size_t i = 0; i < len; i++, (*n)++) if (*n + 1 < size) out[*n] = s[i];
}
static size_t put_end(char *out, size_t size, size_t n) { if (size) out[RK_MIN(n, size - 1)] = 0; return n; }

/* The format's %s replaced by arg, by hand: the format is the app's, never handed to printf. */
static size_t say(char *out, size_t size, const char *format, const char *arg) {
  size_t n = 0;
  bool used = false;
  for (const char *f = format; *f; f++) {
    if (f[0] == '%' && f[1] == '%') { put(out, size, &n, "%", 1); f++; continue; }
    if (f[0] == '%' && f[1] == 's' && !used) { put(out, size, &n, arg ? arg : "", arg ? strlen(arg) : 0); used = true; f++; continue; }
    put(out, size, &n, f, 1);
  }
  return put_end(out, size, n);
}
void rk__say(char *out, size_t size, const char *format, const char *arg) { say(out, size, format, arg); }

/* v in the app's digits: rounded to max decimals, the zeros at the end dropped down to min, with its decimal sign and
   digit groups. */
static size_t figure(char *out, size_t size, double v, int min, int max, const rk_strings *w) {
  char digits[400];
  snprintf(digits, sizeof digits, "%.*f", max, fabs(v));
  size_t ni = strspn(digits, "0123456789"), n = 0;
  const char *frac = digits + ni;
  while (*frac && (*frac < '0' || *frac > '9')) frac++;   /* past the C library's decimal point, whatever its locale */
  size_t nf = strlen(frac);
  while (nf > (size_t)min && frac[nf - 1] == '0') nf--;
  bool zero = strspn(digits, "0") == ni && strspn(frac, "0") == strlen(frac);
  if (v < 0 && !zero) put(out, size, &n, "-", 1);          /* never -0 */
  bool grouped = *w->group && ni >= (size_t)(3 + RK_MAX(1, w->group_min));
  for (size_t i = 0; i < ni; i++) {
    put(out, size, &n, digits + i, 1);
    if (grouped && i + 1 < ni && (ni - i - 1) % 3 == 0) put(out, size, &n, w->group, strlen(w->group));
  }
  if (nf) { put(out, size, &n, w->decimal, strlen(w->decimal)); put(out, size, &n, frac, nf); }
  return put_end(out, size, n);
}
size_t rk__figure(char *out, size_t size, double v, int digits) { return figure(out, size, v, digits, digits, rk__words()); }

/* As the web's number(): Intl.NumberFormat with two decimals at most, or its compact notation with one. */
size_t rk_format_number(char *out, size_t size, double v, bool compact) {
  const rk_strings *w = rk__words();
  if (!out) size = 0;
  if (!isfinite(v)) { size_t n = 0; const char *s = v != v ? "NaN" : v < 0 ? "-Infinity" : "Infinity"; put(out, size, &n, s, strlen(s)); return put_end(out, size, n); }
  if (!compact || fabs(v) < 1e3) return figure(out, size, v, 0, 2, w);
  const char *const forms[4] = { w->thousand, w->million, w->billion, w->trillion };
  double a = fabs(v);
  int e = a >= 1e12 ? 3 : a >= 1e9 ? 2 : a >= 1e6 ? 1 : 0;
  if (e < 3 && *forms[e] && round(a / pow(1e3, e + 1) * 10) >= 10000) e++;   /* 999,950 is 1M, not 1000K */
  if (!*forms[e]) return figure(out, size, v, 0, 1, w);   /* not shortened at this size */
  char num[512];
  figure(num, sizeof num, v / pow(1e3, e + 1), 0, 1, w);
  return say(out, size, forms[e], num);
}

/* A counting word for n: its form for n's plural category, or its other, with n's figure for the %s. */
void rk__count(char *out, size_t size, const char *const *forms, long n) {
  const rk_strings *w = rk__words();
  int cat = w->plural(n);
  if (cat < 0 || cat >= RK__PLURAL_COUNT || !forms[cat]) cat = RK_PLURAL_OTHER;
  char num[64];
  rk_format_number(num, sizeof num, (double)n, false);
  say(out, size, forms[cat], num);
}

void rk_app_set_strings(const rk_strings *strings) {
  for (size_t i = 0; i < WORD_COUNT; i++) free((void *)*word_at(&words, i));
  memset(&words, 0, sizeof words);
  if (strings && RK__SIZED(strings, "rk_app_set_strings", "rk_strings")) {
    rk_strings in;
    memset(&in, 0, sizeof in);
    memcpy(&in, strings, RK_MIN((size_t)strings->size, sizeof in));
    for (size_t i = 0; i < WORD_COUNT; i++) {
      const char **at = word_at(&in, i), *w = *at;
      bool compact = at >= &in.thousand && at <= &in.trillion;   /* "" too: not shortened at that size */
      bool format = at == &in.undone || at == &in.move || (at >= in.items && at < in.items + RK__PLURAL_COUNT) || (compact && w && *w);
      if (w && format && !one_s(w)) { rk__error("rk_app_set_strings: \"%s\" needs exactly one %%s: the English word stays", w); continue; }
      *word_at(&words, i) = w ? rk__strdup(w) : NULL;
    }
    words.group_min = in.group_min; words.plural = in.plural;
  }
  changed(false);
}

uint32_t rk_app_on_change(void (*fn)(void *), void *user) {
  listeners = rk__grow(listeners, &lcap, nlisteners + 1, sizeof *listeners);
  uint32_t id = new_id();
  listeners[nlisteners].id = id; listeners[nlisteners].fn = fn; listeners[nlisteners].user = user; nlisteners++;
  return id;
}

void rk__app_settings_changed(void) {
  bool dark = rk__dark();
  rk__platform()->settings(&rk__prefs.system_dark, &rk__prefs.system_reduced, &rk__prefs.system_compact);
  if (dark != rk__dark()) look_changes();
  changed(true);
}

/* The loop -------------------------------------------------------------------------------------------------------- */

/* The display's frame period: its refresh rate (RK_FPS overrides), 60 where it cannot be asked. */
static double period(void) {
  static double hz;
  if (!hz) {
    const char *e = getenv("RK_FPS");
    hz = e && atof(e) > 0 ? atof(e) : plat && plat->refresh_hz ? plat->refresh_hz() : 60;
    hz = RK_CLAMP(hz, 24.0, 480.0);
  }
  return 1000.0 / hz;
}

static bool ready(rk_window *w) { return !rk__platform()->ready || rk__platform()->ready(w->native); }

/* One turn of the loop: input and timers as they come, then a frame if one is due, then sleep until something
   happens. Frames come at the display's pace while something moves - where the compositor says when (Wayland), the
   moment it takes one, so motion is sampled for the frame that shows it; elsewhere on a steady grid - and at once
   when something changed while nothing moved. */
static void step(double max_wait) {
  const double P = period();
  static double last_tick;
  double now = rk_app_now();
  run_posts();
  run_timers(now);
  rk__anim_close_blocks();
  rk__ui_settle();                                         /* anything left to build, before the frame shows it */
  bool paced = rk__platform()->ready != NULL, any_ready = false;
  for (rk_window *w = rk__windows(); w; w = w->next) any_ready |= ready(w);
  /* paced: when a window can take a frame (or none could for a while: motion still moves on, unseen) */
  bool due = paced ? any_ready || now - last_tick > 100 : !rk__anim_running() || now >= next_frame - 0.5;
  if (due) {
    bool moving = rk__anim_tick(now);
    last_tick = now;
    for (rk_window *w = rk__windows(); w; w = w->next) if (ready(w)) rk__window_frame(w, now);
    next_frame = moving && next_frame + P > now ? next_frame + P : now + P;
  }
  if (!running) return;
  double t = rk_app_now(), wait;
  if (paced && (rk__anim_running() || frame_wanted)) {
    bool can = false;
    for (rk_window *w = rk__windows(); w; w = w->next) can |= ready(w) && (w->needs_draw || rk__anim_running());
    /* a window can take the next frame now; otherwise its frame callback wakes the loop (2 periods at most) */
    wait = RK_MIN(can && !due ? 0.0 : 2 * P, next_due() - t);
  } else if (rk__anim_running() || frame_wanted) wait = RK_MIN(RK_MAX(0.0, next_frame - t), next_due() - t);
  else wait = next_due() - t;
  frame_wanted = false;
  if (!isfinite(wait)) wait = -1;                          /* nothing due: until an event */
  else if (wait < 0) wait = 0;                             /* overdue (a frame took longer than the timer's gap): at once, never forever */
  if (wait < 0 && rk__headless()) wait = 1000.0 / 60.0;    /* headless never sleeps forever */
  if (max_wait >= 0 && (wait < 0 || wait > max_wait)) wait = max_wait;
  rk__platform()->wait(RK_MAX(wait, wait < 0 ? -1.0 : 0.0));
}

void rk__app_window_closed(rk_window *w) {
  (void)w;
  if (!rk__windows()) running = false;                     /* the last window closed: the app ends */
}

void rk_app_stats_reset(void) { rk__stats(NULL, true); }

void rk_app_stats(rk_stats *s) { rk__stats(s, false); }

void rk__stats(rk_stats *s, bool reset) {
  if (s) {
    rk_stats o;
    memset(&o, 0, sizeof o);
    o.size = sizeof o;
    const rk_meter *m = &rk__meter;
    o.frames = m->frames; o.animations = rk__anim_count(); o.views = (uint32_t)rk__live_views; o.glyphs = m->glyphs; o.slow = m->slow;
    o.draw_ms = m->draw_ms; o.last_ms = m->last_ms; o.max_ms = m->max_ms; o.pixels = m->pixels;
    o.latency_ms = m->latency_ms; o.max_latency_ms = m->max_latency_ms;
    if (RK__SIZED(s, "rk_app_stats", "rk_stats")) memcpy(s, &o, RK_MIN((size_t)s->size, sizeof o));
  }
  if (reset) { double at = rk__meter.input_at; memset(&rk__meter, 0, sizeof rk__meter); rk__meter.input_at = at; }
}

void rk_app_quit(int code) { exit_code = code; running = false; rk__platform()->wake(); }

void rk_app_run_for(double ms) {
  double end = rk_app_now() + ms;
  while (running && rk_app_now() < end - 1e-6) step(end - rk_app_now());
  if (running) step(0);
}

int rk_app_main(const rk_app_desc *desc) {
  memset(&app, 0, sizeof app);
  if (!desc || !RK__SIZED(desc, "rk_app_main", "rk_app_desc")) return 2;   /* nothing to launch: said, not waited on */
  set_main();
  memcpy(&app, desc, RK_MIN((size_t)desc->size, sizeof app));
  const char *env = getenv("RK_HEADLESS");
  LOCK_INIT();
  if (app.headless || (env && *env && *env != '0')) {
    plat = &rk__platform_headless;
    if (!plat->init(&app)) return 1;
  } else {
    /* Wayland first where it is built in and a compositor answers; RK_PLATFORM=x11 picks X11 instead */
    const char *want = getenv("RK_PLATFORM");
    plat = NULL;
#if defined(RK_WAYLAND)
    if (!(want && !strcmp(want, "x11")) && rk__platform_wayland.init(&app)) plat = &rk__platform_wayland;
#endif
    (void)want;
    if (!plat && rk__platform_native.init(&app)) plat = &rk__platform_native;
    if (!plat) { fprintf(stderr, "rekta: no display (%s); set RK_HEADLESS=1 to run without one\n", rk__platform_native.name); return 1; }
  }
  plat->settings(&rk__prefs.system_dark, &rk__prefs.system_reduced, &rk__prefs.system_compact);
  running = true; exit_code = 0;
  if (app.launch) app.launch(app.user);
  launched = true;
  int idle = 0;
  while (running) {
    step(-1);
    /* headless with nothing left to happen: done */
    if (rk__headless()) {
      if (!ntimers && !rk__anim_running() && !nposts) { if (++idle > 2) break; } else idle = 0;
    }
  }
  if (app.terminate) app.terminate(app.user);
  const char *prof = getenv("RK_PROFILE");
  const rk_meter *m = &rk__meter;
  if (prof && *prof == '1' && m->frames)
    fprintf(stderr, "rekta: %u frames, %.2f ms a frame on average, %.2f the slowest, %u over 8 ms, %.1f megapixels a frame, %u glyphs\n",
            m->frames, m->draw_ms / m->frames, m->max_ms, m->slow, m->pixels / 1e6 / m->frames, m->glyphs);
  while (rk__windows()) rk_window_close(rk__windows());
  plat->shutdown();
  rk__text_shutdown();
  free(timers); timers = NULL; ntimers = tcap = 0;     /* nothing stays for a later rk_app_main */
  free(listeners); listeners = NULL; nlisteners = lcap = 0;
  LOCK(); free(posts); posts = NULL; nposts = pcap = 0; UNLOCK();
  launched = false;
  return exit_code;
}

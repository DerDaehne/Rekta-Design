/* platform_headless.c: no screen. Windows live in memory, the clock is virtual and advances only as the loop waits,
   so a run is the same on every machine. RK_SCALE sets the pixel density (default 1); RK_THEME=dark the system
   theme; RK_REDUCED_MOTION=1 the system's motion setting. */
#include "rk_internal.h"

static double vclock;

static bool hl_init(const rk_app_desc *d) { (void)d; vclock = 0; return true; }
static void hl_shutdown(void) {}
static void *hl_open(rk_window *w, const rk_window_desc *d) {
  const char *s = getenv("RK_SCALE");
  float scale = s ? (float)atof(s) : 1;
  rk__window_resized(w, d->width, d->height, scale > 0 ? scale : 1);
  return w;
}
static void hl_close(void *n) { (void)n; }
static void hl_show(void *n) { (void)n; }
static void hl_title(void *n, const char *t) { (void)n; (void)t; }
static void hl_present(void *n, const rk_bitmap *fb, const rk_irect *r, int k) { (void)n; (void)fb; (void)r; (void)k; }
static void hl_nop(void *n) { (void)n; }
static bool hl_wait(double ms) { vclock += ms < 0 ? 1000.0 / 60.0 : ms; return true; }
static void hl_wake(void) {}
static double hl_now(void) { return vclock; }
static void hl_resize(void *n, float w, float h) { rk_window *win = n; rk__window_resized(win, w, h, win->scale); }
static void hl_settings(bool *dark, bool *reduced, bool *compact) {
  const char *t = getenv("RK_THEME"), *m = getenv("RK_REDUCED_MOTION");
  *dark = t && !strcmp(t, "dark");
  *reduced = m && *m == '1';
  *compact = true;                                             /* as a desktop with a mouse */
}

/* The headless chooser answers what a test set, a moment later, as a system's would. */
const char *const *rk__headless_choice; int rk__headless_nchoice;
typedef struct hl_ask { void (*done)(void *, const char *const *, int); void *ctx; } hl_ask;
static void hl_answer(void *user) { hl_ask *a = user; a->done(a->ctx, rk__headless_choice, rk__headless_nchoice); free(a); }
static bool hl_choose(void *native, const char *accept, bool multiple, void (*done)(void *, const char *const *, int), void *ctx) {
  (void)native; (void)accept; (void)multiple;
  hl_ask *a = malloc(sizeof *a);
  if (!a) return false;
  a->done = done; a->ctx = ctx;
  rk_app_after(1, false, hl_answer, a);
  return true;
}

char rk__headless_note[512];
static bool hl_notify(const rk_app_desc *app, const char *title, const char *body) {
  (void)app;
  snprintf(rk__headless_note, sizeof rk__headless_note, "%s: %s", title, body);
  return true;
}

const rk_platform rk__platform_headless = {
  "headless", hl_init, hl_shutdown, hl_open, hl_close, hl_show, hl_title, hl_present, hl_nop, hl_nop, hl_nop,
  hl_wait, hl_wake, hl_now, hl_settings, hl_resize, NULL, NULL, NULL, NULL, NULL, NULL, NULL, hl_choose, hl_notify
};

#if defined(RK_HEADLESS_ONLY)
const rk_platform rk__platform_native = {
  "headless", hl_init, hl_shutdown, hl_open, hl_close, hl_show, hl_title, hl_present, hl_nop, hl_nop, hl_nop,
  hl_wait, hl_wake, hl_now, hl_settings, hl_resize, NULL, NULL, NULL, NULL, NULL, NULL, NULL, hl_choose, hl_notify
};
#endif

/* app.c: the run loop, timers, posting from other threads, and the app's settings (theme, motion, density, accent).
   The loop sleeps until an event, a timer or a frame is due; while something moves it draws at 60 frames a second. */
#include "rk_internal.h"

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

typedef struct timer { uint32_t id; double due, every; void (*fn)(void *); void *user; } timer;
typedef struct post { void (*fn)(void *); void *user; } post;
typedef struct listener { void (*fn)(void *); void *user; } listener;

static const rk_platform *plat;
static rk_app_desc app;
static bool running, launched, frame_wanted;
static double next_frame;                                   /* when the next frame is due while something moves */
static int exit_code;
static timer *timers; static int ntimers, tcap; static uint32_t next_id = 1;
static post *posts; static int nposts, pcap;
static listener *listeners; static int nlisteners, lcap;

const rk_platform *rk__platform(void) { return plat ? plat : &rk__platform_headless; }
bool rk__headless(void) { return rk__platform() == &rk__platform_headless; }
void rk__app_request_frame(void) { frame_wanted = true; }
double rk_app_now(void) { return rk__platform()->now(); }

/* Timers ---------------------------------------------------------------------------------------------------------- */

uint32_t rk_app_after(double ms, bool repeat, void (*fn)(void *), void *user) {
  timers = rk__grow(timers, &tcap, ntimers + 1, sizeof *timers);
  timer *t = &timers[ntimers++];
  t->id = next_id++; if (!next_id) next_id = 1;
  t->due = rk_app_now() + RK_MAX(0.0, ms); t->every = repeat ? RK_MAX(1.0, ms) : 0;
  t->fn = fn; t->user = user;
  return t->id;
}

void rk_app_cancel(uint32_t id) {
  for (int i = 0; i < ntimers; i++) if (timers[i].id == id) { timers[i] = timers[--ntimers]; return; }
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
bool rk_app_reduced_motion(void) { return rk__reduced(); }
void rk_app_set_density(rk_density d) { rk__prefs.density = d; changed(false); }
void rk_app_set_accent(rk_color light, rk_color dark) {
  look_changes();
  rk__prefs.accent[0] = light; rk__prefs.accent[1] = dark ? dark : light;
  changed(true);
}
void rk_app_on_change(void (*fn)(void *), void *user) {
  listeners = rk__grow(listeners, &lcap, nlisteners + 1, sizeof *listeners);
  listeners[nlisteners].fn = fn; listeners[nlisteners].user = user; nlisteners++;
}

void rk__app_settings_changed(void) {
  bool dark = rk__dark();
  rk__platform()->settings(&rk__prefs.system_dark, &rk__prefs.system_reduced, &rk__prefs.system_compact);
  if (dark != rk__dark()) look_changes();
  changed(true);
}

/* The loop -------------------------------------------------------------------------------------------------------- */

static void step(double max_wait) {
  double now = rk_app_now();
  run_posts();
  run_timers(now);
  bool moving = rk__anim_tick(now);
  for (rk_window *w = rk__windows(); w; w = w->next) rk__window_frame(w, now);
  if (!running) return;
  /* while something moves, frames come at a steady 60 a second: wait until the next one is due, not a whole frame
     after this one was drawn */
  double wait;
  if (moving || frame_wanted) {
    double t = rk_app_now();
    next_frame = next_frame > t - 1000.0 / 60.0 ? next_frame + 1000.0 / 60.0 : t + 1000.0 / 60.0;
    wait = RK_MIN(next_frame - t, next_due() - t);
  } else wait = next_due() - rk_app_now();
  frame_wanted = false;
  if (!isfinite(wait)) wait = -1;
  if (wait < 0 && rk__headless()) wait = 1000.0 / 60.0;    /* headless never sleeps forever */
  if (max_wait >= 0 && (wait < 0 || wait > max_wait)) wait = max_wait;
  rk__platform()->wait(RK_MAX(wait, wait < 0 ? -1.0 : 0.0));
}

void rk__app_window_closed(rk_window *w) {
  (void)w;
  if (!rk__windows()) running = false;                     /* the last window closed: the app ends */
}

void rk_app_quit(int code) { exit_code = code; running = false; rk__platform()->wake(); }

void rk_app_run_for(double ms) {
  double end = rk_app_now() + ms;
  while (running && rk_app_now() < end - 1e-6) step(end - rk_app_now());
  if (running) step(0);
}

int rk_app_main(const rk_app_desc *desc) {
  memset(&app, 0, sizeof app);
  if (desc) memcpy(&app, desc, RK_MIN((size_t)desc->size, sizeof app));
  const char *env = getenv("RK_HEADLESS");
  plat = app.headless || (env && *env && *env != '0') ? &rk__platform_headless : &rk__platform_native;
  LOCK_INIT();
  if (!plat->init(&app)) {
    if (plat == &rk__platform_headless) return 1;
    fprintf(stderr, "rekta: no display (%s); set RK_HEADLESS=1 to run without one\n", plat->name);
    return 1;
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
  if (prof && *prof == '1' && rk__prof_frames)
    fprintf(stderr, "rekta: %ld frames, %.2f ms a frame on average, %.1f megapixels a frame drawn\n", rk__prof_frames, rk__prof_ms / rk__prof_frames, rk__prof_pixels / 1e6 / rk__prof_frames);
  while (rk__windows()) rk_window_close(rk__windows());
  plat->shutdown();
  rk__text_shutdown();
  free(timers); timers = NULL; ntimers = tcap = 0;
  return exit_code;
}

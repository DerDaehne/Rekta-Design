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
bool rk__headless(void) { return !strcmp(rk__platform()->name, "headless"); }   /* also a build without a windowing system */
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

void rk_app_stats(rk_stats *s, bool reset) {
  if (s) {
    rk_stats o;
    memset(&o, 0, sizeof o);
    o.size = sizeof o;
    const rk_meter *m = &rk__meter;
    o.frames = m->frames; o.animations = rk__anim_count(); o.views = (uint32_t)rk__live_views; o.glyphs = m->glyphs; o.slow = m->slow;
    o.draw_ms = m->draw_ms; o.last_ms = m->last_ms; o.max_ms = m->max_ms; o.pixels = m->pixels;
    o.latency_ms = m->latency_ms; o.max_latency_ms = m->max_latency_ms;
    memcpy(s, &o, RK_MIN((size_t)s->size, sizeof o));
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
  if (desc) memcpy(&app, desc, RK_MIN((size_t)desc->size, sizeof app));
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
  rk__undo_clear();
  plat->shutdown();
  rk__text_shutdown();
  free(timers); timers = NULL; ntimers = tcap = 0;
  return exit_code;
}

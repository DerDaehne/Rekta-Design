/* tiles_shell.h: the window, a tour and a benchmark around the demo's first two sections (rekta: tiles; live: a
 * feed), shared by tiles_imperative.c and tiles_declarative.c so that the two differ only in how they build and change
 * the sections. The file that includes this gives:
 *
 *   static rk_view *rekta_section(void), *live_section(void);
 *   static void live_tick(void *user);      the feed moves: the queue, the runners' status, an event, a task step
 *   static void refresh(void);              the views follow the state (nothing to do for the imperative one)
 *
 *   tiles_x                  a window, the feed every two seconds
 *   tiles_x --shots DIR      the tour without a screen, as PNGs
 *   tiles_x --bench N        what N updates cost, without a screen */
#include "../include/rekta_test.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static rk_window *window;
static rk_controller *places, *home;               /* borrowed: the window owns them */
static bool feed = true;                           /* the feed moves */

static rk_view *rekta_section(void);
static rk_view *live_section(void);
static void live_tick(void *user);
static void refresh(void);

static rk_status status_named(const char *s) {
  return !strcmp(s, "failed") ? RK_STATUS_ERROR : !strcmp(s, "waiting") ? RK_STATUS_WARNING : !strcmp(s, "done") ? RK_STATUS_SUCCESS : RK_STATUS_NONE;
}

/* The place the release notes tile opens: controllers, the same in both (as in demo.c). */
static rk_view *facet(const char *const rows[][2], int n) {
  rk_view *list = rk_rows();
  for (int i = 0; i < n; i++) {
    rk_status st = status_named(rows[i][1]);
    rk_rows_add(list, rows[i][0], st ? NULL : rows[i][1], st);
  }
  return list;
}

static rk_controller *notes_place(void) {
  static const char *const summary[][2] = { { "Nightly import", "failed" }, { "Release notes", "waiting" }, { "Deploy to staging", "done" } };
  static const char *const activity[][2] = { { "Import retried", "02:14" }, { "Review requested", "01:20" }, { "Deploy finished", "yesterday 23:05" } };
  static const char *const files[][2] = { { "schema.sql", "4 kB" }, { "import.log", "18 kB" } };
  rk_controller *pivot = rk_pivot();
  rk_pivot_add(pivot, "summary", facet(summary, 3));
  rk_pivot_add(pivot, "activity", facet(activity, 3));
  rk_pivot_add(pivot, "files", facet(files, 2));
  rk_view *page = rk_vstack();
  rk_controller *place = rk_controller_create_with_view(page, "release notes");
  rk_add(page, rk_label("Release notes", RK_TEXT_TITLE));
  rk_add(page, rk_label("A place of its own. The facets slide from their side; Escape, the back button or Alt+Left turn back home.", RK_TEXT_BODY));
  rk_view *facets = rk_add(page, rk_controller_embed(place, pivot));
  rk_view_set_grow(facets, 1);
  return place;
}

static void open_notes(rk_view *v, void *u) { (void)v; (void)u; rk_places_go(places, notes_place()); }

/* The tour ----------------------------------------------------------------------------------------------------------- */

static const char *shots;

static void shot(const char *name) {
  char path[1024];
  snprintf(path, sizeof path, "%s/%s.png", shots, name);
  if (rk_window_snapshot(window, path)) printf("  %s\n", path); else fprintf(stderr, "could not write %s\n", path);
}

static rk_view *find(const char *name) {
  rk_view *v = rk_view_find(rk_controller_view(places), name);
  if (!v) fprintf(stderr, "no view called %s\n", name);
  return v;
}

static rk_point middle(rk_view *v) {
  rk_rect b = rk_view_bounds(v);
  return rk_view_to_window(v, (rk_point){ b.w / 2, b.h / 2 });
}

static void pointer(rk_event_type type, rk_point p) {
  rk_event e = { sizeof e, type, 0, p, 0, 0, 0, 0, 0, NULL, NULL, 0 };
  rk_window_inject(window, &e);
}

static void press(const char *name) {
  rk_view *v = find(name);
  if (!v) return;
  rk_point p = middle(v);
  pointer(RK_EVENT_POINTER_DOWN, p);
  rk_app_run_for(60);
  pointer(RK_EVENT_POINTER_UP, p);
}

static void key(uint32_t k) { rk_window_key(window, k); }

static void tour(void) {
  rk_app_run_for(1200); shot("01-rekta");
  rk_point p = middle(find("presses"));
  p.x += 50; p.y -= 50;                                        /* near a corner: the tile tilts there */
  pointer(RK_EVENT_POINTER_DOWN, p); rk_app_run_for(120); shot("02-press");
  pointer(RK_EVENT_POINTER_UP, p); rk_app_run_for(140); shot("03-peek");
  rk_app_run_for(600);
  press("pin a tile"); rk_app_run_for(160); shot("04-pin");
  rk_app_run_for(600);
  press("pin a tile"); rk_app_run_for(600);
  press("resize"); rk_app_run_for(160); shot("05-reflow");
  rk_app_run_for(600);
  press("unpin it"); rk_app_run_for(100); shot("06-unpin");
  rk_app_run_for(600); shot("07-unpinned");
  press("notes"); rk_app_run_for(150); shot("08-turn-out");
  rk_app_run_for(1000); shot("09-place");
  key(RK_KEY_ESCAPE); rk_app_run_for(1200);
  key(RK_KEY_PAGE_DOWN); rk_app_run_for(700);
  live_tick(NULL); live_tick(NULL); live_tick(NULL); rk_app_run_for(110); shot("10-live-flip");
  rk_app_run_for(800); shot("11-live");
  live_tick(NULL); rk_app_run_for(100); shot("12-event");
  rk_app_run_for(800);
  for (int i = 0; i < 4; i++) { live_tick(NULL); rk_app_run_for(400); }
  rk_app_run_for(800); shot("13-live-later");
  key(RK_KEY_HOME); rk_app_run_for(900); shot("14-task-done");
  press("restart"); rk_app_run_for(150); shot("15-restart");
  rk_app_run_for(800);
  rk_app_quit(0);
}

/* The benchmark: the CPU time of the updates alone (the frames between them are not counted) --------------------- */

static double cpu_us(void) { return (double)clock() * 1e6 / CLOCKS_PER_SEC; }

static void bench(int n) {
  rk_stats s = { .size = sizeof s };
  rk_app_run_for(1500);
  rk_app_stats(&s);
  printf("%-12s %u views alive\n", "views", s.views);
  double t = 0;
  for (int i = 0; i < n; i++) { double t0 = cpu_us(); live_tick(NULL); t += cpu_us() - t0; rk_app_run_for(400); }
  printf("%-12s %8.1f us an update (%d feed ticks: tiles peek and flip, rows come and go)\n", "feed", t / n, n);
  t = 0;
  rk_view *presses = find("presses");
  for (int i = 0; i < n; i++) { double t0 = cpu_us(); rk_view_tap(presses); t += cpu_us() - t0; rk_app_run_for(400); }
  printf("%-12s %8.1f us a press (%d taps on a tile whose number peeks in, with the event's way to it)\n", "press", t / n, n);
  t = 0;
  for (int i = 0; i < n; i++) { double t0 = cpu_us(); refresh(); t += cpu_us() - t0; }
  printf("%-12s %8.1f us a refresh with nothing changed\n", "refresh", t / n);
  rk_app_stats(&s);
  printf("%-12s %u views alive\n", "views", s.views);
  rk_app_quit(0);
}

/* The app ------------------------------------------------------------------------------------------------------------ */

static int bench_n;

static void launch(void *user) {
  (void)user;
  rk_window_desc wd = { .size = sizeof wd, .title = "Rekta tiles", .width = 1280, .height = 820 };
  window = rk_window_create(&wd);
  home = rk_panorama();
  rk_controller_set_title(home, "home");
  rk_panorama_add(home, "rekta", rekta_section(), 0);
  rk_panorama_add(home, "live", live_section(), 0);
  places = rk_places(home);
  rk_window_show(window, places);
  if (shots) { tour(); return; }
  if (bench_n) { bench(bench_n); return; }
  rk_app_every(2000, live_tick, NULL);
}

int main(int argc, char **argv) {
  rk_app_desc app = { .size = sizeof app, .name = "Rekta tiles", .id = "de.rekta.tiles", .launch = launch };
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--shots") && i + 1 < argc) { shots = argv[++i]; app.headless = true; }
    else if (!strcmp(argv[i], "--bench") && i + 1 < argc) { bench_n = atoi(argv[++i]); app.headless = true; }
  }
  return rk_app_main(&app);
}

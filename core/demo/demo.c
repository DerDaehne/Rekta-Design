/* demo.c: Rekta core in one window. A panorama of tiles that measure the demo itself; a tile that opens a place
   (the turn), live tiles (a new number peeks in, a new status flips), a toast, a sheet, tiles that come and go.
   `demo --shots DIR` runs a script without a screen and writes what it sees as PNGs. */
#include "../include/rekta.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct demo {
  rk_window *window;
  rk_controller *places, *home;
  rk_view *seen, *live, *status, *presses, *board, *grow_tile, *events;
  int npresses, nseen, status_i, queue, pinned;
  float series[24]; int nseries;
  const char *shots;
} demo;

static demo D;

static rk_view *column(float gap) {
  rk_view *v = rk_view_create();
  rk_view_set_layout(v, RK_LAYOUT_COLUMN);
  rk_view_set_gap(v, gap);
  return v;
}

static void add(rk_view *parent, rk_view *child) { rk_view_add(parent, child); rk_release(child); }

static rk_view *text(const char *s, rk_text_style style) { return rk_label_create(s, style); }

static rk_view *button(const char *label, rk_button_kind kind, void (*fn)(rk_view *, void *)) {
  rk_view *b = rk_button_create(label, NULL, kind);
  rk_view_on_activate(b, fn, NULL);
  return b;
}

static rk_view *buttons(void) {
  rk_view *r = rk_view_create();
  rk_view_set_layout(r, RK_LAYOUT_ROW);
  rk_view_set_gap(r, rk_metric_of(RK_SPACE_S));
  rk_view_set_align(r, RK_ALIGN_START, RK_ALIGN_START);
  return r;
}

/* A place: the release notes, as a pivot of three facets. */
static rk_controller *facet(const char *title, const char *rows[][3], int n) {
  rk_view *list = rk_rows_create();
  for (int i = 0; i < n; i++) {
    rk_status st = !strcmp(rows[i][2], "failed") ? RK_STATUS_ERROR : !strcmp(rows[i][2], "waiting") ? RK_STATUS_WARNING : !strcmp(rows[i][2], "done") ? RK_STATUS_SUCCESS : RK_STATUS_NONE;
    rk_rows_add(list, rows[i][0], st ? NULL : rows[i][1], st);
  }
  rk_controller *c = rk_controller_create_with_view(list, title);
  rk_release(list);
  return c;
}

static rk_controller *notes_place(void) {
  static const char *summary[][3] = { { "Nightly import", "", "failed" }, { "Release notes", "", "waiting" }, { "Deploy to staging", "", "done" } };
  static const char *activity[][3] = { { "Import retried", "02:14", "" }, { "Review requested", "01:20", "" }, { "Deploy finished", "yesterday 23:05", "" } };
  static const char *files[][3] = { { "schema.sql", "4 kB", "" }, { "import.log", "18 kB", "" } };
  rk_controller *pivot = rk_pivot_create();
  rk_controller *f;
  f = facet("summary", summary, 3); rk_pivot_add(pivot, f); rk_release(f);
  f = facet("activity", activity, 3); rk_pivot_add(pivot, f); rk_release(f);
  f = facet("files", files, 2); rk_pivot_add(pivot, f); rk_release(f);
  rk_view *v = column(rk_metric_of(RK_SPACE_M));
  rk_view_set_padding(v, (rk_insets){ rk_metric_of(RK_SPACE_L), rk_metric_of(RK_SPACE_XL), rk_metric_of(RK_SPACE_L), rk_metric_of(RK_SPACE_XL) });
  add(v, text("Release notes", RK_TEXT_TITLE));
  rk_view *pv = rk_controller_view(pivot);
  rk_view_set_grow(pv, 1);
  rk_view_add(v, pv);
  rk_view_set_user(v, pivot, rk_release);                    /* the place keeps its pivot */
  rk_controller *place = rk_controller_create_with_view(v, "release notes");
  rk_release(v);
  return place;
}

static void open_notes(rk_view *v, void *u) {
  (void)v; (void)u;
  rk_controller *p = notes_place();
  rk_places_go(D.places, p);
  rk_release(p);
}

/* A sheet: a short question from the top edge. */
static rk_controller *sheet_ctl;
static void sheet_close(rk_view *v, void *u) { (void)v; (void)u; rk_controller_dismiss(sheet_ctl); }
static void sheet_reset(rk_view *v, void *u) {
  (void)v; (void)u;
  rk_controller_dismiss(sheet_ctl);
  D.npresses = 0;
  rk_view_transition(D.presses, RK_TRANSITION_PEEK);
  rk_tile_set_stat(D.presses, "0", "tiles pressed");
  rk_window_toast(D.window, "The demo is back where it started.", NULL, NULL, NULL);
}
static void open_sheet(rk_view *v, void *u) {
  (void)v; (void)u;
  rk_view *s = column(rk_metric_of(RK_SPACE_M));
  add(s, text("sheet", RK_TEXT_KICKER));
  add(s, text("Reset the demo?", RK_TEXT_GROUP));
  add(s, text("The counters go back to zero. The tiles stay where they are.", RK_TEXT_BODY));
  rk_view *row = buttons();
  add(row, button("cancel", RK_BUTTON_DEFAULT, sheet_close));
  add(row, button("reset", RK_BUTTON_PRIMARY, sheet_reset));
  add(s, row);
  rk_release(sheet_ctl);
  sheet_ctl = rk_controller_create_with_view(s, "Reset the demo?");
  rk_release(s);
  rk_controller_present(D.home, sheet_ctl);
}

static void undo_toast(void *u) { (void)u; rk_window_toast(D.window, "It is back.", NULL, NULL, NULL); }
static void raise_toast(rk_view *v, void *u) { (void)v; (void)u; rk_window_toast(D.window, "One thing archived.", "undo", undo_toast, NULL); }

static void press_tile(rk_view *v, void *u) {
  (void)u;
  char buf[16];
  D.npresses++;
  snprintf(buf, sizeof buf, "%d", D.npresses);
  rk_view_transition(v, RK_TRANSITION_PEEK);
  rk_tile_set_stat(v, buf, D.npresses == 1 ? "tile pressed" : "tiles pressed");
}

static void toggle_theme(rk_view *v, void *u) { (void)v; (void)u; rk_app_set_theme(rk_app_is_dark() ? RK_THEME_LIGHT : RK_THEME_DARK); }

/* Tiles that come and go: a pinned tile builds in after the first one, the others make room. */
static void pin(rk_view *v, void *u) {
  (void)v; (void)u;
  char buf[16];
  snprintf(buf, sizeof buf, "%d", ++D.pinned);
  rk_view *t = rk_tile_create(RK_STATUS_NONE, NULL);
  rk_tile_set_stat(t, buf, "pinned tile");
  rk_view_set_name(t, "pinned");
  rk_animate_begin(RK_MOTION_ENTER);
  rk_view_insert(D.board, t, 1);
  rk_animate_commit();
  rk_release(t);
}
static void unpin(rk_view *v, void *u) {
  (void)v; (void)u;
  rk_view *t = rk_view_find(D.board, "pinned");
  if (!t) { rk_window_toast(D.window, "Nothing pinned yet.", NULL, NULL, NULL); return; }
  rk_view_set_name(t, NULL);
  rk_animate_begin(RK_MOTION_LEAVE);
  rk_view_remove(t);
  rk_animate_commit();
}
static void grow(rk_view *v, void *u) {
  (void)v; (void)u;
  static int big;
  big = !big;
  rk_animate_begin(RK_MOTION_REFLOW);
  rk_view_set_span(D.grow_tile, big ? 2 : 1, big ? 2 : 1);
  rk_animate_commit();
}

/* Live: a new count peeks in, a new status flips the tile, the line moves on. */
static void live_tick(void *u) {
  (void)u;
  static const rk_status order[] = { RK_STATUS_INFO, RK_STATUS_WARNING, RK_STATUS_SUCCESS, RK_STATUS_ERROR };
  static int tick;
  tick++;
  char buf[16];
  D.queue = (D.queue + 3 + tick * 7) % 40;
  snprintf(buf, sizeof buf, "%d", D.queue);
  rk_view_transition(D.live, RK_TRANSITION_PEEK);
  rk_tile_set_stat(D.live, buf, "jobs in the queue");
  if (D.nseries < 24) D.nseries++; else memmove(D.series, D.series + 1, 23 * sizeof(float));
  D.series[D.nseries - 1] = (float)D.queue;
  rk_tile_set_series(D.live, D.series, D.nseries);
  if (tick % 3 == 0) {
    D.status_i = (D.status_i + 1) % 4;
    rk_view_transition(D.status, RK_TRANSITION_FLIP);
    rk_tile_set_status(D.status, order[D.status_i], NULL);
  }
}

static rk_view *tiles_section(void) {
  rk_view *s = column(rk_metric_of(RK_SPACE_M));
  add(s, text("Tiles, not cards.", RK_TEXT_GROUP));
  add(s, text("A tile's whole area is one fill, and the fill is its status. Press one: it nudges back where you press it.", RK_TEXT_BODY));
  rk_view *g = rk_view_create();
  rk_view_set_layout(g, RK_LAYOUT_TILES);
  D.board = g;
  D.seen = rk_tile_create(RK_STATUS_INFO, NULL);
  rk_tile_set_stat(D.seen, "1 / 3", "sections seen");
  rk_tile_set_message(D.seen, "Every section you move to counts. When you have seen them all, this tile is done.");
  rk_tile_set_progress(D.seen, 1.0f / 3);
  rk_view_set_span(D.seen, 2, 2);
  add(g, D.seen);
  rk_view *notes = rk_tile_create(RK_STATUS_NONE, NULL);
  rk_tile_set_stat(notes, "0.3", "release notes");
  rk_view_on_activate(notes, open_notes, NULL);
  rk_view_set_name(notes, "notes");
  add(g, notes);
  D.presses = rk_tile_create(RK_STATUS_WARNING, NULL);
  rk_tile_set_stat(D.presses, "0", "tiles pressed");
  rk_view_on_activate(D.presses, press_tile, NULL);
  rk_view_set_name(D.presses, "presses");
  add(g, D.presses);
  D.grow_tile = rk_tile_create(RK_STATUS_SUCCESS, NULL);
  rk_tile_set_stat(D.grow_tile, "12", "builds today");
  rk_view_set_name(D.grow_tile, "grow");
  add(g, D.grow_tile);
  rk_view *idle = rk_tile_create(RK_STATUS_NONE, NULL);
  rk_tile_set_stat(idle, "51", "icons");
  add(g, idle);
  add(s, g);
  rk_view *c = buttons();
  rk_view *b;
  b = button("pin a tile", RK_BUTTON_DEFAULT, pin); rk_view_set_name(b, "pin"); add(c, b);
  b = button("unpin it", RK_BUTTON_DEFAULT, unpin); rk_view_set_name(b, "unpin"); add(c, b);
  b = button("resize", RK_BUTTON_DEFAULT, grow); rk_view_set_name(b, "resize"); add(c, b);
  add(s, c);
  return s;
}

static rk_view *live_section(void) {
  rk_view *s = column(rk_metric_of(RK_SPACE_M));
  add(s, text("Live, and honest about it.", RK_TEXT_GROUP));
  rk_view *g = rk_view_create();
  rk_view_set_layout(g, RK_LAYOUT_TILES);
  D.live = rk_tile_create(RK_STATUS_INFO, NULL);
  rk_tile_set_stat(D.live, "14", "jobs in the queue");
  rk_view_set_span(D.live, 2, 1);
  rk_view_set_name(D.live, "live");
  add(g, D.live);
  D.status = rk_tile_create(RK_STATUS_INFO, NULL);
  rk_tile_set_stat(D.status, "3", "runners");
  rk_view_set_name(D.status, "status");
  add(g, D.status);
  add(s, g);
  add(s, text("latest events", RK_TEXT_KICKER));
  D.events = rk_rows_create();
  rk_rows_add(D.events, "Build #4190 finished", NULL, RK_STATUS_SUCCESS);
  rk_rows_add(D.events, "A runner joined the pool", NULL, RK_STATUS_INFO);
  rk_rows_add(D.events, "Review waiting on Ada", NULL, RK_STATUS_WARNING);
  add(s, D.events);
  return s;
}

static rk_view *controls_section(void) {
  rk_view *s = column(rk_metric_of(RK_SPACE_M));
  add(s, text("Every transition means one thing.", RK_TEXT_GROUP));
  add(s, text("A sheet is a short question from the top edge; a toast says something happened; tiles that come and go make room.", RK_TEXT_BODY));
  rk_view *a = buttons();
  rk_view *b;
  b = button("open a sheet", RK_BUTTON_PRIMARY, open_sheet); rk_view_set_name(b, "sheet"); add(a, b);
  b = button("raise a toast", RK_BUTTON_DEFAULT, raise_toast); rk_view_set_name(b, "toast"); add(a, b);
  add(s, a);
  rk_view *d = buttons();
  b = button("light or dark", RK_BUTTON_QUIET, toggle_theme); rk_view_set_name(b, "theme"); add(d, b);
  add(s, d);
  return s;
}

/* The script for --shots: what a person would do, with a picture at each moment worth seeing. */
static void shot(const char *name) {
  char path[1024];
  snprintf(path, sizeof path, "%s/%s.png", D.shots, name);
  if (!rk_window_snapshot(D.window, path)) fprintf(stderr, "could not write %s\n", path);
  else printf("  %s\n", path);
}

static rk_point center(const char *name) {
  rk_view *v = rk_view_find(rk_controller_view(D.places), name);
  if (!v) return (rk_point){ 0, 0 };
  rk_rect f = rk_view_bounds(v);
  return rk_view_to_window(v, (rk_point){ f.w / 2, f.h / 2 });
}

static void click_at(rk_point p, double hold) {
  rk_event e = { sizeof e, RK_EVENT_POINTER_DOWN, 0, p, 0, 0, 0, 0, 0, NULL };
  rk_window_inject(D.window, &e);
  rk_app_run_for(hold);
  e.type = RK_EVENT_POINTER_UP;
  rk_window_inject(D.window, &e);
}

static void script(void) {
  rk_app_run_for(1200);
  shot("01-home");
  rk_point p = center("presses");
  p.x += 50; p.y -= 50;                                       /* near the top right corner: it tilts there */
  rk_event e = { sizeof e, RK_EVENT_POINTER_DOWN, 0, p, 0, 0, 0, 0, 0, NULL };
  rk_window_inject(D.window, &e);
  rk_app_run_for(120);
  shot("02-press");
  e.type = RK_EVENT_POINTER_UP;
  rk_window_inject(D.window, &e);
  rk_app_run_for(140);
  shot("03-peek");
  rk_app_run_for(600);
  click_at(center("notes"), 60);
  rk_app_run_for(150);
  shot("04-turn-out");
  rk_app_run_for(260);
  shot("05-turn-in");
  rk_app_run_for(800);
  shot("06-place");
  rk_event k = { sizeof k, RK_EVENT_KEY_DOWN, 0, { 0, 0 }, 0, 0, 0, RK_KEY_ESCAPE, 0, NULL };
  rk_window_inject(D.window, &k);
  rk_app_run_for(1200);
  rk_event wheel = { sizeof wheel, RK_EVENT_WHEEL, 0, { 600, 400 }, 0, 0, 900, 0, 0, NULL };
  rk_window_inject(D.window, &wheel);
  rk_app_run_for(700);
  shot("07-scrolled");
  rk_event home = { sizeof home, RK_EVENT_KEY_DOWN, 0, { 0, 0 }, 0, 0, 0, RK_KEY_HOME, 0, NULL };
  rk_window_inject(D.window, &home);                          /* back to the first section, where the board is */
  rk_app_run_for(700);
  click_at(center("pin"), 40);
  rk_app_run_for(160);
  shot("08-pin-enter");
  rk_app_run_for(600);
  click_at(center("resize"), 40);
  rk_app_run_for(160);
  shot("09-resize");
  rk_app_run_for(600);
  rk_event end = { sizeof end, RK_EVENT_KEY_DOWN, 0, { 0, 0 }, 0, 0, 0, RK_KEY_END, 0, NULL };
  rk_window_inject(D.window, &end);                           /* to the last section, where the buttons are */
  rk_app_run_for(700);
  click_at(center("toast"), 40);
  rk_app_run_for(400);
  shot("10-toast");
  click_at(center("sheet"), 40);
  rk_app_run_for(120);
  shot("11-sheet-in");
  rk_app_run_for(500);
  shot("12-sheet");
  rk_window_inject(D.window, &k);
  rk_app_run_for(600);
  live_tick(NULL); live_tick(NULL); live_tick(NULL);
  rk_app_run_for(110);
  shot("13-flip");
  rk_app_run_for(800);
  rk_app_set_theme(RK_THEME_DARK);
  rk_app_run_for(600);
  shot("14-dark");
  rk_app_quit(0);
}

static void launch(void *user) {
  (void)user;
  rk_window_desc wd = { sizeof wd, "Rekta core", 1280, 760, 0, 0, false };
  D.window = rk_window_create(&wd);
  if (!D.window) { rk_app_quit(1); return; }
  D.home = rk_panorama_create();
  rk_controller_set_title(D.home, "home");
  rk_panorama_add(D.home, "rekta", tiles_section(), 0);
  rk_panorama_add(D.home, "live", live_section(), 0);
  rk_panorama_add(D.home, "motion", controls_section(), 0);
  D.places = rk_places_create(D.home);
  rk_window_set_root(D.window, D.places);
  rk_window_show(D.window);
  if (D.shots) { script(); return; }
  rk_app_after(2000, true, live_tick, NULL);
}

int main(int argc, char **argv) {
  rk_app_desc desc = { sizeof desc, "Rekta core", "de.rekta.demo", launch, NULL, NULL, false };
  for (int i = 1; i < argc; i++) if (!strcmp(argv[i], "--shots") && i + 1 < argc) { D.shots = argv[++i]; desc.headless = true; }
  int code = rk_app_main(&desc);
  rk_release(sheet_ctl);
  rk_release(D.places); rk_release(D.home);
  return code;
}

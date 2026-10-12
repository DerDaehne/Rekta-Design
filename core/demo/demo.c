/* demo.c: every feature of rekta-core in one window, as a panorama you move through sideways.
 *
 *   rekta     tiles: the press, a tile that opens a place (the turn), a number that peeks in, a task whose status
 *             flips when it is done, tiles that come and go and a tile that grows (reflow)
 *   live      a live tile with a sparkline, a status that flips, events that arrive and leave
 *   motion    a sheet from the top edge, toasts (one with undo), a cross-fade
 *   controls  every kind of button, progress, hairline rows, and a custom view drawn with the canvas
 *   type      the text styles and all 51 icons
 *   settings  theme, density, motion and accent, as the system or as chosen
 *
 * Keys: Tab moves focus, Enter or Space presses, Page Up and Page Down go by section, Escape goes back.
 * `demo --shots DIR` runs a tour without a screen and writes what it sees as PNGs.
 * `demo --perf` puts the loop under load and measures it (resizing, a storm of presses, settling), without a screen;
 * `demo --perf --screen` does it in a real window. RK_HUD=1 shows the numbers in any run, RK_TRACE=1 prints frames. */
#include "../include/rekta.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct demo {
  rk_window *window;
  rk_controller *places, *home;
  rk_view *board, *task, *presses, *grow_tile;            /* rekta */
  rk_view *live, *status, *events;                         /* live */
  rk_view *fade;                                           /* motion */
  rk_view *bar, *chart;                                    /* controls */
  rk_view *now;                                            /* settings: what is in effect */
  int npresses, pinned, queue, status_i, ticks, events_n, fades;
  float task_done, series[24]; int nseries;
  bool feed, big;
  const char *shots;
  bool perf, screen;
} demo;

static demo D;

/* Small helpers for building views ----------------------------------------------------------------------------- */

static void add(rk_view *parent, rk_view *child) { rk_view_add(parent, child); rk_release(child); }

static rk_view *column(float gap) {
  rk_view *v = rk_view_create();
  rk_view_set_layout(v, RK_LAYOUT_COLUMN);
  rk_view_set_gap(v, gap);
  return v;
}

static rk_view *row(float gap) {
  rk_view *v = rk_view_create();
  rk_view_set_layout(v, RK_LAYOUT_ROW);
  rk_view_set_gap(v, gap);
  rk_view_set_align(v, RK_ALIGN_START, RK_ALIGN_START);
  return v;
}

static rk_view *grid(void) {
  rk_view *v = rk_view_create();
  rk_view_set_layout(v, RK_LAYOUT_TILES);
  return v;
}

static rk_view *text(const char *s, rk_text_style style) { return rk_label_create(s, style); }

static rk_view *button(const char *name, const char *label, const char *icon, rk_button_kind kind, void (*fn)(rk_view *, void *), void *user) {
  rk_view *b = rk_button_create(label, icon, kind);
  if (fn) rk_view_on_activate(b, fn, user);
  rk_view_set_name(b, name);
  return b;
}

static rk_view *tile(const char *name, rk_status status, const char *value, const char *label) {
  rk_view *t = rk_tile_create(status, NULL);
  rk_tile_set_stat(t, value, label);
  rk_view_set_name(t, name);
  return t;
}

static rk_view *section(const char *heading, const char *about) {
  rk_view *s = column(rk_metric_of(RK_SPACE_M));
  add(s, text(heading, RK_TEXT_GROUP));
  if (about) add(s, text(about, RK_TEXT_BODY));
  return s;
}

/* rekta: tiles ------------------------------------------------------------------------------------------------- */

/* A place: the release notes, a pivot of three facets. A place's controller keeps what it needs alive. */
static rk_controller *facet(const char *title, const char *const rows[][2], int n) {
  rk_view *list = rk_rows_create();
  for (int i = 0; i < n; i++) {
    const char *s = rows[i][1];
    rk_status st = !strcmp(s, "failed") ? RK_STATUS_ERROR : !strcmp(s, "waiting") ? RK_STATUS_WARNING : !strcmp(s, "done") ? RK_STATUS_SUCCESS : RK_STATUS_NONE;
    rk_rows_add(list, rows[i][0], st ? NULL : s, st);
  }
  rk_controller *c = rk_controller_create_with_view(list, title);
  rk_release(list);
  return c;
}

static rk_controller *notes_place(void) {
  static const char *const summary[][2] = { { "Nightly import", "failed" }, { "Release notes", "waiting" }, { "Deploy to staging", "done" } };
  static const char *const activity[][2] = { { "Import retried", "02:14" }, { "Review requested", "01:20" }, { "Deploy finished", "yesterday 23:05" } };
  static const char *const files[][2] = { { "schema.sql", "4 kB" }, { "import.log", "18 kB" } };
  rk_controller *pivot = rk_pivot_create(), *f;
  f = facet("summary", summary, 3); rk_pivot_add(pivot, f); rk_release(f);
  f = facet("activity", activity, 3); rk_pivot_add(pivot, f); rk_release(f);
  f = facet("files", files, 2); rk_pivot_add(pivot, f); rk_release(f);
  float l = rk_metric_of(RK_SPACE_L), xl = rk_metric_of(RK_SPACE_XL);
  rk_view *v = column(rk_metric_of(RK_SPACE_M));
  rk_view_set_padding(v, (rk_insets){ l, xl, l, xl });
  add(v, text("Release notes", RK_TEXT_TITLE));
  add(v, text("A place of its own. The facets slide from their side; Escape, the back button or Alt+Left turn back home.", RK_TEXT_BODY));
  rk_view *pv = rk_controller_view(pivot);
  rk_view_set_grow(pv, 1);
  rk_view_add(v, pv);
  rk_view_set_user(v, pivot, rk_release);                     /* the place keeps its pivot */
  rk_controller *place = rk_controller_create_with_view(v, "release notes");
  rk_release(v);
  return place;
}

static void open_notes(rk_view *v, void *u) {
  (void)v; (void)u;
  rk_controller *p = notes_place();
  rk_places_go(D.places, p);                                  /* the board turns away, the place turns in */
  rk_release(p);
}

static void press_tile(rk_view *v, void *u) {
  (void)u;
  char buf[16];
  snprintf(buf, sizeof buf, "%d", ++D.npresses);
  rk_view_transition(v, RK_TRANSITION_PEEK);                  /* a new number peeks in */
  rk_tile_set_stat(v, buf, D.npresses == 1 ? "tile pressed" : "tiles pressed");
}

static void task_tick(void *u) {
  (void)u;
  if (D.task_done >= 1) return;
  D.task_done = fminf(1, D.task_done + 0.125f);
  char buf[16];
  snprintf(buf, sizeof buf, "%d / 8", (int)lroundf(D.task_done * 8));
  rk_tile_set_progress(D.task, D.task_done);                  /* the bar glides to its value */
  rk_tile_set_stat(D.task, buf, "import steps");
  if (D.task_done >= 1) {
    rk_view_transition(D.task, RK_TRANSITION_FLIP);           /* done: a new status turns the tile over */
    rk_tile_set_status(D.task, RK_STATUS_SUCCESS, NULL);
    rk_tile_set_message(D.task, "Imported. Stillness means done.");
    rk_tile_set_progress(D.task, -1);
  }
}

static void restart_task(rk_view *v, void *u) {
  (void)v; (void)u;
  D.task_done = 0;
  rk_view_transition(D.task, RK_TRANSITION_FLIP);
  rk_tile_set_status(D.task, RK_STATUS_INFO, NULL);
  rk_tile_set_stat(D.task, "0 / 8", "import steps");
  rk_tile_set_message(D.task, "Each step advances the bar; the last one flips the tile.");
  rk_tile_set_progress(D.task, 0);
}

static void pin(rk_view *v, void *u) {
  (void)v; (void)u;
  char buf[16];
  snprintf(buf, sizeof buf, "%d", ++D.pinned);
  rk_view *t = tile("pinned", RK_STATUS_NONE, buf, "pinned tile");
  rk_animate_begin(RK_MOTION_ENTER);                          /* it builds in after the first tile; the rest make room */
  rk_view_insert(D.board, t, 1);
  rk_animate_commit();
  rk_release(t);
}

static void unpin(rk_view *v, void *u) {
  (void)v; (void)u;
  rk_view *t = rk_view_find(D.board, "pinned");
  if (!t) { rk_window_toast(D.window, "Nothing pinned yet.", NULL, NULL, NULL); return; }
  rk_view_set_name(t, NULL);
  rk_animate_begin(RK_MOTION_LEAVE);                          /* it leaves first, then the rest close up */
  rk_view_remove(t);
  rk_animate_commit();
}

static void grow(rk_view *v, void *u) {
  (void)v; (void)u;
  D.big = !D.big;
  rk_animate_begin(RK_MOTION_REFLOW);                         /* it grows from its old size; the grid makes room */
  rk_view_set_span(D.grow_tile, D.big ? 2 : 1, D.big ? 2 : 1);
  rk_animate_commit();
}

static rk_view *rekta_section(void) {
  rk_view *s = section("Tiles, not cards.", "A tile's whole area is one fill, and the fill is its status. Press one: it nudges back where you press it. A tile that opens something presses; one that only informs does not.");
  rk_view *g = grid();
  D.board = g;
  D.task = tile("task", RK_STATUS_INFO, "0 / 8", "import steps");
  rk_tile_set_message(D.task, "Each step advances the bar; the last one flips the tile.");
  rk_tile_set_progress(D.task, 0);
  rk_view_set_span(D.task, 2, 2);
  add(g, D.task);
  rk_view *notes = tile("notes", RK_STATUS_NONE, "0.3", "release notes");
  rk_view_on_activate(notes, open_notes, NULL);
  add(g, notes);
  D.presses = tile("presses", RK_STATUS_WARNING, "0", "tiles pressed");
  rk_view_on_activate(D.presses, press_tile, NULL);
  add(g, D.presses);
  D.grow_tile = tile("grow", RK_STATUS_SUCCESS, "12", "builds today");
  add(g, D.grow_tile);
  add(g, tile("icons", RK_STATUS_NONE, "51", "icons"));
  add(s, g);
  rk_view *b = row(rk_metric_of(RK_SPACE_S));
  add(b, button("pin", "pin a tile", NULL, RK_BUTTON_DEFAULT, pin, NULL));
  add(b, button("unpin", "unpin it", NULL, RK_BUTTON_DEFAULT, unpin, NULL));
  add(b, button("resize", "resize", NULL, RK_BUTTON_DEFAULT, grow, NULL));
  add(b, button("restart", "restart", "refresh", RK_BUTTON_QUIET, restart_task, NULL));
  add(s, b);
  return s;
}

/* live --------------------------------------------------------------------------------------------------------- */

static const char *const EVENTS[][2] = {
  { "Build #4190 finished", "s" }, { "A runner joined the pool", "i" }, { "Review waiting on Ada", "w" },
  { "Nightly import retried", "i" }, { "Deploy to staging finished", "s" }, { "Disk at 81% on runner 3", "w" }, { "Upload failed", "e" },
};

static rk_status status_of(char c) { return c == 's' ? RK_STATUS_SUCCESS : c == 'w' ? RK_STATUS_WARNING : c == 'e' ? RK_STATUS_ERROR : RK_STATUS_INFO; }

/* An event arrives at the top; the oldest leaves at the bottom. rk_rows_add appends, so the new row moves up. */
static void push_event(void) {
  const char *const *e = EVENTS[D.events_n++ % 7];
  rk_animate_begin(RK_MOTION_ENTER);
  rk_view *r = rk_rows_add(D.events, e[0], NULL, status_of(e[1][0]));
  rk_retain(r);
  rk_view_remove(r);
  rk_view_insert(D.events, r, 0);
  rk_release(r);
  rk_animate_commit();
  if (rk_view_count(D.events) > 4) {
    rk_animate_begin(RK_MOTION_LEAVE);
    rk_view_remove(rk_view_child(D.events, rk_view_count(D.events) - 1));
    rk_animate_commit();
  }
}

static void live_tick(void *u) {
  (void)u;
  static const rk_status order[] = { RK_STATUS_INFO, RK_STATUS_WARNING, RK_STATUS_SUCCESS, RK_STATUS_ERROR };
  if (!D.feed) return;
  D.ticks++;
  char buf[16];
  D.queue = (D.queue + 3 + D.ticks * 7) % 40;
  snprintf(buf, sizeof buf, "%d", D.queue);
  rk_view_transition(D.live, RK_TRANSITION_PEEK);             /* a new count is news: it peeks in */
  rk_tile_set_stat(D.live, buf, "jobs in the queue");
  if (D.nseries < 24) D.nseries++; else memmove(D.series, D.series + 1, 23 * sizeof(float));
  D.series[D.nseries - 1] = (float)D.queue;
  rk_tile_set_series(D.live, D.series, D.nseries);
  if (D.ticks % 3 == 0) {
    D.status_i = (D.status_i + 1) % 4;
    rk_view_transition(D.status, RK_TRANSITION_FLIP);         /* a new status turns the tile over */
    rk_tile_set_status(D.status, order[D.status_i], NULL);
  }
  if (D.ticks % 2 == 0) push_event();
  task_tick(NULL);
}

static void toggle_feed(rk_view *v, void *u) {
  (void)v; (void)u;
  D.feed = !D.feed;
  rk_window_toast(D.window, D.feed ? "Feed on: the figures are live." : "Feed off: the figures hold still.", NULL, NULL, NULL);
}

static rk_view *live_section(void) {
  rk_view *s = section("Live, and honest about it.", "Every two seconds the queue moves and its number peeks in; every six the runners' status flips. Events arrive at the top and the oldest leaves.");
  rk_view *g = grid();
  D.live = tile("live", RK_STATUS_INFO, "14", "jobs in the queue");
  rk_view_set_span(D.live, 2, 1);
  add(g, D.live);
  D.status = tile("status", RK_STATUS_INFO, "3", "runners");
  add(g, D.status);
  add(s, g);
  add(s, text("latest events", RK_TEXT_KICKER));
  D.events = rk_rows_create();
  for (int i = 0; i < 3; i++) { rk_rows_add(D.events, EVENTS[i][0], NULL, status_of(EVENTS[i][1][0])); D.events_n++; }
  add(s, D.events);
  rk_view *b = row(rk_metric_of(RK_SPACE_S));
  add(b, button("feed", "feed on or off", "pause", RK_BUTTON_QUIET, toggle_feed, NULL));
  add(s, b);
  return s;
}

/* motion: sheets, toasts, a cross-fade -------------------------------------------------------------------------- */

static rk_controller *sheet_ctl;

static void sheet_close(rk_view *v, void *u) { (void)v; (void)u; rk_controller_dismiss(sheet_ctl); }

static void sheet_reset(rk_view *v, void *u) {
  (void)v; (void)u;
  rk_controller_dismiss(sheet_ctl);                           /* it leaves the way it came */
  D.npresses = 0;
  rk_view_transition(D.presses, RK_TRANSITION_PEEK);
  rk_tile_set_stat(D.presses, "0", "tiles pressed");
  rk_window_toast(D.window, "The counter is back at zero.", NULL, NULL, NULL);
}

static void open_sheet(rk_view *v, void *u) {
  (void)v; (void)u;
  rk_view *s = column(rk_metric_of(RK_SPACE_M));
  add(s, text("sheet", RK_TEXT_KICKER));
  add(s, text("Reset the counter?", RK_TEXT_GROUP));
  add(s, text("The pressed tiles go back to zero. Escape or a click on the shading keeps them.", RK_TEXT_BODY));
  rk_view *b = row(rk_metric_of(RK_SPACE_S));
  add(b, button("cancel", "keep it", NULL, RK_BUTTON_DEFAULT, sheet_close, NULL));
  add(b, button("reset", "reset", NULL, RK_BUTTON_PRIMARY, sheet_reset, NULL));
  add(s, b);
  rk_release(sheet_ctl);
  sheet_ctl = rk_controller_create_with_view(s, "Reset the counter?");
  rk_release(s);
  rk_controller_present(D.home, sheet_ctl);
}

static void undone(void *u) { (void)u; rk_window_toast(D.window, "It is back.", NULL, NULL, NULL); }
static void toast_undo(rk_view *v, void *u) { (void)v; (void)u; rk_window_toast(D.window, "One thing archived.", "undo", undone, NULL); }
static void toast_plain(rk_view *v, void *u) { (void)v; (void)u; rk_window_toast(D.window, "Something happened. A toast says so, once.", NULL, NULL, NULL); }

static void fade(rk_view *v, void *u) {
  (void)v; (void)u;
  static const char *const lines[] = { "Same place, new words: the old ones fade out over the new.", "A cross-fade changes the whole look, and moves nothing.", "Theme and accent changes cross-fade the window this way." };
  rk_view_transition(D.fade, RK_TRANSITION_CROSSFADE);
  rk_label_set_text(D.fade, lines[++D.fades % 3]);
}

static rk_view *motion_section(void) {
  rk_view *s = section("Every transition means one thing.", "A sheet is a short question from the top edge; a toast says something happened; an undo toast keeps its window open for five seconds.");
  rk_view *b = row(rk_metric_of(RK_SPACE_S));
  add(b, button("sheet", "open a sheet", NULL, RK_BUTTON_PRIMARY, open_sheet, NULL));
  add(b, button("toast", "archive", "archive", RK_BUTTON_DEFAULT, toast_undo, NULL));
  add(b, button("toast-plain", "a toast", NULL, RK_BUTTON_DEFAULT, toast_plain, NULL));
  add(s, b);
  add(s, text("cross-fade", RK_TEXT_KICKER));
  D.fade = text("Same place, new words: the old ones fade out over the new.", RK_TEXT_BODY);
  add(s, D.fade);
  rk_view *c = row(rk_metric_of(RK_SPACE_S));
  add(c, button("fade", "new words", NULL, RK_BUTTON_QUIET, fade, NULL));
  add(s, c);
  return s;
}

/* controls: buttons, progress, rows, a custom view --------------------------------------------------------------- */

/* A custom view: a bar chart drawn with the canvas. New figures travel to their place over duration-long. */
typedef struct chart { float from[7], to[7]; double start; } chart;

static void chart_draw(rk_view *v, rk_canvas *c, void *state) {
  chart *ch = state;
  rk_rect b = rk_view_bounds(v);
  double d = rk_duration_of(RK_DURATION_LONG), t = d > 0 ? (rk_app_now() - ch->start) / d : 1;
  if (t > 1) t = 1;
  float k = (float)(1 - pow(1 - t, 3));                       /* eased toward the end */
  float base = b.h - 18, w = (b.w - 6 * 8) / 7;
  static const char *const days[7] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
  rk_canvas_fill_rect(c, (rk_rect){ 0, base, b.w, 1 }, rk_color_of(RK_ROLE_TEXT_LOW));
  for (int i = 0; i < 7; i++) {
    float h = (ch->from[i] + (ch->to[i] - ch->from[i]) * k) * (base - 8);
    rk_canvas_fill_rect(c, (rk_rect){ i * (w + 8), base - h, w, h }, rk_color_of(i == 3 ? RK_ROLE_TEXT : RK_ROLE_TEXT_MEDIUM));
    rk_canvas_text(c, days[i], (rk_point){ i * (w + 8), base + 2 }, RK_TEXT_META, rk_color_of(RK_ROLE_TEXT_HIGH));
  }
  if (t < 1) rk_view_redraw(v);                               /* the next frame, while it moves */
}

static rk_size chart_measure(rk_view *v, rk_size available, void *state) {
  (void)v; (void)state;
  rk_size s = { isfinite(available.w) ? available.w : 400, 140 };
  return s;
}

static const rk_view_class chart_class = { sizeof(rk_view_class), "demo_chart", chart_draw, chart_measure, NULL, NULL, free };

static void new_figures(rk_view *v, void *u) {
  (void)v; (void)u;
  chart *ch = rk_view_state(D.chart);
  for (int i = 0; i < 7; i++) {
    ch->from[i] = ch->to[i];
    ch->to[i] = 0.2f + 0.8f * (float)((i * 37 + D.ticks * 53 + D.fades * 29 + D.npresses * 17) % 100) / 100;
  }
  ch->start = rk_app_now();
  rk_view_redraw(D.chart);
}

static void advance(rk_view *v, void *u) {
  (void)v; (void)u;
  static float p = 0.25f;
  p = p >= 1 ? 0 : p + 0.25f;
  rk_progress_set(D.bar, p);                                  /* the value glides */
}

static rk_view *controls_section(void) {
  rk_view *s = section("Native where it can be, Rekta where it shows.", NULL);
  rk_view *b = row(rk_metric_of(RK_SPACE_S));
  add(b, button(NULL, "primary", NULL, RK_BUTTON_PRIMARY, NULL, NULL));
  add(b, button(NULL, "default", NULL, RK_BUTTON_DEFAULT, NULL, NULL));
  add(b, button(NULL, "quiet", NULL, RK_BUTTON_QUIET, NULL, NULL));
  rk_view *off = button(NULL, "disabled", NULL, RK_BUTTON_DEFAULT, NULL, NULL);
  rk_button_set_disabled(off, "nothing to do yet");
  add(b, off);
  add(s, b);
  rk_view *bar = row(2);
  add(bar, button(NULL, "edit", "edit", RK_BUTTON_BAR, NULL, NULL));
  add(bar, button(NULL, "share", "link", RK_BUTTON_BAR, NULL, NULL));
  add(bar, button(NULL, "delete", "trash", RK_BUTTON_BAR, NULL, NULL));
  add(bar, button(NULL, NULL, "more", RK_BUTTON_BAR, NULL, NULL));
  add(s, bar);
  add(s, text("progress", RK_TEXT_KICKER));
  D.bar = rk_progress_create(0.25f);
  add(s, D.bar);
  add(s, rk_progress_create(-1));                             /* working on it, no end known */
  rk_view *p = row(rk_metric_of(RK_SPACE_S));
  add(p, button("advance", "advance", NULL, RK_BUTTON_QUIET, advance, NULL));
  add(s, p);
  add(s, text("a custom view, drawn with the canvas", RK_TEXT_KICKER));
  chart *ch = calloc(1, sizeof *ch);
  for (int i = 0; i < 7; i++) ch->to[i] = ch->from[i] = (float)(i == 5 || i == 6 ? 0.15 : 0.6 + 0.05 * i);
  D.chart = rk_view_create_custom(&chart_class, ch);
  rk_view_set_access(D.chart, RK_ACCESS_IMAGE, "build minutes by weekday");
  add(s, D.chart);
  rk_view *f = row(rk_metric_of(RK_SPACE_S));
  add(f, button("figures", "new figures", NULL, RK_BUTTON_QUIET, new_figures, NULL));
  add(s, f);
  add(s, text("rows", RK_TEXT_KICKER));
  rk_view *rows = rk_rows_create();
  rk_rows_add(rows, "Nightly import", NULL, RK_STATUS_ERROR);
  rk_rows_add(rows, "Release notes", NULL, RK_STATUS_WARNING);
  rk_rows_add(rows, "schema.sql", "4 kB", RK_STATUS_NONE);
  add(s, rows);
  return s;
}

/* type: text styles and icons ------------------------------------------------------------------------------------ */

static const char *const ICONS[] = {
  "archive", "back", "bell", "calendar", "check", "chevron-down", "chevron-left", "chevron-right", "chevron-up", "clock",
  "close", "copy", "down", "download", "edit", "error", "external", "eye", "file", "filter", "folder", "forward", "grip",
  "home", "info", "install", "link", "lock", "menu", "minus", "more", "motion", "offline", "pause", "pin", "play", "plus",
  "redo", "refresh", "search", "sort", "stop", "theme", "trash", "tune", "undo", "up", "upload", "user", "warning", "window",
};

static rk_view *type_section(void) {
  rk_view *s = section("Barlow, in a few roles.", NULL);
  add(s, text("Title, semibold", RK_TEXT_TITLE));
  add(s, text("a kicker, tracked", RK_TEXT_KICKER));
  add(s, text("Body text at size-s with the body line height. It wraps at the width of its column, as a paragraph does.", RK_TEXT_BODY));
  add(s, text("Strong text", RK_TEXT_STRONG));
  add(s, text("Small text for a tile's label", RK_TEXT_SMALL));
  add(s, text("meta with tabular figures 0123456789", RK_TEXT_META));
  add(s, text("51 icons on a 20px grid", RK_TEXT_KICKER));
  int n = (int)(sizeof ICONS / sizeof ICONS[0]);
  rk_view *rows = column(rk_metric_of(RK_SPACE_S));
  for (int i = 0; i < n; i += 13) {
    rk_view *r = row(14);
    for (int k = i; k < i + 13 && k < n; k++) add(r, rk_icon_create(ICONS[k], RK_ROLE_TEXT));
    add(rows, r);
  }
  add(s, rows);
  return s;
}

/* settings ------------------------------------------------------------------------------------------------------- */

static void paint_now(void *u) {
  (void)u;
  char buf[160];
  snprintf(buf, sizeof buf, "Now: %s, %s density, motion %s.", rk_app_is_dark() ? "dark" : "light",
           rk_metric_of(RK_CONTROL_HEIGHT) < 40 ? "compact" : "comfortable", rk_app_reduced_motion() ? "reduced" : "on");
  rk_label_set_text(D.now, buf);
}

static void set_theme(rk_view *v, void *u) { (void)v; rk_app_set_theme((rk_theme)(intptr_t)u); }
static void set_density(rk_view *v, void *u) { (void)v; rk_app_set_density((rk_density)(intptr_t)u); }
static void set_motion(rk_view *v, void *u) { (void)v; rk_app_set_motion((rk_motion_pref)(intptr_t)u); }
static void set_accent(rk_view *v, void *u) { (void)v; if (u) rk_app_set_accent(0x8a2d6bffu, 0xd99ac6ffu); else rk_app_set_accent(0, 0); }

static void choice(rk_view *s, const char *title, const char *const names[], void (*fn)(rk_view *, void *), int count) {
  add(s, text(title, RK_TEXT_KICKER));
  rk_view *r = row(rk_metric_of(RK_SPACE_S));
  for (int i = 0; i < count; i++) add(r, button(names[i], names[i], NULL, RK_BUTTON_DEFAULT, fn, (void *)(intptr_t)i));
  add(s, r);
}

static rk_view *settings_section(void) {
  rk_view *s = section("Yours to choose.", "Each follows the system until you choose. A change of theme or accent cross-fades; reduced motion makes every transition instant, never skipped.");
  static const char *const theme[] = { "system", "light", "dark" }, *const density[] = { "as the pointer", "comfortable", "compact" };
  static const char *const motion[] = { "as the system", "on", "off" }, *const accent[] = { "ink", "plum" };
  choice(s, "theme", theme, set_theme, 3);
  choice(s, "density", density, set_density, 3);
  choice(s, "motion", motion, set_motion, 3);
  choice(s, "accent", accent, set_accent, 2);
  D.now = text("", RK_TEXT_SMALL);
  add(s, D.now);
  paint_now(NULL);
  rk_app_on_change(paint_now, NULL);
  return s;
}

/* The tour for --shots: what a person would do, with a picture at each moment worth seeing ----------------------- */

static void shot(const char *name) {
  char path[1024];
  snprintf(path, sizeof path, "%s/%s.png", D.shots, name);
  if (rk_window_snapshot(D.window, path)) printf("  %s\n", path); else fprintf(stderr, "could not write %s\n", path);
}

static rk_point center(const char *name) {
  rk_view *v = rk_view_find(rk_controller_view(D.places), name);
  if (!v) { fprintf(stderr, "no view named %s\n", name); return (rk_point){ 0, 0 }; }
  rk_rect b = rk_view_bounds(v);
  return rk_view_to_window(v, (rk_point){ b.w / 2, b.h / 2 });
}

static void pointer(rk_event_type type, rk_point p) {
  rk_event e = { sizeof e, type, 0, p, 0, 0, 0, 0, 0, NULL };
  rk_window_inject(D.window, &e);
}

static void click(const char *name) { rk_point p = center(name); pointer(RK_EVENT_POINTER_DOWN, p); rk_app_run_for(60); pointer(RK_EVENT_POINTER_UP, p); }

static void key(uint32_t k) {
  rk_event e = { sizeof e, RK_EVENT_KEY_DOWN, 0, { 0, 0 }, 0, 0, 0, k, 0, NULL };
  rk_window_inject(D.window, &e);
}

static void tour(void) {
  rk_app_run_for(1200);
  shot("01-rekta");
  rk_point p = center("presses");
  p.x += 50; p.y -= 50;                                       /* near a corner: the tile tilts there */
  pointer(RK_EVENT_POINTER_DOWN, p); rk_app_run_for(120); shot("02-press");
  pointer(RK_EVENT_POINTER_UP, p); rk_app_run_for(140); shot("03-peek");
  rk_app_run_for(600);
  click("pin"); rk_app_run_for(160); shot("04-pin");
  rk_app_run_for(600);
  click("resize"); rk_app_run_for(160); shot("05-reflow");
  rk_app_run_for(600);
  click("notes"); rk_app_run_for(150); shot("06-turn-out");
  rk_app_run_for(260); shot("07-turn-in");
  rk_app_run_for(800); shot("08-place");
  key(RK_KEY_ESCAPE); rk_app_run_for(1200);
  key(RK_KEY_PAGE_DOWN); rk_app_run_for(700);
  live_tick(NULL); live_tick(NULL); live_tick(NULL); rk_app_run_for(110); shot("09-live-flip");
  rk_app_run_for(800);
  key(RK_KEY_PAGE_DOWN); rk_app_run_for(700); shot("10-motion");
  click("toast"); rk_app_run_for(400); shot("11-toast");
  click("sheet"); rk_app_run_for(500); shot("12-sheet");
  key(RK_KEY_ESCAPE); rk_app_run_for(600);
  key(RK_KEY_PAGE_DOWN); rk_app_run_for(700); shot("13-controls");
  key(RK_KEY_PAGE_DOWN); rk_app_run_for(700); shot("14-type");
  key(RK_KEY_PAGE_DOWN); rk_app_run_for(700);
  click("dark"); rk_app_run_for(700); shot("15-dark");
  click("plum"); rk_app_run_for(700); shot("16-accent");
  key(RK_KEY_HOME); rk_app_run_for(900); shot("17-rekta-dark");
  rk_app_quit(0);
}

/* --perf: the loop under load. Each phase prints what drawing cost and how long input took to show; a phase fails
   when a press lands on the wrong view, motion does not come to rest, or views are left over. */

static int perf_fails;

static rk_stats stats(bool reset) { rk_stats s = { .size = sizeof s }; rk_app_stats(&s, reset); return s; }

static void phase(const char *name) {
  rk_stats s = stats(true);
  bool slow = s.max_ms > 33 || (s.frames && s.draw_ms / s.frames > 8);
  printf("%-10s %5u frames  draw %6.2f ms avg %7.2f max  %3u slow  input %5.1f ms max  %5u glyphs  %3u moving%s\n", name, s.frames,
         s.frames ? s.draw_ms / s.frames : 0.0, s.max_ms, s.slow, s.max_latency_ms, s.glyphs, s.animations, slow ? "  SLOW" : "");
}

static void fail(const char *what, const char *detail) { perf_fails++; printf("  FAIL %s: %s\n", what, detail); }

static void tap(rk_point p) { pointer(RK_EVENT_POINTER_DOWN, p); rk_app_run_for(16); pointer(RK_EVENT_POINTER_UP, p); rk_app_run_for(24); }

static void expect_hit(const char *name) {
  rk_view *v = rk_view_find(rk_controller_view(D.places), name);
  rk_view *h = v ? rk_window_view_at(D.window, center(name)) : NULL;
  while (h && h != v) h = rk_view_parent(h);
  if (!h) fail("a press misses", name);
}

static void perf(void) {
  rk_size home = rk_window_size(D.window);
  rk_app_run_for(1500);
  phase("open");
  D.feed = true;
  for (int i = 0; i < 3; i++) { live_tick(NULL); rk_app_run_for(2000); }
  phase("idle");
  /* the window's edge dragged about for two seconds, a frame apart */
  for (int i = 0; i <= 120; i++) {
    float t = i / 120.0f * 6.2831853f;
    rk_window_set_size(D.window, home.w + 520 * sinf(t), home.h + 260 * sinf(2 * t));
    rk_app_run_for(1000.0 / 60.0);
  }
  rk_window_set_size(D.window, home.w + 400, home.h + 200);
  rk_app_run_for(300);
  phase("resize");
  static const char *targets[] = { "presses", "notes", "pin", "unpin", "resize", "restart" };
  for (int i = 0; i < 6; i++) expect_hit(targets[i]);
  /* presses, pins, reflows, turns and section changes, faster than any of them finishes */
  D.feed = false;
  rk_stats before = stats(false);
  static const char *storm[] = { "presses", "pin", "resize", ">", "<", "presses", "unpin", "notes", "^", "pin", "grow", "presses", "toast-plain" };
  for (int i = 0; i < 78; i++) {
    const char *a = storm[i % 13];
    if (*a == '>') key(RK_KEY_PAGE_DOWN); else if (*a == '<') key(RK_KEY_PAGE_UP); else if (*a == '^') key(RK_KEY_ESCAPE);
    else { rk_view *v = rk_view_find(rk_controller_view(D.places), a); if (v) tap(center(a)); }
    rk_app_run_for(24);
  }
  phase("storm");
  key(RK_KEY_ESCAPE); key(RK_KEY_HOME);
  rk_app_run_for(2500);
  rk_stats rest = stats(false);
  if (rest.animations) { char b[64]; snprintf(b, sizeof b, "%u animations still running", rest.animations); fail("motion does not rest", b); }
  phase("settle");
  rk_app_run_for(1000);
  rk_stats quiet = stats(false);
  if (quiet.frames > 2) { char b[64]; snprintf(b, sizeof b, "%u frames drawn with nothing moving", quiet.frames); fail("the loop does not sleep", b); }
  if (quiet.views > before.views + 64) { char b[64]; snprintf(b, sizeof b, "%u views, %u before the storm", quiet.views, before.views); fail("views are left over", b); }
  phase("quiet");
  for (int i = 0; i < 6; i++) expect_hit(targets[i]);
  printf(perf_fails ? "perf: %d failed\n" : "perf: all phases pass\n", perf_fails);
  rk_app_quit(perf_fails ? 1 : 0);
}

/* The app ------------------------------------------------------------------------------------------------------- */

static void add_section(const char *title, rk_view *content) {
  rk_panorama_add(D.home, title, content, 0);
  rk_release(content);                                        /* the panorama holds it */
}

static void launch(void *user) {
  (void)user;
  rk_window *w = rk_window_create(&(rk_window_desc){ .size = sizeof(rk_window_desc), .title = "Rekta core", .width = 1280, .height = 820 });
  if (!w) { rk_app_quit(1); return; }
  D.window = w;
  D.feed = true;
  D.home = rk_panorama_create();
  rk_controller_set_title(D.home, "home");
  add_section("rekta", rekta_section());
  add_section("live", live_section());
  add_section("motion", motion_section());
  add_section("controls", controls_section());
  add_section("type", type_section());
  add_section("settings", settings_section());
  D.places = rk_places_create(D.home);
  rk_window_set_root(w, D.places);
  rk_window_show(w);
  if (D.shots) { tour(); return; }
  if (D.perf) { perf(); return; }
  rk_app_after(2000, true, live_tick, NULL);
}

int main(int argc, char **argv) {
  rk_app_desc desc = { .size = sizeof desc, .name = "Rekta core", .id = "de.rekta.demo", .launch = launch };
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--shots") && i + 1 < argc) { D.shots = argv[++i]; desc.headless = true; }
    else if (!strcmp(argv[i], "--perf")) D.perf = true;
    else if (!strcmp(argv[i], "--screen")) D.screen = true;
  }
  if (D.perf && !D.screen) desc.headless = true;
  int code = rk_app_main(&desc);
  rk_release(sheet_ctl);
  rk_release(D.places); rk_release(D.home);
  rk_release(D.window);
  return code;
}

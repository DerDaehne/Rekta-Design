/* demo.c: every feature of rekta-core in one window, as a panorama you move through sideways.
 *
 *   rekta     tiles: the press, a tile that opens a place (the turn), a number that peeks in, a task whose status
 *             flips when it is done, tiles that come and go and a tile that grows (reflow)
 *   live      a live tile with a sparkline, a status that flips, events that arrive and leave
 *   motion    a sheet from the top edge, toasts (one with undo), a cross-fade
 *   controls  every kind of button, a field, progress, hairline rows, and a custom view drawn with the canvas
 *   lists     a million rows, drawn only where they are in sight, selected the Metro way
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
  rk_controller *places, *home, *sheet;                    /* borrowed: the window owns them */
  rk_view *board, *task, *presses, *grow_tile;             /* rekta */
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

/* rekta: tiles ------------------------------------------------------------------------------------------------- */

static rk_status status_named(const char *s) {
  return !strcmp(s, "failed") ? RK_STATUS_ERROR : !strcmp(s, "waiting") ? RK_STATUS_WARNING : !strcmp(s, "done") ? RK_STATUS_SUCCESS : RK_STATUS_NONE;
}

/* A facet of the release notes: hairline rows, a status tag where the meta says one. */
static rk_controller *facet(const char *title, const char *const rows[][2], int n) {
  rk_view *list = rk_rows();
  for (int i = 0; i < n; i++) {
    rk_status st = status_named(rows[i][1]);
    rk_rows_add(list, rows[i][0], st ? NULL : rows[i][1], st);
  }
  return rk_controller_create_with_view(list, title);
}

/* A place of its own: a title, a line, and a pivot of three facets. */
static rk_controller *notes_place(void) {
  static const char *const summary[][2] = { { "Nightly import", "failed" }, { "Release notes", "waiting" }, { "Deploy to staging", "done" } };
  static const char *const activity[][2] = { { "Import retried", "02:14" }, { "Review requested", "01:20" }, { "Deploy finished", "yesterday 23:05" } };
  static const char *const files[][2] = { { "schema.sql", "4 kB" }, { "import.log", "18 kB" } };

  rk_controller *pivot = rk_pivot();
  rk_pivot_add(pivot, facet("summary", summary, 3));
  rk_pivot_add(pivot, facet("activity", activity, 3));
  rk_pivot_add(pivot, facet("files", files, 2));

  float l = rk_metric_of(RK_SPACE_L), xl = rk_metric_of(RK_SPACE_XL);
  rk_view *page = rk_column();
  rk_view_set_padding(page, (rk_insets){ l, xl, l, xl });
  rk_add(page, rk_label("Release notes", RK_TEXT_TITLE));
  rk_add(page, rk_label("A place of its own. The facets slide from their side; Escape, the back button or Alt+Left turn back home.", RK_TEXT_BODY));
  rk_view *facets = rk_add(page, rk_retain(rk_controller_view(pivot)));
  rk_view_set_grow(facets, 1);
  rk_view_set_user(page, pivot, rk_release);                   /* the page keeps its pivot */
  return rk_controller_create_with_view(page, "release notes");
}

static void open_notes(rk_view *v, void *u) {
  (void)v; (void)u;
  rk_places_go(D.places, notes_place());                       /* the board turns away, the place turns in */
}

static void press_tile(rk_view *v, void *u) {
  (void)u;
  char buf[16];
  snprintf(buf, sizeof buf, "%d", ++D.npresses);
  rk_view_transition(v, RK_TRANSITION_PEEK);                   /* a new number peeks in */
  rk_tile_set_stat(v, buf, D.npresses == 1 ? "tile pressed" : "tiles pressed");
}

static void task_tick(void *u) {
  (void)u;
  if (D.task_done >= 1) return;
  D.task_done = fminf(1, D.task_done + 0.125f);
  char buf[16];
  snprintf(buf, sizeof buf, "%d / 8", (int)lroundf(D.task_done * 8));
  rk_tile_set_progress(D.task, D.task_done);                   /* the bar glides to its value */
  rk_tile_set_stat(D.task, buf, "import steps");
  if (D.task_done >= 1) {
    rk_view_transition(D.task, RK_TRANSITION_FLIP);            /* done: a new status turns the tile over */
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
  rk_animate_begin(RK_MOTION_ENTER);                           /* it builds in after the first tile; the rest make room */
  rk_view *t = rk_insert(D.board, rk_tile(buf, "pinned tile"), 1);
  rk_view_set_name(t, "pinned");
  rk_animate_commit();
}

static void unpin(rk_view *v, void *u) {
  (void)v; (void)u;
  rk_view *t = rk_view_find(D.board, "pinned");
  if (!t) { rk_window_toast(D.window, "Nothing pinned yet.", NULL, NULL, NULL); return; }
  rk_view_set_name(t, NULL);
  rk_animate_begin(RK_MOTION_LEAVE);                           /* it leaves first, then the rest close up */
  rk_view_remove(t);
  rk_animate_commit();
}

static void grow(rk_view *v, void *u) {
  (void)v; (void)u;
  D.big = !D.big;
  rk_animate_begin(RK_MOTION_REFLOW);                          /* it grows from its old size; the grid makes room */
  rk_view_set_span(D.grow_tile, D.big ? 2 : 1, D.big ? 2 : 1);
  rk_animate_commit();
}

/* A tile's context menu: what it is for comes from the tile that opened it. */
static void tile_grow(rk_view *menu, int item, void *user) { (void)menu; (void)item; (void)user; grow(NULL, NULL); }
static void tile_unpin(rk_view *menu, int item, void *user) { (void)menu; (void)item; (void)user; unpin(NULL, NULL); }

static rk_view *tile_menu(void) {
  rk_view *m = rk_menu();
  rk_menu_add(m, "resize", NULL, tile_grow, NULL);
  rk_menu_add(m, "unpin", NULL, tile_unpin, NULL);
  return m;
}

static rk_view *rekta_section(void) {
  rk_view *s = rk_column();
  rk_add(s, rk_label("Tiles, not cards.", RK_TEXT_GROUP));
  rk_add(s, rk_label("A tile's whole area is one fill, and the fill is its status. Press one: it nudges back where you press it. A tile that opens something presses; one that only informs does not.", RK_TEXT_BODY));

  D.board = rk_add(s, rk_tile_grid());
  D.task = rk_add(D.board, rk_tile("0 / 8", "import steps"));
  rk_tile_set_status(D.task, RK_STATUS_INFO, NULL);
  rk_tile_set_message(D.task, "Each step advances the bar; the last one flips the tile.");
  rk_tile_set_progress(D.task, 0);
  rk_view_set_span(D.task, 2, 2);
  rk_view_set_name(D.task, "task");

  rk_view *notes = rk_add(D.board, rk_tile("0.3", "release notes"));
  rk_view_on_activate(notes, open_notes, NULL);
  rk_view_set_name(notes, "notes");

  D.presses = rk_add(D.board, rk_tile("0", "tiles pressed"));
  rk_tile_set_status(D.presses, RK_STATUS_WARNING, NULL);
  rk_view_on_activate(D.presses, press_tile, NULL);
  rk_view_set_name(D.presses, "presses");

  D.grow_tile = rk_add(D.board, rk_tile("12", "builds today"));
  rk_tile_set_status(D.grow_tile, RK_STATUS_SUCCESS, NULL);
  rk_view_set_name(D.grow_tile, "grow");
  rk_view_set_menu(D.grow_tile, tile_menu());                   /* a right click or Shift+F10 */

  rk_add(D.board, rk_tile("51", "icons"));

  rk_view *buttons = rk_add(s, rk_row());
  rk_add(buttons, rk_button("pin a tile", pin, NULL));
  rk_add(buttons, rk_button("unpin it", unpin, NULL));
  rk_add(buttons, rk_button("resize", grow, NULL));
  rk_view *restart = rk_add(buttons, rk_button("restart", restart_task, NULL));
  rk_button_set_kind(restart, RK_BUTTON_QUIET);
  rk_button_set_icon(restart, "refresh");
  return s;
}

/* live --------------------------------------------------------------------------------------------------------- */

static const char *const EVENTS[][2] = {
  { "Build #4190 finished", "done" }, { "A runner joined the pool", "info" }, { "Review waiting on Ada", "waiting" },
  { "Nightly import retried", "info" }, { "Deploy to staging finished", "done" }, { "Disk at 81% on runner 3", "waiting" }, { "Upload failed", "failed" },
};

static rk_status event_status(int i) { rk_status st = status_named(EVENTS[i][1]); return st ? st : RK_STATUS_INFO; }

/* An event arrives at the top; the oldest leaves at the bottom. */
static void push_event(void) {
  int i = D.events_n++ % 7;
  rk_animate_begin(RK_MOTION_ENTER);
  rk_view *row = rk_rows_add(D.events, EVENTS[i][0], NULL, event_status(i));
  rk_insert(D.events, rk_retain(row), 0);                      /* added last; moved to the top */
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
  rk_view_transition(D.live, RK_TRANSITION_PEEK);              /* a new count is news: it peeks in */
  rk_tile_set_stat(D.live, buf, "jobs in the queue");
  if (D.nseries < 24) D.nseries++; else memmove(D.series, D.series + 1, 23 * sizeof(float));
  D.series[D.nseries - 1] = (float)D.queue;
  rk_tile_set_series(D.live, D.series, D.nseries);
  if (D.ticks % 3 == 0) {
    D.status_i = (D.status_i + 1) % 4;
    rk_view_transition(D.status, RK_TRANSITION_FLIP);          /* a new status turns the tile over */
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
  rk_view *s = rk_column();
  rk_add(s, rk_label("Live, and honest about it.", RK_TEXT_GROUP));
  rk_add(s, rk_label("Every two seconds the queue moves and its number peeks in; every six the runners' status flips. Events arrive at the top and the oldest leaves.", RK_TEXT_BODY));

  rk_view *grid = rk_add(s, rk_tile_grid());
  D.live = rk_add(grid, rk_tile("14", "jobs in the queue"));
  rk_tile_set_status(D.live, RK_STATUS_INFO, NULL);
  rk_view_set_span(D.live, 2, 1);
  D.status = rk_add(grid, rk_tile("3", "runners"));
  rk_tile_set_status(D.status, RK_STATUS_INFO, NULL);

  rk_add(s, rk_label("latest events", RK_TEXT_KICKER));
  D.events = rk_add(s, rk_rows());
  for (int i = 0; i < 3; i++) rk_rows_add(D.events, EVENTS[i][0], NULL, event_status(i));
  D.events_n = 3;

  rk_view *feed = rk_add(rk_add(s, rk_row()), rk_button("feed on or off", toggle_feed, NULL));
  rk_button_set_kind(feed, RK_BUTTON_QUIET);
  rk_button_set_icon(feed, "pause");
  return s;
}

/* motion: sheets, toasts, a cross-fade -------------------------------------------------------------------------- */

static void sheet_close(rk_view *v, void *u) { (void)v; (void)u; rk_controller_dismiss(D.sheet); D.sheet = NULL; }

static void sheet_reset(rk_view *v, void *u) {
  sheet_close(v, u);                                           /* it leaves the way it came */
  D.npresses = 0;
  rk_view_transition(D.presses, RK_TRANSITION_PEEK);
  rk_tile_set_stat(D.presses, "0", "tiles pressed");
  rk_window_toast(D.window, "The counter is back at zero.", NULL, NULL, NULL);
}

static void open_sheet(rk_view *v, void *u) {
  (void)v; (void)u;
  rk_view *s = rk_column();
  rk_add(s, rk_label("sheet", RK_TEXT_KICKER));
  rk_add(s, rk_label("Reset the counter?", RK_TEXT_GROUP));
  rk_add(s, rk_label("The pressed tiles go back to zero. Escape or a click on the shading keeps them.", RK_TEXT_BODY));
  rk_view *buttons = rk_add(s, rk_row());
  rk_add(buttons, rk_button("keep it", sheet_close, NULL));
  rk_button_set_kind(rk_add(buttons, rk_button("reset", sheet_reset, NULL)), RK_BUTTON_PRIMARY);
  D.sheet = rk_controller_create_with_view(s, "Reset the counter?");
  rk_controller_present(D.home, D.sheet);
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
  rk_view *s = rk_column();
  rk_add(s, rk_label("Every transition means one thing.", RK_TEXT_GROUP));
  rk_add(s, rk_label("A sheet is a short question from the top edge; a toast says something happened; an undo toast keeps its window open for five seconds.", RK_TEXT_BODY));

  rk_view *buttons = rk_add(s, rk_row());
  rk_button_set_kind(rk_add(buttons, rk_button("open a sheet", open_sheet, NULL)), RK_BUTTON_PRIMARY);
  rk_button_set_icon(rk_add(buttons, rk_button("archive", toast_undo, NULL)), "archive");
  rk_add(buttons, rk_button("a toast", toast_plain, NULL));

  rk_add(s, rk_label("cross-fade", RK_TEXT_KICKER));
  D.fade = rk_add(s, rk_label("Same place, new words: the old ones fade out over the new.", RK_TEXT_BODY));
  rk_button_set_kind(rk_add(rk_add(s, rk_row()), rk_button("new words", fade, NULL)), RK_BUTTON_QUIET);
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
  float k = (float)(1 - pow(1 - t, 3));                        /* eased toward the end */
  float base = b.h - 18, w = (b.w - 6 * 8) / 7;
  static const char *const days[7] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
  rk_canvas_fill_rect(c, (rk_rect){ 0, base, b.w, 1 }, rk_color_of(RK_ROLE_TEXT_LOW));
  for (int i = 0; i < 7; i++) {
    float h = (ch->from[i] + (ch->to[i] - ch->from[i]) * k) * (base - 8);
    rk_canvas_fill_rect(c, (rk_rect){ i * (w + 8), base - h, w, h }, rk_color_of(i == 3 ? RK_ROLE_TEXT : RK_ROLE_TEXT_MEDIUM));
    rk_canvas_text(c, days[i], (rk_point){ i * (w + 8), base + 2 }, RK_TEXT_META, rk_color_of(RK_ROLE_TEXT_HIGH));
  }
  if (t < 1) rk_view_redraw(v);                                /* the next frame, while it moves */
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
  rk_progress_set(D.bar, p);                                   /* the value glides */
}

/* A menu: two options that sort (one is chosen), a command, and one that cannot run yet and says why. */
static int sort_by_name, sort_by_time;

static void sort_by(rk_view *menu, int item, void *user) {
  (void)user;
  rk_menu_set_check(menu, sort_by_name, item == sort_by_name);
  rk_menu_set_check(menu, sort_by_time, item == sort_by_time);
  rk_window_toast(rk_view_window(rk_menu_opener(menu)), item == sort_by_name ? "Sorted by name." : "Sorted by time.", NULL, NULL, NULL);
}

static void export_all(rk_view *menu, int item, void *user) {
  (void)item; (void)user;
  rk_window_toast(rk_view_window(rk_menu_opener(menu)), "Exported to builds.csv.", NULL, NULL, NULL);
}

static rk_view *more_menu(void) {
  rk_view *m = rk_menu();
  rk_menu_add_label(m, "sort by");
  sort_by_name = rk_menu_add(m, "name", NULL, sort_by, NULL);
  sort_by_time = rk_menu_add(m, "time", NULL, sort_by, NULL);
  rk_menu_set_check(m, sort_by_name, 1);
  rk_menu_set_check(m, sort_by_time, 0);
  rk_menu_add_separator(m);
  rk_menu_add(m, "export", "Ctrl+E", export_all, NULL);
  rk_menu_set_disabled(m, rk_menu_add(m, "print (needs a printer)", "Ctrl+P", NULL, NULL), true);
  return m;
}

/* A field says what is wrong and how to put it right, on Enter; typing again lifts the error. */
static void check_mail(rk_view *field, void *user) {
  (void)user;
  const char *at = strchr(rk_field_text(field), '@');
  if (!at || !strchr(at, '.')) { rk_field_set_hint(field, "Add the part after @, such as example.com.", true); return; }
  rk_field_set_hint(field, "We write once a week.", false);
  rk_window_toast(rk_view_window(field), "Saved. The next letter goes out on Monday.", NULL, NULL, NULL);
}

static void mail_typed(rk_view *field, void *user) { (void)user; rk_field_set_hint(field, "Enter saves it.", false); }

static rk_view *controls_section(void) {
  rk_view *s = rk_column();
  rk_add(s, rk_label("Native where it can be, Rekta where it shows.", RK_TEXT_GROUP));

  rk_view *kinds = rk_add(s, rk_row());
  rk_button_set_kind(rk_add(kinds, rk_button("primary", NULL, NULL)), RK_BUTTON_PRIMARY);
  rk_add(kinds, rk_button("default", NULL, NULL));
  rk_button_set_kind(rk_add(kinds, rk_button("quiet", NULL, NULL)), RK_BUTTON_QUIET);
  rk_button_set_disabled(rk_add(kinds, rk_button("disabled", NULL, NULL)), "nothing to do yet");

  static const char *const bar[][2] = { { "edit", "edit" }, { "share", "link" }, { "delete", "trash" }, { NULL, "more" } };
  rk_view *app_bar = rk_add(s, rk_row());
  rk_view_set_gap(app_bar, 2);
  for (int i = 0; i < 4; i++) {
    rk_view *b = rk_add(app_bar, rk_button(bar[i][0], NULL, NULL));
    rk_button_set_kind(b, RK_BUTTON_BAR);
    rk_button_set_icon(b, bar[i][1]);
    if (!bar[i][0]) rk_view_set_menu(b, more_menu());          /* "more" opens a menu */
  }

  rk_add(s, rk_label("a field", RK_TEXT_KICKER));
  rk_view *mail = rk_add(s, rk_field("Mail", "name@example.com"));
  rk_field_set_hint(mail, "Enter saves it.", false);
  rk_field_on_submit(mail, check_mail, NULL);
  rk_field_on_change(mail, mail_typed, NULL);

  rk_add(s, rk_label("progress", RK_TEXT_KICKER));
  D.bar = rk_add(s, rk_progress(0.25f));
  rk_add(s, rk_progress(-1));                                  /* working on it, no end known */
  rk_button_set_kind(rk_add(rk_add(s, rk_row()), rk_button("advance", advance, NULL)), RK_BUTTON_QUIET);

  rk_add(s, rk_label("a custom view, drawn with the canvas", RK_TEXT_KICKER));
  chart *ch = calloc(1, sizeof *ch);
  for (int i = 0; i < 7; i++) ch->to[i] = ch->from[i] = (float)(i == 5 || i == 6 ? 0.15 : 0.6 + 0.05 * i);
  D.chart = rk_add(s, rk_view_create_custom(&chart_class, ch));
  rk_view_set_access(D.chart, RK_ACCESS_IMAGE, "build minutes by weekday");
  rk_button_set_kind(rk_add(rk_add(s, rk_row()), rk_button("new figures", new_figures, NULL)), RK_BUTTON_QUIET);

  rk_add(s, rk_label("rows", RK_TEXT_KICKER));
  rk_view *rows = rk_add(s, rk_rows());
  rk_rows_add(rows, "Nightly import", NULL, RK_STATUS_ERROR);
  rk_rows_add(rows, "Release notes", NULL, RK_STATUS_WARNING);
  rk_rows_add(rows, "schema.sql", "4 kB", RK_STATUS_NONE);
  return s;
}

/* lists: a million rows, only those in sight drawn ---------------------------------------------------------------- */

static rk_view *picked;

static void build_item(int i, rk_list_item *out, void *user) {
  static char title[48], meta[24];
  (void)user;
  snprintf(title, sizeof title, "Build #%d", 1000000 - i);
  out->title = title;
  if (i % 11 == 4) { out->status = RK_STATUS_ERROR; return; }
  if (i % 5 == 1) { out->status = RK_STATUS_WARNING; return; }
  snprintf(meta, sizeof meta, "%d min", 3 + (i * 7) % 19);
  out->meta = meta;
}

static void build_show(rk_view *list, int i, void *user) {
  char text[64];
  (void)user;
  snprintf(text, sizeof text, "Build #%d opens.", 1000000 - i);
  rk_window_toast(rk_view_window(list), text, NULL, NULL, NULL);
}

static void builds_picked(rk_view *list, void *user) {
  char text[64];
  (void)user;
  int n = rk_list_selection_count(list);
  snprintf(text, sizeof text, n ? "%d selected. Escape ends it." : "Ctrl+click selects several.", n);
  rk_label_set_text(picked, text);
}

static rk_view *lists_section(void) {
  rk_view *s = rk_column();
  rk_add(s, rk_label("A million rows, a screenful of work.", RK_TEXT_GROUP));
  rk_add(s, rk_label("Only the rows in sight are drawn; nothing is kept for the others. A click opens one; Ctrl+click, Shift+click and Ctrl+A select.", RK_TEXT_BODY));
  rk_view *list = rk_add(s, rk_list(1000000, build_item, NULL));
  rk_view_set_size(list, 0, 420);
  rk_list_on_show(list, build_show, NULL);
  rk_list_on_change(list, builds_picked, NULL);
  picked = rk_add(s, rk_label("Ctrl+click selects several.", RK_TEXT_SMALL));
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
  rk_view *s = rk_column();
  rk_add(s, rk_label("Barlow, in a few roles.", RK_TEXT_GROUP));
  rk_add(s, rk_label("Title, semibold", RK_TEXT_TITLE));
  rk_add(s, rk_label("a kicker, tracked", RK_TEXT_KICKER));
  rk_add(s, rk_label("Body text at size-s with the body line height. It wraps at the width of its column, as a paragraph does.", RK_TEXT_BODY));
  rk_add(s, rk_label("Strong text", RK_TEXT_STRONG));
  rk_add(s, rk_label("Small text for a tile's label", RK_TEXT_SMALL));
  rk_add(s, rk_label("meta with tabular figures 0123456789", RK_TEXT_META));

  rk_add(s, rk_label("51 icons on a 20px grid", RK_TEXT_KICKER));
  int n = (int)(sizeof ICONS / sizeof ICONS[0]);
  rk_view *lines = rk_add(s, rk_column());
  rk_view_set_gap(lines, rk_metric_of(RK_SPACE_S));
  for (int i = 0; i < n; i += 13) {
    rk_view *line = rk_add(lines, rk_row());
    rk_view_set_gap(line, 14);
    for (int k = i; k < i + 13 && k < n; k++) rk_add(line, rk_icon(ICONS[k], RK_ROLE_TEXT));
  }
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

/* A choice: a kicker and a row of buttons, the button's place in the row as the value its handler gets. */
static void choice(rk_view *s, const char *title, const char *const names[], int count, void (*fn)(rk_view *, void *)) {
  rk_add(s, rk_label(title, RK_TEXT_KICKER));
  rk_view *buttons = rk_add(s, rk_row());
  for (int i = 0; i < count; i++) rk_add(buttons, rk_button(names[i], fn, (void *)(intptr_t)i));
}

static rk_view *settings_section(void) {
  static const char *const theme[] = { "system", "light", "dark" }, *const density[] = { "as the pointer", "comfortable", "compact" };
  static const char *const motion[] = { "as the system", "on", "off" }, *const accent[] = { "ink", "plum" };
  rk_view *s = rk_column();
  rk_add(s, rk_label("Yours to choose.", RK_TEXT_GROUP));
  rk_add(s, rk_label("Each follows the system until you choose. A change of theme or accent cross-fades; reduced motion makes every transition instant, never skipped.", RK_TEXT_BODY));
  choice(s, "theme", theme, 3, set_theme);
  choice(s, "density", density, 3, set_density);
  choice(s, "motion", motion, 3, set_motion);
  choice(s, "accent", accent, 2, set_accent);
  D.now = rk_add(s, rk_label("", RK_TEXT_SMALL));
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

static rk_view *find(const char *name) {
  rk_view *v = rk_view_find(rk_controller_view(D.places), name);
  if (!v) fprintf(stderr, "no view called %s\n", name);
  return v;
}

static rk_point middle(rk_view *v) {
  rk_rect b = rk_view_bounds(v);
  return rk_view_to_window(v, (rk_point){ b.w / 2, b.h / 2 });
}

static void pointer(rk_event_type type, rk_point p) {
  rk_event e = { sizeof e, type, 0, p, 0, 0, 0, 0, 0, NULL };
  rk_window_inject(D.window, &e);
}

/* a press held long enough to be seen, then released */
static void press(const char *name) {
  rk_view *v = find(name);
  if (!v) return;
  rk_point p = middle(v);
  pointer(RK_EVENT_POINTER_DOWN, p);
  rk_app_run_for(60);
  pointer(RK_EVENT_POINTER_UP, p);
}

static void key(uint32_t k) { rk_window_key(D.window, k); }

static void tour(void) {
  rk_app_run_for(1200);
  shot("01-rekta");
  rk_point p = middle(find("presses"));
  p.x += 50; p.y -= 50;                                        /* near a corner: the tile tilts there */
  pointer(RK_EVENT_POINTER_DOWN, p); rk_app_run_for(120); shot("02-press");
  pointer(RK_EVENT_POINTER_UP, p); rk_app_run_for(140); shot("03-peek");
  rk_app_run_for(600);
  press("pin a tile"); rk_app_run_for(160); shot("04-pin");
  rk_app_run_for(600);
  press("resize"); rk_app_run_for(160); shot("05-reflow");
  rk_app_run_for(600);
  press("notes"); rk_app_run_for(150); shot("06-turn-out");
  rk_app_run_for(260); shot("07-turn-in");
  rk_app_run_for(800); shot("08-place");
  key(RK_KEY_ESCAPE); rk_app_run_for(1200);
  key(RK_KEY_PAGE_DOWN); rk_app_run_for(700);
  live_tick(NULL); live_tick(NULL); live_tick(NULL); rk_app_run_for(110); shot("09-live-flip");
  rk_app_run_for(800);
  key(RK_KEY_PAGE_DOWN); rk_app_run_for(700); shot("10-motion");
  press("archive"); rk_app_run_for(400); shot("11-toast");
  press("open a sheet"); rk_app_run_for(500); shot("12-sheet");
  key(RK_KEY_ESCAPE); rk_app_run_for(600);
  key(RK_KEY_PAGE_DOWN); rk_app_run_for(700); shot("13-controls");
  press("more"); rk_app_run_for(300); shot("13-menu");
  key(RK_KEY_ESCAPE); rk_app_run_for(200);
  key(RK_KEY_PAGE_DOWN); rk_app_run_for(700); shot("14-lists");
  key(RK_KEY_PAGE_DOWN); rk_app_run_for(700); shot("15-type");
  key(RK_KEY_PAGE_DOWN); rk_app_run_for(700);
  press("dark"); rk_app_run_for(700); shot("16-dark");
  press("plum"); rk_app_run_for(700); shot("17-accent");
  key(RK_KEY_HOME); rk_app_run_for(900); shot("18-rekta-dark");
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

static void expect_hit(const char *name) {
  rk_view *v = find(name);
  rk_view *h = v ? rk_window_view_at(D.window, middle(v)) : NULL;
  while (h && h != v) h = rk_view_parent(h);
  if (!h) fail("a press misses", name);
}

static void perf(void) {
  static const char *const targets[] = { "presses", "notes", "pin a tile", "unpin it", "resize", "restart" };
  static const char *const storm[] = { "presses", "pin a tile", "resize", ">", "<", "presses", "unpin it", "notes", "^", "pin a tile", "grow", "presses", "a toast" };
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
  for (int i = 0; i < 6; i++) expect_hit(targets[i]);

  /* the wheel turned in quick steps, sideways and back, then section by section from the keyboard */
  D.feed = false;
  rk_app_run_for(600);
  stats(true);
  rk_point top = { rk_window_size(D.window).w / 2, 40 };
  for (int dir = 1; dir >= -1; dir -= 2)
    for (int i = 0; i < 14; i++) {
      rk_event e = { sizeof e, RK_EVENT_WHEEL, 0, top, 0, 0, 48.0f * dir, 0, 0, NULL };
      rk_window_inject(D.window, &e);
      rk_app_run_for(i < 10 ? 40 : 120);
    }
  rk_app_run_for(600);
  for (int i = 0; i < 5; i++) { key(RK_KEY_PAGE_DOWN); rk_app_run_for(300); }
  key(RK_KEY_HOME);
  rk_app_run_for(900);
  phase("scroll");

  /* presses, pins, reflows, turns and section changes, faster than any of them finishes */
  rk_stats before = stats(false);
  for (int i = 0; i < 78; i++) {
    const char *a = storm[i % 13];
    if (*a == '>') key(RK_KEY_PAGE_DOWN);
    else if (*a == '<') key(RK_KEY_PAGE_UP);
    else if (*a == '^') key(RK_KEY_ESCAPE);
    else { rk_view *v = rk_view_find(rk_controller_view(D.places), a); if (v) rk_view_tap(v); }
    rk_app_run_for(40);
  }
  phase("storm");

  key(RK_KEY_ESCAPE); key(RK_KEY_HOME);
  while (rk_view_find(D.board, "pinned")) unpin(NULL, NULL);   /* the board as it was, its buttons in sight again */
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

static void launch(void *user) {
  (void)user;
  rk_window_desc wd = { .size = sizeof wd, .title = "Rekta core", .width = 1280, .height = 820 };
  D.window = rk_window_create(&wd);
  if (!D.window) { rk_app_quit(1); return; }

  D.home = rk_panorama();
  rk_controller_set_title(D.home, "home");
  rk_panorama_add(D.home, "rekta", rekta_section(), 0);
  rk_panorama_add(D.home, "live", live_section(), 0);
  rk_panorama_add(D.home, "motion", motion_section(), 0);
  rk_panorama_add(D.home, "controls", controls_section(), 0);
  rk_panorama_add(D.home, "lists", lists_section(), 0);
  rk_panorama_add(D.home, "type", type_section(), 0);
  rk_panorama_add(D.home, "settings", settings_section(), 0);
  D.places = rk_places(D.home);                                /* places keep home, the window keeps places */
  rk_window_show(D.window, D.places);
  rk_release(D.window);                                        /* an open window lives until it is closed */

  D.feed = true;
  if (D.shots) { tour(); return; }
  if (D.perf) { perf(); return; }
  rk_app_after(2000, true, live_tick, NULL);
}

int main(int argc, char **argv) {
  rk_app_desc app = { .size = sizeof app, .name = "Rekta core", .id = "de.rekta.demo", .launch = launch };
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--shots") && i + 1 < argc) { D.shots = argv[++i]; app.headless = true; }
    else if (!strcmp(argv[i], "--perf")) D.perf = true;
    else if (!strcmp(argv[i], "--screen")) D.screen = true;
  }
  if (D.perf && !D.screen) app.headless = true;
  return rk_app_main(&app);
}

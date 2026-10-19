/* demo.c: every feature of rekta-core in one window, as a panorama you move through sideways - declared: one build
 * says what the window shows from the app's state below, and the handlers only change that state.
 *
 *   rekta     tiles: the press, a tile that opens a place (the turn), a number that peeks in, a task whose status
 *             flips when it is done, tiles that come and go and a tile that grows (reflow)
 *   live      a live tile with a sparkline, a status that flips, events that arrive and leave
 *   motion    a sheet from the top edge, toasts (one with undo), a cross-fade
 *   controls  every kind of button, a field, progress, hairline rows, and a custom view drawn with the canvas
 *   lists     a million rows, drawn only where they are in sight, selected the Metro way; a table
 *   desk      a tree beside its detail, rows to put in order, a place to drop pictures
 *   type      the text styles and all 51 icons
 *   settings  theme, density, motion and accent, as the system or as chosen
 *
 * Keys: Tab moves focus, Enter or Space presses, Page Up and Page Down go by section, Escape goes back.
 * `demo --shots DIR` runs a tour without a screen and writes what it sees as PNGs.
 * `demo --perf` puts the loop under load and measures it (resizing, a storm of presses, settling), without a screen;
 * `demo --perf --screen` does it in a real window. RK_HUD=1 shows the numbers in any run, RK_TRACE=1 prints frames. */
#include "../include/rekta_test.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NBUILDS 500

/* The app's state: everything the window shows comes from here. */
static struct {
  rk_window *window;
  /* rekta */
  int presses, pins[8], npins, next_pin, restarts;
  float task_done;
  bool big, notes;
  /* live */
  int queue, status, ticks, events[4], nevents, next_event;
  float series[24]; int nseries;
  bool feed;
  /* motion */
  bool sheet;
  int inbox, fades;
  /* controls */
  float bar;
  double volume, copies;
  int size, sort_by, figures;
  rk_day day;
  bool mail_error, mail_saved;
  /* lists */
  int picked, order[NBUILDS], sort_column, sort_dir;
  char notes_text[NBUILDS][32];
  /* desk */
  int file, steps[4], shown, dir; uint32_t picks;   /* the file in the detail, the way the last pick went, how many */
  bool detail, open_src, open_ui, open_docs;
  /* settings */
  int theme, density, motion, accent, digest;
  bool notify, sounds;
  /* how it runs */
  const char *shots;
  bool perf, screen;
} D = {
  .queue = 14, .inbox = 12, .bar = 0.25f, .volume = 25, .copies = 2, .size = 1, .day = { 2026, 10, 13 },
  .file = 4, .shown = 4, .steps = { 0, 1, 2, 3 }, .open_src = true, .open_ui = true, .notify = true, .digest = 1,
  .sort_column = -1, .sort_dir = 1, .feed = true,
};

static void toast(const char *text) { rk_window_toast(D.window, text, NULL, NULL, NULL); }

static rk_status status_named(const char *s) {
  return !strcmp(s, "failed") ? RK_STATUS_ERROR : !strcmp(s, "waiting") ? RK_STATUS_WARNING : !strcmp(s, "done") ? RK_STATUS_SUCCESS : RK_STATUS_NONE;
}

/* rekta: tiles ------------------------------------------------------------------------------------------------- */

static void open_notes(rk_view *v, void *u) { (void)v; (void)u; D.notes = true; }   /* the board turns away, the place turns in */
static void press_tile(rk_view *v, void *u) { (void)v; (void)u; D.presses++; }       /* the new number peeks in */

static void task_tick(void) { if (D.task_done < 1) D.task_done = fminf(1, D.task_done + 0.125f); }
static void restart_task(rk_view *v, void *u) { (void)v; (void)u; D.task_done = 0; D.restarts++; }   /* it turns over again */

static void pin(rk_view *v, void *u) {
  (void)v; (void)u;
  if (D.npins == 8) return;
  memmove(D.pins + 1, D.pins, (size_t)D.npins * sizeof *D.pins);   /* it builds in after the first tile; the rest make room */
  D.pins[0] = ++D.next_pin;
  D.npins++;
}
static void unpin(rk_view *v, void *u) {
  (void)v; (void)u;
  if (!D.npins) { toast("Nothing pinned yet."); return; }
  memmove(D.pins, D.pins + 1, (size_t)--D.npins * sizeof *D.pins);   /* it leaves first, then the rest close up */
}
static void grow(rk_view *v, void *u) { (void)v; (void)u; D.big = !D.big; }   /* it grows from its old size; the grid makes room */

static void tile_menu(void *u) {                                     /* a right click or Shift+F10 */
  (void)u;
  RK_ITEM("resize", .on_press = grow);
  RK_ITEM("unpin", .on_press = unpin);
}

static void rekta_section(void) {
  bool done = D.task_done >= 1;
  RK_LABEL("Tiles, not cards.", .style = RK_TEXT_GROUP);
  RK_LABEL("A tile's whole area is one fill, and the fill is its status. Press one: it nudges back where you press it. A tile that opens something presses; one that only informs does not.");
  RK_TILES() {
    RK_TILE(rk_ui_fmt("%d / 8", (int)lroundf(D.task_done * 8)), .key = "task", .label = "import steps", .status = done ? RK_STATUS_SUCCESS : RK_STATUS_INFO, .span = { 2, 2 },
            .message = done ? "Imported. Stillness means done." : "Each step advances the bar; the last one flips the tile.",
            .busy = !done, .progress = done ? 0 : D.task_done, .pulse = (uint32_t)D.restarts);
    for (int i = 0; i < D.npins; i++) RK_TILE(rk_ui_fmt("%d", D.pins[i]), RK_ID(D.pins[i]), .label = "pinned tile");
    RK_TILE("0.3", .key = "notes", .label = "release notes", .on_press = open_notes);
    RK_TILE(rk_ui_fmt("%d", D.presses), .key = "presses", .label = D.presses == 1 ? "tile pressed" : "tiles pressed", .status = RK_STATUS_WARNING, .on_press = press_tile);
    RK_TILE("12", .key = "grow", .label = "builds today", .status = RK_STATUS_SUCCESS, .span = { D.big ? 2 : 1, D.big ? 2 : 1 }, .menu = tile_menu);
    RK_TILE("51", .label = "icons");
  }
  RK_HSTACK() {
    RK_BUTTON("pin a tile", .on_press = pin);
    RK_BUTTON("unpin it", .on_press = unpin);
    RK_BUTTON("resize", .on_press = grow);
    RK_BUTTON("restart", .button = RK_BUTTON_QUIET, .icon = "refresh", .on_press = restart_task);
  }
}

/* A place of its own: a title, a line, and a pivot of three facets. */
static void facet(const char *const rows[][2], int n) {
  RK_ROWS() for (int i = 0; i < n; i++) {
    rk_status st = status_named(rows[i][1]);
    RK_ROW(rows[i][0], .meta = st ? NULL : rows[i][1], .status = st);
  }
}

static void notes_place(void) {
  static const char *const summary[][2] = { { "Nightly import", "failed" }, { "Release notes", "waiting" }, { "Deploy to staging", "done" } };
  static const char *const activity[][2] = { { "Import retried", "02:14" }, { "Review requested", "01:20" }, { "Deploy finished", "yesterday 23:05" } };
  static const char *const files[][2] = { { "schema.sql", "4 kB" }, { "import.log", "18 kB" } };
  RK_LABEL("Release notes", .style = RK_TEXT_TITLE);
  RK_LABEL("A place of its own. The facets slide from their side; Escape, the back button or Alt+Left turn back home.");
  RK_PIVOT() {
    RK_PAGE("summary") facet(summary, 3);
    RK_PAGE("activity") facet(activity, 3);
    RK_PAGE("files") facet(files, 2);
  }
}

/* live --------------------------------------------------------------------------------------------------------- */

static const char *const EVENTS[][2] = {
  { "Build #4190 finished", "done" }, { "A runner joined the pool", "info" }, { "Review waiting on Ada", "waiting" },
  { "Nightly import retried", "info" }, { "Deploy to staging finished", "done" }, { "Disk at 81% on runner 3", "waiting" }, { "Upload failed", "failed" },
};
static rk_status event_status(int i) { rk_status st = status_named(EVENTS[i][1]); return st ? st : RK_STATUS_INFO; }

/* An event arrives at the top; the oldest leaves at the bottom. */
static void push_event(void) {
  memmove(D.events + 1, D.events, 3 * sizeof *D.events);
  D.events[0] = D.next_event++;
  if (D.nevents < 4) D.nevents++;
}

static void live_tick(void *u) {
  (void)u;
  if (!D.feed) return;
  D.ticks++;
  D.queue = (D.queue + 3 + D.ticks * 7) % 40;                  /* a new count is news: it peeks in */
  if (D.nseries < 24) D.nseries++; else memmove(D.series, D.series + 1, 23 * sizeof(float));
  D.series[D.nseries - 1] = (float)D.queue;
  if (D.ticks % 3 == 0) D.status = (D.status + 1) % 4;          /* a new status turns the tile over */
  if (D.ticks % 2 == 0) push_event();
  task_tick();
}

static void toggle_feed(rk_view *v, void *u) {
  (void)v; (void)u;
  D.feed = !D.feed;
  toast(D.feed ? "Feed on: the figures are live." : "Feed off: the figures hold still.");
}

static void live_section(void) {
  static const rk_status order[] = { RK_STATUS_INFO, RK_STATUS_WARNING, RK_STATUS_SUCCESS, RK_STATUS_ERROR };
  RK_LABEL("Live, and honest about it.", .style = RK_TEXT_GROUP);
  RK_LABEL("Every two seconds the queue moves and its number peeks in; every six the runners' status flips. Events arrive at the top and the oldest leaves.");
  RK_TILES() {
    RK_TILE(rk_ui_fmt("%d", D.queue), .key = "live", .label = "jobs in the queue", .status = RK_STATUS_INFO, .span = { 2, 1 }, .series = D.series, .nseries = D.nseries);
    RK_TILE("3", .key = "runners", .label = "runners", .status = order[D.status]);
  }
  RK_LABEL("latest events", .style = RK_TEXT_KICKER);
  RK_ROWS() for (int i = 0; i < D.nevents; i++) {
    int e = D.events[i] % 7;
    RK_ROW(EVENTS[e][0], .id = (uint64_t)D.events[i] + 1, .status = event_status(e));
  }
  RK_HSTACK() RK_BUTTON("feed on or off", .button = RK_BUTTON_QUIET, .icon = "pause", .on_press = toggle_feed);
}

/* motion: sheets, toasts, a cross-fade -------------------------------------------------------------------------- */

static void open_sheet(rk_view *v, void *u) { (void)v; (void)u; D.sheet = true; }
static void keep_counter(rk_view *v, void *u) { (void)v; (void)u; D.sheet = false; }   /* it leaves the way it came */
static void reset_counter(rk_view *v, void *u) {
  (void)v; (void)u;
  D.sheet = false;
  D.presses = 0;
  toast("The counter is back at zero.");
}

/* Archiving is an action in the window's history: its toast, Ctrl+Z and the undo button all take it back. */
static void archive_one(void *u) { (void)u; D.inbox--; }
static void unarchive_one(void *u) { (void)u; D.inbox++; }
static void archive(rk_view *v, void *u) {
  (void)v; (void)u;
  if (!D.inbox) { toast("The inbox is empty."); return; }
  rk_window_do(D.window, &(rk_action){ .size = sizeof(rk_action), .label = "One letter archived.", .run = archive_one, .undo = unarchive_one });
}
static void plain_toast(rk_view *v, void *u) { (void)v; (void)u; toast("Something happened. A toast says so, once."); }
static void fade(rk_view *v, void *u) { (void)v; (void)u; D.fades++; }

static void motion_section(void) {
  static const char *const lines[] = { "Same place, new words: the old ones fade out over the new.", "A cross-fade changes the whole look, and moves nothing.", "Theme and accent changes cross-fade the window this way." };
  RK_LABEL("Every transition means one thing.", .style = RK_TEXT_GROUP);
  RK_LABEL("A sheet is a short question from the top edge; a toast says something happened; archiving goes into the window's history: its toast, Ctrl+Z and Ctrl+Y take it back and forth.");
  RK_HSTACK() {
    RK_BUTTON("open a sheet", .button = RK_BUTTON_PRIMARY, .on_press = open_sheet);
    RK_BUTTON("archive", .icon = "archive", .on_press = archive);
    RK_BUTTON("a toast", .on_press = plain_toast);
  }
  RK_LABEL(rk_ui_fmt("Inbox: %d letters.", D.inbox), .style = RK_TEXT_SMALL);
  RK_LABEL("cross-fade", .style = RK_TEXT_KICKER);
  RK_LABEL(lines[D.fades % 3], .change = RK_UI_CROSSFADE);
  RK_HSTACK() RK_BUTTON("new words", .button = RK_BUTTON_QUIET, .on_press = fade);
  RK_SHEET("Reset the counter?", .open = &D.sheet) {
    RK_LABEL("sheet", .style = RK_TEXT_KICKER);
    RK_LABEL("Reset the counter?", .style = RK_TEXT_GROUP);
    RK_LABEL("The pressed tiles go back to zero. Escape or a click on the shading keeps them.");
    RK_HSTACK() {
      RK_BUTTON("keep it", .on_press = keep_counter);
      RK_BUTTON("reset", .button = RK_BUTTON_PRIMARY, .on_press = reset_counter);
    }
  }
}

/* controls: buttons, progress, rows, a custom view --------------------------------------------------------------- */

/* A custom view, made by hand: a bar chart drawn with the canvas. New figures travel to their place with the morph
   motion: the view's own value glides one further (from base to base + 1), and draw reads how far it has come. */
typedef struct chart { float from[7], to[7], base; int figures; } chart;

static void chart_draw(rk_view *v, rk_canvas *c, void *state) {
  chart *ch = state;
  rk_rect b = rk_view_bounds(v);
  float k = rk_view_value(v) - ch->base;                       /* 0 at the old figures, 1 at the new */
  k = k < 0 ? 0 : k > 1 ? 1 : k;
  float base = b.h - 18, w = (b.w - 6 * 8) / 7;
  static const char *const days[7] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
  rk_canvas_fill_rect(c, (rk_rect){ 0, base, b.w, 1 }, rk_color_of(RK_ROLE_TEXT_LOW));
  for (int i = 0; i < 7; i++) {
    float h = (ch->from[i] + (ch->to[i] - ch->from[i]) * k) * (base - 8);
    rk_canvas_fill_rect(c, (rk_rect){ i * (w + 8), base - h, w, h }, rk_color_of(i == 3 ? RK_ROLE_TEXT : RK_ROLE_TEXT_MEDIUM));
    rk_canvas_text(c, days[i], (rk_point){ i * (w + 8), base + 2 }, RK_TEXT_META, rk_color_of(RK_ROLE_TEXT_HIGH));
  }
}
static rk_size chart_measure(rk_view *v, rk_size available, void *state) {
  (void)v; (void)state;
  return (rk_size){ isfinite(available.w) ? available.w : 400, 140 };
}
static const rk_view_class chart_class = { sizeof(rk_view_class), "demo_chart", chart_draw, chart_measure, NULL, NULL, free };

static rk_view *chart_make(void *u) {
  (void)u;
  chart *ch = calloc(1, sizeof *ch);
  for (int i = 0; i < 7; i++) ch->to[i] = ch->from[i] = (float)(i == 5 || i == 6 ? 0.15 : 0.6 + 0.05 * i);
  rk_view *v = rk_view_create_custom(&chart_class, ch);
  rk_view_set_access(v, RK_ACCESS_IMAGE, "build minutes by weekday");
  return v;
}
/* at each build: new figures, when the state has them, from where the bars stand now - even mid-way */
static void chart_update(rk_view *v, void *u) {
  (void)u;
  chart *ch = rk_view_state(v);
  if (ch->figures == D.figures) return;
  ch->figures = D.figures;
  float now = rk_view_value(v), k = now - ch->base;
  k = k < 0 ? 0 : k > 1 ? 1 : k;
  for (int i = 0; i < 7; i++) {
    ch->from[i] += (ch->to[i] - ch->from[i]) * k;
    ch->to[i] = 0.2f + 0.8f * (float)((i * 37 + D.ticks * 53 + D.fades * 29 + D.presses * 17) % 100) / 100;
  }
  ch->base = now;                                              /* on from here: no pause, no jump */
  rk_animate_begin(RK_MOTION_MORPH);
  rk_view_animate_value(v, now + 1);
  rk_animate_commit();
}
static void new_figures(rk_view *v, void *u) { (void)v; (void)u; D.figures++; }

static void slide_bar(rk_view *v, void *u) { (void)v; (void)u; D.bar = (float)(D.volume / 100); }
static void advance(rk_view *v, void *u) { (void)v; (void)u; D.bar = D.bar >= 1 ? 0 : D.bar + 0.25f; }   /* the value glides */

/* A menu: two options that sort (one is chosen), a command, and one that cannot run yet and says why. */
static void sorted(rk_view *v, void *u) { (void)v; (void)u; toast(D.sort_by ? "Sorted by time." : "Sorted by name."); }
static void export_all(rk_view *v, void *u) { (void)v; (void)u; toast("Exported to builds.csv."); }
static void more_menu(void *u) {
  (void)u;
  RK_HEADING("sort by");
  RK_ITEM("name", .chosen = &D.sort_by, .value = 0, .on_press = sorted);
  RK_ITEM("time", .chosen = &D.sort_by, .value = 1, .on_press = sorted);
  RK_SEPARATOR();
  RK_ITEM("export", .shortcut = "Ctrl+E", .on_press = export_all);
  RK_ITEM("print (needs a printer)", .shortcut = "Ctrl+P", .disabled = true);
}

static void save_notes(rk_view *notes, void *u) { (void)u; toast(*rk_field_text(notes) ? "Notes saved." : "Nothing to save yet."); }

/* A field says what is wrong and how to put it right, on Enter; typing again lifts the error. */
static void check_mail(rk_view *field, void *u) {
  (void)u;
  const char *at = strchr(rk_field_text(field), '@');
  D.mail_error = !at || !strchr(at, '.');
  D.mail_saved = !D.mail_error;
  if (D.mail_saved) toast("Saved. The next letter goes out on Monday.");
}
static void mail_typed(rk_view *field, void *u) { (void)field; (void)u; D.mail_error = D.mail_saved = false; }

static void controls_section(void) {
  static const char *const sizes[] = { "small", "medium", "large", "a whole wall" };
  static const char *const cities[] = { "Amsterdam", "Berlin", "Bern", "Bonn", "Copenhagen", "Hamburg", "Lisbon", "Munich", "Vienna" };
  static const char *const bar[][2] = { { "edit", "edit" }, { "share", "link" }, { "delete", "trash" } };

  RK_LABEL("Native where it can be, Rekta where it shows.", .style = RK_TEXT_GROUP);
  RK_HSTACK() {
    RK_BUTTON("primary", .button = RK_BUTTON_PRIMARY);
    RK_BUTTON("default");
    RK_BUTTON("quiet", .button = RK_BUTTON_QUIET);
    RK_BUTTON("disabled", .disabled = true, .tooltip = "nothing to do yet");
  }
  RK_HSTACK(.gap = 2) {
    for (int i = 0; i < 3; i++) RK_BUTTON(bar[i][0], .button = RK_BUTTON_BAR, .icon = bar[i][1]);
    RK_BUTTON(NULL, .button = RK_BUTTON_BAR, .icon = "more", .menu = more_menu);   /* "more" opens a menu */
  }

  RK_LABEL("a field", .style = RK_TEXT_KICKER);
  RK_FIELD("Mail", .placeholder = "name@example.com", .on_submit = check_mail, .on_change = mail_typed,
           .hint = D.mail_saved ? "We write once a week." : "Enter saves it.", .error = D.mail_error ? "Add the part after @, such as example.com." : NULL);
  RK_FIELD("Notes", .placeholder = "Several lines: Enter breaks one, Ctrl+Enter saves them.", .lines = 3, .on_submit = save_notes);

  RK_LABEL("progress", .style = RK_TEXT_KICKER);
  RK_PROGRESS(.key = "bar", .progress = D.bar);
  RK_PROGRESS(.busy = true);                                   /* working on it, no end known */
  RK_HSTACK() RK_BUTTON("advance", .button = RK_BUTTON_QUIET, .on_press = advance);
  RK_LABEL("a select, and a field that suggests", .style = RK_TEXT_KICKER);
  RK_HSTACK() {
    RK_SELECT(RK_OPTIONS(sizes), .chosen = &D.size);
    RK_DATE(.day = &D.day);
  }
  RK_FIELD("City", .placeholder = "Type, or Down for the list", RK_OPTIONS(cities));
  RK_LABEL("a slider sets the bar; a stepper counts", .style = RK_TEXT_KICKER);
  RK_SLIDER(.min = 0, .max = 100, .step = 5, .number = &D.volume, .on_change = slide_bar);
  RK_HSTACK(.cross = RK_ALIGN_CENTER) {
    RK_STEPPER(.min = 1, .max = 20, .step = 1, .number = &D.copies);
    RK_LABEL(rk_ui_fmt(D.copies == 1 ? "%d copy, printed on Monday." : "%d copies, printed on Monday.", (int)D.copies));
  }

  RK_LABEL("a custom view, drawn with the canvas", .style = RK_TEXT_KICKER);
  RK_HOST(.key = "chart", .make = chart_make, .update = chart_update);
  RK_HSTACK() RK_BUTTON("new figures", .button = RK_BUTTON_QUIET, .on_press = new_figures);

  RK_LABEL("rows", .style = RK_TEXT_KICKER);
  RK_ROWS() {
    RK_ROW("Nightly import", .status = RK_STATUS_ERROR);
    RK_ROW("Release notes", .status = RK_STATUS_WARNING);
    RK_ROW("schema.sql", .meta = "4 kB");
  }
}

/* lists: a million rows, only those in sight drawn; a table the app sorts ------------------------------------------ */

static void build_item(int i, rk_list_item *out, void *u) {
  static char title[48], meta[24];
  (void)u;
  snprintf(title, sizeof title, "Build #%d", 1000000 - i);
  out->title = title;
  if (i % 11 == 4) { out->status = RK_STATUS_ERROR; return; }
  if (i % 5 == 1) { out->status = RK_STATUS_WARNING; return; }
  snprintf(meta, sizeof meta, "%d min", 3 + (i * 7) % 19);
  out->meta = meta;
}
static void build_show(rk_view *list, int i, void *u) {
  char text[64];
  (void)list; (void)u;
  snprintf(text, sizeof text, "Build #%d opens.", 1000000 - i);
  toast(text);
}
static void builds_picked(rk_view *list, void *u) { (void)u; D.picked = rk_list_selection_count(list); }

static int build_minutes(int i) { return 3 + (i * 37) % 41; }
static const char *build_status(int i) { return i % 11 == 4 ? "failed" : i % 5 == 1 ? "waiting" : "done"; }

static const char *build_cell(int row, int column, void *u) {
  static char text[32];
  int i = D.order[row];
  (void)u;
  if (column == 0) { snprintf(text, sizeof text, "Build #%d", 5000 - i); return text; }
  if (column == 1) return build_status(i);
  if (column == 3) return D.notes_text[i];
  snprintf(text, sizeof text, "%d min", build_minutes(i));
  return text;
}

/* Editing a note is an action in the window's history: Ctrl+Z (or the toast) takes it back. */
typedef struct note_edit { int build; char before[32], after[32]; } note_edit;
static void note_run(void *u) { note_edit *n = u; memcpy(D.notes_text[n->build], n->after, sizeof n->after); }
static void note_undo(void *u) { note_edit *n = u; memcpy(D.notes_text[n->build], n->before, sizeof n->before); }
static const char *note_commit(rk_view *table, int row, int column, const char *value, void *u) {
  (void)table; (void)column; (void)u;
  if (strlen(value) >= sizeof D.notes_text[0]) return "A note fits in 31 characters; this one is longer.";
  note_edit *n = malloc(sizeof *n);
  if (!n) return "There was no memory for the note.";
  n->build = D.order[row];
  memcpy(n->before, D.notes_text[n->build], sizeof n->before);
  snprintf(n->after, sizeof n->after, "%s", value);
  rk_window_do(D.window, &(rk_action){ .size = sizeof(rk_action), .label = "Note saved.", .run = note_run, .undo = note_undo, .user = n, .free_user = free });
  return NULL;
}

static int by_column(const void *a, const void *b) {
  int x = *(const int *)a, y = *(const int *)b, d;
  if (D.sort_column == 0) d = y - x;                           /* build numbers count down with i */
  else if (D.sort_column == 1) d = strcmp(build_status(x), build_status(y));
  else d = build_minutes(x) - build_minutes(y);
  return (d ? d : x - y) * D.sort_dir;
}
static void table_show(rk_view *table, int row, void *u) {
  char text[64];
  (void)table; (void)u;
  snprintf(text, sizeof text, "Build #%d opens.", 5000 - D.order[row]);
  toast(text);
}
static void sort_builds(rk_view *table, int column, rk_sort sort, void *u) {
  (void)table; (void)u;
  D.sort_column = column; D.sort_dir = sort == RK_SORT_ASCENDING ? 1 : -1;
  qsort(D.order, NBUILDS, sizeof *D.order, by_column);         /* the rows are drawn again in the new order */
}

static void lists_section(void) {
  RK_LABEL("A million rows, a screenful of work.", .style = RK_TEXT_GROUP);
  RK_LABEL("Only the rows in sight are drawn; nothing is kept for the others. A click opens one; Ctrl+click, Shift+click and Ctrl+A select.");
  RK_LIST(.count = 1000000, .item = build_item, .height = 300, .on_select = build_show, .on_change = builds_picked);
  RK_LABEL(rk_ui_fmt(D.picked ? "%d selected. Escape ends it." : "Ctrl+click selects several.", D.picked), .style = RK_TEXT_SMALL);
  RK_LABEL("a table: click a head to sort, drag its edge to widen; double click a note to edit it", .style = RK_TEXT_KICKER);
  RK_TABLE(.count = NBUILDS, .cell = build_cell, .height = 320, .on_sort = sort_builds, .on_edit = note_commit, .on_select = table_show) {
    RK_COLUMN("build", .width = 140, .flags = RK_COLUMN_SORTABLE);
    RK_COLUMN("status", .width = 100, .flags = RK_COLUMN_SORTABLE);
    RK_COLUMN("time", .width = 80, .flags = RK_COLUMN_NUMERIC | RK_COLUMN_SORTABLE);
    RK_COLUMN("note", .flags = RK_COLUMN_EDITABLE);
  }
}

/* desk: a tree beside its detail in a split, rows to put in order, a place to drop pictures ----------------------- */

static const char *const FILES[] = { "src", "ui", "button.c", "field.c", "split.c", "tree.c", "core.c", "docs", "motion.md", "navigation.md", "README.md" };
static const char *const STEPS[] = { "Check out", "Build", "Test", "Package" };

/* compact: the detail turns in; sideways another file slides in from the side it lies on in the tree */
static void desk_show(rk_view *tree, void *u) {
  (void)tree; (void)u;
  if (D.file != D.shown) { D.dir = D.file < D.shown ? -1 : 1; D.picks++; D.shown = D.file; }
  D.detail = true;
}
static void steps_moved(rk_view *rows, int from, int to, void *u) {
  (void)rows; (void)u;
  int s = D.steps[from];                                       /* the app keeps the new order */
  if (from < to) memmove(D.steps + from, D.steps + from + 1, (size_t)(to - from) * sizeof *D.steps);
  else memmove(D.steps + to + 1, D.steps + to, (size_t)(from - to) * sizeof *D.steps);
  D.steps[to] = s;
  char msg[64];
  snprintf(msg, sizeof msg, "Step %d is now step %d.", from + 1, to + 1);
  toast(msg);
}
static void pictures_dropped(rk_view *drop, const char *const *taken, int nt, const char *const *refused, int nr, void *u) {
  (void)drop; (void)taken; (void)refused; (void)u;
  char msg[96];
  snprintf(msg, sizeof msg, nr ? "%d pictures taken, %d other files left." : "%d pictures taken.", nt, nr);
  toast(msg);
}

#define FILE(i) FILES[i], .value = (i)               /* a file: picking it writes its number to the tree's .chosen */

static void desk_section(void) {
  RK_LABEL("A desk: things beside their detail.", .style = RK_TEXT_GROUP);
  RK_SPLIT("files", .open = &D.detail, .pulse = D.picks, .reveal = D.dir, .height = 460) {
    RK_TREE(.chosen = &D.file, .on_press = desk_show) {
      RK_ITEM(FILE(0), .open = &D.open_src) {
        RK_ITEM(FILE(1), .open = &D.open_ui) { RK_ITEM(FILE(2)); RK_ITEM(FILE(3)); RK_ITEM(FILE(4)); }
        RK_ITEM(FILE(5));
        RK_ITEM(FILE(6), .meta = "failed", .status = RK_STATUS_ERROR);
      }
      RK_ITEM(FILE(7), .open = &D.open_docs) { RK_ITEM(FILE(8)); RK_ITEM(FILE(9)); }
      RK_ITEM(FILE(10), .meta = "2 kB");
    }
    RK_VSTACK() {
      RK_LABEL(FILES[D.file], .style = RK_TEXT_GROUP);
      RK_LABEL("steps: drag a grip, or Space then Up and Down", .style = RK_TEXT_KICKER);
      RK_ROWS(.on_move = steps_moved) for (int i = 0; i < 4; i++) RK_ROW(STEPS[D.steps[i]], RK_ID(D.steps[i]));
      RK_DROP("Drop pictures here", .hint = "or choose them · nothing leaves your device", .accept = "image/*", .on_files = pictures_dropped);
    }
  }
}

/* type: text styles and icons ------------------------------------------------------------------------------------ */

static const char *const ICONS[] = {
  "archive", "back", "bell", "calendar", "check", "chevron-down", "chevron-left", "chevron-right", "chevron-up", "clock",
  "close", "copy", "down", "download", "edit", "error", "external", "eye", "file", "filter", "folder", "forward", "grip",
  "home", "info", "install", "link", "lock", "menu", "minus", "more", "motion", "offline", "pause", "pin", "play", "plus",
  "redo", "refresh", "search", "sort", "stop", "theme", "trash", "tune", "undo", "up", "upload", "user", "warning", "window",
};

static void type_section(void) {
  int n = (int)(sizeof ICONS / sizeof ICONS[0]);
  RK_LABEL("Barlow, in a few roles.", .style = RK_TEXT_GROUP);
  RK_LABEL("Title, semibold", .style = RK_TEXT_TITLE);
  RK_LABEL("a kicker, tracked", .style = RK_TEXT_KICKER);
  RK_LABEL("Body text at size-s with the body line height. It wraps at the width of its column, as a paragraph does.");
  RK_LABEL("Strong text", .style = RK_TEXT_STRONG);
  RK_LABEL("Small text for a tile's label", .style = RK_TEXT_SMALL);
  RK_LABEL("meta with tabular figures 0123456789", .style = RK_TEXT_META);
  RK_LABEL("51 icons on a 20px grid", .style = RK_TEXT_KICKER);
  RK_VSTACK(.gap = rk_metric_of(RK_SPACE_S))
    for (int i = 0; i < n; i += 13) RK_HSTACK(.gap = 14) for (int k = i; k < i + 13 && k < n; k++) RK_ICON(ICONS[k]);
}

/* settings ------------------------------------------------------------------------------------------------------- */

static void set_theme(rk_view *v, void *u) { (void)v; (void)u; rk_app_set_theme((rk_theme)D.theme); }
static void set_density(rk_view *v, void *u) { (void)v; (void)u; rk_app_set_density((rk_density)D.density); }
static void set_motion(rk_view *v, void *u) { (void)v; (void)u; rk_app_set_motion((rk_motion_pref)D.motion); }
static void set_accent(rk_view *v, void *u) { (void)v; (void)u; if (D.accent) rk_app_set_accent(0x8a2d6bffu, 0xd99ac6ffu); else rk_app_set_accent(0, 0); }
static void say_notify(rk_view *v, void *u) { (void)v; (void)u; toast(D.notify ? "A failed build will say so." : "Failed builds stay quiet."); }
static void say_digest(rk_view *v, void *u) {
  static const char *const words[] = { "A digest every morning.", "A digest on Mondays.", "No digest." };
  (void)v; (void)u;
  toast(words[D.digest]);
}
static void say_sound(rk_view *v, void *u) { (void)v; (void)u; toast(D.sounds ? "Sounds on." : "Sounds off."); }

/* A choice: a kicker and segments, bound to the app's number. */
static void choice(const char *title, const char *const names[], int count, int *chosen, void (*fn)(rk_view *, void *)) {
  RK_LABEL(title, .style = RK_TEXT_KICKER);
  RK_HSTACK() RK_SEGMENTS(.key = title, .options = names, .count = count, .chosen = chosen, .on_change = fn);
}

static void settings_section(void) {
  static const char *const theme[] = { "system", "light", "dark" }, *const density[] = { "as the pointer", "comfortable", "compact" };
  static const char *const motion[] = { "as the system", "on", "off" }, *const accent[] = { "ink", "plum" };
  static const char *const digest[] = { "every morning", "on Mondays", "never" };
  RK_LABEL("Yours to choose.", .style = RK_TEXT_GROUP);
  RK_LABEL("Each follows the system until you choose. A change of theme or accent cross-fades; reduced motion makes every transition instant, never skipped.");
  choice("theme", theme, 3, &D.theme, set_theme);
  choice("density", density, 3, &D.density, set_density);
  choice("motion", motion, 3, &D.motion, set_motion);
  choice("accent", accent, 2, &D.accent, set_accent);
  RK_LABEL(rk_ui_fmt("Now: %s, %s density, motion %s.", rk_app_is_dark() ? "dark" : "light",
                     rk_metric_of(RK_CONTROL_HEIGHT) < 40 ? "compact" : "comfortable", rk_app_is_reduced_motion() ? "reduced" : "on"),
           .style = RK_TEXT_SMALL);                      /* the look as it is: built again when it changes */
  RK_LABEL("notifications", .style = RK_TEXT_KICKER);
  RK_CHECK("Tell me when a build fails", .on = &D.notify, .on_change = say_notify);
  RK_LABEL("a digest", .style = RK_TEXT_SMALL);
  RK_CHOICES(RK_OPTIONS(digest), .chosen = &D.digest, .on_change = say_digest);
  RK_TOGGLE("Sounds", .on = &D.sounds, .on_change = say_sound);
}

/* The window ----------------------------------------------------------------------------------------------------- */

static void app(void *user) {
  (void)user;
  RK_PLACES() {
    RK_PLACE("home") RK_PANORAMA() {
      RK_SECTION("rekta") rekta_section();
      RK_SECTION("live") live_section();
      RK_SECTION("motion") motion_section();
      RK_SECTION("controls") controls_section();
      RK_SECTION("lists") lists_section();
      RK_SECTION("desk", .width = 1000) desk_section();       /* wide enough for the split to stand sideways */
      RK_SECTION("type") type_section();
      RK_SECTION("settings") settings_section();
    }
    RK_PLACE("release notes", .open = &D.notes) notes_place();
  }
}

/* The tour for --shots: what a person would do, with a picture at each moment worth seeing ----------------------- */

static rk_view *root(void) { return rk_controller_view(rk_window_root(D.window)); }

static void shot(const char *name) {
  char path[1024];
  snprintf(path, sizeof path, "%s/%s.png", D.shots, name);
  if (rk_window_snapshot(D.window, path)) printf("  %s\n", path); else fprintf(stderr, "could not write %s\n", path);
}

static rk_view *find(const char *name) {
  rk_view *v = rk_view_find(root(), name);
  if (!v) fprintf(stderr, "no view called %s\n", name);
  return v;
}

static rk_point middle(rk_view *v) {
  rk_rect b = rk_view_bounds(v);
  return rk_view_to_window(v, (rk_point){ b.w / 2, b.h / 2 });
}

static void pointer(rk_event_type type, rk_point p) {
  rk_event e = { sizeof e, type, 0, p, 0, 0, 0, 0, 0, NULL, NULL, 0 };
  rk_window_inject(D.window, &e);
}

/* a choice from the keyboard, as a person might: focus, then a key */
static void choose(const char *name, uint32_t key) {
  rk_view *v = find(name);
  if (!v) return;
  rk_window_focus(D.window, v);
  rk_window_key(D.window, key);
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

/* the feed's timer, run by the tour at once: the state moves, and what is declared follows */
static void tick_now(void) { live_tick(NULL); rk_ui_update(NULL); }

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
  tick_now(); tick_now(); tick_now(); rk_app_run_for(110); shot("09-live-flip");
  rk_app_run_for(800);
  key(RK_KEY_PAGE_DOWN); rk_app_run_for(700); shot("10-motion");
  press("archive"); rk_app_run_for(400); shot("11-toast");
  press("open a sheet"); rk_app_run_for(500); shot("12-sheet");
  key(RK_KEY_ESCAPE); rk_app_run_for(600);
  key(RK_KEY_PAGE_DOWN); rk_app_run_for(700); shot("13-controls");
  press("more"); rk_app_run_for(300); shot("13-menu");
  key(RK_KEY_ESCAPE); rk_app_run_for(200);
  key(RK_KEY_PAGE_DOWN); rk_app_run_for(700); shot("14-lists");
  key(RK_KEY_PAGE_DOWN); rk_app_run_for(700); shot("14-desk");
  key(RK_KEY_PAGE_DOWN); rk_app_run_for(700); shot("15-type");
  key(RK_KEY_PAGE_DOWN); rk_app_run_for(700);
  choose("theme", RK_KEY_END); rk_app_run_for(700); shot("16-dark");
  choose("accent", RK_KEY_END); rk_app_run_for(700); shot("17-accent");
  key(RK_KEY_HOME); rk_app_run_for(900); shot("18-rekta-dark");
  rk_app_quit(0);
}

/* --perf: the loop under load. Each phase prints what drawing cost and how long input took to show; a phase fails
   when a press lands on the wrong view, motion does not come to rest, or views are left over. */

static int perf_fails;

static rk_stats stats(bool reset) { rk_stats s = { .size = sizeof s }; rk_app_stats(&s); if (reset) rk_app_stats_reset(); return s; }

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
  for (int i = 0; i < 3; i++) { tick_now(); rk_app_run_for(2000); }
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
      rk_event e = { sizeof e, RK_EVENT_WHEEL, 0, top, 0, 0, 48.0f * dir, 0, 0, NULL, NULL, 0 };
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
    else { rk_view *v = rk_view_find(root(), a); if (v) rk_view_tap(v); }
    rk_app_run_for(40);
  }
  phase("storm");

  key(RK_KEY_ESCAPE); key(RK_KEY_HOME);
  D.npins = 0;                                                 /* the board as it was, its buttons in sight again */
  rk_ui_update(NULL);
  rk_app_run_for(6000);                                        /* long enough for the toasts the storm queued to go */
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
  for (int i = 0; i < NBUILDS; i++) D.order[i] = i;
  for (int i = 0; i < 3; i++) D.events[i] = i;                /* the first three events, oldest last */
  D.nevents = D.next_event = 3;
  snprintf(D.notes_text[1], sizeof D.notes_text[1], "flaky test, rerun");
  D.window = rk_window_create(&(rk_window_desc){ .size = sizeof(rk_window_desc), .title = "Rekta core", .width = 1280, .height = 820, .build = app });
  if (!D.window) { rk_app_quit(1); return; }
  if (D.shots) { tour(); return; }
  if (D.perf) { perf(); return; }
  rk_app_every(2000, live_tick, NULL);                         /* the feed: its window follows by itself */
}

int main(int argc, char **argv) {
  rk_app_desc desc = { .size = sizeof desc, .name = "Rekta core", .id = "de.rekta.demo", .launch = launch };
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--shots") && i + 1 < argc) { D.shots = argv[++i]; desc.headless = true; }
    else if (!strcmp(argv[i], "--perf")) D.perf = true;
    else if (!strcmp(argv[i], "--screen")) D.screen = true;
  }
  if (D.perf && !D.screen) desc.headless = true;
  return rk_app_main(&desc);
}

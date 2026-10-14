/* tiles_imperative.c: the demo's rekta and live sections as demo.c builds them - views kept to be changed later, each
   change made where it happens. tiles_declarative.c is the same with declared views; tiles_shell.h is theirs both. */
#include "tiles_shell.h"

/* the sections: from here to the end, as in demo.c */
typedef struct demo {
  rk_view *board, *task, *presses, *grow_tile;             /* rekta */
  rk_view *live, *status, *events;                         /* live */
  int npresses, pinned, queue, status_i, ticks, events_n;
  float task_done, series[24]; int nseries;
  bool big;
} demo;

static demo D;

/* rekta: tiles ------------------------------------------------------------------------------------------------- */

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
  if (!t) { rk_window_toast(window, "Nothing pinned yet.", NULL, NULL, NULL); return; }
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
  rk_view *s = rk_vstack();
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

  rk_view *buttons = rk_add(s, rk_hstack());
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
  rk_insert(D.events, row, 0);                                 /* added last; moved to the top */
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
  if (!feed) return;
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
  feed = !feed;
  rk_window_toast(window, feed ? "Feed on: the figures are live." : "Feed off: the figures hold still.", NULL, NULL, NULL);
}

static rk_view *live_section(void) {
  rk_view *s = rk_vstack();
  rk_add(s, rk_label("Live, and honest about it.", RK_TEXT_GROUP));
  rk_add(s, rk_label("Every two seconds the queue moves and its number peeks in; every six the runners' status flips. Events arrive at the top and the oldest leaves.", RK_TEXT_BODY));

  rk_view *grid = rk_add(s, rk_tile_grid());
  D.queue = 14;                                                /* demo.c leaves it at 0 under a tile that says 14 */
  D.live = rk_add(grid, rk_tile("14", "jobs in the queue"));
  rk_tile_set_status(D.live, RK_STATUS_INFO, NULL);
  rk_view_set_span(D.live, 2, 1);
  D.status = rk_add(grid, rk_tile("3", "runners"));
  rk_tile_set_status(D.status, RK_STATUS_INFO, NULL);

  rk_add(s, rk_label("latest events", RK_TEXT_KICKER));
  D.events = rk_add(s, rk_rows());
  for (int i = 0; i < 3; i++) rk_rows_add(D.events, EVENTS[i][0], NULL, event_status(i));
  D.events_n = 3;

  rk_view *feed = rk_add(rk_add(s, rk_hstack()), rk_button("feed on or off", toggle_feed, NULL));
  rk_button_set_kind(feed, RK_BUTTON_QUIET);
  rk_button_set_icon(feed, "pause");
  return s;
}

static void refresh(void) {}                                   /* each change was made where it happened */

/* tiles_declarative.c: the demo's rekta and live sections with declared views (rekta_ui.h). Each section is a
   function of the state below; the handlers and the feed only change the state, and the views follow it with the
   design's motion. tiles_imperative.c is the same as demo.c builds it; tiles_shell.h is theirs both. */
#include "tiles_shell.h"
#include "../include/rekta_ui.h"

/* the sections: from here to the end */
typedef struct state {
  int npresses, pins[32], npins, pinned;                   /* rekta: the pinned tiles' numbers, the newest last */
  int queue, status_i, ticks, events[4], nevents, events_n; /* live: the events shown, the newest first */
  float task_done, series[24]; int nseries;
  bool big;
} state;

static state S = { .queue = 14, .events = { 0, 1, 2 }, .nevents = 3, .events_n = 3 };

/* rekta: tiles ------------------------------------------------------------------------------------------------- */

static void press_tile(rk_view *v, void *u) { (void)v; (void)u; S.npresses++; }
static void restart_task(rk_view *v, void *u) { (void)v; (void)u; S.task_done = 0; }
static void pin(rk_view *v, void *u) { (void)v; (void)u; if (S.npins < 32) S.pins[S.npins++] = ++S.pinned; }
static void grow(rk_view *v, void *u) { (void)v; (void)u; S.big = !S.big; }
static void unpin(rk_view *v, void *u) {
  (void)v; (void)u;
  if (S.npins) S.npins--; else rk_window_toast(window, "Nothing pinned yet.", NULL, NULL, NULL);
}

/* A tile's context menu: a menu's handlers are not declared ones, so they ask for the build themselves. */
static void tile_grow(rk_view *menu, int item, void *user) { (void)menu; (void)item; (void)user; grow(NULL, NULL); rk_ui_update(NULL); }
static void tile_unpin(rk_view *menu, int item, void *user) { (void)menu; (void)item; (void)user; unpin(NULL, NULL); rk_ui_update(NULL); }

static rk_view *tile_menu(void *user) {
  (void)user;
  rk_view *m = rk_menu();
  rk_menu_add(m, "resize", NULL, tile_grow, NULL);
  rk_menu_add(m, "unpin", NULL, tile_unpin, NULL);
  return m;
}

static void rekta(void *user) {
  (void)user;
  char task[16], presses[16], pinned[16];
  bool done = S.task_done >= 1;
  snprintf(task, sizeof task, "%d / 8", (int)lroundf(S.task_done * 8));
  snprintf(presses, sizeof presses, "%d", S.npresses);
  RK_VSTACK() {
    RK_LABEL("Tiles, not cards.", .style = RK_TEXT_GROUP);
    RK_LABEL("A tile's whole area is one fill, and the fill is its status. Press one: it nudges back where you press it. A tile that opens something presses; one that only informs does not.");
    RK_TILES() {
      /* the bar glides to each step; done, the new status turns the tile over */
      RK_TILE(task, .key = "task", .label = "import steps", .status = done ? RK_STATUS_SUCCESS : RK_STATUS_INFO, .span = { 2, 2 },
              .message = done ? "Imported. Stillness means done." : "Each step advances the bar; the last one flips the tile.",
              .progress = done ? NULL : &S.task_done, .change = RK_UI_IN_PLACE);
      for (int i = S.npins - 1; i >= 0; i--) {                   /* the newest after the task: it builds in, the rest make room */
        snprintf(pinned, sizeof pinned, "%d", S.pins[i]);
        RK_TILE(pinned, .id = (uint64_t)S.pins[i], .name = "pinned", .label = "pinned tile");
      }
      RK_TILE("0.3", .key = "notes", .label = "release notes", .on_press = open_notes);
      RK_TILE(presses, .key = "presses", .label = S.npresses == 1 ? "tile pressed" : "tiles pressed", .status = RK_STATUS_WARNING, .on_press = press_tile);
      RK_TILE("12", .key = "grow", .label = "builds today", .status = RK_STATUS_SUCCESS, .span = { S.big ? 2 : 1, S.big ? 2 : 1 }, .menu = tile_menu);
      RK_TILE("51", .label = "icons");
    }
    RK_HSTACK() {
      RK_BUTTON("pin a tile", .on_press = pin);
      RK_BUTTON("unpin it", .on_press = unpin);
      RK_BUTTON("resize", .on_press = grow);
      RK_BUTTON("restart", .button = RK_BUTTON_QUIET, .icon = "refresh", .on_press = restart_task);
    }
  }
}

/* live --------------------------------------------------------------------------------------------------------- */

static const char *const EVENTS[][2] = {
  { "Build #4190 finished", "done" }, { "A runner joined the pool", "info" }, { "Review waiting on Ada", "waiting" },
  { "Nightly import retried", "info" }, { "Deploy to staging finished", "done" }, { "Disk at 81% on runner 3", "waiting" }, { "Upload failed", "failed" },
};
static const rk_status ORDER[] = { RK_STATUS_INFO, RK_STATUS_WARNING, RK_STATUS_SUCCESS, RK_STATUS_ERROR };

static rk_status event_status(int i) { rk_status st = status_named(EVENTS[i][1]); return st ? st : RK_STATUS_INFO; }

static void live_tick(void *u) {
  (void)u;
  if (!feed) return;
  S.ticks++;
  S.queue = (S.queue + 3 + S.ticks * 7) % 40;
  if (S.nseries < 24) S.nseries++; else memmove(S.series, S.series + 1, 23 * sizeof(float));
  S.series[S.nseries - 1] = (float)S.queue;
  if (S.ticks % 3 == 0) S.status_i = (S.status_i + 1) % 4;
  if (S.ticks % 2 == 0) {                                      /* an event arrives at the top; the oldest of four leaves */
    memmove(S.events + 1, S.events, 3 * sizeof *S.events);
    S.events[0] = S.events_n++;
    if (S.nevents < 4) S.nevents++;
  }
  if (S.task_done < 1) S.task_done = fminf(1, S.task_done + 0.125f);
  rk_ui_update(NULL);                                          /* a timer is no declared handler */
}

static void toggle_feed(rk_view *v, void *u) {
  (void)v; (void)u;
  feed = !feed;
  rk_window_toast(window, feed ? "Feed on: the figures are live." : "Feed off: the figures hold still.", NULL, NULL, NULL);
}

static void live(void *user) {
  (void)user;
  char queue[16];
  snprintf(queue, sizeof queue, "%d", S.queue);
  RK_VSTACK() {
    RK_LABEL("Live, and honest about it.", .style = RK_TEXT_GROUP);
    RK_LABEL("Every two seconds the queue moves and its number peeks in; every six the runners' status flips. Events arrive at the top and the oldest leaves.");
    RK_TILES() {
      RK_TILE(queue, .label = "jobs in the queue", .status = RK_STATUS_INFO, .span = { 2, 1 }, .series = S.series, .nseries = S.nseries);
      RK_TILE("3", .label = "runners", .status = ORDER[S.status_i]);
    }
    RK_LABEL("latest events", .style = RK_TEXT_KICKER);
    RK_ROWS() {
      for (int i = 0; i < S.nevents; i++) {
        int e = S.events[i];
        RK_ROW(EVENTS[e % 7][0], .id = (uint64_t)e + 1, .status = event_status(e % 7));
      }
    }
    RK_HSTACK() RK_BUTTON("feed on or off", .button = RK_BUTTON_QUIET, .icon = "pause", .on_press = toggle_feed);
  }
}

static rk_view *rekta_section(void) { return rk_ui_view(rekta, NULL); }
static rk_view *live_section(void) { return rk_ui_view(live, NULL); }
static void refresh(void) { rk_ui_update(NULL); }

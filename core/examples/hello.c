/* hello.c: a first app. A section of tiles, a button that counts, a tile that opens something. */
#include "rekta.h"
#include <stdio.h>

static rk_view *builds;                            /* the one view that changes later */
static int finished = 312;

static void open_notes(rk_view *tile, void *user) {
  rk_window_toast(rk_view_window(tile), "Release notes are on their way.", NULL, NULL, NULL);
}

static void build_one(rk_view *button, void *user) {
  char text[16];
  snprintf(text, sizeof text, "%d", ++finished);
  rk_view_transition(builds, RK_TRANSITION_PEEK);  /* a new number peeks in */
  rk_tile_set_stat(builds, text, "finished today");
}

static rk_view *today(void) {
  rk_view *page = rk_column();
  rk_add(page, rk_label("Today", RK_TEXT_GROUP));

  rk_view *tiles = rk_add(page, rk_tile_grid());
  builds = rk_add(tiles, rk_tile("312", "finished today"));
  rk_tile_set_status(builds, RK_STATUS_SUCCESS, NULL);
  rk_view_set_span(builds, 2, 1);

  rk_view *notes = rk_add(tiles, rk_tile("0.3", "release notes"));
  rk_view_on_activate(notes, open_notes, NULL);    /* it presses, and opens something */

  rk_add(page, rk_button("build one", build_one, NULL));
  return page;
}

static void launch(void *user) {
  rk_window_desc desc = { .size = sizeof desc, .title = "My tool", .width = 960, .height = 640 };
  rk_window *window = rk_window_create(&desc);

  rk_controller *home = rk_panorama();
  rk_panorama_add(home, "today", today(), 0);
  rk_window_show(window, home);
}

int main(void) {
  rk_app_desc app = { .size = sizeof app, .name = "My tool", .id = "de.example.tool", .launch = launch };
  return rk_app_main(&app);
}

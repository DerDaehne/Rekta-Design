/* hello.c: a first app. A section of tiles, a button that counts, a tile that opens something. */
#include "rekta.h"

static int finished = 312;                         /* the state: the window shows what is here */

static void build_one(rk_view *button, void *user) { finished++; }   /* the window follows: the new number peeks in */

static void open_notes(rk_view *tile, void *user) {
  rk_window_toast(rk_view_window(tile), "Release notes are on their way.", NULL, NULL, NULL);
}

static void today(void *user) {                    /* what the window shows: built again after every handler */
  RK_PANORAMA() RK_SECTION("today") {
    RK_LABEL("Today", .style = RK_TEXT_GROUP);
    RK_TILES() {
      RK_TILE(rk_ui_fmt("%d", finished), .label = "finished today", .status = RK_STATUS_SUCCESS, .span = { 2, 1 });
      RK_TILE("0.3", .label = "release notes", .on_press = open_notes);   /* it presses, and opens something */
    }
    RK_BUTTON("build one", .on_press = build_one);
  }
}

static void launch(void *user) {
  rk_window_desc desc = { .size = sizeof desc, .title = "My tool", .width = 960, .height = 640, .build = today };
  rk_window_create(&desc);
}

int main(void) {
  rk_app_desc app = { .size = sizeof app, .name = "My tool", .id = "de.example.tool", .launch = launch };
  return rk_app_main(&app);
}

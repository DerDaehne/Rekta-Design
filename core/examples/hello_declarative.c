/* hello_declarative.c: hello.c with declared views. The page is a function of the count; the button only counts. */
#include "rekta_ui.h"
#include <stdio.h>

static int finished = 312;

static void open_notes(rk_view *tile, void *user) {
  rk_window_toast(rk_view_window(tile), "Release notes are on their way.", NULL, NULL, NULL);
}

static void build_one(rk_view *button, void *user) { finished++; }   /* the page follows: the new number peeks in */

static void today(void *user) {
  char builds[16];
  snprintf(builds, sizeof builds, "%d", finished);
  RK_VSTACK() {
    RK_LABEL("Today", .style = RK_TEXT_GROUP);
    RK_TILES() {
      RK_TILE(builds, .label = "finished today", .status = RK_STATUS_SUCCESS, .span = { 2, 1 });
      RK_TILE("0.3", .label = "release notes", .on_press = open_notes);   /* it presses, and opens something */
    }
    RK_BUTTON("build one", .on_press = build_one);
  }
}

static void launch(void *user) {
  rk_window_desc desc = { .size = sizeof desc, .title = "My tool", .width = 960, .height = 640 };
  rk_window *window = rk_window_create(&desc);

  rk_controller *home = rk_panorama();
  rk_panorama_add(home, "today", rk_ui_view(today, NULL), 0);
  rk_window_show(window, home);
}

int main(void) {
  rk_app_desc app = { .size = sizeof app, .name = "My tool", .id = "de.example.tool", .launch = launch };
  return rk_app_main(&app);
}

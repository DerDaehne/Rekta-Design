/* navigation.c: one app, its places reached by a pivot along the top or down the side, as tabs are in a browser.

     ./navigation top          the titles along the top: the place in sight is the large one
     ./navigation side         the titles as a list down the leading side
     ./navigation <layout> --shot file.png [width height]   draws it once, headless, and writes the picture
     ./navigation <layout> --film DIR [width height]        goes to people, home and builds, headless, a frame every 20 ms

   RK_SIDE_MOTION=step picks the other motion down the side, for the comparison. */
#include "rekta.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const PLACES[] = { "home", "builds", "people", "settings" };
#define NPLACES 4
static const char *style = "top", *shot_path, *film_dir;
static float width = 1100, height = 680;

/* The places ------------------------------------------------------------------------------------------------------ */

static const char *build_cell(int row, int column, void *user) {
  static char text[32];
  static const char *const status[] = { "done", "failed", "done", "waiting", "done" };
  (void)user;
  if (column == 0) { snprintf(text, sizeof text, "Build #%d", 5000 - row); return text; }
  if (column == 1) return status[row % 5];
  snprintf(text, sizeof text, "%d min", 3 + row * 7 % 40);
  return text;
}

static rk_view *place_view(int i) {
  rk_view *page = rk_column();
  if (i == 0) {
    rk_add(page, rk_label("Good morning.", RK_TEXT_GROUP));
    rk_view *grid = rk_add(page, rk_tile_grid());
    rk_view *t = rk_add(grid, rk_tile("312", "finished today"));
    rk_tile_set_status(t, RK_STATUS_SUCCESS, NULL);
    rk_view_set_span(t, 2, 1);
    rk_tile_set_status(rk_add(grid, rk_tile("2", "failed")), RK_STATUS_ERROR, NULL);
    rk_add(grid, rk_tile("0.3", "release notes"));
  } else if (i == 1) {
    rk_add(page, rk_label("The last builds.", RK_TEXT_GROUP));
    rk_view *table = rk_add(page, rk_table(40, build_cell, NULL));
    rk_table_add_column(table, "build", 0, RK_COLUMN_SORTABLE);
    rk_table_add_column(table, "status", 120, 0);
    rk_table_add_column(table, "time", 90, RK_COLUMN_NUMERIC);
    rk_view_set_size(table, 0, 360);
  } else if (i == 2) {
    rk_add(page, rk_label("Who works on it.", RK_TEXT_GROUP));
    rk_view *rows = rk_add(page, rk_rows());
    rk_rows_add(rows, "Ada Lovelace", "owner", RK_STATUS_NONE);
    rk_rows_add(rows, "Grace Hopper", "reviewer", RK_STATUS_NONE);
    rk_rows_add(rows, "Alan Kay", "away until Monday", RK_STATUS_WARNING);
  } else {
    static const char *const theme[] = { "system", "light", "dark" };
    rk_add(page, rk_label("Yours to choose.", RK_TEXT_GROUP));
    rk_add(page, rk_label("theme", RK_TEXT_KICKER));
    rk_add(rk_add(page, rk_row()), rk_segments(theme, 3, 0));
    rk_add(page, rk_toggle("Sounds", false));
  }
  return page;
}

/* The pivot, along the top or down the side ----------------------------------------------------------------------- */

static rk_controller *nav;

static rk_controller *places(bool side) {
  nav = rk_pivot();
  for (int i = 0; i < NPLACES; i++) {
    rk_view *page = place_view(i);
    rk_view_set_padding(page, (rk_insets){ side ? 0 : rk_metric_of(RK_SPACE_L), 0, 0, 0 });
    rk_pivot_add(nav, rk_controller_create_with_view(page, PLACES[i]));
  }
  if (side) rk_pivot_set_layout(nav, RK_PIVOT_SIDE);
  rk_pivot_select(nav, 1);
  return nav;
}

/* The app ---------------------------------------------------------------------------------------------------------- */

static rk_window *window;
static void take_shot(void *user) {
  (void)user;
  if (!rk_window_snapshot(window, shot_path)) fprintf(stderr, "could not write %s\n", shot_path);
  rk_app_quit(0);
}

/* the film: a frame every 20 ms of virtual time while the pivot goes to people, to home and back to builds */
static int frame;
static void take_frame(void *user) {
  char path[512];
  (void)user;
  snprintf(path, sizeof path, "%s/%04d.png", film_dir, frame++);
  if (!rk_window_snapshot(window, path)) { fprintf(stderr, "could not write %s\n", path); rk_app_quit(1); }
  if (frame == 150) rk_app_quit(0);
}
static void go(void *user) { rk_pivot_select(nav, (int)(intptr_t)user); }

static void launch(void *user) {
  (void)user;
  rk_window_desc d = { .size = sizeof d, .title = "Navigation", .width = width, .height = height };
  window = rk_window_create(&d);
  rk_window_show(window, places(!strcmp(style, "side")));
  if (shot_path) rk_app_after(1500, false, take_shot, NULL);
  if (film_dir) {
    rk_app_after(500, false, go, (void *)(intptr_t)2);
    rk_app_after(1500, false, go, (void *)(intptr_t)0);
    rk_app_after(2300, false, go, (void *)(intptr_t)1);
    rk_app_after(20, true, take_frame, NULL);
  }
  rk_release(window);
}

int main(int argc, char **argv) {
  rk_app_desc app = { .size = sizeof app, .name = "Navigation", .launch = launch };
  for (int i = 1; i < argc; i++) {
    if ((!strcmp(argv[i], "--shot") || !strcmp(argv[i], "--film")) && i + 1 < argc) {
      *(argv[i][2] == 's' ? &shot_path : &film_dir) = argv[i + 1]; i++; app.headless = true;
      if (i + 2 < argc) { width = (float)atof(argv[i + 1]); height = (float)atof(argv[i + 2]); i += 2; }
    } else style = argv[i];
  }
  return rk_app_main(&app);
}

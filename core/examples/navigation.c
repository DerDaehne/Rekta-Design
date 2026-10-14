/* navigation.c: one app, its places reached by a pivot along the top or down the side, as tabs are in a browser.

     ./navigation top          the titles along the top: the place in sight is the large one
     ./navigation side         the titles as a list down the leading side
     ./navigation <layout> --shot file.png [width height]   draws it once, headless, and writes the picture
     ./navigation <layout> --film DIR [width height]        goes to people, home and builds, headless, a frame every 20 ms

   The pivot picks the place, so a change turns it: the place in sight turns away and the next one turns in, each
   part in a time that follows its size. */
#include "rekta_test.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const PLACES[] = { "home", "builds", "people", "settings" };
static const char *style = "top", *shot_path, *film_dir;
static float width = 1100, height = 680;

/* The places ------------------------------------------------------------------------------------------------------ */

static int place = 1;                                      /* the place in sight: the pivot shows it, picking writes it */
static bool sounds;
static int theme;

static const char *build_cell(int row, int column, void *user) {
  static char text[32];
  static const char *const status[] = { "done", "failed", "done", "waiting", "done" };
  (void)user;
  if (column == 0) { snprintf(text, sizeof text, "Build #%d", 5000 - row); return text; }
  if (column == 1) return status[row % 5];
  snprintf(text, sizeof text, "%d min", 3 + row * 7 % 40);
  return text;
}

static void home(void) {
  RK_LABEL("Good morning.", .style = RK_TEXT_GROUP);
  RK_TILES() {
    RK_TILE("312", .label = "finished today", .status = RK_STATUS_SUCCESS, .span = { 2, 1 });
    RK_TILE("2", .label = "failed", .status = RK_STATUS_ERROR);
    RK_TILE("0.3", .label = "release notes");
  }
}

static void builds(void) {
  RK_LABEL("The last builds.", .style = RK_TEXT_GROUP);
  RK_TABLE(.count = 40, .cell = build_cell, .height = 360) {
    RK_COLUMN("build", .flags = RK_COLUMN_SORTABLE);
    RK_COLUMN("status", .width = 120);
    RK_COLUMN("time", .width = 90, .flags = RK_COLUMN_NUMERIC);
  }
}

static void people(void) {
  RK_LABEL("Who works on it.", .style = RK_TEXT_GROUP);
  RK_ROWS() {
    RK_ROW("Ada Lovelace", .meta = "owner");
    RK_ROW("Grace Hopper", .meta = "reviewer");
    RK_ROW("Alan Kay", .meta = "away until Monday", .status = RK_STATUS_WARNING);
  }
}

static void settings(void) {
  static const char *const themes[] = { "system", "light", "dark" };
  RK_LABEL("Yours to choose.", .style = RK_TEXT_GROUP);
  RK_LABEL("theme", .style = RK_TEXT_KICKER);
  RK_HSTACK() RK_SEGMENTS(.options = themes, .count = 3, .chosen = &theme);
  RK_TOGGLE("Sounds", .on = &sounds);
}

/* The pivot, along the top or down the side ----------------------------------------------------------------------- */

static void app(void *user) {
  bool side = !strcmp(style, "side");
  rk_insets pad = { side ? 0 : rk_metric_of(RK_SPACE_L), 0, 0, 0 };
  (void)user;
  RK_PIVOT(.chosen = &place, .side = side) {
    RK_PAGE(PLACES[0], .padding = pad) home();
    RK_PAGE(PLACES[1], .padding = pad) builds();
    RK_PAGE(PLACES[2], .padding = pad) people();
    RK_PAGE(PLACES[3], .padding = pad) settings();
  }
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
  if (frame == 250) rk_app_quit(0);
}
static void go(void *user) { place = (int)(intptr_t)user; }   /* the state changes; the pivot follows */

static void launch(void *user) {
  (void)user;
  rk_window_desc d = { .size = sizeof d, .title = "Navigation", .width = width, .height = height, .build = app };
  window = rk_window_create(&d);
  if (shot_path) rk_app_after(1500, take_shot, NULL);
  if (film_dir) {
    rk_app_after(400, go, (void *)(intptr_t)2);
    rk_app_after(2000, go, (void *)(intptr_t)0);
    rk_app_after(3600, go, (void *)(intptr_t)1);
    rk_app_every(20, take_frame, NULL);
  }
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

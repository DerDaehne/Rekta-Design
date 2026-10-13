/* navigation.c: one app, its places reached three ways - to compare, before one becomes part of Rekta.

     ./navigation pivot        a pivot at the top: the strip is the title, a place at a time (what Rekta has)
     ./navigation list         a list down the left in the strip's light type, the place in sight in the text colour
     ./navigation bar          a list down the left at the group size, an accent bar beside the place in sight
     ./navigation <style> --shot file.png [width height]   draws it once, headless, and writes the picture

   The list styles are a sketch made with the public API (a custom view), not part of the library. */
#include "rekta.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const PLACES[] = { "home", "builds", "people", "settings" };
#define NPLACES 4
static const char *style = "pivot", *shot_path;
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

/* A list down the left: a sketch ---------------------------------------------------------------------------------- */

typedef struct navlist { int active; bool bar; rk_view *content, *shown; } navlist;

static rk_text_style nav_style(const navlist *n) { return n->bar ? RK_TEXT_GROUP : RK_TEXT_PIVOT; }
static float nav_row(const navlist *n) { return rk_text_measure("home", nav_style(n), 0).h + rk_metric_of(RK_SPACE_S); }
static float nav_top(void) { return rk_metric_of(RK_SPACE_L); }

static void nav_draw(rk_view *v, rk_canvas *c, void *st) {
  navlist *n = st;
  float x = rk_metric_of(RK_SPACE_XL), y = nav_top(), row = nav_row(n);
  rk_window *w = rk_view_window(v);
  for (int i = 0; i < NPLACES; i++) {
    bool on = i == n->active;
    if (on && n->bar) rk_canvas_fill_rect(c, (rk_rect){ x - rk_metric_of(RK_SPACE_M), y + row * 0.2f, 4, row * 0.6f }, rk_color_of(RK_ROLE_ACCENT));
    rk_canvas_text(c, PLACES[i], (rk_point){ x, y + rk_metric_of(RK_SPACE_S) / 2 }, nav_style(n), rk_color_of(on ? RK_ROLE_TEXT : RK_ROLE_TEXT_HIGH));
    if (on && w && rk_window_focused(w) == v) {
      rk_size s = rk_text_measure(PLACES[i], nav_style(n), 0);
      rk_canvas_stroke_rect(c, (rk_rect){ x - 6, y, s.w + 12, row }, 2, rk_color_of(RK_ROLE_FOCUS));
    }
    y += row;
  }
}

static void nav_show(rk_view *v, navlist *n, int i) {
  if (i < 0 || i >= NPLACES) return;
  n->active = i;
  if (n->shown) rk_view_remove(n->shown);
  rk_animate_begin(RK_MOTION_ENTER);                       /* the new place comes in as a place does */
  n->shown = rk_add(n->content, place_view(i));
  rk_animate_commit();
  rk_view_redraw(v);
}

static bool nav_event(rk_view *v, const rk_event *e, void *st) {
  navlist *n = st;
  if (e->type == RK_EVENT_POINTER_UP) { int i = (int)((e->position.y - nav_top()) / nav_row(n)); if (e->position.y >= nav_top()) nav_show(v, n, i); return true; }
  if (e->type == RK_EVENT_POINTER_DOWN) return true;
  if (e->type == RK_EVENT_KEY_DOWN && (e->key == RK_KEY_UP || e->key == RK_KEY_DOWN)) { nav_show(v, n, n->active + (e->key == RK_KEY_DOWN ? 1 : -1)); return true; }
  return false;
}

static rk_size nav_measure(rk_view *v, rk_size avail, void *st) {
  (void)v; (void)avail;
  navlist *n = st;
  float w = 0;
  for (int i = 0; i < NPLACES; i++) { float tw = rk_text_measure(PLACES[i], nav_style(n), 0).w; if (tw > w) w = tw; }
  return (rk_size){ w + 2 * rk_metric_of(RK_SPACE_XL), nav_top() + NPLACES * nav_row(n) };
}

static const rk_view_class navlist_class = { sizeof(rk_view_class), "navlist", nav_draw, nav_measure, NULL, nav_event, free };

static rk_controller *list_app(bool bar) {
  navlist *n = calloc(1, sizeof *n);
  n->bar = bar;
  rk_view *root = rk_row();
  rk_view_set_gap(root, 0);
  rk_view_set_align(root, RK_ALIGN_START, RK_ALIGN_STRETCH);
  rk_view *nav = rk_add(root, rk_view_create_custom(&navlist_class, n));
  rk_view_set_focusable(nav, true);
  n->content = rk_add(root, rk_view_create());
  rk_view_set_layout(n->content, RK_LAYOUT_COLUMN);
  rk_view_set_grow(n->content, 1);
  rk_view_set_padding(n->content, (rk_insets){ rk_metric_of(RK_SPACE_L), rk_metric_of(RK_SPACE_XL), 0, rk_metric_of(RK_SPACE_M) });
  n->shown = rk_add(n->content, place_view(1));
  n->active = 1;
  return rk_controller_create_with_view(root, "navigation");
}

/* A pivot at the top: what Rekta has ------------------------------------------------------------------------------ */

static rk_controller *pivot_app(void) {
  rk_controller *nav = rk_pivot();
  for (int i = 0; i < NPLACES; i++) {
    rk_view *page = place_view(i);
    rk_view_set_padding(page, (rk_insets){ rk_metric_of(RK_SPACE_L), 0, 0, 0 });
    rk_pivot_add(nav, rk_controller_create_with_view(page, PLACES[i]));
  }
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

static void launch(void *user) {
  (void)user;
  rk_window_desc d = { .size = sizeof d, .title = "Navigation", .width = width, .height = height };
  window = rk_window_create(&d);
  rk_controller *app = !strcmp(style, "list") ? list_app(false) : !strcmp(style, "bar") ? list_app(true) : pivot_app();
  rk_window_show(window, app);
  if (shot_path) rk_app_after(1500, false, take_shot, NULL);
  rk_release(window);
}

int main(int argc, char **argv) {
  rk_app_desc app = { .size = sizeof app, .name = "Navigation", .launch = launch };
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--shot") && i + 1 < argc) {
      shot_path = argv[++i]; app.headless = true;
      if (i + 2 < argc) { width = (float)atof(argv[i + 1]); height = (float)atof(argv[i + 2]); i += 2; }
    } else style = argv[i];
  }
  return rk_app_main(&app);
}

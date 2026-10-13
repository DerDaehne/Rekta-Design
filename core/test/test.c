/* test.c: rekta-core without a screen. It includes the library (white box), checks tokens, rasterizer, text, layout,
   motion, input, navigation and reference counts, and writes reference scenes as PNGs for test/compare.mjs.
   Usage: test OUT_DIR */
#include "../src/rekta.c"

static int passes, fails;
static const char *out = "test-out";
#define CHECK(cond, ...) do { if (cond) passes++; else { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define CLOSE_TO(a, b, eps) (fabs((double)(a) - (double)(b)) <= (eps))

static void shot(rk_window *w, const char *name) {
  char path[1024];
  snprintf(path, sizeof path, "%s/%s.png", out, name);
  CHECK(rk_window_snapshot(w, path), "write %s", path);
}

static rk_window *window(float w, float h) {
  rk_window_desc d = { .size = sizeof d, .title = "test", .width = w, .height = h };
  rk_window *win = rk_window_create(&d);
  return win;
}

/* takes content */
static void show(rk_window *w, rk_view *content, const char *title) {
  rk_window_show(w, rk_controller_create_with_view(content, title));
  rk_app_run_for(50);
}

static void done(rk_window *w) { rk_window_close(w); rk_release(w); }

/* a column with the gap and padding a test measures against */
static rk_view *col(float gap, float pad) {
  rk_view *v = rk_column();
  rk_view_set_gap(v, gap);
  rk_view_set_padding(v, (rk_insets){ pad, pad, pad, pad });
  return v;
}

/* Tokens, easing ------------------------------------------------------------------------------------------------- */

static void test_tokens(void) {
  CHECK(rk_color_of(RK_ROLE_BG) == 0xffffffffu, "light bg %08x", rk_color_of(RK_ROLE_BG));
  CHECK(rk_color_of(RK_ROLE_INFO_TILE) == 0x7da1f4ffu, "info tile");
  CHECK(rk_metric_of(RK_TILE_UNIT) == 150, "tile unit");
  CHECK(rk_metric_of(RK_CONTROL_HEIGHT) == 32, "compact control height on a desktop: %g", rk_metric_of(RK_CONTROL_HEIGHT));
  rk_app_set_density(RK_DENSITY_COMFORTABLE);
  CHECK(rk_metric_of(RK_CONTROL_HEIGHT) == 44, "comfortable control height");
  rk_app_set_density(RK_DENSITY_SYSTEM);
  CHECK(rk_duration_of(RK_DURATION_SHORT) == 250, "short");
  rk_app_set_motion(RK_MOTION_PREF_OFF);
  CHECK(rk_duration_of(RK_DURATION_SHORT) == 0 && rk_duration_of(RK_TIMING_TOAST) == 5000, "reduced motion zeroes motion, keeps timings");
  rk_app_set_motion(RK_MOTION_PREF_SYSTEM);
  rk_app_set_theme(RK_THEME_DARK);
  CHECK(rk_color_of(RK_ROLE_BG) == 0x0e0e12ffu, "dark bg");
  rk_app_set_theme(RK_THEME_SYSTEM);
  /* CSS ease at its half way and a few known points */
  CHECK(CLOSE_TO(rk__ease(rk__ease_base, 0.5f), 0.8024f, 0.003), "ease 0.5 = %f", rk__ease(rk__ease_base, 0.5f));
  CHECK(rk__ease(rk__ease_base, 0) == 0 && rk__ease(rk__ease_base, 1) == 1, "ease ends");
  CHECK(CLOSE_TO(rk__ease(rk__ease_in, 0.5f), 0.8392f, 0.002), "ease-in(0,0,0.2,1) 0.5 = %f", rk__ease(rk__ease_in, 0.5f));
  CHECK(CLOSE_TO(rk__ease(rk__ease_out, 0.5f), 0.5673f, 0.002), "ease-out(0.4,0,0.5,1) 0.5 = %f", rk__ease(rk__ease_out, 0.5f));
  CHECK(rk_version() == RK_VERSION, "version");
}

/* Raster ---------------------------------------------------------------------------------------------------------- */

static uint32_t px(rk_bitmap *b, int x, int y) { return b->px[(size_t)y * b->stride + x]; }

static void test_raster(void) {
  rk_bitmap *b = rk__bitmap_create(20, 20);
  rk_irect all = { 0, 0, 20, 20 };
  rk__fill_rect(b, all, 2.5f, 2, 10, 10, 0xff000000u);
  CHECK((px(b, 2, 5) >> 24) >= 126 && (px(b, 2, 5) >> 24) <= 129, "half-covered edge pixel: %u", px(b, 2, 5) >> 24);
  CHECK((px(b, 5, 5) >> 24) == 255 && px(b, 12, 5) == 0, "inside solid, outside empty");
  rk__bitmap_clear(b, 0);
  rk_path p = { 0 };
  rk__path_move(&p, 2.5f, 2); rk__path_line(&p, 10, 2); rk__path_line(&p, 10, 10); rk__path_line(&p, 2.5f, 10); rk__path_close(&p);
  rk__path_fill(b, all, &p, 0xff000000u);
  CHECK((px(b, 2, 5) >> 24) >= 126 && (px(b, 2, 5) >> 24) <= 129 && (px(b, 5, 5) >> 24) == 255, "path coverage equals the rect's: %u", px(b, 2, 5) >> 24);
  /* two strokes over each other add up and clamp, never cancel */
  rk__bitmap_clear(b, 0);
  rk__path_reset(&p);
  float a[4] = { 2, 10, 18, 10 }, c[4] = { 10, 2, 10, 18 };
  rk__path_stroke(&p, a, 2, false, 2, false);
  rk__path_stroke(&p, c, 2, false, 2, false);
  rk__path_fill(b, all, &p, 0xff000000u);
  CHECK((px(b, 10, 10) >> 24) == 255 && (px(b, 9, 9) >> 24) == 255, "crossing strokes stay solid");
  rk__path_free(&p);
  rk__bitmap_free(b);
  CHECK(rk__pm(0xff000080u, 1) == 0x80800000u, "premultiplied: %08x", rk__pm(0xff000080u, 1));
  int first, count;
  CHECK(rk__icon_find("archive", &first, &count) && count > 0 && !rk__icon_find("nope", &first, &count), "icons by name");
}

/* Text ------------------------------------------------------------------------------------------------------------ */

static void test_text(void) {
  rk_textfmt f = rk__textfmt(RK_TEXT_BODY, 0);
  CHECK(f.size == 15 && f.weight == 400 && CLOSE_TO(f.line, 1.5, 1e-6), "body style");
  float av = rk__text_width("AV", -1, &f), a = rk__text_width("A", -1, &f), v = rk__text_width("V", -1, &f);
  CHECK(av < a + v - 0.3f, "kerning pulls A and V together: %f vs %f", av, a + v);
  rk_textfmt s = rk__textfmt(RK_TEXT_STAT, 1280);
  CHECK(CLOSE_TO(rk__text_width("1", -1, &s), rk__text_width("8", -1, &s), 1e-3), "tabular figures in a stat");
  CHECK(CLOSE_TO(s.size, RK_CLAMP(0.026f * 1280, 30.0f, 44.0f), 1e-3), "fluid stat size: %f", s.size);
  rk_line lines[8];
  int n = rk__text_wrap("A tile's whole area is one fill, and the fill is its status.", &f, 160, lines, 8);
  CHECK(n >= 3, "wraps: %d lines", n);
  for (int i = 0; i < n; i++) CHECK(lines[i].width <= 160.5f, "line %d fits: %f", i, lines[i].width);
  rk_size m = rk_text_measure("one line", RK_TEXT_SMALL, 0);
  CHECK(CLOSE_TO(m.h, 13 * 1.35, 1e-3), "one line's height: %f", m.h);
}

/* Layout ---------------------------------------------------------------------------------------------------------- */

static void test_layout(void) {
  rk_window *w = window(800, 600);
  rk_view *root = col(8, 16);
  rk_view *row = rk_add(root, rk_row());
  rk_view_set_gap(row, 10);
  rk_view_set_align(row, RK_ALIGN_START, RK_ALIGN_STRETCH);    /* as a plain view: children fill its height */
  rk_view *a = rk_add(row, rk_view_create()), *b = rk_add(row, rk_view_create());
  rk_view_set_size(a, 100, 40);
  rk_view_set_grow(b, 1); rk_view_set_min_size(b, 0, 40);
  rk_view *grid = rk_add(root, rk_tile_grid());
  rk_view *big = rk_add(grid, rk_tile(NULL, NULL));
  rk_view *t1 = rk_add(grid, rk_tile(NULL, NULL)), *t2 = rk_add(grid, rk_tile(NULL, NULL)), *t3 = rk_add(grid, rk_tile(NULL, NULL));
  rk_tile_set_status(big, RK_STATUS_INFO, NULL);
  rk_view_set_span(big, 2, 2);
  rk_view_set_size(grid, 3 * 150 + 2 * 8, 0);
  show(w, root, "layout");
  rk_rect fb = rk_view_frame(b), fa = rk_view_frame(a);
  CHECK(fa.w == 100 && fb.x == 110 && CLOSE_TO(fb.w, 800 - 32 - 110, 0.01), "row: fixed then grow (%g %g)", fb.x, fb.w);
  CHECK(rk_view_frame(t1).x == 316 && rk_view_frame(t1).y == 0, "dense: the first small tile beside the big one");
  CHECK(rk_view_frame(t2).x == 316 && rk_view_frame(t2).y == 158, "dense: the second below it");
  CHECK(rk_view_frame(t3).x == 0 && rk_view_frame(t3).y == 316, "dense: the third in the next row");
  CHECK(rk_view_frame(grid).h == 3 * 150 + 2 * 8, "grid height: %g", rk_view_frame(grid).h);
  /* a resize in a REFLOW block glides: half way, the tile is between its sizes and its content is faded */
  rk_animate_begin(RK_MOTION_REFLOW);
  rk_view_set_span(t1, 2, 1);
  rk_animate_commit();
  CHECK(rk_view_frame(t1).w == 308, "model frame at once: %g", rk_view_frame(t1).w);
  rk_app_run_for(150);
  CHECK(t1->pframe.w > 150 && t1->pframe.w < 308, "presented frame between: %g", t1->pframe.w);
  CHECK(t1->content_alpha == 0, "content faded while it resizes");
  rk_app_run_for(400);
  CHECK(t1->pframe.w == 308 && t1->content_alpha == 1, "settled: %g", t1->pframe.w);
  /* LEAVE: the view leaves first, then the rest close up */
  rk_animate_begin(RK_MOTION_LEAVE);
  rk_view_remove(t2);
  rk_animate_commit();
  CHECK(rk_view_count(grid) == 4 && t2->leaving, "still there while it leaves");
  rk_app_run_for(600);
  CHECK(rk_view_count(grid) == 3, "gone after it left");
  done(w);
}

/* Input ----------------------------------------------------------------------------------------------------------- */

static int activations;
static void on_activate(rk_view *v, void *u) { (void)v; (void)u; activations++; }

static void click(rk_window *w, rk_point p, double hold) {
  rk_event e = { sizeof e, RK_EVENT_POINTER_DOWN, 0, p, 0, 0, 0, 0, 0, NULL };
  rk_window_inject(w, &e);
  rk_app_run_for(hold);
  e.type = RK_EVENT_POINTER_UP;
  rk_window_inject(w, &e);
}

static void key(rk_window *w, uint32_t k, uint32_t mods) {
  rk_event e = { sizeof e, RK_EVENT_KEY_DOWN, 0, { 0, 0 }, 0, 0, 0, k, mods, NULL };
  rk_window_inject(w, &e);
}

static void test_input(void) {
  rk_window *w = window(800, 600);
  rk_view *root = col(8, 16);
  rk_view *grid = rk_add(root, rk_tile_grid());
  rk_view *t = rk_add(grid, rk_tile("0", "presses"));
  rk_tile_set_status(t, RK_STATUS_WARNING, NULL);
  rk_view_on_activate(t, on_activate, NULL);
  rk_view *b = rk_add(root, rk_button("go", on_activate, NULL));
  show(w, root, "input");
  rk_rect r = rk__window_rect(t);
  /* pressed near the top left, the tile tilts there: that edge goes back */
  rk_event e = { sizeof e, RK_EVENT_POINTER_DOWN, 0, { r.x + 10, r.y + 10 }, 0, 0, 0, 0, 0, NULL };
  rk_window_inject(w, &e);
  rk_app_run_for(120);
  CHECK(t->ptransform.rotate_x > 4 && t->ptransform.rotate_y < -4 && CLOSE_TO(t->ptransform.scale_x, 0.97, 0.001), "press pose %g %g %g", t->ptransform.rotate_x, t->ptransform.rotate_y, t->ptransform.scale_x);
  /* released a little outside the tile: the widened hit area still counts it */
  e.type = RK_EVENT_POINTER_UP; e.position = (rk_point){ r.x - 8, r.y + 10 };
  rk_window_inject(w, &e);
  CHECK(activations == 1, "activated on release (%d)", activations);
  rk_app_run_for(400);
  CHECK(t->ptransform.scale_x == 1 && t->ptransform.rotate_x == 0, "back at rest");
  /* the keyboard: Tab reaches the tile, then the button; Enter presses and activates after the sink */
  rk_window_focus(w, NULL);                                   /* the click focused the tile; start from nothing */
  key(w, RK_KEY_TAB, 0);
  CHECK(rk_window_focused(w) == t && w->focus_visible, "Tab focuses the tile");
  key(w, RK_KEY_TAB, 0);
  CHECK(rk_window_focused(w) == b, "then the button");
  key(w, RK_KEY_TAB, RK_MOD_SHIFT);
  CHECK(rk_window_focused(w) == t, "Shift+Tab goes back");
  key(w, RK_KEY_ENTER, 0);
  CHECK(activations == 1, "Enter: the tile sinks first");
  rk_app_run_for(150);
  CHECK(activations == 2, "then activates (%d)", activations);
  /* a peek runs inside the tile */
  rk_view_transition(t, RK_TRANSITION_PEEK);
  rk_tile_set_stat(t, "1", "press");
  rk_app_run_for(150);
  CHECK(t->transition == RK_TRANSITION_PEEK + 1 && t->transition_t > 0.2f && t->transition_t < 0.6f, "peek running: %g", t->transition_t);
  shot(w, "motion-peek");
  rk_app_run_for(400);
  CHECK(t->transition == 0 && !t->snapshot, "peek done");
  done(w);
}

/* Navigation ------------------------------------------------------------------------------------------------------ */

static rk_controller *places_for_test;
static void go_place(rk_view *v, void *u) {
  (void)v; (void)u;
  rk_view *p = col(16, 32);
  rk_add(p, rk_label("A place", RK_TEXT_TITLE));
  rk_view *rows = rk_add(p, rk_rows());
  rk_rows_add(rows, "Nightly import", NULL, RK_STATUS_ERROR);
  rk_rows_add(rows, "schema.sql", "4 kB", RK_STATUS_NONE);
  rk_places_go(places_for_test, rk_controller_create_with_view(p, "a place"));
}

/* Scrolling glides: it starts gently, never overshoots, keeps its speed when the target moves on, and comes to rest in
   about the time it was given. */
static void test_spring(void) {
  rk_window *w = window(400, 300);
  rk_view *v = rk_view_create();
  rk_view_set_size(v, 100, 100);
  rk_retain(v);
  show(w, v, "spring");
  v->pvalue = 0;
  rk__animate_spring(v, RK_PROP_VALUE, 1000, 500);
  float prev = 0, prev_step = 0, max_step = 0, max_jerk = 0;
  bool monotonic = true;
  double t0 = rk_app_now();
  rk_app_run_for(1000.0 / 60);
  CHECK(v->pvalue > 0 && v->pvalue < 40, "it sets off gently (%.1f after one frame)", v->pvalue);
  while (rk__anim_running() && rk_app_now() - t0 < 2000) {
    float step = v->pvalue - prev;
    if (step < -0.01f || v->pvalue > 1000.01f) monotonic = false;
    max_step = RK_MAX(max_step, step);
    if (prev_step > 0) max_jerk = RK_MAX(max_jerk, fabsf(step - prev_step));
    prev_step = step; prev = v->pvalue;
    rk_app_run_for(1000.0 / 60);
  }
  double took = rk_app_now() - t0;
  CHECK(monotonic, "no overshoot");
  CHECK(v->pvalue == 1000, "it lands exactly (%.2f)", v->pvalue);
  CHECK(took > 450 && took < 700, "it rests in about the time given (%.0f ms for 500, a long way)", took);
  CHECK(max_jerk < 0.25f * max_step, "speed changes smoothly frame to frame (jerk %.1f, top step %.1f)", max_jerk, max_step);
  /* a second wheel step while moving: no stop, no jump */
  v->pvalue = 0;
  rk__animate_spring(v, RK_PROP_VALUE, 48, 260);
  rk_app_run_for(80);
  float before = v->pvalue;
  rk_app_run_for(1000.0 / 60);
  float speed = v->pvalue - before;
  rk__animate_spring(v, RK_PROP_VALUE, 96, 260);
  before = v->pvalue;
  rk_app_run_for(1000.0 / 60);
  CHECK(v->pvalue - before >= speed * 0.8f, "a new target keeps the speed (%.2f, was %.2f)", v->pvalue - before, speed);
  rk_app_run_for(600);
  CHECK(v->pvalue == 96 && !rk__anim_running(), "and rests at the new target (%.2f)", v->pvalue);
  rk_release(v);
  done(w);
}

/* The API's one rule: what is added belongs to the tree. Nothing to release, nothing left over. */
static int pressed_count;
static void count_press(rk_view *v, void *u) { (void)v; (void)u; pressed_count++; }

static void test_api(void) {
  int before = rk__live_views;
  rk_window *w = window(600, 400);
  rk_view *page = rk_column();
  rk_view *grid = rk_add(page, rk_tile_grid());
  rk_view *t = rk_add(grid, rk_tile("12", "builds"));
  CHECK(rk_view_parent(t) == grid && rk_view_count(grid) == 1, "rk_add returns the child, in place");
  rk_view *b = rk_add(rk_add(page, rk_row()), rk_button("pin a tile", count_press, NULL));
  CHECK(rk_view_find(page, "pin a tile") == b, "a button is found by its label");
  rk_view_set_name(t, "builds");
  CHECK(rk_view_find(page, "builds") == t, "a view by its name");
  show(w, page, "api");
  rk_view_tap(b);
  CHECK(pressed_count == 1, "rk_view_tap presses it (%d)", pressed_count);
  rk_window_focus(w, b);
  rk_window_key(w, RK_KEY_ENTER);
  rk_app_run_for(200);
  CHECK(pressed_count == 2, "rk_window_key: Enter presses the focused button (%d)", pressed_count);
  rk_view *moved = rk_insert(page, rk_retain(t), 0);           /* moving a view: pass rk_retain() */
  CHECK(moved == t && rk_view_parent(t) == page && rk_view_count(grid) == 0, "rk_insert moves a retained view");
  rk_view_remove(t);
  rk_app_run_for(50);
  done(w);
  rk_app_run_for(50);
  CHECK(rk__live_views == before, "nothing left over (%d views, %d before)", rk__live_views, before);
}

/* A long list draws only the rows in sight; its selection is the Metro way. */
static int item_calls, shown = -1, selection_changes;
static void list_item(int i, rk_list_item *out, void *u) {
  static char title[32];
  (void)u;
  item_calls++;
  snprintf(title, sizeof title, "Build #%d", 4000 + i);
  out->title = title;
  out->status = i % 7 == 3 ? RK_STATUS_ERROR : RK_STATUS_NONE;
  out->meta = i % 7 == 3 ? NULL : "4 kB";
}
static void list_show(rk_view *l, int i, void *u) { (void)l; (void)u; shown = i; }
static void list_change(rk_view *l, void *u) { (void)l; (void)u; selection_changes++; }

static void tap_row(rk_window *w, rk_view *list, int row, uint32_t mods) {
  rk_rect r = rk__window_rect(list);
  rk_point p = { r.x + 40, r.y + (row + 0.5f) * rk_metric_of(RK_ROW_HEIGHT) - list->pvalue };
  rk_event e = { sizeof e, RK_EVENT_POINTER_DOWN, 0, p, 0, 0, 0, 0, mods, NULL };
  rk_window_inject(w, &e);
  e.type = RK_EVENT_POINTER_UP;
  rk_window_inject(w, &e);
}

static void test_list(void) {
  rk_window *w = window(480, 400);
  rk_view *page = col(8, 16);
  rk_view *list = rk_add(page, rk_list(100000, list_item, NULL));
  rk_view_set_size(list, 0, 300);
  rk_list_on_show(list, list_show, NULL);
  rk_list_on_change(list, list_change, NULL);
  show(w, page, "list");
  float H = rk_metric_of(RK_ROW_HEIGHT);
  int in_sight = (int)ceilf(300 / H) + 1;
  item_calls = 0;
  rk_window_snapshot(w, "/dev/null");
  CHECK(item_calls > 0 && item_calls <= in_sight, "a frame asks only for the rows in sight (%d of 100000, %d fit)", item_calls, in_sight);
  tap_row(w, list, 2, 0);
  CHECK(shown == 2 && rk_list_selection_count(list) == 1 && rk_list_selected(list, 2), "a click selects one and shows it (%d)", shown);
  tap_row(w, list, 5, RK_MOD_CTRL);
  CHECK(rk_list_selection_count(list) == 2 && shown == 2, "Ctrl+click toggles, starts selecting several, shows nothing");
  tap_row(w, list, 7, 0);
  CHECK(rk_list_selection_count(list) == 3, "while selecting, a click toggles");
  key(w, RK_KEY_ESCAPE, 0);
  CHECK(rk_list_selection_count(list) == 0, "Escape ends it");
  tap_row(w, list, 3, 0);
  tap_row(w, list, 6, RK_MOD_SHIFT);
  CHECK(rk_list_selection_count(list) == 4 && rk_list_selected(list, 4), "Shift+click takes a range (%d)", rk_list_selection_count(list));
  CHECK(selection_changes >= 6, "every change is told (%d)", selection_changes);
  rk_window_focus(w, list);
  w->focus_visible = true;
  key(w, RK_KEY_DOWN, 0); key(w, RK_KEY_DOWN, 0);
  rk_app_run_for(400);
  shot(w, "list");
  key(w, RK_KEY_END, 0);
  rk_app_run_for(1200);
  CHECK(CLOSE_TO(list->pvalue, 100000 * H - 300, 0.5), "End glides to the last row (%g)", list->pvalue);
  rk_event wheel = { sizeof wheel, RK_EVENT_WHEEL, 0, { 200, 200 }, 0, 0, -480, 0, 0, NULL };
  rk_window_inject(w, &wheel);
  rk_app_run_for(600);
  CHECK(CLOSE_TO(list->pvalue, 100000 * H - 300 - 480, 0.5), "the wheel scrolls it (%g)", list->pvalue);
  rk_list_set_count(list, 5);
  rk_app_run_for(100);
  CHECK(rk_list_selection_count(list) == 2 && list->value == 0, "fewer items: what is gone is no longer selected (%d)", rk_list_selection_count(list));
  done(w);
}

/* A text field: the desktop's keys and the pointer; the caret blinks a while after an edit, then wakes nothing. */
static int field_changes, field_submits;
static void on_field_change(rk_view *f, void *u) { (void)f; (void)u; field_changes++; }
static void on_field_submit(rk_view *f, void *u) { (void)f; (void)u; field_submits++; }

static void type_text(rk_window *w, const char *t) {
  rk_event e = { sizeof e, RK_EVENT_TEXT, 0, { 0, 0 }, 0, 0, 0, 0, 0, t };
  rk_window_inject(w, &e);
}

static void test_field(void) {
  rk_window *w = window(480, 320);
  rk_view *page = col(16, 24);
  rk_view *name = rk_add(page, rk_field("Name", "As it appears on the badge"));
  rk_view *mail = rk_add(page, rk_field("Mail", NULL));
  rk_field_on_change(name, on_field_change, NULL);
  rk_field_on_submit(name, on_field_submit, NULL);
  show(w, page, "field");
  rk_window_focus(w, name);
  type_text(w, "Hallo Welt");
  CHECK(!strcmp(rk_field_text(name), "Hallo Welt") && field_changes == 1, "typing (%s)", rk_field_text(name));
  for (int i = 0; i < 4; i++) key(w, RK_KEY_LEFT, RK_MOD_SHIFT);
  type_text(w, "Rekta");
  CHECK(!strcmp(rk_field_text(name), "Hallo Rekta"), "Shift+Left selects, typing replaces (%s)", rk_field_text(name));
  key(w, RK_KEY_BACKSPACE, RK_MOD_CTRL);
  CHECK(!strcmp(rk_field_text(name), "Hallo "), "Ctrl+Backspace deletes a word (%s)", rk_field_text(name));
  key(w, RK_KEY_HOME, 0); key(w, RK_KEY_END, RK_MOD_SHIFT); key(w, 'C', RK_MOD_CTRL); key(w, RK_KEY_END, 0); key(w, 'V', RK_MOD_CTRL);
  CHECK(!strcmp(rk_field_text(name), "Hallo Hallo "), "copy and paste (%s)", rk_field_text(name));
  key(w, 'A', RK_MOD_CTRL); key(w, 'X', RK_MOD_CTRL);
  CHECK(!strcmp(rk_field_text(name), ""), "Ctrl+A, Ctrl+X");
  type_text(w, "Grüße €");
  key(w, RK_KEY_BACKSPACE, 0);
  CHECK(!strcmp(rk_field_text(name), "Grüße "), "Backspace takes a whole character (%s)", rk_field_text(name));
  key(w, RK_KEY_LEFT, 0); key(w, RK_KEY_LEFT, 0); key(w, RK_KEY_LEFT, 0); key(w, RK_KEY_DELETE, 0);   /* before ß, two bytes */
  CHECK(!strcmp(rk_field_text(name), "Grüe "), "Left over a two-byte character, Delete (%s)", rk_field_text(name));
  key(w, RK_KEY_ENTER, 0);
  CHECK(field_submits == 1, "Enter submits");
  /* the pointer: a click at the start, a double click on the word */
  rk_field_set_text(name, "badge name");
  rk_rect r = rk__window_rect(name);
  float y = r.y + r.h / 2 + 8;
  click(w, (rk_point){ r.x + 13, y }, 10);
  type_text(w, "<");
  CHECK(!strcmp(rk_field_text(name), "<badge name"), "a click places the caret (%s)", rk_field_text(name));
  rk_event d = { sizeof d, RK_EVENT_POINTER_DOWN, rk_app_now(), { r.x + 12 + rk_text_measure("<badge na", RK_TEXT_BODY, 0).w, y }, 0, 0, 0, 0, 0, NULL };
  rk_window_inject(w, &d); d.type = RK_EVENT_POINTER_UP; rk_window_inject(w, &d);
  d.type = RK_EVENT_POINTER_DOWN; d.time += 100; rk_window_inject(w, &d); d.type = RK_EVENT_POINTER_UP; rk_window_inject(w, &d);
  type_text(w, "tag");
  CHECK(!strcmp(rk_field_text(name), "<badge tag"), "a double click takes the word (%s)", rk_field_text(name));
  /* a field that wants something else says why, and how */
  rk_field_set_text(mail, "ada@");
  rk_field_set_hint(mail, "Add the part after @, such as example.com.", true);
  key(w, RK_KEY_HOME, RK_MOD_SHIFT);
  rk_app_run_for(200);
  shot(w, "field");
  CHECK(ntimers > 0, "the caret blinks after an edit");
  rk_app_run_for(11000);
  CHECK(ntimers == 0 && ((field *)name->state)->lit, "ten seconds on, it stays lit and no timer runs (%d)", ntimers);
  done(w);
}

static void test_navigation(void) {
  rk_window *w = window(900, 600);
  rk_controller *home = rk_panorama();
  rk_view *s1 = col(16, 0);
  rk_add(s1, rk_label("Tiles, not cards.", RK_TEXT_GROUP));
  rk_view *grid = rk_add(s1, rk_tile_grid());
  rk_view *t = rk_add(grid, rk_tile("0.3", "release notes"));
  rk_tile_set_status(t, RK_STATUS_INFO, NULL);
  rk_view_on_activate(t, go_place, NULL);
  for (int i = 0; i < 4; i++) rk_tile_set_status(rk_add(grid, rk_tile("12", "builds")), i % 2 ? RK_STATUS_SUCCESS : RK_STATUS_NONE, NULL);
  rk_panorama_add(home, "rekta", s1, 0);
  rk_view *s2 = col(16, 0);
  rk_add(s2, rk_label("Second", RK_TEXT_GROUP));
  rk_panorama_add(home, "second", s2, 0);
  places_for_test = rk_places(home);
  rk_window_show(w, places_for_test);
  rk_app_run_for(100);
  rk_rect r = rk__window_rect(t);
  click(w, (rk_point){ r.x + r.w / 2, r.y + r.h / 2 }, 60);
  CHECK(rk_places_depth(places_for_test) == 2, "a tile goes to a place");
  rk_app_run_for(140);
  shot(w, "motion-turn-out");
  rk_app_run_for(400);
  shot(w, "motion-turn-in");
  rk_app_run_for(800);
  const char *title = rk_controller_title(places_for_test);
  CHECK(title && !strcmp(title, "a place"), "title of the place: %s", title ? title : "(none)");
  shot(w, "place");
  key(w, RK_KEY_ESCAPE, 0);
  CHECK(rk_places_depth(places_for_test) == 1, "Escape goes back");
  rk_app_run_for(1200);
  CHECK(rk_view_window(t) == w && t->popacity == 1 && t->ptransform.rotate_y == 0, "home is back as it was");
  /* the wheel moves the panorama sideways; the strip follows */
  rk_event wheel = { sizeof wheel, RK_EVENT_WHEEL, 0, { 450, 300 }, 0, 0, 600, 0, 0, NULL };
  rk_window_inject(w, &wheel);
  rk_app_run_for(800);
  CHECK(rk_controller_view(home)->value > 0, "scrolled: %g", rk_controller_view(home)->value);
  /* a sheet from the top edge; Escape lets it leave the way it came */
  rk_view *q = col(16, 0);
  rk_add(q, rk_label("Reset the demo?", RK_TEXT_GROUP));
  rk_view *btn = rk_add(q, rk_button("reset", NULL, NULL));
  rk_button_set_kind(btn, RK_BUTTON_PRIMARY);
  rk_controller_present(home, rk_controller_create_with_view(q, "Reset?"));
  rk_app_run_for(400);
  CHECK(w->sheets->nkids == 2 && rk_window_focused(w) == btn, "a sheet: shading and box, focus inside");
  shot(w, "sheet");
  key(w, RK_KEY_ESCAPE, 0);
  rk_app_run_for(400);
  CHECK(w->sheets->nkids == 0, "dismissed");
  rk_window_toast(w, "One thing archived.", "undo", NULL, NULL);
  rk_app_run_for(400);
  CHECK(w->toasts->nkids == 1, "a toast");
  shot(w, "toast");
  rk_app_run_for(5400);
  CHECK(w->toasts->nkids == 0, "gone after its time");
  done(w);
}

/* Scenes for the reference images --------------------------------------------------------------------------------- */

static rk_view *add_tile(rk_view *grid, rk_status st, const char *v, const char *l, int c, int r) {
  rk_view *t = rk_add(grid, rk_tile(v, l));
  rk_tile_set_status(t, st, NULL);
  rk_view_set_span(t, c, r);
  return t;
}

static void scene_tiles(const char *name, float scale) {
  rk_window *w = window(720, 560);
  if (scale != 1) rk__window_resized(w, 720, 560, scale);
  rk_view *root = col(16, 32);
  rk_add(root, rk_label("Tiles, not cards.", RK_TEXT_GROUP));
  rk_view *grid = rk_add(root, rk_tile_grid());
  rk_view *seen = add_tile(grid, RK_STATUS_INFO, "1 / 3", "sections seen", 2, 2);
  rk_tile_set_message(seen, "Every section you move to counts.");
  rk_tile_set_progress(seen, 0.33f);
  add_tile(grid, RK_STATUS_WARNING, "4", "waiting", 1, 1);
  add_tile(grid, RK_STATUS_ERROR, "1", "failed", 1, 1);
  float s[12] = { 4, 6, 5, 9, 8, 12, 11, 15, 13, 16, 14, 18 };
  rk_tile_set_series(add_tile(grid, RK_STATUS_NONE, "118", "requests a second", 2, 1), s, 12);
  add_tile(grid, RK_STATUS_SUCCESS, "312", "finished today", 1, 1);
  show(w, root, "tiles");
  shot(w, name);
  done(w);
}

static void scene_type(void) {
  rk_window *w = window(720, 520);
  rk_view *root = col(8, 32);
  rk_add(root, rk_label("Title, semibold", RK_TEXT_TITLE));
  rk_add(root, rk_label("A group heading", RK_TEXT_GROUP));
  rk_add(root, rk_label("a kicker, tracked", RK_TEXT_KICKER));
  rk_add(root, rk_label("Body text in Barlow at size-s with the body line height, wrapping at the width of its column as a paragraph does. AV To Wa.", RK_TEXT_BODY));
  rk_add(root, rk_label("Strong text", RK_TEXT_STRONG));
  rk_add(root, rk_label("Small text", RK_TEXT_SMALL));
  rk_add(root, rk_label("label", RK_TEXT_LABEL));
  rk_add(root, rk_label("meta 0123456789", RK_TEXT_META));
  rk_add(root, rk_label("summary", RK_TEXT_PIVOT));
  rk_view *icons = rk_add(root, rk_row());
  rk_view_set_gap(icons, 12);
  rk_view_set_align(icons, RK_ALIGN_START, RK_ALIGN_STRETCH);
  for (int i = 0; i < RK_ICON_COUNT; i += 3) rk_add(icons, rk_icon(rk_icon_defs[i].name, RK_ROLE_TEXT));
  show(w, root, "type");
  shot(w, "type");
  done(w);
}

static void scene_controls(const char *name) {
  rk_window *w = window(720, 420);
  rk_view *root = col(16, 32);
  rk_view *row = rk_add(root, rk_row());
  rk_button_set_kind(rk_add(row, rk_button("primary", NULL, NULL)), RK_BUTTON_PRIMARY);
  rk_add(row, rk_button("default", NULL, NULL));
  rk_button_set_kind(rk_add(row, rk_button("quiet", NULL, NULL)), RK_BUTTON_QUIET);
  rk_button_set_disabled(rk_add(row, rk_button("disabled", NULL, NULL)), "nothing to do yet");
  rk_view *bar = rk_add(row, rk_button("archive", NULL, NULL));
  rk_button_set_kind(bar, RK_BUTTON_BAR);
  rk_button_set_icon(bar, "archive");
  rk_view *rows = rk_add(root, rk_rows());
  rk_rows_add(rows, "Nightly import", NULL, RK_STATUS_ERROR);
  rk_rows_add(rows, "Release notes", NULL, RK_STATUS_WARNING);
  rk_rows_add(rows, "Deploy to staging", NULL, RK_STATUS_SUCCESS);
  rk_rows_add(rows, "schema.sql", "4 kB", RK_STATUS_NONE);
  rk_add(root, rk_progress(0.6f));
  show(w, root, "controls");
  shot(w, name);
  done(w);
}

static void scene_flip(void) {
  rk_window *w = window(400, 240);
  rk_view *root = col(16, 32);
  rk_view *t = add_tile(rk_add(root, rk_tile_grid()), RK_STATUS_INFO, "3", "runners", 1, 1);
  show(w, root, "flip");
  rk_view_transition(t, RK_TRANSITION_FLIP);
  rk_tile_set_status(t, RK_STATUS_ERROR, NULL);
  rk_app_run_for(100);
  shot(w, "motion-flip-out");
  rk_app_run_for(200);
  shot(w, "motion-flip-in");
  rk_app_run_for(400);
  done(w);
}

/* Main ------------------------------------------------------------------------------------------------------------ */

static void launch(void *user) {
  (void)user;
  /* a window that stays open, so closing a test's window does not end the app */
  rk_window *keeper = window(100, 100);
  test_tokens();
  test_raster();
  test_text();
  test_layout();
  test_input();
  test_api();
  test_list();
  test_field();
  test_navigation();
  test_spring();
  scene_tiles("tiles", 1);
  scene_tiles("tiles-2x", 2);                                  /* twice the pixels: only sharper */
  scene_type();
  scene_controls("controls");
  scene_flip();
  rk_app_set_theme(RK_THEME_DARK);
  scene_tiles("tiles-dark", 1);
  scene_controls("controls-dark");
  rk_app_set_theme(RK_THEME_SYSTEM);
  rk__prefs.system_reduced = false;
  rk_app_run_for(400);
  done(keeper);
  CHECK(rk__live_views == 0, "every view released: %d left", rk__live_views);
  rk_app_quit(0);
}

int main(int argc, char **argv) {
  setvbuf(stdout, NULL, _IONBF, 0);
  if (argc > 1) out = argv[1];
#if defined(_WIN32)
  CreateDirectoryA(out, NULL);
#else
  char cmd[1100];
  snprintf(cmd, sizeof cmd, "mkdir -p '%s'", out);
  if (system(cmd)) return 2;
#endif
  rk_app_desc d = { sizeof d, "test", "de.rekta.test", launch, NULL, NULL, true };
  rk_app_main(&d);
  printf("%d checks passed, %d failed\n", passes, fails);
  return fails ? 1 : 0;
}

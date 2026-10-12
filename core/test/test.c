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
  rk_window_desc d = { sizeof d, "test", w, h, 0, 0, false };
  rk_window *win = rk_window_create(&d);
  return win;
}

/* takes the caller's reference to content */
static void show(rk_window *w, rk_view *content, const char *title) {
  rk_controller *c = rk_controller_create_with_view(content, title);
  rk_release(content);
  rk_window_set_root(w, c);
  rk_release(c);
  rk_window_show(w);
  rk_app_run_for(50);
}

static void done(rk_window *w) { rk_window_close(w); rk_release(w); }

static rk_view *col(float gap, float pad) {
  rk_view *v = rk_view_create();
  rk_view_set_layout(v, RK_LAYOUT_COLUMN);
  rk_view_set_gap(v, gap);
  rk_view_set_padding(v, (rk_insets){ pad, pad, pad, pad });
  return v;
}
static void add(rk_view *p, rk_view *c) { rk_view_add(p, c); rk_release(c); }

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
  rk_view *row = rk_view_create();
  rk_view_set_layout(row, RK_LAYOUT_ROW);
  rk_view_set_gap(row, 10);
  rk_view *a = rk_view_create(), *b = rk_view_create();
  rk_view_set_size(a, 100, 40);
  rk_view_set_grow(b, 1); rk_view_set_min_size(b, 0, 40);
  add(row, a); add(row, b);
  add(root, row);
  rk_view *grid = rk_view_create();
  rk_view_set_layout(grid, RK_LAYOUT_TILES);
  rk_view *big = rk_tile_create(RK_STATUS_INFO, NULL), *t1 = rk_tile_create(RK_STATUS_NONE, NULL), *t2 = rk_tile_create(RK_STATUS_NONE, NULL), *t3 = rk_tile_create(RK_STATUS_NONE, NULL);
  rk_view_set_span(big, 2, 2);
  add(grid, big); add(grid, t1); add(grid, t2); add(grid, t3);
  rk_view_set_size(grid, 3 * 150 + 2 * 8, 0);
  add(root, grid);
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
  rk_view *grid = rk_view_create();
  rk_view_set_layout(grid, RK_LAYOUT_TILES);
  rk_view *t = rk_tile_create(RK_STATUS_WARNING, NULL);
  rk_tile_set_stat(t, "0", "presses");
  rk_view_on_activate(t, on_activate, NULL);
  add(grid, t);
  rk_view *b = rk_button_create("go", NULL, RK_BUTTON_DEFAULT);
  rk_view_on_activate(b, on_activate, NULL);
  add(root, grid); add(root, b);
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
  add(p, rk_label_create("A place", RK_TEXT_TITLE));
  rk_view *rows = rk_rows_create();
  rk_rows_add(rows, "Nightly import", NULL, RK_STATUS_ERROR);
  rk_rows_add(rows, "schema.sql", "4 kB", RK_STATUS_NONE);
  add(p, rows);
  rk_controller *c = rk_controller_create_with_view(p, "a place");
  rk_release(p);
  rk_places_go(places_for_test, c);
  rk_release(c);
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

static void test_navigation(void) {
  rk_window *w = window(900, 600);
  rk_controller *home = rk_panorama_create();
  rk_view *s1 = col(16, 0), *grid = rk_view_create();
  add(s1, rk_label_create("Tiles, not cards.", RK_TEXT_GROUP));
  rk_view_set_layout(grid, RK_LAYOUT_TILES);
  rk_view *t = rk_tile_create(RK_STATUS_INFO, NULL);
  rk_tile_set_stat(t, "0.3", "release notes");
  rk_view_on_activate(t, go_place, NULL);
  add(grid, t);
  for (int i = 0; i < 4; i++) { rk_view *x = rk_tile_create(i % 2 ? RK_STATUS_SUCCESS : RK_STATUS_NONE, NULL); rk_tile_set_stat(x, "12", "builds"); add(grid, x); }
  add(s1, grid);
  rk_panorama_add(home, "rekta", s1, 0); rk_release(s1);
  rk_view *s2 = col(16, 0);
  add(s2, rk_label_create("Second", RK_TEXT_GROUP));
  rk_panorama_add(home, "second", s2, 0); rk_release(s2);
  places_for_test = rk_places_create(home);
  rk_window_set_root(w, places_for_test);
  rk_window_show(w);
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
  add(q, rk_label_create("Reset the demo?", RK_TEXT_GROUP));
  rk_view *btn = rk_button_create("reset", NULL, RK_BUTTON_PRIMARY);
  add(q, btn);
  rk_controller *sheet = rk_controller_create_with_view(q, "Reset?");
  rk_release(q);
  rk_controller_present(home, sheet);
  rk_app_run_for(400);
  CHECK(w->sheets->nkids == 2 && rk_window_focused(w) == btn, "a sheet: shading and box, focus inside");
  shot(w, "sheet");
  key(w, RK_KEY_ESCAPE, 0);
  rk_app_run_for(400);
  CHECK(w->sheets->nkids == 0, "dismissed");
  rk_release(sheet);
  rk_window_toast(w, "One thing archived.", "undo", NULL, NULL);
  rk_app_run_for(400);
  CHECK(w->toasts->nkids == 1, "a toast");
  shot(w, "toast");
  rk_app_run_for(5400);
  CHECK(w->toasts->nkids == 0, "gone after its time");
  done(w);
  rk_release(places_for_test); rk_release(home);
}

/* Scenes for the reference images --------------------------------------------------------------------------------- */

static rk_view *make_tile(rk_status st, const char *v, const char *l, int c, int r) {
  rk_view *t = rk_tile_create(st, NULL);
  rk_tile_set_stat(t, v, l);
  rk_view_set_span(t, c, r);
  return t;
}

static void scene_tiles(const char *name, float scale) {
  rk_window *w = window(720, 560);
  if (scale != 1) rk__window_resized(w, 720, 560, scale);
  rk_view *root = col(16, 32);
  add(root, rk_label_create("Tiles, not cards.", RK_TEXT_GROUP));
  rk_view *grid = rk_view_create();
  rk_view_set_layout(grid, RK_LAYOUT_TILES);
  rk_view *seen = make_tile(RK_STATUS_INFO, "1 / 3", "sections seen", 2, 2);
  rk_tile_set_message(seen, "Every section you move to counts.");
  rk_tile_set_progress(seen, 0.33f);
  add(grid, seen);
  add(grid, make_tile(RK_STATUS_WARNING, "4", "waiting", 1, 1));
  add(grid, make_tile(RK_STATUS_ERROR, "1", "failed", 1, 1));
  rk_view *live = make_tile(RK_STATUS_NONE, "118", "requests a second", 2, 1);
  float s[12] = { 4, 6, 5, 9, 8, 12, 11, 15, 13, 16, 14, 18 };
  rk_tile_set_series(live, s, 12);
  add(grid, live);
  add(grid, make_tile(RK_STATUS_SUCCESS, "312", "finished today", 1, 1));
  add(root, grid);
  show(w, root, "tiles");
  shot(w, name);
  done(w);
}

static void scene_type(void) {
  rk_window *w = window(720, 520);
  rk_view *root = col(8, 32);
  add(root, rk_label_create("Title, semibold", RK_TEXT_TITLE));
  add(root, rk_label_create("A group heading", RK_TEXT_GROUP));
  add(root, rk_label_create("a kicker, tracked", RK_TEXT_KICKER));
  add(root, rk_label_create("Body text in Barlow at size-s with the body line height, wrapping at the width of its column as a paragraph does. AV To Wa.", RK_TEXT_BODY));
  add(root, rk_label_create("Strong text", RK_TEXT_STRONG));
  add(root, rk_label_create("Small text", RK_TEXT_SMALL));
  add(root, rk_label_create("label", RK_TEXT_LABEL));
  add(root, rk_label_create("meta 0123456789", RK_TEXT_META));
  add(root, rk_label_create("summary", RK_TEXT_PIVOT));
  rk_view *icons = rk_view_create();
  rk_view_set_layout(icons, RK_LAYOUT_ROW);
  rk_view_set_gap(icons, 12);
  for (int i = 0; i < RK_ICON_COUNT; i += 3) add(icons, rk_icon_create(rk_icon_defs[i].name, RK_ROLE_TEXT));
  add(root, icons);
  show(w, root, "type");
  shot(w, "type");
  done(w);
}

static void scene_controls(const char *name) {
  rk_window *w = window(720, 420);
  rk_view *root = col(16, 32);
  rk_view *row = rk_view_create();
  rk_view_set_layout(row, RK_LAYOUT_ROW);
  rk_view_set_gap(row, 8);
  rk_view_set_align(row, RK_ALIGN_START, RK_ALIGN_START);
  add(row, rk_button_create("primary", NULL, RK_BUTTON_PRIMARY));
  add(row, rk_button_create("default", NULL, RK_BUTTON_DEFAULT));
  add(row, rk_button_create("quiet", NULL, RK_BUTTON_QUIET));
  rk_view *off = rk_button_create("disabled", NULL, RK_BUTTON_DEFAULT);
  rk_button_set_disabled(off, "nothing to do yet");
  add(row, off);
  add(row, rk_button_create("archive", "archive", RK_BUTTON_BAR));
  add(root, row);
  rk_view *rows = rk_rows_create();
  rk_rows_add(rows, "Nightly import", NULL, RK_STATUS_ERROR);
  rk_rows_add(rows, "Release notes", NULL, RK_STATUS_WARNING);
  rk_rows_add(rows, "Deploy to staging", NULL, RK_STATUS_SUCCESS);
  rk_rows_add(rows, "schema.sql", "4 kB", RK_STATUS_NONE);
  add(root, rows);
  add(root, rk_progress_create(0.6f));
  show(w, root, "controls");
  shot(w, name);
  done(w);
}

static void scene_flip(void) {
  rk_window *w = window(400, 240);
  rk_view *root = col(16, 32);
  rk_view *grid = rk_view_create();
  rk_view_set_layout(grid, RK_LAYOUT_TILES);
  rk_view *t = make_tile(RK_STATUS_INFO, "3", "runners", 1, 1);
  add(grid, t);
  add(root, grid);
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

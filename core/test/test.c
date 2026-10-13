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

/* Text: where lines may break (CJK between characters, kinsoku), and a face for what Barlow lacks. */
static int breaks(const char *s, int *at, int max) {
  int n = 0, len = (int)strlen(s);
  for (int i = 0; i < len && n < max;) { i = rk__text_next_break(s, i, len); at[n++] = i; }
  return n;
}

static void test_fonts(void) {
  int at[16], n;
  n = breaks("two words-here", at, 16);
  CHECK(n == 3 && at[0] == 4 && at[1] == 10 && at[2] == 14, "Latin: after spaces and hyphens (%d: %d %d %d)", n, at[0], at[1], at[2]);
  n = breaks("\xe6\xbc\xa2\xe5\xad\x97\xe3\x80\x82\xe3\x81\x8b", at, 16);   /* 漢字。か */
  CHECK(n == 3 && at[0] == 3 && at[1] == 9 && at[2] == 12, "CJK: between characters, never before 。 (%d: %d %d %d)", n, at[0], at[1], at[2]);
  n = breaks("\xe3\x81\x82\xe3\x80\x8c\xe3\x81\x84", at, 16);   /* あ「い */
  CHECK(n == 2 && at[0] == 3 && at[1] == 9, "nor after 「 (%d: %d %d)", n, at[0], at[1]);
  n = breaks("Rekta \xed\x95\x9c\xea\xb8\x80", at, 16);   /* Rekta 한글 */
  CHECK(n == 3 && at[0] == 6 && at[1] == 9 && at[2] == 12, "Latin, then Hangul (%d: %d %d %d)", n, at[0], at[1], at[2]);
  rk_textfmt f = rk__textfmt(RK_TEXT_BODY, 0);
  const char *jp = "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe3\x81\xae\xe6\x96\x87\xe7\xab\xa0\xe3\x81\xaf\xe7\xa9\xba\xe7\x99\xbd\xe3\x81\xaa\xe3\x81\x97\xe3\x81\xa7\xe6\x8a\x98\xe3\x82\x8a\xe8\xbf\x94\xe3\x81\x99\xe3\x80\x82";
  rk_line lines[16];
  float one = rk__text_width("\xe6\x97\xa5", -1, &f);
  n = rk__text_wrap(jp, &f, one * 5.5f, lines, 16);
  bool fit = n > 2;
  for (int i = 0; i < n; i++) fit = fit && lines[i].width <= one * 5.5f + 0.5f && lines[i].len % 3 == 0;
  CHECK(fit, "Japanese without spaces wraps at the width, a character at a time (%d lines)", n);
  CHECK(!memcmp(jp + lines[n - 1].start + lines[n - 1].len - 3, "\xe3\x80\x82", 3) && lines[n - 1].len > 3, "and 。 never starts a line");

  /* a face for what Barlow lacks: any font this system has that has one of these */
  static const char *const files[] = { "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", "/usr/share/fonts/TTF/DejaVuSans.ttf", "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",
                                       "C:\\Windows\\Fonts\\seguisym.ttf", "C:\\Windows\\Fonts\\arial.ttf", "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
                                       "/System/Library/Fonts/Apple Symbols.ttf" };
  static const int wanted[] = { 0x2211, 0x221e, 0x2318, 0x2603, 0xd55c, 0x6f22 };   /* ∑ ∞ ⌘ ☃ 한 漢 */
  font *barlow = font_for(400);
  const char *file = NULL;
  for (size_t i = 0; i < sizeof files / sizeof *files && !file; i++) { FILE *t = fopen(files[i], "rb"); if (t) { fclose(t); file = files[i]; } }
  if (!file) { printf("  (no font to fall back on here: the fallback checks are skipped)\n"); return; }
  CHECK(rk_app_add_font(file), "an app's font is taken (%s)", file);
  int cp = 0;
  for (size_t i = 0; i < sizeof wanted / sizeof *wanted && !cp; i++)
    if (!stbtt_FindGlyphIndex(&barlow->info, wanted[i]) && face_has(&fonts[RK_FONT_FILE_COUNT], wanted[i])) cp = wanted[i];
  if (!cp) { printf("  (%s has none of the characters Barlow lacks: skipped)\n", file); return; }
  unsigned char u[5] = { 0 };
  rk__utf8_put(u, cp);
  int g = 0, gb = 0;
  font *face = fallback_for(cp, 400, &g), *bold = fallback_for(cp, 600, &gb);
  CHECK(face == &fonts[RK_FONT_FILE_COUNT] && g && bold == face, "U+%04X comes from it, at any weight", cp);
  rk_window *w = window(200, 80);
  rk_view *l = rk_label((const char *)u, RK_TEXT_TITLE);
  show(w, l, "fallback");
  rk_app_run_for(50);
  rk_bitmap *b = w->fb;
  rk_rect r = rk__window_rect(l);
  int ink = 0;
  for (int y = (int)r.y; b && y < (int)(r.y + r.h) && y < b->h; y++)
    for (int x = (int)r.x; x < (int)(r.x + r.w) && x < b->w; x++) if ((b->px[y * b->stride + x] & 0xff) < 0x80) ink++;
  CHECK(ink > 20, "and is drawn (%d dark pixels)", ink);
  CHECK(!rk_app_add_font("/no/such/font.ttf"), "a file that is not there is refused");
  done(w);
}

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
  /* an input method: what it composes shows at the caret and is not text yet; what it commits is */
  rk_event pe = { sizeof pe, RK_EVENT_COMPOSE, 0, { 0, 0 }, 0, 0, 0, 0, 0, "ka" };
  rk_window_inject(w, &pe);
  CHECK(!strcmp(rk_field_text(name), "Grüe ") && ((field *)name->state)->preedit, "composing is shown, not typed");
  type_text(w, "ko");
  pe.text = "";
  rk_window_inject(w, &pe);
  CHECK(!strcmp(rk_field_text(name), "Grükoe ") && !((field *)name->state)->preedit, "the committed text is typed at the caret (%s)", rk_field_text(name));
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
  /* the field's own history: typing runs together a word at a time */
  rk_field_set_text(name, "");
  const char *keys[] = { "a", "b", " ", "c", "d" };
  for (int i = 0; i < 5; i++) type_text(w, keys[i]);
  key(w, 'Z', RK_MOD_CTRL);
  CHECK(!strcmp(rk_field_text(name), "ab"), "Ctrl+Z takes back the last word (%s)", rk_field_text(name));
  key(w, 'Z', RK_MOD_CTRL);
  CHECK(!strcmp(rk_field_text(name), ""), "and the one before (%s)", rk_field_text(name));
  key(w, 'Z', RK_MOD_CTRL | RK_MOD_SHIFT); key(w, 'Y', RK_MOD_CTRL);
  CHECK(!strcmp(rk_field_text(name), "ab cd"), "Ctrl+Shift+Z and Ctrl+Y redo (%s)", rk_field_text(name));
  rk_field_set_text(name, "<badge tag");
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

/* Menus float by what opened them; keys and choices as states.md says. */
static int menu_chosen = -1, menu_calls;
static void on_menu(rk_view *m, int item, void *u) { (void)m; (void)u; menu_chosen = item; menu_calls++; }

static void test_menu(void) {
  rk_window *w = window(560, 420);
  rk_view *page = col(16, 24);
  rk_view *row = rk_add(page, rk_row());
  rk_view *more = rk_add(row, rk_button("more", NULL, NULL));
  rk_view *m = rk_menu();
  int open_i = rk_menu_add(m, "open", "Enter", on_menu, NULL);
  int rename_i = rk_menu_add(m, "rename", "F2", on_menu, NULL);
  rk_menu_add_separator(m);
  rk_menu_add_label(m, "show");
  int done_i = rk_menu_add(m, "done", NULL, on_menu, NULL);
  int failed_i = rk_menu_add(m, "failed", NULL, on_menu, NULL);
  int delete_i = rk_menu_add(m, "delete (needs rights)", "Del", on_menu, NULL);
  rk_menu_set_check(m, done_i, 1);
  rk_menu_set_check(m, failed_i, 0);
  rk_menu_set_disabled(m, delete_i, true);
  rk_view_set_menu(more, m);
  rk_view *tile = rk_add(rk_add(page, rk_tile_grid()), rk_tile("12", "builds"));
  rk_view *ctx = rk_menu();
  rk_menu_add(ctx, "pin to start", NULL, on_menu, NULL);
  rk_view_set_menu(tile, ctx);
  show(w, page, "menu");

  rk_view_tap(more);
  rk_app_run_for(200);
  rk_rect b = rk__window_rect(more), mr = rk__window_rect(m);
  CHECK(w->floats->nkids == 1 && rk_window_focused(w) == m && rk_menu_opener(m) == more, "a menu button opens its menu, focus in it");
  CHECK(mr.y >= b.y + b.h && fabsf(mr.x - b.x) < 1, "below the button, at its leading edge (%g %g)", mr.x, mr.y);
  key(w, RK_KEY_DOWN, 0); key(w, RK_KEY_DOWN, 0);
  key(w, RK_KEY_ENTER, 0);
  CHECK(menu_chosen == rename_i && w->floats->nkids == 0 && rk_window_focused(w) == more, "Down, Enter: chosen, closed, focus back (%d)", menu_chosen);
  key(w, RK_KEY_ENTER, 0);                                     /* the button, from the keyboard */
  rk_app_run_for(200);
  CHECK(w->floats->nkids == 1 && ((menu *)m->state)->active == open_i, "from the keyboard, the first item is active");
  key(w, 'D', 0);
  CHECK(((menu *)m->state)->active == done_i, "a letter jumps to the next item starting with it");
  key(w, RK_KEY_UP, 0); key(w, RK_KEY_UP, 0); key(w, RK_KEY_UP, 0);
  CHECK(((menu *)m->state)->active == delete_i, "Up wraps, past the label and the line (%d)", ((menu *)m->state)->active);
  menu_calls = 0;
  key(w, RK_KEY_ENTER, 0);
  CHECK(menu_calls == 0 && w->floats->nkids == 1, "a disabled item does nothing");
  key(w, RK_KEY_UP, 0); key(w, RK_KEY_SPACE, 0);
  CHECK(rk_menu_check(m, failed_i) == 1 && w->floats->nkids == 1 && menu_chosen == failed_i, "a check toggles and the menu stays");
  rk_app_run_for(100);
  shot(w, "menu");
  key(w, RK_KEY_ESCAPE, 0);
  CHECK(w->floats->nkids == 0 && rk_window_focused(w) == more, "Escape closes, focus back");
  rk_view_tap(more);
  click(w, (rk_point){ 500, 380 }, 10);
  CHECK(w->floats->nkids == 0, "a press outside closes it");
  /* a context menu: a right click at the pointer, Shift+F10 below the view */
  rk_rect t = rk__window_rect(tile);
  rk_event rc = { sizeof rc, RK_EVENT_POINTER_DOWN, 0, { t.x + 40, t.y + 30 }, 1, 0, 0, 0, 0, NULL };
  rk_window_inject(w, &rc); rc.type = RK_EVENT_POINTER_UP; rk_window_inject(w, &rc);
  rk_rect cr = rk__window_rect(ctx);
  CHECK(w->floats->nkids == 1 && rk_menu_opener(ctx) == tile && fabsf(cr.x - (t.x + 40)) < 1, "a right click opens the context menu at the pointer");
  key(w, RK_KEY_ESCAPE, 0);
  rk_window_focus(w, tile->focusable ? tile : NULL);
  tile->focusable = true; rk_window_focus(w, tile);
  key(w, RK_KEY_F10, RK_MOD_SHIFT);
  CHECK(w->floats->nkids == 1 && rk_window_focused(w) == ctx, "Shift+F10 opens it from the keyboard");
  key(w, RK_KEY_ESCAPE, 0);
  /* near the bottom edge: above */
  rk_menu_open_at(ctx, tile, (rk_point){ 10, 380 - t.y });
  cr = rk__window_rect(ctx);
  CHECK(cr.y + cr.h <= 420 - 15, "never outside the window (%g)", cr.y + cr.h);
  rk_menu_close(ctx);
  done(w);
}

/* A table is a list with columns: the head stays, a click on a sortable head sorts. */
static int sorted_col = -2, sorted_dir;
static const char *table_cell(int row, int col, void *u) {
  static char buf[32];
  (void)u;
  if (col == 0) { snprintf(buf, sizeof buf, "Build #%d", 4000 + row); return buf; }
  if (col == 1) return row % 3 ? "done" : "failed";
  snprintf(buf, sizeof buf, "%d min", 3 + (row * 7) % 40);
  return buf;
}
static void table_sort(rk_view *t, int col, int dir, void *u) { (void)t; (void)u; sorted_col = col; sorted_dir = dir; }

static void test_table(void) {
  rk_window *w = window(560, 360);
  rk_view *page = col(8, 16);
  rk_view *t = rk_add(page, rk_table(1000, table_cell, NULL));
  rk_table_add_column(t, "build", 0, RK_COLUMN_SORTABLE);
  rk_table_add_column(t, "status", 120, 0);
  rk_table_add_column(t, "time", 100, RK_COLUMN_NUMERIC | RK_COLUMN_SORTABLE);
  rk_table_on_sort(t, table_sort, NULL);
  rk_list_on_show(t, list_show, NULL);
  rk_view_set_size(t, 0, 280);
  show(w, page, "table");
  rk_rect r = rk__window_rect(t);
  click(w, (rk_point){ r.x + 60, r.y + 10 }, 10);
  CHECK(sorted_col == 0 && sorted_dir == 1, "a click on a sortable head sorts it ascending");
  click(w, (rk_point){ r.x + 60, r.y + 10 }, 10);
  CHECK(sorted_col == 0 && sorted_dir == -1, "again: descending");
  click(w, (rk_point){ r.x + r.w - 160, r.y + 10 }, 10);
  CHECK(sorted_col == 0, "a head that does not sort does nothing");
  click(w, (rk_point){ r.x + r.w - 40, r.y + 10 }, 10);
  CHECK(sorted_col == 2 && sorted_dir == 1, "the time column sorts");
  float H = rk_metric_of(RK_ROW_HEIGHT), hh = ((list *)t->state)->ncols ? head_height(t->state) : 0;
  shown = -1;
  click(w, (rk_point){ r.x + 60, r.y + hh + 2.5f * H }, 10);
  CHECK(shown == 2 && rk_list_selected(t, 2), "a click on a row selects it and shows it (%d)", shown);
  rk_list_scroll_to(t, 500);
  rk_app_run_for(900);
  CHECK(t->pvalue > 400 * H && rk_window_view_at(w, (rk_point){ r.x + 60, r.y + 10 }) == t, "scrolled far; the head stays on top");
  rk_list_scroll_to(t, 0);
  rk_app_run_for(900);
  shot(w, "table");
  /* a column's edge drags its width; twice on it gives the column its own back; the pointer shows the edge */
  list *L = t->state;
  float edge = r.x + L->cols[1].x + L->cols[1].w, y = r.y + 10;
  rk_event mv = { sizeof mv, RK_EVENT_POINTER_MOVE, 0, { edge + 2, y }, 0, 0, 0, 0, 0, NULL };
  rk_window_inject(w, &mv);
  CHECK(w->cursor == RK__CURSOR_COL_RESIZE, "over a column's edge the pointer shows it (%d)", w->cursor);
  rk_event dn = { sizeof dn, RK_EVENT_POINTER_DOWN, 0, { edge, y }, 0, 0, 0, 0, 0, NULL };
  rk_window_inject(w, &dn);
  mv.position = (rk_point){ edge + 60, y + 30 }; rk_window_inject(w, &mv);
  dn.type = RK_EVENT_POINTER_UP; dn.position = mv.position; rk_window_inject(w, &dn);
  CHECK(CLOSE_TO(rk_table_column_width(t, 1), 180, 0.5) && sorted_col == 2, "a drag widens it, and sorts nothing (%.0f)", rk_table_column_width(t, 1));
  mv.position = (rk_point){ r.x + 60, r.y + hh + 2 * H }; rk_window_inject(w, &mv);
  CHECK(w->cursor == RK__CURSOR_ARROW, "over the rows, the arrow");
  edge = r.x + L->cols[1].x + L->cols[1].w;
  dn.type = RK_EVENT_POINTER_DOWN; dn.position = (rk_point){ edge, y }; rk_window_inject(w, &dn);
  mv.position = (rk_point){ edge - 500, y }; rk_window_inject(w, &mv);
  dn.type = RK_EVENT_POINTER_UP; rk_window_inject(w, &dn);
  CHECK(rk_table_column_width(t, 1) == 48, "never narrower than 48 (%.0f)", rk_table_column_width(t, 1));
  rk_app_run_for(500);
  edge = r.x + L->cols[1].x + L->cols[1].w;
  click(w, (rk_point){ edge, y }, 10); click(w, (rk_point){ edge, y }, 10);
  CHECK(rk_table_column_width(t, 1) == 120, "a double click on the edge: its own width again (%.0f)", rk_table_column_width(t, 1));
  /* the head from the keyboard: Up from the first row goes in; sorting and widths without the pointer */
  rk_window_focus(w, t);
  key(w, RK_KEY_HOME, 0); key(w, RK_KEY_UP, 0);
  CHECK(L->head == 2, "Up from the first row: into the head, at the sorted column (%d)", L->head);
  key(w, RK_KEY_LEFT, 0); key(w, RK_KEY_ENTER, 0);
  CHECK(L->head == 1 && sorted_col == 2, "Left moves; Enter on a column that does not sort does nothing");
  key(w, RK_KEY_LEFT, 0); key(w, RK_KEY_ENTER, 0);
  CHECK(sorted_col == 0 && sorted_dir == 1, "Enter sorts by the column (%d %d)", sorted_col, sorted_dir);
  float w0 = rk_table_column_width(t, 0), m = rk_metric_of(RK_SPACE_M);
  key(w, RK_KEY_RIGHT, RK_MOD_CTRL);
  CHECK(CLOSE_TO(rk_table_column_width(t, 0), w0 + m, 0.5), "Ctrl+Right widens it by space-m (%.0f)", rk_table_column_width(t, 0));
  key(w, RK_KEY_LEFT, RK_MOD_CTRL | RK_MOD_SHIFT);
  CHECK(CLOSE_TO(rk_table_column_width(t, 0), w0 - 3 * m, 0.5), "Ctrl+Shift+Left narrows it four times that (%.0f)", rk_table_column_width(t, 0));
  w->focus_visible = true;
  rk_app_run_for(100);
  shot(w, "table-head");
  key(w, RK_KEY_DOWN, 0);
  CHECK(L->head == -1 && L->active == 0, "Down: back to the rows");
  done(w);
}

/* Editing a cell: a double click or F2 opens it; Enter or Tab keeps the value, Escape puts the old one back; the app
   may refuse, and the cell says why and stays open. */
static char comp_name[8][32] = { "Tile", "Rows", "Tag", "Sheet", "Toast", "Menu", "Field", "Table" };
static char comp_group[8][32] = { "surfaces", "lists", "status", "surfaces", "motion", "surfaces", "inputs", "lists" };
static int commits;
static const char *comp_cell(int row, int col, void *u) { (void)u; return col == 0 ? comp_name[row] : col == 1 ? comp_group[row] : "4 kB"; }
static const char *comp_commit(rk_view *t, int row, int col, const char *value, void *u) {
  (void)t; (void)u;
  if (col == 1 && !*value) return "A component needs a group.";
  snprintf(col == 0 ? comp_name[row] : comp_group[row], 32, "%s", value);
  commits++;
  return NULL;
}

static void test_cell_edit(void) {
  rk_window *w = window(560, 360);
  rk_view *page = col(8, 16);
  rk_view *t = rk_add(page, rk_table(8, comp_cell, NULL));
  rk_table_add_column(t, "component", 0, RK_COLUMN_EDITABLE);
  rk_table_add_column(t, "group", 180, RK_COLUMN_EDITABLE);
  rk_table_add_column(t, "size", 90, RK_COLUMN_NUMERIC);
  rk_table_on_edit(t, comp_commit, NULL);
  rk_view_set_size(t, 0, 260);
  rk_view *save = rk_add(page, rk_button("save", NULL, NULL));
  show(w, page, "cells");
  list *L = t->state;
  rk_rect r = rk__window_rect(t);
  float H = rk_metric_of(RK_ROW_HEIGHT), hh = head_height(L), gx = r.x + L->cols[1].x + 20, ry = r.y + hh + 2.5f * H;
  click(w, (rk_point){ gx, ry }, 10); click(w, (rk_point){ gx, ry }, 10);
  CHECK(L->editor && L->edit_row == 2 && L->edit_col == 1 && rk__view_within(rk_window_focused(w), L->editor), "a double click opens the cell");
  CHECK(!strcmp(rk_field_text(L->editor), "status"), "with its value in it (%s)", L->editor ? rk_field_text(L->editor) : "-");
  type_text(w, "states");
  key(w, RK_KEY_ENTER, 0);
  CHECK(!L->editor && !strcmp(comp_group[2], "states") && rk_window_focused(w) == t, "typing replaces it, Enter keeps it (%s)", comp_group[2]);
  key(w, RK_KEY_F1 + 1, 0);
  CHECK(L->editor && L->edit_col == 0 && L->edit_row == 2, "F2 opens the row's first editable cell");
  type_text(w, "Label");
  key(w, RK_KEY_ESCAPE, 0);
  CHECK(!L->editor && !strcmp(comp_name[2], "Tag") && rk_window_focused(w) == t, "Escape puts the old value back (%s)", comp_name[2]);
  key(w, RK_KEY_F1 + 1, 0);
  int before = commits;
  key(w, RK_KEY_TAB, 0);
  CHECK(L->editor && L->edit_col == 1 && commits == before, "Tab goes on to the next editable cell; nothing changed, nothing committed");
  key(w, RK_KEY_BACKSPACE, 0); key(w, RK_KEY_ENTER, 0);
  CHECK(L->editor && !strcmp(comp_group[2], "states"), "a refusal: the cell stays open");
  rk_app_run_for(100);
  shot(w, "table-edit");
  type_text(w, "inputs"); key(w, RK_KEY_ENTER, 0);
  CHECK(!L->editor && !strcmp(comp_group[2], "inputs"), "then a value it takes (%s)", comp_group[2]);
  key(w, RK_KEY_TAB, RK_MOD_SHIFT);
  CHECK(rk_window_focused(w) == save, "outside the editor, Tab moves focus as ever");
  rk_window_focus(w, t);
  key(w, RK_KEY_F1 + 1, 0); type_text(w, "Badge");
  click(w, (rk_point){ r.x + 60, r.y + hh + 5.5f * H }, 10);
  CHECK(!L->editor && !strcmp(comp_name[2], "Badge") && rk_window_focused(w) == t, "a click elsewhere keeps the value (%s)", comp_name[2]);
  done(w);
}

/* A text area: the same field with more lines - wrapped at its width, Enter breaks a line, Ctrl+Enter submits. */
static void test_textarea(void) {
  rk_window *w = window(420, 360);
  rk_view *page = col(16, 24);
  rk_view *area = rk_add(page, rk_field("Notes", "What happened?"));
  rk_view *line = rk_add(page, rk_field("One line", NULL));
  rk_field_set_lines(area, 4);
  rk_field_on_submit(area, on_field_submit, NULL);
  rk_field_set_text(line, "set before it is laid out");
  show(w, page, "text area");
  rk_app_run_for(50);
  CHECK(((field *)line->state)->scroll == 0, "text set before the layout starts at the start (%.0f)", ((field *)line->state)->scroll);
  field *f = area->state;
  CHECK(rk__window_rect(area).h > rk__window_rect(line).h + 3 * 20, "four lines tall (%.0f)", rk__window_rect(area).h);
  rk_field_set_text(area, "The build on Thursday failed twice before anyone noticed, and the second time it took the docs with it.");
  rk_app_run_for(50);
  bool fits = f->nlines > 1;
  for (int k = 0; k < f->nlines; k++) fits = fits && width(f, f->starts[k], line_stop(f, k)) <= f->wrap_w;
  CHECK(fits, "the text wraps at the width, every line within it (%d lines)", f->nlines);
  bool at_spaces = true;
  for (int k = 1; k < f->nlines; k++) at_spaces = at_spaces && f->text[f->starts[k] - 1] == ' ';
  CHECK(at_spaces, "and wraps at spaces");

  rk_window_focus(w, area);
  rk_field_set_text(area, "first");
  int submits = field_submits;
  key(w, RK_KEY_ENTER, 0); type_text(w, "second line");
  CHECK(!strcmp(rk_field_text(area), "first\nsecond line") && field_submits == submits, "Enter breaks a line (%s)", rk_field_text(area));
  key(w, RK_KEY_UP, 0);
  CHECK(f->caret == 5, "Up goes to the line above, as near the column as it has (%d)", f->caret);
  key(w, RK_KEY_DOWN, 0);
  CHECK(f->caret == 17, "Down comes back to the column it started from (%d)", f->caret);
  key(w, RK_KEY_HOME, 0);
  CHECK(f->caret == 6, "Home keeps to the line (%d)", f->caret);
  key(w, RK_KEY_HOME, RK_MOD_CTRL);
  CHECK(f->caret == 0, "Ctrl+Home goes to the start (%d)", f->caret);
  key(w, RK_KEY_ENTER, RK_MOD_CTRL);
  CHECK(field_submits == submits + 1, "Ctrl+Enter submits");
  key(w, RK_KEY_END, RK_MOD_CTRL);
  rk_field_set_text(line, NULL);
  char *paste = rk__strdup("\r\nthird\r\nfourth\rfifth");
  rk__setstr(&clipboard, NULL);
  if (!rk__platform()->clip_set) { clipboard = paste; paste = NULL; } else rk__platform()->clip_set(paste);
  free(paste);
  key(w, 'V', RK_MOD_CTRL);
  CHECK(!strcmp(rk_field_text(area), "first\nsecond line\nthird\nfourth\nfifth"), "pasted breaks are kept, as \\n (%s)", rk_field_text(area));
  CHECK(f->scroll_y > 0, "five lines in four: it scrolls to keep the caret in sight (%.0f)", f->scroll_y);
  rk_window_focus(w, line);
  key(w, 'V', RK_MOD_CTRL);
  CHECK(!strcmp(rk_field_text(line), " third fourth fifth"), "in one line they are spaces (%s)", rk_field_text(line));

  /* the pointer: a click on a line puts the caret there; the wheel scrolls */
  rk_window_focus(w, area);
  rk_rect r = rk__window_rect(area);
  float lh = line_h(f), top = r.y + input_rect(area, f).y + input_pad_y(f) - f->scroll_y;
  click(w, (rk_point){ r.x + 12 + width(f, 6, 9), top + 1.5f * lh }, 10);
  CHECK(f->caret == 6 + 3, "a click on the second line (%d)", f->caret);
  rk_event over = { sizeof over, RK_EVENT_POINTER_MOVE, 0, { r.x + 40, top + lh }, 0, 0, 0, 0, 0, NULL };
  rk_window_inject(w, &over);
  CHECK(w->cursor == RK__CURSOR_TEXT, "over the text, the I-beam");
  over.position.y = r.y + 4; rk_window_inject(w, &over);
  CHECK(w->cursor == RK__CURSOR_ARROW, "over the label, the arrow");
  rk_event wh = { sizeof wh, RK_EVENT_WHEEL, rk_app_now(), { r.x + 40, r.y + r.h - 20 }, 0, 0, 0, 0, 0, NULL };
  wh.wheel_y = -200;
  rk_window_inject(w, &wh);
  CHECK(f->scroll_y == 0, "the wheel scrolls it back to the top (%.0f)", f->scroll_y);
  /* a selection over a break */
  rk_field_set_text(area, "Thursday's build failed twice.\nThe docs went with it.\nNobody noticed until Friday, when the release notes had no pages left to point to.");
  key(w, RK_KEY_HOME, RK_MOD_CTRL); key(w, RK_KEY_DOWN, 0);
  key(w, RK_KEY_END, RK_MOD_SHIFT); key(w, RK_KEY_RIGHT, RK_MOD_SHIFT); key(w, RK_KEY_RIGHT, RK_MOD_SHIFT | RK_MOD_CTRL);
  rk_app_run_for(200);
  shot(w, "textarea");
  CHECK(!strncmp(rk_field_text(area) + sel_lo(f), "The docs went with it.\nNobody", (size_t)(sel_hi(f) - sel_lo(f))), "Shift extends over the break");
  done(w);
}

/* Focus everywhere: whatever takes a press or a key takes focus, and Tab reaches it - a toast's action too. */
static int acts;
static void act(rk_view *v, void *u) { (void)v; (void)u; acts++; }
static void toast_acted(void *u) { (void)u; acts += 10; }
static void menu_noop(rk_view *m, int i, void *u) { (void)m; (void)i; (void)u; }

static bool takes_input(const rk_view *v) {
  if (v->press != RK_PRESS_NONE || v->on_activate || v->menu) return true;
  const char *n = v->cls ? v->cls->pub.name : "";
  return !strcmp(n, "rk_field") || !strcmp(n, "rk_list") || !strcmp(n, "rk_strip");
}
static int unreachable(const rk_view *v, const rk_view *skip) {
  if (v == skip || v->hidden) return 0;
  int n = takes_input(v) && !v->focusable;
  if (n) fprintf(stderr, "  not focusable: %s\n", v->cls ? v->cls->pub.name : "(plain view)");
  for (int i = 0; i < v->nkids; i++) n += unreachable(v->kids[i], skip);
  return n;
}

static void test_focus(void) {
  rk_window *w = window(900, 600);
  rk_controller *home = rk_panorama();
  rk_view *s1 = col(16, 0);
  rk_view *save = rk_add(s1, rk_button("save", act, NULL));
  rk_view *more = rk_add(s1, rk_button("more", NULL, NULL));
  rk_view *m = rk_menu(); rk_menu_add(m, "export", NULL, menu_noop, NULL); rk_view_set_menu(more, m);
  rk_view *t = rk_add(rk_add(s1, rk_tile_grid()), rk_tile("3", "builds"));
  rk_view_on_activate(t, act, NULL);
  rk_view *rows = rk_add(s1, rk_rows());
  rk_view_on_activate(rk_rows_add(rows, "Release notes", "today", RK_STATUS_NONE), act, NULL);
  rk_add(s1, rk_field("Name", NULL));
  rk_view *l = rk_add(s1, rk_list(50, list_item, NULL)); rk_view_set_size(l, 300, 200);
  rk_view *tb = rk_add(s1, rk_table(50, table_cell, NULL)); rk_table_add_column(tb, "build", 0, 0); rk_view_set_size(tb, 300, 200);
  for (int i = 0; i < 30; i++) rk_add(s1, rk_label("A line that makes the section taller than the window.", RK_TEXT_BODY));
  rk_panorama_add(home, "focus", s1, 0);
  rk_panorama_add(home, "second", col(16, 0), 0);
  rk_window_show(w, rk_places(home));
  rk_app_run_for(100);
  CHECK(unreachable(w->root, w->titlebar) == 0, "everything that takes a press or a key takes focus");
  /* Tab goes round all of them and comes back */
  rk_view *seen[64]; int n = 0;
  rk_window_focus(w, NULL);
  for (int i = 0; i < 64; i++) {
    key(w, RK_KEY_TAB, 0);
    if (n && rk_window_focused(w) == seen[0]) break;
    seen[n++] = rk_window_focused(w);
  }
  int want = 0;
  rk_view *expect[] = { save, more, t, l, tb };
  for (int i = 0; i < 5; i++) for (int j = 0; j < n; j++) if (seen[j] == expect[i]) { want++; break; }
  CHECK(want == 5 && n >= 8, "Tab reaches each of them and comes round (%d stops)", n);
  /* with nothing focused, Up and Down scroll the section in sight, as a page does */
  rk_window_focus(w, NULL);
  rk_view *box = s1->parent;
  float y0 = box->value;
  key(w, RK_KEY_DOWN, 0); key(w, RK_KEY_DOWN, 0);
  rk_app_run_for(400);
  CHECK(box->value > y0 + 60, "Down scrolls the section (%.0f)", box->value);
  key(w, RK_KEY_UP, 0); key(w, RK_KEY_UP, 0); key(w, RK_KEY_UP, 0);
  rk_app_run_for(400);
  CHECK(box->value == 0, "Up brings it back (%.0f)", box->value);
  /* a toast's action: last in the order; focus holds it; Escape dismisses it and focus goes back */
  rk_window_focus(w, save);
  rk_window_toast(w, "Archived.", "undo", toast_acted, NULL);
  rk_app_run_for(300);
  rk_view *toast = w->toasts->kids[0];
  key(w, RK_KEY_TAB, RK_MOD_SHIFT);
  CHECK(rk__view_within(rk_window_focused(w), toast), "Shift+Tab from the first goes round to the toast's action");
  rk_app_run_for(rk_duration_of(RK_TIMING_TOAST) + 1500);
  CHECK(toast->parent && !toast->leaving, "focus holds the toast past its time");
  key(w, RK_KEY_ESCAPE, 0);
  CHECK(toast->leaving && rk_window_focused(w) == save && acts == 0, "Escape dismisses it, focus goes back");
  rk_app_run_for(400);
  /* Enter on the action runs it; focus goes back too */
  rk_window_toast(w, "Archived again.", "undo", toast_acted, NULL);
  rk_app_run_for(300);
  key(w, RK_KEY_TAB, RK_MOD_SHIFT);
  key(w, RK_KEY_ENTER, 0);
  rk_app_run_for(300);
  CHECK(acts == 10 && rk_window_focused(w) == save, "Enter on the action runs it (%d), focus goes back", acts);
  /* the pointer on it holds it too; off it, it goes */
  rk_window_focus(w, NULL);
  rk_window_toast(w, "Hovered.", NULL, NULL, NULL);
  rk_app_run_for(300);
  toast = w->toasts->kids[w->toasts->nkids - 1];
  rk_rect tr = rk__window_rect(toast);
  rk_event mv = { sizeof mv, RK_EVENT_POINTER_MOVE, rk_app_now(), { tr.x + 20, tr.y + tr.h / 2 }, 0, 0, 0, 0, 0, NULL };
  rk_window_inject(w, &mv);
  rk_app_run_for(rk_duration_of(RK_TIMING_TOAST) + 1500);
  CHECK(!toast->leaving, "the pointer holds it");
  mv.position = (rk_point){ 20, 500 }; mv.time = rk_app_now();
  rk_window_inject(w, &mv);
  rk_app_run_for(1500);
  CHECK(w->toasts->nkids == 0, "off it, it goes");
  done(w);
}

/* The app's history: an action runs and is remembered; its toast offers undo; Ctrl+Z outside fields. */
static int counter;
static void count_up(void *u) { (void)u; counter++; }
static void count_down(void *u) { (void)u; counter--; }

static void test_undo(void) {
  rk_window *w = window(480, 300);
  show(w, col(8, 16), "undo");
  rk_window_do(w, "Archived one.", count_up, count_down, NULL, NULL);
  rk_window_do(w, "Archived another.", count_up, count_down, NULL, NULL);
  rk_app_run_for(300);
  CHECK(counter == 2 && w->toasts->nkids >= 1, "actions run, a toast offers undo");
  key(w, 'Z', RK_MOD_CTRL);
  CHECK(counter == 1, "Ctrl+Z undoes the last (%d)", counter);
  key(w, 'Z', RK_MOD_CTRL); key(w, 'Z', RK_MOD_CTRL);
  CHECK(counter == 0 && !rk_window_undo(w), "back to the start, and no further (%d)", counter);
  key(w, 'Z', RK_MOD_CTRL | RK_MOD_SHIFT);
  CHECK(counter == 1, "Ctrl+Shift+Z redoes (%d)", counter);
  rk_window_do(w, "Something new.", count_up, count_down, NULL, NULL);
  CHECK(counter == 2 && !rk_window_redo(w), "a new action: what was undone cannot come back");
  rk_app_run_for(6000);
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
  test_fonts();
  test_list();
  test_field();
  test_menu();
  test_table();
  test_cell_edit();
  test_textarea();
  test_undo();
  test_focus();
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

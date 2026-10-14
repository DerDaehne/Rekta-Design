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

static void done(rk_window *w) { rk_window_close(w); }

/* a column with the gap and padding a test measures against */
static rk_view *col(float gap, float pad) {
  rk_view *v = rk_vstack();
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
  int n;
  rk_line *lines = rk__text_lines("A tile's whole area is one fill, and the fill is its status.", &f, 160, &n);
  CHECK(n >= 3, "wraps: %d lines", n);
  for (int i = 0; i < n; i++) CHECK(lines[i].width <= 160.5f, "line %d fits: %f", i, lines[i].width);
  free(lines);
  char many[4000] = "";
  for (int i = 0; i < 300; i++) strcat(many, "line\n");
  CHECK(rk__text_wrap_size(many, &f, 0, NULL) == 300, "300 lines, every one of them");
  rk_size m = rk_text_measure("one line", RK_TEXT_SMALL, 0);
  CHECK(CLOSE_TO(m.h, 13 * 1.35, 1e-3), "one line's height: %f", m.h);
}

/* Layout ---------------------------------------------------------------------------------------------------------- */

static void test_layout(void) {
  rk_window *w = window(800, 600);
  rk_view *root = col(8, 16);
  rk_view *row = rk_add(root, rk_hstack());
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
  rk_event e = { sizeof e, RK_EVENT_POINTER_DOWN, 0, p, 0, 0, 0, 0, 0, NULL, NULL, 0 };
  rk_window_inject(w, &e);
  rk_app_run_for(hold);
  e.type = RK_EVENT_POINTER_UP;
  rk_window_inject(w, &e);
}

static void key(rk_window *w, uint32_t k, uint32_t mods) {
  rk_event e = { sizeof e, RK_EVENT_KEY_DOWN, 0, { 0, 0 }, 0, 0, 0, k, mods, NULL, NULL, 0 };
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
  rk_event e = { sizeof e, RK_EVENT_POINTER_DOWN, 0, { r.x + 10, r.y + 10 }, 0, 0, 0, 0, 0, NULL, NULL, 0 };
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
  rk_view *p = col(16, 0);                                     /* the place sets it in */
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
static int logged;                                             /* misuse the library reported (rk_app_set_log) */
static char logged_last[256];
static void test_log(rk_log_level level, const char *message, void *user) { (void)level; (void)user; logged++; snprintf(logged_last, sizeof logged_last, "%s", message); if (getenv("RK_TEST_LOG")) fprintf(stderr, "log: %s\n", message); }
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
  float one = rk__text_width("\xe6\x97\xa5", -1, &f);
  rk_line *lines = rk__text_lines(jp, &f, one * 5.5f, &n);
  bool fit = n > 2;
  for (int i = 0; i < n; i++) fit = fit && lines[i].width <= one * 5.5f + 0.5f && lines[i].len % 3 == 0;
  CHECK(fit, "Japanese without spaces wraps at the width, a character at a time (%d lines)", n);
  CHECK(!memcmp(jp + lines[n - 1].start + lines[n - 1].len - 3, "\xe3\x80\x82", 3) && lines[n - 1].len > 3, "and 。 never starts a line");
  free(lines);

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

/* Disabled means it: no press, no Enter, no value; Tab passes it by unless it has a tooltip, its reason, which then
   shows on focus; a disabled column disables what it holds. A tooltip comes after tip-delay under the pointer. */
static void test_enabled(void) {
  rk_window *w = window(600, 400);
  rk_view *page = col(16, 16);
  rk_view *b = rk_add(page, rk_button("send", count_press, NULL));
  rk_view *plain = rk_add(page, rk_button("plain", count_press, NULL));
  rk_view *group = rk_add(page, rk_vstack());
  rk_view *chk = rk_add(group, rk_check("done", false));
  show(w, page, "enabled");
  int before = pressed_count;
  rk_view_set_enabled(b, false);
  CHECK(!rk_view_is_enabled(b) && rk_view_is_enabled(plain), "rk_view_is_enabled");
  rk_view_tap(b);
  rk_window_focus(w, b);
  rk_window_key(w, RK_KEY_ENTER);
  rk_app_run_for(200);
  CHECK(pressed_count == before, "a disabled button fires neither on a click nor on Enter (%d)", pressed_count - before);
  CHECK(rk_window_focused(w) != b, "nor takes focus without a reason");
  rk_view_set_tooltip(b, "add a recipient first");
  CHECK(rk_window_focus(w, b), "with a reason it takes focus");
  rk_window_key(w, RK_KEY_ENTER);
  rk_app_run_for(200);
  CHECK(pressed_count == before, "and still does not fire");
  w->focus_visible = true;
  rk_window_focus(w, NULL);
  rk_window_key(w, RK_KEY_TAB);
  CHECK(rk_window_focused(w) == b && w->tip && w->tip_for == b, "Tab reaches it and its reason shows at once");
  rk_window_key(w, RK_KEY_ESCAPE);
  CHECK(!w->tip, "Escape takes the tooltip away");
  rk_view_set_enabled(group, false);
  CHECK(!rk_view_is_enabled(chk), "a disabled column disables what it holds");
  rk_view_tap(chk);
  CHECK(rk_control_value(chk) == 0, "its checkbox does not toggle");
  rk_view_set_enabled(group, true);
  rk_view_tap(chk);
  CHECK(rk_control_value(chk) == 1, "enabled again, it does");
  /* under the pointer: after tip-delay, not before; leaving hides it */
  rk_view_set_tooltip(plain, "a plain button");
  rk_rect r = rk__window_rect(plain);
  rk_event e = { sizeof e, RK_EVENT_POINTER_MOVE, rk_app_now(), { r.x + 4, r.y + 4 }, 0, 0, 0, 0, 0, NULL, NULL, 0 };
  rk__tip_hide(w, false); w->tip_hid = -1e9;
  rk_window_inject(w, &e);
  rk_app_run_for(rk_duration_of(RK_TIMING_TIP_DELAY) / 2);
  CHECK(!w->tip, "no tooltip before tip-delay");
  rk_app_run_for(rk_duration_of(RK_TIMING_TIP_DELAY));
  CHECK(w->tip && w->tip_for == plain, "the tooltip after tip-delay");
  shot(w, "tooltip");
  e.position = (rk_point){ 590, 390 };
  rk_window_inject(w, &e);
  CHECK(!w->tip, "leaving hides it");
  done(w);
}

/* No fixed limits: ten thousand tiles each get their own cell, six hundred children each a frame; a long list with no
   height of its own fills what its column leaves and scrolls, as a flex: 1 list does on the web. */
static void list_item(int i, rk_list_item *out, void *u);
static void test_limits(void) {
  rk_window *w = window(800, 600);
  rk_view *page = col(8, 0);
  rk_view *grid = rk_add(page, rk_tile_grid());
  for (int i = 0; i < 10000; i++) rk_add(grid, rk_tile("1", "x"));
  rk_view_set_span(rk_view_child(grid, 9999), 2, 2);
  rk_view *stack = rk_add(page, rk_vstack());
  for (int i = 0; i < 600; i++) rk_add(stack, rk_label("line", RK_TEXT_BODY));
  show(w, rk_scroll(page), "limits");
  rk__layout_pending();
  bool distinct = true;
  rk_rect a = rk_view_frame(rk_view_child(grid, 0)), z = rk_view_frame(rk_view_child(grid, 9998)), big = rk_view_frame(rk_view_child(grid, 9999));
  for (int i = 1; i < 10000 && distinct; i++) {
    rk_rect f = rk_view_frame(rk_view_child(grid, i)), e = rk_view_frame(rk_view_child(grid, i - 1));
    if (f.w <= 0 || (f.x == e.x && f.y == e.y)) distinct = false;
  }
  CHECK(distinct && z.y > a.y && big.w > z.w && big.y >= z.y, "10000 tiles, each in a cell of its own, the last one 2 x 2");
  rk_rect last = rk_view_frame(rk_view_child(stack, 599)), before = rk_view_frame(rk_view_child(stack, 598));
  CHECK(last.h > 0 && last.y > before.y, "the 600th child of a column has its frame (%.0f)", last.y);
  done(w);
  w = window(600, 400);
  page = col(8, 16);
  rk_add(page, rk_label("above", RK_TEXT_BODY));
  rk_view *list = rk_add(page, rk_list(100000, list_item, NULL));
  rk_add(page, rk_button("below", NULL, NULL));
  show(w, page, "fill");
  rk_rect lf = rk_view_frame(list), pf = rk_view_frame(page);
  CHECK(lf.h > 100 && lf.y + lf.h < pf.h, "a list with no height fills what its column leaves (%.0f of %.0f)", lf.h, pf.h);
  rk_window_focus(w, list);
  key(w, RK_KEY_END, 0);
  rk_app_run_for(1200);
  CHECK(list->pvalue > 1000, "and scrolls (%.0f)", list->pvalue);
  done(w);
}

/* Damage is honest: after each change, what the frames drew piece by piece is what drawing everything shows. And a
   label's new text draws about the label, not the window. */
static int damage_lies(rk_window *w) {
  rk_app_run_for(1500);                                     /* every motion ends; the frames drew only their damage */
  rk_bitmap *b = w->fb;
  size_t n = (size_t)b->h * b->stride;
  uint32_t *was = rk__alloc(n * sizeof *was);
  memcpy(was, b->px, n * sizeof *was);
  w->needs_draw = true; w->damage_all = true;
  rk__window_frame(w, rk_app_now());
  int off = 0;
  for (int y = 0; y < b->h; y++) for (int x = 0; x < b->w; x++) {
    uint32_t p = b->px[(size_t)y * b->stride + x], q = was[(size_t)y * b->stride + x];
    int d = 0;
    for (int k = 0; k < 32; k += 8) d = RK_MAX(d, abs((int)((p >> k) & 255) - (int)((q >> k) & 255)));
    if (d > 8) off++;
  }
  free(was);
  return off;
}

static void test_damage(void) {
  rk_window *w = window(800, 500);
  rk_view *page = col(12, 16);
  rk_view *title = rk_add(page, rk_label("Builds", RK_TEXT_TITLE));
  rk_view *row = rk_add(page, rk_hstack());
  rk_view *a = rk_add(row, rk_button("one", NULL, NULL));
  rk_view *bt = rk_add(row, rk_button("two", NULL, NULL));
  rk_view *rows = rk_add(page, rk_rows());
  rk_rows_add(rows, "first", "1", RK_STATUS_NONE);
  rk_view *seg = rk_add(page, rk_segments((const char *const[]){ "all", "failed", "waiting" }, 3, 0));
  rk_view *note = rk_add(page, rk_label("a note", RK_TEXT_BODY));
  show(w, page, "damage");
  CHECK(damage_lies(w) == 0, "at rest: nothing stale");
  rk_stats st;
  memset(&st, 0, sizeof st); st.size = sizeof st;
  rk_app_stats_reset();
  rk_label_set_text(note, "a longer note now");
  rk_app_run_for(100);
  rk_app_stats(&st);
  CHECK(st.pixels < 800.0 * 500 / 10, "a label's new text draws about the label (%.0f of %d pixels)", st.pixels, 800 * 500);
  CHECK(damage_lies(w) == 0, "a label's new text: nothing stale (%d)", damage_lies(w));
  rk_label_set_text(title, "Builds and releases");
  CHECK(damage_lies(w) == 0, "a title that grows: nothing stale");
  rk_view_set_hidden(a, true);
  CHECK(damage_lies(w) == 0, "a button hidden, the other moves over: nothing stale");
  rk_view_set_hidden(a, false);
  CHECK(damage_lies(w) == 0, "and shown again");
  rk_rows_add(rows, "second", "2", RK_STATUS_INFO);
  CHECK(damage_lies(w) == 0, "a row added, what is below moves down: nothing stale");
  rk_view_remove(rk_view_child(rows, 0));
  CHECK(damage_lies(w) == 0, "a row removed: nothing stale");
  rk_control_set_value(seg, 2);
  CHECK(damage_lies(w) == 0, "segments: nothing stale");
  w->focus_visible = true;
  rk_window_focus(w, bt);
  CHECK(damage_lies(w) == 0, "a focus ring: nothing stale");
  rk_window_key(w, RK_KEY_TAB);
  CHECK(damage_lies(w) == 0, "focus moves on: nothing stale");
  rk_view_set_enabled(bt, false);
  CHECK(damage_lies(w) == 0, "disabled: nothing stale");
  done(w);
}

static void test_api(void) {
  int before = rk__live_views;
  rk_window *w = window(600, 400);
  rk_view *page = rk_vstack();
  rk_view *grid = rk_add(page, rk_tile_grid());
  rk_view *t = rk_add(grid, rk_tile("12", "builds"));
  CHECK(rk_view_parent(t) == grid && rk_view_count(grid) == 1, "rk_add returns the child, in place");
  rk_view *b = rk_add(rk_add(page, rk_hstack()), rk_button("pin a tile", count_press, NULL));
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
  rk_view *moved = rk_insert(page, t, 0);                      /* a view in the tree moves: nothing to retain */
  CHECK(moved == t && rk_view_parent(t) == page && rk_view_count(grid) == 0, "rk_insert moves a view already in the tree");
  rk_view *label = rk_add(grid, rk_label("one", RK_TEXT_BODY));
  rk_add(page, label);                                         /* moved, then changed: still alive */
  rk_label_set_text(label, "two");
  CHECK(rk_view_parent(label) == page, "a label added again moves and stays alive");
  int was_logged = logged;
  CHECK(!rk_insert(label, page, 0) && logged == was_logged + 1, "a view cannot hold its own parent, and it is reported");
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
  rk_event e = { sizeof e, RK_EVENT_POINTER_DOWN, 0, p, 0, 0, 0, 0, mods, NULL, NULL, 0 };
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
  CHECK(shown == 2 && rk_list_selection_count(list) == 1 && rk_list_is_selected(list, 2), "a click selects one and shows it (%d)", shown);
  tap_row(w, list, 5, RK_MOD_CTRL);
  CHECK(rk_list_selection_count(list) == 2 && shown == 2, "Ctrl+click toggles, starts selecting several, shows nothing");
  tap_row(w, list, 7, 0);
  CHECK(rk_list_selection_count(list) == 3, "while selecting, a click toggles");
  key(w, RK_KEY_ESCAPE, 0);
  CHECK(rk_list_selection_count(list) == 0, "Escape ends it");
  tap_row(w, list, 3, 0);
  tap_row(w, list, 6, RK_MOD_SHIFT);
  CHECK(rk_list_selection_count(list) == 4 && rk_list_is_selected(list, 4), "Shift+click takes a range (%d)", rk_list_selection_count(list));
  CHECK(selection_changes >= 6, "every change is told (%d)", selection_changes);
  rk_window_focus(w, list);
  w->focus_visible = true;
  key(w, RK_KEY_DOWN, 0); key(w, RK_KEY_DOWN, 0);
  rk_app_run_for(400);
  shot(w, "list");
  key(w, RK_KEY_END, 0);
  rk_app_run_for(1200);
  CHECK(CLOSE_TO(list->pvalue, 100000 * H - 300, 0.5), "End glides to the last row (%g)", list->pvalue);
  rk_event wheel = { sizeof wheel, RK_EVENT_WHEEL, 0, { 200, 200 }, 0, 0, -480, 0, 0, NULL, NULL, 0 };
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
  rk_event e = { sizeof e, RK_EVENT_TEXT, 0, { 0, 0 }, 0, 0, 0, 0, 0, t, NULL, 0 };
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
  rk_event pe = { sizeof pe, RK_EVENT_COMPOSE, 0, { 0, 0 }, 0, 0, 0, 0, 0, "ka", NULL, 0 };
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
  rk_event d = { sizeof d, RK_EVENT_POINTER_DOWN, rk_app_now(), { r.x + 12 + rk_text_measure("<badge na", RK_TEXT_BODY, 0).w, y }, 0, 0, 0, 0, 0, NULL, NULL, 0 };
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
  rk_field_set_error(mail, "Add the part after @, such as example.com.");
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
  rk_view *row = rk_add(page, rk_hstack());
  rk_view *more = rk_add(row, rk_button("more", NULL, NULL));
  rk_view *m = rk_menu();
  int open_i = rk_menu_add(m, "open", "Enter", on_menu, NULL);
  int rename_i = rk_menu_add(m, "rename", "F2", on_menu, NULL);
  rk_menu_add_separator(m);
  rk_menu_add_label(m, "show");
  int done_i = rk_menu_add(m, "done", NULL, on_menu, NULL);
  int failed_i = rk_menu_add(m, "failed", NULL, on_menu, NULL);
  int delete_i = rk_menu_add(m, "delete (needs rights)", "Del", on_menu, NULL);
  rk_menu_set_check(m, done_i, RK_CHECKMARK_ON);
  rk_menu_set_check(m, failed_i, RK_CHECKMARK_OFF);
  rk_menu_set_enabled(m, delete_i, false);
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
  CHECK(rk_menu_check(m, failed_i) == RK_CHECKMARK_ON && w->floats->nkids == 1 && menu_chosen == failed_i, "a check toggles and the menu stays");
  rk_app_run_for(100);
  shot(w, "menu");
  key(w, RK_KEY_ESCAPE, 0);
  CHECK(w->floats->nkids == 0 && rk_window_focused(w) == more, "Escape closes, focus back");
  rk_view_tap(more);
  click(w, (rk_point){ 500, 380 }, 10);
  CHECK(w->floats->nkids == 0, "a press outside closes it");
  /* a context menu: a right click at the pointer, Shift+F10 below the view */
  rk_rect t = rk__window_rect(tile);
  rk_event rc = { sizeof rc, RK_EVENT_POINTER_DOWN, 0, { t.x + 40, t.y + 30 }, 1, 0, 0, 0, 0, NULL, NULL, 0 };
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
static void table_sort(rk_view *t, int col, rk_sort sort, void *u) { (void)t; (void)u; sorted_col = col; sorted_dir = sort == RK_SORT_ASCENDING ? 1 : -1; }

/* The selection follows the items, not the rows they stand on: with keys a sort or new items above keep the same
   builds selected and the keyboard on the same one; without keys a sort lets the selection go rather than mark the
   wrong ones. The app selects too, and reads the selection back. */
static int order[8], norder;                                   /* the app's rows: build ids in the order shown */
static uint64_t build_key(int index, void *u) { (void)u; return (uint64_t)order[index]; }
static const char *build_cell(int row, int col, void *u) { static char buf[16]; (void)u; (void)col; snprintf(buf, sizeof buf, "#%d", order[row]); return buf; }
static void build_sort(rk_view *t, int col, rk_sort sort, void *u) {
  int dir = sort == RK_SORT_ASCENDING ? 1 : -1;
  (void)t; (void)col; (void)u;
  for (int i = 0; i < norder; i++) for (int j = i + 1; j < norder; j++) if ((order[j] < order[i]) == (dir > 0)) { int x = order[i]; order[i] = order[j]; order[j] = x; }
}

static void test_list_keys(void) {
  rk_window *w = window(400, 360);
  rk_view *page = col(8, 16);
  rk_view *t = rk_add(page, rk_table(0, build_cell, NULL));
  rk_table_add_column(t, "build", 0, RK_COLUMN_SORTABLE);
  rk_table_on_sort(t, build_sort, NULL);
  norder = 5;
  for (int i = 0; i < norder; i++) order[i] = 10 + i;           /* #10 .. #14 */
  rk_list_set_count(t, norder);
  rk_list_set_key(t, build_key);
  show(w, page, "keys");
  rk_list_select(t, 1, true); rk_list_select(t, 3, true);        /* #11 and #13 */
  int sel[8], n = rk_list_selection(t, sel, 8);
  CHECK(n == 2 && sel[0] == 1 && sel[1] == 3 && rk_list_selection_count(t) == 2, "the app selects, and reads it back (%d)", n);
  rk_list_scroll_to(t, 3);                                       /* the keyboard on #13 */
  rk_rect r = rk__window_rect(t);
  click(w, (rk_point){ r.x + 60, r.y + 10 }, 10);                /* ascending: as it was */
  click(w, (rk_point){ r.x + 60, r.y + 10 }, 10);                /* descending: #14 #13 #12 #11 #10 */
  n = rk_list_selection(t, sel, 8);
  CHECK(order[0] == 14 && n == 2 && sel[0] == 1 && sel[1] == 3 && order[sel[0]] == 13 && order[sel[1]] == 11, "sorted the other way, #11 and #13 stay selected where they went");
  CHECK(order[rk_list_active(t)] == 13, "and the keyboard stays on #13 (row %d)", rk_list_active(t));
  memmove(order + 1, order, (size_t)norder * sizeof *order); order[0] = 99; norder++;   /* a new build above */
  rk_list_set_count(t, norder);
  n = rk_list_selection(t, sel, 8);
  CHECK(n == 2 && order[sel[0]] == 13 && order[sel[1]] == 11 && order[rk_list_active(t)] == 13, "a new item above: the same builds, one row down");
  memmove(order, order + 1, (size_t)(norder - 1) * sizeof *order); norder--;          /* #99 again gone, and #13 too */
  for (int i = 0; i < norder; i++) if (order[i] == 13) { memmove(order + i, order + i + 1, (size_t)(norder - i - 1) * sizeof *order); norder--; break; }
  rk_list_set_count(t, norder);
  CHECK(rk_list_selection_count(t) == 1 && order[rk_list_selection(t, sel, 8) ? sel[0] : 0] == 11, "an item gone leaves the selection");
  rk_list_clear_selection(t);
  CHECK(rk_list_selection_count(t) == 0, "rk_list_clear_selection");
  rk_list_set_key(t, NULL);                                      /* without keys */
  rk_list_select(t, 0, true);
  click(w, (rk_point){ r.x + 60, r.y + 10 }, 10);
  CHECK(rk_list_selection_count(t) == 0, "without keys a sort lets the selection go instead of marking other items");
  done(w);
}

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
  CHECK(shown == 2 && rk_list_is_selected(t, 2), "a click on a row selects it and shows it (%d)", shown);
  rk_list_scroll_to(t, 500);
  rk_app_run_for(900);
  CHECK(t->pvalue > 400 * H && rk_window_view_at(w, (rk_point){ r.x + 60, r.y + 10 }) == t, "scrolled far; the head stays on top");
  rk_list_scroll_to(t, 0);
  rk_app_run_for(900);
  shot(w, "table");
  /* a column's edge drags its width; twice on it gives the column its own back; the pointer shows the edge */
  list *L = t->state;
  float edge = r.x + L->cols[1].x + L->cols[1].w, y = r.y + 10;
  rk_event mv = { sizeof mv, RK_EVENT_POINTER_MOVE, 0, { edge + 2, y }, 0, 0, 0, 0, 0, NULL, NULL, 0 };
  rk_window_inject(w, &mv);
  CHECK(w->cursor == RK__CURSOR_COL_RESIZE, "over a column's edge the pointer shows it (%d)", w->cursor);
  rk_event dn = { sizeof dn, RK_EVENT_POINTER_DOWN, 0, { edge, y }, 0, 0, 0, 0, 0, NULL, NULL, 0 };
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

/* Controls that hold a value: checkbox, options, toggle, segments. */
static int control_changes;
static void control_changed(rk_view *v, void *u) { (void)v; (void)u; control_changes++; }

static void test_controls(void) {
  rk_window *w = window(520, 520);
  rk_view *page = col(8, 16);
  rk_view *chk = rk_add(page, rk_check("Remember this device", false));
  static const char *const often[] = { "daily", "weekly", "never" }, *const span[] = { "day", "week", "month" };
  rk_view *opt = rk_add(page, rk_choices(often, 3, 1));
  rk_view *tog = rk_add(page, rk_toggle("Live tiles", false));
  rk_view *seg = rk_add(page, rk_segments(span, 3, 0));
  rk_view *sld = rk_add(page, rk_slider(0, 100, 5, 40));
  rk_view *stp = rk_add(page, rk_stepper(1, 12, 1, 3));
  rk_view *ctl[] = { chk, opt, tog, seg, sld, stp };
  for (int i = 0; i < 6; i++) rk_control_on_change(ctl[i], control_changed, NULL);
  show(w, page, "controls");
  rk_view_tap(chk);
  CHECK(rk_control_value(chk) == 1 && control_changes == 1, "a click checks it");
  rk_window_focus(w, chk); key(w, RK_KEY_SPACE, 0);
  CHECK(rk_control_value(chk) == 0 && control_changes == 2, "Space unchecks it");
  rk_rect r = rk__window_rect(opt);
  float H = rk_metric_of(RK_CONTROL_HEIGHT);
  click(w, (rk_point){ r.x + 30, r.y + 2.5f * H }, 10);
  CHECK(rk_control_value(opt) == 2, "a click chooses an option (%g)", rk_control_value(opt));
  rk_window_focus(w, opt); key(w, RK_KEY_DOWN, 0);
  CHECK(rk_control_value(opt) == 0, "Down goes round to the first (%g)", rk_control_value(opt));
  key(w, RK_KEY_UP, 0);
  CHECK(rk_control_value(opt) == 2 && control_changes == 5, "Up back to the last");
  rk_view_tap(tog);
  rk_app_run_for(rk_duration_of(RK_DURATION_SHORT) / 2);
  CHECK(rk_control_value(tog) == 1 && tog->pvalue > 0.1f && tog->pvalue < 0.9f, "the toggle turns on, its thumb on the way (%.2f)", tog->pvalue);
  rk_app_run_for(400);
  CHECK(tog->pvalue == 1, "and there");
  int before = control_changes;
  rk_control_set_value(tog, 0);
  rk_app_run_for(400);
  CHECK(rk_control_value(tog) == 0 && control_changes == before, "the app sets it without a change heard");
  rk_view_tap(tog);
  r = rk__window_rect(seg);
  segments *sg = seg->state;
  click(w, (rk_point){ r.x + sg->x[2] + 10, r.y + r.h / 2 }, 10);
  rk_app_run_for(rk_duration_of(RK_DURATION_SHORT) / 2);
  CHECK(rk_control_value(seg) == 2 && seg->pvalue > 0.1f && seg->pvalue < 0.9f, "a segment is chosen, the accent sliding (%.2f)", seg->pvalue);
  rk_window_focus(w, seg);
  key(w, RK_KEY_LEFT, 0); CHECK(rk_control_value(seg) == 1, "Left: the one before");
  key(w, RK_KEY_HOME, 0); key(w, RK_KEY_END, 0); CHECK(rk_control_value(seg) == 2, "Home and End");
  rk_control_set_value(seg, 7);
  CHECK(rk_control_value(seg) == 2, "a value it cannot take is ignored");
  /* the slider: a press puts it there, a drag follows, it snaps to its step; keys */
  slider *sl = sld->state;
  r = rk__window_rect(sld);
  float x0, x1;
  slider_track(sld, sl, &x0, &x1);
  rk_event p = { sizeof p, RK_EVENT_POINTER_DOWN, 0, { r.x + x0 + (x1 - x0) * 0.71f, r.y + r.h / 2 }, 0, 0, 0, 0, 0, NULL, NULL, 0 };
  rk_window_inject(w, &p);
  CHECK(rk_control_value(sld) == 70, "a press on the track moves it there, on its step (%g)", rk_control_value(sld));
  p.type = RK_EVENT_POINTER_MOVE; p.position.x = r.x + x1 + 200; rk_window_inject(w, &p);
  p.type = RK_EVENT_POINTER_UP; rk_window_inject(w, &p);
  CHECK(rk_control_value(sld) == 100, "a drag past the end stops at the end (%g)", rk_control_value(sld));
  rk_window_focus(w, sld);
  key(w, RK_KEY_LEFT, 0); CHECK(rk_control_value(sld) == 95, "Left: a step down (%g)", rk_control_value(sld));
  key(w, RK_KEY_PAGE_DOWN, 0); CHECK(rk_control_value(sld) == 85, "Page Down: a tenth (%g)", rk_control_value(sld));
  key(w, RK_KEY_HOME, 0); CHECK(rk_control_value(sld) == 0, "Home: the start");
  rk_control_set_value(sld, 42.6);
  CHECK(rk_control_value(sld) == 45, "the app's value snaps too (%g)", rk_control_value(sld));
  /* the stepper: a press steps, a held one repeats, typing and Enter take a number, the ends hold */
  r = rk__window_rect(stp);
  float bw = rk_metric_of(RK_CONTROL_HEIGHT);
  click(w, (rk_point){ r.x + r.w - bw / 2, r.y + r.h / 2 }, 10);
  CHECK(rk_control_value(stp) == 4, "plus steps up (%g)", rk_control_value(stp));
  click(w, (rk_point){ r.x + r.w - bw / 2, r.y + r.h / 2 }, 400 + 80 * 3 + 40);
  CHECK(rk_control_value(stp) >= 8 && rk_control_value(stp) <= 9, "held, it repeats (%g)", rk_control_value(stp));
  rk_window_focus(w, stp);
  type_text(w, "1"); type_text(w, "1");
  CHECK(rk_control_value(stp) <= 9, "typing waits for Enter");
  key(w, RK_KEY_ENTER, 0);
  CHECK(rk_control_value(stp) == 11, "Enter takes it (%g)", rk_control_value(stp));
  type_text(w, "40"); key(w, RK_KEY_ENTER, 0);
  CHECK(rk_control_value(stp) == 12, "past the end: the end (%g)", rk_control_value(stp));
  type_text(w, "5"); key(w, RK_KEY_ESCAPE, 0);
  CHECK(rk_control_value(stp) == 12, "Escape puts the number back");
  key(w, RK_KEY_DOWN, 0); key(w, RK_KEY_HOME, 0); key(w, RK_KEY_DOWN, 0);
  CHECK(rk_control_value(stp) == 1, "Down, Home, and no lower than the start (%g)", rk_control_value(stp));
  rk_view_set_enabled(chk, false);
  rk_view_tap(chk);
  CHECK(rk_control_value(chk) == 0 && rk_window_focused(w) != chk, "disabled: no change, no focus");
  rk_control_set_value(chk, 1);
  rk_control_set_value(stp, 3);
  rk_window_focus(w, seg);
  key(w, RK_KEY_LEFT, 0);
  w->focus_visible = true;
  rk_app_run_for(500);
  shot(w, "inputs");
  done(w);
}

/* Pictures: PNG and JPEG, decoded away from the UI thread, fitted, faded in; what cannot be read says so. */
static const unsigned char test_jpeg[] = { 255,216,255,224,0,16,74,70,73,70,0,1,1,0,0,1,0,1,0,0,255,219,0,67,0,2,1,1,1,1,1,2,1,1,1,2,2,2,2,2,4,3,2,2,2,2,5,4,4,3,4,6,5,6,6,6,5,6,6,6,7,9,8,6,7,9,7,6,6,8,11,8,9,10,10,10,10,10,6,8,11,12,11,10,12,9,10,10,10,255,219,0,67,1,2,2,2,2,2,2,5,3,3,5,10,7,6,7,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,255,192,0,17,8,0,8,0,16,3,1,17,0,2,17,1,3,17,1,255,196,0,31,0,0,1,5,1,1,1,1,1,1,0,0,0,0,0,0,0,0,1,2,3,4,5,6,7,8,9,10,11,255,196,0,181,16,0,2,1,3,3,2,4,3,5,5,4,4,0,0,1,125,1,2,3,0,4,17,5,18,33,49,65,6,19,81,97,7,34,113,20,50,129,145,161,8,35,66,177,193,21,82,209,240,36,51,98,114,130,9,10,22,23,24,25,26,37,38,39,40,41,42,52,53,54,55,56,57,58,67,68,69,70,71,72,73,74,83,84,85,86,87,88,89,90,99,100,101,102,103,104,105,106,115,116,117,118,119,120,121,122,131,132,133,134,135,136,137,138,146,147,148,149,150,151,152,153,154,162,163,164,165,166,167,168,169,170,178,179,180,181,182,183,184,185,186,194,195,196,197,198,199,200,201,202,210,211,212,213,214,215,216,217,218,225,226,227,228,229,230,231,232,233,234,241,242,243,244,245,246,247,248,249,250,255,196,0,31,1,0,3,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,1,2,3,4,5,6,7,8,9,10,11,255,196,0,181,17,0,2,1,2,4,4,3,4,7,5,4,4,0,1,2,119,0,1,2,3,17,4,5,33,49,6,18,65,81,7,97,113,19,34,50,129,8,20,66,145,161,177,193,9,35,51,82,240,21,98,114,209,10,22,36,52,225,37,241,23,24,25,26,38,39,40,41,42,53,54,55,56,57,58,67,68,69,70,71,72,73,74,83,84,85,86,87,88,89,90,99,100,101,102,103,104,105,106,115,116,117,118,119,120,121,122,130,131,132,133,134,135,136,137,138,146,147,148,149,150,151,152,153,154,162,163,164,165,166,167,168,169,170,178,179,180,181,182,183,184,185,186,194,195,196,197,198,199,200,201,202,210,211,212,213,214,215,216,217,218,226,227,228,229,230,231,232,233,234,242,243,244,245,246,247,248,249,250,255,218,0,12,3,1,0,2,17,3,17,0,63,0,249,110,191,159,207,245,240,249,206,191,223,195,252,71,63,255,217 };   /* 16 x 8: red left, blue right */

static uint32_t fb_at(rk_window *w, float x, float y) { return w->fb->px[(int)(y * w->scale) * w->fb->stride + (int)(x * w->scale)]; }
static bool near_rgb(uint32_t p, uint32_t rgb, int tol) {   /* upscaling smooths a little, as browsers do */
  for (int sh = 0; sh <= 16; sh += 8) if (abs((int)((p >> sh) & 255) - (int)((rgb >> sh) & 255)) > tol) return false;
  return true;
}

static void test_image(void) {
  /* a 4 x 2 PNG: red, green, blue, white over black, a hole, half red, yellow */
  uint32_t px[8] = { 0xffff0000u, 0xff00ff00u, 0xff0000ffu, 0xffffffffu, 0xff000000u, 0x00000000u, 0x80800000u, 0xffffff00u };
  rk_bitmap bm = { 4, 2, 4, px, 8, NULL };
  char path[600];
  snprintf(path, sizeof path, "%s/../four-by-two.png", out);   /* beside the shots, not among them */
  CHECK(rk__png_write(&bm, path), "a PNG to read (%s)", path);
  rk_window *w = window(420, 360);
  rk_view *page = col(8, 16);
  rk_view *png = rk_add(page, rk_image(path));
  rk_view *cover = rk_add(page, rk_image(path));
  rk_view *jpg = rk_add(page, rk_image_from_memory(test_jpeg, sizeof test_jpeg));
  rk_view *bad = rk_add(page, rk_image_from_memory("not a picture", 13));
  rk_view *none = rk_add(page, rk_image("/no/such/picture.png"));
  rk_view_set_size(png, 80, 40);
  rk_view_set_size(cover, 40, 40); rk_image_set_fit(cover, RK_FIT_COVER);
  rk_view_set_size(jpg, 64, 32);
  int iw, ih;
  CHECK(rk_image_size(png, &iw, &ih) && iw == 4 && ih == 2, "its size is known at once (%d x %d)", iw, ih);
  CHECK(!rk_image_size(bad, &iw, &ih) && !rk_image_size(none, NULL, NULL), "bytes that are no picture, and a missing file, say so");
  show(w, page, "image");
  rk_app_run_for(600);
  CHECK(((image *)png->state)->ready && ((image *)jpg->state)->ready && png->pvalue == 1, "decoded and faded in");
  rk_rect r = rk__window_rect(png);
  uint32_t red = fb_at(w, r.x + 10, r.y + 10), half = fb_at(w, r.x + 50, r.y + 30), hole = fb_at(w, r.x + 30, r.y + 30), yellow = fb_at(w, r.x + 70, r.y + 30);
  CHECK(near_rgb(red, 0xff0000, 16) && near_rgb(yellow, 0xffff00, 16), "each pixel where it belongs, scaled up (%06x %06x)", red & 0xffffff, yellow & 0xffffff);
  CHECK(near_rgb(hole, rk_color_of(RK_ROLE_BG) >> 8, 16), "a hole shows what is under it (%06x)", hole & 0xffffff);
  CHECK(((half >> 16) & 255) > 0xf0 && ((half >> 8) & 255) > 0x70 && ((half >> 8) & 255) < 0x90, "half red over white is pink (%06x)", half & 0xffffff);
  rk_rect cr = rk__window_rect(cover);
  CHECK((fb_at(w, cr.x + 1, cr.y + 1) & 0xffffff) != (rk_color_of(RK_ROLE_BG) >> 8) && (fb_at(w, cr.x + 1, cr.y + 38) & 0xffffff) != (rk_color_of(RK_ROLE_BG) >> 8), "cover fills its box, corner to corner");
  rk_rect jr = rk__window_rect(jpg);
  uint32_t left = fb_at(w, jr.x + 10, jr.y + 16), right = fb_at(w, jr.x + 54, jr.y + 16);
  CHECK(((left >> 16) & 255) > 180 && (left & 255) < 80 && (right & 255) > 180 && ((right >> 16) & 255) < 80, "a JPEG, red and blue (%06x %06x)", left & 0xffffff, right & 0xffffff);
  /* shrinking averages: a 4 x 2 picture in 2 x 1 */
  uint32_t small[2];
  resample(px, 4, 2, small, 2, 1);
  CHECK(((small[0] >> 24) & 255) > 0xb0 && ((small[0] >> 24) & 255) < 0xd0, "shrunk, a hole and its neighbours average out (alpha %02x)", (small[0] >> 24) & 255);
  shot(w, "image");
  /* a big picture is scaled by the worker; a new size gets a new copy, the old one standing in meanwhile */
  rk_bitmap big = { 1200, 1000, 1200, rk__alloc(1200 * 1000 * sizeof(uint32_t)), 1200 * 1000, NULL };
  for (int i = 0; i < 1200 * 1000; i++) big.px[i] = 0xff000000u | (uint32_t)((i % 1200) * 255 / 1199) << 16;
  char bigpath[600];
  snprintf(bigpath, sizeof bigpath, "%s/../big.png", out);
  CHECK(rk__png_write(&big, bigpath), "a big PNG to read");
  free(big.px);
  rk_view *large = rk_add(page, rk_image(bigpath));
  rk_view_set_size(large, 120, 100);
  rk_app_run_for(300);
  image *li = large->state;
  CHECK(li->shown && li->fitted.w == (int)(120 * w->scale) && !li->fitting, "a big picture: its copy came from the worker (%d)", li->fitted.w);
  rk_view_set_size(large, 60, 50);
  rk_app_run_for(300);
  CHECK(li->fitted.w == (int)(60 * w->scale) && li->fitted.h == (int)(50 * w->scale), "a new size, a new copy (%d x %d)", li->fitted.w, li->fitted.h);
  done(w);
}

/* A scroll view: the wheel, the keys focus leaves, focus coming into sight, the bar. */
static int scroll_taps;
static void scroll_tap(rk_view *v, void *u) { (void)v; scroll_taps = (int)(intptr_t)u; }

static void test_scroll(void) {
  rk_window *w = window(360, 300);
  rk_view *page = col(8, 16);
  rk_view *list = rk_vstack();
  rk_view *b[40];
  for (int i = 0; i < 40; i++) { char t[16]; snprintf(t, sizeof t, "item %d", i); b[i] = rk_add(list, rk_button(t, scroll_tap, (void *)(intptr_t)i)); }
  rk_view *sc = rk_add(page, rk_scroll(list));
  rk_view_set_size(sc, 0, 200);
  show(w, page, "scroll");
  scroller *s = sc->state;
  CHECK(s->max > 1000 && sc->value == 0, "taller than it is: it scrolls (%.0f)", s->max);
  rk_rect r = rk__window_rect(sc);
  rk_event wh = { sizeof wh, RK_EVENT_WHEEL, 0, { r.x + 40, r.y + 100 }, 0, 0, 0, 0, 0, NULL, NULL, 0 };
  wh.wheel_y = 300; rk_window_inject(w, &wh);
  rk_app_run_for(500);
  CHECK(sc->pvalue == 300 && s->bar, "the wheel glides it, the bar shows (%.0f)", sc->pvalue);
  shot(w, "scroll");
  rk_app_run_for(1000);
  CHECK(!s->bar && s->rail->passthrough, "a moment later the bar goes");
  /* what shows is what a press reaches */
  rk_view *hit = rk_window_view_at(w, (rk_point){ r.x + 30, r.y + 10 });
  int expect = -1;
  for (int i = 0; i < 40; i++) if (hit == b[i]) expect = i;
  CHECK(expect > 0, "what a press reaches is what shows after scrolling (item %d)", expect);
  rk_view_tap(b[expect]);
  CHECK(scroll_taps == expect, "and it is pressed (%d)", scroll_taps);
  wh.wheel_y = 99999; rk_window_inject(w, &wh);
  rk_app_run_for(500);
  CHECK(sc->value == s->max, "no further than the end");
  /* focus: keys it leaves, and coming into sight */
  rk_window_focus(w, b[0]);
  rk_view_reveal(b[0]);
  rk_app_run_for(500);
  CHECK(sc->value == 0, "a view revealed comes into sight (%.0f)", sc->value);
  key(w, RK_KEY_END, 0); rk_app_run_for(500);
  CHECK(sc->value == s->max, "End, which a button leaves, goes to the end");
  key(w, RK_KEY_HOME, 0); key(w, RK_KEY_PAGE_DOWN, 0); rk_app_run_for(500);
  CHECK(sc->value == 200 - 40, "Home, then Page Down: a page (%.0f)", sc->value);
  rk_window_focus(w, b[0]); key(w, RK_KEY_HOME, 0);
  for (int i = 0; i < 25; i++) key(w, RK_KEY_TAB, 0);
  rk_app_run_for(500);
  rk_rect br = rk__window_rect(b[25]);
  CHECK(rk_window_focused(w) == b[25] && br.y >= r.y && br.y + br.h <= r.y + r.h, "Tab brings what it focuses into sight (%.0f in %.0f..%.0f)", br.y, r.y, r.y + r.h);
  /* the bar drags */
  wh.wheel_y = -99999; rk_window_inject(w, &wh); rk_app_run_for(500);
  wh.wheel_y = 10; rk_window_inject(w, &wh); rk_app_run_for(300);
  float ty, th;
  thumb_of(s, r.h, &ty, &th);
  rk_event d = { sizeof d, RK_EVENT_POINTER_DOWN, 0, { r.x + r.w - 2, r.y + ty + th / 2 }, 0, 0, 0, 0, 0, NULL, NULL, 0 };
  rk_window_inject(w, &d);
  d.type = RK_EVENT_POINTER_MOVE; d.position.y += (r.h - th) / 2; rk_window_inject(w, &d);
  d.type = RK_EVENT_POINTER_UP; rk_window_inject(w, &d);
  CHECK(fabsf(sc->value - (10 + s->max / 2)) < 2, "dragging the bar halfway down scrolls half the way (%.0f of %.0f)", sc->value, s->max);
  done(w);
}

/* A select opens a menu of its options; a field with options suggests them as you type. */
static void test_select(void) {
  rk_window *w = window(460, 420);
  rk_view *page = col(8, 16);
  static const char *const fruit[] = { "apple", "banana", "cherry", "damson" };
  static const char *const city[] = { "Berlin", "Bern", "Bonn", "Hamburg", "Munich" };
  rk_view *sel = rk_add(page, rk_select(fruit, 4, -1));
  rk_view *town = rk_add(page, rk_field("City", "Where you live"));
  rk_field_set_options(town, city, 5);
  rk_control_on_change(sel, control_changed, NULL);
  show(w, page, "select");
  CHECK(rk_control_value(sel) == -1, "nothing chosen yet");
  rk_view_tap(sel);
  rk_app_run_for(200);
  CHECK(w->floats->nkids == 1 && rk_window_focused(w) == w->floats->kids[0], "a press opens its menu");
  key(w, RK_KEY_DOWN, 0); key(w, RK_KEY_DOWN, 0); key(w, RK_KEY_ENTER, 0);
  rk_app_run_for(100);
  CHECK(rk_control_value(sel) == 1 && w->floats->nkids == 0 && rk_window_focused(w) == sel, "Down, Down, Enter: banana, and focus is back (%g)", rk_control_value(sel));
  key(w, RK_KEY_DOWN, 0);
  CHECK(rk_control_value(sel) == 2 && w->floats->nkids == 0, "Down on the box changes it in place");
  key(w, 'D', 0);
  CHECK(rk_control_value(sel) == 3, "a letter jumps to its option");
  key(w, RK_KEY_ENTER, 0);
  rk_app_run_for(100);
  CHECK(w->floats->nkids == 1 && rk__menu_active(w->floats->kids[0]) == 3, "Enter opens it at the chosen option");
  key(w, RK_KEY_ESCAPE, 0);
  CHECK(w->floats->nkids == 0 && rk_control_value(sel) == 3, "Escape closes it, the choice stays");

  rk_window_focus(w, town);
  type_text(w, "b");
  rk_app_run_for(100);
  rk_view *list = w->floats->nkids ? w->floats->kids[0] : NULL;
  CHECK(list && rk_window_focused(w) == town && ((menu *)list->state)->n == 4, "typing shows the options that contain it, focus stays in the field (%d)", list ? ((menu *)list->state)->n : 0);
  type_text(w, "e"); type_text(w, "r");
  CHECK(list && ((menu *)list->state)->n == 2, "fewer as it narrows (%d)", list ? ((menu *)list->state)->n : 0);
  w->focus_visible = true;
  key(w, RK_KEY_DOWN, 0);
  rk_app_run_for(250);
  shot(w, "select");
  key(w, RK_KEY_ENTER, 0);
  CHECK(!strcmp(rk_field_text(town), "Berlin") && w->floats->nkids == 0, "Down and Enter take one (%s)", rk_field_text(town));
  key(w, 'Z', RK_MOD_CTRL);
  CHECK(!strcmp(rk_field_text(town), "ber"), "an edit like any other: Ctrl+Z takes it back (%s)", rk_field_text(town));
  type_text(w, "x");
  CHECK(w->floats->nkids == 0 && !strcmp(rk_field_text(town), "berx"), "nothing matches: no list, the text stays");
  key(w, RK_KEY_BACKSPACE, 0); key(w, RK_KEY_BACKSPACE, 0);
  CHECK(w->floats->nkids == 0, "deleting does not open it");
  key(w, RK_KEY_DOWN, 0);
  CHECK(w->floats->nkids == 1, "Down opens it");
  key(w, RK_KEY_ESCAPE, 0);
  CHECK(w->floats->nkids == 0 && rk_window_focused(w) == town, "Escape closes it, focus stays");
  rk_field_set_text(town, "");
  type_text(w, "m");
  rk_app_run_for(250);
  list = w->floats->nkids ? w->floats->kids[0] : NULL;
  rk_rect lr = list ? rk__window_rect(list) : (rk_rect){ 0, 0, 0, 0 };
  float y = lr.y + rk_metric_of(RK_SPACE_XS) + rk_metric_of(RK_ROW_HEIGHT) * 1.5f;   /* Hamburg, Munich: the second */
  click(w, (rk_point){ lr.x + 40, y }, 10);
  CHECK(!strcmp(rk_field_text(town), "Munich") && rk_window_focused(w) == town, "a click takes one (%s)", rk_field_text(town));
  done(w);
}

/* A date: year, month and day from the keys, or a day from the month below it. */
static void test_date(void) {
  rk_window *w = window(420, 480);
  rk_view *page = col(8, 16);
  rk_view *due = rk_add(page, rk_date(2024, 2, 28));
  rk_view *none = rk_add(page, rk_date(0, 0, 0));
  rk_control_on_change(due, control_changed, NULL);
  show(w, page, "date");
  CHECK(rk_control_value(due) == 20240228 && rk_control_value(none) == 0, "its value is the number; none is 0");
  rk_window_focus(w, due);
  key(w, RK_KEY_RIGHT, 0); key(w, RK_KEY_UP, 0);
  CHECK(rk_control_value(due) == 20240328, "Right to the month, Up: March (%g)", rk_control_value(due));
  key(w, RK_KEY_RIGHT, 0); key(w, RK_KEY_UP, 0);
  CHECK(rk_control_value(due) == 20240329, "the day goes up (%g)", rk_control_value(due));
  rk_control_set_value(due, 20240229);
  key(w, RK_KEY_LEFT, 0); key(w, RK_KEY_LEFT, 0); key(w, RK_KEY_UP, 0);
  CHECK(rk_control_value(due) == 20250228, "a year on from the 29th of February: the 28th (%g)", rk_control_value(due));
  rk_control_set_value(due, 20240230);
  CHECK(rk_control_value(due) == 20250228, "a day that does not exist is refused");
  rk_window_focus(w, none);
  type_text(w, "2023"); type_text(w, "2"); type_text(w, "14");
  CHECK(rk_control_value(none) == 20230214, "typed: 2023, 2, 14 (%g)", rk_control_value(none));
  key(w, RK_KEY_LEFT, 0); type_text(w, "13");
  CHECK(rk_control_value(none) == 20230214, "month 13 is not taken");
  /* the month below it */
  key(w, RK_KEY_ENTER, 0);
  rk_app_run_for(200);
  rk_view *sheet = w->floats->nkids ? w->floats->kids[0] : NULL;
  CHECK(sheet && rk_window_focused(w) == sheet, "Enter opens the month, the keys in it");
  w->focus_visible = true;
  rk_app_run_for(100);
  shot(w, "date");
  key(w, RK_KEY_RIGHT, 0); key(w, RK_KEY_DOWN, 0);
  CHECK(sheet && ((date_sheet *)sheet->state)->cur == 20230222, "Right a day, Down a week (%d)", sheet ? ((date_sheet *)sheet->state)->cur : 0);
  key(w, RK_KEY_PAGE_DOWN, 0); key(w, RK_KEY_PAGE_DOWN, RK_MOD_SHIFT);
  CHECK(sheet && ((date_sheet *)sheet->state)->cur == 20240322, "Page Down a month, with Shift a year (%d)", sheet ? ((date_sheet *)sheet->state)->cur : 0);
  key(w, RK_KEY_PAGE_UP, 0); key(w, RK_KEY_END, 0);
  CHECK(sheet && ((date_sheet *)sheet->state)->cur == 20240229, "End: the month's last day (%d)", sheet ? ((date_sheet *)sheet->state)->cur : 0);
  key(w, RK_KEY_ENTER, 0);
  CHECK(rk_control_value(none) == 20240229 && !w->floats->nkids && rk_window_focused(w) == none, "Enter takes it; focus is back (%g)", rk_control_value(none));
  rk_rect r = rk__window_rect(none);
  click(w, (rk_point){ r.x + r.w - 20, r.y + r.h / 2 }, 10);
  rk_app_run_for(200);
  CHECK(w->floats->nkids == 1, "the calendar opens it too");
  key(w, RK_KEY_ESCAPE, 0);
  CHECK(!w->floats->nkids && rk_control_value(none) == 20240229 && rk_window_focused(w) == none, "Escape closes it as it was");
  key(w, RK_KEY_ENTER, 0);
  rk_app_run_for(200);
  sheet = w->floats->kids[0];
  rk_rect sr = rk__window_rect(sheet), cell = day_cell(20240229, 10);
  click(w, (rk_point){ sr.x + cell.x + cell.w / 2, sr.y + cell.y + cell.h / 2 }, 10);
  CHECK(rk_control_value(none) == 20240210, "a click takes a day (%g)", rk_control_value(none));
  CHECK(weekday(2026, 10, 13) == 1 && weekday(2000, 1, 1) == 5 && add_days(20231231, 1) == 20240101 && add_months(20240131, 1) == 20240229, "the calendar's arithmetic");
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
  rk_event over = { sizeof over, RK_EVENT_POINTER_MOVE, 0, { r.x + 40, top + lh }, 0, 0, 0, 0, 0, NULL, NULL, 0 };
  rk_window_inject(w, &over);
  CHECK(w->cursor == RK__CURSOR_TEXT, "over the text, the I-beam");
  over.position.y = r.y + 4; rk_window_inject(w, &over);
  CHECK(w->cursor == RK__CURSOR_ARROW, "over the label, the arrow");
  rk_event wh = { sizeof wh, RK_EVENT_WHEEL, rk_app_now(), { r.x + 40, r.y + r.h - 20 }, 0, 0, 0, 0, 0, NULL, NULL, 0 };
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
  return !strcmp(n, "rk_field") || !strcmp(n, "rk_list") || !strcmp(n, "rk_strip") || !strcmp(n, "rk_check") || !strcmp(n, "rk_choices") ||
         !strcmp(n, "rk_toggle") || !strcmp(n, "rk_segments") || !strcmp(n, "rk_slider") || !strcmp(n, "rk_stepper") || !strcmp(n, "rk_select") || !strcmp(n, "rk_date");
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
  rk_event mv = { sizeof mv, RK_EVENT_POINTER_MOVE, rk_app_now(), { tr.x + 20, tr.y + tr.h / 2 }, 0, 0, 0, 0, 0, NULL, NULL, 0 };
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
  rk_action act = { .size = sizeof act, .label = "Archived one.", .run = count_up, .undo = count_down };
  rk_window_do(w, &act);
  act.label = "Archived another.";
  rk_window_do(w, &act);
  rk_app_run_for(300);
  CHECK(counter == 2 && w->toasts->nkids >= 1, "actions run, a toast offers undo");
  key(w, 'Z', RK_MOD_CTRL);
  CHECK(counter == 1, "Ctrl+Z undoes the last (%d)", counter);
  key(w, 'Z', RK_MOD_CTRL); key(w, 'Z', RK_MOD_CTRL);
  CHECK(counter == 0 && !rk_window_undo(w), "back to the start, and no further (%d)", counter);
  key(w, 'Z', RK_MOD_CTRL | RK_MOD_SHIFT);
  CHECK(counter == 1, "Ctrl+Shift+Z redoes (%d)", counter);
  act.label = "Something new.";
  rk_window_do(w, &act);
  CHECK(counter == 2 && !rk_window_redo(w), "a new action: what was undone cannot come back");
  rk_app_run_for(6000);
  done(w);
}

/* A tree: items in items; the keys and the pointer as the web's .rk-tree; a branch opens over its height. */
static int tree_shown = -1, tree_toggles;
static void tree_show(rk_view *t, int item, void *u) { (void)t; (void)u; tree_shown = item; }
static void tree_toggled(rk_view *t, int item, bool open, void *u) { (void)t; (void)item; (void)open; (void)u; tree_toggles++; }
/* Collections change: a row says something new, a tree loses a branch or everything, keeps the app's data for an
   item, a menu is built again. */
static void test_collections(void) {
  rk_window *w = window(500, 400);
  rk_view *page = col(8, 16);
  rk_view *rows = rk_add(page, rk_rows());
  rk_view *r0 = rk_rows_add(rows, "Nightly import", NULL, RK_STATUS_WARNING);
  rk_rows_add(rows, "Docs", "3 kB", RK_STATUS_NONE);
  rk_view *tr = rk_add(page, rk_tree());
  int a = rk_tree_add(tr, -1, "a"), a1 = rk_tree_add(tr, a, "a1"), b = rk_tree_add(tr, -1, "b");
  static int payload = 42;
  rk_tree_set_user(tr, a1, &payload);
  show(w, page, "collections");
  rk_row_set(r0, "Nightly import", "done", RK_STATUS_SUCCESS);
  CHECK(!strcmp(((row *)r0->state)->title, "Nightly import") && ((row *)r0->state)->status == RK_STATUS_SUCCESS, "rk_row_set changes a row in place");
  rk_insert(rows, r0, 1);
  CHECK(rk_view_child(rows, 1) == r0, "a row moves like any view");
  CHECK(rk_tree_user(tr, a1) == &payload && rk_tree_user(tr, b) == NULL, "a tree item keeps the app's data");
  rk_tree_set_title(tr, b, "bee");
  CHECK(!strcmp(rk_tree_title(tr, b), "bee"), "and a new title");
  rk_tree_select(tr, a1);
  rk_tree_remove(tr, a);
  CHECK(rk_tree_title(tr, a) == NULL && rk_tree_title(tr, a1) == NULL && rk_tree_selected(tr) == -1 && !strcmp(rk_tree_title(tr, b), "bee"), "a branch removed: it and what was under it, and its selection");
  int c = rk_tree_add(tr, -1, "c");
  CHECK(c > b, "removed numbers are not given again (%d)", c);
  rk_tree_clear(tr);
  CHECK(rk_tree_title(tr, b) == NULL && rk_tree_add(tr, -1, "fresh") == 0, "cleared: numbers start again");
  rk_view *m = rk_menu();
  rk_menu_add(m, "one", NULL, NULL, NULL);
  rk_menu_clear(m);
  CHECK(rk_menu_add(m, "again", NULL, NULL, NULL) == 0, "a menu cleared is built again from 0");
  rk_release(m);
  CHECK(damage_lies(w) == 0, "and drawn honestly");
  done(w);
}

/* A pivot inside a page is held by the page's controller: its facets hear appear, know their window, present a
   sheet; nested, it picks a facet (it slides) rather than the place. Pivot facets are views, as panorama sections;
   a facet controller's title changes in the strip. */
static int appeared_count;
static void facet_appear(rk_controller *c, void *st) { (void)c; (void)st; appeared_count++; }

static void test_embed(void) {
  rk_window *w = window(800, 500);
  rk_view *page = rk_vstack();
  rk_controller *place = rk_controller_create_with_view(page, "notes");
  rk_controller *pivot = rk_pivot();
  rk_pivot_add(pivot, "summary", rk_label("the summary", RK_TEXT_BODY));
  rk_controller_class cls = { sizeof cls, "files", NULL, facet_appear, NULL, NULL, NULL };
  rk_controller *files = rk_controller_create(&cls, NULL);
  rk_controller_set_title(files, "files");
  rk_pivot_add_controller(pivot, files);
  rk_add(page, rk_label("Notes", RK_TEXT_TITLE));
  rk_view *pv = rk_add(page, rk_controller_embed(place, pivot));
  rk_view_set_grow(pv, 1);
  rk_window_show(w, rk_places(place));
  rk_app_run_for(400);
  CHECK(rk_controller_parent(pivot) == place && rk_controller_window(pivot) == w, "embedded: the pivot has its owner and the window");
  CHECK(!((struct strip *)((struct pivot *)pivot->impl)->strip->state)->nav, "nested, the pivot picks a facet, not the place");
  appeared_count = 0;
  rk_pivot_select(pivot, 1);
  rk_app_run_for(800);
  CHECK(appeared_count == 1 && rk_controller_window(files) == w, "the facet controller appears and knows its window");
  rk_controller_present(files, rk_controller_create_with_view(rk_label("a question", RK_TEXT_BODY), "ask"));
  rk_app_run_for(600);
  CHECK(w->sheets->nkids > 0, "and its sheet is shown");
  rk_controller_dismiss(files->sheet);
  rk_app_run_for(600);
  rk_controller_set_title(files, "attachments");
  CHECK(!strcmp(((struct strip *)((struct pivot *)pivot->impl)->strip->state)->titles[1], "attachments"), "a facet's new title shows in the strip");
  int was = logged;
  CHECK(!rk_controller_embed(place, pivot) && logged == was + 1, "a controller with a parent is not embedded twice, and it is said");
  done(w);
}

/* A short question answers once: ok, cancel, or Escape as cancel; the safe answer has focus. A secret field shows
   dots, keeps the text for the app, and gives nothing to the clipboard or a screen reader. */
static int answers, answer_ok;
static void answered(bool ok, void *u) { (void)u; answers++; answer_ok = ok; }

static void test_confirm_secret(void) {
  rk_window *w = window(700, 460);
  rk_view *page = col(12, 16);
  rk_view *pw = rk_add(page, rk_field("password", NULL));
  rk_field_set_secret(pw, true);
  show(w, page, "confirm");
  rk_controller *home = rk_window_root(w);
  rk_window_focus(w, pw);
  type_text(w, "hunter2");
  CHECK(!strcmp(rk_field_text(pw), "hunter2") && !strcmp(pw->access_value, ""), "a secret field keeps the text for the app, none for a screen reader");
  key(w, 'A', RK_MOD_CTRL); key(w, 'C', RK_MOD_CTRL);
  char *clip = rk__platform()->clip_get ? rk__platform()->clip_get() : NULL;
  CHECK(!clip || !strstr(clip, "hunter2"), "nor for the clipboard");
  free(clip);
  rk_confirm_desc d = { .size = sizeof d, .title = "Delete 3 builds?", .text = "This cannot be undone.", .ok = "delete", .cancel_is_safe = true };
  answers = 0;
  rk_confirm(home, &d, answered, NULL);
  rk_app_run_for(500);
  rk_view *focused = rk_window_focused(w);
  CHECK(focused && focused->cls == &button_class && !strcmp(((button *)focused->state)->label, "cancel"), "the safe answer has focus");
  shot(w, "confirm");
  rk_window_key(w, RK_KEY_ENTER);
  rk_app_run_for(600);
  CHECK(answers == 1 && !answer_ok && !w->sheets->nkids, "Enter on cancel: answered no, once, and gone");
  rk_confirm(home, &d, answered, NULL);
  rk_app_run_for(500);
  rk_view_tap(rk_view_find(w->sheets, "delete"));
  rk_app_run_for(600);
  CHECK(answers == 2 && answer_ok, "delete: answered yes");
  rk_confirm(home, &d, answered, NULL);
  rk_app_run_for(500);
  rk_window_key(w, RK_KEY_ESCAPE);
  rk_app_run_for(600);
  CHECK(answers == 3 && !answer_ok, "Escape answers cancel");
  done(w);
}

/* The calls say what they do and say when they are misused: a timer once or again, listeners that end, a history per
   window, a date by its parts, a field told only of the user, segments with none yet, a custom view's value moving
   with the motion; a wrong kind, a missing size, an unknown icon or another thread reported. */
static int ticks, changes;
static void tick(void *u) { (void)u; ticks++; }
static void changed_look(void *u) { (void)u; changes++; }
static void field_told(rk_view *f, void *u) { (void)f; (void)u; changes++; }
static void act_up(void *u) { (*(int *)u)++; }
static void act_down(void *u) { (*(int *)u)--; }
static void *off_thread(void *u) { (void)u; rk_release(rk_label("from elsewhere", RK_TEXT_BODY)); return NULL; }

/* What a view says and offers changes after it is made: every property has its setter. */
static void test_setters(void) {
  rk_view *b = rk_button("save", NULL, NULL);
  rk_size was = rk_view_measure(b, (rk_size){ INFINITY, INFINITY });
  rk_button_set_label(b, "save everything");
  CHECK(rk_view_measure(b, (rk_size){ INFINITY, INFINITY }).w > was.w && rk_view_find(b, "save everything") == b, "a button's new label is drawn and found");
  rk_release(b);
  rk_view *l = rk_label("Today", RK_TEXT_BODY);
  float h = rk_view_measure(l, (rk_size){ 400, INFINITY }).h;
  rk_label_set_style(l, RK_TEXT_GROUP);
  CHECK(rk_view_measure(l, (rk_size){ 400, INFINITY }).h > h, "a label takes a new style");
  rk_release(l);
  const char *three[] = { "a", "b", "c" }, *two[] = { "x", "y" };
  rk_view *seg = rk_segments(three, 3, 2), *sel = rk_select(three, 3, 1), *ch = rk_choices(three, 3, 0);
  rk_control_set_options(seg, two, 2); rk_control_set_options(sel, two, 2); rk_control_set_options(ch, two, 2);
  CHECK(rk_control_value(seg) == -1 && rk_control_value(sel) == 1 && rk_control_value(ch) == 0, "new options keep a choice they have, let go one they lack");
  rk_release(seg); rk_release(sel); rk_release(ch);
  rk_view *sl = rk_slider(0, 100, 10, 80);
  rk_control_set_range(sl, 0, 50, 5);
  CHECK(rk_control_value(sl) == 50, "a new range fits the value in (%g)", rk_control_value(sl));
  rk_release(sl);
  rk_view *k = rk_check("wifi", true), *f = rk_field("Mail", "name@example.com"), *i = rk_icon("home", RK_ROLE_TEXT);
  int said = logged;
  rk_control_set_label(k, "bluetooth"); rk_field_set_label(f, "Address"); rk_field_set_placeholder(f, "street"); rk_icon_set(i, "bell", RK_ROLE_ACCENT);
  CHECK(logged == said, "setters on their own kinds say nothing (%s)", logged_last);
  rk_release(k); rk_release(f); rk_release(i);
  rk_view *row = rk_hstack(), *a1 = rk_add(row, rk_label("a", RK_TEXT_BODY));
  rk_view_set_frame(row, (rk_rect){ 0, 0, 200, 100 });
  rk__layout(row);
  CHECK(rk_view_frame(a1).h < 100, "a row keeps its children to the start across (AUTO)");
  rk_release(row);
}

static void test_api_rules(void) {
  rk_view *bar = rk_hstack();
  rk_view *more = rk_add(bar, rk_button(NULL, NULL, NULL));
  rk_button_set_icon(more, "more");
  rk_view *edit = rk_add(bar, rk_button("edit", NULL, NULL));
  rk_button_set_icon(edit, "edit");
  CHECK(rk_view_find(bar, "more") == more && rk_view_find(bar, "edit") == edit, "a button is found by its label, or its icon alone");
  rk_release(bar);
  ticks = 0;
  rk_app_after(10, tick, NULL);
  uint32_t every = rk_app_every(10, tick, NULL);
  rk_app_run_for(55);
  rk_app_cancel(every);
  int t1 = ticks;
  rk_app_run_for(50);
  CHECK(t1 >= 5 && ticks == t1, "after runs once, every again and again until cancelled (%d)", t1);
  changes = 0;
  uint32_t l = rk_app_on_change(changed_look, NULL);
  rk_app_set_theme(RK_THEME_DARK);
  rk_app_cancel(l);
  rk_app_set_theme(RK_THEME_LIGHT);
  CHECK(changes == 1, "a change listener ends with rk_app_cancel (%d)", changes);
  rk_window *a = window(400, 300), *b = window(400, 300);
  show(a, col(8, 8), "a"); show(b, col(8, 8), "b");
  int na = 0, nb = 0;
  rk_window_do(a, &(rk_action){ .size = sizeof(rk_action), .label = "a", .run = act_up, .undo = act_down, .user = &na });
  rk_window_do(b, &(rk_action){ .size = sizeof(rk_action), .label = "b", .run = act_up, .undo = act_down, .user = &nb });
  CHECK(rk_window_undo(a) && na == 0 && nb == 1 && !rk_window_undo(a), "undo in one window takes back only what was done there");
  int was = logged;
  rk_window_do(a, &(rk_action){ .label = "no size", .run = act_up, .user = &na });
  CHECK(na == 0 && logged == was + 1, "an action without its size: not run, and said");
  done(b);
  rk_view *page = col(8, 8);
  rk_view *date = rk_add(page, rk_date(2026, 10, 14));
  rk_view *f = rk_add(page, rk_field("name", NULL));
  rk_view *seg = rk_add(page, rk_segments((const char *const[]){ "a", "b" }, 2, -1));
  show(a, page, "rules");
  int y, m, d;
  CHECK(rk_date_value(date, &y, &m, &d) && y == 2026 && m == 10 && d == 14, "a date by its parts");
  rk_date_set_value(date, 0, 0, 0);
  CHECK(!rk_date_value(date, NULL, NULL, NULL), "cleared: none");
  changes = 0;
  rk_field_on_change(f, field_told, NULL);
  rk_field_set_text(f, "set by the app");
  rk_window_focus(a, f); type_text(a, "!");
  CHECK(changes == 1, "a field's handler hears the user, not the app (%d)", changes);
  CHECK(rk_control_value(seg) == -1, "segments may have none chosen yet");
  was = logged;
  rk_label_set_text(seg, "x");
  rk_tree_add(f, -1, "x");
  CHECK(logged == was + 2 && strstr(logged_last, "rk_tree_add") && strstr(logged_last, "not a tree"), "a call on the wrong kind of view is said: %s", logged_last);
  was = logged;
  rk_release(rk_icon("no-such-icon", RK_ROLE_TEXT));
  CHECK(logged == was + 1 && strstr(logged_last, "no-such-icon"), "an unknown icon is said");
  rk_view *custom = rk_add(page, rk_view_create());
  rk_animate_begin(RK_MOTION_MORPH);
  rk_view_animate_value(custom, 1);
  rk_animate_commit();
  rk_app_run_for(rk_duration_of(RK_DURATION_LONG) / 2);
  float mid = rk_view_value(custom);
  rk_app_run_for(rk_duration_of(RK_DURATION_LONG));
  CHECK(mid > 0.1f && mid < 0.99f && rk_view_value(custom) == 1, "a custom view's value moves with the morph motion (%.2f half way)", mid);
#if !defined(_WIN32)
  was = logged;
  pthread_t th;
  pthread_create(&th, NULL, off_thread, NULL);
  pthread_join(th, NULL);
  CHECK(logged > was && strstr(logged_last, "another thread"), "a call from another thread is said");
#endif
  done(a);
}

static void test_tree(void) {
  rk_window *w = window(560, 420);
  rk_view *root = col(16, 24);
  rk_view *tr = rk_add(root, rk_tree());
  int src = rk_tree_add(tr, -1, "src"), ui = rk_tree_add(tr, src, "ui");
  int button = rk_tree_add(tr, ui, "button.c"), field = rk_tree_add(tr, ui, "field.c");
  int core = rk_tree_add(tr, src, "core.c"), docs = rk_tree_add(tr, -1, "docs");
  int guide = rk_tree_add(tr, docs, "guide.md"), readme = rk_tree_add(tr, -1, "README.md");
  rk_tree_set_meta(tr, core, "failed", RK_STATUS_ERROR);
  rk_tree_set_meta(tr, readme, "2 kB", RK_STATUS_NONE);
  rk_tree_on_show(tr, tree_show, NULL);
  rk_tree_on_toggle(tr, tree_toggled, NULL);
  show(w, root, "tree");
  float H = rk_metric_of(RK_ROW_HEIGHT);
  CHECK(CLOSE_TO(tr->frame.h, 3 * H, 0.5f), "closed, only the top rows: %g", tr->frame.h);
  CHECK(rk_tree_parent(tr, field) == ui && !strcmp(rk_tree_title(tr, guide), "guide.md"), "parents and titles");
  /* the keyboard: Right opens, Right again goes in, Down moves without selecting */
  rk_window_focus(w, tr);
  key(w, RK_KEY_RIGHT, 0);
  CHECK(rk_tree_is_open(tr, src) && tree_toggles == 1, "Right opens the branch");
  rk_app_run_for(120);
  CHECK(tr->frame.h > 3 * H + 1 && tr->frame.h < 5 * H - 1, "half open, the rows below make room: %g", tr->frame.h);
  rk_app_run_for(300);
  CHECK(CLOSE_TO(tr->frame.h, 5 * H, 0.5f), "open: its two rows too");
  key(w, RK_KEY_RIGHT, 0);
  key(w, RK_KEY_RIGHT, 0);
  key(w, RK_KEY_RIGHT, 0);
  key(w, RK_KEY_DOWN, 0);
  CHECK(rk_tree_is_open(tr, ui) && rk_tree_selected(tr) == -1, "in and open, nothing selected yet");
  key(w, RK_KEY_ENTER, 0);
  CHECK(tree_shown == field && rk_tree_selected(tr) == field, "Enter selects and shows: %d", tree_shown);
  /* Left goes up, then closes */
  key(w, RK_KEY_LEFT, 0);
  key(w, RK_KEY_LEFT, 0);
  CHECK(!rk_tree_is_open(tr, ui), "Left goes up to the branch, then closes it");
  /* a letter: the next row that starts with it */
  type_text(w, "r");
  key(w, RK_KEY_ENTER, 0);
  CHECK(tree_shown == readme, "a letter goes to README.md");
  /* * opens every sibling */
  type_text(w, "*");
  rk_app_run_for(400);
  CHECK(rk_tree_is_open(tr, src) && rk_tree_is_open(tr, docs), "* opens the siblings");
  /* the pointer: a click on the chevron opens without selecting; a click on the row selects and shows */
  rk_rect r = rk__window_rect(tr);
  float ui_y = r.y + 1.5f * H;                                 /* src, ui, ... */
  click(w, (rk_point){ r.x + rk_metric_of(RK_SPACE_M) + 8, ui_y }, 30);
  rk_app_run_for(400);
  CHECK(rk_tree_is_open(tr, ui) && rk_tree_selected(tr) == readme, "the chevron opens, the selection stays");
  click(w, (rk_point){ r.x + 120, r.y + 2.5f * H }, 30);       /* button.c */
  CHECK(tree_shown == button, "a click selects and shows button.c: %d", tree_shown);
  rk_app_run_for(500);
  click(w, (rk_point){ r.x + 120, r.y + 5.5f * H }, 30);       /* docs: twice opens and closes */
  rk_app_run_for(60);
  click(w, (rk_point){ r.x + 120, r.y + 5.5f * H }, 30);
  CHECK(!rk_tree_is_open(tr, docs), "a double click closes docs");
  rk_app_run_for(500);
  rk_window_focus(w, tr);                                      /* from the keyboard again: the active row is ringed */
  key(w, RK_KEY_DOWN, 0);
  rk_app_run_for(300);
  shot(w, "tree");
  done(w);
}

/* A split: side by side with a handle that drags the list's width; compact, one pane at a time that turns. */
static float split_told = -1;
static void split_resized(rk_view *sp, float width, void *u) { (void)sp; (void)u; split_told = width; }
static void split_item(int index, rk_list_item *out, void *user) {
  static char t[32];
  (void)user;
  snprintf(t, sizeof t, "Run %d", 4182 - index);
  out->title = t; out->meta = index % 3 ? "done" : NULL; out->status = index % 3 ? RK_STATUS_NONE : RK_STATUS_ERROR;
}
static void pointer(rk_window *w, int type, rk_point p) {
  rk_event e = { sizeof e, type, rk_app_now(), p, 0, 0, 0, 0, 0, NULL, NULL, 0 };
  rk_window_inject(w, &e);
}

static void test_split(void) {
  rk_window *w = window(1100, 560);
  rk_view *detail = col(16, 0);
  rk_add(detail, rk_label("Run 4182", RK_TEXT_GROUP));
  rk_add(detail, rk_label("Nightly import, failed after 3 min.", RK_TEXT_BODY));
  rk_view *sp = rk_split(rk_list(40, split_item, NULL), detail, "runs");
  rk_split_on_resize(sp, split_resized, NULL);
  rk_window_show(w, rk_controller_create_with_view(sp, "runs"));
  rk_app_run_for(100);
  float own = 4 * rk_metric_of(RK_TILE_UNIT) + 3 * rk_metric_of(RK_SPACE_S), min = rk_metric_of(RK_PANE_MIN);
  CHECK(rk_split_is_sideways(sp) && CLOSE_TO(rk_split_width(sp), own, 0.5f), "sideways, the list four tiles wide: %g", rk_split_width(sp));
  rk_view *handle = rk_view_child(sp, 3);
  rk_rect hr = rk__window_rect(handle);
  CHECK(CLOSE_TO(hr.w, 9, 0.1f) && CLOSE_TO(hr.x + hr.w / 2 - rk__window_rect(sp).x, own + 0.5f, 0.6f), "the handle lies over the hairline");
  CHECK(CLOSE_TO(rk__window_rect(detail).x - rk__window_rect(sp).x, own + 1 + rk_metric_of(RK_SPACE_L), 0.5f), "the detail starts space-l after it");
  /* the keyboard */
  rk_window_focus(w, handle);
  key(w, RK_KEY_RIGHT, 0);
  CHECK(CLOSE_TO(rk_split_width(sp), own + 16, 0.5f) && CLOSE_TO(split_told, own + 16, 0.5f), "Right widens by space-m and tells");
  key(w, RK_KEY_LEFT, RK_MOD_SHIFT);
  CHECK(CLOSE_TO(rk_split_width(sp), own + 16 - 64, 0.5f), "Shift+Left by four times that");
  key(w, RK_KEY_HOME, 0);
  CHECK(CLOSE_TO(rk_split_width(sp), min, 0.5f), "Home: the narrowest, pane-min");
  key(w, RK_KEY_END, 0);
  CHECK(CLOSE_TO(rk_split_width(sp), rk__window_rect(sp).w - min - 1, 0.5f), "End: the widest, the detail at pane-min");
  /* the pointer: a drag, then a double click gives the own width back */
  rk_split_set_width(sp, 500);
  rk_app_run_for(50);
  hr = rk__window_rect(handle);
  rk_point at = { hr.x + 4, hr.y + 100 };
  pointer(w, RK_EVENT_POINTER_DOWN, at);
  pointer(w, RK_EVENT_POINTER_MOVE, (rk_point){ at.x + 30, at.y });
  pointer(w, RK_EVENT_POINTER_MOVE, (rk_point){ at.x + 60, at.y });
  pointer(w, RK_EVENT_POINTER_UP, (rk_point){ at.x + 60, at.y });
  CHECK(CLOSE_TO(rk_split_width(sp), 560, 0.5f) && CLOSE_TO(split_told, 560, 0.5f), "a drag moves the edge with the pointer: %g", rk_split_width(sp));
  rk_app_run_for(500);
  hr = rk__window_rect(handle);
  click(w, (rk_point){ hr.x + 4, hr.y + 100 }, 20);
  rk_app_run_for(60);
  click(w, (rk_point){ hr.x + 4, hr.y + 100 }, 20);
  CHECK(CLOSE_TO(rk_split_width(sp), own, 0.5f) && split_told == 0, "a double click: its own width again");
  rk_split_set_detail_shown(sp, true);
  rk_window_focus(w, NULL);                                    /* the handle lets go of the focus: a hairline again */
  rk_app_run_for(600);
  shot(w, "split");
  /* compact: one pane, a turn each way, the crumb and Escape go back */
  rk__window_resized(w, 560, 560, 1);
  rk_app_run_for(100);
  CHECK(!rk_split_is_sideways(sp) && rk_split_is_detail_shown(sp) && rk_view_child(sp, 0)->hidden, "compact: the detail shown before stays");
  key(w, RK_KEY_ESCAPE, 0);
  rk_app_run_for(1200);
  CHECK(!rk_split_is_detail_shown(sp) && rk_view_child(sp, 1)->hidden, "nothing focused, Escape still turns back to the list");
  rk_split_set_detail_shown(sp, true);
  rk_app_run_for(120);
  CHECK(!rk_split_is_detail_shown(sp) && !rk_view_child(sp, 0)->hidden, "the list turns away first");
  rk_app_run_for(1200);
  CHECK(rk_split_is_detail_shown(sp) && rk_window_focused(w) == rk_view_child(sp, 2), "then the detail, the crumb focused");
  shot(w, "split-compact");
  key(w, RK_KEY_ESCAPE, 0);
  rk_app_run_for(1200);
  CHECK(!rk_split_is_detail_shown(sp), "Escape turns back to the list");
  done(w);
}

/* Sortable rows: the grip drags a row, the others step aside; the keyboard picks it up, moves it, puts it down. */
static int sort_from = -1, sort_to = -1, sort_moves;
static void sort_moved(rk_view *rows, int from, int to, void *u) { (void)rows; (void)u; sort_from = from; sort_to = to; sort_moves++; }
static const char *row_title(rk_view *rows, int i) { return ((row *)rk_view_child(rows, i)->state)->title; }

static void test_sort(void) {
  rk_window *w = window(520, 420);
  rk_view *root = col(16, 24);
  rk_view *rows = rk_add(root, rk_rows());
  static const char *const steps[] = { "Check out", "Build", "Test", "Package", "Deploy" };
  for (int i = 0; i < 5; i++) rk_rows_add(rows, steps[i], i == 4 ? "manual" : NULL, RK_STATUS_NONE);
  rk_rows_set_sortable(rows, sort_moved, NULL);
  show(w, root, "sort");
  rk_view *r0 = rk_view_child(rows, 0), *g0 = ((row *)r0->state)->grip;
  CHECK(g0 && g0->focusable, "each row has a grip that takes focus");
  float H = r0->frame.h;
  rk_rect gr = rk__window_rect(g0);
  rk_point at = { gr.x + 12, gr.y + gr.h / 2 };
  /* a press and a release without moving: no move */
  click(w, at, 30);
  CHECK(sort_moves == 0, "a click on the grip moves nothing");
  /* a drag: 3 px is not yet one; then the row lifts and follows, the others step aside */
  pointer(w, RK_EVENT_POINTER_DOWN, at);
  pointer(w, RK_EVENT_POINTER_MOVE, (rk_point){ at.x, at.y + 3 });
  CHECK(!((row *)r0->state)->lifted, "3 px: not lifted yet");
  pointer(w, RK_EVENT_POINTER_MOVE, (rk_point){ at.x, at.y + 2.2f * H });
  rk_app_run_for(300);
  rk_view *r1 = rk_view_child(rows, 0);                          /* the lifted row is drawn last */
  CHECK(((row *)r0->state)->lifted && rk_view_child(rows, 4) == r0, "lifted and drawn above the others");
  CHECK(CLOSE_TO(r0->ptransform.translate_y, 2.2f * H, 0.5f) && r1->ptransform.translate_y < -H + 1, "it follows; Build stepped up: %g", r1->ptransform.translate_y);
  shot(w, "sort");
  pointer(w, RK_EVENT_POINTER_UP, (rk_point){ at.x, at.y + 2.2f * H });
  CHECK(sort_moves == 1 && sort_from == 0 && sort_to == 2, "let go: moved from 0 to 2 (%d %d)", sort_from, sort_to);
  CHECK(!strcmp(row_title(rows, 0), "Build") && !strcmp(row_title(rows, 2), "Check out"), "in its new place");
  rk_app_run_for(600);
  CHECK(r0->ptransform.translate_y == 0 && !((row *)r0->state)->lifted, "settled");
  /* the keyboard: Space picks Deploy up, Up twice, Space puts it down */
  rk_view *dep = rk_view_child(rows, 4);
  rk_window_focus(w, ((row *)dep->state)->grip);
  key(w, RK_KEY_SPACE, 0);
  CHECK(((row *)dep->state)->lifted, "Space picks it up");
  key(w, RK_KEY_UP, 0); key(w, RK_KEY_UP, 0);
  key(w, RK_KEY_SPACE, 0);
  CHECK(rk_view_child(rows, 2) == dep && !((row *)dep->state)->lifted && sort_to == 2, "Up twice, Space puts it down at 2");
  /* Escape puts it back where it was picked up */
  key(w, RK_KEY_ENTER, 0); key(w, RK_KEY_DOWN, 0); key(w, RK_KEY_ESCAPE, 0);
  CHECK(rk_view_child(rows, 2) == dep && !((row *)dep->state)->lifted, "Escape puts it back");
  /* Alt+Down moves at once; rk_rows_move from the app */
  key(w, RK_KEY_DOWN, RK_MOD_ALT);
  CHECK(rk_view_child(rows, 3) == dep, "Alt+Down moves it at once");
  CHECK(rk_rows_move(rows, 3, 0) && rk_view_child(rows, 0) == dep && !rk_rows_move(rows, 0, 0), "rk_rows_move");
  rk_app_run_for(600);
  done(w);
}

/* A place to drop files: files dragged over it make the edge solid; dropped, the filter sorts them; a click asks the
   system's chooser. Nothing else takes files, so dropped beside it they are refused. */
static int drop_taken, drop_refused, drop_calls;
static char drop_first[64];
static void dropped(rk_view *d, const char *const *taken, int nt, const char *const *refused, int nr, void *u) {
  (void)d; (void)refused; (void)u;
  drop_calls++; drop_taken = nt; drop_refused = nr;
  snprintf(drop_first, sizeof drop_first, "%s", nt ? taken[0] : "");
}
static bool drag(rk_window *w, int type, rk_point p, const char *const *files, int n) {
  rk_event e = { sizeof e, type, rk_app_now(), p, 0, 0, 0, 0, 0, NULL, files, n };
  return rk__window_drag(w, &e);
}

static void test_drop(void) {
  rk_window *w = window(520, 360);
  rk_view *root = col(16, 24);
  rk_add(root, rk_label("files", RK_TEXT_LABEL));
  rk_view *d = rk_add(root, rk_drop("Drop pictures here", "or choose them · nothing leaves your device", "image/*", dropped, NULL));
  rk_add(root, rk_label("Beside the place, files are refused.", RK_TEXT_BODY));
  show(w, root, "drop");
  CHECK(d->frame.h >= 120, "120 px tall at least: %g", d->frame.h);
  shot(w, "drop");
  rk_rect r = rk__window_rect(d);
  rk_point in = { r.x + r.w / 2, r.y + r.h / 2 }, out = { r.x + 10, r.y + r.h + 30 };
  static const char *const files[] = { "/home/ada/a.png", "/home/ada/notes.txt", "/home/ada/C.JPG" };
  CHECK(drag(w, RK_EVENT_DRAG_ENTER, in, NULL, 0) && ((drop_zone *)d->state)->over, "dragged over it: taken, the edge solid");
  rk_app_run_for(300);
  shot(w, "drop-over");
  CHECK(!drag(w, RK_EVENT_DRAG_MOVE, out, NULL, 0) && !((drop_zone *)d->state)->over, "beside it: nothing takes them, it lets go");
  CHECK(!drag(w, RK_EVENT_DROP, out, files, 3) && drop_calls == 0, "dropped beside it: refused, nothing heard");
  drag(w, RK_EVENT_DRAG_ENTER, in, NULL, 0);
  CHECK(drag(w, RK_EVENT_DROP, in, files, 3), "dropped on it: taken");
  CHECK(drop_calls == 1 && drop_taken == 2 && drop_refused == 1 && !strcmp(drop_first, "/home/ada/a.png"), "image/* takes a.png and C.JPG, refuses notes.txt");
  CHECK(!((drop_zone *)d->state)->over, "the edge dashed again");
  static const char *const only_text[] = { "/home/ada/notes.txt" };
  drag(w, RK_EVENT_DROP, in, only_text, 1);
  CHECK(drop_calls == 1, "only refused files: nothing heard");
  CHECK(rk__accepts(".pdf, .md", "Report.PDF") && !rk__accepts(".pdf", "pdf") && rk__accepts("application/pdf", "a.pdf") && rk__accepts(NULL, "x"), "the filter");
  /* a click asks the system's chooser; its answer goes through the same filter */
  static const char *const chosen[] = { "/tmp/photo.jpeg", "/tmp/readme.md" };
  rk__headless_choice = chosen; rk__headless_nchoice = 2;
  click(w, in, 30);
  rk_app_run_for(300);
  CHECK(drop_calls == 2 && drop_taken == 1 && !strcmp(drop_first, "/tmp/photo.jpeg"), "a click: the chooser's photo taken (%d)", drop_calls);
  rk__headless_choice = NULL; rk__headless_nchoice = 0;
  done(w);
}

/* A notification goes to the system as given; without a title there is nothing to show. */
static void test_notify(void) {
  rk__headless_note[0] = 0;
  CHECK(rk_app_notify("Build 42 passed", "in 3 min") && !strcmp(rk__headless_note, "Build 42 passed: in 3 min"), "a notification: %s", rk__headless_note);
  CHECK(rk_app_notify("Synced", NULL) && !strcmp(rk__headless_note, "Synced: "), "no body: the title alone");
  CHECK(!rk_app_notify("", "lost") && !rk_app_notify(NULL, "lost") && !strcmp(rk__headless_note, "Synced: "), "no title: nothing");
}

/* A pivot down the side: its titles in a list, Up and Down pick, the one in sight steps in; a top pivot turns the place. */
static void test_side_pivot(void) {
  rk_window *w = window(900, 500);
  rk_controller *nav = rk_pivot();
  static const char *const names[] = { "home", "builds", "people" };
  rk_view *pages[3];
  for (int i = 0; i < 3; i++) {
    pages[i] = col(16, 0);
    rk_add(pages[i], rk_label(names[i], RK_TEXT_GROUP));
    rk_view *rows = rk_add(pages[i], rk_rows());
    rk_rows_add(rows, "Nightly import", "failed", RK_STATUS_ERROR);
    rk_rows_add(rows, "schema.sql", "4 kB", RK_STATUS_NONE);
    rk_pivot_add(nav, names[i], pages[i]);
  }
  rk_pivot_set_layout(nav, RK_PIVOT_SIDE);
  rk_window_show(w, nav);
  rk_app_run_for(100);
  rk_view *strip = rk_view_child(rk_controller_view(nav), 0);
  rk_rect sr = rk__window_rect(strip), pr = rk__window_rect(pages[0]);
  CHECK(pr.x >= sr.x + sr.w && fabsf(pr.y - sr.y) < 1, "the titles stand beside the page, level with it");
  rk_window_focus(w, strip);
  key(w, RK_KEY_DOWN, 0);
  CHECK(rk_pivot_selected(nav) == 1, "Down picks the next title");
  rk_app_run_for(120);
  CHECK(rk_view_window(pages[0]) == w && rk_view_window(pages[1]) != w, "the place in sight turns away first");
  rk_app_run_for(1200);
  CHECK(rk_view_window(pages[1]) == w && rk_view_window(pages[0]) != w, "then the next one is there");
  rk_view *row = rk_view_child(rk_view_child(pages[1], 1), 1);
  CHECK(row->popacity == 1 && row->ptransform.rotate_y == 0, "every part has turned in");
  shot(w, "pivot-side");
  key(w, RK_KEY_UP, 0);
  rk_app_run_for(1200);
  CHECK(rk_pivot_selected(nav) == 0 && rk_view_window(pages[0]) == w, "Up goes back");
  /* a narrow window keeps the titles along the top, as the web's compact class does; wide again, down the side */
  rk__window_resized(w, 600, 500, 1);
  rk_app_run_for(100);
  sr = rk__window_rect(strip); pr = rk__window_rect(pages[0]);
  CHECK(pr.y >= sr.y + sr.h && sr.w > 500, "narrow: the strip runs along the top, the page below it");
  key(w, RK_KEY_RIGHT, 0);
  CHECK(rk_pivot_selected(nav) == 1, "narrow: Right picks the next title");
  rk_app_run_for(1200);
  shot(w, "pivot-side-narrow");
  rk__window_resized(w, 900, 500, 1);
  rk_app_run_for(100);
  sr = rk__window_rect(strip); pr = rk__window_rect(pages[1]);
  CHECK(pr.x >= sr.x + sr.w && fabsf(pr.y - sr.y) < 1, "wide again: the titles stand beside the page");
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
  const char *title = rk__controller_shown_title(places_for_test);   /* what the title bar names */
  CHECK(title && !strcmp(title, "a place"), "title of the place: %s", title ? title : "(none)");
  shot(w, "place");
  key(w, RK_KEY_ESCAPE, 0);
  CHECK(rk_places_depth(places_for_test) == 1, "Escape goes back");
  rk_app_run_for(1200);
  CHECK(rk_view_window(t) == w && t->popacity == 1 && t->ptransform.rotate_y == 0, "home is back as it was");
  /* the wheel moves the panorama sideways; the strip follows */
  rk_event wheel = { sizeof wheel, RK_EVENT_WHEEL, 0, { 450, 300 }, 0, 0, 600, 0, 0, NULL, NULL, 0 };
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
  rk_view *icons = rk_add(root, rk_hstack());
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
  rk_view *row = rk_add(root, rk_hstack());
  rk_button_set_kind(rk_add(row, rk_button("primary", NULL, NULL)), RK_BUTTON_PRIMARY);
  rk_add(row, rk_button("default", NULL, NULL));
  rk_button_set_kind(rk_add(row, rk_button("quiet", NULL, NULL)), RK_BUTTON_QUIET);
  rk_view *off = rk_add(row, rk_button("disabled", NULL, NULL));
  rk_view_set_enabled(off, false);
  rk_view_set_tooltip(off, "nothing to do yet");
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

/* Declared UI -------------------------------------------------------------------------------------------------------- */

typedef struct decl {
  int count, ids[8], nids, builds, makes, updates, style, menus;
  bool done, big, host, pressable, controlled, sounds;
  float progress;
  const char *button;
  char typed[64];
} decl;

static void decl_count(rk_view *v, void *user) { (void)v; ((decl *)user)->count++; }
static void decl_typed(rk_view *v, void *user) { decl *s = user; snprintf(s->typed, sizeof s->typed, "%s", rk_field_text(v)); }
static void decl_item(int i, rk_list_item *out, void *user) { (void)user; static char t[32]; snprintf(t, sizeof t, "item %d", i); out->title = t; }
static rk_view *decl_make(void *user) { ((decl *)user)->makes++; return rk_list(1000000, decl_item, NULL); }
static void decl_refresh(rk_view *list, void *user) { ((decl *)user)->updates++; rk_list_set_count(list, 1000000 - ((decl *)user)->count); }

static void decl_sounds(rk_view *v, void *user) { ((decl *)user)->sounds = rk_control_value(v) != 0; }
static void decl_menu(void *user) { ((decl *)user)->menus++; RK_ITEM("resize"); }

static void decl_build(void *user) {
  decl *s = user;
  char n[16];
  s->builds++;
  snprintf(n, sizeof n, "%d", s->count);
  RK_VSTACK() {
    RK_LABEL("Today", .key = "title", .style = (rk_text_style)s->style);
    RK_TILES() {
      RK_TILE(n, .key = "count", .label = "pressed", .status = s->done ? RK_STATUS_SUCCESS : RK_STATUS_INFO, .span = { s->big ? 2 : 1, 1 }, .on_press = decl_count);
      RK_TILE("51", .label = "icons", .menu = decl_menu);
    }
    RK_HSTACK(.key = "controls") {
      RK_TOGGLE("Sounds", .key = "sounds", .value = s->sounds, .on_change = decl_sounds);   /* the app keeps it */
      RK_CHECK("Mine", .key = "mine");                                                      /* the user keeps it */
      RK_ICON("bell", .key = "bell");
    }
    RK_PROGRESS(.key = "bar", .progress = s->progress);
    RK_ROWS(.key = "rows") {
      for (int i = 0; i < s->nids; i++) {
        snprintf(n, sizeof n, "row %d", s->ids[i]);                /* a local buffer, again and again: copied */
        RK_ROW(n, .id = (uint64_t)s->ids[i], .on_press = s->pressable ? decl_count : NULL);
      }
    }
    RK_FIELD("Mail", .key = "mail", .hint = s->typed[0] ? "Enter saves it." : "We write once a week.", .input = s->controlled ? s->typed : NULL, .on_change = decl_typed);
    RK_BUTTON(s->button ? s->button : "count", .key = "button", .on_press = decl_count);
    if (s->host) RK_HOST(.key = "list", .make = decl_make, .update = decl_refresh, .height = 200);
  }
}

static void decl_unclosed(void *user) {
  (void)user;
  RK_VSTACK() {
    RK_LABEL("left open");
    return;                                                      /* skips the scope's close: said, and closed */
  }
}

static rk_view *kid(rk_view *v, int i) { return rk_view_child(v, i); }

static void test_declare(void) {
  decl s = { .ids = { 1, 2, 3 }, .nids = 3 };
  rk_window *w = window(800, 760);
  rk_view *page = rk_ui_view(decl_build, &s);
  CHECK(s.builds == 1 && rk_view_count(page) == 1, "a declared view is built at once (%d builds)", s.builds);
  show(w, page, "declared");
  rk_app_run_for(400);
  rk_view *t = rk_view_find(page, "count"), *rows = rk_view_find(page, "rows"), *title = rk_view_find(page, "title");
  rk_view *r1 = kid(rows, 0), *r2 = kid(rows, 1), *r3 = kid(rows, 2), *mail = rk_view_find(page, "mail");
  CHECK(t && rk__is_tile(t) && rows && rk_view_count(rows) == 3 && title && mail, "the views as declared, found by their keys");
  CHECK(!strcmp(((tile *)t->state)->value, "0") && ((tile *)t->state)->status == RK_STATUS_INFO, "the tile as declared");

  /* nothing changed: nothing changes */
  int views = rk__live_views, b0 = s.builds;
  rk_ui_update(page);
  CHECK(s.builds == b0 + 1 && rk_view_find(page, "count") == t && kid(rows, 1) == r2 && rk__live_views == views && rk__anim_count() == 0,
        "built again with nothing new: the same views, nothing made, nothing moving (%d views, %u moving)", rk__live_views - views, rk__anim_count());

  /* a new value peeks in, a new status flips, a new span glides */
  s.count = 5;
  rk_ui_update(page);
  CHECK(t->transition == RK_TRANSITION_PEEK + 1 && !strcmp(((tile *)t->state)->value, "5") && !strcmp(((tile *)t->state)->old_value, "0"),
        "a tile's new value reaches it and peeks in");
  rk_app_run_for(600);
  s.done = true;
  rk_ui_update(page);
  CHECK(t->transition == RK_TRANSITION_FLIP + 1 && ((tile *)t->state)->status == RK_STATUS_SUCCESS, "a new status flips the tile over");
  rk_app_run_for(600);
  float narrow = t->frame.w;
  s.big = true;
  rk_ui_update(page);
  rk_app_run_for(60);
  CHECK(t->frame.w > narrow + 100 && t->pframe.w > narrow && t->pframe.w < t->frame.w - 1, "a new span glides (%g to %g, drawn at %g)", narrow, t->frame.w, t->pframe.w);
  rk_app_run_for(600);
  CHECK(t->pframe.w == t->frame.w && rk__anim_count() == 0, "and comes to rest");

  /* a handler only changes the state; the view is built again after it */
  int before = s.builds;
  rk_view_tap(rk_view_find(page, "button"));
  rk_app_run_for(50);
  CHECK(s.count == 6 && s.builds > before && !strcmp(((tile *)t->state)->value, "6"), "after a declared handler the view follows (%d)", s.count);
  rk_app_run_for(600);

  /* keyed rows: one comes at the top, the others stay who they are */
  views = rk__live_views;
  s.ids[0] = 4; s.ids[1] = 1; s.ids[2] = 2; s.ids[3] = 3; s.nids = 4;
  rk_ui_update(page);
  rk_view *r4 = kid(rows, 0);
  CHECK(rk_view_count(rows) == 4 && kid(rows, 1) == r1 && kid(rows, 2) == r2 && kid(rows, 3) == r3 && r4->entering && rk__anim_count() > 0,
        "a keyed row comes at the top and builds in; the others are the same views");
  CHECK(!strcmp(((rk__row *)r4->state)->title, "row 4"), "its title, from a buffer used for every row");
  rk_app_run_for(600);
  CHECK(!r4->entering && rk__live_views == views + 1, "it settles: one view more (%d)", rk__live_views - views);
  s.ids[0] = 4; s.ids[1] = 1; s.ids[2] = 3; s.nids = 3;
  rk_ui_update(page);
  CHECK(r2->leaving && rk_view_count(rows) == 4, "a row taken out leaves first");
  rk_app_run_for(600);
  CHECK(rk_view_count(rows) == 3 && kid(rows, 2) == r3 && rk__live_views == views, "then it is gone, and nothing is left over (%d)", rk__live_views - views);
  /* moved: 3 to the top; the longest run in order (4, 1) stays put, so only 3 moves */
  s.pressable = true;
  rk_ui_update(page);
  rk_window_focus(w, r1);
  s.ids[0] = 3; s.ids[1] = 4; s.ids[2] = 1;
  rk_ui_update(page);
  CHECK(kid(rows, 0) == r3 && kid(rows, 1) == r4 && kid(rows, 2) == r1 && rk_view_count(rows) == 3, "keyed rows move: the same views in the new order");
  CHECK(rk_window_focused(w) == r1, "focus stays on its row while the others move");
  s.ids[0] = 1; s.ids[1] = 4; s.ids[2] = 3;
  rk_ui_update(page);
  CHECK(kid(rows, 0) == r1 && kid(rows, 2) == r3 && rk_window_focused(w) == r1, "the focused row itself moves, and keeps focus");
  rk_app_run_for(600);
  s.pressable = false;
  rk_ui_update(page);

  /* a field: what the user typed stays, and so do focus and the caret */
  rk_window_focus(w, mail);
  type_text(w, "ada@example");
  CHECK(!strcmp(s.typed, "ada@example") && !strcmp(((field *)mail->state)->hint, "Enter saves it."), "typing reaches the state; the hint follows it");
  s.count++;
  rk_ui_update(page);
  CHECK(!strcmp(rk_field_text(mail), "ada@example") && rk_window_focused(w) == mail, "a build leaves what was typed, and focus, alone");
  s.controlled = true;
  rk_ui_update(page);
  key(w, RK_KEY_LEFT, 0); key(w, RK_KEY_LEFT, 0);
  type_text(w, "x");
  CHECK(!strcmp(s.typed, "ada@exampxle") && !strcmp(rk_field_text(mail), "ada@exampxle") && ((field *)mail->state)->caret == 10, "controlled: the app's text equals the field's, so the caret stays (%d)", ((field *)mail->state)->caret);
  snprintf(s.typed, sizeof s.typed, "%s", "grace@example.com");
  rk_ui_update(page);
  CHECK(!strcmp(rk_field_text(mail), "grace@example.com"), "the app's new text reaches the field");
  s.controlled = false;
  rk_ui_update(page);

  /* a property the view takes only when made: a new view in the old one's place, focus with it */
  rk_view *b1 = rk_view_find(page, "button");
  rk_window_focus(w, b1);
  views = rk__live_views;
  s.button = "count again";
  rk_ui_update(page);
  rk_view *b2 = rk_view_find(page, "button");
  CHECK(b2 == b1 && !strcmp(((button *)b2->state)->label, "count again") && rk__live_views == views && rk_window_focused(w) == b2,
        "a button's new label reaches the same button, focus and all");
  s.style = RK_TEXT_TITLE;
  rk_ui_update(page);
  CHECK(rk_view_find(page, "title") == title && ((label *)title->state)->style == RK_TEXT_TITLE, "a label's new style: the same label");

  /* a view the app makes: once, then told at every build; a million rows, a few views */
  views = rk__live_views;
  s.host = true;
  rk_ui_update(page);
  rk_app_run_for(600);
  rk_view *list = rk_view_find(page, "list");
  s.count++;
  rk_ui_update(page);
  CHECK(list && rk_view_find(page, "list") == list && s.makes == 1 && s.updates == 2, "a hosted view is made once and updated at each build (%d, %d)", s.makes, s.updates);
  CHECK(rk__live_views - views < 8, "a million rows stay data: %d views", rk__live_views - views);
  rk_ui_update(NULL);
  CHECK(s.updates == 3, "rk_ui_update(NULL) builds every declared view");
  s.host = false;
  rk_ui_update(page);
  rk_app_run_for(600);
  CHECK(rk__live_views == views && !rk_view_find(page, "list"), "it goes as any view (%d)", rk__live_views - views);

  /* controls: the app's value reaches the view; the user's stays when the app keeps none */
  rk_view *sounds = rk_view_find(page, "sounds"), *mine = rk_view_find(page, "mine"), *bar = rk_view_find(page, "bar");
  rk_view_tap(sounds); rk_view_tap(mine);
  rk_app_run_for(50);
  CHECK(s.sounds && rk_control_value(sounds) == 1 && rk_control_value(mine) == 1, "a toggle's change reaches the state; a check keeps the user's");
  s.sounds = false; s.progress = 0.5f;
  rk_ui_update(page);
  CHECK(rk_control_value(sounds) == 0 && rk_control_value(mine) == 1 && bar->value == 0.5f, "the app's value reaches its toggle, the progress bar glides to its own");
  rk_view *menu = kid(rk_view_parent(t), 1)->menu;
  rk_ui_update(page);
  CHECK(menu && kid(rk_view_parent(t), 1)->menu == menu && s.menus > 1 && rk__is_object(rk_view_find(page, "bell"), &rk__view_type),
        "a menu is declared at each build, its view made once; an icon is declared too");
  rk_app_run_for(400);

  /* misuse is said */
  int was = logged;
  rk_ui_put(RK_UI(RK_UI_LABEL, .text = "nowhere"));
  CHECK(logged == was + 1, "declaring outside a build is said");
  rk_view *open = rk_ui_view(decl_unclosed, NULL);
  CHECK(logged == was + 2 && rk_view_count(open) == 1 && rk_view_count(kid(open, 0)) == 1, "a scope left open is said, and closed (%s)", logged_last);
  rk_release(open);
  rk_ui_update(rows);
  CHECK(logged == was + 3, "updating a view that is not declared is said");
  done(w);
  CHECK(ui_roots == NULL, "a declared view goes with its window");
}

/* The same page, imperative and declared, drawn the same - also while it moves. */
static int hello_count = 312;
static rk_view *hello_tile;
static void hello_press(rk_view *b, void *u) {
  (void)b; (void)u;
  char text[16];
  snprintf(text, sizeof text, "%d", ++hello_count);
  rk_view_transition(hello_tile, RK_TRANSITION_PEEK);
  rk_tile_set_stat(hello_tile, text, "finished today");
}
static rk_view *hello_imperative(void) {
  rk_view *page = rk_vstack();
  rk_add(page, rk_label("Today", RK_TEXT_GROUP));
  rk_view *tiles = rk_add(page, rk_tile_grid());
  hello_tile = rk_add(tiles, rk_tile("312", "finished today"));
  rk_tile_set_status(hello_tile, RK_STATUS_SUCCESS, NULL);
  rk_view_set_span(hello_tile, 2, 1);
  rk_add(tiles, rk_tile("0.3", "release notes"));
  rk_add(page, rk_button("build one", hello_press, NULL));
  return page;
}
static int hello_declared_count = 312;
static void hello_declared_press(rk_view *b, void *u) { (void)b; (void)u; hello_declared_count++; }
static void hello_declared(void *user) {
  char builds[16];
  (void)user;
  snprintf(builds, sizeof builds, "%d", hello_declared_count);
  RK_VSTACK() {
    RK_LABEL("Today", .style = RK_TEXT_GROUP);
    RK_TILES() {
      RK_TILE(builds, .label = "finished today", .status = RK_STATUS_SUCCESS, .span = { 2, 1 });
      RK_TILE("0.3", .label = "release notes");
    }
    RK_BUTTON("build one", .on_press = hello_declared_press);
  }
}
static bool same_pixels(rk_window *a, rk_window *b) {
  if (a->fb->w != b->fb->w || a->fb->h != b->fb->h) return false;
  for (int y = 0; y < a->fb->h; y++) if (memcmp(a->fb->px + (size_t)y * a->fb->stride, b->fb->px + (size_t)y * b->fb->stride, (size_t)a->fb->w * 4)) return false;
  return true;
}
static void test_declare_same_pixels(void) {
  rk_window *a = window(640, 400), *b = window(640, 400);
  rk_controller *pa = rk_panorama(), *pb = rk_panorama();
  rk_panorama_add(pa, "today", hello_imperative(), 0);
  rk_panorama_add(pb, "today", rk_ui_view(hello_declared, NULL), 0);
  rk_window_show(a, pa); rk_window_show(b, pb);
  rk_app_run_for(1200);
  CHECK(same_pixels(a, b), "hello, declared, draws what hello draws");
  rk_view_tap(rk_view_find(rk_controller_view(pa), "build one"));
  rk_view_tap(rk_view_find(rk_controller_view(pb), "build one"));
  rk_app_run_for(120);
  CHECK(hello_count == hello_declared_count && same_pixels(a, b), "and the same while the new number peeks in");
  rk_app_run_for(600);
  CHECK(same_pixels(a, b), "and when it rests");
  done(a); done(b);
}


/* Declared windows: bound values, navigation, collections, menus ---------------------------------------------------- */

typedef struct dform {
  bool wifi, agree; int size, changes, ticks; double volume, count; rk_day day; char name[16];
} dform;
static const char *const dsizes[] = { "small", "medium", "large" };
static void dform_changed(rk_view *v, void *u) { (void)v; ((dform *)u)->changes++; }
static void dform_tick(void *u) { dform *f = u; f->count = 7; f->ticks++; }
static void dform_build(void *user) {
  dform *f = user;
  RK_TOGGLE("Wi-Fi", .key = "wifi", .on = &f->wifi, .on_change = dform_changed);
  RK_SEGMENTS(.key = "size", .options = dsizes, .count = 3, .chosen = &f->size);
  RK_SLIDER(.key = "volume", .min = 0, .max = 10, .step = 1, .number = &f->volume);
  RK_STEPPER(.key = "count", .min = 0, .max = 9, .step = 1, .number = &f->count);
  RK_DATE(.key = "day", .day = &f->day);
  RK_FIELD("Name", .key = "name", .buffer = f->name, .buffer_size = sizeof f->name);
  RK_CHECK("I agree", .key = "agree", .on = &f->agree);
  RK_BUTTON("go", .key = "go", .disabled = !f->agree, .tooltip = f->agree ? NULL : "agree first");
}

static rk_view *in_window(rk_window *w, const char *name) { return rk_view_find(w->root, name); }

static void test_declared_bindings(void) {
  dform f = { .day = { 2026, 10, 14 } };
  rk_window_desc d = { .size = sizeof d, .title = "form", .width = 800, .height = 900, .build = dform_build, .user = &f };
  rk_window *w = rk_window_create(&d);
  rk_app_run_for(300);
  rk_view *wifi = in_window(w, "wifi"), *size = in_window(w, "size"), *vol = in_window(w, "volume"), *count = in_window(w, "count");
  rk_view *day = in_window(w, "day"), *name = in_window(w, "name"), *agree = in_window(w, "agree"), *go = in_window(w, "go");
  CHECK(wifi && size && vol && count && day && name && agree && go && rk_window_root(w), "a window built from a declaration shows it");
  int y, m, dd;
  CHECK(rk_date_value(day, &y, &m, &dd) && y == 2026 && m == 10 && dd == 14, "a bound date shows the app's day");
  rk_view_tap(wifi);
  rk_app_run_for(50);
  CHECK(f.wifi && f.changes == 1 && rk_control_value(wifi) == 1, "the user's change is written where it is bound, then told");
  f.wifi = false;
  rk_ui_update(NULL);
  CHECK(rk_control_value(wifi) == 0, "the app's change to a bound value reaches the view");
  rk_window_focus(w, size); key(w, RK_KEY_RIGHT, 0);
  rk_window_focus(w, vol); key(w, RK_KEY_RIGHT, 0); key(w, RK_KEY_RIGHT, 0);
  CHECK(f.size == 1 && f.volume == 2, "segments and a slider write their choice and number (%d, %g)", f.size, f.volume);
  rk_window_focus(w, day); key(w, RK_KEY_UP, 0);
  CHECK(f.day.year != 2026 || f.day.month != 10 || f.day.day != 14, "a date writes the day the user picks (%d-%d-%d)", f.day.year, f.day.month, f.day.day);
  rk_window_focus(w, name);
  type_text(w, "Ada Lovelace Byron");
  CHECK(!strcmp(f.name, "Ada Lovelace By") && !strcmp(rk_field_text(name), "Ada Lovelace By"), "a bound field's text is cut to fit its buffer, and so is the field (%s)", rk_field_text(name));
  CHECK(!rk_view_is_enabled(go) && go->tooltip, "a declared view is disabled with its reason");
  rk_view_tap(agree);
  rk_app_run_for(50);
  CHECK(f.agree && rk_view_is_enabled(go) && !go->tooltip, "and enabled when the state says so");
  rk_app_after(10, dform_tick, &f);
  rk_app_run_for(40);
  CHECK(f.ticks == 1 && rk_control_value(count) == 7, "after the app's timer the view follows, with no call for it");
  done(w);
  rk_app_run_for(50);
}

typedef struct dnav { int tab, closes, deleted, kept, opens; bool detail, sheet, asking; } dnav;
static void dnav_open(rk_view *v, void *u) { (void)v; dnav *n = u; n->detail = true; n->opens++; }
static void dnav_close(rk_view *v, void *u) { (void)v; ((dnav *)u)->closes++; }
static void dnav_delete(rk_view *v, void *u) { (void)v; ((dnav *)u)->deleted++; }
static void dnav_keep(rk_view *v, void *u) { (void)v; ((dnav *)u)->kept++; }
static void dnav_build(void *user) {
  dnav *n = user;
  RK_PLACES() {
    RK_PLACE("home") {
      RK_PIVOT(.key = "pivot", .chosen = &n->tab) {
        RK_PAGE("today") { RK_BUTTON("open", .key = "open", .on_press = dnav_open); }
        RK_PAGE("later") { RK_LABEL("later", .key = "later"); }
      }
    }
    RK_PLACE("detail", .open = &n->detail, .on_close = dnav_close) {
      RK_LABEL("the detail", .key = "detail");
      RK_SHEET("note", .open = &n->sheet) { RK_LABEL("a note", .key = "note"); }
    }
  }
  RK_CONFIRM("Delete it?", .open = &n->asking, .ok = "delete", .on_press = dnav_delete, .on_close = dnav_keep);
}

static rk_controller *places_of(rk_window *w) {
  rk_controller *c = rk_window_root(w);
  return c && c->nembedded == 1 ? c->embedded[0] : NULL;
}

static void test_declared_navigation(void) {
  dnav n = { 0 };
  rk_window_desc d = { .size = sizeof d, .title = "nav", .width = 900, .height = 600, .build = dnav_build, .user = &n };
  rk_window *w = rk_window_create(&d);
  rk_app_run_for(600);
  rk_controller *places = places_of(w);
  rk_view *pv = in_window(w, "pivot");
  CHECK(places && places->kind == K_PLACES && rk_places_depth(places) == 1 && pv, "declared places start at their first place");
  CHECK(pv && pv->controller && ((strip *)((pivot *)pv->controller->impl)->strip->state)->nav, "a pivot alone in a place is the place: its titles take the nav level");
  rk_view_tap(in_window(w, "open"));
  rk_app_run_for(1500);
  CHECK(n.opens == 1 && rk_places_depth(places) == 2 && in_window(w, "detail") && rk__view_visible(in_window(w, "detail")), "a place opened by the state turns in");
  key(w, RK_KEY_ESCAPE, 0);
  rk_app_run_for(1500);
  CHECK(!n.detail && n.closes == 1 && rk_places_depth(places) == 1, "Escape goes back: the binding is written false and on_close told");
  n.detail = true;
  rk_ui_update(NULL);
  rk_app_run_for(1500);
  n.sheet = true;
  rk_ui_update(NULL);
  rk_app_run_for(400);
  CHECK(rk_places_depth(places) == 2 && w->sheets->nkids > 0 && in_window(w, "note"), "a sheet comes down when the state opens it");
  key(w, RK_KEY_ESCAPE, 0);
  rk_app_run_for(400);
  CHECK(!n.sheet && rk_places_depth(places) == 2, "Escape takes the sheet away and writes its binding; the place stays");
  n.detail = false;
  rk_ui_update(NULL);
  rk_app_run_for(1500);
  CHECK(rk_places_depth(places) == 1 && n.closes == 1, "the app closes a place: it turns back, and on_close is not told");
  n.tab = 1;
  rk_ui_update(NULL);
  rk_app_run_for(1500);
  CHECK(rk_pivot_selected(pv->controller) == 1, "a bound page is shown");
  rk_view *strip_v = ((pivot *)pv->controller->impl)->strip;
  rk_window_focus(w, strip_v);
  key(w, RK_KEY_LEFT, 0);
  rk_app_run_for(1500);
  CHECK(n.tab == 0 && rk_pivot_selected(pv->controller) == 0, "the user's page is written back (%d)", n.tab);
  n.asking = true;
  rk_ui_update(NULL);
  rk_app_run_for(400);
  rk_view *del = rk_view_find(w->sheets, "delete");
  CHECK(del != NULL, "a confirmation asks when the state says so");
  if (del) rk_view_tap(del);
  rk_app_run_for(400);
  CHECK(!n.asking && n.deleted == 1 && n.kept == 0, "its ok runs on_press and closes it");
  n.asking = true;
  rk_ui_update(NULL);
  rk_app_run_for(400);
  key(w, RK_KEY_ESCAPE, 0);
  rk_app_run_for(400);
  CHECK(!n.asking && n.kept == 1, "Escape is cancel: on_close");
  done(w);
  rk_app_run_for(50);
}

typedef struct dcoll { int n, shown, sort, sorted, opened; bool third, hidden, src_open, swap, docs, can_export; } dcoll;
static void dcoll_item(int i, rk_list_item *out, void *u) { (void)u; static char t[32]; snprintf(t, sizeof t, "item %d", i); out->title = t; }
static const char *dcoll_cell(int r, int c, void *u) { (void)u; static char t[32]; snprintf(t, sizeof t, "%d:%d", r, c); return t; }
static void dcoll_sort(rk_view *v, int c, rk_sort s, void *u) { (void)v; (void)c; (void)s; ((dcoll *)u)->sorted++; }
static void dcoll_open(rk_view *v, void *u) { (void)v; ((dcoll *)u)->opened++; }
static void dcoll_menu(void *u) {
  dcoll *c = u;
  RK_HEADING("sort by");
  RK_ITEM("name", .chosen = &c->sort, .value = 0);
  RK_ITEM("size", .chosen = &c->sort, .value = 1);
  RK_SEPARATOR();
  RK_ITEM("hidden files", .on = &c->hidden);
  RK_ITEM("export", .shortcut = "Ctrl+E", .disabled = !c->can_export);
}
static void dcoll_build(void *user) {
  dcoll *c = user;
  RK_HSTACK(.align = RK_ALIGN_START) {
    RK_LIST(.key = "list", .count = c->n, .item = dcoll_item, .chosen = &c->shown, .width = 200, .height = 200);
    RK_TABLE(.key = "table", .count = c->n, .cell = dcoll_cell, .on_sort = dcoll_sort, .width = 300, .height = 200) {
      RK_COLUMN("name");
      RK_COLUMN("size", .flags = RK_COLUMN_NUMERIC | RK_COLUMN_SORTABLE);
      if (c->third) RK_COLUMN("note");
    }
    RK_TREE(.key = "tree", .width = 200) {
      if (c->swap && c->docs) RK_ITEM("docs", .key = "docs");
      RK_ITEM("src", .key = "src", .open = &c->src_open) {
        RK_ITEM("main.c", .key = "main.c", .on_press = dcoll_open);
        RK_ITEM("view.c", .key = "view.c", .meta = "12 KB");
      }
      if (!c->swap && c->docs) RK_ITEM("docs", .key = "docs");
    }
    RK_BUTTON("more", .key = "more", .menu = dcoll_menu);
  }
}

static void test_declared_collections(void) {
  dcoll c = { .n = 50, .shown = -1, .docs = true };
  rk_window_desc d = { .size = sizeof d, .title = "collections", .width = 1000, .height = 500, .build = dcoll_build, .user = &c };
  rk_window *w = rk_window_create(&d);
  rk_app_run_for(400);
  rk_view *lv = in_window(w, "list"), *tv = in_window(w, "table"), *tree = in_window(w, "tree"), *more = in_window(w, "more");
  CHECK(lv && tv && tree && more && ((list *)tv->state)->ncols == 2, "a list, a table with its declared columns, a tree, a button with a menu");
  c.shown = 3;
  rk_ui_update(NULL);
  CHECK(rk_list_is_selected(lv, 3) && rk_list_selection_count(lv) == 1, "a list shows the bound item");
  c.third = true;
  rk_ui_update(NULL);
  CHECK(in_window(w, "table") == tv && ((list *)tv->state)->ncols == 3, "a column added: the same table");
  c.third = false;
  rk_ui_update(NULL);
  rk_view *tv2 = in_window(w, "table");
  CHECK(tv2 && tv2 != tv && ((list *)tv2->state)->ncols == 2, "a column taken out: the table made again, once");
  CHECK(!strcmp(rk_tree_title(tree, 0), "src") && !strcmp(rk_tree_title(tree, 1), "main.c") && rk_tree_parent(tree, 1) == 0, "a tree's declared items");
  c.src_open = true;
  rk_ui_update(NULL);
  CHECK(rk_tree_is_open(tree, 0), "a bound branch opens");
  rk_window_focus(w, tree);
  key(w, RK_KEY_LEFT, 0);
  rk_app_run_for(50);
  CHECK(!c.src_open && !rk_tree_is_open(tree, 0), "the user closes it: written back");
  c.docs = false;
  rk_ui_update(NULL);
  CHECK(in_window(w, "tree") == tree && rk_tree_title(tree, 3) == NULL, "an item taken out goes from the same tree");
  c.docs = true; c.swap = true;
  rk_ui_update(NULL);
  rk_view *tree2 = in_window(w, "tree");
  CHECK(tree2 && !strcmp(rk_tree_title(tree2, 0), "docs") && !strcmp(rk_tree_title(tree2, 1), "src"), "items in a new order: the tree made again in it");
  rk_view *menu = more->menu;
  CHECK(menu && rk_menu_check(menu, 1) == RK_CHECKMARK_ON && rk_menu_check(menu, 2) == RK_CHECKMARK_OFF && rk_menu_check(menu, 4) == RK_CHECKMARK_OFF,
        "a declared menu: a choice checked where it is bound, a square for a bound flag");
  rk_view_tap(more);
  rk_app_run_for(300);
  key(w, RK_KEY_DOWN, 0); key(w, RK_KEY_DOWN, 0);
  key(w, RK_KEY_ENTER, 0);
  rk_app_run_for(300);
  CHECK(c.sort == 1 && rk_menu_check(menu, 2) == RK_CHECKMARK_ON && rk_menu_check(menu, 1) == RK_CHECKMARK_OFF, "picking a choice writes it, and the marks follow (%d)", c.sort);
  done(w);
  rk_app_run_for(50);
}

typedef struct dskip { int builds, inner; uint32_t version; int pulse; } dskip;
static void dskip_build(void *u) {
  dskip *s = u;
  s->builds++;
  RK_VSTACK(.version = s->version) {
    s->inner++;
    RK_TILES() RK_TILE("1", .key = "tile", .label = "stays", .status = RK_STATUS_SUCCESS, .pulse = (uint32_t)s->pulse);
  }
}
static void test_declared_skip(void) {
  dskip s = { .version = 1 };
  rk_window *w = window(400, 300);
  rk_view *v = rk_ui_view(dskip_build, &s);
  show(w, v, "skip");
  rk_app_run_for(300);
  int inner = s.inner;
  rk_ui_update(v);
  CHECK(s.inner == inner && rk_view_find(v, "tile"), "a scope with the same version is not built again; what it holds stays");
  s.version = 2;
  rk_ui_update(v);
  CHECK(s.inner == inner + 1, "a new version builds it");
  rk_view *t = rk_view_find(v, "tile");
  s.pulse = 1; s.version = 3;
  rk_ui_update(v);
  CHECK(t->transition == RK_TRANSITION_FLIP + 1, "a new pulse turns a tile over with nothing else new");
  rk_app_run_for(600);
  done(w);
}

typedef struct dmore { bool detail; int order[3], moves, tab, closes; } dmore;
static void dmore_moved(rk_view *v, int from, int to, void *u) {
  (void)v;
  dmore *m = u;
  int x = m->order[from];
  if (from < to) memmove(m->order + from, m->order + from + 1, (size_t)(to - from) * sizeof *m->order);
  else memmove(m->order + to + 1, m->order + to, (size_t)(from - to) * sizeof *m->order);
  m->order[to] = x; m->moves++;
}
static void dmore_closed(rk_view *v, void *u) { (void)v; ((dmore *)u)->closes++; }
static void dmore_build(void *user) {
  dmore *m = user;
  static const char *const names[] = { "one", "two", "three" };
  RK_SPLIT("list", .key = "split", .open = &m->detail, .on_close = dmore_closed) {
    RK_SCROLL(.key = "scroll") for (int i = 0; i < 40; i++) RK_LABEL("a line among many", .id = (uint64_t)i + 1);
    RK_ROWS(.key = "rows", .on_move = dmore_moved) for (int i = 0; i < 3; i++) RK_ROW(names[m->order[i]], .id = (uint64_t)m->order[i] + 1);
  }
}
static void dtop_build(void *user) {
  RK_PIVOT(.key = "pivot", .chosen = &((dmore *)user)->tab) {
    RK_PAGE("one") RK_LABEL("first");
    RK_PAGE("two") RK_LABEL("second");
  }
}
static void dview_build(void *user) { (void)user; RK_PIVOT() RK_PAGE("nowhere") RK_LABEL("no"); }

static void test_declared_more(void) {
  dmore m = { .order = { 0, 1, 2 } };
  rk_window_desc d = { .size = sizeof d, .title = "more", .width = 500, .height = 400, .build = dmore_build, .user = &m };   /* compact */
  rk_window *w = rk_window_create(&d);
  rk_app_run_for(400);
  rk_view *split = in_window(w, "split"), *rows = in_window(w, "rows"), *scroll = in_window(w, "scroll");
  CHECK(split && rows && scroll && !rk_split_is_sideways(split) && !rk_split_is_detail_shown(split), "a declared split, compact: its list first");
  m.detail = true;
  rk_ui_update(NULL);
  rk_app_run_for(1500);
  CHECK(rk_split_is_detail_shown(split), "a bound detail turns in");
  key(w, RK_KEY_ESCAPE, 0);
  rk_app_run_for(1500);
  CHECK(!m.detail && m.closes == 1 && !rk_split_is_detail_shown(split), "Escape back to the list writes the binding and tells on_close");
  m.detail = true;
  rk_ui_update(NULL);
  rk_app_run_for(1500);
  rk_view *first = rk_view_child(rows, 0);
  rk_window_focus(w, ((rk__row *)first->state)->grip);
  key(w, RK_KEY_DOWN, RK_MOD_ALT);
  rk_app_run_for(800);
  CHECK(m.moves == 1 && m.order[1] == 0 && rk_view_child(rows, 1) == first && rk_view_count(rows) == 3, "a row the user moves: the app's order follows, the same views (%d: %d %d %d, grip %p)", m.moves, m.order[0], m.order[1], m.order[2], (void *)((rk__row *)first->state)->grip);
  done(w);
  rk_app_run_for(50);

  dmore t = { 0 };
  rk_window_desc d2 = { .size = sizeof d2, .title = "top", .width = 900, .height = 600, .build = dtop_build, .user = &t };
  w = rk_window_create(&d2);
  rk_app_run_for(400);
  rk_view *pv = in_window(w, "pivot");
  CHECK(pv && ((strip *)((pivot *)pv->controller->impl)->strip->state)->nav && rk_window_root(w)->transparent, "a pivot alone at a window's top is the place");
  done(w);
  rk_app_run_for(50);
  int was = logged;
  rk_view *v = rk_ui_view(dview_build, NULL);
  CHECK(logged > was && rk_view_count(v) == 0, "navigation in rk_ui_view is said, not built (%s)", logged_last);
  rk_release(v);
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
  test_controls();
  test_image();
  test_scroll();
  test_select();
  test_date();
  test_textarea();
  test_undo();
  test_focus();
  test_navigation();
  test_side_pivot();
  test_tree();
  test_split();
  test_sort();
  test_drop();
  test_notify();
  test_enabled();
  test_limits();
  test_damage();
  test_list_keys();
  test_collections();
  test_embed();
  test_confirm_secret();
  test_setters();
  test_api_rules();
  test_spring();
  { int lv = rk__live_views; test_declare(); rk_app_run_for(800); if (getenv("RK_LEAKS")) fprintf(stderr, "test_declare: %d views more\n", rk__live_views - lv); }
  { int lv = rk__live_views; test_declare_same_pixels(); rk_app_run_for(800); if (getenv("RK_LEAKS")) fprintf(stderr, "test_declare_same_pixels: %d views more\n", rk__live_views - lv); }
  { int lv = rk__live_views; test_declared_bindings(); rk_app_run_for(800); if (getenv("RK_LEAKS")) fprintf(stderr, "test_declared_bindings: %d views more\n", rk__live_views - lv); }
  { int lv = rk__live_views; test_declared_navigation(); rk_app_run_for(800); if (getenv("RK_LEAKS")) fprintf(stderr, "test_declared_navigation: %d views more\n", rk__live_views - lv); }
  { int lv = rk__live_views; test_declared_collections(); rk_app_run_for(800); if (getenv("RK_LEAKS")) fprintf(stderr, "test_declared_collections: %d views more\n", rk__live_views - lv); }
  { int lv = rk__live_views; test_declared_more(); rk_app_run_for(800); if (getenv("RK_LEAKS")) fprintf(stderr, "more: %d views more\n", rk__live_views - lv); }
  { int lv = rk__live_views; test_declared_skip(); rk_app_run_for(800); if (getenv("RK_LEAKS")) fprintf(stderr, "test_declared_skip: %d views more\n", rk__live_views - lv); }
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
  rk_app_set_log(test_log, NULL);
  rk_app_desc d = { sizeof d, "test", "de.rekta.test", launch, NULL, NULL, true };
  rk_app_main(&d);
  printf("%d checks passed, %d failed\n", passes, fails);
  return fails ? 1 : 0;
}

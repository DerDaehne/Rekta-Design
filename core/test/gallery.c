/* gallery.c: the reference gallery's native half. Each specimen is drawn headless in a frame 320 px wide with 16 px
   around it, in light and in dark; test/gallery.mjs draws the same specimens with rekta.css and holds the two to each
   other. Writes OUT/<name>-<theme>.png (the whole window) and OUT/frames.txt (where each frame lies in it).
     build/gallery OUT */
#include "rekta.h"
#include <stdio.h>
#include <string.h>

#define WIDTH 320
#define PAD 16

static const char *out = "gallery-out";
static FILE *frames;

static rk_view *button(void) { return rk_button("save", NULL, NULL); }
static rk_view *check_on(void) { return rk_check("done", true); }
static rk_view *check_off(void) { return rk_check("done", false); }
static rk_view *toggle_on(void) { return rk_toggle("feed", true); }
static rk_view *toggle_off(void) { return rk_toggle("feed", false); }
static rk_view *segments(void) { static const char *const o[] = { "all", "failed", "waiting" }; return rk_segments(o, 3, 0); }
static rk_view *field(void) { return rk_field("name", "lowercase"); }
static rk_view *field_filled(void) { rk_view *f = rk_field("name", NULL); rk_field_set_text(f, "rekta"); return f; }
static rk_view *slider(void) { return rk_slider(0, 100, 0, 40); }
static rk_view *progress(void) { return rk_progress(0.4f); }
static rk_view *rows(void) {
  rk_view *r = rk_rows();
  rk_rows_add(r, "2010", "Windows Phone 7", RK_STATUS_NONE);
  rk_rows_add(r, "2012", "Windows 8", RK_STATUS_NONE);
  return r;
}
static rk_view *tile(void) {
  rk_view *g = rk_tile_grid(), *t = rk_tile("12", "builds today");
  rk_tile_set_status(t, RK_STATUS_SUCCESS, "done");
  rk_add(g, t);
  return g;
}
static rk_view *drop(void) { return rk_drop("Drop files here", "or choose them", NULL, NULL, NULL); }

static const struct { const char *name; rk_view *(*make)(void); } specimens[] = {
  { "button", button }, { "check-on", check_on }, { "check-off", check_off }, { "toggle-on", toggle_on },
  { "toggle-off", toggle_off }, { "segments", segments }, { "field", field }, { "field-filled", field_filled },
  { "slider", slider }, { "progress", progress }, { "rows", rows }, { "tile", tile }, { "drop", drop },
};

/* where a view lies in the window: its frame and each parent's, added up */
static rk_rect in_window(rk_view *v) {
  rk_rect r = rk_view_frame(v);
  for (rk_view *p = rk_view_parent(v); p; p = rk_view_parent(p)) { rk_rect f = rk_view_frame(p); r.x += f.x; r.y += f.y; }
  return r;
}

static void draw(int i, bool dark) {
  rk_app_set_theme(dark ? RK_THEME_DARK : RK_THEME_LIGHT);
  rk_window_desc d = { .size = sizeof d, .title = "gallery", .width = WIDTH + 2 * PAD + 64, .height = 480 };
  rk_window *w = rk_window_create(&d);
  rk_view *page = rk_column(), *frame = rk_column();
  rk_view_set_padding(frame, (rk_insets){ PAD, PAD, PAD, PAD });
  rk_view_set_size(frame, WIDTH + 2 * PAD, 0);
  rk_add(frame, specimens[i].make());
  rk_add(page, frame);
  rk_window_show(w, rk_controller_create_with_view(page, NULL));
  rk_app_run_for(2000);                                     /* everything settled */
  char path[1024];
  snprintf(path, sizeof path, "%s/%s-%s.png", out, specimens[i].name, dark ? "dark" : "light");
  rk_window_snapshot(w, path);
  rk_rect r = in_window(frame);
  fprintf(frames, "%s-%s %g %g %g %g %g\n", specimens[i].name, dark ? "dark" : "light", r.x, r.y, r.w, r.h, rk_window_scale(w));
  rk_window_close(w);
}

static void launch(void *user) {
  (void)user;
  char path[1024];
  snprintf(path, sizeof path, "%s/frames.txt", out);
  frames = fopen(path, "w");
  if (!frames) { fprintf(stderr, "gallery: cannot write %s\n", path); rk_app_quit(1); return; }
  for (int i = 0; i < (int)(sizeof specimens / sizeof *specimens); i++) { draw(i, false); draw(i, true); }
  fclose(frames);
  rk_app_quit(0);
}

int main(int argc, char **argv) {
  if (argc > 1) out = argv[1];
  rk_app_desc app = { .size = sizeof app, .name = "gallery", .launch = launch, .headless = true };
  return rk_app_main(&app);
}

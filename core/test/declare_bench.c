/* declare_bench.c: what a declared list costs against the same list changed by hand, for 10, 100 and 1000 hairline
   rows in a window: the CPU time of each change alone (the frames between changes run, uncounted). Headless.
   Usage: declare_bench [repeats] */
#include "../include/rekta_test.h"
#include "../include/rekta_ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double cpu_us(void) { return (double)clock() * 1e6 / CLOCKS_PER_SEC; }

enum { MAX = 1000 };
static int n, ids[MAX + 1], next_id, hot = -1, hot_n;
static rk_view *rows_v[MAX + 1];                   /* the imperative list keeps every row to change it */
static rk_window *window;
static int repeats = 40;

static const char *title_of(int id, char *buf, size_t size) { snprintf(buf, size, "Build #%d", id); return buf; }
static const char *meta_of(int i, char *buf, size_t size) { if (i == hot) { snprintf(buf, size, "%d min", hot_n); return buf; } return "3 min"; }

/* declared */
static void list(void *user) {
  char t[32], m[16];
  (void)user;
  RK_ROWS() for (int i = 0; i < n; i++) RK_ROW(title_of(ids[i], t, sizeof t), .id = (uint64_t)ids[i], .meta = meta_of(i, m, sizeof m));
}

/* imperative */
static rk_view *list_by_hand(void) {
  char t[32], m[16];
  rk_view *rows = rk_rows();
  for (int i = 0; i < n; i++) rows_v[i] = rk_rows_add(rows, title_of(ids[i], t, sizeof t), meta_of(i, m, sizeof m), RK_STATUS_NONE);
  return rows;
}

static void reset(int count) { n = count; next_id = count + 1; hot = -1; for (int i = 0; i < n; i++) ids[i] = i + 1; }

static rk_view *show(rk_view *content) {
  rk_view *s = rk_scroll(content);
  rk_window_show(window, rk_controller_create_with_view(s, "bench"));
  rk_app_run_for(300);
  return content;
}

/* one kind of change, made `repeats` times; the frames after each run uncounted */
#define MEASURE(total, body) do { total = 0; for (int r_ = 0; r_ < repeats; r_++) { double t0_ = cpu_us(); body; total += cpu_us() - t0_; rk_app_run_for(250); } total /= repeats; } while (0)

static void run(int count) {
  double build_i, build_d, one_i, one_d, feed_i, feed_d, same_d, rev_i, rev_d;
  char t[32], m[16];
  /* by hand */
  reset(count);
  build_i = 0;
  for (int r = 0; r < 10; r++) { double t0 = cpu_us(); rk_view *v = list_by_hand(); build_i += cpu_us() - t0; rk_release(v); }
  build_i /= 10;
  rk_view *rows = show(list_by_hand());
  MEASURE(one_i, { hot = count / 2; hot_n++; rk_row_set(rows_v[hot], title_of(ids[hot], t, sizeof t), meta_of(hot, m, sizeof m), RK_STATUS_NONE); });
  MEASURE(feed_i, {
    rk_animate_begin(RK_MOTION_ENTER);
    rk_view *row = rk_rows_add(rows, title_of(next_id++, t, sizeof t), "3 min", RK_STATUS_NONE);
    rk_insert(rows, row, 0);
    rk_animate_commit();
    rk_animate_begin(RK_MOTION_LEAVE);
    rk_view_remove(rows_v[n - 1]);
    rk_animate_commit();
    memmove(rows_v + 1, rows_v, (size_t)(n - 1) * sizeof *rows_v);
    rows_v[0] = row;
  });
  MEASURE(rev_i, {
    rk_animate_begin(RK_MOTION_REFLOW);
    for (int i = 0; i < n; i++) rk_insert(rows, rows_v[n - 1 - i], i);
    rk_animate_commit();
    for (int i = 0; i < n / 2; i++) { rk_view *x = rows_v[i]; rows_v[i] = rows_v[n - 1 - i]; rows_v[n - 1 - i] = x; }
  });
  /* declared */
  reset(count);
  build_d = 0;
  for (int r = 0; r < 10; r++) { double t0 = cpu_us(); rk_view *v = rk_ui_view(list, NULL); build_d += cpu_us() - t0; rk_release(v); }
  build_d /= 10;
  rk_view *page = show(rk_ui_view(list, NULL));
  MEASURE(same_d, rk_ui_update(page));
  MEASURE(one_d, { hot = count / 2; hot_n++; rk_ui_update(page); });
  MEASURE(feed_d, { memmove(ids + 1, ids, (size_t)(n - 1) * sizeof *ids); ids[0] = next_id++; rk_ui_update(page); });
  MEASURE(rev_d, { for (int i = 0; i < n / 2; i++) { int x = ids[i]; ids[i] = ids[n - 1 - i]; ids[n - 1 - i] = x; } rk_ui_update(page); });
  printf("%5d rows | build %8.1f %8.1f | nothing changed %8s %8.1f | one row %6.1f %8.1f | one in, one out %7.1f %8.1f | reversed %8.1f %8.1f\n",
         count, build_i, build_d, "-", same_d, one_i, one_d, feed_i, feed_d, rev_i, rev_d);
}

static void launch(void *user) {
  (void)user;
  rk_window_desc d = { .size = sizeof d, .title = "bench", .width = 800, .height = 600 };
  window = rk_window_create(&d);
  printf("CPU time in us of one change, by hand | declared (%d repeats)\n", repeats);
  run(10);
  run(100);
  run(1000);
  rk_app_quit(0);
}

int main(int argc, char **argv) {
  if (argc > 1) repeats = atoi(argv[1]) > 0 ? atoi(argv[1]) : repeats;
  rk_app_desc app = { .size = sizeof app, .name = "bench", .launch = launch, .headless = true };
  return rk_app_main(&app);
}

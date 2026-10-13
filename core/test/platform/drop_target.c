/* drop_target.c: a window that is one place to drop files, for the platform drag tests. It prints what it takes. */
#include "rekta.h"
#include <stdio.h>

static void dropped(rk_view *d, const char *const *taken, int nt, const char *const *refused, int nr, void *u) {
  (void)d; (void)u;
  for (int i = 0; i < nt; i++) printf("taken %s\n", taken[i]);
  for (int i = 0; i < nr; i++) printf("refused %s\n", refused[i]);
  fflush(stdout);
  rk_app_quit(0);
}

static void launch(void *user) {
  (void)user;
  rk_window_desc d = { .size = sizeof d, .title = "drop-target", .width = 400, .height = 300 };
  rk_window *w = rk_window_create(&d);
  rk_view *page = rk_column();
  rk_view_set_padding(page, (rk_insets){ 0, 0, 0, 0 });
  rk_view *zone = rk_add(page, rk_drop("Drop pictures here", NULL, "image/*", dropped, NULL));
  rk_view_set_grow(zone, 1);
  rk_window_show(w, rk_controller_create_with_view(page, "drop"));
  rk_release(w);
  printf("ready\n");
  fflush(stdout);
}

int main(void) {
  rk_app_desc app = { .size = sizeof app, .name = "drop-target", .launch = launch };
  return rk_app_main(&app);
}

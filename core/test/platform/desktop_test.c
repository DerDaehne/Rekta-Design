/* desktop_test.c: asks the system's file chooser for pictures and prints what it answered; with "notify", sends a
   notification instead. For the platform tests. */
#include "rekta.h"
#include <stdio.h>
#include <string.h>

static void chosen(rk_window *w, const char *const *files, int n, void *user) {
  (void)w; (void)user;
  printf("chosen %d\n", n);
  for (int i = 0; i < n; i++) printf("file %s\n", files[i]);
  fflush(stdout);
  rk_app_quit(0);
}

static void ask(void *user) {
  if (!rk_window_choose_files(user, "image/*", true, chosen, NULL)) { printf("no chooser\n"); rk_app_quit(1); }
}

static void quit(void *user) { (void)user; rk_app_quit(0); }

static void notify(void *user) {
  (void)user;
  printf(rk_app_notify("Build 42 passed", "tests & <checks> in 3 min") ? "notified\n" : "no notifications\n");
  fflush(stdout);
  rk_app_after(1000, false, quit, NULL);                   /* the call goes out from a thread of its own */
}

static bool notifying;

static void launch(void *user) {
  (void)user;
  rk_window_desc d = { .size = sizeof d, .title = "chooser-test", .width = 300, .height = 200 };
  rk_window *w = rk_window_create(&d);
  rk_window_show(w, rk_controller_create_with_view(rk_column(), "chooser"));
  rk_app_after(200, false, notifying ? notify : ask, w);
}

int main(int argc, char **argv) {
  notifying = argc > 1 && !strcmp(argv[1], "notify");
  rk_app_desc app = { .size = sizeof app, .name = "desktop-test", .id = "de.example.desktop-test", .launch = launch };
  return rk_app_main(&app);
}

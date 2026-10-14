/* scale_test.c: opens a window, then changes Xft.dpi on the root window as a desktop's settings would, and prints the
   window's scale, logical size and pixel width before and after. For the platform tests (X11). */
#include "rekta.h"
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <stdio.h>
#include <string.h>

static Display *dpy;
static rk_window *win;

static int pixel_width(void) {                       /* the window named scale-test, as the X server has it */
  Window root = DefaultRootWindow(dpy), parent, *kids = NULL;
  unsigned n = 0;
  int width = -1;
  XQueryTree(dpy, root, &root, &parent, &kids, &n);
  for (unsigned i = 0; i < n; i++) {
    char *name = NULL;
    if (XFetchName(dpy, kids[i], &name) && name) {
      XWindowAttributes a;
      if (!strcmp(name, "scale-test") && XGetWindowAttributes(dpy, kids[i], &a)) width = a.width;
      XFree(name);
    }
  }
  if (kids) XFree(kids);
  return width;
}

static void report(const char *when) {
  rk_size s = rk_window_size(win);
  printf("%s scale %g size %gx%g pixels %d\n", when, rk_window_scale(win), s.w, s.h, pixel_width());
  fflush(stdout);
}

static void quit(void *user) { (void)user; report("after"); rk_app_quit(0); }

static void change(void *user) {
  (void)user;
  report("before");
  const char *res = "Xft.dpi:\t192\nXft.antialias:\t1\n";
  XChangeProperty(dpy, DefaultRootWindow(dpy), XA_RESOURCE_MANAGER, XA_STRING, 8, PropModeReplace, (const unsigned char *)res, (int)strlen(res));
  XSync(dpy, False);
  rk_app_after(800, quit, NULL);
}

static void launch(void *user) {
  (void)user;
  rk_window_desc d = { .size = sizeof d, .title = "scale-test", .width = 300, .height = 200 };
  win = rk_window_create(&d);
  rk_window_show(win, rk_controller_create_with_view(rk_vstack(), "scale"));
  rk_app_after(300, change, NULL);
}

int main(void) {
  dpy = XOpenDisplay(NULL);
  if (!dpy) return 2;
  rk_app_desc app = { .size = sizeof app, .name = "scale-test", .launch = launch };
  return rk_app_main(&app);
}

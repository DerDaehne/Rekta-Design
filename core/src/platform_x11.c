/* platform_x11.c: Linux and BSD through Xlib (Wayland desktops run it through XWayland). Pixels go up with
   XPutImage; text input comes through XIM; the theme and the animation setting come from XSETTINGS, which GNOME,
   KDE and Xfce all publish; Xft.dpi gives the scale. Another thread wakes the loop through a pipe. */
#include "rk_internal.h"
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>
#include <unistd.h>

typedef struct xwin { rk_window *w; Window win; GC gc; XIC ic; int pw, ph; bool maximized; } xwin;

static Display *dpy;
static int screen;
static XIM im;
static int wake_pipe[2] = { -1, -1 };
static float dpi_scale = 1;
static Atom A_DELETE, A_PROTOCOLS, A_NET_NAME, A_UTF8, A_MOTIF, A_MOVERESIZE, A_STATE, A_MAX_V, A_MAX_H, A_XS_SEL, A_XS_SETTINGS;
static xwin *wins[64];
static int nwins;

static xwin *find(Window win) { for (int i = 0; i < nwins; i++) if (wins[i]->win == win) return wins[i]; return NULL; }

static double x_now(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

static void read_dpi(void) {
  const char *env = getenv("RK_SCALE");
  if (env && atof(env) > 0) { dpi_scale = (float)atof(env); return; }
  char *rms = XResourceManagerString(dpy);
  if (!rms) return;
  XrmInitialize();
  XrmDatabase db = XrmGetStringDatabase(rms);
  char *type = NULL;
  XrmValue val;
  if (db && XrmGetResource(db, "Xft.dpi", "Xft.Dpi", &type, &val) && val.addr) {
    float dpi = (float)atof(val.addr);
    if (dpi > 0) dpi_scale = roundf(dpi / 96.0f * 4) / 4;   /* to quarter steps: 1, 1.25, 1.5, 2 */
  }
  if (db) XrmDestroyDatabase(db);
}

static bool x_init(const rk_app_desc *d) {
  (void)d;
  dpy = XOpenDisplay(NULL);
  if (!dpy) return false;
  screen = DefaultScreen(dpy);
  if (pipe(wake_pipe) == 0) { fcntl(wake_pipe[0], F_SETFL, O_NONBLOCK); fcntl(wake_pipe[1], F_SETFL, O_NONBLOCK); }
  A_DELETE = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
  A_PROTOCOLS = XInternAtom(dpy, "WM_PROTOCOLS", False);
  A_NET_NAME = XInternAtom(dpy, "_NET_WM_NAME", False);
  A_UTF8 = XInternAtom(dpy, "UTF8_STRING", False);
  A_MOTIF = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
  A_MOVERESIZE = XInternAtom(dpy, "_NET_WM_MOVERESIZE", False);
  A_STATE = XInternAtom(dpy, "_NET_WM_STATE", False);
  A_MAX_V = XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_VERT", False);
  A_MAX_H = XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
  char sel[32];
  snprintf(sel, sizeof sel, "_XSETTINGS_S%d", screen);
  A_XS_SEL = XInternAtom(dpy, sel, False);
  A_XS_SETTINGS = XInternAtom(dpy, "_XSETTINGS_SETTINGS", False);
  XSetLocaleModifiers("");
  im = XOpenIM(dpy, NULL, NULL, NULL);
  read_dpi();
  /* follow XSETTINGS changes: the manager window's property */
  Window mgr = XGetSelectionOwner(dpy, A_XS_SEL);
  if (mgr) XSelectInput(dpy, mgr, PropertyChangeMask);
  return true;
}

static void x_shutdown(void) {
  if (im) XCloseIM(im);
  if (dpy) XCloseDisplay(dpy);
  dpy = NULL; im = NULL;
  if (wake_pipe[0] >= 0) { close(wake_pipe[0]); close(wake_pipe[1]); wake_pipe[0] = wake_pipe[1] = -1; }
}

static void *x_open(rk_window *w, const rk_window_desc *d) {
  if (nwins >= RK_COUNT(wins)) return NULL;
  xwin *x = rk__alloc(sizeof *x);
  x->w = w;
  x->pw = (int)(d->width * dpi_scale); x->ph = (int)(d->height * dpi_scale);
  XSetWindowAttributes a;
  memset(&a, 0, sizeof a);
  a.background_pixel = BlackPixel(dpy, screen);
  a.event_mask = ExposureMask | StructureNotifyMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask |
                 PointerMotionMask | EnterWindowMask | LeaveWindowMask | FocusChangeMask;
  x->win = XCreateWindow(dpy, RootWindow(dpy, screen), 0, 0, (unsigned)x->pw, (unsigned)x->ph, 0, CopyFromParent, InputOutput,
                         CopyFromParent, CWBackPixel | CWEventMask, &a);
  XSetWMProtocols(dpy, x->win, &A_DELETE, 1);
  XSizeHints *hints = XAllocSizeHints();
  hints->flags = PMinSize;
  hints->min_width = (int)(d->min_width * dpi_scale); hints->min_height = (int)(d->min_height * dpi_scale);
  XSetWMNormalHints(dpy, x->win, hints);
  XFree(hints);
  XClassHint *cls = XAllocClassHint();
  cls->res_name = (char *)"rekta"; cls->res_class = (char *)"Rekta";
  XSetClassHint(dpy, x->win, cls);
  XFree(cls);
  if (!d->system_frame) {
    /* Rekta draws its own title bar: no decorations (flags: decorations; decorations: none) */
    long motif[5] = { 2, 0, 0, 0, 0 };
    XChangeProperty(dpy, x->win, A_MOTIF, A_MOTIF, 32, PropModeReplace, (unsigned char *)motif, 5);
  }
  x->gc = XCreateGC(dpy, x->win, 0, NULL);
  if (im) x->ic = XCreateIC(im, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, XNClientWindow, x->win, XNFocusWindow, x->win, NULL);
  wins[nwins++] = x;
  rk__window_resized(w, x->pw / dpi_scale, x->ph / dpi_scale, dpi_scale);
  if (d->title) {
    XStoreName(dpy, x->win, d->title);
    XChangeProperty(dpy, x->win, A_NET_NAME, A_UTF8, 8, PropModeReplace, (const unsigned char *)d->title, (int)strlen(d->title));
  }
  return x;
}

static void x_close(void *n) {
  xwin *x = n;
  if (!x) return;
  for (int i = 0; i < nwins; i++) if (wins[i] == x) { wins[i] = wins[--nwins]; break; }
  if (x->ic) XDestroyIC(x->ic);
  XFreeGC(dpy, x->gc);
  XDestroyWindow(dpy, x->win);
  XFlush(dpy);
  free(x);
}

static void x_show(void *n) { xwin *x = n; if (x) { XMapWindow(dpy, x->win); XFlush(dpy); } }

static void x_title(void *n, const char *t) {
  xwin *x = n;
  if (!x) return;
  XStoreName(dpy, x->win, t);
  XChangeProperty(dpy, x->win, A_NET_NAME, A_UTF8, 8, PropModeReplace, (const unsigned char *)t, (int)strlen(t));
}

static void x_present(void *n, const rk_bitmap *fb) {
  xwin *x = n;
  if (!x || !fb) return;
  /* the frame buffer is 0xAARRGGBB in memory order B, G, R, A: a 24-bit TrueColor visual's layout */
  XImage *img = XCreateImage(dpy, DefaultVisual(dpy, screen), (unsigned)DefaultDepth(dpy, screen), ZPixmap, 0, (char *)fb->px,
                             (unsigned)fb->w, (unsigned)fb->h, 32, fb->stride * 4);
  if (!img) return;
  XPutImage(dpy, x->win, x->gc, img, 0, 0, 0, 0, (unsigned)fb->w, (unsigned)fb->h);
  img->data = NULL;
  XDestroyImage(img);
  XFlush(dpy);
}

static void net_state(xwin *x, bool on) {
  XEvent e;
  memset(&e, 0, sizeof e);
  e.xclient.type = ClientMessage; e.xclient.window = x->win; e.xclient.message_type = A_STATE; e.xclient.format = 32;
  e.xclient.data.l[0] = on ? 1 : 0; e.xclient.data.l[1] = (long)A_MAX_V; e.xclient.data.l[2] = (long)A_MAX_H; e.xclient.data.l[3] = 1;
  XSendEvent(dpy, RootWindow(dpy, screen), False, SubstructureRedirectMask | SubstructureNotifyMask, &e);
  XFlush(dpy);
}

static void x_move(void *n) {
  xwin *x = n;
  if (!x) return;
  Window root, child;
  int rx, ry, wx, wy;
  unsigned mask;
  XQueryPointer(dpy, x->win, &root, &child, &rx, &ry, &wx, &wy, &mask);
  XUngrabPointer(dpy, CurrentTime);
  XEvent e;
  memset(&e, 0, sizeof e);
  e.xclient.type = ClientMessage; e.xclient.window = x->win; e.xclient.message_type = A_MOVERESIZE; e.xclient.format = 32;
  e.xclient.data.l[0] = rx; e.xclient.data.l[1] = ry; e.xclient.data.l[2] = 8 /* move */; e.xclient.data.l[3] = Button1; e.xclient.data.l[4] = 1;
  XSendEvent(dpy, RootWindow(dpy, screen), False, SubstructureRedirectMask | SubstructureNotifyMask, &e);
  XFlush(dpy);
}

static void x_minimize(void *n) { xwin *x = n; if (x) { XIconifyWindow(dpy, x->win, screen); XFlush(dpy); } }
static void x_zoom(void *n) { xwin *x = n; if (x) { x->maximized = !x->maximized; net_state(x, x->maximized); } }

static uint32_t key_of(KeySym k) {
  switch (k) {
    case XK_Tab: case XK_ISO_Left_Tab: return RK_KEY_TAB;
    case XK_Return: case XK_KP_Enter: return RK_KEY_ENTER;
    case XK_Escape: return RK_KEY_ESCAPE;
    case XK_space: return RK_KEY_SPACE;
    case XK_Left: return RK_KEY_LEFT; case XK_Right: return RK_KEY_RIGHT; case XK_Up: return RK_KEY_UP; case XK_Down: return RK_KEY_DOWN;
    case XK_Home: return RK_KEY_HOME; case XK_End: return RK_KEY_END; case XK_Prior: return RK_KEY_PAGE_UP; case XK_Next: return RK_KEY_PAGE_DOWN;
    case XK_BackSpace: return RK_KEY_BACKSPACE; case XK_Delete: return RK_KEY_DELETE;
    case 0x1008FF26: return RK_KEY_BACK;                      /* XF86XK_Back */
    default: break;
  }
  if (k >= XK_F1 && k <= XK_F10) return RK_KEY_F1 + (uint32_t)(k - XK_F1);
  if (k >= XK_a && k <= XK_z) return (uint32_t)('A' + (k - XK_a));
  if (k >= XK_A && k <= XK_Z) return (uint32_t)k;
  if (k >= XK_0 && k <= XK_9) return (uint32_t)k;
  if (k < 0x80 && k > 0x20) return (uint32_t)k;
  return RK_KEY_NONE;
}

static uint32_t mods_of(unsigned s) {
  return ((s & ShiftMask) ? RK_MOD_SHIFT : 0) | ((s & ControlMask) ? RK_MOD_CTRL : 0) | ((s & Mod1Mask) ? RK_MOD_ALT : 0) | ((s & Mod4Mask) ? RK_MOD_SUPER : 0);
}

static void handle(XEvent *ev) {
  if (ev->type == PropertyNotify && ev->xproperty.atom == A_XS_SETTINGS) { rk__app_settings_changed(); return; }
  if (XFilterEvent(ev, None)) return;
  xwin *x = find(ev->xany.window);
  if (!x) return;
  rk_window *w = x->w;
  float k = w->scale;
  rk_event e;
  memset(&e, 0, sizeof e);
  e.size = sizeof e;
  e.time = rk_app_now();
  switch (ev->type) {
    case ConfigureNotify:
      if (ev->xconfigure.width != x->pw || ev->xconfigure.height != x->ph) {
        x->pw = ev->xconfigure.width; x->ph = ev->xconfigure.height;
        rk__window_resized(w, x->pw / dpi_scale, x->ph / dpi_scale, dpi_scale);
      }
      break;
    case Expose: if (ev->xexpose.count == 0) rk__window_invalidate(w); break;
    case ClientMessage:
      if ((Atom)ev->xclient.data.l[0] == A_DELETE) rk__window_close_request(w);
      break;
    case FocusIn: if (x->ic) XSetICFocus(x->ic); break;
    case FocusOut: if (x->ic) XUnsetICFocus(x->ic); break;
    case MotionNotify:
      e.type = RK_EVENT_POINTER_MOVE; e.position = (rk_point){ ev->xmotion.x / k, ev->xmotion.y / k }; e.mods = mods_of(ev->xmotion.state);
      rk__window_event(w, &e);
      break;
    case ButtonPress: case ButtonRelease: {
      unsigned b = ev->xbutton.button;
      e.position = (rk_point){ ev->xbutton.x / k, ev->xbutton.y / k }; e.mods = mods_of(ev->xbutton.state);
      if (b >= 4 && b <= 7) {
        if (ev->type == ButtonRelease) break;
        e.type = RK_EVENT_WHEEL;
        float step = 48;
        if (b == 4) e.wheel_y = -step; else if (b == 5) e.wheel_y = step; else if (b == 6) e.wheel_x = -step; else e.wheel_x = step;
        if (e.mods & RK_MOD_SHIFT) { e.wheel_x = e.wheel_y; e.wheel_y = 0; }
      } else if (b == 8 || b == 9) {
        if (ev->type == ButtonRelease) break;
        e.type = RK_EVENT_KEY_DOWN; e.key = RK_KEY_BACK;
      } else {
        e.type = ev->type == ButtonPress ? RK_EVENT_POINTER_DOWN : RK_EVENT_POINTER_UP;
        e.button = b == 1 ? 0 : b == 3 ? 1 : 2;
      }
      rk__window_event(w, &e);
      break;
    }
    case KeyPress: case KeyRelease: {
      char buf[64];
      KeySym sym = NoSymbol;
      Status st = 0;
      int n = 0;
      if (ev->type == KeyPress && x->ic) n = Xutf8LookupString(x->ic, &ev->xkey, buf, (int)sizeof buf - 1, &sym, &st);
      else n = XLookupString(&ev->xkey, buf, (int)sizeof buf - 1, &sym, NULL);
      if (n < 0) n = 0;
      buf[n] = 0;
      if (sym == NoSymbol) sym = XLookupKeysym(&ev->xkey, 0);
      e.type = ev->type == KeyPress ? RK_EVENT_KEY_DOWN : RK_EVENT_KEY_UP;
      e.key = key_of(sym); e.mods = mods_of(ev->xkey.state);
      if (e.key) rk__window_event(w, &e);
      if (ev->type == KeyPress && n > 0 && (unsigned char)buf[0] >= 0x20 && buf[0] != 0x7f && !(e.mods & (RK_MOD_CTRL | RK_MOD_SUPER))) {
        rk_event t = e;
        t.type = RK_EVENT_TEXT; t.text = buf; t.key = 0;
        rk__window_event(w, &t);
      }
      break;
    }
    default: break;
  }
}

static bool x_wait(double ms) {
  XFlush(dpy);
  if (!XPending(dpy)) {
    struct pollfd fds[2] = { { ConnectionNumber(dpy), POLLIN, 0 }, { wake_pipe[0], POLLIN, 0 } };
    int timeout = ms < 0 ? -1 : (int)ceil(ms);
    poll(fds, wake_pipe[0] >= 0 ? 2 : 1, timeout);
    if (wake_pipe[0] >= 0 && (fds[1].revents & POLLIN)) { char b[64]; while (read(wake_pipe[0], b, sizeof b) > 0) {} }
  }
  while (XPending(dpy)) { XEvent ev; XNextEvent(dpy, &ev); handle(&ev); }
  return true;
}

static void x_wake(void) { if (wake_pipe[1] >= 0) { char c = 1; if (write(wake_pipe[1], &c, 1) < 0) {} } }

/* XSETTINGS: a little-endian or big-endian list of typed entries on the manager's window. We read the theme name
   (a name ending in "dark" or "-dark" means dark) and whether animations are enabled. */
static void x_settings(bool *dark, bool *reduced, bool *compact) {
  *dark = false; *reduced = false; *compact = true;          /* a fine pointer: compact, as the web's media query */
  const char *gt = getenv("GTK_THEME");
  if (gt && (strstr(gt, ":dark") || strstr(gt, "-dark") || strstr(gt, "-Dark"))) *dark = true;
  if (!dpy) return;
  Window mgr = XGetSelectionOwner(dpy, A_XS_SEL);
  if (!mgr) return;
  Atom type; int format; unsigned long n, after; unsigned char *data = NULL;
  if (XGetWindowProperty(dpy, mgr, A_XS_SETTINGS, 0, 1 << 20, False, A_XS_SETTINGS, &type, &format, &n, &after, &data) != Success || !data) return;
  if (n >= 12) {
    bool be = data[0] == 'B';
    #define U32(p) (be ? ((uint32_t)(p)[0] << 24 | (uint32_t)(p)[1] << 16 | (uint32_t)(p)[2] << 8 | (p)[3]) : ((uint32_t)(p)[3] << 24 | (uint32_t)(p)[2] << 16 | (uint32_t)(p)[1] << 8 | (p)[0]))
    #define U16(p) (be ? (uint16_t)((p)[0] << 8 | (p)[1]) : (uint16_t)((p)[1] << 8 | (p)[0]))
    uint32_t count = U32(data + 8);
    size_t at = 12;
    for (uint32_t i = 0; i < count && at + 4 <= n; i++) {
      int kind = data[at];
      uint16_t len = U16(data + at + 2);
      const char *name = (const char *)data + at + 4;
      size_t p = at + 4 + ((len + 3u) & ~3u) + 4;               /* name, padded, then the serial */
      if (p > n) break;
      if (kind == 0) {                                         /* integer */
        int32_t v = (int32_t)U32(data + p);
        if (len == 22 && !strncmp(name, "Gtk/EnableAnimations", 20)) *reduced = v == 0;
        else if (len == 20 && !strncmp(name, "Gtk/EnableAnimations", 20)) *reduced = v == 0;
        at = p + 4;
      } else if (kind == 1) {                                  /* string */
        uint32_t sl = U32(data + p);
        const char *s = (const char *)data + p + 4;
        if (len == 13 && !strncmp(name, "Net/ThemeName", 13) && p + 4 + sl <= n) {
          char theme[128];
          size_t m = RK_MIN(sl, sizeof theme - 1);
          memcpy(theme, s, m); theme[m] = 0;
          for (char *c = theme; *c; c++) if (*c >= 'A' && *c <= 'Z') *c = (char)(*c + 32);
          if (strstr(theme, "dark")) *dark = true;
        }
        at = p + 4 + ((sl + 3u) & ~3u);
      } else if (kind == 2) at = p + 8;                        /* colour */
      else break;
    }
    #undef U32
    #undef U16
  }
  XFree(data);
}

const rk_platform rk__platform_native = {
  "x11", x_init, x_shutdown, x_open, x_close, x_show, x_title, x_present, x_move, x_minimize, x_zoom,
  x_wait, x_wake, x_now, x_settings
};

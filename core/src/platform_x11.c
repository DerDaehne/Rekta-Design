/* platform_x11.c: Linux and BSD through Xlib, where no Wayland compositor answers (or with RK_PLATFORM=x11). Pixels go
   up through MIT-SHM (XPutImage where it is missing); text input comes through XIM; the theme and the animation setting
   come from XSETTINGS, which GNOME, KDE and Xfce all publish; Xft.dpi gives the scale, followed when the desktop
   changes it (X11 has one for the whole screen: no desktop publishes one per monitor). Another thread wakes the loop
   through a pipe. */
#include "rk_internal.h"
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <fcntl.h>
#include <locale.h>
#include <poll.h>
#include <time.h>
#include <unistd.h>

typedef struct xwin {
  rk_window *w; Window win; GC gc; XIC ic; int pw, ph; bool maximized;
  float min_w, min_h;                                        /* logical: the hints follow the scale */
  char *preedit; int plen, pcap;                             /* what the input method composes, on the spot (UTF-8) */
  XPoint spot; bool spot_due, composing;                     /* the caret for the input method, sent between compositions */
  XImage *shm_img; XShmSegmentInfo shm;                      /* the frame in memory the X server reads directly */
  Window dnd_source; bool dnd_uris, dnd_taken; rk_point dnd_at;   /* files dragged over it (XDND) */
} xwin;

static Display *dpy;
static int screen;
static XIM im;
static XIMStyle im_style;                                     /* on the spot, over the spot, or nothing */
static XFontSet im_fonts;                                     /* over the spot needs one */
static XIC make_ic(struct xwin *x);
static int wake_pipe[2] = { -1, -1 };
static float dpi_scale = 1;
static bool use_shm, shm_failed;
static Atom A_CLIPBOARD, A_TARGETS, A_RK_PASTE;
static Atom A_XDND_AWARE, A_XDND_ENTER, A_XDND_POSITION, A_XDND_STATUS, A_XDND_LEAVE, A_XDND_DROP, A_XDND_FINISHED;
static Atom A_XDND_SELECTION, A_XDND_COPY, A_XDND_TYPES, A_URI_LIST, A_RK_DROP;
static Window clip_win;                                       /* owns the CLIPBOARD selection while we hold it */
static char *clip_text;
static Atom A_DELETE, A_PROTOCOLS, A_NET_NAME, A_UTF8, A_MOTIF, A_MOVERESIZE, A_STATE, A_MAX_V, A_MAX_H, A_XS_SEL, A_XS_SETTINGS;
static xwin *wins[64];
static int nwins;

static xwin *find(Window win) { for (int i = 0; i < nwins; i++) if (wins[i]->win == win) return wins[i]; return NULL; }

static double x_now(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

/* The scale from Xft.dpi, read from the root window's resources as they are now (Xlib's copy is from the connect). */
static bool scale_fixed;                                       /* RK_SCALE set: kept */
static void read_dpi(void) {
  const char *env = getenv("RK_SCALE");
  if (env && atof(env) > 0) { dpi_scale = (float)atof(env); scale_fixed = true; return; }
  Atom type_got; int format; unsigned long n, after;
  unsigned char *rms = NULL;
  if (XGetWindowProperty(dpy, RootWindow(dpy, screen), XA_RESOURCE_MANAGER, 0, 1 << 20, False, XA_STRING, &type_got, &format,
                         &n, &after, &rms) != Success || !rms) return;
  XrmInitialize();
  XrmDatabase db = XrmGetStringDatabase((char *)rms);
  XFree(rms);
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
  A_CLIPBOARD = XInternAtom(dpy, "CLIPBOARD", False);
  A_TARGETS = XInternAtom(dpy, "TARGETS", False);
  A_RK_PASTE = XInternAtom(dpy, "_REKTA_PASTE", False);
  A_XDND_AWARE = XInternAtom(dpy, "XdndAware", False); A_XDND_ENTER = XInternAtom(dpy, "XdndEnter", False);
  A_XDND_POSITION = XInternAtom(dpy, "XdndPosition", False); A_XDND_STATUS = XInternAtom(dpy, "XdndStatus", False);
  A_XDND_LEAVE = XInternAtom(dpy, "XdndLeave", False); A_XDND_DROP = XInternAtom(dpy, "XdndDrop", False);
  A_XDND_FINISHED = XInternAtom(dpy, "XdndFinished", False); A_XDND_SELECTION = XInternAtom(dpy, "XdndSelection", False);
  A_XDND_COPY = XInternAtom(dpy, "XdndActionCopy", False); A_XDND_TYPES = XInternAtom(dpy, "XdndTypeList", False);
  A_URI_LIST = XInternAtom(dpy, "text/uri-list", False); A_RK_DROP = XInternAtom(dpy, "_REKTA_DROP", False);
  clip_win = XCreateSimpleWindow(dpy, RootWindow(dpy, screen), 0, 0, 1, 1, 0, 0, 0);
  /* XIM needs a locale the input method knows; like GLFW, take the environment's character type if the app left it at C */
  const char *ctype = setlocale(LC_CTYPE, NULL);
  if (!ctype || !strcmp(ctype, "C") || !strcmp(ctype, "POSIX")) setlocale(LC_CTYPE, "");
  XSetLocaleModifiers("");
  im = XOpenIM(dpy, NULL, NULL, NULL);
  im_style = XIMPreeditNothing | XIMStatusNothing;
  XIMStyles *styles = NULL;
  if (im && !XGetIMValues(im, XNQueryInputStyle, &styles, NULL) && styles) {
    static const XIMStyle wanted[] = { XIMPreeditCallbacks | XIMStatusNothing, XIMPreeditPosition | XIMStatusNothing, XIMPreeditNothing | XIMStatusNothing, XIMPreeditNone | XIMStatusNone };
    for (int k = 0, found = 0; k < 4 && !found; k++)
      for (int i = 0; i < styles->count_styles; i++) if (styles->supported_styles[i] == wanted[k]) { im_style = wanted[k]; found = 1; break; }
    XFree(styles);
  }
  const char *noshm = getenv("RK_NO_SHM");
  use_shm = XShmQueryExtension(dpy) && !(noshm && *noshm == '1');
  read_dpi();
  XSelectInput(dpy, RootWindow(dpy, screen), PropertyChangeMask);   /* Xft.dpi changes */
  /* follow XSETTINGS changes: the manager window's property */
  Window mgr = XGetSelectionOwner(dpy, A_XS_SEL);
  if (mgr) XSelectInput(dpy, mgr, PropertyChangeMask);
  return true;
}

static void x_shutdown(void) {
  free(clip_text); clip_text = NULL;
  if (clip_win) { XDestroyWindow(dpy, clip_win); clip_win = 0; }
  if (im) XCloseIM(im);
  if (im_fonts) XFreeFontSet(dpy, im_fonts);
  if (dpy) XCloseDisplay(dpy);
  dpy = NULL; im = NULL; im_fonts = NULL;
  if (wake_pipe[0] >= 0) { close(wake_pipe[0]); close(wake_pipe[1]); wake_pipe[0] = wake_pipe[1] = -1; }
}

static void *x_open(rk_window *w, const rk_window_desc *d) {
  if (nwins >= RK_COUNT(wins)) return NULL;
  xwin *x = rk__alloc(sizeof *x);
  x->w = w;
  x->pw = (int)(d->width * dpi_scale); x->ph = (int)(d->height * dpi_scale);
  XSetWindowAttributes a;
  memset(&a, 0, sizeof a);
  /* no background: the server never fills new area black before we draw it; contents stay put on a resize */
  a.background_pixmap = None;
  a.bit_gravity = NorthWestGravity;
  a.event_mask = ExposureMask | StructureNotifyMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask |
                 PointerMotionMask | EnterWindowMask | LeaveWindowMask | FocusChangeMask;
  x->win = XCreateWindow(dpy, RootWindow(dpy, screen), 0, 0, (unsigned)x->pw, (unsigned)x->ph, 0, CopyFromParent, InputOutput,
                         CopyFromParent, CWBackPixmap | CWBitGravity | CWEventMask, &a);
  XSetWMProtocols(dpy, x->win, &A_DELETE, 1);
  Atom xdnd_version = 5;                                       /* files may be dragged in (XDND 5) */
  XChangeProperty(dpy, x->win, A_XDND_AWARE, XA_ATOM, 32, PropModeReplace, (unsigned char *)&xdnd_version, 1);
  XSizeHints *hints = XAllocSizeHints();
  hints->flags = PMinSize;
  x->min_w = d->min_width; x->min_h = d->min_height;
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
  if (im) x->ic = make_ic(x);
  wins[nwins++] = x;
  rk__window_resized(w, x->pw / dpi_scale, x->ph / dpi_scale, dpi_scale);
  if (d->title) {
    XStoreName(dpy, x->win, d->title);
    XChangeProperty(dpy, x->win, A_NET_NAME, A_UTF8, 8, PropModeReplace, (const unsigned char *)d->title, (int)strlen(d->title));
  }
  return x;
}

static void shm_free(xwin *x) {
  if (!x->shm_img) return;
  XShmDetach(dpy, &x->shm);
  XSync(dpy, False);
  XDestroyImage(x->shm_img);
  shmdt(x->shm.shmaddr);
  x->shm_img = NULL;
}

static int shm_error(Display *d, XErrorEvent *e) { (void)d; (void)e; shm_failed = true; return 0; }

/* A shared image the size of the window; a remote display refuses the attach, and then XPutImage it is. */
static bool shm_ensure(xwin *x, int w, int h) {
  /* a resize step that fits uses the image it has (frames go up as sub-rectangles); growing leaves a quarter spare */
  if (x->shm_img && x->shm_img->width >= w && x->shm_img->height >= h) return true;
  shm_free(x);
  w += w / 4; h += h / 4;
  XImage *img = XShmCreateImage(dpy, DefaultVisual(dpy, screen), (unsigned)DefaultDepth(dpy, screen), ZPixmap, NULL, &x->shm, (unsigned)w, (unsigned)h);
  if (!img) { use_shm = false; return false; }
  x->shm.shmid = shmget(IPC_PRIVATE, (size_t)img->bytes_per_line * h, IPC_CREAT | 0600);
  if (x->shm.shmid < 0) { XDestroyImage(img); use_shm = false; return false; }
  x->shm.shmaddr = img->data = shmat(x->shm.shmid, NULL, 0);
  x->shm.readOnly = False;
  shm_failed = false;
  int (*old)(Display *, XErrorEvent *) = XSetErrorHandler(shm_error);
  XShmAttach(dpy, &x->shm);
  XSync(dpy, False);
  XSetErrorHandler(old);
  shmctl(x->shm.shmid, IPC_RMID, NULL);                        /* freed once both sides let go */
  if (shm_failed) { shmdt(x->shm.shmaddr); XDestroyImage(img); use_shm = false; return false; }
  x->shm_img = img;
  return true;
}

/* On the spot: the input method tells what it composes, a change at a time, and the field shows it at its caret. */
static int pe_start(XIC ic, XPointer client, XPointer call) { (void)ic; (void)call; xwin *x = (xwin *)client; x->plen = 0; x->composing = true; return -1; }

static void pe_send(xwin *x) {
  rk_event e;
  memset(&e, 0, sizeof e);
  e.size = sizeof e; e.type = RK_EVENT_COMPOSE; e.time = rk_app_now(); e.text = x->plen ? x->preedit : "";
  rk__window_event(x->w, &e);
}

static void pe_done(XIC ic, XPointer client, XPointer call) { (void)ic; (void)call; xwin *x = (xwin *)client; x->plen = 0; x->composing = false; pe_send(x); }

static int utf8_put(char *out, unsigned c) {
  if (c < 0x80) { out[0] = (char)c; return 1; }
  if (c < 0x800) { out[0] = (char)(0xc0 | c >> 6); out[1] = (char)(0x80 | (c & 63)); return 2; }
  if (c < 0x10000) { out[0] = (char)(0xe0 | c >> 12); out[1] = (char)(0x80 | (c >> 6 & 63)); out[2] = (char)(0x80 | (c & 63)); return 3; }
  out[0] = (char)(0xf0 | c >> 18); out[1] = (char)(0x80 | (c >> 12 & 63)); out[2] = (char)(0x80 | (c >> 6 & 63)); out[3] = (char)(0x80 | (c & 63)); return 4;
}

static int byte_at(const xwin *x, int chars) {                 /* the byte offset of the chars'th character */
  int i = 0;
  for (; i < x->plen && chars > 0; chars--) { i++; while (i < x->plen && ((unsigned char)x->preedit[i] & 0xc0) == 0x80) i++; }
  return i;
}

static void pe_draw(XIC ic, XPointer client, XPointer call) {
  (void)ic;
  xwin *x = (xwin *)client;
  XIMPreeditDrawCallbackStruct *d = (XIMPreeditDrawCallbackStruct *)call;
  char add[1024];
  int n = 0;
  if (d->text && d->text->length) {
    if (d->text->encoding_is_wchar) {
      for (int i = 0; i < d->text->length && n < (int)sizeof add - 4; i++) n += utf8_put(add + n, (unsigned)d->text->string.wide_char[i]);
    } else if (d->text->string.multi_byte) {                 /* the locale's encoding: UTF-8 where an input method runs today */
      n = (int)strlen(d->text->string.multi_byte);
      if (n > (int)sizeof add) n = (int)sizeof add;
      memcpy(add, d->text->string.multi_byte, (size_t)n);
    }
  }
  int a = byte_at(x, d->chg_first), b = byte_at(x, d->chg_first + d->chg_length);
  x->preedit = rk__grow(x->preedit, &x->pcap, x->plen - (b - a) + n + 1, 1);
  memmove(x->preedit + a + n, x->preedit + b, (size_t)(x->plen - b));
  memcpy(x->preedit + a, add, (size_t)n);
  x->plen += n - (b - a);
  x->preedit[x->plen] = 0;
  pe_send(x);
}

static void pe_caret(XIC ic, XPointer client, XPointer call) { (void)ic; (void)client; (void)call; }   /* the caret stays at the end */

static XIC make_ic(xwin *x) {
  XIC ic = NULL;
  if (im_style & XIMPreeditCallbacks) {
    XIMCallback cb[4] = { { (XPointer)x, (XIMProc)(void (*)(void))pe_start }, { (XPointer)x, (XIMProc)pe_done }, { (XPointer)x, (XIMProc)pe_draw }, { (XPointer)x, (XIMProc)pe_caret } };
    XVaNestedList pe = XVaCreateNestedList(0, XNPreeditStartCallback, &cb[0], XNPreeditDoneCallback, &cb[1], XNPreeditDrawCallback, &cb[2], XNPreeditCaretCallback, &cb[3], NULL);
    ic = XCreateIC(im, XNInputStyle, im_style, XNClientWindow, x->win, XNFocusWindow, x->win, XNPreeditAttributes, pe, NULL);
    XFree(pe);
  } else if (im_style & XIMPreeditPosition) {
    if (!im_fonts) {
      char **missing = NULL, *def = NULL;
      int nmissing = 0;
      im_fonts = XCreateFontSet(dpy, "-*-*-medium-r-normal--14-*-*-*-*-*-*-*,-*-*-*-*-*--*-*-*-*-*-*-*-*,*", &missing, &nmissing, &def);
      if (missing) XFreeStringList(missing);
    }
    XPoint spot = { 0, 0 };
    XVaNestedList pe = XVaCreateNestedList(0, XNSpotLocation, &spot, XNFontSet, im_fonts, NULL);
    if (im_fonts) ic = XCreateIC(im, XNInputStyle, im_style, XNClientWindow, x->win, XNFocusWindow, x->win, XNPreeditAttributes, pe, NULL);
    XFree(pe);
  }
  if (!ic) ic = XCreateIC(im, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, XNClientWindow, x->win, XNFocusWindow, x->win, NULL);
  return ic;
}

/* A field's caret: the input method's window goes beside it; a field that lets go ends what was being composed.
   The caret is only noted here and goes out once the events are done, between compositions: an input method opens
   the next one right after a commit, and a change of preedit attributes inside it makes uim, for one, drop it. The
   candidate window is placed before typing starts, which is when it needs to be. */
static void x_text_input(void *n, bool on, rk_rect caret) {
  xwin *x = n;
  if (!x || !x->ic) return;
  if (!on) {
    if (x->composing) { char *s = Xutf8ResetIC(x->ic); if (s) XFree(s); x->plen = 0; }
    return;
  }
  XPoint spot = { (short)(caret.x * dpi_scale), (short)((caret.y + caret.h) * dpi_scale) };
  if (spot.x != x->spot.x || spot.y != x->spot.y) { x->spot = spot; x->spot_due = true; }
}

static void send_spots(void) {
  if (XEventsQueued(dpy, QueuedAfterFlush)) return;          /* what is on its way may open a composition: after it */
  for (int i = 0; i < nwins; i++) {
    xwin *x = wins[i];
    if (!x->spot_due || !x->ic || x->composing) continue;   /* never inside a composition: uim, for one, loses it */
    x->spot_due = false;
    XVaNestedList pe = XVaCreateNestedList(0, XNSpotLocation, &x->spot, NULL);
    XSetICValues(x->ic, XNPreeditAttributes, pe, NULL);
    XFree(pe);
  }
}

static void x_cursor(void *n, int shape) {
  static Cursor fonts[3];
  static const unsigned glyphs[3] = { 0, XC_xterm, XC_sb_h_double_arrow };
  xwin *x = n;
  if (!x) return;
  if (shape <= RK__CURSOR_ARROW || shape > RK__CURSOR_COL_RESIZE) { XUndefineCursor(dpy, x->win); return; }
  if (!fonts[shape]) fonts[shape] = XCreateFontCursor(dpy, glyphs[shape]);
  XDefineCursor(dpy, x->win, fonts[shape]);
}

static void x_close(void *n) {
  xwin *x = n;
  if (!x) return;
  shm_free(x);
  for (int i = 0; i < nwins; i++) if (wins[i] == x) { wins[i] = wins[--nwins]; break; }
  if (x->ic) XDestroyIC(x->ic);
  free(x->preedit);
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

/* Only the changed part goes up. The frame buffer is 0xAARRGGBB in memory order B, G, R, A: a 24-bit TrueColor
   visual's layout. With MIT-SHM the server reads the pixels from shared memory; we wait for it (XSync) before the
   next frame writes there. */
/* Each changed part goes up on its own; one round trip for all of them. */
static void x_present(void *n, const rk_bitmap *fb, const rk_irect *rects, int nrects) {
  xwin *x = n;
  if (!x || !fb) return;
  bool shm = use_shm && shm_ensure(x, fb->w, fb->h);
  XImage *img = shm ? NULL : XCreateImage(dpy, DefaultVisual(dpy, screen), (unsigned)DefaultDepth(dpy, screen), ZPixmap, 0, (char *)fb->px,
                                          (unsigned)fb->w, (unsigned)fb->h, 32, fb->stride * 4);
  if (!shm && !img) return;
  for (int i = 0; i < nrects; i++) {
    rk_irect r = rk__irect_meet(rects[i], (rk_irect){ 0, 0, fb->w, fb->h });
    if (rk__irect_empty(r)) continue;
    int w = r.x1 - r.x0, h = r.y1 - r.y0;
    if (shm) {
      for (int y = r.y0; y < r.y1; y++)
        memcpy(x->shm_img->data + (size_t)y * x->shm_img->bytes_per_line + (size_t)r.x0 * 4, fb->px + (size_t)y * fb->stride + r.x0, (size_t)w * 4);
      XShmPutImage(dpy, x->win, x->gc, x->shm_img, r.x0, r.y0, r.x0, r.y0, (unsigned)w, (unsigned)h, False);
    } else XPutImage(dpy, x->win, x->gc, img, r.x0, r.y0, r.x0, r.y0, (unsigned)w, (unsigned)h);
  }
  if (img) { img->data = NULL; XDestroyImage(img); XFlush(dpy); }
  else XSync(dpy, False);                                        /* the shared image may be written again */
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
  if (k >= XK_F1 && k <= XK_F24) return (rk_key)(RK_KEY_F1 + (k - XK_F1));
  if (k == XK_Insert) return RK_KEY_INSERT;
  if (k >= XK_a && k <= XK_z) return (uint32_t)('A' + (k - XK_a));
  if (k >= XK_A && k <= XK_Z) return (uint32_t)k;
  if (k >= XK_0 && k <= XK_9) return (uint32_t)k;
  if (k < 0x80 && k > 0x20) return (uint32_t)k;
  return RK_KEY_NONE;
}

static uint32_t mods_of(unsigned s) {
  return ((s & ShiftMask) ? RK_MOD_SHIFT : 0) | ((s & ControlMask) ? RK_MOD_CTRL : 0) | ((s & Mod1Mask) ? RK_MOD_ALT : 0) | ((s & Mod4Mask) ? RK_MOD_SUPER : 0);
}

/* The clipboard: we own CLIPBOARD while we hold copied text, and hand it to whoever asks, as UTF-8. */
static void clip_request(const XSelectionRequestEvent *r) {
  XSelectionEvent reply = { SelectionNotify, 0, True, r->display, r->requestor, r->selection, r->target, None, r->time };
  Atom prop = r->property ? r->property : r->target;
  if (r->target == A_TARGETS) {
    Atom targets[] = { A_TARGETS, A_UTF8, XA_STRING };
    XChangeProperty(dpy, r->requestor, prop, XA_ATOM, 32, PropModeReplace, (unsigned char *)targets, 3);
    reply.property = prop;
  } else if ((r->target == A_UTF8 || r->target == XA_STRING) && clip_text) {
    XChangeProperty(dpy, r->requestor, prop, r->target, 8, PropModeReplace, (unsigned char *)clip_text, (int)strlen(clip_text));
    reply.property = prop;
  }
  XSendEvent(dpy, r->requestor, False, 0, (XEvent *)&reply);
  XFlush(dpy);
}

static void x_clip_set(const char *utf8) {
  free(clip_text);
  clip_text = rk__strdup(utf8);
  XSetSelectionOwner(dpy, A_CLIPBOARD, clip_win, CurrentTime);
  XFlush(dpy);
}

static Bool is_paste_reply(Display *d, XEvent *e, XPointer arg) { (void)d; (void)arg; return e->type == SelectionNotify && e->xselection.requestor == clip_win; }

/* Asks the owner for UTF-8 and waits for it, a quarter of a second at most; our own text needs no asking. */
static char *x_clip_get(void) {
  Window owner = XGetSelectionOwner(dpy, A_CLIPBOARD);
  if (owner == clip_win) return rk__strdup(clip_text);
  if (owner == None) return NULL;
  XConvertSelection(dpy, A_CLIPBOARD, A_UTF8, A_RK_PASTE, clip_win, CurrentTime);
  XFlush(dpy);
  XEvent ev;
  double until = x_now() + 250;
  while (!XCheckIfEvent(dpy, &ev, is_paste_reply, NULL)) {
    if (x_now() > until) return NULL;
    struct pollfd p = { ConnectionNumber(dpy), POLLIN, 0 };
    poll(&p, 1, 10);
  }
  if (ev.xselection.property == None) return NULL;
  Atom type; int format; unsigned long n, after; unsigned char *data = NULL;
  char *out = NULL;
  if (XGetWindowProperty(dpy, clip_win, A_RK_PASTE, 0, 1 << 20, True, AnyPropertyType, &type, &format, &n, &after, &data) == Success && data && format == 8)
    { out = rk__alloc(n + 1); memcpy(out, data, n); }
  if (data) XFree(data);
  return out;
}

/* Files dragged in, XDND: Enter says what the source offers (we take text/uri-list), each Position is answered with
   a Status that takes them or not, as the view under the pointer says; Drop asks for the list, its SelectionNotify
   hands the files to the window, Finished tells the source. */
static void xdnd_reply(xwin *x, Atom type, long l1, long l2) {
  XEvent r;
  memset(&r, 0, sizeof r);
  r.xclient.type = ClientMessage; r.xclient.display = dpy; r.xclient.window = x->dnd_source;
  r.xclient.message_type = type; r.xclient.format = 32;
  r.xclient.data.l[0] = (long)x->win; r.xclient.data.l[1] = l1;
  if (type == A_XDND_STATUS) r.xclient.data.l[4] = l2; else r.xclient.data.l[2] = l2;
  XSendEvent(dpy, x->dnd_source, False, NoEventMask, &r);
  XFlush(dpy);
}

static bool xdnd_offers_uris(const XClientMessageEvent *m) {
  Window src = (Window)m->data.l[0];
  if (!(m->data.l[1] & 1)) return (Atom)m->data.l[2] == A_URI_LIST || (Atom)m->data.l[3] == A_URI_LIST || (Atom)m->data.l[4] == A_URI_LIST;
  Atom type; int format; unsigned long n = 0, after; unsigned char *data = NULL;   /* more than three: the list */
  bool found = false;
  if (XGetWindowProperty(dpy, src, A_XDND_TYPES, 0, 1024, False, XA_ATOM, &type, &format, &n, &after, &data) == Success && data) {
    for (unsigned long i = 0; i < n; i++) if (((Atom *)data)[i] == A_URI_LIST) found = true;
  }
  if (data) XFree(data);
  return found;
}

static void xdnd_message(xwin *x, const XClientMessageEvent *m) {
  rk_event e;
  memset(&e, 0, sizeof e);
  e.size = sizeof e; e.time = rk_app_now();
  if (m->message_type == A_XDND_ENTER) {
    x->dnd_source = (Window)m->data.l[0]; x->dnd_uris = xdnd_offers_uris(m); x->dnd_taken = false;
    return;
  }
  if ((Window)m->data.l[0] != x->dnd_source) return;
  if (m->message_type == A_XDND_POSITION) {
    int rx = (int)((unsigned long)m->data.l[2] >> 16), ry = (int)(m->data.l[2] & 0xffff), wx = 0, wy = 0;
    Window child;
    XTranslateCoordinates(dpy, RootWindow(dpy, screen), x->win, rx, ry, &wx, &wy, &child);
    x->dnd_at = (rk_point){ wx / x->w->scale, wy / x->w->scale };
    e.type = RK_EVENT_DRAG_MOVE; e.position = x->dnd_at;
    x->dnd_taken = x->dnd_uris && rk__window_drag(x->w, &e);
    xdnd_reply(x, A_XDND_STATUS, x->dnd_taken ? 3 : 2, x->dnd_taken ? (long)A_XDND_COPY : (long)None);   /* 2: every move */
    rk__app_request_frame();
  } else if (m->message_type == A_XDND_LEAVE) {
    e.type = RK_EVENT_DRAG_LEAVE;
    rk__window_drag(x->w, &e);
    x->dnd_source = None;
    rk__app_request_frame();
  } else if (m->message_type == A_XDND_DROP) {
    if (!x->dnd_taken) {
      e.type = RK_EVENT_DRAG_LEAVE;
      rk__window_drag(x->w, &e);
      xdnd_reply(x, A_XDND_FINISHED, 0, None);
      x->dnd_source = None;
      return;
    }
    XConvertSelection(dpy, A_XDND_SELECTION, A_URI_LIST, A_RK_DROP, x->win, (Time)m->data.l[2]);
    XFlush(dpy);
  }
}

static void xdnd_selection(xwin *x, const XSelectionEvent *s) {
  char **paths = NULL;
  int n = 0;
  if (s->property == A_RK_DROP) {
    Atom type; int format; unsigned long len = 0, after; unsigned char *data = NULL;
    if (XGetWindowProperty(dpy, x->win, A_RK_DROP, 0, 1 << 24, True, AnyPropertyType, &type, &format, &len, &after, &data) == Success && data)
      n = rk__uri_list_paths((const char *)data, len, &paths);
    if (data) XFree(data);
  }
  rk_event e;
  memset(&e, 0, sizeof e);
  e.size = sizeof e; e.time = rk_app_now(); e.type = RK_EVENT_DROP; e.position = x->dnd_at;
  e.files = (const char *const *)paths; e.nfiles = n;
  bool took = n > 0 && rk__window_drag(x->w, &e);
  if (n == 0) { e.type = RK_EVENT_DRAG_LEAVE; rk__window_drag(x->w, &e); }
  rk__free_paths(paths, n);
  xdnd_reply(x, A_XDND_FINISHED, took ? 1 : 0, took ? (long)A_XDND_COPY : (long)None);
  x->dnd_source = None;
  rk__app_request_frame();
}

/* The desktop changed the scale: each window keeps its logical size, in the new number of pixels. */
static void rescale(void) {
  float was = dpi_scale;
  if (scale_fixed) return;
  read_dpi();
  if (dpi_scale == was) return;
  for (int i = 0; i < nwins; i++) {
    xwin *x = wins[i];
    float lw = x->pw / was, lh = x->ph / was;
    XSizeHints *hints = XAllocSizeHints();
    hints->flags = PMinSize;
    hints->min_width = (int)(x->min_w * dpi_scale); hints->min_height = (int)(x->min_h * dpi_scale);
    XSetWMNormalHints(dpy, x->win, hints);
    XFree(hints);
    if (!x->maximized) {                                       /* maximized, the pixels stay and it holds more */
      x->pw = (int)lroundf(lw * dpi_scale); x->ph = (int)lroundf(lh * dpi_scale);
      XResizeWindow(dpy, x->win, (unsigned)x->pw, (unsigned)x->ph);
    }
    rk__window_resized(x->w, x->pw / dpi_scale, x->ph / dpi_scale, dpi_scale);
  }
  XFlush(dpy);
  rk__app_request_frame();
}

static void handle(XEvent *ev) {
  if (ev->type == PropertyNotify && ev->xproperty.atom == A_XS_SETTINGS) { rk__app_settings_changed(); return; }
  if (ev->type == PropertyNotify && ev->xproperty.atom == XA_RESOURCE_MANAGER && ev->xproperty.window == RootWindow(dpy, screen)) { rescale(); return; }
  if (ev->type == SelectionRequest) { clip_request(&ev->xselectionrequest); return; }
  if (ev->type == SelectionClear && ev->xselectionclear.window == clip_win) { free(clip_text); clip_text = NULL; return; }
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
        /* the new size, drawn at once, unless the window manager has already sent the next one: then that one */
        XEvent next;
        if (XCheckTypedWindowEvent(dpy, x->win, ConfigureNotify, &next)) XPutBackEvent(dpy, &next);
        else rk__window_frame(w, rk_app_now());
      }
      break;
    case Expose: {
      rk_rect r = { ev->xexpose.x / k, ev->xexpose.y / k, ev->xexpose.width / k, ev->xexpose.height / k };
      rk__window_damage(w, r);
      if (ev->xexpose.count == 0) rk__window_frame(w, rk_app_now());
      break;
    }
    case ClientMessage:
      if (ev->xclient.message_type == A_PROTOCOLS && (Atom)ev->xclient.data.l[0] == A_DELETE) rk__window_close_request(w);
      else xdnd_message(x, &ev->xclient);
      break;
    case SelectionNotify:
      if (ev->xselection.selection == A_XDND_SELECTION) xdnd_selection(x, &ev->xselection);
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
  send_spots();
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

static void x_resize(void *n, float w, float h) {
  xwin *x = n;
  if (x) { XResizeWindow(dpy, x->win, (unsigned)lroundf(w * dpi_scale), (unsigned)lroundf(h * dpi_scale)); XFlush(dpy); }
}

const rk_platform rk__platform_native = {
  "x11", x_init, x_shutdown, x_open, x_close, x_show, x_title, x_present, x_move, x_minimize, x_zoom,
  x_wait, x_wake, x_now, x_settings, x_resize, NULL, NULL, NULL, x_clip_set, x_clip_get, x_text_input, x_cursor, rk__desktop_choose_files, rk__desktop_notify
};

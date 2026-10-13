/* xdnd_send.c: drags files onto the window named argv[1] as a file manager would (XDND 5) and says what came back. */
#define _DEFAULT_SOURCE
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static Display *dpy;
static Window find(Window w, const char *name) {
  char *n = NULL;
  if (XFetchName(dpy, w, &n) && n) { int same = !strcmp(n, name); XFree(n); if (same) return w; }
  Window root, parent, *kids = NULL; unsigned nk = 0;
  if (!XQueryTree(dpy, w, &root, &parent, &kids, &nk)) return 0;
  Window found = 0;
  for (unsigned i = 0; i < nk && !found; i++) found = find(kids[i], name);
  if (kids) XFree(kids);
  return found;
}
static void send(Window to, Atom type, long l1, long l2, long l3, long l4, Window src) {
  XEvent e; memset(&e, 0, sizeof e);
  e.xclient.type = ClientMessage; e.xclient.window = to; e.xclient.message_type = type; e.xclient.format = 32;
  e.xclient.data.l[0] = (long)src; e.xclient.data.l[1] = l1; e.xclient.data.l[2] = l2; e.xclient.data.l[3] = l3; e.xclient.data.l[4] = l4;
  XSendEvent(dpy, to, False, NoEventMask, &e); XFlush(dpy);
}
static int wait_for(Window src, Atom type, XEvent *out, const char *list, Atom uri) {
  for (int tries = 0; tries < 400; tries++) {
    while (XPending(dpy)) {
      XEvent e; XNextEvent(dpy, &e);
      if (e.type == SelectionRequest) {                          /* the target asks for the files */
        XSelectionRequestEvent *r = &e.xselectionrequest;
        XSelectionEvent s = { SelectionNotify, 0, True, dpy, r->requestor, r->selection, r->target, None, r->time };
        if (r->target == uri) { XChangeProperty(dpy, r->requestor, r->property, uri, 8, PropModeReplace, (const unsigned char *)list, (int)strlen(list)); s.property = r->property; }
        XSendEvent(dpy, r->requestor, False, NoEventMask, (XEvent *)&s); XFlush(dpy);
      } else if (e.type == ClientMessage && e.xclient.message_type == type) { *out = e; return 1; }
    }
    usleep(10000);
  }
  (void)src;
  return 0;
}

int main(int argc, char **argv) {
  if (argc < 4) { fprintf(stderr, "usage: xdnd_send window-name x y [uri-list]\n"); return 2; }
  dpy = XOpenDisplay(NULL);
  if (!dpy) return 2;
  Window root = DefaultRootWindow(dpy), target = 0;
  for (int i = 0; i < 300 && !target; i++) { target = find(root, argv[1]); if (!target) usleep(20000); }
  if (!target) { fprintf(stderr, "no window %s\n", argv[1]); return 2; }
  Window src = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
  Atom enter = XInternAtom(dpy, "XdndEnter", False), pos = XInternAtom(dpy, "XdndPosition", False), status = XInternAtom(dpy, "XdndStatus", False);
  Atom drop = XInternAtom(dpy, "XdndDrop", False), finished = XInternAtom(dpy, "XdndFinished", False), sel = XInternAtom(dpy, "XdndSelection", False);
  Atom copy = XInternAtom(dpy, "XdndActionCopy", False), uri = XInternAtom(dpy, "text/uri-list", False);
  const char *list = argc > 4 ? argv[4] : "file:///tmp/a%20b.png\r\nfile:///tmp/c.txt\r\n";
  XSetSelectionOwner(dpy, sel, src, CurrentTime);
  int x, y; Window child;
  XTranslateCoordinates(dpy, target, root, atoi(argv[2]), atoi(argv[3]), &x, &y, &child);
  send(target, enter, 5L << 24, (long)uri, 0, 0, src);
  send(target, pos, 0, ((long)x << 16) | y, CurrentTime, (long)copy, src);
  XEvent st;
  if (!wait_for(src, status, &st, list, uri)) { printf("no status\n"); return 1; }
  int accepted = st.xclient.data.l[1] & 1;
  printf("status %s\n", accepted ? "accepted" : "refused"); fflush(stdout);
  send(target, drop, 0, CurrentTime, 0, 0, src);
  XEvent fin;
  if (!wait_for(src, finished, &fin, list, uri)) { printf("no finished\n"); return 1; }
  printf("finished %s\n", fin.xclient.data.l[1] & 1 ? "taken" : "not taken");
  return 0;
}

/* fake_portal.c: a stand-in for xdg-desktop-portal's FileChooser on a private session bus, for the chooser test. It
   prints the options it was given (multiple, the filter patterns) and answers OpenFile with the URIs in argv[1..]. */
#define _DEFAULT_SOURCE
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct DBusConnection DBusConnection;
typedef struct DBusMessage DBusMessage;
typedef struct { const char *name, *message; unsigned bits; void *pad; char room[32]; } DBusError;
typedef struct { void *d1, *d2; uint32_t d3; int d4[9]; void *p2, *p3; char room[64]; } DBusMessageIter;

static void *h;
#define FN(ret, name, ...) static ret (*name)(__VA_ARGS__)
FN(void, dbus_error_init, DBusError *);
FN(DBusConnection *, dbus_bus_get, int, DBusError *);
FN(int, dbus_bus_request_name, DBusConnection *, const char *, unsigned, DBusError *);
FN(int, dbus_connection_read_write, DBusConnection *, int);
FN(DBusMessage *, dbus_connection_pop_message, DBusConnection *);
FN(int, dbus_message_is_method_call, DBusMessage *, const char *, const char *);
FN(const char *, dbus_message_get_sender, DBusMessage *);
FN(DBusMessage *, dbus_message_new_method_return, DBusMessage *);
FN(DBusMessage *, dbus_message_new_signal, const char *, const char *, const char *);
FN(int, dbus_connection_send, DBusConnection *, DBusMessage *, uint32_t *);
FN(void, dbus_connection_flush, DBusConnection *);
FN(void, dbus_message_unref, DBusMessage *);
FN(int, dbus_message_iter_init, DBusMessage *, DBusMessageIter *);
FN(void, dbus_message_iter_init_append, DBusMessage *, DBusMessageIter *);
FN(int, dbus_message_iter_get_arg_type, DBusMessageIter *);
FN(void, dbus_message_iter_get_basic, DBusMessageIter *, void *);
FN(int, dbus_message_iter_next, DBusMessageIter *);
FN(void, dbus_message_iter_recurse, DBusMessageIter *, DBusMessageIter *);
FN(int, dbus_message_iter_append_basic, DBusMessageIter *, int, const void *);
FN(int, dbus_message_iter_open_container, DBusMessageIter *, int, const char *, DBusMessageIter *);
FN(int, dbus_message_iter_close_container, DBusMessageIter *, DBusMessageIter *);
#define LOAD(name) (*(void **)&name = dlsym(h, #name))

/* every string inside a value, depth first: the filter's patterns come out in order */
static void strings(DBusMessageIter *it, int depth) {
  for (int t; (t = dbus_message_iter_get_arg_type(it)); dbus_message_iter_next(it)) {
    if (t == 's') { const char *s; dbus_message_iter_get_basic(it, &s); printf(" %s", s); }
    else if (t == 'a' || t == 'r' || t == 'v' || t == 'e') { DBusMessageIter sub; dbus_message_iter_recurse(it, &sub); strings(&sub, depth + 1); }
  }
}

int main(int argc, char **argv) {
  h = dlopen("libdbus-1.so.3", RTLD_NOW);
  if (!h) return 2;
  LOAD(dbus_error_init); LOAD(dbus_bus_get); LOAD(dbus_bus_request_name); LOAD(dbus_connection_read_write);
  LOAD(dbus_connection_pop_message); LOAD(dbus_message_is_method_call); LOAD(dbus_message_get_sender);
  LOAD(dbus_message_new_method_return); LOAD(dbus_message_new_signal); LOAD(dbus_connection_send); LOAD(dbus_connection_flush);
  LOAD(dbus_message_unref); LOAD(dbus_message_iter_init); LOAD(dbus_message_iter_init_append); LOAD(dbus_message_iter_get_arg_type);
  LOAD(dbus_message_iter_get_basic); LOAD(dbus_message_iter_next); LOAD(dbus_message_iter_recurse); LOAD(dbus_message_iter_append_basic);
  LOAD(dbus_message_iter_open_container); LOAD(dbus_message_iter_close_container);
  DBusError err;
  dbus_error_init(&err);
  DBusConnection *c = dbus_bus_get(0, &err);
  if (!c) { fprintf(stderr, "no bus: %s\n", err.message ? err.message : "?"); return 2; }
  int got = dbus_bus_request_name(c, "org.freedesktop.portal.Desktop", 4, &err);
  if (got != 1) { fprintf(stderr, "no name (%d): %s\n", got, err.message ? err.message : "?"); return 2; }
  printf("portal ready\n"); fflush(stdout);
  while (dbus_connection_read_write(c, 10000)) {
    DBusMessage *m;
    while ((m = dbus_connection_pop_message(c))) {
      if (!dbus_message_is_method_call(m, "org.freedesktop.portal.FileChooser", "OpenFile")) { dbus_message_unref(m); continue; }
      DBusMessageIter it, dict;
      const char *parent = "", *title = "", *token = "";
      int multiple = 0;
      dbus_message_iter_init(m, &it);
      dbus_message_iter_get_basic(&it, &parent); dbus_message_iter_next(&it);
      dbus_message_iter_get_basic(&it, &title); dbus_message_iter_next(&it);
      printf("title %s\n", title);
      dbus_message_iter_recurse(&it, &dict);
      for (; dbus_message_iter_get_arg_type(&dict) == 'e'; dbus_message_iter_next(&dict)) {
        DBusMessageIter entry, var;
        const char *key;
        dbus_message_iter_recurse(&dict, &entry);
        dbus_message_iter_get_basic(&entry, &key); dbus_message_iter_next(&entry);
        dbus_message_iter_recurse(&entry, &var);
        if (!strcmp(key, "handle_token")) dbus_message_iter_get_basic(&var, &token);
        else if (!strcmp(key, "multiple")) dbus_message_iter_get_basic(&var, &multiple);
        else if (!strcmp(key, "filters")) { printf("filters"); strings(&var, 0); printf("\n"); }
      }
      printf("multiple %d\n", multiple);
      char sender[128] = "", path[256];
      const char *u = dbus_message_get_sender(m);
      for (int i = 0; u && u[i + 1] && i < 126; i++) sender[i] = u[i + 1] == '.' ? '_' : u[i + 1];
      snprintf(path, sizeof path, "/org/freedesktop/portal/desktop/request/%s/%s", sender, token);
      DBusMessage *r = dbus_message_new_method_return(m);
      DBusMessageIter ri;
      const char *p = path;
      dbus_message_iter_init_append(r, &ri);
      dbus_message_iter_append_basic(&ri, 'o', &p);
      dbus_connection_send(c, r, NULL);
      dbus_message_unref(r);
      DBusMessage *s = dbus_message_new_signal(path, "org.freedesktop.portal.Request", "Response");
      DBusMessageIter si, results, entry, var, uris;
      uint32_t code = 0;
      const char *key = "uris";
      dbus_message_iter_init_append(s, &si);
      dbus_message_iter_append_basic(&si, 'u', &code);
      dbus_message_iter_open_container(&si, 'a', "{sv}", &results);
      dbus_message_iter_open_container(&results, 'e', NULL, &entry);
      dbus_message_iter_append_basic(&entry, 's', &key);
      dbus_message_iter_open_container(&entry, 'v', "as", &var);
      dbus_message_iter_open_container(&var, 'a', "s", &uris);
      for (int i = 1; i < argc; i++) { const char *a = argv[i]; dbus_message_iter_append_basic(&uris, 's', &a); }
      dbus_message_iter_close_container(&var, &uris);
      dbus_message_iter_close_container(&entry, &var);
      dbus_message_iter_close_container(&results, &entry);
      dbus_message_iter_close_container(&si, &results);
      dbus_connection_send(c, s, NULL);
      dbus_connection_flush(c);
      dbus_message_unref(s);
      dbus_message_unref(m);
      fflush(stdout);
      return 0;
    }
  }
  return 1;
}

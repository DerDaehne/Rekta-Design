/* desktop_linux.c: what a Linux desktop offers apps, for X11 and Wayland alike. The file chooser is the desktop
   portal's (org.freedesktop.portal.FileChooser, which sandboxed apps may use too), asked over D-Bus; where no portal
   answers, zenity or kdialog. Each runs on a thread of its own, since a person may take minutes, and the answer comes
   back through rk_app_post. libdbus is looked up at run time, so the library links against nothing new. */
#include "rk_internal.h"
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

/* libdbus, as much of it as the portal needs; the structs are larger than libdbus's, which only fills its own part */
typedef struct DBusConnection DBusConnection;
typedef struct DBusMessage DBusMessage;
typedef struct { const char *name, *message; unsigned bits; void *pad; char room[32]; } DBusError;
typedef struct { void *d1, *d2; uint32_t d3; int d4[9]; void *p2, *p3; char room[64]; } DBusMessageIter;
enum { DBUS_SESSION = 0, T_INVALID = 0, T_STRING = 's', T_BOOL = 'b', T_UINT32 = 'u', T_ARRAY = 'a', T_VARIANT = 'v',
       T_DICT = 'e', T_STRUCT = 'r', T_PATH = 'o' };

static struct {
  bool tried, ok;
  int (*threads_init)(void);
  void (*error_init)(DBusError *); void (*error_free)(DBusError *); int (*error_is_set)(const DBusError *);
  DBusConnection *(*bus_get_private)(int, DBusError *); const char *(*bus_unique_name)(DBusConnection *);
  void (*bus_add_match)(DBusConnection *, const char *, DBusError *);
  void (*conn_close)(DBusConnection *); void (*conn_unref)(DBusConnection *);
  int (*conn_read_write)(DBusConnection *, int); DBusMessage *(*conn_pop)(DBusConnection *);
  DBusMessage *(*send_block)(DBusConnection *, DBusMessage *, int, DBusError *);
  DBusMessage *(*new_call)(const char *, const char *, const char *, const char *); void (*msg_unref)(DBusMessage *);
  int (*is_signal)(DBusMessage *, const char *, const char *); const char *(*msg_path)(DBusMessage *);
  void (*iter_append)(DBusMessage *, DBusMessageIter *); int (*iter_init)(DBusMessage *, DBusMessageIter *);
  int (*append_basic)(DBusMessageIter *, int, const void *);
  int (*open)(DBusMessageIter *, int, const char *, DBusMessageIter *); int (*close)(DBusMessageIter *, DBusMessageIter *);
  int (*arg_type)(DBusMessageIter *); void (*get_basic)(DBusMessageIter *, void *); int (*next)(DBusMessageIter *);
  void (*recurse)(DBusMessageIter *, DBusMessageIter *);
} D;

static bool dbus_load(void) {
  if (D.tried) return D.ok;
  D.tried = true;
  void *h = dlopen("libdbus-1.so.3", RTLD_LAZY | RTLD_LOCAL);
  if (!h) return false;
#define F(field, name) (*(void **)&D.field = dlsym(h, name), D.field != NULL)
  D.ok = F(threads_init, "dbus_threads_init_default") && F(error_init, "dbus_error_init") && F(error_free, "dbus_error_free") &&
         F(error_is_set, "dbus_error_is_set") && F(bus_get_private, "dbus_bus_get_private") && F(bus_unique_name, "dbus_bus_get_unique_name") &&
         F(bus_add_match, "dbus_bus_add_match") && F(conn_close, "dbus_connection_close") && F(conn_unref, "dbus_connection_unref") &&
         F(conn_read_write, "dbus_connection_read_write") && F(conn_pop, "dbus_connection_pop_message") &&
         F(send_block, "dbus_connection_send_with_reply_and_block") && F(new_call, "dbus_message_new_method_call") &&
         F(msg_unref, "dbus_message_unref") && F(is_signal, "dbus_message_is_signal") && F(msg_path, "dbus_message_get_path") &&
         F(iter_append, "dbus_message_iter_init_append") && F(iter_init, "dbus_message_iter_init") &&
         F(append_basic, "dbus_message_iter_append_basic") && F(open, "dbus_message_iter_open_container") &&
         F(close, "dbus_message_iter_close_container") && F(arg_type, "dbus_message_iter_get_arg_type") &&
         F(get_basic, "dbus_message_iter_get_basic") && F(next, "dbus_message_iter_next") && F(recurse, "dbus_message_iter_recurse");
#undef F
  if (D.ok) D.threads_init();
  return D.ok;
}

/* One question to the chooser and its answer, carried from the UI thread to the asking thread and back. */
typedef struct ask {
  char *accept; bool multiple;
  void (*done)(void *ctx, const char *const *files, int n); void *ctx;
  char **files; int n; bool answered;
} ask;

static void ask_answer(void *user) {
  ask *a = user;
  a->done(a->ctx, (const char *const *)a->files, a->n);
  rk__free_paths(a->files, a->n);
  free(a->accept); free(a);
}

static void add_file(ask *a, const char *path, size_t len) {
  if (!len) return;
  char *p = rk__alloc(len + 1);
  memcpy(p, path, len);
  a->files = realloc(a->files, (size_t)(a->n + 1) * sizeof *a->files);
  if (!a->files) { free(p); a->n = 0; return; }
  a->files[a->n++] = p;
}

static void append_string_variant(DBusMessageIter *dict, const char *key, int type, const void *value) {
  DBusMessageIter entry, var;
  char sig[2] = { (char)type, 0 };
  D.open(dict, T_DICT, NULL, &entry);
  D.append_basic(&entry, T_STRING, &key);
  D.open(&entry, T_VARIANT, sig, &var);
  D.append_basic(&var, type, value);
  D.close(&entry, &var);
  D.close(dict, &entry);
}

/* The portal: OpenFile with a handle token, then the Response signal on the request it names. False when no portal
   could be asked (no session bus, no portal): then another chooser is tried. */
static bool portal_choose(ask *a) {
  if (!dbus_load()) return false;
  DBusError err;
  D.error_init(&err);
  DBusConnection *c = D.bus_get_private(DBUS_SESSION, &err);
  if (!c) { D.error_free(&err); return false; }
  char token[32], path[256], match[512];
  snprintf(token, sizeof token, "rekta%ld", (long)getpid());
  const char *unique = D.bus_unique_name(c);
  char sender[128] = "";
  for (int i = 0; unique && unique[i + 1] && i < 126; i++) sender[i] = unique[i + 1] == '.' ? '_' : unique[i + 1];   /* ":1.42" -> "1_42" */
  snprintf(path, sizeof path, "/org/freedesktop/portal/desktop/request/%s/%s", sender, token);
  snprintf(match, sizeof match, "type='signal',interface='org.freedesktop.portal.Request',member='Response',path='%s'", path);
  D.bus_add_match(c, match, &err);
  DBusMessage *m = D.new_call("org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop", "org.freedesktop.portal.FileChooser", "OpenFile");
  DBusMessageIter it, dict;
  D.iter_append(m, &it);
  const char *parent = "", *title = a->multiple ? "Choose files" : "Choose a file", *tok = token;
  D.append_basic(&it, T_STRING, &parent);
  D.append_basic(&it, T_STRING, &title);
  D.open(&it, T_ARRAY, "{sv}", &dict);
  append_string_variant(&dict, "handle_token", T_STRING, &tok);
  int multiple = a->multiple;
  append_string_variant(&dict, "multiple", T_BOOL, &multiple);
  char **globs = NULL;
  int ng = rk__accept_globs(a->accept, &globs);
  if (ng) {                                                /* filters a(sa(us)): one filter with every pattern */
    DBusMessageIter entry, var, filters, filter, patterns, pat;
    const char *key = "filters", *name = "Accepted files";
    D.open(&dict, T_DICT, NULL, &entry);
    D.append_basic(&entry, T_STRING, &key);
    D.open(&entry, T_VARIANT, "a(sa(us))", &var);
    D.open(&var, T_ARRAY, "(sa(us))", &filters);
    D.open(&filters, T_STRUCT, NULL, &filter);
    D.append_basic(&filter, T_STRING, &name);
    D.open(&filter, T_ARRAY, "(us)", &patterns);
    for (int i = 0; i < ng; i++) {
      uint32_t glob = 0;
      const char *g = globs[i];
      D.open(&patterns, T_STRUCT, NULL, &pat);
      D.append_basic(&pat, T_UINT32, &glob);
      D.append_basic(&pat, T_STRING, &g);
      D.close(&patterns, &pat);
    }
    D.close(&filter, &patterns);
    D.close(&filters, &filter);
    D.close(&var, &filters);
    D.close(&entry, &var);
    D.close(&dict, &entry);
  }
  rk__free_paths(globs, ng);
  D.close(&it, &dict);
  DBusMessage *reply = D.send_block(c, m, 5000, &err);
  D.msg_unref(m);
  bool asked = reply != NULL;
  if (reply) D.msg_unref(reply);
  D.error_free(&err);
  while (asked && !a->answered && D.conn_read_write(c, 1000)) {   /* the person chooses; the Response says what */
    DBusMessage *s;
    while ((s = D.conn_pop(c))) {
      if (D.is_signal(s, "org.freedesktop.portal.Request", "Response") && D.msg_path(s) && !strcmp(D.msg_path(s), path)) {
        DBusMessageIter r, results;
        uint32_t code = 2;
        if (D.iter_init(s, &r) && D.arg_type(&r) == T_UINT32) { D.get_basic(&r, &code); D.next(&r); }
        if (code == 0 && D.arg_type(&r) == T_ARRAY) {
          D.recurse(&r, &results);
          for (; D.arg_type(&results) == T_DICT; D.next(&results)) {
            DBusMessageIter entry, var, uris;
            const char *key = NULL;
            D.recurse(&results, &entry);
            D.get_basic(&entry, &key);
            if (!key || strcmp(key, "uris")) continue;
            D.next(&entry); D.recurse(&entry, &var);
            if (D.arg_type(&var) != T_ARRAY) continue;
            D.recurse(&var, &uris);
            for (; D.arg_type(&uris) == T_STRING; D.next(&uris)) {
              const char *u = NULL;
              D.get_basic(&uris, &u);
              char **paths = NULL;
              int k = u ? rk__uri_list_paths(u, strlen(u), &paths) : 0;
              for (int i = 0; i < k; i++) add_file(a, paths[i], strlen(paths[i]));
              rk__free_paths(paths, k);
            }
          }
        }
        a->answered = true;
      }
      D.msg_unref(s);
    }
  }
  D.conn_close(c);
  D.conn_unref(c);
  return asked && a->answered;
}

/* zenity or kdialog: the paths one a line on its standard output; a cancel leaves it empty */
static bool tool_choose(ask *a) {
  char **globs = NULL;
  int ng = rk__accept_globs(a->accept, &globs);
  char pats[1024] = "";
  for (int i = 0; i < ng; i++) {                           /* patterns go into a shell line: only safe characters */
    bool safe = true;
    for (const char *p = globs[i]; *p; p++) if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '*' || *p == '.')) safe = false;
    if (safe && strlen(pats) + strlen(globs[i]) + 2 < sizeof pats) { if (*pats) strcat(pats, " "); strcat(pats, globs[i]); }
  }
  rk__free_paths(globs, ng);
  const char *tools[2] = { "zenity", "kdialog" };
  for (int t = 0; t < 2; t++) {
    char cmd[1400];
    if (t == 0) snprintf(cmd, sizeof cmd, "zenity --file-selection %s%s%s%s 2>/dev/null", a->multiple ? "--multiple --separator='\n' " : "",
                         *pats ? "--file-filter='Accepted files | " : "", pats, *pats ? "'" : "");
    else snprintf(cmd, sizeof cmd, "kdialog --getopenfilename . %s%s%s %s 2>/dev/null", *pats ? "'" : "", pats, *pats ? "|Accepted files'" : "",
                  a->multiple ? "--multiple --separate-output" : "");
    char probe[64];
    snprintf(probe, sizeof probe, "command -v %s >/dev/null 2>&1", tools[t]);
    if (system(probe) != 0) continue;
    FILE *f = popen(cmd, "r");
    if (!f) continue;
    char line[4096];
    while (fgets(line, sizeof line, f)) {
      size_t len = strlen(line);
      while (len && (line[len - 1] == '\n' || line[len - 1] == '\r')) len--;
      add_file(a, line, len);
    }
    pclose(f);
    a->answered = true;
    return true;
  }
  return false;
}

static void *chooser_thread(void *user) {
  ask *a = user;
  if (!portal_choose(a) && !tool_choose(a)) { rk__free_paths(a->files, a->n); a->files = NULL; a->n = 0; }
  rk_app_post(ask_answer, a);
  return NULL;
}

bool rk__desktop_choose_files(void *native, const char *accept, bool multiple, void (*done)(void *, const char *const *, int), void *ctx) {
  (void)native;                                            /* the portal is not told which window: it centres on the screen */
  ask *a = rk__alloc(sizeof *a);
  a->accept = accept ? rk__strdup(accept) : NULL; a->multiple = multiple; a->done = done; a->ctx = ctx;
  pthread_t th;
  if (pthread_create(&th, NULL, chooser_thread, a)) { free(a->accept); free(a); return false; }
  pthread_detach(th);
  return true;
}

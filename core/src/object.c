/* object.c: reference counting, the log and the small allocation helpers. */
#include "rk_internal.h"
#include <stdarg.h>

static void (*log_fn)(rk_log_level, const char *, void *);
static void *log_user;

void rk_app_set_log(void (*fn)(rk_log_level level, const char *message, void *user), void *user) { log_fn = fn; log_user = user; }

void rk__log(rk_log_level level, const char *fmt, ...) {
  char msg[512];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(msg, sizeof msg, fmt, ap);
  va_end(ap);
  if (log_fn) log_fn(level, msg, log_user);
  else fprintf(stderr, "rekta: %s: %s\n", level == RK_LOG_ERROR ? "error" : "warning", msg);
}

void *rk__alloc(size_t size) {
  void *p = calloc(1, size ? size : 1);
  if (!p) { fputs("rekta: out of memory\n", stderr); abort(); }
  return p;
}

void *rk__new(const rk_type *type, size_t size) {
  rk_object *o = rk__alloc(size);
  o->magic = RK__MAGIC; o->type = type; o->refs = 1;
  return o;
}

void *rk__grow(void *p, int *cap, int need, size_t item) {
  if (need <= *cap) return p;
  int n = RK_MAX(need, *cap ? *cap * 2 : 8);
  void *q = realloc(p, (size_t)n * item);
  if (!q) { fputs("rekta: out of memory\n", stderr); abort(); }
  memset((char *)q + (size_t)*cap * item, 0, (size_t)(n - *cap) * item);
  *cap = n;
  return q;
}

char *rk__strdup(const char *s) {
  if (!s) return NULL;
  size_t n = strlen(s) + 1;
  char *d = rk__alloc(n);
  memcpy(d, s, n);
  return d;
}

void rk__setstr(char **dst, const char *src) {
  if (*dst == src) return;
  char *n = rk__strdup(src);
  free(*dst);
  *dst = n;
}

void *rk__expect(void *state, const void *view, const char *fn, const char *kind) {
  if (state) return state;
  const rk_view *v = view;
  if (!v) rk__warn("%s: the view is NULL", fn);
  else if (!rk__is_object(v, NULL)) rk__error("%s: not a Rekta object", fn);
  else rk__warn("%s: a %s is not %s", fn, v->cls ? v->cls->pub.name : "plain view", kind);
  return NULL;
}

bool rk__is_object(const void *p, const rk_type *type) {
  const rk_object *o = p;
  return o && o->magic == RK__MAGIC && o->refs > 0 && (!type || o->type == type);
}

static bool alive(const rk_object *o, const char *what) {
  if (o->magic == RK__MAGIC && o->refs > 0) return true;
  rk__error("%s(%p): %s", what, (const void *)o, o->magic == RK__DEAD ? "the object was already freed" : "not a Rekta object");
  return false;
}

void *rk_retain(void *object) {
  if (object && alive(object, "rk_retain")) ((rk_object *)object)->refs++;
  return object;
}

void rk_release(void *object) {
  rk__main_thread_only("rk_release");
  if (!object || !alive(object, "rk_release")) return;
  rk_object *o = object;
  if (--o->refs == 0) {
    if (o->type && o->type->destroy) o->type->destroy(o);
    o->magic = RK__DEAD;
    free(o);
  }
}

uint32_t rk_version(void) { return RK_VERSION; }

rk_transform rk_transform_identity(void) {
  rk_transform t = { 0, 0, 1, 1, 0, 0, 0, 0.5f, 0.5f, 0 };
  return t;
}

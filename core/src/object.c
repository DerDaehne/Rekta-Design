/* object.c: reference counting and the small allocation helpers. */
#include "rk_internal.h"

void *rk__alloc(size_t size) {
  void *p = calloc(1, size ? size : 1);
  if (!p) { fputs("rekta: out of memory\n", stderr); abort(); }
  return p;
}

void *rk__new(const rk_type *type, size_t size) {
  rk_object *o = rk__alloc(size);
  o->type = type; o->refs = 1;
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

void *rk_retain(void *object) {
  if (object) { rk_object *o = object; assert(o->refs > 0); o->refs++; }
  return object;
}

void rk_release(void *object) {
  if (!object) return;
  rk_object *o = object;
  assert(o->refs > 0);
  if (--o->refs == 0) {
    if (o->type && o->type->destroy) o->type->destroy(o);
    free(o);
  }
}

uint32_t rk_version(void) { return RK_VERSION; }

rk_transform rk_transform_identity(void) {
  rk_transform t = { 0, 0, 1, 1, 0, 0, 0, 0.5f, 0.5f, 0 };
  return t;
}

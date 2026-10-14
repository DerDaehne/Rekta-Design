/* sort.c: rows the user puts in order, as the web's sortable() on .rk-rows. Each row gets a grip at its leading edge
   (six dots, 24 px, text-medium); only the grip moves a row, so a click anywhere else stays a click. A row follows the
   pointer once it has moved 4 px up or down, lifted (the background and a 1 px text-medium edge, no shadow); the
   others glide aside over duration-short to show where it would land. Let go, every row that moved glides to its new
   place over duration-moderate, the lifted one too. On the grip, Space or Enter picks the row up, Up and Down move it,
   Space or Enter puts it down and Escape puts it back; Alt with Up or Down moves it at once. The app hears every move
   (from, to) to keep its own order and its undo. Included after widgets.c, whose rows it extends. */
#include "rk_internal.h"

typedef struct sorter {
  void (*moved)(rk_view *rows, int from, int to, void *user); void *user;
  rk_view *drag; int from, to;                              /* a row under the pointer; drawn last while it is lifted */
  float start_y, h; bool dragging;
  float *shift; int shift_cap;                              /* where each other row has been sent, by its place */
  rk_view *grabbed; int grab_from;                          /* a row picked up from the keyboard */
} sorter;

static void sorter_destroy(void *st) { sorter *s = st; free(s->shift); free(s); }
static void sorter_layout(rk_view *v, void *st);
static const rk_class sorter_class = { { sizeof(rk_view_class), "rk_rows_sortable", NULL, NULL, sorter_layout, NULL, sorter_destroy }, NULL, NULL };
static sorter *sorter_of(rk_view *rows) { return rows && rows->cls == &sorter_class ? rows->state : NULL; }

/* The rows in their order. While a row is lifted it is drawn last, so it is the last child; its place is from. */
static rk_view **logical(rk_view *v, int *count) {
  sorter *s = v->state;
  rk_view **out = rk__alloc((size_t)RK_MAX(v->nkids, 1) * sizeof *out);
  int n = 0;
  for (int i = 0; i < v->nkids; i++) if (!(s->dragging && v->kids[i] == s->drag)) out[n++] = v->kids[i];
  if (s->dragging) {
    int at = RK_CLAMP(s->from, 0, n);
    memmove(out + at + 1, out + at, (size_t)(n - at) * sizeof *out);
    out[at] = s->drag; n++;
  }
  *count = n;
  return out;
}

static void sorter_layout(rk_view *v, void *st) {
  (void)st;
  int n;
  rk_view **rows = logical(v, &n);
  rk_insets p = v->padding;
  float w = RK_MAX(0.0f, v->frame.w - p.left - p.right), y = p.top;
  for (int i = 0; i < n; i++) {
    if (rows[i]->hidden) continue;
    float h = rk__measure(rows[i], (rk_size){ w, INFINITY }).h;
    rk__set_frame(rows[i], (rk_rect){ p.left, y, w, h });
    y += h + v->gap;
  }
  free(rows);
}

static bool rows_last(rk_view *v) {
  rk_view *p = v->parent;
  if (!p || !p->nkids) return false;
  sorter *s = sorter_of(p);
  if (!s || !s->dragging) return p->kids[p->nkids - 1] == v;
  /* while a row is lifted it is the last child; in the order it stands at from */
  if (s->from >= p->nkids - 1) return v == s->drag;
  rk_view *last = p->kids[p->nkids - 1] == s->drag ? p->kids[p->nkids - 2] : p->kids[p->nkids - 1];
  return v == last;
}

static int place_of(rk_view *rows, rk_view *row) {
  int n, at = -1;
  rk_view **list = logical(rows, &n);
  for (int i = 0; i < n && at < 0; i++) if (list[i] == row) at = i;
  free(list);
  return at;
}

static void set_y(rk_view *v, float y) {
  rk__anim_cancel(v, RK_PROP_TRANSFORM);
  v->transform.translate_y = y; v->ptransform = v->transform;
  rk__view_dirty(v);
}

static void glide_y(rk_view *v, float from, float to, double dur) {
  rk_transform a = v->ptransform, b = v->transform;
  a.translate_y = from; b.translate_y = to;
  v->transform.translate_y = to;
  rk__animate_transform(v, &a, &b, dur, 0, rk__ease_base);
}

/* Put the row at from into place to, and let every row that moved glide there from where it was drawn. */
static void sorter_move(rk_view *v, int from, int to, bool tell) {
  sorter *s = v->state;
  int n = v->nkids;
  if (from < 0 || from >= n || to < 0 || to >= n) return;
  float *was = rk__alloc((size_t)n * sizeof *was);
  for (int i = 0; i < n; i++) was[i] = v->kids[i]->frame.y + v->kids[i]->ptransform.translate_y;
  rk_view **old = rk__alloc((size_t)n * sizeof *old);
  memcpy(old, v->kids, (size_t)n * sizeof *old);
  if (from != to) {
    rk_view *r = v->kids[from];
    if (from < to) memmove(v->kids + from, v->kids + from + 1, (size_t)(to - from) * sizeof *v->kids);
    else memmove(v->kids + to + 1, v->kids + to, (size_t)(from - to) * sizeof *v->kids);
    v->kids[to] = r;
  }
  rk__view_relayout_up(v);
  rk__layout_pending();
  double d = rk_duration_of(RK_DURATION_MODERATE);
  for (int i = 0; i < n; i++) {
    rk_view *k = old[i];
    float dy = was[i] - k->frame.y;
    if (fabsf(dy) > 0.5f) { set_y(k, dy); glide_y(k, dy, 0, d); } else set_y(k, 0);
  }
  free(was); free(old);
  rk__view_dirty(v);
  (void)s;
  if (tell && from != to && s->moved) s->moved(v, from, to, s->user);
}

/* The grip ----------------------------------------------------------------------------------------------------------- */

static void grip_draw(rk_view *v, rk_canvas *c, void *st) {
  rk_view *rowv = st;
  row *r = rowv->state;
  rk_window *w = rk__view_window(v);
  bool held = r->lifted;
  rk_color ink = rk_color_of(held ? RK_ROLE_TEXT : RK_ROLE_TEXT_MEDIUM);
  float oy = (v->pframe.h - 20) / 2, ox = (v->pframe.w - 20) / 2;
  for (int col = 0; col < 2; col++)
    for (int line = 0; line < 3; line++)
      rk_canvas_fill_rect(c, (rk_rect){ ox + 7 + col * 4, oy + 4 + line * 5, 2, 2 }, ink);
  if (w && w->focus == v && w->focus_visible) rk_canvas_stroke_rect(c, (rk_rect){ 0, 0, v->pframe.w, v->pframe.h }, 2, rk_color_of(RK_ROLE_FOCUS));
}

static void lift(rk_view *rowv, bool on) { ((row *)rowv->state)->lifted = on; rk__view_dirty(rowv); if (((row *)rowv->state)->grip) rk__view_dirty(((row *)rowv->state)->grip); }

/* While dragging: the lifted row follows, the others between where it was and where it would land step aside. */
static void drag_to(rk_view *v, float dy) {
  sorter *s = v->state;
  int n;
  rk_view **list = logical(v, &n);
  set_y(s->drag, dy);
  float mid = s->drag->frame.y + s->drag->frame.h / 2 + dy;
  int to = s->from;
  for (int i = 0; i < n; i++) {
    if (i == s->from) continue;
    float m = list[i]->frame.y + list[i]->frame.h / 2;
    if (i > s->from && mid > m) to = i;
    if (i < s->from && mid < m && to == s->from) to = i;
  }
  s->to = to;
  s->shift = rk__grow(s->shift, &s->shift_cap, n, sizeof *s->shift);
  for (int i = 0; i < n; i++) {
    if (i == s->from) continue;
    float want = s->from < i && i <= to ? -s->h : to <= i && i < s->from ? s->h : 0;
    if (s->shift[i] == want) continue;
    s->shift[i] = want;
    glide_y(list[i], list[i]->ptransform.translate_y, want, rk_duration_of(RK_DURATION_SHORT));
  }
  free(list);
}

static void drag_end(rk_view *v, bool keep) {
  sorter *s = v->state;
  if (!s->dragging) { s->drag = NULL; return; }
  rk_view *r = s->drag;
  int from = s->from, to = keep ? s->to : s->from;
  /* the lifted row back in its place among the children, then the move */
  int at = -1;
  for (int i = 0; i < v->nkids; i++) if (v->kids[i] == r) at = i;
  if (at >= 0 && at != from) {
    memmove(v->kids + from + 1, v->kids + from, (size_t)(at - from) * sizeof *v->kids);
    v->kids[from] = r;
  }
  s->dragging = false; s->drag = NULL;
  lift(r, false);
  sorter_move(v, from, to, true);
}

static bool grip_event(rk_view *g, const rk_event *e, void *st) {
  rk_view *rowv = st, *v = rowv->parent;
  sorter *s = sorter_of(v);
  if (!s) return false;
  switch (e->type) {
    case RK_EVENT_POINTER_DOWN:
      if (e->button || s->grabbed) return false;
      s->drag = rowv; s->dragging = false;
      s->start_y = rk__window_rect(g).y + e->position.y;
      return true;
    case RK_EVENT_POINTER_MOVE: {
      if (s->drag != rowv) return false;
      float dy = rk__window_rect(g).y + e->position.y - s->start_y;
      if (!s->dragging) {
        if (fabsf(dy) < 4) return true;                     /* not yet a drag: a click is still a click */
        s->dragging = true;
        s->from = s->to = place_of(v, rowv);
        s->h = rowv->frame.h + v->gap;
        s->shift = rk__grow(s->shift, &s->shift_cap, v->nkids, sizeof *s->shift);
        memset(s->shift, 0, (size_t)v->nkids * sizeof *s->shift);
        for (int i = 0; i < v->nkids; i++) if (v->kids[i] == rowv) {   /* drawn last, above the others */
          memmove(v->kids + i, v->kids + i + 1, (size_t)(v->nkids - i - 1) * sizeof *v->kids);
          v->kids[v->nkids - 1] = rowv;
          break;
        }
        lift(rowv, true);
      }
      drag_to(v, dy);
      return true;
    }
    case RK_EVENT_POINTER_UP:
      if (s->drag != rowv) return false;
      drag_end(v, true);
      return true;
    case RK_EVENT_KEY_DOWN: {
      int at = place_of(v, rowv), n = v->nkids;
      bool alt = (e->mods & RK_MOD_ALT) != 0;
      if (!s->grabbed) {
        if (e->key == RK_KEY_SPACE || e->key == RK_KEY_ENTER) { s->grabbed = rowv; s->grab_from = at; lift(rowv, true); return true; }
        if (alt && (e->key == RK_KEY_UP || e->key == RK_KEY_DOWN)) { sorter_move(v, at, RK_CLAMP(at + (e->key == RK_KEY_DOWN ? 1 : -1), 0, n - 1), true); return true; }
        return false;
      }
      switch (e->key) {
        case RK_KEY_UP: case RK_KEY_DOWN: sorter_move(v, at, RK_CLAMP(at + (e->key == RK_KEY_DOWN ? 1 : -1), 0, n - 1), true); return true;
        case RK_KEY_SPACE: case RK_KEY_ENTER: s->grabbed = NULL; lift(rowv, false); return true;
        case RK_KEY_ESCAPE: s->grabbed = NULL; lift(rowv, false); sorter_move(v, at, s->grab_from, true); return true;   /* put back; not a step back */
        default: return true;                                 /* held: nothing else moves it, nor anything else */
      }
    }
    case RK_EVENT_BLUR:
      if (s->grabbed == rowv) { s->grabbed = NULL; lift(rowv, false); }   /* let go where it is */
      rk__view_dirty(g);
      return false;
    case RK_EVENT_FOCUS: rk__view_dirty(g); return false;
    default: return false;
  }
}

static const rk_class grip_class = { { sizeof(rk_view_class), "rk_rows_grip", grip_draw, NULL, NULL, grip_event, NULL }, NULL, NULL };

static void give_grip(rk_view *rowv) {
  row *r = rowv->state;
  if (r->grip) return;
  rk_view *g = rk__view_create(&grip_class, rowv);
  g->focusable = true; g->draws_focus = true;
  g->access = RK_ACCESS_BUTTON;
  size_t len = strlen(r->title) + 8;
  char *label = rk__alloc(len);
  snprintf(label, len, "move %s", r->title);
  rk__setstr(&g->access_label, label);
  free(label);
  r->grip = g;
  rk__view_add(rowv, g); rk_release(g);
  rk__view_relayout_up(rowv);
}

static void rows_added(rk_view *rows, rk_view *rowv) { if (sorter_of(rows)) give_grip(rowv); }

void rk_rows_set_sortable(rk_view *rows, void (*moved)(rk_view *rows, int from, int to, void *user), void *user) {
  if (!rows || (rows->cls && rows->cls != &sorter_class)) return;
  if (!rows->cls) { rows->cls = &sorter_class; rows->state = rk__alloc(sizeof(sorter)); }
  sorter *s = rows->state;
  s->moved = moved; s->user = user;
  for (int i = 0; i < rows->nkids; i++) if (rows->kids[i]->cls == &row_class) give_grip(rows->kids[i]);
  rk__view_relayout_up(rows);
}

bool rk_rows_move(rk_view *rows, int from, int to) {
  sorter *s = sorter_of(rows);
  if (!rows || from < 0 || to < 0 || from >= rows->nkids || to >= rows->nkids || from == to) return false;
  if (s) { sorter_move(rows, from, to, false); return true; }
  return false;
}

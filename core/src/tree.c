/* tree.c: items that hold items, opened and closed in place, as the web's .rk-tree. One view draws the rows; the
   app adds items and keeps their numbers. Rows are row-height tall, each level indented by space-m, with a twisty in
   front of every row (an empty one on a leaf, so the names line up): a chevron that points right and turns down over
   duration-short when its branch opens. The selected row takes the accent fill with on-accent text, no checkmark.

   A branch opens over its height with a fade (duration-short, ease-in) and closes in 0.7 of that (ease-out); the rows
   below make room as it goes. Selection never follows focus: Up and Down, Home and End move the active row; Right
   opens a closed branch or goes into an open one, Left closes an open one or goes up to the parent; Enter or Space
   selects and shows; * opens every sibling; a letter goes to the next row that starts with it. A click selects and
   shows; a click on the twisty opens or closes without selecting; a double click opens or closes. Included after
   widgets.c, whose status helpers it shares. */
#include "rk_internal.h"

typedef struct tween { float from, to; double t0, dur; const float *ease; } tween;

static float tween_at(const tween *t, double now) {
  if (t->dur <= 0 || now >= t->t0 + t->dur) return t->to;
  float k = (float)((now - t->t0) / t->dur);
  return t->from + (t->to - t->from) * rk__ease(t->ease, RK_CLAMP(k, 0.0f, 1.0f));
}
static bool tween_moving(const tween *t, double now) { return t->dur > 0 && now < t->t0 + t->dur; }
static void tween_to(tween *t, double now, float to, double dur, const float *ease) {
  t->from = tween_at(t, now); t->to = to; t->t0 = now; t->dur = dur; t->ease = ease;
}

typedef struct node {
  char *title, *meta; rk_status status;
  int parent, first, last, next;                           /* the item's place: -1 where there is none */
  bool open;
  tween grow, turn;                                        /* the branch's height, 0 to 1; the chevron, 0 right to 1 down */
} node;

typedef struct trow { int item, level; float y, bottom, alpha; } trow;   /* a row as drawn: clipped at bottom, faded */

typedef struct tree {
  node *nodes; int n, cap, first, last;
  int active, selected, hover, pressed, press_twisty;
  double down_at; int down_item;                           /* the last press: a second soon after opens or closes */
  trow *rows; int nrows, rcap;
  void (*show)(rk_view *tree, int item, void *user); void *show_user;
  void (*toggle)(rk_view *tree, int item, bool open, void *user); void *toggle_user;
} tree;

static float tree_row_h(void) { return rk_metric_of(RK_ROW_HEIGHT); }
static float indent_of(int level) { return level * rk_metric_of(RK_SPACE_M); }
#define TWISTY 20.0f

/* The rows as they stand now: a branch that opens or closes shows the part of its rows its height reaches, faded as
   it is; what follows starts below that part. */
static float subtree_h(tree *t, int first, double now) {
  float h = 0;
  for (int i = first; i >= 0; i = t->nodes[i].next) {
    h += tree_row_h();
    float g = tween_at(&t->nodes[i].grow, now);
    if (t->nodes[i].first >= 0 && g > 0) h += g * subtree_h(t, t->nodes[i].first, now);
  }
  return h;
}

static float walk(tree *t, int first, int level, float y, float bottom, float alpha, double now) {
  float y0 = y, H = tree_row_h();
  for (int i = first; i >= 0 && y < bottom; i = t->nodes[i].next) {
    t->rows = rk__grow(t->rows, &t->rcap, t->nrows + 1, sizeof *t->rows);
    t->rows[t->nrows++] = (trow){ i, level, y, bottom, alpha };
    y += H;
    float g = tween_at(&t->nodes[i].grow, now);
    if (t->nodes[i].first < 0 || g <= 0) continue;
    float shown = g * subtree_h(t, t->nodes[i].first, now);
    walk(t, t->nodes[i].first, level + 1, y, RK_MIN(bottom, y + shown), alpha * g, now);
    y += shown;
  }
  return y - y0;
}

static float build_rows(tree *t, double now) {
  t->nrows = 0;
  return walk(t, t->first, 0, 0, INFINITY, 1, now);
}

static bool tree_moving(tree *t, double now) {
  for (int i = 0; i < t->n; i++) if (tween_moving(&t->nodes[i].grow, now) || tween_moving(&t->nodes[i].turn, now)) return true;
  return false;
}

/* Rows the keyboard can reach: every row whose branches are all open (or opening). */
static bool reachable(tree *t, int i) {
  for (int p = t->nodes[i].parent; p >= 0; p = t->nodes[p].parent) if (!t->nodes[p].open) return false;
  return true;
}
static int next_reachable(tree *t, int i, int dir) {
  /* in drawing order: a node's first child after it, then its next sibling, then an ancestor's next sibling */
  if (dir > 0) {
    if (t->nodes[i].first >= 0 && t->nodes[i].open) return t->nodes[i].first;
    for (int x = i; x >= 0; x = t->nodes[x].parent) if (t->nodes[x].next >= 0) return t->nodes[x].next;
    return -1;
  }
  int p = t->nodes[i].parent, prev = -1;
  for (int x = p >= 0 ? t->nodes[p].first : t->first; x >= 0 && x != i; x = t->nodes[x].next) prev = x;
  if (prev < 0) return p;
  while (t->nodes[prev].first >= 0 && t->nodes[prev].open) prev = t->nodes[prev].last;
  return prev;
}
static int last_reachable(tree *t) {
  int x = t->last;
  while (x >= 0 && t->nodes[x].first >= 0 && t->nodes[x].open) x = t->nodes[x].last;
  return x;
}

static float row_y(tree *t, int item, double now) {
  build_rows(t, now);
  for (int k = 0; k < t->nrows; k++) if (t->rows[k].item == item) return t->rows[k].y;
  return -1;
}

static void tree_set_active(rk_view *v, tree *t, int i) {
  if (i < 0 || i >= t->n) return;
  rk_window *w = rk__view_window(v);
  if (w) w->focus_visible = true;                          /* moved by a key: the keyboard is in use, show where */
  t->active = i;
  /* reveal where it will stand once the branches have moved */
  double later = rk_app_now() + 10000;
  float y = row_y(t, i, later);
  if (y >= 0) rk__reveal_rect(v, (rk_rect){ 0, y, v->frame.w, tree_row_h() });
  rk__view_dirty(v);
}

static void tree_open(rk_view *v, tree *t, int i, bool open) {
  node *n = &t->nodes[i];
  if (n->first < 0 || n->open == open) return;
  double now = rk_app_now(), d = rk_duration_of(RK_DURATION_SHORT);
  n->open = open;
  tween_to(&n->grow, now, open ? 1 : 0, open ? d : 0.7 * d, open ? rk__ease_in : rk__ease_out);
  tween_to(&n->turn, now, open ? 1 : 0, d, rk__ease_base);
  if (!open && t->active >= 0 && !reachable(t, t->active)) t->active = i;   /* the active row went in: it is the branch */
  if (t->toggle) t->toggle(v, i, open, t->toggle_user);
  rk__view_relayout_up(v);
  rk__view_dirty(v);
  rk__app_request_frame();
}

static void tree_select(rk_view *v, tree *t, int i, bool show) {
  t->selected = i;
  if (i >= 0) t->active = i;
  rk__view_dirty(v);
  if (show && i >= 0 && t->show) t->show(v, i, t->show_user);
}

static void draw_chevron(rk_canvas *c, float cx, float cy, float k, rk_color ink) {
  /* a 6 px box's top and right edges, turned 45 degrees (right) to 135 (down), nudged 1 px as it turns */
  float a = (45 + 90 * k) * (float)M_PI / 180, ca = cosf(a), sa = sinf(a), h = 3;
  rk_point p[3] = { { -h, -h }, { h, -h }, { h, h } };
  for (int i = 0; i < 3; i++) {
    float x = p[i].x, y = p[i].y;
    p[i] = (rk_point){ cx + k + x * ca - y * sa, cy + k + x * sa + y * ca };
  }
  rk_canvas_polyline(c, p, 3, 1.6f, ink);
}

static void tree_draw(rk_view *v, rk_canvas *c, void *st) {
  tree *t = st;
  double now = rk_app_now();
  float W = v->pframe.w, H = tree_row_h(), pad = rk_metric_of(RK_SPACE_S), gap = rk_metric_of(RK_SPACE_XS);
  rk_window *w = rk__view_window(v);
  bool focused = w && w->focus == v && w->focus_visible;
  build_rows(t, now);
  float salpha = c->alpha;
  for (int k = 0; k < t->nrows; k++) {
    trow *r = &t->rows[k];
    node *n = &t->nodes[r->item];
    if (r->alpha <= 0.002f) continue;
    rk_canvas_push_clip(c, (rk_rect){ 0, r->y, W, RK_MAX(0.0f, RK_MIN(H, r->bottom - r->y)) });
    c->alpha = salpha * r->alpha;
    bool on = r->item == t->selected;
    rk_color ink = rk_color_of(on ? RK_ROLE_ON_ACCENT : RK_ROLE_TEXT);
    if (on) rk_canvas_fill_rect(c, (rk_rect){ 0, r->y, W, H }, rk_color_of(RK_ROLE_ACCENT));
    if (r->item == t->hover || r->item == t->pressed) rk_canvas_fill_rect(c, (rk_rect){ 0, r->y, W, H }, rk_color_of(RK_ROLE_HOVER));
    float x = indent_of(r->level);
    if (n->first >= 0) draw_chevron(c, x + 9, r->y + H / 2, tween_at(&n->turn, now), ink);
    x += TWISTY + gap;
    float right = W - pad;
    if (n->status != RK_STATUS_NONE) {
      rk_textfmt tf = rk__textfmt(RK_TEXT_LABEL, 0);
      const char *word = n->meta ? n->meta : status_word(n->status);
      float tw = rk__text_width(word, -1, &tf);
      rk__text_draw(c, word, -1, right - tw, r->y + (H - tf.size * tf.line) / 2, &tf, ink);
      rk_canvas_fill_rect(c, (rk_rect){ right - tw - 14, r->y + H / 2 - 4, 8, 8 }, on ? ink : rk_color_of(status_full(n->status)));
      right -= tw + 14 + rk_metric_of(RK_SPACE_M);
    } else if (n->meta) {
      rk_textfmt mf = rk__textfmt(RK_TEXT_META, 0);
      float mw = rk__text_width(n->meta, -1, &mf);
      rk__text_draw(c, n->meta, -1, right - mw, r->y + (H - mf.size * mf.line) / 2, &mf, on ? ink : rk_color_of(RK_ROLE_TEXT_HIGH));
      right -= mw + rk_metric_of(RK_SPACE_M);
    }
    rk_textfmt f = rk__textfmt(RK_TEXT_BODY, 0);
    rk_canvas_push_clip(c, (rk_rect){ x, r->y, RK_MAX(0.0f, right - x), H });
    rk__text_draw(c, n->title, -1, x, r->y + (H - f.size * f.line) / 2, &f, ink);
    rk_canvas_pop_clip(c);
    if (focused && r->item == t->active)                   /* inside the row; on the accent, in its own ink */
      rk_canvas_stroke_rect(c, on ? (rk_rect){ 2, r->y + 2, W - 4, H - 4 } : (rk_rect){ 0, r->y, W, H }, 2, on ? ink : rk_color_of(RK_ROLE_FOCUS));
    c->alpha = salpha;
    rk_canvas_pop_clip(c);
  }
  if (tree_moving(t, now)) { rk__view_relayout_up(v); rk__view_dirty(v); rk__app_request_frame(); }
}

static rk_size tree_measure(rk_view *v, rk_size avail, void *st) {
  (void)v;
  tree *t = st;
  return (rk_size){ isfinite(avail.w) ? avail.w : 320, build_rows(t, rk_app_now()) };
}

static int row_under(tree *t, float y, double now) {
  build_rows(t, now);
  for (int k = 0; k < t->nrows; k++) {
    trow *r = &t->rows[k];
    if (r->alpha > 0.5f && y >= r->y && y < r->y + tree_row_h() && y < r->bottom) return k;
  }
  return -1;
}

static bool on_twisty(tree *t, int k, float x) {
  float left = indent_of(t->rows[k].level);
  return t->nodes[t->rows[k].item].first >= 0 && x >= left && x < left + TWISTY + rk_metric_of(RK_SPACE_XS);
}

static int tree_fold(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }   /* the locale's case rules are not the tree's */
static int first_letter(const char *s) { return s && *s ? tree_fold((unsigned char)*s) : -1; }

static bool tree_event(rk_view *v, const rk_event *e, void *st) {
  tree *t = st;
  double now = rk_app_now();
  switch (e->type) {
    case RK_EVENT_POINTER_MOVE: {
      int k = row_under(t, e->position.y, now), item = k >= 0 ? t->rows[k].item : -1;
      if (item != t->hover) { t->hover = item; rk__view_dirty(v); }
      return false;
    }
    case RK_EVENT_POINTER_LEAVE: if (t->hover >= 0) { t->hover = -1; rk__view_dirty(v); } return false;
    case RK_EVENT_POINTER_DOWN: {
      if (e->button) return false;
      int k = row_under(t, e->position.y, now);
      if (k < 0) return true;
      t->pressed = t->rows[k].item; t->press_twisty = on_twisty(t, k, e->position.x);
      rk__view_dirty(v);
      return true;
    }
    case RK_EVENT_POINTER_UP: {
      int k = row_under(t, e->position.y, now), was = t->pressed;
      t->pressed = -1;
      rk__view_dirty(v);
      if (k < 0 || t->rows[k].item != was) return true;
      int i = was;
      if (t->press_twisty) { t->active = i; tree_open(v, t, i, !t->nodes[i].open); return true; }
      if (i == t->down_item && e->time - t->down_at < 400) { t->down_at = 0; tree_open(v, t, i, !t->nodes[i].open); return true; }
      t->down_item = i; t->down_at = e->time;
      tree_select(v, t, i, true);
      return true;
    }
    case RK_EVENT_KEY_DOWN: {
      if (e->mods & (RK_MOD_CTRL | RK_MOD_ALT | RK_MOD_SUPER) || t->first < 0) return false;
      int a = t->active >= 0 && reachable(t, t->active) ? t->active : t->first;
      node *n = &t->nodes[a];
      switch (e->key) {
        case RK_KEY_DOWN: { int x = next_reachable(t, a, 1); if (x >= 0) tree_set_active(v, t, x); return true; }
        case RK_KEY_UP: { int x = next_reachable(t, a, -1); if (x >= 0) tree_set_active(v, t, x); return true; }
        case RK_KEY_HOME: tree_set_active(v, t, t->first); return true;
        case RK_KEY_END: tree_set_active(v, t, last_reachable(t)); return true;
        case RK_KEY_RIGHT:
          if (n->first < 0) return false;
          if (!n->open) tree_open(v, t, a, true); else tree_set_active(v, t, n->first);
          return true;
        case RK_KEY_LEFT:
          if (n->first >= 0 && n->open) tree_open(v, t, a, false);
          else if (n->parent >= 0) tree_set_active(v, t, n->parent);
          return true;
        case RK_KEY_ENTER: case RK_KEY_SPACE: tree_select(v, t, a, true); return true;
        default: return false;
      }
    }
    case RK_EVENT_TEXT: {
      if (!e->text || !e->text[0] || e->text[1] || t->first < 0) return false;
      int a = t->active >= 0 && reachable(t, t->active) ? t->active : t->first;
      if (e->text[0] == '*') {                              /* every sibling opens */
        int p = t->nodes[a].parent;
        for (int x = p >= 0 ? t->nodes[p].first : t->first; x >= 0; x = t->nodes[x].next) tree_open(v, t, x, true);
        return true;
      }
      int want = tree_fold((unsigned char)e->text[0]);
      for (int x = next_reachable(t, a, 1), guard = 0; guard <= t->n; guard++) {
        if (x < 0) x = t->first;
        if (first_letter(t->nodes[x].title) == want) { tree_set_active(v, t, x); return true; }
        if (x == a) break;
        x = next_reachable(t, x, 1);
      }
      return true;
    }
    case RK_EVENT_FOCUS: case RK_EVENT_BLUR: rk__view_dirty(v); return false;
    default: return false;
  }
}

static void tree_destroy(void *st) {
  tree *t = st;
  for (int i = 0; i < t->n; i++) { free(t->nodes[i].title); free(t->nodes[i].meta); }
  free(t->nodes); free(t->rows); free(t);
}

static const rk_class tree_class = { { sizeof(rk_view_class), "rk_tree", tree_draw, tree_measure, NULL, tree_event, tree_destroy }, NULL, NULL };

static tree *tree_of(rk_view *v) { return v && v->cls == &tree_class ? v->state : NULL; }
static bool tree_has(tree *t, int i) { return t && i >= 0 && i < t->n; }

rk_view *rk_tree(void) {
  tree *t = rk__alloc(sizeof *t);
  t->first = t->last = t->active = t->selected = t->hover = t->pressed = t->down_item = -1;
  rk_view *v = rk__view_create(&tree_class, t);
  v->focusable = true;
  v->access = RK_ACCESS_TREE;
  v->draws_focus = true;                                   /* the active row carries the ring */
  return v;
}

int rk_tree_add(rk_view *v, int parent, const char *title) {
  tree *t = tree_of(v);
  if (!t || (parent >= 0 && !tree_has(t, parent))) return -1;
  t->nodes = rk__grow(t->nodes, &t->cap, t->n + 1, sizeof *t->nodes);
  int i = t->n++;
  t->nodes[i] = (node){ .title = rk__strdup(title ? title : ""), .status = RK_STATUS_NONE, .parent = parent < 0 ? -1 : parent, .first = -1, .last = -1, .next = -1 };
  int *first = parent >= 0 ? &t->nodes[parent].first : &t->first, *last = parent >= 0 ? &t->nodes[parent].last : &t->last;
  if (*last >= 0) t->nodes[*last].next = i; else *first = i;
  *last = i;
  if (t->active < 0) t->active = i;
  rk__view_relayout_up(v);
  return i;
}

void rk_tree_set_meta(rk_view *v, int item, const char *meta, rk_status status) {
  tree *t = tree_of(v);
  if (!tree_has(t, item)) return;
  free(t->nodes[item].meta);
  t->nodes[item].meta = meta ? rk__strdup(meta) : NULL;
  t->nodes[item].status = status;
  rk__view_dirty(v);
}

void rk_tree_open(rk_view *v, int item, bool open) {
  tree *t = tree_of(v);
  if (!t) return;
  if (item < 0) { for (int i = 0; i < t->n; i++) tree_open(v, t, i, open); return; }   /* -1: every branch */
  if (tree_has(t, item)) tree_open(v, t, item, open);
}

bool rk_tree_is_open(rk_view *v, int item) { tree *t = tree_of(v); return tree_has(t, item) && t->nodes[item].open; }
void rk_tree_select(rk_view *v, int item) { tree *t = tree_of(v); if (t && (item < 0 || tree_has(t, item))) tree_select(v, t, item, false); }
int rk_tree_selected(rk_view *v) { tree *t = tree_of(v); return t ? t->selected : -1; }
int rk_tree_parent(rk_view *v, int item) { tree *t = tree_of(v); return tree_has(t, item) ? t->nodes[item].parent : -1; }
const char *rk_tree_title(rk_view *v, int item) { tree *t = tree_of(v); return tree_has(t, item) ? t->nodes[item].title : NULL; }
void rk_tree_on_show(rk_view *v, void (*fn)(rk_view *, int, void *), void *user) { tree *t = tree_of(v); if (t) { t->show = fn; t->show_user = user; } }
void rk_tree_on_toggle(rk_view *v, void (*fn)(rk_view *, int, bool, void *), void *user) { tree *t = tree_of(v); if (t) { t->toggle = fn; t->toggle_user = user; } }

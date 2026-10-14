/* view.c: the view tree, its properties, measuring and the layouts. */
#include "rk_internal.h"

static void view_destroy(void *self) {
  rk_view *v = self;
  rk__live_views--;
  rk__anim_view_gone(v);
  for (int i = 0; i < v->nkids; i++) { v->kids[i]->parent = NULL; rk_release(v->kids[i]); }
  free(v->kids);
  if (v->cls && v->cls->pub.destroy) v->cls->pub.destroy(v->state);
  if (v->free_user) v->free_user(v->user);
  free(v->name); free(v->access_label); free(v->access_value); free(v->tooltip);
  rk_release(v->menu);
  rk__bitmap_free(v->layer); rk__bitmap_free(v->snapshot);
  free(v->own_cls);
}

const rk_type rk__view_type = { "rk_view", view_destroy };
int rk__live_views;

rk_view *rk__view_create(const rk_class *cls, void *state) {
  rk__main_thread_only("a view's constructor");
  rk_view *v = rk__new(&rk__view_type, sizeof *v);
  rk__live_views++;
  v->opacity = v->popacity = 1;
  v->transform = v->ptransform = rk_transform_identity();
  v->main = v->cross = RK_ALIGN_AUTO;                       /* a column stretches, a row keeps to the start */
  v->span_c = v->span_r = 1;
  v->content_alpha = 1;
  v->cls = cls; v->state = state;
  v->needs_layout = true;
  return v;
}

rk_view *rk_view_create(void) { return rk__view_create(NULL, NULL); }

rk_view *rk_view_create_custom(const rk_view_class *pub, void *state) {
  /* the class may be a shorter, older struct: copy what it has */
  rk_class *cls = rk__alloc(sizeof *cls);
  if (RK__SIZED(pub, "rk_view_create_custom", "rk_view_class")) memcpy(&cls->pub, pub, RK_MIN((size_t)pub->size, sizeof cls->pub));
  cls->pub.size = sizeof cls->pub;
  rk_view *v = rk__view_create(cls, state);
  v->own_cls = cls;
  return v;
}

void *rk_view_state(rk_view *v) { return v->state; }

/* The tree -------------------------------------------------------------------------------------------------------- */

rk_window *rk__view_window(rk_view *v) {
  while (v && !v->window) v = v->parent;
  return v ? v->window : NULL;
}

rk_window *rk_view_window(rk_view *v) { return rk__view_window(v); }

static bool layered(const rk_view *v) {
  const rk_transform *t = &v->ptransform;
  return t->rotate_x != 0 || t->rotate_y != 0 || t->rotate_z != 0 || fabsf(t->scale_x - 1) > 1e-4f || fabsf(t->scale_y - 1) > 1e-4f || v->transition;
}

/* Only what changed is drawn again: the view's box, or, when it or an ancestor turns in depth, that ancestor's box
   with room for the projection. */
static void damage(rk_view *v);

/* What contains the view shows it differently now: the layers of the view and of everything around it are stale. */
void rk__view_dirty(rk_view *v) {
  for (rk_view *p = v; p; p = p->parent) p->layer_ok = false;
  damage(v);
}

void rk__view_moved(rk_view *v) {
  for (rk_view *p = v->parent; p; p = p->parent) p->layer_ok = false;
  damage(v);
}

static void damage(rk_view *v) {
  rk_window *w = rk__view_window(v);
  if (!w) return;
  rk_view *top = NULL;
  for (rk_view *p = v; p && !p->window; p = p->parent) if (layered(p)) top = p;
  rk_rect r = rk__window_rect(top ? top : v);
  float m = top ? RK_MAX(r.w, r.h) * 0.35f + 8 : 8;
  r.x -= m; r.y -= m; r.w += 2 * m; r.h += 2 * m;
  rk__window_damage(w, r);
}

/* The view changed its size or content: the layout runs again before the next frame. Only what changed is drawn
   again - the view where it was now, and (rk__set_frame) every view where it was and where it lands. */
uint32_t rk__measure_gen = 1;

void rk__view_relayout_up(rk_view *v) {
  rk__measure_gen++;                                       /* what was measured may measure differently now */
  for (rk_view *p = v; p; p = p->parent) p->needs_layout = true;
  rk_window *w = rk__view_window(v);
  if (!w) return;
  if (!w->needs_layout) { w->needs_layout = true; rk__app_request_frame(); }
  if (v->placed) rk__view_dirty(v);
}

void rk__view_detach(rk_view *v) {
  rk_view *p = v->parent;
  if (!p) return;
  for (int i = 0; i < p->nkids; i++) if (p->kids[i] == v) {
    memmove(p->kids + i, p->kids + i + 1, (size_t)(p->nkids - i - 1) * sizeof *p->kids);
    p->nkids--;
    break;
  }
  v->parent = NULL;
  rk_window *w = rk__view_window(p);
  if (w && v->placed) rk__window_damage(w, (rk_rect){ rk__window_rect(p).x + v->pframe.x - 8, rk__window_rect(p).y + v->pframe.y - 8, v->pframe.w + 16, v->pframe.h + 16 });
  if (w) {
    for (rk_view *f = w->focus; f; f = f->parent) if (f == v) { w->focus = NULL; break; }
    for (rk_view *f = w->focus_back; f; f = f->parent) if (f == v) { w->focus_back = NULL; break; }
    if (w->hover == v) w->hover = NULL;
    if (w->pressed == v) w->pressed = NULL;
    if (w->capture == v) w->capture = NULL;
    for (rk_view *f = w->drag_over; f; f = f->parent) if (f == v) { w->drag_over = NULL; break; }
  }
  rk__view_relayout_up(p);
  rk_release(v);
}

void rk__view_insert(rk_view *parent, rk_view *child, int index) {
  assert(parent && child && parent != child);
  rk_retain(child);
  if (child->parent) rk__view_detach(child);
  parent->kids = rk__grow(parent->kids, &parent->kcap, parent->nkids + 1, sizeof *parent->kids);
  index = RK_CLAMP(index, 0, parent->nkids);
  memmove(parent->kids + index + 1, parent->kids + index, (size_t)(parent->nkids - index) * sizeof *parent->kids);
  parent->kids[index] = child;
  parent->nkids++;
  child->parent = parent;
  child->leaving = false;
  /* added inside an ENTER block, into a window: it builds in once the layout has placed it */
  if (rk__animating_block() && rk__current_motion() == RK_MOTION_ENTER && rk__view_window(parent)) {
    child->entering = true;
    child->frame.w = child->frame.h = 0;
  }
  rk__view_relayout_up(child);
}

void rk__view_add(rk_view *parent, rk_view *child) { rk__view_insert(parent, child, parent->nkids); }

/* The public add takes the caller's reference: a new view goes into the tree and belongs to it. A view already in a
   tree is moved: the pointer to it was borrowed, so there is nothing to take over. */
rk_view *rk_insert(rk_view *parent, rk_view *child, int index) {
  if (!child) return NULL;
  if (!rk__is_object(parent, &rk__view_type) || !rk__is_object(child, &rk__view_type) || parent == child) {
    rk__error("rk_insert: %s", parent == child ? "a view cannot hold itself" : "not a view");
    if (rk__is_object(child, &rk__view_type) && !child->parent) rk_release(child);
    return NULL;
  }
  for (rk_view *a = parent; a; a = a->parent) if (a == child) { rk__error("rk_insert: a view cannot hold its own parent"); return NULL; }
  bool moved = child->parent != NULL;
  rk__view_insert(parent, child, index);
  if (!moved) rk_release(child);
  return child;
}

rk_view *rk_add(rk_view *parent, rk_view *child) { return rk_insert(parent, child, parent ? parent->nkids : 0); }

static void left_done(rk_view *v, void *user) { (void)user; if (v->leaving) rk__view_detach(v); }

void rk_view_remove(rk_view *child) {
  if (!child->parent) return;
  if (rk__animating_block() && rk__current_motion() == RK_MOTION_LEAVE && rk__view_visible(child) && !rk__reduced()) {
    if (child->leaving) return;                            /* already on its way out */
    /* it leaves first; the layout already lets the others close the gap after it */
    child->leaving = true;
    double d = rk__motion_duration(RK_MOTION_LEAVE, rk__is_tile(child));
    float from = child->popacity, to = 0;
    rk__animate(child, RK_PROP_OPACITY, &from, &to, 1, d, 0, rk__ease_out);
    rk_transform t0 = child->ptransform, t1 = t0;
    if (rk__is_tile(child)) { t1.scale_x = t1.scale_y = 0.9f; } else t1.translate_x += 12;
    rk__animate_transform_cb(child, &t0, &t1, d, 0, rk__ease_out, left_done, NULL);
    rk__view_relayout_up(child->parent);
    return;
  }
  rk__view_detach(child);
}

bool rk__view_within(const rk_view *v, const rk_view *ancestor) {
  for (; v; v = v->parent) if (v == ancestor) return true;
  return false;
}

rk_view *rk_view_parent(rk_view *v) { return v->parent; }
int rk_view_count(rk_view *v) { return v->nkids; }
rk_view *rk_view_child(rk_view *v, int i) { return i >= 0 && i < v->nkids ? v->kids[i] : NULL; }
void rk_view_set_name(rk_view *v, const char *name) { rk__setstr(&v->name, name); }
void rk_view_set_menu(rk_view *v, rk_view *menu) { rk_release(v->menu); v->menu = menu; }   /* takes menu */

/* By name; a button is named by its label until it is given another. */
rk_view *rk_view_find(rk_view *root, const char *name) {
  if (!root || !name) return NULL;
  if (root->name && !strcmp(root->name, name)) return root;
  for (int i = 0; i < root->nkids; i++) { rk_view *f = rk_view_find(root->kids[i], name); if (f) return f; }
  return NULL;
}

bool rk__view_visible(rk_view *v) {
  for (; v; v = v->parent) { if (v->hidden || v->popacity <= 0.001f) return false; if (v->window) return true; }
  return false;
}

/* Geometry -------------------------------------------------------------------------------------------------------- */

rk_rect rk_view_frame(rk_view *v) { return v->frame; }
rk_rect rk_view_bounds(rk_view *v) { rk_rect r = { 0, 0, v->frame.w, v->frame.h }; return r; }

void rk_view_set_frame(rk_view *v, rk_rect f) {
  rk__set_frame(v, f);
  v->needs_layout = true;
  rk__view_dirty(v);
}

rk_rect rk__window_rect(rk_view *v) {
  rk_rect r = { 0, 0, v->pframe.w, v->pframe.h };
  for (rk_view *p = v; p; p = p->parent) { r.x += p->pframe.x + p->ptransform.translate_x; r.y += p->pframe.y + p->ptransform.translate_y; }
  return r;
}

rk_point rk_view_to_window(rk_view *v, rk_point p) {
  rk_rect r = rk__window_rect(v);
  rk_point o = { r.x + p.x, r.y + p.y };
  return o;
}

/* Look ------------------------------------------------------------------------------------------------------------ */

rk_color rk__resolve_fill(rk_view *v) {
  if (v->fill_role) return rk_color_of(v->fill_role);
  return v->fill[rk__dark() ? 1 : 0];
}

static void set_fill_now(rk_view *v) {
  rk_color to = rk__resolve_fill(v);
  if (rk__animating_block() && v->pfill != to && rk__view_visible(v)) {
    float a[4], b[4];
    for (int k = 0; k < 4; k++) { a[k] = (float)((v->pfill >> (24 - 8 * k)) & 255); b[k] = (float)((to >> (24 - 8 * k)) & 255); }
    if (!(v->pfill & 255)) { a[0] = b[0]; a[1] = b[1]; a[2] = b[2]; }     /* from nothing: fade in the colour */
    rk_motion m = rk__current_motion();
    rk__animate(v, RK_PROP_FILL, a, b, 4, rk__motion_duration(m, false), 0, rk__motion_ease(m));
  } else v->pfill = to;
  rk__view_dirty(v);
}

void rk_view_set_fill(rk_view *v, rk_role role) { v->fill_role = role; set_fill_now(v); }
void rk_view_set_fill_color(rk_view *v, rk_color light, rk_color dark) { v->fill_role = RK_ROLE_NONE; v->fill[0] = light; v->fill[1] = dark ? dark : light; set_fill_now(v); }
void rk_view_set_radius(rk_view *v, float r) { v->radius = r; rk__view_dirty(v); }
void rk_view_set_rules(rk_view *v, uint32_t rules) { v->rule_top = (rules & RK_RULE_TOP) != 0; v->rule_bottom = (rules & RK_RULE_BOTTOM) != 0; rk__view_dirty(v); }
void rk_view_set_clip(rk_view *v, bool clip) { v->clip = clip; rk__view_dirty(v); }
float rk_view_opacity(rk_view *v) { return v->opacity; }
rk_transform rk_view_transform(rk_view *v) { return v->transform; }
bool rk_view_is_hidden(rk_view *v) { return v->hidden; }

void rk_view_set_opacity(rk_view *v, float o) {
  o = RK_CLAMP(o, 0.0f, 1.0f);
  v->opacity = o;
  if (rk__animating_block() && rk__view_visible(v)) {
    rk_motion m = rk__current_motion();
    float from = v->popacity;
    rk__animate(v, RK_PROP_OPACITY, &from, &o, 1, rk__motion_duration(m, false), 0, rk__motion_ease(m));
  } else { rk__anim_cancel(v, RK_PROP_OPACITY); v->popacity = o; }
  rk__view_dirty(v);
}

void rk_view_set_transform(rk_view *v, rk_transform t) {
  v->transform = t;
  if (rk__animating_block() && rk__view_visible(v)) {
    rk_motion m = rk__current_motion();
    rk_transform from = v->ptransform;
    rk__animate_transform(v, &from, &t, rk__motion_duration(m, false), 0, rk__motion_ease(m));
  } else { rk__anim_cancel(v, RK_PROP_TRANSFORM); v->ptransform = t; }
  rk__view_dirty(v);
}

void rk_view_set_hidden(rk_view *v, bool hidden) {
  if (v->hidden == hidden) return;
  v->hidden = hidden;
  if (v->parent) rk__view_relayout_up(v->parent);
  rk__view_dirty(v);
}

/* Layout properties ----------------------------------------------------------------------------------------------- */

#define RELAYOUT(v) rk__view_relayout_up(v)
void rk_view_set_layout(rk_view *v, rk_layout l) { v->layout = l; RELAYOUT(v); }
void rk_view_set_padding(rk_view *v, rk_insets p) { v->padding = p; RELAYOUT(v); }
void rk_view_set_gap(rk_view *v, float g) { v->gap = g; RELAYOUT(v); }
void rk_view_set_align(rk_view *v, rk_align main, rk_align cross) { v->main = main; v->cross = cross; RELAYOUT(v); }
void rk_view_set_grow(rk_view *v, float g) { v->grow = g; RELAYOUT(v); }
void rk_view_set_size(rk_view *v, float w, float h) { v->fixed_w = w; v->fixed_h = h; RELAYOUT(v); }
void rk_view_set_min_size(rk_view *v, float w, float h) { v->min_w = w; v->min_h = h; RELAYOUT(v); }
void rk_view_set_span(rk_view *v, int c, int r) { v->span_c = RK_MAX(c, 1); v->span_r = RK_MAX(r, 1); RELAYOUT(v); }
void rk_view_relayout(rk_view *v) { RELAYOUT(v); }
void rk_view_redraw(rk_view *v) { rk__view_dirty(v); }

/* Behaviour ------------------------------------------------------------------------------------------------------- */

void rk_view_set_press(rk_view *v, rk_press p) { v->press = p; if (p != RK_PRESS_NONE) v->focusable = true; }
void rk_view_set_focusable(rk_view *v, bool f) { v->focusable = f; }
void rk_view_on_activate(rk_view *v, void (*fn)(rk_view *, void *), void *user) {
  v->on_activate = fn; v->activate_user = user;
  if (fn && v->press == RK_PRESS_NONE) v->press = rk__is_tile(v) ? RK_PRESS_TILE : RK_PRESS_FLAT;
  if (fn) v->focusable = true;
  else if (!v->cls) { v->press = RK_PRESS_NONE; v->focusable = false; }   /* a plain view no longer acts */
}

void rk_view_set_cursor(rk_view *v, rk_cursor cursor) { if (v) v->cursor = (uint8_t)cursor; }   /* the same numbers as RK__CURSOR_ */

/* A custom view's own number in motion: it glides to target with the motion of the block it is set in (state's,
   outside one), and draw reads where it is. */
void rk_view_animate_value(rk_view *v, float target) {
  if (!v) return;
  rk_motion m = rk__animating_block() ? rk__current_motion() : RK_MOTION_STATE;
  double d = rk__reduced() ? 0 : rk__motion_duration(m, false);
  float from = v->pvalue;
  v->value = target;
  if (d <= 0 || !rk__view_window(v)) { rk__anim_cancel(v, RK_PROP_VALUE); v->pvalue = target; rk__view_dirty(v); return; }
  rk__animate(v, RK_PROP_VALUE, &from, &target, 1, d, 0, rk__ease_base);
}
float rk_view_value(rk_view *v) { return v ? v->pvalue : 0; }

bool rk__view_disabled(const rk_view *v) {
  for (; v; v = v->parent) if (v->disabled) return true;
  return false;
}

/* Disabled, a view and what it holds take no presses, keys or values and are passed over by Tab - unless it has a
   tooltip, its reason: then Tab still reaches it, so the keyboard finds out why. Built-in views draw it greyed. */
void rk_view_set_enabled(rk_view *v, bool enabled) {
  if (!v || v->disabled == !enabled) return;
  v->disabled = !enabled;
  rk_window *w = rk__view_window(v);
  if (w && !enabled && rk__view_within(w->focus, v) && !rk__tip_owner(w->focus)) rk_window_focus(w, NULL);
  if (w && !enabled && rk__view_within(w->pressed, v)) rk__window_pointer_cancel(w);
  rk__view_dirty(v);
}
bool rk_view_is_enabled(rk_view *v) { return v && !rk__view_disabled(v); }
void rk_view_set_user(rk_view *v, void *user, void (*free_user)(void *)) {
  if (v->free_user && v->user != user) v->free_user(v->user);
  v->user = user; v->free_user = free_user;
}
void *rk_view_user(rk_view *v) { return v->user; }
void rk_view_set_access(rk_view *v, rk_access_role role, const char *label) { v->access = role; rk__setstr(&v->access_label, label); }
void rk_view_set_access_value(rk_view *v, const char *value) { rk__setstr(&v->access_value, value); }

/* Measuring ------------------------------------------------------------------------------------------------------- */

float rk__host_width(rk_view *v) {
  rk_window *w = rk__view_window(v);
  return w ? w->size.w : 1280;
}

static bool in_flow(rk_view *k) { return !k->hidden && !k->leaving; }

typedef struct grid { int cols, rows; float unit, colw, gap; } grid;   /* colw: a column's width, unit stretched when compact */

/* Dense packing: each tile takes the first free cell that fits its span, row by row, as grid-auto-flow: dense.
   Sideways the cells are tile-units; compact (a window under 760 px wide or 481 px high, as the web's place) there are
   at least two columns, as many tile-units as fit, stretched to fill the row; rows stay a tile-unit high. */
static grid tile_grid(rk_view *v, float width, rk_rect *out) {
  grid g;
  g.gap = v->gap > 0 ? v->gap : rk_metric_of(RK_SPACE_S);
  g.unit = g.colw = rk_metric_of(RK_TILE_UNIT);
  g.cols = RK_MAX(1, (int)floorf((width + g.gap + 0.01f) / (g.unit + g.gap)));
  rk_window *win = rk__view_window(v);
  if (win && (win->size.w < 760 || win->size.h < 481) && isfinite(width)) {
    g.cols = RK_MAX(2, g.cols);
    g.colw = (width - (g.cols - 1) * g.gap) / g.cols;
  }
  g.rows = 0;
  int cols = g.cols, bcap = 0, rcap = 0, low = 0, n = 0;     /* low: no free cell above this row; rcap rows in bcap bytes */
  unsigned char *used = NULL;                              /* rows of cols cells, grown as the tiles need */
  for (int i = 0; i < v->nkids; i++) {
    rk_view *k = v->kids[i];
    if (!in_flow(k)) continue;
    int sc = RK_CLAMP(k->span_c, 1, cols), sr = RK_MAX(k->span_r, 1), at_r = -1, at_c = -1;
    for (int r = low; at_r < 0; r++) {
      if (r + sr > rcap) { used = rk__grow(used, &bcap, (r + sr) * cols, 1); rcap = bcap / cols; }
      for (int c = 0; c + sc <= cols && at_r < 0; c++) {
        bool free_ = true;
        for (int y = r; y < r + sr && free_; y++) for (int x = c; x < c + sc; x++) if (used[y * cols + x]) { free_ = false; break; }
        if (free_) { at_r = r; at_c = c; }
      }
    }
    for (int y = at_r; y < at_r + sr; y++) memset(used + y * cols + at_c, 1, (size_t)sc);
    while (low < rcap && memchr(used + low * cols, 0, (size_t)cols) == NULL) low++;
    g.rows = RK_MAX(g.rows, at_r + sr);
    if (out) out[n] = (rk_rect){ at_c * (g.colw + g.gap), at_r * (g.unit + g.gap), sc * g.colw + (sc - 1) * g.gap, sr * g.unit + (sr - 1) * g.gap };
    n++;
  }
  free(used);
  return g;
}

static rk_size measure(rk_view *v, rk_size avail);

/* A stack measures a child, then lays it out, which measures its children again: the same question in the same layout
   pass is answered once. */
rk_size rk__measure(rk_view *v, rk_size avail) {
  if (v->measured_gen == rk__measure_gen && v->measured_for.w == avail.w && v->measured_for.h == avail.h) return v->measured;
  rk_size s = measure(v, avail);
  v->measured_gen = rk__measure_gen; v->measured_for = avail; v->measured = s;
  return s;
}

static rk_size measure(rk_view *v, rk_size avail) {
  rk_size s = { 0, 0 };
  rk_insets p = v->padding;
  float iw = avail.w - p.left - p.right, ih = avail.h - p.top - p.bottom;
  if (v->fixed_w > 0) iw = v->fixed_w - p.left - p.right;
  if (v->cls && v->cls->pub.measure) {
    s = v->cls->pub.measure(v, (rk_size){ v->fixed_w > 0 ? v->fixed_w : avail.w, v->fixed_h > 0 ? v->fixed_h : avail.h }, v->state);
  } else switch (v->layout) {
    case RK_LAYOUT_VSTACK: {
      int n = 0;
      for (int i = 0; i < v->nkids; i++) {
        rk_view *k = v->kids[i];
        if (!in_flow(k)) continue;
        rk_size ks = rk__measure(k, (rk_size){ iw, INFINITY });
        s.w = RK_MAX(s.w, ks.w); s.h += ks.h; n++;
      }
      s.h += RK_MAX(0, n - 1) * v->gap;
      s.w += p.left + p.right; s.h += p.top + p.bottom;
      break;
    }
    case RK_LAYOUT_HSTACK: {
      int n = 0;
      for (int i = 0; i < v->nkids; i++) {
        rk_view *k = v->kids[i];
        if (!in_flow(k)) continue;
        rk_size ks = rk__measure(k, (rk_size){ INFINITY, ih });
        s.w += ks.w; s.h = RK_MAX(s.h, ks.h); n++;
      }
      s.w += RK_MAX(0, n - 1) * v->gap;
      s.w += p.left + p.right; s.h += p.top + p.bottom;
      break;
    }
    case RK_LAYOUT_TILES: {
      float w = isfinite(iw) ? iw : 4 * rk_metric_of(RK_TILE_UNIT) + 3 * rk_metric_of(RK_SPACE_S);
      grid g = tile_grid(v, w, NULL);
      s.w = RK_MIN(w, g.cols * g.colw + (g.cols - 1) * g.gap) + p.left + p.right;
      s.h = (g.rows ? g.rows * g.unit + (g.rows - 1) * g.gap : 0) + p.top + p.bottom;
      break;
    }
    case RK_LAYOUT_OVERLAY:
      for (int i = 0; i < v->nkids; i++) {
        rk_view *k = v->kids[i];
        if (!in_flow(k)) continue;
        rk_size ks = rk__measure(k, (rk_size){ iw, ih });
        s.w = RK_MAX(s.w, ks.w); s.h = RK_MAX(s.h, ks.h);
      }
      s.w += p.left + p.right; s.h += p.top + p.bottom;
      break;
    case RK_LAYOUT_MANUAL: default:
      for (int i = 0; i < v->nkids; i++) {
        rk_view *k = v->kids[i];
        if (!in_flow(k)) continue;
        s.w = RK_MAX(s.w, k->frame.x + k->frame.w); s.h = RK_MAX(s.h, k->frame.y + k->frame.h);
      }
      break;
  }
  if (v->fixed_w > 0) s.w = v->fixed_w;
  if (v->fixed_h > 0) s.h = v->fixed_h;
  s.w = RK_MAX(s.w, v->min_w); s.h = RK_MAX(s.h, v->min_h);
  return s;
}

rk_size rk_view_measure(rk_view *v, rk_size avail) { return rk__measure(v, avail); }

/* Layout ---------------------------------------------------------------------------------------------------------- */

/* A stack, as CSS flex: the views without grow take their natural size; those with grow share what is left by their
   weights (flex: 1 1 0), never less than their content - except a view that scrolls (a list, a table, a tree, a
   scroll view), which may shrink to its minimum and so scrolls instead of growing past the window. */
static bool weighted(const rk_view *k, bool column) { return k->grow > 0 && (column ? k->fixed_h : k->fixed_w) <= 0; }   /* a fixed size wins */

static void stack(rk_view *v, bool column) {
  rk_insets p = v->padding;
  float W = v->frame.w - p.left - p.right, H = v->frame.h - p.top - p.bottom;
  float main_len = column ? H : W, cross_len = column ? W : H;
  enum { LOCAL = 32 };
  float sbuf[LOCAL * 3]; int ibuf[LOCAL];
  int cap = 0;
  for (int i = 0; i < v->nkids; i++) if (in_flow(v->kids[i])) cap++;
  float *sizes = cap > LOCAL ? rk__alloc((size_t)cap * 3 * sizeof *sizes) : sbuf, *cross = sizes + cap, *floor_ = cross + cap;
  int *idx = cap > LOCAL ? rk__alloc((size_t)cap * sizeof *idx) : ibuf;
  float fixed = 0, weights = 0;
  int n = 0;
  for (int i = 0; i < v->nkids; i++) {
    rk_view *k = v->kids[i];
    if (!in_flow(k)) continue;
    rk_size ks = rk__measure(k, column ? (rk_size){ W, INFINITY } : (rk_size){ INFINITY, H });
    sizes[n] = column ? ks.h : ks.w; cross[n] = column ? ks.w : ks.h;
    if (weighted(k, column)) { weights += k->grow; floor_[n] = k->scrolls ? (column ? k->min_h : k->min_w) : sizes[n]; }
    else fixed += sizes[n];
    idx[n++] = i;
  }
  fixed += RK_MAX(0, n - 1) * v->gap;
  if (weights > 0 && isfinite(main_len)) {
    /* share what is left by weight; a view held at its floor leaves the share to the others (the freeze of CSS) */
    float left = main_len - fixed, w = weights;
    bool again = true;
    for (int j = 0; j < n; j++) if (weighted(v->kids[idx[j]], column)) sizes[j] = -1;   /* not frozen yet */
    while (again) {
      again = false;
      for (int j = 0; j < n; j++) {
        rk_view *k = v->kids[idx[j]];
        if (!weighted(k, column) || sizes[j] >= 0) continue;
        if (RK_MAX(left, 0.0f) * k->grow / w < floor_[j]) { sizes[j] = floor_[j]; left -= floor_[j]; w -= k->grow; again = true; }
      }
    }
    for (int j = 0; j < n; j++) if (sizes[j] < 0) sizes[j] = w > 0 ? RK_MAX(left, 0.0f) * v->kids[idx[j]]->grow / w : 0;
  }
  float used = RK_MAX(0, n - 1) * v->gap;
  for (int j = 0; j < n; j++) used += sizes[j];
  rk_align main_a = v->main, cross_a = v->cross != RK_ALIGN_AUTO ? v->cross : column ? RK_ALIGN_STRETCH : RK_ALIGN_START;
  float free_ = main_len - used, pos = 0, extra_gap = 0;
  if (free_ > 0) {
    if (main_a == RK_ALIGN_CENTER) pos = free_ / 2;
    else if (main_a == RK_ALIGN_END) pos = free_;
    else if (main_a == RK_ALIGN_BETWEEN && n > 1) extra_gap = free_ / (n - 1);
  }
  for (int j = 0; j < n; j++) {
    rk_view *k = v->kids[idx[j]];
    float cs = cross_len, co = 0;
    float fixed_cross = column ? k->fixed_w : k->fixed_h;
    if (cross_a != RK_ALIGN_STRETCH || fixed_cross > 0 || k->hug) {   /* a fixed size, or a button, is not stretched */
      cs = RK_MIN(cross[j], cross_len);
      if (cross_a == RK_ALIGN_CENTER) co = (cross_len - cs) / 2;
      else if (cross_a == RK_ALIGN_END) co = cross_len - cs;
    }
    rk_rect f = column ? (rk_rect){ p.left + co, p.top + pos, cs, sizes[j] } : (rk_rect){ p.left + pos, p.top + co, sizes[j], cs };
    rk__set_frame(k, f);
    pos += sizes[j] + v->gap + extra_gap;
  }
  if (sizes != sbuf) free(sizes);
  if (idx != ibuf) free(idx);
}

void rk__stack(rk_view *v, bool column) { stack(v, column); }

void rk__layout(rk_view *v) {
  if (v->cls && v->cls->pub.layout) v->cls->pub.layout(v, v->state);
  else switch (v->layout) {
    case RK_LAYOUT_VSTACK: stack(v, true); break;
    case RK_LAYOUT_HSTACK: stack(v, false); break;
    case RK_LAYOUT_TILES: {
      rk_rect *rects = rk__alloc((size_t)RK_MAX(v->nkids, 1) * sizeof *rects);
      rk_insets p = v->padding;
      tile_grid(v, v->frame.w - p.left - p.right, rects);
      int n = 0;
      for (int i = 0; i < v->nkids; i++) {
        rk_view *k = v->kids[i];
        if (!in_flow(k)) continue;
        rk_rect r = rects[n++];
        r.x += p.left; r.y += p.top;
        k->index_hint = n - 1;
        rk__set_frame(k, r);
      }
      free(rects);
      break;
    }
    case RK_LAYOUT_OVERLAY: {
      rk_insets p = v->padding;
      for (int i = 0; i < v->nkids; i++) if (in_flow(v->kids[i]))
        rk__set_frame(v->kids[i], (rk_rect){ p.left, p.top, v->frame.w - p.left - p.right, v->frame.h - p.top - p.bottom });
      break;
    }
    default: break;
  }
  v->needs_layout = false;
  for (int i = 0; i < v->nkids; i++) if (!v->kids[i]->hidden) rk__layout(v->kids[i]);
}

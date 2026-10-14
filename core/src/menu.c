/* menu.c: menus, as states.md's floating surfaces: rk-bg, a 1px text-contrast-medium edge, radius-s, no shadow;
   below the view that opened it, aligned to its leading edge, above it where there is no room, never outside the
   window; a fade and a 4px shift away from the opener over duration-float, gone at once when it closes.

   One item a row at the density's row height: a lowercase verb or option, a filled square for a chosen option, a key
   hint at the end, hairlines between groups, a group's label. Keys: Up and Down (wrapping), Home and End, a letter
   jumps to the next item starting with it, Enter or Space chooses, Escape closes and gives focus back, Tab closes and
   moves on; a press outside closes. A choice closes the menu and then runs the item's handler; an item with a check
   toggles and stays open. The menu is one view that draws its rows; it floats in the window's top layer.

   For the controls built on it (rk_select, a field's suggestions): an item can carry a check mark without a box (the
   chosen option), the menu can be at least as wide as its opener and start at an item, and it can float without
   taking focus - the field it belongs to keeps typing and moves the active row itself. */
#include "rk_internal.h"

enum { ITEM, SEPARATOR, LABEL };

typedef struct menu_item {
  int kind, check;                                         /* check: -1 no square, 0 empty, 1 filled, 2 a mark: the chosen option */
  bool disabled;
  char *label, *key;
  void (*fn)(rk_view *menu, int item, void *user); void *user;
} menu_item;

typedef struct menu {
  menu_item *items; int n, cap;
  int active;                                              /* the row under the pointer or the keys; -1 none */
  bool hover;                                              /* the pointer is on it: the active row shows as hovered */
  rk_view *from;                                           /* what opened it; focus goes back there */
  rk_view *view;
  float min_w; int prefer;                                 /* at least this wide; the item it starts at (-1: the first) */
  bool passive;                                            /* floats without focus: its opener moves the active row */
} menu;

static float pad_y(void) { return rk_metric_of(RK_SPACE_XS); }
static float item_h(const menu_item *it) {
  if (it->kind == SEPARATOR) return 2 * rk_metric_of(RK_SPACE_XS) + 1;
  if (it->kind == LABEL) { rk_textfmt f = rk__textfmt(RK_TEXT_LABEL, 0); return rk_metric_of(RK_SPACE_S) + f.size * f.line + rk_metric_of(RK_SPACE_XS); }
  return rk_metric_of(RK_ROW_HEIGHT);
}
static bool choosable(const menu_item *it) { return it->kind == ITEM; }

static rk_size menu_measure(rk_view *v, rk_size avail, void *st) {
  (void)v; (void)avail;
  menu *m = st;
  rk_textfmt f = rk__textfmt(RK_TEXT_BODY, 0), k = rk__textfmt(RK_TEXT_META, 0);
  float w = 0, h = 2 * pad_y(), side = rk_metric_of(RK_SPACE_M);
  bool squares = false;
  for (int i = 0; i < m->n; i++) if (m->items[i].check >= 0) squares = true;
  for (int i = 0; i < m->n; i++) {
    menu_item *it = &m->items[i];
    float iw = it->label ? rk__text_width(it->label, -1, it->kind == LABEL ? &k : &f) : 0;
    if (squares && it->kind == ITEM) iw += 10 + rk_metric_of(RK_SPACE_S);
    if (it->key) iw += rk_metric_of(RK_SPACE_L) + rk__text_width(it->key, -1, &k);
    w = RK_MAX(w, iw);
    h += item_h(it);
  }
  return (rk_size){ RK_MAX(m->min_w, RK_CLAMP(ceilf(w + 2 * side), 192.0f, 384.0f)), h };
}

static void menu_draw(rk_view *v, rk_canvas *c, void *st) {
  menu *m = st;
  float W = v->pframe.w, H = v->pframe.h, r = rk_metric_of(RK_RADIUS_S), side = rk_metric_of(RK_SPACE_M);
  rk_canvas_fill_round_rect(c, (rk_rect){ 0, 0, W, H }, r, rk_color_of(RK_ROLE_TEXT_MEDIUM));
  rk_canvas_fill_round_rect(c, (rk_rect){ 1, 1, W - 2, H - 2 }, r - 1, rk_color_of(RK_ROLE_BG));
  rk_window *w = rk__view_window(v);
  bool keys = (w && w->focus == v && w->focus_visible) || m->passive;
  bool squares = false;
  for (int i = 0; i < m->n; i++) if (m->items[i].check >= 0) squares = true;
  rk_textfmt f = rk__textfmt(RK_TEXT_BODY, 0), k = rk__textfmt(RK_TEXT_META, 0), l = rk__textfmt(RK_TEXT_LABEL, 0);
  float y = pad_y();
  for (int i = 0; i < m->n; i++) {
    menu_item *it = &m->items[i];
    float h = item_h(it);
    if (it->kind == SEPARATOR) rk_canvas_fill_rect(c, (rk_rect){ 0, y + rk_metric_of(RK_SPACE_XS), W, 1 }, rk_color_of(RK_ROLE_TEXT_LOW));
    else if (it->kind == LABEL) rk__text_draw(c, it->label, -1, side, y + rk_metric_of(RK_SPACE_S), &l, rk_color_of(RK_ROLE_TEXT_HIGH));
    else {
      if (i == m->active && (m->hover || keys) && !it->disabled) rk_canvas_fill_rect(c, (rk_rect){ 1, y, W - 2, h }, rk_color_of(RK_ROLE_HOVER));
      if (i == m->active && keys) rk__focus_ring(c, (rk_rect){ 1, y, W - 2, h }, 0);
      rk_color ink = rk_color_of(it->disabled ? RK_ROLE_DISABLED : RK_ROLE_TEXT);
      float x = side;
      if (squares) {
        if (it->check == 2) {                                 /* the chosen option: a mark, no box */
          rk_point mark[3] = { { x, y + h / 2 - 1 }, { x + 3.5f, y + h / 2 + 2.5f }, { x + 10, y + h / 2 - 4 } };
          rk_canvas_polyline(c, mark, 3, 2, ink);
        } else if (it->check >= 0) {
          rk_rect sq = { x, y + (h - 10) / 2, 10, 10 };
          if (it->check) rk_canvas_fill_rect(c, sq, ink); else rk_canvas_stroke_rect(c, sq, 1, ink);
        }
        x += 10 + rk_metric_of(RK_SPACE_S);
      }
      if (it->label) rk__text_draw(c, it->label, -1, x, y + (h - f.size * f.line) / 2, &f, ink);
      if (it->key) {
        float kw = rk__text_width(it->key, -1, &k);
        rk__text_draw(c, it->key, -1, W - side - kw, y + (h - k.size * k.line) / 2, &k, rk_color_of(RK_ROLE_TEXT_HIGH));
      }
    }
    y += h;
  }
}

static void close_menu(rk_view *v, bool give_back);

static int row_at_y(menu *m, float y) {
  float top = pad_y();
  for (int i = 0; i < m->n; i++) { float h = item_h(&m->items[i]); if (y >= top && y < top + h) return i; top += h; }
  return -1;
}

static void menu_step(rk_view *v, menu *m, int dir) {
  for (int k = 0, i = m->active; k < m->n; k++) {
    i = ((i < 0 ? (dir > 0 ? -1 : 0) : i) + dir + m->n) % m->n;
    if (choosable(&m->items[i])) { m->active = i; break; }
  }
  rk__view_dirty(v);
}

static void choose(rk_view *v, menu *m, int i) {
  if (i < 0 || i >= m->n || !choosable(&m->items[i]) || m->items[i].disabled) return;
  menu_item *it = &m->items[i];
  void (*fn)(rk_view *, int, void *) = it->fn; void *user = it->user;
  if (it->check == 0 || it->check == 1) { it->check = !it->check; rk__view_dirty(v); if (fn) fn(v, i, user); return; }   /* a check: stays open */
  rk_retain(v);
  rk_view *from = m->from ? rk_retain(m->from) : NULL;
  close_menu(v, true);
  m->from = from;                                          /* the opener, for the handler */
  if (fn) fn(v, i, user);
  m->from = NULL;
  rk_release(from);
  rk_release(v);
}

static bool menu_event(rk_view *v, const rk_event *e, void *st) {
  menu *m = st;
  switch (e->type) {
    case RK_EVENT_POINTER_MOVE: {
      int i = row_at_y(m, e->position.y);
      m->hover = i >= 0;
      if (i >= 0 && choosable(&m->items[i]) && i != m->active) { m->active = i; rk__view_dirty(v); }
      return true;
    }
    case RK_EVENT_POINTER_DOWN: return true;
    case RK_EVENT_POINTER_UP: choose(v, m, row_at_y(m, e->position.y)); return true;
    case RK_EVENT_KEY_DOWN:
      switch (e->key) {
        case RK_KEY_DOWN: menu_step(v, m, 1); return true;
        case RK_KEY_UP: menu_step(v, m, -1); return true;
        case RK_KEY_HOME: m->active = -1; menu_step(v, m, 1); return true;
        case RK_KEY_END: m->active = -1; menu_step(v, m, -1); return true;
        case RK_KEY_ENTER: case RK_KEY_SPACE: choose(v, m, m->active); return true;
        case RK_KEY_ESCAPE: rk_menu_close(v); return true;
        default:
          if (e->key >= 'A' && e->key <= 'Z' && !(e->mods & (RK_MOD_CTRL | RK_MOD_ALT | RK_MOD_SUPER))) {   /* the next item with that letter */
            for (int k = 1; k <= m->n; k++) {
              int i = ((m->active < 0 ? -1 : m->active) + k) % m->n;
              const char *s = m->items[i].label;
              if (choosable(&m->items[i]) && s && (s[0] == (char)e->key || s[0] == (char)(e->key + 32))) { m->active = i; rk__view_dirty(v); break; }
            }
            return true;
          }
          return false;
      }
    case RK_EVENT_BLUR: if (v->parent) close_menu(v, false); return false;   /* Tab, or a press elsewhere */
    default: return false;
  }
}

static void menu_destroy(void *st) {
  menu *m = st;
  for (int i = 0; i < m->n; i++) { free(m->items[i].label); free(m->items[i].key); }
  rk_release(m->from);
  free(m->items); free(m);
}

static const rk_class menu_class = { { sizeof(rk_view_class), "rk_menu", menu_draw, menu_measure, NULL, menu_event, menu_destroy }, NULL, NULL };

static menu *menu_of(rk_view *v) { return v && v->cls == &menu_class ? v->state : NULL; }

rk_view *rk_menu(void) {
  menu *m = rk__alloc(sizeof *m);
  m->active = -1; m->prefer = -1;
  rk_view *v = rk__view_create(&menu_class, m);
  m->view = v;
  v->focusable = true; v->draws_focus = true;                /* the active row shows focus, not the whole menu */
  v->access = RK_ACCESS_MENU;
  return v;
}

static menu_item *add_item(menu *m, int kind, const char *label) {
  m->items = rk__grow(m->items, &m->cap, m->n + 1, sizeof *m->items);
  menu_item *it = &m->items[m->n++];
  memset(it, 0, sizeof *it);
  it->kind = kind; it->check = -1; it->label = rk__strdup(label);
  return it;
}

int rk_menu_add(rk_view *v, const char *label, const char *key, void (*fn)(rk_view *, int, void *), void *user) {
  menu *m = menu_of(v);
  if (!m) return -1;
  menu_item *it = add_item(m, ITEM, label);
  it->key = rk__strdup(key); it->fn = fn; it->user = user;
  rk__view_relayout_up(v);
  return m->n - 1;
}

void rk_menu_add_separator(rk_view *v) { menu *m = RK__EXPECT(menu_of, v, "a menu"); if (m) { add_item(m, SEPARATOR, NULL); rk__view_relayout_up(v); } }
void rk_menu_add_label(rk_view *v, const char *label) { menu *m = RK__EXPECT(menu_of, v, "a menu"); if (m) { add_item(m, LABEL, label); rk__view_relayout_up(v); } }

void rk_menu_set_check(rk_view *v, int i, rk_checkmark check) {
  menu *m = RK__EXPECT(menu_of, v, "a menu");
  if (!m || i < 0 || i >= m->n) return;
  m->items[i].check = check == RK_CHECKMARK_ON ? 1 : check == RK_CHECKMARK_OFF ? 0 : -1;
  rk__view_relayout_up(v); rk__view_dirty(v);
}
rk_checkmark rk_menu_check(rk_view *v, int i) { menu *m = RK__EXPECT(menu_of, v, "a menu"); int c = m && i >= 0 && i < m->n ? m->items[i].check : -1; return c < 0 ? RK_CHECKMARK_NONE : c ? RK_CHECKMARK_ON : RK_CHECKMARK_OFF; }
void rk_menu_set_enabled(rk_view *v, int i, bool enabled) { menu *m = RK__EXPECT(menu_of, v, "a menu"); if (m && i >= 0 && i < m->n) { m->items[i].disabled = !enabled; rk__view_dirty(v); } }
rk_view *rk_menu_opener(rk_view *v) { menu *m = RK__EXPECT(menu_of, v, "a menu"); return m ? m->from : NULL; }

/* Opens it in the window's top layer at a rect's bottom left (above it where there is no room below), on screen. */
static void open_by(rk_view *v, rk_view *from, rk_rect at, bool keys, bool passive) {
  menu *m = menu_of(v);
  rk_window *w = from ? rk__view_window(from) : NULL;
  if (!m || !w) return;
  m->passive = passive; v->focusable = !passive;
  if (v->parent) rk_menu_close(v);
  rk_retain(from); rk_release(m->from); m->from = from;    /* held while it is open */
  rk_size s = menu_measure(v, (rk_size){ INFINITY, INFINITY }, m);
  float gap = rk_metric_of(RK_SPACE_XS), edge = rk_metric_of(RK_SPACE_M);
  float x = RK_CLAMP(at.x, edge, RK_MAX(edge, w->size.w - edge - s.w)), y = at.y + at.h + gap;
  bool above = y + s.h > w->size.h - edge && at.y - gap - s.h >= edge;
  if (above) y = at.y - gap - s.h;
  y = RK_CLAMP(y, edge, RK_MAX(edge, w->size.h - edge - s.h));
  m->active = -1; m->hover = false;
  rk__view_add(w->floats, v);
  rk_view_set_frame(v, (rk_rect){ x, y, s.w, s.h });
  rk__layout_pending();
  if (!m->passive) {
    w->focus_visible = keys;
    rk_window_focus(w, v);
  }
  if (m->prefer >= 0 && m->prefer < m->n && choosable(&m->items[m->prefer])) m->active = m->prefer;
  else if (keys) menu_step(v, m, 1);
  /* it comes in from the opener's side: a fade and a 4px shift over duration-float */
  float o0 = 0, o1 = 1;
  rk_transform t0 = v->transform, t1 = v->transform;
  t0.translate_y = above ? 4 : -4;
  double d = rk_duration_of(RK_DURATION_FLOAT);
  rk__animate(v, RK_PROP_OPACITY, &o0, &o1, 1, d, 0, rk__ease_in);
  rk__animate_transform(v, &t0, &t1, d, 0, rk__ease_in);
}

void rk_menu_open(rk_view *v, rk_view *from) {
  if (!from) return;
  rk_window *w = rk__view_window(from);
  open_by(v, from, rk__window_rect(from), w && w->focus_visible, false);
}

void rk_menu_open_at(rk_view *v, rk_view *from, rk_point p) {
  if (!from) return;
  rk_rect r = rk__window_rect(from);
  open_by(v, from, (rk_rect){ r.x + p.x, r.y + p.y, 0, 0 }, false, false);
}

/* It leaves at once; focus goes back to what opened it unless it is moving elsewhere. */
static void close_menu(rk_view *v, bool give_back) {
  menu *m = menu_of(v);
  if (!m || !v->parent) return;
  rk_window *w = rk__view_window(v);
  rk__anim_cancel(v, RK_PROP_OPACITY); rk__anim_cancel(v, RK_PROP_TRANSFORM);
  v->popacity = v->opacity; v->ptransform = v->transform;
  rk_retain(v);
  rk__view_detach(v);
  if (give_back && w && m->from && rk__view_window(m->from) == w && !w->focus && m->from->focusable) rk_window_focus(w, m->from);
  rk_release(m->from);                                     /* held only while open: a button holds its menu, not back */
  m->from = NULL;
  rk_release(v);
}

void rk_menu_close(rk_view *v) { close_menu(v, true); }

/* Closes every open menu in the window, as a press outside them does. */
void rk__menus_close(rk_window *w) {
  while (w->floats->nkids) {
    rk_view *v = w->floats->kids[w->floats->nkids - 1];
    if (menu_of(v)) close_menu(v, false); else rk__view_detach(v);
  }
}

/* For the controls built on a menu --------------------------------------------------------------------------------- */

void rk__menu_mark(rk_view *v, int chosen) {
  menu *m = menu_of(v);
  if (!m) return;
  for (int i = 0; i < m->n; i++) if (m->items[i].kind == ITEM) m->items[i].check = i == chosen ? 2 : -1;
  if (chosen < 0 && m->n) m->items[0].check = -1;
  rk__view_dirty(v);
}

void rk__menu_shape(rk_view *v, float min_w, int prefer) { menu *m = menu_of(v); if (m) { m->min_w = min_w; m->prefer = prefer; } }

void rk_menu_clear(rk_view *v) {
  menu *m = RK__EXPECT(menu_of, v, "a menu");
  if (!m) return;
  for (int i = 0; i < m->n; i++) { free(m->items[i].label); free(m->items[i].key); }
  m->n = 0; m->active = -1;
  rk__view_relayout_up(v);
}

/* Floats below from without taking focus; from moves the active row and chooses. */
void rk__menu_show_passive(rk_view *v, rk_view *from, rk_rect below) {
  menu *m = menu_of(v);
  rk_window *w = from ? rk__view_window(from) : NULL;
  if (!m || !w || !m->n) return;
  bool was = v->parent != NULL;
  if (!was) {
    open_by(v, from, below, false, true);
    m->active = m->prefer >= 0 && m->prefer < m->n ? m->prefer : -1;
  } else {                                                 /* open already: its size follows its items */
    rk_size sz = menu_measure(v, (rk_size){ INFINITY, INFINITY }, m);
    rk_view_set_frame(v, (rk_rect){ v->frame.x, v->frame.y, sz.w, sz.h });
    if (m->active >= m->n) m->active = -1;
  }
  rk__view_dirty(v);
}

bool rk__menu_open(rk_view *v) { return v && v->parent != NULL; }
void rk__menu_move(rk_view *v, int dir) { menu *m = menu_of(v); if (m && m->n) { m->hover = false; menu_step(v, m, dir); } }
int rk__menu_active(rk_view *v) { menu *m = menu_of(v); return m ? m->active : -1; }
void rk__menu_choose(rk_view *v, int i) { menu *m = menu_of(v); if (m) choose(v, m, i); }

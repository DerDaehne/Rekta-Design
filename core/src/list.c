/* list.c: a long list. One view draws the rows in sight and nothing else; the item callback fills a row while it is
   drawn, so a list of a million items costs what a screenful does. It scrolls on the same glide as the panorama.
   Selection is the Metro way, as the web's virtual(): a click selects one item and shows it; Ctrl (Cmd) and a click
   toggles and starts the selection mode, in which every click toggles; Shift and a click takes a range. Keys: Up and
   Down, Home and End, Page Up and Page Down move the active row; Space or Enter is a click; Ctrl+A selects all;
   Escape ends the mode. Included after widgets.c, whose status helpers it shares. */
#include "rk_internal.h"

typedef struct list {
  int count, active, anchor, pressed, nsel;
  bool mode;                                               /* the selection mode: every click toggles */
  uint8_t *sel; size_t selcap;                             /* a bit for each item */
  void (*item)(int index, rk_list_item *out, void *user); void *user;
  void (*show)(rk_view *list, int index, void *user); void *show_user;
  void (*change)(rk_view *list, void *user); void *change_user;
} list;

static float row_height(void) { return rk_metric_of(RK_ROW_HEIGHT); }
static bool is_selected(const list *l, int i) { return i >= 0 && i < l->count && (l->sel[i >> 3] >> (i & 7) & 1); }

static void set_selected(list *l, int i, bool on) {
  if (i < 0 || i >= l->count || is_selected(l, i) == on) return;
  l->sel[i >> 3] ^= (uint8_t)(1u << (i & 7));
  l->nsel += on ? 1 : -1;
}

static void clear_selection(list *l) { memset(l->sel, 0, l->selcap); l->nsel = 0; }

static float max_scroll(rk_view *v, const list *l) { return RK_MAX(0.0f, l->count * row_height() - v->frame.h); }

static void list_changed(rk_view *v, list *l) {
  rk__view_dirty(v);
  if (l->change) l->change(v, l->change_user);
}

/* Scrolls so the row is in sight, gliding there. */
static void reveal_row(rk_view *v, list *l, int i) {
  float H = row_height(), top = i * H, to = v->value;
  if (top < to) to = top;
  else if (top + H > to + v->frame.h) to = top + H - v->frame.h;
  to = RK_CLAMP(to, 0.0f, max_scroll(v, l));
  if (to != v->value) { v->value = to; rk__animate_spring(v, RK_PROP_VALUE, to, 260); }
}

static void set_active(rk_view *v, list *l, int i) {
  if (!l->count) return;
  l->active = RK_CLAMP(i, 0, l->count - 1);
  reveal_row(v, l, l->active);
  rk__view_dirty(v);
}

/* A click, or Space or Enter on the active row. */
static void list_click(rk_view *v, list *l, int i, uint32_t mods) {
  if (i < 0 || i >= l->count) return;
  bool toggle = (mods & (RK_MOD_CTRL | RK_MOD_SUPER)) != 0, range = (mods & RK_MOD_SHIFT) != 0;
  l->active = i;
  if (range) {
    int a = RK_MIN(l->anchor, i), b = RK_MAX(l->anchor, i);
    if (!l->mode) clear_selection(l);
    for (int k = a; k <= b; k++) set_selected(l, k, true);
    l->mode = l->nsel > 1;
  } else if (toggle || l->mode) {
    set_selected(l, i, !is_selected(l, i));
    l->anchor = i;
    l->mode = l->nsel > 0;
  } else {
    clear_selection(l);
    set_selected(l, i, true);
    l->anchor = i;
    list_changed(v, l);
    if (l->show) l->show(v, i, l->show_user);
    return;
  }
  list_changed(v, l);
}

static void draw_row(rk_view *v, rk_canvas *c, list *l, int i, float y, bool ring) {
  float W = v->pframe.w, H = row_height(), pad = rk_metric_of(RK_SPACE_S), left = 14 + pad;
  rk_list_item it = { NULL, NULL, RK_STATUS_NONE };
  l->item(i, &it, l->user);
  bool on = is_selected(l, i);
  rk_color ink = rk_color_of(on ? RK_ROLE_ON_ACCENT : RK_ROLE_TEXT);
  if (on) rk_canvas_fill_rect(c, (rk_rect){ 0, y, W, H }, rk_color_of(RK_ROLE_ACCENT));
  else {
    if (i == l->pressed) rk_canvas_fill_rect(c, (rk_rect){ 0, y, W, H - 1 }, rk_color_of(RK_ROLE_HOVER));
    rk_canvas_fill_rect(c, (rk_rect){ 0, y + H - 1, W, 1 }, rk_color_of(RK_ROLE_TEXT_LOW));
  }
  if (on) {                                                  /* a check without a box, left of the title */
    rk_point check[3] = { { 6, y + H / 2 - 1 }, { 9, y + H / 2 + 2 }, { 15, y + H / 2 - 4 } };
    rk_canvas_polyline(c, check, 3, 2, ink);
  }
  /* the meta or status on the right, then the title in what is left */
  float right = W - pad;
  if (it.status != RK_STATUS_NONE) {
    rk_textfmt tf = rk__textfmt(RK_TEXT_LABEL, 0);
    const char *word = it.meta ? it.meta : status_word(it.status);
    float tw = rk__text_width(word, -1, &tf);
    rk__text_draw(c, word, -1, right - tw, y + (H - tf.size * tf.line) / 2, &tf, ink);
    rk_canvas_fill_rect(c, (rk_rect){ right - tw - 14, y + H / 2 - 4, 8, 8 }, on ? ink : rk_color_of(status_full(it.status)));
    right -= tw + 14 + rk_metric_of(RK_SPACE_M);
  } else if (it.meta) {
    rk_textfmt mf = rk__textfmt(RK_TEXT_META, 0);
    float mw = rk__text_width(it.meta, -1, &mf);
    rk__text_draw(c, it.meta, -1, right - mw, y + (H - mf.size * mf.line) / 2, &mf, on ? ink : rk_color_of(RK_ROLE_TEXT_HIGH));
    right -= mw + rk_metric_of(RK_SPACE_M);
  }
  if (it.title) {
    rk_textfmt f = rk__textfmt(RK_TEXT_BODY, 0);
    rk_canvas_push_clip(c, (rk_rect){ left, y, RK_MAX(0.0f, right - left), H });
    rk__text_draw(c, it.title, -1, left, y + (H - f.size * f.line) / 2, &f, ink);
    rk_canvas_pop_clip(c);
  }
  if (ring) rk_canvas_stroke_rect(c, (rk_rect){ 0, y, W, H }, 2, rk_color_of(RK_ROLE_FOCUS));
}

/* Only the rows the frame draws: those between the top and the bottom of the canvas's clip. */
static void list_draw(rk_view *v, rk_canvas *c, void *st) {
  list *l = st;
  float H = row_height(), scroll = v->pvalue;
  rk_irect clip = rk__canvas_clip(c);
  float top = (clip.y0 - c->oy) / c->scale + scroll, bottom = (clip.y1 - c->oy) / c->scale + scroll;
  int first = RK_MAX(0, (int)floorf(top / H)), last = RK_MIN(l->count - 1, (int)floorf(bottom / H));
  rk_window *w = rk__view_window(v);
  bool focused = w && w->focus == v && w->focus_visible;
  rk_canvas_fill_rect(c, (rk_rect){ 0, 0, v->pframe.w, 1 }, rk_color_of(RK_ROLE_TEXT_LOW));
  for (int i = first; i <= last; i++) draw_row(v, c, l, i, i * H - scroll, focused && i == l->active);
}

/* Its natural height is all its rows; give it a size or let it grow to keep it shorter. */
static rk_size list_measure(rk_view *v, rk_size avail, void *st) {
  (void)v;
  list *l = st;
  return (rk_size){ isfinite(avail.w) ? avail.w : 320, l->count * row_height() };
}

static void list_layout(rk_view *v, void *st) {
  list *l = st;
  float max = max_scroll(v, l);
  if (v->value > max) { v->value = v->pvalue = max; }
}

static int row_at(rk_view *v, const list *l, float y) {
  int i = (int)floorf((y + v->pvalue) / row_height());
  return i >= 0 && i < l->count ? i : -1;
}

static bool list_event(rk_view *v, const rk_event *e, void *st) {
  list *l = st;
  switch (e->type) {
    case RK_EVENT_POINTER_DOWN:
      if (e->button) return false;
      l->pressed = row_at(v, l, e->position.y);
      rk__view_dirty(v);
      return true;
    case RK_EVENT_POINTER_UP: {
      int i = row_at(v, l, e->position.y), was = l->pressed;
      l->pressed = -1;
      rk__view_dirty(v);
      if (i >= 0 && i == was) list_click(v, l, i, e->mods);
      return true;
    }
    case RK_EVENT_WHEEL: {
      float max = max_scroll(v, l), to = RK_CLAMP(v->value + e->wheel_y, 0.0f, max);
      if (to == v->value) return false;                       /* at its end: the wheel moves what holds it */
      v->value = to;
      rk__animate_spring(v, RK_PROP_VALUE, to, 260);
      return true;
    }
    case RK_EVENT_KEY_DOWN: {
      int page = RK_MAX(1, (int)(v->frame.h / row_height()) - 1);
      bool ctrl = (e->mods & (RK_MOD_CTRL | RK_MOD_SUPER)) != 0;
      switch (e->key) {
        case RK_KEY_UP: set_active(v, l, l->active - 1); return true;
        case RK_KEY_DOWN: set_active(v, l, l->active + 1); return true;
        case RK_KEY_HOME: set_active(v, l, 0); return true;
        case RK_KEY_END: set_active(v, l, l->count - 1); return true;
        case RK_KEY_PAGE_UP: set_active(v, l, l->active - page); return true;
        case RK_KEY_PAGE_DOWN: set_active(v, l, l->active + page); return true;
        case RK_KEY_SPACE: case RK_KEY_ENTER: list_click(v, l, l->active, e->mods); return true;
        case 'A':
          if (!ctrl) return false;
          for (int i = 0; i < l->count; i++) set_selected(l, i, true);
          l->mode = l->nsel > 0;
          list_changed(v, l);
          return true;
        case RK_KEY_ESCAPE:
          if (!l->mode) return false;                         /* not selecting: Escape goes back */
          clear_selection(l);
          l->mode = false;
          list_changed(v, l);
          return true;
        default: return false;
      }
    }
    case RK_EVENT_FOCUS: case RK_EVENT_BLUR: rk__view_dirty(v); return false;
    default: return false;
  }
}

static void list_destroy(void *st) { list *l = st; free(l->sel); free(l); }

static const rk_class list_class = { { sizeof(rk_view_class), "rk_list", list_draw, list_measure, list_layout, list_event, list_destroy }, NULL, NULL };

static list *list_of(rk_view *v) { return v && v->cls == &list_class ? v->state : NULL; }

void rk_list_set_count(rk_view *v, int count) {
  list *l = list_of(v);
  if (!l) return;
  count = RK_MAX(0, count);
  size_t need = ((size_t)count + 7) / 8;
  if (need > l->selcap) { l->sel = realloc(l->sel, need); memset(l->sel + l->selcap, 0, need - l->selcap); l->selcap = need; }
  for (int i = count; i < l->count; i++) set_selected(l, i, false);   /* what is gone is no longer selected */
  l->count = count;
  l->active = RK_CLAMP(l->active, 0, RK_MAX(0, count - 1));
  l->anchor = RK_CLAMP(l->anchor, 0, RK_MAX(0, count - 1));
  l->mode = l->mode && l->nsel > 0;
  char buf[32];
  snprintf(buf, sizeof buf, "%d items", count);
  rk__setstr(&v->access_value, buf);
  rk__view_relayout_up(v);
  rk__view_dirty(v);
}

rk_view *rk_list(int count, void (*item)(int index, rk_list_item *out, void *user), void *user) {
  list *l = rk__alloc(sizeof *l);
  l->item = item; l->user = user; l->pressed = -1;
  rk_view *v = rk__view_create(&list_class, l);
  v->clip = true; v->focusable = true;
  v->access = RK_ACCESS_LIST;
  rk_list_set_count(v, count);
  return v;
}

void rk_list_on_show(rk_view *v, void (*fn)(rk_view *, int, void *), void *user) { list *l = list_of(v); if (l) { l->show = fn; l->show_user = user; } }
void rk_list_on_change(rk_view *v, void (*fn)(rk_view *, void *), void *user) { list *l = list_of(v); if (l) { l->change = fn; l->change_user = user; } }
bool rk_list_selected(rk_view *v, int index) { list *l = list_of(v); return l && is_selected(l, index); }
int rk_list_selection_count(rk_view *v) { list *l = list_of(v); return l ? l->nsel : 0; }
void rk_list_scroll_to(rk_view *v, int index) { list *l = list_of(v); if (l && l->count) { l->active = RK_CLAMP(index, 0, l->count - 1); reveal_row(v, l, l->active); rk__view_dirty(v); } }

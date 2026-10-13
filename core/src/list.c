/* list.c: a long list. One view draws the rows in sight and nothing else; the item callback fills a row while it is
   drawn, so a list of a million items costs what a screenful does. It scrolls on the same glide as the panorama.
   Selection is the Metro way, as the web's virtual(): a click selects one item and shows it; Ctrl (Cmd) and a click
   toggles and starts the selection mode, in which every click toggles; Shift and a click takes a range. Keys: Up and
   Down, Home and End, Page Up and Page Down move the active row; Space or Enter is a click; Ctrl+A selects all;
   Escape ends the mode. Included after widgets.c, whose status helpers it shares.

   A table is a list with columns, as the web's .rk-table: a head that stays put (size-xs, semibold, text-high, a
   hairline below; a sorted column's head in the text colour with its triangle), cells from a callback in tabular
   figures, numbers to the right. A click on a sortable head sorts by it, ascending and then descending; the app sorts
   (it owns the rows) and the table draws again. A column's trailing edge in the head drags its width (48px at
   least); a double click on it gives the column back its own. From the keyboard, Up from the first row goes into the
   head: Left and Right move between its columns, Enter or Space sorts, Ctrl with Left or Right changes the width by
   space-m (with Shift, four times that), Down or Escape goes back to the rows.

   An editable column's cell opens on a double click or F2 (the active row's first editable cell): a field as tall as
   the row lies over it, with all a field's keys. Enter or Tab keeps the value - Tab goes on to the next editable cell,
   Shift+Tab back - and Escape puts the old one back; focus going elsewhere keeps it. The app's commit() takes the
   value or says why not, below the cell, which then stays open. */
#include "rk_internal.h"

typedef struct list {
  int count, active, anchor, pressed, nsel;
  bool mode;                                               /* the selection mode: every click toggles */
  uint8_t *sel; size_t selcap;                             /* a bit for each item */
  void (*item)(int index, rk_list_item *out, void *user); void *user;
  void (*show)(rk_view *list, int index, void *user); void *show_user;
  void (*change)(rk_view *list, void *user); void *change_user;
  /* a table */
  struct column { char *title; float width, x, w, set; unsigned flags; } *cols; int ncols, ccap;   /* set: a width given since */
  int head;                                                /* -1, or the column the keyboard is on in the head */
  int resizing, edge_col; float drag_x, drag_w; double edge_at;   /* a column edge being dragged; the last press on one */
  rk_view *editor; int edit_row, edit_col; char *edit_old;   /* a cell being edited */
  const char *(*commit)(rk_view *table, int row, int column, const char *value, void *user); void *commit_user;
  int down_row; double down_at;                            /* the last press on a row: a second soon after edits */
  const char *(*cell)(int row, int column, void *user);
  int sort_col, sort_dir;
  void (*sort)(rk_view *table, int column, int dir, void *user); void *sort_user;
} list;

static float row_height(void) { return rk_metric_of(RK_ROW_HEIGHT); }
static bool is_selected(const list *l, int i) { return i >= 0 && i < l->count && (l->sel[i >> 3] >> (i & 7) & 1); }

static void set_selected(list *l, int i, bool on) {
  if (i < 0 || i >= l->count || is_selected(l, i) == on) return;
  l->sel[i >> 3] ^= (uint8_t)(1u << (i & 7));
  l->nsel += on ? 1 : -1;
}

static void clear_selection(list *l) { memset(l->sel, 0, l->selcap); l->nsel = 0; }

static float head_height(const list *l) {
  if (!l->ncols) return 0;
  rk_textfmt f = rk__textfmt(RK_TEXT_LABEL, 0);
  return ceilf(f.size * f.line + 2 * rk_metric_of(RK_SPACE_S));
}
static float body_height(rk_view *v, const list *l) { return RK_MAX(0.0f, v->frame.h - head_height(l)); }
static float max_scroll(rk_view *v, const list *l) { return RK_MAX(0.0f, l->count * row_height() - body_height(v, l)); }

/* Columns: the fixed widths first, the rest shared by those without one; a check's room on the left. */
#define MIN_COLUMN 48.0f
static float check_room(void) { return 14 + 2 * rk_metric_of(RK_SPACE_S); }
static float col_width(const struct column *c) { return c->set > 0 ? c->set : c->width; }
static void place_columns(list *l, float W) {
  float fixed = 0, x = check_room();
  int flex = 0;
  for (int i = 0; i < l->ncols; i++) { if (col_width(&l->cols[i]) > 0) fixed += col_width(&l->cols[i]); else flex++; }
  float share = flex ? RK_MAX(0.0f, W - check_room() - fixed) / flex : 0;
  for (int i = 0; i < l->ncols; i++) { l->cols[i].x = x; l->cols[i].w = col_width(&l->cols[i]) > 0 ? col_width(&l->cols[i]) : share; x += l->cols[i].w; }
}

/* The column whose trailing edge is under x in the head, or -1. */
static int edge_under(const list *l, float x) {
  for (int k = 0; k < l->ncols; k++) if (fabsf(x - (l->cols[k].x + l->cols[k].w)) <= 4) return k;
  return -1;
}

static void set_width(rk_view *v, list *l, int k, float w) {
  l->cols[k].set = w > 0 ? RK_MAX(MIN_COLUMN, w) : 0;
  place_columns(l, v->frame.w);
  rk__view_dirty(v);
}

static void reveal_row(rk_view *v, list *l, int i);

static void place_editor(rk_view *v, list *l) {
  if (!l->editor) return;
  place_columns(l, v->frame.w);
  const struct column *c = &l->cols[l->edit_col];
  rk_size sz = rk__measure(l->editor, (rk_size){ c->w, INFINITY });
  rk__set_frame(l->editor, (rk_rect){ c->x, head_height(l) + l->edit_row * row_height() - v->value, c->w, sz.h });
}

static void edit_finish(rk_view *v, list *l, bool focus_back) {
  rk_view *ed = l->editor;
  if (!ed) return;
  l->editor = NULL;
  free(l->edit_old); l->edit_old = NULL;
  rk_window *w = rk__view_window(v);
  bool had = w && rk__view_within(w->focus, ed);
  rk_view_remove(ed);
  if (had && focus_back) rk_window_focus(w, v);
  rk__view_dirty(v);
}

/* Takes the value, or says why not and stays open. True when the cell has closed. */
static bool edit_commit(rk_view *v, list *l, bool focus_back) {
  if (!l->editor) return true;
  const char *value = rk_field_text(l->editor);
  if (!strcmp(value, l->edit_old)) { edit_finish(v, l, focus_back); return true; }
  const char *why = l->commit ? l->commit(v, l->edit_row, l->edit_col, value, l->commit_user) : NULL;
  if (!l->editor) return true;                             /* the app closed it itself */
  if (why) { rk_field_set_hint(l->editor, why, true); place_editor(v, l); return false; }
  edit_finish(v, l, focus_back);
  return true;
}

static void editor_submit(rk_view *field, void *user) { (void)field; rk_view *v = user; edit_commit(v, v->state, true); }

static int editable_from(const list *l, int k, int dir) {
  for (; k >= 0 && k < l->ncols; k += dir) if (l->cols[k].flags & RK_COLUMN_EDITABLE) return k;
  return -1;
}

static void edit_start(rk_view *v, list *l, int row, int col) {
  if (!l->cell || row < 0 || row >= l->count || col < 0 || col >= l->ncols || !(l->cols[col].flags & RK_COLUMN_EDITABLE)) return;
  if (l->editor && !edit_commit(v, l, false)) return;
  l->active = row; l->head = -1;
  reveal_row(v, l, row);
  rk__anim_cancel(v, RK_PROP_VALUE);                       /* there at once: the editor lies on the row */
  v->pvalue = v->value;
  const char *s = l->cell(row, col, l->user);
  l->edit_old = rk__strdup(s ? s : "");
  l->edit_row = row; l->edit_col = col;
  rk_view *ed = rk_field(NULL, NULL);
  rk__field_set_box_height(ed, row_height());
  ed->fill_role = RK_ROLE_BG; ed->pfill = rk_color_of(RK_ROLE_BG);   /* a reason below the cell hides the row under it */
  rk_field_set_text(ed, l->edit_old);
  rk_field_on_submit(ed, editor_submit, v);
  l->editor = ed;
  rk__view_add(v, ed); rk_release(ed);
  place_editor(v, l);
  rk_event all = { sizeof all, RK_EVENT_KEY_DOWN, rk_app_now(), { 0, 0 }, 0, 0, 0, 'A', RK_MOD_CTRL, NULL };
  ed->cls->pub.event(ed, &all, ed->state);                  /* the old value, selected: typing replaces it */
  rk_window *w = rk__view_window(v);
  if (w) rk_window_focus(w, ed);
  rk__view_dirty(v);
}

static void sort_by(rk_view *v, list *l, int k) {
  if (k < 0 || k >= l->ncols || !(l->cols[k].flags & RK_COLUMN_SORTABLE)) return;
  l->sort_dir = k == l->sort_col && l->sort_dir > 0 ? -1 : 1;
  l->sort_col = k;
  if (l->sort) l->sort(v, k, l->sort_dir, l->sort_user);
  rk__view_dirty(v);
}

static void list_changed(rk_view *v, list *l) {
  rk__view_dirty(v);
  if (l->change) l->change(v, l->change_user);
}

/* Scrolls so the row is in sight, gliding there. */
static void reveal_row(rk_view *v, list *l, int i) {
  float H = row_height(), top = i * H, to = v->value, body = body_height(v, l);
  if (top < to) to = top;
  else if (top + H > to + body) to = top + H - body;
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
  if (l->ncols) {                                            /* a table's row: its cells */
    rk_textfmt f = rk__textfmt(RK_TEXT_BODY, 0);
    f.tabular = true;
    for (int k = 0; k < l->ncols; k++) {
      const struct column *col = &l->cols[k];
      const char *s = l->cell(i, k, l->user);
      if (!s || !*s) continue;
      float cx = col->x + pad, cw = RK_MAX(0.0f, col->w - 2 * pad), tw = rk__text_width(s, -1, &f);
      rk_canvas_push_clip(c, (rk_rect){ cx, y, cw, H });
      rk__text_draw(c, s, -1, col->flags & RK_COLUMN_NUMERIC ? cx + cw - tw : cx, y + (H - f.size * f.line) / 2, &f, ink);
      rk_canvas_pop_clip(c);
    }
    if (ring) rk_canvas_stroke_rect(c, (rk_rect){ 0, y, W, H }, 2, rk_color_of(RK_ROLE_FOCUS));
    return;
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
/* A table's head: each column's title; a sorted one in the text colour with its triangle. */
static void draw_head(rk_view *v, rk_canvas *c, list *l, bool focused) {
  float W = v->pframe.w, hh = head_height(l), pad = rk_metric_of(RK_SPACE_S);
  rk_textfmt f = rk__textfmt(RK_TEXT_LABEL, 0);
  rk_canvas_fill_rect(c, (rk_rect){ 0, 0, W, hh }, rk_color_of(RK_ROLE_BG));
  rk_canvas_fill_rect(c, (rk_rect){ 0, hh - 1, W, 1 }, rk_color_of(RK_ROLE_TEXT_LOW));
  for (int k = 0; k < l->ncols; k++) {
    const struct column *col = &l->cols[k];
    if (!col->title) continue;
    bool sorted = k == l->sort_col && l->sort_dir;
    rk_color ink = rk_color_of(sorted ? RK_ROLE_TEXT : RK_ROLE_TEXT_HIGH);
    float tw = rk__text_width(col->title, -1, &f), cw = col->w - 2 * pad, arrow = sorted ? 6 + 8 : 0;
    float x = col->flags & RK_COLUMN_NUMERIC ? col->x + pad + cw - tw - arrow : col->x + pad, ty = (hh - f.size * f.line) / 2;
    rk__text_draw(c, col->title, -1, x, ty, &f, ink);
    if (sorted) {
      float ax = x + tw + 6, ay = hh / 2;
      rk_point up[3] = { { ax, ay + 2 }, { ax + 8, ay + 2 }, { ax + 4, ay - 2 } }, down[3] = { { ax, ay - 2 }, { ax + 8, ay - 2 }, { ax + 4, ay + 2 } };
      rk_canvas_fill_polygon(c, l->sort_dir > 0 ? up : down, 3, ink);
    }
  }
  if (focused && l->head >= 0 && l->head < l->ncols)        /* the keyboard is in the head: its column, ringed */
    rk_canvas_stroke_rect(c, (rk_rect){ l->cols[l->head].x, 0, l->cols[l->head].w, hh }, 2, rk_color_of(RK_ROLE_FOCUS));
}

static void list_draw(rk_view *v, rk_canvas *c, void *st) {
  list *l = st;
  float H = row_height(), scroll = v->pvalue, hh = head_height(l);
  if (l->ncols) place_columns(l, v->pframe.w);
  rk_irect clip = rk__canvas_clip(c);
  float top = (clip.y0 - c->oy) / c->scale - hh + scroll, bottom = (clip.y1 - c->oy) / c->scale - hh + scroll;
  int first = RK_MAX(0, (int)floorf(top / H)), last = RK_MIN(l->count - 1, (int)floorf(bottom / H));
  rk_window *w = rk__view_window(v);
  bool focused = w && w->focus == v && w->focus_visible;
  if (!l->ncols) rk_canvas_fill_rect(c, (rk_rect){ 0, 0, v->pframe.w, 1 }, rk_color_of(RK_ROLE_TEXT_LOW));
  rk_canvas_push_clip(c, (rk_rect){ 0, hh, v->pframe.w, RK_MAX(0.0f, v->pframe.h - hh) });
  for (int i = first; i <= last; i++) draw_row(v, c, l, i, hh + i * H - scroll, focused && l->head < 0 && i == l->active);
  rk_canvas_pop_clip(c);
  if (l->ncols) draw_head(v, c, l, focused);
}

/* Its natural height is all its rows; give it a size or let it grow to keep it shorter. */
static rk_size list_measure(rk_view *v, rk_size avail, void *st) {
  (void)v;
  list *l = st;
  return (rk_size){ isfinite(avail.w) ? avail.w : 320, head_height(l) + l->count * row_height() };
}

static void list_layout(rk_view *v, void *st) {
  list *l = st;
  float max = max_scroll(v, l);
  if (v->value > max) { v->value = v->pvalue = max; }
  place_editor(v, l);
}

static int row_at(rk_view *v, const list *l, float y) {
  if (y < head_height(l)) return -1;
  int i = (int)floorf((y - head_height(l) + v->pvalue) / row_height());
  return i >= 0 && i < l->count ? i : -1;
}

static bool list_event(rk_view *v, const rk_event *e, void *st) {
  list *l = st;
  switch (e->type) {
    case RK_EVENT_POINTER_DOWN:
      if (e->button) return false;
      if (l->ncols && e->position.y < head_height(l)) {
        place_columns(l, v->frame.w);
        int edge = edge_under(l, e->position.x);
        if (edge >= 0) {                                        /* a column's edge: drag its width; twice, its own again */
          if (e->time - l->edge_at < 400 && edge == l->edge_col) { set_width(v, l, edge, 0); l->edge_at = 0; return true; }
          l->resizing = l->edge_col = edge; l->edge_at = e->time;
          l->drag_x = e->position.x; l->drag_w = l->cols[edge].w;
          return true;
        }
        for (int k = 0; k < l->ncols; k++)                      /* a sortable head: ascending, then descending */
          if (e->position.x >= l->cols[k].x && e->position.x < l->cols[k].x + l->cols[k].w) sort_by(v, l, k);
        return true;
      }
      l->pressed = row_at(v, l, e->position.y);
      if (l->ncols && l->pressed >= 0 && l->pressed == l->down_row && e->time - l->down_at < 400) {   /* a double click on a cell */
        l->down_at = 0;
        for (int k = 0; k < l->ncols; k++)
          if (e->position.x >= l->cols[k].x && e->position.x < l->cols[k].x + l->cols[k].w) { edit_start(v, l, l->pressed, k); break; }
        l->pressed = -1;
        return true;
      }
      l->down_row = l->pressed; l->down_at = e->time;
      rk__view_dirty(v);
      return true;
    case RK_EVENT_POINTER_MOVE:
      if (l->resizing >= 0) {
        if (fabsf(e->position.x - l->drag_x) > 2) l->edge_at = 0;   /* a drag is not half of a double click */
        set_width(v, l, l->resizing, RK_MAX(MIN_COLUMN, l->drag_w + e->position.x - l->drag_x));
        v->cursor = RK__CURSOR_COL_RESIZE;
        return true;
      }
      v->cursor = l->ncols && e->position.y < head_height(l) && edge_under(l, e->position.x) >= 0 ? RK__CURSOR_COL_RESIZE : RK__CURSOR_ARROW;
      return false;
    case RK_EVENT_POINTER_UP: {
      if (l->resizing >= 0) { l->resizing = -1; return true; }
      int i = row_at(v, l, e->position.y), was = l->pressed;
      l->pressed = -1;
      rk__view_dirty(v);
      if (i >= 0 && i == was) list_click(v, l, i, e->mods);
      return true;
    }
    case RK_EVENT_WHEEL: {
      if (l->editor) return true;                             /* the editor lies on its row: no scrolling under it */
      float max = max_scroll(v, l), to = RK_CLAMP(v->value + e->wheel_y, 0.0f, max);
      if (to == v->value) return false;                       /* at its end: the wheel moves what holds it */
      v->value = to;
      rk__animate_spring(v, RK_PROP_VALUE, to, 260);
      return true;
    }
    case RK_EVENT_KEY_DOWN: {
      int page = RK_MAX(1, (int)(body_height(v, l) / row_height()) - 1);
      bool ctrl = (e->mods & (RK_MOD_CTRL | RK_MOD_SUPER)) != 0;
      rk_window *w = rk__view_window(v);
      if (l->editor && w && rk__view_within(w->focus, l->editor)) {   /* what the editor left: Escape, Tab */
        if (e->key == RK_KEY_ESCAPE) { edit_finish(v, l, true); return true; }
        if (e->key == RK_KEY_TAB) {
          int dir = e->mods & RK_MOD_SHIFT ? -1 : 1, row = l->edit_row, next = editable_from(l, l->edit_col + dir, dir);
          if (edit_commit(v, l, true) && next >= 0) edit_start(v, l, row, next);
          return true;
        }
        return false;
      }
      if (e->key == RK_KEY_F1 + 1 && l->ncols) { edit_start(v, l, l->active, editable_from(l, 0, 1)); return true; }   /* F2 */
      if (l->head >= 0) {                                       /* in the head: columns, their order and widths */
        float step = rk_metric_of(RK_SPACE_M) * (e->mods & RK_MOD_SHIFT ? 4 : 1);
        switch (e->key) {
          case RK_KEY_LEFT: case RK_KEY_RIGHT:
            if (ctrl) set_width(v, l, l->head, RK_MAX(MIN_COLUMN, l->cols[l->head].w + (e->key == RK_KEY_RIGHT ? step : -step)));
            else l->head = RK_CLAMP(l->head + (e->key == RK_KEY_RIGHT ? 1 : -1), 0, l->ncols - 1);
            rk__view_dirty(v);
            return true;
          case RK_KEY_HOME: case RK_KEY_END: l->head = e->key == RK_KEY_HOME ? 0 : l->ncols - 1; rk__view_dirty(v); return true;
          case RK_KEY_ENTER: case RK_KEY_SPACE: sort_by(v, l, l->head); return true;
          case RK_KEY_DOWN: case RK_KEY_ESCAPE: l->head = -1; set_active(v, l, l->active); return true;
          case RK_KEY_UP: return true;
          default: return false;
        }
      }
      switch (e->key) {
        case RK_KEY_UP:
          if (l->ncols && l->active == 0) { place_columns(l, v->frame.w); l->head = l->sort_col >= 0 ? l->sort_col : 0; reveal_row(v, l, 0); rk__view_dirty(v); return true; }
          set_active(v, l, l->active - 1);
          return true;
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
    case RK_EVENT_FOCUS: rk__view_dirty(v); return false;
    case RK_EVENT_BLUR: {
      rk_window *w = rk__view_window(v);
      if (l->editor && w && rk__view_within(w->focus, l->editor)) { edit_commit(v, l, false); return false; }   /* the editor lets go: kept */
      l->head = -1;
      rk__view_dirty(v);
      return false;
    }
    default: return false;
  }
}

static void list_destroy(void *st) {
  list *l = st;
  for (int i = 0; i < l->ncols; i++) free(l->cols[i].title);
  free(l->edit_old); free(l->cols); free(l->sel); free(l);
}

static const rk_class list_class = { { sizeof(rk_view_class), "rk_list", list_draw, list_measure, list_layout, list_event, list_destroy }, NULL, NULL };

static list *list_of(rk_view *v) { return v && v->cls == &list_class ? v->state : NULL; }

void rk_list_set_count(rk_view *v, int count) {
  list *l = list_of(v);
  if (!l) return;
  count = RK_MAX(0, count);
  size_t need = ((size_t)count + 7) / 8;
  if (need > l->selcap) { l->sel = realloc(l->sel, need); memset(l->sel + l->selcap, 0, need - l->selcap); l->selcap = need; }
  for (int i = count; i < l->count; i++) set_selected(l, i, false);   /* what is gone is no longer selected */
  if (l->editor && l->edit_row >= count) edit_finish(v, l, true);     /* its row is gone */
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
  l->item = item; l->user = user; l->pressed = -1; l->head = l->resizing = l->edge_col = l->down_row = -1;
  rk_view *v = rk__view_create(&list_class, l);
  v->clip = true; v->focusable = true; v->draws_focus = true;   /* the active row (or head column) shows focus */
  v->access = RK_ACCESS_LIST;
  rk_list_set_count(v, count);
  return v;
}

void rk_list_on_show(rk_view *v, void (*fn)(rk_view *, int, void *), void *user) { list *l = list_of(v); if (l) { l->show = fn; l->show_user = user; } }
void rk_list_on_change(rk_view *v, void (*fn)(rk_view *, void *), void *user) { list *l = list_of(v); if (l) { l->change = fn; l->change_user = user; } }
bool rk_list_selected(rk_view *v, int index) { list *l = list_of(v); return l && is_selected(l, index); }
int rk_list_selection_count(rk_view *v) { list *l = list_of(v); return l ? l->nsel : 0; }
void rk_list_scroll_to(rk_view *v, int index) { list *l = list_of(v); if (l && l->count) { l->active = RK_CLAMP(index, 0, l->count - 1); reveal_row(v, l, l->active); rk__view_dirty(v); } }

/* A table: a list whose rows are cells. */
static void table_item(int index, rk_list_item *out, void *user) { (void)index; (void)out; (void)user; }

rk_view *rk_table(int count, const char *(*cell)(int row, int column, void *user), void *user) {
  rk_view *v = rk_list(count, table_item, user);
  list *l = list_of(v);
  l->cell = cell;
  l->sort_col = -1;
  v->access = RK_ACCESS_TABLE;
  return v;
}

int rk_table_add_column(rk_view *v, const char *title, float width, unsigned flags) {
  list *l = list_of(v);
  if (!l || !l->cell) return -1;
  l->cols = rk__grow(l->cols, &l->ccap, l->ncols + 1, sizeof *l->cols);
  struct column *c = &l->cols[l->ncols++];
  memset(c, 0, sizeof *c);
  c->title = rk__strdup(title); c->width = RK_MAX(0.0f, width); c->flags = flags;
  rk__view_relayout_up(v); rk__view_dirty(v);
  return l->ncols - 1;
}

void rk_table_on_sort(rk_view *v, void (*fn)(rk_view *, int, int, void *), void *user) { list *l = list_of(v); if (l) { l->sort = fn; l->sort_user = user; } }
void rk_table_set_sort(rk_view *v, int column, int dir) { list *l = list_of(v); if (l) { l->sort_col = column; l->sort_dir = dir > 0 ? 1 : dir < 0 ? -1 : 0; rk__view_dirty(v); } }

void rk_table_set_column_width(rk_view *v, int column, float width) {
  list *l = list_of(v);
  if (l && column >= 0 && column < l->ncols) set_width(v, l, column, width);
}

float rk_table_column_width(rk_view *v, int column) {
  list *l = list_of(v);
  if (!l || column < 0 || column >= l->ncols) return 0;
  place_columns(l, v->frame.w);
  return l->cols[column].w;
}

void rk_table_on_edit(rk_view *v, const char *(*fn)(rk_view *, int, int, const char *, void *), void *user) { list *l = list_of(v); if (l) { l->commit = fn; l->commit_user = user; } }
void rk_table_edit(rk_view *v, int row, int column) { list *l = list_of(v); if (l) edit_start(v, l, row, column); }

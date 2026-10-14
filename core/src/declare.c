/* declare.c: declared views (rekta_ui.h), a layer over the view tree that adds nothing to it.
 *
 * A build says what a view shows, node by node. Each node is matched as it is declared with the one the last build
 * made at its place - by key or id, or else as the next unkeyed node of its kind - and only what differs is set on
 * its view, with the setters an app would ui_call. What comes, goes or moves is gathered while the build runs and done
 * when it ends, so the design's motion tells the change: first what leaves (RK_MOTION_LEAVE: it fades, the rest close
 * up after it), then what enters or moves (RK_MOTION_ENTER, or RK_MOTION_REFLOW when nothing is new). A change of
 * content takes its ui_transition before the setter: a tile's new value peeks in, a new status flips it.
 *
 * A node keeps a copy of what was declared last, never reads it back from the view: so the library pushes what the
 * description ui_changed, never what the user ui_changed (a field's text, a control's value). */
#include "rk_internal.h"
#include "../include/rekta_ui.h"

typedef struct ui_root ui_root;
typedef struct ui_node ui_node;

struct ui_node {
  /* the last description, its strings and series copied */
  rk_ui_kind kind; char *key; uint64_t id;
  char *text, *label, *meta, *message, *icon, *placeholder, *hint, *error, *input, *name, *tooltip;
  rk_text_style style; rk_role color; rk_button_kind button; rk_status status; rk_ui_change change;
  bool has_progress, secret, disabled; float progress; float *series; int nseries;
  double value; int lines;
  int span_c, span_r; float width, height, grow, gap; rk_insets padding;
  void (*on_press)(rk_view *, void *), (*on_change)(rk_view *, void *), (*on_submit)(rk_view *, void *);
  void *user;
  rk_view *(*menu)(void *); rk_view *(*make)(void *);
  /* its view and what it holds */
  rk_view *view;                                   /* a reference of its own; the top's is the declared view, borrowed */
  ui_root *root;
  ui_node **kids; int nkids, kcap;                 /* as they stand */
  ui_node **next; int nnext, ncap;                 /* as this build declares them */
  ui_node *replaces;                               /* the last build's node whose view this one's takes the place of */
  int at;                                          /* its place among the last build's kids; -1: new */
  int from;                                        /* where the next search by key starts */
  bool seen, stays, fresh;                         /* fresh: made in this build, its kids go straight in */
};

enum { OP_REPLACE, OP_REMOVE, OP_PLACE };
typedef struct ui_op { int kind; ui_node *parent, *node, *prev; } ui_op;

struct ui_root {
  ui_node top;                                     /* holds what the build declares */
  void (*build)(void *user); void *user;
  ui_node **stack; int depth, scap;                /* the open nodes; NULL where a description was refused */
  ui_op *ops; int nops, ocap;
  bool building, pending, reflow, entering;
  int handling;                                    /* its handlers running: the build waits for them */
  ui_root *next_root;
};

static ui_root *ui_roots;                             /* every declared view alive */
static ui_root *ui_cur;                               /* the one being built */

static const char *ui_kind_name(rk_ui_kind k) {
  static const char *const names[] = { "top", "RK_VSTACK", "RK_HSTACK", "RK_TILES", "RK_ROWS", "RK_LABEL", "RK_BUTTON",
                                       "RK_TILE", "RK_ROW", "RK_FIELD", "RK_CHECK", "RK_TOGGLE", "RK_PROGRESS", "RK_ICON", "RK_HOST" };
  return k >= 0 && k < RK_COUNT(names) ? names[k] : "an unknown kind";
}
static bool ui_holds(rk_ui_kind k) { return k == RK_UI_NONE || k == RK_UI_VSTACK || k == RK_UI_HSTACK || k == RK_UI_TILES || k == RK_UI_ROWS; }

/* Nodes ----------------------------------------------------------------------------------------------------------- */

/* Its view calls nothing of ours any more: it is leaving, or replaced. */
static void ui_unhook(ui_node *n) {
  rk_view *v = n->view;
  if (n->on_press) rk_view_on_activate(v, NULL, NULL);
  if (n->kind == RK_UI_FIELD) { rk_field_on_change(v, NULL, NULL); rk_field_on_submit(v, NULL, NULL); }
  if (n->kind == RK_UI_CHECK || n->kind == RK_UI_TOGGLE) rk_control_on_change(v, NULL, NULL);
}

static void ui_node_free(ui_node *n) {
  for (int i = 0; i < n->nkids; i++) ui_node_free(n->kids[i]);
  free(n->kids); free(n->next);
  ui_unhook(n);
  rk_release(n->view);
  free(n->key); free(n->text); free(n->label); free(n->meta); free(n->message); free(n->icon); free(n->placeholder);
  free(n->hint); free(n->error); free(n->input); free(n->name); free(n->tooltip); free(n->series);
  free(n);
}

static bool ui_same(const char *a, const char *b) { return a == b || (a && b && !strcmp(a, b)); }

static bool ui_take(char **slot, const char *s) {     /* true when it changed */
  if (ui_same(*slot, s)) return false;
  free(*slot);
  *slot = rk__strdup(s);
  return true;
}

static bool ui_take_progress(ui_node *n, const float *p) {
  bool has = p != NULL;
  float v = has ? *p : 0;
  if (has == n->has_progress && v == n->progress) return false;
  n->has_progress = has; n->progress = v;
  return true;
}

static bool ui_take_series(ui_node *n, const float *s, int count) {
  if (!s || count < 0) count = 0;
  if (count == n->nseries && (!count || !memcmp(n->series, s, (size_t)count * sizeof *s))) return false;
  free(n->series);
  n->series = count ? rk__alloc((size_t)count * sizeof *s) : NULL;
  if (count) memcpy(n->series, s, (size_t)count * sizeof *s);
  n->nseries = count;
  return true;
}

/* The transition a change of content takes, before the setter: the node's own, or what the kind does by itself. */
static void ui_transition(ui_node *n, rk_ui_change automatic, bool fresh) {
  rk_ui_change c = n->change == RK_UI_AUTO ? automatic : n->change;
  if (fresh || c == RK_UI_IN_PLACE || c == RK_UI_AUTO) return;
  rk_view_transition(n->view, c == RK_UI_PEEK ? RK_TRANSITION_PEEK : c == RK_UI_FLIP ? RK_TRANSITION_FLIP : RK_TRANSITION_CROSSFADE);
}

static void ui_pressed(rk_view *v, void *u);
static void ui_changed(rk_view *v, void *u);
static void ui_submitted(rk_view *v, void *u);

static float ui_design_gap(rk_ui_kind k) {
  return k == RK_UI_VSTACK ? rk_metric_of(RK_SPACE_M) : k == RK_UI_HSTACK || k == RK_UI_TILES ? rk_metric_of(RK_SPACE_S) : 0;
}

/* What differs from the last description goes to the view; fresh: the view was just made from d. */
static void ui_update(ui_node *n, const rk_ui *d, bool fresh) {
  ui_root *r = n->root;
  rk_view *v = n->view;
  void *user = d->user ? d->user : r->user;
  bool layout = false;
  n->change = d->change; n->id = d->id; n->user = d->user;
  ui_take(&n->key, d->key);
  if (ui_take(&n->name, d->name ? d->name : d->key)) rk_view_set_name(v, n->name);
  switch (n->kind) {
    case RK_UI_LABEL:
      if (ui_take(&n->text, d->text)) { ui_transition(n, RK_UI_IN_PLACE, fresh); rk_label_set_text(v, n->text); }
      if (n->color != d->color) { n->color = d->color; rk_label_set_color(v, d->color); }
      break;
    case RK_UI_BUTTON:
      if (n->button != d->button) { n->button = d->button; rk_button_set_kind(v, d->button); }
      if (ui_take(&n->icon, d->icon)) rk_button_set_icon(v, n->icon);
      break;
    case RK_UI_TILE: {
      bool stat = ui_take(&n->text, d->text);
      stat |= ui_take(&n->label, d->label);
      bool status = n->status != d->status;
      if (status && !fresh) rk_view_transition(v, RK_TRANSITION_FLIP);   /* a new status turns it over, always */
      else if (stat) ui_transition(n, RK_UI_PEEK, fresh);
      if (status) { n->status = d->status; rk_tile_set_status(v, d->status, NULL); }
      if (stat) rk_tile_set_stat(v, n->text, n->label);
      if (ui_take(&n->message, d->message)) rk_tile_set_message(v, n->message);
      if (ui_take_progress(n, d->progress)) rk_tile_set_progress(v, n->has_progress ? n->progress : -1);
      if (ui_take_series(n, d->series, d->nseries)) rk_tile_set_series(v, n->series, n->nseries);
      break;
    }
    case RK_UI_ROW: {
      bool c = ui_take(&n->text, d->text);
      c |= ui_take(&n->meta, d->meta);
      c |= n->status != d->status;
      if (c) { ui_transition(n, RK_UI_IN_PLACE, fresh); n->status = d->status; rk_row_set(v, n->text, n->meta, n->status); }
      break;
    }
    case RK_UI_FIELD: {
      bool h = ui_take(&n->hint, d->hint);
      h |= ui_take(&n->error, d->error);
      if (h) { if (n->error) rk_field_set_error(v, n->error); else rk_field_set_hint(v, n->hint); }
      /* the app's text, when it changed: never over what the user typed since */
      if (ui_take(&n->input, d->input) && n->input && strcmp(rk_field_text(v), n->input)) rk_field_set_text(v, n->input);
      if (n->lines != d->lines) { n->lines = d->lines; rk_field_set_lines(v, d->lines > 0 ? d->lines : 1); layout = true; }
      if (n->secret != d->secret) { n->secret = d->secret; rk_field_set_secret(v, d->secret); }
      if (n->on_change != d->on_change) { n->on_change = d->on_change; rk_field_on_change(v, d->on_change ? ui_changed : NULL, n); }
      if (n->on_submit != d->on_submit) { n->on_submit = d->on_submit; rk_field_on_submit(v, d->on_submit ? ui_submitted : NULL, n); }
      break;
    }
    case RK_UI_CHECK: case RK_UI_TOGGLE:
      if (n->value != d->value) { n->value = d->value; if (rk_control_value(v) != d->value) rk_control_set_value(v, d->value); }
      if (n->on_change != d->on_change) { n->on_change = d->on_change; rk_control_on_change(v, d->on_change ? ui_changed : NULL, n); }
      break;
    case RK_UI_PROGRESS:
      if (ui_take_progress(n, d->progress)) rk_progress_set_value(v, n->has_progress ? n->progress : -1);
      break;
    case RK_UI_HOST:
      if (d->update) d->update(v, user);
      break;
    case RK_UI_VSTACK: case RK_UI_HSTACK: case RK_UI_TILES: case RK_UI_ROWS:
      if (n->gap != d->gap) { n->gap = d->gap; rk_view_set_gap(v, d->gap > 0 ? d->gap : ui_design_gap(n->kind)); layout = true; }
      if (memcmp(&n->padding, &d->padding, sizeof n->padding)) { n->padding = d->padding; rk_view_set_padding(v, d->padding); layout = true; }
      break;
    default: break;
  }
  /* what any view has */
  if (ui_take(&n->tooltip, d->tooltip)) rk_view_set_tooltip(v, n->tooltip);
  if (n->disabled != d->disabled) { n->disabled = d->disabled; rk_view_set_enabled(v, !d->disabled); }
  int sc = RK_MAX(d->span.columns, 1), sr = RK_MAX(d->span.rows, 1);
  if (sc != n->span_c || sr != n->span_r) { n->span_c = sc; n->span_r = sr; rk_view_set_span(v, sc, sr); layout = true; }
  if (n->width != d->width || n->height != d->height) { n->width = d->width; n->height = d->height; rk_view_set_size(v, d->width, d->height); layout = true; }
  if (n->grow != d->grow) { n->grow = d->grow; rk_view_set_grow(v, d->grow); layout = true; }
  if (n->menu != d->menu) { n->menu = d->menu; rk_view_set_menu(v, d->menu ? d->menu(user) : NULL); }
  if (n->on_press != d->on_press) { n->on_press = d->on_press; rk_view_on_activate(v, d->on_press ? ui_pressed : NULL, d->on_press ? n : NULL); }
  if (layout && !fresh) r->reflow = true;                     /* it glides to its new size or place */
}

/* Properties the view's kind takes only when it is made: a new value makes a new view in the old one's place. */
static bool ui_made_with(const ui_node *n, const rk_ui *d) {
  if (n->kind != d->kind) return false;
  switch (n->kind) {
    case RK_UI_LABEL: return n->style == d->style;
    case RK_UI_BUTTON: case RK_UI_CHECK: case RK_UI_TOGGLE: return ui_same(n->text, d->text);
    case RK_UI_FIELD: return ui_same(n->text, d->text) && ui_same(n->placeholder, d->placeholder);
    case RK_UI_ICON: return n->color == d->color && ui_same(n->icon, d->icon);
    case RK_UI_HOST: return n->make == d->make;
    default: return true;
  }
}

static ui_node *ui_create(ui_root *r, ui_node *parent, const rk_ui *d) {
  ui_node *n = rk__alloc(sizeof *n);
  n->root = r; n->kind = d->kind; n->fresh = true; n->at = -1;
  n->span_c = n->span_r = 1;
  void *user = d->user ? d->user : r->user;
  rk_view *v = NULL;
  switch (d->kind) {
    case RK_UI_VSTACK: v = rk_vstack(); break;
    case RK_UI_HSTACK: v = rk_hstack(); break;
    case RK_UI_TILES: v = rk_tile_grid(); break;
    case RK_UI_ROWS: v = rk_rows(); break;
    case RK_UI_LABEL: v = rk_label(d->text, d->style); n->style = d->style; ui_take(&n->text, d->text); break;
    case RK_UI_BUTTON: v = rk_button(d->text, NULL, NULL); ui_take(&n->text, d->text); break;
    case RK_UI_TILE: v = rk_tile(d->text, d->label); ui_take(&n->text, d->text); ui_take(&n->label, d->label); break;
    case RK_UI_ROW:                                              /* a row is made in its rows, at the end: placed later */
      v = rk_retain(rk_rows_add(parent->view, d->text, d->meta, d->status));
      ui_take(&n->text, d->text); ui_take(&n->meta, d->meta); n->status = d->status;
      break;
    case RK_UI_FIELD: v = rk_field(d->text, d->placeholder); ui_take(&n->text, d->text); ui_take(&n->placeholder, d->placeholder); break;
    case RK_UI_CHECK: v = rk_check(d->text, d->value != 0); ui_take(&n->text, d->text); n->value = d->value; break;
    case RK_UI_TOGGLE: v = rk_toggle(d->text, d->value != 0); ui_take(&n->text, d->text); n->value = d->value; break;
    case RK_UI_PROGRESS: v = rk_progress(d->progress ? *d->progress : -1); ui_take_progress(n, d->progress); break;
    case RK_UI_ICON: v = rk_icon(d->icon, d->color); ui_take(&n->icon, d->icon); n->color = d->color; break;
    case RK_UI_HOST: v = d->make ? d->make(user) : NULL; n->make = d->make; break;
    default: break;
  }
  n->view = v ? v : rk_view_create();
  ui_update(n, d, true);
  return n;
}

/* Who it is: the node of the last build it continues, or NULL. */
static bool ui_same_one(const ui_node *k, const rk_ui *d) {
  return k->id == d->id && (k->key ? d->key && !strcmp(k->key, d->key) : !d->key);
}

static ui_node *ui_find(ui_node *p, const rk_ui *d) {
  bool keyed = d->key || d->id;
  int n = p->nkids;
  for (int i = 0; i < n; i++) {
    int j = keyed ? (p->from + i) % n : i;                     /* by key: from where the last one was, as lists keep order */
    ui_node *k = p->kids[j];
    if (!ui_same_one(k, d) || (!keyed && k->kind != d->kind)) continue;
    if (k->seen) {
      if (keyed) rk__warn("rk_ui: two views are %s%s%s among the same siblings: each needs its own key", d->key ? d->key : "",
                          d->key && d->id ? " " : "", d->id ? "with one id" : "");
      continue;
    }
    k->seen = true; k->at = j;
    if (keyed) p->from = j + 1;
    return k;
  }
  return NULL;
}

/* Building ----------------------------------------------------------------------------------------------------------- */

static void ui_push(ui_root *r, ui_node *n) {
  r->stack = rk__grow(r->stack, &r->scap, r->depth + 1, sizeof *r->stack);
  r->stack[r->depth++] = n;
}

static void ui_record(ui_root *r, int kind, ui_node *parent, ui_node *node, ui_node *prev) {
  r->ops = rk__grow(r->ops, &r->ocap, r->nops + 1, sizeof *r->ops);
  r->ops[r->nops++] = (ui_op){ kind, parent, node, prev };
  if (kind == OP_PLACE && node->at < 0) r->entering = true;
}

/* The kids that keep their places: the longest run of them already in order (what moves least), the rest are moved. */
static void ui_stable(ui_node *n) {
  int m = n->nnext, len = 0;
  int *tail = rk__alloc((size_t)(m + 1) * 2 * sizeof *tail), *prev = tail + m + 1;
  for (int i = 0; i < m; i++) {
    ui_node *k = n->next[i];
    k->stays = false;
    if (k->at < 0) continue;
    int lo = 0, hi = len;
    while (lo < hi) { int mid = (lo + hi) / 2; if (n->next[tail[mid]]->at < k->at) lo = mid + 1; else hi = mid; }
    prev[i] = lo ? tail[lo - 1] : -1;
    tail[lo] = i;
    if (lo == len) len++;
  }
  for (int i = len ? tail[len - 1] : -1; i >= 0; i = prev[i]) n->next[i]->stays = true;
  free(tail);
}

/* A node's scope ends: its kids are known. What the last build had and this one has not goes; the new and the moved
   are placed when the build is over - or at once, in a view made in this build. */
static void ui_finish(ui_root *r, ui_node *n) {
  if (!n) return;
  for (int i = 0; i < n->nkids; i++) if (!n->kids[i]->seen) ui_record(r, OP_REMOVE, n, n->kids[i], NULL);
  if (n->fresh) {
    for (int i = 0; i < n->nnext; i++) {
      rk_view *v = n->next[i]->view;
      if (rk_view_parent(v) != n->view) rk_add(n->view, rk_retain(v));
    }
  } else if (n->nnext) {
    ui_stable(n);
    for (int i = 0; i < n->nnext; i++) {
      ui_node *k = n->next[i];
      if (k->replaces) ui_record(r, OP_REPLACE, n, k, NULL);
      if (!k->stays) ui_record(r, OP_PLACE, n, k, i ? n->next[i - 1] : NULL);
    }
  }
  ui_node **t = n->kids; int tc = n->kcap;
  n->kids = n->next; n->nkids = n->nnext; n->kcap = n->ncap;
  n->next = t; n->ncap = tc; n->nnext = 0;
  for (int i = 0; i < n->nkids; i++) n->kids[i]->seen = false;
  n->fresh = false;
  n->from = 0;
}

void rk_ui_open(const rk_ui *in) {
  ui_root *r = ui_cur;
  if (!r) { rk__error("rk_ui_open: only inside a declared view's build (rk_ui_view)"); return; }
  ui_node *p = r->stack[r->depth - 1], *n = NULL;
  if (p && RK__SIZED(in, "rk_ui_open", "rk_ui")) {
    rk_ui d;
    memset(&d, 0, sizeof d);
    memcpy(&d, in, RK_MIN((size_t)in->size, sizeof d));
    if (d.kind <= RK_UI_NONE || d.kind > RK_UI_HOST) rk__error("rk_ui_open: no kind of view is %d", (int)d.kind);
    else if (!ui_holds(p->kind)) rk__error("rk_ui_open: %s holds no views (%s inside it)", ui_kind_name(p->kind), ui_kind_name(d.kind));
    else if ((d.kind == RK_UI_ROW) != (p->kind == RK_UI_ROWS)) rk__error("rk_ui_open: an RK_ROW goes in RK_ROWS, and RK_ROWS holds only rows (%s in %s)", ui_kind_name(d.kind), ui_kind_name(p->kind));
    else {
      ui_node *old = ui_find(p, &d);
      if (old && ui_made_with(old, &d)) { n = old; ui_update(n, &d, false); }
      else { n = ui_create(r, p, &d); if (old) { n->replaces = old; n->at = old->at; } }
      p->next = rk__grow(p->next, &p->ncap, p->nnext + 1, sizeof *p->next);
      p->next[p->nnext++] = n;
    }
  }
  ui_push(r, n);                                                  /* a refused one too: its close still comes */
}

void rk_ui_close(void) {
  ui_root *r = ui_cur;
  if (!r) { rk__error("rk_ui_close: only inside a declared view's build (rk_ui_view)"); return; }
  if (r->depth <= 1) { rk__warn("rk_ui_close: more closes than opens"); return; }
  ui_finish(r, r->stack[--r->depth]);
}

void rk_ui_put(const rk_ui *ui) {
  if (!ui_cur) { rk__error("rk_ui_put: only inside a declared view's build (rk_ui_view)"); return; }
  rk_ui_open(ui);
  rk_ui_close();
}

/* After the build ------------------------------------------------------------------------------------------------- */

static int ui_index_of(rk_view *parent, rk_view *v) {
  int n = rk_view_count(parent);
  for (int i = 0; i < n; i++) if (rk_view_child(parent, i) == v) return i;
  return -1;
}

/* Puts v at index in parent; focus inside it stays there (taking a view out of its parent lets focus go). */
static void ui_place_at(rk_view *parent, rk_view *v, int index) {
  rk_window *w = rk_view_window(v);
  rk_view *f = w ? rk_window_focused(w) : NULL;
  bool keep = f && rk__view_within(f, v);
  if (rk_view_parent(v) == parent) {
    int at = ui_index_of(parent, v);
    if (at < index) index--;                                   /* it leaves its old place first */
    if (at == index) return;
  } else if (!rk_view_parent(v)) rk_retain(v);                 /* the tree takes this one; the node keeps its own */
  rk_insert(parent, v, index);
  if (keep) rk_window_focus(w, f);
}

static void ui_run(ui_root *r) {
  int removes = 0, places = 0;
  for (int i = 0; i < r->nops; i++) { removes += r->ops[i].kind == OP_REMOVE; places += r->ops[i].kind == OP_PLACE; }
  /* a view made anew for a property its kind takes only when made: in the old one's place, at once */
  for (int i = 0; i < r->nops; i++) {
    ui_op *o = &r->ops[i];
    if (o->kind != OP_REPLACE) continue;
    ui_node *old = o->node->replaces;
    rk_view *ov = old->view, *nv = o->node->view;
    rk_window *w = rk_view_window(ov);
    bool focus = w && rk__view_within(rk_window_focused(w), ov);
    ui_place_at(o->parent->view, nv, ui_index_of(o->parent->view, ov));
    rk_view_remove(ov);
    ui_node_free(old);
    o->node->replaces = NULL;
    if (focus) { rk_view *f = rk__first_focusable(nv); if (f) rk_window_focus(w, f); }
  }
  if (removes) {
    rk_animate_begin(RK_MOTION_LEAVE);                         /* it leaves first, then the rest close up */
    for (int i = 0; i < r->nops; i++) if (r->ops[i].kind == OP_REMOVE) { rk_view_remove(r->ops[i].node->view); ui_node_free(r->ops[i].node); }
    rk_animate_commit();
  }
  if (places) {
    rk_animate_begin(r->entering ? RK_MOTION_ENTER : RK_MOTION_REFLOW);   /* the new build in; the rest make room */
    for (int i = 0; i < r->nops; i++) {
      ui_op *o = &r->ops[i];
      if (o->kind == OP_PLACE) ui_place_at(o->parent->view, o->node->view, o->prev ? ui_index_of(o->parent->view, o->prev->view) + 1 : 0);
    }
    rk_animate_commit();
  } else if (r->reflow && !removes) {
    rk_animate_begin(RK_MOTION_REFLOW);                        /* a new span or size: it grows, the others make room */
    rk_animate_commit();
  }
  r->nops = 0;
}

static void ui_build_once(ui_root *r) {
  ui_root *outer = ui_cur;
  ui_cur = r;
  r->building = true; r->nops = 0; r->reflow = r->entering = false;
  r->depth = 0;
  ui_push(r, &r->top);
  r->build(r->user);
  if (r->depth > 1) {
    rk__warn("a declared view's build left %d scopes open (a return or break inside one?): closed at its end", r->depth - 1);
    while (r->depth > 1) rk_ui_close();
  }
  ui_finish(r, &r->top);
  r->depth = 0;
  r->building = false;
  ui_cur = outer;
  ui_run(r);
}

static void ui_rebuild(ui_root *r) {
  if (r->building || r->handling) { r->pending = true; return; }   /* once the build or the handler is done */
  rk_view *hold = rk_retain(r->top.view);
  int pass = 0;
  do { r->pending = false; ui_build_once(r); } while (r->pending && ++pass < 8);
  if (r->pending) { r->pending = false; rk__warn("rk_ui_update: a build asks for another build each time (8 in a row): stopped"); }
  rk_release(hold);
}

/* A handler declared in the description, then the build: the handler only changes the state. */
static void ui_call(ui_node *n, void (*fn)(rk_view *, void *), rk_view *v) {
  ui_root *r = n->root;
  rk_view *hold = rk_retain(r->top.view);
  r->handling++;
  fn(v, n->user ? n->user : r->user);                          /* n may be gone after this */
  r->handling--;
  ui_rebuild(r);
  rk_release(hold);
}
static void ui_pressed(rk_view *v, void *u) { ui_node *n = u; if (n->on_press) ui_call(n, n->on_press, v); }
static void ui_changed(rk_view *v, void *u) { ui_node *n = u; if (n->on_change) ui_call(n, n->on_change, v); }
static void ui_submitted(rk_view *v, void *u) { ui_node *n = u; if (n->on_submit) ui_call(n, n->on_submit, v); }

/* The declared view -------------------------------------------------------------------------------------------------- */

static void ui_root_destroy(void *state) {
  ui_root *r = state;
  for (ui_root **p = &ui_roots; *p; p = &(*p)->next_root) if (*p == r) { *p = r->next_root; break; }
  for (int i = 0; i < r->top.nkids; i++) ui_node_free(r->top.kids[i]);
  free(r->top.kids); free(r->top.next); free(r->stack); free(r->ops);
  free(r);
}
/* what the build declares fills it, as an overlay: one view, or several over each other */
static const rk_class ui_root_class = { { sizeof(rk_view_class), "rk_ui", NULL, NULL, NULL, NULL, ui_root_destroy }, NULL, NULL };

rk_view *rk_ui_view(void (*build)(void *user), void *user) {
  ui_root *r = rk__alloc(sizeof *r);
  r->build = build; r->user = user;
  r->top.root = r; r->top.fresh = true; r->top.at = -1;
  rk_view *v = rk__view_create(&ui_root_class, r);
  v->layout = RK_LAYOUT_OVERLAY;
  r->top.view = v;
  r->next_root = ui_roots; ui_roots = r;
  if (!build) rk__error("rk_ui_view: no build function: the view stays empty");
  else ui_rebuild(r);
  return v;
}

void rk_ui_update(rk_view *view) {
  if (view) {
    if (!rk__is_object(view, &rk__view_type) || view->cls != &ui_root_class) { rk__error("rk_ui_update: not a declared view (rk_ui_view)"); return; }
    ui_root *r = view->state;
    if (r->build) ui_rebuild(r);
    return;
  }
  /* every one: held while they build, as a build may let another go */
  int n = 0, cap = 0;
  rk_view **all = NULL;
  for (ui_root *r = ui_roots; r; r = r->next_root) { all = rk__grow(all, &cap, n + 1, sizeof *all); all[n++] = rk_retain(r->top.view); }
  for (int i = 0; i < n; i++) { ui_root *r = all[i]->state; if (r->build) ui_rebuild(r); }
  for (int i = 0; i < n; i++) rk_release(all[i]);
  free(all);
}

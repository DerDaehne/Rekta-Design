/* declare.c: declared UI (rekta.h), standing on the view tree and the controllers.

   A build says what a window shows, node by node. Each node is matched as it is declared with the one the last build
   made at its place - by key or id, else as the next unkeyed node of its kind - and only what differs from the last
   description is set on its view, with the setters an app would call. What comes, goes or moves is gathered while the
   build runs and done when it ends, so the design's motion tells the change: first what leaves (RK_MOTION_LEAVE: it
   fades, the rest close up after it), then what enters or moves (RK_MOTION_ENTER, or RK_MOTION_REFLOW when nothing is
   new), then navigation - a place opened turns, a sheet comes down, a page slides. A change of content takes its
   transition before its setter: a tile's new value peeks in, a new status turns it over.

   A node keeps a copy of what was declared last and never reads its view back. A value the app holds (.on, .chosen,
   .number, .day, .buffer, .open: a pointer) is what the view shows after every build; the user's change is written
   there first and the build that follows agrees. A value only given (.value, .input) is where the view starts, and is
   set again only when the declaration moves it, so the user's change stands until the app says otherwise.

   Handlers never build: they mark the app's state as maybe changed, and every declared window builds at the next safe
   point (rk__ui_settle) - after an event, a timer or post of the app's, a look change, a close asked for, a motion's
   end, and before each frame. The library's own timers (a caret, a tooltip) build nothing. A controller's parts - a
   panorama's sections, a pivot's pages, a table's columns, a tree's items - come, go and move in place; only a places
   whose first place is another opens anew. */
#include "rk_internal.h"
#include "rk_tokens.h"
#include <stdarg.h>

typedef struct ui_root ui_root;
typedef struct ui_node ui_node;

/* What each kind is to its parent: a view in its views, a part of it (a section, a page, a column, an item), something
   presented over the window (a sheet), or an entry of a menu. And what it holds. */
enum { R_VIEW, R_SLOT, R_FLOAT, R_ENTRY };
enum { H_NONE, H_VIEWS, H_ROWS, H_TWO, H_COLUMNS, H_ITEMS, H_SECTIONS, H_PAGES, H_PLACES };
typedef struct kind_info { const char *name; unsigned char role, holds; float grow; } kind_info;
static const kind_info ui_kinds[] = {
  [RK_UI_NONE] = { "the top", R_VIEW, H_VIEWS, 0 },
  [RK_UI_VSTACK] = { "RK_VSTACK", R_VIEW, H_VIEWS, 0 },     [RK_UI_HSTACK] = { "RK_HSTACK", R_VIEW, H_VIEWS, 0 },
  [RK_UI_TILES] = { "RK_TILES", R_VIEW, H_VIEWS, 0 },       [RK_UI_ROWS] = { "RK_ROWS", R_VIEW, H_ROWS, 0 },
  [RK_UI_SCROLL] = { "RK_SCROLL", R_VIEW, H_VIEWS, 1 },     [RK_UI_SPLIT] = { "RK_SPLIT", R_VIEW, H_TWO, 1 },
  [RK_UI_LABEL] = { "RK_LABEL", R_VIEW, H_NONE, 0 },        [RK_UI_BUTTON] = { "RK_BUTTON", R_VIEW, H_NONE, 0 },
  [RK_UI_TILE] = { "RK_TILE", R_VIEW, H_NONE, 0 },          [RK_UI_ROW] = { "RK_ROW", R_VIEW, H_NONE, 0 },
  [RK_UI_ICON] = { "RK_ICON", R_VIEW, H_NONE, 0 },          [RK_UI_PROGRESS] = { "RK_PROGRESS", R_VIEW, H_NONE, 0 },
  [RK_UI_IMAGE] = { "RK_IMAGE", R_VIEW, H_NONE, 0 },        [RK_UI_SPACER] = { "RK_SPACER", R_VIEW, H_NONE, 1 },
  [RK_UI_FIELD] = { "RK_FIELD", R_VIEW, H_NONE, 0 },        [RK_UI_CHECK] = { "RK_CHECK", R_VIEW, H_NONE, 0 },
  [RK_UI_TOGGLE] = { "RK_TOGGLE", R_VIEW, H_NONE, 0 },      [RK_UI_CHOICES] = { "RK_CHOICES", R_VIEW, H_NONE, 0 },
  [RK_UI_SEGMENTS] = { "RK_SEGMENTS", R_VIEW, H_NONE, 0 },  [RK_UI_SLIDER] = { "RK_SLIDER", R_VIEW, H_NONE, 0 },
  [RK_UI_STEPPER] = { "RK_STEPPER", R_VIEW, H_NONE, 0 },    [RK_UI_SELECT] = { "RK_SELECT", R_VIEW, H_NONE, 0 },
  [RK_UI_DATE] = { "RK_DATE", R_VIEW, H_NONE, 0 },          [RK_UI_DROP] = { "RK_DROP", R_VIEW, H_NONE, 0 },
  [RK_UI_LIST] = { "RK_LIST", R_VIEW, H_NONE, 1 },          [RK_UI_TABLE] = { "RK_TABLE", R_VIEW, H_COLUMNS, 1 },
  [RK_UI_COLUMN] = { "RK_COLUMN", R_SLOT, H_NONE, 0 },      [RK_UI_TREE] = { "RK_TREE", R_VIEW, H_ITEMS, 0 },
  [RK_UI_ITEM] = { "RK_ITEM", R_SLOT, H_ITEMS, 0 },
  [RK_UI_SEPARATOR] = { "RK_SEPARATOR", R_ENTRY, H_NONE, 0 }, [RK_UI_HEADING] = { "RK_HEADING", R_ENTRY, H_NONE, 0 },
  [RK_UI_PANORAMA] = { "RK_PANORAMA", R_VIEW, H_SECTIONS, 1 }, [RK_UI_SECTION] = { "RK_SECTION", R_SLOT, H_VIEWS, 0 },
  [RK_UI_PIVOT] = { "RK_PIVOT", R_VIEW, H_PAGES, 1 },       [RK_UI_PAGE] = { "RK_PAGE", R_SLOT, H_VIEWS, 0 },
  [RK_UI_PLACES] = { "RK_PLACES", R_VIEW, H_PLACES, 1 },    [RK_UI_PLACE] = { "RK_PLACE", R_SLOT, H_VIEWS, 0 },
  [RK_UI_SHEET] = { "RK_SHEET", R_FLOAT, H_VIEWS, 0 },      [RK_UI_CONFIRM] = { "RK_CONFIRM", R_FLOAT, H_NONE, 0 },
  [RK_UI_HOST] = { "RK_HOST", R_VIEW, H_NONE, 0 },
};
#define LAST_KIND RK_UI_HOST
static const kind_info *ui_info(rk_ui_kind k) { return &ui_kinds[k >= 0 && k <= LAST_KIND ? k : RK_UI_NONE]; }
static bool ui_is_controller(rk_ui_kind k) { return k == RK_UI_PANORAMA || k == RK_UI_PIVOT || k == RK_UI_PLACES; }

struct ui_node {
  rk_ui last;                                      /* as declared last: its strings, series and options its own */
  ui_root *root; ui_node *parent;
  rk_view *view;                                   /* where what it holds goes; a leaf's own view */
  rk_view *outer;                                  /* what stands in its parent's views: a scroll, a split, a controller's */
  rk_controller *ctl;                              /* a panorama, pivot or places; a page's, place's or sheet's own */
  rk_view *panes[2];                               /* a split's */
  rk_view *menu; rk_ui *entries; int nentries;     /* the menu it opens, as declared last */
  ui_node **kids; int nkids, kcap;                 /* as they stand */
  ui_node **next; int nnext, ncap;                 /* as this build declares them */
  ui_node **hash; int hcap;                        /* by key, while a build looks them up */
  ui_node **pushed; int npushed, pcap;             /* places: the places open, in order */
  ui_node **items; int icap;                       /* a tree: its items by number */
  ui_node **slots; int nslots, slcap;              /* a panorama's sections, a pivot's pages, a table's columns: as it
                                                      holds them, in order */
  ui_node *tree;                                   /* an item's tree */
  ui_node *replaces;                               /* the node whose view this new one takes the place of */
  rk_controller *asked;                            /* a confirmation's sheet, while it asks */
  double value;                                    /* the value last set: bound or given */
  char *input;                                     /* a field's text, as last set or typed */
  int at, from, ufrom, num;                        /* num: an item's number in its tree, a column's, a section's or a
                                                      page's index; -1 until it is added */
  uint32_t pulse;
  bool seen, fresh, stale, opened, presented, quiet, hooked;
  bool moved;                                      /* the declaration moved its value this build */
  bool still;                                      /* in a scope declared RK_UI_IN_PLACE: nothing it holds moves */
  bool closed_by_user, warned;                     /* closed by the user without .open; that it came back was said */
  bool reorder;                                    /* a tree or an item: its items stand in another order */
};

struct ui_root {
  ui_node top;
  void (*build)(void *user); void *user;
  rk_controller *host;                             /* a window's: holds what its build declares */
  ui_node **stack; rk_controller **owners; int depth, scap;
  ui_node **gone; int ngone, gcap;                 /* what the build no longer declares */
  ui_node **swaps; int nswaps, swcap;              /* new nodes in an old one's place */
  ui_node **holders; int nholders, hdcap;          /* nodes whose views to arrange */
  ui_node **navs; int nnavs, nvcap;                /* navigation to bring in step after the views */
  rk_ui *collect; int ncollect, ccap; bool collecting;   /* a menu being declared */
  bool building, pending, reflow, inset;
  char *arena; size_t arena_used, arena_cap;       /* rk_ui_fmt's text, until the build ends */
  ui_node *focus;                                  /* a view to focus once it stands where it goes */
  rk_window *window; bool (*on_key)(rk_window *, uint32_t, uint32_t, void *);   /* a window's */
  ui_root *next_root;
};

static ui_root *ui_roots;                             /* every declared view alive */
static ui_root *ui_cur;                               /* the one building */

static void *ui_root_user(ui_node *n, const rk_ui *d) { return d->user ? d->user : n->root->user; }
static void *ui_node_user(ui_node *n) { return ui_root_user(n, &n->last); }

/* What was declared last ------------------------------------------------------------------------------------------ */

#define F(field) offsetof(rk_ui, field)
static const size_t ui_strings[] = { F(key), F(text), F(label), F(meta), F(message), F(hint), F(error), F(placeholder), F(icon),
                                  F(shortcut), F(tooltip), F(access), F(accept), F(ok), F(cancel), F(input) };
#undef F
#define STR(u, off) (*(const char **)((char *)(u) + (off)))

static bool ui_same(const char *a, const char *b) { return a == b || (a && b && !strcmp(a, b)); }
static bool ui_same_series(const float *a, int na, const float *b, int nb) {
  if (!a) na = 0;
  if (!b) nb = 0;
  return na == nb && (!na || !memcmp(a, b, (size_t)na * sizeof *a));
}
static bool ui_same_options(const char *const *a, int na, const char *const *b, int nb) {
  if (!a) na = 0;
  if (!b) nb = 0;
  if (na != nb) return false;
  for (int i = 0; i < na; i++) if (!ui_same(a[i], b[i])) return false;
  return true;
}
static int ui_options_count(const rk_ui *d) { return d->options ? RK_MAX(d->count, 0) : 0; }

/* last = d, keeping last's own copies of what did not change */
static void ui_keep(rk_ui *last, const rk_ui *d) {
  rk_ui k = *d;
  k.size = sizeof k;
  for (int i = 0; i < RK_COUNT(ui_strings); i++) {
    char *old = (char *)STR(last, ui_strings[i]);
    const char *now = STR(d, ui_strings[i]);
    if (ui_same(old, now)) STR(&k, ui_strings[i]) = old;
    else { free(old); STR(&k, ui_strings[i]) = rk__strdup(now); }
  }
  if (ui_same_series(last->series, last->nseries, d->series, d->nseries)) { k.series = last->series; k.nseries = last->series ? last->nseries : 0; }
  else {
    free((void *)last->series);
    int n = d->series ? RK_MAX(d->nseries, 0) : 0;
    float *s = n ? rk__alloc((size_t)n * sizeof *s) : NULL;
    if (n) memcpy(s, d->series, (size_t)n * sizeof *s);
    k.series = s; k.nseries = n;
  }
  int no = ui_options_count(d), lo = ui_options_count(last);
  if (ui_same_options(last->options, lo, d->options, no)) { k.options = last->options; }
  else {
    for (int i = 0; i < lo; i++) free((void *)last->options[i]);
    free((void *)last->options);
    char **o = no ? rk__alloc((size_t)no * sizeof *o) : NULL;
    for (int i = 0; i < no; i++) o[i] = rk__strdup(d->options[i] ? d->options[i] : "");
    k.options = (const char *const *)o;
  }
  if (!k.options) k.count = d->kind == RK_UI_LIST || d->kind == RK_UI_TABLE ? d->count : 0;
  *last = k;
}

static void ui_drop_last(rk_ui *last) {
  for (int i = 0; i < RK_COUNT(ui_strings); i++) free((void *)STR(last, ui_strings[i]));
  free((void *)last->series);
  for (int i = 0; i < ui_options_count(last); i++) free((void *)last->options[i]);
  free((void *)last->options);
  memset(last, 0, sizeof *last);
}

/* The values a build gives: bound, or given */
static int ui_ymd(const rk_day *d) { return d->year > 0 ? d->year * 10000 + d->month * 100 + d->day : 0; }
static double ui_value_of(const rk_ui *d) {
  switch (d->kind) {
    case RK_UI_CHECK: case RK_UI_TOGGLE: return d->on ? (*d->on ? 1 : 0) : (d->value != 0);
    case RK_UI_CHOICES: case RK_UI_SEGMENTS: case RK_UI_SELECT: case RK_UI_PIVOT: case RK_UI_LIST: case RK_UI_TABLE:
      return d->chosen ? *d->chosen : d->value;
    case RK_UI_SLIDER: case RK_UI_STEPPER: return d->number ? *d->number : d->value;
    case RK_UI_DATE: return d->day ? ui_ymd(d->day) : d->value;
    default: return d->value;
  }
}
static bool ui_is_bound(const rk_ui *d) { return d->on || d->chosen || d->number || d->day; }
static bool ui_given_value(const rk_ui *d) {          /* a list or a table without .chosen shows what the user picks */
  if (d->kind == RK_UI_LIST || d->kind == RK_UI_TABLE) return d->chosen != NULL;
  return true;
}
static const char *ui_text_of(const rk_ui *d) { return d->buffer ? d->buffer : d->input; }
/* Open: bound; else a place, a sheet or a confirmation declared is open, a split's detail and a branch are the user's */
static int ui_open_of(const rk_ui *d) {
  if (d->open) return *d->open;
  return d->kind == RK_UI_PLACE || d->kind == RK_UI_SHEET || d->kind == RK_UI_CONFIRM ? 1 : -1;
}
static float ui_range_max(const rk_ui *d) { return d->max > d->min ? d->max : d->min == 0 && d->max == 0 ? 100 : d->min; }

/* Nodes ----------------------------------------------------------------------------------------------------------- */

static ui_node *ui_node_new(ui_root *r, ui_node *parent, rk_ui_kind kind) {
  ui_node *n = rk__alloc(sizeof *n);
  n->root = r; n->parent = parent; n->last.kind = kind; n->fresh = true;
  n->at = n->num = -1;
  return n;
}

/* Its views call nothing of the app's or of ours any more: it is leaving, or replaced. What it holds too. */
static void ui_unhook(ui_node *n) {
  for (int i = 0; i < n->nkids; i++) ui_unhook(n->kids[i]);
  n->hooked = false;
  rk_view *v = n->view;
  rk_ui_kind k = n->last.kind;
  if (n->outer && n->last.on_press && ui_info(k)->role == R_VIEW) rk_view_on_activate(n->outer, NULL, NULL);
  if (n->menu) rk_menu_clear(n->menu);
  switch (k) {
    case RK_UI_FIELD: rk_field_on_change(v, NULL, NULL); rk_field_on_submit(v, NULL, NULL); break;
    case RK_UI_CHECK: case RK_UI_TOGGLE: case RK_UI_CHOICES: case RK_UI_SEGMENTS: case RK_UI_SLIDER: case RK_UI_STEPPER:
    case RK_UI_SELECT: case RK_UI_DATE: rk_control_on_change(v, NULL, NULL); break;
    case RK_UI_LIST: case RK_UI_TABLE: rk_list_on_show(v, NULL, NULL); rk_list_on_change(v, NULL, NULL);
      if (k == RK_UI_TABLE) { rk_table_on_sort(v, NULL, NULL); rk_table_on_edit(v, NULL, NULL); }
      break;
    case RK_UI_TREE: rk_tree_on_show(v, NULL, NULL); rk_tree_on_toggle(v, NULL, NULL); break;
    case RK_UI_ROWS: if (n->last.on_move) rk_rows_set_sortable(v, NULL, NULL); break;
    case RK_UI_DROP: rk__drop_detach(v); break;
    case RK_UI_ITEM: if (n->tree && n->num >= 0 && n->num < n->tree->icap) n->tree->items[n->num] = NULL; break;
    case RK_UI_SPLIT: rk_split_on_back(n->outer, NULL, NULL); break;
    case RK_UI_PIVOT: case RK_UI_PLACES: case RK_UI_PANORAMA:
      if (k == RK_UI_PIVOT) rk_pivot_on_change(n->ctl, NULL, NULL);
      if (k == RK_UI_PLACES) rk_places_on_back(n->ctl, NULL, NULL);
      rk__controller_unembed(n->ctl);
      break;
    case RK_UI_SHEET:
      rk_controller_on_dismiss(n->ctl, NULL, NULL);
      if (n->presented) { n->presented = false; rk_controller_dismiss(n->ctl); }
      break;
    case RK_UI_CONFIRM:
      if (n->asked) { rk_controller *s = n->asked; n->asked = NULL; n->quiet = true; rk_controller_dismiss(s); rk_release(s); }
      break;
    default: break;
  }
}

/* Frees it with what it holds; touches nothing outside it (a batch may be freed after its root is gone). */
static void ui_node_free(ui_node *n) {
  for (int i = 0; i < n->nkids; i++) ui_node_free(n->kids[i]);
  free(n->kids); free(n->next); free(n->hash); free(n->pushed); free(n->items); free(n->slots);
  if (n->last.kind == RK_UI_LIST || n->last.kind == RK_UI_TABLE) rk__list_detach(n->view);
  for (int i = 0; i < n->nentries; i++) ui_drop_last(&n->entries[i]);
  free(n->entries);
  if (n->asked) rk_release(n->asked);
  rk_release(n->menu);
  rk_release(n->panes[0]); rk_release(n->panes[1]);
  if (n->outer != n->view) rk_release(n->outer);
  rk_release(n->view);
  rk_release(n->ctl);
  ui_drop_last(&n->last);
  free(n->input);
  free(n);
}

static void ui_free_batch(void *user) {
  ui_node **batch = user;
  for (ui_node **b = batch; *b; b++) ui_node_free(*b);
  free(batch);
}

/* Handlers: the app's state changes; every declared view builds before the window shows it ---------------------- */

static bool ui_dirty;                              /* the app may have changed its state: build at the next settle */
static int ui_calling;                             /* the app's code running from here: never a build inside it */
#define UI_APP(stmt) do { ui_calling++; stmt; ui_calling--; ui_dirty = true; } while (0)

static void ui_call(ui_node *n, void (*fn)(rk_view *, void *), rk_view *v) { if (fn) UI_APP(fn(v, ui_node_user(n))); else ui_dirty = true; }

static void ui_pressed(rk_view *v, void *u) { ui_node *n = u; if (n->hooked) ui_call(n, n->last.on_press, v); }
static void ui_submitted(rk_view *v, void *u) { ui_node *n = u; if (n->hooked) ui_call(n, n->last.on_submit, v); }

/* the user changed a value: written where it is bound, then told */
static void ui_changed(rk_view *v, void *u) {
  ui_node *n = u;
  if (!n->hooked) return;
  rk_ui *d = &n->last;
  switch (d->kind) {
    case RK_UI_FIELD:
      if (d->buffer && d->buffer_size) {
        const char *t = rk_field_text(v);
        size_t len = strlen(t), cap = d->buffer_size - 1;
        if (len > cap) { len = cap; while (len && ((unsigned char)t[len] & 0xc0) == 0x80) len--; }   /* never half a character */
        memcpy(d->buffer, t, len); d->buffer[len] = 0;
      }
      break;
    case RK_UI_DATE:
      if (d->day) { int y = 0, m = 0, dd = 0; rk_date_value(v, &y, &m, &dd); d->day->year = y; d->day->month = m; d->day->day = dd; }
      break;
    default: {
      double x = rk_control_value(v);
      if (d->on) *d->on = x != 0;
      else if (d->chosen) *d->chosen = (int)x;
      else if (d->number) *d->number = x;
      break;
    }
  }
  ui_call(n, d->on_change, v);
}

/* Lists and tables: the node feeds them while it lives */
static void ui_list_item(int index, rk_list_item *out, void *u) { ui_node *n = u; if (n->last.item) n->last.item(index, out, ui_node_user(n)); }
static uint64_t ui_list_key(int index, void *u) { ui_node *n = u; return n->last.item_key ? n->last.item_key(index, ui_node_user(n)) : (uint64_t)index; }
static const char *ui_table_cell(int row, int column, void *u) { ui_node *n = u; return n->last.cell ? n->last.cell(row, column, ui_node_user(n)) : ""; }
static void ui_list_shown(rk_view *v, int index, void *u) {
  ui_node *n = u;
  if (!n->hooked) return;
  if (n->last.chosen) { *n->last.chosen = index; n->value = index; }   /* shown by the user: not to be shown again */
  if (n->last.on_select) UI_APP(n->last.on_select(v, index, ui_node_user(n))); else ui_dirty = true;
}
static void ui_list_changed(rk_view *v, void *u) { ui_node *n = u; if (n->hooked) ui_call(n, n->last.on_change, v); }
static void ui_table_sorted(rk_view *v, int column, rk_sort sort, void *u) {
  ui_node *n = u;
  if (n->hooked && n->last.on_sort) UI_APP(n->last.on_sort(v, column, sort, ui_node_user(n)));
}
static const char *ui_table_edited(rk_view *v, int row, int column, const char *value, void *u) {
  ui_node *n = u;
  const char *why = NULL;
  if (n->hooked && n->last.on_edit) UI_APP(why = n->last.on_edit(v, row, column, value, ui_node_user(n)));
  return why;
}
static void ui_rows_moved(rk_view *v, int from, int to, void *u) {
  ui_node *n = u;
  if (!n->hooked) return;
  /* the rows moved under the build's feet: its order follows, so the next build finds them where they are */
  if (from >= 0 && from < n->nkids && to >= 0 && to < n->nkids) {
    ui_node *k = n->kids[from];
    if (from < to) memmove(n->kids + from, n->kids + from + 1, (size_t)(to - from) * sizeof *n->kids);
    else memmove(n->kids + to + 1, n->kids + to, (size_t)(from - to) * sizeof *n->kids);
    n->kids[to] = k;
  }
  if (n->last.on_move) UI_APP(n->last.on_move(v, from, to, ui_node_user(n))); else ui_dirty = true;
}
static void ui_drop_files(rk_view *v, const char *const *taken, int nt, const char *const *refused, int nr, void *u) {
  ui_node *n = u;
  if (n->hooked && n->last.on_files) UI_APP(n->last.on_files(v, taken, nt, refused, nr, ui_node_user(n)));
}

/* Trees: an item shown, a branch opened or closed. An item's choice is its own .chosen, or its tree's. */
static ui_node *ui_item_of(ui_node *tree, int num) { return num >= 0 && num < tree->icap ? tree->items[num] : NULL; }
static int *ui_item_chosen(ui_node *it) { return it->last.chosen ? it->last.chosen : it->tree ? it->tree->last.chosen : NULL; }
static void ui_tree_shown(rk_view *v, int num, void *u) {
  ui_node *t = u, *it = t->hooked ? ui_item_of(t, num) : NULL;
  if (!it) return;
  int *chosen = ui_item_chosen(it);
  if (chosen) *chosen = (int)it->last.value;       /* the item shown, where the app keeps it */
  ui_call(it, it->last.on_press ? it->last.on_press : t->last.on_press, v);
}
static void ui_tree_toggled(rk_view *v, int num, bool open, void *u) {
  ui_node *t = u, *it = t->hooked ? ui_item_of(t, num) : NULL;
  if (!it) return;
  it->opened = open;
  if (it->last.open) *it->last.open = open;
  ui_call(it, it->last.on_change, v);
}

/* Navigation: what the user did. Without .open the declaration decides: if it still says open, it opens again -
   and that is said, once, as it is surely not what the app meant. */
static void ui_closed_by_user(ui_node *n, rk_view *v) {
  if (n->last.open) *n->last.open = false;
  else n->closed_by_user = true;
  ui_call(n, n->last.on_close, v);
}
static void ui_pivot_heard(rk_controller *what, int index, void *u) {
  (void)what;
  ui_node *n = u;
  if (!n->hooked) return;
  if (n->last.chosen) *n->last.chosen = index;
  ui_call(n, n->last.on_change, n->outer);
}
static void ui_places_heard(rk_controller *what, void *u) {
  ui_node *n = u;
  if (!n->hooked || n->npushed <= rk_places_depth(what)) return;
  ui_node *gone = n->pushed[--n->npushed];               /* the place the user left */
  gone->opened = false;
  ui_closed_by_user(gone, gone->view);
}
static void ui_sheet_heard(rk_controller *what, void *u) {
  (void)what;
  ui_node *n = u;
  if (!n->hooked) return;
  n->presented = false;
  ui_closed_by_user(n, n->view);
}
static void ui_split_heard(rk_view *what, void *u) {
  (void)what;
  ui_node *n = u;
  if (!n->hooked) return;
  n->opened = false;
  ui_closed_by_user(n, n->outer);
}
static void ui_confirm_answered(bool ok, void *u) {
  ui_node *n = u;
  if (n->asked) { rk_release(n->asked); n->asked = NULL; }
  if (n->quiet) { n->quiet = false; return; }            /* the app closed it */
  if (!n->hooked) return;
  if (n->last.open) *n->last.open = false;
  else n->closed_by_user = true;
  ui_call(n, ok ? n->last.on_press : n->last.on_close, n->parent ? n->parent->view : NULL);
}

/* Menus: declared by a function of their own, built again when what they say changed ---------------------------- */

static void ui_menu_pressed(rk_view *menu, int item, void *u) {
  (void)menu;
  ui_node *n = u;
  if (!n->hooked || item < 0 || item >= n->nentries || n->entries[item].kind != RK_UI_ITEM) return;   /* indices count separators too */
  rk_ui *e = &n->entries[item];
  if (e->on) *e->on = !*e->on;
  else if (e->chosen) *e->chosen = (int)e->value;
  if (e->on_press) UI_APP(e->on_press(n->outer, e->user ? e->user : n->root->user)); else ui_dirty = true;
}

static rk_checkmark ui_entry_check(const rk_ui *e) {
  if (e->on) return *e->on ? RK_CHECKMARK_ON : RK_CHECKMARK_OFF;
  if (e->chosen) return *e->chosen == (int)e->value ? RK_CHECKMARK_ON : RK_CHECKMARK_OFF;
  return e->check;
}

static void ui_fill_menu(ui_node *n, const rk_ui *d) {
  ui_root *r = n->root;
  /* declare it */
  ui_root *outer = ui_cur;
  ui_cur = r;
  bool was = r->collecting;
  int base = r->ncollect;
  r->collecting = true;
  d->menu(ui_root_user(n, d));
  r->collecting = was;
  ui_cur = outer;
  int count = r->ncollect - base;
  rk_ui *now = r->collect + base;
  bool rebuilt = count != n->nentries;
  for (int i = 0; i < count && !rebuilt; i++) {
    rk_ui *a = &n->entries[i], *b = &now[i];
    rebuilt = a->kind != b->kind || !ui_same(a->text, b->text) || !ui_same(a->shortcut, b->shortcut);
  }
  if (rebuilt) {
    rk_menu_clear(n->menu);
    for (int i = 0; i < count; i++) {
      rk_ui *e = &now[i];
      if (e->kind == RK_UI_SEPARATOR) rk_menu_add_separator(n->menu);
      else if (e->kind == RK_UI_HEADING) rk_menu_add_label(n->menu, e->text);
      else rk_menu_add(n->menu, e->text, e->shortcut, ui_menu_pressed, n);
    }
  }
  for (int i = 0; i < count; i++) {
    rk_ui *e = &now[i], *was_e = !rebuilt && i < n->nentries ? &n->entries[i] : NULL;
    if (e->kind != RK_UI_ITEM) continue;
    rk_checkmark c = ui_entry_check(e);
    if (rk_menu_check(n->menu, i) != c) rk_menu_set_check(n->menu, i, c);   /* the menu toggles a square itself as it is picked */
    if (!was_e || was_e->disabled != e->disabled || rebuilt) rk_menu_set_enabled(n->menu, i, !e->disabled);
  }
  /* keep it: what it says now, its bindings too */
  if (count > n->nentries) {
    n->entries = rk__realloc(n->entries, (size_t)count * sizeof *n->entries);
    memset(n->entries + n->nentries, 0, (size_t)(count - n->nentries) * sizeof *n->entries);
  }
  for (int i = count; i < n->nentries; i++) ui_drop_last(&n->entries[i]);
  for (int i = 0; i < count; i++) { ui_keep(&n->entries[i], &now[i]); n->entries[i].user = now[i].user ? now[i].user : ui_root_user(n, d); }
  n->nentries = count;
  for (int i = 0; i < count; i++) ui_drop_last(&now[i]);
  r->ncollect = base;
}

static void ui_menu_opening(rk_view *menu, void *u) { (void)menu; ui_node *n = u; if (n->hooked && n->last.menu) ui_fill_menu(n, &n->last); }

/* A menu is declared when it opens, and again at each build while it is open: a closed menu costs nothing. */
static void ui_sync_menu(ui_node *n, const rk_ui *d) {
  if (!d->menu) {
    if (n->menu) { rk_view_set_menu(n->outer, NULL); rk_release(n->menu); n->menu = NULL; }
    for (int i = 0; i < n->nentries; i++) ui_drop_last(&n->entries[i]);
    n->nentries = 0;
    return;
  }
  if (!n->menu) {
    n->menu = rk_menu();
    rk__menu_on_open(n->menu, ui_menu_opening, n);
    rk_view_set_menu(n->outer, rk_retain(n->menu));
  }
  if (rk__menu_open(n->menu)) ui_fill_menu(n, d);
}

/* What each kind reads: anything else declared on it is said, once, instead of passing in silence ------------------ */

enum {
  G_TEXT, G_LABEL, G_META, G_MESSAGE, G_HINT, G_PLACEHOLDER, G_ICON, G_SHORTCUT, G_ACCEPT, G_ANSWERS, G_STYLE, G_COLOR,
  G_BUTTON, G_STATUS, G_FIT, G_SIDE, G_PROGRESS, G_SERIES, G_OPTIONS, G_COUNT, G_VALUE, G_RANGE, G_INPUT, G_CHECK,
  G_LINES, G_DATA, G_ON, G_CHOSEN, G_NUMBER, G_DAY, G_BUFFER, G_OPEN, G_GAP, G_FLAGS, G_ON_CHANGE, G_ON_SUBMIT,
  G_ON_CLOSE, G_ON_SELECT, G_ITEM, G_CELL, G_ON_MOVE, G_ON_FILES, G_HOST, G_VERSION, G_COUNT_OF
};
#define B(g) (1ull << (g))
static const char *const ui_group_names[G_COUNT_OF] = {
  ".text (its first argument)", ".label", ".meta", ".message", ".hint or .error", ".placeholder", ".icon", ".shortcut",
  ".accept", ".ok or .cancel", ".style", ".color", ".button", ".status", ".fit", ".side", ".progress or .busy",
  ".series", ".options", ".count", ".value", ".min, .max or .step", ".input", ".check", ".lines or .secret", ".data",
  ".on", ".chosen", ".number", ".day", ".buffer", ".open", ".gap, .padding, .align or .cross", ".flags or .sort",
  ".on_change", ".on_submit", ".on_close", ".on_select", ".item or .item_key", ".cell, .on_sort or .on_edit",
  ".on_move", ".on_files", ".make or .update", ".version",
};
static uint64_t ui_present(const rk_ui *d) {
  uint64_t m = 0;
  if (d->text) m |= B(G_TEXT);
  if (d->label) m |= B(G_LABEL);
  if (d->meta) m |= B(G_META);
  if (d->message) m |= B(G_MESSAGE);
  if (d->hint || d->error) m |= B(G_HINT);
  if (d->placeholder) m |= B(G_PLACEHOLDER);
  if (d->icon) m |= B(G_ICON);
  if (d->shortcut) m |= B(G_SHORTCUT);
  if (d->accept) m |= B(G_ACCEPT);
  if (d->ok || d->cancel || d->cancel_is_safe) m |= B(G_ANSWERS);
  if (d->style) m |= B(G_STYLE);
  if (d->color) m |= B(G_COLOR);
  if (d->button) m |= B(G_BUTTON);
  if (d->status) m |= B(G_STATUS);
  if (d->fit) m |= B(G_FIT);
  if (d->side) m |= B(G_SIDE);
  if (d->progress || d->busy) m |= B(G_PROGRESS);
  if (d->series || d->nseries) m |= B(G_SERIES);
  if (d->options) m |= B(G_OPTIONS);
  if (d->count && !d->options) m |= B(G_COUNT);
  if (d->value) m |= B(G_VALUE);
  if (d->min || d->max || d->step) m |= B(G_RANGE);
  if (d->input) m |= B(G_INPUT);
  if (d->check) m |= B(G_CHECK);
  if (d->lines || d->secret) m |= B(G_LINES);
  if (d->data || d->data_size) m |= B(G_DATA);
  if (d->on) m |= B(G_ON);
  if (d->chosen) m |= B(G_CHOSEN);
  if (d->number) m |= B(G_NUMBER);
  if (d->day) m |= B(G_DAY);
  if (d->buffer || d->buffer_size) m |= B(G_BUFFER);
  if (d->open) m |= B(G_OPEN);
  if (d->gap || d->padding.top || d->padding.right || d->padding.bottom || d->padding.left || d->align || d->cross) m |= B(G_GAP);
  if (d->flags || d->sort) m |= B(G_FLAGS);
  if (d->on_change) m |= B(G_ON_CHANGE);
  if (d->on_submit) m |= B(G_ON_SUBMIT);
  if (d->on_close) m |= B(G_ON_CLOSE);
  if (d->on_select) m |= B(G_ON_SELECT);
  if (d->item || d->item_key) m |= B(G_ITEM);
  if (d->cell || d->on_sort || d->on_edit) m |= B(G_CELL);
  if (d->on_move) m |= B(G_ON_MOVE);
  if (d->on_files) m |= B(G_ON_FILES);
  if (d->make || d->update) m |= B(G_HOST);
  if (d->version) m |= B(G_VERSION);
  return m;
}
static uint64_t ui_reads(rk_ui_kind k, bool in_menu) {
  const uint64_t box = B(G_GAP) | B(G_VERSION), control = B(G_VALUE) | B(G_ON_CHANGE);
  switch (k) {
    case RK_UI_VSTACK: case RK_UI_HSTACK: case RK_UI_TILES: case RK_UI_SCROLL: return box;
    case RK_UI_ROWS: return box | B(G_ON_MOVE);
    case RK_UI_SPLIT: return B(G_TEXT) | B(G_OPEN) | B(G_ON_CLOSE) | B(G_VERSION);
    case RK_UI_LABEL: return B(G_TEXT) | B(G_STYLE) | B(G_COLOR);
    case RK_UI_BUTTON: return B(G_TEXT) | B(G_BUTTON) | B(G_ICON);
    case RK_UI_TILE: return B(G_TEXT) | B(G_LABEL) | B(G_META) | B(G_STATUS) | B(G_MESSAGE) | B(G_PROGRESS) | B(G_SERIES);
    case RK_UI_ROW: return B(G_TEXT) | B(G_META) | B(G_STATUS);
    case RK_UI_ICON: return B(G_ICON) | B(G_COLOR);
    case RK_UI_PROGRESS: return B(G_PROGRESS);
    case RK_UI_IMAGE: return B(G_TEXT) | B(G_FIT) | B(G_DATA);
    case RK_UI_SPACER: return 0;
    case RK_UI_FIELD: return B(G_TEXT) | B(G_PLACEHOLDER) | B(G_HINT) | B(G_INPUT) | B(G_BUFFER) | B(G_LINES) | B(G_OPTIONS) | B(G_ON_CHANGE) | B(G_ON_SUBMIT);
    case RK_UI_CHECK: case RK_UI_TOGGLE: return B(G_TEXT) | B(G_ON) | control;
    case RK_UI_CHOICES: case RK_UI_SEGMENTS: case RK_UI_SELECT: return B(G_OPTIONS) | B(G_CHOSEN) | control;
    case RK_UI_SLIDER: case RK_UI_STEPPER: return B(G_RANGE) | B(G_NUMBER) | control;
    case RK_UI_DATE: return B(G_DAY) | control;
    case RK_UI_DROP: return B(G_TEXT) | B(G_HINT) | B(G_ACCEPT) | B(G_ON_FILES);
    case RK_UI_LIST: return B(G_COUNT) | B(G_ITEM) | B(G_CHOSEN) | B(G_ON_SELECT) | B(G_ON_CHANGE) | B(G_VERSION);
    case RK_UI_TABLE: return B(G_COUNT) | B(G_CELL) | B(G_ITEM) | B(G_CHOSEN) | B(G_ON_SELECT) | B(G_ON_CHANGE) | B(G_VERSION);
    case RK_UI_COLUMN: return B(G_TEXT) | B(G_FLAGS);
    case RK_UI_TREE: return B(G_CHOSEN) | B(G_VERSION);
    case RK_UI_ITEM: return in_menu ? B(G_TEXT) | B(G_SHORTCUT) | B(G_ON) | B(G_CHOSEN) | B(G_VALUE) | B(G_CHECK)
                                    : B(G_TEXT) | B(G_META) | B(G_STATUS) | B(G_OPEN) | B(G_CHOSEN) | B(G_VALUE) | B(G_ON_CHANGE);
    case RK_UI_SEPARATOR: return 0;
    case RK_UI_HEADING: return B(G_TEXT);
    case RK_UI_PANORAMA: case RK_UI_PLACES: return B(G_TEXT);
    case RK_UI_SECTION: case RK_UI_PAGE: return B(G_TEXT) | box;
    case RK_UI_PIVOT: return B(G_TEXT) | B(G_CHOSEN) | B(G_SIDE) | control;
    case RK_UI_PLACE: case RK_UI_SHEET: return B(G_TEXT) | B(G_OPEN) | B(G_ON_CLOSE) | box;
    case RK_UI_CONFIRM: return B(G_TEXT) | B(G_LABEL) | B(G_MESSAGE) | B(G_ANSWERS) | B(G_OPEN) | B(G_ON_CLOSE);
    case RK_UI_HOST: return B(G_HOST);
    default: return 0;
  }
}
static void ui_check_fields(const rk_ui *d, bool in_menu) {
  static uint64_t said[LAST_KIND + 2];
  int k = d->kind == RK_UI_ITEM && in_menu ? LAST_KIND + 1 : d->kind;
  uint64_t extra = ui_present(d) & ~ui_reads(d->kind, in_menu) & ~said[k];
  for (int g = 0; extra && g < G_COUNT_OF; g++) if (extra & B(g)) {
    said[k] |= B(g); extra &= ~B(g);
    const char *why = g == G_LABEL && (d->kind == RK_UI_FIELD || d->kind == RK_UI_CHECK || d->kind == RK_UI_TOGGLE || d->kind == RK_UI_BUTTON)
                      ? ": its label is the first argument" : g == G_VALUE && ui_is_bound(d) ? ": it is bound" : "";
    rk__warn("%s%s does not read %s%s", ui_info(d->kind)->name, in_menu ? " in a menu" : "", ui_group_names[g], why);
  }
}
#undef B

/* What differs goes to the view ------------------------------------------------------------------------------------- */

static void ui_transition(ui_node *n, const rk_ui *d, rk_ui_change automatic, bool fresh) {
  rk_ui_change c = d->change == RK_UI_AUTO ? automatic : d->change;
  if (fresh || c == RK_UI_IN_PLACE || c == RK_UI_AUTO) return;
  rk_view_transition(n->outer, c == RK_UI_PEEK ? RK_TRANSITION_PEEK : c == RK_UI_FLIP ? RK_TRANSITION_FLIP : RK_TRANSITION_CROSSFADE);
}

static float ui_design_gap(rk_ui_kind k) {
  switch (k) {
    case RK_UI_HSTACK: case RK_UI_TILES: return rk_metric_of(RK_SPACE_S);
    case RK_UI_ROWS: return 0;
    default: return rk_metric_of(RK_SPACE_M);
  }
}
static bool ui_holds_layout(rk_ui_kind k) {           /* gap, padding and alignment go to n->view */
  return k == RK_UI_NONE || k == RK_UI_VSTACK || k == RK_UI_HSTACK || k == RK_UI_TILES || k == RK_UI_SCROLL || k == RK_UI_SECTION ||
         k == RK_UI_PAGE || k == RK_UI_PLACE || k == RK_UI_SHEET;
}

#define CHANGED(field) (fresh || d->field != o->field)
#define CHANGED_S(field) (fresh || !ui_same(d->field, o->field))

static void ui_update(ui_node *n, const rk_ui *d, bool fresh) {
  ui_root *r = n->root;
  rk_view *v = n->view, *outer = n->outer;
  const rk_ui *o = &n->last;
  rk_ui_kind k = d->kind;
  bool layout = false, pulse = !fresh && d->pulse != n->pulse;
  n->pulse = d->pulse;
  /* bound: controlled - what the app holds is what shows, after every build; given: a start, moved only when the
     declaration moves it, else the user's stays. A list's shown item is moved, never held: the user selects several. */
  double val = ui_value_of(d);
  bool moved = !fresh && val != n->value, bound = ui_is_bound(d);
  bool set_val = ui_given_value(d) && !fresh && (k == RK_UI_LIST || k == RK_UI_TABLE ? moved : bound || moved);
  n->moved = fresh || moved;
  switch (k) {
    case RK_UI_LABEL:
      if (CHANGED_S(text) && !fresh) { ui_transition(n, d, RK_UI_IN_PLACE, fresh); rk_label_set_text(v, d->text); layout = true; }
      if (CHANGED(style) && !fresh) { rk_label_set_style(v, d->style); layout = true; }
      if (CHANGED(color) && (d->color || !fresh)) rk_label_set_color(v, d->color);
      break;
    case RK_UI_BUTTON:
      if (CHANGED_S(text) && !fresh) { rk_button_set_label(v, d->text); layout = true; }
      if (CHANGED(button) && (d->button || !fresh)) rk_button_set_kind(v, d->button);
      if (CHANGED_S(icon) && (d->icon || !fresh)) rk_button_set_icon(v, d->icon);
      break;
    case RK_UI_TILE: {
      bool stat = !fresh && (!ui_same(d->text, o->text) || !ui_same(d->label, o->label));
      bool status = CHANGED(status) || CHANGED_S(meta);
      if (!fresh) {
        if (status && d->status != o->status) rk_view_transition(outer, RK_TRANSITION_FLIP);   /* a new status turns it over, always */
        else if (stat) ui_transition(n, d, RK_UI_PEEK, false);
        else if (pulse) rk_view_transition(outer, d->change == RK_UI_PEEK ? RK_TRANSITION_PEEK : d->change == RK_UI_CROSSFADE ? RK_TRANSITION_CROSSFADE : RK_TRANSITION_FLIP);
      }
      if (status && (d->status || d->meta || !fresh)) rk_tile_set_status(v, d->status, d->meta);
      if (stat) rk_tile_set_stat(v, d->text, d->label);
      if (CHANGED_S(message) && (d->message || !fresh)) rk_tile_set_message(v, d->message);
      float bar = d->busy || d->progress > 0 ? d->progress : -1, was_bar = o->busy || o->progress > 0 ? o->progress : -1;   /* busy: a bar at 0 too */
      if (fresh ? bar >= 0 : bar != was_bar) rk_tile_set_progress(v, bar);
      if ((fresh && d->series && d->nseries > 0) || (!fresh && !ui_same_series(d->series, d->nseries, o->series, o->nseries)))
        rk_tile_set_series(v, d->series, d->series ? d->nseries : 0);
      break;
    }
    case RK_UI_ROW:
      if (!fresh && (!ui_same(d->text, o->text) || !ui_same(d->meta, o->meta) || d->status != o->status)) {
        ui_transition(n, d, RK_UI_IN_PLACE, false);
        rk_row_set(v, d->text, d->meta, d->status);
      } else if (pulse) rk_view_transition(outer, RK_TRANSITION_CROSSFADE);
      break;
    case RK_UI_ICON:
      if (!fresh && (!ui_same(d->icon, o->icon) || d->color != o->color)) rk_icon_set(v, d->icon, d->color);
      break;
    case RK_UI_PROGRESS:
      if (!fresh && (d->progress != o->progress || d->busy != o->busy)) rk_progress_set_value(v, d->busy ? -1 : d->progress);
      break;
    case RK_UI_IMAGE:
      if (CHANGED(fit) && (d->fit || !fresh)) rk_image_set_fit(v, d->fit);
      break;
    case RK_UI_FIELD: {
      if (!fresh && !ui_same(d->text, o->text)) { rk_field_set_label(v, d->text); layout = true; }
      if (!fresh && !ui_same(d->placeholder, o->placeholder)) rk_field_set_placeholder(v, d->placeholder);
      if (CHANGED_S(hint) || CHANGED_S(error)) {
        if (d->error) rk_field_set_error(v, d->error); else if (d->hint || !fresh) rk_field_set_hint(v, d->hint);
        layout = true;
      }
      /* the app's text, when it changed: never over what the user typed since */
      const char *t = ui_text_of(d);                 /* bound: held; given: set when it moves */
      if (t && (d->buffer ? !fresh : !ui_same(t, n->input)) && strcmp(rk_field_text(v), t)) rk_field_set_text(v, t);
      rk__setstr(&n->input, t);
      if (CHANGED(lines) && (d->lines || !fresh)) { rk_field_set_lines(v, d->lines > 0 ? d->lines : 1); layout = true; }
      if (CHANGED(secret) && (d->secret || !fresh)) rk_field_set_secret(v, d->secret);
      if (fresh ? ui_options_count(d) > 0 : !ui_same_options(d->options, ui_options_count(d), o->options, ui_options_count(o)))
        rk_field_set_options(v, d->options, ui_options_count(d));
      break;
    }
    case RK_UI_CHECK: case RK_UI_TOGGLE:
      if (!fresh && !ui_same(d->text, o->text)) { rk_control_set_label(v, d->text); layout = true; }
      break;
    case RK_UI_CHOICES: case RK_UI_SEGMENTS: case RK_UI_SELECT:
      if (!fresh && !ui_same_options(d->options, ui_options_count(d), o->options, ui_options_count(o))) {
        rk_control_set_options(v, d->options, ui_options_count(d));
        set_val = true; layout = true;
      }
      break;
    case RK_UI_SLIDER: case RK_UI_STEPPER:
      if (!fresh && (d->min != o->min || d->max != o->max || d->step != o->step)) { rk_control_set_range(v, d->min, ui_range_max(d), d->step); set_val = true; }
      break;
    case RK_UI_LIST: case RK_UI_TABLE:
      if (fresh || d->count != o->count || !d->version || d->version != o->version) rk_list_set_count(v, RK_MAX(d->count, 0));
      if (fresh ? d->item_key != NULL : (d->item_key != NULL) != (o->item_key != NULL)) rk_list_set_key(v, d->item_key ? ui_list_key : NULL);
      if (set_val) {
        int i = (int)val;
        rk_list_clear_selection(v);
        if (i >= 0 && i < d->count) { rk_list_select(v, i, true); rk_list_scroll_to(v, i); }
        set_val = false;
      }
      break;
    case RK_UI_ROWS:
      if (d->on_move && (fresh || !o->on_move)) rk_rows_set_sortable(v, ui_rows_moved, n);
      break;
    case RK_UI_HOST:
      if (d->update) d->update(v, ui_root_user(n, d));
      break;
    case RK_UI_PIVOT:
      if (CHANGED(side) && (d->side || !fresh)) rk_pivot_set_layout(n->ctl, d->side ? RK_PIVOT_SIDE : RK_PIVOT_TOP);
      if (!fresh && !ui_same(d->text, o->text)) rk_controller_set_title(n->ctl, d->text);
      break;
    case RK_UI_PANORAMA: case RK_UI_PLACES:
      if (!fresh && !ui_same(d->text, o->text)) rk_controller_set_title(n->ctl, d->text);
      break;
    case RK_UI_PAGE: case RK_UI_PLACE: case RK_UI_SHEET:
      if (!fresh && !ui_same(d->text, o->text)) rk_controller_set_title(n->ctl, d->text);   /* a page's title in its strip, live */
      break;
    case RK_UI_SECTION:
      if (!fresh && n->parent && n->num >= 0) {
        if (!ui_same(d->text, o->text)) rk_panorama_set_title(n->parent->ctl, n->num, d->text);
        if (d->width != o->width) rk_panorama_set_width(n->parent->ctl, n->num, d->width);
      }
      break;
    case RK_UI_COLUMN:
      if (!fresh && n->parent && n->num >= 0) {
        if (!ui_same(d->text, o->text) || d->flags != o->flags) rk_table_set_column(n->parent->view, n->num, d->text, d->flags);
        if (d->width != o->width) rk_table_set_column_width(n->parent->view, n->num, d->width);
        if (d->sort != o->sort) rk_table_set_sort(n->parent->view, n->num, d->sort);
      }
      break;
    case RK_UI_ITEM:
      if (!fresh && n->tree && n->num >= 0) {
        if (!ui_same(d->text, o->text)) rk_tree_set_title(n->tree->view, n->num, d->text);
        if (!ui_same(d->meta, o->meta) || d->status != o->status) rk_tree_set_meta(n->tree->view, n->num, d->meta, d->status);
      }
      break;
    default: break;
  }
  if (set_val) {
    if (k == RK_UI_DATE) {
      int x = (int)val, y = 0, m = 0, dd = 0;
      rk_date_value(v, &y, &m, &dd);
      if (x != (y > 0 ? y * 10000 + m * 100 + dd : 0)) rk_date_set_value(v, x / 10000, x / 100 % 100, x % 100);
    } else if (k == RK_UI_PIVOT) { /* in step with the pages, after them */ }
    else if (ui_info(k)->role == R_VIEW && k >= RK_UI_CHECK && k <= RK_UI_SELECT) { if (rk_control_value(v) != val) rk_control_set_value(v, val); }
  }
  if (k != RK_UI_LIST && k != RK_UI_TABLE) n->value = val;  /* as declared; a list keeps the shown item, user's too */
  else if (set_val || fresh) n->value = val;
  /* what any view has */
  if (ui_info(k)->role == R_VIEW && k != RK_UI_NONE) {
    if (CHANGED_S(key) && (d->key || !fresh)) rk_view_set_name(outer, d->key);
    if (CHANGED_S(tooltip) && (d->tooltip || !fresh)) rk_view_set_tooltip(outer, d->tooltip);
    if (CHANGED_S(access) && d->access) rk__setstr(&outer->access_label, d->access);
    if (CHANGED(min_width) || CHANGED(min_height)) { if (d->min_width || d->min_height || !fresh) { rk_view_set_min_size(outer, d->min_width, d->min_height); layout = true; } }
    if (d->focus && (fresh || !o->focus)) r->focus = n;
    if (CHANGED(disabled) && (d->disabled || !fresh)) rk_view_set_enabled(outer, !d->disabled);
    if (CHANGED(fill) && (d->fill || !fresh)) rk_view_set_fill(outer, d->fill);
    if (CHANGED(rules) && (d->rules || !fresh)) rk_view_set_rules(outer, d->rules);
    if (CHANGED(span.columns) || CHANGED(span.rows)) {
      if (d->span.columns > 1 || d->span.rows > 1 || !fresh) { rk_view_set_span(outer, RK_MAX(d->span.columns, 1), RK_MAX(d->span.rows, 1)); layout = true; }
    }
    if (CHANGED(width) || CHANGED(height)) {
      if (d->width || d->height || !fresh) { rk_view_set_size(outer, d->width, d->height); layout = true; }
    }
    if (CHANGED(grow)) { float g = d->grow > 0 ? d->grow : ui_info(k)->grow; if (g || !fresh) { rk_view_set_grow(outer, g); layout = true; } }
    if (CHANGED(on_press) && (d->on_press || !fresh)) rk_view_on_activate(outer, d->on_press ? ui_pressed : NULL, d->on_press ? n : NULL);
  } else if (k == RK_UI_SECTION || k == RK_UI_PAGE || k == RK_UI_PLACE || k == RK_UI_SHEET) {
    if (CHANGED(disabled) && (d->disabled || !fresh)) rk_view_set_enabled(v, !d->disabled);
  }
  if (ui_holds_layout(k) || k == RK_UI_HSTACK || k == RK_UI_ROWS) {
    if (CHANGED(gap) && (d->gap || !fresh)) { rk_view_set_gap(v, d->gap < 0 ? 0 : d->gap > 0 ? d->gap : ui_design_gap(k)); layout = true; }
    if ((fresh && memcmp(&d->padding, &(rk_insets){ 0, 0, 0, 0 }, sizeof d->padding)) || (!fresh && memcmp(&d->padding, &o->padding, sizeof d->padding))) {
      rk_view_set_padding(v, d->padding); layout = true;
    }
    if (CHANGED(align) || CHANGED(cross)) { if (d->align || d->cross || !fresh) { rk_view_set_align(v, d->align, d->cross); layout = true; } }
  }
  if (ui_info(k)->role == R_VIEW && k != RK_UI_NONE && (fresh || d->menu != o->menu || d->menu)) ui_sync_menu(n, d);
  if (layout && !fresh && !n->still) r->reflow = true;   /* it glides to its new size */
  ui_keep(&n->last, d);
}

/* Properties its view takes only when it is made: a new value makes a new view in the old one's place. */
static bool ui_made_with(const ui_node *n, const rk_ui *d) {
  const rk_ui *o = &n->last;
  if (n->stale || o->kind != d->kind) return false;
  switch (d->kind) {
    case RK_UI_IMAGE: return ui_same(o->text, d->text) && o->data == d->data && o->data_size == d->data_size;
    case RK_UI_DROP: return ui_same(o->text, d->text) && ui_same(o->hint, d->hint) && ui_same(o->accept, d->accept);
    case RK_UI_SPLIT: return ui_same(o->text, d->text);
    case RK_UI_HOST: return o->make == d->make;
    default: return true;
  }
}

static rk_view *ui_column_box(void) { rk_view *v = rk_vstack(); return v; }
static rk_view *ui_fill_box(void) { rk_view *v = rk_view_create(); rk_view_set_layout(v, RK_LAYOUT_OVERLAY); return v; }

static rk_controller *ui_owner_here(ui_root *r) { return r->depth ? r->owners[r->depth - 1] : r->host; }

static ui_node *ui_create(ui_root *r, ui_node *parent, const rk_ui *d) {
  ui_node *n = ui_node_new(r, parent, d->kind);
  void *user = ui_root_user(n, d);
  double val = ui_value_of(d);
  rk_view *v = NULL;
  switch (d->kind) {
    case RK_UI_VSTACK: v = rk_vstack(); break;
    case RK_UI_HSTACK: v = rk_hstack(); break;
    case RK_UI_TILES: v = rk_tile_grid(); break;
    case RK_UI_ROWS: v = rk_rows(); break;
    case RK_UI_SCROLL: v = ui_column_box(); n->outer = rk_scroll(rk_retain(v)); break;
    case RK_UI_SPLIT:
      n->panes[0] = ui_fill_box(); n->panes[1] = ui_fill_box();
      v = rk_split(rk_retain(n->panes[0]), rk_retain(n->panes[1]), d->text);
      rk_split_on_back(v, ui_split_heard, n);
      break;
    case RK_UI_LABEL: v = rk_label(d->text, d->style); break;
    case RK_UI_BUTTON: v = rk_button(d->text, NULL, NULL); break;
    case RK_UI_TILE: v = rk_tile(d->text, d->label); break;
    case RK_UI_ROW:                                  /* a row is made in its rows; placed with the others when the build ends */
      v = rk_retain(rk_rows_add(parent->view, d->text, d->meta, d->status));
      rk__view_detach(v);
      break;
    case RK_UI_ICON: v = rk_icon(d->icon, d->color); break;
    case RK_UI_PROGRESS: v = rk_progress(d->busy ? -1 : d->progress); break;
    case RK_UI_IMAGE: v = d->data ? rk_image_from_memory(d->data, d->data_size) : rk_image(d->text); break;
    case RK_UI_SPACER: v = rk_view_create(); break;
    case RK_UI_FIELD:
      v = rk_field(d->text, d->placeholder);
      rk_field_on_change(v, ui_changed, n); rk_field_on_submit(v, ui_submitted, n);
      break;
    case RK_UI_CHECK: v = rk_check(d->text, val != 0); break;
    case RK_UI_TOGGLE: v = rk_toggle(d->text, val != 0); break;
    case RK_UI_CHOICES: v = rk_choices(d->options, ui_options_count(d), (int)val); break;
    case RK_UI_SEGMENTS: v = rk_segments(d->options, ui_options_count(d), (int)val); break;
    case RK_UI_SELECT: v = rk_select(d->options, ui_options_count(d), (int)val); break;
    case RK_UI_SLIDER: v = rk_slider(d->min, ui_range_max(d), d->step, val); break;
    case RK_UI_STEPPER: v = rk_stepper(d->min, ui_range_max(d), d->step, val); break;
    case RK_UI_DATE: { int x = (int)val; v = rk_date(x / 10000, x / 100 % 100, x % 100); break; }
    case RK_UI_DROP: v = rk_drop(d->text, d->hint, d->accept, ui_drop_files, n); break;
    case RK_UI_LIST: v = rk_list(RK_MAX(d->count, 0), ui_list_item, n); break;
    case RK_UI_TABLE: v = rk_table(RK_MAX(d->count, 0), ui_table_cell, n); break;
    case RK_UI_TREE:
      v = rk_tree();
      rk_tree_on_show(v, ui_tree_shown, n); rk_tree_on_toggle(v, ui_tree_toggled, n);
      break;
    case RK_UI_ITEM:
      for (ui_node *p = parent; p; p = p->parent) if (p->last.kind == RK_UI_TREE) { n->tree = p; break; }
      break;
    case RK_UI_PANORAMA: case RK_UI_PIVOT: case RK_UI_PLACES:
      n->ctl = d->kind == RK_UI_PANORAMA ? rk_panorama() : d->kind == RK_UI_PIVOT ? rk_pivot() : rk_places(NULL);
      v = rk_controller_embed(ui_owner_here(r), rk_retain(n->ctl));   /* its view, a reference: the node's */
      if (d->text) rk_controller_set_title(n->ctl, d->text);   /* what the title bar names */
      if (d->kind == RK_UI_PIVOT) rk_pivot_on_change(n->ctl, ui_pivot_heard, n);
      if (d->kind == RK_UI_PLACES) rk_places_on_back(n->ctl, ui_places_heard, n);
      break;
    case RK_UI_SECTION: v = ui_column_box(); break;
    case RK_UI_PAGE: case RK_UI_PLACE: case RK_UI_SHEET:
      v = ui_column_box();
      n->ctl = rk_controller_create_with_view(rk_retain(v), d->text);
      if (d->kind == RK_UI_SHEET) rk_controller_on_dismiss(n->ctl, ui_sheet_heard, n);
      break;
    case RK_UI_CONFIRM: case RK_UI_COLUMN: break;
    case RK_UI_HOST: v = d->make ? d->make(user) : NULL; break;
    default: break;
  }
  if (d->kind >= RK_UI_CHECK && d->kind <= RK_UI_DATE && v) rk_control_on_change(v, ui_changed, n);
  if (!v && ui_info(d->kind)->role == R_VIEW) v = rk_view_create();
  n->view = v;
  if (!n->outer) n->outer = v;
  n->hooked = true;
  if (ui_info(d->kind)->role == R_VIEW) { float g = ui_info(d->kind)->grow; if (g && d->grow <= 0) rk_view_set_grow(n->outer, g); }
  ui_update(n, d, true);
  n->value = val;
  if (d->kind == RK_UI_FIELD && ui_text_of(d)) { rk_field_set_text(v, ui_text_of(d)); rk__setstr(&n->input, ui_text_of(d)); }
  if (d->kind == RK_UI_ITEM) n->opened = false;
  return n;
}

/* Who it is: the node of the last build it continues, or NULL -------------------------------------------------- */

static uint64_t ui_key_hash(const char *key, uint64_t id) {
  uint64_t h = 1469598103934665603ull ^ id * 0x9e3779b97f4a7c15ull;
  if (key) for (const unsigned char *s = (const unsigned char *)key; *s; s++) h = (h ^ *s) * 1099511628211ull;
  return h;
}
static bool ui_keyed(const rk_ui *d) { return d->key || d->id; }
static bool ui_same_one(const ui_node *k, const rk_ui *d) { return k->last.id == d->id && ui_same(k->last.key, d->key); }

static void ui_hash_build(ui_node *p) {
  int cap = 16;
  while (cap < p->nkids * 2) cap *= 2;
  free(p->hash);
  p->hash = rk__alloc((size_t)cap * sizeof *p->hash);
  p->hcap = cap;
  for (int i = 0; i < p->nkids; i++) {
    ui_node *k = p->kids[i];
    if (!ui_keyed(&k->last)) continue;
    for (uint64_t h = ui_key_hash(k->last.key, k->last.id);; h++) if (!p->hash[h & (uint64_t)(cap - 1)]) { p->hash[h & (uint64_t)(cap - 1)] = k; break; }
  }
}

static ui_node *ui_find(ui_node *p, const rk_ui *d) {
  int n = p->nkids;
  if (!ui_keyed(d)) {                                 /* the next unkeyed one of its kind, in order */
    for (int j = p->ufrom; j < n; j++) {
      ui_node *k = p->kids[j];
      if (k->seen || ui_keyed(&k->last) || k->last.kind != d->kind) continue;
      k->seen = true; k->at = j; p->ufrom = j + 1;
      return k;
    }
    return NULL;
  }
  if (p->nnext && ui_keyed(&p->next[p->nnext - 1]->last) && ui_same_one(p->next[p->nnext - 1], d))
    rk__warn("rk_ui: two views are %s%s among the same siblings: each needs its own key", d->key ? d->key : "", d->id ? " (one id)" : "");
  ui_node *k = NULL;
  if (p->from < n && ui_same_one(p->kids[p->from], d)) k = p->kids[p->from];   /* in the order it was: the common case */
  else {
    if (!p->hash) ui_hash_build(p);
    for (uint64_t h = ui_key_hash(d->key, d->id);; h++) {
      ui_node *c = p->hash[h & (uint64_t)(p->hcap - 1)];
      if (!c) break;
      if (ui_same_one(c, d)) { k = c; break; }
    }
  }
  if (!k) return NULL;
  if (k->seen) {
    rk__warn("rk_ui: two views are %s%s among the same siblings: each needs its own key", d->key ? d->key : "", d->id ? " (one id)" : "");
    return NULL;
  }
  int j = p->from < n && p->kids[p->from] == k ? p->from : -1;
  if (j < 0) for (int i = 0; i < n; i++) if (p->kids[i] == k) { j = i; break; }
  k->seen = true; k->at = j; p->from = j + 1;
  return k;
}

/* Building -------------------------------------------------------------------------------------------------------- */

static void ui_push(ui_root *r, ui_node *n, rk_controller *owner) {
  if (r->depth + 1 > r->scap) {
    int cap = RK_MAX(16, r->scap * 2);
    r->stack = rk__realloc(r->stack, (size_t)cap * sizeof *r->stack);
    r->owners = rk__realloc(r->owners, (size_t)cap * sizeof *r->owners);
    r->scap = cap;
  }
  r->stack[r->depth] = n; r->owners[r->depth] = owner;
  r->depth++;
}

static void ui_record(ui_node ***list, int *count, int *cap, ui_node *n) {
  *list = rk__grow(*list, cap, *count + 1, sizeof **list);
  (*list)[(*count)++] = n;
}

/* What a node may hold */
static bool ui_fits(const ui_node *p, rk_ui_kind k) {
  int h = ui_info(p->last.kind)->holds, role = ui_info(k)->role;
  switch (h) {
    case H_VIEWS: return (role == R_VIEW && k != RK_UI_ROW) || role == R_FLOAT;
    case H_TWO: return (role == R_VIEW && k != RK_UI_ROW) || role == R_FLOAT;
    case H_ROWS: return k == RK_UI_ROW;
    case H_COLUMNS: return k == RK_UI_COLUMN;
    case H_ITEMS: return k == RK_UI_ITEM;
    case H_SECTIONS: return k == RK_UI_SECTION || role == R_FLOAT;
    case H_PAGES: return k == RK_UI_PAGE || role == R_FLOAT;
    case H_PLACES: return k == RK_UI_PLACE || role == R_FLOAT;
    default: return false;
  }
}
static const char *ui_what_it_holds(int h) {
  switch (h) {
    case H_VIEWS: case H_TWO: return "views"; case H_ROWS: return "only RK_ROW"; case H_COLUMNS: return "only RK_COLUMN";
    case H_ITEMS: return "only RK_ITEM"; case H_SECTIONS: return "only RK_SECTION"; case H_PAGES: return "only RK_PAGE";
    case H_PLACES: return "only RK_PLACE"; default: return "nothing";
  }
}

/* The owner of what a node declares: the controller things presented or embedded in it belong to */
static rk_controller *ui_owner_of(ui_node *n, rk_controller *outer) {
  switch (n->last.kind) {
    case RK_UI_PANORAMA: return n->ctl;              /* a section's content belongs to its panorama */
    case RK_UI_PAGE: case RK_UI_PLACE: case RK_UI_SHEET: return n->ctl;
    default: return outer;
  }
}

/* Its scope runs: not when its version is the same as the last, nor for a place or a sheet that is closed */
static bool ui_scope_runs(const rk_ui *d, bool fresh, uint32_t was_version) {
  if (!fresh && d->version && d->version == was_version) return false;
  if ((d->kind == RK_UI_PLACE || d->kind == RK_UI_SHEET) && ui_open_of(d) == 0 && !fresh) return false;
  return true;
}

static bool ui_needs_nav(rk_ui_kind k) {
  return k == RK_UI_PANORAMA || k == RK_UI_PIVOT || k == RK_UI_PLACES || k == RK_UI_SHEET || k == RK_UI_CONFIRM || k == RK_UI_SPLIT ||
         k == RK_UI_TABLE || k == RK_UI_TREE;
}

const char *rk_ui_fmt(const char *format, ...) {
  ui_root *r = ui_cur;
  if (!r) { rk__error("rk_ui_fmt: only inside a build"); return ""; }
  va_list ap;
  va_start(ap, format);
  int n = vsnprintf(NULL, 0, format, ap);
  va_end(ap);
  if (n < 0) return "";
  size_t need = r->arena_used + (size_t)n + 1;
  if (need > r->arena_cap) {                       /* a new block: what was given out before stays where it is */
    size_t cap = RK_MAX(need - r->arena_used, (size_t)4096);
    char *block = rk__alloc(cap + sizeof(char *));
    memcpy(block, &r->arena, sizeof(char *));      /* chained, freed when the build ends */
    r->arena = block; r->arena_used = sizeof(char *); r->arena_cap = cap + sizeof(char *);
  }
  char *out = r->arena + r->arena_used;
  va_start(ap, format);
  vsnprintf(out, (size_t)n + 1, format, ap);
  va_end(ap);
  r->arena_used += (size_t)n + 1;
  return out;
}

static void ui_arena_free(ui_root *r) {
  while (r->arena) { char *next; memcpy(&next, r->arena, sizeof next); free(r->arena); r->arena = next; }
  r->arena_used = r->arena_cap = 0;
}

bool rk_ui_open(const rk_ui *in) {
  ui_root *r = ui_cur;
  if (!r) { rk__error("rk_ui_open: only inside a build (rk_window_desc.build or rk_ui_view)"); return false; }
  if (!RK__SIZED(in, "rk_ui_open", "rk_ui")) return false;
  rk_ui d;
  memset(&d, 0, sizeof d);
  memcpy(&d, in, RK_MIN((size_t)in->size, sizeof d));
  if (d.kind <= RK_UI_NONE || d.kind > LAST_KIND) { rk__error("rk_ui_open: no kind of view is %d", (int)d.kind); return false; }
  ui_check_fields(&d, r->collecting);
  if (r->collecting) {                             /* a menu: its entries */
    if (d.kind != RK_UI_ITEM && d.kind != RK_UI_SEPARATOR && d.kind != RK_UI_HEADING) {
      rk__error("rk_ui_open: a menu holds RK_ITEM, RK_SEPARATOR and RK_HEADING (%s in it)", ui_info(d.kind)->name);
      return false;
    }
    r->collect = rk__grow(r->collect, &r->ccap, r->ncollect + 1, sizeof *r->collect);
    memset(&r->collect[r->ncollect], 0, sizeof *r->collect);
    ui_keep(&r->collect[r->ncollect++], &d);
    return false;
  }
  ui_node *p = r->stack[r->depth - 1];
  if (!ui_fits(p, d.kind)) {
    rk__error("rk_ui_open: %s holds %s (%s in it)", ui_info(p->last.kind)->name, ui_what_it_holds(ui_info(p->last.kind)->holds), ui_info(d.kind)->name);
    return false;
  }
  bool needs_owner = ui_is_controller(d.kind) || ui_info(d.kind)->role == R_FLOAT;
  if (needs_owner && !ui_owner_here(r)) {
    rk__error("rk_ui_open: %s goes in a window's build (rk_window_desc.build), not in rk_ui_view", ui_info(d.kind)->name);
    return false;
  }
  if (p->last.kind == RK_UI_SPLIT && ui_info(d.kind)->role == R_VIEW && p->nnext >= 2) {
    rk__error("rk_ui_open: RK_SPLIT holds two views, the list and its detail");
    return false;
  }
  ui_node *old = ui_find(p, &d), *n;
  bool fresh = false, still = p->still || (d.change == RK_UI_IN_PLACE && ui_info(d.kind)->holds != H_NONE);
  uint32_t was_version = old ? old->last.version : 0;
  if (old && ui_made_with(old, &d)) { n = old; n->still = still; ui_update(n, &d, false); }
  else {
    n = ui_create(r, p, &d); fresh = true; n->still = still;
    if (old) { n->replaces = old; n->at = old->at; ui_record(&r->swaps, &r->nswaps, &r->swcap, n); }
  }
  p->next = rk__grow(p->next, &p->ncap, p->nnext + 1, sizeof *p->next);
  p->next[p->nnext++] = n;
  if (ui_needs_nav(d.kind)) ui_record(&r->navs, &r->nnavs, &r->nvcap, n);
  if (ui_info(d.kind)->holds == H_NONE || !ui_scope_runs(&d, fresh, was_version)) return false;
  ui_push(r, n, ui_owner_of(n, ui_owner_here(r)));
  return true;
}

/* A node's scope ends: its kids are known. What the last build had and this one has not goes when the build ends;
   new and moved views are placed then too - or at once, in a view made in this build. */
static void ui_finish(ui_root *r, ui_node *n) {
  bool changed_order = n->nnext != n->nkids;
  for (int i = 0; i < n->nkids; i++) if (!n->kids[i]->seen) ui_record(&r->gone, &r->ngone, &r->gcap, n->kids[i]);
  for (int i = 0; i < n->nnext && !changed_order; i++) if (n->next[i]->at != i) changed_order = true;
  int h = ui_info(n->last.kind)->holds;
  /* a tree adds items at the end of their parent: an item new before old ones, or old ones in another order, and the
     parent's items are put in order once added */
  if (h == H_ITEMS) {
    int last_num = -1; bool new_seen = false;
    for (int i = 0; i < n->nnext; i++) {
      ui_node *k = n->next[i];
      if (k->last.kind != RK_UI_ITEM) continue;
      if (k->num < 0) { new_seen = true; continue; }
      if (new_seen || k->num < last_num) n->reorder = true;
      last_num = k->num;
    }
  }
  if (n->fresh) {
    if (h == H_VIEWS || h == H_ROWS) {
      for (int i = 0; i < n->nnext; i++) {
        ui_node *k = n->next[i];
        if (ui_info(k->last.kind)->role == R_VIEW && rk_view_parent(k->outer) != n->view) rk__view_insert(n->view, k->outer, rk_view_count(n->view));
      }
    } else if (h == H_TWO) {
      int j = 0;
      for (int i = 0; i < n->nnext && j < 2; i++) if (ui_info(n->next[i]->last.kind)->role == R_VIEW) rk__view_insert(n->panes[j++], n->next[i]->outer, 0);
    }
  } else if (changed_order && (h == H_VIEWS || h == H_ROWS || h == H_TWO)) ui_record(&r->holders, &r->nholders, &r->hdcap, n);
  if ((n->last.kind == RK_UI_PAGE || n->last.kind == RK_UI_PLACE) && n->ctl) {   /* one controller in it: it stands for it */
    int views = 0; ui_node *only = NULL;
    for (int i = 0; i < n->nnext; i++) if (ui_info(n->next[i]->last.kind)->role == R_VIEW) { views++; only = n->next[i]; }
    rk__controller_set_transparent(n->ctl, views == 1 && ui_is_controller(only->last.kind));
  }
  ui_node **t = n->kids; int tc = n->kcap;
  n->kids = n->next; n->nkids = n->nnext; n->kcap = n->ncap;
  n->next = t; n->ncap = tc; n->nnext = 0;
  for (int i = 0; i < n->nkids; i++) n->kids[i]->seen = false;
  free(n->hash); n->hash = NULL; n->hcap = 0;
  n->fresh = false;
  n->from = n->ufrom = 0;
}

void rk_ui_close(void) {
  ui_root *r = ui_cur;
  if (!r) { rk__error("rk_ui_close: only inside a build"); return; }
  if (r->depth <= 1) { rk__warn("rk_ui_close: more closes than opens"); return; }
  ui_finish(r, r->stack[--r->depth]);
}

void rk_ui_put(const rk_ui *ui) {
  if (!ui_cur) { rk__error("rk_ui_put: only inside a build (rk_window_desc.build or rk_ui_view)"); return; }
  if (rk_ui_open(ui)) rk_ui_close();
}

/* After the build: views ------------------------------------------------------------------------------------------- */

static int ui_index_in(rk_view *parent, rk_view *v) {
  for (int i = 0; i < parent->nkids; i++) if (parent->kids[i] == v) return i;
  return -1;
}

/* The views a node holds stand in its order in their holder; the views leaving keep their places. True when a view
   was added. */
static bool ui_arrange_in(rk_view *h, rk_view **want, int n) {
  bool added = false;
  for (int i = 0; i < n; i++) if (want[i]->parent != h) { rk__view_insert(h, want[i], h->nkids); added = true; }
  rk_view_reorder(h, want, n);                     /* what leaves, or is not the node's, keeps its slot */
  return added;
}

static void ui_arrange(ui_node *n) {
  if (n->last.kind == RK_UI_SPLIT) {
    int j = 0;
    for (int i = 0; i < n->nkids && j < 2; i++) {
      ui_node *k = n->kids[i];
      if (ui_info(k->last.kind)->role != R_VIEW) continue;
      rk_view *pane = n->panes[j++];
      if (k->outer->parent != pane) {
        for (int c = pane->nkids - 1; c >= 0; c--) if (!pane->kids[c]->leaving) rk_view_remove(pane->kids[c]);
        rk__view_insert(pane, k->outer, pane->nkids);
      }
    }
    return;
  }
  rk_view *buf[64] = { 0 }, **want = n->nkids <= 64 ? buf : rk__alloc((size_t)n->nkids * sizeof *want);
  int m = 0;
  for (int i = 0; i < n->nkids; i++) if (ui_info(n->kids[i]->last.kind)->role == R_VIEW) want[m++] = n->kids[i]->outer;
  ui_arrange_in(n->view, want, m);
  if (want != buf) free(want);
}

static bool ui_any_new(ui_node *n) {
  for (int i = 0; i < n->nkids; i++) {
    ui_node *k = n->kids[i];
    if (ui_info(k->last.kind)->role != R_VIEW) continue;
    rk_view *h = n->last.kind == RK_UI_SPLIT ? NULL : n->view;
    if (h ? k->outer->parent != h : !k->outer->parent) return true;
  }
  return false;
}

/* After the build: navigation --------------------------------------------------------------------------------------- */

/* Closed by the user, declared open again: it comes back, as declared - and that is said, once. */
static void ui_came_back(ui_node *n) {
  if (!n->closed_by_user) return;
  n->closed_by_user = false;
  if (n->warned) return;
  n->warned = true;
  rk__warn("%s \"%s\" was closed by the user and is declared open again: bind .open, or stop declaring it in .on_close",
           ui_info(n->last.kind)->name, n->last.text ? n->last.text : "");
}

/* Who shows a sheet: the page, place or sheet it is declared in, else the window's host */
static rk_controller *ui_presenter_for(ui_root *r, ui_node *n) {
  for (ui_node *p = n->parent; p; p = p->parent)
    if (p->last.kind == RK_UI_PAGE || p->last.kind == RK_UI_PLACE || p->last.kind == RK_UI_SHEET) return p->ctl;
  return r->host;
}

/* A controller's parts, in place: what went is out (ui_take_out), what came goes in where declared, what moved moves. */
static void ui_slot_out(ui_node *n, int j) {
  switch (n->last.kind) {
    case RK_UI_PANORAMA: rk_panorama_remove(n->ctl, j); break;
    case RK_UI_PIVOT: rk_pivot_remove(n->ctl, j); break;
    case RK_UI_TABLE: rk_table_remove_column(n->view, j); break;
    default: break;
  }
  memmove(n->slots + j, n->slots + j + 1, (size_t)(n->nslots - j - 1) * sizeof *n->slots);
  n->nslots--;
  for (int i = j; i < n->nslots; i++) n->slots[i]->num = i;
}

static void ui_slot_in(ui_node *n, int i, ui_node *s) {
  switch (n->last.kind) {
    case RK_UI_PANORAMA: rk_panorama_insert(n->ctl, i, s->last.text, rk_retain(s->view), s->last.width); break;
    case RK_UI_PIVOT: rk_pivot_insert_controller(n->ctl, i, rk_retain(s->ctl)); break;
    case RK_UI_TABLE:
      rk_table_insert_column(n->view, i, s->last.text, s->last.width, s->last.flags);
      if (s->last.sort) rk_table_set_sort(n->view, i, s->last.sort);
      break;
    default: break;
  }
  n->slots = rk__grow(n->slots, &n->slcap, n->nslots + 1, sizeof *n->slots);
  memmove(n->slots + i + 1, n->slots + i, (size_t)(n->nslots - i) * sizeof *n->slots);
  n->slots[i] = s;
  n->nslots++;
  for (int k = i; k < n->nslots; k++) n->slots[k]->num = k;
}

static int ui_sync_slots(ui_node *n, rk_ui_kind kind) {
  int at = 0;
  for (int i = 0; i < n->nkids; i++) {
    ui_node *s = n->kids[i];
    if (s->last.kind != kind) continue;
    if (at < n->nslots && n->slots[at] == s) { at++; continue; }
    if (s->num >= 0 && s->num < n->nslots && n->slots[s->num] == s) ui_slot_out(n, s->num);   /* moved: out, then in */
    ui_slot_in(n, at++, s);
  }
  return at;
}

static void ui_sync_nav(ui_root *r, ui_node *n) {
  rk_ui *d = &n->last;
  switch (d->kind) {
    case RK_UI_PANORAMA: ui_sync_slots(n, RK_UI_SECTION); break;
    case RK_UI_PIVOT: {
      rk_controller *shown = rk_pivot_selected(n->ctl) >= 0 && rk_pivot_selected(n->ctl) < n->nslots ? n->slots[rk_pivot_selected(n->ctl)]->ctl : NULL;
      int slot = ui_sync_slots(n, RK_UI_PAGE);
      if (shown && !d->chosen && !n->moved)        /* the page in sight stays in sight, wherever it went */
        for (int i = 0; i < slot; i++) if (n->slots[i]->ctl == shown && i != rk_pivot_selected(n->ctl)) rk_pivot_select(n->ctl, i);
      int want = d->chosen ? *d->chosen : n->moved ? (int)d->value : -1;   /* bound: held; given: when it moves */
      if (want >= 0 && want < slot && want != rk_pivot_selected(n->ctl)) rk_pivot_select(n->ctl, want);
      break;
    }
    case RK_UI_PLACES: {
      ui_node *want[64]; int m = 0;
      for (int i = 0; i < n->nkids && m < 64; i++) {
        ui_node *p = n->kids[i];
        if (p->last.kind == RK_UI_PLACE && ui_open_of(&p->last) != 0) want[m++] = p;
      }
      int k = 0;
      while (k < m && k < n->npushed && n->pushed[k] == want[k]) k++;
      if (k == 0 && n->npushed) { n->stale = true; r->pending = true; break; }   /* another first place: places anew */
      while (n->npushed > k) { n->pushed[--n->npushed]->opened = false; rk_places_back(n->ctl); }
      for (int i = k; i < m; i++) {
        ui_came_back(want[i]);
        rk_places_go(n->ctl, rk_retain(want[i]->ctl));
        want[i]->opened = true;
        ui_record(&n->pushed, &n->npushed, &n->pcap, want[i]);
      }
      if (!m) rk__warn("RK_PLACES: no place is open; the first one always is");
      break;
    }
    case RK_UI_SHEET: {
      int open = ui_open_of(d);
      if (open != 1) n->closed_by_user = false;
      rk_controller *owner = ui_presenter_for(r, n);
      if (open == 1 && !rk__controller_presented(n->ctl)) {
        ui_came_back(n);
        if (owner && rk_controller_window(owner)) { rk_controller_present(owner, rk_retain(n->ctl)); n->presented = rk__controller_presented(n->ctl); }
      } else if (open == 0 && rk__controller_presented(n->ctl)) { n->presented = false; rk_controller_dismiss(n->ctl); }
      break;
    }
    case RK_UI_CONFIRM: {
      int open = ui_open_of(d);
      if (open != 1) n->closed_by_user = false;
      rk_controller *owner = ui_presenter_for(r, n);
      if (open == 1 && !n->asked && owner && rk_controller_window(owner)) {
        ui_came_back(n);
        rk_confirm_desc c = { sizeof c, d->text, d->message, d->label, d->ok, d->cancel, d->cancel_is_safe };
        rk_controller *s = rk_confirm(owner, &c, ui_confirm_answered, n);
        n->asked = s ? rk_retain(s) : NULL;
      } else if (open == 0 && n->asked) {
        rk_controller *s = n->asked;
        n->asked = NULL; n->quiet = true;
        rk_controller_dismiss(s);
        rk_release(s);
        n->quiet = false;
      }
      break;
    }
    case RK_UI_SPLIT: {
      int open = ui_open_of(d);
      if (open >= 0 && (bool)open != rk_split_is_detail_shown(n->outer)) rk_split_set_detail_shown(n->outer, open);
      n->opened = rk_split_is_detail_shown(n->outer);
      break;
    }
    case RK_UI_TABLE: {
      ui_sync_slots(n, RK_UI_COLUMN);
      rk_table_on_sort(n->view, ui_table_sorted, n);
      rk_table_on_edit(n->view, d->on_edit ? ui_table_edited : NULL, n);
      rk_list_on_show(n->view, ui_list_shown, n);
      rk_list_on_change(n->view, ui_list_changed, n);
      break;
    }
    case RK_UI_TREE: break;                        /* in tree_sync, below */
    default: break;
  }
}

static void ui_tree_add(ui_node *tree, ui_node *holder, int parent_num) {
  for (int i = 0; i < holder->nkids; i++) {
    ui_node *it = holder->kids[i];
    if (it->last.kind != RK_UI_ITEM) continue;
    if (it->num < 0) {
      it->num = rk_tree_add(tree->view, parent_num, it->last.text);
      if (it->num >= 0) {
        if (it->num >= tree->icap) {
          int cap = RK_MAX(16, tree->icap);
          while (cap <= it->num) cap *= 2;
          tree->items = rk__realloc(tree->items, (size_t)cap * sizeof *tree->items);
          memset(tree->items + tree->icap, 0, (size_t)(cap - tree->icap) * sizeof *tree->items);
          tree->icap = cap;
        }
        tree->items[it->num] = it;
        if (it->last.meta || it->last.status) rk_tree_set_meta(tree->view, it->num, it->last.meta, it->last.status);
      }
    }
    if (it->num >= 0) {
      int *chosen = ui_item_chosen(it);
      if (chosen && *chosen == (int)it->last.value && rk_tree_selected(tree->view) != it->num) rk_tree_select(tree->view, it->num);
      int open = ui_open_of(&it->last);
      if (open >= 0 && (bool)open != rk_tree_is_open(tree->view, it->num)) rk_tree_set_open(tree->view, it->num, open);
      it->opened = rk_tree_is_open(tree->view, it->num);
      ui_tree_add(tree, it, it->num);
    }
  }
  if (holder->reorder) {                           /* each to the end, as declared: they stand in that order */
    holder->reorder = false;
    for (int i = 0; i < holder->nkids; i++)
      if (holder->kids[i]->last.kind == RK_UI_ITEM && holder->kids[i]->num >= 0) rk_tree_move(tree->view, holder->kids[i]->num, -1);
  }
}

/* A node no longer declared: it calls nothing, and its view leaves its parent (with motion inside a LEAVE block). */
static void ui_take_out(ui_node *g) {
  ui_unhook(g);
  if (g->last.kind == RK_UI_ITEM && g->tree && g->num >= 0 && !g->tree->stale) rk_tree_remove(g->tree->view, g->num);
  ui_node *p = g->parent;
  if (p && g->num >= 0 && g->num < p->nslots && p->slots[g->num] == g) ui_slot_out(p, g->num);
  if (ui_info(g->last.kind)->role == R_VIEW && g->outer && g->outer->parent) rk_view_remove(g->outer);
  if (g->last.kind == RK_UI_PLACE && g->parent) {          /* a place no longer declared: out of the stack */
    ui_node *pl = g->parent;
    for (int p = 0; p < pl->npushed; p++) if (pl->pushed[p] == g) {
      while (pl->npushed > p) { pl->pushed[--pl->npushed]->opened = false; rk_places_back(pl->ctl); }
      break;
    }
  }
}

static void ui_run(ui_root *r) {
  /* a view made again for what its kind takes only when made: in the old one's place, at once */
  for (int i = 0; i < r->nswaps; i++) {
    ui_node *n = r->swaps[i], *old = n->replaces;
    n->replaces = NULL;
    if (!old) continue;
    rk_view *ov = old->outer;
    rk_view *h = ov ? ov->parent : NULL;
    rk_window *w = ov ? rk_view_window(ov) : NULL;
    bool focus = w && rk_window_focused(w) && rk__view_within(rk_window_focused(w), ov);
    ui_unhook(old);
    if (h && ui_info(n->last.kind)->role == R_VIEW) {
      int at = ui_index_in(h, ov);
      rk__view_insert(h, n->outer, at < 0 ? h->nkids : at);
      rk_view_remove(ov);
    } else if (ov && ov->parent) rk_view_remove(ov);
    if (old->last.kind == RK_UI_PLACES || old->last.kind == RK_UI_PIVOT || old->last.kind == RK_UI_PANORAMA) {
      for (int p = 0; p < r->nholders; p++) if (r->holders[p] == old) r->holders[p] = NULL;
    }
    ui_node_free(old);
    if (focus) { rk_view *f = rk__first_focusable(n->outer); if (f) rk_window_focus(w, f); }
  }
  r->nswaps = 0;
  /* what goes leaves first; its nodes live until it is gone, so a fading list still draws. In a scope declared
     RK_UI_IN_PLACE nothing moves: it goes, comes and stands at once. */
  int moving = 0, nstill = 0;
  ui_node **still = r->ngone ? rk__alloc((size_t)r->ngone * sizeof *still) : NULL;   /* kept apart for after the motion */
  for (int i = 0; i < r->ngone; i++) {
    ui_node *g = r->gone[i];
    if (g->parent && g->parent->still) still[nstill++] = g; else r->gone[moving++] = g;
  }
  if (moving) {
    ui_node **batch = rk__alloc((size_t)(moving + 1) * sizeof *batch);
    memcpy(batch, r->gone, (size_t)moving * sizeof *batch);
    batch[moving] = NULL;
    rk_animate_begin(RK_MOTION_LEAVE);
    for (int i = 0; i < moving; i++) ui_take_out(r->gone[i]);
    rk_animate_on_done(ui_free_batch, batch);
    rk_animate_commit();
  }
  r->ngone = 0;
  /* then the new build comes in and the rest make room */
  bool entering = false, any = false, any_still = nstill > 0;
  for (int i = 0; i < r->nholders; i++) if (r->holders[i]) {
    if (r->holders[i]->still) { any_still = true; continue; }
    any = true; entering |= ui_any_new(r->holders[i]);
  }
  if (any || r->reflow) {
    rk_animate_begin(entering ? RK_MOTION_ENTER : RK_MOTION_REFLOW);
    for (int i = 0; i < r->nholders; i++) if (r->holders[i] && !r->holders[i]->still) ui_arrange(r->holders[i]);
    rk_animate_commit();
  }
  if (any_still) {                                 /* outside any motion: at once */
    for (int i = 0; i < nstill; i++) { ui_take_out(still[i]); ui_node_free(still[i]); }
    for (int i = 0; i < r->nholders; i++) if (r->holders[i] && r->holders[i]->still) ui_arrange(r->holders[i]);
    rk__layout_pending();
  }
  free(still);
  r->nholders = 0;
  r->reflow = false;
  /* then navigation: places turn, pages slide, sheets come down */
  for (int i = 0; i < r->nnavs; i++) {
    ui_node *n = r->navs[i];
    if (n->last.kind == RK_UI_TREE) { if (!n->stale) ui_tree_add(n, n, -1); }
    else ui_sync_nav(r, n);
  }
  r->nnavs = 0;
  /* then focus, where the build asked for it, once its view stands where it goes */
  if (r->focus) {
    ui_node *f = r->focus;
    r->focus = NULL;
    rk_window *w = f->outer ? rk_view_window(f->outer) : NULL;
    rk_view *t = w ? rk__first_focusable(f->outer) : NULL;
    if (t) { w->focus_visible = true; rk_window_focus(w, t); }
  }
}

static void ui_build_once(ui_root *r) {
  ui_root *outer = ui_cur;
  ui_cur = r;
  r->building = true;
  r->depth = 0;
  ui_push(r, &r->top, r->host);
  r->build(r->user);
  if (r->depth > 1) {
    rk__warn("a build left %d scopes open (a return or break inside one?): closed at its end", r->depth - 1);
    while (r->depth > 1) rk_ui_close();
  }
  ui_finish(r, &r->top);
  r->depth = 0;
  r->building = false;
  ui_arena_free(r);
  ui_cur = outer;
  ui_run(r);
  /* a window's host stands for the one controller it holds, so a pivot at the top still picks the place; plain views
     at the top are set in from the window's edges, as a place's are */
  if (r->host) {
    int views = 0; ui_node *only = NULL;
    for (int i = 0; i < r->top.nkids; i++) if (ui_info(r->top.kids[i]->last.kind)->role == R_VIEW) { views++; only = r->top.kids[i]; }
    bool stands = views == 1 && ui_is_controller(only->last.kind);
    rk__controller_set_transparent(r->host, stands);
    bool inset = !(views == 1 && (ui_is_controller(only->last.kind) || only->last.kind == RK_UI_SPLIT));
    if (inset != r->inset) { r->inset = inset; rk__view_relayout_up(r->top.view); }
  }
}

static void ui_rebuild(ui_root *r) {
  if (!r->build) return;
  if (r->building || ui_calling) { ui_dirty = true; return; }   /* never inside a build or the app's handler: after it */
  rk_view *hold = rk_retain(r->top.view);
  int pass = 0;
  do { r->pending = false; ui_build_once(r); } while (r->pending && ++pass < 8);
  if (r->pending) { r->pending = false; rk__warn("a build asks for another build each time (8 in a row): stopped"); }
  rk_release(hold);
}

/* The declared view ------------------------------------------------------------------------------------------------ */

static void ui_root_destroy(void *state) {
  ui_root *r = state;
  for (ui_root **p = &ui_roots; *p; p = &(*p)->next_root) if (*p == r) { *p = r->next_root; break; }
  for (int i = 0; i < r->top.nkids; i++) { ui_unhook(r->top.kids[i]); ui_node_free(r->top.kids[i]); }
  free(r->top.kids); free(r->top.next); free(r->top.hash);
  free(r->stack); free(r->owners); free(r->gone); free(r->swaps); free(r->holders); free(r->navs);
  for (int i = 0; i < r->ncollect; i++) ui_drop_last(&r->collect[i]);
  free(r->collect);
  ui_drop_last(&r->top.last);
  free(r);
}

/* What the build declares stands top to bottom, as in a column; a window's plain views are set in from its edges. */
static void ui_root_layout(rk_view *v, void *st) {
  ui_root *r = st;
  rk_insets in = { 0, 0, 0, 0 };
  if (r->host && r->inset) {
    rk_window *w = rk__view_window(v);
    bool compact = w && (w->size.w < 760 || w->size.h < 481);
    float side = rk_metric_of(compact ? RK_SPACE_M : RK_SPACE_XL), l = rk_metric_of(RK_SPACE_L);
    in = (rk_insets){ l, side, l, side };
  }
  if (memcmp(&in, &v->padding, sizeof in)) v->padding = in;
  rk__stack(v, true);
}
static const rk_class ui_root_class = { { sizeof(rk_view_class), "rk_ui", NULL, NULL, ui_root_layout, NULL, ui_root_destroy }, NULL, NULL };

static ui_root *ui_root_new(void (*build)(void *), void *user) {
  ui_root *r = rk__alloc(sizeof *r);
  r->build = build; r->user = user;
  r->top.root = r; r->top.fresh = true; r->top.at = r->top.num = -1;
  rk_view *v = rk__view_create(&ui_root_class, r);
  v->layout = RK_LAYOUT_VSTACK;
  v->gap = rk_metric_of(RK_SPACE_M);
  r->top.view = r->top.outer = v;
  r->next_root = ui_roots; ui_roots = r;
  return r;
}

rk_view *rk_ui_view(void (*build)(void *user), void *user) {
  ui_root *r = ui_root_new(build, user);
  if (!build) rk__error("rk_ui_view: no build function: the view stays empty");
  else ui_rebuild(r);
  return r->top.view;
}

/* A window's host: the declared view, and the keys no view took */
static rk_view *ui_host_load(rk_controller *c, void *state) { (void)c; return ((ui_root *)state)->top.view; }
static bool ui_host_event(rk_controller *c, const rk_event *e, void *state) {
  (void)c;
  ui_root *r = state;
  bool taken = false;
  if (e->type == RK_EVENT_KEY_DOWN && r->on_key) UI_APP(taken = r->on_key(r->window, e->key, e->mods, r->user));
  return taken;
}

void rk__ui_window(rk_window *w, const rk_window_desc *desc) {
  ui_root *r = ui_root_new(desc->build, desc->user);
  r->window = w; r->on_key = desc->on_key;
  rk_controller_class cls = { sizeof cls, "rk_ui_window", ui_host_load, NULL, NULL, ui_host_event, NULL };
  r->host = rk_controller_create(&cls, r);
  rk_controller_view(r->host);                     /* takes the view */
  ui_rebuild(r);
  rk_window_show(w, r->host);                      /* takes the host */
  ui_rebuild(r);                                   /* in its window now: what needs one (a sheet) can come */
}

void rk_ui_update(rk_view *view) {
  if (view) {
    if (!rk__is_object(view, &rk__view_type) || view->cls != &ui_root_class) { rk__error("rk_ui_update: not a declared view (rk_ui_view)"); return; }
    ui_rebuild(view->state);
    return;
  }
  ui_dirty = true;
  rk__ui_settle();
}

/* Something the library called may have changed the app's state: build at the next settle. */
void rk__ui_dirty(void) { ui_dirty = true; }

/* At the end of an event, a timer, a post, a frame: every declared view builds once if the state may have changed -
   outside any handler and any controller's own work, so a build never runs in the middle of either. */
void rk__ui_settle(void) {
  static bool settling;
  if (!ui_dirty || !ui_roots || settling || ui_calling) return;
  settling = true;
  for (int round = 0; ui_dirty && round < 3; round++) {   /* a host's update may change the state again: a few rounds */
    ui_dirty = false;
    int n = 0, cap = 0;
    rk_view **all = NULL;
    for (ui_root *r = ui_roots; r; r = r->next_root) { all = rk__grow(all, &cap, n + 1, sizeof *all); all[n++] = rk_retain(r->top.view); }
    for (int i = 0; i < n; i++) ui_rebuild(all[i]->state);
    for (int i = 0; i < n; i++) rk_release(all[i]);
    free(all);
  }
  settling = false;
}

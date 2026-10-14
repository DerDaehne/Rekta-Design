/* rekta_ui.h: declared views - an optional layer over rekta.h, in C99.
 *
 * A declared view is a function of the app's state. Its build function says what it shows now, top to bottom, with
 * the macros below; the library keeps the views it made the last time and changes only what differs, with the
 * design's motion: a tile's new value peeks in, a new status flips it, a view that comes enters, one that goes
 * leaves and the rest close up, a new span or order glides. Nothing to keep, nothing to free.
 *
 *     static void today(void *user) {
 *       char builds[16];
 *       snprintf(builds, sizeof builds, "%d", finished);
 *       RK_VSTACK() {
 *         RK_LABEL("Today", .style = RK_TEXT_GROUP);
 *         RK_TILES() RK_TILE(builds, .label = "finished today", .status = RK_STATUS_SUCCESS);
 *         RK_BUTTON("build one", .on_press = build_one);    // build_one only counts: the page follows
 *       }
 *     }
 *     rk_panorama_add(home, "today", rk_ui_view(today, NULL), 0);
 *
 * The rules:
 *   - After a handler declared here (on_press, on_change, on_submit) the view is built again. Anything else that
 *     changes the state - a timer, a menu, a list's callback, another thread through rk_app_post - calls
 *     rk_ui_update().
 *   - Who a view is: its key (or id) among its siblings, or else its place among the unkeyed siblings of its kind.
 *     Give a key to every view that comes and goes in a list.
 *   - The library pushes what the description changed, never what the user changed: a field's text and a control's
 *     value are set when .input or .value differ from the last build (and from what the view holds).
 *   - Strings and arrays are copied or compared when a macro runs: a local buffer is fine.
 *   - A scope ({ ... } after a container) is left at its end: no return, break or goto out of it.
 *   - rk_ui is a size-prefixed descriptor (the macros set .size); fields are only ever added at its end.
 *
 * Views that hold their own data stay data-driven: RK_HOST makes any rk_view once (a list of a million rows, a table,
 * a tree, a custom view) and calls update at each build. Controllers (places, pivots, sheets) stay as in rekta.h; a
 * declared view goes wherever a view goes.
 */
#ifndef REKTA_UI_H
#define REKTA_UI_H

#include "rekta.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum rk_ui_kind {
  RK_UI_NONE,
  RK_UI_VSTACK, RK_UI_HSTACK, RK_UI_TILES, RK_UI_ROWS,          /* containers: what they hold is declared in a scope */
  RK_UI_LABEL, RK_UI_BUTTON, RK_UI_TILE, RK_UI_ROW,            /* RK_UI_ROW only inside RK_UI_ROWS */
  RK_UI_FIELD, RK_UI_CHECK, RK_UI_TOGGLE, RK_UI_PROGRESS, RK_UI_ICON,
  RK_UI_HOST,                                                    /* a view the app makes: .make once, .update each build */
  RK_UI_KIND_FORCE32 = 0x7fffffff
} rk_ui_kind;

/* What a change of text shows. AUTO: a tile's new value or label peeks in; other views change in place. A tile's new
   status always flips it. */
typedef enum rk_ui_change { RK_UI_AUTO, RK_UI_IN_PLACE, RK_UI_PEEK, RK_UI_FLIP, RK_UI_CROSSFADE, RK_UI_CHANGE_FORCE32 = 0x7fffffff } rk_ui_change;

typedef struct rk_ui {
  uint32_t size;                                   /* sizeof(rk_ui): the macros set it */
  rk_ui_kind kind;
  const char *key;                                 /* who it is among its siblings, and its name for rk_view_find */
  uint64_t id;                                     /* or a number for who it is (0: none) */
  /* what it says */
  const char *text;                                /* a label's text, a button's, a tile's value, a row's title, a field's,
                                                      a check's or a toggle's label: the macros' first argument */
  const char *label;                               /* a tile's: what the value counts */
  const char *meta;                                /* a row's */
  const char *message;                             /* a tile's */
  const char *icon;                                /* a button's icon; an icon's name */
  const char *placeholder, *hint, *error;          /* a field's; an error takes the hint's place */
  const char *input;                               /* a field's text, set when it changed since the last build */
  const char *name, *tooltip;                      /* name: for rk_view_find, when not the key */
  rk_text_style style;                             /* a label's */
  rk_role color;                                   /* a label's, an icon's */
  rk_button_kind button;
  rk_status status;                                /* a tile's, a row's */
  const float *progress;                           /* a tile's bar or a progress bar, 0..1; NULL: no bar (a progress bar
                                                      without one is indeterminate) */
  const float *series; int nseries;                /* a tile's sparkline */
  double value;                                    /* a check's or a toggle's, 0 or 1: set when it changed since the last build */
  int lines;                                       /* a field's: more than 1 makes it a text area */
  bool secret;                                     /* a field's */
  rk_ui_change change;
  /* layout */
  struct { int columns, rows; } span;              /* in RK_TILES; 0: 1 */
  float width, height;                             /* fixed; 0: natural */
  float grow;
  float gap;                                       /* a container's; 0: the design's */
  rk_insets padding;
  bool disabled;
  /* behaviour: callbacks get .user, or the declared view's user when it is NULL */
  void (*on_press)(rk_view *view, void *user);     /* a button, a tile or a row is activated */
  void (*on_change)(rk_view *view, void *user);    /* a field's text or a control's value changed by the user */
  void (*on_submit)(rk_view *view, void *user);    /* Enter in a field */
  void *user;
  rk_view *(*menu)(void *user);                    /* makes the menu it opens (rk_view_set_menu), once */
  rk_view *(*make)(void *user);                    /* RK_UI_HOST: makes its view (a reference), once */
  void (*update)(rk_view *view, void *user);       /* RK_UI_HOST: at every build, to give it new data */
} rk_ui;

/* A view whose content build declares; build runs now and again at every rk_ui_update. user is borrowed: it lives as
   long as the view. */
RK_API rk_view *rk_ui_view(void (*build)(void *user), void *user);
RK_API void rk_ui_update(rk_view *view);           /* the state changed: build again now. NULL: every declared view */
/* What the macros call, only inside a build: open a view whose content follows until rk_ui_close; put a view whole. */
RK_API void rk_ui_open(const rk_ui *ui);
RK_API void rk_ui_close(void);
RK_API void rk_ui_put(const rk_ui *ui);

/* The macros (C99; from C++ fill an rk_ui and call the functions). The first argument of a view's macro is what it
   says; the rest are fields by name: RK_TILE("12", .label = "builds today", .status = RK_STATUS_SUCCESS). */
#define RK_UI(kind_, ...) (&(const rk_ui){ .size = sizeof(rk_ui), .kind = (kind_), __VA_ARGS__ })
#define RK_UI_SCOPE(node) for (int rk_ui_once_ = (rk_ui_open(node), 1); rk_ui_once_; rk_ui_once_ = (rk_ui_close(), 0))

#define RK_VSTACK(...)   RK_UI_SCOPE(RK_UI(RK_UI_VSTACK, __VA_ARGS__))
#define RK_HSTACK(...)   RK_UI_SCOPE(RK_UI(RK_UI_HSTACK, __VA_ARGS__))
#define RK_TILES(...)    RK_UI_SCOPE(RK_UI(RK_UI_TILES, __VA_ARGS__))
#define RK_ROWS(...)     RK_UI_SCOPE(RK_UI(RK_UI_ROWS, __VA_ARGS__))

#define RK_LABEL(...)    rk_ui_put(RK_UI(RK_UI_LABEL, .text = __VA_ARGS__))
#define RK_BUTTON(...)   rk_ui_put(RK_UI(RK_UI_BUTTON, .text = __VA_ARGS__))
#define RK_TILE(...)     rk_ui_put(RK_UI(RK_UI_TILE, .text = __VA_ARGS__))
#define RK_ROW(...)      rk_ui_put(RK_UI(RK_UI_ROW, .text = __VA_ARGS__))
#define RK_FIELD(...)    rk_ui_put(RK_UI(RK_UI_FIELD, .text = __VA_ARGS__))
#define RK_CHECK(...)    rk_ui_put(RK_UI(RK_UI_CHECK, .text = __VA_ARGS__))
#define RK_TOGGLE(...)   rk_ui_put(RK_UI(RK_UI_TOGGLE, .text = __VA_ARGS__))
#define RK_ICON(...)     rk_ui_put(RK_UI(RK_UI_ICON, .icon = __VA_ARGS__))
#define RK_PROGRESS(...) rk_ui_put(RK_UI(RK_UI_PROGRESS, __VA_ARGS__))
#define RK_HOST(...)     rk_ui_put(RK_UI(RK_UI_HOST, __VA_ARGS__))

#ifdef __cplusplus
}
#endif

#endif /* REKTA_UI_H */

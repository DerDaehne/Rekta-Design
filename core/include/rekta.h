/* rekta.h: Rekta Design core, native views for desktop apps (Windows, macOS, Linux).
 *
 * The ABI rules (docs/core.md):
 *   - C99. Functions rk_noun_verb, types rk_noun, constants RK_NOUN.
 *   - Value types cross by value and never change layout. Units are logical pixels.
 *   - Objects are opaque and reference counted. One rule for who owns what:
 *       - a view or controller constructor returns a reference you own (rk_label(), rk_vstack(), rk_pivot(), ...);
 *       - every call that puts a view or controller somewhere takes that reference over: adding it to a view, a
 *         panorama, a pivot, places, a scroll, a split or a menu slot, showing it in a window, presenting it, giving
 *         it to rk_controller_create_with_view(). What is in the tree belongs to the tree; you never release it;
 *       - a view that is already in a tree moves when you add it elsewhere - nothing to retain;
 *       - a window belongs to the app from rk_window_create() until it closes: the pointer you get is borrowed;
 *       - every other pointer you get is borrowed, valid while what it points to is in the tree or open.
 *     rk_retain() keeps one beyond that; rk_release() lets it go. Both report a pointer that is no Rekta object, or
 *     one already freed, through rk_app_set_log().
 *   - Descriptors that may grow start with `uint32_t size`; set it to sizeof the struct.
 *   - Callbacks are a function pointer plus a `void *user`.
 *   - Strings are UTF-8 and NUL-terminated; the library copies what it keeps.
 *   - Everything runs on the main thread; rk_app_post is the one function another thread may call.
 */
#ifndef REKTA_H
#define REKTA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RK_VERSION_MAJOR 0
#define RK_VERSION_MINOR 3
#define RK_VERSION_PATCH 0
#define RK_VERSION ((RK_VERSION_MAJOR << 16) | (RK_VERSION_MINOR << 8) | RK_VERSION_PATCH)

#if defined(_WIN32) && defined(RK_SHARED)
#  ifdef RK_BUILD
#    define RK_API __declspec(dllexport)
#  else
#    define RK_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__) && defined(RK_SHARED)
#  define RK_API __attribute__((visibility("default")))
#else
#  define RK_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Values ---------------------------------------------------------------------------------------------------------- */

typedef struct rk_point { float x, y; } rk_point;
typedef struct rk_size { float w, h; } rk_size;
typedef struct rk_rect { float x, y, w, h; } rk_rect;
typedef struct rk_insets { float top, right, bottom, left; } rk_insets;   /* the CSS order */
typedef uint32_t rk_color;                                                 /* 0xRRGGBBAA, straight alpha */
#define RK_AUTO 0.0f                               /* a size, a width: the natural one */

/* A view's transform, applied around its origin (0..1 of its size; 0.5, 0.5 is the centre). Rotations in degrees;
   rotate_x and rotate_y turn the view in depth, seen with the given perspective (0: the token's). */
typedef struct rk_transform {
  float translate_x, translate_y;
  float scale_x, scale_y;
  float rotate_x, rotate_y, rotate_z;
  float origin_x, origin_y;
  float perspective;
} rk_transform;

RK_API uint32_t rk_version(void);
RK_API rk_transform rk_transform_identity(void);

/* Objects --------------------------------------------------------------------------------------------------------- */

typedef struct rk_window rk_window;
typedef struct rk_view rk_view;
typedef struct rk_controller rk_controller;
typedef struct rk_canvas rk_canvas;                /* only valid inside a draw callback */

RK_API void *rk_retain(void *object);              /* returns object */
RK_API void rk_release(void *object);              /* NULL is allowed */

/* Tokens: the theme's colours by role, metrics, durations --------------------------------------------------------- */

typedef enum rk_role {
  RK_ROLE_NONE = 0,                                /* no fill */
  RK_ROLE_BG, RK_ROLE_SURFACE, RK_ROLE_SURFACE_2,
  RK_ROLE_TEXT, RK_ROLE_TEXT_LOW, RK_ROLE_TEXT_MEDIUM, RK_ROLE_TEXT_HIGH,
  RK_ROLE_ACCENT, RK_ROLE_ON_ACCENT,
  RK_ROLE_INFO_FULL, RK_ROLE_INFO_SOFT, RK_ROLE_INFO_TILE,
  RK_ROLE_SUCCESS_FULL, RK_ROLE_SUCCESS_SOFT, RK_ROLE_SUCCESS_TILE,
  RK_ROLE_WARNING_FULL, RK_ROLE_WARNING_SOFT, RK_ROLE_WARNING_TILE,
  RK_ROLE_ERROR_FULL, RK_ROLE_ERROR_SOFT, RK_ROLE_ERROR_TILE,
  RK_ROLE_FOCUS, RK_ROLE_HOVER, RK_ROLE_DISABLED, RK_ROLE_SHADING, RK_ROLE_GLASS,
  RK_ROLE_FORCE32 = 0x7fffffff
} rk_role;

typedef enum rk_metric {
  RK_SPACE_XS, RK_SPACE_S, RK_SPACE_M, RK_SPACE_L, RK_SPACE_XL,
  RK_RADIUS_S, RK_RADIUS_M, RK_RADIUS_L, RK_RADIUS_TILE,
  RK_TILE_UNIT, RK_APPBAR_HEIGHT, RK_TOUCH_TARGET, RK_PANE_MIN,
  RK_CONTROL_HEIGHT, RK_ROW_HEIGHT, RK_CONTROL_PADDING,   /* these three follow the density */
  RK_METRIC_FORCE32 = 0x7fffffff
} rk_metric;

typedef enum rk_duration {
  RK_DURATION_PRESS, RK_DURATION_SHORT, RK_DURATION_MODERATE, RK_DURATION_LONG, RK_DURATION_STAGGER,
  RK_DURATION_FLOAT, RK_DURATION_PIVOT_GLIDE, RK_DURATION_PIVOT_SIZE,
  RK_TIMING_TOAST, RK_TIMING_TAP_DELAY, RK_TIMING_LONG_PRESS, RK_TIMING_TIP_DELAY,
  RK_DURATION_FORCE32 = 0x7fffffff
} rk_duration;

RK_API rk_color rk_color_of(rk_role role);         /* in the current theme and accent */
RK_API float rk_metric_of(rk_metric metric);       /* logical px, in the current density */
RK_API double rk_duration_of(rk_duration d);       /* ms; motion durations are 0 under reduced motion, timings stay */

/* App ------------------------------------------------------------------------------------------------------------- */

typedef struct rk_app_desc {
  uint32_t size;                                   /* sizeof(rk_app_desc) */
  const char *name;                                /* shown by the system */
  const char *id;                                  /* reverse DNS, e.g. "de.example.tool" */
  void (*launch)(void *user);                      /* create the first window here */
  void (*terminate)(void *user);                   /* the last window closed or rk_app_quit was called */
  void *user;
  bool headless;                                   /* no screen: a virtual clock and windows in memory (tests) */
} rk_app_desc;

RK_API int rk_app_main(const rk_app_desc *desc);  /* runs until quit; returns the exit code */
RK_API void rk_app_quit(int code);
RK_API void rk_app_post(void (*fn)(void *user), void *user);    /* any thread: run fn on the main thread soon */
RK_API double rk_app_now(void);                    /* ms, monotonic (virtual when headless) */
RK_API uint32_t rk_app_after(double ms, void (*fn)(void *user), void *user);   /* once; an id, never 0 */
RK_API uint32_t rk_app_every(double ms, void (*fn)(void *user), void *user);   /* again and again */
RK_API void rk_app_cancel(uint32_t id);            /* a timer, or a change listener */
RK_API bool rk_app_notify(const char *title, const char *body);   /* a system notification; false where there is none */

typedef enum rk_theme { RK_THEME_SYSTEM, RK_THEME_LIGHT, RK_THEME_DARK, RK_THEME_FORCE32 = 0x7fffffff } rk_theme;
typedef enum rk_motion_pref { RK_MOTION_PREF_SYSTEM, RK_MOTION_PREF_ON, RK_MOTION_PREF_OFF, RK_MOTION_PREF_FORCE32 = 0x7fffffff } rk_motion_pref;
typedef enum rk_density { RK_DENSITY_SYSTEM, RK_DENSITY_COMFORTABLE, RK_DENSITY_COMPACT, RK_DENSITY_FORCE32 = 0x7fffffff } rk_density;

RK_API void rk_app_set_theme(rk_theme theme);
RK_API bool rk_app_is_dark(void);
RK_API void rk_app_set_motion(rk_motion_pref motion);
RK_API bool rk_app_is_reduced_motion(void);
RK_API void rk_app_set_density(rk_density density);
RK_API void rk_app_set_accent(rk_color light, rk_color dark);    /* 0, 0: the ink of the theme */
/* A font file (TrueType or OpenType, or a collection's first face) for the characters Barlow lacks, tried before the
   system's: an app that writes Japanese may bring its own. False when it cannot be read or drawn with. */
RK_API bool rk_app_add_font(const char *path);
RK_API uint32_t rk_app_on_change(void (*fn)(void *user), void *user);   /* theme, motion, density or accent changed; rk_app_cancel ends it */
/* Misuse - a descriptor without its size, a call on the wrong kind of view, an icon that does not exist - is reported
   here, never passed over in silence; stderr until an app sets its own. */
typedef enum rk_log_level { RK_LOG_ERROR, RK_LOG_WARNING, RK_LOG_LEVEL_FORCE32 = 0x7fffffff } rk_log_level;
RK_API void rk_app_set_log(void (*fn)(rk_log_level level, const char *message, void *user), void *user);

/* Window ---------------------------------------------------------------------------------------------------------- */

typedef struct rk_window_desc {
  uint32_t size;                                   /* sizeof(rk_window_desc) */
  const char *title;
  float width, height;                             /* content size; 0: 1280 × 800 */
  float min_width, min_height;                     /* 0: pane-min × 320 */
  bool system_frame;                               /* keep the system's title bar instead of Rekta's */
} rk_window_desc;

RK_API rk_window *rk_window_create(const rk_window_desc *desc);   /* an open window lives until closed */
RK_API void rk_window_show(rk_window *window, rk_controller *root);  /* takes root; again: another root */
RK_API rk_controller *rk_window_root(rk_window *window);
RK_API void rk_window_close(rk_window *window);
RK_API void rk_window_set_title(rk_window *window, const char *title);
RK_API rk_size rk_window_size(rk_window *window);
RK_API void rk_window_set_size(rk_window *window, float width, float height);   /* logical px; not while maximized */
RK_API float rk_window_scale(rk_window *window);   /* device pixels per logical px */
RK_API void rk_window_on_close(rk_window *window, bool (*should_close)(rk_window *window, void *user), void *user);
RK_API bool rk_window_focus(rk_window *window, rk_view *view);  /* false if the view cannot take focus */
RK_API rk_view *rk_window_focused(rk_window *window);
/* The system's file chooser over the window, filtered by accept: extensions (".png,.jpg") or families ("image/" with
   a star); NULL or "" takes any file. chosen() comes later with the paths, or with none if it was cancelled. False
   when the system has none here. */
typedef enum rk_choose { RK_CHOOSE_ONE, RK_CHOOSE_MANY, RK_CHOOSE_FORCE32 = 0x7fffffff } rk_choose;
RK_API bool rk_window_choose_files(rk_window *window, const char *accept, rk_choose choose,
                                   void (*chosen)(rk_window *window, const char *const *files, int count, void *user), void *user);
/* A window's history, as the web's undoStack(): rk_window_do runs an action and remembers it (up to 50) - its toast
   offers undo. Ctrl (Cmd) and Z undoes, with Shift or Ctrl+Y redoes, outside text fields. Each window has its own. */
typedef struct rk_action {
  uint32_t size;                                   /* sizeof(rk_action) */
  const char *label;                               /* what the toast says: "archived 3 builds" */
  void (*run)(void *user), (*undo)(void *user);
  void *user;
  void (*free_user)(void *user);                   /* NULL, or lets user go when the action leaves the history */
} rk_action;
RK_API void rk_window_do(rk_window *window, const rk_action *action);
RK_API bool rk_window_undo(rk_window *window);     /* false: nothing to undo */
RK_API bool rk_window_redo(rk_window *window);

/* A toast: a short message from the top edge, with an optional action (undo, retry) that keeps it for the toast
   timing and shows the time running out. */
RK_API void rk_window_toast(rk_window *window, const char *message, const char *action, void (*on_action)(void *user), void *user);

/* Views ----------------------------------------------------------------------------------------------------------- */

typedef enum rk_layout {
  RK_LAYOUT_MANUAL,                                /* children keep the frames set on them */
  RK_LAYOUT_VSTACK, RK_LAYOUT_HSTACK,              /* stacks: gap, padding, alignment, grow */
  RK_LAYOUT_TILES,                                 /* the tile grid: units of tile-unit, gap space-s, spans */
  RK_LAYOUT_OVERLAY,                               /* every child fills the padded bounds */
  RK_LAYOUT_FORCE32 = 0x7fffffff
} rk_layout;

typedef enum rk_align { RK_ALIGN_STRETCH, RK_ALIGN_START, RK_ALIGN_CENTER, RK_ALIGN_END, RK_ALIGN_BETWEEN, RK_ALIGN_FORCE32 = 0x7fffffff } rk_align;

typedef enum rk_access_role {
  RK_ACCESS_NONE, RK_ACCESS_GROUP, RK_ACCESS_TEXT, RK_ACCESS_HEADING, RK_ACCESS_BUTTON, RK_ACCESS_LINK,
  RK_ACCESS_LIST, RK_ACCESS_LIST_ITEM, RK_ACCESS_TAB_LIST, RK_ACCESS_TAB, RK_ACCESS_TAB_PANEL, RK_ACCESS_DIALOG,
  RK_ACCESS_IMAGE, RK_ACCESS_PROGRESS, RK_ACCESS_STATUS, RK_ACCESS_TEXT_FIELD, RK_ACCESS_MENU, RK_ACCESS_TABLE,
  RK_ACCESS_CHECKBOX, RK_ACCESS_RADIO_GROUP, RK_ACCESS_SWITCH, RK_ACCESS_SLIDER, RK_ACCESS_SPIN_BUTTON, RK_ACCESS_COMBO_BOX,
  RK_ACCESS_TREE, RK_ACCESS_SEPARATOR,
  RK_ACCESS_FORCE32 = 0x7fffffff
} rk_access_role;

typedef enum rk_press { RK_PRESS_NONE, RK_PRESS_TILE, RK_PRESS_ROW, RK_PRESS_FLAT, RK_PRESS_FORCE32 = 0x7fffffff } rk_press;

RK_API rk_view *rk_view_create(void);              /* a plain view; see rk_vstack() and the like for containers */

/* the tree: adding takes the child over and returns it, so it can be kept to change it later */
RK_API rk_view *rk_add(rk_view *parent, rk_view *child);
RK_API rk_view *rk_insert(rk_view *parent, rk_view *child, int index);
RK_API void rk_view_remove(rk_view *child);       /* from its parent; gone unless you retained it */
RK_API rk_view *rk_view_parent(rk_view *view);
RK_API int rk_view_count(rk_view *view);
RK_API rk_view *rk_view_child(rk_view *view, int index);
RK_API rk_window *rk_view_window(rk_view *view);
RK_API void rk_view_set_name(rk_view *view, const char *name);  /* for finding it: tests, the gallery */
RK_API rk_view *rk_view_find(rk_view *root, const char *name);   /* by name; a button's is its label (or icon) until set */
RK_API void rk_view_set_menu(rk_view *view, rk_view *menu);   /* takes menu: a button opens it; another view on a right click or Shift+F10 */

/* geometry */
RK_API rk_rect rk_view_frame(rk_view *view);      /* in the parent */
RK_API void rk_view_set_frame(rk_view *view, rk_rect frame);    /* used by a MANUAL parent */
RK_API rk_rect rk_view_bounds(rk_view *view);     /* 0, 0, width, height */
RK_API rk_point rk_view_to_window(rk_view *view, rk_point p);
RK_API rk_size rk_view_measure(rk_view *view, rk_size available);   /* its natural size; inf where unbounded */

/* look */
RK_API void rk_view_set_fill(rk_view *view, rk_role role);
RK_API void rk_view_set_fill_color(rk_view *view, rk_color light, rk_color dark);
RK_API void rk_view_set_radius(rk_view *view, float radius);
enum { RK_RULE_TOP = 1, RK_RULE_BOTTOM = 2 };
RK_API void rk_view_set_rules(rk_view *view, uint32_t rules);   /* hairlines in text-low: RK_RULE_TOP | RK_RULE_BOTTOM; 0 none */
RK_API void rk_view_set_opacity(rk_view *view, float opacity);
RK_API float rk_view_opacity(rk_view *view);
RK_API void rk_view_set_transform(rk_view *view, rk_transform transform);
RK_API rk_transform rk_view_transform(rk_view *view);
RK_API void rk_view_set_hidden(rk_view *view, bool hidden);
RK_API bool rk_view_is_hidden(rk_view *view);
RK_API void rk_view_set_clip(rk_view *view, bool clip);

/* layout: the view's own, then what its parent's layout reads from it */
RK_API void rk_view_set_layout(rk_view *view, rk_layout layout);
RK_API void rk_view_set_padding(rk_view *view, rk_insets padding);
RK_API void rk_view_set_gap(rk_view *view, float gap);
RK_API void rk_view_set_align(rk_view *view, rk_align main, rk_align cross);
/* A weight, as CSS flex: 1 or Compose's weight - views with grow share what the others leave in a stack, never less
   than their content; a list or a scroll view may shrink and then scrolls (they have grow 1 from the start). */
RK_API void rk_view_set_grow(rk_view *view, float grow);
RK_API void rk_view_set_size(rk_view *view, float width, float height);   /* fixed; 0: natural */
RK_API void rk_view_set_min_size(rk_view *view, float width, float height);
RK_API void rk_view_set_span(rk_view *view, int columns, int rows);       /* in a TILES parent */
RK_API void rk_view_relayout(rk_view *view);
RK_API void rk_view_redraw(rk_view *view);

/* interaction */
RK_API void rk_view_set_press(rk_view *view, rk_press press);   /* nudges where pressed; activates on release */
RK_API void rk_view_set_focusable(rk_view *view, bool focusable);
/* Disabled, a view and everything in it take no presses, keys or values, built-in views draw greyed, and Tab passes it
   by - unless it has a tooltip, which then says why it is off and Tab still reaches it. */
RK_API void rk_view_set_enabled(rk_view *view, bool enabled);
RK_API bool rk_view_is_enabled(rk_view *view);        /* false when it or a view holding it is disabled */
/* A short ink label after tip-delay under the pointer, at once on keyboard focus; NULL removes it. It repeats or
   completes what the view says, never the only way to learn it. */
RK_API void rk_view_set_tooltip(rk_view *view, const char *text);
RK_API void rk_view_on_activate(rk_view *view, void (*fn)(rk_view *view, void *user), void *user);
/* The one place for data a view owns: free_user lets it go with the view. Callback user pointers elsewhere are
   borrowed - keep what they point to here, or on a controller's state, so it lives exactly as long. */
RK_API void rk_view_set_user(rk_view *view, void *user, void (*free_user)(void *user));
RK_API void *rk_view_user(rk_view *view);

/* accessibility */
RK_API void rk_view_set_access(rk_view *view, rk_access_role role, const char *label);
RK_API void rk_view_set_access_value(rk_view *view, const char *value);

/* Custom views: a class with callbacks, as a UIView subclass overrides draw and layoutSubviews. */

typedef enum rk_event_type {
  RK_EVENT_POINTER_DOWN, RK_EVENT_POINTER_UP, RK_EVENT_POINTER_MOVE, RK_EVENT_POINTER_ENTER, RK_EVENT_POINTER_LEAVE,
  RK_EVENT_WHEEL, RK_EVENT_KEY_DOWN, RK_EVENT_KEY_UP, RK_EVENT_TEXT, RK_EVENT_FOCUS, RK_EVENT_BLUR,
  RK_EVENT_COMPOSE,                                /* text an input method is composing; "" when it is done */
  /* Files dragged from the system: DRAG_MOVE asks the view under the pointer, and its parents, whether they take them
     (true: they do; the system shows a copy). The one that took them hears DRAG_LEAVE when they go elsewhere, or the
     DROP with their paths. Files dropped where nothing takes them are refused. DRAG_ENTER is the platform's; views
     see it as a DRAG_MOVE. */
  RK_EVENT_DRAG_ENTER, RK_EVENT_DRAG_MOVE, RK_EVENT_DRAG_LEAVE, RK_EVENT_DROP,
  RK_EVENT_FORCE32 = 0x7fffffff
} rk_event_type;

/* Keys: printable keys are their uppercase ASCII code ('A', '1'); named keys from 0x100, in ranges kept for them so
   new ones never move the old. */
typedef enum rk_key {
  RK_KEY_NONE = 0, RK_KEY_TAB = 9, RK_KEY_ENTER = 13, RK_KEY_ESCAPE = 27, RK_KEY_SPACE = 32,
  RK_KEY_LEFT = 0x100, RK_KEY_RIGHT, RK_KEY_UP, RK_KEY_DOWN, RK_KEY_HOME, RK_KEY_END, RK_KEY_PAGE_UP, RK_KEY_PAGE_DOWN,
  RK_KEY_BACKSPACE, RK_KEY_DELETE, RK_KEY_INSERT,
  RK_KEY_BACK = 0x130,                             /* the mouse's or the keyboard's back */
  RK_KEY_F1 = 0x140, RK_KEY_F2, RK_KEY_F3, RK_KEY_F4, RK_KEY_F5, RK_KEY_F6, RK_KEY_F7, RK_KEY_F8, RK_KEY_F9, RK_KEY_F10,
  RK_KEY_F11, RK_KEY_F12, RK_KEY_F24 = RK_KEY_F1 + 23,   /* F13 to F24 follow F12 */
  RK_KEY_FORCE32 = 0x7fffffff
} rk_key;
enum { RK_MOD_SHIFT = 1, RK_MOD_CTRL = 2, RK_MOD_ALT = 4, RK_MOD_SUPER = 8 };   /* RK_MOD_SUPER is Cmd on macOS */

typedef struct rk_event {
  uint32_t size;                                   /* sizeof the library's rk_event; read only what you know */
  rk_event_type type;
  double time;                                     /* ms */
  rk_point position;                               /* pointer events: in the receiving view */
  int button;                                      /* 0 primary, 1 secondary, 2 middle */
  float wheel_x, wheel_y;                          /* logical px */
  uint32_t key;                                    /* an rk_key, or a printable key's uppercase ASCII code */
  uint32_t mods;                                   /* RK_MOD_* */
  const char *text;                                /* RK_EVENT_TEXT, RK_EVENT_COMPOSE: UTF-8 */
  const char *const *files; int nfiles;            /* RK_EVENT_DROP: the files' paths, UTF-8, valid for the call */
} rk_event;

typedef struct rk_view_class {
  uint32_t size;                                   /* sizeof(rk_view_class) */
  const char *name;
  void (*draw)(rk_view *view, rk_canvas *canvas, void *state);
  rk_size (*measure)(rk_view *view, rk_size available, void *state);
  void (*layout)(rk_view *view, void *state);      /* place the children with rk_view_set_frame */
  bool (*event)(rk_view *view, const rk_event *event, void *state);   /* true: taken, else it goes up the chain */
  void (*destroy)(void *state);
} rk_view_class;

RK_API rk_view *rk_view_create_custom(const rk_view_class *cls, void *state);
RK_API void *rk_view_state(rk_view *view);
/* For a custom view: a number that moves with the design's motion - set inside rk_animate_begin(RK_MOTION_MORPH) it
   takes duration-long, outside a block duration-short - and the value to draw with while it does. */
RK_API void rk_view_animate_value(rk_view *view, float target);
RK_API float rk_view_value(rk_view *view);
typedef enum rk_cursor { RK_CURSOR_DEFAULT, RK_CURSOR_TEXT, RK_CURSOR_RESIZE_COLUMN, RK_CURSOR_FORCE32 = 0x7fffffff } rk_cursor;
RK_API void rk_view_set_cursor(rk_view *view, rk_cursor cursor);   /* the pointer's shape over it; DEFAULT: its parent's */

/* Built-in views -------------------------------------------------------------------------------------------------- */

typedef enum rk_text_style {
  RK_TEXT_BODY,                                    /* size-s, regular, line-height body */
  RK_TEXT_STRONG,                                  /* size-s, semibold */
  RK_TEXT_SMALL,                                   /* size-xs */
  RK_TEXT_META,                                    /* size-xxs, text-high, tabular figures */
  RK_TEXT_LABEL,                                   /* size-xs, semibold, text-high */
  RK_TEXT_KICKER,                                  /* size-xs, semibold, text-high, tracked */
  RK_TEXT_GROUP,                                   /* size-group, regular, tight */
  RK_TEXT_TITLE,                                   /* size-title, semibold */
  RK_TEXT_STAT,                                    /* size-xxl, medium, tabular figures: a tile's number */
  RK_TEXT_PIVOT,                                   /* size-pivot-active, light: a pivot title */
  RK_TEXT_NAV,                                     /* size-nav-active, light: a place's strip */
  RK_TEXT_FORCE32 = 0x7fffffff
} rk_text_style;

typedef enum rk_status { RK_STATUS_NONE, RK_STATUS_INFO, RK_STATUS_SUCCESS, RK_STATUS_WARNING, RK_STATUS_ERROR, RK_STATUS_FORCE32 = 0x7fffffff } rk_status;

/* Containers with the spacing the design gives them; change it with rk_view_set_gap() and the like. */
RK_API rk_view *rk_vstack(void);                   /* top to bottom, gap space-m (SwiftUI VStack, a flex column) */
RK_API rk_view *rk_hstack(void);                   /* left to right, gap space-s, aligned to the start (HStack) */
RK_API rk_view *rk_tile_grid(void);                /* tiles in units of tile-unit, gap space-s */

RK_API rk_view *rk_label(const char *text, rk_text_style style);   /* wraps at its width */
RK_API void rk_label_set_text(rk_view *label, const char *text);
RK_API void rk_label_set_color(rk_view *label, rk_role role);

typedef enum rk_button_kind { RK_BUTTON_DEFAULT, RK_BUTTON_PRIMARY, RK_BUTTON_QUIET, RK_BUTTON_BAR, RK_BUTTON_FORCE32 = 0x7fffffff } rk_button_kind;
/* A button keeps its own width, also in a column. */
RK_API rk_view *rk_button(const char *label, void (*on_press)(rk_view *button, void *user), void *user);   /* on_press may be NULL */
RK_API void rk_button_set_kind(rk_view *button, rk_button_kind kind);   /* RK_BUTTON_DEFAULT until set */
RK_API void rk_button_set_icon(rk_view *button, const char *icon);      /* from the icon set; NULL removes it */

/* A tile: one status fill, one idea - a value and what it counts. Sizes are spans in a tile grid
   (rk_view_set_span). A tile with an activate handler presses and opens something; without one it only informs. */
RK_API rk_view *rk_tile(const char *value, const char *label);   /* no status until rk_tile_set_status() */
RK_API void rk_tile_set_status(rk_view *tile, rk_status status, const char *word);
RK_API void rk_tile_set_stat(rk_view *tile, const char *value, const char *label);
RK_API void rk_tile_set_message(rk_view *tile, const char *message);     /* NULL removes it */
RK_API void rk_tile_set_series(rk_view *tile, const float *values, int count);   /* a sparkline; 0 removes it */
RK_API void rk_tile_set_progress(rk_view *tile, float progress);        /* 0..1; < 0 removes it */

/* Hairline rows: a title on the left, a meta or a status tag on the right. */
RK_API rk_view *rk_rows(void);
RK_API rk_view *rk_rows_add(rk_view *rows, const char *title, const char *meta, rk_status status);   /* the row, borrowed */
/* A row is a view: rk_insert moves it, rk_view_remove takes it out, and it presses, has a tooltip or a menu like any. */
RK_API void rk_row_set(rk_view *row, const char *title, const char *meta, rk_status status);
/* Rows the user puts in order: each gets a grip that drags it (4 px, then it lifts and the others step aside) or, from
   the keyboard, Space picks it up, Up and Down move it, Space puts it down and Escape puts it back; Alt with Up or Down
   moves it at once. moved() hears each move to keep the app's order (and its undo); rk_rows_move moves one, gliding. */
RK_API void rk_rows_set_sortable(rk_view *rows, void (*moved)(rk_view *rows, int from, int to, void *user), void *user);
RK_API bool rk_rows_move(rk_view *rows, int from, int to);   /* false when out of range, the same place or not sortable */

/* A menu floats by what opened it: below it, above where there is no room. One item a row - a lowercase verb or
   option, a key hint at the end; an item with a check (rk_menu_set_check) toggles and stays open, any other closes
   the menu and then runs its handler. Up, Down, Home, End, a letter, Enter or Space, Escape. */
RK_API rk_view *rk_menu(void);
RK_API int rk_menu_add(rk_view *menu, const char *label, const char *key, void (*fn)(rk_view *menu, int item, void *user), void *user);   /* the item's index */
RK_API void rk_menu_add_separator(rk_view *menu);
RK_API void rk_menu_add_label(rk_view *menu, const char *label);   /* a group's name */
RK_API void rk_menu_clear(rk_view *menu);                          /* every item goes: build it again */
typedef enum rk_checkmark { RK_CHECKMARK_NONE, RK_CHECKMARK_OFF, RK_CHECKMARK_ON, RK_CHECKMARK_FORCE32 = 0x7fffffff } rk_checkmark;   /* NONE: no square */
RK_API void rk_menu_set_check(rk_view *menu, int item, rk_checkmark check);
RK_API rk_checkmark rk_menu_check(rk_view *menu, int item);
RK_API void rk_menu_set_enabled(rk_view *menu, int item, bool enabled);
RK_API void rk_menu_open(rk_view *menu, rk_view *from);            /* below from; focus goes back to it */
RK_API void rk_menu_open_at(rk_view *menu, rk_view *from, rk_point point);   /* at a point in from: a context menu */
RK_API void rk_menu_close(rk_view *menu);
RK_API rk_view *rk_menu_opener(rk_view *menu);                    /* what opened it, while open and in a handler */

/* A view that scrolls its content up and down: the wheel, the keys a focused view inside leaves, and focus landing
   inside move it, on the panorama's spring; a thin bar shows while it moves. Takes content. */
RK_API rk_view *rk_scroll(rk_view *content);
RK_API void rk_view_reveal(rk_view *view);                 /* every scroll it is in brings it in sight */

/* A picture from a PNG or JPEG file, or from those bytes in memory (copied). Its size is known at once; it is decoded
   away from the UI thread and fades in where a placeholder stood. It shows whole (contain) or fills its view (cover),
   its shape kept; its natural size is its pixels, smaller where it has less room. */
typedef enum rk_fit { RK_FIT_CONTAIN, RK_FIT_COVER, RK_FIT_FORCE32 = 0x7fffffff } rk_fit;
RK_API rk_view *rk_image(const char *path);
RK_API rk_view *rk_image_from_memory(const void *data, size_t size);
RK_API void rk_image_set_fit(rk_view *image, rk_fit fit);
RK_API bool rk_image_size(rk_view *image, int *width, int *height);   /* its pixels; false when it cannot be read */

/* Controls that hold a value, as the web's. Each holds a number - 0 or 1, the chosen index (-1: none yet), the
   number, a date as yyyymmdd - that the app reads and sets; the change handler hears what the user changes, never what
   the app sets (a field's handler too). */
RK_API rk_view *rk_check(const char *label, bool on);       /* a checkbox: Space toggles */
RK_API rk_view *rk_choices(const char *const *options, int count, int chosen);   /* options, one chosen (-1: none yet) */
RK_API rk_view *rk_toggle(const char *label, bool on);      /* the Metro switch, "on" or "off" beside it */
RK_API rk_view *rk_segments(const char *const *options, int count, int chosen);  /* two to five, side by side */
RK_API rk_view *rk_slider(double min, double max, double step, double value);   /* step 0: continuous */
RK_API rk_view *rk_stepper(double min, double max, double step, double value);  /* minus, the number, plus */
RK_API rk_view *rk_select(const char *const *options, int count, int chosen);    /* one of a list, from a menu below */
RK_API rk_view *rk_date(int year, int month, int day);    /* 0, 0, 0: none yet */
RK_API bool rk_date_value(rk_view *date, int *year, int *month, int *day);   /* false: none yet */
RK_API void rk_date_set_value(rk_view *date, int year, int month, int day);   /* 0, 0, 0 clears it */
RK_API double rk_control_value(rk_view *control);
RK_API void rk_control_set_value(rk_view *control, double value);   /* a number snaps to the range and step; an index it lacks is ignored */
RK_API void rk_control_on_change(rk_view *control, void (*fn)(rk_view *control, void *user), void *user);

/* A text field, as the web's: a label above, the input, a hint below - or an error, its cause and remedy, marked.
   One line, edited with the desktop's keys and the pointer; Enter submits. With more lines it is a text area: the
   text wraps at its width, Enter breaks a line and Ctrl+Enter submits. */
RK_API rk_view *rk_field(const char *label, const char *placeholder);   /* either may be NULL */
RK_API const char *rk_field_text(rk_view *field);          /* UTF-8; valid until the text changes */
RK_API void rk_field_set_text(rk_view *field, const char *text);
RK_API void rk_field_set_hint(rk_view *field, const char *hint);     /* below the input; NULL removes it */
RK_API void rk_field_set_error(rk_view *field, const char *error);   /* the cause and remedy, marked, in the hint's place */
RK_API void rk_field_on_change(rk_view *field, void (*fn)(rk_view *field, void *user), void *user);   /* what the user types, not rk_field_set_text */
RK_API void rk_field_on_submit(rk_view *field, void (*fn)(rk_view *field, void *user), void *user);   /* Enter */
RK_API void rk_field_set_lines(rk_view *field, int lines);   /* the lines it shows; 1, the default, is one line */
RK_API void rk_field_set_secret(rk_view *field, bool secret);   /* a password: dots shown, never copied or read aloud */
RK_API void rk_field_set_options(rk_view *field, const char *const *options, int count);   /* a combobox: suggestions as you type */

/* A long list: only the rows in sight are drawn and nothing is kept for each item - item() fills a row while it is
   drawn, so the strings it gives need to last only for the call (out->size says how much of the struct this library
   knows). Hairline rows, selected the Metro way: a click selects one and shows it (on_show); Ctrl+click toggles and
   starts selecting several; Shift+click takes a range. In a stack it fills what the others leave, and scrolls. */
typedef struct rk_list_item { uint32_t size; const char *title, *meta; rk_status status; } rk_list_item;
RK_API rk_view *rk_list(int count, void (*item)(int index, rk_list_item *out, void *user), void *user);
RK_API void rk_list_set_count(rk_view *list, int count);   /* the items changed: drawn again */
/* What identifies an item, called with the list's user: then the selection and the active row follow the items when
   they move (a sort, items added above). Without keys an item is its index, and a sort lets the selection go. */
RK_API void rk_list_set_key(rk_view *list, uint64_t (*key)(int index, void *user));
RK_API void rk_list_on_show(rk_view *list, void (*fn)(rk_view *list, int index, void *user), void *user);
RK_API void rk_list_on_change(rk_view *list, void (*fn)(rk_view *list, void *user), void *user);   /* the user changed the selection */
RK_API bool rk_list_is_selected(rk_view *list, int index);
RK_API int rk_list_selection_count(rk_view *list);
RK_API int rk_list_selection(rk_view *list, int *indices, int max);   /* fills up to max, in order; returns how many there are */
RK_API void rk_list_select(rk_view *list, int index, bool selected);   /* the app's choice: drawn, not told */
RK_API void rk_list_clear_selection(rk_view *list);
RK_API int rk_list_active(rk_view *list);                  /* the row the keyboard is on; -1 when empty */
RK_API void rk_list_scroll_to(rk_view *list, int index);   /* glides it into sight and makes it the active row */

/* A place to drop files: a dashed edge that turns solid while files are over it. accept filters them as the file
   chooser does; files() hears those taken and those refused, only when one was taken. A click or Enter opens the
   system's file chooser with the same filter. Files dropped where nothing takes them are refused. */
RK_API rk_view *rk_drop(const char *title, const char *hint, const char *accept,
                        void (*files)(rk_view *drop, const char *const *taken, int ntaken, const char *const *refused, int nrefused, void *user), void *user);

/* A tree: items that hold items, opened and closed in place. The app adds the items and keeps their numbers; parent
   -1 is the top. A click, Enter or Space selects an item and shows it (on_show); selection never follows focus. A
   click on the chevron or a double click opens or closes; Right and Left open and close or go in and up; * opens every
   sibling; a letter goes to the next item that starts with it. Long trees go in an rk_scroll. */
RK_API rk_view *rk_tree(void);
RK_API int rk_tree_add(rk_view *tree, int parent, const char *title);   /* the new item's number, or -1 */
RK_API void rk_tree_set_meta(rk_view *tree, int item, const char *meta, rk_status status);   /* on the right, as a row's */
RK_API void rk_tree_set_open(rk_view *tree, int item, bool open);   /* glides as a click does */
RK_API void rk_tree_set_all_open(rk_view *tree, bool open);
RK_API bool rk_tree_is_open(rk_view *tree, int item);
RK_API void rk_tree_select(rk_view *tree, int item);   /* -1: none; shows nothing */
RK_API int rk_tree_selected(rk_view *tree);
RK_API int rk_tree_parent(rk_view *tree, int item);   /* -1 at the top */
RK_API const char *rk_tree_title(rk_view *tree, int item);
RK_API void rk_tree_set_title(rk_view *tree, int item, const char *title);
RK_API void rk_tree_set_user(rk_view *tree, int item, void *user);   /* the app's own, for its item */
RK_API void *rk_tree_user(rk_view *tree, int item);
RK_API void rk_tree_remove(rk_view *tree, int item);      /* with everything under it; its number is not given again */
RK_API void rk_tree_clear(rk_view *tree);                 /* every item; numbers start again at 0 */
RK_API void rk_tree_on_show(rk_view *tree, void (*fn)(rk_view *tree, int item, void *user), void *user);
RK_API void rk_tree_on_toggle(rk_view *tree, void (*fn)(rk_view *tree, int item, bool open, void *user), void *user);

/* A split: a list beside its detail. Sideways (the split 760 px wide or more, the window 481 px tall or more) the two
   stand side by side and the handle between them drags the list's width: no pane narrower than pane-min, four tiles
   wide at first, a double click gives that back. Compact one pane shows at a time: rk_split_show turns to the detail,
   whose crumb (the title) and Escape turn back. Each pane scrolls itself: give it an rk_scroll where it may be long. */
RK_API rk_view *rk_split(rk_view *list, rk_view *detail, const char *title);   /* takes both; title: the list's, for the crumb */
RK_API void rk_split_set_detail_shown(rk_view *split, bool shown);   /* sideways: a new item's detail slides in; compact: turns */
RK_API bool rk_split_is_detail_shown(rk_view *split);     /* compact: which pane shows */
RK_API bool rk_split_is_sideways(rk_view *split);
RK_API void rk_split_set_width(rk_view *split, float width);   /* the list's, as remembered; 0: its own */
RK_API float rk_split_width(rk_view *split);              /* as laid out now */
RK_API void rk_split_on_resize(rk_view *split, void (*fn)(rk_view *split, float width, void *user), void *user);   /* to remember it */

/* A table is a list with columns - every rk_list_ call works on it. The head stays put; a click on a sortable head
   sorts by it, ascending then descending: the app sorts its rows in on_sort and the table draws them again. */
enum { RK_COLUMN_NUMERIC = 1, RK_COLUMN_SORTABLE = 2, RK_COLUMN_EDITABLE = 4 };   /* numbers sit to the right */
typedef enum rk_sort { RK_SORT_NONE, RK_SORT_ASCENDING, RK_SORT_DESCENDING, RK_SORT_FORCE32 = 0x7fffffff } rk_sort;
RK_API rk_view *rk_table(int count, const char *(*cell)(int row, int column, void *user), void *user);
RK_API int rk_table_add_column(rk_view *table, const char *title, float width, uint32_t flags);   /* width RK_AUTO: shares the rest */
RK_API void rk_table_on_sort(rk_view *table, void (*fn)(rk_view *table, int column, rk_sort sort, void *user), void *user);
RK_API void rk_table_set_sort(rk_view *table, int column, rk_sort sort);   /* shows the sort the rows have */
RK_API void rk_table_set_column_width(rk_view *table, int column, float width);   /* as a drag leaves it; 0: its own */
RK_API float rk_table_column_width(rk_view *table, int column);   /* as laid out now */
/* Editing a cell: a double click or F2 opens it, Enter or Tab keeps the value (Tab goes on to the next editable cell),
   Escape puts the old one back. commit() returns NULL to take the value, or why not - shown below the cell, which
   stays open. */
RK_API void rk_table_on_edit(rk_view *table, const char *(*commit)(rk_view *table, int row, int column, const char *value, void *user), void *user);
RK_API void rk_table_edit(rk_view *table, int row, int column);   /* opens the cell for editing */

RK_API rk_view *rk_icon(const char *name, rk_role color);   /* 20 × 20 from the icon set */
RK_API rk_view *rk_progress(float progress);                /* < 0: indeterminate */
RK_API void rk_progress_set_value(rk_view *progress, float value);

/* Controllers ----------------------------------------------------------------------------------------------------- */

typedef struct rk_controller_class {
  uint32_t size;                                   /* sizeof(rk_controller_class) */
  const char *name;
  rk_view *(*load)(rk_controller *controller, void *state);         /* build the root view; return a reference */
  void (*appear)(rk_controller *controller, void *state);           /* it became visible (after any motion) */
  void (*disappear)(rk_controller *controller, void *state);
  bool (*event)(rk_controller *controller, const rk_event *event, void *state);   /* the responder chain */
  void (*destroy)(void *state);
} rk_controller_class;

RK_API rk_controller *rk_controller_create(const rk_controller_class *cls, void *state);
RK_API rk_controller *rk_controller_create_with_view(rk_view *view, const char *title);   /* takes view; no class needed */
RK_API void *rk_controller_state(rk_controller *controller);
RK_API rk_view *rk_controller_view(rk_controller *controller);    /* loads it on first use */
RK_API void rk_controller_set_title(rk_controller *controller, const char *title);
RK_API const char *rk_controller_title(rk_controller *controller);   /* as set; the title bar names what shows */
RK_API rk_controller *rk_controller_parent(rk_controller *controller);
/* A controller inside another's views (a pivot in a page): the owner takes child, gives it its window, tells it when
   it appears and goes, and shows the sheets it presents. Returns child's view as a reference: add it where it goes. */
RK_API rk_view *rk_controller_embed(rk_controller *owner, rk_controller *child);
RK_API rk_window *rk_controller_window(rk_controller *controller);

/* Places: a navigation stack. Going to a place turns the old one away and the new one in; back (Escape, the back
   button, Alt+Left, the mouse's back button) turns the other way and puts focus back where it was. */
RK_API rk_controller *rk_places(rk_controller *home);   /* takes home */
RK_API void rk_places_go(rk_controller *places, rk_controller *place);   /* takes place */
RK_API void rk_places_back(rk_controller *places);
RK_API int rk_places_depth(rk_controller *places);

/* Panorama: one place laid out sideways in sections under a strip of their titles that glides by place. */
RK_API rk_controller *rk_panorama(void);
RK_API void rk_panorama_add(rk_controller *panorama, const char *title, rk_view *section, float width);   /* takes section; width 0: natural */

/* Pivot: facets of one place in a fixed order; the active title is large; a change slides from its side. */
RK_API rk_controller *rk_pivot(void);
RK_API void rk_pivot_add(rk_controller *pivot, const char *title, rk_view *page);   /* takes page, as rk_panorama_add */
RK_API void rk_pivot_add_controller(rk_controller *pivot, rk_controller *facet);  /* takes facet; its title, live */
RK_API void rk_pivot_select(rk_controller *pivot, int index);
RK_API int rk_pivot_selected(rk_controller *pivot);
typedef enum rk_pivot_layout { RK_PIVOT_TOP, RK_PIVOT_SIDE, RK_PIVOT_LAYOUT_FORCE32 = 0x7fffffff } rk_pivot_layout;
RK_API void rk_pivot_set_layout(rk_controller *pivot, rk_pivot_layout layout);   /* titles along the top or down the leading side */

/* Sheets: a short question from the top edge over a shaded backdrop. Escape or a click on the shading dismisses. */
RK_API void rk_controller_present(rk_controller *presenter, rk_controller *sheet);   /* takes sheet */
RK_API void rk_controller_dismiss(rk_controller *sheet);
/* A short question in a sheet, as the web's confirm(): the safe answer is the primary button and has focus (cancel,
   for what cannot be undone); Escape and the shading answer cancel. answered() hears it once. */
typedef struct rk_confirm_desc {
  uint32_t size;                                   /* sizeof(rk_confirm_desc) */
  const char *title, *text, *kicker;               /* the question; a line under it; a word above it */
  const char *ok, *cancel;                         /* NULL: "ok", "cancel" */
  bool cancel_is_safe;
} rk_confirm_desc;
RK_API void rk_confirm(rk_controller *presenter, const rk_confirm_desc *desc, void (*answered)(bool ok, void *user), void *user);

/* Motion ---------------------------------------------------------------------------------------------------------- */

typedef enum rk_motion {
  RK_MOTION_STATE,                                 /* duration-short, ease-base: colour, opacity, transform */
  RK_MOTION_REFLOW,                                /* duration-moderate: what the layout moves or resizes glides */
  RK_MOTION_ENTER,                                 /* views added build in */
  RK_MOTION_LEAVE,                                 /* views removed leave first; the rest close up */
  RK_MOTION_MORPH,                                 /* duration-long: values */
  RK_MOTION_FORCE32 = 0x7fffffff
} rk_motion;

typedef enum rk_transition { RK_TRANSITION_PEEK, RK_TRANSITION_FLIP, RK_TRANSITION_CROSSFADE, RK_TRANSITION_FORCE32 = 0x7fffffff } rk_transition;

RK_API void rk_animate_begin(rk_motion motion);    /* blocks nest; the innermost motion wins */
RK_API void rk_animate_on_done(void (*fn)(void *user), void *user);
RK_API void rk_animate_commit(void);
RK_API void rk_view_transition(rk_view *view, rk_transition transition);   /* call before changing the view */

/* Drawing (custom views) ------------------------------------------------------------------------------------------ */

RK_API void rk_canvas_fill_rect(rk_canvas *canvas, rk_rect rect, rk_color color);
RK_API void rk_canvas_fill_round_rect(rk_canvas *canvas, rk_rect rect, float radius, rk_color color);
RK_API void rk_canvas_stroke_rect(rk_canvas *canvas, rk_rect rect, float width, rk_color color);   /* inside the rect */
RK_API void rk_canvas_fill_polygon(rk_canvas *canvas, const rk_point *points, int count, rk_color color);
RK_API void rk_canvas_polyline(rk_canvas *canvas, const rk_point *points, int count, float width, rk_color color);
RK_API void rk_canvas_icon(rk_canvas *canvas, const char *name, rk_rect rect, rk_color color);
RK_API float rk_canvas_text(rk_canvas *canvas, const char *text, rk_point origin, rk_text_style style, rk_color color);  /* one line from its top left; returns the width */
RK_API void rk_canvas_push_clip(rk_canvas *canvas, rk_rect rect);
RK_API void rk_canvas_pop_clip(rk_canvas *canvas);
RK_API rk_size rk_text_measure(const char *text, rk_text_style style, float max_width);   /* max_width 0: one line */

#ifdef __cplusplus
}
#endif

#endif /* REKTA_H */

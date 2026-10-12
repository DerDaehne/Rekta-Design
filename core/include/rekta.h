/* rekta.h: Rekta Design core, native views for desktop apps (Windows, macOS, Linux).
 *
 * The ABI rules (docs/core.md):
 *   - C99. Functions rk_noun_verb, types rk_noun, constants RK_NOUN.
 *   - Value types cross by value and never change layout. Units are logical pixels.
 *   - Objects are opaque and reference counted: a function named *_create returns a reference you own, everything
 *     else is borrowed. A container retains what it holds.
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
  RK_ROLE_COUNT,
  RK_ROLE_FORCE32 = 0x7fffffff
} rk_role;

typedef enum rk_metric {
  RK_SPACE_XS, RK_SPACE_S, RK_SPACE_M, RK_SPACE_L, RK_SPACE_XL,
  RK_RADIUS_S, RK_RADIUS_M, RK_RADIUS_L, RK_RADIUS_TILE,
  RK_TILE_UNIT, RK_APPBAR_HEIGHT, RK_TOUCH_TARGET, RK_PANE_MIN,
  RK_CONTROL_HEIGHT, RK_ROW_HEIGHT, RK_CONTROL_PADDING,   /* these three follow the density */
  RK_METRIC_COUNT,
  RK_METRIC_FORCE32 = 0x7fffffff
} rk_metric;

typedef enum rk_duration {
  RK_DURATION_PRESS, RK_DURATION_SHORT, RK_DURATION_MODERATE, RK_DURATION_LONG, RK_DURATION_STAGGER,
  RK_DURATION_FLOAT, RK_DURATION_PIVOT_GLIDE, RK_DURATION_PIVOT_SIZE,
  RK_TIMING_TOAST, RK_TIMING_TAP_DELAY, RK_TIMING_LONG_PRESS, RK_TIMING_TIP_DELAY,
  RK_DURATION_COUNT,
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
RK_API uint32_t rk_app_after(double ms, bool repeat, void (*fn)(void *user), void *user);   /* a timer id, never 0 */
RK_API void rk_app_cancel(uint32_t timer);

typedef enum rk_theme { RK_THEME_SYSTEM, RK_THEME_LIGHT, RK_THEME_DARK, RK_THEME_FORCE32 = 0x7fffffff } rk_theme;
typedef enum rk_motion_pref { RK_MOTION_PREF_SYSTEM, RK_MOTION_PREF_ON, RK_MOTION_PREF_OFF, RK_MOTION_PREF_FORCE32 = 0x7fffffff } rk_motion_pref;
typedef enum rk_density { RK_DENSITY_SYSTEM, RK_DENSITY_COMFORTABLE, RK_DENSITY_COMPACT, RK_DENSITY_FORCE32 = 0x7fffffff } rk_density;

RK_API void rk_app_set_theme(rk_theme theme);
RK_API bool rk_app_is_dark(void);
RK_API void rk_app_set_motion(rk_motion_pref motion);
RK_API bool rk_app_reduced_motion(void);
RK_API void rk_app_set_density(rk_density density);
RK_API void rk_app_set_accent(rk_color light, rk_color dark);    /* 0, 0: the ink of the theme */
RK_API void rk_app_on_change(void (*fn)(void *user), void *user);   /* theme, motion, density or accent changed */

/* Window ---------------------------------------------------------------------------------------------------------- */

typedef struct rk_window_desc {
  uint32_t size;                                   /* sizeof(rk_window_desc) */
  const char *title;
  float width, height;                             /* content size; 0: 1280 × 800 */
  float min_width, min_height;                     /* 0: pane-min × 320 */
  bool system_frame;                               /* keep the system's title bar instead of Rekta's */
} rk_window_desc;

RK_API rk_window *rk_window_create(const rk_window_desc *desc);
RK_API void rk_window_set_root(rk_window *window, rk_controller *root);
RK_API rk_controller *rk_window_root(rk_window *window);
RK_API void rk_window_show(rk_window *window);
RK_API void rk_window_close(rk_window *window);
RK_API void rk_window_set_title(rk_window *window, const char *title);
RK_API rk_size rk_window_size(rk_window *window);
RK_API float rk_window_scale(rk_window *window);   /* device pixels per logical px */
RK_API void rk_window_on_close(rk_window *window, bool (*should_close)(rk_window *window, void *user), void *user);
RK_API bool rk_window_focus(rk_window *window, rk_view *view);  /* false if the view cannot take focus */
RK_API rk_view *rk_window_focused(rk_window *window);
/* A toast: a short message from the top edge, with an optional action (undo, retry) that keeps it for the toast
   timing and shows the time running out. */
RK_API void rk_window_toast(rk_window *window, const char *message, const char *action, void (*on_action)(void *user), void *user);

/* Views ----------------------------------------------------------------------------------------------------------- */

typedef enum rk_layout {
  RK_LAYOUT_MANUAL,                                /* children keep the frames set on them */
  RK_LAYOUT_COLUMN, RK_LAYOUT_ROW,                 /* stacks: gap, padding, alignment, grow */
  RK_LAYOUT_TILES,                                 /* the tile grid: units of tile-unit, gap space-s, spans */
  RK_LAYOUT_OVERLAY,                               /* every child fills the padded bounds */
  RK_LAYOUT_FORCE32 = 0x7fffffff
} rk_layout;

typedef enum rk_align { RK_ALIGN_STRETCH, RK_ALIGN_START, RK_ALIGN_CENTER, RK_ALIGN_END, RK_ALIGN_BETWEEN, RK_ALIGN_FORCE32 = 0x7fffffff } rk_align;

typedef enum rk_access_role {
  RK_ACCESS_NONE, RK_ACCESS_GROUP, RK_ACCESS_TEXT, RK_ACCESS_HEADING, RK_ACCESS_BUTTON, RK_ACCESS_LINK,
  RK_ACCESS_LIST, RK_ACCESS_LIST_ITEM, RK_ACCESS_TAB_LIST, RK_ACCESS_TAB, RK_ACCESS_TAB_PANEL, RK_ACCESS_DIALOG,
  RK_ACCESS_IMAGE, RK_ACCESS_PROGRESS, RK_ACCESS_STATUS,
  RK_ACCESS_FORCE32 = 0x7fffffff
} rk_access_role;

typedef enum rk_press { RK_PRESS_NONE, RK_PRESS_TILE, RK_PRESS_ROW, RK_PRESS_FLAT, RK_PRESS_FORCE32 = 0x7fffffff } rk_press;

RK_API rk_view *rk_view_create(void);              /* a plain view: a container with a layout */

/* the tree */
RK_API void rk_view_add(rk_view *parent, rk_view *child);       /* retains child; moves it if it has a parent */
RK_API void rk_view_insert(rk_view *parent, rk_view *child, int index);
RK_API void rk_view_remove(rk_view *child);       /* from its parent */
RK_API rk_view *rk_view_parent(rk_view *view);
RK_API int rk_view_count(rk_view *view);
RK_API rk_view *rk_view_child(rk_view *view, int index);
RK_API rk_window *rk_view_window(rk_view *view);
RK_API void rk_view_set_name(rk_view *view, const char *name);  /* for finding it: tests, the gallery */
RK_API rk_view *rk_view_find(rk_view *root, const char *name);

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
RK_API void rk_view_set_rule(rk_view *view, bool top, bool bottom);  /* hairlines in text-low */
RK_API void rk_view_set_opacity(rk_view *view, float opacity);
RK_API float rk_view_opacity(rk_view *view);
RK_API void rk_view_set_transform(rk_view *view, rk_transform transform);
RK_API rk_transform rk_view_transform(rk_view *view);
RK_API void rk_view_set_hidden(rk_view *view, bool hidden);
RK_API bool rk_view_hidden(rk_view *view);
RK_API void rk_view_set_clip(rk_view *view, bool clip);

/* layout: the view's own, then what its parent's layout reads from it */
RK_API void rk_view_set_layout(rk_view *view, rk_layout layout);
RK_API void rk_view_set_padding(rk_view *view, rk_insets padding);
RK_API void rk_view_set_gap(rk_view *view, float gap);
RK_API void rk_view_set_align(rk_view *view, rk_align main, rk_align cross);
RK_API void rk_view_set_grow(rk_view *view, float grow);        /* share of the free space in a stack */
RK_API void rk_view_set_size(rk_view *view, float width, float height);   /* fixed; 0: natural */
RK_API void rk_view_set_min_size(rk_view *view, float width, float height);
RK_API void rk_view_set_span(rk_view *view, int columns, int rows);       /* in a TILES parent */
RK_API void rk_view_relayout(rk_view *view);
RK_API void rk_view_redraw(rk_view *view);

/* interaction */
RK_API void rk_view_set_press(rk_view *view, rk_press press);   /* nudges where pressed; activates on release */
RK_API void rk_view_set_focusable(rk_view *view, bool focusable);
RK_API void rk_view_on_activate(rk_view *view, void (*fn)(rk_view *view, void *user), void *user);
RK_API void rk_view_set_user(rk_view *view, void *user, void (*free_user)(void *user));
RK_API void *rk_view_user(rk_view *view);

/* accessibility */
RK_API void rk_view_set_access(rk_view *view, rk_access_role role, const char *label);
RK_API void rk_view_set_access_value(rk_view *view, const char *value);

/* Custom views: a class with callbacks, as a UIView subclass overrides draw and layoutSubviews. */

typedef enum rk_event_type {
  RK_EVENT_POINTER_DOWN, RK_EVENT_POINTER_UP, RK_EVENT_POINTER_MOVE, RK_EVENT_POINTER_ENTER, RK_EVENT_POINTER_LEAVE,
  RK_EVENT_WHEEL, RK_EVENT_KEY_DOWN, RK_EVENT_KEY_UP, RK_EVENT_TEXT, RK_EVENT_FOCUS, RK_EVENT_BLUR,
  RK_EVENT_FORCE32 = 0x7fffffff
} rk_event_type;

enum {
  RK_KEY_NONE = 0, RK_KEY_TAB = 9, RK_KEY_ENTER = 13, RK_KEY_ESCAPE = 27, RK_KEY_SPACE = 32,
  /* printable keys are their uppercase ASCII code ('A', '1'); named keys from 0x100 */
  RK_KEY_LEFT = 0x100, RK_KEY_RIGHT, RK_KEY_UP, RK_KEY_DOWN, RK_KEY_HOME, RK_KEY_END, RK_KEY_PAGE_UP, RK_KEY_PAGE_DOWN,
  RK_KEY_BACKSPACE, RK_KEY_DELETE, RK_KEY_F1, RK_KEY_F10 = RK_KEY_F1 + 9, RK_KEY_BACK /* mouse or keyboard back */
};
enum { RK_MOD_SHIFT = 1, RK_MOD_CTRL = 2, RK_MOD_ALT = 4, RK_MOD_SUPER = 8 };   /* RK_MOD_SUPER is Cmd on macOS */

typedef struct rk_event {
  uint32_t size;                                   /* sizeof the library's rk_event; read only what you know */
  rk_event_type type;
  double time;                                     /* ms */
  rk_point position;                               /* pointer events: in the receiving view */
  int button;                                      /* 0 primary, 1 secondary, 2 middle */
  float wheel_x, wheel_y;                          /* logical px */
  uint32_t key;                                    /* RK_KEY_* */
  uint32_t mods;                                   /* RK_MOD_* */
  const char *text;                                /* RK_EVENT_TEXT: UTF-8 */
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
  RK_TEXT_STYLE_COUNT,
  RK_TEXT_FORCE32 = 0x7fffffff
} rk_text_style;

typedef enum rk_status { RK_STATUS_NONE, RK_STATUS_INFO, RK_STATUS_SUCCESS, RK_STATUS_WARNING, RK_STATUS_ERROR, RK_STATUS_FORCE32 = 0x7fffffff } rk_status;

RK_API rk_view *rk_label_create(const char *text, rk_text_style style);   /* wraps at its width */
RK_API void rk_label_set_text(rk_view *label, const char *text);
RK_API void rk_label_set_color(rk_view *label, rk_role role);

typedef enum rk_button_kind { RK_BUTTON_DEFAULT, RK_BUTTON_PRIMARY, RK_BUTTON_QUIET, RK_BUTTON_BAR, RK_BUTTON_FORCE32 = 0x7fffffff } rk_button_kind;
RK_API rk_view *rk_button_create(const char *label, const char *icon, rk_button_kind kind);   /* icon may be NULL */
RK_API void rk_button_set_disabled(rk_view *button, const char *reason);  /* NULL enables; the reason is its tooltip */

/* A tile: one status fill, one idea. Sizes are spans in a TILES parent (rk_view_set_span). A tile with an
   activate handler presses and opens something; without one it only informs. */
RK_API rk_view *rk_tile_create(rk_status status, const char *word);
RK_API void rk_tile_set_status(rk_view *tile, rk_status status, const char *word);
RK_API void rk_tile_set_stat(rk_view *tile, const char *value, const char *label);
RK_API void rk_tile_set_message(rk_view *tile, const char *message);     /* NULL removes it */
RK_API void rk_tile_set_series(rk_view *tile, const float *values, int count);   /* a sparkline; 0 removes it */
RK_API void rk_tile_set_progress(rk_view *tile, float progress);        /* 0..1; < 0 removes it */

/* Hairline rows: a title on the left, a meta or a status tag on the right. */
RK_API rk_view *rk_rows_create(void);
RK_API rk_view *rk_rows_add(rk_view *rows, const char *title, const char *meta, rk_status status);   /* the row, borrowed */

RK_API rk_view *rk_icon_create(const char *name, rk_role color);   /* 20 × 20 from the icon set */
RK_API rk_view *rk_progress_create(float progress);                /* < 0: indeterminate */
RK_API void rk_progress_set(rk_view *progress, float progress);

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
RK_API rk_controller *rk_controller_create_with_view(rk_view *view, const char *title);   /* no class needed */
RK_API void *rk_controller_state(rk_controller *controller);
RK_API rk_view *rk_controller_view(rk_controller *controller);    /* loads it on first use */
RK_API void rk_controller_set_title(rk_controller *controller, const char *title);
RK_API const char *rk_controller_title(rk_controller *controller);
RK_API rk_controller *rk_controller_parent(rk_controller *controller);
RK_API rk_window *rk_controller_window(rk_controller *controller);

/* Places: a navigation stack. Going to a place turns the old one away and the new one in; back (Escape, the back
   button, Alt+Left, the mouse's back button) turns the other way and puts focus back where it was. */
RK_API rk_controller *rk_places_create(rk_controller *home);
RK_API void rk_places_go(rk_controller *places, rk_controller *place);
RK_API void rk_places_back(rk_controller *places);
RK_API int rk_places_depth(rk_controller *places);

/* Panorama: one place laid out sideways in sections under a strip of their titles that glides by place. */
RK_API rk_controller *rk_panorama_create(void);
RK_API void rk_panorama_add(rk_controller *panorama, const char *title, rk_view *section, float width);   /* 0: the natural width */

/* Pivot: facets of one place in a fixed order; the active title is large; a change slides from its side. */
RK_API rk_controller *rk_pivot_create(void);
RK_API void rk_pivot_add(rk_controller *pivot, rk_controller *facet);
RK_API void rk_pivot_select(rk_controller *pivot, int index);
RK_API int rk_pivot_selected(rk_controller *pivot);

/* Sheets: a short question from the top edge over a shaded backdrop. Escape or a click on the shading dismisses. */
RK_API void rk_controller_present(rk_controller *presenter, rk_controller *sheet);
RK_API void rk_controller_dismiss(rk_controller *sheet);

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

/* Testing --------------------------------------------------------------------------------------------------------- */

RK_API void rk_window_inject(rk_window *window, const rk_event *event);   /* as if it came from the system */
RK_API bool rk_window_snapshot(rk_window *window, const char *png_path);  /* the window's pixels as a PNG */
RK_API void rk_app_run_for(double ms);             /* headless: advance the virtual clock, running every frame */

#ifdef __cplusplus
}
#endif

#endif /* REKTA_H */

/* rk_internal.h: what the parts of rekta-core share. Not part of the ABI. */
#ifndef RK_INTERNAL_H
#define RK_INTERNAL_H

#include "../include/rekta.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define RK_MIN(a, b) ((a) < (b) ? (a) : (b))
#define RK_MAX(a, b) ((a) > (b) ? (a) : (b))
#define RK_CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : (v) > (hi) ? (hi) : (v))
#define RK_COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

/* Objects --------------------------------------------------------------------------------------------------------- */

typedef struct rk_type { const char *name; void (*destroy)(void *self); } rk_type;
typedef struct rk_object { const rk_type *type; int refs; } rk_object;

void *rk__new(const rk_type *type, size_t size);          /* zeroed, one reference */
void *rk__alloc(size_t size);                              /* zeroed; aborts when memory runs out */
void *rk__grow(void *p, int *cap, int need, size_t item);  /* a growable array */
char *rk__strdup(const char *s);
void rk__setstr(char **dst, const char *src);

/* Preferences and tokens (tokens.c) ------------------------------------------------------------------------------- */

typedef struct rk_prefs {
  rk_theme theme; rk_motion_pref motion; rk_density density;
  bool system_dark, system_reduced, system_compact;
  rk_color accent[2];
} rk_prefs;
extern rk_prefs rk__prefs;

bool rk__dark(void);
bool rk__reduced(void);
int rk__density_index(void);
float rk__ease(const float bezier[4], float t);            /* cubic-bezier timing, t in 0..1 */
extern const float rk__ease_base[4], rk__ease_in[4], rk__ease_out[4], rk__ease_glide[4];
rk_color rk__mix(rk_color a, rk_color b, float t);

/* Raster (raster.c): premultiplied 32-bit pixels, 0xAARRGGBB in memory order B, G, R, A ------------------------------ */

typedef struct rk_bitmap {
  int w, h, stride; uint32_t *px; size_t cap;              /* stride and cap in pixels */
  uint8_t *occ;                                            /* which 16 x 16 blocks hold anything; NULL until asked */
} rk_bitmap;
void rk__bitmap_changed(rk_bitmap *b);                     /* drawn into: the block map is out of date */
typedef struct rk_irect { int x0, y0, x1, y1; } rk_irect;
#define RK_DAMAGE_MAX 8                                    /* separate parts a frame draws and presents */

rk_bitmap *rk__bitmap_create(int w, int h);
void rk__view_moved(rk_view *v);                           /* only its place, size or opacity changed: its own layer holds */
void rk__bitmap_fit(rk_bitmap **slot, int w, int h);       /* reshapes in place while it fits; contents undefined */
void rk__bitmap_free(rk_bitmap *b);
void rk__bitmap_clear(rk_bitmap *b, uint32_t pm);
uint32_t rk__pm(rk_color c, float alpha);                  /* straight RGBA plus an extra alpha → premultiplied */
rk_irect rk__irect_meet(rk_irect a, rk_irect b);
static inline bool rk__irect_empty(rk_irect r) { return r.x1 <= r.x0 || r.y1 <= r.y0; }

void rk__fill_rect(rk_bitmap *b, rk_irect clip, float x0, float y0, float x1, float y1, uint32_t pm);   /* AA edges */

/* A path of closed polygons, filled with the nonzero rule (coverage clamped at 1): shapes, strokes, icons. */
typedef struct rk_path { float *pts; int npts, cap; int *ends; int nends, ecap; } rk_path;
void rk__path_reset(rk_path *p);
void rk__path_move(rk_path *p, float x, float y);
void rk__path_line(rk_path *p, float x, float y);
void rk__path_close(rk_path *p);
void rk__path_round_rect(rk_path *p, float x0, float y0, float x1, float y1, float r);
void rk__path_stroke(rk_path *p, const float *xy, int n, bool closed, float width, bool round_joins);
void rk__path_fill(rk_bitmap *b, rk_irect clip, const rk_path *p, uint32_t pm);
void rk__path_free(rk_path *p);

void rk__blit_mask(rk_bitmap *b, rk_irect clip, int x, int y, const uint8_t *mask, int w, int h, int stride, uint32_t pm);
/* Draws src mapped onto the quad (four device points: top left, top right, bottom right, bottom left), with
   anti-aliased edges and bilinear sampling. */
void rk__composite(rk_bitmap *dst, rk_irect clip, rk_bitmap *src, const float quad[8], float opacity);   /* src gets its block map */
double rk__wall_ms(void);
typedef void (*rk_rows_fn)(void *ctx, int y0, int y1);
void rk__parallel_rows(rk_rows_fn fn, void *ctx, int y0, int y1, long pixels);   /* bands of rows over the cores */
void rk__composite_at(rk_bitmap *dst, rk_irect clip, const rk_bitmap *src, int x, int y, float opacity);

bool rk__png_write(const rk_bitmap *b, const char *path);   /* png.c */

/* Text (text.c) --------------------------------------------------------------------------------------------------- */

typedef struct rk_textfmt {
  int weight; float size, line, tracking; bool tabular; rk_role color;
} rk_textfmt;
rk_textfmt rk__textfmt(rk_text_style style, float host_width);
float rk__text_width(const char *s, int len, const rk_textfmt *f);
float rk__text_ascent(const rk_textfmt *f);                /* from the top of the line box to the baseline */
typedef struct rk_line { int start, len; float width; } rk_line;
int rk__text_wrap(const char *s, const rk_textfmt *f, float max_width, rk_line *lines, int max);
void rk__text_draw(rk_canvas *c, const char *s, int len, float x, float top, const rk_textfmt *f, rk_color color);
void rk__text_shutdown(void);
void rk__text_forget(void);
/* fontfind.c: the system's fonts for a character Barlow lacks, and font files mapped into memory */
typedef struct rk_font_ref { char path[512]; int index; char psname[128]; } rk_font_ref;   /* psname picks a face in a collection */
int rk__font_candidates(int cp, int weight, rk_font_ref *out, int max);
const unsigned char *rk__file_map(const char *path, size_t *size);
void rk__file_unmap(const unsigned char *data, size_t size);
int rk__utf8_put(unsigned char *out, int cp);
int rk__text_next_break(const char *s, int from, int end);   /* where a line may end next after from (at most end) */

/* Canvas (canvas.c) ----------------------------------------------------------------------------------------------- */

struct rk_canvas {
  rk_bitmap *target;
  rk_irect clips[32]; int nclip;
  float ox, oy, scale;                                     /* device = o + logical × scale */
  float alpha;
  rk_path path;                                            /* scratch */
};
void rk__canvas_init(rk_canvas *c, rk_bitmap *target, float scale);
static inline rk_irect rk__canvas_clip(const rk_canvas *c) { return c->clips[c->nclip - 1]; }
bool rk__icon_find(const char *name, int *first, int *count);

/* Views (view.c) -------------------------------------------------------------------------------------------------- */

typedef struct rk_controller_impl rk_controller_impl;

/* What a view shows and how it behaves: the public rk_view_class plus internal hooks for the built-in views. */
typedef struct rk_class {
  rk_view_class pub;
  void *(*peek_target)(rk_view *view, void *state, rk_rect *area);   /* the part a peek moves (a tile's stat) */
  rk_controller *(*controller)(rk_view *view);
} rk_class;

struct rk_view {
  rk_object obj;
  rk_view *parent;
  rk_view **kids; int nkids, kcap;
  rk_window *window;                                       /* set on the root of a window's tree, found upwards */
  rk_controller *controller;                               /* the controller whose root view this is (weak) */
  char *name;
  /* model values (what was set) and presentation values (what is drawn, while animating) */
  rk_rect frame, pframe;
  float opacity, popacity;
  rk_transform transform, ptransform;
  rk_color fill[2], pfill;                                 /* light, dark; pfill resolved */
  rk_role fill_role;
  float radius;
  bool rule_top, rule_bottom, hidden, clip, focusable, leaving, entering, placed, fill_anim;
  bool passthrough;                                        /* a layer: only its children take the pointer */
  bool hug;                                                /* keeps its own width across a stack, as a button */
  bool draws_focus;                                        /* draws its own focus ring (a field: around its input) */
  uint8_t cursor;                                          /* the pointer's shape over it, as its last move left it */
  rk_view *menu;                                           /* a button opens it; any other view on a right click */
  /* layout */
  rk_layout layout; rk_insets padding; float gap; rk_align main, cross;
  float grow, fixed_w, fixed_h, min_w, min_h; int span_c, span_r;
  bool needs_layout;
  /* behaviour */
  rk_press press;
  void (*on_activate)(rk_view *view, void *user); void *activate_user;
  void *user; void (*free_user)(void *user);
  rk_access_role access; char *access_label, *access_value;
  const rk_class *cls; void *state;
  rk_class *own_cls;                                       /* a copy of a custom view's class */
  /* drawing */
  rk_bitmap *layer;                                        /* while turning or fading */
  bool layer_ok; uint32_t layer_gen;                       /* the layer still shows the content: drawn again only if not */
  float layer_fx, layer_fy;                                /* where in its first pixel the content sits in the layer */
  int anims;                                               /* animations running on it */
  float content_alpha;                                     /* while a size animates: the content fades back in */
  int transition; float transition_t; rk_bitmap *snapshot;  /* rk_transition + 1 while one runs */
  float value, pvalue;                                     /* a built-in view's animated number (progress, scroll) */
  float press_x, press_y; int pressed;                     /* -1..1 from the centre; 1 pressed, 2 letting go */
  double press_since;
  int index_hint;                                          /* build-in order */
};

extern const rk_type rk__view_type;
extern uint32_t rk__layer_gen;                             /* bumped when everything is drawn again */
extern int rk__live_views;                                 /* views alive: tests check nothing is left over */
rk_view *rk__view_create(const rk_class *cls, void *state);
void rk__view_dirty(rk_view *v);                           /* redraw its window */
void rk__view_relayout_up(rk_view *v);
rk_window *rk__view_window(rk_view *v);
float rk__host_width(rk_view *v);                          /* the width fluid type sizes follow */
rk_size rk__measure(rk_view *v, rk_size avail);
void rk__stack(rk_view *v, bool column);                    /* lay the children out as a column or row does */
void rk__layout(rk_view *v);
void rk__set_frame(rk_view *v, rk_rect f);                 /* animates inside a REFLOW, ENTER or LEAVE block */
rk_rect rk__window_rect(rk_view *v);                       /* the presented frame in window coordinates */
void rk__view_detach(rk_view *v);
void rk__view_insert(rk_view *parent, rk_view *child, int index);   /* retains child */
void rk__view_add(rk_view *parent, rk_view *child);                          /* leaves its parent at once */
rk_color rk__resolve_fill(rk_view *v);
bool rk__view_visible(rk_view *v);

/* Motion (anim.c) ------------------------------------------------------------------------------------------------- */

enum { RK_PROP_FRAME, RK_PROP_OPACITY, RK_PROP_TRANSFORM, RK_PROP_FILL, RK_PROP_CONTENT, RK_PROP_TRANSITION, RK_PROP_VALUE };

typedef struct rk_anim_ctx { rk_motion motion; int depth; void (*done)(void *); void *done_user; int pending; } rk_anim_ctx;
bool rk__animating_block(void);
rk_motion rk__current_motion(void);
void rk__animate(rk_view *v, int prop, const float *from, const float *to, int n, double duration, double delay, const float *ease);
void rk__animate_cb(rk_view *v, int prop, const float *from, const float *to, int n, double duration, double delay, const float *ease,
                    void (*done)(rk_view *v, void *user), void *user);
void rk__anim_cancel(rk_view *v, int prop);
void rk__animate_spring(rk_view *v, int prop, float target, double settle_ms);
/* A glide: a quintic from a value, speed and acceleration to a target at rest, picked up mid-way by a new target. */
typedef struct rk_glide { float p0, p1, v0, a0; double t0, dur; } rk_glide;
float rk__glide_at(const rk_glide *g, double now, float *vel, float *acc);   /* vel, acc may be NULL; per second */
void rk__glide_to(rk_glide *g, double now, float target, double dur_ms);
void rk__glide_set(rk_glide *g, float value);                                /* there at once */
typedef struct rk_tween { float from, to; double t0, dur; const float *ease; } rk_tween;   /* zeroed: at 0, still */
float rk__tween_at(const rk_tween *t, double now);
bool rk__tween_moving(const rk_tween *t, double now);
void rk__tween_to(rk_tween *t, double now, float to, double dur_ms, const float *ease);
bool rk__glide_moving(const rk_glide *g, double now);
bool rk__anim_tick(double now);                            /* true while any animation runs */
bool rk__anim_running(void);
void rk__anim_view_gone(rk_view *v);
double rk__motion_duration(rk_motion m, bool tile);
const float *rk__motion_ease(rk_motion m);

/* Windows (window.c) ---------------------------------------------------------------------------------------------- */

struct rk_window {
  rk_object obj;
  void *native;
  rk_view *root;                                           /* title bar, content, sheets, toasts */
  rk_view *titlebar, *content, *sheets, *toasts, *floats;   /* floats: menus, above everything */
  rk_controller *controller;
  rk_bitmap *fb;                                           /* what the last frame was drawn into */
  rk_bitmap *own_fb; bool fb_elsewhere;                    /* ours, where the platform gives no buffer */
  float scale; rk_size size;
  bool system_frame, shown, closed, needs_draw, needs_layout;
  rk_irect damage[RK_DAMAGE_MAX]; int ndamage; bool damage_all;   /* what the next frame draws, in device pixels */
  char *title;
  rk_view *focus, *hover, *pressed, *capture;
  rk_view *drag_over;                                      /* the view that took the files being dragged (weak) */
  rk_view *focus_back;                                     /* where focus was before it went into a toast */
  int cursor;                                              /* the shape the platform shows */
  rk_view *activated;                                      /* the view whose activate handler is running */
  bool focus_visible;                                      /* focus came from the keyboard */
  rk_point pointer;
  bool (*should_close)(rk_window *w, void *user); void *close_user;
  rk_window *next;                                         /* the app's list of open windows */
};

void rk__window_resized(rk_window *w, float width, float height, float scale);   /* from the platform */
void rk__window_event(rk_window *w, const rk_event *e);
bool rk__window_drag(rk_window *w, const rk_event *e);
int rk__uri_list_paths(const char *text, size_t n, char ***out);   /* text/uri-list: the local files' paths */
void rk__free_paths(char **paths, int n);       /* from the platform: whether a view takes the files */
int rk__accept_globs(const char *accept, char ***out);       /* the filter as "*.png" patterns; 0: any file */
bool rk__accepts(const char *accept, const char *path);    /* a file chooser's or a drop's filter: ".png,.jpg", "image/" with a star */
void rk__window_close_request(rk_window *w);
void rk__window_pointer_cancel(rk_window *w);
void rk__menus_close(rk_window *w);
void rk__background(void (*fn)(void *user), void *user);   /* pool.c: a slow job away from the UI thread */
void rk__undo_clear(void);
void rk__window_frame(rk_window *w, double now);           /* layout, draw and present if needed */
void rk__window_invalidate(rk_window *w);                 /* everything */
void rk__window_damage(rk_window *w, rk_rect r);          /* a part, in logical window coordinates */
void rk__render_view(rk_canvas *c, rk_view *v, float ox, float oy);
rk_bitmap *rk__snapshot(rk_view *v, bool with_fill);     /* the view as drawn now, at the window's scale */
void rk__layout_pending(void);                            /* lay out every window that needs it, now */
rk_window *rk__windows(void);
typedef struct rk_meter {                                 /* behind rk_app_stats */
  uint32_t frames, glyphs, slow; double draw_ms, last_ms, max_ms, pixels, latency_ms, max_latency_ms;
  double input_at;                                         /* an input event not yet shown, by the platform clock; 0 if none */
} rk_meter;
extern rk_meter rk__meter;
uint32_t rk__anim_count(void);

/* Controllers (controller.c) -------------------------------------------------------------------------------------- */

void rk__controller_appear(rk_controller *c, bool appear);
bool rk__controller_event(rk_controller *c, const rk_event *e);
void rk__controller_attach(rk_controller *c, rk_window *w);
bool rk__window_key(rk_window *w, const rk_event *e);     /* window-level keys: sheets, places back */
rk_view *rk__controller_key_view(rk_controller *c);       /* where keys go when nothing has focus */
void rk__reveal(rk_view *v);                               /* scroll it into view (keyboard focus) */
void rk__reveal_rect(rk_view *v, rk_rect r);               /* a part of it, in its own coordinates */
rk_view *rk__first_focusable(rk_view *v);                  /* itself or the first view in it that takes focus */
struct turn;                                               /* a place change, for views that turn as places do */
struct turn *rk__turn_new(void);
void rk__turn_free(struct turn *t);
double rk__turn_away(struct turn *t, rk_view *place, bool back);   /* its parts turn away; when the next may come */
void rk__turn_settle(struct turn *t);                      /* the parts that went stand as they were, for next time */
void rk__turn_arrive(rk_view *place, bool back);           /* its parts turn in */

/* Built-in views (widgets.c) -------------------------------------------------------------------------------------- */

rk_view *rk__titlebar_create(rk_window *w);
void rk__titlebar_update(rk_view *bar);
rk_view *rk__toast_create(const char *message, const char *action, void (*on_action)(void *), void *user);
bool rk__view_within(const rk_view *v, const rk_view *ancestor);
void rk__toast_leaving(rk_view *toast);
void rk__field_set_box_height(rk_view *field, float h);
/* menu.c, for the controls built on a menu */
void rk__menu_mark(rk_view *menu, int chosen);              /* a mark on the chosen item, none on the others */
void rk__menu_shape(rk_view *menu, float min_w, int prefer);
void rk__menu_clear(rk_view *menu);
void rk__menu_show_passive(rk_view *menu, rk_view *from, rk_rect below);   /* below: in the window */
bool rk__menu_open(rk_view *menu);
void rk__menu_move(rk_view *menu, int dir);
int rk__menu_active(rk_view *menu);
void rk__menu_choose(rk_view *menu, int item);                    /* focus inside it goes back to where it came from */
bool rk__is_tile(rk_view *v);
void rk__draw_focus(rk_canvas *c, rk_view *v, rk_rect r);

/* App and platform (app.c, platform_*.c) -------------------------------------------------------------------------- */

enum { RK__CURSOR_ARROW, RK__CURSOR_TEXT, RK__CURSOR_COL_RESIZE };   /* a view's cursor: what is under the pointer */

typedef struct rk_platform {
  const char *name;
  bool (*init)(const rk_app_desc *desc);
  void (*shutdown)(void);
  void *(*open)(rk_window *w, const rk_window_desc *desc);   /* calls rk__window_resized before returning */
  void (*close)(void *native);
  void (*show)(void *native);
  void (*set_title)(void *native, const char *title);
  void (*present)(void *native, const rk_bitmap *fb, const rk_irect *changed, int n);   /* only those parts are new */
  void (*move)(void *native);                              /* start dragging the window (title bar) */
  void (*minimize)(void *native);
  void (*zoom)(void *native);
  bool (*wait)(double timeout_ms);                         /* events until timeout (< 0: until one arrives) */
  void (*wake)(void);                                      /* any thread */
  double (*now)(void);
  void (*settings)(bool *dark, bool *reduced, bool *compact);
  void (*resize)(void *native, float width, float height);  /* logical px */
  bool (*ready)(void *native);                             /* NULL, or whether the compositor takes a frame now */
  double (*refresh_hz)(void);                              /* NULL, or the display's rate */
  rk_bitmap *(*target)(void *native, int w, int h, bool *fresh);   /* NULL, or the buffer to draw the next frame into */
  void (*clip_set)(const char *utf8);                      /* NULL, or the system clipboard takes the text */
  char *(*clip_get)(void);                                 /* NULL, or the system clipboard's text (malloc'd), or NULL */
  void (*text_input)(void *native, bool on, rk_rect caret);   /* NULL, or an input method follows a field's caret */
  void (*cursor)(void *native, int shape);                 /* NULL, or the pointer takes an RK__CURSOR_ shape */
  /* NULL, or the system's file chooser: false if it cannot open; else done() comes later, n 0 if it was cancelled */
  bool (*choose_files)(void *native, const char *accept, bool multiple, void (*done)(void *ctx, const char *const *files, int n), void *ctx);
} rk_platform;
extern const char *const *rk__headless_choice; extern int rk__headless_nchoice;   /* what the headless chooser answers */
bool rk__desktop_choose_files(void *native, const char *accept, bool multiple, void (*done)(void *, const char *const *, int), void *ctx);   /* Linux */

extern const rk_platform rk__platform_headless;
extern const rk_platform rk__platform_native;              /* the OS this was built for */
#if defined(RK_WAYLAND)
extern const rk_platform rk__platform_wayland;             /* Linux: tried before X11 */
#endif
const rk_platform *rk__platform(void);
bool rk__headless(void);
void rk__app_settings_changed(void);                       /* the platform saw a system setting change */
void rk__app_window_closed(rk_window *w);
void rk__app_request_frame(void);

#endif

/* window.c: a window's tree, drawing, input, focus and the press. The tree is: the title bar (unless the system
   frame is kept), the content (the root controller's view), the sheets and the toasts above it. */
#include "rk_internal.h"
#include "rk_tokens.h"

static rk_window *open_windows;

/* RK_PROFILE=1: the time spent drawing, printed when the app ends */
double rk__prof_ms; long rk__prof_frames, rk__prof_pixels;
rk_window *rk__windows(void) { return open_windows; }

/* The root view places the title bar, the content and the layers above it. */
static void root_layout(rk_view *v, void *state) {
  rk_window *w = state;
  float top = 0;
  if (w->titlebar) { top = 36; rk__set_frame(w->titlebar, (rk_rect){ 0, 0, v->frame.w, top }); }
  rk__set_frame(w->content, (rk_rect){ 0, top, v->frame.w, v->frame.h - top });
  rk__set_frame(w->sheets, (rk_rect){ 0, 0, v->frame.w, v->frame.h });
  rk__set_frame(w->toasts, (rk_rect){ 0, top, v->frame.w, v->frame.h - top });
}
static const rk_class root_class = { { sizeof(rk_view_class), "rk_window_root", NULL, NULL, root_layout, NULL, NULL }, NULL, NULL };

/* The toasts sit at the top, one at a time, up to 32rem wide. */
static void toasts_layout(rk_view *v, void *state) {
  (void)state;
  float m = rk_metric_of(RK_SPACE_M), w = RK_MIN(512.0f, v->frame.w - 2 * m);
  for (int i = 0; i < v->nkids; i++) {
    rk_view *t = v->kids[i];
    rk_size s = rk__measure(t, (rk_size){ w, INFINITY });
    rk__set_frame(t, (rk_rect){ (v->frame.w - w) / 2, m, w, s.h });
  }
}
static const rk_class toasts_class = { { sizeof(rk_view_class), "rk_toasts", NULL, NULL, toasts_layout, NULL, NULL }, NULL, NULL };

static void window_destroy(void *self) {
  rk_window *w = self;
  if (w->controller) { rk__controller_appear(w->controller, false); rk_release(w->controller); }
  if (w->root) { w->root->window = NULL; rk_release(w->root); }
  rk__bitmap_free(w->fb);
  free(w->title);
}
static const rk_type window_type = { "rk_window", window_destroy };

rk_window *rk_window_create(const rk_window_desc *desc) {
  rk_window_desc d = { sizeof d, "", 1280, 800, 0, 0, false };
  if (desc) memcpy(&d, desc, RK_MIN((size_t)desc->size, sizeof d));
  if (d.width <= 0) d.width = 1280;
  if (d.height <= 0) d.height = 800;
  if (d.min_width <= 0) d.min_width = rk_metric_of(RK_PANE_MIN);
  if (d.min_height <= 0) d.min_height = 320;
  rk_window *w = rk__new(&window_type, sizeof *w);
  w->title = rk__strdup(d.title ? d.title : "");
  w->system_frame = d.system_frame;
  w->scale = 1;
  w->size = (rk_size){ d.width, d.height };
  w->root = rk__view_create(&root_class, w);
  w->root->window = w;
  w->root->fill_role = RK_ROLE_BG;
  w->content = rk_view_create(); rk_view_set_layout(w->content, RK_LAYOUT_OVERLAY);
  if (!d.system_frame) { w->titlebar = rk__titlebar_create(w); rk_view_add(w->root, w->titlebar); rk_release(w->titlebar); }
  rk_view_add(w->root, w->content); rk_release(w->content);
  /* a toast says something happened; a sheet asks, above everything (the web's dialogs are in the top layer) */
  w->toasts = rk__view_create(&toasts_class, NULL); w->toasts->passthrough = true; rk_view_add(w->root, w->toasts); rk_release(w->toasts);
  w->sheets = rk_view_create(); w->sheets->passthrough = true; rk_view_add(w->root, w->sheets); rk_release(w->sheets);
  w->native = rk__platform()->open(w, &d);
  if (!w->native && !rk__headless()) { rk_release(w); return NULL; }
  w->next = open_windows; open_windows = w;
  rk_retain(w);                                            /* the app keeps an open window alive */
  w->needs_layout = w->needs_draw = true;
  return w;
}

void rk_window_set_root(rk_window *w, rk_controller *c) {
  if (w->controller == c) return;
  if (w->controller) {
    rk__controller_appear(w->controller, false);
    rk_view_remove(rk_controller_view(w->controller));
    rk_release(w->controller);
  }
  w->controller = c ? rk_retain(c) : NULL;
  if (c) {
    rk_view_add(w->content, rk_controller_view(c));
    rk__controller_attach(c, w);
    if (w->shown) rk__controller_appear(c, true);
  }
  if (w->titlebar) rk__titlebar_update(w->titlebar);
  rk__view_relayout_up(w->content);
}

rk_controller *rk_window_root(rk_window *w) { return w->controller; }

void rk_window_show(rk_window *w) {
  if (w->shown) return;
  w->shown = true;
  rk__platform()->show(w->native);
  if (w->controller) rk__controller_appear(w->controller, true);
  rk__window_invalidate(w);
}

void rk_window_close(rk_window *w) {
  if (w->closed) return;
  w->closed = true;
  rk__platform()->close(w->native);
  w->native = NULL;
  rk_window **p = &open_windows;
  while (*p && *p != w) p = &(*p)->next;
  if (*p) *p = w->next;
  rk__app_window_closed(w);
  rk_release(w);
}

void rk__window_close_request(rk_window *w) {
  if (w->should_close && !w->should_close(w, w->close_user)) return;
  rk_window_close(w);
}

void rk_window_set_title(rk_window *w, const char *title) {
  rk__setstr(&w->title, title ? title : "");
  rk__platform()->set_title(w->native, w->title);
  if (w->titlebar) rk__titlebar_update(w->titlebar);
}

rk_size rk_window_size(rk_window *w) { return w->size; }
float rk_window_scale(rk_window *w) { return w->scale; }
void rk_window_on_close(rk_window *w, bool (*fn)(rk_window *, void *), void *user) { w->should_close = fn; w->close_user = user; }

void rk__window_invalidate(rk_window *w) {
  w->damage_all = true;
  if (!w->needs_draw) { w->needs_draw = true; rk__app_request_frame(); }
}

void rk__window_damage(rk_window *w, rk_rect r) {
  float k = w->scale;
  rk_irect d = { (int)floorf(r.x * k) - 2, (int)floorf(r.y * k) - 2, (int)ceilf((r.x + r.w) * k) + 2, (int)ceilf((r.y + r.h) * k) + 2 };
  if (rk__irect_empty(w->damage)) w->damage = d;
  else w->damage = (rk_irect){ RK_MIN(w->damage.x0, d.x0), RK_MIN(w->damage.y0, d.y0), RK_MAX(w->damage.x1, d.x1), RK_MAX(w->damage.y1, d.y1) };
  if (!w->needs_draw) { w->needs_draw = true; rk__app_request_frame(); }
}

void rk__window_resized(rk_window *w, float width, float height, float scale) {
  if (width == w->size.w && height == w->size.h && scale == w->scale && w->fb) return;
  w->size = (rk_size){ width, height };
  w->scale = scale > 0 ? scale : 1;
  w->needs_layout = true;
  rk__view_relayout_up(w->root);
  rk__window_invalidate(w);
}

void rk_window_toast(rk_window *w, const char *message, const char *action, void (*on_action)(void *), void *user) {
  for (int i = w->toasts->nkids - 1; i >= 0; i--) {             /* one at a time: the old one leaves */
    rk_animate_begin(RK_MOTION_LEAVE); rk_view_remove(w->toasts->kids[i]); rk_animate_commit();
  }
  rk_view *t = rk__toast_create(message, action, on_action, user);
  rk_animate_begin(RK_MOTION_ENTER);
  rk_view_add(w->toasts, t);
  rk_animate_commit();
  rk_release(t);
}

/* Layout ---------------------------------------------------------------------------------------------------------- */

static void layout_window(rk_window *w) {
  rk_rect f = { 0, 0, w->size.w, w->size.h };
  rk__set_frame(w->root, f);
  rk__layout(w->root);
  w->needs_layout = false;
}

void rk__layout_pending(void) {
  for (rk_window *w = open_windows; w; w = w->next) if (w->needs_layout) layout_window(w);
}

/* Drawing --------------------------------------------------------------------------------------------------------- */

/* A corner of the view through its transform, as CSS: perspective(p) rotateX rotateY rotateZ scale, around the
   origin; returns logical px relative to the view's top left. */
static void project(const rk_transform *t, float w, float h, float x, float y, float *ox, float *oy) {
  float cx = t->origin_x * w, cy = t->origin_y * h;
  float px = (x - cx) * t->scale_x, py = (y - cy) * t->scale_y, pz = 0;
  float az = t->rotate_z * (float)M_PI / 180, ay = t->rotate_y * (float)M_PI / 180, ax = t->rotate_x * (float)M_PI / 180;
  if (az != 0) { float nx = px * cosf(az) - py * sinf(az), ny = px * sinf(az) + py * cosf(az); px = nx; py = ny; }
  if (ay != 0) { float nx = px * cosf(ay) + pz * sinf(ay), nz = -px * sinf(ay) + pz * cosf(ay); px = nx; pz = nz; }
  if (ax != 0) { float ny = py * cosf(ax) - pz * sinf(ax), nz = py * sinf(ax) + pz * cosf(ax); py = ny; pz = nz; }
  float d = t->perspective > 0 ? t->perspective : RK_TOK_TURNSTILE_PERSPECTIVE;
  float f = d / RK_MAX(d - pz, 1.0f);
  *ox = cx + px * f; *oy = cy + py * f;
}

static bool deep(const rk_transform *t) {
  return t->rotate_x != 0 || t->rotate_y != 0 || t->rotate_z != 0 || fabsf(t->scale_x - 1) > 1e-4f || fabsf(t->scale_y - 1) > 1e-4f;
}

static void draw_content(rk_canvas *c, rk_view *v, float x, float y, bool fill);

static void composite_layer(rk_canvas *c, const rk_bitmap *layer, rk_view *v, const rk_transform *t, float x, float y, float opacity) {
  float k = c->scale, q[8];
  float w = layer->w / k, h = layer->h / k;
  float cx[4] = { 0, w, w, 0 }, cy[4] = { 0, 0, h, h };
  for (int i = 0; i < 4; i++) {
    float px, py;
    project(t, v->pframe.w, v->pframe.h, cx[i], cy[i], &px, &py);
    q[i * 2] = x + px * k; q[i * 2 + 1] = y + py * k;
  }
  rk__composite(c->target, rk__canvas_clip(c), layer, q, opacity);
}

static rk_bitmap *ensure_layer(rk_bitmap **slot, int w, int h) {
  if (!*slot || (*slot)->w != w || (*slot)->h != h) { rk__bitmap_free(*slot); *slot = rk__bitmap_create(w, h); }
  else memset((*slot)->px, 0, (size_t)(*slot)->stride * (*slot)->h * 4);
  return *slot;
}

void rk__render_view(rk_canvas *c, rk_view *v, float pox, float poy) {
  if (v->hidden || v->popacity <= 0.002f || c->alpha <= 0.002f) return;
  float k = c->scale;
  rk_transform t = v->ptransform;
  float x = pox + (v->pframe.x + t.translate_x) * k, y = poy + (v->pframe.y + t.translate_y) * k;
  bool flip = v->transition == RK_TRANSITION_FLIP + 1 && v->snapshot, turned = deep(&t);
  /* outside what this frame draws: skip it (a container that does not clip may still have children reaching in) */
  float pad = turned ? RK_MAX(v->pframe.w, v->pframe.h) * 0.35f * k + 8 : 2;
  rk_irect vb = { (int)floorf(x - pad), (int)floorf(y - pad), (int)ceilf(x + v->pframe.w * k + pad), (int)ceilf(y + v->pframe.h * k + pad) };
  if (rk__irect_empty(rk__irect_meet(vb, rk__canvas_clip(c))) && (v->clip || !v->nkids || v->cls)) return;
  if (!turned && v->popacity >= 0.999f && !flip) { draw_content(c, v, x, y, true); return; }
  float opacity = v->popacity * c->alpha;
  /* a fill with nothing on it (the shading behind a sheet) fades by itself: no layer */
  if (!turned && !flip && !v->nkids && !v->cls) {
    float sa = c->alpha;
    c->alpha = opacity;
    draw_content(c, v, x, y, true);
    c->alpha = sa;
    return;
  }
  int lw = (int)ceilf(v->pframe.w * k), lh = (int)ceilf(v->pframe.h * k);
  if (lw <= 0 || lh <= 0) return;
  if (flip) {
    /* the old look turns away around the horizontal axis; at the half the new one turns in */
    float tt = v->transition_t;
    rk_transform ft = t;
    ft.translate_x = ft.translate_y = 0;
    if (tt < 0.5f) {
      ft.rotate_x += -90 * rk__ease(rk__ease_out, tt / 0.5f);
      composite_layer(c, v->snapshot, v, &ft, x, y, opacity);
      return;
    }
    ft.rotate_x += 90 * (1 - rk__ease(rk__ease_in, (tt - 0.5f) / 0.5f));
    t = ft;
  }
  rk_bitmap *layer = ensure_layer(&v->layer, lw, lh);
  rk_canvas sub;
  memset(&sub, 0, sizeof sub);
  rk__canvas_init(&sub, layer, k);
  draw_content(&sub, v, 0, 0, true);
  rk__path_free(&sub.path);
  if (!turned && !flip) rk__composite_at(c->target, rk__canvas_clip(c), layer, (int)lroundf(x), (int)lroundf(y), opacity);   /* a plain fade */
  else composite_layer(c, layer, v, &t, x, y, opacity);
  if (!v->anims && !v->transition) { rk__bitmap_free(v->layer); v->layer = NULL; }
}

static void draw_content(rk_canvas *c, rk_view *v, float x, float y, bool fill) {
  float k = c->scale, sox = c->ox, soy = c->oy, salpha = c->alpha;
  c->ox = x; c->oy = y;
  rk_rect b = { 0, 0, v->pframe.w, v->pframe.h };
  rk_color fc = v->fill_anim ? v->pfill : (v->window ? 0 : rk__resolve_fill(v));   /* roles follow the theme */
  if (fill && (fc & 255)) rk_canvas_fill_round_rect(c, b, v->radius, fc);
  rk_color rule = rk_color_of(RK_ROLE_TEXT_LOW);
  if (v->rule_top) rk_canvas_fill_rect(c, (rk_rect){ 0, 0, b.w, 1 }, rule);
  if (v->rule_bottom) rk_canvas_fill_rect(c, (rk_rect){ 0, b.h - 1, b.w, 1 }, rule);
  bool peek = v->transition == RK_TRANSITION_PEEK + 1 && v->snapshot && !(v->cls && v->cls->peek_target);
  bool clip = v->clip || peek;
  if (clip) rk_canvas_push_clip(c, b);
  c->alpha *= v->content_alpha;
  float dy = 0;
  if (peek) {
    /* the old content leaves upwards as the new one rises from below */
    float tt = v->transition_t, e0 = rk__ease(rk__ease_out, RK_MIN(1.0f, tt / 0.6f)), e1 = rk__ease(rk__ease_in, RK_CLAMP((tt - 0.2f) / 0.8f, 0.0f, 1.0f));
    float lift = -0.4f * b.h * e0;
    rk__composite_at(c->target, rk__canvas_clip(c), v->snapshot, (int)floorf(x), (int)floorf(y + lift * k), (1 - e0) * c->alpha);
    dy = 0.4f * b.h * (1 - e1);
    c->alpha *= e1;
  }
  c->oy = y + dy * k;
  if (v->cls && v->cls->pub.draw) v->cls->pub.draw(v, c, v->state);
  for (int i = 0; i < v->nkids; i++) { rk__render_view(c, v->kids[i], x, y + dy * k); c->ox = x; c->oy = y + dy * k; }
  c->alpha = salpha;
  if (v->transition == RK_TRANSITION_CROSSFADE + 1 && v->snapshot)
    rk__composite_at(c->target, rk__canvas_clip(c), v->snapshot, (int)floorf(x), (int)floorf(y), (1 - rk__ease(rk__ease_base, v->transition_t)) * c->alpha);
  if (clip) rk_canvas_pop_clip(c);
  c->ox = sox; c->oy = soy;
}

rk_bitmap *rk__snapshot(rk_view *v, bool with_fill) {
  rk_window *w = rk__view_window(v);
  float k = w ? w->scale : 1;
  int lw = (int)ceilf(v->pframe.w * k), lh = (int)ceilf(v->pframe.h * k);
  if (lw <= 0 || lh <= 0) return NULL;
  rk_bitmap *b = rk__bitmap_create(lw, lh);
  if (v->window) rk__bitmap_clear(b, rk__pm(rk_color_of(RK_ROLE_BG), 1));   /* the window's ground, for a cross-fade */
  rk_canvas c;
  memset(&c, 0, sizeof c);
  rk__canvas_init(&c, b, k);
  int tr = v->transition; v->transition = 0;
  draw_content(&c, v, 0, 0, with_fill);
  v->transition = tr;
  rk__path_free(&c.path);
  return b;
}

void rk__window_frame(rk_window *w, double now) {
  (void)now;
  if (w->closed) return;
  if (w->needs_layout) { layout_window(w); w->damage_all = true; }
  if (!w->needs_draw) return;
  w->needs_draw = false;
  clock_t t0 = clock();
  int fw = (int)ceilf(w->size.w * w->scale), fh = (int)ceilf(w->size.h * w->scale);
  if (!w->fb || w->fb->w != fw || w->fb->h != fh) { rk__bitmap_fit(&w->fb, fw, fh); w->damage_all = true; }
  rk_irect all = { 0, 0, fw, fh }, clip = w->damage_all ? all : rk__irect_meet(w->damage, all);
  w->damage = (rk_irect){ 0, 0, 0, 0 }; w->damage_all = false;
  if (rk__irect_empty(clip)) return;
  /* only what changed: the ground under it, then whatever reaches into it */
  rk__fill_rect(w->fb, clip, (float)clip.x0, (float)clip.y0, (float)clip.x1, (float)clip.y1, rk__pm(rk_color_of(RK_ROLE_BG), 1));
  rk_canvas c;
  memset(&c, 0, sizeof c);
  rk__canvas_init(&c, w->fb, w->scale);
  c.clips[0] = clip;
  rk__render_view(&c, w->root, 0, 0);
  if (w->focus && w->focus_visible && rk__view_visible(w->focus)) {
    rk_rect r = rk__window_rect(w->focus);
    c.ox = c.oy = 0; c.alpha = 1;
    rk__draw_focus(&c, w->focus, r);
  }
  rk__path_free(&c.path);
  rk__platform()->present(w->native, w->fb, clip);
  rk__prof_ms += (double)(clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
  rk__prof_frames++;
  rk__prof_pixels += (long)(clip.x1 - clip.x0) * (clip.y1 - clip.y0);
}

/* Input ----------------------------------------------------------------------------------------------------------- */

static rk_view *hit(rk_view *v, float x, float y) {
  if (v->hidden || v->leaving || v->popacity <= 0.01f) return NULL;
  float lx = x - v->pframe.x - v->ptransform.translate_x, ly = y - v->pframe.y - v->ptransform.translate_y;
  bool inside = lx >= 0 && ly >= 0 && lx < v->pframe.w && ly < v->pframe.h;
  if (v->clip && !inside) return NULL;
  for (int i = v->nkids - 1; i >= 0; i--) { rk_view *h = hit(v->kids[i], lx, ly); if (h) return h; }
  /* a plain container with nothing of its own lets the pointer through to what lies under it */
  bool own = v->cls || (rk__resolve_fill(v) & 255) || v->on_activate || v->focusable || v->controller || v->rule_top || v->rule_bottom;
  return inside && !v->window && !v->passthrough && own ? v : NULL;
}

static rk_view *hit_window(rk_window *w, float x, float y) {
  /* a sheet takes everything while it is up */
  if (w->sheets->nkids) { rk_view *h = hit(w->sheets, x, y); return h ? h : w->sheets; }
  rk_view *h = hit(w->root, x, y);
  return h ? h : w->root;
}

static bool dispatch(rk_window *w, rk_view *from, const rk_event *e) {
  rk_event local = *e;
  for (rk_view *v = from; v; v = v->parent) {
    if (v->cls && v->cls->pub.event) {
      rk_rect r = rk__window_rect(v);
      local.position = (rk_point){ e->position.x - r.x, e->position.y - r.y };
      if (v->cls->pub.event(v, &local, v->state)) return true;
    }
    if (v->controller && rk__controller_event(v->controller, e)) return true;
  }
  return rk__window_key(w, e);
}

static rk_view *pressable(rk_view *v) {
  for (; v; v = v->parent) if (v->press != RK_PRESS_NONE || v->on_activate) return v;
  return NULL;
}

static rk_view *focusable_up(rk_view *v) {
  for (; v; v = v->parent) if (v->focusable) return v;
  return NULL;
}

/* The press: tiles and rows tilt toward where they were pressed and sink; it lands over duration-press. */
static rk_transform press_pose(rk_view *v) {
  rk_transform t = v->transform;
  bool tile = v->press == RK_PRESS_TILE;
  float tilt = tile ? RK_TOK_TILE_TILT : v->press == RK_PRESS_ROW ? RK_TOK_ROW_TILT : 0;
  float s = tile ? RK_TOK_TILE_PRESS_SCALE : v->press == RK_PRESS_ROW ? RK_TOK_ROW_PRESS_SCALE : 1.0f;   /* a button only darkens */
  if (!rk__reduced()) { t.rotate_x += v->press_y * -tilt; t.rotate_y += v->press_x * tilt; }
  t.scale_x *= s; t.scale_y *= s;
  t.origin_x = t.origin_y = 0.5f;
  return t;
}

static void press_start(rk_window *w, rk_view *v, float px, float py) {
  w->pressed = v;
  v->pressed = 1;
  v->press_x = RK_CLAMP(px, -1.0f, 1.0f); v->press_y = RK_CLAMP(py, -1.0f, 1.0f);
  v->press_since = rk_app_now();
  rk_transform from = v->ptransform, to = press_pose(v);
  rk__animate(v, RK_PROP_TRANSFORM, &from.translate_x, &to.translate_x, 10, rk_duration_of(RK_DURATION_PRESS), 0, rk__ease_base);
}

static void press_end(rk_window *w, rk_view *v) {
  if (w->pressed == v) w->pressed = NULL;
  v->pressed = 0;
  rk_transform from = v->ptransform, to = v->transform;
  /* a quick tap still shows the whole sink before it comes back */
  double left = rk_duration_of(RK_DURATION_PRESS) - (rk_app_now() - v->press_since);
  rk__animate(v, RK_PROP_TRANSFORM, &from.translate_x, &to.translate_x, 10, rk_duration_of(RK_DURATION_SHORT), RK_MAX(0.0, left), rk__ease_base);
}

static void activate(rk_view *v) {
  if (!v->on_activate) return;
  rk_window *w = rk__view_window(v);
  rk_view *was = w ? w->activated : NULL;
  if (w) w->activated = v;
  rk_retain(v);
  v->on_activate(v, v->activate_user);
  if (w && !w->closed) w->activated = was;
  rk_release(v);
}

typedef struct key_press { rk_window *w; rk_view *v; } key_press;
static void key_release(void *user) {
  key_press *kp = user;
  if (kp->v->parent || kp->v->window) { press_end(kp->w, kp->v); activate(kp->v); }
  rk_release(kp->v);
  free(kp);
}

static void collect_focus(rk_view *v, rk_view **list, int *n, int max) {
  if (v->hidden || v->leaving) return;
  if (v->focusable && *n < max) list[(*n)++] = v;
  for (int i = 0; i < v->nkids; i++) collect_focus(v->kids[i], list, n, max);
}

bool rk_window_focus(rk_window *w, rk_view *v) {
  if (v && !v->focusable) return false;
  if (w->focus == v) return true;
  rk_event e = { sizeof e, RK_EVENT_BLUR, rk_app_now(), { 0, 0 }, 0, 0, 0, 0, 0, NULL };
  if (w->focus) dispatch(w, w->focus, &e);
  w->focus = v;
  e.type = RK_EVENT_FOCUS;
  if (v) dispatch(w, v, &e);
  rk__window_invalidate(w);
  return true;
}

rk_view *rk_window_focused(rk_window *w) { return w->focus; }

static void focus_step(rk_window *w, int dir) {
  rk_view *list[1024];
  int n = 0;
  collect_focus(w->sheets->nkids ? w->sheets : w->root, list, &n, 1024);
  if (!n) return;
  int at = -1;
  for (int i = 0; i < n; i++) if (list[i] == w->focus) at = i;
  int next = at < 0 ? (dir > 0 ? 0 : n - 1) : (at + dir + n) % n;
  w->focus_visible = true;
  rk_window_focus(w, list[next]);
  rk__reveal(list[next]);
}

void rk__window_event(rk_window *w, const rk_event *e) {
  if (w->closed) return;
  switch (e->type) {
    case RK_EVENT_POINTER_MOVE: {
      w->pointer = e->position;
      rk_view *h = w->capture ? w->capture : hit_window(w, e->position.x, e->position.y);
      w->hover = h;                                            /* nothing is drawn differently under the pointer */
      dispatch(w, h, e);
      break;
    }
    case RK_EVENT_POINTER_DOWN: {
      w->pointer = e->position;
      rk_view *h = hit_window(w, e->position.x, e->position.y);
      w->focus_visible = false;
      rk_view *f = focusable_up(h);
      if (f) rk_window_focus(w, f);
      rk_view *p = e->button == 0 ? pressable(h) : NULL;
      if (p) {
        rk_rect r = rk__window_rect(p);
        press_start(w, p, (e->position.x - r.x) / (r.w / 2) - 1, (e->position.y - r.y) / (r.h / 2) - 1);
      }
      w->capture = h;
      dispatch(w, h, e);
      break;
    }
    case RK_EVENT_POINTER_UP: {
      rk_view *h = w->capture ? w->capture : hit_window(w, e->position.x, e->position.y);
      w->capture = NULL;
      rk_view *p = w->pressed;
      if (p) {
        /* the hit area reaches space-m past the resting box while pressed: the sink pulls the edge off the pointer */
        rk_rect r = rk__window_rect(p);
        float m = p->press == RK_PRESS_TILE ? rk_metric_of(RK_SPACE_M) : 0;
        bool inside = e->position.x >= r.x - m && e->position.y >= r.y - m && e->position.x < r.x + r.w + m && e->position.y < r.y + r.h + m;
        press_end(w, p);
        if (inside) activate(p);
      }
      dispatch(w, h, e);
      break;
    }
    case RK_EVENT_KEY_DOWN: {
      if (e->key == RK_KEY_TAB && !(e->mods & (RK_MOD_CTRL | RK_MOD_ALT | RK_MOD_SUPER))) { focus_step(w, e->mods & RK_MOD_SHIFT ? -1 : 1); break; }
      rk_view *f = w->focus;
      if (f && (e->key == RK_KEY_ENTER || e->key == RK_KEY_SPACE) && pressable(f) == f && !f->pressed) {
        w->focus_visible = true;
        press_start(w, f, 0, 0);
        key_press *kp = rk__alloc(sizeof *kp);
        kp->w = w; kp->v = rk_retain(f);
        rk_app_after(rk_duration_of(RK_DURATION_PRESS), false, key_release, kp);
        break;
      }
      rk_view *to = f;
      if (!to && w->sheets->nkids) to = w->sheets->kids[w->sheets->nkids - 1];
      if (!to && w->controller) to = rk__controller_key_view(w->controller);
      dispatch(w, to ? to : w->root, e);
      break;
    }
    case RK_EVENT_WHEEL: dispatch(w, hit_window(w, e->position.x, e->position.y), e); break;
    default: dispatch(w, w->focus ? w->focus : w->root, e); break;
  }
}

void rk_window_inject(rk_window *w, const rk_event *e) {
  rk_event local;
  memset(&local, 0, sizeof local);
  memcpy(&local, e, RK_MIN((size_t)e->size, sizeof local));
  local.size = sizeof local;
  if (!local.time) local.time = rk_app_now();
  rk__window_event(w, &local);
}

bool rk_window_snapshot(rk_window *w, const char *path) {
  rk__layout_pending();
  w->needs_draw = true; w->damage_all = true;
  rk__window_frame(w, rk_app_now());
  return w->fb && rk__png_write(w->fb, path);
}

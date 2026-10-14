/* window.c: a window's tree, drawing, input, focus and the press. The tree is: the title bar (unless the system
   frame is kept), the content (the root controller's view), the sheets and the toasts above it. */
#include "rk_internal.h"
#include "rk_tokens.h"

static rk_window *open_windows;

rk_meter rk__meter;
rk_window *rk__windows(void) { return open_windows; }

/* The root view places the title bar, the content and the layers above it. */
static void root_layout(rk_view *v, void *state) {
  rk_window *w = state;
  float top = 0;
  if (w->titlebar) { top = 36; rk__set_frame(w->titlebar, (rk_rect){ 0, 0, v->frame.w, top }); }
  rk__set_frame(w->content, (rk_rect){ 0, top, v->frame.w, v->frame.h - top });
  rk__set_frame(w->sheets, (rk_rect){ 0, 0, v->frame.w, v->frame.h });
  rk__set_frame(w->toasts, (rk_rect){ 0, top, v->frame.w, v->frame.h - top });
  rk__set_frame(w->floats, (rk_rect){ 0, 0, v->frame.w, v->frame.h });
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
  rk__tip_hide(w, false);
  rk__history_free(w);
  if (w->controller) { rk__controller_appear(w->controller, false); rk_release(w->controller); }
  if (w->root) { w->root->window = NULL; rk_release(w->root); }
  rk__bitmap_free(w->own_fb);
  free(w->title);
}
static const rk_type window_type = { "rk_window", window_destroy };

rk_window *rk_window_create(const rk_window_desc *desc) {
  rk_window_desc d = { sizeof d, "", 1280, 800, 0, 0, false, NULL, NULL, NULL };
  if (desc && RK__SIZED(desc, "rk_window_create", "rk_window_desc")) memcpy(&d, desc, RK_MIN((size_t)desc->size, sizeof d));
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
  if (!d.system_frame) { w->titlebar = rk__titlebar_create(w); rk__view_add(w->root, w->titlebar); rk_release(w->titlebar); }
  rk__view_add(w->root, w->content); rk_release(w->content);
  /* a toast says something happened; a sheet asks, above everything (the web's dialogs are in the top layer) */
  w->toasts = rk__view_create(&toasts_class, NULL); w->toasts->passthrough = true; rk__view_add(w->root, w->toasts); rk_release(w->toasts);
  w->sheets = rk__view_create(&rk__sheets_class, NULL); w->sheets->passthrough = true; rk__view_add(w->root, w->sheets); rk_release(w->sheets);
  w->floats = rk_view_create(); w->floats->passthrough = true; rk__view_add(w->root, w->floats); rk_release(w->floats);   /* menus, on top */
  w->native = rk__platform()->open(w, &d);
  if (!w->native && !rk__headless()) { rk_release(w); return NULL; }
  w->next = open_windows; open_windows = w;              /* the app holds an open window: the caller's pointer is borrowed */
  w->needs_layout = w->needs_draw = true;
  if (d.build) rk__ui_window(w, &d);         /* declared: it shows what the build says */
  return w;
}

static void set_root(rk_window *w, rk_controller *c) {
  if (w->controller == c) return;
  if (w->controller) {
    rk__controller_appear(w->controller, false);
    rk_view_remove(rk_controller_view(w->controller));
    rk_release(w->controller);
  }
  w->controller = c ? rk_retain(c) : NULL;
  if (c) {
    rk__view_add(w->content, rk_controller_view(c));
    rk__controller_attach(c, w);
    if (w->shown) rk__controller_appear(c, true);
  }
  if (w->titlebar) rk__titlebar_update(w->titlebar);
  rk__view_relayout_up(w->content);
}

rk_controller *rk_window_root(rk_window *w) { return w->controller; }

/* Shows the window with root as what it shows (takes the caller's reference); a later call puts another root in. The
   root it already shows is left as it is: that pointer was borrowed. */
void rk_window_show(rk_window *w, rk_controller *root) {
  if (root && root != w->controller) { set_root(w, root); rk_release(root); }
  if (w->shown) return;
  w->shown = true;
  rk__platform()->show(w->native);
  if (w->controller) rk__controller_appear(w->controller, true);
  rk__window_invalidate(w);
}

void rk_window_close(rk_window *w) {
  if (w->closed) return;
  w->closed = true;
  rk__tip_hide(w, false);
  for (int i = w->floats->nkids - 1; i >= 0; i--)          /* an open menu holds its opener, which holds it: let go */
    if (i < w->floats->nkids && rk__is_menu(w->floats->kids[i])) rk_menu_close(w->floats->kids[i]);
  rk__platform()->close(w->native);
  w->native = NULL;
  rk_window **p = &open_windows;
  while (*p && *p != w) p = &(*p)->next;
  if (*p) *p = w->next;
  rk__app_window_closed(w);
  rk_release(w);
}

void rk__window_close_request(rk_window *w) {
  bool closing = !w->should_close || w->should_close(w, w->close_user);
  if (w->should_close) rk__ui_dirty();                     /* it may have asked (unsaved changes?) */
  if (closing) rk_window_close(w);
  rk__ui_settle();
}

void rk_window_set_title(rk_window *w, const char *title) {
  rk__setstr(&w->title, title ? title : "");
  rk__platform()->set_title(w->native, w->title);
  if (w->titlebar) rk__titlebar_update(w->titlebar);
}

rk_size rk_window_size(rk_window *w) { return w->size; }
void rk_window_set_size(rk_window *w, float width, float height) { if (!w->closed) rk__platform()->resize(w->native, RK_MAX(width, 1.0f), RK_MAX(height, 1.0f)); }
float rk_window_scale(rk_window *w) { return w->scale; }
void rk_window_on_close(rk_window *w, bool (*fn)(rk_window *, void *), void *user) { w->should_close = fn; w->close_user = user; }

uint32_t rk__layer_gen;

void rk__window_invalidate(rk_window *w) {
  w->damage_all = true;
  rk__layer_gen++;                                               /* the theme, the size: layers are drawn again */
  if (!w->needs_draw) { w->needs_draw = true; rk__app_request_frame(); }
}

static long irect_area(rk_irect r) { return rk__irect_empty(r) ? 0 : (long)(r.x1 - r.x0) * (r.y1 - r.y0); }
static rk_irect irect_join(rk_irect a, rk_irect b);

/* Damage is a few separate parts: a change far from another is drawn on its own instead of the box around both. A
   part joins another when the box around them wastes little, and parts that can no longer be kept apart are joined
   where that wastes least. */
static void damage_add(rk_window *w, rk_irect d) {
  for (bool merged = true; merged;) {
    merged = false;
    for (int i = 0; i < w->ndamage; i++) {
      rk_irect u = irect_join(w->damage[i], d);
      long sum = irect_area(w->damage[i]) + irect_area(d);
      if (irect_area(u) <= sum + sum / 4 + 64 * 64) {
        d = u;
        w->damage[i] = w->damage[--w->ndamage];
        merged = true;
        break;
      }
    }
  }
  if (w->ndamage < RK_DAMAGE_MAX) { w->damage[w->ndamage++] = d; return; }
  int best = 0;
  long cost = -1;
  for (int i = 0; i < w->ndamage; i++) {
    long c = irect_area(irect_join(w->damage[i], d)) - irect_area(w->damage[i]);
    if (cost < 0 || c < cost) { cost = c; best = i; }
  }
  w->damage[best] = irect_join(w->damage[best], d);
}

void rk__window_damage(rk_window *w, rk_rect r) {
  float k = w->scale;
  rk_irect d = { (int)floorf(r.x * k) - 2, (int)floorf(r.y * k) - 2, (int)ceilf((r.x + r.w) * k) + 2, (int)ceilf((r.y + r.h) * k) + 2 };
  if (!rk__irect_empty(d)) damage_add(w, d);
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

void rk__toast_leaving(rk_view *t) {
  rk_window *w = rk__view_window(t);
  if (!w || !rk__view_within(w->focus, t)) return;
  rk_view *back = w->focus_back;
  w->focus_back = NULL;
  rk_window_focus(w, back && rk__view_window(back) == w && !back->leaving ? back : NULL);
}

void rk_window_toast(rk_window *w, const char *message, const char *action, void (*on_action)(void *), void *user) {
  for (int i = w->toasts->nkids - 1; i >= 0; i--) {             /* one at a time: the old one leaves */
    rk__toast_leaving(w->toasts->kids[i]);
    rk_animate_begin(RK_MOTION_LEAVE); rk_view_remove(w->toasts->kids[i]); rk_animate_commit();
  }
  rk_view *t = rk__toast_create(message, action, on_action, user);
  rk_animate_begin(RK_MOTION_ENTER);
  rk__view_add(w->toasts, t);
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

/* A transform that moves no corner of the view by a quarter of a device pixel looks flat: the view is drawn sharp, as
   at rest, rather than resampled through a layer. A turn's last frames would otherwise end soft and then sharpen a
   moment after the motion stopped. */
static bool seen_deep(const rk_transform *t, float w, float h, float k) {
  if (!deep(t)) return false;
  float cx[4] = { 0, w, w, 0 }, cy[4] = { 0, 0, h, h };
  for (int i = 0; i < 4; i++) {
    float px, py;
    project(t, w, h, cx[i], cy[i], &px, &py);
    if (fabsf(px - cx[i]) * k > 0.25f || fabsf(py - cy[i]) * k > 0.25f) return true;
  }
  return false;
}

static void draw_content(rk_canvas *c, rk_view *v, float x, float y, bool fill);

/* The layer holds the view's content drawn ox, oy device px into it (where the view starts within its first pixel),
   so at rest the layer lands on whole pixels and copies exactly what drawing the view directly would give. */
static void composite_layer(rk_canvas *c, rk_bitmap *layer, rk_view *v, const rk_transform *t, float x, float y, float ox, float oy, float opacity) {
  float k = c->scale, q[8];
  float w = layer->w / k, h = layer->h / k, lx = -ox / k, ly = -oy / k;
  float cx[4] = { lx, lx + w, lx + w, lx }, cy[4] = { ly, ly, ly + h, ly + h };
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
  bool flip = v->transition == RK_TRANSITION_FLIP + 1 && v->snapshot, turned = seen_deep(&t, v->pframe.w, v->pframe.h, k);
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
  float fx = flip ? 0 : x - floorf(x), fy = flip ? 0 : y - floorf(y);
  int lw = (int)ceilf(v->pframe.w * k + fx), lh = (int)ceilf(v->pframe.h * k + fy);
  if (lw <= 0 || lh <= 0) return;
  if (flip) {
    /* the old look turns away around the horizontal axis; at the half the new one turns in */
    float tt = v->transition_t;
    rk_transform ft = t;
    ft.translate_x = ft.translate_y = 0;
    if (tt < 0.5f) {
      ft.rotate_x += -90 * rk__ease(rk__ease_out, tt / 0.5f);
      composite_layer(c, v->snapshot, v, &ft, x, y, 0, 0, opacity);
      return;
    }
    ft.rotate_x += 90 * (1 - rk__ease(rk__ease_in, (tt - 0.5f) / 0.5f));
    t = ft;
  }
  /* the layer is drawn again only when what is in it changed: a page that turns is drawn once */
  rk_bitmap *layer = v->layer;
  if (!layer || layer->w != lw || layer->h != lh || !v->layer_ok || v->layer_gen != rk__layer_gen || fabsf(v->layer_fx - fx) > 0.01f || fabsf(v->layer_fy - fy) > 0.01f) {
    layer = ensure_layer(&v->layer, lw, lh);
    rk_canvas sub;
    memset(&sub, 0, sizeof sub);
    rk__canvas_init(&sub, layer, k);
    draw_content(&sub, v, fx, fy, true);
    v->layer_fx = fx; v->layer_fy = fy;
    rk__path_free(&sub.path);
    rk__bitmap_changed(layer);
    v->layer_ok = true; v->layer_gen = rk__layer_gen;
  }
  if (!turned && !flip) rk__composite_at(c->target, rk__canvas_clip(c), layer, (int)floorf(x), (int)floorf(y), opacity);   /* a plain fade */
  else composite_layer(c, layer, v, &t, x, y, fx, fy, opacity);
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

static rk_irect irect_join(rk_irect a, rk_irect b) {
  if (rk__irect_empty(a)) return b;
  if (rk__irect_empty(b)) return a;
  return (rk_irect){ RK_MIN(a.x0, b.x0), RK_MIN(a.y0, b.y0), RK_MAX(a.x1, b.x1), RK_MAX(a.y1, b.y1) };
}

static int env_on(const char *name) { const char *v = getenv(name); return v && *v && *v != '0'; }

/* RK_HUD=1: what the loop does, in the window's bottom left corner, redrawn with every frame */
static rk_rect hud_rect(rk_window *w) { return (rk_rect){ 8, w->size.h - 8 - 66, 260, 66 }; }

static void draw_hud(rk_canvas *c, rk_window *w) {
  rk_rect r = hud_rect(w);
  char line[3][128];
  const rk_meter *m = &rk__meter;
  snprintf(line[0], sizeof line[0], "%s %.2gx  %.0f x %.0f", rk__platform()->name, w->scale, w->size.w, w->size.h);
  snprintf(line[1], sizeof line[1], "draw %.1f ms  max %.1f  slow %u / %u", m->last_ms, m->max_ms, m->slow, m->frames);
  snprintf(line[2], sizeof line[2], "input %.0f ms  max %.0f  anim %u  glyphs %u", m->latency_ms, m->max_latency_ms, rk__anim_count(), m->glyphs);
  c->ox = c->oy = 0; c->alpha = 1;
  rk_canvas_fill_rect(c, r, 0x000000d0);
  for (int i = 0; i < 3; i++) rk_canvas_text(c, line[i], (rk_point){ r.x + 8, r.y + 6 + i * 19.0f }, RK_TEXT_META, 0xffffffff);
}


void rk__window_frame(rk_window *w, double now) {
  static int hud = -1, trace = -1;
  if (hud < 0) { hud = env_on("RK_HUD"); trace = env_on("RK_TRACE"); }
  if (w->closed) return;
  if (w->needs_layout) layout_window(w);                     /* what it moved, it damaged (rk__set_frame) */
  if (!w->needs_draw) return;
  w->needs_draw = false;
  double t0 = rk__wall_ms();
  int fw = (int)ceilf(w->size.w * w->scale), fh = (int)ceilf(w->size.h * w->scale);
  /* into the platform's buffer where it gives one (Wayland: no copy of the frame), else into our own */
  bool fresh = false;
  rk_bitmap *target = rk__platform()->target ? rk__platform()->target(w->native, fw, fh, &fresh) : NULL;
  if (target) {
    if (fresh) w->damage_all = true;
    w->fb = target; w->fb_elsewhere = true;
  } else {
    if (!w->own_fb || w->own_fb->w != fw || w->own_fb->h != fh) { rk__bitmap_fit(&w->own_fb, fw, fh); w->damage_all = true; }
    if (w->fb_elsewhere) w->damage_all = true;                   /* ours missed the frames drawn elsewhere */
    w->fb = w->own_fb; w->fb_elsewhere = false;
  }
  rk_irect all = { 0, 0, fw, fh };
  if (w->damage_all) { w->damage[0] = all; w->ndamage = 1; }
  if (hud) {
    rk_rect h = hud_rect(w);
    damage_add(w, (rk_irect){ (int)floorf(h.x * w->scale), (int)floorf(h.y * w->scale), (int)ceilf((h.x + h.w) * w->scale), (int)ceilf((h.y + h.h) * w->scale) });
  }
  rk_irect parts[RK_DAMAGE_MAX];
  int n = 0;
  for (int i = 0; i < w->ndamage; i++) { rk_irect r = rk__irect_meet(w->damage[i], all); if (!rk__irect_empty(r)) parts[n++] = r; }
  w->ndamage = 0; w->damage_all = false;
  if (!n) return;
  /* each part on its own: the ground under it, then whatever reaches into it */
  rk_canvas c;
  memset(&c, 0, sizeof c);
  rk__canvas_init(&c, w->fb, w->scale);
  double pixels = 0;
  for (int i = 0; i < n; i++) {
    rk_irect clip = parts[i];
    rk__fill_rect(w->fb, clip, (float)clip.x0, (float)clip.y0, (float)clip.x1, (float)clip.y1, rk__pm(rk_color_of(RK_ROLE_BG), 1));
    c.nclip = 1; c.clips[0] = clip; c.ox = c.oy = 0; c.alpha = 1;
    rk__render_view(&c, w->root, 0, 0);
    if (w->focus && w->focus_visible && !w->focus->draws_focus && rk__view_visible(w->focus)) {
      rk_rect r = rk__window_rect(w->focus);
      c.ox = c.oy = 0; c.alpha = 1;
      rk__draw_focus(&c, w->focus, r);
    }
    if (hud) draw_hud(&c, w);
    pixels += (double)irect_area(clip);
  }
  rk__path_free(&c.path);
  rk__platform()->present(w->native, w->fb, parts, n);
  rk_meter *m = &rk__meter;
  double ms = rk__wall_ms() - t0;
  m->frames++; m->draw_ms += ms; m->last_ms = ms; m->max_ms = RK_MAX(m->max_ms, ms);
  if (ms > 8) m->slow++;
  m->pixels += pixels;
  if (m->input_at > 0) {
    m->latency_ms = RK_MAX(0.0, rk_app_now() - m->input_at);
    m->max_latency_ms = RK_MAX(m->max_latency_ms, m->latency_ms);
    m->input_at = 0;
  }
  if (trace)
    fprintf(stderr, "rk frame t=%.1f draw=%.2fms parts=%d px=%.0f anim=%u glyphs=%u input=%.1fms\n", now, ms, n, pixels,
            rk__anim_count(), m->glyphs, m->latency_ms);
}

/* Input ----------------------------------------------------------------------------------------------------------- */

static void event(rk_window *w, const rk_event *e);

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
  if (w->floats->nkids) { rk_view *h = hit(w->floats, x, y); if (h) return h; }   /* a menu, over everything */
  /* a sheet takes everything while it is up */
  if (w->sheets->nkids) { rk_view *h = hit(w->sheets, x, y); return h ? h : w->sheets; }
  rk_view *h = hit(w->root, x, y);
  return h ? h : w->root;
}

static bool dispatch(rk_window *w, rk_view *from, const rk_event *e) {
  rk_event local = *e;
  bool input = e->type != RK_EVENT_FOCUS && e->type != RK_EVENT_BLUR && e->type != RK_EVENT_POINTER_LEAVE;
  for (rk_view *v = from; v; v = v->parent) {
    if (input && rk__view_disabled(v)) continue;             /* a disabled view hears no input; what holds it may */
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
  for (; v; v = v->parent) if (v->press != RK_PRESS_NONE || v->on_activate || v->own_activate || (v->menu && v->access == RK_ACCESS_BUTTON)) return rk__view_disabled(v) ? NULL : v;
  return NULL;
}

/* Focus may land on it: focusable, and enabled - or disabled with a tooltip, its reason. */
static bool takes_focus(const rk_view *v) { return v->focusable && (!rk__view_disabled(v) || v->tooltip); }

static rk_view *focusable_up(rk_view *v) {
  for (; v; v = v->parent) if (takes_focus(v)) return v;
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
  rk__animate_transform(v, &from, &to, rk_duration_of(RK_DURATION_PRESS), 0, rk__ease_base);
}

static void press_end(rk_window *w, rk_view *v) {
  if (w->pressed == v) w->pressed = NULL;
  v->pressed = 0;
  rk_transform from = v->ptransform, to = v->transform;
  /* a quick tap still shows the whole sink before it comes back */
  double left = rk_duration_of(RK_DURATION_PRESS) - (rk_app_now() - v->press_since);
  rk__animate_transform(v, &from, &to, rk_duration_of(RK_DURATION_SHORT), RK_MAX(0.0, left), rk__ease_base);
}

static void activate(rk_view *v) {
  if (rk__view_disabled(v)) return;
  if (v->menu && v->access == RK_ACCESS_BUTTON) { rk_menu_open(v->menu, v); return; }   /* a menu button */
  if (!v->on_activate && !v->own_activate) return;
  rk_window *w = rk__view_window(v);
  rk_view *was = w ? w->activated : NULL;
  if (w) w->activated = v;
  rk_retain(v);
  if (v->own_activate) v->own_activate(v, v->own_data);
  if (v->on_activate && (v->parent || v->window)) v->on_activate(v, v->activate_user);
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

typedef struct focus_list { rk_view **v; int n, cap; } focus_list;
static void collect_focus(rk_view *v, focus_list *l) {
  if (v->hidden || v->leaving) return;
  if (takes_focus(v)) { l->v = rk__grow(l->v, &l->cap, l->n + 1, sizeof *l->v); l->v[l->n++] = v; }
  for (int i = 0; i < v->nkids; i++) collect_focus(v->kids[i], l);
}

bool rk_window_focus(rk_window *w, rk_view *v) {
  if (v && !takes_focus(v)) return false;
  if (w->focus == v) return true;
  if (v && rk__view_within(v, w->toasts) && !rk__view_within(w->focus, w->toasts)) w->focus_back = w->focus;
  rk_event e = { sizeof e, RK_EVENT_BLUR, rk_app_now(), { 0, 0 }, 0, 0, 0, 0, 0, NULL, NULL, 0 };
  if (w->focus) { dispatch(w, w->focus, &e); rk__view_dirty(w->focus); }   /* its ring goes */
  w->focus = v;
  e.type = RK_EVENT_FOCUS;
  if (v) dispatch(w, v, &e);
  rk__tip_focus(w, v);
  if (v) rk__view_dirty(v);                                 /* and comes here */
  return true;
}

rk_view *rk_window_focused(rk_window *w) { return w->focus; }

static void focus_step(rk_window *w, int dir) {
  focus_list l = { NULL, 0, 0 };
  collect_focus(w->sheets->nkids ? w->sheets : w->root, &l);   /* the root holds the toasts, last */
  if (w->sheets->nkids) collect_focus(w->toasts, &l);           /* over a sheet, a toast's action still comes last */
  rk_view **list = l.v;
  int n = l.n;
  if (!n) { free(list); return; }
  int at = -1;
  for (int i = 0; i < n; i++) if (list[i] == w->focus) at = i;
  int next = at < 0 ? (dir > 0 ? 0 : n - 1) : (at + dir + n) % n;
  rk_view *to = list[next];
  free(list);
  w->focus_visible = true;
  rk_window_focus(w, to);
  rk__reveal(to);
}

void rk__window_event(rk_window *w, const rk_event *e) {
  if (w->closed) return;
  if (w->needs_layout) layout_window(w);                         /* a press lands on what the next frame will show */
  double was = rk__meter.input_at;
  if (!was) rk__meter.input_at = rk_app_now();
  rk_retain(w);
  event(w, e);
  /* a press, a key, text or a drop may have run the app's handlers: what is declared follows (a move or the wheel
     reaches only views made by hand, which say so with rk_ui_update) */
  bool app = e->type != RK_EVENT_POINTER_MOVE && e->type != RK_EVENT_POINTER_ENTER && e->type != RK_EVENT_POINTER_LEAVE &&
             e->type != RK_EVENT_WHEEL && e->type != RK_EVENT_DRAG_ENTER && e->type != RK_EVENT_DRAG_MOVE && e->type != RK_EVENT_DRAG_LEAVE;
  if (app) { rk__ui_dirty(); rk__ui_settle(); }
  if (!w->needs_draw && !rk__anim_running()) rk__meter.input_at = was;   /* nothing to show for it */
  rk_release(w);
}

/* A press the system took over (moving or resizing the window) ends without activating anything. */
void rk__window_pointer_cancel(rk_window *w) {
  if (w->pressed) press_end(w, w->pressed);
  w->capture = NULL;
}

/* Testing: a press and release in the middle of the view, and a key, as if they came from the system. */
void rk_view_tap(rk_view *v) {
  rk_window *w = rk__view_window(v);
  if (!w) return;
  rk__layout_pending();
  rk_rect r = rk__window_rect(v);
  rk_event e;
  memset(&e, 0, sizeof e);
  e.size = sizeof e; e.time = rk_app_now(); e.position = (rk_point){ r.x + r.w / 2, r.y + r.h / 2 };
  e.type = RK_EVENT_POINTER_DOWN; rk__window_event(w, &e);
  e.type = RK_EVENT_POINTER_UP; rk__window_event(w, &e);
}

void rk_window_key(rk_window *w, uint32_t key) {
  rk_event e;
  memset(&e, 0, sizeof e);
  e.size = sizeof e; e.time = rk_app_now(); e.key = key;
  e.type = RK_EVENT_KEY_DOWN; rk__window_event(w, &e);
  e.type = RK_EVENT_KEY_UP; rk__window_event(w, &e);
}

rk_view *rk_window_view_at(rk_window *w, rk_point p) {
  if (w->closed) return NULL;
  rk__layout_pending();
  rk_view *h = hit_window(w, p.x, p.y);
  return h == w->root || h == w->sheets ? NULL : h;
}

static void event(rk_window *w, const rk_event *e) {
  switch (e->type) {
    case RK_EVENT_POINTER_MOVE: {
      w->pointer = e->position;
      rk_view *h = w->capture ? w->capture : hit_window(w, e->position.x, e->position.y);
      w->hover = h;                                            /* nothing is drawn differently under the pointer */
      if (!w->capture) rk__tip_hover(w, h);                    /* but a tooltip may come */
      dispatch(w, h, e);
      int shape = RK__CURSOR_ARROW;                            /* the move has told the views what lies under it */
      for (rk_view *v = h; v; v = v->parent) if (v->cursor) { shape = v->cursor; break; }
      if (shape != w->cursor && !w->closed) { w->cursor = shape; if (rk__platform()->cursor) rk__platform()->cursor(w->native, shape); }
      break;
    }
    case RK_EVENT_POINTER_DOWN: {
      w->pointer = e->position;
      rk__tip_hide(w, true);
      rk_view *h = hit_window(w, e->position.x, e->position.y);
      if (w->floats->nkids && (!h || h->parent != w->floats)) {    /* a press outside a menu closes it, and goes on */
        rk__menus_close(w);
        h = hit_window(w, e->position.x, e->position.y);
      }
      if (e->button == 1) {                                        /* a right click: the nearest context menu */
        rk_view *t = h;
        while (t && !(t->menu && t->access != RK_ACCESS_BUTTON)) t = t->parent;
        if (t) { rk_rect r = rk__window_rect(t); rk_menu_open_at(t->menu, t, (rk_point){ e->position.x - r.x, e->position.y - r.y }); break; }
      }
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
      if (e->key == RK_KEY_TAB && !(e->mods & (RK_MOD_CTRL | RK_MOD_ALT | RK_MOD_SUPER))) {   /* a view may take Tab (a cell editor goes on to the next cell); else focus moves */
        bool taken = false;
        for (rk_view *v = w->focus; v && !taken; v = v->parent) if (v->cls && v->cls->pub.event) {
          rk_rect r = rk__window_rect(v);
          rk_event local = *e;
          local.position = (rk_point){ e->position.x - r.x, e->position.y - r.y };
          taken = v->cls->pub.event(v, &local, v->state);
        }
        if (!taken) focus_step(w, e->mods & RK_MOD_SHIFT ? -1 : 1);
        break;
      }
      rk_view *f = w->focus;
      if (e->key == RK_KEY_ESCAPE && w->tip) { rk__tip_hide(w, true); break; }   /* Escape first takes the tooltip away */
      if (e->key == RK_KEY_F10 && (e->mods & RK_MOD_SHIFT)) {        /* Shift+F10: the focused view's context menu, below it */
        rk_view *t = f;
        while (t && !(t->menu && t->access != RK_ACCESS_BUTTON)) t = t->parent;
        if (t) { w->focus_visible = true; rk_menu_open(t->menu, t); break; }
      }
      if (f && (e->key == RK_KEY_ENTER || e->key == RK_KEY_SPACE) && pressable(f) == f && !f->pressed) {
        w->focus_visible = true;
        press_start(w, f, 0, 0);
        key_press *kp = rk__alloc(sizeof *kp);
        kp->w = w; kp->v = rk_retain(f);
        rk_app_after(rk_duration_of(RK_DURATION_PRESS), key_release, kp);
        break;
      }
      rk_view *to = f;
      if (!to && w->sheets->nkids) to = w->sheets->kids[w->sheets->nkids - 1];
      if (!to && w->controller) to = rk__controller_key_view(w->controller);
      dispatch(w, to ? to : w->root, e);
      break;
    }
    case RK_EVENT_WHEEL: rk__tip_hide(w, true); dispatch(w, hit_window(w, e->position.x, e->position.y), e); break;
    case RK_EVENT_DRAG_ENTER: case RK_EVENT_DRAG_MOVE: case RK_EVENT_DRAG_LEAVE: case RK_EVENT_DROP: rk__window_drag(w, e); break;
    default: dispatch(w, w->focus ? w->focus : w->root, e); break;
  }
}

/* Files dragged over the window: on every move the view under the pointer, or the nearest parent, that answers
   DRAG_MOVE with true takes them; the one before hears DRAG_LEAVE. The drop goes to the one that took them there. */
static void drag_leave(rk_window *w) {
  rk_view *t = w->drag_over;
  w->drag_over = NULL;
  if (!t || !t->cls || !t->cls->pub.event) return;
  rk_event e = { sizeof e, RK_EVENT_DRAG_LEAVE, rk_app_now(), { 0, 0 }, 0, 0, 0, 0, 0, NULL, NULL, 0 };
  t->cls->pub.event(t, &e, t->state);
}

static rk_view *drag_taker(rk_window *w, const rk_event *e) {
  rk_event m = *e;
  m.type = RK_EVENT_DRAG_MOVE;
  for (rk_view *v = hit_window(w, e->position.x, e->position.y); v; v = v->parent) {
    if (!v->cls || !v->cls->pub.event) continue;
    rk_rect r = rk__window_rect(v);
    m.position = (rk_point){ e->position.x - r.x, e->position.y - r.y };
    if (v->cls->pub.event(v, &m, v->state)) return v;
  }
  return NULL;
}

/* A text/uri-list, as X11 and Wayland hand dropped files over: one URI a line, comments after #; the local file:
   URIs as paths, percent-decoded. Returns how many; *out and its strings are malloc'd (rk__free_paths). */
static int hexval(int c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }
int rk__uri_list_paths(const char *text, size_t n, char ***out) {
  int count = 0, cap = 0;
  char **paths = NULL;
  for (size_t i = 0; i < n;) {
    size_t e = i;
    while (e < n && text[e] != '\n' && text[e] != '\r') e++;
    const char *line = text + i;
    size_t len = e - i;
    i = e;
    while (i < n && (text[i] == '\n' || text[i] == '\r')) i++;
    if (!len || line[0] == '#' || len < 7 || strncmp(line, "file://", 7)) continue;
    const char *p = line + 7, *end = line + len;
    if (p < end && *p != '/') {                              /* a host: only localhost is this machine for sure */
      if (end - p < 10 || strncmp(p, "localhost/", 10)) continue;
      p += 9;
    }
    char *path = rk__alloc((size_t)(end - p) + 1), *q = path;
    for (; p < end; p++) {
      if (*p == '%' && end - p >= 3 && hexval(p[1]) >= 0 && hexval(p[2]) >= 0) { *q++ = (char)(hexval(p[1]) * 16 + hexval(p[2])); p += 2; }
      else *q++ = *p;
    }
    *q = 0;
    paths = rk__grow(paths, &cap, count + 1, sizeof *paths);
    paths[count++] = path;
  }
  *out = paths;
  return count;
}
void rk__free_paths(char **paths, int n) { for (int i = 0; i < n; i++) free(paths[i]); free(paths); }

/* The system's file chooser, for this window. */
typedef struct choosing { rk_window *w; void (*chosen)(rk_window *, const char *const *, int, void *); void *user; } choosing;
static void chooser_done(void *ctx, const char *const *files, int n) {
  choosing *c = ctx;
  if (c->chosen) c->chosen(c->w, files, n, c->user);
  rk_release(c->w);
  free(c);
}
bool rk_window_choose_files(rk_window *w, const char *accept, rk_choose choose, void (*chosen)(rk_window *w, const char *const *files, int n, void *user), void *user) {
  bool multiple = choose == RK_CHOOSE_MANY;
  const rk_platform *p = rk__platform();
  if (!w || w->closed || !p->choose_files) return false;
  choosing *c = rk__alloc(sizeof *c);
  c->w = rk_retain(w); c->chosen = chosen; c->user = user;
  if (p->choose_files(w->native, accept, multiple, chooser_done, c)) return true;
  rk_release(w); free(c);
  return false;
}

bool rk__window_drag(rk_window *w, const rk_event *e) {
  rk__layout_pending();
  switch (e->type) {
    case RK_EVENT_DRAG_ENTER: case RK_EVENT_DRAG_MOVE: {
      rk_view *t = drag_taker(w, e);
      if (t != w->drag_over) { drag_leave(w); w->drag_over = t; }
      return t != NULL;
    }
    case RK_EVENT_DRAG_LEAVE: drag_leave(w); return false;
    case RK_EVENT_DROP: {
      rk_view *t = drag_taker(w, e);                         /* where it is let go, which may not be where it last moved */
      if (t != w->drag_over) drag_leave(w);
      w->drag_over = NULL;
      if (!t) return false;
      rk_event d = *e;
      rk_rect r = rk__window_rect(t);
      d.position = (rk_point){ e->position.x - r.x, e->position.y - r.y };
      return t->cls->pub.event(t, &d, t->state);
    }
    default: return false;
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

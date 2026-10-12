/* tokens.c: the tokens in the current theme, density and motion setting; easing curves. */
#include "rk_internal.h"
#include "rk_tokens.h"

rk_prefs rk__prefs = { RK_THEME_SYSTEM, RK_MOTION_PREF_SYSTEM, RK_DENSITY_SYSTEM, false, false, false, { 0, 0 } };

const float rk__ease_base[4] = RK_TOK_EASE_BASE;
const float rk__ease_in[4] = RK_TOK_EASE_IN;
const float rk__ease_out[4] = RK_TOK_EASE_OUT;
const float rk__ease_glide[4] = RK_TOK_EASE_PIVOT_GLIDE;

bool rk__dark(void) { return rk__prefs.theme == RK_THEME_DARK || (rk__prefs.theme == RK_THEME_SYSTEM && rk__prefs.system_dark); }
bool rk__reduced(void) { return rk__prefs.motion == RK_MOTION_PREF_OFF || (rk__prefs.motion == RK_MOTION_PREF_SYSTEM && rk__prefs.system_reduced); }
int rk__density_index(void) {
  return rk__prefs.density == RK_DENSITY_COMPACT || (rk__prefs.density == RK_DENSITY_SYSTEM && rk__prefs.system_compact) ? 1 : 0;
}

rk_color rk_color_of(rk_role role) {
  if (role <= RK_ROLE_NONE || role >= RK_ROLE_COUNT) return 0;
  int t = rk__dark() ? 1 : 0;
  if (role == RK_ROLE_ACCENT && rk__prefs.accent[t]) return rk__prefs.accent[t];
  if (role == RK_ROLE_ON_ACCENT && rk__prefs.accent[t]) {
    /* text on a custom accent: the ground that reads on it */
    rk_color a = rk__prefs.accent[t];
    float l = 0.2126f * ((a >> 24) & 255) + 0.7152f * ((a >> 16) & 255) + 0.0722f * ((a >> 8) & 255);
    return l > 140 ? rk_tok_colors[0][RK_ROLE_TEXT] : 0xffffffffu;
  }
  return rk_tok_colors[t][role];
}

float rk_metric_of(rk_metric metric) {
  if (metric < 0 || metric >= RK_METRIC_COUNT) return 0;
  return rk_tok_metrics[rk__density_index()][metric];
}

double rk_duration_of(rk_duration d) {
  if (d < 0 || d >= RK_DURATION_COUNT) return 0;
  if (d < RK_TOK_MOTION_COUNT && rk__reduced()) return 0;
  return rk_tok_durations[d];
}

rk_color rk__mix(rk_color a, rk_color b, float t) {
  rk_color out = 0;
  for (int s = 0; s < 32; s += 8) {
    float x = (float)((a >> s) & 255), y = (float)((b >> s) & 255);
    out |= (rk_color)(x + (y - x) * t + 0.5f) << s;
  }
  return out;
}

/* cubic-bezier(x1, y1, x2, y2): solve x(s) = t by Newton's method, falling back to bisection, then return y(s). */
float rk__ease(const float b[4], float t) {
  if (t <= 0) return 0;
  if (t >= 1) return 1;
  float x1 = b[0], y1 = b[1], x2 = b[2], y2 = b[3];
  float cx = 3 * x1, bx = 3 * (x2 - x1) - cx, ax = 1 - cx - bx;
  float cy = 3 * y1, by = 3 * (y2 - y1) - cy, ay = 1 - cy - by;
  float s = t;
  for (int i = 0; i < 8; i++) {
    float x = ((ax * s + bx) * s + cx) * s - t, dx = (3 * ax * s + 2 * bx) * s + cx;
    if (fabsf(x) < 1e-5f) goto done;
    if (fabsf(dx) < 1e-6f) break;
    s -= x / dx;
  }
  {
    float lo = 0, hi = 1;
    s = t;
    for (int i = 0; i < 30; i++) {
      float x = ((ax * s + bx) * s + cx) * s;
      if (fabsf(x - t) < 1e-5f) break;
      if (x < t) lo = s; else hi = s;
      s = (lo + hi) / 2;
    }
  }
done:
  return ((ay * s + by) * s + cy) * s;
}

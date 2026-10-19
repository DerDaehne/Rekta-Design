/* tokens.c: the tokens in the current theme, density and motion setting; easing curves. */
#include "rk_internal.h"
#include "rk_tokens.h"

rk_prefs rk__prefs = { .theme = RK_THEME_SYSTEM, .motion = RK_MOTION_PREF_SYSTEM, .density = RK_DENSITY_SYSTEM };

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
  if (role <= RK_ROLE_NONE || role >= RK__ROLE_COUNT) return 0;
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
  if (metric < 0 || metric >= RK__METRIC_COUNT) return 0;
  if (rk__prefs.metric_set[metric]) return rk__prefs.metric[metric];
  return rk_tok_metrics[rk__density_index()][metric];
}

double rk_duration_of(rk_duration d) {
  if (d < 0 || d >= RK__DURATION_COUNT) return 0;
  if (d < RK_TOK_MOTION_COUNT && rk__reduced()) return 0;
  if (rk__prefs.timing_set[d]) return rk__prefs.timing[d];
  return rk_tok_durations[d];
}

rk_color rk__series(int code) {
  rk_color c = code >= 1 && code <= 8 ? rk__prefs.series[code - 1][rk__dark() ? 1 : 0] : 0;
  return c ? c : rk_color_of(RK_ROLE_TEXT);
}
rk_color rk_series_of(int code) { return rk__series(code); }

/* Colour maths ---------------------------------------------------------------------------------------------------- */

static double lin(double v) { return v <= 0.04045 ? v / 12.92 : pow((v + 0.055) / 1.055, 2.4); }
static double gam(double v) { return v <= 0.0031308 ? 12.92 * v : 1.055 * pow(v, 1 / 2.4) - 0.055; }
static void channels(rk_color c, double out[3]) { for (int i = 0; i < 3; i++) out[i] = ((c >> (24 - 8 * i)) & 255) / 255.0; }

static double luminance(rk_color c) {
  double v[3];
  channels(c, v);
  return 0.2126 * lin(v[0]) + 0.7152 * lin(v[1]) + 0.0722 * lin(v[2]);
}

double rk__contrast(rk_color a, rk_color b) {
  double x = luminance(a), y = luminance(b);
  return (RK_MAX(x, y) + 0.05) / (RK_MIN(x, y) + 0.05);
}

static const double cvd[3][3][3] = {
  { { 0.152286, 1.052583, -0.204868 }, { 0.114503, 0.786281, 0.099216 }, { -0.003882, -0.048116, 1.051998 } },   /* protan */
  { { 0.367322, 0.860646, -0.227968 }, { 0.280085, 0.672501, 0.047413 }, { -0.01182, 0.04294, 0.968881 } },      /* deutan */
  { { 1.255528, -0.076749, -0.178779 }, { -0.078411, 0.930809, 0.147602 }, { 0.004733, 0.691367, 0.3039 } },     /* tritan */
};

static void oklab(rk_color c, int kind, double out[3]) {
  double v[3], r[3];
  channels(c, v);
  for (int i = 0; i < 3; i++) r[i] = lin(v[i]);
  if (kind >= 1 && kind <= 3) {                            /* as seen with the deficiency, back to sRGB as color.mjs does */
    double s[3];
    for (int i = 0; i < 3; i++) {
      const double *m = cvd[kind - 1][i];
      s[i] = gam(RK_CLAMP(m[0] * r[0] + m[1] * r[1] + m[2] * r[2], 0.0, 1.0));
    }
    for (int i = 0; i < 3; i++) r[i] = lin(s[i]);
  }
  double l = cbrt(0.4122214708 * r[0] + 0.5363325363 * r[1] + 0.0514459929 * r[2]);
  double m = cbrt(0.2119034982 * r[0] + 0.6806995451 * r[1] + 0.1073969566 * r[2]);
  double s = cbrt(0.0883024619 * r[0] + 0.2817188376 * r[1] + 0.6299787005 * r[2]);
  out[0] = 0.2104542553 * l + 0.793617785 * m - 0.0040720468 * s;
  out[1] = 1.9779984951 * l - 2.428592205 * m + 0.4505937099 * s;
  out[2] = 0.0259040371 * l + 0.7827717662 * m - 0.808675766 * s;
}

double rk__distance(rk_color a, rk_color b, int kind) {
  double p[3], q[3];
  oklab(a, kind, p); oklab(b, kind, q);
  return sqrt((p[0] - q[0]) * (p[0] - q[0]) + (p[1] - q[1]) * (p[1] - q[1]) + (p[2] - q[2]) * (p[2] - q[2]));
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

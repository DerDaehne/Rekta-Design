/* rekta_test.h: driving and measuring a rekta-core app - for tests, tours and profilers, not for the app itself.
 * Exported by the same library; kept out of rekta.h so the API an app sees stays what an app needs. */
#ifndef REKTA_TEST_H
#define REKTA_TEST_H

#include "rekta.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Testing --------------------------------------------------------------------------------------------------------- */

RK_API void rk_window_inject(rk_window *window, const rk_event *event);   /* as if it came from the system */
RK_API void rk_view_tap(rk_view *view);            /* a press and release in its middle */
RK_API void rk_window_key(rk_window *window, uint32_t key);   /* a key pressed and released: an rk_key or a letter */
RK_API rk_view *rk_window_view_at(rk_window *window, rk_point point);   /* what a press there reaches, or NULL */
RK_API bool rk_window_snapshot(rk_window *window, const char *png_path);  /* the window's pixels as a PNG */
RK_API void rk_app_run_for(double ms);             /* headless: advance the virtual clock, running every frame */

/* What the loop did, to measure an app by. RK_HUD=1 shows it in the window's corner; RK_TRACE=1 prints each frame. */
typedef struct rk_stats {
  uint32_t size;                                   /* set by the caller: sizeof(rk_stats) */
  uint32_t frames;                                 /* frames drawn */
  uint32_t animations;                             /* running now */
  uint32_t views;                                  /* alive now */
  uint32_t glyphs;                                 /* glyphs rasterized (each a cache miss) */
  uint32_t slow;                                   /* frames that took longer than 8 ms to draw */
  double draw_ms;                                  /* time spent drawing all frames */
  double last_ms, max_ms;                          /* the last frame, the slowest */
  double pixels;                                   /* device pixels drawn */
  double latency_ms, max_latency_ms;               /* from an input event to the end of the frame that showed it */
} rk_stats;
RK_API void rk_app_stats(rk_stats *stats);
RK_API void rk_app_stats_reset(void);              /* the counts start again from zero */

#ifdef __cplusplus
}
#endif

#endif /* REKTA_TEST_H */

/* rekta.c: the whole library as one translation unit. Compile this file (and, on macOS, platform_cocoa.m).
   Define RK_HEADLESS_ONLY to build without a windowing system (tests, servers), RK_NO_WAYLAND for X11 alone on Linux. */
#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef UNICODE
#    define UNICODE
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  ifndef _USE_MATH_DEFINES
#    define _USE_MATH_DEFINES
#  endif
#endif
#if !defined(_WIN32) && !defined(__APPLE__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L   /* clock_gettime, poll and pipe under -std=c99 */
#endif
#if !defined(_WIN32) && !defined(__APPLE__) && !defined(RK_HEADLESS_ONLY) && !defined(RK_NO_WAYLAND)
#define RK_WAYLAND 1
#endif
#include "object.c"
#include "tokens.c"
#include "pool.c"
#include "raster.c"
#include "png.c"
#include "text.c"
#include "canvas.c"
#include "view.c"
#include "anim.c"
#include "window.c"
#include "widgets.c"
#include "list.c"
#include "controller.c"
#include "app.c"
#include "platform_headless.c"
#if !defined(RK_HEADLESS_ONLY)
#  if defined(_WIN32)
#    include "platform_win32.c"
#  elif defined(__APPLE__)
     /* platform_cocoa.m is compiled on its own, as Objective-C */
#  else
#    include "platform_x11.c"
#    if defined(RK_WAYLAND)
#      include "platform_wayland.c"
#    endif
#  endif
#endif

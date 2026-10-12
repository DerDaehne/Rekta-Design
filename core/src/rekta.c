/* rekta.c: the whole library as one translation unit. Compile this file (and, on macOS, platform_cocoa.m).
   Define RK_HEADLESS_ONLY to build without a windowing system (tests, servers). */
#include "object.c"
#include "tokens.c"
#include "raster.c"
#include "png.c"
#include "text.c"
#include "canvas.c"
#include "view.c"
#include "anim.c"
#include "window.c"
#include "widgets.c"
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
#  endif
#endif

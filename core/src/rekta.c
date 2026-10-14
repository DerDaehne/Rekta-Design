/* rekta.c: the whole library as one translation unit. Compile this file (and, on macOS, platform_cocoa.m).
   Define RK_HEADLESS_ONLY to build without a windowing system (tests, servers), RK_NO_WAYLAND for X11 alone on Linux. */
/* the platform macros (Unicode Win32, POSIX, Wayland) are rk_internal.h's, so each module also compiles on its own */
#include "object.c"
#include "tokens.c"
#include "pool.c"
#include "raster.c"
#include "png.c"
#include "fontfind.c"
#include "text.c"
#include "canvas.c"
#include "view.c"
#include "anim.c"
#include "window.c"
#include "widgets.c"
#include "tip.c"
#include "sort.c"
#include "drop.c"
#include "controls.c"
#include "image.c"
#include "list.c"
#include "tree.c"
#include "field.c"
#include "menu.c"
#include "undo.c"
#include "controller.c"
#include "split.c"
#include "app.c"
#include "platform_headless.c"
#if !defined(RK_HEADLESS_ONLY)
#  if defined(_WIN32)
#    include "platform_win32.c"
#  elif defined(__APPLE__)
     /* platform_cocoa.m is compiled on its own, as Objective-C */
#  else
#    include "desktop_linux.c"
#    include "platform_x11.c"
#    if defined(RK_WAYLAND)
#      include "platform_wayland.c"
#    endif
#  endif
#endif

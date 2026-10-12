# rekta-core

Rekta Design as native views in C, for Windows, macOS and Linux. The web keeps
`rekta.css` and `rekta.js`; both follow the same tokens, icons and motion. The
design and the ABI rules are in [docs/core.md](../docs/core.md); the whole API
is [include/rekta.h](include/rekta.h).

- No dependency beyond the system: Win32, Cocoa or Xlib. Glyphs come from
  `third_party/stb_truetype.h` (public domain); Barlow is built in.
- One header, one translation unit (`src/rekta.c`), plus `src/platform_cocoa.m`
  on macOS.
- A software renderer: every platform draws the same pixels, and the tests
  check them without a screen.

## Build

```sh
make               # build/librekta.a and build/demo
make test          # 73 checks and 14 reference images, headless (the comparison needs node)
make shots         # the demo's script as PNGs in build/shots
make sanitize      # tests and demo under AddressSanitizer and UBSan
```

On Windows, run `build.bat` (or `build.bat test`) from a Developer Command
Prompt, or use MinGW with `make`. To use the library in your own build,
compile `src/rekta.c` (and `src/platform_cocoa.m` with `-fobjc-arc` on macOS)
and link:

| OS | link with |
|---|---|
| Linux | `-lX11 -lm -lpthread` |
| macOS | `-framework Cocoa -framework QuartzCore` |
| Windows | `user32 gdi32 advapi32` |

Define `RK_HEADLESS_ONLY` for a build without a windowing system (tests,
servers); `RK_HEADLESS=1` in the environment runs any build without a screen.

## A first app

```c
#include "rekta.h"

static void open_notes(rk_view *tile, void *user) {
  (void)user;
  rk_window_toast(rk_view_window(tile), "Release notes are on their way.", NULL, NULL, NULL);
}

static void launch(void *user) {
  (void)user;
  rk_window *window = rk_window_create(&(rk_window_desc){ .size = sizeof(rk_window_desc), .title = "My tool", .width = 960, .height = 640 });

  rk_view *board = rk_view_create();
  rk_view_set_layout(board, RK_LAYOUT_TILES);

  rk_view *builds = rk_tile_create(RK_STATUS_SUCCESS, NULL);   /* the fill is the status */
  rk_tile_set_stat(builds, "312", "finished today");
  rk_view_set_span(builds, 2, 1);
  rk_view_add(board, builds);
  rk_release(builds);                                         /* the board holds it now */

  rk_view *notes = rk_tile_create(RK_STATUS_NONE, NULL);
  rk_tile_set_stat(notes, "0.3", "release notes");
  rk_view_on_activate(notes, open_notes, NULL);              /* it presses, and opens something */
  rk_view_add(board, notes);
  rk_release(notes);

  rk_controller *home = rk_panorama_create();
  rk_panorama_add(home, "today", board, 0);
  rk_release(board);

  rk_window_set_root(window, home);
  rk_release(home);
  rk_window_show(window);
  rk_release(window);                                         /* the app keeps an open window */
}

int main(void) {
  return rk_app_main(&(rk_app_desc){ .size = sizeof(rk_app_desc), .name = "My tool", .id = "de.example.tool", .launch = launch });
}
```

A live value changes with its meaning, never with numbers you pick:

```c
rk_view_transition(tile, RK_TRANSITION_PEEK);   /* a new number peeks in */
rk_tile_set_stat(tile, "13", "builds today");

rk_view_transition(tile, RK_TRANSITION_FLIP);   /* a new status turns the tile over */
rk_tile_set_status(tile, RK_STATUS_ERROR, NULL);

rk_animate_begin(RK_MOTION_REFLOW);             /* it grows; the grid makes room */
rk_view_set_span(tile, 2, 2);
rk_animate_commit();
```

## Status

Stage 1 of [docs/core.md](../docs/core.md#stages): tokens, renderer, views,
controllers (places with the turn, panorama, pivot, sheets), motion, the press,
focus and the keyboard, toasts, the title bar; headless and X11 tested here,
Win32 and Cocoa built and tested in CI.

Not there yet: text fields, menus, tables, long lists (stage 2); screen readers
(the accessibility tree is built, the bridges to UI Automation, NSAccessibility
and AT-SPI are stage 3), Wayland without XWayland, several windows' focus
between them.

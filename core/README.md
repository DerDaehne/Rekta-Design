# rekta-core

Rekta Design as native views in C, for Windows, macOS and Linux. The web keeps
`rekta.css` and `rekta.js`; both follow the same tokens, icons and motion. The
design and the ABI rules are in [docs/core.md](../docs/core.md); the whole API
is [include/rekta.h](include/rekta.h).

- No dependency beyond the system: Win32, Cocoa, or Wayland (libwayland,
  libxkbcommon) with Xlib as the fallback. Glyphs come from
  `third_party/stb_truetype.h` (public domain); Barlow is built in.
- One header, one translation unit (`src/rekta.c`), plus `src/platform_cocoa.m`
  on macOS.
- A software renderer: every platform draws the same pixels, and the tests
  check them without a screen.

## Try it with Nix

The flake at the root brings everything the demo needs, on Linux and macOS:

```sh
nix run github:DerDaehne/Rekta-Design    # build and start the demo
nix develop                              # in a clone: the compiler, make, Wayland, X11 and node; then make -C core
nix flake check                          # builds and runs the tests and the reference images
```

The demo is a panorama of six sections, one for each part of the core: tiles
(the press, the turn to a place, peek, flip, tiles that come and go, reflow),
live data, motion (sheet, toasts, cross-fade), controls (every button, progress,
rows, a custom view drawn with the canvas), type and icons, and the settings.
Tab moves focus, Enter presses, Page Up and Page Down go by section, Escape goes
back. `demo --shots DIR` takes the same tour without a screen.

## Build

```sh
make               # build/librekta.a and build/demo
make test          # 73 checks and 14 reference images, headless (the comparison needs node)
make shots         # the demo's tour as PNGs in build/shots
make sanitize      # tests and demo under AddressSanitizer and UBSan
```

On Windows, run `build.bat` (or `build.bat test`) from a Developer Command
Prompt, or use MinGW with `make`. To use the library in your own build,
compile `src/rekta.c` (and `src/platform_cocoa.m` with `-fobjc-arc` on macOS)
and link:

| OS | link with |
|---|---|
| Linux | `-lwayland-client -lwayland-cursor -lxkbcommon -lrt -lX11 -lXext -lm -lpthread -ldl` (or `-DRK_NO_WAYLAND` and only the X11 half) |
| macOS | `-framework Cocoa -framework QuartzCore` |
| Windows | `user32 gdi32 advapi32` |

Define `RK_HEADLESS_ONLY` for a build without a windowing system (tests,
servers); `RK_HEADLESS=1` in the environment runs any build without a screen.

## A first app

`examples/hello.c`, built with the library and run by `make test`; `man ./man/rekta.3`
explains every call in it.

```c
/* hello.c: a first app. A section of tiles, a button that counts, a tile that opens something. */
#include "rekta.h"
#include <stdio.h>

static rk_view *builds;                            /* the one view that changes later */
static int finished = 312;

static void open_notes(rk_view *tile, void *user) {
  rk_window_toast(rk_view_window(tile), "Release notes are on their way.", NULL, NULL, NULL);
}

static void build_one(rk_view *button, void *user) {
  char text[16];
  snprintf(text, sizeof text, "%d", ++finished);
  rk_view_transition(builds, RK_TRANSITION_PEEK);  /* a new number peeks in */
  rk_tile_set_stat(builds, text, "finished today");
}

static rk_view *today(void) {
  rk_view *page = rk_column();
  rk_add(page, rk_label("Today", RK_TEXT_GROUP));

  rk_view *tiles = rk_add(page, rk_tile_grid());
  builds = rk_add(tiles, rk_tile("312", "finished today"));
  rk_tile_set_status(builds, RK_STATUS_SUCCESS, NULL);
  rk_view_set_span(builds, 2, 1);

  rk_view *notes = rk_add(tiles, rk_tile("0.3", "release notes"));
  rk_view_on_activate(notes, open_notes, NULL);    /* it presses, and opens something */

  rk_add(page, rk_button("build one", build_one, NULL));
  return page;
}

static void launch(void *user) {
  rk_window_desc desc = { .size = sizeof desc, .title = "My tool", .width = 960, .height = 640 };
  rk_window *window = rk_window_create(&desc);

  rk_controller *home = rk_panorama();
  rk_panorama_add(home, "today", today(), 0);
  rk_window_show(window, home);
  rk_release(window);                              /* an open window lives until it is closed */
}

int main(void) {
  rk_app_desc app = { .size = sizeof app, .name = "My tool", .id = "de.example.tool", .launch = launch };
  return rk_app_main(&app);
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

Stages 1 and 2 of [docs/core.md](../docs/core.md#stages): tokens, renderer,
views, controllers (places with the turn, panorama, pivot, sheets), motion, the
press, focus everywhere and the keyboard, toasts, undo, the title bar, text
fields and areas with the clipboard and input methods, every script through the
system's fonts, checkboxes, options, toggles, segments, sliders, steppers, selects, comboboxes and dates, scroll views, menus, long lists, tables
that sort, resize and edit; headless, Wayland and X11 tested here (Wayland is
the default where a compositor answers; `RK_PLATFORM=x11` picks X11), Win32 and
Cocoa built and tested in CI.

Not there yet: screen readers (the accessibility tree is built, the bridges to
UI Automation, NSAccessibility and AT-SPI are stage 3), several windows' focus
between them, the reference gallery against the web in CI.

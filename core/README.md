# rekta-core

Rekta Design as native views in C, for Windows, macOS and Linux. The web keeps
`rekta.css` and `rekta.js`; both follow the same tokens, icons and motion. The
design and the ABI rules are in [docs/core.md](../docs/core.md); the whole API
is [include/rekta.h](include/rekta.h).

- No dependency beyond the system: Win32, Cocoa, or Wayland (libwayland,
  libxkbcommon) with Xlib as the fallback. Glyphs come from
  `third_party/stb_truetype.h` and pictures from `third_party/stb_image.h`
  (both public domain); Barlow is built in.
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

The demo is a panorama of sections, one for each part of the core: tiles
(the press, the turn to a place, peek, flip, tiles that come and go, reflow),
live data, motion (sheet, toasts, cross-fade), controls (every button, progress,
rows, a custom view drawn with the canvas), long lists and tables, a desk (a
tree beside its detail in a split, rows to put in order, a place to drop
pictures), type and icons, and the settings.
Tab moves focus, Enter presses, Page Up and Page Down go by section, Escape goes
back. `demo --shots DIR` takes the same tour without a screen.

## Build

```sh
make               # build/librekta.a and build/demo
make test          # 436 checks and 36 reference images, headless (the comparison needs node)
make shots         # the demo's tour as PNGs in build/shots
make sanitize      # tests and demo under AddressSanitizer and UBSan
make modules       # each source compiled on its own; the unity build only joins them
make abi           # the shared library exports the header's functions and nothing else
```

On Windows, run `build.bat` (or `build.bat test`, `build.bat shared`) from a
Developer Command Prompt, or use MinGW with `make`.

`make install` (with `PREFIX`, default `/usr/local`) puts the static and the
shared library, the header, the man page and `rekta.pc` in place, so an app
builds with `cc app.c $(pkg-config --cflags --libs rekta)`. The shared library
(`librekta.so.0.3`, `librekta.0.3.dylib`, `rekta.dll`) exports only the
header's `RK_API` functions, which `make abi` checks; before 1.0 the soname
carries the minor version, since a minor version may still break the ABI.

To compile it into your own build instead, take `src/rekta.c` (and `src/platform_cocoa.m` with `-fobjc-arc` on macOS)
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

static int finished = 312;                         /* the state: the window shows what is here */

static void build_one(rk_view *button, void *user) { finished++; }   /* the window follows: the new number peeks in */

static void open_notes(rk_view *tile, void *user) {
  rk_window_toast(rk_view_window(tile), "Release notes are on their way.", NULL, NULL, NULL);
}

static void today(void *user) {
  char builds[16];
  snprintf(builds, sizeof builds, "%d", finished);
  RK_PANORAMA() RK_SECTION("today") {
    RK_LABEL("Today", .style = RK_TEXT_GROUP);
    RK_TILES() {
      RK_TILE(builds, .label = "finished today", .status = RK_STATUS_SUCCESS, .span = { 2, 1 });
      RK_TILE("0.3", .label = "release notes", .on_press = open_notes);   /* it presses, and opens something */
    }
    RK_BUTTON("build one", .on_press = build_one);
  }
}

static void launch(void *user) {
  rk_window_desc desc = { .size = sizeof desc, .title = "My tool", .width = 960, .height = 640, .build = today };
  rk_window_create(&desc);
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
system's fonts, checkboxes, options, toggles, segments, sliders, steppers, selects, comboboxes and dates, scroll views, pictures, menus, long lists, tables
that sort, resize and edit, trees, splits, rows the user puts in order, files
dragged in from the system and a place to drop them, the system's file chooser
and notifications, pivots along the top or down the side, tooltips, views
disabled with their reason, confirmations, controllers inside another's views,
and misuse said through a log instead of crashing; headless, Wayland and X11 tested here (Wayland is
the default where a compositor answers; `RK_PLATFORM=x11` picks X11), Win32 and
Cocoa built and tested in CI. A reference gallery holds core to the web: the
same specimens drawn by both (`make gallery`, then `node test/gallery.mjs
core/build/gallery`), compared in size, extent, colour and shape in CI.

Not there yet: screen readers (the accessibility tree is built, the bridges to
UI Automation, NSAccessibility and AT-SPI are stage 3), several windows' focus
between them.

# Rekta core: native views in C

`rekta-core` is Rekta Design for desktop apps that do not run in a browser:
Windows, macOS and Linux. The web keeps `rekta.css` and `rekta.js`; both
implement the same specification (the tokens, the icons, [motion.md](motion.md),
[behaviour.md](behaviour.md)), and a reference gallery holds them to it.

## Goals

- **Few dependencies.** The operating system, a C compiler, and
  `stb_truetype.h` (public domain, vendored) for glyph outlines. Nothing to
  install, nothing to ship besides the library.
- **Small.** One header, a handful of C files, one Objective-C file for macOS.
  A unity build (`cc -c core/src/rekta.c`) is enough.
- **Quality first.** Anti-aliased shapes with exact coverage, kerned text
  with sub-pixel positioning, every motion from the tokens, reduced motion
  instant and never skipped, an accessibility tree from the start.
- **A clean ABI.** C99, opaque handles, no struct that can grow crosses the
  boundary except size-prefixed descriptors. Any language with a C FFI can use
  it.
- **Composition as in UIKit.** Windows hold a root controller; controllers own
  views, appear and disappear, contain other controllers and present sheets;
  views form a tree, lay out their children and draw themselves; events travel
  a responder chain.

## Layers

```
app code ──────────────── rekta.h (the ABI)
  controllers   window · controller · places · panorama · pivot · sheet
  views         view tree · layout · built-in views (tile, rows, label, button, icon, toast)
  motion        implicit animations · transitions (peek, flip, turn) · press
  render        display tree → software rasterizer (RGBA, premultiplied) · text · icons
  platform      win32.c · cocoa.m · x11.c · headless.c
```

The platform layer is the only part that differs per OS. It opens a window,
hands the core input and a pixel buffer, and puts the pixels on screen:

| OS | window, input, text input | present | system settings |
|---|---|---|---|
| Windows | Win32, `WM_IME_*` | `StretchDIBits` | registry (dark mode), `SPI_GETCLIENTAREAANIMATION` |
| macOS | Cocoa (`NSWindow`, `NSTextInputClient`) | `CALayer` contents | `effectiveAppearance`, `accessibilityDisplayShouldReduceMotion` |
| Linux | Wayland: xdg-shell, xdg-decoration, fractional-scale, cursor-shape, xkbcommon | `wl_shm` buffers, damage, frame callbacks; resizes acknowledged and drawn in one commit | GNOME and KDE settings |
| Linux (fallback) | Xlib, XIM | MIT-SHM (`XPutImage` without it) | XSETTINGS |
| headless | none | memory, PNG | fixed |

The headless platform runs on a virtual clock: frames advance at 60 per second
without waiting, so a test is deterministic and a screenshot is the same on every
machine.

### Why a software renderer

Rekta draws flat fills, hairlines, small icons and text; the expensive parts are
cached: a view that is moving or turning is drawn once into a layer and only
composited while it moves. A CPU rasterizer keeps the core free of three GPU
APIs, makes every platform pixel-identical (so one set of reference images
checks all three), and fits in a few hundred lines. Only changed regions are
drawn again. The renderer sits behind an internal display-tree interface, so a
GPU backend can come later without touching the ABI.

## The ABI

All of it is in `core/include/rekta.h`.

- **Names.** Functions `rk_noun_verb`, types `rk_noun`, constants `RK_NOUN`.
- **Values** cross by value and never change layout: `rk_point`, `rk_size`,
  `rk_rect`, `rk_insets`, `rk_transform`, `rk_color` (`0xRRGGBBAA`). Units are
  logical pixels (CSS px, Apple pt); the window scales to the screen.
- **Objects** are opaque pointers: `rk_window`, `rk_view`, `rk_controller`,
  reference counted under one rule. A constructor returns a reference you
  own; it is named after the thing (`rk_label`, `rk_tile`, `rk_column`), or
  `*_create` where the thing is a type of its own (window, view, controller).
  Adding, showing and presenting take that reference over (`rk_add`,
  `rk_insert`, `rk_panorama_add`, `rk_pivot_add`, `rk_places`,
  `rk_places_go`, `rk_controller_present`, `rk_window_show`): what goes into
  the tree belongs to the tree, and app code never releases it. `rk_add`
  returns the child, so it can be kept to change it later. Everything else
  is borrowed. `man ./core/man/rekta.3` has the whole API by task.
- **Descriptors** that may grow start with `uint32_t size` (set it to
  `sizeof`): `rk_app_desc`, `rk_window_desc`, `rk_view_class`,
  `rk_controller_class`. A newer library reads an older, shorter struct.
- **Callbacks** are a function pointer and a `void *user`, never more.
- **Enums** are 32-bit (`_FORCE32` members); new members only at the end.
- **Strings** are UTF-8, NUL-terminated, copied by the callee.
- **Threads.** Everything runs on the main thread, as in UIKit;
  `rk_app_post` is the one function another thread may call.
- **Errors.** Creation returns `NULL` when memory runs out; nothing else fails.
  Misuse (a view added to itself, a released object used) asserts in debug builds.
- **Versioning.** `rk_version()` returns `RK_VERSION` of the library. Before
  1.0 a minor version may change the ABI, as the changelog says.

## Composition

**Window.** `rk_window_create` opens one; `rk_window_show` shows it with a
root controller, as `UIWindow.rootViewController`. A window may draw its own Rekta
title bar (the default) or keep the system's frame.

**Controller.** A controller owns one root view, created lazily by its
`load` callback, and receives `appear` and `disappear` as it comes and goes.
Controllers contain controllers: `rk_places` is a navigation stack (a place
change turns), `rk_panorama` lays sections sideways under a pivot strip,
`rk_pivot` shows facets of one place. `rk_controller_present` shows a sheet
from the top edge; `rk_controller_dismiss` lets it leave the way it came.

**View.** A view has a frame in its parent, a fill from the theme (a role, so
dark mode needs no work), a radius, an opacity, a transform with perspective,
children and a layout: `MANUAL` (frames as set), `COLUMN` and `ROW` (stacks
with gap, padding, alignment and grow), `TILES` (the tile grid: unit, gap,
spans), `OVERLAY`. A custom view is a `rk_view_class` with `draw`,
`measure`, `layout`, `event` and `destroy`, as a `UIView` subclass overrides
`draw(_:)` and `layoutSubviews()`. Built-in views: label, button, tile, rows,
icon, progress; toasts belong to the window.

**Events.** Pointer and key events go to the view under the pointer or with
focus, then up the responder chain: view, superview, its controller, the
window, the app. A handler returns `true` when it took the event. Tab moves
focus in reading order; a view that is pressable nudges as in
[tiles.md](tiles.md) and calls its `activate` handler on release, Enter or
Space.

**Motion.** Changes inside `rk_animate_begin(RK_MOTION_…)` …
`rk_animate_commit()` animate with that motion's duration and easing, as
`UIView.animate` does. A motion says what the change means (motion.md), so the
caller never picks numbers:

| motion | tokens | what animates |
|---|---|---|
| `RK_MOTION_STATE` | duration-short, ease-base | colours, opacity, transforms |
| `RK_MOTION_REFLOW` | duration-moderate, ease-base | frames: every view the layout moves or resizes glides (FLIP) |
| `RK_MOTION_ENTER` | duration-short (tiles moderate), ease-in | views added in the block build in |
| `RK_MOTION_LEAVE` | 0.6 × duration-short, ease-out | views removed in the block leave first, then the rest close up |
| `RK_MOTION_MORPH` | duration-long, ease-base | values of a chart or sparkline |

`rk_view_transition(view, RK_TRANSITION_PEEK | FLIP | CROSSFADE)`, called
before a change, animates from the old look to the new: a tile's new number
peeks in, a new status flips it. Reduced motion zeroes every duration; the end
state is always reached.

## Accessibility

Every view has a role, a label and a value; built-in views set theirs. The
tree is built from the start so that the platform bridges (NSAccessibility,
UI Automation, AT-SPI) only translate it. The bridges come after the first
stage; until then a Rekta app is not usable with a screen reader, and the
changelog says so.

## Status

Stages 1 and 2 are done: the renderer, views, controllers, motion, input, text
fields and areas (clipboard, input methods on every platform), menus, long lists,
tables that sort, resize and edit, undo and focus everywhere; the headless
platform, Wayland and X11 are tested here (193 checks, 21 reference images,
AddressSanitizer and UBSan clean); Win32 and Cocoa are built and tested in CI.

## Stages

1. **Foundation.** Tokens for C, the renderer, views, controllers, motion,
   the headless platform and X11; a demo with a panorama of tiles that press,
   turn, flip and peek, a toast and a sheet. Win32 and Cocoa compile in CI.
2. **Tools.** Text fields (selection, clipboard, IME), menus, tables with
   selection, long lists, focus everywhere, live tiles, undo, Win32 and Cocoa
   complete.
3. **Everyone.** Accessibility bridges, several windows, the
   reference gallery against the web in CI (running: thirteen specimens,
   light and dark, in `test/gallery.mjs`).

## Building

```sh
make -C core            # librekta.a and the demo
make -C core test       # headless tests against the reference images
```

On Windows, `core\build.bat` does the same with MSVC. The library links only
against the system: `user32 gdi32 imm32 dwmapi` on Windows, `Cocoa QuartzCore`
on macOS, `wayland-client wayland-cursor xkbcommon X11 Xext` and `libm` on Linux
(`make WAYLAND=0` for X11 alone).

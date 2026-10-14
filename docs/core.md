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
- **Declared first.** An app says what a window shows from its state; the
  library keeps the views and changes only what differs, with the design's
  motion. The tree underneath stays public: custom views and experiments use
  it directly, and a declaration can hold any view made by hand.
- **Composition as in UIKit.** Windows hold a root controller; controllers own
  views, appear and disappear, contain other controllers and present sheets;
  views form a tree, lay out their children and draw themselves; events travel
  a responder chain.

## Layers

```
app code ──────────────── rekta.h (the ABI)
  declared UI   rk_ui descriptors and macros · reconciler (keys, bound and given values, motion per change)
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
  own; it is named after the thing (`rk_label`, `rk_tile`, `rk_vstack`), or
  `*_create` where the thing is a type of its own (window, view, controller).
  Every call that puts a view or controller somewhere takes that reference
  over (`rk_add`, `rk_insert`, `rk_panorama_add`, `rk_pivot_add`,
  `rk_places`, `rk_places_go`, `rk_controller_present`, `rk_window_show`,
  ...): what goes into the tree belongs to the tree, and app code never
  releases it. A view that already has a parent moves. A window belongs to
  the app until it closes. `rk_add` returns the child, so it can be kept to
  change it later. Everything else is borrowed. `man ./core/man/rekta.3` has
  the whole API by task.
- **Descriptors** that may grow start with `uint32_t size` (set it to
  `sizeof`): `rk_app_desc`, `rk_window_desc`, `rk_view_class`,
  `rk_controller_class`, `rk_action`, `rk_confirm_desc`, `rk_list_item`. A
  newer library reads an older, shorter struct; one whose size is 0 is said
  and ignored.
- **Callbacks** are a function pointer and a `void *user`, never more.
- **Enums** are 32-bit (`_FORCE32` members); new members only at the end.
- **Strings** are UTF-8, NUL-terminated, copied by the callee.
- **Threads.** Everything runs on the main thread, as in UIKit;
  `rk_app_post` is the one function another thread may call.
- **Errors.** Nothing returns `NULL` for want of memory: the library says so
  and aborts, so no app has to check. Misuse (a setter for one kind of view on
  another, a size of 0, a call from another thread, an unknown icon, a
  released object) does nothing and is said through `rk_app_set_log`
  (standard error until set); a view added to itself asserts in debug builds.
- **Tests** have a header of their own, `rekta_test.h`: injected input, taps,
  keys, snapshots, the virtual clock and the frame statistics.
- **Versioning.** `rk_version()` returns `RK_VERSION` of the library. Before
  1.0 a minor version may change the ABI, as the changelog says.

## Declared UI

A window given a `build` function calls it to know what to show. Each macro
(`RK_TILE`, `RK_VSTACK`, `RK_PIVOT`, …) fills an `rk_ui`, a size-prefixed
descriptor like the others, and a container's scope holds its children:

```c
static void today(void *user) {
  RK_PANORAMA() RK_SECTION("today") {
    RK_TILES() RK_TILE(rk_ui_fmt("%d", app.finished), .label = "finished");
    RK_BUTTON("build one", .on_press = build_one);
  }
}
```

- **When it builds.** Handlers only change the app's state. After every event,
  timer, post and close the library hands the app, after a look change, at the
  end of a motion and before each frame, every declared window builds again.
  `rk_ui_update` covers state changed where the library cannot see.
- **Reconciling.** A node is matched with the one the last build made by
  `.key` or `.id`, else by its place among unkeyed siblings of its kind; only
  the fields that differ reach the view, through the same setters an app would
  call. A controller's parts (sections, pages, columns, tree items) come, go
  and move in place.
- **Motion per change.** What goes leaves first (`RK_MOTION_LEAVE`), then what
  comes enters or the rest reflow, then navigation turns or slides; a tile's
  new value peeks in, a new status flips it. `.change` picks another
  transition, `RK_UI_IN_PLACE` none.
- **Values.** Bound by a pointer (`.on`, `.chosen`, `.number`, `.day`,
  `.buffer`, `.open`): the view shows what the app holds after every build,
  and the user's change is written there first. Given (`.value`, `.input`):
  where the view starts, set again only when the declaration moves it.
- **Work.** A build costs O(nodes declared); `.version` skips a scope that has
  not changed, and a list's or table's rows are data-driven, so a million rows
  are one node.
- **Mistakes** are said once through the log: a field the kind does not read,
  a key twice, a scope left open by a `return`, a closed sheet declared open
  again.

The tree below stays the foundation, and public: `RK_HOST` puts a view made by
hand into a build, `rk_ui_view` puts a declared view into a tree made by hand.

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

Stages 1 and 2 are done: the declared UI over the whole view set, the renderer, views, controllers, motion, input, text
fields and areas (clipboard, input methods on every platform), menus, long lists,
tables that sort, resize and edit, undo and focus everywhere; the headless
platform, Wayland and X11 are tested here (544 checks, 36 reference images,
AddressSanitizer and UBSan clean, each module compiling on its own); Win32 and
Cocoa are built and tested in CI.

## Stages

1. **Foundation.** Tokens for C, the renderer, views, controllers, motion,
   the headless platform and X11; a demo with a panorama of tiles that press,
   turn, flip and peek, a toast and a sheet. Win32 and Cocoa compile in CI.
2. **Tools.** Text fields (selection, clipboard, IME), menus, tables with
   selection, long lists, focus everywhere, live tiles, undo, Win32 and Cocoa
   complete.
3. **Everyone.** Accessibility bridges, several windows, the
   reference gallery against the web in CI (running: 26 specimens,
   light and dark, in `test/gallery.mjs`).

## Building

```sh
make -C core            # librekta.a and the demo
make -C core test       # headless tests against the reference images
```

On Windows, `core\build.bat` does the same with MSVC. The library links only
against the system: `user32 gdi32 advapi32` on Windows (the rest is loaded
when first needed), `Cocoa QuartzCore`
on macOS, `wayland-client wayland-cursor xkbcommon X11 Xext` and `libm` on Linux
(`make WAYLAND=0` for X11 alone).

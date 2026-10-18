# Changelog

Rekta Design follows [semantic versioning](https://semver.org/). Classes,
tokens, public hooks, `data-rk-*` attributes, events and the `rekta.js`
API are the public surface. Before 1.0, a minor version may still break
it; each entry says how.

## 0.3.0 — 2026-10-18

### Breaking (rekta-core: declared first)

The declared UI is now the way to write a rekta-core app, and the view tree
the public foundation under it. The experiment's separate `rekta_ui.h` is gone:
everything is in `rekta.h`.

| Was | Now |
| --- | --- |
| `rk_align` began with `RK_ALIGN_START` | it begins with `RK_ALIGN_AUTO` (a column stretches, a row keeps to the start): every value moved by one, so rebuild |
| `rk_confirm` returned nothing | it returns the sheet, so the app can dismiss it before the answer |
| `#include "rekta_ui.h"` | `#include "rekta.h"` |

### Breaking (rekta-core: the API review)

An independent review held rekta-core to modern library practice and to how
it feels to use. Its API is now smaller in concepts and says what it means;
apps change as follows (`man rekta` has the whole API):

| Was | Now |
| --- | --- |
| `rk_button_set_disabled(b, reason)`, `rk_control_set_disabled` | `rk_view_set_enabled` on any view, with `rk_view_set_tooltip` for the reason |
| `rk_menu_set_disabled` | `rk_menu_set_enabled` |
| `rk_menu_set_check(menu, i, -1/0/1)` | an `rk_checkmark`: `RK_CHECKMARK_NONE`, `_OFF`, `_ON` |
| `rk_app_after(ms, repeat, fn, user)` | `rk_app_after(ms, fn, user)` once, `rk_app_every` again and again |
| `rk_app_on_change` returned nothing | returns an id for `rk_app_cancel` |
| `rk_window_do(w, label, run, undo, user, free)` | `rk_window_do(w, &(rk_action){ .size = sizeof(rk_action), ... })`; each window has its own history |
| `rk_window_choose_files(w, accept, bool, ...)` | `RK_CHOOSE_ONE` or `RK_CHOOSE_MANY` |
| `rk_field_set_hint(f, text, error)` | `rk_field_set_hint` and `rk_field_set_error` |
| `rk_view_set_rule(v, top, bottom)` | `rk_view_set_rules(v, RK_RULE_TOP \| RK_RULE_BOTTOM)` |
| `rk_list_selected` | `rk_list_is_selected`; `rk_list_item` starts with `size` |
| `rk_tree_open(t, item)`, item -1 for all | `rk_tree_set_open(t, item, open)`, `rk_tree_set_all_open` |
| `rk_split_show(s, detail)`, `rk_split_sideways`, `rk_split_detail_shown` | `rk_split_set_detail_shown`, `rk_split_is_sideways`, `rk_split_is_detail_shown` |
| a table's sort as a direction 1 or -1 | `rk_sort` (`RK_SORT_NONE`, `_ASCENDING`, `_DESCENDING`); column flags `uint32_t` |
| `rk_column`, `rk_row` (stacks) | `rk_vstack`, `rk_hstack`; "row" is a hairline row, "column" a table's |
| `rk_pivot_add(pivot, controller)` | `rk_pivot_add(pivot, title, view)` as the panorama's; `rk_pivot_add_controller` |
| `rk_progress_set` | `rk_progress_set_value` |
| `rk_view_hidden`, `rk_app_reduced_motion` | `rk_view_is_hidden`, `rk_app_is_reduced_motion` |
| `rk_app_stats(s, reset)` | `rk_app_stats(s)`, `rk_app_stats_reset()` |
| `rk_control_value` of a date as yyyymmdd only | also `rk_date_value(d, &y, &m, &d)`, `rk_date_set_value` |
| `*_COUNT` members of public enums | gone; the library keeps its own counts |
| testing calls in `rekta.h` | in `rekta_test.h` (still exported) |
| `rk_view_find` by name or a button's label | by name; a button's name is its label, or its icon alone |
| a view already in a tree needed `rk_retain` to move | `rk_add` and `rk_insert` move it |
| `rk_window_create` returned a reference | the window belongs to the app until it closes; the pointer is borrowed |
| constructors could return `NULL` | never: out of memory the library aborts |
| apps padded a place's page by hand | places set a page in, as a pivot sets its facets in |

### Added

- **Facets side by side.** From `size.pivot-wide` (a new token, 1300px,
  a product decision) a pivot's facets show side by side as panorama
  sections; the strip is a jump list that glides to a facet and follows the
  scroll (`pivot()`, `data-rk-wide`). Narrower, one facet at a time again.
- **Product overrides** are documented as the way to make Rekta Design a
  product's own (`docs/platforms.md`): the accent per theme, the toast and
  stale timings, the tile unit, `size.pivot-wide`, density as a user
  setting, and what must not be overridden.
- **rekta.js speaks the product's language.** Every word it shows or says
  comes from `Rekta.strings` (English by default; `start({ locale,
  strings })`, `setStrings()`, `setLocale()`), chart figures and live
  times follow the locale (`number()`, `locale()`), and a German catalogue
  ships as `rekta.strings.de.js` (`?lang=de` in the showcase). `--check`
  fails on a phrase written outside the catalogue.
- **A domain's colour code.** A product may set `--rk-series-1` to `-8`
  per theme for colours its users already read as information (heart-rate
  zones, say): small marks only, never text or a tile; `chart()` series
  take `code: N`, `.rk-tag[data-rk-code]` a coded square;
  `build.mjs --check-product` holds the values to 3:1 and to colour-blind
  separation. The showcase shows five heart-rate zones.
- **Places in the app bar on a phone** (`.rk-ab-places`, opt-in, up to
  five): the top-level places in the thumb's reach, commands in "more",
  back first, the router marking the place you are in.
- **Facets side by side** keep the chosen one while it is whole in sight,
  and Tab from the strip goes into it.
- **Smaller additions:** `.rk-ab-mid` (the app bar's middle), `.rk-statement`
  (a statement block on the ground), the message tile's length limit,
  `--rk-toast-left` (the countdown line, public), `data-rk-reveal` (the way
  out of `hidden`), `data-rk-measured`, a row sparkline's colour and size.
- `rekta.js` imports without a DOM, so pages that render on the server can
  import it; `--check` imports it in plain Node.
- `build.mjs --check` reports the statuses' separation under tritanopia
  (not a failure; `docs/accessibility.md` says why), and fails when the
  version in `package.json`, `rekta.js`, rekta-core, the flake and the
  newest changelog heading disagree.
- **rekta-core: declared UI.** A window given `rk_window_desc.build` (with
  `.user`, and `.on_key` for keys nothing focused takes) calls it to know what
  to show: macros for every view, collection, menu and controller (`RK_TILE`,
  `RK_VSTACK`, `RK_TABLE` with `RK_COLUMN`, `RK_TREE` with `RK_ITEM`,
  `RK_PLACES`, `RK_PIVOT`, `RK_PANORAMA`, `RK_SHEET`, `RK_CONFIRM`, `RK_HOST` for
  a view made by hand) fill `rk_ui` descriptors. Every declared window builds
  again after each event, timer, post and close the library hands the app, and
  only what differs reaches the views, with the motion each change means: what
  goes leaves first, what comes enters, the rest reflow, a place turns, a tile's
  new value peeks in. Nodes are matched by `.key` or `.id` (`RK_ID(n)`), else by
  place. Bound values (`.on`, `.chosen`, `.number`, `.day`, `.buffer`, `.open`)
  show what the app holds after every build; given ones (`.value`, `.input`)
  are set again only when the declaration moves them. `.version` skips
  unchanged scopes, `.change` picks a transition (`RK_UI_IN_PLACE` for none),
  `rk_ui_fmt` formats text for a build, `RK_OPTIONS` passes a static array,
  `rk_ui_view` and `rk_ui_update` join declared views and trees made by hand.
  Fields a kind does not read, duplicate keys, scopes left by a `return` and
  sheets the user closed but the build opens again are said through the log.
  The demo, `hello.c` and `navigation.c` are declared; the demo is about a
  fifth shorter and draws its storm phase as fast as before.
- **rekta-core: every property changes in place.** Setters for what could only
  be set when made: `rk_label_set_style`, `rk_button_set_label`,
  `rk_control_set_label`, `rk_control_set_options`, `rk_control_set_range`,
  `rk_field_set_label`, `rk_field_set_placeholder`, `rk_icon_set`. Parts change
  where they stand: `rk_panorama_insert`, `_remove`, `_set_title`,
  `_set_width`; `rk_pivot_insert_controller`, `rk_pivot_remove`;
  `rk_table_insert_column`, `_remove_column`, `_set_column`; `rk_tree_move`;
  `rk_view_reorder` puts children in order without taking them out, so focus
  stays. Hooks for what the user did: `rk_places_on_back`,
  `rk_pivot_on_change`, `rk_controller_on_dismiss`, `rk_split_on_back`.
- **rekta-core: from the API review.** Disabled views with a reason: any view
  can be disabled (`rk_view_set_enabled`), greying it and everything in it,
  and a tooltip (`rk_view_set_tooltip`) floats after `tip-delay` or on keyboard
  focus. Misuse is said instead of passing silently or corrupting memory
  (`rk_app_set_log`): a setter on the wrong kind of view, a descriptor without
  its size, a call from another thread, an unknown icon, a pointer that is no
  object or already freed. Lists keep their selection by item
  (`rk_list_set_key`) and say it (`rk_list_selection`, `rk_list_active`,
  `rk_list_select`, `rk_list_clear_selection`); rows change in place
  (`rk_row_set`), menus are built again (`rk_menu_clear`), tree items are
  removed, renamed and carry the app's data. `rk_controller_embed` puts a
  controller inside another's views; `rk_confirm` asks a question in a sheet;
  `rk_field_set_secret` makes a password field; custom views move with the
  design's motion (`rk_view_animate_value`, `rk_view_value`) and set the
  pointer (`rk_view_set_cursor`). No fixed limits remain (10,000 tiles, 600
  children, nesting animation blocks); a relayout draws only what changed and
  measures each view once per pass; each source compiles on its own
  (`make modules`, in CI). The keys F1 to F24 and Insert arrive on every
  platform.

- **The reference gallery**: `core/test/gallery.c` and `test/gallery.mjs`
  draw the same thirteen specimens (button, checkboxes, toggles, segments,
  fields, slider, progress, rows, a tile, a drop place) with rekta-core and with
  rekta.css, light and dark, and CI holds them to each other: the frame's
  size, where the ink lies, the solid colours, and the shape at a quarter of
  the size. A report shows each pair side by side. The first run found four
  places where core had drifted from the specification, fixed below; the web's
  checkbox, the browser's own at first, is now drawn as core draws it.
- **rekta-core: packaging**: `make shared` builds `librekta.so.0.3`
  (`librekta.0.3.dylib`, `rekta.dll`; `build.bat shared` with MSVC), built with
  hidden visibility so it exports only the header's `RK_API` functions;
  `make abi` holds it to that and runs a program against it. `make install`
  adds the shared library and `rekta.pc`; `core/test/install.sh` installs into
  a fresh prefix and builds an app through pkg-config, shared and static. The
  Nix package installs the same.
- **rekta-core: notifications** (`rk_app_notify`): a title and a body from the
  system. On Linux they go to `org.freedesktop.Notifications` over D-Bus from a
  thread of their own, with the body escaped for servers that read markup and
  the app's id as the desktop entry; on Windows a balloon (a toast on 10 and
  later) from an icon in the notification area; on macOS through
  `UNUserNotificationCenter`, loaded at run time, for apps in a bundle. Tested
  end to end on Linux by `core/test/platform/desktop.sh`.
- **rekta-core: X11 follows the scale while the app runs**: when the desktop
  changes `Xft.dpi`, each window keeps its logical size in the new number of
  pixels (`core/test/platform/scale.sh`). X11 has one scale for the screen; no
  desktop publishes one per monitor, so there is none to follow.
- **rekta-core: files dragged in, and a place to drop them** (`rk_drop`), as
  the web's `.rk-drop`: a 2 px dashed text-medium edge that turns solid, in the
  text colour over the hover fill, while files are over it; a filter by
  extension or family (`image/*`); the app hears the files taken and those
  refused. New events `RK_EVENT_DRAG_ENTER`, `_DRAG_MOVE`, `_DRAG_LEAVE` and
  `_DROP` (with `files`, `nfiles` appended to `rk_event`): the view under the
  pointer that takes `DRAG_MOVE` gets the drop, and files dropped where none
  does are refused. `rk_window_choose_files` asks the system's file chooser,
  filtered as the drop place is, and answers later; a click on the drop place
  opens it. On Linux it asks the desktop portal over D-Bus (libdbus looked up
  at run time), else zenity or kdialog (tested end to end by
  `core/test/platform/chooser.sh` against a stand-in portal); on Windows the open
  dialog (comdlg32 looked up at run time); on macOS an `NSOpenPanel` as a sheet. Files come in on every platform: XDND 5 on
  X11 (tested end to end under Xvfb by `core/test/platform/x11.sh`), the data
  device on Wayland, an OLE `IDropTarget` on Windows (ole32 and shell32 looked
  up at run time) and `NSDraggingDestination` on macOS.
- **rekta-core: rows the user puts in order** (`rk_rows_set_sortable`), as
  the web's `sortable()`: a grip on each row; dragged 4 px a row lifts (bg and a
  1 px text-medium edge, no shadow) and follows, the others step aside over
  `duration-short`; let go, every moved row glides to its place over
  `duration-moderate`. Space picks a row up, Up/Down move it, Space puts it
  down, Escape puts it back, Alt+Up/Down moves it at once; `rk_rows_move`.
- **rekta-core: a split** (`rk_split`), as the web's `.rk-split`: sideways
  (760 px wide in a window 481 px tall or more) a list beside its detail with a
  hairline and a 9 px handle that drags the list's width (pane-min each at
  least, four tiles at first, Left/Right by `space-m`, Home/End, a double click
  resets; the app remembers the width through `rk_split_on_resize`); a new
  item's detail slides in. Compact one pane at a time, turning as a place does,
  the detail with a crumb back; Escape goes back too.
- **rekta-core: a tree** (`rk_tree`), as the web's `.rk-tree`: rows at the
  density's height, a level indented by `space-m`, a chevron that turns as a
  branch opens over its height (`duration-short`, closing in 0.7 of it), the
  selection as the accent fill. The web's keys and pointer: Right and Left open
  and close or go in and up, `*` opens the siblings, a letter jumps; a click on
  the chevron or a double click opens or closes. A key that moves the active row
  of a tree or a list now shows the focus ring, as after Tab.

- **rekta-core** (`core/`): Rekta Design as native views in C for
  Windows (Win32), macOS (Cocoa) and Linux (Wayland first, Xlib as the
  fallback), with no dependency
  beyond the system and a vendored `stb_truetype.h`. One header
  (`core/include/rekta.h`) with a clean ABI: opaque reference-counted
  objects, size-prefixed descriptors, 32-bit enums, callbacks with a user
  pointer. Windows, controllers and views composed as in UIKit; column,
  row, tile-grid and overlay layouts; implicit animations named by their
  meaning (`RK_MOTION_STATE`, `REFLOW`, `ENTER`, `LEAVE`, `MORPH`) and
  transitions (`PEEK`, `FLIP`, `CROSSFADE`); the press, the turnstile,
  panoramas with a gliding strip and scrolling sections, pivots, sheets,
  toasts, the title bar, keyboard focus. A software renderer with exact
  anti-aliasing draws the same pixels on every platform; a headless
  platform with a virtual clock runs the tests (73 checks, 14 reference
  images) and the demo's tour. Stage 1 of docs/core.md: text fields,
  menus and tables come next, screen-reader bridges after them.
- rekta-core on Linux speaks Wayland natively: shared-memory buffers with
  damage and frame callbacks, resizes acknowledged and drawn in the same
  commit (the content keeps up with the edge, clicks land where they
  should), its own title bar and resize edges, fractional scales,
  cursor shapes, xkbcommon keys with the seat's repeat. X11 stays as the
  fallback (`RK_PLATFORM=x11`); its resize draws once per burst and keeps
  its shared memory while the window changes size.
- rekta-core shows pictures: `rk_image(path)` and
  `rk_image_from_memory` read PNG and JPEG through a vendored
  `stb_image.h` (v2.30; GIF, HDR and files bigger than 16384 px left
  out). The size comes from the header at once; a background worker
  decodes - and, for a picture over a million pixels, scales - away from
  the UI thread, so a frame never waits for it (a 2400 x 1600 JPEG: the
  slowest frame 3.7 ms). Scaled with a tent filter that averages every
  pixel it shrinks, kept per size; contain or cover; it fades in where a
  placeholder stood. A headless run does the worker's jobs at once, so
  tests draw the same every time.
- rekta-core has a scroll view: `rk_scroll(content)` clips its content
  and scrolls it on the panorama's spring - the wheel until an end, then
  what holds it; the keys a focused view leaves; focus landing inside
  brings it in sight (`rk_view_reveal` does so on request). A 4px bar
  shows while it moves and drags.
- rekta-core has a date field: `rk_date(year, month, day)` shows
  2026-10-13 and holds 20261013. Year, month and day from the keys (Up
  and Down keep the day in its month, digits type it), or a day from
  the month that opens below it, Monday first, today ringed and the
  chosen day in the accent, with the keys of the web's date pickers.
- rekta-core has a select and a combobox: `rk_select(options, count,
  chosen)` opens a menu of its options below it, the chosen one marked
  (Up and Down change it in place, a letter jumps); `rk_field_set_options`
  makes any field a combobox, whose list of matching options floats
  below the input without taking focus - typing, input methods and undo
  stay the field's own.
- rekta-core has sliders and steppers: `rk_slider(min, max, step,
  value)` (a 6px track the accent fills, an ink bar as the thumb, the
  value beside it in tabular figures; a press moves it, a drag follows)
  and `rk_stepper` (minus, the number, plus; a held press repeats, a
  typed number and Enter take it, Escape puts it back). Both snap to
  their step, and the app's values snap too.
- rekta-core has controls that hold a value, as the web's: `rk_check`,
  `rk_choices` (one stop for Tab, the arrows choose), `rk_toggle` (the
  Metro frame, accent fill and ink thumb, the state word beside it) and
  `rk_segments` (the accent block slides to a new choice). One model for
  all: `rk_control_value` / `set_value` / `on_change` / `set_disabled`.
  The demo's settings are segments, with a checkbox, options and a toggle
  for notifications.
- rekta-core sets every script Barlow lacks a glyph for, from another
  face: one the app brings (`rk_app_add_font`) or one the system offers -
  fontconfig on Linux and CoreText on macOS, both opened at run time so
  nothing more is linked (Linux adds `-ldl` for older glibc), and the
  system's fonts on Windows in an order that suits the user's language.
  Faces are mapped, not read, and which face draws a character is
  remembered. Lines break between CJK characters, never before closing
  punctuation or after opening, in labels and fields alike. Arabic,
  Hebrew and the Indic scripts still wait for shaping.
- rekta-core: table cells edit in place. `RK_COLUMN_EDITABLE` and
  `rk_table_on_edit(table, commit, user)`: a double click or F2 opens a
  field as tall as the row over the cell, its old value selected; Enter
  or Tab keeps the value (Tab goes on to the next editable cell, Shift+Tab
  back), Escape puts the old one back, a click elsewhere keeps it.
  commit() returns NULL or why not; the reason shows below the cell,
  which stays open. A view in the focus chain may now take Tab before
  focus moves. The demo's builds table has a note column whose edits go
  through the undo history.
- rekta-core: table columns change width. A column's edge in the head
  drags it (48px at least), a double click gives it its own width back,
  `rk_table_set_column_width` / `rk_table_column_width` let the app set
  and remember it. The head works from the keyboard: Up from the first
  row goes in, Left and Right move, Enter sorts, Ctrl+Left/Right resize,
  Down returns. The pointer takes the shape of what is under it - an
  I-beam over text, the column edge's - on every platform (cursor-shape
  or the theme on Wayland, font cursors on X11, WM_SETCURSOR, NSCursor).
  A list now shows focus on its active row only, not around itself too.
- rekta-core: input methods on every platform. X11 takes the best style
  the input method offers - on the spot (the field shows what is being
  composed), over the spot (the input method draws it at the caret), or
  none - and places its window at the caret between compositions (uim
  drops a composition whose attributes change inside it); LC_CTYPE comes
  from the environment when the app left it at C, as GLFW does, or XIM
  finds no input method. Windows: IMM32, loaded at run time, so the link
  line stays as it was: the composition and candidate windows at the
  caret, the composition shown by the field, the result typed. macOS:
  the view is an NSTextInputClient while a field has focus. Checked on
  X11 against uim-xim with Byeoru (한글, both styles).
- rekta-core: focus everywhere. A toast holds while focus or the
  pointer is on it, Escape dismisses it and focus goes back where it
  came from (over a sheet too, its action is last in the Tab order); with
  nothing focused taking them, Up and Down scroll the section in sight;
  Home and End in a strip of titles. A test walks a window of every
  control and fails on anything that takes input but not focus.
  rekta(3) has a Keys section.
- rekta-core has text areas: `rk_field_set_lines(field, lines)` makes a
  field show several lines, as the web's textarea rows. The text wraps
  at the input's width (at spaces; inside a word only when it is wider
  than the line), Enter breaks a line, Ctrl+Enter submits, Up and Down
  keep their column, Home and End their line, the wheel and the caret
  scroll it. A one-line field is the same code with one line.
- rekta-core has undo, as the web's `undoStack()`: `rk_window_do(window,
  label, run, undo, user, free_user)` runs an action and keeps it (the
  last 50); its toast offers undo, an undo's toast offers redo; Ctrl
  (Cmd) and Z undoes, with Shift or Ctrl+Y redoes; a new action drops
  what was undone. Text fields keep a history of their own, a word at a
  time. The demo's archive button goes through it.
- rekta-core has tables, as the web's `.rk-table`: `rk_table(count, cell,
  user)` is a long list with columns (every `rk_list_` call works on it),
  drawing only the rows in sight; `rk_table_add_column` with a width and
  `RK_COLUMN_NUMERIC` / `RK_COLUMN_SORTABLE`; a head that stays put, a
  click on it sorts ascending then descending through `rk_table_on_sort`
  (the app sorts its rows). The demo has a sortable table of builds.
- rekta-core has menus, as states.md's floating surfaces: `rk_menu`,
  `rk_menu_add` (a verb, a key hint, a handler), separators, group labels,
  items with a square that toggles and keeps the menu open, disabled
  items that say why. `rk_view_set_menu` attaches one: a button opens it,
  any other view on a right click or Shift+F10. Below the opener, above
  where there is no room, never outside the window; a fade and a 4px
  shift over duration-float; the keys and the press outside as on the
  web.
- rekta-core has text fields: `rk_field(label, placeholder)` as the web's
  `.rk-field` (label above, 1px border, a hint or a marked error below),
  one line, edited with the desktop's keys and the pointer, Enter
  submits. The caret blinks for ten seconds after an edit and then stays
  lit, so a field left alone wakes nothing. Copy and paste use the
  system clipboard: CF_UNICODETEXT on Windows, NSPasteboard on macOS, the
  CLIPBOARD selection on X11, wl_data_device on Wayland. On Wayland, dead
  keys and the Compose key compose (xkbcommon), and input methods work
  through text-input-v3: what they compose shows underlined at the caret
  (`RK_EVENT_COMPOSE`), their window follows the caret.
- rekta-core has long lists: `rk_list(count, item, user)` draws only the
  rows in sight and keeps nothing for each item (the callback fills a row
  while it is drawn), scrolls on the glide, and selects the Metro way
  (click shows, Ctrl+click and Shift+click select several, Escape ends it,
  keys as the web's `virtual()`). The demo lists a million builds.
- rekta-core draws a frame's changes as up to eight separate parts and
  presents them together (several damage rectangles in one Wayland
  commit); an app with nothing moving uses no CPU and never wakes. On
  Wayland a frame is drawn straight into the buffer the compositor shows
  (no copy of the frame, no framebuffer of its own): the demo needs 13 MB
  instead of 18.
- rekta-core's API is narrower and says who owns what in one rule:
  constructors are named after what they make (`rk_label`, `rk_button`,
  `rk_tile`, `rk_column`, `rk_row`, `rk_tile_grid`, `rk_rows`, `rk_icon`,
  `rk_progress`, `rk_panorama`, `rk_pivot`, `rk_places`) and return a
  reference; adding, showing and presenting take it over (`rk_add` and
  `rk_insert` return the child). `rk_button(label, on_press, user)` takes
  its handler, `rk_button_set_kind` and `rk_button_set_icon` the rest; a
  button keeps its own width in a column. `rk_window_show(window, root)`
  replaces `rk_window_set_root`; `rk_view_find` finds a button by its label;
  `rk_view_tap` and `rk_window_key` join the testing calls. A man page,
  `core/man/rekta.3`, explains the API by task, with
  `core/examples/hello.c` as its example (built and run by `make test`,
  checked against the copies in the man page and the README). Breaking:
  `rk_view_add`, `rk_view_insert`, `rk_window_set_root` and the
  `rk_*_create` constructors of built-in views are gone.
- rekta-core scrolls and glides smoothly: a wheel step, a section from
  the nav strip or the keyboard travels on a quintic that sets off and
  comes to rest without a jolt, and a new step picks it up mid-way with
  its speed (quick wheel steps go further). The strip's titles slide and
  change size the same way. Frames follow the display: on Wayland the
  compositor's frame callbacks pace them (any refresh rate), elsewhere
  the display's rate (`RK_FPS` overrides).
- `core/demo/demo.c` shows every feature of the core in six sections;
  `flake.nix` builds it on Linux and macOS (`nix run`, `nix develop`,
  `nix flake check` with the tests).
- The token build also writes the C sources of rekta-core
  (`core/src/rk_tokens.h`, `rk_icons.h`, `rk_fonts.h`); `--check` fails
  when they are out of date.

- Inputs, native where they can be: `select.rk-input` with a chevron;
  number, date and time fields in Rekta's frame (the date and time
  pickers' icons from Rekta's set); a combobox that filters as you type
  (`rk-combo`, `Rekta.combobox()`); the Metro toggle switch
  (`rk-toggle`: a square frame, the accent fill, an ink bar as the
  thumb, the state word beside it); a stepper (`rk-stepper`,
  `Rekta.stepper()`); a slider (`rk-range`, with an `<output>`);
  segments (`rk-segments`); a drop zone for files (`rk-drop`,
  `Rekta.drop()`).
- Toasts take a plain action without a countdown (`act`).
- Desktop: tooltips in ink (`data-rk-tip`, after `timing.tip-delay`, at
  once on keyboard focus, never on touch); context menus at the pointer,
  on a long press or Shift+F10 (`Rekta.contextMenu()`); a tree
  (`rk-tree`, `Rekta.tree()`); ordering by a grip, with the pointer or the
  keyboard (`rk-grip`, `Rekta.sortable()`); long lists where only the rows
  in view exist (`rk-virtual`, `Rekta.virtual()`); table columns that size
  and hide (`columns`), and cells that edit in place, with refusals below
  the cell (`edit`).
- Undo history: `Rekta.undoStack()`, Ctrl/Cmd+Z and Ctrl/Cmd+Shift+Z
  after the toast has gone, redo in the undo's toast.
- The live contract: `Rekta.live()` says how fresh a region is, goes
  stale (`timing.stale`) or offline and says so, and holds still while
  you look, with waiting updates behind a button. [docs/live.md](docs/live.md).
- Tokens `timing.tip-delay`, `timing.long-press`, `timing.stale`.
- Data: line and bar charts in ink and grey with one axis, a tooltip
  and crosshair at every point, keys, a live region and a table of the
  numbers (`Rekta.chart()`); meters that take a status colour only when
  their value means one (`rk-meter`, `--rk-v`); an indeterminate
  `rk-progress`; Metro's progress dots for a wait that blocks
  (`rk-dots`, `motion.duration.dots`). [docs/data.md](docs/data.md).
- The app shell: fifty-one icons on a 20px grid (`src/icons/`,
  `dist/rekta.icons.svg` as a sprite, `dist/rekta.icons.json` for other
  platforms, `rk-icon`, `Rekta.icon()`); the title bar of an installed
  app or a native shell (`rk-titlebar`, `Rekta.titlebar()`,
  `data-rk-titlebar`); installing and offline (`Rekta.pwa()`: the service
  worker, a "reload" toast for a waiting version, `install()`).
  `rk:placechange`. [docs/shell.md](docs/shell.md).
- The showcase is an installable app that works offline, with its own
  title bar in a window-controls overlay.
- Data states: `rk-region` with `aria-busy` (a 2px line after
  `busy-delay`, at least `busy-min`), still placeholders `rk-ghost`, and
  `rk-empty` for empty and failed regions. `Rekta.busy()`.
- Pending actions: `Rekta.pending()` keeps the button's width, says what
  it does ("saving…") and runs the line under it.
- Floating surfaces (`rk-float`: bg, 1px edge, no shadow) and menus
  (`rk-menu-item`, `-sep`, `-label`, `-key`), `Rekta.menu()` and
  `Rekta.place()`.
- Tokens `timing.busy-delay`, `timing.busy-min`, `motion.duration.busy`,
  `motion.duration.float`, `layer.float`.
- [docs/states.md](docs/states.md): data states, pending, floating
  surfaces, layers.
- Tables (`rk-table`): kicker head that sticks, sorting with FLIP
  (`rk-sort`, `aria-sort`), numbers in `rk-num`, rows that stack as
  "label value" in a narrow place. `Rekta.table()`.
- Selection, the Metro way: `Rekta.select()` on any list or table body.
  A click selects one item and shows it; Ctrl/Cmd+click adds and starts
  the selection mode, in which every click adds or removes; Shift takes
  a range, Ctrl/Cmd+A all, Escape ends it. A pressed item nudges where
  it is pressed (`motion.transform.row-tilt`, `row-press-scale`); a
  selected one takes the accent fill and a checkmark without a box
  (`rk-check` cell, `rk-mark`). In the mode the app bar carries the
  count and actions (`rk-ab-selection`, `rk-ab-count`, `data-rk-clear`,
  `data-rk-selecting`). `rk:selectionchange`.
- `data-rk-press`: anything can nudge like a tile.
- Split views (`rk-split`): a hairline with an invisible, focusable
  handle sideways (`--rk-split-w`, `size.pane-min`), one pane at a time
  compact. `Rekta.split()`. The shown item is the selected one.
- Items in addresses: `#place/item`, `places().onItem()`, `open()`;
  replaced sideways, a history step compact; deep links put the list
  underneath.
- `tokens.px()` reads a length token in px.
- Go to: `rk-palette` rises from the app bar at every width (Ctrl/Cmd+K,
  "go to"), lists places, sections, items and commands, filters as you
  type. `Rekta.palette()`. Keyboard shortcuts behind `?`; `rk-key`.
- App-bar overflow: controls that do not fit move into "more"
  (`rk-ab-more`, `data-rk-priority`). `Rekta.appbar()`. App-bar controls
  no longer shrink.
- Search and filters: `rk-filters`, `rk-search`, filter buttons that say
  what they keep (`menu(…, { summary })`), `rk-btn--quiet`; parameters
  in the address (`setParams`, `params`). Checkbox items leave a menu
  open.
- Forms: `Rekta.form()` checks late and corrects early, sums up errors in
  `rk-errors` (focused, with links to the fields) and shows server
  refusals at the field or for the form; `rk-optional`.
- Leaving unsaved input asks first: `Rekta.guard()` and `router.guard()`
  (links, back, Escape and the browser's own steps), plus the browser's
  question on closing.
- `Rekta.confirm()`: a sheet with the safe answer first.
  `Rekta.disable()`: a disabled control says why, beside it.
- [docs/forms.md](docs/forms.md): checks, refusals, leaving, dialog
  levels, disabled reasons.
- [docs/collections.md](docs/collections.md).
- The showcase site (`site/`) replaces the Rekta Board demo: every
  section a live example, generated from the tokens at build time
  (`{{token}}`, `<!--@block-->`), with the tokens as a place of their
  own and the components as a table.

- Motion that follows a change of content, each with one meaning
  ([motion.md](docs/motion.md)): `Rekta.reflow()` (a tile resized, the
  grid re-flowing around it), `Rekta.enter()`, `Rekta.leave()` and
  `Rekta.presence()` (rows and tiles that come and go: filters, archive
  and undo, live events, files), `Rekta.tileUpdate()` (a live tile's new
  number peeks in, a new status flips it), `Rekta.sparkTo()` and
  `chart.update()` morphing to new figures, `Rekta.crossfade()` for the
  theme and accent. Sheets leave the way they came; the app bar's
  commands slide when the selection mode starts and ends; checkmarks draw
  in after the nudge; the segments' accent block slides; tree branches
  open to their height; a column put back to its width glides; error
  summaries, a cell's refusal and waiting live updates arrive; progress
  values glide.

### Changed

- **Rekta's view-transition timing applies to its own cross-fades only.**
  `crossfade()` and a theme change through `prefs` mark the root
  `data-rk-crossfade` while they run; an app's own root view transitions
  keep their timing whatever the stylesheet order. The rules that reach
  beyond `.rk-*` are listed in `docs/behaviour.md`.
- `panorama()` gives the panorama `role="region"` when it has no role, and
  warns in the console when it has no accessible name.
- **Checkboxes and radios on the web are Rekta's own** (`.rk-switch`), no
  longer the browser's with `accent-color`: a 20 px square with a 2 px frame,
  checked the accent with an on-accent mark; a radio a ring, checked an accent
  ring around a dot; pressed, the hover fill around it. The same in every
  browser and as rekta-core draws them; the gallery holds them to it with no
  exception left. Forced colours get the system's controls back.
- **rekta-core, as the web**: buttons and segments are 4 px wider each (the
  2 px frame is outside the padding) and their outer corners take `radius-s`, the chosen
  one's frame in the accent; a row's rule is outside its padding, the last row
  has one below as well, and the meta stands on the title's baseline; in a
  compact window (under 760 × 481) tiles stretch to fill the row, at least two
  columns, as they do in a compact place on the web.

- **Reordering takes `duration-moderate`** everywhere, as motion.md
  says (a sortable list used `duration-short`).
- **The accent marks the selection.** Selected rows take the accent fill
  with on-accent text (was the surface fill and a checkbox).
- **The pivot glides by place, not to the edge.** A strip that does not
  fit moves by the active title's place in the order, so the titles
  before it stay in reach; the active title is always whole on screen.
  `rk-pivot-end` is gone: remove it from your markup.
- **A top-level strip is the place's title.** `rk-pivot--nav` takes the
  nav level sideways and the facet level compact, where it sticks. The
  showcase drops its brand line; the strip opens on "rekta". Its
  sections drop the kickers that repeated the strip's titles.
- **rekta-core: a pivot at the top is the place's title.** A pivot with no
  pivot or panorama above it takes the nav level, as `rk-pivot--nav` does on
  the web, and its pages start at the strip's indent. `examples/navigation.c`
  draws one app with its places either way, and `--film` writes its motion
  frame by frame.
- **Heft sets the order, and nothing settles after the turn.** A part's heft
  (0.8 × (area / tile)^0.35, 0.5 to 1.4) sets when it starts as well as how long
  it takes: light parts first, heavy ones later; parts that weigh the same
  follow one another in reading order, half a `duration-stagger` apart (six
  steps at most). Arriving parts land with a
  trace of speed left and are let go in the frame they land. A turning part's
  layer starts on a whole pixel, so it rasterises exactly as the part will rest:
  rekta.js moves a part with a fill or line onto its resting pixels and grows a
  text-only part's box to the pixel without moving its text; the core draws a
  layer's content at its sub-pixel offset and lands the layer on whole pixels
  (fades too), and draws a view directly once its turn moves no corner by a
  quarter pixel. Before, parts shifted by a pixel and sharpened a moment after
  they stopped.
- **The turn swings more and takes half the time.** Leaving parts swing 60°,
  gathering speed, and stay whole for 40% of their time before they fade;
  arriving parts swing in from 70°, quick and without a long creep. Heft runs
  from 0.6 to 1.1 (a tile about 0.85), at most six rows are staggered, and the
  next place starts when the last part is 75% through: a change takes about
  0.8s instead of 1.4s. Tiles that turned in no longer rise a second time
  after the turn (they seemed to settle). Web and core alike.
- **Web: the turn has heft, and a list down the side.** rekta.js turns every
  part of a place by itself, leaving and arriving, in a time that follows its
  size (`--_rk-turn-duration`); a scroller lets its parts turn from its edge.
  `rk-pivot--side` in an `rk-side` lays a pivot's titles down the side, the
  title in sight stepped in by `space-m`; compact, it is the strip again. A
  pivot with `rk-pivot--nav` turns its panels as a place change does.
- **rekta-core: a pivot down the side.** `rk_pivot_set_layout(pivot,
  RK_PIVOT_SIDE)` lays the titles out as a list down the leading side, an equal
  alternative to the strip along the top, as tabs are in a browser. The title
  in sight steps in by `space-m` and darkens over `duration-pivot-size`; Up and
  Down pick a title. A window narrower than 760 px or lower than 481 px keeps
  the titles along the top, as the web's compact class does.
- **rekta-core: a pivot that picks the place turns it.** Its facet turns away
  and the next turns in, as a place change does; a pivot inside a place still
  slides.
- **rekta-core: the turn has heft.** Each part of a place turns by itself (a
  plain layout box no longer turns as one block), in a time that follows its
  size: a line of text in 0.6 of the token's duration, a tile in about the
  token's, a table in up to 1.4. Arriving, the parts turn in one by one, a row
  at a time, instead of the place swinging in whole.

### Fixed

- A pivot without `rekta.js` keeps even gaps between its titles (they were
  32, 155, 56px), and measuring no longer animates the margins.
- The app bar's "more" menu is placed by the stylesheet too, and opens from
  its control's trailing edge with the script; on a phone the status
  banner takes 105px instead of 182px.
- The docs now agree with the code on building in after a turn, the
  arrival window, new data in charts, the next item in the same place, and
  the wide pivot's keys; toasts stay below sheets and there is one control
  size per density, both documented.
- Secondary text inside a status tile or banner (a meta, a facts line, an
  age, a stale value) fell below AA, to 1.9:1 on the light error tile; it
  now takes the primary text colour there.
- The docs agree with the tokens and the code again: the app bar's height,
  the tightest AA pairs, the navigation title levels, the pivot's motion
  tokens, `layer.float`.
- rekta-core, found while building the declared UI: a menu open when its
  window closed kept the window alive (they now close with it); a sheet
  destroyed while it had focus kept the view focused before it; a controller
  destroyed before the controllers it held left them pointing at it; a place
  turning with nothing in it passed `NULL` to `qsort`; a sheet presented again while
  it was leaving lost its new place; a place's turn treated a page and the
  pivot inside it as one block.
- rekta-core: the rasterizer gave the first pixel of a slanted edge crossing
  the clip's left border the wrong coverage, so a rounded corner drawn in part
  did not match the whole; edges are now split at the clip border. Found by
  the new test that holds every partial redraw to a full one.

- rekta-core: a timer overdue when the loop went to sleep (a frame that took
  longer than the timer's gap) put the loop to sleep until the next event;
  it now runs at once.

- rekta-core: a field given its text before it was laid out showed only the
  end of it; it now starts at the start once it has a width.

- A context menu closed on any scroll in the page, so a strip gliding to its
  active title closed one just opened; it now closes when a scroll moves
  what it was opened on.

- rekta-core: removing a view that is already leaving (two toasts in one
  frame) freed it under its own animation; it now keeps leaving.

- The classic script carries `async` functions; `--check` now fails
  when `rekta.global.js` does not parse.
- The sticky section strip no longer shrinks in a place and clips the
  descenders of its titles.

## 0.2.0 — 2026-10-11

The foundation for desktop apps and responsive web apps (including PWAs
on phones), from the UX review of 0.1.

### Breaking

- **One token source, one prefix.** `tokens/rekta.tokens.json` is the
  only source; every web custom property is `--rk-<name>` (was
  `--rekta-color-…` and unprefixed names). `scripts/build-css.mjs` is
  replaced by `scripts/build.mjs`. See the naming table in
  [docs/platforms.md](docs/platforms.md).
- **Sizes in rem.** Type, control, tile and app-bar sizes follow the
  user's text size.
- **State in attributes.** Script state moved from `is-*` classes to
  `data-rk-*` attributes (`data-rk-pressed`, `-turn`, `-phase`,
  `-arriving`, `-fit`, `-state`).
- **Tiles never change data.** A tile opens a place or a sheet, or only
  informs (W17).
- **Build-in only on arrival.** Tiles build in and sparklines draw only
  under `data-rk-arriving`, never on a re-render.

### Added

- `dist/rekta.tokens.json`: resolved, platform-neutral values (logical
  px, ms, hex, Bézier arrays) grouped by theme and density, for other
  desktop frameworks.
- `dist/rekta.js` and `dist/rekta.global.js`: framework-free behaviour
  (press, turnstile primitives, places router, panorama, pivot, toaster,
  banner, sheet, prefs, layered keys). See
  [docs/behaviour.md](docs/behaviour.md).
- Routes and back (D1): an address per place, one history entry per
  place change, one back for the app bar, crumb, Escape and the system;
  cold entries put the parent underneath; focus and scroll restored.
- `rk-crumb` and parent places (K6).
- Window-size classes (D8): sideways vs compact by the place's width
  (container queries) and the viewport height; re-align on change.
- Wheel latch (D7): one wheel gesture, one scroller.
- Keyboard model (D2) and key layering (W14): section jumps, focus on
  headings, Escape innermost first, Ctrl/Cmd+Z only outside text fields.
- Density (W15): 32px controls for a fine pointer, 44px with touch,
  user-selectable; `density.*` tokens.
- Panorama fit (W7), sticky section strip on phones (W8), compact tile
  grid without stretching past two columns (W6).
- Toasts rise from the app bar in the compact class (K7).
- Status vocabulary (W31), selection and floating-surface rules in
  [docs/colour.md](docs/colour.md).
- `--check` also verifies colour-blind separation of statuses and that
  every `--rk-` read in the CSS and JS is defined.
- Rekta Board, the demo site, built on `dist/`, with a Playwright smoke
  test.

### Fixed

- `[hidden]` always hides (W5); reduced motion wins over component
  rules, and status colours no longer leak into neutral children (K2).
- Dragging or selecting text never activates a tile or row (W18);
  touch presses wait for the tap delay (K1); hover tints only where
  hover exists.
- Pivot titles keep a 44px hit area at any scale (K8) and their gaps no
  longer grow with the length of a scaled-down title.
- Fields are 16px on touch (W33); sheets lock the page and close on a
  tap on the shading (K10).
- The app bar's status word is always visible (K16).
- Dark error tile darkened to stay apart from warning under
  deuteranopia.

## 0.1.0

Documentation, tokens and the showcase site.

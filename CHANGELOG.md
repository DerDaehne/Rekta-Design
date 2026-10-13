# Changelog

Rekta Design follows [semantic versioning](https://semver.org/). Classes,
tokens, public hooks, `data-rk-*` attributes, events and the `rekta.js`
API are the public surface. Before 1.0, a minor version may still break
it; each entry says how.

## Unreleased

### Added

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

### Fixed

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

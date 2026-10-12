# Changelog

Rekta Design follows [semantic versioning](https://semver.org/). Classes,
tokens, public hooks, `data-rk-*` attributes, events and the `rekta.js`
API are the public surface. Before 1.0, a minor version may still break
it; each entry says how.

## Unreleased

### Added

- **rekta-core** (`core/`): Rekta Design as native views in C for
  Windows (Win32), macOS (Cocoa) and Linux (Xlib), with no dependency
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
  images) and the demo's script. Stage 1 of docs/core.md: text fields,
  menus and tables come next, screen-reader bridges after them.
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

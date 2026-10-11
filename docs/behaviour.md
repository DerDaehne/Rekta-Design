# Behaviour: rekta.js

`dist/rekta.js` is Rekta's behaviour for the web: press, the place
change, routes and back, the panorama, pivots, toasts, the banner and
sheets. It is a plain ES module with no dependencies and no framework.
`dist/rekta.global.js` is the same code as a classic script that sets
`window.Rekta`.

It works with any framework, or none: every part takes an element,
reads its timings from the `--rk-` tokens, and returns a controller
with `destroy()`. A component framework mounts it in its "after mount"
hook and calls `destroy()` in its "before unmount" hook.

## Load order

```html
<link rel="stylesheet" href="rekta.tokens.css">
<link rel="stylesheet" href="rekta.fonts.css">
<link rel="stylesheet" href="rekta.css">
<script type="module">
  import * as Rekta from './rekta.js';
  Rekta.start();
</script>
```

For a remembered theme with no flash, restore the prefs before first
paint: load `rekta.global.js` in `<head>` and call
`Rekta.prefs.restore()` there.

## The contract

- **Classes are yours.** Rekta defines only `.rk-*` classes and never
  adds or removes a class itself. Everything the script writes is a
  `data-rk-*` attribute, a `--_rk-*` property (internal), one of the
  public hooks below, or `hidden`.
- **Hide with `hidden`.** Rekta's display rules never override it.
- **Timings are tokens.** Every duration comes from a `--rk-duration-*`
  token as it applies at that moment, so reduced motion (each motion
  duration `0s`) makes every step instant and never skips one.
- **Status is `data-status`.** `info`, `success`, `warning`, `error`, or
  none. It sets the fill, mark and soft colours of the element that
  carries it; a tag, tile or banner without its own `data-status` stays
  neutral inside a status row.

### State the script writes

| Attribute | On | Meaning |
|---|---|---|
| `data-rk-pressed` | a tile | Being pressed, or held for the rest of `duration-press` after a quick tap. |
| `data-rk-turn` | units of a place, or the place | `out`, `back-out`, `pressed` while leaving; `in`, `back-in` while arriving. |
| `data-rk-arriving` | the root or a place | Arrival build-in: tiles build in and sparklines draw only under it. Removed once the build-in is over, so a re-render never replays it. |
| `data-rk-phase` | a toast, the banner | `leaving` while it goes; `clear` on the banner when no state stands. |
| `data-rk-fit` | a panorama, a pivot | Everything fits: nothing scrolls and the trailing spacer is gone. |
| `data-rk-state` | the app bar | `halted` or `paused`, following the banner. |
| `data-rk-pending` | a button | Its action is running (`pending()`). |
| `data-rk-side` | a floating surface | `below` or `above` its control (`place()`). |
| `aria-busy` | a `.rk-region` | Loading (`busy()`). |
| `data-rk-selected` | a table row | Its checkbox is checked (`table()`). |
| `data-rk-selecting` | the app bar | Rows are selected; the bar carries the selection. |
| `data-rk-dragging` | a split handle | Being dragged. |
| `data-theme`, `data-density`, `data-motion` | the root | Prefs overrides (see `prefs`). |

### Public hooks

| Property | Written by | Read by |
|---|---|---|
| `--rk-i` | the page | A tile's build-in order (0–10). |
| `--rk-sec-w` | the page | A panorama section's width when sideways. |
| `--rk-split-w` | the page, `split()` | The width of a split's list pane; the handle writes it while dragged. |
| `--rk-banner-h` | `banner()` | `scroll-padding`, the sticky strip, toasts without anchor positioning. |
| `--rk-pivot-h` | `panorama()` | `scroll-padding` under a sticky section strip. |
| `--rk-pivot-scale` | `pivot()`, `panorama()` | The inactive pivot titles' scale. |

### Events

| Event | On | Detail |
|---|---|---|
| `rk:prefs` | `document` | `{ name, value }` after `prefs.set`. |
| `rk:bannerchange` | the banner (bubbles) | `{ state }`, the state now shown or `null`. |
| `rk:selectionchange` | a table (bubbles) | `{ count, rows }` after the selection changes. |

## API

### `start({ root, restore })`

The defaults for a page: restores prefs (unless `restore: false`, for an
embedded page whose host owns theme, density and motion), attaches
`press` to `root`, attaches `sheet` to every `dialog.rk-sheet`, and
marks the first paint as an arrival. Returns `{ destroy }`.

### `prefs`

`prefs.get(name)`, `prefs.set(name, value)`, `prefs.restore()`.

| Name | Values | Default |
|---|---|---|
| `theme` | `system`, `light`, `dark` | `system` |
| `density` | `auto`, `comfortable`, `compact` | `auto`: compact for a fine pointer with no touch screen |
| `motion` | `system`, `on`, `off` | `system` |

The default removes the attribute from the root. Values are remembered
in `localStorage` under `rekta-prefs`; pass `{ remember: false }` to
skip that.

### `keys.on(priority, handler)`

One keyboard dispatcher for the whole page. The handler gets the
`keydown` event and returns `true` when it used the key; the event is
then prevented and nothing below sees it. Returns a function that
removes the handler. Composition (IME) events never reach a handler.

| Priority | Layer |
|---|---|
| 40 | Floating surfaces: menu, palette, combobox list |
| 30 | A toast that has focus (Escape dismisses it) |
| 25 | The open toast's undo (Ctrl/Cmd+Z, not in a text field) |
| 20 | A selection (Escape clears it) |
| 10 | The place (Escape goes back) |

`keys.escapeIsFree(event)` is false while focus is in a text field, a
combobox, listbox, menu or expanded control, or while a modal or popover
is open. Native `<dialog>` and popovers handle their own Escape.

### `press(scope)`

Tiles that are links or buttons (`a.rk-tile`, `button.rk-tile`) nudge
back where they are pressed. Enter presses any tile; Space presses
button tiles (Space scrolls on a link). On touch the press waits
`duration-tap-delay` and cancels on 8 px of movement, so the start of a
scroll never sinks a tile. A click after 4 px of dragging, or with text
selected inside the tile or row, is cancelled: dragging never
activates. Returns `{ destroy }`.

### `turn`

The place change as two primitives that any router can call.

| Member | Does |
|---|---|
| `turn.leave(place, { pressed, back })` | Turns the visible units of `place` away, a row at a time, `pressed` last from its pressed pose; then hides `place`. Resolves when it is gone. |
| `turn.enter(place, { back, focus, arriving })` | Shows `place` and turns it in, then focuses `focus`. Its tiles build in unless `arriving` is false (the default for `back`). |
| `turn.finish()` | Ends a running turn in its end state. Call it before any navigation that does not wait: a navigation is never refused. |
| `turn.running` | Whether a turn runs. |

With a framework router: `await turn.leave(oldEl, { pressed })`, render
the new route, then `await turn.enter(newEl, { focus: heading })`.

### `places({ mode, base, home, title, onChange })`

A small router for places mounted in the page as
`<main class="rk-place" data-rk-place="name" data-rk-parent="parent" hidden>`.

- Every place has an address: `#name` (`mode: 'hash'`, the default) or
  `base/name` (`mode: 'path'`, which needs the server to serve the page
  for every place path).
- A plain primary click on a link to a place turns to it and pushes one
  history entry. Ctrl/Cmd/Shift/Alt and middle clicks are the
  browser's, so "open in new tab" works.
- Back is `history.back()` and plays the back turnstile, restores every
  scroll offset in the place and returns focus to the link that opened
  it. The browser's back and forward, the system back gesture and the
  mouse back button go through the same path.
- Elements with `data-rk-back` go back: the app bar's
  `.rk-ab-back[data-rk-back]` (hidden at the top) and each place's
  `.rk-crumb`. A crumb is a link to the parent's address: it goes back
  when the parent is the place below, otherwise forward to the parent.
- Escape goes back when nothing nearer wants it.
- A cold entry (a deep link, a reload) below the top puts the parent
  place underneath, so back stays in the app.
- `document.title` becomes `Place title · title`.

- A place can show one item at a time, addressed as `#name/item`.
  `onItem(place, fn, { push })` hears it: `fn(item | null, { back, initial })`
  runs whenever that place's item changes and may return a name for the
  window title; `push()` says whether choosing an item is a history step
  of its own (a split in the compact class). `open(item)` shows an item
  of the current place; a link to `#name/item` does the same.

Returns `{ current, item, below, navigate(name, { replace, pressed }), open(item, { replace }), back(name?), onItem, urlOf(name, item), destroy }`.

### `panorama(el, { pivot })`

`el` is a `.rk-panorama` of `.rk-sec` sections; `pivot` is an optional
`.rk-pivot` of `#section` links. Sideways:

- One wheel gesture (events less than `duration-wheel-latch` apart)
  latches to one scroller: to the section under the pointer if it can
  still scroll that way (it is then swallowed at that section's end),
  otherwise to the panorama, which then stays sideways even as the
  pointer crosses sections. Horizontal input and Shift+wheel stay
  native; at either end the wheel falls through; Ctrl+wheel zooms.
- With focus on the panorama: ← → PageUp PageDown jump a section, Home
  and End go to the ends.
- The pivot follows the scroll. A pivot link aligns its section flush
  left (or as far as the panorama scrolls) and moves focus to the
  section's heading. A click wins over the follow.
- When everything fits, nothing scrolls (`data-rk-fit`).
- When the window-size class changes, the current section stays
  current.

Compact, the sections stack; a pivot that is a direct child of the
place sticks under the banner and publishes `--rk-pivot-h`.

Returns `{ goTo(sectionOrId, { focus, instant }), current, destroy }`.

### `pivot(el, { onSelect })`

`el` is a `.rk-pivot[role=tablist]` of `[role=tab]` buttons with
`aria-controls`. A click or ← → Home End selects a facet; the strip
glides it to the left edge and the panel slides in from its side. Facets
add nothing to history. Returns `{ select(index, { focus }), destroy }`.

### `toaster(region)`

`region` is an always-mounted `.rk-toasts` with `role="status"` and
`aria-atomic="false"`, placed after the content whose actions raise
toasts.

`toaster.show(text, { key, undo, from, action })` shows one toast,
replacing the one open. With `undo`, the toast has an undo button and a
`duration-toast` window that counts down once a second and holds while
it is hovered or has focus. The same `key` with `undo` updates the open
toast and restarts its window. Ctrl/Cmd+Z undoes while the window is
open, except in a text field; Escape inside a toast dismisses it. When
the toast had focus as it goes, focus returns to `from`, or to the
place's title. Undo is a shortcut, never the only way back.

Returns `{ show, open, dismiss(), destroy }`.

### `banner(el, { states, order, appbar, liveWord, toaster, onAction })`

`el` is the `.rk-banner`, mounted once with two live regions
(`[role=alert]`, `[role=status]`) and an optional `.rk-actions` remedy
button.

- `states`: `{ name: { word, status, alert, text, action, since, until, on, off } }`.
  `status` is the fill; `alert: true` only for a halt; `on` and `off`
  are the toast texts for a state that starts or ends while another
  holds the banner.
- `order`: most severe first. One banner at a time; the most severe
  state that is on holds it.
- `appbar` gets `data-rk-state`; `liveWord` gets the state's word.
- A state that ends is an event: `toaster` reports it once the banner
  has gone.
- The remedy button calls `onAction(name)`, or turns the state off.

Returns `{ set(name, on, { from }), toggle(name), is(name), shown, destroy }`.

### `sheet(dialog)`

For `<dialog class="rk-sheet">` opened with `showModal()`. A tap on the
shading cancels where `closedby` is not supported. Use
`closedby="closerequest"` on a sheet with unsaved input. Returns
`{ destroy }`.

### `busy(region, work)`

Marks a `.rk-region` `aria-busy` while `work` (a promise, or a function
returning one) runs. The line shows only after `busy-delay` (CSS) and,
once shown, stays at least `busy-min`. Resolves or rejects with the
work. See [states.md](states.md).

### `pending(button, work, { label })`

The button says what it is doing (`label`, its `data-rk-pending-label`,
or its text plus "…") at the same width, refuses more clicks and writes
`data-rk-pending` (the line under it) until `work` settles. Resolves or
rejects with the work.

### `menu(trigger, el, { onSelect })`

A menu button. `el` holds `.rk-menu-item` buttons (`role` `menuitem`,
`menuitemcheckbox` or `menuitemradio`), `.rk-menu-sep` and
`.rk-menu-label`; the script makes it a `.rk-float` popover with
`role="menu"`, makes the trigger its invoker (`aria-haspopup`,
`aria-expanded`, `aria-controls`), places it, and runs its keys (see
[states.md](states.md#floating-surfaces)). A choice toggles a checkbox
item or picks a radio item, closes the menu, returns focus to the
trigger and calls `onSelect(item)`. Returns `{ open(index), close(), destroy }`.

### `table(el, { onSort, appbar, onSelect })`

`el` is a `.rk-table`. A `.rk-sort` button in a head cell sorts by its
column (ascending, then descending; `aria-sort`), moving the rows with
FLIP; with `onSort({ column, dir, th })` the app sorts instead. Rows
with a `.rk-check` checkbox are selectable (Shift+click ranges, the head
checkbox, Ctrl/Cmd+A, Escape at key layer 20); a selected row gets
`data-rk-selected`. With `appbar`, the bar gets `data-rk-selecting`, its
`.rk-ab-count` the count, and its `[data-rk-clear]` button clears. Cells
get `data-label` from their head for the stacked layout. Fires
`rk:selectionchange`. Returns `{ selected(), clear(), selectAll(), sort(column, dir), refresh(), destroy }`
(`refresh` after adding or removing rows).

### `split(el, { key })`

`el` is a `.rk-split` of `.rk-split-list`, `.rk-split-handle` and
`.rk-split-detail`. Sideways, the handle drags `--rk-split-w` (and
moves with ← → Home End, resets on a double click, remembers the width
under `key`). Compact, `show('detail' | 'list', { focus })` turns
between the panes. `reveal(dir)` slides a new item into the detail
sideways. Returns `{ show, reveal, sideways, showing, destroy }`. Pair
it with `places().onItem(place, fn, { push: () => !split.sideways })`.

### `place(el, anchor)`

Places a fixed floating surface below `anchor`, or above it when there
is no room; writes `data-rk-side`. For your own floating surfaces.

### `tokens`

`tokens.value(name)`, `tokens.ms(name)`, `tokens.number(name)`, `tokens.px(name)` (a length in px, rem resolved): a token
as it applies right now, for an app's own animations.

## Versions

`Rekta.version` is the package version. Rekta follows semantic
versioning: removing or renaming a class, token, hook, attribute, event
or API member is a major change, and adding one is a minor change.
Before 1.0, a minor version may still break things, and the changelog
says so. See [CHANGELOG.md](../CHANGELOG.md).

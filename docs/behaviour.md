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

**Server rendering.** Importing `rekta.js` touches no DOM, so a page
that renders on the server (SvelteKit, Next.js, Astro, Nuxt) can import
it at the top of a module. Call its functions only in the browser, in a
mount hook or an effect. `node scripts/build.mjs --check` imports it in
plain Node, so this cannot break unnoticed.

## The contract

- **Classes are yours.** Rekta defines only `.rk-*` classes and never
  adds or removes a class itself. Everything the script writes is a
  `data-rk-*` attribute, a `--_rk-*` property (internal), one of the
  public hooks below, or `hidden`.
- **Hide with `hidden`.** Inside an `.rk` root, `hidden` wins over every
  `display` rule, Rekta's and the page's alike: `rekta.css` sets
  `display: none !important` on it, so no layout of Rekta's can show a
  hidden element by accident. A page that has to show an element that
  carries `hidden` (server-rendered facets shown side by side until the
  script takes over, say) adds `data-rk-reveal`: `hidden` stays, for the
  script and for assistive technology reading the attribute, and the
  page's own `display` applies. `hidden="until-found"` is left alone.
- **Timings are tokens.** Every duration comes from a `--rk-duration-*`
  token as it applies at that moment, so reduced motion (each motion
  duration `0s`) makes every step instant and never skips one.
- **Status is `data-status`.** `info`, `success`, `warning`, `error`, or
  none. It sets the fill, mark and soft colours of the element that
  carries it; a tag, tile or banner without its own `data-status` stays
  neutral inside a status row.

### Rules on the root

`rekta.css` styles `.rk-*` classes. A few rules reach further, on
purpose, and a page that composes Rekta with its own CSS should know
them:

| Rule | Why |
|---|---|
| `:root { scroll-padding-block: … }` | Anchors and focus scroll clear of the banner and section strip above (`--rk-banner-h`, `--rk-pivot-h`) and the app bar below. |
| `:root:has(dialog.rk-sheet[open])` | The page under an open sheet stops scrolling, and keeps its scrollbar's room so nothing shifts. |
| `[data-status='…']` | Sets only the internal `--_rk-fill`, `--_rk-mark` and `--_rk-soft`, which nothing outside Rekta's components reads. |
| `:root[data-theme]`, `[data-density]`, `[data-motion]`, `[data-rk-titlebar]` | The switches `prefs` and the installed app's title bar write on the root. |
| `:root[data-rk-crossfade]::view-transition-old(root)` and `-new(root)` | Rekta's cross-fade timing, only while `crossfade()` (or a theme change through `prefs`) runs its own view transition: an app's own root view transitions keep their timing. |

### State the script writes

| Attribute | On | Meaning |
|---|---|---|
| `data-rk-pressed` | a tile, a `[data-rk-press]` item | Being pressed, or held for the rest of `duration-press` after a quick tap. |
| `data-rk-turn` | units of a place, or the place | `out`, `back-out`, `pressed` while leaving; `in`, `back-in` while arriving. |
| `data-rk-arriving` | the root or a place | Arrival build-in: tiles build in and sparklines draw only under it; `turned` after a place turn, where only sparklines draw. Removed once the build-in is over (`duration-moderate` + 10 × `duration-stagger` + `duration-long` + 50ms), so a re-render never replays it. A page without the script may set and remove it itself, with the same value and window. |
| `data-rk-phase` | a toast, the banner | `leaving` while it goes; `clear` on the banner when no state stands. |
| `data-rk-fit` | a panorama, a pivot | Everything fits: nothing scrolls (and the panorama's trailing spacer is gone). |
| `data-rk-measured` | a pivot | `rekta.js` has measured its titles: inactive ones shrink by a scale that animates, instead of the stylesheet's measure-free font-size. |
| `data-rk-wide` | a pivot and its facets' parent | The facets show side by side (`pivot()`, from `size.pivot-wide` up). |
| `data-rk-state` | the app bar | `halted` or `paused`, following the banner. |
| `data-rk-pending` | a button | Its action is running (`pending()`). |
| `data-rk-side` | a floating surface | `below` or `above` its control (`place()`). |
| `aria-busy` | a `.rk-region` | Loading (`busy()`). |
| `data-rk-select` | a list or table body | Its items are chosen the Metro way (`select()`). |
| `aria-selected` | an item of it | `true` when selected: the accent fill and the checkmark. |
| `data-rk-press` | an item of it | It nudges when pressed (`press()`). |
| `data-rk-selecting` | the list, the app bar | In the selection mode; the bar carries the selection. |
| `data-rk-dragging` | a split handle | Being dragged. |
| `data-rk-over` | a drop zone | Files are dragged over it. |
| `data-rk-live` | a live region | `fresh`, `stale` or `offline`. |
| `data-rk-held` | a live region | Held still while the pointer rests on it or focus is in it. |
| `data-rk-phase="leaving"` | a sheet | Leaving, before it closes. |
| `data-rk-unselecting` | the app bar | Its own commands coming back after the selection mode. |
| `data-rk-sliding` | segments, the chosen label | The accent block is sliding to the new choice. |
| `data-rk-grow` | a chart's bars | Growing in on the first draw. |
| `data-rk-dragging`, `data-rk-grabbed` | a sortable item | Being dragged; picked up from the keyboard. |
| `data-rk-sorting` | a sortable list | An item is being dragged. |
| `data-rk-editing` | a table cell | Being edited in place. |
| `data-rk-active` | a virtual row | The active row (`aria-activedescendant`). |
| `data-rk-wco` | the root | An installed app's window-controls overlay is visible. |
| `data-rk-titlebar` | the root | Set by a native shell: show the title bar. |
| `data-value` | a combobox's input | The chosen option's value. |
| `--_rk-v` | a slider | How far its value is along the track, for the fill. |
| `data-rk-overflow` | an app-bar control | Moved into "more" (`appbar()`). |
| `data-theme`, `data-density`, `data-motion` | the root | Prefs overrides (see `prefs`). |

### Public hooks

| Property | Written by | Read by |
|---|---|---|
| `--rk-i` | the page | A tile's build-in order (0–10). |
| `--rk-v` | the page | A meter's value, 0 to 1. |
| `--rk-titlebar-controls` | a native shell | Room kept at the title bar's end for the window's own buttons (140px by default) when `data-rk-titlebar` shows it. |
| `--rk-sec-w` | the page | A panorama section's width when sideways. |
| `--rk-split-w` | the page, `split()` | The width of a split's list pane; the handle writes it while dragged. |
| `--rk-banner-h` | `banner()` | `scroll-padding`, the sticky strip, toasts without anchor positioning. |
| `--rk-pivot-h` | `panorama()` | `scroll-padding` under a sticky section strip. |
| `--rk-pivot-scale` | `pivot()`, `panorama()` | The inactive pivot titles' scale. |
| `--rk-toast-left` | `toaster()`, or the page | What is left of a toast's undo window, 1 to 0: the countdown line under its action. A page without the script sets it with the figure. |

### Events

| Event | On | Detail |
|---|---|---|
| `rk:prefs` | `document` | `{ name, value }` after `prefs.set`. |
| `rk:bannerchange` | the banner (bubbles) | `{ state }`, the state now shown or `null`. |
| `rk:selectionchange` | a list, table body or virtual list (bubbles) | `{ count, mode, items }` (`indices` for a virtual list) after the selection changes. |
| `rk:edit` | a table (bubbles) | `{ row, cell, value }` after an edit is kept. |
| `rk:historychange` | `document` | `{ undo, redo }`, the labels next in line. |
| `rk:placechange` | `document` | `{ from, to, back }` after a place change has finished. |
| `rk:pwa` | `document` | The app's install and offline state (see `pwa()`). |

## API

### `start({ root, restore })`

The defaults for a page: restores prefs (unless `restore: false`, for an
embedded page whose host owns theme, density and motion), attaches
`press` to `root`, attaches `sheet` to every `dialog.rk-sheet` and
`stepper` to every `.rk-stepper`, keeps every `input.rk-range` filled
to its value, gives every `.rk-toggle` checkbox `role="switch"`, turns on
tooltips (`tips()`), and marks the first paint as an arrival. Returns `{ destroy }`.

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
back where they are pressed, and so does anything with `data-rk-press`
(selectable rows, with the smaller `row-tilt` and `row-press-scale`).
A press on a control inside it is the control's own. Enter presses any
tile; Space presses button tiles and `data-rk-press` items (Space
scrolls on a link). On touch the press waits
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

- Search and filters are parameters of the address (`#name?q=web`).
  `setParams(params)` replaces them (never pushing); `params` reads
  them; the `onItem` watcher gets them as `params` on arrival and back.

- `guard(fn)`: `fn()` may hold any change of place (a link, back,
  Escape, the browser's back or forward). It returns `true` at once when
  it has nothing to ask, else `false` or a promise of the answer; on the
  browser's own step the router takes it back, asks, and replays it.

Returns `{ current, item, params, below, navigate(name, { replace, pressed }), open(item, { replace }), setParams(params), back(name?), guard(fn), onItem, urlOf(name, item, params), destroy }`.

### `panorama(el, { pivot })`

`el` is a `.rk-panorama` of `.rk-sec` sections; `pivot` is an optional
`.rk-pivot` of `#section` links. The panorama takes focus (`tabindex="0"`)
so the keys can scroll it, which makes it a landmark: `panorama()` gives
it `role="region"` when it has no role, and the page must give it a name,
`aria-label` or `aria-labelledby` (the place's title). Without one a
screen reader announces a nameless group, and `panorama()` warns in the
console.

```html
<div class="rk-panorama" aria-labelledby="home-title"> … </div>
```

Sideways:

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
glides by the facet's place in the order (the ones before it stay in reach) and the panel slides in from its side. Facets
add nothing to history.

Wide: when the panels share a parent at least `--rk-pivot-wide` wide
(`size.pivot-wide`, 1300px; 0 never), every facet shows, side by side as
panorama sections sized in tile units, and both the pivot and that parent
get `data-rk-wide`. The strip stays visible as a jump list: a title (a
click, or ← → Home End with focus staying on the strip) glides its facet
to the leading edge over `duration-pivot-glide`, Tab moves focus into the
facet chosen, and the title follows the facet at the leading edge as the
user scrolls. Narrower, it
is one facet at a time again, the one last chosen. A pivot that picks the
place (`.rk-pivot--nav`) never goes wide. The threshold is the product's
to pick, per pivot if need be: `style="--rk-pivot-wide: 1100px"`.

Returns `{ select(index, { focus }), wide, destroy }`.

### `toaster(region)`

`region` is an always-mounted `.rk-toasts` with `role="status"` and
`aria-atomic="false"`, placed after the content whose actions raise
toasts.

`toaster.show(text, { key, undo, from, action, act })` shows one toast,
replacing the one open. With `undo`, the toast has an undo button and a
`duration-toast` window that counts down once a second and holds while
it is hovered or has focus. The same `key` with `undo` updates the open
toast and restarts its window. Ctrl/Cmd+Z undoes while the window is
open, except in a text field; Escape inside a toast dismisses it. When
the toast had focus as it goes, focus returns to `from`, or to the
place's title. Undo is a shortcut, never the only way back. `act:
{ label, run }` adds a plain action without a countdown ("reload").

Returns `{ show, open, dismiss(), destroy }`.

### `banner(el, { states, order, appbar, liveWord, toaster, onAction })`

`el` is the `.rk-banner`, mounted once with two live regions
(`[role=alert]`, `[role=status]`) and an optional `.rk-actions` remedy
button. Sideways, the word, the sentence and the action stand in a row
on the tile grid's widths; compact, the word (its time under it) and the
sentence share a line and the action goes under them, about 105px on a
375px phone.

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
`closedby="closerequest"` on a sheet with unsaved input. The sheet leaves
the way it came before it closes (`data-rk-phase="leaving"`): `close()`,
Escape and a `method=dialog` form all wait for it, and `close` fires
after. Returns `{ destroy }`.

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

### `menu(trigger, el, { onSelect, summary })`

A menu button. `el` holds `.rk-menu-item` buttons (`role` `menuitem`,
`menuitemcheckbox` or `menuitemradio`), `.rk-menu-sep` and
`.rk-menu-label`; the script makes it a `.rk-float` popover with
`role="menu"`, makes the trigger its invoker (`aria-haspopup`,
`aria-expanded`, `aria-controls`), places it, and runs its keys (see
[states.md](states.md#floating-surfaces)). A choice toggles a checkbox
item or picks a radio item, closes the menu, returns focus to the
trigger and calls `onSelect(item)`; a checkbox item toggles and leaves the
menu open. With `summary` ("status") the trigger says what is ticked
("status: failed, done", or "status: any"): a filter button. Returns
`{ open(index), close(), set(isChecked), destroy }` (`set` ticks
checkbox items from outside, as when a filter comes back from the
address).

### `select(el, { items, onShow, onChange, appbar })`

Metro selection on `el`, a list (`.rk-rows`, a grid of items) or a
table body; `items` picks its items (default: its `li` or `tr`
children). A click selects one item and calls `onShow(item)`;
Ctrl/Cmd+click toggles and starts the selection mode, in which every
click toggles; Shift+click takes a range. Keys: ↑ ↓ (← → in a grid),
Home End, Space or Enter as a click, Ctrl/Cmd+A all, Escape (key layer
20) ends the mode. The list is one tab stop (roving `tabindex`); it gets
`role=listbox` (a table `role=grid`) and `aria-multiselectable`, each
item `aria-selected` and `data-rk-press`. Hidden items drop out of the
selection. With `appbar`, the bar gets `data-rk-selecting` in the mode,
its `.rk-ab-count` the count, and its `[data-rk-clear]` button clears.
`onChange(items, { mode })` and `rk:selectionchange` follow every
change. Returns `{ selected(), only(item, { show }), toggle(item), clear(), selectAll(), mode, refresh(), destroy }`
(`refresh` after adding, removing or hiding items).

### `table(el, { onSort, select, onShow, onSelect, appbar, columns, edit })`

`el` is a `.rk-table`. A `.rk-sort` button in a head cell sorts by its
column (ascending, then descending; `aria-sort`), moving the rows with
FLIP; with `onSort({ column, dir, th })` the app sorts instead. With
`select: true`, or when the body has an `.rk-check` cell, its rows are
chosen with `select()` (`onShow`, `onSelect` as its `onChange`,
`appbar`); the `.rk-check` cell holds the checkmark. Cells get
`data-label` from their head for the stacked layout. Returns
`{ selected(), only(row, { show }), clear(), selectAll(), mode, sort(column, dir), hide(i), show(i), hidden(), width(i, px), edit(cell), refresh(), destroy }`
(`refresh` after adding, removing or hiding rows).

With `columns` (`true` or `{ key }`), each head cell gets a `.rk-col-handle`
separator on its trailing edge: dragged or moved with ← → (Shift: four
times) and Home, it sizes its column (the table switches to a fixed
layout); a double click resets it. `hide(i)` and `show(i)` take a column
out and back. Widths and hidden columns are remembered under `key`.

With `edit: { onCommit(row, cell, value, old) }`, cells with
`data-rk-edit` edit in place on a double click or F2 on their row: Enter
or Tab keeps (Tab moves on, Shift+Tab back), Escape puts the old value
back. If `onCommit` returns a promise, the cell waits (`aria-busy`); a
rejection's `message` shows below the cell (`.rk-cell-error`) and the
field stays open. A kept edit fires `rk:edit` (`{ row, cell, value }`).

### `split(el, { key })`

`el` is a `.rk-split` of `.rk-split-list`, `.rk-split-handle` and
`.rk-split-detail`. Sideways, the handle drags `--rk-split-w` (and
moves with ← → Home End, resets on a double click, remembers the width
under `key`). Compact, `show('detail' | 'list', { focus })` turns
between the panes. `reveal(dir)` slides a new item into the detail
sideways. Returns `{ show, reveal, sideways, showing, destroy }`. Pair
it with `places().onItem(place, fn, { push: () => !split.sideways })`.

### `palette(el, { items, trigger, router, shortcut })`

`el` is a `.rk-palette` holding an input and a list. `items()` returns
entries `{ label, group, keys, keywords, href | run }`; it is called each
time the palette opens, so commands can say what they will do now. The
palette rises from the app bar, filters as you type, moves with ↑ ↓,
runs with Enter (an `href` goes through `router`), and clears, then
closes, with Escape. Ctrl/Cmd+`shortcut` (default K) toggles it from
anywhere; `trigger` (the app bar's "go to") toggles it too. Returns
`{ open(), close(), isOpen, destroy }`.

### `appbar(el, { more })`

Moves the app bar's controls that do not fit into the menu of `more` (a
`.rk-ab.rk-ab-more` button, shown only while needed), the lowest
`data-rk-priority` first, then from the end; writes `data-rk-overflow`
on each moved control. Back, the live state and a selection's controls
never move. Returns `{ layout(), destroy }`.

### `form(el, { validators, onSubmit, submit })`

Checks late and corrects early (see [forms.md](forms.md)). `validators`
maps a control's id to `fn(value, control)` returning a cause-and-remedy
sentence or nothing. Errors go to the field (`data-status="error"`,
`aria-invalid`, the hint) and, on sending, into a `.rk-errors` summary
that takes focus. Without errors, `onSubmit(values)` runs while the
submit button is pending; it may reject with `{ fields: { id: message } }`
or `{ message }`. Returns `{ check(), setErrors(map, message), clear(), errors, destroy }`.

### `guard(el, { router, what })`

While the form differs from how it was loaded (or last `clean()`ed),
leaving its place asks "Leave without saving?" through `router.guard`,
and closing the window asks the browser's question. Discarding restores
the form. Returns `{ dirty, changed(), clean(), restore(), destroy }`.

### `confirm({ title, text, kicker, ok, cancel, safe })`

A short question in a sheet; resolves `true` for `ok`. The safe answer
(`safe: 'cancel'` for an action that cannot be undone) is the primary
button and takes focus. Escape and the shading answer cancel.

### `disable(control, reason)`

The control cannot act now and says why in a `.rk-hint` beside it
(`aria-describedby`); it stays focusable (`aria-disabled`) and clicks do
nothing. `disable(control, null)` enables it again.

### `combobox(input, { options, source, onSelect, free })`

`input` is a `.rk-input` inside a `.rk-combo` (optionally with a
`.rk-combo-btn` chevron). `options` is a list of strings or
`{ label, value, group, hint, keywords }` (or a function returning one);
`source(q)` instead returns a promise of them, for a remote list. Typing
filters (every word must start a word of the label or its keywords);
the list floats below like a menu (`role=listbox`, `aria-activedescendant`).
↓ or Alt+↓ opens, ↑ ↓ move, Enter chooses, Escape closes, then clears.
Without `free`, leaving the field with text that is not a choice puts
the last choice back. `onSelect(option | null)` follows a choice; the
input gets `data-value`. Returns `{ open, close, value, set(value), destroy }`.

### `stepper(el)`

`el` is a `.rk-stepper`: a button with `data-rk-step="-1"`, an
`input[type=number]`, a button with `data-rk-step="1"`. A press steps
once (firing `input` and `change`); held, it repeats after 400ms every
80ms. A button at the limit gets `aria-disabled`. The buttons are out of
the tab order: ↑ ↓ in the field do the same. `start()` attaches it.

### `drop(el, { onFiles, accept })`

`el` is a `.rk-drop` label around an `input[type=file]`. Files dropped on
it or chosen with the input go to `onFiles(accepted, refused)` (`accept`
as in the input's attribute). While files are dragged over it, it gets
`data-rk-over`. While it is mounted, files dropped elsewhere on the page
are not opened by the browser. Returns `{ destroy }`.

### `tips()`

Started by `start()`. Any element with `data-rk-tip="text"` gets an ink
tooltip (`.rk-tip`, `role=tooltip`) after a rest of `timing.tip-delay`
under a mouse or pen, at once on keyboard focus or after another tip,
never on touch; its `aria-keyshortcuts` adds the keys. `data-rk-tip="overflow"`
shows the element's own text, only while its box cuts it off. Escape
(key layer 45), a press, scrolling or leaving hide it. While shown, it
describes its control (`aria-describedby`) unless it repeats its name.

### `contextMenu(scope, el, { target, onOpen, onSelect })`

`el` is a `.rk-float` of `.rk-menu-item`s, as for `menu()`. A right click,
a long press on touch (`timing.long-press`), or Shift+F10 / the
context-menu key on a focused `target` (a selector, inside `scope`)
opens it at the pointer or below the target, turned to stay on screen.
`onOpen(target, event)` prepares it (return `false` to leave the
browser's menu); `onSelect(item, target)` acts. Keys as in a menu;
Escape or a choice returns focus to the target; a press outside,
scrolling or resizing closes it. Returns `{ open(target), close, target, destroy }`.

### `tree(el, { onShow, onToggle })`

`el` is a `ul.rk-tree` of `li`s, each its row first, then a `ul` of
children (`aria-expanded` on an `li` sets it open). The script adds the
roles, levels, a `.rk-tree-row` and a `.rk-tree-twisty` (the chevron), and
a roving tab stop. A click selects and calls `onShow(li)`; the chevron or
a double click opens and closes (`onToggle(li, open)`). Keys: ↑ ↓, → ←,
Home End, Enter Space, `*`, letters. Returns `{ expand(li, open), expandAll(open), select(li, show), selected(), refresh(), destroy }`.

### `sortable(list, { items, handle, onMove })`

Items (`:scope > li` by default) move by their grip (`.rk-grip`, a button):
dragged, the item gets `data-rk-dragging` and the list
`data-rk-sorting`; the others make room and everything settles with FLIP.
Keys on the grip: Space or Enter picks up (`data-rk-grabbed`,
`aria-pressed`), ↑ ↓ move, Space or Enter puts down, Escape puts back;
Alt+↑ ↓ moves at once. A live region after the list says each move.
`onMove(from, to, item)` follows a move. Returns `{ move(item, to), refresh(), destroy }`.

### `virtual(el, { count, render, onShow, onChange, appbar, label })`

`el` is a `.rk-virtual` with a bounded height. Only the rows in view and
six either side exist (`.rk-virtual-row`, placed by `top`, the density's
row height); `render(i, row)` fills one. It is a listbox with one tab
stop (`aria-activedescendant`, `aria-posinset`, `aria-setsize`) chosen the
Metro way like `select()`, on indices: `onShow(i)`, `onChange(indices, { mode })`,
`rk:selectionchange` with `{ count, mode, indices }`, the app bar's mode.
Returns `{ refresh(count), scrollTo(i), selected(), only(i), clear(), mode, count, destroy }`.

### `undoStack({ toaster, limit })`

`do({ label, run, undo, redo })` runs an action and remembers it (up to
`limit`, 50); with `toaster`, its toast offers undo. `undo()` and `redo()`
step back and forth; an undo's toast offers redo. Ctrl/Cmd+Z and
Ctrl/Cmd+Shift+Z (Ctrl+Y off Apple) work outside text fields (key layer
4, below an open toast's own). `entries()` lists `{ label, at, done }`
for a history view. `rk:historychange` on `document` after each change.
Returns `{ do, undo, redo, canUndo, canRedo, entries(), clear(), destroy }`.

### `live(region, { stale, age, hold })`

The live contract for a region (see [live.md](live.md)). `update(apply, key)`
applies a change, or queues it while the region is held (the same `key`
replaces a waiting one); `beat()` is a heartbeat without a change;
`offline(true | false)`. The region gets `data-rk-live` (`fresh`, `stale`
after `stale` ms or `timing.stale`, `offline`) and `data-rk-held` while
the pointer rests on it or focus is in it; `age` (`.rk-age` inside it by
default) says how fresh it is, every second; a quiet `.rk-live-more`
button after it shows and applies waiting updates. Returns `{ update, beat, offline, flush(), state, held, waiting, destroy }`.

### `chart(el, { type, labels, series, unit, min, max, height, summary, table })`

A line (`type: 'line'`) or bar (`'bars'`) chart drawn as SVG at `el`'s
width, redrawn when it changes. `series` is `[{ label, values }]`; the
first is ink, a second grey. Ticks are round and compact (`unit` follows
each value); bars start at zero, lines at `min` or the data's lowest.
Two series get a legend (`.rk-legend`); lines get end labels when wide
enough and clear of each other; bars label their peak. Hover, or ← →
Home End on the focused chart, marks a point (`data-rk-hot`, a crosshair
and markers for lines), shows the values in a tooltip and says them in a
live region. With `table` (default), a `details.rk-chart-table` holds the
numbers. `summary` is the accessible name. See [data.md](data.md).
Returns `{ update(options), active, destroy }`: `update` with new values
on the same labels moves the line or the bars to them, and the axis with
them, over `duration-long`; new labels redraw.

### Motion: `reflow`, `enter`, `leave`, `presence`, `tileUpdate`, `sparkTo`, `crossfade`

The movements that follow a change of content ([motion.md](motion.md)).
Each takes its duration from a token, so reduced motion makes it instant;
the change itself always happens.

- `reflow(scope, change, { items, size })`: FLIP. Runs `change()`; what it
  moved or resized among `items` (`scope`'s children by default) glides
  from where it was. A resized box scales from its old size and fades its
  content back in (`size: false` for moves only). Resolves with
  `change()`'s result once settled.
- `enter(el)`: something new in a list (a row slides in) or a grid (a tile
  builds in). Call it after inserting.
- `leave(el)`: something goes; resolves with the held animation once it
  has, so hide or remove the element, then `cancel()` it.
- `presence(scope, { items, show })`: items come and go by `show(item)`
  (they are hidden or shown): those that go leave, the rest close up,
  those that come enter. A newer call finishes the running one at once.
  Resolves when done.
- `tileUpdate(tile, change, { kind })`: a live tile has news. `'peek'` (a
  new number or line, the default) brings the new `.rk-stat` up from
  below; `'flip'` (a new status) turns the tile over. A continuous reading
  needs neither: change it in place.
- `sparkTo(polyline, points)`: a line moves its points to new ones over
  `duration-long`; a different number of points redraws at once.
- `crossfade(change)`: a change of the whole look cross-fades, where view
  transitions exist. `prefs.set('theme', …)` uses it.

### `icon(name, { label, src })`

An `<svg class="rk-icon">` using `#rk-i-NAME` from the sprite (inlined in
the page, or at `src`, e.g. `'rekta.icons.svg'`). Without `label` it is
`aria-hidden`; with one, `role=img` with that name.

### `titlebar(el, { router })`

`el` is a `.rk-titlebar` (a direct child of `.rk`): shown in an
installed PWA's window-controls overlay, or when the root has
`data-rk-titlebar`. Its `[data-rk-back]` button follows the router like
the app bar's; its `.rk-titlebar-place` names the current place (none
at home). The root gets `data-rk-wco` while the overlay is visible.
Returns `{ destroy }`.

### `pwa({ sw, toaster, onChange })`

Registers the service worker `sw` where a web app manifest is linked and
the page is in a secure context. When a new version waits, `toaster`
shows "A new version is ready" with a plain "reload" action, which asks
it to take over and reloads. Keeps the browser's install prompt for
`install()`. `onChange(state)` and `rk:pwa` on `document` follow every
change of `{ supported, installed, canInstall, offlineReady, updateReady }`.
Returns `{ state, install(), update(), check(), destroy }`.

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

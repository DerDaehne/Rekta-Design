# Navigation

## Pivot

A pivot is a strip of titles in a **fixed order**. Exactly one is
active; it renders large (`typography.size.pivot-active` /
`nav-active`, depending on level — see [typography.md](typography.md)).
The others render smaller and dimmer, in their fixed positions before
and after the active one.

Changing the active title does **not** reorder the strip and does
**not** slide the new active title to the front. Instead, a strip that
does not fit glides by the active title's **place in the order**: the
first title shows the start of the strip, the last its end, and the
titles between move it in proportion. So the strip moves less than the
content below it, and the titles just before the active one stay
visible and in reach: one click goes back a step. The active title is
always whole on screen; titles further away run off either edge,
clipped where the edge cuts them. A strip that fits holds still.
Selecting a title (including a nested pivot's) moves the content it
names flush left, never centred or right-aligned.

Two motions play at once, not one after the other: the strip's glide
uses a pronounced ease-in-out with clear acceleration and deceleration
(`ease-pivot-glide`); the active title's size change uses a lighter,
quicker one (`ease-in-out`, over `duration-pivot-size`).
Only the previously active and newly active titles ever change size —
every other title stays exactly as it was. The strip's height never
depends on which title is active: a title is sized from a fixed line
box, scaled about its baseline, never by changing its `font-size`, so
every title stands on one baseline, the active one's. (The baseline lies
the face's descent and half the leading above the line box's foot:
`rekta.js` measures it in the page; without the script the
`typography.metrics` tokens give it.)

This is deliberately *not* a tab bar with an underline, and deliberately
*not* "MRU" reordering (most-recently-used first) — a fixed order means
the user always knows where a given facet is relative to the others,
even though which one is currently in view changes.

Reduced motion: the scroll becomes an instant jump (no smooth scroll)
and the active title does not animate its size change — it is simply
large from the first paint of the new state.

### Nesting

The same component works at every level: a top-level pivot picks the
place the user is in; inside a place, a second pivot can pick a facet of
that place (e.g. a record's different tabs).

A place's top-level strip (`rk-pivot--nav`) **is** its title, as a
Metro hub's was: no brand line or header above it. Where the place
itself is a section (the home's overview), its entry carries the
product's name, so the strip opens on it. Sideways it takes the nav
level (`nav-active`, `nav-inactive`); compact, where it sticks to the
top, it takes the facet level (`pivot-active`, `pivot-inactive`), so it
holds less of a small screen. The sections below it start with their
own heading; a kicker repeating the strip's title would say it twice. Above a width threshold
(`size.pivot-wide`, 1300px — a product picks its own, it is not one of
the four structural breakpoints), a pivot renders all its facets side by
side as panorama sections instead of one at a time. Its strip stays
visible above them, always: it is the jump list and the marker of where
you are, and it follows the scroll. Keys: on the strip, ← → Home End pick
a title and glide its facet to the leading edge, focus staying on the
title; Tab goes into the facet chosen (not the first one in the page),
and Tab on from there moves through that facet and then the next. A
facet scrolled into view by wheel or touch becomes the chosen title.
`pivot()` does this on the web (see [behaviour.md](behaviour.md)); a
pivot that picks the place never does.

### Along the top or down the side

A pivot that picks the place can lay its titles along the top (the strip)
or down the leading side (`rk-pivot--side` in an `rk-side`;
`rk_pivot_set_layout(pivot, RK_PIVOT_SIDE)` in the core), as a browser's
tabs can be horizontal or vertical. They are equal alternatives: same
fixed order, same facets, same turn between places. Down the side the
titles are one size (`pivot-active`), the inactive ones at
`text-contrast-medium`; the title in sight steps in by `space-m` and takes
`text-primary`. Up and Down pick a title, Home and End the first and last.
Compact, the list is the strip along the top again.

## Places, history and back

A **place** is one screen of the app: one `<main>`, named by its title.
Places form a tree: the top place (a board, a home), and below it the
places its tiles and links open, each with one parent.

- **Every place has an address.** A place can be bookmarked, shared,
  reloaded and opened in a new tab or window. On the web that is a path
  or a hash; in a native app, a route the app can restore.
- **Only a place change makes a history entry.** Choosing a facet,
  jumping to a section or scrolling replaces nothing and adds nothing:
  one back always means one place back.
- **One back for everything.** The app bar's back, the crumb, Escape,
  the browser's back, the system's back (Android's gesture, the mouse
  back button, Alt+Left) all do the same thing: go back one place, with
  the back turnstile, to the scroll position, facet and focus the user
  left. Focus returns to the link or tile that opened the place.
- **Back stays in the app.** A cold entry (a deep link, a reload, a
  shared address) below the top puts the parent place underneath, so the
  first back goes to the parent, never out of the app.
- **Navigation is never refused.** A navigation that comes while a
  place change is still turning finishes that turn and goes; nothing
  waits for an animation.
- **Modifier clicks are the platform's.** Ctrl/Cmd/Shift and middle
  clicks on a link to a place open it where the platform opens links (a
  new tab, a new window); only a plain click turns the place.
- **The title names the place**: the window or tab title is
  "Place · App" ("Item · Place · App" when an item shows).
- **An item can be part of the address.** A place that shows one item at
  a time (a split view's current run) adds it: `#runs/4182`. Choosing an
  item replaces the address while the list stays in view, and is a step
  of its own where the item replaces the list (compact); see
  [collections.md](collections.md#split-view).
- **A facet is part of the address, as a query**, like filters:
  `#runs?facet=history` (`/runs?facet=history` in path mode). Choosing a
  facet replaces the address and never adds a history entry; a reload or
  a shared link opens that facet. The first facet is the default and
  leaves the parameter out. A pivot that is the place's whole content
  uses `facet`; a place with two pivots names each (`?view=…`). A path
  segment would make each facet a place of its own (and back would step
  through them); a hash is the place's already.

  ```js
  const tabs = Rekta.pivot(strip, {
    onSelect(i, tab) {
      const p = router.params;                                 // a copy
      if (i) p.set('facet', tab.dataset.facet); else p.delete('facet');
      router.setParams(p);
    },
  });
  router.onItem('runs', (item, { params }) => {             // arrival and back
    const at = [...strip.children].findIndex((t) => t.dataset.facet === params.get('facet'));
    tabs.select(Math.max(0, at), { silent: true });
  });
  ```

  **The first paint** renders the addressed facet: the
  server (or the framework's render) marks its tab `aria-selected="true"`
  and leaves the other panels `hidden`; `pivot()` starts on the tab
  marked selected and keeps it, so nothing jumps when the script arrives.
  An unknown facet falls back to the first, without an error. Where the
  facets may show side by side, `.rk-facets` around the panels' parent
  and `data-rk-reveal` on the hidden panels let the first paint lay them
  out wide from the same width `pivot()` measures
  ([behaviour.md](behaviour.md), `pivot()`).

### Back with a router of your own

`places()` puts every scroll offset back on back. A framework router
(SvelteKit, Next) does it with `scrollState(root)`, kept with its
history entry, so a panorama's sections and a wide facet row come back
where the user left them:

```js
// SvelteKit: +page.svelte
import { scrollState } from 'rekta-design';
let place;                                    // bind:this on the place's <main>
export const snapshot = {
  capture: () => scrollState(place).capture(),
  restore: (s) => requestAnimationFrame(() => scrollState(place).restore(s)),
};
```

With `history.state` (Next, or no framework), store `capture()` in the
entry before leaving (`history.replaceState({ ...history.state, rk: s }, '')`)
and call `restore(history.state.rk)` after the place has rendered on
`popstate`. Give the boxes ids (or `data-rk-scroll`) when their number
can change between visits.

**Data that comes after mount.** A place that loads its data once it is
mounted (a `fetch` on mount, `{#await}`, streamed data, a list that
loads more) has no sections yet, or short ones, when the router
restores. Call `restore()` as early as above anyway: it sets what it
can and waits for the rest, setting each box as it appears and grows,
for up to `timing.restore` (5s). If the user scrolls first, their
position stays. Keep the handle when the place can go before then:

```js
const run = scrollState(place).restore(s);   // run.pending: still waiting
onDestroy(() => run.cancel());
```

### Crumb

Every place below the top starts with a crumb: a small link up to its
parent (`rk-crumb`, an arrow and the parent's name). It is a real link
to the parent's address. When the parent is the place below in history,
the crumb goes back (and plays the back turnstile); otherwise it goes
forward to the parent. Sideways, the app bar has no back button, so the
crumb is the visible way up.

## App bar

A fixed bar anchored to the **bottom** of the viewport, at every width
including desktop — not just on mobile, and not docked to the top. It
carries the small set of commands that must be reachable from
*anywhere*: the system's live/running status, and a handful of global
actions (in the source product: pause, emergency stop, a command
palette, a keyboard-shortcut reference). Each is a labelled control (an
icon plus a word, never an icon alone), because the app bar is reachable
without having read a manual. Its controls are 48px in every density.

The live status is a square and a word, and the word is always visible
(`live`, or the banner state's word such as `halted`): the square's
colour is never the only signal.

In the compact window-size class, a "back" control is the leading item
in the same bar, within the thumb's reach, hidden at the top place.
Otherwise navigation lives in the pivot and the crumb, not the app bar,
with one opt-in exception below.

### Places in the bar (compact)

On a phone the top-level places of an app with few of them (up to five,
with short words; four read best on a 360px phone) may sit in the app
bar instead of the strip at the top, in the thumb's reach:
`<nav class="rk-ab-places">` in the `.rk-appbar`, one `a.rk-ab` per place
with an icon over a lowercase word.

- **Only compact.** The bar decides by its own width, below 760px; wider,
  `.rk-ab-places` is not shown and the pivot holds the places as always.
  A landscape phone (wide but short) keeps the pivot.
- **Where you are:** the place you are in is in `text-primary`, the
  others in `text-contrast-high` (on glass nothing lower, [colour.md](colour.md#text-on-glass));
  `aria-current="page"` marks it, and
  `places()` keeps the mark on the bar's links. On a place below it (a
  detail), the place you came from stays marked, and choosing it goes
  back to its top.
- **Back** stays the leading item, before the places.
- **Commands** all move into "more" (`appbar()` does it while the places
  show); the live state stays, its square over its word.
- **A selection** takes the bar over as always: the places step aside
  while the selection's commands show.
- **The strip at the top steps aside** (it would say the same twice).
  `appbar()` writes `data-rk-places` on the bar and on the root while
  the places show, and `rekta.css` hides `.rk-place > .rk-pivot--nav`
  under it. The signal comes from the bar's own width, so it holds in a
  narrow pane too, where a media query would disagree. Without the
  script the strip stays, which is safe. Sideways the strip is the
  navigation.
- **What names the place then.** The marked place in the bar is its
  visible name: the icon and word in `text-primary`, with
  `aria-current="page"`. The page still starts with a heading: a place
  whose first section opens with its own visible title needs nothing
  more; one whose title was only the strip gives it
  `<h1 class="rk-place-name">`, which `rekta.css` keeps for screen
  readers only while the strip shows and shows, at the strip's active
  size, while the places are in the bar. Never a kicker repeating the
  bar's word. It is a direct child of `.rk-place`, right after the
  strip, so it takes the strip's side padding and stands where the
  strip stood:

  ```html
  <main class="rk-place" data-rk-place="history">
    <nav class="rk-pivot rk-pivot--nav" aria-label="places">…</nav>
    <h1 class="rk-place-name">history</h1>
    <div class="rk-panorama">…</div>
  </main>
  ```

  With a framework router the `h1` goes where the strip goes (in the
  layout that holds the place and its strip), not into the page
  component inside the content area: there its own padding would add
  to the place name's. When the place changes, it turns with the place
  ([motion.md](motion.md)).

### Go to

"go to" in the app bar (and Ctrl/Cmd+K anywhere) opens the **palette**:
one list to go to a place, a section, an item (a run) or to run a
command. It rises from the app bar at every width, centred, up to 40rem
wide, never as an overlay in the middle of the screen: it belongs to the
bar that opened it. Typing filters (each word must start a word of an
entry or its keywords); ↑ ↓ move, Enter goes or runs, Escape clears the
text and then closes. Entries are grouped (places, sections, items,
commands) and a command shows its key. Commands say what they will do
now ("resume intake" while paused).

### The middle of the bar

Between the live state and the controls, `.rk-ab-mid` holds a product's
own short line: a mode ("Alt only"), a count, a key waiting for its
second key ("g, then a place"). One line, `xs`, secondary colour, no
controls.

- **It gives way first.** It takes the room the controls leave, cuts its
  text with an ellipsis before any control moves into "more", and is
  gone while "more" shows. What it says there must be said elsewhere too
  (a key hint is also in the command's tooltip and the palette).
- **Unless it is pinned.** Feedback that is said nowhere else and lasts
  a moment, such as a key sequence waiting for its second key, gets
  `data-rk-priority="pinned"` while it runs: the middle keeps its words
  (up to half the bar) and the controls move into "more" to make room;
  `appbar()` notices the change on its own. Take the attribute off when
  the sequence ends, and the controls come back.
- **Several parts** (a state and its key hint, a count and a word) are
  elements side by side: the middle lays them out in a row, `space-s`
  apart on one baseline, and cuts the last one.

```html
<span class="rk-ab-mid" data-rk-priority="pinned"><span>go to</span><kbd class="rk-key">g</kbd><span>then a place</span></span>
```

### A control that cannot act now

A command that cannot run now ("print" with no printer) stays in the
bar, in place: glyph and word in `disabled`, no hover or press tint,
still focusable, so its reason can be heard. The reason is its tooltip,
not a hint beside it, which the bar has no room for: after
`timing.tip-delay` under a resting pointer, at once on keyboard focus,
on touch when it is tapped. `disable(control, reason)` does it on a
`.rk-ab`. When the control moves into "more", the reason moves with it:
its item is disabled and says it in the label, "print (needs a
printer)".

### A control in the middle

A control may stand in the middle of the bar (a clock: the time over
the date, two lines in the 48px control), marked `data-rk-align="center"`.
Its floating surface is centred on it ([states.md](states.md#floating-surfaces)).

- **Two sides.** The bar then has a leading side (back, the live state,
  the middle line and the controls before the centred one) and a
  trailing side (the controls after it, "more" last). Each side has half
  the bar less half the centred control; the centred control stands at
  the bar's centre and never moves into "more".
- **Each side gives its own way.** A side that does not fit gives its
  own controls away, the least important first: moving a control of the
  other side would not help. "more" ends the trailing side whichever
  side gave the controls.
- **The middle line** cuts before the centred control, pinned or not (a
  pinned one keeps up to half the bar, but always ends before it), and
  is gone only while the leading side has given controls away; a full
  trailing side leaves it alone.

### A short indicator in place of a control

A system indicator (the volume, the brightness, while it is being set)
does not open anything: it stands for a moment in place of a control,
usually the centred one. The control turns away about its horizontal
axis and the indicator turns in, the turn of a live tile's new status
(`flip-angle`, its curves and perspective); `timing.indicator` (1.7s)
after the last change it turns back. Changes while it stands go in
place: a reading that keeps changing is no news. It means "this holds
only now"; what lasts is a control or a toast. The value is also said
in a live region. `indicate(control, content)` does it on the web.

### A count in a control

A control that counts (notifications 3) shows its figure in a part of
its own, `data-rk-peek`; when the count changes, the new figure peeks
in as a live tile's number does (`tileUpdate()`), and the control's
edge glides if its width changes ([motion.md](motion.md), resize).

### Giving way to full screen

The bar stays at every width, with one exception: while a window or a
place takes the whole screen (a video, a presentation), the bar gives
way. It glides out over its own edge, `duration-short` · `ease-out`,
and back in the same way · `ease-in` when the full screen ends; while
out it takes no focus. The page writes `data-rk-away` on the bar. It
has stepped aside, not gone: a key or a gesture at that edge may bring
it back for a moment.

### Programs in the background

Other programs' status icons do not each get a control: the bar keeps
"never an icon alone". They gather in one control, "in the background"
with their number, whose menu lists each program: its icon, its name,
its state at the item's end where a key hint would stand, in the key
hint's style. A program's own menu is the item's second level
([states.md](states.md#floating-surfaces)), its words as the program
writes them. The count peeks as above.

### When the bar is full

Controls that do not fit move into "more", the least important first
(`data-rk-priority`), and its menu repeats their words. Back, the live
state and a selection's controls never move. Nothing is hidden for good:
every control is reachable at every width. The menu rises from the bar
at its trailing edge (`.rk-ab-menu`: `rekta.css` places it, anchored to
`.rk-ab-more` where the browser has anchor positioning); its keys are
those of every floating surface ([states.md](states.md#floating-surfaces)).

## The recent stack

A product with running items (windows, open documents, sessions) has
one way to switch between them: the **recent stack**. A key (Alt+Tab,
or the product's own) calls it up, and it shows the most recently used
items, the last first, as preview tiles side by side
([tiles.md](tiles.md#tile-content-patterns)), two units high, over the
sheet's shading (`dialog.rk-recent` holding a `.rk-tiles` row, with
`grid()`).

- **Moving:** ← → (and the key that called it up, again), a swipe or the
  wheel move the focus from tile to tile; the row scrolls to keep it in
  view, as a panorama does for its keys. The item before the current
  one has focus when it opens, so one key press and release switches
  back.
- **Choosing:** Enter, a press or letting go of the key that called it
  brings the item to the front; the tile presses as every tile does,
  and the stack goes at once. Escape closes it and nothing changes.
- **Motion:** it and its shading fade in over `duration-short`, the
  sheet's shading; it is called up many times an hour, so nothing in it
  builds in or turns.

An app decides how many items it shows and whether it groups them by
program.

## Tooltips

A tooltip is a small ink strip, like a toast, centred above its control
(below when there is no room), `size.tip-gap` (6px) from it and at least
`space-m` from the window's edges. It shows after the pointer has rested for
`timing.tip-delay`, at once when focus arrives by keyboard, and at once
when moving on from another tip. On touch it shows only when a disabled
control is tapped (its reason, [forms.md](forms.md#disabled-controls-say-why));
otherwise never. Escape, a press, scrolling or leaving hide it. It names an icon-only control, completes a
label with its shortcut, or shows the whole of a text its box cuts off
(`data-rk-tip="overflow"`). It never holds the only way to learn what a
control does: an icon-only control still has its accessible name.

## Keyboard

Every pointer action has a keyboard path.

| Where | Keys | Does |
|---|---|---|
| A focused panorama (sideways) | ← → PageUp PageDown | The previous or next section, aligned flush left |
| | Home End | The first or last section |
| A section strip | Enter on a title | Aligns the section and moves focus to its heading |
| A facet pivot (tabs) | ← → Home End | The previous, next, first or last facet; focus follows |
| A tile | Enter (Space on a button tile) | Presses it; release activates |
| A grid of tiles that open | ← → ↑ ↓ | The nearest tile that way: one in the same row (column) first, on a tie the better-aligned one; on into the grids of the panorama's other sections; at the edge focus stays |
| | Home End | The grid's first or last tile |
| | Tab | Leaves the grid: it is one tab stop, the tile last focused |
| A list, a table, a long list, a tile grid, a tree, a menu | a letter or digit | The next entry that starts with it; letters typed within `timing.type-ahead` form a prefix (below) |
| Anywhere | Escape | Closes the innermost thing that is open (see below) |
| Anywhere but a text field | Ctrl/Cmd+Z | Undoes the last action (the open toast's first) |
| Anywhere but a text field | Ctrl/Cmd+Shift+Z, Ctrl+Y | Does the undone action again |
| A row, a file, a focused item | Shift+F10, the context-menu key | Its context menu |
| A table row | F2 | Edits its first editable cell |
| A tree | ← → ↑ ↓ Home End, * | Close or go up, open or go in, move, open the siblings |
| A grip | Space, ↑ ↓, Escape; Alt+↑ ↓ | Pick up, move, put back; move at once |
| Anywhere | Ctrl/Cmd+K | Go to: the palette |
| Anywhere but a text field | ? | The keyboard shortcuts |
| A table | Ctrl/Cmd+A, Shift+click | Select all, select a range |

The keyboard shortcuts are listed in the app itself (a sheet behind `?`
and an entry in the palette), never only in a manual.

### Writing a key

A key hint (a tooltip's, a menu item's, the shortcut sheet's) writes a
combination one way on every platform: the modifiers in the order Ctrl,
Alt, Shift, Meta, then the key, with "+" between them and no spaces:
"Ctrl+K", "Ctrl+Shift+Z", "Shift+F10". A letter is a capital. On Apple
the modifiers are their symbols, in the same order and with no
separator: ⌘K, ⇧⌘Z. The Meta key outside Apple is "Win" on Windows and
"Super" elsewhere (Linux, ChromeOS, a platform not known); rekta.js takes
the word from its catalogue (`keyMeta`). Keys that are alternatives or a
sequence stand apart, with a space ("← →", "Space ↑ ↓"). These pages
write "Ctrl/Cmd+K" for a key that is Ctrl on one platform and ⌘ on the
other; an app shows the one for where it runs, as `tips()` and the app
bar's menu do from `aria-keyshortcuts` ("Control+K Meta+K").

### Typing: filter or jump

Where there is a field, typing filters; where there is none, typing
jumps.

- **Filter:** the palette, a combobox and a search field narrow their
  list as you type. Focus stays in the field.
- **Jump:** in a collection whose entries take focus (a list or table
  with `select()`, a long list, a grid of tiles that open, a tree, a
  menu), a letter or digit moves focus to the next entry whose text
  starts with it, and the view scrolls along. Letters typed less than
  `timing.type-ahead` (1s) apart form one prefix ("ma" finds "march"
  after "may"); the same letter again moves on to the next entry with
  it, round to the first. Nothing that matches: focus stays. A jump
  never selects or acts: it only moves focus.
- A place offers one or the other for one collection, never both: a
  long alphabetical list ("all apps") jumps; the start surface, with
  its search field, filters.

### A grid of tiles

A grid of tiles that open (a start screen, a dashboard) is one tab stop,
like a list: Tab reaches the tile last focused (the first, to begin
with) and the next Tab leaves the grid. The arrows move the way the eye
does, by position, not by order in the page: the nearest tile that way,
one that shares the row (or column) before one that does not, and of
two as near the one better aligned; a tile in no shared row counts
only while it lies more that way than across. In a panorama they go on
into the next section's grid (sideways → reaches the section beside,
compact ↓ the one below); at the edge focus stays where it is (no
wrap). Home and End go to the grid's first and last tile, letters
jump. A grid of tiles that only inform takes no focus at all
([tiles.md](tiles.md#what-a-tile-does)).

### Who gets a key

Keys are layered: the innermost thing that is open gets the key first,
and once it uses it nothing else does.

1. A tooltip: Escape hides it. Then a floating surface (menu, context
   menu, palette, a combobox's list), a modal sheet or a popover:
   Escape closes it; a field being edited in a table puts its old value
   back.
2. A toast that has focus: Escape dismisses it.
3. The open toast's undo window: Ctrl/Cmd+Z undoes, except in a text
   field, where it is the field's own undo. With no toast open, the
   undo history takes Ctrl/Cmd+Z and Ctrl/Cmd+Shift+Z.
4. A selection: Escape clears it.
5. The place: Escape goes back.

Escape never goes back while focus is in a text field or a list that
uses it, and no shortcut fires while an input method is composing.
An app's own shortcuts skip a key Rekta has used (`defaultPrevented`);
the full table, per layer and component, is in
[behaviour.md](behaviour.md#keysonpriority-handler).

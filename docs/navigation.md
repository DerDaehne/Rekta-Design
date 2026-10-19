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
uses a pronounced ease-in-out with clear acceleration and deceleration;
the active title's size change uses a lighter, quicker ease-in-out.
Only the previously active and newly active titles ever change size —
every other title stays exactly as it was. The strip's height never
depends on which title is active: a title is sized from a fixed line
box (scaled from its own baseline), never by changing its `font-size`.

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
  An unknown facet falls back to the first, without an error.

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
  others in `text-contrast-medium`; `aria-current="page"` marks it, and
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
  bar's word.

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

### When the bar is full

Controls that do not fit move into "more", the least important first
(`data-rk-priority`), and its menu repeats their words. Back, the live
state and a selection's controls never move. Nothing is hidden for good:
every control is reachable at every width. The menu rises from the bar
at its trailing edge (`.rk-ab-menu`: `rekta.css` places it, anchored to
`.rk-ab-more` where the browser has anchor positioning); its keys are
those of every floating surface ([states.md](states.md#floating-surfaces)).

## Tooltips

A tooltip is a small ink strip, like a toast, above its control (below
when there is no room). It shows after the pointer has rested for
`timing.tip-delay`, at once when focus arrives by keyboard, and at once
when moving on from another tip; never on touch. Escape, a press,
scrolling or leaving hide it. It names an icon-only control, completes a
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

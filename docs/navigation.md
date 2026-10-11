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
holds less of a small screen. Above a width threshold
(1300px in the source prototype — a project adopting Rekta Design can pick
its own, it is not one of the four structural breakpoints), a pivot may
render all its facets side by side as a panorama instead of one at a
time, with the pivot strip still present below them for narrower
viewports or as a jump list.

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
in the same bar, within the thumb's reach, hidden at the top place. It is
the one navigational concession the bottom bar makes; everywhere else,
navigation lives in the pivot and the crumb, not the app bar.

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

### When the bar is full

Controls that do not fit move into "more", the least important first
(`data-rk-priority`), and its menu repeats their words. Back, the live
state and a selection's controls never move. Nothing is hidden for good:
every control is reachable at every width.

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
| Anywhere but a text field | Ctrl/Cmd+Z | Undoes the action of the open toast |
| Anywhere | Ctrl/Cmd+K | Go to: the palette |
| Anywhere but a text field | ? | The keyboard shortcuts |
| A table | Ctrl/Cmd+A, Shift+click | Select all, select a range |

The keyboard shortcuts are listed in the app itself (a sheet behind `?`
and an entry in the palette), never only in a manual.

### Who gets a key

Keys are layered: the innermost thing that is open gets the key first,
and once it uses it nothing else does.

1. A floating surface (menu, palette, a combobox's list), a modal sheet
   or a popover: Escape closes it.
2. A toast that has focus: Escape dismisses it.
3. The open toast's undo window: Ctrl/Cmd+Z undoes, except in a text
   field, where it is the field's own undo.
4. A selection: Escape clears it.
5. The place: Escape goes back.

Escape never goes back while focus is in a text field or a list that
uses it, and no shortcut fires while an input method is composing.

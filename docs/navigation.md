# Navigation

## Pivot

A pivot is a strip of titles in a **fixed order**. Exactly one is
active; it renders large (`typography.size.pivot-active` /
`nav-active`, depending on level — see [typography.md](typography.md)).
The others render smaller and dimmer, in their fixed positions before
and after the active one.

Changing the active title does **not** reorder the strip and does
**not** slide the new active title to the front. Instead, the whole
strip glides so the newly active title sits at the strip's left edge —
titles that come before it in the fixed order visibly scroll off-screen
to the left, the same way the panels below do; nothing clips them out
of view early. A trailing spacer after the last title means even the
last title in the order can still reach the left edge. Selecting a
title (including a nested pivot's) also moves the content it names to
the same flush-left position, never centred or right-aligned.

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
that place (e.g. a record's different tabs). Above a width threshold
(1300px in the source prototype — a project adopting Rekta Design can pick
its own, it is not one of the four structural breakpoints), a pivot may
render all its facets side by side as a panorama instead of one at a
time, with the pivot strip still present below them for narrower
viewports or as a jump list.

## App bar

A fixed bar anchored to the **bottom** of the viewport, at every width
including desktop — not just on mobile, and not docked to the top. It
carries the small set of commands that must be reachable from
*anywhere*: the system's live/running status, and a handful of global
actions (in the source product: pause, emergency stop, a command
palette, a keyboard-shortcut reference). Each is a labelled control (an
icon plus a word, never an icon alone), because the app bar is reachable
without having read a manual.

On mobile, a "back" control is the leading item in the same bar — it is
the one navigational concession the bottom bar makes; everywhere else,
navigation lives in the pivot, not the app bar.

A command entry surface (e.g. a command palette) opens as a sheet that
rises from above this bar, not as a page overlay centred on the screen —
it visually belongs to the bar that triggered it.

## Back on mobile

Below `breakpoint.sideways-scroll-min` (760px), "back" is an explicit,
always-reachable control (the leading app-bar item), not just a
platform/browser gesture — users at this width are assumed to be using
the product one-handed, so the control sits in the thumb-reachable
bottom bar, with a touch target that meets the minimum in
[accessibility.md](accessibility.md). It navigates browser/app history
when there is any, and otherwise falls back to the top-level place.

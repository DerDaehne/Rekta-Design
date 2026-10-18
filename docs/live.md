# Live data and undo

## The live contract

Live data is Rekta's reason for motion, so it has to keep a promise:
what looks current is current, and what is not says so.

- **Freshness is said.** A live region carries an age line (`rk-age`):
  "updated just now", "updated 12 s ago", "updated at 14:05". It counts in
  wall-clock time, so reduced motion never stops it.
- **Stale says so.** With no update and no heartbeat for
  `timing.stale` (30 s, a product may choose its own), the region is
  stale (`data-rk-live="stale"`): the age line takes the waiting square
  and says "stale · no update for 2 min", values turn
  `text-contrast-high`, sparklines fade. Nothing pretends to be live.
- **Offline says so.** When the connection drops, the region says
  "offline · last update 5 min ago" and keeps its last values; the
  banner carries the state for the whole app.
- **Holding still.** While the pointer rests on a region or focus is in
  it, it holds still (`data-rk-held`): updates wait, a quiet button says
  how many ("3 updates waiting · show"), and they apply when the region
  is let go or the button is pressed. Nothing moves under the pointer,
  nothing jumps while someone reads. The same key replaces a waiting
  update, so a held value catches up in one step.
- **Heartbeats.** A source with nothing new to say still beats, so
  silence never looks like staleness.
- **Pausing is the user's.** A paused, halted or offline banner state
  stops live drawing everywhere; the regions turn stale on their own.

## A growing list: a trace, a log

A list that grows by appending (a build's log, a request's trace, a
chat-like timeline) reads in time order and keeps the live contract.

- **New rows at the end**, oldest first: it reads down like a transcript.
  (A list of current things with the newest on top, such as
  notifications, is not a log: there new items enter at the top and
  push nothing the user is reading, because they come above it only
  while the list is scrolled to its top.)
- **Following the newest.** While the list is scrolled to its end (within
  one row), it follows: new rows come in and the list keeps its end in
  view. Scrolling away (up), the pointer resting on it or focus in it
  stops following; the list then holds still, rows append below out of
  sight, and a quiet button at its end says how many ("12 new · show",
  the same `.rk-live-more` look as "updates waiting"). The button, or
  scrolling back to the end, shows them and follows again. Nothing ever
  scrolls the list while the user reads it.
- **Arrival.** A row that comes in view while following enters like any
  new row in a list (`enter(el)`: it slides in over `duration-moderate`).
  More than a few rows a second arrive at once, without motion; under
  reduced motion they simply appear. A row that arrives out of sight has
  no motion.
- **Step rows.** A row that marks something other than an entry (a
  manual intervention, a phase starting, a resume after a pause) is a
  full-width row whose words are in the kicker style (lowercase,
  `text-contrast-high`, the time and who: "paused by ana · 14:02"), with
  a hairline above it; a status square (`.rk-tag[data-status]`) only
  where it is a status. It never takes a fill and never moves the rows
  around it.
- **Assistive technology:** the list is `role="log"` (polite, additions
  only), so new rows are read as they come; with many per second, say
  the count instead ("12 new lines") through the region's status.
- **Age and staleness** as for every live region: the age line counts
  from the last row or heartbeat; a stopped trace says it has stopped
  ("finished at 14:20") rather than turning stale.

## Undo

Anything a person does to data can be taken back: archive, edit, move,
remove. A confirm sheet is for what cannot be undone; everything else
asks for nothing and offers undo instead.

- Each action's toast offers undo for `timing.toast`; the window holds
  while the toast is hovered or focused.
- After the toast has gone, Ctrl/Cmd+Z still takes the last step back
  and Ctrl/Cmd+Shift+Z (Ctrl+Y) does it again, outside text fields (a
  field's own undo wins there). An undo's toast offers redo.
- The history is a list a product can show ("what you did"), newest
  first, undone steps struck through until something new is done.
- Undo restores the thing where it was: a row reappears in its place,
  a moved item goes back to its position.

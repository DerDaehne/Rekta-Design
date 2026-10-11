# Collections: tables, selection, split views

## Table

A table is rows on hairlines, like every other list in Rekta, with
columns that line up.

- **Head:** in the kicker's voice (`rk-font-size-xs`, semibold,
  `rk-text-contrast-high`) with a hairline below. It sticks to the top of
  whatever scrolls the table (a section, a split's pane).
- **Sort:** a head cell that sorts holds a button (`rk-sort`) and
  `aria-sort`. The sorted column turns `rk-text-primary` with a small
  triangle pointing the way it sorts. A click sorts ascending, the next
  one descending; the other columns let go. The rows move to their new
  places (FLIP), so the user sees what moved.
- **Cells:** the row's main cell is a `<th scope="row">` (semibold, a
  link when the row opens something); numbers align right in tabular
  figures (`rk-num`); a status is a tag. Rows take the density's row
  height.
- **Narrow:** in a place narrower than `bp-sideways-scroll-min`, rows
  stack instead of scrolling sideways: the main cell and the status on
  the first line, the other cells as "label value" below, labelled from
  their column. The head becomes a row of sort buttons. Compact means
  vertical only.

## Search and filters

A row above the list or table: the search field first, then one button
per filter, then "clear filters" (a quiet button) while anything is
filtered.

- **A filter button says what it keeps:** "status: failed, done", or
  "status: any". It opens a menu of checkbox items that stays open, so
  several can be ticked in a row.
- **Search** narrows as you type (a short pause, no button); Escape in
  the field clears it.
- **Facts follow:** the facts line says how many show ("12 runs ·
  showing 3"); no match says so in the region, with "clear filters".
- **The address keeps them** (`#runs?q=web&status=failed`): reload,
  share and back restore them. They replace the address and never add
  a history step: filters are a view of the place, not a place.
- Rows a filter hides drop out of the selection.

## Selection

When an action applies to several rows at once, the rows get a checkbox
at their leading edge (`rk-check`).

- **Look:** a selected row takes `rk-surface`. Never a status colour and
  never the accent: a selected failed run still reads as failed.
- **Pointer:** a click on a checkbox selects that row, Shift+click the
  range from the last one; the head checkbox selects all or none and
  shows a partial selection. Clicking the row's title still opens it.
- **Keys:** Space toggles the focused checkbox, Ctrl/Cmd+A selects all
  while focus is in the table, Escape clears the selection (before it
  would go back).
- **The app bar carries it.** While anything is selected, the app bar
  shows the count ("3 selected"), the actions that apply to the selection
  and "clear", in place of its own controls. The count is a live region.
- An action on a selection that changes data offers undo in a toast and
  puts the rows back where they were.

## Current item

In a split view, the row whose detail shows is **current**, which is not
the same as selected: a row can be both. The current row has a 2px
`rk-text-primary` bar at its leading edge (`aria-current="true"`).

## Split view

A list and the detail of its current item.

- **Sideways:** side by side. The list takes `--rk-split-w` (four tile
  units by default), the detail the rest; neither pane gets narrower than
  `size.pane-min`. Each pane scrolls itself. Between them, a 1px
  `rk-text-contrast-low` hairline with an invisible 9px handle that turns
  2px and `rk-text-contrast-medium` while hovered, focused or dragged.
  The handle is a focusable separator: ← → move it by `space-m` (Shift:
  four times), Home End to the limits, a double click resets it, and the
  width is remembered.
- **Compact:** one pane at a time. Choosing an item turns from the list
  to the detail; the detail starts with a crumb back to the list.
- **Address:** the current item is part of the address (`#runs/4182`),
  so it can be shared and reloaded. Sideways, choosing an item replaces
  the address (the list stays in view, so there is nothing to go back
  to); compact, it is a step of its own, and back returns to the list
  with the item's row focused. A deep link to an item puts the list
  underneath.
- **Motion:** sideways, the new item slides in from its side like a
  facet; compact, the panes turn like places.

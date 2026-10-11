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

Rows and list items are chosen the Metro way: no checkboxes, the item
itself answers. It works the same on a table, a list of rows or a grid
of items (`Rekta.select()`, or `table()` for a table).

- **A click selects and shows.** A plain click (or tap) selects that one
  item and shows it: its detail beside the list, or in its place on a
  phone. Clicking its title link does the same.
- **Ctrl/Cmd+click starts the selection mode.** It adds the item to the
  selection. From then on every plain click adds or removes an item,
  until the selection is empty again or Escape ends the mode. Shift+click
  takes the range from the last item; Ctrl/Cmd+A selects every visible
  item.
- **Look:** a pressed item nudges where it is pressed, like a tile but
  smaller (`motion.transform.row-tilt`, `row-press-scale`). A selected
  item takes the **accent** fill with `on-accent` text, and a checkmark
  without a box at its leading edge (the `rk-check` cell of a table, or
  the item's own padding in `rk-rows`). Tags inside keep their word and
  mark. Never a status colour.
- **Keys:** the list is one tab stop; ↑ ↓ (and ← → in a grid) move,
  Home End jump, Space or Enter act like a click (with Ctrl/Cmd or
  Shift as above), Escape ends the selection mode before it would go
  back.
- **The app bar carries it.** In the selection mode the app bar shows
  the count ("3 selected"), the actions that apply to the selection and
  "clear", in place of its own controls. The count is a live region. One
  selected item is not the mode: it is the item being shown.
- An action on a selection that changes data offers undo in a toast and
  puts the rows back where they were.
- Inside an item, a real control (a button, a switch) acts on its own and
  does not select.

## Shown item

In a split view, the item whose detail shows is the selected one: a
plain click selects and shows in one step. In the selection mode the
detail keeps the item it showed.

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

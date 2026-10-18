# States, floating surfaces and layers

## Data states

Every region that shows data (a list, a table, a tile grid) is in one
of four states, and says which.

| State | Looks | Words |
|---|---|---|
| **Loading** | After `timing.busy-delay` (300ms), a 2px line runs along the region's first hairline. What was there stays; on a first load, still placeholders (`rk-ghost`) hold the layout. Once shown, the line stays at least `timing.busy-min` (500ms), so it never flashes. | The kicker may say "loading". |
| **Ready** | The data. | — |
| **Empty** | A sentence in `rk-text-contrast-high` on the first hairline (`rk-empty`), and the way on if there is one. | Say what is empty and why, without blame: "Nothing open. Well done." "No jobs match 'deploy'." |
| **Failed** | The same, in `rk-text-primary` with the error square (`rk-empty` with `data-status="error"`), and a retry. Data that was there stays, marked as from before. | Cause and remedy: "Could not load the jobs: the job server is offline. What you see is from before." |

- **Nothing shimmers.** Movement in Rekta means live data or a
  transition; a placeholder that pulses or shimmers claims a meaning it
  does not have. Placeholders stand still.
- **The region starts at its first hairline.** Put the kicker above the
  region, so the line runs where the content will be.
- Mark the loading region `aria-busy="true"` (`Rekta.busy()` does), so
  assistive technology waits for it.
- Under reduced motion the line stands still at full width, in
  `rk-text-contrast-medium`.

## Pending actions

A button whose action is still running says what it is doing, in the
verb's -ing form, at the same width: "save schedule" becomes "saving
schedule…". It refuses more clicks (`aria-disabled`), and after
`timing.busy-delay` the 2px line runs under it. No spinner: words carry
it. When the action ends, the label comes back and a toast reports the
result; a failure is said at the control that caused it.

## Floating surfaces

Menus, the command palette, the places list and a combobox's list float
next to the control that opened them.

- **Look:** `rk-bg`, a 1px `rk-text-contrast-medium` edge, `rk-radius-s`,
  no shadow. The edge parts them from what is below in both themes and in
  forced colours; a shadow would be the only one in the system.
- **Place:** below the control, aligned to its leading edge; above it
  when there is no room below; never outside the window.
- **Motion:** a fade and a 4px shift away from the control over
  `motion.duration.float` (150ms); it leaves at once.
- **Rows:** one item per row at the density's row height, a lowercase
  verb or option. A chosen option shows a filled square. A key hint
  sits at the end in `rk-meta`. A hairline separates groups.
- **Keys:** ↑ ↓ (wrapping), Home End, a letter jumps to the next item
  starting with it, Enter or Space chooses, Escape closes and returns
  focus to the control, Tab closes and moves on. A click outside closes.
- A disabled item stays in the list, greyed, and says why in its label
  when it is not obvious ("print (needs a printer)").

## Layers

From bottom to top:

| Layer | What |
|---|---|
| Content | Places, panoramas, tiles, rows |
| `layer.sticky` | The section strip sticking on a phone |
| `layer.appbar` | The app bar |
| `layer.banner` | The status banner |
| `layer.toast` | The toast region |
| `layer.float` | Floating surfaces (menus, lists) where the browser has no popover top layer |
| Top layer | Sheets (modal dialogs), floating surfaces (popovers, where the browser has them), in the order they opened |

- One modal at a time. A floating surface may open from inside a sheet;
  it sits above it.
- Escape closes the topmost thing first (see the key layers in
  [navigation.md](navigation.md#who-gets-a-key)).
- A sheet holds focus. Report the result of an action taken in a sheet
  after the sheet has closed, not as a toast beneath its shading.

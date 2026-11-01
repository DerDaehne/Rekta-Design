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

The line goes with the label: once shown, it stays until the label comes
back and leaves in the same moment, never before. No `timing.busy-min`
holds it, as one holds a region's line: the label coming back says the
action has ended, so the line ending with it is that answer, not a flash.

A failure whose control has gone (a background save, an action from a
sheet that has closed, a place the user has left) has nowhere else to
be said: it goes in a toast with `status: 'error'` (`toaster.show(text,
{ status })`). The toast takes the error fill and is announced at once
(`role="alert"`); it names what failed and, where there is one, the way
to try again (`act`). A failure that lasts, such as the server being
unreachable, is a banner state, not a toast. Toasts that report a
success carry no status: the plain toast is the success.

A tag (`.rk-tag`) in a toast says a status in its words, as anywhere.
In the plain, ink toast its word takes the toast's text colour and its
square an edge in that colour, so a status colour that would sink into
the ink still shows; the neutral square keeps 3:1 on the ink (`--check`
holds it), and under more contrast it takes the toast's words' colour. In a toast with a status, the tag reads on the fill as on a
tile.

### A level indicator

A value the user changes with a key (volume, brightness, a keyboard
layout, zoom in an app) shows its new level briefly: a toast without an
action, its icon, the word, a meter and the value in tabular figures
("volume ▬▬▬── 60%"). `toaster.level(word, value, { icon, text })` on
the web, `rk_window_level()` in rekta-core.

- **Once.** It arrives like a toast; pressed again while it shows, it
  stays where it is and only its value glides, as progress does. Its own
  time, `timing.level` (1.5s), starts again with each change: it confirms
  what the key did, so it goes sooner than a toast.
- **One at a time,** like any toast: it takes the open toast's place,
  and a toast takes its. An undo that goes with a replaced toast stays in
  the history (Ctrl/Cmd+Z).
- **No status, no action.** The meter is in the toast's own colour, never
  a status fill: a loud volume is not a warning.
- **Where:** in the toast region, also over full-screen content. A
  product whose app bar is always in view (a desktop shell) may show it
  in the bar's middle instead, pinned while it shows: the same parts in
  one line. One form per product, never both.

## Floating surfaces

Menus, the command palette, the places list and a combobox's list float
next to the control that opened them.

- **Look:** `rk-bg`, a 1px `rk-text-contrast-medium` edge, `rk-radius-s`,
  no shadow. The edge parts them from what is below in both themes and in
  forced colours; a shadow would be the only one in the system.
- **Place:** `space-xs` from the control, at least `space-m` from the
  window's edges (the same margin limits its width). Below the control
  when it fits there; else above it when it fits there; else on the side
  with more room, moved back inside the window: it may then cover the
  control, and its content scrolls. Never outside the window.
- **Alignment:** to the control's leading edge (to its trailing edge
  when the window's edge is too near for that). A control that stands
  in the middle of its container (a clock in the middle of an app bar)
  says so with `data-rk-align="center"` (in rekta-core
  `rk_menu_set_align(menu, RK_ALIGN_CENTER)`): its surface is centred on
  it instead, still inside the window. `place()` and `menu()` on the web
  and rekta-core's menus follow this one rule.
- **Motion:** a fade and a 4px shift away from the control over
  `motion.duration.float` (150ms); it leaves at once.
- **Rows:** one item per row at the density's row height, a lowercase
  verb or option. In a menu, a chosen option (a checkbox or radio item,
  a setting the menu toggles) shows a filled square. In a list that
  picks a field's value (a select's, a combobox's), the chosen option
  carries a tick without a box instead: it is the field's value, not a
  setting. A key hint sits at the end in `rk-meta`. A hairline
  separates groups.
- **Keys:** ↑ ↓ (wrapping), Home End, letters jump to the next item
  starting with it, Enter or Space chooses, Escape closes and returns
  focus to the control, Tab closes and moves on. A click outside closes.
- A disabled item stays in the list, greyed, and says why in its label
  when it is not obvious ("print (needs a printer)"). The app bar's
  "more" does this for a disabled control it holds.

## Side panel

A surface that slides in from the trailing edge, full height, for
switches, sliders and lists that belong to the whole app or system: quick
settings, notifications ([live.md](live.md#a-notification-centre)).
It is a sheet from the side: `<dialog class="rk-sheet rk-sheet--side">`,
opened with `showModal()` and run by `sheet()` like any sheet.

- **Look:** `bg`, like a sheet, with a hairline on its leading edge; no
  status fill. Two tile units wide plus its padding
  (`2 × tile-unit + space-s + 2 × space-m`), never under `pane-min` and
  never wider than the window (a phone: the whole width). The width is
  the same on every facet, so the panel never jumps; a longer text is cut
  with an ellipsis (`data-rk-tip="overflow"` shows it whole).
- **Motion:** whole from the trailing edge over `duration-moderate`, and
  back the same way ([motion.md](motion.md)).
- **Behaves as a sheet:** it shades the rest (`shading` and the blur), a
  tap on the shading closes it, it holds focus, Escape closes it, it sits
  in the top layer and has a visible way out (its close button).
- **Content:** groups, each a heading over rows with a toggle
  (`rk-toggle`) or a slider (`rk-range`); never tiles as switches (a tile
  never toggles). It may carry a pivot for its facets (settings and
  notifications), in the facet level.
- **Input it cannot hold** (a password for a new network) is asked in a
  sheet over it.

### Choosing in place

Picking one of several values in a group (a network, a device, an
output) does not open anything over the panel: the group gives its place
to the list of values, under a row "back · name" (`.rk-in-back`), and the
list slides in like the next item of a place. The back row, Escape and
← go back up to the group, with focus on the control that opened the
list; levels may nest. It is the tree's ← (up to the parent) and the
compact split's crumb, in one surface. `inPlace(scope)` does it on the
web: a control with `data-rk-in="ID"` opens the list with that id in the
place of its group (`[data-rk-group]`). The second level of a menu can
work the same way.

```html
<section class="rk-region" data-rk-group aria-labelledby="g-net">
  <h2 class="rk-kicker" id="g-net">network</h2>
  <ul class="rk-rows"><li><button class="rk-btn rk-btn--quiet" data-rk-in="nets" data-rk-in-name="wi-fi">wi-fi · home</button></li></ul>
</section>
<section class="rk-region" id="nets" aria-label="wi-fi" hidden>…the networks…</section>
```

rekta-core has no side panel yet: its sheets come from the top
([platforms.md](platforms.md#what-rekta-core-does-not-draw-yet)).

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
| Top layer | Sheets (modal dialogs) and side panels, floating surfaces (popovers, where the browser has them), in the order they opened |

- One modal at a time. A floating surface may open from inside a sheet;
  it sits above it.
- Escape closes the topmost thing first (see the key layers in
  [navigation.md](navigation.md#who-gets-a-key)).
- A sheet holds focus. Focus goes back to what had it once the sheet
  has gone, not as it starts to leave. Report the result of an action
  taken in a sheet after the sheet has closed, not as a toast beneath
  its shading.
- A sheet's answer is given as it starts to leave. A close request
  while it leaves (the shading, Escape, another button) changes no
  answer and asks nothing: it only ends the leaving at once.
- **Every sheet has a visible way out**: its safe answer, or a close
  button (`.rk-btn--icon.rk-btn--quiet.rk-sheet-close`, `value="cancel"`,
  `aria-label="close"`) at its top corner. Escape and the shading are not
  enough: a touch screen may have neither key nor gesture, and with
  `closedby="closerequest"` the shading does nothing. The markup is in
  [forms.md](forms.md#a-form-in-a-sheet).
- **Toasts stay below sheets**, always: the toast region is never made a
  popover to reach the top layer. A sheet, or the palette, that has to
  say something while it is open says it inside itself, where the user
  is looking: a message in place under the field or list it is about
  (`.rk-hint`, or `.rk-empty` for a list with nothing to show; motion.md,
  "A message in place"). A toast is for what happened, once nothing
  covers the page; a hint inside the sheet is for what is happening in
  it.

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

## A notification from another program

A product that shows what other programs report (a desktop shell, a
hub, a messenger's web client) shows each notification as a toast, with
the same layer, place and window, and these differences:

- **It enters from the edge.** It came from outside, so it glides in
  whole from the window's edge, without a fade, to where toasts stand,
  and leaves back out the way it came (sideways the top edge; compact,
  where toasts rise from the app bar, the bottom edge). The motion is
  the toast's ([motion.md](motion.md)).
- **Title and text.** The sender's title in semibold, the text under it
  in at most two lines, then an ellipsis. All of it is read out; the cut
  is only visual. Without a title it is one sentence, as any toast.
- **The sender's icon, or a picture.** At the start, in the icon's 20px
  field: the sender's icon in the toast's text colour (a single-colour,
  symbolic one: in an ink strip a brand's colours mean nothing, and
  colour in Rekta carries meaning), or, when the notification brings a
  picture of its own (an avatar, an album cover), that picture, cropped
  square. A picture is content and keeps its colours. Neither stands
  without the sender's name in the title or the text.
- **The text is pressable.** Pressing it runs the notification's default
  action ("open what this is about"); without one, it closes the toast.
  It tints on press and carries the focus ring, as the action does. The
  rule of one action stays: a further action of the notification is the
  toast's `act`, and the rest wait in the place the notification opens.

```js
toasts.show('Are we still on for 3?', { edge: true, title: 'Ana Ruiz', picture: '/avatars/ana.png', open: () => openChat('ana') });
```

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
- **An icon per item** stands at the start, 20px like `.rk-icon`,
  `space-s` before the words (after a chosen option's square, which is
  the menu's own state). Once one item of a list has an icon, every row
  starts its words at the same edge. An icon stands beside its word,
  never instead of it.
- **Words another program supplies** (a status area's menu, a plug-in's
  context menu) are content: they stand as the supplier writes them, in
  its case. The menu's own words (the back row's "back", its labels) stay lowercase.
- **A second level** stands in place of the list, in the same surface,
  never as a second surface beside it: one form for every width and for
  touch, where a cascade has no room. The item that has one ends in a
  chevron pointing on, in place of a key hint; it opens with →, Enter,
  Space or a press. The level's rows replace the list under a back row
  (a chevron pointing back and the level's name, a hairline below it);
  the surface keeps its width and its place (moved back inside the
  window only when the level does not fit there). ← and Escape go back
  one level, to the item that opened it; Tab or a click outside close
  every level. Levels may nest, each the same way. The change has no
  motion: the surface stays put, and the back row says where you are.
  A side panel that chooses in place (a setting's choices replacing its
  list) follows the same rule.
- **Keys:** ↑ ↓ (wrapping), Home End, letters jump to the next item
  starting with it, Enter or Space chooses, Escape closes and returns
  focus to the control (in a second level, goes back one level), Tab
  closes and moves on. A click outside closes.
- A disabled item stays in the list, greyed, and says why in its label
  when it is not obvious ("print (needs a printer)"). The app bar's
  "more" does this for a disabled control it holds.

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

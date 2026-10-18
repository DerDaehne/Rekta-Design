# Forms and decisions

## Fields

A label above, the control, a hint below that turns into the error when
there is one (see the Field component). Required fields carry no mark;
an optional one says "(optional)" in its label (`rk-optional`).

## Inputs

Native where it can be: a select, a number, a date and a time are the
platform's own controls in Rekta's frame (`rk-input`), so their pickers,
keyboard handling and screen-reader support come for free. The rest are
Rekta's own.

| Need | Control | Notes |
|---|---|---|
| one of many, a list you know | `select.rk-input` | a chevron, the system's list |
| one of many, a long list or one to search | combobox (`rk-combo`, `Rekta.combobox()`) | filters as you type; floats below like a menu; without `free`, keeps only a choice from its list |
| two to five options seen at a glance | segments (`rk-segments`) | joined buttons, the chosen one in the accent; radios underneath |
| on or off, acting at once | toggle switch (`rk-toggle`) | `role="switch"`; outside forms that are sent |
| on or off, sent with a form | checkbox (`rk-switch`) | waits for "save" |
| a whole number | stepper (`rk-stepper`) | − field +, repeats while held, says when its limit is reached |
| a value on a scale | slider (`rk-range`) | the accent fills up to the value; an `<output>` shows it |
| a date or a time | `input[type=date | time].rk-input` | the browser's own pickers |
| files | drop zone (`rk-drop`, `Rekta.drop()`) | dashed until files are dragged over it; dropping files elsewhere never opens them |

- **Toggle switch, the Metro way:** a square-cornered frame (2px
  `text-primary`), the `accent` fill growing from the leading edge when
  on, a solid ink bar as the thumb, and the state word ("on", "off")
  beside it. It acts at once, so it never sits in a form that waits for
  "save": use a checkbox there.
- **Combobox:** ↓ or Alt+↓ opens, ↑ ↓ move, Enter chooses, Escape
  closes, then clears (it never goes back while it has something to
  close or clear). The current choice carries a check. A remote list
  shows the loading line while it waits, and says so when it fails.
- **Segments** are for options that fit on one line; more than five, or
  long labels, go in a select.
- **Files:** each file becomes a row with its size, a progress bar while
  it uploads and its status; a file that is too large says why in its
  row, never in a dialog.

## Check late, correct early

- A field is checked when it is left after an edit, never while it is
  first being typed in, and never before the user has touched it.
- Once a field shows an error, it is checked again on every keystroke,
  so the error goes away the moment it is fixed.
- On sending, every field is checked. With errors, a **summary** comes
  first in the form (`rk-errors`): a sentence with the error square ("2
  fields need attention") and a link to each field ("start time: 25:00 is
  not a time of day"), a hairline below, no fill. It takes focus, so it is
  read at once; its links move focus to their field. Each field keeps
  showing its own error.
- Every error is cause and remedy in one sentence, at the field.

## When the server says no

The send runs as a pending action (the button says "saving…"). A refusal
shows the same way as a check: at the field when it names one ("A
project called 'Release notes' already exists. Choose another name."),
or in the summary for the whole form ("Could not save: the database is
read-only until 15:00. Your changes are still here."). Input is never
thrown away by a refusal.

## Leaving with unsaved input

While a form differs from how it was loaded or last saved, leaving its
place (a link, back, Escape, the browser's back or forward) asks first,
in a sheet: "Leave without saving?", with "keep editing" as the safe,
primary answer and "discard changes" beside it. Escape and a tap on the
shading keep editing. Discarding puts the form back as it was. Closing
the window asks the browser's own question. An unchanged form never
asks.

## Dialog levels

How much an action asks depends on what it costs to take back:

| The action | Asks | Example |
|---|---|---|
| Can be undone | Nothing: it happens, and a toast offers undo | archive, mark done, approve |
| Cannot be undone, small | A sheet whose button repeats the verb; the safe answer is primary | delete one draft |
| Cannot be undone, wide | The same sheet, naming what will go ("12 runs and their logs") | delete a project |

Prefer making an action undoable over asking about it. A sheet never
asks "Are you sure?": it names the action and its consequence.

## One size of control

Rekta has one control height per density, `control-height`: 44px where
a touch screen is present, 32px with a fine pointer only. There is no
small button and no small field, on purpose:

- **Touch.** A smaller control on a touch screen would fall below
  `size.touch-target` (44px); one that only looks smaller, with an
  invisible larger hit area, overlaps its neighbours in a row of them.
- **Calm.** Size does not rank controls; the kind does. A secondary
  action is a quiet button (`.rk-btn--quiet`), not a smaller one.
- **Density is the lever.** A user or a product that wants more on the
  screen picks the compact density, and every control shrinks together.

For actions on each row of a list, use a quiet icon button at the row's
height, the row's context menu (right click, long press, the menu key),
or the selection mode's app bar for several rows at once.

## Disabled controls say why

A control that cannot act now stays visible and says why in a hint next
to it ("Archive needs at least one done job."), linked to it, so the
reason is read with it. It stays focusable (`aria-disabled`), unlike a
native `disabled` control, which a keyboard user could not reach to
hear the reason. No tooltips: they do not exist on touch.

`disable(control, reason)` does this: it writes the reason into a
`.rk-hint.rk-why` placed right after the control and linked by
`aria-describedby` (the control's `data-rk-why` names it, so a page can
place the hint itself), and `disable(control, false)` takes it away. In
a `.rk-actions` row the hint takes the rest of the row beside the
buttons (`.rk-actions > .rk-why`).

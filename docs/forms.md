# Forms and decisions

## Fields

A label above, the control, a hint below that turns into the error when
there is one (see the Field component). Required fields carry no mark;
an optional one says "(optional)" in its label (`rk-optional`).

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

## Disabled controls say why

A control that cannot act now stays visible and says why in a hint next
to it ("Archive needs at least one done job."), linked to it, so the
reason is read with it. It stays focusable (`aria-disabled`), unlike a
native `disabled` control, which a keyboard user could not reach to
hear the reason. No tooltips: they do not exist on touch.

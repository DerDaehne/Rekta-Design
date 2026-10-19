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

The summary's markup, as `form()` writes it and as a server renders it
for a form sent without the script:

```html
<form class="fields" novalidate>
  <div class="rk-errors" role="group" tabindex="-1" aria-labelledby="f-errors">
    <p id="f-errors">2 fields need attention</p>
    <ul>
      <li><a href="#start">start time: 25:00 is not a time of day. Use hh:mm, up to 23:59.</a></li>
      <li><a href="#name">name: A project called “Release notes” already exists. Choose another name.</a></li>
    </ul>
  </div>
  <div class="rk-field" data-status="error">
    <label class="rk-label" for="start">start time</label>
    <input class="rk-input" id="start" aria-invalid="true" aria-describedby="h-start" value="25:00">
    <span class="rk-hint" id="h-start">25:00 is not a time of day. Use hh:mm, up to 23:59.</span>
  </div>
  …
</form>
```

- It is the form's **first child**, a `group` named by its sentence,
  focusable (`tabindex="-1"`) but not in the tab order.
- **Focus:** it takes focus as the page arrives (a server-rendered
  refusal: `autofocus` on it, or `focus()` on load) and each time the
  form is sent with errors.
- Each link names the field's label, a colon, then the field's own
  sentence, and points at the field's `id`; following it moves focus to
  the field (without the script, the fragment link does).
- A refusal for the whole form is the sentence alone, without a list.
- The fields keep their own errors (`data-status="error"`,
  `aria-invalid`, the hint), so the summary is never the only place.

The showcase's accent form shows it: send it with an empty name.

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

**The destructive button.** The button that takes an action that cannot
be undone (in that sheet, and the one that opens it) is
`.rk-btn[data-status="error"]`: its frame and words in
`status-error-full`, never filled and never primary, since the safe
answer is primary. `--check` holds its words to 4.5:1 on the ground and
the surface in both themes. It is a frame colour, not a warning sign:
an undoable action (archive) never wears it.

```html
<p class="rk-actions"><button class="rk-btn rk-btn--primary" value="cancel">keep the project</button><button class="rk-btn" data-status="error" value="ok">delete the project</button></p>
```

## A form in a sheet

- **A visible way out.** Every sheet has one a finger can reach: its
  safe answer ("cancel", "keep it"), or, where the sheet has no such
  answer, a close button at its top corner. On a touch screen with
  `closedby="closerequest"` neither Escape nor the shading closes it.

  ```html
  <dialog class="rk-sheet" closedby="closerequest" aria-labelledby="t">
    <form method="dialog">
      <p class="rk-kicker">group</p>
      <h2 class="rk-group" id="t">Rename the group</h2>
      …
      <p class="rk-actions"><button class="rk-btn" value="cancel">cancel</button><button class="rk-btn rk-btn--primary" value="save">rename</button></p>
      <button class="rk-btn rk-btn--icon rk-btn--quiet rk-sheet-close" value="cancel" aria-label="close"><svg class="rk-icon" aria-hidden="true"><use href="#rk-i-close"/></svg></button>
    </form>
  </dialog>
  ```

- **Unsaved input asks before it goes.** One modal at a time, so the
  question is not a second sheet: it takes the sheet's place, inside it.
  `sheet(dialog, { dirty: () => … })` asks while `dirty()` is true, on
  every close request (Escape, the shading, a `value="cancel"` button):
  "unsaved · Discard what you entered?", with "keep editing" as the
  primary, focused answer and "discard changes" beside it. Escape or the
  shading while it asks keep editing; focus goes back where it was. A
  save closes without asking; so does `dialog.close()`, the app's own
  decision. An unchanged form never asks. (A browser closes a dialog on
  a second Escape pressed with no click or key in between, whatever a
  page does; a question the user is answering has had one.)

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

**The icon button** is `.rk-btn--icon` (with `.rk-btn--quiet` for row
actions and a sheet's close): square at `control-height`, at
`row-height` inside `.rk-rows` and `.rk-table`, never smaller than
`size.touch-target` under a coarse pointer, whatever the density. The
glyph is 20px. It has no words, so it **needs an `aria-label`** (a
tooltip, `data-rk-tip`, may repeat it for the mouse); the focus ring is
the button's own.

```html
<button class="rk-btn rk-btn--icon rk-btn--quiet" type="button" aria-label="more for build 4182" data-rk-tip="more"><svg class="rk-icon" aria-hidden="true"><use href="#rk-i-more"/></svg></button>
```

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

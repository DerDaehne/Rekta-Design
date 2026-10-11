# Accessibility

## Keyboard first

Every action reachable by pointer is reachable by keyboard, not the
other way around — keyboard support is not an add-on layered onto a
mouse-first design. Panoramas and pivots both have a keyboard model for
moving along them (see [navigation.md](navigation.md#keyboard)); a
sideways-scrolling panorama that can only be driven by a mouse wheel or
drag is not a complete implementation of it. A jump to a section moves
focus to its heading; a place change moves focus to the new place's
title, and back returns it to the control that opened the place.

## Focus on fills

A focus ring must stay visible no matter what it's drawn on, including
a full-colour tile fill (see [tiles.md](tiles.md)) — a ring colour that
works on a neutral background but disappears on a status fill is a
defect, not an edge case. The practical rule from the source
implementation: draw the ring in the *current text colour* of whatever
it's on (inside the element, as an inset ring) rather than in a single
fixed accent colour, since text colour was already chosen to clear AA
against every fill it can appear on (see [colour.md](colour.md)). A
fixed-colour ring is acceptable only on neutral surfaces where it is
separately contrast-checked (`focus` clears ≥ 3:1 against both `bg` and
`surface` in both themes in the source tokens).

## Reduced motion

Covered in full in [motion.md](motion.md): every duration token goes to
zero, every transition becomes instant rather than skipped, and this is
driven by both the OS-level `prefers-reduced-motion` signal and an
explicit in-app override, so a user who wants it off can turn it off
even on a platform/OS that doesn't expose the system setting.

## Touch targets

Any interactive control reachable on a touch surface has a minimum
target size of 44px on a side (44×44 CSS px in the web build); controls
in a thumb-reachable bottom bar on narrow viewports (see
[navigation.md](navigation.md)) are sized at the upper end of that range
rather than the minimum, since they're deliberately placed for one-handed
reach.

## Pointer and touch

- **Hover is never the only way.** Hover tints exist only where hover
  exists (`(hover: hover)`); on touch they would stick after a tap.
- **Dragging never activates.** A drag that starts on a tile or row, or
  a text selection inside one, cancels the click. A fine pointer can
  select text in tiles and rows; touch gets no selection and no
  long-press callout on them.
- **Fields stay at 16px on touch**, so a phone never zooms the page
  when a field takes focus.
- **Sheets keep the page still.** The page behind a sheet does not
  scroll, and a tap on the shading cancels the sheet unless it holds
  unsaved input.

## Also non-negotiable (carried from the source product, kept general)

- **Colour is never the only signal.** Every status tile also states its
  status as a word (see [tiles.md](tiles.md)), the app bar's live square
  always has its word beside it, and every chart has a text
  equivalent (an accessible name/description stating the current value
  and trend in words, not just a line).
- **Contrast is checked, not assumed.** See [colour.md](colour.md) and
  `node scripts/build.mjs --check`, which fails the build when a text,
  mark, focus or soft pair drops below its minimum, or when two statuses
  come closer than 0.09 (OKLab) under protanopia or deuteranopia.
- **Errors state cause and remedy together**, at the point of the error
  — not just "something went wrong," and not in a location disconnected
  from the control that caused it.

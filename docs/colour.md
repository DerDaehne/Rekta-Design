# Colour

## Neutral surfaces

The ground (`color.*.bg`), surface (`color.*.surface`) and its hover
step (`color.*.surface-2`) are all neutral — near-white/near-black and
two close greys. There is no selectable brand accent colour; Rekta Design
does not pick a hue to represent "the product." Hierarchy between
content comes from typography, spacing and hairlines
(`color.*.text.contrast-low`), not from a tint.

## Status fills, not an accent

Four status hues carry meaning, and only meaning — never branding:

| Status | Means |
|---|---|
| `info` | active / in progress |
| `warning` | waiting / paused |
| `error` | failed |
| `success` | done |

Each status has two forms in the tokens:

- **`status.<name>.full`** — fully saturated. Used only for *small*
  marks: a tag's coloured dot, a ticker square, a sparkline, a progress
  bar. Small shapes can carry a loud colour without overwhelming the
  page.
- **`status.<name>.tile`** — the same hue dampened toward the ground
  (mixed with white in the light theme, with the surface colour in the
  dark theme). This is the only form ever used as a **large fill** (a
  tile's whole background) — see [tiles.md](tiles.md). Muted large
  areas, vivid small accents.

There is also a `status.<name>.soft` form (hue mixed heavily toward the
ground, closer to a tint than a fill) kept for secondary uses such as a
soft background behind a status label in running text.

## Contrast rule: AA on every fill

The primary text colour (`color.*.text.primary`) is used as the text
colour on top of **every** tile fill, in both themes — never a
status-specific text colour. This is a hard constraint, not a
convenience: every tile fill was chosen so that primary-on-tile clears
WCAG AA (≥ 4.5:1) for normal text, in both the light and dark token
sets. `scripts/build-css.mjs --check` verifies exactly these eight pairs
(four statuses × two themes) and fails the build if a future token edit
breaks one.

Secondary text (`color.*.text.contrast-high`) is used on neutral
surfaces, never on a status tile — its contrast was only checked against
`bg`/`surface`, not against the dampened status hues.

## Dark theme is not an inverted light theme

`color.dark.*` is an independently chosen token set, not a formula
applied to `color.light.*`. Notably, warning's dampened mix is lighter
relative to its surroundings in the dark theme than in the light theme —
dark-theme warning is the one status whose AA margin is tighter (5.3:1
vs. 14.8:1 in light), which is why the self-check measures every pair
rather than assuming symmetry.

## Focus, hover, disabled

`focus` differs between themes (a darker blue in light, a brighter blue
in dark) specifically so the focus ring clears AA against whatever it's
drawn on in that theme — a single focus colour used in both themes failed
contrast on at least one surface in testing. `hover` is a translucent
neutral overlay (same rgba value in both themes, since it is applied on
top of the theme's own surface colour). `disabled` is a single flat grey
per theme, used for text and borders, never as a fill.

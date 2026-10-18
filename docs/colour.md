# Colour

## Neutral surfaces

The ground (`color.*.bg`), surface (`color.*.surface`) and its hover
step (`color.*.surface-2`) are all neutral — near-white/near-black and
two close greys. Rekta Design does not pick a hue to represent "the
product": its one product slot, the **accent** (`color.*.accent` with
`on-accent`), is ink until a product sets it per theme. It fills the
primary button, checked controls and a selected item, never a tile or a
status, and must hold 3:1 on the ground and stay clearly apart from
every status hue. Hierarchy between
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
sets. `node scripts/build.mjs --check` verifies these eight pairs (four
statuses × two themes), together with every other text, mark, focus and
soft-tint pair, and fails the build if a future token edit breaks one.

Secondary text (`color.*.text.contrast-high`) is used on neutral
surfaces, never on a status tile — its contrast was only checked against
`bg`/`surface`, not against the dampened status hues (on the light error
tile it falls to 1.9:1). `rekta.css` enforces this: inside a status tile
or banner, `--rk-text-contrast-high` takes the primary text colour, so a
meta, a facts line, an age or a stale value placed there the documented
way stays at AA. A product that draws its own secondary text on a fill
has to do the same: `--check` measures token pairs, not the selectors
that combine them.

## Dark theme is not an inverted light theme

`color.dark.*` is an independently chosen token set, not a formula
applied to `color.light.*`, and the margins are not symmetric. Primary
text on the tile fills measures, at the time of writing: dark success
4.99:1, the tightest pair; light error 5.25:1, the next; dark warning
6.62:1 against light warning 14.83:1. That is why the self-check measures
every pair rather than assuming symmetry, and why the numbers here are an
illustration: `node scripts/build.mjs --check` is the authority.

## Focus, hover, disabled

`focus` differs between themes (a darker blue in light, a brighter blue
in dark) specifically so the focus ring clears AA against whatever it's
drawn on in that theme — a single focus colour used in both themes failed
contrast on at least one surface in testing. `hover` is a translucent
neutral overlay (same rgba value in both themes, since it is applied on
top of the theme's own surface colour). `disabled` is a single flat grey
per theme, used for text and borders, never as a fill.

## Status vocabulary

Four statuses and their absence, each with one fixed word. The word is
always shown with the colour (a tile's status word, a tag's word, the
banner's word), and the same state uses the same word everywhere in a
product.

| `data-status` | Word | Means |
|---|---|---|
| `info` | active | Running, in progress, live |
| `warning` | waiting | Paused, queued, waiting for someone |
| `error` | failed | Stopped by a fault; needs action |
| `success` | done | Finished as intended |
| none | idle | Nothing is happening; no status |

A product may add words for its own states (offline, halted, read-only
in a banner) but maps each onto one of these colours and never reuses a
word for two states. A status is never decoration: no status colour for
emphasis, selection, branding or a chart series.

## Selection and floating surfaces

- **Selected items** take the `accent` fill with `on-accent` text and a
  checkmark without a box at the leading edge, the Metro way. A status
  inside a selected item keeps its word and its mark (edged in
  `on-accent`), so a selected failed job still reads as failed.
  Selection is never a status colour.
- **Floating surfaces** (menus, the command palette, the places list, a
  combobox's list) are `bg` with a 1px `text-contrast-medium` edge and
  `radius-s`. No shadow: the edge parts them from what is below in both
  themes, and in forced colours.

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

## Text on glass

`glass` is the app bar's frosted ground: the ground colour at 82%, with
a blur of what scrolls under it. The blur averages that content, so the
limits are all white and all black behind it (a light document under
dark glass). Only `text-primary` and `text-contrast-high` stand on
glass; `text-contrast-medium` there falls to about 3.3:1. `--check`
measures both over white and over black (4.5:1, 7:1 under more
contrast) and fails when `rekta.css` colours a part of the app bar in
anything lower. Disabled controls stay in `disabled`: they are not text
to read.

## Dark theme is not an inverted light theme

`color.dark.*` is an independently chosen token set, not a formula
applied to `color.light.*`, and the margins are not symmetric. Primary
text on the tile fills measures, at the time of writing: dark success
4.99:1, the tightest pair; light error 5.25:1, the next; dark warning
6.62:1 against light warning 14.83:1. That is why the self-check measures
every pair rather than assuming symmetry, and why the numbers here are an
illustration: `node scripts/build.mjs --check` is the authority.
Under more contrast every one of them holds 7:1 (below).

## More contrast, less transparency

Two settings people choose for themselves, and Rekta answers both. In
the normal case no value changes.

- **More contrast** (`prefers-contrast: more`, or `contrast: more` in
  the prefs): each theme takes its contrast set, `color.light-contrast`
  or `color.dark-contrast`, which holds only the values that change.
  Every text pair holds **7:1** (WCAG AAA): `text-contrast-high` and
  `text-contrast-medium` are raised, ink on every tile and soft tint
  holds 7:1, and the tiles where it did not (light error, dark success
  and warning) are shifted until it does, with their neighbours moved
  just enough to stay 0.09 apart under red-green colour blindness. Each
  status tile gains a **2px frame in its full colour**, and its word
  stays: the frame and the word now carry the status, not the calm fill
  alone. A destructive button's words take ink inside its error frame.
  Glass is opaque (the ground). The accent is the product's: the
  default (ink with `bg` on it) holds 7:1; a product's own accent should
  too, set in the same contrast selectors where it does not.
- **Less transparency** (`prefers-reduced-transparency: reduce`, or
  `transparency: reduce` in the prefs): glass becomes the ground colour,
  and nothing blurs, neither the app bar nor the shading behind a sheet.

`node scripts/build.mjs --check` measures both contrast sets like the
themes, at 7:1 for text and 3:1 for marks, with the same colour-blind
distances. Forced colours (Windows contrast themes) stay as they are:
the system's colours replace Rekta's there, and the frame goes with
every other shadow.

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

Colours Rekta did not choose follow the same rule: an app's icon, a
favicon or a logo keeps its own colours, stands only on a neutral ground,
and never says a status ([shell.md](shell.md#picture-icons)).

That holds for Rekta's own mark too: a line with a square, both in ink
(`text-primary` of the theme the mark stands in; the showcase's icon
draws its own ink ground and the mark in the ground's colour, one colour
for line and square). The square turns `success` only where it reports
"done", such as a splash that shows it once the app is ready.

## Product slots

A product has two colour slots of its own, both set per theme:

- **The accent** (`--rk-accent`, `--rk-on-accent`): its one hue, for the
  primary button, checked controls and a selected item.
- **A domain colour code** (`--rk-series-1` to `-8`): colours its users
  already read as information, such as heart-rate zones, for small data
  marks only ([data.md](data.md#a-domains-colour-code)). Never text,
  never a tile; 3:1 on the ground; apart under colour blindness.
  `build.mjs --check-product` checks a product's values.

Everything else is the design's ([platforms.md](platforms.md#product-overrides)).

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

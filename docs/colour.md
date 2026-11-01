# Colour

## Neutral surfaces

The ground (`color.*.bg`), surface (`color.*.surface`) and its hover
step (`color.*.surface-2`) are all neutral — near-white/near-black and
two close greys. Rekta Design does not pick a hue to represent "the
product": its one product slot, the **accent** (`color.*.accent` with
`on-accent`), is ink until a product sets it per theme or picks a named
one. It fills the primary button, checked controls, a selected item and
at most one accent surface per place, never a tile or a status, and
must hold 3:1 on the ground and stay apart from status
([The accent](#the-accent)). Hierarchy between
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
  default (ink with `bg` on it) and every named accent hold 7:1 with
  their own contrast pair; a product's own accent should too, set in the
  same contrast selectors where it does not (`--check-product` warns,
  `--accent` derives one). Content fills go back to the surface: a set
  of tiles is plain, and only a status keeps a colour.
- **Less transparency** (`prefers-reduced-transparency: reduce`, or
  `transparency: reduce` in the prefs): glass becomes the ground colour,
  and nothing blurs, neither the app bar nor the shading behind a sheet.

`node scripts/build.mjs --check` measures both contrast sets like the
themes, at 7:1 for text and 3:1 for marks, with the same colour-blind
distances. Forced colours (Windows contrast themes) stay as they are:
the system's colours replace Rekta's there, and the frame goes with
every other shadow.

## The accent

One hue is the product's: the accent and the text on it, per theme. It
fills the primary button, checked controls, a selected item, selected
text and at most one accent surface per place. It is never a tile fill
and never a status. Ink is the default.

### Apart from status

An accent must never read as a status. Measured per theme against every
`status-*-full` and `status-*-tile` of that theme, it holds:

| Rule | Number |
|---|---|
| Contrast on `bg` and on `surface` | 3:1 |
| `on-accent` on the accent | 4.5:1, 7:1 under more contrast |
| OKLab distance, seen | 0.09 |
| OKLab distance under protanopia and under deuteranopia | 0.05 |
| Hue (OKLCH) from each `status-*-full` | 20°, unless the accent's chroma is below 0.05 (a grey has no hue to mistake) |

Tritanopia is measured and reported, not required, as for the statuses
among themselves. 0.09 is the distance the statuses keep from each
other. Under red-green colour blindness the accent asks less (0.05)
because it is never the only signal: a selection has its checkmark, a
button its words, a checked control its mark. At 0.09 there, almost only
violets would be left.

Each number can be recomputed without JavaScript: OKLab and OKLCH from
sRGB (Björn Ottosson's matrices), the simulation of protanopia,
deuteranopia and tritanopia after Machado, Oliveira and Fernandes 2009
at severity 1, and the WCAG 2 contrast ratio. `scripts/lib/color.mjs`
has them in under 200 lines, with the matrices; it ships as `rekta.color.js`
(and `rekta.color.global.js`, `window.RektaColor`), and rekta-core has
the same in C. `apartFromStatus(colour, theme)` gives, per kind of
vision, the nearest status colour and the distance to it, the nearest
status hue and what breaks the rule in words.

The four statuses take red, yellow, green and blue. What is left is the
arc between green and blue (teal, petrol) and the one from blue through
violet to pink, and the greys.

### Named accents

Rekta ships checked accents, each a pair per theme and per contrast set
(`tokens/rekta.accents.json`, `accents` in `rekta.tokens.json`). A
product or its user picks a name instead of four pairs:

| Name | Light | Dark | Text on it |
|---|---|---|---|
| `ink` | `text-primary` | `text-primary` | `bg`: the default |
| `teal` | `#049896` | `#00aba9` | dark |
| `steel` | `#637586` | `#6c7e8f` | white in light, dark in dark |
| `violet` | `#65389f` | `#c2a7f4` | white in light, dark in dark |
| `mauve` | `#756089` | `#7a648e` | white |
| `pink` | `#d858b6` | `#f472d0` | dark |
| `magenta` | `#d70473` | `#d70473` | white |

Under more contrast each has its own darker or lighter pair at 7:1 (in
the file). On the web, `data-accent="teal"` on the root picks one, or
`Rekta.prefs.set('accent', 'teal')`, which remembers it; a named accent
wins over a product's own `--rk-accent`, and `product` (the default)
goes back to it. rekta-core: `rk_app_set_accent_named("teal")`.
`--check` holds every pair to the rules above. The set grows only if the
status colours change or the rules soften: five hues and two greys are
fewer than Metro's twenty, and Metro itself advised using loud colours
sparingly.

### Deriving an accent

A product that has one colour (its brand, or a colour a user typed) can
let Rekta find the pairs. The method, the same on every platform:

1. Take the colour's OKLCH lightness `L`, chroma `C` and hue `h`.
2. If `C` is 0.05 or more and `h` lies within 20° of a status hue of a
   theme, refuse it in that theme: no lightness changes a hue.
3. Otherwise try lightnesses from 0 to 1 in steps of 0.005, nearest to
   `L` first (of two as near, the darker). At each, the colour is
   `(L', C, h)`; outside sRGB its chroma comes down until it fits.
   `on-accent` is the theme's `bg` or `text-primary`, whichever has more
   contrast on it.
4. The first lightness where every rule of the table holds, at the
   theme's text contrast (4.5:1, 7:1 under more contrast), is the
   accent for that theme.
5. A theme where none holds refuses the colour, and says what breaks at
   its own lightness.

`node scripts/build.mjs --accent '#049896'` prints the CSS for every
theme and contrast set, with the selectors of
[platforms.md](platforms.md#product-overrides) and the measured numbers;
`deriveAccent(colour, themes)` in `rekta.color.js` does it in a page.
Computed colours are not designed ones: dark is not an inversion of
light, and a derived pair is a substitute for a chosen one, not the
main path.

### Letting users choose

A product may pass the choice of accent to its users. Then:

- **Offer only pairs that pass.** The named accents do; a product's own
  list is checked with `--check-product`.
- **A colour a user types is checked or derived, never taken as it is.**
  Refused, it stays unchosen, and the reason says why in the user's
  terms: too faint on the ground (and which way to go), or too close to
  a status (which one, and that it would read as it), from what
  `accentCheck` and `apartFromStatus` return.
- **Per theme.** A user picks one accent; the product applies its pair
  for each theme and contrast set.
- **Off under forced colours.** The system's colours replace it, as
  every other colour.

### Accent surface

One place-sized area per place may show the accent: a header, a welcome
or a lock screen, with large, light type in `on-accent` on it
(`.rk-accent-surface`). It carries the product's identity, not
information, so it stays rare:

- **At most one per place**, never a second beside it, and never inside
  a tile, a row or a sheet.
- **No status inside it.** A status tile, a banner or a status tag does
  not stand on it; a status the place has goes on the ground below.
- **Words in `on-accent`**: every text colour on it takes `on-accent`
  (4.5:1 by the accent's rule, 7:1 under more contrast; Rekta asks
  4.5:1 even for large type). A primary button on it turns over:
  `on-accent` with the accent's words. Focus rings take the words'
  colour.
- **With ink as the accent the surface is ink.**
- **Forced colours:** the fill goes; the ground with a 1px frame stays.
  Reduced transparency changes nothing: the surface is opaque.

### Content fills

A set of tiles without a status (programs, documents, bookmarks) may
take the main colour of what each tile opens, the Metro way of a tile
carrying the colour of its app. The fills are the five content tokens
(`content-teal`, `-steel`, `-violet`, `-plum`, `-rose`): muted so ink
holds AA on each, and apart from status by the same rule as the accent.
Their limits:

- **Opt-in, per set**, and one switch turns the whole set plain again.
- **Never beside a status.** A set with a status tile in it is plain;
  rekta.css and rekta-core ignore the fills there.
- **Off under more contrast** (the contrast sets make them the surface)
  and under forced colours.
- **Never text, never a mark, never a status.**

[tiles.md](tiles.md#content-fills) has the markup and how a product
picks a tile's fill.

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

That holds for Rekta's own mark too: a line with a square, both in ink
(`text-primary` of the theme the mark stands in; the showcase's icon
draws its own ink ground and the mark in the ground's colour, one colour
for line and square). The square turns `success` only where it reports
"done", such as a splash that shows it once the app is ready.

## Product slots

A product has two colour slots of its own, both set per theme:

- **The accent** (`--rk-accent`, `--rk-on-accent`): its one hue, for the
  primary button, checked controls, a selected item and one accent
  surface per place; its own pairs or a named one
  ([The accent](#the-accent)). `build.mjs --check-product` checks a
  product's values.
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

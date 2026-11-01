# Tiles

## No cards

A tile is not a card. A card is a neutral container with a border or
shadow that *holds* content of any colour. A tile's entire area *is* a
single fill, and that fill *is* the content's status — there is nothing
neutral about a tile and nothing to put "inside" a border, because
there is no border. See [colour.md](colour.md) for which fill values are
allowed on a tile (always the dampened `status.<name>.tile` form, never
`full`).

## Full-colour status tiles

A tile's background communicates its status before the user reads a
single word on it. Fills come only from the status token set (see
[colour.md](colour.md)); a tile with no particular status uses the plain
neutral `surface` fill. The one exception is a set of tiles that has no
status at all, which may opt in to content fills (below). Text on a tile is always the theme's primary
text colour (checked for AA against every status fill — see
[colour.md](colour.md)), never a status-matching text colour.

A tile's **size** is driven by the importance of what it shows, not by
a uniform grid cell: something that needs the user's attention (an open
question, something failed) is given a larger tile than something
merely informative. A grid of tiles therefore reads as a priority map at
a glance, before any individual value is read.

## Content fills

A set of objects without a status, such as a launcher's programs,
documents or bookmarks, may give each tile the main colour of what it
opens: the Metro way of a tile carrying its app's colour. The tile's
colour then says "this program", not a status, and that is why it has
limits ([colour.md](colour.md#content-fills)):

```html
<div class="rk-tiles" data-rk-fills="content">
  <a class="rk-tile" href="/mail" data-rk-fill="teal"><span class="rk-stat"><b>mail</b></span></a>
  <a class="rk-tile" href="/notes" data-rk-fill="violet"><span class="rk-stat"><b>notes</b></span></a>
  <a class="rk-tile" href="/files"><span class="rk-stat"><b>files</b></span></a>
</div>
```

- **Opt-in on the set:** `data-rk-fills="content"` on the `.rk-tiles`.
  Without it, or with `plain`, every tile of the set is `surface`: one
  switch, and a product hands that switch to its users.
- **Five fills:** `teal`, `steel`, `violet`, `plum`, `rose`, as
  `data-rk-fill` on a tile (`content-*` tokens). Each is muted so ink
  holds 4.5:1 on it, and stands apart from every status colour (0.09
  OKLab seen, 0.05 under red-green colour blindness, its hue 20° from
  each status hue): `--check` holds both. No secondary text colour on
  them: as on a status tile, secondary text takes ink.
- **Never with a status.** A set that has a status tile in it shows no
  content fill at all: the status tile keeps its fill and every other
  tile is plain. Do not build such a set: a colourful grid hides the one
  tile that failed. Put status tiles in a set of their own.
- **Off under more contrast and forced colours:** the set is plain, the
  tiles' words and icons stay.
- **Picking a tile's fill:** from the main colour of the icon of what it
  opens, the same way every time. Take the icon's opaque pixels with a
  chroma (OKLCH) of 0.05 or more; their most common hue, in 10° bins, is
  its main hue. Without such pixels (a grey icon, one drawn in the text
  colour), or with a main hue more than 30° from every fill's hue, the
  tile stays plain: a red, yellow or green icon does not become a
  colour that is not its own. Otherwise the fill whose hue is nearest.
  The same icon gives the same fill.
- **The icon on it** keeps its own colours, as the product draws it;
  Rekta's own icons on a tile are drawn in ink.

rekta-core: `rk_tile_grid_set_content(grid, true)` and
`rk_tile_set_content(tile, RK_ROLE_CONTENT_TEAL)`, declared `.content`
and `.content_fills`.

## Radius 0–2

Tiles use `radius.tile` (2px in the source tokens, deliberately in the
0–2px range) — just enough to avoid a razor-sharp corner, nowhere near
the rounder radii (`radius.s/m/l`, 4/8/12px) used elsewhere for buttons,
fields and dialogs. A tile should read as *tile-shaped*: the small,
nearly-square unit of an information grid, not a soft card.

## Tile content patterns

A tile's content follows one of a small number of repeatable patterns,
rather than ad-hoc layout per tile type:

- **Stat pair** — one large tabular number (medium weight, so it reads
  as data) with a short lowercase label beneath it. The basic unit of a
  tile: "what is the one number that matters here, and what is it
  called."
- **Sparkline** — a small inline line or bar chart tracing a recent
  trend, drawn in the tile's text colour (never a separate chart
  colour) so it stays legible on every status fill. Present only on
  tiles representing something live/ongoing; a finished item's tile
  omits it rather than showing a flat or stale line.
- **Progress** — a thin linear meter (a few px tall), also drawn in the
  tile's text colour, for anything with a known completion fraction. It
  never needs a numeric label next to it when the tile already states
  the fraction in a stat pair.
- **Status word** — the status is also spelled out as a small word
  (not just implied by colour), typically top-right, so the tile's
  meaning does not depend on colour perception alone.
- **Message** — one plain sentence (`.rk-tile-msg`, size `s`) for a tile
  whose content is a statement rather than a number: "The import has run
  every night this month." A kicker (`.rk-kicker`) may name it above,
  and a fact line (`.rk-facts`) may carry one small technical value
  below ("30 of 30 runs"). About 60 characters fit a 1 × 1 tile and 140
  a 2 × 1; past four lines (six on a wide tile) the sentence ends in an
  ellipsis, and the whole of it is still read by screen readers. A
  longer text is not a tile: open a place where it can be read whole, or
  put it on the ground as a statement block
  ([typography.md](typography.md#statement-block)). A message tile is
  neutral (`surface`) unless what it says is a status, then it takes that
  fill and status word like any other tile.

- **Preview** — a running item seen live (a window, an open document,
  a session), two units wide (`.rk-tile--preview`). The top part is the
  preview: a picture or an element the app supplies (a capture, a
  video, a canvas), cropped to fill (`.rk-tile-preview`). It never
  carries text, status or a fill of its own: those are the tile's. The
  foot, on the tile's surface, holds the program's icon and the item's
  title in one line, cut with an ellipsis (`.rk-tile-foot`). Without a
  preview the top part is the surface. A press brings the item to the
  front. How the preview is captured, and how often it is refreshed, is
  the app's; Rekta sets its frame.

  ```html
  <button class="rk-tile rk-tile--preview" type="button">
    <span class="rk-tile-preview"><img src="capture.png" alt=""></span>
    <span class="rk-tile-foot"><svg class="rk-icon" aria-hidden="true"><use href="#rk-i-file"/></svg><span>Quarterly report.odt</span></span>
  </button>
  ```

A tile combines at most two or three of these — a tile that tries to
show a stat pair, a sparkline, a progress meter and a long status
description all at once has stopped being a tile and become a page that
needs its own screen.

## What a tile does

A tile **opens** something, or it is only information. It never changes
data.

- **A tile that opens** is a link (to a place) or a button (that opens a
  sheet). It nudges back where it is pressed, and a link turns the place
  (see [motion.md](motion.md)). A grid of them is one tab stop, the
  arrows moving to the nearest tile that way
  ([navigation.md](navigation.md#a-grid-of-tiles)). There is no arrow glyph on it: the press
  and the turn say that it goes somewhere.
- **A tile that only informs** is not a control. It does not press and
  takes no focus.
- **A tile never toggles, cycles or deletes.** A tile's status changes
  because its data changed, never because it was clicked: a click that
  changes what the tile shows would make every tile a potential
  accident on a board people tap to look closer.

Rows follow the same split, one control per meaning:

| In a row | Is | Does |
|---|---|---|
| The title | A link, when the row opens a place | Opens it |
| A state | A toggle switch (`rk-toggle`): it acts at once ([forms.md](forms.md#inputs)) | Sets the state (done, enabled) |
| An action | A button with a verb | Acts on the row (approve, retry) |

A row is never a single button that does one of these by surprise.

### Status region: figures with their own controls

A running job's figures next to pause, cancel and start is common, and
it is not a tile: a tile never changes data. It is a **status region**,
a plain region on the ground with the figures as a tile shows them and
its controls below.

```html
<section class="rk-region rk-status-region" aria-labelledby="job">
  <p class="rk-kicker" id="job">nightly build · <span class="rk-tag" data-status="info">running</span></p>
  <div class="rk-figures">
    <span class="rk-stat"><b>412</b><span>tests run</span></span>
    <span class="rk-stat"><b>3</b><span>failed</span></span>
    <span class="rk-stat"><b>6 min</b><span>left</span></span>
  </div>
  <p class="rk-actions"><button class="rk-btn">pause</button><button class="rk-btn" data-status="error">cancel the build</button></p>
</section>
```

- **Spacing:** `.rk-status-region` gives it a section's rhythm:
  `space-m` between the parts (figures and actions, as in a sheet or a
  section) and `space-s` between the kicker and the figures (as in a
  tile, where the kicker stands over its figures). `.rk-region` alone
  adds no space: lists and tables are regions too.
- **Not a tile:** no fill (its status is a tag in the kicker, never the
  ground), no press, no focus on the region, no link around it.
- **Figures** in `.rk-figures` (`.rk-stat` each: the number over its
  words), live under the live contract ([live.md](live.md)) like any
  region.
- **Controls** in `.rk-actions` under the figures, verbs that act on
  what the figures describe ("pause", "cancel the build"), the usual
  rules: one primary at most, a destructive one framed in the error
  colour ([forms.md](forms.md#dialog-levels)), a running one pending, one
  that cannot act now disabled with its reason.
- **Where it ends:** a region that opens something more is a tile (a
  link) without controls, and its controls live in the place it opens.
  Never both: a press on figures that sit next to a cancel button must
  not be the way into the details. A board of many jobs stays tiles; the
  status region is for the one thing a place is about.

## Press

A tile that opens something tilts back towards where it is pressed, by
up to `motion.transform.tile-tilt` (8°), and shrinks to
`tile-press-scale` over `duration-press`; pressed in the middle it sinks
straight back. It comes back over `duration-short`. A quick tap still
shows the whole sink. While pressed, its hit area grows by `space-m`, so
a release on the edge that moved away still lands. Dragging, or
selecting text in the tile, never activates it. On touch, the press
waits `timing.tap-delay` (100ms) and gives up on 8px of movement, so the
start of a scroll never sinks a tile.

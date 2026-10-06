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
neutral `surface` fill. Text on a tile is always the theme's primary
text colour (checked for AA against every status fill — see
[colour.md](colour.md)), never a status-matching text colour.

A tile's **size** is driven by the importance of what it shows, not by
a uniform grid cell: something that needs the user's attention (an open
question, something failed) is given a larger tile than something
merely informative. A grid of tiles therefore reads as a priority map at
a glance, before any individual value is read.

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

A tile combines at most two or three of these — a tile that tries to
show a stat pair, a sparkline, a progress meter and a long status
description all at once has stopped being a tile and become a page that
needs its own screen.

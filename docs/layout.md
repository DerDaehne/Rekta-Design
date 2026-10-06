# Layout

## Panorama

A panorama is a single place rendered as a canvas wider than the
viewport, divided into named sections placed side by side. The user
scrolls through it horizontally; a thin section strip (or just the
section titles) hints at what's further along.

- **Desktop (≥ `breakpoint.sideways-scroll-min`, 760px):** the panorama
  fills the height between the app's fixed chrome (its top and/or bottom
  bars). Sideways motion comes from a horizontal drag, a scrollbar, or —
  distinctively — the vertical mouse wheel (see below and
  [navigation.md](navigation.md)). Sections scroll *vertically within
  themselves* if their own content overflows; the panorama itself never
  grows taller than its box.
- **Mobile (< 760px): vertical only.** Sections stack top to bottom and
  the whole page scrolls vertically, like any normal mobile page.
  Sideways motion on a phone reads as unnatural — it is never used for
  page content there, only for the pivot title strip (see
  [navigation.md](navigation.md)), which stays a horizontal strip at
  every width.

## Sideways scrolling by mouse wheel (desktop)

On desktop, a plain vertical wheel gesture over a panorama moves it
*sideways*, not up and down — the panorama's own content has no vertical
scroll of its own, so the wheel is free to mean "move along this
place." Rules, in order:

1. If the pointer is over a part of the page that still has room to
   scroll vertically in the wheel's direction (e.g. a long list inside a
   section), that inner scroll wins — nothing is sideways-hijacked out
   from under genuinely vertical content.
2. Horizontal input — a trackpad swipe, or holding Shift while using a
   wheel — is left alone and scrolls natively. Only a "pure vertical"
   wheel signal is redirected, so nothing scrolls twice.
3. At either end of the panorama, the wheel gesture falls through to the
   page (so a user who keeps scrolling past the end gets normal page
   behaviour, not a dead stop).
4. Below the 760px breakpoint, the wheel is never redirected — mobile
   (and anything that width implies, like a small tiled window) always
   scrolls vertically.

This is a deliberate, named exception to "the wheel scrolls the page":
it only applies to panoramas, pivot title strips, and similarly
structured sideways collections, never to ordinary vertical content.

## Breakpoints

Four widths are explicit design targets, not just "it doesn't break":
at each, the layout is reconsidered for how much more a bigger screen
can usefully show, not just stretched.

| Breakpoint | Token | What changes |
|---|---|---|
| 375px | `breakpoint.mobile` | Panoramas and multi-column grids stack into a single column (two for tile grids); the pivot header is the only thing that still scrolls sideways, as a strip. |
| 1440px | `breakpoint.desktop` | A panorama shows several sections side by side, enough that the next one is visibly starting at the edge. |
| 1920px | `breakpoint.wide` | All of a panorama's sections fit side by side without scrolling. |
| 2560px | `breakpoint.ultrawide` | Tile grids gain more columns (the tile unit grows slightly, from `--unit` 150 to 170 in the source prototype) and individual tiles can show a second level of detail (e.g. a second chart) instead of sitting in empty space. |

The general rule: a bigger screen shows **more concurrent information**,
never just **bigger whitespace**. Going from 1440 to 2560 should add
columns and detail, not margin.

`breakpoint.sideways-scroll-min` (760px) is the one structural
breakpoint: below it, every layout (panorama, pivot, tile grid) commits
to a single vertical column; at or above it, sideways layouts are live.

## How large screens show more

- A panorama reveals additional sections rather than growing the ones
  already visible.
- A tile grid gains columns and, at the largest step, richer per-tile
  content (e.g. a tile that shows one stat at 1440px can show a stat
  plus a trend line at 2560px).
- A detail view (one record, with secondary panels) can place those
  panels beside the primary content once there's room, instead of
  pushing them below it.
- Multi-column forms and grids (e.g. a list with its own detail form)
  go side by side once there's room for both without cramping either.

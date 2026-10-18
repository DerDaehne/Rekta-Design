# Layout

## Window-size classes

Rekta has two layouts, and the **place** decides which one it uses: a
place is a size container, so a pane in a split decides by its own
width, not the window's.

- **Sideways**: the place is at least `breakpoint.sideways-scroll-min`
  (760px) wide **and** the viewport is taller than
  `breakpoint.short-height` (480px).
- **Compact**: everything else. That covers a phone, a landscape phone,
  a short or narrow window, a pane, and 400% zoom.

When the class changes (a window is resized, a phone turns, a pane
opens), the current section stays current: the panorama re-aligns to
it rather than keeping a pixel offset that now points somewhere else.

## Panorama

A panorama is a single place rendered as a canvas wider than the
viewport, divided into named sections placed side by side. The user
scrolls through it horizontally; a section strip (a pivot of section
links) hints at what's further along and jumps to a section.

- **Sideways:** the panorama fills the height between the app's fixed
  chrome. Sideways motion comes from a horizontal drag, a scrollbar, the
  keyboard (see [navigation.md](navigation.md)), or the vertical mouse
  wheel (below). Sections scroll *vertically within themselves* if their
  own content overflows; the panorama never grows taller than its box.
  Sections are sized in tile units (`--rk-sec-w`), so a wider window
  shows more sections, never wider ones.
- **Compact: vertical only.** Sections stack top to bottom and the page
  scrolls, like any normal mobile page. Sideways motion on a phone fights
  the system's back gesture and reads as unnatural, so it is never used
  for page content there. The section strip stays a horizontal strip and
  sticks under the banner, so the user always sees where they are.
- **Fit first.** When every section fits, nothing scrolls: no trailing
  spacer, no scrollbar, and the strip only changes title sizes. A jump
  aligns a section flush left, or as far as the panorama can scroll.

## Sideways scrolling by mouse wheel

Sideways, a plain vertical wheel gesture over a panorama moves it
*sideways*, so the wheel is free to mean "move along this place." The
unit is the **gesture**, not the single wheel event: wheel events less
than `timing.wheel-latch` (200ms) apart are one gesture, and one gesture
belongs to one scroller. Rules, decided when a gesture starts:

1. **Inner scroll first.** If the pointer is over a part of the page
   that still has room to scroll vertically in the wheel's direction
   (a long section, a list), the gesture belongs to it. When that part
   reaches its end mid-gesture, the rest of the gesture is swallowed
   there; it never turns into a sideways move.
2. **Otherwise the panorama.** The gesture moves the panorama sideways,
   and keeps doing so as sections pass under the pointer, even when one
   of them could scroll vertically. Inner scroll gets the *next*
   gesture.
3. **Horizontal input is native.** A trackpad swipe, a tilt wheel or
   Shift+wheel is left alone and scrolls natively. Only a "pure
   vertical" wheel signal is redirected, so nothing scrolls twice.
4. **The ends fall through.** At either end of the panorama, the wheel
   goes to the page, so a user who keeps scrolling past the end gets
   normal page behaviour, not a dead stop.
5. **Ctrl+wheel zooms**, as everywhere.
6. **Compact never redirects.** The wheel always scrolls vertically.

The same applies to a pivot's title strip, inside the strip's own box.
This is a deliberate, named exception to "the wheel scrolls the page":
it only applies to panoramas and title strips, never to ordinary
vertical content.

## Tile grids

- **Compact:** at least two columns, as many `tile-unit` columns as fit,
  each stretched to fill the row: `repeat(auto-fill, minmax(min(tile-unit,
  (100% - gap) / 2), 1fr))`.
- **Sideways:** fixed `tile-unit` cells; a wider section shows more
  columns, never wider tiles.

## Density

Controls and rows have two heights, picked by the pointer:

| Density | When | Controls and rows |
|---|---|---|
| Comfortable | Any touch screen is present | 44px (`density.comfortable.*`) |
| Compact | A fine pointer (mouse, trackpad) and no touch screen | 32px (`density.compact.*`) |

The user can choose either (a setting with "by pointer", "comfortable"
and "compact"). The app bar stays 56px high (`size.appbar-height`) with
48px controls in both (on a phone it may also hold an app's top-level
places: [navigation.md](navigation.md#places-in-the-bar-compact)); touch
targets on a touch screen never drop below 44px.

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

`breakpoint.sideways-scroll-min` (760px), with `breakpoint.short-height`
(480px), is the one structural breakpoint: it divides the two
window-size classes above.

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

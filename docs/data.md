# Data: charts, meters, progress

Data is quiet until it matters. Rekta draws it in **ink and grey**: no
series palette, no rainbow, no status colour for a line that is not a
status. The one exception is a domain's own colour code (below).

## A domain's colour code

Some domains have colours their users already read as information:
heart-rate zones from cool to warm, the fixed colours the common
training platforms give power and heart rate, a transit network's line
colours. Recolouring them in ink and grey would hide what they say. For
these, and only these, a product sets a code of up to eight colours,
`--rk-series-1` to `--rk-series-8`, per theme (the selectors are those of
the accent, [platforms.md](platforms.md#product-overrides)).

- **Small marks only:** a line, a bar, a zone, a legend key, a tag's
  square (`.rk-tag[data-rk-code="N"]`), a meter's fill
  (`.rk-meter[data-rk-code="N"]`; a `data-status` on it still wins), a
  share of a share bar (`.rk-shares`, below). Never a tile fill, never a
  large area, and **text is never drawn in a code colour**: the label
  beside the mark says what it is.
- **3:1 against the ground and the surface**, in each theme.
- **Apart under colour blindness:** every pair at least 0.09 (OKLab)
  under protanopia and deuteranopia, as the status tiles are; tritanopia
  is reported. Aim for 0.07 there too, as the status colours have
  (0.067–0.069): blue and green codes are the pair tritanopia merges. The
  showcase's light zones were 0.038 apart (zone 1 blue, zone 3 green);
  zone 3 now leans yellow (`#7d8d5e`) and zone 1 a touch greener
  (`#5489b6`), which keeps every pair at 0.09 under all three.
- **Both themes**, the same codes.
- `chart()` draws a series in its code with `{ label, values, code: N }`;
  without a value set, a code falls back to ink.
- `node scripts/build.mjs --check-product product.css` checks the values
  a product sets against these rules; `--check` holds the showcase's own
  example (five heart-rate zones) to them.

A code is not a way round "three series become small multiples": it is
for colours with a meaning the users bring with them, not for telling
arbitrary series apart.

### Shares of a whole

Time per zone, a budget by category: one bar split into its shares
(`.rk-shares`), each child's `--rk-v` its share in any unit (minutes, a
count) and its `data-rk-code` its colour (ink without one). The shares
stand 2px apart like a category's bars, at least 2px wide so a small one
still shows, `space-s` high, at most three tile units wide.

```html
<div class="rk-shares" role="img" aria-label="Time in zones: zone 1 12 min, zone 2 31 min, zone 3 18 min">
  <span data-rk-code="1" style="--rk-v:12"></span><span data-rk-code="2" style="--rk-v:31"></span><span data-rk-code="3" style="--rk-v:18"></span>
</div>
<p class="rk-actions" aria-hidden="true"><span class="rk-tag" data-rk-code="1">z1 · 12 min</span> …</p>
```

- **Words:** the bar is an image whose name says every share; the words
  go beside or under it as tags (`.rk-tag[data-rk-code]`, the square in
  the code, the words in ink), in the bar's order, `aria-hidden` when
  they repeat the name. Never a number inside a share.
- **One bar per whole.** Two wholes to compare are two bars, one under
  the other, at the same width.

## Charts

- **Colours:** the first series in `text-primary`, a second in
  `text-contrast-medium`; the accent marks only the point or bar under
  the pointer. A status colour appears only where a value means that
  status (a threshold crossed), never to tell series apart. Three or
  more series become small multiples, not a third grey. Ink and grey
  are 44.8 (light) and 36.1 (dark) apart in OKLab ×100 under every
  colour-vision deficiency, and both hold 3:1 on the ground.
- **One axis.** Never two y-scales in one chart: two measures of
  different scale are two charts.
- **Marks:** lines 2px with round joins; bars at most 24px wide with
  2px between bars of one category, square ends (Rekta's marks are
  square), growing from one baseline; markers 8px with a 2px ring in the
  ground colour.
- **Grid:** hairlines (1px `text-contrast-low`, solid) at round ticks;
  tick labels compact from 1,000 (1K, 2.5K) in `text-contrast-high`.
  Bars always start at zero; lines may start where the data does.
- **Labels:** a legend for two series and more, never for one (the
  title says what is plotted). Lines carry their name at their end
  when the ends do not collide and the chart is wide enough; bars label
  their peak. Never a number on every point. Text always wears a text
  colour, never the series colour.
- **Reading values:** under the pointer, a crosshair (lines) or the bar
  itself (the others dim) and a tooltip with every value at that point.
  Focused, ← → Home End move along the points and say each value to
  screen readers. Every chart has a table of its numbers ("show the
  numbers"), and a summary as its accessible name.
- **Motion:** lines draw in and bars grow from the baseline once, when
  first drawn (`duration-long`, `duration-moderate`). After that, as the
  data morph in [motion.md](motion.md) says: a redraw on resize is
  instant; new values on the same labels move to their place over
  `duration-long` (lines bend, bars grow or shrink, the axis travels with
  them); new labels redraw and draw in again. A sparkline moves its points
  the same way (`sparkTo()`).

## Sparklines

A sparkline is a trend at a glance: one series, no axis, no labels.

- **On a tile**, in the tile's text colour ([tiles.md](tiles.md)).
- **In a row** (one trend per item of a live list), in the row's
  secondary text colour (`text-contrast-high`), like the meta it stands
  beside; a row that carries a `data-status` draws it in the status
  mark. `rekta.css` sets both and gives it a tile unit's width; drawn in
  the tile sparkline's 200 × 28 box, that is about the row's text height.
- **Empty or all zero.** No values yet: draw nothing and keep the room,
  so the rows stay aligned and the meta says why ("no data yet"). All
  zero, or one constant value: that is a value, so draw it, flat along
  the bottom of the box.
- **Clear of the hairline.** Points are laid out inside the box inset by
  half the stroke (1px), so a flat line is never cut off; the row's
  padding (`space-s`) keeps that line clear of the row's hairline.
- **Stale or offline**, it fades to 35% with the rest of the live region;
  a finished item's row drops it rather than show a still line.

## Meters

A quantity against its whole (`rk-meter`, `role="meter"`): a 6px
hairline-grey track, the fill in ink, or in a domain's code
(`data-rk-code="N"`) when the quantity belongs to one (time in zone 2). When the value means a status
(a disk nearly full, storage full), the fill takes that status's
`full` colour, the label carries the status tag, and the accessible
value says it in words ("81%, nearly full"). The label and the value
sit above the meter. The page sets `--rk-v` (0 to 1); the fill follows
in `duration-moderate`.

## Progress

| Wait | Form |
|---|---|
| a region loading | the 2px line along its first hairline (see [states.md](states.md)) |
| a button's action | the line under the button, its label saying what it does |
| a known amount | `progress.rk-progress` with a value, the label and the percentage above |
| an unknown amount, in place | `progress.rk-progress` without a value: a segment runs along it |
| a wait that blocks everything (signing in, installing, the first load) | the dots (`rk-dots`) |

**The dots, the Metro way:** five small squares fly in from the leading
edge, bunch up slowly through the middle and speed off to the far edge,
one after another (`motion.duration.dots` per flight). They are for a
wait that holds the whole screen; everywhere else, the calm line. Under
reduced motion they stand still in the middle, with the words beside
them saying what is happening.

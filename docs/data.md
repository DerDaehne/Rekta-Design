# Data: charts, meters, progress

Data is quiet until it matters. Rekta draws it in **ink and grey**: no
series palette, no rainbow, no status colour for a line that is not a
status.

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
  first drawn (`duration-long`, `duration-moderate`); a redraw on resize
  or new data is instant.

## Meters

A quantity against its whole (`rk-meter`, `role="meter"`): a 6px
hairline-grey track, the fill in ink. When the value means a status
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

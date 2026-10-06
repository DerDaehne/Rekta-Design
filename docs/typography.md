# Typography

## Three-level title hierarchy

Any single screen uses **up to three** title levels, never more, each
with a distinct job:

1. **Small, secondary info** — a kicker line above everything else
   (a breadcrumb, a status summary, a timestamp). Quiet: small size,
   muted colour (`color.*.text.contrast-high`), lowercase for UI words.
2. **The one bold main title** — what this screen *is* (a record's own
   title, or a place's name). This is the only place weight
   `typography.weight.semibold` (600) appears at a large size — it is
   reserved for exactly one title per screen, so it keeps its meaning as
   "the main thing."
3. **A smaller fact line below the title** — supporting detail (key
   metrics, a short status line), small and in secondary colour, not
   competing with the main title.

A navigation title (the pivot's active entry, see
[navigation.md](navigation.md)) is a different thing from a screen's own
main title: navigation titles are large and **light**
(`typography.weight.light`, 300) — they name a *place*, read calmly,
almost like a section header you scroll past. A screen's own main title
(level 2 above) is large and **bold** — it names *this specific thing*,
and should feel more assertive than the navigation chrome around it.
Where a brand or product name is the active navigation title, that one
word may be semibold while the rest of the title stays light (e.g. one
bold word, one light word) — the one deliberate exception to "navigation
titles are light."

## Scale

All sizes are tokens (`typography.size.*`); do not hand-roll pixel
values. Two kinds of size token exist:

- **Fixed** sizes (`xxs` 12px, `xs` 13px, `s` 15px — the body text size
  — `m` 18px) for body copy, labels, and small UI text that should not
  grow with the viewport.
- **Fluid** sizes, expressed as CSS `clamp()` in the web build, for
  anything that should scale gently with viewport width: `l`
  (22–28px), `xl` (26–34px), `xxl` (30–44px), `group` (22–30px, section
  headings), `title` (30–48px, the one bold main title),
  `pivot-active`/`pivot-inactive` (20–42px) and `nav-active`/
  `nav-inactive` (22–72px) for the two navigation levels described
  above.

A platform without CSS `clamp()` (see [platforms.md](platforms.md))
should interpolate the same min/preferred/max triple against its own
window-width signal — the fluid token's three numbers are the contract,
`clamp()` is just the web's way of expressing it.

## Weight and case

Five weights (`typography.weight.*`): light (300) for navigation titles,
regular (400) for body text, medium (500) for numbers that should read
as data (tabular figures), semibold (600) for the one bold title per
screen and for small UI words/labels, bold (700) available but rarely
used. UI words (labels, nav entries, buttons) are set in lowercase as a
visual signature distinguishing *interface* text from *content* text,
which always keeps its own natural case.

# Rekta Design

Rekta Design is a platform-neutral design language: an information-dense,
motion-guided way to lay out an app, distilled from a prototype built for
one product (kabai studio, "Metro × Porsche v3") into something any
project — web or native — can reuse.

Its name comes from Latin *recta*, the straight line: sharp edges, a
strict grid, nothing rounded off. Its signature layout is the
**panorama**, a canvas
wider than the viewport that the user moves through sideways, with
**live tiles** carrying real information instead of icons, organised
into **pivots** (ordered facets of one place).

## Principles

1. **Content over chrome.** No cards, no boxed panels. Information sits
   directly on the ground, grouped by hairlines and whitespace, not by
   backgrounds and borders.
2. **Motion as guidance.** Every transition means something specific —
   which place you're in, which facet you're looking at, or that
   something just happened — never decoration. See [docs/motion.md](docs/motion.md).
3. **Information-dense live tiles.** A tile's size reflects the
   importance of what it shows, and its fill colour *is* its status.
4. **Panorama and pivot.** A panorama is one place, wider than the
   screen, scrolled through sideways. A pivot is one place's ordered
   facets, with a fixed order and a large, light active title.
5. **Refined precision.** Hairlines, tabular figures, calm large areas,
   vivid small accents. Nothing is decorative; everything earns its
   weight.

Full write-ups: [docs/principles.md](docs/principles.md),
[docs/layout.md](docs/layout.md), [docs/navigation.md](docs/navigation.md),
[docs/typography.md](docs/typography.md), [docs/colour.md](docs/colour.md),
[docs/tiles.md](docs/tiles.md), [docs/motion.md](docs/motion.md),
[docs/accessibility.md](docs/accessibility.md),
[docs/platforms.md](docs/platforms.md).

## Using the tokens

[`tokens/rekta.tokens.json`](tokens/rekta.tokens.json) holds every
value — colour (light/dark, status fills, text), typography, spacing,
radius, motion (durations, easing), breakpoints — in
[W3C Design Tokens (DTCG)](https://tr.designtokens.org/format/) format,
so any tool that reads that format can consume it directly.

For a web project, build plain CSS custom properties from it:

```sh
node scripts/build-css.mjs          # writes dist/rekta.css
node scripts/build-css.mjs --check  # self-check: no deps, no network
```

`dist/rekta.css` defines `:root` custom properties for the light
theme, a `prefers-color-scheme: dark` block plus a `[data-theme="dark"]`
override for the dark theme, and zeroes motion durations under
`prefers-reduced-motion: reduce` or `[data-motion="off"]`.

For a native app (including a future native desktop client), read the
token JSON directly — there is no CSS dependency, see
[docs/platforms.md](docs/platforms.md) for how the same tokens and
behaviours map onto non-web platforms.

## Status

**v0 — documentation and tokens only.** There is no component library
yet. The plan is to extract one later from an application that has
already built the Metro × Porsche components against real product
requirements — building a library ahead of that would mean guessing at
APIs no one has used yet.

## Repo

Local only, no remote. This repo may be made public later; until then,
treat it as a draft.

## Licence

**All rights reserved, licence to be decided.** See [NOTICE](NOTICE) for
the provenance of token values and the typeface, and what is explicitly
*not* included.

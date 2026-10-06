# Platform mapping

Rekta Design's tokens and behaviours are defined platform-neutrally in
`tokens/rekta.tokens.json` and the docs in this folder. This page is
the mapping — for each platform-specific concept the design relies on,
what it means on the web and what it means on a native desktop app. It
is a mapping, not an implementation: no platform's actual code lives
here.

| Concept | Web | Native desktop |
|---|---|---|
| **Units** | CSS pixels (`px`), fluid sizes via `clamp(min, preferred, max)` against viewport width (`vw`). | Density-independent/logical pixels (the platform's own unit — e.g. DIPs), scaled by the OS's display-scale factor. A fluid token's min/preferred/max triple is interpolated against the *window's* current width in the same units, since there is no separate "viewport." |
| **Breakpoints** (`breakpoint.*`) | CSS media queries on viewport width. | Window-size classes: the app reads its own window width and switches layout at the same four numeric thresholds. A maximised window on a small laptop display and a small floating window on a large monitor hit the same thresholds the same way — breakpoints key off window size, never display/monitor size. |
| **Sideways scroll by wheel** ([layout.md](layout.md)) | A `wheel` event with pure vertical delta, captured on the panorama/pivot-strip element, redirected to horizontal scroll, with the fallback/edge rules in [layout.md](layout.md). | The platform's native scroll/pointer event for a mouse wheel, same redirect rule. A trackpad's native horizontal pan gesture is left alone exactly as on the web (rule 2 in [layout.md](layout.md)) — most native toolkits already distinguish wheel-delta from trackpad-pan events, which makes this rule easier to implement natively than it is on the web. |
| **Keyboard** | Standard DOM key events; focus order follows DOM order; `:focus-visible`. | The platform's native focus/key-event system; focus order follows the native tab/traversal order. The *behaviours* in [navigation.md](navigation.md) and [accessibility.md](accessibility.md) (every pointer action also has a keyboard path, visible focus on every fill) apply unchanged — only the event plumbing differs. |
| **Window sizes vs. breakpoints** | N/A — the browser chrome is not part of the design. | A native window can be resized to anything, including below the mobile breakpoint (375px-equivalent) in a floating/tiled state. The design must still degrade the same way a narrow browser window does: stack to a single column, drop sideways scroll. There is no "minimum sane window size" assumption. |
| **Reduced motion** ([motion.md](motion.md)) | CSS `prefers-reduced-motion: reduce` media query, plus an in-app `[data-motion="off"]` override. | The platform's own OS-level reduced-motion setting (each major desktop OS exposes one) read via that platform's native API, plus the same in-app override. Both must map onto the same effect: all `motion.duration.*` tokens become `0`. |
| **Light/dark theme** | CSS `prefers-color-scheme`, plus an in-app `[data-theme]` override. | The platform's native light/dark appearance notification, plus the same in-app override. Both select between the independent `color.light.*` / `color.dark.*` token sets (see [colour.md](colour.md)) — neither platform derives one theme from the other algorithmically. |
| **Fonts** | `@font-face` / web font loading; `typography.font-family.base` lists a CSS-style fallback stack. | The platform's native font loading/embedding; the same family name, with the platform's own fallback-stack mechanism substituted for the CSS list. No specific typeface is mandated by the tokens — see [NOTICE](../NOTICE) regarding Barlow specifically. |
| **Focus rings on fills** ([accessibility.md](accessibility.md)) | `outline`/`box-shadow` drawn in `currentColor`, inset. | The platform's native focus-indicator drawing, with the same rule: draw in the control's current text colour, inset, rather than a single fixed accent colour, so it survives on every status tile fill. |

## What stays identical across platforms

Everything in `tokens/rekta.tokens.json` that isn't a unit-of-measure
or an OS integration point above is platform-neutral by construction:
colour values, the type scale's min/preferred/max triples, spacing and
radius numbers, motion durations and easing curves (a cubic-bezier is
just four numbers; any platform's animation system can consume it), and
the panorama/pivot/tile/app-bar *behaviours* described in
[layout.md](layout.md), [navigation.md](navigation.md) and
[tiles.md](tiles.md). A native app adopting Rekta Design should never need
to invent a different colour or a different motion curve — only a
different way of feeding the same numbers into its own rendering and
event system.

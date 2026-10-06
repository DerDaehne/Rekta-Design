# Motion

Every transition in Rekta Design has one specific meaning. The table below
is the full system — if a new transition is needed, it should map onto
one of these meanings or extend the table with a new, equally specific
one; it should never be "add some animation because it feels nice."

| Transition | Duration · easing | Meaning |
|---|---|---|
| **Place change** | `motion.duration.moderate` (400ms) · `motion.easing.out`, rotated `motion.turnstile-angle` (40°) around the leading edge, with a fade | "You are now at a different place." Reserved for real navigation between top-level places, never for a facet change within one place. |
| **Facet change (pivot)** | 0.75 × `motion.duration.moderate` (300ms) · `motion.easing.out`, travels `motion.slide-distance` (24px) from the trailing edge, with a fade | "Same place, different facet." The direction of travel matches the facet's position in the pivot's fixed order. |
| **Panorama scroll** | native smooth scroll; an active section indicator transitions over `motion.duration.short` (250ms) | "Same place, you're looking sideways." Not a place change and not a facet change — nothing about the current facet or place has changed, only the scroll position. |
| **Tiles building in** | `motion.duration.moderate` per tile, staggered ~30ms × index (capped around 10 tiles) · `motion.easing.in`, 6px rise | "This view is assembling itself from live data right now" — a first paint should not look like a static printed page. |
| **Reordering (FLIP)** | `motion.duration.moderate` (400ms), computed per-element (first/last/invert/play) | "Something just moved because of what you did" — an item leaving, arriving, or changing group should be *seen* moving, not just appear/disappear, so cause and effect stay visible. |
| **Sheets (dialogs, command entry)** | `motion.duration.short` (250ms) · `motion.easing.in`, 16px from the top edge, with a shaded + blurred backdrop | "A short question, from above; everything behind it stays exactly where it was." Sheets enter from the top edge, never scale in from the center. |
| **Toast** | `motion.duration.short` (250ms), 12px from the top | "Something happened"; a toast with an undo action shows a countdown rather than just vanishing, so the user can see their window closing. |
| **Status banner** | `motion.duration.short` (250ms), fade + 8px | A persistent state change (e.g. "the system is halted") announces itself once via this fade-in, then stays static — its *colour* (not continued motion) carries which kind of halt it is. |
| **Live indicators** | lines draw in over `motion.duration.long` (600ms); bars grow over the same; a live pulse/blink cycles roughly every 1.2–1.6s | "This is updating right now." Reserved for genuinely live data — a finished/static item's charts render immediately, with no draw-in and no pulse, so stillness itself signals "done." |
| **State changes** (hover, border, colour, underline) | `motion.duration.short` (250ms) · `motion.easing.base` | The calm default for anything reacting to pointer/focus: nothing snaps, everything glides briefly. |

## Reduced motion

`prefers-reduced-motion: reduce` (or an explicit in-app motion-off
setting) sets all three duration tokens
(`motion.duration.short/moderate/long`) to `0s`. This must make every
transition in the table above instant, never skipped — the *end state*
each transition reaches (new place, new facet, reordered item, open
sheet) must still be reachable and still happen; only the perceptible
motion in between disappears. A build that hides content under reduced
motion rather than snapping to it instantly has a bug, not a feature.

Live indicators (the one category above that isn't a one-shot
transition) also stop under reduced motion — no pulsing, no drawing
animation — but the live *values* themselves must keep updating; reduced
motion removes animated presentation, not functionality.

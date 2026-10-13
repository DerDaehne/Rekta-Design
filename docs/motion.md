# Motion

Every transition in Rekta Design has one specific meaning. The table below
is the full system — if a new transition is needed, it should map onto
one of these meanings or extend the table with a new, equally specific
one; it should never be "add some animation because it feels nice."

| Transition | Duration · easing | Meaning |
|---|---|---|
| **Press** | `motion.duration.press` (100ms) in, `motion.duration.short` (250ms) out · tilt up to `motion.transform.tile-tilt` (8°) towards the pressed point, scale `tile-press-scale` | "This tile heard you." Only on tiles that open something; a quick tap still shows the whole sink. |
| **Place change** | A page turned in a book. Leaving: each part of the place swings 60° toward the viewer around the leading edge of the box that clips it, gathering speed (cubic-bezier(0.35, 0, 0.65, 0.6)), whole for the first 40% of its time, then fading; the pressed tile last. The next place starts when the last part is 75% through. Arriving: each part swings in from 70° on the other side, quick at first, and lands with a trace of speed left (cubic-bezier(0.22, 0.9, 0.6, 0.98)), whole after its first third; nothing builds in after it. Each part's heft is 0.8 × (its area / a 150 × 150 tile)^0.35, held between 0.5 and 1.4 (a line of text about 0.6, a tile 0.8, a wide tile 1.0, a table 1.4). Heft sets both when a part starts, (heft − 0.5) × 6 × `duration-stagger` after the turn begins, and how long it takes, `motion.duration.short` (leaving) or `motion.duration.moderate` (arriving) × heft. A part is a tile, a control, a line of text, a row, or anything that shows something of its own; a plain layout box or a scroller lets its children turn one by one | "You are now at a different place." Light parts go first and quickest, heavy ones later and slower: the inertia everyone knows. A whole change takes about 0.8s. A part is let go in the frame it lands, already on the pixels it will rest on, so nothing shifts, sharpens or settles afterwards. Reserved for real navigation between places (a history entry, or the pivot that picks the place), never for a facet or section change. Back, or a title earlier in the order, runs it the other way round. |
| **Facet change (pivot)** | 0.75 × `motion.duration.moderate` (300ms) · `motion.easing.out`, travels `motion.slide-distance` (24px) from the trailing edge, with a fade | "Same place, different facet." The direction of travel matches the facet's position in the pivot's fixed order. A pivot that picks the place (the top-level strip or list) turns it instead. |
| **Title picked down the side** | `motion.duration.pivot-size` · ease-in-out: the title in sight steps in by `space-m` and darkens; the one before steps back | "This one." The step says which was picked; the size stays, so the list never reflows. |
| **Panorama scroll** | native smooth scroll; an active section indicator transitions over `motion.duration.short` (250ms) | "Same place, you're looking sideways." Not a place change and not a facet change — nothing about the current facet or place has changed, only the scroll position. |
| **Tiles building in** | `motion.duration.moderate` per tile, staggered ~30ms × index (capped around 10 tiles) · `motion.easing.in`, 6px rise | "This view is assembling itself from live data right now" — a first paint should not look like a static printed page. Only on arrival (first paint, or a place a tile opened): a re-render of live data never replays it. |
| **Reordering (FLIP)** | `motion.duration.moderate` (400ms) · `motion.easing.base`, computed per-element (first/last/invert/play); dragged and keyboard-moved items alike | "Something just moved because of what you did" — an item leaving, arriving, or changing group should be *seen* moving, not just appear/disappear, so cause and effect stay visible. |
| **Resize and re-flow** | `motion.duration.moderate` (400ms) · `motion.easing.base`, FLIP: the resized box scales from its old size while its content fades back in over the last 45%; the boxes around it glide to their new places | "This grew (or shrank), and the rest made room." A tile changing size, a grid re-flowing around it. Content is never seen stretched. |
| **Arriving, leaving** | Arriving: a row `motion.duration.short`, 6px from above with a fade; a tile `motion.duration.moderate`, 6px rise, from 96%. Leaving: 0.6 × `motion.duration.short` · `motion.easing.out`, a row drifts 12px towards its trailing edge, a tile shrinks to 90%, with a fade. Then the rest close the gap (reorder) | "This one is new; that one is gone." Filtering, archiving and undoing it, a live event, a pinned tile, a file dropped in. More than 60 changes at a time apply at once: motion is for a few things moving, not a whole list replaced. |
| **Live tile news** | `motion.duration.moderate`. Peek: the old content leaves upwards (0.6×) while the new one rises from below (0.8×, after 0.2×). Flip: the tile turns away around its horizontal axis (half), changes, turns back (half), perspective `turnstile-perspective` | Peek: "a new number or line"; flip: "a new status." A continuous reading (frames a second, requests a second) is no news: it changes in place. Inside a live region the change waits while the region is held. |
| **Data morph** | `motion.duration.long` (600ms) · `motion.easing.base`; the axis travels from the old extent to the new | "The same thing, measured again": new figures on the same labels bend the line or grow the bars to them. New labels redraw and draw in again. A sparkline moves its points the same way. |
| **Mode change (app bar)** | `motion.duration.short` · `motion.easing.in`, 12px rise with a fade | "The bar now acts on your selection" — and back. Its commands change, the bar stays. |
| **Checkmark** | `motion.duration.short`, after `motion.duration.press`, drawn short stroke first | "Selected." After the nudge, so the press reads first. Not on virtual rows, which are drawn again as they scroll. |
| **Segments** | `motion.duration.short` · `motion.easing.base`: the accent block slides from the old choice to the new | "The choice moved here." The chosen label waits for the block; its text turns light halfway. |
| **Tree branch** | open `motion.duration.short` · `motion.easing.in` to its height, with a fade; close 0.7 × that · `motion.easing.out` | "These belong to it." |
| **Whole-look change** (theme, accent) | `motion.duration.short` · `motion.easing.base` cross-fade (a view transition, where the browser has them) | "Same page, new light." Nothing moves. |
| **Sheets (dialogs, command entry)** | `motion.duration.short` (250ms) · `motion.easing.in`, 16px from the top edge, with a shaded + blurred backdrop; leaving 0.8 × that · `motion.easing.out`, back up 16px | "A short question, from above; everything behind it stays exactly where it was." Sheets enter from the top edge, never scale in from the center, and leave the way they came. |
| **Toast** | `motion.duration.short` (250ms), 12px from the top | "Something happened"; a toast with an undo action shows a countdown rather than just vanishing, so the user can see their window closing. |
| **A message in place** (error summary, a refusal under a cell, waiting updates) | `motion.duration.short` (a cell's refusal `motion.duration.float`), fade + 8px from above | "This needs you, here." Once on arrival; it does not move again. |
| **Status banner** | `motion.duration.short` (250ms), fade + 8px | A persistent state change (e.g. "the system is halted") announces itself once via this fade-in, then stays static — its *colour* (not continued motion) carries which kind of halt it is. |
| **Live indicators** | lines draw in over `motion.duration.long` (600ms); bars grow over the same; a live pulse/blink cycles roughly every 1.2–1.6s | "This is updating right now." Reserved for genuinely live data — a finished/static item's charts render immediately, with no draw-in and no pulse, so stillness itself signals "done." |
| **Loading, pending** | a 2px line passes over `motion.duration.busy` (1200ms), linear, after `timing.busy-delay`; stands still at full width under reduced motion | "Working on it." Never a shimmer or a pulse on placeholders: they stand still. |
| **Floating surface** | `motion.duration.float` (150ms) · `motion.easing.in`, fade + 4px away from its control; leaves at once | "Options for this control, right here." |
| **State changes** (hover, border, colour, underline) | `motion.duration.short` (250ms) · `motion.easing.base` | The calm default for anything reacting to pointer/focus: nothing snaps, everything glides briefly. A column put back to its width, a value going stale, a progress bar's value glide the same way (progress over `motion.duration.moderate`). |

## Reduced motion

`prefers-reduced-motion: reduce` (or an explicit in-app motion-off
setting) sets every `motion.duration.*` token to `0s`. The `timing.*`
tokens stay: the toast's undo window is wall time the user needs, not
motion. A press under reduced motion only shrinks; it does not tilt. This must make every
transition in the table above instant, never skipped — the *end state*
each transition reaches (new place, new facet, reordered item, open
sheet) must still be reachable and still happen; only the perceptible
motion in between disappears. A build that hides content under reduced
motion rather than snapping to it instantly has a bug, not a feature.

Live indicators (the one category above that isn't a one-shot
transition) also stop under reduced motion — no pulsing, no drawing
animation — but the live *values* themselves must keep updating; reduced
motion removes animated presentation, not functionality.

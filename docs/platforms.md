# Platforms

Rekta Design is defined once, platform-neutrally: the tokens in
`tokens/rekta.tokens.json`, the rules in this folder, and the behaviour
contract in [behaviour.md](behaviour.md). The web build (`rekta.css`,
`rekta.js`) is one implementation of it. Any other party can implement
Rekta for another toolkit (WinUI, WPF, Qt, GTK, SwiftUI, Compose,
Flutter, a game UI, a terminal) from the same sources without reading a
line of CSS. This page is the guide for doing that.

## What to read

| File | What it is | Who reads it |
|---|---|---|
| `tokens/rekta.tokens.json` | The source, in [DTCG](https://tr.designtokens.org/format/) format, with aliases and a `$description` on every token. | Token tools (Style Dictionary, Tokens Studio, Figma plugins), and anyone generating code. |
| `dist/rekta.tokens.json` | The same values **resolved** for a platform: no aliases, plain numbers in logical px and ms, colours as `#rrggbb` or `#rrggbbaa`, easings as four numbers, grouped by theme and density. | Native implementations. Start here. |
| `dist/rekta.tokens.css` | The resolved values as `--rk-` custom properties, with theme, density and reduced-motion switching. | Web projects. |
| `docs/*.md` | Rules: what each part means, how it behaves, what it must never do. | Everyone. |

`npm run build` (or `node scripts/build.mjs`) writes `dist/`. The build
has no dependencies.

## Token names

One name per token, the same on every platform. The web prefixes it with
`--rk-`; a native implementation uses it as a key, an enum case or a
resource name in its own casing (`space-m` → `SpaceM`, `space_m`,
`spaceM`).

| Source path | Name | Example |
|---|---|---|
| `color.<theme>.<a>.<b>` | `a-b` | `color.light.status.error.tile` → `status-error-tile` |
| `typography.font-family.<x>` | `font-<x>` | `font-sans` |
| `typography.weight.<x>` | `font-weight-<x>` | `font-weight-semibold` |
| `typography.size.<x>` | `font-size-<x>` | `font-size-title` |
| `typography.line-height.<x>` | `line-height-<x>` | `line-height-compact` |
| `spacing.<x>` | `space-<x>` | `space-m` |
| `radius.<x>` | `radius-<x>` | `radius-tile` |
| `size.<x>` | `<x>` | `tile-unit`, `appbar-height`, `touch-target` |
| `density.<mode>.<x>` | `<x>` | `control-height` |
| `motion.duration.<x>` | `duration-<x>` | `duration-moderate` |
| `motion.easing.<x>` | `ease-<x>` | `ease-pivot-glide` |
| `motion.transform.<x>` | `<x>` | `turnstile-angle`, `tile-tilt` |
| `timing.<x>` | `duration-<x>` | `duration-toast` |
| `layer.<x>` | `layer-<x>` | `layer-toast` |
| `breakpoint.<x>` | `bp-<x>` | `bp-sideways-scroll-min` |

## Values and units

| Kind | In `dist/rekta.tokens.json` | On a native platform |
|---|---|---|
| Length | A number of logical px. | DIPs, dp, pt: the platform's density-independent unit, scaled by the OS display scale. The web build turns type, control and tile sizes into `rem` so they follow the user's text size; a native build should scale the same tokens with the OS text-size setting (Dynamic Type, Android font scale, Windows text scaling). |
| Fluid length | `{ min, max, viewport }` | `clamp(min, viewport × width, max)`, where `width` is the width of the box that hosts the text: the place (pane), not the screen. Recompute when that box resizes. |
| Colour | `#rrggbb`, or `#rrggbbaa` for the translucent ones (`hover`, `glass`, `shading`). | A colour in the platform's sRGB type. |
| Duration | Milliseconds. | The platform's duration type. |
| Easing | `[x1, y1, x2, y2]`, a CSS cubic-bezier. | Every animation system takes a cubic Bézier: `KeySpline` (XAML), `QEasingCurve::BezierSpline` (Qt), `Animation.timingCurve` (SwiftUI), `CubicBezierEasing` (Compose), `Cubic` (Flutter). |
| Angle | Degrees. | Degrees or radians as the platform wants. |
| Layer | An integer order. | Only the order matters: sticky strip < app bar < banner < toast; sheets and modals sit above all of them in the platform's own modal layer. |

## Switching

Four inputs pick values. Each has a system source and an in-app
override; the override wins.

| Input | System source | Override | Effect |
|---|---|---|---|
| Theme | The OS light/dark appearance. | `system`, `light`, `dark` | Picks `themes.light` or `themes.dark`. Dark is designed separately, never derived. |
| Density | The pointer: a fine pointer with no touch screen is compact; anything with touch is comfortable. | `auto`, `comfortable`, `compact` | Picks `densities.*`: 32 or 44 px controls and rows. The app bar stays 56 px high with 48 px controls in both. |
| Motion | The OS reduced-motion setting. | `system`, `on`, `off` | Reduced: every `motion.duration.*` becomes 0. `timing.*` stays: the undo window is wall time, not motion. |
| Window-size class | The size of the place (pane) and of the window. | none | See below. |

Every transition must still reach its end state with zero duration:
instant, never skipped.

## Window-size classes

Two classes, decided by the **place** (the pane that hosts the content),
not the screen or the device:

- **Sideways**: the place is at least `bp-sideways-scroll-min` (760)
  wide **and** the window is taller than `bp-short-height` (480).
  Panoramas move sideways, each section scrolls inside itself, tiles sit
  in fixed `tile-unit` cells, toasts hang from the top centre, and the
  app bar has no back button (every place below the top has a crumb).
- **Compact**: everything else, including a phone, a landscape phone, a
  short or narrow window, a pane in a split, and 400% zoom. Everything
  stacks and the window scrolls; tiles take at least two columns; the
  section strip sticks under the banner; toasts rise from the app bar;
  back is the leading app-bar item.

When the class changes, the current section stays current: re-align to
it, never to a pixel offset. A native window can be resized to any size;
there is no minimum window size.

## Behaviours to rebuild

The web's `rekta.js` is the reference implementation of each behaviour;
[behaviour.md](behaviour.md) states the contract. A native
implementation needs the same:

- **Press**: a tile tilts back towards the point pressed, by up to
  `tile-tilt`, and scales to `tile-press-scale` over `duration-press`; it
  comes back over `duration-short`. A tap shows the whole press. On
  touch, the press waits `duration-tap-delay` and cancels on 8 px of
  movement. A drag or a text selection never activates.
- **Place change (turnstile)**: units of the leaving place turn
  `turnstile-angle` away around the leading edge of the box that clips
  them, a row at a time, `duration-stagger` apart, the pressed tile last.
  The new place turns in over `duration-moderate`. Back runs it the
  other way. A navigation during a turn finishes that turn first; it is
  never refused.
- **History**: one entry per place; facets and section jumps add none.
  Back, Escape, the crumb and the OS back command (Android back, the
  mouse back button, Alt+Left, a window-title back button) all go back
  one place. Map the shell's back command to the implementation's back.
- **Panorama wheel**: one wheel gesture (events less than
  `duration-wheel-latch` apart) belongs to one scroller. Most toolkits
  tell a wheel from a trackpad pan; leave the pan alone.
- **Keys**: Escape closes the innermost thing first (a menu, a toast
  with focus, a selection, then the place); Ctrl/Cmd+Z undoes the open
  toast's action except in a text field.
- **Toast and banner**: one toast at a time, the undo window held while
  hovered or focused; one banner at a time, the most severe state first,
  its end reported as a toast.

## What stays identical

Colour values, the type scale, spacing, radius, durations, easing
curves, angles, the status vocabulary and the behaviours. A native
implementation never invents a colour or a curve; it only feeds the same
numbers into its own rendering and event system. If something is
missing for a platform, add a token to the source, not a constant to the
implementation.

## Icons and the window

- **Icons:** `dist/rekta.icons.json` holds each icon's shapes (`path`
  data and `circle`s on a 20 × 20 grid) with the stroke width (1.6),
  caps (`butt`) and joins (`miter`). Draw them with the toolkit's vector
  API in the text colour; a shape with `fill` is solid. Scale them with
  the text size like any other logical-pixel size.
- **The title bar:** a desktop app that draws its own title bar (no
  system frame) follows `rk-titlebar`: the window's drag area on the
  ground with a hairline below, back when the current place has a
  parent, the app's name, the place's name in light weight, and room at
  the end for the system's window buttons. A web view inside a native
  shell gets the same by setting `data-rk-titlebar` on the root.
- **Installed web apps:** see [shell.md](shell.md): the window-controls
  overlay, offline, and updates that wait to be asked.

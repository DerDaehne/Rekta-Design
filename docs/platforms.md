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
| `dist/rekta.tokens.json` | The same values **resolved** for a platform: no aliases, plain numbers in logical px and ms, colours as `#rrggbb` or `#rrggbbaa`, easings as four numbers, grouped by theme (`light`, `dark`, and `light-contrast`, `dark-contrast` for more contrast, each complete) and density. | Native implementations. Start here. |
| `dist/rekta.tokens.css` | The resolved values as `--rk-` custom properties, with theme, density and reduced-motion switching. | Web projects. |
| `tokens/rekta.accents.json` | The named accents: an accent and the text on it per theme and contrast set, checked ([colour.md](colour.md#named-accents)). Resolved as `accents` in `dist/rekta.tokens.json`. | Anyone offering an accent by name. |
| `dist/rekta.color.js` | The colour rules as a module without dependencies: contrast, OKLab distance and colour blindness, apart from status, an accent's check and derivation. | A product that checks or derives colours at run time; the reference for porting them. |
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
| `color.<theme>.<a>.<b>` | `a-b` | `color.light.status.error.tile` → `status-error-tile`; `color.light-contrast.*` holds the same names, for more contrast |
| `typography.font-family.<x>` | `font-<x>` | `font-sans` |
| `typography.weight.<x>` | `font-weight-<x>` | `font-weight-semibold` |
| `typography.size.<x>` | `font-size-<x>` | `font-size-title` |
| `typography.line-height.<x>` | `line-height-<x>` | `line-height-compact` |
| `typography.metrics.<x>` | `font-<x>` | `font-descent` |
| `spacing.<x>` | `space-<x>` | `space-m` |
| `radius.<x>` | `radius-<x>` | `radius-tile` |
| `size.<x>` | `<x>` | `tile-unit`, `appbar-height`, `touch-target` |
| `density.<mode>.<x>` | `<x>` | `control-height` |
| `motion.duration.<x>` | `duration-<x>` | `duration-moderate` |
| `motion.easing.<x>` | `ease-<x>` | `ease-pivot-glide` |
| `motion.transform.<x>` | `<x>` | `turnstile-out-angle`, `tile-tilt` |
| `motion.proportion.<x>` | `<x>` | `turn-out-whole` |
| `motion.<group>.<x>` (any other group) | `<group>-<x>` | `heft-min` |
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
| Share | A plain number, mostly 0 to 1: of the movement's own time (`turn-out-whole`), or of the length its description names (`peek-travel`, of the content's height). | The same number. |
| Layer | An integer order. | Only the order matters: sticky strip < app bar < banner < toast; sheets and modals sit above all of them in the platform's own modal layer. |

## Switching

Six inputs pick values. Each has a system source and an in-app
override; the override wins. A seventh, the accent, has no system
source (below).

| Input | System source | Override | Effect |
|---|---|---|---|
| Theme | The OS light/dark appearance. | `system`, `light`, `dark` | Picks `themes.light` or `themes.dark`. Dark is designed separately, never derived. |
| Density | The pointer: a fine pointer with no touch screen is compact; anything with touch is comfortable. | `auto`, `comfortable`, `compact` | Picks `densities.*`: 32 or 44 px controls and rows. The app bar stays 56 px high with 48 px controls in both. |
| Motion | The OS reduced-motion setting. | `system`, `on`, `off` | Reduced: every `motion.duration.*` becomes 0. `timing.*` stays: the undo window is wall time, not motion. |
| Contrast | The OS "increase contrast" setting (`NSWorkspace.accessibilityDisplayShouldIncreaseContrast` on macOS, `UIAccessibility.isDarkerSystemColorsEnabled` on iOS, GNOME's `high-contrast`). Not Windows' contrast themes: those are forced colours, the system's own. | `system`, `more`, `normal` | More: picks `themes.light-contrast` or `themes.dark-contrast` (the theme with its contrast set over it): every text pair at 7:1, opaque glass. Each status tile gets a 2px frame in its `status-*-full` colour. |
| Transparency | The OS "reduce transparency" setting (`accessibilityDisplayShouldReduceTransparency` on macOS, `isReduceTransparencyEnabled` on iOS). | `system`, `reduce`, `normal` | Reduce: `glass` takes the value of `bg`, and nothing blurs behind it or behind a sheet's `shading`. |
| Window-size class | The size of the place (pane) and of the window. | none | See below. |
| Accent | None: the product's own accent, ink when it sets none. | `product`, or a named accent | Picks `accents.<name>` for the theme and contrast set in use. |

Every transition must still reach its end state with zero duration:
instant, never skipped.

A native implementation honours the system's contrast and transparency
settings where the platform has them, and follows a change while it
runs. rekta-core takes the contrast setting from the app,
`rk_app_set_contrast(bool)`: no platform layer of it reads the system's
yet, and it draws no glass.

## Product overrides

Some values are a product's decision, not the design's. They are the
official way to make Rekta Design a product's own without forking it: on
the web, override the `--rk-*` property after the Rekta stylesheets; in
rekta-core, call the setter. A value rekta-core keeps the design's
(motion, colours, the type scale) is refused there with a log line,
not set.

| Token | Property | What a product decides | rekta-core |
|---|---|---|---|
| `color.*.accent`, `on-accent` | `--rk-accent`, `--rk-on-accent` | Its one hue: primary button, checked controls, a selected item, one accent surface per place. Per theme and contrast set; or a named accent (`data-accent`, `prefs` `accent`). Checked with `build.mjs --check-product`; `build.mjs --accent COLOUR` derives the pairs from one colour. | `rk_app_set_accent(light, dark)`, held to the same rules (said through the log); `rk_app_set_accent_named(name)` |
| `timing.toast` | `--rk-duration-toast` | How long a toast stays, and so the undo window (say 10s instead of 5s). Read when a toast opens. | `rk_app_set_timing(RK_TIMING_TOAST, ms)` |
| `timing.stale` | `--rk-duration-stale` | When live data without an update counts as stale. A live region's own `stale` option wins. | `rk_app_set_timing(RK_TIMING_STALE, ms)`; `rk_duration_of` reads it |
| `size.tile-unit`, `size.tile-unit-ultrawide` | `--rk-tile-unit`, `--rk-tile-unit-ultrawide` | The tile grid's cell, below and from 2560px. Set both: from 2560px the stylesheet sets the first from the second. | `rk_app_set_metric(RK_TILE_UNIT, px)` (the ultrawide cell stays the token's) |
| `size.pivot-wide` | `--rk-pivot-wide` | From what width a pivot shows its facets side by side; 0 never. May differ per pivot. | `rk_app_set_metric(RK_PIVOT_WIDE, px)`; per pivot `rk_pivot_set_wide` |
| a domain colour code (no token: a product slot) | `--rk-series-1` to `-8` | Colours its users already read, such as heart-rate zones, for small data marks; per theme, as the accent. Checked with `build.mjs --check-product`. | `rk_app_set_series(code, light, dark)`, held to the same rules (said through the log); `rk_tile_set_code`, `rk_row_set_code` |
| density | `data-density` on the root | Not a token to override but a setting for the user: `Rekta.prefs` writes it (`auto`, `comfortable`, `compact`). | `rk_app_set_density` |
| `typography.font-family.sans` | `--rk-font-sans` | Its typeface, under the conditions below ([typography.md](typography.md#the-family)). | not yet: rekta-core draws Barlow, built in; `rk_app_add_font` adds a face only for the characters Barlow lacks |

A length may be written in any form, `px`, `rem` or a `calc()` of
other tokens: `rekta.js` resolves it as the browser lays it out.

The accent is set per theme, with the selectors `rekta.tokens.css`
uses, so the dark value also wins over the system's dark mode and the
in-app theme:

```css
/* after rekta.tokens.css and rekta.css */
:root { --rk-accent: #7a2a8c; --rk-on-accent: #ffffff; --rk-duration-toast: 10000ms; }
@media (prefers-color-scheme: dark) {
  :root:not([data-theme="light"]) { --rk-accent: #d39be0; --rk-on-accent: #2a0f31; }
}
:root[data-theme="dark"] { --rk-accent: #d39be0; --rk-on-accent: #2a0f31; }
```

An accent must hold 3:1 against `bg` and `surface` in its theme,
`on-accent` 4.5:1 on it (7:1 under more contrast, in the contrast
selectors), and stay apart from status: 0.09 (OKLab) from every status
colour, 0.05 under protanopia and deuteranopia, its hue 20° from each
status hue unless it is nearly grey ([colour.md](colour.md#apart-from-status)).
The showcase measures a candidate live and says why it refuses one.

rekta-core has the named accents and checks a product's accent, but it
does not derive one from a colour yet (derive it with `build.mjs
--accent` and pass the pairs), and it has no accent surface: a view
filled with `RK_ROLE_ACCENT` keeps its text in ink, so draw `on-accent`
text on it yourself.

A family of its own is a product's too, under four conditions: it has
the four weights the roles use (300, 400, 500, 600), tabular figures,
a licence that allows shipping it with the app, and it ships with the
app (its files beside `rekta.fonts.css`, never from a font CDN). The
type scale stays the design's; the text budgets that depend on the
family's width are measured again ([typography.md](typography.md#the-family)).

```css
/* after rekta.tokens.css and rekta.css; the @font-face rules for its four weights in the product's own CSS */
:root { --rk-font-sans: 'Atkinson Hyperlegible Next', 'Arial', sans-serif; }
```

Everything else is the design, not a setting. In particular, do not
override:

- **The status colours, text, `bg`, `surface` and `focus`.** Their AA
  contrast and their separation under colour blindness are measured
  together by `--check`; one changed value can break a pair it cannot see.
- **`motion.duration.*` and the easings.** Each motion has one meaning
  ([motion.md](motion.md)), and under reduced motion `rekta.tokens.css`
  sets every duration to 0 on `:root`: an override on `:root` after it
  wins over that too and brings motion back to people who asked for none.
  (The `timing.*` values above are wall time and stay under reduced
  motion; that is why they may change.)
- **Spacing, radius and the type scale.** The tile grid, the hairlines and
  the title hierarchy are built from them. (The family is a product's;
  its sizes are not.)

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
- **Place change (turnstile)**: the parts of the leaving place turn
  `turnstile-out-angle` away around the leading edge of the box that
  clips them (`ease-turn-out`), each by its weight: light parts first and
  quickest, heavy ones later and slower, (heft − `heft-min`) ×
  `heft-spread` × `duration-stagger` after the turn begins, over
  `duration-short` × heft; the pressed tile last, once its press has
  sunk in. The new place turns in from `turnstile-in-angle`
  (`ease-turn-in`) over `duration-moderate` × heft, once the last
  leaving part is `turn-overlap` through. [motion.md](motion.md#place-change-in-detail)
  gives heft, rows and the pressed tile. Back runs it the other way. A navigation during a turn finishes that turn first; it is
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
  toast's action except in a text field. A grid of tiles that open is
  one tab stop with arrows by position, and in every collection whose
  entries take focus letters jump, one prefix while typed within
  `timing.type-ahead` ([navigation.md](navigation.md#keyboard)).
  rekta-core has the letters in its list, table and tree; its tiles are
  still one Tab stop each.
- **Toast and banner**: one toast at a time, the undo window held while
  hovered or focused; one banner at a time, the most severe state first,
  its end reported as a toast.
- **Desktop patterns**: a place called up (a launcher) turns at half the
  pace; a month to look at; the display step; an icon per menu item.
  rekta-core has these (`rk_controller_set_called`, `rk_month`,
  `RK_TEXT_DISPLAY`, `rk_menu_set_icon`). It does not have yet: an app
  bar (so no centred control, indicator or giving way to full screen),
  a notification's title, picture and pressable text in its toast
  (`rk_window_toast` is a message and one action), a menu's second
  level, the preview tile and the recent stack, and the session bar. An
  app on rekta-core builds those from its views by these pages
  ([navigation.md](navigation.md), [states.md](states.md),
  [tiles.md](tiles.md), [shell.md](shell.md)).

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
- **Window buttons:** minimise, maximise (restore while maximised) and
  close, quiet, close never red, the name stepping back while the window
  is not the active one ([shell.md](shell.md#window-buttons)). rekta-core
  draws them so on Windows, X11 and Wayland; macOS keeps its own.
- **Picture icons** (an app's icon, a favicon, a logo): a single-colour
  variant in the text colour where the source has one, else the picture
  at the fixed sizes, only on a neutral ground
  ([shell.md](shell.md#picture-icons)).

## What rekta-core does not draw yet

Some patterns are the web's alone for now. Each is in the docs as the
design; rekta-core follows when an app needs it, and until then an app
builds it from views or does without:

- **The app bar** (with the live state, the places and running things):
  rekta-core has none; a native app uses its window's own chrome.
- **A side panel and choosing in place:** rekta-core's sheets come from
  the top.
- **A launch tile:** rekta-core's tiles carry a stat, a message, a
  sparkline and progress, not an icon and a count; a tile spans any
  number of units (`rk_view_set_span`).
- **Dragging a row away:** rekta-core's lists take Delete
  (`rk_list_on_remove`), not the drag; its declared lists (`RK_LIST`)
  do not offer the removal yet.
- **A family of the product's:** rekta-core draws Barlow, built in.
- **The inactive title on macOS:** the system draws that title bar's
  buttons; the name does not step back there yet.
- **Other programs' windows** ([motion.md](motion.md#other-programs-windows)):
  neither the web build nor rekta-core shows them; the tokens are there
  for a surface that does.

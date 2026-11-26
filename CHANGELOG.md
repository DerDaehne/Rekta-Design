# Changelog

Rekta Design follows [semantic versioning](https://semver.org/). Classes,
tokens, public hooks, `data-rk-*` attributes, events and the `rekta.js`
API are the public surface. Before 1.0, a minor version may still break
it; each entry says how.

## Unreleased

Rekta is first the design of a desktop shell and its apps; the web is a
profile of it, for installable apps (PWAs). That changes no rule; the
README, principles.md and platforms.md say so, and platforms.md gains a
glossary. Two motion rules are settled: the end of a scroll, and the
dots' fade in.

### Changed

- **Focus when the last entry goes** (collections.md, live.md,
  behaviour.md): when no entry of a removable list is left, focus moves
  to the control before the list in the focus order, else to the
  place's focus fallback. `removable()` does it for Delete, the row's
  button and a drag, and gains `focusBefore()` for a control that
  empties the list and hides with it ("dismiss all"). rekta-core does
  not yet (platforms.md).
- **Warm neutrals** (colour.md): the seven neutral tokens of each theme
  carry one warm cast, a yellow-red hue at a chroma of at most 0.021,
  below the 0.05 under which Rekta counts a colour as grey. Light:
  `bg` `#faf8f4`, `surface` `#efeae1`, `surface-2` `#e5dfd3`,
  `text-primary` `#1f1a14`, `text-contrast-high` `#554e45`,
  `-medium` `#6a6258`, `-low` `#d8d0c3`. Dark: `#1b1916`,
  `#2a2723`, `#34302b`, `#f5f1ea`, `#b8b0a4`, `#9a9285`,
  `#4b463e`. `glass` is the new ground at 82%, `shading` the warm ink
  (light) and a warm step above the ground (dark) at 67%. The contrast
  sets raise `text-contrast-medium` and `-high` in the same cast, at
  7:1 or more on `bg` and `surface`. Status colours, content fills,
  `focus`, `hover` and `disabled` keep their values, except where
  `--check` asked: the light error tile `#cb6267` (ink at 4.5:1), the
  dark `status-error-full` `#ff4d49` (a destructive button's words at
  4.5:1 on the lighter surface), and under more contrast the light info
  `#83a7fa`, success `#7ce0b8` and error `#ed8a89` tiles and the
  dark success `#1e5a56` and warning `#664d06` tiles (ink at 7:1, 0.09
  apart under red-green colour blindness, error still the darkest). Four
  named accents move just enough to keep 3:1 on the new surfaces: teal
  light `#009694`, pink light `#d556b4`, mauve dark `#7e6892`, magenta
  dark `#d90a74`. The neutral square of a tag in the ink toast takes the
  toast's words' colour in every contrast setting (the warm placeholder
  grey sinks into the ink). rekta-core's reference images are drawn
  again in the new colours; the showcase's icons and manifest take the
  warm ink and ground.
- **More space around sections and their titles** (layout.md, "A
  section's space", now a rule of its own): a section's content stands
  `space-xl` from its top and bottom (was `space-l`), and its title
  `space-l` from what follows (was `space-m`). Other blocks of a
  section keep `space-m`, rows and tiles `space-s`; no token changes.
  On the web `.rk-sec` and `.rk-sec > .rk-group + *`, and a table head
  sticking in a section moves with the padding. rekta-core: a panorama's
  section sets its content `space-xl` in, and a group title that is a
  direct child of the section stands `space-l` from what follows; three
  reference images show it (`motion-turn-out`, `sheet`, `toast`).
- **A sparser accent** (colour.md, forms.md): the accent fills only the
  primary button, a selected item, selected text and the accent surface,
  and it is the focus ring. `focus` now takes the accent's value (ink by
  default, so the blue ring is gone); every accent holds 3:1 on `bg` and
  `surface` in each theme, contrast set and on the black ground, so the
  ring does too (lowest: 3.01:1, pink in light and magenta in dark). On
  a selected row or tree row the ring inside takes `on-accent`. Checked
  controls are states, not actions, and stand in ink: a checked
  checkbox or radio, a toggle switch's fill, the slider's fill, a chosen
  segment and a pressed toggle button take `text-primary` with `bg` on
  it (`accent-color` too). With the default accent (ink) they look as
  before; with a product's or a named accent they no longer take it.
  rekta-core draws the same; its chosen day keeps the accent and, with
  focus, an inset ring in `on-accent`. Nine reference images show the
  ink ring.

### Added

- **Any ground for a workspace** (shell.md, colour.md, platforms.md):
  the ground behind a desktop's windows may be a picture, a pattern (an
  imprint of tiles, say) or a gradient, the product's choice or the
  person's; plain `bg` stays the default. No words stand on it bare.
  Text on glass keeps its measure over white and black, the limits of
  every ground, so no token and no number changes. Rekta advises on a
  ground and never refuses one, measured as seen (laid over `bg`):
  calm (no more than a quarter of 16 × 9 cells holds more than 3:1
  inside) and grey under glass (the glass over each cell's average
  stays below chroma 0.05); a ground the advice speaks against gets a
  hint once, also said in a polite live region, and the choice stands. A picture
  covers the surface, cropped about its centre or a point it names; an
  accent may come with it, in the product's slot. `rekta.color.js`
  gains `GROUND`, `groundSamples({ data, width, height })` and
  `groundCheck(bands, cells, theme)`; `--check` measures the showcase's
  picture with them in every theme and holds them to a striped and a
  saturated ground, to the edge of a quarter, and to pictures smaller
  than the grid. The showcase's desktop offers a plain ground, the
  picture "sky" (CC0, see NOTICE) and a picture of your own. rekta-core
  has no workspace ground yet (platforms.md).
- **Dusk, a third theme** (colour.md, platforms.md, behaviour.md,
  shell.md): made from the picture sky and picked by the person beside
  light and dark, never from a system setting; the system's light or
  dark setting picks its variant. Token sets `color.dusk-light` and
  `color.dusk-dark` hold only what changes from light and dark: the
  neutrals (light: `bg` `#f5f9fc`, `surface` `#e4ecf2`,
  `surface-2` `#d7e1e9`, `text-primary` `#10181c`,
  `text-contrast-high` `#475157`, `-medium` `#5a666c`, `-low`
  `#c7d3dc`; dark: `#0e1a21`, `#1d292f`, `#273238`, `#faefef`,
  `#bdadac`, `#a08e8e`, `#3f484e`), glass (the ground at 82%) and
  shading; status, content fills and the accent stay the theme's below.
  `color.dusk-light-contrast` and `color.dusk-dark-contrast` raise
  `text-contrast-high` and `-medium` to 7:1 over the contrast sets.
  The black ground lies over the dark variant. "Neutral surfaces" lets a
  theme made from a picture carry its casts, at the same chroma.
  `rekta.tokens.json` gains the themes `dusk-light`, `dusk-dark`,
  their `-contrast` forms, `dusk-black` and `dusk-black-contrast`,
  and each named accent's pair for them (the pair of the theme below);
  `--check` measures every pair in them, holds every theme's neutrals
  to a chroma of 0.022 (measured to three decimal places, as
  colour.md now says) and dusk's sets to the neutrals, glass and
  shading, and `--check-product` measures a product's accent and codes
  in every theme they show in, saying each fault once with the themes
  it holds in. On the web: `prefs` `theme` `dusk`,
  `:root[data-theme="dusk"]`; the showcase offers it. rekta-core does
  not have it yet.
- **The black ground** (colour.md, platforms.md, behaviour.md): a
  person with an OLED screen may put the dark theme on `#000000`. A
  choice, never the default and never from a system setting; the light
  theme keeps its own. Token set `color.black` holds only what changes
  from `color.dark`: `bg` and `glass` (the ground, as under less
  transparency); surfaces, text, hairlines, shading, status and the
  accent stay the dark theme's, and measure more on black (surface
  1.41:1 against 1.18:1, hairline 2.24:1 against 1.87:1). It lies over
  the dark contrast set alike. `rekta.tokens.json` gains the resolved
  themes `black` and `black-contrast` and each named accent's pair
  for them; `--check` measures both like the themes and holds surface
  and hairline on black at least to the dark ground's. On the web:
  `prefs` `ground` (`normal`, `black`), `:root[data-ground="black"]`,
  and the app bar does not blur there. rekta-core:
  `rk_app_set_black(bool)`; the demo and the showcase offer it.
- **Opener tiles in a side panel** (states.md, tiles.md, motion.md): a
  panel whose areas no longer fit its height may put each area behind a
  tile at its foot, with the area's icon, name and state word. A press
  opens the area's level and never switches; the area is switched in
  the level. The level grows out of the tile over the panel below its
  head and shrinks back into it ("Growing into a level": resize and
  re-flow, `duration-moderate` on `ease-base`, no new token); focus goes
  into the level and back to the tile at once; under reduced motion the
  level stands at once. Toggle buttons stay for the two to four most
  frequent states, with one action (forms.md). states.md no longer says
  "never tiles" for a side panel; a tile still never toggles. On the
  web: `.rk-opener` with `.rk-opener-name` and `.rk-opener-state`, and
  `.rk-levels` around the panel's content and its levels; `inPlace()`
  grows a level out of a tile that opens it and keeps the content
  `inert` under it instead of `hidden`. The showcase's quick settings
  have two.
- **A picture in a row** (collections.md): a list whose items may be
  pictures shows a small picture of such an item at its row's leading
  edge, where an icon would stand: a square at `row-height`, cropped
  about its centre, square-cornered, with the rows' hairline inside its
  edge. While it loads and when it cannot be shown, the square stands
  in `surface`, still. The words are two lines, what the item is over
  where it came from and when, and they are the row's name; the picture
  is hidden from assistive technology. On the web: `img.rk-row-pic`;
  `start()` gives one that cannot be shown a transparent pixel for its
  source, as WebKit draws its broken-picture sign over any cover.
  rekta-core has no picture in a row yet (platforms.md). The showcase
  has a list of what was copied.
- **The thin scrollbar** (layout.md): only the grip, no track, square,
  `text-contrast-low` at rest and `text-contrast-medium` under the pointer
  and while dragged, always visible while the content overflows, in a
  gutter of its own of 10px (the grip 2px inside it). Tokens
  `size.scrollbar-thickness` (6px) and `size.scrollbar-grip-min` (11px),
  measured from Chromium's native `thin`. On the web it stays the native
  bar in Rekta's colour, now on every scroller (sheets, side panel,
  session bar, `.rk-float`, palette list, table wrap, split panes and the
  page itself; the section strip and the recent stack keep `none`).
  `--rk-scrollbar` stays the sheet's hook, so no size is named `scrollbar`.
- **rekta-core's `rk_scroll` draws that bar** from the two tokens. It was
  4px, a grip of at least 24px, `text-contrast-medium` and `text-primary`
  while dragged, hidden 900ms after the last scroll, and its rail took
  presses over the content's trailing edge. Now the content is narrower
  by the gutter while it overflows. `rekta.3` and `rekta.h` say so. Lists,
  tables, menus and the panorama draw no bar yet (platforms.md).
- **An app-bar control whose surface is open** (navigation.md) holds the
  press tint (`hover`) until the surface closes, as a button whose menu
  is open does, and says it is expanded; the tint comes and goes over
  `duration-short` · `ease-base` like every press tint. On the web:
  `.rk-ab[aria-expanded="true"]`, which `menu()`, `palette()` and
  `appbar()`'s "more" already write. The showcase's clock and quick
  settings write it for their month and side panel.
- **Hiding a lasting state the person knows** (states.md, behaviour.md,
  shell.md, motion.md, platforms.md): a product may mark, per state,
  that the person may hide it. A quiet × ("hide") after the banner's
  action folds the banner into the app bar's word until the state ends;
  the banner leaves as when its state ends, its room freed, and a polite
  announcement says where the state stays. A new start of the state
  shows the banner again, and so does any state not hidden that starts
  (the banner shows the most severe state that is on and not hidden).
  A state can be hidden only while the app bar says its word (with one
  word, only the most severe state that is on), and hidden it still
  does what it did (a hidden offline state still stops live drawing).
  A halt is never hidden, nor any state the product does not mark, and
  no state where there is no app-bar word to keep it. On the web:
  `banner()` states take `hideable: true`; the handle gains
  `hidden(name)`; the × is `.rk-banner-hide`, made in `.rk-actions`;
  strings `hide` and `bannerHidden` (en, de). Nothing changes for a
  state without `hideable`. The showcase's offline state can be hidden.
  rekta-core has no banner yet.
- **A block to copy** (typography.md): values a person copies somewhere
  else (a version for a bug report, an id) stand in a statement block
  that takes focus as one stop and ends in a quiet hint, "Ctrl+C copies"
  (⌘C on Apple). Ctrl/Cmd+C copies it whole as plain lines; a selection
  in it is copied as anywhere. A toast says "copied". A refusal is said
  at the block: the hint turns into "not copied" in a field error's tone
  for `timing.toast`, said once in a live region, then comes back. Each
  line stays one line, cut with an ellipsis, whole in the overflow tip;
  assistive technology reads the values as they stand.
  On the web: `copyable(el, { toaster, text })`, which writes
  `data-rk-copyable`; `.rk-hint[data-status="error"]` takes the field
  error's tone anywhere; and the strings `copied`, `notCopied` and
  `copyHint` (German "kopiert", "nicht kopiert", "… kopiert").
  rekta-core has no block to copy yet (platforms.md). The showcase's
  "get started" has one with its version and the browser.

### Contributing

- CONTRIBUTING.md: the noreply identity in a fresh clone, the checks
  before every push, findings as review comments that the author fixes,
  merge commits on GitHub and the branch deleted after.
- `scripts/scan-secrets.sh`: gitleaks, local home paths (in binary files
  too), mail addresses, a private tracker's ticket ids and non-noreply
  identities, in the working tree and in what each commit not yet on
  `main` adds, and the patterns of a local, gitignored
  `.privacy-patterns`, reported by their number only.
  `scripts/scan-secrets.test.sh` plants each case in a throwaway
  repository. CI runs both, without private patterns.

### Fixed

- A sideways section's clip cut the focus ring of a control flush with
  its leading edge (a tile, a row, a field, a button, a block). `.rk-sec`
  now leaves the ring's reach (6px, the ring's width plus the largest offset)
  free at that edge, scrolls a focus into view with that reach free above and
  below too, and the section's place in the panorama stays where it was;
  accessibility.md says that a container that clips keeps its children's
  ring whole, platforms.md lists rekta-core's section as still clipping it.
- The dots fade in on `dots-ease-arrive`, with their flight, as
  rekta.css does; data.md and the description of
  `motion.dots.whole-at` named `ease-base`. An implementation that
  followed the docs now fades a dot in on `dots-ease-arrive`; the fade
  out stays on `ease-base`.
- `panorama()` no longer cancels the rest of a wheel gesture at a
  section's end, which took the platform's own answer there away (a
  touchpad's bounce). The section holds its scroll chain while the
  gesture lasts instead (`overscroll-behavior-y: contain`, inline, its
  own value back after), so the gesture still never turns sideways or
  reaches the page, and at the panorama's ends the wheel still falls
  through. layout.md and behaviour.md say the rest of the gesture
  "stays there" instead of "is swallowed" (#150).
- `panorama()` holds a section's scroll chain during a gesture only
  while its `overscroll-behavior-y` is `auto`; a product's own value
  (`none`) stays, as motion.md says Rekta forces no answer.
- Less transparency also takes the blur from the shading behind the
  recent stack (`.rk-recent::backdrop`), as for a sheet and the scrim;
  platforms.md and colour.md already said nothing blurs there.
- A menu with icons: a picture icon (`.rk-picture`) counts for the shared
  edge like Rekta's icon, and the square of a choice stands in the
  icons' field (or, when an item has square and icon together, every row
  keeps the square's room), so all rows start their words at one edge.

### Documented

- collections.md: a reordered item settles in `duration-moderate`
  after release, as motion.md and `sortable()` have it; `duration-short`
  is only for the others making room while the drag runs.
- navigation.md, "Back with a router of your own": the scroll snapshot
  goes in the layout that holds `.rk-place` (the page only when it
  renders the `main` itself), and a `restore()` still waiting ends with
  the next navigation (`onNavigate`, `routeChangeStart`), in the layout
  and in the page alike, never only with the destruction of a component.
- README: what Rekta is for, in the first lines; "What's in the box" by
  layer: the specification, tokens and icons, the web profile,
  rekta-core, each one implementation among several.
- principles.md: the surfaces in the order shell, app, phone, web, and
  the one sentence on what comes first.
- platforms.md: Rekta as a specification; a glossary of the words the
  docs use (window, screen, pane, place, and the web words: viewport,
  page scroll, hover, back and Escape, `prefers-*`, tab, `<dialog>` and
  the top layer, popover, URL and route, the root, first paint, `.rk-*`
  and `data-rk-*`, `role` and `aria-*`) and what each means on every
  platform; the CSS examples of the product overrides say they are the
  web's; `fluidDimension` is named as Rekta's own token type.
- Tokens: `typography.font-family.sans` and `.mono` and `layer.float`
  describe their rule without CSS words; the CSS output is unchanged.
  states.md's layer table says the same of `layer.float`.
- The six places where a rule existed only as its web mechanism now say
  the rule first, for every platform, and the mechanism after it as "On
  the web: …": the modal surfaces (a sheet, the side panel, the session
  bar, the recent stack: what they hold, shade and close on), the
  stage's height and its jumps (layout.md), the names of classes and
  attributes (data.md, live.md, typography.md, shell.md), role, name and
  focus path (forms.md, states.md, navigation.md), back and the history
  (navigation.md, forms.md), and the browser capabilities named in
  passing (anchor positioning, the top layer, view transitions, hover).
  No rule changes; behaviour.md stays the web contract.
- motion.md: the end of a scroll belongs to the platform. Rekta adds no
  give of its own there, for any input, and does not suppress the
  platform's own answer (#150).

## 0.6.0 — 2026-11-02

The reports #87–#157 on 0.5.0: motion, controls, layout and parity,
accessibility, colour and the patterns of a whole system's surfaces (a
launcher, a lock screen, notifications, windows). Rekta is one language
for every surface; a pattern describes its main case, and a product may
deviate where it keeps Rekta's rules (principles.md). Declined: #146,
#150, #153; deferred: #155. Open points: #159; rekta-core's parity with
the web: #160. And rekta-core: what was left after #35 (#84).
rekta-core's soname follows the minor version (`librekta.so.0.6`), so
apps linked against 0.5 relink.

### Changed (may need a change in an app)

- **A split's detail slides only after a real advance.**
  `rk_split_set_detail_shown(true)` slides sideways only when no item
  showed before; the first display and the same item shown again stay
  still. A next item slides with `rk_split_reveal(split, dir)` (or a new
  `.pulse` on a declared `RK_SPLIT`), from the side of travel.
- **Contrast is the user's preference.** `data-contrast` on the root is
  now the more-contrast switch (`more`, `normal`; unset follows
  `prefers-contrast`), with the colour sets `color.light-contrast` and
  `color.dark-contrast` (every text pair at 7:1, opaque glass, a 2px
  frame on status tiles, a destructive button's words in ink). A page
  that used `data-contrast` for its own purpose renames it (#156).
- **A grid of tiles that open is one tab stop** with `Rekta.grid(el)`
  (`start()` calls it): the arrows move by position (#129).
- **Letters jump** in a select, a long list, a tile grid, a tree and a
  menu; letters typed within `timing.type-ahead` form one prefix (#134).
- **`disable()` on an app-bar control** keeps it focusable, greys glyph
  and word, puts the reason in its tooltip and a hidden description; in
  "more" it is a disabled item naming the reason (`cannotAct`) (#126).
- **`confirm({ safe: 'cancel' })`** marks its `ok` button destructive
  (`data-status="error"`); `sheet()` and `guard()` mark their discard
  button the same; core has `RK_BUTTON_DESTRUCTIVE` (#101).
- **Key hints** are written one way: Ctrl, Alt, Shift, Meta, then the
  key, joined by "+"; Apple's symbols ⌃⌥⇧⌘ without a separator; Meta
  outside Apple is "Win" or "Super" (`keyMeta`) (#141).
- **Inputs with `aria-disabled="true"`** look disabled; a stepper and a
  drop zone refuse (#108). Selected text takes the accent and the caret
  the text colour (#136).
- **Floating surfaces follow one rule:** `size.tip-gap` from their
  control, `space-m` from the window edge, the side with room, clamped,
  and they scroll when longer than the window; `data-rk-align`, a
  place's `align` and `rk_menu_set_align` centre them (#115 #131).
- **`scrollState().restore()`** returns `{ pending, cancel }` and waits
  up to `timing.restore` (5s) for the content (#102).
- **A sheet answers when it starts to leave**; a close request while it
  leaves only finishes it; focus returns after it has gone (#124).
- **Sparklines** draw a fixed 2px line, inset 1px; a single value lies
  flat at the bottom (#116).
- **Motion:** `motion.transform.turnstile-angle` is a deprecated alias of
  `turnstile-out-angle`; app-bar controls only fade; reflow moves
  without scaling; a live tile's news is staggered by `duration-greet`
  (#88 #111 #113 #117 #118 #107 #122 #148 #143).
- **The accent has a rule with numbers** (#97): 3:1 on bg and surface,
  on-accent 4.5:1 (7:1 under more contrast), and apart from status: 0.09
  OKLab from every status colour, 0.05 under protanopia and deuteranopia,
  its hue 20° from each status hue unless its chroma is below 0.05.
  `--check-product` checks a product's accent and fails one that breaks
  it. rekta-core's text on a custom accent is the ground or the ink,
  whichever holds more contrast.
- **`data-accent` on the root is Rekta's** (prefs `accent`, the named
  accents). A page that used it for its own purpose renames it.
- **Tiles arrive** (#144): a tile building in rises `arrive-distance`
  (32px) over `duration-long` on `ease-landing` and is whole after
  `duration-short`; tiles follow one another `duration-greet` apart, four
  steps at most (`--rk-i` 0–3). The arrival window is 3 × greet + 2 ×
  long + 50ms.
- **A panorama never ends flush on a section's edge** (#147): the gap
  between sections changes a little, never below `space-m`, so at least
  `space-xl` of the next section shows.
- **An app bar with a centred control** (`data-rk-align="center"`) has
  two sides, each giving away only its own controls (#120); running
  things (`.rk-ab-run`) go into "more" before any command (#98).
- **rekta-core's title bar** (#95): window buttons `touch-target` wide (44,
  was 46), maximize and restore glyphs, the name in `text-contrast-high`
  while another window is active; `rk_strings.restore` is a new word
  after `plural`.
- Tiles three and four units wide (`.rk-w3`, `.rk-w4`) take a compact
  grid's whole row (#94). `tileUpdate()` also peeks a part marked
  `data-rk-peek` (#120). rekta-core's date cell is `size.day-cell`
  (36px, unchanged) (#127).

### Added

- `rk_split_reveal(split, dir)`: 1 from the trailing side, -1 from the
  leading side, 0 a replace; declared `.reveal` with `.pulse`.
- More of core's words in `rk_strings` (at its end): month and weekday
  names, the status words, a list's count, the window buttons, a drop's
  default name. Counts have plural forms (`rk_plural`, a `plural`
  callback, English by default; a word given in any form is the app's in
  all of them).
- `rk_format_number(out, size, v, compact)`, the web's `number()` in C:
  decimal sign, digit groups (with CLDR's minimum grouping digits) and
  compact forms from `rk_strings`; sliders, steppers and counts use it.
- `test/words.sh` also catches counts and figures written with printf;
  46 words come from the table.
- Motion tokens: easings `turn-out`, `turn-in`, `in-out`; turnstile out
  and in angles, reach, row and flush; peek travel, flip angle;
  `motion.proportion.*`, `motion.heft.*`; durations `greet` and
  `beads`; `busy.*`, `dots.*`, `beads.*`. `.rk-dots--beads`,
  `data-rk-motion="inline"`. `--check` holds every keyframe stop and
  curve to its token (#88 #107 #117 #118 #122 #143 #148).
- `.rk-btn[aria-pressed]`, a toggle button, with `.rk-btn-state` (#140).
- `.rk-facets`, a filter bar the build gives a container query
  (`data-rk-narrow`); `pivot()` measures it (#99).
- `.rk-status-region`, a polite live region for status lines (#114).
- Tokens `size.tip-gap`, `timing.restore`, `timing.type-ahead`,
  `typography.metrics.ascent` and `descent` (#112 #115 #102 #134).
- Preferences `contrast` and `transparency` (`data-transparency="reduce"`
  or `prefers-reduced-transparency`: glass becomes the ground) (#156).
- `rk_app_set_contrast`, `rk_menu_set_align`, `RK_TIMING_TYPE_AHEAD`;
  core lists and tables jump by letter, trees by prefix (#156 #131 #134).
- `virtual({ text })` names a row's text for letter jumps (#134).
- Strings `keyMeta` and `cannotAct` (#141 #126).
- `--check` measures text on glass against all white and all black, a
  tag in an ink toast, and rejects `any` in the declarations (#130 #139
  #104).
- **Colour:** named accents (`tokens/rekta.accents.json`: ink, teal,
  steel, violet, mauve, pink, magenta; `data-accent`, prefs `accent`,
  `rk_app_set_accent_named()`) (#151); an accent derived from one colour,
  `build.mjs --accent COLOUR` (#152); `dist/rekta.color.js` with the
  colour rules (#97); `.rk-accent-surface` (#154); content fills for a set
  of tiles without a status (`data-rk-fills="content"`, `data-rk-fill`,
  tokens `content-*`, off under more contrast, never beside a status;
  `rk_tile_grid_set_content`, `rk_tile_set_content`) (#157).
- **Shell patterns:** the side panel (`.rk-sheet--side`) and choosing in
  place (`inPlace()`) (#90); a notification centre and `removable()`,
  `rk_list_on_remove` (#91); the level indicator, `toaster().level()`,
  `rk_window_level`, `indicate()` in the app bar's middle, both on
  `timing.level` (1.5s) (#92, #120); picture icons, `.rk-picture` (#93);
  the launch tile (`.rk-launch`), `size.tile-icon`, `size.tile-icon-large`
  (#94); notifications from other programs in toasts (`edge`, `title`,
  `icon`, `picture`, `open`) (#121); the preview tile and the recent stack
  (`.rk-tile--preview`, `dialog.rk-recent`) (#123); a month to look at
  (`.rk-month`, `rk_month()`) (#127); a menu's second level in place and
  an icon per item (`.rk-menu-level`, `rk_menu_set_icon()`) (#128); the
  session's actions (`.rk-sheet.rk-session`) (#132); the display type
  step (`typography.size.display`, `.rk-display`, `RK_TEXT_DISPLAY`) and
  the lock screen (#142).
- **Icons:** maximize, restore, speaker, speaker-muted, microphone,
  microphone-muted, wireless, bluetooth, battery, music, previous, next:
  63 in all (#95, #138).
- **Motion:** places called up turn at half the pace (`data-rk-called`,
  `rk_controller_set_called()`) (#133); the carried title
  (`data-rk-carry`, `rk_view_set_carry()`, `turn` `carry`) (#145); the
  gesture hint, `nudge()` and `rk_view_nudge()` (#149); tokens
  `easing.landing`, `arrive-distance`, `nudge-distance`,
  `step-back-scale`, `step-back-brightness` (#89, #144, #149).
- `--check-product` fails a font loaded from another server: the family
  is a product slot (#96).
- **The showcase shows everything** (the GitHub Page): a new "system"
  section, a small desktop (window buttons, an app bar with running
  things, a centred clock and its month, volume keys, a notification
  centre, toasts from other programs, quick settings in a side panel, the
  session, a lock screen, the recent stack), a place "all apps" called up
  at half the pace (launch tiles, content fills, the accent surface), and
  the wave-1 parts it lacked (facets, a status region, beads, inline
  motion, letter jumps, a disabled app-bar control).

### Fixed

- Pivot tabs inherit the font (#100); `Word` is `never[]` where it must
  be (#104); segment joints (#109); the select's chevron is stroked like
  the combobox's (#135); a tag in an ink toast shows its square (#139).
- Pivot titles sit on the baseline (#112); floating surfaces never leave
  the window (#115); a sheet's shading fades in and out with its curve;
  `confirm`'s ok is no longer turned into cancel (#124); the mark is
  ink (#106).
- Inactive places in the phone app bar reach the contrast of text on
  glass (#130); a tooltip shown by a tap hides on lift (#126).
- The arriving item of a turnstile is whole at 35% as documented; core's
  busy line follows the web's path and stays still under reduced motion
  (#88 #113).
- The showcase's sample accent read as "done" (0.038 from success); it
  is now `#1a6680`, and the accent form says why it refuses a colour.
- rekta-core: a tap that ends a leaving sheet no longer reads a freed
  view (found by ASan).
- A disabled place in a compact app bar keeps the disabled colour,
  current or not (#137 by jay23606, in #162).
- The panorama's mark: at its start it is the first section, however
  narrow the sections (#164); a jump by key keeps its target marked for
  the whole glide instead of falling back to the old section (#165).
  behaviour.md states the rule.

### Documented

- motion.md: a place change in detail, a live tile's news, the app-bar
  controls; data.md: busy, dots and beads.
- The minimum pending time at a button (#110); a select's list (#135);
  caret and selection (#136); disabled per control (#108); writing a key
  (#141); tags in toasts (#139).
- "until" and "since" (#125); share bars in rows (#105); a place name's
  markup and that it turns with its place (#103); toasts with the same
  key (#87).
- Text on glass and more contrast (colour.md, accessibility.md,
  platforms.md); a grid of tiles and typing to filter or jump
  (navigation.md); the tooltip exception for disabled controls
  (forms.md).
- principles.md: one language for every surface, and what a product may
  deviate from.
- colour.md: the accent (apart from status, named, derived, chosen by
  users, the accent surface) and content fills; tiles.md: content fills,
  the launch tile, the preview tile, sizes up to 4 × 2.
- motion.md: other programs' windows, the windows stepping back, the
  side panel, choosing in place, the level indicator, a place called up,
  the carried title, the gesture hint; layout.md: never flush at the
  edge.
- shell.md: window buttons, picture icons, the ground of a workspace,
  the session, the lock screen; live.md: a notification centre;
  states.md, navigation.md, forms.md and typography.md for the patterns
  above; platforms.md: what rekta-core does not draw yet.

Still English in core: the date box's empty "yyyy-mm-dd" and its field
order.

## 0.5.0 — 2026-10-19

Reports #67–#83 on 0.4.0, and rekta-core caught up with the web (#35).
rekta-core's soname follows the minor version (`librekta.so.0.5`), so
apps linked against 0.4 relink.

### Changed (may need a change in an app)

- **A toast's window is wall-clock time** (#67): it runs against a
  deadline, so a busy page or a background tab no longer stretches it.
- **A same-key toast tells the previous `onEnd` `'merged'`** when it
  brings its own (#68); add `merged` next to `undone` where an app
  completes the action in `onEnd`.
- **A pending button keeps every variant's look** (#73): one exception in
  the disabled rule instead of repainting primary and error; `--quiet`
  no longer grows a frame.
- **An open sheet keeps the scrollbar's room as padding** (#75):
  `:root` gets `padding-right: var(--rk-scrollbar)`, which `sheet()`
  measures, instead of `scrollbar-gutter: stable`.
- **A wide facet row is at most as tall as the window** under
  `--rk-stage-top` and its facets scroll within themselves (#80).
- **`places()` keeps scroll offsets by box** (id, `data-rk-scroll`,
  else order) and includes a wide facet row and its facets (#82).

### Added

- Toasts: `release()`, `setText(text, { announce })`, `remaining` and
  `held` that follow the clock (#69, #70). Banner: `set(name, true,
  { text })` and `update(name, text)` for a state that is on (#71).
- `sheet()` takes Escape on its `keydown` while there is something to
  lose: Escape, Escape, Escape never closes it without asking (#72).
- `appbar()` lays out again when a child grows (the live word), on a
  pinned `.rk-ab-mid`, and once fonts load (#74); `.rk-ab-mid` lays out
  several parts and can be pinned (#78, #79).
- `pivot()`: `select('name')`, `sync()`, `chosen`, `gone`; facets that
  come and go keep one tab stop (#81). The wheel over a wide row below a
  title block scrolls the window first (#80).
- `scrollState(root)` for routers of their own (#82).
- `tokens/rekta.hooks.json`, shipped as `hooks` in `rekta.tokens.json`;
  `--check` holds the docs' table to it (#77).

### rekta-core (#35: one design with the web)

- **Its words are the app's** (web #16): `rk_strings` and
  `rk_app_set_strings()`, English by default with the web catalogue's
  keys (undo, redo, undone, ok, cancel, choose, back, move, on, off);
  `make test` fails when such a word is written anywhere else.
- **Domain colour codes** (web #33): `rk_app_set_series()`, held to the
  web's product rules (3:1, 0.09 apart under protan and deutan, ported
  to C); `rk_tile_set_code()`, `rk_row_set_code()`; declared `.code`.
- **Product overrides** (web #11): `rk_app_set_metric()` for
  `RK_TILE_UNIT` and the new `RK_PIVOT_WIDE`, `rk_app_set_timing()` for
  `RK_TIMING_TOAST` and the new `RK_TIMING_STALE`; `size.pivot-wide` and
  `timing.stale` are in `rk_tokens.h`.
- **Facets side by side** (web #12, #24): `rk_pivot_set_wide()`,
  `rk_pivot_is_wide()`; declared `.wide`.
- 285 exported functions (9 new); `rk_ui` and the enums grew at their
  end only. Still English: the date picker's month and day names, the
  status words, item counts, a drop's default words; locale-aware
  numbers are not there yet.

### Documented

The five recipe and comment fixes of #76; one rule for on/off controls,
acts at once or waits for "save" (#83).

## 0.4.0 — 2026-10-19

Reports #36–#65, from products built on 0.3.0: Rekta Design in apps with a
framework router and a server. rekta-core is unchanged apart from its
version: its soname follows the minor version (`librekta.so.0.4`), so apps
linked against 0.3 relink.

### Changed (may need a change in an app)

- **A nav pivot turns only with `places()` or `pivot(el, { turn })`** (#40).
  A facet pivot no longer refers to the turn, so a bundle that only uses
  `pivot` drops it (6.0 → 4.0 KB gzip). A nav pivot without either now
  slides like a facet pivot.
- **`pivot()` owns `hidden` on its panels one facet at a time, from the
  start**; wide, it shows every panel with `data-rk-reveal` and leaves
  `hidden` alone (#36).
- **Counting strings are plural forms** (#54): `undoWindow`, `selected`,
  `guardText`, `updatesWaiting` and `needAttention` are
  `{ one, other, … }`, picked with `Intl.PluralRules`. A function given to
  `setStrings()` still works.
- **The banner's time line is the state's real start** (#47): kept per
  state from `set(name, true)`, or the `since` given; `since: false` shows
  none.
- **Places in the app bar hide the places' top strip** (#50) through
  `:root[data-rk-places]`, which `appbar()` writes; pinned controls move
  into "more" while the places show.
- **A sheet is sized against the viewport** (#61), so it spans a phone
  whatever the scrollbar gutter.
- **`.rk-title` breaks long words** (`overflow-wrap: anywhere`, #58): its
  narrowest width is now one character, so a title in a flex row breaks
  inside a word where it used to widen the row. Give such a row's title
  `min-width: max-content` (or `flex: none`) to keep the old behaviour.
  (Listed here after the release; #76.)
- **The showcase's light zones 1 and 3** moved to `#5489b6` and `#7d8d5e`
  (#52), 0.09 apart under tritanopia too.

### Added

- `strip(el)` for a router's strip of links, `arrive(el)` exported,
  `pivot()`'s `select(i, { silent })`, `layout()` and `turn` option; lengths
  in any form (`calc()`) (#37–#42).
- Toasts: `windowMs`, `onEnd(reason)`, `status`, and the toast's
  `setRemaining(ms)`, `remaining`, `held`, with `rk:toasthold` (#45, #46).
- Focus fallback: `start({ focusFallback })`, `[data-rk-focus-fallback]`,
  then `h1`, `main` (#43).
- Banner remedy as a link: `action: { label, href }` (#47).
- App bar: `data-rk-priority="pinned"`, key hints apart in "more",
  `data-rk-places`, `.rk-place-name` (#48–#50).
- `sheet(dialog, { dirty })`: unsaved input asks in the sheet before it
  goes; `.rk-sheet-close` (#59, #60).
- Buttons: `.rk-btn[data-status="error"]` for destructive actions,
  `.rk-btn--icon`, and a pending button keeps its colours (#63–#65).
- Data: `.rk-meter[data-rk-code]`, the share bar `.rk-shares` (#51);
  `.rk-figures` for a status region (#57); `.rk-clamp` for a long title
  (#58).
- TypeScript declarations: `dist/rekta.d.ts`, `dist/rekta.strings.de.d.ts`,
  `types` in `exports`, and `./strings.de.js` exported (#53).
- `--check`: declarations match the exports, counting strings have `one`
  and `other`, a destructive button's words hold 4.5:1.

### Documented

The keys Rekta takes, per layer and component, and that app handlers skip
`defaultPrevented` (#44); a facet in the address (#55); a growing live list
(#56); the status region (#57); a long main title (#58); a form in a sheet
and a visible way out (#59, #60); the error summary's markup (#62).

### Fixed

- The showcase's reminder said "in 2 days" for a date three days ahead
  after 21:00: it counts calendar days now.

## 0.3.0 — 2026-10-18

### Breaking (rekta-core: declared first)

The declared UI is now the way to write a rekta-core app, and the view tree
the public foundation under it. The experiment's separate `rekta_ui.h` is gone:
everything is in `rekta.h`.

| Was | Now |
| --- | --- |
| `rk_align` began with `RK_ALIGN_START` | it begins with `RK_ALIGN_AUTO` (a column stretches, a row keeps to the start): every value moved by one, so rebuild |
| `rk_confirm` returned nothing | it returns the sheet, so the app can dismiss it before the answer |
| `#include "rekta_ui.h"` | `#include "rekta.h"` |

### Breaking (rekta-core: the API review)

An independent review held rekta-core to modern library practice and to how
it feels to use. Its API is now smaller in concepts and says what it means;
apps change as follows (`man rekta` has the whole API):

| Was | Now |
| --- | --- |
| `rk_button_set_disabled(b, reason)`, `rk_control_set_disabled` | `rk_view_set_enabled` on any view, with `rk_view_set_tooltip` for the reason |
| `rk_menu_set_disabled` | `rk_menu_set_enabled` |
| `rk_menu_set_check(menu, i, -1/0/1)` | an `rk_checkmark`: `RK_CHECKMARK_NONE`, `_OFF`, `_ON` |
| `rk_app_after(ms, repeat, fn, user)` | `rk_app_after(ms, fn, user)` once, `rk_app_every` again and again |
| `rk_app_on_change` returned nothing | returns an id for `rk_app_cancel` |
| `rk_window_do(w, label, run, undo, user, free)` | `rk_window_do(w, &(rk_action){ .size = sizeof(rk_action), ... })`; each window has its own history |
| `rk_window_choose_files(w, accept, bool, ...)` | `RK_CHOOSE_ONE` or `RK_CHOOSE_MANY` |
| `rk_field_set_hint(f, text, error)` | `rk_field_set_hint` and `rk_field_set_error` |
| `rk_view_set_rule(v, top, bottom)` | `rk_view_set_rules(v, RK_RULE_TOP \| RK_RULE_BOTTOM)` |
| `rk_list_selected` | `rk_list_is_selected`; `rk_list_item` starts with `size` |
| `rk_tree_open(t, item)`, item -1 for all | `rk_tree_set_open(t, item, open)`, `rk_tree_set_all_open` |
| `rk_split_show(s, detail)`, `rk_split_sideways`, `rk_split_detail_shown` | `rk_split_set_detail_shown`, `rk_split_is_sideways`, `rk_split_is_detail_shown` |
| a table's sort as a direction 1 or -1 | `rk_sort` (`RK_SORT_NONE`, `_ASCENDING`, `_DESCENDING`); column flags `uint32_t` |
| `rk_column`, `rk_row` (stacks) | `rk_vstack`, `rk_hstack`; "row" is a hairline row, "column" a table's |
| `rk_pivot_add(pivot, controller)` | `rk_pivot_add(pivot, title, view)` as the panorama's; `rk_pivot_add_controller` |
| `rk_progress_set` | `rk_progress_set_value` |
| `rk_view_hidden`, `rk_app_reduced_motion` | `rk_view_is_hidden`, `rk_app_is_reduced_motion` |
| `rk_app_stats(s, reset)` | `rk_app_stats(s)`, `rk_app_stats_reset()` |
| `rk_control_value` of a date as yyyymmdd only | also `rk_date_value(d, &y, &m, &d)`, `rk_date_set_value` |
| `*_COUNT` members of public enums | gone; the library keeps its own counts |
| testing calls in `rekta.h` | in `rekta_test.h` (still exported) |
| `rk_view_find` by name or a button's label | by name; a button's name is its label, or its icon alone |
| a view already in a tree needed `rk_retain` to move | `rk_add` and `rk_insert` move it |
| `rk_window_create` returned a reference | the window belongs to the app until it closes; the pointer is borrowed |
| constructors could return `NULL` | never: out of memory the library aborts |
| apps padded a place's page by hand | places set a page in, as a pivot sets its facets in |

### Added

- **Facets side by side.** From `size.pivot-wide` (a new token, 1300px,
  a product decision) a pivot's facets show side by side as panorama
  sections; the strip is a jump list that glides to a facet and follows the
  scroll (`pivot()`, `data-rk-wide`). Narrower, one facet at a time again.
- **Product overrides** are documented as the way to make Rekta Design a
  product's own (`docs/platforms.md`): the accent per theme, the toast and
  stale timings, the tile unit, `size.pivot-wide`, density as a user
  setting, and what must not be overridden.
- **rekta.js speaks the product's language.** Every word it shows or says
  comes from `Rekta.strings` (English by default; `start({ locale,
  strings })`, `setStrings()`, `setLocale()`), chart figures and live
  times follow the locale (`number()`, `locale()`), and a German catalogue
  ships as `rekta.strings.de.js` (`?lang=de` in the showcase). `--check`
  fails on a phrase written outside the catalogue.
- **A domain's colour code.** A product may set `--rk-series-1` to `-8`
  per theme for colours its users already read as information (heart-rate
  zones, say): small marks only, never text or a tile; `chart()` series
  take `code: N`, `.rk-tag[data-rk-code]` a coded square;
  `build.mjs --check-product` holds the values to 3:1 and to colour-blind
  separation. The showcase shows five heart-rate zones.
- **Places in the app bar on a phone** (`.rk-ab-places`, opt-in, up to
  five): the top-level places in the thumb's reach, commands in "more",
  back first, the router marking the place you are in.
- **Facets side by side** keep the chosen one while it is whole in sight,
  and Tab from the strip goes into it.
- **Smaller additions:** `.rk-ab-mid` (the app bar's middle), `.rk-statement`
  (a statement block on the ground), the message tile's length limit,
  `--rk-toast-left` (the countdown line, public), `data-rk-reveal` (the way
  out of `hidden`), `data-rk-measured`, a row sparkline's colour and size.
- `rekta.js` imports without a DOM, so pages that render on the server can
  import it; `--check` imports it in plain Node.
- `build.mjs --check` reports the statuses' separation under tritanopia
  (not a failure; `docs/accessibility.md` says why), and fails when the
  version in `package.json`, `rekta.js`, rekta-core, the flake and the
  newest changelog heading disagree.
- **rekta-core: declared UI.** A window given `rk_window_desc.build` (with
  `.user`, and `.on_key` for keys nothing focused takes) calls it to know what
  to show: macros for every view, collection, menu and controller (`RK_TILE`,
  `RK_VSTACK`, `RK_TABLE` with `RK_COLUMN`, `RK_TREE` with `RK_ITEM`,
  `RK_PLACES`, `RK_PIVOT`, `RK_PANORAMA`, `RK_SHEET`, `RK_CONFIRM`, `RK_HOST` for
  a view made by hand) fill `rk_ui` descriptors. Every declared window builds
  again after each event, timer, post and close the library hands the app, and
  only what differs reaches the views, with the motion each change means: what
  goes leaves first, what comes enters, the rest reflow, a place turns, a tile's
  new value peeks in. Nodes are matched by `.key` or `.id` (`RK_ID(n)`), else by
  place. Bound values (`.on`, `.chosen`, `.number`, `.day`, `.buffer`, `.open`)
  show what the app holds after every build; given ones (`.value`, `.input`)
  are set again only when the declaration moves them. `.version` skips
  unchanged scopes, `.change` picks a transition (`RK_UI_IN_PLACE` for none),
  `rk_ui_fmt` formats text for a build, `RK_OPTIONS` passes a static array,
  `rk_ui_view` and `rk_ui_update` join declared views and trees made by hand.
  Fields a kind does not read, duplicate keys, scopes left by a `return` and
  sheets the user closed but the build opens again are said through the log.
  The demo, `hello.c` and `navigation.c` are declared; the demo is about a
  fifth shorter and draws its storm phase as fast as before.
- **rekta-core: every property changes in place.** Setters for what could only
  be set when made: `rk_label_set_style`, `rk_button_set_label`,
  `rk_control_set_label`, `rk_control_set_options`, `rk_control_set_range`,
  `rk_field_set_label`, `rk_field_set_placeholder`, `rk_icon_set`. Parts change
  where they stand: `rk_panorama_insert`, `_remove`, `_set_title`,
  `_set_width`; `rk_pivot_insert_controller`, `rk_pivot_remove`;
  `rk_table_insert_column`, `_remove_column`, `_set_column`; `rk_tree_move`;
  `rk_view_reorder` puts children in order without taking them out, so focus
  stays. Hooks for what the user did: `rk_places_on_back`,
  `rk_pivot_on_change`, `rk_controller_on_dismiss`, `rk_split_on_back`.
- **rekta-core: from the API review.** Disabled views with a reason: any view
  can be disabled (`rk_view_set_enabled`), greying it and everything in it,
  and a tooltip (`rk_view_set_tooltip`) floats after `tip-delay` or on keyboard
  focus. Misuse is said instead of passing silently or corrupting memory
  (`rk_app_set_log`): a setter on the wrong kind of view, a descriptor without
  its size, a call from another thread, an unknown icon, a pointer that is no
  object or already freed. Lists keep their selection by item
  (`rk_list_set_key`) and say it (`rk_list_selection`, `rk_list_active`,
  `rk_list_select`, `rk_list_clear_selection`); rows change in place
  (`rk_row_set`), menus are built again (`rk_menu_clear`), tree items are
  removed, renamed and carry the app's data. `rk_controller_embed` puts a
  controller inside another's views; `rk_confirm` asks a question in a sheet;
  `rk_field_set_secret` makes a password field; custom views move with the
  design's motion (`rk_view_animate_value`, `rk_view_value`) and set the
  pointer (`rk_view_set_cursor`). No fixed limits remain (10,000 tiles, 600
  children, nesting animation blocks); a relayout draws only what changed and
  measures each view once per pass; each source compiles on its own
  (`make modules`, in CI). The keys F1 to F24 and Insert arrive on every
  platform.

- **The reference gallery**: `core/test/gallery.c` and `test/gallery.mjs`
  draw the same thirteen specimens (button, checkboxes, toggles, segments,
  fields, slider, progress, rows, a tile, a drop place) with rekta-core and with
  rekta.css, light and dark, and CI holds them to each other: the frame's
  size, where the ink lies, the solid colours, and the shape at a quarter of
  the size. A report shows each pair side by side. The first run found four
  places where core had drifted from the specification, fixed below; the web's
  checkbox, the browser's own at first, is now drawn as core draws it.
- **rekta-core: packaging**: `make shared` builds `librekta.so.0.3`
  (`librekta.0.3.dylib`, `rekta.dll`; `build.bat shared` with MSVC), built with
  hidden visibility so it exports only the header's `RK_API` functions;
  `make abi` holds it to that and runs a program against it. `make install`
  adds the shared library and `rekta.pc`; `core/test/install.sh` installs into
  a fresh prefix and builds an app through pkg-config, shared and static. The
  Nix package installs the same.
- **rekta-core: notifications** (`rk_app_notify`): a title and a body from the
  system. On Linux they go to `org.freedesktop.Notifications` over D-Bus from a
  thread of their own, with the body escaped for servers that read markup and
  the app's id as the desktop entry; on Windows a balloon (a toast on 10 and
  later) from an icon in the notification area; on macOS through
  `UNUserNotificationCenter`, loaded at run time, for apps in a bundle. Tested
  end to end on Linux by `core/test/platform/desktop.sh`.
- **rekta-core: X11 follows the scale while the app runs**: when the desktop
  changes `Xft.dpi`, each window keeps its logical size in the new number of
  pixels (`core/test/platform/scale.sh`). X11 has one scale for the screen; no
  desktop publishes one per monitor, so there is none to follow.
- **rekta-core: files dragged in, and a place to drop them** (`rk_drop`), as
  the web's `.rk-drop`: a 2 px dashed text-medium edge that turns solid, in the
  text colour over the hover fill, while files are over it; a filter by
  extension or family (`image/*`); the app hears the files taken and those
  refused. New events `RK_EVENT_DRAG_ENTER`, `_DRAG_MOVE`, `_DRAG_LEAVE` and
  `_DROP` (with `files`, `nfiles` appended to `rk_event`): the view under the
  pointer that takes `DRAG_MOVE` gets the drop, and files dropped where none
  does are refused. `rk_window_choose_files` asks the system's file chooser,
  filtered as the drop place is, and answers later; a click on the drop place
  opens it. On Linux it asks the desktop portal over D-Bus (libdbus looked up
  at run time), else zenity or kdialog (tested end to end by
  `core/test/platform/chooser.sh` against a stand-in portal); on Windows the open
  dialog (comdlg32 looked up at run time); on macOS an `NSOpenPanel` as a sheet. Files come in on every platform: XDND 5 on
  X11 (tested end to end under Xvfb by `core/test/platform/x11.sh`), the data
  device on Wayland, an OLE `IDropTarget` on Windows (ole32 and shell32 looked
  up at run time) and `NSDraggingDestination` on macOS.
- **rekta-core: rows the user puts in order** (`rk_rows_set_sortable`), as
  the web's `sortable()`: a grip on each row; dragged 4 px a row lifts (bg and a
  1 px text-medium edge, no shadow) and follows, the others step aside over
  `duration-short`; let go, every moved row glides to its place over
  `duration-moderate`. Space picks a row up, Up/Down move it, Space puts it
  down, Escape puts it back, Alt+Up/Down moves it at once; `rk_rows_move`.
- **rekta-core: a split** (`rk_split`), as the web's `.rk-split`: sideways
  (760 px wide in a window 481 px tall or more) a list beside its detail with a
  hairline and a 9 px handle that drags the list's width (pane-min each at
  least, four tiles at first, Left/Right by `space-m`, Home/End, a double click
  resets; the app remembers the width through `rk_split_on_resize`); a new
  item's detail slides in. Compact one pane at a time, turning as a place does,
  the detail with a crumb back; Escape goes back too.
- **rekta-core: a tree** (`rk_tree`), as the web's `.rk-tree`: rows at the
  density's height, a level indented by `space-m`, a chevron that turns as a
  branch opens over its height (`duration-short`, closing in 0.7 of it), the
  selection as the accent fill. The web's keys and pointer: Right and Left open
  and close or go in and up, `*` opens the siblings, a letter jumps; a click on
  the chevron or a double click opens or closes. A key that moves the active row
  of a tree or a list now shows the focus ring, as after Tab.

- **rekta-core** (`core/`): Rekta Design as native views in C for
  Windows (Win32), macOS (Cocoa) and Linux (Wayland first, Xlib as the
  fallback), with no dependency
  beyond the system and a vendored `stb_truetype.h`. One header
  (`core/include/rekta.h`) with a clean ABI: opaque reference-counted
  objects, size-prefixed descriptors, 32-bit enums, callbacks with a user
  pointer. Windows, controllers and views composed as in UIKit; column,
  row, tile-grid and overlay layouts; implicit animations named by their
  meaning (`RK_MOTION_STATE`, `REFLOW`, `ENTER`, `LEAVE`, `MORPH`) and
  transitions (`PEEK`, `FLIP`, `CROSSFADE`); the press, the turnstile,
  panoramas with a gliding strip and scrolling sections, pivots, sheets,
  toasts, the title bar, keyboard focus. A software renderer with exact
  anti-aliasing draws the same pixels on every platform; a headless
  platform with a virtual clock runs the tests (73 checks, 14 reference
  images) and the demo's tour. Stage 1 of docs/core.md: text fields,
  menus and tables come next, screen-reader bridges after them.
- rekta-core on Linux speaks Wayland natively: shared-memory buffers with
  damage and frame callbacks, resizes acknowledged and drawn in the same
  commit (the content keeps up with the edge, clicks land where they
  should), its own title bar and resize edges, fractional scales,
  cursor shapes, xkbcommon keys with the seat's repeat. X11 stays as the
  fallback (`RK_PLATFORM=x11`); its resize draws once per burst and keeps
  its shared memory while the window changes size.
- rekta-core shows pictures: `rk_image(path)` and
  `rk_image_from_memory` read PNG and JPEG through a vendored
  `stb_image.h` (v2.30; GIF, HDR and files bigger than 16384 px left
  out). The size comes from the header at once; a background worker
  decodes - and, for a picture over a million pixels, scales - away from
  the UI thread, so a frame never waits for it (a 2400 x 1600 JPEG: the
  slowest frame 3.7 ms). Scaled with a tent filter that averages every
  pixel it shrinks, kept per size; contain or cover; it fades in where a
  placeholder stood. A headless run does the worker's jobs at once, so
  tests draw the same every time.
- rekta-core has a scroll view: `rk_scroll(content)` clips its content
  and scrolls it on the panorama's spring - the wheel until an end, then
  what holds it; the keys a focused view leaves; focus landing inside
  brings it in sight (`rk_view_reveal` does so on request). A 4px bar
  shows while it moves and drags.
- rekta-core has a date field: `rk_date(year, month, day)` shows
  2026-10-13 and holds 20261013. Year, month and day from the keys (Up
  and Down keep the day in its month, digits type it), or a day from
  the month that opens below it, Monday first, today ringed and the
  chosen day in the accent, with the keys of the web's date pickers.
- rekta-core has a select and a combobox: `rk_select(options, count,
  chosen)` opens a menu of its options below it, the chosen one marked
  (Up and Down change it in place, a letter jumps); `rk_field_set_options`
  makes any field a combobox, whose list of matching options floats
  below the input without taking focus - typing, input methods and undo
  stay the field's own.
- rekta-core has sliders and steppers: `rk_slider(min, max, step,
  value)` (a 6px track the accent fills, an ink bar as the thumb, the
  value beside it in tabular figures; a press moves it, a drag follows)
  and `rk_stepper` (minus, the number, plus; a held press repeats, a
  typed number and Enter take it, Escape puts it back). Both snap to
  their step, and the app's values snap too.
- rekta-core has controls that hold a value, as the web's: `rk_check`,
  `rk_choices` (one stop for Tab, the arrows choose), `rk_toggle` (the
  Metro frame, accent fill and ink thumb, the state word beside it) and
  `rk_segments` (the accent block slides to a new choice). One model for
  all: `rk_control_value` / `set_value` / `on_change` / `set_disabled`.
  The demo's settings are segments, with a checkbox, options and a toggle
  for notifications.
- rekta-core sets every script Barlow lacks a glyph for, from another
  face: one the app brings (`rk_app_add_font`) or one the system offers -
  fontconfig on Linux and CoreText on macOS, both opened at run time so
  nothing more is linked (Linux adds `-ldl` for older glibc), and the
  system's fonts on Windows in an order that suits the user's language.
  Faces are mapped, not read, and which face draws a character is
  remembered. Lines break between CJK characters, never before closing
  punctuation or after opening, in labels and fields alike. Arabic,
  Hebrew and the Indic scripts still wait for shaping.
- rekta-core: table cells edit in place. `RK_COLUMN_EDITABLE` and
  `rk_table_on_edit(table, commit, user)`: a double click or F2 opens a
  field as tall as the row over the cell, its old value selected; Enter
  or Tab keeps the value (Tab goes on to the next editable cell, Shift+Tab
  back), Escape puts the old one back, a click elsewhere keeps it.
  commit() returns NULL or why not; the reason shows below the cell,
  which stays open. A view in the focus chain may now take Tab before
  focus moves. The demo's builds table has a note column whose edits go
  through the undo history.
- rekta-core: table columns change width. A column's edge in the head
  drags it (48px at least), a double click gives it its own width back,
  `rk_table_set_column_width` / `rk_table_column_width` let the app set
  and remember it. The head works from the keyboard: Up from the first
  row goes in, Left and Right move, Enter sorts, Ctrl+Left/Right resize,
  Down returns. The pointer takes the shape of what is under it - an
  I-beam over text, the column edge's - on every platform (cursor-shape
  or the theme on Wayland, font cursors on X11, WM_SETCURSOR, NSCursor).
  A list now shows focus on its active row only, not around itself too.
- rekta-core: input methods on every platform. X11 takes the best style
  the input method offers - on the spot (the field shows what is being
  composed), over the spot (the input method draws it at the caret), or
  none - and places its window at the caret between compositions (uim
  drops a composition whose attributes change inside it); LC_CTYPE comes
  from the environment when the app left it at C, as GLFW does, or XIM
  finds no input method. Windows: IMM32, loaded at run time, so the link
  line stays as it was: the composition and candidate windows at the
  caret, the composition shown by the field, the result typed. macOS:
  the view is an NSTextInputClient while a field has focus. Checked on
  X11 against uim-xim with Byeoru (한글, both styles).
- rekta-core: focus everywhere. A toast holds while focus or the
  pointer is on it, Escape dismisses it and focus goes back where it
  came from (over a sheet too, its action is last in the Tab order); with
  nothing focused taking them, Up and Down scroll the section in sight;
  Home and End in a strip of titles. A test walks a window of every
  control and fails on anything that takes input but not focus.
  rekta(3) has a Keys section.
- rekta-core has text areas: `rk_field_set_lines(field, lines)` makes a
  field show several lines, as the web's textarea rows. The text wraps
  at the input's width (at spaces; inside a word only when it is wider
  than the line), Enter breaks a line, Ctrl+Enter submits, Up and Down
  keep their column, Home and End their line, the wheel and the caret
  scroll it. A one-line field is the same code with one line.
- rekta-core has undo, as the web's `undoStack()`: `rk_window_do(window,
  label, run, undo, user, free_user)` runs an action and keeps it (the
  last 50); its toast offers undo, an undo's toast offers redo; Ctrl
  (Cmd) and Z undoes, with Shift or Ctrl+Y redoes; a new action drops
  what was undone. Text fields keep a history of their own, a word at a
  time. The demo's archive button goes through it.
- rekta-core has tables, as the web's `.rk-table`: `rk_table(count, cell,
  user)` is a long list with columns (every `rk_list_` call works on it),
  drawing only the rows in sight; `rk_table_add_column` with a width and
  `RK_COLUMN_NUMERIC` / `RK_COLUMN_SORTABLE`; a head that stays put, a
  click on it sorts ascending then descending through `rk_table_on_sort`
  (the app sorts its rows). The demo has a sortable table of builds.
- rekta-core has menus, as states.md's floating surfaces: `rk_menu`,
  `rk_menu_add` (a verb, a key hint, a handler), separators, group labels,
  items with a square that toggles and keeps the menu open, disabled
  items that say why. `rk_view_set_menu` attaches one: a button opens it,
  any other view on a right click or Shift+F10. Below the opener, above
  where there is no room, never outside the window; a fade and a 4px
  shift over duration-float; the keys and the press outside as on the
  web.
- rekta-core has text fields: `rk_field(label, placeholder)` as the web's
  `.rk-field` (label above, 1px border, a hint or a marked error below),
  one line, edited with the desktop's keys and the pointer, Enter
  submits. The caret blinks for ten seconds after an edit and then stays
  lit, so a field left alone wakes nothing. Copy and paste use the
  system clipboard: CF_UNICODETEXT on Windows, NSPasteboard on macOS, the
  CLIPBOARD selection on X11, wl_data_device on Wayland. On Wayland, dead
  keys and the Compose key compose (xkbcommon), and input methods work
  through text-input-v3: what they compose shows underlined at the caret
  (`RK_EVENT_COMPOSE`), their window follows the caret.
- rekta-core has long lists: `rk_list(count, item, user)` draws only the
  rows in sight and keeps nothing for each item (the callback fills a row
  while it is drawn), scrolls on the glide, and selects the Metro way
  (click shows, Ctrl+click and Shift+click select several, Escape ends it,
  keys as the web's `virtual()`). The demo lists a million builds.
- rekta-core draws a frame's changes as up to eight separate parts and
  presents them together (several damage rectangles in one Wayland
  commit); an app with nothing moving uses no CPU and never wakes. On
  Wayland a frame is drawn straight into the buffer the compositor shows
  (no copy of the frame, no framebuffer of its own): the demo needs 13 MB
  instead of 18.
- rekta-core's API is narrower and says who owns what in one rule:
  constructors are named after what they make (`rk_label`, `rk_button`,
  `rk_tile`, `rk_column`, `rk_row`, `rk_tile_grid`, `rk_rows`, `rk_icon`,
  `rk_progress`, `rk_panorama`, `rk_pivot`, `rk_places`) and return a
  reference; adding, showing and presenting take it over (`rk_add` and
  `rk_insert` return the child). `rk_button(label, on_press, user)` takes
  its handler, `rk_button_set_kind` and `rk_button_set_icon` the rest; a
  button keeps its own width in a column. `rk_window_show(window, root)`
  replaces `rk_window_set_root`; `rk_view_find` finds a button by its label;
  `rk_view_tap` and `rk_window_key` join the testing calls. A man page,
  `core/man/rekta.3`, explains the API by task, with
  `core/examples/hello.c` as its example (built and run by `make test`,
  checked against the copies in the man page and the README). Breaking:
  `rk_view_add`, `rk_view_insert`, `rk_window_set_root` and the
  `rk_*_create` constructors of built-in views are gone.
- rekta-core scrolls and glides smoothly: a wheel step, a section from
  the nav strip or the keyboard travels on a quintic that sets off and
  comes to rest without a jolt, and a new step picks it up mid-way with
  its speed (quick wheel steps go further). The strip's titles slide and
  change size the same way. Frames follow the display: on Wayland the
  compositor's frame callbacks pace them (any refresh rate), elsewhere
  the display's rate (`RK_FPS` overrides).
- `core/demo/demo.c` shows every feature of the core in six sections;
  `flake.nix` builds it on Linux and macOS (`nix run`, `nix develop`,
  `nix flake check` with the tests).
- The token build also writes the C sources of rekta-core
  (`core/src/rk_tokens.h`, `rk_icons.h`, `rk_fonts.h`); `--check` fails
  when they are out of date.

- Inputs, native where they can be: `select.rk-input` with a chevron;
  number, date and time fields in Rekta's frame (the date and time
  pickers' icons from Rekta's set); a combobox that filters as you type
  (`rk-combo`, `Rekta.combobox()`); the Metro toggle switch
  (`rk-toggle`: a square frame, the accent fill, an ink bar as the
  thumb, the state word beside it); a stepper (`rk-stepper`,
  `Rekta.stepper()`); a slider (`rk-range`, with an `<output>`);
  segments (`rk-segments`); a drop zone for files (`rk-drop`,
  `Rekta.drop()`).
- Toasts take a plain action without a countdown (`act`).
- Desktop: tooltips in ink (`data-rk-tip`, after `timing.tip-delay`, at
  once on keyboard focus, never on touch); context menus at the pointer,
  on a long press or Shift+F10 (`Rekta.contextMenu()`); a tree
  (`rk-tree`, `Rekta.tree()`); ordering by a grip, with the pointer or the
  keyboard (`rk-grip`, `Rekta.sortable()`); long lists where only the rows
  in view exist (`rk-virtual`, `Rekta.virtual()`); table columns that size
  and hide (`columns`), and cells that edit in place, with refusals below
  the cell (`edit`).
- Undo history: `Rekta.undoStack()`, Ctrl/Cmd+Z and Ctrl/Cmd+Shift+Z
  after the toast has gone, redo in the undo's toast.
- The live contract: `Rekta.live()` says how fresh a region is, goes
  stale (`timing.stale`) or offline and says so, and holds still while
  you look, with waiting updates behind a button. [docs/live.md](docs/live.md).
- Tokens `timing.tip-delay`, `timing.long-press`, `timing.stale`.
- Data: line and bar charts in ink and grey with one axis, a tooltip
  and crosshair at every point, keys, a live region and a table of the
  numbers (`Rekta.chart()`); meters that take a status colour only when
  their value means one (`rk-meter`, `--rk-v`); an indeterminate
  `rk-progress`; Metro's progress dots for a wait that blocks
  (`rk-dots`, `motion.duration.dots`). [docs/data.md](docs/data.md).
- The app shell: fifty-one icons on a 20px grid (`src/icons/`,
  `dist/rekta.icons.svg` as a sprite, `dist/rekta.icons.json` for other
  platforms, `rk-icon`, `Rekta.icon()`); the title bar of an installed
  app or a native shell (`rk-titlebar`, `Rekta.titlebar()`,
  `data-rk-titlebar`); installing and offline (`Rekta.pwa()`: the service
  worker, a "reload" toast for a waiting version, `install()`).
  `rk:placechange`. [docs/shell.md](docs/shell.md).
- The showcase is an installable app that works offline, with its own
  title bar in a window-controls overlay.
- Data states: `rk-region` with `aria-busy` (a 2px line after
  `busy-delay`, at least `busy-min`), still placeholders `rk-ghost`, and
  `rk-empty` for empty and failed regions. `Rekta.busy()`.
- Pending actions: `Rekta.pending()` keeps the button's width, says what
  it does ("saving…") and runs the line under it.
- Floating surfaces (`rk-float`: bg, 1px edge, no shadow) and menus
  (`rk-menu-item`, `-sep`, `-label`, `-key`), `Rekta.menu()` and
  `Rekta.place()`.
- Tokens `timing.busy-delay`, `timing.busy-min`, `motion.duration.busy`,
  `motion.duration.float`, `layer.float`.
- [docs/states.md](docs/states.md): data states, pending, floating
  surfaces, layers.
- Tables (`rk-table`): kicker head that sticks, sorting with FLIP
  (`rk-sort`, `aria-sort`), numbers in `rk-num`, rows that stack as
  "label value" in a narrow place. `Rekta.table()`.
- Selection, the Metro way: `Rekta.select()` on any list or table body.
  A click selects one item and shows it; Ctrl/Cmd+click adds and starts
  the selection mode, in which every click adds or removes; Shift takes
  a range, Ctrl/Cmd+A all, Escape ends it. A pressed item nudges where
  it is pressed (`motion.transform.row-tilt`, `row-press-scale`); a
  selected one takes the accent fill and a checkmark without a box
  (`rk-check` cell, `rk-mark`). In the mode the app bar carries the
  count and actions (`rk-ab-selection`, `rk-ab-count`, `data-rk-clear`,
  `data-rk-selecting`). `rk:selectionchange`.
- `data-rk-press`: anything can nudge like a tile.
- Split views (`rk-split`): a hairline with an invisible, focusable
  handle sideways (`--rk-split-w`, `size.pane-min`), one pane at a time
  compact. `Rekta.split()`. The shown item is the selected one.
- Items in addresses: `#place/item`, `places().onItem()`, `open()`;
  replaced sideways, a history step compact; deep links put the list
  underneath.
- `tokens.px()` reads a length token in px.
- Go to: `rk-palette` rises from the app bar at every width (Ctrl/Cmd+K,
  "go to"), lists places, sections, items and commands, filters as you
  type. `Rekta.palette()`. Keyboard shortcuts behind `?`; `rk-key`.
- App-bar overflow: controls that do not fit move into "more"
  (`rk-ab-more`, `data-rk-priority`). `Rekta.appbar()`. App-bar controls
  no longer shrink.
- Search and filters: `rk-filters`, `rk-search`, filter buttons that say
  what they keep (`menu(…, { summary })`), `rk-btn--quiet`; parameters
  in the address (`setParams`, `params`). Checkbox items leave a menu
  open.
- Forms: `Rekta.form()` checks late and corrects early, sums up errors in
  `rk-errors` (focused, with links to the fields) and shows server
  refusals at the field or for the form; `rk-optional`.
- Leaving unsaved input asks first: `Rekta.guard()` and `router.guard()`
  (links, back, Escape and the browser's own steps), plus the browser's
  question on closing.
- `Rekta.confirm()`: a sheet with the safe answer first.
  `Rekta.disable()`: a disabled control says why, beside it.
- [docs/forms.md](docs/forms.md): checks, refusals, leaving, dialog
  levels, disabled reasons.
- [docs/collections.md](docs/collections.md).
- The showcase site (`site/`) replaces the Rekta Board demo: every
  section a live example, generated from the tokens at build time
  (`{{token}}`, `<!--@block-->`), with the tokens as a place of their
  own and the components as a table.

- Motion that follows a change of content, each with one meaning
  ([motion.md](docs/motion.md)): `Rekta.reflow()` (a tile resized, the
  grid re-flowing around it), `Rekta.enter()`, `Rekta.leave()` and
  `Rekta.presence()` (rows and tiles that come and go: filters, archive
  and undo, live events, files), `Rekta.tileUpdate()` (a live tile's new
  number peeks in, a new status flips it), `Rekta.sparkTo()` and
  `chart.update()` morphing to new figures, `Rekta.crossfade()` for the
  theme and accent. Sheets leave the way they came; the app bar's
  commands slide when the selection mode starts and ends; checkmarks draw
  in after the nudge; the segments' accent block slides; tree branches
  open to their height; a column put back to its width glides; error
  summaries, a cell's refusal and waiting live updates arrive; progress
  values glide.

### Changed

- **Rekta's view-transition timing applies to its own cross-fades only.**
  `crossfade()` and a theme change through `prefs` mark the root
  `data-rk-crossfade` while they run; an app's own root view transitions
  keep their timing whatever the stylesheet order. The rules that reach
  beyond `.rk-*` are listed in `docs/behaviour.md`.
- `panorama()` gives the panorama `role="region"` when it has no role, and
  warns in the console when it has no accessible name.
- **Checkboxes and radios on the web are Rekta's own** (`.rk-switch`), no
  longer the browser's with `accent-color`: a 20 px square with a 2 px frame,
  checked the accent with an on-accent mark; a radio a ring, checked an accent
  ring around a dot; pressed, the hover fill around it. The same in every
  browser and as rekta-core draws them; the gallery holds them to it with no
  exception left. Forced colours get the system's controls back.
- **rekta-core, as the web**: buttons and segments are 4 px wider each (the
  2 px frame is outside the padding) and their outer corners take `radius-s`, the chosen
  one's frame in the accent; a row's rule is outside its padding, the last row
  has one below as well, and the meta stands on the title's baseline; in a
  compact window (under 760 × 481) tiles stretch to fill the row, at least two
  columns, as they do in a compact place on the web.

- **Reordering takes `duration-moderate`** everywhere, as motion.md
  says (a sortable list used `duration-short`).
- **The accent marks the selection.** Selected rows take the accent fill
  with on-accent text (was the surface fill and a checkbox).
- **The pivot glides by place, not to the edge.** A strip that does not
  fit moves by the active title's place in the order, so the titles
  before it stay in reach; the active title is always whole on screen.
  `rk-pivot-end` is gone: remove it from your markup.
- **A top-level strip is the place's title.** `rk-pivot--nav` takes the
  nav level sideways and the facet level compact, where it sticks. The
  showcase drops its brand line; the strip opens on "rekta". Its
  sections drop the kickers that repeated the strip's titles.
- **rekta-core: a pivot at the top is the place's title.** A pivot with no
  pivot or panorama above it takes the nav level, as `rk-pivot--nav` does on
  the web, and its pages start at the strip's indent. `examples/navigation.c`
  draws one app with its places either way, and `--film` writes its motion
  frame by frame.
- **Heft sets the order, and nothing settles after the turn.** A part's heft
  (0.8 × (area / tile)^0.35, 0.5 to 1.4) sets when it starts as well as how long
  it takes: light parts first, heavy ones later; parts that weigh the same
  follow one another in reading order, half a `duration-stagger` apart (six
  steps at most). Arriving parts land with a
  trace of speed left and are let go in the frame they land. A turning part's
  layer starts on a whole pixel, so it rasterises exactly as the part will rest:
  rekta.js moves a part with a fill or line onto its resting pixels and grows a
  text-only part's box to the pixel without moving its text; the core draws a
  layer's content at its sub-pixel offset and lands the layer on whole pixels
  (fades too), and draws a view directly once its turn moves no corner by a
  quarter pixel. Before, parts shifted by a pixel and sharpened a moment after
  they stopped.
- **The turn swings more and takes half the time.** Leaving parts swing 60°,
  gathering speed, and stay whole for 40% of their time before they fade;
  arriving parts swing in from 70°, quick and without a long creep. Heft runs
  from 0.6 to 1.1 (a tile about 0.85), at most six rows are staggered, and the
  next place starts when the last part is 75% through: a change takes about
  0.8s instead of 1.4s. Tiles that turned in no longer rise a second time
  after the turn (they seemed to settle). Web and core alike.
- **Web: the turn has heft, and a list down the side.** rekta.js turns every
  part of a place by itself, leaving and arriving, in a time that follows its
  size (`--_rk-turn-duration`); a scroller lets its parts turn from its edge.
  `rk-pivot--side` in an `rk-side` lays a pivot's titles down the side, the
  title in sight stepped in by `space-m`; compact, it is the strip again. A
  pivot with `rk-pivot--nav` turns its panels as a place change does.
- **rekta-core: a pivot down the side.** `rk_pivot_set_layout(pivot,
  RK_PIVOT_SIDE)` lays the titles out as a list down the leading side, an equal
  alternative to the strip along the top, as tabs are in a browser. The title
  in sight steps in by `space-m` and darkens over `duration-pivot-size`; Up and
  Down pick a title. A window narrower than 760 px or lower than 481 px keeps
  the titles along the top, as the web's compact class does.
- **rekta-core: a pivot that picks the place turns it.** Its facet turns away
  and the next turns in, as a place change does; a pivot inside a place still
  slides.
- **rekta-core: the turn has heft.** Each part of a place turns by itself (a
  plain layout box no longer turns as one block), in a time that follows its
  size: a line of text in 0.6 of the token's duration, a tile in about the
  token's, a table in up to 1.4. Arriving, the parts turn in one by one, a row
  at a time, instead of the place swinging in whole.

### Fixed

- A pivot without `rekta.js` keeps even gaps between its titles (they were
  32, 155, 56px), and measuring no longer animates the margins.
- The app bar's "more" menu is placed by the stylesheet too, and opens from
  its control's trailing edge with the script; on a phone the status
  banner takes 105px instead of 182px.
- The docs now agree with the code on building in after a turn, the
  arrival window, new data in charts, the next item in the same place, and
  the wide pivot's keys; toasts stay below sheets and there is one control
  size per density, both documented.
- Secondary text inside a status tile or banner (a meta, a facts line, an
  age, a stale value) fell below AA, to 1.9:1 on the light error tile; it
  now takes the primary text colour there.
- The docs agree with the tokens and the code again: the app bar's height,
  the tightest AA pairs, the navigation title levels, the pivot's motion
  tokens, `layer.float`.
- rekta-core, found while building the declared UI: a menu open when its
  window closed kept the window alive (they now close with it); a sheet
  destroyed while it had focus kept the view focused before it; a controller
  destroyed before the controllers it held left them pointing at it; a place
  turning with nothing in it passed `NULL` to `qsort`; a sheet presented again while
  it was leaving lost its new place; a place's turn treated a page and the
  pivot inside it as one block.
- rekta-core: the rasterizer gave the first pixel of a slanted edge crossing
  the clip's left border the wrong coverage, so a rounded corner drawn in part
  did not match the whole; edges are now split at the clip border. Found by
  the new test that holds every partial redraw to a full one.

- rekta-core: a timer overdue when the loop went to sleep (a frame that took
  longer than the timer's gap) put the loop to sleep until the next event;
  it now runs at once.

- rekta-core: a field given its text before it was laid out showed only the
  end of it; it now starts at the start once it has a width.

- A context menu closed on any scroll in the page, so a strip gliding to its
  active title closed one just opened; it now closes when a scroll moves
  what it was opened on.

- rekta-core: removing a view that is already leaving (two toasts in one
  frame) freed it under its own animation; it now keeps leaving.

- The classic script carries `async` functions; `--check` now fails
  when `rekta.global.js` does not parse.
- The sticky section strip no longer shrinks in a place and clips the
  descenders of its titles.

## 0.2.0 — 2026-10-11

The foundation for desktop apps and responsive web apps (including PWAs
on phones), from the UX review of 0.1.

### Breaking

- **One token source, one prefix.** `tokens/rekta.tokens.json` is the
  only source; every web custom property is `--rk-<name>` (was
  `--rekta-color-…` and unprefixed names). `scripts/build-css.mjs` is
  replaced by `scripts/build.mjs`. See the naming table in
  [docs/platforms.md](docs/platforms.md).
- **Sizes in rem.** Type, control, tile and app-bar sizes follow the
  user's text size.
- **State in attributes.** Script state moved from `is-*` classes to
  `data-rk-*` attributes (`data-rk-pressed`, `-turn`, `-phase`,
  `-arriving`, `-fit`, `-state`).
- **Tiles never change data.** A tile opens a place or a sheet, or only
  informs (W17).
- **Build-in only on arrival.** Tiles build in and sparklines draw only
  under `data-rk-arriving`, never on a re-render.

### Added

- `dist/rekta.tokens.json`: resolved, platform-neutral values (logical
  px, ms, hex, Bézier arrays) grouped by theme and density, for other
  desktop frameworks.
- `dist/rekta.js` and `dist/rekta.global.js`: framework-free behaviour
  (press, turnstile primitives, places router, panorama, pivot, toaster,
  banner, sheet, prefs, layered keys). See
  [docs/behaviour.md](docs/behaviour.md).
- Routes and back (D1): an address per place, one history entry per
  place change, one back for the app bar, crumb, Escape and the system;
  cold entries put the parent underneath; focus and scroll restored.
- `rk-crumb` and parent places (K6).
- Window-size classes (D8): sideways vs compact by the place's width
  (container queries) and the viewport height; re-align on change.
- Wheel latch (D7): one wheel gesture, one scroller.
- Keyboard model (D2) and key layering (W14): section jumps, focus on
  headings, Escape innermost first, Ctrl/Cmd+Z only outside text fields.
- Density (W15): 32px controls for a fine pointer, 44px with touch,
  user-selectable; `density.*` tokens.
- Panorama fit (W7), sticky section strip on phones (W8), compact tile
  grid without stretching past two columns (W6).
- Toasts rise from the app bar in the compact class (K7).
- Status vocabulary (W31), selection and floating-surface rules in
  [docs/colour.md](docs/colour.md).
- `--check` also verifies colour-blind separation of statuses and that
  every `--rk-` read in the CSS and JS is defined.
- Rekta Board, the demo site, built on `dist/`, with a Playwright smoke
  test.

### Fixed

- `[hidden]` always hides (W5); reduced motion wins over component
  rules, and status colours no longer leak into neutral children (K2).
- Dragging or selecting text never activates a tile or row (W18);
  touch presses wait for the tap delay (K1); hover tints only where
  hover exists.
- Pivot titles keep a 44px hit area at any scale (K8) and their gaps no
  longer grow with the length of a scaled-down title.
- Fields are 16px on touch (W33); sheets lock the page and close on a
  tap on the shading (K10).
- The app bar's status word is always visible (K16).
- Dark error tile darkened to stay apart from warning under
  deuteranopia.

## 0.1.0

Documentation, tokens and the showcase site.

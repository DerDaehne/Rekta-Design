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
[docs/platforms.md](docs/platforms.md),
[docs/behaviour.md](docs/behaviour.md), [docs/states.md](docs/states.md),
[docs/collections.md](docs/collections.md), [docs/forms.md](docs/forms.md).

## What's in the box

Rekta has no dependencies on any framework. It ships four layers for the
web and a native core, and each one is usable without the next:

| Layer | Files | For |
|---|---|---|
| **Tokens** | `tokens/rekta.tokens.json` (source, [DTCG](https://tr.designtokens.org/format/)), `dist/rekta.tokens.json` (resolved, platform-neutral), `dist/rekta.tokens.css` (`--rk-` custom properties) | Every platform. Another party can implement Rekta for WinUI, Qt, SwiftUI, Compose, Flutter or anything else from these: see [docs/platforms.md](docs/platforms.md). |
| **Icons** | `src/icons/*.svg` (source), `dist/rekta.icons.svg` (a sprite), `dist/rekta.icons.json` (the shapes as data) | Every platform: 51 icons on a 20px grid, stroke 1.6, square ends. See [docs/shell.md](docs/shell.md). |
| **Components** | `dist/rekta.css`, `dist/rekta.fonts.css` with `dist/fonts/` | The web: tiles, rows, tags, buttons, fields and every input, pivots, panoramas, the app bar and title bar, sheets, toasts, the status banner, menus and context menus, tooltips, tables, trees, long lists, splits, charts, meters, progress. Classes `.rk-*`, nothing global. |
| **Native core** | `core/` (`include/rekta.h`, `src/rekta.c`): C99, no dependency beyond the system; `nix run` starts its demo | Desktop apps on Windows, macOS and Linux without web technology: the same tokens, icons and motion, views and controllers composed as in UIKit, a software renderer that draws the same pixels everywhere. See [core/README.md](core/README.md) and [docs/core.md](docs/core.md). |
| **KDE Plasma** | `kde/`, built by `scripts/build-kde.mjs`; `nix build .#rekta-kde` | The Linux desktop itself: global themes Rekta Light and Rekta Dark with the colour schemes, a Plasma Style, the title bar, a Kvantum style for Qt apps, a splash, a wallpaper and an SDDM login screen. See [kde/README.md](kde/README.md). |
| **Behaviour** | `dist/rekta.js` (ES module), `dist/rekta.global.js` (classic script, `window.Rekta`) | The web, with any framework or none: press, the turnstile, routes and back, the panorama's wheel and keyboard, pivots, toasts and an undo history, the banner, live regions, menus, selection, tables, trees, long lists, sorting by hand, forms and inputs, charts, tooltips, the title bar, installing and offline. See [docs/behaviour.md](docs/behaviour.md). |

## Using it on the web

```html
<link rel="stylesheet" href="rekta.tokens.css">
<link rel="stylesheet" href="rekta.fonts.css">
<link rel="stylesheet" href="rekta.css">
<script type="module">
  import * as Rekta from './rekta.js';
  Rekta.start();                                   // prefs, press, sheets, first-paint build-in
  Rekta.places({ home: 'home' });                  // places, addresses, back
  Rekta.panorama(document.querySelector('.rk-panorama'), { pivot: document.querySelector('.rk-pivot') });
</script>
```

Theme, density and motion follow the system and can be overridden on
the root element (`data-theme`, `data-density`, `data-motion`) or with
`Rekta.prefs.set()`. Script state lives in `data-rk-*` attributes, never
in classes, so a framework that owns `className` keeps working. The
whole contract, with every hook, attribute and event, is in
[docs/behaviour.md](docs/behaviour.md).

## Building and checking

```sh
node scripts/build.mjs           # writes dist/ (no dependencies, no network)
node scripts/build.mjs --check   # contrast, colour-blind separation, every --rk- read defined
node scripts/build-site.mjs      # dist/ plus the showcase site in _site/
node scripts/build-site.mjs --serve
PLAYWRIGHT=/path/to/playwright/index.mjs node test/smoke.mjs   # clicks through the showcase in Chromium
```

Node 20 or later. The smoke test needs Playwright, which the repository
does not depend on.

## Showcase site

`site/` is **the showcase**, an installable app that works offline, built in Rekta on `dist/`: one panorama
whose sections each explain a part of the system by being it. Live
tiles measure the page itself, the colour section measures its contrast
in the theme on screen, the components sit in a table chosen the Metro
way, every token has a row in a place of its own with its detail beside
it, and the banner, toasts, menus, the palette, a guarded form and the
prefs all work. Numbers in the copy come from the tokens at build time.
`.github/workflows/pages.yml` builds it and deploys it to GitHub Pages
on every push to `main` (and by hand).

## Status

**0.2: tokens, components and behaviour.** The foundation is in place:
one token source for every platform, the component stylesheet and a
framework-free behaviour script, with routes and back, window-size
classes, density and a keyboard model. Unreleased: everything data-heavy
desktop and web apps need: data states, menus, context menus and
tooltips, tables with Metro selection, columns and editing, trees, long
lists, splits, a command palette, search and filters, forms and every
input, the live contract and an undo history, charts, meters and
progress, an icon set, the title bar of an installed app, and installing
and offline. Next: date ranges, a notification centre, rich text. See
[CHANGELOG.md](CHANGELOG.md).

## Repo

This repo may be made public later; until then, treat it as a draft.

## License

- **Code, design tokens, scripts, CSS and JavaScript** (`tokens/`,
  `src/`, `scripts/`, `site/` source, `test/`, everything generated in
  `dist/`): [MIT](LICENSE).
- **Documentation and images** (this README and `docs/`, including
  `docs/images/`): [CC BY 4.0](LICENSE-docs). Suggested attribution:
  "Rekta Design by DerDaehne, CC BY 4.0".
- **Barlow** (bundled with the site in `site/fonts/barlow/`): SIL Open
  Font License 1.1, see [`site/fonts/barlow/OFL.txt`](site/fonts/barlow/OFL.txt).

See [NOTICE](NOTICE) for the provenance of token values and the typeface,
and what is explicitly *not* included.

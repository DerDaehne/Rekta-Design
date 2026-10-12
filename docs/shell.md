# The app shell: icons, title bar, installing, offline

## Icons

Fifty-one icons on one grid (`src/icons/*.svg`): 20 × 20, a 1.6 stroke,
square ends (`butt` caps, `miter` joins), no fill except where a shape
is solid by nature (the dots of `more` and `grip`, the half disc of
`theme`). They are drawn in the text colour, so they follow every theme
and forced colours.

- **Web:** `dist/rekta.icons.svg` is a sprite of `<symbol id="rk-i-NAME">`.
  Inline it once in the page (or reference the file) and use
  `<svg class="rk-icon"><use href="#rk-i-search"/></svg>`, or
  `Rekta.icon('search')`. `rk-icon--16` is the small size.
- **Other platforms:** `dist/rekta.icons.json` lists each icon's shapes
  (`path`, `circle`) with their attributes, the grid, the stroke width,
  the caps and joins: enough to draw them with any vector API.
- **Use:** an icon goes beside a word. Alone (an app-bar or toolbar
  button), it has an accessible name and a tooltip with that name. New
  icons follow the same grid and stroke; `--check` fails an icon that
  is off the grid.

## Title bar

An installed app has no browser around it, so Rekta draws its own title
bar (`rk-titlebar`): the window's drag area, on the ground, a hairline
below; a back button that follows the router, the app's name, and the
current place's name in light weight. Its controls are not part of the
drag area.

- **Installed PWA:** with `"display_override": ["window-controls-overlay", …]`
  in the manifest, the browser leaves the title bar to the page; the bar
  takes `titlebar-area-*`, so the window's own buttons keep their room.
- **Native shells:** Electron (`titleBarStyle: 'hidden'`,
  `titleBarOverlay`), Tauri (`decorations: false`) or a WebView set
  `data-rk-titlebar` on the root to show it, and `--rk-titlebar-controls`
  to the width their window buttons need.
- In a browser tab it stays hidden: the browser has its own.

## Installing and offline

- **Manifest:** a name, a start URL and scope of `./`, `standalone` with
  the window-controls overlay first, a background and theme colour per
  theme (`theme-color` metas), an SVG icon, 192 and 512px PNGs and a
  maskable one (its content inside the safe zone).
- **Service worker:** caches every file of the app on install, so the
  app opens offline; serves from the cache first. Its version is a hash
  of the files, so every change makes a new one.
- **Updates never surprise.** A new version installs in the background
  and waits; a toast says "A new version is ready" with "reload". It
  takes over only when asked, never under someone mid-task.
- **Offline says so:** the banner's offline state, and live regions turn
  offline (see [live.md](live.md)); what the app saved keeps working.
- **Install** stays the browser's offer: the app keeps the prompt and
  offers it on its own button; where the browser does not offer it, the
  button says why.

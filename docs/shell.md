# The app shell: icons, title bar, installing, offline

## Icons

Fifty-three icons on one grid (`src/icons/*.svg`): 20 × 20, a 1.6 stroke,
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

### Picture icons

Products show icons they did not draw: an app's icon, a site's favicon,
an integration's logo, another program's status icon. They are coloured
and off Rekta's grid, so one rule holds for all of them, on the web and
natively, whatever icon theme or tray protocol they come from.

- **A single-colour variant first.** Where the source offers one (a
  "symbolic" icon), use it and draw it in the text colour like Rekta's
  own (on the web an inline SVG with `currentColor`); it then follows
  every theme and forced colours.
- **Otherwise the picture** (`img.rk-picture`), in fixed sizes: 20px in
  rows, menus, toasts and the app bar, as Rekta's icons;
  `size.tile-icon` (48) and `size.tile-icon-large` (96) only on a launch
  tile ([tiles.md](tiles.md#tile-content-patterns)). Drawn from the
  nearest larger size the source offers, never stretched up.
- **Only on a neutral ground** (`bg`, `surface`, glass), never on a
  status fill: its own colours would fight the fill's meaning. A row
  that carries one and turns selected keeps it as it is, beside the
  checkmark.
- **Always with a word**, as every icon: the app's or sender's name
  beside it (or as its accessible name where the word stands elsewhere,
  on a launch tile).
- **Its colours are never a status.** A red app icon says nothing about
  failure; the status word and square say that, next to it.

## Window buttons

A title bar that draws the window's own buttons (a native app without
the system's frame, rekta-core, a web app in a frameless native shell)
draws minimise, maximise (restore while maximised) and close:

- **Quiet icon buttons** (`.rk-btn.rk-btn--icon.rk-btn--quiet` in
  `.rk-titlebar-win`): the bar's height and `touch-target` wide, at its
  trailing end, each with its name ("minimize", "maximize" or "restore",
  "close") as its accessible name. They take no focus from Tab: the
  system's own keys and window menu do the same.
- **Icons:** `minus`, `maximize` (one square) and `restore` (two), and
  `close`.
- **Hover and press** as any quiet button: the hover tint and the text
  colour. **Close never turns red:** the error colour means "failed",
  and a close button is not a failure.
- **Maximised:** the middle button shows `restore` and says "restore";
  the bar keeps its height and loses no part.
- **Inactive** (another window is the active one): the app's name steps
  back to `text-contrast-high`, the colour of the place's name; the
  hairline and the buttons stay. `titlebar()` writes `data-rk-inactive`
  on the bar; rekta-core does it from the platform's focus.
- **A caption from elsewhere** ("Document — App", a program's own title):
  the part that names the thing (the document) takes the app's name's
  place and weight; the part that names the program takes the place's
  light weight after it. Where the two cannot be told apart, the whole
  caption is the app's name.
- On macOS the system's buttons stay where they are (rekta-core keeps
  their room at the leading edge).

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

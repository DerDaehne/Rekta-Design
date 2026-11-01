# The app shell: icons, title bar, installing, offline, the workspace and the session

## Icons

Sixty-three icons on one grid (`src/icons/*.svg`): 20 × 20, a 1.6 stroke,
square ends (`butt` caps, `miter` joins), no fill except where a shape
is solid by nature (the dots of `more` and `grip`, the half disc of
`theme`). They are drawn in the text colour, so they follow every theme
and forced colours.

- **Devices and media:** `speaker`, `microphone`, `wireless`,
  `bluetooth`, `battery`, `music`, and `previous` and `next` for a
  track (`back` and `forward` are for places, not media). A muted or
  lost state strikes its glyph through corner to corner:
  `speaker-muted`, `microphone-muted`, and `offline` for `wireless`.
  The battery shows no level: the value stands beside it as a word
  ("battery 64%"), so it is read, not guessed from a fill.

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

## The ground of a workspace

A surface that other content lies on as a whole (a desktop behind
windows, a kiosk's screen, a full-screen surface behind places) has the
page's ground: plain `bg`, in both themes, without a picture, a
pattern, a gradient or an imprint of tiles. The ground is where the eye
rests; what lies on it carries the information. A user who picks a
picture of their own for it is the app's to allow, as a setting, never
the default.

## The session

Actions that concern the whole session (lock, switch user, sign out,
restart, power off) are called by one fixed key or control from any
state, and they are a choice between equal ways, not a question: so
neither a banner (a lasting state) nor an ordinary sheet (a short
question).

- **The bar.** A sheet across the full width at the top in the banner's
  geometry: a large light word in the first tile column ("session" or
  the account's name), the actions as buttons behind it, on `surface`
  without a status, the sheet's shading behind it
  (`dialog.rk-sheet.rk-session`, run by `sheet()`). It comes in once as
  the banner does, a fade and 8px over `duration-short`.
- **The actions.** The most frequent (lock) is the primary button, the
  others quiet, each an icon beside its word. Restart and power off ask
  nothing more: calling the bar was the question, and the bar is the
  answer. (Unsaved work in the apps is theirs to guard as they close.)
- **Closing:** Escape, the shading, back or its close button; focus
  returns where it was; reduced motion, at once.

```html
<dialog class="rk-sheet rk-session" closedby="any" aria-labelledby="s-word">
  <form method="dialog">
    <span class="rk-banner-head"><span class="rk-banner-word" id="s-word">session</span></span>
    <p class="rk-actions">
      <button class="rk-btn rk-btn--primary" value="lock"><svg class="rk-icon" aria-hidden="true"><use href="#rk-i-lock"/></svg>lock</button>
      <button class="rk-btn rk-btn--quiet" value="switch"><svg class="rk-icon" aria-hidden="true"><use href="#rk-i-user"/></svg>switch user</button>
      <button class="rk-btn rk-btn--quiet" value="out">sign out</button>
      <button class="rk-btn rk-btn--quiet" value="restart"><svg class="rk-icon" aria-hidden="true"><use href="#rk-i-refresh"/></svg>restart</button>
      <button class="rk-btn rk-btn--quiet" value="off">power off</button>
    </p>
    <button class="rk-btn rk-btn--icon rk-btn--quiet rk-sheet-close" value="cancel" aria-label="close"><svg class="rk-icon" aria-hidden="true"><use href="#rk-i-close"/></svg></button>
  </form>
</dialog>
```

**Leaving the session.** Two movements, each an existing meaning
([motion.md](motion.md)): before the session is locked or left, its
windows step back as they do under a full-screen place, without shading
or blur, over `duration-moderate` · `ease-out` ("you are leaving this
session; it stays as it was"); then the lock or sign-in screen comes
from above, whole, as a sheet does (`duration-short` · `ease-in`, 16px
and a fade), over the whole screen ("who are you?"); its parts do not
build in again. Where switching sessions is a hard cut (a console of
its own, another display), the session being left draws the first and
the screen that follows the second; nothing moves in between. Reduced
motion: both at once.

### The lock and sign-in screen

The screen between a person and their session is a place of its own,
on the ground, with three things in three corners and nothing in the
middle:

- **Time and date**, at the leading top: the time in the display step
  (`.rk-display`, read from across the room), the date below it as a
  fact line.
- **The account and its field**, at the leading bottom: the account's
  name as the screen's one bold title, the password field under it with
  its label, and the button. While the answer is awaited, the dots run
  between field and button at the field's width (a wait that blocks,
  [data.md](data.md#progress)); a refusal is said under the field and
  the input stays ([forms.md](forms.md)). A quiet button below switches
  the account.
- **A status area**, at the trailing bottom: what is playing, the
  number of new messages; figures and words, no controls and no
  content, since no one has signed in yet.

A second screen shows only the time, as an inactive title
(`pivot-inactive`, `text-contrast-high`); the screen the person turns
to takes the display step. An app decides which status it shows there.

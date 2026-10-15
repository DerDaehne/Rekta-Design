#!/usr/bin/env node
// Rekta Design for KDE Plasma 6, from the same tokens as every other platform. Plain Node; rsvg-convert (librsvg) renders
// the wallpapers and the previews.
//
//   node scripts/build-kde.mjs [prefix]   writes [prefix]/share (default dist/kde/share), the layout of a Nix package or /usr:
//     color-schemes/RektaLight.colors, RektaDark.colors        the colours of every KDE app, and of GTK apps through them
//     plasma/desktoptheme/rekta/                               the Plasma Style: panels, popups, task buttons, the controls in
//                                                              them; it takes its colours from the colour scheme
//     aurorae/themes/RektaLight/, RektaDark/                   the window decoration: rekta-core's title bar
//     Kvantum/Rekta/                                           the application style: Rekta and RektaDark for Kvantum
//     plasma/look-and-feel/design.rekta.light/, .dark/         the global themes that set all of the above, with a splash
//     wallpapers/Rekta/                                        one wallpaper, light and dark
//     sddm/themes/rekta/                                       the login screen
//
// Nothing here invents a value: colours, radii and durations are tokens. Two derived numbers are named where they are made:
// the opacity of the ink that equals a hairline colour on the ground (Plasma draws with the scheme's text colour), and the
// press, which is the hover overlay laid twice.

import { copyFileSync, mkdirSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { execFileSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { buildTokensJson, loadIcons, loadTokens } from './build.mjs';

const root = join(dirname(fileURLToPath(import.meta.url)), '..');
const pkg = JSON.parse(readFileSync(join(root, 'package.json'), 'utf8'));
const T = buildTokensJson(loadTokens(), pkg.version);
const ICONS = loadIcons();
const out = join(process.argv[2] || join(root, 'dist', 'kde'), 'share');
const AUTHOR = 'DerDaehne', SITE = 'https://github.com/DerDaehne/Rekta-Design';

const write = (path, text) => { const p = join(out, path); mkdirSync(dirname(p), { recursive: true }); writeFileSync(p, text); };
const copy = (from, path) => { const p = join(out, path); mkdirSync(dirname(p), { recursive: true }); copyFileSync(join(root, from), p); };
const f3 = (n) => +(+n).toFixed(3);
const rgb = (c) => [1, 3, 5].map((i) => parseInt(c.slice(i, i + 2), 16)).join(',');
const alphaOf = (c) => (c.length > 7 ? parseInt(c.slice(7, 9), 16) / 255 : 1);
const solid = (c) => c.slice(0, 7);
const ini = (sections) => Object.entries(sections).map(([name, keys]) =>
  `[${name}]\n` + Object.entries(keys).map(([k, v]) => `${k}=${v}`).join('\n')).join('\n\n') + '\n';

const radius = { s: T.radius['radius-s'], tile: T.radius['radius-tile'] };
const ms = (name) => T.motion[`duration-${name}`];
const bezier = (name) => T.motion[`ease-${name}`];

// The opacity of the text colour that equals a colour on the ground, averaged over both themes: Plasma draws with the colour
// scheme's text, so a hairline (text-contrast-low) is ink at this opacity.
function inkAlpha(name) {
  const per = ['light', 'dark'].map((t) => {
    const c = T.themes[t], ch = (x, i) => parseInt(x.slice(1 + 2 * i, 3 + 2 * i), 16);
    return [0, 1, 2].reduce((s, i) => s + (ch(c[name], i) - ch(c.bg, i)) / (ch(c['text-primary'], i) - ch(c.bg, i)), 0) / 3;
  });
  return f3((per[0] + per[1]) / 2);
}
const LOW = inkAlpha('text-contrast-low'), MEDIUM = inkAlpha('text-contrast-medium'), HIGH = inkAlpha('text-contrast-high');
const SURFACE = inkAlpha('surface');
const hover = T.themes.dark.hover;                              // one translucent grey for both themes
const HOVER = { color: solid(hover), op: f3(alphaOf(hover)) };
const PRESS = { color: solid(hover), op: f3(1 - (1 - alphaOf(hover)) ** 2) };

// ------------------------------------------------------------------------------------------------------------- geometry

// A frame is cut into nine cells (corners, sides, centre) that FrameSvg and Kvantum stretch to a widget. Each cell is drawn
// as its own element, so the shapes are clipped to the cells here: polygons, corners as arcs of 15° steps.
function roundRect(x0, y0, x1, y1, r) {
  if (x1 <= x0 || y1 <= y0) return [];
  r = Math.max(0, Math.min(r, (x1 - x0) / 2, (y1 - y0) / 2));
  if (!r) return [[x0, y0], [x1, y0], [x1, y1], [x0, y1]];
  const p = [];
  const arc = (cx, cy, from) => { for (let i = 0; i <= 6; i++) { const a = (from + 15 * i) * Math.PI / 180; p.push([cx + r * Math.cos(a), cy + r * Math.sin(a)]); } };
  arc(x1 - r, y0 + r, -90); arc(x1 - r, y1 - r, 0); arc(x0 + r, y1 - r, 90); arc(x0 + r, y0 + r, 180);
  return p;
}
function clip(poly, [x0, y0, x1, y1]) {
  const at = (a, b, axis, v) => { const t = (v - a[axis]) / (b[axis] - a[axis]); return axis ? [a[0] + t * (b[0] - a[0]), v] : [v, a[1] + t * (b[1] - a[1])]; };
  const edges = [[(p) => p[0] >= x0, 0, x0], [(p) => p[0] <= x1, 0, x1], [(p) => p[1] >= y0, 1, y0], [(p) => p[1] <= y1, 1, y1]];
  let res = poly;
  for (const [inside, axis, v] of edges) {
    const src = res; res = [];
    src.forEach((b, i) => {
      const a = src[(i + src.length - 1) % src.length];
      if (inside(b)) { if (!inside(a)) res.push(at(a, b, axis, v)); res.push(b); } else if (inside(a)) res.push(at(a, b, axis, v));
    });
  }
  const area = res.reduce((s, [x, y], i) => { const [u, w] = res[(i + 1) % res.length]; return s + x * w - u * y; }, 0);
  return Math.abs(area) > 1e-6 ? res : [];
}
const pathOf = (polys, dx, dy) => polys.filter((p) => p.length > 2)
  .map((p) => 'M' + p.map(([x, y]) => `${f3(x + dx)} ${f3(y + dy)}`).join('L') + 'Z').join('');

// A paint is a colour-scheme class (Plasma recolours it) or a fixed colour, each with an opacity.
const paint = (p) => p.cls
  ? ` class="${p.cls}" style="fill:currentColor;fill-opacity:${p.op ?? 1}"`
  : ` style="fill:${p.color};fill-opacity:${p.op ?? 1}"`;
const strokePaint = (p, w) => p.cls
  ? ` class="${p.cls}" style="fill:none;stroke:currentColor;stroke-opacity:${p.op ?? 1};stroke-width:${w};stroke-linecap:butt;stroke-linejoin:miter"`
  : ` style="fill:none;stroke:${p.color};stroke-opacity:${p.op ?? 1};stroke-width:${w};stroke-linecap:butt;stroke-linejoin:miter"`;
const box = (x, y, w, h) => `<rect x="${f3(x)}" y="${f3(y)}" width="${f3(w)}" height="${f3(h)}" style="fill:#000;fill-opacity:0"/>`;

// Layers, from the outer edge in: {fill, d, r} fills the shape d px in with corner radius r; {ring, d, w, r} strokes w px
// along it; {side, d, w, paint} draws a line along one side; {rect: (W, H) => [x0, y0, x1, y1], paint} anywhere.
function layerPolys(l, W, H) {
  const d = l.d || 0, r = l.r || 0, w = l.w || 1;
  if (l.fill) return [roundRect(d, d, W - d, H - d, r)];
  if (l.ring) return [roundRect(d, d, W - d, H - d, r), roundRect(d + w, d + w, W - d - w, H - d - w, Math.max(r - w, 0))];
  if (l.side) return [roundRect(...{ top: [0, d, W, d + w], bottom: [0, H - d - w, W, H - d], left: [d, 0, d + w, H], right: [W - d - w, 0, W - d, H] }[l.side], 0)];
  return [roundRect(...l.rect(W, H), 0)];
}

// One frame at (x, y): border sizes b = {t, r, b, l}, centre c; name(cell) is each cell's id, or null to leave it out.
function frame(name, x, y, b, layers, c = 8) {
  const W = b.l + c + b.r, H = b.t + c + b.b, inner = Math.min(b.t, b.r, b.b, b.l);
  for (const l of layers) if (l.r && (l.d || 0) + l.r > inner) throw new Error(`${name('center')}: a radius of ${l.r} at ${l.d || 0} px in needs borders of ${(l.d || 0) + l.r}`);
  const xs = [0, b.l, b.l + c, W], ys = [0, b.t, b.t + c, H];
  const cells = [['topleft', 'top', 'topright'], ['left', 'center', 'right'], ['bottomleft', 'bottom', 'bottomright']];
  let svg = '';
  for (let row = 0; row < 3; row++) for (let col = 0; col < 3; col++) {
    const id = name(cells[row][col]), cell = [xs[col], ys[row], xs[col + 1], ys[row + 1]];
    if (!id || cell[2] <= cell[0] || cell[3] <= cell[1]) continue;
    svg += `<g id="${id}">${box(x + cell[0], y + cell[1], cell[2] - cell[0], cell[3] - cell[1])}`;
    for (const l of layers) {
      const d = pathOf(layerPolys(l, W, H).map((p) => clip(p, cell)), x, y);
      if (d) svg += `<path d="${d}" fill-rule="evenodd"${paint(l.fill || l.ring || l.paint)}/>`;
    }
    svg += '</g>\n';
  }
  return { svg, w: W, h: H };
}
const all = (n) => ({ t: n, r: n, b: n, l: n });

// An icon's path data moved and scaled: s times, then by (dx, dy). Circles become two arcs.
function transformPath(d, s, dx, dy) {
  const tok = d.match(/[a-zA-Z]|-?(?:\d*\.\d+|\d+\.?\d*)(?:e[-+]?\d+)?/g), arity = { m: 2, l: 2, h: 1, v: 1, c: 6, s: 4, q: 4, t: 2, a: 7, z: 0 };
  let res = '', cmd = '', i = 0;
  while (i < tok.length) {
    if (/[a-zA-Z]/.test(tok[i])) { cmd = tok[i++]; res += cmd; if (cmd.toLowerCase() === 'z') continue; } else res += ' ';
    const lc = cmd.toLowerCase(), abs = cmd !== lc, v = Array.from({ length: arity[lc] }, () => +tok[i++]);
    const X = (n) => (abs ? n * s + dx : n * s), Y = (n) => (abs ? n * s + dy : n * s);
    const m = lc === 'h' ? [X(v[0])] : lc === 'v' ? [Y(v[0])]
      : lc === 'a' ? [v[0] * s, v[1] * s, v[2], v[3], v[4], X(v[5]), Y(v[6])]
        : v.map((n, k) => (k % 2 ? Y(n) : X(n)));
    res += m.map(f3).join(' ');
  }
  return res;
}
function iconPaths(name, s, dx, dy) {
  const icon = ICONS[name];
  if (!icon) throw new Error(`no icon ${name}`);
  return icon.shapes.map(({ tag, attrs }) => {
    if (tag === 'path') return transformPath(attrs.d, s, dx, dy);
    if (tag === 'circle') { const cx = +attrs.cx * s + dx, cy = +attrs.cy * s + dy, r = +attrs.r * s; return `M${f3(cx - r)} ${f3(cy)}a${f3(r)} ${f3(r)} 0 1 0 ${f3(2 * r)} 0a${f3(r)} ${f3(r)} 0 1 0 ${f3(-2 * r)} 0`; }
    throw new Error(`icon ${name}: no support for <${tag}>`);
  });
}
// The icon drawn into a w × h element at (x, y), size px square in its middle, its stroke scaled with it.
const icon = (name, x, y, w, h, size, p, stroke = 1.6) => iconPaths(name, size / 20, x + (w - size) / 2, y + (h - size) / 2)
  .map((d) => `<path d="${d}"${strokePaint(p, f3(stroke * size / 20))}/>`).join('');

// An SVG sheet that stacks elements top to bottom.
class Sheet {
  constructor(style = '') { this.body = ''; this.y = 0; this.w = 0; this.style = style; }
  place(w, h) { const y = this.y; this.y += Math.ceil(h) + 2; this.w = Math.max(this.w, Math.ceil(w)); return y; }
  frame(name, b, layers, c) { const f = frame(name, 0, this.y, b, layers, c); this.place(f.w, f.h); this.body += f.svg; return this; }
  // A free element: draw(x, y) returns its contents; the element is exactly w × h.
  el(id, w, h, draw = () => '') { const y = this.place(w, h); this.body += `<g id="${id}">${box(0, y, w, h)}${draw(0, y)}</g>\n`; return this; }
  // A hint: never painted, only measured.
  hint(id, w, h) { const y = this.place(w, h); this.body += `<rect id="${id}" x="0" y="${y}" width="${f3(Math.max(w, 0.001))}" height="${f3(Math.max(h, 0.001))}" style="fill:#f0f"/>\n`; return this; }
  margins(prefix, m) {
    const p = prefix ? `${prefix}-` : '', [t, r, b, l] = typeof m === 'number' ? [m, m, m, m] : [m.t, m.r, m.b, m.l];
    return this.hint(`${p}hint-top-margin`, 2, t).hint(`${p}hint-bottom-margin`, 2, b).hint(`${p}hint-left-margin`, l, 2).hint(`${p}hint-right-margin`, r, 2);
  }
  doc() { return `<svg xmlns="http://www.w3.org/2000/svg" width="${this.w}" height="${this.y}" viewBox="0 0 ${this.w} ${this.y}">\n${this.style}${this.body}</svg>\n`; }
}

// ------------------------------------------------------------------------------------------------------- colour schemes

// Selection is a grey (text-contrast-low) under the ink, not the ink itself: a full-ink selection reads too hard on a
// desktop. Checked controls and filled parts keep the ink (see Kvantum).
function colorScheme(id, label, c, comp) {
  const set = (bg, alt, x = c) => ({
    BackgroundNormal: rgb(bg), BackgroundAlternate: rgb(alt), ForegroundNormal: rgb(x['text-primary']),
    ForegroundInactive: rgb(x['text-contrast-high']), ForegroundActive: rgb(x.focus), ForegroundLink: rgb(x.focus),
    ForegroundVisited: rgb(x['text-contrast-high']), ForegroundNegative: rgb(x['status-error-full']),
    ForegroundNeutral: rgb(x['status-warning-full']), ForegroundPositive: rgb(x['status-success-full']),
    DecorationFocus: rgb(x.focus), DecorationHover: rgb(x['text-contrast-medium']),
  });
  return ini({
    'ColorEffects:Disabled': { Color: '56,56,56', ColorAmount: 0, ColorEffect: 0, ContrastAmount: 0.65, ContrastEffect: 1, IntensityAmount: 0.1, IntensityEffect: 2 },
    'ColorEffects:Inactive': { ChangeSelectionColor: true, Color: '112,111,110', ColorAmount: 0.025, ColorEffect: 2, ContrastAmount: 0.1, ContrastEffect: 2, Enable: false, IntensityAmount: 0, IntensityEffect: 0 },
    'Colors:Button': set(c.surface, c['surface-2']),
    'Colors:Complementary': set(comp.bg, comp.surface, comp),
    'Colors:Header': set(c.bg, c.surface),
    'Colors:Header][Inactive': set(c.bg, c.surface),
    'Colors:Selection': { ...set(c['text-contrast-low'], c['surface-2']), ForegroundInactive: rgb(c['text-primary']) },
    'Colors:Tooltip': set(c.bg, c.surface),
    'Colors:View': set(c.bg, c.surface),
    'Colors:Window': set(c.bg, c.surface),
    General: { ColorScheme: id, Name: label, accentActiveTitlebar: false, shadeSortColumn: false },
    KDE: { contrast: 4 },
    WM: {
      activeBackground: rgb(c.bg), activeBlend: rgb(c['text-primary']), activeForeground: rgb(c['text-primary']),
      inactiveBackground: rgb(c.bg), inactiveBlend: rgb(c['text-contrast-medium']), inactiveForeground: rgb(c['text-contrast-medium']),
    },
  });
}

// --------------------------------------------------------------------------------------------------------- Plasma Style

// Default colours only; Plasma replaces this sheet with the colour scheme's.
const SCHEME_STYLE = '<style type="text/css" id="current-color-scheme">.ColorScheme-Text{color:#232629}.ColorScheme-Background{color:#eff0f1}'
  + '.ColorScheme-Highlight{color:#3daee9}.ColorScheme-ViewText{color:#232629}.ColorScheme-ViewBackground{color:#fcfcfc}'
  + '.ColorScheme-ButtonText{color:#232629}.ColorScheme-ButtonBackground{color:#eff0f1}.ColorScheme-ButtonFocus{color:#3daee9}'
  + '.ColorScheme-NeutralText{color:#f67400}.ColorScheme-PositiveText{color:#27ae60}.ColorScheme-NegativeText{color:#da4453}</style>\n';
const ink = (op = 1) => ({ cls: 'ColorScheme-Text', op });
const ground = (op = 1) => ({ cls: 'ColorScheme-Background', op });
const focusRing = { cls: 'ColorScheme-ButtonFocus', op: 1 };
const named = (prefix) => (cell) => (prefix ? `${prefix}-${cell}` : cell);
const sheet = () => new Sheet(SCHEME_STYLE);

// Popups, menus, tooltips and desktop widgets: a floating surface, the ground with a 1px edge and radius-s, no shadow.
function floating(shadow = false) {
  const s = sheet().frame(named(''), all(radius.s), [{ fill: ground(), r: radius.s }, { ring: ink(MEDIUM), r: radius.s }])
    .margins('', radius.s)
    .frame(named('mask'), all(radius.s), [{ fill: { color: '#000' }, r: radius.s }]);
  if (shadow) s.frame(named('shadow'), all(1), []).margins('shadow', 0);  // asked for by name; nothing to draw
  return s.doc();
}
// Panels: flush to the screen edge, a hairline on the sides that face the desktop. Glass where KWin blurs behind it.
const panel = (op) => sheet().frame(named(''), all(1), [{ fill: ground(op) }, { ring: ink(LOW) }]).margins('', 4)
  .frame(named('mask'), all(1), [{ fill: { color: '#000' } }]).doc();

function plasmaButton() {
  const r = radius.s, s = sheet();
  // normal is also the box of a checkbox (Plasma draws its check over it).
  s.frame(named('normal'), all(r), [{ ring: ink(), w: 2, r }]).margins('normal', 6);
  s.frame(named('pressed'), all(r), [{ fill: PRESS, r }, { ring: ink(), w: 2, r }]).margins('pressed', 6);
  s.frame(named('hover'), all(r), [{ fill: HOVER, r }]).margins('hover', 0);
  // Focus: a 2px ring 2px outside the button, as outline-offset does on the web.
  s.frame(named('focus'), all(2 * r), [{ ring: focusRing, w: 2, r: 2 * r }]).margins('focus', r);
  s.frame(named('toolbutton-hover'), all(r), [{ fill: HOVER, r }]).margins('toolbutton-hover', 4);
  s.frame(named('toolbutton-pressed'), all(r), [{ fill: PRESS, r }]).margins('toolbutton-pressed', 4);
  s.frame(named('toolbutton-focus'), all(r + 2), [{ ring: focusRing, w: 2, r: r + 2 }]).margins('toolbutton-focus', 2);
  return s.doc();
}

function plasmaLineEdit() {
  const r = radius.s, s = sheet();
  s.el('hint-focus-over-base', 2, 2);
  s.frame(named('base'), all(r), [{ fill: { cls: 'ColorScheme-ViewBackground' }, r }, { ring: { cls: 'ColorScheme-ViewText', op: MEDIUM }, r }]).margins('base', 6);
  s.frame(named('hover'), all(r), [{ ring: { cls: 'ColorScheme-ViewText', op: HIGH }, r }]).margins('hover', 0);
  s.frame(named('focus'), all(r), [{ ring: { cls: 'ColorScheme-ViewText' }, r }]).margins('focus', 0);
  // Keyboard focus: the ink edge and a 2px focus ring 2px outside it.
  s.frame(named('focusframe'), all(2 * r), [{ ring: focusRing, w: 2, r: 2 * r }, { ring: { cls: 'ColorScheme-ViewText' }, d: r, r }]).margins('focusframe', r);
  return s.doc();
}

// Rows in lists: the hover overlay under the pointer, the selection grey when chosen. Square, as rows are.
function viewItem() {
  const s = sheet(), hl = { cls: 'ColorScheme-Highlight' };
  s.frame(named('normal'), all(5), []);
  s.frame(named('hover'), all(5), [{ fill: HOVER }]);
  s.frame(named('selected'), all(5), [{ fill: hl }]);
  s.frame(named('selected+hover'), all(5), [{ fill: hl }, { fill: HOVER }]);
  return s.doc();
}
function listItem() {
  const s = sheet(), hl = { cls: 'ColorScheme-Highlight' };
  s.frame(named('normal'), all(2), []).margins('normal', 6);
  s.frame(named('pressed'), all(2), [{ fill: hl }]).margins('pressed', 6);
  s.frame(named('hover'), all(2), [{ fill: HOVER }]);
  s.frame(named('section'), all(2), [{ side: 'bottom', paint: ink(LOW) }]).margins('section', 6);
  s.el('separator', 40, 1, (x, y) => `<rect x="${x}" y="${y}" width="40" height="1"${paint(ink(LOW))}/>`);
  return s.doc();
}

// Task buttons: a 2px line on the screen-edge side says "running", the ink line and the surface say "this one is active".
function tasks() {
  const s = sheet(), edges = { '': 'bottom', north: 'top', west: 'left', east: 'right' };
  const states = {
    normal: (e) => [{ side: e, w: 2, paint: ink(MEDIUM) }],
    minimized: (e) => [{ side: e, w: 2, paint: ink(LOW) }],
    focus: (e) => [{ fill: ink(SURFACE) }, { side: e, w: 2, paint: ink() }],
    attention: (e) => [{ fill: { cls: 'ColorScheme-NeutralText', op: LOW } }, { side: e, w: 2, paint: { cls: 'ColorScheme-NeutralText' } }],
    progress: () => [{ fill: { cls: 'ColorScheme-ButtonFocus', op: LOW } }],
    hover: (e) => [{ fill: HOVER }, { side: e, w: 2, paint: ink(MEDIUM) }],
  };
  for (const st of ['normal', 'minimized', 'focus', 'attention']) states[`${st}-hover`] = (e) => [{ fill: HOVER }, ...states[st](e)];
  for (const [dir, edge] of Object.entries(edges)) {
    const b = dir === 'west' || dir === 'east' ? { t: 4, b: 4, l: 3, r: 3 } : { t: 3, b: 3, l: 4, r: 4 };
    for (const [st, layers] of Object.entries(states)) {
      const p = dir ? `${dir}-${st}` : st;
      s.frame(named(p), b, layers(edge)).margins(p, 4);
    }
  }
  for (const [dir, ic] of [['top', 'chevron-up'], ['bottom', 'chevron-down'], ['left', 'chevron-left'], ['right', 'chevron-right']])
    s.el(`group-expander-${dir}`, 11, 11, (x, y) => icon(ic, x, y, 11, 11, 11, ink()));
  return s.doc();
}

// Tabs, like the pivot: the active one is underlined in ink, on the side that faces its content.
function tabBar() {
  const s = sheet();
  for (const [dir, side, m] of [['north', 'bottom', { t: 10, b: 10, l: 6, r: 6 }], ['south', 'top', { t: 10, b: 10, l: 6, r: 6 }],
    ['west', 'right', { t: 6, b: 6, l: 4, r: 8 }], ['east', 'left', { t: 6, b: 6, l: 8, r: 4 }]])
    s.frame(named(`${dir}-active-tab`), all(4), [{ side, w: 2, paint: ink() }]).margins(`${dir}-active-tab`, m);
  return s.doc();
}

// The header and footer of a popup: no fill, a hairline towards the content.
const plasmoidHeading = () => sheet().el('hint-stretch-borders', 2, 2)
  .frame(named('header'), all(6), [{ side: 'bottom', paint: ink(LOW) }])
  .frame(named('footer'), all(6), [{ side: 'top', paint: ink(LOW) }]).margins('', 6).doc();

// Scrollbars: a square bar, no groove; it darkens under the pointer. Metrics as Breeze's, so nothing reflows.
function scrollBar() {
  const s = sheet().el('hint-scrollbar-size', 6, 6), inset = (p) => s.hint(`${p}-hint-top-inset`, 3, 6).hint(`${p}-hint-bottom-inset`, 3, 6)
    .hint(`${p}-hint-left-inset`, 6, 3).hint(`${p}-hint-right-inset`, 6, 3);
  s.frame(named('slider'), all(3), [{ fill: ink(MEDIUM) }]); inset('slider');
  s.frame(named('mouseover-slider'), all(3), [{ fill: ink(HIGH) }]); inset('mouseover-slider');
  for (const p of ['background-vertical', 'background-horizontal']) { s.frame(named(p), all(3), []); inset(p); }
  return s.doc();
}

// The slider of the web: a 6px track, filled in ink up to the value, and an ink bar as the thumb.
function slider() {
  const s = sheet().el('hint-handle-size', 10, 24);
  s.frame(named('groove'), all(3), [{ fill: ink(LOW) }]);
  s.frame(named('groove-highlight'), all(3), [{ fill: ink() }]);
  for (const [o, w, h] of [['horizontal', 10, 24], ['vertical', 24, 10]]) {
    s.el(`${o}-slider-handle`, w, h, (x, y) => `<rect x="${x}" y="${y}" width="${w}" height="${h}"${paint(ink())}/>`);
    s.el(`${o}-slider-hover`, w, h);
    s.el(`${o}-slider-focus`, w + 8, h + 8, (x, y) => ringRect(x, y, w + 8, h + 8, 2, focusRing));
  }
  return s.doc();
}
const ringRect = (x, y, w, h, sw, p) => `<path d="M${x} ${y}h${w}v${h}h${-w}zM${x + sw} ${y + sw}v${h - 2 * sw}h${w - 2 * sw}v${-(h - 2 * sw)}z" fill-rule="evenodd"${paint(p)}/>`;

const progress = () => sheet().el('hint-bar-size', 4, 4)
  .frame(named('bar-inactive'), all(2), [{ fill: ink(LOW) }])
  .frame(named('bar-active'), all(2), [{ fill: ink() }]).doc();

// The check of a checkbox, drawn over the button's normal frame: the box filled in ink, the check cut out of it.
const checkmarks = () => sheet()
  .el('checkbox', 16, 16, (x, y) => `<path d="${pathOf([roundRect(x, y, x + 16, y + 16, radius.s)], 0, 0)}"${paint(ink())}/>${icon('check', x, y, 16, 16, 16, ground(), 2.5)}`)
  .el('radiobutton', 16, 16, (x, y) => `<circle cx="${x + 8}" cy="${y + 8}" r="4"${paint(ink())}/>`).doc();

function radioButton() {
  const s = sheet().el('hint-size', 16, 16), disc = (x, y, r, p) => `<circle cx="${x + 8}" cy="${y + 8}" r="${r}"${paint(p)}/>`;
  s.el('normal', 16, 16, (x, y) => `<path d="M${x} ${y + 8}a8 8 0 1 0 16 0a8 8 0 1 0 -16 0zM${x + 2} ${y + 8}a6 6 0 1 0 12 0a6 6 0 1 0 -12 0z" fill-rule="evenodd"${paint(ink())}/>`);
  s.el('checked', 16, 16, (x, y) => disc(x, y, 4, ink()));
  s.el('hover', 16, 16, (x, y) => disc(x, y, 8, HOVER));
  s.el('symbol', 6, 6, (x, y) => `<circle cx="${x + 3}" cy="${y + 3}" r="3"${paint(ink())}/>`);
  s.el('focus', 20, 20, (x, y) => `<path d="M${x} ${y + 10}a10 10 0 1 0 20 0a10 10 0 1 0 -20 0zM${x + 2} ${y + 10}a8 8 0 1 0 16 0a8 8 0 1 0 -16 0z" fill-rule="evenodd"${paint(focusRing)}/>`);
  return s.doc();
}

// A switch as Metro drew it: a 2px track, filled in ink when on, a solid bar as the thumb (hollow when on or under the pointer).
function switchSvg() {
  const s = sheet().el('hint-bar-size', 36, 18).el('hint-stretch-borders', 2, 2);
  s.frame(named('inactive'), all(2), [{ ring: ink(), w: 2 }]);
  s.frame(named('active'), all(2), [{ fill: ink() }]);
  const bar = (x, y) => `<rect x="${x}" y="${y}" width="8" height="18"${paint(ink())}/>`;
  const hollow = (x, y) => `<rect x="${x}" y="${y}" width="8" height="18"${paint(ground())}/>${ringRect(x, y, 8, 18, 2, ink())}`;
  s.el('handle', 8, 18, bar);
  for (const id of ['handle-active', 'handle-hover', 'handle-pressed']) s.el(id, 8, 18, hollow);
  s.el('handle-focus', 16, 26, (x, y) => ringRect(x, y, 16, 26, 2, focusRing));
  return s.doc();
}

const menuBarItem = () => sheet().frame(named('normal'), all(6), []).margins('normal', 8)
  .frame(named('hover'), all(6), [{ fill: HOVER }]).margins('hover', 8)
  .frame(named('pressed'), all(6), [{ fill: PRESS }]).margins('pressed', 8).doc();
// Frames: content over chrome, so a plain frame draws nothing; a sunken one keeps a hairline.
const frames = () => sheet().frame(named('plain'), all(1), []).margins('plain', 6)
  .frame(named('sunken'), all(1), [{ ring: ink(LOW) }]).margins('sunken', 6)
  .frame(named('raised'), all(1), []).margins('raised', 6).doc();
const line = () => sheet().el('horizontal-line', 1, 1, (x, y) => `<rect x="${x}" y="${y}" width="1" height="1"${paint(ink(LOW))}/>`)
  .el('vertical-line', 1, 1, (x, y) => `<rect x="${x}" y="${y}" width="1" height="1"${paint(ink(LOW))}/>`).doc();

function plasmaStyle() {
  const dir = 'plasma/desktoptheme/rekta', glass = f3(alphaOf(T.themes.dark.glass));
  write(`${dir}/metadata.json`, JSON.stringify({
    KPlugin: { Authors: [{ Name: AUTHOR }], Category: '', Description: 'Rekta Design: straight lines, hairlines instead of shadows, the colours of the colour scheme', EnabledByDefault: true, Id: 'rekta', License: 'MIT', Name: 'Rekta', Version: pkg.version, Website: SITE },
    'X-Plasma-API': '5.0',
  }, null, 2) + '\n');
  write(`${dir}/plasmarc`, ini({ AdaptiveTransparency: { enabled: true } }));
  const files = {
    'dialogs/background': floating(), 'widgets/tooltip': floating(true), 'widgets/background': floating(),
    'widgets/panel-background': panel(glass), 'widgets/button': plasmaButton(), 'widgets/lineedit': plasmaLineEdit(),
    'widgets/viewitem': viewItem(), 'widgets/listitem': listItem(), 'widgets/tasks': tasks(), 'widgets/tabbar': tabBar(),
    'widgets/plasmoidheading': plasmoidHeading(), 'widgets/scrollbar': scrollBar(), 'widgets/slider': slider(),
    'widgets/bar_meter_horizontal': progress(), 'widgets/checkmarks': checkmarks(), 'widgets/radiobutton': radioButton(),
    'widgets/switch': switchSvg(), 'widgets/menubaritem': menuBarItem(), 'widgets/frame': frames(), 'widgets/line': line(),
  };
  for (const [f, svg] of Object.entries(files)) write(`${dir}/${f}.svg`, svg);
  // Without blur (translucent/), without compositing (opaque/) and for an opaque panel (solid/): never see-through.
  for (const v of ['solid', 'translucent', 'opaque']) {
    write(`${dir}/${v}/dialogs/background.svg`, floating());
    write(`${dir}/${v}/widgets/tooltip.svg`, floating(true));
    write(`${dir}/${v}/widgets/panel-background.svg`, panel(1));
    if (v !== 'opaque') write(`${dir}/${v}/widgets/background.svg`, floating());
  }
}

// ------------------------------------------------------------------------------------------------------ window decoration

// rekta-core's title bar: 36px on the ground, a hairline below, the title on the left, 46px buttons with 20px icons on the
// right. The window keeps a 1px edge: medium while active, low while not; no shadow.
const TITLE = 36, BUTTON = 46;
function aurorae(id, c, label) {
  const dir = `aurorae/themes/${id}`, col = (x) => ({ color: solid(x) });
  const deco = (edge) => [{ fill: col(c.bg) }, { rect: (W) => [0, TITLE - 1, W, TITLE], paint: col(c['text-contrast-low']) },
    ...['top', 'left', 'right', 'bottom'].map((side) => ({ side, paint: col(edge) }))];
  const s = new Sheet()
    .frame(named('decoration'), { t: TITLE, r: 1, b: 1, l: 1 }, deco(c['text-contrast-medium']))
    .frame(named('decoration-inactive'), { t: TITLE, r: 1, b: 1, l: 1 }, deco(c['text-contrast-low']))
    .frame(named('mask'), { t: TITLE, r: 1, b: 1, l: 1 }, [{ fill: { color: '#000' } }]);
  write(`${dir}/decoration.svg`, s.doc());
  const h = TITLE - 2, states = {
    active: [c['text-primary']], inactive: [c['text-contrast-medium']], hover: [c['text-primary'], HOVER], pressed: [c['text-primary'], PRESS],
    deactivated: [c.disabled], 'hover-inactive': [c['text-primary'], HOVER], 'pressed-inactive': [c['text-primary'], PRESS], 'deactivated-inactive': [c.disabled],
  };
  for (const [file, ic] of Object.entries({ close: 'close', maximize: 'window', restore: 'window', minimize: 'minus', alldesktops: 'pin', keepabove: 'up', keepbelow: 'down', shade: 'chevron-up', help: 'info' })) {
    const b = new Sheet();
    for (const [st, [fg, bg]] of Object.entries(states))
      b.el(`${st}-center`, BUTTON, h, (x, y) => (bg ? `<rect x="${x}" y="${y}" width="${BUTTON}" height="${h}"${paint(bg)}/>` : '') + icon(ic, x, y, BUTTON, h, 20, col(fg)));
    write(`${dir}/${file}.svg`, b.doc());
  }
  write(`${dir}/metadata.desktop`, ini({
    'Desktop Entry': { Name: label, Comment: 'The title bar of rekta-core: a hairline, no shadow', 'X-KDE-PluginInfo-Author': AUTHOR, 'X-KDE-PluginInfo-Name': id, 'X-KDE-PluginInfo-Version': pkg.version, 'X-KDE-PluginInfo-License': 'MIT', 'X-KDE-PluginInfo-Website': SITE, 'X-KDE-PluginInfo-EnabledByDefault': true },
  }));
  const title = { TitleEdgeTop: 1, TitleEdgeBottom: 1, TitleEdgeLeft: 12, TitleEdgeRight: 0 };
  write(`${dir}/${id}rc`, ini({
    General: {
      ActiveTextColor: `${rgb(c['text-primary'])},255`, InactiveTextColor: `${rgb(c['text-contrast-medium'])},255`,
      TitleAlignment: 'Left', TitleVerticalAlignment: 'Center', UseTextShadow: false, Shadow: false, Animation: ms('short'),
    },
    Layout: {
      BorderLeft: 1, BorderRight: 1, BorderBottom: 1, ...title, TitleBorderLeft: 8, TitleBorderRight: 8, TitleHeight: h,
      ButtonWidth: BUTTON, ButtonHeight: h, ButtonSpacing: 0, ButtonMarginTop: 0, ExplicitButtonSpacer: 8,
      PaddingTop: 0, PaddingBottom: 0, PaddingLeft: 0, PaddingRight: 0,
      ...Object.fromEntries(Object.entries(title).map(([k, v]) => [`${k}Maximized`, v])),
    },
  }));
}

// --------------------------------------------------------------------------------------------------------------- Kvantum

// Kvantum draws Qt widgets from named SVG elements: <element>-<state> for an interior, <element>-<state>-<cell> for its
// frame. Colours here are fixed per theme; Kvantum does not read the colour scheme.
function kvantum(c, dark) {
  const P = (x, op) => ({ color: solid(x), op }), INK = P(c['text-primary']), GROUND = P(c.bg), LOWc = P(c['text-contrast-low']);
  const MED = P(c['text-contrast-medium']), HIGHc = P(c['text-contrast-high']), FOCUS = P(c.focus), DIS = P(c.disabled), SEL = LOWc;
  const s = new Sheet(), r = radius.s;
  const kf = (el, state, b, layers, c0) => s.frame((cell) => (cell === 'center' ? `${el}-${state}` : `${el}-${state}-${cell}`), b, layers, c0);
  const ki = (id, w, h, draw) => s.el(id, w, h, draw);
  const states = (el, b, map) => { for (const [st, layers] of Object.entries(map)) kf(el, st, b, layers); };
  const glyph = (id, name, size, p, stroke) => ki(id, size, size, (x, y) => icon(name, x, y, size, size, size, p, stroke));
  const indicators = (base, name, size, colors = { normal: INK, focused: INK, pressed: INK, toggled: INK, disabled: DIS }, stroke = 2) => {
    for (const [st, p] of Object.entries(colors)) glyph(`${base}-${st}`, name, size, p, stroke * 20 / size);
  };

  // Push buttons: a 2px ink edge, the hover overlay under the pointer, filled in ink when toggled (the selected state).
  states('button', all(r), {
    normal: [{ ring: INK, w: 2, r }], focused: [{ fill: HOVER, r }, { ring: INK, w: 2, r }], pressed: [{ fill: PRESS, r }, { ring: INK, w: 2, r }],
    toggled: [{ fill: INK, r }], disabled: [{ ring: DIS, w: 2, r }],
  });
  s.frame((cell) => (cell === 'center' ? null : `button-focus-${cell}`), all(r), [{ ring: FOCUS, w: 2, r }]);
  ki('button-default-indicator', 8, 8); ki('flat-button-default-indicator', 8, 8);   // the default button is bold, nothing more
  // Tool buttons: no edge; the overlay under the pointer, the selection grey when toggled.
  states('tbutton', all(r), { normal: [], focused: [{ fill: HOVER, r }], pressed: [{ fill: PRESS, r }], toggled: [{ fill: SEL, r }], disabled: [] });
  for (const dir of ['up', 'down', 'left', 'right']) {
    indicators(`arrow-${dir}`, `chevron-${dir}`, 10, undefined, 1.5);
    indicators(`flat-arrow-${dir}`, `chevron-${dir}`, 10, undefined, 1.5);
  }
  indicators('carrow', 'chevron-down', 10, undefined, 1.5);

  // Fields and combo boxes: a 1px edge 2px in, radius-tile; with focus, the edge turns ink inside a 2px focus ring.
  const t = radius.tile, field = (edge, extra = []) => [{ fill: GROUND, d: 2, r: t }, { ring: edge, d: 2, r: t }, ...extra];
  const focused = field(INK, [{ ring: FOCUS, w: 2, r: t + 2 }]);
  states('lineedit', all(4), { normal: field(MED), focused, disabled: field(DIS) });
  states('combo', all(4), { normal: field(MED), focused: field(INK), pressed: focused, toggled: focused, disabled: field(DIS) });
  states('spin', all(4), { normal: field(MED), focused: field(INK, [{ fill: HOVER, d: 3, r: t - 1 }]), pressed: field(INK, [{ fill: PRESS, d: 3, r: t - 1 }]), toggled: field(INK), disabled: field(DIS) });
  for (const [n, ic] of [['plus', 'plus'], ['minus', 'minus'], ['up', 'chevron-up'], ['down', 'chevron-down']]) indicators(`spin-${n}`, ic, 10, undefined, 1.5);

  // Checkboxes and radio buttons: a 2px ink edge; checked, the box fills with ink and the check is cut out of it. In menus, as
  // in Metro, only the check, no box.
  const sq = (p) => (x, y) => `<rect x="${x}" y="${y}" width="16" height="16"${paint(p)}/>`, edge2 = (p) => (x, y) => ringRect(x, y, 16, 16, 2, p);
  const check = (p) => (x, y) => icon('check', x, y, 16, 16, 16, p, 2.5), dash = (p) => (x, y) => `<rect x="${x + 4}" y="${y + 7}" width="8" height="2"${paint(p)}/>`;
  const both = (...fs) => (x, y) => fs.map((f) => f(x, y)).join('');
  for (const [st, fill] of [['normal', null], ['focused', HOVER], ['pressed', PRESS]]) {
    ki(`checkbox-${st}`, 16, 16, both(...(fill ? [sq(fill)] : []), edge2(INK)));
    ki(`checkbox-checked-${st}`, 16, 16, both(sq(INK), check(GROUND)));
    ki(`checkbox-tristate-${st}`, 16, 16, both(sq(INK), dash(GROUND)));
    ki(`menu-checkbox-${st}`, 16, 16);
    ki(`menu-checkbox-checked-${st}`, 16, 16, check(INK));
    ki(`menu-checkbox-tristate-${st}`, 16, 16, dash(INK));
  }
  ki('checkbox-disabled', 16, 16, edge2(DIS)); ki('checkbox-checked-disabled', 16, 16, both(sq(DIS), check(GROUND)));
  ki('checkbox-tristate-disabled', 16, 16, both(sq(DIS), dash(GROUND)));
  const ringC = (p) => (x, y) => `<path d="M${x} ${y + 8}a8 8 0 1 0 16 0a8 8 0 1 0 -16 0zM${x + 2} ${y + 8}a6 6 0 1 0 12 0a6 6 0 1 0 -12 0z" fill-rule="evenodd"${paint(p)}/>`;
  const disc = (rr, p) => (x, y) => `<circle cx="${x + 8}" cy="${y + 8}" r="${rr}"${paint(p)}/>`;
  for (const [st, fill] of [['normal', null], ['focused', HOVER], ['pressed', PRESS]]) {
    ki(`radio-${st}`, 16, 16, both(...(fill ? [disc(8, fill)] : []), ringC(INK)));
    ki(`radio-checked-${st}`, 16, 16, both(...(fill ? [disc(8, fill)] : []), ringC(INK), disc(4, INK)));
    ki(`menu-radio-${st}`, 16, 16);
    ki(`menu-radio-checked-${st}`, 16, 16, disc(4, INK));
  }
  ki('radio-disabled', 16, 16, ringC(DIS)); ki('radio-checked-disabled', 16, 16, both(ringC(DIS), disc(4, DIS)));

  s.frame((cell) => (cell === 'center' ? null : `focus-${cell}`), all(2), [{ ring: FOCUS, w: 2, r: 2 }]);
  // Frames around views: a hairline. Content over chrome.
  states('common', all(1), { normal: [{ ring: LOWc }], focused: [{ ring: LOWc }] });

  // Tabs, as the pivot: no box; the current one underlined in ink.
  states('tab', all(4), { normal: [], focused: [], pressed: [{ side: 'bottom', w: 2, paint: INK }], toggled: [{ side: 'bottom', w: 2, paint: INK }] });
  for (const [st, bg] of [['normal', null], ['focused', HOVER], ['pressed', PRESS], ['toggled', null], ['disabled', null]])
    ki(`tab-close-${st}`, 16, 16, both(...(bg ? [(x, y) => `<rect x="${x}" y="${y}" width="16" height="16"${paint(bg)}/>`] : []), (x, y) => icon('close', x, y, 16, 16, 14, st === 'disabled' ? DIS : INK, 1.8)));
  ki('tab-tear', 2, 20);
  states('tabframe', all(1), { normal: [{ side: 'top', paint: LOWc }] });
  states('tabBarFrame', all(1), { normal: [{ side: 'bottom', paint: LOWc }] });

  indicators('tree-plus', 'chevron-right', 10, { normal: INK, focused: INK, pressed: INK, disabled: DIS }, 1.5);
  indicators('tree-minus', 'chevron-down', 10, { normal: INK, focused: INK, pressed: INK, disabled: DIS }, 1.5);

  // Table headers: no fill, a hairline below; the overlay under the pointer.
  states('header', all(1), { normal: [{ side: 'bottom', paint: LOWc }], focused: [{ fill: HOVER }, { side: 'bottom', paint: LOWc }], pressed: [{ fill: PRESS }, { side: 'bottom', paint: LOWc }], toggled: [{ side: 'bottom', paint: LOWc }] });
  ki('header-separator', 1, 20);
  indicators('header-down', 'chevron-down', 10, { normal: HIGHc, focused: INK, pressed: INK, disabled: DIS }, 1.5);
  indicators('header-up', 'chevron-up', 10, { normal: HIGHc, focused: INK, pressed: INK, disabled: DIS }, 1.5);
  ki('resize-grip-normal', 13, 13); ki('resize-grip-focused', 13, 13);

  // Tool bars and menu bars: on the ground, a hairline below.
  states('toolbar', all(1), { normal: [{ side: 'bottom', paint: LOWc }] });
  ki('toolbar-handle', 8, 20); ki('toolbar-separator', 5, 20, (x, y) => `<rect x="${x + 2}" y="${y}" width="1" height="20"${paint(LOWc)}/>`);

  // Sliders, as on the web: a 6px track filled in ink to the value, an ink bar as the thumb. Drawn upright; Kvantum turns it.
  states('slider', all(3), { normal: [{ fill: LOWc }], toggled: [{ fill: INK }], disabled: [{ fill: P(c.disabled, LOW) }] });
  ki('slider-tick-normal', 5, 1, (x, y) => `<rect x="${x}" y="${y}" width="5" height="1"${paint(MED)}/>`);
  for (const [st, p] of [['normal', INK], ['focused', INK], ['pressed', INK], ['disabled', DIS]])
    ki(`slidercursor-${st}`, 24, 10, (x, y) => `<rect x="${x}" y="${y}" width="24" height="10"${paint(p)}/>`);

  // Progress: 4px, the track the ink at 20%, the value the ink.
  const block = (p) => (x, y) => `<rect x="${x}" y="${y}" width="8" height="8"${paint(p)}/>`;
  ki('progress-normal', 8, 8, block(LOWc));
  ki('progress-pattern-normal', 8, 8, block(INK)); ki('progress-pattern-disabled', 8, 8, block(DIS));

  // Rows: the hover overlay, and the selection grey when selected. Square.
  states('itemview', all(2), { normal: [], focused: [{ fill: HOVER }], pressed: [{ fill: SEL }], toggled: [{ fill: SEL }] });

  // Splitters: the hairline between two panes.
  for (const [st, p] of [['normal', LOWc], ['focused', MED], ['pressed', FOCUS]])
    ki(`splitter-${st}`, 5, 20, (x, y) => `<rect x="${x + 2}" y="${y}" width="1" height="20"${paint(p)}/>`);
  for (const st of ['normal', 'focused', 'pressed']) { ki(`splitter-grip-${st}`, 4, 4); ki(`grip-${st}`, 4, 4); }

  // Scrollbars: a thin square bar that widens and darkens under the pointer; no groove.
  states('scrollbarslider', all(2), { normal: [{ fill: MED, d: 2 }], focused: [{ fill: HIGHc }], pressed: [{ fill: INK }] });

  // Menus: a floating surface (the ground, a 1px edge, radius-s); items take the overlay, inset like rows in a Plasma popup.
  states('menu', all(r), { normal: [{ fill: GROUND, r }, { ring: MED, r }] });
  states('menuitem', all(1), { normal: [], focused: [{ fill: HOVER }], pressed: [{ fill: PRESS }], toggled: [{ fill: HOVER }] });
  ki('menuitem-separator', 20, 9, (x, y) => `<rect x="${x}" y="${y + 4}" width="20" height="1"${paint(LOWc)}/>`);
  ki('menuitem-tearoff-normal', 20, 8); ki('menuitem-tearoff-focused', 20, 8);
  indicators('menuitem-right', 'chevron-right', 10, { normal: INK, focused: INK, pressed: INK, disabled: DIS }, 1.5);
  indicators('menuitem-left', 'chevron-left', 10, { normal: INK, focused: INK, pressed: INK, disabled: DIS }, 1.5);
  states('menubaritem', all(r), { normal: [], focused: [{ fill: HOVER, r }], pressed: [{ fill: PRESS, r }], toggled: [{ fill: HOVER, r }] });
  states('menubar', all(1), { normal: [] });

  // Tooltips, as on the web: the ink with the ground's colour as text.
  states('tooltip', all(r), { normal: [{ fill: INK, r }] });

  // Title bars of MDI windows and dock widgets: the decoration's buttons.
  states('titlebar', all(1), { normal: [{ fill: GROUND }, { side: 'bottom', paint: LOWc }], focused: [{ fill: GROUND }, { side: 'bottom', paint: LOWc }] });
  for (const [n, ic] of [['close', 'close'], ['maximize', 'window'], ['restore', 'window'], ['minimize', 'minus'], ['shade', 'chevron-up']])
    indicators(`mdi-${n}`, ic, 12, { normal: INK, focused: INK, pressed: INK, disabled: DIS }, 1.6);
  ki('mdi-menu-normal', 12, 12);
  glyph('dock-close', 'close', 12, INK, 2); glyph('dock-restore', 'window', 12, INK, 2);
  states('dock', all(1), { normal: [], focused: [] });
  ki('dial', 40, 40, (x, y) => `<path d="M${x} ${y + 20}a20 20 0 1 0 40 0a20 20 0 1 0 -40 0zM${x + 2} ${y + 20}a18 18 0 1 0 36 0a18 18 0 1 0 -36 0z" fill-rule="evenodd"${paint(MED)}/>`);
  ki('dial-notches', 40, 40); ki('dial-handle', 8, 8, (x, y) => `<rect x="${x}" y="${y}" width="8" height="8"${paint(INK)}/>`);

  const text = (n = INK, f = INK, p = INK, t = INK) => ({ 'text.normal.color': solid(n.color), 'text.focus.color': solid(f.color), 'text.press.color': solid(p.color), 'text.toggle.color': solid(t.color) });
  const fr = (n, more = {}) => ({ frame: true, 'frame.top': n.t ?? n, 'frame.bottom': n.b ?? n, 'frame.left': n.l ?? n, 'frame.right': n.r ?? n, ...more });
  const margins = (t, b, l, rr) => ({ 'text.margin': true, 'text.margin.top': t, 'text.margin.bottom': b, 'text.margin.left': l, 'text.margin.right': rr });
  const config = {
    '%General': {
      author: AUTHOR, comment: `Rekta Design ${dark ? 'dark' : 'light'}: straight lines, 2px ink edges, hairlines, no shadows`,
      ...(dark ? { dark_titlebar: true } : {}),
      respect_DE: true, x11drag: 'menubar_and_primary_toolbar', alt_mnemonic: true, click_behavior: 0,
      left_tabs: true, attach_active_tab: false, embedded_tabs: false, joined_inactive_tabs: false, mirror_doc_tabs: true, bold_active_tab: false,
      group_toolbar_buttons: false, toolbar_item_spacing: 2, toolbar_interior_spacing: 2, toolbar_separator_thickness: 5, center_toolbar_handle: true,
      spread_progressbar: true, progressbar_thickness: 4, textless_progressbar: false,
      composite: true, menu_shadow_depth: 0, tooltip_shadow_depth: 0, shadowless_popup: true, spread_menuitems: false, scrollable_menu: true,
      menu_separator_height: 9, submenu_overlap: 0, submenu_delay: 250,
      translucent_windows: false, blurring: false, popup_blurring: false, reduce_window_opacity: 0, reduce_menu_opacity: 0,
      animate_states: true, no_inactiveness: true, no_window_pattern: true,
      splitter_width: 5, scroll_width: 8, scroll_min_extent: 36, scroll_arrows: false, scrollbar_in_view: true,
      transient_scrollbar: true, transient_groove: false, center_scrollbar_indicator: true,
      slider_width: 6, slider_handle_width: 24, slider_handle_length: 10, check_size: 16,
      tree_branch_line: false, groupbox_top_label: true, button_contents_shift: false,
      inline_spin_indicators: false, vertical_spin_indicators: false, spin_button_width: 32,
      combo_as_lineedit: true, combo_menu: true, combo_focus_rect: false, square_combo_button: false, hide_combo_checkboxes: true,
      layout_spacing: T.spacing['space-s'], layout_margin: T.spacing['space-m'], small_icon_size: 16, large_icon_size: 32, button_icon_size: 16, toolbar_icon_size: 22,
      toolbutton_style: 0, dialog_button_layout: 0, menubar_mouse_tracking: true, merge_menubar_with_toolbar: true,
      fill_rubberband: false, remove_extra_frames: true, tooltip_delay: -1,
    },
    GeneralColors: {
      'window.color': solid(c.bg), 'base.color': solid(c.bg), 'alt.base.color': solid(c.surface), 'button.color': solid(c.surface),
      'light.color': solid(c['surface-2']), 'mid.light.color': solid(c['text-contrast-low']), 'dark.color': solid(c.bg), 'mid.color': solid(c['text-contrast-low']),
      'shadow.color': '#000000', 'highlight.color': solid(c['text-contrast-low']), 'inactive.highlight.color': solid(c['surface-2']),
      'tooltip.base.color': solid(c['text-primary']), 'text.color': solid(c['text-primary']), 'window.text.color': solid(c['text-primary']),
      'button.text.color': solid(c['text-primary']), 'disabled.text.color': solid(c.disabled), 'tooltip.text.color': solid(c.bg),
      'highlight.text.color': solid(c['text-primary']), 'link.color': solid(c.focus), 'link.visited.color': solid(c['text-contrast-high']),
      'progress.indicator.text.color': solid(c['text-primary']),
    },
    Hacks: {
      transparent_ktitle_label: true, transparent_dolphin_view: false, transparent_menutitle: true, respect_darkness: true,
      force_size_grip: false, iconless_pushbutton: false, iconless_menu: false, normal_default_pushbutton: false,
      tint_on_mouseover: 0, no_selection_tint: true, disabled_icon_opacity: 50, kcapacitybar_as_progressbar: true, blur_translucent: false,
      single_top_toolbar: false, middle_click_scroll: false,
    },
    PanelButtonCommand: {
      ...fr(r), 'frame.element': 'button', interior: true, 'interior.element': 'button', 'frame.expansion': 0, focusFrame: true,
      'indicator.element': 'arrow', 'indicator.size': 10, ...text(INK, INK, INK, GROUND), 'text.bold': true, 'text.boldness': 2,
      'text.shadow': false, 'text.iconspacing': 6, ...margins(4, 4, 11, 11), min_height: T.densities.compact['control-height'],
    },
    PanelButtonTool: {
      inherits: 'PanelButtonCommand', 'frame.element': 'tbutton', 'interior.element': 'tbutton', focusFrame: false,
      ...text(), 'text.bold': false, ...margins(4, 4, 6, 6), min_height: 0,
    },
    ToolbarButton: { inherits: 'PanelButtonTool', 'frame.element': 'tbutton', 'interior.element': 'tbutton', ...text() },
    Dock: { inherits: 'PanelButtonCommand', frame: false, interior: false, 'indicator.element': 'dock', ...text() },
    DockTitle: { inherits: 'PanelButtonCommand', frame: false, interior: false, ...text(HIGHc, INK), 'text.bold': true, ...margins(4, 4, 8, 8) },
    IndicatorSpinBox: { inherits: 'PanelButtonCommand', ...fr(4), 'frame.element': 'spin', 'interior.element': 'spin', 'indicator.element': 'spin', 'indicator.size': 10, focusFrame: false, ...text() },
    RadioButton: { inherits: 'PanelButtonCommand', frame: false, 'interior.element': 'radio', focusFrame: false, ...text(), 'text.bold': false, ...margins(2, 2, 6, 6), min_height: 0 },
    CheckBox: { inherits: 'PanelButtonCommand', frame: false, 'interior.element': 'checkbox', focusFrame: false, ...text(), 'text.bold': false, ...margins(2, 2, 6, 6), min_height: 0 },
    Focus: { inherits: 'PanelButtonCommand', frame: true, 'frame.element': 'focus', ...fr(2), interior: false, 'frame.patternsize': 0 },
    GenericFrame: { inherits: 'PanelButtonCommand', ...fr(1), 'frame.element': 'common', interior: false, focusFrame: false, ...text() },
    LineEdit: { inherits: 'PanelButtonCommand', ...fr(4), 'frame.element': 'lineedit', 'interior.element': 'lineedit', focusFrame: false, ...text(), 'text.bold': false, ...margins(4, 4, 8, 8) },
    ToolbarLineEdit: { inherits: 'LineEdit', 'frame.element': 'lineedit', 'interior.element': 'lineedit' },
    DropDownButton: { inherits: 'PanelButtonCommand', 'indicator.element': 'arrow-down' },
    IndicatorArrow: { 'indicator.element': 'arrow', 'indicator.size': 10 },
    ToolboxTab: { inherits: 'PanelButtonCommand', ...text(HIGHc, INK, INK, INK) },
    Tab: {
      inherits: 'PanelButtonCommand', ...fr(4), 'frame.element': 'tab', 'interior.element': 'tab', 'indicator.element': 'tab', 'indicator.size': 16,
      focusFrame: false, ...text(HIGHc, INK, INK, INK), 'text.bold': false, ...margins(4, 4, 12, 12), min_height: 0,
    },
    TabFrame: { inherits: 'PanelButtonCommand', ...fr(1), 'frame.element': 'tabframe', interior: false, focusFrame: false },
    TabBarFrame: { inherits: 'GenericFrame', ...fr(1), 'frame.element': 'tabBarFrame', interior: false },
    TreeExpander: { inherits: 'PanelButtonCommand', frame: false, interior: false, 'indicator.element': 'tree', 'indicator.size': 10 },
    HeaderSection: {
      inherits: 'PanelButtonCommand', ...fr(1), 'frame.element': 'header', 'interior.element': 'header', 'indicator.element': 'header', 'indicator.size': 10,
      focusFrame: false, ...text(HIGHc, INK, INK, INK), 'text.bold': true, ...margins(4, 4, 8, 8), min_height: 0, 'frame.expansion': 0,
    },
    SizeGrip: { 'indicator.element': 'resize-grip' },
    Toolbar: { inherits: 'PanelButtonCommand', ...fr(1), 'frame.element': 'toolbar', interior: false, 'indicator.element': 'toolbar', 'indicator.size': 5, ...text(), 'text.bold': false, 'text.margin': 0, focusFrame: false },
    Slider: { inherits: 'PanelButtonCommand', ...fr(3), 'frame.element': 'slider', 'interior.element': 'slider', focusFrame: false },
    SliderCursor: { inherits: 'PanelButtonCommand', frame: false, 'interior.element': 'slidercursor' },
    Progressbar: { inherits: 'PanelButtonCommand', frame: false, 'frame.element': 'progress', interior: true, 'interior.element': 'progress', ...text(), 'text.bold': false, 'text.margin': 0, 'frame.expansion': 0 },
    ProgressbarContents: { inherits: 'PanelButtonCommand', frame: false, interior: true, 'interior.element': 'progress-pattern' },
    ItemView: { inherits: 'PanelButtonCommand', ...fr(2), 'frame.element': 'itemview', 'interior.element': 'itemview', focusFrame: false, ...text(), 'text.bold': false, ...margins(2, 2, 4, 4), min_height: 0, 'frame.expansion': 0 },
    Splitter: { inherits: 'PanelButtonCommand', frame: false, interior: true, 'interior.element': 'splitter', 'indicator.element': 'splitter-grip', 'indicator.size': 16 },
    Scrollbar: { inherits: 'PanelButtonCommand', 'indicator.element': 'arrow', 'indicator.size': 10 },
    ScrollbarSlider: { inherits: 'PanelButtonCommand', ...fr(2), 'frame.element': 'scrollbarslider', interior: true, 'interior.element': 'scrollbarslider', 'indicator.element': 'grip', 'indicator.size': 4, focusFrame: false, 'frame.expansion': 0 },
    ScrollbarGroove: { inherits: 'PanelButtonCommand', frame: false, interior: false },
    MenuItem: { inherits: 'PanelButtonCommand', ...fr(1), 'frame.element': 'menuitem', 'interior.element': 'menuitem', 'indicator.element': 'menuitem', focusFrame: false, ...text(), 'text.bold': false, ...margins(6, 6, 8, 8), min_height: T.densities.compact['control-height'] },
    MenuBar: { inherits: 'PanelButtonCommand', frame: false, 'frame.element': 'menubar', interior: false, ...text() },
    MenuBarItem: { inherits: 'PanelButtonCommand', ...fr(r), 'frame.element': 'menubaritem', 'interior.element': 'menubaritem', focusFrame: false, ...text(), 'text.bold': false, ...margins(2, 2, 8, 8), min_height: 0 },
    TitleBar: { inherits: 'PanelButtonCommand', frame: false, interior: true, 'interior.element': 'titlebar', 'indicator.element': 'mdi', 'indicator.size': 12, ...text(HIGHc, INK), 'text.bold': true, 'text.italic': false },
    ComboBox: { inherits: 'PanelButtonCommand', ...fr(4), 'frame.element': 'combo', 'interior.element': 'combo', 'indicator.element': 'carrow', focusFrame: false, ...text(), 'text.bold': false, ...margins(4, 4, 8, 8) },
    Menu: { inherits: 'PanelButtonCommand', ...fr(r), 'frame.element': 'menu', 'interior.element': 'menu', ...text(), 'text.bold': false, 'text.shadow': false, 'frame.expansion': 0 },
    GroupBox: { inherits: 'GenericFrame', frame: false, interior: false, ...text(), 'text.bold': true, 'text.margin': 0, 'frame.expansion': 0 },
    ToolTip: { inherits: 'GenericFrame', ...fr(r), 'frame.element': 'tooltip', interior: true, 'interior.element': 'tooltip', ...text(GROUND), 'text.margin': true, 'text.margin.top': 2, 'text.margin.bottom': 2, 'text.margin.left': 4, 'text.margin.right': 4, 'frame.expansion': 0 },
    StatusBar: { inherits: 'GenericFrame', frame: false, interior: false },
    Window: { interior: false },
  };
  return { svg: s.doc(), kvconfig: ini(config) };
}

// ----------------------------------------------------------------------------------------- wallpaper, previews, splash

// Rendered with rsvg-convert, as Plasma wants pixels for wallpapers and previews.
function render(svg, w, h, file) {
  const p = join(out, file), src = `${p}.svg`;
  mkdirSync(dirname(p), { recursive: true });
  writeFileSync(src, svg);
  try { execFileSync('rsvg-convert', ['-w', `${w}`, '-h', `${h}`, '-o', p, src]); } catch (e) {
    throw new Error(`rsvg-convert (librsvg) renders the wallpaper and previews: ${e.message}`);
  } finally { rmSync(src); }
}

// The mark of the app icon at the size of a screen: one straight line across the ground, and the small success square over
// its end. Calm large areas, one vivid small accent.
function wallpaperSvg(c, W, H, light) {
  const t = Math.round(Math.min(W, H) * 0.008), y = Math.round(H * 0.618), x1 = Math.round(W * 0.618);
  const gap = Math.round(t * 88 / 36);   // as in site/icon.svg: the square sits 88/36 of the line's height above it
  return `<svg xmlns="http://www.w3.org/2000/svg" width="${W}" height="${H}" viewBox="0 0 ${W} ${H}">`
    + `<rect width="${W}" height="${H}" fill="${solid(light ? c.surface : c.bg)}"/>`
    + `<rect x="0" y="${y}" width="${x1}" height="${t}" fill="${solid(light ? c['text-contrast-low'] : c['surface-2'])}"/>`
    + `<rect x="${x1 - t}" y="${y - gap - t}" width="${t}" height="${t}" fill="${solid(c['status-success-full'])}"/></svg>`;
}
function wallpaper() {
  const dir = 'wallpapers/Rekta';
  write(`${dir}/metadata.json`, JSON.stringify({ KPlugin: { Authors: [{ Name: AUTHOR }], Id: 'Rekta', License: 'CC-BY-4.0', Name: 'Rekta' } }, null, 2) + '\n');
  for (const [W, H] of [[3840, 2160], [2560, 1600], [5120, 2160], [5120, 1440], [2160, 3840]]) {
    render(wallpaperSvg(T.themes.light, W, H, true), W, H, `${dir}/contents/images/${W}x${H}.png`);
    render(wallpaperSvg(T.themes.dark, W, H, false), W, H, `${dir}/contents/images_dark/${W}x${H}.png`);
  }
  render(wallpaperSvg(T.themes.dark, 1600, 1000, false), 400, 250, `${dir}/contents/screenshot.png`);
}

// A small picture of the desktop for the global theme's entry in System Settings: the wallpaper, a window with the title
// bar, rows and a few status tiles, and the panel.
function previewSvg(c, light) {
  const W = 1600, H = 1000, k = (n) => solid(c[n]);
  const tile = (x, y, w, h, name) => `<rect x="${x}" y="${y}" width="${w}" height="${h}" rx="${radius.tile}" fill="${k(`status-${name}-tile`)}"/>`;
  const row = (y, w) => `<rect x="900" y="${y - 34}" width="${w}" height="14" fill="${k('text-contrast-medium')}"/><rect x="900" y="${y}" width="460" height="2" fill="${k('text-contrast-low')}"/>`;
  return wallpaperSvg(c, W, H, light).replace('</svg>', '')
    + `<rect x="200" y="120" width="1200" height="700" fill="${k('bg')}" stroke="${k('text-contrast-medium')}" stroke-width="2"/>`
    + `<rect x="200" y="190" width="1200" height="2" fill="${k('text-contrast-low')}"/><rect x="230" y="146" width="160" height="18" fill="${k('text-primary')}"/>`
    + ['close', 'window', 'minus'].map((n, i) => icon(n, 1400 - 92 * (i + 1), 120, 92, 70, 40, { color: k('text-primary') })).join('')
    + `<rect x="240" y="250" width="420" height="44" fill="${k('text-primary')}"/>`
    + tile(240, 340, 300, 300, 'info') + tile(552, 340, 144, 144, 'warning') + tile(552, 496, 144, 144, 'success') + tile(708, 340, 144, 144, 'error')
    + [[440, 300], [540, 360], [640, 240], [740, 320]].map(([y, w]) => row(y, w)).join('')
    + `<rect x="0" y="${H - 64}" width="${W}" height="64" fill="${k('bg')}"/><rect x="0" y="${H - 64}" width="${W}" height="2" fill="${k('text-contrast-low')}"/>`
    + [0, 1, 2, 3].map((i) => `<rect x="${640 + i * 84}" y="${H - 52}" width="40" height="40" fill="${k(i === 1 ? 'text-primary' : 'text-contrast-medium')}"/>`).join('')
    + `<rect x="${628 + 84}" y="${H - 6}" width="64" height="4" fill="${k('text-primary')}"/></svg>`;
}

function fill(template, values) {
  return template.replace(/@([\w-]+)@/g, (m, key) => { if (!(key in values)) throw new Error(`template: no value for ${m}`); return values[key]; });
}
const qmlEase = (name) => bezier(name).concat([1, 1]).join(', ');

function lookAndFeel(id, label, c, light) {
  const dir = `plasma/look-and-feel/${id}`, variant = light ? 'Light' : 'Dark';
  write(`${dir}/metadata.json`, JSON.stringify({
    KPackageStructure: 'Plasma/LookAndFeel',
    KPlugin: { Authors: [{ Name: AUTHOR }], Category: '', Description: `Rekta Design, ${light ? 'light' : 'dark'}: the colour scheme, the Plasma Style, the title bar, the Kvantum style, a splash and a wallpaper`, Id: id, License: 'MIT', Name: label, Version: pkg.version, Website: SITE },
  }, null, 2) + '\n');
  write(`${dir}/contents/defaults`, [
    `[kdeglobals][KDE]\nwidgetStyle=${light ? 'kvantum' : 'kvantum-dark'}`,
    `[kdeglobals][General]\nColorScheme=Rekta${variant}`,
    '[plasmarc][Theme]\nname=rekta',
    '[Wallpaper]\nImage=Rekta',
    `[kwinrc][org.kde.kdecoration2]\nlibrary=org.kde.kwin.aurorae\ntheme=__aurorae__svg__Rekta${variant}`,
    `[ksplashrc][KSplash]\nEngine=KSplashQML\nTheme=${id}`,
  ].join('\n\n') + '\n');
  write(`${dir}/contents/splash/Splash.qml`, fill(readFileSync(join(root, 'kde', 'splash', 'Splash.qml'), 'utf8'), {
    bg: solid(c.bg), track: solid(c['text-contrast-low']), ink: solid(c['text-primary']), done: solid(c['status-success-full']),
    moderate: ms('moderate'), short: ms('short'), 'ease-base': qmlEase('base'), 'ease-in': qmlEase('in'),
  }));
  render(previewSvg(c, light), 600, 375, `${dir}/contents/previews/preview.png`);
}

// The login screen: Barlow ships with it, its colours come from theme.conf (both themes; variant picks one).
function sddm() {
  const dir = 'sddm/themes/rekta', keys = ['bg', 'surface', 'text-primary', 'text-contrast-low', 'text-contrast-medium', 'text-contrast-high',
    'accent', 'on-accent', 'focus', 'disabled', 'status-error-full', 'status-warning-full', 'status-success-full'];
  const colours = Object.fromEntries(['light', 'dark'].flatMap((t) => keys.map((k) => [`${t}_${k.replace(/-/g, '_')}`, solid(T.themes[t][k])])));
  write(`${dir}/theme.conf`, ini({ General: { variant: 'dark', ...colours, hover: solid(hover), hoverOpacity: HOVER.op } }));
  write(`${dir}/metadata.desktop`, ini({
    SddmGreeterTheme: {
      Name: 'Rekta', Description: 'Rekta Design: the ground, a pivot of users, one field', Author: AUTHOR, License: 'MIT', Type: 'sddm-theme',
      Version: pkg.version, Website: SITE, Screenshot: 'preview.png', MainScript: 'Main.qml', ConfigFile: 'theme.conf', 'Theme-Id': 'rekta', 'Theme-API': '2.0', QtVersion: 6,
    },
  }));
  write(`${dir}/Main.qml`, fill(readFileSync(join(root, 'kde', 'sddm', 'Main.qml'), 'utf8'), {
    press: ms('press'), short: ms('short'), moderate: ms('moderate'), float: ms('float'), stagger: ms('stagger'),
    'ease-base': qmlEase('base'), 'ease-in': qmlEase('in'), 'ease-out': qmlEase('out'),
    'icon-chevron-up': ICONS['chevron-up'].shapes[0].attrs.d, 'icon-check': ICONS.check.shapes[0].attrs.d,
  }));
  for (const w of [300, 400, 600]) copy(`core/fonts/barlow-${w}.ttf`, `${dir}/fonts/barlow-${w}.ttf`);
  copy('core/fonts/OFL.txt', `${dir}/fonts/OFL.txt`);
  render(previewSvg(T.themes.dark, false), 800, 500, `${dir}/preview.png`);
}

// --------------------------------------------------------------------------------------------------------------- build

rmSync(out, { recursive: true, force: true });
const themes = { Light: T.themes.light, Dark: T.themes.dark };
for (const [v, c] of Object.entries(themes)) {
  write(`color-schemes/Rekta${v}.colors`, colorScheme(`Rekta${v}`, `Rekta ${v}`, c, T.themes.dark));
  aurorae(`Rekta${v}`, c, `Rekta ${v}`);
  const kv = kvantum(c, v === 'Dark'), name = v === 'Light' ? 'Rekta' : 'RektaDark';
  write(`Kvantum/Rekta/${name}.kvconfig`, kv.kvconfig);
  write(`Kvantum/Rekta/${name}.svg`, kv.svg);
}
plasmaStyle();
lookAndFeel('design.rekta.light', 'Rekta Light', T.themes.light, true);
lookAndFeel('design.rekta.dark', 'Rekta Dark', T.themes.dark, false);
wallpaper();
sddm();
console.log(`✓ ${out} written (Rekta Design ${pkg.version} for KDE Plasma 6)`);

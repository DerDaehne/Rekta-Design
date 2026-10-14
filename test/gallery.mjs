// The reference gallery: the same specimens drawn by rekta-core (core/test/gallery.c, headless) and by rekta.css in
// Chromium, each in a frame 320 px wide with 16 px around it, light and dark, held to each other. Text is rasterized by
// different engines, so pixels are not compared one for one; what must agree is
//   size     the frame's height, within 2 px
//   extent   where the ink lies (anything off the background), each edge within 2 px
//   colour   every solid colour one side fills a share of the frame with, the other fills too (within 10 per channel)
//   shape    the two at a quarter of the size, in grey, apart by at most 8 on average
// and every specimen exists on both sides. Writes report.html beside the core images: each pair side by side.
//   make -C core gallery && node test/gallery.mjs core/build/gallery
//   PLAYWRIGHT=/path/to/playwright/index.mjs ...     (default: import 'playwright')
import { createReadStream, readFileSync, statSync, writeFileSync } from 'node:fs';
import { createServer } from 'node:http';
import { dirname, extname, join, normalize, sep } from 'node:path';
import { fileURLToPath } from 'node:url';
import { deflateSync, inflateSync } from 'node:zlib';

const { chromium } = await import(process.env.PLAYWRIGHT || 'playwright');
const root = join(dirname(fileURLToPath(import.meta.url)), '..');
const coreDir = process.argv[2] || join(root, 'core/build/gallery');
const dist = join(root, 'dist');

const specimens = {
  'button': '<button class="rk-btn" type="button">save</button>',
  'check-on': '<label class="rk-switch"><input type="checkbox" checked> done</label>',
  'check-off': '<label class="rk-switch"><input type="checkbox"> done</label>',
  'toggle-on': toggle(true),
  'toggle-off': toggle(false),
  'segments': '<div class="rk-segments" role="radiogroup" aria-label="show">' +
    ['all', 'failed', 'waiting'].map((o, i) => `<label><input type="radio" name="s"${i ? '' : ' checked'}>${o}</label>`).join('') + '</div>',
  'field': '<div class="rk-field"><label class="rk-label" for="f">name</label><input class="rk-input" id="f" placeholder="lowercase"></div>',
  'field-filled': '<div class="rk-field"><label class="rk-label" for="f">name</label><input class="rk-input" id="f" value="rekta"></div>',
  'slider': '<div class="rk-slider"><input class="rk-range" type="range" min="0" max="100" value="40" aria-label="value"><output>40</output></div>',
  'progress': '<progress class="rk-progress" max="100" value="40"></progress>',
  'rows': '<ul class="rk-rows"><li><span>2010</span><span class="rk-meta">Windows Phone 7</span></li><li><span>2012</span><span class="rk-meta">Windows 8</span></li></ul>',
  'tile': '<div class="rk-tiles"><div class="rk-tile" data-status="success"><span class="rk-tile-word">done</span><span class="rk-stat"><b>12</b><span>builds today</span></span></div></div>',
  'drop': '<label class="rk-drop"><input type="file" multiple><b>Drop files here</b><span>or choose them</span></label>',
};
// where the web leaves a part to the browser, its colours are the browser's: the other checks still hold
const browserDrawn = {};
function toggle(on) {
  return `<label class="rk-toggle"><span class="rk-toggle-label">feed</span><input type="checkbox"${on ? ' checked' : ''}>` +
    '<span class="rk-toggle-track" aria-hidden="true"></span><span class="rk-toggle-word" data-on="on" data-off="off" aria-hidden="true"></span></label>';
}

// PNG in (8-bit RGB or RGBA), RGBA out
function decode(b) {
  let at = 8, w = 0, h = 0, ch = 4;
  const idat = [];
  while (at < b.length) {
    const len = b.readUInt32BE(at), type = b.toString('latin1', at + 4, at + 8), data = b.subarray(at + 8, at + 8 + len);
    if (type === 'IHDR') {
      w = data.readUInt32BE(0); h = data.readUInt32BE(4);
      if (data[8] !== 8 || (data[9] !== 6 && data[9] !== 2)) throw new Error('not 8-bit RGB(A)');
      ch = data[9] === 6 ? 4 : 3;
    }
    if (type === 'IDAT') idat.push(data);
    at += 12 + len;
  }
  const raw = inflateSync(Buffer.concat(idat)), row = w * ch, line = Buffer.alloc(row), prev = Buffer.alloc(row), px = Buffer.alloc(w * h * 4);
  for (let y = 0; y < h; y++) {
    const f = raw[y * (row + 1)], src = y * (row + 1) + 1;
    for (let x = 0; x < row; x++) {
      const a = x >= ch ? line[x - ch] : 0, up = prev[x], c = x >= ch ? prev[x - ch] : 0;
      let v = raw[src + x];
      if (f === 1) v += a; else if (f === 2) v += up; else if (f === 3) v += (a + up) >> 1;
      else if (f === 4) { const p = a + up - c, pa = Math.abs(p - a), pb = Math.abs(p - up), pc = Math.abs(p - c); v += pa <= pb && pa <= pc ? a : pb <= pc ? up : c; }
      line[x] = v & 255;
    }
    for (let x = 0; x < w; x++) for (let k = 0; k < 4; k++) px[(y * w + x) * 4 + k] = k < ch ? line[x * ch + k] : 255;
    line.copy(prev);
  }
  return { w, h, px };
}

function crop(img, x, y, w, h) {
  const px = Buffer.alloc(w * h * 4);
  for (let j = 0; j < h; j++) img.px.copy(px, j * w * 4, ((y + j) * img.w + x) * 4, ((y + j) * img.w + x + w) * 4);
  return { w, h, px };
}

const at = (img, x, y) => { const i = (y * img.w + x) * 4; return [img.px[i], img.px[i + 1], img.px[i + 2]]; };
const apart = (a, b) => Math.max(Math.abs(a[0] - b[0]), Math.abs(a[1] - b[1]), Math.abs(a[2] - b[2]));

function extent(img) {
  const bg = at(img, 0, 0);
  let x0 = img.w, y0 = img.h, x1 = -1, y1 = -1;
  for (let y = 0; y < img.h; y++) for (let x = 0; x < img.w; x++)
    if (apart(at(img, x, y), bg) > 24) { x0 = Math.min(x0, x); y0 = Math.min(y0, y); x1 = Math.max(x1, x); y1 = Math.max(y1, y); }
  return x1 < 0 ? null : { x0, y0, x1, y1 };
}

// solid colours that fill at least 1% of the frame, the background aside
function solids(img) {
  const bg = at(img, 0, 0), count = new Map();
  for (let i = 0; i < img.w * img.h; i++) {
    const k = (img.px[i * 4] << 16) | (img.px[i * 4 + 1] << 8) | img.px[i * 4 + 2];
    count.set(k, (count.get(k) || 0) + 1);
  }
  return [...count].filter(([k, n]) => n >= img.w * img.h * 0.01).map(([k]) => [k >> 16, (k >> 8) & 255, k & 255])
    .filter((c) => apart(c, bg) > 10);
}

function shapeDistance(a, b) {
  const w = Math.min(a.w, b.w) >> 2, h = Math.min(a.h, b.h) >> 2;
  const grey = (img, x, y) => { let s = 0; for (let j = 0; j < 4; j++) for (let i = 0; i < 4; i++) { const c = at(img, x * 4 + i, y * 4 + j); s += 0.299 * c[0] + 0.587 * c[1] + 0.114 * c[2]; } return s / 16; };
  let sum = 0;
  for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) sum += Math.abs(grey(a, x, y) - grey(b, x, y));
  return sum / (w * h);
}

const hex = (c) => '#' + c.map((v) => v.toString(16).padStart(2, '0')).join('');

// the core side: whole windows and where each frame lies in them
const frames = new Map(readFileSync(join(coreDir, 'frames.txt'), 'utf8').trim().split('\n').map((l) => {
  const [name, x, y, w, h] = l.split(' ');
  return [name, { x: Math.round(+x), y: Math.round(+y), w: Math.round(+w), h: Math.round(+h) }];
}));

const types = { '.html': 'text/html', '.css': 'text/css', '.js': 'text/javascript', '.woff2': 'font/woff2' };
const server = createServer((req, res) => {
  try {
    const path = decodeURIComponent(new URL(req.url, 'http://x').pathname);
    if (path === '/gallery.html') {
      const u = new URL(req.url, 'http://x');
      const body = specimens[u.searchParams.get('s')];
      res.writeHead(200, { 'content-type': 'text/html' });
      res.end(`<!doctype html><html lang="en" data-theme="${u.searchParams.get('t')}" data-motion="off"><head><meta charset="utf-8">
<link rel="stylesheet" href="/rekta.tokens.css"><link rel="stylesheet" href="/rekta.fonts.css"><link rel="stylesheet" href="/rekta.css">
<style>body { margin: 0; } .frame { width: 352px; padding: 16px; }</style></head>
<body class="rk"><div class="frame">${body}</div><script src="/rekta.global.js"></script></body></html>`);
      return;
    }
    const file = join(dist, normalize(path));
    if (!file.startsWith(dist + sep)) throw new Error('outside');
    statSync(file);
    res.writeHead(200, { 'content-type': types[extname(file)] || 'application/octet-stream' });
    createReadStream(file).pipe(res);
  } catch { res.writeHead(404).end(); }
}).listen(0, '127.0.0.1');
await new Promise((r) => server.on('listening', r));
const base = `http://127.0.0.1:${server.address().port}/`;

const browser = await chromium.launch();
const page = await browser.newPage({ viewport: { width: 1024, height: 600 }, deviceScaleFactor: 1 });   // a desktop, as core's window
const failures = [], rows = [];
let checked = 0;
const fail = (what) => failures.push(what);

const coreNames = [...frames.keys()].map((k) => k.replace(/-(light|dark)$/, ''));
for (const n of new Set(coreNames)) if (!specimens[n]) fail(`${n}: drawn by core, missing on the web`);

for (const name of Object.keys(specimens)) {
  for (const theme of ['light', 'dark']) {
    const key = `${name}-${theme}`, f = frames.get(key);
    if (!f) { fail(`${key}: missing in core`); continue; }
    await page.goto(`${base}gallery.html?s=${name}&t=${theme}`);
    await page.evaluate(() => document.fonts.ready);
    await page.waitForTimeout(100);                         // rekta.js has enhanced what it enhances
    const web = decode(await page.locator('.frame').screenshot({ animations: 'disabled', caret: 'hide' }));
    const core = crop(decode(readFileSync(join(coreDir, `${key}.png`))), f.x, f.y, f.w, f.h);
    writeFileSync(join(coreDir, `${key}-web.png`), await page.locator('.frame').screenshot({ animations: 'disabled', caret: 'hide' }));
    const notes = [];
    checked += 1;
    if (Math.abs(core.h - web.h) > 2 || core.w !== web.w) notes.push(`size ${core.w}×${core.h} in core, ${web.w}×${web.h} on the web`);
    const ec = extent(core), ew = extent(web);
    if (!ec || !ew) notes.push('nothing drawn');
    else {
      const off = ['x0', 'y0', 'x1', 'y1'].filter((k) => Math.abs(ec[k] - ew[k]) > 2);
      if (off.length) notes.push(`ink at ${ec.x0},${ec.y0}–${ec.x1},${ec.y1} in core, ${ew.x0},${ew.y0}–${ew.x1},${ew.y1} on the web`);
    }
    const sc = solids(core), sw = solids(web);
    const lone = (a, b) => a.filter((c) => !b.some((d) => apart(c, d) <= 10)).map(hex);
    const onlyCore = lone(sc, sw), onlyWeb = lone(sw, sc);
    if (!browserDrawn[key] && onlyCore.length) notes.push(`fills only core has: ${onlyCore.join(' ')}`);
    if (!browserDrawn[key] && onlyWeb.length) notes.push(`fills only the web has: ${onlyWeb.join(' ')}`);
    const shape = shapeDistance(core, web);
    if (shape > 8) notes.push(`shapes apart by ${shape.toFixed(1)}`);
    for (const n of notes) fail(`${key}: ${n}`);
    rows.push({ key, notes, shape, aside: browserDrawn[key] && (onlyCore.length || onlyWeb.length) ? `colours not held: ${browserDrawn[key]}` : '' });
  }
}
await browser.close();
server.close();

const esc = (s) => s.replace(/&/g, '&amp;').replace(/</g, '&lt;');
writeFileSync(join(coreDir, 'report.html'), `<!doctype html><meta charset="utf-8"><title>Reference gallery</title>
<style>body{font:14px system-ui;margin:24px;background:#888}figure{display:inline-block;margin:0 16px 24px 0;vertical-align:top;background:#fff;padding:8px}
figcaption{max-width:720px}.bad{color:#b00}img{image-rendering:pixelated;margin-right:4px}</style>
<h1>Core | web</h1>${rows.map((r) => `<figure><div><img src="${r.key}-core.png"><img src="${r.key}-web.png"></div>
<figcaption><b>${r.key}</b> · shape ${r.shape.toFixed(1)}${r.aside ? `<div>${esc(r.aside)}</div>` : ''}${r.notes.map((n) => `<div class="bad">${esc(n)}</div>`).join('')}</figcaption></figure>`).join('\n')}`);
for (const r of rows) {
  const f = frames.get(r.key);
  const img = crop(decode(readFileSync(join(coreDir, `${r.key}.png`))), f.x, f.y, f.w, f.h);
  writeFileSync(join(coreDir, `${r.key}-core.png`), encode(img));
}

function encode(img) {
  const crcTable = Array.from({ length: 256 }, (_, n) => { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; return c >>> 0; });
  const crc = (buf) => { let c = 0xffffffff; for (const b of buf) c = crcTable[(c ^ b) & 255] ^ (c >>> 8); return (c ^ 0xffffffff) >>> 0; };
  const chunk = (type, data) => { const len = Buffer.alloc(4), t = Buffer.from(type, 'latin1'), c = Buffer.alloc(4); len.writeUInt32BE(data.length); c.writeUInt32BE(crc(Buffer.concat([t, data]))); return Buffer.concat([len, t, data, c]); };
  const ihdr = Buffer.alloc(13); ihdr.writeUInt32BE(img.w, 0); ihdr.writeUInt32BE(img.h, 4); ihdr[8] = 8; ihdr[9] = 6;
  const raw = Buffer.alloc((img.w * 4 + 1) * img.h);
  for (let y = 0; y < img.h; y++) img.px.copy(raw, y * (img.w * 4 + 1) + 1, y * img.w * 4, (y + 1) * img.w * 4);
  return Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ihdr), chunk('IDAT', deflateSync(raw)), chunk('IEND', Buffer.alloc(0))]);
}

if (failures.length) {
  console.log(`✗ the gallery: ${failures.length} of ${checked} specimens differ (see ${join(coreDir, 'report.html')})`);
  for (const f of failures) console.log(`  ${f}`);
  process.exit(1);
}
const worst = rows.reduce((m, r) => (r.shape > m.shape ? r : m), rows[0]);
console.log(`✓ the gallery: ${checked} specimens, core and web agree in size, extent, colour and shape (furthest apart: ${worst.key}, ${worst.shape.toFixed(1)} of 8)`);

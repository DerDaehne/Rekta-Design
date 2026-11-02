#!/usr/bin/env node
// Clicks through the showcase (_site/) in Chromium and checks the behaviour contract: places, back and focus, the
// turnstile, the panorama's keys and wheel, Metro selection, tables, filters, the split, menus, the palette, toasts,
// the banner, forms and their guard, prefs, sheets, a phone, and reduced motion.
// Needs Playwright, which the repository does not depend on:
//   PLAYWRIGHT=/path/to/playwright/index.mjs node test/smoke.mjs     (default: import 'playwright')
// Run node scripts/build-site.mjs first. Screenshots go to site-shots/. Exit 1 on any failed check or page error.

import { createReadStream, mkdirSync, statSync } from 'node:fs';
import { createServer } from 'node:http';
import { dirname, extname, join, normalize, sep } from 'node:path';
import { fileURLToPath } from 'node:url';

const { chromium } = await import(process.env.PLAYWRIGHT || 'playwright');
const root = join(dirname(fileURLToPath(import.meta.url)), '..');
const out = join(root, '_site'), shots = join(root, 'site-shots');
mkdirSync(shots, { recursive: true });

const types = { '.html': 'text/html', '.css': 'text/css', '.js': 'text/javascript', '.json': 'application/json', '.woff2': 'font/woff2' };
const server = createServer((req, res) => {
  try {
    let file = join(out, normalize(decodeURIComponent(new URL(req.url, 'http://x').pathname)));
    if (file !== out && !file.startsWith(out + sep)) throw new Error('outside');
    if (statSync(file).isDirectory()) file = join(file, 'index.html');
    res.writeHead(200, { 'content-type': types[extname(file)] || 'application/octet-stream' });
    createReadStream(file).pipe(res);
  } catch { res.writeHead(404).end(); }
}).listen(0, '127.0.0.1');
await new Promise((r) => server.on('listening', r));
const base = `http://127.0.0.1:${server.address().port}/`;

const browser = await chromium.launch();
const errors = [], failures = [];
let passed = 0;
function ok(cond, what, detail = '') { if (cond) passed += 1; else failures.push(`${what}${detail !== '' ? ` (${detail})` : ''}`); }
async function open(opts = {}, path = '') {
  const ctx = await browser.newContext({ viewport: { width: 1440, height: 900 }, colorScheme: 'light', reducedMotion: 'no-preference', ...opts });
  const p = await ctx.newPage();
  if (process.env.SMOKE_CPU) await (await ctx.newCDPSession(p)).send('Emulation.setCPUThrottlingRate', { rate: +process.env.SMOKE_CPU });   /* a busy runner, here */
  const tag = `${opts.viewport ? opts.viewport.width : 1440}${opts.reducedMotion === 'reduce' ? '/reduced' : ''}`;
  p.on('pageerror', (e) => errors.push(`${tag}: ${e.message}`));
  p.on('console', (m) => { if (m.type() === 'error') errors.push(`${tag} console: ${m.text()}`); });
  await p.goto(base + path);
  await p.waitForTimeout(1400);
  return p;
}
const visible = (p, place) => p.evaluate((n) => !document.querySelector(`[data-rk-place="${n}"]`).hidden, place);
const focused = (p) => p.evaluate(() => { const a = document.activeElement; return a.id || a.dataset.name || a.className || a.tagName; });
const text = async (p, sel) => { const el = await p.$(sel); return el ? (await el.textContent()).trim() : '(none)'; };
const toast = (p) => text(p, '.rk-toast:not([data-rk-phase]) .rk-toast-msg');
const isOpen = (p, sel) => p.evaluate((s) => document.querySelector(s).matches(':popover-open'), sel);
// Waits until cond() holds (polling, up to ms) and says whether it did: a busy runner gets there, just later.
const until = async (p, cond, ms = 2000) => {
  for (const end = Date.now() + ms; Date.now() < end; await p.waitForTimeout(50)) if (await cond()) return true;
  return !!(await cond());
};
const sheetTitle = (p) => p.evaluate(() => { const d = [...document.querySelectorAll('dialog')].find((x) => x.open); return d ? d.querySelector('h2').textContent : null; });
const go = async (p, id) => { await p.click(`#nav a[href="#${id}"]`); await p.waitForTimeout(900); };
const TURN = 1300;

// Rest looks: no horizontal page overflow in either theme, on a desktop, a phone and a short window.
for (const [w, h, scheme] of [[1440, 900, 'light'], [1440, 900, 'dark'], [390, 844, 'light'], [390, 844, 'dark'], [844, 390, 'light']]) {
  const p = await open({ viewport: { width: w, height: h }, colorScheme: scheme, hasTouch: w < 760 || h < 481 });
  ok(!(await p.evaluate(() => document.documentElement.scrollWidth > innerWidth + 1)), `${w}x${h} ${scheme}: no horizontal overflow`);
  if (w === 1440) {
    // Secondary text composed into a status fill (a meta, facts, an age, a stale value) keeps AA: --check sees token
    // pairs only, so the selectors that combine them are measured here.
    const worst = await p.evaluate(() => {
      const lum = (c) => { const [r, g, b] = c.match(/[\d.]+/g).slice(0, 3).map((v) => { v /= 255; return v <= 0.04045 ? v / 12.92 : ((v + 0.055) / 1.055) ** 2.4; }); return 0.2126 * r + 0.7152 * g + 0.0722 * b; };
      const ratio = (a, b) => { const [x, y] = [lum(a), lum(b)].sort((m, n) => n - m); return (x + 0.05) / (y + 0.05); };
      let low = { r: 99, what: '' };
      for (const st of ['info', 'success', 'warning', 'error']) {
        const host = document.createElement('div');
        host.innerHTML = `<div class="rk-tile" data-status="${st}"><span class="rk-meta">m</span><span class="rk-facts">f</span><span class="rk-age">a</span><span data-rk-live="stale"><b data-rk-value>v</b></span></div>`
          + `<div class="rk-banner" data-status="${st}"><span class="rk-meta">m</span><span class="rk-facts">f</span></div>`;
        document.body.append(host);
        for (const fill of host.children) for (const el of fill.querySelectorAll('*')) {
          const r = ratio(getComputedStyle(el).color, getComputedStyle(fill).backgroundColor);
          if (r < low.r) low = { r, what: `${el.className || el.tagName} on ${fill.className} ${st}` };
        }
        host.remove();
      }
      return low;
    });
    ok(worst.r >= 4.5, `${scheme}: secondary text on every status fill clears AA`, `${worst.what} ${worst.r.toFixed(2)}:1`);
    // A tag in the plain (ink) toast: its word reads on the ink, its neutral square holds 3:1, every square has an edge (#139).
    const inToast = await p.evaluate(() => {
      const lum = (c) => { const [r, g, b] = c.match(/[\d.]+/g).slice(0, 3).map((v) => { v /= 255; return v <= 0.04045 ? v / 12.92 : ((v + 0.055) / 1.055) ** 2.4; }); return 0.2126 * r + 0.7152 * g + 0.0722 * b; };
      const ratio = (a, b) => { const [x, y] = [lum(a), lum(b)].sort((m, n) => n - m); return (x + 0.05) / (y + 0.05); };
      const t = document.createElement('div'); t.className = 'rk-toast';
      t.innerHTML = '<p class="rk-toast-msg">Build 4182 <span class="rk-tag">idle</span> <span class="rk-tag" data-status="warning">waiting</span> <span class="rk-tag" data-status="success">done</span></p>';
      document.querySelector('.rk').append(t);
      const ink = getComputedStyle(t).backgroundColor, tags = [...t.querySelectorAll('.rk-tag')];
      const out = {
        word: Math.min(...tags.map((x) => ratio(getComputedStyle(x).color, ink))),
        square: ratio(getComputedStyle(tags[0], '::before').backgroundColor, ink),
        edges: tags.every((x) => getComputedStyle(x, '::before').boxShadow.startsWith(getComputedStyle(t).color)),
      };
      t.remove();
      return out;
    });
    ok(inToast.word >= 4.5 && inToast.square >= 3 && inToast.edges, `${scheme}: a tag in an ink toast: its word reads, its square shows`, JSON.stringify(inToast));
  }
  await p.screenshot({ path: join(shots, `rest-${w}x${h}-${scheme}.png`) });
  await p.close();
}

// More contrast and less transparency (#156): from the system (emulated) and from the prefs. Every text pair measured
// at 7:1, each status tile framed in its full colour, glass opaque and nothing blurred; text on glass (#130) over
// white and black.
{
  const measure = (p) => p.evaluate(() => {
    const rgba = (c) => c.match(/[\d.]+/g).map(Number);
    const lum = (c) => { const [r, g, b] = rgba(c).slice(0, 3).map((v) => { v /= 255; return v <= 0.04045 ? v / 12.92 : ((v + 0.055) / 1.055) ** 2.4; }); return 0.2126 * r + 0.7152 * g + 0.0722 * b; };
    const ratio = (a, b) => { const [x, y] = [lum(a), lum(b)].sort((m, n) => n - m); return (x + 0.05) / (y + 0.05); };
    const over = (top, under) => { const t = rgba(top), u = rgba(under), a = t[3] ?? 1; return `rgb(${[0, 1, 2].map((i) => t[i] * a + u[i] * (1 - a)).join(',')})`; };
    const host = document.createElement('div');
    host.className = 'rk';
    host.innerHTML = ['bg', 'surface'].map((g) => `<div style="background:var(--rk-${g})"><span class="rk-meta">m</span><span class="rk-facts">f</span><span class="rk-hint">h</span><span style="color:var(--rk-text-contrast-medium)">p</span><span class="rk-tag">t</span></div>`).join('')
      + ['info', 'success', 'warning', 'error'].map((st) => `<div class="rk-tile" data-status="${st}"><span class="rk-tile-word">w</span><span class="rk-meta">m</span></div>`).join('')
      + '<div class="rk-tag rk-tag--soft" data-status="error">soft</div><button class="rk-btn rk-btn--primary">ok</button><button class="rk-btn" data-status="error">delete</button>';
    document.body.append(host);
    let low = { r: 99, what: '' };
    for (const el of host.querySelectorAll('span, .rk-tag--soft, .rk-btn')) {
      let fill = el; while (fill && rgba(getComputedStyle(fill).backgroundColor)[3] === 0) fill = fill.parentElement;
      const bgc = fill ? getComputedStyle(fill).backgroundColor : 'rgb(255,255,255)', r = ratio(getComputedStyle(el).color, bgc);
      if (r < low.r) low = { r, what: `${el.className || el.textContent} on ${fill ? fill.className || fill.getAttribute('style') : 'body'}` };
    }
    const tiles = [...host.querySelectorAll('.rk-tile')].map((t) => getComputedStyle(t).boxShadow);
    host.remove();
    const bar = document.querySelector('#bar'), cs = getComputedStyle(bar), glass = cs.backgroundColor;
    const onGlass = [...bar.querySelectorAll('.rk-ab-status, .rk-ab')].filter((e) => e.getClientRects().length).flatMap((e) => ['rgb(255,255,255)', 'rgb(0,0,0)'].map((b) => ratio(getComputedStyle(e).color, over(glass, b))));
    return { low, framed: tiles.every((s) => /2px/.test(s) && s.includes('inset')), tiles, opaque: (rgba(glass)[3] ?? 1) === 1, blur: cs.backdropFilter, glassLow: Math.min(...onGlass) };
  });
  for (const scheme of ['light', 'dark']) {
    const c = await open({ colorScheme: scheme, contrast: 'more' });
    const m = await measure(c);
    ok(m.low.r >= 7, `${scheme}, the system asks for more contrast: every text pair holds 7:1`, `${m.low.what} ${m.low.r.toFixed(2)}:1`);
    ok(m.framed, `${scheme}, more contrast: every status tile has a 2px frame`, m.tiles.join(' | '));
    ok(m.opaque && m.glassLow >= 7, `${scheme}, more contrast: glass is opaque, its text at 7:1`, `${m.opaque} ${m.glassLow.toFixed(2)}`);
    await c.evaluate(() => Rekta.prefs.set('contrast', 'normal', { remember: false })); await c.waitForTimeout(100);
    const n = await measure(c);
    ok(n.low.r < 7 && !n.framed && !n.opaque && n.glassLow >= 4.5, `${scheme}: contrast "normal" keeps the design's colours against the system's setting; text on glass over white and black holds 4.5:1`, `${n.low.r.toFixed(2)} ${n.framed} ${n.opaque} ${n.glassLow.toFixed(2)}`);
    await c.close();
  }
  const c = await open();
  const plain = await measure(c);
  ok(!plain.framed && !plain.opaque && /blur/.test(plain.blur), 'in the normal case nothing changes: no frame, glass with its blur', JSON.stringify(plain));
  await c.evaluate(() => { Rekta.prefs.set('contrast', 'more', { remember: false }); Rekta.prefs.set('transparency', 'reduce', { remember: false }); });
  await c.waitForTimeout(400);
  const more = await measure(c);
  ok(more.low.r >= 7 && more.framed, 'contrast "more" from the prefs: 7:1 and the frames', `${more.low.what} ${more.low.r.toFixed(2)}`);
  ok(more.opaque && /^(none)?$/.test(more.blur) && await c.evaluate(() => { const s = document.createElement('div'); s.className = 'rk-scrim'; document.body.append(s); const b = getComputedStyle(s).backdropFilter; s.remove(); return b === 'none'; }),
    'transparency "reduce": glass is opaque, the bar and the scrim do not blur', more.blur);
  await c.evaluate(() => { Rekta.prefs.set('contrast', 'system', { remember: false }); Rekta.prefs.set('transparency', 'system', { remember: false }); });
  await c.close();
}

const p = await open();
ok(await p.evaluate(() => getComputedStyle(document.querySelector('#pan')).display === 'flex'), 'desktop panorama is sideways');
ok(await p.evaluate(() => document.querySelector('#back').hidden), 'app bar back hidden at home');
ok(await p.evaluate(() => { const n = document.querySelector('#nav'); return n.scrollHeight <= n.clientHeight + 1; }), 'the section strip is not cut');

// Generated from the tokens, measured live.
const count = await p.evaluate(() => JSON.parse(document.querySelector('#tokens-data').textContent).length);
ok((await text(p, '#token-count')) === String(count), 'the tokens tile counts the tokens');
ok(await p.evaluate(() => [...document.querySelectorAll('[data-ratio]')].every((b) => /^\d+\.\d : 1$/.test(b.textContent))), 'contrast is measured live');
ok(!(await p.evaluate(() => /\{\{|<!--@/.test(document.body.innerHTML))), 'every placeholder was filled at build time');
// The pivot's size change and the live pulse take the in-out curve from its token, not from the CSS keyword.
{
  const c = await p.evaluate(() => ({ pivot: getComputedStyle(document.querySelector('.rk-pivot > a')).transitionTimingFunction.split(', cubic')[0], pulse: getComputedStyle(document.querySelector('.rk-pulse')).animationTimingFunction }));
  ok(c.pivot === 'cubic-bezier(0.42, 0, 0.58, 1)' && c.pulse === 'cubic-bezier(0.42, 0, 0.58, 1)', 'the pivot size change and the pulse use ease-in-out from its token', JSON.stringify(c));
}
ok((await p.evaluate(() => { const seen = new Set(), twice = []; document.querySelectorAll('[id]').forEach((e) => { if (seen.has(e.id)) twice.push(e.id); seen.add(e.id); }); return twice.join(); })) === '', 'every id is unique');

// Tile → place: the turnstile, the address, the title, focus; the crumb back.
await p.click('#tile-notes');
// The turn takes its angle and curve from the tokens (turnstile-out-angle, ease-turn-out), not from literals. Both are
// read in one look while the parts leave: the leaving lasts about 0.4s, and the time a click takes to return varies
// with the page and the runner, so a fixed wait and a second round trip could land after it.
{
  const t = await p.evaluate(() => new Promise((res) => {
    const end = performance.now() + 1000;
    (function look() {
      const u = document.querySelector('[data-rk-turn="out"]');
      if (!u && performance.now() < end) { requestAnimationFrame(look); return; }
      const s = getComputedStyle(document.documentElement);
      res({ n: document.querySelectorAll('[data-rk-turn]').length, ease: u && getComputedStyle(u).animationTimingFunction, want: s.getPropertyValue('--rk-ease-turn-out').trim(), angle: s.getPropertyValue('--rk-turnstile-out-angle').trim() });
    }());
  }));
  ok(t.n > 3, 'leaving units turn', t.n);
  ok(t.ease === t.want && t.want === 'cubic-bezier(0.35, 0, 0.65, 0.6)' && t.angle === '60deg', 'a leaving part turns by the token angle on the token curve', JSON.stringify(t));
}
await p.screenshot({ path: join(shots, 'turn-mid.png') });
await p.waitForTimeout(TURN);
ok(await visible(p, 'notes') && !(await visible(p, 'home')), 'the release notes show, home hides');
ok((await p.evaluate(() => location.hash)) === '#notes' && (await p.title()) === 'Rekta 0.2 · Rekta Design', 'address and title name the place', await p.title());
ok((await focused(p)) === 'notes-title', 'focus on the place title', await focused(p));
await p.click('[data-rk-place=notes] .rk-crumb'); await p.waitForTimeout(TURN);
ok(await visible(p, 'home') && (await p.evaluate(() => location.hash)) === '', 'the crumb goes back home');
ok((await focused(p)) === 'tile-notes', 'focus back on the opening tile', await focused(p));
// The tapped part carries its title into the place it opens (#145): the title stands while the rest turns away, then
// travels on ease-pivot-glide over duration-moderate to the new place's title and lands on its pixels; the new place's
// parts arrive from the middle of the travel on. Back runs it the other way. Nothing is left behind, and focus lands.
{
  const copies = () => p.evaluate(() => [...document.body.children].filter((c) => c.getAttribute('aria-hidden') === 'true' && c.style.position === 'fixed').map((c) => c.textContent.trim()));
  await p.click('#tile-notes');
  let leaving = null;
  await until(p, async () => (leaving = await p.evaluate(() => ({ source: getComputedStyle(document.querySelector('#tile-notes [data-rk-carry]')).opacity }))) && leaving.source === '0', 800);
  const standing = await copies();
  let flight = null;
  await until(p, async () => (flight = await p.evaluate(() => {
    const t = document.querySelector('#notes-title'), copy = [...document.body.children].find((c) => c.style.position === 'fixed' && c.textContent.trim() === 'Rekta 0.2');
    if (!copy) return null;
    const a = copy.getAnimations()[0], r = t.getBoundingClientRect(), delays = [...document.querySelectorAll('[data-rk-place=notes] [data-rk-turn]')].map((u) => parseFloat(u.style.getPropertyValue('--_rk-turn-delay')));
    return { at: [Math.round(parseFloat(copy.style.left) - r.left), Math.round(parseFloat(copy.style.top) - r.top)].join(), lands: a.effect.getKeyframes().at(-1).transform, d: a.effect.getTiming().duration, ease: a.effect.getTiming().easing, title: getComputedStyle(t).opacity, first: Math.min(...delays) };
  })), 2000);
  const settled = () => p.evaluate(() => ({ title: getComputedStyle(document.querySelector('#notes-title')).opacity, source: document.querySelector('#tile-notes [data-rk-carry]').style.opacity, left: [...document.body.children].filter((c) => c.style.position === 'fixed' && c.getAttribute('aria-hidden') === 'true').length }));
  let after = null;
  await until(p, async () => (after = await settled()) && after.title === '1' && !after.left, 3000);
  ok(leaving.source === '0' && standing.includes('0.2'), 'leaving, the tapped tile\'s title stands while its tile turns away', JSON.stringify({ leaving, standing }));
  ok(flight && flight.at === '0,0' && flight.lands === 'none' && flight.d === 400 && flight.ease === 'cubic-bezier(0.65, 0, 0.35, 1)' && flight.title === '0' && flight.first >= 200, 'the title travels to the new place\'s title on ease-pivot-glide and lands on its pixels; the rest arrive from the middle', JSON.stringify(flight));
  ok(after.title === '1' && after.source === '' && !after.left && (await focused(p)) === 'notes-title', 'once landed the title is the place\'s own and has focus; nothing is left behind', JSON.stringify(after));
  await p.waitForTimeout(TURN);
  await p.click('[data-rk-place=notes] .rk-crumb');
  let back = [];
  await until(p, async () => (back = await copies()).includes('Rekta 0.2'), 1500);
  let home = null;
  await until(p, async () => (home = await p.evaluate(() => ({ tile: getComputedStyle(document.querySelector('#tile-notes [data-rk-carry]')).opacity, title: document.querySelector('#notes-title').style.opacity, left: [...document.body.children].filter((c) => c.style.position === 'fixed' && c.getAttribute('aria-hidden') === 'true').length }))) && home.tile === '1' && !home.left, 3000);
  ok(back.includes('Rekta 0.2') && home.tile === '1' && home.title === '' && !home.left && (await visible(p, 'home')) && (await focused(p)) === 'tile-notes', 'back, the place\'s title travels into the tile that opened it, and focus returns to the tile', JSON.stringify({ back, home }));
  await p.waitForTimeout(TURN);
}

// Browser back and forward; Escape; a navigation during a turn.
await p.click('#tile-tokens'); await p.waitForTimeout(TURN);
ok(await visible(p, 'tokens'), 'the tokens place shows');
await p.screenshot({ path: join(shots, 'tokens.png') });
await p.goBack(); await p.waitForTimeout(TURN);
ok(await visible(p, 'home'), 'browser back shows home');
await p.goForward(); await p.waitForTimeout(TURN);
ok(await visible(p, 'tokens'), 'browser forward shows the tokens');
await p.keyboard.press('Escape'); await p.waitForTimeout(TURN);
ok(await visible(p, 'home') && (await focused(p)) === 'tile-tokens', 'Escape goes back, focus on the tile', await focused(p));
await p.click('#tile-notes'); await p.waitForTimeout(80);
await p.goBack(); await p.waitForTimeout(TURN + 300);
ok(await visible(p, 'home') && (await p.evaluate(() => document.querySelectorAll('[data-rk-place]:not([hidden])').length === 1)), 'back during a turn lands home, one place shows');

// Panorama: keys, the pivot, the wheel; the sections-seen tile.
await p.focus('#pan');
await p.keyboard.press('End'); await p.waitForTimeout(900);
ok((await p.getAttribute('#nav a[href="#start"]', 'aria-current')) === 'location', 'End jumps to the last section');
await p.keyboard.press('Home'); await p.waitForTimeout(900);
ok((await p.getAttribute('#nav a[href="#overview"]', 'aria-current')) === 'location', 'Home jumps to the first section');
await go(p, 'lists');
ok((await focused(p)) === 'lists-title' && (await p.evaluate(() => location.hash)) === '', 'a pivot jump focuses the heading, changes no address', await focused(p));
ok(+(await text(p, '#seen-n')) >= 2, 'sections seen counts', await text(p, '#seen-n'));
const strip = () => p.evaluate(() => { const n = document.querySelector('#nav'), a = n.querySelector('[aria-current]'), b = a.previousElementSibling, w = n.getBoundingClientRect(); return { active: a.getBoundingClientRect().left >= w.left && a.getBoundingClientRect().right <= w.right, before: b.getBoundingClientRect().left >= w.left, left: Math.round(a.getBoundingClientRect().left) }; });
const s1 = await strip();
ok(s1.active && s1.before && s1.left > 200, 'the strip glides by place: the active title whole, the one before it in reach', JSON.stringify(s1));
await go(p, 'overview');
const x0 = await p.evaluate(() => document.querySelector('#pan').scrollLeft);
await p.mouse.move(700, 500); await p.mouse.wheel(0, 300); await p.waitForTimeout(300);
ok((await p.evaluate(() => document.querySelector('#pan').scrollLeft)) > x0, 'a vertical wheel moves the panorama sideways');
ok(+(await text(p, '[data-live=fps] [data-v]')) > 0, 'the live tile measures frames', await text(p, '[data-live=fps] [data-v]'));

// Tile grids (#129): one tab stop; the arrows by position, the shared row or column first, a tie to the better-aligned
// one, on into the next grid of the panorama, no wrap; Home End; letters jump (#134). A fixture with a known layout.
{
  const at = () => p.evaluate(() => document.activeElement.textContent);
  ok(await p.evaluate(() => { const g = document.querySelector('#tile-tokens').closest('.rk-tiles'); return g.hasAttribute('data-rk-grid') && [...g.querySelectorAll('a.rk-tile')].filter((t) => t.tabIndex === 0).length === 1; }),
    'start(): the overview\'s grid of tiles that open is one tab stop');
  await p.evaluate(() => {
    const pan = document.createElement('div'); pan.className = 'rk-panorama'; pan.id = 't-pan';
    pan.style.cssText = 'display:block;position:fixed;inset:0 auto auto 0;width:800px;height:400px;padding:0;overflow:visible;z-index:999;background:var(--rk-bg)';
    const grid = (left, top, names) => `<div class="rk-tiles" style="position:absolute;left:${left}px;top:${top}px;display:grid;grid-template-columns:repeat(3,100px);grid-auto-rows:100px;gap:8px">${names.map((n, i) => `<a class="rk-tile${n === 'alpha' ? ' rk-w2' : ''}" href="#" id="t-${n}">${n}</a>`).join('')}</div>`;
    pan.innerHTML = grid(0, 0, ['alpha', 'beta', 'bravo', 'charlie', 'delta']) + grid(400, 50, ['echo', 'foxtrot']);
    pan.addEventListener('click', (e) => e.preventDefault());
    document.querySelector('.rk').append(pan);
    window.tGrids = [...pan.querySelectorAll('.rk-tiles')].map((g) => Rekta.grid(g));
  });
  ok(await p.evaluate(() => [...document.querySelectorAll('#t-pan .rk-tiles')].every((g) => [...g.children].filter((t) => t.tabIndex === 0).length === 1)), 'grid(): each grid one tab stop');
  await p.focus('#t-alpha');
  const keys = async (...k) => { for (const x of k) await p.keyboard.press(x); };
  await keys('ArrowRight'); const r1 = await at();
  await keys('ArrowDown'); const d1 = await at();
  await keys('ArrowLeft', 'ArrowLeft'); const l2 = await at();
  await keys('ArrowLeft'); const edge = await at();
  await keys('ArrowUp'); const up = await at();
  ok(r1 === 'beta' && d1 === 'delta' && l2 === 'bravo' && edge === 'bravo' && up === 'alpha', 'the arrows: → beta, ↓ delta, ← ← bravo, ← at the edge stays, ↑ alpha (a wide tile above)', [r1, d1, l2, edge, up].join(' '));
  await keys('ArrowRight', 'ArrowRight'); const across = await at();
  const stops = await p.evaluate(() => [...document.querySelectorAll('#t-pan a')].filter((t) => t.tabIndex === 0).map((t) => t.textContent).join());
  ok(across === 'echo' && stops === 'beta,echo', '→ at a grid\'s end goes on into the next grid of the panorama; each keeps its tab stop', `${across} / ${stops}`);
  await keys('ArrowDown'); const below = await at();
  await keys('ArrowLeft'); const tie = await at();
  ok(below === 'echo' && tie === 'beta', '↓ with nothing below stays; ← on a tie takes the better-aligned tile', `${below} ${tie}`);
  await keys('End'); const end = await at(); await keys('Home'); const home = await at();
  ok(end === 'delta' && home === 'alpha', 'Home and End: the grid\'s first and last tile', `${end} ${home}`);
  await keys('b'); const b1 = await at(); await keys('r'); const br = await at();
  await p.waitForTimeout(1100); await keys('e'); const e1 = await at();
  ok(b1 === 'beta' && br === 'bravo' && e1 === 'echo', 'letters jump: b to beta, b r to bravo, after a pause e to echo in the next grid', `${b1} ${br} ${e1}`);
  await p.focus('#t-alpha'); await keys('Tab');
  ok(await p.evaluate(() => !document.activeElement.closest('#t-pan .rk-tiles:first-child')), 'Tab leaves the grid', await p.evaluate(() => document.activeElement.id || document.activeElement.tagName));
  await p.evaluate(() => { window.tGrids.forEach((g) => g.destroy()); document.querySelector('#t-pan').remove(); });
}

// Tiles: a button tile opens a sheet.
await go(p, 'tiles');
await p.click('#tile-presses'); await p.waitForTimeout(300);
ok((await sheetTitle(p)) === 'A button tile opens a sheet.' && (await text(p, '#presses')) !== '0', 'a button tile opens a sheet; presses count');
await p.keyboard.press('Escape'); await p.waitForTimeout(300);

// Panorama section: a pivot with facets.
await go(p, 'panorama');
await p.click('#tab-2'); await p.waitForTimeout(500);
ok(await p.evaluate(() => !document.querySelector('#facet-2').hidden && document.querySelector('#facet-0').hidden), 'a facet shows its panel');
ok(/sideways · 1440/.test(await text(p, '#env-class')), 'the environment reads out', await text(p, '#env-class'));
// A pivot whose facets' container is wider than size.pivot-wide shows them side by side, its strip a jump list.
const wide = await p.evaluate(async () => {
  const box = document.createElement('div');
  box.style.cssText = 'position:fixed;left:0;top:0;width:1300px;z-index:-1';
  box.innerHTML = '<div class="rk-pivot" role="tablist" aria-label="w" style="--rk-pivot-wide: 900px">'
    + [0, 1, 2, 3].map((i) => `<button role="tab" id="wt${i}" aria-controls="wf${i}" aria-selected="${i === 0}">facet ${i}</button>`).join('') + '</div>'
    + '<div id="wfs">' + [0, 1, 2, 3].map((i) => `<div role="tabpanel" id="wf${i}" ${i ? 'hidden' : ''}><p>${'text '.repeat(40)}</p></div>`).join('') + '</div>';
  document.body.append(box);
  // As a framework would: it binds hidden to the facet it holds, set from onSelect (#36).
  let held = 0;
  const pv = Rekta.pivot(box.firstChild, { onSelect: (i) => { held = i; [...box.querySelectorAll('[role=tabpanel]')].forEach((x, k) => { x.hidden = k !== held; }); } });
  const wfs = box.querySelector('#wfs'), wait = (ms) => new Promise((r) => setTimeout(r, ms));
  const shown = () => [...wfs.children].filter((x) => getComputedStyle(x).display !== 'none').length;
  const out = { wide: pv.wide, shown: shown(), flex: getComputedStyle(wfs).display };
  box.querySelector('#wt2').click(); await wait(700);
  out.jumped = wfs.scrollLeft > 0 && box.querySelector('#wt2').getAttribute('aria-selected') === 'true';
  box.querySelector('#wt2').focus(); box.querySelector('#wt2').dispatchEvent(new KeyboardEvent('keydown', { key: 'Tab', bubbles: true, cancelable: true }));
  out.tabbed = document.activeElement && document.activeElement.id === 'wf2';
  wfs.scrollTo({ left: 0, behavior: 'instant' }); await wait(200);
  out.followed = box.querySelector('#wt0').getAttribute('aria-selected') === 'true';
  out.bound = shown() === 4;                            /* the framework hid the others; wide still shows them all */
  const before = held;
  pv.select(3, { silent: true }); await wait(50);
  out.silent = held === before && box.querySelector('#wt3').getAttribute('aria-selected') === 'true';   /* #37 */
  box.style.width = '700px'; await wait(200);
  out.narrow = !pv.wide && shown() === 1;
  // #38: an override in calc() reads as the length it is.
  box.firstChild.style.setProperty('--rk-pivot-wide', 'calc(var(--rk-tile-unit) * 4)'); box.style.width = '650px'; pv.layout(); await wait(100);
  out.calc = pv.wide;
  box.style.width = '500px'; await wait(150);
  // #39: a title's count changes: measured again, so the gap stays even.
  const t1 = box.querySelector('#wt1'); t1.textContent = 'facet 1 · 128 new things'; await wait(150);
  const r = [...box.querySelectorAll('[role=tab]')].map((e) => e.getBoundingClientRect());
  out.gaps = r.slice(1).map((x, i) => Math.round(x.left - r[i].right)).join(',');
  pv.destroy(); box.remove();
  return out;
});
ok(wide.wide && wide.shown === 4 && wide.flex === 'flex', 'a pivot above size.pivot-wide shows every facet side by side', JSON.stringify(wide));
ok(wide.jumped && wide.followed, 'its strip jumps to a facet and follows the scroll', JSON.stringify(wide));
ok(wide.tabbed, 'Tab from the strip goes into the facet chosen', JSON.stringify(wide));
ok(wide.narrow, 'below it, one facet at a time again', JSON.stringify(wide));
ok(wide.bound, 'wide, panels show even with hidden bound by a framework (data-rk-reveal)', JSON.stringify(wide));
ok(wide.silent, 'select(i, { silent }) calls no onSelect', JSON.stringify(wide));
ok(wide.calc, 'a --rk-pivot-wide written in calc() is read as its length', JSON.stringify(wide));
ok(new Set(wide.gaps.split(',')).size === 1, 'a title whose words change is measured again: even gaps', wide.gaps);
// strip(): a strip of links a router marks (#41); arrive() is exported (#42).
// Facets that come and go, by name; scroll offsets kept and put back; a wide row below a title block.
const facets = await p.evaluate(async () => {
  const wait = (t) => new Promise((r) => setTimeout(r, t)), frames = () => new Promise((r) => requestAnimationFrame(() => requestAnimationFrame(r)));
  const box = document.createElement('div'); box.style.cssText = 'position:fixed;left:0;top:0;width:600px;z-index:-1';
  box.innerHTML = '<div class="rk-pivot" role="tablist" aria-label="f">' + ['all', 'questions', 'drafts'].map((n, i) => `<button role="tab" id="ft-${n}" data-facet="${n}" aria-controls="fp-${n}" aria-selected="${i === 0}">${n}</button>`).join('') + '</div>'
    + '<div>' + ['all', 'questions', 'drafts'].map((n, i) => `<div role="tabpanel" id="fp-${n}" ${i ? 'hidden' : ''}>${n}</div>`).join('') + '</div>';
  document.body.append(box);
  const told = [];
  const pv = Rekta.pivot(box.firstChild, { onSelect: (i, t) => told.push(t.dataset.facet) });
  pv.select('questions', { silent: true }); await wait(50);
  const out = { byName: pv.chosen === 'questions' && !box.querySelector('#fp-questions').hidden };
  box.querySelector('#ft-questions').remove(); box.querySelector('#fp-questions').remove(); await frames();
  const tabs = () => [...box.querySelectorAll('[role=tab]')];
  out.gone = { selected: tabs().filter((t) => t.getAttribute('aria-selected') === 'true').map((t) => t.dataset.facet).join(), stops: tabs().filter((t) => t.tabIndex === 0).length, told: told.join(), shown: !box.querySelector('#fp-drafts').hidden };
  const t = document.createElement('button'); t.setAttribute('role', 'tab'); t.id = 'ft-errors'; t.dataset.facet = 'errors'; t.setAttribute('aria-controls', 'fp-errors'); t.textContent = 'errors';
  const pnl = document.createElement('div'); pnl.setAttribute('role', 'tabpanel'); pnl.id = 'fp-errors'; pnl.textContent = 'errors';
  box.querySelector('#fp-all').parentElement.append(pnl); box.firstChild.append(t); await frames();
  out.added = t.tabIndex === -1 && t.getAttribute('aria-selected') === 'false' && pnl.hidden && tabs().filter((x) => x.tabIndex === 0).length === 1;
  pv.destroy(); box.remove();
  // scrollState: every box and the window, back where they were.
  const place = document.createElement('div'); place.style.cssText = 'position:fixed;left:0;top:0;width:300px;height:200px;overflow:auto;z-index:-1';
  place.innerHTML = '<div data-rk-scroll="a" style="height:100px;overflow:auto"><div style="height:600px"></div></div><div style="height:900px"></div>';
  document.body.append(place);
  place.scrollTop = 120; place.querySelector('[data-rk-scroll]').scrollTop = 250;
  const st = Rekta.scrollState(place).capture(); place.scrollTop = 0; place.querySelector('[data-rk-scroll]').scrollTop = 0;
  Rekta.scrollState(place).restore(JSON.parse(JSON.stringify(st)));
  out.scroll = place.scrollTop === 120 && place.querySelector('[data-rk-scroll]').scrollTop === 250 && st.at['@a'][1] === 250;
  place.remove();
  return out;
});
ok(facets.byName, 'a facet chosen by name', JSON.stringify(facets));
ok(facets.gone.selected === 'drafts' && facets.gone.stops === 1 && facets.gone.told === 'drafts' && facets.gone.shown, 'the chosen facet goes: its neighbour is chosen, one tab stop stays', JSON.stringify(facets.gone));
ok(facets.added, 'a facet that comes takes no tab stop, its panel hidden', JSON.stringify(facets));
ok(facets.scroll, 'scrollState captures every scrolled box and puts it back', JSON.stringify(facets));
// The first paint of facets that may go wide: .rk-facets is a container, so before the script the row is laid out from
// the same width pivot() measures. In a 900px pane of a 1440px window (a media query would say wide) both say narrow.
const fp = await p.evaluate(async () => {
  const frames = () => new Promise((r) => requestAnimationFrame(() => requestAnimationFrame(r)));
  const out = {};
  for (const w of [900, 1400]) {
    const host = document.createElement('div'); host.style.cssText = `position:absolute;left:0;top:0;width:${w}px;z-index:-1`;
    host.innerHTML = '<div class="rk-pivot" role="tablist" aria-label="fp">' + [0, 1, 2].map((i) => `<button role="tab" id="fpt${w}-${i}" aria-controls="fpp${w}-${i}" aria-selected="${i === 0}">facet ${i}</button>`).join('') + '</div>'
      + '<div class="rk-facets"><div>' + [0, 1, 2].map((i) => `<section role="tabpanel" id="fpp${w}-${i}"${i ? ' hidden data-rk-reveal' : ''}><p>${'text '.repeat(60)}</p></section>`).join('') + '</div></div>';
    document.querySelector('.rk').append(host); await frames();
    const shown = () => [...host.querySelectorAll('[role=tabpanel]')].filter((x) => x.getClientRects().length).map((x) => Math.round(x.getBoundingClientRect().left)).join();
    const before = shown(), pv = Rekta.pivot(host.querySelector('.rk-pivot')); await frames();
    out[w] = { before, after: shown(), wide: pv.wide };
    pv.destroy(); host.remove();
  }
  return out;
});
ok(fp[900].before.split(',').length === 1 && !fp[900].wide && fp[900].after === fp[900].before, 'in a narrow container the first paint and pivot() both show one facet', JSON.stringify(fp[900]));
ok(fp[1400].before.split(',').length === 3 && fp[1400].wide && fp[1400].after === fp[1400].before, 'in a wide container the first paint lays the facets out where pivot() does', JSON.stringify(fp[1400]));
// Geometry the docs fix: inactive pivot titles stand on the active one's baseline; a sparkline's line is 2px at any
// tile width; a status region spaces its kicker, figures and actions.
const geo = await p.evaluate(async () => {
  const frames = () => new Promise((r) => requestAnimationFrame(() => requestAnimationFrame(r)));
  const host = document.createElement('div'); host.style.cssText = 'position:absolute;left:0;top:0;width:900px;z-index:-1';
  host.innerHTML = '<div class="rk-pivot" role="tablist" aria-label="g">' + ['all', 'questions', 'drafts'].map((n, i) => `<button role="tab" id="gt-${n}" aria-controls="gp-${n}" aria-selected="${i === 1}">${n}</button>`).join('') + '</div>'
    + '<div>' + ['all', 'questions', 'drafts'].map((n) => `<div role="tabpanel" id="gp-${n}">${n}</div>`).join('') + '</div>'
    + '<div class="rk-tiles" style="width:600px">' + ['', 'rk-w2'].map((w) => `<div class="rk-tile ${w}"><span class="rk-stat"><b>1</b><span>x</span></span><svg class="rk-spark rk-spark--static" viewBox="0 0 200 28"><polyline points="1,14 199,14"/></svg></div>`).join('') + '</div>'
    + '<section class="rk-region rk-status-region" aria-label="job"><p class="rk-kicker">nightly build</p><div class="rk-figures"><span class="rk-stat"><b>412</b><span>tests run</span></span></div><p class="rk-actions"><button class="rk-btn">pause</button></p></section>';
  document.querySelector('.rk').append(host);
  const pv = Rekta.pivot(host.querySelector('.rk-pivot')); await frames(); await frames();
  const feet = [...host.querySelectorAll('[role=tab]')].map((t) => { const m = document.createElement('span'); m.style.cssText = 'display:inline-block;width:0;height:0'; t.append(m); const b = m.getBoundingClientRect().bottom; m.remove(); return b; });
  const strokes = [...host.querySelectorAll('.rk-spark')].map((svg) => {
    const l = svg.querySelector('polyline'), k = svg.getScreenCTM().a, pt = svg.createSVGPoint(); pt.x = 100; let half = 0;
    for (let d = 0; d < 4; d += 0.05) { pt.y = 14 + d / k; if (l.isPointInStroke(pt)) half = d; }
    return { width: Math.round(svg.getBoundingClientRect().width), stroke: +(2 * half).toFixed(2) };
  });
  const region = host.querySelector('.rk-status-region'), mt = (s) => parseFloat(getComputedStyle(region.querySelector(s)).marginTop);
  const out = { feet, scaled: host.querySelector('#gt-all').getBoundingClientRect().height < host.querySelector('#gt-questions').getBoundingClientRect().height, strokes,
    region: { figures: mt('.rk-figures'), actions: mt('.rk-actions'), s: Rekta.tokens.px('--rk-space-s'), m: Rekta.tokens.px('--rk-space-m') } };
  pv.destroy(); host.remove();
  return out;
});
ok(geo.scaled && Math.max(...geo.feet) - Math.min(...geo.feet) <= 0.5, 'inactive pivot titles scale about the baseline: one baseline for all', JSON.stringify(geo.feet));
ok(geo.strokes[1].width > geo.strokes[0].width * 1.8 && geo.strokes.every((s) => Math.abs(s.stroke - 2) <= 0.2), 'a sparkline\'s line is 2px on a 1 × 1 and a 2 × 1 tile', JSON.stringify(geo.strokes));
ok(geo.region.figures === geo.region.s && geo.region.actions === geo.region.m, 'a status region: space-s from kicker to figures, space-m to the actions', JSON.stringify(geo.region));
// Data that renders after the navigation: restore() waits for the boxes to appear and grow; a user scroll first wins.
const late = await p.evaluate(async () => {
  const wait = (t) => new Promise((r) => setTimeout(r, t));
  const content = '<div data-rk-scroll="late" style="height:100px;overflow:auto"><div style="height:800px"></div></div><div style="height:900px"></div>';
  const mk = () => { const el = document.createElement('div'); el.style.cssText = 'position:fixed;left:0;top:0;width:300px;height:200px;overflow:auto;z-index:-1'; document.body.append(el); return el; };
  const st = { y: scrollY, at: { root: [0, 300], '@late': [0, 400] } };
  const a = mk(), run = Rekta.scrollState(a).restore(st), waited = run.pending;
  setTimeout(() => { a.innerHTML = content; }, 300); await wait(800);
  const out = { waited, root: a.scrollTop, late: a.querySelector('[data-rk-scroll]').scrollTop, done: !run.pending };
  a.remove();
  const b = mk(), run2 = Rekta.scrollState(b).restore(st);
  b.dispatchEvent(new WheelEvent('wheel', { bubbles: true, deltaY: 40 }));
  setTimeout(() => { b.innerHTML = content; }, 300); await wait(800);
  out.user = { pending: run2.pending, root: b.scrollTop, late: b.querySelector('[data-rk-scroll]').scrollTop };
  b.remove();
  return out;
});
ok(late.waited && late.root === 300 && late.late === 400 && late.done, 'restore() waits for sections that render 300ms later and puts them at their offsets', JSON.stringify(late));
ok(!late.user.pending && late.user.root === 0 && late.user.late === 0, 'a user scroll before they render keeps the user\'s position', JSON.stringify(late.user));
const stage = await p.evaluate(async () => {
  const wait = (t) => new Promise((r) => setTimeout(r, t));
  const rec = document.createElement('section'); rec.id = 'rec';
  rec.innerHTML = '<div style="height:700px">title block</div><div class="rk-pivot" role="tablist" aria-label="s" style="--rk-pivot-wide: 900px">'
    + [0, 1, 2, 3].map((i) => `<button role="tab" id="st${i}" aria-controls="sf${i}" aria-selected="${i === 0}">facet ${i}</button>`).join('') + '</div>'
    + '<div id="sfs" style="width:1300px">' + [0, 1, 2, 3].map((i) => `<div role="tabpanel" id="sf${i}" ${i ? 'hidden' : ''}><p>${'text '.repeat(900)}</p></div>`).join('') + '</div><div style="height:400px"></div>';
  document.body.append(rec);
  const pv = Rekta.pivot(rec.querySelector('.rk-pivot')); await wait(300);
  const row = rec.querySelector('#sfs');
  scrollTo({ top: rec.getBoundingClientRect().top + scrollY, behavior: 'instant' }); await wait(100);
  const y0 = scrollY, top0 = row.getBoundingClientRect().top;
  row.querySelector('#sf0').dispatchEvent(new WheelEvent('wheel', { deltaY: 120, bubbles: true, cancelable: true })); await wait(50);
  const out = { wide: pv.wide, fits: row.getBoundingClientRect().height <= innerHeight, panelScrolls: getComputedStyle(row.querySelector('#sf0')).overflowY === 'auto', moved: Math.round(scrollY - y0), top0: Math.round(top0) };
  for (let i = 0; i < 20; i += 1) row.querySelector('#sf0').dispatchEvent(new WheelEvent('wheel', { deltaY: 120, bubbles: true, cancelable: true }));
  await wait(50);
  out.stopped = Math.round(row.getBoundingClientRect().top);
  const e = new WheelEvent('wheel', { deltaY: 120, bubbles: true, cancelable: true }); row.querySelector('#sf0').dispatchEvent(e);
  out.innerNow = !e.defaultPrevented;
  pv.destroy(); rec.remove(); scrollTo({ top: 0, behavior: 'instant' });
  return out;
});
ok(stage.wide && stage.fits && stage.panelScrolls, 'a wide row below a title block fits the window; each facet scrolls within itself', JSON.stringify(stage));
ok(stage.moved === 120 && stage.stopped <= 2 && stage.stopped >= -2 && stage.innerNow, 'the wheel scrolls the window until the row is whole in view, then the facets take it', JSON.stringify(stage));

const stripped = await p.evaluate(async () => {
  const wait = (ms) => new Promise((r) => setTimeout(r, ms));
  const n = document.createElement('nav'); n.className = 'rk-pivot rk-pivot--nav'; n.style.cssText = 'position:fixed;left:0;top:0;width:900px;z-index:-1';
  n.innerHTML = '<a href="#x1" aria-current="page">home</a><a href="#x2">history and more</a><a href="#x3">files</a>';
  document.body.append(n);
  const s = Rekta.strip(n); await wait(100);
  const gaps = () => { const r = [...n.children].map((e) => e.getBoundingClientRect()); return r.slice(1).map((x, i) => Math.round(x.left - r[i].right)).join(','); };
  const out = { measured: n.hasAttribute('data-rk-measured'), before: gaps() };
  n.children[0].removeAttribute('aria-current'); n.children[1].setAttribute('aria-current', 'page'); await wait(400);
  out.after = gaps(); out.scaled = getComputedStyle(n.children[0]).transform !== 'none';
  s.destroy(); n.remove();
  const place = document.createElement('div'); document.body.append(place); Rekta.arrive(place);
  out.arrive = place.hasAttribute('data-rk-arriving'); place.remove();
  return out;
});
ok(stripped.measured && new Set(stripped.before.split(',')).size === 1 && new Set(stripped.after.split(',')).size === 1 && stripped.scaled, 'strip() measures a link strip and follows the mark', JSON.stringify(stripped));

// Live tile news: the peek travels peek-travel of the content's height, the flip turns flip-angle.
const news = await p.evaluate(async () => {
  const grid = document.createElement('div'); grid.className = 'rk-tiles'; grid.style.cssText = 'position:fixed;left:0;top:0;width:640px;z-index:-1';
  grid.innerHTML = '<div class="rk-tile"><span class="rk-stat">12</span></div><div class="rk-tile"><span class="rk-stat">ok</span></div>';
  document.body.append(grid);
  const [a, b] = grid.children, out = {};
  const peek = Rekta.tileUpdate(a, () => { a.firstChild.textContent = '13'; });
  const ghost = a.querySelector('.rk-stat[aria-hidden]');
  out.leaves = ghost && ghost.getAnimations()[0].effect.getKeyframes().at(-1).transform;
  out.rises = a.querySelector('.rk-stat:not([aria-hidden])').getAnimations()[0].effect.getKeyframes()[0].transform;
  const flip = Rekta.tileUpdate(b, () => { b.firstChild.textContent = 'down'; }, { kind: 'flip' });
  out.flips = b.getAnimations()[0].effect.getKeyframes().at(-1).transform;
  await Promise.all([peek, flip]); grid.remove();
  return out;
});
ok(news.leaves === 'translateY(-40%)' && news.rises === 'translateY(40%)' && /rotateX\(-90deg\)/.test(news.flips), 'tile news peeks by peek-travel and flips by flip-angle', JSON.stringify(news));
// News on several tiles at once comes one after another in reading order, duration-greet apart, four steps at most.
// The showcase's live feed has news every 1.5s too, which would join the batch: it is off while this is measured, and
// the check waits until any news it had is past duration-moderate.
const greeted = await p.evaluate(async () => {
  const feed = document.querySelector('#lv-feed'), was = feed && feed.checked;
  if (feed) feed.checked = false;
  await new Promise((r) => setTimeout(r, 2 * parseFloat(getComputedStyle(document.documentElement).getPropertyValue('--rk-duration-moderate'))));
  const grid = document.createElement('div'); grid.className = 'rk-tiles'; grid.style.cssText = 'position:fixed;left:0;top:0;width:900px;z-index:-1';
  grid.innerHTML = '<div class="rk-tile"><span class="rk-stat">1</span></div>'.repeat(6);
  document.body.append(grid);
  const tiles = [...grid.children], runs = [...tiles].reverse().map((t) => Rekta.tileUpdate(t, () => { t.firstChild.textContent = '2'; }));
  await null;
  const d = parseFloat(getComputedStyle(document.documentElement).getPropertyValue('--rk-duration-moderate'));
  const waits = tiles.map((t) => Math.round(t.querySelector('.rk-stat:not([aria-hidden])').getAnimations()[0].effect.getTiming().delay - 0.2 * d));
  await Promise.all(runs); grid.remove();
  if (feed) feed.checked = was;
  return waits.join();
});
ok(greeted === '0,75,150,225,225,225', 'news on several tiles goes in reading order, duration-greet apart, four steps', greeted);
// A control in a one-line bar has no path: it fades in once its neighbours have made room, and fades out.
const inline = await p.evaluate(async () => {
  const frame = () => new Promise((r) => requestAnimationFrame(() => requestAnimationFrame(r)));
  const bar = document.createElement('div'); bar.className = 'rk-actions'; bar.style.cssText = 'position:fixed;left:0;top:0;width:600px;z-index:-1';
  bar.innerHTML = '<button class="rk-btn">one</button><button class="rk-btn" hidden>two</button><button class="rk-btn">three</button>';
  document.querySelector('.rk').append(bar);
  const [, b, c] = bar.children, path = (el) => el.getAnimations().some((x) => x.effect.getKeyframes().some((k) => k.transform && k.transform !== 'none'));
  const run = Rekta.presence(bar, { show: () => true });
  await frame();
  const out = { waits: getComputedStyle(b).opacity, glides: c.getAnimations().length > 0, path: path(b) };
  await run;
  out.after = getComputedStyle(b).opacity;
  const l = await Rekta.leave(c); out.leavePath = l.effect.getKeyframes().some((k) => k.transform); l.cancel();
  bar.remove();
  return out;
});
ok(inline.waits === '0' && inline.glides && !inline.path && inline.after === '1' && !inline.leavePath, 'a bar control fades in after its neighbours make room, and leaves by a fade, without a path', JSON.stringify(inline));
ok(stripped.arrive, 'arrive() is exported for framework routers', JSON.stringify(stripped));
// Tiles building in (#144): each rises arrive-distance over duration-long on ease-landing and is whole after
// duration-short on the same curve; they follow one another duration-greet apart, four steps at most.
const built = await p.evaluate(() => {
  const place = document.createElement('div'); place.style.cssText = 'position:fixed;left:0;top:0;width:1000px;z-index:-1';
  place.innerHTML = `<div class="rk-tiles">${[0, 1, 2, 3, 4, 5].map((i) => `<div class="rk-tile" style="--rk-i:${i}"><span class="rk-stat">${i}</span></div>`).join('')}</div>`;
  document.querySelector('.rk').append(place);
  Rekta.arrive(place);
  const tiles = [...place.querySelectorAll('.rk-tile')], anim = (t, name) => t.getAnimations().find((a) => a.animationName === name);
  const rise = anim(tiles[0], 'rk-build-in'), fade = anim(tiles[0], 'rk-build-in-fade');
  const out = { rise: Math.round(rise.effect.getTiming().duration), fade: Math.round(fade.effect.getTiming().duration), ease: [rise, fade].map((a) => a.effect.getKeyframes()[0].easing).join(' '), delays: tiles.map((t) => Math.round(anim(t, 'rk-build-in').effect.getTiming().delay)).join() };
  rise.pause(); fade.pause(); rise.currentTime = 0; fade.currentTime = 0;
  out.from = getComputedStyle(tiles[0]).transform; out.hidden = getComputedStyle(tiles[0]).opacity;
  rise.currentTime = fade.currentTime = out.fade;
  out.wholeAt = getComputedStyle(tiles[0]).opacity; out.still = new DOMMatrix(getComputedStyle(tiles[0]).transform).f;
  place.remove();
  return out;
});
ok(built.rise === 600 && built.fade === 250 && built.ease === 'cubic-bezier(0.1, 0.9, 0.2, 1) cubic-bezier(0.1, 0.9, 0.2, 1)' && built.delays === '0,75,150,225,225,225', 'tiles build in over duration-long on ease-landing, whole after duration-short, duration-greet apart, four steps', JSON.stringify(built));
ok(built.from === 'matrix(1, 0, 0, 1, 0, 32)' && built.hidden === '0' && built.wholeAt === '1' && built.still > 0 && built.still < 3, 'a tile rises arrive-distance; it is whole while it still settles', JSON.stringify(built));
// A panorama never ends flush on a section edge (#147): at every width the window's edge falls inside a section,
// space-xl from either of its ends, the gaps even and never below space-m; when everything fits, nothing scrolls.
const edges = await p.evaluate(async () => {
  const frame = () => new Promise((r) => requestAnimationFrame(() => requestAnimationFrame(r)));
  const place = document.createElement('div'); place.className = 'rk-place'; place.style.cssText = 'position:fixed;left:0;top:0;height:400px;z-index:-1';
  place.innerHTML = `<div class="rk-panorama" aria-label="edges" style="--rk-sec-w:500px;height:300px">${'<section class="rk-sec"><p>a</p></section>'.repeat(4)}<div class="rk-panorama-end"></div></div>`;
  document.querySelector('.rk').append(place);
  const pan = place.firstChild, api = Rekta.panorama(pan), out = { bad: [], changed: 0, fits: 0, worst: 0 };
  for (let w = 800; w <= 2400; w += 10) {
    place.style.width = `${w}px`; await frame();
    const r = [...pan.querySelectorAll('.rk-sec')].map((s) => s.getBoundingClientRect()), edge = pan.getBoundingClientRect().left + pan.clientWidth;
    const gaps = r.slice(1).map((s, i) => Math.round(s.left - r[i].right)), gap = gaps[0];
    const even = gaps.every((g) => g === gap), inside = r.some((s) => s.left + 48 <= edge + 0.5 && edge - 0.5 <= s.right - 48);
    if (pan.hasAttribute('data-rk-fit')) { out.fits += 1; if (r[3].right > edge + 1 || gap < 16 || gap > 48 || !even) out.bad.push(`${w}: fit, gap ${gap}`); continue; }
    if (!inside || gap < 16 || !even) out.bad.push(`${w}: edge ${Math.round(edge - r[0].left)}, gaps ${gaps}`);
    if (gap !== 48) { out.changed += 1; out.worst = Math.max(out.worst, Math.abs(gap - 48)); }
  }
  api.destroy(); place.remove();
  return out;
});
ok(!edges.bad.length && edges.changed > 0 && edges.fits > 0 && edges.worst <= 96, 'a panorama never ends flush on a section edge: a piece of the next section shows, the gaps even and at least space-m', JSON.stringify(edges));
// The pivot's mark while it follows (#164, #165): with sections narrower than a third of the width, the start marks
// the first; a jump by key keeps its target marked for the whole glide, as a click on a title does.
const follows = await p.evaluate(async () => {
  const frame = () => new Promise((r) => requestAnimationFrame(() => requestAnimationFrame(r)));
  const place = document.createElement('div'); place.className = 'rk-place'; place.style.cssText = 'position:fixed;left:0;top:0;width:1400px;height:400px;z-index:-1';
  const ids = [...'abcdefghij'];
  place.innerHTML = `<nav class="t-strip">${ids.map((i) => `<a href="#t-sec-${i}">${i}</a>`).join('')}</nav>`
    + `<div class="rk-panorama" aria-label="letters" style="--rk-sec-w:150px;height:300px">${ids.map((i) => `<section class="rk-sec" id="t-sec-${i}"><p>${i}</p></section>`).join('')}<div class="rk-panorama-end"></div></div>`;
  document.querySelector('.rk').append(place);
  const strip = place.firstChild, pan = place.lastChild, api = Rekta.panorama(pan, { pivot: strip });
  const marked = () => { const a = strip.querySelector('[aria-current]'); return a ? a.textContent : ''; };
  const out = {};
  await frame(); await frame();                                                // its gaps are measured: it rests
  pan.scrollTo({ left: 600, behavior: 'instant' }); await frame(); await frame();
  out.mid = marked();
  pan.scrollTo({ left: 0, behavior: 'instant' }); await frame(); await frame();
  out.start = marked();
  pan.focus();
  const seen = new Set();
  pan.dispatchEvent(new KeyboardEvent('keydown', { key: 'ArrowRight', bubbles: true, cancelable: true }));
  const end = performance.now() + Rekta.tokens.ms('--rk-duration-pivot-glide') + 300;
  while (performance.now() < end) { await new Promise((r) => requestAnimationFrame(r)); seen.add(marked()); }
  out.during = [...seen].join();
  api.destroy(); place.remove();
  return out;
});
ok(follows.mid !== 'a' && follows.start === 'a', 'at its start the panorama marks its first section, however narrow the sections', JSON.stringify(follows));
ok(follows.during === 'b', 'a jump by key keeps its target marked for the whole glide', JSON.stringify(follows));
// The gesture hint (#149): nudge-distance the gesture's way and back, duration-short each way on ease-landing, added
// to the part's own transform; it rests where it was.
const nudged = await p.evaluate(async () => {
  const t = document.createElement('div'); t.className = 'rk-tile'; t.style.cssText = 'position:fixed;left:200px;top:200px;width:150px;height:150px;z-index:-1';
  document.querySelector('.rk').append(t);
  const run = Rekta.nudge(t, 'left'), a = t.getAnimations()[0], k = a.effect.getKeyframes();
  const out = { to: k[1].transform, at: k[1].computedOffset, ease: k[0].easing, back: k[1].easing, composite: a.effect.composite, d: a.effect.getTiming().duration };
  await run;
  out.after = t.getAnimations().length; out.rest = getComputedStyle(t).transform;
  try { Rekta.nudge(t, 'sideways'); out.unknown = 'accepted'; } catch { out.unknown = 'refused'; }
  t.remove();
  return out;
});
ok(nudged.to === 'translate(-16px, 0px)' && nudged.at === 0.5 && nudged.ease === 'cubic-bezier(0.1, 0.9, 0.2, 1)' && nudged.back === nudged.ease && nudged.composite === 'add' && nudged.d === 500, 'a gesture hint moves nudge-distance the gesture\'s way and back on ease-landing, added to the part\'s own transform', JSON.stringify(nudged));
ok(nudged.after === 0 && nudged.rest === 'none' && nudged.unknown === 'refused', 'a gesture hint rests where the part was; an unknown direction is refused', JSON.stringify(nudged));

// A domain colour code: a tag's square and a chart series take --rk-series-N; their text stays ink.
const codes = await p.evaluate(() => {
  const want = getComputedStyle(document.documentElement).getPropertyValue('--rk-series-2').trim();
  const rgb = (c) => { const e = document.createElement('i'); e.style.color = c; document.body.append(e); const v = getComputedStyle(e).color; e.remove(); return v; };
  const tag = document.querySelector('.rk-tag[data-rk-code="2"]');
  const box = document.createElement('div'); box.style.width = '400px'; document.body.append(box);
  const ch = Rekta.chart(box, { type: 'line', labels: ['a', 'b', 'c'], series: [{ label: 'heart rate', values: [120, 140, 135], code: 2 }], table: false });
  const line = box.querySelector('.rk-line[data-rk-code="2"]'), out = { want: rgb(want), tag: getComputedStyle(tag, '::before').backgroundColor, tagText: getComputedStyle(tag).color, line: line ? getComputedStyle(line).stroke : null, ink: rgb('var(--rk-text-primary)') };
  ch.destroy(); box.remove();
  return out;
});
ok(codes.tag === codes.want && codes.line === codes.want && codes.tagText === codes.ink, 'a domain colour code marks the tag and the chart line, never the text', JSON.stringify(codes));
const shares = await p.evaluate(() => {
  const rgb = (c) => { const e = document.createElement('i'); e.style.color = c; document.body.append(e); const v = getComputedStyle(e).color; e.remove(); return v; };
  const parts = [...document.querySelectorAll('#zones > span')], w = parts.map((x) => x.getBoundingClientRect().width);
  const meter = document.querySelector('#zone-meter');
  meter.setAttribute('data-status', 'warning'); const withStatus = getComputedStyle(meter, '::before').backgroundColor; meter.removeAttribute('data-status');
  return {
    colours: parts.every((x, i) => getComputedStyle(x).backgroundColor === rgb(`var(--rk-series-${i + 1})`)),
    ratio: Math.abs(w[1] / w[0] - 31 / 12) < 0.15, gap: Math.round(parts[1].getBoundingClientRect().left - parts[0].getBoundingClientRect().right),
    meter: getComputedStyle(meter, '::before').backgroundColor === rgb('var(--rk-series-2)'), status: withStatus === rgb('var(--rk-status-warning-full)'),
  };
});
ok(shares.colours && shares.ratio && shares.gap === 2, 'a share bar: each share in its code, as wide as its share, 2px apart', JSON.stringify(shares));
ok(shares.meter && shares.status, 'a meter fills in its code; a status wins', JSON.stringify(shares));
// A pivot's titles, buttons or links, are written as the strip says: case and spacing set by CSS reach them (#100).
const cased = await p.evaluate(() => {
  const n = document.createElement('div'); n.className = 'rk-pivot'; n.style.cssText = 'text-transform:uppercase;letter-spacing:3px';
  n.innerHTML = '<button type="button" aria-selected="true">jobs</button><button type="button">runs</button><a href="#">logs</a>';
  document.querySelector('.rk').append(n);
  const out = [...n.children].map((x) => `${getComputedStyle(x).textTransform} ${getComputedStyle(x).letterSpacing}`);
  n.remove();
  return out;
});
ok(cased.every((x) => x === 'uppercase 3px'), 'a pivot\'s tabs take the strip\'s text-transform and letter-spacing', JSON.stringify(cased));

// rekta.js in German (?lang=de): its words from the catalogue, its numbers in the locale; none of its English shows.
{
  const de = await open({}, '?lang=de');
  await de.waitForTimeout(800);
  const seen = await de.evaluate(() => {
    // What rekta.js itself writes: live ages, a table's column handles, a chart's table of numbers, sortable grips.
    const own = [...document.querySelectorAll('.rk-age, .rk-chart-table > summary, .rk-live-more')].map((e) => e.textContent)
      .concat([...document.querySelectorAll('th > [role=separator][aria-label], [data-rk-grip][aria-label], .rk-grip[aria-label]')].map((e) => e.getAttribute('aria-label')))
      .filter(Boolean);
    const english = own.filter((x) => /\b(updated|ago|just now|show the numbers|width of|move|position|waiting)\b/.test(x));
    return { english, count: own.length, sample: own.slice(0, 4), number: Rekta.number(1234.5), locale: Rekta.locale() };
  });
  ok(seen.count > 2 && seen.english.length === 0, 'in German, what rekta.js writes is German', JSON.stringify(seen));
  ok(seen.locale === 'de' && seen.number === '1234,5', 'and its numbers follow the locale', JSON.stringify(seen));
  await de.close();
}

// Motion: reorder, toast, reduce motion.
await go(p, 'motion');
await p.click('#flip li:first-child input'); await p.waitForTimeout(500);
ok((await p.evaluate(() => document.querySelector('#flip li:last-child span').textContent)) === 'write the release notes', 'a finished row moves to the end');
await p.click('#m-toast'); await p.waitForTimeout(300);
ok((await toast(p)).startsWith('Something happened'), 'a toast');
await p.check('#m-off'); await p.waitForTimeout(200);
ok((await p.evaluate(() => getComputedStyle(document.documentElement).getPropertyValue('--rk-duration-short').trim())) === '0s', 'reduce motion zeroes durations');
await p.uncheck('#m-off'); await p.waitForTimeout(200);

// Lists: Metro selection. A click selects and shows; Ctrl+click starts the mode; then clicks add and remove.
await go(p, 'lists');
const row = (n) => `#comp-body tr:nth-child(${n})`;
const sel = () => p.evaluate(() => [...document.querySelectorAll('#comp-body tr[aria-selected="true"]')].map((r) => r.dataset.name));
const mode = () => p.evaluate(() => document.querySelector('#bar').hasAttribute('data-rk-selecting'));
const box = await p.evaluate((s) => { const r = document.querySelector(s).getBoundingClientRect(); return { x: r.left + r.width * 0.8, y: r.top + r.height / 2 }; }, row(2));
await p.mouse.move(box.x, box.y); await p.mouse.down(); await p.waitForTimeout(60);
ok(await p.evaluate((s) => document.querySelector(s).hasAttribute('data-rk-pressed'), row(2)), 'a row nudges where it is pressed');
await p.mouse.up(); await p.waitForTimeout(350);
ok(JSON.stringify(await sel()) === '["Sparkline"]' && (await text(p, '#comp-shown')).startsWith('Shown: Sparkline'), 'a click selects one row and shows it', `${await sel()} / ${await text(p, '#comp-shown')}`);
ok(await p.evaluate((s) => {
  const r = document.querySelector(s), probe = document.createElement('i');
  probe.style.color = 'var(--rk-accent)'; document.body.appendChild(probe);
  const c = getComputedStyle(probe).color; probe.remove();
  return getComputedStyle(r).backgroundColor === c;
}, row(2)), 'a selected row takes the accent');
ok(await p.evaluate((s) => getComputedStyle(document.querySelector(`${s} .rk-check`), '::before').content !== 'none', row(2)), 'a selected row shows its checkmark');
await p.screenshot({ path: join(shots, 'selected.png') });
ok(!(await mode()), 'one selection is not the selection mode');
await p.click(row(4), { modifiers: ['Control'] }); await p.waitForTimeout(200);
ok((await sel()).length === 2 && await mode() && (await text(p, '.rk-ab-count')) === '2 selected', 'Ctrl+click adds and starts the mode; the app bar carries it', `${await sel()} / ${await text(p, '.rk-ab-count')}`);
await p.click(row(5)); await p.waitForTimeout(150);
ok((await sel()).length === 3, 'in the mode a plain click adds');
await p.screenshot({ path: join(shots, 'selecting.png') });
await p.click(row(5)); await p.waitForTimeout(150);
ok((await sel()).length === 2, 'and removes');
await p.keyboard.press('Escape'); await p.waitForTimeout(200);
ok((await sel()).length === 0 && !(await mode()) && await visible(p, 'home'), 'Escape ends the mode, not the place');
await p.click(row(1)); await p.click(row(3), { modifiers: ['Shift'] }); await p.waitForTimeout(150);
ok((await sel()).length === 3, 'Shift+click selects a range', await sel());
await p.keyboard.press('Escape');
await p.focus(row(1)); await p.keyboard.press('ArrowDown'); await p.keyboard.press(' '); await p.waitForTimeout(150);
ok(JSON.stringify(await sel()) === '["Sparkline"]', '↓ moves, Space selects', await sel());
await p.keyboard.press('Escape');
// Letters jump (#134): the next row whose name starts with them; the same letter again moves on; typed together they
// are a prefix. Focus moves, the selection stays.
{
  await p.focus(row(1)); await p.waitForTimeout(1100);
  const at = () => p.evaluate(() => document.activeElement.dataset.name);
  await p.keyboard.press('s'); const s1 = await at(); await p.keyboard.press('s'); const s2 = await at();
  await p.waitForTimeout(1100); await p.keyboard.press('p'); await p.keyboard.press('a'); await p.keyboard.press('n'); const pan = await at();
  const stop = await p.evaluate(() => [...document.querySelectorAll('#comp-body tr')].filter((r) => r.tabIndex === 0).map((r) => r.dataset.name).join());
  ok(s1 === 'Sparkline' && s2 === 'Sheet' && pan === 'Panorama' && stop === 'Panorama' && JSON.stringify(await sel()) === '["Sparkline"]', 'letters jump in a table: s, s again, then "pan"; the tab stop follows, the selection stays', `${s1} ${s2} ${pan} / ${stop} / ${await sel()}`);
}
await p.click('#comp-table thead th:nth-child(2) .rk-sort'); await p.waitForTimeout(500);
ok((await p.evaluate(() => document.querySelector('#comp-body tr').dataset.name)) === 'Accent surface', 'sort by component');
await p.click('#comp-status'); await p.waitForTimeout(200); await p.click('#comp-status-menu [data-v=warning]'); await p.waitForTimeout(200);
ok((await text(p, '#comp-status')) === 'status: waiting' && (await p.evaluate(() => [...document.querySelectorAll('#comp-body tr')].filter((r) => !r.hidden).length)) === 3, 'a filter keeps the waiting components', await text(p, '#comp-status'));
await p.keyboard.press('Escape'); await p.click('#comp-clear'); await p.waitForTimeout(200);
await p.click('#comp-more'); await p.waitForTimeout(200);
const line = () => p.evaluate(() => getComputedStyle(document.querySelector('#comp-region'), '::before').visibility);
// the choice and the look in one step: a slow runner's round trips must not eat the busy delay
const busyAtOnce = await p.evaluate(() => {
  document.querySelector('#comp-menu [data-act=refresh]').click();
  const r = document.querySelector('#comp-region');
  return r.getAttribute('aria-busy') === 'true' && getComputedStyle(r, '::before').visibility === 'hidden';
});
ok(busyAtOnce, 'loading: no line before the delay');
// the line comes once the delay is over, while the 1.2s load still runs: wait for it rather than guess when a busy runner gets there
const lineCame = await p.waitForFunction(() => getComputedStyle(document.querySelector('#comp-region'), '::before').visibility === 'visible', null, { timeout: 1100, polling: 50 }).then(() => true, () => false);
ok(lineCame, 'loading: the line after the delay', await line());
const toastCame = await p.waitForFunction(() => document.querySelector('.rk-toast:not([data-rk-phase]) .rk-toast-msg')?.textContent.trim() === 'Components refreshed.', null, { timeout: 3000, polling: 50 }).then(() => true, () => false);
ok(toastCame, 'refresh ends with a toast', await toast(p));
await p.click('#comp-more'); await p.waitForTimeout(200); await p.click('#comp-menu [data-act=fail]'); await p.waitForTimeout(1900);
ok((await p.getAttribute('#comp-error', 'data-status')) === 'error', 'a failed refresh says so in the region');
await p.click('#comp-retry'); await p.waitForTimeout(1600);
ok(!(await p.$('#comp-error')), 'retry loads again');
const picks = () => p.evaluate(() => [...document.querySelectorAll('#pick-rows [aria-selected="true"]')].map((r) => r.dataset.name));
await p.click('#pick-rows li:nth-child(1)'); await p.waitForTimeout(300);
ok(JSON.stringify(await picks()) === '["Nightly import"]' && (await text(p, '#pick-shown')) === 'Shown: Nightly import.', 'rows: a click selects and shows');
ok(await p.evaluate(() => getComputedStyle(document.querySelector('#pick-rows li:nth-child(1)'), '::before').content !== 'none'), 'rows: a checkmark without a box');
await p.click('#pick-rows li:nth-child(3)', { modifiers: ['Control'] }); await p.waitForTimeout(200);
ok((await picks()).length === 2 && (await text(p, '.rk-ab-count')) === '2 selected' && await mode(), 'rows: Ctrl+click starts the mode');
await p.screenshot({ path: join(shots, 'rows-selected.png') });
await p.click('#sel-copy');
ok(await until(p, async () => /^(Copied 2 names\.|Copying needs clipboard access; 2 selected\.)$/.test(await toast(p))), 'the app bar acts on the selection', await toast(p));
await p.click('#pick-rows li:nth-child(1)'); await p.keyboard.press('Escape'); await p.waitForTimeout(200);
ok((await picks()).length === 0 && !(await mode()), 'rows: Escape ends the mode');

// Lists, the desktop way: columns, editing, a context menu, archive with undo.
const colW = () => p.evaluate(() => Math.round(document.querySelector('#comp-table thead th:nth-child(2)').getBoundingClientRect().width));
const w1 = await colW();
await p.focus('#comp-table thead th:nth-child(2) .rk-col-handle'); await p.keyboard.press('ArrowRight'); await p.waitForTimeout(100);
ok((await colW()) - w1 === 16, 'a column handle sizes its column with the keys', `${w1} → ${await colW()}`);
await p.click('#comp-cols'); await p.waitForTimeout(200); await p.click('#comp-cols-menu [data-col="4"]'); await p.keyboard.press('Escape'); await p.waitForTimeout(150);
ok(await p.evaluate(() => document.querySelector('#comp-table thead th:nth-child(5)').hidden && document.querySelector('#comp-body tr td:nth-child(5)').hidden), 'the columns menu hides a column');
await p.click('#comp-cols'); await p.waitForTimeout(200); await p.click('#comp-cols-menu [data-col="4"]'); await p.keyboard.press('Escape'); await p.waitForTimeout(150);
const tileRow = '#comp-body tr[data-name="Tile"]';
await p.dblclick(`${tileRow} td:nth-child(3)`); await p.waitForTimeout(150);
ok(!!(await p.$(`${tileRow} .rk-cell-input`)), 'a double click edits a cell in place');
await p.fill(`${tileRow} .rk-cell-input`, ''); await p.keyboard.press('Enter'); await p.waitForTimeout(700);
ok((await text(p, `${tileRow} .rk-cell-error`)) === 'A component needs a group.' && !!(await p.$(`${tileRow} .rk-cell-input`)), 'a refusal says why below the cell and keeps it open');
await p.fill(`${tileRow} .rk-cell-input`, 'surfaces'); await p.keyboard.press('Enter');
await p.waitForFunction(() => { const t = document.querySelector('.rk-toast:not([data-rk-phase]) .rk-toast-msg'); return t && t.textContent.startsWith('Moved Tile'); }, null, { timeout: 3000 }).catch(() => {});
ok((await text(p, `${tileRow} td:nth-child(3)`)) === 'surfaces' && (await toast(p)).startsWith('Moved Tile to'), 'Enter keeps the value; the history remembers it', await toast(p));
await p.focus(tileRow); await p.keyboard.press('F2'); await p.waitForTimeout(100); await p.keyboard.press('Escape'); await p.waitForTimeout(100);
ok((await text(p, `${tileRow} td:nth-child(3)`)) === 'surfaces' && await visible(p, 'home'), 'F2 edits, Escape puts the old value back, not the place');
const tagHidden = () => p.evaluate(() => document.querySelector('#comp-body tr[data-name="Tag"]').hidden);
await p.click('#comp-body tr[data-name="Tag"]', { button: 'right' });
const ctxState = () => p.evaluate(() => {
  const at = document.querySelector('#comp-body tr[data-name="Tag"]').getBoundingClientRect(), hit = document.elementFromPoint(at.x + at.width / 2, at.y + at.height / 2);
  return `open ${document.querySelector('#comp-ctx').matches(':popover-open')}, selected ${document.querySelector('#comp-body tr[data-name="Tag"]').getAttribute('aria-selected')}, focus ${document.activeElement.dataset.act || document.activeElement.tagName}, under the pointer ${hit ? hit.className || hit.tagName : '-'}, toast ${document.querySelector('.rk-toast:not([data-rk-phase])') ? 'yes' : 'no'}`;
});
ok(await until(p, async () => await isOpen(p, '#comp-ctx') && (await p.getAttribute('#comp-body tr[data-name="Tag"]', 'aria-selected')) === 'true' && (await p.evaluate(() => document.activeElement.dataset.act)) === 'show'), 'a right click opens the row’s menu, selects the row, focuses the first item', await ctxState());
await p.keyboard.press('ArrowDown'); await p.keyboard.press('ArrowDown'); await p.keyboard.press('ArrowDown'); await p.keyboard.press('Enter');
ok(await until(p, async () => await tagHidden() && (await toast(p)).startsWith('Archived Tag')), 'the menu archives the row', await toast(p));
await p.keyboard.press('Control+z');
ok(await until(p, async () => !(await tagHidden()) && (await toast(p)).startsWith('Undone: archived Tag')), 'Ctrl+Z brings it back', await toast(p));
await p.keyboard.press('Control+Shift+z');
ok(await until(p, tagHidden), 'Ctrl+Shift+Z archives it again');
await p.keyboard.press('Control+z'); await until(p, async () => !(await tagHidden()));
await p.focus('#comp-body tr[data-name="Rows"]'); await p.keyboard.press('Shift+F10');
ok(await until(p, () => isOpen(p, '#comp-ctx')), 'Shift+F10 opens the menu from the keyboard');
await p.keyboard.press('Escape');
ok(await until(p, async () => !(await isOpen(p, '#comp-ctx')) && (await focused(p)) === 'Rows') && await visible(p, 'home'), 'Escape closes it and focus returns to the row', await focused(p));
await p.focus('#comp-body tr[data-name="Rows"]'); await p.keyboard.press('Shift+F10'); await until(p, () => isOpen(p, '#comp-ctx'));
await p.evaluate(() => document.querySelector('#nav').dispatchEvent(new Event('scroll')));
ok(await isOpen(p, '#comp-ctx'), 'a scroll elsewhere leaves the menu open');
await p.evaluate(() => { document.querySelector('#comp-table').style.transform = 'translateY(40px)'; document.querySelector('#nav').dispatchEvent(new Event('scroll')); });
ok(!(await isOpen(p, '#comp-ctx')), 'a scroll that moves its row closes it');
await p.evaluate(() => { document.querySelector('#comp-table').style.transform = ''; });

// Desktop: tooltips, the tree, a sortable list, ten thousand lines.
await go(p, 'desktop');
await p.hover('#tree-open'); await p.waitForTimeout(250);
ok(!(await p.evaluate(() => document.querySelector('.rk-tip') && document.querySelector('.rk-tip').matches(':popover-open'))), 'no tooltip before the pointer rests');
await p.waitForTimeout(500);
ok((await p.evaluate(() => document.querySelector('.rk-tip').matches(':popover-open') && document.querySelector('.rk-tip').textContent)) === 'open every folder *', 'a tooltip after a rest, with its key', await p.evaluate(() => document.querySelector('.rk-tip') && document.querySelector('.rk-tip').textContent));
await p.hover('#tree-close'); await p.waitForTimeout(60);
ok((await p.evaluate(() => document.querySelector('.rk-tip').textContent)) === 'close every folder', 'the next tooltip comes at once');
await p.keyboard.press('Escape'); await p.waitForTimeout(100);
ok(!(await p.evaluate(() => document.querySelector('.rk-tip').matches(':popover-open'))) && await visible(p, 'home'), 'Escape hides the tooltip, nothing more');
// A key combination is written one way: Ctrl, Alt, Shift, Meta, then the key, joined by "+"; Meta outside Apple is the
// catalogue's keyMeta, "Super" here on Linux (#141).
await p.evaluate(() => {
  const box = document.createElement('p'); box.id = 'keys-fixture'; box.style.cssText = 'position:fixed;left:200px;top:200px;z-index:9999;display:flex;gap:40px';
  box.innerHTML = '<button class="rk-btn" type="button" data-rk-tip="redo" aria-keyshortcuts="Control+Shift+Z Meta+Shift+Z">a</button>'
    + '<button class="rk-btn" type="button" data-rk-tip="launcher" aria-keyshortcuts="Meta+Shift+Z">b</button>'
    + '<button class="rk-btn" type="button" data-rk-tip="find" aria-keyshortcuts="Shift+Alt+Control+f">c</button>';
  document.querySelector('.rk').append(box);
});
const keyOf = async (n) => { await p.hover(`#keys-fixture > :nth-child(${n})`); await p.waitForTimeout(900); const k = await text(p, '.rk-tip .rk-key'); await p.mouse.move(5, 5); await p.waitForTimeout(200); return k; };
const written = { both: await keyOf(1), meta: await keyOf(2), order: await keyOf(3) };
await p.evaluate(() => Rekta.setStrings({ keyMeta: (os) => (os === 'windows' ? 'Win' : 'Meta') }));
written.word = await keyOf(2);
await p.evaluate(() => { Rekta.setStrings(null); document.querySelector('#keys-fixture').remove(); });
ok(written.both === 'Ctrl+Shift+Z' && written.order === 'Ctrl+Alt+Shift+F', 'a key combination: modifiers in order, joined by +', JSON.stringify(written));
ok(written.meta === 'Shift+Super+Z' && written.word === 'Shift+Meta+Z', 'the Meta key outside Apple is the catalogue\'s keyMeta', JSON.stringify(written));
// A disabled app-bar control (#126): glyph and word in disabled, no tint, still focusable; its reason is its tooltip,
// linked with aria-describedby, after a rest and at once on keyboard focus; in "more" the item names it.
{
  const icon = '<svg viewBox="0 0 20 20" aria-hidden="true"><circle cx="9" cy="9" r="5.5"/></svg>';
  const d = await p.evaluate((icon) => {
    const el = document.createElement('div'); el.className = 'rk-appbar'; el.id = 't-bar'; el.style.cssText = 'width:700px;left:0;right:auto;bottom:120px';
    el.innerHTML = `<span class="rk-ab-status"><i class="rk-pulse"></i><span class="rk-ab-k">live</span></span><button class="rk-ab" id="t-first" type="button">${icon}<span>first</span></button>`
      + `<button class="rk-ab" id="t-print" type="button" data-rk-tip="print it">${icon}<span>print</span></button><button class="rk-ab rk-ab-more" type="button" hidden>${icon}<span>more</span></button>`;
    document.querySelector('.rk').append(el);
    window.tBar = Rekta.appbar(el);
    const b = el.querySelector('#t-print');
    window.tPrinted = 0; b.addEventListener('click', () => { window.tPrinted += 1; });
    Rekta.disable(b, 'needs a printer');
    const probe = document.createElement('i'); probe.style.color = 'var(--rk-disabled)'; el.append(probe);
    const grey = getComputedStyle(probe).color; probe.remove();
    const why = document.getElementById(b.getAttribute('aria-describedby'));
    return { grey: getComputedStyle(b).color === grey && getComputedStyle(b.querySelector('svg')).stroke === grey, tip: b.dataset.rkTip, why: why && why.hidden ? why.textContent : '(none)', focusable: b.tabIndex === 0 && !b.disabled, hint: !!el.querySelector('.rk-hint') };
  }, icon);
  ok(d.grey && d.focusable && d.tip === 'needs a printer' && d.why === 'needs a printer' && !d.hint, 'disable() in the app bar: disabled colour, focusable, the reason in its tooltip and a hidden description, no hint beside it', JSON.stringify(d));
  // The tip comes after a rest: wait until it is open (a busy runner gets there later), then look once.
  await p.hover('#t-print'); await until(p, () => p.evaluate(() => document.querySelector('.rk-tip').matches(':popover-open')), 3000);
  const rest = await p.evaluate(() => ({ tip: document.querySelector('.rk-tip').matches(':popover-open') && document.querySelector('.rk-tip').textContent, tint: getComputedStyle(document.querySelector('#t-print')).backgroundColor, said: document.querySelector('#t-print').getAttribute('aria-describedby') }));
  ok(rest.tip === 'needs a printer' && /rgba\(0, 0, 0, 0\)|transparent/.test(rest.tint) && !/rk-tip/.test(rest.said), 'under a resting pointer the reason shows, no hover tint, described once', JSON.stringify(rest));
  await p.click('#t-print', { force: true }); await p.waitForTimeout(100);
  ok((await p.evaluate(() => window.tPrinted)) === 0, 'a click does nothing');
  await p.mouse.move(5, 5); await p.waitForTimeout(100);
  await p.focus('#t-first'); await p.keyboard.press('Tab'); await p.waitForTimeout(100);
  ok((await focused(p)) === 't-print' && (await p.evaluate(() => document.querySelector('.rk-tip').matches(':popover-open') && document.querySelector('.rk-tip').textContent)) === 'needs a printer', 'Tab reaches it and its reason shows at once', await focused(p));
  await p.keyboard.press('Escape');
  const more = await p.evaluate(async () => {
    const el = document.querySelector('#t-bar'); el.style.width = '150px'; window.tBar.layout();
    const item = [...el.querySelectorAll('.rk-ab-menu .rk-menu-item')].find((x) => x._rkFor && x._rkFor.id === 't-print');
    return { moved: el.querySelector('#t-print').hasAttribute('data-rk-overflow'), label: item ? item.textContent : '(none)', off: item ? item.getAttribute('aria-disabled') : '' };
  });
  ok(more.moved && more.label === 'print (needs a printer)' && more.off === 'true', 'moved into "more", the reason goes with it: a disabled item that names it', JSON.stringify(more));
  const back = await p.evaluate(() => { const b = document.querySelector('#t-print'); Rekta.disable(b, null); return { tip: b.dataset.rkTip, off: b.getAttribute('aria-disabled'), desc: b.getAttribute('aria-describedby'), why: !!document.querySelector('#t-bar .rk-why') }; });
  ok(back.tip === 'print it' && back.off === null && back.desc === null && !back.why, 'enabled again: its own tooltip comes back', JSON.stringify(back));
  await p.evaluate(() => { window.tBar.destroy(); document.querySelector('#t-bar').remove(); });
}
// In a compact bar a place's own colours are for one that can act: a disabled place, current or not, keeps the disabled
// colour (#137, reported by jay23606).
{
  const places = await p.evaluate(() => {
    const bar = document.createElement('div'); bar.className = 'rk-appbar'; bar.style.cssText = 'width:400px;left:0;right:auto;bottom:200px';
    bar.innerHTML = '<nav class="rk-ab-places"><a class="rk-ab" href="#" aria-current="page" aria-disabled="true"><span>here</span></a>'
      + '<a class="rk-ab" href="#" aria-disabled="true"><span>off</span></a><a class="rk-ab" href="#"><span>on</span></a><a class="rk-ab" href="#" aria-current="page"><span>now</span></a></nav>';
    document.querySelector('.rk').append(bar);
    const probe = (v) => { const i = document.createElement('i'); i.style.color = `var(${v})`; bar.append(i); const c = getComputedStyle(i).color; i.remove(); return c; };
    const [cur, off, on, now] = [...bar.querySelectorAll('.rk-ab')].map((a) => getComputedStyle(a).color);
    const r = { cur, off, on, now, disabled: probe('--rk-disabled'), high: probe('--rk-text-contrast-high'), ink: probe('--rk-text-primary') };
    bar.remove();
    return r;
  });
  ok(places.cur === places.disabled && places.off === places.disabled && places.on === places.high && places.now === places.ink, 'a disabled place in a compact bar keeps the disabled colour, current or not', JSON.stringify(places));
}
// Shell patterns (#120 #121 #128 #133 #142): an app bar with a centred control, a short indicator, a count that peeks,
// giving way to full screen; a notification from another program; a menu's second level and icons; a called place;
// the display step.
{
  const icon = '<svg viewBox="0 0 20 20" aria-hidden="true"><circle cx="9" cy="9" r="5.5"/></svg>';
  const two = await p.evaluate(async (icon) => {
    const wait = (t) => new Promise((r) => setTimeout(r, t));
    const el = document.createElement('div'); el.className = 'rk-appbar'; el.id = 't-two'; el.style.cssText = 'width:900px;left:0;right:auto;bottom:200px';
    el.innerHTML = `<span class="rk-ab-status"><i class="rk-pulse"></i><span class="rk-ab-k">live</span></span><span class="rk-ab-mid" id="t2-mid">a mode that says a little more</span>`
      + `<button class="rk-ab" id="t2-a" type="button">${icon}<span>alpha</span></button><button class="rk-ab" id="t2-b" type="button" data-rk-priority="2">${icon}<span>beta</span></button>`
      + `<button class="rk-ab" id="t2-clock" type="button" data-rk-align="center"><span>12:45<br>1 November</span></button>`
      + `<button class="rk-ab" id="t2-c" type="button">${icon}<span>gamma</span></button><button class="rk-ab" id="t2-d" type="button" data-rk-priority="3"><span data-rk-peek>3</span><span>notifications</span></button>`
      + `<button class="rk-ab rk-ab-more" type="button" hidden>${icon}<span>more</span></button>`;
    document.querySelector('.rk').append(el);
    const bar = Rekta.appbar(el); await wait(100);
    const centre = (id) => { const r = el.querySelector(id).getBoundingClientRect(); return r.left + r.width / 2; };
    const b = el.getBoundingClientRect(), c = el.querySelector('#t2-clock').getBoundingClientRect();
    const r = { wide: Math.abs(centre('#t2-clock') - (b.left + b.width / 2)), moved: el.querySelectorAll('[data-rk-overflow]').length, mid: el.querySelector('#t2-mid').getBoundingClientRect().right <= c.left + 0.5 };
    el.style.width = '470px'; bar.layout(); await wait(50);
    const c2 = el.querySelector('#t2-clock').getBoundingClientRect(), b2 = el.getBoundingClientRect();
    const lead = [...el.querySelectorAll('#t2-a, #t2-b, .rk-ab-status')].filter((x) => x.getClientRects().length).every((x) => x.getBoundingClientRect().right <= c2.left + 0.5);
    const trail = [...el.querySelectorAll('#t2-c, #t2-d, .rk-ab-more')].filter((x) => x.getClientRects().length).every((x) => x.getBoundingClientRect().left >= c2.right - 0.5);
    r.narrow = { centre: Math.abs(c2.left + c2.width / 2 - (b2.left + b2.width / 2)), lead, trail, full: el.getAttribute('data-rk-full'), clock: el.querySelector('#t2-clock').hasAttribute('data-rk-overflow'),
      more: !el.querySelector('.rk-ab-more').hidden, lastIsMore: [...el.children].filter((x) => x.getClientRects().length && !x.matches('.rk-ab-menu')).pop().matches('.rk-ab-more'),
      midShown: getComputedStyle(el.querySelector('#t2-mid')).display !== 'none', midBefore: el.querySelector('#t2-mid').getBoundingClientRect().right <= c2.left + 0.5 };
    // A count peeks; an indicator stands in place of the clock and turns back; the bar gives way to full screen.
    const d = el.querySelector('#t2-d'); d.removeAttribute('data-rk-overflow');
    const peek = Rekta.tileUpdate(d, () => { d.querySelector('[data-rk-peek]').textContent = '4'; });
    await wait(30); r.peeks = d.querySelector('[data-rk-peek]').getAnimations().length > 0 || !!d.querySelector('[aria-hidden="true"][data-rk-peek]'); await peek;
    const meter = document.createElement('span'); meter.className = 'rk-meter'; meter.style.setProperty('--rk-v', 0.4);
    const clock = el.querySelector('#t2-clock');
    Rekta.indicate(clock, meter); await wait(30);
    r.flips = clock.getAnimations().length > 0; await wait(400);
    r.stands = clock.hasAttribute('data-rk-indicating') && clock.contains(meter) && clock.querySelector(':scope > span').hidden && meter.getBoundingClientRect().width > 100;
    await wait(Rekta.tokens.ms('--rk-duration-level') + 700);
    r.back = !clock.hasAttribute('data-rk-indicating') && !clock.contains(meter) && !clock.querySelector(':scope > span').hidden;
    // Read once its glide out has ended (getAnimations() includes the transitions), not after a fixed wait.
    el.setAttribute('data-rk-away', ''); await Promise.all(el.getAnimations().map((x) => x.finished.catch(() => {}))); await wait(50);
    r.away = el.getBoundingClientRect().top >= b2.bottom - 1 && getComputedStyle(el).visibility === 'hidden';
    bar.destroy(); el.remove();
    return r;
  }, icon);
  ok(two.wide < 1 && two.moved === 0 && two.mid, 'a centred control stands at the bar\'s centre; the middle line ends before it', JSON.stringify(two));
  ok(two.narrow.centre < 1 && two.narrow.lead && two.narrow.trail && !two.narrow.clock, 'narrower: each side keeps to its half, the centred control stays', JSON.stringify(two.narrow));
  ok(two.narrow.more && two.narrow.lastIsMore && /start|end/.test(two.narrow.full || ''), 'a side that does not fit gives its own controls away; more ends the trailing side', JSON.stringify(two.narrow));
  ok(two.narrow.full === 'end' ? two.narrow.midShown && two.narrow.midBefore : !two.narrow.midShown, 'the middle line goes only when the leading side is full, and cuts before the centred control', JSON.stringify(two.narrow));
  ok(two.peeks, 'a count in a control (data-rk-peek) peeks');
  ok(two.flips && two.stands && two.back, 'indicate(): the indicator turns in, stands in place, and turns back after timing.level', JSON.stringify(two));
  ok(two.away, 'data-rk-away: the bar glides out over its edge and takes no focus');

  // A notification from another program: from the edge, title over two lines at most, a picture, a pressable text.
  const note = await p.evaluate(async () => {
    const wait = (t) => new Promise((r) => setTimeout(r, t));
    const region = document.createElement('div'); region.className = 'rk-toasts'; region.setAttribute('role', 'status'); region.id = 't-toasts';
    document.querySelector('.rk').append(region);
    const t = Rekta.toaster(region); let opened = 0, ended = '';
    const pic = 'data:image/svg+xml,' + encodeURIComponent('<svg xmlns="http://www.w3.org/2000/svg" width="40" height="20"><rect width="40" height="20" fill="#c33"/></svg>');
    const long = 'Are we still on for three? I booked the room by the window, and the projector is there too. Bring the drafts and the numbers from last week, please.';
    const n = t.show(long, { edge: true, title: 'Ana Ruiz', picture: pic, open: () => { opened += 1; }, onEnd: (why) => { ended = why; } });
    await wait(20);
    const el = n.el, msg = el.querySelector('.rk-toast-msg'), text = el.querySelector('.rk-toast-text'), img = el.querySelector('.rk-toast-pic img');
    const anim = el.getAnimations()[0], kf = anim && anim.effect.getKeyframes();
    const r = { from: el.dataset.rkFrom, button: msg.tagName, opacity: kf ? kf.every((k) => k.opacity === undefined) : false, edge: kf ? kf[0].transform : '',
      title: getComputedStyle(el.querySelector('.rk-toast-title')).fontWeight, lines: Math.round(text.getBoundingClientRect().height / parseFloat(getComputedStyle(text).lineHeight)), whole: msg.textContent.includes('last week'),
      pic: img ? [img.getBoundingClientRect().width, img.getBoundingClientRect().height, getComputedStyle(img).objectFit] : null };
    await Promise.all(el.getAnimations().map((a) => a.finished.catch(() => {})));
    r.inPlace = Math.abs(el.getBoundingClientRect().top - region.getBoundingClientRect().top) < 1;
    msg.click(); await wait(10);
    r.opened = opened === 1 && ended === 'opened' && el.dataset.rkPhase === 'leaving';
    const anim2 = el.getAnimations()[0]; r.out = anim2 ? anim2.animationName : '';
    await wait(300);
    const plain = t.show('A plain one.'); r.plain = plain.el.querySelector('.rk-toast-msg').tagName === 'P' && !plain.el.hasAttribute('data-rk-from');
    const quiet = t.show('No default action', { edge: true, icon: 'bell', onEnd: (why) => { ended = why; } }); await wait(300);
    r.icon = !!quiet.el.querySelector('.rk-toast-pic svg.rk-icon'); quiet.el.querySelector('.rk-toast-msg').click(); await wait(10);
    r.closes = ended === 'dismissed';
    t.destroy(); await wait(300); region.remove();
    return r;
  });
  ok(note.from === 'edge' && note.opacity && /translateY\(calc\(-100%/.test(note.edge) && note.inPlace, 'a notification enters whole from the edge, without a fade, to where toasts stand', JSON.stringify(note));
  ok(note.title === '600' && note.lines === 2 && note.whole, 'its title is semibold, its text cut after two lines and still all there', JSON.stringify(note));
  ok(note.pic && note.pic[0] === 20 && note.pic[1] === 20 && note.pic[2] === 'cover' && note.icon, 'the sender\'s icon or its own picture in the 20px field, cropped square', JSON.stringify(note));
  ok(note.button === 'BUTTON' && note.opened && note.out === 'rk-toast-edge-out' && note.closes && note.plain, 'pressing the text runs the default action (opened), else closes; it leaves the way it came; a plain toast is unchanged', JSON.stringify(note));

  // A menu's second level in place; an icon per item.
  await p.evaluate((icon) => {
    const wrap = document.createElement('div'); wrap.id = 't-menu'; wrap.style.cssText = 'position:fixed;left:40px;top:120px';
    wrap.innerHTML = `<button class="rk-btn" id="t-menu-btn" type="button">tray</button><div id="t-tray"><button class="rk-menu-item" role="menuitem" id="t-m-net" aria-haspopup="menu" aria-controls="t-lvl">${icon.replace('<svg', '<svg class="rk-icon"')}network</button>`
      + `<button class="rk-menu-item" role="menuitem" id="t-m-sound">sound</button><div class="rk-menu-level" id="t-lvl" role="menu" aria-label="network" hidden>`
      + '<button class="rk-menu-item" role="menuitemradio" aria-checked="true" id="t-m-home">Home</button><button class="rk-menu-item" role="menuitemradio" aria-checked="false" id="t-m-office">Office 5G</button></div></div>';
    document.querySelector('.rk').append(wrap);
    window.tMenuChosen = ''; window.tMenu = Rekta.menu(wrap.querySelector('#t-menu-btn'), wrap.querySelector('#t-tray'), { onSelect: (i) => { window.tMenuChosen = i.id; } });
  }, icon);
  await p.focus('#t-menu-btn'); await p.keyboard.press('Enter'); await p.waitForTimeout(250);
  const lv0 = await p.evaluate(() => {
    const m = document.querySelector('#t-tray'), net = m.querySelector('#t-m-net'), snd = m.querySelector('#t-m-sound'), words = (b) => { const t = document.createRange(); t.selectNodeContents(b.lastChild); return t.getBoundingClientRect().left; };
    return { focus: document.activeElement.id, chevron: getComputedStyle(net, '::after').content !== 'none', aligned: Math.abs(words(net) - words(snd)) < 1, w: m.getBoundingClientRect().width, l: m.getBoundingClientRect().left, t: m.getBoundingClientRect().top };
  });
  await p.keyboard.press('ArrowRight'); await p.waitForTimeout(100);
  const lv1 = await p.evaluate(() => {
    const m = document.querySelector('#t-tray'), back = m.querySelector('.rk-menu-back'), shown = (id) => m.querySelector(id).getClientRects().length > 0;
    return { focus: document.activeElement.id, back: back ? [back.textContent, back.getAttribute('aria-label'), shown('.rk-menu-back')] : null, list: shown('#t-m-net') || shown('#t-m-sound'), expanded: m.querySelector('#t-m-net').getAttribute('aria-expanded'),
      w: m.getBoundingClientRect().width, l: m.getBoundingClientRect().left, t: m.getBoundingClientRect().top, anims: m.getAnimations({ subtree: true }).filter((a) => a.playState === 'running').length };
  });
  ok(lv0.focus === 't-m-net' && lv0.chevron && lv0.aligned, 'an item with a level ends in a chevron; once one item has an icon, every row\'s words start at one edge', JSON.stringify(lv0));
  ok(lv1.focus === 't-m-home' && lv1.back && lv1.back[0] === 'network' && lv1.back[1] === 'back: network' && lv1.back[2] && !lv1.list && lv1.expanded === 'true', '→ opens the level in place, under a back row with its name', JSON.stringify(lv1));
  ok(Math.abs(lv1.w - lv0.w) < 1 && Math.abs(lv1.l - lv0.l) < 1 && Math.abs(lv1.t - lv0.t) < 1 && lv1.anims === 0, 'the surface keeps its width and place; nothing moves', JSON.stringify([lv0, lv1]));
  await p.keyboard.press('Escape'); await p.waitForTimeout(50);
  const lv2 = await p.evaluate(() => ({ focus: document.activeElement.id, open: document.querySelector('#t-tray').matches(':popover-open'), level: document.querySelector('#t-lvl').hidden }));
  ok(lv2.focus === 't-m-net' && lv2.open && lv2.level, 'Escape goes back one level, to the item that opened it', JSON.stringify(lv2));
  await p.keyboard.press('Enter'); await p.waitForTimeout(50); await p.keyboard.press('ArrowDown'); await p.keyboard.press('Enter'); await p.waitForTimeout(50);
  const lv3 = await p.evaluate(() => ({ chosen: window.tMenuChosen, open: document.querySelector('#t-tray').matches(':popover-open'), level: document.querySelector('#t-lvl').hidden, focus: document.activeElement.id }));
  ok(lv3.chosen === 't-m-office' && !lv3.open && lv3.level && lv3.focus === 't-menu-btn', 'a choice in the level closes every level, focus back to the control', JSON.stringify(lv3));
  await p.evaluate(() => { window.tMenu.destroy(); document.querySelector('#t-menu').remove(); });

  // A place called up turns at half the pace; the display step is twice nav-active, light, tabular.
  const called = await p.evaluate(async () => {
    const make = (name) => { const m = document.createElement('main'); m.className = 'rk-place'; m.dataset.rkPlace = name; m.style.cssText = 'position:fixed;inset:0;background:var(--rk-bg);z-index:5'; m.innerHTML = '<h1 class="rk-title">A place</h1><div class="rk-tiles"><a class="rk-tile" href="#">one</a><a class="rk-tile" href="#">two</a></div>'; document.body.append(m); return m; };
    // The pace is read from the durations the turn gives its parts, which a busy runner does not stretch; a wall-clock
    // ratio would carry the fixed cost of a frame or two in every turn.
    const dur = (place) => parseFloat(place.querySelector('.rk-tile').style.getPropertyValue('--_rk-turn-duration'));
    const time = async (opts) => {
      const a = make('t-a'), b = make('t-b'); b.hidden = true;
      const out = Rekta.turn.leave(a, opts), dOut = dur(a); await out;
      const into = Rekta.turn.enter(b, opts), dIn = dur(b); await into;
      a.remove(); b.remove(); return dOut + dIn;
    };
    const full = await time({}), half = await time({ called: true });
    const a = make('t-c'); a.setAttribute('data-rk-called', ''); a.hidden = true;
    const plain = make('t-d'); plain.hidden = true;
    const p1 = Rekta.turn.enter(plain), dPlain = dur(plain); await p1; plain.remove();
    const p2 = Rekta.turn.enter(a), byAttr = dur(a) / dPlain; await p2; a.remove();
    const d = document.createElement('p'); d.className = 'rk-display'; d.textContent = '12:45'; document.querySelector('.rk-place:not([hidden])').append(d);
    const s = getComputedStyle(d), r = { full, half, byAttr, size: parseFloat(s.fontSize), weight: s.fontWeight, nums: s.fontVariantNumeric, nav: 2 * Rekta.tokens.px('--rk-font-size-nav-active') };
    d.remove();
    return r;
  });
  ok(Math.abs(called.half / called.full - 0.5) < 0.02 && Math.abs(called.byAttr - 0.5) < 0.02, 'a place called up (data-rk-called) turns at half the pace', JSON.stringify(called));
  ok(Math.abs(called.size - called.nav) < 1 && called.weight === '300' && called.nums === 'tabular-nums', '.rk-display: twice nav-active, light, tabular figures', JSON.stringify(called));
  // A month to look at (#127): day-cell squares, seven wide, today framed 2px inside its cell.
  const month = await p.evaluate(() => {
    const t = document.createElement('table'); t.className = 'rk-month';
    t.innerHTML = '<caption>October 2026</caption><thead><tr>' + ['Mo', 'Tu', 'We', 'Th', 'Fr', 'Sa', 'Su'].map((d) => `<th scope="col">${d}</th>`).join('') + '</tr></thead>'
      + '<tbody><tr><td></td><td></td><td></td><td>1</td><td>2</td><td>3</td><td aria-current="date">4</td></tr></tbody>';
    document.querySelector('.rk-place:not([hidden])').append(t);
    const cell = t.querySelector('td').getBoundingClientRect(), today = getComputedStyle(t.querySelector('[aria-current]'));
    const r = { cell: [cell.width, cell.height], width: t.getBoundingClientRect().width, unit: Rekta.tokens.px('--rk-day-cell'), frame: [today.outlineWidth, today.outlineOffset, today.outlineStyle] };
    t.remove();
    return r;
  });
  // A preview tile (#123): two units wide, the app's picture fills the top, the title one line and cut at the foot.
  const prev = await p.evaluate(() => {
    const g = document.createElement('div'); g.className = 'rk-tiles'; g.style.cssText = 'width:800px';
    const pic = 'data:image/svg+xml,' + encodeURIComponent('<svg xmlns="http://www.w3.org/2000/svg" width="160" height="90"><rect width="160" height="90" fill="#36c"/></svg>');
    g.innerHTML = `<button class="rk-tile rk-tile--preview" type="button"><span class="rk-tile-preview"><img src="${pic}" alt=""></span><span class="rk-tile-foot"><svg class="rk-icon" aria-hidden="true"><use href="#rk-i-file"/></svg><span>A very long title of an open document that does not fit on one line at all.odt</span></span></button>`;
    document.querySelector('.rk-place:not([hidden])').append(g);
    const t = g.firstChild, tr = t.getBoundingClientRect(), pr = t.querySelector('.rk-tile-preview').getBoundingClientRect(), title = t.querySelector('.rk-tile-foot > span');
    const unit = Rekta.tokens.px('--rk-tile-unit'), gap = Rekta.tokens.px('--rk-space-s');
    const r = { w: tr.width, want: 2 * unit + gap, top: pr.top - tr.top, fills: Math.abs(pr.width - tr.width) < 1 && pr.height > tr.height / 2, cut: title.scrollWidth > title.clientWidth && getComputedStyle(title).textOverflow === 'ellipsis' && title.getBoundingClientRect().height < 30 };
    g.remove();
    return r;
  });
  ok(Math.abs(prev.w - prev.want) < 1 && prev.top < 1 && prev.fills && prev.cut, 'a preview tile: two units wide, the preview fills its top, the title one line and cut', JSON.stringify(prev));
  // The session's actions (#132): a sheet across the full width at the top, on surface, in the banner's geometry.
  const sess = await p.evaluate(async () => {
    const d = document.createElement('dialog'); d.className = 'rk-sheet rk-session'; d.setAttribute('aria-labelledby', 't-s-word');
    d.innerHTML = '<form method="dialog"><span class="rk-banner-head"><span class="rk-banner-word" id="t-s-word">session</span></span><p class="rk-actions"><button class="rk-btn rk-btn--primary" value="lock">lock</button><button class="rk-btn rk-btn--quiet" value="off">power off</button></p>'
      + '<button class="rk-btn rk-btn--icon rk-btn--quiet rk-sheet-close" value="cancel" aria-label="close">×</button></form>';
    document.querySelector('.rk').append(d);
    const s = Rekta.sheet(d); d.showModal(); await new Promise((r) => setTimeout(r, 400));
    const r = d.getBoundingClientRect(), word = d.querySelector('.rk-banner-word').getBoundingClientRect(), acts = d.querySelector('.rk-actions').getBoundingClientRect();
    const probe = document.createElement('i'); probe.style.color = 'var(--rk-surface)'; document.body.append(probe); const surface = getComputedStyle(probe).color; probe.remove();
    const out = { top: r.top, width: Math.abs(r.width - document.documentElement.clientWidth), bg: getComputedStyle(d).backgroundColor === surface, row: acts.left > word.right && Math.abs(acts.top - word.top) < 40, anim: d.getAnimations().map((a) => a.animationName) };
    d.close(); await new Promise((res) => setTimeout(res, 400)); s.destroy(); d.remove();
    return out;
  });
  ok(sess.top < 1 && sess.width < 2 && sess.bg && sess.row && sess.anim.includes('rk-banner-in'), 'the session bar: full width at the top, on surface, the word then the actions, in as the banner comes', JSON.stringify(sess));
  ok(month.unit === 36 && Math.abs(month.cell[0] - 36) < 0.5 && Math.abs(month.cell[1] - 36) < 0.5 && Math.abs(month.width - 7 * 36) < 0.5 && month.frame.join() === '1px,-3px,solid', '.rk-month: day-cell squares, seven wide, today framed inside its cell', JSON.stringify(month));
}
await p.mouse.move(5, 5);
await p.click('#tree > li:nth-child(1) > .rk-tree-row'); await p.waitForTimeout(100);
await p.keyboard.press('ArrowDown'); await p.keyboard.press('ArrowDown'); await p.waitForTimeout(100);
ok((await p.evaluate(() => document.activeElement.querySelector(':scope > .rk-tree-row').textContent.trim())) === 'src', '↓ moves through the tree');
await p.keyboard.press('ArrowDown'); await p.keyboard.press('ArrowRight'); await p.waitForTimeout(150);
ok((await p.evaluate(() => document.activeElement.getAttribute('aria-expanded'))) === 'true', '→ opens a folder');
await p.keyboard.press('ArrowRight'); await p.keyboard.press('Enter'); await p.waitForTimeout(150);
ok((await text(p, '#file-name')) === 'rekta.css' && (await text(p, '#file-path')) === 'src / css', '→ goes in, Enter shows the file', `${await text(p, '#file-path')} ${await text(p, '#file-name')}`);
await p.keyboard.press('ArrowLeft'); await p.keyboard.press('ArrowLeft'); await p.waitForTimeout(150);
ok((await p.evaluate(() => document.activeElement.getAttribute('aria-expanded'))) === 'false', '← goes up, then closes');
await p.waitForTimeout(1100); await p.keyboard.press('s'); await p.keyboard.press('i'); await p.waitForTimeout(100);
ok((await p.evaluate(() => document.activeElement.querySelector(':scope > .rk-tree-row').textContent.trim())) === 'site', 'letters typed together jump in the tree: "si" finds site');
await p.click('#tree > li:nth-child(1) li > .rk-tree-row', { button: 'right' }); await p.waitForTimeout(200);
ok(await isOpen(p, '#tree-menu') && (await text(p, '#file-name')) === 'rekta.tokens.json', 'a right click on a file selects it and opens its menu');
await p.keyboard.press('Escape'); await p.waitForTimeout(150);
const order = () => p.evaluate(() => [...document.querySelectorAll('#steps b')].map((b) => b.textContent));
await p.focus('#steps li:nth-child(1) .rk-grip'); await p.keyboard.press(' '); await p.keyboard.press('ArrowDown'); await p.keyboard.press('ArrowDown'); await p.keyboard.press(' '); await p.waitForTimeout(400);
ok((await order())[2] === 'write the release notes' && (await p.evaluate(() => document.activeElement.classList.contains('rk-grip'))), 'Space picks up, ↓ moves, Space puts down; focus stays on the grip');
const g = await p.$('#steps li:nth-child(5) .rk-grip'), gb = await g.boundingBox(), topBox = await (await p.$('#steps li:nth-child(1)')).boundingBox();
await p.mouse.move(gb.x + gb.width / 2, gb.y + gb.height / 2); await p.mouse.down();
await p.mouse.move(gb.x + gb.width / 2, gb.y - 20, { steps: 4 }); await p.mouse.move(gb.x + gb.width / 2, topBox.y + 4, { steps: 8 }); await p.waitForTimeout(150);
ok(await p.evaluate(() => document.querySelector('#steps [data-rk-dragging]') !== null), 'a dragged item lifts');
await p.mouse.up(); await p.waitForTimeout(400);
ok((await order())[0] === 'publish the tokens' && (await toast(p)).startsWith('Moved “publish the tokens” to step 1'), 'dropped at the top', JSON.stringify(await order()));
await p.keyboard.press('Control+z'); await p.waitForTimeout(400);
ok((await order())[4] === 'publish the tokens', 'and undone');
ok((await p.evaluate(() => document.querySelectorAll('#log .rk-virtual-row').length)) < 40 && (await text(p, '#log-facts')).startsWith('10,000 of 10,000'), 'ten thousand lines, only a few in the page');
await p.click('#log .rk-virtual-row:nth-child(3)'); await p.waitForTimeout(150);
ok((await text(p, '#log-shown')).startsWith('Line 3 · 00:00:14'), 'a click on a line selects and shows it', await text(p, '#log-shown'));
await p.keyboard.press('End'); await p.waitForTimeout(300);
ok((await p.evaluate(() => document.querySelector('#log [data-rk-active]') && document.querySelector('#log [data-rk-active]').getAttribute('aria-posinset'))) === '10000', 'End reaches line 10 000; it exists once it is in view');
const logAt = () => p.evaluate(() => { const r = document.querySelector('#log [data-rk-active]'); return r ? `${r.getAttribute('aria-posinset')} ${r.querySelector('.rk-tag').dataset.status}` : '(none)'; });
await p.waitForTimeout(1100); await p.keyboard.press('f'); await p.waitForTimeout(150); const f1 = await logAt();
await p.keyboard.press('f'); await p.waitForTimeout(150); const f2 = await logAt();
ok(/ error$/.test(f1) && / error$/.test(f2) && +f2.split(' ')[0] > +f1.split(' ')[0], 'letters jump in the long list: f to the first failed line, f again to the next', `${f1} / ${f2}`);
await p.click('.rk-filters input[value=error]', { force: true }); await p.waitForTimeout(200);
ok(/^\d+ of 10,000/.test(await text(p, '#log-facts')) && (await p.evaluate(() => [...document.querySelectorAll('#log .rk-tag')].every((t) => t.dataset.status === 'error'))), 'segments filter the long list', await text(p, '#log-facts'));
await p.click('#log .rk-virtual-row:nth-child(1)'); await p.click('#log .rk-virtual-row:nth-child(4)', { modifiers: ['Control'] }); await p.waitForTimeout(150);
ok((await text(p, '.rk-ab-count')) === '2 selected' && await mode(), 'Metro selection in the long list');
await p.keyboard.press('Escape'); await p.waitForTimeout(150);
ok(!(await mode()), 'Escape ends it');

// Live and undo: fresh, held, stale, offline; the history.
await go(p, 'live-undo');
ok((await p.getAttribute('#live-region', 'data-rk-live')) === 'fresh' && (await text(p, '#live-region .rk-age')).startsWith('updated'), 'a live region says how fresh it is');
await p.hover('#lv-rps'); await p.waitForTimeout(3300);
ok((await p.evaluate(() => document.querySelector('#live-region').hasAttribute('data-rk-held'))) && /updates? waiting/.test(await text(p, '.rk-live-more')), 'pointing holds it still; updates wait behind a button', await text(p, '.rk-live-more'));
const heldValue = await text(p, '#lv-rps');
await p.waitForTimeout(1600);
ok((await text(p, '#lv-rps')) === heldValue, 'held values do not change');
await p.mouse.move(5, 5); await p.waitForTimeout(200);
ok(!(await p.evaluate(() => document.querySelector('#live-region').hasAttribute('data-rk-held'))) && (await p.evaluate(() => document.querySelector('.rk-live-more').hidden)), 'let go, the waiting updates apply');
await p.click('label.rk-toggle:has(#lv-online)'); await p.waitForTimeout(300);
ok((await p.getAttribute('#live-region', 'data-rk-live')) === 'offline' && (await text(p, '#live-region .rk-age')).startsWith('offline'), 'offline says so');
await p.click('label.rk-toggle:has(#lv-online)'); await p.click('label.rk-toggle:has(#lv-feed)'); await p.waitForTimeout(9800);
ok((await p.getAttribute('#live-region', 'data-rk-live')) === 'stale' && (await text(p, '#live-region .rk-age')).startsWith('stale · no update for'), 'no update for a while: stale, and it says so', await text(p, '#live-region .rk-age'));
await p.click('label.rk-toggle:has(#lv-feed)'); await p.waitForTimeout(1700);
ok((await p.getAttribute('#live-region', 'data-rk-live')) === 'fresh', 'fresh again with the next update');
ok((await p.evaluate(() => [...document.querySelectorAll('#h-list li')].map((l) => l.firstChild.textContent))).includes('Moved Tile to “surfaces”'), 'the history lists what you did');
await p.click('#h-undo'); await p.waitForTimeout(300);
ok((await p.evaluate(() => !!document.querySelector('#h-list li[data-undone]'))) && (await p.getAttribute('#h-redo', 'aria-disabled')) === 'false', 'undo from the history; redo can act');

// Feedback: toasts with undo, pending, the banner.
await go(p, 'feedback');
await p.click('#f-undo'); await p.waitForTimeout(300);
await p.click('#feedback-title'); await p.keyboard.press('Control+z'); await p.waitForTimeout(500);
ok((await toast(p)) === 'It is back.', 'Ctrl+Z undoes', await toast(p));
await p.click('#f-pending'); await p.waitForTimeout(100);
ok((await text(p, '#f-pending')) === 'saving…', 'pending: the button says what it does', await text(p, '#f-pending'));
await p.waitForTimeout(1700);
ok((await toast(p)) === 'Saved.' && (await text(p, '#f-pending')) === 'save', 'pending ends, label back');
await p.check('#s-halted'); await p.waitForTimeout(500);
ok((await text(p, '.rk-banner-word')) === 'halted' && await p.evaluate(() => !!document.querySelector('#banner [role=alert] .rk-banner-word')) && (await p.getAttribute('#bar', 'data-rk-state')) === 'halted', 'halted: an alert, the app bar follows');
await p.click('#ab-pause'); await p.waitForTimeout(400);
ok((await text(p, '.rk-banner-word')) === 'halted' && (await toast(p)).startsWith('Live data paused'), 'pause under halt: the banner holds, a toast says so', await toast(p));
await p.screenshot({ path: join(shots, 'halted.png') });
await p.click('#remedy'); await p.waitForTimeout(600);
ok((await text(p, '.rk-banner-word')) === 'paused', 'release steps down to paused');
await p.click('#remedy'); await p.waitForTimeout(800);
ok((await p.getAttribute('#banner', 'data-rk-phase')) === 'clear', 'resume clears the banner');
await p.check('#s-offline'); await p.waitForTimeout(400);
ok((await text(p, '#data-now')) === 'frozen', 'offline freezes the figures');
await p.click('#remedy'); await p.waitForTimeout(1500);
ok((await p.getAttribute('#banner', 'data-rk-phase')) === 'clear', 'retry reconnects');

// The toaster for server-side undo and failures; the focus fallback; the banner's real start and a remedy that links.
const tst = await p.evaluate(async () => {
  const wait = (t) => new Promise((r) => setTimeout(r, t));
  const region = document.createElement('div'); region.className = 'rk-toasts'; region.setAttribute('role', 'status');
  document.querySelector('.rk').append(region);
  const t = Rekta.toaster(region), ends = [], holds = [];
  region.addEventListener('rk:toasthold', (e) => holds.push(e.detail.held));
  const plain = t.show('Plain.'); await wait(50);
  const plainBg = getComputedStyle(plain.el).backgroundColor;
  const err = t.show('Could not save the draft.', { status: 'error', onEnd: (w) => ends.push(w) }); await wait(50);
  const status = err.el.dataset.status === 'error' && err.el.getAttribute('role') === 'alert' && getComputedStyle(err.el).backgroundColor !== plainBg;
  let undone = 0;
  const u = t.show('3 deleted', { undo: () => { undone += 1; }, windowMs: 3000, onEnd: (w) => ends.push(w) }); await wait(400);
  const count = () => u.el.querySelector('.rk-toast-count').textContent;
  const first = count();
  u.setRemaining(1500); const set = count();
  u.el.dispatchEvent(new PointerEvent('pointerenter')); const r0 = u.remaining; await wait(600);
  const held = u.held && u.el.hasAttribute('data-rk-held') && Math.abs(u.remaining - r0) < 5;
  u.setRemaining(0); await wait(50);
  const t2 = t.show('2 archived', { undo: () => { undone += 1; }, onEnd: (w) => ends.push(w) }); await wait(50); t2.undo(); await wait(400);
  // Focus comes back to the fallback when the control that raised the toast is gone.
  const fb = document.createElement('p'); fb.textContent = 'fallback'; fb.setAttribute('data-rk-focus-fallback', '');
  document.querySelector('#feedback').append(fb);
  const gone = document.createElement('button');
  const t3 = t.show('Sent.', { undo: () => {}, from: gone }); await wait(500);   /* in once the one before has gone */
  t3.el.querySelector('button').focus(); t.dismiss(); await wait(50);
  const fellBack = document.activeElement === fb;
  fb.remove(); t.destroy(); await wait(300); region.remove();
  return { status, first, set, held, holds: holds.join(), ends: ends.join(), undone, fellBack };
});
ok(tst.status, 'an error toast takes the error fill and is an alert');
ok(tst.first === '3' && tst.set === '2', 'an undo window from windowMs, set by setRemaining', `${tst.first} ${tst.set}`);
ok(tst.held && tst.holds.startsWith('true'), 'a held toast says so and its remaining time stands still', tst.holds);
ok(tst.ends === 'replaced,expired,undone' && tst.undone === 1, 'onEnd says why a toast went; setRemaining(0) ends it while held', `${tst.ends} ${tst.undone}`);
ok(tst.fellBack, 'focus goes to data-rk-focus-fallback when the toast\'s control is gone');
const tst2 = await p.evaluate(async () => {
  const wait = (t) => new Promise((r) => setTimeout(r, t));
  const region = document.createElement('div'); region.className = 'rk-toasts'; region.setAttribute('role', 'status');
  document.querySelector('.rk').append(region);
  const t = Rekta.toaster(region), out = {};
  // A busy page does not stretch the window: it ends on the clock.
  const t0 = Date.now(); let endedAt = 0;
  t.show('1 deleted', { undo: () => {}, windowMs: 3000, onEnd: () => { endedAt = Date.now() - t0; } });
  await wait(300); const block = Date.now(); while (Date.now() - block < 2500) { /* busy */ }
  for (let i = 0; i < 40 && !endedAt; i += 1) await wait(50);
  out.deadline = endedAt; await wait(400);
  // The same key: the first onEnd hears 'merged', the second the end.
  const ends = [];
  t.show('1 archived', { key: 'arch', undo: () => {}, onEnd: (w) => ends.push(`a1:${w}`) });
  t.show('2 archived', { key: 'arch', undo: () => {}, onEnd: (w) => ends.push(`a2:${w}`) });
  t.dismiss(); out.merged = ends.join(); await wait(400);
  // The same key without an onEnd of its own: the first onEnd stands for both and hears the end, once.
  const kept = [];
  t.show('1 moved', { key: 'mv', undo: () => {}, onEnd: (w) => kept.push(`m1:${w}`) });
  t.show('2 moved', { key: 'mv', undo: () => {} });
  await wait(100); t.dismiss(); await wait(400); out.kept = kept.join();
  // release(): held, then counting although the pointer stays; setText leaves the window alone.
  const r = t.show('3 sent', { undo: () => {}, windowMs: 5000 }); await wait(400);
  r.el.dispatchEvent(new PointerEvent('pointerenter')); await wait(300);
  const heldBefore = r.held, a = r.remaining; await wait(400); const b = r.remaining;
  r.release(); await wait(500); const c = r.remaining;
  r.setText('3 sent. Holding no longer extends.'); const live = r.el.getAttribute('aria-live'); const d = r.remaining;
  out.release = { heldBefore, stood: Math.abs(a - b) < 20, runs: c < b - 300, held: r.held, attr: r.el.hasAttribute('data-rk-held'), live, kept: d <= c && d > c - 200, text: r.el.querySelector('.rk-toast-msg').firstChild.nodeValue };
  t.destroy(); region.remove();
  return out;
});
ok(tst2.deadline > 2700 && tst2.deadline < 3500, 'a busy page does not stretch a toast\'s window', tst2.deadline);
ok(tst2.merged === 'a1:merged,a2:dismissed', 'a same-key toast tells the first onEnd it merged', tst2.merged);
ok(tst2.kept === 'm1:dismissed', 'a same-key toast without its own onEnd keeps the first one, called once with the end', tst2.kept);
ok(tst2.release.heldBefore && tst2.release.stood && tst2.release.runs && !tst2.release.held && !tst2.release.attr, 'release() lets a held toast run out', JSON.stringify(tst2.release));
ok(tst2.release.live === 'off' && tst2.release.kept && tst2.release.text === '3 sent. Holding no longer extends.', 'setText changes the words quietly and leaves the window alone', JSON.stringify(tst2.release));
const ban = await p.evaluate(async () => {
  const wait = (t) => new Promise((r) => setTimeout(r, t));
  const h = document.documentElement.style.getPropertyValue('--rk-banner-h');
  const el = document.createElement('div'); el.className = 'rk-banner';
  el.innerHTML = '<div class="rk-banner-live"><p role="alert"></p><p role="status"></p></div><p class="rk-actions"><button class="rk-btn" type="button">x</button></p>';
  document.querySelector('.rk').prepend(el);
  const b = Rekta.banner(el, { order: ['halted', 'signedout'], states: { signedout: { word: 'signed out', status: 'warning', text: 'Sign in again.', action: { label: 'sign in', href: '/login' } }, halted: { word: 'halted', status: 'error', text: 'Stopped.', action: 'release' } } });
  b.set('signedout', true, { since: false }); await wait(50);
  const link = el.querySelector('a.rk-btn');
  const r = { noTime: !el.querySelector('.rk-banner-time'), link: !!link && !link.hidden && link.getAttribute('href') === '/login' && link.textContent === 'sign in' && el.querySelector('button.rk-btn').hidden };
  b.set('halted', true, { since: new Date(2026, 0, 2, 9, 5) }); await wait(50);
  const time = el.querySelector('.rk-banner-time');
  r.since = time ? time.textContent : '(none)'; r.dt = time ? time.dateTime : '';
  r.button = !el.querySelector('button.rk-btn').hidden && el.querySelector('a.rk-btn').hidden && el.querySelector('button.rk-btn').textContent === 'release';
  const timeNode = el.querySelector('.rk-banner-time');
  b.set('halted', true, { text: 'Stopped: 3 runs.' }); const m = el.querySelector('.rk-banner-msg');
  r.update = m.textContent === 'Stopped: 3 runs.' && m.getAttribute('aria-live') === 'off' && el.querySelector('.rk-banner-time') === timeNode;
  b.destroy(); el.remove(); document.documentElement.style.setProperty('--rk-banner-h', h);
  return r;
});
ok(ban.noTime, 'a banner state whose start is unknown has no time line');
ok(ban.update, 'a state that is on takes a new sentence in place, quietly, its start kept');
ok(ban.link, 'a remedy with an href is a link');
ok(/since .*2.*09:05|since .*09:05.*2/.test(ban.since) && ban.dt === new Date(2026, 0, 2, 9, 5).toISOString() && ban.button, 'a banner state gives its real start, with the day', `${ban.since} ${ban.dt}`);

// Navigation: the palette, the shortcut sheet, a radio menu.
await p.keyboard.press('Control+k'); await p.waitForTimeout(300);
ok(await isOpen(p, '#palette') && await p.evaluate(() => document.activeElement === document.querySelector('#palette input')), 'Ctrl+K opens the palette');
const pb = await p.evaluate(() => ({ pal: document.querySelector('#palette').getBoundingClientRect().bottom, bar: document.querySelector('#bar').getBoundingClientRect().top }));
ok(pb.pal <= pb.bar + 1, 'the palette rises from the app bar', JSON.stringify(pb));
await p.keyboard.type('release'); await p.waitForTimeout(150);
const activeOpt = () => p.evaluate(() => { const id = document.activeElement.getAttribute('aria-activedescendant'); return id ? document.getElementById(id).textContent : null; });
ok((await activeOpt()) === 'Release notes', 'typing filters', await activeOpt());
await p.keyboard.press('Escape');
ok((await p.inputValue('#palette input')) === '' && await isOpen(p, '#palette'), 'Escape clears first');
await p.keyboard.type('--rk-accent'); await p.waitForTimeout(150);
ok((await activeOpt()) === '--rk-accent', 'a token in the palette', await activeOpt());
await p.keyboard.press('Enter'); await p.waitForTimeout(TURN);
ok(await visible(p, 'tokens') && (await p.evaluate(() => location.hash)) === '#tokens/accent', 'the palette goes to a token', await p.evaluate(() => location.hash));
await p.keyboard.press('Escape'); await p.waitForTimeout(TURN);
ok(await visible(p, 'home'), 'and back');
await p.locator('#navigation-title').focus(); await p.keyboard.press('?'); await p.waitForTimeout(300);
ok((await sheetTitle(p)) === 'Keyboard shortcuts', '? opens the shortcuts');
await p.keyboard.press('Escape'); await p.waitForTimeout(300);
await go(p, 'navigation');
await p.click('#n-sort'); await p.waitForTimeout(200); await p.click('#n-sort-menu [data-sort="2"]'); await p.waitForTimeout(500);
ok((await p.evaluate(() => document.querySelector('#tok-body tr td:nth-child(3)').textContent)) === 'breakpoint', 'a radio menu sorts the tokens by group');
await p.click('#n-sort'); await p.waitForTimeout(200); await p.click('#n-sort-menu [data-sort="1"]'); await p.waitForTimeout(300);
// Floating surfaces by one rule (states.md): space-xs from the control, space-m from the window; one that fits on
// neither side takes the side with more room and stays inside the window; a centred control centres its surface.
const flo = await p.evaluate(async () => {
  const wait = (t) => new Promise((r) => setTimeout(r, t));
  const px = (n) => Rekta.tokens.px(n), edge = px('--rk-space-m'), gap = px('--rk-space-xs'), vh = innerHeight, vw = document.documentElement.clientWidth;
  const rows = (n) => '<p style="height:44px;margin:0">row</p>'.repeat(n);
  const host = document.createElement('div'); host.innerHTML = '<style>#fx-float { max-height: 500px } #fx-tall { max-height: none }</style>'
    + '<button id="fx-ctl" class="rk-btn" style="position:fixed;left:40%;top:400px;height:40px">ctl</button>'
    + `<div id="fx-float" class="rk-float" popover="manual">${rows(20)}</div><div id="fx-small" class="rk-float" popover="manual">${rows(2)}</div>`
    + `<div id="fx-tall" class="rk-float" popover="manual">${rows(30)}</div>`;
  document.querySelector('.rk').append(host);
  const ctl = host.querySelector('#fx-ctl'), f = host.querySelector('#fx-float'), small = host.querySelector('#fx-small'), tall = host.querySelector('#fx-tall'), out = {};
  // Where place() put a surface, without the 4px shift of its entry.
  const box = (el) => { const l = parseFloat(el.style.left), t = parseFloat(el.style.top); return { left: l, top: t, width: el.offsetWidth, right: l + el.offsetWidth, bottom: t + el.offsetHeight }; }, c = ctl.getBoundingClientRect();
  // 500px under a control at 400-440 in a 900px window: 440 below, 380 above, so neither; below has more room.
  f.showPopover(); Rekta.place(f, ctl);
  out.nowhere = { side: f.dataset.rkSide, top: box(f).top, bottom: box(f).bottom, inside: box(f).top >= edge - 0.5 && box(f).bottom <= vh - edge + 0.5 };
  f.hidePopover();
  small.showPopover(); Rekta.place(small, ctl);
  out.below = { side: small.dataset.rkSide, gap: box(small).top - c.bottom, left: box(small).left - c.left };
  small.hidePopover();
  tall.showPopover(); Rekta.place(tall, ctl);
  out.tall = { top: box(tall).top, bottom: box(tall).bottom, inside: box(tall).top >= edge - 0.5 && box(tall).bottom <= vh - edge + 0.5, scrolls: tall.scrollHeight > tall.clientHeight };
  tall.hidePopover();
  // A control in the middle of its bar: menu() places by the trigger's data-rk-align.
  ctl.setAttribute('data-rk-align', 'center');
  const m = document.createElement('div'); m.innerHTML = '<button class="rk-menu-item" role="menuitem">one</button><button class="rk-menu-item" role="menuitem">two</button>';
  host.append(m);
  const mm = Rekta.menu(ctl, m); mm.open(); await wait(250);
  const mc = box(m);
  out.centre = { menu: mc.left + mc.width / 2, ctl: c.left + c.width / 2, side: m.dataset.rkSide };
  mm.close(); ctl.style.left = `${vw - 30}px`; mm.open(); await wait(250);
  out.atEdge = { right: box(m).right, limit: vw - edge };
  mm.close(); mm.destroy(); host.remove();
  return { ...out, gap, vh };
});
ok(flo.nowhere.side === 'below' && flo.nowhere.inside, 'a surface that fits on neither side takes the side with more room and stays in the window', JSON.stringify(flo.nowhere));
ok(flo.below.side === 'below' && Math.abs(flo.below.gap - flo.gap) < 1 && Math.abs(flo.below.left) < 1, 'below its control by space-xs, at its leading edge', JSON.stringify(flo.below));
ok(flo.tall.inside && flo.tall.scrolls, 'a surface taller than the window keeps to it and scrolls', JSON.stringify(flo.tall));
ok(Math.abs(flo.centre.menu - flo.centre.ctl) <= 1, 'a menu under a control marked data-rk-align="center" is centred on it', JSON.stringify(flo.centre));
ok(flo.atEdge.right <= flo.atEdge.limit + 0.5, 'centred near the window\'s edge, it stays inside', JSON.stringify(flo.atEdge));

// Forms: make an accent. Late checks, the summary, real contrast, server refusals, the guard, confirm, disable.
await go(p, 'forms');
ok((await p.getAttribute('#a-remove', 'aria-disabled')) === 'true', 'remove says why it cannot act yet');
await p.click('#a-save'); await p.waitForTimeout(300);
ok((await text(p, '.rk-errors > p')) === '2 fields need attention' && await p.evaluate(() => document.activeElement.classList.contains('rk-errors')), 'sending empty: a summary takes focus', await text(p, '.rk-errors > p'));
ok(await p.evaluate(() => { const b = document.querySelector('.rk-errors'), h = b && document.getElementById(b.getAttribute('aria-labelledby'));
  return b === b.parentElement.firstElementChild && b.getAttribute('role') === 'group' && b.getAttribute('tabindex') === '-1' && h && h.tagName === 'P' && [...b.querySelectorAll('ul > li > a')].every((a) => /^#a-/.test(a.getAttribute('href'))); }), 'the summary has the markup forms.md gives');
await p.click('.rk-errors a[href="#a-light"]');
ok((await focused(p)) === 'a-light', 'a summary link focuses its field');
await p.fill('#a-light', '#f7cb47'); await p.waitForTimeout(100);
const hint = (id) => p.evaluate((i) => document.querySelector(`#${i}`).closest('.rk-field').querySelector('.rk-hint').textContent, id);
ok((await hint('a-light')).includes(':1 on the light ground'), 'the contrast is checked for real', await hint('a-light'));
// A colour that reads as a status is refused with the status and a colour that holds (docs/colour.md, #97 #152).
await p.fill('#a-light', '#1d6b80'); await p.waitForTimeout(100);
ok(/too close to the status colour for “done”.*Try #[0-9a-f]{6}/.test(await hint('a-light')), 'a colour too close to a status is refused, with the status and a derived one', await hint('a-light'));
await p.fill('#a-light', '#2762ec'); await p.waitForTimeout(100);
ok((await hint('a-light')).includes('“active”') && (await hint('a-light')).includes('Its hue is a status hue'), 'a status hue cannot be derived away', await hint('a-light'));
await p.fill('#a-light', '#1a6680'); await p.waitForTimeout(100);
ok(!(await p.evaluate(() => document.querySelector('#a-light').closest('.rk-field').dataset.status)), 'corrected as you type');
await p.fill('#a-name', 'teal'); await p.click('#a-save'); await p.waitForTimeout(1200);
ok((await hint('a-name')).includes('already exists'), 'the server says no at the field', await hint('a-name'));
await p.fill('#a-name', 'sea'); await p.click('#a-save');
await p.waitForFunction(() => { const t = document.querySelector('.rk-toast:not([data-rk-phase]) .rk-toast-msg'); return t && t.textContent.startsWith('Accent'); }, null, { timeout: 4000 }).catch(() => {});
await until(p, () => p.evaluate(() => document.documentElement.dataset.accent === 'custom'), 2000);   /* the accent pref cross-fades in */
ok((await toast(p)).startsWith('Accent “sea” applied') && (await p.evaluate(() => getComputedStyle(document.documentElement).getPropertyValue('--rk-accent').trim())) === '#1a6680', 'the accent applies', `${await toast(p)} / ${await p.evaluate(() => getComputedStyle(document.documentElement).getPropertyValue('--rk-accent').trim() + ' ' + document.documentElement.dataset.accent + ' ' + localStorage.getItem('rekta-prefs'))}`);
ok(!(await p.getAttribute('#a-remove', 'aria-disabled')), 'remove can act now');
await p.click('#a-remove'); await p.waitForTimeout(400);
ok((await sheetTitle(p)) === 'Remove “sea”?' && (await p.evaluate(() => document.activeElement.textContent)) === 'keep it', 'confirm: the safe answer first');
const okLook = await p.evaluate(() => {
  const rgb = (c) => { const e = document.createElement('i'); e.style.color = c; document.body.append(e); const v = getComputedStyle(e).color; e.remove(); return v; };
  const b = document.querySelector('dialog[open] button[value=ok]'), s = getComputedStyle(b), red = rgb('var(--rk-status-error-full)');
  return { status: b.dataset.status, words: s.color === red, frame: s.borderTopColor === red, filled: s.backgroundColor !== 'rgba(0, 0, 0, 0)', primary: b.classList.contains('rk-btn--primary') };
});
ok(okLook.status === 'error' && okLook.words && okLook.frame && !okLook.filled && !okLook.primary, 'confirm({ safe: \'cancel\' }): ok is the destructive button, framed in the error colour (#101)', JSON.stringify(okLook));
await p.click('dialog[open] button[value=ok]');
await until(p, () => p.evaluate(() => !document.querySelector('dialog[open]')), 3000);   /* the sheet leaves first; its answer counts once it has gone */
ok((await p.evaluate(() => document.documentElement.dataset.accent)) === undefined && (await p.getAttribute('#a-remove', 'aria-disabled')) === 'true', 'removed, ink again', await p.evaluate(() => `accent ${document.documentElement.dataset.accent} remove ${document.querySelector('#a-remove').getAttribute('aria-disabled')} open ${!!document.querySelector('dialog[open]')} phase ${document.querySelector('dialog[open]')?.getAttribute('data-rk-phase')}`));

// Buttons: a running one keeps its colours, a destructive one is framed in the error colour, an icon button is square.
const btns = await p.evaluate(async () => {
  const rgb = (c) => { const e = document.createElement('i'); e.style.color = c; document.body.append(e); const v = getComputedStyle(e).color; e.remove(); return v; };
  const row = document.createElement('p'); row.className = 'rk-actions';
  row.innerHTML = '<button class="rk-btn rk-btn--primary" type="button">save</button><button class="rk-btn" type="button" data-status="error">delete</button><button class="rk-btn rk-btn--quiet" type="button">resend</button>';
  document.querySelector('#forms').append(row);
  const [save, del, quiet] = row.children, before = getComputedStyle(save).backgroundColor, quietBefore = [getComputedStyle(quiet).borderTopColor, getComputedStyle(quiet).color];
  let release; const run = Rekta.pending(save, new Promise((r) => { release = r; }));
  let release2; const run2 = Rekta.pending(quiet, new Promise((r) => { release2 = r; }));
  await new Promise((r) => setTimeout(r, 50));
  const out = { before, during: getComputedStyle(save).backgroundColor, disabled: rgb('var(--rk-disabled)'), ad: save.getAttribute('aria-disabled'),
    pendingKeeps: save.getAttribute('aria-disabled') === 'true' && getComputedStyle(save).backgroundColor === before && getComputedStyle(save).color !== rgb('var(--rk-disabled)'),
    danger: getComputedStyle(del).color === rgb('var(--rk-status-error-full)') && getComputedStyle(del).borderTopColor === rgb('var(--rk-status-error-full)') && getComputedStyle(del).backgroundColor === 'rgba(0, 0, 0, 0)',
    quiet: quiet.getAttribute('aria-disabled') === 'true' && getComputedStyle(quiet).borderTopColor === quietBefore[0] && getComputedStyle(quiet).color === quietBefore[1] };
  release(); release2(); await run; await run2; row.remove();
  return out;
});
ok(btns.pendingKeeps, 'a running button keeps its colours (it is not disabled)', JSON.stringify(btns));
ok(btns.danger, 'a destructive button: frame and words in the error colour, not filled', JSON.stringify(btns));
ok(btns.quiet, 'a quiet button keeps its look while it runs (no frame appears)', JSON.stringify(btns));
// A running button's line comes after busy-delay and goes with the label, in the same step: no busy-min (#110).
const lineGoes = await p.evaluate(async () => {
  const b = document.createElement('button'); b.className = 'rk-btn'; b.type = 'button'; b.textContent = 'save';
  document.querySelector('#forms').append(b);
  const line = () => { const a = getComputedStyle(b, '::after'); return a.content !== 'none' && a.visibility === 'visible'; };
  let release; const run = Rekta.pending(b, new Promise((r) => { release = r; }));
  const early = line();
  await new Promise((r) => setTimeout(r, Rekta.tokens.ms('--rk-duration-busy-delay') + 200));
  const shown = line(), during = b.textContent;
  release(); await run;
  const out = { early, shown, during, after: line(), label: b.textContent };
  b.remove();
  return out;
});
ok(!lineGoes.early && lineGoes.shown && lineGoes.during === 'save…', 'a running button: the line only after busy-delay', JSON.stringify(lineGoes));
ok(!lineGoes.after && lineGoes.label === 'save', 'and it goes with the label, in the same step', JSON.stringify(lineGoes));
// Every input disabled with a reason (aria-disabled) looks disabled, as a natively disabled one does (#108).
const dis = await p.evaluate(async () => {
  const rgb = (c) => { const e = document.createElement('i'); e.style.color = c; document.body.append(e); const v = getComputedStyle(e).color; e.remove(); return v; };
  const grey = rgb('var(--rk-disabled)'), cs = (el, pe) => getComputedStyle(el, pe);
  const box = document.createElement('div'); box.className = 'rk-actions'; box.style.cssText = 'position:fixed;left:0;top:0;z-index:9999;background:var(--rk-bg);padding:8px';
  box.innerHTML = '<label class="rk-switch"><input type="checkbox" checked aria-disabled="true"> sent</label>'
    + '<label class="rk-toggle"><span class="rk-toggle-label">feed</span><input type="checkbox" role="switch" checked aria-disabled="true"><span class="rk-toggle-track"></span><span class="rk-toggle-word" data-on="on" data-off="off"></span></label>'
    + '<input class="rk-input" aria-disabled="true" value="text" style="width:8em">'
    + '<select class="rk-input" aria-disabled="true" style="width:8em"><option>one</option></select>'
    + '<div class="rk-combo" style="width:10em"><input class="rk-input" aria-disabled="true"><button class="rk-combo-btn" type="button" tabindex="-1" aria-label="open"></button></div>'
    + '<div class="rk-stepper"><button type="button" data-rk-step="-1">−</button><input class="rk-input" type="number" value="3" aria-disabled="true"><button type="button" data-rk-step="1">+</button></div>'
    + '<div class="rk-slider" style="width:12em"><input class="rk-range" type="range" aria-disabled="true" value="50"><output>50</output></div>'
    + '<fieldset class="rk-segments"><label><input type="radio" name="dz" checked aria-disabled="true">one</label><label><input type="radio" name="dz" aria-disabled="true">two</label></fieldset>'
    + '<label class="rk-drop" style="width:10em"><input type="file" aria-disabled="true"><b>drop</b> files</label>';
  document.querySelector('.rk').append(box);
  const q = (s) => box.querySelector(s), st = q('.rk-stepper');
  // The slider's thumb has no computed style to read: its rule is looked up instead.
  const rules = [...document.styleSheets].flatMap((sh) => { try { return [...sh.cssRules]; } catch { return []; } });
  const stepped = Rekta.stepper(st); st.querySelector('[data-rk-step="1"]').click();
  const files = []; const zone = Rekta.drop(q('.rk-drop'), { onFiles: (f) => files.push(...f) });
  const dt = new DataTransfer(); dt.items.add(new File(['x'], 'a.txt', { type: 'text/plain' }));
  q('.rk-drop').dispatchEvent(new DragEvent('drop', { dataTransfer: dt, bubbles: true, cancelable: true }));
  const out = {
    switch: cs(q('.rk-switch')).color === grey && cs(q('.rk-switch input')).borderTopColor === grey && cs(q('.rk-switch input')).backgroundColor === grey,
    toggle: cs(q('.rk-toggle-track')).borderTopColor === grey && cs(q('.rk-toggle-track'), '::after').backgroundColor === grey && cs(q('.rk-toggle-label')).color === grey && cs(q('.rk-toggle-word')).color === grey && cs(q('.rk-toggle-track')).backgroundImage.includes(grey),
    field: cs(q('input.rk-input')).color === grey && cs(q('input.rk-input')).borderTopColor === grey,
    select: cs(q('select.rk-input')).color === grey && cs(q('select.rk-input')).borderTopColor === grey,
    combo: cs(q('.rk-combo-btn')).color === grey,
    stepper: [...st.querySelectorAll('[data-rk-step]')].every((b) => cs(b).color === grey && cs(b).borderTopColor === grey) && st.querySelector('input').value === '3',
    slider: cs(q('.rk-slider output')).color === grey && cs(q('.rk-range')).cursor === 'not-allowed'
      && rules.some((r) => /aria-disabled/.test(r.selectorText) && /slider-thumb/.test(r.selectorText) && /--rk-disabled/.test(r.style.background)),
    segments: [...box.querySelectorAll('.rk-segments > label')].every((l) => cs(l).borderTopColor === grey) && cs(q('.rk-segments > label')).backgroundColor === grey && cs(q('.rk-segments > label + label')).color === grey,
    drop: cs(q('.rk-drop')).borderTopColor === grey && cs(q('.rk-drop b')).color === grey && files.length === 0,
  };
  stepped.destroy(); zone.destroy(); box.remove();
  return out;
});
for (const [k, v] of Object.entries(dis)) ok(v, `${k} with aria-disabled looks disabled${k === 'stepper' || k === 'drop' ? ' and does not act' : ''}`, JSON.stringify(dis));
// The chosen segment lies above its neighbours: its accent frame shows at both joints (#109).
const joints = await p.evaluate(() => {
  const box = document.createElement('div'); box.style.cssText = 'position:fixed;left:0;top:0;z-index:9999;background:var(--rk-bg);padding:16px';
  box.innerHTML = '<fieldset class="rk-segments"><label><input type="radio" name="jz">one</label><label><input type="radio" name="jz" checked>two</label><label><input type="radio" name="jz">three</label></fieldset>';
  document.querySelector('.rk').append(box);
  const [a, b] = box.querySelectorAll('label'), r = b.getBoundingClientRect(), y = r.top + r.height / 2;
  const at = (x) => { const e = document.elementFromPoint(x, y); return e && e.closest('label'); };
  const out = { left: at(r.left + 1) === b, right: at(r.right - 1) === b, overlap: Math.round(a.getBoundingClientRect().right - r.left) };
  box.remove();
  return out;
});
ok(joints.left && joints.right && joints.overlap === 2, 'the chosen segment\'s frame lies on top at both joints', JSON.stringify(joints));
// A select's chevron is the combobox's, stroked in the text colour (#135); the caret and selected text (#136).
const writing = await p.evaluate(() => {
  const rgb = (c) => { const e = document.createElement('i'); e.style.color = c; document.body.append(e); const v = getComputedStyle(e).color; e.remove(); return v; };
  const box = document.createElement('div');
  box.innerHTML = '<p>words</p><select class="rk-input"><option>daily</option></select><div class="rk-combo"><input class="rk-input" value="Berlin"><button class="rk-combo-btn" type="button" aria-label="open"></button></div>'
    + '<ul class="rk-rows" data-rk-select><li aria-selected="true"><span>picked</span></li></ul>';
  document.querySelector('.rk').append(box);
  const sel = box.querySelector('select'), input = box.querySelector('input'), ink = rgb('var(--rk-text-primary)');
  const chevron = getComputedStyle(sel).backgroundImage, text = getComputedStyle(box.querySelector('p'), '::selection'), row = getComputedStyle(box.querySelector('li span'), '::selection');
  const out = {
    stroked: (chevron.match(/linear-gradient/g) || []).length === 2 && chevron.split(ink).length === 5 && getComputedStyle(sel).backgroundSize === '4px 6px, 4px 6px',
    same: chevron === getComputedStyle(box.querySelector('.rk-combo-btn'), '::before').backgroundImage,
    selection: text.backgroundColor === rgb('var(--rk-accent)') && text.color === rgb('var(--rk-on-accent)'),
    onAccent: row.backgroundColor === rgb('var(--rk-on-accent)') && row.color === rgb('var(--rk-accent)'),
    caret: getComputedStyle(input).caretColor === getComputedStyle(input).color,
  };
  box.remove();
  return out;
});
ok(writing.stroked && writing.same, 'a select\'s chevron is the combobox\'s: two strokes in the text colour', JSON.stringify(writing));
ok(writing.selection && writing.onAccent, 'selected text takes the accent with on-accent text, turned over on an accent fill', JSON.stringify(writing));
ok(writing.caret, 'the caret is the text colour', JSON.stringify(writing));
// A form in a sheet: a close request asks while there is input; Escape while asking keeps editing; an untouched one closes.
await p.click('#view-open'); await p.waitForTimeout(400);
const closeBtn = await p.evaluate(() => { const r = document.querySelector('#view-sheet .rk-sheet-close').getBoundingClientRect(); return { w: Math.round(r.width), h: Math.round(r.height), label: document.querySelector('#view-sheet .rk-sheet-close').getAttribute('aria-label') }; });
ok(closeBtn.w === closeBtn.h && closeBtn.w >= 32 && closeBtn.label === 'close', 'the sheet has a visible way out: a square icon button with a name', JSON.stringify(closeBtn));
await p.keyboard.press('Escape');
ok(await until(p, () => p.evaluate(() => !document.querySelector('#view-sheet').open), 3000), 'an untouched form in a sheet just closes');
await p.click('#view-open'); await p.waitForTimeout(400);
await p.fill('#view-name', 'release review'); await p.keyboard.press('Escape'); await p.waitForTimeout(200);
const asked = await p.evaluate(() => { const d = document.querySelector('#view-sheet'), f = document.activeElement; return { open: d.open, asking: d.hasAttribute('data-rk-asking'), focus: f.textContent, field: document.querySelector('#view-name').getClientRects().length }; });
ok(asked.open && asked.asking && asked.focus === 'keep editing' && asked.field === 0, 'with input, Escape asks in the sheet, "keep editing" first', JSON.stringify(asked));
await p.keyboard.press('Escape'); await p.waitForTimeout(200);
ok(await p.evaluate(() => { const d = document.querySelector('#view-sheet'); return d.open && !d.hasAttribute('data-rk-asking') && document.activeElement.id === 'view-name' && document.activeElement.value === 'release review'; }), 'Escape while it asks keeps editing, focus back in the field');
await p.keyboard.press('Escape'); await p.waitForTimeout(200);
const third = await p.evaluate(() => { const d = document.querySelector('#view-sheet'); return { open: d.open, asking: d.hasAttribute('data-rk-asking'), value: document.querySelector('#view-name').value }; });
await p.keyboard.press('Escape'); await p.waitForTimeout(200);
ok(third.open && third.asking && third.value === 'release review' && await p.evaluate(() => document.querySelector('#view-sheet').open), 'a third and fourth Escape ask and keep editing; the sheet never closes without asking', JSON.stringify(third));
await p.click('#view-sheet .rk-sheet-close'); await p.waitForTimeout(200);
ok((await p.getAttribute('#view-sheet .rk-sheet-ask .rk-btn:not(.rk-btn--primary)', 'data-status')) === 'error', '"discard changes" is the destructive button');
await p.click('#view-sheet .rk-sheet-ask .rk-btn:not(.rk-btn--primary)');
ok(await until(p, () => p.evaluate(() => !document.querySelector('#view-sheet').open), 3000), 'the close button asks too, and discarding closes');
await p.fill('#a-name', 'sea-2');
await go(p, 'overview');
await p.click('#tile-notes'); await p.waitForTimeout(400);
ok((await sheetTitle(p)) === 'Leave without saving?', 'leaving with changes asks', await sheetTitle(p));
await p.keyboard.press('Escape'); await p.waitForTimeout(400);
ok(await visible(p, 'home') && (await p.inputValue('#a-name')) === 'sea-2', 'Escape keeps editing');
await p.click('#tile-notes'); await p.waitForTimeout(400); await p.click('dialog[open] button[value=ok]');
ok(await until(p, () => visible(p, 'notes'), TURN * 2) && (await p.inputValue('#a-name')) === 'sea', 'discard leaves and puts the form back', await p.inputValue('#a-name'));
await p.keyboard.press('Escape'); await p.waitForTimeout(TURN);
await go(p, 'feedback');
await p.check('#s-readonly'); await p.waitForTimeout(300);
await go(p, 'forms');
await p.click('#a-save'); await p.waitForTimeout(1200);
ok((await text(p, '.rk-errors > p')).startsWith('Could not apply: maintenance'), 'the server says no to the whole form', await text(p, '.rk-errors > p'));
await go(p, 'feedback'); await p.uncheck('#s-readonly'); await p.waitForTimeout(600);

// Inputs: build a tile with a select, a stepper, segments and a toggle; a combobox; the text size; a date; files.
await go(p, 'inputs');
await p.selectOption('#b-status', 'error'); await p.waitForTimeout(100);
ok((await p.getAttribute('#built', 'data-status')) === 'error' && (await text(p, '#built .rk-tile-word')) === 'failed', 'a select sets the tile');
await p.click('.rk-stepper [data-rk-step="1"]'); await p.waitForTimeout(100);
ok((await p.inputValue('#b-value')) === '13' && (await text(p, '#built .rk-stat b')) === '13', 'the stepper steps');
await p.fill('#b-value', '999'); await p.dispatchEvent('#b-value', 'input');
ok((await p.getAttribute('.rk-stepper [data-rk-step="1"]', 'aria-disabled')) === 'true', 'the stepper says when its limit is reached');
await p.click('#builder .rk-segments label:nth-child(3)'); await p.waitForTimeout(100);
ok(await p.evaluate(() => document.querySelector('#built').matches('.rk-w2.rk-h2')), 'segments choose the size');
// Toggle buttons: pressed in the accent like a chosen segment, not pressed a quiet button; icon beside the word, the
// state word below; a press switches at once (#140).
const quick = () => p.evaluate(() => {
  const rgb = (c) => { const e = document.createElement('i'); e.style.color = c; document.body.append(e); const v = getComputedStyle(e).color; e.remove(); return v; };
  const [on, off] = document.querySelectorAll('#quick .rk-btn'), word = on.querySelector('span'), state = on.querySelector('.rk-btn-state');
  const icon = on.querySelector('svg').getBoundingClientRect(), w = word.getBoundingClientRect(), st = state.getBoundingClientRect();
  return {
    pressed: on.getAttribute('aria-pressed'), state: getComputedStyle(state, '::before').content,
    on: getComputedStyle(on).backgroundColor === rgb('var(--rk-accent)') && getComputedStyle(on).color === rgb('var(--rk-on-accent)') && getComputedStyle(on).borderTopColor === rgb('var(--rk-accent)'),
    off: getComputedStyle(off).borderTopColor === 'rgba(0, 0, 0, 0)' && getComputedStyle(off).color === rgb('var(--rk-text-contrast-high)') && getComputedStyle(off).backgroundColor === 'rgba(0, 0, 0, 0)',
    beside: icon.right <= w.left, below: st.top >= w.top + 8 && st.left === w.left,
  };
});
const q0 = await quick();
ok(q0.on && q0.off, 'a toggle button: pressed takes the accent, not pressed is quiet', JSON.stringify(q0));
ok(q0.beside && q0.below && q0.state === '"on"', 'its icon beside the word, the state word below', JSON.stringify(q0));
await p.click('#quick .rk-btn'); await p.waitForTimeout(100);
const q1 = await quick();
ok(q1.pressed === 'false' && q1.state === '"off"', 'a press switches it at once; the state word follows', JSON.stringify(q1));
await p.click('#quick .rk-btn'); await p.waitForTimeout(100);
await p.click('#builder .rk-toggle'); await p.waitForTimeout(100);
ok((await p.getAttribute('#b-spark', 'role')) === 'switch' && !!(await p.$('#built .rk-spark')), 'a toggle is a switch; it adds a sparkline at once');
await p.selectOption('#b-status', 'success'); await p.waitForTimeout(100);
ok(!(await p.$('#built .rk-spark')), 'a done tile drops its sparkline: stillness means done');
await p.selectOption('#b-status', 'info'); await p.waitForTimeout(100);
ok((await p.evaluate(() => getComputedStyle(document.querySelector('#builder .rk-toggle-word'), '::before').content)) === '"on"', 'the toggle says its state in a word');
await p.click('#find-token'); await p.keyboard.type('focus'); await p.waitForTimeout(150);
ok((await p.getAttribute('#find-token', 'aria-expanded')) === 'true' && (await p.evaluate(() => document.querySelector(`#${document.querySelector('#find-token').getAttribute('aria-activedescendant')}`).textContent)).startsWith('--rk-focus'), 'the combobox filters as you type');
await p.keyboard.press('Enter'); await p.waitForTimeout(100);
ok((await p.inputValue('#find-token')) === '--rk-focus' && (await text(p, '#find-hint')).startsWith('#1a44ea'), 'Enter chooses; the hint shows the value', await text(p, '#find-hint'));
await p.fill('#find-token', 'nonsense'); await p.keyboard.press('Tab'); await p.waitForTimeout(100);
ok((await p.inputValue('#find-token')) === '--rk-focus', 'leaving with other text puts the choice back');
await p.focus('#find-token'); await p.keyboard.press('Escape'); await p.keyboard.press('Escape'); await p.waitForTimeout(100);
ok((await p.inputValue('#find-token')) === '' && await visible(p, 'home'), 'Escape closes, then clears, and never goes back');
await p.focus('#text-size'); await p.keyboard.press('ArrowRight'); await p.waitForTimeout(100);
ok((await p.evaluate(() => document.documentElement.style.fontSize)) === '112.5%' && (await text(p, '.rk-slider output')) === '112.5%', 'the slider sets the text size');
await p.keyboard.press('ArrowLeft'); await p.waitForTimeout(100);
ok(/^In 3 days, at 09:00\.$/.test(await text(p, '#when-hint')), 'a date in the browser’s own picker', await text(p, '#when-hint'));
await p.setInputFiles('#drop input', [{ name: 'notes.txt', mimeType: 'text/plain', buffer: Buffer.from('hello') }, { name: 'huge.bin', mimeType: 'application/octet-stream', buffer: Buffer.alloc(5 * 1024 * 1024) }]);
await p.waitForTimeout(400);
ok((await p.evaluate(() => [...document.querySelectorAll('#files li .rk-tag')].map((t) => t.textContent).sort().join())) === 'active,failed' || (await p.evaluate(() => [...document.querySelectorAll('#files li .rk-tag')].map((t) => t.textContent).sort().join())) === 'done,failed', 'files: one uploads, one is too large');
await p.waitForTimeout(1500);
ok((await p.evaluate(() => [...document.querySelectorAll('#files li .rk-tag')].map((t) => t.textContent).sort().join())) === 'done,failed', 'and the upload finishes');
await p.screenshot({ path: join(shots, 'inputs.png') });

// Data: charts in ink and grey, a tooltip at each point, a table; meters; progress; the dots.
await go(p, 'data');
ok(await p.evaluate(() => document.querySelectorAll('#chart-line .rk-line').length === 2 && document.querySelectorAll('#chart-bars .rk-bar').length === 7), 'a line chart with two series, a bar chart with seven bars');
ok(await p.evaluate(() => { const s = getComputedStyle(document.querySelector('#chart-line .rk-line.rk-s0')).stroke, probe = document.createElement('i'); probe.style.color = 'var(--rk-text-primary)'; document.body.appendChild(probe); const c = getComputedStyle(probe).color; probe.remove(); return s === c; }), 'the first series is ink');
ok(!(await p.evaluate(() => document.querySelector('#chart-line .rk-legend').hidden)) && (await p.evaluate(() => document.querySelector('#chart-bars .rk-legend').hidden)), 'two series get a legend, one does not');
const lb = await (await p.$('#chart-line svg')).boundingBox();
await p.mouse.move(lb.x + lb.width * 0.5, lb.y + lb.height * 0.5); await p.waitForTimeout(200);
ok(!!(await p.$('#chart-line .rk-chart-cross')) && (await p.evaluate(() => document.querySelector('.rk-tip').matches(':popover-open') && / · these two weeks \d+ · the two before \d+$/.test(document.querySelector('.rk-tip').textContent))), 'the pointer shows a crosshair and every value at that point', await p.evaluate(() => document.querySelector('.rk-tip').textContent));
await p.mouse.move(5, 5); await p.waitForTimeout(100);
await p.focus('#chart-bars svg'); await p.keyboard.press('Home'); await p.waitForTimeout(150);
ok((await p.evaluate(() => document.querySelector('#chart-bars .rk-sr').textContent)) === 'Mon · 412 min' && (await p.evaluate(() => document.querySelectorAll('#chart-bars [data-rk-hot]').length)) === 1, 'Home on a focused chart reads the first value, to screen readers too', await p.evaluate(() => document.querySelector('#chart-bars .rk-sr').textContent));
await p.keyboard.press('ArrowRight'); await p.waitForTimeout(100);
ok((await p.evaluate(() => document.querySelector('#chart-bars .rk-sr').textContent)) === 'Tue · 468 min', '→ reads the next');
await p.click('.chart-head .rk-segments label:nth-child(3)'); await p.waitForTimeout(300);
ok((await p.evaluate(() => document.querySelectorAll('#chart-bars .rk-bar').length)) === 6 && (await p.evaluate(() => document.querySelector('#chart-bars .rk-chart-value').textContent)) === '12.6K min', 'segments redraw the bars; the peak is labelled, compactly');
await p.click('#chart-line .rk-chart-table summary'); await p.waitForTimeout(100);
ok((await p.evaluate(() => document.querySelectorAll('#chart-line .rk-chart-table tbody tr').length)) === 14, 'every chart shows its numbers as a table');
ok((await p.evaluate(() => getComputedStyle(document.querySelector('.rk-meter[data-status=error]'), '::before').width)) === (await p.evaluate(() => getComputedStyle(document.querySelector('.rk-meter[data-status=error]')).width)), 'a full meter is full');
ok(await p.evaluate(() => { const m = document.querySelector('.rk-meter[data-status=warning]'), f = getComputedStyle(m, '::before').backgroundColor, probe = document.createElement('i'); probe.style.color = 'var(--rk-status-warning-full)'; document.body.appendChild(probe); const c = getComputedStyle(probe).color; probe.remove(); return f === c; }), 'a meter takes a status colour only when its value means one');
await p.click('#signin'); await p.waitForTimeout(200);
ok(!(await p.evaluate(() => document.querySelector('#dots').hidden)) && (await p.evaluate(() => getComputedStyle(document.querySelector('#dots i')).animationName)) === 'rk-dots', 'the dots fly while a sign-in blocks');
await p.waitForTimeout(2700);
ok((await p.evaluate(() => document.querySelector('#dots').hidden)) && (await toast(p)) === 'Signed in.', 'and stop when it is done');
// The busy line, the dots and the beads move by their tokens (motion.busy, motion.dots, motion.beads).
const waits = await p.evaluate(() => {
  const box = document.createElement('div'); box.style.cssText = 'position:fixed;left:0;top:0;width:500px;z-index:-1';
  box.innerHTML = '<progress class="rk-progress"></progress><div class="rk-dots"><i></i><i></i><i></i><i></i><i></i></div><div class="rk-dots rk-dots--beads"><i></i><i></i><i></i></div>';
  document.body.append(box);
  const [bar, dots, beads] = box.children, s = getComputedStyle(bar), d = getComputedStyle(dots.children[1]), b = getComputedStyle(beads.children[2]);
  const out = { size: s.backgroundSize.split(',')[0], from: bar.getAnimations()[0] && bar.getAnimations()[0].effect.getKeyframes()[0].backgroundPosition, dots: `${d.animationDelay} ${d.animationTimingFunction}`, beads: `${b.animationName} ${b.animationDelay} ${b.animationTimingFunction} ${b.width}`, count: beads.children.length };
  box.remove();
  return out;
});
ok(/^calc\(30%\)$|^30%/.test(waits.size) && waits.dots === '0.168s cubic-bezier(0.25, 0.1, 0.25, 1)' && waits.beads === 'rk-beads 0.96s cubic-bezier(0.42, 0, 0.58, 1) 6px', 'the busy segment, the dots and the beads take their tokens', JSON.stringify(waits));

// Look: prefs, density, motion, the accent slot.
await go(p, 'look');
await p.check('input[name=theme][value=dark]'); await p.check('input[name=accent][value=teal]'); await p.waitForTimeout(300);
ok((await p.evaluate(() => document.documentElement.dataset.theme)) === 'dark' && (await p.evaluate(() => getComputedStyle(document.documentElement).getPropertyValue('--rk-accent').trim())) === '#00aba9', 'theme dark, the named accent teal in dark', await p.evaluate(() => getComputedStyle(document.documentElement).getPropertyValue('--rk-accent').trim()));
await p.screenshot({ path: join(shots, 'dark-teal.png') });
// Named accents (#151): each pair as tokens/rekta.accents.json has it, in each theme and contrast set, over a
// product's own accent; product goes back to it.
const named = await p.evaluate(async () => {
  const root = document.documentElement, { accents } = JSON.parse(document.querySelector('#themes-data').textContent), out = { bad: [] };
  const was = { theme: root.dataset.theme, contrast: root.dataset.contrast, accent: root.dataset.accent };
  const own = document.createElement('style'); own.textContent = ':root { --rk-accent: #7a2a8c; --rk-on-accent: #ffffff; }'; document.head.append(own);
  const read = (n) => getComputedStyle(root).getPropertyValue(`--rk-${n}`).trim();
  for (const [name, a] of Object.entries(accents)) for (const theme of ['light', 'dark', 'light-contrast', 'dark-contrast']) {
    root.dataset.theme = theme.startsWith('dark') ? 'dark' : 'light';
    if (theme.endsWith('contrast')) root.dataset.contrast = 'more'; else delete root.dataset.contrast;
    Rekta.prefs.set('accent', name, { remember: false });
    if (read('accent') !== a[theme].accent || read('on-accent') !== a[theme]['on-accent']) out.bad.push(`${name} ${theme}: ${read('accent')} ${read('on-accent')}`);
  }
  root.dataset.theme = 'light'; delete root.dataset.contrast;
  Rekta.prefs.set('accent', 'product', { remember: false });
  out.product = read('accent'); out.attr = root.hasAttribute('data-accent');
  own.remove();
  Object.entries(was).forEach(([k, v]) => { if (v === undefined) delete root.dataset[k]; else root.dataset[k] = v; });
  return out;
});
ok(!named.bad.length && named.product === '#7a2a8c' && !named.attr, 'a named accent sets its pair in every theme and contrast set, over the product\'s own; product goes back to it', JSON.stringify(named));
// Content fills (#157): an opted-in set without a status takes them, ink and secondary text at AA on each; a status
// tile in the set, the switch off, or more contrast make the whole set plain.
const fills = await p.evaluate(() => {
  const root = document.documentElement, was = root.dataset.contrast;
  const lum = (c) => { const [r, g, b] = c.match(/[\d.]+/g).slice(0, 3).map((v) => { v /= 255; return v <= 0.04045 ? v / 12.92 : ((v + 0.055) / 1.055) ** 2.4; }); return 0.2126 * r + 0.7152 * g + 0.0722 * b; };
  const ratio = (a, b) => { const [x, y] = [lum(a), lum(b)].sort((m, n) => n - m); return (x + 0.05) / (y + 0.05); };
  const rgb = (c) => { const e = document.createElement('i'); e.style.color = c; document.body.append(e); const v = getComputedStyle(e).color; e.remove(); return v; };
  const names = ['teal', 'steel', 'violet', 'plum', 'rose'];
  const set = document.createElement('div'); set.className = 'rk-tiles'; set.dataset.rkFills = 'content';
  set.innerHTML = names.map((n) => `<div class="rk-tile" data-rk-fill="${n}"><span class="rk-stat"><b>1</b><span>app</span></span><span class="rk-meta">m</span></div>`).join('') + '<div class="rk-tile"><b>plain</b></div>';
  document.querySelector('#colour').append(set);
  const tiles = [...set.children], bg = (t) => getComputedStyle(t).backgroundColor, surface = rgb('var(--rk-surface)');
  tiles.forEach((t) => { t.style.transition = 'none'; });   /* the fill as it is, not on its way */
  const out = {
    on: names.every((n, i) => bg(tiles[i]) === rgb(`var(--rk-content-${n})`) && bg(tiles[i]) !== surface) && bg(tiles[5]) === surface,
    low: Math.min(...tiles.slice(0, 5).flatMap((t) => [...t.querySelectorAll('b, .rk-stat span, .rk-meta')].map((e) => ratio(getComputedStyle(e).color, bg(t))))),
  };
  set.dataset.rkFills = 'plain'; out.plain = tiles.every((t) => bg(t) === surface);
  set.dataset.rkFills = 'content'; tiles[5].dataset.status = 'error';
  out.withStatus = tiles.slice(0, 5).every((t) => bg(t) === surface) && bg(tiles[5]) === rgb('var(--rk-status-error-tile)');
  delete tiles[5].dataset.status; root.dataset.contrast = 'more';
  out.contrast = tiles.slice(0, 5).every((t) => bg(t) === rgb('var(--rk-surface)'));
  if (was === undefined) delete root.dataset.contrast; else root.dataset.contrast = was;
  set.remove();
  return out;
});
ok(fills.on && fills.low >= 4.5, 'content fills: an opted-in set without a status takes them, every text on them at AA', JSON.stringify(fills));
ok(fills.plain && fills.withStatus && fills.contrast, 'one switch, a status tile in the set, or more contrast: the whole set is plain', JSON.stringify(fills));
ok(await p.evaluate(() => { const s = document.querySelector('#launcher'); return s.dataset.rkFills === 'content' && getComputedStyle(s.firstElementChild).backgroundColor !== getComputedStyle(s.lastElementChild).backgroundColor; }), 'the showcase\'s launcher shows content fills');
// An accent surface (#154): the accent with on-accent words; secondary text, a primary button turned over and a
// focus ring all read on it, in light and under more contrast with a named accent.
const surf = await p.evaluate(() => {
  const root = document.documentElement, was = { contrast: root.dataset.contrast, accent: root.dataset.accent };
  const lum = (c) => { const [r, g, b] = c.match(/[\d.]+/g).slice(0, 3).map((v) => { v /= 255; return v <= 0.04045 ? v / 12.92 : ((v + 0.055) / 1.055) ** 2.4; }); return 0.2126 * r + 0.7152 * g + 0.0722 * b; };
  const ratio = (a, b) => { const [x, y] = [lum(a), lum(b)].sort((m, n) => n - m); return (x + 0.05) / (y + 0.05); };
  const rgb = (c) => { const e = document.createElement('i'); e.style.color = c; document.body.append(e); const v = getComputedStyle(e).color; e.remove(); return v; };
  const s = document.createElement('header'); s.className = 'rk-accent-surface';
  s.innerHTML = '<h2 class="rk-title">welcome</h2><p class="rk-meta">since 9:00</p><p class="rk-facts">3 new</p><button class="rk-btn rk-btn--primary" type="button">start</button><button class="rk-btn" type="button">later</button>';
  document.querySelector('#colour').append(s);
  s.querySelectorAll('.rk-btn').forEach((b) => { b.style.transition = 'none'; });
  const measure = () => {
    const fill = getComputedStyle(s).backgroundColor, [, meta, facts, primary, plain] = s.children, ps = getComputedStyle(primary);
    return {
      fill: fill === rgb('var(--rk-accent)'), words: getComputedStyle(s).color === rgb('var(--rk-on-accent)'),
      low: Math.min(...[s.firstElementChild, meta, facts, plain].map((e) => ratio(getComputedStyle(e).color, fill))),
      turned: ps.backgroundColor === rgb('var(--rk-on-accent)') && ps.color === rgb('var(--rk-accent)'),
    };
  };
  const out = { now: measure() };
  root.dataset.accent = 'magenta'; root.dataset.contrast = 'more'; out.more = measure();
  Object.entries(was).forEach(([k, v]) => { if (v === undefined) delete root.dataset[k]; else root.dataset[k] = v; });
  s.remove();
  return out;
});
ok(surf.now.fill && surf.now.words && surf.now.turned && surf.now.low >= 4.5 && surf.more.turned && surf.more.low >= 7, 'an accent surface: on-accent words at 4.5:1 (7:1 under more contrast), a primary button turned over', JSON.stringify(surf));
const btnH = () => p.evaluate(() => Math.round(document.querySelector('#reset').getBoundingClientRect().height));
ok((await btnH()) === 32, 'fine pointer: compact controls', await btnH());
await p.check('input[name=density][value=comfortable]'); await p.waitForTimeout(200);
ok((await btnH()) === 44, 'comfortable: 44px controls', await btnH());
const fade = await p.evaluate(async () => {           /* Rekta's view-transition timing only while its own runs */
  const root = document.documentElement, done = Rekta.crossfade(() => {}), during = root.hasAttribute('data-rk-crossfade');
  await done;
  return { during, after: root.hasAttribute('data-rk-crossfade'), vt: typeof document.startViewTransition === 'function' };
});
ok(!fade.vt || (fade.during && !fade.after), 'the root carries data-rk-crossfade only while Rekta cross-fades', JSON.stringify(fade));
await p.check('input[name=density][value=auto]'); await p.check('input[name=theme][value=system]'); await p.check('input[name=accent][value=product]');
ok((await p.evaluate(() => JSON.parse(localStorage.getItem('rekta-prefs')).theme)) === 'system', 'prefs remembered');

// Sheet: the shading cancels; reset.
await p.click('#reset'); await p.waitForTimeout(300);
ok((await p.evaluate(() => getComputedStyle(document.documentElement).overflow)) === 'hidden', 'the page behind a sheet does not scroll');
await p.mouse.click(700, 850);
ok(await until(p, () => p.evaluate(() => !document.querySelector('#sheet').open), 3000), 'a tap on the shading cancels');
await p.click('#reset'); await p.waitForTimeout(300); await p.click('#sheet button[value=reset]'); await p.waitForTimeout(600);
ok((await toast(p)).startsWith('Demo reset') && (await text(p, '#presses')) === '0', 'reset restores the demo');
// The answer is given as a sheet starts to leave: a tap on the shading while it leaves changes no answer, asks nothing,
// and only ends the leaving at once; focus comes back once it has gone. A long duration-short keeps it leaving while
// the pointer gets there, on a busy runner too.
await p.evaluate(() => {
  document.documentElement.style.setProperty('--rk-duration-short', '1500ms');
  const b = document.createElement('button'); b.id = 'fx-opener'; b.textContent = 'opener'; document.querySelector('.rk').append(b); b.focus();
  window.fxAnswer = 'none'; Rekta.confirm({ title: 'Archive 3 builds?' }).then((v) => { window.fxAnswer = v; });
  window.fxShadeIn = document.getAnimations().some((a) => a.effect && a.effect.pseudoElement === '::backdrop' && a.animationName === 'rk-shade-in');
});
await p.waitForTimeout(1700);
await p.click('dialog[open] button[value="ok"]');
const leavingNow = await p.evaluate(() => { const d = [...document.querySelectorAll('dialog')].find((x) => x.open); return { leaving: !!d && d.getAttribute('data-rk-phase') === 'leaving', focusIn: !!d && d.contains(document.activeElement) }; });
await p.mouse.click(700, 850); await p.waitForTimeout(100);
const shut = await p.evaluate(() => ({ answer: window.fxAnswer, open: [...document.querySelectorAll('dialog')].some((x) => x.open), focus: document.activeElement.id, shadeIn: window.fxShadeIn }));
ok(shut.shadeIn, 'the shading fades in with the sheet');
ok(leavingNow.leaving && leavingNow.focusIn && shut.answer === true && !shut.open, 'ok, then a tap on the shading while it leaves: still ok, gone at once', JSON.stringify({ ...leavingNow, ...shut }));
ok(shut.focus === 'fx-opener', 'focus comes back once the sheet has gone', shut.focus);
// With input to lose: a close request while it leaves asks nothing and keeps the first answer.
const dirtyLeave = await p.evaluate(async () => {
  const wait = (t) => new Promise((r) => setTimeout(r, t));
  const d = document.createElement('dialog'); d.className = 'rk-sheet';
  d.innerHTML = '<form method="dialog"><h2 class="rk-group">Edit</h2><input value="changed"><p class="rk-actions"><button class="rk-btn" value="cancel">cancel</button><button class="rk-btn" value="save">save</button></p></form>';
  document.querySelector('.rk').append(d);
  const s = Rekta.sheet(d, { dirty: () => true });
  d.showModal(); await wait(1700);
  d.querySelector('button[value="save"]').click(); await wait(50);
  const leaving = d.getAttribute('data-rk-phase') === 'leaving';
  d.querySelector('button[value="cancel"]').click(); await wait(50);
  const out = { leaving, asked: !!d.querySelector('.rk-sheet-ask'), open: d.open, value: d.returnValue };
  s.destroy(); d.remove(); document.getElementById('fx-opener').remove();
  document.documentElement.style.removeProperty('--rk-duration-short');
  return out;
});
ok(dirtyLeave.leaving && !dirtyLeave.asked && !dirtyLeave.open && dirtyLeave.value === 'save', 'a close request while a dirty sheet leaves asks nothing and keeps its answer', JSON.stringify(dirtyLeave));

// Tokens: a table in a split; the current token in the address; selection; filters in the address; the divider.
// (Places in the app bar, as a phone app has them: the router marks the place you are at.)
await p.evaluate(() => { const n = document.createElement('nav'); n.className = 'rk-ab-places'; n.id = 'probe-places'; n.innerHTML = `<a class="rk-ab" href="${location.pathname}" aria-current="page">home</a><a class="rk-ab" href="#tokens">tokens</a>`; document.querySelector('#bar').append(n); });
await p.evaluate(() => window.site.router.navigate('tokens')); await p.waitForTimeout(TURN);
ok(await p.evaluate(() => { const [h, t] = document.querySelectorAll('#probe-places a'); const r = t.getAttribute('aria-current') === 'page' && !h.hasAttribute('aria-current'); document.querySelector('#probe-places').remove(); return r; }), 'places in the app bar: the router marks the one you are at');
ok((await p.evaluate(() => document.querySelectorAll('#tok-body tr').length)) === count, 'every token has a row');
const hist = await p.evaluate(() => history.length);
await p.click('#tok-body tr[data-name="focus"] th a'); await p.waitForTimeout(500);
ok((await p.evaluate(() => location.hash)) === '#tokens/focus' && (await p.evaluate(() => history.length)) === hist, 'a click selects, shows and replaces the address', await p.evaluate(() => location.hash));
ok((await text(p, '#token-title')) === '--rk-focus' && (await p.getAttribute('#tok-body tr[data-name="focus"]', 'aria-selected')) === 'true', 'the detail shows beside the list');
ok((await p.title()).startsWith('--rk-focus · '), 'the title names the token', await p.title());
await p.screenshot({ path: join(shots, 'token.png') });
await p.click('#tok-body tr[data-name="bg"]', { modifiers: ['Control'] }); await p.waitForTimeout(200);
ok((await text(p, '.rk-ab-count')) === '2 selected', 'Ctrl+click in the tokens');
// Plural forms follow the locale's rules (Intl.PluralRules): Polish says 3 and 5 differently.
await p.evaluate(() => { Rekta.setLocale('pl'); Rekta.setStrings({ selected: { one: '1 zaznaczony', few: (n) => `${n} zaznaczone`, many: (n) => `${n} zaznaczonych`, other: (n) => `${n} zaznaczonego` } }); });
await p.click('#tok-body tr[data-name="surface"]', { modifiers: ['Control'] }); await p.waitForTimeout(100);
const pl3 = await text(p, '.rk-ab-count');
await p.click('#tok-body tr[data-name="accent"]', { modifiers: ['Control'] }); await p.click('#tok-body tr[data-name="text-primary"]', { modifiers: ['Control'] }); await p.waitForTimeout(100);
const pl5 = await text(p, '.rk-ab-count');
await p.evaluate(() => { Rekta.setStrings(null); Rekta.setLocale(''); });
ok(pl3 === '3 zaznaczone' && pl5 === '5 zaznaczonych', 'a count picks its plural form by the locale', `${pl3} / ${pl5}`);
await p.keyboard.press('Escape'); await p.waitForTimeout(200);
ok(await visible(p, 'tokens'), 'Escape ends the selection before it goes back');
await p.click('#tok-group'); await p.waitForTimeout(200); await p.click('#tok-group-menu [data-v=motion]'); await p.keyboard.press('Escape'); await p.waitForTimeout(200);
ok((await p.evaluate(() => location.hash)).includes('group=motion') && (await p.evaluate(() => [...document.querySelectorAll('#tok-body tr')].every((r) => r.hidden || r.dataset.group === 'motion'))), 'filters keep motion tokens, in the address', await p.evaluate(() => location.hash));
await p.click('#tok-clear'); await p.waitForTimeout(200);
const w0 = await p.evaluate(() => document.querySelector('#tokens-list').getBoundingClientRect().width);
await p.focus('#tokens-split .rk-split-handle'); await p.keyboard.press('ArrowRight'); await p.waitForTimeout(100);
ok(Math.round((await p.evaluate(() => document.querySelector('#tokens-list').getBoundingClientRect().width)) - w0) === 16, 'the divider moves with the keyboard');
await p.close();

// Deep links: the place, its list and the token underneath; filters in the address.
const d = await open({}, '#tokens/space-m');
ok((await text(d, '#token-title')) === '--rk-space-m' && (await d.evaluate(() => history.state.rk.index)) === 2, 'a deep link to a token puts home and the list underneath', await d.evaluate(() => JSON.stringify(history.state)));
await d.keyboard.press('Escape'); await d.waitForTimeout(600);
ok((await d.evaluate(() => location.hash)) === '#tokens', 'back from it shows the list', await d.evaluate(() => location.hash));
await d.close();
const d2 = await open({}, '#tokens?group=radius');
ok(await d2.evaluate(() => [...document.querySelectorAll('#tok-body tr')].filter((r) => !r.hidden).every((r) => r.dataset.group === 'radius')) && (await text(d2, '#tok-group')) === 'group: radius', 'a filtered address opens filtered', await text(d2, '#tok-group'));
await d2.close();

// Icons and the app shell: the sprite, the gallery, the title bar, installable, offline.
const sh = await open();
ok((await sh.evaluate(() => document.querySelectorAll('symbol[id^="rk-i-"]').length)) === 63 && (await sh.evaluate(() => document.querySelectorAll('.icon-gallery li').length)) === 63, 'sixty-three icons in the sprite and the gallery');
ok(await sh.evaluate(() => { const u = document.querySelector('.icon-gallery use'); const b = u.closest('svg').getBBox(); return b.width > 4 && b.height > 4; }), 'an icon draws from the sprite');
ok((await sh.evaluate(() => getComputedStyle(document.querySelector('#titlebar')).display)) === 'none', 'the title bar hides in a browser tab');
await sh.evaluate(() => document.documentElement.setAttribute('data-rk-titlebar', '')); await sh.waitForTimeout(100);
ok((await sh.evaluate(() => getComputedStyle(document.querySelector('#titlebar')).display)) === 'flex' && (await sh.evaluate(() => getComputedStyle(document.querySelector('#titlebar')).getPropertyValue('-webkit-app-region') || getComputedStyle(document.querySelector('#titlebar')).getPropertyValue('app-region'))) === 'drag', 'a native shell shows it: the window’s drag area');
await sh.evaluate(() => window.site.router.navigate('tokens')); await sh.waitForTimeout(TURN);
ok((await text(sh, '#titlebar .rk-titlebar-place')) === 'Every value, a token.' && !(await sh.evaluate(() => document.querySelector('#titlebar [data-rk-back]').hidden)), 'the title bar names the place and offers back');
await sh.evaluate(() => document.documentElement.removeAttribute('data-rk-titlebar'));
const man = await (await fetch(base + (await sh.getAttribute('link[rel=manifest]', 'href')))).json();
ok(man.name === 'Rekta Design' && man.display_override[0] === 'window-controls-overlay' && man.icons.some((i) => i.purpose === 'maskable'), 'a web app manifest with a window-controls overlay and a maskable icon');
ok(await sh.evaluate(() => navigator.serviceWorker.ready.then((r) => !!r.active)), 'the service worker is active');
await sh.waitForTimeout(500);
ok((await text(sh, '#pwa-offline')) === 'yes', 'the page says it works offline');
await sh.context().setOffline(true);
await sh.reload(); await sh.waitForTimeout(1500);
ok((await sh.title()).includes('Rekta Design') && (await sh.evaluate(() => document.querySelectorAll('#tok-body tr').length)) > 100, 'offline, a reload still opens the whole page');
await sh.evaluate(() => window.dispatchEvent(new Event('offline'))); await sh.waitForTimeout(500);
ok((await text(sh, '.rk-banner-word')) === 'offline', 'offline, the banner says so');
await sh.context().setOffline(false);
await sh.close();

// Phone: stacked panorama, sticky strip, toasts from the app bar, overflow, the split one pane at a time.
const m = await open({ viewport: { width: 390, height: 844 }, hasTouch: true, isMobile: true });
ok((await m.evaluate(() => getComputedStyle(document.querySelector('#pan')).display)) !== 'flex', 'phone: the panorama stacks');
await m.evaluate(() => scrollTo(0, 2400)); await m.waitForTimeout(300);
ok((await m.evaluate(() => Math.round(document.querySelector('#nav').getBoundingClientRect().top))) === 0, 'phone: the section strip sticks');
await m.tap('#nav a[href="#feedback"]'); await m.waitForTimeout(900);
ok((await m.evaluate(() => document.querySelector('#feedback').getBoundingClientRect().top)) >= (await m.evaluate(() => document.querySelector('#nav').getBoundingClientRect().bottom)) - 2, 'phone: a jump lands below the strip');
await m.tap('#f-event'); await m.waitForTimeout(400);
const tb = await m.evaluate(() => ({ toast: document.querySelector('.rk-toast:not([data-rk-phase])').getBoundingClientRect().bottom, bar: document.querySelector('#bar').getBoundingClientRect().top }));
ok(tb.toast <= tb.bar + 1, 'phone: a toast rises from the app bar', JSON.stringify(tb));
ok(await m.evaluate(() => { const b = document.querySelector('#bar'); return b.scrollWidth <= b.clientWidth; }), 'phone: the app bar fits');
await m.setViewportSize({ width: 300, height: 844 }); await m.waitForTimeout(300);
ok(await m.evaluate(() => document.querySelector('#ab-motion').hasAttribute('data-rk-overflow') && !document.querySelector('#ab-more').hidden), 'narrower: what does not fit moves into more');
await m.tap('#ab-more'); await m.waitForTimeout(300);
ok((await m.evaluate(() => [...document.querySelectorAll('.rk-ab-menu .rk-menu-item')].map((b) => b.textContent))).some((t) => t.includes('motion')), 'narrower: more repeats the moved controls');
await m.keyboard.press('Escape');
await m.setViewportSize({ width: 390, height: 844 }); await m.waitForTimeout(300);
// An app bar of its own at 320px: a pinned control stays, the menu keeps a key hint apart, the places step the strip aside.
const bar = await m.evaluate(async () => {
  const wait = (t) => new Promise((r) => setTimeout(r, t));
  const icon = '<svg viewBox="0 0 20 20" aria-hidden="true"><circle cx="9" cy="9" r="5.5"/></svg>';
  const el = document.createElement('div'); el.className = 'rk-appbar'; el.style.cssText = 'width:320px;left:0;right:auto';
  el.innerHTML = `<button class="rk-ab rk-ab-back" type="button">${icon}<span>back</span></button><span class="rk-ab-status"><i class="rk-pulse"></i><span class="rk-ab-k">live</span></span>`
    + `<button class="rk-ab" id="t-search" type="button" data-rk-priority="pinned">${icon}<span>search</span></button>`
    + `<button class="rk-ab" id="t-goto" type="button" aria-keyshortcuts="Control+K Meta+K">${icon}<span>go to</span><span class="rk-key rk-ab-k" aria-hidden="true">Ctrl+K</span></button>`
    + `<button class="rk-ab" type="button" data-rk-priority="5">${icon}<span>other</span></button><button class="rk-ab rk-ab-more" type="button" hidden>${icon}<span>more</span></button>`;
  // The place name as navigation.md gives it: a direct child of the place, right after the strip.
  const name = document.createElement('h1'); name.className = 'rk-place-name'; name.textContent = 'overview';
  const strip = document.querySelector('#nav'); strip.after(name);
  const titleLeft = strip.getBoundingClientRect().left + parseFloat(getComputedStyle(strip).paddingLeft);
  document.querySelector('.rk').append(el);
  const a = Rekta.appbar(el); await wait(100);
  const item = [...el.querySelectorAll('.rk-ab-menu .rk-menu-item')].find((b) => b._rkFor && b._rkFor.id === 't-goto');
  const r = {
    pinned: !el.querySelector('#t-search').hasAttribute('data-rk-overflow') && el.querySelector('#t-goto').hasAttribute('data-rk-overflow') && el.scrollWidth <= el.clientWidth + 1,
    words: item ? item.firstChild.textContent : '(none)', key: item ? (item.querySelector('.rk-menu-key') || {}).textContent : '', ks: item ? item.getAttribute('aria-keyshortcuts') : '',
    nameHidden: name.getBoundingClientRect().height <= 1,
  };
  const pl = document.createElement('nav'); pl.className = 'rk-ab-places';
  pl.innerHTML = `<a class="rk-ab" href="#" aria-current="page">${icon}<span>today</span></a><a class="rk-ab" href="#">${icon}<span>plan</span></a>`;
  el.querySelector('.rk-ab-status').after(pl); await wait(100);
  r.places = el.hasAttribute('data-rk-places') && document.documentElement.hasAttribute('data-rk-places') && getComputedStyle(document.querySelector('#nav')).display === 'none'
    && el.querySelector('#t-search').hasAttribute('data-rk-overflow');
  r.nameShown = name.getBoundingClientRect().height > 20;
  { const t = document.createRange(); t.selectNodeContents(name); r.flush = Math.abs(t.getBoundingClientRect().left - titleLeft); }
  a.destroy(); el.remove(); await wait(50);
  r.after = !document.documentElement.hasAttribute('data-rk-places') && getComputedStyle(document.querySelector('#nav')).display !== 'none';
  name.remove();
  return r;
});
ok(bar.pinned, 'a pinned control stays in the bar; one without a priority moves first');
ok(bar.words === 'go to' && bar.key === 'Ctrl+K' && bar.ks === 'Control+K Meta+K', 'a moved control\'s menu item keeps its key hint apart', JSON.stringify(bar));
ok(bar.places && bar.after, 'places in the bar: data-rk-places on the bar and the root, the strip steps aside, back when it goes');
ok(bar.nameHidden && bar.nameShown, '.rk-place-name shows only while the places are in the bar');
ok(bar.flush < 1, 'right after the strip, the place name stands where the strip\'s title stood', bar.flush);
ok(await m.evaluate(() => { const n = document.querySelectorAll('.rk-place-name'), s = n[0] && getComputedStyle(n[0]);
  return n.length === 1 && n[0].closest('.phone') && n[0].getBoundingClientRect().height > 20 && Math.abs(parseFloat(s.fontSize) - Rekta.tokens.px('--rk-font-size-pivot-active')) < 0.5; }), 'the showcase\'s phone shows a place without a title of its own, named by .rk-place-name');
const grow = await m.evaluate(async () => {
  const wait = (t) => new Promise((r) => setTimeout(r, t));
  const icon = '<svg viewBox="0 0 20 20" aria-hidden="true"><circle cx="9" cy="9" r="5.5"/></svg>';
  const el = document.createElement('div'); el.className = 'rk-appbar'; el.style.cssText = 'width:390px;left:0;right:auto';
  el.innerHTML = `<span class="rk-ab-status"><i class="rk-pulse"></i><span class="rk-ab-k" id="t-live">live</span></span><span class="rk-ab-mid" id="t-mid"><span>go to</span><kbd class="rk-key">g</kbd><span>then a place</span></span>`
    + `<button class="rk-ab" type="button">${icon}<span>pause</span></button><button class="rk-ab" type="button">${icon}<span>theme</span></button><button class="rk-ab" type="button">${icon}<span>motion</span></button><button class="rk-ab rk-ab-more" type="button" hidden>${icon}<span>more</span></button>`;
  document.querySelector('.rk').append(el);
  const a = Rekta.appbar(el); await wait(150);
  const r = { fitsFirst: el.scrollWidth <= el.clientWidth + 1 && el.querySelector('.rk-ab-more').hidden, row: getComputedStyle(el.querySelector('#t-mid')).display };
  el.querySelector('#t-live').textContent = 'offline · reconnecting since 14:02'; await wait(200);
  r.grown = !el.querySelector('.rk-ab-more').hidden && !!el.querySelector('[data-rk-overflow]') && el.scrollWidth <= el.clientWidth + 1;
  r.midGone = getComputedStyle(el.querySelector('#t-mid')).display === 'none';
  el.querySelector('#t-live').textContent = 'offline'; el.querySelector('#t-mid').setAttribute('data-rk-priority', 'pinned'); await wait(250);
  r.midPinned = !el.querySelector('.rk-ab-more').hidden && getComputedStyle(el.querySelector('#t-mid')).display !== 'none' && el.querySelector('#t-mid').getBoundingClientRect().width >= el.querySelector('#t-mid').scrollWidth - 1 && el.scrollWidth <= el.clientWidth + 1;
  a.destroy(); el.remove();
  return r;
});
ok(grow.fitsFirst && /flex/.test(grow.row), 'the middle lays its parts out in a row', JSON.stringify(grow));
ok(grow.grown && grow.midGone, 'a live word that grows moves a control into more, by itself', JSON.stringify(grow));
ok(grow.midPinned, 'a pinned middle (a key waiting for its second) stays while more shows', JSON.stringify(grow));
// On touch a tap on a disabled app-bar control shows its reason (#126); a tap on one that can act shows nothing.
await m.evaluate(() => {
  const el = document.createElement('div'); el.className = 'rk-appbar'; el.id = 't-bar'; el.style.cssText = 'width:390px;left:0;right:auto;bottom:120px';
  el.innerHTML = '<button class="rk-ab" id="t-print" type="button"><span>print</span></button><button class="rk-ab" id="t-ok" type="button" data-rk-tip="share it"><span>share</span></button>';
  document.querySelector('.rk').append(el);
  Rekta.disable(el.querySelector('#t-print'), 'needs a printer');
});
const tipNow = () => m.evaluate(() => { const t = document.querySelector('.rk-tip'); return t && t.matches(':popover-open') ? t.textContent : ''; });
await m.tap('#t-ok'); await m.waitForTimeout(150); const okTip = await tipNow();
await m.tap('#t-print', { force: true }); await m.waitForTimeout(150); const why = await tipNow();
ok(okTip === '' && why === 'needs a printer', 'touch: a tap shows a disabled control\'s reason, and no other tip', `${okTip} / ${why}`);
await m.evaluate(() => document.querySelector('#t-bar').remove());
await m.evaluate(() => scrollTo(0, 0)); await m.waitForTimeout(200);
await m.tap('#tile-notes'); await m.waitForTimeout(TURN);
ok(await visible(m, 'notes') && !(await m.evaluate(() => document.querySelector('#back').hidden)), 'phone: a tile opens the place, back shows');
await m.tap('#back'); await m.waitForTimeout(TURN);
ok(await visible(m, 'home'), 'phone: the app bar back returns');
ok((await m.evaluate(() => Math.round(document.querySelector('#reset').getBoundingClientRect().height))) === 44, 'touch: 44px controls');
await m.evaluate(() => window.site.router.navigate('tokens')); await m.waitForTimeout(TURN);
ok((await m.evaluate(() => getComputedStyle(document.querySelector('#tok-body tr')).display)) === 'flex', 'phone: table rows stack', await m.evaluate(() => getComputedStyle(document.querySelector('#tok-body tr')).display));
await m.tap('#tok-body tr[data-name="bg"] th a'); await m.waitForTimeout(TURN);
ok(await m.evaluate(() => document.querySelector('#tokens-list').hidden && !document.querySelector('#tokens-detail').hidden) && (await m.evaluate(() => document.activeElement.id)) === 'token-title', 'phone: the token replaces the list, focus on it', await focused(m));
await m.screenshot({ path: join(shots, 'phone-token.png') });
await m.goBack(); await m.waitForTimeout(TURN);
ok(await m.evaluate(() => !document.querySelector('#tokens-list').hidden), 'phone: back returns to the list', await focused(m));
await m.close();

// A sheet with classic scrollbars (Playwright hides them by default, so this browser shows them): it spans a phone,
// sits in the middle of a wide window, and nothing behind it shifts.
{
  const sb = await chromium.launch({ ignoreDefaultArgs: ['--hide-scrollbars'] });
  for (const w of [375, 1000]) {
    const n = await sb.newPage({ viewport: { width: w, height: 700 } });
    n.on('pageerror', (e) => errors.push(`${w}/scrollbars: ${e.message}`));
    await n.goto(base); await n.waitForTimeout(1200);
    await n.evaluate(() => { const d = document.createElement('div'); d.style.height = '3000px'; document.body.append(d); });
    const before = await n.evaluate(() => ({ gutter: innerWidth - document.documentElement.clientWidth, title: document.querySelector('#overview-title').getBoundingClientRect().right }));
    await n.evaluate(() => document.querySelector('#sheet').showModal()); await n.waitForTimeout(500);
    const r = await n.evaluate(() => { const b = document.querySelector('#sheet').getBoundingClientRect(); return { x: Math.round(b.x), w: Math.round(b.width), centre: Math.round(b.x + b.width / 2), vw: innerWidth, title: document.querySelector('#overview-title').getBoundingClientRect().right }; });
    const placed = w < 600 ? r.x === 0 && r.w === r.vw : Math.abs(r.centre - r.vw / 2) <= 1;
    ok(before.gutter > 0 && placed && Math.abs(r.title - before.title) < 1, `a sheet at ${w}px with classic scrollbars: ${w < 600 ? 'the whole width' : 'centred'}, nothing behind shifts`, JSON.stringify({ ...before, ...r }));
    await n.close();
  }
  await sb.close();
}

// Motion that follows a change of content: tiles re-flow, flip and peek; rows come and go; a sheet leaves.
const q = await open();
const anims = (sel) => q.evaluate((s) => [...document.querySelectorAll(s)].reduce((n, el) => n + el.getAnimations().length, 0), sel);
const setSize = (pg, v) => pg.evaluate((x) => { const r = document.querySelector(`input[name=b-size][value="${x}"]`); r.checked = true; r.dispatchEvent(new Event('change', { bubbles: true })); }, v);
// the change and the look in one step, so a slow runner cannot miss the 250ms slide
const resized = await q.evaluate(() => {
  const r = document.querySelector('input[name=b-size][value="h2"]'); r.checked = true; r.dispatchEvent(new Event('change', { bubbles: true }));
  const n = (s) => [...document.querySelectorAll(s)].reduce((k, el) => k + el.getAnimations().length, 0);
  return { tile: n('#built'), next: n('#built ~ .rk-tile'), slide: !!document.querySelector('#builder .rk-segments-slide') };
});
ok(resized.tile > 0 && resized.next > 0, 'a resized tile scales to its size and its neighbours glide', JSON.stringify(resized));
ok(resized.slide, 'the segments accent slides to the new choice');
await q.waitForTimeout(500);
ok(await anims('#built, #built ~ .rk-tile') === 0 && await q.evaluate(() => document.querySelector('#built').classList.contains('rk-h2') && !document.querySelector('.rk-segments-slide')), 'the re-flow settles');
await q.evaluate(() => { const v = document.querySelector('#b-value'); v.value = '13'; v.dispatchEvent(new Event('input', { bubbles: true })); });
ok(await q.evaluate(() => document.querySelectorAll('#built > .rk-stat').length === 2 && document.querySelector('#built > .rk-stat[aria-hidden] b').textContent === '12'), 'a new number peeks in: the old one leaves upwards');
await q.waitForTimeout(500);
ok(await q.evaluate(() => document.querySelectorAll('#built > .rk-stat').length === 1 && document.querySelector('#built .rk-stat b').textContent === '13'), 'the peek settles on the new number');
ok(await q.evaluate(() => { const v = document.querySelector('#b-status'); v.value = 'error'; v.dispatchEvent(new Event('change', { bubbles: true })); const t = document.querySelector('#built'); return t.getAnimations().length > 0 && t.dataset.status === 'info'; }), 'a new status turns the tile over first');
await q.waitForTimeout(500);
ok((await q.evaluate(() => document.querySelector('#built').dataset.status)) === 'error', 'and it comes back with the new status');
await q.evaluate(() => document.querySelector('#tile-pin').click()); await q.waitForTimeout(60);
ok(await q.evaluate(() => { const t = document.querySelector('#live-tiles [data-pinned]'); return !!t && t.previousElementSibling.id === 'tile-seen' && t.getAnimations().length > 0; }), 'a pinned tile builds in after the first one');
await q.waitForTimeout(500);
await q.evaluate(() => document.querySelector('#tile-unpin').click()); await q.waitForTimeout(600);
ok(await q.evaluate(() => !document.querySelector('#live-tiles [data-pinned]') && document.querySelector('#tile-seen').nextElementSibling.dataset.live === 'moves'), 'unpinned, it leaves and the tiles close up');
const filterComp = (pg, v) => pg.evaluate((x) => { const i = document.querySelector('#comp-q'); i.value = x; i.dispatchEvent(new Event('input', { bubbles: true })); }, v);
ok(await q.evaluate(() => { const i = document.querySelector('#comp-q'); i.value = 'spark'; i.dispatchEvent(new Event('input', { bubbles: true })); const rows = [...document.querySelectorAll('#comp-body tr')]; return rows.every((r) => !r.hidden) && rows.some((r) => r.getAnimations().length); }), 'filtered rows leave before they hide');
await q.waitForTimeout(700);
ok((await q.evaluate(() => document.querySelectorAll('#comp-body tr:not([hidden])').length)) === 1, 'then only the matching row stays', await q.evaluate(() => document.querySelectorAll('#comp-body tr:not([hidden])').length));
await filterComp(q, ''); await q.waitForTimeout(40);
ok(await q.evaluate(() => document.querySelectorAll('#comp-body tr:not([hidden])').length > 1 && [...document.querySelectorAll('#comp-body tr')].some((r) => r.getAnimations().length)), 'cleared, the rows come back and slide in');
await q.waitForTimeout(600);
await q.evaluate(() => document.querySelector('#press-sheet').showModal()); await q.waitForTimeout(300);
ok(await q.evaluate(() => { const d = document.querySelector('#press-sheet'); d.close(); return d.open && d.dataset.rkPhase === 'leaving'; }), 'a closing sheet leaves before it closes');
ok(await until(q, async () => !(await q.evaluate(() => document.querySelector('#press-sheet').open))), 'then it is closed');
const linePts = () => q.evaluate(() => document.querySelector('#chart-line path.rk-line').getAttribute('d'));
const l0 = await linePts();
await q.evaluate(() => { const r = document.querySelector('input[name=line-what][value=reviews]'); r.checked = true; r.dispatchEvent(new Event('change', { bubbles: true })); });
await q.waitForTimeout(200); const l1 = await linePts(); await q.waitForTimeout(700); const l2 = await linePts();
ok(l1 !== l0 && l1 !== l2 && (await q.evaluate(() => document.querySelectorAll('#chart-line path.rk-line:not([data-rk-draw])').length)) === 2, 'new figures on the same days: the lines bend to them, not drawn again');
await q.evaluate(() => window.site.files.expandAll(false)); await q.waitForTimeout(600);
ok(await q.evaluate(() => { window.site.files.expandAll(true); return [...document.querySelectorAll('#tree [role=group]')].some((u) => u.getAnimations().length); }), 'a tree branch opens to its height');
await q.close();

// Shell patterns (#89 to #98): a side panel from the trailing edge with a list chosen in place, a level indicator,
// entries removed by Delete, a button or a drag, a launch tile, the title bar's window buttons and inactive name, and
// running things in an app bar without a live state.
const sp = await open();
const panel = await sp.evaluate(async () => {
  const wait = (t) => new Promise((res) => setTimeout(res, t));
  const d = document.createElement('dialog'); d.className = 'rk-sheet rk-sheet--side'; d.id = 't-panel';
  d.innerHTML = '<form method="dialog"><button class="rk-btn rk-btn--icon rk-btn--quiet rk-sheet-close" value="cancel" aria-label="close">×</button>'
    + '<section class="rk-region" data-rk-group id="t-group"><p class="rk-kicker">network</p><button class="rk-btn" type="button" id="t-wifi" data-rk-in="t-nets" data-rk-in-name="wi-fi">wi-fi · home</button></section>'
    + '<section class="rk-region" id="t-nets" aria-label="wi-fi" hidden><button class="rk-btn" type="button" id="t-net1">home</button><button class="rk-btn" type="button">office</button></section></form>';
  document.querySelector('.rk').append(d);
  Rekta.sheet(d); window.tIn = Rekta.inPlace(d);
  d.showModal();
  const moving = d.getAnimations().length > 0;
  await wait(600);
  const r = d.getBoundingClientRect();
  return { moving, right: Math.round(r.right), vw: innerWidth, h: Math.round(r.height), vh: innerHeight, w: Math.round(r.width), want: Math.round(2 * Rekta.tokens.px('--rk-tile-unit') + Rekta.tokens.px('--rk-space-s') + 2 * Rekta.tokens.px('--rk-space-m')) };
});
ok(panel.moving && panel.right === panel.vw && panel.h === panel.vh && panel.w === panel.want, 'a side panel slides in at the trailing edge, full height, two tile units wide with its padding', JSON.stringify(panel));
await sp.click('#t-wifi'); await sp.waitForTimeout(400);
const chose = await sp.evaluate(() => ({ group: document.querySelector('#t-group').hidden, list: !document.querySelector('#t-nets').hidden, back: (document.querySelector('#t-nets > .rk-in-back') || {}).textContent, focus: document.activeElement.id, depth: window.tIn.depth }));
ok(chose.group && chose.list && chose.back === 'back · wi-fi' && chose.focus === 't-net1' && chose.depth === 1, 'choosing in place: the list takes the group\'s place under "back · name", focus in it', JSON.stringify(chose));
await sp.keyboard.press('Escape'); await sp.waitForTimeout(400);
const back1 = await sp.evaluate(() => ({ open: document.querySelector('#t-panel').open, group: !document.querySelector('#t-group').hidden, list: document.querySelector('#t-nets').hidden, focus: document.activeElement.id }));
ok(back1.open && back1.group && back1.list && back1.focus === 't-wifi', 'Escape goes back to the group first; the panel stays open', JSON.stringify(back1));
await sp.click('#t-wifi'); await sp.waitForTimeout(400); await sp.keyboard.press('ArrowLeft'); await sp.waitForTimeout(300);
ok(await sp.evaluate(() => !document.querySelector('#t-group').hidden && document.activeElement.id === 't-wifi'), '← goes back too');
const leaving = await sp.evaluate(async () => {
  const d = document.querySelector('#t-panel'); d.close();
  // What it does is read from its own leaving animation, not sampled at a moment a busy runner may not have reached.
  const own = d.getAnimations().find((a) => !(a instanceof CSSAnimation) && !a.effect.pseudoElement), frames = own ? own.effect.getKeyframes() : [];
  return { phase: d.dataset.rkPhase, to: frames.length ? frames.at(-1).transform : '', fades: frames.some((f) => 'opacity' in f), d: own ? own.effect.getTiming().duration : 0 };
});
ok(leaving.phase === 'leaving' && leaving.to === 'translateX(100%)' && !leaving.fades && leaving.d > 0, 'it leaves whole towards its trailing edge', JSON.stringify(leaving));
ok(await until(sp, async () => !(await sp.evaluate(() => document.querySelector('#t-panel').open))), 'then it is closed');
await sp.evaluate(() => document.querySelector('#t-panel').remove());
// The level indicator: one toast, again with the same word it stays and its value changes; it goes timing.level after.
await sp.evaluate(() => {
  const reg = document.createElement('div'); reg.className = 'rk-toasts'; reg.setAttribute('role', 'status'); reg.id = 't-toasts';
  document.querySelector('.rk').append(reg); window.tToasts = Rekta.toaster(reg);
  window.tToasts.level('volume', 0.4, { icon: 'theme' });
});
await sp.waitForTimeout(300);
const lv1 = await sp.evaluate(() => { const t = document.querySelector('#t-toasts .rk-toast'); window.tLevelEl = t; return { level: t.classList.contains('rk-toast--level'), word: t.querySelector('.rk-toast-msg').textContent, fig: t.querySelector('.rk-toast-level').textContent, v: t.querySelector('.rk-meter').style.getPropertyValue('--rk-v'), icon: !!t.querySelector('svg.rk-icon'), act: !!t.querySelector('button') }; });
ok(lv1.level && lv1.word === 'volume' && lv1.fig === '40%' && lv1.v === '0.4' && lv1.icon && !lv1.act, 'a level indicator: icon, word, meter and figure, no action', JSON.stringify(lv1));
await until(sp, async () => (await sp.evaluate(() => document.querySelectorAll('#t-toasts .rk-toast').length)) === 0, 3000);   // the first one has gone
// Pressed again 0.7s in, and looked at 1s after that: past timing.level from the first press, so it stands only if its
// time started again. Timed in the page, so round trips to a busy runner do not move the presses.
const lv2 = await sp.evaluate(async () => {
  const wait = (t) => new Promise((r) => setTimeout(r, t));
  window.tToasts.level('volume', 0.5); window.tLevelEl = document.querySelector('#t-toasts .rk-toast:not([data-rk-phase])');
  await wait(700);
  window.tToasts.level('volume', 0.6);
  await wait(1000);
  const all = document.querySelectorAll('#t-toasts .rk-toast');
  return { n: all.length, same: all[0] === window.tLevelEl, fig: all[0].querySelector('.rk-toast-level').textContent, leaving: all[0].hasAttribute('data-rk-phase') };
});
ok(lv2.n === 1 && lv2.same && lv2.fig === '60%' && !lv2.leaving, 'pressed again, it stays and only its value changes; its time starts again', JSON.stringify(lv2));
ok(await until(sp, async () => (await sp.evaluate(() => document.querySelectorAll('#t-toasts .rk-toast').length)) === 0, 2500), 'it goes timing.level after the last change');
await sp.evaluate(() => { window.tToasts.destroy(); document.querySelector('#t-toasts').remove(); });
// Removing entries: Delete moves focus on, a button, a drag past half; a short drag glides back and clicks nothing.
await sp.evaluate(() => {
  const ul = document.createElement('ul'); ul.className = 'rk-rows'; ul.id = 't-rem'; ul.style.cssText = 'position:fixed;left:200px;top:200px;width:300px;background:var(--rk-bg);z-index:5;margin:0';
  ul.innerHTML = ['a', 'b', 'c', 'd'].map((x) => `<li id="t-r-${x}" tabindex="0"><span>${x}</span><button class="rk-btn rk-btn--icon rk-btn--quiet" type="button" data-rk-remove aria-label="dismiss">×</button></li>`).join('');
  document.body.append(ul);
  window.tRemoved = []; window.tClicks = 0;
  ul.addEventListener('click', () => { window.tClicks += 1; });
  Rekta.removable(ul, { onRemove: (li, { left }) => { window.tRemoved.push(`${li.id.slice(4)}:${left}`); li.remove(); } });
  document.querySelector('#t-r-b').focus();
});
await sp.keyboard.press('Delete');
const del = await sp.evaluate(() => ({ removed: window.tRemoved.join(), focus: document.activeElement.id, attr: document.querySelector('#t-rem').hasAttribute('data-rk-removable'), ta: getComputedStyle(document.querySelector('#t-r-a')).touchAction }));
ok(del.removed === 'b:false' && del.focus === 't-r-c' && del.attr && del.ta === 'pan-y', 'Delete removes the focused entry, focus moves to the next', JSON.stringify(del));
await sp.click('#t-r-a [data-rk-remove]');
ok((await sp.evaluate(() => window.tRemoved.join())) === 'b:false,a:false', 'its remove button removes it');
const dragRow = async (sel, dx) => {
  const b = await sp.evaluate((s) => { const r = document.querySelector(s).getBoundingClientRect(); return { x: r.left + 40, y: r.top + r.height / 2 }; }, sel);
  await sp.mouse.move(b.x, b.y); await sp.mouse.down();
  for (let i = 1; i <= 8; i += 1) { await sp.mouse.move(b.x + (dx * i) / 8, b.y); await sp.waitForTimeout(16); }
  const mid = await sp.evaluate((s) => new DOMMatrixReadOnly(getComputedStyle(document.querySelector(s)).transform).m41, sel);
  await sp.mouse.up();
  return mid;
};
const followed = await dragRow('#t-r-c', 60);
await sp.waitForTimeout(400);
const short = await sp.evaluate(() => ({ removed: window.tRemoved.length, there: !!document.querySelector('#t-r-c'), x: new DOMMatrixReadOnly(getComputedStyle(document.querySelector('#t-r-c')).transform).m41, clicks: window.tClicks }));
ok(Math.round(followed) === 60 && short.removed === 2 && short.there && short.x === 0 && short.clicks === 1, 'dragged a little, the entry follows and glides back; the drag clicks nothing', JSON.stringify({ followed, ...short }));
await dragRow('#t-r-d', 200);
ok(await until(sp, async () => (await sp.evaluate(() => window.tRemoved.join())) === 'b:false,a:false,d:true'), 'dragged past half its width, it leaves that way and is removed', await sp.evaluate(() => window.tRemoved.join()));
await sp.evaluate(() => document.querySelector('#t-rem').remove());
// A launch tile: icon at the top leading corner, the count at the trailing one, the name at the foot; peek only when wide.
const launch = await sp.evaluate(() => {
  const g = document.createElement('div'); g.className = 'rk-tiles'; g.style.cssText = 'width:700px';
  const pic = 'data:image/svg+xml,' + encodeURIComponent('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 8 8"><rect width="8" height="8" fill="%23c33"/></svg>');
  const tile = (cls, id) => `<a class="rk-tile rk-launch ${cls}" id="${id}" href="#"><img class="rk-picture" src="${pic}" alt=""><span class="rk-launch-count">12</span><span class="rk-launch-peek">Ana: the build is green again</span><span class="rk-launch-name">Mail</span></a>`;
  g.innerHTML = tile('rk-w2', 't-l2') + tile('', 't-l1') + tile('rk-w4', 't-l4') + tile('rk-h2', 't-lh');
  document.querySelector('[data-rk-place="home"] .rk-sec').append(g);
  const box = (s) => document.querySelector(s).getBoundingClientRect();
  const t = box('#t-l2'), i = box('#t-l2 .rk-picture'), c = box('#t-l2 .rk-launch-count'), n = box('#t-l2 .rk-launch-name'), ih = box('#t-lh .rk-picture');
  const r = {
    icon: [Math.round(i.width), Math.round(i.left - t.left), Math.round(i.top - t.top)], count: Math.round(t.right - c.right), name: Math.round(t.bottom - n.bottom),
    peekWide: box('#t-l2 .rk-launch-peek').height > 0, peekSmall: getComputedStyle(document.querySelector('#t-l1 .rk-launch-peek')).display,
    w4: Math.round(box('#t-l4').width), unit: Rekta.tokens.px('--rk-tile-unit'), gap: Rekta.tokens.px('--rk-space-s'), large: Math.round(ih.width),
  };
  g.remove();
  return r;
});
ok(launch.icon.join() === '48,12,12' && launch.count === 12 && launch.name === 12 && launch.peekWide && launch.peekSmall === 'none' && launch.large === 96, 'a launch tile: its icon at the tile icon size, the count and the name at its corners, peek only when wide', JSON.stringify(launch));
ok(launch.w4 === Math.round(4 * launch.unit + 3 * launch.gap), 'a tile four units wide spans four units sideways', JSON.stringify(launch));
// The title bar: window buttons touch-target wide, close quiet like the others; another window active, the name steps back.
const tbar = await sp.evaluate(async () => {
  const wait = (t) => new Promise((res) => setTimeout(res, t));
  const bar = document.createElement('div'); bar.className = 'rk-titlebar rk-titlebar--inline'; bar.style.cssText = 'position:fixed;left:0;top:300px;width:600px;z-index:5';
  bar.innerHTML = '<span class="rk-titlebar-name"><b>Rekta</b> Design</span><span class="rk-titlebar-place">today</span><span class="rk-titlebar-win">'
    + ['minus', 'maximize', 'close'].map((n) => `<button class="rk-btn rk-btn--icon rk-btn--quiet" type="button" aria-label="${n}">${Rekta.icon(n).outerHTML}</button>`).join('') + '</span>';
  document.body.append(bar);
  const t = Rekta.titlebar(bar); await wait(50);
  const btns = [...bar.querySelectorAll('.rk-titlebar-win > button')];
  const r = { widths: btns.map((b) => Math.round(b.getBoundingClientRect().width)), end: Math.round(bar.getBoundingClientRect().right - btns[2].getBoundingClientRect().right), quiet: getComputedStyle(btns[2]).color === getComputedStyle(btns[0]).color, active: !bar.hasAttribute('data-rk-inactive') };
  const name = bar.querySelector('.rk-titlebar-name'), place = bar.querySelector('.rk-titlebar-place'), had = document.hasFocus;
  r.nameBefore = getComputedStyle(name).color;
  document.hasFocus = () => false; dispatchEvent(new Event('blur')); await wait(50);
  r.inactive = bar.hasAttribute('data-rk-inactive'); r.nameAfter = getComputedStyle(name).color; r.place = getComputedStyle(place).color;
  document.hasFocus = had; dispatchEvent(new Event('focus')); await wait(50);
  r.back = !bar.hasAttribute('data-rk-inactive');
  t.destroy(); bar.remove();
  return r;
});
ok(tbar.widths.join() === '44,44,44' && tbar.end === 0 && tbar.quiet, 'window buttons: touch-target wide at the bar\'s end, close as quiet as the others', JSON.stringify(tbar));
ok(tbar.active && tbar.inactive && tbar.nameAfter === tbar.place && tbar.nameAfter !== tbar.nameBefore && tbar.back, 'another window active: the name steps back to the place\'s colour, and comes back', JSON.stringify(tbar));
// Running things in an app bar without a live state: in order of start, the one in front marked, the commands at the
// end; when the bar is full they move into "more" before any command, the one in front last.
const run = await sp.evaluate(async () => {
  const wait = (t) => new Promise((res) => setTimeout(res, t));
  const icon = '<svg viewBox="0 0 20 20" aria-hidden="true"><circle cx="9" cy="9" r="5.5"/></svg>';
  const el = document.createElement('div'); el.className = 'rk-appbar'; el.style.cssText = 'width:700px;left:0;right:auto;bottom:200px';
  el.innerHTML = `<button class="rk-ab rk-ab-run" id="t-w1" type="button">${icon}<span>notes</span></button><button class="rk-ab rk-ab-run" id="t-w2" type="button" aria-current="true">${icon}<span>terminal</span></button>`
    + `<button class="rk-ab rk-ab-run" id="t-w3" type="button">${icon}<span>files</span></button><button class="rk-ab" id="t-cmd" type="button">${icon}<span>search</span></button>`
    + `<button class="rk-ab rk-ab-more" type="button" hidden>${icon}<span>more</span></button>`;
  document.querySelector('.rk').append(el);
  const a = Rekta.appbar(el); await wait(100);
  const col = (s) => getComputedStyle(el.querySelector(s)).color, root = getComputedStyle(document.documentElement);
  const toRgb = (v) => { const p = document.createElement('i'); p.style.color = v; document.body.append(p); const c = getComputedStyle(p).color; p.remove(); return c; };
  const r = {
    front: col('#t-w2') === toRgb(root.getPropertyValue('--rk-text-primary')), back: col('#t-w1') === toRgb(root.getPropertyValue('--rk-text-contrast-high')),
    end: Math.round(el.getBoundingClientRect().right - el.querySelector('#t-cmd').getBoundingClientRect().right - parseFloat(getComputedStyle(el).paddingRight)),
  };
  el.style.width = '330px'; a.layout(); await wait(100);
  r.moved = ['#t-w1', '#t-w2', '#t-w3', '#t-cmd'].map((s) => (el.querySelector(s).hasAttribute('data-rk-overflow') ? 1 : 0)).join('');
  a.destroy(); el.remove();
  return r;
});
ok(run.front && run.back && run.end === 0, 'running things: the one in front in the primary colour, the others stepped back; with no live state the commands still stand at the end', JSON.stringify(run));
ok(run.moved === '1011' || run.moved === '1010' || run.moved === '0010', 'a full bar moves running things into "more" first, the latest started first, the one in front last', JSON.stringify(run));
await sp.close();

// The showcase shows what the reports on 0.5.0 added, each demo where it belongs and working: a status region, a
// field disabled with its reason, the beads, tags that fade in place, the facets' first paint, the carried titles,
// a called place on its accent surface with launch tiles in every width and content fills, a desktop with window
// buttons, running things, a clock with its month, the level in a toast or the bar, programs in the background, a
// disabled bar control, notifications from other programs in a centre, quick settings chosen in place, the session,
// the lock screen and the recent stack.
const sy = await open();
{
  // Tiles: a status region runs; cancelling asks with the destructive answer, then a control that could not act can.
  await sy.evaluate(() => window.site.home.goTo('tiles')); await sy.waitForTimeout(600);
  const job = await sy.evaluate(() => ({ region: !!document.querySelector('#tiles .rk-status-region .rk-figures'), run: +document.querySelector('#job-run').textContent, word: document.querySelector('#job-tag').textContent, again: document.querySelector('#job-again').getAttribute('aria-disabled'), why: (document.getElementById(document.querySelector('#job-again').dataset.rkWhy) || {}).textContent }));
  ok(job.region && job.run > 0 && job.word === 'active' && job.again === 'true' && /still running/.test(job.why), 'tiles: a status region counts, its tag says active, "start again" says why it cannot act', JSON.stringify(job));
  await sy.click('#job-cancel'); await sy.waitForTimeout(400);
  const ask = await sy.evaluate(() => { const d = [...document.querySelectorAll('dialog')].find((x) => x.open); return d && { ok: d.querySelector('[value=ok]').dataset.status, primary: d.querySelector('.rk-btn--primary').value, focus: document.activeElement.value }; });
  ok(ask && ask.ok === 'error' && ask.primary === 'cancel' && ask.focus === 'cancel', 'confirm({ safe: "cancel" }): keeping it is the primary answer, cancelling the build is destructive', JSON.stringify(ask));
  await sy.click('dialog[open] [value=ok]'); await sy.waitForTimeout(500);
  ok(await sy.evaluate(() => document.querySelector('#job-tag').textContent === 'idle' && !document.querySelector('#job-tag').dataset.status && document.querySelector('#job-cancel').hidden && document.querySelector('#job-again').getAttribute('aria-disabled') !== 'true'), 'cancelled, the region is idle and can start again');
  ok(await sy.evaluate(() => !!document.querySelector('#live-tiles a[href="#notes"] [data-rk-carry]') && !!document.querySelector('#tile-apps [data-rk-carry]')), 'more tiles carry their title into the place they open');
  // Inputs: a field disabled with its reason looks disabled, says why and refuses.
  const keep = await sy.evaluate(() => {
    const i = document.querySelector('#keep-days'), plus = i.nextElementSibling; plus.click();
    return { value: i.value, aria: i.getAttribute('aria-disabled'), why: document.querySelector('#keep-why').textContent, described: (i.getAttribute('aria-describedby') || '').includes('keep-why')};
  });
  ok(keep.value === '30' && keep.aria === 'true' && /organisation/.test(keep.why) && keep.described, 'inputs: a stepper with aria-disabled says why and refuses its buttons', JSON.stringify(keep));
  // Data: the beads run for a long wait.
  await sy.evaluate(() => document.querySelector('#startup').click()); await sy.waitForTimeout(200);
  ok(await sy.evaluate(() => { const b = document.querySelector('#beads'); return !b.hidden && b.matches('.rk-dots--beads') && b.children.length === 3 && [...b.children].every((i) => i.getAnimations().length); }), 'data: "start up" runs the beads');
  // Lists: the statuses a filter shows stand as tags that fade in place, without a path.
  await sy.evaluate(() => window.site.home.goTo('lists')); await sy.waitForTimeout(500);
  await sy.click('#comp-status'); await sy.click('#comp-status-menu [data-v="success"]'); await sy.keyboard.press('Escape'); await sy.waitForTimeout(60);
  const tagIn = await sy.evaluate(() => { const t = document.querySelector('#comp-tags [data-v="success"]'); const k = t.getAnimations().flatMap((a) => a.effect.getKeyframes()); return { shown: !t.hidden, moves: k.some((f) => f.transform && f.transform !== 'none'), fades: k.some((f) => 'opacity' in f), others: [...document.querySelectorAll('#comp-tags .rk-tag:not([data-v="success"])')].every((x) => x.hidden) }; });
  ok(tagIn.shown && tagIn.others && !tagIn.moves, 'lists: the chosen status stands as a tag and fades in place (data-rk-motion="inline")', JSON.stringify(tagIn));
  await sy.evaluate(() => document.querySelector('#comp-clear').click()); await sy.waitForTimeout(500);
  ok(await sy.evaluate(() => [...document.querySelectorAll('#comp-tags .rk-tag')].every((t) => t.hidden)), 'cleared, the tag goes');
  // The showcase's own panorama never ends flush: the window's edge cuts into a section, space-xl from its ends.
  await sy.evaluate(() => window.site.home.goTo('overview')); await sy.waitForTimeout(800);
  const edge = await sy.evaluate(() => {
    const pan = document.querySelector('#pan'), at = pan.getBoundingClientRect().left + pan.clientWidth, xl = Rekta.tokens.px('--rk-space-xl');
    const sec = [...pan.querySelectorAll(':scope > .rk-sec')].map((x) => x.getBoundingClientRect()).find((r) => r.left <= at && r.right >= at);
    return { inside: !!sec, room: sec ? Math.round(Math.min(at - sec.left, sec.right - at)) : 0, xl };
  });
  ok(edge.inside && edge.room >= edge.xl - 1, 'panorama: the window\'s edge falls inside a section, never flush', JSON.stringify(edge));
  ok(await sy.evaluate(() => { const f = document.querySelector('#panorama .rk-facets > div'); return !!f && f.contains(document.querySelector('#facet-0')) && f.hasAttribute('data-rk-narrow'); }), 'panorama: the facets sit in .rk-facets, and pivot() decides narrow there');
}
{
  // A whole system: the desktop's window buttons, the running things, the inactive name.
  await sy.evaluate(() => window.site.home.goTo('system')); await sy.waitForTimeout(700);
  const run0 = await sy.evaluate(() => ({ runs: [...document.querySelectorAll('#screen-bar .rk-ab-run')].map((b) => `${b.dataset.app}${b.getAttribute('aria-current') === 'true' ? '*' : ''}`).join(), pic: document.querySelector('#screen-bar .rk-ab-run img.rk-picture').naturalWidth > 0, win: document.querySelector('#win-name').textContent, btns: [...document.querySelectorAll('#win-bar .rk-titlebar-win > .rk-btn')].map((b) => b.getAttribute('aria-label')).join() }));
  ok(run0.runs === 'notes*,chat' && run0.pic && run0.win === 'Notes' && run0.btns === 'minimize,maximize,close', 'system: running things in the bar, the one in front marked, a picture icon; the window has its three buttons', JSON.stringify(run0));
  await sy.click('#win-max');
  // Measured once the window's own transitions have ended, not after a fixed wait.
  const max = await sy.evaluate(async () => { await Promise.all(document.querySelector('#win').getAnimations().map((x) => x.finished.catch(() => {}))); return { max: document.querySelector('#win').hasAttribute('data-max'), label: document.querySelector('#win-max').getAttribute('aria-label'), glyph: document.querySelector('#win-max use').getAttribute('href'), w: Math.round(document.querySelector('#win').getBoundingClientRect().width), desk: Math.round(document.querySelector('#desk').getBoundingClientRect().width) }; });
  ok(max.max && max.label === 'restore' && max.glyph === '#rk-i-restore' && Math.abs(max.w - max.desk) <= 1, 'maximize fills the desktop and the button becomes restore', JSON.stringify(max));
  await sy.click('#win-max'); await sy.waitForTimeout(500);
  ok(await sy.evaluate(() => !document.querySelector('#win').hasAttribute('data-max') && document.querySelector('#win-max').getAttribute('aria-label') === 'maximize'), 'restore brings it back');
  await sy.mouse.click(...(await sy.evaluate(() => { const r = document.querySelector('#desk').getBoundingClientRect(); return [r.right - 20, r.bottom - 20]; }))); await sy.waitForTimeout(100);
  ok(await sy.evaluate(() => document.querySelector('#win-bar').hasAttribute('data-rk-inactive') && !document.querySelector('#screen-bar [aria-current]')), 'a press on the desktop: the window is inactive, its name steps back');
  await sy.click('#win-min'); await sy.waitForTimeout(100);
  ok(await sy.evaluate(() => document.querySelector('#win-name').textContent === 'Chat'), 'minimize brings the next one to the front');
  await sy.click('#win-min'); await sy.waitForTimeout(100);
  ok(await sy.evaluate(() => document.querySelector('#win').hidden && !document.querySelector('#screen-bar [aria-current]')), 'all minimized, no window shows');
  await sy.click('#screen-bar .rk-ab-run[data-app="notes"]'); await sy.waitForTimeout(100);
  ok(await sy.evaluate(() => !document.querySelector('#win').hidden && document.querySelector('#win-name').textContent === 'Notes' && document.querySelector('#screen-bar .rk-ab-run[data-app="notes"]').getAttribute('aria-current') === 'true'), 'a running thing brings its window back');
  // The bar: a centred clock and its month, a disabled control with its reason, two sides that give way.
  const clock = await sy.evaluate(() => { const b = document.querySelector('#screen-bar').getBoundingClientRect(), c = document.querySelector('#clock').getBoundingClientRect(); return { off: Math.abs(c.left + c.width / 2 - (b.left + b.width / 2)), cast: document.querySelector('#sb-cast').getAttribute('aria-disabled'), tip: document.querySelector('#sb-cast').dataset.rkTip }; });
  ok(clock.off <= 1 && clock.cast === 'true' && /nearby/.test(clock.tip), 'the clock stands in the middle of the bar; "cast" cannot act and its tooltip says why', JSON.stringify(clock));
  await sy.click('#clock'); await sy.waitForTimeout(300);
  const cal = await sy.evaluate(() => { const f = document.querySelector('#clock-cal'), c = document.querySelector('#clock').getBoundingClientRect(), r = f.getBoundingClientRect(); return { open: f.matches(':popover-open'), today: +document.querySelector('#month [aria-current="date"]').textContent === new Date().getDate(), off: Math.abs(r.left + r.width / 2 - (c.left + c.width / 2)), days: document.querySelectorAll('#month thead th').length }; });
  ok(cal.open && cal.today && cal.days === 7 && cal.off <= 1, 'the clock opens this month (.rk-month), today framed, centred on the clock', JSON.stringify(cal));
  await sy.keyboard.press('Escape'); await sy.waitForTimeout(200);
  // The level: in a toast, then in the bar's middle in place of the clock.
  await sy.click('#vol-up'); await sy.waitForTimeout(400);
  ok(await sy.evaluate(() => { const t = document.querySelector('.rk-toast.rk-toast--level:not([data-rk-phase])'); return !!t && t.textContent.includes('volume') && t.textContent.includes('70') && !t.dataset.status; }), 'volume up: the level indicator in a toast, at 70%');
  await sy.click('label:has(> input[name=level-in][value=bar])'); await sy.click('#vol-down'); await sy.waitForTimeout(600);
  ok(await sy.evaluate(() => document.querySelector('#clock').hasAttribute('data-rk-indicating') && document.querySelector('#clock .level b').textContent === '60%' && document.querySelector('#level-said').textContent === 'volume 60%'), 'in the bar: the level stands in place of the clock (indicate()) and is said in a live region');
  ok(await until(sy, () => sy.evaluate(() => !document.querySelector('#clock').hasAttribute('data-rk-indicating') && !!document.querySelector('#clock-time').offsetWidth), 3500), 'then the clock turns back');
  // Programs in the background: a menu with icons and a second level.
  await sy.evaluate(() => { const b = document.querySelector('#sb-bg'); if (b.hasAttribute('data-rk-overflow')) window.site.screenBar.layout(); });
  const bgShown = await sy.evaluate(() => !document.querySelector('#sb-bg').hasAttribute('data-rk-overflow'));
  if (bgShown) {
    await sy.click('#sb-bg'); await sy.waitForTimeout(300); await sy.keyboard.press('Enter'); await sy.waitForTimeout(200);
    const lvl = await sy.evaluate(() => ({ level: !document.querySelector('#bg-music').hidden, back: (document.querySelector('#bg-music .rk-menu-back') || {}).textContent, focus: document.activeElement.dataset.act, icons: document.querySelectorAll('#bg-menu .rk-menu-item > .rk-icon').length }));
    ok(lvl.level && /Music/.test(lvl.back || '') && lvl.focus === 'previous' && lvl.icons >= 5, 'in the background: a program\'s own menu is a second level with icons', JSON.stringify(lvl));
    await sy.keyboard.press('Escape'); await sy.keyboard.press('Escape'); await sy.waitForTimeout(200);
  } else ok(true, 'in the background: in "more" at this width');
  // Notifications from another program: from the edge, with its picture and title; the centre takes it on top and
  // the count peeks; dismissed with undo; a tap hints at the swipe.
  await sy.click('#n-message'); await sy.waitForTimeout(400);
  const note = await sy.evaluate(() => { const t = document.querySelector('.rk-toast:not([data-rk-phase])'); return { edge: t.getAttribute('data-rk-from'), title: (t.querySelector('.rk-toast-title') || {}).textContent, pic: !!t.querySelector('img'), first: document.querySelector('#centre > li b').textContent, n: document.querySelectorAll('#centre > li').length, count: document.querySelector('#sb-count').textContent }; });
  ok(note.edge === 'edge' && note.title === 'Ana Ruiz' && note.pic && note.first === 'Ana Ruiz' && note.n === 4 && note.count === '4', 'a message arrives: a toast from the edge with its picture and title; on top of the centre; the count follows', JSON.stringify(note));
  await sy.click('#n-backup'); await sy.waitForTimeout(400);
  ok(await sy.evaluate(() => { const t = document.querySelector('.rk-toast:not([data-rk-phase])'); return !!t.querySelector('.rk-toast-msg svg.rk-icon') && t.querySelector('.rk-toast-title').textContent === 'Backup'; }), 'a backup finishes: its icon, drawn in the toast\'s colour');
  await sy.click('#centre > li:nth-child(2) .centre-text'); await sy.waitForTimeout(80);
  ok(await sy.evaluate(() => document.querySelector('#centre > li:nth-child(2)').getAnimations().length > 0), 'a tap on a notification hints at its swipe (nudge())');
  await sy.click('#centre > li:first-child [data-rk-remove]');
  ok(await until(sy, () => sy.evaluate(() => { const t = document.querySelector('.rk-toast:not([data-rk-phase]) .rk-toast-msg'); return document.querySelectorAll('#centre > li').length === 4 && !!t && /Dismissed/.test(t.textContent); })), 'dismiss removes it, and a toast offers undo');
  await sy.click('.rk-toast:not([data-rk-phase]) .rk-toast-act'); await sy.waitForTimeout(700);
  ok(await sy.evaluate(() => document.querySelectorAll('#centre > li').length === 5 && document.querySelector('#centre > li b').textContent === 'Backup'), 'undo puts it back where it was');
  // Quick settings: a side panel; a group gives its place to its list.
  await sy.evaluate(() => document.querySelector('#sb-quick').click());
  // Measured once its slide in has ended, not after a fixed wait a busy runner may not have reached.
  const quick = await sy.evaluate(async () => {
    const d = document.querySelector('#quick-panel');
    await Promise.all(d.getAnimations().map((a) => a.finished.catch(() => {})));
    const r = d.getBoundingClientRect();
    return { open: d.open, right: r.right, vw: document.documentElement.clientWidth, h: r.height, vh: innerHeight };
  });
  ok(quick.open && Math.abs(quick.right - quick.vw) < 1 && Math.abs(quick.h - quick.vh) < 1, 'quick settings: a side panel at the trailing edge, full height', JSON.stringify(quick));
  await sy.click('#qs-net'); await sy.waitForTimeout(400);
  ok(await sy.evaluate(() => !document.querySelector('#qs-nets').hidden && document.querySelector('#qs-nets > .rk-in-back').textContent === 'back · wi-fi'), 'wi-fi opens its networks in place, under "back · wi-fi"');
  await sy.click('label:has(> input[value="Office 5G"])'); await sy.waitForTimeout(400);
  ok(await sy.evaluate(() => document.querySelector('#qs-nets').hidden && document.querySelector('#qs-net-v').textContent === 'Office 5G' && document.activeElement.id === 'qs-net'), 'choosing one goes back to the group, which says it');
  await sy.keyboard.press('Escape'); await sy.waitForTimeout(700);
  // The session, the lock screen and signing in again.
  await sy.click('#session-open'); await sy.waitForTimeout(400);
  ok(await sy.evaluate(() => { const d = document.querySelector('#session'); return d.open && d.querySelector('.rk-btn--primary').value === 'lock' && Math.round(d.getBoundingClientRect().width) === document.documentElement.clientWidth; }), 'session: a bar across the top, lock first');
  await sy.click('#session [value=lock]'); await sy.waitForTimeout(1200);
  const locked = await sy.evaluate(() => ({ shown: !document.querySelector('#lock').hidden, display: document.querySelector('#lock-time').matches('.rk-display'), size: parseFloat(getComputedStyle(document.querySelector('#lock-time')).fontSize), inert: document.querySelector('#desk').inert, focus: document.activeElement.id }));
  ok(locked.shown && locked.display && locked.size >= 80 && locked.inert && locked.focus === 'lock-pw', 'lock: the lock screen with the time in the display step; the desktop behind it is inert', JSON.stringify(locked));
  await sy.click('#lock-in'); await sy.waitForTimeout(300);
  ok(await sy.evaluate(() => /Type the password/.test(document.querySelector('#lock-form').textContent) && document.querySelector('#lock-pw').getAttribute('aria-invalid') === 'true'), 'an empty password is refused at the field');
  await sy.fill('#lock-pw', 'x'); await sy.click('#lock-in'); await sy.waitForTimeout(300);
  ok(await sy.evaluate(() => !document.querySelector('#lock-dots').hidden), 'signing in: the dots run');
  ok(await until(sy, () => sy.evaluate(() => document.querySelector('#lock').hidden && !document.querySelector('#desk').inert), 4000), 'then the desktop is back');
  // The recent stack: the one before the current one has focus; Enter brings it to the front.
  await sy.click('#recent-open'); await sy.waitForTimeout(400);
  const rec = await sy.evaluate(() => ({ open: document.querySelector('#recent').open, tiles: [...document.querySelectorAll('#recent-tiles > .rk-tile--preview')].map((t) => t.dataset.app).join(), focus: document.activeElement.dataset.app, foot: !!document.querySelector('#recent-tiles .rk-tile-foot') }));
  ok(rec.open && rec.tiles.startsWith('notes,') && rec.focus === rec.tiles.split(',')[1] && rec.foot, 'the recent stack: preview tiles, the last used first, the one before has focus', JSON.stringify(rec));
  await sy.keyboard.press('Enter'); await sy.waitForTimeout(300);
  ok(await sy.evaluate((f) => !document.querySelector('#recent').open && window.site.desk().front === f, rec.focus), 'Enter brings it to the front');
}
{
  // All apps: a called place on its accent surface; launch tiles one to four units wide, content fills, the grid's
  // arrows and letters; a launch tile opens its program on the desktop.
  await sy.click('#tile-apps'); await sy.waitForTimeout(1500);
  const apps = await sy.evaluate(() => {
    const head = document.querySelector('[data-rk-place="apps"] .rk-accent-surface'), probe = document.createElement('i');
    probe.style.color = 'var(--rk-accent)'; head.append(probe); const accent = getComputedStyle(probe).color; probe.remove();
    const tiles = [...document.querySelectorAll('#apps-grid .rk-launch')], unit = Rekta.tokens.px('--rk-tile-unit');
    return { shown: !document.querySelector('[data-rk-place="apps"]').hidden, called: document.querySelector('[data-rk-place="apps"]').hasAttribute('data-rk-called'), bg: getComputedStyle(head).backgroundColor === accent,
      widths: tiles.map((t) => Math.round(t.getBoundingClientRect().width / unit)).join(), icons: tiles.map((t) => Math.round(t.querySelector('.rk-icon, .rk-picture').getBoundingClientRect().width)).join(),
      fill: getComputedStyle(document.querySelector('#apps-grid [data-app="notes"]')).backgroundColor, teal: (() => { const i = document.createElement('i'); i.style.color = 'var(--rk-content-teal)'; document.body.append(i); const c = getComputedStyle(i).color; i.remove(); return c; })(),
      running: document.querySelectorAll('#apps-running > .rk-tile--preview').length };
  });
  ok(apps.shown && apps.called && apps.bg, 'all apps: a called place whose header is the accent surface', JSON.stringify(apps));
  ok(apps.widths === '2,1,1,2,3,4' && apps.icons === '96,48,48,48,48,48', 'launch tiles one to four units wide; the icon 48px, 96px on a tile two high', JSON.stringify(apps));
  ok(apps.fill === apps.teal && apps.running >= 2, 'content fills on the launch tiles; the running programs as preview tiles', JSON.stringify(apps));
  await sy.click('label:has(> #apps-fills)'); await sy.waitForTimeout(300);
  ok(await sy.evaluate(() => document.querySelector('#apps-grid').dataset.rkFills === 'plain' && getComputedStyle(document.querySelector('#apps-grid [data-app="notes"]')).backgroundColor === getComputedStyle(document.querySelector('#apps-grid [data-app="chat"]')).backgroundColor), 'one switch makes the set plain');
  await sy.focus('#apps-grid [data-app="chat"]'); await sy.keyboard.press('ArrowRight');
  ok((await sy.evaluate(() => document.activeElement.dataset.app)) === 'notes', 'the arrows move between launch tiles (grid())');
  await sy.keyboard.press('t');
  ok((await sy.evaluate(() => document.activeElement.dataset.app)) === 'terminal', 'a letter jumps to the program that starts with it');
  await sy.keyboard.press('Enter'); await sy.waitForTimeout(2200);
  ok(await sy.evaluate(() => !document.querySelector('[data-rk-place="home"]').hidden && document.querySelector('#win-name').textContent === 'Terminal' && document.querySelector('#screen-bar .rk-ab-run[data-app="terminal"]').getAttribute('aria-current') === 'true'), 'a launch tile opens its program on the desktop, in front, in the bar');
}
await sy.close();

// Reduced motion: a place change is instant, never skipped.
const r = await open({ reducedMotion: 'reduce' });
await r.click('#tile-notes'); await r.waitForTimeout(150);
ok(await visible(r, 'notes') && !(await visible(r, 'home')), 'reduced motion: the place changes at once');
await setSize(r, 'w2'); await filterComp(r, 'spark'); await r.waitForTimeout(20);
ok(await r.evaluate(() => document.querySelector('#built').getAnimations().length === 0 && document.querySelector('#built').classList.contains('rk-w2') && document.querySelectorAll('#comp-body tr:not([hidden])').length === 1), 'reduced motion: a resize and a filter apply at once');
ok(await r.evaluate(() => {
  const box = document.createElement('div'); box.className = 'rk-dots rk-dots--beads'; box.style.cssText = 'position:fixed;left:0;top:0;width:500px';
  box.innerHTML = '<i></i><i></i><i></i>'; document.body.append(box);
  const lefts = [...box.children].map((i) => Math.round(i.getBoundingClientRect().left)), still = [...box.children].every((i) => !i.getAnimations().length && getComputedStyle(i).opacity === '1');
  box.remove();
  return still && lefts.join() === '235,247,259';
}), 'reduced motion: the beads stand still in the middle');
ok(await r.evaluate(async () => {
  const t = document.createElement('div'); t.className = 'rk-tile'; t.style.cssText = 'position:fixed;left:0;top:0;width:150px;height:150px';
  document.querySelector('.rk').append(t);
  await Rekta.nudge(t, 'right');
  const still = !t.getAnimations().length;
  t.remove();
  return still;
}), 'reduced motion: no gesture hint');
await r.close();

await browser.close();
server.close();
console.log(`${passed} checks passed`);
if (failures.length) console.log(`FAILED:\n  ${failures.join('\n  ')}`);
console.log(errors.length ? `PAGE ERRORS:\n  ${errors.join('\n  ')}` : 'no page errors');
process.exit(failures.length || errors.length ? 1 : 0);

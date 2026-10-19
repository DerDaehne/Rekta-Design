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
  }
  await p.screenshot({ path: join(shots, `rest-${w}x${h}-${scheme}.png`) });
  await p.close();
}

const p = await open();
ok(await p.evaluate(() => getComputedStyle(document.querySelector('#pan')).display === 'flex'), 'desktop panorama is sideways');
ok(await p.evaluate(() => document.querySelector('#back').hidden), 'app bar back hidden at home');
ok(await p.evaluate(() => { const n = document.querySelector('#nav'); return n.scrollHeight <= n.clientHeight + 1; }), 'the section strip is not cut');

// Generated from the tokens, measured live.
const count = await p.evaluate(() => JSON.parse(document.querySelector('#tokens-data').textContent).length);
ok((await text(p, '#token-count')) === String(count), 'the tokens tile counts the tokens');
ok(await p.evaluate(() => [...document.querySelectorAll('[data-contrast]')].every((b) => /^\d+\.\d : 1$/.test(b.textContent))), 'contrast is measured live');
ok(!(await p.evaluate(() => /\{\{|<!--@/.test(document.body.innerHTML))), 'every placeholder was filled at build time');
ok((await p.evaluate(() => { const seen = new Set(), twice = []; document.querySelectorAll('[id]').forEach((e) => { if (seen.has(e.id)) twice.push(e.id); seen.add(e.id); }); return twice.join(); })) === '', 'every id is unique');

// Tile → place: the turnstile, the address, the title, focus; the crumb back.
await p.click('#tile-notes'); await p.waitForTimeout(120);
ok(await p.evaluate(() => document.querySelectorAll('[data-rk-turn]').length > 3), 'leaving units turn');
await p.screenshot({ path: join(shots, 'turn-mid.png') });
await p.waitForTimeout(TURN);
ok(await visible(p, 'notes') && !(await visible(p, 'home')), 'the release notes show, home hides');
ok((await p.evaluate(() => location.hash)) === '#notes' && (await p.title()) === 'Rekta 0.2 · Rekta Design', 'address and title name the place', await p.title());
ok((await focused(p)) === 'notes-title', 'focus on the place title', await focused(p));
await p.click('[data-rk-place=notes] .rk-crumb'); await p.waitForTimeout(TURN);
ok(await visible(p, 'home') && (await p.evaluate(() => location.hash)) === '', 'the crumb goes back home');
ok((await focused(p)) === 'tile-notes', 'focus back on the opening tile', await focused(p));

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
ok(stripped.arrive, 'arrive() is exported for framework routers', JSON.stringify(stripped));

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
await p.click('#comp-table thead th:nth-child(2) .rk-sort'); await p.waitForTimeout(500);
ok((await p.evaluate(() => document.querySelector('#comp-body tr').dataset.name)) === 'App bar', 'sort by component');
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
  b.destroy(); el.remove(); document.documentElement.style.setProperty('--rk-banner-h', h);
  return r;
});
ok(ban.noTime, 'a banner state whose start is unknown has no time line');
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
await p.fill('#a-light', '#1f6f5c'); await p.waitForTimeout(100);
ok(!(await p.evaluate(() => document.querySelector('#a-light').closest('.rk-field').dataset.status)), 'corrected as you type');
await p.fill('#a-name', 'plum'); await p.click('#a-save'); await p.waitForTimeout(1200);
ok((await hint('a-name')).includes('already exists'), 'the server says no at the field', await hint('a-name'));
await p.fill('#a-name', 'sea'); await p.click('#a-save');
await p.waitForFunction(() => { const t = document.querySelector('.rk-toast:not([data-rk-phase]) .rk-toast-msg'); return t && t.textContent.startsWith('Accent'); }, null, { timeout: 4000 }).catch(() => {});
ok((await toast(p)).startsWith('Accent “sea” applied') && (await p.evaluate(() => getComputedStyle(document.documentElement).getPropertyValue('--rk-accent').trim())) === '#1f6f5c', 'the accent applies', `${await toast(p)} / ${await p.evaluate(() => getComputedStyle(document.documentElement).getPropertyValue('--rk-accent').trim() + ' ' + document.documentElement.dataset.accent + ' ' + localStorage.getItem('rekta-prefs'))}`);
ok(!(await p.getAttribute('#a-remove', 'aria-disabled')), 'remove can act now');
await p.click('#a-remove'); await p.waitForTimeout(400);
ok((await sheetTitle(p)) === 'Remove “sea”?' && (await p.evaluate(() => document.activeElement.textContent)) === 'keep it', 'confirm: the safe answer first');
await p.click('dialog[open] button[value=ok]');
await until(p, () => p.evaluate(() => !document.querySelector('dialog[open]')), 3000);   /* the sheet leaves first; its answer counts once it has gone */
ok((await p.evaluate(() => document.documentElement.dataset.accent)) === undefined && (await p.getAttribute('#a-remove', 'aria-disabled')) === 'true', 'removed, ink again', await p.evaluate(() => `accent ${document.documentElement.dataset.accent} remove ${document.querySelector('#a-remove').getAttribute('aria-disabled')} open ${!!document.querySelector('dialog[open]')} phase ${document.querySelector('dialog[open]')?.getAttribute('data-rk-phase')}`));

// Buttons: a running one keeps its colours, a destructive one is framed in the error colour, an icon button is square.
const btns = await p.evaluate(async () => {
  const rgb = (c) => { const e = document.createElement('i'); e.style.color = c; document.body.append(e); const v = getComputedStyle(e).color; e.remove(); return v; };
  const row = document.createElement('p'); row.className = 'rk-actions';
  row.innerHTML = '<button class="rk-btn rk-btn--primary" type="button">save</button><button class="rk-btn" type="button" data-status="error">delete</button>';
  document.querySelector('#forms').append(row);
  const [save, del] = row.children, before = getComputedStyle(save).backgroundColor;
  let release; const run = Rekta.pending(save, new Promise((r) => { release = r; }));
  await new Promise((r) => setTimeout(r, 50));
  const out = { before, during: getComputedStyle(save).backgroundColor, disabled: rgb('var(--rk-disabled)'), ad: save.getAttribute('aria-disabled'),
    pendingKeeps: save.getAttribute('aria-disabled') === 'true' && getComputedStyle(save).backgroundColor === before && getComputedStyle(save).color !== rgb('var(--rk-disabled)'),
    danger: getComputedStyle(del).color === rgb('var(--rk-status-error-full)') && getComputedStyle(del).borderTopColor === rgb('var(--rk-status-error-full)') && getComputedStyle(del).backgroundColor === 'rgba(0, 0, 0, 0)' };
  release(); await run; row.remove();
  return out;
});
ok(btns.pendingKeeps, 'a running button keeps its colours (it is not disabled)', JSON.stringify(btns));
ok(btns.danger, 'a destructive button: frame and words in the error colour, not filled', JSON.stringify(btns));
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
await p.click('#view-sheet .rk-sheet-close'); await p.waitForTimeout(200);
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

// Look: prefs, density, motion, the accent slot.
await go(p, 'look');
await p.check('input[name=theme][value=dark]'); await p.check('input[name=accent][value=plum]'); await p.waitForTimeout(300);
ok((await p.evaluate(() => document.documentElement.dataset.theme)) === 'dark' && (await p.evaluate(() => getComputedStyle(document.documentElement).getPropertyValue('--rk-accent').trim())) === '#c38aed', 'theme dark, the plum accent in dark', await p.evaluate(() => getComputedStyle(document.documentElement).getPropertyValue('--rk-accent').trim()));
await p.screenshot({ path: join(shots, 'dark-plum.png') });
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
await p.check('input[name=density][value=auto]'); await p.check('input[name=theme][value=system]'); await p.check('input[name=accent][value=ink]');
ok((await p.evaluate(() => JSON.parse(localStorage.getItem('rekta-prefs')).theme)) === 'system', 'prefs remembered');

// Sheet: the shading cancels; reset.
await p.click('#reset'); await p.waitForTimeout(300);
ok((await p.evaluate(() => getComputedStyle(document.documentElement).overflow)) === 'hidden', 'the page behind a sheet does not scroll');
await p.mouse.click(700, 850);
ok(await until(p, () => p.evaluate(() => !document.querySelector('#sheet').open), 3000), 'a tap on the shading cancels');
await p.click('#reset'); await p.waitForTimeout(300); await p.click('#sheet button[value=reset]'); await p.waitForTimeout(600);
ok((await toast(p)).startsWith('Demo reset') && (await text(p, '#presses')) === '0', 'reset restores the demo');

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
ok((await sh.evaluate(() => document.querySelectorAll('symbol[id^="rk-i-"]').length)) === 51 && (await sh.evaluate(() => document.querySelectorAll('.icon-gallery li').length)) === 51, 'fifty-one icons in the sprite and the gallery');
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
    + `<button class="rk-ab" id="t-goto" type="button" aria-keyshortcuts="Control+K Meta+K">${icon}<span>go to</span><span class="rk-key rk-ab-k" aria-hidden="true">Ctrl K</span></button>`
    + `<button class="rk-ab" type="button" data-rk-priority="5">${icon}<span>other</span></button><button class="rk-ab rk-ab-more" type="button" hidden>${icon}<span>more</span></button>`;
  const name = document.createElement('h1'); name.className = 'rk-place-name'; name.textContent = 'overview';
  document.querySelector('[data-rk-place="home"]').prepend(name);
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
  a.destroy(); el.remove(); await wait(50);
  r.after = !document.documentElement.hasAttribute('data-rk-places') && getComputedStyle(document.querySelector('#nav')).display !== 'none';
  name.remove();
  return r;
});
ok(bar.pinned, 'a pinned control stays in the bar; one without a priority moves first');
ok(bar.words === 'go to' && bar.key && bar.ks === 'Control+K Meta+K', 'a moved control\'s menu item keeps its key hint apart', JSON.stringify(bar));
ok(bar.places && bar.after, 'places in the bar: data-rk-places on the bar and the root, the strip steps aside, back when it goes');
ok(bar.nameHidden && bar.nameShown, '.rk-place-name shows only while the places are in the bar');
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

// A sheet at 375px with classic scrollbars: the whole width, not short by the reserved gutter.
{
  const n = await open({ viewport: { width: 375, height: 800 } });
  await n.evaluate(() => document.querySelector('#sheet').showModal()); await n.waitForTimeout(500);
  const r = await n.evaluate(() => { const b = document.querySelector('#sheet').getBoundingClientRect(); return { x: Math.round(b.x), w: Math.round(b.width), vw: innerWidth }; });
  ok(r.x === 0 && r.w === r.vw, 'a phone-wide sheet spans the viewport, scrollbar gutter or not', JSON.stringify(r));
  await n.close();
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

// Reduced motion: a place change is instant, never skipped.
const r = await open({ reducedMotion: 'reduce' });
await r.click('#tile-notes'); await r.waitForTimeout(150);
ok(await visible(r, 'notes') && !(await visible(r, 'home')), 'reduced motion: the place changes at once');
await setSize(r, 'w2'); await filterComp(r, 'spark'); await r.waitForTimeout(20);
ok(await r.evaluate(() => document.querySelector('#built').getAnimations().length === 0 && document.querySelector('#built').classList.contains('rk-w2') && document.querySelectorAll('#comp-body tr:not([hidden])').length === 1), 'reduced motion: a resize and a filter apply at once');
await r.close();

await browser.close();
server.close();
console.log(`${passed} checks passed`);
if (failures.length) console.log(`FAILED:\n  ${failures.join('\n  ')}`);
console.log(errors.length ? `PAGE ERRORS:\n  ${errors.join('\n  ')}` : 'no page errors');
process.exit(failures.length || errors.length ? 1 : 0);

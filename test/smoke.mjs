#!/usr/bin/env node
// Clicks through Rekta Board (_site/) in Chromium and checks the behaviour contract: routes and back, focus, the
// turnstile, the panorama's keyboard and wheel, toasts, the banner, prefs, the sheet, a phone and reduced motion.
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
function ok(cond, what, detail = '') {
  if (cond) passed += 1; else failures.push(`${what}${detail ? ` (${detail})` : ''}`);
}
async function open(opts = {}, path = '') {
  const ctx = await browser.newContext({ viewport: { width: 1440, height: 900 }, colorScheme: 'light', reducedMotion: 'no-preference', ...opts });
  const p = await ctx.newPage();
  const tag = `${opts.viewport ? opts.viewport.width : 1440}${opts.reducedMotion === 'reduce' ? '/reduced' : ''}`;
  p.on('pageerror', (e) => errors.push(`${tag}: ${e.message}`));
  p.on('console', (m) => { if (m.type() === 'error') errors.push(`${tag} console: ${m.text()}`); });
  await p.goto(base + path);
  await p.waitForTimeout(1400);
  return p;
}
const visible = (p, place) => p.evaluate((n) => !document.querySelector(`[data-rk-place="${n}"]`).hidden, place);
const focused = (p) => p.evaluate(() => document.activeElement.id || document.activeElement.className || document.activeElement.tagName);
const text = async (p, sel) => { const el = await p.$(sel); return el ? (await el.textContent()).trim() : '(none)'; };
const toast = (p) => text(p, '.rk-toast:not([data-rk-phase]) .rk-toast-msg');
const TURN = 1300;

// Rest looks: no horizontal page overflow in either theme, on a desktop and a phone.
for (const [w, h, scheme] of [[1440, 900, 'light'], [1440, 900, 'dark'], [390, 844, 'light'], [390, 844, 'dark'], [844, 390, 'light']]) {
  const p = await open({ viewport: { width: w, height: h }, colorScheme: scheme, hasTouch: w < 760 || h < 481 });
  ok(!(await p.evaluate(() => document.documentElement.scrollWidth > innerWidth + 1)), `${w}x${h} ${scheme}: no horizontal overflow`);
  await p.screenshot({ path: join(shots, `rest-${w}x${h}-${scheme}.png`) });
  await p.close();
}

// Desktop click-through.
const p = await open();
ok(await p.evaluate(() => getComputedStyle(document.querySelector('#pan')).display === 'flex'), 'desktop panorama is sideways');
ok(await p.evaluate(() => document.querySelector('#back').hidden), 'app bar back hidden at home');

// Tile → place: the turnstile, the address, the title, focus.
await p.click('#tile-import');
await p.waitForTimeout(120);
ok(await p.evaluate(() => document.querySelectorAll('[data-rk-turn]').length > 3), 'leaving units turn');
await p.screenshot({ path: join(shots, 'turn-mid.png') });
await p.waitForTimeout(TURN);
ok(await visible(p, 'import') && !(await visible(p, 'board')), 'import place shows, board hides');
ok((await p.evaluate(() => location.hash)) === '#import', 'address is #import');
ok((await p.title()) === 'Nightly import · Rekta Board', 'title names the place', await p.title());
ok((await focused(p)) === 'import-title', 'focus on the place title', await focused(p));
ok(await p.evaluate(() => !document.querySelector('[data-rk-turn], [data-rk-pressed]')), 'no turn or press state left');
await p.screenshot({ path: join(shots, 'place-import.png') });

await p.click('#retry'); await p.waitForTimeout(400);
ok((await toast(p)).startsWith('Nightly import retrying'), 'retry toast', await toast(p));
ok((await p.getAttribute('#tile-import', 'data-status')) === 'info', 'board tile follows the retry');

// The crumb goes back (the board is the place below): focus returns to the opening tile.
await p.click('[data-rk-place=import] .rk-crumb'); await p.waitForTimeout(TURN);
ok(await visible(p, 'board'), 'crumb returns to the board');
ok((await p.evaluate(() => location.hash)) === '', 'address is the board again');
ok((await focused(p)) === 'tile-import', 'focus back on the opening tile', await focused(p));
ok((await p.evaluate(() => history.state.rk.index)) === 0, 'crumb used history (index 0)');

// Browser back and forward play the turnstile both ways.
await p.click('#tile-queue'); await p.waitForTimeout(TURN);
ok(await visible(p, 'queue'), 'queue place shows');
await p.goBack(); await p.waitForTimeout(TURN);
ok(await visible(p, 'board'), 'browser back shows the board');
await p.goForward(); await p.waitForTimeout(TURN);
ok(await visible(p, 'queue'), 'browser forward shows the queue');
await p.keyboard.press('Escape'); await p.waitForTimeout(TURN);
ok(await visible(p, 'board'), 'Escape goes back');
ok((await focused(p)) === 'tile-queue', 'Escape returns focus to the tile', await focused(p));

// A navigation during a turn is never refused: the running turn finishes first.
await p.click('#tile-reviews'); await p.waitForTimeout(80);
await p.goBack(); await p.waitForTimeout(TURN + 300);
ok(await visible(p, 'board') && !(await visible(p, 'reviews')), 'back during a turn lands on the board');
ok(await p.evaluate(() => document.querySelectorAll('[data-rk-place]:not([hidden])').length === 1), 'exactly one place shows');

// Panorama: keyboard section jumps, pivot follows, focus on the section heading from the pivot.
await p.focus('#pan');
await p.keyboard.press('End'); await p.waitForTimeout(900);
ok((await p.getAttribute('#nav a[href="#look"]', 'aria-current')) === 'location', 'End jumps to the last section');
await p.keyboard.press('Home'); await p.waitForTimeout(900);
ok((await p.getAttribute('#nav a[href="#overview"]', 'aria-current')) === 'location', 'Home jumps to the first section');
await p.click('#nav a[href="#jobs"]'); await p.waitForTimeout(900);
ok((await focused(p)) === 'jobs-title', 'a pivot jump focuses the section heading', await focused(p));
ok((await p.evaluate(() => location.hash)) === '', 'a section jump changes no address');

// Wheel: a vertical wheel over the panorama moves it sideways.
await p.click('#nav a[href="#overview"]'); await p.waitForTimeout(900);
const x0 = await p.evaluate(() => document.querySelector('#pan').scrollLeft);
await p.mouse.move(900, 600); await p.mouse.wheel(0, 300); await p.waitForTimeout(300);
ok((await p.evaluate(() => document.querySelector('#pan').scrollLeft)) > x0, 'vertical wheel moves the panorama sideways');

// Jobs: the switch sets state, the row moves; archive, undo by button and by Ctrl+Z, hold, expiry.
await p.click('#nav a[href="#jobs"]'); await p.waitForTimeout(900);
await p.click('#open li:first-child .rk-switch input'); await p.waitForTimeout(500);
ok((await p.evaluate(() => [...document.querySelectorAll('#done .job-name')].map((b) => b.textContent)[0])) === 'Nightly import', 'switch moves the job to done');
await p.click('#archive'); await p.waitForTimeout(400);
ok((await toast(p)) === '3 jobs archived, 5 seconds to', 'archive toast', await toast(p));
await p.waitForTimeout(2100);
ok(['3', '2'].includes(await text(p, '.rk-toast:not([data-rk-phase]) .rk-toast-count')), 'undo window counts down');
await p.click('.rk-toast:not([data-rk-phase]) .rk-toast-act'); await p.waitForTimeout(500);
ok((await p.evaluate(() => document.querySelectorAll('#done > li').length)) === 3, 'undo restores the rows');
ok((await focused(p)) === 'archive', 'focus returns to the control that raised the toast', await focused(p));
await p.waitForTimeout(300); await p.click('#archive'); await p.waitForTimeout(300);
await p.click('#jobs-title'); await p.keyboard.press('Control+z'); await p.waitForTimeout(500);
ok((await p.evaluate(() => document.querySelectorAll('#done > li').length)) === 3, 'Ctrl+Z undoes');
await p.click('#f-name'); await p.click('#archive'); await p.waitForTimeout(300);
await p.focus('#f-name'); await p.keyboard.press('Control+z'); await p.waitForTimeout(300);
ok((await p.evaluate(() => document.querySelectorAll('#done > li').length)) === 0, 'Ctrl+Z in a text field is the field\'s');
await p.hover('.rk-toast:not([data-rk-phase])'); await p.waitForTimeout(2500);
ok(['5', '4'].includes(await text(p, '.rk-toast:not([data-rk-phase]) .rk-toast-count')), 'hover holds the window');
await p.mouse.move(5, 5); await p.waitForTimeout(5600);
ok((await p.evaluate(() => document.querySelectorAll('.rk-toast').length)) === 0, 'the toast expires');
ok(await p.evaluate(() => document.querySelector('#archive').disabled), 'archive disabled with nothing done');

// Facets: arrow keys, nothing added to history.
const len = await p.evaluate(() => history.length);
await p.click('#nav a[href="#facets"]'); await p.waitForTimeout(900);
await p.click('#tab-0'); await p.keyboard.press('ArrowRight'); await p.waitForTimeout(400);
ok((await text(p, '[role=tab][aria-selected=true]')) === 'activity', 'ArrowRight selects the next facet');
ok((await p.evaluate(() => history.length)) === len, 'facets add no history entries');

// Controls: cause and remedy, then save.
await p.click('#nav a[href="#controls"]'); await p.waitForTimeout(900);
ok((await p.getAttribute('#time-field', 'data-status')) === 'error', 'invalid time marked');
await p.fill('#f-time', '02:30'); await p.click('#save'); await p.waitForTimeout(400);
ok((await toast(p)).startsWith('Schedule saved'), 'save toast');

// Banner: halted wins over paused, the remedy steps down, the end is a toast.
await p.click('#ab-stop'); await p.waitForTimeout(500);
ok((await text(p, '.rk-banner-word')) === 'halted' && (await p.getAttribute('#banner', 'data-status')) === 'error', 'halted banner');
ok(await p.evaluate(() => !!document.querySelector('#banner [role=alert] .rk-banner-word')), 'halted speaks as an alert');
ok((await p.getAttribute('#bar', 'data-rk-state')) === 'halted', 'app bar halted');
await p.click('#ab-pause'); await p.waitForTimeout(400);
ok((await text(p, '.rk-banner-word')) === 'halted' && (await toast(p)).startsWith('Intake paused'), 'pause under halt: banner holds, toast says so');
await p.screenshot({ path: join(shots, 'halted.png') });
await p.click('#remedy'); await p.waitForTimeout(600);
ok((await text(p, '.rk-banner-word')) === 'paused' && (await toast(p)).startsWith('Emergency stop released'), 'release steps down to paused');
await p.click('#remedy'); await p.waitForTimeout(800);
ok((await p.getAttribute('#banner', 'data-rk-phase')) === 'clear' && (await toast(p)).startsWith('Intake resumed'), 'resume clears the banner');
await p.click('#nav a[href="#system"]'); await p.waitForTimeout(900);
await p.check('#s-offline'); await p.waitForTimeout(400);
ok((await text(p, '#data-now')) === 'frozen', 'offline freezes the figures');
await p.click('#remedy'); await p.waitForTimeout(1500);
ok((await p.getAttribute('#banner', 'data-rk-phase')) === 'clear', 'retry reconnects');
await p.check('#s-readonly'); await p.waitForTimeout(400);
ok((await text(p, '.rk-banner-word')) === 'read-only' && await p.evaluate(() => document.querySelector('#remedy').parentElement.hidden), 'read-only has no action');
await p.uncheck('#s-readonly'); await p.waitForTimeout(600);

// Look: prefs on the root, density, motion, accent.
await p.click('#nav a[href="#look"]'); await p.waitForTimeout(900);
await p.check('input[name=theme][value=dark]'); await p.check('input[name=accent][value=plum]'); await p.waitForTimeout(300);
ok((await p.evaluate(() => document.documentElement.dataset.theme)) === 'dark', 'theme dark on the root');
ok((await p.evaluate(() => getComputedStyle(document.documentElement).getPropertyValue('--rk-accent').trim())) === '#c38aed', 'accent slot in dark');
await p.screenshot({ path: join(shots, 'dark-plum.png') });
const btnH = () => p.evaluate(() => Math.round(document.querySelector('#reset').getBoundingClientRect().height));
ok((await btnH()) === 32, 'fine pointer: compact 32px controls', await btnH());
await p.check('input[name=density][value=comfortable]'); await p.waitForTimeout(200);
ok((await btnH()) === 44, 'comfortable: 44px controls', await btnH());
await p.check('input[name=density][value=auto]');
await p.check('input[name=motion][value=off]'); await p.waitForTimeout(200);
ok((await p.evaluate(() => getComputedStyle(document.documentElement).getPropertyValue('--rk-duration-short').trim())) === '0s', 'motion off zeroes durations');
ok((await p.evaluate(() => JSON.parse(localStorage.getItem('rekta-prefs')).motion)) === 'off', 'prefs remembered');
await p.check('input[name=motion][value=system]'); await p.check('input[name=theme][value=system]');

// Sheet: opens, a tap on the shading cancels, reset works.
await p.click('#reset'); await p.waitForTimeout(300);
ok(await p.evaluate(() => document.querySelector('#sheet').open), 'sheet opens');
ok((await p.evaluate(() => getComputedStyle(document.documentElement).overflow)) === 'hidden', 'page behind the sheet does not scroll');
await p.mouse.click(700, 850); await p.waitForTimeout(400);
ok(!(await p.evaluate(() => document.querySelector('#sheet').open)), 'a tap on the shading cancels');
await p.click('#reset'); await p.waitForTimeout(300);
await p.click('#sheet button[value=reset]'); await p.waitForTimeout(600);
ok((await p.evaluate(() => document.querySelectorAll('#open > li').length)) === 3 && (await p.inputValue('#f-time')) === '25:00', 'reset restores the demo');

// Reviews: a row action, the tile follows, undo.
await p.click('#nav a[href="#overview"]'); await p.waitForTimeout(900);
await p.click('#tile-reviews'); await p.waitForTimeout(TURN);
await p.click('#review-list li:first-child [data-approve]'); await p.waitForTimeout(300);
ok((await text(p, '#reviews-open')) === '2', 'approving updates the board tile');
ok((await focused(p)) === 'rk-btn', 'focus moves to the next row\'s action', await focused(p));
await p.keyboard.press('Control+z'); await p.waitForTimeout(400);
ok((await text(p, '#reviews-open')) === '3', 'Ctrl+Z restores the review');
await p.close();

// Deep link: a cold entry puts the parent underneath, so back stays in the app.
const d = await open({}, '#reviews');
ok(await visible(d, 'reviews'), 'deep link opens the place');
ok((await d.evaluate(() => history.state.rk.index)) === 1, 'the parent is underneath');
await d.keyboard.press('Escape'); await d.waitForTimeout(TURN);
ok(await visible(d, 'board') && (await d.evaluate(() => location.hash)) === '', 'back from a deep link lands on the board');
await d.close();

// Phone: sticky strip, toasts from the app bar, the app bar's back, touch press.
const m = await open({ viewport: { width: 390, height: 844 }, hasTouch: true, isMobile: true });
ok((await m.evaluate(() => getComputedStyle(document.querySelector('#pan')).display)) !== 'flex', 'phone panorama stacks');
await m.evaluate(() => scrollTo(0, 1600)); await m.waitForTimeout(300);
ok((await m.evaluate(() => Math.round(document.querySelector('#nav').getBoundingClientRect().top))) === 0, 'section strip sticks to the top');
ok((await m.evaluate(() => parseFloat(document.documentElement.style.getPropertyValue('--rk-pivot-h')))) > 40, 'strip height published');
await m.tap('#nav a[href="#system"]'); await m.waitForTimeout(900);
const sysTop = await m.evaluate(() => document.querySelector('#system').getBoundingClientRect().top);
const navH = await m.evaluate(() => document.querySelector('#nav').getBoundingClientRect().bottom);
ok(sysTop >= navH - 2, 'a section jump lands below the strip', `${sysTop} vs ${navH}`);
await m.tap('#ab-stop'); await m.waitForTimeout(500);
await m.tap('#event'); await m.waitForTimeout(400);
const tb = await m.evaluate(() => ({ toast: document.querySelector('.rk-toast:not([data-rk-phase])').getBoundingClientRect().bottom, bar: document.querySelector('#bar').getBoundingClientRect().top }));
ok(tb.toast <= tb.bar, 'phone toast rises from the app bar', JSON.stringify(tb));
await m.screenshot({ path: join(shots, 'phone-halted.png') });
await m.tap('#ab-stop'); await m.waitForTimeout(600);
await m.evaluate(() => scrollTo(0, 0)); await m.waitForTimeout(200);
await m.tap('#tile-import'); await m.waitForTimeout(TURN);
ok(await visible(m, 'import'), 'phone: tap opens the place');
ok(!(await m.evaluate(() => document.querySelector('#back').hidden)), 'phone: app bar back shows');
await m.screenshot({ path: join(shots, 'phone-place.png') });
await m.tap('#back'); await m.waitForTimeout(TURN);
ok(await visible(m, 'board'), 'phone: app bar back returns');
ok((await m.evaluate(() => Math.round(document.querySelector('#reset').getBoundingClientRect().height))) === 44, 'touch: 44px controls');
await m.close();

// Reduced motion: a place change is instant, never skipped.
const r = await open({ reducedMotion: 'reduce' });
await r.click('#tile-import'); await r.waitForTimeout(150);
ok(await visible(r, 'import') && !(await visible(r, 'board')), 'reduced motion: the place changes at once');
await r.close();

await browser.close();
server.close();
console.log(`${passed} checks passed`);
if (failures.length) console.log(`FAILED:\n  ${failures.join('\n  ')}`);
console.log(errors.length ? `PAGE ERRORS:\n  ${errors.join('\n  ')}` : 'no page errors');
process.exit(failures.length || errors.length ? 1 : 0);

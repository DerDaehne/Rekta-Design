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
ok(await p.evaluate(() => document.querySelectorAll('#running .rk-ghost').length > 0), 'queue rows load behind still placeholders');
await p.waitForTimeout(1400);
ok(await p.evaluate(() => !document.querySelector('#queue-region').hasAttribute('aria-busy') && document.querySelectorAll('#running b').length === 3), 'queue rows arrive and the line goes');
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

// Menu: the keyboard, the invoker, a choice; refresh shows the loading line after its delay, never at once.
await p.focus('#jobs-more'); await p.keyboard.press('Enter'); await p.waitForTimeout(300);
const act = () => p.evaluate(() => document.activeElement.dataset.act || document.activeElement.id);
ok(await p.evaluate(() => document.querySelector('#jobs-menu').matches(':popover-open')) && (await p.getAttribute('#jobs-more', 'aria-expanded')) === 'true', 'Enter opens the menu');
ok((await act()) === 'refresh', 'the first item takes focus', await act());
await p.keyboard.press('ArrowDown');
ok((await act()) === 'show-done', '↓ moves down');
await p.keyboard.press('ArrowUp'); await p.keyboard.press('ArrowUp');
ok((await act()) === 'print', '↑ wraps to the end');
await p.keyboard.press('e');
ok((await act()) === 'export', 'a letter jumps to its item');
await p.keyboard.press('Escape'); await p.waitForTimeout(200);
ok(!(await p.evaluate(() => document.querySelector('#jobs-menu').matches(':popover-open'))) && (await act()) === 'jobs-more', 'Escape closes and returns focus');
await p.click('#jobs-more'); await p.waitForTimeout(200); await p.click('#jobs-more'); await p.waitForTimeout(200);
ok(!(await p.evaluate(() => document.querySelector('#jobs-menu').matches(':popover-open'))), 'a second click on the trigger closes the menu');
await p.click('#jobs-more'); await p.waitForTimeout(200);
const menuBox = await p.evaluate(() => { const m = document.querySelector('#jobs-menu').getBoundingClientRect(), t = document.querySelector('#jobs-more').getBoundingClientRect(); return { top: m.top, below: t.bottom, inView: m.right <= innerWidth && m.left >= 0 }; });
ok(menuBox.top >= menuBox.below && menuBox.inView, 'the menu sits below its trigger, inside the window', JSON.stringify(menuBox));
await p.click('#jobs-menu [data-act=refresh]'); await p.waitForTimeout(100);
const line = () => p.evaluate(() => getComputedStyle(document.querySelector('#jobs-region'), '::before').visibility);
ok((await p.getAttribute('#jobs-region', 'aria-busy')) === 'true' && (await line()) === 'hidden', 'loading: no line before the delay');
await p.waitForTimeout(400);
ok((await line()) === 'visible', 'loading: the line after the delay');
await p.waitForTimeout(1300);
ok(!(await p.evaluate(() => document.querySelector('#jobs-region').hasAttribute('aria-busy'))) && (await toast(p)) === 'Jobs refreshed.', 'refresh ends');
await p.click('#jobs-more'); await p.waitForTimeout(200); await p.click('#jobs-menu [data-act=show-done]'); await p.waitForTimeout(200);
ok(await p.evaluate(() => document.querySelector('#done').hidden), 'a checkbox item toggles');
ok(await p.evaluate(() => document.querySelector('#jobs-menu').matches(':popover-open')), 'a checkbox item leaves the menu open');
ok((await p.getAttribute('#jobs-menu [data-act=show-done]', 'aria-checked')) === 'false', 'the checkbox item shows its state');
await p.click('#jobs-menu [data-act=show-done]'); await p.waitForTimeout(200);
await p.keyboard.press('Escape'); await p.waitForTimeout(200);

// Facets: arrow keys, nothing added to history.
const len = await p.evaluate(() => history.length);
await p.click('#nav a[href="#facets"]'); await p.waitForTimeout(900);
await p.click('#tab-0'); await p.keyboard.press('ArrowRight'); await p.waitForTimeout(400);
ok((await text(p, '[role=tab][aria-selected=true]')) === 'activity', 'ArrowRight selects the next facet');
ok((await p.evaluate(() => history.length)) === len, 'facets add no history entries');

// Controls: cause and remedy, then save.
await p.click('#nav a[href="#controls"]'); await p.waitForTimeout(900);
ok((await p.getAttribute('#time-field', 'data-status')) === 'error', 'invalid time marked');
await p.fill('#f-time', '02:30'); await p.click('#save'); await p.waitForTimeout(100);
ok((await text(p, '#save')) === 'saving schedule…' && (await p.getAttribute('#save', 'aria-disabled')) === 'true', 'pending: the button says what it does');
await p.waitForTimeout(1200);
ok((await toast(p)).startsWith('Schedule saved') && (await text(p, '#save')) === 'save schedule', 'save toast, label back');

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
await p.click('#jobs-more'); await p.waitForTimeout(200); await p.click('#jobs-menu [data-act=refresh]'); await p.waitForTimeout(1900);
ok((await p.getAttribute('#jobs-error', 'data-status')) === 'error', 'a failed load says so in the region');
await p.click('#remedy'); await p.waitForTimeout(1500);
ok((await p.getAttribute('#banner', 'data-rk-phase')) === 'clear', 'retry reconnects');
await p.click('#jobs-retry'); await p.waitForTimeout(1900);
ok(!(await p.$('#jobs-error')), 'retry in the region loads again');
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

// Runs: a sortable, selectable table in a split; the current run is part of the address.
await p.evaluate(() => window.board.router.navigate('runs')); await p.waitForTimeout(TURN);
const runIds = () => p.evaluate(() => [...document.querySelectorAll('#runs-body tr')].map((tr) => tr.dataset.run));
ok((await runIds()).length === 12, 'runs table has 12 rows');
await p.click('#runs-table thead th:nth-child(5) .rk-sort'); await p.waitForTimeout(500);
ok((await p.getAttribute('#runs-table thead th:nth-child(5)', 'aria-sort')) === 'ascending' && (await runIds())[0] === '4184', 'sort ascending by duration');
ok((await p.getAttribute('#runs-table thead th:nth-child(6)', 'aria-sort')) === 'none', 'the other column lets go of its sort');
await p.click('#runs-table thead th:nth-child(5) .rk-sort'); await p.waitForTimeout(500);
ok((await runIds())[0] === '4178', 'sort descending by duration');
await p.click('#runs-body tr:nth-child(1) .rk-check input');
await p.click('#runs-body tr:nth-child(4) .rk-check input', { modifiers: ['Shift'] }); await p.waitForTimeout(200);
ok((await p.evaluate(() => document.querySelectorAll('#runs-body tr[data-rk-selected]').length)) === 4, 'Shift+click selects a range');
ok(await p.evaluate(() => document.querySelector('#bar').hasAttribute('data-rk-selecting')) && (await text(p, '.rk-ab-count')) === '4 selected', 'the app bar carries the selection');
ok(await p.evaluate(() => document.querySelector('#runs-table thead .rk-check input').indeterminate), 'the head checkbox shows a partial selection');
await p.keyboard.press('Escape'); await p.waitForTimeout(200);
ok((await p.evaluate(() => document.querySelectorAll('#runs-body tr[data-rk-selected]').length)) === 0 && await p.evaluate(() => !document.querySelector('#bar').hasAttribute('data-rk-selecting')), 'Escape clears the selection, not the place');
ok(await visible(p, 'runs'), 'Escape with a selection stays in the place');
await p.focus('#runs-body tr:nth-child(2) .rk-check input'); await p.keyboard.press('Control+a'); await p.waitForTimeout(200);
ok((await p.evaluate(() => document.querySelectorAll('#runs-body tr[data-rk-selected]').length)) === 12, 'Ctrl+A selects all');
await p.click('[data-rk-clear]'); await p.waitForTimeout(200);
await p.click('#runs-body tr:nth-child(2) .rk-check input'); await p.click('#runs-body tr:nth-child(3) .rk-check input');
await p.click('#sel-archive'); await p.waitForTimeout(300);
ok((await runIds()).length === 10 && (await toast(p)).startsWith('2 runs archived'), 'archive the selection, with undo');
await p.keyboard.press('Control+z'); await p.waitForTimeout(600);
ok((await runIds()).length === 12 && (await runIds())[0] === '4178', 'undo puts them back in sort order');
const hist = await p.evaluate(() => history.length);
await p.click('#runs-body tr:nth-child(3) th a'); await p.waitForTimeout(500);
const third = (await runIds())[2];
ok((await p.evaluate(() => location.hash)) === `#runs/${third}`, 'a run has an address', await p.evaluate(() => location.hash));
ok((await p.evaluate(() => history.length)) === hist, 'sideways, choosing a run replaces the address');
ok((await p.getAttribute(`#runs-body tr[data-run="${third}"]`, 'aria-current')) === 'true' && (await text(p, '#run-title')) === `Run #${third}`, 'the current run shows beside the list');
ok((await p.title()).startsWith(`Run #${third} · `), 'the title names the run');
const w0 = await p.evaluate(() => document.querySelector('#runs-list').getBoundingClientRect().width);
await p.focus('.rk-split-handle'); await p.keyboard.press('ArrowRight'); await p.waitForTimeout(100);
const w1 = await p.evaluate(() => document.querySelector('#runs-list').getBoundingClientRect().width);
ok(Math.round(w1 - w0) === 16, 'the divider moves with the keyboard', `${w0} → ${w1}`);
const hb = await p.evaluate(() => { const r = document.querySelector('.rk-split-handle').getBoundingClientRect(); return { x: r.left + r.width / 2, y: r.top + 200 }; });
await p.mouse.move(hb.x, hb.y); await p.mouse.down(); await p.mouse.move(hb.x + 100, hb.y, { steps: 5 }); await p.mouse.up(); await p.waitForTimeout(100);
const w2 = await p.evaluate(() => document.querySelector('#runs-list').getBoundingClientRect().width);
ok(Math.abs(w2 - w1 - 100) <= 2, 'the divider drags', `${w1} → ${w2}`);

// Search and filters: the trigger says what it keeps, rows hide, the address keeps it, clear brings everything back.
const shownRuns = () => p.evaluate(() => [...document.querySelectorAll('#runs-body tr')].filter((tr) => !tr.hidden).length);
await p.click('#f-status'); await p.waitForTimeout(200);
await p.click('#f-status-menu [data-v=error]'); await p.click('#f-status-menu [data-v=warning]'); await p.waitForTimeout(200);
ok(await p.evaluate(() => document.querySelector('#f-status-menu').matches(':popover-open')), 'a filter menu stays open for several choices');
ok((await text(p, '#f-status')) === 'status: failed, waiting', 'the filter button says what it keeps', await text(p, '#f-status'));
ok((await shownRuns()) === 2, 'the filter hides the other rows');
ok((await p.evaluate(() => location.hash)).includes('status=failed%2Cwaiting'), 'the address keeps the filter', await p.evaluate(() => location.hash));
await p.keyboard.press('Escape'); await p.waitForTimeout(200);
await p.click('#f-status'); await p.waitForTimeout(200); await p.click('#f-status-menu [data-v=warning]'); await p.click('#f-status-menu [data-v=error]'); await p.keyboard.press('Escape');
await p.fill('#runs-q', 'mobile'); await p.waitForTimeout(400);
ok((await shownRuns()) === 3 && (await text(p, '#runs-facts')).endsWith('showing 3') && (await p.evaluate(() => location.hash)).includes('q=mobile'), 'search narrows the rows and stays in the address');
await p.fill('#runs-q', 'nothing like this'); await p.waitForTimeout(400);
ok(!(await p.evaluate(() => document.querySelector('#runs-none').hidden)), 'no match says so in the region');
await p.click('#f-clear'); await p.waitForTimeout(300);
ok((await shownRuns()) === 12 && !(await p.evaluate(() => location.hash)).includes('?') && (await text(p, '#f-status')) === 'status: any', 'clear filters brings everything back');

// Go to: the palette rises from the app bar; Ctrl+K, typing, ↓, Enter; Escape clears, then closes.
await p.keyboard.press('Control+k'); await p.waitForTimeout(300);
ok(await p.evaluate(() => document.querySelector('#palette').matches(':popover-open') && document.activeElement === document.querySelector('#palette input')), 'Ctrl+K opens the palette with focus in it');
const pb = await p.evaluate(() => ({ pal: document.querySelector('#palette').getBoundingClientRect().bottom, bar: document.querySelector('#bar').getBoundingClientRect().top }));
ok(pb.pal <= pb.bar, 'the palette rises from the app bar', JSON.stringify(pb));
await p.keyboard.type('queue'); await p.waitForTimeout(200);
const activeOpt = () => p.evaluate(() => { const id = document.activeElement.getAttribute('aria-activedescendant'); return id ? document.getElementById(id).textContent : null; });
ok((await activeOpt()) === 'Build queue', 'typing filters, the best match is active', await activeOpt());
await p.keyboard.press('Escape'); await p.waitForTimeout(100);
ok((await p.inputValue('#palette input')) === '' && await p.evaluate(() => document.querySelector('#palette').matches(':popover-open')), 'Escape clears the text first');
await p.keyboard.type('review'); await p.keyboard.press('Enter'); await p.waitForTimeout(TURN);
ok(await visible(p, 'reviews') && !(await p.evaluate(() => document.querySelector('#palette').matches(':popover-open'))), 'Enter goes to the place');
await p.click('#ab-goto'); await p.waitForTimeout(300); await p.click('#ab-goto'); await p.waitForTimeout(300);
ok(!(await p.evaluate(() => document.querySelector('#palette').matches(':popover-open'))), 'a second click on go to closes it');
await p.keyboard.press('Control+k'); await p.waitForTimeout(200); await p.keyboard.press('Escape'); await p.waitForTimeout(200);
ok(!(await p.evaluate(() => document.querySelector('#palette').matches(':popover-open'))), 'Escape on empty text closes the palette');
await p.locator('#reviews-title').focus(); await p.keyboard.press('?'); await p.waitForTimeout(300);
ok(await p.evaluate(() => document.querySelector('#keys-sheet').open), '? opens the keyboard shortcuts');
await p.keyboard.press('Escape'); await p.waitForTimeout(300);
ok(!(await p.evaluate(() => document.querySelector('#keys-sheet').open)) && await visible(p, 'reviews'), 'Escape closes the sheet, not the place');
await p.close();

// Deep link: a cold entry puts the parent underneath, so back stays in the app.
const d = await open({}, '#reviews');
ok(await visible(d, 'reviews'), 'deep link opens the place');
ok((await d.evaluate(() => history.state.rk.index)) === 1, 'the parent is underneath');
await d.keyboard.press('Escape'); await d.waitForTimeout(TURN);
ok(await visible(d, 'board') && (await d.evaluate(() => location.hash)) === '', 'back from a deep link lands on the board');
await d.close();
const d2 = await open({}, '#runs/4179');
ok((await text(d2, '#run-title')) === 'Run #4179' && (await d2.evaluate(() => history.state.rk.index)) === 2, 'a deep link to a run puts the place and its list underneath');
await d2.keyboard.press('Escape'); await d2.waitForTimeout(600);
ok((await d2.evaluate(() => location.hash)) === '#runs' && await visible(d2, 'runs'), 'back from a deep-linked run shows the list');
await d2.close();
const d3 = await open({}, '#runs?status=failed');
ok((await d3.evaluate(() => [...document.querySelectorAll('#runs-body tr')].filter((tr) => !tr.hidden).length)) === 1 && (await text(d3, '#f-status')) === 'status: failed', 'a filtered address opens filtered');
await d3.close();

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
ok(await m.evaluate(() => document.querySelector('#ab-theme').hasAttribute('data-rk-overflow') && !document.querySelector('#ab-more').hidden), 'phone: what does not fit moves into more');
await m.tap('#ab-more'); await m.waitForTimeout(300);
const moreItems = await m.evaluate(() => [...document.querySelectorAll('.rk-ab-menu .rk-menu-item')].map((b) => b.textContent));
ok(moreItems.includes('motion on'), 'phone: more repeats the moved controls\' words', moreItems.join(' | '));
await m.tap('.rk-ab-menu .rk-menu-item:nth-child(2)'); await m.waitForTimeout(300);
ok((await m.evaluate(() => document.documentElement.dataset.motion)) === 'off', 'phone: a control in more still works');
await m.evaluate(() => window.board.router.navigate('runs')); await m.waitForTimeout(TURN);
ok((await m.evaluate(() => getComputedStyle(document.querySelector('#runs-body tr')).display)) === 'flex', 'phone: table rows stack');
ok((await m.evaluate(() => getComputedStyle(document.querySelector('#runs-body td:nth-child(4)'), '::before').content)) === '"target "', 'phone: cells carry their column\'s label');
await m.tap('#runs-body tr:nth-child(6) th a'); await m.waitForTimeout(TURN);
ok(await m.evaluate(() => document.querySelector('#runs-list').hidden && !document.querySelector('#runs-detail').hidden), 'phone: the run replaces the list');
ok((await m.evaluate(() => document.activeElement.id)) === 'run-title', 'phone: focus on the run');
await m.goBack(); await m.waitForTimeout(TURN);
ok(await m.evaluate(() => !document.querySelector('#runs-list').hidden && document.querySelector('#runs-detail').hidden) && (await m.evaluate(() => location.hash)) === '#runs', 'phone: back returns to the list');
ok((await m.evaluate(() => document.activeElement.textContent)) === '#4179', 'phone: focus back on the run it opened', await m.evaluate(() => document.activeElement.textContent));
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

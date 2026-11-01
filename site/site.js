/* Rekta Design, the showcase. Every part of the page is a live example of the system on rekta.js (window.Rekta);
   this file only feeds it data and wires the examples together. The data is the system itself where it can be: its
   tokens, its components, and what this page measures about itself. */
(() => {
  'use strict';
  const { prefs, tokens: tok } = Rekta;
  const $ = (s, el = document) => el.querySelector(s);
  const $$ = (s, el = document) => [...el.querySelectorAll(s)];
  const root = document.documentElement;
  const wait = (ms) => new Promise((r) => setTimeout(r, ms));
  const store = {
    get: (k) => { try { return JSON.parse(localStorage.getItem(k) || 'null'); } catch { return null; } },
    set: (k, v) => { try { if (v == null) localStorage.removeItem(k); else localStorage.setItem(k, JSON.stringify(v)); } catch { /* per visit only */ } },
  };
  const TOKENS = JSON.parse($('#tokens-data').textContent);
  const WORD = { info: 'active', warning: 'waiting', error: 'failed', success: 'done' };

  // ?lang=de: every word rekta.js shows or says, and its numbers, in German (the page's own text stays English).
  const lang = new URLSearchParams(location.search).get('lang');
  const app = lang === 'de' && window.RektaStrings ? Rekta.start({ locale: 'de', strings: RektaStrings.de }) : Rekta.start();
  const router = Rekta.places({ home: 'home', title: 'Rekta Design' });
  const home = Rekta.panorama($('#pan'), { pivot: $('#nav') });
  const facets = Rekta.pivot($('#facet-strip'));
  const toasts = Rekta.toaster($('#toasts'));
  const hist = Rekta.undoStack({ toaster: toasts });

  /* ---------- colour: contrast measured live, in the theme on screen ---------- */
  const probe = document.createElement('i');
  probe.style.cssText = 'position:absolute;width:0;height:0;overflow:hidden';
  $('#app').appendChild(probe);
  function rgb(name) {
    probe.style.color = `var(--rk-${name})`;
    const m = getComputedStyle(probe).color.match(/[\d.]+/g) || [0, 0, 0];
    return m.slice(0, 3).map(Number);
  }
  const lum = ([r, g, b]) => [r, g, b].map((v) => { v /= 255; return v <= 0.03928 ? v / 12.92 : ((v + 0.055) / 1.055) ** 2.4; }).reduce((a, v, i) => a + v * [0.2126, 0.7152, 0.0722][i], 0);
  const ratio = (a, b) => { const x = lum(a), y = lum(b); return (Math.max(x, y) + 0.05) / (Math.min(x, y) + 0.05); };
  function measure() {
    for (const b of $$('[data-ratio]')) {
      const [fg, bg] = b.dataset.ratio.split(' '), r = ratio(rgb(fg), rgb(bg));
      b.textContent = `${r.toFixed(1)} : 1`;
      if (b.dataset.min) b.closest('tr').querySelector('th').title = r >= +b.dataset.min ? 'passes' : 'fails';
    }
  }

  /* ---------- the environment, read out live ---------- */
  const reduced = matchMedia('(prefers-reduced-motion: reduce)');
  function env() {
    const h = Math.round(tok.px('--rk-control-height'));
    $('#env-density').textContent = `${prefs.get('density') === 'auto' ? 'by pointer' : prefs.get('density')} · ${h}px controls`;
    const dark = root.dataset.theme === 'dark' || (root.dataset.theme !== 'light' && matchMedia('(prefers-color-scheme: dark)').matches);
    $('#env-theme').textContent = `${prefs.get('theme')} · ${dark ? 'dark' : 'light'}`;
    const m = prefs.get('motion');
    $('#env-motion').textContent = m === 'system' ? (reduced.matches ? 'system · reduced' : 'system · on') : m;
    $('#env-class').textContent = `${getComputedStyle($('#pan')).display === 'flex' ? 'sideways' : 'compact'} · ${innerWidth} × ${innerHeight}`;
  }
  addEventListener('resize', env);

  /* ---------- look: theme, density, motion, contrast, transparency (Rekta prefs) and the accent slot ---------- */
  const PREFS = ['theme', 'density', 'motion', 'contrast', 'transparency', 'accent'];
  function paintPrefs() {
    for (const name of PREFS) { const r = $(`input[name=${name}][value="${prefs.get(name)}"]`); if (r) r.checked = true; }
    $('#theme-word').textContent = prefs.get('theme');
    const m = prefs.get('motion');
    $('#motion-word').textContent = m === 'system' ? (reduced.matches ? 'reduced' : 'on') : m;
    $('#m-off').checked = m === 'off' || (m === 'system' && reduced.matches);
    measure(); env();
  }
  document.addEventListener('rk:prefs', paintPrefs);
  reduced.addEventListener('change', paintPrefs);
  matchMedia('(prefers-color-scheme: dark)').addEventListener('change', paintPrefs);
  matchMedia('(prefers-contrast: more)').addEventListener('change', paintPrefs);
  // The accent is a pref too: a named accent of Rekta's, or "custom", the visitor's own, styled by site-head.js.
  for (const name of PREFS) $$(`input[name=${name}]`).forEach((r) => r.addEventListener('change', () => prefs.set(name, r.value)));
  const THEMES = ['system', 'light', 'dark'];
  $('#ab-theme').addEventListener('click', () => prefs.set('theme', THEMES[(THEMES.indexOf(prefs.get('theme')) + 1) % 3]));
  $('#ab-motion').addEventListener('click', () => prefs.set('motion', prefs.get('motion') === 'off' ? 'system' : 'off'));
  $('#m-off').addEventListener('change', (e) => prefs.set('motion', e.target.checked ? 'off' : 'system'));

  /* ---------- the banner: lasting states, the most severe first ---------- */
  const STATES = {
    halted: { word: 'halted', status: 'error', alert: true, action: 'release stop', text: 'Everything live has stopped: an emergency stop holds every update. Release the stop to go on.', on: 'Stopped everything', off: 'Stop released. Live data goes on.' },
    offline: { word: 'offline', status: 'warning', action: 'retry now', text: 'This page lost its connection. Figures are frozen; it retries every 10 seconds.', on: 'Connection lost', off: 'Connected again. Figures are live.' },
    paused: { word: 'paused', status: 'warning', action: 'resume', text: 'You paused the live data. Figures hold still until you resume.', on: 'Live data paused', off: 'Live data resumed.' },
    readonly: { word: 'read-only', since: 'until', until: '15:00', text: 'Maintenance: everything stays visible; changes open again when it ends.', on: 'Maintenance started', off: 'Maintenance finished. Changes are open again.' },
  };
  const ORDER = ['halted', 'offline', 'paused', 'readonly'];
  const remedy = $('#remedy');
  let retrying = 0;
  const banner = Rekta.banner($('#banner'), {
    states: STATES, order: ORDER, appbar: $('#bar'), liveWord: $('#live'), toaster: toasts,
    onAction(name) {
      if (name === 'offline') { remedy.textContent = 'retrying…'; remedy.setAttribute('aria-disabled', 'true'); retrying = setTimeout(() => setState('offline', false), 900); return; }
      setState(name, false, name === 'paused' ? $('#ab-pause') : null);
    },
  });
  const live = () => !(banner.is('halted') || banner.is('offline') || banner.is('paused'));
  function paintState() {
    $('#ab-pause span').textContent = banner.is('paused') ? 'resume' : 'pause';
    ORDER.forEach((n) => { $(`#s-${n}`).checked = banner.is(n); });
    $('#state-now').textContent = banner.shown ? STATES[banner.shown].word : 'none';
    $('#data-now').textContent = live() ? 'updating' : 'frozen';
  }
  function setState(name, on, from) { clearTimeout(retrying); banner.set(name, on, { from }); paintState(); }
  ORDER.forEach((n) => $(`#s-${n}`).addEventListener('change', (e) => setState(n, e.target.checked, e.target)));
  $('#ab-pause').addEventListener('click', (e) => setState('paused', !banner.is('paused'), e.currentTarget));

  /* ---------- live tiles: what this page measures about itself ---------- */
  const series = { fps: [], moves: [] };
  let frames = 0, moves = 0, presses = 0, seconds = 0;
  (function count() { frames += 1; requestAnimationFrame(count); }());
  addEventListener('pointermove', () => { moves += 1; }, { passive: true });
  document.addEventListener('pointerdown', (e) => { if (e.target.closest && e.target.closest('a.rk-tile, button.rk-tile')) { presses += 1; $('#presses').textContent = presses; } });
  document.addEventListener('keydown', (e) => { if (e.key === 'Enter' && e.target.matches && e.target.matches('a.rk-tile, button.rk-tile')) { presses += 1; $('#presses').textContent = presses; } });
  // A sparkline's points in its 200 x 28 box (docs/data.md): inset by half the 2px stroke, a pixel on the screen, on
  // every side; no values draw nothing, one value or a constant series lies flat along the bottom.
  function points(h, svg) {
    if (!h.length) return '';
    const k = 200 / (svg.getBoundingClientRect().width || 200), lo = Math.min(...h), span = Math.max(...h) - lo || 1;
    const at = (v, x) => `${x.toFixed(1)},${(28 - k - (v - lo) / span * (28 - 2 * k)).toFixed(1)}`;
    return h.length === 1 ? `${at(h[0], k)} ${at(h[0], 200 - k)}` : h.map((v, i) => at(v, k + i * (200 - 2 * k) / (h.length - 1))).join(' ');
  }
  function drawLive(key, value, label) {
    for (const tile of $$(`[data-live=${key}]`)) {
      $('[data-v]', tile).textContent = value;
      const svg = $('.rk-spark', tile), h = series[key];
      Rekta.sparkTo($('polyline', svg), points(h, svg));
      const trend = h.length > 1 ? (h[h.length - 1] > h[0] ? 'rising' : h[h.length - 1] < h[0] ? 'falling' : 'flat') : 'flat';
      svg.setAttribute('aria-label', `${label}: ${value}, ${trend} over the last ${h.length} seconds`);
    }
  }
  setInterval(() => {
    seconds += 1;
    $('#time-here').textContent = seconds < 60 ? `${seconds} s` : `${Math.floor(seconds / 60)} min`;
    if (live()) {
      for (const [k, v] of [['fps', frames], ['moves', moves]]) { series[k].push(v); if (series[k].length > 24) series[k].shift(); }
      drawLive('fps', frames, 'frames a second');
      drawLive('moves', moves, 'pointer moves a second');
    }
    frames = 0; moves = 0;
  }, 1000);
  // Sections seen: each section the panorama shows counts; all seen, the tile is done and holds still.
  const SECTIONS = $$('#pan > .rk-sec').map((s) => s.id), seen = new Set(['overview']);
  $('#seen-of').textContent = SECTIONS.length;
  $('#seen-progress').max = SECTIONS.length;
  function paintSeen() {
    const n = seen.size, tile = $('#tile-seen');
    $('#seen-n').textContent = n;
    $('#seen-progress').value = n;
    if (n >= SECTIONS.length) { tile.dataset.status = 'success'; $('.rk-tile-word', tile).textContent = 'done'; $('#seen-msg').textContent = 'You have seen every section. Stillness means done.'; }
    else { tile.dataset.status = 'info'; $('.rk-tile-word', tile).textContent = 'active'; }
  }
  const seenWatch = () => {
    const c = home.current;
    if (!c || seen.has(c.id)) return;
    seen.add(c.id);
    Rekta.tileUpdate($('#tile-seen'), paintSeen, { kind: seen.size >= SECTIONS.length ? 'flip' : 'peek' });
  };
  $('#pan').addEventListener('scroll', seenWatch, { passive: true });
  addEventListener('scroll', seenWatch, { passive: true });
  $('#nav').addEventListener('click', () => setTimeout(seenWatch, 50));
  // Pin and unpin: a tile comes into the grid after the first one, the others make room; unpinned, they close up.
  const liveTiles = $('#live-tiles');
  let pins = 0;
  const paintPins = () => {
    const n = $$('[data-pinned]', liveTiles).length;
    $('#tile-unpin').setAttribute('aria-disabled', String(!n));
    $('#tile-unpin').dataset.rkTip = n ? 'unpin the last pinned tile' : 'nothing pinned yet';
  };
  $('#tile-pin').addEventListener('click', () => {
    pins += 1;
    const t = document.createElement('div');
    t.className = 'rk-tile'; t.dataset.pinned = '';
    t.innerHTML = `<span class="rk-tile-word">idle</span><span class="rk-stat"><b>${pins}</b><span>pinned tile</span></span>`;
    Rekta.reflow(liveTiles, () => $('#tile-seen').after(t), { size: false });
    Rekta.enter(t);
    paintPins();
  });
  $('#tile-unpin').addEventListener('click', async () => {
    const t = $('[data-pinned]', liveTiles);
    if (!t) return;
    t.removeAttribute('data-pinned'); paintPins();
    const a = await Rekta.leave(t);
    Rekta.reflow(liveTiles, () => { t.remove(); if (a) a.cancel(); }, { size: false, items: () => [...liveTiles.children].filter((x) => x !== t) });
  });
  $('#token-count').textContent = TOKENS.length;
  $('#tile-presses').addEventListener('click', () => {
    $('#press-text').textContent = `You have pressed ${presses === 1 ? '1 tile' : `${presses} tiles`} on this page. A button tile opens a sheet like this one; it never changes data itself.`;
    $('#press-sheet').showModal();
  });

  /* ---------- motion: reorder, sheet, toast, build-in ---------- */
  function flipMove(list, change) {
    const items = $$(':scope > li', list), before = new Map(items.map((li) => [li, li.getBoundingClientRect()]));
    change();
    const d = tok.ms('--rk-duration-moderate');
    if (!d) return;
    for (const li of items) {
      const a = before.get(li), b = li.getBoundingClientRect();
      if (a.top !== b.top) li.animate([{ transform: `translateY(${a.top - b.top}px)` }, { transform: 'none' }], { duration: d, easing: tok.value('--rk-ease-base') });
    }
  }
  $('#flip').addEventListener('change', (e) => {
    const li = e.target.closest('li'), list = $('#flip');
    flipMove(list, () => { if (e.target.checked) list.append(li); else list.prepend(li); });
  });
  $('#m-sheet').addEventListener('click', () => $('#sheet').showModal());
  $('#m-toast').addEventListener('click', () => toasts.show('Something happened, and this toast says so once.', { key: 'motion' }));
  $('#m-replay').addEventListener('click', () => {
    const place = $('[data-rk-place=home]');
    place.removeAttribute('data-rk-arriving'); void place.offsetWidth;
    place.setAttribute('data-rk-arriving', '');
    setTimeout(() => place.removeAttribute('data-rk-arriving'), 3 * tok.ms('--rk-duration-greet') + 2 * tok.ms('--rk-duration-long') + 50);
  });

  /* ---------- feedback ---------- */
  const EVENTS = ['Build 0.2.1 queued behind 2 runs', 'A comment arrived on the colour section', 'The tokens were checked: every pair passes'];
  let ev = 0;
  $('#f-event').addEventListener('click', () => { toasts.show(EVENTS[ev % EVENTS.length], { key: `event-${ev}` }); ev += 1; });
  $('#f-undo').addEventListener('click', (e) => toasts.show('One thing archived', { key: 'archive', from: e.currentTarget, undo: () => toasts.show('It is back.', { key: 'archive-back' }) }));
  $('#f-pending').addEventListener('click', (e) => Rekta.pending(e.currentTarget, wait(1500)).then(() => toasts.show('Saved.', { key: 'saved' })));

  /* ---------- lists & tables: the components, selected the Metro way ---------- */
  const COMPONENTS = [
    ['Tile', 'tiles', 'success', '0.1'], ['Sparkline', 'tiles', 'success', '0.1'], ['Tag', 'status', 'success', '0.1'], ['Rows', 'content', 'success', '0.1'],
    ['Button', 'actions', 'success', '0.1'], ['Field', 'forms', 'success', '0.1'], ['Pivot', 'navigation', 'success', '0.1'], ['Panorama', 'layout', 'success', '0.1'],
    ['App bar', 'navigation', 'success', '0.1'], ['Sheet', 'overlays', 'success', '0.1'], ['Toast', 'feedback', 'success', '0.1'], ['Status banner', 'feedback', 'success', '0.1'],
    ['Crumb', 'navigation', 'success', '0.2'], ['Data states', 'feedback', 'success', '0.2'], ['Menu', 'overlays', 'success', '0.2'], ['Table', 'content', 'success', '0.2'],
    ['Split', 'layout', 'success', '0.2'], ['Palette', 'navigation', 'success', '0.2'], ['Filters', 'forms', 'success', '0.2'], ['Form summary', 'forms', 'info', '0.2'],
    ['Combobox', 'forms', 'success', '0.3'], ['Toggle switch', 'forms', 'success', '0.3'], ['Stepper', 'forms', 'success', '0.3'], ['Slider', 'forms', 'success', '0.3'],
    ['Segments', 'forms', 'success', '0.3'], ['Drop zone', 'forms', 'success', '0.3'], ['Tooltip', 'overlays', 'success', '0.3'], ['Context menu', 'overlays', 'success', '0.3'],
    ['Tree', 'content', 'success', '0.3'], ['Sortable', 'content', 'success', '0.3'], ['Long list', 'content', 'success', '0.3'], ['Undo history', 'feedback', 'success', '0.3'],
    ['Live contract', 'feedback', 'success', '0.3'], ['Chart', 'data', 'success', '0.3'], ['Meter', 'data', 'success', '0.3'], ['Progress dots', 'data', 'success', '0.3'],
    ['Icons', 'assets', 'success', '0.3'], ['Title bar', 'shell', 'success', '0.3'], ['App shell', 'shell', 'info', '0.3'],
    ['Share bar', 'data', 'success', '0.4'], ['Icon button', 'actions', 'success', '0.4'], ['Status region', 'layout', 'success', '0.4'],
    ['Scroll state', 'navigation', 'success', '0.5'],
    ['Toggle button', 'actions', 'success', 'next'], ['Side panel', 'overlays', 'success', 'next'], ['Notification centre', 'feedback', 'success', 'next'], ['Level indicator', 'feedback', 'success', 'next'],
    ['Launch tile', 'tiles', 'success', 'next'], ['Preview tile', 'tiles', 'success', 'next'], ['Recent stack', 'navigation', 'success', 'next'], ['Month', 'forms', 'success', 'next'],
    ['Session', 'shell', 'success', 'next'], ['Accent surface', 'layout', 'success', 'next'], ['Beads', 'data', 'success', 'next'],
    ['Date range', 'forms', 'warning', 'next'], ['Rich text', 'forms', 'warning', 'next'], ['More grounds', 'colour', 'warning', 'next'],
  ];
  const compHtml = COMPONENTS.map(([name, group, status, since]) => `<tr data-name="${name}" data-status-v="${status}"><td class="rk-check"></td><th scope="row">${name}</th><td data-rk-edit data-rk-tip="double-click or F2 to edit">${group}</td><td><span class="rk-tag" data-status="${status}">${WORD[status]}</span></td><td class="rk-num" data-sort="${since === 'next' ? 9 : since}">${since}</td></tr>`).join('');
  $('#comp-body').innerHTML = compHtml;
  const setGroup = (row, g) => { row.cells[2].textContent = g; const c = COMPONENTS.find((x) => x[0] === row.dataset.name); if (c) c[1] = g; };
  const compTable = Rekta.table($('#comp-table'), {
    appbar: $('#bar'),
    columns: { key: 'rekta-site-comp-cols' },
    edit: {
      onCommit: (row, td, value, old) => wait(500).then(() => {
        if (!value) throw new Error('A component needs a group.');
        if (value.length > 16) throw new Error('Keep a group to 16 letters or fewer.');
        const c = COMPONENTS.find((x) => x[0] === row.dataset.name); if (c) c[1] = value;
        hist.do({ label: `Moved ${row.dataset.name} to “${value}”`, run: () => {}, undo: () => setGroup(row, old), redo: () => setGroup(row, value) });
      }),
    },
    onShow(row) { const [n, g, s, v] = COMPONENTS.find((c) => c[0] === row.dataset.name); $('#comp-shown').textContent = `Shown: ${n} · ${g} · ${WORD[s]} · since ${v}`; },
    onSelect(rows, { mode }) { if (mode) $('#comp-shown').textContent = `${rows.length} selected · the app bar carries them`; else if (!rows.length) $('#comp-shown').textContent = 'Nothing selected.'; },
  });
  function compFilter() {
    const q = $('#comp-q').value.trim().toLowerCase(), st = $$('#comp-status-menu [aria-checked="true"]').map((i) => i.dataset.v);
    const rows = $$('#comp-body tr'), keep = (tr) => !tr.hasAttribute('data-archived') && (!q || tr.textContent.toLowerCase().includes(q)) && (!st.length || st.includes(tr.dataset.statusV));
    const shown = rows.filter(keep).length;
    $('#comp-none').hidden = shown > 0;
    $('#comp-clear').hidden = !(q || st.length);
    const archived = $$('#comp-body tr[data-archived]').length;
    $('#comp-facts').textContent = `${COMPONENTS.length} components${shown < COMPONENTS.length ? ` · showing ${shown}` : ''}${archived ? ` · ${archived} archived` : ''} · ${COMPONENTS.filter((c) => c[2] === 'warning').length} waiting`;
    // The statuses shown stand as tags in a row: each fades in and out in place, as a control in a bar does.
    Rekta.presence($('#comp-tags'), { items: $$('#comp-tags > .rk-tag'), show: (t) => st.includes(t.dataset.v) });
    // Rows that go leave, the rest close up, rows that come back slide in (Rekta.presence).
    return Rekta.presence($('#comp-body'), { items: rows, show: keep }).then(() => compTable.refresh());
  }
  const compStatus = Rekta.menu($('#comp-status'), $('#comp-status-menu'), { summary: 'status', onSelect: compFilter });
  $('#comp-q').addEventListener('input', compFilter);
  const clearComp = () => { $('#comp-q').value = ''; compStatus.set(() => false); compFilter(); };
  $('#comp-clear').addEventListener('click', clearComp);
  Rekta.menu($('#comp-more'), $('#comp-menu'), {
    onSelect(item) {
      const region = $('#comp-region'), old = $('#comp-error');
      if (old) old.remove();
      Rekta.busy(region, async () => { await wait(1200); if (item.dataset.act === 'fail' || !live()) throw new Error('no'); })
        .then(() => toasts.show('Components refreshed.', { key: 'refresh' }), () => {
          const p = document.createElement('div');
          p.className = 'rk-empty'; p.id = 'comp-error'; p.dataset.status = 'error';
          p.innerHTML = '<p>Could not refresh the components: the server did not answer. What you see is from before.</p><p class="rk-actions"><button class="rk-btn" type="button" id="comp-retry">retry</button></p>';
          region.prepend(p);
        });
    },
  });
  // Archive: rows leave the table; the history takes them back.
  function archive(rows) {
    rows = rows.filter((r) => !r.hasAttribute('data-archived'));
    if (!rows.length) return;
    hist.do({
      label: rows.length === 1 ? `Archived ${rows[0].dataset.name}` : `Archived ${rows.length} components`,
      run: () => { rows.forEach((r) => r.setAttribute('data-archived', '')); compTable.clear(); compFilter(); },
      undo: () => { rows.forEach((r) => r.removeAttribute('data-archived')); compFilter(); },
    });
  }
  $('#sel-archive').addEventListener('click', () => { if (compTable.mode) archive(compTable.selected()); else toasts.show('Archive works on the components in “lists & tables”.', { key: 'archive-where' }); });
  // Columns: a menu shows and hides them; their edges size them.
  const colMenu = Rekta.menu($('#comp-cols'), $('#comp-cols-menu'), { onSelect(item) { const i = +item.dataset.col; if (item.getAttribute('aria-checked') === 'true') compTable.show(i); else compTable.hide(i); } });
  colMenu.set((i) => !compTable.hidden().includes(+i.dataset.col));
  // A context menu on each row: the selection follows the row it opens on.
  Rekta.contextMenu($('#comp-body'), $('#comp-ctx'), {
    target: 'tr',
    onOpen(row) { if (row.getAttribute('aria-selected') !== 'true') compTable.only(row, { show: false }); },
    async onSelect(item, row) {
      const act = item.dataset.act;
      if (act === 'show') compTable.only(row);
      else if (act === 'edit') compTable.edit(row.querySelector('td[data-rk-edit]'));
      else if (act === 'copy') { let ok = true; try { await navigator.clipboard.writeText(row.dataset.name); } catch { ok = false; } toasts.show(ok ? `Copied “${row.dataset.name}”.` : 'Copying needs clipboard access.', { key: 'copy' }); }
      else if (act === 'archive') archive(compTable.mode ? compTable.selected() : [row]);
    },
  });

  // The same selection on plain rows.
  const pick = Rekta.select($('#pick-rows'), {
    appbar: $('#bar'),
    onShow(li) { $('#pick-shown').textContent = `Shown: ${li.dataset.name}.`; },
    onChange(items, { mode }) { if (mode) $('#pick-shown').textContent = `${items.length} selected`; else if (!items.length) $('#pick-shown').textContent = 'Nothing selected.'; },
  });
  document.addEventListener('click', (e) => { if (e.target.closest('#comp-retry')) { $('#comp-error').remove(); Rekta.busy($('#comp-region'), wait(900)).then(() => toasts.show('Components refreshed.', { key: 'refresh' })); } });
  compFilter();

  /* ---------- the tokens place: a split, the current token in the address ---------- */
  const GROUPS = [...new Set(TOKENS.map((t) => t.group))];
  $('#tok-group-menu').innerHTML = GROUPS.map((g) => `<button class="rk-menu-item" role="menuitemcheckbox" aria-checked="false" type="button" data-v="${g}">${g}</button>`).join('');
  const isColour = (v) => /^#|^rgb|^var\(--rk-/.test(v || '');
  const short = (t) => (t.alt && t.alt !== t.value ? `${t.value} · ${t.alt}` : t.value);
  $('#tok-body').innerHTML = TOKENS.map((t) => `<tr data-name="${t.name}" data-group="${t.group}"><td class="rk-check"></td><th scope="row"><a href="#tokens/${t.name}">${t.name}</a></th><td>${t.group}</td><td>${short(t).replace(/</g, '&lt;')}</td></tr>`).join('');
  const tokSplit = Rekta.split($('#tokens-split'), { key: 'rekta-site-tokens-split' });
  const tokTable = Rekta.table($('#tok-table'), { appbar: $('#bar'), onShow: (row) => router.open(row.dataset.name) });
  function renderToken(name) {
    const t = TOKENS.find((x) => x.name === name);
    if (!t) { $('#token-detail').innerHTML = '<p class="rk-empty">Choose a token to see what it is for.</p>'; return; }
    const sw = t.group === 'colour' ? `<div class="token-swatches"><span style="background:${t.value.startsWith('var(') ? t.value : t.value}" title="light"></span><span style="background:${t.alt && t.alt.startsWith('var(') ? t.alt : t.alt}" title="dark"></span></div>` : '';
    const esc = (s) => String(s || '').replace(/&/g, '&amp;').replace(/</g, '&lt;');
    $('#token-detail').innerHTML = `<div><p class="rk-kicker">${t.group}</p><h2 class="token-name" id="token-title" tabindex="-1">--rk-${t.name}</h2></div>${sw}`
      + `<p>${esc(t.description) || 'A token of Rekta Design.'}</p><ul class="rk-rows">`
      + `<li>${t.altLabel === 'compact' ? 'comfortable' : t.altLabel === 'dark' ? 'light' : 'value'}<b>${esc(t.value)}</b></li>`
      + (t.alt ? `<li>${t.altLabel}<b>${esc(t.alt)}</b></li>` : '')
      + `<li>source<code>${esc(t.path)}</code></li><li>on the web<code>var(--rk-${t.name})</code></li><li>native<code>${t.name.replace(/(^|-)(\w)/g, (m, a, c) => c.toUpperCase())}</code></li></ul>`;
  }
  function tokFilter({ write = true } = {}) {
    const q = $('#tok-q').value.trim().toLowerCase(), gs = $$('#tok-group-menu [aria-checked="true"]').map((i) => i.dataset.v);
    let shown = 0;
    for (const tr of $$('#tok-body tr')) { const keep = (!q || tr.textContent.toLowerCase().includes(q)) && (!gs.length || gs.includes(tr.dataset.group)); tr.hidden = !keep; if (keep) shown += 1; }
    $('#tok-none').hidden = shown > 0;
    $('#tok-clear').hidden = !(q || gs.length);
    $('#tokens-facts').textContent = `${TOKENS.length} tokens${shown < TOKENS.length ? ` · showing ${shown}` : ''} · the same names on every platform`;
    tokTable.refresh();
    if (write && router.current === 'tokens') {
      const p = new URLSearchParams();
      if (q) p.set('q', q);
      if (gs.length) p.set('group', gs.join(','));
      router.setParams(p);
    }
  }
  const tokGroup = Rekta.menu($('#tok-group'), $('#tok-group-menu'), { summary: 'group', onSelect: () => tokFilter() });
  let typing = 0;
  $('#tok-q').addEventListener('input', () => { clearTimeout(typing); typing = setTimeout(tokFilter, 150); });
  const clearTok = () => { $('#tok-q').value = ''; tokGroup.set(() => false); tokFilter(); $('#tok-q').focus(); };
  $('#tok-clear').addEventListener('click', clearTok);
  document.addEventListener('click', (e) => { const c = e.target.closest('[data-clear]'); if (c) (c.dataset.clear === 'tok' ? clearTok : clearComp)(); });
  let lastToken = null;
  router.onItem('tokens', (name, { initial, params }) => {
    const words = (k) => (params.get(k) || '').split(',').filter(Boolean);
    $('#tok-q').value = params.get('q') || '';
    tokGroup.set((i) => words('group').includes(i.dataset.v));
    tokFilter({ write: false });
    const row = name && $(`#tok-body tr[data-name="${CSS.escape(name)}"]`);
    if (row) { if (!tokTable.mode) tokTable.only(row, { show: false }); } else if (!name && !tokTable.mode) tokTable.clear();
    renderToken(name);
    if (name) {
      if (!initial) tokSplit.reveal();
      tokSplit.show('detail', { focus: $('#token-title'), animate: !initial });
      lastToken = name;
    } else {
      const link = lastToken && $(`#tok-body tr[data-name="${CSS.escape(lastToken)}"]`);
      tokSplit.show('list', { focus: link, animate: !initial });
    }
    return name ? `--rk-${name}` : null;
  }, { push: () => !tokSplit.sideways });
  // The app bar carries whichever list is in its selection mode.
  $('#sel-copy').addEventListener('click', async (e) => {
    const from = [tokTable, compTable, pick, logList].find((s) => s.mode) || compTable, rows = from.selected();
    const text = from === tokTable ? rows.map((r) => `var(--rk-${r.dataset.name})`).join('\n') : from === logList ? rows.map((k) => { const l = logLine(logMap[k]); return `${l.time} ${WORD[l.level]} ${l.msg}`; }).join('\n') : rows.map((r) => r.dataset.name).join(', ');
    let ok = true;
    try { await navigator.clipboard.writeText(text); } catch { ok = false; }
    toasts.show(ok ? `Copied ${rows.length === 1 ? '1 name' : `${rows.length} names`}.` : `Copying needs clipboard access; ${rows.length} selected.`, { key: 'copy', from: e.currentTarget });
  });

  /* ---------- navigation: go to, shortcuts, a radio menu ---------- */
  const palette = Rekta.palette($('#palette'), {
    trigger: $('#ab-goto'), router,
    items: () => [
      { group: 'places', label: 'Rekta Design', href: '#', keywords: 'home overview' },
      { group: 'places', label: 'Tokens', href: '#tokens', keywords: 'values table split' },
      { group: 'places', label: 'Release notes', href: '#notes', keywords: 'new 0.2' },
      { group: 'places', label: 'All apps', href: '#apps', keywords: 'start launcher programs' },
      ...$$('#nav a').map((a) => ({ group: 'sections', label: a.textContent, keywords: a.hash.slice(1), run: () => router.navigate('home').then(() => home.goTo(a.hash.slice(1))) })),
      ...TOKENS.map((t) => ({ group: 'tokens', label: `--rk-${t.name}`, keywords: `${t.group} ${t.name}`, href: `#tokens/${t.name}` })),
      { group: 'commands', label: banner.is('paused') ? 'resume the live data' : 'pause the live data', run: () => $('#ab-pause').click() },
      { group: 'commands', label: `theme: next (now ${prefs.get('theme')})`, run: () => $('#ab-theme').click() },
      { group: 'commands', label: prefs.get('motion') === 'off' ? 'turn motion on' : 'turn motion off', run: () => $('#ab-motion').click() },
      { group: 'commands', label: 'keyboard shortcuts', keys: '?', run: () => $('#keys-sheet').showModal() },
    ],
  });
  Rekta.appbar($('#bar'));
  $('#n-goto').addEventListener('click', () => palette.open());
  $('#n-keys').addEventListener('click', () => $('#keys-sheet').showModal());
  Rekta.menu($('#n-sort'), $('#n-sort-menu'), { onSelect: (item) => { tokTable.sort(+item.dataset.sort, 'ascending'); toasts.show(`Tokens sorted by ${item.textContent.trim()}.`, { key: 'sort' }); } });
  Rekta.keys.on(5, (e) => {
    if (e.key !== '?' || e.ctrlKey || e.metaKey || e.altKey || Rekta.keys.isEditable(e.target) || document.querySelector('dialog[open]')) return false;
    $('#keys-sheet').showModal(); return true;
  });
  // Links to a section of this place move the panorama instead of the address.
  document.addEventListener('click', (e) => {
    const a = e.target.closest('[data-goto]');
    if (!a || e.button !== 0 || e.ctrlKey || e.metaKey || e.shiftKey) return;
    e.preventDefault();
    router.navigate('home').then(() => home.goTo(a.dataset.goto));
  });

  /* ---------- colour: content fills, one switch for the whole set ---------- */
  $('#launcher-fills').addEventListener('change', (e) => { $('#launcher').dataset.rkFills = e.target.checked ? 'content' : 'plain'; });

  /* ---------- forms: make an accent, checked late, guarded ---------- */
  // Checked as a product lets its users choose (docs/colour.md, Letting users choose): with rekta.color.js against
  // every theme's colours, and refused with the reason and a colour that would hold.
  let customAccent = store.get('rekta-site-accent-def');
  if (customAccent && !customAccent.pairs) customAccent = null;   /* one made before accents had a pair per contrast set */
  const HEX = /^#[0-9a-f]{6}$/i, C = window.RektaColor, { themes: THEMES_DATA, accents: RektaAccents } = JSON.parse($('#themes-data').textContent);
  const STATUS_WORD = { info: 'active', success: 'done', warning: 'waiting', error: 'failed' };
  const onFor = (hex, t) => (C.contrast(hex, t.bg) >= C.contrast(hex, t['text-primary']) ? t.bg : t['text-primary']);
  function accentHint(v, theme) {
    const t = v.trim(), th = THEMES_DATA[theme];
    if (!HEX.test(t)) return 'Write a hex colour with six digits, like #1a6680.';
    const check = C.accentCheck(t, onFor(t, th), th, C.TEXT[theme]);
    if (check.ok) return null;
    const derived = C.deriveAccent(t, { [theme]: th }), better = derived.ok ? ` Try ${derived.themes[theme].accent}: the nearest that holds.` : '';
    const r = C.contrast(t, th.bg);
    if (r < 3) return `${t} holds only ${r.toFixed(1)}:1 on the ${theme} ground; an accent needs 3:1.${better || ` Try a ${theme === 'light' ? 'darker' : 'lighter'} one.`}`;
    if (check.apart.problems.length) {
      const n = check.apart.hue.degrees < C.APART.hue && check.apart.chroma >= C.APART.grey ? check.apart.hue : check.apart.nearest.normal;
      return `${t} is too close to the status colour for “${STATUS_WORD[n.status]}”: it would read as a status.${better || ' Its hue is a status hue: try one between green and blue, or from blue through violet to pink.'}`;
    }
    return `No text reads on ${t} at ${C.TEXT[theme]}:1.${better}`;
  }
  const accentForm = Rekta.form($('#accent-form'), {
    validators: {
      'a-name': (v) => (!v.trim() ? 'Give the accent a name.' : /^[a-z][a-z0-9-]*$/.test(v.trim()) ? null : 'Use lowercase letters, digits and dashes, like "sea-green".'),
      'a-light': (v) => accentHint(v, 'light'),
      'a-dark': (v) => (v.trim() ? accentHint(v, 'dark') : null),
    },
    async onSubmit(values) {
      await wait(800);
      if (banner.is('readonly')) throw { message: 'Could not apply: maintenance holds every change until 15:00. Your accent is still here.' };
      if (values['a-name'].trim() in RektaAccents) throw { fields: { 'a-name': `An accent called "${values['a-name'].trim()}" already exists. Choose another name.` } };
      // The light value as given; dark as given, or derived from the light one; under more contrast each derived.
      const light = values['a-light'].trim(), dark = values['a-dark'].trim();
      const from = (seed, theme) => (C.deriveAccent(seed, { [theme]: THEMES_DATA[theme] }).themes[theme] || RektaAccents.ink[theme]);
      const pairs = {
        light: { accent: light, 'on-accent': onFor(light, THEMES_DATA.light) },
        dark: dark ? { accent: dark, 'on-accent': onFor(dark, THEMES_DATA.dark) } : from(light, 'dark'),
        'light-contrast': from(light, 'light-contrast'),
      };
      pairs['dark-contrast'] = from(pairs.dark.accent, 'dark-contrast');
      customAccent = { name: values['a-name'].trim(), pairs };
      window.siteAccent(customAccent);
      store.set('rekta-site-accent-def', customAccent); prefs.set('accent', 'custom');
      accentGuard.clean();
      paintAccent();
      toasts.show(`Accent “${customAccent.name}” applied: the primary buttons and selected rows take it.`, { key: 'accent', from: $('#a-save') });
    },
  });
  const accentGuard = Rekta.guard($('#accent-form'), { router, what: 'The accent' });
  function paintAccent() {
    $('#accent-custom').disabled = !customAccent;
    Rekta.disable($('#a-remove'), customAccent ? null : 'There is no accent of yours to remove yet.');
    paintPrefs();
  }
  $('#a-remove').addEventListener('click', async () => {
    if (!(await Rekta.confirm({ kicker: 'accent', title: `Remove “${customAccent.name}”?`, text: 'The primary buttons and selected rows go back to ink. You can make it again.', ok: 'remove accent', cancel: 'keep it', safe: 'cancel' }))) return;
    customAccent = null; window.siteAccent(null); store.set('rekta-site-accent-def', null); prefs.set('accent', 'product'); paintAccent();
    toasts.show('Accent removed.', { key: 'accent' });
  });
  // A form in a sheet: a close request asks while the name has been typed in.
  const viewSheet = $('#view-sheet');
  Rekta.sheet(viewSheet, { dirty: () => $('#view-name').value.trim() !== '' });
  $('#view-open').addEventListener('click', () => { $('#view-name').value = ''; viewSheet.showModal(); });
  viewSheet.addEventListener('close', () => {
    if (viewSheet.returnValue === 'save' && $('#view-name').value.trim()) toasts.show(`View “${$('#view-name').value.trim()}” saved.`, { key: 'view', from: $('#view-open') });
  });
  if (customAccent) { $('#a-name').value = customAccent.name; $('#a-light').value = customAccent.pairs.light.accent; $('#a-dark').value = customAccent.pairs.dark.accent; accentGuard.clean(); }

  /* ---------- desktop: a tree, tooltips, context menus, a sortable list, ten thousand lines ---------- */
  const rowText = (li) => li.querySelector(':scope > .rk-tree-row').textContent.trim();
  const pathOf = (li) => { const p = []; for (let x = li; x; x = x.parentElement.closest('[role=treeitem]')) p.unshift(rowText(x)); return p; };
  const files = Rekta.tree($('#tree'), {
    onShow(li) {
      const p = pathOf(li), kids = li.querySelectorAll(':scope > ul > li').length;
      $('#file-path').textContent = p.length > 1 ? p.slice(0, -1).join(' / ') : 'Rekta Design';
      $('#file-name').textContent = p[p.length - 1];
      $('#file-about').textContent = li.dataset.about || `A folder of ${kids === 1 ? 'one item' : `${kids} items`}. → opens it, ← closes it.`;
    },
  });
  async function copyPath(li) {
    if (!li) { toasts.show('Choose a file first.', { key: 'copy' }); return; }
    const path = pathOf(li).join('/');
    let ok = true; try { await navigator.clipboard.writeText(path); } catch { ok = false; }
    toasts.show(ok ? `Copied ${path}.` : `Copying needs clipboard access: ${path}`, { key: 'copy' });
  }
  $('#tree-open').addEventListener('click', () => files.expandAll(true));
  $('#tree-close').addEventListener('click', () => files.expandAll(false));
  $('#tree-copy').addEventListener('click', () => copyPath(files.selected()));
  $('#tree').addEventListener('keydown', (e) => { if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'c' && e.target.matches('[role=treeitem]')) { e.preventDefault(); copyPath(e.target); } });
  Rekta.contextMenu($('#tree'), $('#tree-menu'), {
    target: '[role=treeitem]',
    onOpen(li) { files.select(li); },
    onSelect(item, li) {
      if (item.dataset.act === 'show') files.select(li);
      else if (item.dataset.act === 'copy') copyPath(li);
      else if (item.dataset.act === 'open') files.expandAll(true);
    },
  });
  let replaying = false;
  const steps = Rekta.sortable($('#steps'), {
    onMove(from, to, item) {
      if (replaying) return;
      const name = item.querySelector('b').textContent, again = (i) => { replaying = true; steps.move(item, i); replaying = false; };
      hist.do({ label: `Moved “${name}” to step ${to + 1}`, run: () => {}, undo: () => again(from), redo: () => again(to) });
    },
  });
  const STEP_ORDER = $$('#steps > li').map((li) => li.querySelector('b').textContent);
  // Ten thousand log lines, made up the same way every time.
  const LOG_N = 10000;
  const MSGS = ['build started', 'tests passed', 'cache warmed', 'schema check failed: column “due” is missing', 'retry scheduled in 30 s', 'deploy finished', 'token check passed', 'waiting for a free runner', 'artifact uploaded', 'smoke test passed', 'contrast check passed', 'review requested'];
  const rnd = (i) => { const x = Math.sin(i * 12.9898 + 78.233) * 43758.5453; return x - Math.floor(x); };
  function logLine(i) {
    const r = rnd(i), s = i * 7, level = r < 0.03 ? 'error' : r < 0.13 ? 'warning' : r < 0.55 ? 'info' : 'success';
    const msg = level === 'error' ? MSGS[3] : level === 'warning' ? MSGS[i % 2 ? 4 : 7] : MSGS[[0, 1, 2, 5, 6, 8, 9, 10, 11][Math.floor(rnd(i + 1) * 9)]];
    return { n: i + 1, time: [Math.floor(s / 3600) % 24, Math.floor(s / 60) % 60, s % 60].map((v) => String(v).padStart(2, '0')).join(':'), level, msg };
  }
  let logMap = Array.from({ length: LOG_N }, (_, i) => i);
  const logList = Rekta.virtual($('#log'), {
    count: LOG_N, label: 'log lines', appbar: $('#bar'),
    text(k) { const l = logLine(logMap[k]); return `${WORD[l.level]} ${l.msg}`; },   // a letter jumps by the line's word
    render(k, row) {
      const l = logLine(logMap[k]);
      row.innerHTML = `<span class="rk-meta">#${String(l.n).padStart(5, '0')}</span><span class="rk-tag" data-status="${l.level}">${WORD[l.level]}</span><span></span><span class="rk-meta">${l.time}</span>`;
      row.children[2].textContent = l.msg;
    },
    onShow(k) { const l = logLine(logMap[k]); $('#log-shown').textContent = `Line ${l.n} · ${l.time} · ${WORD[l.level]} · ${l.msg}`; },
    onChange(list, { mode }) { if (mode) $('#log-shown').textContent = `${list.length} lines selected · the app bar carries them`; },
  });
  function logFilter() {
    const f = ($$('input[name=log-f]').find((r) => r.checked) || {}).value || 'all';
    logMap = f === 'all' ? Array.from({ length: LOG_N }, (_, i) => i) : Array.from({ length: LOG_N }, (_, i) => i).filter((i) => logLine(i).level === f);
    logList.clear(); logList.refresh(logMap.length); $('#log').scrollTop = 0;
    $('#log-facts').textContent = `${logMap.length.toLocaleString('en')} of ${LOG_N.toLocaleString('en')} lines · ${$$('#log .rk-virtual-row').length} in the page`;
  }
  $$('input[name=log-f]').forEach((r) => r.addEventListener('change', logFilter));
  logFilter();

  /* ---------- live and undo ---------- */
  const feed = Rekta.live($('#live-region'), { stale: 8000 });
  const EVENTS_LIVE = [['Build #4190 finished', 'success'], ['A runner joined the pool', 'info'], ['Review waiting on Ada', 'warning'], ['Nightly import retried', 'info'], ['Deploy to staging finished', 'success'], ['Disk at 81% on runner 3', 'warning']];
  let rps = 118, queue = 14, doneToday = 312, tickN = 0;
  const rpsSeries = [];
  function pushEvent([text, status]) {
    const list = $('#lv-events'), li = document.createElement('li');
    li.innerHTML = `<span></span><span class="rk-tag" data-status="${status}">${WORD[status]}</span>`;
    li.firstChild.textContent = text;
    Rekta.reflow(list, () => { list.prepend(li); while (list.children.length > 4) list.lastChild.remove(); }, { size: false });
    Rekta.enter(li);
  }
  function liveTick() {
    if (!$('#lv-feed').checked || !$('#lv-online').checked || !live()) return;
    tickN += 1;
    rps = Math.max(40, Math.min(220, Math.round(rps + (rnd(tickN) - 0.5) * 30)));
    queue = Math.max(0, Math.min(40, queue + Math.round((rnd(tickN + 7) - 0.55) * 4)));
    doneToday += Math.round(rnd(tickN + 3) * 3);
    rpsSeries.push(rps); if (rpsSeries.length > 24) rpsSeries.shift();
    feed.update(() => { $('#lv-rps').textContent = rps; Rekta.sparkTo($('#live-region .rk-spark polyline'), points(rpsSeries, $('#live-region .rk-spark'))); }, 'rps');
    // A new count is news: it peeks in. Requests a second is a continuous reading: it changes in place.
    feed.update(() => Rekta.tileUpdate($('#lv-queue').closest('.rk-tile'), () => { $('#lv-queue').textContent = queue; }), 'queue');
    feed.update(() => Rekta.tileUpdate($('#lv-done').closest('.rk-tile'), () => { $('#lv-done').textContent = doneToday; }), 'done');
    if (tickN % 3 === 0) { const ev = EVENTS_LIVE[tickN % EVENTS_LIVE.length]; feed.update(() => pushEvent(ev)); }
  }
  setInterval(liveTick, 1500); liveTick();
  $('#lv-online').addEventListener('change', (e) => { feed.offline(!e.target.checked); toasts.show(e.target.checked ? 'Connected again. Figures are live.' : 'Connection lost. The figures stay, marked as offline.', { key: 'lv-online' }); });
  $('#lv-feed').addEventListener('change', (e) => toasts.show(e.target.checked ? 'Feed on.' : `Feed off: in ${Math.round(8)} seconds the figures say they are stale.`, { key: 'lv-feed' }));
  let histSeen = 0;
  function paintHistory() {
    const list = $('#h-list'), entries = hist.entries().slice().reverse(), grew = entries.length > histSeen;
    histSeen = entries.length;
    for (const [btn, can, why] of [[$('#h-undo'), hist.canUndo, 'nothing to undo yet'], [$('#h-redo'), hist.canRedo, 'nothing undone to do again']]) {
      btn.setAttribute('aria-disabled', String(!can));
      btn.dataset.rkTip = can ? (btn.id === 'h-undo' ? 'undo the last action' : 'do it again') : why;
    }
    if (!entries.length) { list.innerHTML = '<li class="rk-empty">Nothing yet. Archive a component, edit a group, or order the release.</li>'; return; }
    list.replaceChildren(...entries.map((e) => {
      const li = document.createElement('li');
      li.innerHTML = '<b></b><span class="rk-meta"></span>';
      li.firstChild.textContent = e.label;
      li.lastChild.textContent = e.done ? new Date(e.at).toTimeString().slice(0, 8) : 'undone';
      if (!e.done) li.setAttribute('data-undone', '');
      return li;
    }));
    if (grew) Rekta.enter(list.firstElementChild);
  }
  document.addEventListener('rk:historychange', paintHistory);
  $('#h-undo').addEventListener('click', () => { if (hist.canUndo) hist.undo(); });
  $('#h-redo').addEventListener('click', () => { if (hist.canRedo) hist.redo(); });
  paintHistory();

  /* ---------- data: charts in ink and grey, meters, progress, the dots ---------- */
  const MON = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'];
  const dayLabels = Array.from({ length: 14 }, (_, i) => { const d = new Date(); d.setDate(d.getDate() - 13 + i); return `${MON[d.getMonth()]} ${d.getDate()}`; });
  const lineChart = Rekta.chart($('#chart-line'), {
    type: 'line', labels: dayLabels,
    series: [
      { label: 'these two weeks', values: dayLabels.map((_, i) => Math.round(26 + i * 1.2 + rnd(i + 40) * 12)) },
      { label: 'the two before', values: dayLabels.map((_, i) => Math.round(22 + i * 0.6 + rnd(i + 90) * 10)) },
    ],
    summary: 'Deploys a day over the last two weeks, against the two weeks before. Both rise; these two weeks stay above.',
  });
  // Same days, other figures: the lines move to the new values instead of being drawn again.
  const LINES = {
    deploys: { a: (i) => Math.round(26 + i * 1.2 + rnd(i + 40) * 12), b: (i) => Math.round(22 + i * 0.6 + rnd(i + 90) * 10), summary: 'Deploys a day over the last two weeks, against the two weeks before. Both rise; these two weeks stay above.' },
    reviews: { a: (i) => Math.round(18 - i * 0.4 + rnd(i + 140) * 9), b: (i) => Math.round(14 + rnd(i + 190) * 8), summary: 'Reviews a day over the last two weeks, against the two weeks before. Fewer each day now, still above the weeks before.' },
  };
  $$('input[name=line-what]').forEach((r) => r.addEventListener('change', () => {
    const d = LINES[r.value];
    lineChart.update({ series: [{ label: 'these two weeks', values: dayLabels.map((_, i) => d.a(i)) }, { label: 'the two before', values: dayLabels.map((_, i) => d.b(i)) }], summary: d.summary });
  }));
  const RANGES = {
    week: { labels: ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun'], values: [412, 468, 431, 502, 377, 64, 41] },
    weeks: { labels: Array.from({ length: 8 }, (_, i) => `wk ${33 + i}`), values: [2140, 2380, 2210, 2620, 2490, 2850, 2710, 2295] },
    months: { labels: ['May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct'], values: [9800, 10400, 8900, 11800, 12600, 7300] },
  };
  const barChart = Rekta.chart($('#chart-bars'), { type: 'bars', labels: RANGES.week.labels, series: [{ label: 'build minutes', values: RANGES.week.values }], unit: ' min', summary: 'Build minutes by weekday: busiest on Thursday, quiet at the weekend.' });
  $$('input[name=bars-range]').forEach((r) => r.addEventListener('change', () => { const d = RANGES[r.value]; barChart.update({ labels: d.labels, series: [{ label: 'build minutes', values: d.values }], summary: `Build minutes ${r.parentElement.textContent.trim()}.` }); }));
  let cpu = 42;
  setInterval(() => {
    if (!live()) return;
    cpu = Math.max(8, Math.min(96, Math.round(cpu + (Math.random() - 0.5) * 18)));
    const m = $('#m-cpu'); m.style.setProperty('--rk-v', cpu / 100); m.setAttribute('aria-valuenow', cpu); $('#m-cpu-v').textContent = `${cpu}%`;
  }, 2000);
  let imp = 0;
  setInterval(() => { imp = imp >= 100 ? 0 : Math.min(100, imp + 3); $('#imp').value = imp; $('#imp-v').textContent = imp >= 100 ? 'done' : `${imp}%`; }, 400);
  $('#signin').addEventListener('click', (e) => {
    const b = e.currentTarget;
    if (b.getAttribute('aria-disabled') === 'true') return;
    b.setAttribute('aria-disabled', 'true'); b.textContent = 'signing in…';
    $('#dots').hidden = false; $('#signin-msg').textContent = 'Signing in. Nothing else can happen until it is done.';
    setTimeout(() => { $('#dots').hidden = true; b.removeAttribute('aria-disabled'); b.textContent = 'sign in'; $('#signin-msg').textContent = 'Signed in, as no one in particular.'; toasts.show('Signed in.', { key: 'signin', from: b }); }, 2600);
  });

  /* ---------- the app shell: the title bar, installable, offline ---------- */
  Rekta.titlebar($('#titlebar'), { router });
  const tag = (el, on, yes, no, st) => { el.dataset.status = on ? st : ''; if (!on) delete el.dataset.status; el.textContent = on ? yes : no; };
  function paintPwa(s) {
    tag($('#pwa-offline'), s.offlineReady, 'yes', s.supported ? 'not yet' : 'not here', 'success');
    tag($('#pwa-installed'), s.installed, 'yes, in its own window', 'in a browser tab', 'success');
    tag($('#pwa-online'), navigator.onLine, 'online', 'offline', 'success');
    if (!navigator.onLine) $('#pwa-online').dataset.status = 'warning';
    Rekta.disable($('#pwa-install'), s.installed ? 'Already installed.' : s.canInstall ? null : (s.supported ? 'Your browser offers installing from its own menu, or not at all.' : 'Installing needs the site served over https.'));
  }
  const app2 = Rekta.pwa({ toaster: toasts, onChange: paintPwa });
  paintPwa(app2.state);
  $('#pwa-install').addEventListener('click', async () => { if ($('#pwa-install').getAttribute('aria-disabled') === 'true') return; await app2.install(); });
  $('#pwa-check').addEventListener('click', async () => {
    await Rekta.pending($('#pwa-check'), Promise.all([app2.check().catch(() => {}), wait(600)]), { label: 'checking…' });
    if (!app2.state.updateReady) toasts.show('This is the newest version.', { key: 'rk-pwa-check' });
  });
  addEventListener('offline', () => { setState('offline', true); paintPwa(app2.state); });
  addEventListener('online', () => { setState('offline', false); paintPwa(app2.state); });

  /* ---------- inputs: build a tile, find a token, text size, a date, files ---------- */
  const built = $('#built');
  let builtWas = null;
  function build() {
    const st = $('#b-status').value, size = ($$('input[name=b-size]').find((r) => r.checked) || {}).value, value = $('#b-value').value;
    const was = builtWas;
    builtWas = { st, size, value };
    // A new size re-flows the grid around the tile; a new status turns it over; a new number peeks in.
    if (!was || (size === was.size && st === was.st && value === was.value)) paintBuilt();
    else if (size !== was.size) Rekta.reflow(built.parentElement, paintBuilt);
    else if (st !== was.st) Rekta.tileUpdate(built, paintBuilt, { kind: 'flip' });
    else Rekta.tileUpdate(built, paintBuilt);
  }
  function paintBuilt() {
    const st = $('#b-status').value, size = ($$('input[name=b-size]').find((r) => r.checked) || {}).value;
    if (st) built.dataset.status = st; else delete built.dataset.status;
    built.className = `rk-tile${size === 'w2' ? ' rk-w2' : size === 'h2' ? ' rk-w2 rk-h2' : ''}`;
    $('.rk-tile-word', built).textContent = st ? WORD[st] : 'idle';
    $('.rk-stat b', built).textContent = $('#b-value').value || '0';
    $('.rk-stat span', built).textContent = $('#b-label').value.trim() || 'no label';
    let spark = $('.rk-spark', built);
    if ($('#b-spark').checked && !spark && st !== 'success') {
      built.insertAdjacentHTML('beforeend', '<svg class="rk-spark" viewBox="0 0 200 28" role="img" aria-label="rising over the last 8 hours"><polyline pathLength="1" points="1,24 30,20 60,22 90,14 120,16 150,9 180,11 199,4"/></svg>');
    } else if ((!$('#b-spark').checked || st === 'success') && spark) spark.remove();
  }
  $('#builder').addEventListener('input', build);
  $('#builder').addEventListener('change', build);
  $('#builder').addEventListener('submit', (e) => e.preventDefault());
  build();
  const tokenFind = Rekta.combobox($('#find-token'), {
    options: () => TOKENS.map((t) => ({ label: `--rk-${t.name}`, value: t.name, hint: t.group, group: t.group, keywords: `${t.name} ${t.value || ''}` })),
    onSelect(o) {
      const t = o && TOKENS.find((x) => x.name === o.value);
      $('#find-hint').textContent = t ? `${t.value}${t.alt && t.alt !== t.value ? ` · ${t.altLabel} ${t.alt}` : ''}: ${t.description || 'a token of Rekta Design.'}` : 'A combobox: it filters as you type and keeps only a choice from its list.';
    },
  });
  // Toggle buttons: a press switches the state at once; the state word follows from data-on and data-off, or is set here.
  $('#quick').addEventListener('click', (e) => {
    const b = e.target.closest('[aria-pressed]');
    if (!b) return;
    const on = b.getAttribute('aria-pressed') !== 'true';
    b.setAttribute('aria-pressed', String(on));
    if (b.id === 'quick-pin') b.querySelector('.rk-btn-state').textContent = on ? '3 tiles' : 'none';
  });
  $('#text-size').addEventListener('input', (e) => { root.style.fontSize = +e.target.value === 100 ? '' : `${e.target.value}%`; env(); });
  const later = new Date(); later.setDate(later.getDate() + 3);
  $('#when-d').value = `${later.getFullYear()}-${String(later.getMonth() + 1).padStart(2, '0')}-${String(later.getDate()).padStart(2, '0')}`;
  // Days are calendar days in the user's time zone, whatever the hour now.
  const midnight = (x) => new Date(x.getFullYear(), x.getMonth(), x.getDate());
  function when() {
    const d = $('#when-d').value, t = $('#when-t').value || '09:00';
    if (!d) { $('#when-hint').textContent = 'No date: nothing to remind you of.'; return; }
    const at = new Date(`${d}T${t}`), days = Math.round((midnight(at) - midnight(new Date())) / 864e5);
    $('#when-hint').textContent = at < Date.now() ? 'That has passed: choose a later time.' : `${days === 0 ? 'Today' : days === 1 ? 'Tomorrow' : `In ${days} days`}, at ${t}.`;
  }
  $('#when-d').addEventListener('input', when); $('#when-t').addEventListener('input', when); when();
  const LIMIT = 4 * 1024 * 1024;
  const kb = (n) => (n < 1024 * 1024 ? `${Math.max(1, Math.round(n / 1024))} kB` : `${(n / 1024 / 1024).toFixed(1)} MB`);
  Rekta.drop($('#drop'), {
    onFiles(files) {
      const list = $('#files'), added = [];
      list.hidden = false;
      for (const f of files) {
        const li = document.createElement('li');
        added.push(li);
        li.innerHTML = `<b></b><span class="rk-tag" data-status="warning">waiting</span><span class="rk-meta"></span><progress class="rk-progress" max="100" value="0"></progress>`;
        $('b', li).textContent = f.name; $('.rk-meta', li).textContent = kb(f.size);
        if (f.size > LIMIT) { const tag = $('.rk-tag', li); tag.dataset.status = 'error'; tag.textContent = 'failed'; $('.rk-meta', li).textContent = `${kb(f.size)} · larger than ${kb(LIMIT)}`; $('progress', li).remove(); continue; }
        let v = 0;
        const tag = $('.rk-tag', li), bar = $('progress', li), step = Math.max(4, 40 - f.size / 50000);
        tag.dataset.status = 'info'; tag.textContent = 'active';
        const timer = setInterval(() => {
          v = Math.min(100, v + step); bar.value = v;
          if (v >= 100) { clearInterval(timer); tag.dataset.status = 'success'; tag.textContent = 'done'; bar.remove(); }
        }, 120);
      }
      Rekta.reflow(list, () => added.forEach((li) => list.prepend(li)), { size: false });
      added.forEach(Rekta.enter);
      toasts.show(`${files.length === 1 ? '1 file' : `${files.length} files`} added. Nothing leaves your device.`, { key: 'files' });
    },
  });

  /* ---------- tiles: a status region, figures with their own controls ---------- */
  // A job's figures beside the controls that act on it are not a tile (a tile never changes data): no fill, no press,
  // its status a tag in the kicker. Its figures are a continuous reading, so they change in place.
  const JOB = { tests: 412, pace: 9 };
  const JOB_WORD = { running: ['info', 'active'], paused: ['warning', 'waiting'], done: ['success', 'done'], cancelled: ['', 'idle'] };
  let job = { run: 0, state: 'running' };
  function paintJob() {
    const [st, word] = JOB_WORD[job.state], tag = $('#job-tag'), ended = job.state === 'done' || job.state === 'cancelled';
    if (st) tag.dataset.status = st; else delete tag.dataset.status;
    tag.textContent = word;
    $('#job-run').textContent = job.run;
    $('#job-failed').textContent = 0;
    $('#job-left').textContent = ended ? '–' : `${Math.ceil((JOB.tests - job.run) / JOB.pace)} s`;
    $('#job-pause').textContent = job.state === 'paused' ? 'resume' : 'pause';
    $('#job-pause').hidden = ended; $('#job-cancel').hidden = ended;
    Rekta.disable($('#job-again'), ended ? null : 'It is still running: cancel it first.');
  }
  setInterval(() => {
    if (job.state !== 'running' || !live()) return;
    job.run = Math.min(JOB.tests, job.run + JOB.pace);
    if (job.run >= JOB.tests) job.state = 'done';
    paintJob();
  }, 1000);
  $('#job-pause').addEventListener('click', () => { job.state = job.state === 'paused' ? 'running' : 'paused'; paintJob(); });
  $('#job-cancel').addEventListener('click', async () => {
    if (!(await Rekta.confirm({ kicker: 'nightly build', title: 'Cancel the build?', text: `${JOB.tests - job.run} tests have not run yet. Its results so far stay.`, ok: 'cancel the build', cancel: 'keep it running', safe: 'cancel' }))) return;
    job.state = 'cancelled'; paintJob(); $('#job-again').focus();
    toasts.show('The nightly build is cancelled.', { key: 'job' });
  });
  $('#job-again').addEventListener('click', () => { job = { run: 0, state: 'running' }; paintJob(); $('#job-pause').focus(); });
  paintJob();

  /* ---------- inputs: a field that cannot change now, and says why ---------- */
  Rekta.disable($('#keep-days'), 'Your organisation sets this: logs are kept 30 days.');
  $('#keep-days').readOnly = true;

  /* ---------- data: the beads, for a long wait that holds the whole screen ---------- */
  $('#startup').addEventListener('click', (e) => {
    const b = e.currentTarget;
    if (b.getAttribute('aria-disabled') === 'true') return;
    b.setAttribute('aria-disabled', 'true'); b.textContent = 'starting up…';
    $('#beads').hidden = false; $('#startup-msg').textContent = 'Starting up. This takes a while; nothing else can happen meanwhile.';
    setTimeout(() => { $('#beads').hidden = true; b.removeAttribute('aria-disabled'); b.textContent = 'start up'; $('#startup-msg').textContent = 'Started.'; toasts.show('Started up.', { key: 'startup', from: b }); }, 4000);
  });

  /* ---------- a whole system: a desktop, its bar, notifications, quick settings, the session, the lock screen ---------- */
  // Picture icons stand for what Rekta did not draw: another program's icon, a sender's photo. Coloured and off the
  // grid, they keep their own colours, on a neutral ground only. Here they are drawn from the content colours, so the
  // page needs no image files; the letters are set in the ink of the theme the page opened in.
  const PICS = { chat: ['content-teal', 'C'], ana: ['content-rose', 'AR'] };
  function pictureOf(name) {
    const [fill, letters] = PICS[name];
    const svg = `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 48 48"><rect width="48" height="48" rx="12" fill="${tok.value(`--rk-${fill}`)}"/>`
      + `<text x="24" y="31" font-family="sans-serif" font-size="${letters.length > 1 ? 18 : 24}" font-weight="600" text-anchor="middle" fill="${tok.value('--rk-text-primary')}">${letters}</text></svg>`;
    return `data:image/svg+xml,${encodeURIComponent(svg)}`;
  }
  const pictures = (scope) => $$('img[data-pic]', scope).forEach((img) => { img.src = pictureOf(img.dataset.pic); });
  pictures(document);
  const APPS = {
    chat: { name: 'Chat', pic: 'chat', title: 'Ana Ruiz', facts: 'The build is green again. Shall we tag it after lunch?' },
    notes: { name: 'Notes', icon: 'edit', title: 'Shopping', facts: 'coffee beans · oat milk · a new notebook' },
    files: { name: 'Files', icon: 'folder', title: 'Shared album', facts: '128 photos · 3 uploading' },
    music: { name: 'Music', icon: 'music', title: 'Calm, side B', facts: 'track 2 of 6 · 3 min 12 s left' },
    terminal: { name: 'Terminal', icon: 'window', title: 'make test', facts: '412 passed · none failed · 38 s' },
    calendar: { name: 'Calendar', icon: 'calendar', title: 'Review with Ana', facts: 'today, 15:00 to 15:30 · small room' },
  };
  const appIcon = (id) => { if (APPS[id].pic) { const i = document.createElement('img'); i.className = 'rk-picture'; i.alt = ''; i.src = pictureOf(APPS[id].pic); return i; } return Rekta.icon(APPS[id].icon); };
  const deskStart = () => ({ running: ['notes', 'chat'], used: ['notes', 'chat'], front: 'notes', min: [], max: false, active: true });
  let desk = deskStart();
  const bar = $('#screen-bar'), win = $('#win');
  const runOf = (id) => $(`.rk-ab-run[data-app="${id}"]`, bar);
  // The window shows the program in front; the bar marks it; the desktop says so when nothing is open.
  function paintDesk() {
    const shown = desk.front && !desk.min.includes(desk.front) ? desk.front : null, a = shown && APPS[shown];
    win.hidden = !shown;
    $('#desk-empty').hidden = !!desk.running.length;
    for (const b of $$('.rk-ab-run', bar)) { if (b.dataset.app === shown && desk.active) b.setAttribute('aria-current', 'true'); else b.removeAttribute('aria-current'); }
    $('#win-bar').toggleAttribute('data-rk-inactive', !desk.active);
    if (a) {
      $('#win-name').replaceChildren(Object.assign(document.createElement('b'), { textContent: a.name })); $('#win-place').textContent = a.title;
      const [k, t, f] = $('#win-body').children; k.textContent = a.name.toLowerCase(); t.textContent = a.title; f.textContent = a.facts;
    }
    win.toggleAttribute('data-max', desk.max);
    const word = desk.max ? 'restore' : 'maximize', mx = $('#win-max');
    mx.setAttribute('aria-label', word); mx.dataset.rkTip = word; $('use', mx).setAttribute('href', `#rk-i-${word}`);
    $('#apps-facts').textContent = `${Object.keys(APPS).length} programs · ${desk.running.length} running`;
    paintRunning($('#apps-running'), desk.running);
  }
  // A preview tile: what the program shows, small, with its icon and name at the foot.
  function previewTile(id) {
    const a = APPS[id], t = document.createElement('button');
    t.type = 'button'; t.className = 'rk-tile rk-tile--preview'; t.dataset.app = id;
    t.setAttribute('aria-label', `${a.name}, ${a.title}`);
    t.innerHTML = '<span class="rk-tile-preview"><span class="mock" aria-hidden="true"><span></span><span></span></span></span><span class="rk-tile-foot"><span></span></span>';
    const [title, facts] = $('.mock', t).children; title.textContent = a.title; facts.textContent = a.facts;
    const foot = $('.rk-tile-foot', t); foot.prepend(appIcon(id)); foot.lastChild.textContent = a.name;
    return t;
  }
  const grids = new Map();
  function paintRunning(el, ids) {
    el.replaceChildren(...ids.map(previewTile));
    if (!grids.has(el)) grids.set(el, Rekta.grid(el)); else grids.get(el).refresh();
  }
  function toFront(id) {
    if (!desk.running.includes(id)) {
      desk.running.push(id);
      const b = document.createElement('button');
      b.type = 'button'; b.className = 'rk-ab rk-ab-run'; b.dataset.app = id;
      b.append(appIcon(id), Object.assign(document.createElement('span'), { textContent: APPS[id].name }));
      const runs = $$('.rk-ab-run', bar);
      if (runs.length) runs[runs.length - 1].after(b); else bar.prepend(b);
      Rekta.enter(b);
    }
    desk.min = desk.min.filter((x) => x !== id);
    desk.used = [id, ...desk.used.filter((x) => x !== id)];
    desk.front = id; desk.active = true;
    paintDesk();
  }
  const nextFront = () => desk.used.find((x) => desk.running.includes(x) && !desk.min.includes(x)) || null;
  // Choosing the program in front minimises it; choosing another brings it to the front.
  bar.addEventListener('click', (e) => {
    const b = e.target.closest('.rk-ab-run');
    if (!b) return;
    if (b.getAttribute('aria-current') === 'true') $('#win-min').click(); else toFront(b.dataset.app);
  });
  $('#win-min').addEventListener('click', () => { desk.min.push(desk.front); desk.front = nextFront(); desk.active = !!desk.front; paintDesk(); });
  // Maximising and restoring re-arrange: the window grows or shrinks and its content fades back in (resize, re-flow).
  $('#win-max').addEventListener('click', () => Rekta.reflow($('#desk'), () => { desk.max = !desk.max; desk.active = true; paintDesk(); }, { items: [win] }));
  $('#win-close').addEventListener('click', async () => {
    const id = desk.front, b = runOf(id);
    desk.running = desk.running.filter((x) => x !== id); desk.used = desk.used.filter((x) => x !== id);
    desk.front = nextFront(); desk.active = !!desk.front;
    paintDesk();
    if (b) { const a = await Rekta.leave(b); b.remove(); if (a) a.cancel(); }
  });
  // A press on the desktop makes the window inactive: its name steps back; a press on the window makes it active.
  $('#desk').addEventListener('pointerdown', (e) => {
    const on = !!e.target.closest('#win');
    if (on !== desk.active) { desk.active = on; paintDesk(); }
  });
  Rekta.disable($('#sb-cast'), 'no screen to cast to nearby');
  const screenBar = Rekta.appbar(bar);
  // The clock: the time over the date; it opens this month, to read.
  const MONTHS = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December'];
  const DAYS = [['Mo', 'Monday'], ['Tu', 'Tuesday'], ['We', 'Wednesday'], ['Th', 'Thursday'], ['Fr', 'Friday'], ['Sa', 'Saturday'], ['Su', 'Sunday']];
  const hhmm = (d) => `${String(d.getHours()).padStart(2, '0')}:${String(d.getMinutes()).padStart(2, '0')}`;
  function paintClock() {
    const d = new Date(), day = DAYS[(d.getDay() + 6) % 7][1];
    $('#clock-time').textContent = hhmm(d); $('#clock-date').textContent = `${day.slice(0, 3)} ${d.getDate()} ${MONTHS[d.getMonth()].slice(0, 3)}`;
    $('#clock').setAttribute('aria-label', `${hhmm(d)}, ${day} ${d.getDate()} ${MONTHS[d.getMonth()]}: this month`);
    $('#lock-time').textContent = hhmm(d); $('#lock-date').textContent = `${day}, ${d.getDate()} ${MONTHS[d.getMonth()]}`;
    const first = (new Date(d.getFullYear(), d.getMonth(), 1).getDay() + 6) % 7, days = new Date(d.getFullYear(), d.getMonth() + 1, 0).getDate();
    const cells = [...Array(first).fill('<td></td>'), ...Array.from({ length: days }, (_, i) => (i + 1 === d.getDate() ? `<td aria-current="date">${i + 1}</td>` : `<td>${i + 1}</td>`))];
    while (cells.length % 7) cells.push('<td></td>');
    const rows = Array.from({ length: cells.length / 7 }, (_, r) => `<tr>${cells.slice(r * 7, r * 7 + 7).join('')}</tr>`).join('');
    $('#month').innerHTML = `<caption>${MONTHS[d.getMonth()]} ${d.getFullYear()}</caption><thead><tr>${DAYS.map(([s, l]) => `<th scope="col" abbr="${l}">${s}</th>`).join('')}</tr></thead><tbody>${rows}</tbody>`;
  }
  paintClock(); setInterval(paintClock, 15000);
  $('#clock-cal').addEventListener('toggle', (e) => { if (e.newState === 'open') Rekta.place($('#clock-cal'), $('#clock')); });
  // The volume keys: the level in a toast, or for a moment in place of the clock (indicate()); one form at a time.
  let volume = 0.6, levelIn = 'toast';
  const level = document.createElement('span');
  level.className = 'level';
  level.innerHTML = '<span class="level-icon"></span><span class="rk-meter"></span><b></b>';
  function setVolume(v, { show = true } = {}) {
    volume = Math.round(Math.max(0, Math.min(1, v)) * 10) / 10;
    $('#qs-vol').value = volume * 100;
    $('#qs-vol').dispatchEvent(new Event('input', { bubbles: true }));
    if (!show) return;
    const pct = `${Math.round(volume * 100)}%`, glyph = volume ? 'speaker' : 'speaker-muted';
    if (levelIn === 'toast') { toasts.level('volume', volume, { icon: glyph }); return; }
    level.firstChild.replaceChildren(Rekta.icon(glyph));
    level.children[1].style.setProperty('--rk-v', volume);
    level.lastChild.textContent = pct;
    Rekta.indicate($('#clock'), level);
    $('#level-said').textContent = `volume ${pct}`;
  }
  $('#vol-down').addEventListener('click', () => setVolume(volume - 0.1));
  $('#vol-up').addEventListener('click', () => setVolume(volume + 0.1));
  $$('input[name=level-in]').forEach((r) => r.addEventListener('change', () => { levelIn = r.value; }));
  $('#qs-vol').addEventListener('input', (e) => { if (e.isTrusted) volume = +e.target.value / 100; });
  // In the background: one control for the programs without a window, each with its own menu as a second level.
  let playing = true;
  Rekta.menu($('#sb-bg'), $('#bg-menu'), {
    onSelect(item) {
      const act = item.dataset.act;
      if (act === 'pause') { playing = !playing; item.lastChild.textContent = playing ? 'pause' : 'play'; $('use', item).setAttribute('href', `#rk-i-${playing ? 'pause' : 'play'}`); $('#bg-menu .rk-menu-key').textContent = playing ? 'playing' : 'paused'; }
      const said = { previous: 'Music: the track before.', next: 'Music: the next track.', pause: playing ? 'Music plays again.' : 'Music paused.', sync: 'Sync: everything is up to date.', folder: 'Sync: the folder opens in Files.' }[act];
      if (said) toasts.show(said, { key: 'background' });
      if (act === 'folder') toFront('files');
    },
  });
  // The notification centre: newest on top, each removable by its button, Delete or a swipe, every removal undone.
  const centre = $('#centre'), CENTRE = centre.innerHTML;
  let upload = 20;
  function paintCentre() {
    const n = $$(':scope > li', centre).length;
    $('#centre-none').hidden = n > 0; $('#centre-all').hidden = !n;
    const count = $('#sb-count');
    if (count.textContent !== String(n)) Rekta.tileUpdate($('#sb-notes'), () => { count.textContent = n; });
  }
  function dismiss(items, left) {
    const where = items.map((li) => [li, li.nextElementSibling]);
    const label = items.length === 1 ? `Dismissed “${$('b', items[0]).textContent}”` : `Dismissed ${items.length} notifications`;
    let first = true;
    hist.do({
      label,
      run: async () => {
        const gone = first && left;
        first = false;
        const anims = gone ? [] : await Promise.all(items.map((li) => Rekta.leave(li)));
        Rekta.reflow(centre, () => { items.forEach((li) => li.remove()); anims.forEach((a) => a && a.cancel()); paintCentre(); }, { size: false, items: $$(':scope > li', centre).filter((li) => !items.includes(li)) });
      },
      undo: () => {
        Rekta.reflow(centre, () => {
          for (const [li, next] of where.slice().reverse()) { li.removeAttribute('style'); if (next && next.isConnected) next.before(li); else centre.append(li); }
          paintCentre();
        }, { size: false });
        items.forEach(Rekta.enter);
      },
    });
  }
  const centreRemove = Rekta.removable(centre, { onRemove: (li, { left }) => dismiss([li], left) });
  $('#centre-all').addEventListener('click', () => dismiss($$(':scope > li', centre), false));
  // A tap on a row, not on its button, hints at the swipe nobody can see: the row moves the way it would go.
  centre.addEventListener('click', (e) => {
    const li = e.target.closest('#centre > li');
    if (li && !e.target.closest('button, a')) Rekta.nudge(li, getComputedStyle(centre).direction === 'rtl' ? 'left' : 'right');
  });
  function arrive({ pic = '', icon: glyph = '', title, text }) {
    const li = document.createElement('li');
    li.innerHTML = '<span class="centre-text"><b></b><span></span></span><span class="rk-meta centre-time"></span><button class="rk-btn rk-btn--icon rk-btn--quiet" type="button" data-rk-remove aria-label="dismiss" data-rk-tip="dismiss"></button>';
    li.prepend(pic ? Object.assign(document.createElement('img'), { className: 'rk-picture', alt: '', src: pictureOf(pic) }) : Rekta.icon(glyph));
    $('b', li).textContent = title; $('.centre-text > span', li).textContent = text; $('.centre-time', li).textContent = hhmm(new Date());
    $('[data-rk-remove]', li).append(Rekta.icon('close'));
    Rekta.reflow(centre, () => centre.prepend(li), { size: false });
    Rekta.enter(li);
    paintCentre();
    return li;
  }
  const MESSAGES = ['Are we still on for 3?', 'I moved the review to the small room.', 'Tagged. Thank you!'];
  let said = 0;
  $('#n-message').addEventListener('click', () => {
    const text = MESSAGES[said++ % MESSAGES.length];
    arrive({ pic: 'ana', title: 'Ana Ruiz', text });
    toasts.show(text, { key: `message-${said}`, edge: true, title: 'Ana Ruiz', picture: pictureOf('ana'), open: () => { toFront('chat'); home.goTo('system'); } });
  });
  $('#n-backup').addEventListener('click', () => {
    const li = arrive({ icon: 'download', title: 'Backup', text: 'The backup is complete: 412 files saved.' });
    toasts.show('The backup is complete: 412 files saved.', { key: 'backup', edge: true, title: 'Backup', icon: 'download', open: () => $('[data-rk-remove]', li).focus() });
  });
  $('#sb-notes').addEventListener('click', () => { const b = $('#centre [data-rk-remove]'); if (b) b.focus(); else $('#centre-none').scrollIntoView({ block: 'nearest' }); });
  setInterval(() => {
    const bar2 = $('#upload-bar');
    if (!bar2 || !live() || upload >= 100) return;
    upload = Math.min(100, upload + 10); bar2.value = upload;
    if (upload >= 100) { const t = $('#upload-tag'); t.dataset.status = 'success'; t.textContent = 'done'; bar2.remove(); }
  }, 1500);
  paintCentre();
  // Quick settings: a side panel from the trailing edge; a group gives its place to the list it opens.
  const quick = $('#quick-panel'), quickIn = Rekta.inPlace(quick);
  $('#sb-quick').addEventListener('click', () => quick.showModal());
  quick.addEventListener('close', () => quickIn.reset());
  for (const [name, out] of [['qs-net', '#qs-net-v'], ['qs-out', '#qs-out-v']]) {
    $$(`input[name=${name}]`).forEach((r) => r.addEventListener('change', () => { $(out).textContent = r.value; quickIn.back(); }));
  }
  // The session: lock, switch, sign out, restart, power off; a choice, not a question.
  $('#session-open').addEventListener('click', () => $('#session').showModal());
  $('#session').addEventListener('close', () => {
    const v = $('#session').returnValue;
    if (v === 'lock' || v === 'switch' || v === 'out') lockScreen();
    else if (v === 'restart' || v === 'off') toasts.show('Nothing restarts here: this desktop only shows how.', { key: 'session' });
  });
  // Locking: the windows step back, then the lock screen comes from above, whole, as a sheet does.
  const behind = () => [$('#desk'), bar];
  async function lockScreen() {
    const d = tok.ms('--rk-duration-moderate'), to = { transform: `scale(${tok.number('--rk-step-back-scale')})`, filter: `brightness(${tok.number('--rk-step-back-brightness')})` };
    const back = behind().map((el) => el.animate([{ transform: 'none', filter: 'none' }, to], { duration: d, easing: tok.value('--rk-ease-out'), fill: 'forwards' }));
    await Promise.all(back.map((a) => a.finished.catch(() => {})));
    behind().forEach((el) => { el.inert = true; });
    $('#lock').hidden = false;
    $('#lock').animate([{ opacity: 0, transform: `translateY(-16px)` }, { opacity: 1, transform: 'none' }], { duration: tok.ms('--rk-duration-short'), easing: tok.value('--rk-ease-in') });
    back.forEach((a) => a.cancel());
    lockForm.clear(); $('#lock-pw').value = ''; $('#lock-pw').focus({ preventScroll: true });
  }
  const lockForm = Rekta.form($('#lock-form'), {
    validators: { 'lock-pw': (v) => (v ? null : 'Type the password. Any will do here.') },
    async onSubmit() {
      $('#lock-dots').hidden = false;
      await wait(1500);
      $('#lock-dots').hidden = true;
      // Signed in: the lock screen fades to the ground and the desktop is there again.
      await $('#lock').animate([{ opacity: 1 }, { opacity: 0 }], { duration: tok.ms('--rk-duration-moderate'), easing: tok.value('--rk-ease-in') }).finished.catch(() => {});
      $('#lock').hidden = true;
      behind().forEach((el) => { el.inert = false; });
      $('#session-open').focus({ preventScroll: true });
      toasts.show('Welcome back, Ada.', { key: 'session' });
    },
  });
  $('#lock-switch').addEventListener('click', () => toasts.show('Ada is the only account on this desktop.', { key: 'session' }));
  // The recent stack: the running programs, the last used first; the one before the current one has focus, so one
  // press switches back. A press or Enter brings it to the front; Escape closes and nothing changes.
  const recent = $('#recent');
  $('#recent-open').addEventListener('click', () => {
    if (!desk.used.length) { toasts.show('Nothing is running.', { key: 'recent' }); return; }
    paintRunning($('#recent-tiles'), desk.used);
    recent.showModal();
    const tiles = $$('#recent-tiles > .rk-tile');
    (tiles[1] || tiles[0]).focus();
  });
  recent.addEventListener('click', (e) => {
    const t = e.target.closest('.rk-tile');
    if (t) { recent.close(); toFront(t.dataset.app); } else if (e.target === recent) recent.close();
  });
  // All apps: a launch tile opens its program on the desktop; a preview tile brings it to the front.
  const openApp = (id) => { toFront(id); router.navigate('home').then(() => home.goTo('system')); };
  $('#apps-grid').addEventListener('click', (e) => { const t = e.target.closest('[data-app]'); if (t) openApp(t.dataset.app); });
  $('#apps-running').addEventListener('click', (e) => { const t = e.target.closest('[data-app]'); if (t) openApp(t.dataset.app); });
  $('#apps-fills').addEventListener('change', (e) => { $('#apps-grid').dataset.rkFills = e.target.checked ? 'content' : 'plain'; });
  paintDesk();
  function resetSystem() {
    for (const b of $$('.rk-ab-run', bar)) if (!['notes', 'chat'].includes(b.dataset.app)) b.remove();
    desk = deskStart(); paintDesk();
    centre.innerHTML = CENTRE; pictures(centre); upload = 20; paintCentre();
    job = { run: 0, state: 'running' }; paintJob();
    $('#apps-fills').checked = true; $('#apps-grid').dataset.rkFills = 'content';
    if (!$('#lock').hidden) { $('#lock').hidden = true; behind().forEach((el) => { el.inert = false; }); }
  }

  /* ---------- sheet and reset ---------- */
  const sheet = $('#sheet');
  $('#reset').addEventListener('click', () => sheet.showModal());
  sheet.addEventListener('close', () => {
    if (sheet.returnValue !== 'reset') return;
    toasts.dismiss();
    ORDER.forEach((n) => banner.set(n, false)); paintState();
    compTable.clear(); $$('#comp-body tr[data-archived]').forEach((r) => r.removeAttribute('data-archived')); clearComp(); pick.clear(); logList.clear();
    const order = new Map(STEP_ORDER.map((n, i) => [n, i])); $$('#steps > li').sort((a, b) => order.get(a.querySelector('b').textContent) - order.get(b.querySelector('b').textContent)).forEach((li) => $('#steps').appendChild(li));
    hist.clear(); $('#lv-feed').checked = true; $('#lv-online').checked = true; feed.offline(false);
    tokTable.clear(); $('#tok-q').value = ''; tokGroup.set(() => false); tokFilter({ write: false });
    customAccent = null; window.siteAccent(null); store.set('rekta-site-accent-def', null); prefs.set('accent', 'product');
    $('#launcher-fills').checked = true; $('#launcher').dataset.rkFills = 'content';
    $('#a-name').value = ''; $('#a-light').value = ''; $('#a-dark').value = ''; accentForm.clear(); accentGuard.clean();
    presses = 0; $('#presses').textContent = '0'; seen.clear(); seen.add(home.current ? home.current.id : 'overview'); paintSeen(); $$('[data-pinned]', liveTiles).forEach((t) => t.remove()); pins = 0; paintPins();
    facets.select(0);
    $('#text-size').value = 100; $('#text-size').dispatchEvent(new Event('input', { bubbles: true }));
    $('#builder').reset(); build(); tokenFind.set(null); $('#files').replaceChildren(); $('#files').hidden = true;
    paintAccent(); resetSystem();
    toasts.show('Demo reset. Everything is back at the start.', { key: 'reset' });
  });

  paintState();
  paintAccent();
  paintSeen();
  requestAnimationFrame(() => { measure(); env(); });
  window.site = { app, router, home, facets, toasts, banner, hist, pwa: app2, lineChart, barChart, compTable, pick, files, steps, logList, feed, tokenFind, tokTable, tokSplit, palette, accentForm, accentGuard, screenBar, quickIn, centreRemove, lockForm, desk: () => desk };
})();

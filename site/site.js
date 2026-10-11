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

  const app = Rekta.start();
  const router = Rekta.places({ home: 'home', title: 'Rekta Design' });
  const home = Rekta.panorama($('#pan'), { pivot: $('#nav') });
  const facets = Rekta.pivot($('#facet-strip'));
  const toasts = Rekta.toaster($('#toasts'));

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
  const hexRgb = (h) => [1, 3, 5].map((i) => parseInt(h.slice(i, i + 2), 16));
  function measure() {
    for (const b of $$('[data-contrast]')) {
      const [fg, bg] = b.dataset.contrast.split(' '), r = ratio(rgb(fg), rgb(bg));
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

  /* ---------- look: theme, density, motion (Rekta prefs) and the accent slot ---------- */
  function paintPrefs() {
    for (const name of ['theme', 'density', 'motion']) { const r = $(`input[name=${name}][value="${prefs.get(name)}"]`); if (r) r.checked = true; }
    $('#theme-word').textContent = prefs.get('theme');
    const m = prefs.get('motion');
    $('#motion-word').textContent = m === 'system' ? (reduced.matches ? 'reduced' : 'on') : m;
    $('#m-off').checked = m === 'off' || (m === 'system' && reduced.matches);
    const a = root.dataset.accent === 'plum' ? 'plum' : root.dataset.accent === 'custom' ? 'custom' : 'ink';
    $(`input[name=accent][value="${a}"]`).checked = true;
    measure(); env();
  }
  document.addEventListener('rk:prefs', paintPrefs);
  reduced.addEventListener('change', paintPrefs);
  matchMedia('(prefers-color-scheme: dark)').addEventListener('change', paintPrefs);
  for (const name of ['theme', 'density', 'motion']) $$(`input[name=${name}]`).forEach((r) => r.addEventListener('change', () => prefs.set(name, r.value)));
  $$('input[name=accent]').forEach((r) => r.addEventListener('change', () => {
    if (r.value === 'plum') { root.dataset.accent = 'plum'; store.set('rekta-site-accent', 'plum'); }
    else if (r.value === 'custom' && customAccent) { window.siteAccent(customAccent); store.set('rekta-site-accent', 'custom'); }
    else { delete root.dataset.accent; store.set('rekta-site-accent', null); }
    paintPrefs();
  }));
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
  function points(h) {
    if (h.length < 2) return '0,14 200,14';
    const lo = Math.min(...h), span = Math.max(...h) - lo || 1;
    return h.map((v, i) => `${(i * 200 / (h.length - 1)).toFixed(1)},${(26 - (v - lo) / span * 24).toFixed(1)}`).join(' ');
  }
  function drawLive(key, value, label) {
    for (const tile of $$(`[data-live=${key}]`)) {
      $('[data-v]', tile).textContent = value;
      const svg = $('.rk-spark', tile), h = series[key];
      $('polyline', svg).setAttribute('points', points(h));
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
  const seenWatch = () => { const c = home.current; if (c && !seen.has(c.id)) { seen.add(c.id); paintSeen(); } };
  $('#pan').addEventListener('scroll', seenWatch, { passive: true });
  addEventListener('scroll', seenWatch, { passive: true });
  $('#nav').addEventListener('click', () => setTimeout(seenWatch, 50));
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
    setTimeout(() => place.removeAttribute('data-rk-arriving'), tok.ms('--rk-duration-moderate') + 10 * tok.ms('--rk-duration-stagger') + tok.ms('--rk-duration-long') + 50);
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
    ['Live contract', 'feedback', 'warning', 'next'], ['Combobox', 'forms', 'warning', 'next'], ['Undo history', 'feedback', 'warning', 'next'],
  ];
  const compHtml = COMPONENTS.map(([name, group, status, since]) => `<tr data-name="${name}" data-status-v="${status}"><td class="rk-check"></td><th scope="row">${name}</th><td>${group}</td><td><span class="rk-tag" data-status="${status}">${WORD[status]}</span></td><td class="rk-num" data-sort="${since === 'next' ? 9 : since}">${since}</td></tr>`).join('');
  $('#comp-body').innerHTML = compHtml;
  const compTable = Rekta.table($('#comp-table'), {
    appbar: $('#bar'),
    onShow(row) { const [n, g, s, v] = COMPONENTS.find((c) => c[0] === row.dataset.name); $('#comp-shown').textContent = `Shown: ${n} · ${g} · ${WORD[s]} · since ${v}`; },
    onSelect(rows, { mode }) { if (mode) $('#comp-shown').textContent = `${rows.length} selected · the app bar carries them`; else if (!rows.length) $('#comp-shown').textContent = 'Nothing selected.'; },
  });
  function compFilter() {
    const q = $('#comp-q').value.trim().toLowerCase(), st = $$('#comp-status-menu [aria-checked="true"]').map((i) => i.dataset.v);
    let shown = 0;
    for (const tr of $$('#comp-body tr')) { const keep = (!q || tr.textContent.toLowerCase().includes(q)) && (!st.length || st.includes(tr.dataset.statusV)); tr.hidden = !keep; if (keep) shown += 1; }
    $('#comp-none').hidden = shown > 0;
    $('#comp-clear').hidden = !(q || st.length);
    $('#comp-facts').textContent = `${COMPONENTS.length} components${shown < COMPONENTS.length ? ` · showing ${shown}` : ''} · ${COMPONENTS.filter((c) => c[2] === 'warning').length} waiting`;
    compTable.refresh();
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
    const from = [tokTable, compTable, pick].find((s) => s.mode) || compTable, rows = from.selected();
    const text = from === tokTable ? rows.map((r) => `var(--rk-${r.dataset.name})`).join('\n') : rows.map((r) => r.dataset.name).join(', ');
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

  /* ---------- forms: make an accent, checked late, guarded ---------- */
  let customAccent = store.get('rekta-site-accent-def');
  const HEX = /^#[0-9a-f]{6}$/i;
  const onFor = (hex) => (ratio(hexRgb(hex), [255, 255, 255]) >= ratio(hexRgb(hex), [1, 2, 5]) ? '#ffffff' : '#010205');
  function hexCheck(v, ground, need, groundName) {
    const t = v.trim();
    if (!HEX.test(t)) return 'Write a hex colour with six digits, like #1f6f5c.';
    const r = ratio(hexRgb(t), ground);
    if (r < need) return `${t} holds only ${r.toFixed(1)}:1 on the ${groundName} ground; an accent needs ${need}:1. Try a ${groundName === 'light' ? 'darker' : 'lighter'} one.`;
    return null;
  }
  const accentForm = Rekta.form($('#accent-form'), {
    validators: {
      'a-name': (v) => (!v.trim() ? 'Give the accent a name.' : /^[a-z][a-z0-9-]*$/.test(v.trim()) ? null : 'Use lowercase letters, digits and dashes, like "sea-green".'),
      'a-light': (v) => hexCheck(v, hexRgb('#ffffff'), 3, 'light'),
      'a-dark': (v) => (v.trim() ? hexCheck(v, hexRgb('#0e0e12'), 3, 'dark') : null),
    },
    async onSubmit(values) {
      await wait(800);
      if (banner.is('readonly')) throw { message: 'Could not apply: maintenance holds every change until 15:00. Your accent is still here.' };
      if (values['a-name'].trim() === 'plum') throw { fields: { 'a-name': 'An accent called "plum" already exists. Choose another name.' } };
      const light = values['a-light'].trim(), dark = values['a-dark'].trim();
      customAccent = { name: values['a-name'].trim(), light, onLight: onFor(light), dark: dark || null, onDark: dark ? onFor(dark) : null };
      window.siteAccent(customAccent);
      store.set('rekta-site-accent-def', customAccent); store.set('rekta-site-accent', 'custom');
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
    customAccent = null; window.siteAccent(null); store.set('rekta-site-accent-def', null); store.set('rekta-site-accent', null); paintAccent();
    toasts.show('Accent removed.', { key: 'accent' });
  });
  if (customAccent) { $('#a-name').value = customAccent.name; $('#a-light').value = customAccent.light; $('#a-dark').value = customAccent.dark || ''; accentGuard.clean(); }

  /* ---------- inputs: build a tile, find a token, text size, a date, files ---------- */
  const built = $('#built');
  function build() {
    const st = $('#b-status').value, size = ($$('input[name=b-size]').find((r) => r.checked) || {}).value;
    if (st) built.dataset.status = st; else delete built.dataset.status;
    built.className = `rk-tile${size === 'w2' ? ' rk-w2' : size === 'h2' ? ' rk-w2 rk-h2' : ''}`;
    $('.rk-tile-word', built).textContent = st ? WORD[st] : 'idle';
    $('.rk-stat b', built).textContent = $('#b-value').value || '0';
    $('.rk-stat span', built).textContent = $('#b-label').value.trim() || 'no label';
    let spark = $('.rk-spark', built);
    if ($('#b-spark').checked && !spark && st !== 'success') {
      built.insertAdjacentHTML('beforeend', '<svg class="rk-spark" viewBox="0 0 200 28" role="img" aria-label="rising over the last 8 hours"><polyline pathLength="1" points="0,24 30,20 60,22 90,14 120,16 150,9 180,11 200,4"/></svg>');
    } else if ((!$('#b-spark').checked || st === 'success') && spark) spark.remove();
  }
  $('#builder').addEventListener('input', build);
  $('#builder').addEventListener('change', build);
  $('#builder').addEventListener('submit', (e) => e.preventDefault());
  const tokenFind = Rekta.combobox($('#find-token'), {
    options: () => TOKENS.map((t) => ({ label: `--rk-${t.name}`, value: t.name, hint: t.group, group: t.group, keywords: `${t.name} ${t.value || ''}` })),
    onSelect(o) {
      const t = o && TOKENS.find((x) => x.name === o.value);
      $('#find-hint').textContent = t ? `${t.value}${t.alt && t.alt !== t.value ? ` · ${t.altLabel} ${t.alt}` : ''}: ${t.description || 'a token of Rekta Design.'}` : 'A combobox: it filters as you type and keeps only a choice from its list.';
    },
  });
  $('#text-size').addEventListener('input', (e) => { root.style.fontSize = +e.target.value === 100 ? '' : `${e.target.value}%`; env(); });
  const today = new Date(); today.setDate(today.getDate() + 3);
  $('#when-d').value = today.toISOString().slice(0, 10);
  function when() {
    const d = $('#when-d').value, t = $('#when-t').value || '09:00';
    if (!d) { $('#when-hint').textContent = 'No date: nothing to remind you of.'; return; }
    const at = new Date(`${d}T${t}`), days = Math.round((at - Date.now()) / 864e5);
    $('#when-hint').textContent = at < Date.now() ? 'That has passed: choose a later time.' : `${days === 0 ? 'Today' : days === 1 ? 'Tomorrow' : `In ${days} days`}, at ${t}.`;
  }
  $('#when-d').addEventListener('input', when); $('#when-t').addEventListener('input', when); when();
  const LIMIT = 4 * 1024 * 1024;
  const kb = (n) => (n < 1024 * 1024 ? `${Math.max(1, Math.round(n / 1024))} kB` : `${(n / 1024 / 1024).toFixed(1)} MB`);
  Rekta.drop($('#drop'), {
    onFiles(files) {
      const list = $('#files');
      list.hidden = false;
      for (const f of files) {
        const li = document.createElement('li');
        li.innerHTML = `<b></b><span class="rk-tag" data-status="warning">waiting</span><span class="rk-meta"></span><progress class="rk-progress" max="100" value="0"></progress>`;
        $('b', li).textContent = f.name; $('.rk-meta', li).textContent = kb(f.size);
        list.prepend(li);
        if (f.size > LIMIT) { const tag = $('.rk-tag', li); tag.dataset.status = 'error'; tag.textContent = 'failed'; $('.rk-meta', li).textContent = `${kb(f.size)} · larger than ${kb(LIMIT)}`; $('progress', li).remove(); continue; }
        let v = 0;
        const tag = $('.rk-tag', li), bar = $('progress', li), step = Math.max(4, 40 - f.size / 50000);
        tag.dataset.status = 'info'; tag.textContent = 'active';
        const timer = setInterval(() => {
          v = Math.min(100, v + step); bar.value = v;
          if (v >= 100) { clearInterval(timer); tag.dataset.status = 'success'; tag.textContent = 'done'; bar.remove(); }
        }, 120);
      }
      toasts.show(`${files.length === 1 ? '1 file' : `${files.length} files`} added. Nothing leaves your device.`, { key: 'files' });
    },
  });

  /* ---------- sheet and reset ---------- */
  const sheet = $('#sheet');
  $('#reset').addEventListener('click', () => sheet.showModal());
  sheet.addEventListener('close', () => {
    if (sheet.returnValue !== 'reset') return;
    toasts.dismiss();
    ORDER.forEach((n) => banner.set(n, false)); paintState();
    compTable.clear(); clearComp(); pick.clear();
    tokTable.clear(); $('#tok-q').value = ''; tokGroup.set(() => false); tokFilter({ write: false });
    customAccent = null; window.siteAccent(null); store.set('rekta-site-accent-def', null); store.set('rekta-site-accent', null);
    $('#a-name').value = ''; $('#a-light').value = ''; $('#a-dark').value = ''; accentForm.clear(); accentGuard.clean();
    if (root.dataset.accent === 'plum') { delete root.dataset.accent; }
    presses = 0; $('#presses').textContent = '0'; seen.clear(); seen.add(home.current ? home.current.id : 'overview'); paintSeen();
    facets.select(0);
    $('#text-size').value = 100; $('#text-size').dispatchEvent(new Event('input', { bubbles: true }));
    $('#builder').reset(); build(); tokenFind.set(null); $('#files').replaceChildren(); $('#files').hidden = true;
    paintAccent();
    toasts.show('Demo reset. Everything is back at the start.', { key: 'reset' });
  });

  paintState();
  paintAccent();
  paintSeen();
  requestAnimationFrame(() => { measure(); env(); });
  window.site = { app, router, home, facets, toasts, banner, compTable, pick, tokenFind, tokTable, tokSplit, palette, accentForm, accentGuard };
})();

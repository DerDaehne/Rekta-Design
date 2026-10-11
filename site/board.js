/* Rekta Board: a demo app on rekta.js (window.Rekta). Only the demo's own data and wiring live here; press, the
   turnstile, routes and back, the panorama, pivots, toasts, the banner and sheets are Rekta's. */
(() => {
  'use strict';
  const { prefs, tokens } = Rekta;
  const $ = (s, el = document) => el.querySelector(s);
  const $$ = (s, el = document) => [...el.querySelectorAll(s)];
  const root = document.documentElement;
  const store = {
    get: (k) => { try { return localStorage.getItem(k); } catch { return null; } },
    set: (k, v) => { try { if (v == null) localStorage.removeItem(k); else localStorage.setItem(k, v); } catch { /* per visit only */ } },
  };
  const now = () => { const d = new Date(); return `${String(d.getHours()).padStart(2, '0')}:${String(d.getMinutes()).padStart(2, '0')}`; };

  const app = Rekta.start();
  const router = Rekta.places({ home: 'board', title: 'Rekta Board', onChange: ({ to }) => { if (to === 'queue') loadQueue(); } });
  const board = Rekta.panorama($('#pan'), { pivot: $('#nav') });
  const facets = Rekta.pivot($('#facet-strip'));
  const toasts = Rekta.toaster($('#toasts'));
  const wait = (msec) => new Promise((r) => setTimeout(r, msec));

  /* ---------- look: theme, density, motion (Rekta prefs) and the demo's accent ---------- */
  const reduced = matchMedia('(prefers-reduced-motion: reduce)');
  function paintPrefs() {
    for (const name of ['theme', 'density', 'motion']) { const r = $(`input[name=${name}][value="${prefs.get(name)}"]`); if (r) r.checked = true; }
    $('#theme-word').textContent = prefs.get('theme');
    const m = prefs.get('motion');
    $('#motion-word').textContent = m === 'system' ? (reduced.matches ? 'reduced' : 'on') : m;
    $(`input[name=accent][value="${root.dataset.accent === 'plum' ? 'plum' : 'ink'}"]`).checked = true;
  }
  document.addEventListener('rk:prefs', paintPrefs);
  reduced.addEventListener('change', paintPrefs);
  for (const name of ['theme', 'density', 'motion']) $$(`input[name=${name}]`).forEach((r) => r.addEventListener('change', () => prefs.set(name, r.value)));
  $$('input[name=accent]').forEach((r) => r.addEventListener('change', () => {
    if (r.value === 'plum') root.dataset.accent = 'plum'; else delete root.dataset.accent;
    store.set('rekta-board-accent', r.value === 'plum' ? 'plum' : null);
  }));
  const THEMES = ['system', 'light', 'dark'];
  $('#ab-theme').addEventListener('click', () => prefs.set('theme', THEMES[(THEMES.indexOf(prefs.get('theme')) + 1) % 3]));
  $('#ab-motion').addEventListener('click', () => prefs.set('motion', prefs.get('motion') === 'off' ? 'system' : 'off'));
  paintPrefs();

  /* ---------- status banner: four states, the most severe holds the banner ---------- */
  const STATES = {
    halted: { word: 'halted', status: 'error', alert: true, action: 'release stop',
      text: 'Emergency stop pressed: every running job stopped mid-step, nothing written. Check the target hosts, then release the stop.',
      on: 'Emergency stop pressed', off: 'Emergency stop released. 12 jobs resume from their last step.' },
    offline: { word: 'offline', status: 'warning', action: 'retry now',
      text: 'Lost the connection to the job server. Figures are frozen; it retries every 10 seconds.',
      on: 'Connection lost', off: 'Connected again. Figures are live.' },
    paused: { word: 'paused', status: 'warning', action: 'resume',
      text: 'You paused intake. Running jobs finish; new jobs wait in the queue and start in order when you resume.',
      on: 'Intake paused', off: 'Intake resumed. 7 queued jobs start in order.' },
    readonly: { word: 'read-only', since: 'until', until: '15:00',
      text: 'Maintenance on the job database. Everything stays visible; changes open again when it ends.',
      on: 'Maintenance started', off: 'Maintenance finished. Changes are open again.' },
  };
  const ORDER = ['halted', 'offline', 'paused', 'readonly'];
  const remedy = $('#remedy');
  let retrying = 0;
  const banner = Rekta.banner($('#banner'), {
    states: STATES, order: ORDER, appbar: $('#bar'), liveWord: $('#live'), toaster: toasts,
    onAction(name) {
      if (name === 'offline') {
        remedy.textContent = 'retrying…'; remedy.setAttribute('aria-disabled', 'true');
        retrying = setTimeout(() => setState('offline', false), 900);
        return;
      }
      setState(name, false, name === 'halted' ? $('#ab-stop') : name === 'paused' ? $('#ab-pause') : null);
    },
  });
  const liveData = () => !(banner.is('halted') || banner.is('offline') || banner.is('paused'));
  function paintState() {
    $('#ab-pause span').textContent = banner.is('paused') ? 'resume' : 'pause';
    $('#ab-stop span').textContent = banner.is('halted') ? 'release stop' : 'stop';
    ORDER.forEach((n) => { $(`#s-${n}`).checked = banner.is(n); });
    $('#state-now').textContent = banner.shown ? STATES[banner.shown].word : 'none';
    $('#data-now').textContent = liveData() ? 'updating' : 'frozen';
  }
  function setState(name, on, from) { clearTimeout(retrying); banner.set(name, on, { from }); paintState(); }
  ORDER.forEach((n) => $(`#s-${n}`).addEventListener('change', (e) => setState(n, e.target.checked, e.target)));
  $('#ab-pause').addEventListener('click', (e) => setState('paused', !banner.is('paused'), e.currentTarget));
  $('#ab-stop').addEventListener('click', (e) => setState('halted', !banner.is('halted'), e.currentTarget));
  const EVENTS = ['Deploy 2026.10.11-3 queued behind 2 runs', 'Incident #212 opened and on-call paged', 'Backup of the job database finished in 4 min'];
  let ev = 0;
  $('#event').addEventListener('click', () => { toasts.show(EVENTS[ev % EVENTS.length], { key: `event-${ev}` }); ev += 1; });

  /* ---------- live tiles and sparklines ---------- */
  const series = {
    queue: { start: 12, min: 3, max: 22, step: 2, digits: 0, unit: '' },
    coverage: { start: 87.4, min: 85, max: 90, step: 0.3, digits: 1, unit: '%' },
  };
  // Deterministic demo data (a seeded random walk), so every visit looks the same at rest.
  let seedState = 7;
  const rand = () => { seedState = (seedState * 1664525 + 1013904223) % 4294967296; return seedState / 4294967296; };
  function step(s) {
    s.value = Number(Math.min(s.max, Math.max(s.min, s.value + (rand() - 0.5) * 2 * s.step)).toFixed(s.digits));
    s.history.push(s.value);
    if (s.history.length > 24) s.history.shift();
  }
  function points(h) {
    const lo = Math.min(...h), span = Math.max(...h) - lo || 1;
    return h.map((v, i) => `${(i * 200 / (h.length - 1)).toFixed(1)},${(26 - (v - lo) / span * 24).toFixed(1)}`).join(' ');
  }
  function draw() {
    for (const [key, s] of Object.entries(series)) {
      const tile = $(`[data-live=${key}]`), text = s.value.toFixed(s.digits) + s.unit;
      const trend = s.value > s.history[0] ? 'rising' : s.value < s.history[0] ? 'falling' : 'flat';
      $('[data-v]', tile).textContent = text;
      const svg = $('.rk-spark', tile);
      $('polyline', svg).setAttribute('points', points(s.history));
      // On the tile the stat already gives the value; the line adds the trend.
      svg.setAttribute('aria-label', `${trend} over the last ${s.history.length} readings`);
      if (key !== 'queue') continue;
      const t = $('[data-trend=queue]'), big = $('[data-trend=queue-big]');
      $('polyline', t).setAttribute('points', points(s.history));
      t.setAttribute('aria-label', `build queue, ${text} jobs, ${trend}`);
      $('[data-trend-v=queue]').textContent = `${text} jobs`;
      $('polyline', big).setAttribute('points', points(s.history));
      big.setAttribute('aria-label', `build queue, ${text} jobs, ${trend} over the last ${s.history.length} readings`);
      $('#queue-facts').textContent = `${text} jobs · 3 running · ${liveData() ? 'live' : 'figures frozen'}`;
    }
  }
  function seed() {
    seedState = 7;
    for (const s of Object.values(series)) { s.value = s.start; s.history = []; for (let n = 0; n < 24; n += 1) step(s); }
    draw();
  }
  seed();
  setInterval(() => { if (!liveData()) return; Object.values(series).forEach(step); draw(); }, 2000);

  /* ---------- jobs: a switch sets state, the row moves to its group (FLIP), archive with undo ---------- */
  const openList = $('#open'), doneList = $('#done'), archiveBtn = $('#archive');
  const JOB = { error: ['failed', '3 of 5 steps'], warning: ['waiting', 'review requested'], info: ['active', '62%'], success: ['done', ''] };
  function flipMove(change) {
    const items = $$('.jobs > li');
    const before = new Map(items.map((li) => [li, li.getBoundingClientRect()]));
    change();
    const d = tokens.ms('--rk-duration-moderate');
    if (!d) return;
    for (const li of items) {
      if (!li.isConnected) continue;
      const a = before.get(li), b = li.getBoundingClientRect();
      if (a.top !== b.top || a.left !== b.left) li.animate([{ transform: `translate(${a.left - b.left}px, ${a.top - b.top}px)` }, { transform: 'none' }], { duration: d, easing: tokens.value('--rk-ease-base') });
    }
  }
  function empties() {
    for (const ul of [openList, doneList]) {
      const empty = ul.nextElementSibling && ul.nextElementSibling.classList.contains('rk-empty') ? ul.nextElementSibling : null;
      if (!ul.children.length && !empty) { const p = document.createElement('p'); p.className = 'rk-empty'; p.textContent = ul === openList ? 'Nothing open. Well done.' : 'Nothing done yet.'; ul.after(p); }
      if (ul.children.length && empty) empty.remove();
    }
    archiveBtn.disabled = !doneList.children.length;
  }
  function setJob(li, done) {
    const tag = $('.rk-tag', li), meta = $('.rk-meta', li);
    if (!li.dataset.meta) li.dataset.meta = meta.textContent;
    const status = done ? 'success' : (li.dataset.orig === 'success' ? 'info' : li.dataset.orig);
    tag.dataset.status = status;
    tag.textContent = JOB[status][0];
    meta.textContent = done && li.dataset.orig !== 'success' ? 'just now' : li.dataset.meta;
  }
  document.addEventListener('change', (e) => {
    const li = e.target.closest('.jobs > li');
    if (!li || !e.target.matches('input[type=checkbox]')) return;
    const done = e.target.checked;
    flipMove(() => { setJob(li, done); if (done) doneList.prepend(li); else openList.append(li); empties(); });
    e.target.focus();
  });
  archiveBtn.addEventListener('click', () => {
    const lis = $$('#done > li');
    if (!lis.length) return;
    const names = lis.map((li) => $('.job-name', li).textContent);
    flipMove(() => { lis.forEach((li) => li.remove()); empties(); });
    toasts.show(lis.length === 1 ? `${names[0]} archived` : `${lis.length} jobs archived`, {
      key: 'archive', from: archiveBtn,
      undo() {
        flipMove(() => { lis.forEach((li) => doneList.append(li)); empties(); });
        toasts.show(lis.length === 1 ? `${names[0]} is back` : `${lis.length} jobs are back`, { key: 'archive-back' });
      },
    });
  });
  empties();

  // More: a menu. Refresh loads the jobs again: the region keeps what it shows and runs the line; offline it fails
  // and says so in the region, with the way on.
  const jobsRegion = $('#jobs-region');
  function refreshJobs() {
    const failed = $('#jobs-error');
    if (failed) failed.remove();
    return Rekta.busy(jobsRegion, async () => {
      await wait(1200);
      if (banner.is('offline')) throw new Error('offline');
    }).then(() => toasts.show('Jobs refreshed.', { key: 'refresh' }), () => {
      const p = document.createElement('div');
      p.className = 'rk-empty'; p.id = 'jobs-error'; p.dataset.status = 'error';
      p.innerHTML = '<p>Could not load the jobs: the job server is offline. What you see is from before.</p><p class="rk-actions"><button class="rk-btn" type="button" id="jobs-retry">retry</button></p>';
      jobsRegion.prepend(p);
    });
  }
  document.addEventListener('click', (e) => { if (e.target.closest('#jobs-retry')) refreshJobs(); });
  Rekta.menu($('#jobs-more'), $('#jobs-menu'), {
    onSelect(item) {
      const act = item.dataset.act;
      if (act === 'refresh') refreshJobs();
      if (act === 'show-done') { const on = item.getAttribute('aria-checked') === 'true'; $('#done-label').hidden = !on; doneList.hidden = !on; }
      if (act === 'export') toasts.show('Exported 5 jobs as jobs.csv.', { key: 'export' });
    },
  });

  /* ---------- reviews: an action per row, the board's tile follows ---------- */
  function paintReviews() {
    const rows = $$('#review-list > li'), waiting = rows.filter((li) => $('[data-approve]', li)).length, done = rows.length - waiting;
    const tile = $('#tile-reviews');
    $('#reviews-open').textContent = waiting;
    $('#reviews-progress').value = done;
    $('#reviews-progress').textContent = `${done} of ${rows.length}`;
    $('#reviews-facts').textContent = waiting ? `${waiting} of ${rows.length} waiting · release 1.8` : `all ${rows.length} approved · release 1.8`;
    if (waiting) { tile.dataset.status = 'warning'; $('.rk-tile-word', tile).textContent = 'waiting'; }
    else { tile.dataset.status = 'success'; $('.rk-tile-word', tile).textContent = 'done'; }
    $('#facts').textContent = `1 failed · ${waiting} waiting · 12 running · updated every 2 s`;
  }
  $('#review-list').addEventListener('click', (e) => {
    const btn = e.target.closest('[data-approve]');
    if (!btn) return;
    const li = btn.closest('li'), name = $('b', li).textContent, tag = $('.rk-tag', li);
    const next = li.nextElementSibling && $('[data-approve]', li.nextElementSibling);
    tag.dataset.status = 'success'; tag.textContent = 'done';
    btn.remove();
    paintReviews();
    // The control is gone, so focus moves to the next row's action, or to the place's title.
    (next || $('#reviews-title')).focus();
    toasts.show(`${name} approved`, {
      key: 'review', from: next || $('#reviews-title'),
      undo() {
        tag.dataset.status = 'warning'; tag.textContent = 'waiting';
        li.append(btn);
        paintReviews();
        btn.focus();
      },
    });
  });
  $('#reviews-title').tabIndex = -1;

  /* ---------- controls: live validation, cause and remedy ---------- */
  const timeField = $('#time-field'), timeInput = $('#f-time'), timeHint = $('#h-time'), save = $('#save');
  function validate() {
    const v = timeInput.value.trim(), m = /^(\d{1,2}):(\d{2})$/.exec(v), ok = !!m && +m[1] < 24 && +m[2] < 60;
    if (ok) { timeField.removeAttribute('data-status'); timeInput.removeAttribute('aria-invalid'); timeHint.textContent = 'Daily, in 24-hour time.'; }
    else {
      timeField.dataset.status = 'error'; timeInput.setAttribute('aria-invalid', 'true');
      timeHint.textContent = m ? `${v} is not a time of day. Use 00:00 to 23:59.` : 'Write the time as hours and minutes, for example 02:30.';
    }
    save.disabled = !ok;
    return ok;
  }
  timeInput.addEventListener('input', validate);
  $('#schedule-form').addEventListener('submit', (e) => {
    e.preventDefault();
    if (!validate()) { timeInput.focus(); return; }
    Rekta.pending(save, wait(900)).then(() => toasts.show(`Schedule saved: ${$('#f-name').value.trim()} runs daily at ${timeInput.value.trim()}.`, { key: 'schedule', from: save }));
  });
  $('#revert').addEventListener('click', () => { $('#f-name').value = 'Nightly import'; timeInput.value = '25:00'; $('#f-retry').checked = true; validate(); });
  validate();

  /* ---------- the import place: retry, and the board's tile follows ---------- */
  // Delegated: a reset re-renders the place.
  document.addEventListener('click', (e) => {
    if (!e.target.closest('#retry')) return;
    const stepTile = $('#import-step'), tile = $('#tile-import'), retries = $('#import-retries');
    if (stepTile.dataset.status === 'info') return;
    stepTile.dataset.status = 'info'; $('.rk-tile-word', stepTile).textContent = 'active';
    $('.rk-tile-msg', stepTile).textContent = 'Retrying the schema check after the migration.';
    tile.dataset.status = 'info'; $('.rk-tile-word', tile).textContent = 'active';
    $('.rk-tile-msg', tile).textContent = 'Retrying from the schema check.';
    retries.textContent = Math.max(0, +retries.textContent - 1);
    const tag = $$('#import-steps .rk-tag')[2]; tag.dataset.status = 'info'; tag.textContent = 'active';
    $('#import-facts').textContent = `retrying step 3 of 5 · started ${now()}`;
    $('#retry').disabled = true;
    $('#import-title').focus();
    toasts.show('Nightly import retrying from the schema check.', { key: 'retry' });
  });

  /* ---------- the queue place: its rows load on the first visit, with still placeholders ---------- */
  let queueLoaded = false;
  function loadQueue() {
    if (queueLoaded) return;
    queueLoaded = true;
    Rekta.busy($('#queue-region'), wait(1000)).then(() => {
      $('#running').innerHTML = [['Build #4182', 'web · 2 min'], ['Build #4183', 'api · 1 min'], ['Build #4179', 'docs · 6 min']]
        .map(([b, m]) => `<li><b>${b}</b><span class="rk-meta">${m}</span><span class="rk-tag" data-status="info">active</span></li>`).join('');
    });
  }

  /* ---------- runs: a table in a split, the current run in the address (#runs/4182) ---------- */
  const RUN_WORD = { info: 'active', warning: 'waiting', error: 'failed', success: 'done' };
  const RUNS = [
    [4184, 'warning', 'mobile', 0, '02:31'], [4183, 'info', 'api', 64, '02:29'], [4182, 'info', 'web', 128, '02:27'],
    [4181, 'success', 'docs', 41, '02:20'], [4180, 'success', 'web', 190, '02:12'], [4179, 'error', 'api', 380, '02:05'],
    [4178, 'success', 'mobile', 512, '01:58'], [4177, 'success', 'web', 175, '01:44'], [4176, 'success', 'docs', 38, '01:31'],
    [4175, 'success', 'api', 96, '01:20'], [4174, 'success', 'web', 201, '01:02'], [4173, 'success', 'mobile', 488, '00:47'],
  ].map(([id, status, target, secs, at]) => ({ id: String(id), status, target, secs, at }));
  const dur = (s) => (!s ? '–' : s < 60 ? `${s} s` : `${Math.round(s / 60)} min`);
  const runsHtml = RUNS.map((r) => `<tr data-run="${r.id}"><td class="rk-check"><input type="checkbox" aria-label="select run #${r.id}"></td>`
    + `<th scope="row"><a href="#runs/${r.id}">#${r.id}</a></th><td><span class="rk-tag" data-status="${r.status}">${RUN_WORD[r.status]}</span></td>`
    + `<td>${r.target}</td><td class="rk-num" data-sort="${r.secs}">${dur(r.secs)}</td><td class="rk-num" data-sort="${r.at.replace(':', '')}">${r.at}</td></tr>`).join('');
  $('#runs-body').innerHTML = runsHtml;
  const runsTable = Rekta.table($('#runs-table'), { appbar: $('#bar') });
  const runsSplit = Rekta.split($('#runs-split'), { key: 'rekta-board-runs-split' });
  function runsFacts() {
    const rows = $$('#runs-body tr'), failed = rows.filter((tr) => $('.rk-tag', tr).dataset.status === 'error').length;
    const shown = rows.filter((tr) => !tr.hidden).length;
    $('#runs-facts').textContent = `${rows.length} runs · ${failed} failed${shown < rows.length ? ` · showing ${shown}` : ''}`;
  }
  function renderRun(id) {
    const r = RUNS.find((x) => x.id === id);
    if (!r) { $('#run-detail').innerHTML = '<p class="rk-empty">Choose a run to see its steps.</p>'; return; }
    const steps = ['checkout', 'build', 'test', 'deploy'], at = { error: 2, info: 1, warning: 0 }[r.status] ?? 4;
    $('#run-detail').innerHTML = `<p class="rk-kicker">run · ${r.target}</p><h2 class="rk-group" id="run-title" tabindex="-1">Run #${r.id}</h2>`
      + `<p class="rk-facts">${RUN_WORD[r.status]} · started ${r.at} · ran ${dur(r.secs)}</p><ul class="rk-rows">`
      + steps.map((s, i) => { const st = i < at ? 'success' : i === at ? r.status : null; return `<li><b>${s}</b><span class="rk-tag"${st ? ` data-status="${st}"` : ''}>${st ? RUN_WORD[st] : 'idle'}</span></li>`; }).join('')
      + '</ul>';
  }
  let lastRun = null;
  // Search and filters: rows hide, the address keeps them (#runs?q=…&status=…), the selection forgets hidden rows.
  const checkedIn = (menuEl) => $$('[aria-checked="true"]', menuEl).map((i) => i.dataset.v);
  function applyFilters({ write = true } = {}) {
    const q = $('#runs-q').value.trim().toLowerCase(), st = checkedIn($('#f-status-menu')), tg = checkedIn($('#f-target-menu'));
    let shown = 0;
    for (const tr of $$('#runs-body tr')) {
      const r = RUNS.find((x) => x.id === tr.dataset.run);
      const keep = (!q || `#${r.id} ${r.target} ${RUN_WORD[r.status]}`.includes(q)) && (!st.length || st.includes(r.status)) && (!tg.length || tg.includes(r.target));
      tr.hidden = !keep;
      if (keep) shown += 1;
    }
    $('#f-clear').hidden = !(q || st.length || tg.length);
    $('#runs-none').hidden = shown > 0;
    runsTable.refresh(); runsFacts();
    if (write && router.current === 'runs') {
      const p = new URLSearchParams();
      if (q) p.set('q', q);
      if (st.length) p.set('status', st.map((v) => RUN_WORD[v]).join(','));
      if (tg.length) p.set('target', tg.join(','));
      router.setParams(p);
    }
  }
  const fStatus = Rekta.menu($('#f-status'), $('#f-status-menu'), { summary: 'status', onSelect: () => applyFilters() });
  const fTarget = Rekta.menu($('#f-target'), $('#f-target-menu'), { summary: 'target', onSelect: () => applyFilters() });
  let typing = 0;
  $('#runs-q').addEventListener('input', () => { clearTimeout(typing); typing = setTimeout(applyFilters, 150); });
  function clearFilters() { $('#runs-q').value = ''; fStatus.set(() => false); fTarget.set(() => false); applyFilters(); $('#runs-q').focus(); }
  $('#f-clear').addEventListener('click', clearFilters);
  document.addEventListener('click', (e) => { if (e.target.closest('[data-clear-filters]')) clearFilters(); });
  function filtersFrom(params) {
    const words = (k) => (params.get(k) || '').split(',').filter(Boolean);
    const st = words('status'), tg = words('target');
    $('#runs-q').value = params.get('q') || '';
    fStatus.set((i) => st.includes(RUN_WORD[i.dataset.v]));
    fTarget.set((i) => tg.includes(i.dataset.v));
    applyFilters({ write: false });
  }

  router.onItem('runs', (id, { initial, params }) => {
    filtersFrom(params);
    $$('#runs-body tr').forEach((tr) => { if (tr.dataset.run === id) tr.setAttribute('aria-current', 'true'); else tr.removeAttribute('aria-current'); });
    renderRun(id);
    if (id) {
      if (!initial) runsSplit.reveal();
      runsSplit.show('detail', { focus: $('#run-title'), animate: !initial });
      lastRun = id;
    } else {
      const link = lastRun && $(`#runs-body tr[data-run="${lastRun}"] a`);
      runsSplit.show('list', { focus: link, animate: !initial });
    }
    return id ? `Run #${id}` : null;
  }, { push: () => !runsSplit.sideways });
  $('#sel-export').addEventListener('click', () => toasts.show(`Exported ${runsTable.selected().length} runs as runs.csv.`, { key: 'export-runs', from: $('#sel-export') }));
  $('#sel-archive').addEventListener('click', () => {
    const rows = runsTable.selected();
    if (!rows.length) return;
    rows.forEach((tr) => tr.remove());
    if (rows.some((tr) => tr.dataset.run === router.item)) router.open(null, { replace: true });
    runsTable.refresh(); runsFacts();
    $('#runs-table .rk-check input').focus();
    toasts.show(rows.length === 1 ? `Run #${rows[0].dataset.run} archived` : `${rows.length} runs archived`, {
      key: 'archive-runs', from: $('#runs-table .rk-check input'),
      undo() {
        rows.forEach((tr) => { $('.rk-check input', tr).checked = false; $('#runs-body').appendChild(tr); });
        const th = $('#runs-table thead th[aria-sort="ascending"], #runs-table thead th[aria-sort="descending"]');
        if (th) runsTable.sort(th.cellIndex, th.getAttribute('aria-sort'));
        runsTable.refresh(); runsFacts();
      },
    });
  });

  /* ---------- go to: a palette from the app bar, and the app bar's overflow ---------- */
  const SECTIONS = [['overview', 'overview'], ['jobs', 'jobs'], ['facets', 'facets'], ['controls', 'controls'], ['system', 'system state'], ['look', 'theme, density and motion']];
  Rekta.palette($('#palette'), {
    trigger: $('#ab-goto'), router,
    items: () => [
      { group: 'places', label: 'Rekta Board', href: '#', keywords: 'board home' },
      { group: 'places', label: 'Nightly import', href: '#import', keywords: 'job failed' },
      { group: 'places', label: 'Build queue', href: '#queue', keywords: 'builds' },
      { group: 'places', label: 'Awaiting review', href: '#reviews', keywords: 'reviews approve' },
      { group: 'places', label: 'Runs today', href: '#runs', keywords: 'deploys' },
      ...SECTIONS.map(([id, label]) => ({ group: 'board sections', label, keywords: id, run: () => router.navigate('board').then(() => board.goTo(id)) })),
      ...RUNS.map((r) => ({ group: 'runs', label: `Run #${r.id}`, keywords: `${r.target} ${RUN_WORD[r.status]}`, href: `#runs/${r.id}` })),
      { group: 'commands', label: banner.is('paused') ? 'resume intake' : 'pause intake', run: () => $('#ab-pause').click() },
      { group: 'commands', label: banner.is('halted') ? 'release the emergency stop' : 'emergency stop', run: () => $('#ab-stop').click() },
      { group: 'commands', label: 'refresh the jobs', run: () => refreshJobs() },
      { group: 'commands', label: `theme: next (now ${prefs.get('theme')})`, run: () => $('#ab-theme').click() },
      { group: 'commands', label: prefs.get('motion') === 'off' ? 'turn motion on' : 'turn motion off', run: () => $('#ab-motion').click() },
      { group: 'commands', label: 'keyboard shortcuts', keys: '?', run: () => $('#keys-sheet').showModal() },
    ],
  });
  Rekta.appbar($('#bar'));
  Rekta.keys.on(5, (e) => {
    if (e.key !== '?' || e.ctrlKey || e.metaKey || e.altKey || Rekta.keys.isEditable(e.target) || document.querySelector('dialog[open]')) return false;
    $('#keys-sheet').showModal(); return true;
  });

  /* ---------- sheet and reset ---------- */
  const sheet = $('#sheet'), resetBtn = $('#reset');
  resetBtn.addEventListener('click', () => sheet.showModal());
  const start = {
    open: openList.innerHTML, done: doneList.innerHTML, tiles: $('#tiles').innerHTML,
    importPlace: $('[data-rk-place=import]').innerHTML, reviews: $('#review-list').innerHTML,
  };
  function resetDemo() {
    toasts.dismiss();
    ORDER.forEach((n) => banner.set(n, false));
    paintState();
    openList.innerHTML = start.open; doneList.innerHTML = start.done; empties();
    if ($('#jobs-error')) $('#jobs-error').remove();
    $('#done-label').hidden = false; doneList.hidden = false;
    $('#jobs-menu [data-act=show-done]').setAttribute('aria-checked', 'true');
    $('#tiles').innerHTML = start.tiles; seed();
    $('[data-rk-place=import]').innerHTML = start.importPlace;
    $('#review-list').innerHTML = start.reviews; paintReviews();
    $('#runs-body').innerHTML = runsHtml; $('#runs-q').value = ''; fStatus.set(() => false); fTarget.set(() => false); applyFilters({ write: false });
    $('#revert').click();
    facets.select(0);
    board.goTo('overview', { focus: false });
    toasts.show('Demo reset. Everything is back at the start.', { key: 'reset' });
  }
  sheet.addEventListener('close', () => { resetBtn.focus(); if (sheet.returnValue === 'reset') resetDemo(); });

  paintState();
  paintReviews();
  window.board = { app, router, board, facets, toasts, banner };
})();

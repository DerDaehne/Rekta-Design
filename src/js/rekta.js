/* Rekta Design behaviour. A framework-free ES module with no dependencies.

   Every timing and angle comes from the --rk- tokens, so reduced motion (every motion duration 0s) makes each step
   instant, never skipped. State the script writes lives in data-rk-* attributes and --_rk-* properties, never in
   classes, so a framework that owns className keeps working. Each controller returns { destroy() }.

   The parts:
     prefs      theme, density and motion switches on the root element, remembered per user
     keys       one layered keyboard dispatcher: the innermost thing that owns a key gets it (Escape, Ctrl/Cmd+Z)
     press      tiles nudge back where they are pressed; dragging or selecting text never activates them
     turn       the place change (turnstile) as two primitives, leave() and enter(), any router can call
     places     a small router for places mounted in the page: URLs, history, back, titles, focus
     panorama   sideways sections: one wheel gesture latches to one scroller, keyboard jumps, the pivot follows
     pivot      facets of a place: fixed order, glide, keyboard
     toaster    one toast at a time, with an undo window that holds while hovered or focused
     banner     one lasting system state at a time, the most severe first
     sheet      a short question from the top edge; a tap on the shading cancels
     busy       a data region loads: the line after busy-delay, never a flash
     pending    a button's action runs: it says what it is doing, at the same width
     menu       a menu button with a floating list, placed next to its control
     table      sorting, selection with the app bar carrying it, labels for the stacked layout
     split      a list and its current item: a draggable divider sideways, one pane at a time compact
     palette    go to a place or run a command, from a list that rises from the app bar (Ctrl/Cmd+K)
     appbar     controls that do not fit move into a "more" menu
     form       late checks, early corrections, a summary of what needs attention, server answers at the field
     guard      unsaved input asks before it is left behind
     confirm    a short question in a sheet, the safe answer first
     disable    a control that cannot act now says why
     start      the defaults for a page: prefs, press, sheets, arrival build-in
     tokens     token values as they apply right now (theme, density, reduced motion) */

export const version = '0.2.0';

// Tokens -------------------------------------------------------------------------------------------------------------
const docEl = document.documentElement;
function css(name, el = docEl) { return getComputedStyle(el).getPropertyValue(name).trim(); }
function ms(name, el) { const v = css(name, el); return (parseFloat(v) || 0) * (/ms$/.test(v) ? 1 : 1000); }
function num(name, el) { return parseFloat(css(name, el)) || 0; }
// A length token in px, whatever unit it is written in (rem follows the root's font size).
function px(name, el) { const v = css(name, el), n = parseFloat(v) || 0; return /rem$/.test(v) ? n * (parseFloat(getComputedStyle(docEl).fontSize) || 16) : n; }
function bezier(name) {
  const m = /cubic-bezier\(([^)]+)\)/.exec(css(name));
  const [x1, y1, x2, y2] = m ? m[1].split(',').map(Number) : [0.25, 0.1, 0.25, 1];
  // Solve x(t) = x for t by Newton-Raphson, then return y(t), like a browser's own timing function.
  const a = (u, v) => 1 - 3 * v + 3 * u, b = (u, v) => 3 * v - 6 * u, c = (u) => 3 * u;
  const sx = (t) => ((a(x1, x2) * t + b(x1, x2)) * t + c(x1)) * t, sy = (t) => ((a(y1, y2) * t + b(y1, y2)) * t + c(y1)) * t;
  const dx = (t) => 3 * a(x1, x2) * t * t + 2 * b(x1, x2) * t + c(x1);
  return (x) => { let t = x; for (let i = 0; i < 8; i += 1) { const e = sx(t) - x; if (Math.abs(e) < 1e-4) break; const d = dx(t); if (Math.abs(d) < 1e-6) break; t -= e / d; } return sy(Math.min(1, Math.max(0, t))); };
}
// Read a token where it applies: tokens.value('--rk-accent'), tokens.ms('--rk-duration-short') (0 under reduced
// motion), tokens.number('--rk-space-m').
export const tokens = { value: css, ms, number: num, px };
const openLayer = (sel) => { try { return !!document.querySelector(sel); } catch { return false; } };
const later = (fn, wait) => (wait > 0 ? setTimeout(fn, wait) : (queueMicrotask(fn), 0));
const isEditable = (t) => !!t && (t.isContentEditable || (t.matches && t.matches('textarea, select, input:not([type=checkbox], [type=radio], [type=range], [type=color], [type=file], [type=button], [type=submit], [type=reset], [type=image])')));
function animateScroll(el, to, duration, ease) {
  cancelAnimationFrame(el._rkRaf);
  const from = el.scrollLeft;
  if (!duration || Math.abs(from - to) < 1) { el.scrollTo({ left: to, behavior: 'instant' }); return; }
  const t0 = performance.now();
  const step = (now) => {
    const t = Math.min(1, (now - t0) / duration);
    el.scrollTo({ left: from + (to - from) * ease(t), behavior: 'instant' });
    el._rkRaf = t < 1 ? requestAnimationFrame(step) : 0;
  };
  el._rkRaf = requestAnimationFrame(step);
}
function focusHeading(scope) {
  const h = scope && scope.querySelector('.rk-title, .rk-group, h1, h2, h3');
  if (!h) return null;
  if (!h.hasAttribute('tabindex') && !h.matches('a, button, input, select, textarea')) h.setAttribute('tabindex', '-1');
  h.focus({ preventScroll: true });
  return h;
}

// Prefs --------------------------------------------------------------------------------------------------------------
// theme: system | light | dark; density: auto | comfortable | compact; motion: system | on | off. The default
// removes the attribute, so the system setting (or the pointer, for density) decides. Set them before first paint.
const PREF_DEFAULT = { theme: 'system', density: 'auto', motion: 'system' };
export const prefs = {
  key: 'rekta-prefs',
  read() { try { return JSON.parse(localStorage.getItem(this.key) || '{}'); } catch { return {}; } },
  write(all) { try { localStorage.setItem(this.key, JSON.stringify(all)); } catch { /* private mode: this visit only */ } },
  get(name) { return docEl.dataset[name] || PREF_DEFAULT[name]; },
  set(name, value, { remember = true } = {}) {
    if (!(name in PREF_DEFAULT)) throw new Error(`unknown pref ${name}`);
    if (!value || value === PREF_DEFAULT[name]) delete docEl.dataset[name]; else docEl.dataset[name] = value;
    if (remember) { const all = this.read(); all[name] = this.get(name); this.write(all); }
    document.dispatchEvent(new CustomEvent('rk:prefs', { detail: { name, value: this.get(name) } }));
  },
  restore() { const all = this.read(); for (const name of Object.keys(PREF_DEFAULT)) if (all[name]) this.set(name, all[name], { remember: false }); },
};

// Keys ---------------------------------------------------------------------------------------------------------------
// The innermost layer that wants a key gets it. Higher priority first: floating surfaces 40, a focused toast 30,
// a selection 20, the place (back) 10. A handler returns true when it used the key; it is then prevented.
const layers = [];
let keysBound = false;
function onKey(e) {
  if (e.defaultPrevented || e.isComposing || e.keyCode === 229) return;
  for (const layer of layers) {
    if (layer.fn(e)) { e.preventDefault(); return; }
  }
}
export const keys = {
  on(priority, fn) {
    const layer = { priority, fn };
    layers.push(layer); layers.sort((a, b) => b.priority - a.priority);
    if (!keysBound) { document.addEventListener('keydown', onKey); keysBound = true; }
    return () => { const i = layers.indexOf(layer); if (i >= 0) layers.splice(i, 1); };
  },
  // Escape may go back to the previous place only when nothing nearer wants it.
  escapeIsFree(e) {
    const t = e.target;
    if (isEditable(t)) return false;
    if (t.closest && t.closest('[role=combobox], [role=listbox], [role=menu], [aria-expanded=true]')) return false;
    if (openLayer(':modal') || openLayer(':popover-open')) return false;
    return true;
  },
  isEditable,
};

// Press --------------------------------------------------------------------------------------------------------------
// A tile nudges back where it is pressed. On touch it presses only after tap-delay without movement, so the start of a
// scroll never sinks it. Movement past 4px (8px on touch) releases it, and a click that follows a drag or a text
// selection inside the tile is cancelled: dragging never activates. A quick tap still shows the whole sink.
const TILE = 'a.rk-tile, button.rk-tile';
const pressState = { pressed: null, last: null, since: 0, letGo: 0, lettingGo: null };
function setPressed(tile, x, y) {
  releasePress(true);
  tile.style.setProperty('--_rk-px', x.toFixed(3));
  tile.style.setProperty('--_rk-py', y.toFixed(3));
  tile.setAttribute('data-rk-pressed', '');
  Object.assign(pressState, { pressed: tile, last: tile, since: performance.now() });
}
function dropPress() {
  clearTimeout(pressState.letGo);
  if (pressState.lettingGo) pressState.lettingGo.removeAttribute('data-rk-pressed');
  pressState.lettingGo = null;
}
function releasePress(now) {
  if (now) dropPress();
  const tile = pressState.pressed;
  if (!tile) return;
  const wait = now ? 0 : Math.max(0, ms('--rk-duration-press') - (performance.now() - pressState.since));
  pressState.pressed = null;
  dropPress();
  if (!wait) { tile.removeAttribute('data-rk-pressed'); return; }
  pressState.lettingGo = tile;
  pressState.letGo = setTimeout(dropPress, wait);
}
export function press(scope = document) {
  let start = null, pending = 0;
  const clamp = (v) => Math.max(-1, Math.min(1, v));
  const down = (e) => {
    if (e.button > 0) return;
    const tile = e.target.closest && e.target.closest(TILE);
    if (!tile || turn.running) { start = null; return; }
    const r = tile.getBoundingClientRect();
    const x = clamp((e.clientX - r.left) / r.width * 2 - 1), y = clamp((e.clientY - r.top) / r.height * 2 - 1);
    start = { x: e.clientX, y: e.clientY, tile, touch: e.pointerType === 'touch', moved: false, px: x, py: y };
    clearTimeout(pending);
    if (start.touch) pending = setTimeout(() => { pending = 0; if (start && !start.moved) setPressed(tile, x, y); }, ms('--rk-duration-tap-delay'));
    else setPressed(tile, x, y);
  };
  const move = (e) => {
    if (!start || start.moved) return;
    if (Math.hypot(e.clientX - start.x, e.clientY - start.y) > (start.touch ? 8 : 4)) {
      start.moved = true; clearTimeout(pending); pending = 0;
      if (pressState.pressed === start.tile) releasePress(true);
    }
  };
  const up = () => {
    if (pending && start && !start.moved) { clearTimeout(pending); pending = 0; setPressed(start.tile, start.px, start.py); }
    releasePress(false);
  };
  const cancel = () => { clearTimeout(pending); pending = 0; if (start) start.moved = true; releasePress(true); };
  const click = (e) => {
    if (e.detail === 0) return;                                           // keyboard and assistive activation
    const control = e.target.closest && e.target.closest(`${TILE}, .rk-rows a, .rk-rows button`);
    if (!control) return;
    const sel = getSelection();
    const selecting = sel && !sel.isCollapsed && control.contains(sel.anchorNode);
    if ((start && start.moved && control.contains(start.tile)) || selecting) { e.preventDefault(); e.stopImmediatePropagation(); }
  };
  const keydown = (e) => {
    if (e.repeat || turn.running || !e.target.matches || !e.target.matches(TILE)) return;
    if (e.key === 'Enter' || (e.key === ' ' && e.target.tagName === 'BUTTON')) setPressed(e.target, 0, 0);
  };
  const keyup = (e) => { if (e.key === ' ' || e.key === 'Enter') releasePress(false); };
  const blur = () => releasePress(true);
  const dragstart = (e) => { if (e.target.closest && e.target.closest(TILE)) e.preventDefault(); };
  scope.addEventListener('pointerdown', down);
  scope.addEventListener('pointermove', move, { passive: true });
  document.addEventListener('pointerup', up);
  document.addEventListener('pointercancel', cancel);
  scope.addEventListener('click', click, true);
  scope.addEventListener('keydown', keydown);
  document.addEventListener('keyup', keyup);
  scope.addEventListener('dragstart', dragstart);
  window.addEventListener('blur', blur);
  return {
    destroy() {
      scope.removeEventListener('pointerdown', down); scope.removeEventListener('pointermove', move);
      document.removeEventListener('pointerup', up); document.removeEventListener('pointercancel', cancel);
      scope.removeEventListener('click', click, true); scope.removeEventListener('keydown', keydown);
      document.removeEventListener('keyup', keyup); scope.removeEventListener('dragstart', dragstart);
      window.removeEventListener('blur', blur);
    },
  };
}

// Turn ---------------------------------------------------------------------------------------------------------------
// The place change as two primitives. leave(place) turns the visible units of a place away, a row at a time, the
// pressed tile last from its pressed pose, then hides the place. enter(place) shows it and turns it in; its tiles build
// in unless it is a place you return to. A router (this one, or any other) calls leave, swaps what it renders, then
// calls enter. finish() ends a running turn in its end state: a navigation is never refused.
let turning = null;
const TURN_PROPS = ['--_rk-turn-origin', '--_rk-turn-oy', '--_rk-turn-delay', '--_rk-turn-perspective', '--_rk-tint'];
function turnUnits(place, last) {
  const list = [], vh = innerHeight, vw = innerWidth;
  const shown = (el) => { const r = el.getBoundingClientRect(); return r.width > 0 && r.height > 0 && r.bottom > 0 && r.top < vh && r.right > 0 && r.left < vw; };
  (function walk(el) {
    for (const c of el.children) {
      if (c.hidden || !shown(c)) continue;
      if (c.classList.contains('rk-tile') || !c.querySelector('.rk-tile')) list.push(c); else walk(c);
    }
  })(place);
  const at = new Map(list.map((u) => [u, u.getBoundingClientRect()]));
  list.sort((a, b) => { const p = at.get(a), q = at.get(b); return (Math.round(p.top / 8) - Math.round(q.top / 8)) || (p.left - q.left); });
  if (last && at.has(last)) { list.splice(list.indexOf(last), 1); list.push(last); }
  return { list, at };
}
function perspective(far) { return Math.round((num('--rk-turnstile-perspective') || 800) + 1.2 * Math.max(0, far)) + 'px'; }
function arrive(el) {
  el.setAttribute('data-rk-arriving', '');
  const wait = ms('--rk-duration-moderate') + 10 * ms('--rk-duration-stagger') + ms('--rk-duration-long') + 50;
  clearTimeout(el._rkArrive);
  el._rkArrive = setTimeout(() => el.removeAttribute('data-rk-arriving'), wait);
}
export const turn = {
  get running() { return !!turning; },
  finish() { if (turning) turning.finish(); },
  leave(place, { pressed = null, back = false } = {}) {
    turn.finish();
    // A place still building in leaves from its end state: the turn wins over the arrival.
    for (const el of [place, docEl]) { clearTimeout(el._rkArrive); el.removeAttribute('data-rk-arriving'); }
    return new Promise((resolve) => {
      const timers = [];
      // The pressed tile keeps its press until it leaves; one activated without a press sinks now, in its centre.
      const held = back ? null : pressed;
      let hold = 0;
      if (held) {
        const sinking = held === pressState.pressed || held === pressState.lettingGo;
        if (held !== pressState.last) { held.style.setProperty('--_rk-px', 0); held.style.setProperty('--_rk-py', 0); }
        hold = Math.max(0, ms('--rk-duration-press') - (sinking ? performance.now() - pressState.since : 0));
        if (pressState.pressed === held) pressState.pressed = null;
      }
      releasePress(true);
      if (held) held.setAttribute('data-rk-pressed', '');
      const out = ms('--rk-duration-short'), step = ms('--rk-duration-stagger');
      const u = turnUnits(place, held), kind = back ? 'back-out' : 'out';
      // Each unit turns around the leading edge of the box that clips it, so that edge never cuts it; a unit flush
      // with that box's top turns from its top. A row shares a delay and a perspective; the pressed tile has the
      // last slot to itself.
      let slot = -1, key;
      const slots = [], boxes = [], far = [];
      for (const el of u.list) {
        const r = u.at.get(el);
        let c = el.parentElement;
        while (c && c !== place && getComputedStyle(c).overflowX === 'visible') c = c.parentElement;
        const cr = (c || place).getBoundingClientRect(), k = el === held ? 'P' : Math.round(r.top / 8);
        if (k !== key) { slot += 1; key = k; far[slot] = 0; }
        slots.push(slot); boxes.push(cr);
        far[slot] = Math.max(far[slot], r.right - cr.left);
      }
      let end = 0;
      u.list.forEach((el, i) => {
        const r = u.at.get(el), cr = boxes[i];
        let at = Math.min(slots[i], el === held ? 10 : 9) * step;
        el.style.setProperty('--_rk-turn-origin', `${cr.left - r.left}px`);
        if (Math.abs(r.top - cr.top) < 2) el.style.setProperty('--_rk-turn-oy', '0px');
        el.style.setProperty('--_rk-turn-perspective', perspective(far[slots[i]]));
        if (el === held) {
          at = Math.max(at, hold);
          timers.push(later(() => { if (el.matches(':hover')) el.style.setProperty('--_rk-tint', 'var(--rk-hover)'); el.setAttribute('data-rk-turn', 'pressed'); }, at));
        } else {
          el.style.setProperty('--_rk-turn-delay', `${at}ms`);
          el.setAttribute('data-rk-turn', kind);
        }
        end = Math.max(end, at);
      });
      const done = () => {
        timers.forEach(clearTimeout);
        place.hidden = true;
        if (held) held.removeAttribute('data-rk-pressed');
        for (const el of u.list) { el.removeAttribute('data-rk-turn'); TURN_PROPS.forEach((p) => el.style.removeProperty(p)); }
        turning = null;
        resolve();
      };
      turning = { finish: done };
      // The place goes when its last unit is 80% through, all but faded, so the next one follows without a gap.
      timers.push(later(done, u.list.length ? end + 0.8 * out : 0));
    });
  },
  enter(place, { back = false, focus = null, arriving = !back } = {}) {
    turn.finish();
    return new Promise((resolve) => {
      place.hidden = false;
      // A place you return to comes back as you left it: no second build-in, no redraw.
      if (arriving) arrive(place);
      else place.getAnimations({ subtree: true }).forEach((a) => { if (a.animationName === 'rk-build-in' || a.animationName === 'rk-draw') a.finish(); });
      // The new place turns around its leading edge at the middle of the viewport, however tall it is.
      const r = place.getBoundingClientRect();
      place.style.setProperty('--_rk-turn-oy', `${Math.round(innerHeight / 2 - r.top)}px`);
      place.style.setProperty('--_rk-turn-perspective', perspective(r.width));
      place.setAttribute('data-rk-turn', back ? 'back-in' : 'in');
      if (focus) focus.focus({ preventScroll: true });
      const done = () => {
        clearTimeout(timer);
        place.removeAttribute('data-rk-turn');
        place.style.removeProperty('--_rk-turn-oy'); place.style.removeProperty('--_rk-turn-perspective');
        turning = null;
        resolve();
      };
      turning = { finish: done };
      const timer = later(done, ms('--rk-duration-moderate'));
    });
  },
};

// Places -------------------------------------------------------------------------------------------------------------
// A small router for places mounted in the page as <main class="rk-place" data-rk-place="name" data-rk-parent="…">.
// Every place has an address (#name in hash mode, base/name in path mode). Only a place change pushes a history
// entry: facets and section jumps replace nothing and push nothing. Back (the app bar's back, Escape, the browser's
// or the system's back) is history.back() and plays the back turnstile. A crumb links up to the parent place: it is
// back when the parent is the place below, otherwise a step forward to the parent. A cold entry (a deep link, a
// reload) puts the parent place underneath, so back stays in the app. Plain primary clicks on links to places turn
// the place; Ctrl/Cmd/Shift/Alt and middle clicks are the browser's. A navigation is never refused: one that comes
// while a turn runs finishes that turn first.
// A place can show one item at a time (the run a split shows): #name/item. onItem(place, fn, { push }) hears it.
// Choosing an item replaces the address, unless push() says it should be a step of its own (a split in the compact
// class, where the item replaces the list); then back returns to the list. Search and filters are parameters of the
// address (#name?status=failed): setParams() replaces them, never pushing; the watcher hears them on arrival and back.
export function places({ root = document, mode = 'hash', base = '', home, title = document.title, onChange } = {}) {
  const all = () => [...root.querySelectorAll('[data-rk-place]')];
  const byName = (name) => (name ? root.querySelector(`[data-rk-place="${CSS.escape(name)}"]`) : null);
  home = home || (all()[0] && all()[0].dataset.rkPlace);
  const enc = encodeURIComponent;
  const urlOf = (name, item = null, params = '') => {
    const q = params ? `?${params}` : '';
    return mode === 'hash'
      ? (name === home && !item && !q ? location.pathname + location.search : `#${enc(name)}${item ? `/${enc(item)}` : ''}${q}`)
      : `${base}/${name === home && !item ? '' : enc(name)}${item ? `/${enc(item)}` : ''}${q}`;
  };
  // An address's place and item, or null when it names no place here.
  const parse = (href) => {
    const u = new URL(href, location.href);
    if (u.origin !== location.origin) return null;
    if (mode === 'hash' && (u.pathname !== location.pathname || u.search !== location.search)) return null;
    if (mode === 'path' && !(u.pathname === base || u.pathname.startsWith(`${base}/`))) return null;
    let raw = mode === 'hash' ? u.hash.slice(1) : u.pathname.slice(base.length).replace(/^\/|\/$/g, '');
    let params = mode === 'hash' ? '' : u.search.slice(1);
    if (mode === 'hash' && raw.includes('?')) { params = raw.slice(raw.indexOf('?') + 1); raw = raw.slice(0, raw.indexOf('?')); }
    const [first, ...rest] = raw.split('/');
    const name = decodeURIComponent(first || '') || home, item = rest.length ? decodeURIComponent(rest.join('/')) : null;
    return byName(name) ? { name, item, params } : null;
  };
  const nameOf = (href) => { const p = parse(href); return p && p.name; };
  const parentOf = (name) => { const p = byName(name) && byName(name).dataset.rkParent; return byName(p) ? p : home; };
  const titleOf = (name) => { const h = byName(name).querySelector('.rk-title'); const t = h && h.textContent.trim(); return name === home || !t ? title : `${t} · ${title}`; };
  // Everything that scrolls in a place keeps its offset while the place is away.
  const scrollers = (el) => [el, ...el.querySelectorAll('.rk-panorama, .rk-sec, .rk-split-list, .rk-split-detail, [data-rk-scroll]')];
  let current = home, item = null, params = '', index = 0, below = null, pushedItem = false, busy = Promise.resolve();
  const watchers = new Map();
  const backButtons = () => document.querySelectorAll('.rk-ab-back[data-rk-back]');
  const showBack = () => backButtons().forEach((b) => { b.hidden = current === home && index === 0; });
  const rk = () => (history.state && history.state.rk) || {};
  function snapshot(opener) {
    const el = byName(current), prev = rk();
    return { ...(history.state || {}), rk: { ...prev, place: current, index, below, item, params, pushedItem, scrollY, scroll: scrollers(el).map((s) => [s.scrollLeft, s.scrollTop]), opener: opener || prev.opener || null } };
  }
  function restore(el, state) {
    const offsets = (state && state.scroll) || [];
    scrollers(el).forEach((s, i) => { const o = offsets[i] || [0, 0]; s.scrollTo({ left: o[0], top: o[1], behavior: 'instant' }); });
    scrollTo({ left: 0, top: (state && state.scrollY) || 0, behavior: 'instant' });
  }
  function focusTarget(to, state, back) {
    if (back && state && state.opener) {
      const o = [...to.querySelectorAll('a[href]')].find((a) => a.getAttribute('href') === state.opener);
      if (o && o.getClientRects().length) return o;
    }
    const t = to.querySelector('.rk-title') || to;
    if (!t.hasAttribute('tabindex') && !t.matches('a, button, input, select, textarea')) t.setAttribute('tabindex', '-1');
    return t;
  }
  // The place's item or parameters changed: tell its watcher, which may name the item for the title.
  function setItem(next, opts = {}) {
    item = next || null;
    const w = watchers.get(current), named = w ? w.fn(item, { ...opts, params: new URLSearchParams(params) }) : null;
    document.title = typeof named === 'string' && named ? `${named} · ${titleOf(current)}` : titleOf(current);
  }
  function show(name, state, { instant = false, back = false, pressed = null } = {}) {
    const prev = current, from = byName(prev), to = byName(name);
    current = name;
    below = state ? state.below || null : null;
    pushedItem = !!(state && state.pushedItem);
    params = (state && state.params) || '';
    showBack();
    const run = async () => {
      if (from && from !== to) {
        if (instant) { turn.finish(); from.hidden = true; } else await turn.leave(from, { back, pressed });
      }
      to.hidden = false;
      setItem(state && state.item, { back, place: true });
      restore(to, state);
      const f = focusTarget(to, state, back);
      if (instant) { turn.finish(); if (!back) arrive(to); f.focus({ preventScroll: true }); } else await turn.enter(to, { back, focus: f });
      if (onChange) onChange({ from: prev, to: name, back });
    };
    busy = busy.then(run, run);
    return busy;
  }
  // Guards (unsaved input) may hold a place change: each returns true at once when it has nothing to ask, or false,
  // or a promise of the answer. verdict() is true when every guard lets go at once, else a promise.
  const guards = new Set();
  let bypass = false, ignoreNext = null;
  function verdict() {
    const answers = [...guards].map((g) => { try { return g(); } catch { return false; } });
    if (answers.every((a) => a === true)) return true;
    return Promise.all(answers.map((a) => Promise.resolve(a).catch(() => false))).then((all) => all.every(Boolean));
  }
  function navigate(target, { replace = false, pressed = null, force = false } = {}) {
    const p = byName(target) ? { name: target, item: null } : parse(target);
    if (!p) return Promise.resolve();
    if (p.name === current) { if (p.item !== item) open(p.item); return Promise.resolve(); }
    if (!force) { const v = verdict(); if (v !== true) return Promise.resolve(v).then((ok) => (ok ? navigate(target, { replace, pressed, force: true }) : undefined)); }
    turn.finish();
    history.replaceState(snapshot(pressed && pressed.getAttribute('href')), '');
    const entry = { rk: { place: p.name, index: replace ? index : index + 1, below: replace ? below : current, item: p.item, params: p.params || '', pushedItem: false } };
    if (replace) history.replaceState(entry, '', urlOf(p.name, p.item, entry.rk.params)); else history.pushState(entry, '', urlOf(p.name, p.item, entry.rk.params));
    index = entry.rk.index;
    return show(p.name, entry.rk, { pressed });
  }
  // open(item): show an item of the current place (null: none). It replaces the address unless the watcher's push()
  // says it is a step of its own.
  function open(next, { replace } = {}) {
    next = next || null;
    if (next === item) return;
    const w = watchers.get(current);
    const push = replace === undefined ? !!(next && !pushedItem && w && w.push && w.push()) : !replace;
    if (push) {
      history.replaceState(snapshot(), '');
      index += 1; pushedItem = true;
      history.pushState({ rk: { place: current, index, below, item: next, params, pushedItem: true } }, '', urlOf(current, next, params));
    } else {
      const s = snapshot();
      history.replaceState({ ...s, rk: { ...s.rk, item: next } }, '', urlOf(current, next, params));
    }
    setItem(next, { back: false });
  }
  // back(): one step back in history; at the bottom of the app's history, up to the parent place instead.
  // back(name): up to that place, by history when it is the place below (or, for an item, the list beneath it).
  function back(target) {
    if (target && target === current) {
      if (item && pushedItem && index > 0) { turn.finish(); history.back(); } else open(null, { replace: true });
      return true;
    }
    if (target && target !== below) { navigate(target); return true; }
    if (index > 0) {
      turn.finish(); history.replaceState(snapshot(), '');
      if (target && pushedItem && index > 1) history.go(-2); else history.back();
      return true;
    }
    if (current === home) return false;
    const up = target || parentOf(current);
    const go = () => { history.replaceState({ rk: { place: up, index: 0, below: null } }, '', urlOf(up)); show(up, null, { back: true }); };
    const v = verdict();
    if (v === true) go(); else Promise.resolve(v).then((ok) => { if (ok) go(); });
    return true;
  }
  const pop = (e) => {
    if (ignoreNext) { const done = ignoreNext; ignoreNext = null; done(); return; }
    const state = e.state && e.state.rk;
    const p = state && byName(state.place) ? { name: state.place, item: state.item || null, params: state.params || '' } : parse(location.href);
    if (!p) return;
    const to = state ? state.index : 0, backward = to < index, jump = Math.abs(to - index);
    // The browser has already moved; a guard that holds the change takes the step back, asks, and replays it.
    const v = p.name !== current && !bypass && to !== index ? verdict() : true;
    if (v !== true) {
      const delta = to - index;
      const reverted = new Promise((r) => { ignoreNext = r; });
      history.go(-delta);
      Promise.all([v, reverted]).then(([ok]) => { if (ok) { bypass = true; history.go(delta); } });
      return;
    }
    bypass = false;
    turn.finish();
    index = to;
    if (p.name === current) {
      below = state ? state.below || null : null;
      pushedItem = !!(state && state.pushedItem);
      const paramsChanged = (p.params || '') !== params;
      params = p.params || '';
      if (p.item !== item || paramsChanged) setItem(p.item, { back: backward });
      restore(byName(current), state);
      showBack();
      return;
    }
    show(p.name, state || { item: p.item, params: p.params }, { back: backward, instant: jump > 1 || e.hasUAVisualTransition });
  };
  const click = (e) => {
    if (e.defaultPrevented || e.button !== 0 || e.metaKey || e.ctrlKey || e.shiftKey || e.altKey) return;
    const backer = e.target.closest && e.target.closest('[data-rk-back]');
    if (backer) {
      e.preventDefault();
      back(backer.matches('a[href]') ? nameOf(backer.href) : undefined);
      return;
    }
    const a = e.target.closest && e.target.closest('a[href]');
    if (!a || (a.target && a.target !== '_self') || a.hasAttribute('download')) return;
    const p = parse(a.href);
    if (!p) return;
    e.preventDefault();
    if (p.name === current) open(p.item);
    else navigate(a.href, { pressed: a.matches('.rk-tile') ? a : null });
  };
  const offKeys = keys.on(10, (e) => {
    if (e.key !== 'Escape' || e.altKey || e.ctrlKey || e.metaKey || e.shiftKey || (current === home && index === 0 && !item) || !keys.escapeIsFree(e)) return false;
    if (index === 0 && item) { open(null, { replace: true }); return true; }
    return back();
  });
  // Start on the place and item the address names. A cold entry puts the parent place underneath, and the place's
  // list beneath an item, so back stays in the app.
  const start = parse(location.href) || { name: home, item: null, params: '' };
  const st = rk();
  if (st.place === start.name && (st.item || null) === start.item) {
    index = st.index || 0; below = st.below || null; pushedItem = !!st.pushedItem;
  } else {
    const chain = [];
    if (start.name !== home) chain.push({ place: parentOf(start.name), item: null, below: null });
    const placeBelow = start.name !== home ? parentOf(start.name) : null;
    if (start.item) chain.push({ place: start.name, item: null, below: placeBelow });
    chain.push({ place: start.name, item: start.item, below: placeBelow, pushedItem: !!start.item, params: start.params });
    chain.forEach((c, i) => {
      const entry = { rk: { place: c.place, index: i, below: c.below, item: c.item, params: c.params || '', pushedItem: !!c.pushedItem } };
      if (i === 0) history.replaceState({ ...(history.state || {}), ...entry }, '', urlOf(c.place, c.item, entry.rk.params));
      else history.pushState(entry, '', urlOf(c.place, c.item, entry.rk.params));
    });
    index = chain.length - 1; below = placeBelow; pushedItem = !!start.item;
  }
  for (const el of all()) el.hidden = el.dataset.rkPlace !== start.name;
  current = start.name;
  item = start.item;
  params = start.params || '';
  document.title = titleOf(current);
  showBack();
  if (st.place === start.name) requestAnimationFrame(() => restore(byName(current), st));
  addEventListener('popstate', pop);
  document.addEventListener('click', click);
  return {
    get current() { return current; },
    get item() { return item; },
    get below() { return below; },
    get params() { return new URLSearchParams(params); },
    // setParams(p): the current place's search and filters, in the address; replaces, never pushes.
    setParams(p) {
      const next = new URLSearchParams(p).toString();
      if (next === params) return;
      params = next;
      const s = snapshot();
      history.replaceState({ ...s, rk: { ...s.rk, params } }, '', urlOf(current, item, params));
    },
    navigate, back, open, urlOf,
    // guard(fn): fn() may hold any change of place, from a link, back, Escape or the browser. It returns true at once
    // when it has nothing to ask (then nothing waits), else false or a promise of the answer. Returns a remover.
    guard(fn) { guards.add(fn); return () => guards.delete(fn); },
    // onItem(place, fn, { push }): fn(item | null, { back, initial }) whenever that place's item changes; it may
    // return a name for the window title. push() decides whether choosing an item is a history step of its own.
    onItem(place, fn, { push } = {}) {
      watchers.set(place, { fn, push });
      if (place === current) setItem(item, { initial: true });
      return () => watchers.delete(place);
    },
    destroy() { removeEventListener('popstate', pop); document.removeEventListener('click', click); offKeys(); },
  };
}

// Panorama -----------------------------------------------------------------------------------------------------------
// Sideways (the place is wide and the viewport tall enough) a panorama moves sideways; compact it stacks. One wheel
// gesture (events closer than duration-wheel-latch) latches to one scroller: to a section that can still scroll in
// its direction, and is then swallowed at that section's end instead of turning sideways; otherwise to the panorama,
// and stays sideways while the pointer crosses sections. Horizontal input and Shift+wheel stay native; at either end
// the wheel falls through. Keys on the focused panorama: ← → PageUp PageDown jump a section, Home End to the ends.
// A pivot of section links follows the scroll; choosing a link aligns that section flush left (or as far as the
// panorama scrolls) and moves focus to its heading. When everything fits, nothing scrolls (data-rk-fit).
export function panorama(el, { pivot = null } = {}) {
  const sections = () => [...el.querySelectorAll(':scope > .rk-sec')];
  const sideways = () => getComputedStyle(el).display === 'flex' && getComputedStyle(el).overflowX !== 'visible';
  let current = sections()[0] || null, gesture = null, clicked = false, wasSideways = sideways();
  const glideEase = bezier('--rk-ease-pivot-glide');
  const links = () => (pivot ? [...pivot.querySelectorAll('a[href^="#"]')] : []);
  const linkFor = (sec) => links().find((a) => a.hash === `#${sec.id}`);

  function startOf(sec) {
    const pad = parseFloat(getComputedStyle(el).paddingLeft) || 0;
    return Math.min(el.scrollLeft + sec.getBoundingClientRect().left - el.getBoundingClientRect().left - pad, el.scrollWidth - el.clientWidth);
  }
  function mark(sec) {
    if (!sec) return;
    current = sec;
    for (const a of links()) { if (a.hash === `#${sec.id}`) a.setAttribute('aria-current', 'location'); else a.removeAttribute('aria-current'); }
    if (pivot) pivotGlide(pivot, linkFor(sec));
  }
  function goTo(sec, { focus = true, instant = false } = {}) {
    if (typeof sec === 'string') sec = el.querySelector(`#${CSS.escape(sec)}`);
    if (!sec) return;
    clicked = true;
    mark(sec);
    if (sideways()) animateScroll(el, startOf(sec), instant ? 0 : ms('--rk-duration-pivot-glide'), glideEase);
    else sec.scrollIntoView({ behavior: instant || !ms('--rk-duration-short') ? 'instant' : 'smooth', block: 'start' });
    if (focus) focusHeading(sec);
  }
  function follow() {
    if (clicked) return;
    const list = sections();
    if (!list.length) return;
    let active = list[0];
    if (sideways()) {
      const edge = el.getBoundingClientRect().left, reach = el.clientWidth / 3;
      for (const s of list) if (s.getBoundingClientRect().left - edge <= reach) active = s;
      if (el.scrollLeft >= el.scrollWidth - el.clientWidth - 1) active = list[list.length - 1];
    } else {
      const edge = num('--rk-banner-h') + num('--rk-pivot-h'), reach = innerHeight / 3;
      for (const s of list) if (s.getBoundingClientRect().top - edge <= reach) active = s;
    }
    if (active !== current) mark(active);
  }
  let queued = false;
  const queue = () => { if (!queued) { queued = true; requestAnimationFrame(() => { queued = false; follow(); }); } };
  const userScroll = () => { clicked = false; };

  // One gesture, one scroller.
  function wheel(e) {
    if (e.ctrlKey || !sideways()) return;                                            // pinch zoom; compact
    const now = e.timeStamp, latch = ms('--rk-duration-wheel-latch') || 200;
    const horizontal = e.shiftKey || Math.abs(e.deltaX) >= Math.abs(e.deltaY);
    const down = e.deltaY > 0;
    if (!gesture || now - gesture.last > latch) {
      clicked = false;
      gesture = { target: 'pan', inner: null };
      if (horizontal) gesture.target = 'native';
      else {
        for (let n = e.target; n && n !== el; n = n.parentElement) {
          const can = n.scrollHeight > n.clientHeight + 1 && /auto|scroll/.test(getComputedStyle(n).overflowY);
          if (can && (down ? n.scrollTop + n.clientHeight < n.scrollHeight - 1 : n.scrollTop > 0)) { gesture = { target: 'inner', inner: n }; break; }
        }
      }
    }
    gesture.last = now;
    if (gesture.target === 'native' || horizontal) return;
    if (gesture.target === 'inner') {
      const n = gesture.inner;
      const room = down ? n.scrollTop + n.clientHeight < n.scrollHeight - 1 : n.scrollTop > 0;
      if (!room) e.preventDefault();                                                  // swallowed at its end
      return;
    }
    const max = el.scrollWidth - el.clientWidth;
    if (down ? el.scrollLeft >= max - 1 : el.scrollLeft <= 0) return;                // falls through at the ends
    e.preventDefault();
    el.scrollBy({ left: e.deltaY * (e.deltaMode === 1 ? 16 : e.deltaMode === 2 ? el.clientWidth : 1), behavior: 'instant' });
  }
  function key(e) {
    if (e.target !== el || !sideways() || e.altKey || e.ctrlKey || e.metaKey) return;
    const list = sections(), i = Math.max(0, list.indexOf(current));
    const to = { ArrowRight: i + 1, PageDown: i + 1, ArrowLeft: i - 1, PageUp: i - 1, Home: 0, End: list.length - 1 }[e.key];
    if (to === undefined) return;
    e.preventDefault();
    goTo(list[Math.max(0, Math.min(list.length - 1, to))], { focus: false });
  }
  function pivotClick(e) {
    const a = e.target.closest && e.target.closest('a[href^="#"]');
    if (!a || e.button !== 0 || e.metaKey || e.ctrlKey || e.shiftKey) return;
    const sec = el.querySelector(a.hash);
    if (!sec) return;
    e.preventDefault();
    goTo(sec);
  }
  function fit() {
    const list = sections();
    // A panorama in a place that is away keeps everything as it was; it is measured again when it shows.
    if (!list.length || !el.getClientRects().length) { stickyStrip(); return; }
    const last = list[list.length - 1];
    const contentRight = el.scrollLeft + last.getBoundingClientRect().right - el.getBoundingClientRect().left;
    el.toggleAttribute('data-rk-fit', sideways() && contentRight <= el.clientWidth + 1);
    if (pivot) { pivotScale(pivot); pivotFit(pivot); }
    stickyStrip();
    // A change of window-size class keeps the reading position: the current section is state, not an offset.
    const now = sideways();
    if (now !== wasSideways) { wasSideways = now; if (current) goTo(current, { focus: false, instant: true }); clicked = false; }
  }
  // A section strip that sticks (compact, a direct child of the place) publishes its height, so focus and section
  // jumps land below it.
  let wrote = false;
  function stickyStrip() {
    if (!pivot) return;
    const sticky = pivot.getClientRects().length && pivot.parentElement.classList.contains('rk-place') && getComputedStyle(pivot).position === 'sticky';
    if (sticky) { docEl.style.setProperty('--rk-pivot-h', `${pivot.offsetHeight}px`); wrote = true; }
    else if (wrote) { docEl.style.removeProperty('--rk-pivot-h'); wrote = false; }
  }
  const ro = new ResizeObserver(fit);
  ro.observe(el);
  if (pivot) ro.observe(pivot);
  const short = matchMedia('(min-height: 481px)');
  short.addEventListener('change', fit);
  el.addEventListener('wheel', wheel, { passive: false });
  el.addEventListener('keydown', key);
  el.addEventListener('scroll', queue, { passive: true });
  addEventListener('scroll', queue, { passive: true });
  for (const ev of ['wheel', 'touchstart', 'keydown']) el.addEventListener(ev, userScroll, { passive: true });
  if (pivot) { pivot.addEventListener('click', pivotClick); pivot.addEventListener('wheel', stripWheel, { passive: false }); pivotScale(pivot); }
  if (document.fonts) document.fonts.ready.then(fit);
  if (el.tabIndex < 0 && !el.hasAttribute('tabindex')) el.tabIndex = 0;
  requestAnimationFrame(() => { fit(); mark(current); });
  return {
    goTo,
    get current() { return current; },
    destroy() {
      ro.disconnect(); short.removeEventListener('change', fit);
      el.removeEventListener('wheel', wheel); el.removeEventListener('keydown', key); el.removeEventListener('scroll', queue);
      removeEventListener('scroll', queue);
      if (pivot) { pivot.removeEventListener('click', pivotClick); pivot.removeEventListener('wheel', stripWheel); }
    },
  };
}
// A strip of titles moves sideways under a vertical wheel, inside its own box only.
function stripWheel(e) {
  const strip = e.currentTarget;
  if (e.ctrlKey || e.shiftKey || Math.abs(e.deltaX) >= Math.abs(e.deltaY)) return;
  const max = strip.scrollWidth - strip.clientWidth, down = e.deltaY > 0;
  if (max <= 0 || (down ? strip.scrollLeft >= max - 1 : strip.scrollLeft <= 0)) return;
  e.preventDefault();
  strip.scrollBy({ left: e.deltaY * (e.deltaMode === 1 ? 16 : 1), behavior: 'instant' });
}

// Pivot --------------------------------------------------------------------------------------------------------------
// Inactive titles scale down from the active title's line box by the token ratio (pivot-inactive / pivot-active, or
// nav-inactive / nav-active for rk-pivot--nav), measured in the page so fluid sizes stay right.
function pivotScale(strip) {
  if (!strip.getClientRects().length) return;
  const nav = strip.classList.contains('rk-pivot--nav');
  const probe = document.createElement('span');
  probe.style.cssText = 'position:absolute;visibility:hidden;font-size:var(--rk-font-size-' + (nav ? 'nav' : 'pivot') + '-inactive)';
  strip.appendChild(probe);
  const item = strip.querySelector('a, button');
  const inactive = parseFloat(getComputedStyle(probe).fontSize), active = item ? parseFloat(getComputedStyle(item).fontSize) : inactive;
  probe.remove();
  if (inactive && active) strip.style.setProperty('--rk-pivot-scale', (inactive / active).toFixed(3));
  for (const el of strip.querySelectorAll(':scope > a, :scope > button')) if (el.offsetWidth) el.style.setProperty('--_rk-w', `${el.offsetWidth}px`);
}
function pivotFit(strip) {
  const items = [...strip.querySelectorAll(':scope > a, :scope > button')];
  if (!items.length) return;
  const last = items[items.length - 1];
  const right = strip.scrollLeft + last.getBoundingClientRect().right - strip.getBoundingClientRect().left;
  strip.toggleAttribute('data-rk-fit', right <= strip.clientWidth + 1);
}
// A strip that does not fit glides so the active title sits at its left edge; one that fits holds still.
function pivotGlide(strip, item) {
  if (!item || strip.hasAttribute('data-rk-fit')) return;
  const first = strip.querySelector('a, button');
  animateScroll(strip, item.offsetLeft - first.offsetLeft, ms('--rk-duration-pivot-glide'), bezier('--rk-ease-pivot-glide'));
}
export function pivot(el, { onSelect } = {}) {
  const tabs = () => [...el.querySelectorAll('[role=tab]')];
  const panel = (t) => document.getElementById(t.getAttribute('aria-controls'));
  function select(i, { focus = false } = {}) {
    const list = tabs(), prev = list.findIndex((t) => t.getAttribute('aria-selected') === 'true');
    if (i === prev || !list[i]) return;
    list.forEach((t, k) => { t.setAttribute('aria-selected', String(k === i)); t.tabIndex = k === i ? 0 : -1; const p = panel(t); if (p) p.hidden = k !== i; });
    pivotGlide(el, list[i]);
    const d = ms('--rk-duration-moderate') * 0.75, p = panel(list[i]);
    if (d && p) p.animate([{ transform: `translateX(${(i > prev ? 1 : -1) * num('--rk-slide-distance')}px)`, opacity: 0 }, { transform: 'none', opacity: 1 }], { duration: d, easing: css('--rk-ease-out') });
    if (focus) list[i].focus();
    if (onSelect) onSelect(i, list[i]);
  }
  const click = (e) => { const t = e.target.closest('[role=tab]'); if (t) select(tabs().indexOf(t)); };
  const key = (e) => {
    const list = tabs(), i = list.indexOf(e.target);
    if (i < 0) return;
    const to = { ArrowRight: i + 1, ArrowLeft: i - 1, Home: 0, End: list.length - 1 }[e.key];
    if (to === undefined) return;
    e.preventDefault();
    select((to + list.length) % list.length, { focus: true });
  };
  const resize = () => { pivotScale(el); pivotFit(el); };
  const ro = new ResizeObserver(resize);
  el.addEventListener('click', click); el.addEventListener('keydown', key); el.addEventListener('wheel', stripWheel, { passive: false });
  ro.observe(el);
  resize();
  if (document.fonts) document.fonts.ready.then(resize);
  return { select, destroy() { el.removeEventListener('click', click); el.removeEventListener('keydown', key); el.removeEventListener('wheel', stripWheel); ro.disconnect(); } };
}

// Toaster ------------------------------------------------------------------------------------------------------------
// One toast at a time in a region that stays mounted (role=status, aria-atomic=false). A toast replacing one still
// leaving is inserted once that one has gone, so only what shows is announced. The same key with undo updates the
// open toast and restarts its window. Hover, a touch or focus holds the window. Escape inside a toast dismisses it;
// Ctrl/Cmd+Z undoes while the window is open, except in a text field. Undo is a shortcut, never the only way back.
export function toaster(region) {
  let open = null;
  const windowSec = () => Math.round(ms('--rk-duration-toast') / 1000) || 5;
  function say(msg, text, undoable, again) {
    msg.textContent = text;
    if (!undoable) return;
    const sr = document.createElement('span');
    sr.className = 'rk-toast-sr';
    sr.textContent = `, ${windowSec()} seconds to${again ? ' undo' : ''}`;
    msg.appendChild(sr);
  }
  function show(text, { key = text, undo = null, from = null, action = 'undo' } = {}) {
    if (open && open.key === key && open.undo && undo) { open.update(text, undo); return open; }
    if (open) open.leave(false);
    const WIN = windowSec();
    let left = WIN, timer = 0, hover = false, focus = false, done = false, waiting = 0;
    let plainLeft = ms('--rk-duration-toast'), since = 0;
    const el = document.createElement('div');
    el.className = 'rk-toast';
    const msg = el.appendChild(document.createElement('p'));
    msg.className = 'rk-toast-msg';
    let count = null;
    if (undo) {
      const btn = el.appendChild(document.createElement('button'));
      btn.type = 'button'; btn.className = 'rk-toast-act'; btn.setAttribute('aria-keyshortcuts', 'Control+Z Meta+Z');
      btn.append(`${action} `);
      count = btn.appendChild(document.createElement('span'));
      count.className = 'rk-toast-count'; count.setAttribute('aria-hidden', 'true');
      const line = el.appendChild(document.createElement('span'));
      line.className = 'rk-toast-line'; line.setAttribute('aria-hidden', 'true');
      btn.addEventListener('click', () => leave(true));
    }
    say(msg, text, !!undo);
    const paint = () => { if (count) { count.textContent = left; el.style.setProperty('--_rk-left', left / WIN); } };
    const tick = () => {
      timer = 0;
      if (!undo) { leave(false); return; }
      left -= 1;
      if (left <= 0) { el.style.setProperty('--_rk-left', 0); leave(false); return; }
      paint(); hold();
    };
    function hold() {
      if (done || waiting) return;
      if (hover || focus) { if (timer) { clearTimeout(timer); timer = 0; if (!undo) plainLeft -= Date.now() - since; } }
      else if (!timer) { since = Date.now(); timer = setTimeout(tick, undo ? 1000 : Math.max(0, plainLeft)); }
    }
    function leave(doUndo) {
      if (done) return;
      done = true; clearTimeout(timer);
      if (waiting) { clearTimeout(waiting); waiting = 0; if (open === api) open = null; if (doUndo && undo) undo(); return; }
      const hadFocus = el.contains(document.activeElement);
      el.setAttribute('data-rk-phase', 'leaving');
      setTimeout(() => el.remove(), ms('--rk-duration-short'));
      if (open === api) open = null;
      if (doUndo && undo) undo();
      if (hadFocus) {
        const target = from && from.isConnected && !from.closest('[hidden]') ? from : document.querySelector('.rk-place:not([hidden]) .rk-title');
        if (target) { if (!target.hasAttribute('tabindex') && target.matches('.rk-title')) target.setAttribute('tabindex', '-1'); target.focus(); }
      }
    }
    const api = {
      key, el,
      leave,
      undo: undo ? () => leave(true) : null,
      update(t, u) { undo = u; say(msg, t, !!u, true); left = WIN; paint(); clearTimeout(timer); timer = 0; hold(); },
    };
    el.addEventListener('pointerenter', () => { hover = true; hold(); });
    el.addEventListener('pointerleave', () => { hover = false; hold(); });
    el.addEventListener('focusin', () => { focus = true; hold(); });
    el.addEventListener('focusout', (e) => { if (!el.contains(e.relatedTarget)) { focus = false; hold(); } });
    paint();
    open = api;
    const insert = () => { waiting = 0; region.appendChild(el); hold(); };
    if (region.querySelector('.rk-toast[data-rk-phase="leaving"]')) waiting = setTimeout(insert, ms('--rk-duration-short')); else insert();
    return api;
  }
  const offEsc = keys.on(30, (e) => {
    if (e.key !== 'Escape' || !open || !open.el.contains(e.target)) return false;
    open.leave(false); return true;
  });
  const offUndo = keys.on(25, (e) => {
    if (!open || !open.undo || e.shiftKey || !(e.ctrlKey || e.metaKey) || e.key.toLowerCase() !== 'z' || isEditable(e.target)) return false;
    open.undo(); return true;
  });
  return {
    show,
    get open() { return open; },
    dismiss() { if (open) open.leave(false); },
    destroy() { offEsc(); offUndo(); },
  };
}

// Banner -------------------------------------------------------------------------------------------------------------
// One lasting system state at a time, the most severe first (order). states: { name: { word, status, alert, text,
// action, since: 'since' | 'until', until, on, off } }. A new state fades in once and holds still; a change of kind
// glides to its new height; an ending state is an event, so the toaster says so once the banner has gone. Both live
// regions stay mounted. The app bar follows with data-rk-state (halted for error, paused for warning).
export function banner(el, { states, order = Object.keys(states), appbar = null, liveWord = null, toaster: toasts = null, onAction = null } = {}) {
  const alertRegion = el.querySelector('[role=alert]'), statusRegion = el.querySelector('[role=status]');
  const remedy = el.querySelector('.rk-actions .rk-btn');
  const on = Object.fromEntries(order.map((n) => [n, false]));
  let shown = null, clearing = 0, setBy = null;
  if (!el.hasAttribute('data-rk-phase') && !el.querySelector('.rk-banner-word')) el.setAttribute('data-rk-phase', 'clear');
  // The banner's height on the root, written once the banner has settled: scroll-padding keeps focus clear of it,
  // and toasts hang below it where anchor positioning is missing.
  let settle = 0;
  const ro = new ResizeObserver(() => {
    clearTimeout(settle);
    settle = setTimeout(() => docEl.style.setProperty('--rk-banner-h', `${Math.ceil(el.getBoundingClientRect().height)}px`), 60);
  });
  ro.observe(el, { box: 'border-box' });
  const now = () => { const d = new Date(); return `${String(d.getHours()).padStart(2, '0')}:${String(d.getMinutes()).padStart(2, '0')}`; };
  function write(name) {
    const s = states[name], t = s.until || now();
    const head = document.createElement('span'); head.className = 'rk-banner-head';
    const word = head.appendChild(document.createElement('span')); word.className = 'rk-banner-word'; word.textContent = s.word || name;
    const time = head.appendChild(document.createElement('time')); time.className = 'rk-banner-time'; time.dateTime = t; time.textContent = `${s.since || 'since'} ${t}`;
    const msg = document.createElement('span'); msg.className = 'rk-banner-msg'; msg.textContent = s.text;
    alertRegion.replaceChildren(); statusRegion.replaceChildren();
    (s.alert ? alertRegion : statusRegion).replaceChildren(head, msg);
  }
  function show(name) {
    const s = states[name];
    clearTimeout(clearing);
    const standing = el.getAttribute('data-rk-phase') !== 'clear', h0 = el.getBoundingClientRect().height;
    el.removeAttribute('data-rk-phase');
    if (s.status) el.dataset.status = s.status; else delete el.dataset.status;
    el.style.animation = 'none'; void el.offsetWidth; el.style.animation = '';
    write(name);
    if (remedy) { remedy.textContent = s.action || ''; remedy.parentElement.hidden = !s.action; remedy.removeAttribute('aria-disabled'); }
    // A change of kind, or a state that returns while the banner leaves, glides to its new height.
    const h1 = el.getBoundingClientRect().height, d = ms('--rk-duration-short');
    if (standing && d && Math.abs(h1 - h0) >= 1) el.animate([{ height: `${h0}px` }, { height: `${h1}px` }], { duration: d, easing: css('--rk-ease-in') });
  }
  function clear(then) {
    if (el.contains(document.activeElement)) {
      const target = setBy && setBy.isConnected && !setBy.closest('[hidden]') ? setBy : document.querySelector('.rk-place:not([hidden]) .rk-title');
      if (target) { if (target.matches('.rk-title') && !target.hasAttribute('tabindex')) target.setAttribute('tabindex', '-1'); target.focus(); }
    }
    el.setAttribute('data-rk-phase', 'leaving');
    clearing = setTimeout(() => {
      el.setAttribute('data-rk-phase', 'clear');
      delete el.dataset.status;
      alertRegion.replaceChildren(); statusRegion.replaceChildren();
      if (then) then();
    }, ms('--rk-duration-short'));
  }
  function sync(changed, from) {
    const name = order.find((n) => on[n]) || null;
    if (appbar) {
      const top = name && states[name].status;
      if (top === 'error') appbar.setAttribute('data-rk-state', 'halted');
      else if (top === 'warning') appbar.setAttribute('data-rk-state', 'paused');
      else appbar.removeAttribute('data-rk-state');
    }
    if (liveWord) liveWord.textContent = name ? (states[name].word || name) : 'live';
    if (name === shown) {
      if (changed && toasts) toasts.show(`${on[changed] ? states[changed].on : states[changed].off.split('.')[0]}. The banner keeps showing “${states[name].word || name}” until that ends.`, { key: 'state' });
      return;
    }
    const before = shown;
    shown = name;
    if (from) setBy = from;
    if (name) { show(name); if (before && changed && !on[changed] && toasts) toasts.show(states[changed].off, { key: 'state' }); }
    else clear(() => { if (changed && toasts) toasts.show(states[changed].off, { key: 'state' }); });
    el.dispatchEvent(new CustomEvent('rk:bannerchange', { bubbles: true, detail: { state: name } }));
  }
  const act = () => { if (shown && remedy.getAttribute('aria-disabled') !== 'true') (onAction ? onAction(shown) : set(shown, false)); };
  if (remedy) remedy.addEventListener('click', act);
  function set(name, value, { from = null } = {}) {
    if (!(name in on)) throw new Error(`unknown banner state ${name}`);
    if (on[name] === !!value) return;
    on[name] = !!value;
    sync(name, from);
  }
  sync(null, null);
  return {
    set,
    toggle(name, opts) { set(name, !on[name], opts); },
    is(name) { return on[name]; },
    get shown() { return shown; },
    destroy() { ro.disconnect(); if (remedy) remedy.removeEventListener('click', act); },
  };
}

// Sheet --------------------------------------------------------------------------------------------------------------
// <dialog class="rk-sheet"> opened with showModal(). A tap on the shading cancels where closedby is not supported.
export function sheet(dialog) {
  const click = (e) => {
    if ('closedBy' in HTMLDialogElement.prototype || e.target !== dialog || dialog.getAttribute('closedby') === 'closerequest') return;
    const r = dialog.getBoundingClientRect();
    if (e.clientX < r.left || e.clientX > r.right || e.clientY < r.top || e.clientY > r.bottom) dialog.close('cancel');
  };
  if (!dialog.hasAttribute('closedby')) dialog.setAttribute('closedby', 'any');
  dialog.addEventListener('click', click);
  return { destroy() { dialog.removeEventListener('click', click); } };
}

// Data states ------------------------------------------------------------------------------------------------------
// busy(region, work): the region is aria-busy while work (a promise or a function returning one) runs. The line shows
// only after busy-delay (CSS), and once shown stays at least busy-min, so an answer never makes it flash. Resolves or
// rejects with the work.
export async function busy(region, work) {
  const t0 = performance.now();
  region.setAttribute('aria-busy', 'true');
  try {
    return await (typeof work === 'function' ? work() : work);
  } finally {
    const shown = performance.now() - t0 - ms('--rk-duration-busy-delay');
    if (shown > 0) await new Promise((r) => setTimeout(r, Math.max(0, ms('--rk-duration-busy-min') - shown)));
    region.removeAttribute('aria-busy');
  }
}
// pending(button, work, { label }): the button says what it is doing ("saving…") at the same width, refuses further
// clicks, and runs the line under it after busy-delay. Label: the option, data-rk-pending-label, or its text plus "…".
export async function pending(button, work, { label } = {}) {
  if (button.hasAttribute('data-rk-pending')) return undefined;
  const nodes = [...button.childNodes], width = button.offsetWidth;
  const block = (e) => { e.preventDefault(); e.stopImmediatePropagation(); };
  button.style.minWidth = `${width}px`;
  button.textContent = label || button.dataset.rkPendingLabel || `${button.textContent.trim()}…`;
  button.setAttribute('aria-disabled', 'true');
  button.setAttribute('data-rk-pending', '');
  button.addEventListener('click', block, true);
  try {
    return await (typeof work === 'function' ? work() : work);
  } finally {
    button.removeEventListener('click', block, true);
    button.replaceChildren(...nodes);
    button.removeAttribute('data-rk-pending');
    button.removeAttribute('aria-disabled');
    button.style.minWidth = '';
  }
}

// Floating surfaces -------------------------------------------------------------------------------------------------
// place(el, anchor): puts a fixed floating surface below its control, aligned to its leading edge, or above it when
// there is no room below; it never leaves the viewport. data-rk-side says which side it took.
export function place(el, anchor) {
  const a = anchor.getBoundingClientRect(), gap = 4, edge = num('--rk-space-s') || 8;
  el.style.left = '0px'; el.style.top = '0px';
  const w = el.offsetWidth, h = el.offsetHeight, vw = document.documentElement.clientWidth, vh = innerHeight;
  const below = vh - a.bottom - gap - edge, above = a.top - gap - edge;
  const side = h <= below || below >= above ? 'below' : 'above';
  const top = side === 'below' ? a.bottom + gap : Math.max(edge, a.top - gap - h);
  const left = Math.min(Math.max(edge, a.left), Math.max(edge, vw - w - edge));
  el.style.left = `${Math.round(left)}px`; el.style.top = `${Math.round(top)}px`;
  el.dataset.rkSide = side;
}
const canPopover = typeof HTMLElement !== 'undefined' && 'popover' in HTMLElement.prototype;
function openFloat(el) { if (canPopover) { if (!el.matches(':popover-open')) el.showPopover(); } else { el.hidden = false; el.setAttribute('data-rk-open', ''); } }
function closeFloat(el) { if (canPopover) { if (el.matches(':popover-open')) el.hidePopover(); } else { el.hidden = true; el.removeAttribute('data-rk-open'); } }
const isOpen = (el) => (canPopover ? el.matches(':popover-open') : !el.hidden);

// menu(trigger, el, { onSelect }): a menu button. el is a .rk-float of .rk-menu-item buttons (role menuitem,
// menuitemcheckbox or menuitemradio), opened by a click, Enter, Space or ↓ (↑ from the end). In the menu: ↑ ↓ wrap,
// Home End, a letter jumps to the next item starting with it; Enter or Space chooses; Escape, Tab or a click outside
// closes and focus returns to the trigger after a choice or Escape. A checkbox item toggles and leaves the menu open,
// so several can be ticked in a row. With summary ("status"), the trigger says what is ticked: "status: failed, done",
// or "status: any" when nothing is (a filter button).
export function menu(trigger, el, { onSelect, summary = null } = {}) {
  el.id = el.id || `rk-menu-${Math.random().toString(36).slice(2, 8)}`;
  el.setAttribute('role', 'menu');
  if (canPopover) el.setAttribute('popover', 'auto'); else el.hidden = true;
  el.classList.add('rk-float');
  trigger.setAttribute('aria-haspopup', 'menu');
  trigger.setAttribute('aria-controls', el.id);
  trigger.setAttribute('aria-expanded', 'false');
  const items = () => [...el.querySelectorAll('.rk-menu-item')].filter((i) => i.getClientRects().length);
  const focusItem = (i) => { const list = items(); if (list.length) list[(i + list.length) % list.length].focus(); };
  function summarize() {
    if (!summary) return;
    const on = [...el.querySelectorAll('[aria-checked="true"]')].map((i) => i.textContent.trim());
    trigger.textContent = `${summary}: ${on.length ? on.join(', ') : 'any'}`;
  }
  summarize();
  // With popovers the trigger is the menu's invoker: the browser toggles it on click and a click on the trigger
  // never counts as a click outside. Placement and focus follow the toggle.
  let first = 0;
  if (canPopover && 'popoverTargetElement' in trigger) trigger.popoverTargetElement = el;
  const shown = () => { place(el, trigger); trigger.setAttribute('aria-expanded', 'true'); focusItem(first); first = 0; };
  function open(at = 0) {
    if (isOpen(el)) { focusItem(at); return; }
    first = at;
    openFloat(el);
    if (!canPopover) shown();
  }
  function close(refocus = true) {
    if (!isOpen(el)) return;
    closeFloat(el);
    trigger.setAttribute('aria-expanded', 'false');
    if (refocus) trigger.focus();
  }
  const toggle = (e) => { if (e.newState === 'open') shown(); else trigger.setAttribute('aria-expanded', 'false'); };
  const click = (e) => {
    if (canPopover && trigger.popoverTargetElement === el) return;                    // the browser toggles
    e.preventDefault(); if (isOpen(el)) close(); else open(0);
  };
  const triggerKey = (e) => {
    if (e.key === 'ArrowDown' || e.key === 'ArrowUp') { e.preventDefault(); open(e.key === 'ArrowUp' ? -1 : 0); }
  };
  const key = (e) => {
    const list = items(), i = list.indexOf(document.activeElement);
    if (e.key === 'ArrowDown') { e.preventDefault(); focusItem(i + 1); }
    else if (e.key === 'ArrowUp') { e.preventDefault(); focusItem(i - 1); }
    else if (e.key === 'Home') { e.preventDefault(); focusItem(0); }
    else if (e.key === 'End') { e.preventDefault(); focusItem(-1); }
    else if (e.key === 'Escape') { e.preventDefault(); close(); }
    else if (e.key === 'Tab') close(false);
    else if (e.key.length === 1 && /\S/.test(e.key) && !e.ctrlKey && !e.metaKey && !e.altKey) {
      const k = e.key.toLowerCase(), order = [...list.slice(i + 1), ...list.slice(0, i + 1)];
      const hit = order.find((it) => it.textContent.trim().toLowerCase().startsWith(k));
      if (hit) hit.focus();
    }
  };
  const choose = (e) => {
    const item = e.target.closest('.rk-menu-item');
    if (!item || item.getAttribute('aria-disabled') === 'true') { if (item) e.preventDefault(); return; }
    const checkbox = item.getAttribute('role') === 'menuitemcheckbox';
    if (checkbox) item.setAttribute('aria-checked', String(item.getAttribute('aria-checked') !== 'true'));
    if (item.getAttribute('role') === 'menuitemradio') el.querySelectorAll('[role=menuitemradio]').forEach((r) => r.setAttribute('aria-checked', String(r === item)));
    summarize();
    if (!checkbox) close(); else if (!canPopover || el.matches(':popover-open')) place(el, trigger);
    if (onSelect) onSelect(item);
  };
  const outside = (e) => { if (!canPopover && isOpen(el) && !el.contains(e.target) && !trigger.contains(e.target)) close(false); };
  const reflow = () => { if (isOpen(el)) place(el, trigger); };
  trigger.addEventListener('click', click);
  trigger.addEventListener('keydown', triggerKey);
  el.addEventListener('keydown', key);
  el.addEventListener('click', choose);
  el.addEventListener('toggle', toggle);
  document.addEventListener('pointerdown', outside, true);
  addEventListener('resize', reflow);
  addEventListener('scroll', reflow, true);
  return {
    open, close,
    // Set the checkbox items from outside (a filter restored from the address); the trigger follows.
    set(checked) { el.querySelectorAll('[role=menuitemcheckbox]').forEach((i) => i.setAttribute('aria-checked', String(checked(i)))); summarize(); },
    destroy() {
      trigger.removeEventListener('click', click); trigger.removeEventListener('keydown', triggerKey);
      el.removeEventListener('keydown', key); el.removeEventListener('click', choose); el.removeEventListener('toggle', toggle);
      document.removeEventListener('pointerdown', outside, true); removeEventListener('resize', reflow); removeEventListener('scroll', reflow, true);
    },
  };
}

// Table ------------------------------------------------------------------------------------------------------------
// table(el, { onSort, appbar, onSelect }): a .rk-table. A sort button in a head cell sorts by its column, ascending
// then descending, with the rows moving to their new places (FLIP); onSort takes over for data sorted elsewhere.
// Rows with a .rk-check checkbox are selectable: Shift+click selects a range, the head checkbox all or none,
// Ctrl/Cmd+A all while focus is in the table, Escape clears. A selected row gets data-rk-selected. With an appbar,
// the bar carries the selection (data-rk-selecting, .rk-ab-count, a [data-rk-clear] button). Cells get data-label
// from their head for the stacked layout of a narrow place.
export function table(el, { onSort, appbar = null, onSelect } = {}) {
  const head = () => (el.tHead ? [...el.tHead.rows[0].cells] : []);
  const rows = () => (el.tBodies[0] ? [...el.tBodies[0].rows] : []);
  const box = (row) => row.querySelector('.rk-check input[type=checkbox]');
  const all = () => el.tHead && el.tHead.querySelector('.rk-check input[type=checkbox]');
  const collator = new Intl.Collator(undefined, { numeric: true, sensitivity: 'base' });
  let anchor = null, shift = false;
  function label() {
    const names = head().map((th) => th.textContent.trim());
    for (const r of rows()) [...r.cells].forEach((c, i) => { if (!c.hasAttribute('data-label') && !c.classList.contains('rk-check') && names[i]) c.dataset.label = names[i]; });
  }
  const value = (cell) => (cell ? (cell.dataset.sort !== undefined ? cell.dataset.sort : cell.textContent.trim()) : '');
  function sortRows(column, dir) {
    const list = rows(), body = el.tBodies[0];
    const before = list.length <= 200 ? new Map(list.map((r) => [r, r.getBoundingClientRect().top])) : null;
    const sign = dir === 'descending' ? -1 : 1;
    list.sort((a, b) => {
      const x = value(a.cells[column]), y = value(b.cells[column]), nx = Number(x), ny = Number(y);
      return sign * (x !== '' && y !== '' && Number.isFinite(nx) && Number.isFinite(ny) ? nx - ny : collator.compare(x, y));
    }).forEach((r) => body.appendChild(r));
    const d = ms('--rk-duration-moderate');
    if (before && d) for (const r of list) {
      const dy = before.get(r) - r.getBoundingClientRect().top;
      if (dy) r.animate([{ transform: `translateY(${dy}px)` }, { transform: 'none' }], { duration: d, easing: css('--rk-ease-base') });
    }
  }
  function sort(th, dir) {
    for (const h of head()) if (h !== th && h.hasAttribute('aria-sort')) h.setAttribute('aria-sort', 'none');
    th.setAttribute('aria-sort', dir);
    if (onSort) onSort({ column: th.cellIndex, dir, th }); else sortRows(th.cellIndex, dir);
  }
  const selected = () => rows().filter((r) => r.hasAttribute('data-rk-selected'));
  // Rows a filter has hidden are out of the selection: they are unticked and never ticked by all or a range.
  const selectable = () => rows().filter((r) => box(r) && !r.hidden);
  function sync() {
    for (const r of rows()) { const b = box(r); if (b && r.hidden && b.checked) b.checked = false; }
    const list = selectable(), count = list.filter((r) => box(r).checked).length;
    for (const r of list) r.toggleAttribute('data-rk-selected', box(r).checked);
    const a = all();
    if (a) { a.checked = !!count && count === list.length; a.indeterminate = !!count && count < list.length; }
    if (appbar) {
      appbar.toggleAttribute('data-rk-selecting', count > 0);
      const c = appbar.querySelector('.rk-ab-count');
      if (c) c.textContent = `${count} selected`;
    }
    if (onSelect) onSelect(selected());
    el.dispatchEvent(new CustomEvent('rk:selectionchange', { bubbles: true, detail: { count, rows: selected() } }));
  }
  function setAll(on) { for (const r of selectable()) { const b = box(r); if (!b.disabled) b.checked = on; } sync(); }
  function clear({ focus = true } = {}) {
    const hadFocus = appbar && appbar.contains(document.activeElement);
    setAll(false);
    if (focus && hadFocus) { const t = all() || (rows()[0] && box(rows()[0])); if (t) t.focus(); }
  }
  const click = (e) => {
    const sorter = e.target.closest('.rk-sort');
    if (sorter) { const th = sorter.closest('th'); sort(th, th.getAttribute('aria-sort') === 'ascending' ? 'descending' : 'ascending'); return; }
    if (e.target.matches('.rk-check input[type=checkbox]')) shift = e.shiftKey;
  };
  const change = (e) => {
    const b = e.target;
    if (!b.matches('.rk-check input[type=checkbox]')) return;
    if (b === all()) { setAll(b.checked); return; }
    const row = b.closest('tr');
    if (shift && anchor && anchor.isConnected) {
      const list = selectable(), [i, j] = [list.indexOf(anchor), list.indexOf(row)].sort((m, n) => m - n);
      list.slice(i, j + 1).forEach((r) => { const x = box(r); if (x && !x.disabled) x.checked = b.checked; });
    }
    anchor = row; shift = false;
    sync();
  };
  const key = (e) => {
    if ((e.ctrlKey || e.metaKey) && !e.shiftKey && !e.altKey && e.key.toLowerCase() === 'a' && !isEditable(e.target) && selectable().length) { e.preventDefault(); setAll(true); }
  };
  const offEsc = keys.on(20, (e) => {
    if (e.key !== 'Escape' || !selected().length || isEditable(e.target)) return false;
    if (!el.contains(e.target) && !(appbar && appbar.contains(e.target))) return false;
    clear(); return true;
  });
  const clearClick = (e) => { if (e.target.closest('[data-rk-clear]')) clear(); };
  el.addEventListener('click', click);
  el.addEventListener('change', change);
  el.addEventListener('keydown', key);
  if (appbar) appbar.addEventListener('click', clearClick);
  label();
  sync();
  return {
    selected, clear, selectAll: () => setAll(true), sort: (column, dir = 'ascending') => sort(head()[column], dir),
    refresh() { label(); sync(); },
    destroy() { el.removeEventListener('click', click); el.removeEventListener('change', change); el.removeEventListener('keydown', key); if (appbar) appbar.removeEventListener('click', clearClick); offEsc(); },
  };
}

// Split ------------------------------------------------------------------------------------------------------------
// split(el, { key }): a .rk-split of .rk-split-list, .rk-split-handle and .rk-split-detail. Sideways both panes show;
// the handle drags the list's width (--rk-split-w), ← → move it by space-m (Shift: four times), Home End to the
// limits, a double click resets it; the width is remembered under key. Compact, one pane shows at a time:
// show('detail') turns to the detail, show('list') back to the list, with the turnstile. sideways says which.
export function split(el, { key = null } = {}) {
  const list = el.querySelector(':scope > .rk-split-list'), detail = el.querySelector(':scope > .rk-split-detail');
  const handle = el.querySelector(':scope > .rk-split-handle');
  const isSideways = () => getComputedStyle(el).display === 'grid';
  let showing = 'list', was = null;
  const store = { get() { try { return key && parseFloat(localStorage.getItem(key)); } catch { return NaN; } }, set(v) { try { if (key) localStorage.setItem(key, String(v)); } catch { /* this visit only */ } } };
  function limits() { const min = px('--rk-pane-min') || 240; return [min, Math.max(min, el.clientWidth - min - 1)]; }
  function setWidth(px, remember = true) {
    const [min, max] = limits(), w = Math.round(Math.min(max, Math.max(min, px)));
    el.style.setProperty('--rk-split-w', `${w}px`);
    if (handle) { handle.setAttribute('aria-valuenow', String(w)); handle.setAttribute('aria-valuemin', String(Math.round(min))); handle.setAttribute('aria-valuemax', String(Math.round(max))); }
    if (remember) store.set(w);
  }
  function sync() {
    const now = isSideways();
    if (now === was) return;
    was = now;
    turn.finish();
    if (now) { list.hidden = false; detail.hidden = false; }
    else { list.hidden = showing !== 'list'; detail.hidden = showing !== 'detail'; }
  }
  // show(which, { focus }): compact, turns to that pane (the list comes back as a back turn); sideways, nothing moves.
  async function show(which, { focus = null, animate = true } = {}) {
    if (which === showing) return;
    showing = which;
    if (isSideways()) return;
    const from = which === 'detail' ? list : detail, to = which === 'detail' ? detail : list, back = which === 'list';
    if (animate && !from.hidden) { await turn.leave(from, { back }); await turn.enter(to, { back, focus, arriving: false }); }
    else { from.hidden = true; to.hidden = false; if (focus) focus.focus({ preventScroll: true }); }
  }
  // reveal(dir): sideways, the detail's new item slides in from its side, as a facet does.
  function reveal(dir = 1) {
    const d = ms('--rk-duration-moderate') * 0.75;
    if (d && isSideways()) detail.animate([{ transform: `translateX(${dir * num('--rk-slide-distance')}px)`, opacity: 0 }, { transform: 'none', opacity: 1 }], { duration: d, easing: css('--rk-ease-out') });
  }
  let drag = null;
  const down = (e) => { if (e.button > 0) return; drag = { x: e.clientX, w: list.getBoundingClientRect().width }; handle.setPointerCapture(e.pointerId); handle.setAttribute('data-rk-dragging', ''); e.preventDefault(); };
  const move = (e) => { if (drag) setWidth(drag.w + (e.clientX - drag.x), false); };
  const up = () => { if (!drag) return; drag = null; handle.removeAttribute('data-rk-dragging'); setWidth(list.getBoundingClientRect().width); };
  const keydown = (e) => {
    const [min, max] = limits(), w = list.getBoundingClientRect().width, step = num('--rk-space-m') * (e.shiftKey ? 4 : 1);
    const to = { ArrowLeft: w - step, ArrowRight: w + step, Home: min, End: max }[e.key];
    if (to === undefined) return;
    e.preventDefault(); setWidth(to);
  };
  const reset = () => { el.style.removeProperty('--rk-split-w'); if (key) try { localStorage.removeItem(key); } catch { /* ignore */ } setWidth(list.getBoundingClientRect().width, false); };
  if (handle) {
    handle.setAttribute('role', 'separator'); handle.setAttribute('aria-orientation', 'vertical');
    if (!handle.hasAttribute('tabindex')) handle.tabIndex = 0;
    if (list.id) handle.setAttribute('aria-controls', list.id);
    handle.addEventListener('pointerdown', down); handle.addEventListener('pointermove', move);
    handle.addEventListener('pointerup', up); handle.addEventListener('pointercancel', up);
    handle.addEventListener('keydown', keydown); handle.addEventListener('dblclick', reset);
  }
  const ro = new ResizeObserver(() => { sync(); if (isSideways() && handle) setWidth(list.getBoundingClientRect().width, false); });
  ro.observe(el);
  const saved = store.get();
  if (saved) el.style.setProperty('--rk-split-w', `${saved}px`);
  sync();
  return {
    show, reveal,
    get sideways() { return isSideways(); },
    get showing() { return showing; },
    destroy() {
      ro.disconnect();
      if (handle) { handle.removeEventListener('pointerdown', down); handle.removeEventListener('pointermove', move); handle.removeEventListener('pointerup', up); handle.removeEventListener('pointercancel', up); handle.removeEventListener('keydown', keydown); handle.removeEventListener('dblclick', reset); }
    },
  };
}

// Palette ----------------------------------------------------------------------------------------------------------
// palette(el, { items, trigger, router, shortcut }): go to a place or run a command, from a list that rises from the
// app bar at every width. el is a .rk-palette holding an input and a list; items() returns { label, group, keys,
// keywords, href | run }. Typing filters (every word must start a word of the label or its keywords); ↑ ↓ move,
// Enter runs, Escape clears the text, then closes. Ctrl/Cmd+K (shortcut) opens it from anywhere.
export function palette(el, { items = () => [], trigger = null, router = null, shortcut = 'k' } = {}) {
  const input = el.querySelector('input'), list = el.querySelector('ul, ol');
  el.id = el.id || `rk-palette-${Math.random().toString(36).slice(2, 8)}`;
  list.id = list.id || `${el.id}-list`;
  if (canPopover) el.setAttribute('popover', 'auto'); else el.hidden = true;
  el.classList.add('rk-float');
  input.setAttribute('role', 'combobox'); input.setAttribute('aria-controls', list.id); input.setAttribute('aria-expanded', 'false');
  input.setAttribute('aria-autocomplete', 'list'); input.setAttribute('autocomplete', 'off'); input.spellcheck = false;
  list.setAttribute('role', 'listbox');
  if (trigger) { trigger.setAttribute('aria-haspopup', 'dialog'); trigger.setAttribute('aria-controls', el.id); trigger.setAttribute('aria-expanded', 'false'); }
  let shown = [], active = 0, opener = null;
  const words = (t) => t.toLowerCase().split(/[^\p{L}\p{N}]+/u).filter(Boolean);
  function match(it, q) {
    if (!q) return 1;
    const hay = words(`${it.label} ${it.keywords || ''}`), label = it.label.toLowerCase();
    if (!words(q).every((w) => hay.some((h) => h.startsWith(w)))) return label.includes(q.toLowerCase()) ? 1 : 0;
    return label.startsWith(q.toLowerCase()) ? 3 : 2;
  }
  function render() {
    const q = input.value.trim(), all = (typeof items === 'function' ? items() : items) || [];
    const groups = [];
    for (const it of all) if (!groups.includes(it.group || '')) groups.push(it.group || '');
    shown = [];
    all.forEach((it, i) => { const score = match(it, q); if (score) shown.push({ it, score, i }); });
    shown.sort((a, b) => groups.indexOf(a.it.group || '') - groups.indexOf(b.it.group || '') || (q ? b.score - a.score : 0) || a.i - b.i);
    active = 0;
    list.replaceChildren();
    let group = null;
    shown.forEach(({ it }, k) => {
      if ((it.group || '') !== group) {
        group = it.group || '';
        if (group) { const h = document.createElement('li'); h.className = 'rk-menu-label'; h.setAttribute('role', 'presentation'); h.textContent = group; list.appendChild(h); }
      }
      const li = document.createElement('li');
      li.className = 'rk-menu-item'; li.id = `${el.id}-o${k}`; li.setAttribute('role', 'option');
      li.append(it.label);
      if (it.keys) { const kk = document.createElement('span'); kk.className = 'rk-menu-key'; kk.textContent = it.keys; li.appendChild(kk); }
      li.addEventListener('pointermove', () => mark(k));
      li.addEventListener('click', () => run(k));
      list.appendChild(li);
    });
    if (!shown.length) {
      const li = document.createElement('li'); li.className = 'rk-empty rk-palette-empty'; li.setAttribute('role', 'presentation');
      li.textContent = `Nothing to go to or run matches “${q}”.`; list.appendChild(li);
    }
    mark(0);
  }
  function mark(k) {
    if (!shown.length) { input.removeAttribute('aria-activedescendant'); return; }
    active = (k + shown.length) % shown.length;
    list.querySelectorAll('[role=option]').forEach((o, i) => o.setAttribute('aria-selected', String(i === active)));
    const o = list.querySelector(`#${CSS.escape(`${el.id}-o${active}`)}`);
    input.setAttribute('aria-activedescendant', o.id);
    o.scrollIntoView({ block: 'nearest' });
  }
  function open() {
    if (isOpen(el)) { input.focus(); return; }
    opener = document.activeElement;
    input.value = '';
    openFloat(el);
    render();
    input.setAttribute('aria-expanded', 'true');
    if (trigger) trigger.setAttribute('aria-expanded', 'true');
    input.focus();
  }
  function close(refocus = true) {
    if (!isOpen(el)) return;
    closeFloat(el);
    input.setAttribute('aria-expanded', 'false');
    if (trigger) trigger.setAttribute('aria-expanded', 'false');
    if (refocus && opener && opener.isConnected) opener.focus();
  }
  function run(k) {
    const pick = shown[k];
    if (!pick) return;
    close(!pick.it.href);
    if (pick.it.href) { if (router) router.navigate(pick.it.href); else location.href = pick.it.href; }
    else if (pick.it.run) pick.it.run();
  }
  const key = (e) => {
    if (e.key === 'ArrowDown') { e.preventDefault(); mark(active + 1); }
    else if (e.key === 'ArrowUp') { e.preventDefault(); mark(active - 1); }
    else if (e.key === 'Enter') { e.preventDefault(); run(active); }
    else if (e.key === 'Escape') { e.preventDefault(); if (input.value) { input.value = ''; render(); } else close(); }
    else if (e.key === 'Tab') e.preventDefault();
  };
  const toggle = (e) => { if (e.newState === 'closed') { input.setAttribute('aria-expanded', 'false'); if (trigger) trigger.setAttribute('aria-expanded', 'false'); } };
  // A pointer press on the trigger would light-dismiss the open palette first; the click then reopens it. Remember.
  let wasOpen = false;
  const down = () => { wasOpen = isOpen(el); };
  const triggerClick = (e) => { e.preventDefault(); if (wasOpen) { wasOpen = false; return; } open(); };
  const offKey = keys.on(5, (e) => {
    if (!shortcut || !(e.ctrlKey || e.metaKey) || e.shiftKey || e.altKey || e.key.toLowerCase() !== shortcut) return false;
    if (isOpen(el)) close(); else open();
    return true;
  });
  const outside = (e) => { if (!canPopover && isOpen(el) && !el.contains(e.target) && !(trigger && trigger.contains(e.target))) close(false); };
  input.addEventListener('input', render);
  input.addEventListener('keydown', key);
  el.addEventListener('toggle', toggle);
  document.addEventListener('pointerdown', outside, true);
  if (trigger) { trigger.addEventListener('pointerdown', down); trigger.addEventListener('click', triggerClick); }
  return {
    open, close,
    get isOpen() { return isOpen(el); },
    destroy() {
      input.removeEventListener('input', render); input.removeEventListener('keydown', key); el.removeEventListener('toggle', toggle);
      document.removeEventListener('pointerdown', outside, true); offKey();
      if (trigger) { trigger.removeEventListener('pointerdown', down); trigger.removeEventListener('click', triggerClick); }
    },
  };
}

// App bar ----------------------------------------------------------------------------------------------------------
// appbar(el, { more }): controls that do not fit move into a "more" menu, the least important first (data-rk-priority,
// higher stays longer; then from the end). more is a .rk-ab button shown only while something has moved; its menu
// repeats each moved control's words, and choosing one clicks the control. Back, the live state and a selection's
// controls never move.
export function appbar(el, { more = el.querySelector('.rk-ab-more') } = {}) {
  if (!more) return { layout() {}, destroy() {} };
  const pop = document.createElement('div');
  pop.className = 'rk-float rk-ab-menu';
  el.appendChild(pop);
  const words = (c) => (c.getAttribute('aria-label') || c.textContent).replace(/\s+/g, ' ').trim();
  const candidates = () => [...el.querySelectorAll(':scope > .rk-ab')].filter((c) => c !== more && !c.classList.contains('rk-ab-back'))
    .map((c, i) => ({ c, i, p: Number(c.dataset.rkPriority || 0) }))
    .sort((a, b) => a.p - b.p || b.i - a.i).map((x) => x.c);
  function build() {
    pop.replaceChildren(...[...el.querySelectorAll(':scope > .rk-ab[data-rk-overflow]')].map((c) => {
      const b = document.createElement('button');
      b.type = 'button'; b.className = 'rk-menu-item'; b.setAttribute('role', 'menuitem'); b.textContent = words(c);
      b._rkFor = c;
      return b;
    }));
  }
  const fits = () => el.scrollWidth <= el.clientWidth + 1;
  function layout() {
    if (el.hasAttribute('data-rk-selecting')) return;
    el.querySelectorAll(':scope > [data-rk-overflow]').forEach((c) => c.removeAttribute('data-rk-overflow'));
    more.hidden = true;
    if (!fits()) {
      more.hidden = false;
      for (const c of candidates()) { if (fits()) break; c.setAttribute('data-rk-overflow', ''); }
    }
    build();
  }
  const m = menu(more, pop, { onSelect: (item) => { if (item._rkFor) item._rkFor.click(); } });
  const rebuild = () => build();
  more.addEventListener('pointerdown', rebuild); more.addEventListener('keydown', rebuild);
  const ro = new ResizeObserver(layout);
  ro.observe(el);
  layout();
  return { layout, destroy() { ro.disconnect(); m.destroy(); more.removeEventListener('pointerdown', rebuild); more.removeEventListener('keydown', rebuild); pop.remove(); } };
}

// Forms ------------------------------------------------------------------------------------------------------------
const uid = (p) => `${p}-${Math.random().toString(36).slice(2, 8)}`;
const controlsOf = (form) => [...form.querySelectorAll('input, select, textarea')].filter((c) => !/^(button|submit|reset|image|hidden)$/.test(c.type));
const labelOf = (c) => { const l = c.id && document.querySelector(`label[for="${CSS.escape(c.id)}"]`); return ((l && l.textContent) || c.getAttribute('aria-label') || c.name || 'field').replace(/\s+/g, ' ').trim(); };

// disable(control, reason): a control that cannot act now, and says why next to it (a .rk-hint linked by
// aria-describedby). It stays focusable (aria-disabled), so the reason is read with it; clicks do nothing.
// disable(control, null) enables it again.
export function disable(control, reason) {
  const id = control.dataset.rkWhy || uid('rk-why');
  control.dataset.rkWhy = id;
  let hint = document.getElementById(id);
  const ids = (control.getAttribute('aria-describedby') || '').split(/\s+/).filter((x) => x && x !== id);
  if (!control._rkBlock) control._rkBlock = (e) => { if (control.getAttribute('aria-disabled') === 'true') { e.preventDefault(); e.stopImmediatePropagation(); } };
  if (reason) {
    control.disabled = false;
    control.setAttribute('aria-disabled', 'true');
    if (!hint) { hint = document.createElement('span'); hint.className = 'rk-hint rk-why'; hint.id = id; control.after(hint); }
    hint.textContent = reason;
    control.setAttribute('aria-describedby', [...ids, id].join(' '));
    control.addEventListener('click', control._rkBlock, true);
  } else {
    control.removeAttribute('aria-disabled');
    if (hint) hint.remove();
    if (ids.length) control.setAttribute('aria-describedby', ids.join(' ')); else control.removeAttribute('aria-describedby');
    control.removeEventListener('click', control._rkBlock, true);
  }
}

// confirm({ title, text, kicker, ok, cancel, safe }): a short question in a sheet; resolves true when ok is chosen.
// The safe answer is the primary button and takes focus: for an action that cannot be undone, safe: 'cancel'.
// Escape and a tap on the shading answer cancel.
export function confirm({ title, text = '', kicker = '', ok = 'ok', cancel = 'cancel', safe = 'ok', root = document.querySelector('.rk') || document.body } = {}) {
  const d = document.createElement('dialog');
  d.className = 'rk-sheet';
  const tid = uid('rk-confirm');
  d.setAttribute('aria-labelledby', tid);
  const f = d.appendChild(document.createElement('form'));
  f.method = 'dialog';
  if (kicker) { const k = f.appendChild(document.createElement('p')); k.className = 'rk-kicker'; k.textContent = kicker; }
  const h = f.appendChild(document.createElement('h2')); h.className = 'rk-group'; h.id = tid; h.textContent = title;
  if (text) { const t = f.appendChild(document.createElement('p')); t.textContent = text; }
  const row = f.appendChild(document.createElement('p')); row.className = 'rk-actions';
  const b = (label, value) => { const x = row.appendChild(document.createElement('button')); x.className = `rk-btn${safe === value ? ' rk-btn--primary' : ''}`; x.value = value; x.textContent = label; if (safe === value) x.autofocus = true; return x; };
  const opener = document.activeElement;
  b(cancel, 'cancel'); b(ok, 'ok');
  root.appendChild(d);
  const sh = sheet(d);
  return new Promise((resolve) => {
    d.addEventListener('close', () => {
      sh.destroy();
      resolve(d.returnValue === 'ok');
      setTimeout(() => d.remove(), ms('--rk-duration-short'));
      if (opener && opener.isConnected && !opener.closest('[hidden]')) opener.focus({ preventScroll: true });
    }, { once: true });
    d.showModal();
  });
}

// form(el, { validators, onSubmit, submit }): checks a form late and corrects it early. validators maps a control's
// id to fn(value, control) → a cause-and-remedy sentence, or nothing. A field is checked when it is left after an
// edit, and again on every keystroke while it shows an error. On submit with errors, a summary at the top of the form
// (.rk-errors: "2 fields need attention", a link to each) takes focus. Without errors, onSubmit(values) runs while
// the submit button is pending; it may reject with { fields: { id: message } } (the server said no to a field) or
// { message } (to the whole form), which show the same way. Returns { check, setErrors, clear, destroy }.
export function form(el, { validators = {}, onSubmit = null, submit = el.querySelector('[type=submit]') } = {}) {
  el.noValidate = true;
  const edited = new Set();
  const fieldOf = (c) => c.closest('.rk-field') || c.parentElement;
  function hintOf(c) {
    const f = fieldOf(c);
    let h = f.querySelector('.rk-hint:not(.rk-why)');
    if (!h) { h = document.createElement('span'); h.className = 'rk-hint'; f.appendChild(h); }
    if (!h.id) h.id = uid('rk-hint');
    const ids = (c.getAttribute('aria-describedby') || '').split(/\s+/).filter(Boolean);
    if (!ids.includes(h.id)) c.setAttribute('aria-describedby', [...ids, h.id].join(' '));
    if (h.dataset.rkHint === undefined) h.dataset.rkHint = h.textContent;
    return h;
  }
  function setError(c, msg) {
    const f = fieldOf(c), h = hintOf(c);
    if (msg) { f.dataset.status = 'error'; c.setAttribute('aria-invalid', 'true'); h.textContent = msg; }
    else { if (f.dataset.status === 'error') delete f.dataset.status; c.removeAttribute('aria-invalid'); h.textContent = h.dataset.rkHint; }
  }
  const run = (c) => { const v = validators[c.id]; return v ? v(c.type === 'checkbox' ? c.checked : c.value, c) || null : null; };
  const check1 = (c) => { const m = run(c); setError(c, m); return m; };
  function errors() { return controlsOf(el).map((c) => ({ c, m: el.querySelector(`#${CSS.escape(c.id)}[aria-invalid="true"]`) ? hintOf(c).textContent : null })).filter((x) => x.m); }
  let box = null;
  function summarize(list, message = null) {
    if (!list.length && !message) { if (box) { box.remove(); box = null; } return; }
    if (!box) {
      box = document.createElement('div'); box.className = 'rk-errors'; box.tabIndex = -1; box.setAttribute('role', 'group');
      el.prepend(box);
    }
    const hid = uid('rk-errors');
    box.setAttribute('aria-labelledby', hid);
    const head = document.createElement('p'); head.id = hid;
    head.textContent = message || (list.length === 1 ? '1 field needs attention' : `${list.length} fields need attention`);
    const ul = document.createElement('ul');
    for (const { c, m } of list) {
      const a = document.createElement('a'); a.href = `#${c.id}`; a.textContent = `${labelOf(c)}: ${m}`;
      a.addEventListener('click', (e) => { e.preventDefault(); c.focus(); });
      ul.appendChild(document.createElement('li')).appendChild(a);
    }
    box.replaceChildren(head, ...(list.length ? [ul] : []));
    box.focus();
  }
  function check() { const list = []; for (const c of controlsOf(el)) { const m = check1(c); if (m) list.push({ c, m }); } return list; }
  function setErrors(map = {}, message = null) {
    const list = [];
    for (const [id, m] of Object.entries(map)) { const c = el.querySelector(`#${CSS.escape(id)}`); if (c) { setError(c, m); list.push({ c, m }); } }
    summarize(list, message && !list.length ? message : null);
  }
  const values = () => Object.fromEntries(controlsOf(el).filter((c) => c.id || c.name).map((c) => [c.id || c.name, c.type === 'checkbox' ? c.checked : c.value]));
  const input = (e) => { edited.add(e.target); if (e.target.getAttribute('aria-invalid') === 'true') check1(e.target); };
  const blur = (e) => { if (edited.has(e.target)) check1(e.target); };
  const onsubmit = async (e) => {
    e.preventDefault();
    const list = check();
    if (list.length) { summarize(list); return; }
    summarize([]);
    if (!onSubmit) return;
    try { await (submit ? pending(submit, () => onSubmit(values())) : onSubmit(values())); }
    catch (err) { if (err && err.fields) setErrors(err.fields, err.message); else summarize([], (err && err.message) || 'Could not save. Try again.'); }
  };
  el.addEventListener('input', input);
  el.addEventListener('focusout', blur);
  el.addEventListener('submit', onsubmit);
  return {
    check, setErrors,
    clear() { edited.clear(); controlsOf(el).forEach((c) => setError(c, null)); summarize([]); },
    get errors() { return errors(); },
    destroy() { el.removeEventListener('input', input); el.removeEventListener('focusout', blur); el.removeEventListener('submit', onsubmit); },
  };
}

// guard(el, { router, what }): unsaved input is not lost by accident. While the form differs from how it was
// loaded (or last saved: clean()), leaving its place asks "Leave without saving?" (keep editing is the safe answer)
// and closing the window asks the browser's question. Discarding puts the form back as it was.
export function guard(el, { router = null, what = 'This form' } = {}) {
  const state = (c) => (c.type === 'checkbox' || c.type === 'radio' ? c.checked : c.value);
  let saved = new Map();
  const clean = () => { saved = new Map(controlsOf(el).map((c) => [c, state(c)])); };
  const changed = () => controlsOf(el).filter((c) => saved.has(c) && saved.get(c) !== state(c));
  const restore = () => { for (const [c, v] of saved) { if (c.type === 'checkbox' || c.type === 'radio') c.checked = v; else c.value = v; } el.dispatchEvent(new Event('rk:restored', { bubbles: true })); };
  clean();
  const unload = (e) => { if (changed().length) { e.preventDefault(); e.returnValue = ''; } };
  addEventListener('beforeunload', unload);
  const place = el.closest('[data-rk-place]');
  const off = router ? router.guard(() => {
    const n = changed().length;
    if (!n || (place && place.dataset.rkPlace !== router.current)) return true;
    return confirm({ kicker: 'unsaved', title: 'Leave without saving?', text: `${what} has ${n === 1 ? '1 unsaved change' : `${n} unsaved changes`}.`, ok: 'discard changes', cancel: 'keep editing', safe: 'cancel' })
      .then((leave) => { if (leave) restore(); return leave; });
  }) : () => {};
  return { get dirty() { return changed().length > 0; }, changed, clean, restore, destroy() { removeEventListener('beforeunload', unload); off(); } };
}

// Start --------------------------------------------------------------------------------------------------------------
// The defaults for a page: restore the user's prefs, press on tiles, sheets, and the first paint's build-in. An
// embedded page whose host owns theme, density and motion passes restore: false.
export function start({ root = document, restore = true } = {}) {
  if (restore) prefs.restore();
  const parts = [press(root)];
  root.querySelectorAll('dialog.rk-sheet').forEach((d) => parts.push(sheet(d)));
  arrive(docEl);
  return { destroy() { parts.forEach((p) => p.destroy()); } };
}

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
     start      the defaults for a page: prefs, press, sheets, arrival build-in
     tokens     token values as they apply right now (theme, density, reduced motion) */

export const version = '0.2.0';

// Tokens -------------------------------------------------------------------------------------------------------------
const docEl = document.documentElement;
function css(name, el = docEl) { return getComputedStyle(el).getPropertyValue(name).trim(); }
function ms(name, el) { const v = css(name, el); return (parseFloat(v) || 0) * (/ms$/.test(v) ? 1 : 1000); }
function num(name, el) { return parseFloat(css(name, el)) || 0; }
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
export const tokens = { value: css, ms, number: num };
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
export function places({ root = document, mode = 'hash', base = '', home, title = document.title, onChange } = {}) {
  const all = () => [...root.querySelectorAll('[data-rk-place]')];
  const byName = (name) => (name ? root.querySelector(`[data-rk-place="${CSS.escape(name)}"]`) : null);
  home = home || (all()[0] && all()[0].dataset.rkPlace);
  const urlOf = (name) => (mode === 'hash' ? (name === home ? location.pathname + location.search : `#${encodeURIComponent(name)}`) : `${base}/${name === home ? '' : encodeURIComponent(name)}`);
  const nameOf = (href) => {
    const u = new URL(href, location.href);
    if (u.origin !== location.origin) return null;
    if (mode === 'hash' && (u.pathname !== location.pathname || u.search !== location.search)) return null;
    if (mode === 'path' && !(u.pathname === base || u.pathname.startsWith(`${base}/`))) return null;
    const name = decodeURIComponent(mode === 'hash' ? u.hash.slice(1) : u.pathname.slice(base.length).replace(/^\/|\/$/g, '')) || home;
    return byName(name) ? name : null;
  };
  const parentOf = (name) => { const p = byName(name) && byName(name).dataset.rkParent; return byName(p) ? p : home; };
  const titleOf = (name) => { const h = byName(name).querySelector('.rk-title'); const t = h && h.textContent.trim(); return name === home || !t ? title : `${t} · ${title}`; };
  // Everything that scrolls in a place keeps its offset while the place is away.
  const scrollers = (el) => [el, ...el.querySelectorAll('.rk-panorama, .rk-sec, [data-rk-scroll]')];
  let current = home, index = 0, below = null, busy = Promise.resolve();
  const backButtons = () => document.querySelectorAll('.rk-ab-back[data-rk-back]');
  const showBack = () => backButtons().forEach((b) => { b.hidden = current === home && index === 0; });
  function snapshot(opener) {
    const el = byName(current), prev = (history.state && history.state.rk) || {};
    return { ...(history.state || {}), rk: { ...prev, place: current, index, below, scrollY, scroll: scrollers(el).map((s) => [s.scrollLeft, s.scrollTop]), opener: opener || prev.opener || null } };
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
  function show(name, state, { instant = false, back = false, pressed = null } = {}) {
    const prev = current, from = byName(prev), to = byName(name);
    current = name;
    below = (state && state.below) || (history.state && history.state.rk && history.state.rk.below) || null;
    document.title = titleOf(name);
    showBack();
    const run = async () => {
      if (from && from !== to) {
        if (instant) { turn.finish(); from.hidden = true; } else await turn.leave(from, { back, pressed });
      }
      to.hidden = false;
      restore(to, state);
      const f = focusTarget(to, state, back);
      if (instant) { turn.finish(); if (!back) arrive(to); f.focus({ preventScroll: true }); } else await turn.enter(to, { back, focus: f });
      if (onChange) onChange({ from: prev, to: name, back });
    };
    busy = busy.then(run, run);
    return busy;
  }
  function navigate(target, { replace = false, pressed = null } = {}) {
    const name = byName(target) ? target : nameOf(target);
    if (!name || name === current) return Promise.resolve();
    turn.finish();
    history.replaceState(snapshot(pressed && pressed.getAttribute('href')), '');
    const entry = { rk: { place: name, index: replace ? index : index + 1, below: replace ? below : current } };
    if (replace) history.replaceState(entry, '', urlOf(name)); else history.pushState(entry, '', urlOf(name));
    index = entry.rk.index;
    return show(name, entry.rk, { pressed });
  }
  // back(): one step back in history; at the bottom of the app's history, up to the parent place instead.
  // back(name): up to that place, by history when it is the place below.
  function back(target) {
    if (target && target !== below) { navigate(target); return true; }
    if (index > 0) { turn.finish(); history.replaceState(snapshot(), ''); history.back(); return true; }
    if (current === home) return false;
    const up = target || parentOf(current);
    history.replaceState({ rk: { place: up, index: 0, below: null } }, '', urlOf(up));
    show(up, null, { back: true });
    return true;
  }
  const pop = (e) => {
    const state = e.state && e.state.rk;
    const name = (state && byName(state.place) && state.place) || nameOf(location.href);
    if (!name) return;
    turn.finish();
    const to = state ? state.index : 0, backward = to < index, jump = Math.abs(to - index);
    index = to;
    if (name === current) { below = state ? state.below : null; restore(byName(name), state); showBack(); return; }
    show(name, state, { back: backward, instant: jump > 1 || e.hasUAVisualTransition });
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
    const name = nameOf(a.href);
    if (!name) return;
    e.preventDefault();
    navigate(name, { pressed: a.matches('.rk-tile') ? a : null });
  };
  const offKeys = keys.on(10, (e) => {
    if (e.key !== 'Escape' || e.altKey || e.ctrlKey || e.metaKey || e.shiftKey || (current === home && index === 0) || !keys.escapeIsFree(e)) return false;
    return back();
  });
  // Start on the place the address names. A cold entry below the top puts its parent underneath.
  const startName = nameOf(location.href) || home;
  const st = history.state && history.state.rk;
  if (st && st.place === startName) { index = st.index || 0; below = st.below || null; }
  else if (startName !== home) {
    const parent = parentOf(startName);
    history.replaceState({ ...(history.state || {}), rk: { place: parent, index: 0, below: null } }, '', urlOf(parent));
    history.pushState({ rk: { place: startName, index: 1, below: parent } }, '', urlOf(startName));
    index = 1; below = parent;
  } else history.replaceState({ ...(history.state || {}), rk: { place: home, index: 0, below: null } }, '');
  for (const el of all()) el.hidden = el.dataset.rkPlace !== startName;
  current = startName;
  document.title = titleOf(current);
  showBack();
  if (st && st.place === startName) requestAnimationFrame(() => restore(byName(current), st));
  addEventListener('popstate', pop);
  document.addEventListener('click', click);
  return {
    get current() { return current; },
    get below() { return below; },
    navigate, back, urlOf,
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

// Start --------------------------------------------------------------------------------------------------------------
// The defaults for a page: restore the user's prefs, press on tiles, sheets, and the first paint's build-in.
export function start({ root = document } = {}) {
  prefs.restore();
  const parts = [press(root)];
  root.querySelectorAll('dialog.rk-sheet').forEach((d) => parts.push(sheet(d)));
  arrive(docEl);
  return { destroy() { parts.forEach((p) => p.destroy()); } };
}

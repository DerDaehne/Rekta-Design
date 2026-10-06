// Showcase site behaviour, no dependencies: theme and motion switches, sideways wheel,
// pivot that follows the panorama, live tiles and the motion demos.
// Loaded without defer so the saved theme applies before the first paint.

const root = document.documentElement;
const store = {
  get(key) { try { return localStorage.getItem(key); } catch { return null; } },
  set(key, value) { try { value == null ? localStorage.removeItem(key) : localStorage.setItem(key, value); } catch { /* private mode: per-visit only */ } }
};
if (store.get('rekta-theme')) root.dataset.theme = store.get('rekta-theme');
if (store.get('rekta-motion')) root.dataset.motion = store.get('rekta-motion');

const wide = matchMedia('(min-width: {{breakpoint.sideways-scroll-min}})');
const systemReduced = matchMedia('(prefers-reduced-motion: reduce)');
const systemDark = matchMedia('(prefers-color-scheme: dark)');
const $ = (selector) => document.querySelector(selector);
const token = (name) => getComputedStyle(root).getPropertyValue(`--rekta-${name}`).trim();
// Duration tokens are 0s under reduced motion, so every animation below asks this first.
const ms = (name) => {
  const value = token(`motion-duration-${name}`);
  return parseFloat(value) * (value.endsWith('ms') ? 1 : 1000);
};
// Same unit handling as ms(), for the pivot's own (non-token) custom properties.
const rawMs = (name) => {
  const value = getComputedStyle(root).getPropertyValue(name).trim();
  return parseFloat(value) * (value.endsWith('ms') ? 1 : 1000);
};

// cubic-bezier(x1,y1,x2,y2) as a function of time (0..1 -> eased 0..1), solved by
// Newton-Raphson like a browser's own transition timing — lets JS reuse the exact
// curve a CSS `transition-timing-function` would use, for the scroll properties
// CSS transitions cannot animate (scrollLeft is not a CSS property).
function cubicBezier(x1, y1, x2, y2) {
  const a = (u, v) => 1 - 3 * v + 3 * u;
  const b = (u, v) => 3 * v - 6 * u;
  const c = (u) => 3 * u;
  const sampleX = (t) => ((a(x1, x2) * t + b(x1, x2)) * t + c(x1)) * t;
  const sampleY = (t) => ((a(y1, y2) * t + b(y1, y2)) * t + c(y1)) * t;
  const sampleXDeriv = (t) => 3 * a(x1, x2) * t * t + 2 * b(x1, x2) * t + c(x1);
  return (x) => {
    let t = x;
    for (let i = 0; i < 8; i += 1) {
      const dx = sampleX(t) - x;
      if (Math.abs(dx) < 1e-4) break;
      const d = sampleXDeriv(t);
      if (Math.abs(d) < 1e-6) break;
      t -= dx / d;
    }
    return sampleY(Math.min(1, Math.max(0, t)));
  };
}
const pivotEase = cubicBezier(0.65, 0, 0.35, 1);

// Animates an element's horizontal scroll with pivotEase; instant at duration 0 (reduced
// motion). Every step forces behavior: 'instant' — both .pivot and .panorama carry CSS
// scroll-behavior: smooth, which a plain `el.scrollLeft = x` would otherwise also obey,
// fighting this rAF loop's own easing one frame at a time.
function animateScroll(el, to, duration) {
  cancelAnimationFrame(el.pivotRaf);
  const from = el.scrollLeft;
  if (!duration || from === to) { el.scrollTo({ left: to, behavior: 'instant' }); return; }
  const start = performance.now();
  const step = (now) => {
    const t = Math.min(1, (now - start) / duration);
    el.scrollTo({ left: from + (to - from) * pivotEase(t), behavior: 'instant' });
    el.pivotRaf = t < 1 ? requestAnimationFrame(step) : undefined;
  };
  el.pivotRaf = requestAnimationFrame(step);
}

// Inactive pivot titles scale down from the fixed (active-size) line box by this ratio,
// so the row's height always matches the active size and never depends on which title
// is active. Computed from the actual tokens (set before first paint, see site.js header).
function setPivotScale() {
  root.style.setProperty('--pivot-inactive-scale', parseFloat(token('typography-size-pivot-inactive')) / parseFloat(token('typography-size-pivot-active')));
}
setPivotScale();

// WCAG 2 contrast, same formula as scripts/build-css.mjs --check.
function luminance(hex) {
  const n = parseInt(hex.slice(1), 16);
  const [r, g, b] = [n >> 16, (n >> 8) & 255, n & 255].map((c) => {
    const s = c / 255;
    return s <= 0.03928 ? s / 12.92 : ((s + 0.055) / 1.055) ** 2.4;
  });
  return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}
function contrast(hexA, hexB) {
  const [light, dark] = [luminance(hexA), luminance(hexB)].sort((a, b) => b - a);
  return (light + 0.05) / (dark + 0.05);
}
function showContrast() {
  for (const el of document.querySelectorAll('[data-contrast]')) {
    const [fg, bg] = el.dataset.contrast.split(' ').map((name) => token(`color-${name}`));
    const ratio = contrast(fg, bg);
    const verdict = el.dataset.min ? (ratio >= Number(el.dataset.min) ? ' pass' : ' FAIL') : '';
    el.textContent = `${ratio.toFixed(1)}:1${verdict}`;
  }
}

function showSwitches() {
  const motionOff = root.dataset.motion === 'off';
  $('#theme-word').textContent = root.dataset.theme ?? 'system';
  $('#motion-word').textContent = motionOff ? 'off' : systemReduced.matches ? 'reduced' : 'on';
  $('[data-action="motion"]').setAttribute('aria-pressed', motionOff);
  $('#motion-check').checked = motionOff || systemReduced.matches;
  showContrast();
}
function setTheme(theme) {
  if (theme) root.dataset.theme = theme; else delete root.dataset.theme;
  store.set('rekta-theme', theme);
  showSwitches();
}
function setMotionOff(off) {
  if (off) root.dataset.motion = 'off'; else delete root.dataset.motion;
  store.set('rekta-motion', off ? 'off' : null);
  showSwitches();
}

// Sideways wheel, in the order of docs/layout.md.
function sidewaysWheel(event) {
  const strip = event.currentTarget;
  if (event.defaultPrevented || event.ctrlKey || !wide.matches) return; // rule 4 (and pinch zoom)
  if (event.shiftKey || Math.abs(event.deltaX) >= Math.abs(event.deltaY)) return; // rule 2
  const down = event.deltaY > 0;
  for (let el = event.target; el && el !== strip; el = el.parentElement) { // rule 1
    const scrollable = el.scrollHeight > el.clientHeight + 1 && /auto|scroll/.test(getComputedStyle(el).overflowY);
    if (scrollable && (down ? el.scrollTop + el.clientHeight < el.scrollHeight - 1 : el.scrollTop > 0)) return;
  }
  const max = strip.scrollWidth - strip.clientWidth;
  if (down ? strip.scrollLeft >= max - 1 : strip.scrollLeft <= 0) return; // rule 3
  event.preventDefault();
  const unit = event.deltaMode === 1 ? 16 : event.deltaMode === 2 ? strip.clientWidth : 1;
  strip.scrollBy({ left: event.deltaY * unit, behavior: 'instant' });
}

// Pivot: fixed order; glides the strip so the active title sits at the left edge.
// Runs alongside the active title's own (separate, lighter) size transition, not after it.
function alignPivot(strip, active) {
  const to = active.offsetLeft - strip.firstElementChild.offsetLeft;
  animateScroll(strip, to, rawMs('--pivot-glide-duration'));
}

function followPanorama() {
  const panorama = $('#panorama');
  const pivot = $('.top .pivot');
  const sections = [...panorama.querySelectorAll('.sec')];
  let current = sections[0];
  let queued = false;
  const update = () => {
    queued = false;
    const sideways = wide.matches;
    const edge = sideways ? panorama.getBoundingClientRect().left : $('.top').getBoundingClientRect().bottom;
    const reach = (sideways ? panorama.clientWidth : innerHeight) / 3;
    const active = sections.findLast((s) => (sideways ? s.getBoundingClientRect().left : s.getBoundingClientRect().top) - edge <= reach) ?? sections[0];
    if (active === current) return;
    current = active;
    for (const link of pivot.querySelectorAll('a')) {
      if (link.hash === `#${active.id}`) link.setAttribute('aria-current', 'true'); else link.removeAttribute('aria-current');
    }
    alignPivot(pivot, pivot.querySelector(`a[href="#${active.id}"]`));
  };
  const queue = () => { if (!queued) { queued = true; requestAnimationFrame(update); } };
  panorama.addEventListener('scroll', queue, { passive: true });
  addEventListener('scroll', queue, { passive: true });
  addEventListener('resize', () => { root.style.setProperty('--header-h', `${$('.top').offsetHeight}px`); setPivotScale(); queue(); });
  root.style.setProperty('--header-h', `${$('.top').offsetHeight}px`);
  update();
}

// Live tiles: invented data, updated every two seconds unless paused.
const series = {
  queue: { value: 12, min: 3, max: 22, step: 2, digits: 0, unit: '', noun: 'jobs in the build queue' },
  coverage: { value: 87.4, min: 85, max: 90, step: 0.3, digits: 1, unit: '%', noun: 'test coverage' }
};
let paused = false;

function seed() {
  for (const s of Object.values(series)) {
    s.history = [];
    for (let i = 0; i < 24; i += 1) step(s);
  }
}
function step(s) {
  const next = s.value + (Math.random() - 0.5) * 2 * s.step;
  s.value = Number(Math.min(s.max, Math.max(s.min, next)).toFixed(s.digits));
  s.history.push(s.value);
  if (s.history.length > 24) s.history.shift();
}
function drawSeries() {
  for (const [key, s] of Object.entries(series)) {
    const tile = $(`[data-live="${key}"]`);
    const text = `${s.value.toFixed(s.digits)}${s.unit}`;
    tile.querySelector('[data-v]').textContent = text;
    const low = Math.min(...s.history);
    const span = Math.max(...s.history) - low || 1;
    const points = s.history.map((v, i) => `${(i * 200 / (s.history.length - 1)).toFixed(1)},${(26 - ((v - low) / span) * 24).toFixed(1)}`);
    const svg = tile.querySelector('.spark');
    svg.querySelector('polyline').setAttribute('points', points.join(' '));
    const first = s.history[0];
    const trend = s.value > first ? 'rising' : s.value < first ? 'falling' : 'flat';
    svg.setAttribute('aria-label', `${text} ${s.noun}, ${trend} over the last ${s.history.length} readings`);
  }
}

const STATUS_ORDER = ['info', 'warning', 'error', 'success', ''];
const STATUS_WORD = { info: 'active', warning: 'waiting', error: 'failed', success: 'done', '': 'idle' };

function setStatus(el, status) {
  el.dataset.status = status;
  el.querySelector('.tile-word').textContent = STATUS_WORD[status];
}

// FLIP: measure, change the DOM, then play each element from where it was.
function flip(list, change) {
  const items = [...list.children];
  const before = new Map(items.map((el) => [el, el.getBoundingClientRect()]));
  change();
  const duration = ms('moderate');
  if (!duration) return;
  for (const el of items) {
    const a = before.get(el);
    const b = el.getBoundingClientRect();
    if (a.left !== b.left || a.top !== b.top) {
      el.animate([{ transform: `translate(${a.left - b.left}px, ${a.top - b.top}px)` }, { transform: 'none' }], { duration, easing: token('motion-easing-base') });
    }
  }
}

function restartAnimation(el) {
  el.style.animation = 'none';
  void el.offsetWidth;
  el.style.animation = '';
}

const PLACES = ['inbox', 'board', 'archive', 'settings'];
let toastTimer;
let backSteps = 0;

const actions = {
  back() {
    if (backSteps > 0) { backSteps -= 1; history.back(); } else { $('#overview').scrollIntoView(); }
  },
  pause(button) {
    paused = !paused;
    button.setAttribute('aria-pressed', paused);
    button.querySelector('span').textContent = paused ? 'resume' : 'pause';
    $('#live-word').textContent = paused ? 'demo paused' : 'live demo data';
    $('.appbar').classList.toggle('paused', paused);
  },
  theme() {
    const themes = [null, 'light', 'dark'];
    setTheme(themes[(themes.indexOf(root.dataset.theme ?? null) + 1) % themes.length]);
  },
  motion() { setMotionOff(root.dataset.motion !== 'off'); },
  turn() {
    const place = $('#turn');
    const name = place.querySelector('b');
    name.textContent = PLACES[(PLACES.indexOf(name.textContent) + 1) % PLACES.length];
    place.classList.remove('in');
    void place.offsetWidth;
    place.classList.add('in');
  },
  sheet() { $('#sheet').showModal(); },
  toast() {
    const toast = $('#toast');
    let left = 5;
    $('#toast-count').textContent = left;
    toast.hidden = false;
    restartAnimation(toast);
    clearInterval(toastTimer);
    toastTimer = setInterval(() => {
      left -= 1;
      $('#toast-count').textContent = left;
      if (left === 0) { clearInterval(toastTimer); toast.hidden = true; }
    }, 1000);
  },
  undo() { clearInterval(toastTimer); $('#toast').hidden = true; },
  replay() { document.querySelectorAll('.tile').forEach(restartAnimation); }
};

function setupFacets(box) {
  const tabs = [...box.querySelectorAll('[role="tab"]')];
  const select = (index) => {
    const previous = tabs.findIndex((t) => t.getAttribute('aria-selected') === 'true');
    if (index === previous) return;
    tabs.forEach((tab, i) => {
      tab.setAttribute('aria-selected', i === index);
      tab.tabIndex = i === index ? 0 : -1;
      document.getElementById(tab.getAttribute('aria-controls')).hidden = i !== index;
    });
    alignPivot(tabs[0].parentElement, tabs[index]);
    const duration = ms('moderate') * 0.75;
    if (!duration) return;
    const distance = parseFloat(token('motion-slide-distance')) * (index > previous ? 1 : -1);
    document.getElementById(tabs[index].getAttribute('aria-controls')).animate(
      [{ transform: `translateX(${distance}px)`, opacity: 0 }, { transform: 'none', opacity: 1 }],
      { duration, easing: token('motion-easing-out') }
    );
  };
  tabs.forEach((tab, i) => {
    tab.addEventListener('click', () => select(i));
    tab.addEventListener('keydown', (event) => {
      const move = { ArrowRight: 1, ArrowLeft: -1 }[event.key];
      if (!move) return;
      const next = (i + move + tabs.length) % tabs.length;
      select(next);
      tabs[next].focus();
    });
  });
}

document.addEventListener('DOMContentLoaded', () => {
  for (const strip of document.querySelectorAll('[data-sideways]')) strip.addEventListener('wheel', sidewaysWheel, { passive: false });
  document.querySelectorAll('[data-facets]').forEach(setupFacets);
  followPanorama();
  showSwitches();
  systemDark.addEventListener('change', showContrast);
  systemReduced.addEventListener('change', showSwitches);
  $('#motion-check').addEventListener('change', (event) => setMotionOff(event.target.checked));
  $('#sheet').addEventListener('close', (event) => {
    if (event.target.returnValue === 'reset') { seed(); drawSeries(); }
  });

  document.addEventListener('click', (event) => {
    const action = event.target.closest('[data-action]');
    if (action) actions[action.dataset.action](action);
    const pivotLink = event.target.closest('.top .pivot a');
    if (pivotLink) {
      backSteps += 1;
      // Flush-left, not the browser's default "scroll just enough into view": aligns the
      // same way alignPivot aligns the strip itself, and with the same glide easing.
      const section = wide.matches ? document.querySelector(pivotLink.hash) : null;
      if (section) {
        event.preventDefault();
        history.pushState(null, '', pivotLink.hash);
        const panorama = $('#panorama');
        const gutter = panorama.getBoundingClientRect().left + parseFloat(getComputedStyle(panorama).paddingLeft);
        const to = panorama.scrollLeft + section.getBoundingClientRect().left - gutter;
        animateScroll(panorama, to, rawMs('--pivot-glide-duration'));
      }
    }
    const tile = event.target.closest('[data-cycle]');
    if (tile) setStatus(tile, STATUS_ORDER[(STATUS_ORDER.indexOf(tile.dataset.status ?? '') + 1) % STATUS_ORDER.length]);
    const item = event.target.closest('#flip button');
    if (item) {
      const li = item.parentElement;
      flip(li.parentElement, () => {
        const done = item.dataset.status === 'success';
        setStatus(item, done ? 'info' : 'success');
        if (done) li.parentElement.prepend(li); else li.parentElement.append(li);
      });
      item.focus();
    }
  });

  seed();
  drawSeries();
  setInterval(() => {
    if (paused) return;
    Object.values(series).forEach(step);
    drawSeries();
  }, 2000);
});

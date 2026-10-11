/* Runs in <head>, before first paint: the remembered theme, density, motion and accent, so nothing flashes. */
(function () {
  'use strict';
  Rekta.prefs.restore();
  // An accent the visitor made in the forms section: its own small stylesheet, one value per theme.
  window.siteAccent = function (a) {
    var root = document.documentElement, el = document.getElementById('site-accent');
    if (!a) { if (el) el.remove(); if (root.dataset.accent === 'custom') delete root.dataset.accent; return; }
    if (!el) { el = document.createElement('style'); el.id = 'site-accent'; document.head.appendChild(el); }
    var dark = a.dark ? '--rk-accent: ' + a.dark + '; --rk-on-accent: ' + a.onDark + ';' : '--rk-accent: var(--rk-text-primary); --rk-on-accent: var(--rk-bg);';
    el.textContent = ':root[data-accent="custom"] { --rk-accent: ' + a.light + '; --rk-on-accent: ' + a.onLight + '; }\n'
      + '@media (prefers-color-scheme: dark) { :root[data-accent="custom"]:not([data-theme="light"]) { ' + dark + ' } }\n'
      + ':root[data-accent="custom"][data-theme="dark"] { ' + dark + ' }';
    root.dataset.accent = 'custom';
  };
  try {
    var active = JSON.parse(localStorage.getItem('rekta-site-accent') || 'null');
    var mine = JSON.parse(localStorage.getItem('rekta-site-accent-def') || 'null');
    if (active === 'plum') document.documentElement.dataset.accent = 'plum';
    else if (active === 'custom' && mine && mine.light) window.siteAccent(mine);
  } catch (e) { /* this visit only */ }
}());

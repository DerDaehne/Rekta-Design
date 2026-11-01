/* Runs in <head>, before first paint: the remembered theme, density, motion and accent, so nothing flashes. */
(function () {
  'use strict';
  Rekta.prefs.restore();
  // An accent the visitor made in the forms section: its own small stylesheet, a pair per theme and contrast set,
  // under the name "custom" (prefs: accent), as a product styles a name of its own.
  window.siteAccent = function (a) {
    var el = document.getElementById('site-accent'), r = ':root[data-accent="custom"]';
    if (!a) { if (el) el.remove(); if (Rekta.prefs.get('accent') === 'custom') Rekta.prefs.set('accent', 'product', { remember: false }); return; }
    if (!el) { el = document.createElement('style'); el.id = 'site-accent'; document.head.appendChild(el); }
    var p = function (t) { return '--rk-accent: ' + a.pairs[t].accent + '; --rk-on-accent: ' + a.pairs[t]['on-accent'] + ';'; };
    el.textContent = r + ' { ' + p('light') + ' }\n'
      + '@media (prefers-color-scheme: dark) { ' + r + ':not([data-theme="light"]) { ' + p('dark') + ' } }\n'
      + r + '[data-theme="dark"] { ' + p('dark') + ' }\n'
      + '@media (prefers-contrast: more) { ' + r + ':not([data-contrast="normal"]) { ' + p('light-contrast') + ' } }\n'
      + r + '[data-contrast="more"] { ' + p('light-contrast') + ' }\n'
      + '@media (prefers-contrast: more) and (prefers-color-scheme: dark) { ' + r + ':not([data-contrast="normal"]):not([data-theme="light"]) { ' + p('dark-contrast') + ' } }\n'
      + '@media (prefers-contrast: more) { ' + r + ':not([data-contrast="normal"])[data-theme="dark"] { ' + p('dark-contrast') + ' } }\n'
      + '@media (prefers-color-scheme: dark) { ' + r + '[data-contrast="more"]:not([data-theme="light"]) { ' + p('dark-contrast') + ' } }\n'
      + r + '[data-contrast="more"][data-theme="dark"] { ' + p('dark-contrast') + ' }';
  };
  try {
    var mine = JSON.parse(localStorage.getItem('rekta-site-accent-def') || 'null');
    if (mine && mine.pairs) window.siteAccent(mine);
    else if (Rekta.prefs.get('accent') === 'custom') Rekta.prefs.set('accent', 'product', { remember: false });
  } catch (e) { /* this visit only */ }
}());

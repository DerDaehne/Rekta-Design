#!/usr/bin/env node
// Plain Node, no dependencies. Checks the stylesheet for a disabled app-bar control (.rk-ab with aria-disabled or the
// disabled attribute): it takes the disabled colour, and no other rule that reaches it sets a colour or a hover, press
// or focus tint. Rules are read as text and compared by specificity and source order, so a later or more specific
// rule (the places in a compact bar, the current place) counts. The browser suite (test/smoke.mjs) clicks through the
// bar; this reads the rules, so it runs without a browser.
//
//   node test/appbar-disabled.mjs     exit 1 on any failed check

import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = join(dirname(fileURLToPath(import.meta.url)), '..');
const css = readFileSync(join(root, 'src', 'css', 'rekta.css'), 'utf8').replace(/\/\*[\s\S]*?\*\//g, '');

// Splits a selector list on commas outside parentheses and brackets.
function splitTop(list) {
  const parts = [];
  let depth = 0, cur = '';
  for (const ch of list) {
    if (ch === '(' || ch === '[') depth += 1;
    if (ch === ')' || ch === ']') depth -= 1;
    if (ch === ',' && depth === 0) { parts.push(cur.trim()); cur = ''; } else cur += ch;
  }
  if (cur.trim()) parts.push(cur.trim());
  return parts;
}

const cmp = (a, b) => a[0] - b[0] || a[1] - b[1] || a[2] - b[2];

// [ids, classes/attributes/pseudo-classes, types] of one selector; :not(), :is() and :has() count their most specific argument.
function specificity(sel) {
  let s = sel;
  const extra = [0, 0, 0];
  let m;
  while ((m = /:(not|is|has)\(/.exec(s))) {
    let depth = 0, end = m.index + m[0].length - 1;
    for (; end < s.length; end += 1) {
      if (s[end] === '(') depth += 1;
      if (s[end] === ')' && --depth === 0) break;
    }
    const best = splitTop(s.slice(m.index + m[0].length, end)).map(specificity).reduce((a, b) => (cmp(b, a) > 0 ? b : a), [0, 0, 0]);
    best.forEach((v, i) => { extra[i] += v; });
    s = s.slice(0, m.index) + s.slice(end + 1);
  }
  return [
    extra[0] + (s.match(/#[\w-]+/g) || []).length,
    extra[1] + (s.match(/\.[\w-]+|\[[^\]]*\]|:[\w-]+/g) || []).length,
    extra[2] + (s.match(/(^|[\s>+~])[a-z][\w-]*/gi) || []).length,
  ];
}

// Every rule as its selectors and body, in source order; an @media block's inner rules are found on their own.
const rules = [...css.matchAll(/([^{}]+)\{([^{}]*)\}/g)].map(([, sel, body], index) => ({ index, body, selectors: splitTop(sel) }));

const AB = (s) => /\.rk-ab(?![\w-])/.test(s);                          // the app-bar control, not .rk-ab-places and the like
const excludesDisabled = (s) => /:not\([^)]*(disabled|aria-disabled)/.test(s) || /:enabled\b/.test(s);
const requiresDisabled = (s) => /:disabled\b|\[aria-disabled='true'\]/.test(s) && !/:not\(/.test(s);
const DISABLED_COLOUR = /(?<![\w-])color\s*:\s*var\(--rk-disabled\)/;
const COLOUR = /(?<![\w-])color\s*:\s*([^;]+)/;
const TINT = /(?<![\w-])background(-color|-image)?\s*:\s*(?!none\b)[^;]+/;

const failures = [];
let passed = 0;
function check(cond, pass, fail) { if (cond) { passed += 1; console.log(`ok   ${pass}`); } else { failures.push(fail); console.log(`FAIL ${fail}`); } }
// Source order breaks a tie in specificity: a later rule of equal specificity wins.
const beats = (a, b) => cmp(a.sp, b.sp) > 0 || (cmp(a.sp, b.sp) === 0 && a.index > b.index);

// The disabled colour: the rule that wins for a disabled control, among those that set it.
let disabled = null;
for (const r of rules) for (const s of r.selectors) {
  if (!AB(s) || !requiresDisabled(s) || !DISABLED_COLOUR.test(r.body)) continue;
  const cand = { sp: specificity(s), index: r.index, sel: s };
  if (!disabled || beats(cand, disabled)) disabled = cand;
}
check(disabled,
  'a disabled app-bar control takes the disabled colour (glyph and word)',
  'a disabled app-bar control has no disabled colour: no rule sets color: var(--rk-disabled) on .rk-ab[aria-disabled=\'true\']');

// No other colour reaches a disabled control and beats it, in the cascade (specificity, then source order).
const colourRivals = [];
for (const r of rules) for (const s of r.selectors) {
  if (!disabled || !AB(s) || excludesDisabled(s) || requiresDisabled(s)) continue;
  const c = r.body.match(COLOUR);
  if (c && c[1].trim() !== 'var(--rk-disabled)' && beats({ sp: specificity(s), index: r.index }, disabled)) colourRivals.push(`${s} { color: ${c[1].trim()} }`);
}
check(!!disabled && colourRivals.length === 0,
  'no app-bar rule overrides the disabled colour of a disabled control (cascade order and specificity)',
  disabled ? `a rule overrides the disabled colour of a disabled app-bar control: ${colourRivals[0]}` : 'no disabled colour to check the cascade against');

// No tint on hover or press reaches a disabled control: each tint rule for an app-bar control is excluded from disabled.
for (const state of ['hover', 'active']) {
  const tints = rules.flatMap((r) => r.selectors.filter((s) => AB(s) && s.includes(`:${state}`) && TINT.test(r.body)).map((s) => ({ s, body: r.body })));
  const kept = tints.filter(({ s }) => !excludesDisabled(s));
  check(tints.length && !kept.length,
    `the ${state} tint of an app-bar control leaves a disabled one alone`,
    kept.length ? `an app-bar control keeps its ${state} tint while disabled: ${kept[0].s}` : `no ${state} tint rule found for .rk-ab (the check cannot see the tint)`);
}

console.log(`\n${passed} passed, ${failures.length} failed`);
process.exit(failures.length ? 1 : 0);

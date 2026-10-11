#!/usr/bin/env node
// Plain Node, no dependencies. The one build for Rekta Design.
//
//   node scripts/build.mjs           writes dist/:
//     rekta.tokens.css    every token as a --rk- custom property: light on :root, dark for a dark system setting
//                         (unless the page chose light) and for [data-theme="dark"], compact density for a fine
//                         pointer (unless [data-density="comfortable"]) and for [data-density="compact"], every
//                         motion duration 0s under reduced motion (unless [data-motion="on"]) and [data-motion="off"]
//     rekta.tokens.json   resolved, platform-neutral values for other frameworks (see docs/platforms.md)
//     rekta.css           the components (src/css/rekta.css)
//     rekta.fonts.css     @font-face rules for the bundled Barlow files, copied to dist/fonts/
//     rekta.js            the behaviour as an ES module (src/js/rekta.js)
//     rekta.global.js     the same as a classic script that sets window.Rekta
//   node scripts/build.mjs --check   checks the tokens and the stylesheet instead of writing (exit 1 on failure)

import { copyFileSync, mkdirSync, readFileSync, readdirSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { contrast, distance, parse, toHex8 } from './lib/color.mjs';

const root = join(dirname(fileURLToPath(import.meta.url)), '..');
const dist = join(root, 'dist');
const PREFIX = 'rk';

export function loadTokens() {
  return JSON.parse(readFileSync(join(root, 'tokens', 'rekta.tokens.json'), 'utf8'));
}

// Walks the tree and calls fn(path, token) for every node carrying $value.
export function walk(node, path, fn) {
  for (const [key, value] of Object.entries(node)) {
    if (key.startsWith('$') || !value || typeof value !== 'object') continue;
    if ('$value' in value) fn([...path, key], value);
    else walk(value, [...path, key], fn);
  }
}

// The token's name (the custom property without --rk-). Documented in docs/platforms.md.
export function tokenName(path) {
  const [group, ...rest] = path;
  switch (group) {
    case 'color': return rest.slice(1).join('-');                    // color.light.text.primary -> text-primary
    case 'typography': {
      const [kind, key] = rest;
      return { 'font-family': `font-${key}`, weight: `font-weight-${key}`, size: `font-size-${key}`, 'line-height': `line-height-${key}` }[kind];
    }
    case 'spacing': return `space-${rest.join('-')}`;
    case 'radius': return `radius-${rest.join('-')}`;
    case 'size': return rest.join('-');
    case 'density': return rest.slice(1).join('-');                  // density.compact.control-height -> control-height
    case 'motion': {
      const [kind, key] = rest;
      return { duration: `duration-${key}`, easing: `ease-${key}`, transform: key }[kind];
    }
    case 'timing': return `duration-${rest.join('-')}`;
    case 'layer': return `layer-${rest.join('-')}`;
    case 'breakpoint': return `bp-${rest.join('-')}`;
    default: throw new Error(`no naming rule for ${path.join('.')}`);
  }
}

// Sizes that should follow the user's text size go out as rem on the web.
const REM = (path) => (path[0] === 'typography' && path[1] === 'size')
  || (path[0] === 'size' && ['tile-unit', 'tile-unit-ultrawide', 'touch-target', 'appbar-height', 'pane-min'].includes(path[1]))
  || (path[0] === 'density' && ['control-height', 'row-height'].includes(path[2]));

const rem = (px) => `${+(px / 16).toFixed(4)}rem`;

function resolveAlias(tokens, ref) {
  const node = ref.slice(1, -1).split('.').reduce((n, k) => n?.[k], tokens);
  if (!node || !('$value' in node)) throw new Error(`alias to a missing token: ${ref}`);
  return node;
}

export function cssValue(tokens, path, token) {
  const v = token.$value;
  if (typeof v === 'string' && v.startsWith('{')) {
    const target = v.slice(1, -1).split('.');
    return `var(--${PREFIX}-${tokenName(target)})`;
  }
  switch (token.$type) {
    case 'color': case 'duration': return String(v);
    case 'number': {
      const unit = token.$extensions?.rekta?.unit;
      return unit ? `${v}${unit}` : String(v);
    }
    case 'fontWeight': return String(v);
    case 'dimension': return REM(path) ? rem(v.value) : `${v.value}${v.unit}`;
    case 'fluidDimension': return `clamp(${rem(v.min.value)}, ${+(v.viewport * 100).toFixed(2)}cqi, ${rem(v.max.value)})`;
    case 'fontFamily': return (Array.isArray(v) ? v : [v]).map((n) => (/\s/.test(n) ? `"${n}"` : n)).join(', ');
    case 'cubicBezier': return `cubic-bezier(${v.join(', ')})`;
    default: throw new Error(`unknown $type ${token.$type} at ${path.join('.')}`);
  }
}

function collect(tokens, node, base) {
  const out = [];
  walk(node, base, (path, token) => out.push({ path, token, name: tokenName(path) }));
  return out;
}

const decl = (tokens, list) => list.map(({ path, token, name }) => `  --${PREFIX}-${name}: ${cssValue(tokens, path, token)};`).join('\n');

export function buildTokensCss(tokens) {
  const light = collect(tokens, tokens.color.light, ['color', 'light']);
  const dark = collect(tokens, tokens.color.dark, ['color', 'dark']);
  const comfortable = collect(tokens, tokens.density.comfortable, ['density', 'comfortable']);
  const compact = collect(tokens, tokens.density.compact, ['density', 'compact']);
  const rest = ['typography', 'spacing', 'radius', 'size', 'motion', 'timing', 'layer', 'breakpoint']
    .flatMap((g) => collect(tokens, tokens[g], [g]));
  const durations = collect(tokens, tokens.motion.duration, ['motion', 'duration']);
  const zero = durations.map(({ name }) => `  --${PREFIX}-${name}: 0s;`).join('\n');
  return `/* Rekta Design tokens. Generated from tokens/rekta.tokens.json by scripts/build.mjs; do not edit. */

/* Light, comfortable, full motion. */
:root {
${decl(tokens, light)}
${decl(tokens, comfortable)}
${decl(tokens, rest)}
  color-scheme: light;
}

/* Dark: the system setting, unless the page chose light; and an explicit choice. */
@media (prefers-color-scheme: dark) {
  :root:not([data-theme="light"]) {
${decl(tokens, dark).replace(/^/gm, '  ')}
    color-scheme: dark;
  }
}
:root[data-theme="dark"] {
${decl(tokens, dark)}
  color-scheme: dark;
}

/* Compact density: a fine primary pointer and no coarse one, unless the page chose comfortable; and an explicit choice. */
@media (pointer: fine) and (not (any-pointer: coarse)) {
  :root:not([data-density="comfortable"]) {
${decl(tokens, compact).replace(/^/gm, '  ')}
  }
}
:root[data-density="compact"] {
${decl(tokens, compact)}
}
:root[data-density="comfortable"] {
${decl(tokens, comfortable)}
}

/* Reduced motion: every transition becomes instant, never skipped. timing.* (wall time) stays. */
@media (prefers-reduced-motion: reduce) {
  :root:not([data-motion="on"]) {
${zero.replace(/^/gm, '  ')}
  }
}
:root[data-motion="off"] {
${zero}
}
`;
}

// Resolved, platform-neutral values: colours as #rrggbb or #rrggbbaa, lengths in logical px, durations in ms,
// easings as [x1, y1, x2, y2], angles in degrees, fluid sizes as {min, max, viewport}.
export function buildTokensJson(tokens, version) {
  const plain = (path, token) => {
    let t = token;
    while (typeof t.$value === 'string' && t.$value.startsWith('{')) t = resolveAlias(tokens, t.$value);
    const v = t.$value;
    switch (t.$type) {
      case 'color': return toHex8(parse(v));
      case 'dimension': return v.value;
      case 'fluidDimension': return { min: v.min.value, max: v.max.value, viewport: v.viewport };
      case 'duration': return parseFloat(v) * (String(v).endsWith('ms') ? 1 : 1000);
      default: return v;
    }
  };
  const group = (node, base) => {
    const out = {};
    walk(node, base, (path, token) => { out[tokenName(path)] = plain(path, token); });
    return out;
  };
  const describe = (node, base) => {
    const out = {};
    walk(node, base, (path, token) => { if (token.$description) out[tokenName(path)] = token.$description; });
    return out;
  };
  const all = {};
  for (const g of ['color', 'typography', 'spacing', 'radius', 'size', 'density', 'motion', 'timing', 'layer', 'breakpoint']) Object.assign(all, describe(tokens[g], [g]));
  return {
    name: 'Rekta Design',
    version,
    units: { length: 'logical px (CSS px, Android dp, Apple pt)', duration: 'ms', angle: 'deg', easing: 'cubic-bezier [x1, y1, x2, y2]', fluid: '{min, max, viewport}: interpolate between min and max at viewport × hosting box width' },
    themes: { light: group(tokens.color.light, ['color', 'light']), dark: group(tokens.color.dark, ['color', 'dark']) },
    densities: { comfortable: group(tokens.density.comfortable, ['density', 'comfortable']), compact: group(tokens.density.compact, ['density', 'compact']) },
    typography: group(tokens.typography, ['typography']),
    spacing: group(tokens.spacing, ['spacing']),
    radius: group(tokens.radius, ['radius']),
    size: group(tokens.size, ['size']),
    motion: group(tokens.motion, ['motion']),
    timing: group(tokens.timing, ['timing']),
    layer: group(tokens.layer, ['layer']),
    breakpoint: group(tokens.breakpoint, ['breakpoint']),
    descriptions: all,
  };
}

function fontsCss() {
  const files = readdirSync(join(root, 'site', 'fonts', 'barlow')).filter((f) => f.endsWith('.woff2')).sort();
  return '/* Barlow (SIL OFL 1.1), latin. Ship these files with the app; never load them from a CDN. */\n' + files.map((f) => {
    const weight = /-(\d{3})-/.exec(f)[1];
    return `@font-face { font-family: Barlow; font-style: normal; font-weight: ${weight}; font-display: swap; src: url(fonts/${f}) format('woff2'); }`;
  }).join('\n') + '\n';
}

// The ES module becomes a classic script: export statements go, the module's names are gathered on window.Rekta.
export function toGlobal(esm) {
  const names = [...esm.matchAll(/^export\s+(?:async\s+function\*?|function\*?|const|class|let)\s+([A-Za-z_$][\w$]*)/gm)].map((m) => m[1]);
  const body = esm.replace(/^export\s+(async\s+function|function|const|class|let)\s/gm, '$1 ').replace(/^export\s*\{[^}]*\};?\s*$/gm, '');
  if (/^export\b/m.test(body)) throw new Error('rekta.js has an export the classic script cannot carry');
  return `/* Rekta Design behaviour, classic script. Generated from src/js/rekta.js by scripts/build.mjs; do not edit. */\n(function () {\n${body}\nwindow.Rekta = { ${names.join(', ')} };\n})();\n`;
}

function check(tokens) {
  const problems = [];
  const t = buildTokensJson(tokens, 'check');
  const STATUS = ['info', 'success', 'warning', 'error'];
  for (const theme of ['light', 'dark']) {
    const c = t.themes[theme];
    const need = (fg, bg, min, what) => { const r = contrast(c[fg], c[bg]); if (r < min) problems.push(`${theme}: ${what} ${fg} on ${bg} is ${r.toFixed(2)}:1, needs ${min}:1`); };
    for (const bg of ['bg', 'surface']) {
      need('text-primary', bg, 4.5, 'text');
      need('text-contrast-high', bg, 4.5, 'secondary text');
      need('text-contrast-medium', bg, 4.5, 'placeholder');
      need('focus', bg, 3, 'focus ring');
      need('accent', bg, 3, 'accent');
      for (const s of STATUS) need(`status-${s}-full`, bg, 3, 'status mark');
    }
    need('on-accent', 'accent', 4.5, 'text');
    for (const s of STATUS) {
      need('text-primary', `status-${s}-tile`, 4.5, 'tile text');
      need('text-primary', `status-${s}-soft`, 4.5, 'soft label');
      need(`status-${s}-full`, `status-${s}-soft`, 3, 'mark on its soft tint');
    }
    // Statuses must stay apart under red-green colour blindness (OKLab distance of the simulated colours).
    for (const form of ['full', 'tile']) for (const kind of ['protan', 'deutan']) {
      for (let i = 0; i < STATUS.length; i += 1) for (let j = i + 1; j < STATUS.length; j += 1) {
        const d = distance(c[`status-${STATUS[i]}-${form}`], c[`status-${STATUS[j]}-${form}`], kind);
        if (d < 0.09) problems.push(`${theme}: ${STATUS[i]} and ${STATUS[j]} ${form} are ${d.toFixed(3)} apart under ${kind}, need 0.09`);
      }
    }
  }
  // Every --rk- property the stylesheet reads is a token or a documented hook.
  const css = readFileSync(join(root, 'src', 'css', 'rekta.css'), 'utf8');
  const defined = new Set();
  for (const g of ['color']) walk(tokens.color.light, ['color', 'light'], (p) => defined.add(tokenName(p)));
  for (const g of ['typography', 'spacing', 'radius', 'size', 'motion', 'timing', 'layer', 'breakpoint']) walk(tokens[g], [g], (p) => defined.add(tokenName(p)));
  walk(tokens.density.comfortable, ['density', 'comfortable'], (p) => defined.add(tokenName(p)));
  const HOOKS = new Set(['i', 'v', 'sec-w', 'split-w', 'banner-h', 'pivot-scale', 'pivot-h']);
  for (const m of css.matchAll(/var\(--rk-([\w-]+)/g)) {
    if (!defined.has(m[1]) && !HOOKS.has(m[1])) problems.push(`rekta.css reads --rk-${m[1]}, which is neither a token nor a public hook`);
  }
  // The script reads tokens by name too (and builds the pivot's font-size names from a stem).
  const js = readFileSync(join(root, 'src', 'js', 'rekta.js'), 'utf8');
  for (const m of js.matchAll(/'--rk-([\w-]*\w)'/g)) {
    if (!defined.has(m[1]) && !HOOKS.has(m[1])) problems.push(`rekta.js reads --rk-${m[1]}, which is neither a token nor a public hook`);
  }
  for (const n of ['pivot-inactive', 'nav-inactive']) if (!defined.has(`font-size-${n}`)) problems.push(`rekta.js needs --rk-font-size-${n}`);
  // The classic script must parse as one, with every export on window.Rekta.
  try { new Function(toGlobal(js)); } catch (e) { problems.push(`rekta.global.js does not parse: ${e.message}`); }
  return problems;
}

export function buildDist() {
  const tokens = loadTokens();
  const pkg = JSON.parse(readFileSync(join(root, 'package.json'), 'utf8'));
  mkdirSync(join(dist, 'fonts'), { recursive: true });
  writeFileSync(join(dist, 'rekta.tokens.css'), buildTokensCss(tokens));
  writeFileSync(join(dist, 'rekta.tokens.json'), JSON.stringify(buildTokensJson(tokens, pkg.version), null, 2) + '\n');
  writeFileSync(join(dist, 'rekta.css'), `/* Rekta Design ${pkg.version} components. Load after rekta.tokens.css. */\n` + readFileSync(join(root, 'src', 'css', 'rekta.css'), 'utf8'));
  writeFileSync(join(dist, 'rekta.fonts.css'), fontsCss());
  for (const f of readdirSync(join(root, 'site', 'fonts', 'barlow'))) copyFileSync(join(root, 'site', 'fonts', 'barlow', f), join(dist, 'fonts', f));
  const esm = readFileSync(join(root, 'src', 'js', 'rekta.js'), 'utf8').replace(/^export const version = '[^']*';/m, `export const version = '${pkg.version}';`);
  writeFileSync(join(dist, 'rekta.js'), esm);
  writeFileSync(join(dist, 'rekta.global.js'), toGlobal(esm));
  return pkg.version;
}

export function checkAll() { return check(loadTokens()); }

if (import.meta.url === `file://${process.argv[1]}`) {
  if (process.argv.includes('--check')) {
    const problems = checkAll();
    if (problems.length) { console.error(problems.map((p) => `✗ ${p}`).join('\n')); process.exit(1); }
    console.log('✓ tokens and stylesheet pass: contrast, status marks, colour-blind separation, every --rk- read in CSS and JS is defined');
  } else {
    console.log(`✓ dist/ written (Rekta Design ${buildDist()})`);
  }
}

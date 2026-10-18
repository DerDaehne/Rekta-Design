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
//   and core/src/ (committed, so the C library builds without Node):
//     rk_tokens.h         the tokens as C tables in the order of the enums in core/include/rekta.h
//     rk_icons.h          the icons as polylines on the 20-unit grid
//     rk_fonts.h          the Barlow files in core/fonts/ as byte arrays
//   node scripts/build.mjs --check   checks the tokens and the stylesheet instead of writing (exit 1 on failure),
//                                    and that core/src/ is up to date

import { copyFileSync, existsSync, mkdirSync, readFileSync, readdirSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { execFileSync } from 'node:child_process';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { contrast, distance, parse, toHex8 } from './lib/color.mjs';
import { coreFonts, coreIcons, coreTokens } from './lib/core.mjs';

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

// Said by --check without failing it: trade-offs the docs name (accessibility.md), measured on every build.
const notes = [];

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
    // Under blue-yellow colour blindness (tritan) the separation is reported, not required: the word on every status
    // tile carries the meaning there (docs/accessibility.md).
    for (const form of ['full', 'tile']) for (let i = 0; i < STATUS.length; i += 1) for (let j = i + 1; j < STATUS.length; j += 1) {
      const d = distance(c[`status-${STATUS[i]}-${form}`], c[`status-${STATUS[j]}-${form}`], 'tritan');
      if (d < 0.09) notes.push(`${theme}: ${STATUS[i]} and ${STATUS[j]} ${form} are ${d.toFixed(3)} apart under tritan (0.09 is required under protan and deutan)`);
    }
  }
  // Every --rk- property the stylesheet reads is a token or a documented hook.
  const css = readFileSync(join(root, 'src', 'css', 'rekta.css'), 'utf8');
  const defined = new Set();
  for (const g of ['color']) walk(tokens.color.light, ['color', 'light'], (p) => defined.add(tokenName(p)));
  for (const g of ['typography', 'spacing', 'radius', 'size', 'motion', 'timing', 'layer', 'breakpoint']) walk(tokens[g], [g], (p) => defined.add(tokenName(p)));
  walk(tokens.density.comfortable, ['density', 'comfortable'], (p) => defined.add(tokenName(p)));
  const HOOKS = new Set(['i', 'v', 'titlebar-controls', 'sec-w', 'split-w', 'banner-h', 'pivot-scale', 'pivot-h', 'toast-left', ...[1, 2, 3, 4, 5, 6, 7, 8].map((n) => `series-${n}`)]);
  for (const m of css.matchAll(/var\(--rk-([\w-]+)/g)) {
    if (!defined.has(m[1]) && !HOOKS.has(m[1])) problems.push(`rekta.css reads --rk-${m[1]}, which is neither a token nor a public hook`);
  }
  // The script reads tokens by name too (and builds the pivot's font-size names from a stem).
  const js = readFileSync(join(root, 'src', 'js', 'rekta.js'), 'utf8');
  for (const m of js.matchAll(/'--rk-([\w-]*\w)'/g)) {
    if (!defined.has(m[1]) && !HOOKS.has(m[1])) problems.push(`rekta.js reads --rk-${m[1]}, which is neither a token nor a public hook`);
  }
  for (const n of ['pivot-inactive', 'nav-inactive']) if (!defined.has(`font-size-${n}`)) problems.push(`rekta.js needs --rk-font-size-${n}`);
  // Every word the script shows or says comes from its strings catalogue (rk-strings-begin … end), so a product can
  // translate it: an English phrase in a literal anywhere else fails. A heuristic: two words in a row, outside errors
  // thrown, console output and selectors.
  {
    const a = js.indexOf('// rk-strings-begin'), b = js.indexOf('// rk-strings-end');
    if (a < 0 || b < a) problems.push('rekta.js has no strings catalogue (rk-strings-begin … rk-strings-end)');
    const lines = js.split('\n'), first = js.slice(0, a).split('\n').length, last = js.slice(0, b).split('\n').length;
    const TECH = /throw new Error|console\.|querySelector|\.matches\(|closest\(|cssText|insertAdjacentHTML|new RegExp/;
    lines.forEach((line, i) => {
      if (i + 1 >= first && i + 1 <= last) return;
      const l = line.replace(/\/\/.*$/, '').replace(/\/\*.*?\*\//g, '');
      if (TECH.test(l)) return;
      for (const m of l.matchAll(/'([^'\n]*)'|`([^`\n]*)`|"([^"\n]*)"/g)) {
        const text = (m[1] ?? m[2] ?? m[3]).replace(/\$\{[^}]*\}/g, ' ').replace(/\s+/g, ' ');
        if (/[A-Za-z]{2,} [a-z]{2,}/.test(text) && !/[[\]#>:]|^\s*\.|--|rk-|\(/.test(text)) problems.push(`rekta.js:${i + 1} shows words outside its strings catalogue: "${text.trim().slice(0, 60)}"`);
      }
    });
  }
  // Each catalogue shipped in another language has exactly the English keys. An entry that counts (COUNT_ARG in
  // rekta.js) is a set of plural forms with at least one and other, each an Intl.PluralRules category; every other
  // entry is a string or a function. The catalogues are imported, as a product would.
  {
    const counts = new Set(((js.match(/const COUNT_ARG = \{([^}]*)\}/) || [])[1] || '').match(/\w+(?=:)/g) || []);
    if (!counts.size) problems.push('rekta.js names no counting entries (COUNT_ARG)');
    const shape = (file, name) => {
      const code = `const m = await import(${JSON.stringify(pathToFileURL(file).href)}); console.log(JSON.stringify(Object.fromEntries(Object.entries(m[${JSON.stringify(name)}]).map(([k, v]) => [k, v && typeof v === 'object' ? Object.keys(v) : typeof v]))));`;
      try { return JSON.parse(execFileSync(process.execPath, ['--input-type=module', '-e', code], { stdio: 'pipe' }).toString()); }
      catch (e) { problems.push(`${file} cannot be imported: ${String(e.stderr || e.message).split('\n').find((l) => /Error/.test(l)) || e.message}`); return null; }
    };
    const CATEGORIES = new Set(['zero', 'one', 'two', 'few', 'many', 'other']);
    const judge = (label, cat) => {
      for (const [k, v] of Object.entries(cat)) {
        if (counts.has(k)) {
          if (!Array.isArray(v)) problems.push(`${label}: ${k} counts, so it is a set of plural forms ({ one, other, ... }), not a ${v}`);
          else {
            for (const f of ['one', 'other']) if (!v.includes(f)) problems.push(`${label}: ${k} lacks its "${f}" form`);
            for (const f of v) if (!CATEGORIES.has(f)) problems.push(`${label}: ${k} has "${f}", which is not a plural category`);
          }
        } else if (Array.isArray(v)) problems.push(`${label}: ${k} has plural forms but does not count (add it to COUNT_ARG)`);
      }
    };
    const en = shape(join(root, 'src', 'js', 'rekta.js'), 'strings');
    if (en) judge('rekta.js', en);
    for (const f of readdirSync(join(root, 'src', 'js')).filter((x) => /^strings\.[\w-]+\.js$/.test(x))) {
      const lang = f.slice(8, -3).replace('-', '_'), other = shape(join(root, 'src', 'js', f), lang);
      if (!en || !other) continue;
      judge(`src/js/${f}`, other);
      for (const k of Object.keys(en)) if (!(k in other)) problems.push(`src/js/${f} lacks ${k}`);
      for (const k of Object.keys(other)) if (!(k in en)) problems.push(`src/js/${f} has ${k}, which rekta.js does not use`);
    }
  }
  // The TypeScript declarations (src/js/rekta.d.ts, shipped as dist/rekta.d.ts) name exactly what rekta.js exports,
  // and every catalogue has its own declaration.
  {
    const dts = readFileSync(join(root, 'src', 'js', 'rekta.d.ts'), 'utf8');
    const exported = new Set([...js.matchAll(/^export (?:async )?(?:function|const|let) (\w+)/gm)].map((m) => m[1]));
    const declared = new Set([...dts.matchAll(/^export declare (?:function|const|let) (\w+)/gm)].map((m) => m[1]));
    for (const n of exported) if (!declared.has(n)) problems.push(`rekta.js exports ${n}, which src/js/rekta.d.ts does not declare`);
    for (const n of declared) if (!exported.has(n)) problems.push(`src/js/rekta.d.ts declares ${n}, which rekta.js does not export`);
    for (const f of readdirSync(join(root, 'src', 'js')).filter((x) => /^strings\.[\w-]+\.js$/.test(x))) {
      if (!existsSync(join(root, 'src', 'js', f.replace(/\.js$/, '.d.ts')))) problems.push(`src/js/${f} has no declaration (${f.replace(/\.js$/, '.d.ts')})`);
    }
  }
  // Importing the module touches no DOM, so a page that renders on the server can import it (call it in the browser).
  try { execFileSync(process.execPath, ['--input-type=module', '-e', `await import(${JSON.stringify(pathToFileURL(join(root, 'src', 'js', 'rekta.js')).href)})`], { stdio: 'pipe' }); }
  catch (e) { problems.push(`rekta.js cannot be imported without a DOM (server rendering): ${String(e.stderr || e.message).split('\n').find((l) => /Error/.test(l)) || e.message}`); }
  // The classic script must parse as one, with every export on window.Rekta.
  try { new Function(toGlobal(js)); } catch (e) { problems.push(`rekta.global.js does not parse: ${e.message}`); }
  return problems;
}

// A product's own values, checked against the same rules as Rekta's (docs/data.md): the domain colour codes it sets
// (--rk-series-N, per theme) must hold 3:1 against the ground and surface, stay 0.09 apart under red-green colour
// blindness (tritan reported), be set in both themes, and never colour text.
//   node scripts/build.mjs --check-product product.css
export function checkProduct(css, file = 'product CSS') {
  const problems = [], notes = [], t = buildTokensJson(loadTokens(), 'check'), codes = { light: {}, dark: {} };
  // Walk the rules: a declaration is dark when a selector or @media around it says dark.
  const stack = [];
  let buf = '';
  for (const ch of css.replace(/\/\*[\s\S]*?\*\//g, '')) {
    if (ch === '{') { stack.push(buf.trim()); buf = ''; }
    else if (ch === '}') { scan(buf); buf = ''; stack.pop(); }
    else if (ch === ';') { scan(buf); buf = ''; }
    else buf += ch;
  }
  function scan(decl) {
    const m = /^\s*--rk-series-(\d+)\s*:\s*(.+?)\s*$/.exec(decl);
    if (m) {
      const ctx = stack.join(' '), theme = /dark/.test(ctx) && !/data-theme=["']?light["']?\]\s*$/.test(stack[stack.length - 1] || '') ? 'dark' : 'light';
      codes[theme][+m[1]] = m[2];
    }
    if (/^\s*(color|-webkit-text-fill-color)\s*:.*var\(--rk-series-/.test(decl)) problems.push(`${file}: text drawn in a series colour (${decl.trim()}); codes are for small marks only`);
  }
  for (const theme of ['light', 'dark']) {
    const c = t.themes[theme], set = codes[theme], ns = Object.keys(set).map(Number).sort((x, y) => x - y);
    const other = codes[theme === 'light' ? 'dark' : 'light'];
    for (const n of Object.keys(other)) if (!(n in set)) problems.push(`${file}: --rk-series-${n} is set for ${theme === 'light' ? 'dark' : 'light'} but not for ${theme}`);
    for (const n of ns) {
      if (n < 1 || n > 8) problems.push(`${file}: --rk-series-${n} is outside 1 to 8`);
      let col; try { col = parse(set[n]); } catch { problems.push(`${file}: ${theme} --rk-series-${n} is not a colour the check reads (hex or rgb): ${set[n]}`); continue; }
      const hex = toHex8(col);
      for (const bg of ['bg', 'surface']) { const r = contrast(hex, c[bg]); if (r < 3) problems.push(`${file}: ${theme} --rk-series-${n} ${set[n]} on ${bg} is ${r.toFixed(2)}:1, needs 3:1`); }
    }
    for (let i = 0; i < ns.length; i += 1) for (let j = i + 1; j < ns.length; j += 1) {
      let a, b; try { a = toHex8(parse(set[ns[i]])); b = toHex8(parse(set[ns[j]])); } catch { continue; }
      for (const kind of ['protan', 'deutan']) { const d = distance(a, b, kind); if (d < 0.09) problems.push(`${file}: ${theme} --rk-series-${ns[i]} and -${ns[j]} are ${d.toFixed(3)} apart under ${kind}, need 0.09`); }
      const d = distance(a, b, 'tritan'); if (d < 0.09) notes.push(`${file}: ${theme} --rk-series-${ns[i]} and -${ns[j]} are ${d.toFixed(3)} apart under tritan`);
    }
  }
  return { problems, notes, count: Object.keys(codes.light).length + Object.keys(codes.dark).length };
}

// Icons: src/icons/*.svg (20 × 20, stroke 1.6, square ends) become one sprite of <symbol id="rk-i-NAME"> for the web
// and a JSON list of each icon's shapes for other platforms.
export function loadIcons() {
  const dir = join(root, 'src', 'icons'), icons = {};
  for (const f of readdirSync(dir).filter((x) => x.endsWith('.svg')).sort()) {
    const svg = readFileSync(join(dir, f), 'utf8'), name = f.slice(0, -4);
    const viewBox = (/viewBox="([^"]+)"/.exec(svg) || [])[1];
    const shapes = [...svg.matchAll(/<(path|circle|rect|line|polyline)\b([^>]*?)\/>/g)].map(([, tag, attrs]) => ({ tag, attrs: Object.fromEntries([...attrs.matchAll(/([\w-]+)="([^"]*)"/g)].map(([, k, v]) => [k, v])) }));
    icons[name] = { viewBox, shapes, inner: svg.replace(/^[\s\S]*?<svg[^>]*>|<\/svg>\s*$/g, '').trim() };
  }
  return icons;
}
function iconsSprite(icons) {
  return `<svg xmlns="http://www.w3.org/2000/svg" style="display:none">${Object.entries(icons).map(([n, i]) => `<symbol id="rk-i-${n}" viewBox="${i.viewBox}" fill="none" stroke="currentColor" stroke-width="1.6">${i.inner}</symbol>`).join('')}</svg>\n`;
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
  copyFileSync(join(root, 'src', 'js', 'rekta.d.ts'), join(dist, 'rekta.d.ts'));
  // Catalogues in other languages: as a module (export const de), and as a classic script (window.RektaStrings.de).
  for (const f of readdirSync(join(root, 'src', 'js')).filter((x) => /^strings\.[\w-]+\.js$/.test(x))) {
    const text = readFileSync(join(root, 'src', 'js', f), 'utf8'), lang = f.split('.')[1];
    writeFileSync(join(dist, `rekta.${f}`), text);
    if (existsSync(join(root, 'src', 'js', `strings.${lang}.d.ts`))) copyFileSync(join(root, 'src', 'js', `strings.${lang}.d.ts`), join(dist, `rekta.strings.${lang}.d.ts`));
    writeFileSync(join(dist, `rekta.strings.${lang}.global.js`), text.replace(new RegExp(`^export const ${lang.replace('-', '_')} = `, 'm'), `(window.RektaStrings = window.RektaStrings || {}).${lang.replace('-', '_')} = `));
  }
  const icons = loadIcons();
  writeFileSync(join(dist, 'rekta.icons.svg'), iconsSprite(icons));
  writeFileSync(join(dist, 'rekta.icons.json'), JSON.stringify({ name: 'Rekta Design icons', version: pkg.version, grid: 20, strokeWidth: 1.6, lineCap: 'butt', lineJoin: 'miter', color: 'text colour (currentColor); a filled shape says fill', icons: Object.fromEntries(Object.entries(icons).map(([n, i]) => [n, i.shapes])) }, null, 1) + '\n');
  for (const [file, text] of Object.entries(coreFiles(tokens, pkg.version, icons))) writeFileSync(join(root, 'core', 'src', file), text);
  return pkg.version;
}

// The generated C sources of rekta-core, by file name.
function coreFiles(tokens, version, icons) {
  const { out, problems } = coreTokens(buildTokensJson(tokens, version));
  if (problems.length) throw new Error(problems.join('\n'));
  return { 'rk_tokens.h': out, 'rk_icons.h': coreIcons(icons), 'rk_fonts.h': coreFonts(join(root, 'core', 'fonts')) };
}

export function checkAll() {
  const tokens = loadTokens(), problems = check(tokens), icons = loadIcons();
  for (const [n, i] of Object.entries(icons)) {
    if (i.viewBox !== '0 0 20 20') problems.push(`icon ${n} is not on the 20px grid (viewBox ${i.viewBox})`);
    if (!i.shapes.length) problems.push(`icon ${n} has no shapes`);
  }
  // One version everywhere: package.json, rekta.js, rekta-core, the flake and the newest released changelog heading.
  try {
    const want = JSON.parse(readFileSync(join(root, 'package.json'), 'utf8')).version;
    const read = (file) => readFileSync(join(root, file), 'utf8');
    const h = read('core/include/rekta.h'), part = (n) => (new RegExp(`#define RK_VERSION_${n} (\\d+)`).exec(h) || [])[1];
    const found = {
      'src/js/rekta.js': (/^export const version = '([^']+)';/m.exec(read('src/js/rekta.js')) || [])[1],
      'core/include/rekta.h': `${part('MAJOR')}.${part('MINOR')}.${part('PATCH')}`,
      'flake.nix': (/version = "([^"]+)";/.exec(read('flake.nix')) || [])[1],
      'CHANGELOG.md': (/^## (\d+\.\d+\.\d+)/m.exec(read('CHANGELOG.md')) || [])[1],
    };
    for (const [file, v] of Object.entries(found)) if (v !== want) problems.push(`${file} says version ${v}, package.json ${want}`);
  } catch (e) { problems.push(`the version check: ${e.message}`); }
  try {
    const pkg = JSON.parse(readFileSync(join(root, 'package.json'), 'utf8'));
    for (const [file, text] of Object.entries(coreFiles(tokens, pkg.version, icons))) {
      let now = '';
      try { now = readFileSync(join(root, 'core', 'src', file), 'utf8'); } catch { /* missing */ }
      if (now !== text) problems.push(`core/src/${file} is out of date: run node scripts/build.mjs`);
    }
  } catch (e) { problems.push(`the C target: ${e.message}`); }
  // The showcase sets a domain colour code as a product would: it is held to the same rules.
  const site = checkProduct(readFileSync(join(root, 'site', 'site.css'), 'utf8'), 'site/site.css');
  problems.push(...site.problems); notes.push(...site.notes);
  return problems;
}

if (import.meta.url === `file://${process.argv[1]}`) {
  const productAt = process.argv.indexOf('--check-product');
  if (productAt > 0) {
    const file = process.argv[productAt + 1];
    if (!file) { console.error('usage: node scripts/build.mjs --check-product product.css'); process.exit(2); }
    const r = checkProduct(readFileSync(file, 'utf8'), file);
    for (const n of r.notes) console.log(`  i ${n}`);
    if (r.problems.length) { console.error(r.problems.map((p) => `✗ ${p}`).join('\n')); process.exit(1); }
    console.log(r.count ? `✓ ${file}: ${r.count} domain colour values pass: 3:1 on the ground, apart under colour blindness, both themes, no text` : `✓ ${file}: no domain colour values (--rk-series-N) to check`);
  } else if (process.argv.includes('--check')) {
    const problems = checkAll();
    if (problems.length) { console.error(problems.map((p) => `✗ ${p}`).join('\n')); process.exit(1); }
    console.log('✓ tokens and stylesheet pass: contrast, status marks, colour-blind separation, every --rk- read in CSS and JS is defined, every icon on the grid, one version everywhere');
    for (const n of notes) console.log(`  i ${n}`);
  } else {
    console.log(`✓ dist/ written (Rekta Design ${buildDist()})`);
  }
}

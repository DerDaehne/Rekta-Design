#!/usr/bin/env node
// Plain Node, no dependencies. Builds the showcase site from site/ and the dist/ files it runs on. Like the docs, the
// site takes its numbers from the tokens at build time: {{token-name}} becomes the token's value ({{token-name|px}}
// in px at the default text size), and
// <!--@block--> becomes generated markup (status tiles, swatches, contrast pairs, the type scale, the token data).
//
//   node scripts/build-site.mjs                  runs the build, then writes _site/: site/*.html|css|js beside
//                                                dist/rekta.* and dist/fonts/
//   node scripts/build-site.mjs --single FILE    also writes one self-contained page (styles, fonts and scripts
//                                                inline, no <html>/<head>/<body> wrapper) for hosts that take a single
//                                                file, such as an artifact
//   node scripts/build-site.mjs --serve          then serves _site/ on http://127.0.0.1:4199 (Ctrl+C to stop)

import { createHash } from 'node:crypto';
import { copyFileSync, cpSync, createReadStream, mkdirSync, readFileSync, readdirSync, rmSync, statSync, writeFileSync } from 'node:fs';
import { createServer } from 'node:http';
import { dirname, extname, join, normalize, sep } from 'node:path';
import { fileURLToPath } from 'node:url';
import { buildDist, cssValue, loadIcons, loadTokens, tokenName, walk } from './build.mjs';

const root = join(dirname(fileURLToPath(import.meta.url)), '..');
const out = join(root, '_site');
const dist = join(root, 'dist');
const site = join(root, 'site');

const version = buildDist();
const tokens = loadTokens();

// Every token as data: name, group, value (light, or comfortable), alt (dark, or compact), description, path.
const GROUP = { color: 'colour', typography: 'type', spacing: 'space', radius: 'radius', size: 'size', density: 'density', motion: 'motion', timing: 'timing', layer: 'layer', breakpoint: 'breakpoint' };
function tokenData() {
  const out = new Map();
  const add = (path, token, base, alt) => {
    const name = tokenName(path), entry = out.get(name) || { name, group: GROUP[path[0]], path: base, description: token.$description || '' };
    const v = cssValue(tokens, path, token);
    if (alt) { entry.alt = v; entry.altLabel = alt; } else entry.value = v;
    out.set(name, entry);
  };
  walk(tokens.color.light, ['color', 'light'], (p, t) => add(p, t, `color.*.${p.slice(2).join('.')}`));
  walk(tokens.color.dark, ['color', 'dark'], (p, t) => add(p, t, `color.*.${p.slice(2).join('.')}`, 'dark'));
  walk(tokens.density.comfortable, ['density', 'comfortable'], (p, t) => add(p, t, `density.*.${p.slice(2).join('.')}`));
  walk(tokens.density.compact, ['density', 'compact'], (p, t) => add(p, t, `density.*.${p.slice(2).join('.')}`, 'compact'));
  for (const g of ['typography', 'spacing', 'radius', 'size', 'motion', 'timing', 'layer', 'breakpoint']) walk(tokens[g], [g], (p, t) => add(p, t, p.join('.')));
  return [...out.values()];
}
const data = tokenData();
const byName = new Map(data.map((t) => [t.name, t]));
const esc = (t) => String(t).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
const WORD = { info: 'active', success: 'done', warning: 'waiting', error: 'failed' };
const blocks = {
  status: () => ['info', 'success', 'warning', 'error'].map((st, i) => `<div class="rk-tile" data-status="${st}" style="--rk-i:${i}"><span class="rk-tile-word">${WORD[st]}</span>`
    + `<span class="rk-stat"><b data-contrast="text-primary status-${st}-tile">–</b><span>primary text on ${st}</span></span>`
    + `<span class="rk-meta tile-hex">${byName.get(`status-${st}-tile`).value} · ${byName.get(`status-${st}-tile`).alt}</span></div>`).join('\n'),
  swatches: () => `<ul class="swatches">${['bg', 'surface', 'surface-2', 'text-primary', 'text-contrast-high', 'text-contrast-medium', 'text-contrast-low', 'accent', 'focus', 'disabled'].map((n) => {
    const t = byName.get(n), v = t.value.startsWith('var(') ? 'text-primary' : t.value, a = t.alt.startsWith('var(') ? 'text-primary' : t.alt;
    return `<li><span class="swatch" style="background:var(--rk-${n})"></span><code>${n}</code><span class="rk-meta">${esc(v)} · ${esc(a)}</span></li>`;
  }).join('')}</ul>`,
  pairs: () => `<table class="rk-table pairs"><thead><tr><th scope="col">pair</th><th scope="col" class="rk-num">needs</th><th scope="col" class="rk-num">now</th></tr></thead><tbody>${[
    ['text-primary', 'bg', 4.5], ['text-primary', 'surface', 4.5], ['text-contrast-high', 'bg', 4.5], ['text-contrast-high', 'surface', 4.5],
    ['text-contrast-medium', 'bg', 3], ['focus', 'bg', 3], ['on-accent', 'accent', 4.5],
  ].map(([fg, bg, min]) => `<tr><th scope="row"><code>${fg}</code> on <code>${bg}</code></th><td class="rk-num">${min} : 1</td><td class="rk-num"><b data-contrast="${fg} ${bg}" data-min="${min}">–</b></td></tr>`).join('')}</tbody></table>`,
  typescale: () => `<ul class="rk-rows scale">${data.filter((t) => t.group === 'type' && t.name.startsWith('font-size-')).map((t) => `<li><span class="scale-sample" style="font-size:var(--rk-${t.name})">Rekta</span><span><code>${t.name.slice(10)}</code> <span class="rk-meta">${esc(t.value)}</span></span></li>`).join('')}</ul>`,
  'tokens-data': () => `<script type="application/json" id="tokens-data">${JSON.stringify(data).replace(/</g, '\\u003c')}</script>`,
  // The icon sprite inline, so <use href="#rk-i-NAME"> works in the page and in the single-file build alike.
  icons: () => readFileSync(join(dist, 'rekta.icons.svg'), 'utf8').trim().replace('<svg ', '<svg aria-hidden="true" '),
  'icon-gallery': () => `<ul class="icon-gallery">${Object.keys(loadIcons()).map((n) => `<li><svg class="rk-icon" aria-hidden="true"><use href="#rk-i-${n}"/></svg><code>${n}</code></li>`).join('')}</ul>`,
};
function render(text) {
  return text
    .replace(/<!--@([\w-]+)-->/g, (_, n) => { if (!blocks[n]) throw new Error(`unknown block <!--@${n}-->`); return blocks[n](); })
    .replace(/\{\{([\w-]+)(\|px)?\}\}/g, (_, n, px) => {
      const t = byName.get(n);
      if (!t) throw new Error(`unknown token {{${n}}}`);
      return esc(px ? t.value.replace(/(-?[\d.]+)rem\b/g, (m, v) => `${+v * 16}px`) : t.value);
    });
}

rmSync(out, { recursive: true, force: true });
mkdirSync(out, { recursive: true });
for (const f of readdirSync(site)) {
  if (/\.html$/.test(f)) writeFileSync(join(out, f), render(readFileSync(join(site, f), 'utf8')));
  else if (/\.(css|js|png|svg|webmanifest)$/.test(f) && f !== 'sw.js') copyFileSync(join(site, f), join(out, f));
}
for (const f of readdirSync(dist)) if (f.startsWith('rekta.')) copyFileSync(join(dist, f), join(out, f));
cpSync(join(dist, 'fonts'), join(out, 'fonts'), { recursive: true });
// The service worker caches every file of the site; its version is a hash of them, so each change makes a new one.
const files = [];
(function list(dir, rel) { for (const f of readdirSync(dir).sort()) { const p = join(dir, f); if (statSync(p).isDirectory()) list(p, `${rel}${f}/`); else files.push(`${rel}${f}`); } }(out, ''));
const hash = createHash('sha256');
for (const f of files) hash.update(f).update(readFileSync(join(out, f)));
writeFileSync(join(out, 'sw.js'), readFileSync(join(site, 'sw.js'), 'utf8').replace('__RK_VERSION__', `${version}-${hash.digest('hex').slice(0, 10)}`).replace('__RK_FILES__', JSON.stringify(['./', ...files])));
console.log(`✓ _site/ written (Rekta Design ${version} showcase)`);

const single = process.argv.indexOf('--single');
if (single > 0) {
  const target = process.argv[single + 1];
  if (!target) throw new Error('--single needs a file name');
  const html = readFileSync(join(out, 'index.html'), 'utf8');
  const read = (f) => readFileSync(join(out, f), 'utf8');
  const fonts = read('rekta.fonts.css').replace(/url\(fonts\/([\w.-]+)\)/g, (_, f) => `url(data:font/woff2;base64,${readFileSync(join(out, 'fonts', f)).toString('base64')})`);
  const style = (f) => {
    const css = f === 'rekta.fonts.css' ? fonts : read(f);
    if (/<\/style/i.test(css)) throw new Error(`${f} contains a closing style tag`);
    return `<style>\n${css}</style>`;
  };
  const script = (f) => {
    const js = read(f);
    if (/<\/script/i.test(js)) throw new Error(`${f} contains a closing script tag`);
    return `<script>\n${js}</script>`;
  };
  const head = /<head>([\s\S]*?)<\/head>/.exec(html)[1], body = /<body>([\s\S]*?)<\/body>/.exec(html)[1];
  const inline = (part) => part
    .replace(/<link rel="stylesheet" href="([\w.-]+)">/g, (_, f) => style(f))
    .replace(/<script src="([\w.-]+)"><\/script>/g, (_, f) => script(f));
  // The dist files do not travel with one page, so their download links become plain names.
  const page = (inline(head).replace(/^\s*<meta[^>]*>\n/gm, '').replace(/^\s*<link rel="(manifest|icon|apple-touch-icon)"[^>]*>\n/gm, '') + inline(body)).replace(/<a href="(rekta\.[\w.]+)" download>([^<]*)<\/a>/g, '<code>$2</code>');
  writeFileSync(target, page.trim() + '\n');
  console.log(`✓ ${target} written (${Math.round(page.length / 1024)} kB, self-contained)`);
}

if (process.argv.includes('--serve')) {
  const types = {
    '.html': 'text/html; charset=utf-8', '.css': 'text/css; charset=utf-8', '.js': 'text/javascript; charset=utf-8',
    '.json': 'application/json', '.woff2': 'font/woff2', '.txt': 'text/plain; charset=utf-8',
  };
  createServer((req, res) => {
    try {
      let file = join(out, normalize(decodeURIComponent(new URL(req.url, 'http://localhost').pathname)));
      if (file !== out && !file.startsWith(out + sep)) throw new Error('outside _site');
      if (statSync(file).isDirectory()) file = join(file, 'index.html');
      statSync(file);
      res.writeHead(200, { 'content-type': types[extname(file)] ?? 'application/octet-stream' });
      createReadStream(file).pipe(res);
    } catch {
      res.writeHead(404, { 'content-type': 'text/plain' }).end('not found');
    }
  }).listen(4199, '127.0.0.1', () => console.log('serving http://127.0.0.1:4199'));
}

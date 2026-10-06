#!/usr/bin/env node
// Plain Node, no dependencies. Builds the showcase site into _site/: runs the CSS
// build, copies site/ plus dist/rekta.css and the token file, and fills two kinds
// of placeholders in site/*.html|css|js from tokens/rekta.tokens.json:
//   {{dotted.token.path}}  the token's CSS value, e.g. {{motion.duration.short}} -> 250ms
//   <!--@block-->          generated markup (swatches, status tiles, contrast pairs, type scale)
// `--serve` then serves _site/ on http://127.0.0.1:4199 (Ctrl+C to stop).

import { cpSync, copyFileSync, createReadStream, readFileSync, readdirSync, rmSync, statSync, writeFileSync } from 'node:fs';
import { createServer } from 'node:http';
import { dirname, extname, join, normalize, sep } from 'node:path';
import { fileURLToPath } from 'node:url';
import { cssValue, loadTokens, walk, writeCss } from './build-css.mjs';

const root = join(dirname(fileURLToPath(import.meta.url)), '..');
const out = join(root, '_site');
const tokens = loadTokens();

function token(path) {
  const node = path.split('.').reduce((n, key) => n?.[key], tokens);
  if (!node || !('$value' in node)) throw new Error(`unknown token in site source: {{${path}}}`);
  return node;
}

// Colour tokens become one CSS variable per name; the theme picks light or dark values.
const colorVar = (name) => `var(--rekta-color-${name.replaceAll('.', '-')})`;
const isMeta = (key) => key.startsWith('$');

const STATUS_MEANING = { info: 'active', warning: 'waiting', error: 'failed', success: 'done' };

function swatches() {
  const items = [];
  walk(tokens.color.light, [], (path, light) => {
    if (path[0] === 'status' || !String(light.$value).startsWith('#')) return;
    const name = path.join('.');
    const dark = token(`color.dark.${name}`).$value;
    items.push(`<li class="swatch"><span class="swatch-fill" style="background:${colorVar(name)}"></span><code>${name}</code><span class="hex">${light.$value} · ${dark}</span></li>`);
  });
  return `<ul class="swatches">${items.join('')}</ul>`;
}

function statusTiles() {
  return Object.keys(tokens.color.light.status).filter((key) => !isMeta(key)).map((status) => {
    const tile = (theme) => token(`color.${theme}.status.${status}.tile`).$value;
    return `<div class="tile" data-status="${status}">
  <span class="tile-word">${STATUS_MEANING[status] ?? status}</span>
  <span class="stat"><b data-contrast="text-primary status-${status}-tile">–</b><span>${status} · primary text on tile</span></span>
  <span class="forms"><i style="background:${colorVar(`status.${status}.full`)}"></i>full <i style="background:${colorVar(`status.${status}.soft`)}"></i>soft</span>
  <span class="hex">tile ${tile('light')} · ${tile('dark')}</span>
</div>`;
  }).join('\n');
}

// Pairs on neutral ground; status-tile pairs are shown on the tiles themselves.
const NEUTRAL_PAIRS = [
  ['text-primary', 'bg', 4.5], ['text-primary', 'surface', 4.5],
  ['text-contrast-high', 'bg', 4.5], ['text-contrast-high', 'surface', 4.5],
  ['focus', 'bg', 3], ['focus', 'surface', 3]
];

function contrastPairs() {
  const rows = NEUTRAL_PAIRS.map(([fg, bg, min]) =>
    `<tr><td><code>${fg}</code> on <code>${bg}</code></td><td>≥ ${min}:1</td><td><b data-contrast="${fg} ${bg}" data-min="${min}">–</b></td></tr>`);
  return `<table class="pairs"><thead><tr><th>pair</th><th>needs</th><th>now</th></tr></thead><tbody>${rows.join('')}</tbody></table>`;
}

function typeScale() {
  const sizes = Object.entries(tokens.typography.size).filter(([key]) => !isMeta(key)).map(([key, t]) =>
    `<li><span class="ts-sample" style="font-size:var(--rekta-typography-size-${key})">Rekta</span><code>${key}</code><span class="hex">${cssValue(t)}</span></li>`);
  const weights = Object.entries(tokens.typography.weight).filter(([key]) => !isMeta(key)).map(([key, t]) =>
    `<li><span class="ts-sample" style="font-weight:${t.$value}">${key}</span><span class="hex">${t.$value}</span></li>`);
  return `<ul class="scale">${sizes.join('')}</ul><ul class="scale scale--weights">${weights.join('')}</ul>`;
}

const blocks = { swatches, status: statusTiles, pairs: contrastPairs, typescale: typeScale };

function render(text) {
  return text
    .replace(/<!--@([\w-]+)-->/g, (_, name) => {
      if (!blocks[name]) throw new Error(`unknown block in site source: <!--@${name}-->`);
      return blocks[name]();
    })
    .replace(/\{\{([\w.-]+)\}\}/g, (_, path) => cssValue(token(path)));
}

writeCss(tokens);
rmSync(out, { recursive: true, force: true });
cpSync(join(root, 'site'), out, { recursive: true });
for (const file of readdirSync(out)) {
  if (/\.(html|css|js)$/.test(file)) writeFileSync(join(out, file), render(readFileSync(join(out, file), 'utf8')));
}
copyFileSync(join(root, 'dist', 'rekta.css'), join(out, 'rekta.css'));
copyFileSync(join(root, 'tokens', 'rekta.tokens.json'), join(out, 'rekta.tokens.json'));
console.log(`built ${out}`);

if (process.argv.includes('--serve')) {
  const types = {
    '.html': 'text/html; charset=utf-8', '.css': 'text/css; charset=utf-8', '.js': 'text/javascript; charset=utf-8',
    '.json': 'application/json', '.woff2': 'font/woff2', '.txt': 'text/plain; charset=utf-8'
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

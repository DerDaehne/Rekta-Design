#!/usr/bin/env node
// Plain Node, no dependencies. Builds the demo site, Rekta Board, from site/ and the dist/ files it runs on.
//
//   node scripts/build-site.mjs                  runs the build, then writes _site/: site/*.html|css|js beside
//                                                dist/rekta.* and dist/fonts/
//   node scripts/build-site.mjs --single FILE    also writes one self-contained page (styles, fonts and scripts
//                                                inline, no <html>/<head>/<body> wrapper) for hosts that take a single
//                                                file, such as an artifact
//   node scripts/build-site.mjs --serve          then serves _site/ on http://127.0.0.1:4199 (Ctrl+C to stop)

import { copyFileSync, cpSync, createReadStream, mkdirSync, readFileSync, readdirSync, rmSync, statSync, writeFileSync } from 'node:fs';
import { createServer } from 'node:http';
import { dirname, extname, join, normalize, sep } from 'node:path';
import { fileURLToPath } from 'node:url';
import { buildDist } from './build.mjs';

const root = join(dirname(fileURLToPath(import.meta.url)), '..');
const out = join(root, '_site');
const dist = join(root, 'dist');
const site = join(root, 'site');

const version = buildDist();
rmSync(out, { recursive: true, force: true });
mkdirSync(out, { recursive: true });
for (const f of readdirSync(site)) if (/\.(html|css|js)$/.test(f)) copyFileSync(join(site, f), join(out, f));
for (const f of readdirSync(dist)) if (f.startsWith('rekta.')) copyFileSync(join(dist, f), join(out, f));
cpSync(join(dist, 'fonts'), join(out, 'fonts'), { recursive: true });
console.log(`✓ _site/ written (Rekta Board on Rekta Design ${version})`);

const single = process.argv.indexOf('--single');
if (single > 0) {
  const target = process.argv[single + 1];
  if (!target) throw new Error('--single needs a file name');
  const html = readFileSync(join(site, 'index.html'), 'utf8');
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
  const page = inline(head).replace(/^\s*<meta[^>]*>\n/gm, '') + inline(body);
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

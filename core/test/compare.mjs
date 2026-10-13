// Compares the PNGs a test run wrote with the reference images. A pixel counts as different when a channel is off by
// more than 8 (libm and compilers differ in the last bits); an image fails when more than 0.2% of its pixels differ.
//   node test/compare.mjs REF_DIR OUT_DIR [--update]   --update copies the run's images over the references
import { copyFileSync, existsSync, mkdirSync, readFileSync, readdirSync } from 'node:fs';
import { join } from 'node:path';
import { inflateSync } from 'node:zlib';

const [ref, out] = process.argv.slice(2).filter((a) => !a.startsWith('--'));
const update = process.argv.includes('--update');

function decode(file) {
  const b = readFileSync(file);
  let at = 8, w = 0, h = 0, idat = [];
  while (at < b.length) {
    const len = b.readUInt32BE(at), type = b.toString('latin1', at + 4, at + 8), data = b.subarray(at + 8, at + 8 + len);
    if (type === 'IHDR') { w = data.readUInt32BE(0); h = data.readUInt32BE(4); if (data[8] !== 8 || data[9] !== 6) throw new Error(`${file}: not 8-bit RGBA`); }
    if (type === 'IDAT') idat.push(data);
    at += 12 + len;
  }
  const raw = inflateSync(Buffer.concat(idat)), px = Buffer.alloc(w * h * 4), row = w * 4;
  for (let y = 0; y < h; y++) {
    const f = raw[y * (row + 1)], src = y * (row + 1) + 1, dst = y * row;
    for (let x = 0; x < row; x++) {
      const a = x >= 4 ? px[dst + x - 4] : 0, up = y ? px[dst - row + x] : 0, c = x >= 4 && y ? px[dst - row + x - 4] : 0;
      let v = raw[src + x];
      if (f === 1) v += a; else if (f === 2) v += up; else if (f === 3) v += (a + up) >> 1;
      else if (f === 4) { const p = a + up - c, pa = Math.abs(p - a), pb = Math.abs(p - up), pc = Math.abs(p - c); v += pa <= pb && pa <= pc ? a : pb <= pc ? up : c; }
      px[dst + x] = v & 255;
    }
  }
  return { w, h, px };
}

const names = readdirSync(out).filter((f) => f.endsWith('.png')).sort();
if (update) {
  mkdirSync(ref, { recursive: true });
  for (const n of names) copyFileSync(join(out, n), join(ref, n));
  console.log(`✓ ${names.length} reference images updated in ${ref}`);
  process.exit(0);
}
let failed = 0;
for (const n of names) {
  if (!existsSync(join(ref, n))) { console.log(`✗ ${n}: no reference (run with --update)`); failed++; continue; }
  const a = decode(join(ref, n)), b = decode(join(out, n));
  if (a.w !== b.w || a.h !== b.h) { console.log(`✗ ${n}: ${b.w}×${b.h}, the reference is ${a.w}×${a.h}`); failed++; continue; }
  let diff = 0, worst = 0;
  for (let i = 0; i < a.px.length; i += 4) {
    let m = 0;
    for (let k = 0; k < 4; k++) m = Math.max(m, Math.abs(a.px[i + k] - b.px[i + k]));
    if (m > 8) diff++;
    worst = Math.max(worst, m);
  }
  const share = diff / (a.w * a.h);
  if (share > 0.002) { console.log(`✗ ${n}: ${(share * 100).toFixed(2)}% of the pixels differ (worst ${worst})`); failed++; }
  else console.log(`✓ ${n}${diff ? ` (${diff} pixels within tolerance, worst ${worst})` : ''}`);
}
for (const n of existsSync(ref) ? readdirSync(ref).filter((f) => f.endsWith('.png')) : []) if (!names.includes(n)) { console.log(`✗ ${n}: the run did not write it`); failed++; }
console.log(failed ? `${failed} of ${names.length} images differ` : `all ${names.length} images match the references`);
process.exit(failed ? 1 : 0);

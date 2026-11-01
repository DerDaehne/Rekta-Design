// Colour maths for the token check, and for a product that checks or derives its own colours (shipped as
// dist/rekta.color.js): WCAG 2 contrast, OKLab distance and colour-vision-deficiency simulation
// (Machado, Oliveira and Fernandes 2009, severity 1). No dependencies.

export function parse(css) {
  const s = String(css).trim();
  let m = /^#([0-9a-f]{6})([0-9a-f]{2})?$/i.exec(s);
  if (m) {
    const n = parseInt(m[1], 16);
    return { r: (n >> 16) / 255, g: ((n >> 8) & 255) / 255, b: (n & 255) / 255, a: m[2] ? parseInt(m[2], 16) / 255 : 1 };
  }
  m = /^#([0-9a-f]{3})$/i.exec(s);
  if (m) return parse('#' + m[1].split('').map((c) => c + c).join(''));
  m = /^rgba?\(\s*([\d.]+)\s*,\s*([\d.]+)\s*,\s*([\d.]+)\s*(?:,\s*([\d.]+)\s*)?\)$/i.exec(s);
  if (m) return { r: +m[1] / 255, g: +m[2] / 255, b: +m[3] / 255, a: m[4] === undefined ? 1 : +m[4] };
  throw new Error(`unsupported colour: ${css}`);
}

export function toHex8({ r, g, b, a }) {
  const h = (v) => Math.round(Math.min(1, Math.max(0, v)) * 255).toString(16).padStart(2, '0');
  return '#' + h(r) + h(g) + h(b) + (a < 1 ? h(a) : '');
}

// A translucent colour laid over an opaque one (alpha compositing in sRGB, as a browser paints it), as #rrggbb.
export function over(top, under) {
  const t = parse(top), u = parse(under);
  return toHex8({ r: t.r * t.a + u.r * (1 - t.a), g: t.g * t.a + u.g * (1 - t.a), b: t.b * t.a + u.b * (1 - t.a), a: 1 });
}

const lin = (v) => (v <= 0.04045 ? v / 12.92 : ((v + 0.055) / 1.055) ** 2.4);
const gam = (v) => (v <= 0.0031308 ? 12.92 * v : 1.055 * v ** (1 / 2.4) - 0.055);

export function luminance(css) {
  const { r, g, b } = parse(css);
  return 0.2126 * lin(r) + 0.7152 * lin(g) + 0.0722 * lin(b);
}

export function contrast(a, b) {
  const [x, y] = [luminance(a), luminance(b)].sort((p, q) => q - p);
  return (x + 0.05) / (y + 0.05);
}

function oklab({ r, g, b }) {
  [r, g, b] = [r, g, b].map(lin);
  const l = Math.cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b);
  const m = Math.cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b);
  const s = Math.cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b);
  return [0.2104542553 * l + 0.793617785 * m - 0.0040720468 * s, 1.9779984951 * l - 2.428592205 * m + 0.4505937099 * s, 0.0259040371 * l + 0.7827717662 * m - 0.808675766 * s];
}

const CVD = {
  protan: [[0.152286, 1.052583, -0.204868], [0.114503, 0.786281, 0.099216], [-0.003882, -0.048116, 1.051998]],
  deutan: [[0.367322, 0.860646, -0.227968], [0.280085, 0.672501, 0.047413], [-0.01182, 0.04294, 0.968881]],
  tritan: [[1.255528, -0.076749, -0.178779], [-0.078411, 0.930809, 0.147602], [0.004733, 0.691367, 0.3039]],
};

export function simulate(css, kind) {
  const c = parse(css), v = [lin(c.r), lin(c.g), lin(c.b)];
  const [r, g, b] = CVD[kind].map((row) => gam(Math.min(1, Math.max(0, row[0] * v[0] + row[1] * v[1] + row[2] * v[2]))));
  return { r, g, b, a: 1 };
}

export function distance(a, b, kind) {
  const p = oklab(kind ? simulate(a, kind) : parse(a)), q = oklab(kind ? simulate(b, kind) : parse(b));
  return Math.hypot(p[0] - q[0], p[1] - q[1], p[2] - q[2]);
}

// OKLCH, the polar form of OKLab: lightness 0 to 1, chroma, hue in degrees.
export function oklch(css) {
  const [L, a, b] = oklab(parse(css));
  return [L, Math.hypot(a, b), ((Math.atan2(b, a) * 180) / Math.PI + 360) % 360];
}

// The sRGB colour at an OKLCH point as #rrggbb, or null where it lies outside sRGB.
export function fromOklch(L, C, h) {
  const a = C * Math.cos((h * Math.PI) / 180), b = C * Math.sin((h * Math.PI) / 180);
  const l = (L + 0.3963377774 * a + 0.2158037573 * b) ** 3, m = (L - 0.1055613458 * a - 0.0638541728 * b) ** 3, s = (L - 0.0894841775 * a - 1.291485548 * b) ** 3;
  const rgb = [4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s, -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s, -0.0041960863 * l - 0.7034186147 * m + 1.707614701 * s];
  if (rgb.some((v) => v < -1e-4 || v > 1 + 1e-4)) return null;
  const [r, g, bl] = rgb.map((v) => gam(Math.min(1, Math.max(0, v))));
  return toHex8({ r, g, b: bl, a: 1 });
}

// Apart from status (docs/colour.md): a colour that is not a status, a product's accent or a tile's content fill,
// keeps this much from every status-*-full and status-*-tile of its theme, so it never reads as one. OKLab distance
// seen and under protanopia and deuteranopia; its hue (OKLCH) from each status-*-full's, unless its chroma is so low
// that it reads as grey. Tritanopia is reported, as for the statuses among themselves.
export const APART = { normal: 0.09, protan: 0.05, deutan: 0.05, hue: 20, grey: 0.05 };
const STATUS = ['info', 'success', 'warning', 'error'];

// How far a colour stands from the status colours of one theme (a theme of rekta.tokens.json, resolved): for each
// kind of vision the nearest status colour and the distance to it, the nearest status hue, and what breaks the rule,
// in words. A product that lets its users choose a colour shows them the nearest status and why.
export function apartFromStatus(css, theme) {
  const nearest = {};
  for (const kind of ['normal', 'protan', 'deutan', 'tritan']) {
    for (const status of STATUS) for (const form of ['full', 'tile']) {
      const d = distance(css, theme[`status-${status}-${form}`], kind === 'normal' ? undefined : kind);
      if (!nearest[kind] || d < nearest[kind].distance) nearest[kind] = { status, form, distance: d };
    }
  }
  const [, chroma, h] = oklch(css);
  let hue = null;
  for (const status of STATUS) {
    const d = Math.abs(h - oklch(theme[`status-${status}-full`])[2]), degrees = Math.min(d, 360 - d);
    if (!hue || degrees < hue.degrees) hue = { status, degrees };
  }
  const problems = [];
  for (const kind of ['normal', 'protan', 'deutan']) {
    const n = nearest[kind];
    if (n.distance < APART[kind]) problems.push(`${n.distance.toFixed(3)} from status-${n.status}-${n.form}${kind === 'normal' ? '' : ` under ${kind}`}, needs ${APART[kind]}`);
  }
  if (chroma >= APART.grey && hue.degrees < APART.hue) problems.push(`its hue is ${hue.degrees.toFixed(0)}° from ${hue.status}'s, needs ${APART.hue}° (or a chroma below ${APART.grey})`);
  return { nearest, hue, chroma, problems };
}

// The text contrast each theme asks for: 7:1 under more contrast.
export const TEXT = { light: 4.5, dark: 4.5, 'light-contrast': 7, 'dark-contrast': 7 };

// An accent and the text on it in one theme (docs/colour.md, The accent): 3:1 on bg and surface, on-accent at the
// theme's text contrast on it, apart from status.
export function accentCheck(accent, onAccent, theme, text = 4.5) {
  const problems = [];
  for (const g of ['bg', 'surface']) {
    const r = contrast(accent, theme[g]);
    if (r < 3) problems.push(`${r.toFixed(2)}:1 on ${g}, needs 3:1`);
  }
  const r = contrast(onAccent, accent);
  if (r < text) problems.push(`on-accent on it is ${r.toFixed(2)}:1, needs ${text}:1`);
  const apart = apartFromStatus(accent, theme);
  problems.push(...apart.problems);
  return { ok: !problems.length, problems, apart };
}

// An accent derived from one colour (docs/colour.md, Deriving an accent), for each theme given (the themes of
// rekta.tokens.json): its hue and chroma kept, its lightness moved as little as it takes for every accent rule to
// hold, in steps of 0.005 (of two as near, the darker). Outside sRGB the chroma comes down until it fits. on-accent is
// the theme's bg or text-primary, whichever stands out more on it. A colour whose own hue is a status hue is refused
// first: no lightness changes its hue, and a chroma brought down to fit must not pass it off as grey. A theme where no
// lightness holds refuses the colour too, with what breaks the rules at the colour's own lightness.
export function deriveAccent(seed, themes) {
  const [L0, C, h] = oklch(seed);
  const steps = Array.from({ length: 201 }, (_, i) => i / 200).sort((x, y) => Math.abs(x - L0) - Math.abs(y - L0));
  const out = { ok: true, themes: {}, problems: {} };
  for (const [name, text] of Object.entries(TEXT)) {
    const theme = themes[name];
    if (!theme) continue;
    const at = (L) => {
      let c = fromOklch(L, C, h);
      if (!c) {
        let lo = 0, hi = C;
        for (let i = 0; i < 24; i += 1) { const mid = (lo + hi) / 2; if (fromOklch(L, mid, h)) lo = mid; else hi = mid; }
        c = fromOklch(L, lo, h);
      }
      const on = contrast(theme.bg, c) >= contrast(theme['text-primary'], c) ? theme.bg : theme['text-primary'];
      return { accent: c, 'on-accent': on };
    };
    const { hue } = apartFromStatus(seed, theme);
    if (C >= APART.grey && hue.degrees < APART.hue) {
      out.ok = false;
      out.problems[name] = [`its hue is ${hue.degrees.toFixed(0)}° from ${hue.status}'s, needs ${APART.hue}° (or a chroma below ${APART.grey})`];
      continue;
    }
    const found = steps.map(at).find((p) => accentCheck(p.accent, p['on-accent'], theme, text).ok);
    if (found) out.themes[name] = found;
    else {
      const own = at(L0);
      out.ok = false;
      out.problems[name] = ['no lightness holds every rule', ...accentCheck(own.accent, own['on-accent'], theme, text).problems];
    }
  }
  return out;
}

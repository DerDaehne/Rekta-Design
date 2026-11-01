// Rekta Design colour rules (rekta.color.js; rekta.color.global.js sets window.RektaColor). No DOM, no dependencies.
// A theme is one of `themes` in rekta.tokens.json: every colour token of that theme by name, resolved to #rrggbb.

export type Theme = Record<string, string>;
export type ThemeName = 'light' | 'dark' | 'light-contrast' | 'dark-contrast';
export type Vision = 'protan' | 'deutan' | 'tritan';
export interface Rgba { r: number; g: number; b: number; a: number }
export interface Nearest { status: 'info' | 'success' | 'warning' | 'error'; form: 'full' | 'tile'; distance: number }
export interface Apart {
  /** Per kind of vision, the nearest status colour of the theme and the OKLab distance to it. */
  nearest: Record<'normal' | Vision, Nearest>;
  /** The nearest status hue and how far, in degrees. */
  hue: { status: Nearest['status']; degrees: number };
  chroma: number;
  /** What breaks the rule, in words; empty when the colour stands apart. */
  problems: string[];
}
export interface AccentPair { accent: string; 'on-accent': string }

/** A CSS colour (#rgb, #rrggbb, #rrggbbaa, rgb(), rgba()) as channels 0 to 1. */
export declare function parse(css: string): Rgba;
export declare function toHex8(c: Rgba): string;
/** A translucent colour laid over an opaque one, as a browser paints it. */
export declare function over(top: string, under: string): string;
export declare function luminance(css: string): number;
/** WCAG 2 contrast ratio, 1 to 21. */
export declare function contrast(a: string, b: string): number;
/** The colour as seen with a colour-vision deficiency (Machado et al. 2009, severity 1). */
export declare function simulate(css: string, kind: Vision): Rgba;
/** OKLab distance, seen or under a deficiency. */
export declare function distance(a: string, b: string, kind?: Vision): number;
/** [lightness 0 to 1, chroma, hue in degrees]. */
export declare function oklch(css: string): [number, number, number];
/** #rrggbb, or null outside sRGB. */
export declare function fromOklch(L: number, C: number, h: number): string | null;
/** Apart from status: OKLab 0.09 seen, 0.05 under protanopia and deuteranopia, hue 20° unless the chroma is below 0.05. */
export declare const APART: { normal: number; protan: number; deutan: number; hue: number; grey: number };
export declare function apartFromStatus(css: string, theme: Theme): Apart;
/** The text contrast each theme asks for: 4.5, and 7 under more contrast. */
export declare const TEXT: Record<ThemeName, number>;
/** An accent and the text on it in one theme: 3:1 on bg and surface, on-accent at `text`, apart from status. */
export declare function accentCheck(accent: string, onAccent: string, theme: Theme, text?: number): { ok: boolean; problems: string[]; apart: Apart };
/** An accent derived from one colour for each theme given: lightness moved as little as the rules need. */
export declare function deriveAccent(seed: string, themes: Partial<Record<ThemeName, Theme>>): {
  ok: boolean;
  themes: Partial<Record<ThemeName, AccentPair>>;
  problems: Partial<Record<ThemeName, string[]>>;
};

// src/graphics/sprites/decorations/TowerKit.ts
//
// Isometric building helpers for the Ember Tower (余烬之塔) props. World px,
// 2:1 iso: one tile is 64×32, so a point `a` px along the +col axis and `b`
// px along the +row axis sits at (a − b, (a + b) / 2) from the footprint
// centre; `a = b = 32` is the front corner of a 2×2 footprint. Visible faces:
// the +b face (front-left, lit) and the +a face (front-right, shaded).
import { tone, glow, type Tone, type Rand, shade, polyP, flat, line, ellipseP, blobP, contactShadow, rgbaHex, curve } from './DecorKit';
import { drawFlame } from './CampProps';
import { drawRock } from './Rock';

export type P2 = [number, number];
type Ctx = CanvasRenderingContext2D;

export interface Iso {
  cx: number;
  gy: number;
}

/** Screen point of footprint coords (a, b) at height h. */
export function ip(o: Iso, a: number, b: number, h = 0): P2 {
  return [o.cx + a - b, o.gy + (a + b) / 2 - h];
}

const toneCache = new Map<string, Tone>();
export function T(c: number, light = 0.3, shadow = 0.42): Tone {
  const k = `${c}:${light}:${shadow}`;
  let t = toneCache.get(k);
  if (!t) {
    t = tone(c, { light, shadow });
    toneCache.set(k, t);
  }
  return t;
}

/**
 * Axis-aligned iso box between a0..a1, b0..b1, heights h0..h1. `faces`
 * picks the tones; the top is optional (roofs cover it).
 */
export function box(
  ctx: Ctx, o: Iso, a0: number, a1: number, b0: number, b1: number, h0: number, h1: number, t: Tone,
  opts: { top?: boolean; lw?: number; leftFill?: string; rightFill?: string; topFill?: string } = {},
): void {
  const lw = opts.lw ?? 0.7;
  const left = [ip(o, a0, b1, h0), ip(o, a1, b1, h0), ip(o, a1, b1, h1), ip(o, a0, b1, h1)];
  const right = [ip(o, a1, b1, h0), ip(o, a1, b0, h0), ip(o, a1, b0, h1), ip(o, a1, b1, h1)];
  flat(ctx, polyP(ctx, left), opts.leftFill ?? t.base, t.line, lw);
  flat(ctx, polyP(ctx, right), opts.rightFill ?? t.shade, t.line, lw);
  if (opts.top !== false) {
    flat(ctx, polyP(ctx, [ip(o, a0, b0, h1), ip(o, a1, b0, h1), ip(o, a1, b1, h1), ip(o, a0, b1, h1)]), opts.topFill ?? t.light, t.line, lw);
  }
}

/** Flat diamond on the ground (plinths, beds, rugs). */
export function slab(ctx: Ctx, o: Iso, a0: number, a1: number, b0: number, b1: number, h: number, t: Tone, lw = 0.7): void {
  box(ctx, o, a0, a1, b0, b1, 0, h, t, { lw });
}

/** Gable roof over a box: ridge along the a axis (default) or b axis. */
export function gable(
  ctx: Ctx, o: Iso, a0: number, a1: number, b0: number, b1: number, h: number, rise: number, t: Tone,
  ridgeAlongA = true, over = 3,
): void {
  if (ridgeAlongA) {
    const bm = (b0 + b1) / 2;
    // Gable end on the +a face (triangle), roof plane toward +b (front-left).
    const tri = [ip(o, a1, b0 - over, h - 1), ip(o, a1, b1 + over, h - 1), ip(o, a1, bm, h + rise)];
    flat(ctx, polyP(ctx, tri), t.shade, t.line, 0.7);
    const plane = [ip(o, a0 - over, bm, h + rise), ip(o, a1 + over * 0.3, bm, h + rise), ip(o, a1 + over * 0.3, b1 + over, h - 1), ip(o, a0 - over, b1 + over, h - 1)];
    shade(ctx, polyP(ctx, plane), t, { band: 2, hi: 1.2, stroke: 0.8 });
  } else {
    const am = (a0 + a1) / 2;
    const tri = [ip(o, a0 - over, b1, h - 1), ip(o, a1 + over, b1, h - 1), ip(o, am, b1, h + rise)];
    flat(ctx, polyP(ctx, tri), t.base, t.line, 0.7);
    const plane = [ip(o, am, b0 - over, h + rise), ip(o, am, b1 + over * 0.3, h + rise), ip(o, a1 + over, b1 + over * 0.3, h - 1), ip(o, a1 + over, b0 - over, h - 1)];
    flat(ctx, polyP(ctx, plane), t.shade, t.line, 0.8);
  }
}

/** Shingle / plank lines across a roof plane between two eave points and the ridge. */
export function roofLines(ctx: Ctx, from: P2, to: P2, ridgeFrom: P2, ridgeTo: P2, n: number, color: string): void {
  for (let i = 1; i < n; i++) {
    const k = i / n;
    const a: P2 = [from[0] + (ridgeFrom[0] - from[0]) * k, from[1] + (ridgeFrom[1] - from[1]) * k];
    const b: P2 = [to[0] + (ridgeTo[0] - to[0]) * k, to[1] + (ridgeTo[1] - to[1]) * k];
    line(ctx, [a, b], color, 0.6);
  }
}

/** Upright cylinder (well rings, tower shafts, braziers). */
export function cylinder(ctx: Ctx, x: number, gy: number, r: number, h: number, t: Tone, opts: { top?: boolean; topFill?: string; rTop?: number; band?: number } = {}): void {
  const rt = opts.rTop ?? r;
  shade(ctx, () => {
    ctx.moveTo(x - r, gy);
    ctx.lineTo(x - rt, gy - h);
    ctx.lineTo(x + rt, gy - h);
    ctx.lineTo(x + r, gy);
    ctx.ellipse(x, gy, r, r * 0.5, 0, 0, Math.PI);
    ctx.closePath();
  }, t, { band: opts.band ?? r * 0.55, hi: Math.max(1, r * 0.15), stroke: 0.8 });
  if (opts.top !== false) flat(ctx, ellipseP(ctx, x, gy - h, rt, rt * 0.5), opts.topFill ?? t.light, t.line, 0.7);
}

/** Block courses on a face strip: horizontal lines along a face between two base points. */
export function faceCourses(ctx: Ctx, p0: P2, p1: P2, h0: number, h1: number, step: number, color: string, r: Rand, joints = true): void {
  let row = 0;
  for (let h = h0 + step; h < h1 - 0.5; h += step) {
    line(ctx, [[p0[0], p0[1] - h], [p1[0], p1[1] - h]], color, 0.55);
    if (joints) {
      const n = Math.max(1, Math.round(Math.hypot(p1[0] - p0[0], p1[1] - p0[1]) / 9));
      for (let i = 0; i < n; i++) {
        const k = (i + (row % 2 ? 0.5 : 0.1) + r() * 0.15) / n;
        if (k >= 1) continue;
        const x = p0[0] + (p1[0] - p0[0]) * k;
        const y = p0[1] + (p1[1] - p0[1]) * k;
        line(ctx, [[x, y - h], [x, y - Math.min(h1, h + step)]], color, 0.5);
      }
    }
    row++;
  }
}

/** Heap of rubble stones around (x, gy). */
export function rubble(ctx: Ctx, x: number, gy: number, w: number, n: number, col: number, r: Rand, sizeK = 1): void {
  for (let i = 0; i < n; i++) {
    const dx = (r() - 0.5) * w;
    const dy = (r() - 0.5) * w * 0.3;
    const s = (3 + r() * 5) * sizeK;
    drawRock(ctx, x + dx, gy + dy, s * 1.3, s, col, r, 0);
  }
}

/** Charred beam lying at an angle. */
export function beam(ctx: Ctx, a: P2, b: P2, w: number, col: number, charred = false): void {
  const dx = b[0] - a[0], dy = b[1] - a[1];
  const l = Math.hypot(dx, dy) || 1;
  const nx = (-dy / l) * w, ny = (dx / l) * w;
  const t = T(col, 0.25);
  shade(ctx, polyP(ctx, [[a[0] + nx, a[1] + ny], [b[0] + nx, b[1] + ny], [b[0] - nx, b[1] - ny], [a[0] - nx, a[1] - ny]]), t, { band: w * 0.8, hi: 0.5, stroke: 0.6 });
  flat(ctx, ellipseP(ctx, b[0], b[1], w * 0.9, w * 0.9), charred ? '#2a2220' : t.light, t.line, 0.5);
  if (charred) {
    for (let k = 0.2; k < 0.9; k += 0.25) {
      const x = a[0] + dx * k, y = a[1] + dy * k;
      line(ctx, [[x + nx * 0.8, y + ny * 0.8], [x - nx * 0.6 + dx * 0.04, y - ny * 0.6 + dy * 0.04]], '#1c1614', 0.6);
    }
  }
}

/** Soot / scorch stain on the ground. */
export function scorch(ctx: Ctx, x: number, gy: number, rx: number, alpha = 0.45): void {
  ctx.save();
  ctx.translate(x, gy);
  ctx.scale(1, 0.5);
  const g = ctx.createRadialGradient(0, 0, 0, 0, 0, rx);
  g.addColorStop(0, `rgba(30,22,20,${alpha})`);
  g.addColorStop(0.7, `rgba(30,22,20,${alpha * 0.5})`);
  g.addColorStop(1, 'rgba(30,22,20,0)');
  ctx.fillStyle = g;
  ctx.beginPath();
  ctx.arc(0, 0, rx, 0, Math.PI * 2);
  ctx.fill();
  ctx.restore();
}

/** Ground footprint patch (packed earth / flagstones) under a building. */
export function footprint(ctx: Ctx, o: Iso, half: number, col: number, alpha = 1, flags = false, r?: Rand): void {
  const pts = [ip(o, -half, -half), ip(o, half, -half), ip(o, half, half), ip(o, -half, half)];
  ctx.save();
  ctx.globalAlpha = alpha;
  const t = T(col, 0.2, 0.3);
  flat(ctx, blobP(ctx, pts.map(p => [p[0], p[1]] as P2)), t.base);
  if (flags && r) {
    for (let i = 0; i < 14; i++) {
      const a = (r() - 0.5) * half * 1.6;
      const b = (r() - 0.5) * half * 1.6;
      const [x, y] = ip(o, a, b);
      flat(ctx, ellipseP(ctx, x, y, 3.5 + r() * 2, 1.8 + r()), t.light, t.shade, 0.4);
    }
  }
  ctx.restore();
}

/** Hanging lantern with glow. */
export function lantern(ctx: Ctx, x: number, y: number, col = 0xffc060, size = 1): void {
  glow(ctx, { x, y }, 9 * size, col, 0.55);
  const t = T(0x3a2c24);
  flat(ctx, polyP(ctx, [[x - 2 * size, y - 2.5 * size], [x + 2 * size, y - 2.5 * size], [x + 1.5 * size, y + 2.5 * size], [x - 1.5 * size, y + 2.5 * size]]), rgbaHex(col, 0.95), t.line, 0.6);
  flat(ctx, polyP(ctx, [[x - 2.6 * size, y - 2.5 * size], [x + 2.6 * size, y - 2.5 * size], [x, y - 4.5 * size]]), t.base, t.line, 0.5);
  flat(ctx, ellipseP(ctx, x, y, 0.9 * size, 1.4 * size), '#fff4c8');
}

/** Pennant banner on a pole. */
export function pennant(ctx: Ctx, x: number, gy: number, h: number, col: number, wave = 0, dir = 1): void {
  const pole = T(0x6a4a30);
  shade(ctx, polyP(ctx, [[x - 0.9, gy], [x - 0.9, gy - h], [x + 0.9, gy - h], [x + 0.9, gy]]), pole, { band: 0.8, hi: 0, stroke: 0.5 });
  flat(ctx, ellipseP(ctx, x, gy - h - 1, 1.6, 1.6), '#e8c060', '#6a4a18', 0.5);
  const t = T(col, 0.35);
  const top = gy - h + 1;
  shade(ctx, () => {
    ctx.moveTo(x, top);
    ctx.quadraticCurveTo(x + dir * 8, top + 1 + wave, x + dir * 16, top + 3);
    ctx.lineTo(x + dir * 11, top + 6.5);
    ctx.lineTo(x + dir * 15, top + 10);
    ctx.quadraticCurveTo(x + dir * 7, top + 10 - wave, x, top + 11);
    ctx.closePath();
  }, t, { band: 2, hi: 0.8, stroke: 0.6 });
}

/** Vertical hanging banner (cloth) from a bar. */
export function hangBanner(ctx: Ctx, x: number, top: number, w: number, h: number, col: number, emblem?: (cx: number, cy: number) => void): void {
  const t = T(col, 0.35);
  shade(ctx, () => {
    ctx.moveTo(x - w / 2, top);
    ctx.lineTo(x + w / 2, top);
    ctx.lineTo(x + w / 2, top + h);
    ctx.lineTo(x, top + h - w * 0.35);
    ctx.lineTo(x - w / 2, top + h);
    ctx.closePath();
  }, t, { band: w * 0.3, hi: 0.8, stroke: 0.6 });
  line(ctx, [[x - w / 2 - 1.5, top], [x + w / 2 + 1.5, top]], '#5a3a20', 1.4);
  line(ctx, [[x - w / 2 + 1, top + 2], [x + w / 2 - 1, top + 2]], '#e8c060', 0.6);
  emblem?.(x, top + h * 0.45);
}

/** Leafy herb tuft. */
export function herb(ctx: Ctx, x: number, gy: number, h: number, col: number, r: Rand, flower?: string): void {
  const t = T(col, 0.35);
  for (let i = 0; i < 5; i++) {
    const a = -0.9 + (i / 4) * 1.8 + (r() - 0.5) * 0.3;
    const hh = h * (0.65 + r() * 0.4);
    const tip: P2 = [x + Math.sin(a) * hh, gy - Math.cos(a) * hh];
    ctx.beginPath();
    ctx.moveTo(x - 0.8, gy);
    ctx.quadraticCurveTo(x + Math.sin(a) * hh * 0.4 - 1.2, gy - hh * 0.6, tip[0], tip[1]);
    ctx.quadraticCurveTo(x + Math.sin(a) * hh * 0.4 + 1.2, gy - hh * 0.5, x + 0.8, gy);
    ctx.closePath();
    ctx.fillStyle = i % 2 ? t.base : t.light;
    ctx.fill();
    ctx.strokeStyle = t.line;
    ctx.lineWidth = 0.35;
    ctx.stroke();
    if (flower && i % 2 === 0) flat(ctx, ellipseP(ctx, tip[0], tip[1], 1.3, 1.1), flower, 'rgba(60,30,40,0.6)', 0.3);
  }
}

/** Wooden crate (iso) standing on (a, b). */
export function crate(ctx: Ctx, o: Iso, a: number, b: number, s: number, col = 0x9a6a3e, h0 = 0): void {
  const t = T(col, 0.3);
  box(ctx, o, a - s, a + s, b - s, b + s, h0, h0 + s * 2, t);
  const [lx0, ly0] = ip(o, a - s, b + s, h0 + 0.8);
  const [lx1, ly1] = ip(o, a + s, b + s, h0 + s * 2 - 0.8);
  line(ctx, [[lx0, ly0], [lx1, ly1]], t.line, 0.6);
  const [rx0, ry0] = ip(o, a + s, b + s, h0 + s * 2 - 0.8);
  const [rx1, ry1] = ip(o, a + s, b - s, h0 + 0.8);
  line(ctx, [[rx0, ry0], [rx1, ry1]], t.line, 0.6);
}

/** Barrel at screen point. */
export function barrel(ctx: Ctx, x: number, gy: number, r = 4.5, h = 11, col = 0x8a5a34): void {
  const t = T(col, 0.3);
  cylinder(ctx, x, gy, r, h, t, { topFill: T(col, 0.45).light });
  for (const k of [0.22, 0.78]) {
    ctx.beginPath();
    ctx.ellipse(x, gy - h * k, r + 0.2, (r + 0.2) * 0.5, 0, 0, Math.PI);
    ctx.strokeStyle = '#4a4040';
    ctx.lineWidth = 1;
    ctx.stroke();
  }
}

/** Sack. */
export function sack(ctx: Ctx, x: number, gy: number, s = 1, col = 0xc8ae7a): void {
  const t = T(col, 0.3);
  shade(ctx, blobP(ctx, [[x - 5 * s, gy], [x - 5.5 * s, gy - 5 * s], [x - 2 * s, gy - 9 * s], [x + 2 * s, gy - 9 * s], [x + 5.5 * s, gy - 5 * s], [x + 5 * s, gy]]), t, { band: 2 * s, hi: 0.8, stroke: 0.6 });
  line(ctx, [[x - 2 * s, gy - 8 * s], [x + 2 * s, gy - 8 * s]], t.line, 0.8);
}

/** Small ember bed / glowing coals. */
export function coals(ctx: Ctx, x: number, gy: number, w: number, r: Rand, hot = 1): void {
  glow(ctx, { x, y: gy - 1 }, w * 1.3, 0xff6a20, 0.45 * hot);
  for (let i = 0; i < 7; i++) {
    const dx = (r() - 0.5) * w;
    const dy = (r() - 0.5) * w * 0.3;
    flat(ctx, ellipseP(ctx, x + dx, gy + dy, 1.8, 1.1), i % 3 ? '#ff8a30' : '#ffd070', '#5a1a10', 0.3);
  }
}

export { drawFlame, glow, contactShadow, flat, line, ellipseP, polyP, blobP, shade, curve, rgbaHex, tone };

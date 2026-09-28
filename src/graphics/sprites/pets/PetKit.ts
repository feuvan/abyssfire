/**
 * Ley-beast (灵兽) sheets.
 *
 * Pets are small creatures drawn with the same isometric 3/4 projection as
 * monsters (`ViewRig`: se = front 3/4, ne = back 3/4, sw / nw mirrored), so
 * `CharacterAnimator.resolveFacing` drives them exactly like a monster sheet.
 *
 * Sheet layout (view-major, like monster sheets): per view 20 frames —
 * idle 4 (loop), walk 6 (loop), attack 4, cast 4, hurt 2. Attack tracks end
 * on the contact pose (`PET_ATTACK_CONTACT` = 1, the monster convention);
 * cast tracks peak (the spell releases) on frame 2.
 *
 * Creatures are authored in pet-local 3D: origin on the ground under the
 * body, x forward, y down (negative is up), z lateral (+ = near/right side).
 * `Body3` collects cel-shaded solids (ellipsoids, capsules, polygons, flat
 * side-art planes), depth-sorts them for the view and paints them; the frame
 * then goes through the rig compositor (ink outline, rim light, glow).
 */
import type { EntityDrawer, PlayerView } from '../types';
import {
  CENTER_X,
  GROUND_Y,
  blobPath,
  capsulePath,
  cel,
  frameTime,
  glow,
  groundShadow,
  polyPath,
  renderRigFrame,
  rgba,
  tone,
  vec,
  type Tone,
  type V,
} from '../rig/Rig';
import { ViewRig, hull, type HumanView, type V3 } from '../rig/HumanView';
import { withAffine } from '../rig/MonsterView';
import { getCurrentZonePalette, standardOutlineBlur } from '../../ZonePalette';

export type PetStage = 0 | 1 | 2;
export type PetAction = 'idle' | 'walk' | 'attack' | 'cast' | 'hurt';

export const PET_ACTION_ORDER: readonly PetAction[] = ['idle', 'walk', 'attack', 'cast', 'hurt'];
export const PET_ACTION_FRAME_COUNTS: Readonly<Record<PetAction, number>> = {
  idle: 4,
  walk: 6,
  attack: 4,
  cast: 4,
  hurt: 2,
};
/** Frames per view in a pet sheet. */
export const PET_VIEW_FRAMES = PET_ACTION_ORDER.reduce((n, a) => n + PET_ACTION_FRAME_COUNTS[a], 0);
export const PET_VIEWS: readonly PlayerView[] = ['se', 'ne'];
export const PET_SHEET_FRAMES = PET_VIEW_FRAMES * PET_VIEWS.length;
/** Registered frame rates (fps) per action. */
export const PET_FRAME_RATES: Readonly<Record<PetAction, number>> = {
  idle: 6,
  walk: 10,
  attack: 12,
  cast: 10,
  hurt: 10,
};
/** AnimConfig.attackContact for pet sheets: the bite lands on the last attack frame. */
export const PET_ATTACK_CONTACT = 1;
/** Frame (0-based) of the cast animation on which the spell releases. */
export const PET_CAST_RELEASE_FRAME = 2;

/** Frame size in world px (before TEXTURE_SCALE); the same for every pet and stage. */
export const PET_FRAME_W = 56;
export const PET_FRAME_H = 56;
/** Authored pet units → frame units (a stage-0 pet stands ~half a goblin tall). */
export const PET_BASE_SCALE = 1.1;
/** Frame units the ground line sits above the rig ground, leaving room for auras below the feet. */
export const PET_GROUND_LIFT = 16;
/** Ground line in the 96-unit frame (sprite originY = PET_GROUND_Y / 96). */
export const PET_GROUND_Y = GROUND_Y - PET_GROUND_LIFT;

export const PET_IDS = [
  'pet_sprite',
  'pet_owl',
  'pet_storm_wolf',
  'pet_cat',
  'pet_jade_tortoise',
  'pet_dragon',
  'pet_phoenix',
  'pet_void_butterfly',
] as const;
export type PetId = (typeof PET_IDS)[number];

export const PET_STAGES: readonly PetStage[] = [0, 1, 2];

/** Texture key of a pet sheet: `beast_<id>`, `beast_<id>_e1`, `beast_<id>_e2`. */
export function petSheetKey(petId: string, stage: PetStage): string {
  return stage === 0 ? `beast_${petId}` : `beast_${petId}_e${stage}`;
}

/** Sheet frame range of an action in one view. */
export function petActionFrameRange(view: PlayerView, action: PetAction): { start: number; end: number } {
  let start = Math.max(0, PET_VIEWS.indexOf(view)) * PET_VIEW_FRAMES;
  for (const a of PET_ACTION_ORDER) {
    if (a === action) return { start, end: start + PET_ACTION_FRAME_COUNTS[a] - 1 };
    start += PET_ACTION_FRAME_COUNTS[a];
  }
  return { start: 0, end: 0 };
}

export function petTime(action: PetAction, frame: number): number {
  const n = PET_ACTION_FRAME_COUNTS[action];
  return frameTime(frame % n, n, action === 'idle' || action === 'walk');
}

// ── 3D creature painter ─────────────────────────────────────────────────

export function p3(x: number, y: number, z = 0): V3 {
  return { x, y, z };
}

/** Rotation of a local frame: pitch about z (+ = nose down), then yaw about y (+ = nose toward +z). */
export interface Orient {
  pitch?: number;
  yaw?: number;
  roll?: number;
}

function rot(o: Orient | undefined, v: V3): V3 {
  if (!o) return v;
  let { x, y, z } = v;
  if (o.roll) {
    // Roll about the forward (x) axis: + tips the top toward +z.
    const c = Math.cos(o.roll), s = Math.sin(o.roll);
    const ny = y * c - z * s;
    const nz = y * s + z * c;
    y = ny;
    z = nz;
  }
  if (o.pitch) {
    // + noses down: forward (+x) turns toward +y (down).
    const c = Math.cos(o.pitch), s = Math.sin(o.pitch);
    const nx = x * c - y * s;
    const ny = x * s + y * c;
    x = nx;
    y = ny;
  }
  if (o.yaw) {
    const c = Math.cos(o.yaw), s = Math.sin(o.yaw);
    const nx = x * c - z * s;
    const nz = x * s + z * c;
    x = nx;
    z = nz;
  }
  return { x, y, z };
}

export interface Ball {
  c: V3;
  r: V3;
  o?: Orient;
}

export interface SolidOpts {
  /** Depth bias (+ = drawn later). */
  bias?: number;
  band?: number;
  hi?: number;
  stroke?: number;
  /** Paint on top of the solid in the same depth slot (eyes, markings). */
  after?: () => void;
  /** Absolute sort depth instead of the solid's centre. */
  d?: number;
  alpha?: number;
}

export class Body3 {
  readonly rig: ViewRig;
  readonly view: HumanView;
  readonly front: boolean;
  readonly ctx: CanvasRenderingContext2D;
  private items: { d: number; draw: () => void }[] = [];

  constructor(ctx: CanvasRenderingContext2D, view: HumanView) {
    this.ctx = ctx;
    this.view = view;
    this.rig = new ViewRig(view, 0, vec(CENTER_X, GROUND_Y), CENTER_X);
    this.front = view === 'se';
  }

  /** Screen point of a local point. */
  p(q: V3): V {
    return this.rig.pt(CENTER_X + q.x, GROUND_Y + q.y, q.z);
  }

  pt(x: number, y: number, z = 0): V {
    return this.rig.pt(CENTER_X + x, GROUND_Y + y, z);
  }

  /** Depth toward the camera of a local point. */
  depth(q: V3): number {
    return this.rig.depth(CENTER_X + q.x, GROUND_Y + q.y, q.z);
  }

  /** How squarely a local normal faces the camera (−1…1). */
  facing(n: V3): number {
    return this.rig.facing(n.x, n.y, n.z);
  }

  add(d: number, draw: () => void): void {
    this.items.push({ d, draw });
  }

  /** Screen ellipse of an ellipsoid. */
  ellipse(b: Ball): { c: V; rx: number; ry: number; rot: number } {
    const axes = [p3(b.r.x, 0, 0), p3(0, b.r.y, 0), p3(0, 0, b.r.z)].map(a => {
      const w = rot(b.o, a);
      return this.rig.vec(w.x, w.y, w.z);
    });
    let sa = 0, sb = 0, sc = 0;
    for (const m of axes) {
      sa += m.x * m.x;
      sb += m.x * m.y;
      sc += m.y * m.y;
    }
    const mean = (sa + sc) / 2;
    const dev = Math.sqrt(((sa - sc) / 2) ** 2 + sb * sb);
    const th = 0.5 * Math.atan2(2 * sb, sa - sc);
    return { c: this.p(b.c), rx: Math.sqrt(mean + dev), ry: Math.sqrt(Math.max(0.0001, mean - dev)), rot: th };
  }

  /** Surface point of an ellipsoid along a local direction, and how much it faces the camera. */
  surface(b: Ball, dir: V3, lift = 0): { p: V; vis: number; q: V3 } {
    const l = Math.hypot(dir.x, dir.y, dir.z) || 1;
    const d = p3(dir.x / l, dir.y / l, dir.z / l);
    const off = rot(b.o, p3(d.x * (b.r.x + lift), d.y * (b.r.y + lift), d.z * (b.r.z + lift)));
    const q = p3(b.c.x + off.x, b.c.y + off.y, b.c.z + off.z);
    const n = rot(b.o, p3(d.x / b.r.x, d.y / b.r.y, d.z / b.r.z));
    return { p: this.p(q), vis: this.facing(n), q };
  }

  /** A point in a ball's local frame (x fwd, y down, z side), in pet space. */
  local(b: Ball, x: number, y: number, z: number): V3 {
    const off = rot(b.o, p3(x, y, z));
    return p3(b.c.x + off.x, b.c.y + off.y, b.c.z + off.z);
  }

  /** Paint an ellipsoid immediately (inside another part's slot). */
  ballNow(b: Ball, t: Tone, o: { band?: number; hi?: number; stroke?: number; alpha?: number } = {}): { c: V; rx: number; ry: number; rot: number } {
    const e = this.ellipse(b);
    this.withAlpha(o.alpha, () => {
      cel(this.ctx, () => this.ctx.ellipse(e.c.x, e.c.y, e.rx, e.ry, e.rot, 0, Math.PI * 2), t, { band: o.band ?? Math.min(e.rx, e.ry) * 0.38, hi: o.hi, stroke: o.stroke });
    });
    return e;
  }

  /** Paint a polygon through local points immediately. */
  polyNow(pts: readonly V3[], t: Tone, o: { band?: number; hi?: number; stroke?: number; smooth?: boolean; hull?: boolean } = {}): V[] {
    let scr = pts.map(q => this.p(q));
    if (o.hull) scr = hull(scr);
    cel(this.ctx, () => (o.smooth ? blobPath(this.ctx, scr) : polyPath(this.ctx, scr)), t, { band: o.band ?? 0.8, hi: o.hi, stroke: o.stroke });
    return scr;
  }

  /** Run `fn` clipped to a ball's screen ellipse. */
  clipBall(b: Ball, fn: () => void): void {
    const e = this.ellipse(b);
    this.ctx.save();
    this.ctx.beginPath();
    this.ctx.ellipse(e.c.x, e.c.y, e.rx, e.ry, e.rot, 0, Math.PI * 2);
    this.ctx.clip();
    fn();
    this.ctx.restore();
  }

  ball(b: Ball, t: Tone, o: SolidOpts = {}): void {
    this.add((o.d ?? this.depth(b.c)) + (o.bias ?? 0), () => {
      const e = this.ellipse(b);
      const band = o.band ?? Math.min(e.rx, e.ry) * 0.38;
      this.withAlpha(o.alpha, () => {
        cel(this.ctx, () => this.ctx.ellipse(e.c.x, e.c.y, e.rx, e.ry, e.rot, 0, Math.PI * 2), t, { band, hi: o.hi, stroke: o.stroke });
      });
      o.after?.();
    });
  }

  capsule(a: V3, b: V3, ra: number, rb: number, t: Tone, o: SolidOpts = {}): void {
    const mid = p3((a.x + b.x) / 2, (a.y + b.y) / 2, (a.z + b.z) / 2);
    this.add((o.d ?? this.depth(mid)) + (o.bias ?? 0), () => {
      const A = this.p(a), B = this.p(b);
      this.withAlpha(o.alpha, () => {
        cel(this.ctx, () => capsulePath(this.ctx, A, B, ra, rb), t, { band: o.band ?? Math.min(ra, rb) * 0.5, hi: o.hi, stroke: o.stroke });
      });
      o.after?.();
    });
  }

  /** Tube through several points (tails, necks). */
  tube(pts: readonly V3[], radii: readonly number[], t: Tone, o: SolidOpts = {}): void {
    let dsum = 0;
    for (const q of pts) dsum += this.depth(q);
    this.add((o.d ?? dsum / pts.length) + (o.bias ?? 0), () => {
      const scr = pts.map(q => this.p(q));
      this.withAlpha(o.alpha, () => {
        // Outline pass for all segments first so joints stay seamless.
        const path = (): void => {
          for (let i = 0; i + 1 < scr.length; i++) {
            capsulePath(this.ctx, scr[i], scr[i + 1], radii[Math.min(i, radii.length - 1)], radii[Math.min(i + 1, radii.length - 1)]);
          }
        };
        cel(this.ctx, path, t, { band: o.band ?? Math.min(...radii) * 0.5, hi: o.hi, stroke: o.stroke });
      });
      o.after?.();
    });
  }

  /** Cel-filled polygon (or its hull / smooth blob) through local points. */
  poly(pts: readonly V3[], t: Tone, o: SolidOpts & { hull?: boolean; smooth?: boolean } = {}): void {
    let dsum = 0;
    for (const q of pts) dsum += this.depth(q);
    this.add((o.d ?? dsum / pts.length) + (o.bias ?? 0), () => {
      let scr = pts.map(q => this.p(q));
      if (o.hull) scr = hull(scr);
      this.withAlpha(o.alpha, () => {
        cel(this.ctx, () => (o.smooth ? blobPath(this.ctx, scr) : polyPath(this.ctx, scr)), t, { band: o.band ?? 0.8, hi: o.hi, stroke: o.stroke });
      });
      o.after?.();
    });
  }

  /**
   * Flat side art in its own plane: `fn` draws in 2D with +x along `u` and
   * +y along `v` (both local 3D vectors, per unit), origin at `origin`.
   */
  plane(origin: V3, u: V3, v: V3, fn: (ctx: CanvasRenderingContext2D) => void, o: SolidOpts & { reach?: number } = {}): void {
    const reach = o.reach ?? 6;
    const dMid = this.depth(p3(origin.x + u.x * reach, origin.y + u.y * reach, origin.z + u.z * reach));
    this.add((o.d ?? dMid) + (o.bias ?? 0), () => {
      this.withAlpha(o.alpha, () => {
        withAffine(this.ctx, q => this.p(p3(
          origin.x + u.x * q.x + v.x * q.y,
          origin.y + u.y * q.x + v.y * q.y,
          origin.z + u.z * q.x + v.z * q.y,
        )), () => fn(this.ctx));
      });
      o.after?.();
    });
  }

  /** Side art on the sagittal plane at lateral offset z (x forward, y down, pet space). */
  side(z: number, fn: (ctx: CanvasRenderingContext2D) => void, o: SolidOpts & { at?: V3 } = {}): void {
    const at = o.at ?? p3(0, -10, z);
    this.plane(p3(0, 0, z), p3(1, 0, 0), p3(0, 1, 0), fn, { ...o, d: o.d ?? this.depth(p3(at.x, at.y, z)) });
  }

  /** Eye decal on a ball; returns false when it faces away. */
  eye(b: Ball, dir: V3, r: number, opts: { iris: string; sclera?: string; pupil?: string; glint?: boolean; slit?: boolean; lid?: number; minVis?: number }): boolean {
    const s = this.surface(b, dir, 0.05);
    if (s.vis < (opts.minVis ?? 0.05)) return false;
    const ctx = this.ctx;
    const squash = Math.max(0.35, Math.min(1, s.vis * 1.4));
    const lid = opts.lid ?? 1;
    ctx.save();
    ctx.translate(s.p.x, s.p.y);
    ctx.scale(squash, lid);
    if (opts.sclera) {
      ctx.fillStyle = opts.sclera;
      ctx.beginPath();
      ctx.ellipse(0, 0, r * 1.18, r * 1.25, 0, 0, Math.PI * 2);
      ctx.fill();
    }
    ctx.fillStyle = opts.iris;
    ctx.beginPath();
    ctx.ellipse(0, 0, r, r * 1.12, 0, 0, Math.PI * 2);
    ctx.fill();
    ctx.strokeStyle = 'rgba(20,10,30,0.8)';
    ctx.lineWidth = 0.35;
    ctx.stroke();
    if (opts.pupil) {
      ctx.fillStyle = opts.pupil;
      ctx.beginPath();
      if (opts.slit) ctx.ellipse(0.1, 0.1, r * 0.22, r * 0.85, 0, 0, Math.PI * 2);
      else ctx.ellipse(0.15 * r, 0.1 * r, r * 0.52, r * 0.6, 0, 0, Math.PI * 2);
      ctx.fill();
    }
    if (opts.glint !== false) {
      ctx.fillStyle = 'rgba(255,255,255,0.95)';
      ctx.beginPath();
      ctx.arc(-r * 0.32, -r * 0.4, r * 0.34, 0, Math.PI * 2);
      ctx.fill();
    }
    ctx.restore();
    return true;
  }

  private withAlpha(a: number | undefined, fn: () => void): void {
    if (a === undefined || a >= 1) {
      fn();
      return;
    }
    this.ctx.save();
    this.ctx.globalAlpha *= a;
    fn();
    this.ctx.restore();
  }

  flush(): void {
    this.items.sort((a, b) => a.d - b.d);
    for (const it of this.items) it.draw();
    this.items = [];
  }
}

/**
 * A wing (or fin, ear flap) in its own plane hinged at `anchor`: `paint`
 * draws in 2D with +x along the span and +y toward the tail. `spread` swings
 * the span from straight up (0) out to the side (π/2) and below (> π/2);
 * `sweep` folds it back toward the tail.
 */
export function wing(
  b: Body3, anchor: V3, side: 1 | -1, spread: number, sweep: number,
  paint: (ctx: CanvasRenderingContext2D) => void, o: SolidOpts & { reach?: number } = {},
): void {
  const cs = Math.cos(spread), ss = Math.sin(spread), cw = Math.cos(sweep), sw = Math.sin(sweep);
  const u = p3(-sw, -cs * cw, side * ss * cw);
  const v = p3(-cw, sw * 0.3 + 0.12, 0);
  b.plane(anchor, u, v, paint, { reach: 6, ...o });
}

// ── Shared looks ────────────────────────────────────────────────────────

const toneCache = new Map<string, Tone>();
/** Cached tone(). */
export function tn(c: number, light = 0.34, shadow = 0.4): Tone {
  const key = `${c}:${light}:${shadow}`;
  let t = toneCache.get(key);
  if (!t) {
    t = tone(c, { light, shadow });
    toneCache.set(key, t);
  }
  return t;
}

/** Deterministic 0..1 hash. */
export function h01(i: number): number {
  const s = Math.sin(i * 127.1 + 311.7) * 43758.5453;
  return s - Math.floor(s);
}

/** Sparkle: a four-point star. */
export function star(ctx: CanvasRenderingContext2D, c: V, r: number, color: string): void {
  ctx.fillStyle = color;
  ctx.beginPath();
  ctx.moveTo(c.x, c.y - r);
  ctx.quadraticCurveTo(c.x + r * 0.18, c.y - r * 0.18, c.x + r, c.y);
  ctx.quadraticCurveTo(c.x + r * 0.18, c.y + r * 0.18, c.x, c.y + r);
  ctx.quadraticCurveTo(c.x - r * 0.18, c.y + r * 0.18, c.x - r, c.y);
  ctx.quadraticCurveTo(c.x - r * 0.18, c.y - r * 0.18, c.x, c.y - r);
  ctx.fill();
}

/**
 * Evolution aura under the pet: stage 1 a soft pool of light and a thin rune
 * ring, stage 2 a brighter double ring with rune ticks.
 */
export function stageAuraUnder(ctx: CanvasRenderingContext2D, stage: PetStage, color: number, t: number, r: number): void {
  if (stage === 0) return;
  const gy = GROUND_Y + 0.5;
  ctx.save();
  ctx.translate(CENTER_X, gy);
  ctx.scale(1, 0.5);
  const pulse = 0.85 + Math.sin(t * Math.PI * 2) * 0.15;
  const R = r * (stage === 2 ? 1.25 : 1.05);
  const g = ctx.createRadialGradient(0, 0, 0, 0, 0, R);
  g.addColorStop(0, rgba(color, (stage === 2 ? 0.42 : 0.28) * pulse));
  g.addColorStop(0.7, rgba(color, (stage === 2 ? 0.16 : 0.1) * pulse));
  g.addColorStop(1, rgba(color, 0));
  ctx.fillStyle = g;
  ctx.beginPath();
  ctx.arc(0, 0, R, 0, Math.PI * 2);
  ctx.fill();
  ctx.strokeStyle = rgba(color, stage === 2 ? 0.8 : 0.5);
  ctx.lineWidth = stage === 2 ? 1.4 : 1;
  ctx.beginPath();
  ctx.arc(0, 0, R * 0.78, 0, Math.PI * 2);
  ctx.stroke();
  if (stage === 2) {
    ctx.lineWidth = 0.8;
    ctx.beginPath();
    ctx.arc(0, 0, R * 0.6, 0, Math.PI * 2);
    ctx.stroke();
    // Rune ticks between the rings, slowly turning.
    const n = 10;
    for (let i = 0; i < n; i++) {
      const a = (i / n) * Math.PI * 2 + t * Math.PI * 2 / n;
      const c = Math.cos(a), s = Math.sin(a);
      ctx.beginPath();
      ctx.moveTo(c * R * 0.63, s * R * 0.63);
      ctx.lineTo(c * R * 0.75, s * R * 0.75);
      ctx.stroke();
    }
  }
  ctx.restore();
}

/** Rising motes around an evolved pet (drawn over the body). */
export function stageMotes(ctx: CanvasRenderingContext2D, stage: PetStage, color: number, t: number, spread: number, height: number, seed: number): void {
  if (stage === 0) return;
  const n = stage === 2 ? 7 : 4;
  for (let i = 0; i < n; i++) {
    const k = (h01(seed + i) + t) % 1;
    const x = CENTER_X + (h01(seed + i * 3.1) - 0.5) * spread * 2;
    const y = GROUND_Y - 2 - k * height;
    const a = Math.sin(k * Math.PI) * (stage === 2 ? 0.95 : 0.75);
    const r = 0.7 + h01(seed + i * 7.3) * (stage === 2 ? 1.1 : 0.7);
    glow(ctx, vec(x, y), r * 3, color, a * 0.5);
    star(ctx, vec(x, y), r * 1.3, rgba(0xffffff, a * 0.9));
  }
}

// ── Pet drawer ──────────────────────────────────────────────────────────

export interface PetDrawCtx<P> {
  b: Body3;
  p: P;
  action: PetAction;
  t: number;
  stage: PetStage;
  view: HumanView;
}

export interface PetSpec<P> {
  id: PetId;
  /** Hovering creature (wings/float); informational for the follower AI. */
  flyer: boolean;
  /** Figure scale per stage (1 = authored size). */
  scale: readonly [number, number, number];
  /** Aura colour for evolved stages. */
  aura: number;
  pose(action: PetAction, t: number, stage: PetStage): P;
  /** Add the creature's parts to the body painter. */
  build(d: PetDrawCtx<P>): void;
  /** Ground shadow: forward offset, radius, lift above ground (units). */
  shadow(p: P, stage: PetStage): { x: number; r: number; lift: number };
  /** Glows and particles over the inked body (unit space, same projection). */
  fx?(ctx: CanvasRenderingContext2D, d: PetDrawCtx<P>): void;
  ink?: string;
  rim?: string;
}

export interface PetDrawer extends EntityDrawer {
  readonly petId: PetId;
  readonly stage: PetStage;
  readonly flyer: boolean;
  /** Draw one frame of an action in a view (unit time). Used by icons. */
  drawPose(ctx: CanvasRenderingContext2D, action: PetAction, frame: number, w: number, h: number, view: HumanView): void;
}

export function definePet<P>(spec: PetSpec<P>, stage: PetStage): PetDrawer {
  const drawPose = (ctx: CanvasRenderingContext2D, action: PetAction, frame: number, w: number, h: number, view: HumanView): void => {
    const t = petTime(action, frame);
    const p = spec.pose(action, t, stage);
    const palette = getCurrentZonePalette();
    const sh = spec.shadow(p, stage);
    const k = spec.scale[stage] * PET_BASE_SCALE;
    let body: Body3 | null = null;
    const dctx = (c: CanvasRenderingContext2D): PetDrawCtx<P> => {
      body = new Body3(c, view);
      return { b: body, p, action, t, stage, view };
    };
    const lift = (c: CanvasRenderingContext2D): void => {
      c.translate(0, -PET_GROUND_LIFT / k);
    };
    renderRigFrame(
      ctx, w, h,
      c => {
        lift(c);
        const d = dctx(c);
        spec.build(d);
        d.b.flush();
      },
      {
        glowColor: palette.entityOutlineColor,
        glowBlur: standardOutlineBlur(w, h),
        scale: k,
        ink: spec.ink,
        rim: spec.rim,
        inkWidth: 1.5,
        rimWidth: 1.1,
      },
      c => {
        lift(c);
        stageAuraUnder(c, stage, spec.aura, t, 17);
        const gx = new Body3(c, view).pt(sh.x, 0, 0).x;
        groundShadow(c, gx, sh.r, sh.lift);
      },
      spec.fx || stage > 0
        ? c => {
          lift(c);
          if (spec.fx) {
            const b2 = new Body3(c, view);
            spec.fx(c, { b: b2, p, action, t, stage, view });
          }
          stageMotes(c, stage, spec.aura, t, 20, 36, spec.id.length * 13);
        }
        : undefined,
    );
    void body;
  };
  return {
    key: petSheetKey(spec.id, stage),
    frameW: PET_FRAME_W,
    frameH: PET_FRAME_H,
    totalFrames: PET_SHEET_FRAMES,
    inked: true,
    views: PET_VIEWS,
    petId: spec.id,
    stage,
    flyer: spec.flyer,
    drawPose,
    drawFrame(ctx, frame, action, w, h, _utils, view) {
      drawPose(ctx, action as PetAction, frame, w, h, (view ?? 'se') as HumanView);
    },
  };
}

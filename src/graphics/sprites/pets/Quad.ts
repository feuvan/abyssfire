/**
 * Small four-legged ley-beasts (wolf pup, shadow cat, lava drake, tortoise):
 * shared pose model, gait and a parametric body built on `Body3`. Each pet
 * supplies its proportions, palette and extra painters (marks, tail tips).
 */
import { solveIK, vec, type Tone, type V } from '../rig/Rig';
import type { V3 } from '../rig/HumanView';
import { Body3, p3, type Ball, type PetAction, type PetStage } from './PetKit';

export interface QuadPose {
  /** Body centre (x forward, y up negative). */
  bx: number;
  by: number;
  /** Body pitch (+ = nose down). */
  pitch: number;
  /** Head offset from its rest place, and pitch. */
  hx: number;
  hy: number;
  hp: number;
  /** Head turn toward the camera side (yaw). */
  hyaw: number;
  /** Mouth open 0..1. */
  jaw: number;
  /** Feet: forward offset from the leg root and lift, per leg (FN, FF, BN, BF). */
  feet: [V, V, V, V];
  /** Tail raise (rad, + up) and side wag (units). */
  tail: number;
  wag: number;
  /** Ear flatten 0 (pricked) … 1 (pinned back). */
  ears: number;
  /** Eyes 1 open … 0 shut. */
  lid: number;
  /** Effect intensity (claw smear / glow) 0..1. */
  fx: number;
  /** Cast glow 0..1. */
  glow: number;
}

export interface QuadBuild {
  /** Body ellipsoid radii (length, height, width halves). */
  body: V3;
  /** Height of the body centre at rest. */
  bodyY: number;
  /** Head ball radii and rest offset from the body centre. */
  head: V3;
  headAt: V;
  /** Leg segment lengths and thickness. */
  thigh: number;
  shin: number;
  legR: number;
  pawR: number;
  /** Lateral spread of the legs (fraction of body width). */
  legSpread: number;
  /** Leg roots along the body (fraction of body length). */
  frontAt: number;
  backAt: number;
}

export function quadRest(b: QuadBuild): QuadPose {
  const z = vec(0, 0);
  return {
    bx: 0, by: -b.bodyY, pitch: 0, hx: 0, hy: 0, hp: 0, hyaw: 0, jaw: 0,
    feet: [z, z, z, z], tail: 0.5, wag: 0, ears: 0, lid: 1, fx: 0, glow: 0,
  };
}

function lerp(a: number, b: number, k: number): number {
  return a + (b - a) * k;
}

export function lerpQuad(a: QuadPose, b: QuadPose, k: number): QuadPose {
  const out = { ...a } as QuadPose;
  for (const key of Object.keys(a) as (keyof QuadPose)[]) {
    if (key === 'feet') continue;
    (out[key] as number) = lerp(a[key] as number, b[key] as number, k);
  }
  out.feet = a.feet.map((f, i) => vec(lerp(f.x, b.feet[i].x, k), lerp(f.y, b.feet[i].y, k))) as QuadPose['feet'];
  return out;
}

/** Sample keyed quad poses (smoothstep between keys). */
export function quadTrack(keys: readonly [number, QuadPose][], t: number): QuadPose {
  if (t <= keys[0][0]) return keys[0][1];
  for (let i = 1; i < keys.length; i++) {
    if (t <= keys[i][0]) {
      const [a0, pa] = keys[i - 1];
      const [a1, pb] = keys[i];
      const u = (t - a0) / Math.max(1e-4, a1 - a0);
      return lerpQuad(pa, pb, u * u * (3 - 2 * u));
    }
  }
  return keys[keys.length - 1][1];
}

/** Trot gait: diagonal pairs swing together. */
export function quadWalk(rest: QuadPose, t: number, stride: number, lift: number, bob: number): QuadPose {
  const ph = t * Math.PI * 2;
  const foot = (phase: number): V => vec(Math.sin(ph + phase) * stride, Math.max(0, Math.cos(ph + phase)) * lift);
  return {
    ...rest,
    by: rest.by - Math.abs(Math.sin(ph)) * bob,
    pitch: rest.pitch + Math.sin(ph * 2) * 0.03,
    hy: rest.hy + Math.sin(ph * 2 + 0.6) * bob * 0.6,
    feet: [foot(0), foot(Math.PI), foot(Math.PI), foot(0)],
    tail: rest.tail + 0.1,
    wag: Math.sin(ph) * 1.6,
  };
}

export function quadIdle(rest: QuadPose, t: number): QuadPose {
  const ph = t * Math.PI * 2;
  return {
    ...rest,
    by: rest.by + Math.sin(ph) * 0.35,
    pitch: rest.pitch + Math.sin(ph) * 0.015,
    hy: rest.hy + Math.sin(ph - 0.7) * 0.5,
    hp: rest.hp + Math.sin(ph - 0.4) * 0.04,
    wag: Math.sin(ph * 2) * 1.2,
    lid: t > 0.7 && t < 0.8 ? 0.2 : rest.lid,
  };
}

export interface QuadSkin {
  fur: Tone;
  /** Far-side legs (slightly darker). */
  furFar: Tone;
  belly?: Tone;
  /** Body tone when it differs from the legs (a shell). */
  body?: Tone;
  paw?: Tone;
  /** Paint the head (ball given); runs in the head's depth slot. */
  head(b: Body3, head: Ball, p: QuadPose): void;
  /** Extra parts: ears, tail, marks (added to the painter). */
  extra?(b: Body3, g: QuadGeo, p: QuadPose): void;
  /** Painted on the body ball after its fill (markings). */
  bodyMarks?(b: Body3, body: Ball, p: QuadPose): void;
}

export interface QuadGeo {
  body: Ball;
  head: Ball;
  /** Root of the tail on the body (pet space). */
  tailRoot: V3;
  /** Leg roots (FN, FF, BN, BF). */
  roots: V3[];
  feet: V3[];
}

export function quadGeo(q: QuadBuild, p: QuadPose, front = true): QuadGeo {
  const body: Ball = { c: p3(p.bx, p.by, 0), r: q.body, o: { pitch: p.pitch } };
  const bodyAt = (x: number, y: number, z: number): V3 => {
    const c = Math.cos(p.pitch), s = Math.sin(p.pitch);
    return p3(p.bx + x * c - y * s, p.by + x * s + y * c, z);
  };
  const hc = bodyAt(q.headAt.x + p.hx, q.headAt.y + p.hy, 0);
  const head: Ball = { c: hc, r: q.head, o: { pitch: p.hp + p.pitch * 0.5, yaw: p.hyaw + (front ? 0.38 : 0) } };
  const zs = q.body.z * q.legSpread;
  const roots = [
    bodyAt(q.body.x * q.frontAt, q.body.y * 0.35, zs),
    bodyAt(q.body.x * q.frontAt, q.body.y * 0.35, -zs),
    bodyAt(-q.body.x * q.backAt, q.body.y * 0.35, zs),
    bodyAt(-q.body.x * q.backAt, q.body.y * 0.35, -zs),
  ];
  const restX = [q.body.x * q.frontAt, q.body.x * q.frontAt, -q.body.x * q.backAt, -q.body.x * q.backAt];
  const feet = p.feet.map((f, i) => p3(p.bx + restX[i] + f.x, -f.y, roots[i].z));
  return { body, head, tailRoot: bodyAt(-q.body.x * 0.9, -q.body.y * 0.25, 0), roots, feet };
}

/** Paint a quadruped: legs, body, head and the skin's extras, depth sorted. */
export function buildQuad(b: Body3, q: QuadBuild, skin: QuadSkin, p: QuadPose): QuadGeo {
  const g = quadGeo(q, p, b.front);
  const bodyD = b.depth(g.body.c);
  // Legs: far legs always behind the body, near legs in front of it.
  for (let i = 0; i < 4; i++) {
    const near = i % 2 === 0;
    const front = i < 2;
    const root = g.roots[i];
    const foot = g.feet[i];
    const ik = solveIK(vec(root.x, root.y), vec(foot.x, foot.y - q.pawR * 0.6), q.thigh, q.shin, front ? 1 : -1);
    const knee = p3(ik.mid.x, ik.mid.y, root.z);
    const ankle = p3(ik.end.x, ik.end.y, root.z);
    const t = near ? skin.fur : skin.furFar;
    const d = near ? bodyD + 0.4 + (front ? 0.1 : 0) : bodyD - 40 + (front ? 0.1 : 0);
    b.tube([root, knee, ankle], [q.legR * 1.3, q.legR, q.legR * 0.85], t, { d, stroke: 0 });
    b.ball({ c: p3(ankle.x + q.pawR * 0.35, ankle.y + q.pawR * 0.25, root.z), r: p3(q.pawR * 1.2, q.pawR * 0.75, q.pawR) }, skin.paw ?? t, { d: d + 0.02, stroke: 0.3 });
  }
  b.ball(g.body, skin.body ?? skin.fur, {
    d: bodyD,
    after: () => {
      if (skin.belly) {
        const e = b.ellipse({ c: b.local(g.body, q.body.x * 0.25, q.body.y * 0.42, 0), r: p3(q.body.x * 0.62, q.body.y * 0.55, q.body.z * 0.8), o: g.body.o });
        b.ctx.save();
        const be = b.ellipse(g.body);
        b.ctx.beginPath();
        b.ctx.ellipse(be.c.x, be.c.y, be.rx, be.ry, be.rot, 0, Math.PI * 2);
        b.ctx.clip();
        b.ctx.fillStyle = skin.belly.base;
        b.ctx.beginPath();
        b.ctx.ellipse(e.c.x, e.c.y, e.rx, e.ry, e.rot, 0, Math.PI * 2);
        b.ctx.fill();
        b.ctx.restore();
      }
      skin.bodyMarks?.(b, g.body, p);
    },
  });
  b.add(b.depth(g.head.c) + 1.5, () => skin.head(b, g.head, p));
  skin.extra?.(b, g, p);
  return g;
}

/** Tail chain from the root: rises by `raise`, curls by `curl`, wags sideways. */
export function tailPoints(root: V3, len: number, n: number, raise: number, curl: number, wag: number, droop = 0): V3[] {
  const pts: V3[] = [root];
  let ang = Math.PI + raise; // pointing back (−x), rotated up by raise
  let x = root.x, y = root.y;
  const seg = len / n;
  for (let i = 1; i <= n; i++) {
    const k = i / n;
    ang += curl / n;
    x += Math.cos(ang) * seg;
    y += Math.sin(ang) * seg + droop * k * seg * 0.3;
    pts.push(p3(x, y, root.z + wag * k * k));
  }
  return pts;
}

/** Standard beast tracks from a rest pose: pounce attack, cast (rear up / howl), hurt. */
export function quadActionPose(rest: QuadPose, action: PetAction, t: number, stage: PetStage, style: 'bite' | 'swipe' | 'snap' = 'bite'): QuadPose | null {
  void stage;
  const P = (o: Partial<QuadPose>): QuadPose => ({ ...rest, ...o });
  const f = (x: number, y = 0): V => vec(x, y);
  switch (action) {
    case 'attack': {
      if (style === 'snap') {
        // Tortoise: neck draws back, then snaps forward.
        return quadTrack([
          [0, rest],
          [0.33, P({ hx: -3, hy: 0.5, hp: -0.15, pitch: -0.04, fx: 0.2 })],
          [0.67, P({ hx: 3, hy: 1, hp: 0.1, jaw: 0.6, fx: 0.6 })],
          [1, P({ hx: 6.5, hy: 1.5, hp: 0.22, jaw: 1, bx: 1.2, fx: 1 })],
        ], t);
      }
      const crouch = P({ bx: -2.5, by: rest.by + 2.4, pitch: 0.12, hy: 1.4, hp: 0.15, ears: 0.7, tail: rest.tail - 0.3, feet: [f(1.5), f(1.5), f(1), f(1)], fx: 0.2 });
      const leap = P({ bx: 3.5, by: rest.by - 3.5, pitch: -0.16, hy: -1, hp: -0.2, jaw: 0.5, ears: 1, tail: rest.tail + 0.3, feet: [f(6, 4), f(5, 3.5), f(-4, 1.5), f(-4.5, 1)], fx: 0.6 });
      const hit = style === 'swipe'
        ? P({ bx: 6.5, by: rest.by - 0.5, pitch: 0.12, hy: 0.8, hp: 0.1, jaw: 0.35, ears: 1, tail: rest.tail + 0.5, feet: [f(10, 6), f(4, 0), f(-2.5), f(-3)], fx: 1 })
        : P({ bx: 7, by: rest.by + 0.6, pitch: 0.16, hy: 2, hp: 0.32, jaw: 1, ears: 1, tail: rest.tail + 0.4, feet: [f(6.5, 0), f(5.5, 0), f(-3), f(-3.5)], fx: 1 });
      return quadTrack([[0, rest], [0.33, crouch], [0.67, leap], [1, hit]], t);
    }
    case 'cast': {
      const gather = P({ bx: -1.2, by: rest.by + 1.2, pitch: -0.1, hy: -0.5, hp: -0.25, lid: 0.5, glow: 0.4, tail: rest.tail + 0.3 });
      const peak = P({ bx: -1.8, by: rest.by - 1.2, pitch: -0.26, hy: -2.5, hp: -0.6, jaw: 0.7, lid: 0.3, glow: 1, tail: rest.tail + 0.6, feet: [f(1.5, 2), f(1, 1.5), f(-1), f(-1)] });
      const settle = P({ bx: -1, by: rest.by - 0.5, pitch: -0.15, hy: -1.5, hp: -0.35, jaw: 0.3, lid: 0.7, glow: 0.7, tail: rest.tail + 0.4 });
      return quadTrack([[0, rest], [0.33, gather], [0.67, peak], [1, settle]], t);
    }
    case 'hurt': {
      const recoil = P({ bx: -3, by: rest.by + 0.5, pitch: -0.12, hy: -1.2, hp: -0.3, ears: 1, lid: 0.15, tail: rest.tail - 0.6, feet: [f(2, 1), f(1.5, 0.5), f(-1), f(-1)] });
      return quadTrack([[0, recoil], [1, P({ bx: -1.5, pitch: -0.05, hp: -0.12, ears: 0.6, lid: 0.6, tail: rest.tail - 0.3 })]], t);
    }
    default:
      return null;
  }
}

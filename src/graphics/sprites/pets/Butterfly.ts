// 虚空蝶 — a void butterfly: violet wings edged in night-black with pale
// lilac eye-spots and a scatter of stars, a slim dark body and antennae
// tipped with light. Awakened (stage 1) its wing edges glow; at stage 2 its
// hindwings trail long streamers and the wings hold a whole starfield.
import { glow, rgba, vec } from '../rig/Rig';
import { Body3, h01, p3, star, tn, wing, type Ball, type PetAction, type PetDrawCtx, type PetSpec, type PetStage } from './PetKit';
import { flyPose, type FlyPose, type FlyStyle } from './Fly';

const STYLE: FlyStyle = { hover: 19, flapMid: 0.75, flapAmp: -0.62, flapsIdle: 2, flapsWalk: 3, bob: 2.2 };

const WINGC = [0x7a4ad0, 0x8050e0, 0x8a50f0] as const;
const EDGE = [0x2a1650, 0x241250, 0x1a0e48] as const;
const SPOT = '#ecd8ff';

function pose(action: PetAction, t: number, stage: PetStage): FlyPose {
  const p = flyPose(STYLE, action, t, stage);
  // Butterflies keep the body level-ish and fold their wings up to strike.
  return { ...p, pitch: p.pitch * 0.5 };
}

function thorax(p: FlyPose): Ball {
  return { c: p3(p.bx, p.by, 0), r: p3(2.2, 2, 2), o: { pitch: p.pitch } };
}

function foreWing(ctx: CanvasRenderingContext2D, L: number, stage: PetStage, far: boolean): void {
  const t = tn(far ? 0x4a2a90 : WINGC[stage], 0.45);
  const path = (): void => {
    ctx.beginPath();
    ctx.moveTo(0, -0.8);
    ctx.quadraticCurveTo(L * 0.55, -L * 0.42, L * 1.02, -L * 0.36);
    ctx.quadraticCurveTo(L * 0.95, L * 0.05, L * 0.72, L * 0.3);
    ctx.quadraticCurveTo(L * 0.35, L * 0.28, 0, 1.4);
    ctx.closePath();
  };
  path();
  ctx.fillStyle = t.base;
  ctx.fill();
  ctx.save();
  path();
  ctx.clip();
  // Night-black border and a lighter inner flush.
  ctx.strokeStyle = rgba(EDGE[stage], 1);
  ctx.lineWidth = L * 0.16;
  path();
  ctx.stroke();
  ctx.fillStyle = t.light;
  ctx.beginPath();
  ctx.ellipse(L * 0.3, -L * 0.05, L * 0.28, L * 0.14, -0.4, 0, Math.PI * 2);
  ctx.fill();
  // Eye-spot and stars.
  ctx.fillStyle = SPOT;
  ctx.beginPath();
  ctx.ellipse(L * 0.72, -L * 0.14, L * 0.11, L * 0.09, 0, 0, Math.PI * 2);
  ctx.fill();
  ctx.fillStyle = rgba(EDGE[stage], 1);
  ctx.beginPath();
  ctx.arc(L * 0.72, -L * 0.14, L * 0.045, 0, Math.PI * 2);
  ctx.fill();
  const n = [3, 4, 8][stage];
  ctx.fillStyle = 'rgba(255,255,255,0.95)';
  for (let i = 0; i < n; i++) {
    ctx.beginPath();
    ctx.arc(L * (0.25 + h01(i + 3) * 0.7), L * (-0.3 + h01(i + 11) * 0.5), 0.35 + (i % 2) * 0.2, 0, Math.PI * 2);
    ctx.fill();
  }
  ctx.restore();
  path();
  ctx.strokeStyle = stage > 0 ? 'rgba(210,170,255,0.95)' : t.line;
  ctx.lineWidth = stage > 0 ? 0.6 : 0.45;
  ctx.stroke();
}

function hindWing(ctx: CanvasRenderingContext2D, L: number, stage: PetStage, far: boolean): void {
  const t = tn(far ? 0x40247e : WINGC[stage], 0.4);
  const path = (): void => {
    ctx.beginPath();
    ctx.moveTo(0, 0.5);
    ctx.quadraticCurveTo(L * 0.6, L * 0.1, L * 0.66, L * 0.45);
    if (stage === 2) {
      ctx.quadraticCurveTo(L * 0.55, L * 0.7, L * 0.42, L * 0.78);
      ctx.quadraticCurveTo(L * 0.4, L * 1.2, L * 0.28, L * 1.45);
      ctx.quadraticCurveTo(L * 0.26, L * 1.05, L * 0.2, L * 0.75);
    } else {
      ctx.quadraticCurveTo(L * 0.52, L * 0.78, L * 0.2, L * 0.7);
    }
    ctx.quadraticCurveTo(0, L * 0.4, 0, 2);
    ctx.closePath();
  };
  path();
  ctx.fillStyle = t.base;
  ctx.fill();
  ctx.save();
  path();
  ctx.clip();
  ctx.strokeStyle = rgba(EDGE[stage], 1);
  ctx.lineWidth = L * 0.14;
  path();
  ctx.stroke();
  ctx.fillStyle = SPOT;
  ctx.beginPath();
  ctx.ellipse(L * 0.42, L * 0.45, L * 0.09, L * 0.08, 0, 0, Math.PI * 2);
  ctx.fill();
  ctx.restore();
  path();
  ctx.strokeStyle = stage > 0 ? 'rgba(210,170,255,0.95)' : t.line;
  ctx.lineWidth = stage > 0 ? 0.6 : 0.45;
  ctx.stroke();
}

function build(d: PetDrawCtx<FlyPose>): void {
  const { b, p, stage } = d;
  const th = thorax(p);
  const L = [12, 13.5, 15][stage];
  const dark = tn(0x2a1c40, 0.4);
  for (const s of [1, -1] as const) {
    const spread = s > 0 ? p.wn : p.wf;
    const anchor = b.local(th, 0.2, -1.2, s * 0.8);
    wing(b, anchor, s, spread + 0.12, 0.1 + p.sweep * 0.3, ctx => hindWing(ctx, L * 0.85, stage, s < 0), { bias: s > 0 ? 0.1 : -20.1, reach: 4 });
    wing(b, anchor, s, spread, -0.05 + p.sweep * 0.3, ctx => foreWing(ctx, L, stage, s < 0), { bias: s > 0 ? 0.2 : -20, reach: 4 });
  }
  // Abdomen, thorax, head.
  const abd = [b.local(th, -1, 0.4, 0), b.local(th, -4, 1.4, 0), b.local(th, -7, 2.8 - p.tail * 2, 0)];
  b.tube(abd, [1.7, 1.4, 0.7], dark, { stroke: 0, band: 0.5 });
  b.ball(th, dark, { band: 0.8 });
  const head: Ball = { c: b.local(th, 2.8, -0.6, 0), r: p3(1.6, 1.5, 1.6), o: { yaw: b.front ? 0.5 : 0 } };
  b.ball(head, dark, {
    band: 0.5,
    bias: 0.5,
    after: () => {
      for (const s of [1, -1]) b.eye(head, p3(0.6, 0, s * 0.7), 0.7, { iris: '#d8b0ff', glint: true });
    },
  });
  // Antennae.
  b.add(b.depth(head.c) + 0.6, () => {
    for (const s of [1, -1]) {
      const a = b.p(b.local(head, 0.4, -1.2, s * 0.6));
      const tip = b.p(b.local(head, 3.5, -6 - p.glow * 1.5, s * 2.8));
      b.ctx.strokeStyle = '#1a1030';
      b.ctx.lineWidth = 0.45;
      b.ctx.beginPath();
      b.ctx.moveTo(a.x, a.y);
      b.ctx.quadraticCurveTo(a.x + (tip.x - a.x) * 0.2, tip.y + 1, tip.x, tip.y);
      b.ctx.stroke();
      b.ctx.fillStyle = '#e8d0ff';
      b.ctx.beginPath();
      b.ctx.arc(tip.x, tip.y, 0.7, 0, Math.PI * 2);
      b.ctx.fill();
    }
  });
}

export const ButterflySpec: PetSpec<FlyPose> = {
  id: 'pet_void_butterfly',
  flyer: true,
  scale: [1.15, 1.3, 1.46],
  aura: 0xa070ff,
  pose,
  build,
  shadow: p => ({ x: p.bx, r: 9, lift: Math.max(0, -p.by - 6) }),
  fx: (ctx, d) => {
    const { b, p, stage, action, t } = d;
    const th = thorax(p);
    const c = b.p(th.c);
    glow(ctx, c, 12 + stage * 2, 0xa070ff, 0.28 + stage * 0.06);
    // Antenna tip glow.
    for (const s of [1, -1]) {
      const head: Ball = { c: b.local(th, 2.8, -0.6, 0), r: p3(1.6, 1.5, 1.6) };
      glow(ctx, b.p(b.local(head, 3.5, -6 - p.glow * 1.5, s * 2.8)), 2.5, 0xe0c0ff, 0.6);
    }
    // Void dust falling from the wings.
    const n = 3 + stage * 2;
    for (let i = 0; i < n; i++) {
      const k = (t + i / n) % 1;
      const q = b.pt(p.bx - 4 + h01(i) * 8, p.by + 2 + k * 14, (h01(i + 5) - 0.5) * 12);
      ctx.fillStyle = `rgba(${stage === 2 ? '220,200,255' : '190,150,255'},${0.8 * (1 - k)})`;
      ctx.beginPath();
      ctx.arc(q.x, q.y, 0.55, 0, Math.PI * 2);
      ctx.fill();
    }
    if (action === 'cast' && p.glow > 0.05) {
      // Void orb gathering between the wings.
      const o = vec(c.x + 2, c.y - 8 - p.glow * 3);
      glow(ctx, o, 10 * p.glow, 0x8a50ff, 0.6 * p.glow);
      ctx.fillStyle = `rgba(30,10,60,${0.9 * p.glow})`;
      ctx.beginPath();
      ctx.arc(o.x, o.y, 2.6 * p.glow, 0, Math.PI * 2);
      ctx.fill();
      ctx.strokeStyle = `rgba(210,170,255,${p.glow})`;
      ctx.lineWidth = 0.7;
      ctx.stroke();
      for (let i = 0; i < 4; i++) {
        const a = (i / 4) * Math.PI * 2 + t * 3;
        star(ctx, vec(o.x + Math.cos(a) * 6 * p.glow, o.y + Math.sin(a) * 3.5 * p.glow), 1, `rgba(235,220,255,${p.glow})`);
      }
    }
    if (action === 'attack' && t > 0.9) {
      const m = b.p(b.local(th, 9, 2, 0));
      glow(ctx, m, 7, 0xb080ff, 0.65);
      star(ctx, m, 3, 'rgba(240,225,255,0.95)');
    }
  },
  ink: '#140a26',
  rim: 'rgba(225,200,255,0.75)',
};

// 月鸮 — a silver-blue moon owl: round body, pale heart-shaped facial disc,
// big moonlit eyes, ear tufts and soft barred wings. Awakened (stage 1) a
// crescent glows on its brow and stars speckle the wings; at stage 2 long
// silver tufts, star-tipped primaries and a crescent halo.
import { glow, rgba, vec } from '../rig/Rig';
import { Body3, p3, star, tn, wing, type Ball, type PetAction, type PetDrawCtx, type PetSpec, type PetStage } from './PetKit';
import { flyPose, type FlyPose, type FlyStyle } from './Fly';

const STYLE: FlyStyle = { hover: 21, flapMid: 1.05, flapAmp: 0.55, flapsIdle: 1, flapsWalk: 2, bob: 1.6 };

const BODY = [0xa9b6ce, 0xacbcd8, 0xb8c8e6] as const;
const FAR = [0x6c7a96, 0x6a7ca0, 0x7084ac] as const;
const WING = [0x8b9cbe, 0x889ec8, 0x92aad6] as const;
const PRIM = [0x4a5676, 0x44567e, 0x3e5288] as const;
const DISC = 0xeef2fa;
const EYE = ['#8fdcff', '#9fe6ff', '#c2f2ff'] as const;

function pose(action: PetAction, t: number, stage: PetStage): FlyPose {
  const p = flyPose(STYLE, action, t, stage);
  return { ...p, talon: action === 'attack' ? p.talon : 0.1 };
}

function bodyBall(p: FlyPose): Ball {
  return { c: p3(p.bx, p.by, 0), r: p3(5.6, 6.6, 5.4), o: { pitch: p.pitch * 0.6 } };
}

function headBall(b: Body3, body: Ball, p: FlyPose): Ball {
  return { c: b.local(body, 1 + p.hx, -7.6 + p.hy, 0), r: p3(5.4, 5, 5.8), o: { pitch: p.hp + p.pitch * 0.3, yaw: b.front ? 0.6 : 0.15 } };
}

function wingArt(ctx: CanvasRenderingContext2D, L: number, stage: PetStage, far: boolean): void {
  const base = tn(far ? FAR[stage] : WING[stage], 0.4);
  const prim = tn(PRIM[stage], 0.3);
  // Primaries (dark fingered tips) behind the covert.
  ctx.beginPath();
  ctx.moveTo(L * 0.35, -1.6);
  ctx.quadraticCurveTo(L * 0.85, -2.4, L * 1.05, 0.6);
  for (let i = 0; i < 4; i++) {
    const x = L * (1 - i * 0.14);
    ctx.quadraticCurveTo(x - 0.4, 3.6 + i * 0.6, x - L * 0.1, 2.2 + i * 0.5);
  }
  ctx.lineTo(L * 0.35, 3);
  ctx.closePath();
  ctx.fillStyle = prim.base;
  ctx.fill();
  ctx.strokeStyle = prim.line;
  ctx.lineWidth = 0.45;
  ctx.stroke();
  // Main wing (rounded, scalloped trailing edge).
  ctx.beginPath();
  ctx.moveTo(0, -1.2);
  ctx.quadraticCurveTo(L * 0.45, -2.6, L * 0.78, -1);
  ctx.quadraticCurveTo(L * 0.72, 1.6, L * 0.6, 2.6);
  for (let i = 0; i < 4; i++) {
    const x = L * (0.6 - i * 0.15);
    ctx.quadraticCurveTo(x - L * 0.04, 5.2, x - L * 0.15, 4.2 + i * 0.3);
  }
  ctx.lineTo(0, 4.6);
  ctx.closePath();
  ctx.fillStyle = base.base;
  ctx.fill();
  ctx.strokeStyle = base.line;
  ctx.lineWidth = 0.5;
  ctx.stroke();
  // Covert band and bars.
  ctx.fillStyle = base.light;
  ctx.beginPath();
  ctx.moveTo(0, -1);
  ctx.quadraticCurveTo(L * 0.45, -2.3, L * 0.74, -0.8);
  ctx.quadraticCurveTo(L * 0.4, 0.4, 0, 0.8);
  ctx.fill();
  ctx.strokeStyle = base.shade;
  ctx.lineWidth = 0.4;
  for (let i = 1; i < 4; i++) {
    ctx.beginPath();
    ctx.moveTo(L * 0.12 * i, 1.4);
    ctx.lineTo(L * 0.12 * i + 1, 3.6);
    ctx.stroke();
  }
  if (stage > 0) {
    ctx.fillStyle = 'rgba(230,246,255,0.95)';
    for (const [x, y] of [[0.3, 2], [0.55, 0.6], [0.45, 3.2]] as const) {
      ctx.beginPath();
      ctx.arc(L * x, y, 0.45, 0, Math.PI * 2);
      ctx.fill();
    }
  }
}

function build(d: PetDrawCtx<FlyPose>): void {
  const { b, p, stage } = d;
  const body = bodyBall(p);
  const head = headBall(b, body, p);
  const L = [11, 12.5, 14][stage];
  // Wings.
  for (const s of [1, -1] as const) {
    const anchor = b.local(body, 0.5, -3, s * 4.2);
    wing(b, anchor, s, s > 0 ? p.wn : p.wf, 0.25 + p.sweep * 0.5, ctx => wingArt(ctx, L, stage, s < 0), { bias: s > 0 ? 0.5 : -20, reach: 4 });
  }
  // Tail fan.
  const tail = [b.local(body, -4, 3.5, -2.2), b.local(body, -8.5, 5 - p.tail * 3, -3), b.local(body, -9, 6 - p.tail * 3, 0), b.local(body, -8.5, 5 - p.tail * 3, 3), b.local(body, -4, 3.5, 2.2)];
  b.poly(tail, tn(WING[stage], 0.35), { band: 0.7, smooth: true, d: b.depth(b.local(body, -6, 4, 0)) - 2 });
  // Talons.
  for (const s of [1, -1]) {
    const hip = b.local(body, 1, 5, s * 2);
    const foot = p3(hip.x + 1 + p.talon * 5, hip.y + 3.4 - p.talon * 1.2, hip.z);
    b.capsule(hip, foot, 1.2, 0.8, tn(0xd8c890, 0.35), { stroke: 0, bias: s > 0 ? 0.3 : -15 });
    b.add(b.depth(foot) + (s > 0 ? 0.5 : -15), () => {
      const q = b.p(foot);
      b.ctx.strokeStyle = '#3a2a18';
      b.ctx.lineWidth = 0.55;
      b.ctx.lineCap = 'round';
      for (const dx of [-0.8, 0, 0.8]) {
        b.ctx.beginPath();
        b.ctx.moveTo(q.x + dx, q.y);
        b.ctx.quadraticCurveTo(q.x + dx + 0.9, q.y + 0.4, q.x + dx + 0.8, q.y + 1.4);
        b.ctx.stroke();
      }
    });
  }
  // Body with a pale chest and V-bars.
  b.ball(body, tn(BODY[stage], 0.42), {
    after: () => {
      b.clipBall(body, () => {
        const ch = b.ellipse({ c: b.local(body, 3.2, 1.2, 0), r: p3(3.6, 5, 4.4), o: body.o });
        if (b.surface(body, p3(1, 0.2, 0)).vis > -0.1) {
          b.ctx.fillStyle = tn(DISC).base;
          b.ctx.beginPath();
          b.ctx.ellipse(ch.c.x, ch.c.y, ch.rx, ch.ry, ch.rot, 0, Math.PI * 2);
          b.ctx.fill();
          b.ctx.strokeStyle = rgba(0x7a88a8, 0.8);
          b.ctx.lineWidth = 0.45;
          for (const [y, z] of [[-0.1, 0.35], [0.25, -0.2], [0.5, 0.3], [0.15, 0.7], [0.55, -0.55]] as const) {
            const q = b.surface(body, p3(0.9, y, z));
            if (q.vis < 0) continue;
            b.ctx.beginPath();
            b.ctx.moveTo(q.p.x - 0.8, q.p.y - 0.5);
            b.ctx.lineTo(q.p.x, q.p.y + 0.3);
            b.ctx.lineTo(q.p.x + 0.8, q.p.y - 0.5);
            b.ctx.stroke();
          }
        }
      });
    },
  });
  // Ear tufts.
  const tuftLen = [3.6, 4.2, 6][stage];
  for (const s of [1, -1]) {
    b.poly([b.local(head, 0.8, -3.4, s * 2.4), b.local(head, -1.6, -3.6, s * 4.2), b.local(head, -1.2, -3.8 - tuftLen, s * 4.8)], s > 0 ? tn(BODY[stage], 0.4) : tn(FAR[stage], 0.3), { band: 0.6, bias: s > 0 ? 2 : -2, d: b.depth(head.c) + s * 2 });
  }
  // Head with facial disc, eyes, beak.
  b.ball(head, tn(BODY[stage], 0.42), {
    bias: 1.5,
    after: () => {
      const front = b.surface(head, p3(1, 0.1, 0));
      if (front.vis > -0.25) {
        b.clipBall(head, () => {
          const f = b.ellipse({ c: b.local(head, 3.6, 0.6, 0), r: p3(2.4, 3.6, 4.6), o: head.o });
          const t2 = tn(DISC);
          b.ctx.fillStyle = t2.base;
          b.ctx.beginPath();
          b.ctx.ellipse(f.c.x, f.c.y, f.rx, f.ry, f.rot, 0, Math.PI * 2);
          b.ctx.fill();
          b.ctx.strokeStyle = rgba(0x6a78a0, 0.9);
          b.ctx.lineWidth = 0.5;
          b.ctx.stroke();
          // Heart notch between the brows.
          const n = b.surface(head, p3(0.72, -0.62, 0), 0.1);
          b.ctx.fillStyle = tn(BODY[stage]).base;
          b.ctx.beginPath();
          b.ctx.moveTo(n.p.x - 1.4, n.p.y - 0.8);
          b.ctx.lineTo(n.p.x + 1.4, n.p.y - 0.8);
          b.ctx.lineTo(n.p.x, n.p.y + 1.4);
          b.ctx.fill();
        });
        for (const s of [1, -1]) b.eye(head, p3(0.8, 0.02, s * 0.46), 1.6, { iris: EYE[stage], pupil: '#0e1628', lid: p.lid, sclera: '#10182c' });
        const bk = b.surface(head, p3(1, 0.42, 0), 0.2);
        if (bk.vis > -0.1) {
          b.ctx.fillStyle = '#e8c060';
          b.ctx.strokeStyle = '#6a4a18';
          b.ctx.lineWidth = 0.4;
          b.ctx.beginPath();
          b.ctx.moveTo(bk.p.x - 1, bk.p.y - 1.2);
          b.ctx.lineTo(bk.p.x + 1, bk.p.y - 1.2);
          b.ctx.lineTo(bk.p.x + 0.2, bk.p.y + 1.4 + p.beak * 0.6);
          b.ctx.closePath();
          b.ctx.fill();
          b.ctx.stroke();
        }
      }
      if (stage > 0) {
        const c = b.surface(head, p3(0.55, -0.83, 0));
        if (c.vis > -0.2) {
          b.ctx.fillStyle = 'rgba(240,250,255,0.98)';
          b.ctx.beginPath();
          b.ctx.arc(c.p.x, c.p.y, 1.5, 0, Math.PI * 2);
          b.ctx.arc(c.p.x + 0.7, c.p.y - 0.5, 1.25, 0, Math.PI * 2, true);
          b.ctx.fill('evenodd');
        }
      }
    },
  });
}

export const OwlSpec: PetSpec<FlyPose> = {
  id: 'pet_owl',
  flyer: true,
  scale: [1, 1.14, 1.3],
  aura: 0x9fd8ff,
  pose,
  build,
  shadow: p => ({ x: p.bx, r: 9, lift: Math.max(0, -p.by - 6) }),
  fx: (ctx, d) => {
    const { b, p, stage, action, t } = d;
    const body = bodyBall(p);
    const head = headBall(b, body, p);
    for (const s of [1, -1]) {
      const e = b.surface(head, p3(0.8, 0.02, s * 0.46));
      if (e.vis > 0.1) glow(ctx, e.p, 3 + stage * 0.6, 0x9fe0ff, 0.3 + stage * 0.1);
    }
    if (stage > 0) {
      const c = b.surface(head, p3(0.55, -0.83, 0));
      glow(ctx, c.p, 4 + stage, 0xdff4ff, 0.5);
    }
    if (stage === 2) {
      // Crescent halo behind the head.
      const hc = b.p(b.local(head, -2, -6.5, 0));
      ctx.save();
      ctx.strokeStyle = 'rgba(220,240,255,0.6)';
      ctx.lineWidth = 0.8;
      ctx.beginPath();
      ctx.ellipse(hc.x, hc.y, 6, 2.2, -0.15, Math.PI * 0.1, Math.PI * 1.25);
      ctx.stroke();
      ctx.restore();
      glow(ctx, hc, 7, 0xbfe6ff, 0.3);
      // Star-tipped primaries.
      for (const s of [1, -1] as const) {
        const L = 14;
        const sp = s > 0 ? p.wn : p.wf;
        const cs = Math.cos(sp), ss = Math.sin(sp), sw = Math.sin(0.25 + p.sweep * 0.5), cw = Math.cos(0.25 + p.sweep * 0.5);
        const a = b.local(body, 0.5, -3, s * 4.2);
        const tip = b.p(p3(a.x - sw * L - cw * 1, a.y - cs * cw * L + 0.3, a.z + s * ss * cw * L));
        glow(ctx, tip, 3, 0xcfeaff, 0.6);
        star(ctx, tip, 1.5, 'rgba(255,255,255,0.95)');
      }
    }
    if (action === 'cast' && p.glow > 0.05) {
      const c = b.p(b.local(body, 2, -1, 0));
      glow(ctx, c, 16 * p.glow, 0xbfe6ff, 0.45 * p.glow);
      ctx.save();
      ctx.globalAlpha = p.glow;
      ctx.fillStyle = 'rgba(240,250,255,0.95)';
      ctx.beginPath();
      ctx.arc(c.x, c.y - 16, 3.6, 0, Math.PI * 2);
      ctx.arc(c.x + 1.6, c.y - 17, 3, 0, Math.PI * 2, true);
      ctx.fill('evenodd');
      ctx.restore();
    }
    if (action === 'attack' && t > 0.9) {
      const m = b.p(b.local(body, 4.5, 7, 0));
      glow(ctx, m, 7, 0xcfeaff, 0.5);
      ctx.strokeStyle = 'rgba(235,248,255,0.9)';
      ctx.lineWidth = 0.9;
      for (let i = 0; i < 3; i++) {
        ctx.beginPath();
        ctx.moveTo(m.x - 1 + i * 2, m.y - 5 + i);
        ctx.lineTo(m.x + 4 + i * 2, m.y + 2 + i);
        ctx.stroke();
      }
    }
    void vec;
  },
  ink: '#161c30',
  rim: 'rgba(230,244,255,0.7)',
};

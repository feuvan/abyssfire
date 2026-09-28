// 赫莉娅之烬 — the ember chick Helia left behind: a round, downy firebird
// chick in gold and flame orange with a flickering crest, stubby flame-edged
// wings and a little tail of fire. Awakened (stage 1) long tail plumes with
// eye-spots; stage 2 a full blazing crest and a golden sun halo.
import { glow, vec } from '../rig/Rig';
import { Body3, p3, tn, wing, type Ball, type PetAction, type PetDrawCtx, type PetSpec, type PetStage } from './PetKit';
import { flyPose, type FlyPose, type FlyStyle } from './Fly';
import { drawFlame } from '../decorations/CampProps';

const STYLE: FlyStyle = { hover: 17, flapMid: 1.15, flapAmp: 0.65, flapsIdle: 2, flapsWalk: 3, bob: 1.4 };

const DOWN = [0xffa63a, 0xffac36, 0xffb834] as const;
const BREAST = [0xffe08a, 0xffe69a, 0xfff0b0] as const;
const WINGC = [0xf0702a, 0xf26a24, 0xf45a20] as const;

function pose(action: PetAction, t: number, stage: PetStage): FlyPose {
  return flyPose(STYLE, action, t, stage);
}

function bodyBall(p: FlyPose): Ball {
  return { c: p3(p.bx, p.by, 0), r: p3(6.2, 6, 5.8), o: { pitch: p.pitch * 0.5 } };
}

function headBall(b: Body3, body: Ball, p: FlyPose): Ball {
  return { c: b.local(body, 2.6 + p.hx, -6.4 + p.hy, 0), r: p3(4.6, 4.4, 4.5), o: { pitch: p.hp, yaw: b.front ? 0.5 : 0.1 } };
}

function wingArt(ctx: CanvasRenderingContext2D, L: number, stage: PetStage, far: boolean, ph: number): void {
  const t = tn(far ? 0xb84a1a : WINGC[stage], 0.45);
  // Flame-tongue primaries.
  for (let i = 0; i < 3; i++) {
    const x = L * (0.55 + i * 0.2);
    ctx.fillStyle = i % 2 ? '#ffd060' : '#ff9a30';
    ctx.beginPath();
    ctx.moveTo(x - 1.6, 0.5);
    ctx.quadraticCurveTo(x + 1 + Math.sin(ph + i) * 0.6, 3.5, x + 1.2, 6 + i * 0.4);
    ctx.quadraticCurveTo(x + 1.8, 2.8, x + 1.4, 0);
    ctx.fill();
  }
  ctx.beginPath();
  ctx.moveTo(0, -1);
  ctx.quadraticCurveTo(L * 0.6, -2.2, L, -0.4);
  ctx.quadraticCurveTo(L * 0.9, 2.6, L * 0.55, 3.6);
  ctx.quadraticCurveTo(L * 0.3, 4.6, 0, 3.6);
  ctx.closePath();
  ctx.fillStyle = t.base;
  ctx.fill();
  ctx.strokeStyle = t.line;
  ctx.lineWidth = 0.5;
  ctx.stroke();
  ctx.fillStyle = t.light;
  ctx.beginPath();
  ctx.moveTo(0, -0.8);
  ctx.quadraticCurveTo(L * 0.6, -1.9, L * 0.92, -0.3);
  ctx.quadraticCurveTo(L * 0.5, 0.8, 0, 0.8);
  ctx.fill();
}

function tailPlumes(b: Body3, body: Ball, p: FlyPose, stage: PetStage): { base: ReturnType<Body3['p']>; tip: ReturnType<Body3['p']>; len: number }[] {
  const n = stage === 0 ? 2 : 3;
  const len = [6, 12, 15][stage];
  const out = [];
  for (let i = 0; i < n; i++) {
    const z = (i - (n - 1) / 2) * 1.8;
    const a = b.local(body, -5.5, 2, z);
    const tip = b.local(body, -5.5 - len * 0.9, 2 + len * (0.25 - p.tail * 0.3) + i * 0.8, z * 1.8);
    out.push({ base: b.p(a), tip: b.p(tip), len });
  }
  return out;
}

function build(d: PetDrawCtx<FlyPose>): void {
  const { b, p, stage, t } = d;
  const body = bodyBall(p);
  const head = headBall(b, body, p);
  const L = [7, 8.5, 10][stage];
  for (const s of [1, -1] as const) {
    const anchor = b.local(body, 0.5, -2.5, s * 4.6);
    wing(b, anchor, s, s > 0 ? p.wn : p.wf, 0.35 + p.sweep * 0.5, ctx => wingArt(ctx, L, stage, s < 0, t * 6.28), { bias: s > 0 ? 0.5 : -20, reach: 3 });
  }
  // Tail flames / plumes (behind the body in the front view).
  b.add(b.depth(b.local(body, -6, 2, 0)) - 1, () => {
    for (const pl of tailPlumes(b, body, p, stage)) {
      const dx = pl.tip.x - pl.base.x, dy = pl.tip.y - pl.base.y;
      const l = Math.hypot(dx, dy) || 1;
      const nx = -dy / l, ny = dx / l;
      const w = stage === 0 ? 1.8 : 1.3;
      const tt = tn(stage === 0 ? 0xff8a2a : 0xf45a20, 0.45);
      b.ctx.fillStyle = tt.base;
      b.ctx.strokeStyle = tt.line;
      b.ctx.lineWidth = 0.4;
      b.ctx.beginPath();
      b.ctx.moveTo(pl.base.x + nx * w, pl.base.y + ny * w);
      b.ctx.quadraticCurveTo(pl.base.x + dx * 0.5 + nx * w * 1.2, pl.base.y + dy * 0.5 + ny * w * 1.2 + Math.sin(t * 6.28) * 0.6, pl.tip.x, pl.tip.y);
      b.ctx.quadraticCurveTo(pl.base.x + dx * 0.5 - nx * w, pl.base.y + dy * 0.5 - ny * w, pl.base.x - nx * w, pl.base.y - ny * w);
      b.ctx.closePath();
      b.ctx.fill();
      b.ctx.stroke();
      if (stage > 0) {
        // Eye-spot at the plume tip.
        b.ctx.fillStyle = '#ffe070';
        b.ctx.beginPath();
        b.ctx.ellipse(pl.tip.x - dx / l * 1.6, pl.tip.y - dy / l * 1.6, 1.5, 1.2, Math.atan2(dy, dx), 0, Math.PI * 2);
        b.ctx.fill();
        b.ctx.fillStyle = '#c8301a';
        b.ctx.beginPath();
        b.ctx.arc(pl.tip.x - dx / l * 1.6, pl.tip.y - dy / l * 1.6, 0.6, 0, Math.PI * 2);
        b.ctx.fill();
      }
    }
  });
  // Little legs.
  for (const s of [1, -1]) {
    const hip = b.local(body, 1, 5, s * 2);
    const foot = p3(hip.x + 0.8 + p.talon * 4, hip.y + 2.4 - p.talon, hip.z);
    b.capsule(hip, foot, 0.8, 0.6, tn(0xd06a2a, 0.3), { stroke: 0, bias: s > 0 ? 0.3 : -15 });
  }
  b.ball(body, tn(DOWN[stage], 0.45), {
    band: 2.2,
    after: () => {
      b.clipBall(body, () => {
        if (b.surface(body, p3(1, 0.3, 0)).vis > -0.2) {
          const ch = b.ellipse({ c: b.local(body, 3.8, 1.6, 0), r: p3(3.4, 4.4, 4.6), o: body.o });
          b.ctx.fillStyle = tn(BREAST[stage]).base;
          b.ctx.beginPath();
          b.ctx.ellipse(ch.c.x, ch.c.y, ch.rx, ch.ry, ch.rot, 0, Math.PI * 2);
          b.ctx.fill();
        }
        // Downy tufts.
        b.ctx.strokeStyle = tn(DOWN[stage]).shade;
        b.ctx.lineWidth = 0.45;
        for (const [x, y, z] of [[0.2, -0.4, 0.9], [-0.3, 0.1, 0.95], [-0.6, -0.3, 0.7]] as const) {
          const q = b.surface(body, p3(x, y, z));
          if (q.vis < 0) continue;
          b.ctx.beginPath();
          b.ctx.moveTo(q.p.x - 1, q.p.y);
          b.ctx.quadraticCurveTo(q.p.x, q.p.y + 1, q.p.x + 1, q.p.y);
          b.ctx.stroke();
        }
      });
    },
  });
  // Crest flames on the head.
  const crest = [3, 4, 6][stage];
  b.add(b.depth(head.c) + 1.2, () => {
    for (let i = 0; i < crest; i++) {
      const k = crest === 1 ? 0.5 : i / (crest - 1);
      const q = b.surface(head, p3(0.5 - k * 1.3, -1, 0), -0.4);
      const h = (stage === 2 ? 7 : 5) * (1 - Math.abs(k - 0.4) * 0.7);
      drawFlame(b.ctx, q.p.x, q.p.y + 1, 2.4, h, t * 6.28 + i * 1.7, i % 2 ? 0xffb030 : 0xff6a1a);
    }
  });
  b.ball(head, tn(DOWN[stage], 0.45), {
    bias: 1.5,
    band: 1.4,
    after: () => {
      for (const s of [1, -1]) b.eye(head, p3(0.72, -0.1, s * 0.62), 1.3, { iris: '#2a1008', pupil: '#120604', lid: p.lid, glint: true });
      const bk = b.surface(head, p3(1, 0.25, 0), 0.1);
      if (bk.vis > -0.3) {
        const dir = b.rig.vec(1, 0.15, 0);
        const L2 = Math.hypot(dir.x, dir.y) || 1;
        const ux = dir.x / L2, uy = dir.y / L2;
        b.ctx.fillStyle = '#ff8a2a';
        b.ctx.strokeStyle = '#6a2a10';
        b.ctx.lineWidth = 0.4;
        b.ctx.beginPath();
        b.ctx.moveTo(bk.p.x - uy * 1.2, bk.p.y + ux * 1.2 - 0.6);
        b.ctx.lineTo(bk.p.x + ux * 2.8, bk.p.y + uy * 2.8 + p.beak * 0.4);
        b.ctx.lineTo(bk.p.x + uy * 1.2, bk.p.y - ux * 1.2 + 0.6);
        b.ctx.closePath();
        b.ctx.fill();
        b.ctx.stroke();
      }
      // Rosy cheek.
      const ck = b.surface(head, p3(0.55, 0.35, 0.8));
      if (ck.vis > 0.1) {
        b.ctx.fillStyle = 'rgba(255,110,80,0.55)';
        b.ctx.beginPath();
        b.ctx.ellipse(ck.p.x, ck.p.y, 1.2, 0.7, 0, 0, Math.PI * 2);
        b.ctx.fill();
      }
    },
  });
}

export const PhoenixChickSpec: PetSpec<FlyPose> = {
  id: 'pet_phoenix',
  flyer: true,
  scale: [1, 1.14, 1.3],
  aura: 0xffa640,
  pose,
  build,
  shadow: p => ({ x: p.bx, r: 9, lift: Math.max(0, -p.by - 6) }),
  fx: (ctx, d) => {
    const { b, p, stage, action, t } = d;
    const body = bodyBall(p);
    const head = headBall(b, body, p);
    glow(ctx, b.p(body.c), 12 + stage * 2, 0xff8a2a, 0.25 + stage * 0.06);
    if (stage === 2) {
      // Sun halo behind the head.
      const hc = b.p(b.local(head, -1.5, -2, 0));
      ctx.save();
      ctx.strokeStyle = 'rgba(255,220,120,0.75)';
      ctx.lineWidth = 0.9;
      ctx.beginPath();
      ctx.arc(hc.x, hc.y, 7.5, 0, Math.PI * 2);
      ctx.stroke();
      for (let i = 0; i < 12; i++) {
        const a = (i / 12) * Math.PI * 2 + t * 0.5;
        ctx.beginPath();
        ctx.moveTo(hc.x + Math.cos(a) * 8.3, hc.y + Math.sin(a) * 8.3);
        ctx.lineTo(hc.x + Math.cos(a) * 9.6, hc.y + Math.sin(a) * 9.6);
        ctx.stroke();
      }
      ctx.restore();
    }
    // Drifting embers.
    for (let i = 0; i < 4 + stage; i++) {
      const k = (t + i / (4 + stage)) % 1;
      const q = b.p(b.local(body, -4 + ((i * 37) % 9), 3 - k * 16, ((i * 13) % 7) - 3));
      ctx.fillStyle = `rgba(255,${190 + (i % 3) * 20},90,${0.8 * (1 - k)})`;
      ctx.beginPath();
      ctx.arc(q.x, q.y, 0.6, 0, Math.PI * 2);
      ctx.fill();
    }
    if (action === 'cast' && p.glow > 0.05) {
      // Healing sun-flare: a warm ring bursting out.
      const c = b.p(body.c);
      glow(ctx, c, 20 * p.glow, 0xffc050, 0.5 * p.glow);
      ctx.strokeStyle = `rgba(255,236,150,${0.9 * p.glow})`;
      ctx.lineWidth = 1.2;
      ctx.beginPath();
      ctx.ellipse(c.x, c.y, 8 + p.glow * 8, 6 + p.glow * 6, 0, 0, Math.PI * 2);
      ctx.stroke();
    }
    if (action === 'attack' && t > 0.9) {
      const m = b.surface(head, p3(1, 0.25, 0), 2);
      glow(ctx, m.p, 7, 0xffa040, 0.6);
      drawFlame(ctx, m.p.x + 2, m.p.y + 2, 4, 6, 1, 0xff6a1a);
    }
    void vec;
  },
  ink: '#3a0e04',
  rim: 'rgba(255,240,170,0.75)',
};

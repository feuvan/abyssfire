// 灵脉精灵 — a little ley-line sprite: a glowing green-gold seed of a body
// with a big-eyed face, a two-leaf sprout on its head, four dragonfly wings
// and a wisp tail that trails light. Awakened (stage 1) a leaf crown and
// orbiting motes; stage 2 a blossom crown, gold-veined wings and a halo.
import { glow, rgba, vec } from '../rig/Rig';
import { Body3, h01, p3, star, tn, wing, type Ball, type PetAction, type PetDrawCtx, type PetSpec, type PetStage } from './PetKit';
import { flyPose, type FlyPose, type FlyStyle } from './Fly';

const STYLE: FlyStyle = { hover: 22, flapMid: 0.95, flapAmp: 0.5, flapsIdle: 3, flapsWalk: 4, bob: 2 };

const SKIN = [0xc8f4b0, 0xd0f8b8, 0xe0fcc8] as const;
const TAIL = [0x8ae07a, 0x9ae880, 0xb0f090] as const;
const LEAF = 0x5ab84a;

function pose(action: PetAction, t: number, stage: PetStage): FlyPose {
  const p = flyPose(STYLE, action, t, stage);
  return { ...p, tail: p.tail + Math.sin(t * Math.PI * 2) * 0.3 };
}

function headBall(p: FlyPose, front: boolean): Ball {
  return { c: p3(p.bx + p.hx * 0.5, p.by - 3 + p.hy, 0), r: p3(5.4, 5.2, 5.4), o: { pitch: p.hp + p.pitch * 0.4, yaw: front ? 0.55 : 0.1 } };
}

function tailPts(b: Body3, p: FlyPose, t: number): ReturnType<typeof p3>[] {
  const pts = [];
  for (let i = 0; i <= 5; i++) {
    const k = i / 5;
    pts.push(p3(p.bx - 1 - k * 6 - p.pitch * k * 6, p.by + 3 + k * 9 - p.tail * k * 5, Math.sin(t * Math.PI * 2 + k * 3) * 2 * k));
  }
  void b;
  return pts;
}

function wingArt(ctx: CanvasRenderingContext2D, L: number, w: number, stage: PetStage): void {
  ctx.beginPath();
  ctx.moveTo(0, -0.4);
  ctx.quadraticCurveTo(L * 0.5, -w, L, 0);
  ctx.quadraticCurveTo(L * 0.55, w * 0.9, 0, 0.6);
  ctx.closePath();
  ctx.fillStyle = stage === 2 ? 'rgba(236,255,200,0.78)' : 'rgba(210,255,220,0.72)';
  ctx.fill();
  ctx.strokeStyle = stage === 2 ? 'rgba(210,170,60,0.95)' : 'rgba(90,160,110,0.9)';
  ctx.lineWidth = 0.45;
  ctx.stroke();
  ctx.beginPath();
  ctx.moveTo(0, 0);
  ctx.lineTo(L * 0.9, 0);
  ctx.moveTo(L * 0.3, 0);
  ctx.lineTo(L * 0.55, -w * 0.45);
  ctx.moveTo(L * 0.5, 0);
  ctx.lineTo(L * 0.75, w * 0.4);
  ctx.strokeStyle = stage === 2 ? 'rgba(240,200,80,0.9)' : 'rgba(150,220,160,0.8)';
  ctx.lineWidth = 0.35;
  ctx.stroke();
}

function build(d: PetDrawCtx<FlyPose>): void {
  const { b, p, stage, t } = d;
  const head = headBall(p, b.front);
  const skin = tn(SKIN[stage], 0.5, 0.3);
  // Wings: two pairs off the upper back.
  const L = [9, 10.5, 12.5][stage];
  for (const s of [1, -1] as const) {
    const spread = s > 0 ? p.wn : p.wf;
    const anchor = b.local(head, -3.2, 1.5, s * 1.6);
    wing(b, anchor, s, spread, 0.5, ctx => wingArt(ctx, L, 2.6, stage), { bias: s > 0 ? 0.2 : -20, reach: 3 });
    wing(b, b.local(head, -3.4, 2.6, s * 1.4), s, spread + 0.45, 0.9, ctx => wingArt(ctx, L * 0.8, 2.2, stage), { bias: s > 0 ? 0.1 : -20.1, reach: 3 });
  }
  // Wisp tail (seed body tapering into light).
  const tp = tailPts(b, p, t);
  b.tube(tp, [3.4, 2.8, 2, 1.3, 0.8, 0.4], tn(TAIL[stage], 0.5, 0.3), { stroke: 0, band: 0.8, d: b.depth(head.c) - 3 });
  // Tiny arms.
  for (const s of [1, -1]) {
    const sh = b.local(head, 0.5, 5, s * 2.6);
    const hand = p3(sh.x + 1.5 + p.talon * 2.5, sh.y + 1.6 - p.glow * 3.5, sh.z + s * 0.6);
    b.capsule(sh, hand, 0.8, 0.8, skin, { stroke: 0, bias: s > 0 ? 3 : -3 });
  }
  // Sprout / crown.
  const leaf = tn(LEAF, 0.45);
  b.add(b.depth(head.c) + 1, () => {
    const top = b.surface(head, p3(-0.1, -1, 0), -0.3).p;
    const sway = Math.sin(t * Math.PI * 2) * 0.8;
    b.ctx.strokeStyle = leaf.line;
    b.ctx.lineWidth = 0.6;
    b.ctx.beginPath();
    b.ctx.moveTo(top.x, top.y);
    b.ctx.quadraticCurveTo(top.x + sway, top.y - 2, top.x + sway * 1.5, top.y - 3.5);
    b.ctx.stroke();
    for (const s of [-1, 1]) {
      const bx = top.x + sway * 1.5, by = top.y - 3.5;
      b.ctx.fillStyle = s < 0 ? leaf.base : leaf.light;
      b.ctx.beginPath();
      b.ctx.moveTo(bx, by);
      b.ctx.quadraticCurveTo(bx + s * 2.5, by - 3, bx + s * 5, by - 1);
      b.ctx.quadraticCurveTo(bx + s * 2.5, by + 1, bx, by);
      b.ctx.fill();
      b.ctx.stroke();
    }
  });
  b.ball(head, skin, {
    band: 1.2,
    bias: 1,
    after: () => {
      for (const s of [1, -1]) b.eye(head, p3(0.74, 0.08, s * 0.5), 1.45, { iris: '#1e3a28', pupil: '#0a1a10', lid: p.lid, glint: true });
      const m = b.surface(head, p3(0.9, 0.45, 0));
      if (m.vis > 0.1) {
        b.ctx.strokeStyle = '#3a6a3a';
        b.ctx.lineWidth = 0.45;
        b.ctx.beginPath();
        if (p.beak > 0.3) b.ctx.ellipse(m.p.x, m.p.y, 0.7, 0.5 + p.beak * 0.4, 0, 0, Math.PI * 2);
        else b.ctx.arc(m.p.x, m.p.y - 0.6, 0.9, 0.3, Math.PI - 0.3);
        b.ctx.stroke();
      }
      for (const s of [1, -1]) {
        const ck = b.surface(head, p3(0.6, 0.45, s * 0.75));
        if (ck.vis > 0.15) {
          b.ctx.fillStyle = 'rgba(255,170,150,0.5)';
          b.ctx.beginPath();
          b.ctx.ellipse(ck.p.x, ck.p.y, 1.1, 0.6, 0, 0, Math.PI * 2);
          b.ctx.fill();
        }
      }
    },
  });
  if (stage > 0) {
    // Crown of leaves (stage 1) or blossoms (stage 2).
    b.add(b.depth(head.c) + 2, () => {
      const n = 5;
      for (let i = 0; i < n; i++) {
        const a = -0.9 + (i / (n - 1)) * 1.8;
        const q = b.surface(head, p3(Math.cos(a) * 0.35, -0.8, Math.sin(a) * 0.7), 0.1);
        if (q.vis < -0.3) continue;
        if (stage === 1) {
          b.ctx.fillStyle = leaf.base;
          b.ctx.strokeStyle = leaf.line;
          b.ctx.lineWidth = 0.4;
          b.ctx.beginPath();
          b.ctx.ellipse(q.p.x, q.p.y - 1, 0.9, 1.8, a * 0.6, 0, Math.PI * 2);
          b.ctx.fill();
          b.ctx.stroke();
        } else {
          const col = ['#ffd860', '#ff9ac0', '#fff4c0', '#ff9ac0', '#ffd860'][i];
          for (let k = 0; k < 5; k++) {
            const pa = (k / 5) * Math.PI * 2;
            b.ctx.fillStyle = col;
            b.ctx.beginPath();
            b.ctx.arc(q.p.x + Math.cos(pa) * 1, q.p.y - 1 + Math.sin(pa) * 1, 0.8, 0, Math.PI * 2);
            b.ctx.fill();
          }
          b.ctx.fillStyle = '#e89020';
          b.ctx.beginPath();
          b.ctx.arc(q.p.x, q.p.y - 1, 0.5, 0, Math.PI * 2);
          b.ctx.fill();
        }
      }
    });
  }
}

export const SpriteSpec: PetSpec<FlyPose> = {
  id: 'pet_sprite',
  flyer: true,
  scale: [1.15, 1.3, 1.46],
  aura: 0xb8f070,
  pose,
  build,
  shadow: p => ({ x: p.bx, r: 8, lift: Math.max(0, -p.by - 6) }),
  fx: (ctx, d) => {
    const { b, p, stage, action, t } = d;
    const head = headBall(p, b.front);
    const c = b.p(head.c);
    glow(ctx, c, 13 + stage * 2, 0xc8ff90, 0.32 + stage * 0.05);
    // Light trailing from the wisp tail.
    const tp = tailPts(b, p, t);
    const tip = b.p(tp[5]);
    glow(ctx, tip, 4, 0xe8ff90, 0.6);
    for (let i = 0; i < 3; i++) {
      const k = (t + i / 3) % 1;
      star(ctx, vec(tip.x - k * 4 + i, tip.y + k * 3), 0.9 * (1 - k) + 0.3, rgba(0xfaffd0, 0.9 * (1 - k)));
    }
    if (stage > 0) {
      // Orbiting motes.
      const n = stage === 2 ? 3 : 2;
      for (let i = 0; i < n; i++) {
        const a = t * Math.PI * 2 + (i / n) * Math.PI * 2;
        const q = b.pt(p.bx + Math.cos(a) * 10, p.by - 2 + Math.sin(a * 2) * 1.5, Math.sin(a) * 10);
        glow(ctx, q, 3, 0xe0ff90, 0.7);
        ctx.fillStyle = '#fbffe0';
        ctx.beginPath();
        ctx.arc(q.x, q.y, 0.8, 0, Math.PI * 2);
        ctx.fill();
      }
    }
    if (stage === 2) {
      const hc = b.p(b.local(head, -1, -7.5, 0));
      ctx.save();
      ctx.strokeStyle = 'rgba(255,236,140,0.85)';
      ctx.lineWidth = 0.8;
      ctx.beginPath();
      ctx.ellipse(hc.x, hc.y, 4.5, 1.6, 0, 0, Math.PI * 2);
      ctx.stroke();
      ctx.restore();
      glow(ctx, hc, 5, 0xffe890, 0.35);
    }
    if (action === 'cast' && p.glow > 0.05) {
      // Healing pulse: rising leaf-green ring and sparkles.
      glow(ctx, c, 22 * p.glow, 0xa8ff80, 0.45 * p.glow);
      ctx.strokeStyle = `rgba(210,255,170,${0.9 * p.glow})`;
      ctx.lineWidth = 1.1;
      ctx.beginPath();
      ctx.ellipse(c.x, c.y + 3, 6 + p.glow * 9, 3 + p.glow * 4, 0, 0, Math.PI * 2);
      ctx.stroke();
      for (let i = 0; i < 5; i++) {
        const a = (i / 5) * Math.PI * 2 + t;
        star(ctx, vec(c.x + Math.cos(a) * (8 + p.glow * 5), c.y + Math.sin(a) * 5 - p.glow * 4), 1.3 * p.glow, `rgba(245,255,210,${p.glow})`);
      }
    }
    if (action === 'attack' && t > 0.9) {
      // A zap of ley light.
      const m = b.p(b.local(head, 8, 3, 0));
      glow(ctx, m, 7, 0xd0ff80, 0.7);
      star(ctx, m, 3.2, 'rgba(250,255,220,0.95)');
      for (let i = 0; i < 4; i++) {
        const a = h01(i) * Math.PI * 2;
        ctx.strokeStyle = 'rgba(230,255,170,0.9)';
        ctx.lineWidth = 0.6;
        ctx.beginPath();
        ctx.moveTo(m.x + Math.cos(a) * 2, m.y + Math.sin(a) * 2);
        ctx.lineTo(m.x + Math.cos(a) * 5, m.y + Math.sin(a) * 5);
        ctx.stroke();
      }
    }
  },
  ink: '#1a3a1c',
  rim: 'rgba(250,255,210,0.8)',
};

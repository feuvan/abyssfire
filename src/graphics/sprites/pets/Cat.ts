// 影猫 — a black shadow cat with violet eyes whose tail frays into smoke.
// Awakened (stage 1) violet sigils glow on the brow and flank; at stage 2 it
// grows a second smoky tail and violet flame licks at its ear tips.
import { glow, rgba, vec } from '../rig/Rig';
import { Body3, h01, p3, tn, type Ball, type PetAction, type PetDrawCtx, type PetSpec, type PetStage } from './PetKit';
import { buildQuad, quadActionPose, quadGeo, quadIdle, quadRest, quadWalk, tailPoints, type QuadBuild, type QuadPose } from './Quad';

const Q: QuadBuild = {
  body: p3(8.6, 4.6, 4.5),
  bodyY: 10.8,
  head: p3(5.2, 4.9, 5.1),
  headAt: vec(8.8, -5.6),
  thigh: 4.8,
  shin: 4.5,
  legR: 1.4,
  pawR: 1.5,
  legSpread: 0.6,
  frontAt: 0.6,
  backAt: 0.62,
};

const REST: QuadPose = { ...quadRest(Q), tail: 0.9 };

const FUR = [0x302a3e, 0x2c2640, 0x2a2244] as const;
const FAR = [0x1c1828, 0x1a1630, 0x1a1434] as const;
const EYE = ['#c89aff', '#d2a8ff', '#e4c0ff'] as const;
const SIGIL = 0xb070ff;

function pose(action: PetAction, t: number, stage: PetStage): QuadPose {
  if (action === 'idle') {
    const p = quadIdle(REST, t);
    return { ...p, wag: Math.sin(t * Math.PI * 2) * 2.4 };
  }
  if (action === 'walk') return quadWalk(REST, t, 3.4, 2.4, 0.5);
  if (action === 'cast') {
    // Arch the back and puff up rather than rear.
    const k = t < 0.67 ? t / 0.67 : 1 - (t - 0.67) * 0.6;
    const e = k * k * (3 - 2 * k);
    return { ...REST, by: REST.by - 1.6 * e, pitch: 0.05 * e, hy: 1.2 * e, hp: 0.2 * e, jaw: 0.8 * e, ears: 0.8 * e, tail: REST.tail + 0.5 * e, glow: e, lid: 1 };
  }
  return quadActionPose(REST, action, t, stage, 'swipe') ?? REST;
}

function tails(g: ReturnType<typeof quadGeo>, p: QuadPose, stage: PetStage): { pts: ReturnType<typeof tailPoints>; }[] {
  const out = [{ pts: tailPoints(g.tailRoot, 15, 6, 0.35 + p.tail * 0.6, 1.1, p.wag, 0) }];
  if (stage === 2) out.push({ pts: tailPoints({ ...g.tailRoot, z: g.tailRoot.z - 1 }, 13, 6, 0.2 + p.tail * 0.5, 0.6, -p.wag - 2.5, 0) });
  return out;
}

function drawHead(b: Body3, head: Ball, p: QuadPose, stage: PetStage): void {
  const fur = tn(FUR[stage], 0.45, 0.35);
  const muzzle: Ball = { c: b.local(head, 3.9, 1.6, 0), r: p3(2.2, 1.9, 2.8), o: head.o };
  const muzzleFirst = b.depth(muzzle.c) < b.depth(head.c);
  const drawMuzzle = (): void => {
    if (p.jaw > 0.1) {
      const m = b.ellipse({ c: b.local(head, 4, 2.6 + p.jaw * 0.6, 0), r: p3(1.8, 0.8 + p.jaw, 1.8), o: head.o });
      b.ctx.fillStyle = '#6a2a4a';
      b.ctx.beginPath();
      b.ctx.ellipse(m.c.x, m.c.y, m.rx, m.ry, m.rot, 0, Math.PI * 2);
      b.ctx.fill();
    }
    b.ballNow(muzzle, tn(0x3e3850, 0.45), { band: 0.7, stroke: 0 });
    const nose = b.surface(muzzle, p3(1, -0.45, 0));
    if (nose.vis > -0.2) {
      b.ctx.fillStyle = '#e88ab0';
      b.ctx.beginPath();
      b.ctx.moveTo(nose.p.x - 0.9, nose.p.y - 0.5);
      b.ctx.lineTo(nose.p.x + 0.9, nose.p.y - 0.5);
      b.ctx.lineTo(nose.p.x, nose.p.y + 0.6);
      b.ctx.fill();
      // Whiskers.
      b.ctx.strokeStyle = 'rgba(220,210,240,0.75)';
      b.ctx.lineWidth = 0.3;
      for (const s of [-1, 1]) {
        for (const dy of [-0.3, 0.6]) {
          b.ctx.beginPath();
          b.ctx.moveTo(nose.p.x + s * 1.5, nose.p.y + dy + 0.8);
          b.ctx.lineTo(nose.p.x + s * 5, nose.p.y + dy * 2);
          b.ctx.stroke();
        }
      }
    }
  };
  if (muzzleFirst) drawMuzzle();
  b.ballNow(head, fur, { band: 1.3 });
  if (stage > 0) {
    // Brow sigil: a small violet diamond with a tick either side.
    const s = b.surface(head, p3(0.62, -0.78, 0));
    if (s.vis > 0) {
      b.ctx.fillStyle = rgba(0xd8b0ff, 0.95);
      b.ctx.beginPath();
      b.ctx.moveTo(s.p.x, s.p.y - 1.6);
      b.ctx.lineTo(s.p.x + 1, s.p.y);
      b.ctx.lineTo(s.p.x, s.p.y + 1.2);
      b.ctx.lineTo(s.p.x - 1, s.p.y);
      b.ctx.fill();
    }
  }
  for (const side of [1, -1]) {
    b.eye(head, p3(0.72, -0.1, side * 0.6), 1.35, { iris: EYE[stage], pupil: '#140a20', slit: true, lid: p.lid * (p.glow > 0.5 ? 0.8 : 1) });
  }
  if (!muzzleFirst) drawMuzzle();
}

function build(d: PetDrawCtx<QuadPose>): void {
  const { b, p, stage } = d;
  const fur = tn(FUR[stage], 0.45, 0.35);
  const far = tn(FAR[stage], 0.35, 0.3);
  buildQuad(b, Q, {
    fur,
    furFar: far,
    head: (bb, head, pp) => drawHead(bb, head, pp, stage),
    bodyMarks: (bb, body) => {
      if (stage === 0) return;
      bb.clipBall(body, () => {
        bb.ctx.strokeStyle = rgba(SIGIL, 0.9);
        bb.ctx.lineWidth = 0.8;
        for (const x of [-0.45, -0.1, 0.25]) {
          const a = bb.surface(body, p3(x, -0.9, 0.35));
          const m = bb.surface(body, p3(x + 0.12, -0.4, 0.9));
          const c = bb.surface(body, p3(x + 0.05, 0.1, 1));
          if (m.vis < 0) continue;
          bb.ctx.beginPath();
          bb.ctx.moveTo(a.p.x, a.p.y);
          bb.ctx.quadraticCurveTo(m.p.x, m.p.y, c.p.x, c.p.y);
          bb.ctx.stroke();
        }
      });
    },
    extra: (bb, g, pp) => {
      const head = g.head;
      for (const s of [1, -1]) {
        const back = pp.ears * 2.4;
        const pts = [
          bb.local(head, 1.8, -3.4, s * 1.2),
          bb.local(head, -1.8, -3.2, s * 4),
          bb.local(head, -0.2 - back, -8.8 + back * 1.5, s * 3.3),
        ];
        const inner = [
          bb.local(head, 1, -3.7, s * 1.9),
          bb.local(head, -1.2, -3.6, s * 3.4),
          bb.local(head, -0.3 - back, -7.4 + back * 1.3, s * 3.1),
        ];
        bb.poly(pts, s > 0 ? fur : far, {
          band: 0.8,
          bias: s > 0 ? 2 : -2,
          after: () => {
            if (bb.front || s > 0) {
              bb.ctx.fillStyle = '#6a3a7a';
              const q = inner.map(v => bb.p(v));
              bb.ctx.beginPath();
              bb.ctx.moveTo(q[0].x, q[0].y);
              bb.ctx.lineTo(q[1].x, q[1].y);
              bb.ctx.lineTo(q[2].x, q[2].y);
              bb.ctx.fill();
            }
          },
        });
      }
      for (const tl of tails(g, pp, stage)) {
        // Solid for the first two thirds; the rest is smoke (fx pass).
        bb.tube(tl.pts.slice(0, 5), [1.5, 1.4, 1.3, 1.2, 1.05], fur, { band: 0.6, stroke: 0 });
      }
    },
  }, p);
}

function smoke(ctx: CanvasRenderingContext2D, x: number, y: number, r: number, a: number, stage: PetStage): void {
  ctx.fillStyle = `rgba(${stage === 2 ? '70,40,110' : '52,40,76'},${a})`;
  ctx.beginPath();
  ctx.arc(x, y, r, 0, Math.PI * 2);
  ctx.fill();
}

export const CatSpec: PetSpec<QuadPose> = {
  id: 'pet_cat',
  flyer: false,
  scale: [1, 1.14, 1.3],
  aura: 0xa070ff,
  pose,
  build,
  shadow: p => ({ x: p.bx + 1, r: 12, lift: 0 }),
  fx: (ctx, d) => {
    const { b, p, stage, action, t } = d;
    const g = quadGeo(Q, p, b.front);
    // Smoky tail tips.
    for (const tl of tails(g, p, stage)) {
      const pts = tl.pts;
      for (let i = 0; i < 6; i++) {
        const k = i / 5;
        const a = pts[4], c = pts[6];
        const q = b.pt(a.x + (c.x - a.x) * k + Math.sin(t * 6.28 + i) * 0.6, a.y + (c.y - a.y) * k - k * 2, a.z + (c.z - a.z) * k);
        smoke(ctx, q.x, q.y, 1.6 - k * 0.6 + h01(i + t * 7) * 0.4, 0.75 - k * 0.5, stage);
      }
      const tip = b.p(pts[6]);
      for (let i = 0; i < 3; i++) {
        const k = (t + i / 3) % 1;
        smoke(ctx, tip.x + Math.sin(k * 6 + i) * 1.5, tip.y - k * 7, 1 + k * 1.2, 0.5 * (1 - k), stage);
      }
    }
    // Glowing eyes read on dark ground.
    const head = g.head;
    for (const s of [1, -1]) {
      const e = b.surface(head, p3(0.72, -0.1, s * 0.6));
      if (e.vis > 0.15) glow(ctx, e.p, 2.6 + stage * 0.5, 0xc080ff, 0.35 + stage * 0.08);
    }
    if (stage === 2) {
      // Violet flame on the ear tips.
      for (const s of [1, -1]) {
        const tip = b.p(b.local(head, -0.2 - p.ears * 2.4, -8.8 + p.ears * 3.6, s * 3.3));
        glow(ctx, tip, 3.2, 0xb070ff, 0.6);
        ctx.fillStyle = 'rgba(220,180,255,0.9)';
        ctx.beginPath();
        ctx.moveTo(tip.x - 0.8, tip.y);
        ctx.quadraticCurveTo(tip.x, tip.y - 3.5 - Math.sin(t * 6.28 + s) * 0.8, tip.x + 0.8, tip.y);
        ctx.fill();
      }
    }
    if (action === 'attack' && t > 0.6) {
      // Three violet claw rakes.
      const paw = b.p(g.feet[0]);
      const k = (t - 0.6) / 0.4;
      ctx.strokeStyle = `rgba(210,170,255,${0.9 * k})`;
      ctx.lineWidth = 0.9;
      ctx.lineCap = 'round';
      for (let i = 0; i < 3; i++) {
        ctx.beginPath();
        ctx.moveTo(paw.x + 2 + i * 1.6, paw.y - 9 + i * 0.8);
        ctx.quadraticCurveTo(paw.x + 7 + i * 1.6, paw.y - 5 + i, paw.x + 6 + i * 1.4, paw.y + 1 + i * 0.6);
        ctx.stroke();
      }
      glow(ctx, vec(paw.x + 5, paw.y - 4), 7, 0xa070ff, 0.45 * k);
    }
    if (action === 'cast' && p.glow > 0.05) {
      // Shadow burst ring.
      const c = b.pt(p.bx, -1, 0);
      ctx.save();
      ctx.translate(c.x, c.y);
      ctx.scale(1, 0.5);
      ctx.strokeStyle = `rgba(150,90,230,${0.8 * p.glow})`;
      ctx.lineWidth = 1.6;
      ctx.beginPath();
      ctx.arc(0, 0, 10 + p.glow * 10, 0, Math.PI * 2);
      ctx.stroke();
      ctx.restore();
      for (let i = 0; i < 6; i++) {
        const a = (i / 6) * Math.PI * 2;
        smoke(ctx, c.x + Math.cos(a) * (8 + p.glow * 9), c.y + Math.sin(a) * (4 + p.glow * 4) - 2, 1.8, 0.55 * p.glow, stage);
      }
    }
  },
  ink: '#0e0a16',
  rim: 'rgba(200,160,255,0.75)',
};

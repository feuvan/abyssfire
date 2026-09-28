// 熔岩幼龙 — a lava hatchling from the dwarves' forge: basalt-dark hide split
// by glowing ember cracks, a molten belly, stubby horns, little leathery
// wings and a tail that ends in a flame. Breathes fire (cast). Awakened
// (stage 1) bigger wings and brighter cracks; stage 2 a crown of horns and
// a flame mane down the neck.
import { glow, rgba, vec } from '../rig/Rig';
import { Body3, p3, tn, wing, type Ball, type PetAction, type PetDrawCtx, type PetSpec, type PetStage } from './PetKit';
import { buildQuad, quadActionPose, quadGeo, quadIdle, quadRest, quadTrack, quadWalk, tailPoints, type QuadBuild, type QuadPose } from './Quad';
import { drawFlame } from '../decorations/CampProps';

const Q: QuadBuild = {
  body: p3(7.8, 5.6, 5.8),
  bodyY: 10.4,
  head: p3(5.2, 4.6, 4.7),
  headAt: vec(8.6, -6.4),
  thigh: 4.4,
  shin: 4.2,
  legR: 2,
  pawR: 2,
  legSpread: 0.66,
  frontAt: 0.55,
  backAt: 0.55,
};

const REST: QuadPose = { ...quadRest(Q), tail: 0.15 };

const HIDE = [0x5a2a26, 0x5e2822, 0x642420] as const;
const HIDE_FAR = [0x3a1a1a, 0x3c1818, 0x401614] as const;
const BELLY = [0xf09a3a, 0xf6a83a, 0xffb840] as const;
const CRACK = [0xff8a2a, 0xffa030, 0xffc040] as const;
const HORN = 0xe8d4a0;

function pose(action: PetAction, t: number, stage: PetStage): QuadPose {
  const ph = t * Math.PI * 2;
  if (action === 'idle') {
    const p = quadIdle(REST, t);
    return { ...p, fx: 0.5 + Math.sin(ph) * 0.5 };
  }
  if (action === 'walk') {
    const p = quadWalk(REST, t, 2.8, 2.2, 0.7);
    return { ...p, fx: 0.5 + Math.sin(ph * 2) * 0.5 };
  }
  if (action === 'cast') {
    // Inhale (rear back), then breathe a cone of fire forward-down.
    const inhale = { ...REST, bx: -1.5, by: REST.by - 1, pitch: -0.18, hy: -2, hp: -0.45, jaw: 0.2, glow: 0.3, tail: REST.tail + 0.4 };
    const breathe = { ...REST, bx: 1.2, by: REST.by + 0.4, pitch: 0.06, hx: 1.5, hy: 1.2, hp: 0.25, jaw: 1, glow: 1, tail: REST.tail - 0.1 };
    return quadTrack([[0, REST], [0.33, inhale], [0.67, breathe], [1, { ...breathe, glow: 0.8 }]], t);
  }
  return quadActionPose(REST, action, t, stage, 'bite') ?? REST;
}

function drawHead(b: Body3, head: Ball, p: QuadPose, stage: PetStage): void {
  const hide = tn(HIDE[stage], 0.42);
  const snout: Ball = { c: b.local(head, 4.6, 1.2, 0), r: p3(4, 2.1, 2.7), o: { ...head.o, pitch: (head.o?.pitch ?? 0) + 0.12 } };
  const jaw: Ball = { c: b.local(head, 3.8, 3 + p.jaw * 1.8, 0), r: p3(3, 1.3, 2.5), o: { ...head.o, pitch: (head.o?.pitch ?? 0) + p.jaw * 0.55 } };
  const snoutFirst = b.depth(snout.c) < b.depth(head.c);
  const drawSnout = (): void => {
    if (p.jaw > 0.1) {
      const m = b.ellipse({ c: b.local(head, 4.2, 2.8 + p.jaw * 0.9, 0), r: p3(2.8, 1 + p.jaw * 1.3, 2.2), o: head.o });
      b.ctx.fillStyle = '#ffb040';
      b.ctx.beginPath();
      b.ctx.ellipse(m.c.x, m.c.y, m.rx, m.ry, m.rot, 0, Math.PI * 2);
      b.ctx.fill();
      b.ballNow(jaw, tn(BELLY[stage], 0.35), { band: 0.6 });
    }
    b.ballNow(snout, hide, { band: 1 });
    // Nostrils with a wisp of smoke glow.
    const n = b.surface(snout, p3(0.9, -0.45, 0.3));
    if (n.vis > 0) {
      b.ctx.fillStyle = '#ffb040';
      b.ctx.beginPath();
      b.ctx.ellipse(n.p.x, n.p.y, 0.6, 0.4, 0, 0, Math.PI * 2);
      b.ctx.fill();
    }
  };
  if (snoutFirst) drawSnout();
  b.ballNow(head, hide, { band: 1.3 });
  // Ember crack across the brow.
  const c0 = b.surface(head, p3(0.3, -0.9, 0.3));
  const c1 = b.surface(head, p3(-0.3, -0.8, 0.5));
  if (c0.vis > 0 || c1.vis > 0) {
    b.ctx.strokeStyle = rgba(CRACK[stage], 1);
    b.ctx.lineWidth = 0.7;
    b.ctx.beginPath();
    b.ctx.moveTo(c0.p.x, c0.p.y);
    b.ctx.lineTo((c0.p.x + c1.p.x) / 2 + 0.6, (c0.p.y + c1.p.y) / 2 + 0.5);
    b.ctx.lineTo(c1.p.x, c1.p.y);
    b.ctx.stroke();
  }
  for (const s of [1, -1]) b.eye(head, p3(0.62, -0.22, s * 0.66), 1.3, { iris: '#ffd040', pupil: '#2a0a04', slit: true, lid: p.lid, sclera: '#2a0e0a' });
  if (!snoutFirst) drawSnout();
}

function build(d: PetDrawCtx<QuadPose>): void {
  const { b, p, stage, t, action } = d;
  const hide = tn(HIDE[stage], 0.42);
  const far = tn(HIDE_FAR[stage], 0.3);
  const g0 = quadGeo(Q, p, b.front);
  // Wings (behind / beside the shoulders), flapping a little.
  const flap = action === 'walk' ? Math.sin(t * Math.PI * 4) * 0.35 : action === 'cast' ? -0.3 * p.glow : Math.sin(t * Math.PI * 2) * 0.12;
  const span = [9, 11.5, 13][stage];
  const mem = tn(stage === 2 ? 0xd05a2a : 0xb04a2a, 0.4);
  for (const s of [1, -1] as const) {
    const anchor = b.local(g0.body, 2.5, -4.2, s * 3);
    wing(b, anchor, s, 0.9 + flap, 0.35, ctx => {
      // Finger bones fanning back from the leading edge; membrane between.
      const L = span;
      ctx.beginPath();
      ctx.moveTo(0, 0);
      ctx.lineTo(L, -1);
      ctx.quadraticCurveTo(L * 0.8, L * 0.35, L * 0.75, L * 0.55);
      ctx.quadraticCurveTo(L * 0.55, L * 0.45, L * 0.45, L * 0.7);
      ctx.quadraticCurveTo(L * 0.3, L * 0.5, 0, L * 0.45);
      ctx.closePath();
      const tt = s > 0 ? mem : tn(0x7a2a1a, 0.3);
      ctx.fillStyle = tt.base;
      ctx.fill();
      ctx.strokeStyle = tt.line;
      ctx.lineWidth = 0.5;
      ctx.stroke();
      ctx.strokeStyle = hide.base;
      ctx.lineWidth = 1.1;
      ctx.beginPath();
      ctx.moveTo(0, 0); ctx.lineTo(L, -1);
      ctx.moveTo(L * 0.62, -0.6); ctx.lineTo(L * 0.75, L * 0.55);
      ctx.moveTo(L * 0.3, -0.3); ctx.lineTo(L * 0.45, L * 0.7);
      ctx.stroke();
    }, { bias: s > 0 ? 0 : -30, reach: 3 });
  }
  buildQuad(b, Q, {
    fur: hide,
    furFar: far,
    belly: tn(BELLY[stage], 0.35),
    head: (bb, head, pp) => drawHead(bb, head, pp, stage),
    bodyMarks: (bb, body) => {
      // Belly plates and ember cracks.
      bb.ctx.strokeStyle = rgba(0x8a4a1a, 0.7);
      bb.ctx.lineWidth = 0.4;
      for (const x of [-0.3, 0, 0.3]) {
        const a = bb.surface(body, p3(x, 0.55, 0.55));
        const c = bb.surface(body, p3(x, 0.8, -0.1));
        if (a.vis < 0) continue;
        bb.ctx.beginPath(); bb.ctx.moveTo(a.p.x, a.p.y); bb.ctx.lineTo(c.p.x, c.p.y); bb.ctx.stroke();
      }
      const hot = 0.75 + p.fx * 0.25;
      bb.ctx.strokeStyle = rgba(CRACK[stage], hot);
      bb.ctx.lineWidth = stage === 0 ? 0.7 : 0.9;
      const cracks: [number, number, number][][] = [
        [[-0.6, -0.5, 0.6], [-0.3, -0.2, 0.9], [-0.1, -0.45, 0.85], [0.2, -0.15, 0.95]],
        [[0.2, -0.85, 0.35], [0.45, -0.6, 0.6], [0.6, -0.7, 0.3]],
        [[-0.4, -0.9, 0.2], [-0.15, -0.95, -0.1]],
      ];
      for (const cr of cracks) {
        const pts = cr.map(([x, y, z]) => bb.surface(body, p3(x, y, z)));
        if (pts.every(q => q.vis < 0)) continue;
        bb.ctx.beginPath();
        pts.forEach((q, i) => (i ? bb.ctx.lineTo(q.p.x, q.p.y) : bb.ctx.moveTo(q.p.x, q.p.y)));
        bb.ctx.stroke();
      }
    },
    extra: (bb, g, pp) => {
      const head = g.head;
      // Horns (two, four at stage 2), swept back.
      const horns: [number, number, number, number][] = [[-1.2, -3.4, 2.1, 5.5]];
      if (stage === 2) horns.push([-2.6, -2.4, 3.6, 3.6]);
      const horn = tn(HORN, 0.35);
      for (const [x, y, z, len] of horns) {
        for (const s of [1, -1]) {
          const base = bb.local(head, x, y, s * z);
          const tip = bb.local(head, x - len, y - len * 0.8, s * (z + 0.8));
          bb.poly([bb.local(head, x + 1.6, y + 0.6, s * z), bb.local(head, x - 1.6, y + 0.9, s * z), bb.local(head, x, y + 0.6, s * (z + 1.2)), tip, base], s > 0 ? horn : tn(0xb8a478, 0.3), { hull: true, band: 0.6, bias: s * 1.5, stroke: 0.4 });
        }
      }
      // Neck bridge.
      bb.tube([bb.local(g.body, 5.5, -2, 0), bb.local(head, -2.5, 1.5, 0)], [3.8, 3.4], hide, { d: bb.depth(head.c) + 1.2, stroke: 0, bias: -0.1 });
      // Tail with flame tip.
      const tp = tailPoints(g.tailRoot, 13, 5, 0.1 + pp.tail * 0.5, -0.4, pp.wag * 0.8, 0.6);
      bb.tube(tp, [3, 2.4, 1.8, 1.3, 0.9, 0.6], hide, { stroke: 0, band: 0.8 });
      if (stage === 2) {
        // Flame mane spines down the neck and back.
        for (let i = 0; i < 5; i++) {
          const k = i / 4;
          const base = bb.local(g.body, 5 - k * 11, -Q.body.y * (0.95 - k * 0.1), 0);
          bb.add(bb.depth(base) - 0.5, () => {
            const q = bb.p(base);
            drawFlame(bb.ctx, q.x, q.y + 1, 2.6 - k * 0.6, 5 - k * 1.2, t * 6.28 + i, 0xff7a20);
          });
        }
      }
    },
  }, p);
}

export const DragonSpec: PetSpec<QuadPose> = {
  id: 'pet_dragon',
  flyer: false,
  scale: [1, 1.14, 1.3],
  aura: 0xff7a2a,
  pose,
  build,
  shadow: p => ({ x: p.bx, r: 13, lift: 0 }),
  fx: (ctx, d) => {
    const { b, p, stage, action, t } = d;
    const g = quadGeo(Q, p, b.front);
    // Tail flame.
    const tp = tailPoints(g.tailRoot, 13, 5, 0.1 + p.tail * 0.5, -0.4, p.wag * 0.8, 0.6);
    const tip = b.p(tp[5]);
    drawFlame(ctx, tip.x, tip.y + 1.5, 3.2 + stage * 0.6, 6 + stage, t * Math.PI * 2, 0xff6a1a);
    // Cracks glow softly.
    glow(ctx, b.p(g.body.c), 10, CRACK[stage], 0.12 + stage * 0.06);
    if (action === 'cast' && p.glow > 0.5 && p.jaw > 0.6) {
      // Fire breath cone.
      const m = b.surface(g.head, p3(1, 0.5, 0));
      const dir = b.rig.vec(1, 0.35, 0);
      const len = 17 * p.glow;
      const n = vec(-dir.y, dir.x);
      const L = Math.hypot(dir.x, dir.y);
      const ux = dir.x / L, uy = dir.y / L;
      const nx = n.x / L, ny = n.y / L;
      const cone = (w: number, l: number, col: string): void => {
        ctx.fillStyle = col;
        ctx.beginPath();
        ctx.moveTo(m.p.x, m.p.y);
        ctx.quadraticCurveTo(m.p.x + ux * l * 0.6 + nx * w, m.p.y + uy * l * 0.6 + ny * w, m.p.x + ux * l + nx * w * 0.5, m.p.y + uy * l + ny * w * 0.5);
        ctx.quadraticCurveTo(m.p.x + ux * l * 1.1, m.p.y + uy * l * 1.1, m.p.x + ux * l - nx * w * 0.5, m.p.y + uy * l - ny * w * 0.5);
        ctx.quadraticCurveTo(m.p.x + ux * l * 0.6 - nx * w, m.p.y + uy * l * 0.6 - ny * w, m.p.x, m.p.y);
        ctx.fill();
      };
      glow(ctx, vec(m.p.x + ux * len * 0.6, m.p.y + uy * len * 0.6), len * 0.7, 0xff7a20, 0.5);
      cone(8, len, 'rgba(230,70,20,0.85)');
      cone(5, len * 0.8, 'rgba(255,150,40,0.9)');
      cone(2.5, len * 0.55, 'rgba(255,236,160,0.95)');
    }
    if (action === 'attack' && t > 0.9) {
      const m = b.surface(g.head, p3(1, 0.4, 0));
      glow(ctx, m.p, 7, 0xff9a40, 0.55);
      ctx.strokeStyle = 'rgba(255,220,150,0.9)';
      ctx.lineWidth = 0.9;
      for (let i = 0; i < 3; i++) {
        ctx.beginPath();
        ctx.moveTo(m.p.x + 3 + i * 1.5, m.p.y - 4 + i * 2.6);
        ctx.lineTo(m.p.x + 7 + i * 1.5, m.p.y - 5 + i * 2.6);
        ctx.stroke();
      }
    }
  },
  ink: '#1e0806',
  rim: 'rgba(255,200,140,0.65)',
};

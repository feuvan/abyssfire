// 玄武 — a jade tortoise under a domed stone shell, with a little serpent
// for a tail that coils up over its back. Awakened (stage 1) the shell's
// scutes glow with ley runes; at stage 2 the shell carries jade crystals and
// a gilded rim, and the serpent wears a golden crest.
import { glow, rgba, vec } from '../rig/Rig';
import { Body3, p3, star, tn, type Ball, type PetAction, type PetDrawCtx, type PetSpec, type PetStage } from './PetKit';
import { buildQuad, quadActionPose, quadGeo, quadIdle, quadRest, quadTrack, quadWalk, type QuadBuild, type QuadPose } from './Quad';
import type { V3 } from '../rig/HumanView';

const Q: QuadBuild = {
  body: p3(9.6, 6.2, 8.4),
  bodyY: 8.4,
  head: p3(3.6, 3.2, 3.3),
  headAt: vec(12.6, -0.8),
  thigh: 2.8,
  shin: 2.6,
  legR: 2.2,
  pawR: 2.2,
  legSpread: 0.78,
  frontAt: 0.62,
  backAt: 0.6,
};

const REST: QuadPose = { ...quadRest(Q), tail: 0 };

const SKIN = [0x86c878, 0x8ad07c, 0x96dc84] as const;
const SKIN_FAR = [0x5a9a58, 0x5e9e5a, 0x64a860] as const;
const SHELL = [0x6a8a70, 0x5f8f72, 0x5a9478] as const;
const SCUTE = [0x3f8a62, 0x2f9a6a, 0x28a878] as const;
const RIM = [0x8a8a7a, 0x8a8a7a, 0xc8a048] as const;
const LEY = 0xb8ff90;

function pose(action: PetAction, t: number, stage: PetStage): QuadPose {
  if (action === 'idle') {
    const p = quadIdle(REST, t);
    return { ...p, by: REST.by + Math.sin(t * Math.PI * 2) * 0.25, hx: Math.sin(t * Math.PI * 2) * 0.4 };
  }
  if (action === 'walk') {
    const p = quadWalk(REST, t, 2.2, 1.6, 0.3);
    return { ...p, hx: Math.sin(t * Math.PI * 4) * 0.5 };
  }
  if (action === 'cast') {
    // Pull in and glow: the shell flares with runes (the ward / taunt).
    const hide = { ...REST, hx: -3.5, hy: 0.8, by: REST.by + 1.2, glow: 1, lid: 0.2, feet: REST.feet.map(() => vec(0, 0)) as QuadPose['feet'] };
    return quadTrack([[0, REST], [0.33, { ...hide, glow: 0.5 }], [0.67, hide], [1, { ...REST, hx: -1, glow: 0.6 }]], t);
  }
  return quadActionPose(REST, action, t, stage, 'snap') ?? REST;
}

/** Serpent tail: rises from the rear, arcs over the shell's back half. */
function snakePath(g: ReturnType<typeof quadGeo>, p: QuadPose, t: number, action: PetAction): V3[] {
  const r = g.tailRoot;
  const sway = Math.sin(t * Math.PI * 2) * 0.8;
  const strike = action === 'attack' ? p.fx : 0;
  const rear = action === 'cast' ? p.glow : 0;
  const pts: V3[] = [];
  const n = 7;
  for (let i = 0; i <= n; i++) {
    const k = i / n;
    const x = r.x - 4.5 * Math.sin(k * Math.PI) + k * (4 + strike * 9);
    const y = r.y + 1 - k * (9 + rear * 3 - strike * 3) - Math.sin(k * Math.PI) * 2.5;
    const z = r.z + Math.sin(k * Math.PI * 2) * 2.2 + sway * k;
    pts.push(p3(x, y, z));
  }
  return pts;
}

function drawHead(b: Body3, head: Ball, p: QuadPose, stage: PetStage): void {
  const skin = tn(SKIN[stage], 0.4);
  b.ballNow(head, skin, { band: 0.9 });
  // Beak-like mouth line.
  const m = b.surface(head, p3(1, 0.35, 0));
  if (m.vis > -0.1) {
    b.ctx.strokeStyle = tn(SKIN[stage]).line;
    b.ctx.lineWidth = 0.5;
    b.ctx.beginPath();
    b.ctx.moveTo(m.p.x - 1.8, m.p.y - 0.2 + p.jaw * 0.6);
    b.ctx.lineTo(m.p.x + 0.3, m.p.y + p.jaw * 1.2);
    b.ctx.stroke();
  }
  for (const s of [1, -1]) b.eye(head, p3(0.55, -0.35, s * 0.72), 0.95, { iris: '#1c2a20', lid: p.lid, glint: true });
}

function build(d: PetDrawCtx<QuadPose>): void {
  const { b, p, stage, t, action } = d;
  const skin = tn(SKIN[stage], 0.4);
  const skinFar = tn(SKIN_FAR[stage], 0.3);
  const shell = tn(SHELL[stage], 0.32);
  buildQuad(b, Q, {
    fur: skin,
    body: shell,
    furFar: skinFar,
    paw: skin,
    head: (bb, head, pp) => drawHead(bb, head, pp, stage),
    bodyMarks: (bb, body) => {
      // Scutes: a spine row and two flank rows of hex plates.
      const plates: [number, number, number, number][] = [
        [0.45, -0.88, 0, 1], [0, -1, 0, 1.15], [-0.45, -0.88, 0, 1],
        [0.5, -0.45, 0.75, 0.85], [0, -0.55, 0.85, 0.95], [-0.5, -0.45, 0.75, 0.85],
        [0.5, -0.45, -0.75, 0.85], [0, -0.55, -0.85, 0.95], [-0.5, -0.45, -0.75, 0.85],
      ];
      const scute = tn(SCUTE[stage], 0.45);
      for (const [x, y, z, sz] of plates) {
        const c = bb.surface(body, p3(x, y, z));
        if (c.vis < 0.02) continue;
        const k = Math.min(1, c.vis * 1.6 + 0.2);
        const rr = 2.3 * sz;
        bb.ctx.beginPath();
        for (let i = 0; i < 6; i++) {
          const a = (i / 6) * Math.PI * 2 + Math.PI / 6;
          const px = c.p.x + Math.cos(a) * rr;
          const py = c.p.y + Math.sin(a) * rr * (0.55 + 0.35 * k);
          if (i) bb.ctx.lineTo(px, py); else bb.ctx.moveTo(px, py);
        }
        bb.ctx.closePath();
        bb.ctx.fillStyle = scute.base;
        bb.ctx.fill();
        bb.ctx.strokeStyle = scute.line;
        bb.ctx.lineWidth = 0.5;
        bb.ctx.stroke();
        bb.ctx.fillStyle = scute.light;
        bb.ctx.beginPath();
        bb.ctx.ellipse(c.p.x - rr * 0.3, c.p.y - rr * 0.25, rr * 0.35, rr * 0.2, 0, 0, Math.PI * 2);
        bb.ctx.fill();
        if (stage > 0 || (action === 'cast' && p.glow > 0.2)) {
          // Ley rune: a small glowing tick mark.
          bb.ctx.strokeStyle = rgba(LEY, stage > 0 ? 0.9 : p.glow);
          bb.ctx.lineWidth = 0.55;
          bb.ctx.beginPath();
          bb.ctx.moveTo(c.p.x - 0.8, c.p.y + 0.5);
          bb.ctx.lineTo(c.p.x, c.p.y - 0.8);
          bb.ctx.lineTo(c.p.x + 0.8, c.p.y + 0.5);
          bb.ctx.stroke();
        }
      }
    },
    extra: (bb, g, pp) => {
      // Shell rim (marginal band) under the dome.
      const rim: Ball = { c: bb.local(g.body, 0, 3.4, 0), r: p3(Q.body.x * 1.06, 1.8, Q.body.z * 1.06), o: g.body.o };
      bb.ball(rim, tn(RIM[stage], 0.35), { d: bb.depth(g.body.c) - 0.01, band: 0.8 });
      // Neck.
      const neckBase = bb.local(g.body, 8, 1.2, 0);
      bb.tube([neckBase, bb.local(g.head, -2.6, 0.6, 0)], [2.2, 2.4], skin, { d: bb.depth(g.head.c) + 1, stroke: 0, bias: -0.2 });
      // Serpent tail.
      const sp = snakePath(g, pp, t, action);
      const snake = tn(stage === 2 ? 0x2a7a6a : 0x2f6a5a, 0.4);
      bb.tube(sp, [1.6, 1.5, 1.4, 1.3, 1.2, 1.1, 1, 0.9], snake, { stroke: 0, band: 0.5 });
      const hd = sp[sp.length - 1];
      const dir = p3(hd.x - sp[sp.length - 2].x, hd.y - sp[sp.length - 2].y, 0);
      const pitch = Math.atan2(dir.y, dir.x);
      const sh: Ball = { c: p3(hd.x + Math.cos(pitch) * 1.2, hd.y + Math.sin(pitch) * 1.2, hd.z), r: p3(2, 1.4, 1.5), o: { pitch } };
      bb.ball(sh, snake, {
        band: 0.5,
        bias: 0.3,
        after: () => {
          for (const s of [1, -1]) bb.eye(sh, p3(0.45, -0.4, s * 0.7), 0.55, { iris: '#ffcc40', glint: false, pupil: '#201008', slit: true });
          const tip = bb.surface(sh, p3(1, 0.2, 0));
          if (action === 'attack' || t < 0.5) {
            bb.ctx.strokeStyle = '#e04a5a';
            bb.ctx.lineWidth = 0.4;
            bb.ctx.beginPath();
            bb.ctx.moveTo(tip.p.x, tip.p.y);
            bb.ctx.lineTo(tip.p.x + 1.6, tip.p.y + 0.3);
            bb.ctx.lineTo(tip.p.x + 2.2, tip.p.y - 0.3);
            bb.ctx.moveTo(tip.p.x + 1.6, tip.p.y + 0.3);
            bb.ctx.lineTo(tip.p.x + 2.2, tip.p.y + 0.9);
            bb.ctx.stroke();
          }
          if (stage === 2) {
            const c = bb.surface(sh, p3(-0.2, -1, 0));
            bb.ctx.fillStyle = '#f0c850';
            bb.ctx.beginPath();
            bb.ctx.moveTo(c.p.x - 1.2, c.p.y + 0.3);
            bb.ctx.lineTo(c.p.x - 0.6, c.p.y - 1.8);
            bb.ctx.lineTo(c.p.x, c.p.y - 0.4);
            bb.ctx.lineTo(c.p.x + 0.6, c.p.y - 1.8);
            bb.ctx.lineTo(c.p.x + 1.2, c.p.y + 0.3);
            bb.ctx.fill();
          }
        },
      });
      if (stage === 2) {
        // Jade crystals on the shell.
        for (const [x, z, h] of [[-0.25, 0.3, 3.6], [0.15, -0.35, 3], [-0.05, 0.05, 4.6]] as const) {
          const base = bb.surface(g.body, p3(x, -1, z));
          const q = base.q;
          const top = p3(q.x - 0.6, q.y - h, q.z);
          bb.poly([p3(q.x - 1.2, q.y + 0.4, q.z), top, p3(q.x + 1.2, q.y + 0.4, q.z), p3(q.x, q.y + 0.6, q.z + 1)], tn(0x5aefb0, 0.55), { hull: true, band: 0.6, bias: 0.5 });
        }
      }
    },
  }, p);
}

export const TortoiseSpec: PetSpec<QuadPose> = {
  id: 'pet_jade_tortoise',
  flyer: false,
  scale: [1, 1.14, 1.3],
  aura: 0x7aefa0,
  pose,
  build,
  shadow: p => ({ x: p.bx, r: 15, lift: 0 }),
  fx: (ctx, d) => {
    const { b, p, stage, action, t } = d;
    const g = quadGeo(Q, p, b.front);
    if (stage === 2) {
      for (const [x, z] of [[-0.05, 0.05], [-0.25, 0.3]] as const) {
        const s = b.surface(g.body, p3(x, -1, z));
        glow(ctx, vec(s.p.x, s.p.y - 5), 5, 0x7affc0, 0.45);
      }
    }
    if (action === 'cast' && p.glow > 0.05) {
      // Ward: a hexagonal barrier ring flashes round the shell.
      const c = b.pt(p.bx, p.by, 0);
      ctx.save();
      ctx.strokeStyle = `rgba(170,255,190,${0.85 * p.glow})`;
      ctx.lineWidth = 1.3;
      ctx.beginPath();
      for (let i = 0; i <= 6; i++) {
        const a = (i / 6) * Math.PI * 2 + Math.PI / 6;
        const x = c.x + Math.cos(a) * 18, y = c.y + Math.sin(a) * 13;
        if (i) ctx.lineTo(x, y); else ctx.moveTo(x, y);
      }
      ctx.stroke();
      ctx.restore();
      glow(ctx, c, 18, 0x8affb0, 0.35 * p.glow);
      for (let i = 0; i < 3; i++) star(ctx, vec(c.x - 12 + i * 12, c.y - 12 - (i % 2) * 4), 1.4 * p.glow, `rgba(230,255,220,${p.glow})`);
    }
    if (action === 'attack' && t > 0.9) {
      const m = b.surface(g.head, p3(1, 0.2, 0));
      glow(ctx, m.p, 6, 0xc0ffb0, 0.5);
      ctx.strokeStyle = 'rgba(230,255,220,0.9)';
      ctx.lineWidth = 0.8;
      for (let i = 0; i < 3; i++) {
        ctx.beginPath();
        ctx.moveTo(m.p.x + 2 + i, m.p.y - 3 + i * 2.4);
        ctx.lineTo(m.p.x + 5.5 + i, m.p.y - 3.6 + i * 2.4);
        ctx.stroke();
      }
    }
  },
  ink: '#142a1c',
  rim: 'rgba(230,255,220,0.6)',
};

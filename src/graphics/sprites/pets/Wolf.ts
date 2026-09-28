// 月牙 — silver wolf pup, son of the moon wolf Volgan. Oversized ears and
// paws, a bushy white-tipped tail and a crescent moon on the brow. Awakened
// (stage 1) the crescent glows and moon-blue bands run down the flanks; at
// stage 2 a frosted ruff and a star-tipped tail.
import { glow, rgba, vec } from '../rig/Rig';
import { Body3, p3, star, tn, type Ball, type PetAction, type PetSpec, type PetStage } from './PetKit';
import { buildQuad, quadActionPose, quadGeo, quadIdle, quadRest, quadWalk, tailPoints, type QuadBuild, type QuadPose } from './Quad';

const Q: QuadBuild = {
  body: p3(8, 5.2, 5.4),
  bodyY: 11.5,
  head: p3(6.2, 5.8, 5.6),
  headAt: vec(8.6, -5.8),
  thigh: 4.8,
  shin: 4.6,
  legR: 1.75,
  pawR: 1.75,
  legSpread: 0.62,
  frontAt: 0.58,
  backAt: 0.6,
};

const REST = quadRest(Q);

interface Pal {
  fur: number;
  far: number;
  belly: number;
  saddle: number;
  eye: string;
  mark: number;
  band: number;
}

const PAL: Record<PetStage, Pal> = {
  0: { fur: 0xc3ccd8, far: 0x8e99ab, belly: 0xf0f4fa, saddle: 0x8a96aa, eye: '#46b8f0', mark: 0xeef8ff, band: 0x7fb8e8 },
  1: { fur: 0xc6d2e2, far: 0x8f9cb4, belly: 0xf2f6fc, saddle: 0x7c8cab, eye: '#5ad0ff', mark: 0xdff6ff, band: 0x6fb2f0 },
  2: { fur: 0xd2dcec, far: 0x97a5c0, belly: 0xf6f9ff, saddle: 0x6f82b0, eye: '#7ae0ff', mark: 0xfff4c8, band: 0x5aa8ff },
};

function pose(action: PetAction, t: number, stage: PetStage): QuadPose {
  if (action === 'idle') return quadIdle(REST, t);
  if (action === 'walk') return quadWalk(REST, t, 3.2, 2.6, 0.8);
  return quadActionPose(REST, action, t, stage, 'bite') ?? REST;
}

function crescent(ctx: CanvasRenderingContext2D, x: number, y: number, r: number, sx: number, color: string): void {
  ctx.save();
  ctx.translate(x, y);
  ctx.scale(sx, 1);
  ctx.fillStyle = color;
  ctx.beginPath();
  ctx.arc(0, 0, r, 0, Math.PI * 2);
  ctx.arc(r * 0.45, -r * 0.3, r * 0.85, 0, Math.PI * 2, true);
  ctx.fill('evenodd');
  ctx.restore();
}

function drawHead(b: Body3, head: Ball, p: QuadPose, stage: PetStage): void {
  const pal = PAL[stage];
  const fur = tn(pal.fur, 0.38);
  const snout: Ball = { c: b.local(head, 4.8, 1.9, 0), r: p3(3, 1.95, 2.3), o: { ...head.o, pitch: (head.o?.pitch ?? 0) + 0.1 } };
  const jawBall: Ball = { c: b.local(head, 4.2, 3.2 + p.jaw * 1.6, 0), r: p3(2.8, 1.3, 2.2), o: { ...head.o, pitch: (head.o?.pitch ?? 0) + p.jaw * 0.5 } };
  const snoutFirst = b.depth(snout.c) < b.depth(head.c);
  const drawSnout = (): void => {
    if (p.jaw > 0.1) {
      const m = b.ellipse({ c: b.local(head, 4.4, 2.8 + p.jaw * 0.8, 0), r: p3(2.6, 1 + p.jaw * 1.2, 2), o: head.o });
      b.ctx.fillStyle = '#5a1e2e';
      b.ctx.beginPath();
      b.ctx.ellipse(m.c.x, m.c.y, m.rx, m.ry, m.rot, 0, Math.PI * 2);
      b.ctx.fill();
      b.ballNow(jawBall, tn(pal.belly, 0.3), { band: 0.6 });
    }
    b.ballNow(snout, tn(pal.fur, 0.45), { band: 0.8 });
    const nose = b.surface(snout, p3(1, -0.25, 0));
    if (nose.vis > -0.3) {
      b.ctx.fillStyle = '#26222e';
      b.ctx.beginPath();
      b.ctx.ellipse(nose.p.x, nose.p.y, 1.3, 0.95, 0, 0, Math.PI * 2);
      b.ctx.fill();
      b.ctx.fillStyle = 'rgba(255,255,255,0.7)';
      b.ctx.beginPath();
      b.ctx.arc(nose.p.x - 0.4, nose.p.y - 0.35, 0.35, 0, Math.PI * 2);
      b.ctx.fill();
    }
    if (p.jaw > 0.5) {
      // Little fangs.
      const f1 = b.surface(snout, p3(0.8, 0.75, 0.45));
      const f2 = b.surface(snout, p3(0.8, 0.75, -0.45));
      for (const f of [f1, f2]) {
        if (f.vis < -0.2) continue;
        b.ctx.fillStyle = '#fbf6ea';
        b.ctx.beginPath();
        b.ctx.moveTo(f.p.x - 0.5, f.p.y);
        b.ctx.lineTo(f.p.x + 0.5, f.p.y);
        b.ctx.lineTo(f.p.x, f.p.y + 1.3);
        b.ctx.fill();
      }
    }
  };
  if (snoutFirst) drawSnout();
  b.ballNow(head, fur, { band: 1.5 });
  // Pale cheeks.
  if (b.surface(head, p3(0.7, 0.45, 0)).vis > 0) b.clipBall(head, () => {
    const ch = b.ellipse({ c: b.local(head, 3, 2.6, 0), r: p3(3.4, 2.6, 5), o: head.o });
    b.ctx.fillStyle = tn(pal.belly).base;
    b.ctx.beginPath();
    b.ctx.ellipse(ch.c.x, ch.c.y, ch.rx, ch.ry, ch.rot, 0, Math.PI * 2);
    b.ctx.fill();
  });
  // Crescent on the brow.
  const brow = b.surface(head, p3(0.55, -0.85, 0));
  if (brow.vis > -0.1) {
    crescent(b.ctx, brow.p.x, brow.p.y, 1.7 + stage * 0.25, Math.max(0.5, Math.min(1, brow.vis + 0.5)), rgba(pal.mark, 1));
  }
  for (const s of [1, -1]) {
    b.eye(head, p3(0.72, -0.12, s * 0.62), 1.25, { iris: pal.eye, pupil: '#10202e', lid: p.lid, sclera: '#1a2030' });
  }
  if (!snoutFirst) drawSnout();
}

function build(d: Parameters<PetSpec<QuadPose>['build']>[0]): void {
  const { b, p, stage } = d;
  const pal = PAL[stage];
  const fur = tn(pal.fur, 0.38);
  buildQuad(b, Q, {
    fur,
    furFar: tn(pal.far, 0.25),
    belly: tn(pal.belly),
    paw: tn(pal.belly, 0.3),
    head: (bb, head, pp) => drawHead(bb, head, pp, stage),
    bodyMarks: (bb, body) => {
      bb.clipBall(body, () => {
        // Darker saddle along the back.
        const s = bb.ellipse({ c: bb.local(body, -1, -3.4, 0), r: p3(7, 2.6, 4.2), o: body.o });
        bb.ctx.fillStyle = tn(pal.saddle).base;
        bb.ctx.beginPath();
        bb.ctx.ellipse(s.c.x, s.c.y, s.rx, s.ry, s.rot, 0, Math.PI * 2);
        bb.ctx.fill();
        if (stage > 0) {
          // Moon-blue flank bands.
          bb.ctx.strokeStyle = rgba(pal.band, 0.95);
          bb.ctx.lineWidth = 1.1;
          for (const x of [-3.5, 0, 3.5]) {
            const a = bb.surface(body, p3(x / 8, -0.7, 0.7));
            const c = bb.surface(body, p3(x / 8 + 0.12, 0.1, 1));
            if (a.vis < 0 && c.vis < 0) continue;
            bb.ctx.beginPath();
            bb.ctx.moveTo(a.p.x, a.p.y);
            bb.ctx.lineTo(c.p.x, c.p.y);
            bb.ctx.stroke();
          }
        }
      });
    },
    extra: (bb, g, pp) => {
      const head = g.head;
      // Ears: big pup ears, pinned back when attacking.
      for (const s of [1, -1]) {
        const back = pp.ears * 2.2;
        const pts = [
          bb.local(head, 1.2, -3.6, s * 1.4),
          bb.local(head, -2.6, -3.2, s * 3.8),
          bb.local(head, -1.4 - back, -9.6 + back * 1.4, s * 3.9),
        ];
        const inner = [
          bb.local(head, 0.3, -3.9, s * 2),
          bb.local(head, -2, -3.7, s * 3.4),
          bb.local(head, -1.4 - back, -8 + back * 1.2, s * 3.6),
        ];
        bb.poly(pts, s > 0 ? fur : tn(pal.far, 0.3), {
          band: 1,
          bias: s > 0 ? 2 : -2,
          after: () => {
            if (bb.front || s > 0) {
              bb.ctx.fillStyle = '#e6aebb';
              bb.ctx.beginPath();
              const q = inner.map(v => bb.p(v));
              bb.ctx.moveTo(q[0].x, q[0].y);
              bb.ctx.lineTo(q[1].x, q[1].y);
              bb.ctx.lineTo(q[2].x, q[2].y);
              bb.ctx.fill();
            }
          },
        });
      }
      // Bushy tail with a white tip.
      const tp = tailPoints(g.tailRoot, 11, 4, 0.55 + pp.tail * 0.5, 0.6, pp.wag, 0);
      bb.tube(tp, [1.8, 2.9, 3.2, 2.6, 1.3], fur, {
        band: 1.2,
        stroke: 0,
        after: () => {
          const tip = bb.p(tp[4]);
          const mid = bb.p(tp[3]);
          bb.ctx.fillStyle = tn(pal.belly).base;
          bb.ctx.beginPath();
          bb.ctx.ellipse((tip.x + mid.x) / 2 + (tip.x - mid.x) * 0.25, (tip.y + mid.y) / 2 + (tip.y - mid.y) * 0.25, 2, 1.6, Math.atan2(tip.y - mid.y, tip.x - mid.x), 0, Math.PI * 2);
          bb.ctx.fill();
        },
      });
      if (stage === 2) {
        // Frosted ruff round the neck.
        const neck = bb.local(g.body, 6.2, -2.4, 0);
        const nd = bb.depth(neck);
        bb.add(nd + 1, () => {
          const c = bb.p(neck);
          const n = 11;
          const pts: { x: number; y: number }[] = [];
          for (let i = 0; i < n * 2; i++) {
            const a = (i / (n * 2)) * Math.PI * 2;
            const r = i % 2 === 0 ? 5.6 : 3.8;
            pts.push({ x: c.x + Math.cos(a) * r * 0.9, y: c.y + Math.sin(a) * r * 0.75 });
          }
          const t = tn(0xe4eeff, 0.4);
          bb.ctx.beginPath();
          pts.forEach((q, i) => (i ? bb.ctx.lineTo(q.x, q.y) : bb.ctx.moveTo(q.x, q.y)));
          bb.ctx.closePath();
          bb.ctx.fillStyle = t.base;
          bb.ctx.fill();
          bb.ctx.strokeStyle = t.line;
          bb.ctx.lineWidth = 0.5;
          bb.ctx.stroke();
          bb.ctx.fillStyle = rgba(0x9fd4ff, 0.8);
          for (let i = 0; i < n * 2; i += 2) {
            const q = pts[i];
            bb.ctx.beginPath();
            bb.ctx.arc(q.x, q.y, 0.6, 0, Math.PI * 2);
            bb.ctx.fill();
          }
        });
      }
    },
  }, p);
}

export const WolfSpec: PetSpec<QuadPose> = {
  id: 'pet_storm_wolf',
  flyer: false,
  scale: [1, 1.14, 1.3],
  aura: 0x8fd8ff,
  pose,
  build,
  shadow: p => ({ x: p.bx + 1, r: 13, lift: 0 }),
  fx: (ctx, d) => {
    const { b, p, stage, action, t } = d;
    const pal = PAL[stage];
    const g = quadGeo(Q, p, b.front);
    // Brow crescent glow (evolved) and the howl's moonlight on cast.
    const head: Ball = g.head;
    const brow = b.surface(head, p3(0.55, -0.85, 0));
    if (stage > 0 && brow.vis > -0.1) glow(ctx, brow.p, 4 + stage * 1.5, pal.band, 0.45 + stage * 0.12);
    if (action === 'cast' && p.glow > 0.05) {
      const top = b.pt(p.bx + 4, p.by - 18, 0);
      glow(ctx, top, 12 * p.glow, 0xcfeaff, 0.55 * p.glow);
      ctx.save();
      ctx.globalAlpha = p.glow;
      ctx.fillStyle = 'rgba(240,250,255,0.95)';
      ctx.beginPath();
      ctx.arc(top.x, top.y, 4, 0, Math.PI * 2);
      ctx.arc(top.x + 1.8, top.y - 1.2, 3.4, 0, Math.PI * 2, true);
      ctx.fill('evenodd');
      ctx.restore();
      for (let i = 0; i < 3; i++) star(ctx, vec(top.x - 8 + i * 8, top.y + 4 - (i % 2) * 5), 1.2 * p.glow, `rgba(220,245,255,${p.glow})`);
    }
    if (action === 'attack' && t > 0.9) {
      const m = b.surface(head, p3(1, 0.4, 0));
      ctx.strokeStyle = 'rgba(230,246,255,0.9)';
      ctx.lineWidth = 0.9;
      ctx.lineCap = 'round';
      for (let i = 0; i < 3; i++) {
        ctx.beginPath();
        ctx.moveTo(m.p.x + 3 + i * 1.6, m.p.y - 4 + i * 2.6);
        ctx.lineTo(m.p.x + 7 + i * 1.6, m.p.y - 5 + i * 2.6);
        ctx.stroke();
      }
      glow(ctx, m.p, 7, 0xbfe6ff, 0.5);
    }
    if (stage === 2) {
      // Star glint on the tail tip.
      const tp = tailPoints(g.tailRoot, 11, 4, 0.55 + p.tail * 0.5, 0.6, p.wag, 0);
      const tip = b.p(tp[4]);
      glow(ctx, tip, 4, 0xbfe6ff, 0.5);
      star(ctx, tip, 1.8, 'rgba(255,255,255,0.95)');
    }
  },
  ink: '#1c2236',
  rim: 'rgba(230,244,255,0.7)',
};

// src/graphics/sprites/decorations/TowerProps.ts
//
// 余烬之塔 (Ember Tower) homestead props: the ruined elven tower where the
// hero woke, its return portal, the 归炉 hearthstone that stands by every
// camp fire, and the six wings of the homestead in three states each —
// `_0` ruin (rubble, charred), `_1` restored, `_2` thriving (lights,
// banners, plants, glow).
//
// Keys: decor_tower_main, decor_tower_portal, decor_tower_hearthstone,
// decor_tower_<building>_<0|1|2>. Every building's sprite origin
// (anchorY) is the centre of its 2×2-tile footprint; the tower's is the
// centre of its 3×3 footprint.
import { defineDecor, rng, type DecorDrawer, type Rand } from './DecorKit';
import {
  T, ip, box, slab, gable, cylinder, faceCourses, rubble, beam, scorch, footprint, lantern, pennant, hangBanner, herb, crate, barrel, sack, coals,
  drawFlame, glow, contactShadow, flat, line, ellipseP, polyP, blobP, shade, curve, rgbaHex,
  type Iso, type P2,
} from './TowerKit';

type Ctx = CanvasRenderingContext2D;

export const TOWER_BUILDING_IDS = ['herb_garden', 'pet_house', 'gem_workshop', 'training_ground', 'altar', 'warehouse'] as const;
export type TowerBuildingId = (typeof TOWER_BUILDING_IDS)[number];

/** Texture key for a homestead wing at a stage (0 ruin, 1 restored, 2 thriving). */
export function towerBuildingKey(id: string, stage: number): string {
  return `decor_tower_${id}_${Math.max(0, Math.min(2, Math.floor(stage)))}`;
}

// ── Palette ─────────────────────────────────────────────────────────────

const STONE = 0xe4ddce; // elven white stone
const STONE_OLD = 0xa49c90; // weathered / sooty
const STONE_GREY = 0x8e8a86;
const CHAR = 0x3a302c;
const WOOD = 0x8f5e38;
const WOOD_DARK = 0x6a4428;
const SOIL = 0x6a4a32;
const EMBER = 0xff8a2a;
const LEY = 0xa8f070;
const GOLD = 0xe8c060;

const HALF = 32; // 2×2 footprint half size along each iso axis

// ── Shared stage dressing ───────────────────────────────────────────────

function ruinGround(ctx: Ctx, o: Iso, r: Rand): void {
  footprint(ctx, o, HALF, 0x5a4c40, 0.8, true, r);
  scorch(ctx, o.cx + 4, o.gy + 2, 40, 0.5);
  scorch(ctx, o.cx - 18, o.gy - 6, 18, 0.4);
}

function weeds(ctx: Ctx, o: Iso, r: Rand, n: number, col = 0x6a8a3a): void {
  for (let i = 0; i < n; i++) {
    const [x, y] = ip(o, (r() - 0.5) * HALF * 1.8, (r() - 0.5) * HALF * 1.8);
    herb(ctx, x, y, 3 + r() * 3, col, r);
  }
}

function brokenWall(ctx: Ctx, o: Iso, a0: number, a1: number, b0: number, b1: number, hMax: number, col: number, r: Rand): void {
  // A run of wall along a or b with a jagged top.
  const alongA = Math.abs(a1 - a0) > Math.abs(b1 - b0);
  const n = 6;
  const t = T(col, 0.25);
  for (let i = 0; i < n; i++) {
    const k0 = i / n, k1 = (i + 1) / n;
    const h = hMax * (0.25 + r() * 0.75) * (i === 0 || i === n - 1 ? 0.6 : 1);
    if (alongA) box(ctx, o, a0 + (a1 - a0) * k0, a0 + (a1 - a0) * k1, b0, b1, 0, h, t);
    else box(ctx, o, a0, a1, b0 + (b1 - b0) * k0, b0 + (b1 - b0) * k1, 0, h, t);
  }
}

function sparkleDots(ctx: Ctx, o: Iso, r: Rand, n: number, col: number, hMax: number): void {
  for (let i = 0; i < n; i++) {
    const [x, y] = ip(o, (r() - 0.5) * HALF * 2, (r() - 0.5) * HALF * 2, 8 + r() * hMax);
    glow(ctx, { x, y }, 3.5, col, 0.6);
    flat(ctx, ellipseP(ctx, x, y, 0.9, 0.9), '#fffbe0');
  }
}

// ── 药草园 herb garden ──────────────────────────────────────────────────

function drawHerbGarden(ctx: Ctx, o: Iso, r: Rand, stage: number): void {
  if (stage === 0) {
    ruinGround(ctx, o, r);
    // Burnt bed frames and dead stalks.
    for (const a0 of [-26, 4]) {
      slab(ctx, o, a0, a0 + 20, -24, 24, 2.5, T(CHAR, 0.2));
      for (let i = 0; i < 5; i++) {
        const [x, y] = ip(o, a0 + 10 + (r() - 0.5) * 12, -18 + i * 9, 2.5);
        line(ctx, [[x, y], [x + (r() - 0.5) * 3, y - 4 - r() * 3]], '#5a4a36', 0.8);
      }
    }
    for (let i = 0; i < 4; i++) {
      const [x, y] = ip(o, -30 + i * 16, -30);
      const h = 4 + r() * 8;
      box(ctx, o, -31 + i * 16, -29 + i * 16, -31, -29, 0, h, T(CHAR, 0.2));
      void x; void y;
    }
    beam(ctx, ip(o, -20, 26, 1), ip(o, 8, 30, 3), 1.6, WOOD_DARK, true);
    rubble(ctx, ...ip(o, 22, 20), 18, 5, STONE_GREY, r, 0.8);
    weeds(ctx, o, r, 3, 0x7a7a40);
    return;
  }
  footprint(ctx, o, HALF, stage === 2 ? 0x7a6a40 : 0x6e5a3e, 0.9);
  // Back fence (drawn first).
  const fence = T(stage === 2 ? 0xa87848 : WOOD, 0.3);
  for (let i = 0; i <= 4; i++) box(ctx, o, -31 + i * 15, -29 + i * 15, -32, -30, 0, 10, fence);
  for (const h of [4, 8]) line(ctx, [ip(o, -31, -31, h), ip(o, 31, -31, h)], fence.line, 1.1);
  for (let i = 0; i <= 4; i++) box(ctx, o, -32, -30, -31 + i * 15, -29 + i * 15, 0, 10, fence, {});
  for (const h of [4, 8]) line(ctx, [ip(o, -31, -31, h), ip(o, -31, 31, h)], fence.line, 1.1);
  if (stage === 2) {
    // Trellis arch with vines and a lantern at the back corner.
    const post = T(0x9a6a3a);
    const [px0, py0] = ip(o, -28, -12);
    const [px1, py1] = ip(o, -12, -28);
    box(ctx, o, -29, -27, -13, -11, 0, 30, post);
    box(ctx, o, -13, -11, -29, -27, 0, 30, post);
    curve(ctx, [px0, py0 - 30], [(px0 + px1) / 2, (py0 + py1) / 2 - 44], [px1, py1 - 30], post.line, 3);
    curve(ctx, [px0, py0 - 30], [(px0 + px1) / 2, (py0 + py1) / 2 - 44], [px1, py1 - 30], post.base, 1.8);
    for (let i = 0; i < 9; i++) {
      const k = i / 8;
      const x = px0 + (px1 - px0) * k;
      const y = py0 + (py1 - py0) * k - 30 - Math.sin(k * Math.PI) * 11;
      flat(ctx, ellipseP(ctx, x, y, 2.6, 1.8, k), i % 2 ? '#5aa048' : '#79c05a', '#2a5028', 0.4);
      if (i % 3 === 1) flat(ctx, ellipseP(ctx, x, y + 1.5, 1.2, 1.2), '#f07aa0');
    }
    lantern(ctx, (px0 + px1) / 2, (py0 + py1) / 2 - 30, 0xffd070, 1);
    // Beehive skep.
    const [hx, hy] = ip(o, 22, -26);
    shade(ctx, () => { ctx.moveTo(hx - 6, hy); ctx.quadraticCurveTo(hx - 7, hy - 12, hx, hy - 13); ctx.quadraticCurveTo(hx + 7, hy - 12, hx + 6, hy); ctx.closePath(); }, T(0xd8a848, 0.35), { band: 2.5, hi: 1, stroke: 0.6 });
    for (let k = 1; k < 4; k++) curve(ctx, [hx - 6 + k * 0.4, hy - k * 3], [hx, hy - k * 3 + 1.2], [hx + 6 - k * 0.4, hy - k * 3], '#8a6420', 0.5);
    flat(ctx, ellipseP(ctx, hx + 1, hy - 3, 1.4, 1), '#3a2410');
  }
  // Raised beds with herb rows.
  const bedWood = T(stage === 2 ? 0x9a6a3e : WOOD, 0.3);
  const plants = stage === 2 ? [0x5aa84a, 0x7ac05a, 0x4a9a6a] : [0x5a9a44, 0x6aa850];
  for (const a0 of [-24, 6]) {
    box(ctx, o, a0, a0 + 18, -22, 24, 0, 5, bedWood, { topFill: T(SOIL).base });
    for (let i = 0; i < 5; i++) {
      for (const da of [4.5, 13.5]) {
        const [x, y] = ip(o, a0 + da, -17 + i * 9.5, 5);
        const col = plants[(i + (da > 5 ? 1 : 0)) % plants.length];
        const fl = stage === 2 && (i + (da > 5 ? 1 : 0)) % 2 === 0 ? ['#f0d050', '#e878a8', '#a0c8ff'][i % 3] : undefined;
        herb(ctx, x, y, stage === 2 ? 7 + r() * 3 : 5 + r() * 2, col, r, fl);
        if (stage === 2 && i === 2 && da > 5) {
          // Glowing ley herb.
          glow(ctx, { x, y: y - 5 }, 9, LEY, 0.55);
          flat(ctx, ellipseP(ctx, x, y - 6, 1.6, 1.6), '#f4ffc0');
        }
      }
    }
  }
  // Watering can / stone path.
  if (stage >= 1) {
    const [wx, wy] = ip(o, 28, 10);
    cylinder(ctx, wx, wy, 3.2, 5, T(0x8aa0b0, 0.4));
    line(ctx, [[wx + 3, wy - 3], [wx + 7, wy - 7]], '#5a6a78', 1);
  }
  if (stage === 2) {
    for (let i = 0; i < 4; i++) {
      const [x, y] = ip(o, 30 - i * 2, -20 + i * 13);
      flat(ctx, ellipseP(ctx, x, y, 4, 2), '#b8b0a0', '#6a6258', 0.4);
    }
    sparkleDots(ctx, o, r, 3, LEY, 22);
  }
}

// ── 月井 moon well + beast shelter ──────────────────────────────────────

function wellRing(ctx: Ctx, x: number, gy: number, rr: number, h: number, col: number, water: string | null, broken = false, r?: Rand): void {
  const t = T(col, 0.3);
  cylinder(ctx, x, gy, rr, h, t, { topFill: t.light });
  flat(ctx, ellipseP(ctx, x, gy - h, rr - 2.5, (rr - 2.5) * 0.5), water ?? '#2a2226', t.line, 0.6);
  // Stone joints on the ring face.
  for (let i = 0; i < 7; i++) {
    const a = Math.PI * (0.08 + (i / 6) * 0.84);
    const px = x + Math.cos(a) * rr;
    const py = gy + Math.sin(a) * rr * 0.5;
    line(ctx, [[px, py - h], [px, py]], t.shade, 0.6);
  }
  ctx.beginPath();
  ctx.ellipse(x, gy - h / 2, rr, rr * 0.5, 0, 0, Math.PI);
  ctx.strokeStyle = t.shade;
  ctx.lineWidth = 0.6;
  ctx.stroke();
  if (broken && r) rubble(ctx, x + rr * 0.6, gy + 3, 12, 4, col, r, 0.8);
}

function drawPetHouse(ctx: Ctx, o: Iso, r: Rand, stage: number): void {
  const [wx, wy] = ip(o, -8, 10);
  if (stage === 0) {
    ruinGround(ctx, o, r);
    // Collapsed lean-to (charred beams) at the back.
    beam(ctx, ip(o, 8, -28, 0), ip(o, 18, -10, 16), 1.8, WOOD_DARK, true);
    beam(ctx, ip(o, 26, -28, 0), ip(o, 20, -12, 12), 1.8, WOOD_DARK, true);
    beam(ctx, ip(o, 4, -18, 1), ip(o, 30, -18, 3), 1.6, CHAR, true);
    // Dry, broken well ring.
    wellRing(ctx, wx, wy, 12, 6, STONE_OLD, null, true, r);
    box(ctx, o, -22, -18, 2, 8, 0, 4, T(STONE_OLD));
    weeds(ctx, o, r, 4, 0x7a7a40);
    return;
  }
  footprint(ctx, o, HALF, stage === 2 ? 0x6a7a5a : 0x6a6450, 0.85, true, r);
  // Beast shelter (hutch with roof) at the back right.
  const wood = T(stage === 2 ? 0xa07048 : WOOD, 0.3);
  box(ctx, o, 6, 30, -30, -8, 0, 14, wood, { top: false });
  // Arched opening on the front-left face.
  const [dx, dy] = ip(o, 18, -8, 0);
  flat(ctx, () => { ctx.moveTo(dx - 7, dy + 3.5); ctx.lineTo(dx - 7, dy - 5); ctx.quadraticCurveTo(dx, dy - 13, dx + 7, dy - 8.5); ctx.lineTo(dx + 7, dy - 3.5); ctx.closePath(); }, '#2a1c16', wood.line, 0.6);
  // Straw bedding.
  flat(ctx, ellipseP(ctx, dx, dy - 0.5, 6, 2.2), '#d8b860', '#8a6a2a', 0.4);
  if (stage === 2) {
    flat(ctx, ellipseP(ctx, dx + 1, dy - 1.5, 4, 1.8), '#8a9ad8', '#3a4478', 0.4); // cushion
  }
  gable(ctx, o, 4, 32, -32, -6, 14, 10, T(stage === 2 ? 0x4a6aa8 : 0x6a7890, 0.3), true, 2);
  // The well.
  const water = stage === 2 ? '#bfe4ff' : '#4a7ab0';
  wellRing(ctx, wx, wy, 13, 8, stage === 2 ? STONE : 0xb8b0a2, water);
  if (stage === 2) {
    glow(ctx, { x: wx, y: wy - 9 }, 13, 0x9fd8ff, 0.4);
    // Moon ripples.
    ctx.strokeStyle = 'rgba(255,255,255,0.8)';
    ctx.lineWidth = 0.6;
    ctx.beginPath();
    ctx.ellipse(wx, wy - 8, 6, 3, 0, 0, Math.PI * 2);
    ctx.stroke();
    // Crescent-moon arch over the well with a hanging lantern.
    const arch = T(0xc8d0e0, 0.4);
    line(ctx, [[wx - 13, wy - 8], [wx - 13, wy - 34]], arch.line, 2.6);
    line(ctx, [[wx - 13, wy - 8], [wx - 13, wy - 34]], arch.base, 1.5);
    line(ctx, [[wx + 13, wy - 8], [wx + 13, wy - 34]], arch.line, 2.6);
    line(ctx, [[wx + 13, wy - 8], [wx + 13, wy - 34]], arch.base, 1.5);
    curve(ctx, [wx - 14, wy - 33], [wx, wy - 46], [wx + 14, wy - 33], arch.line, 2.6);
    curve(ctx, [wx - 14, wy - 33], [wx, wy - 46], [wx + 14, wy - 33], arch.base, 1.5);
    // Crescent ornament.
    ctx.save();
    ctx.fillStyle = '#f4f0d0';
    ctx.beginPath();
    ctx.arc(wx, wy - 44, 4.5, 0, Math.PI * 2);
    ctx.arc(wx + 2, wy - 45.5, 3.8, 0, Math.PI * 2, true);
    ctx.fill('evenodd');
    ctx.restore();
    glow(ctx, { x: wx, y: wy - 44 }, 8, 0xdff0ff, 0.5);
    line(ctx, [[wx, wy - 40], [wx, wy - 30]], '#6a6a78', 0.5);
    lantern(ctx, wx, wy - 27, 0x9fd8ff, 0.9);
    // Moonflowers.
    for (const [a, b] of [[-28, 22], [-26, 28], [6, 28], [26, 12]] as const) {
      const [x, y] = ip(o, a, b);
      herb(ctx, x, y, 5, 0x5a8a6a, r, '#e8f0ff');
    }
    sparkleDots(ctx, o, r, 4, 0xbfe6ff, 30);
  } else {
    // Rope and bucket.
    line(ctx, [[wx - 11, wy - 8], [wx - 11, wy - 22]], '#6a4a2a', 1.4);
    line(ctx, [[wx + 11, wy - 8], [wx + 11, wy - 22]], '#6a4a2a', 1.4);
    line(ctx, [[wx - 12, wy - 22], [wx + 12, wy - 22]], '#6a4a2a', 1.6);
    const [bx, by] = ip(o, 14, 20);
    cylinder(ctx, bx, by, 3, 4, T(0x8a6a4a));
  }
  // Water bowl.
  const [qx, qy] = ip(o, 24, 4);
  flat(ctx, ellipseP(ctx, qx, qy, 4, 2), '#8a8a90', '#4a4a50', 0.5);
  flat(ctx, ellipseP(ctx, qx, qy - 0.4, 2.8, 1.2), stage === 2 ? '#bfe4ff' : '#5a8ac0');
}

// ── 炉台 dwarven forge hearth ────────────────────────────────────────────

function anvil(ctx: Ctx, x: number, gy: number, tipped = false): void {
  const t = T(tipped ? 0x6a6060 : 0x5a6070, 0.35);
  if (tipped) {
    shade(ctx, polyP(ctx, [[x - 8, gy], [x - 6, gy - 5], [x + 6, gy - 7], [x + 9, gy - 2], [x + 5, gy + 1]]), t, { band: 2, hi: 0.6, stroke: 0.6 });
    return;
  }
  // Stump.
  cylinder(ctx, x, gy, 5, 6, T(WOOD_DARK, 0.3), { topFill: '#b08a5a' });
  shade(ctx, polyP(ctx, [[x - 4, gy - 6], [x - 3, gy - 9], [x - 9, gy - 11], [x - 6, gy - 13], [x + 7, gy - 13], [x + 8, gy - 11], [x + 3, gy - 9], [x + 4, gy - 6]]), t, { band: 1.8, hi: 0.8, stroke: 0.6 });
}

function drawForge(ctx: Ctx, o: Iso, r: Rand, stage: number): void {
  if (stage === 0) {
    ruinGround(ctx, o, r);
    brokenWall(ctx, o, -28, 4, -28, -8, 16, STONE_OLD, r);
    scorch(ctx, ...ip(o, -12, -18), 14, 0.6);
    const [ax, ay] = ip(o, 14, 14);
    anvil(ctx, ax, ay, true);
    rubble(ctx, ...ip(o, -6, 14), 20, 6, STONE_GREY, r, 0.9);
    beam(ctx, ip(o, 20, -24, 0), ip(o, 30, 0, 2), 1.6, CHAR, true);
    return;
  }
  footprint(ctx, o, HALF, 0x6a625a, 0.9, true, r);
  const stone = T(stage === 2 ? 0x9a92a0 : 0x8a847e, 0.3);
  const hH = stage === 2 ? 22 : 18;
  // Chimney at the back.
  box(ctx, o, -24, -12, -28, -18, 0, stage === 2 ? 58 : 48, stone);
  faceCourses(ctx, ip(o, -24, -18), ip(o, -12, -18), 0, stage === 2 ? 58 : 48, 6, stone.shade, r, false);
  if (stage === 2) glow(ctx, { x: ip(o, -18, -23, 60)[0], y: ip(o, -18, -23, 60)[1] }, 10, EMBER, 0.4);
  // Hearth block.
  box(ctx, o, -28, 6, -24, -4, 0, hH, stone);
  faceCourses(ctx, ip(o, -28, -4), ip(o, 6, -4), 0, hH, 6, stone.shade, r);
  // Fire mouth on the front-left face.
  const [fx, fy] = ip(o, -11, -4, 0);
  flat(ctx, () => { ctx.moveTo(fx - 9, fy + 4.5 - 3); ctx.lineTo(fx - 9, fy - 6); ctx.quadraticCurveTo(fx, fy - 17, fx + 9, fy - 10.5); ctx.lineTo(fx + 9, fy - 1.5); ctx.closePath(); }, '#2a1410', stone.line, 0.7);
  coals(ctx, fx, fy - 1, 12, r, stage === 2 ? 1.2 : 0.9);
  drawFlame(ctx, fx, fy - 1, stage === 2 ? 10 : 7, stage === 2 ? 14 : 9, 0.7, EMBER);
  if (stage === 2) {
    // Brass trim and a rune plate over the mouth.
    line(ctx, [ip(o, -28, -4, hH - 2), ip(o, 6, -4, hH - 2)], GOLD_CSS, 1.1);
    const [rx, ry] = ip(o, -11, -4, hH - 6);
    flat(ctx, polyP(ctx, [[rx - 4, ry - 2], [rx + 4, ry - 6], [rx + 4, ry - 1], [rx - 4, ry + 3]]), '#c8a048', '#6a4a18', 0.5);
    glow(ctx, { x: rx, y: ry - 1.5 }, 5, EMBER, 0.5);
  }
  // Bellows beside the hearth.
  const [bx, by] = ip(o, 12, -16);
  shade(ctx, polyP(ctx, [[bx - 7, by], [bx - 4, by - 7], [bx + 6, by - 5], [bx + 7, by + 1]]), T(0x7a4a2a, 0.3), { band: 2, hi: 0.6, stroke: 0.6 });
  line(ctx, [[bx + 6, by - 2], [bx + 12, by - 4]], '#4a3020', 1.2);
  // Anvil in front.
  const [ax, ay] = ip(o, 14, 16);
  anvil(ctx, ax, ay);
  if (stage === 2) {
    // Gem rack with glowing crystals.
    const [gx, gy] = ip(o, -20, 20);
    box(ctx, o, -28, -12, 16, 22, 0, 12, T(0x7a5234));
    const cols = [0xff4a6a, 0x4ac8ff, 0x7aff8a, 0xffd04a, 0xc07aff];
    cols.forEach((c, i) => {
      const x = gx - 8 + i * 4.2;
      const y = gy - 12 - (i % 2);
      glow(ctx, { x, y: y - 2 }, 4, c, 0.5);
      flat(ctx, polyP(ctx, [[x - 1.6, y], [x, y - 5], [x + 1.6, y]]), rgbaHex(c, 1), '#2a1830', 0.4);
    });
    // Dwarven banner on the chimney.
    const [cbx, cby] = ip(o, -12, -23, 40);
    hangBanner(ctx, cbx + 4, cby, 9, 16, 0x2a4a8a, (x, y) => {
      flat(ctx, polyP(ctx, [[x - 3, y + 2], [x, y - 3], [x + 3, y + 2]]), '#e8c060');
    });
    lantern(ctx, ...ip(o, 8, -4, 20), 0xffb060, 0.9);
    // Sparks.
    for (let i = 0; i < 6; i++) {
      const x = ax + (r() - 0.5) * 16;
      const y = ay - 14 - r() * 12;
      flat(ctx, ellipseP(ctx, x, y, 0.9, 0.9), i % 2 ? '#ffd070' : '#ff8a30');
    }
    glow(ctx, { x: ax, y: ay - 13 }, 7, EMBER, 0.4);
  } else {
    const [bux, buy] = ip(o, 26, 2);
    cylinder(ctx, bux, buy, 3.5, 5, T(0x6a6a70), { topFill: '#3a5a7a' });
  }
}

const GOLD_CSS = '#e8c060';

// ── 商队驿站 caravan post ────────────────────────────────────────────────

function tent(ctx: Ctx, o: Iso, a0: number, a1: number, b0: number, b1: number, rise: number, col: number, stripe?: number): void {
  const bm = (b0 + b1) / 2;
  const t = T(col, 0.3);
  // Back plane (visible sliver) → gable end → front plane.
  const ridge0 = ip(o, a0, bm, rise), ridge1 = ip(o, a1, bm, rise);
  const endTri = [ip(o, a1, b0, 0), ip(o, a1, b1, 0), ridge1];
  flat(ctx, polyP(ctx, endTri), t.shade, t.line, 0.7);
  // Door flap on the gable end.
  const [dx, dy] = ip(o, a1, bm, 0);
  flat(ctx, polyP(ctx, [[dx - 5, dy + 2.5], [dx, dy - rise * 0.6], [dx + 5, dy - 2.5]]), '#2a1c16', t.line, 0.5);
  const front = [ridge0, ridge1, ip(o, a1, b1, 0), ip(o, a0, b1, 0)];
  shade(ctx, polyP(ctx, front), t, { band: 3, hi: 1.2, stroke: 0.8 });
  if (stripe !== undefined) {
    const s = T(stripe, 0.3);
    for (let i = 0; i < 4; i++) {
      const k0 = (i * 2 + 0.5) / 8, k1 = (i * 2 + 1.5) / 8;
      const aa0 = a0 + (a1 - a0) * k0, aa1 = a0 + (a1 - a0) * k1;
      flat(ctx, polyP(ctx, [ip(o, aa0, bm, rise), ip(o, aa1, bm, rise), ip(o, aa1, b1, 0), ip(o, aa0, b1, 0)]), s.base, s.line, 0.4);
    }
  }
  line(ctx, [ridge0, ridge1], t.line, 1.2);
}

function camel(ctx: Ctx, x: number, gy: number): void {
  const t = T(0xc8a06a, 0.35);
  // Lying camel: body, hump, folded legs, neck and head.
  shade(ctx, ellipseP(ctx, x, gy - 6, 12, 6), t, { band: 3, hi: 1, stroke: 0.7 });
  shade(ctx, ellipseP(ctx, x - 1, gy - 12, 6, 4.5), t, { band: 2, hi: 1, stroke: 0.7 });
  flat(ctx, ellipseP(ctx, x - 1, gy - 13, 5.4, 2.4), '#b0304a', '#5a1020', 0.5); // saddle rug
  shade(ctx, () => { ctx.moveTo(x + 8, gy - 8); ctx.quadraticCurveTo(x + 16, gy - 12, x + 15, gy - 20); ctx.lineTo(x + 18, gy - 21); ctx.quadraticCurveTo(x + 20, gy - 12, x + 12, gy - 4); ctx.closePath(); }, t, { band: 1.5, hi: 0.6, stroke: 0.7 });
  shade(ctx, ellipseP(ctx, x + 18, gy - 21, 4, 2.6, 0.2), t, { band: 1.2, hi: 0.6, stroke: 0.7 });
  flat(ctx, ellipseP(ctx, x + 18.5, gy - 22, 0.7, 0.7), '#1a1410');
  for (const dx of [-8, 3]) flat(ctx, ellipseP(ctx, x + dx, gy - 0.5, 4, 1.6), t.shade, t.line, 0.5);
}

function drawCaravan(ctx: Ctx, o: Iso, r: Rand, stage: number): void {
  if (stage === 0) {
    ruinGround(ctx, o, r);
    // Burnt tent frame with torn canvas.
    beam(ctx, ip(o, -24, -16, 0), ip(o, -10, -6, 22), 1.4, CHAR, true);
    beam(ctx, ip(o, -24, 4, 0), ip(o, -10, -6, 22), 1.4, CHAR, true);
    beam(ctx, ip(o, -10, -6, 22), ip(o, 14, -6, 12), 1.3, CHAR, true);
    const [tx, ty] = ip(o, -16, -2, 8);
    flat(ctx, polyP(ctx, [[tx - 6, ty - 6], [tx + 6, ty - 10], [tx + 8, ty + 2], [tx + 2, ty], [tx - 4, ty + 4]]), '#8a7a60', '#3a2c20', 0.5);
    // Broken crates and a charred wheel.
    box(ctx, o, 12, 22, 12, 22, 0, 5, T(CHAR, 0.2));
    const [wx, wy] = ip(o, 22, -14);
    ctx.beginPath();
    ctx.ellipse(wx, wy - 7, 5, 7, 0.3, 0, Math.PI * 2);
    ctx.strokeStyle = '#2a2020';
    ctx.lineWidth = 1.6;
    ctx.stroke();
    line(ctx, [[wx - 3, wy - 12], [wx + 3, wy - 2]], '#2a2020', 1);
    rubble(ctx, ...ip(o, 0, 22), 16, 4, STONE_GREY, r, 0.7);
    return;
  }
  footprint(ctx, o, HALF, 0x8a7a58, 0.85);
  if (stage === 2) {
    // Rug in front of the tent.
    const rug = [ip(o, 8, 4, 0.3), ip(o, 26, 4, 0.3), ip(o, 26, 22, 0.3), ip(o, 8, 22, 0.3)];
    flat(ctx, polyP(ctx, rug), '#a8344a', '#5a1424', 0.6);
    flat(ctx, polyP(ctx, [ip(o, 11, 7, 0.4), ip(o, 23, 7, 0.4), ip(o, 23, 19, 0.4), ip(o, 11, 19, 0.4)]), '#e8b048', '#8a5a20', 0.4);
  }
  tent(ctx, o, -28, 4, -28, 2, stage === 2 ? 30 : 26, stage === 2 ? 0xeee0c0 : 0xd8c8a0, stage === 2 ? 0xc04a3a : undefined);
  if (stage === 2) {
    pennant(ctx, ...ip(o, 6, -28), 36, 0xc04a3a, 1.5);
    pennant(ctx, ...ip(o, -28, 4), 30, 0x3a6ac0, -1);
    lantern(ctx, ...ip(o, 6, -8, 18), 0xffc060, 0.9);
    camel(ctx, ...ip(o, 20, -18));
  }
  // Goods.
  crate(ctx, o, 22, 16, 4.5);
  crate(ctx, o, 26, 26, 4);
  if (stage === 2) crate(ctx, o, 22, 16, 3.5, 0xb08050, 9);
  const [bx, by] = ip(o, 12, 26);
  barrel(ctx, bx, by);
  if (stage === 2) {
    sack(ctx, ...ip(o, -6, 22), 1);
    sack(ctx, ...ip(o, 2, 28), 0.9, 0xd8c090);
  } else {
    // Hitching post.
    const [hx, hy] = ip(o, 18, -18);
    box(ctx, o, 17, 19, -19, -17, 0, 12, T(WOOD));
    box(ctx, o, 17, 19, -9, -7, 0, 12, T(WOOD));
    line(ctx, [[hx, hy - 10], ip(o, 18, -8, 10)], '#5a3a20', 1.4);
  }
}

// ── 心焰祭坛 heart-flame altar ───────────────────────────────────────────

function heartFlame(ctx: Ctx, x: number, by: number, s: number): void {
  glow(ctx, { x, y: by - 10 * s }, 24 * s, 0xffa040, 0.6);
  drawFlame(ctx, x, by, 14 * s, 26 * s, 1.1, 0xffa030);
  // Heart-shaped core.
  ctx.save();
  ctx.translate(x, by - 9 * s);
  ctx.scale(s, s);
  ctx.beginPath();
  ctx.moveTo(0, 4);
  ctx.bezierCurveTo(-6, -1, -4, -6, 0, -3);
  ctx.bezierCurveTo(4, -6, 6, -1, 0, 4);
  ctx.fillStyle = '#fff2b0';
  ctx.fill();
  ctx.restore();
}

function drawAltar(ctx: Ctx, o: Iso, r: Rand, stage: number): void {
  const stone = T(stage === 2 ? STONE : stage === 1 ? 0xc8c0b2 : STONE_OLD, 0.3);
  if (stage === 0) {
    ruinGround(ctx, o, r);
    slab(ctx, o, -20, 20, -20, 20, 3, stone);
    // Toppled slab and broken pillars.
    box(ctx, o, -26, -18, -26, -18, 0, 10, stone);
    box(ctx, o, 18, 26, -26, -18, 0, 6, stone);
    const [px, py] = ip(o, -2, 2, 3);
    shade(ctx, polyP(ctx, [[px - 14, py + 2], [px - 10, py - 6], [px + 12, py - 2], [px + 10, py + 6]]), stone, { band: 3, hi: 1, stroke: 0.7 });
    // Cold ash bowl.
    const [bx, by] = ip(o, 14, 14);
    cylinder(ctx, bx, by, 5, 3, T(0x5a5450), { topFill: '#8a8480' });
    rubble(ctx, ...ip(o, 20, -4), 16, 5, STONE_OLD, r, 0.8);
    return;
  }
  footprint(ctx, o, HALF, stage === 2 ? 0x8a8070 : 0x7a7264, 0.9, true, r);
  // Back pillars.
  const pillar = (a: number, b: number, h: number): void => {
    box(ctx, o, a - 3.5, a + 3.5, b - 3.5, b + 3.5, 0, h, stone);
    box(ctx, o, a - 4.5, a + 4.5, b - 4.5, b + 4.5, h, h + 3, stone);
    if (stage === 2) {
      const [x, y] = ip(o, a, b, h + 3);
      drawFlame(ctx, x, y, 5, 8, 0.4 + a, EMBER);
    }
  };
  pillar(24, -24, stage === 2 ? 30 : 24);
  if (stage === 2) {
    const [x0, y0] = ip(o, 24, -24, 24);
    hangBanner(ctx, x0 + 7, y0 + 2, 8, 16, 0xa8303a, (x, y) => heartMini(ctx, x, y));
  }
  // Steps and altar block.
  slab(ctx, o, -22, 22, -22, 22, 3, stone);
  slab(ctx, o, -16, 16, -16, 16, 6, stone);
  box(ctx, o, -9, 9, -9, 9, 6, 20, stone);
  if (stage === 2) {
    // Glowing runes on the steps and the block.
    for (const [a0, b0] of [[-22, 22], [-16, 16]] as const) {
      for (let i = 1; i < 5; i++) {
        const [x, y] = ip(o, a0 + (i / 5) * (-2 * a0), b0, a0 === -22 ? 1.5 : 4.5);
        flat(ctx, polyP(ctx, [[x - 1.5, y], [x, y - 1.2], [x + 1.5, y], [x, y + 1.2]]), '#ffb050');
        glow(ctx, { x, y }, 3, EMBER, 0.45);
      }
    }
    line(ctx, [ip(o, -9, 9, 18), ip(o, 9, 9, 18), ip(o, 9, -9, 18)], GOLD_CSS, 1);
  }
  // Brazier bowl on top.
  const [bx, by] = ip(o, 0, 0, 20);
  cylinder(ctx, bx, by, 7, 4, T(0x6a5040, 0.3), { rTop: 9, topFill: '#2a1a14' });
  if (stage === 2) heartFlame(ctx, bx, by - 3, 1.1);
  else drawFlame(ctx, bx, by - 3, 8, 12, 0.9, EMBER);
  pillar(-24, 24, stage === 2 ? 30 : 24);
  if (stage === 2) {
    const [x0, y0] = ip(o, -24, 24, 24);
    hangBanner(ctx, x0 + 7, y0 + 2, 8, 14, 0xa8303a, (x, y) => heartMini(ctx, x, y));
    // Floating embers.
    for (let i = 0; i < 7; i++) {
      const x = bx + (r() - 0.5) * 30;
      const y = by - 20 - r() * 30;
      glow(ctx, { x, y }, 3, EMBER, 0.6);
      flat(ctx, ellipseP(ctx, x, y, 0.9, 0.9), '#ffe0a0');
    }
  }
}

function heartMini(ctx: Ctx, x: number, y: number): void {
  ctx.beginPath();
  ctx.moveTo(x, y + 3);
  ctx.bezierCurveTo(x - 4.5, y - 0.5, x - 3, y - 4.5, x, y - 2);
  ctx.bezierCurveTo(x + 3, y - 4.5, x + 4.5, y - 0.5, x, y + 3);
  ctx.fillStyle = '#ffc060';
  ctx.fill();
}

// ── 仓库 warehouse ──────────────────────────────────────────────────────

function drawWarehouse(ctx: Ctx, o: Iso, r: Rand, stage: number): void {
  if (stage === 0) {
    ruinGround(ctx, o, r);
    brokenWall(ctx, o, -28, 18, -30, -26, 18, 0x7a6a58, r);
    brokenWall(ctx, o, -30, -26, -26, 12, 14, 0x7a6a58, r);
    beam(ctx, ip(o, -20, -20, 18), ip(o, 12, 6, 0), 1.8, CHAR, true);
    beam(ctx, ip(o, -26, 4, 12), ip(o, 4, 16, 0), 1.6, CHAR, true);
    sack(ctx, ...ip(o, 12, 20), 1, 0x8a7a5a);
    sack(ctx, ...ip(o, 20, 14), 0.8, 0x7a6a50);
    rubble(ctx, ...ip(o, 0, 0), 26, 7, STONE_GREY, r, 0.8);
    return;
  }
  footprint(ctx, o, HALF, 0x6a6050, 0.85, true, r);
  const wall = T(stage === 2 ? 0xa8764a : WOOD, 0.3);
  const H = stage === 2 ? 28 : 24;
  if (stage === 2) {
    box(ctx, o, -30, 18, -30, 8, 0, 7, T(STONE_OLD, 0.3));
    box(ctx, o, -28, 16, -28, 6, 7, H, wall, { top: false });
  } else {
    box(ctx, o, -28, 16, -28, 6, 0, H, wall, { top: false });
  }
  // Plank lines on both faces.
  for (let i = 1; i < 8; i++) {
    const a = -28 + (44 * i) / 8;
    line(ctx, [ip(o, a, 6, stage === 2 ? 7 : 0), ip(o, a, 6, H)], wall.shade, 0.6);
  }
  for (let i = 1; i < 6; i++) {
    const b = -28 + (34 * i) / 6;
    line(ctx, [ip(o, 16, b, stage === 2 ? 7 : 0), ip(o, 16, b, H)], wall.line, 0.5);
  }
  // Door on the front-left face.
  const d = [ip(o, -12, 6, 0), ip(o, 2, 6, 0), ip(o, 2, 6, 16), ip(o, -12, 6, 16)];
  flat(ctx, polyP(ctx, d), '#4a2c18', wall.line, 0.7);
  line(ctx, [ip(o, -12, 6, 16), ip(o, 2, 6, 0)], '#2a1a10', 0.7);
  line(ctx, [ip(o, -12, 6, 0), ip(o, 2, 6, 16)], '#2a1a10', 0.7);
  const roof = T(stage === 2 ? 0xb04a3a : 0x7a6a5a, 0.3);
  gable(ctx, o, -30, 18, -30, 8, H, 14, roof, true, 3);
  // Roof courses.
  const e0 = ip(o, -33, 11, H - 1), e1 = ip(o, 18.9, 11, H - 1), r0 = ip(o, -33, -11, H + 14), r1 = ip(o, 18.9, -11, H + 14);
  for (let i = 1; i < 4; i++) {
    const k = i / 4;
    line(ctx, [[e0[0] + (r0[0] - e0[0]) * k, e0[1] + (r0[1] - e0[1]) * k], [e1[0] + (r1[0] - e1[0]) * k, e1[1] + (r1[1] - e1[1]) * k]], roof.shade, 0.7);
  }
  // Goods out front.
  crate(ctx, o, 26, 18, 4.5);
  crate(ctx, o, 24, 28, 4);
  const [bx, by] = ip(o, 8, 26);
  barrel(ctx, bx, by);
  if (stage === 2) {
    crate(ctx, o, 26, 18, 3.5, 0xb08050, 9);
    const [b2x, b2y] = ip(o, 14, 30);
    barrel(ctx, b2x, b2y, 4, 9, 0x7a4a2a);
    sack(ctx, ...ip(o, -20, 22), 1);
    sack(ctx, ...ip(o, -12, 26), 0.9, 0xd8c090);
    lantern(ctx, ...ip(o, 4, 6, 20), 0xffc060, 0.9);
    const [hx, hy] = ip(o, 16, -10, H - 2);
    hangBanner(ctx, hx + 2, hy, 8, 13, 0x3a6a3a, (x, y) => flat(ctx, ellipseP(ctx, x, y, 2, 2), GOLD_CSS));
  }
}

// ── Tower centrepiece ───────────────────────────────────────────────────

function drawTower(ctx: Ctx, cx: number, gy: number, r: Rand): void {
  const o: Iso = { cx, gy };
  const H3 = 48; // 3×3 footprint half size
  // Flagstone court with ley-line runes.
  footprint(ctx, o, H3, 0x8a8472, 0.9, true, r);
  for (const [a, b] of [[H3, 0], [-H3, 0], [0, H3], [0, -H3]] as const) {
    const p0 = ip(o, a * 0.45, b * 0.45), p1 = ip(o, a * 0.95, b * 0.95);
    line(ctx, [p0, p1], rgbaHex(LEY, 0.5), 2.4);
    line(ctx, [p0, p1], '#f4ffc8', 0.8);
    glow(ctx, { x: p1[0], y: p1[1] }, 6, LEY, 0.5);
  }
  rubble(ctx, cx - 58, gy + 6, 24, 6, STONE, r, 1);
  rubble(ctx, cx + 54, gy - 12, 18, 4, STONE, r, 1.1);
  // Stepped round plinth.
  const white = T(STONE, 0.25, 0.38);
  cylinder(ctx, cx, gy + 4, 62, 6, T(0xcfc8b8, 0.25));
  cylinder(ctx, cx, gy, 52, 8, white);
  // Rune ring on the plinth face.
  for (let i = 0; i < 11; i++) {
    const a = Math.PI * (0.1 + (i / 10) * 0.8);
    const x = cx + Math.cos(a) * 52, y = gy + Math.sin(a) * 26 - 4;
    flat(ctx, polyP(ctx, [[x - 1.8, y], [x, y - 2], [x + 1.8, y], [x, y + 2]]), '#e8ffb0');
    glow(ctx, { x, y }, 4, LEY, 0.45);
  }
  // Shaft.
  const base = gy - 8;
  const R0 = 38, R1 = 31, top = 196;
  const shaft = (): void => {
    ctx.moveTo(cx - R0, base);
    ctx.lineTo(cx - R1, base - top);
    ctx.lineTo(cx + R1, base - top);
    ctx.lineTo(cx + R0, base);
    ctx.ellipse(cx, base, R0, R0 * 0.5, 0, 0, Math.PI);
    ctx.closePath();
  };
  shade(ctx, shaft, white, { band: 24, hi: 4, stroke: 1 });
  // Block courses (curved) and staggered joints.
  const rad = (h: number): number => R0 + (R1 - R0) * (h / top);
  for (let h = 12, row = 0; h < top; h += 12, row++) {
    const rr = rad(h);
    ctx.beginPath();
    ctx.ellipse(cx, base - h, rr, rr * 0.5, 0, 0.05, Math.PI - 0.05);
    ctx.strokeStyle = 'rgba(120,108,96,0.55)';
    ctx.lineWidth = 0.7;
    ctx.stroke();
    for (let j = 0; j < 6; j++) {
      const a = Math.PI * ((j + (row % 2 ? 0.5 : 0) + 0.3) / 6.3);
      const x = cx + Math.cos(a) * rr;
      const y = base - h + Math.sin(a) * rr * 0.5;
      line(ctx, [[x, y], [x, y - 12]], 'rgba(120,108,96,0.4)', 0.6);
    }
  }
  // Gold band trims.
  for (const h of [40, 132]) {
    const rr = rad(h) + 0.6;
    ctx.beginPath();
    ctx.ellipse(cx, base - h, rr, rr * 0.5, 0, 0, Math.PI);
    ctx.strokeStyle = '#8a6a20';
    ctx.lineWidth = 2.4;
    ctx.stroke();
    ctx.strokeStyle = GOLD_CSS;
    ctx.lineWidth = 1.3;
    ctx.stroke();
  }
  // Slender elven buttress ribs.
  for (const s of [-1, 1]) {
    const x0 = cx + s * R0 * 0.72, x1 = cx + s * R1 * 0.72;
    const t = s < 0 ? T(0xf0ece2, 0.3) : T(0xc8c0b0, 0.3);
    shade(ctx, polyP(ctx, [[x0 - 3, base + 14], [x1 - 2, base - top + 8], [x1 + 2, base - top + 8], [x0 + 3, base + 14]]), t, { band: 2, hi: 1, stroke: 0.7 });
  }
  // Arched windows.
  const arch = (x: number, y: number, w: number, h: number, lit: number): void => {
    flat(ctx, () => { ctx.moveTo(x - w / 2, y); ctx.lineTo(x - w / 2, y - h + w / 2); ctx.arc(x, y - h + w / 2, w / 2, Math.PI, 0); ctx.lineTo(x + w / 2, y); ctx.closePath(); }, '#2a1c20', '#6a5a50', 0.8);
    if (lit > 0) {
      glow(ctx, { x, y: y - h * 0.4 }, w * 1.4, EMBER, 0.35 * lit);
      flat(ctx, () => { ctx.moveTo(x - w / 2 + 1, y - 1); ctx.lineTo(x - w / 2 + 1, y - h * 0.45); ctx.lineTo(x + w / 2 - 1, y - h * 0.3); ctx.lineTo(x + w / 2 - 1, y - 1); ctx.closePath(); }, rgbaHex(0xff9a40, 0.55 * lit));
    }
  };
  arch(cx - 14, base - 150, 8, 18, 0.5);
  arch(cx + 10, base - 162, 7, 16, 0.3);
  arch(cx - 6, base - 44, 7, 14, 0);
  // The heart: a great arched breach with the ember core.
  const hx = cx + 4, hy = base - 74;
  glow(ctx, { x: hx, y: hy - 16 }, 46, 0xff7a20, 0.45);
  flat(ctx, () => { ctx.moveTo(hx - 13, hy); ctx.lineTo(hx - 14, hy - 22); ctx.quadraticCurveTo(hx - 12, hy - 42, hx, hy - 44); ctx.quadraticCurveTo(hx + 12, hy - 42, hx + 13, hy - 22); ctx.lineTo(hx + 12, hy); ctx.closePath(); }, '#3a1410', '#6a4a40', 1);
  // Inner glow and the floating ember heart.
  ctx.save();
  ctx.beginPath();
  ctx.moveTo(hx - 13, hy); ctx.lineTo(hx - 14, hy - 22); ctx.quadraticCurveTo(hx - 12, hy - 42, hx, hy - 44); ctx.quadraticCurveTo(hx + 12, hy - 42, hx + 13, hy - 22); ctx.lineTo(hx + 12, hy); ctx.closePath();
  ctx.clip();
  const g = ctx.createRadialGradient(hx, hy - 18, 0, hx, hy - 18, 24);
  g.addColorStop(0, 'rgba(255,220,140,1)');
  g.addColorStop(0.35, 'rgba(255,130,40,0.9)');
  g.addColorStop(1, 'rgba(90,20,10,0.9)');
  ctx.fillStyle = g;
  ctx.fillRect(hx - 20, hy - 50, 40, 52);
  ctx.restore();
  drawFlame(ctx, hx, hy - 4, 12, 26, 0.6, 0xff8a2a);
  const hs = 1.3;
  ctx.save();
  ctx.translate(hx, hy - 20);
  ctx.scale(hs, hs);
  ctx.beginPath();
  ctx.moveTo(0, 5);
  ctx.bezierCurveTo(-7, -1, -4.5, -7, 0, -3.5);
  ctx.bezierCurveTo(4.5, -7, 7, -1, 0, 5);
  ctx.fillStyle = '#fff6c8';
  ctx.fill();
  ctx.restore();
  // Cracks radiating from the breach, lit from within.
  for (const [dx, dy, ex, ey] of [[-13, -30, -24, -38], [13, -26, 22, -20], [-12, -8, -20, 2], [12, -40, 18, -54]] as const) {
    line(ctx, [[hx + dx, hy + dy], [hx + (dx + ex) / 2 + 2, hy + (dy + ey) / 2], [hx + ex, hy + ey]], '#5a3020', 1.4);
    line(ctx, [[hx + dx, hy + dy], [hx + (dx + ex) / 2 + 2, hy + (dy + ey) / 2]], '#ffb060', 0.6);
  }
  // Broken crown: jagged rim, one tall spire shard, the far wall fallen.
  const ty = base - top;
  const back = T(0xcfc6b4, 0.25);
  flat(ctx, ellipseP(ctx, cx, ty, R1, R1 * 0.5), '#4a3a36', back.line, 0.8); // hollow interior
  const jag: P2[] = [];
  const n = 14;
  for (let i = 0; i <= n; i++) {
    const a = Math.PI * (i / n);
    const x = cx + Math.cos(a) * R1;
    const y = ty + Math.sin(a) * R1 * 0.5;
    const hgt = [10, 16, 6, 22, 12, 4, 0, 8, 14, 5, 18, 26, 34, 20, 8][i];
    jag.push([x, y - hgt]);
  }
  const rim: P2[] = [[cx + R1, ty], ...jag.slice().reverse().map(q => q).reverse()];
  void rim;
  shade(ctx, () => {
    ctx.moveTo(cx + R1, ty + 2);
    for (const q of jag) ctx.lineTo(q[0], q[1]);
    ctx.lineTo(cx - R1, ty + 2);
    ctx.ellipse(cx, ty + 2, R1, R1 * 0.5, 0, Math.PI, 0, true);
    ctx.closePath();
  }, white, { band: 18, hi: 3, stroke: 1 });
  // Tall spire shard on the left.
  const sx = cx - 22;
  shade(ctx, polyP(ctx, [[sx - 7, ty + 8], [sx - 5, ty - 44], [sx - 1, ty - 60], [sx + 3, ty - 40], [sx + 1, ty - 30], [sx + 6, ty - 26], [sx + 7, ty + 10]]), white, { band: 5, hi: 1.5, stroke: 1 });
  line(ctx, [[sx - 3, ty - 20], [sx - 1, ty - 52]], GOLD_CSS, 0.8);
  // Wisps of ember smoke from the broken top.
  for (let i = 0; i < 6; i++) {
    const x = cx + (r() - 0.5) * 30;
    const y = ty - 6 - r() * 24;
    glow(ctx, { x, y }, 3, EMBER, 0.55);
    flat(ctx, ellipseP(ctx, x, y, 0.9, 0.9), '#ffd8a0');
  }
  // Ivy climbing the lower left.
  for (let i = 0; i < 16; i++) {
    const k = i / 15;
    const x = cx - R0 + 4 + Math.sin(k * 7) * 4 + k * 6;
    const y = base - 2 - k * 70;
    flat(ctx, ellipseP(ctx, x, y, 3, 2, k * 2), i % 2 ? '#5a9a48' : '#3f7a3a', '#1f4020', 0.4);
  }
  // Fallen blocks at the foot.
  for (const [dx, dy, w] of [[-34, 26, 9], [30, 30, 7], [42, 14, 6]] as const) {
    box(ctx, o, dx - w, dx + w, dy - w * 0.7, dy + w * 0.7, 0, w * 1.1, T(0xd8d0c0, 0.25));
  }
}

// ── Drawers ─────────────────────────────────────────────────────────────

const BUILDING_W = 150;
const BUILDING_H = 150;
const BUILDING_GROUND = 110;

const PAINTERS: Record<TowerBuildingId, (ctx: Ctx, o: Iso, r: Rand, stage: number) => void> = {
  herb_garden: drawHerbGarden,
  pet_house: drawPetHouse,
  gem_workshop: drawForge,
  training_ground: drawCaravan,
  altar: drawAltar,
  warehouse: drawWarehouse,
};

function buildingDrawer(id: TowerBuildingId, stage: number): DecorDrawer {
  return defineDecor({
    key: towerBuildingKey(id, stage),
    w: BUILDING_W,
    h: BUILDING_H,
    ground: BUILDING_GROUND,
    tall: stage > 0 && id !== 'herb_garden',
    draw(ctx, { cx, gy }) {
      const r = rng(0x5eed + stage * 101 + id.length * 7);
      contactShadow(ctx, cx + 4, gy + 4, 52, 22, 0.28);
      PAINTERS[id](ctx, { cx, gy }, r, stage);
    },
  });
}

export const TowerMainDrawer = defineDecor({
  key: 'decor_tower_main',
  w: 220,
  h: 340,
  ground: 286,
  tall: true,
  draw(ctx, { cx, gy }) {
    contactShadow(ctx, cx + 6, gy + 6, 84, 36, 0.35);
    drawTower(ctx, cx, gy, rng(0x70e2));
  },
});

const PORTAL_FRAMES = 6;
export const TowerPortalDrawer = defineDecor({
  key: 'decor_tower_portal',
  w: 110,
  h: 70,
  ground: 40,
  flat: true,
  frames: PORTAL_FRAMES,
  loop: { frames: PORTAL_FRAMES, fps: 8 },
  draw(ctx, { cx, gy }, frame) {
    const ph = (frame / PORTAL_FRAMES) * Math.PI * 2;
    const R = 44;
    // Stone ring.
    ctx.save();
    ctx.translate(cx, gy);
    ctx.scale(1, 0.5);
    const ringT = T(0xb8b0a4, 0.3);
    ctx.beginPath();
    ctx.arc(0, 0, R, 0, Math.PI * 2);
    ctx.arc(0, 0, R - 9, 0, Math.PI * 2, true);
    ctx.fillStyle = ringT.base;
    ctx.fill('evenodd');
    ctx.strokeStyle = ringT.line;
    ctx.lineWidth = 1.4;
    ctx.stroke();
    // Joints.
    for (let i = 0; i < 12; i++) {
      const a = (i / 12) * Math.PI * 2;
      ctx.beginPath();
      ctx.moveTo(Math.cos(a) * (R - 9), Math.sin(a) * (R - 9));
      ctx.lineTo(Math.cos(a) * R, Math.sin(a) * R);
      ctx.strokeStyle = ringT.shade;
      ctx.lineWidth = 1;
      ctx.stroke();
    }
    // Inner glowing disc.
    const g = ctx.createRadialGradient(0, 0, 0, 0, 0, R - 9);
    g.addColorStop(0, 'rgba(255,230,160,0.85)');
    g.addColorStop(0.55, 'rgba(255,140,50,0.55)');
    g.addColorStop(1, 'rgba(160,50,20,0.35)');
    ctx.fillStyle = g;
    ctx.beginPath();
    ctx.arc(0, 0, R - 9.5, 0, Math.PI * 2);
    ctx.fill();
    // Turning rune circle.
    ctx.strokeStyle = 'rgba(255,240,190,0.9)';
    ctx.lineWidth = 1.2;
    ctx.beginPath();
    ctx.arc(0, 0, R - 16, 0, Math.PI * 2);
    ctx.stroke();
    for (let i = 0; i < 8; i++) {
      const a = (i / 8) * Math.PI * 2 + ph / 8;
      const x = Math.cos(a) * (R - 16), y = Math.sin(a) * (R - 16);
      ctx.fillStyle = '#fff4c8';
      ctx.beginPath();
      ctx.moveTo(x, y - 3.2);
      ctx.lineTo(x + 2.4, y);
      ctx.lineTo(x, y + 3.2);
      ctx.lineTo(x - 2.4, y);
      ctx.fill();
    }
    // Inner star.
    ctx.strokeStyle = 'rgba(255,220,150,0.8)';
    ctx.lineWidth = 0.9;
    ctx.beginPath();
    for (const off of [0, Math.PI / 3]) {
      for (let i = 0; i <= 3; i++) {
        const a = off + (i / 3) * Math.PI * 2 - ph / 12;
        const x = Math.cos(a) * (R - 20), y = Math.sin(a) * (R - 20);
        if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
      }
    }
    ctx.stroke();
    ctx.restore();
    glow(ctx, { x: cx, y: gy }, 36, EMBER, 0.35 + Math.sin(ph) * 0.06);
    // Rising motes.
    for (let i = 0; i < 6; i++) {
      const k = ((frame / PORTAL_FRAMES) + i / 6) % 1;
      const a = i * 1.7;
      const x = cx + Math.cos(a) * 24 * (1 - k * 0.3);
      const y = gy + Math.sin(a) * 11 - k * 26;
      glow(ctx, { x, y }, 3, 0xffc070, 0.7 * (1 - k));
      flat(ctx, ellipseP(ctx, x, y, 0.9, 0.9), `rgba(255,248,220,${1 - k})`);
    }
  },
});

const HEARTH_FRAMES = 4;
export const TowerHearthstoneDrawer = defineDecor({
  key: 'decor_tower_hearthstone',
  w: 32,
  h: 50,
  ground: 44,
  frames: HEARTH_FRAMES,
  loop: { frames: HEARTH_FRAMES, fps: 5 },
  draw(ctx, { cx, gy, r }, frame) {
    const ph = (frame / HEARTH_FRAMES) * Math.PI * 2;
    contactShadow(ctx, cx + 1, gy, 12, 4, 0.4);
    const stone = T(0x8a8ea0, 0.35);
    // Base stones.
    flat(ctx, ellipseP(ctx, cx - 6, gy - 1, 4, 2.4), stone.shade, stone.line, 0.5);
    flat(ctx, ellipseP(ctx, cx + 6, gy - 0.5, 3.5, 2.2), stone.shade, stone.line, 0.5);
    // Standing rune stone.
    shade(ctx, blobP(ctx, [[cx - 7, gy], [cx - 8, gy - 16], [cx - 6, gy - 30], [cx, gy - 34], [cx + 6, gy - 30], [cx + 8, gy - 14], [cx + 7, gy]]), stone, { band: 4, hi: 1.2, stroke: 0.8 });
    // Carved hearth rune (a flame in a circle).
    const hot = 0.75 + Math.sin(ph) * 0.25;
    const rc = rgbaHex(0xffa040, 0.9 * hot);
    glow(ctx, { x: cx, y: gy - 16 }, 9, EMBER, 0.35 * hot);
    ctx.strokeStyle = rc;
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.arc(cx, gy - 15, 4.2, 0, Math.PI * 2);
    ctx.stroke();
    ctx.beginPath();
    ctx.moveTo(cx, gy - 12);
    ctx.quadraticCurveTo(cx - 2.6, gy - 15, cx, gy - 18.5);
    ctx.quadraticCurveTo(cx + 2.6, gy - 15, cx, gy - 12);
    ctx.stroke();
    // Ember gem set in the crown.
    glow(ctx, { x: cx, y: gy - 29 }, 8 + hot * 2, EMBER, 0.6 * hot);
    flat(ctx, polyP(ctx, [[cx, gy - 33], [cx + 2.6, gy - 29], [cx, gy - 26], [cx - 2.6, gy - 29]]), '#ff9a3a', '#6a2a10', 0.5);
    flat(ctx, polyP(ctx, [[cx - 0.5, gy - 32], [cx + 1, gy - 29.5], [cx - 0.5, gy - 28]]), '#fff0b0');
    // A spark drifting up.
    const k = (frame / HEARTH_FRAMES + r() * 0.2) % 1;
    flat(ctx, ellipseP(ctx, cx + Math.sin(ph) * 2, gy - 34 - k * 10, 0.8, 0.8), `rgba(255,220,150,${1 - k})`);
  },
});

export const TOWER_BUILDING_DRAWERS: readonly DecorDrawer[] = TOWER_BUILDING_IDS.flatMap(id => [0, 1, 2].map(st => buildingDrawer(id, st)));

export const TOWER_PROP_DRAWERS: readonly DecorDrawer[] = [
  TowerMainDrawer,
  TowerPortalDrawer,
  TowerHearthstoneDrawer,
  ...TOWER_BUILDING_DRAWERS,
];

/**
 * Painted icons for the touch HUD's panel / toggle buttons, in the same inked cel-shaded
 * style as the item icons and UI glyphs (docs/art-direction.md). Icons that already exist
 * elsewhere (helm, spellbook, quest marker, sword) are reused; the rest are drawn here.
 *
 *   ensureHudIcon(scene, id) -> texture key (generated on first use).
 */
import Phaser from 'phaser';
import { P, celI, tone, glow, css, roundRectPath, line, inked, sparkle } from '../graphics/icons/IconKit';
import { ensureItemIcon } from '../graphics/icons/ItemIcons';
import { ensureGlyph } from '../graphics/icons/UiGlyphs';

export type HudIconId =
  | 'inventory' | 'character' | 'skills' | 'map' | 'homestead' | 'quest' | 'pets'
  | 'auto' | 'loot' | 'log';

const SIZE = 96;

type Draw = (c: CanvasRenderingContext2D) => void;

const LEATHER = 0x9a6038;
const WOOD = 0xa06d3c;
const PARCHMENT = 0xe6d2a2;

const DRAWN: Partial<Record<HudIconId, Draw>> = {
  // Satchel with a flap and brass buckle
  inventory: (c) => {
    const lt = tone(LEATHER, { light: 0.4 });
    celI(c, () => { c.moveTo(34, 30); c.bezierCurveTo(34, 12, 62, 12, 62, 30); c.lineTo(56, 30); c.bezierCurveTo(56, 20, 40, 20, 40, 30); c.closePath(); }, tone(0x6a3f24));
    celI(c, () => roundRectPath(c, 14, 30, 68, 56, 14), lt, { band: 3.4, hi: 1.3 });
    celI(c, () => { c.moveTo(14, 42); c.quadraticCurveTo(48, 34, 82, 42); c.lineTo(78, 62); c.quadraticCurveTo(48, 70, 18, 62); c.closePath(); }, tone(0x7a4628, { light: 0.35 }), { band: 2.4, hi: 1 });
    celI(c, () => roundRectPath(c, 41, 56, 14, 14, 3), tone(0xe3b44c, { light: 0.5 }), { band: 2, hi: 0.8 });
    line(c, [P(22, 74), P(74, 74)], 1.6, lt.shade);
  },
  // Folded parchment map with a red route and an X
  map: (c) => {
    const pt = tone(PARCHMENT, { light: 0.3 });
    const panel = (x0: number, y0: number, x1: number, y1: number, shade: number): void => {
      celI(c, () => { c.moveTo(x0, y0); c.lineTo(x1, y1); c.lineTo(x1, y1 + 56); c.lineTo(x0, y0 + 56); c.closePath(); }, tone(shade, { light: 0.3 }), { band: 2, hi: 0.8 });
    };
    panel(12, 22, 36, 16, 0xd8c08e);
    panel(36, 16, 60, 22, PARCHMENT);
    panel(60, 22, 84, 16, 0xd8c08e);
    c.save();
    c.setLineDash([4, 4]);
    line(c, [P(20, 62), P(34, 48), P(50, 56), P(66, 40)], 2.4, css(0xb03a2a));
    c.restore();
    line(c, [P(64, 32), P(74, 42)], 3, css(0xb03a2a));
    line(c, [P(74, 32), P(64, 42)], 3, css(0xb03a2a));
    line(c, [P(36, 18), P(36, 72)], 1.2, pt.shade);
    line(c, [P(60, 24), P(60, 78)], 1.2, pt.shade);
  },
  // Cottage: timber walls, red roof, chimney, lit window
  homestead: (c) => {
    celI(c, () => roundRectPath(c, 62, 18, 10, 22, 2), tone(0x8c8c96));
    celI(c, () => roundRectPath(c, 22, 46, 52, 38, 3), tone(WOOD, { light: 0.35 }), { band: 3, hi: 1.2 });
    celI(c, () => { c.moveTo(10, 50); c.lineTo(48, 16); c.lineTo(86, 50); c.lineTo(78, 56); c.lineTo(48, 30); c.lineTo(18, 56); c.closePath(); }, tone(0xc0443a, { light: 0.35 }), { band: 3, hi: 1.2 });
    celI(c, () => roundRectPath(c, 41, 60, 14, 24, 3), tone(0x5c3522));
    celI(c, () => roundRectPath(c, 28, 56, 10, 10, 2), tone(0xffd060, { light: 0.6 }), { band: 1, hi: 0.5 });
    celI(c, () => roundRectPath(c, 58, 56, 10, 10, 2), tone(0xffd060, { light: 0.6 }), { band: 1, hi: 0.5 });
  },
  // Open scroll with lines of text (combat log)
  log: (c) => {
    const pt = tone(PARCHMENT, { light: 0.3 });
    celI(c, () => roundRectPath(c, 20, 18, 56, 62, 4), pt, { band: 2.6, hi: 1 });
    celI(c, () => roundRectPath(c, 14, 12, 68, 12, 6), tone(0xc9ae7a), { band: 2, hi: 0.8 });
    celI(c, () => roundRectPath(c, 14, 74, 68, 12, 6), tone(0xc9ae7a), { band: 2, hi: 0.8 });
    for (let i = 0; i < 5; i++) line(c, [P(28, 34 + i * 8), P(i % 2 ? 60 : 68, 34 + i * 8)], 2, pt.shade);
  },
  // Paw print with a ley spark (灵兽 panel)
  pets: (c) => {
    const pad = tone(0x6fd8b8, { light: 0.4 });
    celI(c, () => { c.ellipse(48, 62, 20, 17, 0, 0, Math.PI * 2); }, pad, { band: 3, hi: 1.2 });
    const toe = (x: number, y: number, rx: number, ry: number, rot: number): void => {
      celI(c, () => { c.ellipse(x, y, rx, ry, rot, 0, Math.PI * 2); }, pad, { band: 2, hi: 0.9 });
    };
    toe(22, 42, 8, 10, -0.5);
    toe(38, 28, 8, 11, -0.15);
    toe(58, 28, 8, 11, 0.15);
    toe(74, 42, 8, 10, 0.5);
  },
  // Circular arrows around a spark (auto-combat)
  auto: (c) => {
    const t = tone(0x8ff07a, { light: 0.4 });
    const arc = (a0: number, a1: number): void => {
      celI(c, () => {
        c.arc(48, 48, 34, a0, a1);
        c.arc(48, 48, 22, a1, a0, true);
        c.closePath();
      }, t, { band: 2, hi: 0.8 });
      const ax = 48 + Math.cos(a1) * 28, ay = 48 + Math.sin(a1) * 28;
      const tx = -Math.sin(a1), ty = Math.cos(a1);
      celI(c, () => {
        c.moveTo(ax + Math.cos(a1) * 12, ay + Math.sin(a1) * 12);
        c.lineTo(ax + tx * 14, ay + ty * 14);
        c.lineTo(ax - Math.cos(a1) * 12, ay - Math.sin(a1) * 12);
        c.closePath();
      }, t, { band: 2, hi: 0.8 });
    };
    arc(Math.PI * 1.05, Math.PI * 1.75);
    arc(Math.PI * 0.05, Math.PI * 0.75);
  },
};

const DRAWN_OPTS: Partial<Record<HudIconId, Parameters<typeof inked>[3]>> = {
  auto: { over: (c) => { sparkle(c, 48, 48, 11, '#ffffff'); glow(c, P(48, 48), 18, 0x8ff07a, 0.5); } },
  homestead: { under: (c) => glow(c, P(48, 64), 30, 0xffc060, 0.25) },
  pets: { under: (c) => glow(c, P(48, 56), 34, 0x6ff0c8, 0.3), over: (c) => sparkle(c, 48, 62, 7, '#e8fff8') },
};

/** Texture key for a HUD icon (96×96, transparent), generated on first use. */
export function ensureHudIcon(scene: Phaser.Scene, id: HudIconId): string {
  switch (id) {
    case 'character': return ensureItemIcon(scene, 'a_helm');
    case 'skills': return ensureGlyph(scene, 'int');
    case 'quest': return ensureGlyph(scene, 'quest_available');
    case 'loot': return ensureGlyph(scene, 'gold');
    default: break;
  }
  const key = `hud_icon_${id}`;
  if (scene.textures.exists(key)) return key;
  const draw = DRAWN[id];
  const canvas = document.createElement('canvas');
  canvas.width = SIZE;
  canvas.height = SIZE;
  if (draw) inked(canvas.getContext('2d', { willReadFrequently: true })!, SIZE, draw, { inkWidth: 3, rim: 1.3, ...DRAWN_OPTS[id] });
  scene.textures.addCanvas(key, canvas);
  return key;
}

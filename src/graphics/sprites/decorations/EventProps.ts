import type { EntityDrawer } from '../types';

type EventPropKind =
  | 'rune_pillar'
  | 'root_altar'
  | 'gem_lock'
  | 'sundial'
  | 'abyss_array'
  | 'defend_campfire'
  | 'defend_abyss_seal';

function createEventPropDrawer(key: string, kind: EventPropKind): EntityDrawer {
  return {
    key,
    frameW: 64,
    frameH: 72,
    totalFrames: 1,

    drawFrame(ctx, _frame, _action, w, h, utils) {
      const s = w / 64;
      const cx = w / 2;
      const ground = h * 0.94;

      ctx.fillStyle = 'rgba(0,0,0,0.28)';
      utils.fillEllipse(ctx, cx, ground, 24 * s, 6 * s);

      if (kind === 'rune_pillar') {
        utils.drawStoneTexture(ctx, cx - 13 * s, ground - 48 * s, 26 * s, 43 * s, 0x5d6665);
        ctx.fillStyle = utils.rgb(0x464e4d);
        ctx.beginPath();
        ctx.moveTo(cx - 17 * s, ground - 5 * s);
        ctx.lineTo(cx - 12 * s, ground - 15 * s);
        ctx.lineTo(cx + 12 * s, ground - 15 * s);
        ctx.lineTo(cx + 17 * s, ground - 5 * s);
        ctx.closePath();
        ctx.fill();
        ctx.strokeStyle = '#72d8ff';
        ctx.lineWidth = 2 * s;
        ctx.beginPath();
        ctx.moveTo(cx, ground - 42 * s);
        ctx.lineTo(cx - 6 * s, ground - 34 * s);
        ctx.lineTo(cx + 5 * s, ground - 27 * s);
        ctx.lineTo(cx, ground - 19 * s);
        ctx.stroke();
      } else if (kind === 'root_altar') {
        ctx.strokeStyle = utils.rgb(0x4b3628);
        ctx.lineWidth = 7 * s;
        ctx.lineCap = 'round';
        for (const offset of [-14, 0, 14]) {
          ctx.beginPath();
          ctx.moveTo(cx + offset * s, ground - 3 * s);
          ctx.quadraticCurveTo(cx + offset * 0.5 * s, ground - 24 * s, cx + offset * 0.3 * s, ground - 43 * s);
          ctx.stroke();
        }
        ctx.fillStyle = utils.rgb(0x667d3c);
        utils.fillEllipse(ctx, cx, ground - 17 * s, 19 * s, 9 * s);
        ctx.fillStyle = '#9ee26d';
        for (const offset of [-9, 0, 9]) utils.fillCircle(ctx, cx + offset * s, ground - 19 * s, 2.5 * s);
      } else if (kind === 'gem_lock') {
        utils.drawStoneTexture(ctx, cx - 22 * s, ground - 25 * s, 44 * s, 22 * s, 0x575861);
        ctx.fillStyle = utils.rgb(0x41424a);
        utils.roundRect(ctx, cx - 18 * s, ground - 33 * s, 36 * s, 15 * s, 4 * s);
        ctx.fill();
        const gems = [0xd34a4a, 0x53b570, 0x4a78cf];
        for (let i = 0; i < gems.length; i++) {
          const gx = cx + (i - 1) * 12 * s;
          ctx.fillStyle = utils.rgb(gems[i]);
          ctx.beginPath();
          ctx.moveTo(gx, ground - 34 * s);
          ctx.lineTo(gx + 5 * s, ground - 27 * s);
          ctx.lineTo(gx, ground - 20 * s);
          ctx.lineTo(gx - 5 * s, ground - 27 * s);
          ctx.closePath();
          ctx.fill();
        }
      } else if (kind === 'sundial') {
        ctx.fillStyle = utils.rgb(0x98734c);
        utils.fillEllipse(ctx, cx, ground - 14 * s, 23 * s, 10 * s);
        ctx.strokeStyle = utils.rgb(0xd6bd7c);
        ctx.lineWidth = 2 * s;
        ctx.beginPath();
        ctx.ellipse(cx, ground - 14 * s, 18 * s, 7 * s, 0, 0, Math.PI * 2);
        ctx.stroke();
        ctx.fillStyle = utils.rgb(0x5c4534);
        ctx.beginPath();
        ctx.moveTo(cx, ground - 16 * s);
        ctx.lineTo(cx + 4 * s, ground - 42 * s);
        ctx.lineTo(cx + 8 * s, ground - 14 * s);
        ctx.closePath();
        ctx.fill();
        ctx.strokeStyle = 'rgba(55,35,25,0.5)';
        ctx.lineWidth = 3 * s;
        ctx.beginPath();
        ctx.moveTo(cx + 3 * s, ground - 14 * s);
        ctx.lineTo(cx + 19 * s, ground - 8 * s);
        ctx.stroke();
      } else if (kind === 'abyss_array') {
        const aura = ctx.createRadialGradient(cx, ground - 13 * s, 0, cx, ground - 13 * s, 28 * s);
        aura.addColorStop(0, 'rgba(96,225,230,0.5)');
        aura.addColorStop(1, 'rgba(78,45,138,0)');
        ctx.fillStyle = aura;
        utils.fillCircle(ctx, cx, ground - 13 * s, 28 * s);
        ctx.strokeStyle = '#68dce1';
        ctx.lineWidth = 2 * s;
        ctx.beginPath();
        ctx.ellipse(cx, ground - 10 * s, 23 * s, 10 * s, 0, 0, Math.PI * 2);
        ctx.stroke();
        ctx.beginPath();
        for (let i = 0; i < 5; i++) {
          const angle = -Math.PI / 2 + i * Math.PI * 0.8;
          const px = cx + Math.cos(angle) * 20 * s;
          const py = ground - 10 * s + Math.sin(angle) * 8 * s;
          if (i === 0) ctx.moveTo(px, py); else ctx.lineTo(px, py);
        }
        ctx.closePath();
        ctx.stroke();
        ctx.fillStyle = '#b48cff';
        for (let i = 0; i < 4; i++) utils.fillCircle(ctx, cx + (i - 1.5) * 8 * s, ground - 30 * s - (i % 2) * 7 * s, 2 * s);
      } else if (kind === 'defend_campfire') {
        ctx.fillStyle = utils.rgb(0x4c5158);
        for (let i = 0; i < 10; i++) {
          const angle = i * Math.PI * 0.2;
          utils.fillEllipse(ctx, cx + Math.cos(angle) * 17 * s, ground - 7 * s + Math.sin(angle) * 5 * s, 5 * s, 3 * s);
        }
        ctx.strokeStyle = utils.rgb(0x4a2c18);
        ctx.lineWidth = 6 * s;
        ctx.lineCap = 'round';
        ctx.beginPath();
        ctx.moveTo(cx - 15 * s, ground - 10 * s);
        ctx.lineTo(cx + 15 * s, ground - 4 * s);
        ctx.moveTo(cx + 15 * s, ground - 10 * s);
        ctx.lineTo(cx - 15 * s, ground - 4 * s);
        ctx.stroke();
        const glow = ctx.createRadialGradient(cx, ground - 22 * s, 0, cx, ground - 22 * s, 24 * s);
        glow.addColorStop(0, 'rgba(255,212,72,0.65)');
        glow.addColorStop(1, 'rgba(207,71,30,0)');
        ctx.fillStyle = glow;
        utils.fillCircle(ctx, cx, ground - 22 * s, 24 * s);
        utils.drawFlameLayer(ctx, cx, ground - 8 * s, 18 * s, 38 * s, '#d94b25', 0.7);
        utils.drawFlameLayer(ctx, cx, ground - 9 * s, 11 * s, 29 * s, '#ff9c32', 1.8);
        utils.drawFlameLayer(ctx, cx, ground - 10 * s, 5 * s, 18 * s, '#ffe47d', 2.4);
      } else {
        const aura = ctx.createRadialGradient(cx, ground - 25 * s, 0, cx, ground - 25 * s, 31 * s);
        aura.addColorStop(0, 'rgba(95,225,227,0.42)');
        aura.addColorStop(1, 'rgba(54,31,102,0)');
        ctx.fillStyle = aura;
        utils.fillCircle(ctx, cx, ground - 25 * s, 31 * s);
        ctx.fillStyle = utils.rgb(0x282837);
        ctx.beginPath();
        ctx.moveTo(cx, ground - 60 * s);
        ctx.lineTo(cx + 15 * s, ground - 13 * s);
        ctx.lineTo(cx + 9 * s, ground - 4 * s);
        ctx.lineTo(cx - 9 * s, ground - 4 * s);
        ctx.lineTo(cx - 15 * s, ground - 13 * s);
        ctx.closePath();
        ctx.fill();
        ctx.strokeStyle = '#67dce2';
        ctx.lineWidth = 2.2 * s;
        ctx.beginPath();
        ctx.moveTo(cx, ground - 48 * s);
        ctx.lineTo(cx - 7 * s, ground - 35 * s);
        ctx.lineTo(cx + 7 * s, ground - 25 * s);
        ctx.lineTo(cx, ground - 12 * s);
        ctx.stroke();
        ctx.strokeStyle = utils.rgb(0x7d629f);
        ctx.lineWidth = 2 * s;
        ctx.beginPath();
        ctx.moveTo(cx - 17 * s, ground - 43 * s);
        ctx.lineTo(cx - 11 * s, ground - 25 * s);
        ctx.moveTo(cx + 17 * s, ground - 43 * s);
        ctx.lineTo(cx + 11 * s, ground - 25 * s);
        ctx.stroke();
      }
    },
  };
}

export const EventPuzzleRunePillarDrawer = createEventPropDrawer('decor_event_puzzle_rune_pillar', 'rune_pillar');
export const EventPuzzleRootAltarDrawer = createEventPropDrawer('decor_event_puzzle_root_altar', 'root_altar');
export const EventPuzzleGemLockDrawer = createEventPropDrawer('decor_event_puzzle_gem_lock', 'gem_lock');
export const EventPuzzleSundialDrawer = createEventPropDrawer('decor_event_puzzle_sundial', 'sundial');
export const EventPuzzleAbyssArrayDrawer = createEventPropDrawer('decor_event_puzzle_abyss_array', 'abyss_array');
export const DefendCampfireDrawer = createEventPropDrawer('decor_defend_campfire', 'defend_campfire');
export const DefendAbyssSealDrawer = createEventPropDrawer('decor_defend_abyss_seal', 'defend_abyss_seal');

export const EVENT_PROP_DRAWERS: readonly EntityDrawer[] = [
  EventPuzzleRunePillarDrawer,
  EventPuzzleRootAltarDrawer,
  EventPuzzleGemLockDrawer,
  EventPuzzleSundialDrawer,
  EventPuzzleAbyssArrayDrawer,
  DefendCampfireDrawer,
  DefendAbyssSealDrawer,
];

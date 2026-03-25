import type { EntityDrawer } from '../types';

export const PuzzleStoneDrawer: EntityDrawer = {
  key: 'decor_puzzle_stone',
  frameW: 28,
  frameH: 26,
  totalFrames: 1,

  drawFrame(ctx, _frame, _action, w, h, utils) {
    const s = w / 28;

    // Shadow
    ctx.fillStyle = 'rgba(0,0,0,0.25)';
    utils.fillEllipse(ctx, w / 2, h - 1.5 * s, 12 * s, 3 * s);

    // Stone pedestal base
    ctx.fillStyle = utils.rgb(0x5a5a5a);
    ctx.beginPath();
    ctx.moveTo(w * 0.1, h * 0.9);
    ctx.lineTo(w * 0.2, h * 0.6);
    ctx.lineTo(w * 0.8, h * 0.6);
    ctx.lineTo(w * 0.9, h * 0.9);
    ctx.closePath();
    ctx.fill();

    // Stone top face
    ctx.fillStyle = utils.rgb(0x6a6a6a);
    ctx.fillRect(w * 0.15, h * 0.55, w * 0.7, h * 0.08);

    // Carved rune symbols
    ctx.strokeStyle = utils.rgb(0x44aaff, 0.7);
    ctx.lineWidth = 1.2 * s;

    // Rune 1 - circle
    ctx.beginPath();
    ctx.arc(w * 0.35, h * 0.75, 3 * s, 0, Math.PI * 2);
    ctx.stroke();

    // Rune 2 - triangle
    ctx.beginPath();
    ctx.moveTo(w * 0.65, h * 0.68);
    ctx.lineTo(w * 0.6, h * 0.82);
    ctx.lineTo(w * 0.7, h * 0.82);
    ctx.closePath();
    ctx.stroke();

    // Rune 3 - cross/diamond at top
    ctx.beginPath();
    ctx.moveTo(w * 0.5, h * 0.35);
    ctx.lineTo(w * 0.55, h * 0.42);
    ctx.lineTo(w * 0.5, h * 0.49);
    ctx.lineTo(w * 0.45, h * 0.42);
    ctx.closePath();
    ctx.stroke();

    // Glowing aura around runes
    ctx.fillStyle = 'rgba(68,170,255,0.15)';
    utils.fillEllipse(ctx, w * 0.35, h * 0.75, 5 * s, 5 * s);
    utils.fillEllipse(ctx, w * 0.65, h * 0.75, 5 * s, 5 * s);
    utils.fillEllipse(ctx, w * 0.5, h * 0.42, 5 * s, 5 * s);

    // Cracks in stone
    ctx.strokeStyle = utils.rgb(0x3a3a3a, 0.5);
    ctx.lineWidth = 0.6 * s;
    ctx.beginPath();
    ctx.moveTo(w * 0.25, h * 0.65);
    ctx.lineTo(w * 0.3, h * 0.78);
    ctx.stroke();
    ctx.beginPath();
    ctx.moveTo(w * 0.75, h * 0.62);
    ctx.lineTo(w * 0.72, h * 0.72);
    ctx.stroke();

    // Moss patches
    ctx.fillStyle = utils.rgb(0x3a5a30, 0.4);
    utils.fillEllipse(ctx, w * 0.15, h * 0.85, 3 * s, 1.5 * s);
    utils.fillEllipse(ctx, w * 0.85, h * 0.82, 2.5 * s, 1.5 * s);
  },
};

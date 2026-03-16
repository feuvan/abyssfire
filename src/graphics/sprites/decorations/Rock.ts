// src/graphics/sprites/decorations/Rock.ts
import type { EntityDrawer } from '../types';

export const RockDrawer: EntityDrawer = {
  key: 'decor_rock',
  frameW: 16,
  frameH: 12,
  totalFrames: 1,

  drawFrame(ctx, _frame, _action, w, h, utils) {
    const s = w / 16;

    // Ground shadow
    ctx.fillStyle = 'rgba(0,0,0,0.15)';
    utils.fillEllipse(ctx, w / 2, h - s, 6.5 * s, 1.5 * s);

    // Angular polygon rock shape via clip path
    ctx.save();
    ctx.beginPath();
    ctx.moveTo(w * 0.18, h * 0.88);
    ctx.lineTo(w * 0.08, h * 0.58);
    ctx.lineTo(w * 0.20, h * 0.28);
    ctx.lineTo(w * 0.45, h * 0.12);
    ctx.lineTo(w * 0.72, h * 0.18);
    ctx.lineTo(w * 0.90, h * 0.48);
    ctx.lineTo(w * 0.85, h * 0.82);
    ctx.closePath();
    ctx.clip();

    // Stone texture fill
    utils.drawStoneTexture(ctx, w * 0.08, h * 0.12, w * 0.82, h * 0.76, 0x4e5258);

    // Depth gradient overlay
    const grad = ctx.createLinearGradient(w * 0.1, h * 0.15, w * 0.85, h * 0.85);
    grad.addColorStop(0, 'rgba(80,85,90,0.4)');
    grad.addColorStop(0.4, 'rgba(0,0,0,0)');
    grad.addColorStop(1, 'rgba(20,22,26,0.35)');
    ctx.fillStyle = grad;
    ctx.fillRect(0, 0, w, h);

    ctx.restore();

    // Lichen spots (green-grey circles at low alpha)
    const lichenSpots: [number, number, number][] = [
      [0.35, 0.32, 1.4],
      [0.58, 0.25, 1.0],
      [0.70, 0.48, 1.6],
      [0.24, 0.55, 1.1],
      [0.50, 0.60, 0.9],
    ];
    for (const [lxF, lyF, lr] of lichenSpots) {
      ctx.fillStyle = 'rgba(88,102,70,0.38)';
      utils.fillCircle(ctx, w * lxF, h * lyF, lr * s);
    }

    // Edge highlight (top-left)
    ctx.strokeStyle = 'rgba(110,115,122,0.4)';
    ctx.lineWidth = 0.7 * s;
    ctx.beginPath();
    ctx.moveTo(w * 0.20, h * 0.28);
    ctx.lineTo(w * 0.45, h * 0.12);
    ctx.lineTo(w * 0.72, h * 0.18);
    ctx.stroke();
  },
};

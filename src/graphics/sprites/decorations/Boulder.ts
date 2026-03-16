// src/graphics/sprites/decorations/Boulder.ts
import type { EntityDrawer } from '../types';

export const BoulderDrawer: EntityDrawer = {
  key: 'decor_boulder',
  frameW: 20,
  frameH: 16,
  totalFrames: 1,

  drawFrame(ctx, _frame, _action, w, h, utils) {
    const s = w / 20;

    // Ground shadow
    ctx.fillStyle = 'rgba(0,0,0,0.22)';
    utils.fillEllipse(ctx, w / 2, h - s, 9 * s, 2.2 * s);

    // Massive angular polygon boulder via clip
    ctx.save();
    ctx.beginPath();
    ctx.moveTo(w * 0.10, h * 0.90);
    ctx.lineTo(w * 0.05, h * 0.55);
    ctx.lineTo(w * 0.14, h * 0.22);
    ctx.lineTo(w * 0.38, h * 0.08);
    ctx.lineTo(w * 0.65, h * 0.06);
    ctx.lineTo(w * 0.88, h * 0.22);
    ctx.lineTo(w * 0.95, h * 0.58);
    ctx.lineTo(w * 0.85, h * 0.88);
    ctx.closePath();
    ctx.clip();

    // Stone texture fill (covers the clipped region)
    utils.drawStoneTexture(ctx, w * 0.05, h * 0.06, w * 0.90, h * 0.84, 0x4a4a50);

    // Depth shading overlay
    const grad = ctx.createLinearGradient(w * 0.1, h * 0.1, w * 0.9, h * 0.9);
    grad.addColorStop(0, 'rgba(90,90,100,0.35)');
    grad.addColorStop(0.35, 'rgba(0,0,0,0)');
    grad.addColorStop(1, 'rgba(10,10,15,0.45)');
    ctx.fillStyle = grad;
    ctx.fillRect(0, 0, w, h);

    ctx.restore();

    // Moss on top — green ellipse overlay
    ctx.fillStyle = 'rgba(55,90,40,0.38)';
    utils.fillEllipse(ctx, w * 0.48, h * 0.20, w * 0.28, h * 0.12);
    ctx.fillStyle = 'rgba(70,110,50,0.22)';
    utils.fillEllipse(ctx, w * 0.52, h * 0.16, w * 0.18, h * 0.08);

    // Edge highlight (top-left face)
    ctx.strokeStyle = 'rgba(100,105,115,0.45)';
    ctx.lineWidth = 0.8 * s;
    ctx.beginPath();
    ctx.moveTo(w * 0.14, h * 0.22);
    ctx.lineTo(w * 0.38, h * 0.08);
    ctx.lineTo(w * 0.65, h * 0.06);
    ctx.lineTo(w * 0.88, h * 0.22);
    ctx.stroke();
  },
};

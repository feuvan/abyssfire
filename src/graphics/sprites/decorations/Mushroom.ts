// src/graphics/sprites/decorations/Mushroom.ts
import type { EntityDrawer } from '../types';

export const MushroomDrawer: EntityDrawer = {
  key: 'decor_mushroom',
  frameW: 10,
  frameH: 12,
  totalFrames: 1,

  drawFrame(ctx, _frame, _action, w, h, utils) {
    const s = w / 10;
    const cx = w / 2;

    // Ground shadow
    ctx.fillStyle = 'rgba(0,0,0,0.14)';
    utils.fillEllipse(ctx, cx, h - 0.8 * s, 4 * s, 1.2 * s);

    // Thick stem
    const stemGrad = ctx.createLinearGradient(cx - 2 * s, h * 0.52, cx + 2 * s, h * 0.52);
    stemGrad.addColorStop(0, '#c8b898');
    stemGrad.addColorStop(0.5, '#e0d0b0');
    stemGrad.addColorStop(1, '#a89870');
    ctx.fillStyle = stemGrad;
    utils.roundRect(ctx, cx - 2 * s, h * 0.52, 4 * s, h * 0.40, 1.5 * s);
    ctx.fill();

    // Gills underneath cap (thin arcs)
    ctx.strokeStyle = 'rgba(160,120,80,0.5)';
    ctx.lineWidth = 0.5 * s;
    for (let i = -2; i <= 2; i++) {
      ctx.beginPath();
      ctx.arc(cx + i * 1.4 * s, h * 0.52, 1.0 * s, Math.PI * 0.1, Math.PI * 0.9);
      ctx.stroke();
    }

    // Cap ellipse
    const capGrad = ctx.createRadialGradient(cx - 1.5 * s, h * 0.28, 0, cx, h * 0.35, 5.5 * s);
    capGrad.addColorStop(0, '#c03a28');
    capGrad.addColorStop(0.5, '#8a1a10');
    capGrad.addColorStop(1, '#5a0a08');
    ctx.fillStyle = capGrad;
    utils.fillEllipse(ctx, cx, h * 0.38, 5 * s, 4.2 * s);

    // Cap rim underside hint
    ctx.fillStyle = 'rgba(220,190,150,0.3)';
    utils.fillEllipse(ctx, cx, h * 0.50, 4.5 * s, 1.2 * s);

    // Spots on cap (small white circles)
    const spots: [number, number, number][] = [
      [0.38, 0.22, 1.1],
      [0.62, 0.26, 0.8],
      [0.50, 0.32, 0.7],
      [0.28, 0.35, 0.9],
      [0.70, 0.38, 0.75],
    ];
    for (const [sxF, syF, sr] of spots) {
      ctx.fillStyle = 'rgba(230,220,200,0.75)';
      utils.fillCircle(ctx, w * sxF, h * syF, sr * s);
      // Inner spot sheen
      ctx.fillStyle = 'rgba(255,255,255,0.4)';
      utils.fillCircle(ctx, w * sxF - 0.2 * s, h * syF - 0.2 * s, sr * 0.4 * s);
    }
  },
};

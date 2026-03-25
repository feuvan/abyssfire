import type { EntityDrawer } from '../types';

export const GoldPileDrawer: EntityDrawer = {
  key: 'decor_gold_pile',
  frameW: 24,
  frameH: 18,
  totalFrames: 1,

  drawFrame(ctx, _frame, _action, w, h, utils) {
    const s = w / 24;

    // Shadow
    ctx.fillStyle = 'rgba(0,0,0,0.2)';
    utils.fillEllipse(ctx, w / 2, h - 1 * s, 10 * s, 2.5 * s);

    // Coin pile base
    const pileGrad = ctx.createRadialGradient(w / 2, h * 0.6, 2 * s, w / 2, h * 0.6, 10 * s);
    pileGrad.addColorStop(0, utils.rgb(0xFFD700));
    pileGrad.addColorStop(0.7, utils.rgb(0xDAA520));
    pileGrad.addColorStop(1, utils.rgb(0xB8860B));
    ctx.fillStyle = pileGrad;
    utils.fillEllipse(ctx, w / 2, h * 0.7, 10 * s, 5 * s);

    // Individual coins scattered on top
    const coinPositions = [
      [0.35, 0.55], [0.55, 0.45], [0.65, 0.58], [0.45, 0.38],
      [0.5, 0.6], [0.38, 0.68], [0.62, 0.48], [0.52, 0.52],
    ];
    for (const [xf, yf] of coinPositions) {
      ctx.fillStyle = utils.rgb(0xFFD700, 0.9);
      utils.fillEllipse(ctx, w * xf, h * yf, 2.5 * s, 1.8 * s);
      ctx.strokeStyle = utils.rgb(0xB8860B, 0.6);
      ctx.lineWidth = 0.5 * s;
      ctx.beginPath();
      ctx.ellipse(w * xf, h * yf, 2.5 * s, 1.8 * s, 0, 0, Math.PI * 2);
      ctx.stroke();
    }

    // Sparkle highlights
    ctx.fillStyle = 'rgba(255,255,220,0.7)';
    utils.fillCircle(ctx, w * 0.4, h * 0.42, 0.8 * s);
    utils.fillCircle(ctx, w * 0.6, h * 0.5, 0.6 * s);
  },
};

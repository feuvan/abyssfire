import type { EntityDrawer } from '../types';

export const TreasureChestDrawer: EntityDrawer = {
  key: 'decor_treasure_chest',
  frameW: 28,
  frameH: 22,
  totalFrames: 1,

  drawFrame(ctx, _frame, _action, w, h, utils) {
    const s = w / 28;

    // Shadow
    ctx.fillStyle = 'rgba(0,0,0,0.25)';
    utils.fillEllipse(ctx, w / 2, h - 1.5 * s, 12 * s, 3 * s);

    // Chest body
    const bodyGrad = ctx.createLinearGradient(w * 0.15, h * 0.5, w * 0.85, h * 0.9);
    bodyGrad.addColorStop(0, utils.rgb(0x8B6914));
    bodyGrad.addColorStop(0.5, utils.rgb(0xDAA520));
    bodyGrad.addColorStop(1, utils.rgb(0x8B6914));
    ctx.fillStyle = bodyGrad;
    utils.roundRect(ctx, w * 0.12, h * 0.45, w * 0.76, h * 0.45, 2 * s);
    ctx.fill();

    // Metal bands
    ctx.fillStyle = utils.rgb(0x666666);
    ctx.fillRect(w * 0.12, h * 0.52, w * 0.76, 1.5 * s);
    ctx.fillRect(w * 0.12, h * 0.72, w * 0.76, 1.5 * s);

    // Chest lid (domed)
    const lidGrad = ctx.createLinearGradient(w * 0.1, h * 0.25, w * 0.9, h * 0.5);
    lidGrad.addColorStop(0, utils.rgb(0xCD853F));
    lidGrad.addColorStop(0.5, utils.rgb(0xDEB887));
    lidGrad.addColorStop(1, utils.rgb(0xCD853F));
    ctx.fillStyle = lidGrad;
    ctx.beginPath();
    ctx.moveTo(w * 0.1, h * 0.48);
    ctx.quadraticCurveTo(w * 0.5, h * 0.15, w * 0.9, h * 0.48);
    ctx.closePath();
    ctx.fill();

    // Lid band
    ctx.strokeStyle = utils.rgb(0x666666);
    ctx.lineWidth = 1.5 * s;
    ctx.beginPath();
    ctx.moveTo(w * 0.18, h * 0.47);
    ctx.quadraticCurveTo(w * 0.5, h * 0.22, w * 0.82, h * 0.47);
    ctx.stroke();

    // Lock/clasp
    ctx.fillStyle = utils.rgb(0xFFD700);
    utils.roundRect(ctx, w * 0.42, h * 0.42, w * 0.16, h * 0.12, 1.5 * s);
    ctx.fill();
    // Keyhole
    ctx.fillStyle = utils.rgb(0x333333);
    utils.fillCircle(ctx, w * 0.5, h * 0.47, 1.2 * s);
    ctx.fillRect(w * 0.49, h * 0.47, 1 * s, 3 * s);

    // Glow sparkles
    ctx.fillStyle = 'rgba(255,215,0,0.5)';
    utils.fillCircle(ctx, w * 0.3, h * 0.3, 1 * s);
    utils.fillCircle(ctx, w * 0.7, h * 0.35, 0.8 * s);
    utils.fillCircle(ctx, w * 0.55, h * 0.2, 0.6 * s);
  },
};

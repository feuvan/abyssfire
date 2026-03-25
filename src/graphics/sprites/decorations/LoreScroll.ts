import type { EntityDrawer } from '../types';

export const LoreScrollDrawer: EntityDrawer = {
  key: 'decor_lore_scroll',
  frameW: 20,
  frameH: 24,
  totalFrames: 1,

  drawFrame(ctx, _frame, _action, w, h, utils) {
    const s = w / 20;

    // Shadow
    ctx.fillStyle = 'rgba(0,0,0,0.2)';
    utils.fillEllipse(ctx, w / 2, h - 1 * s, 8 * s, 2 * s);

    // Scroll body (parchment)
    const scrollGrad = ctx.createLinearGradient(w * 0.2, 0, w * 0.8, 0);
    scrollGrad.addColorStop(0, utils.rgb(0xC4A972));
    scrollGrad.addColorStop(0.3, utils.rgb(0xDEB887));
    scrollGrad.addColorStop(0.7, utils.rgb(0xDEB887));
    scrollGrad.addColorStop(1, utils.rgb(0xC4A972));
    ctx.fillStyle = scrollGrad;
    utils.roundRect(ctx, w * 0.22, h * 0.15, w * 0.56, h * 0.7, 1 * s);
    ctx.fill();

    // Top roll
    ctx.fillStyle = utils.rgb(0xC4A972);
    utils.fillEllipse(ctx, w / 2, h * 0.17, w * 0.35, 3 * s);
    ctx.fillStyle = utils.rgb(0x8B7355);
    utils.fillEllipse(ctx, w / 2, h * 0.17, w * 0.35, 1.5 * s);

    // Bottom roll
    ctx.fillStyle = utils.rgb(0xC4A972);
    utils.fillEllipse(ctx, w / 2, h * 0.83, w * 0.35, 3 * s);
    ctx.fillStyle = utils.rgb(0x8B7355);
    utils.fillEllipse(ctx, w / 2, h * 0.83, w * 0.35, 1.5 * s);

    // Text lines
    ctx.fillStyle = utils.rgb(0x4a3a2a, 0.5);
    for (let i = 0; i < 4; i++) {
      const ly = h * 0.32 + i * 3.5 * s;
      const lw = (5 + Math.random() * 4) * s;
      ctx.fillRect(w / 2 - lw / 2, ly, lw, 0.8 * s);
    }

    // Wax seal
    ctx.fillStyle = utils.rgb(0x8B0000);
    utils.fillCircle(ctx, w * 0.55, h * 0.72, 2 * s);
    ctx.fillStyle = utils.rgb(0xAA2222, 0.6);
    utils.fillCircle(ctx, w * 0.54, h * 0.71, 1 * s);
  },
};

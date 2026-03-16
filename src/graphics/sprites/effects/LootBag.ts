// src/graphics/sprites/effects/LootBag.ts
import type { EntityDrawer } from '../types';

export const LootBagDrawer: EntityDrawer = {
  key: 'loot_bag',
  frameW: 24,
  frameH: 24,
  totalFrames: 1,

  drawFrame(ctx, _frame, _action, w, h, utils) {
    const s = w / 24;
    const cx = w / 2;

    // Ground shadow
    ctx.fillStyle = 'rgba(0,0,0,0.22)';
    utils.fillEllipse(ctx, cx, h - 1.5 * s, 8 * s, 2.2 * s);

    // Main bag body — bulging leather pouch shape
    utils.drawLeatherTexture(ctx, cx - 7 * s, h * 0.28, 14 * s, 13 * s, 0x4a3020);

    // Bulge highlight to give rounded shape
    const bulgeGrad = ctx.createRadialGradient(cx - 2 * s, h * 0.36, 0, cx, h * 0.48, 8 * s);
    bulgeGrad.addColorStop(0, 'rgba(110,75,40,0.35)');
    bulgeGrad.addColorStop(0.55, 'rgba(0,0,0,0)');
    bulgeGrad.addColorStop(1, 'rgba(20,10,5,0.30)');
    ctx.fillStyle = bulgeGrad;
    utils.roundRect(ctx, cx - 7 * s, h * 0.28, 14 * s, 13 * s, 3 * s);
    ctx.fill();

    // Drawstring neck — slightly narrower section
    utils.drawLeatherTexture(ctx, cx - 5 * s, h * 0.16, 10 * s, 5 * s, 0x3a2416);

    // Drawstring tie at top
    ctx.strokeStyle = utils.rgb(0x6a4a28, 0.85);
    ctx.lineWidth = 1.2 * s;
    ctx.lineCap = 'round';
    // Left loop of bow
    ctx.beginPath();
    ctx.moveTo(cx - 1 * s, h * 0.14);
    ctx.quadraticCurveTo(cx - 4 * s, h * 0.06, cx - 2 * s, h * 0.16);
    ctx.stroke();
    // Right loop of bow
    ctx.beginPath();
    ctx.moveTo(cx + 1 * s, h * 0.14);
    ctx.quadraticCurveTo(cx + 4 * s, h * 0.06, cx + 2 * s, h * 0.16);
    ctx.stroke();

    // Visible stitching — dashed line down center
    ctx.strokeStyle = utils.rgb(0x2a1808, 0.55);
    ctx.lineWidth = 0.6 * s;
    ctx.setLineDash([1.2 * s, 1.0 * s]);
    ctx.beginPath();
    ctx.moveTo(cx, h * 0.30);
    ctx.lineTo(cx, h * 0.38);
    ctx.moveTo(cx, h * 0.38); // slight curve for bulge
    ctx.quadraticCurveTo(cx + 0.5 * s, h * 0.50, cx, h * 0.60);
    ctx.lineTo(cx, h * 0.38);
    ctx.stroke();
    ctx.setLineDash([]);

    // Side stitching lines
    ctx.strokeStyle = utils.rgb(0x2a1808, 0.30);
    ctx.lineWidth = 0.5 * s;
    ctx.setLineDash([0.8 * s, 0.8 * s]);
    // Left seam
    ctx.beginPath();
    ctx.moveTo(cx - 6.5 * s, h * 0.30);
    ctx.lineTo(cx - 6.5 * s, h * 0.40);
    ctx.stroke();
    // Right seam
    ctx.beginPath();
    ctx.moveTo(cx + 6.5 * s, h * 0.30);
    ctx.lineTo(cx + 6.5 * s, h * 0.40);
    ctx.stroke();
    ctx.setLineDash([]);

    // Metallic buckle/clasp with sheen — small rect
    utils.drawMetalSurface(ctx, cx - 2.5 * s, h * 0.50, 5 * s, 3.5 * s, 0x8a7020);
    // Buckle border
    ctx.strokeStyle = utils.rgb(0x5a4c10, 0.7);
    ctx.lineWidth = 0.6 * s;
    ctx.strokeRect(cx - 2.5 * s, h * 0.50, 5 * s, 3.5 * s);
    // Buckle pin
    ctx.strokeStyle = utils.rgb(0xc0a830, 0.8);
    ctx.lineWidth = 0.7 * s;
    ctx.beginPath();
    ctx.moveTo(cx, h * 0.50);
    ctx.lineTo(cx, h * 0.50 + 3.5 * s);
    ctx.stroke();
    // Buckle sheen highlight
    ctx.fillStyle = 'rgba(220,200,80,0.30)';
    ctx.fillRect(cx - 2.0 * s, h * 0.51, 1.5 * s, 1.2 * s);
  },
};

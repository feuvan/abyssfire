// src/graphics/sprites/decorations/Crystal.ts
import type { EntityDrawer } from '../types';

export const CrystalDrawer: EntityDrawer = {
  key: 'decor_crystal',
  frameW: 14,
  frameH: 18,
  totalFrames: 1,

  drawFrame(ctx, _frame, _action, w, h, utils) {
    const s = w / 14;
    const cx = w / 2;

    // Ground shadow
    ctx.fillStyle = 'rgba(0,0,0,0.16)';
    utils.fillEllipse(ctx, cx, h - s, 4.5 * s, 1.3 * s);

    // Define crystal polygon facets
    const tip = { x: cx, y: h * 0.04 };
    const topL = { x: cx - 4 * s, y: h * 0.28 };
    const topR = { x: cx + 4 * s, y: h * 0.22 };
    const midL = { x: cx - 5 * s, y: h * 0.58 };
    const midR = { x: cx + 4.5 * s, y: h * 0.55 };
    const botL = { x: cx - 2.5 * s, y: h * 0.88 };
    const botR = { x: cx + 2.5 * s, y: h * 0.88 };

    // Back-left facet (darker)
    ctx.fillStyle = utils.rgb(0x2a1248, 0.85);
    ctx.beginPath();
    ctx.moveTo(tip.x, tip.y);
    ctx.lineTo(topL.x, topL.y);
    ctx.lineTo(midL.x, midL.y);
    ctx.lineTo(botL.x, botL.y);
    ctx.closePath();
    ctx.fill();

    // Front-right facet (mid tone)
    ctx.fillStyle = utils.rgb(0x4a1e7a, 0.80);
    ctx.beginPath();
    ctx.moveTo(tip.x, tip.y);
    ctx.lineTo(topR.x, topR.y);
    ctx.lineTo(midR.x, midR.y);
    ctx.lineTo(botR.x, botR.y);
    ctx.closePath();
    ctx.fill();

    // Center front facet
    ctx.fillStyle = utils.rgb(0x3a1560, 0.88);
    ctx.beginPath();
    ctx.moveTo(tip.x, tip.y);
    ctx.lineTo(topL.x, topL.y);
    ctx.lineTo(botL.x, botL.y);
    ctx.lineTo(botR.x, botR.y);
    ctx.lineTo(midR.x, midR.y);
    ctx.lineTo(topR.x, topR.y);
    ctx.closePath();
    ctx.fill();

    // Inner glow (radial gradient)
    const glow = ctx.createRadialGradient(cx, h * 0.46, 0, cx, h * 0.46, 5 * s);
    glow.addColorStop(0, 'rgba(160,80,255,0.35)');
    glow.addColorStop(0.5, 'rgba(100,40,180,0.18)');
    glow.addColorStop(1, 'rgba(60,10,120,0)');
    ctx.fillStyle = glow;
    ctx.beginPath();
    ctx.moveTo(tip.x, tip.y);
    ctx.lineTo(topL.x, topL.y);
    ctx.lineTo(midL.x, midL.y);
    ctx.lineTo(botL.x, botL.y);
    ctx.lineTo(botR.x, botR.y);
    ctx.lineTo(midR.x, midR.y);
    ctx.lineTo(topR.x, topR.y);
    ctx.closePath();
    ctx.fill();

    // Facet edge lines (transparent outer strokes at low alpha)
    ctx.strokeStyle = 'rgba(180,120,255,0.25)';
    ctx.lineWidth = 0.7 * s;
    ctx.beginPath();
    ctx.moveTo(tip.x, tip.y);
    ctx.lineTo(topL.x, topL.y);
    ctx.lineTo(midL.x, midL.y);
    ctx.lineTo(botL.x, botL.y);
    ctx.lineTo(botR.x, botR.y);
    ctx.lineTo(midR.x, midR.y);
    ctx.lineTo(topR.x, topR.y);
    ctx.closePath();
    ctx.stroke();
    // Internal facet dividers
    ctx.strokeStyle = 'rgba(200,140,255,0.18)';
    ctx.lineWidth = 0.5 * s;
    ctx.beginPath();
    ctx.moveTo(tip.x, tip.y); ctx.lineTo(botL.x, botL.y); ctx.stroke();
    ctx.beginPath();
    ctx.moveTo(tip.x, tip.y); ctx.lineTo(botR.x, botR.y); ctx.stroke();

    // Bright highlight streak on left face
    ctx.fillStyle = 'rgba(220,200,255,0.30)';
    ctx.beginPath();
    ctx.moveTo(tip.x - 0.5 * s, tip.y + 1.5 * s);
    ctx.lineTo(topL.x + 1.5 * s, topL.y + 2 * s);
    ctx.lineTo(topL.x + 2.5 * s, topL.y + 3.5 * s);
    ctx.lineTo(tip.x + 0.3 * s, tip.y + 2 * s);
    ctx.closePath();
    ctx.fill();
  },
};

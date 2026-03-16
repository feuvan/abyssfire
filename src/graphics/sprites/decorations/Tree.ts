// src/graphics/sprites/decorations/Tree.ts
import type { EntityDrawer } from '../types';

export const TreeDrawer: EntityDrawer = {
  key: 'decor_tree',
  frameW: 24,
  frameH: 36,
  totalFrames: 1,

  drawFrame(ctx, _frame, _action, w, h, utils) {
    const s = w / 24;

    // Ground shadow
    ctx.fillStyle = 'rgba(0,0,0,0.18)';
    utils.fillEllipse(ctx, w / 2, h - 1.5 * s, 9 * s, 2 * s);

    // Visible roots at base (curved strokes)
    ctx.strokeStyle = utils.rgb(0x1a0f06, 0.6);
    ctx.lineWidth = 1 * s;
    ctx.lineCap = 'round';
    for (let i = 0; i < 3; i++) {
      const angle = (i / 3) * Math.PI - Math.PI * 0.25;
      const rx = w / 2 + Math.cos(angle) * 4 * s;
      const ry = h * 0.72 + Math.sin(angle) * 2 * s;
      ctx.beginPath();
      ctx.moveTo(w / 2, h * 0.68);
      ctx.quadraticCurveTo(w / 2 + Math.cos(angle) * 2 * s, h * 0.70, rx, ry);
      ctx.stroke();
    }

    // Trunk — gnarled via drawPart with bark detail
    utils.drawPart(ctx, w / 2 - 2.5 * s, h * 0.42, 5 * s, h * 0.30, 0x2a1a0a, 1 * s);
    // Bark vertical line details
    ctx.strokeStyle = utils.rgb(0x120d04, 0.35);
    ctx.lineWidth = 0.6 * s;
    for (let i = 0; i < 3; i++) {
      const lx = w / 2 - 1.5 * s + i * 1.5 * s;
      ctx.beginPath();
      ctx.moveTo(lx, h * 0.44);
      ctx.lineTo(lx + (i % 2 === 0 ? 0.5 * s : -0.5 * s), h * 0.68);
      ctx.stroke();
    }
    // Subtle gnarl knot
    ctx.strokeStyle = utils.rgb(0x120d04, 0.25);
    ctx.lineWidth = 0.8 * s;
    ctx.beginPath();
    ctx.ellipse(w / 2 + 0.5 * s, h * 0.56, 1.2 * s, 0.8 * s, 0.3, 0, Math.PI * 2);
    ctx.stroke();

    // Canopy — 4 overlapping ellipses with depth gradient
    const canopyColors: [number, number, number, number][] = [
      // cx offset, cy, rx, ry
      [0, 0.32, 0.40, 0.28],   // back-left
      [0.08, 0.28, 0.38, 0.25], // back-right
      [-0.06, 0.26, 0.35, 0.24], // mid
      [0, 0.22, 0.42, 0.30],    // front
    ];
    const greens = [0x0d2e0b, 0x134d10, 0x1a5c15, 0x226820];
    for (let i = 0; i < canopyColors.length; i++) {
      const [dxF, cyF, rxF, ryF] = canopyColors[i];
      const baseGreen = greens[i];
      const grad = ctx.createRadialGradient(
        w / 2 + dxF * w - w * 0.06, h * cyF - h * 0.04, 0,
        w / 2 + dxF * w, h * cyF, w * rxF
      );
      grad.addColorStop(0, utils.rgb(utils.lighten(baseGreen, 18)));
      grad.addColorStop(0.6, utils.rgb(baseGreen));
      grad.addColorStop(1, utils.rgb(utils.darken(baseGreen, 12)));
      ctx.fillStyle = grad;
      utils.fillEllipse(ctx, w / 2 + dxF * w, h * cyF, w * rxF, h * ryF);
    }

    // Canopy highlight specular
    ctx.fillStyle = 'rgba(80,160,60,0.12)';
    utils.fillEllipse(ctx, w * 0.42, h * 0.18, w * 0.12, h * 0.07);
  },
};

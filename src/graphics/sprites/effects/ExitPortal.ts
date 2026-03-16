// src/graphics/sprites/effects/ExitPortal.ts
import type { EntityDrawer } from '../types';

export const ExitPortalDrawer: EntityDrawer = {
  key: 'exit_portal',
  frameW: 32,
  frameH: 32,
  totalFrames: 1,

  drawFrame(ctx, _frame, _action, w, h, utils) {
    const s = w / 32;
    const cx = w / 2, cy = h / 2;
    const baseGreen = 0x00dc64;

    // Ground shadow
    ctx.fillStyle = 'rgba(0,0,0,0.18)';
    utils.fillEllipse(ctx, cx, h - 2 * s, 12 * s, 3 * s);

    // Outer vortex rings — concentric arc strokes with subtle rotation offsets
    const ringDefs: [number, number, number, string][] = [
      // radius, lineWidth, rotationOffset, color
      [13 * s, 1.2 * s, 0.0,  'rgba(0,220,100,0.18)'],
      [11 * s, 1.5 * s, 0.4,  'rgba(0,220,100,0.28)'],
      [9 * s,  1.8 * s, 0.8,  'rgba(0,200,90,0.38)'],
      [7 * s,  2.0 * s, 1.2,  'rgba(0,180,80,0.50)'],
      [5 * s,  2.2 * s, 1.6,  'rgba(0,160,70,0.60)'],
    ];
    for (const [r, lw, rot, color] of ringDefs) {
      ctx.strokeStyle = color;
      ctx.lineWidth = lw;
      ctx.lineCap = 'round';
      // Each ring is a near-complete arc with a small gap, rotated to suggest swirl
      ctx.beginPath();
      ctx.arc(cx, cy, r, rot, rot + Math.PI * 1.75);
      ctx.stroke();
      // Second arc segment for denser swirl feel
      ctx.globalAlpha = 0.5;
      ctx.beginPath();
      ctx.arc(cx, cy, r * 0.92, rot + Math.PI, rot + Math.PI * 1.9);
      ctx.stroke();
      ctx.globalAlpha = 1.0;
    }

    // Swirl arms — curved strokes emanating inward
    ctx.strokeStyle = utils.rgb(baseGreen, 0.30);
    ctx.lineWidth = 1.0 * s;
    for (let i = 0; i < 4; i++) {
      const startAngle = (i / 4) * Math.PI * 2;
      const sx = cx + Math.cos(startAngle) * 12 * s;
      const sy = cy + Math.sin(startAngle) * 12 * s;
      const cpAngle = startAngle + 0.8;
      const cpx = cx + Math.cos(cpAngle) * 6 * s;
      const cpy = cy + Math.sin(cpAngle) * 6 * s;
      ctx.beginPath();
      ctx.moveTo(sx, sy);
      ctx.quadraticCurveTo(cpx, cpy, cx + Math.cos(startAngle + 1.6) * 2 * s, cy + Math.sin(startAngle + 1.6) * 2 * s);
      ctx.stroke();
    }

    // Inner glow gradient: white → green → transparent
    const innerGlow = ctx.createRadialGradient(cx, cy, 0, cx, cy, 9 * s);
    innerGlow.addColorStop(0,   'rgba(255,255,255,0.55)');
    innerGlow.addColorStop(0.15, 'rgba(180,255,210,0.45)');
    innerGlow.addColorStop(0.40, 'rgba(0,220,100,0.30)');
    innerGlow.addColorStop(0.70, 'rgba(0,160,70,0.12)');
    innerGlow.addColorStop(1,   'rgba(0,100,40,0)');
    ctx.fillStyle = innerGlow;
    utils.fillCircle(ctx, cx, cy, 9 * s);

    // Core bright spot
    const coreGlow = ctx.createRadialGradient(cx, cy, 0, cx, cy, 3 * s);
    coreGlow.addColorStop(0, 'rgba(255,255,255,0.80)');
    coreGlow.addColorStop(0.5, 'rgba(150,255,190,0.50)');
    coreGlow.addColorStop(1, 'rgba(0,200,80,0)');
    ctx.fillStyle = coreGlow;
    utils.fillCircle(ctx, cx, cy, 3 * s);

    // Edge sparkle hints — small bright dots at perimeter
    const sparkleAngles = [0, 0.65, 1.30, 1.95, 2.60, 3.25, 3.90, 4.55, 5.20, 5.85];
    for (let i = 0; i < sparkleAngles.length; i++) {
      const ang = sparkleAngles[i];
      const dist = (i % 3 === 0 ? 13 : i % 3 === 1 ? 12 : 11) * s;
      const sx = cx + Math.cos(ang) * dist;
      const sy = cy + Math.sin(ang) * dist;
      const brightness = i % 2 === 0 ? 0.80 : 0.50;
      ctx.fillStyle = `rgba(180,255,210,${brightness})`;
      utils.fillCircle(ctx, sx, sy, (i % 2 === 0 ? 0.9 : 0.55) * s);
    }

    // Outer dim ring outline
    ctx.strokeStyle = utils.rgb(baseGreen, 0.20);
    ctx.lineWidth = 0.8 * s;
    ctx.beginPath();
    ctx.arc(cx, cy, 14 * s, 0, Math.PI * 2);
    ctx.stroke();
  },
};

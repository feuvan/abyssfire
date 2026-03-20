// src/graphics/sprites/monsters/Slime.ts
import type { EntityDrawer, MonsterAction } from '../types';
import type { DrawUtils } from '../../DrawUtils';

export const SlimeDrawer: EntityDrawer = {
  key: 'monster_slime',
  frameW: 48,
  frameH: 40,
  totalFrames: 20,

  drawFrame(ctx, frame, action, w, h, utils) {
    const act = action as MonsterAction;
    const s = w / 48; // scale factor

    // Animation phase
    const frameCounts: Record<MonsterAction, number> = { idle: 4, walk: 6, attack: 4, hurt: 2, death: 4 };
    const count = frameCounts[act] || 4;
    const localFrame = frame % count;
    const t = count > 1 ? localFrame / (count - 1) : 0;
    const phase = (localFrame / count) * Math.PI * 2;

    // Animation parameters per action
    let squishX = 1, squishY = 1, offsetY = 0, alpha = 1;
    switch (act) {
      case 'idle':
        squishY = 1 + Math.sin(phase) * 0.08;
        squishX = 1 - Math.sin(phase) * 0.05;
        break;
      case 'walk':
        squishY = 1 + Math.sin(phase) * 0.12;
        squishX = 1 - Math.sin(phase) * 0.08;
        offsetY = -Math.abs(Math.sin(phase)) * 3 * s;
        break;
      case 'attack':
        squishX = 1 + t * 0.3;
        squishY = 1 - t * 0.15;
        offsetY = -t * 4 * s;
        break;
      case 'hurt':
        squishX = 1 - t * 0.15;
        squishY = 1 + t * 0.1;
        alpha = 0.7 + t * 0.3;
        break;
      case 'death':
        squishY = 1 - t * 0.7;
        squishX = 1 + t * 0.5;
        alpha = 1 - t * 0.8;
        break;
    }

    ctx.save();
    ctx.globalAlpha = alpha;
    const cx = w / 2, baseY = h * 0.88;

    // Shadow
    ctx.fillStyle = 'rgba(0,0,0,0.25)';
    utils.fillEllipse(ctx, cx, baseY + 2 * s, 17 * s * squishX, 3 * s);

    // Puddle drip
    ctx.fillStyle = 'rgba(7,52,17,0.25)';
    utils.fillEllipse(ctx, cx, baseY, 18 * s * squishX, 4 * s);

    // Main body
    const bodyRx = 16 * s * squishX;
    const bodyRy = 14 * s * squishY;
    const bodyCy = baseY - bodyRy * 0.6 + offsetY;

    // Soft green glow outline around body
    utils.softOutline(ctx, 'rgba(30, 180, 60, 0.3)', 6);

    const grad = ctx.createRadialGradient(
      cx - bodyRx * 0.15, bodyCy - bodyRy * 0.2, 0,
      cx, bodyCy, bodyRx
    );
    grad.addColorStop(0, '#28773b');
    grad.addColorStop(0.5, '#125521');
    grad.addColorStop(1, '#073410');
    ctx.fillStyle = grad;
    ctx.beginPath();
    ctx.ellipse(cx, bodyCy, bodyRx, bodyRy, 0, 0, Math.PI * 2);
    ctx.fill();

    // Subsurface glow
    const glowGrad = ctx.createRadialGradient(cx - 2 * s, bodyCy - 2 * s, 0, cx, bodyCy, bodyRx * 0.7);
    glowGrad.addColorStop(0, 'rgba(63,167,78,0.25)');
    glowGrad.addColorStop(1, 'rgba(18,85,34,0)');
    ctx.fillStyle = glowGrad;
    utils.fillEllipse(ctx, cx - 2 * s, bodyCy - 2 * s, bodyRx * 0.65, bodyRy * 0.6);

    // End soft outline
    utils.softOutlineEnd(ctx);

    // Rim light on body edge
    utils.rimLight(ctx, cx, bodyCy, bodyRx, bodyRy, 'rgba(100,255,120,0.12)');

    // Internal particles
    ctx.fillStyle = 'rgba(7,63,18,0.4)';
    utils.fillCircle(ctx, cx - 6 * s, bodyCy + 2 * s, 2 * s);
    utils.fillCircle(ctx, cx + 4 * s, bodyCy + 4 * s, 1.5 * s);

    // Specular highlight
    ctx.fillStyle = 'rgba(255,255,255,0.07)';
    ctx.save();
    ctx.translate(cx - 6 * s, bodyCy - bodyRy * 0.4);
    ctx.rotate(-0.25);
    utils.fillEllipse(ctx, 0, 0, 5 * s, 3 * s);
    ctx.restore();
    ctx.fillStyle = 'rgba(255,255,255,0.08)';
    utils.fillEllipse(ctx, cx - 7 * s, bodyCy - bodyRy * 0.5, 2.5 * s, 1.5 * s);

    // Eyes
    const eyeSpread = 6 * s * squishX;
    for (const side of [-1, 1]) {
      const ex = cx + side * eyeSpread;
      const ey = bodyCy - bodyRy * 0.15;
      ctx.fillStyle = '#0a3a0a';
      utils.fillEllipse(ctx, ex, ey, 3 * s, 3.5 * s);
      ctx.fillStyle = '#1d7730';  // keep emissive eye glow
      utils.fillEllipse(ctx, ex, ey - 0.5 * s, 2 * s, 2.5 * s);
      ctx.fillStyle = '#0a2a0a';
      utils.fillEllipse(ctx, ex, ey - 1 * s, 1 * s, 1.2 * s);
      ctx.fillStyle = 'rgba(170,255,170,0.5)';  // keep bright reflection
      utils.fillCircle(ctx, ex - 0.5 * s, ey - 1.5 * s, 0.5 * s);
    }

    // Mouth
    ctx.strokeStyle = '#0a3a10';
    ctx.lineWidth = 0.8 * s;
    ctx.beginPath();
    ctx.moveTo(cx - 4 * s, bodyCy + bodyRy * 0.25);
    ctx.quadraticCurveTo(cx, bodyCy + bodyRy * 0.35, cx + 4 * s, bodyCy + bodyRy * 0.25);
    ctx.stroke();

    // Drip tendrils
    if (act !== 'death') {
      ctx.fillStyle = 'rgba(7,52,17,0.6)';
      ctx.beginPath();
      ctx.moveTo(cx - 10 * s, baseY - 2 * s);
      ctx.quadraticCurveTo(cx - 12 * s, baseY + 2 * s, cx - 11 * s, baseY + 4 * s);
      ctx.quadraticCurveTo(cx - 10 * s, baseY + 3 * s, cx - 9 * s, baseY - 1 * s);
      ctx.fill();
    }

    ctx.restore();
  },
};

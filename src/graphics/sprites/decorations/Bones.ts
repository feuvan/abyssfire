// src/graphics/sprites/decorations/Bones.ts
import type { EntityDrawer } from '../types';

export const BonesDrawer: EntityDrawer = {
  key: 'decor_bones',
  frameW: 16,
  frameH: 10,
  totalFrames: 1,

  drawFrame(ctx, _frame, _action, w, h, utils) {
    const s = w / 16;
    const BONE_COLOR = 0xc8be9a;

    // Ground shadow
    ctx.fillStyle = 'rgba(0,0,0,0.12)';
    utils.fillEllipse(ctx, w / 2, h - 0.6 * s, 7 * s, 1.2 * s);

    // Partially buried long bone (horizontal, slightly angled)
    utils.drawBoneSegment(ctx, w * 0.08, h * 0.62, w * 0.68, h * 0.50, 2.2 * s, BONE_COLOR);

    // Second scattered bone (diagonal)
    utils.drawBoneSegment(ctx, w * 0.55, h * 0.75, w * 0.92, h * 0.38, 1.8 * s, BONE_COLOR);

    // Small rib fragment
    utils.drawBoneSegment(ctx, w * 0.20, h * 0.30, w * 0.40, h * 0.22, 1.2 * s, BONE_COLOR);

    // Cracked skull fragment (partially buried — only partial circle visible)
    const skullX = w * 0.72, skullY = h * 0.68;
    ctx.fillStyle = utils.rgb(utils.darken(BONE_COLOR, 5));
    ctx.beginPath();
    ctx.arc(skullX, skullY, 2.8 * s, Math.PI, Math.PI * 2);
    ctx.fill();
    // Skull crack
    ctx.strokeStyle = utils.rgb(utils.darken(BONE_COLOR, 35), 0.7);
    ctx.lineWidth = 0.5 * s;
    ctx.beginPath();
    ctx.moveTo(skullX, skullY - 2.8 * s);
    ctx.quadraticCurveTo(skullX + 0.8 * s, skullY - 1.5 * s, skullX + 1.5 * s, skullY - 0.5 * s);
    ctx.stroke();
    // Skull eye socket hint
    ctx.fillStyle = utils.rgb(0x30282a, 0.55);
    utils.fillEllipse(ctx, skullX - 0.8 * s, skullY - 1.2 * s, 0.9 * s, 0.7 * s);

    // Dirt/burial overlay at bottom (partially buried effect)
    ctx.fillStyle = 'rgba(60,45,25,0.22)';
    ctx.fillRect(0, h * 0.80, w, h * 0.20);
  },
};

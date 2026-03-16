// src/graphics/sprites/npcs/BlacksmithAdvanced.ts
import type { EntityDrawer, NPCAction } from '../types';
import type { DrawUtils } from '../../DrawUtils';

const SKIN        = 0xa08060;
const SKIN_DARK   = 0x806040;
const SKIN_LIGHT  = 0xc09878;
const HAIR        = 0x1a1a2a;  // very dark, almost black
const APRON       = 0x3a3a4a;
const APRON_TRIM  = 0x6a3050;
const SHIRT       = 0x5a5a6a;
const METAL_HEAD  = 0x8a8a9a;
const METAL_LIGHT = 0xc0c0d0;
const WOOD_HANDLE = 0x3a2810;
const BOOT        = 0x1a1a2a;
const BELT        = 0x2a2a3a;
const ORB_COLOR   = 0x9a30a0;  // ornate hammer orb accent

export const BlacksmithAdvancedDrawer: EntityDrawer = {
  key: 'npc_blacksmith_advanced',
  frameW: 80,
  frameH: 120,
  totalFrames: 24,

  drawFrame(ctx, frame, action, w, h, utils) {
    const act = action as NPCAction;
    const s = w / 80;

    const frameCounts: Record<NPCAction, number> = {
      working: 8, alert: 4, idle: 6, talking: 6,
    };
    const count = frameCounts[act] || 6;
    const t = count > 1 ? frame / (count - 1) : 0;
    const phase = (frame / count) * Math.PI * 2;

    const cx = w / 2;
    const ground = h * 0.96;

    let bob = 0;
    let hammerRot = 0;
    let hammerY = 0;
    let bodyLean = 0;
    let headTilt = 0;
    let leftArmSwing = 0;
    let mouthOpen = false;
    let eyebrowRaise = 0;

    switch (act) {
      case 'working': {
        const strike = Math.sin(phase);
        bob = strike > 0 ? -strike * 3 * s : 0;
        hammerRot = strike > 0 ? -strike * 0.65 : strike * 0.2;
        hammerY = strike > 0 ? -strike * 16 * s : strike * 5 * s;
        bodyLean = strike * 2.5 * s;
        headTilt = strike * 0.04;
        leftArmSwing = Math.sin(phase + 0.5) * 3 * s;
        break;
      }
      case 'alert': {
        bob = t * -2 * s;
        headTilt = t * -0.05;
        eyebrowRaise = 2 + t * 2;
        break;
      }
      case 'idle': {
        bob = Math.sin(phase) * 1.2 * s;
        bodyLean = Math.sin(phase * 0.5) * 1 * s;
        leftArmSwing = Math.sin(phase) * 1.5 * s;
        hammerY = Math.sin(phase) * 1 * s;
        headTilt = Math.sin(phase + 1) * 0.012;
        break;
      }
      case 'talking': {
        bob = Math.sin(phase) * 0.8 * s;
        bodyLean = -1.5 * s + Math.sin(phase) * 1 * s;
        leftArmSwing = Math.sin(phase * 2) * 7 * s;
        hammerY = -8 * s + Math.sin(phase) * 2 * s;
        headTilt = Math.sin(phase * 1.5) * 0.03;
        mouthOpen = Math.sin(phase * 3) > 0.2;
        eyebrowRaise = Math.sin(phase * 2) > 0 ? 1.5 : 0;
        break;
      }
    }

    ctx.save();
    const by = ground + bob;

    // Shadow
    ctx.fillStyle = 'rgba(0,0,0,0.28)';
    utils.fillEllipse(ctx, cx, ground + 3 * s, 24 * s, 5 * s);

    // ── Boots ──
    for (const side of [-1, 1]) {
      const bx = cx + side * 7 * s;
      utils.drawMetalSurface(ctx, bx - 5 * s, by - 7 * s, 11 * s, 8 * s, BOOT);
      // Boot buckle
      ctx.fillStyle = utils.rgb(0x6a6a80);
      ctx.fillRect(bx - 1 * s, by - 6 * s, 2 * s, 3 * s);
    }

    // ── Legs ──
    const legColor = utils.darken(APRON, 20);
    utils.drawPart(ctx, cx - 13 * s, by - 26 * s, 10 * s, 21 * s, legColor, 3 * s);
    utils.drawPart(ctx, cx + 3 * s, by - 26 * s, 10 * s, 21 * s, legColor, 3 * s);

    // ── Apron (heavier, with trim) ──
    utils.drawLeatherTexture(ctx, cx - 15 * s + bodyLean, by - 54 * s, 30 * s, 36 * s, APRON);
    // Trim strips
    ctx.fillStyle = utils.rgb(APRON_TRIM, 0.8);
    ctx.fillRect(cx - 14 * s + bodyLean, by - 54 * s, 3 * s, 36 * s);
    ctx.fillRect(cx + 11 * s + bodyLean, by - 54 * s, 3 * s, 36 * s);
    ctx.strokeStyle = utils.rgb(utils.darken(APRON, 30), 0.5);
    ctx.lineWidth = 0.8 * s;
    utils.roundRect(ctx, cx - 15 * s + bodyLean, by - 54 * s, 30 * s, 36 * s, 3 * s);
    ctx.stroke();

    // ── Shirt (broad chest) ──
    utils.drawPart(ctx, cx - 14 * s + bodyLean, by - 52 * s, 28 * s, 32 * s, SHIRT, 4 * s);

    // ── Belt ──
    utils.drawPart(ctx, cx - 14 * s + bodyLean, by - 23 * s, 28 * s, 5 * s, BELT, 1 * s);
    ctx.fillStyle = utils.rgb(0x8a7040);
    utils.roundRect(ctx, cx - 2.5 * s + bodyLean, by - 22.5 * s, 5 * s, 4 * s, 1 * s);
    ctx.fill();

    // ── Left arm (bare, very muscular) ──
    const laBaseY = by - 50 * s;
    const laX = cx - 21 * s + bodyLean;
    utils.drawLimb(ctx, [
      { x: laX + 4 * s, y: laBaseY },
      { x: laX + 3 * s, y: laBaseY + 11 * s },
      { x: laX + 3 * s + leftArmSwing * 0.5, y: laBaseY + 20 * s + leftArmSwing },
    ], 6 * s, SKIN);
    ctx.fillStyle = utils.rgb(SKIN);
    utils.fillCircle(ctx, laX + 3 * s + leftArmSwing * 0.5, laBaseY + 20 * s + leftArmSwing, 4.5 * s);

    // ── Right arm ──
    const raBaseX = cx + 17 * s + bodyLean;
    const raBaseY = by - 50 * s;
    const armEndX = raBaseX + Math.sin(hammerRot) * 13 * s;
    const armEndY = raBaseY + hammerY + 20 * s * Math.cos(hammerRot * 0.5);
    utils.drawLimb(ctx, [
      { x: raBaseX, y: raBaseY },
      { x: raBaseX + Math.sin(hammerRot * 0.4) * 6 * s, y: raBaseY + 11 * s + hammerY * 0.4 },
      { x: armEndX, y: armEndY },
    ], 6 * s, SKIN);
    ctx.fillStyle = utils.rgb(SKIN);
    utils.fillCircle(ctx, armEndX, armEndY, 4.5 * s);

    // ── Ornate hammer (drawn so handle extends AWAY from body) ──
    ctx.save();
    ctx.translate(armEndX, armEndY);
    ctx.rotate(hammerRot + 0.5);
    // Handle — extends upward-right from hand, away from body
    ctx.fillStyle = utils.rgb(WOOD_HANDLE);
    ctx.fillRect(-1.5 * s, -22 * s, 3 * s, 22 * s);
    // Ornate metal head
    utils.drawMetalSurface(ctx, -7 * s, -31 * s, 14 * s, 9 * s, METAL_HEAD);
    // Engravings on head
    ctx.strokeStyle = utils.rgb(utils.darken(METAL_HEAD, 30), 0.6);
    ctx.lineWidth = 0.5 * s;
    ctx.beginPath();
    ctx.moveTo(-5 * s, -30 * s); ctx.lineTo(5 * s, -30 * s);
    ctx.stroke();
    ctx.beginPath();
    ctx.moveTo(-5 * s, -28 * s); ctx.lineTo(5 * s, -28 * s);
    ctx.stroke();
    // Ornate orb accent on hammer
    const orbGrad = ctx.createRadialGradient(0, -32 * s, 0, 0, -32 * s, 2.5 * s);
    orbGrad.addColorStop(0, utils.rgb(utils.lighten(ORB_COLOR, 40)));
    orbGrad.addColorStop(1, utils.rgb(ORB_COLOR));
    ctx.fillStyle = orbGrad;
    utils.fillCircle(ctx, 0, -32 * s, 2.5 * s);
    ctx.restore();

    // ── Neck ──
    ctx.fillStyle = utils.rgb(SKIN_DARK);
    utils.roundRect(ctx, cx - 5 * s + bodyLean * 0.3, by - 58 * s, 10 * s, 8 * s, 2 * s);
    ctx.fill();

    // ── Head (bald) ──
    const headX = cx + bodyLean * 0.3;
    const headY = by - 66 * s;

    ctx.save();
    ctx.translate(headX, headY);
    ctx.rotate(headTilt);

    // Bald head — shiny dome
    const headGrad = ctx.createRadialGradient(-4 * s, -6 * s, 0, 0, 0, 14 * s);
    headGrad.addColorStop(0, utils.rgb(SKIN_LIGHT));
    headGrad.addColorStop(0.4, utils.rgb(SKIN));
    headGrad.addColorStop(1, utils.rgb(SKIN_DARK));
    ctx.fillStyle = headGrad;
    utils.roundRect(ctx, -12 * s, -12 * s, 24 * s, 22 * s, 5 * s);
    ctx.fill();
    ctx.strokeStyle = utils.rgb(SKIN_DARK, 0.3);
    ctx.lineWidth = 0.8 * s;
    utils.roundRect(ctx, -12 * s, -12 * s, 24 * s, 22 * s, 5 * s);
    ctx.stroke();
    // Shiny bald shine
    ctx.fillStyle = 'rgba(255,255,255,0.12)';
    utils.fillEllipse(ctx, -5 * s, -9 * s, 4 * s, 2.5 * s);

    // Stubble pattern
    ctx.fillStyle = utils.rgb(HAIR, 0.12);
    for (let i = 0; i < 6; i++) {
      const sx = -8 * s + i * 3 * s;
      utils.fillEllipse(ctx, sx, 2 * s, 1 * s, 1.5 * s);
    }

    // Heavy brow ridge
    ctx.strokeStyle = utils.rgb(SKIN_DARK, 0.7);
    ctx.lineWidth = 3 * s;
    ctx.lineCap = 'round';
    ctx.beginPath();
    ctx.moveTo(-10 * s, -4 * s); ctx.lineTo(-3 * s, -5 * s);
    ctx.stroke();
    ctx.beginPath();
    ctx.moveTo(10 * s, -4 * s); ctx.lineTo(3 * s, -5 * s);
    ctx.stroke();

    // Eyebrows
    const browY = -5 * s - eyebrowRaise * s;
    ctx.strokeStyle = utils.rgb(HAIR, 0.8);
    ctx.lineWidth = 2 * s;
    ctx.beginPath();
    ctx.moveTo(-9 * s, browY); ctx.lineTo(-3 * s, browY + 1.5 * s);
    ctx.stroke();
    ctx.beginPath();
    ctx.moveTo(9 * s, browY); ctx.lineTo(3 * s, browY + 1.5 * s);
    ctx.stroke();

    // Eyes (darker skin, sharper look)
    ctx.fillStyle = '#d8d0c8';
    utils.fillEllipse(ctx, -5.5 * s, -1 * s, 3 * s, 2.5 * s);
    utils.fillEllipse(ctx, 5.5 * s, -1 * s, 3 * s, 2.5 * s);
    ctx.fillStyle = '#1a1010';
    utils.fillEllipse(ctx, -5.5 * s, -1 * s, 1.8 * s, 2 * s);
    utils.fillEllipse(ctx, 5.5 * s, -1 * s, 1.8 * s, 2 * s);
    ctx.fillStyle = 'rgba(255,255,255,0.55)';
    utils.fillCircle(ctx, -4.5 * s, -2 * s, 0.7 * s);
    utils.fillCircle(ctx, 6.5 * s, -2 * s, 0.7 * s);

    // Nose
    ctx.fillStyle = utils.rgb(SKIN_DARK, 0.5);
    utils.fillEllipse(ctx, 0, 2.5 * s, 2.5 * s, 2 * s);

    // Mouth
    if (mouthOpen) {
      ctx.fillStyle = '#3a1a0a';
      utils.fillEllipse(ctx, 0, 7 * s, 3.5 * s, 2 * s);
    } else {
      ctx.strokeStyle = utils.rgb(SKIN_DARK, 0.8);
      ctx.lineWidth = 1.5 * s;
      ctx.lineCap = 'round';
      ctx.beginPath();
      ctx.moveTo(-4 * s, 6.5 * s);
      ctx.quadraticCurveTo(0, 8 * s, 4 * s, 6.5 * s);
      ctx.stroke();
    }

    // Full beard (darker skin, darker beard)
    ctx.fillStyle = utils.rgb(HAIR);
    ctx.beginPath();
    ctx.moveTo(-9 * s, 6 * s);
    ctx.lineTo(9 * s, 6 * s);
    ctx.lineTo(7 * s, 17 * s);
    ctx.lineTo(0, 21 * s);
    ctx.lineTo(-7 * s, 17 * s);
    ctx.closePath();
    ctx.fill();
    ctx.fillStyle = utils.rgb(utils.lighten(HAIR, 20), 0.2);
    ctx.beginPath();
    ctx.moveTo(-3 * s, 7 * s); ctx.lineTo(3 * s, 7 * s);
    ctx.lineTo(1 * s, 13 * s); ctx.lineTo(-1 * s, 13 * s);
    ctx.closePath();
    ctx.fill();

    ctx.restore(); // head
    ctx.restore(); // frame
  },
};

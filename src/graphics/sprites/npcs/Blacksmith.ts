// src/graphics/sprites/npcs/Blacksmith.ts
import type { EntityDrawer, NPCAction } from '../types';
import type { DrawUtils } from '../../DrawUtils';

const SKIN        = 0xc09070;
const SKIN_DARK   = 0x9a7050;
const SKIN_LIGHT  = 0xd4a888;
const HAIR        = 0x2a1a0a;
const APRON       = 0x5a3a1a;
const APRON_DARK  = 0x3a2008;
const SHIRT       = 0x8a6a4a;
const METAL_HEAD  = 0x6a6a6a;
const METAL_LIGHT = 0x9a9aaa;
const WOOD_HANDLE = 0x4a3018;
const BOOT        = 0x2a1a0a;
const BELT        = 0x3a2810;

export const BlacksmithDrawer: EntityDrawer = {
  key: 'npc_blacksmith',
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

    // --- Animation state ---
    let bob = 0;
    let hammerRot = 0;      // right arm rotation (radians)
    let hammerY = 0;        // extra Y offset for hammer arm
    let bodyLean = 0;
    let headTilt = 0;
    let leftArmSwing = 0;
    let mouthOpen = false;
    let eyebrowRaise = 0;

    switch (act) {
      case 'working': {
        // Hammer strike cycle: raise up then slam down
        const strike = Math.sin(phase);
        bob = strike > 0 ? -strike * 3 * s : 0;
        hammerRot = strike > 0 ? -strike * 0.7 : strike * 0.25;
        hammerY = strike > 0 ? -strike * 14 * s : strike * 4 * s;
        bodyLean = strike * 2 * s;
        headTilt = strike * 0.05;
        leftArmSwing = Math.sin(phase + 0.5) * 3 * s;
        break;
      }
      case 'alert': {
        // Look up from work, straighten
        const tA = t;
        bob = tA * -2 * s;
        bodyLean = 0;
        headTilt = tA * -0.06;
        eyebrowRaise = 2 + tA * 2;
        break;
      }
      case 'idle': {
        bob = Math.sin(phase) * 1.5 * s;
        bodyLean = Math.sin(phase * 0.5) * 1.5 * s;
        leftArmSwing = Math.sin(phase) * 2 * s;
        hammerY = Math.sin(phase) * 1 * s;
        headTilt = Math.sin(phase + 1) * 0.015;
        break;
      }
      case 'talking': {
        bob = Math.sin(phase) * 1 * s;
        bodyLean = -1.5 * s + Math.sin(phase) * 1 * s;
        leftArmSwing = Math.sin(phase * 2) * 6 * s;
        hammerY = -8 * s + Math.sin(phase) * 2 * s;  // hammer on shoulder
        headTilt = Math.sin(phase * 1.5) * 0.03;
        mouthOpen = Math.sin(phase * 3) > 0.2;
        eyebrowRaise = Math.sin(phase * 2) > 0 ? 1.5 : 0;
        break;
      }
    }

    ctx.save();
    const by = ground + bob;

    // Shadow
    ctx.fillStyle = 'rgba(0,0,0,0.25)';
    utils.fillEllipse(ctx, cx, ground + 3 * s, 22 * s, 5 * s);

    // ── Boots ──
    for (const side of [-1, 1]) {
      const bx = cx + side * 7 * s;
      utils.drawPart(ctx, bx - 5 * s, by - 6 * s, 10 * s, 7 * s, BOOT, 2 * s);
    }

    // ── Legs ──
    const legColor = 0x3a2a1a;
    utils.drawPart(ctx, cx - 12 * s, by - 24 * s, 9 * s, 20 * s, legColor, 3 * s);
    utils.drawPart(ctx, cx + 3 * s, by - 24 * s, 9 * s, 20 * s, legColor, 3 * s);

    // ── Apron (behind torso) ──
    utils.drawLeatherTexture(ctx, cx - 14 * s + bodyLean, by - 52 * s, 28 * s, 34 * s, APRON);
    ctx.strokeStyle = utils.rgb(APRON_DARK, 0.6);
    ctx.lineWidth = 0.8 * s;
    utils.roundRect(ctx, cx - 14 * s + bodyLean, by - 52 * s, 28 * s, 34 * s, 3 * s);
    ctx.stroke();

    // ── Torso (shirt visible at sides) ──
    utils.drawPart(ctx, cx - 13 * s + bodyLean, by - 50 * s, 26 * s, 30 * s, SHIRT, 4 * s);

    // ── Belt ──
    utils.drawPart(ctx, cx - 13 * s + bodyLean, by - 22 * s, 26 * s, 5 * s, BELT, 1 * s);
    ctx.fillStyle = utils.rgb(0x8a7040);
    utils.roundRect(ctx, cx - 2 * s + bodyLean, by - 22 * s + 0.5 * s, 5 * s, 4 * s, 1 * s);
    ctx.fill();

    // ── Left arm (bare, muscular) ──
    const laBaseY = by - 48 * s;
    const laX = cx - 19 * s + bodyLean;
    utils.drawLimb(ctx, [
      { x: laX + 3 * s, y: laBaseY },
      { x: laX + 2 * s, y: laBaseY + 10 * s },
      { x: laX + 2 * s + leftArmSwing * 0.5, y: laBaseY + 18 * s + leftArmSwing },
    ], 5 * s, SKIN);
    // Left hand
    ctx.fillStyle = utils.rgb(SKIN);
    utils.fillCircle(ctx, laX + 2 * s + leftArmSwing * 0.5, laBaseY + 18 * s + leftArmSwing, 4 * s);

    // ── Right arm (holding hammer) ──
    const raBaseX = cx + 15 * s + bodyLean;
    const raBaseY = by - 48 * s;
    // Arm position driven by hammerRot
    const armEndX = raBaseX + Math.sin(hammerRot) * 12 * s;
    const armEndY = raBaseY + hammerY + 18 * s * Math.cos(hammerRot * 0.5);
    utils.drawLimb(ctx, [
      { x: raBaseX, y: raBaseY },
      { x: raBaseX + Math.sin(hammerRot * 0.4) * 6 * s, y: raBaseY + 10 * s + hammerY * 0.4 },
      { x: armEndX, y: armEndY },
    ], 5 * s, SKIN);
    // Right hand
    ctx.fillStyle = utils.rgb(SKIN);
    utils.fillCircle(ctx, armEndX, armEndY, 4 * s);

    // ── Hammer in right hand (handle extends upward, not through body) ──
    ctx.save();
    ctx.translate(armEndX, armEndY);
    ctx.rotate(hammerRot + 0.5);
    // Handle — extends upward-right from hand, away from body
    ctx.fillStyle = utils.rgb(WOOD_HANDLE);
    ctx.fillRect(-1.5 * s, -20 * s, 3 * s, 20 * s);
    // Head
    utils.drawMetalSurface(ctx, -6 * s, -28 * s, 12 * s, 8 * s, METAL_HEAD);
    // Metal highlight
    ctx.strokeStyle = utils.rgb(METAL_LIGHT, 0.4);
    ctx.lineWidth = 0.8 * s;
    ctx.beginPath();
    ctx.moveTo(-5 * s, -27 * s); ctx.lineTo(5 * s, -27 * s);
    ctx.stroke();
    ctx.restore();

    // ── Neck ──
    ctx.fillStyle = utils.rgb(SKIN_DARK);
    utils.roundRect(ctx, cx - 5 * s + bodyLean, by - 56 * s, 10 * s, 8 * s, 2 * s);
    ctx.fill();

    // ── Head ──
    const headX = cx + bodyLean * 0.3;
    const headY = by - 64 * s;

    ctx.save();
    ctx.translate(headX, headY);
    ctx.rotate(headTilt);

    // Short dark hair (top)
    ctx.fillStyle = utils.rgb(HAIR);
    utils.roundRect(ctx, -11 * s, -12 * s, 22 * s, 12 * s, 4 * s);
    ctx.fill();
    // Sideburns
    ctx.fillRect(-11 * s, -2 * s, 3 * s, 6 * s);
    ctx.fillRect(8 * s, -2 * s, 3 * s, 6 * s);

    // Square jaw head
    const headGrad = ctx.createRadialGradient(-3 * s, -4 * s, 0, 0, 0, 13 * s);
    headGrad.addColorStop(0, utils.rgb(SKIN_LIGHT));
    headGrad.addColorStop(0.5, utils.rgb(SKIN));
    headGrad.addColorStop(1, utils.rgb(SKIN_DARK));
    ctx.fillStyle = headGrad;
    utils.roundRect(ctx, -11 * s, -10 * s, 22 * s, 20 * s, 4 * s);
    ctx.fill();
    ctx.strokeStyle = utils.rgb(SKIN_DARK, 0.4);
    ctx.lineWidth = 0.8 * s;
    utils.roundRect(ctx, -11 * s, -10 * s, 22 * s, 20 * s, 4 * s);
    ctx.stroke();

    // Eyebrows (thick, furrowed)
    const browY = -5 * s - eyebrowRaise * s;
    ctx.strokeStyle = utils.rgb(HAIR);
    ctx.lineWidth = 2 * s;
    ctx.lineCap = 'round';
    ctx.beginPath();
    ctx.moveTo(-8 * s, browY); ctx.lineTo(-3 * s, browY + 1 * s);
    ctx.stroke();
    ctx.beginPath();
    ctx.moveTo(8 * s, browY); ctx.lineTo(3 * s, browY + 1 * s);
    ctx.stroke();

    // Eyes
    ctx.fillStyle = '#e0dcd8';
    utils.fillEllipse(ctx, -5 * s, -1.5 * s, 3 * s, 2.5 * s);
    utils.fillEllipse(ctx, 5 * s, -1.5 * s, 3 * s, 2.5 * s);
    ctx.fillStyle = '#2a1a10';
    utils.fillEllipse(ctx, -5 * s, -1.5 * s, 1.8 * s, 2 * s);
    utils.fillEllipse(ctx, 5 * s, -1.5 * s, 1.8 * s, 2 * s);
    ctx.fillStyle = 'rgba(255,255,255,0.6)';
    utils.fillCircle(ctx, -4 * s, -2.5 * s, 0.7 * s);
    utils.fillCircle(ctx, 6 * s, -2.5 * s, 0.7 * s);

    // Nose (broad)
    ctx.fillStyle = utils.rgb(SKIN_DARK, 0.5);
    utils.fillEllipse(ctx, 0, 2 * s, 2.5 * s, 2 * s);

    // Mouth
    if (mouthOpen) {
      ctx.fillStyle = '#3a1a0a';
      utils.fillEllipse(ctx, 0, 6.5 * s, 3 * s, 2 * s);
    } else {
      ctx.strokeStyle = utils.rgb(SKIN_DARK, 0.7);
      ctx.lineWidth = 1.2 * s;
      ctx.beginPath();
      ctx.moveTo(-4 * s, 6 * s);
      ctx.quadraticCurveTo(0, 7.5 * s, 4 * s, 6 * s);
      ctx.stroke();
    }

    // Full beard
    ctx.fillStyle = utils.rgb(HAIR);
    ctx.beginPath();
    ctx.moveTo(-9 * s, 6 * s);
    ctx.lineTo(9 * s, 6 * s);
    ctx.lineTo(7 * s, 16 * s);
    ctx.lineTo(0, 20 * s);
    ctx.lineTo(-7 * s, 16 * s);
    ctx.closePath();
    ctx.fill();
    // Beard highlight
    ctx.fillStyle = utils.rgb(utils.lighten(HAIR, 15), 0.25);
    ctx.beginPath();
    ctx.moveTo(-3 * s, 7 * s); ctx.lineTo(3 * s, 7 * s);
    ctx.lineTo(1 * s, 13 * s); ctx.lineTo(-1 * s, 13 * s);
    ctx.closePath();
    ctx.fill();

    // Sweat drop (working state)
    if (act === 'working') {
      ctx.fillStyle = 'rgba(150,200,240,0.5)';
      utils.fillEllipse(ctx, 10 * s, -8 * s, 1.5 * s, 2 * s);
    }

    ctx.restore(); // head

    ctx.restore(); // frame
  },
};

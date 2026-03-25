import type { EntityDrawer, NPCAction } from '../types';

const SKIN = 0x9d7d5c;
const SKIN_DARK = 0x7e623b;
const SKIN_LIGHT = 0xb1976f;
const CLOAK = 0x3a4a30;
const CLOAK_DARK = 0x2a3a20;
const SHIRT = 0x8a7a60;
const BELT = 0x4a3015;
const BOOT = 0x3a2a1a;
const PACK = 0x6a5a40;
const COIN = 0xd4aa30;

export const WanderingMerchantDrawer: EntityDrawer = {
  key: 'npc_wandering_merchant',
  frameW: 80,
  frameH: 120,
  totalFrames: 24,

  drawFrame(ctx, frame, action, w, h, utils) {
    const act = action as NPCAction;
    const s = w / 80;
    const frameCounts: Record<NPCAction, number> = { working: 8, alert: 4, idle: 6, talking: 6 };
    const count = frameCounts[act] || 6;
    const phase = (frame / count) * Math.PI * 2;

    const cx = w / 2;
    const ground = h * 0.96;

    let bob = 0;
    let bodyLean = 0;
    let headTilt = 0;
    let leftArmY = 0;
    let rightArmY = 0;
    let mouthOpen = false;

    switch (act) {
      case 'working':
        bob = Math.sin(phase) * 1.5 * s;
        bodyLean = Math.sin(phase) * 1 * s;
        rightArmY = Math.sin(phase * 2) * 4 * s;
        leftArmY = Math.sin(phase + 1) * 2 * s;
        break;
      case 'alert':
        bob = -2 * s;
        headTilt = -0.04;
        break;
      case 'idle':
        bob = Math.sin(phase) * 1 * s;
        bodyLean = Math.sin(phase * 0.5) * 1 * s;
        leftArmY = Math.sin(phase) * 2 * s;
        rightArmY = Math.sin(phase + Math.PI) * 2 * s;
        headTilt = Math.sin(phase) * 0.015;
        break;
      case 'talking':
        bob = Math.sin(phase) * 1 * s;
        bodyLean = Math.sin(phase) * 1.5 * s;
        leftArmY = Math.sin(phase * 2) * 4 * s;
        rightArmY = Math.sin(phase * 2 + Math.PI) * 3 * s;
        mouthOpen = Math.sin(phase * 3) > 0.3;
        headTilt = Math.sin(phase * 1.5) * 0.02;
        break;
    }

    ctx.save();
    const by = ground + bob;

    // Shadow
    ctx.fillStyle = 'rgba(0,0,0,0.25)';
    utils.fillEllipse(ctx, cx, ground + 3 * s, 18 * s, 4 * s);

    // Boots (worn travel boots)
    for (const side of [-1, 1]) {
      utils.drawPart(ctx, cx + side * 6 * s - 4 * s, by - 7 * s, 8 * s, 8 * s, BOOT, 2 * s);
    }

    // Legs
    utils.drawPart(ctx, cx - 10 * s, by - 24 * s, 7 * s, 19 * s, utils.darken(SHIRT, 15), 3 * s);
    utils.drawPart(ctx, cx + 3 * s, by - 24 * s, 7 * s, 19 * s, utils.darken(SHIRT, 15), 3 * s);

    utils.zoneNpcOutline(ctx, w, h);

    // Body / shirt
    utils.drawPart(ctx, cx - 12 * s + bodyLean, by - 48 * s, 24 * s, 28 * s, SHIRT, 5 * s);

    // Cloak (draped over shoulders)
    ctx.fillStyle = utils.rgb(CLOAK);
    ctx.beginPath();
    ctx.moveTo(cx - 14 * s + bodyLean, by - 48 * s);
    ctx.lineTo(cx - 16 * s + bodyLean, by - 18 * s);
    ctx.lineTo(cx - 8 * s + bodyLean, by - 18 * s);
    ctx.lineTo(cx - 10 * s + bodyLean, by - 48 * s);
    ctx.closePath();
    ctx.fill();
    ctx.beginPath();
    ctx.moveTo(cx + 10 * s + bodyLean, by - 48 * s);
    ctx.lineTo(cx + 8 * s + bodyLean, by - 18 * s);
    ctx.lineTo(cx + 16 * s + bodyLean, by - 18 * s);
    ctx.lineTo(cx + 14 * s + bodyLean, by - 48 * s);
    ctx.closePath();
    ctx.fill();

    // Belt with pouch
    utils.drawPart(ctx, cx - 11 * s + bodyLean, by - 22 * s, 22 * s, 4 * s, BELT, 1 * s);
    ctx.fillStyle = utils.rgb(PACK, 0.9);
    utils.roundRect(ctx, cx + 6 * s + bodyLean, by - 22 * s, 6 * s, 6 * s, 1.5 * s);
    ctx.fill();

    utils.softOutlineEnd(ctx);

    // Backpack (behind, peeking out)
    ctx.fillStyle = utils.rgb(PACK);
    utils.roundRect(ctx, cx - 16 * s + bodyLean, by - 44 * s, 8 * s, 16 * s, 3 * s);
    ctx.fill();
    ctx.fillStyle = utils.rgb(utils.darken(PACK, 15));
    ctx.fillRect(cx - 15 * s + bodyLean, by - 38 * s, 6 * s, 1 * s);
    ctx.fillRect(cx - 15 * s + bodyLean, by - 34 * s, 6 * s, 1 * s);

    // Left arm
    const laX = cx - 16 * s + bodyLean;
    const laY = by - 44 * s;
    utils.drawLimb(ctx, [
      { x: laX + 3 * s, y: laY },
      { x: laX + 2 * s, y: laY + 10 * s },
      { x: laX + 3 * s, y: laY + 16 * s + leftArmY },
    ], 4 * s, CLOAK_DARK);
    ctx.fillStyle = utils.rgb(SKIN);
    utils.fillCircle(ctx, laX + 3 * s, laY + 16 * s + leftArmY, 3 * s);

    // Walking staff in left hand
    const staffX = laX + 3 * s;
    const staffTopY = laY + 6 * s + leftArmY;
    ctx.strokeStyle = utils.rgb(0x5a4a30);
    ctx.lineWidth = 2 * s;
    ctx.beginPath();
    ctx.moveTo(staffX, staffTopY - 14 * s);
    ctx.lineTo(staffX + 1 * s, by - 3 * s);
    ctx.stroke();

    // Right arm
    const raX = cx + 12 * s + bodyLean;
    const raY = by - 44 * s;
    utils.drawLimb(ctx, [
      { x: raX + 1 * s, y: raY },
      { x: raX + 2 * s, y: raY + 10 * s },
      { x: raX + 2 * s, y: raY + 16 * s + rightArmY },
    ], 4 * s, CLOAK_DARK);
    ctx.fillStyle = utils.rgb(SKIN);
    utils.fillCircle(ctx, raX + 2 * s, raY + 16 * s + rightArmY, 3 * s);

    // Dangling coin bag in right hand when talking
    if (act === 'talking' || act === 'working') {
      const rhX = raX + 2 * s;
      const rhY = raY + 16 * s + rightArmY;
      ctx.fillStyle = utils.rgb(PACK);
      utils.fillCircle(ctx, rhX, rhY - 4 * s, 4 * s);
      ctx.fillStyle = utils.rgb(COIN, 0.7);
      utils.fillCircle(ctx, rhX, rhY - 4 * s, 2 * s);
    }

    // Neck
    ctx.fillStyle = utils.rgb(SKIN);
    utils.roundRect(ctx, cx - 3.5 * s + bodyLean * 0.3, by - 53 * s, 7 * s, 6 * s, 2 * s);
    ctx.fill();

    // Head
    const headX = cx + bodyLean * 0.3;
    const headY = by - 60 * s;
    ctx.save();
    ctx.translate(headX, headY);
    ctx.rotate(headTilt);

    // Hood (from cloak)
    ctx.fillStyle = utils.rgb(CLOAK);
    ctx.beginPath();
    ctx.moveTo(-12 * s, -6 * s);
    ctx.quadraticCurveTo(0, -16 * s, 12 * s, -6 * s);
    ctx.lineTo(10 * s, 2 * s);
    ctx.lineTo(-10 * s, 2 * s);
    ctx.closePath();
    ctx.fill();

    // Face
    const headGrad = ctx.createRadialGradient(-2 * s, -2 * s, 0, 0, 0, 10 * s);
    headGrad.addColorStop(0, utils.rgb(SKIN_LIGHT));
    headGrad.addColorStop(0.5, utils.rgb(SKIN));
    headGrad.addColorStop(1, utils.rgb(SKIN_DARK));
    ctx.fillStyle = headGrad;
    utils.roundRect(ctx, -9 * s, -8 * s, 18 * s, 16 * s, 4 * s);
    ctx.fill();

    // Beard (scruffy traveler)
    ctx.fillStyle = utils.rgb(0x5a4a3a, 0.6);
    ctx.beginPath();
    ctx.moveTo(-6 * s, 3 * s);
    ctx.quadraticCurveTo(0, 10 * s, 6 * s, 3 * s);
    ctx.closePath();
    ctx.fill();

    // Eyes (weathered)
    ctx.fillStyle = '#e8e4e0';
    utils.fillEllipse(ctx, -4 * s, -1 * s, 2.5 * s, 2.2 * s);
    utils.fillEllipse(ctx, 4 * s, -1 * s, 2.5 * s, 2.2 * s);
    ctx.fillStyle = '#3a2818';
    utils.fillCircle(ctx, -4 * s, -1 * s, 1.5 * s);
    utils.fillCircle(ctx, 4 * s, -1 * s, 1.5 * s);
    ctx.fillStyle = 'rgba(255,255,255,0.5)';
    utils.fillCircle(ctx, -3 * s, -2 * s, 0.6 * s);
    utils.fillCircle(ctx, 5 * s, -2 * s, 0.6 * s);

    // Nose
    ctx.fillStyle = utils.rgb(SKIN_DARK, 0.4);
    utils.fillEllipse(ctx, 0, 2 * s, 1.8 * s, 1.5 * s);

    // Mouth
    if (mouthOpen) {
      ctx.fillStyle = '#3a1a0a';
      utils.fillEllipse(ctx, 0, 5 * s, 2.5 * s, 1.5 * s);
    } else {
      ctx.strokeStyle = utils.rgb(SKIN_DARK, 0.6);
      ctx.lineWidth = 1 * s;
      ctx.beginPath();
      ctx.moveTo(-3 * s, 4.5 * s);
      ctx.quadraticCurveTo(0, 6 * s, 3 * s, 4.5 * s);
      ctx.stroke();
    }

    ctx.restore();
    ctx.restore();
  },
};

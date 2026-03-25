import type { EntityDrawer, NPCAction } from '../types';

const SKIN = 0x9d7d5c;
const SKIN_DARK = 0x7e623b;
const SKIN_LIGHT = 0xb1976f;
const TORN_CLOTH = 0x7a6a50;
const ROPE = 0x5a4a30;

export const RescueNPCDrawer: EntityDrawer = {
  key: 'npc_rescue',
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
    let armShake = 0;

    switch (act) {
      case 'idle':
        bob = Math.sin(phase) * 0.8 * s;
        bodyLean = Math.sin(phase * 0.5) * 0.5 * s;
        headTilt = Math.sin(phase) * 0.02;
        break;
      case 'alert':
        armShake = Math.sin(phase * 4) * 2 * s;
        headTilt = -0.03;
        break;
      case 'talking':
        bob = Math.sin(phase) * 1 * s;
        headTilt = Math.sin(phase * 1.5) * 0.025;
        armShake = Math.sin(phase * 3) * 1.5 * s;
        break;
      default:
        bob = Math.sin(phase) * 0.5 * s;
        break;
    }

    ctx.save();
    const by = ground + bob;

    // Shadow
    ctx.fillStyle = 'rgba(0,0,0,0.2)';
    utils.fillEllipse(ctx, cx, ground + 2 * s, 16 * s, 3.5 * s);

    // Bare feet
    for (const side of [-1, 1]) {
      ctx.fillStyle = utils.rgb(SKIN_DARK);
      utils.fillEllipse(ctx, cx + side * 5 * s, by - 2 * s, 4 * s, 2 * s);
    }

    // Legs (torn cloth)
    utils.drawPart(ctx, cx - 9 * s, by - 22 * s, 7 * s, 18 * s, TORN_CLOTH, 2 * s);
    utils.drawPart(ctx, cx + 2 * s, by - 22 * s, 7 * s, 18 * s, TORN_CLOTH, 2 * s);

    utils.zoneNpcOutline(ctx, w, h);

    // Torn tunic body
    ctx.fillStyle = utils.rgb(TORN_CLOTH);
    utils.roundRect(ctx, cx - 11 * s + bodyLean, by - 44 * s, 22 * s, 24 * s, 4 * s);
    ctx.fill();

    // Tears/rips on clothing
    ctx.strokeStyle = utils.rgb(utils.darken(TORN_CLOTH, 20), 0.7);
    ctx.lineWidth = 0.8 * s;
    ctx.beginPath();
    ctx.moveTo(cx - 5 * s, by - 38 * s); ctx.lineTo(cx - 3 * s, by - 30 * s);
    ctx.stroke();
    ctx.beginPath();
    ctx.moveTo(cx + 4 * s, by - 35 * s); ctx.lineTo(cx + 6 * s, by - 28 * s);
    ctx.stroke();

    // Rope bindings around wrists
    ctx.strokeStyle = utils.rgb(ROPE);
    ctx.lineWidth = 2 * s;
    ctx.beginPath();
    ctx.moveTo(cx - 14 * s + armShake, by - 30 * s);
    ctx.lineTo(cx + 14 * s - armShake, by - 30 * s);
    ctx.stroke();

    utils.softOutlineEnd(ctx);

    // Arms (bound together)
    const armY = by - 32 * s;
    utils.drawLimb(ctx, [
      { x: cx - 13 * s + bodyLean, y: by - 42 * s },
      { x: cx - 14 * s + armShake, y: armY },
    ], 4 * s, SKIN);
    utils.drawLimb(ctx, [
      { x: cx + 13 * s + bodyLean, y: by - 42 * s },
      { x: cx + 14 * s - armShake, y: armY },
    ], 4 * s, SKIN);

    // Hands
    ctx.fillStyle = utils.rgb(SKIN);
    utils.fillCircle(ctx, cx - 14 * s + armShake, armY, 3 * s);
    utils.fillCircle(ctx, cx + 14 * s - armShake, armY, 3 * s);

    // Neck
    ctx.fillStyle = utils.rgb(SKIN);
    utils.roundRect(ctx, cx - 3 * s, by - 50 * s, 6 * s, 7 * s, 2 * s);
    ctx.fill();

    // Head
    const headX = cx + bodyLean * 0.3;
    const headY = by - 58 * s;
    ctx.save();
    ctx.translate(headX, headY);
    ctx.rotate(headTilt);

    // Messy hair
    ctx.fillStyle = utils.rgb(0x4a3a2a);
    for (let i = -3; i <= 3; i++) {
      ctx.fillRect(i * 2.5 * s - 1 * s, -11 * s + Math.abs(i) * s, 2 * s, 5 * s);
    }

    // Face (stressed)
    const headGrad = ctx.createRadialGradient(-2 * s, -1 * s, 0, 0, 0, 10 * s);
    headGrad.addColorStop(0, utils.rgb(SKIN_LIGHT));
    headGrad.addColorStop(0.5, utils.rgb(SKIN));
    headGrad.addColorStop(1, utils.rgb(SKIN_DARK));
    ctx.fillStyle = headGrad;
    utils.roundRect(ctx, -9 * s, -8 * s, 18 * s, 16 * s, 4 * s);
    ctx.fill();

    // Worried eyes (wide)
    ctx.fillStyle = '#e8e4e0';
    utils.fillEllipse(ctx, -4 * s, -1 * s, 2.8 * s, 3 * s);
    utils.fillEllipse(ctx, 4 * s, -1 * s, 2.8 * s, 3 * s);
    ctx.fillStyle = '#3a2818';
    utils.fillCircle(ctx, -4 * s, 0, 1.5 * s);
    utils.fillCircle(ctx, 4 * s, 0, 1.5 * s);

    // Worried eyebrows
    ctx.strokeStyle = utils.rgb(0x4a3a2a);
    ctx.lineWidth = 1.5 * s;
    ctx.beginPath();
    ctx.moveTo(-6 * s, -5 * s); ctx.lineTo(-3 * s, -3.5 * s);
    ctx.stroke();
    ctx.beginPath();
    ctx.moveTo(3 * s, -3.5 * s); ctx.lineTo(6 * s, -5 * s);
    ctx.stroke();

    // Distressed mouth
    ctx.strokeStyle = utils.rgb(SKIN_DARK, 0.7);
    ctx.lineWidth = 1 * s;
    ctx.beginPath();
    ctx.moveTo(-3 * s, 5 * s);
    ctx.quadraticCurveTo(0, 3 * s, 3 * s, 5 * s);
    ctx.stroke();

    // Help exclamation (!)
    ctx.fillStyle = 'rgba(255,100,50,0.8)';
    ctx.fillRect(-1 * s, -18 * s, 2 * s, 6 * s);
    utils.fillCircle(ctx, 0, -10 * s, 1 * s);

    ctx.restore();
    ctx.restore();
  },
};

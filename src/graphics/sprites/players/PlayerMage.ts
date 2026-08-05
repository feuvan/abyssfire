// src/graphics/sprites/players/PlayerMage.ts
import type { EntityDrawer, PlayerAction } from '../types';
import {
  PLAYER_ACTION_FRAME_COUNTS,
  PLAYER_TOTAL_FRAMES,
} from '../types';
import type { DrawUtils } from '../../DrawUtils';
import { samplePose, sinePulse, smoothstep } from './PlayerMotion';

const ROBE_BASE    = 0x1e132a;
const ROBE_LIGHT   = 0x2d1e3d;
const ROBE_DARK    = 0x0f071b;
const SKIN         = 0x8d795e;
const SKIN_DARK    = 0x735c45;
const STAFF_WOOD   = 0x4c3417;
const CRYSTAL      = 0x8a5ac0;
const CRYSTAL_GLOW = 0xc09ae0;
const ARCANE       = 0x8a5ac0;
const HAT_COLOR    = 0x2d1e3d;
const HAT_TRIM     = 0x8a5ac0;

export const PlayerMageDrawer: EntityDrawer = {
  key: 'player_mage',
  frameW: 64,
  frameH: 96,
  totalFrames: PLAYER_TOTAL_FRAMES,

  drawFrame(ctx, frame, action, w, h, utils) {
    const act = action as PlayerAction;
    const s = w / 64;

    const count = PLAYER_ACTION_FRAME_COUNTS[act];
    const localFrame = frame % count;
    const t = count > 1 ? localFrame / (count - 1) : 0;
    const phase = (localFrame / count) * Math.PI * 2;

    let alpha = 1;
    let bodyOffsetY = 0;
    let globalRotation = 0;
    let staffAngle = -Math.PI * 0.1;
    let castIntensity = 0;
    let wispPhase = 0;
    let leanX = 0;
    let crouch = 0;
    let robeFlow = 0;
    let motionEnergy = 0;

    switch (act) {
      case 'idle':
        // Weightless breathing, robe and focus crystal move out of phase.
        bodyOffsetY = Math.sin(phase) * 1.15 * s;
        staffAngle = -Math.PI * 0.1 + Math.sin(phase - 0.7) * 0.045;
        leanX = Math.sin(phase * 0.5) * 0.45;
        robeFlow = Math.sin(phase - 0.9) * 0.35;
        castIntensity = 0.08 + (Math.sin(phase) + 1) * 0.04;
        wispPhase = phase * 0.5;
        break;
      case 'walk':
        bodyOffsetY = -Math.abs(Math.sin(phase)) * 1.45 * s;
        staffAngle = -Math.PI * 0.1 + Math.sin(phase) * 0.105;
        leanX = Math.sin(phase) * 1.1;
        robeFlow = Math.sin(phase - 0.7);
        globalRotation = Math.sin(phase) * 0.012;
        break;
      case 'attack':
        // Compact staff jab followed by an arcane pulse.
        staffAngle = samplePose(t, [
          { at: 0, value: -0.1 * Math.PI },
          { at: 0.24, value: -0.24 * Math.PI },
          { at: 0.54, value: 0.3 * Math.PI },
          { at: 0.75, value: 0.22 * Math.PI },
          { at: 1, value: -0.1 * Math.PI },
        ]);
        leanX = samplePose(t, [
          { at: 0, value: 0 },
          { at: 0.24, value: -1.5 },
          { at: 0.56, value: 4.5 },
          { at: 1, value: 0 },
        ]);
        bodyOffsetY = -sinePulse(t) * 2.2 * s;
        castIntensity = samplePose(t, [
          { at: 0, value: 0.08 },
          { at: 0.38, value: 0.25 },
          { at: 0.58, value: 0.92 },
          { at: 1, value: 0.05 },
        ]);
        wispPhase = t * Math.PI * 2.4;
        robeFlow = -sinePulse(t) * 0.7;
        motionEnergy = castIntensity * 0.7;
        break;
      case 'hurt':
        crouch = sinePulse(t);
        bodyOffsetY = crouch * 5 * s;
        leanX = -crouch * 3.2;
        staffAngle = -Math.PI * 0.1 - crouch * 0.52;
        globalRotation = -crouch * 0.055;
        robeFlow = crouch;
        alpha = 0.68 + smoothstep(t) * 0.32;
        break;
      case 'dodge':
        // Astral slip: collapse into a narrow silhouette and phase forward.
        crouch = sinePulse(t);
        leanX = samplePose(t, [
          { at: 0, value: -0.5 },
          { at: 0.38, value: 6.5 },
          { at: 0.7, value: 4 },
          { at: 1, value: 0 },
        ]);
        bodyOffsetY = crouch * 3.5 * s;
        staffAngle = -Math.PI * 0.1 + crouch * 0.75;
        robeFlow = -crouch * 1.3;
        castIntensity = crouch;
        wispPhase = phase * 1.5;
        motionEnergy = crouch;
        alpha = 1 - crouch * 0.28;
        break;
      case 'death':
        globalRotation = smoothstep(t) * Math.PI * 0.45;
        bodyOffsetY = smoothstep(t) * h * 0.4;
        staffAngle = -Math.PI * 0.1 - smoothstep(t) * 0.65;
        robeFlow = smoothstep(t);
        castIntensity = Math.max(0, 1 - t * 1.8) * 0.5;
        alpha = 1 - smoothstep(t) * 0.86;
        break;
      case 'cast':
        // Gather, suspend, and release with a clear anticipation beat.
        staffAngle = samplePose(t, [
          { at: 0, value: -0.1 * Math.PI },
          { at: 0.38, value: -0.58 * Math.PI },
          { at: 0.66, value: -0.52 * Math.PI },
          { at: 0.8, value: 0.12 * Math.PI },
          { at: 1, value: -0.1 * Math.PI },
        ]);
        castIntensity = samplePose(t, [
          { at: 0, value: 0.05 },
          { at: 0.42, value: 0.72 },
          { at: 0.68, value: 1 },
          { at: 0.82, value: 0.86 },
          { at: 1, value: 0.08 },
        ]);
        wispPhase = t * Math.PI * 3.5;
        bodyOffsetY = -sinePulse(t) * 3 * s;
        leanX = samplePose(t, [
          { at: 0, value: 0 },
          { at: 0.45, value: -1.5 },
          { at: 0.8, value: 3 },
          { at: 1, value: 0 },
        ]);
        robeFlow = Math.sin(t * Math.PI * 2) * castIntensity;
        motionEnergy = castIntensity;
        break;
    }

    ctx.save();
    ctx.globalAlpha = alpha;

    const cx = w / 2;
    const baseY = h * 0.95;

    ctx.translate(cx, baseY + bodyOffsetY);
    ctx.rotate(globalRotation);
    ctx.translate(-cx, -(baseY + bodyOffsetY));

    // Shadow
    ctx.fillStyle = 'rgba(0,0,0,0.23)';
    utils.fillEllipse(
      ctx,
      cx - leanX * 0.25 * s,
      baseY + 1 * s,
      (13 + crouch * 3) * s,
      Math.max(1.5, 2.5 - crouch * 0.7) * s,
    );

    if (motionEnergy > 0.15) {
      const auraX = cx + leanX * s;
      const auraY = baseY - 49 * s + bodyOffsetY;
      const aura = ctx.createRadialGradient(auraX, auraY, 0, auraX, auraY, 23 * s);
      aura.addColorStop(0, utils.rgb(CRYSTAL_GLOW, motionEnergy * 0.18));
      aura.addColorStop(0.58, utils.rgb(CRYSTAL, motionEnergy * 0.08));
      aura.addColorStop(1, utils.rgb(CRYSTAL, 0));
      ctx.fillStyle = aura;
      utils.fillCircle(ctx, auraX, auraY, 23 * s);
    }

    // ── Robe (lower — wide trapezoid with wavy hem) ────────────────────────
    const torsoX = cx + leanX * s;
    const torsoY = baseY - 50 * s + bodyOffsetY + crouch * 1.8 * s;
    const robeTopW = 14 * s;
    const robeBotW = 22 * s;
    const robeTop = torsoY + 2 * s;
    const robeBot = baseY - 4 * s;

    // Soft outline glow
    utils.zonePlayerOutline(ctx, w, h);

    // Main robe fill
    const robeGrad = ctx.createLinearGradient(torsoX - robeBotW / 2, robeTop, torsoX + robeBotW / 2, robeBot);
    robeGrad.addColorStop(0, utils.rgb(ROBE_LIGHT));
    robeGrad.addColorStop(0.5, utils.rgb(ROBE_BASE));
    robeGrad.addColorStop(1, utils.rgb(ROBE_DARK));
    ctx.fillStyle = robeGrad;
    ctx.beginPath();
    ctx.moveTo(torsoX - robeTopW / 2, robeTop);
    ctx.lineTo(torsoX + robeTopW / 2, robeTop);
    ctx.lineTo(torsoX + robeBotW / 2, robeBot);
    // Wavy hem (3 gentle waves)
    const waveY = robeBot;
    const waveAmp = 2.5 * s * (
      1
      + (act === 'walk' ? Math.abs(Math.sin(phase)) * 0.75 : 0)
      + Math.abs(robeFlow) * 0.45
    );
    ctx.quadraticCurveTo(torsoX + robeBotW / 2 - 4 * s, waveY - waveAmp, torsoX + 2 * s, waveY);
    ctx.quadraticCurveTo(torsoX - 4 * s, waveY + waveAmp, torsoX - robeBotW / 2, waveY);
    ctx.closePath();
    ctx.fill();

    // Robe trim / hem accent
    ctx.strokeStyle = utils.rgb(ROBE_LIGHT, 0.6);
    ctx.lineWidth = 1 * s;
    ctx.beginPath();
    ctx.moveTo(torsoX - robeBotW / 2, waveY);
    ctx.quadraticCurveTo(torsoX - 4 * s, waveY + waveAmp, torsoX, waveY);
    ctx.quadraticCurveTo(torsoX + 4 * s, waveY - waveAmp * 0.5, torsoX + robeBotW / 2, waveY);
    ctx.stroke();

    // Cloth shoes — flat ellipses visible below robe
    for (const side of [-1, 1]) {
      const shoePhase = act === 'walk'
        ? phase + (side === -1 ? 0 : Math.PI)
        : 0;
      ctx.fillStyle = utils.rgb(ROBE_DARK);
      utils.fillEllipse(
        ctx,
        torsoX + side * 5 * s + Math.sin(shoePhase) * 2 * s,
        baseY - 2 * s - Math.max(0, Math.sin(shoePhase)) * 1.3 * s,
        5 * s,
        3 * s,
      );
    }

    // ── Upper Torso / Chest ───────────────────────────────────────────────
    // Robe body
    ctx.fillStyle = utils.rgb(ROBE_BASE);
    utils.fillEllipse(ctx, torsoX, torsoY, 9 * s, 12 * s);
    // Chest overlay — lighter center panel
    const chestGrad = ctx.createLinearGradient(torsoX - 5 * s, torsoY - 10 * s, torsoX + 5 * s, torsoY + 5 * s);
    chestGrad.addColorStop(0, utils.rgb(ROBE_LIGHT, 0.7));
    chestGrad.addColorStop(1, utils.rgb(ROBE_DARK, 0.7));
    ctx.fillStyle = chestGrad;
    ctx.fillRect(torsoX - 5 * s, torsoY - 8 * s, 10 * s, 14 * s);
    // Arcane symbol hint on chest
    ctx.strokeStyle = utils.rgb(ARCANE, 0.3);
    ctx.lineWidth = 0.8 * s;
    ctx.beginPath();
    ctx.arc(torsoX, torsoY - 3 * s, 3.5 * s, 0, Math.PI * 2);
    ctx.stroke();

    // End soft outline
    utils.softOutlineEnd(ctx);

    // Rim light on chest
    utils.zonePlayerRimLight(ctx, torsoX, torsoY, 9 * s, 12 * s);

    // ── Staff ─────────────────────────────────────────────────────────────
    const staffPivotX = torsoX + 10 * s;
    const staffPivotY = torsoY - 5 * s;
    const staffLen = 40 * s;

    ctx.save();
    ctx.translate(staffPivotX, staffPivotY);
    ctx.rotate(staffAngle);

    // Gnarled staff shaft
    ctx.strokeStyle = utils.rgb(STAFF_WOOD);
    ctx.lineWidth = 2.5 * s;
    ctx.lineCap = 'round';
    ctx.beginPath();
    ctx.moveTo(0, 0);
    ctx.bezierCurveTo(-2 * s, -staffLen * 0.3, 2 * s, -staffLen * 0.7, 0, -staffLen);
    ctx.stroke();
    // Knot on shaft
    ctx.fillStyle = utils.rgb(utils.darken(STAFF_WOOD, 20));
    utils.fillCircle(ctx, -1 * s, -staffLen * 0.55, 2 * s);

    // Crystal tip — radial gradient sphere
    const crystalY = -staffLen - 4 * s;
    const crystalR = 5.5 * s;
    const crystalGrad = ctx.createRadialGradient(-2 * s, crystalY - 2 * s, 0.5 * s, 0, crystalY, crystalR);
    crystalGrad.addColorStop(0, '#ffffff');
    crystalGrad.addColorStop(0.3, utils.rgb(CRYSTAL_GLOW));
    crystalGrad.addColorStop(1, utils.rgb(CRYSTAL));
    ctx.fillStyle = crystalGrad;
    utils.fillCircle(ctx, 0, crystalY, crystalR);

    // Crystal outer glow, intensity driven by cast/attack
    if (castIntensity > 0) {
      const glowR = crystalR * (1.8 + castIntensity);
      const glowGrad = ctx.createRadialGradient(0, crystalY, 0, 0, crystalY, glowR);
      glowGrad.addColorStop(0, utils.rgb(CRYSTAL_GLOW, 0.6 * castIntensity));
      glowGrad.addColorStop(1, utils.rgb(CRYSTAL, 0));
      ctx.fillStyle = glowGrad;
      utils.fillCircle(ctx, 0, crystalY, glowR);
    }

    ctx.restore(); // staff transform

    // ── Arms ──────────────────────────────────────────────────────────────
    for (const side of [-1, 1]) {
      const isRight = side === 1;
      const armPhase = act === 'walk' ? phase + (isRight ? Math.PI : 0) : 0;
      const shoulderX = torsoX + side * 9 * s;
      const shoulderY = torsoY - 9 * s;

      let elbowX: number, elbowY: number, handX: number, handY: number;

      if (isRight && (act === 'attack' || act === 'cast' || act === 'dodge')) {
        // Arm raised to hold staff aloft
        const castT = act === 'cast' ? castIntensity * 0.8
          : act === 'dodge' ? crouch * 0.65
            : t * 0.8;
        elbowX = shoulderX + 3 * s;
        elbowY = shoulderY - castT * 8 * s;
        handX = elbowX + 4 * s;
        handY = elbowY + 3 * s - castT * 5 * s;
      } else {
        elbowX = shoulderX + side * 3 * s + Math.sin(armPhase) * 2 * s;
        elbowY = shoulderY + 8 * s;
        handX = elbowX + side * 2 * s + Math.sin(armPhase) * 1.5 * s;
        handY = elbowY + 8 * s + Math.sin(armPhase) * 2 * s;
      }

      // Sleeve
      utils.drawLimb(ctx, [
        { x: shoulderX, y: shoulderY },
        { x: elbowX, y: elbowY },
        { x: handX, y: handY },
      ], 4.5 * s, ROBE_BASE);
      // Sleeve cuff (lighter at wrist)
      ctx.fillStyle = utils.rgb(ROBE_LIGHT, 0.7);
      utils.fillEllipse(ctx, handX, handY, 3.5 * s, 2.5 * s);

      // Hand / skin
      ctx.fillStyle = utils.rgb(SKIN);
      utils.fillEllipse(ctx, handX, handY + 1 * s, 2.8 * s, 2 * s);

      // ── Cast wisps around hands ──────────────────────────────────────────
      if (castIntensity > 0.3) {
        const wispCount = 4;
        for (let i = 0; i < wispCount; i++) {
          const wAngle = wispPhase + (i / wispCount) * Math.PI * 2;
          const wDist = 5 * s * castIntensity;
          const wx = handX + Math.cos(wAngle) * wDist;
          const wy = handY + Math.sin(wAngle) * wDist;
          // Glow circle
          const wGlow = ctx.createRadialGradient(wx, wy, 0, wx, wy, 3 * s);
          wGlow.addColorStop(0, utils.rgb(CRYSTAL_GLOW, 0.8 * castIntensity));
          wGlow.addColorStop(1, utils.rgb(ARCANE, 0));
          ctx.fillStyle = wGlow;
          utils.fillCircle(ctx, wx, wy, 3 * s);
          // Core
          ctx.fillStyle = 'rgba(255,255,255,0.7)';
          utils.fillCircle(ctx, wx, wy, 0.8 * s);
        }
      }
    }

    // ── Head ──────────────────────────────────────────────────────────────
    const headX = torsoX;
    const headY = torsoY - 20 * s;

    // Neck
    ctx.fillStyle = utils.rgb(SKIN_DARK);
    ctx.fillRect(headX - 2.5 * s, torsoY - 14 * s, 5 * s, 6 * s);

    // Face — lean oval
    const faceGrad = ctx.createRadialGradient(headX - 2 * s, headY - 2 * s, 0, headX, headY, 8 * s);
    faceGrad.addColorStop(0, utils.rgb(utils.lighten(SKIN, 20)));
    faceGrad.addColorStop(0.6, utils.rgb(SKIN));
    faceGrad.addColorStop(1, utils.rgb(SKIN_DARK));
    ctx.fillStyle = faceGrad;
    utils.fillEllipse(ctx, headX, headY, 8 * s, 9 * s);

    // Eyes — slightly large and wise
    for (const side of [-1, 1]) {
      const ex = headX + side * 3 * s;
      const ey = headY - 1 * s;
      ctx.fillStyle = '#1a0a0a';
      utils.fillEllipse(ctx, ex, ey, 2 * s, 2 * s);
      ctx.fillStyle = '#4a2a7a';
      utils.fillCircle(ctx, ex, ey, 1.2 * s);
      ctx.fillStyle = 'rgba(255,220,180,0.5)';
      utils.fillCircle(ctx, ex - 0.5 * s, ey - 0.5 * s, 0.5 * s);
    }

    // Mouth — slight smile
    ctx.strokeStyle = utils.rgb(SKIN_DARK, 0.8);
    ctx.lineWidth = 0.8 * s;
    ctx.beginPath();
    ctx.moveTo(headX - 2.5 * s, headY + 3.5 * s);
    ctx.quadraticCurveTo(headX, headY + 5 * s, headX + 2.5 * s, headY + 3.5 * s);
    ctx.stroke();

    // ── Pointed Wizard Hat ────────────────────────────────────────────────
    const hatBaseX = headX;
    const hatBaseY = headY - 6 * s;
    const hatTipX = hatBaseX + 2 * s;
    const hatTipY = hatBaseY - 22 * s;

    // Hat body (tall triangle, slight lean)
    ctx.fillStyle = utils.rgb(HAT_COLOR);
    ctx.beginPath();
    ctx.moveTo(hatBaseX - 12 * s, hatBaseY);
    ctx.lineTo(hatBaseX + 12 * s, hatBaseY);
    ctx.lineTo(hatTipX + 1 * s, hatTipY);
    ctx.closePath();
    ctx.fill();
    // Hat shading
    const hatGrad = ctx.createLinearGradient(hatBaseX - 12 * s, hatBaseY, hatTipX, hatTipY);
    hatGrad.addColorStop(0, utils.rgb(ROBE_DARK, 0.4));
    hatGrad.addColorStop(0.5, 'rgba(0,0,0,0)');
    hatGrad.addColorStop(1, utils.rgb(ROBE_LIGHT, 0.3));
    ctx.fillStyle = hatGrad;
    ctx.beginPath();
    ctx.moveTo(hatBaseX - 12 * s, hatBaseY);
    ctx.lineTo(hatBaseX + 12 * s, hatBaseY);
    ctx.lineTo(hatTipX + 1 * s, hatTipY);
    ctx.closePath();
    ctx.fill();

    // Hat brim
    ctx.fillStyle = utils.rgb(utils.darken(HAT_COLOR, 20));
    utils.fillEllipse(ctx, hatBaseX, hatBaseY, 13 * s, 4 * s);
    ctx.fillStyle = utils.rgb(HAT_TRIM, 0.5);
    ctx.beginPath();
    ctx.ellipse(hatBaseX, hatBaseY, 13 * s, 4 * s, 0, 0, Math.PI * 2);
    ctx.lineWidth = 1.2 * s;
    ctx.strokeStyle = utils.rgb(HAT_TRIM, 0.7);
    ctx.stroke();

    // Star motif near tip (circle with 5-point star hint)
    const starX = hatTipX - 1 * s;
    const starY = hatTipY + 8 * s;
    const starGrad = ctx.createRadialGradient(starX, starY, 0, starX, starY, 4 * s);
    starGrad.addColorStop(0, utils.rgb(CRYSTAL_GLOW, 0.9));
    starGrad.addColorStop(1, utils.rgb(HAT_TRIM, 0));
    ctx.fillStyle = starGrad;
    utils.fillCircle(ctx, starX, starY, 4 * s);
    // Star points
    ctx.fillStyle = utils.rgb(CRYSTAL_GLOW, 0.7);
    for (let i = 0; i < 5; i++) {
      const ang = (i / 5) * Math.PI * 2 - Math.PI * 0.5;
      const px = starX + Math.cos(ang) * 3 * s;
      const py = starY + Math.sin(ang) * 3 * s;
      utils.fillCircle(ctx, px, py, 0.7 * s);
    }

    ctx.restore();
  },
};

import type { EntityDrawer, NPCAction } from '../types';

type EventNPCRole =
  | 'lost_traveler'
  | 'wounded_hunter'
  | 'trapped_miner'
  | 'caravan_guard'
  | 'abyss_explorer'
  | 'traveling_merchant'
  | 'wounded_explorer'
  | 'mercenary_tank'
  | 'mercenary_melee'
  | 'mercenary_ranged'
  | 'mercenary_healer'
  | 'mercenary_mage';

interface EventNPCStyle {
  key: string;
  role: EventNPCRole;
  skin: number;
  hair: number;
  tunic: number;
  trousers: number;
  accent: number;
  leather: number;
}

const FRAME_COUNTS: Record<NPCAction, number> = {
  working: 8,
  alert: 4,
  idle: 6,
  talking: 6,
};

function drawBackEquipment(
  ctx: CanvasRenderingContext2D,
  style: EventNPCStyle,
  cx: number,
  by: number,
  bodyLean: number,
  s: number,
  utils: Parameters<EntityDrawer['drawFrame']>[5],
): void {
  switch (style.role) {
    case 'lost_traveler':
    case 'traveling_merchant': {
      ctx.fillStyle = utils.rgb(style.leather);
      utils.roundRect(ctx, cx - 18 * s + bodyLean, by - 49 * s, 13 * s, 28 * s, 4 * s);
      ctx.fill();
      ctx.strokeStyle = utils.rgb(utils.lighten(style.leather, 24), 0.7);
      ctx.lineWidth = 1.2 * s;
      ctx.strokeRect(cx - 15 * s + bodyLean, by - 44 * s, 7 * s, 15 * s);
      break;
    }
    case 'wounded_hunter': {
      ctx.strokeStyle = utils.rgb(style.leather);
      ctx.lineWidth = 2.2 * s;
      ctx.beginPath();
      ctx.arc(cx + bodyLean, by - 37 * s, 19 * s, -1.15, 1.15);
      ctx.stroke();
      ctx.strokeStyle = utils.rgb(0xc7b07a, 0.8);
      ctx.lineWidth = 0.8 * s;
      ctx.beginPath();
      ctx.moveTo(cx + 8 * s + bodyLean, by - 54 * s);
      ctx.lineTo(cx + 8 * s + bodyLean, by - 20 * s);
      ctx.stroke();
      break;
    }
    case 'trapped_miner': {
      ctx.strokeStyle = utils.rgb(utils.darken(style.leather, 18));
      ctx.lineWidth = 3 * s;
      ctx.beginPath();
      ctx.moveTo(cx - 16 * s + bodyLean, by - 56 * s);
      ctx.lineTo(cx + 15 * s + bodyLean, by - 12 * s);
      ctx.stroke();
      ctx.strokeStyle = utils.rgb(0x9ca3a8);
      ctx.lineWidth = 2.2 * s;
      ctx.beginPath();
      ctx.moveTo(cx - 20 * s + bodyLean, by - 60 * s);
      ctx.lineTo(cx - 11 * s + bodyLean, by - 54 * s);
      ctx.stroke();
      break;
    }
    case 'caravan_guard': {
      ctx.fillStyle = utils.rgb(style.accent);
      utils.fillEllipse(ctx, cx - 13 * s + bodyLean, by - 37 * s, 10 * s, 18 * s);
      ctx.strokeStyle = utils.rgb(0xd6b75d, 0.75);
      ctx.lineWidth = 1.2 * s;
      ctx.beginPath();
      ctx.arc(cx - 13 * s + bodyLean, by - 37 * s, 7 * s, 0, Math.PI * 2);
      ctx.stroke();
      break;
    }
    case 'abyss_explorer': {
      ctx.fillStyle = utils.rgb(0x231b35);
      utils.roundRect(ctx, cx - 17 * s + bodyLean, by - 50 * s, 11 * s, 25 * s, 3 * s);
      ctx.fill();
      ctx.fillStyle = utils.rgb(style.accent, 0.8);
      utils.fillCircle(ctx, cx - 12 * s + bodyLean, by - 43 * s, 2 * s);
      utils.fillCircle(ctx, cx - 12 * s + bodyLean, by - 34 * s, 2 * s);
      break;
    }
    case 'wounded_explorer': {
      ctx.strokeStyle = utils.rgb(style.leather);
      ctx.lineWidth = 2.5 * s;
      ctx.beginPath();
      ctx.moveTo(cx + 17 * s + bodyLean, by - 61 * s);
      ctx.lineTo(cx + 14 * s + bodyLean, by - 1 * s);
      ctx.stroke();
      break;
    }
    default: {
      ctx.fillStyle = utils.rgb(utils.darken(style.tunic, 20));
      utils.roundRect(ctx, cx - 16 * s + bodyLean, by - 48 * s, 7 * s, 25 * s, 2 * s);
      ctx.fill();
      break;
    }
  }
}

function drawHeadwear(
  ctx: CanvasRenderingContext2D,
  style: EventNPCStyle,
  s: number,
  utils: Parameters<EntityDrawer['drawFrame']>[5],
): void {
  switch (style.role) {
    case 'lost_traveler':
      ctx.fillStyle = utils.rgb(style.accent);
      ctx.beginPath();
      ctx.moveTo(-11 * s, -8 * s);
      ctx.quadraticCurveTo(0, -17 * s, 11 * s, -8 * s);
      ctx.lineTo(8 * s, -4 * s);
      ctx.lineTo(-8 * s, -4 * s);
      ctx.closePath();
      ctx.fill();
      break;
    case 'wounded_hunter':
      ctx.fillStyle = utils.rgb(style.accent);
      ctx.beginPath();
      ctx.moveTo(-12 * s, -7 * s);
      ctx.quadraticCurveTo(0, -16 * s, 12 * s, -7 * s);
      ctx.lineTo(9 * s, -2 * s);
      ctx.lineTo(-9 * s, -2 * s);
      ctx.closePath();
      ctx.fill();
      ctx.fillStyle = utils.rgb(0xbab091);
      ctx.fillRect(-10 * s, -2 * s, 20 * s, 3 * s);
      break;
    case 'trapped_miner':
      ctx.fillStyle = utils.rgb(style.accent);
      ctx.beginPath();
      ctx.arc(0, -7 * s, 11 * s, Math.PI, Math.PI * 2);
      ctx.lineTo(12 * s, -5 * s);
      ctx.lineTo(-12 * s, -5 * s);
      ctx.closePath();
      ctx.fill();
      ctx.fillStyle = utils.rgb(0xf1c85b);
      utils.fillCircle(ctx, 0, -10 * s, 3.2 * s);
      ctx.fillStyle = '#fff1a6';
      utils.fillCircle(ctx, -0.8 * s, -11 * s, 1.2 * s);
      break;
    case 'caravan_guard':
      ctx.fillStyle = utils.rgb(style.accent);
      ctx.beginPath();
      ctx.arc(0, -7 * s, 11 * s, Math.PI, Math.PI * 2);
      ctx.lineTo(10 * s, -2 * s);
      ctx.lineTo(-10 * s, -2 * s);
      ctx.closePath();
      ctx.fill();
      ctx.fillStyle = utils.rgb(utils.lighten(style.accent, 28));
      ctx.fillRect(-11 * s, -5 * s, 22 * s, 4 * s);
      ctx.fillRect(7 * s, -3 * s, 4 * s, 13 * s);
      break;
    case 'abyss_explorer':
      ctx.fillStyle = utils.rgb(0x241933);
      ctx.beginPath();
      ctx.moveTo(-12 * s, -5 * s);
      ctx.quadraticCurveTo(0, -18 * s, 12 * s, -5 * s);
      ctx.lineTo(9 * s, 4 * s);
      ctx.lineTo(-9 * s, 4 * s);
      ctx.closePath();
      ctx.fill();
      ctx.strokeStyle = utils.rgb(style.accent, 0.85);
      ctx.lineWidth = 2 * s;
      ctx.beginPath();
      ctx.arc(-4 * s, -1 * s, 3 * s, 0, Math.PI * 2);
      ctx.arc(4 * s, -1 * s, 3 * s, 0, Math.PI * 2);
      ctx.stroke();
      ctx.beginPath();
      ctx.moveTo(-1 * s, -1 * s);
      ctx.lineTo(1 * s, -1 * s);
      ctx.stroke();
      break;
    case 'traveling_merchant':
      ctx.fillStyle = utils.rgb(style.accent);
      utils.roundRect(ctx, -11 * s, -13 * s, 22 * s, 8 * s, 3 * s);
      ctx.fill();
      ctx.fillRect(-14 * s, -6 * s, 28 * s, 3 * s);
      ctx.fillStyle = utils.rgb(0xd6ad42);
      utils.fillCircle(ctx, -6 * s, -9 * s, 1.5 * s);
      break;
    case 'wounded_explorer':
      ctx.fillStyle = utils.rgb(style.accent);
      ctx.fillRect(-11 * s, -7 * s, 22 * s, 5 * s);
      ctx.fillRect(6 * s, -4 * s, 5 * s, 12 * s);
      break;
    default:
      ctx.fillStyle = utils.rgb(style.accent);
      ctx.fillRect(-11 * s, -8 * s, 22 * s, 5 * s);
      ctx.fillStyle = utils.rgb(utils.lighten(style.accent, 24));
      utils.fillCircle(ctx, 0, -5 * s, 2 * s);
      break;
  }
}

function drawRoleDetails(
  ctx: CanvasRenderingContext2D,
  style: EventNPCStyle,
  cx: number,
  by: number,
  bodyLean: number,
  handY: number,
  phase: number,
  s: number,
  utils: Parameters<EntityDrawer['drawFrame']>[5],
): void {
  if (style.role === 'lost_traveler') {
    const mapY = handY - 2 * s;
    ctx.fillStyle = utils.rgb(0xd6c69a);
    ctx.fillRect(cx - 9 * s, mapY - 6 * s, 18 * s, 11 * s);
    ctx.strokeStyle = utils.rgb(0x6b825b);
    ctx.lineWidth = 0.8 * s;
    ctx.beginPath();
    ctx.moveTo(cx - 6 * s, mapY + 1 * s);
    ctx.lineTo(cx - 1 * s, mapY - 3 * s);
    ctx.lineTo(cx + 5 * s, mapY + 2 * s);
    ctx.stroke();
  } else if (style.role === 'wounded_hunter' || style.role === 'wounded_explorer') {
    ctx.fillStyle = utils.rgb(0xd8d0b8);
    ctx.fillRect(cx - 16 * s + bodyLean, by - 39 * s, 7 * s, 10 * s);
    ctx.strokeStyle = utils.rgb(0xa84b3f, 0.65);
    ctx.lineWidth = 1 * s;
    ctx.beginPath();
    ctx.moveTo(cx - 13 * s + bodyLean, by - 37 * s);
    ctx.lineTo(cx - 11 * s + bodyLean, by - 32 * s);
    ctx.stroke();
  } else if (style.role === 'caravan_guard') {
    ctx.strokeStyle = utils.rgb(0x76512d);
    ctx.lineWidth = 2.5 * s;
    ctx.beginPath();
    ctx.moveTo(cx + 18 * s, by - 67 * s);
    ctx.lineTo(cx + 18 * s, by - 1 * s);
    ctx.stroke();
    ctx.fillStyle = utils.rgb(0xc7a34e);
    ctx.beginPath();
    ctx.moveTo(cx + 18 * s, by - 72 * s);
    ctx.lineTo(cx + 14 * s, by - 64 * s);
    ctx.lineTo(cx + 22 * s, by - 64 * s);
    ctx.closePath();
    ctx.fill();
  } else if (style.role === 'abyss_explorer') {
    const lanternY = handY + Math.sin(phase) * 2 * s;
    ctx.strokeStyle = utils.rgb(0x8f85a4);
    ctx.lineWidth = 1.2 * s;
    ctx.strokeRect(cx + 11 * s, lanternY - 8 * s, 9 * s, 11 * s);
    const glow = ctx.createRadialGradient(cx + 15.5 * s, lanternY - 3 * s, 0, cx + 15.5 * s, lanternY - 3 * s, 8 * s);
    glow.addColorStop(0, 'rgba(113,241,255,0.95)');
    glow.addColorStop(1, 'rgba(97,82,190,0)');
    ctx.fillStyle = glow;
    utils.fillCircle(ctx, cx + 15.5 * s, lanternY - 3 * s, 8 * s);
    ctx.fillStyle = utils.rgb(style.accent);
    utils.fillCircle(ctx, cx + 15.5 * s, lanternY - 3 * s, 2.2 * s);
  } else if (style.role === 'traveling_merchant') {
    const purseY = handY + Math.sin(phase * 2) * 2 * s;
    ctx.fillStyle = utils.rgb(style.leather);
    utils.fillCircle(ctx, cx + 15 * s, purseY - 4 * s, 5 * s);
    ctx.fillStyle = utils.rgb(0xd6ad42);
    utils.fillCircle(ctx, cx + 15 * s, purseY - 4 * s, 2 * s);
  } else if (style.role.startsWith('mercenary_')) {
    const propY = handY + Math.sin(phase) * 1.5 * s;
    ctx.strokeStyle = utils.rgb(style.leather);
    ctx.lineWidth = 2.2 * s;
    if (style.role === 'mercenary_tank') {
      ctx.fillStyle = utils.rgb(style.accent);
      utils.fillEllipse(ctx, cx - 18 * s, by - 34 * s, 9 * s, 13 * s);
      ctx.stroke();
      ctx.beginPath();
      ctx.moveTo(cx + 15 * s, propY - 20 * s);
      ctx.lineTo(cx + 19 * s, propY + 9 * s);
      ctx.stroke();
    } else if (style.role === 'mercenary_melee') {
      ctx.beginPath();
      ctx.moveTo(cx + 15 * s, propY + 8 * s);
      ctx.lineTo(cx + 22 * s, propY - 19 * s);
      ctx.stroke();
      ctx.strokeStyle = utils.rgb(style.accent);
      ctx.lineWidth = 4 * s;
      ctx.beginPath();
      ctx.moveTo(cx + 17 * s, propY - 18 * s);
      ctx.lineTo(cx + 25 * s, propY - 24 * s);
      ctx.stroke();
    } else if (style.role === 'mercenary_ranged') {
      ctx.beginPath();
      ctx.arc(cx + 18 * s, by - 34 * s, 14 * s, -1.3, 1.3);
      ctx.stroke();
      ctx.beginPath();
      ctx.moveTo(cx + 18 * s, by - 48 * s);
      ctx.lineTo(cx + 18 * s, by - 20 * s);
      ctx.stroke();
    } else {
      ctx.beginPath();
      ctx.moveTo(cx + 16 * s, propY + 8 * s);
      ctx.lineTo(cx + 17 * s, propY - 26 * s);
      ctx.stroke();
      ctx.fillStyle = utils.rgb(style.accent);
      utils.fillCircle(ctx, cx + 17 * s, propY - 29 * s, 5 * s);
      if (style.role === 'mercenary_mage') {
        ctx.fillStyle = utils.rgb(0x72d9ff, 0.75);
        utils.fillCircle(ctx, cx + 17 * s, propY - 29 * s, 2 * s);
      }
    }
  }
}

function createEventNPCDrawer(style: EventNPCStyle): EntityDrawer {
  return {
    key: style.key,
    frameW: 80,
    frameH: 120,
    totalFrames: 24,

    drawFrame(ctx, frame, action, w, h, utils) {
      const act = action as NPCAction;
      const count = FRAME_COUNTS[act] ?? 6;
      const phase = (frame / count) * Math.PI * 2;
      const s = w / 80;
      const cx = w / 2;
      const ground = h * 0.96;
      const isInjured = style.role === 'wounded_hunter' || style.role === 'wounded_explorer';

      const bob = act === 'alert'
        ? -1.5 * s
        : Math.sin(phase) * (act === 'working' ? 1.3 : 0.8) * s;
      const bodyLean = (Math.sin(phase * 0.5) * 0.8 + (isInjured ? -1.4 : 0)) * s;
      const armMotion = Math.sin(phase * (act === 'talking' ? 2 : 1)) * 2.5 * s;
      const headTilt = (isInjured ? -0.06 : 0) + Math.sin(phase) * 0.018;
      const by = ground + bob;

      ctx.save();
      ctx.fillStyle = 'rgba(0,0,0,0.25)';
      utils.fillEllipse(ctx, cx, ground + 3 * s, 18 * s, 4 * s);

      drawBackEquipment(ctx, style, cx, by, bodyLean, s, utils);

      for (const side of [-1, 1]) {
        const limp = isInjured && side === -1 ? 3 * s : 0;
        utils.drawPart(ctx, cx + side * 6 * s - 4 * s, by - 7 * s - limp, 8 * s, 8 * s, style.leather, 2 * s);
      }
      utils.drawPart(ctx, cx - 10 * s, by - 26 * s, 8 * s, 20 * s, style.trousers, 3 * s);
      utils.drawPart(ctx, cx + 2 * s, by - 26 * s, 8 * s, 20 * s, style.trousers, 3 * s);

      utils.zoneNpcOutline(ctx, w, h);
      utils.drawPart(ctx, cx - 13 * s + bodyLean, by - 52 * s, 26 * s, 30 * s, style.tunic, 5 * s);
      ctx.fillStyle = utils.rgb(style.accent, 0.9);
      ctx.fillRect(cx - 11 * s + bodyLean, by - 31 * s, 22 * s, 4 * s);
      ctx.fillStyle = utils.rgb(0xc6a75d);
      ctx.fillRect(cx - 2 * s + bodyLean, by - 31 * s, 4 * s, 4 * s);
      utils.softOutlineEnd(ctx);

      const leftHandX = cx - 16 * s + bodyLean;
      const rightHandX = cx + 16 * s + bodyLean;
      const handY = by - 29 * s + armMotion;
      utils.drawLimb(ctx, [
        { x: cx - 12 * s + bodyLean, y: by - 48 * s },
        { x: leftHandX, y: handY },
      ], 4.5 * s, style.tunic);
      utils.drawLimb(ctx, [
        { x: cx + 12 * s + bodyLean, y: by - 48 * s },
        { x: rightHandX, y: handY - armMotion * 0.6 },
      ], 4.5 * s, style.tunic);
      ctx.fillStyle = utils.rgb(style.skin);
      utils.fillCircle(ctx, leftHandX, handY, 3.2 * s);
      utils.fillCircle(ctx, rightHandX, handY - armMotion * 0.6, 3.2 * s);

      ctx.fillStyle = utils.rgb(style.skin);
      utils.roundRect(ctx, cx - 3.5 * s + bodyLean * 0.3, by - 57 * s, 7 * s, 7 * s, 2 * s);
      ctx.fill();

      ctx.save();
      ctx.translate(cx + bodyLean * 0.3, by - 64 * s);
      ctx.rotate(headTilt);
      ctx.fillStyle = utils.rgb(style.hair);
      utils.roundRect(ctx, -10 * s, -11 * s, 20 * s, 10 * s, 4 * s);
      ctx.fill();

      const headGradient = ctx.createRadialGradient(-3 * s, -3 * s, 0, 0, 0, 11 * s);
      headGradient.addColorStop(0, utils.rgb(utils.lighten(style.skin, 28)));
      headGradient.addColorStop(0.65, utils.rgb(style.skin));
      headGradient.addColorStop(1, utils.rgb(utils.darken(style.skin, 28)));
      ctx.fillStyle = headGradient;
      utils.roundRect(ctx, -9 * s, -8 * s, 18 * s, 17 * s, 5 * s);
      ctx.fill();

      ctx.fillStyle = '#e9e6dc';
      utils.fillEllipse(ctx, -4 * s, -1 * s, 2.4 * s, 2.2 * s);
      utils.fillEllipse(ctx, 4 * s, -1 * s, 2.4 * s, 2.2 * s);
      ctx.fillStyle = utils.rgb(0x32261f);
      utils.fillCircle(ctx, -4 * s, -0.5 * s, 1.2 * s);
      utils.fillCircle(ctx, 4 * s, -0.5 * s, 1.2 * s);
      ctx.strokeStyle = utils.rgb(utils.darken(style.skin, 36), 0.8);
      ctx.lineWidth = 1.1 * s;
      ctx.beginPath();
      ctx.moveTo(-3 * s, 5 * s);
      ctx.quadraticCurveTo(0, 3.5 * s, 3 * s, 5 * s);
      ctx.stroke();

      drawHeadwear(ctx, style, s, utils);
      ctx.restore();

      drawRoleDetails(ctx, style, cx, by, bodyLean, handY, phase, s, utils);
      ctx.restore();
    },
  };
}

export const RescueLostTravelerDrawer = createEventNPCDrawer({
  key: 'npc_rescue_lost_traveler', role: 'lost_traveler',
  skin: 0xa78463, hair: 0x493326, tunic: 0x526b62, trousers: 0x4a4f43,
  accent: 0x677d4d, leather: 0x674931,
});

export const RescueWoundedHunterDrawer = createEventNPCDrawer({
  key: 'npc_rescue_wounded_hunter', role: 'wounded_hunter',
  skin: 0x9b7659, hair: 0x392d24, tunic: 0x41563b, trousers: 0x3b4535,
  accent: 0x304a34, leather: 0x5b3e2b,
});

export const RescueTrappedMinerDrawer = createEventNPCDrawer({
  key: 'npc_rescue_trapped_miner', role: 'trapped_miner',
  skin: 0x9e795c, hair: 0x3a3029, tunic: 0x796047, trousers: 0x4c4640,
  accent: 0xb58a3b, leather: 0x493529,
});

export const RescueCaravanGuardDrawer = createEventNPCDrawer({
  key: 'npc_rescue_caravan_guard', role: 'caravan_guard',
  skin: 0x8f684b, hair: 0x2b211c, tunic: 0x6b4350, trousers: 0x423444,
  accent: 0x315e62, leather: 0x5a3c27,
});

export const RescueAbyssExplorerDrawer = createEventNPCDrawer({
  key: 'npc_rescue_abyss_explorer', role: 'abyss_explorer',
  skin: 0x917169, hair: 0x292331, tunic: 0x453957, trousers: 0x302b3d,
  accent: 0x55c7cf, leather: 0x312a39,
});

export const EscortTravelingMerchantDrawer = createEventNPCDrawer({
  key: 'npc_escort_traveling_merchant', role: 'traveling_merchant',
  skin: 0xa98663, hair: 0x3f2d21, tunic: 0x6b3e39, trousers: 0x354458,
  accent: 0x28706d, leather: 0x68472d,
});

export const EscortWoundedExplorerDrawer = createEventNPCDrawer({
  key: 'npc_escort_wounded_explorer', role: 'wounded_explorer',
  skin: 0x977055, hair: 0x443025, tunic: 0x8a6846, trousers: 0x51463b,
  accent: 0x8d3f3f, leather: 0x5f412d,
});

export const MercenaryTankDrawer = createEventNPCDrawer({
  key: 'npc_mercenary_tank', role: 'mercenary_tank',
  skin: 0x987354, hair: 0x30241e, tunic: 0x315678, trousers: 0x35404e,
  accent: 0x5ca0c7, leather: 0x4d392a,
});

export const MercenaryMeleeDrawer = createEventNPCDrawer({
  key: 'npc_mercenary_melee', role: 'mercenary_melee',
  skin: 0xa07558, hair: 0x3e261d, tunic: 0x8d3d37, trousers: 0x4c3430,
  accent: 0xd07a40, leather: 0x5d3826,
});

export const MercenaryRangedDrawer = createEventNPCDrawer({
  key: 'npc_mercenary_ranged', role: 'mercenary_ranged',
  skin: 0x997257, hair: 0x342c23, tunic: 0x376443, trousers: 0x354437,
  accent: 0x75b76a, leather: 0x60442c,
});

export const MercenaryHealerDrawer = createEventNPCDrawer({
  key: 'npc_mercenary_healer', role: 'mercenary_healer',
  skin: 0xa9876d, hair: 0x624f44, tunic: 0xd7d1c4, trousers: 0x536a79,
  accent: 0xf1ce68, leather: 0x6d5845,
});

export const MercenaryMageDrawer = createEventNPCDrawer({
  key: 'npc_mercenary_mage', role: 'mercenary_mage',
  skin: 0x8e6a66, hair: 0x30233b, tunic: 0x5b3977, trousers: 0x382d52,
  accent: 0x9f78dc, leather: 0x4d3458,
});

export const EVENT_NPC_DRAWERS: readonly EntityDrawer[] = [
  RescueLostTravelerDrawer,
  RescueWoundedHunterDrawer,
  RescueTrappedMinerDrawer,
  RescueCaravanGuardDrawer,
  RescueAbyssExplorerDrawer,
  EscortTravelingMerchantDrawer,
  EscortWoundedExplorerDrawer,
  MercenaryTankDrawer,
  MercenaryMeleeDrawer,
  MercenaryRangedDrawer,
  MercenaryHealerDrawer,
  MercenaryMageDrawer,
];

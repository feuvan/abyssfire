/**
 * Procedural item icons.
 *
 * Contract (used by UIScene and anything else that shows items):
 *   ensureItemIcon(scene, iconId, variant?) -> texture key, generated on first use.
 *   ensureItemIconFor(scene, item) -> resolves the item's base icon id (and its
 *     visual variant, so an iron sword and a flamberge look different).
 * Icons are ITEM_ICON_SIZE × ITEM_ICON_SIZE textures with a transparent
 * background; quality framing (border colour, glow) is the caller's job.
 *
 * Icon ids: w_sword w_axe w_mace w_dagger w_bow w_staff w_wand w_shield
 *   a_helm a_armor a_gloves a_boots a_belt j_ring j_amulet
 *   g_ruby g_sapphire g_emerald g_topaz g_diamond c_hp c_mp c_antidote c_scroll
 *   m_ore (crafting material fallback), m_scrap m_dust m_essence (blacksmith materials).
 * Variants: an item base id (e.g. 'w_claymore', 'g_ruby_3', 'c_hp_potion_l')
 * picks a visual tier of its icon; unknown or omitted -> the icon's default look.
 */
import Phaser from 'phaser';
import type { ItemInstance, WeaponBase } from '../../data/types';
import { getItemBase } from '../../data/items/bases';
import { capsulePath, ellipsePath, blobPath, polyPath } from '../sprites/rig/Rig';
import {
  P, UNIT, celI, tone, glow, css, mixHex, ribbonPath, sample, roundRectPath, sparkle, streak, line,
  clipTo, facetGem, gemCut, inked, starPath, type V, type InkOpts,
} from './IconKit';

/** Texture edge in pixels (already hi-res; display at ~36–48 px). */
export const ITEM_ICON_SIZE = 96;

export function itemIconKey(iconId: string, variant?: string): string {
  return variant ? `item_icon_${iconId}__${variant}` : `item_icon_${iconId}`;
}

/** Fallback icon id by item type when a base has no icon. */
function fallbackIconId(item: ItemInstance): string {
  const base = getItemBase(item.baseId);
  switch (base?.type) {
    case 'weapon': {
      const wt = (base as WeaponBase).weaponType;
      return wt ? `w_${wt}` : 'w_sword';
    }
    case 'armor': return 'a_armor';
    case 'accessory': return 'j_ring';
    case 'gem': return 'g_ruby';
    case 'scroll': return 'c_scroll';
    case 'material': return 'm_ore';
    default: return 'c_hp';
  }
}

export function iconIdFor(item: ItemInstance): string {
  return getItemBase(item.baseId)?.icon ?? fallbackIconId(item);
}

/** Visual variant for an item (its base id) when that base has a distinct look. */
export function iconVariantFor(item: ItemInstance): string | undefined {
  return hasItemIconVariant(item.baseId) ? item.baseId : undefined;
}

export function ensureItemIcon(scene: Phaser.Scene, iconId: string, variant?: string): string {
  const v = variant && hasItemIconVariant(variant) ? variant : undefined;
  const key = itemIconKey(iconId, v);
  if (scene.textures.exists(key)) return key;
  const canvas = document.createElement('canvas');
  canvas.width = ITEM_ICON_SIZE;
  canvas.height = ITEM_ICON_SIZE;
  const ctx = canvas.getContext('2d', { willReadFrequently: true })!;
  drawItemIcon(ctx, iconId, ITEM_ICON_SIZE, v);
  scene.textures.addCanvas(key, canvas);
  return key;
}

export function ensureItemIconFor(scene: Phaser.Scene, item: ItemInstance): string {
  return ensureItemIcon(scene, iconIdFor(item), iconVariantFor(item));
}

/** Every icon id this module draws (for previews / pre-warming). */
export const ITEM_ICON_IDS: readonly string[] = [
  'w_sword', 'w_axe', 'w_mace', 'w_dagger', 'w_bow', 'w_staff', 'w_wand', 'w_shield',
  'a_helm', 'a_armor', 'a_gloves', 'a_boots', 'a_belt', 'j_ring', 'j_amulet',
  'g_ruby', 'g_sapphire', 'g_emerald', 'g_topaz', 'g_diamond',
  'c_hp', 'c_mp', 'c_antidote', 'c_scroll', 'c_ley_fruit', 'm_ore', 'm_scrap', 'm_dust', 'm_essence',
];

/** Draw an item icon into `ctx` (size × size px, transparent background). */
export function drawItemIcon(ctx: CanvasRenderingContext2D, iconId: string, size: number, variant?: string): void {
  const spec = iconSpec(iconId, variant);
  inked(ctx, size, spec.draw, spec.opts);
}

// ════════════════════════════════════════════════════════════════════════
// Palette
// ════════════════════════════════════════════════════════════════════════

const STEEL = 0xc5ced9;
const IRON = 0x8c95a3;
const DARK_IRON = 0x4d5361;
const GOLD = 0xe3b44c;
const BRONZE = 0xc4843e;
const COPPER = 0xcf7a45;
const SILVER = 0xdde3ec;
const PLATINUM = 0xe8eef4;
const LEATHER = 0x8a5332;
const DARK_LEATHER = 0x5c3522;
const WOOD = 0xa06d3c;
const DARK_WOOD = 0x6b4428;
const CLOTH = 0xc2ad7e;
const BONE = 0xe9dfc2;
const DEMON = 0x6a1f2c;
const DEMON_BLACK = 0x2f2432;
const ABYSS = 0x3a2c58;
const CRIMSON = 0xff3a52;
const VIOLET = 0xa968ff;
const CYAN = 0x56dcff;
const DRAGON = 0x3f9d5c;
const RUBY = 0xe0283c;
const SAPPHIRE = 0x2f6cf0;
const EMERALD = 0x22c060;
const TOPAZ = 0xffc02a;
const DIAMOND = 0xd8f4ff;
const RUST = 0x9a5028;

interface IconSpec {
  draw: (c: CanvasRenderingContext2D) => void;
  opts?: InkOpts;
}

// ════════════════════════════════════════════════════════════════════════
// Variant tables
// ════════════════════════════════════════════════════════════════════════

type BladeShape = 'straight' | 'broad' | 'wavy' | 'serrated' | 'curved' | 'needle';
type GuardShape = 'bar' | 'cross' | 'wing' | 'disc';

interface BladeStyle {
  len: number;
  width: number;
  shape: BladeShape;
  blade: number;
  guard: number;
  guardShape: GuardShape;
  guardW?: number;
  grip: number;
  gripLen: number;
  pommel: number;
  gem?: number;
  glow?: number;
  rust?: boolean;
  fuller?: boolean;
  drip?: number;
}

const SWORDS: Record<string, BladeStyle> = {
  default: { len: 52, width: 5.4, shape: 'straight', blade: STEEL, guard: IRON, guardShape: 'bar', grip: LEATHER, gripLen: 15, pommel: IRON, fuller: true },
  w_rusty_sword: { len: 50, width: 5.2, shape: 'straight', blade: 0xa89684, guard: 0x756c66, guardShape: 'bar', grip: DARK_LEATHER, gripLen: 15, pommel: 0x756c66, rust: true },
  w_broad_sword: { len: 54, width: 8, shape: 'broad', blade: STEEL, guard: BRONZE, guardShape: 'cross', grip: LEATHER, gripLen: 15, pommel: BRONZE, fuller: true },
  w_militia_sword: { len: 56, width: 6, shape: 'straight', blade: 0xd3dbe4, guard: IRON, guardShape: 'cross', grip: 0x3e5a8a, gripLen: 16, pommel: IRON, fuller: true },
  w_claymore: { len: 62, width: 7, shape: 'broad', blade: 0xd8e0ea, guard: DARK_IRON, guardShape: 'cross', guardW: 20, grip: DARK_LEATHER, gripLen: 20, pommel: GOLD, gem: RUBY, fuller: true },
  w_flamberge: { len: 62, width: 6.4, shape: 'wavy', blade: 0xe6d6c8, guard: GOLD, guardShape: 'wing', guardW: 17, grip: 0x8a1f24, gripLen: 18, pommel: GOLD, gem: 0xff7a1a, glow: 0xff8a2a },
  w_demon_blade: { len: 60, width: 7.4, shape: 'serrated', blade: 0x4a3446, guard: DEMON, guardShape: 'wing', guardW: 18, grip: DEMON_BLACK, gripLen: 17, pommel: DEMON, gem: CRIMSON, glow: CRIMSON },
};
SWORDS.w_short_sword = SWORDS.default;

const DAGGERS: Record<string, BladeStyle> = {
  default: { len: 36, width: 5.4, shape: 'straight', blade: STEEL, guard: IRON, guardShape: 'bar', guardW: 10, grip: LEATHER, gripLen: 13, pommel: IRON },
  w_stiletto: { len: 42, width: 3.4, shape: 'needle', blade: SILVER, guard: GOLD, guardShape: 'disc', grip: 0x3a2f4a, gripLen: 14, pommel: GOLD },
  w_kris: { len: 38, width: 5, shape: 'wavy', blade: 0xcfc4b0, guard: BRONZE, guardShape: 'bar', guardW: 9, grip: WOOD, gripLen: 13, pommel: BRONZE },
  w_assassin_blade: { len: 38, width: 6, shape: 'curved', blade: 0xbfd0c2, guard: DARK_IRON, guardShape: 'bar', guardW: 9, grip: 0x2c3a2c, gripLen: 13, pommel: DARK_IRON, drip: 0x7be03a },
  w_shadow_blade: { len: 40, width: 5.6, shape: 'curved', blade: 0x5a4a82, guard: DARK_IRON, guardShape: 'wing', guardW: 11, grip: DEMON_BLACK, gripLen: 13, pommel: DARK_IRON, gem: VIOLET, glow: VIOLET },
  w_abyssal_dagger: { len: 40, width: 6.2, shape: 'serrated', blade: 0x3a2838, guard: DEMON, guardShape: 'wing', guardW: 12, grip: DEMON_BLACK, gripLen: 13, pommel: DEMON, gem: CRIMSON, glow: CRIMSON },
};

interface BowStyle {
  h: number;
  bulge: number;
  recurve: number;
  limbW: number;
  wood: number;
  tip?: number;
  bands?: number;
  wrap: number;
  glow?: number;
  feathers?: number;
  spikes?: boolean;
}

const BOWS: Record<string, BowStyle> = {
  default: { h: 42, bulge: 13, recurve: 0, limbW: 5, wood: 0xb67c45, wrap: 0x6a3f24 },
  w_long_bow: { h: 48, bulge: 13, recurve: 0, limbW: 4.8, wood: 0x8c5530, wrap: 0x2f5a3a, tip: 0xd8c8a0 },
  w_composite_bow: { h: 44, bulge: 12, recurve: 6, limbW: 5.2, wood: 0xa06a3a, wrap: 0x7a2a24, tip: 0x3a2a24, bands: 0xe0cfa0 },
  w_war_bow: { h: 46, bulge: 13, recurve: 8, limbW: 5.6, wood: 0x5c3a28, wrap: 0x8a2a24, tip: STEEL, bands: IRON, spikes: true },
  w_eagle_bow: { h: 46, bulge: 13, recurve: 6, limbW: 5.2, wood: 0xeee4cc, wrap: 0x2f5aa0, tip: GOLD, bands: GOLD, feathers: 0xf4f0e6 },
  w_abyssal_bow: { h: 47, bulge: 14, recurve: 9, limbW: 5.6, wood: 0x2e2640, wrap: 0x5a1f2c, tip: 0x6a3aa0, bands: 0x7a4ac0, glow: VIOLET, spikes: true },
};
BOWS.w_short_bow = BOWS.default;

type StaffHead = 'oak' | 'arcane' | 'rune' | 'elder' | 'lich' | 'abyss';
const STAFFS: Record<string, StaffHead> = {
  default: 'oak', w_oak_staff: 'oak', w_arcane_staff: 'arcane', w_rune_staff: 'rune',
  w_elder_staff: 'elder', w_lich_staff: 'lich', w_abyssal_staff: 'abyss',
};

type ShieldKind = 'wood' | 'iron' | 'tower' | 'kite' | 'rune' | 'abyss';
const SHIELDS: Record<string, ShieldKind> = {
  default: 'iron', w_wooden_shield: 'wood', w_iron_shield: 'iron', w_tower_shield: 'tower',
  w_kite_shield: 'kite', w_runic_shield: 'rune', w_abyssal_shield: 'abyss',
};

type HelmKind = 'cloth' | 'leather' | 'iron' | 'full' | 'plate' | 'demon' | 'dragon';
const HELMS: Record<string, HelmKind> = {
  default: 'iron', a_cloth_cap: 'cloth', a_leather_helm: 'leather', a_iron_helm: 'iron',
  a_full_helm: 'full', a_plate_helm: 'plate', a_demon_helm: 'demon', a_dragon_helm: 'dragon',
};

type ArmorKind = 'quilted' | 'leather' | 'chain' | 'scale' | 'plate' | 'heavy' | 'demon' | 'dragon';
const ARMORS: Record<string, ArmorKind> = {
  default: 'leather', a_quilted_armor: 'quilted', a_leather_armor: 'leather', a_chain_mail: 'chain',
  a_scale_mail: 'scale', a_plate_armor: 'plate', a_heavy_plate_armor: 'heavy', a_demon_armor: 'demon',
  a_dragon_armor: 'dragon',
};

type LimbKind = 'leather' | 'chain' | 'iron' | 'plate' | 'demon';
const GLOVES: Record<string, LimbKind> = {
  default: 'leather', a_leather_gloves: 'leather', a_chain_gloves: 'chain', a_gauntlets: 'iron',
  a_plate_gloves: 'plate', a_demon_gloves: 'demon',
};
const BOOTS: Record<string, LimbKind> = {
  default: 'leather', a_leather_boots: 'leather', a_chain_boots: 'chain', a_greaves: 'iron',
  a_plate_boots: 'plate', a_demon_boots: 'demon',
};

type BeltKind = 'leather' | 'heavy' | 'war' | 'plated';
const BELTS: Record<string, BeltKind> = {
  default: 'leather', a_leather_belt: 'leather', a_heavy_belt: 'heavy', a_war_belt: 'war', a_plated_belt: 'plated',
};

const RINGS: Record<string, { band: number; gem?: number }> = {
  default: { band: GOLD, gem: RUBY },
  j_copper_ring: { band: COPPER },
  j_silver_ring: { band: SILVER, gem: SAPPHIRE },
  j_gold_ring: { band: GOLD, gem: RUBY },
  j_platinum_ring: { band: PLATINUM, gem: 0xb070ff },
};

type AmuletKind = 'bone' | 'jade' | 'arcane';
const AMULETS: Record<string, AmuletKind> = {
  default: 'jade', j_bone_amulet: 'bone', j_jade_amulet: 'jade', j_arcane_amulet: 'arcane',
};

const POTION_SIZES: Record<string, 's' | 'm' | 'l'> = {
  c_hp_potion_s: 's', c_hp_potion_m: 'm', c_hp_potion_l: 'l',
  c_mp_potion_s: 's', c_mp_potion_m: 'm', c_mp_potion_l: 'l',
};

const SCROLLS: Record<string, 'tp' | 'id'> = { default: 'tp', c_tp_scroll: 'tp', c_id_scroll: 'id' };

function gemTier(variant?: string): number {
  const m = variant?.match(/^g_[a-z]+_(\d)$/);
  return m ? Number(m[1]) : 2;
}

const VARIANT_TABLES: Record<string, unknown>[] = [
  SWORDS, DAGGERS, BOWS, STAFFS, SHIELDS, HELMS, ARMORS, GLOVES, BOOTS, BELTS, RINGS, AMULETS, POTION_SIZES, SCROLLS,
];

/** True when `baseId` has a visual variant distinct from its icon's default. */
export function hasItemIconVariant(baseId: string): boolean {
  if (/^g_[a-z]+_\d$/.test(baseId)) return true;
  return VARIANT_TABLES.some((t) => baseId in t && baseId !== 'default');
}

function pick<T>(table: Record<string, T>, variant?: string): T {
  return (variant && table[variant]) || table.default;
}

// ════════════════════════════════════════════════════════════════════════
// Dispatcher
// ════════════════════════════════════════════════════════════════════════

function iconSpec(iconId: string, variant?: string): IconSpec {
  switch (iconId) {
    case 'w_sword': return bladeSpec(pick(SWORDS, variant), false);
    case 'w_dagger': return bladeSpec(pick(DAGGERS, variant), true);
    case 'w_axe': return { draw: drawAxe };
    case 'w_mace': return { draw: drawMace };
    case 'w_bow': return bowSpec(pick(BOWS, variant));
    case 'w_staff': return staffSpec(pick(STAFFS, variant));
    case 'w_wand': return wandSpec();
    case 'w_shield': return shieldSpec(pick(SHIELDS, variant));
    case 'a_helm': return helmSpec(pick(HELMS, variant));
    case 'a_armor': return armorSpec(pick(ARMORS, variant));
    case 'a_gloves': return glovesSpec(pick(GLOVES, variant));
    case 'a_boots': return bootsSpec(pick(BOOTS, variant));
    case 'a_belt': return beltSpec(pick(BELTS, variant));
    case 'j_ring': return ringSpec(pick(RINGS, variant));
    case 'j_amulet': return amuletSpec(pick(AMULETS, variant));
    case 'g_ruby': return gemSpec(RUBY, 'oval', gemTier(variant));
    case 'g_sapphire': return gemSpec(SAPPHIRE, 'pear', gemTier(variant));
    case 'g_emerald': return gemSpec(EMERALD, 'emerald', gemTier(variant));
    case 'g_topaz': return gemSpec(TOPAZ, 'trillion', gemTier(variant));
    case 'g_diamond': return gemSpec(DIAMOND, 'round', gemTier(variant));
    case 'c_hp': return potionSpec(0xe02838, (variant && POTION_SIZES[variant]) || 'm', 'round');
    case 'c_mp': return potionSpec(0x2f6cf0, (variant && POTION_SIZES[variant]) || 'm', 'round');
    case 'c_antidote': return potionSpec(0x5fd03a, 'm', 'square');
    case 'c_scroll': return scrollSpec(pick(SCROLLS, variant));
    case 'c_ley_fruit': return { draw: drawLeyFruit, opts: { under: (c) => glow(c, P(48, 56), 40, 0x6ff0c8, 0.6), over: leyFruitSparkles } };
    case 'm_ore': return { draw: drawOre };
    case 'm_scrap': return { draw: drawScrap };
    case 'm_dust': return { draw: drawDust, opts: { under: (c) => glow(c, P(48, 60), 40, 0x5f8cff, 0.55), over: dustSparkles } };
    case 'm_essence': return { draw: drawEssence, opts: { under: (c) => glow(c, P(48, 50), 42, 0xffc23a, 0.7), over: essenceSparkles } };
    default:
      if (iconId.startsWith('w_')) return bladeSpec(SWORDS.default, false);
      if (iconId.startsWith('a_')) return armorSpec('leather');
      if (iconId.startsWith('j_')) return ringSpec(RINGS.default);
      if (iconId.startsWith('g_')) return gemSpec(RUBY, 'oval', 2);
      if (iconId.startsWith('c_s')) return scrollSpec('tp');
      return potionSpec(0xe02838, 'm', 'round');
  }
}

// ════════════════════════════════════════════════════════════════════════
// Shared helpers
// ════════════════════════════════════════════════════════════════════════

/** Offset a polyline along its left normal by d(t). */
function offsetLine(pts: readonly V[], d: (t: number) => number): V[] {
  const n = pts.length;
  return pts.map((p, i) => {
    const a = pts[Math.max(0, i - 1)];
    const b = pts[Math.min(n - 1, i + 1)];
    const dx = b.x - a.x;
    const dy = b.y - a.y;
    const len = Math.hypot(dx, dy) || 1;
    const k = d(i / (n - 1));
    return P(p.x + (dy / len) * k, p.y + (-dx / len) * k);
  });
}

/** Orb with a radial body, specular highlight and bloom. */
function orb(c: CanvasRenderingContext2D, x: number, y: number, r: number, color: number): void {
  const t = tone(color, { light: 0.5 });
  const g = c.createRadialGradient(x - r * 0.35, y - r * 0.4, r * 0.1, x, y, r);
  g.addColorStop(0, css(mixHex(color, 0xffffff, 0.75)));
  g.addColorStop(0.45, t.base);
  g.addColorStop(1, t.shade);
  c.beginPath();
  c.arc(x, y, r, 0, Math.PI * 2);
  c.fillStyle = g;
  c.fill();
  c.strokeStyle = t.line;
  c.lineWidth = 1.3;
  c.stroke();
  // inner swirl
  c.save();
  c.beginPath();
  c.arc(x, y, r * 0.98, 0, Math.PI * 2);
  c.clip();
  c.strokeStyle = css(mixHex(color, 0xffffff, 0.5), 0.55);
  c.lineWidth = 1.2;
  c.beginPath();
  c.arc(x + r * 0.2, y + r * 0.1, r * 0.55, Math.PI * 0.1, Math.PI * 1.1);
  c.stroke();
  c.restore();
  c.beginPath();
  c.ellipse(x - r * 0.38, y - r * 0.42, r * 0.28, r * 0.17, -0.7, 0, Math.PI * 2);
  c.fillStyle = 'rgba(255,255,255,0.9)';
  c.fill();
}

function rivet(c: CanvasRenderingContext2D, x: number, y: number, r: number, color: number): void {
  const t = tone(color);
  c.beginPath();
  c.arc(x, y, r, 0, Math.PI * 2);
  c.fillStyle = t.shade;
  c.fill();
  c.beginPath();
  c.arc(x - r * 0.25, y - r * 0.25, r * 0.7, 0, Math.PI * 2);
  c.fillStyle = t.light;
  c.fill();
}

function smallGem(c: CanvasRenderingContext2D, x: number, y: number, r: number, color: number): void {
  facetGem(c, x, y, gemCut('round', r), color, { table: 0.5, stroke: 1, sparkles: r > 3.5 ? 1 : 0 });
}

// ════════════════════════════════════════════════════════════════════════
// Blades (swords & daggers)
// ════════════════════════════════════════════════════════════════════════

function bladeWidth(st: BladeStyle, t: number): [number, number] {
  const w = st.width;
  const tipStart = st.shape === 'broad' ? 0.88 : 0.8;
  const taper = (k: number): number => {
    if (t < tipStart) return w * (1 - k * t);
    const edge = w * (1 - k * tipStart);
    return edge * Math.pow((1 - t) / (1 - tipStart), 0.9);
  };
  switch (st.shape) {
    case 'broad': { const v = taper(0.1); return [v, v]; }
    case 'needle': { const v = w * Math.pow(1 - t, 0.85) + 0.2; return [v, v]; }
    case 'wavy': {
      const v = taper(0.2) * (1 + 0.2 * Math.sin(t * Math.PI * 8) * Math.min(1, (1 - t) * 4));
      return [v, v];
    }
    case 'serrated': {
      const v = taper(0.14);
      const saw = t > 0.08 && t < 0.78 ? 2.4 * ((t * 11) % 1) : 0;
      return [v, v + saw];
    }
    case 'curved': {
      const v = w * (0.75 + 0.45 * Math.sin(Math.PI * Math.min(1, t * 1.1) * 0.85)) * Math.pow(1 - t, 0.55);
      return [v * 0.8, v * 1.1];
    }
    default: { const v = taper(0.16); return [v, v]; }
  }
}

function bladeSpec(st: BladeStyle, dagger: boolean): IconSpec {
  const span = st.len + st.gripLen + 9;
  const fit = Math.min(1.5, (dagger ? 100 : 112) / span);
  const dir = P(Math.SQRT1_2, -Math.SQRT1_2);
  const mid = (st.len - st.gripLen - 8) / 2 * fit;
  const gx = 48 - dir.x * mid;
  const gy = 48 - dir.y * mid;
  const draw = (c: CanvasRenderingContext2D): void => {
    c.save();
    c.translate(gx, gy);
    c.rotate(Math.PI / 4);
    c.scale(fit, fit);
    drawBladeLocal(c, st);
    c.restore();
  };
  return { draw, opts: { inkWidth: 2 / Math.max(1, fit * 0.85) } };
}

/**
 * Draw a sword/dagger (by base id, e.g. 'w_flamberge') in local space: guard
 * at the origin, blade pointing along -y. Shared with the skill emblems.
 */
export function drawBladeMotif(c: CanvasRenderingContext2D, baseId: string, dagger = false): void {
  drawBladeLocal(c, dagger ? pick(DAGGERS, baseId) : pick(SWORDS, baseId));
}

function drawBladeLocal(c: CanvasRenderingContext2D, st: BladeStyle): void {
  const L = st.len;
  const curve = st.shape === 'curved' ? -0.22 * L : 0;
  const pts = sample(28, (t) => P(curve * t * t, -1.5 - t * (L - 1.5)));
  const wf = (t: number): [number, number] => bladeWidth(st, t);
  const bt = tone(st.blade, { light: 0.45 });
  const bladePath = (): void => ribbonPath(c, pts, wf);

  // Grip
  const gt = tone(st.grip);
  celI(c, () => capsulePath(c, P(0, 1), P(0, st.gripLen), 3.1, 2.8), gt, { band: 1.6, hi: 0.6, stroke: 1.1 });
  clipTo(c, () => capsulePath(c, P(0, 1), P(0, st.gripLen), 3.1, 2.8), () => {
    for (let y = 3; y < st.gripLen; y += 3.4) line(c, [P(-3.5, y), P(3.5, y + 2)], 1, gt.shade);
  });
  // Pommel
  const pt = tone(st.pommel, { light: 0.45 });
  celI(c, () => c.arc(0, st.gripLen + 3.6, 4.2, 0, Math.PI * 2), pt, { band: 1.6, hi: 0.6, stroke: 1.1 });
  if (st.gem) smallGem(c, 0, st.gripLen + 3.6, 2.4, st.gem);

  // Blade
  celI(c, bladePath, bt, { band: 1.8, hi: 0.8, stroke: 1.2 });
  clipTo(c, bladePath, () => {
    // shaded half (away from the light)
    c.beginPath();
    ribbonPath(c, pts, (t) => [0, wf(t)[1] + 1]);
    c.fillStyle = bt.shade;
    c.globalAlpha = 0.6;
    c.fill();
    c.globalAlpha = 1;
    if (st.rust) {
      c.fillStyle = css(RUST, 0.75);
      for (const [x, y, r] of [[2, -12, 3], [-2, -25, 2.2], [3, -33, 2.6], [-1, -41, 1.6], [2.5, -6, 1.8]] as const) {
        c.beginPath();
        c.ellipse(x, y, r, r * 1.5, 0.3, 0, Math.PI * 2);
        c.fill();
      }
    }
    if (st.fuller) {
      line(c, pts.slice(1, 18), 1.6, bt.line);
      line(c, offsetLine(pts.slice(1, 18), () => 1.1), 0.8, bt.light);
    }
    // Bright edge along the lit side
    streak(c, offsetLine(pts.slice(0, 25), (t) => wf(t * 24 / 28)[0] - 1.1), 1.2, 'rgba(255,255,255,0.75)');
    if (st.glow) {
      c.save();
      c.globalCompositeOperation = 'lighter';
      streak(c, pts.slice(2, 22), 3.2, css(st.glow, 0.45));
      streak(c, pts.slice(2, 22), 1.2, css(mixHex(st.glow, 0xffffff, 0.5), 0.9));
      c.restore();
    }
  });
  if (st.drip) {
    const tip = pts[pts.length - 3];
    const dt = tone(st.drip, { light: 0.5 });
    celI(c, () => blobPath(c, [P(tip.x - 2, tip.y + 2), P(tip.x + 3, tip.y + 1), P(tip.x + 4, tip.y + 9), P(tip.x + 2.5, tip.y + 12), P(tip.x, tip.y + 9)]), dt, { band: 1, hi: 0.5, stroke: 0.9 });
    // poison sheen on the edge
    clipTo(c, bladePath, () => {
      c.fillStyle = css(st.drip!, 0.35);
      c.fillRect(-10, -L, 20, L * 0.45);
    });
  }

  // Guard
  const gw = st.guardW ?? st.width + 8;
  const guardT = tone(st.guard, { light: 0.45 });
  switch (st.guardShape) {
    case 'bar':
      celI(c, () => capsulePath(c, P(-gw, 0), P(gw, 0), 2.8, 2.8), guardT, { band: 1.4, hi: 0.6, stroke: 1.1 });
      break;
    case 'disc':
      celI(c, () => ellipsePath(c, P(0, 0), gw * 0.75, 2.8), guardT, { band: 1.2, hi: 0.5, stroke: 1.1 });
      break;
    case 'cross': {
      const q = sample(16, (t) => { const x = -gw + t * 2 * gw; return P(x, -Math.pow(x / gw, 2) * 5 + 1); });
      celI(c, () => ribbonPath(c, q, (t) => 2 + 1.2 * Math.pow(Math.abs(t - 0.5) * 2, 3)), guardT, { band: 1.4, hi: 0.6, stroke: 1.1 });
      celI(c, () => roundRectPath(c, -4.2, -2.5, 8.4, 6, 2), guardT, { band: 1.2, hi: 0.5, stroke: 1 });
      break;
    }
    case 'wing': {
      const w = st.width;
      celI(c, () => polyPath(c, [P(-gw - 2, -9), P(-w - 1, -2.5), P(0, -4), P(w + 1, -2.5), P(gw + 2, -9), P(gw - 3, 1.5), P(4, 4.2), P(0, 6), P(-4, 4.2), P(-gw + 3, 1.5)]), guardT, { band: 1.6, hi: 0.6, stroke: 1.1 });
      if (st.gem) smallGem(c, 0, 0.6, 3, st.gem);
      break;
    }
  }
  if (st.guardShape !== 'wing' && st.gem && st.guardShape === 'cross') smallGem(c, 0, 0.5, 2.4, st.gem);
}

// ════════════════════════════════════════════════════════════════════════
// Axe, mace, wand
// ════════════════════════════════════════════════════════════════════════

/** Battle axe centred in the 96-unit icon space (shared with skill emblems). */
export function drawAxe(c: CanvasRenderingContext2D): void {
  c.save();
  c.translate(48, 50);
  c.rotate(Math.PI / 4.6);
  // Haft
  const wt = tone(WOOD);
  const haft = (): void => capsulePath(c, P(0, 44), P(0, -44), 3.6, 3.2);
  celI(c, haft, wt, { band: 1.8, hi: 0.7 });
  clipTo(c, haft, () => { for (let y = 20; y < 42; y += 4) line(c, [P(-4, y), P(4, y + 2.5)], 1.3, tone(DARK_LEATHER).base); });
  celI(c, () => c.arc(0, 45, 4.6, 0, Math.PI * 2), tone(IRON), { band: 1.4, hi: 0.6, stroke: 1.1 });
  // Big crescent blade (lit side)
  const st = tone(STEEL, { light: 0.45 });
  const big = (): void => {
    c.moveTo(-3, -36);
    c.quadraticCurveTo(-14, -40, -26, -52);
    c.quadraticCurveTo(-42, -28, -27, -4);
    c.quadraticCurveTo(-16, -14, -3, -16);
    c.closePath();
  };
  celI(c, big, st, { band: 2.2, hi: 1 });
  clipTo(c, big, () => {
    // edge bevel band
    c.beginPath();
    c.moveTo(-26, -52);
    c.quadraticCurveTo(-42, -28, -27, -4);
    c.quadraticCurveTo(-35, -28, -22, -48);
    c.closePath();
    c.fillStyle = css(mixHex(STEEL, 0xffffff, 0.65));
    c.fill();
    c.fillStyle = st.shade;
    c.globalAlpha = 0.5;
    c.beginPath();
    c.moveTo(-3, -24);
    c.quadraticCurveTo(-18, -18, -27, -4);
    c.lineTo(-3, -4);
    c.fill();
    c.globalAlpha = 1;
  });
  // Back blade
  const back = (): void => {
    c.moveTo(3, -34);
    c.quadraticCurveTo(10, -38, 18, -44);
    c.quadraticCurveTo(26, -28, 18, -12);
    c.quadraticCurveTo(10, -18, 3, -18);
    c.closePath();
  };
  celI(c, back, tone(mixHex(STEEL, 0x6a7090, 0.3)), { band: 1.8, hi: 0.8 });
  // Socket & top spike
  celI(c, () => polyPath(c, [P(-1.5, -58), P(3.5, -44), P(-3.5, -44)]), st, { band: 1, hi: 0.5, stroke: 1.1 });
  celI(c, () => roundRectPath(c, -5, -40, 10, 26, 2.5), tone(DARK_IRON), { band: 1.6, hi: 0.7 });
  rivet(c, 0, -33, 1.8, GOLD);
  rivet(c, 0, -21, 1.8, GOLD);
  c.restore();
  sparkle(c, 20, 30, 5);
}

function drawMace(c: CanvasRenderingContext2D): void {
  c.save();
  c.translate(48, 50);
  c.rotate(Math.PI / 4);
  const haft = (): void => capsulePath(c, P(0, 42), P(0, -30), 4.4, 4);
  celI(c, haft, tone(DARK_WOOD), { band: 1.6, hi: 0.7 });
  clipTo(c, haft, () => { for (let y = 18; y < 40; y += 4) line(c, [P(-4, y), P(4, y + 2.5)], 1.3, tone(LEATHER).light); });
  celI(c, () => c.arc(0, 43, 4.4, 0, Math.PI * 2), tone(IRON), { band: 1.4, hi: 0.6, stroke: 1.1 });
  const ht = tone(IRON, { light: 0.45 });
  // flanges
  for (const a of [-1.6, -0.8, 0, 0.8, 1.6]) {
    c.save();
    c.translate(0, -38);
    c.rotate(a);
    celI(c, () => polyPath(c, [P(-4, 6), P(0, -18), P(4, 6)]), ht, { band: 1.4, hi: 0.6 });
    c.restore();
  }
  celI(c, () => c.arc(0, -38, 11, 0, Math.PI * 2), ht, { band: 2.4, hi: 1 });
  celI(c, () => c.arc(0, -38, 5, 0, Math.PI * 2), tone(GOLD), { band: 1.2, hi: 0.5 });
  c.restore();
}

function wandSpec(): IconSpec {
  return {
    draw: (c) => {
      c.save();
      c.translate(48, 50);
      c.rotate(Math.PI / 4);
      celI(c, () => capsulePath(c, P(0, 36), P(0, -26), 2.6, 3.4), tone(0x7a3a8a), { band: 1.4, hi: 0.6 });
      for (const y of [-18, 20]) celI(c, () => roundRectPath(c, -4.2, y, 8.4, 4, 1.5), tone(GOLD), { band: 1, hi: 0.4, stroke: 1 });
      c.restore();
      facetGem(c, 66, 30, gemCut('marquise', 11).map((p) => P(p.x * 0.8 + p.y * 0.55, p.y * 0.8 - p.x * 0.55)), CYAN, { sparkles: 1 });
    },
    opts: { over: (c) => { glow(c, P(66, 30), 20, CYAN, 0.35); sparkle(c, 76, 20, 5); } },
  };
}

// ════════════════════════════════════════════════════════════════════════
// Bows
// ════════════════════════════════════════════════════════════════════════

function bowSpec(st: BowStyle): IconSpec {
  const H = st.h;
  const B = st.bulge;
  const R = st.recurve;
  const shift = (B - R) / 2 - 2;
  const at = (t: number): V => P(-B * (1 - t * t) + R * Math.pow(Math.abs(t), 4) + shift, t * H);
  const pts = sample(40, (u) => at(u * 2 - 1));
  const tipTop = at(-1);
  const tipBot = at(1);
  const draw = (c: CanvasRenderingContext2D): void => {
    c.save();
    c.translate(48, 48);
    c.rotate(Math.PI / 4);
    // String
    line(c, [tipTop, P(tipTop.x + 1.2, 0), tipBot], 2.2, css(0xf4ecd4));
    const wood = tone(st.wood, { light: 0.4 });
    const limb = (): void => ribbonPath(c, pts, (t) => {
      const a = Math.abs(t * 2 - 1);
      const base = st.limbW * (1 - 0.5 * Math.pow(a, 1.3));
      return base + (a < 0.14 ? 1.3 : 0);
    });
    celI(c, limb, wood, { band: 1.8, hi: 0.8 });
    clipTo(c, limb, () => {
      // wood grain / lit edge
      streak(c, offsetLine(pts.slice(3, 38), () => st.limbW * 0.45), 1, css(mixHex(st.wood, 0xffffff, 0.45), 0.8));
      if (st.bands) {
        for (const u of [0.2, 0.33, 0.67, 0.8]) {
          const p = pts[Math.round(u * 40)];
          c.beginPath();
          c.arc(p.x, p.y, st.limbW + 1, 0, Math.PI * 2);
          c.fillStyle = css(st.bands);
          c.fill();
          c.beginPath();
          c.arc(p.x - 0.8, p.y - 0.8, st.limbW, 0, Math.PI * 2);
          c.fillStyle = css(mixHex(st.bands, 0xffffff, 0.35));
          c.fill();
        }
      }
      if (st.glow) {
        c.save();
        c.globalCompositeOperation = 'lighter';
        streak(c, pts.slice(6, 16), 1.4, css(st.glow, 0.9));
        streak(c, pts.slice(25, 35), 1.4, css(st.glow, 0.9));
        c.restore();
      }
    });
    // Grip wrap
    const gt = tone(st.wrap);
    const gp = at(0);
    celI(c, () => roundRectPath(c, gp.x - st.limbW - 1.8, -7, (st.limbW + 1.8) * 2, 14, 2.5), gt, { band: 1.3, hi: 0.5, stroke: 1.1 });
    clipTo(c, () => roundRectPath(c, gp.x - st.limbW - 1.8, -7, (st.limbW + 1.8) * 2, 14, 2.5), () => {
      for (let y = -6; y < 7; y += 2.6) line(c, [P(gp.x - 8, y), P(gp.x + 8, y + 1.5)], 0.9, gt.shade);
    });
    // Tips
    if (st.tip) {
      const tt = tone(st.tip, { light: 0.45 });
      for (const [a, b] of [[pts[0], pts[4]], [pts[40], pts[36]]] as const) {
        celI(c, () => capsulePath(c, a, b, 2.2, st.limbW * 0.9), tt, { band: 1, hi: 0.4, stroke: 1 });
      }
    }
    if (st.spikes) {
      const sp = tone(st.tip ?? STEEL, { light: 0.4 });
      for (const [u, s] of [[0.25, -1], [0.75, -1]] as const) {
        const p = pts[Math.round(u * 40)];
        celI(c, () => polyPath(c, [P(p.x - 2, p.y - 3), P(p.x - 9 * -s * -1 - 2, p.y + (u < 0.5 ? -2 : 2)), P(p.x - 2, p.y + 3)]), sp, { band: 0.8, hi: 0.4, stroke: 0.9 });
      }
    }
    if (st.feathers) {
      const ft = tone(st.feathers);
      for (const p of [pts[2], pts[38]]) {
        const d = p.y < 0 ? -1 : 1;
        celI(c, () => blobPath(c, [P(p.x, p.y), P(p.x - 8, p.y + d * 4), P(p.x - 12, p.y + d * 12), P(p.x - 4, p.y + d * 8)]), ft, { band: 1.2, hi: 0.5, stroke: 1 });
        line(c, [P(p.x - 1, p.y + d * 1), P(p.x - 9, p.y + d * 9)], 0.8, ft.shade);
      }
    }
    c.restore();
  };
  return {
    draw,
    opts: st.glow ? { over: (c) => { glow(c, P(48 - 16, 48 - 16), 14, st.glow!, 0.25); } } : undefined,
  };
}

// ════════════════════════════════════════════════════════════════════════
// Staffs
// ════════════════════════════════════════════════════════════════════════

function staffSpec(kind: StaffHead): IconSpec {
  const a = P(14, 90);
  const b = P(64, 32);
  const gnarled = kind === 'oak' || kind === 'elder';
  const hpts = sample(24, (t) => {
    const x = a.x + (b.x - a.x) * t;
    const y = a.y + (b.y - a.y) * t;
    const wob = gnarled ? Math.sin(t * Math.PI * 3.2) * 1.8 : 0;
    return P(x + wob * 0.7, y + wob * 0.7);
  });
  const haftColor = { oak: WOOD, arcane: 0x7a4a2e, rune: 0x6c7488, elder: 0xd8ccb0, lich: BONE, abyss: DEMON_BLACK }[kind];
  const orbColor = { oak: 0x6ee05a, arcane: 0x4f8cff, rune: CYAN, elder: 0xffb84a, lich: 0x7dff9a, abyss: CRIMSON }[kind];
  const head = P(68, 27);
  const draw = (c: CanvasRenderingContext2D): void => {
    const ht = tone(haftColor, { light: 0.4 });
    const haft = (): void => ribbonPath(c, hpts, (t) => 4.4 - t * 0.8 + (gnarled && Math.abs(t - 0.45) < 0.05 ? 1 : 0));
    celI(c, haft, ht, { band: 1.8, hi: 0.7 });
    clipTo(c, haft, () => {
      if (kind === 'lich') {
        for (let t = 0.1; t < 1; t += 0.16) { const p = hpts[Math.round(t * 24)]; line(c, [P(p.x - 4, p.y - 4), P(p.x + 4, p.y + 4)], 1.3, ht.shade); }
      } else if (kind === 'rune') {
        c.save();
        c.globalCompositeOperation = 'lighter';
        for (const t of [0.25, 0.45, 0.65]) { const p = hpts[Math.round(t * 24)]; c.fillStyle = css(CYAN, 0.9); c.fillRect(p.x - 1, p.y - 1, 2.2, 2.2); }
        c.restore();
      } else {
        streak(c, offsetLine(hpts.slice(2, 22), () => 1.6), 0.9, css(mixHex(haftColor, 0xffffff, 0.4), 0.8));
      }
    });
    // grip wrap & butt cap
    const wt = tone(kind === 'abyss' ? DEMON : kind === 'lich' ? 0x3a4a3a : 0x3e5a8a);
    const g0 = hpts[9];
    const g1 = hpts[13];
    celI(c, () => capsulePath(c, g0, g1, 4.2, 4.2), wt, { band: 1.2, hi: 0.5, stroke: 1.1 });
    clipTo(c, () => capsulePath(c, g0, g1, 4.2, 4.2), () => { for (let i = 0; i < 5; i++) { const p = hpts[9 + i]; line(c, [P(p.x - 5, p.y), P(p.x, p.y + 5)], 0.9, wt.shade); } });
    celI(c, () => polyPath(c, [P(a.x - 4, a.y - 3), P(a.x + 3, a.y + 3), P(a.x - 4, a.y + 5)]), tone(kind === 'elder' ? GOLD : IRON), { band: 1, hi: 0.4, stroke: 1 });

    switch (kind) {
      case 'oak': {
        // twigs cradling a green gem
        const tw = tone(WOOD);
        for (const pts of [
          [P(62, 34), P(56, 22), P(58, 12), P(63, 14)],
          [P(64, 32), P(78, 28), P(84, 18), P(80, 16)],
        ]) celI(c, () => ribbonPath(c, sample(10, (t) => bez(pts, t)), (t) => 2.6 * (1 - t) + 0.6), tw, { band: 1.2, hi: 0.5, stroke: 1 });
        for (const [x, y, r] of [[54, 14, -0.6], [84, 30, 0.9]] as const) {
          c.save(); c.translate(x, y); c.rotate(r);
          celI(c, () => blobPath(c, [P(0, -6), P(3.5, 0), P(0, 6), P(-3.5, 0)]), tone(0x5ab04a), { band: 1.2, hi: 0.5, stroke: 1 });
          line(c, [P(0, -5), P(0, 5)], 0.8, tone(0x5ab04a).shade);
          c.restore();
        }
        facetGem(c, 70, 22, gemCut('oval', 8), 0x4ad05a, { sparkles: 1 });
        break;
      }
      case 'arcane': {
        const gt = tone(GOLD, { light: 0.45 });
        celI(c, () => { c.arc(head.x, head.y, 14, Math.PI * 0.15, Math.PI * 1.35); c.arc(head.x - 1, head.y + 1, 9.5, Math.PI * 1.35, Math.PI * 0.15, true); c.closePath(); }, gt, { band: 1.4, hi: 0.6 });
        orb(c, head.x + 1, head.y - 1, 9, orbColor);
        break;
      }
      case 'rune': {
        const it = tone(0x8a94a8);
        for (const s of [-1, 1]) celI(c, () => polyPath(c, [P(head.x - 4, head.y + 10), P(head.x + s * 10 - 2, head.y - 2 - s * 2), P(head.x + s * 3, head.y + 4)]), it, { band: 1, hi: 0.4, stroke: 1.1 });
        facetGem(c, head.x, head.y - 4, gemCut('marquise', 13).map((p) => P(p.x * 0.8 + p.y * 0.5, p.y * 0.85 - p.x * 0.5)), CYAN, { sparkles: 1 });
        break;
      }
      case 'elder': {
        orb(c, head.x, head.y, 11, orbColor);
        const tw = tone(0xd8ccb0);
        for (const pts of [
          [P(60, 36), P(52, 20), P(62, 10), P(72, 14)],
          [P(64, 34), P(82, 34), P(84, 18), P(76, 14)],
        ]) celI(c, () => ribbonPath(c, sample(12, (t) => bez(pts, t)), (t) => 2.8 * (1 - t) + 0.7), tw, { band: 1.2, hi: 0.5, stroke: 1 });
        break;
      }
      case 'lich': {
        const bt = tone(BONE);
        const skull = (): void => blobPath(c, [P(58, 18), P(66, 10), P(78, 12), P(82, 22), P(78, 32), P(74, 38), P(64, 38), P(60, 30)]);
        celI(c, skull, bt, { band: 2, hi: 0.8 });
        c.fillStyle = '#1a1420';
        c.beginPath(); c.ellipse(66, 25, 3.6, 4.2, 0, 0, Math.PI * 2); c.fill();
        c.beginPath(); c.ellipse(76, 24, 3.2, 4, 0, 0, Math.PI * 2); c.fill();
        c.beginPath(); polyPath(c, [P(70, 29), P(72, 33), P(69, 33)]); c.fill();
        for (let x = 64; x < 76; x += 3) line(c, [P(x, 36), P(x, 39)], 0.9, bt.shade);
        c.fillStyle = css(orbColor);
        c.beginPath(); c.arc(66.5, 25.5, 1.8, 0, Math.PI * 2); c.fill();
        c.beginPath(); c.arc(76, 24.5, 1.6, 0, Math.PI * 2); c.fill();
        // horns
        for (const pts of [[P(60, 16), P(52, 10), P(50, 2)], [P(80, 14), P(88, 10), P(90, 2)]]) {
          celI(c, () => ribbonPath(c, sample(8, (t) => bez(pts, t)), (t) => 2.6 * (1 - t) + 0.3), tone(0x8a7a60), { band: 1, hi: 0.4, stroke: 1 });
        }
        break;
      }
      case 'abyss': {
        const mt = tone(0x40324e, { light: 0.4 });
        celI(c, () => polyPath(c, [P(56, 38), P(50, 18), P(58, 24), P(60, 6), P(68, 18), P(78, 4), P(78, 20), P(92, 16), P(80, 30), P(74, 40)]), mt, { band: 1.6, hi: 0.6 });
        orb(c, head.x, head.y, 8.5, orbColor);
        break;
      }
    }
  };
  const glowy = kind !== 'oak';
  return {
    draw,
    opts: {
      over: (c) => {
        if (glowy) glow(c, head, kind === 'lich' ? 14 : 22, orbColor, kind === 'lich' ? 0.3 : 0.4);
        sparkle(c, head.x - 12, head.y - 10, 4.5, '#ffffff', 0.9);
      },
    },
  };
}

export function bez(p: readonly V[], t: number): V {
  if (p.length === 3) {
    const u = 1 - t;
    return P(u * u * p[0].x + 2 * u * t * p[1].x + t * t * p[2].x, u * u * p[0].y + 2 * u * t * p[1].y + t * t * p[2].y);
  }
  const u = 1 - t;
  return P(
    u * u * u * p[0].x + 3 * u * u * t * p[1].x + 3 * u * t * t * p[2].x + t * t * t * p[3].x,
    u * u * u * p[0].y + 3 * u * u * t * p[1].y + 3 * u * t * t * p[2].y + t * t * t * p[3].y,
  );
}

// ════════════════════════════════════════════════════════════════════════
// Shields
// ════════════════════════════════════════════════════════════════════════

export function heaterPath(c: CanvasRenderingContext2D, cx: number, top: number, w: number, h: number): void {
  c.moveTo(cx - w, top + 3);
  c.quadraticCurveTo(cx, top - 4, cx + w, top + 3);
  c.lineTo(cx + w, top + h * 0.38);
  c.quadraticCurveTo(cx + w * 0.95, top + h * 0.8, cx, top + h);
  c.quadraticCurveTo(cx - w * 0.95, top + h * 0.8, cx - w, top + h * 0.38);
  c.closePath();
}

function shieldSpec(kind: ShieldKind): IconSpec {
  const draw = (c: CanvasRenderingContext2D): void => {
    let path: (dx: number, dy: number) => void;
    let face: number;
    let rimC: number;
    switch (kind) {
      case 'wood':
        path = (dx, dy) => c.arc(46 + dx, 48 + dy, 36, 0, Math.PI * 2);
        face = 0xa8703c; rimC = IRON; break;
      case 'tower':
        path = (dx, dy) => { c.moveTo(22 + dx, 14 + dy); c.quadraticCurveTo(46 + dx, 6 + dy, 70 + dx, 14 + dy); c.lineTo(70 + dx, 74 + dy); c.quadraticCurveTo(46 + dx, 94 + dy, 22 + dx, 74 + dy); c.closePath(); };
        face = 0xa8323a; rimC = DARK_IRON; break;
      case 'kite':
        path = (dx, dy) => { c.moveTo(46 + dx, 6 + dy); c.quadraticCurveTo(76 + dx, 8 + dy, 74 + dx, 34 + dy); c.quadraticCurveTo(68 + dx, 64 + dy, 46 + dx, 92 + dy); c.quadraticCurveTo(24 + dx, 64 + dy, 18 + dx, 34 + dy); c.quadraticCurveTo(16 + dx, 8 + dy, 46 + dx, 6 + dy); c.closePath(); };
        face = 0x2f5aa8; rimC = GOLD; break;
      case 'rune':
        path = (dx, dy) => heaterPath(c, 46 + dx, 10 + dy, 32, 80);
        face = 0x4a5468; rimC = 0x8a94a8; break;
      case 'abyss':
        path = (dx, dy) => polyPath(c, [P(46 + dx, 4 + dy), P(58 + dx, 14 + dy), P(78 + dx, 8 + dy), P(72 + dx, 34 + dy), P(80 + dx, 50 + dy), P(64 + dx, 62 + dy), P(46 + dx, 92 + dy), P(28 + dx, 62 + dy), P(12 + dx, 50 + dy), P(20 + dx, 34 + dy), P(14 + dx, 8 + dy), P(34 + dx, 14 + dy)]);
        face = 0x3a2a4a; rimC = DEMON; break;
      default:
        path = (dx, dy) => heaterPath(c, 46 + dx, 10 + dy, 32, 80);
        face = 0xa4aebb; rimC = DARK_IRON; break;
    }
    // Edge thickness (3/4 view)
    celI(c, () => path(4, 3), tone(mixHex(rimC, 0x201830, 0.35)), { band: 1, hi: 0.5, stroke: 1.2, noLight: true });
    // Rim + face
    const rt = tone(rimC, { light: 0.45 });
    celI(c, () => path(0, 0), rt, { band: 2, hi: 0.8 });
    const inset = (): void => {
      c.save();
      c.translate(46, 48);
      c.scale(0.84, 0.86);
      c.translate(-46, -48);
      path(0, 0);
      c.restore();
    };
    const ft = tone(face, { light: 0.35 });
    celI(c, inset, ft, { band: 3.2, hi: 1.4, stroke: 1.2 });
    clipTo(c, inset, () => {
      switch (kind) {
        case 'wood':
          for (const x of [26, 38, 50, 62]) line(c, [P(x, 8), P(x, 90)], 1.4, ft.shade);
          for (const x of [27.5, 39.5, 51.5, 63.5]) line(c, [P(x, 8), P(x, 90)], 0.8, ft.light);
          line(c, [P(34, 30), P(38, 36)], 1, ft.shade);
          line(c, [P(56, 60), P(59, 68)], 1, ft.shade);
          break;
        case 'iron':
          c.fillStyle = css(0x3e5a8a); c.fillRect(40, 0, 12, 96);
          c.fillStyle = css(mixHex(0x3e5a8a, 0xffffff, 0.25)); c.fillRect(40, 0, 3, 96);
          break;
        case 'tower':
          c.fillStyle = css(GOLD); c.fillRect(42, 0, 8, 96); c.fillRect(0, 40, 96, 8);
          c.fillStyle = css(mixHex(GOLD, 0xffffff, 0.4)); c.fillRect(42, 0, 2.5, 96); c.fillRect(0, 40, 96, 2.5);
          break;
        case 'kite': {
          c.fillStyle = css(0xf0ead8);
          c.beginPath(); c.moveTo(0, 30); c.lineTo(46, 58); c.lineTo(96, 30); c.lineTo(96, 42); c.lineTo(46, 70); c.lineTo(0, 42); c.closePath(); c.fill();
          break;
        }
        case 'rune':
          c.save();
          c.globalCompositeOperation = 'lighter';
          c.strokeStyle = css(CYAN, 0.95);
          c.lineWidth = 2.2;
          c.lineCap = 'round';
          c.beginPath();
          c.moveTo(46, 24); c.lineTo(46, 70); c.moveTo(36, 36); c.lineTo(46, 46); c.lineTo(56, 36);
          c.moveTo(36, 56); c.lineTo(46, 62); c.lineTo(56, 56);
          c.stroke();
          c.restore();
          break;
        default: break;
      }
      // soft sheen across the face
      const g = c.createLinearGradient(20, 10, 70, 80);
      g.addColorStop(0, 'rgba(255,255,255,0.22)');
      g.addColorStop(0.45, 'rgba(255,255,255,0)');
      c.fillStyle = g;
      c.fillRect(0, 0, 96, 96);
    });
    // Boss / emblem
    switch (kind) {
      case 'wood':
        celI(c, () => c.arc(46, 48, 10, 0, Math.PI * 2), tone(IRON, { light: 0.5 }), { band: 2, hi: 0.8 });
        for (let i = 0; i < 8; i++) { const a = (i / 8) * Math.PI * 2; rivet(c, 46 + Math.cos(a) * 31.5, 48 + Math.sin(a) * 31.5, 1.8, STEEL); }
        break;
      case 'iron':
        celI(c, () => c.arc(46, 44, 8, 0, Math.PI * 2), tone(STEEL, { light: 0.5 }), { band: 1.8, hi: 0.8 });
        for (const [x, y] of [[22, 20], [70, 20], [24, 56], [68, 56]]) rivet(c, x, y, 2, STEEL);
        break;
      case 'tower':
        celI(c, () => c.arc(46, 44, 7, 0, Math.PI * 2), tone(GOLD, { light: 0.5 }), { band: 1.6, hi: 0.7 });
        for (const [x, y] of [[26, 18], [66, 18], [26, 72], [66, 72]]) rivet(c, x, y, 2, STEEL);
        break;
      case 'kite':
        celI(c, () => starPath(c, 46, 34, 4, 10, 3.6), tone(GOLD, { light: 0.5 }), { band: 1.2, hi: 0.5 });
        break;
      case 'rune':
        smallGem(c, 46, 46, 5, CYAN);
        break;
      case 'abyss':
        celI(c, () => ellipsePath(c, P(46, 44), 12, 7), tone(0xffe0a0), { band: 1, hi: 0.4 });
        c.fillStyle = css(CRIMSON);
        c.beginPath(); c.ellipse(46, 44, 4, 6.5, 0, 0, Math.PI * 2); c.fill();
        c.fillStyle = '#1a0a10';
        c.beginPath(); c.ellipse(46, 44, 1.5, 5, 0, 0, Math.PI * 2); c.fill();
        break;
    }
  };
  const over = kind === 'rune'
    ? (c: CanvasRenderingContext2D) => glow(c, P(46, 46), 26, CYAN, 0.28)
    : kind === 'abyss'
      ? (c: CanvasRenderingContext2D) => glow(c, P(46, 44), 20, CRIMSON, 0.4)
      : (c: CanvasRenderingContext2D) => sparkle(c, 28, 20, 4.5, '#ffffff', 0.9);
  return { draw, opts: { over } };
}

// ════════════════════════════════════════════════════════════════════════
// Helmets
// ════════════════════════════════════════════════════════════════════════

function helmSpec(kind: HelmKind): IconSpec {
  const draw = (c: CanvasRenderingContext2D): void => {
    const dome = (w: number, top: number, bottom: number, cx = 48): void => {
      c.moveTo(cx - w, bottom);
      c.bezierCurveTo(cx - w - 1, top + 6, cx - w * 0.6, top, cx, top);
      c.bezierCurveTo(cx + w * 0.6, top, cx + w + 1, top + 6, cx + w, bottom);
      c.closePath();
    };
    switch (kind) {
      case 'cloth': {
        const ct = tone(0x4f7a4a);
        celI(c, () => { c.moveTo(18, 66); c.bezierCurveTo(16, 30, 40, 14, 62, 18); c.quadraticCurveTo(84, 22, 84, 36); c.quadraticCurveTo(80, 30, 74, 34); c.bezierCurveTo(78, 46, 78, 58, 78, 66); c.closePath(); }, ct, { band: 3, hi: 1.3 });
        clipTo(c, () => { c.moveTo(18, 66); c.bezierCurveTo(16, 30, 40, 14, 62, 18); c.quadraticCurveTo(84, 22, 84, 36); c.lineTo(78, 66); c.closePath(); }, () => {
          line(c, [P(30, 60), P(36, 30), P(58, 20)], 1.2, ct.shade);
          line(c, [P(50, 62), P(56, 34)], 1.2, ct.shade);
        });
        const bt = tone(CLOTH);
        celI(c, () => roundRectPath(c, 14, 60, 68, 16, 7), bt, { band: 2, hi: 0.9 });
        for (let x = 22; x < 78; x += 7) line(c, [P(x, 63), P(x + 1, 73)], 1, bt.shade);
        celI(c, () => { c.arc(84, 38, 5, 0, Math.PI * 2); }, tone(0xd8423a), { band: 1.2, hi: 0.5 });
        break;
      }
      case 'leather': {
        const lt = tone(LEATHER);
        celI(c, () => dome(30, 14, 64), lt, { band: 3, hi: 1.3 });
        // ear flaps
        celI(c, () => blobPath(c, [P(18, 52), P(30, 54), P(30, 80), P(22, 84), P(16, 70)]), lt, { band: 2, hi: 0.9 });
        celI(c, () => blobPath(c, [P(66, 54), P(78, 52), P(80, 70), P(74, 84), P(66, 80)]), lt, { band: 2, hi: 0.9 });
        const st = tone(DARK_LEATHER);
        celI(c, () => roundRectPath(c, 16, 56, 64, 10, 4), st, { band: 1.6, hi: 0.7 });
        c.setLineDash([2.5, 2.5]);
        line(c, [P(48, 16), P(48, 56)], 1.2, lt.light);
        line(c, [P(24, 36), P(32, 20), P(48, 15)], 1, lt.light);
        c.setLineDash([]);
        rivet(c, 48, 61, 2.4, BRONZE);
        break;
      }
      case 'iron': {
        const it = tone(IRON, { light: 0.45 });
        celI(c, () => dome(30, 12, 66), it, { band: 3.4, hi: 1.4 });
        // riveted band + crest ridge
        celI(c, () => roundRectPath(c, 16, 58, 64, 11, 3), tone(DARK_IRON), { band: 1.6, hi: 0.7 });
        celI(c, () => roundRectPath(c, 45, 12, 7, 48, 3), tone(STEEL, { light: 0.5 }), { band: 1.2, hi: 0.5, stroke: 1.1 });
        // nasal
        celI(c, () => polyPath(c, [P(45, 62), P(52, 62), P(51, 86), P(48.5, 90), P(46, 86)]), tone(STEEL, { light: 0.5 }), { band: 1.2, hi: 0.5 });
        for (const x of [24, 36, 60, 72]) rivet(c, x, 63.5, 1.8, STEEL);
        streak(c, [P(26, 50), P(26, 30), P(40, 18)], 2, 'rgba(255,255,255,0.55)');
        break;
      }
      case 'full': {
        const it = tone(0xa5aebb, { light: 0.45 });
        const body = (): void => { c.moveTo(20, 86); c.lineTo(18, 30); c.quadraticCurveTo(18, 12, 48, 10); c.quadraticCurveTo(78, 12, 78, 30); c.lineTo(76, 86); c.quadraticCurveTo(48, 92, 20, 86); c.closePath(); };
        celI(c, body, it, { band: 3.4, hi: 1.4 });
        clipTo(c, body, () => {
          c.fillStyle = it.shade;
          c.globalAlpha = 0.45;
          c.fillRect(56, 0, 40, 96);
          c.globalAlpha = 1;
        });
        // eye slit & breaths
        c.fillStyle = '#16101c';
        c.beginPath(); roundRectPath(c, 22, 40, 52, 6, 2); c.fill();
        celI(c, () => roundRectPath(c, 45, 12, 6, 74, 2), tone(GOLD), { band: 1, hi: 0.4, stroke: 1 });
        for (const [x, y] of [[58, 58], [64, 58], [58, 66], [64, 66], [58, 74], [64, 74]]) { c.beginPath(); c.arc(x, y, 1.3, 0, Math.PI * 2); c.fillStyle = '#16101c'; c.fill(); }
        streak(c, [P(25, 70), P(24, 30), P(36, 16)], 2, 'rgba(255,255,255,0.5)');
        break;
      }
      case 'plate': {
        // plume
        const pl = tone(0xd8323a);
        celI(c, () => { c.moveTo(44, 16); c.bezierCurveTo(30, 2, 10, 6, 6, 22); c.bezierCurveTo(18, 14, 28, 16, 36, 24); c.closePath(); }, pl, { band: 2, hi: 0.9 });
        const it = tone(STEEL, { light: 0.5 });
        celI(c, () => { c.moveTo(20, 76); c.bezierCurveTo(14, 40, 26, 14, 50, 14); c.bezierCurveTo(74, 14, 82, 34, 82, 50); c.lineTo(86, 62); c.lineTo(76, 70); c.lineTo(74, 84); c.quadraticCurveTo(46, 90, 20, 76); c.closePath(); }, it, { band: 3.4, hi: 1.4 });
        // visor
        const vt = tone(0xaab4c2, { light: 0.45 });
        celI(c, () => { c.moveTo(38, 40); c.quadraticCurveTo(62, 32, 84, 44); c.lineTo(86, 60); c.quadraticCurveTo(62, 56, 40, 62); c.closePath(); }, vt, { band: 2, hi: 0.8 });
        c.fillStyle = '#16101c';
        c.beginPath(); c.moveTo(44, 48); c.quadraticCurveTo(64, 42, 82, 49); c.lineTo(82, 52); c.quadraticCurveTo(64, 46, 44, 52); c.closePath(); c.fill();
        rivet(c, 38, 51, 2.6, GOLD);
        celI(c, () => roundRectPath(c, 22, 74, 52, 7, 3), tone(GOLD), { band: 1, hi: 0.4, stroke: 1 });
        streak(c, [P(24, 62), P(26, 36), P(42, 20)], 2.2, 'rgba(255,255,255,0.6)');
        break;
      }
      case 'demon': {
        const hn = tone(0xd8c8a8);
        for (const pts of [[P(28, 30), P(10, 26), P(4, 8), P(14, 2)], [P(68, 30), P(86, 26), P(92, 8), P(82, 2)]]) {
          celI(c, () => ribbonPath(c, sample(14, (t) => bez(pts, t)), (t) => 6 * Math.pow(1 - t, 0.8) + 0.3), hn, { band: 1.8, hi: 0.7 });
          const band = sample(14, (t) => bez(pts, t));
          for (const i of [3, 6, 9]) line(c, [P(band[i].x - 3, band[i].y - 3), P(band[i].x + 3, band[i].y + 3)], 1, hn.shade);
        }
        const dt = tone(0x4a2436, { light: 0.35 });
        celI(c, () => { c.moveTo(20, 80); c.bezierCurveTo(14, 40, 26, 16, 48, 16); c.bezierCurveTo(70, 16, 82, 40, 76, 80); c.lineTo(62, 86); c.lineTo(48, 76); c.lineTo(34, 86); c.closePath(); }, dt, { band: 3.4, hi: 1.4 });
        celI(c, () => polyPath(c, [P(48, 8), P(54, 26), P(48, 30), P(42, 26)]), tone(0x8a2a3a), { band: 1, hi: 0.4 });
        c.fillStyle = '#10080c';
        c.beginPath(); polyPath(c, [P(26, 46), P(44, 52), P(44, 58), P(28, 54)]); c.fill();
        c.beginPath(); polyPath(c, [P(70, 46), P(52, 52), P(52, 58), P(68, 54)]); c.fill();
        c.fillStyle = css(CRIMSON);
        c.beginPath(); c.ellipse(37, 53, 3.6, 1.8, 0.3, 0, Math.PI * 2); c.fill();
        c.beginPath(); c.ellipse(59, 53, 3.6, 1.8, -0.3, 0, Math.PI * 2); c.fill();
        break;
      }
      case 'dragon': {
        const hn = tone(0xe8d8a0);
        for (const pts of [[P(30, 30), P(16, 24), P(6, 26), P(2, 36)], [P(66, 26), P(80, 12), P(92, 12), P(94, 4)]]) {
          celI(c, () => ribbonPath(c, sample(14, (t) => bez(pts, t)), (t) => 5.5 * Math.pow(1 - t, 0.8) + 0.3), hn, { band: 1.6, hi: 0.6 });
        }
        const gt = tone(DRAGON, { light: 0.4 });
        const shell = (): void => { c.moveTo(18, 80); c.bezierCurveTo(12, 40, 26, 14, 50, 14); c.bezierCurveTo(74, 14, 84, 40, 78, 80); c.quadraticCurveTo(48, 90, 18, 80); c.closePath(); };
        celI(c, shell, gt, { band: 3.4, hi: 1.4 });
        clipTo(c, shell, () => {
          for (let r = 0; r < 5; r++) for (let x = 14 + (r % 2) * 5; x < 86; x += 10) {
            const y = 26 + r * 9;
            c.beginPath(); c.arc(x, y, 5, 0.1 * Math.PI, 0.9 * Math.PI); c.strokeStyle = gt.shade; c.lineWidth = 1.3; c.stroke();
          }
        });
        // gold brow + crest
        celI(c, () => { c.moveTo(18, 52); c.quadraticCurveTo(48, 40, 78, 52); c.lineTo(78, 60); c.quadraticCurveTo(48, 48, 18, 60); c.closePath(); }, tone(GOLD, { light: 0.45 }), { band: 1.4, hi: 0.6 });
        celI(c, () => polyPath(c, [P(40, 16), P(46, 2), P(52, 14), P(58, 4), P(60, 18)]), tone(GOLD), { band: 1, hi: 0.4 });
        c.fillStyle = '#10140c';
        c.beginPath(); c.moveTo(26, 62); c.quadraticCurveTo(48, 54, 70, 62); c.lineTo(70, 66); c.quadraticCurveTo(48, 60, 26, 66); c.closePath(); c.fill();
        smallGem(c, 48, 47, 4, RUBY);
        break;
      }
    }
  };
  const over = kind === 'demon'
    ? (c: CanvasRenderingContext2D) => { glow(c, P(37, 53), 8, CRIMSON, 0.6); glow(c, P(59, 53), 8, CRIMSON, 0.6); }
    : (c: CanvasRenderingContext2D) => { if (kind !== 'cloth' && kind !== 'leather') sparkle(c, 32, 24, 4, '#ffffff', 0.85); };
  return { draw, opts: { over } };
}

// ════════════════════════════════════════════════════════════════════════
// Body armour
// ════════════════════════════════════════════════════════════════════════

export function torsoPath(c: CanvasRenderingContext2D): void {
  c.moveTo(34, 12);
  c.quadraticCurveTo(48, 22, 62, 12);
  c.lineTo(76, 18);
  c.quadraticCurveTo(80, 28, 78, 38);
  c.quadraticCurveTo(70, 42, 70, 50);
  c.lineTo(72, 86);
  c.quadraticCurveTo(48, 92, 24, 86);
  c.lineTo(26, 50);
  c.quadraticCurveTo(26, 42, 18, 38);
  c.quadraticCurveTo(16, 28, 20, 18);
  c.closePath();
}

function armorSpec(kind: ArmorKind): IconSpec {
  const main = { quilted: CLOTH, leather: LEATHER, chain: 0x9aa2b0, scale: 0xb58a4a, plate: STEEL, heavy: 0xb8c2cf, demon: 0x5a2230, dragon: DRAGON }[kind];
  const draw = (c: CanvasRenderingContext2D): void => {
    const t = tone(main, { light: kind === 'plate' || kind === 'heavy' ? 0.5 : 0.38 });
    celI(c, () => torsoPath(c), t, { band: 3.6, hi: 1.5 });
    clipTo(c, () => torsoPath(c), () => {
      switch (kind) {
        case 'quilted':
          c.strokeStyle = t.shade; c.lineWidth = 1.2;
          for (let k = -60; k < 120; k += 11) {
            c.beginPath(); c.moveTo(k, 0); c.lineTo(k + 96, 96); c.stroke();
            c.beginPath(); c.moveTo(k + 96, 0); c.lineTo(k, 96); c.stroke();
          }
          break;
        case 'chain':
          c.strokeStyle = t.shade; c.lineWidth = 1.1;
          for (let y = 16; y < 92; y += 5) for (let x = 14 + ((y / 5) % 2) * 3; x < 84; x += 6) {
            c.beginPath(); c.arc(x, y, 2.6, 0.15 * Math.PI, 0.85 * Math.PI); c.stroke();
          }
          break;
        case 'scale':
        case 'dragon':
          for (let y = 22; y < 92; y += 7) for (let x = 14 + ((y / 7) % 2) * 4.5; x < 86; x += 9) {
            c.beginPath(); c.arc(x, y, 4.8, 0, Math.PI); c.fillStyle = t.shade; c.fill();
            c.beginPath(); c.arc(x - 0.8, y - 0.8, 3.6, 0.1, Math.PI - 0.1); c.fillStyle = t.light; c.fill();
          }
          break;
        case 'leather':
          c.setLineDash([2.5, 2.5]);
          line(c, [P(48, 22), P(48, 88)], 1.2, t.light);
          c.setLineDash([]);
          break;
        default: break;
      }
      // Cool shadow down the right side
      c.fillStyle = t.shade;
      c.globalAlpha = 0.35;
      c.beginPath(); c.moveTo(60, 20); c.quadraticCurveTo(70, 50, 62, 92); c.lineTo(96, 92); c.lineTo(96, 0); c.closePath(); c.fill();
      c.globalAlpha = 1;
    });
    switch (kind) {
      case 'quilted': {
        celI(c, () => { c.moveTo(34, 12); c.quadraticCurveTo(48, 26, 62, 12); c.lineTo(58, 12); c.quadraticCurveTo(48, 20, 38, 12); c.closePath(); }, tone(0x8a6a3a), { band: 1, hi: 0.4 });
        celI(c, () => roundRectPath(c, 24, 70, 48, 8, 3), tone(DARK_LEATHER), { band: 1.4, hi: 0.6 });
        break;
      }
      case 'leather': {
        const st = tone(DARK_LEATHER);
        celI(c, () => ribbonPath(c, [P(22, 20), P(48, 50), P(70, 80)], 3.4), st, { band: 1.4, hi: 0.6 });
        celI(c, () => roundRectPath(c, 24, 66, 48, 8, 3), st, { band: 1.4, hi: 0.6 });
        celI(c, () => roundRectPath(c, 43, 64, 10, 12, 2), tone(BRONZE), { band: 1, hi: 0.4, stroke: 1 });
        for (const x of [20, 76]) celI(c, () => ellipsePath(c, P(x, 26), 8, 11, x < 48 ? 0.3 : -0.3), tone(mixHex(LEATHER, 0x000000, 0.1)), { band: 1.6, hi: 0.7 });
        break;
      }
      case 'chain':
        celI(c, () => { c.moveTo(32, 12); c.quadraticCurveTo(48, 28, 64, 12); c.lineTo(60, 10); c.quadraticCurveTo(48, 20, 36, 10); c.closePath(); }, tone(LEATHER), { band: 1, hi: 0.4 });
        celI(c, () => { c.moveTo(24, 82); c.quadraticCurveTo(48, 88, 72, 82); c.lineTo(72, 88); c.quadraticCurveTo(48, 94, 24, 88); c.closePath(); }, tone(LEATHER), { band: 1, hi: 0.4 });
        break;
      case 'scale':
        celI(c, () => roundRectPath(c, 24, 64, 48, 7, 3), tone(DARK_LEATHER), { band: 1.2, hi: 0.5 });
        rivet(c, 48, 67.5, 3, GOLD);
        break;
      case 'plate':
      case 'heavy': {
        const trim = kind === 'heavy' ? GOLD : IRON;
        // breastplate ridge
        line(c, [P(48, 22), P(48, 60)], 1.6, t.shade);
        streak(c, [P(46, 24), P(45, 58)], 1.2, 'rgba(255,255,255,0.6)');
        // faulds
        for (let i = 0; i < 3; i++) {
          const y = 64 + i * 8;
          celI(c, () => { c.moveTo(25, y); c.quadraticCurveTo(48, y + 5, 71, y); c.lineTo(71.5, y + 7); c.quadraticCurveTo(48, y + 12, 24.5, y + 7); c.closePath(); }, tone(mixHex(main, 0x606878, 0.2 + i * 0.1), { light: 0.45 }), { band: 1.4, hi: 0.6 });
        }
        // pauldrons
        const pr = kind === 'heavy' ? 14 : 12;
        for (const [x, rot] of [[20, 0.35], [76, -0.35]] as const) {
          celI(c, () => ellipsePath(c, P(x, 24), pr, pr * 0.8, rot), tone(main, { light: 0.5 }), { band: 2.4, hi: 1 });
          celI(c, () => { c.ellipse(x, 28, pr, pr * 0.62, rot, 0.1 * Math.PI, 0.9 * Math.PI); c.ellipse(x, 30, pr * 0.9, pr * 0.46, rot, 0.9 * Math.PI, 0.1 * Math.PI, true); c.closePath(); }, tone(trim), { band: 1, hi: 0.4, stroke: 1 });
        }
        celI(c, () => { c.moveTo(34, 12); c.quadraticCurveTo(48, 24, 62, 12); c.lineTo(60, 17); c.quadraticCurveTo(48, 28, 36, 17); c.closePath(); }, tone(trim), { band: 1, hi: 0.4, stroke: 1 });
        if (kind === 'heavy') smallGem(c, 48, 40, 4.2, SAPPHIRE);
        break;
      }
      case 'demon': {
        // bone ribs
        const bt = tone(BONE);
        for (let i = 0; i < 3; i++) {
          const y = 36 + i * 10;
          celI(c, () => ribbonPath(c, sample(10, (t) => P(30 + t * 36, y + Math.sin(t * Math.PI) * -5)), 2.2), bt, { band: 1, hi: 0.4, stroke: 1 });
        }
        for (const [x, s] of [[20, -1], [76, 1]] as const) {
          celI(c, () => ellipsePath(c, P(x, 24), 13, 10, s * 0.35), tone(0x3a1c28, { light: 0.35 }), { band: 2.2, hi: 0.9 });
          for (const k of [-1, 0, 1]) celI(c, () => polyPath(c, [P(x + k * 6 - 3, 18 + Math.abs(k) * 2), P(x + k * 6 + s * 4, 2 + Math.abs(k) * 4), P(x + k * 6 + 3, 18 + Math.abs(k) * 2)]), bt, { band: 1, hi: 0.4, stroke: 1 });
        }
        c.fillStyle = css(CRIMSON);
        c.beginPath(); c.arc(48, 24, 4, 0, Math.PI * 2); c.fill();
        break;
      }
      case 'dragon': {
        for (const [x, rot] of [[20, 0.35], [76, -0.35]] as const) {
          celI(c, () => ellipsePath(c, P(x, 24), 13, 10, rot), tone(GOLD, { light: 0.45 }), { band: 2.2, hi: 0.9 });
        }
        celI(c, () => { c.moveTo(34, 12); c.quadraticCurveTo(48, 24, 62, 12); c.lineTo(58, 36); c.lineTo(48, 42); c.lineTo(38, 36); c.closePath(); }, tone(GOLD, { light: 0.45 }), { band: 1.6, hi: 0.6 });
        smallGem(c, 48, 30, 5, RUBY);
        break;
      }
    }
  };
  const over = kind === 'demon'
    ? (c: CanvasRenderingContext2D) => glow(c, P(48, 24), 14, CRIMSON, 0.55)
    : kind === 'plate' || kind === 'heavy' || kind === 'dragon'
      ? (c: CanvasRenderingContext2D) => sparkle(c, 16, 18, 4.5, '#ffffff', 0.9)
      : undefined;
  return { draw, opts: { over } };
}

// ════════════════════════════════════════════════════════════════════════
// Gloves
// ════════════════════════════════════════════════════════════════════════

function glovesSpec(kind: LimbKind): IconSpec {
  const main = { leather: LEATHER, chain: 0x9aa2b0, iron: IRON, plate: STEEL, demon: 0x4a2436 }[kind];
  const metal = kind === 'iron' || kind === 'plate' || kind === 'demon';
  const draw = (c: CanvasRenderingContext2D): void => {
    c.save();
    c.translate(48, 50);
    c.rotate(-0.22);
    c.translate(-48, -50);
    const ft = tone(kind === 'chain' ? LEATHER : main, { light: 0.42 });
    // fingers (back to front)
    const fingers: [V, V][] = [[P(62, 44), P(72, 16)], [P(55, 42), P(60, 8)], [P(47, 42), P(47, 10)], [P(39, 44), P(35, 16)]];
    for (const [a, b] of fingers) {
      celI(c, () => capsulePath(c, a, b, 5.2, 4.4), ft, { band: 1.8, hi: 0.7 });
      if (metal) {
        for (const k of [0.35, 0.65]) {
          const p = P(a.x + (b.x - a.x) * k, a.y + (b.y - a.y) * k);
          line(c, [P(p.x - 4.5, p.y), P(p.x + 4.5, p.y)], 1.2, ft.shade);
        }
      }
      if (kind === 'demon') {
        celI(c, () => polyPath(c, [P(b.x - 3.5, b.y + 1), P(b.x + (b.x - a.x) * 0.25, b.y - 9), P(b.x + 3.5, b.y + 1)]), tone(BONE), { band: 0.8, hi: 0.3, stroke: 1 });
      }
    }
    // thumb
    celI(c, () => capsulePath(c, P(34, 60), P(18, 42), 6.2, 4.8), ft, { band: 1.8, hi: 0.7 });
    // back of hand
    const ht = tone(main, { light: 0.45 });
    const back = (): void => blobPath(c, [P(30, 40), P(50, 36), P(68, 40), P(70, 58), P(64, 70), P(36, 70), P(28, 58)]);
    celI(c, back, ht, { band: 3, hi: 1.3 });
    clipTo(c, back, () => {
      if (kind === 'chain') {
        c.strokeStyle = ht.shade; c.lineWidth = 1.1;
        for (let y = 38; y < 72; y += 5) for (let x = 28 + ((y / 5) % 2) * 3; x < 72; x += 6) { c.beginPath(); c.arc(x, y, 2.5, 0.15 * Math.PI, 0.85 * Math.PI); c.stroke(); }
      } else if (metal) {
        for (let y = 46; y < 70; y += 8) line(c, [P(26, y), P(72, y - 2)], 1.4, ht.shade);
        streak(c, [P(34, 44), P(56, 40)], 1.6, 'rgba(255,255,255,0.6)');
      } else {
        c.setLineDash([2.2, 2.2]);
        line(c, [P(32, 46), P(50, 42), P(66, 46)], 1.1, ht.light);
        c.setLineDash([]);
      }
    });
    if (kind === 'demon') rivet(c, 49, 52, 3.4, 0xa02838);
    // cuff
    const cuffC = kind === 'leather' ? DARK_LEATHER : kind === 'chain' ? LEATHER : kind === 'demon' ? 0x3a1c28 : main;
    const ct = tone(cuffC, { light: 0.45 });
    const flare = kind === 'plate' || kind === 'demon' ? 5 : 1;
    celI(c, () => { c.moveTo(34 - flare, 68); c.lineTo(68 + flare, 68); c.lineTo(66 + flare * 1.6, 90); c.lineTo(32 - flare * 1.6, 90); c.closePath(); }, ct, { band: 2.2, hi: 0.9 });
    celI(c, () => roundRectPath(c, 31 - flare, 66, 40 + flare * 2, 7, 3), tone(kind === 'plate' ? GOLD : mixHex(cuffC, 0x000000, 0.15)), { band: 1.2, hi: 0.5, stroke: 1 });
    if (kind === 'iron' || kind === 'plate') for (const x of [38, 50, 62]) rivet(c, x, 80, 1.8, kind === 'plate' ? GOLD : STEEL);
    if (kind === 'demon') for (const x of [34, 46, 58, 70]) celI(c, () => polyPath(c, [P(x - 3, 90), P(x, 97), P(x + 3, 90)]), tone(BONE), { band: 0.6, hi: 0.3, stroke: 0.9 });
    c.restore();
  };
  return { draw, opts: { over: metal && kind !== 'demon' ? (c) => sparkle(c, 30, 30, 4, '#ffffff', 0.85) : undefined } };
}

// ════════════════════════════════════════════════════════════════════════
// Boots
// ════════════════════════════════════════════════════════════════════════

function bootsSpec(kind: LimbKind): IconSpec {
  const main = { leather: LEATHER, chain: 0x9aa2b0, iron: LEATHER, plate: STEEL, demon: 0x4a2436 }[kind];
  const draw = (c: CanvasRenderingContext2D): void => {
    const t = tone(main, { light: 0.42 });
    // back boot (pair reads as "boots")
    c.save();
    c.translate(-12, -6);
    const bt = tone(mixHex(main, 0x201830, 0.35));
    celI(c, () => bootPath(c), bt, { band: 2, hi: 0.8, noLight: true });
    c.restore();
    celI(c, () => bootPath(c), t, { band: 3.2, hi: 1.4 });
    clipTo(c, () => bootPath(c), () => {
      if (kind === 'chain') {
        c.strokeStyle = t.shade; c.lineWidth = 1.1;
        for (let y = 20; y < 76; y += 5) for (let x = 30 + ((y / 5) % 2) * 3; x < 90; x += 6) { c.beginPath(); c.arc(x, y, 2.5, 0.15 * Math.PI, 0.85 * Math.PI); c.stroke(); }
      }
      if (kind === 'plate' || kind === 'demon') {
        for (const y of [30, 42, 54]) line(c, [P(30, y), P(66, y + 1)], 1.4, t.shade);
        for (const x of [66, 74, 82]) line(c, [P(x, 66), P(x - 2, 84)], 1.4, t.shade);
      }
      c.fillStyle = t.shade; c.globalAlpha = 0.35; c.fillRect(0, 76, 96, 20); c.globalAlpha = 1;
    });
    // sole
    celI(c, () => { c.moveTo(28, 84); c.lineTo(88, 84); c.quadraticCurveTo(92, 84, 91, 90); c.lineTo(28, 90); c.closePath(); }, tone(0x3a2a24), { band: 1, hi: 0.4, stroke: 1 });
    switch (kind) {
      case 'leather': {
        const ct = tone(mixHex(LEATHER, 0xffffff, 0.12));
        celI(c, () => { c.moveTo(24, 10); c.lineTo(70, 10); c.lineTo(68, 26); c.quadraticCurveTo(46, 30, 26, 26); c.closePath(); }, ct, { band: 1.8, hi: 0.7 });
        for (let y = 36; y < 62; y += 7) line(c, [P(58, y), P(66, y + 3)], 1.4, css(0xe8d8b0));
        break;
      }
      case 'chain':
        celI(c, () => roundRectPath(c, 26, 12, 42, 8, 3), tone(LEATHER), { band: 1.2, hi: 0.5 });
        break;
      case 'iron': {
        // greave over leather boot
        const gt = tone(STEEL, { light: 0.5 });
        celI(c, () => { c.moveTo(38, 8); c.quadraticCurveTo(52, 4, 68, 8); c.lineTo(68, 58); c.quadraticCurveTo(58, 70, 44, 64); c.closePath(); }, gt, { band: 2.2, hi: 0.9 });
        line(c, [P(55, 10), P(55, 62)], 1.3, gt.shade);
        streak(c, [P(44, 12), P(44, 56)], 1.6, 'rgba(255,255,255,0.6)');
        for (const y of [18, 48]) rivet(c, 62, y, 1.8, BRONZE);
        break;
      }
      case 'plate':
        celI(c, () => roundRectPath(c, 26, 10, 44, 8, 3), tone(GOLD), { band: 1.2, hi: 0.5 });
        celI(c, () => ellipsePath(c, P(44, 60), 8, 7), tone(STEEL, { light: 0.5 }), { band: 1.6, hi: 0.6 });
        break;
      case 'demon':
        celI(c, () => roundRectPath(c, 26, 10, 44, 8, 3), tone(0x3a1c28), { band: 1.2, hi: 0.5 });
        for (const [x, y] of [[28, 30], [28, 46], [70, 18]]) celI(c, () => polyPath(c, [P(x, y - 4), P(x - (x < 48 ? 10 : -10), y - 7), P(x, y + 4)]), tone(BONE), { band: 0.8, hi: 0.3, stroke: 1 });
        celI(c, () => polyPath(c, [P(86, 76), P(96, 82), P(88, 86)]), tone(BONE), { band: 0.8, hi: 0.3, stroke: 1 });
        break;
    }
  };
  return { draw, opts: { over: kind === 'plate' || kind === 'iron' ? (c) => sparkle(c, 42, 16, 4, '#ffffff', 0.85) : undefined } };
}

export function bootPath(c: CanvasRenderingContext2D): void {
  c.moveTo(26, 10);
  c.lineTo(68, 10);
  c.lineTo(66, 58);
  c.quadraticCurveTo(70, 64, 82, 68);
  c.quadraticCurveTo(92, 72, 90, 84);
  c.lineTo(28, 84);
  c.quadraticCurveTo(26, 60, 30, 40);
  c.closePath();
}

// ════════════════════════════════════════════════════════════════════════
// Belts
// ════════════════════════════════════════════════════════════════════════

function beltSpec(kind: BeltKind): IconSpec {
  const wide = kind === 'heavy' || kind === 'war' || kind === 'plated' ? 1.35 : 1;
  const leather = kind === 'plated' ? 0x4a3a34 : kind === 'war' ? 0x6a2a24 : LEATHER;
  const draw = (c: CanvasRenderingContext2D): void => {
    const lt = tone(leather, { light: 0.35 });
    const cx = 48;
    const cy = 50;
    const rx = 38;
    const ry = 22;
    const bw = 7 * wide;
    // back half (inside of belt, darker)
    const back = (): void => { c.ellipse(cx, cy - bw / 2, rx, ry, 0, Math.PI, Math.PI * 2); c.ellipse(cx, cy + bw / 2, rx, ry, 0, Math.PI * 2, Math.PI, true); c.closePath(); };
    celI(c, back, tone(mixHex(leather, 0x100818, 0.45)), { band: 1.2, hi: 0.5, noLight: true });
    // front half
    const front = (): void => { c.ellipse(cx, cy - bw / 2, rx, ry, 0, 0, Math.PI); c.ellipse(cx, cy + bw / 2, rx, ry, 0, Math.PI, 0, true); c.closePath(); };
    celI(c, front, lt, { band: 2, hi: 0.9 });
    clipTo(c, front, () => {
      c.setLineDash([2.4, 2.4]);
      c.beginPath(); c.ellipse(cx, cy - bw / 2 + 2, rx - 1, ry, 0, 0.1, Math.PI - 0.1); c.strokeStyle = lt.light; c.lineWidth = 1; c.stroke();
      c.beginPath(); c.ellipse(cx, cy + bw / 2 - 2, rx - 1, ry, 0, 0.1, Math.PI - 0.1); c.stroke();
      c.setLineDash([]);
    });
    // studs / plates along the front
    const around = (a: number): V => P(cx + Math.cos(a) * rx, cy + Math.sin(a) * ry);
    if (kind === 'war') for (const a of [0.35, 0.8, 2.3, 2.75]) { const p = around(a); rivet(c, p.x, p.y, 2.6, STEEL); }
    if (kind === 'plated') {
      for (const a of [0.45, 0.95, 2.2, 2.7]) {
        const p = around(a);
        celI(c, () => roundRectPath(c, p.x - 5, p.y - bw / 2 - 1, 10, bw + 2, 2), tone(STEEL, { light: 0.5 }), { band: 1.2, hi: 0.5, stroke: 1 });
      }
    }
    // pouch for heavy belts
    if (kind === 'heavy' || kind === 'war') {
      const pt = tone(mixHex(leather, 0xffffff, 0.1));
      celI(c, () => roundRectPath(c, 66, 56, 16, 18, 4), pt, { band: 1.6, hi: 0.7 });
      celI(c, () => { c.moveTo(66, 60); c.quadraticCurveTo(74, 66, 82, 60); c.lineTo(82, 58); c.quadraticCurveTo(74, 54, 66, 58); c.closePath(); }, tone(mixHex(leather, 0x000000, 0.2)), { band: 1, hi: 0.4, stroke: 1 });
    }
    // strap tail
    celI(c, () => { c.moveTo(56, cy + ry - bw / 2 + 1); c.lineTo(70, cy + ry - bw / 2 + 8); c.lineTo(68, cy + ry + bw / 2 + 9); c.lineTo(56, cy + ry + bw / 2 + 1); c.closePath(); }, lt, { band: 1.2, hi: 0.5, stroke: 1 });
    // buckle
    const bk = tone(kind === 'plated' ? STEEL : GOLD, { light: 0.5 });
    const by = cy + ry;
    celI(c, () => { roundRectPath(c, cx - 10, by - bw / 2 - 5, 20, bw + 10, 3); roundRectPath(c, cx - 5.5, by - bw / 2 - 1, 11, bw + 2, 1.5); }, bk, { band: 1.4, hi: 0.6 });
    // evenodd hole: repaint the leather inside the buckle frame
    celI(c, () => roundRectPath(c, cx - 5.5, by - bw / 2 - 1, 11, bw + 2, 1.5), lt, { band: 1, hi: 0.4, stroke: 1 });
    line(c, [P(cx, by - bw / 2 - 1), P(cx, by + bw / 2 + 1)], 1.8, bk.light);
  };
  return { draw, opts: { over: (c) => sparkle(c, 42, 66, 4, '#ffffff', 0.9) } };
}

// ════════════════════════════════════════════════════════════════════════
// Jewellery
// ════════════════════════════════════════════════════════════════════════

function ringSpec(st: { band: number; gem?: number }): IconSpec {
  const draw = (c: CanvasRenderingContext2D): void => {
    const bt = tone(st.band, { light: 0.55 });
    const cx = 48;
    const cy = 58;
    const ringPath = (): void => { c.ellipse(cx, cy, 28, 22, 0, 0, Math.PI * 2); c.ellipse(cx, cy + 2, 19, 13, 0, 0, Math.PI * 2, true); };
    // thickness
    celI(c, () => { c.ellipse(cx + 1.5, cy + 4, 28, 22, 0, 0, Math.PI * 2); c.ellipse(cx + 1.5, cy + 6, 19, 13, 0, 0, Math.PI * 2, true); }, tone(mixHex(st.band, 0x301820, 0.45)), { band: 1, hi: 0.4, noLight: true });
    celI(c, ringPath, bt, { band: 2.6, hi: 1.2 });
    clipTo(c, ringPath, () => {
      streak(c, [P(24, 60), P(28, 44), P(44, 37)], 2.2, 'rgba(255,255,255,0.8)');
      streak(c, [P(60, 78), P(72, 70)], 1.6, 'rgba(255,255,255,0.45)');
    });
    if (st.gem) {
      // setting with prongs
      const pt = tone(st.band, { light: 0.5 });
      celI(c, () => { c.moveTo(34, 38); c.lineTo(62, 38); c.lineTo(56, 48); c.lineTo(40, 48); c.closePath(); }, pt, { band: 1.4, hi: 0.6 });
      facetGem(c, 48, 28, gemCut('round', 13), st.gem, { sparkles: 2 });
      for (const [x, y] of [[36, 20], [60, 20], [36, 36], [60, 36]]) celI(c, () => c.arc(x, y, 2.4, 0, Math.PI * 2), pt, { band: 0.8, hi: 0.3, stroke: 1 });
    } else {
      // engraved band
      clipTo(c, ringPath, () => { for (let a = 0.2; a < Math.PI - 0.2; a += 0.35) { const x = cx + Math.cos(a) * 24; const y = cy + Math.sin(a) * 18; line(c, [P(x - 1.5, y - 2), P(x + 1.5, y + 2)], 1, bt.shade); } });
    }
  };
  return { draw, opts: { over: (c) => { if (st.gem) glow(c, P(48, 28), 18, st.gem, 0.3); sparkle(c, 70, 44, 5, '#ffffff', 0.9); } } };
}

function amuletSpec(kind: AmuletKind): IconSpec {
  const chainC = kind === 'bone' ? 0x8a6a48 : GOLD;
  const draw = (c: CanvasRenderingContext2D): void => {
    // chain links along a U
    const ct = tone(chainC, { light: 0.45 });
    const chainPts = sample(22, (t) => { const a = Math.PI * (1 - t); return P(48 + Math.cos(a) * 32, 10 + Math.sin(a) * 34 * (t < 0.5 ? 1 : 1)); });
    if (kind === 'bone') {
      celI(c, () => ribbonPath(c, chainPts, 1.5), ct, { band: 0.8, hi: 0.4, stroke: 1 });
      for (const i of [5, 17]) celI(c, () => c.arc(chainPts[i].x, chainPts[i].y, 3, 0, Math.PI * 2), tone(0xb04a3a), { band: 1, hi: 0.4, stroke: 1 });
    } else {
      chainPts.forEach((p, i) => {
        const q = chainPts[Math.min(chainPts.length - 1, i + 1)];
        const ang = Math.atan2(q.y - p.y, q.x - p.x);
        celI(c, () => ellipsePath(c, p, i % 2 === 0 ? 3.2 : 2.6, i % 2 === 0 ? 1.8 : 1.3, ang), ct, { band: 0.8, hi: 0.4, stroke: 0.9 });
      });
    }
    const px = 48;
    const py = 62;
    switch (kind) {
      case 'bone': {
        const bt = tone(BONE);
        celI(c, () => { c.moveTo(40, 44); c.quadraticCurveTo(48, 40, 56, 44); c.quadraticCurveTo(58, 66, 48, 90); c.quadraticCurveTo(40, 70, 40, 44); c.closePath(); }, bt, { band: 2.2, hi: 0.9 });
        line(c, [P(44, 50), P(46, 72)], 1.2, bt.light);
        celI(c, () => roundRectPath(c, 38, 40, 20, 7, 3), tone(DARK_LEATHER), { band: 1, hi: 0.4, stroke: 1 });
        break;
      }
      case 'jade': {
        celI(c, () => roundRectPath(c, 42, 40, 12, 8, 3), tone(GOLD), { band: 1, hi: 0.4, stroke: 1 });
        celI(c, () => ellipsePath(c, P(px, py + 4), 20, 20), tone(GOLD, { light: 0.5 }), { band: 2, hi: 0.8 });
        facetGem(c, px, py + 4, gemCut('oval', 16).map((p) => P(p.x * 1.1, p.y)), 0x2fb870, { table: 0.6, sparkles: 1 });
        break;
      }
      case 'arcane': {
        celI(c, () => roundRectPath(c, 42, 38, 12, 8, 3), tone(GOLD), { band: 1, hi: 0.4, stroke: 1 });
        celI(c, () => starPath(c, px, py + 4, 8, 24, 13, -Math.PI / 2), tone(GOLD, { light: 0.5 }), { band: 2, hi: 0.8 });
        celI(c, () => c.arc(px, py + 4, 13, 0, Math.PI * 2), tone(0x2a2248), { band: 1.2, hi: 0.5 });
        orb(c, px, py + 4, 10, 0x5a8cff);
        break;
      }
    }
  };
  return {
    draw,
    opts: {
      over: (c) => {
        if (kind === 'arcane') glow(c, P(48, 66), 26, 0x5a8cff, 0.4);
        if (kind === 'jade') glow(c, P(48, 66), 18, 0x2fb870, 0.25);
        sparkle(c, 64, 52, 4.5, '#ffffff', 0.9);
      },
    },
  };
}

// ════════════════════════════════════════════════════════════════════════
// Gems
// ════════════════════════════════════════════════════════════════════════

function gemSpec(color: number, cut: 'oval' | 'round' | 'emerald' | 'pear' | 'trillion', tier: number): IconSpec {
  const r = [0, 22, 27, 31, 32, 33][Math.max(1, Math.min(5, tier))];
  let pts = gemCut(cut, r);
  if (tier === 1) {
    // Chipped: knock a corner off and roughen the girdle
    pts = pts.map((p, i) => (i % 3 === 1 ? P(p.x * 0.86, p.y * 0.9) : p));
    pts.splice(Math.floor(pts.length * 0.3), 1);
  }
  const cx = 48;
  const cy = cut === 'pear' ? 50 : 49;
  return {
    draw: (c) => facetGem(c, cx, cy, pts, color, { sparkles: tier >= 2 ? 2 : 1, stroke: 1.6 }),
    opts: {
      inkWidth: 2.2,
      under: tier >= 3 ? (c) => glow(c, P(cx, cy), r * (tier >= 4 ? 1.6 : 1.35), color, tier >= 4 ? 0.5 : 0.35) : undefined,
      over: (c) => {
        if (tier >= 2) sparkle(c, cx + r * 0.62, cy - r * 0.7, tier >= 3 ? 6 : 4.5, '#ffffff', 0.95);
        if (tier >= 4) sparkle(c, cx - r * 0.85, cy + r * 0.6, 4.5, '#ffffff', 0.85);
        if (tier >= 5) {
          c.save();
          c.globalCompositeOperation = 'lighter';
          c.strokeStyle = css(mixHex(color, 0xffffff, 0.3), 0.6);
          c.lineWidth = 1.4;
          for (let i = 0; i < 8; i++) {
            const a = (i / 8) * Math.PI * 2 + 0.2;
            c.beginPath();
            c.moveTo(cx + Math.cos(a) * (r + 4), cy + Math.sin(a) * (r + 4));
            c.lineTo(cx + Math.cos(a) * (r + 11), cy + Math.sin(a) * (r + 11));
            c.stroke();
          }
          c.restore();
        }
      },
    },
  };
}

// ════════════════════════════════════════════════════════════════════════
// Potions
// ════════════════════════════════════════════════════════════════════════

function potionSpec(color: number, size: 's' | 'm' | 'l', shape: 'round' | 'square'): IconSpec {
  const g = { s: { r: 17, cy: 62, neck: 7, neckH: 18 }, m: { r: 23, cy: 60, neck: 7.5, neckH: 14 }, l: { r: 28, cy: 60, neck: 8.5, neckH: 12 } }[size];
  const cx = 48;
  const neckTop = g.cy - g.r - g.neckH + 4;
  const body = (): void => {
    if (shape === 'square') {
      c0.moveTo(cx - g.neck, neckTop);
      c0.lineTo(cx + g.neck, neckTop);
      c0.lineTo(cx + g.neck, g.cy - g.r + 2);
      c0.lineTo(cx + g.r, g.cy - g.r + 10);
      c0.lineTo(cx + g.r, g.cy + g.r - 2);
      c0.quadraticCurveTo(cx + g.r, g.cy + g.r + 4, cx + g.r - 6, g.cy + g.r + 4);
      c0.lineTo(cx - g.r + 6, g.cy + g.r + 4);
      c0.quadraticCurveTo(cx - g.r, g.cy + g.r + 4, cx - g.r, g.cy + g.r - 2);
      c0.lineTo(cx - g.r, g.cy - g.r + 10);
      c0.lineTo(cx - g.neck, g.cy - g.r + 2);
      c0.closePath();
    } else if (size === 's') {
      // slim vial
      c0.moveTo(cx - g.neck, neckTop);
      c0.lineTo(cx + g.neck, neckTop);
      c0.lineTo(cx + g.neck, g.cy - g.r);
      c0.bezierCurveTo(cx + g.r + 4, g.cy - g.r + 8, cx + g.r, g.cy + g.r + 6, cx, g.cy + g.r + 6);
      c0.bezierCurveTo(cx - g.r, g.cy + g.r + 6, cx - g.r - 4, g.cy - g.r + 8, cx - g.neck, g.cy - g.r);
      c0.closePath();
    } else {
      const a = Math.asin(g.neck / g.r);
      c0.moveTo(cx - g.neck, neckTop);
      c0.lineTo(cx + g.neck, neckTop);
      c0.lineTo(cx + g.neck, g.cy - Math.cos(a) * g.r);
      c0.arc(cx, g.cy, g.r, -Math.PI / 2 + a, Math.PI * 1.5 - a);
      c0.closePath();
    }
  };
  let c0: CanvasRenderingContext2D;
  const draw = (c: CanvasRenderingContext2D): void => {
    c0 = c;
    const lt = tone(color, { light: 0.4, shadow: 0.35 });
    const level = g.cy - g.r * 0.35;
    // glass back
    c.beginPath(); body(); c.fillStyle = 'rgba(200,225,245,0.35)'; c.fill();
    // liquid
    clipTo(c, body, () => {
      const lg = c.createLinearGradient(cx - g.r, level, cx + g.r, g.cy + g.r);
      lg.addColorStop(0, lt.light);
      lg.addColorStop(0.45, lt.base);
      lg.addColorStop(1, lt.shade);
      c.fillStyle = lg;
      c.fillRect(0, level, 96, 96);
      // meniscus
      c.beginPath(); c.ellipse(cx, level, g.r, 3.4, 0, 0, Math.PI * 2);
      c.fillStyle = css(mixHex(color, 0xffffff, 0.45));
      c.fill();
      // inner glow core
      const rg = c.createRadialGradient(cx - 3, g.cy + 2, 1, cx, g.cy + 4, g.r);
      rg.addColorStop(0, css(mixHex(color, 0xffffff, 0.55), 0.8));
      rg.addColorStop(1, css(color, 0));
      c.fillStyle = rg;
      c.fillRect(0, level + 3, 96, 96);
      // bubbles
      c.strokeStyle = 'rgba(255,255,255,0.75)';
      c.lineWidth = 1;
      for (const [x, y, r] of [[cx + g.r * 0.35, g.cy + g.r * 0.2, 2.4], [cx + g.r * 0.15, g.cy - g.r * 0.05, 1.6], [cx - g.r * 0.2, g.cy + g.r * 0.45, 1.9]] as const) {
        c.beginPath(); c.arc(x, y, r, 0, Math.PI * 2); c.stroke();
      }
      // cool shade on the right edge of the glass
      c.fillStyle = 'rgba(20,10,50,0.28)';
      c.beginPath(); c.arc(cx + g.r * 0.55, g.cy + 2, g.r * 0.95, -Math.PI * 0.4, Math.PI * 0.6); c.lineTo(96, 96); c.lineTo(96, 0); c.closePath(); c.fill();
    });
    // glass outline
    c.beginPath(); body(); c.strokeStyle = css(mixHex(color, 0x1a1030, 0.6)); c.lineWidth = 1.8; c.lineJoin = 'round'; c.stroke();
    // highlight streaks
    streak(c, [P(cx - g.r * 0.7, g.cy + g.r * 0.2), P(cx - g.r * 0.75, g.cy - g.r * 0.45), P(cx - g.r * 0.3, g.cy - g.r * 0.8)], 3, 'rgba(255,255,255,0.85)');
    c.beginPath(); c.arc(cx - g.r * 0.62, g.cy + g.r * 0.45, 1.8, 0, Math.PI * 2); c.fillStyle = 'rgba(255,255,255,0.8)'; c.fill();
    streak(c, [P(cx - g.neck + 2.4, neckTop + 4), P(cx - g.neck + 2.4, g.cy - g.r + 2)], 1.6, 'rgba(255,255,255,0.7)');
    // neck lip
    const nt = tone(0xcfe4f2, { light: 0.4 });
    celI(c, () => roundRectPath(c, cx - g.neck - 2.5, neckTop - 1, (g.neck + 2.5) * 2, 5, 2), nt, { band: 1, hi: 0.4, stroke: 1.1 });
    // cork
    const ct = tone(0xb07a48);
    celI(c, () => roundRectPath(c, cx - g.neck + 1, neckTop - 11, (g.neck - 1) * 2, 11, 2.5), ct, { band: 1.6, hi: 0.6 });
    line(c, [P(cx - g.neck + 3, neckTop - 7), P(cx + g.neck - 3, neckTop - 6)], 0.9, ct.shade);
    if (size === 'l') {
      // gold collar + tag cord
      celI(c, () => roundRectPath(c, cx - g.neck - 1.5, neckTop + 7, (g.neck + 1.5) * 2, 5, 2), tone(GOLD), { band: 1, hi: 0.4, stroke: 1 });
    }
    if (shape === 'square') {
      // label with a leaf glyph
      const lb = tone(0xeee2c0);
      celI(c, () => roundRectPath(c, cx - 11, g.cy - 4, 22, 16, 2), lb, { band: 1.2, hi: 0.5, stroke: 1.1 });
      c.save(); c.translate(cx, g.cy + 4); c.rotate(-0.6);
      c.beginPath(); blobPath(c, [P(0, -6), P(3.5, 0), P(0, 6), P(-3.5, 0)]); c.fillStyle = css(0x3a9a3a); c.fill();
      c.restore();
    }
  };
  return {
    draw,
    opts: {
      under: (c) => glow(c, P(cx, g.cy + 2), g.r * 1.45, color, 0.28),
      over: (c) => sparkle(c, cx + g.r * 0.55, g.cy - g.r * 0.55, 4.5, '#ffffff', 0.9),
    },
  };
}

// ════════════════════════════════════════════════════════════════════════
// Scroll
// ════════════════════════════════════════════════════════════════════════

function scrollSpec(kind: 'tp' | 'id'): IconSpec {
  const accent = kind === 'tp' ? 0x3e8cff : 0xd8a030;
  const seal = kind === 'tp' ? 0x2f5ac0 : 0xc0282e;
  const draw = (c: CanvasRenderingContext2D): void => {
    c.save();
    c.translate(48, 48);
    c.rotate(-0.5);
    c.translate(-48, -48);
    const pt = tone(0xf0dcaa, { light: 0.35 });
    // sheet
    const sheet = (): void => { c.moveTo(22, 26); c.lineTo(74, 26); c.quadraticCurveTo(70, 48, 74, 70); c.lineTo(22, 70); c.quadraticCurveTo(26, 48, 22, 26); c.closePath(); };
    celI(c, sheet, pt, { band: 2.6, hi: 1.1 });
    clipTo(c, sheet, () => {
      // glyph
      c.save();
      c.strokeStyle = css(accent);
      c.lineWidth = 2.2;
      c.lineCap = 'round';
      if (kind === 'tp') {
        c.beginPath();
        for (let i = 0; i <= 40; i++) {
          const a = i * 0.42;
          const r = 1 + i * 0.36;
          const x = 48 + Math.cos(a) * r;
          const y = 48 + Math.sin(a) * r * 0.9;
          if (i === 0) c.moveTo(x, y);
          else c.lineTo(x, y);
        }
        c.stroke();
      } else {
        c.beginPath(); c.ellipse(48, 48, 13, 8, 0, 0, Math.PI * 2); c.stroke();
        c.fillStyle = css(accent);
        c.beginPath(); c.arc(48, 48, 4.4, 0, Math.PI * 2); c.fill();
      }
      c.restore();
      for (const y of [34, 62]) line(c, [P(30, y), P(66, y)], 1, pt.shade);
    });
    // rolls
    const rt = tone(0xe2c890, { light: 0.4 });
    for (const y of [26, 70]) {
      celI(c, () => capsulePath(c, P(18, y), P(78, y), 5.4, 5.4), rt, { band: 1.8, hi: 0.7 });
      celI(c, () => ellipsePath(c, P(78, y), 3, 5.2), tone(0xc8a86a), { band: 0.8, hi: 0.3, stroke: 1 });
      c.beginPath(); c.ellipse(78, y, 1.2, 2.4, 0, 0, Math.PI * 2); c.fillStyle = rt.shade; c.fill();
    }
    // ribbon + wax seal
    const rb = tone(accent);
    celI(c, () => polyPath(c, [P(56, 68), P(62, 88), P(58, 86), P(55, 90), P(52, 70)]), rb, { band: 1, hi: 0.4, stroke: 1 });
    celI(c, () => polyPath(c, [P(62, 68), P(72, 86), P(68, 85), P(66, 89), P(58, 70)]), rb, { band: 1, hi: 0.4, stroke: 1 });
    celI(c, () => blobPath(c, [P(52, 64), P(60, 60), P(68, 64), P(69, 72), P(62, 78), P(53, 75)]), tone(seal, { light: 0.4 }), { band: 1.6, hi: 0.6 });
    c.beginPath(); c.arc(60.5, 69, 3.6, 0, Math.PI * 2); c.strokeStyle = tone(seal).shade; c.lineWidth = 1.2; c.stroke();
    c.restore();
  };
  return { draw, opts: { over: kind === 'tp' ? (c) => glow(c, P(48, 48), 16, accent, 0.3) : undefined } };
}

// ════════════════════════════════════════════════════════════════════════
// Materials
// ════════════════════════════════════════════════════════════════════════

function drawOre(c: CanvasRenderingContext2D): void {
  const rt = tone(0x6a6070);
  const rock = (): void => blobPath(c, [P(18, 64), P(28, 36), P(50, 24), P(74, 34), P(82, 60), P(66, 80), P(34, 82)]);
  celI(c, rock, rt, { band: 4, hi: 1.6 });
  clipTo(c, rock, () => {
    line(c, [P(36, 40), P(46, 56), P(40, 74)], 1.4, rt.shade);
    line(c, [P(60, 34), P(58, 50), P(72, 62)], 1.4, rt.shade);
  });
  for (const [x, y, r] of [[48, 50, 8], [64, 64, 6], [32, 60, 5]] as const) {
    facetGem(c, x, y, gemCut('trillion', r), 0xff9a3a, { sparkles: r > 6 ? 1 : 0, stroke: 1 });
  }
}

/** 铁屑 — a bent iron offcut, a hex nut and a curled shaving. */
function drawScrap(c: CanvasRenderingContext2D): void {
  const plate = tone(IRON);
  const shard = (): void => polyPath(c, [P(10, 66), P(38, 46), P(64, 56), P(68, 72), P(54, 88), P(16, 88)]);
  celI(c, shard, plate, { band: 4, hi: 1.5 });
  clipTo(c, shard, () => {
    line(c, [P(22, 78), P(44, 62)], 1.2, plate.shade);
    for (const [x, y] of [[26, 72], [46, 76]] as const) {
      c.beginPath(); c.arc(x, y, 2.4, 0, Math.PI * 2); c.fillStyle = plate.shade; c.fill();
    }
  });
  // hex nut
  const nt = tone(STEEL);
  const hex = sample(6, (t) => { const a = t * Math.PI * 2 * (5 / 6) + Math.PI / 6; return P(64 + Math.cos(a) * 19, 40 + Math.sin(a) * 19); });
  celI(c, () => polyPath(c, hex), nt, { band: 4, hi: 1.6 });
  c.beginPath(); c.arc(64, 40, 7.5, 0, Math.PI * 2); c.fillStyle = css(0x2a2530); c.fill();
  c.lineWidth = 1.4; c.strokeStyle = nt.line; c.stroke();
  // curled shaving
  const curl = sample(26, (t) => { const a = -0.4 + t * 5; const r = 17 - t * 10; return P(28 + Math.cos(a) * r, 34 + Math.sin(a) * r); });
  line(c, curl, 6, css(0x3a3f4a));
  line(c, curl, 3.2, css(mixHex(SILVER, 0xffffff, 0.2)));
  // loose chips
  for (const [x, y, r] of [[78, 72, 4], [70, 84, 3], [84, 62, 2.6]] as const) {
    celI(c, () => polyPath(c, [P(x - r, y), P(x, y - r), P(x + r, y + r * 0.4), P(x - r * 0.3, y + r)]), tone(DARK_IRON), { band: 1.5, stroke: 1 });
  }
}

/** 魔尘 — a heap of glowing blue powder. */
function drawDust(c: CanvasRenderingContext2D): void {
  const dt = tone(0x4f78f0, { light: 0.5, shadow: 0.35 });
  const heap = (): void => blobPath(c, [P(10, 84), P(22, 66), P(38, 50), P(52, 46), P(66, 56), P(80, 70), P(88, 84), P(48, 90)]);
  celI(c, heap, dt, { band: 6, hi: 1.8 });
  clipTo(c, heap, () => {
    for (let i = 0; i < 26; i++) {
      const x = 16 + ((i * 37) % 68);
      const y = 52 + ((i * 23) % 34);
      c.beginPath(); c.arc(x, y, 1.1 + (i % 3) * 0.5, 0, Math.PI * 2);
      c.fillStyle = i % 4 === 0 ? 'rgba(255,255,255,0.9)' : css(0xa8c4ff, 0.8); c.fill();
    }
  });
}

function dustSparkles(c: CanvasRenderingContext2D): void {
  sparkle(c, 50, 36, 6, '#e8f0ff');
  sparkle(c, 30, 48, 3.5, '#bcd2ff');
  sparkle(c, 72, 44, 4, '#d6e2ff', 0.9);
  for (const [x, y] of [[40, 28], [62, 26], [56, 16]] as const) {
    c.beginPath(); c.arc(x, y, 1.4, 0, Math.PI * 2); c.fillStyle = 'rgba(190,210,255,0.85)'; c.fill();
  }
}

/** 稀有精华 — a floating golden teardrop crystal. */
function drawEssence(c: CanvasRenderingContext2D): void {
  const drop = gemCut('pear', 26);
  facetGem(c, 48, 50, drop, 0xffbf2e, { sparkles: 2, stroke: 1.4, table: 0.5 });
  // small orbiting shards
  facetGem(c, 22, 72, gemCut('trillion', 7), 0xffd35a, { stroke: 1 });
  facetGem(c, 76, 30, gemCut('trillion', 5.5), 0xffd35a, { stroke: 1 });
}

/** 灵脉果: a glowing teal fruit with a leaf and a ley-vein seam (pet food). */
function drawLeyFruit(c: CanvasRenderingContext2D): void {
  celI(c, () => { c.moveTo(48, 30); c.quadraticCurveTo(50, 20, 56, 14); c.lineTo(59, 17); c.quadraticCurveTo(54, 22, 53, 31); c.closePath(); }, tone(0x6b4428));
  celI(c, () => { c.moveTo(54, 22); c.bezierCurveTo(62, 10, 78, 12, 82, 20); c.bezierCurveTo(74, 28, 62, 28, 54, 22); c.closePath(); }, tone(0x3fae5c, { light: 0.4 }), { band: 2, hi: 0.9 });
  celI(c, () => ellipsePath(c, P(48, 58), 27, 28), tone(0x3fd0b0, { light: 0.45 }), { band: 3.4, hi: 1.4 });
  line(c, [P(40, 36), P(36, 52), P(42, 70), P(50, 82)], 2.2, css(0xc8fff0));
  line(c, [P(58, 38), P(62, 56), P(56, 76)], 1.6, css(0x9ff6e0));
}

function leyFruitSparkles(c: CanvasRenderingContext2D): void {
  sparkle(c, 30, 40, 5, '#e8fff8');
  sparkle(c, 70, 72, 4, '#bff8ea', 0.9);
}

function essenceSparkles(c: CanvasRenderingContext2D): void {
  sparkle(c, 36, 30, 6, '#fff6d8');
  sparkle(c, 66, 70, 4.5, '#ffe7a0', 0.9);
}

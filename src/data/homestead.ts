/**
 * 余烬之塔 (Ember Tower) — homestead data: the tower's wings, what they cost,
 * which chapter brings their ally, what the herb garden grows, caravan
 * expeditions and altar blessings. Pure data + pure helpers (no Phaser), so
 * HomesteadSystem and its tests share one source.
 *
 * Player-facing text is i18n: `data.homestead.<id>.name/.desc`,
 * `homestead.blessing.<id>.name/.desc`, `homestead.expedition.<id>` (see
 * src/i18n/locales/homestead.ts).
 */
import type { EquipStats } from '../systems/CombatSystem';
import type { HomesteadBuilding } from './types';
import { GEM_STAT_MAP, getItemBase } from './items/bases';

/** Zone id of the tower (not part of the zone progression). */
export const TOWER_ZONE_ID = 'ember_tower';
/** The tower opens once the Ch.1 quest 篝火营地 is turned in. */
export const TOWER_UNLOCK_QUEST = 'q_explore_goblin_camp';
/** Pet food grown in the herb garden (item defined by the pet system). */
export const LEY_FRUIT_ID = 'c_ley_fruit';

/**
 * The tower's wings. `unlockQuest` is the main quest whose turn-in sends that
 * chapter's ally to the tower (the chapter finale; the altar opens with the
 * Ch.5 quest 以渊为引). Upgrades cost gold + embers.
 */
export const BUILDINGS: HomesteadBuilding[] = [
  {
    id: 'herb_garden', name: '药草园', description: '随击杀产出药水与灵脉果，回塔收获',
    maxLevel: 5,
    costPerLevel: [{ gold: 100 }, { gold: 250, embers: 5 }, { gold: 500, embers: 10 }, { gold: 1000, embers: 20 }, { gold: 2000, embers: 35 }],
    bonusPerLevel: [{ stat: 'potionDiscount', value: 5 }],
    unlockQuest: 'q_secure_plains', allyNpc: 'tower_herbalist',
  },
  {
    id: 'pet_house', name: '月井', description: '未出战的灵兽在此修养，提升灵兽经验',
    maxLevel: 5,
    costPerLevel: [{ gold: 300 }, { gold: 600, embers: 8 }, { gold: 1200, embers: 15 }, { gold: 2000, embers: 25 }, { gold: 3500, embers: 40 }],
    bonusPerLevel: [{ stat: 'petExpBonus', value: 10 }],
    unlockQuest: 'q_seal_dark_source', allyNpc: 'tower_hermit',
  },
  {
    id: 'gem_workshop', name: '炉台', description: '3 颗同阶宝石合成 1 颗高阶宝石',
    maxLevel: 5,
    costPerLevel: [{ gold: 200 }, { gold: 450, embers: 8 }, { gold: 900, embers: 15 }, { gold: 1800, embers: 25 }, { gold: 3500, embers: 40 }],
    bonusPerLevel: [{ stat: 'gemBonus', value: 2 }],
    unlockQuest: 'q_kill_stone_guardian', allyNpc: 'tower_dwarf',
  },
  {
    id: 'training_ground', name: '商队驿站', description: '佣兵经验加成，可派灵兽远行',
    maxLevel: 5,
    costPerLevel: [{ gold: 150 }, { gold: 350, embers: 6 }, { gold: 700, embers: 12 }, { gold: 1400, embers: 20 }, { gold: 2800, embers: 30 }],
    bonusPerLevel: [{ stat: 'mercExpBonus', value: 5 }],
    unlockQuest: 'q_seal_fire_rift', allyNpc: 'tower_nomad',
  },
  {
    id: 'altar', name: '心焰祭坛', description: '献祭余烬，换取限时祝福',
    maxLevel: 3,
    costPerLevel: [{ gold: 500, embers: 10 }, { gold: 1500, embers: 30 }, { gold: 4000, embers: 60 }],
    bonusPerLevel: [{ stat: 'altarBonus', value: 3 }],
    unlockQuest: 'q_collect_demon_essence', allyNpc: 'tower_warden',
  },
  {
    id: 'warehouse', name: '仓库', description: '扩展仓库格子',
    maxLevel: 5,
    costPerLevel: [{ gold: 100 }, { gold: 200 }, { gold: 400, embers: 5 }, { gold: 800, embers: 10 }, { gold: 1600, embers: 20 }],
    bonusPerLevel: [{ stat: 'stashSlots', value: 10 }],
  },
];

export function getBuildingDef(id: string): HomesteadBuilding | undefined {
  return BUILDINGS.find(b => b.id === id);
}

/**
 * Look of a wing in the tower: 0 ruin (locked or never built), 1 restored,
 * 2 thriving (level 3+, or 2+ for the three-level altar).
 */
export function buildingStage(def: HomesteadBuilding, level: number, unlocked: boolean): 0 | 1 | 2 {
  if (!unlocked || level <= 0) return 0;
  const thriving = def.maxLevel >= 5 ? 3 : 2;
  return level >= thriving ? 2 : 1;
}

// ── Embers (余烬) ─────────────────────────────────────────────────────────

/** Embers for a kill: zone bosses 5, mini-bosses 3, affixed elites 1. */
export function embersForKill(m: { elite?: boolean; isMiniBoss?: boolean }, eliteAffixCount: number): number {
  if (m.elite) return 5;
  if (m.isMiniBoss) return 3;
  return eliteAffixCount > 0 ? 1 : 0;
}

/** Embers for a quest turn-in: the quest's own reward, else 2 for main quests and 1 for side quests. */
export function embersForQuest(q: { category: 'main' | 'side'; rewards: { embers?: number } }): number {
  return q.rewards.embers ?? (q.category === 'main' ? 2 : 1);
}

// ── Herb garden (药草园) ────────────────────────────────────────────────────

/** Kills needed for one garden yield. */
export function gardenInterval(level: number): number {
  return Math.max(6, 16 - 2 * level);
}

/** How many items the garden holds before it stops growing. */
export function gardenCapacity(level: number): number {
  return level <= 0 ? 0 : 4 + 4 * level;
}

/** One garden yield: ley fruit sometimes, otherwise a potion whose tier grows with the garden. */
export function rollGardenYield(level: number, rng: () => number = Math.random): string {
  if (rng() < 0.12 + 0.03 * level) return LEY_FRUIT_ID;
  const hp = rng() < 0.6;
  if (hp) return level >= 4 ? 'c_hp_potion_l' : level >= 2 ? 'c_hp_potion_m' : 'c_hp_potion_s';
  return level >= 2 ? 'c_mp_potion_m' : 'c_mp_potion_s';
}

// ── Gem workshop (炉台) ─────────────────────────────────────────────────────

/** Gems combined into one of the next tier. */
export const GEM_COMBINE_COUNT = 3;

/** The gem three of `gemId` combine into, or null at the top of its line. */
export function nextGemId(gemId: string): string | null {
  const m = /^g_([a-z]+)_(\d)$/.exec(gemId);
  if (!m) return null;
  const next = `g_${m[1]}_${Number(m[2]) + 1}`;
  return GEM_STAT_MAP[next] ? next : null;
}

/** Highest tier the workshop can make: level 1 → tier 2 … level 4+ → tier 5. */
export function maxCombineTier(workshopLevel: number): number {
  return workshopLevel <= 0 ? 0 : Math.min(5, workshopLevel + 1);
}

/** Gold per combine (scales with the tier made). */
export function gemCombineGold(targetTier: number): number {
  return 40 * targetTier * targetTier;
}

export type GemCombineBlock = 'noNext' | 'workshop' | 'level' | 'count' | 'gold' | null;

/** Why combining `gemId` is not possible right now (null: it is). */
export function gemCombineBlock(gemId: string, have: number, workshopLevel: number, playerLevel: number, gold: number): GemCombineBlock {
  const next = nextGemId(gemId);
  if (!next) return 'noNext';
  const tier = GEM_STAT_MAP[next].tier;
  if (tier > maxCombineTier(workshopLevel)) return 'workshop';
  if ((getItemBase(next)?.levelReq ?? 1) > playerLevel) return 'level';
  if (have < GEM_COMBINE_COUNT) return 'count';
  if (gold < gemCombineGold(tier)) return 'gold';
  return null;
}

// ── Caravan expeditions (商队驿站) ──────────────────────────────────────────

export interface ExpeditionOption {
  id: 'short' | 'long';
  killsRequired: number;
  durationMs: number;
}

/** A pet on expedition is back after this many kills or this much play time, whichever comes first. */
export const EXPEDITION_OPTIONS: readonly ExpeditionOption[] = [
  { id: 'short', killsRequired: 25, durationMs: 10 * 60_000 },
  { id: 'long', killsRequired: 60, durationMs: 25 * 60_000 },
];

export interface ExpeditionReward {
  embers: number;
  gold: number;
  items: { itemId: string; count: number }[];
}

/** What an expedition brings back, scaled by the caravan post's level and the hero's level. */
export function rollExpeditionReward(optionId: string, postLevel: number, playerLevel: number, rng: () => number = Math.random): ExpeditionReward {
  const long = optionId === 'long';
  const lv = Math.max(1, postLevel);
  const embers = long ? 10 + 4 * lv : 4 + 2 * lv;
  const gold = Math.round((long ? 60 : 25) * lv + playerLevel * (long ? 12 : 5) * (0.8 + rng() * 0.4));
  const items: ExpeditionReward['items'] = [];
  if (long) {
    const lines = ['ruby', 'sapphire', 'emerald', 'topaz'];
    const tier = Math.min(3, 1 + Math.floor(playerLevel / 15));
    items.push({ itemId: `g_${lines[Math.floor(rng() * lines.length) % lines.length]}_${tier}`, count: 1 });
    items.push({ itemId: LEY_FRUIT_ID, count: 2 });
  } else {
    items.push({ itemId: rng() < 0.5 ? LEY_FRUIT_ID : (playerLevel >= 25 ? 'c_hp_potion_l' : 'c_hp_potion_m'), count: 1 });
  }
  return { embers, gold, items };
}

// ── Altar blessings (心焰祭坛) ──────────────────────────────────────────────

export interface BlessingDef {
  id: string;
  /** Stats at altar level 1; each further level adds half again. */
  stats: Partial<EquipStats>;
  glyph: 'blade' | 'shield' | 'coin' | 'wing';
}

export const BLESSINGS: readonly BlessingDef[] = [
  { id: 'ember_edge', stats: { damagePercent: 10, critRate: 3 }, glyph: 'blade' },
  { id: 'hearth_ward', stats: { maxHpPercent: 12, defensePercent: 12 }, glyph: 'shield' },
  { id: 'ley_fortune', stats: { magicFind: 30, expBonus: 10 }, glyph: 'coin' },
  { id: 'swift_flame', stats: { attackSpeed: 8, moveSpeed: 10 }, glyph: 'wing' },
];

/** A blessing lasts this much play time outside the tower, or until the hero returns to it. */
export const BLESSING_DURATION_MS = 30 * 60_000;

export function blessingCost(altarLevel: number): number {
  return 10 + 5 * Math.max(0, altarLevel - 1);
}

/** Stats of a blessing bought at `altarLevel`. */
export function blessingStats(id: string, altarLevel: number): Partial<EquipStats> {
  const def = BLESSINGS.find(b => b.id === id);
  if (!def) return {};
  const mul = 1 + 0.5 * Math.max(0, altarLevel - 1);
  const out: Partial<EquipStats> = {};
  for (const [k, v] of Object.entries(def.stats) as [keyof EquipStats, number][]) out[k] = Math.round(v * mul);
  return out;
}

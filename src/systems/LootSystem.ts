import { randomInt, randomFloat, chance } from '../utils/MathUtils';
import { Weapons, Armors, Accessories, Consumables, Gems, getItemBase } from '../data/items/bases';
import { Prefixes, Suffixes } from '../data/items/affixes';
import { LegendaryItems, SetDefinitions, SetPieceBases } from '../data/items/sets';
import { DUNGEON_EXCLUSIVE_LEGENDARIES, DUNGEON_EXCLUSIVE_SETS, DUNGEON_SET_PIECE_BASES } from '../data/dungeonData';
import type { ItemInstance, ItemQuality, ItemAffix, AffixDefinition, MonsterDefinition, WeaponBase, ArmorBase } from '../data/types';
import type { Difficulty } from './DifficultySystem';

let uidCounter = 0;
function genUid(): string { return `item_${Date.now()}_${uidCounter++}`; }

export class LootSystem {
  /** Quality ordering for comparison. */
  private static readonly QUALITY_ORDER: ItemQuality[] = ['normal', 'magic', 'rare', 'legendary', 'set'];

  /** Check if quality meets minimum threshold. */
  static qualityMeetsFloor(quality: ItemQuality, floor: ItemQuality): boolean {
    return LootSystem.QUALITY_ORDER.indexOf(quality) >= LootSystem.QUALITY_ORDER.indexOf(floor);
  }

  /** Difficulty-based loot modifiers: item level bonus, quality bonus, extra affix count. */
  private static getDifficultyLootMods(difficulty: Difficulty): { levelBonus: number; qualityBonus: number; extraAffixes: number } {
    switch (difficulty) {
      case 'nightmare': return { levelBonus: 3, qualityBonus: 5, extraAffixes: 1 };
      case 'hell': return { levelBonus: 6, qualityBonus: 12, extraAffixes: 2 };
      default: return { levelBonus: 0, qualityBonus: 0, extraAffixes: 0 };
    }
  }

  generateLoot(monster: MonsterDefinition, playerLuck: number, affixLootBonus = 0, difficulty: Difficulty = 'normal'): ItemInstance[] {
    const items: ItemInstance[] = [];
    const diffMods = LootSystem.getDifficultyLootMods(difficulty);
    const level = monster.level + diffMods.levelBonus;
    const baseDropRate = monster.elite ? 80 : 40;
    const luckBonus = playerLuck * 0.5;

    // Gold is handled separately in ZoneScene
    // Equipment drop
    if (chance(baseDropRate + luckBonus)) {
      const quality = this.rollQuality(level, playerLuck, monster.elite ?? false, affixLootBonus + diffMods.qualityBonus);
      const item = this.generateEquipment(level, quality, diffMods.extraAffixes);
      if (item) items.push(item);
    }

    // Second drop for elites
    if (monster.elite && chance(50 + luckBonus)) {
      const quality = this.rollQuality(level, playerLuck, true, affixLootBonus + diffMods.qualityBonus);
      const item = this.generateEquipment(level, quality, diffMods.extraAffixes);
      if (item) items.push(item);
    }

    // Third drop for affix elites with high bonus
    if (affixLootBonus >= 10 && chance(30 + luckBonus + affixLootBonus)) {
      const quality = this.rollQuality(level, playerLuck, true, affixLootBonus + diffMods.qualityBonus);
      const item = this.generateEquipment(level, quality, diffMods.extraAffixes);
      if (item) items.push(item);
    }

    // Mini-boss guaranteed loot quality floor:
    // Sub-dungeon mini-bosses guarantee at least rare+, zone mini-bosses guarantee at least magic+
    if (monster.isMiniBoss) {
      const qualityFloor: ItemQuality = monster.isSubDungeonMiniBoss ? 'rare' : 'magic';
      this.enforceMiniBossQualityFloor(items, level, qualityFloor);
    }

    // Consumable drop
    if (chance(30)) {
      const potion = this.generateConsumable(level);
      if (potion) items.push(potion);
    }

    // Gem drop (rare, improved for affix elites)
    if (chance(5 + luckBonus * 0.2 + affixLootBonus * 0.3)) {
      const gem = this.generateGem(level);
      if (gem) items.push(gem);
    }

    return items;
  }

  /**
   * Enforce minimum loot quality for mini-bosses.
   * If no equipment item meets the quality floor, either upgrade the best existing
   * equipment item or add a new guaranteed drop.
   */
  private enforceMiniBossQualityFloor(items: ItemInstance[], level: number, qualityFloor: ItemQuality): void {
    const equipItems = items.filter(i => {
      const base = getItemBase(i.baseId);
      return base && (base.slot !== undefined);
    });

    const hasQualityItem = equipItems.some(i => LootSystem.qualityMeetsFloor(i.quality, qualityFloor));

    if (!hasQualityItem) {
      // Generate a guaranteed equipment drop at the quality floor
      // Use broader level search to ensure we always find a base item
      let item = this.generateEquipment(level, qualityFloor);
      if (!item) {
        // Retry with a wider level range (go lower) if no item found at the monster's level
        item = this.generateEquipmentWide(level, qualityFloor);
      }
      if (item) {
        items.push(item);
      }
    }
  }

  /** Generate equipment with a wider level search range (used as fallback for mini-boss drops). */
  private generateEquipmentWide(level: number, quality: ItemQuality): ItemInstance | null {
    const allEquip = [...Weapons, ...Armors, ...Accessories]
      .filter(b => b.levelReq <= level + 5 && b.levelReq >= Math.max(1, level - 20));
    if (allEquip.length === 0) {
      // Last resort: pick any equipment item
      const anyEquip = [...Weapons, ...Armors, ...Accessories];
      if (anyEquip.length === 0) return null;
      const base = anyEquip[randomInt(0, anyEquip.length - 1)];
      return this.createItem(base.id, level, quality);
    }
    const base = allEquip[randomInt(0, allEquip.length - 1)];
    return this.createItem(base.id, level, quality);
  }

  private rollQuality(level: number, luck: number, isElite: boolean, affixBonus = 0): ItemQuality {
    const roll = Math.random() * 100;
    const luckMod = luck * 0.3;
    const eliteMod = isElite ? 15 : 0;
    const afxMod = affixBonus;

    if (roll < 0.5 + luckMod * 0.1 + (level > 20 ? 1 : 0) + afxMod * 0.15) return 'legendary';
    if (roll < 2 + luckMod * 0.2 + eliteMod * 0.5 + afxMod * 0.3) return 'set';
    if (roll < 15 + luckMod + eliteMod + afxMod) return 'rare';
    if (roll < 45 + luckMod + afxMod * 0.5) return 'magic';
    return 'normal';
  }

  generateEquipment(level: number, quality: ItemQuality, extraAffixes = 0): ItemInstance | null {
    // For set quality, directly pick a random set piece to avoid the base-matching problem
    if (quality === 'set') {
      return this.generateSetPiece(level);
    }

    // Pick a random base appropriate for the level
    const allEquip = [...Weapons, ...Armors, ...Accessories]
      .filter(b => b.levelReq <= level + 3 && b.levelReq >= Math.max(1, level - 10));
    if (allEquip.length === 0) return null;

    const base = allEquip[randomInt(0, allEquip.length - 1)];
    return this.createItem(base.id, level, quality, extraAffixes);
  }

  private generateSetPiece(level: number): ItemInstance | null {
    const allSets = [...SetDefinitions, ...DUNGEON_EXCLUSIVE_SETS];
    const allPieceBases = { ...SetPieceBases, ...DUNGEON_SET_PIECE_BASES };

    // Collect all valid set pieces (level-appropriate)
    const candidates: { pieceId: string; baseId: string; setDef: typeof allSets[number] }[] = [];
    for (const setDef of allSets) {
      for (const pieceId of setDef.pieces) {
        const baseId = allPieceBases[pieceId];
        if (!baseId) continue;
        const base = getItemBase(baseId);
        if (!base) continue;
        if (base.levelReq <= level + 5 && base.levelReq >= Math.max(1, level - 15)) {
          candidates.push({ pieceId, baseId, setDef });
        }
      }
    }

    if (candidates.length === 0) return null;
    const pick = candidates[randomInt(0, candidates.length - 1)];
    const base = getItemBase(pick.baseId);
    if (!base) return null;

    const item: ItemInstance = {
      uid: genUid(),
      baseId: pick.baseId,
      name: `${pick.setDef.name} ${base.name}`,
      quality: 'set',
      level,
      affixes: [],
      sockets: [],
      identified: true,
      quantity: 1,
      stats: {},
      setId: pick.setDef.id,
    };

    const pieceAffixes = pick.setDef.pieceAffixes?.[pick.pieceId];
    if (pieceAffixes) {
      item.affixes = [...pieceAffixes];
    }
    // Add 1-2 random affixes for variety (avoid stats already used by fixed affixes)
    this.addRandomAffixes(item, level, 1, 2);
    this.computeStats(item);
    return item;
  }

  createItem(baseId: string, level: number, quality: ItemQuality, extraAffixes = 0): ItemInstance | null {
    const base = getItemBase(baseId);
    if (!base) return null;

    const item: ItemInstance = {
      uid: genUid(),
      baseId,
      name: base.name,
      quality,
      level,
      affixes: [],
      sockets: [],
      identified: true,
      quantity: 1,
      stats: {},
    };

    // Generate affixes based on quality (extraAffixes from difficulty add to count)
    switch (quality) {
      case 'magic':
        this.addRandomAffixes(item, level, 1, 2 + extraAffixes);
        break;
      case 'rare':
        this.addRandomAffixes(item, level, 3, 4 + extraAffixes);
        break;
      case 'legendary':
        this.makeLegendary(item, baseId);
        break;
      case 'set':
        this.makeSetItem(item, baseId);
        break;
    }

    this.buildItemName(item);
    this.computeStats(item);
    return item;
  }

  private addRandomAffixes(item: ItemInstance, level: number, min: number, max: number): void {
    const count = randomInt(min, max);
    const usedIds = new Set<string>();
    // Pre-populate with existing affix IDs (e.g. from set piece fixed affixes)
    for (const existing of item.affixes) {
      usedIds.add(existing.affixId);
    }
    let prefixCount = 0;
    let suffixCount = 0;
    const base = getItemBase(item.baseId);
    const itemSlot = base?.slot;

    // Determine preferred affix tier based on item level
    // Zone 1 (lv 1-7): tiers 1-2, Zone 2 (lv 8-17): tiers 1-3,
    // Zone 3 (lv 18-27): tiers 2-4, Zone 4 (lv 28-37): tiers 3-5, Zone 5 (lv 38+): tiers 4-5
    const minTier = level < 8 ? 1 : level < 18 ? 1 : level < 28 ? 2 : level < 38 ? 3 : 4;
    const maxTier = level < 8 ? 2 : level < 18 ? 3 : level < 28 ? 4 : 5;

    for (let i = 0; i < count; i++) {
      const wantPrefix = prefixCount <= suffixCount;
      const pool = (wantPrefix ? Prefixes : Suffixes)
        .filter(a => {
          if (a.levelReq > level + 5) return false;
          if (usedIds.has(a.id)) return false;
          if (a.allowedSlots && itemSlot && !a.allowedSlots.includes(itemSlot)) return false;
          // Prefer zone-appropriate tiers: allow ±1 around the ideal range
          if (a.tier < Math.max(1, minTier - 1) || a.tier > Math.min(5, maxTier + 1)) return false;
          return true;
        });

      if (pool.length === 0) continue;

      // Weight towards ideal tier range
      const weighted: AffixDefinition[] = [];
      for (const a of pool) {
        const inRange = a.tier >= minTier && a.tier <= maxTier;
        const weight = inRange ? 3 : 1;
        for (let w = 0; w < weight; w++) weighted.push(a);
      }

      const affix = weighted[randomInt(0, weighted.length - 1)];
      const value = randomInt(affix.minValue, affix.maxValue);

      item.affixes.push({
        affixId: affix.id,
        name: affix.name,
        stat: affix.stat,
        value,
      });

      usedIds.add(affix.id);
      if (affix.type === 'prefix') prefixCount++;
      else suffixCount++;
    }
  }

  private makeLegendary(item: ItemInstance, baseId: string): void {
    const allLegendaries = [...LegendaryItems, ...DUNGEON_EXCLUSIVE_LEGENDARIES];
    const legendaryDef = allLegendaries.find(l => l.baseId === baseId);
    if (legendaryDef) {
      item.name = legendaryDef.name;
      // Scale legendary affix values by item level (base values defined for ~lv30-40)
      const levelScale = Math.max(0.6, Math.min(1.5, item.level / 35));
      item.affixes = legendaryDef.fixedAffixes.map(a => ({
        ...a,
        value: Math.round(a.value * levelScale),
      }));
      item.legendaryEffect = legendaryDef.specialEffectDescription;
      item.identified = true;
    } else {
      // Generic legendary if no specific one exists
      this.addRandomAffixes(item, item.level, 3, 5);
      item.legendaryEffect = '蕴含未知的力量';
    }
  }

  private makeSetItem(item: ItemInstance, baseId: string): void {
    const allSets = [...SetDefinitions, ...DUNGEON_EXCLUSIVE_SETS];
    // Find the set this piece belongs to
    for (const setDef of allSets) {
      if (setDef.pieces.includes(baseId) && setDef.pieceAffixes?.[baseId]) {
        item.setId = setDef.id;
        item.name = setDef.name;
        item.affixes = [...setDef.pieceAffixes[baseId]];
        item.identified = true;
        return;
      }
    }
    // Fallback: generic set item with random affixes
    this.addRandomAffixes(item, item.level, 2, 3);
  }

  private buildItemName(item: ItemInstance): void {
    if (item.quality === 'legendary' || item.quality === 'set') return;
    if (item.quality === 'normal') return;

    const prefix = item.affixes.find(a => {
      const def = [...Prefixes].find(p => p.id === a.affixId);
      return def?.type === 'prefix';
    });
    const suffix = item.affixes.find(a => {
      const def = [...Suffixes].find(s => s.id === a.affixId);
      return def?.type === 'suffix';
    });

    const base = getItemBase(item.baseId);
    if (!base) return;

    let name = base.name;
    if (prefix) name = `${prefix.name}${name}`;
    if (suffix) name = `${name} (${suffix.name})`;
    item.name = name;
  }

  private computeStats(item: ItemInstance): void {
    const stats: Record<string, number> = {};
    for (const affix of item.affixes) {
      stats[affix.stat] = (stats[affix.stat] ?? 0) + affix.value;
    }
    for (const gem of item.sockets) {
      stats[gem.stat] = (stats[gem.stat] ?? 0) + gem.value;
    }
    item.stats = stats;
  }

  private generateConsumable(level: number): ItemInstance | null {
    const available = Consumables.filter(c => c.levelReq <= level + 5);
    if (available.length === 0) return null;
    const base = available[randomInt(0, available.length - 1)];
    return {
      uid: genUid(),
      baseId: base.id,
      name: base.name,
      quality: 'normal',
      level: 1,
      affixes: [],
      sockets: [],
      identified: true,
      quantity: randomInt(1, 3),
      stats: {},
    };
  }

  private generateGem(level: number): ItemInstance | null {
    const available = Gems.filter(g => g.levelReq <= level + 5);
    if (available.length === 0) return null;
    const base = available[randomInt(0, available.length - 1)];
    return {
      uid: genUid(),
      baseId: base.id,
      name: base.name,
      quality: 'normal',
      level: 1,
      affixes: [],
      sockets: [],
      identified: true,
      quantity: 1,
      stats: {},
    };
  }
}

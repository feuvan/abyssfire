import { randomInt, randomFloat, chance } from '../utils/MathUtils';
import { Weapons, Armors, Accessories, Consumables, Gems, getItemBase } from '../data/items/bases';
import { Prefixes, Suffixes } from '../data/items/affixes';
import { LegendaryItems } from '../data/items/sets';
import type { ItemInstance, ItemQuality, ItemAffix, AffixDefinition, MonsterDefinition, WeaponBase, ArmorBase } from '../data/types';

let uidCounter = 0;
function genUid(): string { return `item_${Date.now()}_${uidCounter++}`; }

export class LootSystem {
  generateLoot(monster: MonsterDefinition, playerLuck: number): ItemInstance[] {
    const items: ItemInstance[] = [];
    const level = monster.level;
    const baseDropRate = monster.elite ? 80 : 40;
    const luckBonus = playerLuck * 0.5;

    // Gold is handled separately in ZoneScene
    // Equipment drop
    if (chance(baseDropRate + luckBonus)) {
      const quality = this.rollQuality(level, playerLuck, monster.elite ?? false);
      const item = this.generateEquipment(level, quality);
      if (item) items.push(item);
    }

    // Second drop for elites
    if (monster.elite && chance(50 + luckBonus)) {
      const quality = this.rollQuality(level, playerLuck, true);
      const item = this.generateEquipment(level, quality);
      if (item) items.push(item);
    }

    // Consumable drop
    if (chance(30)) {
      const potion = this.generateConsumable(level);
      if (potion) items.push(potion);
    }

    // Gem drop (rare)
    if (chance(5 + luckBonus * 0.2)) {
      const gem = this.generateGem(level);
      if (gem) items.push(gem);
    }

    return items;
  }

  private rollQuality(level: number, luck: number, isElite: boolean): ItemQuality {
    const roll = Math.random() * 100;
    const luckMod = luck * 0.3;
    const eliteMod = isElite ? 15 : 0;

    if (roll < 0.5 + luckMod * 0.1 + (level > 20 ? 1 : 0)) return 'legendary';
    if (roll < 2 + luckMod * 0.2 + eliteMod * 0.5) return 'set';
    if (roll < 15 + luckMod + eliteMod) return 'rare';
    if (roll < 45 + luckMod) return 'magic';
    return 'normal';
  }

  generateEquipment(level: number, quality: ItemQuality): ItemInstance | null {
    // Pick a random base appropriate for the level
    const allEquip = [...Weapons, ...Armors, ...Accessories]
      .filter(b => b.levelReq <= level + 3 && b.levelReq >= Math.max(1, level - 10));
    if (allEquip.length === 0) return null;

    const base = allEquip[randomInt(0, allEquip.length - 1)];
    return this.createItem(base.id, level, quality);
  }

  createItem(baseId: string, level: number, quality: ItemQuality): ItemInstance | null {
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

    // Generate affixes based on quality
    switch (quality) {
      case 'magic':
        this.addRandomAffixes(item, level, 1, 2);
        break;
      case 'rare':
        this.addRandomAffixes(item, level, 3, 4);
        break;
      case 'legendary':
        this.makeLegendary(item, baseId);
        break;
      case 'set':
        // Sets have fixed stats - simplified for now
        this.addRandomAffixes(item, level, 2, 3);
        break;
    }

    this.buildItemName(item);
    this.computeStats(item);
    return item;
  }

  private addRandomAffixes(item: ItemInstance, level: number, min: number, max: number): void {
    const count = randomInt(min, max);
    const usedIds = new Set<string>();
    let prefixCount = 0;
    let suffixCount = 0;

    for (let i = 0; i < count; i++) {
      const wantPrefix = prefixCount <= suffixCount;
      const pool = (wantPrefix ? Prefixes : Suffixes)
        .filter(a => a.levelReq <= level + 5 && !usedIds.has(a.id));

      if (pool.length === 0) continue;

      const affix = pool[randomInt(0, pool.length - 1)];
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
    const legendaryDef = LegendaryItems.find(l => l.baseId === baseId);
    if (legendaryDef) {
      item.name = legendaryDef.name;
      item.affixes = [...legendaryDef.fixedAffixes];
      item.legendaryEffect = legendaryDef.specialEffectDescription;
      item.identified = true;
    } else {
      // Generic legendary if no specific one exists
      this.addRandomAffixes(item, item.level, 3, 5);
      item.legendaryEffect = '蕴含未知的力量';
    }
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

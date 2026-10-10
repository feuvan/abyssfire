/**
 * Items and loot: item_bases, affixes, sets, legendaries, economy, loot_rules, crafting.
 *
 * Tables are dumped verbatim from src/data/items/*.ts and src/data/dungeonData.ts (array order kept:
 * it is the uniform-pick pool order). Behaviour constants of LootSystem / InventorySystem /
 * CraftingSystem / QuestRewards are read from the code: exported constants directly, private ones
 * through probes of the real functions (rollQuality with scripted Math.random, useConsumable,
 * craftCost, salvageYield, rewardItemLevel) or pinned with source assertions.
 */
import { Weapons, Armors, Accessories, Consumables, Gems, Materials, GEM_STAT_MAP, AllItemBases } from '../../../../../src/data/items/bases';
import { Prefixes, Suffixes, STAT_DISPLAY } from '../../../../../src/data/items/affixes';
import { SetDefinitions, SetPieceBases, LegendaryItems } from '../../../../../src/data/items/sets';
import { DUNGEON_EXCLUSIVE_LEGENDARIES, DUNGEON_EXCLUSIVE_SETS, DUNGEON_SET_PIECE_BASES } from '../../../../../src/data/dungeonData';
import { InventorySystem, MAX_INVENTORY, BASE_STASH_SLOTS } from '../../../../../src/systems/InventorySystem';
import * as InventoryMod from '../../../../../src/systems/InventorySystem';
import { LootSystem } from '../../../../../src/systems/LootSystem';
import * as Crafting from '../../../../../src/systems/CraftingSystem';
import * as QuestRewards from '../../../../../src/systems/QuestRewards';
import { leyFruitDropChance } from '../../../../../src/systems/PetSystem';
import { BUILDINGS } from '../../../../../src/data/homestead';
import type { ItemInstance, ItemQuality, QuestDefinition } from '../../../../../src/data/types';
import { assert, assertSource, plain, withRandom, type TableResult } from '../util';

const exp = <T>(mod: unknown, name: string): T => {
  const v = (mod as Record<string, unknown>)[`__expose_${name}`];
  assert(v !== undefined, `exposed binding ${name} missing`);
  return v as T;
};
const ZS = 'src/scenes/ZoneScene.ts';

function fakeItem(baseId: string, quality: ItemQuality, level: number): ItemInstance {
  return { uid: 'probe', baseId, name: '', quality, level, affixes: [], sockets: [], identified: true, quantity: 1, stats: {} };
}

// ── item_bases.json ────────────────────────────────────────────────────────

function itemBases(): TableResult {
  // Consumable effects: InventorySystem.useConsumable (InventorySystem.ts:407-432), called per base.
  const consumableEffects: Record<string, { effect: string; value: number } | null> = {};
  for (const base of [...Consumables]) {
    const inv = new InventorySystem();
    inv.inventory.push({ ...fakeItem(base.id, 'normal', 1), quantity: 2 });
    consumableEffects[base.id] = inv.useConsumable('probe');
  }
  // Ground potions (ZoneScene.ts:3900-3906): these bases drop as instant potion pickups instead of items.
  assertSource(ZS,
    "c_hp_potion_s: { type: 'hp', amount: 50 },", "c_hp_potion_m: { type: 'hp', amount: 150 },",
    "c_hp_potion_l: { type: 'hp', amount: 400 },", "c_mp_potion_s: { type: 'mp', amount: 30 },",
    "c_mp_potion_m: { type: 'mp', amount: 80 },");
  const groundPotions = {
    c_hp_potion_s: { type: 'hp', amount: 50 }, c_hp_potion_m: { type: 'hp', amount: 150 },
    c_hp_potion_l: { type: 'hp', amount: 400 }, c_mp_potion_s: { type: 'mp', amount: 30 },
    c_mp_potion_m: { type: 'mp', amount: 80 },
  };
  assertSource('src/systems/InventorySystem.ts', "for (const s of ['str', 'dex', 'int', 'vit', 'spi', 'lck']) {");
  const groups = {
    weapons: plain(Weapons), armors: plain(Armors), accessories: plain(Accessories),
    consumables: plain(Consumables), gems: plain(Gems), materials: plain(Materials),
  };
  const ids = AllItemBases.map(b => b.id);
  assert(new Set(ids).size === ids.length, 'duplicate item base id');
  for (const g of Gems) assert(GEM_STAT_MAP[g.id], `gem ${g.id} has no GEM_STAT_MAP row`);
  return {
    file: 'item_bases.json',
    source: ['src/data/items/bases.ts', 'src/systems/InventorySystem.ts', ZS],
    data: {
      order: ids,
      equipmentPoolOrder: ['weapons', 'armors', 'accessories'],
      groups,
      gemStats: plain(GEM_STAT_MAP),
      allStatsExpandsTo: ['str', 'dex', 'int', 'vit', 'spi', 'lck'],
      consumableEffects,
      groundPotions,
      materialIds: [...Crafting.MATERIAL_IDS],
      i18n: { name: 'data.item.<id>.name', desc: 'data.item.<id>.desc' },
    },
    counts: {
      bases: ids.length, weapons: Weapons.length, armors: Armors.length, accessories: Accessories.length,
      consumables: Consumables.length, gems: Gems.length, materials: Materials.length,
    },
  };
}

// ── affixes / sets / legendaries ───────────────────────────────────────────

function affixes(): TableResult {
  return {
    file: 'affixes.json',
    source: ['src/data/items/affixes.ts'],
    data: { prefixes: plain(Prefixes), suffixes: plain(Suffixes), statDisplay: plain(STAT_DISPLAY),
      i18n: { name: 'data.affix.<id> (getAffixName looks up data.affix.<id>.name, which no table defines, so the web shows the stored zh name / nameEn)' } },
    counts: { prefixes: Prefixes.length, suffixes: Suffixes.length, statDisplay: Object.keys(STAT_DISPLAY).length },
  };
}

function sets(): TableResult {
  const all = [
    ...SetDefinitions.map(s => ({ ...plain(s), dungeonExclusive: false })),
    ...DUNGEON_EXCLUSIVE_SETS.map(s => ({ ...plain(s), dungeonExclusive: true })),
  ];
  const pieceBases: Record<string, { baseId: string; dungeonExclusive: boolean }> = {};
  for (const [k, v] of Object.entries(SetPieceBases)) pieceBases[k] = { baseId: v, dungeonExclusive: false };
  for (const [k, v] of Object.entries(DUNGEON_SET_PIECE_BASES)) {
    assert(!(k in pieceBases), `set piece ${k} defined twice`);
    pieceBases[k] = { baseId: v, dungeonExclusive: true };
  }
  for (const s of all) for (const p of s.pieces) assert(pieceBases[p], `set piece ${p} has no base`);
  return {
    file: 'sets.json',
    source: ['src/data/items/sets.ts', 'src/data/dungeonData.ts'],
    data: { sets: all, pieceBases, i18n: { name: 'data.set.<id>.name', bonus: 'data.set.<id>.bonus.<count>', affix: 'data.setAffix.<affixId>' } },
    counts: { sets: all.length, pieces: Object.keys(pieceBases).length },
  };
}

function legendaries(): TableResult {
  const all = [
    ...LegendaryItems.map(l => ({ ...plain(l), dungeonExclusive: false })),
    ...DUNGEON_EXCLUSIVE_LEGENDARIES.map(l => ({ ...plain(l), dungeonExclusive: true })),
  ];
  return {
    file: 'legendaries.json',
    source: ['src/data/items/sets.ts', 'src/data/dungeonData.ts'],
    data: {
      legendaries: all,
      lookup: 'first legendary (array order, overworld first) whose baseId matches; none → generic legendary (3–5 random affixes)',
      levelScale: { divisor: 35, min: 0.6, max: 1.5, rounding: 'Math.round' },
      // DECISIONS C11: specialEffect keys combat reads as item stats.
      appliedSpecialEffects: ['killHealPercent', 'elementalDamagePercent', 'doubleShot', 'ignoreDefense', 'dodgeCounter', 'damageReduction', 'cooldownReduction'],
      i18n: { name: 'data.legendary.<id>.name', effect: 'data.legendary.<id>.effect', affix: 'data.legAffix.<affixId>' },
    },
    counts: { legendaries: all.length, overworld: LegendaryItems.length, dungeon: DUNGEON_EXCLUSIVE_LEGENDARIES.length },
  };
}

// ── economy.json ───────────────────────────────────────────────────────────

function economy(): TableResult {
  const maxBuyback = (InventorySystem as unknown as Record<string, number>).MAX_BUYBACK;
  assert(maxBuyback === 5, 'MAX_BUYBACK');
  assertSource('src/scenes/UIScene.ts', 'const buyPrice = base.sellPrice * 3;');
  assertSource('src/systems/InventorySystem.ts', 'const price = base ? base.sellPrice * item.quantity : 1;',
    'this.buybackItems.push({ item: soldCopy, buybackPrice: price * 5 });');
  const warehouse = BUILDINGS.find(b => b.id === 'warehouse');
  assert(warehouse, 'warehouse building');
  return {
    file: 'economy.json',
    source: ['src/systems/InventorySystem.ts', 'src/scenes/UIScene.ts', 'src/data/homestead.ts'],
    data: {
      buyPriceMultiplier: 3,
      buybackPriceMultiplier: 5,
      buybackSlots: maxBuyback,
      sellPrice: 'base.sellPrice × quantity (unknown base → 1)',
      // DECISIONS I9 (port change): sell price × quality multiplier.
      sellQualityMultiplier: { normal: 1, magic: 1.5, rare: 2.5, legendary: 4, set: 4, decision: 'I9' },
      bagCapacity: MAX_INVENTORY,
      stashBaseSlots: BASE_STASH_SLOTS,
      stashSlotsPerWarehouseLevel: warehouse.bonusPerLevel.filter(b => b.stat === 'stashSlots').map(b => b.value),
      shopItemLevel: 'player level, quality normal',
      sortOrder: {
        quality: plain(exp<Record<string, number>>(InventoryMod, 'QUALITY_ORDER')),
        type: plain(exp<Record<string, number>>(InventoryMod, 'TYPE_ORDER')),
      },
    },
    counts: { constants: 8 },
  };
}

// ── loot_rules.json ────────────────────────────────────────────────────────

const QUALITY_THRESHOLDS = [
  // r = rand()*100; lm = luck*0.3; em = elite ? 15 : 0; first match wins (LootSystem.ts:124-135)
  { quality: 'legendary', base: 0.5, lm: 0.1, em: 0, affix: 0.15, levelOver20: 1 },
  { quality: 'set', base: 2, lm: 0.2, em: 0.5, affix: 0.3, levelOver20: 0 },
  { quality: 'rare', base: 15, lm: 1, em: 1, affix: 1, levelOver20: 0 },
  { quality: 'magic', base: 45, lm: 1, em: 0, affix: 0.5, levelOver20: 0 },
] as const;

function tableQuality(r: number, level: number, luck: number, elite: boolean, affix: number): string {
  const lm = luck * 0.3;
  const em = elite ? 15 : 0;
  for (const t of QUALITY_THRESHOLDS) {
    if (r < t.base + lm * t.lm + em * t.em + affix * t.affix + (level > 20 ? t.levelOver20 : 0)) return t.quality;
  }
  return 'normal';
}

function lootRules(): TableResult {
  const loot = new LootSystem() as unknown as { rollQuality: (l: number, luck: number, e: boolean, a: number) => string };
  // Probe rollQuality with scripted draws against the threshold table.
  for (const level of [3, 20, 21, 40]) for (const luck of [0, 5, 13]) for (const elite of [false, true]) for (const affix of [0, 5, 17]) {
    for (const r of [0, 0.4, 0.9, 1.3, 1.6, 2.2, 5, 11, 16.4, 30, 36, 44.9, 46, 49, 60, 99.9]) {
      const web = withRandom([r / 100], () => loot.rollQuality(level, luck, elite, affix));
      assert(web === tableQuality(r, level, luck, elite, affix), `rollQuality L${level} luck${luck} e${elite} a${affix} r${r}: web ${web}`);
    }
  }
  assertSource('src/systems/LootSystem.ts',
    'const baseDropRate = monster.elite ? 80 : 40;', 'const luckBonus = playerLuck * 0.5;',
    'if (monster.elite && chance(50 + luckBonus)) {',
    'if (affixLootBonus >= 10 && chance(30 + luckBonus + affixLootBonus)) {',
    "const qualityFloor: ItemQuality = monster.isSubDungeonMiniBoss ? 'rare' : 'magic';",
    'if (chance(30)) {', 'if (chance(5 + luckBonus * 0.2 + affixLootBonus * 0.3)) {',
    '.filter(b => b.levelReq <= level + 5 && b.levelReq >= Math.max(1, level - 20));',
    '.filter(b => b.levelReq <= level + 3 && b.levelReq >= Math.max(1, level - 10));',
    'if (base.levelReq <= level + 5 && base.levelReq >= Math.max(1, level - 15)) {',
    'this.addRandomAffixes(item, level, 1, 2 + extraAffixes);', 'this.addRandomAffixes(item, level, 3, 4 + extraAffixes);',
    'this.addRandomAffixes(item, item.level, 3, 5);', 'this.addRandomAffixes(item, level, 1, 2);',
    'this.addRandomAffixes(item, item.level, 2, 3);',
    'const minTier = level < 8 ? 1 : level < 18 ? 1 : level < 28 ? 2 : level < 38 ? 3 : 4;',
    'const maxTier = level < 8 ? 2 : level < 18 ? 3 : level < 28 ? 4 : 5;',
    'if (a.levelReq > level + 5) return false;',
    'if (a.tier < Math.max(1, minTier - 1) || a.tier > Math.min(5, maxTier + 1)) return false;',
    'const weight = inRange ? 3 : 1;', 'const wantPrefix = prefixCount <= suffixCount;',
    'const levelScale = Math.max(0.6, Math.min(1.5, item.level / 35));',
    'const available = Consumables.filter(c => c.levelReq <= level + 5);', 'quantity: randomInt(1, 3),',
    'const available = Gems.filter(g => g.levelReq <= level + 5);');
  assertSource(ZS, 'this.time.delayedCall(60000, () => {', 'this.time.delayedCall(30000, () => {',
    'if (this.player.autoLootMode !== \'off\' && time - this.lastAutoLootCheck > 300) {',
    'const luckBonus = this.player.stats.lck + (homeBonus[\'magicFind\'] ?? 0)');
  // Treasure-cache drops (dropLootAtPosition, ZoneScene.ts:3684-3717): no despawn timer, no ITEM_DROPPED, +-10 / +-5 px
  // visual jitter, a 400 ms fall-in from 30 px above.
  assertSource(ZS, 'const offsetX = (Math.random() - 0.5) * 20 * DPR;', 'const offsetY = (Math.random() - 0.5) * 10 * DPR;',
    'const container = this.add.container(finalX, finalY - 30 * DPR);', 'y: finalY, duration: 400, ease: \'Bounce.easeOut\',');
  assert(leyFruitDropChance(true) === 0.12 && leyFruitDropChance(false) === 0.015, 'ley fruit chance');
  // Quest pick-one gear (QuestRewards.ts:13-91).
  const q = (level: number, category: 'main' | 'side', choiceQuality?: 'magic' | 'rare' | 'legendary') =>
    ({ level, category, rewards: { exp: 0, gold: 0, choiceQuality } }) as unknown as QuestDefinition;
  assert(QuestRewards.rewardItemLevel(q(7, 'main'), 3) === 7 && QuestRewards.rewardItemLevel(q(7, 'main'), 20) === 12
    && QuestRewards.rewardItemLevel(q(7, 'main'), 9) === 9, 'rewardItemLevel');
  assert(QuestRewards.rewardChoiceQuality(q(1, 'main')) === 'rare' && QuestRewards.rewardChoiceQuality(q(1, 'side')) === 'magic'
    && QuestRewards.rewardChoiceQuality(q(1, 'side', 'legendary')) === 'legendary', 'rewardChoiceQuality');
  assertSource('src/systems/QuestRewards.ts', 'const types = CLASS_WEAPON_TYPES[classId] ?? [\'sword\'];', '.slice(0, 3);',
    'export const FALLBACK_COLLECT_CHANCE = 0.25;');
  return {
    file: 'loot_rules.json',
    source: ['src/systems/LootSystem.ts', ZS, 'src/systems/PetSystem.ts', 'src/systems/QuestRewards.ts'],
    data: {
      qualityOrder: ['normal', 'magic', 'rare', 'legendary', 'set'],
      drops: {
        equipmentChance: { normal: 40, elite: 80, luckFactor: 0.5 },
        eliteSecondChance: { base: 50, luckFactor: 0.5 },
        affixThirdDrop: { minAffixBonus: 10, base: 30, luckFactor: 0.5, affixFactor: 1 },
        miniBossFloor: { zone: 'magic', subDungeon: 'rare' },
        consumableChance: 30,
        gemChance: { base: 5, luckFactor: 0.1, affixFactor: 0.3, note: '5 + (luck*0.5)*0.2 + affix*0.3' },
        leyFruitChance: { elite: leyFruitDropChance(true), other: leyFruitDropChance(false), itemId: 'c_ley_fruit' },
        chancesArePercent: true,
      },
      luckInput: {
        web: ['player.stats.lck (raw)', 'homestead + pet magicFind', 'labyrinth floor magicFindBonus'],
        port: ['gear lck', 'gear magicFind'],
        decision: 'I1 (gear lck / magicFind count, same x0.5 / x0.3 coefficients)',
      },
      qualityRoll: {
        luckFactor: 0.3, eliteBonus: 15,
        thresholds: QUALITY_THRESHOLDS,
        formula: 'r=rand*100; lm=luck*0.3; em=elite?15:0; first t with r < base + lm*t.lm + em*t.em + affix*t.affix + (level>20 ? levelOver20 : 0); else normal',
      },
      baseWindows: { equipment: [-10, 3], wide: [-20, 5], setPiece: [-15, 5], minLevel: 1 },
      levelGate: { consumable: 5, gem: 5, affix: 5 },
      affixCounts: { magic: [1, 2], rare: [3, 4], genericLegendary: [3, 5], setPieceExtra: [1, 2], genericSet: [2, 3],
        difficultyExtraAddsToMax: true },
      affixTierBands: [
        { belowLevel: 8, tiers: [1, 2] }, { belowLevel: 18, tiers: [1, 3] }, { belowLevel: 28, tiers: [2, 4] },
        { belowLevel: 38, tiers: [3, 5] }, { belowLevel: null, tiers: [4, 5] },
      ],
      affixTierSlack: 1,
      affixWeight: { inBand: 3, outOfBand: 1 },
      affixAlternation: 'prefix when prefixCount <= suffixCount, else suffix',
      legendaryLevelScale: { divisor: 35, min: 0.6, max: 1.5, rounding: 'Math.round' },
      droppedConsumable: { level: 1, quantity: [1, 3] },
      droppedGem: { level: 1, quantity: 1 },
      groundItemLifetimeMs: 60000,
      potionPickupLifetimeMs: 30000,
      pickupRadiusSq: 4,
      clickHitBoxTiles: 1.5,
      autoLootIntervalMs: 300,
      cacheDrop: { despawns: false, itemDroppedEvent: false, jitterPx: [20, 10], fallInMs: 400, fallHeightPx: 30 },
      questRewards: {
        classWeaponTypes: plain(QuestRewards.CLASS_WEAPON_TYPES),
        unknownClassWeaponTypes: ['sword'],
        shieldClasses: [...exp<Set<string>>(QuestRewards, 'SHIELD_CLASSES')],
        itemLevel: 'max(quest.level, min(playerLevel, quest.level + 5))',
        levelHeadroom: 2,
        // Port (DECISIONS I3 enforces levelReq on equip): usable = levelReq <= min(itemLevel + 2, heroLevel), so a
        // pick-one reward is always equippable at turn-in (the hero level only rises after the card was generated).
        capUsableAtHeroLevel: true,
        topCandidates: 3,
        quality: { main: 'rare', side: 'magic', override: 'rewards.choiceQuality' },
        fallbackCollectChance: QuestRewards.FALLBACK_COLLECT_CHANCE,
      },
    },
    counts: { qualityThresholds: QUALITY_THRESHOLDS.length },
  };
}

// ── crafting.json ──────────────────────────────────────────────────────────

function crafting(): TableResult {
  const affixRange = exp<Record<string, [number, number]>>(Crafting, 'AFFIX_RANGE');
  // Costs per action/quality, read from craftCost on probe items (CraftingSystem.ts:140-160).
  const costs: Record<string, unknown> = {};
  const U = Crafting.craftGoldUnit(10);
  assert(U === 6 * (10 + 5) && Crafting.craftGoldUnit(0) === 36 && Crafting.craftGoldUnit(7.9) === 72, 'craftGoldUnit');
  for (const action of Crafting.CRAFT_ACTIONS) {
    const perQuality: Record<string, unknown> = {};
    for (const quality of ['normal', 'magic', 'rare', 'legendary', 'set'] as ItemQuality[]) {
      const weapon = Crafting.craftCost(action, fakeItem('w_short_sword', quality, 10));
      const accessory = Crafting.craftCost(action, fakeItem('j_copper_ring', quality, 10));
      const conv = (c: { gold: number; materials: Record<string, number> } | null) => c === null ? null : { goldUnits: c.gold / U, materials: plain(c.materials) };
      perQuality[quality] = { weaponOrArmor: conv(weapon), accessory: conv(accessory) };
    }
    costs[action] = perQuality;
  }
  assert(Crafting.craftCost('salvage', fakeItem('c_hp_potion_s', 'normal', 1)) === null, 'non-equipment → null');
  // Salvage yield coefficients, checked against salvageYield.
  const yieldRule = {
    scrap: { base: 1, perLevelDiv: 12, normalBonus: 1 },
    magic: { dust: { base: 1, perLevelDiv: 20 } },
    rare: { dust: { base: 1, perLevelDiv: 20 }, essence: { base: 1, perLevelDiv: null } },
    legendaryOrSet: { dust: { base: 2, perLevelDiv: 20 }, essence: { base: 2, perLevelDiv: 25 } },
  };
  for (const L of [0, 1, 11, 12, 24, 25, 40, 60]) {
    const lv = Math.max(1, L || 1);
    const y = (q: ItemQuality) => Crafting.salvageYield(fakeItem('w_short_sword', q, L));
    assert(y('normal').m_scrap === 1 + Math.floor(lv / 12) + 1 && y('magic').m_scrap === 1 + Math.floor(lv / 12), 'scrap');
    assert(y('magic').m_dust === 1 + Math.floor(lv / 20) && y('rare').m_essence === 1, 'dust/essence');
    assert(y('set').m_dust === 2 + Math.floor(lv / 20) && y('legendary').m_essence === 2 + Math.floor(lv / 25), 'legendary yield');
  }
  return {
    file: 'crafting.json',
    source: ['src/systems/CraftingSystem.ts'],
    data: {
      actions: [...Crafting.CRAFT_ACTIONS],
      materials: [...Crafting.MATERIAL_IDS],
      goldUnit: { mul: 6, add: 5, levelMin: 1, formula: '6 * (max(1, floor(level)) + 5)' },
      costs,
      salvageYield: yieldRule,
      rerollAffixCounts: plain(affixRange),
      upgradeTargets: { normal: Crafting.upgradeTarget('normal'), magic: Crafting.upgradeTarget('magic') },
      maxItemSockets: Crafting.MAX_ITEM_SOCKETS,
      maxBonusSockets: Crafting.MAX_BONUS_SOCKETS,
      bagCapacity: Crafting.CRAFT_BAG_CAPACITY,
    },
    counts: { actions: Crafting.CRAFT_ACTIONS.length },
  };
}

export function exportItems(): TableResult[] {
  return [itemBases(), affixes(), sets(), legendaries(), economy(), lootRules(), crafting()];
}

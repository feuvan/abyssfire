import { describe, expect, it } from 'vitest';
import { HomesteadSystem } from '../systems/HomesteadSystem';
import {
  BLESSING_DURATION_MS, BUILDINGS, EXPEDITION_OPTIONS, LEY_FRUIT_ID, TOWER_UNLOCK_QUEST,
  blessingCost, blessingStats, buildingStage, embersForKill, embersForQuest, gardenCapacity, gardenInterval,
  gemCombineBlock, gemCombineGold, maxCombineTier, nextGemId, rollExpeditionReward, rollGardenYield,
} from '../data/homestead';
import { AllQuests } from '../data/quests/all_quests';
import { AllMaps } from '../data/maps';
import { NPCDefinitions } from '../data/npcs';
import { TOWER_PLOTS, TOWER_PORTAL, TOWER_MEADOW } from '../data/maps/ember_tower';
import type { SaveData } from '../data/types';

const CHAPTER_FINALES = ['q_secure_plains', 'q_seal_dark_source', 'q_kill_stone_guardian', 'q_seal_fire_rift', 'q_collect_demon_essence'];

function seq(...values: number[]): () => number {
  let i = 0;
  return () => values[i++ % values.length];
}

describe('Ember Tower unlocks', () => {
  it('ties every story wing to an existing main quest and an ally NPC', () => {
    for (const b of BUILDINGS) {
      if (!b.unlockQuest) continue;
      const q = AllQuests.find(x => x.id === b.unlockQuest);
      expect(q?.category, b.id).toBe('main');
      expect(NPCDefinitions[b.allyNpc!], b.allyNpc).toBeDefined();
    }
    expect(BUILDINGS.filter(b => b.unlockQuest).map(b => b.unlockQuest).sort()).toEqual([...CHAPTER_FINALES].sort());
    expect(BUILDINGS.find(b => b.id === 'altar')?.unlockQuest).toBe('q_collect_demon_essence');
    expect(BUILDINGS.find(b => b.id === 'warehouse')?.unlockQuest).toBeUndefined();
  });

  it('opens the tower after 篝火营地 and each wing after its chapter finale', () => {
    const hs = new HomesteadSystem();
    expect(hs.tower.towerUnlocked).toBe(false);
    expect(hs.tower.isBuildingUnlocked('warehouse')).toBe(true);
    expect(hs.tower.isBuildingUnlocked('herb_garden')).toBe(false);

    expect(hs.tower.syncUnlocks(['q_kill_slimes', 'q_kill_goblins', TOWER_UNLOCK_QUEST])).toEqual([]);
    expect(hs.tower.towerUnlocked).toBe(true);

    expect(hs.tower.syncUnlocks([TOWER_UNLOCK_QUEST, 'q_secure_plains'])).toEqual(['herb_garden']);
    expect(hs.tower.isBuildingUnlocked('herb_garden')).toBe(true);
    // The ally restores the wing: level 1 for free.
    expect(hs.getBuildingLevel('herb_garden')).toBe(1);
    expect(hs.tower.isBuildingUnlocked('pet_house')).toBe(false);

    const fresh = hs.tower.syncUnlocks([TOWER_UNLOCK_QUEST, ...CHAPTER_FINALES]);
    expect(fresh.sort()).toEqual(['altar', 'gem_workshop', 'pet_house', 'training_ground']);
    // Syncing again reports nothing new.
    expect(hs.tower.syncUnlocks([TOWER_UNLOCK_QUEST, ...CHAPTER_FINALES])).toEqual([]);
    // The warehouse is not a story wing: it stays at 0 until upgraded.
    expect(hs.getBuildingLevel('warehouse')).toBe(0);
  });

  it('keeps an old save\'s building levels and derives its unlocks from completed quests', () => {
    const hs = new HomesteadSystem();
    hs.buildings = { herb_garden: 4, gem_workshop: 2, altar: 0 };
    hs.tower.load(undefined);
    hs.tower.syncUnlocks([TOWER_UNLOCK_QUEST, 'q_secure_plains', 'q_seal_dark_source', 'q_kill_stone_guardian']);
    expect(hs.getBuildingLevel('herb_garden')).toBe(4);
    expect(hs.getBuildingLevel('gem_workshop')).toBe(2);
    expect(hs.getBuildingLevel('pet_house')).toBe(1);
    expect(hs.tower.isBuildingUnlocked('altar')).toBe(false);
    expect(hs.getBuildingLevel('altar')).toBe(0);
  });

  it('draws each wing as ruin, restored or thriving', () => {
    const garden = BUILDINGS.find(b => b.id === 'herb_garden')!;
    const altar = BUILDINGS.find(b => b.id === 'altar')!;
    expect(buildingStage(garden, 3, false)).toBe(0);
    expect(buildingStage(garden, 0, true)).toBe(0);
    expect(buildingStage(garden, 1, true)).toBe(1);
    expect(buildingStage(garden, 2, true)).toBe(1);
    expect(buildingStage(garden, 3, true)).toBe(2);
    expect(buildingStage(altar, 1, true)).toBe(1);
    expect(buildingStage(altar, 2, true)).toBe(2);
  });
});

describe('upgrade costs (gold + embers)', () => {
  it('charges embers and returns the gold cost', () => {
    const hs = new HomesteadSystem();
    hs.tower.syncUnlocks([TOWER_UNLOCK_QUEST, 'q_secure_plains']);
    expect(hs.getUpgradeCost('herb_garden')).toEqual({ gold: 250, embers: 5 });
    expect(hs.canUpgrade('herb_garden', 1000)).toBe(false); // no embers yet
    hs.tower.addEmbers(7);
    expect(hs.canUpgrade('herb_garden', 100)).toBe(false); // not enough gold
    expect(hs.canUpgrade('herb_garden', 250)).toBe(true);
    expect(hs.upgrade('herb_garden')).toBe(250);
    expect(hs.getBuildingLevel('herb_garden')).toBe(2);
    expect(hs.tower.embers).toBe(2);
  });

  it('refuses locked wings and maxed wings', () => {
    const hs = new HomesteadSystem();
    hs.tower.addEmbers(999);
    expect(hs.canUpgrade('altar', 99999)).toBe(false);
    expect(hs.upgrade('altar')).toBe(0);
    hs.buildings.warehouse = 5;
    expect(hs.getUpgradeCost('warehouse')).toBeNull();
    expect(hs.upgrade('warehouse')).toBe(0);
    // The warehouse is always open; its first levels are gold only.
    hs.buildings.warehouse = 0;
    expect(hs.getUpgradeCost('warehouse')).toEqual({ gold: 100, embers: 0 });
  });

  it('keeps the old building bonuses', () => {
    const hs = new HomesteadSystem();
    hs.buildings = { herb_garden: 2, training_ground: 3, gem_workshop: 1, warehouse: 2, altar: 1 };
    const b = hs.getTotalBonuses();
    expect(b.potionDiscount).toBe(10);
    expect(b.mercExpBonus).toBe(15);
    expect(b.gemBonus).toBe(2);
    expect(b.stashSlots).toBe(20);
    expect(b.altarBonus).toBe(3);
    expect(hs.getTrainingGroundBonus()).toBe(15);
  });
});

describe('embers', () => {
  it('come from bosses, mini-bosses, affixed elites and quests', () => {
    expect(embersForKill({ elite: true }, 0)).toBe(5);
    expect(embersForKill({ isMiniBoss: true }, 0)).toBe(3);
    expect(embersForKill({}, 2)).toBe(1);
    expect(embersForKill({}, 0)).toBe(0);
    expect(embersForQuest({ category: 'main', rewards: {} })).toBe(2);
    expect(embersForQuest({ category: 'side', rewards: {} })).toBe(1);
    expect(embersForQuest({ category: 'main', rewards: { embers: 15 } })).toBe(15);
    for (const id of CHAPTER_FINALES) expect(AllQuests.find(q => q.id === id)?.rewards.embers, id).toBeGreaterThan(0);
  });
});

describe('herb garden', () => {
  it('grows one item every few kills, faster with level, up to its capacity', () => {
    expect(gardenInterval(1)).toBe(14);
    expect(gardenInterval(5)).toBe(6);
    expect(gardenCapacity(0)).toBe(0);
    expect(gardenCapacity(2)).toBe(12);

    const hs = new HomesteadSystem();
    // Locked: nothing grows.
    for (let i = 0; i < 30; i++) expect(hs.tower.onKillGarden()).toBeNull();
    hs.tower.syncUnlocks(['q_secure_plains']);
    const rng = seq(0.9, 0.1); // potion, hp
    const grown: string[] = [];
    for (let i = 0; i < 14; i++) {
      const g = hs.tower.onKillGarden(rng);
      if (g) grown.push(g);
    }
    expect(grown).toEqual(['c_hp_potion_s']);
    expect(hs.tower.gardenStockCount()).toBe(1);

    for (let i = 0; i < 14 * 20; i++) hs.tower.onKillGarden(rng);
    expect(hs.tower.gardenStockCount()).toBe(gardenCapacity(1));
  });

  it('rolls ley fruit and better potions as it grows', () => {
    expect(rollGardenYield(1, seq(0.05))).toBe(LEY_FRUIT_ID);
    expect(rollGardenYield(1, seq(0.9, 0.1))).toBe('c_hp_potion_s');
    expect(rollGardenYield(3, seq(0.9, 0.1))).toBe('c_hp_potion_m');
    expect(rollGardenYield(5, seq(0.9, 0.1))).toBe('c_hp_potion_l');
    expect(rollGardenYield(5, seq(0.9, 0.9))).toBe('c_mp_potion_m');
  });

  it('harvests everything and takes back what did not fit', () => {
    const hs = new HomesteadSystem();
    hs.tower.garden.stock = { c_hp_potion_s: 3, [LEY_FRUIT_ID]: 1 };
    expect(hs.tower.harvest()).toEqual({ c_hp_potion_s: 3, [LEY_FRUIT_ID]: 1 });
    expect(hs.tower.gardenStockCount()).toBe(0);
    hs.tower.returnToGarden('c_hp_potion_s', 2);
    expect(hs.tower.garden.stock).toEqual({ c_hp_potion_s: 2 });
  });
});

describe('gem workshop', () => {
  it('combines three gems into the next tier, capped by the workshop level and hero level', () => {
    expect(nextGemId('g_ruby_1')).toBe('g_ruby_2');
    expect(nextGemId('g_ruby_3')).toBeNull();
    expect(nextGemId('g_diamond_4')).toBe('g_diamond_5');
    expect(nextGemId('m_scrap')).toBeNull();
    expect(maxCombineTier(0)).toBe(0);
    expect(maxCombineTier(1)).toBe(2);
    expect(maxCombineTier(5)).toBe(5);

    expect(gemCombineBlock('g_ruby_1', 3, 0, 20, 9999)).toBe('workshop');
    expect(gemCombineBlock('g_ruby_1', 3, 1, 10, 9999)).toBe('level'); // g_ruby_2 needs level 15
    expect(gemCombineBlock('g_ruby_1', 2, 1, 20, 9999)).toBe('count');
    expect(gemCombineBlock('g_ruby_1', 3, 1, 20, 10)).toBe('gold');
    expect(gemCombineBlock('g_ruby_1', 3, 1, 20, gemCombineGold(2))).toBeNull();
    expect(gemCombineBlock('g_ruby_2', 3, 1, 40, 9999)).toBe('workshop'); // tier 3 needs level 2
    expect(gemCombineBlock('g_ruby_2', 3, 2, 40, 9999)).toBeNull();
    expect(gemCombineBlock('g_ruby_3', 9, 5, 50, 9999)).toBe('noNext');
  });
});

describe('caravan expeditions', () => {
  function withPost(level = 1): HomesteadSystem {
    const hs = new HomesteadSystem();
    hs.tower.syncUnlocks(['q_seal_fire_rift']);
    hs.buildings.training_ground = level;
    return hs;
  }

  it('sends one idle pet at a time', () => {
    const locked = new HomesteadSystem();
    expect(locked.tower.expeditionBlock('pet_owl', ['pet_owl'], null, 'short')).toBe('locked');
    const hs = withPost();
    expect(hs.tower.expeditionBlock('pet_owl', [], null, 'short')).toBe('noPet');
    expect(hs.tower.expeditionBlock('pet_owl', ['pet_owl'], 'pet_owl', 'short')).toBe('active');
    expect(hs.tower.expeditionBlock('pet_owl', ['pet_owl'], null, 'nowhere')).toBe('option');
    expect(hs.tower.sendExpedition('pet_owl', 'short', ['pet_owl', 'pet_cat'], 'pet_cat')).toBe(true);
    expect(hs.tower.isPetAway('pet_owl')).toBe(true);
    expect(hs.tower.expeditionBlock('pet_cat', ['pet_owl', 'pet_cat'], null, 'short')).toBe('busy');
  });

  it('comes back after enough kills', () => {
    const hs = withPost(2);
    hs.tower.sendExpedition('pet_owl', 'short', ['pet_owl'], null);
    const need = EXPEDITION_OPTIONS.find(o => o.id === 'short')!.killsRequired;
    expect(hs.tower.claimExpedition(20)).toBeNull();
    let back = false;
    for (let i = 0; i < need; i++) back = hs.tower.onKillExpedition() || back;
    expect(back).toBe(true);
    const r = hs.tower.claimExpedition(20, seq(0.5))!;
    expect(r.petId).toBe('pet_owl');
    expect(r.embers).toBe(8); // 4 + 2 × post level 2
    expect(r.gold).toBeGreaterThan(0);
    expect(r.items.length).toBeGreaterThan(0);
    expect(hs.tower.embers).toBe(8);
    expect(hs.tower.expedition).toBeNull();
    expect(hs.tower.isPetAway('pet_owl')).toBe(false);
  });

  it('comes back when its time runs out', () => {
    const hs = withPost();
    hs.tower.sendExpedition('pet_cat', 'long', ['pet_cat'], null);
    const opt = EXPEDITION_OPTIONS.find(o => o.id === 'long')!;
    expect(hs.tower.tick(opt.durationMs - 1).expeditionReturned).toBe(false);
    expect(hs.tower.tick(1).expeditionReturned).toBe(true);
    expect(hs.tower.expeditionDone).toBe(true);
  });

  it('brings a gem and ley fruit back from a long journey', () => {
    const r = rollExpeditionReward('long', 3, 32, seq(0.1));
    expect(r.embers).toBe(22);
    expect(r.items.map(i => i.itemId)).toEqual(['g_ruby_3', LEY_FRUIT_ID]);
  });
});

describe('altar blessings', () => {
  function withAltar(level = 1): HomesteadSystem {
    const hs = new HomesteadSystem();
    hs.tower.syncUnlocks(['q_collect_demon_essence']);
    hs.buildings.altar = level;
    return hs;
  }

  it('costs embers and scales with the altar level', () => {
    expect(blessingCost(1)).toBe(10);
    expect(blessingCost(3)).toBe(20);
    expect(blessingStats('ember_edge', 1)).toEqual({ damagePercent: 10, critRate: 3 });
    expect(blessingStats('ember_edge', 3)).toEqual({ damagePercent: 20, critRate: 6 });
    expect(blessingStats('nope', 1)).toEqual({});

    expect(new HomesteadSystem().tower.blessingBlock('ember_edge')).toBe('locked');
    const hs = withAltar(2);
    expect(hs.tower.blessingBlock('ember_edge')).toBe('embers');
    hs.tower.addEmbers(20);
    expect(hs.tower.blessingBlock('nope')).toBe('unknown');
    expect(hs.tower.buyBlessing('hearth_ward')).toBe(true);
    expect(hs.tower.embers).toBe(5);
    expect(hs.tower.blessingStats()).toEqual(blessingStats('hearth_ward', 2));
  });

  it('lasts until the next return to the tower or until its time runs out', () => {
    const hs = withAltar();
    hs.tower.addEmbers(30);
    hs.tower.buyBlessing('ley_fortune');
    expect(hs.tower.tick(BLESSING_DURATION_MS - 1).blessingEnded).toBe(false);
    expect(hs.tower.tick(1).blessingEnded).toBe(true);
    expect(hs.tower.blessingStats()).toEqual({});

    hs.tower.buyBlessing('swift_flame');
    expect(hs.tower.onEnterTower()).toBe(true);
    expect(hs.tower.blessing).toBeNull();
    expect(hs.tower.onEnterTower()).toBe(false);
  });
});

describe('save and migration', () => {
  it('round-trips the tower state', () => {
    const hs = new HomesteadSystem();
    hs.tower.syncUnlocks([TOWER_UNLOCK_QUEST, 'q_secure_plains', 'q_seal_fire_rift', 'q_collect_demon_essence']);
    hs.tower.addEmbers(42);
    hs.tower.garden = { progress: 3, stock: { c_hp_potion_m: 2 } };
    hs.tower.sendExpedition('pet_owl', 'long', ['pet_owl'], null);
    hs.tower.buyBlessing('ember_edge');
    hs.tower.towerReturn = { mapId: 'twilight_forest', col: 18, row: 55 };
    const data = JSON.parse(JSON.stringify(hs.tower.toSave()));

    const hs2 = new HomesteadSystem();
    hs2.tower.load(data);
    expect(hs2.tower.toSave()).toEqual(hs.tower.toSave());
    expect(hs2.tower.embers).toBe(32);
  });

  it('gives saves from before the tower safe defaults', () => {
    const old: SaveData['homestead'] = { buildings: { warehouse: 2 }, pets: [] };
    const hs = new HomesteadSystem();
    hs.buildings = old.buildings;
    hs.tower.load(old);
    expect(hs.tower.embers).toBe(0);
    expect(hs.tower.garden).toEqual({ progress: 0, stock: {} });
    expect(hs.tower.expedition).toBeNull();
    expect(hs.tower.blessing).toBeNull();
    expect(hs.tower.towerReturn).toBeNull();
    expect(hs.getBuildingLevel('warehouse')).toBe(2);
  });

  it('drops malformed fields', () => {
    const hs = new HomesteadSystem();
    hs.tower.load({
      embers: Number.NaN,
      garden: { progress: -4, stock: { c_hp_potion_s: -1, c_mp_potion_s: 2 } },
      expedition: { petId: 'pet_owl', optionId: 'moon', kills: 1, killsRequired: 3, remainingMs: 5 },
      blessing: { id: 'unknown', level: 1, remainingMs: 1000 },
      towerReturn: { mapId: 'emerald_plains', col: 5, row: 6 },
    });
    expect(hs.tower.embers).toBe(0);
    expect(hs.tower.garden).toEqual({ progress: 0, stock: { c_mp_potion_s: 2 } });
    expect(hs.tower.expedition).toBeNull();
    expect(hs.tower.blessing).toBeNull();
    expect(hs.tower.towerReturn).toEqual({ mapId: 'emerald_plains', col: 5, row: 6 });
  });

  it('resets with a new game', () => {
    const hs = new HomesteadSystem();
    hs.buildings.warehouse = 3;
    hs.tower.addEmbers(5);
    hs.resetState();
    expect(hs.buildings).toEqual({});
    expect(hs.tower.embers).toBe(0);
  });
});

describe('ember_tower map', () => {
  const map = AllMaps.ember_tower;

  it('is a small, monster-free zone outside the progression', () => {
    expect(map).toBeDefined();
    expect(map.cols).toBe(48);
    expect(map.rows).toBe(48);
    expect(map.spawns).toEqual([]);
    expect(map.exits).toEqual([]);
  });

  it('keeps every plot, ally, the portal and the meadow reachable from the start', () => {
    const seen = map.collisions.map(r => r.map(() => false));
    const stack: [number, number][] = [[map.playerStart.col, map.playerStart.row]];
    seen[map.playerStart.row][map.playerStart.col] = true;
    while (stack.length) {
      const [c, r] = stack.pop()!;
      for (const [dc, dr] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) {
        const nc = c + dc, nr = r + dr;
        if (map.collisions[nr]?.[nc] && !seen[nr][nc]) { seen[nr][nc] = true; stack.push([nc, nr]); }
      }
    }
    const reach = (col: number, row: number) => [[0, 0], [1, 0], [-1, 0], [0, 1], [0, -1], [1, 1]].some(([dc, dr]) => seen[row + dr]?.[col + dc]);
    for (const [id, plot] of Object.entries(TOWER_PLOTS)) {
      expect(seen[plot.npc.row][plot.npc.col], `${id} ally spot`).toBe(true);
      expect(reach(plot.col, plot.row + 1) || reach(plot.col + 2, plot.row + 1), `${id} plot`).toBe(true);
    }
    expect(seen[TOWER_PORTAL.row][TOWER_PORTAL.col]).toBe(true);
    expect(seen[TOWER_MEADOW.row][TOWER_MEADOW.col]).toBe(true);
    for (const n of map.fieldNpcs ?? []) expect(NPCDefinitions[n.npcId], n.npcId).toBeDefined();
  });
});

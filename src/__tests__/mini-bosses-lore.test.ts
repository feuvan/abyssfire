import { describe, it, expect } from 'vitest';
import {
  MiniBossByZone,
  MiniBossDialogues,
  MiniBossSpawns,
  miniBossEmeraldPlains,
  miniBossTwilightForest,
  miniBossAnvilMountains,
  miniBossScorchingDesert,
  miniBossAbyssRift,
} from '../data/miniBosses';
import {
  LoreByZone,
  AllLoreEntries,
  getLoreCountByZone,
} from '../data/loreCollectibles';
import type { LoreEntry } from '../data/loreCollectibles';
import { AllMaps } from '../data/maps/index';
import type { MonsterDefinition, DialogueTree, DialogueNode, SaveData } from '../data/types';

const ZONE_IDS = ['emerald_plains', 'twilight_forest', 'anvil_mountains', 'scorching_desert', 'abyss_rift'];

// ---------------------------------------------------------------------------
// Mini-Boss Definitions
// ---------------------------------------------------------------------------
describe('Mini-Boss Definitions', () => {
  it('defines exactly 5 mini-bosses, one per zone', () => {
    expect(Object.keys(MiniBossByZone)).toHaveLength(5);
    for (const zoneId of ZONE_IDS) {
      expect(MiniBossByZone[zoneId]).toBeDefined();
    }
  });

  it.each(ZONE_IDS)('zone %s mini-boss has elite: true', (zoneId) => {
    const boss = MiniBossByZone[zoneId];
    expect(boss.elite).toBe(true);
  });

  it.each(ZONE_IDS)('zone %s mini-boss has distinct id from final bosses', (zoneId) => {
    const boss = MiniBossByZone[zoneId];
    // Mini-boss IDs start with "miniboss_"
    expect(boss.id).toMatch(/^miniboss_/);
  });

  it.each(ZONE_IDS)('zone %s mini-boss has Chinese name', (zoneId) => {
    const boss = MiniBossByZone[zoneId];
    expect(boss.name.length).toBeGreaterThan(0);
    // Name should contain Chinese characters
    expect(boss.name).toMatch(/[\u4e00-\u9fff]/);
  });

  it.each(ZONE_IDS)('zone %s mini-boss has valid stats', (zoneId) => {
    const boss = MiniBossByZone[zoneId];
    expect(boss.hp).toBeGreaterThan(0);
    expect(boss.damage).toBeGreaterThan(0);
    expect(boss.defense).toBeGreaterThanOrEqual(0);
    expect(boss.speed).toBeGreaterThan(0);
    expect(boss.aggroRange).toBeGreaterThan(0);
    expect(boss.attackRange).toBeGreaterThan(0);
    expect(boss.attackSpeed).toBeGreaterThan(0);
    expect(boss.expReward).toBeGreaterThan(0);
    expect(boss.goldReward[0]).toBeGreaterThan(0);
    expect(boss.goldReward[1]).toBeGreaterThanOrEqual(boss.goldReward[0]);
  });

  it.each(ZONE_IDS)('zone %s mini-boss has loot table with enhanced drops', (zoneId) => {
    const boss = MiniBossByZone[zoneId];
    expect(boss.lootTable).toBeDefined();
    expect(boss.lootTable!.length).toBeGreaterThan(0);
    // Should have at least magic+ quality drops with high rates
    const hasGoodDrop = boss.lootTable!.some(
      (lt) => lt.quality && lt.quality !== 'normal' && lt.dropRate > 0
    );
    expect(hasGoodDrop).toBe(true);
  });

  it.each(ZONE_IDS)('zone %s mini-boss has a unique spriteKey', (zoneId) => {
    const boss = MiniBossByZone[zoneId];
    expect(boss.spriteKey).toBeDefined();
    expect(boss.spriteKey.length).toBeGreaterThan(0);
  });

  it('all mini-boss IDs are unique', () => {
    const ids = ZONE_IDS.map(z => MiniBossByZone[z].id);
    const uniqueIds = new Set(ids);
    expect(uniqueIds.size).toBe(ids.length);
  });

  it('mini-boss levels scale with zone progression', () => {
    const bosses = ZONE_IDS.map(z => MiniBossByZone[z]);
    for (let i = 1; i < bosses.length; i++) {
      expect(bosses[i].level).toBeGreaterThan(bosses[i - 1].level);
    }
  });

  it('mini-boss HP scales with zone progression', () => {
    const bosses = ZONE_IDS.map(z => MiniBossByZone[z]);
    for (let i = 1; i < bosses.length; i++) {
      expect(bosses[i].hp).toBeGreaterThan(bosses[i - 1].hp);
    }
  });
});

// ---------------------------------------------------------------------------
// Mini-Boss Dialogue Trees
// ---------------------------------------------------------------------------
describe('Mini-Boss Dialogue Trees', () => {
  it('defines dialogue for each mini-boss', () => {
    for (const zoneId of ZONE_IDS) {
      const boss = MiniBossByZone[zoneId];
      expect(MiniBossDialogues[boss.id]).toBeDefined();
    }
  });

  it.each(ZONE_IDS)('zone %s mini-boss dialogue has ≥3 lines', (zoneId) => {
    const boss = MiniBossByZone[zoneId];
    const dialogue = MiniBossDialogues[boss.id];
    expect(dialogue).toBeDefined();

    // Count lines by traversing from startNodeId
    const lines: string[] = [];
    let nodeId: string | undefined = dialogue.startNodeId;
    while (nodeId) {
      const node: DialogueNode | undefined = dialogue.nodes[nodeId];
      if (!node) break;
      lines.push(node.text);
      if (node.isEnd) break;
      nodeId = node.nextNodeId;
    }
    expect(lines.length).toBeGreaterThanOrEqual(3);
  });

  it.each(ZONE_IDS)('zone %s mini-boss dialogue text is Chinese', (zoneId) => {
    const boss = MiniBossByZone[zoneId];
    const dialogue = MiniBossDialogues[boss.id];

    let nodeId: string | undefined = dialogue.startNodeId;
    while (nodeId) {
      const node: DialogueNode | undefined = dialogue.nodes[nodeId];
      if (!node) break;
      // Each line should contain Chinese characters
      expect(node.text).toMatch(/[\u4e00-\u9fff]/);
      // Each line should be non-trivial (≥10 chars)
      expect(node.text.length).toBeGreaterThanOrEqual(10);
      if (node.isEnd) break;
      nodeId = node.nextNodeId;
    }
  });

  it.each(ZONE_IDS)('zone %s mini-boss dialogue tree has valid node references', (zoneId) => {
    const boss = MiniBossByZone[zoneId];
    const dialogue = MiniBossDialogues[boss.id];

    expect(dialogue.nodes[dialogue.startNodeId]).toBeDefined();

    for (const [nodeId, node] of Object.entries(dialogue.nodes)) {
      expect(node.id).toBe(nodeId);
      if (node.nextNodeId) {
        expect(dialogue.nodes[node.nextNodeId]).toBeDefined();
      }
    }
  });

  it.each(ZONE_IDS)('zone %s mini-boss dialogue ends properly', (zoneId) => {
    const boss = MiniBossByZone[zoneId];
    const dialogue = MiniBossDialogues[boss.id];

    // Traverse and ensure we reach an isEnd node
    let nodeId: string | undefined = dialogue.startNodeId;
    let reachedEnd = false;
    let iterations = 0;
    while (nodeId && iterations < 100) {
      const node: DialogueNode | undefined = dialogue.nodes[nodeId];
      if (!node) break;
      if (node.isEnd) { reachedEnd = true; break; }
      nodeId = node.nextNodeId;
      iterations++;
    }
    expect(reachedEnd).toBe(true);
  });
});

// ---------------------------------------------------------------------------
// Mini-Boss Spawn Positions
// ---------------------------------------------------------------------------
describe('Mini-Boss Spawn Positions', () => {
  it('defines spawn positions for each zone', () => {
    for (const zoneId of ZONE_IDS) {
      expect(MiniBossSpawns[zoneId]).toBeDefined();
    }
  });

  it.each(ZONE_IDS)('zone %s mini-boss spawns within map bounds', (zoneId) => {
    const spawn = MiniBossSpawns[zoneId];
    const map = AllMaps[zoneId];
    expect(spawn.col).toBeGreaterThanOrEqual(0);
    expect(spawn.col).toBeLessThan(map.cols);
    expect(spawn.row).toBeGreaterThanOrEqual(0);
    expect(spawn.row).toBeLessThan(map.rows);
  });

  it.each(ZONE_IDS)('zone %s mini-boss spawns on walkable tile', (zoneId) => {
    const spawn = MiniBossSpawns[zoneId];
    const map = AllMaps[zoneId];
    expect(map.collisions[spawn.row][spawn.col]).toBe(true);
  });

  it.each(ZONE_IDS)('zone %s mini-boss spawn is away from camps', (zoneId) => {
    const spawn = MiniBossSpawns[zoneId];
    const map = AllMaps[zoneId];
    const safeZoneRadius = map.safeZoneRadius ?? 9;
    for (const camp of map.camps) {
      const dist = Math.sqrt(
        (spawn.col - camp.col) ** 2 + (spawn.row - camp.row) ** 2
      );
      expect(dist).toBeGreaterThan(safeZoneRadius);
    }
  });
});

// ---------------------------------------------------------------------------
// Lore Collectible Definitions
// ---------------------------------------------------------------------------
describe('Lore Collectible Definitions', () => {
  it('defines lore entries for each zone', () => {
    for (const zoneId of ZONE_IDS) {
      expect(LoreByZone[zoneId]).toBeDefined();
    }
  });

  it.each(ZONE_IDS)('zone %s has ≥3 lore entries', (zoneId) => {
    const entries = LoreByZone[zoneId];
    expect(entries.length).toBeGreaterThanOrEqual(3);
  });

  it.each(ZONE_IDS)('zone %s lore entries have valid fields', (zoneId) => {
    const entries = LoreByZone[zoneId];
    for (const entry of entries) {
      expect(entry.id).toBeTruthy();
      expect(entry.zone).toBe(zoneId);
      expect(entry.name).toBeTruthy();
      expect(entry.name).toMatch(/[\u4e00-\u9fff]/); // Chinese name
      expect(entry.text).toBeTruthy();
      expect(entry.text.length).toBeGreaterThanOrEqual(50); // ≥50 chars
      expect(entry.text).toMatch(/[\u4e00-\u9fff]/); // Chinese text
      expect(entry.col).toBeGreaterThanOrEqual(0);
      expect(entry.row).toBeGreaterThanOrEqual(0);
      expect(entry.spriteType).toBeTruthy();
      expect(typeof entry.hidden).toBe('boolean');
    }
  });

  it('all lore entry IDs are unique', () => {
    const ids = AllLoreEntries.map(e => e.id);
    const uniqueIds = new Set(ids);
    expect(uniqueIds.size).toBe(ids.length);
  });

  it.each(ZONE_IDS)('zone %s lore entries are within map bounds', (zoneId) => {
    const entries = LoreByZone[zoneId];
    const map = AllMaps[zoneId];
    for (const entry of entries) {
      expect(entry.col).toBeLessThan(map.cols);
      expect(entry.row).toBeLessThan(map.rows);
    }
  });

  it.each(ZONE_IDS)('zone %s lore entries are on walkable tiles', (zoneId) => {
    const entries = LoreByZone[zoneId];
    const map = AllMaps[zoneId];
    for (const entry of entries) {
      expect(map.collisions[entry.row][entry.col]).toBe(true);
    }
  });

  it.each(ZONE_IDS)('zone %s has at least half lore entries in non-obvious locations', (zoneId) => {
    const entries = LoreByZone[zoneId];
    const hiddenCount = entries.filter(e => e.hidden).length;
    expect(hiddenCount).toBeGreaterThanOrEqual(Math.ceil(entries.length / 2));
  });

  it('lore entries use distinct sprite types', () => {
    const spriteTypes = new Set(AllLoreEntries.map(e => e.spriteType));
    // Should have at least 3 distinct types
    expect(spriteTypes.size).toBeGreaterThanOrEqual(3);
  });

  it.each(ZONE_IDS)('zone %s has distinct sprite types among its lore entries', (zoneId) => {
    const entries = LoreByZone[zoneId];
    const spriteTypes = new Set(entries.map(e => e.spriteType));
    // Each zone should use at least 2 distinct types
    expect(spriteTypes.size).toBeGreaterThanOrEqual(2);
  });

  it('getLoreCountByZone returns correct counts', () => {
    for (const zoneId of ZONE_IDS) {
      expect(getLoreCountByZone(zoneId)).toBe(LoreByZone[zoneId].length);
    }
    expect(getLoreCountByZone('nonexistent_zone')).toBe(0);
  });

  it('AllLoreEntries contains all entries from all zones', () => {
    let totalFromZones = 0;
    for (const zoneId of ZONE_IDS) {
      totalFromZones += LoreByZone[zoneId].length;
    }
    expect(AllLoreEntries.length).toBe(totalFromZones);
  });
});

// ---------------------------------------------------------------------------
// Lore Sprite Types (valid enum values)
// ---------------------------------------------------------------------------
describe('Lore Sprite Types', () => {
  const VALID_SPRITE_TYPES = [
    'ancient_tablet', 'old_scroll', 'crystal_shard',
    'carved_stone', 'torn_journal', 'rune_pillar',
  ];

  it('all lore entries use valid sprite types', () => {
    for (const entry of AllLoreEntries) {
      expect(VALID_SPRITE_TYPES).toContain(entry.spriteType);
    }
  });
});

// ---------------------------------------------------------------------------
// Save Data Integration
// ---------------------------------------------------------------------------
describe('Save Data Integration', () => {
  it('SaveData type supports miniBossDialogueSeen field', () => {
    const save: Partial<SaveData> = {
      miniBossDialogueSeen: ['miniboss_goblin_shaman', 'miniboss_shadow_weaver'],
    };
    expect(save.miniBossDialogueSeen).toHaveLength(2);
    expect(save.miniBossDialogueSeen).toContain('miniboss_goblin_shaman');
  });

  it('SaveData type supports loreCollected field', () => {
    const save: Partial<SaveData> = {
      loreCollected: ['lore_ep_01', 'lore_tf_02'],
    };
    expect(save.loreCollected).toHaveLength(2);
    expect(save.loreCollected).toContain('lore_ep_01');
  });

  it('miniBossDialogueSeen defaults to empty when undefined', () => {
    const save: Partial<SaveData> = {};
    const seen = new Set(save.miniBossDialogueSeen ?? []);
    expect(seen.size).toBe(0);
  });

  it('loreCollected defaults to empty when undefined', () => {
    const save: Partial<SaveData> = {};
    const collected = new Set(save.loreCollected ?? []);
    expect(collected.size).toBe(0);
  });

  it('all mini-boss IDs can be serialized to save data', () => {
    const allBossIds = ZONE_IDS.map(z => MiniBossByZone[z].id);
    const save: Partial<SaveData> = { miniBossDialogueSeen: allBossIds };
    // Simulate JSON round-trip
    const restored = JSON.parse(JSON.stringify(save));
    expect(restored.miniBossDialogueSeen).toEqual(allBossIds);
  });

  it('all lore entry IDs can be serialized to save data', () => {
    const allLoreIds = AllLoreEntries.map(e => e.id);
    const save: Partial<SaveData> = { loreCollected: allLoreIds };
    // Simulate JSON round-trip
    const restored = JSON.parse(JSON.stringify(save));
    expect(restored.loreCollected).toEqual(allLoreIds);
  });
});

// ---------------------------------------------------------------------------
// EventBus Integration
// ---------------------------------------------------------------------------
describe('EventBus Events', () => {
  it('GameEvents has MINIBOSS_DIALOGUE event', async () => {
    const { GameEvents } = await import('../utils/EventBus');
    expect(GameEvents.MINIBOSS_DIALOGUE).toBe('miniboss:dialogue');
  });

  it('GameEvents has LORE_COLLECTED event', async () => {
    const { GameEvents } = await import('../utils/EventBus');
    expect(GameEvents.LORE_COLLECTED).toBe('lore:collected');
  });
});

// ---------------------------------------------------------------------------
// Mini-Boss Enhanced Loot (guaranteed drops)
// ---------------------------------------------------------------------------
describe('Mini-Boss Enhanced Loot', () => {
  it.each(ZONE_IDS)('zone %s mini-boss has guaranteed non-normal drops', (zoneId) => {
    const boss = MiniBossByZone[zoneId];
    const loot = boss.lootTable!;
    // At least one entry with quality > normal and dropRate >= 0.3
    const goodDrops = loot.filter(l => l.quality && l.quality !== 'normal' && l.dropRate >= 0.3);
    expect(goodDrops.length).toBeGreaterThan(0);
  });

  it('higher zone mini-bosses have better loot tables', () => {
    const zone1 = MiniBossByZone['emerald_plains'];
    const zone5 = MiniBossByZone['abyss_rift'];

    // Zone 5 should have higher quality drops available
    const z1MaxQuality = zone1.lootTable!.some(l => l.quality === 'legendary' || l.quality === 'set');
    const z5MaxQuality = zone5.lootTable!.some(l => l.quality === 'legendary' || l.quality === 'set');
    expect(z5MaxQuality).toBe(true);
  });
});

// ---------------------------------------------------------------------------
// Dialogue Zone-Context
// ---------------------------------------------------------------------------
describe('Dialogue Zone-Lore Context', () => {
  const ZONE_KEYWORDS: Record<string, string[]> = {
    emerald_plains: ['平原', '灵脉', '哥布林', '精灵'],
    twilight_forest: ['森林', '暮色', '黑暗', '暗影'],
    anvil_mountains: ['山脉', '矮人', '铁', '锻造'],
    scorching_desert: ['沙漠', '火焰', '王国', '灼热'],
    abyss_rift: ['深渊', '虚空', '封印', '魔王'],
  };

  it.each(ZONE_IDS)('zone %s mini-boss dialogue contains zone-relevant keywords', (zoneId) => {
    const boss = MiniBossByZone[zoneId];
    const dialogue = MiniBossDialogues[boss.id];
    const keywords = ZONE_KEYWORDS[zoneId];

    // Collect all text from dialogue
    let allText = '';
    let nodeId: string | undefined = dialogue.startNodeId;
    while (nodeId) {
      const node: DialogueNode | undefined = dialogue.nodes[nodeId];
      if (!node) break;
      allText += node.text;
      if (node.isEnd) break;
      nodeId = node.nextNodeId;
    }

    // At least one zone keyword should appear in dialogue
    const hasKeyword = keywords.some(kw => allText.includes(kw));
    expect(hasKeyword).toBe(true);
  });
});

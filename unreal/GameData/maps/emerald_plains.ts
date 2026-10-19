import type { MapData } from '../types';

// Tile/collision arrays are generated at runtime by MapGenerator
// Size: 120x120 (expanded from 80x80 for deeper exploration)
// Tile types: 0=grass, 1=dirt_path, 2=stone, 3=water, 4=wall, 5=camp_ground

export const EmeraldPlainsMap: MapData = {
  id: 'emerald_plains',
  name: '翡翠平原',
  cols: 120,
  rows: 120,
  tiles: [],
  collisions: [],
  theme: 'plains',
  seed: 12345,
  spawns: [
    // Q1 — top-left quadrant
    { col: 25, row: 20, monsterId: 'slime_green', count: 8 },
    { col: 35, row: 40, monsterId: 'goblin', count: 6 },
    { col: 20, row: 45, monsterId: 'slime_green', count: 5 },
    // Q2 — top-right quadrant
    { col: 85, row: 18, monsterId: 'slime_green', count: 6 },
    { col: 95, row: 35, monsterId: 'goblin', count: 6 },
    { col: 75, row: 50, monsterId: 'goblin', count: 4 },
    // Q3 — bottom-left quadrant
    { col: 30, row: 80, monsterId: 'goblin', count: 8 },
    { col: 15, row: 95, monsterId: 'goblin_chief', count: 1 },
    // Q4 — bottom-right quadrant
    { col: 80, row: 75, monsterId: 'goblin', count: 5 },
    { col: 95, row: 90, monsterId: 'goblin', count: 6 },
  ],
  camps: [
    { col: 15, row: 15, npcs: ['blacksmith', 'merchant', 'quest_elder'] },
    { col: 95, row: 100, npcs: ['merchant'] },
  ],
  playerStart: { col: 15, row: 22 },
  exits: [
    { col: 119, row: 60, targetMap: 'twilight_forest', targetCol: 2, targetRow: 58 },
  ],
  levelRange: [1, 7] as [number, number],

  // ─── New Content: Field NPCs ──────────────────────────────────────────
  fieldNpcs: [
    { col: 50, row: 30, npcId: 'plains_herbalist' },
    { col: 70, row: 65, npcId: 'plains_wanderer' },
  ],

  // ─── New Content: Hidden Area ─────────────────────────────────────────
  hiddenAreas: [
    {
      id: 'hidden_ep_elven_cache',
      name: '精灵族秘密宝库',
      col: 108,
      row: 108,
      radius: 6,
      discoveryText: '你发现了一处被草丛掩盖的精灵族遗迹！这里似乎是远古精灵用来储存珍贵物品的秘密宝库。',
      rewards: [
        { type: 'chest', value: 'rare', col: 108, row: 108 },
        { type: 'gold_pile', value: '200', col: 110, row: 107 },
      ],
    },
  ],

  // ─── New Content: Environmental Storytelling Decorations ──────────────
  storyDecorations: [
    {
      id: 'story_ep_ruined_tower',
      name: '倒塌的精灵塔',
      description: '这座塔楼曾是精灵族的瞭望塔，如今只剩残垣断壁。石块上仍可辨认出精灵族的蔓藤纹饰，诉说着远古文明的辉煌。',
      col: 55, row: 45,
      spriteType: 'ruins',
    },
    {
      id: 'story_ep_goblin_totem',
      name: '哥布林图腾柱',
      description: '一根由杂乱骨头和兽皮拼接而成的图腾柱，散发着淡淡的腥臭。这是哥布林部落的领地标记，预示着前方有更强大的哥布林在等待。',
      col: 40, row: 70,
      spriteType: 'war_banner',
    },
    {
      id: 'story_ep_ancient_well',
      name: '干涸的古井',
      description: '一口被青苔覆盖的石砌古井，井水早已干涸。井壁上刻着精灵文字——这曾是灵脉能量涌出的泉眼，如今灵脉断裂，泉水不再。',
      col: 88, row: 55,
      spriteType: 'broken_altar',
    },
  ],
};

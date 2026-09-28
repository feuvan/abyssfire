import type { MapData } from '../types';

// Tile/collision arrays are generated at runtime by MapGenerator
// Size: 120x120 (expanded from 80x80 for deeper exploration)
// Tile types: 0=grass, 1=dirt_path, 2=stone, 3=water, 4=wall, 5=camp_ground

export const AbyssRiftMap: MapData = {
  id: 'abyss_rift',
  name: '深渊裂谷',
  cols: 120,
  rows: 120,
  tiles: [],
  collisions: [],
  theme: 'abyss',
  seed: 61725,
  bgColor: '#1a0a12',
  spawns: [
    // Q1 — top-left quadrant
    { col: 35, row: 15, monsterId: 'imp', count: 10 },
    { col: 20, row: 48, monsterId: 'lesser_demon', count: 6 },
    // Q2 — top-right quadrant
    { col: 85, row: 22, monsterId: 'imp', count: 8 },
    { col: 70, row: 38, monsterId: 'lesser_demon', count: 6 },
    { col: 95, row: 55, monsterId: 'succubus', count: 6 },
    // Q3 — bottom-left quadrant
    { col: 35, row: 85, monsterId: 'succubus', count: 6 },
    { col: 20, row: 95, monsterId: 'succubus', count: 3 },
    { col: 55, row: 105, monsterId: 'demon_lord', count: 1 },
    // Q4 — bottom-right quadrant
    { col: 60, row: 65, monsterId: 'imp', count: 6 },
    { col: 80, row: 80, monsterId: 'lesser_demon', count: 4 },
  ],
  camps: [
    { col: 15, row: 22, npcs: ['blacksmith_advanced', 'merchant_desert', 'stash', 'quest_warden'] },
    { col: 88, row: 100, npcs: ['merchant_desert'] },
  ],
  playerStart: { col: 3, row: 58 },
  exits: [
    { col: 0, row: 58, targetMap: 'scorching_desert', targetCol: 118, targetRow: 118 },
  ],
  // Rare ley-beast: the void butterfly drifts out of the rift's cracks.
  petSpawns: [
    { col: 45, row: 60, petId: 'pet_void_butterfly', chance: 0.08 },
    { col: 100, row: 35, petId: 'pet_void_butterfly', chance: 0.08 },
  ],
  levelRange: [38, 48] as [number, number],

  // ─── New Content: Field NPCs ──────────────────────────────────────────
  fieldNpcs: [
    { col: 50, row: 37, npcId: 'abyss_fallen_knight' },
    { col: 75, row: 50, npcId: 'abyss_void_researcher' },
  ],

  // ─── New Content: Hidden Area ─────────────────────────────────────────
  hiddenAreas: [
    {
      id: 'hidden_ar_sealed_chamber',
      name: '封印密室',
      col: 105,
      row: 105,
      radius: 5,
      discoveryText: '你在扭曲的空间裂缝中发现了一间被封印的密室！这里似乎是远古贤者留下的紧急避难所，其中藏有他们的遗物。',
      rewards: [
        { type: 'chest', value: 'legendary', col: 105, row: 105 },
        { type: 'gold_pile', value: '800', col: 107, row: 104 },
      ],
    },
  ],

  // ─── New Content: Sub-Dungeon Entrance ────────────────────────────────
  subDungeonEntrances: [
    {
      id: 'entrance_demon_altar',
      name: '恶魔祭坛入口',
      col: 90,
      row: 40,
      targetSubDungeon: 'sub_demon_altar',
    },
  ],

  // ─── New Content: Environmental Storytelling Decorations ──────────────
  storyDecorations: [
    {
      id: 'story_ar_corrupted_seal',
      name: '破碎的封印法阵',
      description: '一个巨大的魔法阵刻在地面上，但已被暗紫色的能量侵蚀得面目全非。法阵边缘仍残留着微弱的金色光芒——那是远古贤者封印力量的余韵。每当虚空能量脉动时，法阵就会发出痛苦的颤鸣。',
      col: 55, row: 45,
      spriteType: 'ritual_circle',
    },
    {
      id: 'story_ar_frozen_warrior',
      name: '时间冻结的战士',
      description: '一位身着全副铠甲的战士被某种力量凝固在原地，保持着挥剑劈砍的姿势。他的面容扭曲着恐惧，铠甲上刻着封印守护者的徽章。虚空的时间扭曲力量将他永远定格在了最后一战的瞬间。',
      col: 40, row: 75,
      spriteType: 'frozen_corpse',
    },
    {
      id: 'story_ar_demon_war_banner',
      name: '恶魔军旗',
      description: '一面由暗红色皮革制成的巨大旗帜，上面绘着深渊魔王的纹章。旗帜在没有风的虚空中诡异地飘动着，散发着令人不安的低沉嗡鸣。这是恶魔大军入侵人间时插下的战旗。',
      col: 85, row: 65,
      spriteType: 'war_banner',
    },
  ],
};

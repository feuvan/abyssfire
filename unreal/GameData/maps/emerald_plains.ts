import type { MapData } from '../types';

// Tile/collision arrays are generated at runtime by MapGenerator
// Size: 120x120 (expanded from 80x80 for deeper exploration)
// Tile types: 0=grass, 1=dirt_path, 2=stone, 3=water, 4=wall, 5=camp_ground

/**
 * Chapter 1 of the dark remake: 赤幡原 / The Red Field (Docs/dark-remake/chapter1.md §1, §17). The zone id stays
 * `emerald_plains` (DR3) and the generator theme stays `plains`.
 *
 * Generator inputs kept (zero churn, chapter1.md §17): the 10 spawn positions, both camps, playerStart, the exit, the
 * field-NPC tiles, the hidden area, the three story decorations (positions and spriteType), seed and theme. The one
 * golden re-baseline (DR4) adds the Pit entrance below (17,26); its other two changes are landmarks outside this file
 * (M5's defend target in ../quests/all_quests.ts, the three echo lore spots in ../loreCollectibles.ts).
 *
 * Not generator inputs: spawn monster ids and counts (55 in the same 10 entries, §17), the camp NPC lists (camp 1 adds
 * Brother Aldwin; Mag Crowe replaces the shared merchant in camp 2, §3), the presentation theme and the mood regions.
 *
 * Wave 2 (package 2F; none of it a generator input, DECISIONS DR4): the §17 W2 mixes (same counts: a mix spawns where the
 * plain entry stood, K5), their leaders (the Dead Ranks' standard-bearer, the Hollow's elite deserter), the dawn variants
 * (after `rf_chapter_dawn` the trench, the Broken Redoubt and the Dead Ranks spawn gravegnawers, §8 *Saved world state*
 * and the 2F review; not Valdras's entry)
 * and the Sheepfold's six seated dead (§10: corpse seeds for Call the Flock).
 */
export const EmeraldPlainsMap: MapData = {
  id: 'emerald_plains',
  name: '赤幡原',
  cols: 120,
  rows: 120,
  tiles: [],
  collisions: [],
  theme: 'plains',
  seed: 12345,
  // U1: art, terrain style, camp kit and mood resolve `march` first (`tree@march` before `tree`); ember_tower stays plains.
  presentationTheme: 'march',
  spawns: [
    // Q1 — top-left quadrant
    // Burial Trenches (M1); at dawn the dead lie down and the scavengers remain.
    { col: 25, row: 20, monsterId: 'risen_footman', count: 8, variants: [{ whenFlag: 'rf_chapter_dawn', monsterId: 'gravegnawer', count: 8 }] },
    { col: 35, row: 40, monsterId: 'gravegnawer', count: 8 },         // the Gnawed Field (M2)
    { col: 20, row: 45, monsterId: 'breachclot', count: 7 },          // the Seep (S1)
    // Q2 — top-right quadrant
    // the Weeping Ditch: bloats among clots
    { col: 85, row: 18, monsterId: 'cinderbloat', count: 6, mix: [{ monsterId: 'cinderbloat', count: 3 }, { monsterId: 'breachclot', count: 3 }] },
    { col: 95, row: 35, monsterId: 'oath_zealot', count: 5 },         // the Penitents' Camp
    // the Broken Redoubt: the crossbow company behind its shields; at dawn, gnawers (2F review: every standing dead lies
    // down, cs_rf_finale)
    {
      col: 75, row: 50, monsterId: 'risen_arbalest', count: 5,
      mix: [{ monsterId: 'risen_arbalest', count: 3 }, { monsterId: 'risen_shieldbearer', count: 2 }],
      variants: [{ whenFlag: 'rf_chapter_dawn', monsterId: 'gravegnawer', count: 5 }],
    },
    // Q3 — bottom-left quadrant
    // the Dead Ranks: the shield wall that never broke, led by its standard (kill it first); at dawn, gnawers
    {
      col: 30, row: 80, monsterId: 'risen_shieldbearer', count: 6,
      mix: [{ monsterId: 'risen_shieldbearer', count: 3 }, { monsterId: 'risen_footman', count: 2 }, { monsterId: 'dead_standard_bearer', count: 1 }],
      leader: { monsterId: 'dead_standard_bearer' },
      variants: [{ whenFlag: 'rf_chapter_dawn', monsterId: 'gravegnawer', count: 6 }],
    },
    { col: 15, row: 95, monsterId: 'valdras', count: 1 },             // the Breach Rim (story boss, M7)
    // Q4 — bottom-right quadrant
    // Deserters' Hollow, the Hollow Watch (monsters.md §3): an elite leader with one affix, his drummer and four more
    {
      col: 80, row: 75, monsterId: 'deserter', count: 6,
      mix: [{ monsterId: 'deserter', count: 5 }, { monsterId: 'deserter_drummer', count: 1 }],
      leader: { monsterId: 'deserter', elite: true },
    },
    // the Crowmarket midden: a bonegorger and two gnawers
    { col: 95, row: 90, monsterId: 'bonegorger', count: 3, mix: [{ monsterId: 'bonegorger', count: 1 }, { monsterId: 'gravegnawer', count: 2 }] },
  ],
  camps: [
    // 丧钟堡 / Knellwatch (the port appends the stash keeper, DECISIONS I7) and 鸦集 / Crowmarket.
    { col: 15, row: 15, npcs: ['blacksmith', 'merchant', 'quest_elder', 'knell_chaplain'] },
    { col: 95, row: 100, npcs: ['crow_trader'] },
  ],
  playerStart: { col: 15, row: 22 },
  exits: [
    { col: 119, row: 60, targetMap: 'twilight_forest', targetCol: 2, targetRow: 58 },
  ],
  levelRange: [1, 7] as [number, number],

  // ─── Field NPCs: Mother Marl's dressing station, Corran's culvert ─────
  fieldNpcs: [
    { col: 50, row: 30, npcId: 'plains_herbalist' },
    { col: 70, row: 65, npcId: 'plains_wanderer' },
  ],

  // ─── Hidden area: the Paymaster's Wagon (rewards unchanged) ───────────
  hiddenAreas: [
    {
      id: 'hidden_rf_paymaster',
      name: '军饷车',
      col: 108,
      row: 108,
      radius: 6,
      discoveryText: '车夫还被铁链锁在钱箱上。没有人来救他，也没有人来拿钱。',
      rewards: [
        { type: 'chest', value: 'rare', col: 108, row: 108 },
        { type: 'gold_pile', value: '200', col: 110, row: 107 },
      ],
    },
  ],

  // ─── The Pit of the Faithful (信者之坑, Wave 3): sealed until sub-dungeon travel (K7) ───
  // A generator input (the one re-baseline, DR4): four tiles below where the hero woke, outside camp 1's safe radius.
  subDungeonEntrances: [
    {
      id: 'entrance_rf_pit',
      name: '信者之坑',
      col: 17,
      row: 26,
      targetSubDungeon: 'sub_rf_pit',
      sealed: true,
      sealedTextKey: 'data.subDungeonSealed.entrance_rf_pit',
    },
  ],

  // ─── Story decorations (positions and spriteType kept; meshes re-pointed by the presentation theme) ───
  storyDecorations: [
    {
      id: 'story_rf_belfry',
      name: '灰麦村钟楼',
      description: '灰麦村的钟裂成两半，陷在泥里。钟锤不见了。',
      col: 55, row: 45,
      spriteType: 'ruins',
    },
    {
      id: 'story_rf_red_banner',
      name: '赤幡',
      description: '一面红旗插在盾牌堆成的土丘上。每一面盾，都朝着丧钟堡。',
      col: 40, row: 70,
      spriteType: 'war_banner',
    },
    {
      id: 'story_rf_penitents_altar',
      name: '赎罪坛',
      description: '石盆里的沟槽一路斜向裂口。槽里积着灰，和干了的黑。',
      col: 88, row: 55,
      spriteType: 'broken_altar',
    },
  ],

  // ─── The Sheepfold's six seated dead (chapter1.md §10): bodies the Corpseherd raises (they never fade; K3 seeds) ───
  corpseSeeds: [
    { col: 57, row: 54, monsterId: 'risen_footman' },
    { col: 58, row: 57, monsterId: 'risen_footman' },
    { col: 61, row: 58, monsterId: 'risen_footman' },
    { col: 63, row: 56, monsterId: 'risen_footman' },
    { col: 62, row: 52, monsterId: 'risen_footman' },
    { col: 59, row: 52, monsterId: 'risen_footman' },
  ],

  // ─── Mood regions (chapter1.md §9.1 / §15): the Breach Rim, 16 tiles around the dais ───
  regions: [
    { id: 'breach_rim', col: 15, row: 95, radius: 16, mood: 'breach_rim' },
  ],
};

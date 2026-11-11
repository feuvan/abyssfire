/**
 * Sub-dungeon definitions — separate map areas accessible via entrance tiles.
 * Each has own spawns, a mini-boss, and an exit back to the parent zone.
 * Sub-dungeon entrances are interactable objects on the parent zone map.
 *
 * Dark remake (unreal/Docs/dark-remake/, Wave 3 package 3B): this module replaces the web's `src/data/subDungeons.ts`
 * (GameData/redirects.json). The web's two generated sub-dungeons stay as copied (nothing enters them: the core does
 * not port their generator; Chapters 3 and 5 are behind the sealed gate, DECISIONS W7). Chapter 1 adds 信者之坑 / the
 * Pit of the Faithful (`sub_rf_pit`, ./maps/sub_rf_pit.ts: hand-authored, entered from the Red Field) and its guardian,
 * 掘坑官 / the Pitwarden.
 */

import type { SubDungeonMapData, MonsterDefinition } from './types';
import { SubRfPitMap } from './maps/sub_rf_pit';

// ─── Sub-Dungeon 1: Abandoned Dwarf Mine (Anvil Mountains) ─────────────
export const subDungeonDwarfMine: SubDungeonMapData = {
  id: 'sub_dwarf_mine',
  name: '废弃矮人矿道',
  parentZone: 'anvil_mountains',
  cols: 40,
  rows: 40,
  theme: 'mountain',
  seed: 77701,
  spawns: [
    { col: 12, row: 10, monsterId: 'gargoyle', count: 4 },
    { col: 25, row: 15, monsterId: 'stone_golem', count: 3 },
    { col: 10, row: 25, monsterId: 'gargoyle', count: 4 },
    { col: 30, row: 28, monsterId: 'stone_golem', count: 3 },
  ],
  miniBoss: { col: 20, row: 32, monsterId: 'sub_mine_guardian' },
  playerStart: { col: 5, row: 5 },
  exit: { col: 5, row: 3, returnCol: 35, returnRow: 92 },
  levelRange: [20, 27],
  bgColor: '#161616',
};

// ─── Sub-Dungeon 2: Demon Altar Cavern (Abyss Rift) ───────────────────
export const subDungeonDemonAltar: SubDungeonMapData = {
  id: 'sub_demon_altar',
  name: '恶魔祭坛洞窟',
  parentZone: 'abyss_rift',
  cols: 40,
  rows: 40,
  theme: 'abyss',
  seed: 88802,
  spawns: [
    { col: 10, row: 12, monsterId: 'imp', count: 6 },
    { col: 28, row: 10, monsterId: 'lesser_demon', count: 4 },
    { col: 15, row: 25, monsterId: 'succubus', count: 3 },
    { col: 30, row: 30, monsterId: 'lesser_demon', count: 4 },
  ],
  miniBoss: { col: 20, row: 35, monsterId: 'sub_altar_keeper' },
  playerStart: { col: 5, row: 5 },
  exit: { col: 5, row: 3, returnCol: 90, returnRow: 40 },
  levelRange: [40, 48],
  bgColor: '#140810',
};

/** Sub-dungeon mini-boss definitions. */
export const SubDungeonMiniBosses: Record<string, MonsterDefinition> = {
  sub_mine_guardian: {
    id: 'sub_mine_guardian',
    name: '矿道铁卫',
    level: 26,
    hp: 1300,
    damage: 42,
    defense: 38,
    speed: 28,
    aggroRange: 8,
    attackRange: 2.0,
    attackSpeed: 1500,
    expReward: 280,
    goldReward: [45, 90],
    spriteKey: 'monster_sub_mine_guardian',
    elite: true,
    isMiniBoss: true,
    isSubDungeonMiniBoss: true,
    animCategory: 'large',
    lootTable: [
      { quality: 'magic', dropRate: 0.9 },
      { quality: 'rare', dropRate: 0.5 },
      { quality: 'legendary', dropRate: 0.05 },
    ],
  },
  sub_altar_keeper: {
    id: 'sub_altar_keeper',
    name: '祭坛守卫者',
    level: 46,
    hp: 3800,
    damage: 72,
    defense: 45,
    speed: 48,
    aggroRange: 9,
    attackRange: 3.0,
    attackSpeed: 950,
    expReward: 520,
    goldReward: [120, 240],
    spriteKey: 'monster_sub_altar_keeper',
    elite: true,
    isMiniBoss: true,
    isSubDungeonMiniBoss: true,
    animCategory: 'demonic',
    lootTable: [
      { quality: 'rare', dropRate: 0.8 },
      { quality: 'legendary', dropRate: 0.12 },
      { quality: 'set', dropRate: 0.05 },
    ],
  },
  // ─── Chapter 1 (dark remake): 掘坑官 / the Pitwarden, the Pit of the Faithful's guardian (monsters.md §2.0, §2.2) ───
  // An Unburied officer with a ditching spade: he dug the pits before the battle "for whoever won't hold" (lore_rf_02)
  // and still digs one that is already full. He stands in the Pitwarden's Cut behind two shieldbearers
  // (./maps/sub_rf_pit.ts); S7 `q_rf_the_pit` kills him, and the Pit cannot be cleansed while he stands.
  // Numbers: the §2.0 row (Lv 8, 11 defence, speed 40, aggro 7, reach 1.8, a chop every 1500 ms, 20-40 gold, 110 exp)
  // but for his life and his damage (DR9 *3B review*): 1500 and 28 where the row says 420 and 17. The hero S7 sends
  // down is Lv 9-10. At 420 the frame-perfect warrior's fight lasted 1.4 s at the median; at 3B's 840 it lasted 3-7 s
  // by class and the Pit as a whole was the safest stretch of the chapter (the largest hit taken inside 0.6-2.8 % of
  // the hero's life at the median, against 7-12 % on the surface): the review asked for a Pit that registers. At
  // 1500 / 28, with his two shieldbearers now his guard (one pack: ./maps/sub_rf_pit.ts), the fight is 6 / 10 / 20 s
  // at the median (warrior / mage / rogue), the largest hit inside 3.8-7.6 % of life at the median and up to 21 % (the
  // hit cap is 35 %), Dig in every fight, a second Dig in most of the rogue's. The playthrough sweep pins floors for
  // both (the guardian's fight, the largest hit inside).
  // Clips and beats are the art lane's (Art/Export/manifest.json SK_Mon_Pitwarden `abilities`): Attack01 the chop
  // (Contact 620), Attack02 the sweep (Contact 700), Cast01 Dig (Plant 330, Contact 800).
  sub_pitwarden: {
    id: 'sub_pitwarden',
    name: '掘坑官',
    level: 8,
    hp: 1500,
    damage: 28,
    defense: 11,
    speed: 40,
    aggroRange: 7,
    attackRange: 1.8,
    attackSpeed: 1500,
    expReward: 110,
    goldReward: [20, 40],
    spriteKey: 'monster_sub_pitwarden',
    elite: true,
    isMiniBoss: true,
    isSubDungeonMiniBoss: true,
    animCategory: 'humanoid',
    family: 'unburied',
    hitCapHeroMaxHpPct: 35,
    // 3B review: one pack with his two shieldbearers (the map's `guard` entry). A guard that is provoked brings him
    // from anywhere in the Cut; his own alarm wakes the two (they stand dormant until then).
    packAggro: 12,
    abilities: [
      // 掘 Dig: he drives the spade in (Cast01; the blade bites at 330 ms) and a crimson line of 4 tiles toward where
      // the hero stood collapses 800 ms after the windup began: x1.2 and slow 30 % for 2.5 s. Every 12 s, and his
      // opener: a ranged special (rangeMax past his 2.16-tile swing reach), so it meets a hero walking in.
      {
        id: 'dig', kind: 'line', clip: 'Cast01', windupMs: 800, cooldownMs: 12000, rangeMax: 4, aim: 'heroAtWindup',
        shape: { length: 4, width: 1 }, dmgMul: 1.2, status: { type: 'slow', ms: 2500, value: 30 }, priority: 2,
      },
      // 铲扫 Spade Sweep: a 100 degree cone r2.2 toward where the hero stood, 700 ms (Attack02: the mini-boss windup
      // floor), x1.3; every 6 s.
      {
        id: 'spade_sweep', kind: 'cone', clip: 'Attack02', windupMs: 700, cooldownMs: 6000, initialCooldownMs: 2500,
        aim: 'heroAtWindup', shape: { radius: 2.2, arcDeg: 100 }, dmgMul: 1.3, priority: 1,
      },
    ],
    // His fall is the cleanse's beat: remains, never raised or eaten (the art's `corpse.guard`).
    corpse: { leaves: 'remains' },
    // 3B review: the Corpseherd's table (miniBosses.ts), no richer - he is a Lv 8 side-quest guardian, not the web's
    // Lv 26 mine guardian whose table 3B had copied. He drops it once per save (the core: a guardian killed again is
    // spent and pays nothing).
    lootTable: [
      { quality: 'magic', dropRate: 0.8 },
      { quality: 'rare', dropRate: 0.35 },
      { quality: 'legendary', dropRate: 0.03 },
    ],
  },
};

/** All sub-dungeon map data indexed by sub-dungeon ID. */
export const AllSubDungeons: Record<string, SubDungeonMapData> = {
  sub_dwarf_mine: subDungeonDwarfMine,
  sub_demon_altar: subDungeonDemonAltar,
  sub_rf_pit: SubRfPitMap,
};

/** Map from parent zone ID → sub-dungeon IDs available in that zone. */
export const SubDungeonsByZone: Record<string, string[]> = {
  anvil_mountains: ['sub_dwarf_mine'],
  abyss_rift: ['sub_demon_altar'],
  emerald_plains: ['sub_rf_pit'],
};

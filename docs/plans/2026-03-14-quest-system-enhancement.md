# Quest System Enhancement Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Enhance quest system with 47 quests (25 main + 22 side), dynamic NPC !/? indicators, minimap quest markers, improved quest tracker, full quest log panel (J key), and working progress tracking for all quest types (kill/collect/explore/talk).

**Architecture:** Extend QuestDefinition/QuestObjective types with category, questArea, and location fields. Rewrite quest data with 47 quests. Add dynamic !/? markers on quest NPCs. Enhance UIScene with minimap quest markers, multi-objective tracker, and a tabbed quest log panel. Wire up progress for collect (auto on monster kill), explore (player proximity), and talk (NPC interaction) quest types.

**Tech Stack:** Phaser 3, TypeScript, Vite

**Verification:** `npx tsc --noEmit` after each task (no test framework). `npm run dev` for visual verification.

---

## Task 1: Type Definitions

**Files:**
- Modify: `src/data/types.ts:194-224`

**Step 1: Update QuestDefinition, QuestObjective, NPCDefinition types**

In `src/data/types.ts`, replace lines 194-233 with:

```typescript
export interface QuestDefinition {
  id: string;
  name: string;
  description: string;
  zone: string;
  type: 'kill' | 'collect' | 'explore' | 'talk';
  category: 'main' | 'side';
  objectives: QuestObjective[];
  rewards: QuestReward;
  prereqQuests?: string[];
  level: number;
  questArea?: { col: number; row: number; radius: number };
}

export interface QuestObjective {
  type: 'kill' | 'collect' | 'explore' | 'talk';
  targetId: string;
  targetName: string;
  required: number;
  current: number;
  location?: { col: number; row: number; radius: number };
}

export interface QuestReward {
  exp: number;
  gold: number;
  items?: string[];
}

export interface QuestProgress {
  questId: string;
  status: 'available' | 'active' | 'completed' | 'turned_in';
  objectives: { current: number }[];
}

export interface NPCDefinition {
  id: string;
  name: string;
  type: 'blacksmith' | 'merchant' | 'quest' | 'stash';
  dialogue: string[];
  shopItems?: string[];
  quests?: string[];
}
```

**Step 2: Verify compilation**

Run: `npx tsc --noEmit`
Expected: Errors in all_quests.ts (missing `category` field). This is expected and fixed in Task 2.

**Step 3: Commit**

```bash
git add src/data/types.ts
git commit -m "feat(quest): add category, questArea, and objective location to quest types"
```

---

## Task 2: Quest Data & NPC Assignments

**Files:**
- Rewrite: `src/data/quests/all_quests.ts`
- Modify: `src/data/npcs.ts`
- Modify: `src/data/maps/twilight_forest.ts`

### Design notes

- Existing quests get `category: 'main'` or `'side'` and optionally `questArea`
- Some existing quest prereqs change due to new quests inserted into chains
- `q_talk_desert_nomad` renamed to `q_explore_desert` (changed from talk→explore, since the target NPC IS the quest giver)
- `q_report_victory` renamed to `q_secure_plains` (changed from talk→explore, same reason)
- `q_talk_hermit` kept as talk quest; add `forest_hermit` NPC to twilight_forest map
- `q_reforge_artifact` and `q_forge_seal` are collect-only (removed talk component since target is quest giver)
- For collect quests, targetIds are `mat_*` strings; progress tracked by zone-based drop chance on monster kills
- For explore quests, objectives include `location` for proximity-based completion

### Existing monsters by zone (for kill quest targetIds)

- **Emerald Plains**: slime_green, goblin, goblin_chief
- **Twilight Forest**: skeleton, zombie, werewolf, werewolf_alpha
- **Anvil Mountains**: gargoyle, stone_golem, mountain_troll
- **Scorching Desert**: fire_elemental, desert_scorpion, sandworm, phoenix
- **Abyss Rift**: imp, lesser_demon, succubus, demon_lord

**Step 1: Rewrite `src/data/quests/all_quests.ts`**

Replace entire file with:

```typescript
import type { QuestDefinition } from '../types';

export const AllQuests: QuestDefinition[] = [
  // ═══════════════════════════════════════
  // Zone 1: Emerald Plains (Lv 1-10)
  // ═══════════════════════════════════════

  // --- Main Line (protect village from goblins) ---
  {
    id: 'q_kill_slimes',
    name: '史莱姆之灾',
    description: '翡翠平原上的史莱姆正在侵蚀农田，请消灭它们以保护村庄。',
    zone: 'emerald_plains',
    type: 'kill',
    category: 'main',
    objectives: [
      { type: 'kill', targetId: 'slime_green', targetName: '绿色史莱姆', required: 10, current: 0 },
    ],
    rewards: { exp: 120, gold: 25 },
    level: 1,
    questArea: { col: 32, row: 12, radius: 12 },
  },
  {
    id: 'q_kill_goblins',
    name: '哥布林猎杀',
    description: '哥布林部落越来越嚣张了，消灭一批哥布林以震慑它们。',
    zone: 'emerald_plains',
    type: 'kill',
    category: 'main',
    objectives: [
      { type: 'kill', targetId: 'goblin', targetName: '哥布林', required: 15, current: 0 },
    ],
    rewards: { exp: 200, gold: 40 },
    prereqQuests: ['q_kill_slimes'],
    level: 3,
    questArea: { col: 34, row: 40, radius: 15 },
  },
  {
    id: 'q_explore_goblin_camp',
    name: '哥布林营地',
    description: '哥布林首领隐藏在某个营地中，前去侦察他们的大本营。',
    zone: 'emerald_plains',
    type: 'explore',
    category: 'main',
    objectives: [
      { type: 'explore', targetId: 'zone_goblin_camp', targetName: '哥布林营地', required: 1, current: 0, location: { col: 40, row: 52, radius: 8 } },
    ],
    rewards: { exp: 250, gold: 35 },
    prereqQuests: ['q_kill_goblins'],
    level: 5,
  },
  {
    id: 'q_find_goblin_chief',
    name: '首领之首',
    description: '哥布林首领藏身于平原深处，找到并击败它！',
    zone: 'emerald_plains',
    type: 'kill',
    category: 'main',
    objectives: [
      { type: 'kill', targetId: 'goblin_chief', targetName: '哥布林首领', required: 1, current: 0 },
    ],
    rewards: { exp: 500, gold: 80, items: ['w_short_sword'] },
    prereqQuests: ['q_explore_goblin_camp'],
    level: 7,
    questArea: { col: 25, row: 65, radius: 10 },
  },
  {
    id: 'q_secure_plains',
    name: '确保平原安全',
    description: '哥布林首领已被消灭，但仍需巡逻平原各处确保没有残余威胁。',
    zone: 'emerald_plains',
    type: 'explore',
    category: 'main',
    objectives: [
      { type: 'explore', targetId: 'zone_east_plains', targetName: '平原东部', required: 1, current: 0, location: { col: 65, row: 30, radius: 10 } },
      { type: 'explore', targetId: 'zone_south_plains', targetName: '平原南部', required: 1, current: 0, location: { col: 50, row: 55, radius: 10 } },
    ],
    rewards: { exp: 400, gold: 60 },
    prereqQuests: ['q_find_goblin_chief'],
    level: 8,
  },

  // --- Side Quests ---
  {
    id: 'q_collect_slime_gel',
    name: '史莱姆凝胶',
    description: '商人需要史莱姆凝胶来调制药水，帮他收集一些吧。',
    zone: 'emerald_plains',
    type: 'collect',
    category: 'side',
    objectives: [
      { type: 'collect', targetId: 'mat_slime_gel', targetName: '史莱姆凝胶', required: 8, current: 0 },
    ],
    rewards: { exp: 80, gold: 30, items: ['c_hp_potion_s'] },
    level: 2,
    questArea: { col: 45, row: 16, radius: 12 },
  },
  {
    id: 'q_herb_gathering',
    name: '草药采集',
    description: '村里的药师急需草药来治疗伤者，帮忙在平原上采集一些。',
    zone: 'emerald_plains',
    type: 'collect',
    category: 'side',
    objectives: [
      { type: 'collect', targetId: 'mat_herb', targetName: '翡翠草药', required: 5, current: 0 },
    ],
    rewards: { exp: 60, gold: 20, items: ['c_hp_potion_s'] },
    level: 2,
    questArea: { col: 20, row: 25, radius: 10 },
  },
  {
    id: 'q_lost_pendant',
    name: '遗失的挂坠',
    description: '一位村民的传家挂坠被哥布林抢走了，击败哥布林找回挂坠。',
    zone: 'emerald_plains',
    type: 'kill',
    category: 'side',
    objectives: [
      { type: 'kill', targetId: 'goblin', targetName: '哥布林', required: 8, current: 0 },
      { type: 'collect', targetId: 'mat_pendant', targetName: '传家挂坠', required: 1, current: 0 },
    ],
    rewards: { exp: 150, gold: 35 },
    prereqQuests: ['q_kill_slimes'],
    level: 4,
    questArea: { col: 56, row: 40, radius: 10 },
  },
  {
    id: 'q_bandit_trouble',
    name: '路匪横行',
    description: '平原上的哥布林开始抢劫过路商人，去商路附近清剿它们。',
    zone: 'emerald_plains',
    type: 'kill',
    category: 'side',
    objectives: [
      { type: 'kill', targetId: 'goblin', targetName: '哥布林', required: 12, current: 0 },
    ],
    rewards: { exp: 220, gold: 45 },
    prereqQuests: ['q_kill_goblins'],
    level: 6,
    questArea: { col: 50, row: 55, radius: 10 },
  },
  {
    id: 'q_rare_mushroom',
    name: '珍稀蘑菇',
    description: '平原湿地中生长着珍稀的魔力蘑菇，药师愿意高价收购。',
    zone: 'emerald_plains',
    type: 'collect',
    category: 'side',
    objectives: [
      { type: 'collect', targetId: 'mat_mushroom', targetName: '魔力蘑菇', required: 6, current: 0 },
    ],
    rewards: { exp: 100, gold: 40 },
    level: 5,
    questArea: { col: 60, row: 20, radius: 10 },
  },

  // ═══════════════════════════════════════
  // Zone 2: Twilight Forest (Lv 10-20)
  // ═══════════════════════════════════════

  // --- Main Line (investigate undead rising) ---
  {
    id: 'q_explore_forest',
    name: '暮色侦察',
    description: '暮色森林中有不祥的气息，前去侦察森林的各个区域。',
    zone: 'twilight_forest',
    type: 'explore',
    category: 'main',
    objectives: [
      { type: 'explore', targetId: 'zone_forest_north', targetName: '森林北部', required: 1, current: 0, location: { col: 26, row: 10, radius: 10 } },
      { type: 'explore', targetId: 'zone_forest_graveyard', targetName: '废弃墓地', required: 1, current: 0, location: { col: 10, row: 29, radius: 10 } },
      { type: 'explore', targetId: 'zone_forest_ruins', targetName: '古老遗迹', required: 1, current: 0, location: { col: 58, row: 26, radius: 10 } },
    ],
    rewards: { exp: 600, gold: 60 },
    level: 10,
  },
  {
    id: 'q_kill_undead',
    name: '亡灵净化',
    description: '不死生物在暮色森林中游荡，将它们送回安息之所。',
    zone: 'twilight_forest',
    type: 'kill',
    category: 'main',
    objectives: [
      { type: 'kill', targetId: 'skeleton', targetName: '骷髅战士', required: 12, current: 0 },
      { type: 'kill', targetId: 'zombie', targetName: '腐尸', required: 8, current: 0 },
    ],
    rewards: { exp: 800, gold: 75, items: ['c_hp_potion_m'] },
    prereqQuests: ['q_explore_forest'],
    level: 12,
    questArea: { col: 30, row: 30, radius: 15 },
  },
  {
    id: 'q_talk_hermit',
    name: '隐士的智慧',
    description: '森林深处住着一位隐士，他或许知道亡灵为何复苏。去找到他。',
    zone: 'twilight_forest',
    type: 'talk',
    category: 'main',
    objectives: [
      { type: 'talk', targetId: 'forest_hermit', targetName: '森林隐士', required: 1, current: 0, location: { col: 65, row: 42, radius: 5 } },
    ],
    rewards: { exp: 400, gold: 50 },
    prereqQuests: ['q_kill_undead'],
    level: 14,
    questArea: { col: 65, row: 42, radius: 8 },
  },
  {
    id: 'q_kill_werewolf_alpha',
    name: '狼王之祸',
    description: '一只凶猛的狼人首领统治着森林的黑暗深处，必须消灭它。',
    zone: 'twilight_forest',
    type: 'kill',
    category: 'main',
    objectives: [
      { type: 'kill', targetId: 'werewolf', targetName: '狼人', required: 6, current: 0 },
      { type: 'kill', targetId: 'werewolf_alpha', targetName: '狼人首领', required: 1, current: 0 },
    ],
    rewards: { exp: 1500, gold: 120, items: ['w_battle_axe'] },
    prereqQuests: ['q_talk_hermit'],
    level: 18,
    questArea: { col: 58, row: 58, radius: 10 },
  },
  {
    id: 'q_seal_dark_source',
    name: '封印黑暗之源',
    description: '隐士说亡灵源于森林深处的黑暗能量源，前去找到并净化它。',
    zone: 'twilight_forest',
    type: 'kill',
    category: 'main',
    objectives: [
      { type: 'explore', targetId: 'zone_dark_source', targetName: '黑暗之源', required: 1, current: 0, location: { col: 19, row: 64, radius: 8 } },
      { type: 'kill', targetId: 'zombie', targetName: '腐尸', required: 15, current: 0 },
    ],
    rewards: { exp: 1800, gold: 150 },
    prereqQuests: ['q_kill_werewolf_alpha'],
    level: 19,
    questArea: { col: 19, row: 64, radius: 10 },
  },

  // --- Side Quests ---
  {
    id: 'q_collect_wolf_pelts',
    name: '狼皮收集',
    description: '铁匠需要上好的狼皮来制作护甲，击败狼人收集它们的皮毛。',
    zone: 'twilight_forest',
    type: 'kill',
    category: 'side',
    objectives: [
      { type: 'kill', targetId: 'werewolf', targetName: '狼人', required: 8, current: 0 },
    ],
    rewards: { exp: 500, gold: 55 },
    level: 11,
    questArea: { col: 32, row: 48, radius: 10 },
  },
  {
    id: 'q_spider_nest',
    name: '蛛巢清剿',
    description: '森林中的蛛巢附近聚集了大量骷髅守卫，清除这些威胁。',
    zone: 'twilight_forest',
    type: 'kill',
    category: 'side',
    objectives: [
      { type: 'kill', targetId: 'skeleton', targetName: '骷髅战士', required: 10, current: 0 },
    ],
    rewards: { exp: 450, gold: 50 },
    level: 13,
    questArea: { col: 40, row: 35, radius: 10 },
  },
  {
    id: 'q_lost_scout',
    name: '失踪的斥候',
    description: '一名斥候在森林南部失踪了。前去搜索并清除附近的亡灵。',
    zone: 'twilight_forest',
    type: 'kill',
    category: 'side',
    objectives: [
      { type: 'explore', targetId: 'zone_scout_camp', targetName: '斥候营地', required: 1, current: 0, location: { col: 15, row: 50, radius: 8 } },
      { type: 'kill', targetId: 'zombie', targetName: '腐尸', required: 5, current: 0 },
    ],
    rewards: { exp: 650, gold: 65 },
    prereqQuests: ['q_explore_forest'],
    level: 15,
    questArea: { col: 15, row: 50, radius: 10 },
  },
  {
    id: 'q_ancient_relic',
    name: '古代遗物',
    description: '古老遗迹中散落着远古文明的遗物，收集它们也许能揭开森林的秘密。',
    zone: 'twilight_forest',
    type: 'collect',
    category: 'side',
    objectives: [
      { type: 'collect', targetId: 'mat_ancient_relic', targetName: '远古遗物', required: 5, current: 0 },
    ],
    rewards: { exp: 700, gold: 70 },
    prereqQuests: ['q_kill_undead'],
    level: 16,
    questArea: { col: 58, row: 26, radius: 10 },
  },
  {
    id: 'q_moonlight_herb',
    name: '月光草药',
    description: '暮色森林深处生长着珍贵的月光草药，据说有强大的治愈效果。',
    zone: 'twilight_forest',
    type: 'collect',
    category: 'side',
    objectives: [
      { type: 'collect', targetId: 'mat_moonlight_herb', targetName: '月光草药', required: 8, current: 0 },
    ],
    rewards: { exp: 550, gold: 60, items: ['c_hp_potion_m'] },
    level: 17,
    questArea: { col: 51, row: 16, radius: 10 },
  },

  // ═══════════════════════════════════════
  // Zone 3: Anvil Mountains (Lv 20-30)
  // ═══════════════════════════════════════

  // --- Main Line (reclaim dwarf ruins) ---
  {
    id: 'q_explore_dwarf_ruins',
    name: '矮人遗迹',
    description: '铁砧山脉中隐藏着古老的矮人遗迹，前去探索其秘密。',
    zone: 'anvil_mountains',
    type: 'explore',
    category: 'main',
    objectives: [
      { type: 'explore', targetId: 'zone_mine_entrance', targetName: '矿洞入口', required: 1, current: 0, location: { col: 26, row: 10, radius: 10 } },
      { type: 'explore', targetId: 'zone_forge_hall', targetName: '锻造大厅', required: 1, current: 0, location: { col: 48, row: 38, radius: 10 } },
      { type: 'explore', targetId: 'zone_throne_room', targetName: '矮人王座', required: 1, current: 0, location: { col: 45, row: 61, radius: 10 } },
    ],
    rewards: { exp: 1800, gold: 150 },
    level: 20,
  },
  {
    id: 'q_kill_gargoyles',
    name: '石翼之灾',
    description: '石像鬼盘踞在山脉的高处，威胁着所有过往的旅人。',
    zone: 'anvil_mountains',
    type: 'kill',
    category: 'main',
    objectives: [
      { type: 'kill', targetId: 'gargoyle', targetName: '石像鬼', required: 15, current: 0 },
    ],
    rewards: { exp: 2200, gold: 180 },
    prereqQuests: ['q_explore_dwarf_ruins'],
    level: 22,
    questArea: { col: 46, row: 15, radius: 15 },
  },
  {
    id: 'q_collect_dwarf_relics',
    name: '矮人遗物',
    description: '收集散落在遗迹中的矮人工艺品，也许铁匠可以利用它们。',
    zone: 'anvil_mountains',
    type: 'collect',
    category: 'main',
    objectives: [
      { type: 'collect', targetId: 'mat_dwarf_ingot', targetName: '矮人秘银锭', required: 5, current: 0 },
      { type: 'collect', targetId: 'mat_rune_fragment', targetName: '符文碎片', required: 10, current: 0 },
    ],
    rewards: { exp: 2500, gold: 200, items: ['w_claymore'] },
    prereqQuests: ['q_kill_gargoyles'],
    level: 25,
    questArea: { col: 35, row: 40, radius: 15 },
  },
  {
    id: 'q_reforge_artifact',
    name: '重铸神器',
    description: '矮人长老需要特殊材料来重铸古代神器，帮他收集秘银核心。',
    zone: 'anvil_mountains',
    type: 'collect',
    category: 'main',
    objectives: [
      { type: 'collect', targetId: 'mat_mithril_core', targetName: '秘银核心', required: 3, current: 0 },
    ],
    rewards: { exp: 2800, gold: 250 },
    prereqQuests: ['q_collect_dwarf_relics'],
    level: 27,
    questArea: { col: 55, row: 52, radius: 10 },
  },
  {
    id: 'q_kill_stone_guardian',
    name: '石之守卫',
    description: '矮人遗迹最深处的石之守卫依然忠诚地守护着宝藏，击败它。',
    zone: 'anvil_mountains',
    type: 'kill',
    category: 'main',
    objectives: [
      { type: 'kill', targetId: 'mountain_troll', targetName: '山岭巨魔', required: 1, current: 0 },
    ],
    rewards: { exp: 3500, gold: 300, items: ['a_plate_armor'] },
    prereqQuests: ['q_reforge_artifact'],
    level: 28,
    questArea: { col: 45, row: 61, radius: 8 },
  },

  // --- Side Quests ---
  {
    id: 'q_crystal_mining',
    name: '水晶矿脉',
    description: '山脉中发现了珍贵的水晶矿脉，采集水晶用于武器附魔。',
    zone: 'anvil_mountains',
    type: 'collect',
    category: 'side',
    objectives: [
      { type: 'collect', targetId: 'mat_crystal', targetName: '山脉水晶', required: 8, current: 0 },
    ],
    rewards: { exp: 1600, gold: 130 },
    level: 21,
    questArea: { col: 58, row: 16, radius: 10 },
  },
  {
    id: 'q_mountain_bandits',
    name: '山贼窝点',
    description: '石巨人占据了曾经的商路，清除它们恢复通行。',
    zone: 'anvil_mountains',
    type: 'kill',
    category: 'side',
    objectives: [
      { type: 'kill', targetId: 'stone_golem', targetName: '石巨人', required: 10, current: 0 },
    ],
    rewards: { exp: 1900, gold: 160 },
    level: 23,
    questArea: { col: 16, row: 45, radius: 10 },
  },
  {
    id: 'q_trapped_miners',
    name: '被困矿工',
    description: '一群矿工被困在塌方的矿洞中，清除洞口的石像鬼解救他们。',
    zone: 'anvil_mountains',
    type: 'kill',
    category: 'side',
    objectives: [
      { type: 'explore', targetId: 'zone_collapsed_mine', targetName: '塌方矿洞', required: 1, current: 0, location: { col: 35, row: 22, radius: 8 } },
      { type: 'kill', targetId: 'gargoyle', targetName: '石像鬼', required: 8, current: 0 },
    ],
    rewards: { exp: 2000, gold: 170 },
    prereqQuests: ['q_explore_dwarf_ruins'],
    level: 24,
    questArea: { col: 35, row: 22, radius: 10 },
  },
  {
    id: 'q_dragon_egg',
    name: '龙蛋之谜',
    description: '山脉深处传说有龙蛋的巢穴，前去探索并收集龙鳞碎片。',
    zone: 'anvil_mountains',
    type: 'collect',
    category: 'side',
    objectives: [
      { type: 'explore', targetId: 'zone_dragon_nest', targetName: '龙巢', required: 1, current: 0, location: { col: 12, row: 58, radius: 8 } },
      { type: 'collect', targetId: 'mat_dragon_scale', targetName: '龙鳞碎片', required: 3, current: 0 },
    ],
    rewards: { exp: 2400, gold: 200, items: ['g_ruby_2'] },
    prereqQuests: ['q_kill_gargoyles'],
    level: 26,
    questArea: { col: 12, row: 58, radius: 10 },
  },

  // ═══════════════════════════════════════
  // Zone 4: Scorching Desert (Lv 30-40)
  // ═══════════════════════════════════════

  // --- Main Line (quell fire threat) ---
  {
    id: 'q_explore_desert',
    name: '探索灼热沙漠',
    description: '灼热的沙漠危机四伏，先探索周围的环境了解地形。',
    zone: 'scorching_desert',
    type: 'explore',
    category: 'main',
    objectives: [
      { type: 'explore', targetId: 'zone_fire_wastes', targetName: '烈焰荒地', required: 1, current: 0, location: { col: 26, row: 16, radius: 10 } },
      { type: 'explore', targetId: 'zone_scorpion_valley', targetName: '蝎谷', required: 1, current: 0, location: { col: 51, row: 32, radius: 10 } },
    ],
    rewards: { exp: 2800, gold: 200 },
    level: 30,
  },
  {
    id: 'q_kill_fire_elementals',
    name: '烈焰之心',
    description: '火焰元素在沙丘间肆虐，消灭它们平息火焰。',
    zone: 'scorching_desert',
    type: 'kill',
    category: 'main',
    objectives: [
      { type: 'kill', targetId: 'fire_elemental', targetName: '火焰元素', required: 12, current: 0 },
    ],
    rewards: { exp: 3800, gold: 250, items: ['g_ruby_2'] },
    prereqQuests: ['q_explore_desert'],
    level: 32,
    questArea: { col: 42, row: 16, radius: 15 },
  },
  {
    id: 'q_explore_oasis',
    name: '绿洲探索',
    description: '沙漠中有一片隐藏的绿洲，据说那里有对抗火焰的线索。',
    zone: 'scorching_desert',
    type: 'explore',
    category: 'main',
    objectives: [
      { type: 'explore', targetId: 'zone_desert_oasis', targetName: '沙漠绿洲', required: 1, current: 0, location: { col: 45, row: 22, radius: 10 } },
    ],
    rewards: { exp: 3200, gold: 220 },
    prereqQuests: ['q_kill_fire_elementals'],
    level: 35,
  },
  {
    id: 'q_kill_sandworms',
    name: '沙虫巢穴',
    description: '巨大的沙虫正在吞噬商路，找到它们的巢穴并清除威胁。',
    zone: 'scorching_desert',
    type: 'kill',
    category: 'main',
    objectives: [
      { type: 'kill', targetId: 'sandworm', targetName: '沙虫', required: 8, current: 0 },
    ],
    rewards: { exp: 5000, gold: 350, items: ['w_demon_blade'] },
    prereqQuests: ['q_explore_oasis'],
    level: 36,
    questArea: { col: 49, row: 45, radius: 15 },
  },
  {
    id: 'q_seal_fire_rift',
    name: '封印火焰裂隙',
    description: '沙漠深处有一道火焰裂隙，击败守护它的凤凰来封印它。',
    zone: 'scorching_desert',
    type: 'kill',
    category: 'main',
    objectives: [
      { type: 'kill', targetId: 'phoenix', targetName: '凤凰', required: 1, current: 0 },
    ],
    rewards: { exp: 6000, gold: 400 },
    prereqQuests: ['q_kill_sandworms'],
    level: 38,
    questArea: { col: 32, row: 70, radius: 8 },
  },

  // --- Side Quests ---
  {
    id: 'q_water_supply',
    name: '水源补给',
    description: '沙漠中水比金子还贵，从击败的怪物身上搜集水囊。',
    zone: 'scorching_desert',
    type: 'collect',
    category: 'side',
    objectives: [
      { type: 'collect', targetId: 'mat_water', targetName: '净化水囊', required: 5, current: 0 },
    ],
    rewards: { exp: 2500, gold: 180, items: ['c_hp_potion_m'] },
    level: 31,
    questArea: { col: 16, row: 38, radius: 12 },
  },
  {
    id: 'q_scorpion_venom',
    name: '蝎毒采集',
    description: '沙漠蝎子的毒液可以提炼成强效毒药，击败它们采集蝎毒。',
    zone: 'scorching_desert',
    type: 'kill',
    category: 'side',
    objectives: [
      { type: 'kill', targetId: 'desert_scorpion', targetName: '沙漠蝎子', required: 8, current: 0 },
      { type: 'collect', targetId: 'mat_venom', targetName: '蝎毒', required: 5, current: 0 },
    ],
    rewards: { exp: 3000, gold: 200 },
    level: 33,
    questArea: { col: 64, row: 48, radius: 10 },
  },
  {
    id: 'q_buried_treasure',
    name: '沙下宝藏',
    description: '传说沙漠深处埋藏着古老文明的宝藏，前去探索遗迹。',
    zone: 'scorching_desert',
    type: 'explore',
    category: 'side',
    objectives: [
      { type: 'explore', targetId: 'zone_buried_ruins', targetName: '埋藏遗迹', required: 1, current: 0, location: { col: 64, row: 48, radius: 10 } },
    ],
    rewards: { exp: 3500, gold: 250 },
    prereqQuests: ['q_explore_desert'],
    level: 34,
  },
  {
    id: 'q_mirage_beasts',
    name: '海市蜃楼之兽',
    description: '沙漠热浪中出现了由火焰凝聚而成的魔兽，消灭这些异变体。',
    zone: 'scorching_desert',
    type: 'kill',
    category: 'side',
    objectives: [
      { type: 'kill', targetId: 'fire_elemental', targetName: '火焰元素', required: 15, current: 0 },
    ],
    rewards: { exp: 4200, gold: 280 },
    prereqQuests: ['q_kill_fire_elementals'],
    level: 37,
    questArea: { col: 45, row: 22, radius: 12 },
  },

  // ═══════════════════════════════════════
  // Zone 5: Abyss Rift (Lv 40-50)
  // ═══════════════════════════════════════

  // --- Main Line (seal the abyss) ---
  {
    id: 'q_explore_abyss',
    name: '深渊之门',
    description: '深渊裂隙已经开启，勇士必须探索这片被恶魔污染的领域。',
    zone: 'abyss_rift',
    type: 'explore',
    category: 'main',
    objectives: [
      { type: 'explore', targetId: 'zone_rift_entrance', targetName: '裂隙入口', required: 1, current: 0, location: { col: 26, row: 10, radius: 10 } },
      { type: 'explore', targetId: 'zone_demon_spire', targetName: '恶魔尖塔', required: 1, current: 0, location: { col: 64, row: 38, radius: 10 } },
      { type: 'explore', targetId: 'zone_throne_of_chaos', targetName: '混沌王座', required: 1, current: 0, location: { col: 38, row: 70, radius: 10 } },
    ],
    rewards: { exp: 6000, gold: 400 },
    level: 40,
  },
  {
    id: 'q_kill_demons',
    name: '恶魔驱逐',
    description: '深渊中的恶魔必须被消灭，否则它们将涌入凡间。',
    zone: 'abyss_rift',
    type: 'kill',
    category: 'main',
    objectives: [
      { type: 'kill', targetId: 'imp', targetName: '小恶魔', required: 20, current: 0 },
      { type: 'kill', targetId: 'lesser_demon', targetName: '次级恶魔', required: 10, current: 0 },
    ],
    rewards: { exp: 8000, gold: 500 },
    prereqQuests: ['q_explore_abyss'],
    level: 43,
    questArea: { col: 40, row: 30, radius: 15 },
  },
  {
    id: 'q_collect_demon_essence',
    name: '恶魔精华',
    description: '收集恶魔精华来封印深渊裂隙，阻止更多恶魔入侵。',
    zone: 'abyss_rift',
    type: 'collect',
    category: 'main',
    objectives: [
      { type: 'collect', targetId: 'mat_demon_essence', targetName: '恶魔精华', required: 15, current: 0 },
    ],
    rewards: { exp: 7500, gold: 450, items: ['a_demon_helm'] },
    prereqQuests: ['q_kill_demons'],
    level: 45,
    questArea: { col: 45, row: 45, radius: 15 },
  },
  {
    id: 'q_forge_seal',
    name: '锻造封印',
    description: '收集封印碎片来锻造封印深渊的钥匙。',
    zone: 'abyss_rift',
    type: 'collect',
    category: 'main',
    objectives: [
      { type: 'collect', targetId: 'mat_seal_fragment', targetName: '封印碎片', required: 5, current: 0 },
    ],
    rewards: { exp: 9000, gold: 550 },
    prereqQuests: ['q_collect_demon_essence'],
    level: 47,
    questArea: { col: 55, row: 55, radius: 10 },
  },
  {
    id: 'q_kill_abyss_lord',
    name: '深渊领主',
    description: '深渊领主是一切灾厄的根源，击败它来拯救这个世界！',
    zone: 'abyss_rift',
    type: 'kill',
    category: 'main',
    objectives: [
      { type: 'kill', targetId: 'demon_lord', targetName: '深渊领主', required: 1, current: 0 },
    ],
    rewards: { exp: 15000, gold: 1000, items: ['a_dragon_armor'] },
    prereqQuests: ['q_forge_seal'],
    level: 50,
    questArea: { col: 38, row: 70, radius: 8 },
  },

  // --- Side Quests ---
  {
    id: 'q_corrupted_souls',
    name: '堕落之魂',
    description: '小恶魔在裂隙中四处游荡，清除它们减轻深渊的腐蚀。',
    zone: 'abyss_rift',
    type: 'kill',
    category: 'side',
    objectives: [
      { type: 'kill', targetId: 'imp', targetName: '小恶魔', required: 15, current: 0 },
    ],
    rewards: { exp: 5500, gold: 350 },
    level: 41,
    questArea: { col: 42, row: 18, radius: 12 },
  },
  {
    id: 'q_void_crystals',
    name: '虚空水晶',
    description: '深渊中结晶的虚空能量可以用来强化封印，尽可能多收集。',
    zone: 'abyss_rift',
    type: 'collect',
    category: 'side',
    objectives: [
      { type: 'collect', targetId: 'mat_void_crystal', targetName: '虚空水晶', required: 10, current: 0 },
    ],
    rewards: { exp: 6000, gold: 380 },
    level: 42,
    questArea: { col: 16, row: 32, radius: 10 },
  },
  {
    id: 'q_fallen_hero',
    name: '陨落英雄',
    description: '深渊中有一处英雄纪念碑和陨落神殿，探索它们寻找前人的遗志。',
    zone: 'abyss_rift',
    type: 'explore',
    category: 'side',
    objectives: [
      { type: 'explore', targetId: 'zone_fallen_shrine', targetName: '陨落神殿', required: 1, current: 0, location: { col: 26, row: 58, radius: 8 } },
      { type: 'explore', targetId: 'zone_hero_memorial', targetName: '英雄纪念碑', required: 1, current: 0, location: { col: 55, row: 55, radius: 8 } },
    ],
    rewards: { exp: 6500, gold: 400 },
    prereqQuests: ['q_explore_abyss'],
    level: 44,
  },
  {
    id: 'q_demon_weaponry',
    name: '恶魔兵器',
    description: '恶魔军团使用的武器蕴含强大力量，从它们身上收集恶魔兵器。',
    zone: 'abyss_rift',
    type: 'kill',
    category: 'side',
    objectives: [
      { type: 'kill', targetId: 'lesser_demon', targetName: '次级恶魔', required: 10, current: 0 },
      { type: 'kill', targetId: 'succubus', targetName: '魅魔', required: 5, current: 0 },
    ],
    rewards: { exp: 7000, gold: 420, items: ['w_abyssal_staff'] },
    prereqQuests: ['q_kill_demons'],
    level: 46,
    questArea: { col: 45, row: 42, radius: 12 },
  },
];
```

**Step 2: Update `src/data/npcs.ts`**

Add `forest_hermit` NPC and update all quest NPC assignments:

```typescript
import type { NPCDefinition } from './types';

export const NPCDefinitions: Record<string, NPCDefinition> = {
  blacksmith: {
    id: 'blacksmith',
    name: '铁匠',
    type: 'blacksmith',
    dialogue: ['欢迎来到我的铁匠铺!', '需要修理装备或者打造新武器吗？'],
    shopItems: ['w_short_sword', 'w_broad_sword', 'w_dagger', 'w_stiletto', 'w_short_bow', 'w_oak_staff', 'w_wooden_shield', 'a_leather_helm', 'a_leather_armor', 'a_leather_gloves', 'a_leather_boots', 'a_leather_belt'],
  },
  merchant: {
    id: 'merchant',
    name: '商人',
    type: 'merchant',
    dialogue: ['需要补给吗？我这里应有尽有!', '药水、卷轴、宝石，随你挑选。'],
    shopItems: ['c_hp_potion_s', 'c_hp_potion_m', 'c_mp_potion_s', 'c_mp_potion_m', 'c_antidote', 'c_tp_scroll', 'c_id_scroll', 'g_ruby_1', 'g_sapphire_1', 'g_emerald_1', 'g_topaz_1'],
  },
  stash: {
    id: 'stash',
    name: '仓库管理员',
    type: 'stash',
    dialogue: ['需要存放物品吗？', '您的仓库安全可靠。'],
  },
  quest_elder: {
    id: 'quest_elder',
    name: '村长',
    type: 'quest',
    dialogue: ['勇士，翡翠平原上的怪物越来越多了...', '请帮助我们清除这些威胁!'],
    quests: ['q_kill_slimes', 'q_collect_slime_gel', 'q_herb_gathering', 'q_kill_goblins', 'q_explore_goblin_camp', 'q_lost_pendant', 'q_find_goblin_chief', 'q_bandit_trouble', 'q_rare_mushroom', 'q_secure_plains'],
  },
  quest_scout: {
    id: 'quest_scout',
    name: '侦察兵',
    type: 'quest',
    dialogue: ['暮色森林中有不祥的动静...', '你愿意去调查一下吗？'],
    quests: ['q_explore_forest', 'q_collect_wolf_pelts', 'q_kill_undead', 'q_spider_nest', 'q_talk_hermit', 'q_lost_scout', 'q_ancient_relic', 'q_moonlight_herb', 'q_kill_werewolf_alpha', 'q_seal_dark_source'],
  },
  forest_hermit: {
    id: 'forest_hermit',
    name: '森林隐士',
    type: 'quest',
    dialogue: ['你终于找到我了...亡灵复苏的根源在森林深处的黑暗能量。', '去找到那个黑暗之源，否则整片森林都将沦陷。'],
    quests: [],
  },
  blacksmith_advanced: {
    id: 'blacksmith_advanced',
    name: '高级铁匠',
    type: 'blacksmith',
    dialogue: ['只有最好的材料才配得上我的手艺。'],
    shopItems: ['w_battle_axe', 'w_arcane_staff', 'w_iron_shield', 'a_chain_mail', 'a_iron_helm', 'a_chain_gloves', 'a_chain_boots', 'a_heavy_belt'],
  },
  quest_dwarf: {
    id: 'quest_dwarf',
    name: '矮人长老',
    type: 'quest',
    dialogue: ['这些山脉曾是我族的家园...', '帮助我们夺回先祖的遗迹吧。'],
    quests: ['q_explore_dwarf_ruins', 'q_crystal_mining', 'q_kill_gargoyles', 'q_mountain_bandits', 'q_trapped_miners', 'q_collect_dwarf_relics', 'q_dragon_egg', 'q_reforge_artifact', 'q_kill_stone_guardian'],
  },
  quest_nomad: {
    id: 'quest_nomad',
    name: '沙漠游牧民',
    type: 'quest',
    dialogue: ['灼热的沙漠中危机四伏...', '只有最勇敢的人才能在这里生存。'],
    quests: ['q_explore_desert', 'q_water_supply', 'q_kill_fire_elementals', 'q_scorpion_venom', 'q_buried_treasure', 'q_explore_oasis', 'q_kill_sandworms', 'q_mirage_beasts', 'q_seal_fire_rift'],
  },
  quest_warden: {
    id: 'quest_warden',
    name: '深渊守望者',
    type: 'quest',
    dialogue: ['深渊裂隙正在扩大，恶魔即将涌入...', '我们需要你的力量来封印它。'],
    quests: ['q_explore_abyss', 'q_corrupted_souls', 'q_void_crystals', 'q_kill_demons', 'q_fallen_hero', 'q_collect_demon_essence', 'q_demon_weaponry', 'q_forge_seal', 'q_kill_abyss_lord'],
  },
  merchant_desert: {
    id: 'merchant_desert',
    name: '沙漠商人',
    type: 'merchant',
    dialogue: ['沙漠里水比金子还贵...', '不过我有你需要的一切。'],
    shopItems: ['c_hp_potion_m', 'c_hp_potion_l', 'c_mp_potion_m', 'c_antidote', 'c_tp_scroll', 'c_id_scroll', 'g_ruby_2', 'g_sapphire_2', 'g_diamond_1'],
  },
};
```

**Step 3: Add forest_hermit to twilight_forest map**

In `src/data/maps/twilight_forest.ts`, add `forest_hermit` to a remote location. Either add a new camp or add to existing second camp:

Change the second camp line:
```typescript
// Old:
{ col: 70, row: 67, npcs: ['merchant'] },
// New:
{ col: 70, row: 67, npcs: ['merchant'] },
{ col: 66, row: 42, npcs: ['forest_hermit'] },
```

This adds a small camp at (66, 42) with just the hermit, matching the quest target location.

**Step 4: Verify compilation**

Run: `npx tsc --noEmit`
Expected: PASS

**Step 5: Commit**

```bash
git add src/data/quests/all_quests.ts src/data/npcs.ts src/data/maps/twilight_forest.ts
git commit -m "feat(quest): expand to 47 quests with main/side categories and NPC assignments"
```

---

## Task 3: Quest Progress Tracking for All Types

**Files:**
- Modify: `src/scenes/ZoneScene.ts`

Currently only `kill` quest progress is tracked (line 544). Collect, explore, and talk types never trigger `updateProgress`. This task wires up all types.

**Step 1: Add talk progress to NPC interaction**

In `interactNPC()` (around line 625), add `updateProgress('talk', def.id)` right after the log message:

```typescript
private interactNPC(npc: NPC): void {
    const def = npc.definition;
    EventBus.emit(GameEvents.LOG_MESSAGE, { text: def.dialogue[0], type: 'info' });

    // Progress talk quests
    this.questSystem.updateProgress('talk', def.id);

    switch (def.type) {
    // ... rest unchanged
```

**Step 2: Add collect progress on monster kill**

In `onMonsterKilled()` (around line 544, after the kill progress line), add:

```typescript
    this.questSystem.updateProgress('kill', monster.definition.id);

    // Progress collect quests: monsters in this zone drop quest collectibles
    const activeQuests = this.questSystem.getActiveQuests();
    for (const { quest, progress } of activeQuests) {
      if (progress.status !== 'active') continue;
      if (quest.zone !== this.currentMapId) continue;
      for (let i = 0; i < quest.objectives.length; i++) {
        const obj = quest.objectives[i];
        if (obj.type === 'collect' && progress.objectives[i].current < obj.required) {
          if (Math.random() < 0.4) {
            this.questSystem.updateProgress('collect', obj.targetId);
          }
          break;
        }
      }
    }
```

**Step 3: Add explore progress check in update loop**

In `update()` method (around line 250, after the fog update), add a throttled explore check:

```typescript
    // Throttled explore quest check
    if (Math.floor(time / 500) !== Math.floor((time - delta) / 500)) {
      this.checkExploreQuests();
    }
```

Add the new method to ZoneScene:

```typescript
  private checkExploreQuests(): void {
    const activeQuests = this.questSystem.getActiveQuests();
    for (const { quest, progress } of activeQuests) {
      if (progress.status !== 'active') continue;
      if (quest.zone !== this.currentMapId) continue;
      for (let i = 0; i < quest.objectives.length; i++) {
        const obj = quest.objectives[i];
        if (obj.type === 'explore' && obj.location && progress.objectives[i].current < obj.required) {
          const dx = this.player.tileCol - obj.location.col;
          const dy = this.player.tileRow - obj.location.row;
          const dist = Math.sqrt(dx * dx + dy * dy);
          if (dist <= obj.location.radius) {
            this.questSystem.updateProgress('explore', obj.targetId);
            EventBus.emit(GameEvents.LOG_MESSAGE, {
              text: `发现: ${obj.targetName}`,
              type: 'system',
            });
          }
        }
      }
    }
  }
```

**Step 4: Verify compilation**

Run: `npx tsc --noEmit`

**Step 5: Commit**

```bash
git add src/scenes/ZoneScene.ts
git commit -m "feat(quest): wire up progress tracking for collect, explore, and talk quest types"
```

---

## Task 4: NPC Dynamic Quest Indicators

**Files:**
- Modify: `src/entities/NPC.ts`
- Modify: `src/scenes/ZoneScene.ts`

Replace the static `!` marker with a dynamic marker (!/?) that updates based on quest state.

**Step 1: Update NPC.ts**

Store the quest marker as a property and add a setter method. Replace the static `!` creation:

```typescript
import Phaser from 'phaser';
import { cartToIso } from '../utils/IsometricUtils';
import type { NPCDefinition } from '../data/types';

export class NPC {
  scene: Phaser.Scene;
  sprite: Phaser.GameObjects.Container;
  definition: NPCDefinition;
  tileCol: number;
  tileRow: number;
  nameLabel: Phaser.GameObjects.Text;
  questMarker: Phaser.GameObjects.Text | null = null;
  private questMarkerTween: Phaser.Tweens.Tween | null = null;

  constructor(scene: Phaser.Scene, definition: NPCDefinition, col: number, row: number) {
    this.scene = scene;
    this.definition = definition;
    this.tileCol = col;
    this.tileRow = row;

    const worldPos = cartToIso(col, row);
    this.sprite = scene.add.container(worldPos.x, worldPos.y);
    this.sprite.setDepth(worldPos.y + 80);

    // Use loaded sprite texture if available, otherwise draw procedural fallback
    const spriteKey = `npc_${definition.type}`;
    if (scene.textures.exists(spriteKey)) {
      const img = scene.add.image(0, -32, spriteKey);
      this.sprite.add(img);
    } else {
      this.drawProceduralNPC(scene);
    }

    // Quest marker (created for quest NPCs, updated dynamically)
    if (definition.type === 'quest') {
      this.questMarker = scene.add.text(0, -68, '!', {
        fontSize: '18px',
        color: '#f1c40f',
        fontFamily: '"Cinzel", serif',
        fontStyle: 'bold',
        stroke: '#000000',
        strokeThickness: 3,
      }).setOrigin(0.5);
      this.sprite.add(this.questMarker);
      this.questMarkerTween = scene.tweens.add({
        targets: this.questMarker, y: this.questMarker.y - 4, duration: 600,
        yoyo: true, repeat: -1, ease: 'Sine.easeInOut',
      });
    }

    // Name label
    this.nameLabel = scene.add.text(0, 12, definition.name, {
      fontSize: '11px',
      color: '#ffffff',
      fontFamily: '"Noto Sans SC", sans-serif',
      stroke: '#000000',
      strokeThickness: 3,
    }).setOrigin(0.5);
    this.sprite.add(this.nameLabel);

    // Interactive hit area
    const hitZone = scene.add.rectangle(0, -20, 40, 70, 0xffffff, 0);
    hitZone.setInteractive({ useHandCursor: true });
    this.sprite.add(hitZone);
    this.sprite.setSize(40, 70);
    this.sprite.setInteractive(new Phaser.Geom.Rectangle(-20, -60, 40, 80), Phaser.Geom.Rectangle.Contains);
  }

  setQuestMarker(text: string, color: string): void {
    if (!this.questMarker) return;
    if (text) {
      this.questMarker.setText(text).setColor(color).setVisible(true);
    } else {
      this.questMarker.setVisible(false);
    }
  }

  // ... keep getNPCColor, getHatColor, drawProceduralNPC, isNearPlayer unchanged
```

**Step 2: Add NPC marker updates in ZoneScene**

Add a new method and call it in the update loop (throttled, every 500ms):

In `update()`, add after explore quest check:

```typescript
    // Throttled NPC quest marker update
    if (Math.floor(time / 500) !== Math.floor((time - delta) / 500)) {
      this.checkExploreQuests();
      this.updateNPCQuestMarkers();
    }
```

Add the method:

```typescript
  private updateNPCQuestMarkers(): void {
    for (const npc of this.npcs) {
      const def = npc.definition;
      if (def.type !== 'quest' || !def.quests) continue;

      let hasCompletedQuest = false;
      let hasActiveQuest = false;
      let hasAvailableQuest = false;
      let isMainQuest = false;

      for (const qid of def.quests) {
        const prog = this.questSystem.progress.get(qid);
        if (prog) {
          if (prog.status === 'completed') hasCompletedQuest = true;
          else if (prog.status === 'active') hasActiveQuest = true;
        }
      }

      if (!hasCompletedQuest) {
        const available = this.questSystem.getAvailableQuests(def.quests, this.player.level);
        for (const q of available) {
          if (!this.questSystem.progress.has(q.id)) {
            hasAvailableQuest = true;
            if (q.category === 'main') isMainQuest = true;
            break;
          }
        }
      }

      if (hasCompletedQuest) {
        npc.setQuestMarker('?', '#f1c40f');
      } else if (hasAvailableQuest) {
        npc.setQuestMarker('!', isMainQuest ? '#f1c40f' : '#95a5a6');
      } else if (hasActiveQuest) {
        npc.setQuestMarker('?', '#888888');
      } else {
        npc.setQuestMarker('', '');
      }
    }
  }
```

**Step 3: Verify compilation**

Run: `npx tsc --noEmit`

**Step 4: Commit**

```bash
git add src/entities/NPC.ts src/scenes/ZoneScene.ts
git commit -m "feat(quest): dynamic NPC quest indicators (!/?)"
```

---

## Task 5: Quest Tracker UI Enhancement

**Files:**
- Modify: `src/scenes/UIScene.ts` (update method, around line 752)

Replace the existing quest tracker update (shows only 3 quests, 1st objective) with an enhanced version showing all active quests with all objectives and main/side labels.

**Step 1: Replace quest tracker update code**

In `UIScene.update()`, replace the quest tracker block:

```typescript
    // Old code (lines 752-761):
    // if (this.zone?.questSystem) { ... }

    // New code:
    if (this.zone?.questSystem) {
      const active = this.zone.questSystem.getActiveQuests();
      const lines: string[] = [];
      // Sort: main quests first
      const sorted = active.sort((a, b) => {
        if (a.quest.category === 'main' && b.quest.category !== 'main') return -1;
        if (a.quest.category !== 'main' && b.quest.category === 'main') return 1;
        return 0;
      });
      for (const { quest, progress } of sorted) {
        const tag = quest.category === 'main' ? '[主线]' : '[支线]';
        const statusTag = progress.status === 'completed' ? ' [完成]' : '';
        lines.push(`${tag} ${quest.name}${statusTag}`);
        for (let i = 0; i < quest.objectives.length; i++) {
          const obj = quest.objectives[i];
          const cur = progress.objectives[i]?.current ?? 0;
          const done = cur >= obj.required;
          const mark = done ? '✓' : `${cur}/${obj.required}`;
          lines.push(`  ${obj.targetName} ${mark}`);
        }
      }
      this.questTracker.setText(lines.join('\n'));
    }
```

**Step 2: Verify compilation**

Run: `npx tsc --noEmit`

**Step 3: Commit**

```bash
git add src/scenes/UIScene.ts
git commit -m "feat(quest): enhanced quest tracker with all objectives and main/side labels"
```

---

## Task 6: Minimap Quest Markers

**Files:**
- Modify: `src/scenes/UIScene.ts` (updateMinimap method, around line 622)

Add quest NPC markers (!/?) and active quest target area markers to the minimap.

**Step 1: Add quest markers to updateMinimap()**

After the exits drawing section (before the closing `}`), add:

```typescript
    // Quest NPC markers on minimap
    if (this.zone?.questSystem) {
      const mapData = AllMaps[(this.zone as any).currentMapId];
      if (mapData) {
        for (const camp of mapData.camps) {
          for (const npcId of camp.npcs) {
            const npcDef = NPCDefinitions[npcId];
            if (!npcDef || npcDef.type !== 'quest' || !npcDef.quests) continue;
            // Check if NPC has available or completed quests
            let hasAvailable = false;
            let hasCompleted = false;
            for (const qid of npcDef.quests) {
              const prog = this.zone.questSystem.progress.get(qid);
              if (prog && prog.status === 'completed') hasCompleted = true;
            }
            if (!hasCompleted) {
              const avail = this.zone.questSystem.getAvailableQuests(npcDef.quests, this.player.level);
              for (const q of avail) {
                if (!this.zone.questSystem.progress.has(q.id)) { hasAvailable = true; break; }
              }
            }
            if (hasCompleted || hasAvailable) {
              const color = hasCompleted ? 0xf1c40f : 0xf1c40f;
              this.minimap.fillStyle(color);
              this.minimap.fillCircle(camp.col * sx, camp.row * sy, 2.5);
            }
          }
        }

        // Active quest target area markers
        const activeQuests = this.zone.questSystem.getActiveQuests();
        for (const { quest, progress } of activeQuests) {
          if (progress.status !== 'active') continue;
          if (quest.zone !== (this.zone as any).currentMapId) continue;
          // Quest area marker
          if (quest.questArea) {
            const qa = quest.questArea;
            const qColor = quest.category === 'main' ? 0xf1c40f : 0x95a5a6;
            this.minimap.fillStyle(qColor, 0.25);
            this.minimap.fillCircle(qa.col * sx, qa.row * sy, qa.radius * sx);
            this.minimap.lineStyle(1, qColor, 0.6);
            this.minimap.strokeCircle(qa.col * sx, qa.row * sy, qa.radius * sx);
          }
          // Explore objective markers
          for (let i = 0; i < quest.objectives.length; i++) {
            const obj = quest.objectives[i];
            if (obj.type === 'explore' && obj.location && progress.objectives[i].current < obj.required) {
              this.minimap.fillStyle(0xf39c12, 0.5);
              this.minimap.fillRect(obj.location.col * sx - 1.5, obj.location.row * sy - 1.5, 3, 3);
            }
          }
        }
      }
    }
```

You'll need to import NPCDefinitions at the top of UIScene.ts:

```typescript
import { NPCDefinitions } from '../data/npcs';
```

**Step 2: Verify compilation**

Run: `npx tsc --noEmit`

**Step 3: Commit**

```bash
git add src/scenes/UIScene.ts
git commit -m "feat(quest): minimap quest NPC markers and target area indicators"
```

---

## Task 7: Quest Log Panel (J Key)

**Files:**
- Modify: `src/scenes/ZoneScene.ts` (add J key binding)
- Modify: `src/scenes/UIScene.ts` (add quest log panel)

### Step 1: Add J key binding in ZoneScene

In the keyboard setup block (around line 149), add:

```typescript
        J: this.input.keyboard.addKey(Phaser.Input.Keyboard.KeyCodes.J),
```

In the `handleSkillInput()` method or wherever other panel toggles are handled, add:

```typescript
    if (Phaser.Input.Keyboard.JustDown(this.wasd.J)) {
      EventBus.emit(GameEvents.UI_TOGGLE_PANEL, { panel: 'quest' });
    }
```

### Step 2: Add quest panel toggle in UIScene event listener

In `setupEventListeners()`, add to the UI_TOGGLE_PANEL handler:

```typescript
      if (data.panel === 'quest') this.toggleQuestLog();
```

### Step 3: Add questLogPanel property

Add to UIScene class properties (around line 38):

```typescript
  private questLogPanel: Phaser.GameObjects.Container | null = null;
  private questLogTab: 'active' | 'completed' = 'active';
  private questLogPage = 0;
  private questLogSelectedIndex = 0;
```

### Step 4: Add quest log panel to closeAllPanels

In `closeAllPanels()`, add:

```typescript
    if (this.questLogPanel) { this.questLogPanel.destroy(); this.questLogPanel = null; }
```

### Step 5: Implement toggleQuestLog and panel rendering

Add these methods to UIScene:

```typescript
  private toggleQuestLog(): void {
    if (this.questLogPanel) {
      this.questLogPanel.destroy();
      this.questLogPanel = null;
      return;
    }
    this.closeAllPanels();
    this.createQuestLogPanel();
  }

  private createQuestLogPanel(): void {
    const pw = 700, ph = 480;
    const px = (GAME_WIDTH - pw) / 2, py = (GAME_HEIGHT - ph) / 2;
    this.questLogPanel = this.add.container(px, py).setDepth(4000);

    // Background
    const bg = this.add.rectangle(0, 0, pw, ph, 0x0f0f1e, 0.95).setOrigin(0, 0).setStrokeStyle(2, 0xc0934a);
    this.questLogPanel.add(bg);

    // Title
    this.questLogPanel.add(this.add.text(pw / 2, 14, '任务日志', {
      fontSize: '16px', color: '#c0934a', fontFamily: TITLE_FONT, fontStyle: 'bold',
    }).setOrigin(0.5, 0));

    // Close button
    const closeBtn = this.add.text(pw - 16, 10, '✕', {
      fontSize: '16px', color: '#c0392b', fontFamily: FONT,
    }).setOrigin(0.5, 0).setInteractive({ useHandCursor: true });
    closeBtn.on('pointerdown', () => this.toggleQuestLog());
    this.questLogPanel.add(closeBtn);

    // Tab buttons
    const tabY = 38;
    const activeTab = this.add.rectangle(100, tabY, 160, 24, this.questLogTab === 'active' ? 0x1a2a3a : 0x111122)
      .setStrokeStyle(1, 0x2471a3).setInteractive({ useHandCursor: true });
    activeTab.on('pointerdown', () => { this.questLogTab = 'active'; this.questLogPage = 0; this.questLogSelectedIndex = 0; this.refreshQuestLog(); });
    this.questLogPanel.add(activeTab);
    this.questLogPanel.add(this.add.text(100, tabY, '进行中', {
      fontSize: '12px', color: this.questLogTab === 'active' ? '#5dade2' : '#666', fontFamily: FONT,
    }).setOrigin(0.5));

    const completedTab = this.add.rectangle(270, tabY, 160, 24, this.questLogTab === 'completed' ? 0x1a2a3a : 0x111122)
      .setStrokeStyle(1, 0x2471a3).setInteractive({ useHandCursor: true });
    completedTab.on('pointerdown', () => { this.questLogTab = 'completed'; this.questLogPage = 0; this.questLogSelectedIndex = 0; this.refreshQuestLog(); });
    this.questLogPanel.add(completedTab);
    this.questLogPanel.add(this.add.text(270, tabY, '已完成', {
      fontSize: '12px', color: this.questLogTab === 'completed' ? '#5dade2' : '#666', fontFamily: FONT,
    }).setOrigin(0.5));

    // Render quest content
    this.renderQuestLogContent();
  }

  private refreshQuestLog(): void {
    if (!this.questLogPanel) return;
    this.questLogPanel.destroy();
    this.questLogPanel = null;
    this.createQuestLogPanel();
  }

  private renderQuestLogContent(): void {
    if (!this.questLogPanel || !this.zone?.questSystem) return;

    const pw = 700, listW = 240, detailX = 255, detailW = 430;
    const listStartY = 58, itemH = 28, maxItems = 14;

    // Get quest list
    let quests: { quest: import('../data/types').QuestDefinition; progress: import('../data/types').QuestProgress }[];
    if (this.questLogTab === 'active') {
      quests = this.zone.questSystem.getActiveQuests();
    } else {
      quests = [];
      for (const [id, prog] of this.zone.questSystem.progress.entries()) {
        if (prog.status === 'turned_in') {
          const allQuests = (this.zone.questSystem as any).quests as Map<string, import('../data/types').QuestDefinition>;
          const q = allQuests.get(id);
          if (q) quests.push({ quest: q, progress: prog });
        }
      }
    }

    // Sort: main first
    quests.sort((a, b) => {
      if (a.quest.category === 'main' && b.quest.category !== 'main') return -1;
      if (a.quest.category !== 'main' && b.quest.category === 'main') return 1;
      return a.quest.level - b.quest.level;
    });

    const totalPages = Math.max(1, Math.ceil(quests.length / maxItems));
    const pageQuests = quests.slice(this.questLogPage * maxItems, (this.questLogPage + 1) * maxItems);

    // Divider line
    this.questLogPanel.add(this.add.rectangle(listW + 7, 58, 1, 400, 0x333344).setOrigin(0, 0));

    // Quest list
    pageQuests.forEach((entry, i) => {
      const y = listStartY + i * itemH;
      const isSelected = i === this.questLogSelectedIndex;
      const listBg = this.add.rectangle(5, y, listW, itemH - 2, isSelected ? 0x1a2a3a : 0x0f0f1e)
        .setOrigin(0, 0).setStrokeStyle(isSelected ? 1 : 0, 0x2471a3)
        .setInteractive({ useHandCursor: true });
      listBg.on('pointerdown', () => { this.questLogSelectedIndex = i; this.refreshQuestLog(); });
      this.questLogPanel!.add(listBg);

      const tagColor = entry.quest.category === 'main' ? '#c0934a' : '#95a5a6';
      const tag = entry.quest.category === 'main' ? '[主]' : '[支]';
      this.questLogPanel!.add(this.add.text(12, y + 5, tag, {
        fontSize: '10px', color: tagColor, fontFamily: FONT, fontStyle: 'bold',
      }));
      const nameColor = this.questLogTab === 'completed' ? '#555' : (isSelected ? '#e0d8cc' : '#aaa');
      this.questLogPanel!.add(this.add.text(36, y + 5, `${entry.quest.name} Lv.${entry.quest.level}`, {
        fontSize: '10px', color: nameColor, fontFamily: FONT,
      }));
    });

    // Pagination
    if (totalPages > 1) {
      const pageY = listStartY + maxItems * itemH + 4;
      if (this.questLogPage > 0) {
        const prevBtn = this.add.text(40, pageY, '◀ 上一页', {
          fontSize: '10px', color: '#5dade2', fontFamily: FONT,
        }).setInteractive({ useHandCursor: true });
        prevBtn.on('pointerdown', () => { this.questLogPage--; this.questLogSelectedIndex = 0; this.refreshQuestLog(); });
        this.questLogPanel.add(prevBtn);
      }
      this.questLogPanel.add(this.add.text(120, pageY, `${this.questLogPage + 1}/${totalPages}`, {
        fontSize: '10px', color: '#666', fontFamily: FONT,
      }));
      if (this.questLogPage < totalPages - 1) {
        const nextBtn = this.add.text(160, pageY, '下一页 ▶', {
          fontSize: '10px', color: '#5dade2', fontFamily: FONT,
        }).setInteractive({ useHandCursor: true });
        nextBtn.on('pointerdown', () => { this.questLogPage++; this.questLogSelectedIndex = 0; this.refreshQuestLog(); });
        this.questLogPanel.add(nextBtn);
      }
    }

    // No quests message
    if (pageQuests.length === 0) {
      this.questLogPanel.add(this.add.text(120, 120, this.questLogTab === 'active' ? '暂无进行中的任务' : '暂无已完成的任务', {
        fontSize: '12px', color: '#555', fontFamily: FONT,
      }).setOrigin(0.5, 0));
      return;
    }

    // Quest detail (right side)
    const selected = pageQuests[this.questLogSelectedIndex] ?? pageQuests[0];
    if (!selected) return;

    let dy = listStartY;

    // Quest name
    this.questLogPanel.add(this.add.text(detailX + detailW / 2, dy, selected.quest.name, {
      fontSize: '15px', color: '#c0934a', fontFamily: TITLE_FONT, fontStyle: 'bold',
    }).setOrigin(0.5, 0));
    dy += 24;

    // Category + Level + Zone
    const zoneNames: Record<string, string> = {
      emerald_plains: '翡翠平原', twilight_forest: '暮色森林',
      anvil_mountains: '铁砧山脉', scorching_desert: '灼热沙漠', abyss_rift: '深渊裂隙',
    };
    const catText = selected.quest.category === 'main' ? '主线任务' : '支线任务';
    const catColor = selected.quest.category === 'main' ? '#c0934a' : '#95a5a6';
    this.questLogPanel.add(this.add.text(detailX + 5, dy, `${catText}  |  Lv.${selected.quest.level}  |  ${zoneNames[selected.quest.zone] ?? selected.quest.zone}`, {
      fontSize: '10px', color: catColor, fontFamily: FONT,
    }));
    dy += 20;

    // Description
    this.questLogPanel.add(this.add.text(detailX + 5, dy, selected.quest.description, {
      fontSize: '11px', color: '#bbb', fontFamily: FONT, wordWrap: { width: detailW - 20 },
    }));
    dy += 40;

    // Objectives header
    this.questLogPanel.add(this.add.text(detailX + 5, dy, '目标:', {
      fontSize: '11px', color: '#e0d8cc', fontFamily: FONT, fontStyle: 'bold',
    }));
    dy += 18;

    // Objectives with progress
    for (let i = 0; i < selected.quest.objectives.length; i++) {
      const obj = selected.quest.objectives[i];
      const cur = selected.progress.objectives[i]?.current ?? 0;
      const done = cur >= obj.required;
      const statusText = done ? '✓' : `${cur}/${obj.required}`;
      const objColor = done ? '#27ae60' : '#e0d8cc';

      this.questLogPanel.add(this.add.text(detailX + 15, dy, `• ${obj.targetName}  ${statusText}`, {
        fontSize: '10px', color: objColor, fontFamily: FONT,
      }));

      // Progress bar
      const barX = detailX + 15, barY = dy + 14, barW = detailW - 40, barH = 4;
      this.questLogPanel.add(this.add.rectangle(barX, barY, barW, barH, 0x1a1a2e).setOrigin(0, 0).setStrokeStyle(1, 0x333344));
      const fillW = Math.min(barW * (cur / obj.required), barW);
      if (fillW > 0) {
        this.questLogPanel.add(this.add.rectangle(barX, barY, fillW, barH, done ? 0x27ae60 : 0x2471a3).setOrigin(0, 0));
      }
      dy += 24;
    }

    dy += 8;

    // Rewards
    this.questLogPanel.add(this.add.text(detailX + 5, dy, '奖励:', {
      fontSize: '11px', color: '#e0d8cc', fontFamily: FONT, fontStyle: 'bold',
    }));
    dy += 18;

    const rewardParts: string[] = [];
    rewardParts.push(`经验 +${selected.quest.rewards.exp}`);
    rewardParts.push(`金币 +${selected.quest.rewards.gold}`);
    if (selected.quest.rewards.items) {
      rewardParts.push(`物品 x${selected.quest.rewards.items.length}`);
    }
    this.questLogPanel.add(this.add.text(detailX + 15, dy, rewardParts.join('  |  '), {
      fontSize: '10px', color: '#f1c40f', fontFamily: FONT,
    }));
    dy += 20;

    // Prereqs
    if (selected.quest.prereqQuests && selected.quest.prereqQuests.length > 0) {
      dy += 4;
      const prereqNames = selected.quest.prereqQuests.map(pid => {
        const allQuests = (this.zone!.questSystem as any).quests as Map<string, import('../data/types').QuestDefinition>;
        const pq = allQuests.get(pid);
        return pq ? pq.name : pid;
      });
      this.questLogPanel.add(this.add.text(detailX + 5, dy, `前置任务: ${prereqNames.join(', ')}`, {
        fontSize: '9px', color: '#666', fontFamily: FONT,
      }));
    }
  }
```

### Step 6: Make QuestSystem.quests accessible for the panel

In `src/systems/QuestSystem.ts`, the `quests` map is private. The quest log panel needs to access quest definitions for turned-in quests and prereq names. Change `private quests` to just `quests`:

```typescript
// Old:
private quests: Map<string, QuestDefinition> = new Map();
// New:
quests: Map<string, QuestDefinition> = new Map();
```

### Step 7: Verify compilation

Run: `npx tsc --noEmit`

### Step 8: Commit

```bash
git add src/scenes/ZoneScene.ts src/scenes/UIScene.ts src/systems/QuestSystem.ts
git commit -m "feat(quest): quest log panel with J key, tabs, detail view, and progress bars"
```

---

## Parallelization Notes

- **Tasks 1-2** are sequential (types → data)
- **Task 3** depends on Tasks 1-2 (needs quest data with location fields)
- **Tasks 4, 5, 6, 7** depend on Tasks 1-3 but are partially independent:
  - Task 4 (NPC.ts + ZoneScene markers) is independent
  - Tasks 5, 6, 7 all modify UIScene.ts — do sequentially
- **Best execution order**: 1 → 2 → 3 → 4 → 5 → 6 → 7
- Tasks 3 and 4 can be parallelized (different files in ZoneScene; Task 3 modifies `onMonsterKilled` + `interactNPC` + `update`; Task 4 adds `updateNPCQuestMarkers` method)

## Verification Checklist

After all tasks:
1. `npx tsc --noEmit` passes
2. `npm run dev` starts without errors
3. In-game: talk to 村长 → see quest list with [主线]/[支线] labels
4. Accept a quest → NPC marker changes from `!` to `?` gray
5. Minimap shows quest target area highlighted
6. Quest tracker (right side) shows all objectives with progress
7. Press J → quest log panel opens with tabs, quest list, detail view
8. Kill monsters → kill quest progress increments
9. Kill monsters in zone → collect quest progress has chance to increment
10. Walk to explore location → explore quest progress updates with discovery message
11. Talk to forest_hermit → talk quest completes

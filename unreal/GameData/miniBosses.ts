import type { MonsterDefinition, DialogueTree } from './types';

/**
 * Mini-boss definitions — one per zone, distinct from final bosses.
 * Each has elite: true, pre-fight dialogue, and guaranteed enhanced loot.
 */

// ─── Zone 1: 赤幡原 / The Red Field (dark remake, Docs/dark-remake/chapter1.md §10, monsters.md §2.5) ───
// 尸牧婆 / the Corpseherd: a washerwoman from the crusade's baggage train who keeps the dead walking "home". On the main
// path (M3). A ranged elite (range 3.0 authored, so the exporter's DECISIONS M5 override is gone) whose bell-toll bolt
// slows; Wave 2 (package 2F): Call the Flock (two raised at the pull, then one every 8 s, at most 3 alive; they collapse
// when she dies), the kite and Keen (chapter1.md §10). The Sheepfold's six seated dead are map corpse seeds
// (maps/emerald_plains.ts).
// 2F review: 600 HP (was 300). The main path brings the hero at Lv 10 (DR9's exp flow), where 300 HP died before Keen;
// at 600 every class sees the opening raise and, in most fights, Keen (500 left the warrior's Keen at 55-80 % of fights);
// the 8 s repeat raise is for a slower or earlier hero (chapter1.md §10, §19). More (1200+) made an early Lv 5 meeting a
// two-minute fight. She now has more life than the web's zone-2 mini-boss (520), which stays behind the sealed east gate
// (W7) until Chapter 2 is remade.
export const miniBossEmeraldPlains: MonsterDefinition = {
  id: 'miniboss_corpseherd',
  name: '尸牧婆',
  level: 6,
  hp: 600,
  damage: 15,
  defense: 10,
  speed: 45,
  aggroRange: 7,
  attackRange: 3.0,
  attackSpeed: 1300,
  expReward: 95,
  goldReward: [15, 30],
  spriteKey: 'monster_miniboss_corpseherd',
  elite: true,
  isMiniBoss: true,
  animCategory: 'humanoid',
  // The Unburied (1B review; monsters.md §1.2: her breach-violet eyes are the Unburied marker): dead, the bell never
  // tolled for her (chapter1.md §10). Her voice stays the authored human lullaby and handbell (monsters.md §2.5 Sound).
  family: 'unburied',
  // The bell-toll bolt: a breach-violet ripple (DR5) that slows 20 % for 2 s on every hit.
  projectileColor: 0xb02fd0,
  onHitStatus: [{ status: 'slow', chance: 1, value: { kind: 'fixed', amount: 20 }, durationMs: 2000 }],
  hitCapHeroMaxHpPct: 35,
  abilities: [
    // 召羊 Call the Flock at the pull: she rings her bell (Cast01) and two corpses within 6 tiles stand as footmen at 40 %
    // life (no exp or loot, never respawn) under breach-violet markers for 900 ms; once per fight.
    {
      id: 'call_the_flock_pull', kind: 'raise', clip: 'Cast01', windupMs: 900, cooldownMs: 600000, rangeMax: 12, aim: 'corpse',
      priority: 2,
      summon: { monsterId: 'risen_footman', count: 2, at: 'corpse', radius: 6, capAlive: 3, hpPct: 40 },
    },
    // Then one every 8 s: at most 3 alive, at most one raise per 8 s (with the pull's two, the window holds the next).
    {
      id: 'call_the_flock', kind: 'raise', clip: 'Cast01', windupMs: 900, cooldownMs: 8000, initialCooldownMs: 8000, rangeMax: 12,
      aim: 'corpse', priority: 1,
      summon: { monsterId: 'risen_footman', count: 1, at: 'corpse', radius: 6, capAlive: 3, perWindowCap: 1, windowMs: 8000, hpPct: 40 },
    },
    // Keen (below 50 %): a crimson circle r4 round her, 800 ms (Cast02, FX_Keen), slow 25 % for 3 s and no damage
    // (chapter1.md §10; 2F review: a status-only strike, dmgMul 0); every 12 s (the design names no cooldown).
    {
      id: 'keen', kind: 'circle', clip: 'Cast02', windupMs: 800, cooldownMs: 12000, hpBelowPct: 50, aim: 'self',
      shape: { radius: 4 }, dmgMul: 0, status: { type: 'slow', ms: 3000, value: 25 },
    },
  ],
  // She holds 2-3 tiles from the hero (chapter1.md §10's 3-5 band fitted to her range 3.0, decision M5: a kite ends
  // within the attack range; 2F outcomes).
  kite: { min: 2, max: 3 },
  // Her body is a story beat (cs_rf_herd): remains, never raised or eaten.
  corpse: { leaves: 'remains' },
  lootTable: [
    { quality: 'magic', dropRate: 0.8 },
    { quality: 'rare', dropRate: 0.35 },
    { quality: 'legendary', dropRate: 0.03 },
  ],
};

// ─── Zone 2: Twilight Forest ──────────────────────────────────────────
export const miniBossTwilightForest: MonsterDefinition = {
  id: 'miniboss_shadow_weaver',
  name: '暗影织者',
  level: 14,
  hp: 520,
  damage: 28,
  defense: 16,
  speed: 55,
  aggroRange: 7,
  attackRange: 3.0,
  attackSpeed: 1100,
  expReward: 140,
  goldReward: [22, 45],
  spriteKey: 'monster_shadow_weaver',
  elite: true,
  isMiniBoss: true,
  animCategory: 'humanoid',
  lootTable: [
    { quality: 'magic', dropRate: 0.8 },
    { quality: 'rare', dropRate: 0.35 },
    { quality: 'legendary', dropRate: 0.03 },
  ],
};

// ─── Zone 3: Anvil Mountains ──────────────────────────────────────────
export const miniBossAnvilMountains: MonsterDefinition = {
  id: 'miniboss_iron_guardian',
  name: '铁甲守卫',
  level: 24,
  hp: 1100,
  damage: 38,
  defense: 35,
  speed: 30,
  aggroRange: 7,
  attackRange: 2.0,
  attackSpeed: 1600,
  expReward: 240,
  goldReward: [38, 75],
  spriteKey: 'monster_iron_guardian',
  elite: true,
  isMiniBoss: true,
  animCategory: 'large',
  lootTable: [
    { quality: 'magic', dropRate: 0.8 },
    { quality: 'rare', dropRate: 0.4 },
    { quality: 'legendary', dropRate: 0.04 },
  ],
};

// ─── Zone 4: Scorching Desert ─────────────────────────────────────────
export const miniBossScorchingDesert: MonsterDefinition = {
  id: 'miniboss_sand_wraith',
  name: '沙漠亡灵',
  level: 34,
  hp: 1800,
  damage: 50,
  defense: 30,
  speed: 65,
  aggroRange: 8,
  attackRange: 3.0,
  attackSpeed: 1000,
  expReward: 350,
  goldReward: [60, 120],
  spriteKey: 'monster_sand_wraith',
  elite: true,
  isMiniBoss: true,
  animCategory: 'demonic',
  lootTable: [
    { quality: 'magic', dropRate: 0.7 },
    { quality: 'rare', dropRate: 0.45 },
    { quality: 'legendary', dropRate: 0.06 },
  ],
};

// ─── Zone 5: Abyss Rift ──────────────────────────────────────────────
export const miniBossAbyssRift: MonsterDefinition = {
  id: 'miniboss_void_herald',
  name: '虚空先驱',
  level: 44,
  hp: 3200,
  damage: 65,
  defense: 42,
  speed: 50,
  aggroRange: 8,
  attackRange: 3.5,
  attackSpeed: 1000,
  expReward: 450,
  goldReward: [100, 200],
  spriteKey: 'monster_void_herald',
  elite: true,
  isMiniBoss: true,
  animCategory: 'demonic',
  lootTable: [
    { quality: 'rare', dropRate: 0.7 },
    { quality: 'legendary', dropRate: 0.1 },
    { quality: 'set', dropRate: 0.04 },
  ],
};

/** All mini-boss definitions indexed by zone map ID. */
export const MiniBossByZone: Record<string, MonsterDefinition> = {
  emerald_plains: miniBossEmeraldPlains,
  twilight_forest: miniBossTwilightForest,
  anvil_mountains: miniBossAnvilMountains,
  scorching_desert: miniBossScorchingDesert,
  abyss_rift: miniBossAbyssRift,
};

// ─── Pre-fight Dialogue Trees (3+ lines each) ────────────────────────

export const MiniBossDialogues: Record<string, DialogueTree> = {
  // chapter1.md §10: the Corpseherd holds (Taunt) until the three lines are dismissed.
  miniboss_corpseherd: {
    startNodeId: 'line1',
    nodes: {
      line1: {
        id: 'line1',
        text: '嘘……别惊了我的羊群。',
        nextNodeId: 'line2',
      },
      line2: {
        id: 'line2',
        text: '我的两个儿子也在里面。我领着他们走，一圈，又一圈。',
        nextNodeId: 'line3',
      },
      line3: {
        id: 'line3',
        text: '天快黑了，他们得回家。你挡着路了。',
        isEnd: true,
      },
    },
  },
  miniboss_shadow_weaver: {
    startNodeId: 'line1',
    nodes: {
      line1: {
        id: 'line1',
        text: '森林在低语……你感受到了吗？暮色森林的每一棵树，每一片叶子，都在为我传递信息。',
        nextNodeId: 'line2',
      },
      line2: {
        id: 'line2',
        text: '我是暗影织者，操纵黑暗的蛛丝。是我召唤了亡灵大军，让这片森林永远笼罩在暮色之中。',
        nextNodeId: 'line3',
      },
      line3: {
        id: 'line3',
        text: '森林深处隐藏着远古的封印，而你……不配知晓它的秘密。准备迎接永恒的黑暗吧！',
        isEnd: true,
      },
    },
  },
  miniboss_iron_guardian: {
    startNodeId: 'line1',
    nodes: {
      line1: {
        id: 'line1',
        text: '矮人的铁砧……已经沉寂了千年。你踏入这古老的领地，就必须面对守卫者的审判。',
        nextNodeId: 'line2',
      },
      line2: {
        id: 'line2',
        text: '我是远古矮人锻造的铁甲守卫，被赋予了守护山脉遗迹的使命。任何入侵者都将化为废铁。',
        nextNodeId: 'line3',
      },
      line3: {
        id: 'line3',
        text: '在这座山脉的深处，沉睡着矮人王朝最后的秘宝。而你，绝不能活着离开这里！',
        isEnd: true,
      },
    },
  },
  miniboss_sand_wraith: {
    startNodeId: 'line1',
    nodes: {
      line1: {
        id: 'line1',
        text: '灼热的沙风中……你听到了吗？那是千年前毁灭的沙漠王国中亡魂的哀嚎。',
        nextNodeId: 'line2',
      },
      line2: {
        id: 'line2',
        text: '我曾是沙漠王国的大祭司，如今化为不死的亡灵。火焰裂隙的力量让我重生，也让我永受诅咒。',
        nextNodeId: 'line3',
      },
      line3: {
        id: 'line3',
        text: '这片沙漠之下埋藏着王国覆灭的真相。你想知道？那就用你的鲜血来换取答案吧！',
        nextNodeId: 'line4',
      },
      line4: {
        id: 'line4',
        text: '沙漠的怒火将吞噬一切……准备好了吗，冒险者？',
        isEnd: true,
      },
    },
  },
  miniboss_void_herald: {
    startNodeId: 'line1',
    nodes: {
      line1: {
        id: 'line1',
        text: '深渊之门即将完全打开……你来得太迟了，凡人。虚空的意志不可阻挡。',
        nextNodeId: 'line2',
      },
      line2: {
        id: 'line2',
        text: '我是虚空先驱，深渊裂隙的第一使者。在魔王降临之前，我的使命是清除所有障碍。',
        nextNodeId: 'line3',
      },
      line3: {
        id: 'line3',
        text: '你以为封印深渊就能拯救这个世界？天真。深渊的力量早已渗透到了每一个角落。',
        nextNodeId: 'line4',
      },
      line4: {
        id: 'line4',
        text: '来吧，让我看看人类最后的挣扎！虚空的黑暗将是你的归宿！',
        isEnd: true,
      },
    },
  },
};

/** Mini-boss spawn positions per zone (col, row on the 120×120 grid).
 *  Placed in distinct, non-camp locations away from final bosses. */
export const MiniBossSpawns: Record<string, { col: number; row: number }> = {
  emerald_plains: { col: 60, row: 55 },
  twilight_forest: { col: 55, row: 30 },
  anvil_mountains: { col: 50, row: 70 },
  scorching_desert: { col: 70, row: 50 },
  abyss_rift: { col: 45, row: 55 },
};

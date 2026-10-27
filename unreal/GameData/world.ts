/**
 * Overrides for tables that live inside web system files (implementation-plan.md §2.4). Those files are code, so they are
 * not redirected; instead each exporter table applies `override ?? web` per entry and records what it changed in a
 * `remake` block of the written file (`{"remake": {"source": [...], "replaced": ["achievements.ach_kill_slime"], ...}}`).
 *
 * Every table is optional. Per table:
 *   replace  web entries by key (the key must exist in the web table; for id-keyed lists the value may carry a new id,
 *            which renames the entry in place and keeps its slot)
 *   add      new entries, appended in this order (the key must not exist in the web table)
 *   remove   web entries to drop (the keys must exist)
 * An entry is replaced whole: to change one field, spread the web entry (import it from `@web/...`; system modules are
 * not redirected, so they may be imported here). Unknown table names, unknown keys and a table the exporter did not
 * apply fail the export (unreal/Tools/export-data/src/remake.ts).
 *
 * Chapter 1 of the dark remake (Docs/dark-remake/chapter1.md §13-15, monsters.md §3; package 1B part 1, world data):
 * the two Chapter 1 achievements re-targeted, the plains event pool, the `march` mood family (zone mood, the dawn mood
 * after the chapter, the Breach Rim region), its fog, light scale and weather, the camp kit and terrain style `march`,
 * and the seven elite affixes re-voiced and recoloured off the hero's ember hue (DR5). The Ember Tower keeps the warm
 * plains look (its mood theme stays `plains`; it keeps the plains pollen it used to borrow from emerald_plains).
 */
import type { AchievementDefinition, CampTheme, RemakeEliteAffixDefinition } from './types';
import type { ZoneEventData } from '@web/systems/RandomEventSystem';
import { ZONE_EVENT_DATA } from '@web/systems/RandomEventSystem';
import type { ZoneMood, ZonePaletteEntry } from '@web/graphics/ZonePalette';
import type { EliteAffixType } from '@web/systems/EliteAffixSystem';
import { ELITE_AFFIX_DEFINITIONS } from '@web/systems/EliteAffixSystem';
import type { TerrainTheme } from '@web/graphics/terrain/TerrainStyles';

export interface EntryOverride<T> {
  readonly replace?: Readonly<Record<string, T>>;
  readonly add?: Readonly<Record<string, T>>;
  readonly remove?: readonly string[];
}

/**
 * [W2] A zone's ambush champion pack (K6, the 2D review's event champion packs; chapter1.md §13: the Muster, a
 * standard-bearer and three footmen sharing one affix, 10 % of ambushes; MapData.h AmbushChampionDef). Only when present,
 * an ambush draws once more and, under `chance` (0 < chance <= 1), spawns `mix` (counts >= 1, 20 members at most; the
 * zone's monsters, never the story boss) as one pack in its place, `leader` (a member's id) leading it; `sharedAffix`
 * rolls the zone's affixes once and gives them to every member (each keeps those its family may roll); `leaderLoot`
 * (needs `leader`; monsters.md §2.8 "a champion Muster drops one guaranteed magic item"): the leader's kill drops one
 * more equipment item of exactly that quality at its level, after its regular loot. Feature `events.ambushChampion`
 * (checked by the exporter's world.ts).
 */
export interface RemakeAmbushChampion {
  chance: number;
  mix: { monsterId: string; count: number }[];
  leader?: string;
  sharedAffix?: boolean;
  leaderLoot?: 'magic' | 'rare';
  /** [2F review] Once this world flag is set the champion pack is off (no extra draw). Feature `events.ambushVariants`. */
  notFlag?: string;
}

/**
 * [2F review] An ambush variant (random_events.json `zones.<zone>.ambushVariants`, feature `events.ambushVariants`): the
 * first whose world flag is set replaces `ambushMonsters` for the zone's ambushes (and the rescue guards that fall back to
 * them).
 */
export interface RemakeAmbushVariant {
  whenFlag: string;
  monsters: string[];
}

/** A zone's event data plus the remake's rescue guards (random_events.json `zones.<zone>.rescueMonsters`; the core uses
 * `ambushMonsters` when it is absent), its [W2] ambush champion pack and its [2F review] ambush variants. */
export type RemakeZoneEventData = ZoneEventData & {
  rescueMonsters?: string[];
  ambushChampion?: RemakeAmbushChampion;
  ambushVariants?: RemakeAmbushVariant[];
};

/**
 * [W2] A status row the remake adds to status_effects.json (K6; SkillData.h StatusStackRule): Ashnumb's `stackCount`
 * rules (one instance whose value counts stacks; each application adds them up to maxStacks and restarts the shared
 * timer): `healingReceivedPerStack` (0..1) off every heal and the regeneration, `slowAtMaxStacksPct` (0..100) at full
 * stacks. `tickIntervalMs` 0 (it deals no damage). Its name key `sys.statusEffect.name.<type>` must exist (i18n).
 */
export interface RemakeStatusEffectRow {
  tickIntervalMs: number;
  stacking: 'stackCount';
  stacks: { maxStacks: number; healingReceivedPerStack?: number; slowAtMaxStacksPct?: number };
}

/** WeatherSystem's (module-private) WeatherConfig, widened for remake ambiences. */
export interface RemakeWeather {
  type: string;
  ambience?: string;
  tint?: number;
}

/** zone_moods.json `port.fog` row (DECISIONS P14). */
export interface RemakeFog {
  color: number;
  maxOpacity: number;
  density: number;
}

/**
 * zone_moods.json `port.windupTint` row (DECISIONS DR5): the body tint a monster's wind-up ramps to (UE
 * AAbyssCharacterActor, CPD Telegraph at up to 0.85) under that mood; `elite` for elites and mini-bosses. Moods without
 * a row keep the web port's #FFC4B0 / #FF7A5C.
 */
export interface RemakeWindupTint {
  normal: number;
  elite: number;
}

export interface RemakeOverrides {
  /** achievements.json `achievements` (AchievementSystem ACHIEVEMENTS), by id. */
  readonly achievements?: EntryOverride<AchievementDefinition>;
  /** random_events.json `zones` and shops.json `wanderingMerchant` (RandomEventSystem ZONE_EVENT_DATA), by zone id. */
  readonly randomEvents?: EntryOverride<RemakeZoneEventData>;
  /** zone_moods.json. */
  readonly zoneMoods?: {
    /** `themeByZone` (LightingSystem ZONE_THEME_BY_ID), zone id -> mood theme. */
    readonly themeByZone?: EntryOverride<string>;
    /** `moods` (ZonePalette ZONE_MOODS), by theme. */
    readonly moods?: EntryOverride<ZoneMood>;
    /** `palettes` (ZonePalette ZONE_PALETTES), by theme. */
    readonly palettes?: EntryOverride<ZonePaletteEntry>;
    /** `weather` (WeatherSystem ZONE_WEATHER), by zone id. */
    readonly weather?: EntryOverride<RemakeWeather>;
    /** `weatherByTheme` (WeatherSystem THEME_WEATHER), theme -> zone id whose weather it borrows. */
    readonly weatherByTheme?: EntryOverride<string>;
    /** `campThemes` (camp-themes CAMP_THEMES), by theme. */
    readonly campThemes?: EntryOverride<CampTheme>;
    /** `port.fog` (exporter table, DECISIONS P14), by theme. */
    readonly fog?: EntryOverride<RemakeFog>;
    /**
     * `port.lightScale` (exporter table; DECISIONS R18 override), by mood theme: the fake-light scale of that mood in place
     * of R18's formula (max(0.12, ambientAlpha x (1 - luma(ambient)) x 1.4)). The web table is empty.
     */
    readonly lightScale?: EntryOverride<number>;
    /**
     * `port.moodWeather` (exporter table), by mood theme: the weather / ambience a mood brings where it blends in over a
     * zone (the dawn mood after the chapter, a mood region). A zone's own weather stays `weather`. The web table is empty.
     */
    readonly moodWeather?: EntryOverride<RemakeWeather>;
    /**
     * `port.windupTint` (exporter table; DECISIONS DR5), by mood theme: the monsters' wind-up body tint under that mood
     * in place of the web port's #FFC4B0 / #FF7A5C (UE AbyssThemeRules::WindupTintFor). The web table is empty.
     */
    readonly windupTint?: EntryOverride<RemakeWindupTint>;
  };
  /** elite_affixes.json. */
  readonly eliteAffixes?: {
    /**
     * `definitions` (EliteAffixSystem ELITE_AFFIX_DEFINITIONS; `order` follows), by affix type. K3: an entry may carry
     * `onDeath`, `riseAgain` and `families`, and `add` may append the `unburied` type (RemakeEliteAffixDefinition;
     * checked by the exporter's combat.ts).
     */
    readonly definitions?: EntryOverride<RemakeEliteAffixDefinition>;
    /** `zoneAffixCounts` (EliteAffixSystem ZONE_AFFIX_COUNTS), zone id -> [min, max]. */
    readonly zoneAffixCounts?: EntryOverride<[number, number]>;
  };
  /** terrain_styles.json `themes` (TerrainStyles TERRAIN_THEMES), by theme. */
  readonly terrainStyles?: EntryOverride<TerrainTheme>;
  /**
   * status_effects.json (K6), by status type: `add` only (the web's six keep their rules): the remake's statuses the core
   * knows (`ashnumb`). Feature `status.ashnumb` (checked by the exporter's combat.ts).
   */
  readonly statusEffects?: EntryOverride<RemakeStatusEffectRow>;
}

// ── Elite affixes (monsters.md §3): same mechanics and stats, re-voiced, recoloured off the ember hue (DR5) ──
// Hues: breach-violet #B02FD0 family for breach magic, crimson #A3221E-#C0283A for the oath, bone and grave-cold for the
// rest; nothing in the hero's ember family (#FF7A2A, hue 15-35 deg, sat > .6). The wave-2 telegraphs / on-death effects
// of Scorched, Breachwalker, Dirgecursed and Gravecold come with the K2 / K3 packages. 1B review: three of monsters.md §3's
// names collided with story terms an elite label puts before a monster's name (焚誓 the prologue's Burned Oath, 赤誓 the
// Red-Oath faction, 丧钟 the hub 丧钟堡), so they are 焦灼 Scorched, 蛮力 Brutish and 哀咒 Dirgecursed.
const AFFIX_LOOK: Record<EliteAffixType, { name: string; nameEn: string; vfxColor: number }> = {
  fire_enhanced: { name: '焦灼', nameEn: 'Scorched', vfxColor: 0xb02fd0 },
  swift: { name: '驱策', nameEn: 'Driven', vfxColor: 0xd8ccb0 },
  teleporting: { name: '渊行', nameEn: 'Breachwalker', vfxColor: 0xd08cff },
  extra_strong: { name: '蛮力', nameEn: 'Brutish', vfxColor: 0xc0283a },
  curse_aura: { name: '哀咒', nameEn: 'Dirgecursed', vfxColor: 0x7a2a90 },
  vampiric: { name: '噬渊', nameEn: 'Breach-Drinker', vfxColor: 0x6a0f2a },
  frozen: { name: '墓寒', nameEn: 'Gravecold', vfxColor: 0xcfe8ff },
};
// Wave 2 (package 2F, monsters.md §3 *Added*): Scorched bursts in breach fire 1 s after it dies (r2, crimson decal,
// capped like an exploder; and, an exploder's rule (monsters.md §1.3, 2F review), it hurts monsters x2 so the player can
// use it), Gravecold leaves a frost ring 600 ms after (r2.5, slow 30 % for 2.5 s). K3 on-death blocks: the burst's damage
// scales with the difficulty, the dodge roll's i-frames avoid it, Death Save holds; bursts landing together share one
// 35 % cap per second (2F review, OnDeath.h kOnDeathWindowCapPct: a Scorched Muster's four bursts).
const AFFIX_ON_DEATH: Partial<Record<EliteAffixType, Pick<RemakeEliteAffixDefinition, 'onDeath'>>> = {
  fire_enhanced: {
    onDeath: {
      kind: 'burst', delayMs: 1000, shape: { kind: 'circle', radius: 2 }, dmg: 20, damageType: 'fire', capHeroMaxHpPct: 20,
      hitsMonsters: true, monsterDmgMul: 2,
    },
  },
  frozen: {
    onDeath: { kind: 'ring', delayMs: 600, shape: { kind: 'circle', radius: 2.5 }, damageType: 'ice', status: { type: 'slow', ms: 2500, value: 30 } },
  },
};
const affixDefinitions = Object.fromEntries((Object.keys(AFFIX_LOOK) as EliteAffixType[])
  .map(type => [type, { ...ELITE_AFFIX_DEFINITIONS[type], ...AFFIX_LOOK[type], ...AFFIX_ON_DEATH[type] }])) as Record<EliteAffixType, RemakeEliteAffixDefinition>;
// monsters.md §3 (new, Wave 2): 不葬 Unburied, the Unburied family only: the elite rises once at 50 % life 3 s after it dies
// (a crit does not stop it; a fire killing blow or a burned corpse does; K3). No other stat; the loot bonus of Driven.
// 2F review: never rolled for a monster that cannot rise (EliteAffixes.h: a boss role - the Corpseherd, Valdras, the hunt
// leaders - a Raised body, a body that leaves no raisable corpse).
const UNBURIED_AFFIX: RemakeEliteAffixDefinition = {
  type: 'unburied', name: '不葬', nameEn: 'Unburied',
  damageMult: 1.0, speedMult: 1.0, hpMult: 1.0, defenseMult: 1.0, extraFireDamage: 0, teleportCooldownMs: 0, curseAuraRadius: 0,
  curseAuraReduction: 0, lifestealFraction: 0, freezeChance: 0, vfxColor: 0xd08cff, lootQualityBonus: 3,
  riseAgain: { chance: 1, delayMs: 3000, hpPct: 50 },
  families: ['unburied'],
};

// ── Terrain style `march` (chapter1.md §15): trampled mud, ash-grey dead grass, scorched patches, wheel ruts, still black
// water with an oily sheen. Bases from the art lane's reference march ground (Art/blender/README.md "Review": ash grass
// #4C4940, mud #3A332D, stone #55524C, camp ash #47423C; water lifted to #262A30 below); the layer kinds and ranks are the plains ones.
// No bones (DR6: no exposed bone in Chapter 1). ──
const MARCH_TERRAIN: TerrainTheme = {
  ground: {
    0: {
      base: 0x4c4940, rank: 4, lip: 0x5e5a50,
      layers: [
        { kind: 'patch', count: 4, colors: [0x46433b, 0x524e45, 0x46433b], r: [12, 24] },
        { kind: 'patch', count: 3, colors: [0x575247, 0x2e2b27], r: [4, 10] },
        { kind: 'tuft', count: 16, color: 0x5a564c, light: 0x77705f, size: [3.2, 5.2] },
      ],
      details: ['bigTuft', 'flowers', 'stones', 'bigTuft', 'crackStar', 'stones'],
      // Ash and dull poppies.
      accents: [0x7a2a26, 0x8a8478, 0x6a2420, 0x9a9284],
    },
    1: {
      base: 0x3a332d, rank: 1,
      layers: [
        { kind: 'patch', count: 4, colors: [0x352e29, 0x40382f], r: [10, 22] },
        // Wheel ruts.
        { kind: 'crack', count: 2, color: 0x2a241f, light: 0x4a4138, len: [8, 14], width: 2 },
        { kind: 'pebble', count: 8, color: 0x4a4239, shade: 0x2c2620, light: 0x5c5247, size: [1.1, 2.2] },
      ],
      details: ['stones', 'straw', 'crackStar', 'stones'],
      accents: [0x5a5045],
    },
    2: {
      base: 0x55524c, rank: 2,
      layers: [
        { kind: 'slabs', cells: 2, mortar: 0x403d38, mortarW: 0.035, jitter: 0.06, bevelLight: 0x66625a, bevelShade: 0x47443f },
        { kind: 'patch', count: 3, colors: [0x4c4940], r: [3, 7] },
      ],
      details: ['bigTuft', 'crackStar', 'stones'],
      accents: [0x4c4940],
    },
    // Water: the env art review's lift (Art/blender/README.md, terrain_march `waterOverride`): the tar-black #121418
    // (L* 6) read as a dead hole once the runtime sampled this theme; L* 17 with L* 21.5 shallows and a faint grey sheen
    // band at the shore, matching the painted T_Water_March_BC.
    3: {
      base: 0x262a30, rank: 0,
      layers: [
        { kind: 'patch', count: 3, colors: [0x22262b, 0x2c3036], r: [14, 26] },
        // The oily sheen.
        { kind: 'wave', count: 3, color: 0x3a3550, len: [6, 11], alpha: 0.4 },
      ],
      details: ['stones'],
      accents: [0x2a2630],
      liquid: { shallow: 0x30343a, foam: 0x4e5258, bank: 0x2c2620 },
    },
    5: {
      base: 0x47423c, rank: 2,
      layers: [
        { kind: 'patch', count: 4, colors: [0x403b35, 0x4e4842], r: [10, 20] },
        // Ash flakes.
        { kind: 'leaf', count: 4, colors: [0x5a554e, 0x6a645b], size: [1.6, 3] },
        { kind: 'pebble', count: 6, color: 0x524c45, shade: 0x332e29, light: 0x625b52, size: [1, 1.8] },
      ],
      details: ['straw', 'stones', 'straw'],
      accents: [0x5a5045],
    },
  },
  // Ash-crusted boulders, broken stone; the accent is the dull poppy.
  outcrop: { kind: 'mossBoulder', rock: 0x5a5852, cap: 0x4c4940, accent: 0x7a2a26, extra: 0x3e3b35, shadow: 0x1e1c1a },
  // The pew palisade: charred timber, iron bands, the crusade's crimson.
  palisade: { kind: 'darkStakes', wood: 0x3a2e24, band: 0x5e5248, accent: 0x5e1012 },
};

export const REMAKE_OVERRIDES: RemakeOverrides = {
  // chapter1.md §14: the Chapter 1 kill achievements re-targeted (rewards kept).
  achievements: {
    replace: {
      ach_kill_slime: {
        id: 'ach_kill_breachclot', name: '止血', description: '击杀50个渊血凝块', type: 'kill', targetId: 'breachclot', required: 50,
        reward: { stat: 'lck', value: 2 },
      },
      ach_kill_goblin_chief: {
        id: 'ach_kill_valdras', name: '撤退令', description: '击败焚旗元帅·瓦德拉斯', type: 'kill', targetId: 'valdras', required: 1,
        title: '撤退令',
      },
      // 1B review: the panel lists every achievement, locked ones too, so the web names are re-voiced (ids, types, counts
      // and rewards kept; ach_kill_demon_lord gets a neutral name until Chapter 5's pass, DR11).
      ach_first_kill: { id: 'ach_first_kill', name: '第一个名字', description: '放倒第一个敌人', type: 'kill', required: 1, title: '灰中人' },
      ach_kill_100: { id: 'ach_kill_100', name: '一百下钟声', description: '放倒100个敌人', type: 'kill', required: 100, reward: { stat: 'damage', value: 2 } },
      ach_kill_500: {
        id: 'ach_kill_500', name: '掘墓人', description: '放倒500个敌人', type: 'kill', required: 500, reward: { stat: 'damage', value: 5 },
        title: '掘墓人',
      },
      ach_kill_demon_lord: {
        id: 'ach_kill_demon_lord', name: '火的尽头', description: '在深渊之底，结束这一切', type: 'kill', targetId: 'demon_lord', required: 1,
        reward: { stat: 'str', value: 10 }, title: '守火人',
      },
      ach_level_10: { id: 'ach_level_10', name: '还站着', description: '达到10级', type: 'level', required: 10 },
      ach_level_25: { id: 'ach_level_25', name: '老兵', description: '达到25级', type: 'level', required: 25, title: '老兵' },
      ach_level_50: {
        id: 'ach_level_50', name: '不熄', description: '达到50级', type: 'level', required: 50, reward: { stat: 'str', value: 5 },
        title: '不熄者',
      },
      ach_explore_all: {
        id: 'ach_explore_all', name: '走遍焚境', description: '探索所有区域', type: 'explore', required: 5, reward: { stat: 'lck', value: 5 },
        title: '行路人',
      },
      ach_quest_10: { id: 'ach_quest_10', name: '十件差事', description: '完成10个任务', type: 'quest', required: 10, reward: { stat: 'lck', value: 3 } },
      ach_collect_legendary: {
        id: 'ach_collect_legendary', name: '死者的遗物', description: '收集一件传奇装备', type: 'collect', required: 1, title: '拾遗人',
      },
    },
  },
  // chapter1.md §13: same kinds, weights, counts and rewards. Here: the ambush pool (Hands from the Mud's footmen and
  // gnawers), the rescue's guards (`rescueMonsters`, remake-only: the Bound Pilgrim is held by Red-Oath zealots; the core
  // falls back to ambushMonsters without it), the Five Handbells puzzle and the pilgrim's name. The events' Red Field
  // lines (ambush toll, the dead knight's strongbox, the Relic Peddler, the pilgrim) are per-zone i18n keys
  // (./i18n/ch1World.ts, RandomEvents.cpp RevZoneKey). Still the plains art: the rune-pillar puzzle prop and the lost
  // traveller's look wait for the art lane's handbell post and pilgrim (no art ids yet); the Muster champion pack is W2.
  randomEvents: {
    replace: {
      emerald_plains: {
        ...ZONE_EVENT_DATA.emerald_plains,
        ambushMonsters: ['risen_footman', 'gravegnawer'],
        puzzleDescriptions: [
          { prompt: '五口小钟挂在断桩上。钟舌上刻着数，从一到五。', solution: '照钟舌上的数，从一敲到五', reward: '五口钟依次响过。断桩下压着一个油布包。', rewardGold: 50, rewardExp: 30 },
        ],
        rescueNpcName: '被缚的朝圣者',
        rescueMonsters: ['oath_zealot'],
        // Wave 2 (K6, chapter1.md §13): one ambush in ten is the champion Muster: a standard-bearer and three footmen
        // sharing one affix (the zone's one, rolled from the standard-bearer's Unburied pool), the standard-bearer leading;
        // it drops one guaranteed magic item (monsters.md §2.8).
        // 2F review (chapter1.md §8 cs_rf_finale: "the dead who still stood lie down"): no Muster after dawn.
        ambushChampion: {
          chance: 0.1,
          mix: [{ monsterId: 'dead_standard_bearer', count: 1 }, { monsterId: 'risen_footman', count: 3 }],
          leader: 'dead_standard_bearer', sharedAffix: true, leaderLoot: 'magic', notFlag: 'rf_chapter_dawn',
        },
        // 2F review: after dawn the hands from the mud are the scavengers' (the footmen lie down).
        ambushVariants: [{ whenFlag: 'rf_chapter_dawn', monsters: ['gravegnawer'] }],
      },
    },
  },
  zoneMoods: {
    // emerald_plains (赤幡原) takes the march mood; ember_tower keeps plains.
    themeByZone: { replace: { emerald_plains: 'march' } },
    // chapter1.md §15 (P14 applies 55 % of a mood).
    moods: {
      add: {
        march: {
          ambient: 0xa8aebb, ambientAlpha: 0.30, vignette: 0x08070a, vignetteAlpha: 0.58,
          haze: 0x5e5a57, hazeAlpha: 0.12,
          saturation: 0.68, contrast: 1.12, lift: [-0.010, -0.006, 0.004], gain: [0.0, 0.004, 0.020],
        },
        // After the chapter (W1: on chapterCompleteQuest; W2: the rf_chapter_dawn flag).
        march_dawn: {
          ambient: 0xc8b8a0, ambientAlpha: 0.26, vignette: 0x120e0a, vignetteAlpha: 0.45,
          haze: 0x7a7064, hazeAlpha: 0.08,
          saturation: 0.78, contrast: 1.08, lift: [0.004, 0.002, -0.004], gain: [0.020, 0.012, 0.0],
        },
        // The Breach Rim region (maps emerald_plains `regions`): darker, the violet uplight from the crack.
        breach_rim: {
          ambient: 0x6a4a8a, ambientAlpha: 0.35, vignette: 0x0a0610, vignetteAlpha: 0.62,
          haze: 0x2a1a36, hazeAlpha: 0.14,
          saturation: 0.60, contrast: 1.15, lift: [-0.012, -0.014, 0.010], gain: [0.010, 0.0, 0.030],
        },
      },
    },
    weather: {
      // Falling ash over the march (ember motes near the breach are the region's).
      replace: { emerald_plains: { type: 'none', ambience: 'ashfall', tint: 0xcfcac0 } },
      // The homestead keeps the plains pollen it borrowed from emerald_plains through its theme.
      add: { ember_tower: { type: 'none', ambience: 'pollen', tint: 0xffe8a0 } },
    },
    // Plains-themed places that have no weather of their own (sub-dungeons, floors) keep the plains pollen.
    weatherByTheme: { replace: { plains: 'ember_tower' }, add: { march: 'emerald_plains' } },
    // Charred timber, the pew palisade, Knellwatch's banner (a black field with a bell); the hearth is the only warm light.
    campThemes: {
      add: {
        march: {
          wallColor: '#2e2620', wallDark: '#1a1512', wallLight: '#463a30', wallTop: '#5a4c40', groundColor: '#24201c',
          bannerColor: '#141214', bannerDark: '#0a090a', torchFlame: 0xffb060, tentColor: '#3a3028',
        },
      },
    },
    fog: {
      add: {
        march: { color: 0x4e525c, maxOpacity: 0.40, density: 1.15 },
        march_dawn: { color: 0x6a6660, maxOpacity: 0.30, density: 0.95 },
        breach_rim: { color: 0x3a2a48, maxOpacity: 0.45, density: 1.25 },
      },
    },
    // R18 override: the dark grade would leave the fake lights at the 0.12 floor (march's formula value is 0.134).
    lightScale: { add: { march: 0.32, march_dawn: 0.22, breach_rim: 0.40 } },
    moodWeather: {
      add: {
        march_dawn: { type: 'none', ambience: 'dust_motes' },
        breach_rim: { type: 'none', ambience: 'sparks', tint: 0xb02fd0 },
      },
    },
    // DR5 hazard language on the body: the wind-up ramps to warning-crimson (the telegraph rim #D11F2E), elites to the
    // contact flash #FF3A48 (M_AF_Telegraph contact); nothing in the hero's ember family.
    windupTint: {
      add: {
        march: { normal: 0xd11f2e, elite: 0xff3a48 },
        march_dawn: { normal: 0xd11f2e, elite: 0xff3a48 },
        breach_rim: { normal: 0xd11f2e, elite: 0xff3a48 },
      },
    },
  },
  eliteAffixes: {
    definitions: { replace: affixDefinitions, add: { unburied: UNBURIED_AFFIX } },
  },
  terrainStyles: { add: { march: MARCH_TERRAIN } },
  // monsters.md §4 (Wave 2, K6): 麻灰 Ashnumb, from breach-blood contact: each stack -10 % healing received, at most 5,
  // slowed 20 % at 5; one shared 6 s timer refreshed by every landed hit; the antidote cleanses it.
  statusEffects: {
    add: { ashnumb: { tickIntervalMs: 0, stacking: 'stackCount', stacks: { maxStacks: 5, healingReceivedPerStack: 0.1, slowAtMaxStacksPct: 20 } } },
  },
};

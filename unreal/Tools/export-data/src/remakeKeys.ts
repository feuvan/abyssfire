/**
 * Record key lists for the remake field gate (implementation-plan.md §2.6; `checkRecordKeys` in remake.ts).
 *
 * Every content record the exporter writes (monster, quest, map, NPC, cutscene, story trigger) may only carry keys of its web type or
 * remake-only fields typed in unreal/GameData/types.ts and registered in `REMAKE_FIELDS`; anything else (a typo such as
 * `familly`, a field built through a spread or a cast that tsc cannot see, a field a package typed but never
 * registered) fails the export instead of leaking into unreal/Data past REMAKE_SCHEMA.
 *
 * The lists are pinned to the types by `satisfies Exact<...>`: tsc (run by run.mjs before every export) fails when a
 * web type gains or loses a key, or when GameData/types.ts types a remake field that is missing here — add it to the
 * level's `remake` list (and a REMAKE_FIELDS row; the export checks that at start). Levels are dotted paths into the
 * record (`[]` = each array element), the same notation as REMAKE_FIELDS.
 */
import type * as Web from '../../../../src/data/types';
import type * as WebStory from '../../../../src/data/story/types';
import type * as GD from '../../../GameData/types';
import type * as GDStory from '../../../GameData/story/types';
import type { RemakeKind } from './remake';

/** The keys of every member of a union (keyof of a union keeps only the common ones). */
type KeysOf<T> = T extends unknown ? Extract<keyof T, string> : never;
type Elem<T> = NonNullable<T> extends readonly (infer E)[] ? E : never;
/** An object literal with exactly the keys K (a missing or an extra key is a tsc error). */
type Exact<K extends string> = Record<K, true>;
type Remade<G, W> = Exclude<KeysOf<G>, KeysOf<W>>;

export interface LevelKeys {
  /** Keys of the web type at this level. */
  web: readonly string[];
  /** Remake-only keys typed in GameData/types.ts at this level (each needs a REMAKE_FIELDS row or a DERIVED_ONLY entry). */
  remake: readonly string[];
}

const keys = (o: object): string[] => Object.keys(o);

type MonsterRemade = Remade<GD.MonsterDefinition, Web.MonsterDefinition>;
const monster = {
  web: { id: true, name: true, level: true, hp: true, damage: true, defense: true, speed: true, aggroRange: true, attackRange: true,
    attackSpeed: true, expReward: true, goldReward: true, spriteKey: true, elite: true, isMiniBoss: true, isSubDungeonMiniBoss: true,
    lootTable: true, bossSkills: true, animCategory: true } satisfies Exact<KeysOf<Web.MonsterDefinition>>,
  remake: { family: true, damageType: true, onHitStatus: true, projectileColor: true, abilities: true, hitCapHeroMaxHpPct: true,
    onDeath: true, corpse: true, riseAgain: true, phases: true, packAggro: true, kite: true, block: true, morale: true,
    aura: true } satisfies Exact<MonsterRemade>,
};

const quest = {
  web: { id: true, name: true, description: true, zone: true, type: true, category: true, objectives: true, rewards: true,
    prereqQuests: true, level: true, questArea: true, escortNpc: true, defendTarget: true, clues: true, craftPhases: true,
    reacceptable: true, hunts: true } satisfies Exact<KeysOf<Web.QuestDefinition>>,
  remake: { progressKey: true, setsFlags: true } satisfies Exact<Remade<GD.QuestDefinition, Web.QuestDefinition>>,
};
type QuestReward = Web.QuestReward;
const questRewards = {
  web: { exp: true, gold: true, items: true, petReward: true, embers: true, choices: true, choiceQuality: true } satisfies Exact<KeysOf<QuestReward>>,
  remake: { choiceLabels: true, permanent: true } satisfies Exact<Remade<GD.QuestReward, Web.QuestReward>>,
};
type WebDefend = NonNullable<Web.QuestDefinition['defendTarget']>;
const questDefend = {
  web: { name: true, spriteKey: true, col: true, row: true, totalWaves: true } satisfies Exact<KeysOf<WebDefend>>,
  remake: { waves: true, approachDeg: true } satisfies Exact<Remade<GD.RemakeDefendTarget, WebDefend>>,
};
const questObjective = {
  web: { type: true, targetId: true, targetName: true, required: true, current: true, location: true, source: true, itemKind: true,
    labelKey: true } satisfies Exact<KeysOf<Web.QuestObjective>>,
  remake: {} satisfies Exact<Remade<Elem<GD.QuestDefinition['objectives']>, Web.QuestObjective>>,
};

const map = {
  web: { id: true, name: true, cols: true, rows: true, tiles: true, collisions: true, spawns: true, camps: true, playerStart: true,
    exits: true, levelRange: true, bgColor: true, theme: true, seed: true, decorations: true, safeZoneRadius: true, petSpawns: true,
    hiddenAreas: true, subDungeonEntrances: true, storyDecorations: true, fieldNpcs: true } satisfies Exact<KeysOf<Web.MapData>>,
  remake: { presentationTheme: true, regions: true, corpseSeeds: true } satisfies Exact<Remade<GD.MapData, Web.MapData>>,
};
const mapEntrance = {
  web: { id: true, name: true, col: true, row: true, targetSubDungeon: true } satisfies Exact<KeysOf<Web.SubDungeonEntrance>>,
  remake: { sealed: true, sealedTextKey: true } satisfies Exact<Remade<GD.SubDungeonEntrance, Web.SubDungeonEntrance>>,
};
type WebSpawn = Elem<Web.MapData['spawns']>;
const mapSpawn = {
  web: { col: true, row: true, monsterId: true, count: true } satisfies Exact<KeysOf<WebSpawn>>,
  remake: { mix: true, leader: true, variants: true } satisfies Exact<Remade<Elem<GD.MapData['spawns']>, WebSpawn>>,
};

type Npc = Web.NPCDefinition;
const npc = {
  web: { id: true, name: true, type: true, dialogue: true, shopItems: true, quests: true, dialogueTree: true, spriteId: true } satisfies Exact<KeysOf<Npc>>,
  remake: { lines: true, visibleWhen: true } satisfies Exact<Remade<GD.NPCDefinition, Web.NPCDefinition>>,
};

const cutscene = {
  web: { id: true, steps: true } satisfies Exact<KeysOf<WebStory.Cutscene>>,
  remake: {} satisfies Exact<Remade<GDStory.Cutscene, WebStory.Cutscene>>,
};
type Step = WebStory.CutsceneStep;
const cutsceneStep = {
  web: { kind: true, text: true, speaker: true, target: true, ms: true, title: true, subtitle: true, intensity: true, color: true } satisfies Exact<KeysOf<Step>>,
  remake: { anim: true } satisfies Exact<Remade<GDStory.CutsceneStep, WebStory.CutsceneStep>>,
};

const trigger = {
  web: { on: true, questId: true, monsterId: true, zoneId: true, cutscene: true, grantPet: true } satisfies Exact<KeysOf<WebStory.StoryTrigger>>,
  remake: { whileQuest: true } satisfies Exact<Remade<GDStory.StoryTrigger, WebStory.StoryTrigger>>,
};

const level = (l: { web: object; remake: object }): LevelKeys => ({ web: keys(l.web), remake: keys(l.remake) });

/** Per record kind, the checked levels ('' = the record itself) and their allowed keys. */
export const RECORD_KEYS: Readonly<Record<RemakeKind, Readonly<Record<string, LevelKeys>>>> = {
  monster: { '': level(monster) },
  quest: { '': level(quest), rewards: level(questRewards), defendTarget: level(questDefend), 'objectives[]': level(questObjective) },
  map: { '': level(map), 'subDungeonEntrances[]': level(mapEntrance), 'spawns[]': level(mapSpawn) },
  npc: { '': level(npc) },
  cutscene: { '': level(cutscene), 'steps[]': level(cutsceneStep) },
  trigger: { '': level(trigger) },
};

/**
 * Typed remake fields that need no REMAKE_SCHEMA feature: an authored monster `onHitStatus` / `projectileColor` goes into
 * `derived` (which the core already reads) and is never written at top level (monsters.ts `takeAuthored`).
 */
export const DERIVED_ONLY: Readonly<Partial<Record<RemakeKind, readonly string[]>>> = {
  monster: ['onHitStatus', 'projectileColor'],
};

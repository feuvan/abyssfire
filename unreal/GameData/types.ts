/**
 * Remake content types: the web game's data types (src/data/types.ts), re-exported, with the remake-only fields of
 * implementation-plan.md §2.6 added where a GameData module needs them.
 *
 * GameData modules import their types from here (`../types` / `./types`), never from `@web/data/types` directly, so an
 * extension below reaches every module at once. A name declared here shadows the web name re-exported by `export type *`.
 *
 * Remake-only fields are optional. The exporter withholds each one from `unreal/Data` until the core package that parses
 * it has landed (`REMAKE_SCHEMA` in unreal/Tools/export-data/src/main.ts, field registry in .../src/remake.ts), so a
 * field may be authored before the core reads it. Only the Wave 1 fields are typed here; the Wave 2 / Wave 3 packages
 * (K2-K7) add theirs together with their `REMAKE_SCHEMA` feature.
 */
import type * as Web from '@web/data/types';
import type { StatusEffectType } from '@web/systems/StatusEffectSystem';

export type * from '@web/data/types';

/** Damage type of a monster's attacks (the hero skill damage types). */
export type RemakeDamageType = Web.SkillDefinition['damageType'];

/**
 * An authored monster -> hero on-hit status (replaces the web's spriteKey / id keyword rule for this monster; same shape
 * as `monsters.json` `derived.onHitStatus`). `value`: `monsterDamage` = max(min, floor(scaled damage × fraction)).
 */
export interface RemakeOnHitStatus {
  status: StatusEffectType;
  chance: number;
  value: { kind: 'monsterDamage'; fraction: number; min: number } | { kind: 'fixed'; amount: number };
  durationMs: number;
}

export interface MonsterDefinition extends Web.MonsterDefinition {
  /** [W1] Family id (monsters.md §1.1): voice set, marker colour, family rules. Feature `monster.family`. */
  family?: string;
  /** [W1] Damage type of its attacks (today every monster hit is physical). Feature `monster.damageType`. */
  damageType?: RemakeDamageType;
  /** [W1] Authored on-hit statuses; win over the keyword rule. Feature `monster.onHitStatus`. */
  onHitStatus?: RemakeOnHitStatus[];
  /** [W1] Bolt tint 0xRRGGBB for a ranged monster; wins over the keyword rule. Feature `monster.projectileColor`. */
  projectileColor?: number;
}

/** One defend wave: exactly these monsters (K1 `SpawnDefendWave`). */
export type RemakeDefendWave = { monsterId: string; count: number }[];

export interface QuestReward extends Web.QuestReward {
  /** [W1] i18n keys labelling `choices` slot by slot (same length). Feature `quest.choiceLabels`. */
  choiceLabels?: string[];
}

export type RemakeDefendTarget = NonNullable<Web.QuestDefinition['defendTarget']> & {
  /** [W1] Composition per wave (length = totalWaves). Feature `quest.defendWaves`. */
  waves?: RemakeDefendWave[];
  /** [W1] Spawn arc centre in degrees (waves spawn on the 180° arc around it). Feature `quest.defendApproach`. */
  approachDeg?: number;
};

export interface QuestDefinition extends Omit<Web.QuestDefinition, 'rewards' | 'defendTarget'> {
  rewards: QuestReward;
  defendTarget?: RemakeDefendTarget;
  /** [W1] i18n key of the giver's line while the quest is active (`data.quest.<id>.progress`). Feature `quest.progressKey`. */
  progressKey?: string;
}

export interface SubDungeonEntrance extends Web.SubDungeonEntrance {
  /** [W1] Inert until its sub-dungeon exists: interacting logs `sealedTextKey` (K1). Feature `map.sealedEntrances`. */
  sealed?: boolean;
  /** i18n key of the sealed toast. Feature `map.sealedEntrances`. */
  sealedTextKey?: string;
}

export interface MapData extends Omit<Web.MapData, 'subDungeonEntrances'> {
  subDungeonEntrances?: SubDungeonEntrance[];
  /**
   * [W1] Presentation theme (art, terrain style, camp kit, mood); the generator `theme` stays the web's so the golden
   * maps do not move. Feature `map.presentationTheme`.
   */
  presentationTheme?: string;
}

/**
 * Remake content types: the web game's data types (src/data/types.ts), re-exported, with the remake-only fields of
 * implementation-plan.md §2.6 added where a GameData module needs them.
 *
 * GameData modules import their types from here (`../types` / `./types`), never from `@web/data/types` directly, so an
 * extension below reaches every module at once. A name declared here shadows the web name re-exported by `export type *`.
 *
 * Remake-only fields are optional. The exporter withholds each one from `unreal/Data` until the core package that parses
 * it has landed (`REMAKE_SCHEMA` in unreal/Tools/export-data/src/main.ts, field registry in .../src/remake.ts), so a
 * field may be authored before the core reads it. The Wave 1 fields, K2's (monster abilities, hit cap) and K3's (on-death
 * effects, corpses, the Unburied rise, map corpse seeds, elite affix on-death / rise / families) are typed here; the
 * later Wave 2 / Wave 3 packages (K4-K7) add theirs together with their `REMAKE_SCHEMA` feature.
 */
import type * as Web from '@web/data/types';
import type { StatusEffectType } from '@web/systems/StatusEffectSystem';
import type { EliteAffixDefinition, EliteAffixType } from '@web/systems/EliteAffixSystem';

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

/** [W2] Ground shape of an ability or hazard (tiles, degrees; MonsterData.h AbilityShapeDef). */
export interface RemakeAbilityShape {
  /** Defaults to the ability's kind for circle / cone / line / ring; required for a hazard ability and `hazard.shape`. */
  kind?: 'circle' | 'cone' | 'line' | 'ring';
  /** Circle radius, ring outer radius, cone reach. */
  radius?: number;
  /** Cone opening (total degrees, 0 < arcDeg <= 360). */
  arcDeg?: number;
  /** Line: a length x width rectangle from the origin along the direction. */
  length?: number;
  width?: number;
  /** Ring inner radius. */
  innerRadius?: number;
}

/** [W2] A status an ability (or its hazard) rolls on a landed hit: `value` (> 0) = slow / freeze percent or DoT per tick. */
export interface RemakeAbilityStatus {
  type: StatusEffectType;
  /** 0 < chance <= 1 (default 1, no roll). */
  chance?: number;
  ms: number;
  value: number;
}

/**
 * [W2] One monster ability (implementation-plan.md §3 K2; MonsterData.h MonsterAbilityDef has the rules). Feature
 * `monster.abilities`; the kinds whose runtime lands later are value-gated (raise / summon: `monster.abilityRaise` /
 * `monster.abilitySummon`, K3; buff / call: `monster.abilityBuff` / `monster.abilityCall`, K5).
 */
export interface RemakeMonsterAbility {
  id: string;
  kind: 'circle' | 'cone' | 'line' | 'ring' | 'bolt' | 'raise' | 'summon' | 'hazard' | 'buff' | 'call';
  /** Manifest clip it plays (Attack02, Cast01, Lunge, ...); its Contact (else Release) notify is the contact. */
  clip: string;
  /** Contact from windup start (>= 600, a boss >= 700); omitted = the clip's beat. */
  windupMs?: number;
  cooldownMs: number;
  /** From the monster's first swing of a fight (default 0: ready at once); a raise / summon caster's first fight step. */
  initialCooldownMs?: number;
  /** Hero distance bounds, tiles. Swing-picked kinds act only within the swing reach (attackRange x 1.2); raise / summon
   * are casts between swings (Chase or Attack) and are not bound by it. */
  rangeMin?: number;
  rangeMax?: number;
  /** Usable while life < hpBelowPct percent. */
  hpBelowPct?: number;
  /** Boss phases (K4) it is used in; omitted = every phase. */
  phases?: number[];
  /** Higher first (default 0). */
  priority?: number;
  /** circle / ring / hazard: heroAtWindup | self | anchor; cone / line: not corpse; bolt: hero; raise: corpse; buff / call: self. */
  aim: 'hero' | 'heroAtWindup' | 'self' | 'corpse' | 'anchor';
  shape?: RemakeAbilityShape;
  /** x the monster's damage roll (default 1). */
  dmgMul?: number;
  /** Default: the monster's damageType. */
  damageType?: RemakeDamageType;
  status?: RemakeAbilityStatus;
  hitsMonsters?: boolean;
  monsterDmgMul?: number;
  hazard?: { durationMs: number; tickMs: number; tickDmgMul: number; status?: RemakeAbilityStatus; shape?: RemakeAbilityShape };
  summon?: {
    monsterId: string; count?: number; at?: 'self' | 'anchors' | 'corpse'; anchors?: { col: number; row: number }[];
    capAlive?: number; perWindowCap?: number; windowMs?: number; hpPct?: number;
    /** [K3] A raise's corpse search radius from the caster (default 6); a summon at `self`: its ring (default 1.5). */
    radius?: number;
  };
  buff?: { stat: 'damage' | 'attackSpeed' | 'moveSpeed' | 'fearImmune'; mul?: number; ms: number; radius?: number; family?: string };
}

/**
 * [W2] An on-death effect (implementation-plan.md §3 K3; MonsterData.h MonsterOnDeathDef has the rules). Feature
 * `monster.onDeath` (on a monster); the elite affix field of the same shape needs no feature (elite_affixes.json, K3).
 * burst: a circle hit after a fuse (`delayMs` >= 800, `capHeroMaxHpPct` required: monsters.md §1.3); ring: a circle or
 * ring hit (damage or a status); puddle: a ground hazard ticking the damage; spawn: `spawn.count` bodies (Raised, no
 * raiser). Damage: `dmg` (x the difficulty's monster damage) or `hpPct` of the dying monster's life.
 */
export interface RemakeOnDeath {
  kind: 'burst' | 'ring' | 'puddle' | 'spawn';
  delayMs?: number;
  /** burst: a circle; ring / puddle: a circle or a ring (`kind` names it; ring defaults to ring, puddle to circle). */
  shape?: RemakeAbilityShape;
  dmg?: number;
  hpPct?: number;
  damageType?: RemakeDamageType;
  status?: RemakeAbilityStatus;
  hitsMonsters?: boolean;
  /** Monsters inside take floor(damage x monsterDmgMul) (default 1). */
  monsterDmgMul?: number;
  /** No hit on the hero above this percent of its max life (x the difficulty's damage multiplier). */
  capHeroMaxHpPct?: number;
  puddle?: { durationMs: number; tickMs: number };
  spawn?: { monsterId: string; count?: number };
}

/** [W2] What a killed monster's body leaves (monsters.md §1.5, K3). Feature `monster.corpse`. */
export interface RemakeCorpse {
  /** corpse: raised and eaten (Unburied, Living); remains: neither (a carcass, a boss's body); none: no entry. */
  leaves: 'corpse' | 'remains' | 'none';
  /** It eats corpses (the Gravegnawer). */
  consumes?: boolean;
}

/** [W2] The Unburied rise (monsters.md §2.2 trait / §3 affix, K3); needs `corpse.leaves: 'corpse'`. Feature `monster.riseAgain`. */
export interface RemakeRiseAgain {
  /** 0 < chance <= 1 (default 1). */
  chance?: number;
  delayMs?: number;
  /** Stands up at this percent of its life (default 100). */
  hpPct?: number;
  /** A critical killing blow stops it too (a fire killing blow always does). */
  notOnCrit?: boolean;
}

/**
 * An elite affix definition (EliteAffixSystem) as the remake's `eliteAffixes.definitions` override writes it (K3):
 * the new type `unburied` (monsters.md §3), and the optional `onDeath` (Scorched's burst, Gravecold's ring), `riseAgain`
 * and `families` (the monster families that may roll it; empty / omitted = every monster).
 */
export interface RemakeEliteAffixDefinition extends Omit<EliteAffixDefinition, 'type'> {
  type: EliteAffixType | 'unburied';
  onDeath?: RemakeOnDeath;
  riseAgain?: RemakeRiseAgain;
  families?: string[];
}

export interface MonsterDefinition extends Web.MonsterDefinition {
  /** [W1] Family id (monsters.md §1.1): voice set, marker colour, family rules. Feature `monster.family`. */
  family?: string;
  /** [W1] Damage type of its attacks (today every monster hit is physical). Feature `monster.damageType` (K2 reads it). */
  damageType?: RemakeDamageType;
  /** [W1] Authored on-hit statuses; win over the keyword rule. Feature `monster.onHitStatus`. */
  onHitStatus?: RemakeOnHitStatus[];
  /** [W1] Bolt tint 0xRRGGBB for a ranged monster; wins over the keyword rule. Feature `monster.projectileColor`. */
  projectileColor?: number;
  /** [W2] Abilities and telegraphs (K2). Feature `monster.abilities`. */
  abilities?: RemakeMonsterAbility[];
  /**
   * [W2] No single hit above this percent of the hero's max life on Normal (scaled by the difficulty's damage multiplier
   * on Nightmare / Hell; monsters.md §1.3: 35). Feature `monster.hitCap`.
   */
  hitCapHeroMaxHpPct?: number;
  /** [W2] What happens where it dies (K3). Feature `monster.onDeath`. */
  onDeath?: RemakeOnDeath;
  /** [W2] Its body (K3). Feature `monster.corpse`. */
  corpse?: RemakeCorpse;
  /** [W2] The Unburied rise (K3). Feature `monster.riseAgain`. */
  riseAgain?: RemakeRiseAgain;
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

/**
 * [W1] A mood region of a map (chapter1.md §9.1 / §15: the Breach Rim, a 16-tile region with its own mood). Not a
 * generator input. Feature `map.regions`; the core / UE packages that read it (K1, U1) may extend the shape together with
 * that feature.
 */
export interface RemakeRegion {
  /** Region id (unique per map), e.g. `breach_rim`. */
  id: string;
  /** Centre tile. */
  col: number;
  row: number;
  /** Radius in tiles. */
  radius: number;
  /** `zone_moods.json` mood id that blends in inside the region (a `zoneMoods.moods` entry, GameData/world.ts). */
  mood: string;
}

export interface MapData extends Omit<Web.MapData, 'subDungeonEntrances'> {
  subDungeonEntrances?: SubDungeonEntrance[];
  /**
   * [W1] Presentation theme (art, terrain style, camp kit, mood); the generator `theme` stays the web's so the golden
   * maps do not move. Feature `map.presentationTheme`.
   */
  presentationTheme?: string;
  /** [W1] Mood regions (the Breach Rim). Feature `map.regions`. */
  regions?: RemakeRegion[];
  /**
   * [W2] Bodies lying in the zone at entry (K3; the Sheepfold's six, chapter1.md §10): a corpse of `monsterId` (its
   * `corpse.leaves` must leave one) at the tile; it never fades. Feature `map.corpseSeeds`.
   */
  corpseSeeds?: { col: number; row: number; monsterId: string }[];
}

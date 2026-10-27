/**
 * Remake content types: the web game's data types (src/data/types.ts), re-exported, with the remake-only fields of
 * implementation-plan.md §2.6 added where a GameData module needs them.
 *
 * GameData modules import their types from here (`../types` / `./types`), never from `@web/data/types` directly, so an
 * extension below reaches every module at once. A name declared here shadows the web name re-exported by `export type *`.
 *
 * Remake-only fields are optional. The exporter withholds each one from `unreal/Data` until the core package that parses
 * it has landed (`REMAKE_SCHEMA` in unreal/Tools/export-data/src/main.ts, field registry in .../src/remake.ts), so a
 * field may be authored before the core reads it. The Wave 1 fields, K2's (monster abilities, hit cap), K3's (on-death
 * effects, corpses, the Unburied rise, map corpse seeds, elite affix on-death / rise / families), K4's (monster phases)
 * K5's (pack aggro, kite, block, morale, auras, spawn mixes and leaders; buff / call abilities) and K6's (the Ashnumb
 * status, quest `setsFlags` and permanent rewards, spawn variants, NPC `lines` / `visibleWhen`) are typed here; K7 adds
 * its own together with its `REMAKE_SCHEMA` feature. (K6's status row and ambush champion packs are override-table
 * fields: GameData/world.ts.)
 */
import type * as Web from '@web/data/types';
import type { StatusEffectType } from '@web/systems/StatusEffectSystem';
import type { EliteAffixDefinition, EliteAffixType } from '@web/systems/EliteAffixSystem';

export type * from '@web/data/types';

/** Damage type of a monster's attacks (the hero skill damage types). */
export type RemakeDamageType = Web.SkillDefinition['damageType'];

/**
 * A status a monster can put on the hero: the web's six, and [W2] K6 `ashnumb` (麻灰, monsters.md §4: -10 % healing
 * received per stack, max 5, slowed 20 % at 5; it lands only once GameData/world.ts `statusEffects` adds its row; feature
 * `status.ashnumb` gates the value on monsters).
 */
export type RemakeStatusType = StatusEffectType | 'ashnumb';

/**
 * An authored monster -> hero on-hit status (replaces the web's spriteKey / id keyword rule for this monster; same shape
 * as `monsters.json` `derived.onHitStatus`). `value`: `monsterDamage` = max(min, floor(scaled damage × fraction)).
 */
export interface RemakeOnHitStatus {
  status: RemakeStatusType;
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
  type: RemakeStatusType;
  /** 0 < chance <= 1 (default 1, no roll). */
  chance?: number;
  ms: number;
  value: number;
}

/**
 * [W2] One monster ability (implementation-plan.md §3 K2; MonsterData.h MonsterAbilityDef has the rules). Feature
 * `monster.abilities`; the kinds whose runtime landed later are value-gated by their own features (raise / summon:
 * `monster.abilityRaise` / `monster.abilitySummon`, K3; buff / call: `monster.abilityBuff` / `monster.abilityCall`, K5).
 */
export interface RemakeMonsterAbility {
  id: string;
  kind: 'circle' | 'cone' | 'line' | 'ring' | 'bolt' | 'raise' | 'summon' | 'hazard' | 'buff' | 'call';
  /**
   * Manifest clip it plays (Attack02, Cast01, Lunge, ...); its Contact (else Release) notify is the contact. [2F review] A
   * hazard or a call may name none: the caster plays nothing and is not rooted while it winds up (then `windupMs` is
   * required): the ground decal / the call's ripple is the tell.
   */
  clip?: string;
  /** Contact from windup start (>= 600, a boss >= 700); omitted = the clip's beat. */
  windupMs?: number;
  cooldownMs: number;
  /** From the monster's first swing of a fight (default 0: ready at once); a raise / summon caster's first fight step. */
  initialCooldownMs?: number;
  /** Hero distance bounds, tiles. Swing-picked kinds act within the swing reach (attackRange x 1.2); raise / summon /
   * buff / call are casts between swings (Chase or Attack) and are not bound by it, and [2F review] neither is a ranged
   * special: a damaging ability whose rangeMax passes the swing reach also starts between swings (Breach-seep, Lunge). */
  rangeMin?: number;
  rangeMax?: number;
  /** Usable while life < hpBelowPct percent. */
  hpBelowPct?: number;
  /** Phases (K4: the rows of the monster's `phases` entered, 0 = none yet) it is used in; omitted = every phase. */
  phases?: number[];
  /** Higher first (default 0). */
  priority?: number;
  /** circle / ring / hazard: heroAtWindup | self | anchor; cone / line: not corpse; bolt: hero; raise: corpse; buff / call: self. */
  aim: 'hero' | 'heroAtWindup' | 'self' | 'corpse' | 'anchor';
  shape?: RemakeAbilityShape;
  /** x the monster's damage roll (default 1). [2F review] 0 on a circle / cone / line / ring: a status-only strike (no
   * damage roll; `status` required, no `hitsMonsters`). */
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
  /**
   * buff: `stat` x `mul` (0 < mul <= 10) for `ms` on the monsters within `radius` (0..30; 0 = the caster) of `family`
   * (it ends early when the caster dies).
   * call [K5]: provokes every other monster within `radius` (> 0) of `family` (the social call), and buffs them (and the
   * caster) only when it names a `stat` with its `ms`.
   */
  buff?: RemakeBuff;
  /**
   * [2F review] A charge (line abilities, not aim `anchor`): at contact, after the hit, the caster stands at the farthest
   * point of the line it reaches in a straight walkable line inside its leash and tether, outside the camps.
   */
  dash?: boolean;
}

/** [W2] A buff / call ability's block (K5; MonsterData.h AbilityBuffDef). `stat` and `ms` are required for a buff. */
export interface RemakeBuff {
  stat?: RemakeBuffStat;
  mul?: number;
  ms?: number;
  radius?: number;
  family?: string;
}

/** [W2] What a buff or an aura raises (K5): damage (outgoing), attackSpeed (swings per second), moveSpeed, fearImmune. */
export type RemakeBuffStat = 'damage' | 'attackSpeed' | 'moveSpeed' | 'fearImmune';

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
 * [W2] One row of a monster's phase table (implementation-plan.md §3 K4; MonsterData.h MonsterPhaseDef has the rules).
 * Feature `monster.phases`. Rows strictly decrease in `hpBelowPct`; entering row i makes the monster's phase i + 1 (0 = as
 * defined), once, in order. A row's multipliers and tether hold while it is the current phase (x the base, not x the
 * previous row); its arena stays until a later row lights another; abilitiesOn / abilitiesOff switch in sequence (an
 * ability first switched by an abilitiesOn starts off). Every `centre` defaults to the monster's spawn anchor.
 */
export interface RemakeMonsterPhase {
  /** Entered when life < hpBelowPct percent (0 < hpBelowPct <= 100). */
  hpBelowPct: number;
  /** An AnimAction played on entry (roar, taunt, cast02, ...; not idle / walk / death): rooted, still takes damage. */
  enterAction?: 'attack' | 'cast' | 'hurt' | 'dodge' | 'signature' | 'work' | 'talk' | 'roar' | 'taunt' | 'stun' | 'attack02'
    | 'cast02' | 'rise';
  /** Multipliers in (0, 10], default 1: move speed, swings per second, damage taken. */
  speedMul?: number;
  attackSpeedMul?: number;
  damageTakenMul?: number;
  /** The tether: chase / patrol goals held within `radius` (<= 30) of the centre (no M1 reset). */
  leash?: { centre?: { col: number; row: number }; radius: number };
  abilitiesOn?: string[];
  abilitiesOff?: string[];
  /** Raised bodies (raiser = the monster) stood up `delayMs` after the entry; `respawns` x again `respawnMs` after a kill. */
  summons?: {
    monsterId: string; count?: number; at?: 'self' | 'anchors'; anchors?: { col: number; row: number }[]; radius?: number;
    hpPct?: number; delayMs?: number; respawnMs?: number; respawns?: number;
  }[];
  /** The soft enrage: a ring from fromR to toR over overMs; the hero outside takes dpsPctMaxHp % of max life per second. */
  arena?: {
    kind: 'ringShrink'; centre?: { col: number; row: number }; fromR: number; toR: number; overMs: number; dpsPctMaxHp: number;
    damageType?: RemakeDamageType;
  };
  /** A Raised killed within `radius` of the centre is drawn in after `delayMs` (fire can burn it first): heals healPct %. */
  feed?: { centre?: { col: number; row: number }; radius: number; healPct: number; delayMs?: number };
  /** Music stinger id UE plays on entry. */
  stinger?: string;
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

/**
 * [W2] Kite (K5; MonsterData.h MonsterKiteDef): a ranged monster with the hero closer than `min` steps away to about `max`
 * tiles from it (inside its leash, in a straight walkable line, never into a camp), then strikes before it kites again.
 * 0 < min < max <= 30, max within its attackRange (the kite ends in Attack only with the hero within attackRange: a band
 * edge past it walks the monster back in before it shoots). Feature `monster.kite`.
 */
export interface RemakeKite {
  min: number;
  max: number;
}

/**
 * [W2] Block (K5; MonsterData.h MonsterBlockDef): a hit from within `arcDeg` (0..360, total) around its heading is
 * multiplied by `damageMul` (0 <= damageMul < 1) and sparks (EvBlock); `wall` (0..30 tiles): fighting blockers that close
 * share one heading. Feature `monster.block`.
 */
export interface RemakeBlock {
  arcDeg: number;
  damageMul: number;
  wall?: number;
}

/**
 * [W2] Morale (K5; MonsterData.h MonsterMoraleDef): flees for `fleeOnLeaderDeathMs` (<= 60000) when its pack's leader
 * dies, if within `fleeRadius` (omitted: the whole pack) of the body, unless fearImmune or a living sustain member
 * (`sustainFamily`, any when omitted; a buff / call granting fearImmune) stands within `sustainRadius`. Feature
 * `monster.morale`.
 */
export interface RemakeMorale {
  fleeOnLeaderDeathMs: number;
  fleeRadius?: number;
  sustainFamily?: string;
  sustainRadius?: number;
}

/** [W2] Aura (K5; MonsterData.h MonsterAuraDef): monsters within `radius` (it included; of `family`) have `stat` x `mul`. */
export interface RemakeAura {
  stat: RemakeBuffStat;
  mul?: number;
  radius: number;
  family?: string;
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
  /** [W2] Boss phases and arenas (K4). Feature `monster.phases`. */
  phases?: RemakeMonsterPhase[];
  /**
   * [W2] Pack aggro (K5): tiles (0 < packAggro <= 30) within which another member of its pack being provoked or entering
   * Chase provokes this monster (once; it does not pass the alarm on). Feature `monster.packAggro`.
   */
  packAggro?: number;
  /** [W2] Kite band (K5). Feature `monster.kite`. */
  kite?: RemakeKite;
  /** [W2] Frontal block and shield wall (K5). Feature `monster.block`. */
  block?: RemakeBlock;
  /** [W2] Morale: flee when the pack's leader falls (K5). Feature `monster.morale`. */
  morale?: RemakeMorale;
  /** [W2] Aura (K5). Feature `monster.aura`. */
  aura?: RemakeAura;
}

/** One defend wave: exactly these monsters (K1 `SpawnDefendWave`). */
export type RemakeDefendWave = { monsterId: string; count: number }[];

/**
 * [W2] A quest's permanent reward (K6; QuestData.h QuestPermanentRewardDef): granted once per (quest, difficulty) at the
 * turn-in and saved (`permanentRewards`). maxLife 0..1000 is a flat max life on the hero's stat merge; statPoints /
 * skillPoints 0..100 go to the free points; respecTokens 0..100 wait for the respec UI. At least one above 0.
 */
export interface RemakePermanentReward {
  maxLife?: number;
  statPoints?: number;
  skillPoints?: number;
  respecTokens?: number;
}

export interface QuestReward extends Web.QuestReward {
  /** [W1] i18n keys labelling `choices` slot by slot (same length). Feature `quest.choiceLabels`. */
  choiceLabels?: string[];
  /** [W2] Permanent reward (K6). Feature `quest.permanentRewards`. */
  permanent?: RemakePermanentReward;
}

export type RemakeDefendTarget = NonNullable<Web.QuestDefinition['defendTarget']> & {
  /** [W1] Composition per wave (length = totalWaves). Feature `quest.defendWaves`. */
  waves?: RemakeDefendWave[];
  /** [W1] Spawn arc centre in degrees (waves spawn on the 180° arc around it). Feature `quest.defendApproach`. */
  approachDeg?: number;
};

/**
 * A quest's hunt (the web QuestHunt: makeHuntDefinition over its base monster) with [W2] remake extras (package 2F):
 * `abilities` follow the base's on the hunt's own definition (exported as quest_hunts.json `hunts[].abilities`, which the
 * core's MakeHuntDefinition appends, and merged into `defsNormal`; checked like a monster's), so a unique hunt can do what
 * its base cannot (monsters.md §2.4: Hungerbell's bell calls the field). Feature `quest.huntAbilities`.
 */
export type RemakeQuestHunt = Web.QuestHunt & { abilities?: RemakeMonsterAbility[] };

export interface QuestDefinition extends Omit<Web.QuestDefinition, 'rewards' | 'defendTarget' | 'hunts'> {
  rewards: QuestReward;
  defendTarget?: RemakeDefendTarget;
  hunts?: RemakeQuestHunt[];
  /** [W1] i18n key of the giver's line while the quest is active (`data.quest.<id>.progress`). Feature `quest.progressKey`. */
  progressKey?: string;
  /**
   * [W2] World flags its turn-in sets (K6; chapter1.md §8 `rf_hungerbell_silenced`, `rf_chapter_dawn`): saved, never
   * cleared; NPC conditions and spawn variants read them, UE moods / audio follow them. Non-empty ids, each once.
   * Feature `quest.setsFlags`.
   */
  setsFlags?: string[];
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

/**
 * A map spawn entry (a pack, monsters-ai.md §6.1) with its [W2] K5 fields (feature `map.spawnMix`): `mix` spawns these
 * monsters in list order instead of `count` x `monsterId` (the counts sum to `count`; the entry's `monsterId` stays and
 * must be one of the mix's members: the story-boss spawn and the quest guide's spawn points read it); `leader` names the
 * member that leads the pack (the first of its id), optionally elite (an elite leader rolls its affixes on the Ai stream
 * right after its placement, as an elite definition does, so the zone's later placements move). Map spawns only: a
 * sub-dungeon's spawns have no pack until K7. Not generator inputs (DECISIONS DR4: Wave 2 is golden-neutral).
 */
export type MapSpawn = Web.MapData['spawns'][number] & {
  mix?: { monsterId: string; count: number }[];
  leader?: { monsterId: string; elite?: boolean };
  /**
   * [W2] K6 spawn variants (feature `map.spawnVariants`; chapter1.md §8: after `rf_chapter_dawn` the trench and Dead
   * Ranks spawn Gravegnawers): the first whose world flag is set replaces the entry's mix / monsterId with `count`
   * (0..100; 0 = nothing) x `monsterId`, one pack without a leader, at the zone's spawn and at every respawn. One variant
   * per flag; never the story boss, nor on its entry.
   */
  variants?: RemakeSpawnVariant[];
};

/** [W2] One K6 spawn variant (MapData.h MapSpawnVariantDef). */
export interface RemakeSpawnVariant {
  whenFlag: string;
  monsterId: string;
  count: number;
}

export interface MapData extends Omit<Web.MapData, 'subDungeonEntrances' | 'spawns'> {
  spawns: MapSpawn[];
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

/**
 * [W2] K6 condition leaves (WorldStateData.h): a world flag set / not set, or a quest in a state (notStarted: never
 * accepted; active; completed: done, not turned in; open: active or completed; turnedIn; notTurnedIn: anything else).
 */
export type RemakeQuestGate = 'notStarted' | 'active' | 'completed' | 'open' | 'turnedIn' | 'notTurnedIn';
export type RemakeConditionLeaf = { flag: string } | { notFlag: string } | { quest: string; state: RemakeQuestGate };

/**
 * [W2] A K6 condition (npcs.json `lines[].when`, `visibleWhen`): every key holds together (`flag`, `notFlag`, `quest` +
 * `state`), and one leaf of `any` when present (each leaf names exactly one of flag / notFlag / quest; no nesting). At
 * least one key.
 */
export interface RemakeCondition {
  flag?: string;
  notFlag?: string;
  quest?: string;
  state?: RemakeQuestGate;
  any?: RemakeConditionLeaf[];
}

/** [W2] One K6 NPC line: an i18n key said while `when` holds (always without it). */
export interface RemakeNpcLine {
  key: string;
  when?: RemakeCondition;
}

export interface NPCDefinition extends Web.NPCDefinition {
  /**
   * [W2] K6 quest-gated lines (chapter1.md §3; feature `npc.lines`): when one holds they replace `dialogue` as what the NPC
   * says: the first holding line is logged on interaction, the linear panel shows the second (else the first). None
   * holding: the web's `dialogue` lines. Keys exist in zh-CN and en.
   */
  lines?: RemakeNpcLine[];
  /**
   * [W2] K6: the NPC stands in its camp / field spot only while this holds (feature `npc.visibleWhen`). The export (and
   * DataStore) refuse a condition that could hide it while an open quest needs it (its giver, a talk target, a craft
   * NPC); hiding it only before a quest is offered warns.
   */
  visibleWhen?: RemakeCondition;
}

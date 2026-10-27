/**
 * monsters.json, minibosses.json, quest_hunts.json, monster_ai.json
 *
 * Every MonsterDefinition is dumped verbatim (TS field names, attackSpeed in ms) with a `derived`
 * block (DECISIONS C6 / monsters-ai §1.1): isRanged, onHitStatus (from the spriteKey/id keyword rules
 * of ZoneScene.applyMonsterStatusEffect — probed on the real method) and projectileColor (the
 * keyword rule of ZoneScene.resolveMonsterStrike). DECISIONS M5 is applied as an explicit, recorded
 * override.
 */
import { MonstersByZone, getMonsterDef } from '../../../../../src/data/monsters';
import { AllDungeonMonsters, DungeonExclusiveMonsters, DungeonBossDef, DungeonMidBossDef, DungeonMonsterPool } from '../../../../../src/data/dungeonData';
import { MiniBossByZone, MiniBossDialogues, MiniBossSpawns } from '../../../../../src/data/miniBosses';
import { SubDungeonMiniBosses } from '../../../../../src/data/subDungeons';
import { AllQuests } from '../../../../../src/data/quests/all_quests';
import { makeHuntDefinition } from '../../../../../src/systems/QuestHunts';
import { ZoneScene } from '../../../../../src/scenes/ZoneScene';
import * as StoryDirectorMod from '../../../../../src/systems/StoryDirector';
import { STORY_TRIGGERS, BOSS_INTROS } from '../../../../../src/data/story/script';
import { EmeraldPlainsMap } from '../../../../../src/data/maps';
import type { MonsterDefinition } from '../../../../../src/data/types';
import type { RemakeOnHitStatus, RemakeMonsterAbility, RemakeAbilityShape, RemakeAbilityStatus, RemakeOnDeath, RemakeCorpse,
  RemakeRiseAgain, RemakeMonsterPhase, RemakeKite, RemakeBlock, RemakeMorale, RemakeAura,
  RemakeQuestHunt } from '../../../../GameData/types';
import { CHAPTER_CONFIG } from '../../../../GameData/config';
import { assert, assertSource, plain, withRandom, type TableResult } from '../util';
import { gateRemakeValues, noteConfig, remakeFeatureOn, remakeStatusTypes, withholdRemakeFields } from '../remake';

const ZS = 'src/scenes/ZoneScene.ts';
const exp = <T>(mod: unknown, name: string): T => {
  const v = (mod as Record<string, unknown>)[`__expose_${name}`];
  assert(v !== undefined, `exposed binding ${name} missing`);
  return v as T;
};

type Source = 'zone' | 'dungeon' | 'miniBoss' | 'subDungeonBoss';

/**
 * DECISIONS M7: the Chapter 1 story boss (a regular zone spawn in the web, respawning every 15 s — monsters QUIRK Q8)
 * spawns once per zone visit (role `storyBoss`, never respawns within a visit) and not after its quest is turned in,
 * except as a farmable boss once the chapter is complete (once per visit). "Chapter complete" = the quest whose
 * turn-in plays the chapter finale cutscene. Explicit data so the C++ never hardcodes the boss or the chapter rule.
 * The ids come from unreal/GameData/config.ts (the remake's chapter config); every one is checked below.
 */
const FINALE_TRIGGER = STORY_TRIGGERS.find(t => t.on === 'quest_turned_in' && t.cutscene === CHAPTER_CONFIG.chapterFinaleCutscene);
const STORY_BOSS = {
  monsterId: CHAPTER_CONFIG.storyBoss.monsterId,
  oncePerVisit: true,
  notAfterQuestTurnIn: CHAPTER_CONFIG.storyBoss.notAfterQuestTurnIn,
  farmableAfterChapter: true,
  chapterCompleteQuest: FINALE_TRIGGER && 'questId' in FINALE_TRIGGER ? FINALE_TRIGGER.questId : '',
  chapterFinaleCutscene: CHAPTER_CONFIG.chapterFinaleCutscene,
  decision: 'M7',
};
assert(STORY_BOSS.chapterCompleteQuest === CHAPTER_CONFIG.chapterCompleteQuest,
  `M7 / GameData/config.ts: the quest_turned_in trigger of ${STORY_BOSS.chapterFinaleCutscene} is ` +
  `'${STORY_BOSS.chapterCompleteQuest}', not chapterCompleteQuest '${CHAPTER_CONFIG.chapterCompleteQuest}'`);
assert(EmeraldPlainsMap.spawns.some(sp => sp.monsterId === STORY_BOSS.monsterId), 'M7: the story boss is not an emerald_plains spawn');
assert(BOSS_INTROS.some(b => b.monsterId === STORY_BOSS.monsterId), 'M7: the story boss has no boss intro');
assert(AllQuests.some(q => q.id === STORY_BOSS.notAfterQuestTurnIn && q.objectives.some(o => o.type === 'kill' && o.targetId === STORY_BOSS.monsterId)),
  'M7: notAfterQuestTurnIn must be the quest that kills the story boss');

/**
 * DECISIONS overrides applied to the exported definitions (web value kept in `overrides`). Empty since the dark remake's
 * Chapter 1 roster (unreal/GameData): M5 made the web's goblin shaman (attackRange 2.5, melee) a ranged caster at 3.0
 * here; the remake mini-boss that holds its slot, miniboss_corpseherd, authors range 3.0 (implementation-plan.md §2.3).
 */
const OVERRIDES: { id: string; field: keyof MonsterDefinition; port: number; decision: string; note: string }[] = [];

/**
 * Monster → hero on-hit statuses. The keyword rules (ZoneScene.ts:6204-6237) are evaluated here into an
 * explicit per-monster list, then checked against the real method.
 */
function onHitStatusRule(def: MonsterDefinition): Record<string, unknown>[] {
  const has = (...k: string[]) => k.some(w => def.spriteKey.includes(w) || def.id.includes(w));
  const out: Record<string, unknown>[] = [];
  if (has('fire', 'phoenix', 'lava')) out.push({ status: 'burn', chance: 0.3, value: { kind: 'monsterDamage', fraction: 0.2, min: 1 }, durationMs: 3000 });
  if (has('poison', 'venom', 'spider')) out.push({ status: 'poison', chance: 0.25, value: { kind: 'monsterDamage', fraction: 0.15, min: 1 }, durationMs: 4000 });
  if (has('ice', 'frost')) out.push({ status: 'slow', chance: 0.2, value: { kind: 'fixed', amount: 30 }, durationMs: 3000 });
  return out;
}

function probeOnHit(def: MonsterDefinition, damage: number, rand: number): { type: string; value: number; duration: number }[] {
  const out: { type: string; value: number; duration: number }[] = [];
  const fake = { statusEffects: { apply: (_t: string, type: string, value: number, duration: number) => { out.push({ type, value, duration }); return duration; } } };
  const monster = { id: 'probe', definition: { ...def, damage } };
  const proto = ZoneScene.prototype as unknown as Record<string, (...a: unknown[]) => void>;
  withRandom(new Array(8).fill(rand), () => proto.applyMonsterStatusEffect.call(fake, monster, 0));
  return out;
}

function checkOnHit(def: MonsterDefinition, rules: Record<string, unknown>[]): void {
  const all = probeOnHit(def, 1000, 0);
  assert(all.length === rules.length, `${def.id}: onHitStatus count ${rules.length} != web ${all.length}`);
  rules.forEach((r, i) => {
    assert(all[i].type === r.status && all[i].duration === r.durationMs, `${def.id}: onHit ${r.status}`);
    const v = r.value as { kind: string; fraction?: number; min?: number; amount?: number };
    if (v.kind === 'monsterDamage') {
      assert(all[i].value === Math.max(v.min!, Math.floor(1000 * v.fraction!)), `${def.id}: onHit value`);
      assert(probeOnHit(def, 0, 0)[i].value === v.min, `${def.id}: onHit min`);
    } else assert(all[i].value === v.amount, `${def.id}: onHit fixed value`);
    let lo = 0, hi = 1;
    for (let k = 0; k < 60; k++) {
      const mid = (lo + hi) / 2;
      if (probeOnHit(def, 1000, mid).some(e => e.type === r.status)) lo = mid; else hi = mid;
    }
    assert(Math.abs(hi - (r.chance as number)) < 1e-12, `${def.id}: onHit chance ${r.chance} != web ${hi}`);
  });
}

/** Projectile tint of a ranged monster (ZoneScene.ts:2951-2953). */
function projectileColor(def: MonsterDefinition): number {
  const k = def.spriteKey;
  return k.includes('fire') || k.includes('phoenix') ? 0xff6600 : k.includes('ice') ? 0x4488ff : 0xcc44cc;
}

/**
 * Remake monsters may author `onHitStatus` / `projectileColor` (GameData/types.ts); an authored value wins over the
 * keyword rule (implementation-plan.md §2.3) and goes into `derived`, which the core already reads, so it needs no
 * REMAKE_SCHEMA feature. The authored top-level field is consumed here (never written).
 */
interface Authored { onHitStatus?: RemakeOnHitStatus[]; projectileColor?: number }
function takeAuthored(d: MonsterDefinition): Authored {
  const r = d as MonsterDefinition & Authored;
  const out: Authored = { onHitStatus: r.onHitStatus, projectileColor: r.projectileColor };
  delete r.onHitStatus;
  delete r.projectileColor;
  return out;
}

// ── K2 abilities (implementation-plan.md §3 K2) ──

const ABILITY_KEYS = ['id', 'kind', 'clip', 'windupMs', 'cooldownMs', 'initialCooldownMs', 'rangeMin', 'rangeMax', 'hpBelowPct',
  'phases', 'priority', 'aim', 'shape', 'dmgMul', 'damageType', 'status', 'hitsMonsters', 'monsterDmgMul', 'hazard', 'summon', 'buff',
  'dash'];
const SHAPE_KINDS = ['circle', 'cone', 'line', 'ring'];
const STATUS_TYPES = ['burn', 'freeze', 'poison', 'bleed', 'slow', 'stun'];
const DAMAGE_TYPES = ['physical', 'fire', 'ice', 'lightning', 'poison', 'arcane'];
const AIMS_BY_KIND: Record<RemakeMonsterAbility['kind'], readonly RemakeMonsterAbility['aim'][]> = {
  circle: ['heroAtWindup', 'self', 'anchor'], ring: ['heroAtWindup', 'self', 'anchor'], hazard: ['heroAtWindup', 'self', 'anchor'],
  cone: ['hero', 'heroAtWindup', 'self', 'anchor'], line: ['hero', 'heroAtWindup', 'self', 'anchor'], bolt: ['hero'],
  raise: ['corpse'], summon: ['hero', 'heroAtWindup', 'self', 'anchor'], buff: ['self'], call: ['self'],
};
const isNum = (v: unknown): v is number => typeof v === 'number' && Number.isFinite(v);
// Monster.ts `distToPlayer > attackRange * 1.2` (asserted below; monster_ai.json `attackExitMul`): the Attack state holds
// within this multiple of attackRange, and a due swing picks its abilities in that state (2F review: a ranged special,
// rangeMax past this reach, also starts between swings: MonsterAbilities.h AbilityIsRangedSpecial).
const ATTACK_EXIT_MUL = 1.2;
// Upper bounds of the timing fields (LoadMonsters.cpp): a windup <= 5 s, a hazard <= 60 s, a cooldown <= 10 min.
const MAX_WINDUP_MS = 5000, MAX_HAZARD_MS = 60000, MAX_COOLDOWN_MS = 600000;

function checkKeys(o: object, known: readonly string[], where: string): void {
  for (const k of Object.keys(o)) assert(known.includes(k), `${where}: unknown field '${k}' (known: ${known.join(', ')})`);
}

function checkShape(sh: RemakeAbilityShape, implied: string | null, where: string): void {
  checkKeys(sh, ['kind', 'radius', 'arcDeg', 'length', 'width', 'innerRadius'], where);
  const kind = sh.kind ?? implied;
  assert(kind !== null && SHAPE_KINDS.includes(kind), `${where}.kind: a hazard shape names its kind (circle | cone | line | ring)`);
  assert(implied === null || sh.kind === undefined || sh.kind === implied, `${where}.kind: differs from the ability's kind '${implied}'`);
  const r = sh.radius ?? 0, arc = sh.arcDeg ?? 0, len = sh.length ?? 0, w = sh.width ?? 0, inner = sh.innerRadius ?? 0;
  if (kind === 'circle') assert(r > 0, `${where}: a circle needs radius > 0`);
  if (kind === 'ring') assert(inner >= 0 && r > inner, `${where}: a ring needs radius > innerRadius >= 0`);
  if (kind === 'cone') assert(r > 0 && arc > 0 && arc <= 360, `${where}: a cone needs radius > 0, 0 < arcDeg <= 360`);
  if (kind === 'line') assert(len > 0 && w > 0, `${where}: a line needs length > 0 and width > 0`);
}

/**
 * A status a monster may put on the hero: the web's six, and (K6) a type GameData/world.ts `statusEffects` adds a row for
 * (`ashnumb`; the value itself is gated by REMAKE_VALUES `status.ashnumb`): without its row it would never land.
 */
function checkStatusType(type: string, where: string): void {
  if (STATUS_TYPES.includes(type)) return;
  assert(type === 'ashnumb', `${where}: unknown status '${type}'`);
  assert(remakeStatusTypes().includes(type), `${where}: status '${type}' has no status_effects.json row ` +
    '(GameData/world.ts REMAKE_OVERRIDES.statusEffects.add): it would never land');
}

function checkStatus(st: RemakeAbilityStatus, where: string): void {
  checkKeys(st, ['type', 'chance', 'ms', 'value'], where);
  checkStatusType(st.type, `${where}.type`);
  assert(st.chance === undefined || (st.chance > 0 && st.chance <= 1), `${where}.chance: expected 0 < chance <= 1`);
  assert(isNum(st.ms) && st.ms > 0, `${where}.ms: expected > 0`);
  assert(isNum(st.value) && st.value > 0, `${where}.value: expected > 0 (a status of value 0 never applies)`);
}

/**
 * The K2 rules the C++ loader (LoadMonsters.cpp ReadMonsterAbility) and DataStore::Finalize enforce, checked here first
 * so a content error fails the export: known keys at every level (a misspelt optional field would otherwise be dropped
 * silently), the windup floor (600 ms; 700 ms for a mini-boss / sub-dungeon boss / the story boss, monsters.md §1.3),
 * shapes and aims per kind, hazard / summon / buff blocks only where they apply, unique ids, `bossSkills` naming an
 * ability once the monster has any, the hit cap a percent. Summoned monster ids are returned for the caller to resolve.
 */
function checkK2Monster(def: MonsterDefinition, where: string, boss: boolean): string[] {
  const d = def as MonsterDefinition & { abilities?: RemakeMonsterAbility[]; hitCapHeroMaxHpPct?: number; damageType?: string };
  const summoned: string[] = [];
  if (d.damageType !== undefined) assert(DAMAGE_TYPES.includes(d.damageType), `${where}.damageType: unknown '${d.damageType}'`);
  if (d.hitCapHeroMaxHpPct !== undefined) {
    assert(isNum(d.hitCapHeroMaxHpPct) && d.hitCapHeroMaxHpPct > 0 && d.hitCapHeroMaxHpPct <= 100,
      `${where}.hitCapHeroMaxHpPct: expected a percent in (0, 100]`);
  }
  const abilities = d.abilities ?? [];
  const ids = new Set<string>();
  abilities.forEach((a, i) => {
    const at = `${where}.abilities[${i}]`;
    checkKeys(a, ABILITY_KEYS, at);
    assert(typeof a.id === 'string' && a.id !== '' && !ids.has(a.id), `${at}.id: empty or duplicate ability id '${a.id}'`);
    ids.add(a.id);
    assert(Object.hasOwn(AIMS_BY_KIND, a.kind), `${at}.kind: unknown kind '${a.kind}'`);
    // 2F review: a hazard or a call may play no clip (the caster is not rooted; its windup is authored then).
    if (a.clip === undefined) {
      assert(a.kind === 'hazard' || a.kind === 'call', `${at}: a ${a.kind} ability names its clip (only a hazard or a call may play none)`);
      assert(a.windupMs !== undefined, `${at}: an ability without a clip authors its windupMs (no clip times it)`);
    } else {
      assert(typeof a.clip === 'string' && a.clip !== '', `${at}.clip: empty clip`);
    }
    if (a.windupMs !== undefined) {
      assert(isNum(a.windupMs) && a.windupMs >= (boss ? 700 : 600), `${at}.windupMs: a ${boss ? 'boss ' : ''}special winds up >= ${boss ? 700 : 600} ms`);
      assert(a.windupMs <= MAX_WINDUP_MS, `${at}.windupMs: expected <= ${MAX_WINDUP_MS}`);
    }
    assert(isNum(a.cooldownMs) && a.cooldownMs >= 0 && a.cooldownMs <= MAX_COOLDOWN_MS, `${at}.cooldownMs: expected 0..${MAX_COOLDOWN_MS}`);
    const initialCd = a.initialCooldownMs ?? 0;
    assert(isNum(initialCd) && initialCd >= 0 && initialCd <= MAX_COOLDOWN_MS, `${at}.initialCooldownMs: expected 0..${MAX_COOLDOWN_MS}`);
    assert((a.rangeMin ?? 0) >= 0, `${at}.rangeMin: expected >= 0`);
    assert(a.rangeMax === undefined || a.rangeMax > (a.rangeMin ?? 0), `${at}.rangeMax: expected > rangeMin`);
    // A due swing (Attack state, within attackRange x 1.2) picks it; DataStore::Finalize warns on the same rule. The
    // product MonsterAI tests (1.5 x 1.2 = 1.7999999999999998) with a hair of slack, so a range authored as the reach (1.8)
    // stays bound by it. K3 raise / summon and K5 buff / call are casts between swings, from Chase as well, and so (2F
    // review) is a ranged special: a damaging ability whose rangeMax passes the reach.
    const swingReach = def.attackRange * ATTACK_EXIT_MUL;
    const shownReach = Math.round(swingReach * 1000) / 1000;
    const castBetweenSwings = a.kind === 'raise' || a.kind === 'summon' || a.kind === 'buff' || a.kind === 'call' ||
      (a.rangeMax !== undefined && a.rangeMax > swingReach + 1e-9);
    assert(castBetweenSwings || (a.rangeMin ?? 0) <= swingReach + 1e-9,
      `${at}.rangeMin: ${a.rangeMin} is past the swing reach (attackRange ${def.attackRange} x ${ATTACK_EXIT_MUL} = ${shownReach}): never picked (a rangeMax past the reach makes a ranged special)`);
    assert(a.hpBelowPct === undefined || (a.hpBelowPct > 0 && a.hpBelowPct <= 100), `${at}.hpBelowPct: expected (0, 100]`);
    assert((a.phases ?? []).every(p => Number.isInteger(p) && p >= 0), `${at}.phases: phase indices are integers >= 0`);
    assert(a.priority === undefined || Number.isInteger(a.priority), `${at}.priority: an integer`);
    assert(AIMS_BY_KIND[a.kind].includes(a.aim), `${at}.aim: '${a.aim}' does not fit a ${a.kind} ability (${AIMS_BY_KIND[a.kind].join(' | ')})`);
    assert((a.dmgMul ?? 1) >= 0 && (a.monsterDmgMul ?? 1) >= 0, `${at}: dmgMul / monsterDmgMul >= 0`);
    if (a.damageType !== undefined) assert(DAMAGE_TYPES.includes(a.damageType), `${at}.damageType: unknown '${a.damageType}'`);
    if (a.status) checkStatus(a.status, `${at}.status`);
    const implied = SHAPE_KINDS.includes(a.kind) ? a.kind : null;
    const ground = implied !== null || a.kind === 'hazard';
    if (ground) {
      assert(a.shape !== undefined, `${at}: a ${a.kind} ability needs a shape`);
      checkShape(a.shape!, implied, `${at}.shape`);
    } else {
      assert(a.shape === undefined, `${at}.shape: only circle / cone / line / ring / hazard abilities have a shape`);
    }
    if (a.hazard) {
      const h = a.hazard;
      checkKeys(h, ['durationMs', 'tickMs', 'tickDmgMul', 'status', 'shape'], `${at}.hazard`);
      assert(ground, `${at}.hazard: only ground abilities leave a hazard`);
      assert(isNum(h.durationMs) && h.durationMs > 0 && h.durationMs <= MAX_HAZARD_MS, `${at}.hazard.durationMs: expected 0 < durationMs <= ${MAX_HAZARD_MS}`);
      assert(isNum(h.tickMs) && h.tickMs >= 100 && h.tickMs <= h.durationMs, `${at}.hazard.tickMs: expected 100 <= tickMs <= durationMs`);
      assert(isNum(h.tickDmgMul) && h.tickDmgMul >= 0, `${at}.hazard.tickDmgMul: expected >= 0`);
      if (h.status) checkStatus(h.status, `${at}.hazard.status`);
      if (h.shape) {
        assert(a.kind !== 'hazard', `${at}.hazard.shape: a hazard ability's shape is the ability's own 'shape'`);
        checkShape(h.shape, null, `${at}.hazard.shape`);
      }
    } else {
      assert(a.kind !== 'hazard', `${at}: a hazard ability needs its 'hazard' block`);
    }
    assert(a.kind !== 'hazard' || (a.dmgMul === undefined && a.status === undefined),
      `${at}: a hazard ability hits through its hazard: use hazard.tickDmgMul / hazard.status`);
    // 2F review: dmgMul 0 makes a strike status-only (the Corpseherd's Keen): it needs its status and hurts no monster.
    if (implied !== null && a.dmgMul !== undefined && !(a.dmgMul > 0)) {
      assert(a.status !== undefined, `${at}.dmgMul: a status-only strike (dmgMul 0) needs its 'status'`);
      assert(!a.hitsMonsters, `${at}.hitsMonsters: a status-only strike (dmgMul 0) hits no monster`);
    }
    // 2F review: a dash (a charge) runs along a line from the caster.
    if (a.dash !== undefined) {
      assert(typeof a.dash === 'boolean', `${at}.dash: a boolean`);
      assert(!a.dash || a.kind === 'line', `${at}.dash: only a line ability dashes`);
      assert(!a.dash || a.aim !== 'anchor', `${at}.dash: a dash starts at the caster: not aim 'anchor'`);
    }
    const summons = a.kind === 'raise' || a.kind === 'summon';
    if (a.summon) {
      const m = a.summon;
      checkKeys(m, ['monsterId', 'count', 'at', 'anchors', 'capAlive', 'perWindowCap', 'windowMs', 'hpPct', 'radius'], `${at}.summon`);
      assert(summons, `${at}.summon: only raise / summon abilities summon`);
      assert(typeof m.monsterId === 'string' && m.monsterId !== '', `${at}.summon.monsterId: empty`);
      summoned.push(m.monsterId);
      assert(m.count === undefined || (Number.isInteger(m.count) && m.count >= 1), `${at}.summon.count: an integer >= 1`);
      const where2 = m.at ?? (a.kind === 'raise' ? 'corpse' : 'self');
      assert(['self', 'anchors', 'corpse'].includes(where2), `${at}.summon.at: unknown '${where2}'`);
      assert(a.kind !== 'raise' || where2 === 'corpse', `${at}.summon.at: a raise stands a corpse up`);
      assert(where2 !== 'anchors' || (m.anchors ?? []).length > 0, `${at}.summon.anchors: at 'anchors' needs anchors`);
      assert((m.capAlive ?? 0) >= 0 && (m.perWindowCap ?? 0) >= 0 && (m.windowMs ?? 0) >= 0, `${at}.summon: caps and window >= 0`);
      assert((m.windowMs ?? 0) <= MAX_COOLDOWN_MS, `${at}.summon.windowMs: expected <= ${MAX_COOLDOWN_MS}`);
      assert(!(m.perWindowCap ?? 0) || (m.windowMs ?? 0) > 0, `${at}.summon.windowMs: a per-window cap needs windowMs > 0`);
      assert(m.hpPct === undefined || (m.hpPct > 0 && m.hpPct <= 100), `${at}.summon.hpPct: expected (0, 100]`);
      // K3: a raise's corpse search radius / a summon's ring around the caster (LoadMonsters.cpp kLoadSummonMaxRadius).
      assert(m.radius === undefined || (isNum(m.radius) && m.radius > 0 && m.radius <= 30), `${at}.summon.radius: expected 0 < radius <= 30`);
      assert(m.radius === undefined || where2 !== 'anchors', `${at}.summon.radius: a summon at 'anchors' stands its bodies on the anchors: no radius`);
    } else {
      assert(!summons, `${at}: a ${a.kind} ability needs its 'summon' block`);
    }
    if (a.buff) {
      // LoadMonsters.cpp ReadMonsterAbility (K5): a buff names its stat and ms; a call provokes within radius > 0 and buffs
      // only with a stat (then with its ms).
      const b = a.buff;
      checkKeys(b, ['stat', 'mul', 'ms', 'radius', 'family'], `${at}.buff`);
      assert(a.kind === 'buff' || a.kind === 'call', `${at}.buff: only buff / call abilities buff`);
      const hasStat = a.kind === 'buff' || b.stat !== undefined;
      if (hasStat) {
        assert(b.stat !== undefined, `${at}.buff.stat: a buff names its stat (${BUFF_STATS.join(', ')})`);
        assert(BUFF_STATS.includes(b.stat), `${at}.buff.stat: unknown '${b.stat}'`);
        assert(isNum(b.ms) && b.ms > 0 && b.ms <= MAX_COOLDOWN_MS, `${at}.buff.ms: expected 0 < ms <= ${MAX_COOLDOWN_MS}`);
      } else {
        assert(b.ms === undefined && b.mul === undefined, `${at}.buff: a call buffs (ms, mul) only with a stat`);
      }
      assert(b.mul === undefined || (isNum(b.mul) && b.mul > 0 && b.mul <= 10), `${at}.buff.mul: expected 0 < mul <= 10`);
      const r = b.radius ?? 0;
      assert(isNum(r) && r >= 0 && r <= MAX_PACK_RADIUS, `${at}.buff.radius: expected 0 <= radius <= ${MAX_PACK_RADIUS}`);
      assert(a.kind !== 'call' || r > 0, `${at}.buff.radius: a call reaches radius > 0 tiles`);
      assert(b.family === undefined || (typeof b.family === 'string' && b.family !== ''), `${at}.buff.family: an empty family id`);
    } else {
      assert(a.kind !== 'buff', `${at}: a buff ability needs its 'buff' block`);
      assert(a.kind !== 'call', `${at}: a call ability needs its 'buff' block (its radius, family and optional buff)`);
    }
  });
  if (abilities.length > 0) {
    for (const id of def.bossSkills ?? []) assert(ids.has(id), `${where}.bossSkills: '${id}' names no ability of the monster`);
  }
  return summoned;
}

// ── K3 on-death effects, corpses and the Unburied rise (implementation-plan.md §3 K3) ──

const ON_DEATH_KEYS = ['kind', 'delayMs', 'shape', 'dmg', 'hpPct', 'damageType', 'status', 'hitsMonsters', 'monsterDmgMul',
  'capHeroMaxHpPct', 'puddle', 'spawn'];
// LoadMonsters.cpp: an exploder's fuse >= 800 ms (monsters.md §1.3); an on-death delay <= 10 s, a rise <= 60 s; an on-death
// spawn of 1..20 bodies.
const BURST_MIN_FUSE_MS = 800, MAX_ON_DEATH_DELAY_MS = 10000, MAX_RISE_DELAY_MS = 60000, MAX_ON_DEATH_SPAWN = 20;

/**
 * The rules of LoadMonsters.cpp ReadMonsterOnDeath (also elite_affixes.json `onDeath`, combat.ts): known keys at every
 * level; per kind its shape (burst: a circle; ring / puddle: a circle or a ring; spawn: none), its damage (dmg or hpPct,
 * not both; a burst needs one and a cap, a ring / puddle damage or a status; a spawn none) and its block (puddle / spawn
 * only for their kind). Returns the spawned monster id for the caller to resolve.
 */
export function checkOnDeath(d: RemakeOnDeath, where: string): string[] {
  checkKeys(d, ON_DEATH_KEYS, where);
  assert(['burst', 'ring', 'puddle', 'spawn'].includes(d.kind), `${where}.kind: unknown '${d.kind}' (burst | ring | puddle | spawn)`);
  const delay = d.delayMs ?? 0;
  assert(isNum(delay) && delay >= 0 && delay <= MAX_ON_DEATH_DELAY_MS, `${where}.delayMs: expected 0..${MAX_ON_DEATH_DELAY_MS}`);
  assert(d.kind !== 'burst' || delay >= BURST_MIN_FUSE_MS, `${where}.delayMs: an exploder's fuse lasts >= ${BURST_MIN_FUSE_MS} ms (monsters.md §1.3)`);
  if (d.kind === 'spawn') {
    assert(d.shape === undefined, `${where}.shape: a spawn has no shape`);
    for (const k of ['dmg', 'hpPct', 'damageType', 'status', 'hitsMonsters', 'monsterDmgMul', 'capHeroMaxHpPct'] as const) {
      assert(d[k] === undefined, `${where}.${k}: a spawn deals no damage`);
    }
  } else {
    assert(d.shape !== undefined, `${where}: a ${d.kind} needs a shape`);
    if (d.kind === 'burst') {
      checkShape(d.shape!, 'circle', `${where}.shape`);
    } else {
      checkShape(d.shape!, d.shape!.kind === undefined ? (d.kind === 'ring' ? 'ring' : 'circle') : null, `${where}.shape`);
      assert(d.shape!.kind === undefined || d.shape!.kind === 'circle' || d.shape!.kind === 'ring', `${where}.shape.kind: an on-death shape is a circle or a ring`);
    }
  }
  assert(d.dmg === undefined || (isNum(d.dmg) && d.dmg > 0), `${where}.dmg: expected > 0`);
  assert(d.hpPct === undefined || (isNum(d.hpPct) && d.hpPct > 0 && d.hpPct <= 100), `${where}.hpPct: expected (0, 100]`);
  assert(d.dmg === undefined || d.hpPct === undefined, `${where}: an on-death effect deals dmg or hpPct, not both`);
  if (d.damageType !== undefined) assert(DAMAGE_TYPES.includes(d.damageType), `${where}.damageType: unknown '${d.damageType}'`);
  if (d.status) checkStatus(d.status, `${where}.status`);
  assert(d.hitsMonsters === undefined || typeof d.hitsMonsters === 'boolean', `${where}.hitsMonsters: a boolean`);
  assert(d.monsterDmgMul === undefined || (isNum(d.monsterDmgMul) && d.monsterDmgMul >= 0), `${where}.monsterDmgMul: expected >= 0`);
  assert(d.capHeroMaxHpPct === undefined || (isNum(d.capHeroMaxHpPct) && d.capHeroMaxHpPct > 0 && d.capHeroMaxHpPct <= 100),
    `${where}.capHeroMaxHpPct: expected a percent in (0, 100]`);
  const damages = d.dmg !== undefined || d.hpPct !== undefined;
  if (d.kind === 'burst') {
    assert(damages, `${where}: a burst deals damage: dmg or hpPct`);
    assert(d.capHeroMaxHpPct !== undefined, `${where}: an exploder's damage is capped: capHeroMaxHpPct (monsters.md §1.3)`);
  }
  if (d.kind === 'ring' || d.kind === 'puddle') assert(damages || d.status !== undefined, `${where}: a ${d.kind} needs damage or a status`);
  if (d.puddle) {
    checkKeys(d.puddle, ['durationMs', 'tickMs'], `${where}.puddle`);
    assert(d.kind === 'puddle', `${where}.puddle: only a puddle has a 'puddle' block`);
    assert(isNum(d.puddle.durationMs) && d.puddle.durationMs > 0 && d.puddle.durationMs <= MAX_HAZARD_MS, `${where}.puddle.durationMs: expected 0 < durationMs <= ${MAX_HAZARD_MS}`);
    assert(isNum(d.puddle.tickMs) && d.puddle.tickMs >= 100 && d.puddle.tickMs <= d.puddle.durationMs, `${where}.puddle.tickMs: expected 100 <= tickMs <= durationMs`);
  } else {
    assert(d.kind !== 'puddle', `${where}: a puddle needs its 'puddle' block`);
  }
  if (d.spawn) {
    checkKeys(d.spawn, ['monsterId', 'count'], `${where}.spawn`);
    assert(d.kind === 'spawn', `${where}.spawn: only a spawn has a 'spawn' block`);
    assert(typeof d.spawn.monsterId === 'string' && d.spawn.monsterId !== '', `${where}.spawn.monsterId: empty`);
    const n = d.spawn.count ?? 1;
    assert(Number.isInteger(n) && n >= 1 && n <= MAX_ON_DEATH_SPAWN, `${where}.spawn.count: an integer 1..${MAX_ON_DEATH_SPAWN}`);
    return [d.spawn.monsterId];
  }
  assert(d.kind !== 'spawn', `${where}: a spawn needs its 'spawn' block`);
  return [];
}

/** LoadMonsters.cpp ReadMonsterRiseAgain (also elite_affixes.json `riseAgain`). */
export function checkRiseAgain(r: RemakeRiseAgain, where: string): void {
  checkKeys(r, ['chance', 'delayMs', 'hpPct', 'notOnCrit'], where);
  assert(r.chance === undefined || (isNum(r.chance) && r.chance > 0 && r.chance <= 1), `${where}.chance: expected 0 < chance <= 1`);
  assert(r.delayMs === undefined || (isNum(r.delayMs) && r.delayMs >= 0 && r.delayMs <= MAX_RISE_DELAY_MS), `${where}.delayMs: expected 0..${MAX_RISE_DELAY_MS}`);
  assert(r.hpPct === undefined || (isNum(r.hpPct) && r.hpPct > 0 && r.hpPct <= 100), `${where}.hpPct: expected (0, 100]`);
  assert(r.notOnCrit === undefined || typeof r.notOnCrit === 'boolean', `${where}.notOnCrit: a boolean`);
}

/**
 * K3 on a monster (LoadMonsters.cpp ReadMonsterDef): `family` a non-empty id, `onDeath`, `corpse {leaves, consumes}`, and
 * `riseAgain` only on a monster whose body leaves a corpse. Returns the on-death spawn ids.
 */
function checkK3Monster(def: MonsterDefinition, where: string): string[] {
  const d = def as MonsterDefinition & { family?: string; onDeath?: RemakeOnDeath; corpse?: RemakeCorpse; riseAgain?: RemakeRiseAgain };
  if (d.family !== undefined) assert(typeof d.family === 'string' && d.family !== '', `${where}.family: an empty family id`);
  const spawned = d.onDeath ? checkOnDeath(d.onDeath, `${where}.onDeath`) : [];
  if (d.corpse) {
    checkKeys(d.corpse, ['leaves', 'consumes'], `${where}.corpse`);
    assert(['corpse', 'remains', 'none'].includes(d.corpse.leaves), `${where}.corpse.leaves: unknown '${d.corpse.leaves}' (corpse | remains | none)`);
    assert(d.corpse.consumes === undefined || typeof d.corpse.consumes === 'boolean', `${where}.corpse.consumes: a boolean`);
  }
  if (d.riseAgain) {
    checkRiseAgain(d.riseAgain, `${where}.riseAgain`);
    assert(d.corpse?.leaves === 'corpse', `${where}.riseAgain: a monster rises from its own corpse: corpse.leaves must be 'corpse'`);
  }
  return spawned;
}

// ── K4 boss phases and arenas (implementation-plan.md §3 K4) ──

const PHASE_KEYS = ['hpBelowPct', 'enterAction', 'speedMul', 'attackSpeedMul', 'damageTakenMul', 'leash', 'abilitiesOn',
  'abilitiesOff', 'summons', 'arena', 'feed', 'stinger'];
// AnimAction (SimTypes.h) less idle / walk / death.
const ENTER_ACTIONS = ['attack', 'cast', 'hurt', 'dodge', 'signature', 'work', 'talk', 'roar', 'taunt', 'stun', 'attack02',
  'cast02', 'rise'];
// LoadMonsters.cpp: multipliers in (0, 10]; a tether, a feed or a summon ring within 30 tiles, an arena within 60; an arena
// closes over <= 10 min; a phase summon waits <= 60 s and stands back up <= 10 times, each <= 10 min later; a feed draws
// its corpse in <= 10 s.
const MAX_PHASE_MUL = 10, MAX_PHASE_RADIUS = 30, MAX_ARENA_RADIUS = 60, MAX_PHASE_SUMMON_DELAY_MS = 60000,
  MAX_PHASE_RESPAWNS = 10, MAX_FEED_DELAY_MS = 10000;

function checkCentre(c: { col: number; row: number } | undefined, where: string): void {
  if (c === undefined) return;
  checkKeys(c, ['col', 'row'], where);
  assert(Number.isInteger(c.col) && Number.isInteger(c.row), `${where}: {col, row} integers`);
}

/**
 * The rules of LoadMonsters.cpp ReadMonsterPhase / ReadMonsterDef (K4): known keys at every level, thresholds strictly
 * decreasing in (0, 100], an enter action that is an action (not idle / walk / death), multipliers in (0, 10], the
 * tether / arena / feed / summon bounds, switched abilities that are the monster's own (and not both on and off in a
 * row). Warns (as DataStore::Finalize does) on an ability whose phases[] lists only phases the monster never reaches.
 * Returns the summoned monster ids for the caller to resolve.
 */
function checkK4Monster(def: MonsterDefinition, where: string): string[] {
  const d = def as MonsterDefinition & { phases?: RemakeMonsterPhase[]; abilities?: RemakeMonsterAbility[] };
  const summoned: string[] = [];
  const abilityIds = new Set((d.abilities ?? []).map(a => a.id));
  const rows = d.phases ?? [];
  const mul = (v: number | undefined, at: string) =>
    assert(v === undefined || (isNum(v) && v > 0 && v <= MAX_PHASE_MUL), `${at}: expected 0 < multiplier <= ${MAX_PHASE_MUL}`);
  rows.forEach((p, i) => {
    const at = `${where}.phases[${i}]`;
    checkKeys(p, PHASE_KEYS, at);
    assert(isNum(p.hpBelowPct) && p.hpBelowPct > 0 && p.hpBelowPct <= 100, `${at}.hpBelowPct: expected (0, 100]`);
    assert(i === 0 || p.hpBelowPct < rows[i - 1].hpBelowPct, `${at}.hpBelowPct: phase thresholds strictly decrease`);
    assert(p.enterAction === undefined || ENTER_ACTIONS.includes(p.enterAction),
      `${at}.enterAction: '${p.enterAction}' is not an action (${ENTER_ACTIONS.join(' | ')})`);
    mul(p.speedMul, `${at}.speedMul`);
    mul(p.attackSpeedMul, `${at}.attackSpeedMul`);
    mul(p.damageTakenMul, `${at}.damageTakenMul`);
    if (p.leash) {
      checkKeys(p.leash, ['centre', 'radius'], `${at}.leash`);
      checkCentre(p.leash.centre, `${at}.leash.centre`);
      assert(isNum(p.leash.radius) && p.leash.radius > 0 && p.leash.radius <= MAX_PHASE_RADIUS, `${at}.leash.radius: expected 0 < radius <= ${MAX_PHASE_RADIUS}`);
    }
    for (const id of p.abilitiesOn ?? []) {
      assert(abilityIds.has(id), `${at}.abilitiesOn: '${id}' names no ability of the monster`);
      assert(!(p.abilitiesOff ?? []).includes(id), `${at}.abilitiesOff: '${id}' is switched both on and off`);
    }
    for (const id of p.abilitiesOff ?? []) assert(abilityIds.has(id), `${at}.abilitiesOff: '${id}' names no ability of the monster`);
    (p.summons ?? []).forEach((m, j) => {
      const sat = `${at}.summons[${j}]`;
      checkKeys(m, ['monsterId', 'count', 'at', 'anchors', 'radius', 'hpPct', 'delayMs', 'respawnMs', 'respawns'], sat);
      assert(typeof m.monsterId === 'string' && m.monsterId !== '', `${sat}.monsterId: empty`);
      summoned.push(m.monsterId);
      assert(m.count === undefined || (Number.isInteger(m.count) && m.count >= 1 && m.count <= MAX_ON_DEATH_SPAWN), `${sat}.count: an integer 1..${MAX_ON_DEATH_SPAWN}`);
      const where2 = m.at ?? 'self';
      assert(where2 === 'self' || where2 === 'anchors', `${sat}.at: a phase summons at 'self' or 'anchors'`);
      assert(where2 !== 'anchors' || (m.anchors ?? []).length > 0, `${sat}.anchors: at 'anchors' needs anchors`);
      assert(where2 === 'anchors' || m.anchors === undefined || m.anchors.length === 0, `${sat}.anchors: anchors need at 'anchors'`);
      (m.anchors ?? []).forEach((t, k) => checkCentre(t, `${sat}.anchors[${k}]`));
      assert(m.radius === undefined || (isNum(m.radius) && m.radius > 0 && m.radius <= MAX_PHASE_RADIUS), `${sat}.radius: expected 0 < radius <= ${MAX_PHASE_RADIUS}`);
      assert(m.radius === undefined || where2 !== 'anchors', `${sat}.radius: a summon at 'anchors' stands its bodies on the anchors: no radius`);
      assert(m.hpPct === undefined || (m.hpPct > 0 && m.hpPct <= 100), `${sat}.hpPct: expected (0, 100]`);
      assert(m.delayMs === undefined || (isNum(m.delayMs) && m.delayMs >= 0 && m.delayMs <= MAX_PHASE_SUMMON_DELAY_MS), `${sat}.delayMs: expected 0..${MAX_PHASE_SUMMON_DELAY_MS}`);
      assert(m.respawnMs === undefined || (isNum(m.respawnMs) && m.respawnMs >= 0 && m.respawnMs <= MAX_COOLDOWN_MS), `${sat}.respawnMs: expected 0..${MAX_COOLDOWN_MS}`);
      assert(m.respawns === undefined || (Number.isInteger(m.respawns) && m.respawns >= 0 && m.respawns <= MAX_PHASE_RESPAWNS), `${sat}.respawns: an integer 0..${MAX_PHASE_RESPAWNS}`);
    });
    if (p.arena) {
      const a = p.arena;
      checkKeys(a, ['kind', 'centre', 'fromR', 'toR', 'overMs', 'dpsPctMaxHp', 'damageType'], `${at}.arena`);
      assert(a.kind === 'ringShrink', `${at}.arena.kind: unknown '${a.kind}' (ringShrink)`);
      checkCentre(a.centre, `${at}.arena.centre`);
      assert(isNum(a.fromR) && a.fromR > 0 && a.fromR <= MAX_ARENA_RADIUS, `${at}.arena.fromR: expected 0 < fromR <= ${MAX_ARENA_RADIUS}`);
      assert(isNum(a.toR) && a.toR > 0 && a.toR <= a.fromR, `${at}.arena.toR: expected 0 < toR <= fromR (a ring shrinks)`);
      assert(isNum(a.overMs) && a.overMs > 0 && a.overMs <= MAX_COOLDOWN_MS, `${at}.arena.overMs: expected 0 < overMs <= ${MAX_COOLDOWN_MS}`);
      assert(isNum(a.dpsPctMaxHp) && a.dpsPctMaxHp > 0 && a.dpsPctMaxHp <= 100, `${at}.arena.dpsPctMaxHp: expected a percent in (0, 100]`);
      if (a.damageType !== undefined) assert(DAMAGE_TYPES.includes(a.damageType), `${at}.arena.damageType: unknown '${a.damageType}'`);
    }
    if (p.feed) {
      const f = p.feed;
      checkKeys(f, ['centre', 'radius', 'healPct', 'delayMs'], `${at}.feed`);
      checkCentre(f.centre, `${at}.feed.centre`);
      assert(isNum(f.radius) && f.radius > 0 && f.radius <= MAX_PHASE_RADIUS, `${at}.feed.radius: expected 0 < radius <= ${MAX_PHASE_RADIUS}`);
      assert(isNum(f.healPct) && f.healPct > 0 && f.healPct <= 100, `${at}.feed.healPct: expected (0, 100]`);
      assert(f.delayMs === undefined || (isNum(f.delayMs) && f.delayMs >= 0 && f.delayMs <= MAX_FEED_DELAY_MS), `${at}.feed.delayMs: expected 0..${MAX_FEED_DELAY_MS}`);
    }
    assert(p.stinger === undefined || (typeof p.stinger === 'string' && p.stinger !== ''), `${at}.stinger: an empty stinger id`);
  });
  for (const a of d.abilities ?? []) {
    const listed = a.phases ?? [];
    if (listed.length > 0 && listed.every(ph => ph > rows.length)) {
      console.warn(`  warning: ${where}.abilities.${a.id}.phases: never usable: the monster reaches phases 0..${rows.length}`);
    }
  }
  return summoned;
}

// ── K5 packs, morale, kite, block and auras (implementation-plan.md §3 K5) ──

const BUFF_STATS = ['damage', 'attackSpeed', 'moveSpeed', 'fearImmune'];
// LoadMonsters.cpp kLoadPackMaxRadius / kLoadFleeMaxMs: every K5 radius within 30 tiles; a flee lasts <= 60 s.
const MAX_PACK_RADIUS = 30, MAX_FLEE_MS = 60000;
// monster_ai.json `rangedThreshold` (Monster.ts `attackRange > 2.5`): a kite is a ranged monster's rule.
const RANGED_THRESHOLD = 2.5;

/**
 * The rules of LoadMonsters.cpp ReadMonsterK5 and DataStore::Finalize's K5 checks: known keys in every block, radii in
 * (0, 30], a kite band 0 < min < max on a ranged monster (attackRange > 2.5) ending within its attack range (a kite runs
 * in Chase and ends in Attack only with the hero within attackRange; past it the monster walks back in before it shoots;
 * DataStore warns, the export fails), a block arc in (0, 360] reducing hits (0 <= damageMul < 1), a flee of (0, 60] s,
 * a sustain family with its radius, an aura stat.
 */
function checkK5Monster(def: MonsterDefinition, where: string): void {
  const d = def as MonsterDefinition & { packAggro?: number; kite?: RemakeKite; block?: RemakeBlock; morale?: RemakeMorale;
    aura?: RemakeAura };
  const radius = (v: unknown, at: string) =>
    assert(isNum(v) && v > 0 && v <= MAX_PACK_RADIUS, `${at}: expected 0 < radius <= ${MAX_PACK_RADIUS}`);
  if (d.packAggro !== undefined) radius(d.packAggro, `${where}.packAggro`);
  if (d.kite) {
    const k = d.kite;
    checkKeys(k, ['min', 'max'], `${where}.kite`);
    radius(k.min, `${where}.kite.min`);
    radius(k.max, `${where}.kite.max`);
    assert(k.max > k.min, `${where}.kite.max: expected max > min (the band it keeps from the hero)`);
    assert(def.attackRange > RANGED_THRESHOLD, `${where}.kite: a kite is a ranged monster's rule (attackRange ${def.attackRange} <= ${RANGED_THRESHOLD})`);
    assert(k.max <= def.attackRange + 1e-9, `${where}.kite.max: ${k.max} is past the attack range (attackRange ` +
      `${def.attackRange}): the kite would end in Chase and the monster walk back in before it shoots`);
  }
  if (d.block) {
    const b = d.block;
    checkKeys(b, ['arcDeg', 'damageMul', 'wall'], `${where}.block`);
    assert(isNum(b.arcDeg) && b.arcDeg > 0 && b.arcDeg <= 360, `${where}.block.arcDeg: expected 0 < arcDeg <= 360`);
    assert(isNum(b.damageMul) && b.damageMul >= 0 && b.damageMul < 1, `${where}.block.damageMul: expected 0 <= damageMul < 1`);
    if (b.wall !== undefined) radius(b.wall, `${where}.block.wall`);
  }
  if (d.morale) {
    const m = d.morale;
    checkKeys(m, ['fleeOnLeaderDeathMs', 'fleeRadius', 'sustainFamily', 'sustainRadius'], `${where}.morale`);
    assert(isNum(m.fleeOnLeaderDeathMs) && m.fleeOnLeaderDeathMs > 0 && m.fleeOnLeaderDeathMs <= MAX_FLEE_MS,
      `${where}.morale.fleeOnLeaderDeathMs: expected 0 < fleeOnLeaderDeathMs <= ${MAX_FLEE_MS}`);
    if (m.fleeRadius !== undefined) radius(m.fleeRadius, `${where}.morale.fleeRadius`);
    if (m.sustainRadius !== undefined) radius(m.sustainRadius, `${where}.morale.sustainRadius`);
    assert(m.sustainFamily === undefined || (typeof m.sustainFamily === 'string' && m.sustainFamily !== ''),
      `${where}.morale.sustainFamily: an empty family id`);
    assert(m.sustainFamily === undefined || m.sustainRadius !== undefined, `${where}.morale.sustainFamily: a sustain needs its sustainRadius`);
  }
  if (d.aura) {
    const a = d.aura;
    checkKeys(a, ['stat', 'mul', 'radius', 'family'], `${where}.aura`);
    assert(BUFF_STATS.includes(a.stat), `${where}.aura.stat: unknown '${a.stat}'`);
    assert(a.mul === undefined || (isNum(a.mul) && a.mul > 0 && a.mul <= 10), `${where}.aura.mul: expected 0 < mul <= 10`);
    radius(a.radius, `${where}.aura.radius`);
    assert(a.family === undefined || (typeof a.family === 'string' && a.family !== ''), `${where}.aura.family: an empty family id`);
  }
}

// No tether / arena check against the M1 leash (the 2C review's warnTetherPastLeash): a monster whose phase table holds an
// arena or a tether is leashed to its authored home over its whole fight's reach (2F review; MonsterAI.h
// MonsterLeashRange, Phases.h PhaseFightReach), so neither can send it home from inside.

export function exportMonsters(): TableResult[] {
  assertSource(ZS,
    "const projColor = spriteKey.includes('fire') || spriteKey.includes('phoenix') ? 0xff6600 : spriteKey.includes('ice') ? 0x4488ff : 0xcc44cc;",
    'const ranged = monster.definition.attackRange > 2.5;');

  const defs: Record<string, Record<string, unknown>> = {};
  const sources: Record<string, Source[]> = {};
  const summonedIds: [string, string][] = [];  // [summoner, summoned] (K2), resolved once every definition is known
  const overridesApplied: Record<string, unknown>[] = [];
  const add = (def: MonsterDefinition, source: Source, zone: string | null) => {
    const prior = defs[def.id];
    if (prior) {
      // Same id from two places: must be the same definition (else getMonsterDef order would matter).
      assert(JSON.stringify(plain(def)) === JSON.stringify(prior.__web), `monster ${def.id} defined differently in two tables`);
      sources[def.id].push(source);
      return;
    }
    gateRemakeValues('monster', def, `monsters.${def.id}`);
    const boss = !!def.isMiniBoss || !!def.isSubDungeonMiniBoss || def.id === STORY_BOSS.monsterId;
    summonedIds.push(...checkK2Monster(def, `monsters.${def.id}`, boss).map(id => [def.id, id] as [string, string]));
    summonedIds.push(...checkK3Monster(def, `monsters.${def.id}`).map(id => [def.id, id] as [string, string]));
    summonedIds.push(...checkK4Monster(def, `monsters.${def.id}`).map(id => [def.id, id] as [string, string]));
    checkK5Monster(def, `monsters.${def.id}`);
    const d: MonsterDefinition = withholdRemakeFields('monster', plain(def), `monsters.${def.id}`);
    const authored = takeAuthored(d);
    const web = plain(def);
    for (const o of OVERRIDES.filter(x => x.id === def.id)) {
      overridesApplied.push({ id: o.id, field: o.field, web: d[o.field] as number, port: o.port, decision: o.decision, note: o.note });
      (d as unknown as Record<string, unknown>)[o.field] = o.port;
    }
    (authored.onHitStatus ?? []).forEach((r, i) => checkStatusType(r.status, `monsters.${def.id}.onHitStatus[${i}].status`));  // K6
    const onHitStatus = authored.onHitStatus ? plain(authored.onHitStatus) as unknown as Record<string, unknown>[] : onHitStatusRule(def);
    if (!authored.onHitStatus) checkOnHit(def, onHitStatus);
    sources[def.id] = [source];
    defs[def.id] = {
      __web: web,
      ...d,
      derived: {
        isRanged: d.attackRange > 2.5,
        onHitStatus,
        projectileColor: authored.projectileColor ?? projectileColor(d),
        homeZone: zone,
        nameKey: `data.monster.${d.id}`,
      },
    };
  };
  const byZone: Record<string, string[]> = {};
  for (const [zone, list] of Object.entries(MonstersByZone)) {
    byZone[zone] = list.map(m => m.id);
    for (const m of list) add(m, 'zone', zone);
  }
  for (const m of Object.values(AllDungeonMonsters)) add(m, 'dungeon', null);
  for (const [zone, m] of Object.entries(MiniBossByZone)) add(m, 'miniBoss', zone);
  for (const m of Object.values(SubDungeonMiniBosses)) add(m, 'subDungeonBoss', null);
  for (const o of OVERRIDES) assert(defs[o.id], `override for unknown monster ${o.id}`);
  for (const [by, id] of summonedIds) assert(defs[id], `monsters.${by}: an ability, on-death effect or phase summons unknown monster '${id}'`);
  for (const id of Object.keys(defs)) {
    (defs[id].derived as Record<string, unknown>).sources = sources[id];
    delete defs[id].__web;
  }
  // getMonsterDef lookup order: dungeon-exclusive table first, then zones in MonstersByZone order.
  for (const id of Object.keys(defs)) {
    const found = getMonsterDef(id);
    if (sources[id].includes('zone') || sources[id].includes('dungeon')) assert(found && found.id === id, `getMonsterDef(${id})`);
    else assert(!found, `getMonsterDef should not find ${id}`);
  }

  const webFinalBoss = exp<string>(StoryDirectorMod, 'FINAL_BOSS');
  const finalBoss = CHAPTER_CONFIG.finalBoss ?? webFinalBoss;
  assert(defs[finalBoss], `GameData/config.ts finalBoss: unknown monster '${finalBoss}'`);
  for (const key of ['storyBoss', 'chapterCompleteQuest', 'chapterFinaleCutscene'] as const) noteConfig(['monster_ai.json'], key);
  noteConfig(['monster_ai.json'], 'finalBoss', webFinalBoss);

  // ── minibosses.json ──
  const miniBossByZone = Object.fromEntries(Object.entries(MiniBossByZone).map(([z, m]) => [z, m.id]));

  // ── quest_hunts.json ──
  const hunts: Record<string, unknown>[] = [];
  const huntDefs: Record<string, unknown> = {};
  for (const q of AllQuests) for (const remakeHunt of (q.hunts ?? []) as RemakeQuestHunt[]) {
    // 2F (`quest.huntAbilities`): a hunt's own abilities join its definition after its base's (the core's
    // MakeHuntDefinition appends quest_hunts.json `hunts[].abilities` to the base's; `defsNormal` carries the result).
    // quests.json keeps the web hunt fields only.
    const { abilities: huntAbilities, ...h } = remakeHunt;
    const ownAbilities = huntAbilities !== undefined && remakeFeatureOn('quest.huntAbilities') ? huntAbilities : undefined;
    hunts.push({ questId: q.id, zone: q.zone, ...plain(h), ...(ownAbilities ? { abilities: plain(ownAbilities) } : {}) });
    const base = (MonstersByZone[q.zone] ?? []).find(m => m.id === h.monsterId) ?? getMonsterDef(h.monsterId);
    assert(base, `hunt ${h.huntId}: base monster ${h.monsterId} not found`);
    const def = makeHuntDefinition(base, h, h.name);
    if (ownAbilities !== undefined) {
      assert(Array.isArray(ownAbilities) && ownAbilities.length > 0, `quests.${q.id}.hunts.${h.huntId}.abilities: an empty list (omit it)`);
      const own = (base as MonsterDefinition & { abilities?: RemakeMonsterAbility[] }).abilities ?? [];
      for (const a of ownAbilities) {
        assert(!own.some(b => b.id === a.id), `quests.${q.id}.hunts.${h.huntId}.abilities.${a.id}: repeats an ability id of its base`);
      }
      (def as MonsterDefinition & { abilities?: RemakeMonsterAbility[] }).abilities = [...own, ...ownAbilities];
      const where = `huntDefs.${h.huntId}`;
      for (const id of checkK2Monster(def, where, true)) assert(defs[id], `${where}: an ability summons unknown monster '${id}'`);
    }
    gateRemakeValues('monster', def, `huntDefs.${h.huntId}`);
    takeAuthored(withholdRemakeFields('monster', def, `huntDefs.${h.huntId}`));
    const hpMul = h.hpMul ?? 4, dmgMul = h.dmgMul ?? 1.5;
    assert(def.hp === Math.round(base.hp * hpMul) && def.damage === Math.round(base.damage * dmgMul)
      && def.defense === Math.round(base.defense * 1.2) && def.expReward === Math.round(base.expReward * Math.max(3, hpMul))
      && def.goldReward[0] === base.goldReward[0] * 3 && def.aggroRange === Math.max(base.aggroRange, 7)
      && def.elite === true && def.isMiniBoss === true, `makeHuntDefinition rule for ${h.huntId}`);
    huntDefs[h.huntId] = plain(def);
  }
  assertSource(ZS, 'body?.setScale(body.scaleX * 1.25);', 'const c = spot.col + randomInt(-3, 3);',
    ': this.findWalkableNear(hunt.col, hunt.row, 6);', 'this.cameras.main.shake(260, 0.004);');

  // ── monster_ai.json ── (monsters-ai §17; web constants pinned to their source)
  assertSource('src/entities/Monster.ts', 'leashRange: number = 8;', 'this.hp = Math.min(this.maxHp, this.hp + this.maxHp * 0.01);',
    '} else if (this.patrolTimer > 3000) {', 'const pc = this.spawnCol + randomInt(-2, 2);', 'if (dist < 0.1) return true;',
    'if (distToPlayer > this.definition.aggroRange * 1.5) {', 'if (distToPlayer > this.definition.attackRange * 1.2) {',
    'const targetSpeed = this.definition.speed * speedMultiplier * (delta / 1000) * 0.03;', 'private readonly moveAccel = 6;',
    'str: Math.floor(definition.damage * 0.8),', 'dex: Math.floor(definition.speed * 0.1),', 'vit: Math.floor(definition.hp * 0.1),');
  assertSource(ZS, 'private static readonly MONSTER_AI_CULL_DIST_SQ = 30 * 30;', "if (this.simulationScheduler.due('active-monsters', time, 250)) {",
    'const c = Math.max(1, Math.min(this.mapData.cols - 2, spawn.col + randomInt(-3, 3)));', 'const tc = dead.spawnCol + randomInt(-2, 2);',
    'this.time.delayedCall(15000, () => this.respawnMonster(monster));', 'const reach = monster.definition.attackRange * 1.35 + 0.5;',
    'if (md < 16 && monster.isAggro() && time - ((monster as unknown as { lastEscortAttack?: number }).lastEscortAttack ?? -Infinity) > 2000) {',
    'if (md < 9 && time - ((monster as unknown as { lastDefendAttack?: number }).lastDefendAttack ?? 0) > 2000) {');
  const ai = {
    leashRange: 8, leashHealFractionPerTick: 0.01,
    patrolIntervalMs: 3000, patrolRadius: 2, arriveEpsilon: 0.1,
    chaseDropMul: 1.5, attackExitMul: ATTACK_EXIT_MUL, moveSpeedScale: 0.03, moveAccel: 6,
    rangedThreshold: 2.5, meleeReachMul: 1.35, meleeReachAdd: 0.5, swingQueryRadius: 12,
    aiCullRadius: 30, activeRefreshMs: 250,
    spawnJitter: 3, respawnDelayMs: 15000, respawnJitter: 2, safeZoneRadiusDefault: 9,
    monsterStats: { strPerDamage: 0.8, dexPerSpeed: 0.1, vitPerHp: 0.1, int: 3, spi: 3, lck: 3 },
    hunt: { hpMul: 4, dmgMul: 1.5, defMul: 1.2, expMulMin: 3, goldMul: 3, aggroMin: 7, minionJitter: 3, spotSearchRadius: 6,
      visualScale: 1.25, revealShakeMs: 260, revealShakeIntensity: 0.004 },
    boss: { sightRange: exp<number>(StoryDirectorMod, 'BOSS_SIGHT'), barRange: exp<number>(StoryDirectorMod, 'BOSS_BAR_RANGE'),
      finalBoss },
    escortChip: { radiusSq: 16, intervalMs: 2000, damageMul: 0.3 },
    defendChip: { radiusSq: 9, intervalMs: 2000, damageMul: 0.2 },
    // Port decisions (DECISIONS M1–M4, M7, M10, M11).
    port: {
      leash: { mode: 'returning', healFractionPerSecond: 0.6, decision: 'M1' },
      provoke: { durationMs: 5000, decision: 'M2' },
      respawnAnchor: 'original spawn (no drift), decision M3',
      noRespawn: ['ambush', 'rescue', 'defendWave', 'hunt', 'huntMinion', 'miniBoss', 'storyBoss', 'labyrinth'],
      miniBossOncePerVisit: true,
      storyBoss: STORY_BOSS,
      patrolTimeoutMs: 4000, placementTries: 8, decisionRobustness: 'M10',
      yawTurnRateDegPerSec: 720, decisionYaw: 'M11',
      pathfinding: 'A* on the tile grid when line-of-walk is blocked + separation + wall sliding (M6)',
    },
  };
  assertSource(ZS, 'const dmg = Math.max(1, Math.floor(monster.definition.damage * 0.3));',
    'const dmg = Math.max(1, Math.floor(monster.definition.damage * 0.2));');

  const totalDefs = Object.keys(defs).length;
  return [
    {
      file: 'monsters.json',
      source: ['src/data/monsters/*.ts', 'src/data/dungeonData.ts', 'src/data/miniBosses.ts', 'src/data/subDungeons.ts', ZS],
      data: {
        lookupOrder: ['dungeon', 'zone'],
        byZone,
        dungeon: { exclusive: Object.keys(DungeonExclusiveMonsters), boss: DungeonBossDef.id, midBoss: DungeonMidBossDef.id, pool: [...DungeonMonsterPool] },
        defs,
        overrides: overridesApplied,
      },
      counts: { defs: totalDefs, zoneMonsters: Object.values(byZone).reduce((a, l) => a + l.length, 0),
        emeraldPlains: byZone.emerald_plains?.length ?? 0, miniBosses: Object.keys(MiniBossByZone).length },
    },
    {
      file: 'minibosses.json',
      source: ['src/data/miniBosses.ts', 'src/data/subDungeons.ts'],
      data: {
        byZone: miniBossByZone,
        spawns: plain(MiniBossSpawns),
        subDungeon: Object.keys(SubDungeonMiniBosses),
        dialogues: plain(MiniBossDialogues),
        i18n: { line: 'data.miniBossDialogue.<monsterId>.<nodeId> (M8: the port reads lines from these keys)' },
      },
      counts: { zones: Object.keys(miniBossByZone).length, dialogues: Object.keys(MiniBossDialogues).length },
    },
    {
      file: 'quest_hunts.json',
      source: ['src/data/quests/all_quests.ts', 'src/systems/QuestHunts.ts', ZS],
      // defsNormal: makeHuntDefinition over each hunt's base monster.
      deps: ['src/data/monsters/*.ts', 'src/data/dungeonData.ts'],
      data: {
        rules: { hpMulDefault: 4, dmgMulDefault: 1.5, defenseMul: 1.2, expMulMin: 3, goldMul: 3, aggroMin: 7, elite: true, isMiniBoss: true,
          rounding: 'Math.round', baseLookup: 'quest zone list first, then getMonsterDef', nameKey: 'data.monster.<huntId>',
          difficultyScaling: 'scaleMonster after makeHuntDefinition', eliteAffixes: 'rolled for the zone' },
        hunts,
        defsNormal: huntDefs,
      },
      counts: { hunts: hunts.length },
    },
    {
      file: 'monster_ai.json',
      source: ['src/entities/Monster.ts', ZS, 'src/systems/StoryDirector.ts', 'src/systems/QuestHunts.ts'],
      // port.storyBoss.chapterCompleteQuest: the quest_turned_in trigger of the finale cutscene.
      deps: ['src/data/story/script.ts'],
      data: ai,
      counts: { constants: Object.keys(ai).length },
    },
  ];
}

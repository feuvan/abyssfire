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
import type { RemakeOnHitStatus, RemakeMonsterAbility, RemakeAbilityShape, RemakeAbilityStatus } from '../../../../GameData/types';
import { CHAPTER_CONFIG } from '../../../../GameData/config';
import { assert, assertSource, plain, withRandom, type TableResult } from '../util';
import { gateRemakeValues, noteConfig, withholdRemakeFields } from '../remake';

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
  'phases', 'priority', 'aim', 'shape', 'dmgMul', 'damageType', 'status', 'hitsMonsters', 'monsterDmgMul', 'hazard', 'summon', 'buff'];
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
// within this multiple of attackRange, and an ability is picked only on a due swing in that state.
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

function checkStatus(st: RemakeAbilityStatus, where: string): void {
  checkKeys(st, ['type', 'chance', 'ms', 'value'], where);
  assert(STATUS_TYPES.includes(st.type), `${where}.type: unknown status '${st.type}'`);
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
    assert(typeof a.clip === 'string' && a.clip !== '', `${at}.clip: empty clip`);
    if (a.windupMs !== undefined) {
      assert(isNum(a.windupMs) && a.windupMs >= (boss ? 700 : 600), `${at}.windupMs: a ${boss ? 'boss ' : ''}special winds up >= ${boss ? 700 : 600} ms`);
      assert(a.windupMs <= MAX_WINDUP_MS, `${at}.windupMs: expected <= ${MAX_WINDUP_MS}`);
    }
    assert(isNum(a.cooldownMs) && a.cooldownMs >= 0 && a.cooldownMs <= MAX_COOLDOWN_MS, `${at}.cooldownMs: expected 0..${MAX_COOLDOWN_MS}`);
    const initialCd = a.initialCooldownMs ?? 0;
    assert(isNum(initialCd) && initialCd >= 0 && initialCd <= MAX_COOLDOWN_MS, `${at}.initialCooldownMs: expected 0..${MAX_COOLDOWN_MS}`);
    assert((a.rangeMin ?? 0) >= 0, `${at}.rangeMin: expected >= 0`);
    assert(a.rangeMax === undefined || a.rangeMax > (a.rangeMin ?? 0), `${at}.rangeMax: expected > rangeMin`);
    // Picked only on a due swing (Attack state, within attackRange x 1.2): DataStore::Finalize warns on the same rule.
    // The product MonsterAI tests (1.5 x 1.2 = 1.7999999999999998) with a hair of slack, so a range authored as the reach
    // (1.8) passes.
    const swingReach = def.attackRange * ATTACK_EXIT_MUL;
    const shownReach = Math.round(swingReach * 1000) / 1000;
    assert((a.rangeMin ?? 0) <= swingReach + 1e-9,
      `${at}.rangeMin: ${a.rangeMin} is past the swing reach (attackRange ${def.attackRange} x ${ATTACK_EXIT_MUL} = ${shownReach}): never picked, abilities fire only on a due swing`);
    if (a.rangeMax !== undefined && a.rangeMax > swingReach + 1e-9) {
      console.warn(`  warning: ${at}.rangeMax: ${a.rangeMax} past the swing reach (${shownReach} tiles) has no effect: abilities fire only on a due swing`);
    }
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
    const summons = a.kind === 'raise' || a.kind === 'summon';
    if (a.summon) {
      const m = a.summon;
      checkKeys(m, ['monsterId', 'count', 'at', 'anchors', 'capAlive', 'perWindowCap', 'windowMs', 'hpPct'], `${at}.summon`);
      assert(summons, `${at}.summon: only raise / summon abilities summon`);
      assert(typeof m.monsterId === 'string' && m.monsterId !== '', `${at}.summon.monsterId: empty`);
      summoned.push(m.monsterId);
      assert(m.count === undefined || (Number.isInteger(m.count) && m.count >= 1), `${at}.summon.count: an integer >= 1`);
      const where2 = m.at ?? (a.kind === 'raise' ? 'corpse' : 'self');
      assert(['self', 'anchors', 'corpse'].includes(where2), `${at}.summon.at: unknown '${where2}'`);
      assert(a.kind !== 'raise' || where2 === 'corpse', `${at}.summon.at: a raise stands a corpse up`);
      assert(where2 !== 'anchors' || (m.anchors ?? []).length > 0, `${at}.summon.anchors: at 'anchors' needs anchors`);
      assert((m.capAlive ?? 0) >= 0 && (m.perWindowCap ?? 0) >= 0 && (m.windowMs ?? 0) >= 0, `${at}.summon: caps and window >= 0`);
      assert(!(m.perWindowCap ?? 0) || (m.windowMs ?? 0) > 0, `${at}.summon.windowMs: a per-window cap needs windowMs > 0`);
      assert(m.hpPct === undefined || (m.hpPct > 0 && m.hpPct <= 100), `${at}.summon.hpPct: expected (0, 100]`);
    } else {
      assert(!summons, `${at}: a ${a.kind} ability needs its 'summon' block`);
    }
    if (a.buff) {
      const b = a.buff;
      checkKeys(b, ['stat', 'mul', 'ms', 'radius', 'family'], `${at}.buff`);
      assert(a.kind === 'buff' || a.kind === 'call', `${at}.buff: only buff / call abilities buff`);
      assert(['damage', 'attackSpeed', 'moveSpeed', 'fearImmune'].includes(b.stat), `${at}.buff.stat: unknown '${b.stat}'`);
      assert((b.mul ?? 1) > 0 && isNum(b.ms) && b.ms > 0 && (b.radius ?? 0) >= 0, `${at}.buff: mul > 0, ms > 0, radius >= 0`);
    } else {
      assert(a.kind !== 'buff', `${at}: a buff ability needs its 'buff' block`);
    }
  });
  if (abilities.length > 0) {
    for (const id of def.bossSkills ?? []) assert(ids.has(id), `${where}.bossSkills: '${id}' names no ability of the monster`);
  }
  return summoned;
}

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
    const d: MonsterDefinition = withholdRemakeFields('monster', plain(def), `monsters.${def.id}`);
    const authored = takeAuthored(d);
    const web = plain(def);
    for (const o of OVERRIDES.filter(x => x.id === def.id)) {
      overridesApplied.push({ id: o.id, field: o.field, web: d[o.field] as number, port: o.port, decision: o.decision, note: o.note });
      (d as unknown as Record<string, unknown>)[o.field] = o.port;
    }
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
  for (const [by, id] of summonedIds) assert(defs[id], `monsters.${by}: an ability summons unknown monster '${id}'`);
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
  for (const q of AllQuests) for (const h of q.hunts ?? []) {
    hunts.push({ questId: q.id, zone: q.zone, ...plain(h) });
    const base = (MonstersByZone[q.zone] ?? []).find(m => m.id === h.monsterId) ?? getMonsterDef(h.monsterId);
    assert(base, `hunt ${h.huntId}: base monster ${h.monsterId} not found`);
    const def = makeHuntDefinition(base, h, h.name);
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

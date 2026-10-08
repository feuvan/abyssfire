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
import { assert, assertSource, plain, withRandom, type TableResult } from '../util';

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
 */
const FINALE_TRIGGER = STORY_TRIGGERS.find(t => t.on === 'quest_turned_in' && t.cutscene === 'cs_ep_finale');
const STORY_BOSS = {
  monsterId: 'goblin_chief',
  oncePerVisit: true,
  notAfterQuestTurnIn: 'q_find_goblin_chief',
  farmableAfterChapter: true,
  chapterCompleteQuest: FINALE_TRIGGER && 'questId' in FINALE_TRIGGER ? FINALE_TRIGGER.questId : '',
  chapterFinaleCutscene: 'cs_ep_finale',
  decision: 'M7',
};
assert(STORY_BOSS.chapterCompleteQuest === 'q_secure_plains', 'M7: the Chapter 1 finale trigger moved');
assert(EmeraldPlainsMap.spawns.some(sp => sp.monsterId === STORY_BOSS.monsterId), 'M7: the story boss is not an emerald_plains spawn');
assert(BOSS_INTROS.some(b => b.monsterId === STORY_BOSS.monsterId), 'M7: the story boss has no boss intro');
assert(AllQuests.some(q => q.id === STORY_BOSS.notAfterQuestTurnIn && q.objectives.some(o => o.type === 'kill' && o.targetId === STORY_BOSS.monsterId)),
  'M7: notAfterQuestTurnIn must be the quest that kills the story boss');

/** DECISIONS overrides applied to the exported definitions (web value kept in `overrides`). */
const OVERRIDES: { id: string; field: keyof MonsterDefinition; port: number; decision: string; note: string }[] = [
  { id: 'miniboss_goblin_shaman', field: 'attackRange', port: 3.0, decision: 'M5',
    note: 'Goblin Shaman becomes a ranged caster (spirit-fire bolt); web 2.5 was melee (ranged iff > 2.5)' },
];

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

export function exportMonsters(): TableResult[] {
  assertSource(ZS,
    "const projColor = spriteKey.includes('fire') || spriteKey.includes('phoenix') ? 0xff6600 : spriteKey.includes('ice') ? 0x4488ff : 0xcc44cc;",
    'const ranged = monster.definition.attackRange > 2.5;');

  const defs: Record<string, Record<string, unknown>> = {};
  const sources: Record<string, Source[]> = {};
  const overridesApplied: Record<string, unknown>[] = [];
  const add = (def: MonsterDefinition, source: Source, zone: string | null) => {
    const prior = defs[def.id];
    if (prior) {
      // Same id from two places: must be the same definition (else getMonsterDef order would matter).
      assert(JSON.stringify(plain(def)) === JSON.stringify(prior.__web), `monster ${def.id} defined differently in two tables`);
      sources[def.id].push(source);
      return;
    }
    const d: MonsterDefinition = plain(def);
    const web = plain(def);
    for (const o of OVERRIDES.filter(x => x.id === def.id)) {
      overridesApplied.push({ id: o.id, field: o.field, web: d[o.field] as number, port: o.port, decision: o.decision, note: o.note });
      (d as unknown as Record<string, unknown>)[o.field] = o.port;
    }
    const onHitStatus = onHitStatusRule(def);
    checkOnHit(def, onHitStatus);
    sources[def.id] = [source];
    defs[def.id] = {
      __web: web,
      ...d,
      derived: {
        isRanged: d.attackRange > 2.5,
        onHitStatus,
        projectileColor: projectileColor(d),
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
    chaseDropMul: 1.5, attackExitMul: 1.2, moveSpeedScale: 0.03, moveAccel: 6,
    rangedThreshold: 2.5, meleeReachMul: 1.35, meleeReachAdd: 0.5, swingQueryRadius: 12,
    aiCullRadius: 30, activeRefreshMs: 250,
    spawnJitter: 3, respawnDelayMs: 15000, respawnJitter: 2, safeZoneRadiusDefault: 9,
    monsterStats: { strPerDamage: 0.8, dexPerSpeed: 0.1, vitPerHp: 0.1, int: 3, spi: 3, lck: 3 },
    hunt: { hpMul: 4, dmgMul: 1.5, defMul: 1.2, expMulMin: 3, goldMul: 3, aggroMin: 7, minionJitter: 3, spotSearchRadius: 6,
      visualScale: 1.25, revealShakeMs: 260, revealShakeIntensity: 0.004 },
    boss: { sightRange: exp<number>(StoryDirectorMod, 'BOSS_SIGHT'), barRange: exp<number>(StoryDirectorMod, 'BOSS_BAR_RANGE'),
      finalBoss: exp<string>(StoryDirectorMod, 'FINAL_BOSS') },
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
      data: ai,
      counts: { constants: Object.keys(ai).length },
    },
  ];
}

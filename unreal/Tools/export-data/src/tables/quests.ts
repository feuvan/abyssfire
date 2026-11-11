/**
 * quests.json and quest_tuning.json.
 *
 * AllQuests is dumped verbatim in array order (all chapters; the order is the quest log / iteration order).
 * Each quest gets `derived` with the giver NPC (QuestWorld.questGiverOf: first NPC in NPCDefinitions key
 * order that lists it) and the item kind of each collect objective (QuestWorld.questItemKindOf).
 */
import { AllQuests } from '../../../../../src/data/quests/all_quests';
import { questGiverOf, questItemKindOf } from '../../../../../src/systems/QuestWorld';
import * as QuestWorldMod from '../../../../../src/systems/QuestWorld';
import { isCollectObjective, FALLBACK_COLLECT_CHANCE } from '../../../../../src/systems/QuestRewards';
import { NPCDefinitions } from '../../../../../src/data/npcs';
import * as Homestead from '../../../../../src/data/homestead';
import { assert, assertSource, plain, type TableResult } from '../util';
import { gateRemakeValues, withholdRemakeFields } from '../remake';
import { subDungeons, isHandAuthored } from '../subdungeons';
import type { RemakeQuestHunt, QuestObjective as RemakeObjective } from '../../../../GameData/types';

const ZS = 'src/scenes/ZoneScene.ts';
const exp = <T>(mod: unknown, name: string): T => {
  const v = (mod as Record<string, unknown>)[`__expose_${name}`];
  assert(v !== undefined, `exposed binding ${name} missing`);
  return v as T;
};

/**
 * K1 hook shapes the C++ loader rejects (LoadQuests.cpp), checked here first so a content error fails the export:
 * one `choiceLabels` key per `choices` slot, one `defendTarget.waves` composition per wave with counts >= 1.
 */
function checkK1Hooks(q: unknown): void {
  const r = q as { id: string; progressKey?: string; rewards: { choices?: unknown[]; choiceLabels?: string[] };
    defendTarget?: { totalWaves: number; waves?: { monsterId: string; count: number }[][] } };
  const where = `quests.${r.id}`;
  if (r.progressKey !== undefined) assert(r.progressKey !== '', `${where}.progressKey: empty key`);
  const labels = r.rewards.choiceLabels;
  if (labels) {
    assert(labels.length === (r.rewards.choices?.length ?? 0),
      `${where}.rewards.choiceLabels: ${labels.length} label(s) for ${r.rewards.choices?.length ?? 0} choice slot(s)`);
    assert(labels.every(k => k !== ''), `${where}.rewards.choiceLabels: empty key`);
  }
  const waves = r.defendTarget?.waves;
  if (waves) {
    assert(waves.length === r.defendTarget!.totalWaves,
      `${where}.defendTarget.waves: ${waves.length} wave(s) for totalWaves ${r.defendTarget!.totalWaves}`);
    waves.forEach((w, i) => {
      assert(w.length > 0, `${where}.defendTarget.waves[${i}]: an empty wave`);
      for (const e of w) assert(Number.isInteger(e.count) && e.count >= 1 && e.monsterId !== '', `${where}.defendTarget.waves[${i}]: bad entry ${JSON.stringify(e)}`);
    });
  }
}

/** Permanent reward bounds (QuestData.h kPermanentMaxLifeMax / kPermanentPointsMax). */
const PERMANENT_MAX = { maxLife: 1000, statPoints: 100, skillPoints: 100, respecTokens: 100 } as const;

/**
 * K6 shapes the C++ loader rejects (LoadQuests.cpp), checked here first: `setsFlags` a non-empty list of non-empty ids,
 * each once; `rewards.permanent` known keys, integers within their bounds, at least one above 0.
 */
function checkK6Quest(q: unknown): void {
  const r = q as { id: string; setsFlags?: unknown; rewards: { permanent?: Record<string, unknown> } };
  const where = `quests.${r.id}`;
  if (r.setsFlags !== undefined) {
    assert(Array.isArray(r.setsFlags) && r.setsFlags.length > 0, `${where}.setsFlags: an empty or malformed list (omit it)`);
    const seen = new Set<string>();
    for (const f of r.setsFlags as unknown[]) {
      assert(typeof f === 'string' && f !== '', `${where}.setsFlags: empty or malformed flag ${JSON.stringify(f)}`);
      assert(!seen.has(f as string), `${where}.setsFlags: duplicate flag '${f}'`);
      seen.add(f as string);
    }
  }
  const p = r.rewards.permanent;
  if (p !== undefined) {
    let any = false;
    for (const [k, v] of Object.entries(p)) {
      assert(Object.hasOwn(PERMANENT_MAX, k), `${where}.rewards.permanent: unknown field '${k}' (known: ${Object.keys(PERMANENT_MAX).join(', ')})`);
      const max = PERMANENT_MAX[k as keyof typeof PERMANENT_MAX];
      assert(Number.isInteger(v) && (v as number) >= 0 && (v as number) <= max, `${where}.rewards.permanent.${k}: expected an integer 0..${max}`);
      any = any || (v as number) > 0;
    }
    assert(any, `${where}.rewards.permanent: grants nothing (omit it)`);
  }
}

/** clear_area bound (QuestData.h kClearAreaMaxRemaining). */
const CLEAR_AREA_MAX_REMAINING = 1000;

/**
 * K7 shapes the C++ loader and DataStore::Finalize reject (LoadQuests.cpp ReadObjective; QuestData.h QuestObjectiveDef),
 * checked here first: an objective's `zone` is a hand-authored sub-dungeon of the quest's zone (never on an escort /
 * defend_wave objective); a `clear_area` names its sub-dungeon as `targetId` (`zone` omitted or the same), has
 * `required` 1, `maxRemaining` an integer 0..1000, `after` an earlier objective that is not a clear_area, and a
 * sub-dungeon with a `cleansedFlag`; `maxRemaining` / `after` sit on clear_area objectives only. `cleansers` collects
 * the sub-dungeons already cleansed by a quest: one clear_area per sub-dungeon.
 */
function checkK7Quest(q: unknown, cleansers: Map<string, string>): void {
  const r = q as { id: string; zone: string; objectives: RemakeObjective[] };
  const all = subDungeons();
  r.objectives.forEach((o, i) => {
    const where = `quests.${r.id}.objectives[${i}]`;
    const clear = o.type === 'clear_area';
    for (const key of ['maxRemaining', 'after'] as const) {
      assert(clear || o[key] === undefined, `${where}.${key}: only a clear_area objective has it`);
    }
    if (clear) {
      assert(typeof o.targetId === 'string' && o.targetId !== '', `${where}.targetId: a clear_area names the sub-dungeon to cleanse`);
      assert(o.zone === undefined || o.zone === o.targetId, `${where}.zone: a clear_area's zone is its targetId ('${o.targetId}')`);
      assert(o.required === 1, `${where}.required: a clear_area objective has required 1`);
      assert(o.maxRemaining === undefined || (Number.isInteger(o.maxRemaining) && o.maxRemaining >= 0 && o.maxRemaining <= CLEAR_AREA_MAX_REMAINING),
        `${where}.maxRemaining: expected an integer 0..${CLEAR_AREA_MAX_REMAINING}`);
      if (o.after !== undefined) {
        assert(Number.isInteger(o.after) && o.after >= 0 && o.after < i, `${where}.after: objective ${o.after} is not an earlier objective`);
        assert(r.objectives[o.after].type !== 'clear_area', `${where}.after: a clear_area cannot wait for another clear_area`);
      }
    }
    const zone = clear ? o.targetId : o.zone;
    // 3B review: `spot` is the explore spot of an objective in a sub-dungeon (GameData/types.ts QuestObjective; written
    // as `location` below). The web generator never sees it, so it is no landmark of the parent's grid.
    const spot = (o as { spot?: unknown }).spot as { col: number; row: number; radius: number } | undefined;
    if (spot !== undefined) {
      assert(o.type === 'explore', `${where}.spot: only an explore objective has a spot`);
      assert(o.zone !== undefined, `${where}.spot: only an objective with a sub-dungeon \`zone\` has a spot (use location)`);
      assert((o as { location?: unknown }).location === undefined, `${where}.spot: the objective also has a location`);
      for (const key of Object.keys(spot)) assert(['col', 'row', 'radius'].includes(key), `${where}.spot: unknown field '${key}'`);
      assert(Number.isInteger(spot.col) && Number.isInteger(spot.row) && typeof spot.radius === 'number' && spot.radius > 0,
        `${where}.spot: expected {col, row, radius > 0}`);
      const at = o.zone !== undefined ? all[o.zone] : undefined;
      assert(!at || !isHandAuthored(at) || (at.collisions ?? []).length === 0 || at.collisions![spot.row]?.[spot.col] === true,
        `${where}.spot: (${spot.col},${spot.row}) is not a walkable tile of '${o.zone}'`);
    }
    if (zone === undefined) return;
    assert(typeof zone === 'string' && zone !== '', `${where}.zone: an empty zone id (omit it for the quest's zone)`);
    assert(zone !== r.zone, `${where}.zone: the quest's own zone (omit it)`);
    const sd = all[zone];
    assert(sd, `${where}.zone: unknown sub-dungeon '${zone}'`);
    assert(isHandAuthored(sd), `${where}.zone: '${zone}' is a generated sub-dungeon (nothing enters it)`);
    assert(sd.parentZone === r.zone, `${where}.zone: '${zone}' is a sub-dungeon of '${sd.parentZone}', not of the quest's zone '${r.zone}'`);
    assert(o.type !== 'escort' && o.type !== 'defend_wave', `${where}.zone: an escort / defend_wave objective is done in the quest's zone`);
    if (!clear) return;
    assert(typeof sd.cleansedFlag === 'string' && sd.cleansedFlag !== '', `${where}: '${zone}' has no cleansedFlag (it cannot be cleansed)`);
    assert(!cleansers.has(zone), `${where}: a second clear_area of '${zone}' (quest '${cleansers.get(zone)}' cleanses it)`);
    cleansers.set(zone, r.id);
  });
}

/**
 * K7 soft-lock guards (DataStore::Finalize has the same rules). Once cleansed a sub-dungeon spawns nothing, so the
 * quest that cleanses it cannot be one that fails and is taken again, and a kill or a drop another objective needs from
 * that sub-dungeon has to be earned before the cleanse: in the cleansing quest it is the objective its clear_area waits
 * for (`after`); in another quest, that quest is a prerequisite of the cleansing one.
 */
function checkCleanseOrder(cleansers: Map<string, string>): void {
  type Q = { id: string; prereqQuests?: string[]; escortNpc?: unknown; defendTarget?: unknown; objectives: RemakeObjective[] };
  const all = AllQuests as unknown as Q[];
  const byId = new Map(all.map(q => [q.id, q]));
  const requires = (quest: Q, prereq: string): boolean => {
    const open = [quest];
    const seen = new Set<string>();
    while (open.length > 0) {
      const at = open.pop()!;
      if (seen.has(at.id)) continue;
      seen.add(at.id);
      for (const p of at.prereqQuests ?? []) {
        if (p === prereq) return true;
        const next = byId.get(p);
        if (next) open.push(next);
      }
    }
    return false;
  };
  for (const [zone, cleanserId] of cleansers) {
    const cleanser = byId.get(cleanserId)!;
    assert(!cleanser.escortNpc && !cleanser.defendTarget,
      `quests.${cleanser.id}.objectives: a quest that can fail (an escort / a defend target) cannot cleanse '${zone}': taken again, it would find the sub-dungeon empty`);
    const after = cleanser.objectives.find(o => o.type === 'clear_area' && o.targetId === zone)?.after ?? -1;
    for (const q of all) {
      q.objectives.forEach((o, i) => {
        if (o.type === 'clear_area' || o.zone !== zone) return;
        const source = (o as unknown as { source?: { kind?: string } }).source;
        const needsMonsters = o.type === 'kill' || ((o.type === 'collect' || o.type === 'craft_collect') && source?.kind === 'drop');
        if (!needsMonsters) return;
        const where = `quests.${q.id}.objectives[${i}].zone`;
        if (q === cleanser) {
          assert(i === after, `${where}: '${zone}' spawns nothing once this quest has cleansed it: its clear_area has to wait for this objective (\`after\`)`);
        } else {
          assert(requires(cleanser, q.id), `${where}: '${zone}' spawns nothing once quest '${cleanser.id}' has cleansed it: '${q.id}' has to be a prerequisite of that quest`);
        }
      });
    }
  }
}

export function exportQuests(): TableResult[] {
  const ids = new Set<string>();
  const cleansers = new Map<string, string>();
  const quests = AllQuests.map(q => {
    assert(!ids.has(q.id), `duplicate quest ${q.id}`);
    ids.add(q.id);
    const giver = questGiverOf(q.id);
    gateRemakeValues('quest', q, `quests.${q.id}`);
    checkK1Hooks(q);
    checkK6Quest(q);
    checkK7Quest(q, cleansers);
    const out = withholdRemakeFields('quest', plain(q), `quests.${q.id}`);
    // 2F: a hunt's own abilities live on its definition (quest_hunts.json defsNormal, tables/monsters.ts), not here.
    if (out.hunts) out.hunts = (out.hunts as RemakeQuestHunt[]).map(({ abilities: _abilities, ...h }) => h);
    // 3B review: a sub-dungeon objective's `spot` is its `location` in quests.json (the one field the core reads). A
    // spot the schema gate withheld is gone already.
    if (Array.isArray(out.objectives)) {
      const objectives = out.objectives as unknown as Record<string, unknown>[];
      (out as unknown as Record<string, unknown>).objectives = objectives.map(o => {
        if (o.spot === undefined) return o;
        const { spot, ...rest } = o;
        return { ...rest, location: spot };
      });
    }
    return {
      ...out,
      derived: {
        giverNpcId: giver,
        objectiveItemKinds: q.objectives.map(o => (isCollectObjective(o) ? questItemKindOf(o) : null)),
        embersOnTurnIn: Homestead.embersForQuest(q),
        i18n: { name: `data.quest.${q.id}.name`, desc: `data.quest.${q.id}.desc`, offer: `data.quest.${q.id}.offer`, complete: `data.quest.${q.id}.complete` },
      },
    };
  });
  for (const q of AllQuests) for (const p of q.prereqQuests ?? []) assert(ids.has(p), `${q.id}: unknown prereq ${p}`);
  checkCleanseOrder(cleansers);
  for (const npc of Object.values(NPCDefinitions)) for (const q of npc.quests ?? []) assert(ids.has(q), `${npc.id} lists unknown quest ${q}`);
  const byZone: Record<string, number> = {};
  for (const q of AllQuests) byZone[q.zone] = (byZone[q.zone] ?? 0) + 1;

  // quest_tuning.json — constants of the quest runtime (quests-story-ch1 §14), pinned to source.
  assertSource('src/systems/QuestSystem.ts', 'if (q.level > playerLevel + 5) return false;');
  assertSource(ZS, 'const baseHp = quest.level * 20 + 100;', 'let budget = (this.player.moveSpeed / 38) * 0.9 * (delta / 1000);',
    'if (escortDistSq <= 25) {', 'if (playerDistSq <= 36) {', 'const baseHp = quest.level * 30 + 200;',
    'if (!this.defendWaveActive && playerDistSq < 225) {', 'if (time - this.defendWaveTimer > 5000) {');
  const tuning = {
    levelGateAbove: 5,
    gatherRange: exp<number>(QuestWorldMod, 'GATHER_RANGE'),
    clueRange: exp<number>(QuestWorldMod, 'CLUE_RANGE'),
    guideNear: exp<number>(QuestWorldMod, 'GUIDE_NEAR'),
    fallbackCollectChance: FALLBACK_COLLECT_CHANCE,
    itemKindByTarget: plain(exp<Record<string, string>>(QuestWorldMod, 'KIND_BY_TARGET')),
    defaultItemKind: 'relic',
    escort: { hpBase: 100, hpPerLevel: 20, speedDivisor: 38, speedFactor: 0.9, catchUpTiles: 14, arriveEscortSq: 25, arriveHeroSq: 36,
      threatRangeSq: 16, hitIntervalMs: 2000, dmgMul: 0.3 },
    defend: { hpBase: 200, hpPerLevel: 30, startRangeSq: 225, waveDelayMs: 5000, hitRangeSq: 9, hitIntervalMs: 2000, dmgMul: 0.2 },
    embers: { questMain: 2, questSide: 1, override: 'rewards.embers' },
    port: { dialogueRewardsOncePerChoice: 'Q1', goblinChiefAreaCentredOnSpawn: 'Q4', escortLabelFix: 'Q8' },
  };
  return [
    {
      file: 'quests.json',
      source: ['src/data/quests/all_quests.ts', 'src/systems/QuestWorld.ts', 'src/data/npcs.ts'],
      // derived.embersOnTurnIn: embersForQuest.
      deps: ['src/data/homestead.ts'],
      data: { quests },
      counts: { quests: quests.length, emeraldPlains: byZone.emerald_plains ?? 0, hunts: AllQuests.reduce((n, q) => n + (q.hunts?.length ?? 0), 0) },
    },
    {
      file: 'quest_tuning.json',
      source: ['src/systems/QuestWorld.ts', 'src/systems/QuestSystem.ts', 'src/systems/QuestRewards.ts', ZS],
      data: tuning,
      counts: { constants: Object.keys(tuning).length },
    },
  ];
}

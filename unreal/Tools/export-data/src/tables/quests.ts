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

const ZS = 'src/scenes/ZoneScene.ts';
const exp = <T>(mod: unknown, name: string): T => {
  const v = (mod as Record<string, unknown>)[`__expose_${name}`];
  assert(v !== undefined, `exposed binding ${name} missing`);
  return v as T;
};

export function exportQuests(): TableResult[] {
  const ids = new Set<string>();
  const quests = AllQuests.map(q => {
    assert(!ids.has(q.id), `duplicate quest ${q.id}`);
    ids.add(q.id);
    const giver = questGiverOf(q.id);
    return {
      ...plain(q),
      derived: {
        giverNpcId: giver,
        objectiveItemKinds: q.objectives.map(o => (isCollectObjective(o) ? questItemKindOf(o) : null)),
        embersOnTurnIn: Homestead.embersForQuest(q),
        i18n: { name: `data.quest.${q.id}.name`, desc: `data.quest.${q.id}.desc`, offer: `data.quest.${q.id}.offer`, complete: `data.quest.${q.id}.complete` },
      },
    };
  });
  for (const q of AllQuests) for (const p of q.prereqQuests ?? []) assert(ids.has(p), `${q.id}: unknown prereq ${p}`);
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

/**
 * autopilot_routes.json — the bot's routes (implementation-plan.md §6): unreal/GameData/autopilot/*.ts, written verbatim
 * (step order = play order). `abyss::AutoPilot` (Source/AbyssCore/Private/tools/AutoPilot.cpp) interprets them; its
 * Chapter1 scenario plays the route of the starting zone and its Smoke scenario the same route up to `smokeEnd`.
 *
 * A route is content: every id it names is checked here against the content the export loads (quests, NPCs and their
 * dialogue trees, the zone's monsters, hunts, mini-boss and story boss, map bounds, clue and explore locations), and
 * every quest of the route's zone must be accepted by one of its steps, so a content change that strands the bot fails
 * the export instead of the playthrough. The C++ loader (LoadAutoPilot.cpp) and DataStore::Finalize check the shape and
 * the ids again.
 */
import { AllQuests } from '../../../../../src/data/quests/all_quests';
import { NPCDefinitions } from '../../../../../src/data/npcs';
import { MonstersByZone } from '../../../../../src/data/monsters';
import { MiniBossByZone } from '../../../../../src/data/miniBosses';
import { AllMaps } from '../../../../../src/data/maps';
import { CH1_ROUTE } from '../../../../GameData/autopilot/ch1';
import type { ApCondition, ApStep, ApTile, AutoPilotRoute } from '../../../../GameData/autopilot/types';
import type { QuestDefinition } from '../../../../GameData/types';
import { CHAPTER_CONFIG } from '../../../../GameData/config';
import { assert, plain, type TableResult } from '../util';

const ROUTES: readonly AutoPilotRoute[] = [CH1_ROUTE];

/** Step kinds the core's interpreter knows (AutoPilotStepKind in AutoPilotData.h, same spelling and order). */
export const STEP_KINDS = ['checkpoint', 'note', 'waitIdle', 'talk', 'closePanels', 'tree', 'visit', 'shop', 'townTrip',
  'gather', 'hunt', 'boss', 'clues', 'explore', 'escort', 'defend', 'goTo', 'portalHome', 'equipUpgrades',
  'smokeEnd'] as const satisfies readonly ApStep['kind'][];

const quests = new Map((AllQuests as QuestDefinition[]).map(q => [q.id, q]));

function checkRoute(r: AutoPilotRoute): void {
  const at = (i: number) => `autopilot route ${r.id} step ${i}`;
  const map = AllMaps[r.zone];
  assert(map, `autopilot route ${r.id}: unknown zone '${r.zone}'`);
  const zoneMonsters = new Set((MonstersByZone[r.zone] ?? []).map(m => m.id));
  const zoneQuests = [...quests.values()].filter(q => q.zone === r.zone);
  const zoneHunts = new Set(zoneQuests.flatMap(q => (q.hunts ?? []).map(h => h.huntId)));
  const tile = (t: ApTile, where: string) => {
    assert(Array.isArray(t) && t.length === 2 && t.every(n => Number.isInteger(n)), `${where}: a tile is [col, row]`);
    assert(t[0] >= 0 && t[0] < map.cols && t[1] >= 0 && t[1] < map.rows, `${where}: tile [${t}] outside ${r.zone}`);
  };
  const quest = (id: string, where: string) => {
    const q = quests.get(id);
    assert(q, `${where}: unknown quest '${id}'`);
    assert(q.zone === r.zone, `${where}: quest '${id}' is not in ${r.zone}`);
    return q;
  };
  const npc = (id: string, where: string) => {
    const n = NPCDefinitions[id];
    assert(n, `${where}: unknown npc '${id}'`);
    return n;
  };
  const objective = (q: QuestDefinition, obj: number, where: string) => {
    assert(Number.isInteger(obj) && obj >= 0 && obj < q.objectives.length, `${where}: ${q.id} has no objective ${obj}`);
    return q.objectives[obj];
  };
  const conditions = (until: readonly ApCondition[], where: string) => {
    assert(until.length > 0, `${where}: 'until' needs at least one condition`);
    for (const c of until) {
      const q = quest(c.quest, where);
      if (c.kind === 'objective') {
        const o = objective(q, c.obj, where);
        assert(Number.isInteger(c.atLeast) && c.atLeast >= 1 && c.atLeast <= o.required,
          `${where}: ${q.id}#${c.obj} atLeast ${c.atLeast} (required ${o.required})`);
      } else {
        assert(c.kind === 'questDone', `${where}: unknown condition kind '${(c as { kind: string }).kind}'`);
      }
    }
  };
  const accepted = new Set<string>();
  const checkpoints = new Set<string>();
  let smokeEnds = 0;
  r.steps.forEach((s, i) => {
    const where = `${at(i)} (${s.kind})`;
    assert((STEP_KINDS as readonly string[]).includes(s.kind), `${where}: unknown step kind`);
    switch (s.kind) {
      case 'checkpoint':
        assert(/^[a-z0-9_]+$/.test(s.id), `${where}: checkpoint ids are snake_case ASCII`);
        assert(!checkpoints.has(s.id), `${where}: duplicate checkpoint '${s.id}'`);
        checkpoints.add(s.id);
        break;
      case 'note': assert(s.text !== '' && /^[\x20-\x7e]+$/.test(s.text), `${where}: notes are printable ASCII`); break;
      case 'waitIdle': assert(s.maxSec > 0, `${where}: maxSec > 0`); break;
      case 'talk': npc(s.npc, where); break;
      case 'closePanels': case 'portalHome': case 'equipUpgrades': break;
      case 'tree': {
        const tree = npc(s.npc, where).dialogueTree;
        assert(tree, `${where}: npc '${s.npc}' has no dialogue tree`);
        assert(s.path.length > 0, `${where}: empty path`);
        let node = tree.nodes[tree.startNodeId];
        for (const next of s.path) {
          const choice = node?.choices?.find(c => c.nextNodeId === next);
          assert(choice, `${where}: no choice leads from '${node?.id}' to '${next}'`);
          if (choice.questTrigger) accepted.add(choice.questTrigger);
          node = tree.nodes[next];
        }
        break;
      }
      case 'visit': {
        const n = npc(s.npc, where);
        for (const id of s.accept) {
          quest(id, where);
          assert(n.quests?.includes(id), `${where}: '${s.npc}' does not give '${id}'`);
          accepted.add(id);
        }
        break;
      }
      case 'shop':
        assert(npc(s.npc, where).shopItems?.length, `${where}: '${s.npc}' has no shop`);
        for (const v of [s.hp, s.mp]) assert(v === undefined || (Number.isInteger(v) && v >= 0), `${where}: potion targets are counts`);
        break;
      case 'townTrip': assert(npc(s.npc, where).shopItems?.length, `${where}: '${s.npc}' has no shop`); break;
      case 'gather': {
        const o = objective(quest(s.quest, where), s.obj, where);
        assert(o.source?.kind === 'gather', `${where}: ${s.quest}#${s.obj} is not a gather objective`);
        assert(Number.isInteger(s.count) && s.count >= 1 && s.count <= o.required, `${where}: count ${s.count}`);
        break;
      }
      case 'hunt':
        assert(s.monsters.length > 0, `${where}: no monsters`);
        for (const m of s.monsters) {
          assert(zoneMonsters.has(m) || zoneHunts.has(m) || MiniBossByZone[r.zone]?.id === m,
            `${where}: '${m}' is neither a monster nor a hunt of ${r.zone}`);
        }
        tile(s.anchor, where);
        conditions(s.until, where);
        assert(s.maxSec > 0, `${where}: maxSec > 0`);
        break;
      case 'boss':
        if (s.role === 'storyBoss') {
          assert(zoneMonsters.has(CHAPTER_CONFIG.storyBoss.monsterId), `${where}: the story boss does not live in ${r.zone}`);
        } else if (s.role === 'miniBoss') {
          assert(MiniBossByZone[r.zone], `${where}: ${r.zone} has no mini-boss`);
        } else {
          assert(s.role === 'huntLeader' && zoneHunts.size > 0, `${where}: role '${s.role}' has no monster in ${r.zone}`);
        }
        tile(s.anchor, where);
        conditions(s.until, where);
        assert(s.maxSec > 0, `${where}: maxSec > 0`);
        break;
      case 'clues': {
        const q = quest(s.quest, where);
        const clueObjs = q.objectives.filter(o => o.type === 'investigate_clue');
        assert(s.tiles.length > 0 && s.tiles.length <= clueObjs.length, `${where}: ${s.tiles.length} tile(s) for ${clueObjs.length} clue(s)`);
        s.tiles.forEach((t, k) => {
          tile(t, where);
          const loc = clueObjs[k].location;
          assert(loc && Math.hypot(loc.col - t[0], loc.row - t[1]) < 3, `${where}: tile [${t}] is not clue ${k}'s mark`);
        });
        break;
      }
      case 'explore': {
        const o = objective(quest(s.quest, where), s.obj, where);
        assert(o.type === 'explore' && o.location, `${where}: ${s.quest}#${s.obj} is not an explore objective`);
        tile(s.tile, where);
        assert(Math.hypot(o.location.col - s.tile[0], o.location.row - s.tile[1]) <= o.location.radius,
          `${where}: tile [${s.tile}] outside the explore circle`);
        break;
      }
      case 'escort':
        assert(quest(s.quest, where).escortNpc, `${where}: ${s.quest} has no escort NPC`);
        assert(Number.isInteger(s.tries) && s.tries >= 1, `${where}: tries >= 1`);
        break;
      case 'defend':
        assert(quest(s.quest, where).defendTarget, `${where}: ${s.quest} has no defend target`);
        assert(Number.isInteger(s.tries) && s.tries >= 1, `${where}: tries >= 1`);
        assert(s.standOff >= 0 && s.standOff < 8, `${where}: standOff in [0, 8) (inside the wave ring)`);
        break;
      case 'goTo':
        tile(s.tile, where);
        assert(s.arrive > 0 && s.maxSec > 0, `${where}: arrive and maxSec > 0`);
        break;
      case 'smokeEnd': ++smokeEnds; break;
    }
  });
  assert(smokeEnds <= 1, `autopilot route ${r.id}: more than one smokeEnd`);
  for (const q of zoneQuests) {
    assert(accepted.has(q.id), `autopilot route ${r.id}: no step accepts ${q.id} (every quest of ${r.zone} must be played)`);
  }
}

export function exportAutoPilot(): TableResult[] {
  const ids = new Set<string>();
  for (const r of ROUTES) {
    assert(!ids.has(r.id), `duplicate autopilot route ${r.id}`);
    ids.add(r.id);
    checkRoute(r);
  }
  return [{
    file: 'autopilot_routes.json',
    source: ['unreal/GameData/autopilot/ch1.ts', 'unreal/GameData/autopilot/types.ts'],
    data: { stepKinds: [...STEP_KINDS], routes: ROUTES.map(r => plain(r)) },
    counts: { routes: ROUTES.length, steps: ROUTES.reduce((n, r) => n + r.steps.length, 0) },
  }];
}

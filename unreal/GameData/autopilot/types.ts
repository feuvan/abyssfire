/**
 * The bot's route as data (implementation-plan.md §6): `abyss::AutoPilot` (Source/AbyssCore/Private/tools/AutoPilot.cpp)
 * plays a route step by step; each step kind maps onto one of its tasks. Exported to `Data/autopilot_routes.json` by
 * unreal/Tools/export-data/src/tables/autopilot.ts, which checks every id against the exported content; the core's
 * DataStore checks them again at load (Finalize).
 *
 * Type-only module (main.ts TYPE_ONLY_MODULES): the routes live in `./ch1.ts` (and later chapters' files).
 */

/** A tile `[col, row]` (the bot walks to its centre). */
export type ApTile = readonly [number, number];

/** A condition on quest progress; a step's `until` list holds when every entry holds. */
export type ApCondition =
  /** The quest is completed or turned in. */
  | { readonly kind: 'questDone'; readonly quest: string }
  /** Objective `obj` (index into the quest's objectives) has reached `atLeast`. */
  | { readonly kind: 'objective'; readonly quest: string; readonly obj: number; readonly atLeast: number };

/** Monster roles the `boss` step hunts by (the core's MonsterRole). */
export type ApBossRole = 'storyBoss' | 'miniBoss' | 'huntLeader';

export type ApStep =
  /** A named stop: the playthrough test checks the state there; costs no frame. */
  | { readonly kind: 'checkpoint'; readonly id: string }
  /** A line in the bot's diary. */
  | { readonly kind: 'note'; readonly text: string }
  /** Until no story beat and no core modal is up (closes stray modals); fails after `maxSec`. */
  | { readonly kind: 'waitIdle'; readonly maxSec: number }
  /** Walk to the NPC and talk (one of its panels opens). */
  | { readonly kind: 'talk'; readonly npc: string }
  | { readonly kind: 'closePanels' }
  /** Talk, open the NPC's dialogue tree and choose the buttons leading to `path` node by node, then Leave. */
  | { readonly kind: 'tree'; readonly npc: string; readonly path: readonly string[] }
  /** Work the NPC's quest card: turn in everything completed, then accept exactly the quests in `accept`. */
  | { readonly kind: 'visit'; readonly npc: string; readonly accept: readonly string[] }
  /** Sell spare gear and buy HP / MP potions up to `hp` / `mp` (omitted: the class's targets). */
  | { readonly kind: 'shop'; readonly npc: string; readonly hp?: number; readonly mp?: number }
  /** Shop at `npc` (class potion targets), then equip upgrades. */
  | { readonly kind: 'townTrip'; readonly npc: string }
  /** Walk onto gather nodes until objective `obj` of `quest` reaches `count`. */
  | { readonly kind: 'gather'; readonly quest: string; readonly obj: number; readonly count: number }
  /** Kill the nearest monsters of these defs (monster or hunt ids) around `anchor` until `until` holds. */
  | { readonly kind: 'hunt'; readonly monsters: readonly string[]; readonly anchor: ApTile;
      readonly until: readonly ApCondition[]; readonly maxSec: number }
  /** Like `hunt`, the targets chosen by role (the story boss, the zone mini-boss, a hunt leader). */
  | { readonly kind: 'boss'; readonly role: ApBossRole; readonly anchor: ApTile;
      readonly until: readonly ApCondition[]; readonly maxSec: number }
  /** Examine the clue marks of `quest` near these tiles, in objective order. */
  | { readonly kind: 'clues'; readonly quest: string; readonly tiles: readonly ApTile[] }
  /** Stand inside the explore circle around `tile` until objective `obj` counts. */
  | { readonly kind: 'explore'; readonly quest: string; readonly obj: number; readonly tile: ApTile }
  /** Lead the quest's escort NPC home; a failed escort is taken again from its giver, at most `tries` attempts. */
  | { readonly kind: 'escort'; readonly quest: string; readonly tries: number }
  /**
   * Hold the quest's defend target through every wave: stand `standOff` tiles out from it towards the waves'
   * approach, fight the wave monsters (nearest to the target first), retreat to a camp to heal under 30 % life without
   * potions. A fallen target is taken again from its giver, at most `tries` attempts.
   */
  | { readonly kind: 'defend'; readonly quest: string; readonly tries: number; readonly standOff: number }
  /** Walk to `tile` (fighting on the way) until within `arrive` tiles; fails after `maxSec`. */
  | { readonly kind: 'goTo'; readonly tile: ApTile; readonly arrive: number; readonly maxSec: number }
  /** Town portal to the nearest camp (walks on when it cannot cast). */
  | { readonly kind: 'portalHome' }
  /** Equip every bag item that beats what its slot holds. */
  | { readonly kind: 'equipUpgrades' }
  /** The smoke scenario ends here (the in-game autotest's `smoke`); the chapter scenario goes on. Once per route. */
  | { readonly kind: 'smokeEnd' };

export interface AutoPilotRoute {
  /** Route id (unique). */
  readonly id: string;
  /** The zone it plays; the core's Chapter1 scenario plays the route of the starting zone (maps.json defaultMap). */
  readonly zone: string;
  readonly steps: readonly ApStep[];
}

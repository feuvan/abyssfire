import type { AutoPilotRoute } from './types';

/**
 * The bot's Chapter 1 route (implementation-plan.md §6, package 1B part 3): the headless playthrough test
 * (CoreTests/tests/playthrough_test.cpp, three classes) and the in-game autotest (`-abyssautotest=smoke|chapter`) play
 * it through `abyss::AutoPilot`. Every quest of the Red Field is accepted and turned in, the mini-boss and the story
 * boss fall, and every Chapter 1 beat plays (chapter1.md §5-8).
 *
 * Outline: camp tour (Hald, Tamsin, the Captain's tree, Aldwin: S2 + S5) → barrowmoss → the trenches (M1) and the Seep
 * (six ichor) → the Captain: M1 (`cs_rf_brand`), take M2; Aldwin: S2, take S3 → **smoke ends** → tag-thief clues 1-2,
 * Marl (S1's talk), clue 3, Dreck → gnawers and the Hungerbell (M2) → the lantern graves (S5) → home: S1 + M2, take
 * M3; S3 + S5 (the Lanternwisp), take S6 → Ashwheat, the Corpseherd (M3, `cs_rf_herd`) → escort Old Bern (S6) →
 * Corran: S4, Harke → home: M3 (`cs_rf_whisper`), take M4; S6 → Valdras (M4, his intro and `cs_rf_marshal_falls`) →
 * home: M4, take M5 (`cs_rf_siege`) → the Boneyard Gate siege → home: M5 (`cs_rf_finale`) → equip. Wave 3 inserts the
 * Pit after M2.
 *
 * Checkpoints are where the test checks the state (playthrough_test.cpp re-pins them with the route); the in-game
 * autotest starts its camp tour at `intro`. A town portal takes the nearest camp: before each portal home the route
 * stands on Knellwatch's side of the line between the two camps (160 col + 170 row < 18575), so the bot never walks
 * back from the Crowmarket camp through the far half of the map (its Lv 6-7 monsters would push the hero past the
 * DR9 level band). Coordinates are the map's generator inputs (chapter1.md §17): the
 * tag-thief and lantern clues, Ashwheat, the Sheepfold (60,55), Corran's culvert, the Breach Rim (15,95).
 */
export const CH1_ROUTE: AutoPilotRoute = {
  id: 'ch1',
  zone: 'emerald_plains',
  steps: [
    // ---- new game: prologue (skipped) + chapter card ----
    { kind: 'checkpoint', id: 'start' },
    { kind: 'waitIdle', maxSec: 180 },
    { kind: 'checkpoint', id: 'intro' },

    // ---- Knellwatch: Hald, Tamsin, the Captain's tree (reward_ask once, S1 by its trigger), M1, Aldwin's S2 + S5 ----
    { kind: 'note', text: '== Knellwatch' },
    { kind: 'talk', npc: 'blacksmith' },
    { kind: 'checkpoint', id: 'blacksmith' },
    { kind: 'closePanels' },
    { kind: 'talk', npc: 'stash' },
    { kind: 'checkpoint', id: 'stash' },
    { kind: 'closePanels' },
    { kind: 'tree', npc: 'quest_elder', path: ['reward_ask', 'help', 'other_work', 'accept_ichor'] },
    { kind: 'checkpoint', id: 'captain_tree' },
    { kind: 'visit', npc: 'quest_elder', accept: ['q_rf_burial_detail'] },
    { kind: 'visit', npc: 'knell_chaplain', accept: ['q_rf_barrowmoss', 'q_rf_lanternwisp'] },
    { kind: 'checkpoint', id: 'first_quests' },
    { kind: 'shop', npc: 'merchant', hp: 2, mp: 0 },

    // ---- barrowmoss (S2), the Burial Trenches (M1), the Seep (S1's ichor) ----
    { kind: 'note', text: '== barrowmoss' },
    { kind: 'gather', quest: 'q_rf_barrowmoss', obj: 0, count: 5 },
    { kind: 'checkpoint', id: 'barrowmoss' },
    { kind: 'note', text: '== the trenches' },
    { kind: 'hunt', monsters: ['risen_footman'], anchor: [25, 20], maxSec: 600,
      until: [{ kind: 'questDone', quest: 'q_rf_burial_detail' }] },
    { kind: 'checkpoint', id: 'trenches' },
    { kind: 'note', text: '== the Seep' },
    { kind: 'hunt', monsters: ['breachclot'], anchor: [20, 45], maxSec: 600,
      until: [{ kind: 'objective', quest: 'q_rf_black_blood', obj: 0, atLeast: 6 }] },
    { kind: 'checkpoint', id: 'seep' },

    // ---- home: M1 (cs_rf_brand), take M2; Aldwin: S2, take S3 ----
    { kind: 'note', text: '== the Captain: M1' },
    { kind: 'portalHome' },
    { kind: 'visit', npc: 'quest_elder', accept: ['q_rf_gnawed_field'] },
    { kind: 'checkpoint', id: 'captain_m1_turned_in' },
    { kind: 'waitIdle', maxSec: 120 },
    { kind: 'checkpoint', id: 'captain_m1' },
    { kind: 'visit', npc: 'knell_chaplain', accept: ['q_rf_tag_thief'] },
    { kind: 'checkpoint', id: 'chaplain_m1' },
    { kind: 'smokeEnd' },
    { kind: 'townTrip', npc: 'merchant' },

    // ---- the tag thief: clues 1-2, Marl (S1's delivery), clue 3, Dreck ----
    { kind: 'note', text: '== the tag thief + Marl' },
    { kind: 'clues', quest: 'q_rf_tag_thief', tiles: [[30, 24], [42, 34]] },
    { kind: 'shop', npc: 'plains_herbalist' },
    { kind: 'checkpoint', id: 'marl' },
    { kind: 'clues', quest: 'q_rf_tag_thief', tiles: [[30, 24], [42, 34], [54, 44]] },
    { kind: 'hunt', monsters: ['hunt_dreck'], anchor: [60, 48], maxSec: 300,
      until: [{ kind: 'questDone', quest: 'q_rf_tag_thief' }] },
    { kind: 'checkpoint', id: 'dreck' },

    // ---- the Gnawed Field: twelve gravegnawers, then the Hungerbell (M2) ----
    { kind: 'note', text: '== the Gnawed Field' },
    { kind: 'hunt', monsters: ['gravegnawer'], anchor: [35, 40], maxSec: 600,
      until: [{ kind: 'objective', quest: 'q_rf_gnawed_field', obj: 0, atLeast: 12 }] },
    { kind: 'hunt', monsters: ['hunt_hungerbell'], anchor: [45, 35], maxSec: 300,
      until: [{ kind: 'questDone', quest: 'q_rf_gnawed_field' }] },
    { kind: 'checkpoint', id: 'gnawed_field' },

    // ---- the light in the Weeping Ditch (S5) ----
    { kind: 'note', text: '== the lantern graves' },
    { kind: 'clues', quest: 'q_rf_lanternwisp', tiles: [[78, 12], [92, 10], [100, 22], [86, 28]] },
    { kind: 'checkpoint', id: 'lanternwisp' },

    // ---- home: S1 + M2, take M3; Aldwin: S3 + S5 (the Lanternwisp), take S6 ----
    { kind: 'note', text: '== home: M2' },
    // The last lantern grave lies on the line between the two camps: step west so the portal takes Knellwatch.
    { kind: 'goTo', tile: [78, 30], arrive: 3, maxSec: 120 },
    { kind: 'portalHome' },
    { kind: 'visit', npc: 'quest_elder', accept: ['q_rf_ashwheat'] },
    { kind: 'visit', npc: 'knell_chaplain', accept: ['q_rf_hallowed_ground'] },
    { kind: 'checkpoint', id: 'home_m2_turned_in' },
    { kind: 'waitIdle', maxSec: 120 },
    { kind: 'checkpoint', id: 'home_m2' },
    { kind: 'townTrip', npc: 'merchant' },

    // ---- M3: Ashwheat, then the Corpseherd at the Sheepfold (cs_rf_herd) ----
    { kind: 'note', text: '== Ashwheat + the Corpseherd' },
    { kind: 'explore', quest: 'q_rf_ashwheat', obj: 0, tile: [40, 52] },
    { kind: 'checkpoint', id: 'ashwheat' },
    { kind: 'goTo', tile: [60, 55], arrive: 8, maxSec: 300 },
    { kind: 'boss', role: 'miniBoss', anchor: [60, 55], maxSec: 300,
      until: [{ kind: 'questDone', quest: 'q_rf_ashwheat' }] },
    { kind: 'checkpoint', id: 'corpseherd' },

    // ---- S6: Old Bern to the last hallowed ground ----
    { kind: 'note', text: '== Old Bern' },
    { kind: 'escort', quest: 'q_rf_hallowed_ground', tries: 3 },
    { kind: 'checkpoint', id: 'escort' },

    // ---- S4: Corran's bounty on Harke ----
    { kind: 'note', text: '== Corran + Harke' },
    { kind: 'visit', npc: 'plains_wanderer', accept: ['q_rf_oathless'] },
    { kind: 'checkpoint', id: 'corran' },
    { kind: 'hunt', monsters: ['hunt_harke'], anchor: [82, 72], maxSec: 400,
      until: [{ kind: 'questDone', quest: 'q_rf_oathless' }] },
    { kind: 'checkpoint', id: 'harke' },
    { kind: 'visit', npc: 'plains_wanderer', accept: [] },
    { kind: 'checkpoint', id: 'oathless' },

    // ---- home: M3 (cs_rf_whisper), take M4; Aldwin: S6 ----
    { kind: 'note', text: '== home: M3' },
    // Corran stands nearer the Crowmarket camp: walk west first so the portal takes Knellwatch (no walk back past the
    // Crowmarket's bonegorgers).
    { kind: 'goTo', tile: [50, 50], arrive: 3, maxSec: 300 },
    { kind: 'portalHome' },
    { kind: 'goTo', tile: [15, 18], arrive: 4, maxSec: 400 },
    { kind: 'visit', npc: 'quest_elder', accept: ['q_rf_marshal'] },
    { kind: 'checkpoint', id: 'home_m3_turned_in' },
    { kind: 'waitIdle', maxSec: 120 },
    { kind: 'visit', npc: 'knell_chaplain', accept: [] },
    { kind: 'checkpoint', id: 'home_m3' },
    { kind: 'townTrip', npc: 'merchant' },

    // ---- M4: Lord-Marshal Valdras on the Breach Rim (boss intro, cs_rf_marshal_falls); 450 s (2F review: the longest
    // sweep fight is under 240 s, the rest is the walk back from a death) ----
    { kind: 'note', text: '== Valdras' },
    { kind: 'goTo', tile: [18, 88], arrive: 3, maxSec: 400 },
    { kind: 'boss', role: 'storyBoss', anchor: [15, 95], maxSec: 450,
      until: [{ kind: 'questDone', quest: 'q_rf_marshal' }] },
    { kind: 'checkpoint', id: 'valdras' },

    // ---- home: M4 (weapon / armour choice), take M5 (cs_rf_siege) ----
    { kind: 'note', text: '== home: M4' },
    // The Rim's floor is nearer the Crowmarket camp: back up the approach first so the portal takes Knellwatch.
    { kind: 'goTo', tile: [18, 88], arrive: 2, maxSec: 120 },
    { kind: 'portalHome' },
    { kind: 'visit', npc: 'quest_elder', accept: ['q_rf_last_toll'] },
    { kind: 'checkpoint', id: 'home_m4_turned_in' },
    { kind: 'waitIdle', maxSec: 120 },
    { kind: 'checkpoint', id: 'home_m4' },
    { kind: 'townTrip', npc: 'merchant' },

    // ---- M5: hold the Boneyard Gate till dawn ----
    { kind: 'note', text: '== the Boneyard Gate' },
    { kind: 'defend', quest: 'q_rf_last_toll', tries: 3, standOff: 3 },
    { kind: 'checkpoint', id: 'siege' },

    // ---- home: M5 (cs_rf_finale) ----
    { kind: 'note', text: '== home: M5' },
    { kind: 'goTo', tile: [15, 18], arrive: 4, maxSec: 400 },
    { kind: 'visit', npc: 'quest_elder', accept: [] },
    { kind: 'checkpoint', id: 'home_m5_turned_in' },
    { kind: 'waitIdle', maxSec: 120 },
    { kind: 'checkpoint', id: 'finale' },
    { kind: 'equipUpgrades' },
  ],
};

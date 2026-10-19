// Test content layer (implementation-plan.md 8 "T0", work package 0B).
//
// The tests are split so that a content change re-pins one place:
//   - content_ch1_test.cpp holds every assertion that pins Chapter 1 content (quest / NPC / monster / story /
//     dialogue ids, coordinates, exp and gold totals, the golden map facts of emerald_plains, ...), moved there
//     verbatim from the area test files. playthrough_test.cpp (the bot's Chapter 1 route) is the other file the
//     content switch re-pins.
//   - this header holds the Chapter 1 handles that systems tests still need on the live data (`ch1::`), the list of
//     Chapter 1 content ids that no other test file may spell (`ch1::kPinnedIds`, enforced by content_ch1_test.cpp),
//     and synthetic fixtures (`fixture::`) for the tests that only need "a melee monster", "an elite", "a slime" or "a
//     ranged caster".
// Every other test file tests a system: through these handles, through the fixtures, or through its own synthetic
// data. The content switch (C1) re-pins this header's `ch1` block, content_ch1_test.cpp and playthrough_test.cpp.
// Caveat for C1: several systems rigs still stand on the live Chapter 1 map (world_test's WorldRig, the monsters / pets
// zone rigs entering `ch1::kZone`, combat_test's open tile (48,40), coordinates relative to `kExit` / `kCamp`). The
// remake keeps those anchors, but the one golden-map re-baseline (DR4) re-rolls the decor, so a prop may land on a rig's
// tile: re-run the whole suite after the switch and move such a rig to another open tile (a systems-file edit that
// changes no assertion).
#pragma once

#include <array>
#include <string>
#include <string_view>

#include "abyss/data/MonsterData.h"

namespace abyss::test {

// ---------------------------------------------------------------------------------------------------------------------
// Chapter 1 handles (live data). Role keys the remake keeps (chapter1.md 3, "NPC ids are kept as role keys") are named
// by role; a system test that needs "the starting zone" or "an NPC with a dialogue tree in the first camp" names it
// here, never as a literal.
// ---------------------------------------------------------------------------------------------------------------------
namespace ch1 {

inline constexpr char kZone[] = "emerald_plains";         // the Chapter 1 zone (MapData defaultMap)
inline constexpr char kElder[] = "quest_elder";           // camp 1: the main quest giver with the dialogue tree
inline constexpr char kHerbalist[] = "plains_herbalist";  // field NPC (50,30): a merchant with a shop
inline constexpr char kWanderer[] = "plains_wanderer";    // field NPC (70,65): a side-quest giver with linear lines

// Map coordinates the remake keeps (chapter1.md 17: map-generator inputs, zero churn).
inline constexpr TilePos kCamp{15, 15};         // camp 1: the respawn point (camps[0])
inline constexpr TilePos kPlayerStart{15, 22};  // a new hero's tile
inline constexpr TilePos kExit{119, 60};        // the east exit to twilight_forest

inline constexpr char kStoryBoss[] = "valdras";  // monster_ai storyBoss (M7); audio bossMusic scores it boss_ch1

// Main-chain slots (chapter1.md "Id map": the remake keeps each slot's role and order; package 1B part 2 renamed them).
inline constexpr char kQuestM1[] = "q_rf_burial_detail";  // M1: the first main quest (slot q_kill_slimes)
inline constexpr char kQuestM2[] = "q_rf_gnawed_field";   // M2 (slot q_kill_goblins)
inline constexpr char kQuestM3[] = "q_rf_ashwheat";       // M3: its turn-in opens the Ember Tower
inline constexpr char kQuestM4[] = "q_rf_marshal";        // M4: the story boss quest
inline constexpr char kQuestM5[] = "q_rf_last_toll";      // M5: chapter complete; sends the herb_garden wing's ally

// Chapter 1 content ids only content_ch1_test.cpp, playthrough_test.cpp and this header may spell as string literals
// (content_ch1_test.cpp "content isolation"): the Chapter 1 content ids (chapter1.md "Id map": quests, monsters, hunts,
// beats, lore, achievements, quest items and targets, the Captain's tree's quest nodes) and the Chapter 1 NPCs, which
// other files reach through the handles above. Generic role keys kept by every camp (`blacksmith`, `merchant`, `stash`)
// and the zone id are not listed. Package 1B part 1 (world data) put the remake's monsters, hunts, lore, hidden area,
// story decorations, achievements, NPCs and the boss intro here; part 2 (narrative) the quests, their targets, the
// beats and the tree's quest nodes.
inline constexpr std::array<std::string_view, 73> kPinnedIds = {
    // quests (chapter1.md Id map: M1-M5, S1-S6)
    "q_rf_burial_detail", "q_rf_gnawed_field", "q_rf_ashwheat", "q_rf_marshal", "q_rf_last_toll", "q_rf_black_blood",
    "q_rf_barrowmoss", "q_rf_tag_thief", "q_rf_oathless", "q_rf_lanternwisp", "q_rf_hallowed_ground",
    // NPCs (role keys kept, chapter1.md 3; the two new ones)
    "quest_elder", "plains_herbalist", "plains_wanderer", "knell_chaplain", "crow_trader",
    // monsters, the mini-boss, hunts (monsters.md 2)
    "breachclot", "risen_footman", "gravegnawer", "cinderbloat", "risen_arbalest", "oath_zealot", "deserter",
    "deserter_drummer", "risen_shieldbearer", "dead_standard_bearer", "bonegorger", "valdras", "miniboss_corpseherd",
    "hunt_hungerbell", "hunt_dreck", "hunt_harke",
    // quest objective targets and items
    "mat_breach_ichor", "mat_barrowmoss", "mat_tag_pouch", "zone_rf_ashwheat", "defend_rf_boneyard_gate",
    "escort_rf_bern", "clue_rf_tag_cord", "clue_rf_tag_cloth", "clue_rf_tag_ashes", "clue_rf_lantern_1",
    "clue_rf_lantern_2", "clue_rf_lantern_3", "clue_rf_lantern_4",
    // story beats (chapter1.md 8)
    "cs_rf_brand", "cs_rf_whisper", "cs_rf_herd", "cs_rf_marshal_falls", "cs_rf_siege", "cs_rf_finale",
    "cs_boss_valdras",
    // lore and echoes, hidden area, story decorations, the sealed Pit (chapter1.md 12, 17)
    "lore_rf_01", "lore_rf_02", "lore_rf_03", "lore_rf_04", "lore_rf_echo_01", "lore_rf_echo_02", "lore_rf_echo_03",
    "hidden_rf_paymaster", "story_rf_belfry", "story_rf_red_banner", "story_rf_penitents_altar", "entrance_rf_pit",
    "sub_rf_pit",
    // achievements (chapter1.md 14)
    "ach_kill_breachclot", "ach_kill_valdras",
    // the Captain's dialogue tree: quest nodes (chapter1.md 4)
    "accept_burial", "gnawers_first", "accept_gnawers", "other_work", "accept_ichor", "chaplain_pointer",
};

}  // namespace ch1

// ---------------------------------------------------------------------------------------------------------------------
// Synthetic monster fixtures. Stats frozen at T0 from today's Chapter 1 roster so every combat / AI vector keeps its
// value (content_ch1_test.cpp checks that provenance on today's data); ids are synthetic, so no
// content change moves them. The sprite keys stay the shipped art ids: attack contact beats are read from the asset
// manifest by sprite key (MonsterArtGameId), and those are art, not chapter content.
// ---------------------------------------------------------------------------------------------------------------------
namespace fixture {

inline MonsterDef MakeFixtureMonster(const char* id, int32_t level, double hp, double damage, double defense,
                                     double speed, double aggroRange, double attackRange, double attackSpeedMs,
                                     double expReward, double goldMin, double goldMax, const char* spriteKey,
                                     AnimRig rig, bool elite, bool isMiniBoss) {
  MonsterDef d;
  d.id = id;
  d.name = id;
  d.level = level;
  d.hp = hp;
  d.damage = damage;
  d.defense = defense;
  d.speed = speed;
  d.aggroRange = aggroRange;
  d.attackRange = attackRange;
  d.attackSpeedMs = attackSpeedMs;
  d.expReward = expReward;
  d.goldMin = goldMin;
  d.goldMax = goldMax;
  d.spriteKey = spriteKey;
  d.elite = elite;
  d.isMiniBoss = isMiniBoss;
  d.animCategory = rig;
  d.isRanged = attackRange > 2.5;  // the exporter's derived rule (monsters 1.1, vector 11)
  d.projectileColor = 0xcc44cc;     // the default bolt colour
  d.nameKey = std::string("data.monster.") + id;
  d.sources.push_back(MonsterSource::Zone);  // not `= {Zone}`: GCC 13 + sanitizers warn on the 1-element memmove
  return d;
}

// A regular melee humanoid: level 3, 55 HP, 8 damage, 4 defense, aggro 5, reach 1.5, a swing every 1200 ms.
inline const MonsterDef& Grunt() {
  static const MonsterDef d = MakeFixtureMonster("fx_grunt", 3, 55, 8, 4, 55, 5, 1.5, 1200, 18, 3, 6, "monster_goblin",
                                                 AnimRig::Humanoid, false, false);
  return d;
}

// An elite melee humanoid that survives a hit: level 5, 160 HP, 14 damage, 8 defense, aggro 6, reach 1.8.
inline const MonsterDef& Brute() {
  static const MonsterDef d = MakeFixtureMonster("fx_brute", 5, 160, 14, 8, 50, 6, 1.8, 1000, 55, 8, 15,
                                                 "monster_goblin_chief", AnimRig::Humanoid, true, false);
  return d;
}

// A regular slime-rig melee monster: level 1, 30 HP, 5 damage, 2 defense, aggro 4, reach 1.2.
inline const MonsterDef& Ooze() {
  static const MonsterDef d = MakeFixtureMonster("fx_ooze", 1, 30, 5, 2, 40, 4, 1.2, 1500, 12, 2, 4, "monster_slime",
                                                 AnimRig::Slime, false, false);
  return d;
}

// An elite ranged mini-boss caster: level 6, 280 HP, 16 damage, reach 3.0 (ranged: a bolt at contact).
inline const MonsterDef& Caster() {
  static const MonsterDef d = MakeFixtureMonster("fx_caster", 6, 280, 16, 10, 45, 7, 3.0, 1300, 90, 15, 30,
                                                 "monster_goblin_shaman", AnimRig::Humanoid, true, true);
  return d;
}

}  // namespace fixture

}  // namespace abyss::test

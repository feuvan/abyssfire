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

inline constexpr char kStoryBoss[] = "goblin_chief";  // monster_ai storyBoss (M7); audio bossMusic scores it boss_ch1

// Main-chain slots (chapter1.md "Id map": the remake keeps each slot's role and order).
inline constexpr char kQuestM1[] = "q_kill_slimes";          // M1: the first main quest
inline constexpr char kQuestM2[] = "q_kill_goblins";         // M2
inline constexpr char kQuestM3[] = "q_explore_goblin_camp";  // M3: its turn-in opens the Ember Tower
inline constexpr char kQuestM4[] = "q_find_goblin_chief";    // M4: the story boss quest
inline constexpr char kQuestM5[] = "q_secure_plains";        // M5: chapter complete; sends the herb_garden wing's ally

// Chapter 1 content ids only content_ch1_test.cpp, playthrough_test.cpp and this header may spell as string literals
// (content_ch1_test.cpp "content isolation"): the ids the content switch renames or removes (chapter1.md "Id map":
// quests, monsters, hunts, beats, lore, achievements, quest items and targets, the elder tree's quest nodes) and the
// Chapter 1 NPCs, which other files reach through the handles above. Generic role keys kept by every camp
// (`blacksmith`, `merchant`, `stash`) and the zone id are not listed. C1 replaces the list with the remake's ids.
inline constexpr std::array<std::string_view, 55> kPinnedIds = {
    // quests (chapter1.md Id map: M1-M5, S1-S6)
    "q_kill_slimes", "q_kill_goblins", "q_explore_goblin_camp", "q_find_goblin_chief", "q_secure_plains",
    "q_collect_slime_gel", "q_herb_gathering", "q_lost_pendant", "q_bandit_trouble", "q_pet_sprite_friend",
    "q_escort_merchant_plains",
    // NPCs
    "quest_elder", "plains_herbalist", "plains_wanderer",
    // monsters, the mini-boss, hunts
    "slime_green", "goblin", "goblin_chief", "miniboss_goblin_shaman", "hunt_pendant_thief", "hunt_redcap_gruk",
    // quest objective targets and items
    "mat_slime_gel", "mat_herb", "mat_pendant", "zone_goblin_camp", "zone_east_plains", "zone_south_plains",
    "escort_merchant", "clue_pendant_fence", "clue_pendant_cloth", "clue_pendant_ashes", "clue_sprite_1",
    "clue_sprite_2", "clue_sprite_3", "clue_sprite_4",
    // story beats
    "cs_ep_mark", "cs_ep_whisper", "cs_ep_finale", "cs_boss_goblin_chief",
    // lore, hidden area, story decorations
    "lore_ep_01", "lore_ep_02", "lore_ep_03", "lore_ep_04", "hidden_ep_elven_cache", "story_ep_ruined_tower",
    "story_ep_goblin_totem", "story_ep_ancient_well",
    // achievements
    "ach_kill_slime", "ach_kill_goblin_chief",
    // the elder's dialogue tree: quest nodes
    "accept_slimes", "goblin_first", "accept_goblins", "goblin_info", "other_quests", "accept_gel", "accept_herbs",
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

// Save area (world owner; per-section readers belong to each area): migrations, parse, serialise, position fallback.
// Spec: save-ui-input.md 3.2 (schema / key order), 3.3 (v1 -> v2 -> v3 migrations, refuse newer versions, load
// normalisations), 3.7 (findNearestWalkablePosition vectors); DECISIONS U2 (v4 fields), I8 (item field names).
// Web suites ported: SaveMigration.test.ts (migrateV1toV2, CURRENT_SAVE_VERSION, findNearestWalkablePosition,
// integration scenarios) and SpiritSaveMigration.test.ts. JSON fixtures write the web's Chinese strings as \u escapes
// (core sources and tests are ASCII-only).
#include <cmath>
#include <functional>
#include <string>
#include <vector>

#include "TestUtil.h"
#include "abyss/base/Json.h"
#include "abyss/data/DataStore.h"
#include "abyss/save/SaveIO.h"
#include "abyss/sim/GameSim.h"
#include "doctest/doctest.h"

using namespace abyss;

namespace {
// Border walls, everything else walkable; `blocked` adds unwalkable tiles.
std::function<bool(int32_t, int32_t)> BorderMap(int32_t cols, int32_t rows, std::vector<TilePos> blocked = {}) {
  return [=](int32_t c, int32_t r) {
    if (c <= 0 || r <= 0 || c >= cols - 1 || r >= rows - 1) return false;
    for (const TilePos& b : blocked) {
      if (b.col == c && b.row == r) return false;
    }
    return true;
  };
}

// makeItem (SaveMigration.test.ts): a magic iron sword ("\u94c1\u5251") with one str affix.
std::string SaveTestItem(const std::string& uid, const std::string& extra = "", bool sockets = true) {
  std::string s = R"({"uid":")" + uid +
                  R"(","baseId":"sword_01","name":"\u94c1\u5251","quality":"magic","level":5,)"
                  R"("affixes":[{"affixId":"str_1","name":"\u529b\u91cf","stat":"str","value":3}],)";
  if (sockets) s += R"("sockets":[],)";
  s += R"("identified":true,"quantity":1,"stats":{"str":3})";
  s += extra;
  s += "}";
  return s;
}

// makeV1Save (SaveMigration.test.ts): a v0.10.0 save without the v2 fields. `overrides` are appended members
// (a later duplicate key replaces the earlier one in JS object literals; here the fixture simply omits it).
std::string SaveTestV1(const std::string& inventory = "", const std::string& equipment = "",
                       const std::string& stash = "", const std::string& extra = "") {
  const std::string inv = inventory.empty() ? "[" + SaveTestItem("item_001") + "]" : inventory;
  const std::string eq = equipment.empty()
                             ? R"({"weapon":)" + SaveTestItem("eq_weapon") + "}"
                             : equipment;
  const std::string st = stash.empty() ? "[" + SaveTestItem("stash_001") + "]" : stash;
  return std::string(R"({"id":"test_save_v1","version":1,"timestamp":1700000000000,"classId":"warrior",)") +
         R"("player":{"level":25,"exp":50000,"gold":12000,"hp":300,"maxHp":350,"mana":100,"maxMana":120,)"
         R"("stats":{"str":15,"dex":12,"vit":14,"int":10,"spi":8,"lck":5},"freeStatPoints":5,"freeSkillPoints":2,)"
         R"("skillLevels":{"power_strike":5,"shield_bash":3,"war_stomp":2},"tileCol":40,"tileRow":40,)"
         R"("currentMap":"emerald_plains"},)"
         R"("inventory":)" + inv + R"(,"equipment":)" + eq + R"(,"stash":)" + st + "," +
         R"("quests":[{"questId":"quest_001","status":"active","objectives":[{"current":3}]}],)"
         R"("exploration":{"emerald_plains":[[true,false],[false,true]]},)"
         R"("homestead":{"buildings":{"herb_garden":2,"warehouse":1},"pets":[{"petId":"wolf","level":3,"exp":15}],)"
         R"("activePet":"wolf"},)"
         R"("achievements":{"kill:slime_green":50,"kill_master":1},)"
         R"("settings":{"autoCombat":true,"musicVolume":0.6,"sfxVolume":0.8,"autoLootMode":"magic"})" +
         extra + "}";
}

// makeV2Save: every v2 field present.
std::string SaveTestV2() {
  std::string v1 = SaveTestV1();
  v1.pop_back();  // drop the closing brace
  const std::string marker = R"("version":1)";
  v1.replace(v1.find(marker), marker.size(), R"("version":2)");
  return v1 +
         R"(,"difficulty":"nightmare","completedDifficulties":["normal"],)"
         R"("mercenary":{"type":"tank","level":10,"exp":500,"hp":200,"mana":50,"equipment":{"weapon":)" +
         SaveTestItem("merc_weapon") +
         R"(},"alive":true},)"
         R"("dialogueState":{"npc_elder":{"visitedNodes":["start"],"choicesMade":{"start":"accept_quest"}}},)"
         R"("miniBossDialogueSeen":["boss_goblin_king"],"loreCollected":["lore_001"],)"
         R"("discoveredHiddenAreas":["hidden_cave_01"]})";
}

// The SpiritSaveMigration fixture (a v2 mage).
std::string SaveTestSpiritV2(const std::string& classId = "mage", const std::string& spirit = "",
                             int version = 2, bool withDifficulty = true) {
  std::string s = R"({"id":"spirit-migration","version":)" + std::to_string(version) +
                  R"(,"timestamp":123,"classId":")" + classId +
                  R"(","player":{"level":8,"exp":42,"gold":100,"hp":80,"maxHp":100,"mana":60,"maxMana":90,)"
                  R"("stats":{"str":4,"dex":6,"vit":8,"int":20,"spi":16,"lck":5},"freeStatPoints":0,)"
                  R"("freeSkillPoints":2,"skillLevels":{"fireball":5},"tileCol":10,"tileRow":12,)"
                  R"("currentMap":"emerald_plains")";
  if (!spirit.empty()) s += R"(,"spirit":)" + spirit;
  s += R"(},"inventory":[],"equipment":{},"stash":[],"quests":[],"exploration":{},)"
       R"("homestead":{"buildings":{},"pets":[]},"achievements":{},)"
       R"("settings":{"autoCombat":false,"musicVolume":0.5,"sfxVolume":0.7,"autoLootMode":"off"})";
  if (withDifficulty) s += R"(,"difficulty":"normal","completedDifficulties":[])";
  return s + "}";
}

JsonValue SaveTestTree(const std::string& text) {
  JsonValue v;
  REQUIRE(ParseJson(text, v));
  return v;
}

JsonValue SaveTestMigrated(const std::string& text, const DataStore* data = nullptr) {
  JsonValue v = SaveTestTree(text);
  REQUIRE(MigrateRaw(v, data));
  return v;
}

bool SaveTestIsEmptyArray(const JsonValue& v) { return v.IsArray() && v.Size() == 0; }
bool SaveTestIsEmptyObject(const JsonValue& v) { return v.IsObject() && v.Size() == 0; }

std::vector<std::string> SaveTestKeys(const JsonValue& v) {
  std::vector<std::string> keys;
  for (const JsonMember& m : v.Members()) keys.push_back(m.key);
  return keys;
}

const DataStore& SaveTestData() { return test::RealData(); }

}  // namespace

TEST_SUITE("save") {
  // ===================================================================================================================
  // findNearestWalkablePosition (3.7; SaveMigration.test.ts)
  // ===================================================================================================================
  TEST_CASE("findNearestWalkablePosition vectors (save-ui-input 3.7)") {
    TilePos out;
    const std::vector<TilePos> camp1{{5, 5}};
    CHECK(FindNearestWalkablePosition(0, 0, BorderMap(20, 20), 20, 20, camp1, out));
    CHECK(out == TilePos{5, 5});
    CHECK_FALSE(FindNearestWalkablePosition(3, 3, BorderMap(20, 20), 20, 20, camp1, out));
    const std::vector<TilePos> camps2{{5, 5}, {15, 15}};
    CHECK(FindNearestWalkablePosition(14, 14, BorderMap(20, 20, {{14, 14}}), 20, 20, camps2, out));
    CHECK(out == TilePos{15, 15});
    CHECK(FindNearestWalkablePosition(200, 200, BorderMap(20, 20), 20, 20, camp1, out));
    CHECK(out == TilePos{5, 5});
    const std::vector<TilePos> camp15{{15, 15}};
    CHECK(FindNearestWalkablePosition(79, 79, BorderMap(120, 120, {{79, 79}}), 120, 120, camp15, out));
    CHECK(out == TilePos{15, 15});
    CHECK_FALSE(FindNearestWalkablePosition(1, 1, BorderMap(20, 20), 20, 20, camp1, out));
    const std::vector<TilePos> camps3{{5, 5}, {25, 25}};
    CHECK(FindNearestWalkablePosition(23, 23, BorderMap(30, 30, {{23, 23}}), 30, 30, camps3, out));
    CHECK(out == TilePos{25, 25});
    CHECK_FALSE(FindNearestWalkablePosition(0, 0, BorderMap(20, 20), 20, 20, {}, out));  // no camps
    // integration: "position on wall scenario triggers reset to camp" (top border (5,0)).
    CHECK(FindNearestWalkablePosition(5, 0, BorderMap(20, 20), 20, 20, camp1, out));
    CHECK(out == TilePos{5, 5});
    // JS Math.round: 2.5 -> 3, -0.5 -> 0 (rounded tile, unrounded distance); a tie keeps the first camp.
    CHECK(FindNearestWalkablePosition(-0.5, 1, BorderMap(20, 20), 20, 20, camp1, out));  // -0.5 rounds to col 0
    const std::vector<TilePos> tie{{4, 10}, {16, 10}};
    CHECK(FindNearestWalkablePosition(10, 10, BorderMap(20, 20, {{10, 10}}), 20, 20, tie, out));
    CHECK(out == TilePos{4, 10});
    CHECK_FALSE(FindNearestWalkablePosition(1.49, 1.49, BorderMap(20, 20), 20, 20, camp1, out));
  }

  TEST_CASE("SaveData defaults are the v4 new-game shape") {
    SaveData s;
    CHECK(s.version == kCurrentSaveVersion);
    CHECK(s.version == 4);  // web CURRENT_SAVE_VERSION 3; the port is v4 (U2)
    CHECK(s.exploration.IsObject());
    CHECK(s.abyss.unlockedTier == 1);
    CHECK(s.abyss.bestTier == 0);
    CHECK_FALSE(s.soulEcho.present);
    const SaveSlotInfo info = SummarizeSave(2, s);
    CHECK(info.slot == 2);
    CHECK(info.exists);
  }

  // ===================================================================================================================
  // migrateV1toV2 (SaveMigration.test.ts), run through MigrateRaw (v1 -> v4 chain)
  // ===================================================================================================================
  TEST_CASE("migrateV1toV2: version and the missing v2 fields get their defaults") {
    const JsonValue v = SaveTestMigrated(SaveTestV1());
    CHECK(v.Get("version").AsInt() == kCurrentSaveVersion);  // chained up to the current version
    CHECK(v.Get("difficulty").AsString() == "normal");
    CHECK(SaveTestIsEmptyArray(v.Get("completedDifficulties")));
    CHECK_FALSE(v.Has("mercenary"));  // undefined = no mercenary hired
    CHECK(SaveTestIsEmptyObject(v.Get("dialogueState")));
    CHECK(SaveTestIsEmptyArray(v.Get("loreCollected")));
    CHECK(SaveTestIsEmptyArray(v.Get("miniBossDialogueSeen")));
    CHECK(SaveTestIsEmptyArray(v.Get("discoveredHiddenAreas")));
  }

  TEST_CASE("migrateV1toV2: items without sockets get [] (inventory, equipment, stash); existing sockets kept") {
    const std::string noSock = SaveTestItem("no_sock", "", false);
    const JsonValue v = SaveTestMigrated(SaveTestV1("[" + noSock + "]", R"({"weapon":)" + SaveTestItem("eq", "", false) + "}",
                                                    "[" + SaveTestItem("st", "", false) + "]"));
    CHECK(SaveTestIsEmptyArray(v.Get("inventory").At(0).Get("sockets")));
    CHECK(SaveTestIsEmptyArray(v.Get("equipment").Get("weapon").Get("sockets")));
    CHECK(SaveTestIsEmptyArray(v.Get("stash").At(0).Get("sockets")));
    // preserves existing sockets
    const std::string gem = R"({"gemId":"ruby_1","name":"\u7ea2\u5b9d\u77f3","stat":"str","value":5,"tier":1})";
    const std::string socketed =
        R"({"uid":"socketed","baseId":"sword_01","name":"x","quality":"magic","level":5,"affixes":[],"sockets":[)" + gem +
        R"(],"identified":true,"quantity":1,"stats":{}})";
    const JsonValue w = SaveTestMigrated(SaveTestV1("[" + socketed + "]"));
    CHECK(w.Get("inventory").At(0).Get("sockets") == SaveTestTree("[" + gem + "]"));
    // "v1 save items without sockets get empty sockets array" (helmet slot, no sockets field at all)
    const std::string helm =
        R"({"uid":"old_item","baseId":"helm_01","name":"\u94c1\u76d4","quality":"normal","level":3,"affixes":[],)"
        R"("identified":true,"quantity":1,"stats":{}})";
    const JsonValue x = SaveTestMigrated(SaveTestV1("[" + helm + "]", R"({"helmet":)" + helm + "}", "[" + helm + "]"));
    CHECK(SaveTestIsEmptyArray(x.Get("inventory").At(0).Get("sockets")));
    CHECK(SaveTestIsEmptyArray(x.Get("equipment").Get("helmet").Get("sockets")));
    CHECK(SaveTestIsEmptyArray(x.Get("stash").At(0).Get("sockets")));
  }

  TEST_CASE("migrateV1toV2: existing data is preserved unchanged") {
    const JsonValue orig = SaveTestTree(SaveTestV1());
    const JsonValue v = SaveTestMigrated(SaveTestV1());
    CHECK(v.Get("player").Get("stats") == orig.Get("player").Get("stats"));
    CHECK(v.Get("player").Get("level").AsInt() == 25);
    CHECK(v.Get("player").Get("exp").AsInt() == 50000);
    CHECK(v.Get("player").Get("gold").AsInt() == 12000);
    CHECK(v.Get("player").Get("hp").AsInt() == 300);
    CHECK(v.Get("player").Get("mana").AsInt() == 100);
    CHECK(v.Get("player").Get("tileCol").AsInt() == 40);
    CHECK(v.Get("player").Get("tileRow").AsInt() == 40);
    CHECK(v.Get("inventory").Size() == 1);
    CHECK(v.Get("inventory").At(0).Get("uid").AsString() == "item_001");
    CHECK(v.Get("inventory").At(0).Get("name") == orig.Get("inventory").At(0).Get("name"));
    CHECK(v.Get("equipment").Get("weapon").Get("uid").AsString() == "eq_weapon");
    CHECK(v.Get("quests").Size() == 1);
    CHECK(v.Get("quests").At(0).Get("questId").AsString() == "quest_001");
    CHECK(v.Get("quests").At(0).Get("status").AsString() == "active");
    CHECK(v.Get("homestead").Get("buildings") == SaveTestTree(R"({"herb_garden":2,"warehouse":1})"));
    CHECK(v.Get("homestead").Get("pets").Size() == 1);
    CHECK(v.Get("homestead").Get("activePet").AsString() == "wolf");
    CHECK(v.Get("exploration") == SaveTestTree(R"({"emerald_plains":[[true,false],[false,true]]})"));
    CHECK(v.Get("achievements") == SaveTestTree(R"({"kill:slime_green":50,"kill_master":1})"));
    CHECK(v.Get("settings").Get("autoCombat").AsBool() == true);
    CHECK(v.Get("settings").Get("musicVolume").AsDouble() == 0.6);
    CHECK(v.Get("settings").Get("sfxVolume").AsDouble() == 0.8);
    CHECK(v.Get("settings").Get("autoLootMode").AsString() == "magic");
  }

  TEST_CASE("migrateV1toV2: v2 fields already present in a v1 save are kept") {
    const std::string extra =
        R"(,"difficulty":"nightmare","completedDifficulties":["normal"],)"
        R"("mercenary":{"type":"healer","level":8,"exp":300,"hp":150,"mana":80,"equipment":{},"alive":true},)"
        R"("dialogueState":{"npc_01":{"visitedNodes":["a"],"choicesMade":{}}},"loreCollected":["lore_x"])";
    const JsonValue v = SaveTestMigrated(SaveTestV1("", "", "", extra));
    CHECK(v.Get("difficulty").AsString() == "nightmare");
    CHECK(v.Get("completedDifficulties") == SaveTestTree(R"(["normal"])"));
    REQUIRE(v.Has("mercenary"));
    CHECK(v.Get("mercenary").Get("type").AsString() == "healer");
    CHECK(v.Get("dialogueState") == SaveTestTree(R"({"npc_01":{"visitedNodes":["a"],"choicesMade":{}}})"));
    CHECK(v.Get("loreCollected") == SaveTestTree(R"(["lore_x"])"));
  }

  TEST_CASE("migrateV1toV2: a minimal v1 save (no collections at all) gets every default") {
    const std::string minimal =
        R"({"id":"minimal","version":1,"timestamp":1000,"classId":"mage","player":{"level":1,"exp":0,"gold":0,)"
        R"("hp":50,"maxHp":50,"mana":30,"maxMana":30,"stats":{"str":15,"dex":12,"vit":14,"int":10,"spi":8,"lck":5},)"
        R"("freeStatPoints":0,"freeSkillPoints":0,"skillLevels":{},"tileCol":15,"tileRow":15,)"
        R"("currentMap":"emerald_plains"}})";
    const JsonValue v = SaveTestMigrated(minimal);
    CHECK(SaveTestIsEmptyArray(v.Get("inventory")));
    CHECK(SaveTestIsEmptyObject(v.Get("equipment")));
    CHECK(SaveTestIsEmptyArray(v.Get("stash")));
    CHECK(SaveTestIsEmptyArray(v.Get("quests")));
    CHECK(SaveTestIsEmptyObject(v.Get("exploration")));
    REQUIRE(v.Get("homestead").IsObject());
    CHECK(SaveTestIsEmptyObject(v.Get("homestead").Get("buildings")));
    CHECK(SaveTestIsEmptyArray(v.Get("homestead").Get("pets")));
    CHECK_FALSE(v.Get("homestead").Has("activePet"));
    CHECK(SaveTestIsEmptyObject(v.Get("achievements")));
    REQUIRE(v.Get("settings").IsObject());
    CHECK(v.Get("settings").Get("autoCombat").AsBool(true) == false);
    CHECK(v.Get("settings").Get("musicVolume").AsDouble() == 0.5);
    CHECK(v.Get("settings").Get("sfxVolume").AsDouble() == 0.7);
    CHECK(v.Get("settings").Get("autoLootMode").AsString() == "off");
    CHECK(v.Get("difficulty").AsString() == "normal");
    CHECK(SaveTestIsEmptyArray(v.Get("completedDifficulties")));
    CHECK_FALSE(v.Has("mercenary"));
    CHECK(SaveTestIsEmptyObject(v.Get("dialogueState")));
    CHECK(SaveTestIsEmptyArray(v.Get("loreCollected")));
    CHECK(SaveTestIsEmptyArray(v.Get("miniBossDialogueSeen")));
    CHECK(SaveTestIsEmptyArray(v.Get("discoveredHiddenAreas")));
  }

  TEST_CASE("migrateV1toV2: idempotent on a v2 save (nothing changes but the version chain)") {
    const JsonValue orig = SaveTestTree(SaveTestV2());
    JsonValue once = SaveTestMigrated(SaveTestV2());
    CHECK(once.Get("difficulty").AsString() == "nightmare");
    CHECK(once.Get("completedDifficulties") == SaveTestTree(R"(["normal"])"));
    CHECK(once.Get("mercenary") == orig.Get("mercenary"));
    CHECK(once.Get("mercenary").Get("type").AsString() == "tank");
    CHECK(once.Get("dialogueState") == orig.Get("dialogueState"));
    CHECK(once.Get("loreCollected") == SaveTestTree(R"(["lore_001"])"));
    JsonValue twice = once;
    REQUIRE(MigrateRaw(twice));
    CHECK(twice == once);  // a current-version tree is left untouched
  }

  TEST_CASE("migration: JS truthiness of the v1 defaults, non-object mercenary dropped, root must be an object") {
    // `if (!save.difficulty)`: "" / null / 0 / false are replaced; a truthy wrong type is kept.
    const JsonValue v = SaveTestMigrated(
        SaveTestV1("", "", "", R"(,"difficulty":"","completedDifficulties":null,"loreCollected":0,"mercenary":5)"));
    CHECK(v.Get("difficulty").AsString() == "normal");
    CHECK(SaveTestIsEmptyArray(v.Get("completedDifficulties")));
    CHECK(SaveTestIsEmptyArray(v.Get("loreCollected")));
    CHECK_FALSE(v.Has("mercenary"));  // truthy, typeof !== 'object' -> undefined
    const JsonValue w = SaveTestMigrated(SaveTestV1("", "", "", R"(,"mercenary":null)"));
    CHECK(w.Get("mercenary").IsNull());  // falsy: left alone (absent mercenary on load)
    JsonValue notObject = SaveTestTree("[1,2]");
    CHECK_FALSE(MigrateRaw(notObject));
    CHECK(notObject == SaveTestTree("[1,2]"));
    // A missing version counts as the oldest format; v4 fields are added once.
    JsonValue noVersion = SaveTestTree(R"({"player":{"level":1}})");
    REQUIRE(MigrateRaw(noVersion));
    CHECK(noVersion.Get("version").AsInt() == 4);
    CHECK(SaveTestIsEmptyArray(noVersion.Get("inventory")));
    CHECK(noVersion.Get("player").Get("spirit") == SaveTestTree(R"({"value":0,"resonanceRemainingMs":0})"));
    CHECK(noVersion.Get("itemUidCounter").AsInt() == 1);
    CHECK(noVersion.Get("potionSlots") == SaveTestTree(R"(["",""])"));
    CHECK(SaveTestIsEmptyArray(noVersion.Get("visitedZones")));
    CHECK_FALSE(noVersion.Has("hotbar"));
    CHECK_FALSE(noVersion.Has("rng"));
  }

  // ===================================================================================================================
  // migrateV2toV3 (SpiritSaveMigration.test.ts)
  // ===================================================================================================================
  TEST_CASE("migrateV2toV3: Spirit state added, preserved, sanitised and clamped to the class profile") {
    const DataStore& data = SaveTestData();
    const JsonValue empty = SaveTestMigrated(SaveTestSpiritV2(), &data);
    CHECK(empty.Get("version").AsInt() == kCurrentSaveVersion);
    CHECK(empty.Get("player").Get("spirit") == SaveTestTree(R"({"value":0,"resonanceRemainingMs":0})"));
    const JsonValue kept = SaveTestMigrated(SaveTestSpiritV2("mage", R"({"value":64,"resonanceRemainingMs":2500})"), &data);
    CHECK(kept.Get("player").Get("spirit") == SaveTestTree(R"({"value":64,"resonanceRemainingMs":2500})"));
    // NaN cannot be written in JSON: a non-number value and a negative remaining are sanitised the same way.
    const JsonValue bad = SaveTestMigrated(SaveTestSpiritV2("mage", R"({"value":"x","resonanceRemainingMs":-10})"), &data);
    CHECK(bad.Get("player").Get("spirit") == SaveTestTree(R"({"value":0,"resonanceRemainingMs":0})"));
    const JsonValue rogue = SaveTestMigrated(SaveTestSpiritV2("rogue", R"({"value":100,"resonanceRemainingMs":9000})"), &data);
    CHECK(rogue.Get("player").Get("spirit") == SaveTestTree(R"({"value":100,"resonanceRemainingMs":5500})"));
    // Value above maxValue (100) clamps; value 0 ends any resonance.
    const JsonValue over = SaveTestMigrated(SaveTestSpiritV2("warrior", R"({"value":250,"resonanceRemainingMs":100})"), &data);
    CHECK(over.Get("player").Get("spirit") == SaveTestTree(R"({"value":100,"resonanceRemainingMs":100})"));
    const JsonValue zero = SaveTestMigrated(SaveTestSpiritV2("warrior", R"({"value":0,"resonanceRemainingMs":100})"), &data);
    CHECK(zero.Get("player").Get("spirit").Get("resonanceRemainingMs").AsDouble() == 0);
    // Without data only the lower bounds apply (Spirit::Restore clamps to the profile at load).
    const JsonValue noData = SaveTestMigrated(SaveTestSpiritV2("rogue", R"({"value":100,"resonanceRemainingMs":9000})"));
    CHECK(noData.Get("player").Get("spirit").Get("resonanceRemainingMs").AsDouble() == 9000);
    // A v3 save keeps its spirit as written (the step only runs for version < 3).
    const JsonValue v3 = SaveTestMigrated(SaveTestSpiritV2("rogue", R"({"value":100,"resonanceRemainingMs":9000})", 3), &data);
    CHECK(v3.Get("player").Get("spirit").Get("resonanceRemainingMs").AsDouble() == 9000);
  }

  TEST_CASE("migrateSaveData: a legacy v1 save migrates through every version in one pass") {
    const DataStore& data = SaveTestData();
    const JsonValue v = SaveTestMigrated(SaveTestSpiritV2("mage", "", 1, false), &data);
    CHECK(v.Get("version").AsInt() == kCurrentSaveVersion);
    CHECK(v.Get("difficulty").AsString() == "normal");
    CHECK(SaveTestIsEmptyArray(v.Get("completedDifficulties")));
    CHECK(v.Get("player").Get("spirit") == SaveTestTree(R"({"value":0,"resonanceRemainingMs":0})"));
  }

  // ===================================================================================================================
  // ParseSave / SerializeSave (3.2, 3.3 normalisations, U2)
  // ===================================================================================================================
  TEST_CASE("ParseSave: errors (not JSON, not an object, version too new, no player) leave the output untouched") {
    SaveData s;
    s.id = "keep";
    std::string err;
    CHECK(ParseSave("{not json", s, &err) == SaveError::ParseFailed);
    CHECK_FALSE(err.empty());
    CHECK(ParseSave("[1,2,3]", s, &err) == SaveError::NotAnObject);
    CHECK(ParseSave(R"({"version":5,"player":{}})", s, &err) == SaveError::VersionTooNew);
    CHECK(ParseSave(R"({"version":4.5,"player":{}})", s, &err) == SaveError::VersionTooNew);
    CHECK(ParseSave(R"({"version":3,"player":7})", s, &err) == SaveError::Invalid);
    CHECK(s.id == "keep");
    // Too deep: refused by the parser (kMaxJsonDepth), not a stack overflow.
    std::string deep = R"({"player":{},"x":)";
    deep += std::string(kMaxJsonDepth + 8, '[');
    deep += std::string(kMaxJsonDepth + 8, ']');
    deep += "}";
    CHECK(ParseSave(deep, s, &err) == SaveError::ParseFailed);
  }

  TEST_CASE("ParseSave: a v1 save loads with the 3.3 normalisations") {
    SaveData s;
    std::string err;
    const std::string unidentified = std::string(R"({"uid":"u2","baseId":"sword_01","name":"x","quality":"rare",)") +
                                     R"("level":3,"affixes":[],"identified":false,"quantity":1,"stats":{}})";
    REQUIRE(ParseSave(SaveTestV1("[" + unidentified + "]", "", "",
                                 R"(,"soulEcho":{"mapId":"emerald_plains","col":"a","row":3,"gold":5,"exp":1},)"
                                 R"("abyss":{"unlockedTier":0,"bestTier":-3,"bestTimeMs":81234.5})"),
                      s, &err) == SaveError::None);
    CHECK(err.empty());
    CHECK(s.version == kCurrentSaveVersion);
    CHECK(s.id == "test_save_v1");
    CHECK(s.timestamp == 1700000000000);
    CHECK(s.classId == ClassId::Warrior);
    CHECK(s.player.level == 25);
    CHECK(s.player.gold == 12000);
    CHECK(s.player.tileCol == 40);
    CHECK(s.player.currentMap == "emerald_plains");
    CHECK(s.player.hasSpirit);  // v2 -> v3 added it
    REQUIRE(s.inventory.size() == 1);
    CHECK(s.inventory[0].identified);  // every item identified on load
    CHECK(s.inventory[0].quality == ItemQuality::Rare);
    REQUIRE(s.equipment[EnumIndex(EquipSlot::Weapon)].has_value());
    CHECK(s.equipment[EnumIndex(EquipSlot::Weapon)]->uid == "eq_weapon");
    REQUIRE(s.stash.size() == 1);
    REQUIRE(s.quests.size() == 1);
    CHECK(s.exploration == SaveTestTree(R"({"emerald_plains":[[true,false],[false,true]]})"));
    CHECK(s.homestead.hasLegacyPets);  // migratePetSave reads the legacy list
    CHECK_FALSE(s.pets.present);
    CHECK(s.settings.autoCombat);
    CHECK(s.settings.autoLootMode == AutoLootMode::Magic);
    CHECK(s.difficulty == Difficulty::Normal);
    CHECK(s.completedDifficulties.empty());
    CHECK_FALSE(s.hasMercenary);
    // storySeen missing -> ['prologue'] (veterans skip the prologue)
    CHECK(s.hasStorySeen);
    CHECK(s.storySeen == std::vector<std::string>{"prologue"});
    CHECK_FALSE(s.soulEcho.present);  // col not finite -> dropped
    CHECK(s.abyss.unlockedTier == 1);  // max(1, 0)
    CHECK(s.abyss.bestTier == 0);      // max(0, -3)
    CHECK(s.abyss.hasBestTime);
    CHECK(s.abyss.bestTimeMs == 81234.5);
    // v4 defaults
    CHECK(s.slot == 0);
    CHECK_FALSE(s.hasHotbar);
    CHECK(s.potionSlots == std::array<std::string, 2>{"", ""});
    CHECK(s.playTimeMs == 0);
    CHECK_FALSE(s.rng.present);
    CHECK(s.dialogueOnce.empty());
    CHECK(s.hiddenRewardsClaimed.empty());
    CHECK(s.itemUidCounter == 1);
    CHECK(s.visitedZones.empty());
  }

  TEST_CASE("ParseSave: v2 fields, unknown class / difficulty fall back, completed list filtered") {
    SaveData s;
    REQUIRE(ParseSave(SaveTestV2(), s) == SaveError::None);
    CHECK(s.difficulty == Difficulty::Nightmare);
    CHECK(s.completedDifficulties == std::vector<Difficulty>{Difficulty::Normal});
    CHECK(s.hasMercenary);
    CHECK(s.mercenary.Get("type").AsString() == "tank");
    REQUIRE(s.dialogueState.size() == 1);
    CHECK(s.dialogueState[0].npcId == "npc_elder");
    CHECK(s.miniBossDialogueSeen == std::vector<std::string>{"boss_goblin_king"});
    CHECK(s.loreCollected == std::vector<std::string>{"lore_001"});
    CHECK(s.discoveredHiddenAreas == std::vector<std::string>{"hidden_cave_01"});
    SaveData t;
    REQUIRE(ParseSave(SaveTestSpiritV2("paladin") , t) == SaveError::None);
    CHECK(t.classId == ClassId::Warrior);  // unknown class -> warrior (ZoneScene.ts:427)
    std::string text = SaveTestSpiritV2("mage", "", 2, false);
    text.pop_back();
    text += R"(,"difficulty":"ultra","completedDifficulties":["hell","x","normal","hell"],"storySeen":["a","b"]})";
    SaveData u;
    REQUIRE(ParseSave(text, u) == SaveError::None);
    CHECK(u.difficulty == Difficulty::Normal);
    CHECK(u.completedDifficulties == std::vector<Difficulty>{Difficulty::Hell, Difficulty::Normal});
    CHECK(u.storySeen == std::vector<std::string>{"a", "b"});
  }

  TEST_CASE("SerializeSave: 3.2 key order then the v4 fields; parse -> serialise is a fixed point") {
    SaveData s;
    REQUIRE(ParseSave(SaveTestV2(), s) == SaveError::None);
    s.hasHotbar = true;
    s.hotbar = {"slash", "", "whirlwind", "", "", ""};
    s.potionSlots = {"hp_potion", ""};
    s.playTimeMs = 123456.5;
    s.rng.present = true;
    for (size_t i = 0; i < kRngStreamCount; ++i) {
      s.rng.streams[i].s = {static_cast<uint32_t>(i + 1), 0xFFFFFFFFu, 0, static_cast<uint32_t>(i * 7)};
    }
    s.dialogueOnce = {"elder|n1|0"};
    s.hiddenRewardsClaimed = {"hidden_ep_elven_cache#1"};
    s.itemUidCounter = 42;
    s.visitedZones = {"emerald_plains"};
    s.soulEcho.present = true;
    s.soulEcho.mapId = "emerald_plains";
    s.soulEcho.col = 40.25;
    s.soulEcho.row = 41.5;
    s.soulEcho.gold = 120;
    s.slot = 2;
    const std::string text = SerializeSave(s);
    const JsonValue tree = SaveTestTree(text);
    const std::vector<std::string> expected{
        "id", "version", "timestamp", "classId", "player", "inventory", "equipment", "stash", "quests", "exploration",
        "homestead", "achievements", "settings", "difficulty", "completedDifficulties", "mercenary", "dialogueState",
        "miniBossDialogueSeen", "loreCollected", "discoveredHiddenAreas", "storySeen", "soulEcho", "abyss", "slot",
        "hotbar", "potionSlots", "playTimeMs", "rng", "dialogueOnce", "hiddenRewardsClaimed", "itemUidCounter",
        "visitedZones"};
    CHECK(SaveTestKeys(tree) == expected);
    CHECK(tree.Get("version").AsInt() == 4);
    CHECK(tree.Get("hotbar") == SaveTestTree(R"(["slash",null,"whirlwind",null,null,null])"));
    CHECK(tree.Get("rng").Get("combat") == SaveTestTree("[1,4294967295,0,0]"));
    CHECK(SaveTestKeys(tree.Get("rng")) ==
          std::vector<std::string>{"combat", "loot", "ai", "world", "events", "quests", "pets"});
    CHECK(tree.Get("abyss") == SaveTestTree(R"({"unlockedTier":1,"bestTier":0})"));
    CHECK(SaveTestKeys(tree.Get("player")) ==
          std::vector<std::string>{"level", "exp", "gold", "hp", "maxHp", "mana", "maxMana", "stats", "freeStatPoints",
                                   "freeSkillPoints", "skillLevels", "spirit", "tileCol", "tileRow", "currentMap"});
    // Round trip: parse(serialise(s)) serialises to the same bytes.
    SaveData back;
    std::string err;
    REQUIRE(ParseSave(text, back, &err) == SaveError::None);
    CHECK(SerializeSave(back) == text);
    CHECK(back.hasHotbar);
    CHECK(back.hotbar == s.hotbar);
    CHECK(back.potionSlots == s.potionSlots);
    CHECK(back.playTimeMs == s.playTimeMs);
    REQUIRE(back.rng.present);
    CHECK(back.rng.streams == s.rng.streams);
    CHECK(back.dialogueOnce == s.dialogueOnce);
    CHECK(back.hiddenRewardsClaimed == s.hiddenRewardsClaimed);
    CHECK(back.itemUidCounter == 42);
    CHECK(back.visitedZones == s.visitedZones);
    CHECK(back.slot == 2);
    REQUIRE(back.soulEcho.present);
    CHECK(back.soulEcho.col == 40.25);
    CHECK(back.mercenary == s.mercenary);
    // Pretty output parses to the same tree.
    CHECK(SaveTestTree(SerializeSave(s, true)) == tree);
  }

  TEST_CASE("ParseSave v4: malformed rng streams fall back to reseeding; bad slot / play time default") {
    std::string base = SaveTestSpiritV2();
    base.pop_back();
    auto parse = [&base](const std::string& extra) {
      SaveData s;
      REQUIRE(ParseSave(base + extra + "}", s) == SaveError::None);
      return s;
    };
    const std::string good = R"("combat":[1,2,3,4],"loot":[1,2,3,4],"ai":[1,2,3,4],"world":[1,2,3,4],)"
                             R"("events":[1,2,3,4],"quests":[1,2,3,4],"pets":[1,2,3,4])";
    CHECK(parse(R"(,"rng":{)" + good + "}").rng.present);
    CHECK_FALSE(parse(R"(,"rng":{"combat":[1,2,3,4]})").rng.present);                         // streams missing
    std::string zero = good;
    zero.replace(zero.find("[1,2,3,4]"), 9, "[0,0,0,0]");
    CHECK_FALSE(parse(R"(,"rng":{)" + zero + "}").rng.present);  // all-zero xoshiro state
    std::string big = good;
    big.replace(big.find("[1,2,3,4]"), 9, "[4294967296,2,3,4]");
    CHECK_FALSE(parse(R"(,"rng":{)" + big + "}").rng.present);
    const SaveData odd = parse(R"(,"slot":-4,"playTimeMs":-10,"itemUidCounter":0,"hotbar":[1,"slash"])");
    CHECK(odd.slot == 0);
    CHECK(odd.playTimeMs == 0);
    CHECK(odd.itemUidCounter == 1);
    CHECK(odd.hasHotbar);
    CHECK(odd.hotbar[0].empty());
    CHECK(odd.hotbar[1] == "slash");
  }

  TEST_CASE("SummarizeSave: the Continue card and the difficulty selector rule (save-ui-input 1.2)") {
    SaveData s;
    REQUIRE(ParseSave(SaveTestV2(), s) == SaveError::None);
    s.playTimeMs = 5000;
    const SaveSlotInfo info = SummarizeSave(1, s);
    CHECK(info.slot == 1);
    CHECK(info.level == 25);
    CHECK(info.mapId == "emerald_plains");
    CHECK(info.classId == ClassId::Warrior);
    CHECK(info.difficulty == Difficulty::Nightmare);
    CHECK(info.completedDifficulties == std::vector<Difficulty>{Difficulty::Normal});
    CHECK(info.showDifficultySelector);
    CHECK(info.playTimeMs == 5000);
  }

  // ===================================================================================================================
  // GameSim integration: BuildSave -> SerializeSave -> ParseSave -> ApplySave (U2)
  // ===================================================================================================================
  TEST_CASE("GameSim: a new game's save round-trips through the v4 JSON") {
    const DataStore& data = SaveTestData();
    auto sim = GameSim::Create(data, SimConfig{});
    REQUIRE(sim != nullptr);
    REQUIRE(sim->NewGame(ClassId::Mage, Difficulty::Normal, 99, 1));
    for (int i = 0; i < 10; ++i) sim->Step();
    const std::string text = sim->SaveGame(1760000000000);
    REQUIRE_FALSE(text.empty());
    SaveData parsed;
    std::string err;
    REQUIRE(ParseSave(text, parsed, &err, &data) == SaveError::None);
    CHECK(parsed.classId == ClassId::Mage);
    CHECK(parsed.timestamp == 1760000000000);
    CHECK(parsed.slot == 1);
    CHECK(parsed.player.currentMap == data.World().defaultMap);
    CHECK(parsed.rng.present);
    CHECK(parsed.hasHotbar);
    CHECK(parsed.pets.present);
    CHECK(SerializeSave(parsed) == text);  // what the sim wrote is a fixed point of parse -> serialise
    auto sim2 = GameSim::Create(data, SimConfig{});
    REQUIRE(sim2->LoadGame(text, &err) == SaveError::None);
    // load -> save writes the same document, except the streams the zone entry draws from again after the restore
    // (spawn placement on RngStream::Ai).
    auto withoutAiStream = [](const std::string& json) {
      JsonValue v = SaveTestTree(json);
      JsonValue* rng = v.FindMutable("rng");
      REQUIRE(rng != nullptr);
      rng->Remove("ai");
      return v;
    };
    const std::string reloaded = sim2->SaveGame(1760000000000);
    CHECK(withoutAiStream(reloaded) == withoutAiStream(text));
    // A save from a newer build is refused and leaves the session alone.
    std::string newer = text;
    newer.replace(newer.find(R"("version":4)"), 11, R"("version":9)");
    CHECK(sim2->LoadGame(newer, &err) == SaveError::VersionTooNew);
    CHECK(sim2->SaveGame(1760000000000) == reloaded);
  }
}

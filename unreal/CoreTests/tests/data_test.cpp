// Data layer tests: load the real unreal/Data/*.json and spot-check every table against the port specs.
#include <algorithm>
#include <initializer_list>
#include <string>
#include <vector>

#include "TestUtil.h"
#include "abyss/base/Json.h"
#include "abyss/base/StrUtil.h"
#include "abyss/data/DataStore.h"
#include "doctest/doctest.h"

using namespace abyss;

TEST_SUITE("data") {

TEST_CASE("DataStore loads every exported table without errors") {
  DataStore store;
  DataLoadReport report;
  const bool ok = store.LoadAll(
      [](std::string_view name, std::string& out) {
        out = test::ReadFile(test::DataDir() + "/" + std::string(name));
        return !out.empty();
      },
      report);
  INFO(report.Summary(80));
  CHECK(ok);
  CHECK(report.errors.empty());
  CHECK(store.IsFinalized());
  CHECK(report.loadedFiles.size() >= RequiredDataFiles().size());
  CHECK(store.RawTable("terrain_styles.json") != nullptr);
  CHECK(store.RawTable("music.json") != nullptr);
  CHECK(store.RawTable("classes.json") == nullptr);
}

TEST_CASE("classes, skills, formulas, spirit") {
  const DataStore& d = test::RealData();
  REQUIRE(d.IsFinalized());
  const ClassTables& c = d.Classes();
  REQUIRE(c.classes.size() == 3);
  const ClassDef* w = c.Find(ClassId::Warrior);
  REQUIRE(w);
  CHECK(w->baseStats == PrimaryStats{12, 8, 10, 5, 5, 5});
  CHECK(w->skills.size() == 15);
  const SkillDef* slash = d.FindSkill("slash");
  REQUIRE(slash);
  CHECK(slash->tier == 1);
  CHECK(slash->manaCost == 8);
  CHECK(slash->cooldownMs == 2000);
  CHECK(slash->range == 1.5);
  CHECK(slash->damageMultiplier == 1.5);
  CHECK(slash->scaling.damagePerLevel == 0.18);
  CHECK(slash->scaling.Has(ScalingField::DamagePerLevel));          // stated in the JSON
  CHECK(slash->scaling.buffValuePerLevel == 0.02);                  // default filled from skill_rules
  CHECK_FALSE(slash->scaling.Has(ScalingField::BuffValuePerLevel));  // explicit presence bit, no NaN sentinel
  // C4 persistent ground effects carry their lifetime / tick count / trigger as data (never a core constant).
  const SkillDef* wall = d.FindSkill("fire_wall");
  REQUIRE(wall);
  CHECK(wall->port.persistentGround);
  CHECK(wall->port.groundDurationMs == 3000);
  CHECK(wall->port.groundTicks == 6);
  CHECK(wall->port.groundTrigger == GroundTrigger::Periodic);
  for (const char* id : {"explosive_trap", "slow_trap", "chain_trap"}) {
    const SkillDef* trap = d.FindSkill(id);
    REQUIRE(trap);
    CHECK(trap->port.persistentGround);
    CHECK(trap->port.groundTrigger == GroundTrigger::Armed);
    CHECK(trap->port.groundTicks == 1);
    CHECK(trap->port.groundDurationMs > 0);
  }
  CHECK(d.FindSkill("arrow_rain")->port.groundDurationMs == 3000);  // "持续3秒" in the web description
  CHECK_FALSE(slash->port.persistentGround);
  CHECK(slash->port.groundTicks == 0);
  CHECK(slash->execKind == SkillExecKind::Single);
  CHECK(slash->animKind == SkillAnimKind::Attack);
  REQUIRE(slash->synergies.size() == 1);
  CHECK(slash->synergies[0].skillId == "whirlwind");
  const SkillDef* fireball = d.FindSkill("fireball");
  REQUIRE(fireball);
  CHECK(fireball->hasProjectile);
  CHECK(fireball->projectile.minMs == 300);
  REQUIRE(fireball->statusRules.size() == 1);
  CHECK(fireball->statusRules[0].status == StatusType::Burn);
  CHECK(fireball->statusRules[0].hasChance);
  CHECK(fireball->statusRules[0].chance == 0.6);
  CHECK(fireball->statusRules[0].valueKind == StatusValueKind::Damage);
  const SkillDef* slowTrap = d.FindSkill("slow_trap");
  REQUIRE(slowTrap);
  CHECK(slowTrap->execKind == SkillExecKind::SlowTrap);
  REQUIRE(slowTrap->statusRules.size() == 1);
  CHECK_FALSE(slowTrap->statusRules[0].hasDuration);
  CHECK(slowTrap->statusRules[0].valueKind == StatusValueKind::BuffPercent);
  const SkillDef* lifeRegen = d.FindSkill("life_regen");
  REQUIRE(lifeRegen);
  CHECK(lifeRegen->passive);
  CHECK(lifeRegen->passiveRule.kind == PassiveRuleKind::Regen);
  CHECK(lifeRegen->passiveRule.hpPerSecondPerLevel == 2);
  const SkillDef* teleport = d.FindSkill("teleport");
  REQUIRE(teleport);
  CHECK(teleport->port.hasTeleport);
  CHECK(teleport->port.teleportMaxRangeTiles == 8);
  CHECK(d.FindSkill("charge")->port.hasDash);
  CHECK(d.FindSkill("combustion")->hasBonusVsStatus);
  CHECK(d.FindSkill("piercing_arrow")->hasLineTarget);

  CHECK(c.trees.size() == 9);
  CHECK(c.skillRules.RequiredPlayerLevel(2) == 6);
  CHECK(c.skillRules.RequiredTreePoints(3) == 9);
  CHECK(c.skillRules.RequiredPlayerLevel(4) == 19);  // fallback 1 + 3*6
  CHECK(c.skillRules.loadoutSize == 6);
  CHECK(c.skillRules.groundAoeSkills.size() == 6);
  CHECK(c.skillRules.cooldownFloorMs == 500);

  CHECK(c.formulas.ExpToNext(1) == 28);
  CHECK(c.formulas.ExpToNext(20) == 1700);
  CHECK(c.formulas.maxHpPerVit == 10);
  CHECK(c.formulas.critCapPercent == 75);
  CHECK(c.buffCaps.Apply(BuffStat::DamageReduction, 2.0) == 0.9);
  CHECK(c.buffCaps.Apply(BuffStat::DamageAmplify, 2.0) == 2.0);
  CHECK(c.spirit.For(ClassId::Warrior).hitGain == 8);
  CHECK(c.spirit.For(ClassId::Rogue).resonanceDurationMs == 5500);
  CHECK(c.spirit.spiGainFactor == 0.015);
  CHECK(c.statusRules.TickInterval(StatusType::Burn) == 1000);
  CHECK(c.statusRules.Stacking(StatusType::Poison) == StatusStacking::RefreshKeepStronger);
  CHECK(c.statusRules.Diminishes(StatusType::Stun));
  CHECK_FALSE(c.statusRules.Diminishes(StatusType::Burn));
}

TEST_CASE("combat tables") {
  const CombatTables& c = test::RealData().Combat();
  CHECK(c.input.inputBufferMs == 180);
  CHECK(c.input.dodgeDistanceTiles[EnumIndex(ClassId::Rogue)] == 2.6);
  CHECK(c.anim.Contact(AnimRig::Warrior).contactMsAtSpeed1 == 308);
  CHECK(c.anim.Contact(AnimRig::Humanoid).contactMsAtSpeed1 == 250);
  CHECK(c.anim.Preset(AnimRig::Mage).castDuration == 500);
  CHECK(c.anim.TransitionMs("idle", "walk") == 90);
  CHECK(c.anim.TransitionMs("nope", "x") == 80);
  CHECK(c.hitFeedback.Profile(HitWeight::Normal).targetStopMs == 60);
  CHECK(c.hitFeedback.Profile(HitWeight::Kill).sparks == 14);
  CHECK(c.hitFeedback.heavyRatio == 0.25);
  CHECK(c.eliteAffixes.order.size() == 7);
  CHECK(c.eliteAffixes.Def(EliteAffixType::ExtraStrong).damageMult == 1.35);
  int32_t mn = 0, mx = 0;
  c.eliteAffixes.CountFor("abyss_rift", mn, mx);
  CHECK(mn == 2);
  c.eliteAffixes.CountFor("unknown_zone", mn, mx);
  CHECK(mn == 1);
  CHECK(c.difficulty.Def(Difficulty::Nightmare).hpMul == 1.5);
  CHECK(c.difficulty.Def(Difficulty::Hell).lootQualityBonus == 12);
  CHECK(c.soulEcho.minLevel == 5);
  CHECK(c.soulEcho.goldShare[EnumIndex(Difficulty::Hell)] == 0.2);
}

TEST_CASE("item tables") {
  const ItemTables& it = test::RealData().Items();
  CHECK(it.bases.size() == 97);
  const ItemBaseDef* sword = it.FindBase("w_rusty_sword");
  REQUIRE(sword);
  CHECK(sword->IsEquipment());
  CHECK(sword->slot == EquipSlot::Weapon);
  CHECK(sword->hasBaseDamage);
  CHECK(sword->weaponType == WeaponType::Sword);
  const ItemBaseDef* hp = it.FindBase("c_hp_potion_s");
  REQUIRE(hp);
  CHECK(hp->consumableEffect == ConsumableEffect::Heal);
  CHECK(hp->consumableValue == 50);
  CHECK(hp->isGroundPotion);
  CHECK(hp->groundPotionAmount == 50);
  const ItemBaseDef* ruby = it.FindBase("g_ruby_1");
  REQUIRE(ruby);
  CHECK(ruby->isGem);
  CHECK(ruby->gemStat == Stat::Str);
  CHECK(it.FindBase("c_ley_fruit")->consumableEffect == ConsumableEffect::None);
  CHECK(it.prefixes.size() == 29);
  CHECK(it.suffixes.size() == 39);
  CHECK(it.FindAffix("pre_sharp")->kind == AffixKind::Prefix);
  CHECK(it.sets.size() == 6);
  CHECK(it.legendaries.size() == 11);
  CHECK(it.FindLegendaryForBase("w_demon_blade")->id == "leg_soulreaver");
  CHECK(it.economy.buyPriceMultiplier == 3);
  CHECK(it.economy.sellQualityMultiplier[EnumIndex(ItemQuality::Rare)] == 2.5);
  CHECK(it.economy.bagCapacity == 100);
  CHECK(it.loot.qualityThresholds.size() == 4);
  CHECK(it.loot.qualityThresholds[0].quality == ItemQuality::Legendary);
  CHECK(it.loot.equipmentWindowBelow == -10);
  CHECK(it.loot.affixTierBands.size() == 5);
  CHECK(it.crafting.Cost(CraftAction::Reforge, ItemQuality::Magic, false).allowed);
  CHECK(it.crafting.Cost(CraftAction::Reforge, ItemQuality::Magic, false).goldUnits == 1);
  CHECK_FALSE(it.crafting.Cost(CraftAction::Upgrade, ItemQuality::Rare, false).allowed);
  CHECK_FALSE(it.crafting.Cost(CraftAction::Socket, ItemQuality::Normal, true).allowed);
  CHECK(it.FindShop("blacksmith")->items.size() == 12);
}

TEST_CASE("shops: milestone-removed wares are not sold (I4 TP scroll, I2 ID scroll)") {
  const DataStore& d = test::RealData();
  for (const ShopDef& shop : d.Items().shops) {
    for (const std::string& item : shop.items) {
      CHECK(item != "c_tp_scroll");
      CHECK(item != "c_id_scroll");
    }
  }
  const NpcDef* merchant = d.FindNpc("merchant");
  REQUIRE(merchant);
  CHECK(std::find(merchant->shopItems.begin(), merchant->shopItems.end(), "c_tp_scroll") == merchant->shopItems.end());
  CHECK(std::find(merchant->shopItems.begin(), merchant->shopItems.end(), "c_hp_potion_s") != merchant->shopItems.end());
  CHECK(d.Items().FindBase("c_tp_scroll") != nullptr);  // the base stays (old data / later milestones)
}

TEST_CASE("monster tables") {
  const MonsterTables& m = test::RealData().Monsters();
  CHECK(m.defs.size() == 29);
  const MonsterDef* goblin = m.Find("goblin");
  REQUIRE(goblin);
  CHECK(goblin->hp == 55);
  CHECK(goblin->damage == 8);
  CHECK(goblin->defense == 4);
  CHECK(goblin->attackSpeedMs == 1200);
  CHECK_FALSE(goblin->isRanged);
  CHECK(goblin->nameKey == "data.monster.goblin");
  const MonsterDef* shaman = m.Find("miniboss_goblin_shaman");
  REQUIRE(shaman);
  CHECK(shaman->attackRange == 3);  // M5
  CHECK(shaman->isRanged);
  CHECK(m.FindForZone("emerald_plains", "slime_green") != nullptr);
  const MiniBossEntry* mb = m.MiniBossFor("emerald_plains");
  REQUIRE(mb);
  CHECK(mb->spawn == TilePos{60, 55});
  CHECK(m.MiniBossDialogue("miniboss_goblin_shaman")->nodes.size() == 3);
  CHECK(m.hunts.size() == 17);
  const HuntDef* thief = m.FindHunt("hunt_pendant_thief");
  REQUIRE(thief);
  CHECK(thief->defNormal.hp == 165);
  CHECK(thief->defNormal.damage == 10);
  CHECK(thief->revealAfterPrevious);
  CHECK(m.ai.leashMode == LeashMode::Returning);
  CHECK(m.ai.placementTries == 8);
  CHECK(m.ai.respawnDelayMs == 15000);
}

TEST_CASE("world tables") {
  const WorldTables& w = test::RealData().World();
  CHECK(w.maps.size() == 6);
  CHECK(w.mapOrder.size() == 5);
  const MapDef* ep = w.FindMap("emerald_plains");
  REQUIRE(ep);
  CHECK(ep->cols == 120);
  CHECK(ep->generated);
  CHECK(ep->generatorSeed == 12345);
  CHECK(ep->generatorTheme == MapTheme::Plains);
  CHECK(ep->generatorAvoid.size() == 28);
  REQUIRE(ep->camps.size() == 2);
  CHECK(ep->camps[0].pos == TilePos{15, 15});
  CHECK(ep->playerStart == TilePos{15, 22});
  CHECK(ep->levelMin == 1);
  CHECK(ep->safeZoneRadiusEffective == 9);
  CHECK(ep->orderIndex == 0);
  const MapDef* tower = w.FindMap("ember_tower");
  REQUIRE(tower);
  CHECK_FALSE(tower->generated);
  CHECK(tower->tiles.size() == 48);
  CHECK(tower->orderIndex == -1);
  CHECK(w.subDungeons.size() == 2);
  CHECK(w.mapGen.Theme(MapTheme::Plains).wallDensity == 0.03);
  CHECK(w.mapGen.Theme(MapTheme::Forest).grove.size() == 4);
  CHECK(w.mapGen.rngMultiplier == 16807);
  CHECK(w.constants.townPortalChannelMs == 1500);
  CHECK(w.constants.cameraPitchDeg == -50);
  CHECK(w.randomEvents.types.size() == 5);
  CHECK(w.randomEvents.ForZone("emerald_plains")->ambushCountMin == 3);
  // I7: the stash keeper stands in the Chapter 1 camp (appended slot); W7: the sealed gate message is an i18n key.
  CHECK(ep->camps[0].npcs == std::vector<std::string>{"blacksmith", "merchant", "quest_elder", "stash"});
  CHECK(w.constants.sealedGateMessageKey == "zone.exit.sealedChapter2");
  CHECK(test::RealData().Strings().Lookup(LocaleId::ZhCN, w.constants.sealedGateMessageKey) != nullptr);
  CHECK(test::RealData().Strings().Lookup(LocaleId::En, w.constants.sealedGateMessageKey) != nullptr);
  MapTheme t{};
  CHECK(w.moods.ThemeFor("ember_tower", t));
  CHECK(t == MapTheme::Plains);
}

TEST_CASE("quests, npcs, dialogue, story, lore") {
  const DataStore& d = test::RealData();
  const QuestTables& q = d.Quests();
  CHECK(q.quests.size() == 58);
  const QuestDef* slimes = q.Find("q_kill_slimes");
  REQUIRE(slimes);
  CHECK(slimes->category == QuestCategory::Main);
  REQUIRE(slimes->objectives.size() == 1);
  CHECK(slimes->objectives[0].type == ObjectiveType::Kill);
  CHECK(slimes->objectives[0].targetId == "slime_green");
  CHECK(slimes->objectives[0].required == 10);
  CHECK(slimes->giverNpcId == "quest_elder");
  CHECK(slimes->embersOnTurnIn == 2);
  const QuestDef* chief = q.Find("q_find_goblin_chief");
  REQUIRE(chief);
  CHECK(chief->questArea.col == 15);  // Q4 data fix: centred on the chief's spawn
  CHECK(chief->questArea.row == 95);
  CHECK(chief->rewards.choices.size() == 2);
  const QuestDef* herbs = q.Find("q_herb_gathering");
  REQUIRE(herbs);
  CHECK(herbs->objectives[0].sourceKind == ItemSourceKind::Gather);
  CHECK(herbs->objectives[0].gatherCount == 7);
  CHECK(q.achievements.size() == 12);
  CHECK(q.achievementsCountKillOnce);
  CHECK(q.ItemKindFor("mat_herb") == "herb");
  CHECK(q.ItemKindFor("unknown") == "relic");

  CHECK(d.Npcs().npcs.size() == 27);
  CHECK(d.Npcs().GiverOf("q_kill_slimes")->id == "quest_elder");
  CHECK(d.Dialogues().trees.size() == 10);
  const DialogueTree* elder = d.Dialogues().Find("quest_elder");
  REQUIRE(elder);
  CHECK(elder->FindNode(elder->startNodeId) != nullptr);

  const StoryScript& s = d.Story();
  CHECK(s.cutscenes.size() == 22);
  CHECK(s.triggers.size() == 16);
  CHECK(s.triggers[0].subjectId == "q_kill_slimes");
  CHECK(s.BossIntroFor("goblin_chief")->cutscene == "cs_boss_goblin_chief");
  const Cutscene* mark = s.FindCutscene("cs_ep_mark");
  REQUIRE(mark);
  CHECK(mark->steps[0].kind == StoryStepKind::Focus);
  CHECK(mark->steps[0].target.kind == StoryActorKind::Npc);
  CHECK(s.timing.beatDelayQuestTurnedInMs == 650);
  // Presentation phases the core step player times (8.5): chapter card 3500 + 3800 + 1600 = 8.9 s uncut.
  CHECK(s.timing.chapterIntroMs + s.timing.chapterHoldMs + s.timing.chapterOutroMs == 8900);
  CHECK(s.timing.titleHoldMs == 2200);
  CHECK(s.timing.title.inMs == 980);
  CHECK(s.timing.title.outMs == 450);
  CHECK(s.timing.narrate.inMs == 1100);
  CHECK(s.timing.narrate.outMs == 650);
  CHECK(s.timing.whisper.inMs == 1300);
  CHECK(s.timing.say.inMs == 220);
  CHECK(s.timing.slidePartInMs == 900);
  CHECK(s.timing.sequenceBackdropInMs == 900);
  CHECK(s.ChapterFor("emerald_plains") != nullptr);
  CHECK(d.Lore().entries.size() == 21);
  CHECK(d.Lore().ForZone("emerald_plains").size() == 4);
}

TEST_CASE("pets, homestead, audio, ui, abyss run") {
  const DataStore& d = test::RealData();
  CHECK(d.Pets().pets.size() == 8);
  const PetDef* sprite = d.FindPet("pet_sprite");
  REQUIRE(sprite);
  CHECK(sprite->passiveStat == Stat::ExpBonus);
  CHECK(sprite->abilities.size() == 2);
  CHECK(sprite->primaryAbilityId == "sprite_heal_pulse");
  CHECK(d.Pets().evolutionMult.size() == 3);
  CHECK(d.Pets().system.feedExp == 120);
  CHECK(d.Homestead().buildings.size() == 6);
  CHECK(d.Homestead().FindBuilding("herb_garden")->unlockQuest == "q_secure_plains");
  CHECK(d.Homestead().gardenIntervalByLevel.size() == 7);
  CHECK(d.Audio().cues.size() == EnumCount<SfxId>());  // 33 web cues + the A7 port cues
  const AudioCueDef* aggro = d.Audio().Find(SfxId::MonsterAggro);
  REQUIRE(aggro);
  CHECK_FALSE(aggro->webReachable);
  CHECK(aggro->families == std::vector<AnimRig>{AnimRig::Humanoid, AnimRig::Slime});
  CHECK(d.Audio().rules.monsterAggro == SfxId::MonsterAggro);
  CHECK(d.Audio().rules.monsterHurt == SfxId::MonsterHurt);
  CHECK(d.Audio().rules.skillUsedByDamageType[EnumIndex(DamageType::Fire)] == SfxId::SkillFire);
  CHECK(d.Audio().music.bossVictoryHoldMs == 8000);
  CHECK(d.UiTheme().designWidth == 1280);
  CHECK(d.RenderQualityProfiles().profiles[EnumIndex(RenderQuality::High)].maxDynamicLights == 32);
  CHECK(d.AbyssRun().boons.size() == 16);
  CHECK(d.AbyssRun().floorThemes.size() == 4);
  CHECK(d.Strings().HasLocale(LocaleId::ZhCN));
  CHECK(d.Strings().HasLocale(LocaleId::En));
}

TEST_CASE("validation errors carry file and JSON path") {
  DataStore store;
  DataLoadReport report;
  CHECK_FALSE(store.LoadFile("classes.json", "{\"schemaVersion\":1,\"classOrder\":[],\"classes\":[{\"id\":\"warrior\"}]}",
                             report));
  REQUIRE_FALSE(report.errors.empty());
  bool sawPath = false;
  for (const DataIssue& e : report.errors) {
    CHECK(e.file == "classes.json");
    if (e.path == "classes[0].name") sawPath = true;
  }
  CHECK(sawPath);

  DataLoadReport r2;
  CHECK_FALSE(store.LoadFile("soul_echo.json", "{\"schemaVersion\":2}", r2));
  CHECK(Contains(r2.Summary(), "schemaVersion"));

  DataLoadReport r3;
  CHECK_FALSE(store.LoadFile("difficulty.json", "{not json", r3));
  CHECK(Contains(r3.errors[0].message, "JSON parse error"));

  DataLoadReport r4;
  CHECK_FALSE(store.LoadFile("skill_trees.json",
                             "{\"schemaVersion\":1,\"trees\":[{\"id\":\"x\",\"classId\":\"paladin\",\"tabOrder\":0,"
                             "\"nameKey\":\"k\",\"color\":1}],\"damageTypeColors\":{}}",
                             r4));
  CHECK(Contains(r4.Summary(), "unknown value 'paladin'"));

  DataStore missing;
  DataLoadReport r5;
  CHECK_FALSE(missing.LoadAll([](std::string_view, std::string&) { return false; }, r5));
  CHECK(r5.errors.size() >= RequiredDataFiles().size());
}

namespace {
// Loads the real tables with one file replaced by `edit(parsed JSON)`; returns the report.
template <class Edit>
DataLoadReport LoadWithEdit(std::string_view fileName, Edit edit) {
  JsonValue doc;
  REQUIRE(ParseJson(test::ReadFile(test::DataDir() + "/" + std::string(fileName)), doc));
  edit(doc);
  const std::string edited = WriteJson(doc);
  DataStore store;
  DataLoadReport report;
  store.LoadAll(
      [&](std::string_view name, std::string& out) {
        out = name == fileName ? edited : test::ReadFile(test::DataDir() + "/" + std::string(name));
        return !out.empty();
      },
      report);
  return report;
}
bool HasError(const DataLoadReport& r, std::string_view file, std::string_view pathPrefix, std::string_view text) {
  for (const DataIssue& e : r.errors) {
    if (e.file == file && StartsWith(e.path, pathPrefix) && Contains(e.message, text)) return true;
  }
  return false;
}
JsonValue* Path(JsonValue& root, std::initializer_list<std::string_view> keys) {
  JsonValue* v = &root;
  for (std::string_view k : keys) {
    v = v->FindMutable(k);
    if (v == nullptr) return nullptr;
  }
  return v;
}
}  // namespace

TEST_CASE("map geometry is validated (sizes, positions, counts, level ranges)") {
  for (const int64_t bad : {int64_t{0}, int64_t{-5}, int64_t{60000}}) {
    const DataLoadReport r = LoadWithEdit("maps.json", [&](JsonValue& d) {
      Path(d, {"maps", "emerald_plains"})->Set("cols", JsonValue::Integer(bad));
    });
    INFO(r.Summary(5));
    CHECK(HasError(r, "maps.json", "maps.emerald_plains", "outside 1..512"));
  }
  const DataLoadReport spawn = LoadWithEdit("maps.json", [](JsonValue& d) {
    JsonValue& s0 = Path(d, {"maps", "emerald_plains", "spawns"})->MutableItems()[0];
    s0.Set("col", JsonValue::Integer(500));
    s0.Set("count", JsonValue::Integer(-1));
  });
  CHECK(HasError(spawn, "maps.json", "maps.emerald_plains.spawns[0]", "outside the 120x120 grid"));
  CHECK(HasError(spawn, "maps.json", "maps.emerald_plains.spawns[0].count", "negative"));
  const DataLoadReport hidden = LoadWithEdit("maps.json", [](JsonValue& d) {
    JsonValue& area = Path(d, {"maps", "emerald_plains", "hiddenAreas"})->MutableItems()[0];
    area.FindMutable("rewards")->MutableItems()[0].Set("col", JsonValue::Integer(-3));
  });
  CHECK(HasError(hidden, "maps.json", "maps.emerald_plains.hiddenAreas[0].rewards[0]", "outside"));
  const DataLoadReport range = LoadWithEdit("maps.json", [](JsonValue& d) {
    JsonValue arr = JsonValue::Array();
    arr.Append(JsonValue::Integer(9));
    arr.Append(JsonValue::Integer(2));
    Path(d, {"maps", "emerald_plains"})->Set("levelRange", arr);
  });
  CHECK(HasError(range, "maps.json", "maps.emerald_plains.levelRange", "levelMin > levelMax"));
}

TEST_CASE("keyed tables must list every enum value; [a, b] pairs must have two entries") {
  const DataLoadReport keyed = LoadWithEdit("hit_feedback.json", [](JsonValue& d) {
    CHECK(Path(d, {"profiles"})->Remove("kill"));
  });
  CHECK(HasError(keyed, "hit_feedback.json", "profiles", "missing key 'kill'"));
  const DataLoadReport dodge = LoadWithEdit("combat_input.json", [](JsonValue& d) {
    CHECK(Path(d, {"dodge", "distanceTiles"})->Remove("rogue"));
  });
  CHECK(HasError(dodge, "combat_input.json", "dodge.distanceTiles", "missing key 'rogue'"));
  const DataLoadReport pair = LoadWithEdit("world_constants.json", [](JsonValue& d) {
    JsonValue one = JsonValue::Array();
    one.Append(JsonValue::Number(0.75));
    Path(d, {"camera"})->Set("zoomRange", one);
  });
  CHECK(HasError(pair, "world_constants.json", "camera.zoomRange", "expected [min, max]"));
  const DataLoadReport ground = LoadWithEdit("classes.json", [](JsonValue& d) {
    for (JsonValue& c : Path(d, {"classes"})->MutableItems()) {
      for (JsonValue& sk : c.FindMutable("skills")->MutableItems()) {
        if (sk.Get("id").AsString() == "fire_wall") Path(sk, {"derived", "port"})->Set("groundTicks", JsonValue::Integer(0));
      }
    }
  });
  CHECK(HasError(ground, "classes.json", "classes.mage.skills.fire_wall.derived.port.groundTicks", "groundTicks >= 1"));
}

}  // TEST_SUITE

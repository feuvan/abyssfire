// GameSim facade (lead): session start, fixed step, freeze gates, command routing, snapshot, difficulty ladder.
// These run against the stubbed subsystems; they check the plumbing that every area relies on.
#include <algorithm>
#include <cmath>
#include <limits>

#include "TestUtil.h"
#include "abyss/hero/Hero.h"
#include "abyss/items/Inventory.h"
#include "abyss/pets/PetSystem.h"
#include "abyss/quests/Lore.h"
#include "abyss/quests/QuestSystem.h"
#include "abyss/save/SaveData.h"
#include "abyss/sim/GameSim.h"
#include "abyss/story/StoryDirector.h"
#include "abyss/world/Zone.h"
#include "doctest/doctest.h"

using namespace abyss;

namespace {
template <class E>
size_t Count(std::span<const Event> events) {
  size_t n = 0;
  for (const Event& e : events) {
    if (std::holds_alternative<E>(e)) ++n;
  }
  return n;
}

bool HasPanelRequest(std::span<const Event> events, PanelId panel, bool open) {
  for (const Event& e : events) {
    if (const EvPanelRequest* r = std::get_if<EvPanelRequest>(&e)) {
      if (r->panel == panel && r->open == open) return true;
    }
  }
  return false;
}

bool HasModal(const Snapshot& v, PanelId panel) {
  for (PanelId p : v.modals) {
    if (p == panel) return true;
  }
  return false;
}

// A new game opens with the prologue and the chapter card (quests-story 16.6), which freeze the world (S2) until they
// finish on real time. Tests that need a live world play them out at once.
void FinishNewGameStory(GameSim& sim) { sim.Context().sys.story->FinishAllBeats(); }

std::unique_ptr<GameSim> StartSim(ClassId cls = ClassId::Warrior, uint64_t seed = 11) {
  auto sim = GameSim::Create(test::RealData(), SimConfig{});
  REQUIRE(sim != nullptr);
  REQUIRE(sim->NewGame(cls, Difficulty::Normal, seed));
  FinishNewGameStory(*sim);
  sim->Step();
  return sim;
}
}  // namespace

TEST_SUITE("sim") {
  TEST_CASE("NewGame enters the default zone at playerStart and steps at 60 Hz") {
    const DataStore& data = test::RealData();
    REQUIRE(data.IsFinalized());
    SimConfig cfg;
    auto sim = GameSim::Create(data, cfg);
    REQUIRE(sim != nullptr);
    CHECK_FALSE(sim->HasSession());
    REQUIRE(sim->NewGame(ClassId::Warrior, Difficulty::Normal, 12345));
    CHECK(sim->HasSession());
    CHECK(Count<EvZone>(sim->Events()) == 1);
    CHECK(Count<EvEntitySpawned>(sim->Events()) >= 1);
    const Snapshot& v = sim->View();
    CHECK(v.zone.mapId == data.World().defaultMap);
    const MapDef* map = data.FindMap(data.World().defaultMap);
    REQUIRE(map != nullptr);
    CHECK(v.hero.pos == Vec2(map->playerStart.col, map->playerStart.row));
    CHECK(v.zone.grid != nullptr);
    CHECK(v.zone.cols == map->cols);
    CHECK_FALSE(v.npcs.empty());  // camp NPCs are placed by the zone runtime

    CHECK(sim->Context().sys.story->IsCinematic());  // the prologue holds the world
    FinishNewGameStory(*sim);
    for (int i = 0; i < 120; ++i) sim->Step();
    CHECK(sim->NowMs() == doctest::Approx(2000.0));
    CHECK(sim->View().steps == 120);
    // Frame(): 60 ms of real time = 3 steps (the accumulator keeps the 10 ms remainder).
    CHECK(sim->Frame(60.0) == 3);
    CHECK(sim->Frame(10.0) == 1);
  }

  TEST_CASE("pause menu freezes the sim clock; hero commands are rejected while frozen") {
    auto sim = GameSim::Create(test::RealData(), SimConfig{});
    REQUIRE(sim->NewGame(ClassId::Mage, Difficulty::Normal, 7));
    FinishNewGameStory(*sim);
    sim->Step();
    sim->Submit(CmdOpenPanel{PanelId::SystemMenu});
    sim->Step();  // the command applies in this step; the freeze holds from here on
    const double frozenAt = sim->NowMs();
    CHECK(sim->WorldFrozen());
    sim->Submit(CmdDebug{"teleport", "", 0, {30, 30}});  // not a hero gameplay command: applied while frozen
    for (int i = 0; i < 10; ++i) sim->Step();
    CHECK(sim->NowMs() == frozenAt);
    CHECK(sim->View().frozen);
    sim->Submit(CmdClosePanel{PanelId::SystemMenu});
    sim->Step();  // applies the close (unfreezes) without stepping
    sim->Step();
    CHECK(sim->NowMs() > frozenAt);
    CHECK_FALSE(sim->WorldFrozen());
  }

  TEST_CASE("command classification and difficulty ladder") {
    CHECK(IsHeroGameplayCommand(Command{CmdMoveTo{{1, 1}}}));
    CHECK(IsHeroGameplayCommand(Command{CmdCastSkill{}}));
    CHECK_FALSE(IsHeroGameplayCommand(Command{CmdOpenPanel{}}));
    CHECK_FALSE(IsHeroGameplayCommand(Command{CmdStorySkip{}}));

    const std::vector<Difficulty> none;
    DifficultyStates s = GetDifficultyStates(none);
    CHECK(s.Of(Difficulty::Normal) == DifficultyState::Available);
    CHECK(s.Of(Difficulty::Nightmare) == DifficultyState::Locked);
    const std::vector<Difficulty> normalDone{Difficulty::Normal};
    s = GetDifficultyStates(normalDone);
    CHECK(s.Of(Difficulty::Normal) == DifficultyState::Completed);
    CHECK(s.Of(Difficulty::Nightmare) == DifficultyState::Available);
    CHECK(s.hasNextUnlocked);
    CHECK(s.nextUnlocked == Difficulty::Nightmare);
    CHECK(DeriveCompletedDifficulties(Difficulty::Hell, none) ==
          std::vector<Difficulty>{Difficulty::Normal, Difficulty::Nightmare});
    CHECK_FALSE(ShouldShowDifficultySelector(Difficulty::Normal, none));
    CHECK(ShouldShowDifficultySelector(Difficulty::Nightmare, none));
  }

  TEST_CASE("core-owned modals: derived from the owners' state, S2 freeze, U7 same-batch input block") {
    auto sim = StartSim();
    // Control: a hero command applies when nothing is open (the target must be a live monster: SetAttackTarget).
    EntityId foe = kNoEntity;
    for (const MonsterView& m : sim->View().monsters) {
      if (m.alive) {
        foe = m.id;
        break;
      }
    }
    REQUIRE(foe != kNoEntity);
    sim->Submit(CmdAttackTarget{foe});
    sim->Step();
    CHECK(sim->View().hero.target == foe);
    CHECK_FALSE(sim->View().inputBlocked);
    sim->Submit(CmdClearTarget{});
    sim->Step();

    // The dialogue opens inside the batch; the attack later in the same batch is already rejected.
    sim->Submit(CmdDialogueOpenTree{"quest_elder"});
    sim->Submit(CmdAttackTarget{foe});
    sim->Step();
    CHECK(sim->View().hero.target == kNoEntity);
    CHECK(sim->View().inputBlocked);
    CHECK(sim->WorldFrozen());  // S2: dialogue freezes
    CHECK(HasModal(sim->View(), PanelId::Dialogue));
    CHECK(HasPanelRequest(sim->Events(), PanelId::Dialogue, true));
    const double frozenAt = sim->NowMs();
    for (int i = 0; i < 5; ++i) sim->Step();
    CHECK(sim->NowMs() == frozenAt);

    // UE cannot open or echo core-owned panels.
    sim->Submit(CmdOpenPanel{PanelId::Shop});
    sim->Step();
    CHECK_FALSE(HasModal(sim->View(), PanelId::Shop));
    CHECK(sim->Context().session.panels.open.empty());

    // A UE close goes through the owner.
    sim->Submit(CmdClosePanel{PanelId::Dialogue});
    sim->Step();
    CHECK(HasPanelRequest(sim->Events(), PanelId::Dialogue, false));
    CHECK_FALSE(sim->View().inputBlocked);
    CHECK(sim->View().modals.empty());
    sim->Step();
    CHECK_FALSE(sim->WorldFrozen());
    CHECK(sim->NowMs() > frozenAt);

    // A core-owned modal that does not freeze still blocks input (stash, U7).
    sim->Context().sys.inventory->OpenStash("stash");
    sim->Submit(CmdAttackTarget{foe});
    sim->Step();
    CHECK(sim->View().hero.target == kNoEntity);
    CHECK_FALSE(sim->WorldFrozen());
    CHECK(sim->View().inputBlocked);
    CHECK(HasPanelRequest(sim->Events(), PanelId::Stash, true));
    sim->Submit(CmdClosePanel{PanelId::Stash});
    sim->Step();
    CHECK_FALSE(sim->Context().sys.inventory->Stash().open);
    CHECK_FALSE(sim->View().inputBlocked);
  }

  TEST_CASE("the hero's death closes core-owned modals; Dying rejects the system menu (C12)") {
    auto sim = StartSim(ClassId::Rogue);
    sim->Submit(CmdDialogueOpenTree{"quest_elder"});
    sim->Step();
    REQUIRE(sim->WorldFrozen());
    sim->Context().bus.Publish(HeroDiedMsg{sim->View().hero.pos});
    sim->Step();
    CHECK(sim->View().modals.empty());
    CHECK(HasPanelRequest(sim->Events(), PanelId::Dialogue, false));
    sim->Step();
    CHECK_FALSE(sim->WorldFrozen());

    sim->Context().sys.hero->SetLife(HeroLife::Dying);
    sim->Submit(CmdOpenPanel{PanelId::SystemMenu});
    sim->Submit(CmdOpenPanel{PanelId::Inventory});
    sim->Step();
    CHECK_FALSE(sim->Context().session.panels.IsOpen(PanelId::SystemMenu));
    CHECK(sim->Context().session.panels.IsOpen(PanelId::Inventory));
    CHECK_FALSE(sim->WorldFrozen());
    sim->Context().sys.hero->SetLife(HeroLife::Alive);
  }

  TEST_CASE("save -> load -> save is idempotent (the zone entry's spawn rolls do not advance the saved RNG streams)") {
    // Regression (playthrough): ApplySave restored the RNG streams before the zone entry, whose monster spawns then
    // drew from the Ai stream, so SaveGame() right after LoadGame() differed in rng.ai from the save just loaded.
    auto sim = StartSim(ClassId::Warrior, 77);
    for (int i = 0; i < 600; ++i) sim->Step();
    const std::string save = sim->SaveGame(1234);
    auto fresh = GameSim::Create(test::RealData(), SimConfig{});
    REQUIRE(fresh != nullptr);
    std::string err;
    REQUIRE(fresh->LoadGame(save, &err) == SaveError::None);
    CHECK(fresh->SaveGame(1234) == save);
    // Loading again into the same instance (UE's Continue after a session) gives the same state.
    REQUIRE(fresh->LoadGame(save, &err) == SaveError::None);
    CHECK(fresh->SaveGame(1234) == save);
  }

  TEST_CASE("retroGrantPets: a story pet lost to a save during its beat is granted once its quest is turned in") {
    // Regression (final review): cs_tf_moonfang (monster_killed werewolf_alpha) grants pet_storm_wolf when the beat
    // ends. A save written while it plays (level-up, app background, quit) has neither the beat nor the pet, and
    // monster_killed triggers never replay; the web's EmberTower.retroGrantPets (homestead.json storyPets) repairs it
    // on every zone entry once q_kill_werewolf_alpha is turned in.
    auto sim = StartSim(ClassId::Rogue, 13);
    MonsterKilledMsg k;
    k.defId = "werewolf_alpha";
    sim->Context().sys.story->OnMonsterKilled(k);
    REQUIRE(sim->Context().sys.story->IsCinematic());
    SaveData mid;
    sim->BuildSave(mid, 500);
    CHECK(std::find(mid.storySeen.begin(), mid.storySeen.end(), "cs_tf_moonfang") == mid.storySeen.end());

    auto fresh = GameSim::Create(test::RealData(), SimConfig{});
    REQUIRE(fresh != nullptr);
    std::string err;
    REQUIRE(fresh->LoadGame(mid, &err) == SaveError::None);
    SimContext& f = fresh->Context();
    CHECK_FALSE(f.sys.pets->Has("pet_storm_wolf"));
    CHECK_FALSE(f.sys.story->IsBusy());  // the beat is gone for good

    // Turned in before the save: the load grants it (before the equip-stat merge).
    SaveData turned = mid;
    turned.quests.push_back(QuestProgress{"q_kill_werewolf_alpha", QuestStatus::TurnedIn, {1}});
    REQUIRE(fresh->LoadGame(turned, &err) == SaveError::None);
    CHECK(f.sys.pets->Has("pet_storm_wolf"));
    CHECK(f.sys.pets->Active() != nullptr);

    // Turned in during the session: the next zone entry grants it.
    REQUIRE(fresh->LoadGame(mid, &err) == SaveError::None);
    REQUIRE_FALSE(f.sys.pets->Has("pet_storm_wolf"));
    std::vector<QuestProgress> all(f.sys.quests->AllProgress().begin(), f.sys.quests->AllProgress().end());
    all.push_back(QuestProgress{"q_kill_werewolf_alpha", QuestStatus::TurnedIn, {1}});
    f.sys.quests->Load(all);
    f.sys.zone->RequestZoneChange("emerald_plains", f.sys.zone->CampPosition(0));
    for (int i = 0; i < 90 && !f.sys.pets->Has("pet_storm_wolf"); ++i) fresh->Step();
    CHECK(f.sys.pets->Has("pet_storm_wolf"));
    // A replay of the beat later adds nothing (no duplicate log): the pet is owned.
    CHECK(f.sys.pets->Owned().size() == 1);
  }

  TEST_CASE("a load is a pure function of the save; its zone-entry spawns run on derived streams, not the saved ones") {
    // S3 (final review): ApplySave runs the zone entry (spawn jitter / elite rolls on RngStream::Ai) on states derived
    // from the saved ones and then restores the saved states, so the first AI draws in play do not replay the uniforms
    // that placed the zone's monsters, while save -> load -> save stays byte-identical.
    auto sim = StartSim(ClassId::Mage, 909);
    for (int i = 0; i < 300; ++i) sim->Step();
    const std::string save = sim->SaveGame(4242);
    std::string err;
    auto a = GameSim::Create(test::RealData(), SimConfig{});
    auto b = GameSim::Create(test::RealData(), SimConfig{});
    REQUIRE(a->LoadGame(save, &err) == SaveError::None);
    REQUIRE(b->LoadGame(save, &err) == SaveError::None);
    CHECK(a->SaveGame(4242) == save);  // the saved streams are restored exactly after the entry
    for (int i = 0; i < 600; ++i) {
      a->Step();
      b->Step();
    }
    CHECK(a->SaveGame(5000) == b->SaveGame(5000));
    REQUIRE(a->View().monsters.size() == b->View().monsters.size());
    for (size_t i = 0; i < a->View().monsters.size(); ++i) {
      CHECK(a->View().monsters[i].pos == b->View().monsters[i].pos);
    }
  }

  TEST_CASE("a hero above a lowered maximum is saved clamped, as the load restores it (FIX Q8 symmetry)") {
    // Regression (playthrough): swapping to gear with less maxHp / maxMana leaves hp / mana above the new maxima until
    // the next heal (recalcDerived never clamps); the save wrote them raw while ApplySave clamps, so
    // save -> load -> save changed player.hp / player.mana.
    auto sim = StartSim(ClassId::Mage, 31);
    Hero& hero = *sim->Context().sys.hero;
    const double maxHp = hero.MaxHp(), maxMana = hero.MaxMana();
    hero.SetHp(maxHp + 40);
    hero.SetMana(maxMana + 25);
    SaveData built;
    sim->BuildSave(built, 0);
    CHECK(built.player.hp == maxHp);
    CHECK(built.player.mana == maxMana);
    CHECK(hero.Hp() == maxHp + 40);  // the live state is untouched (web parity until the next heal)
    const std::string save = sim->SaveGame(99);
    auto fresh = GameSim::Create(test::RealData(), SimConfig{});
    REQUIRE(fresh != nullptr);
    std::string err;
    REQUIRE(fresh->LoadGame(save, &err) == SaveError::None);
    CHECK(fresh->View().hero.hp == maxHp);
    CHECK(fresh->SaveGame(99) == save);
  }

  TEST_CASE("LoadGame: SaveError paths, non-finite position, visitedZones, difficulty override") {
    auto sim = StartSim(ClassId::Mage, 21);
    SaveData base;
    sim->BuildSave(base, 1000);
    REQUIRE(base.classId == ClassId::Mage);
    CHECK_FALSE(base.visitedZones.empty());  // the starting zone counts as visited

    std::string err;
    SaveData tooNew = base;
    tooNew.version = kCurrentSaveVersion + 1;
    tooNew.player.gold = 777;
    CHECK(sim->LoadGame(tooNew, &err) == SaveError::VersionTooNew);
    CHECK(sim->HasSession());
    CHECK(sim->View().hero.gold == base.player.gold);  // session unchanged

    SaveData nanPos = base;
    nanPos.player.tileCol = std::numeric_limits<double>::quiet_NaN();
    nanPos.player.tileRow = std::numeric_limits<double>::infinity();
    nanPos.player.hp = 10;
    nanPos.visitedZones = {"emerald_plains", "twilight_forest"};
    CHECK(sim->LoadGame(nanPos, &err) == SaveError::None);
    const Vec2 camp = sim->Context().sys.zone->CampPosition(0);
    CHECK(std::isfinite(sim->View().hero.pos.x));
    CHECK(sim->View().hero.pos == camp);
    CHECK(sim->Context().session.visitedZones == nanPos.visitedZones);
    SaveData again;
    sim->BuildSave(again, 2000);
    CHECK(again.visitedZones == nanPos.visitedZones);

    // Difficulty override (Continue -> selector): Nightmare needs Normal completed.
    const std::string json = sim->SaveGame(3000);
    CHECK(sim->LoadGame(json, &err, Difficulty::Nightmare) == SaveError::Invalid);
    SaveData done = base;
    // (count, value) rather than {Normal}: GCC 13 -O2 + sanitizers reports a false -Warray-bounds on the 1-byte
    // initializer_list memmove.
    const std::vector<Difficulty> normalOnly(1, Difficulty::Normal);
    done.completedDifficulties = normalOnly;
    const SaveSlotInfo info = SummarizeSave(1, done);
    CHECK(info.exists);
    CHECK(info.classId == ClassId::Mage);
    CHECK(info.difficulty == Difficulty::Normal);
    CHECK(info.completedDifficulties == normalOnly);
    CHECK(info.showDifficultySelector);
    CHECK_FALSE(SummarizeSave(1, base).showDifficultySelector);
  }

  TEST_CASE("hidden-area reward props respawn for discovered areas and claim through RewardService (Q5)") {
    auto sim = StartSim(ClassId::Warrior, 5);
    SaveData save;
    sim->BuildSave(save, 0);
    REQUIRE(save.player.currentMap == "emerald_plains");
    save.discoveredHiddenAreas = {"hidden_ep_elven_cache"};
    std::string err;
    REQUIRE(sim->LoadGame(save, &err) == SaveError::None);
    const LoreSystem& lore = *sim->Context().sys.lore;
    REQUIRE(lore.HiddenRewards().size() == 2);
    size_t markers = 0;
    for (const WorldMarkerView& m : sim->View().markers) {
      if (m.kind == MarkerKind::HiddenReward) ++markers;
    }
    CHECK(markers == 2);
    const int64_t gold = sim->View().hero.gold;
    CHECK(sim->Context().sys.lore->ClaimHiddenReward("hidden_ep_elven_cache", 1));
    CHECK(sim->Context().sys.hero->Gold() == gold + 200);
    CHECK(lore.IsRewardClaimed("hidden_ep_elven_cache", 1));
    CHECK_FALSE(sim->Context().sys.lore->ClaimHiddenReward("hidden_ep_elven_cache", 1));
    CHECK(lore.HiddenRewards().size() == 1);

    SaveData after;
    sim->BuildSave(after, 0);
    CHECK(after.hiddenRewardsClaimed == std::vector<std::string>{"hidden_ep_elven_cache#1"});
    REQUIRE(sim->LoadGame(after, &err) == SaveError::None);
    REQUIRE(sim->Context().sys.lore->HiddenRewards().size() == 1);  // only the unclaimed chest comes back
    CHECK(sim->Context().sys.lore->HiddenRewards()[0].rewardIndex == 0);
  }

  TEST_CASE("StoryDirector::ResolveActor") {
    auto sim = StartSim(ClassId::Rogue, 9);
    const StoryDirector& story = *sim->Context().sys.story;
    StoryActor hero;
    hero.kind = StoryActorKind::Hero;
    StoryActorView v = story.ResolveActor(hero);
    CHECK(v.resolved);
    CHECK(v.entity == kHeroEntityId);
    CHECK(v.artId == "rogue");
    CHECK(v.nameKey == "story.speaker.hero");

    StoryActor villain;
    villain.kind = StoryActorKind::Villain;
    v = story.ResolveActor(villain);
    CHECK(v.artId == "emblem_villain");

    StoryActor npc;
    npc.kind = StoryActorKind::Npc;
    npc.id = "quest_elder";
    v = story.ResolveActor(npc);
    CHECK(v.resolved);  // stands in the starting camp
    CHECK(v.entity != kNoEntity);
    CHECK(v.nameKey == "data.npc.quest_elder.name");

    StoryActor missing;
    missing.kind = StoryActorKind::Monster;
    missing.id = "no_such_monster";
    v = story.ResolveActor(missing);
    CHECK_FALSE(v.resolved);
  }
}

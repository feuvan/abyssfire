// Pets area (quests+story+pets owner): pet rules, companion, homestead. Spec vectors: quests-story-ch1.md 18.
// Foundation smoke tests only.
#include "SimHarness.h"
#include "abyss/pets/Homestead.h"
#include "abyss/pets/PetCompanion.h"
#include "abyss/pets/PetSystem.h"
#include "doctest/doctest.h"

using namespace abyss;

TEST_SUITE("pets") {
  TEST_CASE("evolution stage = count of [10, 20] reached (18.3)") {
    const PetTables& t = test::RealData().Pets();
    CHECK(PetEvolutionForLevel(t, 9) == 0);
    CHECK(PetEvolutionForLevel(t, 10) == 1);
    CHECK(PetEvolutionForLevel(t, 20) == 2);
  }

  TEST_CASE("quest embers: reward.embers ?? (main 2 : side 1) (4.4.1)") {
    const HomesteadTables& t = test::RealData().Homestead();
    CHECK(EmbersForQuest(t, QuestCategory::Main, false, 0) == 2);
    CHECK(EmbersForQuest(t, QuestCategory::Side, false, 0) == 1);
    CHECK(EmbersForQuest(t, QuestCategory::Side, true, 4) == 4);
    HomesteadTower tower(t);
    CHECK(tower.AddEmbers(-3) == 0);
    CHECK(tower.AddEmbers(5) == 5);
    CHECK(tower.Embers() == 5);
  }

  TEST_CASE("PetSystem starts with no beasts") {
    test::SimHarness h;
    PetSystem p(h.ctx.data, h.events, h.bus);
    CHECK(p.Owned().empty());
    CHECK(p.Active() == nullptr);
  }

  TEST_CASE("PetArtId: beast_<id>, evolved stages add _e<stage> (PetKit.ts)") {
    CHECK(PetArtId("pet_sprite", 0) == "beast_pet_sprite");
    CHECK(PetArtId("pet_sprite", 1) == "beast_pet_sprite_e1");
    CHECK(PetArtId("pet_sprite", 2) == "beast_pet_sprite_e2");
  }
}

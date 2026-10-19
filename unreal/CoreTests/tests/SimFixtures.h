// GameSim-facade test fixtures (moved out of sim_test.cpp so content_ch1_test.cpp shares them): event counters, modal
// helpers and StartSim (a new game with the prologue and chapter card played out).
#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

#include "TestUtil.h"
#include "abyss/combat/Combat.h"
#include "abyss/hero/Hero.h"
#include "abyss/items/Inventory.h"
#include "abyss/monsters/MonsterSystem.h"
#include "abyss/pets/PetSystem.h"
#include "abyss/quests/Lore.h"
#include "abyss/quests/QuestSystem.h"
#include "abyss/save/SaveData.h"
#include "abyss/sim/GameSim.h"
#include "abyss/story/StoryDirector.h"
#include "abyss/world/Zone.h"
#include "doctest/doctest.h"

namespace abyss::test::sim_fx {

template <class E>
inline size_t Count(std::span<const Event> events) {
  size_t n = 0;
  for (const Event& e : events) {
    if (std::holds_alternative<E>(e)) ++n;
  }
  return n;
}

inline bool HasPanelRequest(std::span<const Event> events, PanelId panel, bool open) {
  for (const Event& e : events) {
    if (const EvPanelRequest* r = std::get_if<EvPanelRequest>(&e)) {
      if (r->panel == panel && r->open == open) return true;
    }
  }
  return false;
}

inline bool HasModal(const Snapshot& v, PanelId panel) {
  for (PanelId p : v.modals) {
    if (p == panel) return true;
  }
  return false;
}

// A new game opens with the prologue and the chapter card (quests-story 16.6), which freeze the world (S2) until they
// finish on real time. Tests that need a live world play them out at once.
inline void FinishNewGameStory(GameSim& sim) { sim.Context().sys.story->FinishAllBeats(); }

inline std::unique_ptr<GameSim> StartSim(ClassId cls = ClassId::Warrior, uint64_t seed = 11) {
  auto sim = GameSim::Create(test::RealData(), SimConfig{});
  REQUIRE(sim != nullptr);
  REQUIRE(sim->NewGame(cls, Difficulty::Normal, seed));
  FinishNewGameStory(*sim);
  sim->Step();
  return sim;
}

}  // namespace abyss::test::sim_fx

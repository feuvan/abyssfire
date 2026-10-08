// The shared services and subsystem registry every runtime system receives at construction (ARCHITECTURE 3.2).
//
// Dependency rules (binding for all areas):
// * GameSim owns everything below. It builds one SimContext, then constructs the subsystems in the order of
//   SimSystems, passing `SimContext&`, and finally fills SimSystems. A constructor may store the reference but must not
//   call other subsystems (their pointers are not set yet); cross-system calls happen from Tick/OnTimer/handlers only.
// * Pure building blocks (Damage, Skills scaling, StatusEffectSystem, MapGen, Pathfinder, QuestSystem, LootGen,
//   Crafting, MonsterAI, SpatialGrid, PetSystem rules...) take narrow references and never see SimContext, so they are
//   unit-testable alone. Runtime systems (the ones listed in SimSystems) glue them to the sim.
// * Cross-area notifications go through `bus` (GameplayBus); GameSim subscribes the handlers in a fixed order in one
//   place (Private/sim/SimWiring.cpp) so hook order never depends on construction order. Presentation goes to
//   `events` (EventSink). Nothing else is global.
// * Exp, gold and item rewards always go through `sys.rewards` (RewardService), never Hero::AddExp / AddGold directly.
// * Modal panels: an area that opens a modal (dialogue, quest card, shop, stash, mini-boss, lore text, puzzle) only sets
//   its own state; GameSim derives the freeze, the input block and EvPanelRequest from it (SimTypes.h PanelId).
// * Time: every absolute time is SimClock ms (`clock.NowMs()`), durations are ms (double). Delayed actions are timers
//   in `timers` (TimerQueue) owned by a TimerOwner; GameSim routes a due timer to `<owner system>->OnTimer(t)`.
// * RNG (S3): each area draws only from its stream(s):
//     Combat - damage rolls (dodge/crit), procs, status-rule chances, skill effects, elite on-hit rolls
//     Loot   - loot generation, potion drops, ley-fruit drops, quest pick-one gear, shop stock, crafting rolls,
//              kill gold
//     Ai     - monster AI (patrol, leash), elite affix rolls, respawn jitter, mini-boss/hunt placement
//     World  - zone runtime (hidden rewards, interact fallbacks), town portal, exploration
//     Events - random events (trigger rolls, event contents)
//     Quests - quest drops, gather spot tie-breaks, dialogue rewards, story/achievement rolls
//     Pets   - pet AI, pet abilities, garden yields, expeditions
//   Map generation uses its own Park-Miller generator seeded from map data (W10), never an RngSet stream.
#pragma once

#include <cstdint>

#include "abyss/base/Enums.h"
#include "abyss/base/Platform.h"
#include "abyss/base/Rng.h"
#include "abyss/base/SimClock.h"
#include "abyss/base/Stats.h"
#include "abyss/base/Timers.h"
#include "abyss/base/Types.h"
#include "abyss/sim/Events.h"
#include "abyss/sim/GameplayBus.h"
#include "abyss/sim/Session.h"

namespace abyss {

class DataStore;
class I18n;

// hero + combat
class Hero;
class RewardService;
class StatusEffectSystem;
class CombatSystem;
class ProjectileSystem;
class SoulEchoSystem;
// monsters
class MonsterSystem;
// items
class InventorySystem;
class GroundLootSystem;
class ShopSystem;
// quests + story + pets
class QuestSystem;
class QuestWorld;
class DialogueSystem;
class AchievementSystem;
class LoreSystem;
class StoryDirector;
class PetSystem;
class PetCompanion;
class HomesteadSystem;
// world
class ZoneRuntime;
class HeroLocomotion;
class ExplorationSystem;
class RandomEventSystem;
class AudioDirector;

struct SimConfig {
  uint64_t defaultSeed = 0x0AB5F17E5EEDull;  // NewGame(seed == 0) uses this
  bool touchMode = false;                     // U7: HUD panels pause the sim on touch
  bool milestone1 = true;                     // U6 / Q3: hide later-milestone features (tower UI, mercenary, labyrinth)
  double autosaveIntervalMs = 60000;          // U1 / FIX Q4
  int32_t maxStepsPerFrame = 4;               // S1 spiral-of-death clamp
  double maxFrameMs = 250;                    // ue58-platform 13: resume from background
  bool enableDebugCommands = false;
};

// Non-owning pointers to the runtime subsystems, in construction order. Never null once GameSim::Create returned.
struct SimSystems {
  Hero* hero = nullptr;
  StatusEffectSystem* status = nullptr;
  ZoneRuntime* zone = nullptr;
  HeroLocomotion* locomotion = nullptr;
  ExplorationSystem* exploration = nullptr;
  MonsterSystem* monsters = nullptr;
  ProjectileSystem* projectiles = nullptr;
  CombatSystem* combat = nullptr;
  SoulEchoSystem* soulEcho = nullptr;
  InventorySystem* inventory = nullptr;
  GroundLootSystem* groundLoot = nullptr;
  ShopSystem* shop = nullptr;
  RewardService* rewards = nullptr;  // the one exp / gold / item reward path (hero/Rewards.h)
  QuestSystem* quests = nullptr;
  QuestWorld* questWorld = nullptr;
  DialogueSystem* dialogue = nullptr;
  AchievementSystem* achievements = nullptr;
  LoreSystem* lore = nullptr;
  StoryDirector* story = nullptr;
  HomesteadSystem* homestead = nullptr;
  PetSystem* pets = nullptr;
  PetCompanion* petCompanion = nullptr;
  RandomEventSystem* randomEvents = nullptr;
  AudioDirector* audio = nullptr;
};

struct SimContext {
  const DataStore& data;
  const SimConfig& config;
  SimClock& clock;
  RngSet& rng;
  TimerQueue& timers;
  EventSink& events;
  GameplayBus& bus;
  EntityIdAllocator& ids;
  SessionState& session;
  SimSystems sys;
  // Merged EquipStats (loot 8.3 / classes 16 step 4): gear + achievement bonuses + active pet passive (minus
  // expBonus/magicFind) + altar blessing (+ labyrinth boons later). Rebuilt by GameSim at the start of every unfrozen
  // step and immediately on EquipStatsDirtyMsg. Read-only for every area.
  EquipStats equip;

  double Now() const { return clock.NowMs(); }
  Rng& Rand(RngStream s) { return rng.Get(s); }
};

}  // namespace abyss

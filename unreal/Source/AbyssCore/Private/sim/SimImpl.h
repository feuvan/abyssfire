// GameSim internals: owns the shared services and every runtime subsystem, the command queue and the snapshot.
//
// Files (owner: lead; areas add calls to their own systems here only through review):
//   GameSim.cpp      facade, session start, step loop (D13 step algorithm), freeze handling, timer dispatch,
//                    per-step update order (classes 16 / monsters 3.9 / world 17)
//   SimWiring.cpp    GameplayBus subscriptions in their binding order (kill pipeline = monsters-ai 11)
//   SimCommands.cpp  command routing + freeze / Dying gates
//   SimZone.cpp      zone entry / exit sequences (world 9.1), equip-stat merge (loot 8.3)
//   SimSave.cpp      BuildSave / ApplySave (save-ui-input 3.4 / 3.5 order)
//   SimSnapshot.cpp  snapshot assembly
#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "abyss/audio/Audio.h"
#include "abyss/base/Rng.h"
#include "abyss/base/SimClock.h"
#include "abyss/base/Timers.h"
#include "abyss/combat/Combat.h"
#include "abyss/combat/Projectiles.h"
#include "abyss/combat/SoulEcho.h"
#include "abyss/combat/StatusEffects.h"
#include "abyss/data/DataStore.h"
#include "abyss/hero/Hero.h"
#include "abyss/hero/Rewards.h"
#include "abyss/items/GroundLoot.h"
#include "abyss/items/Inventory.h"
#include "abyss/items/Shop.h"
#include "abyss/monsters/MonsterSystem.h"
#include "abyss/pets/Homestead.h"
#include "abyss/pets/PetCompanion.h"
#include "abyss/pets/PetSystem.h"
#include "abyss/quests/Achievements.h"
#include "abyss/quests/Dialogue.h"
#include "abyss/quests/Lore.h"
#include "abyss/quests/QuestSystem.h"
#include "abyss/quests/QuestWorld.h"
#include "abyss/save/SaveData.h"
#include "abyss/save/SaveIO.h"
#include "abyss/sim/Commands.h"
#include "abyss/sim/Events.h"
#include "abyss/sim/GameSim.h"
#include "abyss/sim/GameplayBus.h"
#include "abyss/sim/Session.h"
#include "abyss/sim/SimContext.h"
#include "abyss/sim/Snapshot.h"
#include "abyss/story/StoryDirector.h"
#include "abyss/world/Exploration.h"
#include "abyss/world/Locomotion.h"
#include "abyss/world/RandomEvents.h"
#include "abyss/world/Zone.h"

namespace abyss {

struct SimImpl {
  SimImpl(const DataStore& data, const SimConfig& cfg);
  ~SimImpl();

  // ---- shared services (declaration order = construction order; ctx references them) ----
  const DataStore& data;
  SimConfig config;
  SimClock clock;
  RngSet rng;
  TimerQueue timers;
  EventSink events;
  GameplayBus bus;
  EntityIdAllocator ids;
  SessionState session;
  SimContext ctx;

  // ---- runtime systems (SimSystems order) ----
  std::unique_ptr<Hero> hero;
  std::unique_ptr<StatusEffectSystem> status;
  std::unique_ptr<ZoneRuntime> zone;
  std::unique_ptr<HeroLocomotion> locomotion;
  std::unique_ptr<ExplorationSystem> exploration;
  std::unique_ptr<MonsterSystem> monsters;
  std::unique_ptr<ProjectileSystem> projectiles;
  std::unique_ptr<CombatSystem> combat;
  std::unique_ptr<SoulEchoSystem> soulEcho;
  std::unique_ptr<InventorySystem> inventory;
  std::unique_ptr<GroundLootSystem> groundLoot;
  std::unique_ptr<ShopSystem> shop;
  std::unique_ptr<RewardService> rewards;
  std::unique_ptr<QuestSystem> quests;
  std::unique_ptr<QuestWorld> questWorld;
  std::unique_ptr<DialogueSystem> dialogue;
  std::unique_ptr<AchievementSystem> achievements;
  std::unique_ptr<LoreSystem> lore;
  std::unique_ptr<StoryDirector> story;
  std::unique_ptr<HomesteadSystem> homestead;
  std::unique_ptr<PetSystem> pets;
  std::unique_ptr<PetCompanion> petCompanion;
  std::unique_ptr<RandomEventSystem> randomEvents;
  std::unique_ptr<AudioDirector> audio;

  // ---- loop state ----
  std::vector<Command> pending;
  Snapshot snapshot;
  bool hasSession = false;
  bool wasFrozen = false;
  uint32_t coreModalMask = 0;           // bit (1 << PanelId) per open core-owned modal at the last SyncFreeze
  std::array<std::string, EnumCount<PanelId>()> coreModalNpc{};  // npcId per panel when it opened (EvPanelRequest)

  // GameSim.cpp
  void BuildSystems(ClassId cls);  // (re)creates every system for a new session, then Wire()
  void ResetSession();
  void StepOnce();
  void Update(double dtMs);
  void DispatchTimer(const Timer& t);
  uint32_t ComputeFreezeMask() const;
  // Recomputes the clock's freeze mask (runs OnFreezeBegin on a rising edge) and diffs the core-owned modals
  // (EvPanelRequest on every open / close).
  void SyncFreeze();
  void OnFreezeBegin(bool cinematic);
  bool WorldFrozen() const { return clock.IsFrozen(); }
  bool InputBlocked() const;  // frozen or a modal panel (core-owned or UE-reported) is open (U7)
  // Core-owned modal panels open now (SimTypes.h IsCoreOwnedPanel), derived from the owning systems' state.
  uint32_t CoreModalMask() const;
  std::string CoreModalNpc(PanelId p) const;
  void SyncCoreModals();
  // Closes every core-owned modal through its owner (ExitZone, the hero's death).
  void CloseCoreModals();
  // CmdClosePanel for a core-owned modal: the owner's Close().
  void CloseCoreModal(PanelId p);
  void RequestSave(SaveReason reason);
  bool CanSave() const;
  // SimWiring.cpp
  void Wire();
  void OnMonsterKilledDifficulty(const MonsterKilledMsg& m);
  // SimCommands.cpp
  void ApplyCommands(bool frozen);
  void ApplyCommand(const Command& c);
  // SimZone.cpp
  bool EnterZone(std::string_view mapId, bool hasTarget, Vec2 target);
  void ExitZone();
  void RebuildEquipStats();
  // SimSave.cpp
  void BuildSave(SaveData& out, int64_t unixMs) const;
  SaveError ApplySave(const SaveData& s, std::string* err);
  // SimSnapshot.cpp
  void BuildSnapshot();
};

}  // namespace abyss

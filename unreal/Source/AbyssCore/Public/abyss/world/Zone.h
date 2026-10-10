// ZoneRuntime: the live zone (map, grid, pathfinder, camps, exits, NPC placements, hidden areas, story decorations),
// zone entry / exit / transition, the pointer press priority chain, the interact action, the town portal.
// Spec: world-map-nav.md 3.4 (camps), 6.2-6.3 (NPC placement / safe zones via quests 6.2), 7.1 (press chain), 7.4
// (pointer-free actions: FindInteractTarget, InteractPrompt, town portal), 9.1 (zone entry order), 9.2 (exits), 9.3
// (state carried), 9.4 (town portal), 9.5 (death respawn position), 9.6 (save position / new game), 10.3 (hidden
// areas), 12 (interactables), 17 (tick order), 18 (API proposal); save-ui-input.md 1.4 (zone lifecycle);
// DECISIONS W3 (1.5 s channel cancelled by movement input, damage >= 10 % max HP or death; nearest camp), W7 (sealed
// gate to twilight_forest), W8 (walk-then-act; exits armed after > sqrt(6) tiles away), Q6 (tap NPC -> walk -> talk;
// touch Talk/Use button within 2.5 tiles), C7, I10 (Q22), D13 T13.
//
// Owner area: world. Runtime system (SimContext). Timers: TimerOwner::World (town portal T13, zone fade).
#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "abyss/base/Platform.h"
#include "abyss/base/Timers.h"
#include "abyss/base/Types.h"
#include "abyss/data/MapData.h"
#include "abyss/sim/SimTypes.h"
#include "abyss/world/Grid.h"
#include "abyss/world/Pathfinding.h"

namespace abyss {

struct SimContext;
struct Snapshot;
struct SaveData;

enum class WorldTimerKind : uint16_t { TownPortal = 1, ZoneTransition = 2 };

struct NpcPlacement {
  EntityId id = kNoEntity;
  std::string npcId;
  Vec2 pos;
  Vec2 facing{0, 1};
  bool field = false;  // field NPC (fieldNpcs) vs camp NPC
};

struct ExitState {
  MapExitDef def;
  bool armed = false;   // W8: hero has been > sqrt(6) tiles away since entering
  bool sealed = false;  // W7: shows the sealed gate, never transitions in milestone 1
};

struct InteractTarget {
  InteractKind kind = InteractKind::None;
  EntityId id = kNoEntity;
  Vec2 pos;
  std::string key;  // npc id / area id / ... (for handlers that are not entity based)
};

// A pending walk-then-act interaction (W8 / Q6 / Q22): fired when the hero arrives within range. Abandoned when any
// other movement order replaces the walk (HeroLocomotion::MoveGeneration), the target disappears, or the walk ends
// out of range.
struct PendingInteraction {
  bool active = false;
  InteractTarget target;
  double range = 0;
  uint32_t moveGeneration = 0;
};

class ABYSS_API ZoneRuntime {
 public:
  explicit ZoneRuntime(SimContext& ctx);
  ~ZoneRuntime();
  ZoneRuntime(const ZoneRuntime&) = delete;
  ZoneRuntime& operator=(const ZoneRuntime&) = delete;

  // ---- lifecycle (9.1) ----
  // Builds the zone runtime (grid via world/MapGen, pathfinder, camp + field NPC placements, exits) and places the hero
  // (`hasTarget` false -> playerStart). Unknown map -> the default map (world 9.1 step 1). GameSim's zone-entry sequence
  // (Private/sim/SimZone.cpp) calls this first, then every other system's OnZoneEnter in the world 9.1 order, then
  // publishes ZoneEnteredMsg / EvZone{Entered} / the zone log / SaveRequestMsg{ZoneEntered}.
  bool EnterZone(std::string_view mapId, bool hasTarget, Vec2 target);
  // changeZone (9.2): guard (session.transitioning), SaveRequestMsg{ZoneChange}, EvZone{TransitionBegan}; after the
  // ZoneTransition timer (zoneFadeInMs on the sim clock) GameSim runs the exit + entry sequences.
  void RequestZoneChange(std::string_view mapId, Vec2 target);
  // Drops the zone runtime (GameSim calls every system's OnZoneExit first, W12: EvZone{Exited} is emitted).
  void ExitZone();
  // The transition requested by RequestZoneChange once its timer fired (GameSim polls it).
  bool TakePendingTransition(std::string& mapId, Vec2& target);
  bool HasZone() const { return map_ != nullptr; }

  // ---- per step ----
  void Tick();  // exits (armed/sealed), pending interaction arrival, interact prompt (EvInteractPrompt on change)
  void OnTimer(const Timer& t);

  // ---- input ----
  // Pointer press on a world tile (7.1 chain with the port fixes): Secondary = town portal; then (alive hero only)
  // loot, NPC, hidden reward, event prop, monster, exit, ground (path + hold-to-move start with `pointerId`).
  // Out-of-range loot / NPCs / hidden rewards / event props are walked to and used on arrival (W5 / W8 / Q6 / Q22);
  // an exit is walked to and fires by proximity (W6 / W8).
  void OnPointerPress(Vec2 tile, PointerButton button, int32_t pointerId = 0);
  // Interact action (7.4): nearest in-range target, then its handler.
  bool Interact();
  // Interaction with a specific world entity (CmdInteract with a target): NPC, ground item, hidden reward, event prop;
  // walk-then-act when out of range (W8 / Q6 / Q22).
  bool InteractWith(EntityId target);
  // FindInteractTarget (7.4): the in-range candidate with the smallest distSq from the hero; ties go to the lower order
  // (loot, NPC, hidden reward, event puzzle prop), then list order. Kind None when nothing is in range.
  InteractTarget FindInteractTarget() const;
  // Town portal (9.4 + W3).
  PortalRefusal CanUseTownPortal() const;
  bool UseTownPortal();
  void CancelTownPortal();         // movement input, damage >= 10 % max HP, death (W3)
  // HeroMoveInputMsg (keyboard / stick / click-move, W3 / U10): cancels the channel when the data switch
  // world_constants townPortal.cancelOnMove is on (GameSim wiring).
  void OnHeroMoveInput();
  bool IsPortaling() const { return portalTimer_ != kNoTimer; }

  // ---- queries ----
  const MapDef& Map() const { return *map_; }
  const std::string& MapId() const;
  const ZoneGrid& Grid() const { return grid_; }
  const Pathfinder& Paths() const { return *pathfinder_; }
  bool Walkable(int32_t col, int32_t row) const { return grid_.Walkable(col, row); }
  std::span<const NpcPlacement> Npcs() const { return npcs_; }
  const NpcPlacement* FindNpc(std::string_view npcId) const;
  const NpcPlacement* FindNpcEntity(EntityId id) const;
  std::span<const ExitState> Exits() const { return exits_; }
  double SafeZoneRadius() const;
  bool InSafeZone(Vec2 p) const;           // any camp: distSq < safeR^2 (strict)
  bool NearCampfire(Vec2 p) const;         // within campfireRadiusTiles of a camp (regen x50)
  Vec2 CampPosition(int32_t index) const;  // camps[i] (fallback playerStart, then (3,3))
  Vec2 NearestCamp(Vec2 p) const;
  // findNearestWalkablePosition (save 3.7) on the live grid.
  bool NearestWalkableCamp(Vec2 p, Vec2& out) const;
  const PendingInteraction& Pending() const { return pending_; }
  const InteractTarget& Prompt() const { return prompt_; }
  Vec2 PortalDestination() const { return portalDestination_; }

  void FillSnapshot(Snapshot& out) const;

 private:
  // The live position and interact range of a target; false when it no longer exists.
  bool ResolveTarget(const InteractTarget& t, Vec2& pos, double& range) const;
  // Runs the handler of an in-range target (the 7.1 row's action). False when nothing happened.
  bool ActOn(const InteractTarget& t);
  // In range -> ActOn now; else walk there and act on arrival (W8). False when neither is possible.
  bool ActOrWalk(const InteractTarget& t);
  void TickExits();
  void TickPending();
  void TickPrompt();
  SimContext& ctx_;
  const MapDef* map_ = nullptr;
  ZoneGrid grid_;
  std::unique_ptr<Pathfinder> pathfinder_;
  std::vector<NpcPlacement> npcs_;
  std::vector<ExitState> exits_;
  PendingInteraction pending_;
  InteractTarget prompt_;
  TimerId portalTimer_ = kNoTimer;
  double portalStartMs_ = 0;
  Vec2 portalDestination_;
  TimerId transitionTimer_ = kNoTimer;
  std::string pendingZone_;
  Vec2 pendingTarget_;
  bool transitionReady_ = false;
};

}  // namespace abyss

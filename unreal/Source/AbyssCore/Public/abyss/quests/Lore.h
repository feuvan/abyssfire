// Lore pickups, the lore popup, hidden-area discovery records and their reward props, story decorations' tooltips.
// Spec: quests-story-ch1.md 1.9, 10.4 (Chapter 1 lore and exploration content); world-map-nav.md 10.3 (hidden areas:
// discovery, rewards, QUIRK W8 -> FIX: persist claimed reward indices, respawn unclaimed rewards of discovered areas on
// zone entry), 12.1 (lore pickups), 12.2 (story decorations); save-ui-input.md 3.2 (loreCollected,
// discoveredHiddenAreas), 7.12 (lore popup), DECISIONS Q5 (hidden-area rewards persist; chest overflow to the stash),
// W8 (walk-then-act).
//
// Owner area: quests+story+pets. Runtime system (SimContext). The zone geometry (where things are) comes from
// ZoneRuntime / MapDef; this system owns the collected / discovered / claimed records, the pickup and reward props and
// the rewards. Rewards go through RewardService (gold pile: GoldReason::HiddenReward; chest: OverflowPolicy::Stash).
// RNG: RngStream::World (chest item).
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "abyss/base/Platform.h"
#include "abyss/base/Types.h"
#include "abyss/data/MapData.h"

namespace abyss {

struct SimContext;
struct Snapshot;
struct SaveData;

struct LorePickup {
  EntityId id = kNoEntity;
  std::string loreId;
  Vec2 pos;
};

// One reward of a discovered hidden area lying in the world (world 10.3). Spawned on discovery and, while unclaimed, on
// every later entry of the zone (Q5). The interact chain resolves InteractKind::HiddenReward by this entity id.
struct HiddenRewardProp {
  EntityId id = kNoEntity;
  std::string areaId;
  int32_t rewardIndex = 0;  // index in HiddenAreaDef::rewards; save key "<areaId>#<rewardIndex>"
  HiddenRewardType type = HiddenRewardType::Chest;
  Vec2 pos;
};

// The lore popup (save-ui-input 7.12): a core-owned modal (PanelId::LoreText) opened when a lore pickup is collected.
// UE closes it (close button, backdrop, its 8 s presentation auto-close) with CmdClosePanel{LoreText}; a newer pickup
// replaces the text (FIX quests Q15: UE restarts its timer per open).
struct LoreTextState {
  bool open = false;
  std::string loreId;
};

class ABYSS_API LoreSystem {
 public:
  explicit LoreSystem(SimContext& ctx);

  // Spawns the zone's uncollected lore pickups and the unclaimed reward props of its discovered hidden areas (Q5),
  // emitting EvEntitySpawned{EntityKind::Prop} for each.
  void OnZoneEnter();
  void OnZoneExit();  // drops the props (no events: the zone is unloading) and closes the lore popup
  // Per step: lore pickup in range (walk-then-act), hidden-area discovery (world 10.3: all five check points explored
  // in this visit) -> record, EvHiddenAreaDiscovered, log, banner, SpawnRewardProps.
  void Tick();
  // Explicit interaction on a pickup / reward prop (pointer chain, touch Use button; ZoneRuntime walks first).
  bool Collect(EntityId pickup);
  // Claims one reward: chest -> RewardService::GrantItem(OverflowPolicy::Stash, ItemSource::HiddenReward), gold pile ->
  // ChangeGold(parseInt(value || "100"), GoldReason::HiddenReward), lore -> log only; records "<areaId>#<index>",
  // despawns the prop (EvEntityDespawned{Collected}). False for an unknown prop / already claimed / Dying hero.
  bool ClaimHiddenReward(EntityId prop);
  bool ClaimHiddenReward(std::string_view areaId, int32_t rewardIndex);

  bool IsCollected(std::string_view loreId) const;
  bool IsDiscovered(std::string_view areaId) const;
  bool IsRewardClaimed(std::string_view areaId, int32_t rewardIndex) const;
  static std::string RewardKey(std::string_view areaId, int32_t rewardIndex);  // "<areaId>#<rewardIndex>"
  const std::vector<std::string>& Collected() const { return collected_; }
  const std::vector<std::string>& Discovered() const { return discovered_; }
  const std::vector<LorePickup>& Pickups() const { return pickups_; }
  const std::vector<HiddenRewardProp>& HiddenRewards() const { return rewardProps_; }
  const HiddenRewardProp* FindHiddenReward(EntityId prop) const;

  // Lore popup (core-owned modal).
  const LoreTextState& Text() const { return text_; }
  void CloseText();

  void FillSnapshot(Snapshot& out) const;
  void WriteSave(SaveData& out) const;
  void ReadSave(const SaveData& in);

 private:
  // Spawns one prop per unclaimed reward of `area` (skips rewards already lying in the world).
  void SpawnRewardProps(const HiddenAreaDef& area);
  void RemoveRewardProp(EntityId prop);

  SimContext& ctx_;
  std::vector<std::string> collected_;
  std::vector<std::string> discovered_;
  std::vector<std::string> rewardsClaimed_;  // Q5 "<areaId>#<rewardIndex>"
  std::vector<LorePickup> pickups_;
  std::vector<HiddenRewardProp> rewardProps_;
  LoreTextState text_;
};

}  // namespace abyss

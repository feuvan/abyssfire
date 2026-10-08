// Lore pickups, the lore popup and hidden-area records / reward props (quests-story-ch1.md 1.9, 10.4; world-map-nav.md
// 10.3, 12.1-12.2; save-ui-input.md 7.12; Q5). Owner area: quests+story+pets. Zone-entry spawning, reward props,
// claiming and the save records are implemented; per-step pickup / discovery (Tick) and Collect are STUBS.
#include "abyss/base/Platform.h"

#include "abyss/quests/Lore.h"

#include <algorithm>
#include <charconv>

#include "abyss/base/Assert.h"
#include "abyss/base/StrUtil.h"
#include "abyss/data/DataStore.h"
#include "abyss/hero/Hero.h"
#include "abyss/hero/Rewards.h"
#include "abyss/items/LootGen.h"
#include "abyss/save/SaveData.h"
#include "abyss/sim/SimContext.h"
#include "abyss/sim/Snapshot.h"
#include "abyss/world/Zone.h"

namespace abyss {

namespace {
bool ContainsId(const std::vector<std::string>& v, std::string_view id) {
  return std::find(v.begin(), v.end(), id) != v.end();
}

// JS parseInt(value || "100") for the gold pile (world 10.3): leading decimal digits; "" -> 100; no digits -> 0 (the
// web would add NaN - a data bug the port does not reproduce).
int64_t GoldPileAmount(std::string_view value) {
  if (value.empty()) return 100;
  size_t i = 0;
  while (i < value.size() && (value[i] == ' ' || value[i] == '\t')) ++i;
  int64_t out = 0;
  const char* b = value.data() + i;
  const auto res = std::from_chars(b, value.data() + value.size(), out);
  return res.ptr == b ? 0 : (std::max)(int64_t{0}, out);
}

ItemQuality ChestQuality(std::string_view value) {
  if (value == "legendary") return ItemQuality::Legendary;
  if (value == "rare") return ItemQuality::Rare;
  return ItemQuality::Magic;
}
}  // namespace

LoreSystem::LoreSystem(SimContext& ctx) : ctx_(ctx) {}

std::string LoreSystem::RewardKey(std::string_view areaId, int32_t rewardIndex) {
  return StrCat(areaId, "#", rewardIndex);
}

void LoreSystem::OnZoneEnter() {
  pickups_.clear();
  rewardProps_.clear();
  const ZoneRuntime* zone = ctx_.sys.zone;
  if (zone == nullptr || !zone->HasZone()) return;
  const MapDef& map = zone->Map();
  for (const LoreEntryDef* e : ctx_.data.Lore().ForZone(map.id)) {
    if (IsCollected(e->id)) continue;
    LorePickup p;
    p.id = ctx_.ids.Next();
    p.loreId = e->id;
    p.pos = e->pos.Center();
    ctx_.events.Emit(EvEntitySpawned{p.id, EntityKind::Prop, "lore:" + e->id, e->spriteType, p.pos, Vec2(1, 0), 1.0});
    pickups_.push_back(std::move(p));
  }
  // Q5: unclaimed rewards of areas discovered in an earlier visit lie where they were.
  for (const HiddenAreaDef& area : map.hiddenAreas) {
    if (IsDiscovered(area.id)) SpawnRewardProps(area);
  }
}

void LoreSystem::OnZoneExit() {
  pickups_.clear();
  rewardProps_.clear();
  CloseText();
}

void LoreSystem::SpawnRewardProps(const HiddenAreaDef& area) {
  for (size_t i = 0; i < area.rewards.size(); ++i) {
    const int32_t index = static_cast<int32_t>(i);
    if (IsRewardClaimed(area.id, index)) continue;
    const bool present = std::any_of(rewardProps_.begin(), rewardProps_.end(), [&](const HiddenRewardProp& p) {
      return p.areaId == area.id && p.rewardIndex == index;
    });
    if (present) continue;
    const HiddenRewardDef& r = area.rewards[i];
    HiddenRewardProp prop;
    prop.id = ctx_.ids.Next();
    prop.areaId = area.id;
    prop.rewardIndex = index;
    prop.type = r.type;
    prop.pos = r.pos.Center();
    const std::string type(EnumName(r.type));
    ctx_.events.Emit(EvEntitySpawned{prop.id, EntityKind::Prop, "hidden_reward:" + type, "hidden_" + type, prop.pos,
                                     Vec2(1, 0), 1.0});
    rewardProps_.push_back(std::move(prop));
  }
}

void LoreSystem::Tick() { ABYSS_UNIMPLEMENTED(); }

bool LoreSystem::Collect(EntityId pickup) {
  ABYSS_UNIMPLEMENTED();
  return false;
}

const HiddenRewardProp* LoreSystem::FindHiddenReward(EntityId prop) const {
  for (const HiddenRewardProp& p : rewardProps_) {
    if (p.id == prop) return &p;
  }
  return nullptr;
}

void LoreSystem::RemoveRewardProp(EntityId prop) {
  for (size_t i = 0; i < rewardProps_.size(); ++i) {
    if (rewardProps_[i].id != prop) continue;
    rewardProps_.erase(rewardProps_.begin() + static_cast<std::ptrdiff_t>(i));
    ctx_.events.Emit(EvEntityDespawned{prop, EntityKind::Prop, DespawnReason::Collected});
    return;
  }
}

bool LoreSystem::ClaimHiddenReward(std::string_view areaId, int32_t rewardIndex) {
  for (const HiddenRewardProp& p : rewardProps_) {
    if (p.areaId == areaId && p.rewardIndex == rewardIndex) return ClaimHiddenReward(p.id);
  }
  return false;
}

bool LoreSystem::ClaimHiddenReward(EntityId propId) {
  const HiddenRewardProp* found = FindHiddenReward(propId);
  if (found == nullptr) return false;
  if (ctx_.sys.hero == nullptr || ctx_.sys.hero->Life() != HeroLife::Alive) return false;  // 5.1.1
  if (IsRewardClaimed(found->areaId, found->rewardIndex)) return false;
  const HiddenRewardProp prop = *found;  // copy: RemoveRewardProp erases the entry
  const ZoneRuntime* zone = ctx_.sys.zone;
  const HiddenAreaDef* area = nullptr;
  if (zone != nullptr && zone->HasZone()) {
    for (const HiddenAreaDef& a : zone->Map().hiddenAreas) {
      if (a.id == prop.areaId) area = &a;
    }
  }
  if (area == nullptr || prop.rewardIndex < 0 || static_cast<size_t>(prop.rewardIndex) >= area->rewards.size()) {
    return false;
  }
  const HiddenRewardDef& def = area->rewards[static_cast<size_t>(prop.rewardIndex)];
  RewardService* rewards = ctx_.sys.rewards;
  switch (def.type) {
    case HiddenRewardType::GoldPile:
      if (rewards != nullptr) rewards->ChangeGold(GoldPileAmount(def.value), GoldReason::HiddenReward);
      break;
    case HiddenRewardType::Chest: {
      // generateEquipment(levelRange[1], quality) straight into the bag; Q5: overflow to the stash.
      if (rewards != nullptr && ctx_.sys.inventory != nullptr) {
        const LootContext lc{&ctx_.data, &ctx_.Rand(RngStream::World), &ctx_.sys.inventory->Uids()};
        std::optional<ItemInstance> item = GenerateEquipment(lc, zone->Map().levelMax, ChestQuality(def.value));
        if (item.has_value()) rewards->GrantItem(*item, OverflowPolicy::Stash, ItemSource::HiddenReward);
      }
      break;
    }
    case HiddenRewardType::Lore:
    case HiddenRewardType::RareSpawn:
      break;  // lore: log only (world 10.3); rare_spawn: later milestones
  }
  rewardsClaimed_.push_back(RewardKey(prop.areaId, prop.rewardIndex));
  RemoveRewardProp(prop.id);
  return true;
}

bool LoreSystem::IsCollected(std::string_view loreId) const { return ContainsId(collected_, loreId); }

bool LoreSystem::IsDiscovered(std::string_view areaId) const { return ContainsId(discovered_, areaId); }

bool LoreSystem::IsRewardClaimed(std::string_view areaId, int32_t rewardIndex) const {
  return ContainsId(rewardsClaimed_, RewardKey(areaId, rewardIndex));
}

void LoreSystem::CloseText() { text_ = LoreTextState{}; }

void LoreSystem::FillSnapshot(Snapshot& out) const {
  out.lore = this;
  out.loreText = &text_;
  for (const LorePickup& p : pickups_) {
    WorldMarkerView v;
    v.id = p.id;
    v.kind = MarkerKind::LorePickup;
    v.pos = p.pos;
    v.key = p.loreId;
    out.markers.push_back(std::move(v));
  }
  for (const HiddenRewardProp& p : rewardProps_) {
    WorldMarkerView v;
    v.id = p.id;
    v.kind = MarkerKind::HiddenReward;
    v.pos = p.pos;
    v.key = RewardKey(p.areaId, p.rewardIndex);
    out.markers.push_back(std::move(v));
  }
}

void LoreSystem::WriteSave(SaveData& out) const {
  out.loreCollected = collected_;
  out.discoveredHiddenAreas = discovered_;
  out.hiddenRewardsClaimed = rewardsClaimed_;
}

void LoreSystem::ReadSave(const SaveData& in) {
  collected_ = in.loreCollected;
  discovered_ = in.discoveredHiddenAreas;
  rewardsClaimed_ = in.hiddenRewardsClaimed;
}

}  // namespace abyss

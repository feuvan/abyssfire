// Lore pickups, the lore popup and hidden-area records / reward props (quests-story-ch1.md 1.9, 10.4; world-map-nav.md
// 10.3, 12.1-12.2; save-ui-input.md 7.12; Q5). Owner area: quests+story+pets.
#include "abyss/base/Platform.h"

#include "abyss/quests/Lore.h"

#include <algorithm>
#include <charconv>
#include <cmath>

#include "abyss/base/I18n.h"
#include "abyss/base/Math.h"
#include "abyss/base/StrUtil.h"
#include "abyss/data/DataStore.h"
#include "abyss/hero/Hero.h"
#include "abyss/hero/Rewards.h"
#include "abyss/items/LootGen.h"
#include "abyss/save/SaveData.h"
#include "abyss/sim/SimContext.h"
#include "abyss/sim/Snapshot.h"
#include "abyss/world/Exploration.h"
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
  decorFocus_.clear();
  decorTooltip_ = false;
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

void LoreSystem::Tick() {
  const ZoneRuntime* zone = ctx_.sys.zone;
  const Hero* hero = ctx_.sys.hero;
  if (zone == nullptr || !zone->HasZone() || hero == nullptr) return;
  const Vec2 hp = hero->Position();
  const bool alive = hero->Life() == HeroLife::Alive;

  // 12.1 lore pickups: distSq(hero, tile) <= pickupRangeSq (4).
  if (alive) {
    const double rangeSq = ctx_.data.Lore().pickupRangeSq;
    for (size_t i = pickups_.size(); i-- > 0;) {
      if (i >= pickups_.size()) continue;  // a collection may have shrunk the list
      if (DistSq(hp, pickups_[i].pos) <= rangeSq) Collect(pickups_[i].id);
    }
  }

  // 10.3 hidden areas: the four bound corners and the centre explored during this visit.
  if (ctx_.sys.exploration != nullptr) {
    const ExplorationGrid& grid = ctx_.sys.exploration->Grid();
    for (const HiddenAreaDef& area : zone->Map().hiddenAreas) {
      if (IsDiscovered(area.id)) continue;
      double c0 = area.center.col - area.radius, r0 = area.center.row - area.radius;
      double c1 = area.center.col + area.radius, r1 = area.center.row + area.radius;
      if (area.hasBounds) {
        c0 = area.boundsStart.col;
        r0 = area.boundsStart.row;
        c1 = area.boundsEnd.col;
        r1 = area.boundsEnd.row;
      }
      // A non-integral point reads as unexplored (the web indexes exploredTiles with it -> undefined).
      const auto explored = [&grid](double c, double r) {
        if (c != std::floor(c) || r != std::floor(r)) return false;
        return grid.IsExplored(static_cast<int32_t>(c), static_cast<int32_t>(r));
      };
      if (!explored(c0, r0) || !explored(c1, r0) || !explored(c0, r1) || !explored(c1, r1) ||
          !explored(area.center.col, area.center.row)) {
        continue;
      }
      discovered_.push_back(area.id);
      ctx_.events.Emit(EvHiddenAreaDiscovered{area.id});
      const std::string base = "data.hiddenArea." + area.id;
      ctx_.events.Log(MakeLoc("zone.hiddenArea.discovered", {KeyArg("areaName", base + ".name")}), LogType::System);
      ctx_.events.Emit(EvBanner{BannerKind::Discovery, MakeLoc(base + ".discovery"), LocText{}});
      SpawnRewardProps(area);
    }
  }

  // 12.2 story decorations: nearest with distSq <= 9 shows its label; tooltip when that one is within distSq 2.
  std::string focus;
  bool tooltip = false;
  double best = 0;
  for (const StoryDecorationDef& d : zone->Map().storyDecorations) {
    const double dsq = DistSq(hp, d.pos.Center());
    if (dsq > 9) continue;
    if (focus.empty() || dsq < best) {
      focus = d.id;
      best = dsq;
    }
  }
  if (!focus.empty()) tooltip = best <= 2;
  if (focus != decorFocus_ || tooltip != decorTooltip_) {
    decorFocus_ = focus;
    decorTooltip_ = tooltip;
    ctx_.events.Emit(EvStoryDecorFocus{decorFocus_, decorTooltip_});
  }
}

// checkLorePickup (12.1): record (saved), LORE_COLLECTED (popup = the core-owned LoreText modal; Q15: a newer pickup
// replaces the text), log zone.lore.discovered {loreName} (Q11: key), the prop fades (EvEntityDespawned{Collected}).
bool LoreSystem::Collect(EntityId pickup) {
  if (ctx_.sys.hero == nullptr || ctx_.sys.hero->Life() != HeroLife::Alive) return false;
  for (size_t i = 0; i < pickups_.size(); ++i) {
    if (pickups_[i].id != pickup) continue;
    const LorePickup p = pickups_[i];
    pickups_.erase(pickups_.begin() + static_cast<std::ptrdiff_t>(i));
    if (!IsCollected(p.loreId)) collected_.push_back(p.loreId);
    text_.open = true;
    text_.loreId = p.loreId;
    ctx_.events.Emit(EvLoreCollected{p.loreId});
    ctx_.events.Log(MakeLoc("zone.lore.discovered", {KeyArg("loreName", "data.lore." + p.loreId + ".name")}),
                    LogType::System);
    ctx_.events.Emit(EvEntityDespawned{p.id, EntityKind::Prop, DespawnReason::Collected});
    ctx_.bus.Publish(LoreCollectedMsg{p.loreId});
    return true;
  }
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

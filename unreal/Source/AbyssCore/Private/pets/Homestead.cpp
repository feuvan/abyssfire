// Homestead / Ember Tower state (quests-story-ch1.md 4.4, 4.6, 4.7; save-ui-input.md 3.2-3.3). Web:
// src/data/homestead.ts (pure helpers), src/systems/HomesteadTower.ts + HomesteadSystem.ts (state), EmberTower.ts (hooks
// and tower actions).
#include "abyss/base/Platform.h"

#include "abyss/pets/Homestead.h"

#include <algorithm>
#include <cmath>
#include <iterator>

#include "abyss/base/I18n.h"
#include "abyss/base/Math.h"
#include "abyss/base/StrUtil.h"
#include "abyss/data/DataStore.h"
#include "abyss/hero/Hero.h"
#include "abyss/hero/Rewards.h"
#include "abyss/items/Inventory.h"
#include "abyss/items/LootGen.h"
#include "abyss/pets/PetSystem.h"
#include "abyss/quests/QuestSystem.h"
#include "abyss/save/SaveData.h"
#include "abyss/sim/SimContext.h"
#include "abyss/sim/Snapshot.h"

namespace abyss {

namespace {

// Building ids the web's rules name directly (homestead.ts BUILDINGS; HomesteadTower.ts).
constexpr std::string_view kHsHerbGarden = "herb_garden";
constexpr std::string_view kHsTrainingGround = "training_ground";
constexpr std::string_view kHsAltar = "altar";
constexpr std::string_view kHsGemWorkshop = "gem_workshop";
constexpr int32_t kHsTrainingBonusPerLevel = 5;  // getTrainingGroundBonus: level x 5 (%)
// buildingStage: a five-level wing thrives from Lv3, a shorter one (the altar) from Lv2.
constexpr int32_t kHsThrivingLongWing = 3, kHsThrivingShortWing = 2, kHsLongWingLevels = 5;
// Gem workshop (homestead.ts maxCombineTier / gemCombineGold).
constexpr int32_t kHsMaxGemTier = 5;
constexpr int64_t kHsGemGoldPerTierSq = 40;
// Expeditions (rollExpeditionReward).
constexpr std::string_view kHsLongExpedition = "long";
constexpr int32_t kHsLongEmbersBase = 10, kHsLongEmbersPerLevel = 4, kHsShortEmbersBase = 4, kHsShortEmbersPerLevel = 2;
constexpr double kHsLongGoldPerLevel = 60, kHsShortGoldPerLevel = 25;
constexpr double kHsLongGoldPerHeroLevel = 12, kHsShortGoldPerHeroLevel = 5;
constexpr double kHsGoldJitterMin = 0.8, kHsGoldJitterSpan = 0.4;
constexpr std::string_view kHsGemLines[] = {"ruby", "sapphire", "emerald", "topaz"};
constexpr int32_t kHsExpeditionGemMaxTier = 3, kHsExpeditionGemLevelsPerTier = 15;
constexpr int32_t kHsLongLeyFruit = 2;
constexpr double kHsShortFruitChance = 0.5;
constexpr int32_t kHsShortLargePotionLevel = 25;
// Blessings (blessingCost / blessingStats).
constexpr int32_t kHsBlessingCostBase = 10, kHsBlessingCostPerLevel = 5;
constexpr double kHsBlessingStatsPerLevel = 0.5;

bool HsContains(std::span<const std::string> list, std::string_view id) {
  return std::find(list.begin(), list.end(), id) != list.end();
}

I18nArg HsBuildingNameArg(const DataStore& data, const BuildingDef* def, std::string_view id) {
  const std::string key = StrCat("data.homestead.", id, ".name");
  if (data.Strings().Has(key)) return KeyArg("name", key);
  return I18nArg{"name", def != nullptr ? def->name : std::string(id), false};
}

}  // namespace

// =====================================================================================================================
// pure helpers (homestead.ts)
// =====================================================================================================================

int32_t EmbersForKill(const HomesteadTables& t, bool elite, bool isMiniBoss, int32_t eliteAffixCount) {
  if (elite) return t.embersKillElite;
  if (isMiniBoss) return t.embersKillMiniBoss;
  return eliteAffixCount > 0 ? t.embersKillAffixedPerAffix : t.embersKillPlain;
}

int32_t EmbersForQuest(const HomesteadTables& t, QuestCategory category, bool hasRewardEmbers, int32_t rewardEmbers) {
  if (hasRewardEmbers) return rewardEmbers;
  return category == QuestCategory::Main ? t.embersQuestMain : t.embersQuestSide;
}

int32_t GardenInterval(const HomesteadTables& t, int32_t level) {
  return (std::max)(t.gardenIntervalMin, t.gardenIntervalBase + t.gardenIntervalPerLevel * level);
}

int32_t GardenCapacity(const HomesteadTables& t, int32_t level) {
  return level <= 0 ? 0 : t.gardenCapacityBase + t.gardenCapacityPerLevel * level;
}

std::string RollGardenYield(const HomesteadTables& t, int32_t level, Rng& rng) {
  if (rng.Float01() < t.gardenLeyFruitBase + t.gardenLeyFruitPerLevel * static_cast<double>(level)) return t.leyFruitId;
  const bool hp = rng.Float01() < t.gardenHpShare;
  if (hp) {
    if (level >= t.gardenHpLargeFromLevel) return t.gardenHpPotionL;
    return level >= t.gardenMediumFromLevel ? t.gardenHpPotionM : t.gardenHpPotionS;
  }
  return level >= t.gardenMediumFromLevel ? t.gardenMpPotionM : t.gardenMpPotionS;
}

int32_t BuildingStage(const BuildingDef& def, int32_t level, bool unlocked) {
  if (!unlocked || level <= 0) return 0;
  const int32_t thriving = def.maxLevel >= kHsLongWingLevels ? kHsThrivingLongWing : kHsThrivingShortWing;
  return level >= thriving ? 2 : 1;
}

std::string NextGemId(const DataStore& data, std::string_view gemId) {
  // /^g_([a-z]+)_(\d)$/
  if (gemId.size() < 5 || gemId.substr(0, 2) != "g_") return {};
  const size_t sep = gemId.rfind('_');
  if (sep == std::string_view::npos || sep <= 2 || sep + 2 != gemId.size()) return {};
  const std::string_view line = gemId.substr(2, sep - 2);
  for (char c : line) {
    if (c < 'a' || c > 'z') return {};
  }
  const char digit = gemId[sep + 1];
  if (digit < '0' || digit > '9') return {};
  const std::string next = StrCat("g_", line, "_", static_cast<int32_t>(digit - '0') + 1);
  const ItemBaseDef* base = data.FindItemBase(next);
  return base != nullptr && base->isGem ? next : std::string();
}

int32_t MaxCombineTier(int32_t workshopLevel) {
  return workshopLevel <= 0 ? 0 : (std::min)(kHsMaxGemTier, workshopLevel + 1);
}

int64_t GemCombineGold(int32_t targetTier) {
  return kHsGemGoldPerTierSq * static_cast<int64_t>(targetTier) * targetTier;
}

GemCombineBlock GemCombineBlockFor(const DataStore& data, std::string_view gemId, int32_t have, int32_t workshopLevel,
                                   int32_t playerLevel, int64_t gold) {
  const std::string next = NextGemId(data, gemId);
  if (next.empty()) return GemCombineBlock::NoNext;
  const ItemBaseDef* base = data.FindItemBase(next);
  const int32_t tier = base->gemTier;
  if (tier > MaxCombineTier(workshopLevel)) return GemCombineBlock::Workshop;
  if (base->levelReq > playerLevel) return GemCombineBlock::Level;
  if (have < data.Homestead().gemCombineCount) return GemCombineBlock::Count;
  if (gold < GemCombineGold(tier)) return GemCombineBlock::Gold;
  return GemCombineBlock::None;
}

ExpeditionReward RollExpeditionReward(const HomesteadTables& t, std::string_view optionId, int32_t postLevel,
                                      int32_t playerLevel, Rng& rng) {
  const bool isLong = optionId == kHsLongExpedition;
  const int32_t lv = (std::max)(1, postLevel);
  ExpeditionReward r;
  r.embers = isLong ? kHsLongEmbersBase + kHsLongEmbersPerLevel * lv : kHsShortEmbersBase + kHsShortEmbersPerLevel * lv;
  const double baseGold = (isLong ? kHsLongGoldPerLevel : kHsShortGoldPerLevel) * static_cast<double>(lv);
  const double jitter = kHsGoldJitterMin + rng.Float01() * kHsGoldJitterSpan;
  const double perHero = static_cast<double>(playerLevel) * (isLong ? kHsLongGoldPerHeroLevel : kHsShortGoldPerHeroLevel);
  r.gold = static_cast<int64_t>(JsRound(baseGold + perHero * jitter));
  if (isLong) {
    const size_t n = std::size(kHsGemLines);
    const size_t pick = static_cast<size_t>(std::floor(rng.Float01() * static_cast<double>(n))) % n;
    const int32_t tier = (std::min)(kHsExpeditionGemMaxTier, 1 + playerLevel / kHsExpeditionGemLevelsPerTier);
    r.items.emplace_back(StrCat("g_", kHsGemLines[pick], "_", tier), 1);
    r.items.emplace_back(t.leyFruitId, kHsLongLeyFruit);
  } else {
    const bool fruit = rng.Float01() < kHsShortFruitChance;
    r.items.emplace_back(fruit ? t.leyFruitId
                               : (playerLevel >= kHsShortLargePotionLevel ? t.gardenHpPotionL : t.gardenHpPotionM),
                         1);
  }
  return r;
}

int32_t BlessingCost(int32_t altarLevel) {
  return kHsBlessingCostBase + kHsBlessingCostPerLevel * (std::max)(0, altarLevel - 1);
}

StatBag BlessingStatsFor(const HomesteadTables& t, std::string_view id, int32_t altarLevel) {
  StatBag out;
  for (const BlessingDef& b : t.blessings) {
    if (b.id != id) continue;
    const double mul = 1.0 + kHsBlessingStatsPerLevel * static_cast<double>((std::max)(0, altarLevel - 1));
    for (const StatValue& sv : b.stats.Items()) out.Set(sv.stat, JsRound(sv.value * mul));
    break;
  }
  return out;
}

// =====================================================================================================================
// HomesteadTower
// =====================================================================================================================

HomesteadTower::HomesteadTower(const HomesteadTables& t) : t_(&t) {}

int32_t HomesteadTower::BuildingLevel(std::string_view buildingId) const {
  for (const auto& [id, level] : buildings_) {
    if (id == buildingId) return level;
  }
  return 0;
}

void HomesteadTower::SetBuildingLevel(std::string_view buildingId, int32_t level) {
  for (auto& [id, lv] : buildings_) {
    if (id == buildingId) {
      lv = level;
      return;
    }
  }
  buildings_.emplace_back(std::string(buildingId), level);
}

StatBag HomesteadTower::BuildingBonuses() const {
  StatBag out;
  for (const auto& [id, level] : buildings_) {
    const BuildingDef* def = t_->FindBuilding(id);
    if (def == nullptr) continue;
    for (const StatValue& b : def->bonusPerLevel) out.Add(b.stat, b.value * static_cast<double>(level));
  }
  return out;
}

int32_t HomesteadTower::TrainingGroundBonus() const {
  return BuildingLevel(kHsTrainingGround) * kHsTrainingBonusPerLevel;
}

bool HomesteadTower::UpgradeCost(std::string_view buildingId, BuildingLevelCost& out) const {
  const BuildingDef* def = t_->FindBuilding(buildingId);
  if (def == nullptr) return false;
  const int32_t lv = BuildingLevel(buildingId);
  if (lv >= def->maxLevel || lv < 0 || static_cast<size_t>(lv) >= def->costPerLevel.size()) return false;
  out = def->costPerLevel[static_cast<size_t>(lv)];
  return true;
}

bool HomesteadTower::CanUpgrade(std::string_view buildingId, int64_t gold) const {
  return CanUpgrade(buildingId, gold, embers_);
}

bool HomesteadTower::CanUpgrade(std::string_view buildingId, int64_t gold, int32_t embers) const {
  BuildingLevelCost cost;
  if (!UpgradeCost(buildingId, cost) || !IsBuildingUnlocked(buildingId)) return false;
  return gold >= cost.gold && embers >= cost.embers;
}

int64_t HomesteadTower::Upgrade(std::string_view buildingId) {
  BuildingLevelCost cost;
  if (!UpgradeCost(buildingId, cost) || !IsBuildingUnlocked(buildingId) || embers_ < cost.embers) return 0;
  const int32_t lv = BuildingLevel(buildingId);
  embers_ -= cost.embers;
  SetBuildingLevel(buildingId, lv + 1);
  return cost.gold;
}

std::vector<std::string> HomesteadTower::SyncUnlocks(std::span<const std::string> turnedInQuests) {
  std::vector<std::string> before;
  for (const BuildingDef& b : t_->buildings) {
    if (IsBuildingUnlocked(b.id)) before.push_back(b.id);
  }
  turnedIn_.assign(turnedInQuests.begin(), turnedInQuests.end());
  std::vector<std::string> fresh;
  for (const BuildingDef& b : t_->buildings) {
    if (b.unlockQuest.empty() || !IsBuildingUnlocked(b.id)) continue;
    if (BuildingLevel(b.id) <= 0) SetBuildingLevel(b.id, 1);  // the ally restores the wing: Lv1 for free
    if (std::find(before.begin(), before.end(), b.id) == before.end()) fresh.push_back(b.id);
  }
  return fresh;
}

bool HomesteadTower::TowerUnlocked() const { return HsContains(turnedIn_, t_->towerUnlockQuest); }

bool HomesteadTower::IsBuildingUnlocked(std::string_view buildingId) const {
  const BuildingDef* def = t_->FindBuilding(buildingId);
  if (def == nullptr) return false;
  return def->unlockQuest.empty() || HsContains(turnedIn_, def->unlockQuest);
}

int32_t HomesteadTower::AddEmbers(int32_t n) {
  const int32_t add = (std::max)(0, n);
  embers_ += add;
  return add;
}

int32_t HomesteadTower::GardenStockCount() const {
  int32_t n = 0;
  for (const auto& [id, count] : garden_.stock) n += count;
  return n;
}

std::string HomesteadTower::OnKillGarden(Rng& rng) {
  const int32_t lv = BuildingLevel(kHsHerbGarden);
  if (lv <= 0 || !IsBuildingUnlocked(kHsHerbGarden)) return {};
  if (GardenStockCount() >= GardenCapacity(*t_, lv)) return {};  // full: progress does not advance
  ++garden_.progress;
  if (garden_.progress < GardenInterval(*t_, lv)) return {};
  garden_.progress = 0;
  std::string id = RollGardenYield(*t_, lv, rng);
  ReturnToGarden(id, 1);
  return id;
}

std::vector<std::pair<std::string, int32_t>> HomesteadTower::Harvest() {
  std::vector<std::pair<std::string, int32_t>> out = std::move(garden_.stock);
  garden_.stock.clear();
  return out;
}

void HomesteadTower::ReturnToGarden(std::string_view itemId, int32_t count) {
  if (count <= 0) return;
  for (auto& [id, n] : garden_.stock) {
    if (id == itemId) {
      n += count;
      return;
    }
  }
  garden_.stock.emplace_back(std::string(itemId), count);
}

ExpeditionBlock HomesteadTower::ExpeditionBlockFor(std::string_view petId, std::span<const std::string> ownedPets,
                                                   std::string_view activePet, std::string_view optionId) const {
  if (BuildingLevel(kHsTrainingGround) <= 0 || !IsBuildingUnlocked(kHsTrainingGround)) return ExpeditionBlock::Locked;
  if (expedition_.has_value()) return ExpeditionBlock::Busy;
  bool known = false;
  for (const ExpeditionOptionDef& o : t_->expeditionOptions) known = known || o.id == optionId;
  if (!known) return ExpeditionBlock::Option;
  if (petId.empty() || !HsContains(ownedPets, petId)) return ExpeditionBlock::NoPet;
  if (petId == activePet) return ExpeditionBlock::Active;
  return ExpeditionBlock::None;
}

bool HomesteadTower::SendExpedition(std::string_view petId, std::string_view optionId,
                                    std::span<const std::string> ownedPets, std::string_view activePet) {
  if (ExpeditionBlockFor(petId, ownedPets, activePet, optionId) != ExpeditionBlock::None) return false;
  for (const ExpeditionOptionDef& o : t_->expeditionOptions) {
    if (o.id != optionId) continue;
    expedition_ = ExpeditionState{std::string(petId), o.id, 0, o.killsRequired, o.durationMs};
    return true;
  }
  return false;
}

bool HomesteadTower::IsPetAway(std::string_view petId) const {
  return expedition_.has_value() && expedition_->petId == petId;
}

bool HomesteadTower::ExpeditionDone() const {
  return expedition_.has_value() && (expedition_->kills >= expedition_->killsRequired || expedition_->remainingMs <= 0);
}

std::optional<ClaimedExpedition> HomesteadTower::ClaimExpedition(int32_t playerLevel, Rng& rng) {
  if (!expedition_.has_value() || !ExpeditionDone()) return std::nullopt;
  ClaimedExpedition c;
  c.reward = RollExpeditionReward(*t_, expedition_->optionId, BuildingLevel(kHsTrainingGround), playerLevel, rng);
  c.petId = expedition_->petId;
  expedition_.reset();
  embers_ += c.reward.embers;
  return c;
}

bool HomesteadTower::OnKillExpedition() {
  if (!expedition_.has_value() || ExpeditionDone()) return false;
  ++expedition_->kills;
  return ExpeditionDone();
}

BlessingBlock HomesteadTower::BlessingBlockFor(std::string_view blessingId) const {
  const int32_t lv = BuildingLevel(kHsAltar);
  if (lv <= 0 || !IsBuildingUnlocked(kHsAltar)) return BlessingBlock::Locked;
  bool known = false;
  for (const BlessingDef& b : t_->blessings) known = known || b.id == blessingId;
  if (!known) return BlessingBlock::Unknown;
  if (embers_ < BlessingCost(lv)) return BlessingBlock::Embers;
  return BlessingBlock::None;
}

bool HomesteadTower::BuyBlessing(std::string_view blessingId) {
  if (BlessingBlockFor(blessingId) != BlessingBlock::None) return false;
  const int32_t lv = BuildingLevel(kHsAltar);
  embers_ -= BlessingCost(lv);
  blessing_ = BlessingState{std::string(blessingId), lv, t_->blessingDurationMs};
  return true;
}

StatBag HomesteadTower::BlessingStats() const {
  return blessing_.has_value() ? BlessingStatsFor(*t_, blessing_->id, blessing_->level) : StatBag{};
}

HomesteadTower::TickResult HomesteadTower::Tick(double dtMs) {
  TickResult r;
  if (blessing_.has_value()) {
    blessing_->remainingMs -= dtMs;
    if (blessing_->remainingMs <= 0) {
      blessing_.reset();
      r.blessingEnded = true;
    }
  }
  if (expedition_.has_value() && !ExpeditionDone()) {
    expedition_->remainingMs = (std::max)(0.0, expedition_->remainingMs - dtMs);
    r.expeditionReturned = ExpeditionDone();
  }
  return r;
}

bool HomesteadTower::OnEnterTower() {
  const bool had = blessing_.has_value();
  blessing_.reset();
  return had;
}

void HomesteadTower::WriteSave(SaveHomestead& out) const {
  out.buildings = buildings_;
  out.embers = embers_;
  out.garden = garden_;
  out.expedition = expedition_;
  out.blessing = blessing_;
  out.towerReturn = towerReturn_;
}

void HomesteadTower::Load(const SaveHomestead& in) {
  Reset();
  buildings_ = in.buildings;
  embers_ = (std::max)(0, in.embers);
  garden_.progress = (std::max)(0, in.garden.progress);
  for (const auto& [id, n] : in.garden.stock) {
    if (n > 0) ReturnToGarden(id, n);
  }
  if (in.expedition.has_value()) {
    const ExpeditionState& e = *in.expedition;
    bool known = false;
    for (const ExpeditionOptionDef& o : t_->expeditionOptions) known = known || o.id == e.optionId;
    if (known) {
      expedition_ = ExpeditionState{e.petId, e.optionId, (std::max)(0, e.kills), (std::max)(1, e.killsRequired),
                                    (std::max)(0.0, e.remainingMs)};
    }
  }
  if (in.blessing.has_value()) {
    const BlessingState& b = *in.blessing;
    bool known = false;
    for (const BlessingDef& d : t_->blessings) known = known || d.id == b.id;
    if (known && b.remainingMs > 0) blessing_ = BlessingState{b.id, (std::max)(1, b.level), b.remainingMs};
  }
  if (in.towerReturn.has_value()) towerReturn_ = in.towerReturn;
}

void HomesteadTower::Reset() {
  buildings_.clear();
  turnedIn_.clear();
  embers_ = 0;
  garden_ = GardenState{};
  expedition_.reset();
  blessing_.reset();
  towerReturn_.reset();
}

// =====================================================================================================================
// HomesteadSystem (runtime)
// =====================================================================================================================

HomesteadSystem::HomesteadSystem(SimContext& ctx) : ctx_(ctx), tower_(ctx.data.Homestead()) {}

StatBag HomesteadSystem::TotalBonuses() const {
  StatBag out = tower_.BuildingBonuses();
  out.AddAll(tower_.BlessingStats());
  return out;
}

bool HomesteadSystem::TowerHidden() const {
  return ctx_.config.milestone1 && ctx_.data.Homestead().towerHiddenInMilestone1;
}

bool HomesteadSystem::InTower() const { return ctx_.session.currentMap == ctx_.data.Homestead().towerZoneId; }

std::vector<std::string> HomesteadSystem::TurnedInQuests() const {
  return ctx_.sys.quests != nullptr ? ctx_.sys.quests->TurnedInIds() : std::vector<std::string>{};
}

std::vector<std::string> HomesteadSystem::SyncUnlocks() {
  const std::vector<std::string> done = TurnedInQuests();
  return tower_.SyncUnlocks(done);
}

void HomesteadSystem::OnMonsterKilled(const MonsterKilledMsg& m) {
  const int32_t n = tower_.AddEmbers(EmbersForKill(ctx_.data.Homestead(), m.elite, m.isMiniBoss, m.eliteAffixCount));
  if (n > 0) ctx_.events.Emit(EvEmbersGained{n, m.pos, true});  // the +N float is shown in Ch1 too (OQ10)
  tower_.OnKillGarden(ctx_.Rand(RngStream::Pets));
  if (tower_.OnKillExpedition()) ctx_.events.Log(MakeLoc("homestead.log.expeditionBack"), LogType::System);
}

void HomesteadSystem::OnQuestTurnedIn(const QuestTurnedInMsg& m) {
  const HomesteadTables& t = ctx_.data.Homestead();
  if (const QuestDef* q = ctx_.data.FindQuest(m.questId)) {
    const int32_t n = tower_.AddEmbers(EmbersForQuest(t, q->category, q->rewards.hasEmbers, q->rewards.embers));
    if (n > 0) {
      ctx_.events.Log(MakeLoc("homestead.log.questEmbers", {{"n", ToStr(n)}}), LogType::Loot);
      const Vec2 at = ctx_.sys.hero != nullptr ? ctx_.sys.hero->Position() : Vec2();
      ctx_.events.Emit(EvEmbersGained{n, at, false});
    }
  }
  const bool wasOpen = tower_.TowerUnlocked();
  const std::vector<std::string> fresh = SyncUnlocks();
  if (TowerHidden()) return;  // OQ8: the unlock lines wait for the tower milestone (state is still derived)
  for (const std::string& id : fresh) {
    ctx_.events.Log(MakeLoc("homestead.log.wingUnlocked", {HsBuildingNameArg(ctx_.data, t.FindBuilding(id), id)}),
                    LogType::System);
  }
  if (!wasOpen && m.questId == t.towerUnlockQuest && tower_.TowerUnlocked()) {
    ctx_.events.Log(MakeLoc("homestead.log.towerUnlocked"), LogType::System);
  }
}

void HomesteadSystem::Tick(double dtMs) {
  const bool inTower = InTower();
  const HomesteadTower::TickResult r = tower_.Tick(inTower ? 0.0 : dtMs);
  if (inTower && tower_.Expedition().has_value() && !tower_.ExpeditionDone()) {
    // Expeditions keep travelling while the hero rests at home.
    ExpeditionState& e = *tower_.Expedition();
    e.remainingMs = (std::max)(0.0, e.remainingMs - dtMs);
    if (tower_.ExpeditionDone()) ctx_.events.Log(MakeLoc("homestead.log.expeditionBack"), LogType::System);
  }
  if (r.blessingEnded) {
    ctx_.events.Log(MakeLoc("homestead.log.blessingFaded"), LogType::System);
    ctx_.bus.Publish(EquipStatsDirtyMsg{});
  }
  if (r.expeditionReturned) ctx_.events.Log(MakeLoc("homestead.log.expeditionBack"), LogType::System);
}

void HomesteadSystem::OnEnterTower() {
  SyncUnlocks();
  if (tower_.OnEnterTower()) {
    ctx_.events.Log(MakeLoc("homestead.log.blessingHome"), LogType::System);
    ctx_.bus.Publish(EquipStatsDirtyMsg{});
  }
}

bool HomesteadSystem::GrantItem(std::string_view baseId) {
  if (ctx_.sys.inventory == nullptr) return false;
  InventorySystem& inv = *ctx_.sys.inventory;
  const int32_t level = ctx_.sys.hero != nullptr ? ctx_.sys.hero->Level() : 1;
  const LootContext lc{&ctx_.data, &ctx_.Rand(RngStream::Pets), &inv.Uids()};
  std::optional<ItemInstance> item = CreateItem(lc, baseId, level, ItemQuality::Normal);
  if (!item.has_value()) return false;
  item->quantity = 1;
  if (!inv.Items().AddItem(*item).ok) inv.Items().PushStashOverflow(std::move(*item));
  return true;
}

bool HomesteadSystem::UpgradeBuilding(std::string_view buildingId) {
  if (ctx_.sys.hero == nullptr) return false;
  if (!tower_.CanUpgrade(buildingId, ctx_.sys.hero->Gold())) return false;
  const int64_t gold = tower_.Upgrade(buildingId);
  if (ctx_.sys.rewards != nullptr) {
    ctx_.sys.rewards->SpendGold(gold, GoldReason::Homestead);
  } else {
    ctx_.sys.hero->SpendGold(gold);
  }
  const BuildingDef* def = ctx_.data.Homestead().FindBuilding(buildingId);
  ctx_.events.Log(MakeLoc("sys.homestead.buildingUpgrade", {HsBuildingNameArg(ctx_.data, def, buildingId),
                                                             {"level", ToStr(tower_.BuildingLevel(buildingId))}}),
                  LogType::System);
  return true;
}

int32_t HomesteadSystem::HarvestGarden() {
  if (!InTower() || ctx_.sys.inventory == nullptr) return 0;
  InventorySystem& inv = *ctx_.sys.inventory;
  const int32_t level = ctx_.sys.hero != nullptr ? ctx_.sys.hero->Level() : 1;
  int32_t taken = 0;
  for (const auto& [id, count] : tower_.Harvest()) {
    int32_t left = count;
    while (left > 0) {
      const LootContext lc{&ctx_.data, &ctx_.Rand(RngStream::Pets), &inv.Uids()};
      std::optional<ItemInstance> item = CreateItem(lc, id, level, ItemQuality::Normal);
      if (!item.has_value()) break;
      item->quantity = 1;
      if (!inv.Items().AddItem(*item).ok) break;
      --left;
      ++taken;
    }
    tower_.ReturnToGarden(id, left);
  }
  if (taken > 0) {
    ctx_.events.Log(MakeLoc("homestead.log.harvested", {{"n", ToStr(taken)}}), LogType::Loot);
    ctx_.events.Emit(EvInventoryChanged{});
  }
  return taken;
}

bool HomesteadSystem::CombineGem(std::string_view gemId) {
  if (!InTower() || ctx_.sys.inventory == nullptr || ctx_.sys.hero == nullptr) return false;
  if (!tower_.IsBuildingUnlocked(kHsGemWorkshop)) return false;
  InventorySystem& inv = *ctx_.sys.inventory;
  Hero& hero = *ctx_.sys.hero;
  const int32_t have = inv.Items().CountOf(gemId);
  if (GemCombineBlockFor(ctx_.data, gemId, have, tower_.BuildingLevel(kHsGemWorkshop), hero.Level(), hero.Gold()) !=
      GemCombineBlock::None) {
    return false;
  }
  const std::string next = NextGemId(ctx_.data, gemId);
  const LootContext lc{&ctx_.data, &ctx_.Rand(RngStream::Pets), &inv.Uids()};
  std::optional<ItemInstance> made = CreateItem(lc, next, hero.Level(), ItemQuality::Normal);
  if (!made.has_value()) return false;
  int32_t need = ctx_.data.Homestead().gemCombineCount;
  std::vector<std::pair<std::string, int32_t>> takes;
  for (const ItemInstance& it : inv.Items().Bag()) {
    if (need <= 0) break;
    if (it.baseId != gemId) continue;
    const int32_t take = (std::min)(need, it.quantity);
    takes.emplace_back(it.uid, take);
    need -= take;
  }
  for (const auto& [uid, take] : takes) inv.Items().RemoveItem(uid, take, &inv.Uids());
  const ItemBaseDef* base = ctx_.data.FindItemBase(next);
  const int64_t cost = GemCombineGold(base != nullptr ? base->gemTier : 0);
  if (ctx_.sys.rewards != nullptr) {
    ctx_.sys.rewards->SpendGold(cost, GoldReason::Homestead);
  } else {
    hero.SpendGold(cost);
  }
  made->quantity = 1;
  if (!inv.Items().AddItem(*made).ok) inv.Items().PushStashOverflow(std::move(*made));
  ctx_.events.Log(MakeLoc("homestead.log.gemCombined", {KeyArg("name", StrCat("data.item.", next, ".name"))}),
                  LogType::Loot);
  ctx_.events.Emit(EvInventoryChanged{});
  return true;
}

bool HomesteadSystem::SendExpedition(std::string_view petId, std::string_view optionId) {
  if (!InTower() || ctx_.sys.pets == nullptr) return false;
  std::vector<std::string> owned;
  for (const PetInstance& p : ctx_.sys.pets->Owned()) owned.push_back(p.petId);
  if (!tower_.SendExpedition(petId, optionId, owned, ctx_.sys.pets->ActiveId())) return false;
  ctx_.events.Log(MakeLoc("homestead.log.expeditionSent"), LogType::System);
  return true;
}

bool HomesteadSystem::ClaimExpedition() {
  if (!InTower() || ctx_.sys.hero == nullptr) return false;
  std::optional<ClaimedExpedition> c = tower_.ClaimExpedition(ctx_.sys.hero->Level(), ctx_.Rand(RngStream::Pets));
  if (!c.has_value()) return false;
  if (ctx_.sys.rewards != nullptr) {
    ctx_.sys.rewards->ChangeGold(c->reward.gold, GoldReason::Homestead);
  } else {
    ctx_.sys.hero->AddGold(c->reward.gold);
  }
  for (const auto& [id, count] : c->reward.items) {
    for (int32_t i = 0; i < count; ++i) GrantItem(id);
  }
  ctx_.events.Log(MakeLoc("homestead.log.expeditionClaimed",
                          {{"embers", ToStr(c->reward.embers)}, {"gold", ToStrI64(c->reward.gold)}}),
                  LogType::Loot);
  ctx_.events.Emit(EvInventoryChanged{});
  return true;
}

bool HomesteadSystem::BuyBlessing(std::string_view blessingId) {
  if (!InTower() || !tower_.BuyBlessing(blessingId)) return false;
  ctx_.events.Log(MakeLoc("homestead.log.blessed", {KeyArg("name", StrCat("homestead.blessing.", blessingId, ".name"))}),
                  LogType::System);
  ctx_.bus.Publish(EquipStatsDirtyMsg{});
  return true;
}

void HomesteadSystem::FillSnapshot(Snapshot& out) const { out.homestead = &tower_; }

void HomesteadSystem::WriteSave(SaveData& out) const { tower_.WriteSave(out.homestead); }

void HomesteadSystem::ReadSave(const SaveData& in) {
  tower_.Load(in.homestead);
  SyncUnlocks();  // the EmberTower constructor's sync: nothing is logged
}

}  // namespace abyss

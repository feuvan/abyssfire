// Ley-beasts: rules and state (quests-story-ch1.md 18.2-18.4; save-ui-input.md 3.3). Web: src/systems/PetSystem.ts.
#include "abyss/base/Platform.h"

#include "abyss/pets/PetSystem.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "abyss/base/I18n.h"
#include "abyss/base/Json.h"
#include "abyss/base/Math.h"
#include "abyss/base/StrUtil.h"
#include "abyss/data/DataStore.h"
#include "abyss/save/SaveData.h"
#include "abyss/save/SaveIO.h"

namespace abyss {

namespace {

// The 月井 building whose level raises the bond cap and pet exp (homestead.ts BUILDINGS; PetSystem.ts:249-256).
constexpr std::string_view kPsPetHouseId = "pet_house";
// getExpMultiplier: +10 % per 月井 level; resting exp share 0.2 + 0.1 (level - 1) (PetSystem.ts:254-256, 349-351).
constexpr double kPsPetHouseExpPerLevel = 0.1;
constexpr double kPsRestingExpBase = 0.2;
constexpr double kPsRestingExpPerLevel = 0.1;
// Bond progress per active minute.
constexpr double kPsActiveMinuteMs = 60000.0;

int64_t PsFloorToI64(double v) {
  constexpr double kLim = 9007199254740992.0;  // 2^53
  if (!(v == v)) return 0;
  return static_cast<int64_t>(std::floor(std::clamp(v, -kLim, kLim)));
}

}  // namespace

// =====================================================================================================================
// formulas (18.3)
// =====================================================================================================================

int64_t PetExpToNext(const PetSystemConstants& c, int32_t level) {
  return PsFloorToI64(c.expToNextBase + c.expToNextPerLevel * static_cast<double>(level));
}

int64_t PetKillExp(const PetSystemConstants& c, int32_t monsterLevel) {
  return PsFloorToI64(c.killExpBase + c.killExpPerMonsterLevel * static_cast<double>((std::max)(0, monsterLevel)));
}

int32_t PetEvolutionForLevel(const PetTables& t, int32_t level) {
  int32_t stage = 0;
  for (int32_t threshold : t.evolutionLevels) {
    if (level >= threshold) ++stage;
  }
  return stage;
}

double PetBondMultiplier(const PetTables& t, int32_t bond) {
  return 1.0 + t.system.bondMultiplierPerBond * static_cast<double>(std::clamp(bond, 0, t.maxBond));
}

double PetEvolutionMultiplier(const PetTables& t, int32_t evolved) {
  if (t.evolutionMult.empty()) return 1.0;
  const int32_t last = static_cast<int32_t>(t.evolutionMult.size()) - 1;
  return t.evolutionMult[static_cast<size_t>(std::clamp(evolved, 0, last))];
}

double PetPassiveValue(const PetTables& t, const PetDef& def, const PetInstance& p) {
  const double base = def.passiveBase + def.passivePerLevel * static_cast<double>((std::max)(1, p.level) - 1);
  const double evo = PetEvolutionMultiplier(t, p.evolved);
  return JsRound(base * evo * PetBondMultiplier(t, p.bond) * 10.0) / 10.0;
}

int32_t PetAttackDamage(const PetTables& t, double heroDamage, const PetInstance& p) {
  const PetSystemConstants& c = t.system;
  const double fraction =
      (std::min)(c.damageMaxFraction, c.damageBaseFraction + static_cast<double>(p.level) * c.damagePerLevelFraction);
  const double evo = PetEvolutionMultiplier(t, p.evolved);
  const int32_t raw = FloorInt(heroDamage * fraction * evo);
  return raw > 0 ? (std::max)(1, raw) : 0;
}

double LeyFruitDropChance(const PetSystemConstants& c, bool elite) {
  return elite ? c.leyFruitDropElite : c.leyFruitDropOther;
}

StatBag MergeBonuses(const StatBag& a, const StatBag& b) {
  StatBag out = a;
  out.AddAll(b);
  return out;
}

I18nArg PetNameArg(std::string argName, const DataStore& data, std::string_view petId, int32_t evolved) {
  const PetDef* def = data.FindPet(petId);
  const std::string nameKey = def != nullptr ? def->nameKey : StrCat("data.pet.", petId, ".name");
  const I18n& i18n = data.Strings();
  if (evolved <= 0) {
    if (i18n.Has(nameKey)) return KeyArg(std::move(argName), nameKey);
    return I18nArg{std::move(argName), std::string(petId), false};
  }
  const std::string name = i18n.NameOr(nameKey, petId);
  const I18nArg nameArg{"name", name, false};
  const std::string text = i18n.T(evolved >= 2 ? "sys.pet.evoName.2" : "sys.pet.evoName.1", {&nameArg, 1});
  return I18nArg{std::move(argName), text, false};
}

LocText PetDisplayName(const DataStore& data, std::string_view petId, int32_t evolved) {
  const PetDef* def = data.FindPet(petId);
  const std::string nameKey = def != nullptr ? def->nameKey : StrCat("data.pet.", petId, ".name");
  const I18nArg name = data.Strings().Has(nameKey) ? KeyArg("name", nameKey) : I18nArg{"name", std::string(petId), false};
  if (evolved <= 0) return name.isKey ? MakeLoc(nameKey) : MakeLoc(std::string(petId));
  return MakeLoc(evolved >= 2 ? "sys.pet.evoName.2" : "sys.pet.evoName.1", {name});
}

// =====================================================================================================================
// migratePetSave (save 3.3)
// =====================================================================================================================

PetSaveNormalized MigratePetSave(const DataStore& data, const SaveData& in) {
  const PetTables& t = data.Pets();
  std::vector<PetInstance> raw;
  bool fromLegacy = false;
  if (in.pets.present && in.pets.hasOwned) {
    raw = in.pets.owned;
  } else {
    fromLegacy = true;
  }
  if (fromLegacy && in.homestead.hasLegacyPets && in.homestead.legacyPets.IsArray()) {
    for (const JsonValue& v : in.homestead.legacyPets.Items()) {
      PetInstance p;
      if (ReadPetJson(v, p)) raw.push_back(std::move(p));
    }
  }
  PetSaveNormalized out;
  for (const PetInstance& p : raw) {
    if (p.petId.empty() || data.FindPet(p.petId) == nullptr) continue;
    bool dup = false;
    for (const PetInstance& o : out.owned) dup = dup || o.petId == p.petId;
    if (dup) continue;
    PetInstance n;
    n.petId = p.petId;
    n.level = std::clamp(p.level, 1, t.maxLevel);
    n.exp = n.level >= t.maxLevel ? 0 : std::clamp<int64_t>(p.exp, 0, PetExpToNext(t.system, n.level) - 1);
    const int32_t maxStage = static_cast<int32_t>(t.evolutionLevels.size());
    n.evolved = (std::max)(PetEvolutionForLevel(t, n.level), std::clamp(p.evolved, 0, maxStage));
    n.bond = std::clamp(p.bond, 0, t.maxBond);
    n.bondProgress = std::clamp(p.bondProgress, 0, t.system.bondProgressPerLevel - 1);
    out.owned.push_back(std::move(n));
  }
  const std::string& rawActive =
      in.pets.present ? in.pets.active : (in.homestead.hasLegacyActivePet ? in.homestead.legacyActivePet : std::string());
  for (const PetInstance& o : out.owned) {
    if (!rawActive.empty() && o.petId == rawActive) out.active = rawActive;
  }
  return out;
}

// =====================================================================================================================
// PetSystem
// =====================================================================================================================

PetSystem::PetSystem(const DataStore& data, EventSink& events, const GameplayBus& bus)
    : data_(&data), events_(&events), bus_(&bus) {}

void PetSystem::SetBuildingLevelSource(std::function<int32_t(std::string_view buildingId)> src) {
  buildingLevel_ = std::move(src);
}

void PetSystem::SetAwaySource(std::function<bool(std::string_view petId)> src) { away_ = std::move(src); }

bool PetSystem::IsAway(std::string_view petId) const { return away_ ? away_(petId) : false; }

int32_t PetSystem::BuildingLevel(std::string_view id) const { return buildingLevel_ ? buildingLevel_(id) : 0; }

I18nArg PetSystem::NameArg(const PetInstance& p, std::string argName) const {
  return PetNameArg(std::move(argName), *data_, p.petId, p.evolved);
}

void PetSystem::EmitChanged(std::string_view petId) {
  events_->Emit(EvPet{EvPet::Kind::Changed, std::string(petId), false});
}

bool PetSystem::AddPet(std::string_view petId, bool silent) {
  if (data_->FindPet(petId) == nullptr) return false;
  if (Has(petId)) {
    if (!silent) events_->Log(MakeLoc("sys.pet.duplicate"), LogType::System);
    return false;
  }
  PetInstance p;
  p.petId = std::string(petId);
  pets_.push_back(p);
  const bool becameActive = active_.empty();
  if (becameActive) active_ = p.petId;
  if (!silent) events_->Log(MakeLoc("sys.pet.obtained", {NameArg(p)}), LogType::System);
  events_->Emit(EvPet{EvPet::Kind::Obtained, p.petId, silent});
  EmitChanged(p.petId);
  if (becameActive) bus_->Publish(PetChangedMsg{active_, PetChangeReason::Obtained});
  return true;
}

void PetSystem::SetActivePet(std::string_view petId) {
  if (!petId.empty() && (!Has(petId) || IsAway(petId))) return;
  if (active_ == petId) return;
  active_ = std::string(petId);
  activeMs_ = 0;
  EmitChanged(petId);
  bus_->Publish(PetChangedMsg{active_, active_.empty() ? PetChangeReason::Rested : PetChangeReason::Activated});
}

int32_t PetSystem::AddExp(std::string_view petId, double amount, bool silent) {
  PetInstance* pet = FindMutable(petId);
  const PetTables& t = data_->Pets();
  if (pet == nullptr || !(amount > 0) || pet->level >= t.maxLevel) return 0;
  pet->exp += PsFloorToI64(amount);
  int32_t gained = 0;
  bool evolvedNow = false;
  while (pet->level < t.maxLevel && pet->exp >= PetExpToNext(t.system, pet->level)) {
    pet->exp -= PetExpToNext(t.system, pet->level);
    ++pet->level;
    ++gained;
    if (!silent) {
      events_->Log(MakeLoc("sys.pet.levelUp", {NameArg(*pet), {"level", ToStr(pet->level)}}), LogType::System);
      events_->Emit(EvPet{EvPet::Kind::LevelUp, pet->petId, false});
    }
    const int32_t stage = PetEvolutionForLevel(t, pet->level);
    if (stage > pet->evolved) {
      const I18nArg before = NameArg(*pet);
      pet->evolved = stage;
      evolvedNow = true;
      // Always logged, even when silent (PetSystem.ts:310-314).
      events_->Log(MakeLoc("sys.pet.evolved", {before, NameArg(*pet, "evolvedName")}), LogType::System);
      events_->Emit(EvPet{EvPet::Kind::Evolved, pet->petId, silent});
    }
  }
  if (pet->level >= t.maxLevel) pet->exp = 0;
  if (gained > 0) {
    const std::string id = pet->petId;  // the publishes below may run code that touches pets_
    EmitChanged(id);
    if (id == active_) {
      if (evolvedNow) {
        bus_->Publish(PetChangedMsg{active_, PetChangeReason::Evolved});
      } else {
        bus_->Publish(EquipStatsDirtyMsg{});  // the passive value grew with the level
      }
    }
  }
  return gained;
}

int32_t PetSystem::AddBond(std::string_view petId, int32_t progress) {
  PetInstance* pet = FindMutable(petId);
  if (pet == nullptr || progress <= 0) return 0;
  const PetSystemConstants& c = data_->Pets().system;
  const int32_t cap = BondCap();
  if (pet->bond >= cap) {
    pet->bondProgress = 0;
    return 0;
  }
  int64_t bp = static_cast<int64_t>(pet->bondProgress) + progress;
  int32_t gained = 0;
  while (pet->bond < cap && bp >= c.bondProgressPerLevel) {
    bp -= c.bondProgressPerLevel;
    ++pet->bond;
    ++gained;
    events_->Log(MakeLoc("sys.pet.bondUp", {NameArg(*pet), {"bond", ToStr(pet->bond)}}), LogType::System);
  }
  if (pet->bond >= cap) bp = 0;
  pet->bondProgress = static_cast<int32_t>((std::min)(bp, static_cast<int64_t>((std::numeric_limits<int32_t>::max)())));
  if (gained > 0) {
    const std::string id = pet->petId;
    EmitChanged(id);
    if (id == active_) bus_->Publish(EquipStatsDirtyMsg{});  // the passive scales with the bond
  }
  return gained;
}

void PetSystem::OnKill(int32_t monsterLevel) {
  const PetSystemConstants& c = data_->Pets().system;
  const double exp = static_cast<double>(PetKillExp(c, monsterLevel)) * ExpMultiplier();
  if (!active_.empty()) {
    const std::string id = active_;
    AddExp(id, exp);
    AddBond(id, c.bondPerKill);
  }
  const int32_t well = BuildingLevel(kPsPetHouseId);
  if (well > 0) {
    const double share = kPsRestingExpBase + kPsRestingExpPerLevel * static_cast<double>(well - 1);
    GrantRestingExp(PsFloorToI64(static_cast<double>(PetKillExp(c, monsterLevel)) * share));
  }
}

void PetSystem::GrantRestingExp(int64_t amount) {
  if (amount <= 0) return;
  std::vector<std::string> ids;
  for (const PetInstance& p : pets_) {
    if (p.petId != active_) ids.push_back(p.petId);
  }
  for (const std::string& id : ids) AddExp(id, static_cast<double>(amount), true);
}

void PetSystem::TickActive(double dtMs) {
  if (active_.empty() || !(dtMs > 0)) return;
  activeMs_ += dtMs;
  const PetSystemConstants& c = data_->Pets().system;
  while (activeMs_ >= kPsActiveMinuteMs) {
    activeMs_ -= kPsActiveMinuteMs;
    AddBond(active_, c.bondPerActiveMinute);
  }
}

bool PetSystem::CanFeed(std::string_view petId) const {
  const PetInstance* pet = Find(petId);
  if (pet == nullptr) return false;
  return pet->level < data_->Pets().maxLevel || pet->bond < BondCap();
}

bool PetSystem::Feed(std::string_view petId) {
  if (!CanFeed(petId)) return false;
  const PetInstance* pet = Find(petId);
  const std::string id = pet->petId;
  const PetSystemConstants& c = data_->Pets().system;
  events_->Log(MakeLoc("sys.pet.fed", {NameArg(*pet)}), LogType::System);
  AddExp(id, c.feedExp * ExpMultiplier());
  AddBond(id, c.bondPerFeed);
  EmitChanged(id);
  return true;
}

int32_t PetSystem::BondCap() const {
  const PetTables& t = data_->Pets();
  return (std::min)(t.maxBond, t.system.baseBondCap + (std::max)(0, BuildingLevel(kPsPetHouseId)));
}

double PetSystem::ExpMultiplier() const {
  return 1.0 + static_cast<double>((std::max)(0, BuildingLevel(kPsPetHouseId))) * kPsPetHouseExpPerLevel;
}

StatBag PetSystem::Bonuses() const {
  StatBag out;
  const PetInstance* pet = Active();
  const PetDef* def = ActiveDef();
  if (pet == nullptr || def == nullptr) return out;
  out.Set(def->passiveStat, PetPassiveValue(data_->Pets(), *def, *pet));
  return out;
}

int32_t PetSystem::PetDamage(double heroDamage) const {
  const PetInstance* pet = Active();
  return pet != nullptr ? PetAttackDamage(data_->Pets(), heroDamage, *pet) : 0;
}

std::string_view PetSystem::DisplayNameKey(const PetInstance& p) const {
  const PetDef* def = data_->FindPet(p.petId);
  return def != nullptr ? std::string_view(def->nameKey) : std::string_view(p.petId);
}

LocText PetSystem::DisplayName(const PetInstance& p) const { return PetDisplayName(*data_, p.petId, p.evolved); }

std::vector<const PetAbilityDef*> PetSystem::UnlockedAbilities(std::string_view petId) const {
  const PetDef* def = data_->FindPet(petId);
  const PetInstance* pet = Find(petId);
  if (def == nullptr || pet == nullptr) return {};
  return UnlockedPetAbilities(*def, pet->evolved);
}

PetInstance* PetSystem::FindMutable(std::string_view petId) {
  for (PetInstance& p : pets_) {
    if (p.petId == petId) return &p;
  }
  return nullptr;
}

const PetInstance* PetSystem::Find(std::string_view petId) const {
  for (const PetInstance& p : pets_) {
    if (p.petId == petId) return &p;
  }
  return nullptr;
}

const PetInstance* PetSystem::Active() const { return active_.empty() ? nullptr : Find(active_); }

const PetDef* PetSystem::ActiveDef() const { return active_.empty() ? nullptr : data_->FindPet(active_); }

void PetSystem::WriteSave(SaveData& out) const {
  out.pets.present = true;
  out.pets.hasOwned = true;
  out.pets.owned = pets_;
  out.pets.active = active_;
}

void PetSystem::ReadSave(const SaveData& in) {
  PetSaveNormalized n = MigratePetSave(*data_, in);
  pets_ = std::move(n.owned);
  active_ = std::move(n.active);
  activeMs_ = 0;
  bus_->Publish(PetChangedMsg{active_, PetChangeReason::Loaded});
}

void PetSystem::Reset() {
  pets_.clear();
  active_.clear();
  activeMs_ = 0;
}

}  // namespace abyss

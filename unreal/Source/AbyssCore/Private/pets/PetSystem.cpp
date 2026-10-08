// Ley-beasts: rules and state (quests-story-ch1.md 18.2-18.4; save-ui-input.md 3.3). STUB: owner area
// quests+story+pets. Lookups are real.
#include "abyss/base/Platform.h"

#include "abyss/pets/PetSystem.h"

#include "abyss/base/Assert.h"
#include "abyss/data/DataStore.h"
#include "abyss/save/SaveData.h"

namespace abyss {

int64_t PetExpToNext(const PetSystemConstants& c, int32_t level) {
  ABYSS_UNIMPLEMENTED();
  return 0;
}

int64_t PetKillExp(const PetSystemConstants& c, int32_t monsterLevel) {
  ABYSS_UNIMPLEMENTED();
  return 0;
}

int32_t PetEvolutionForLevel(const PetTables& t, int32_t level) {
  int32_t stage = 0;
  for (int32_t threshold : t.evolutionLevels) {
    if (level >= threshold) ++stage;
  }
  return stage;
}

double PetBondMultiplier(const PetSystemConstants& c, int32_t bond) {
  ABYSS_UNIMPLEMENTED();
  return 1.0;
}

double PetPassiveValue(const PetTables& t, const PetDef& def, const PetInstance& p) {
  ABYSS_UNIMPLEMENTED();
  return 0;
}

int32_t PetAttackDamage(const PetTables& t, double heroDamage, const PetInstance& p) {
  ABYSS_UNIMPLEMENTED();
  return 0;
}

PetSystem::PetSystem(const DataStore& data, EventSink& events, const GameplayBus& bus)
    : data_(&data), events_(&events), bus_(&bus) {}

void PetSystem::SetBuildingLevelSource(std::function<int32_t(std::string_view buildingId)> src) {
  buildingLevel_ = std::move(src);
}

void PetSystem::SetAwaySource(std::function<bool(std::string_view petId)> src) { away_ = std::move(src); }

bool PetSystem::AddPet(std::string_view petId, bool silent) {
  ABYSS_UNIMPLEMENTED();
  (void)events_;
  (void)activeMs_;
  return false;
}

void PetSystem::SetActivePet(std::string_view petId) { ABYSS_UNIMPLEMENTED(); }

int32_t PetSystem::AddExp(std::string_view petId, double amount, bool silent) {
  ABYSS_UNIMPLEMENTED();
  return 0;
}

int32_t PetSystem::AddBond(std::string_view petId, int32_t progress) {
  ABYSS_UNIMPLEMENTED();
  return 0;
}

void PetSystem::OnKill(int32_t monsterLevel) {
  if (!active_.empty()) ABYSS_UNIMPLEMENTED();
}

void PetSystem::TickActive(double dtMs) { ABYSS_UNIMPLEMENTED(); }

bool PetSystem::CanFeed(std::string_view petId) const {
  ABYSS_UNIMPLEMENTED();
  return false;
}

bool PetSystem::Feed(std::string_view petId) {
  ABYSS_UNIMPLEMENTED();
  return false;
}

int32_t PetSystem::BondCap() const {
  ABYSS_UNIMPLEMENTED();
  return data_->Pets().system.baseBondCap;
}

double PetSystem::ExpMultiplier() const {
  ABYSS_UNIMPLEMENTED();
  return 1.0;
}

StatBag PetSystem::Bonuses() const {
  if (!active_.empty()) ABYSS_UNIMPLEMENTED();
  return {};
}

int32_t PetSystem::PetDamage(double heroDamage) const {
  ABYSS_UNIMPLEMENTED();
  return 0;
}

std::string_view PetSystem::DisplayNameKey(const PetInstance& p) const {
  const PetDef* def = data_->FindPet(p.petId);
  return def != nullptr ? std::string_view(def->nameKey) : std::string_view(p.petId);
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

void PetSystem::WriteSave(SaveData& out) const {
  out.pets.present = true;
  out.pets.owned = pets_;
  out.pets.active = active_;
}

void PetSystem::ReadSave(const SaveData& in) {
  ABYSS_UNIMPLEMENTED();
  (void)bus_;
}

}  // namespace abyss

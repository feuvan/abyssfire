// Ley-beasts (pets): ownership, level / exp / evolution, bond, passive bonus, kill exp, feeding (pure rules + state).
// Spec: quests-story-ch1.md 18.0-18.4 (decision for Chapter 1, PetDef, state and save, PetSystem formulas and
// operations, how the active beast feeds the hero), save-ui-input.md 3.3 (migratePetSave normalisation);
// DECISIONS Q3 (Chapter 1 ships the pet_sprite slice; the pets panel shows owned beasts only).
//
// Owner area: quests+story+pets. `PetSystem` is pure (unit-testable alone): it takes its building-level / away
// sources as callbacks (homestead), logs through the sink and publishes PetChangedMsg{active pet, reason} whenever the
// active beast or its look changes: SetActivePet (Activated / Rested), AddPet making it active (Obtained), an
// evolution of the active beast (Evolved), ReadSave (Loaded). GameSim wiring: equip stats rebuilt, then
// PetCompanion::OnPetChanged updates the companion entity.
#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "abyss/base/Platform.h"
#include "abyss/base/Stats.h"
#include "abyss/data/PetData.h"
#include "abyss/sim/Events.h"
#include "abyss/sim/GameplayBus.h"

namespace abyss {

class DataStore;
struct SaveData;

struct PetInstance {
  std::string petId;
  int32_t level = 1;      // 1..20
  int64_t exp = 0;
  int32_t evolved = 0;    // 0..2
  int32_t bond = 0;       // 0..5
  int32_t bondProgress = 0;  // 0..99
  bool operator==(const PetInstance& o) const = default;
};

// Formulas (18.3), JS rounding.
ABYSS_API int64_t PetExpToNext(const PetSystemConstants& c, int32_t level);
ABYSS_API int64_t PetKillExp(const PetSystemConstants& c, int32_t monsterLevel);
ABYSS_API int32_t PetEvolutionForLevel(const PetTables& t, int32_t level);
ABYSS_API double PetBondMultiplier(const PetSystemConstants& c, int32_t bond);
ABYSS_API double PetPassiveValue(const PetTables& t, const PetDef& def, const PetInstance& p);
ABYSS_API int32_t PetAttackDamage(const PetTables& t, double heroDamage, const PetInstance& p);

class ABYSS_API PetSystem {
 public:
  PetSystem(const DataStore& data, EventSink& events, const GameplayBus& bus);

  // Sources wired by GameSim (homestead): pet_house level and expedition "away" state.
  void SetBuildingLevelSource(std::function<int32_t(std::string_view buildingId)> src);
  void SetAwaySource(std::function<bool(std::string_view petId)> src);

  bool AddPet(std::string_view petId, bool silent = false);
  void SetActivePet(std::string_view petId);  // "" = rest
  int32_t AddExp(std::string_view petId, double amount, bool silent = false);  // levels gained
  int32_t AddBond(std::string_view petId, int32_t progress);
  void OnKill(int32_t monsterLevel);  // 18.3 onKill: active exp + bond; resting exp (pet_house)
  void TickActive(double dtMs);       // bond per active minute (called by PetCompanion only)
  bool CanFeed(std::string_view petId) const;
  bool Feed(std::string_view petId);  // caller removes the ley fruit

  int32_t BondCap() const;
  double ExpMultiplier() const;
  // getBonuses (18.3): {passiveStat: value} of the active pet; empty when resting.
  StatBag Bonuses() const;
  int32_t PetDamage(double heroDamage) const;
  std::string_view DisplayNameKey(const PetInstance& p) const;

  const PetInstance* Find(std::string_view petId) const;
  const PetInstance* Active() const;
  std::span<const PetInstance> Owned() const { return pets_; }

  // Save shape {owned, active}; Load applies migratePetSave (save 3.3) including legacy homestead.pets.
  void WriteSave(SaveData& out) const;
  void ReadSave(const SaveData& in);

 private:
  PetInstance* FindMutable(std::string_view petId);

  const DataStore* data_;
  EventSink* events_;
  const GameplayBus* bus_;
  std::vector<PetInstance> pets_;  // acquisition order
  std::string active_;
  double activeMs_ = 0;  // not saved
  std::function<int32_t(std::string_view)> buildingLevel_;
  std::function<bool(std::string_view)> away_;
};

}  // namespace abyss

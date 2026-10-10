// Ley-beasts (pets): ownership, level / exp / evolution, bond, passive bonus, kill exp, feeding (pure rules + state).
// Spec: quests-story-ch1.md 18.0-18.4 (decision for Chapter 1, PetDef, state and save, PetSystem formulas and
// operations, how the active beast feeds the hero), save-ui-input.md 3.3 (migratePetSave normalisation);
// DECISIONS Q3 (Chapter 1 ships the pet_sprite slice; the pets panel shows owned beasts only).
//
// Owner area: quests+story+pets. `PetSystem` is pure (unit-testable alone): it takes its building-level / away
// sources as callbacks (homestead), logs through the sink and publishes PetChangedMsg{active pet, reason} whenever the
// active beast or its look changes: SetActivePet (Activated / Rested), AddPet making it active (Obtained), an
// evolution of the active beast (Evolved), ReadSave (Loaded). GameSim wiring: equip stats rebuilt, then
// PetCompanion::OnPetChanged updates the companion entity. A level / bond change of the active beast (its passive
// value changed) publishes EquipStatsDirtyMsg instead (the web's PET_CHANGED cache invalidation).
//
// Presentation: every web PET_CHANGED is an EvPet{Changed} (pets panel / medallion refresh); PET_OBTAINED is
// EvPet{Obtained, silent} (no web listener: no toast, QP5); EvPet{LevelUp} / {Evolved} mirror the log lines.
#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "abyss/base/I18n.h"
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
ABYSS_API int64_t PetExpToNext(const PetSystemConstants& c, int32_t level);   // 60 + 40 L
ABYSS_API int64_t PetKillExp(const PetSystemConstants& c, int32_t monsterLevel);  // 10 + max(0, mL)
ABYSS_API int32_t PetEvolutionForLevel(const PetTables& t, int32_t level);     // #{[10, 20] <= L}
ABYSS_API double PetBondMultiplier(const PetTables& t, int32_t bond);          // 1 + 0.1 clamp(bond, 0, 5)
ABYSS_API double PetEvolutionMultiplier(const PetTables& t, int32_t evolved);  // EVO[clamp(evolved, 0, 2)]
// round((base + perLevel (max(1, L) - 1)) x EVO x bondMultiplier x 10) / 10
ABYSS_API double PetPassiveValue(const PetTables& t, const PetDef& def, const PetInstance& p);
// raw = floor(D x min(0.15, 0.05 + 0.005 L) x EVO); raw > 0 ? max(1, raw) : 0
ABYSS_API int32_t PetAttackDamage(const PetTables& t, double heroDamage, const PetInstance& p);
// leyFruitDropChance (18.3): elite ? 0.12 : 0.015.
ABYSS_API double LeyFruitDropChance(const PetSystemConstants& c, bool elite);
// mergeBonuses: key-wise sum (insertion order of `a`, then new keys of `b`).
ABYSS_API StatBag MergeBonuses(const StatBag& a, const StatBag& b);
// Log / UI argument naming a beast with its evolution suffix (getPetDisplayName, 18.3): stage 0 is a KeyArg on
// data.pet.<id>.name (the id itself when the key is missing); an evolved beast's sys.pet.evoName.<1|2> composite cannot
// be one key, so it is rendered with the data's I18n in its current locale (the ItemNameArg pattern, Item.h).
ABYSS_API I18nArg PetNameArg(std::string argName, const DataStore& data, std::string_view petId, int32_t evolved);
// Display name as a LocText for view-models: data.pet.<id>.name, or sys.pet.evoName.<n> {name: KeyArg}.
ABYSS_API LocText PetDisplayName(const DataStore& data, std::string_view petId, int32_t evolved);

// migratePetSave (save-ui-input 3.3; PetSystem.ts:435-456) on the typed save: source pets.owned when the save has a
// `pets` object with an `owned` array, else the legacy homestead.pets list; active = pets.active when `pets` exists,
// else the legacy homestead.activePet. Drops unknown / duplicate ids, clamps level 1..20, exp 0..expToNext-1 (0 at 20),
// evolved = max(stage for level, clamp(evolved, 0, 2)), bond 0..5, bondProgress 0..99; active kept only when owned.
struct PetSaveNormalized {
  std::vector<PetInstance> owned;
  std::string active;  // "" = none
};
ABYSS_API PetSaveNormalized MigratePetSave(const DataStore& data, const SaveData& in);

class ABYSS_API PetSystem {
 public:
  PetSystem(const DataStore& data, EventSink& events, const GameplayBus& bus);

  // Sources wired by GameSim (homestead): pet_house level and expedition "away" state.
  void SetBuildingLevelSource(std::function<int32_t(std::string_view buildingId)> src);
  void SetAwaySource(std::function<bool(std::string_view petId)> src);
  bool IsAway(std::string_view petId) const;

  // addPet (18.3): unknown -> false; owned -> log sys.pet.duplicate (unless silent), false; else append Lv1, the first
  // beast becomes active, log sys.pet.obtained (unless silent), EvPet{Obtained}, EvPet{Changed}, PetChangedMsg when it
  // became active.
  bool AddPet(std::string_view petId, bool silent = false);
  // setActivePet: not owned / away -> no-op; same -> no-op; else set, activeMs = 0, EvPet{Changed}, PetChangedMsg
  // (Activated, or Rested for "").
  void SetActivePet(std::string_view petId);  // "" = rest
  int32_t AddExp(std::string_view petId, double amount, bool silent = false);  // levels gained
  int32_t AddBond(std::string_view petId, int32_t progress);                   // bond levels gained
  void OnKill(int32_t monsterLevel);  // 18.3 onKill: active exp + bond; resting exp (pet_house)
  void GrantRestingExp(int64_t amount);  // every owned non-active beast, silently
  void TickActive(double dtMs);       // bond per active minute (called by PetCompanion only)
  bool CanFeed(std::string_view petId) const;
  bool Feed(std::string_view petId);  // caller removes the ley fruit

  int32_t BondCap() const;
  double ExpMultiplier() const;
  // getBonuses (18.3): {passiveStat: value} of the active pet; empty when resting.
  StatBag Bonuses() const;
  int32_t PetDamage(double heroDamage) const;  // calculatePetDamage: active beast, else 0
  std::string_view DisplayNameKey(const PetInstance& p) const;  // data.pet.<id>.name (no evolution suffix)
  LocText DisplayName(const PetInstance& p) const;              // with the evolution suffix
  std::vector<const PetAbilityDef*> UnlockedAbilities(std::string_view petId) const;

  const PetInstance* Find(std::string_view petId) const;
  const PetInstance* Active() const;
  const PetDef* ActiveDef() const;
  std::string_view ActiveId() const { return active_; }
  std::span<const PetInstance> Owned() const { return pets_; }
  bool Has(std::string_view petId) const { return Find(petId) != nullptr; }
  // Test hook (the web tests poke instance fields directly).
  PetInstance* FindMutableForTesting(std::string_view petId) { return FindMutable(petId); }

  // Save shape {owned, active}; Load applies migratePetSave (save 3.3) including legacy homestead.pets.
  void WriteSave(SaveData& out) const;
  void ReadSave(const SaveData& in);
  void Reset();

 private:
  PetInstance* FindMutable(std::string_view petId);
  int32_t BuildingLevel(std::string_view id) const;
  I18nArg NameArg(const PetInstance& p, std::string argName = "name") const;
  void EmitChanged(std::string_view petId);

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

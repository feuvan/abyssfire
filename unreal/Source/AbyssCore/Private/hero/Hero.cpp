// The hero (classes-stats-skills.md 1.7, 3-6, 18; C1, C2, C12). STUB: owner area hero+combat. The constructor and
// trivial accessors are real; formulas (derived stats, leveling, regen, save mapping) are stubs.
#include "abyss/base/Platform.h"

#include "abyss/hero/Hero.h"

#include <algorithm>

#include "abyss/base/Assert.h"
#include "abyss/data/DataStore.h"
#include "abyss/save/SaveData.h"

namespace abyss {

namespace {
const ClassDef& EmptyClass() {
  static const ClassDef kEmpty;
  return kEmpty;
}
}  // namespace

Hero::Hero(const DataStore& data, ClassId cls)
    : data_(&data),
      cls_(cls),
      class_(data.Classes().Find(cls)),
      skills_(data, cls),
      spirit_(data.Classes().spirit, cls) {
  ABYSS_ASSERT(class_ != nullptr, "Hero: unknown class");
  if (class_ != nullptr) stats_ = class_->baseStats;  // fallback: zero stats, empty class (ClassData)
}

const ClassDef& Hero::ClassData() const { return class_ != nullptr ? *class_ : EmptyClass(); }

int64_t Hero::ExpToNext() const { return data_->Classes().formulas.ExpToNext(level_); }

LevelUpResult Hero::AddExp(int64_t amount, const EquipStats& eq) {
  ABYSS_UNIMPLEMENTED();
  return {false, level_};
}

void Hero::RemoveExp(int64_t amount) { exp_ = std::max<int64_t>(0, exp_ - std::max<int64_t>(0, amount)); }

void Hero::AddGold(int64_t amount) { gold_ += amount; }

bool Hero::SpendGold(int64_t amount) {
  if (amount < 0 || gold_ < amount) return false;
  gold_ -= amount;
  return true;
}

bool Hero::AllocateStat(PrimaryStat s) {
  ABYSS_UNIMPLEMENTED();
  return false;
}

void Hero::RecalcDerived(const EquipStats& eq) {
  ABYSS_UNIMPLEMENTED();
  eq_ = eq;
}

double Hero::GroundSpeedTilesPerSec() const {
  ABYSS_UNIMPLEMENTED();
  return derived_.moveSpeed / 36.0;
}

double Hero::Heal(double amount) {
  ABYSS_UNIMPLEMENTED();
  return 0;
}

double Hero::RestoreMana(double amount) {
  ABYSS_UNIMPLEMENTED();
  return 0;
}

void Hero::SpendMana(double amount) { mana_ = std::max(0.0, mana_ - amount); }

double Hero::ApplyDamage(double amount) {
  const double before = hp_;
  hp_ = std::max(0.0, hp_ - amount);
  return before - hp_;
}

void Hero::FillHpMana() {
  hp_ = derived_.maxHp;
  mana_ = derived_.maxMana;
}

void Hero::TickRegen(double dtMs, const RegenModifiers& mods) { ABYSS_UNIMPLEMENTED(); }

Combatant Hero::AsCombatant() const {
  ABYSS_UNIMPLEMENTED();
  Combatant c;
  c.stats = stats_;
  c.baseDamage = derived_.baseDamage;
  c.defense = derived_.defense;
  c.mana = mana_;
  c.maxHp = derived_.maxHp;
  c.buffs = &buffs_;
  c.eq = &eq_;
  return c;
}

void Hero::ToSave(SaveHero& out) const { ABYSS_UNIMPLEMENTED(); }

void Hero::FromSave(const SaveHero& in) { ABYSS_UNIMPLEMENTED(); }

}  // namespace abyss

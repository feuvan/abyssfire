// The hero (classes-stats-skills.md 1.7, 3-6, 18; C1, C2, C12). Web source: src/entities/Player.ts (recalcDerived,
// addExp, update regen), ZoneScene.ts:1372-1379 (Life Regen passive), :4298-4314 (restore). Owner area: hero.
#include "abyss/base/Platform.h"

#include "abyss/hero/Hero.h"

#include <algorithm>
#include <cmath>

#include "abyss/base/Assert.h"
#include "abyss/base/Units.h"
#include "abyss/data/DataStore.h"
#include "abyss/save/SaveData.h"

namespace abyss {

namespace {
const ClassDef& HeroEmptyClassDef() {
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
  // Player constructor: derived without gear (an all-zero bag gives the same numbers), full HP / MP.
  RecalcDerived(EquipStats{});
  FillHpMana();
}

const ClassDef& Hero::ClassData() const { return class_ != nullptr ? *class_ : HeroEmptyClassDef(); }

int64_t Hero::ExpToNext() const { return data_->Classes().formulas.ExpToNext(level_); }

// ---------------------------------------------------------------------------------------------------------------------
// Progression (5.2, 6)
// ---------------------------------------------------------------------------------------------------------------------
LevelUpResult Hero::AddExp(int64_t amount, const EquipStats& eq) {
  const HeroFormulas& f = data_->Classes().formulas;
  exp_ += (std::max)(int64_t{0}, amount);
  if (f.levelCap > 0 && level_ >= f.levelCap) return {false, level_};  // no cap in the shipped data (levelCap null)
  const int64_t needed = ExpToNext();
  if (exp_ < needed) return {false, level_};
  // ONE level per call; the overflow stays in exp and converts on the next call.
  exp_ -= needed;
  ++level_;
  freeStatPoints_ += f.statPointsPerLevel;
  freeSkillPoints_ += f.skillPointsPerLevel;
  // C1 (FIX Q2): derive WITH gear before the refill. 5.1.1: a Dying hero keeps the level and points but is not
  // refilled (the respawn does that).
  RecalcDerived(eq);
  if (life_ == HeroLife::Alive) FillHpMana();
  return {true, level_};
}

void Hero::RemoveExp(int64_t amount) { exp_ = (std::max)(int64_t{0}, exp_ - (std::max)(int64_t{0}, amount)); }

void Hero::AddGold(int64_t amount) { gold_ += amount; }

bool Hero::SpendGold(int64_t amount) {
  if (amount < 0 || gold_ < amount) return false;
  gold_ -= amount;
  return true;
}

bool Hero::AllocateStat(PrimaryStat s) {
  // Character panel "+" (6): one point per call, no cap, no undo. Current hp / mana are not changed.
  if (freeStatPoints_ <= 0) return false;
  --freeStatPoints_;
  stats_.Ref(s) += 1;
  RecalcDerived(eq_);  // with the last merged gear bag (the next step recomputes it anyway)
  return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// Derived (3): recalcDerived with equipStats present. An all-zero bag reproduces the gear-less constructor values.
// ---------------------------------------------------------------------------------------------------------------------
void Hero::RecalcDerived(const EquipStats& eq) {
  const HeroFormulas& f = data_->Classes().formulas;
  eq_ = eq;
  // Effective primaries include gear (3.1 matrix: maxHp/maxMana/baseDamage/defense only).
  const double eStr = static_cast<double>(stats_.str) + eq.Get(Stat::Str);
  const double eVit = static_cast<double>(stats_.vit) + eq.Get(Stat::Vit);
  const double eSpi = static_cast<double>(stats_.spi) + eq.Get(Stat::Spi);
  const double eInt = static_cast<double>(stats_.int_) + eq.Get(Stat::Int);
  const double level = static_cast<double>(level_);

  double hp = f.maxHpBase + eVit * f.maxHpPerVit + (level - 1) * f.maxHpPerLevelAbove1;
  double mp = f.maxManaBase + eSpi * f.maxManaPerSpi + eInt * f.maxManaPerInt + (level - 1) * f.maxManaPerLevelAbove1;
  const double dmg = f.baseDamageBase + eStr * f.baseDamagePerStr + level * f.baseDamagePerLevel;
  const double def = f.defenseBase + eVit * f.defensePerVit + level * f.defensePerLevel;
  double spd = f.moveSpeed;
  double aspd = f.attackIntervalMs;

  hp += eq.Get(Stat::MaxHp);
  hp = std::floor(hp * (1 + eq.Get(Stat::MaxHpPercent) / 100));
  mp += eq.Get(Stat::MaxMana);
  mp = std::floor(mp * (1 + eq.Get(Stat::MaxManaPercent) / 100));
  spd = std::floor(spd * (1 + eq.Get(Stat::MoveSpeed) / 100));
  aspd = (std::max)(f.attackIntervalMinMs, std::floor(aspd * (1 - eq.Get(Stat::AttackSpeed) / 100)));

  derived_.maxHp = hp;
  derived_.maxMana = mp;
  derived_.baseDamage = dmg;
  derived_.defense = def;
  derived_.moveSpeed = std::floor(spd * spirit_.MoveSpeedMultiplier());
  derived_.attackSpeedMs = aspd;
  derived_.attackRange = f.attackRange;
}

double Hero::GroundSpeedTilesPerSec() const { return HeroTilesPerSecond(derived_.moveSpeed); }

// ---------------------------------------------------------------------------------------------------------------------
// Resources
// ---------------------------------------------------------------------------------------------------------------------
double Hero::Heal(double amount) {
  // 5.1.1: no HP gain unless Alive. Every web heal is `hp = Math.min(maxHp, hp + x)`, which also snaps an hp above a
  // lowered maxHp (3: recalcDerived does not clamp) down to maxHp.
  if (life_ != HeroLife::Alive || !std::isfinite(amount) || amount < 0) return 0;
  const double before = hp_;
  hp_ = (std::min)(derived_.maxHp, before + amount);
  return (std::max)(0.0, hp_ - before);
}

double Hero::RestoreMana(double amount) {
  // `mana = Math.min(maxMana, mana + x)` (potions, mana steal, free-cast refund, pet mana); snaps down like Heal.
  if (life_ != HeroLife::Alive || !std::isfinite(amount) || amount < 0) return 0;
  const double before = mana_;
  mana_ = (std::min)(derived_.maxMana, before + amount);
  return (std::max)(0.0, mana_ - before);
}

void Hero::SpendMana(double amount) { mana_ = (std::max)(0.0, mana_ - amount); }

double Hero::ApplyDamage(double amount) {
  const double before = hp_;
  hp_ = (std::max)(0.0, hp_ - amount);
  return before - hp_;
}

void Hero::FillHpMana() {
  hp_ = derived_.maxHp;
  mana_ = derived_.maxMana;
}

// Step 6 (4.2; ZoneScene.ts:1372-1379): the Life Regen passive, BEFORE the Unyielding check reads the hp ratio. Linear
// +perLevel HP/s per level, times the step's campfire / poison HP multiplier.
void Hero::TickLifeRegen(double dtMs, const RegenModifiers& mods) {
  if (life_ != HeroLife::Alive || hp_ <= 0) return;
  const std::vector<SkillDef>& skills = ClassData().skills;
  for (size_t i = 0; i < skills.size(); ++i) {
    const SkillDef& s = skills[i];
    if (s.passiveRule.kind != PassiveRuleKind::Regen) continue;
    const int32_t level = skills_.Level(static_cast<int32_t>(i));
    if (level <= 0) continue;
    const double regenBonus = static_cast<double>(level) * s.passiveRule.hpPerSecondPerLevel;
    if (hp_ < derived_.maxHp && hp_ > 0) {
      hp_ = (std::min)(derived_.maxHp, hp_ + regenBonus * dtMs / 1000 * mods.hpMul);
    }
  }
}

// Step 7 (4.1; Player.update after the spirit drain): mana regen, then HP regen. Primary stats are RAW (3.1); gear
// hpRegen / manaRegen come from the last RecalcDerived bag.
void Hero::TickRegen(double dtMs, const RegenModifiers& mods) {
  if (life_ != HeroLife::Alive || hp_ <= 0) return;
  const HeroFormulas& f = data_->Classes().formulas;
  if (mana_ < derived_.maxMana) {
    const double perSec = f.manaRegenBase + static_cast<double>(stats_.spi) * f.manaRegenPerSpi;
    mana_ = (std::min)(derived_.maxMana, mana_ + (perSec + eq_.Get(Stat::ManaRegen)) * mods.mpMul * dtMs / 1000);
  }
  if (hp_ < derived_.maxHp && hp_ > 0) {
    const double perSec = f.hpRegenBase + static_cast<double>(stats_.vit) * f.hpRegenPerVit;
    hp_ = (std::min)(derived_.maxHp, hp_ + (perSec + eq_.Get(Stat::HpRegen)) * mods.hpMul * dtMs / 1000);
  }
}

Combatant Hero::AsCombatant() const {
  // toCombatEntity (12): raw stats, derived baseDamage / defense, merged bag, outgoing = spirit damage multiplier.
  Combatant c;
  c.stats = stats_;
  c.baseDamage = derived_.baseDamage;
  c.defense = derived_.defense;
  c.mana = mana_;
  c.maxHp = derived_.maxHp;
  c.buffs = &buffs_;
  c.eq = &eq_;
  c.outgoingMultiplier = spirit_.DamageMultiplier();
  c.extraCritPercent = 0;  // the shadow_step crit buff (FIX Q18) is added by CombatSystem
  return c;
}

// ---------------------------------------------------------------------------------------------------------------------
// Save (18; save-ui-input 3.2 player section, 3.5 step 1)
// ---------------------------------------------------------------------------------------------------------------------
void Hero::ToSave(SaveHero& out) const {
  out.level = level_;
  out.exp = exp_;
  out.gold = gold_;
  out.hp = hp_;
  out.maxHp = derived_.maxHp;
  out.mana = mana_;
  out.maxMana = derived_.maxMana;
  out.stats = stats_;
  out.freeStatPoints = freeStatPoints_;
  out.freeSkillPoints = freeSkillPoints_;
  out.skillLevels = skills_.LevelsForSave();
  out.hasSpirit = true;
  out.spirit = spirit_.ToSave();
  out.tileCol = pos_.x;
  out.tileRow = pos_.y;
  // currentMap is the session's (GameSim BuildSave).
}

void Hero::FromSave(const SaveHero& in) {
  // Restore step 1 (3.5) with load normalisations: level >= 1, non-negative exp / gold / points (a corrupt level 0
  // would make expToNext 0). Levels clamp to [0, maxLevel] (SkillBook::LoadLevels). Spirit.restore clamps.
  level_ = (std::max)(1, in.level);
  exp_ = (std::max)(int64_t{0}, in.exp);
  gold_ = (std::max)(int64_t{0}, in.gold);
  stats_ = in.stats;
  freeStatPoints_ = (std::max)(0, in.freeStatPoints);
  freeSkillPoints_ = (std::max)(0, in.freeSkillPoints);
  skills_.LoadLevels(in.skillLevels);
  skills_.ResetCooldowns();
  skills_.FillHotbarDefault();  // saves without `hotbar`; GameSim then applies SaveData.hotbar when present (C3)
  spirit_.Restore(in.hasSpirit ? in.spirit : SpiritSaveState{});
  buffs_.Clear();
  life_ = HeroLife::Alive;
  RecalcDerived(eq_);
  // Raw values; GameSim re-derives with gear and clamps (FIX Q8) or respawns a dead save (FIX Q35).
  hp_ = std::isfinite(in.hp) ? in.hp : 0.0;
  mana_ = std::isfinite(in.mana) ? in.mana : derived_.maxMana;
}

}  // namespace abyss

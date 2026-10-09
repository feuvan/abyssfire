// Skill levels, progression gates, scaling, hotbar, cooldowns (classes-stats-skills.md sections 7-8; C1, C3).
// Web sources: src/systems/CombatSystem.ts (tieredScale and the getSkill* scaling functions),
// src/systems/SkillProgressionSystem.ts (starter levels, gates, investment, loadout). Owner area: hero.
#include "abyss/base/Platform.h"

#include "abyss/hero/Skills.h"

#include <algorithm>
#include <cmath>

#include "abyss/base/Assert.h"
#include "abyss/base/Math.h"
#include "abyss/data/DataStore.h"

namespace abyss {

// ---------------------------------------------------------------------------------------------------------------------
// Scaling (8). Same summation order and floor/ceil calls as the web.
// ---------------------------------------------------------------------------------------------------------------------
double TieredScale(const SkillRules& rules, double per, int32_t level) {
  if (level <= 1) return 0;
  double total = 0;
  for (int32_t i = 2; i <= level; ++i) {
    // Brackets in data order; the first one containing i applies (web: i <= 8 -> x1, i <= 16 -> x0.75, else x0.5).
    for (const SkillTierWeight& b : rules.tierWeights) {
      if (i >= b.fromLevel && (!b.hasToLevel || i <= b.toLevel)) {
        total += per * b.weight;  // per * 1.0 == per exactly, so the x1 bracket matches the web's `total += per`
        break;
      }
    }
  }
  return total;
}

double SkillDamageMultiplier(const SkillRules& rules, const SkillDef& s, int32_t level) {
  return s.damageMultiplier + TieredScale(rules, s.scaling.damagePerLevel, level);
}

int32_t SkillManaCost(const SkillRules& rules, const SkillDef& s, int32_t level) {
  return FloorInt(s.manaCost + TieredScale(rules, s.scaling.manaCostPerLevel, level));
}

double SkillCooldownMs(const SkillRules& rules, const SkillDef& s, int32_t level, double cdrPercent) {
  const double raw =
      (std::max)(rules.cooldownFloorMs, std::floor(s.cooldownMs - TieredScale(rules, s.scaling.cooldownReductionPerLevel, level)));
  const double cdr = (std::max)(0.0, (std::min)(rules.cooldownReductionCapPercent, cdrPercent));
  return std::floor(raw * (1.0 - cdr / 100.0));
}

double SkillAoeRadius(const SkillRules& rules, const SkillDef& s, int32_t level) {
  return s.aoeRadius + TieredScale(rules, s.scaling.aoeRadiusPerLevel, level);
}

double SkillBuffValue(const SkillRules& rules, const SkillDef& s, int32_t level) {
  const double base = s.hasBuff ? s.buff.value : 0.0;
  return base + TieredScale(rules, s.scaling.buffValuePerLevel, level);
}

double SkillBuffDurationMs(const SkillRules& rules, const SkillDef& s, int32_t level) {
  const double base = s.hasBuff ? s.buff.durationMs : 0.0;
  return std::floor(base + TieredScale(rules, s.scaling.buffDurationPerLevel, level));
}

int32_t SkillCastMana(const SkillRules& rules, const SkillDef& s, int32_t level, double spiritManaCostMul) {
  return CeilInt(static_cast<double>(SkillManaCost(rules, s, level)) * spiritManaCostMul);
}

// ---------------------------------------------------------------------------------------------------------------------
// SkillBook
// ---------------------------------------------------------------------------------------------------------------------
namespace {
const ClassDef& SkillBookEmptyClassDef() {
  static const ClassDef kEmpty;
  return kEmpty;
}
const SkillDef& SkillBookEmptySkillDef() {
  static const SkillDef kEmpty;
  return kEmpty;
}
}  // namespace

SkillBook::SkillBook(const DataStore& data, ClassId cls) : data_(&data), class_(data.Classes().Find(cls)) {
  ABYSS_ASSERT(class_ != nullptr, "SkillBook: unknown class");
  const size_t n = class_ ? class_->skills.size() : 0;
  levels_.assign(n, 0);
  readyAt_.assign(n, 0.0);
  hotbar_.fill(-1);
}

const ClassDef& SkillBook::Class() const { return class_ != nullptr ? *class_ : SkillBookEmptyClassDef(); }

const SkillDef& SkillBook::Skill(int32_t index) const {
  if (class_ == nullptr || index < 0 || static_cast<size_t>(index) >= class_->skills.size()) {
    return SkillBookEmptySkillDef();
  }
  return class_->skills[static_cast<size_t>(index)];
}

int32_t SkillBook::IndexOf(std::string_view skillId) const {
  return class_ != nullptr ? class_->SkillIndex(skillId) : -1;
}

void SkillBook::InitStarterLevels() {
  // getStarterSkillLevels (7.1): tier 1 -> level 1, others 0.
  const SkillRules& rules = data_->Classes().skillRules;
  for (size_t i = 0; i < levels_.size(); ++i) {
    levels_[i] = Skill(static_cast<int32_t>(i)).tier == 1 ? rules.starterLevelTier1 : rules.starterLevelOther;
  }
  FillHotbarDefault();
}

int32_t SkillBook::Level(int32_t index) const {
  if (index < 0 || static_cast<size_t>(index) >= levels_.size()) return 0;
  return levels_[static_cast<size_t>(index)];
}

int32_t SkillBook::Level(std::string_view skillId) const { return Level(IndexOf(skillId)); }

void SkillBook::SetLevel(int32_t index, int32_t level) {
  if (index < 0 || static_cast<size_t>(index) >= levels_.size()) return;
  levels_[static_cast<size_t>(index)] = level;
}

int32_t SkillBook::InvestedTreePoints(std::string_view treeId) const {
  // getTreeInvestedPoints (7.2): sum of max(0, level) over the tree's skills, starter levels included.
  int32_t total = 0;
  for (size_t i = 0; i < levels_.size(); ++i) {
    if (Skill(static_cast<int32_t>(i)).tree == treeId) total += (std::max)(0, levels_[i]);
  }
  return total;
}

InvestState SkillBook::GetInvestState(int32_t index, int32_t heroLevel, int32_t freeSkillPoints) const {
  InvestState st;
  if (index < 0 || static_cast<size_t>(index) >= levels_.size()) {
    st.canInvest = false;
    st.reason = InvestBlock::Maxed;  // nothing to invest in
    return st;
  }
  const SkillDef& s = Skill(index);
  const SkillRules& rules = data_->Classes().skillRules;
  const int32_t currentLevel = (std::max)(0, Level(index));
  st.requiredPlayerLevel = rules.RequiredPlayerLevel(s.tier);
  st.requiredTreePoints = rules.RequiredTreePoints(s.tier);
  st.investedTreePoints = InvestedTreePoints(s.tree);

  // Checks in the web's order (7.3).
  InvestBlock reason = InvestBlock::Ready;
  if (currentLevel >= s.maxLevel) {
    reason = InvestBlock::Maxed;
  } else if (freeSkillPoints <= 0) {
    reason = InvestBlock::NoPoints;
  } else if (heroLevel < st.requiredPlayerLevel) {
    reason = InvestBlock::PlayerLevel;
  } else if (st.investedTreePoints < st.requiredTreePoints) {
    reason = InvestBlock::TreePoints;
  } else if (s.tier > 1) {
    bool hasPreviousTier = false;
    for (size_t i = 0; i < levels_.size() && !hasPreviousTier; ++i) {
      const SkillDef& c = Skill(static_cast<int32_t>(i));
      hasPreviousTier = c.tree == s.tree && c.tier == s.tier - 1 && levels_[i] > 0;
    }
    if (!hasPreviousTier) reason = InvestBlock::PreviousTier;
  }
  st.reason = reason;
  st.canInvest = reason == InvestBlock::Ready;
  return st;
}

bool SkillBook::Invest(int32_t index, int32_t heroLevel, int32_t& freeSkillPoints) {
  if (!GetInvestState(index, heroLevel, freeSkillPoints).canInvest) return false;
  int32_t& level = levels_[static_cast<size_t>(index)];
  level = (std::max)(0, level) + 1;
  freeSkillPoints -= 1;
  if (level == 1) AutoFillHotbar(index);  // C3: a newly learned skill takes the first empty slot
  return true;
}

double SkillBook::SynergyFactor(const SkillDef& s) const {
  // getSynergyBonus: 1 when the skill has no synergies; otherwise 1 + (sum in list order, starting from 0).
  if (s.synergies.empty()) return 1.0;
  double bonus = 0;
  for (const SkillSynergy& syn : s.synergies) bonus += syn.damagePerLevel * static_cast<double>(Level(syn.skillId));
  return 1.0 + bonus;
}

std::vector<int32_t> SkillBook::LearnedLoadout(int32_t limit) const {
  std::vector<int32_t> out;
  for (size_t i = 0; i < levels_.size() && static_cast<int32_t>(out.size()) < limit; ++i) {
    if (levels_[i] > 0 && !Skill(static_cast<int32_t>(i)).passive) out.push_back(static_cast<int32_t>(i));
  }
  return out;
}

int32_t SkillBook::HotbarCapacity() const {
  const int32_t size = data_->Classes().skillRules.loadoutSize;
  return Clamp(size, 0, kHotbarSlots);
}

int32_t SkillBook::HotbarSkill(int32_t slot) const {
  if (slot < 0 || slot >= kHotbarSlots) return -1;
  return hotbar_[static_cast<size_t>(slot)];
}

int32_t SkillBook::HotbarSlotOf(int32_t skillIndex) const {
  if (skillIndex < 0) return -1;
  for (int32_t i = 0; i < kHotbarSlots; ++i) {
    if (hotbar_[static_cast<size_t>(i)] == skillIndex) return i;
  }
  return -1;
}

bool SkillBook::SetHotbar(int32_t slot, int32_t skillIndex) {
  if (slot < 0 || slot >= HotbarCapacity()) return false;
  if (skillIndex == -1) {
    hotbar_[static_cast<size_t>(slot)] = -1;
    return true;
  }
  if (skillIndex < 0 || static_cast<size_t>(skillIndex) >= levels_.size()) return false;
  if (Skill(skillIndex).passive || Level(skillIndex) <= 0) return false;  // C1: passives never on the hotbar
  const int32_t previous = HotbarSlotOf(skillIndex);
  if (previous == slot) return true;
  const int32_t displaced = hotbar_[static_cast<size_t>(slot)];
  hotbar_[static_cast<size_t>(slot)] = skillIndex;
  if (previous >= 0) hotbar_[static_cast<size_t>(previous)] = displaced;
  return true;
}

void SkillBook::FillHotbarDefault() {
  hotbar_.fill(-1);
  const std::vector<int32_t> loadout = LearnedLoadout(HotbarCapacity());
  for (size_t i = 0; i < loadout.size(); ++i) hotbar_[i] = loadout[i];
}

void SkillBook::AutoFillHotbar(int32_t index) {
  if (Skill(index).passive || Level(index) <= 0 || HotbarSlotOf(index) >= 0) return;
  const int32_t capacity = HotbarCapacity();
  for (int32_t i = 0; i < capacity; ++i) {
    if (hotbar_[static_cast<size_t>(i)] < 0) {
      hotbar_[static_cast<size_t>(i)] = index;
      return;
    }
  }
}

double SkillBook::ReadyAtMs(int32_t index) const {
  if (index < 0 || static_cast<size_t>(index) >= readyAt_.size()) return 0;
  return readyAt_[static_cast<size_t>(index)];
}

void SkillBook::StartCooldown(int32_t index, double readyAtMs) {
  if (index < 0 || static_cast<size_t>(index) >= readyAt_.size()) return;
  readyAt_[static_cast<size_t>(index)] = readyAtMs;
}

void SkillBook::ResetCooldowns() { readyAt_.assign(readyAt_.size(), 0.0); }

std::vector<std::pair<std::string, int32_t>> SkillBook::LevelsForSave() const {
  std::vector<std::pair<std::string, int32_t>> out;
  out.reserve(levels_.size());
  for (size_t i = 0; i < levels_.size(); ++i) out.emplace_back(Skill(static_cast<int32_t>(i)).id, levels_[i]);
  return out;
}

void SkillBook::LoadLevels(const std::vector<std::pair<std::string, int32_t>>& levels) {
  levels_.assign(levels_.size(), 0);
  for (const auto& [id, level] : levels) {
    const int32_t index = IndexOf(id);
    if (index < 0) continue;
    SetLevel(index, Clamp(level, 0, (std::max)(0, Skill(index).maxLevel)));
  }
}

std::array<std::string, SkillBook::kHotbarSlots> SkillBook::HotbarForSave() const {
  std::array<std::string, kHotbarSlots> out{};
  for (int32_t i = 0; i < kHotbarSlots; ++i) {
    const int32_t s = hotbar_[static_cast<size_t>(i)];
    if (s >= 0) out[static_cast<size_t>(i)] = Skill(s).id;
  }
  return out;
}

void SkillBook::LoadHotbar(const std::array<std::string, kHotbarSlots>& ids) {
  hotbar_.fill(-1);
  const int32_t capacity = HotbarCapacity();
  for (int32_t slot = 0; slot < capacity; ++slot) {
    const std::string& id = ids[static_cast<size_t>(slot)];
    if (id.empty()) continue;
    const int32_t index = IndexOf(id);
    if (index < 0 || Skill(index).passive || Level(index) <= 0 || HotbarSlotOf(index) >= 0) continue;
    hotbar_[static_cast<size_t>(slot)] = index;
  }
}

}  // namespace abyss

// Skill levels, progression gates, scaling, hotbar, cooldowns (classes-stats-skills.md sections 7-8; C1, C3).
// STUB: owner area hero+combat. The constructor, accessors and hotbar/cooldown storage are real; formulas and
// progression rules are stubs.
#include "abyss/base/Platform.h"

#include "abyss/hero/Skills.h"

#include "abyss/base/Assert.h"
#include "abyss/data/DataStore.h"

namespace abyss {

double TieredScale(const SkillRules& rules, double per, int32_t level) {
  ABYSS_UNIMPLEMENTED();
  return 0;
}

double SkillDamageMultiplier(const SkillRules& rules, const SkillDef& s, int32_t level) {
  ABYSS_UNIMPLEMENTED();
  return s.damageMultiplier;
}

int32_t SkillManaCost(const SkillRules& rules, const SkillDef& s, int32_t level) {
  ABYSS_UNIMPLEMENTED();
  return static_cast<int32_t>(s.manaCost);
}

double SkillCooldownMs(const SkillRules& rules, const SkillDef& s, int32_t level, double cdrPercent) {
  ABYSS_UNIMPLEMENTED();
  return s.cooldownMs;
}

double SkillAoeRadius(const SkillRules& rules, const SkillDef& s, int32_t level) {
  ABYSS_UNIMPLEMENTED();
  return s.aoeRadius;
}

double SkillBuffValue(const SkillRules& rules, const SkillDef& s, int32_t level) {
  ABYSS_UNIMPLEMENTED();
  return s.buff.value;
}

double SkillBuffDurationMs(const SkillRules& rules, const SkillDef& s, int32_t level) {
  ABYSS_UNIMPLEMENTED();
  return s.buff.durationMs;
}

int32_t SkillCastMana(const SkillRules& rules, const SkillDef& s, int32_t level, double spiritManaCostMul) {
  ABYSS_UNIMPLEMENTED();
  return static_cast<int32_t>(s.manaCost);
}

SkillBook::SkillBook(const DataStore& data, ClassId cls) : data_(&data), class_(data.Classes().Find(cls)) {
  ABYSS_ASSERT(class_ != nullptr, "SkillBook: unknown class");
  const size_t n = class_ ? class_->skills.size() : 0;
  levels_.assign(n, 0);
  readyAt_.assign(n, 0.0);
  hotbar_.fill(-1);
}

namespace {
const ClassDef& EmptyClassDef() {
  static const ClassDef kEmpty;
  return kEmpty;
}
const SkillDef& EmptySkillDef() {
  static const SkillDef kEmpty;
  return kEmpty;
}
}  // namespace

const ClassDef& SkillBook::Class() const { return class_ != nullptr ? *class_ : EmptyClassDef(); }

const SkillDef& SkillBook::Skill(int32_t index) const {
  if (class_ == nullptr || index < 0 || static_cast<size_t>(index) >= class_->skills.size()) return EmptySkillDef();
  return class_->skills[static_cast<size_t>(index)];
}

int32_t SkillBook::IndexOf(std::string_view skillId) const {
  return class_ != nullptr ? class_->SkillIndex(skillId) : -1;
}

void SkillBook::InitStarterLevels() {
  ABYSS_UNIMPLEMENTED();
  (void)data_;
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
  ABYSS_UNIMPLEMENTED();
  return 0;
}

InvestState SkillBook::GetInvestState(int32_t index, int32_t heroLevel, int32_t freeSkillPoints) const {
  ABYSS_UNIMPLEMENTED();
  return {};
}

bool SkillBook::Invest(int32_t index, int32_t heroLevel, int32_t& freeSkillPoints) {
  ABYSS_UNIMPLEMENTED();
  return false;
}

double SkillBook::SynergyFactor(const SkillDef& s) const {
  ABYSS_UNIMPLEMENTED();
  return 1.0;
}

int32_t SkillBook::HotbarSkill(int32_t slot) const {
  if (slot < 0 || slot >= kHotbarSlots) return -1;
  return hotbar_[static_cast<size_t>(slot)];
}

bool SkillBook::SetHotbar(int32_t slot, int32_t skillIndex) {
  ABYSS_UNIMPLEMENTED();
  return false;
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
  for (const auto& [id, level] : levels) SetLevel(IndexOf(id), level);
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
  ABYSS_UNIMPLEMENTED();
}

}  // namespace abyss

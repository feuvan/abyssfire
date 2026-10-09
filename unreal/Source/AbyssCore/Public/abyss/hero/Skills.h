// Skill levels, progression gates, scaling formulas, explicit hotbar and cooldowns.
// Spec: classes-stats-skills.md 7 (progression), 8 (scaling), DECISIONS C1 (passives off the hotbar), C3 (explicit
// player-editable 6-slot hotbar saved in the save; new skills auto-fill the first empty slot).
//
// Owner area: hero+combat. Skills are referenced by their index in the class definition (ClassDef::skills order).
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "abyss/base/Platform.h"
#include "abyss/data/ClassData.h"

namespace abyss {

class DataStore;

// ---- scaling (pure, 8) ----
// tieredScale(per, L): loop over levels 2..L with the skill_rules tier weights (summation order kept).
ABYSS_API double TieredScale(const SkillRules& rules, double per, int32_t level);
ABYSS_API double SkillDamageMultiplier(const SkillRules& rules, const SkillDef& s, int32_t level);
ABYSS_API int32_t SkillManaCost(const SkillRules& rules, const SkillDef& s, int32_t level);
// floor(max(floor, floor(cd - tiered)) * (1 - clamp(cdr, 0, cap)/100)).
ABYSS_API double SkillCooldownMs(const SkillRules& rules, const SkillDef& s, int32_t level, double cdrPercent);
ABYSS_API double SkillAoeRadius(const SkillRules& rules, const SkillDef& s, int32_t level);
ABYSS_API double SkillBuffValue(const SkillRules& rules, const SkillDef& s, int32_t level);
ABYSS_API double SkillBuffDurationMs(const SkillRules& rules, const SkillDef& s, int32_t level);
// ceil(manaCost(L) * spirit mana-cost multiplier).
ABYSS_API int32_t SkillCastMana(const SkillRules& rules, const SkillDef& s, int32_t level, double spiritManaCostMul);

enum class InvestBlock : uint8_t { Ready, Maxed, NoPoints, PlayerLevel, TreePoints, PreviousTier };

struct InvestState {
  bool canInvest = false;
  InvestBlock reason = InvestBlock::Ready;
  int32_t requiredPlayerLevel = 1;
  int32_t requiredTreePoints = 0;
  int32_t investedTreePoints = 0;
};

class ABYSS_API SkillBook {
 public:
  static constexpr int32_t kHotbarSlots = 6;

  // An unknown class (unfinalized data) reports an assert and leaves an empty book: Class() is an empty ClassDef,
  // SkillCount() 0, IndexOf -1, Skill() only valid for 0 <= index < SkillCount().
  SkillBook(const DataStore& data, ClassId cls);

  const ClassDef& Class() const;
  size_t SkillCount() const { return levels_.size(); }
  // Precondition: 0 <= index < SkillCount() (an out-of-range index returns an empty SkillDef).
  const SkillDef& Skill(int32_t index) const;
  int32_t IndexOf(std::string_view skillId) const;

  // Starter levels (7.1): tier-1 skills at level 1, others 0. Also fills the hotbar with them (non-passive).
  void InitStarterLevels();
  int32_t Level(int32_t index) const;
  int32_t Level(std::string_view skillId) const;
  void SetLevel(int32_t index, int32_t level);

  // Invested tree points = sum of current levels in the tree (starter levels included).
  int32_t InvestedTreePoints(std::string_view treeId) const;
  // getSkillInvestmentState (7.3), checks in the spec order.
  InvestState GetInvestState(int32_t index, int32_t heroLevel, int32_t freeSkillPoints) const;
  // investSkillPoint: +1 level and -1 point on success; a skill that just reached level 1 auto-fills the first empty
  // hotbar slot (C3, never passives). Returns false (state unchanged) when blocked.
  bool Invest(int32_t index, int32_t heroLevel, int32_t& freeSkillPoints);

  // Synergy factor: 1 + sum(syn.damagePerLevel * level(syn.skillId)).
  double SynergyFactor(const SkillDef& s) const;

  // getLearnedSkillLoadout (7.5) with the C1 fix: learned (level > 0), non-passive skills in definition order, the first
  // `limit` (limit <= 0 -> empty). This is the default hotbar fill.
  std::vector<int32_t> LearnedLoadout(int32_t limit) const;

  // ---- hotbar (C3) ----
  // Usable slots: min(kHotbarSlots, skill_rules loadoutSize).
  int32_t HotbarCapacity() const;
  int32_t HotbarSkill(int32_t slot) const;  // skill index or -1
  int32_t HotbarSlotOf(int32_t skillIndex) const;  // slot or -1
  // Binds a learned, non-passive skill (or clears the slot with -1). A skill already bound elsewhere moves to this slot
  // and the skill it displaces (if any) takes the vacated slot (swap). False (unchanged) for an invalid slot, an
  // unknown, unlearned or passive skill.
  bool SetHotbar(int32_t slot, int32_t skillIndex);
  // Clears the hotbar and fills it with LearnedLoadout(HotbarCapacity()) (new game, saves without a `hotbar`).
  void FillHotbarDefault();
  const std::array<int32_t, kHotbarSlots>& Hotbar() const { return hotbar_; }

  // ---- cooldowns (absolute SimClock ready times; reset on zone change, Q23; not saved) ----
  double ReadyAtMs(int32_t index) const;
  bool IsReady(int32_t index, double nowMs) const { return nowMs >= ReadyAtMs(index); }
  void StartCooldown(int32_t index, double readyAtMs);
  void ResetCooldowns();

  // ---- save ----
  // skillLevels as {skillId: level} in class definition order.
  std::vector<std::pair<std::string, int32_t>> LevelsForSave() const;
  // Missing keys = 0; unknown ids ignored; levels clamped to [0, maxLevel]; a repeated id keeps its last value (JS
  // object / Map semantics).
  void LoadLevels(const std::vector<std::pair<std::string, int32_t>>& levels);
  std::array<std::string, kHotbarSlots> HotbarForSave() const;  // skill ids ("" = empty)
  // Restores explicit slot bindings (C3). Call after LoadLevels. Empty, unknown, unlearned, passive and repeated ids
  // leave their slot empty; no auto-fill (the player may have cleared slots on purpose).
  void LoadHotbar(const std::array<std::string, kHotbarSlots>& ids);

 private:
  // C3: puts a learned, non-passive skill that is not on the hotbar into the first empty usable slot.
  void AutoFillHotbar(int32_t index);

  const DataStore* data_;
  const ClassDef* class_;
  std::vector<int32_t> levels_;
  std::vector<double> readyAt_;
  std::array<int32_t, kHotbarSlots> hotbar_{};
};

}  // namespace abyss

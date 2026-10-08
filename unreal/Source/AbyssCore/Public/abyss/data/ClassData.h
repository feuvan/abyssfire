// Classes, skill trees, hero formulas, buff caps, spirit profiles.
// Sources: classes.json, skill_trees.json, hero_formulas.json, buff_caps.json, spirit_profiles.json.
// Spec: classes-stats-skills.md 1-6, 11, 14.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/Platform.h"
#include "abyss/base/Stats.h"
#include "abyss/data/SkillData.h"

namespace abyss {

struct ABYSS_API ClassDef {
  std::string id;
  ClassId cls = ClassId::Warrior;
  std::string name, nameEn, description;  // zh-CN fallbacks (i18n data.class.<id>.*)
  PrimaryStats baseStats;
  PrimaryStats statGrowth;  // unused by design (C2), kept for a later balance pass
  std::vector<SkillDef> skills;  // definition order matters (hotbar default, auto-combat priority)

  const SkillDef* FindSkill(std::string_view skillId) const;
  int32_t SkillIndex(std::string_view skillId) const;  // -1 when absent
};

struct SkillTreeDef {
  std::string id;
  ClassId classId = ClassId::Warrior;
  int32_t tabOrder = 0;
  std::string nameKey;
  uint32_t color = 0;
};

// hero_formulas.json (classes 3-5, 12).
struct ABYSS_API HeroFormulas {
  // derived stats
  double maxHpBase = 50, maxHpPerVit = 10, maxHpPerLevelAbove1 = 15;
  double maxManaBase = 30, maxManaPerSpi = 8, maxManaPerInt = 3, maxManaPerLevelAbove1 = 8;
  double baseDamageBase = 8, baseDamagePerStr = 0.8, baseDamagePerLevel = 2;
  double defenseBase = 3, defensePerVit = 0.5, defensePerLevel = 1;
  double moveSpeed = 120;          // iso px/s (web units); ground speed = moveSpeed / 36 tiles/s (S5)
  double attackIntervalMs = 1000;
  double attackIntervalMinMs = 200;
  double attackRange = 1.5;
  // regen
  double hpRegenBase = 0.5, hpRegenPerVit = 0.05;
  double manaRegenBase = 1, manaRegenPerSpi = 0.1;
  double campfireRadiusTiles = 5;
  double campfireHpMultiplier = 50, campfireManaMultiplier = 50;
  double poisonedHpRegenMultiplier = 0.5;
  // leveling: expToNext(L) = floor(a*L*L + b*L)
  double expToNextA = 3, expToNextB = 25;
  int32_t statPointsPerLevel = 5;
  int32_t skillPointsPerLevel = 1;
  int32_t levelCap = 0;  // 0 = none
  // damage formula constants (classes 12)
  double dodgePerDex = 0.3, dodgeCapPercent = 30;
  double critPerDex = 0.2, critPerLck = 0.5, critCapPercent = 75;
  double critMultiplierBase = 1.5, critMultiplierPerLck = 0.01;
  double statToDamage = 0.5;
  double defenseFactor = 0.5;
  double resistCapPercent = 75;
  double damageReductionCap = 0.9;
  double outgoingMultiplierMin = 0, outgoingMultiplierMax = 10;
  double minDamage = 1;

  int64_t ExpToNext(int32_t level) const;
};

// buff_caps.json: getBuffValue caps (absent = uncapped).
struct ABYSS_API BuffCaps {
  std::array<double, EnumCount<BuffStat>()> cap{};
  std::array<bool, EnumCount<BuffStat>()> capped{};
  double Apply(BuffStat s, double total) const;
};

// spirit_profiles.json (classes 14).
struct SpiritProfileDef {
  std::string id;
  ClassId classId = ClassId::Warrior;
  uint32_t visualColor = 0;
  double maxValue = 100;
  double hitGain = 0, killGain = 0, dodgeGain = 0, critBonusGain = 0;
  double resonanceDurationMs = 0;
  double resonanceDamageBonus = 0;
  double resonanceManaCostMultiplier = 1;
  double resonanceMoveSpeedBonus = 0;
};

struct SpiritTable {
  std::array<SpiritProfileDef, EnumCount<ClassId>()> profiles{};
  ClassId fallbackClass = ClassId::Warrior;
  double spiGainFactor = 0.015;
  double spiClampMin = 0, spiClampMax = 200;
  const SpiritProfileDef& For(ClassId c) const { return profiles[static_cast<size_t>(c)]; }
};

struct ABYSS_API ClassTables {
  std::vector<ClassDef> classes;  // classOrder
  std::vector<SkillTreeDef> trees;
  std::array<uint32_t, EnumCount<DamageType>()> damageTypeColors{};
  SkillRules skillRules;
  HeroFormulas formulas;
  BuffCaps buffCaps;
  SpiritTable spirit;
  StatusEffectRules statusRules;

  const ClassDef* Find(ClassId c) const;
  const ClassDef* Find(std::string_view id) const;
  // Searches every class (skill ids are unique across classes).
  const SkillDef* FindSkill(std::string_view skillId) const;
  const SkillTreeDef* FindTree(std::string_view treeId) const;
};

}  // namespace abyss

// Skill definitions and skill/status rule tables.
// Sources: Data/classes.json (skills + exporter `derived` block, DECISIONS C6), skill_rules.json,
// status_effects.json. Spec: classes-stats-skills.md 1.2-1.4, 7-9, 13; combat-feel.md 6-7.
// Field names follow the JSON (camelCase). Times are ms, distances tiles.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/Platform.h"

namespace abyss {

// Buff stats (classes-stats-skills.md 11.1; buff_caps.json caps + uncapped).
enum class BuffStat : uint8_t {
  DamageReduction,
  DefenseBonus,
  DamageBonus,
  AttackSpeed,
  PoisonDamage,
  StealthDamage,
  ManaShield,
  DamageAmplify,
  CritBonus,
  SlowEffect,
  Taunted,
};
ABYSS_ENUM_STRINGS(BuffStat, "damageReduction", "defenseBonus", "damageBonus", "attackSpeed", "poisonDamage",
                   "stealthDamage", "manaShield", "damageAmplify", "critBonus", "slowEffect", "taunted")

// releaseSkill branch (classes 9.5) - exporter field derived.execKind.
enum class SkillExecKind : uint8_t { Teleport, ShadowStep, DeathMark, SlowTrap, Buff, Aoe, Single };
ABYSS_ENUM_STRINGS(SkillExecKind, "teleport", "shadow_step", "death_mark", "slow_trap", "buff", "aoe", "single")

enum class SkillAnimKind : uint8_t { Cast, Attack };
ABYSS_ENUM_STRINGS(SkillAnimKind, "cast", "attack")

enum class PassiveRuleKind : uint8_t { None, Regen, LowHpProc, DualWield };
ABYSS_ENUM_STRINGS(PassiveRuleKind, "none", "regen", "lowHpProc", "dualWield")

// How a status rule computes its value (classes 13.5, monsters derived.onHitStatus).
enum class StatusValueKind : uint8_t {
  Fixed,          // `amount`
  Damage,         // max(min, floor(dealtDamage * fraction))
  BuffPercent,    // round(buffValue(L) * 100) (slow_trap)
  MonsterDamage,  // max(min, floor(spawnScaledDef.damage * fraction)) (monster on-hit)
};
ABYSS_ENUM_STRINGS(StatusValueKind, "fixed", "damage", "buffPercent", "monsterDamage")

// One "apply status on hit" rule, evaluated in array order after each hit (C6).
struct StatusRule {
  StatusType status = StatusType::Burn;
  bool hasChance = false;     // false = no RNG draw (always applies)
  double chance = 0.0;        // 0..1, compared with rng.Float01() < chance
  StatusValueKind valueKind = StatusValueKind::Fixed;
  double amount = 0.0;        // Fixed
  double fraction = 0.0;      // Damage / MonsterDamage
  double min = 0.0;           // Damage / MonsterDamage
  bool hasDuration = false;   // false = use the skill's scaled buff duration (slow_trap)
  int32_t durationMs = 0;
};

// Which SkillScaling fields the skill's JSON states (bit per field). Absence is explicit data, never a NaN sentinel: a
// NaN check is folded away by fast-math and would silently keep NaN skill damage (Platform.h).
enum class ScalingField : uint8_t {
  DamagePerLevel,
  ManaCostPerLevel,
  CooldownReductionPerLevel,
  AoeRadiusPerLevel,
  BuffValuePerLevel,
  BuffDurationPerLevel,
};
constexpr uint8_t ScalingBit(ScalingField f) { return static_cast<uint8_t>(1u << static_cast<unsigned>(f)); }

struct SkillScaling {
  // Absent fields (bit clear in `present`) take skill_rules.json scaling.defaults (filled by DataStore::Finalize).
  double damagePerLevel = 0.05;
  double manaCostPerLevel = 0.5;
  double cooldownReductionPerLevel = 0.0;
  double aoeRadiusPerLevel = 0.0;
  double buffValuePerLevel = 0.02;
  double buffDurationPerLevel = 0.0;
  uint8_t present = 0;  // ScalingBit mask of the fields read from the skill's JSON
  bool Has(ScalingField f) const { return (present & ScalingBit(f)) != 0; }
};

struct SkillSynergy {
  std::string skillId;
  double damagePerLevel = 0.0;
};

struct SkillBuffDef {
  BuffStat stat = BuffStat::DamageReduction;
  double value = 0.0;
  double durationMs = 0.0;
};

struct SkillPassiveRule {
  PassiveRuleKind kind = PassiveRuleKind::None;
  double hpPerSecondPerLevel = 0.0;  // regen (life_regen: 2)
  double hpRatioBelow = 0.0;         // lowHpProc (unyielding: 0.3)
  double damageBonusPerLevel = 0.0;  // dualWield (0.03)
  int32_t buffDurationMs = 0;        // dualWield (2000)
  std::string tag;                   // dualWield buff tag
};

// clamp(isoPx * msPerPx, minMs, maxMs) - combat-feel.md 6.4 (port px = tileDist * 36, S4).
struct ProjectileTimingDef {
  double minMs = 0.0;
  double maxMs = 0.0;
  double msPerPx = 0.0;
};

// min(maxMs, px * msPerPx) per target (multishot, piercing_arrow).
struct ArrowDelayDef {
  double maxMs = 0.0;
  double msPerPx = 0.0;
};

// C4 persistent ground effect trigger (classes.json skills[].derived.port.groundTrigger).
//   Periodic: the effect ticks `groundTicks` times, the first tick at once (the cast still hits immediately, like the
//             web's one-shot), then every groundDurationMs / groundTicks; it ends at start + groundDurationMs.
//   Armed:    a trap: it waits (armed) up to groundDurationMs; the first step a living monster is inside its radius it
//             fires its `groundTicks` hits (the first at once, then every groundDurationMs / groundTicks counted from
//             the trigger) and ends; an untriggered trap expires silently.
// Each tick deals 1 / groundTicks of the skill hit (same total damage, C4) to every monster inside the radius.
enum class GroundTrigger : uint8_t { Periodic, Armed };
ABYSS_ENUM_STRINGS(GroundTrigger, "periodic", "armed")

struct SkillPortDef {
  bool hasDash = false;  // C4 Charge dash
  int32_t dashDurationMs = 0;
  bool dashStopAtMeleeRange = false;
  bool persistentGround = false;  // C4 Fire Wall / Arrow Rain / traps (fields below; Finalize checks them)
  int32_t groundDurationMs = 0;   // lifetime (Periodic) / armed time (Armed)
  int32_t groundTicks = 0;        // >= 1 when persistentGround; damage share per tick = 1 / groundTicks
  GroundTrigger groundTrigger = GroundTrigger::Periodic;
  int32_t chainStaggerMs = 0;     // C4 Chain Lightning
  double coneDeg = 0.0;           // C4 Multishot
  bool hasTeleport = false;       // C1/C8 Teleport limits
  double teleportMaxRangeTiles = 0.0;
  double teleportTouchJoystickTiles = 0.0;
  double teleportTouchDeadzone = 0.0;
  int32_t teleportWalkableSearchRings = 0;
};

struct SkillDef {
  // ---- identity ----
  std::string id;
  ClassId classId = ClassId::Warrior;
  int32_t indexInClass = 0;  // definition order = default hotbar order and auto-combat priority
  std::string name, nameEn, description;  // zh-CN fallbacks (i18n data.skill.<id>.name/.desc)
  std::string tree;
  int32_t tier = 1;
  int32_t maxLevel = 20;
  // ---- numbers (level 1) ----
  double manaCost = 0.0;
  double cooldownMs = 0.0;  // JSON "cooldown"
  double range = 0.0;
  double damageMultiplier = 0.0;
  DamageType damageType = DamageType::Physical;
  bool aoe = false;
  double aoeRadius = 0.0;
  bool hasBuff = false;
  SkillBuffDef buff;
  std::string icon;
  bool hasScaling = false;
  SkillScaling scaling;
  std::vector<SkillSynergy> synergies;
  double critBonus = 0.0;     // percentage points
  bool hasStunDuration = false;
  double stunDurationMs = 0.0;  // JSON "stunDuration" (not level-scaled, Q5)
  // ---- exporter-derived fields (C6) ----
  SkillExecKind execKind = SkillExecKind::Single;
  bool passive = false;
  SkillPassiveRule passiveRule;
  bool groundAnchored = false;
  bool hasProjectile = false;
  ProjectileTimingDef projectile;
  int32_t aoeDelayMs = 0;
  bool hasArrowDelay = false;
  ArrowDelayDef arrowDelay;
  bool hasLineTarget = false;
  double lineHalfWidthFactor = 0.0;
  SkillAnimKind animKind = SkillAnimKind::Cast;
  bool instantRelease = false;
  bool requiresTarget = false;
  bool rangeCheck = false;
  std::vector<StatusRule> statusRules;
  bool hasBonusVsStatus = false;
  StatusType bonusVsStatus = StatusType::Burn;
  double bonusVsStatusMul = 1.0;
  bool tauntAoe = false;
  uint32_t impactColor = 0;
  bool hasScorch = false;
  DamageType scorch = DamageType::Fire;  // ground scorch decal type (fixed by damageType)
  std::string vfxId;
  SkillPortDef port;
};

// skill_rules.json
struct SkillTierWeight {
  int32_t fromLevel = 2;
  bool hasToLevel = false;
  int32_t toLevel = 0;
  double weight = 1.0;
};

struct ABYSS_API SkillRules {
  // tier -> required hero level / invested tree points (index = tier); fallback formulas for other tiers.
  std::vector<int32_t> tierPlayerLevel;   // [0] unused
  std::vector<int32_t> tierTreePoints;    // [0] unused
  int32_t fallbackPlayerLevelBase = 1;
  int32_t fallbackPlayerLevelPerTierAbove1 = 6;
  int32_t fallbackTreePointsPerTierAbove1 = 5;
  int32_t fallbackTreePointsMin = 0;
  int32_t starterLevelTier1 = 1;
  int32_t starterLevelOther = 0;
  int32_t loadoutSize = 6;
  std::vector<std::string> groundAoeSkills;
  std::vector<SkillTierWeight> tierWeights;  // tieredScale brackets
  SkillScaling scalingDefaults;
  double cooldownFloorMs = 500.0;
  double cooldownReductionCapPercent = 50.0;
  double rangeSlackTiles = 1.0;
  double groundAnchorReachSlackTiles = 1.0;
  std::vector<int32_t> gamepadSkillButtons;
  std::vector<std::string> passiveSkills;

  int32_t RequiredPlayerLevel(int32_t tier) const;
  int32_t RequiredTreePoints(int32_t tier) const;
};

// status_effects.json
enum class StatusStacking : uint8_t { Stack, RefreshKeepStronger, Replace };
ABYSS_ENUM_STRINGS(StatusStacking, "stack", "refreshKeepStronger", "replace")

struct MonsterKeywordRule {
  std::vector<std::string> keywords;
  StatusRule rule;
};

struct ABYSS_API StatusEffectRules {
  std::array<int32_t, EnumCount<StatusType>()> tickIntervalMs{};
  std::array<StatusStacking, EnumCount<StatusType>()> stacking{};
  std::vector<StatusType> diminishingAppliesTo;  // freeze, stun
  double diminishingFactor = 0.5;
  double diminishingImmunityMs = 3000.0;
  double diminishingWindowMs = 6000.0;
  double slowMinSpeedFactor = 0.2;
  double poisonedHpRegenMultiplier = 0.5;
  std::vector<MonsterKeywordRule> monsterKeywordRules;  // documentation of how monsters.derived.onHitStatus was built
  // elite "frozen" affix: on hit, chance = affix freezeChance -> hero status (slow 30, 2500 ms)
  StatusType eliteFrozenStatus = StatusType::Slow;
  double eliteFrozenValue = 30.0;
  int32_t eliteFrozenDurationMs = 2500;

  int32_t TickInterval(StatusType t) const { return tickIntervalMs[static_cast<size_t>(t)]; }
  StatusStacking Stacking(StatusType t) const { return stacking[static_cast<size_t>(t)]; }
  bool Diminishes(StatusType t) const;
};

}  // namespace abyss

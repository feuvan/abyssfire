// Combat constants: input/dodge, projectile & animation timing, hit feedback, elite affixes, difficulty, soul echo.
// Sources: combat_input.json, projectile_timing.json, anim_timing.json, hit_feedback.json, elite_affixes.json,
// difficulty.json, soul_echo.json. Spec: combat-feel.md (all sections), monsters-ai.md 4.2.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/Platform.h"

namespace abyss {

// combat_input.json (combat-feel.md 6.1, 8, 9).
struct CombatInputDef {
  double inputBufferMs = 180;
  double dodgeCooldownMs = 900;
  double dodgeInvulnerabilityMs = 220;
  std::array<double, EnumCount<ClassId>()> dodgeDistanceTiles{};  // per class
  double dodgeStepTiles = 0.25;
  double dodgeMinTiles = 0.5;
  double dodgeDefaultDirX = 1, dodgeDefaultDirY = -1;
  int32_t gamepadDodgeButton = 1;
  int32_t gamepadTargetCycleButton = 4;
  std::vector<int32_t> gamepadSkillButtons;
  double targetCycleRangeTiles = 14;
  double monsterSwingScanRadiusTiles = 12;
  double monsterMeleeReachMul = 1.35, monsterMeleeReachAdd = 0.5;
  double monsterRangedThreshold = 2.5;
  double monsterProjectileMsPerPx = 2, monsterProjectileMinMs = 200, monsterProjectileMaxMs = 500;
  double combatStateOffDebounceMs = 1500;
  double monsterRespawnMs = 15000;
  double holdMoveRepathMs = 120;
};

// projectile_timing.json (combat-feel.md 6.4-6.5, 10).
struct ProjectileTimingTable {
  double isoTileWidthPx = 64, isoTileHeightPx = 32;
  double pxPerTileProjectile = 36, pxPerTileVfx = 45;  // must equal Units.h (validated)
  double chestHeightPx = 18;
  double monsterProjectileMsPerPx = 2, monsterProjectileMinMs = 200, monsterProjectileMaxMs = 500;
  // hero beats at speed 1 (validated against anim_timing.contact)
  std::array<double, EnumCount<ClassId>()> heroContactMsAtSpeed1{};
  std::array<double, EnumCount<ClassId>()> heroCastReleaseMs{};
};

// One CharacterAnimator preset (anim_timing.json presets.<rig>), every field verbatim.
struct AnimConfigDef {
  double idleBobAmount = 0, idleBobSpeed = 0, idleScalePulse = 0, idleSwayX = 0;
  double walkBobAmount = 0, walkBobSpeed = 0, walkTilt = 0, walkSquash = 0;
  double attackLunge = 0, attackDuration = 0, attackSquash = 0, attackWindup = 0;
  bool attackShake = false;
  double attackContact = 1;
  double castLean = 0, castDuration = 0;
  bool castGlow = false;
  double dodgeDuration = 0;
  double hurtKnockback = 0, hurtDuration = 0;
  bool hurtFlash = false;
  std::string deathStyle;
  double deathDuration = 0;
  double idleFrameRate = 0, walkFrameRate = 0, attackFrameRate = 0, castFrameRate = 0, hurtFrameRate = 0,
         dodgeFrameRate = 0, deathFrameRate = 0;
};

// Derived contact table per rig (anim_timing.json contact.<rig>, combat-feel.md 10.1).
struct RigContactDef {
  int32_t attackFrames = 0;
  double attackFps = 0;
  int32_t contactFrame = 0;
  double contactMsAtSpeed1 = 0;
  double frameContactMs = 0;
  bool hasCastReleaseMs = false;
  double castReleaseMs = 0;
  double fullSpeedBelowIntervalMs = 0;
};

struct AnimTransition {
  std::string from, to;
  double ms = 0;
};

struct ABYSS_API AnimTimingTable {
  std::array<AnimConfigDef, EnumCount<AnimRig>()> presets{};
  std::array<RigContactDef, EnumCount<AnimRig>()> contact{};
  AnimRig fallbackPreset = AnimRig::Humanoid;
  double castPhaseCharge = 0.46, castPhaseRelease = 0.2, castPhaseRecover = 0.34;
  double attackWindupOfContact = 0.62, attackStrikeOfContact = 0.38, attackRecoverMinMs = 70;
  double attackSpeedIntervalFactor = 0.9, attackSpeedScaleMin = 0.35, attackSpeedScaleMax = 1;
  std::vector<AnimTransition> transitions;
  double transitionDefaultMs = 80;

  const AnimConfigDef& Preset(AnimRig r) const { return presets[static_cast<size_t>(r)]; }
  const RigContactDef& Contact(AnimRig r) const { return contact[static_cast<size_t>(r)]; }
  double TransitionMs(std::string_view from, std::string_view to) const;
};

// hit_feedback.json (combat-feel.md 11).
struct HitProfileDef {
  double targetStopMs = 0, attackerStopMs = 0, flashMs = 0, recoil = 0, shakeMs = 0, shakeIntensity = 0;
  int32_t sparks = 0;
  double ringRadius = 0;
};

struct HitFeedbackTable {
  std::array<HitProfileDef, EnumCount<HitWeight>()> profiles{};
  double heavyRatio = 0.25, normalRatio = 0.06;
  double monsterHitAttackerStopFactor = 0.6;
  double eliteKillSlowMoDurationMs = 200, eliteKillSlowMoTimeScale = 0.4;
  std::array<uint32_t, EnumCount<ClassId>()> classImpactColors{};
  uint32_t fallbackImpactColor = 0;
  double aoeHitShakeDurationMs = 100, aoeHitShakeBase = 0.004, aoeHitShakePerHit = 0.001;
  double playerHitShakeCritMs = 150, playerHitShakeCritIntensity = 0.008;
  double playerHitShakePerRatio = 0.01, playerHitShakeMin = 0.002, playerHitShakeMax = 0.006;
  double playerHitShakeMsBase = 50, playerHitShakeMsPerRatio = 100, playerHitShakeMsMin = 50,
         playerHitShakeMsMax = 120;
  double shakeThrottleMs = 100;
  uint32_t painTintColor = 0;
  double painTintMs = 100;
  double lowHpBelowRatio = 0.3;
  double lowHpStrengthBase = 0.2, lowHpStrengthPerSeverity = 0.25, lowHpPulse = 0.05, lowHpPulseRate = 0.005;
  double lowHpRadiusBase = 0.7, lowHpRadiusPerSeverity = 0.15;

  const HitProfileDef& Profile(HitWeight w) const { return profiles[static_cast<size_t>(w)]; }
};

// elite_affixes.json (combat-feel.md 17).
enum class EliteAffixType : uint8_t { FireEnhanced, Swift, Teleporting, ExtraStrong, CurseAura, Vampiric, Frozen };
ABYSS_ENUM_STRINGS(EliteAffixType, "fire_enhanced", "swift", "teleporting", "extra_strong", "curse_aura", "vampiric",
                   "frozen")

struct EliteAffixDef {
  EliteAffixType type = EliteAffixType::FireEnhanced;
  std::string name, nameEn;
  double damageMult = 1, speedMult = 1, hpMult = 1, defenseMult = 1;
  double extraFireDamage = 0;
  double teleportCooldownMs = 0;
  double curseAuraRadius = 0, curseAuraReduction = 0;
  double lifestealFraction = 0;
  double freezeChance = 0;
  uint32_t vfxColor = 0;
  double lootQualityBonus = 0;
};

struct ZoneAffixCount {
  std::string zoneId;
  int32_t min = 1, max = 1;
};

struct ABYSS_API EliteAffixTable {
  std::vector<EliteAffixType> order;  // roll pool order
  std::array<EliteAffixDef, EnumCount<EliteAffixType>()> defs{};
  std::vector<ZoneAffixCount> zoneCounts;
  int32_t defaultCountMin = 1, defaultCountMax = 1;
  double freezeChanceCap = 0.5;
  double teleportWindowDistSqMin = 4, teleportWindowDistSqMax = 225;
  double teleportOffsetMin = 1, teleportOffsetMax = 2;
  double curseBuffDurationMs = 2000, curseLogIntervalMs = 2000;
  std::string curseTag = "curseAura";
  StatusType frozenOnHitStatus = StatusType::Slow;
  double frozenOnHitValue = 30, frozenOnHitDurationMs = 2500;

  const EliteAffixDef& Def(EliteAffixType t) const { return defs[static_cast<size_t>(t)]; }
  // [min, max] affix count for a zone (default when unknown).
  void CountFor(std::string_view zoneId, int32_t& outMin, int32_t& outMax) const;
};

// difficulty.json (combat-feel.md 14).
struct DifficultyDef {
  double hpMul = 1, damageMul = 1, defenseMul = 1, expMul = 1;
  int32_t lootLevelBonus = 0;
  double lootQualityBonus = 0;
  int32_t lootExtraAffixes = 0;
};

struct DifficultyTable {
  std::array<DifficultyDef, EnumCount<Difficulty>()> defs{};
  std::string unlockBossId = "demon_lord";
  std::string unlockZoneId = "abyss_rift";
  const DifficultyDef& Def(Difficulty d) const { return defs[static_cast<size_t>(d)]; }
};

// soul_echo.json (combat-feel.md 15).
struct SoulEchoDef {
  int32_t minLevel = 5;
  std::array<double, EnumCount<Difficulty>()> goldShare{};
  std::array<double, EnumCount<Difficulty>()> expShare{};
  double claimRangeTiles = 1.5;
};

struct CombatTables {
  CombatInputDef input;
  ProjectileTimingTable projectiles;
  AnimTimingTable anim;
  HitFeedbackTable hitFeedback;
  EliteAffixTable eliteAffixes;
  DifficultyTable difficulty;
  SoulEchoDef soulEcho;
};

}  // namespace abyss

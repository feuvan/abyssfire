// Hit classification, contact/release beat timing and camera-shake rules.
// Spec: combat-feel.md section 10 (contact-beat timing), section 11 (hit feedback: classifyHit, HIT_PROFILES,
// player-hit shake, AoE shake, throttles, slow motion); ARCHITECTURE section 3.1 / 5 (contact ms come from the asset
// manifest, anim_timing.json is the fallback); DECISIONS P5 (core owns timing), C9 (shake amplitude), S6 (slow-mo).
//
// Owner area: hero+combat (monsters and pets call the same functions for their swings). Pure functions.
#pragma once

#include <cstdint>
#include <string_view>

#include "abyss/base/Enums.h"
#include "abyss/base/Platform.h"
#include "abyss/data/AssetManifest.h"
#include "abyss/data/CombatData.h"
#include "abyss/data/SkillData.h"

namespace abyss {

// classifyHit (11.0): tick -> Tick; killed -> Kill; crit -> Crit; ratio = maxHp > 0 ? dmg / maxHp : 0;
// ratio >= heavyRatio -> Heavy; >= normalRatio -> Normal; else Light.
ABYSS_API HitWeight ClassifyHit(const HitFeedbackTable& t, double damage, double targetMaxHp, bool isCrit, bool killed,
                                bool isTick);

// attackSpeedScale (10.1): (interval > 0 && animMs > 0) ? clamp(interval * 0.9 / animMs, 0.35, 1) : 1.
ABYSS_API double AttackSpeedScale(const AnimTimingTable& t, double animMs, double attackIntervalMs);

// Timing of one attack or cast, relative to its start (sim ms).
struct ActionTiming {
  double contactMs = 0;   // attack: round(frameContact * speed); cast: release beat (not speed-scaled)
  double windupMs = 0;    // attack: 0.62 * contact (monster telegraph); cast: 0
  double durationMs = 0;  // attackDuration * speed / castDuration
  double speed = 1;       // AttackSpeedScale (1 for casts)
  double playRate = 1;    // 1 / speed: montage play rate that puts the authored Contact notify at contactMs
};

// Attack contact (10.1). Contact at speed 1 comes from the manifest clip `clip` of the asset mapped to `artId`
// (AnimClipDef::contactMs) when present, else from anim_timing contact[rig].frameContactMs. `attackDuration` is
// presets[rig].attackDuration. The hero passes its derived attackSpeed, monsters def.attackSpeedMs.
ABYSS_API ActionTiming ComputeAttackTiming(const AnimTimingTable& t, const AssetManifest& manifest, std::string_view artId,
                                           AnimRig rig, double attackIntervalMs, std::string_view clip = "Attack01");

// Cast release (10.2): manifest releaseMs when present, else contact[rig].castReleaseMs, else
// round(castDuration * castPhaseCharge). Never speed-scaled.
ABYSS_API ActionTiming ComputeCastTiming(const AnimTimingTable& t, const AssetManifest& manifest, std::string_view artId,
                                         AnimRig rig, std::string_view clip = "Cast01");

// Hero rig for a class (Warrior/Mage/Rogue presets).
ABYSS_API AnimRig HeroRig(ClassId cls);

struct ShakeRequest {
  double durationMs = 0;
  double intensity = 0;  // web units (C9)
  bool Empty() const { return durationMs <= 0 || intensity <= 0; }
};

// Player-hit shake (11.5): crit -> (150, 0.008); else ratio = dmg / maxHp,
// (clamp(50 + ratio * 100, 50, 120), clamp(ratio * 0.01, 0.002, 0.006)).
ABYSS_API ShakeRequest HeroHitShake(const HitFeedbackTable& t, double damage, double heroMaxHp, bool isCrit);
// AoE batch shake (6.5): (100, 0.004 + hits * 0.001) when hits >= 1.
ABYSS_API ShakeRequest AoeHitShake(const HitFeedbackTable& t, int32_t hits);
// Profile shake of a hit weight (via the impact burst; none for Tick / Light).
ABYSS_API ShakeRequest ProfileShake(const HitFeedbackTable& t, HitWeight w);

// Shake throttle (11.5): a shake within shakeThrottleMs of the last accepted one is dropped; a new shake while one
// is running is dropped (no override). Runs on the sim clock in the core so tests are deterministic.
class ABYSS_API ShakeThrottle {
 public:
  bool Accept(const ShakeRequest& s, double nowMs, double throttleMs);
  void Reset() { lastAcceptedMs_ = -1e300; runningUntilMs_ = -1e300; }

 private:
  double lastAcceptedMs_ = -1e300;
  double runningUntilMs_ = -1e300;
};

// Impact colours (11.1): class basic-impact colour; skills carry SkillDef::impactColor (C6).
ABYSS_API uint32_t ClassImpactColor(const HitFeedbackTable& t, ClassId cls);

// Monster attacker hit-stop on a landed hit: round(attackerStopMs * 0.6) (5.3).
ABYSS_API double MonsterAttackerStopMs(const HitFeedbackTable& t, HitWeight w);

}  // namespace abyss

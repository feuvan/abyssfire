// Hit classification, contact timing and shake rules (combat-feel.md sections 10-11; P5, C9, S6).
#include "abyss/base/Platform.h"

#include "abyss/combat/HitFeedback.h"

#include <cmath>

#include "abyss/base/Math.h"

namespace abyss {

HitWeight ClassifyHit(const HitFeedbackTable& t, double damage, double targetMaxHp, bool isCrit, bool killed,
                      bool isTick) {
  if (isTick) return HitWeight::Tick;
  if (killed) return HitWeight::Kill;
  if (isCrit) return HitWeight::Crit;
  const double ratio = targetMaxHp > 0 ? damage / targetMaxHp : 0.0;
  if (ratio >= t.heavyRatio) return HitWeight::Heavy;
  if (ratio >= t.normalRatio) return HitWeight::Normal;
  return HitWeight::Light;
}

double AttackSpeedScale(const AnimTimingTable& t, double animMs, double attackIntervalMs) {
  if (!(attackIntervalMs > 0) || !(animMs > 0)) return 1.0;
  return Clamp(attackIntervalMs * t.attackSpeedIntervalFactor / animMs, t.attackSpeedScaleMin, t.attackSpeedScaleMax);
}

namespace {
const AnimClipDef* HitFeedbackFindClip(const AssetManifest& manifest, std::string_view artId, std::string_view clip) {
  if (!manifest.loaded || artId.empty() || clip.empty()) return nullptr;
  const AssetEntryDef* asset = manifest.FindByGameId(artId);
  return asset != nullptr ? asset->FindClip(clip) : nullptr;
}
}  // namespace

ActionTiming ComputeAttackTiming(const AnimTimingTable& t, const AssetManifest& manifest, std::string_view artId,
                                 AnimRig rig, double attackIntervalMs, std::string_view clip) {
  const AnimConfigDef& cfg = t.Preset(rig);
  const AnimClipDef* c = HitFeedbackFindClip(manifest, artId, clip);
  // Contact at play rate 1: the authored Contact notify when the manifest has it, else the web's frame rule
  // round((frames - 1) * attackContact) * 1000 / fps (anim_timing contact table, unrounded). The manifest stores the
  // notify in whole ms: a notify on the web's beat (within its rounding, e.g. 308 for 307.69) is that beat, so the
  // exact frame value is scaled (10.1: contactMs = round(frameContactMs x speed); 0.7 -> 215, not round(308 x 0.7)).
  const double frameContact = t.Contact(rig).frameContactMs;
  double contact1 = frameContact;
  if (c != nullptr && c->hasContactMs && !(std::fabs(c->contactMs - frameContact) <= 0.5)) contact1 = c->contactMs;
  ActionTiming a;
  a.speed = AttackSpeedScale(t, cfg.attackDuration, attackIntervalMs);
  a.contactMs = JsRound(contact1 * a.speed);
  a.windupMs = a.contactMs * t.attackWindupOfContact;
  a.durationMs = cfg.attackDuration * a.speed;
  a.playRate = a.speed > 0 ? 1.0 / a.speed : 1.0;
  return a;
}

ActionTiming ComputeCastTiming(const AnimTimingTable& t, const AssetManifest& manifest, std::string_view artId,
                               AnimRig rig, std::string_view clip) {
  const AnimConfigDef& cfg = t.Preset(rig);
  const RigContactDef& rc = t.Contact(rig);
  const AnimClipDef* c = HitFeedbackFindClip(manifest, artId, clip);
  ActionTiming a;
  if (c != nullptr && c->hasReleaseMs) {
    a.contactMs = c->releaseMs;
  } else if (rc.hasCastReleaseMs) {
    a.contactMs = rc.castReleaseMs;
  } else {
    a.contactMs = JsRound(cfg.castDuration * t.castPhaseCharge);
  }
  a.windupMs = 0;
  a.durationMs = cfg.castDuration;
  a.speed = 1;
  a.playRate = 1;
  return a;
}

AnimRig HeroRig(ClassId cls) {
  switch (cls) {
    case ClassId::Warrior:
      return AnimRig::Warrior;
    case ClassId::Mage:
      return AnimRig::Mage;
    case ClassId::Rogue:
      return AnimRig::Rogue;
  }
  return AnimRig::Warrior;
}

ShakeRequest HeroHitShake(const HitFeedbackTable& t, double damage, double heroMaxHp, bool isCrit) {
  if (isCrit) return {t.playerHitShakeCritMs, t.playerHitShakeCritIntensity};
  const double ratio = heroMaxHp > 0 ? damage / heroMaxHp : 0.0;
  ShakeRequest s;
  s.durationMs =
      Clamp(t.playerHitShakeMsBase + ratio * t.playerHitShakeMsPerRatio, t.playerHitShakeMsMin, t.playerHitShakeMsMax);
  s.intensity = Clamp(ratio * t.playerHitShakePerRatio, t.playerHitShakeMin, t.playerHitShakeMax);
  return s;
}

ShakeRequest AoeHitShake(const HitFeedbackTable& t, int32_t hits) {
  if (hits < 1) return {};
  return {t.aoeHitShakeDurationMs, t.aoeHitShakeBase + hits * t.aoeHitShakePerHit};
}

ShakeRequest ProfileShake(const HitFeedbackTable& t, HitWeight w) {
  const HitProfileDef& p = t.Profile(w);
  return {p.shakeMs, p.shakeIntensity};
}

// VFXManager throttle (a shake < throttleMs after the last accepted one is ignored) + Phaser's no-override rule (a new
// shake while one is still running is ignored).
bool ShakeThrottle::Accept(const ShakeRequest& s, double nowMs, double throttleMs) {
  if (s.Empty()) return false;
  if (nowMs - lastAcceptedMs_ < throttleMs) return false;
  if (nowMs < runningUntilMs_) return false;
  lastAcceptedMs_ = nowMs;
  runningUntilMs_ = nowMs + s.durationMs;
  return true;
}

uint32_t ClassImpactColor(const HitFeedbackTable& t, ClassId cls) {
  return t.classImpactColors[static_cast<size_t>(cls)];
}

double MonsterAttackerStopMs(const HitFeedbackTable& t, HitWeight w) {
  return JsRound(t.Profile(w).attackerStopMs * t.monsterHitAttackerStopFactor);
}

}  // namespace abyss

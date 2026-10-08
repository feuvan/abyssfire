// Hit classification, contact timing and shake rules (combat-feel.md sections 10-11). STUB: owner area hero+combat.
#include "abyss/base/Platform.h"

#include "abyss/combat/HitFeedback.h"

#include "abyss/base/Assert.h"

namespace abyss {

HitWeight ClassifyHit(const HitFeedbackTable& t, double damage, double targetMaxHp, bool isCrit, bool killed,
                      bool isTick) {
  ABYSS_UNIMPLEMENTED();
  return HitWeight::Normal;
}

double AttackSpeedScale(const AnimTimingTable& t, double animMs, double attackIntervalMs) {
  ABYSS_UNIMPLEMENTED();
  return 1.0;
}

ActionTiming ComputeAttackTiming(const AnimTimingTable& t, const AssetManifest& manifest, std::string_view artId,
                                 AnimRig rig, double attackIntervalMs, std::string_view clip) {
  ABYSS_UNIMPLEMENTED();
  ActionTiming a;
  a.contactMs = t.Contact(rig).frameContactMs;
  return a;
}

ActionTiming ComputeCastTiming(const AnimTimingTable& t, const AssetManifest& manifest, std::string_view artId,
                               AnimRig rig, std::string_view clip) {
  ABYSS_UNIMPLEMENTED();
  ActionTiming a;
  a.contactMs = t.Contact(rig).castReleaseMs;
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
  ABYSS_UNIMPLEMENTED();
  return {};
}

ShakeRequest AoeHitShake(const HitFeedbackTable& t, int32_t hits) {
  ABYSS_UNIMPLEMENTED();
  return {};
}

ShakeRequest ProfileShake(const HitFeedbackTable& t, HitWeight w) {
  const HitProfileDef& p = t.Profile(w);
  return {p.shakeMs, p.shakeIntensity};
}

bool ShakeThrottle::Accept(const ShakeRequest& s, double nowMs, double throttleMs) {
  ABYSS_UNIMPLEMENTED();
  (void)lastAcceptedMs_;
  (void)runningUntilMs_;
  return true;
}

uint32_t ClassImpactColor(const HitFeedbackTable& t, ClassId cls) {
  return t.classImpactColors[static_cast<size_t>(cls)];
}

double MonsterAttackerStopMs(const HitFeedbackTable& t, HitWeight w) {
  ABYSS_UNIMPLEMENTED();
  return 0;
}

}  // namespace abyss

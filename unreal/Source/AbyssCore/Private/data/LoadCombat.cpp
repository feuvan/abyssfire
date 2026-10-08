#include "abyss/base/Platform.h"

#include "JsonReader.h"

namespace abyss::dataload {
namespace {

// Fills a per-enum array from {enumName: value}. Every enum value must be present: a missing key would silently keep a
// zero entry (dodge distance 0, an all-zero hit profile), so it is reported as an error with the JSON path.
template <class E, class T, size_t N>
void ReadKeyed(const JNode& obj, std::array<T, N>& out, T (*read)(const JNode&)) {
  std::array<bool, N> seen{};
  for (const auto& [k, n] : obj.SelfMembers()) {
    E e{};
    if (!ParseEnum(k, e)) {
      n.Error("unknown key '" + k + "'");
      continue;
    }
    out[EnumIndex(e)] = read(n);
    seen[EnumIndex(e)] = true;
  }
  if (!obj.Exists()) return;  // SelfMembers already reported the missing table
  for (size_t i = 0; i < N; ++i) {
    if (!seen[i]) obj.Error("missing key '" + std::string(EnumName(static_cast<E>(i))) + "'");
  }
}

// A [a, b] number pair; a wrong length is an error (never a silent default).
bool ReadNumPair(const JNode& n, std::string_view key, double& a, double& b) {
  const std::vector<double> v = n.NumList(key);
  if (v.size() != 2) {
    n.Child(key).Error("expected [a, b]");
    return false;
  }
  a = v[0];
  b = v[1];
  return true;
}
bool ReadIntPair(const JNode& n, std::string_view key, int32_t& a, int32_t& b) {
  const std::vector<int32_t> v = n.IntList(key);
  if (v.size() != 2) {
    n.Child(key).Error("expected [a, b]");
    return false;
  }
  a = v[0];
  b = v[1];
  return true;
}

double ReadNum(const JNode& n) { return n.AsNum(); }
uint32_t ReadColor(const JNode& n) { return n.AsColor(); }

AnimConfigDef ReadAnimConfig(const JNode& n) {
  AnimConfigDef a;
  a.idleBobAmount = n.Num("idleBobAmount");
  a.idleBobSpeed = n.Num("idleBobSpeed");
  a.idleScalePulse = n.Num("idleScalePulse");
  a.idleSwayX = n.Num("idleSwayX");
  a.walkBobAmount = n.Num("walkBobAmount");
  a.walkBobSpeed = n.Num("walkBobSpeed");
  a.walkTilt = n.Num("walkTilt");
  a.walkSquash = n.Num("walkSquash");
  a.attackLunge = n.Num("attackLunge");
  a.attackDuration = n.Num("attackDuration");
  a.attackSquash = n.Num("attackSquash");
  a.attackWindup = n.Num("attackWindup");
  a.attackShake = n.Bool("attackShake");
  a.attackContact = n.Num("attackContact");
  a.castLean = n.Num("castLean");
  a.castDuration = n.Num("castDuration");
  a.castGlow = n.Bool("castGlow");
  a.dodgeDuration = n.Num("dodgeDuration");
  a.hurtKnockback = n.Num("hurtKnockback");
  a.hurtDuration = n.Num("hurtDuration");
  a.hurtFlash = n.Bool("hurtFlash");
  a.deathStyle = n.Str("deathStyle");
  a.deathDuration = n.Num("deathDuration");
  a.idleFrameRate = n.Num("idleFrameRate");
  a.walkFrameRate = n.Num("walkFrameRate");
  a.attackFrameRate = n.Num("attackFrameRate");
  a.castFrameRate = n.Num("castFrameRate");
  a.hurtFrameRate = n.Num("hurtFrameRate");
  a.dodgeFrameRate = n.Num("dodgeFrameRate");
  a.deathFrameRate = n.Num("deathFrameRate");
  return a;
}

RigContactDef ReadRigContact(const JNode& n) {
  RigContactDef c;
  c.attackFrames = n.Int("attackFrames");
  c.attackFps = n.Num("attackFps");
  c.contactFrame = n.Int("contactFrame");
  c.contactMsAtSpeed1 = n.Num("contactMsAtSpeed1");
  c.frameContactMs = n.Num("frameContactMs");
  if (n.Has("castReleaseMs")) {
    c.hasCastReleaseMs = true;
    c.castReleaseMs = n.Num("castReleaseMs");
  }
  c.fullSpeedBelowIntervalMs = n.Num("fullSpeedBelowIntervalMs");
  return c;
}

HitProfileDef ReadHitProfile(const JNode& n) {
  HitProfileDef p;
  p.targetStopMs = n.Num("targetStopMs");
  p.attackerStopMs = n.Num("attackerStopMs");
  p.flashMs = n.Num("flashMs");
  p.recoil = n.Num("recoil");
  p.shakeMs = n.Num("shakeMs");
  p.shakeIntensity = n.Num("shakeIntensity");
  p.sparks = n.Int("sparks");
  p.ringRadius = n.Num("ringRadius");
  return p;
}

}  // namespace

void LoadCombatInputFile(const JNode& r, CombatTables& out) {
  CombatInputDef& c = out.input;
  c.inputBufferMs = r.Num("inputBufferMs");
  const JNode d = r.Child("dodge");
  c.dodgeCooldownMs = d.Num("cooldownMs");
  c.dodgeInvulnerabilityMs = d.Num("invulnerabilityMs");
  ReadKeyed<ClassId>(d.Child("distanceTiles"), c.dodgeDistanceTiles, &ReadNum);
  c.dodgeStepTiles = d.Num("stepTiles");
  c.dodgeMinTiles = d.Num("minTiles");
  ReadNumPair(d, "defaultDirection", c.dodgeDefaultDirX, c.dodgeDefaultDirY);
  const JNode g = r.Child("gamepad");
  c.gamepadDodgeButton = g.Int("dodgeButton");
  c.gamepadTargetCycleButton = g.Int("targetCycleButton");
  c.gamepadSkillButtons = g.IntList("skillButtons");
  c.targetCycleRangeTiles = r.Num("targetCycleRangeTiles");
  c.monsterSwingScanRadiusTiles = r.Num("monsterSwingScanRadiusTiles");
  c.monsterMeleeReachMul = r.Child("monsterMeleeReach").Num("mul");
  c.monsterMeleeReachAdd = r.Child("monsterMeleeReach").Num("add");
  c.monsterRangedThreshold = r.Num("monsterRangedThreshold");
  c.monsterProjectileMsPerPx = r.Child("monsterProjectileMs").Num("msPerPx");
  c.monsterProjectileMinMs = r.Child("monsterProjectileMs").Num("minMs");
  c.monsterProjectileMaxMs = r.Child("monsterProjectileMs").Num("maxMs");
  c.combatStateOffDebounceMs = r.Num("combatStateOffDebounceMs");
  c.monsterRespawnMs = r.Num("monsterRespawnMs");
  c.holdMoveRepathMs = r.Num("holdMoveRepathMs");
}

void LoadProjectileTimingFile(const JNode& r, CombatTables& out) {
  ProjectileTimingTable& p = out.projectiles;
  p.isoTileWidthPx = r.Child("isoPx").Num("tileWidth");
  p.isoTileHeightPx = r.Child("isoPx").Num("tileHeight");
  p.pxPerTileProjectile = r.Child("portPxPerTile").Num("projectileAndArrow");
  p.pxPerTileVfx = r.Child("portPxPerTile").Num("vfxSize");
  p.chestHeightPx = r.Num("chestHeightPx");
  p.monsterProjectileMsPerPx = r.Child("monsterProjectile").Num("msPerPx");
  p.monsterProjectileMinMs = r.Child("monsterProjectile").Num("minMs");
  p.monsterProjectileMaxMs = r.Child("monsterProjectile").Num("maxMs");
  for (const auto& [k, n] : r.Members("heroBeats")) {
    ClassId c{};
    if (!ParseEnum(k, c)) {
      n.Error("unknown class");
      continue;
    }
    p.heroContactMsAtSpeed1[EnumIndex(c)] = n.Num("contactMsAtSpeed1");
    p.heroCastReleaseMs[EnumIndex(c)] = n.Num("castReleaseMs");
  }
  // skillProjectiles / skillArrowDelay / skillAoeDelayMs duplicate classes.json derived fields (cross-checked).
}

void LoadAnimTimingFile(const JNode& r, CombatTables& out) {
  AnimTimingTable& a = out.anim;
  a.transitions.clear();
  ReadKeyed<AnimRig>(r.Child("presets"), a.presets, &ReadAnimConfig);
  ReadKeyed<AnimRig>(r.Child("contact"), a.contact, &ReadRigContact);
  a.fallbackPreset = r.Enum<AnimRig>("fallbackPreset");
  a.castPhaseCharge = r.Child("castPhases").Num("charge");
  a.castPhaseRelease = r.Child("castPhases").Num("release");
  a.castPhaseRecover = r.Child("castPhases").Num("recover");
  a.attackWindupOfContact = r.Child("attackPhases").Num("windupOfContact");
  a.attackStrikeOfContact = r.Child("attackPhases").Num("strikeOfContact");
  a.attackRecoverMinMs = r.Child("attackPhases").Num("recoverMinMs");
  a.attackSpeedIntervalFactor = r.Child("attackSpeedScale").Num("intervalFactor");
  a.attackSpeedScaleMin = r.Child("attackSpeedScale").Num("min");
  a.attackSpeedScaleMax = r.Child("attackSpeedScale").Num("max");
  for (const auto& [k, n] : r.Members("transitionsMs")) {
    const size_t arrow = k.find("->");
    if (arrow == std::string::npos) {
      n.Error("transition key must be 'from->to'");
      continue;
    }
    a.transitions.push_back(AnimTransition{k.substr(0, arrow), k.substr(arrow + 2), n.AsNum()});
  }
  a.transitionDefaultMs = r.Num("transitionDefaultMs");
  // heroSheet / monsterSheet / legacyImpactDelay are 2D-sheet facts kept for reference only.
}

void LoadHitFeedbackFile(const JNode& r, CombatTables& out) {
  HitFeedbackTable& h = out.hitFeedback;
  ReadKeyed<HitWeight>(r.Child("profiles"), h.profiles, &ReadHitProfile);
  h.heavyRatio = r.Child("classify").Num("heavyRatio");
  h.normalRatio = r.Child("classify").Num("normalRatio");
  h.monsterHitAttackerStopFactor = r.Num("monsterHitAttackerStopFactor");
  h.eliteKillSlowMoDurationMs = r.Child("eliteKillSlowMotion").Num("durationMs");
  h.eliteKillSlowMoTimeScale = r.Child("eliteKillSlowMotion").Num("timeScale");
  ReadKeyed<ClassId>(r.Child("classImpactColors"), h.classImpactColors, &ReadColor);
  h.fallbackImpactColor = r.Color("fallbackImpactColor");
  const JNode aoe = r.Child("aoeHitShake");
  h.aoeHitShakeDurationMs = aoe.Num("durationMs");
  h.aoeHitShakeBase = aoe.Num("base");
  h.aoeHitShakePerHit = aoe.Num("perHit");
  const JNode ph = r.Child("playerHitShake");
  h.playerHitShakeCritMs = ph.Child("crit").Num("durationMs");
  h.playerHitShakeCritIntensity = ph.Child("crit").Num("intensity");
  const JNode ni = ph.Child("normal").Child("intensity");
  h.playerHitShakePerRatio = ni.Num("perRatio");
  h.playerHitShakeMin = ni.Num("min");
  h.playerHitShakeMax = ni.Num("max");
  const JNode nd = ph.Child("normal").Child("durationMs");
  h.playerHitShakeMsBase = nd.Num("base");
  h.playerHitShakeMsPerRatio = nd.Num("perRatio");
  h.playerHitShakeMsMin = nd.Num("min");
  h.playerHitShakeMsMax = nd.Num("max");
  h.shakeThrottleMs = r.Num("shakeThrottleMs");
  h.painTintColor = r.Child("painTint").Color("color");
  h.painTintMs = r.Child("painTint").Num("durationMs");
  const JNode v = r.Child("lowHpVignette");
  h.lowHpBelowRatio = v.Num("belowHpRatio");
  h.lowHpStrengthBase = v.Child("strength").Num("base");
  h.lowHpStrengthPerSeverity = v.Child("strength").Num("perSeverity");
  h.lowHpPulse = v.Child("strength").Num("pulse");
  h.lowHpPulseRate = v.Child("strength").Num("pulseRate");
  h.lowHpRadiusBase = v.Child("radius").Num("base");
  h.lowHpRadiusPerSeverity = v.Child("radius").Num("perSeverity");
}

void LoadEliteAffixesFile(const JNode& r, CombatTables& out) {
  EliteAffixTable& e = out.eliteAffixes;
  e.order = r.EnumList<EliteAffixType>("order");
  for (const auto& [k, n] : r.Members("definitions")) {
    EliteAffixType t{};
    if (!ParseEnum(k, t)) {
      n.Error("unknown elite affix");
      continue;
    }
    EliteAffixDef& d = e.defs[EnumIndex(t)];
    d.type = n.Enum<EliteAffixType>("type");
    d.name = n.Str("name");
    d.nameEn = n.Str("nameEn", "");
    d.damageMult = n.Num("damageMult");
    d.speedMult = n.Num("speedMult");
    d.hpMult = n.Num("hpMult");
    d.defenseMult = n.Num("defenseMult");
    d.extraFireDamage = n.Num("extraFireDamage");
    d.teleportCooldownMs = n.Num("teleportCooldownMs");
    d.curseAuraRadius = n.Num("curseAuraRadius");
    d.curseAuraReduction = n.Num("curseAuraReduction");
    d.lifestealFraction = n.Num("lifestealFraction");
    d.freezeChance = n.Num("freezeChance");
    d.vfxColor = n.Color("vfxColor");
    d.lootQualityBonus = n.Num("lootQualityBonus");
  }
  e.zoneCounts.clear();
  for (const auto& [k, n] : r.Members("zoneAffixCounts")) {
    const std::vector<int32_t> mm = r.Child("zoneAffixCounts").IntList(k);
    if (mm.size() != 2) {
      n.Error("expected [min, max]");
      continue;
    }
    e.zoneCounts.push_back(ZoneAffixCount{k, mm[0], mm[1]});
  }
  ReadIntPair(r, "defaultAffixCount", e.defaultCountMin, e.defaultCountMax);
  e.freezeChanceCap = r.Child("stacking").Num("freezeChanceCap");
  const JNode rt = r.Child("runtime");
  ReadNumPair(rt, "teleportWindowDistSq", e.teleportWindowDistSqMin, e.teleportWindowDistSqMax);
  ReadNumPair(rt, "teleportOffset", e.teleportOffsetMin, e.teleportOffsetMax);
  e.curseBuffDurationMs = rt.Child("curseAura").Num("buffDurationMs");
  e.curseLogIntervalMs = rt.Child("curseAura").Num("logIntervalMs");
  e.curseTag = rt.Child("curseAura").Str("tag");
  e.frozenOnHitStatus = rt.Child("frozenOnHit").Enum<StatusType>("status");
  e.frozenOnHitValue = rt.Child("frozenOnHit").Num("value");
  e.frozenOnHitDurationMs = rt.Child("frozenOnHit").Num("durationMs");
}

void LoadDifficultyFile(const JNode& r, CombatTables& out) {
  DifficultyTable& d = out.difficulty;
  for (const auto& [k, n] : r.Members("monsterMultipliers")) {
    Difficulty df{};
    if (!ParseEnum(k, df)) {
      n.Error("unknown difficulty");
      continue;
    }
    DifficultyDef& x = d.defs[EnumIndex(df)];
    x.hpMul = n.Num("hp");
    x.damageMul = n.Num("damage");
    x.defenseMul = n.Num("defense");
    x.expMul = n.Num("exp");
  }
  for (const auto& [k, n] : r.Members("lootMods")) {
    Difficulty df{};
    if (!ParseEnum(k, df)) {
      n.Error("unknown difficulty");
      continue;
    }
    DifficultyDef& x = d.defs[EnumIndex(df)];
    x.lootLevelBonus = n.Int("levelBonus");
    x.lootQualityBonus = n.Num("qualityBonus");
    x.lootExtraAffixes = n.Int("extraAffixes");
  }
  d.unlockBossId = r.Child("unlock").Str("bossId");
  d.unlockZoneId = r.Child("unlock").Str("zoneId");
}

void LoadSoulEchoFile(const JNode& r, CombatTables& out) {
  SoulEchoDef& s = out.soulEcho;
  s.minLevel = r.Int("minLevel");
  ReadKeyed<Difficulty>(r.Child("goldShare"), s.goldShare, &ReadNum);
  ReadKeyed<Difficulty>(r.Child("expShare"), s.expShare, &ReadNum);
  s.claimRangeTiles = r.Num("claimRangeTiles");
}

}  // namespace abyss::dataload

#include "abyss/base/Platform.h"

#include "JsonReader.h"

#include "abyss/base/StrUtil.h"

namespace abyss::dataload {
namespace {

size_t TierKey(const std::string& k) {
  int64_t v = 0;
  return ParseInt(k, v) && v > 0 && v < 100 ? static_cast<size_t>(v) : 0;
}

PrimaryStats ReadPrimaryStats(const JNode& n) {
  PrimaryStats s;
  s.str = n.Int("str");
  s.dex = n.Int("dex");
  s.vit = n.Int("vit");
  s.int_ = n.Int("int");
  s.spi = n.Int("spi");
  s.lck = n.Int("lck");
  return s;
}

ProjectileTimingDef ReadProjectileTiming(const JNode& n) {
  ProjectileTimingDef p;
  p.minMs = n.Num("minMs");
  p.maxMs = n.Num("maxMs");
  p.msPerPx = n.Num("msPerPx");
  return p;
}

SkillDef ReadSkill(const JNode& n, ClassId cls, int32_t index) {
  SkillDef s;
  s.classId = cls;
  s.indexInClass = index;
  s.id = n.Str("id");
  s.name = n.Str("name");
  s.nameEn = n.Str("nameEn", "");
  s.description = n.Str("description", "");
  s.tree = n.Str("tree");
  s.tier = n.Int("tier");
  s.maxLevel = n.Int("maxLevel");
  s.manaCost = n.Num("manaCost");
  s.cooldownMs = n.Num("cooldown");
  s.range = n.Num("range");
  s.damageMultiplier = n.Num("damageMultiplier");
  s.damageType = n.Enum<DamageType>("damageType");
  s.aoe = n.Bool("aoe", false);
  s.aoeRadius = n.Num("aoeRadius", 0.0);
  if (n.Has("buff")) {
    const JNode b = n.Child("buff");
    s.hasBuff = true;
    s.buff.stat = b.Enum<BuffStat>("stat");
    s.buff.value = b.Num("value");
    s.buff.durationMs = b.Num("duration");
  }
  s.icon = n.Str("icon", "");
  if (n.Has("scaling")) {
    // Absent per-field values are filled from skill_rules defaults in Finalize; `present` records which ones the JSON
    // states (explicit presence bits, no NaN sentinels: Platform.h).
    const JNode sc = n.Child("scaling");
    s.hasScaling = true;
    const auto read = [&](std::string_view key, ScalingField f, double& out) {
      if (!sc.Has(key)) return;
      out = sc.Num(key);
      s.scaling.present = static_cast<uint8_t>(s.scaling.present | ScalingBit(f));
    };
    read("damagePerLevel", ScalingField::DamagePerLevel, s.scaling.damagePerLevel);
    read("manaCostPerLevel", ScalingField::ManaCostPerLevel, s.scaling.manaCostPerLevel);
    read("cooldownReductionPerLevel", ScalingField::CooldownReductionPerLevel, s.scaling.cooldownReductionPerLevel);
    read("aoeRadiusPerLevel", ScalingField::AoeRadiusPerLevel, s.scaling.aoeRadiusPerLevel);
    read("buffValuePerLevel", ScalingField::BuffValuePerLevel, s.scaling.buffValuePerLevel);
    read("buffDurationPerLevel", ScalingField::BuffDurationPerLevel, s.scaling.buffDurationPerLevel);
  }
  for (const JNode& syn : n.Items("synergies", true)) {
    s.synergies.push_back(SkillSynergy{syn.Str("skillId"), syn.Num("damagePerLevel")});
  }
  s.critBonus = n.Num("critBonus", 0.0);
  if (n.Has("stunDuration")) {
    s.hasStunDuration = true;
    s.stunDurationMs = n.Num("stunDuration");
  }

  const JNode d = n.Child("derived");
  if (!d.Exists()) {
    n.Error("missing exporter 'derived' block (DECISIONS C6)");
    return s;
  }
  s.execKind = d.Enum<SkillExecKind>("execKind");
  s.passive = d.Bool("passive");
  if (d.Has("passiveRule")) {
    const JNode pr = d.Child("passiveRule");
    s.passiveRule.kind = pr.Enum<PassiveRuleKind>("kind");
    s.passiveRule.hpPerSecondPerLevel = pr.Num("hpPerSecondPerLevel", 0.0);
    s.passiveRule.hpRatioBelow = pr.Num("hpRatioBelow", 0.0);
    s.passiveRule.damageBonusPerLevel = pr.Num("damageBonusPerLevel", 0.0);
    s.passiveRule.buffDurationMs = pr.Int("buffDurationMs", 0);
    s.passiveRule.tag = pr.Str("tag", "");
  }
  s.groundAnchored = d.Bool("groundAnchored");
  if (d.Has("projectile")) {
    s.hasProjectile = true;
    s.projectile = ReadProjectileTiming(d.Child("projectile"));
  }
  s.aoeDelayMs = d.Int("aoeDelayMs", 0);
  if (d.Has("arrowDelay")) {
    const JNode ad = d.Child("arrowDelay");
    s.hasArrowDelay = true;
    s.arrowDelay.maxMs = ad.Num("maxMs");
    s.arrowDelay.msPerPx = ad.Num("msPerPx");
  }
  if (d.Has("lineTarget")) {
    s.hasLineTarget = true;
    s.lineHalfWidthFactor = d.Child("lineTarget").Num("halfWidthFactor");
  }
  s.animKind = d.Enum<SkillAnimKind>("animKind");
  s.instantRelease = d.Bool("instantRelease");
  s.requiresTarget = d.Bool("requiresTarget");
  s.rangeCheck = d.Bool("rangeCheck");
  for (const JNode& r : d.Items("statusRule", true)) s.statusRules.push_back(ReadStatusRule(r));
  if (d.Has("bonusVsStatus")) {
    const JNode b = d.Child("bonusVsStatus");
    s.hasBonusVsStatus = true;
    s.bonusVsStatus = b.Enum<StatusType>("status");
    s.bonusVsStatusMul = b.Num("mul");
  }
  s.tauntAoe = d.Bool("tauntAoe");
  s.impactColor = d.Color("impactColor");
  if (d.Has("scorch")) {
    s.hasScorch = true;
    s.scorch = d.Enum<DamageType>("scorch");
  }
  s.vfxId = d.Str("vfxId");
  if (d.Has("port")) {
    const JNode p = d.Child("port");
    if (p.Has("dash")) {
      const JNode dash = p.Child("dash");
      s.port.hasDash = true;
      s.port.dashDurationMs = dash.Int("durationMs");
      s.port.dashStopAtMeleeRange = dash.Bool("stopAtMeleeRange", false);
    }
    s.port.persistentGround = p.Bool("persistentGround", false);
    if (s.port.persistentGround) {
      s.port.groundDurationMs = p.Int("groundDurationMs");
      s.port.groundTicks = p.Int("groundTicks");
      s.port.groundTrigger = p.Enum<GroundTrigger>("groundTrigger");
    }
    s.port.chainStaggerMs = p.Int("chainStaggerMs", 0);
    s.port.coneDeg = p.Num("coneDeg", 0.0);
    if (p.Has("maxRangeTiles")) {
      s.port.hasTeleport = true;
      s.port.teleportMaxRangeTiles = p.Num("maxRangeTiles");
      s.port.teleportTouchJoystickTiles = p.Num("touchJoystickTiles", 0.0);
      s.port.teleportTouchDeadzone = p.Num("touchDeadzone", 0.0);
      s.port.teleportWalkableSearchRings = p.Int("walkableSearchRings", 0);
    }
  }
  return s;
}

}  // namespace

StatusRule ReadStatusRule(const JNode& n) {
  StatusRule r;
  r.status = n.Enum<StatusType>("status");
  if (n.Has("chance")) {
    r.hasChance = true;
    r.chance = n.Num("chance");
  }
  const JNode v = n.Child("value");
  if (v.V().IsObject()) {
    r.valueKind = v.Enum<StatusValueKind>("kind");
    r.amount = v.Num("amount", 0.0);
    r.fraction = v.Num("fraction", 0.0);
    r.min = v.Num("min", 0.0);
  } else if (v.V().IsNumber()) {
    r.valueKind = StatusValueKind::Fixed;
    r.amount = v.AsNum();
  } else {
    v.Error("expected a status value object or number");
  }
  if (n.Has("durationMs")) {
    r.hasDuration = true;
    r.durationMs = n.Int("durationMs");
  }
  return r;
}

void LoadClassesFile(const JNode& r, ClassTables& out) {
  out.classes.clear();
  const std::vector<std::string> order = r.StrList("classOrder");
  for (const JNode& c : r.Items("classes")) {
    ClassDef def;
    def.id = c.Str("id");
    if (!ParseEnum(def.id, def.cls)) c.Child("id").Error("unknown class id '" + def.id + "'");
    def.name = c.Str("name");
    def.nameEn = c.Str("nameEn", "");
    def.description = c.Str("description", "");
    def.baseStats = ReadPrimaryStats(c.Child("baseStats"));
    def.statGrowth = ReadPrimaryStats(c.Child("statGrowth"));
    int32_t i = 0;
    for (const JNode& s : c.Items("skills")) def.skills.push_back(ReadSkill(s, def.cls, i++));
    out.classes.push_back(std::move(def));
  }
  if (order.size() != out.classes.size()) r.Child("classOrder").Error("classOrder does not match classes");
  for (size_t i = 0; i < order.size() && i < out.classes.size(); ++i) {
    if (order[i] != out.classes[i].id) r.Child("classOrder").Error("classOrder differs from the classes array order");
  }
}

void LoadSkillTreesFile(const JNode& r, ClassTables& out) {
  out.trees.clear();
  for (const JNode& t : r.Items("trees")) {
    SkillTreeDef d;
    d.id = t.Str("id");
    d.classId = t.Enum<ClassId>("classId");
    d.tabOrder = t.Int("tabOrder");
    d.nameKey = t.Str("nameKey");
    d.color = t.Color("color");
    out.trees.push_back(std::move(d));
  }
  for (const auto& [k, n] : r.Members("damageTypeColors")) {
    DamageType dt{};
    if (!ParseEnum(k, dt)) {
      n.Error("unknown damage type");
      continue;
    }
    out.damageTypeColors[EnumIndex(dt)] = n.AsColor();
  }
}

void LoadSkillRulesFile(const JNode& r, ClassTables& out) {
  SkillRules& s = out.skillRules;
  s.tierPlayerLevel.assign(1, 0);
  s.tierTreePoints.assign(1, 0);
  for (const auto& [k, n] : r.Members("tierPlayerLevel")) {
    const size_t tier = TierKey(k);
    if (tier == 0) {
      n.Error("tier keys must be 1..N");
      continue;
    }
    if (s.tierPlayerLevel.size() <= tier) s.tierPlayerLevel.resize(tier + 1, 0);
    s.tierPlayerLevel[tier] = n.AsInt();
  }
  for (const auto& [k, n] : r.Members("tierTreePoints")) {
    const size_t tier = TierKey(k);
    if (tier == 0) {
      n.Error("tier keys must be 1..N");
      continue;
    }
    if (s.tierTreePoints.size() <= tier) s.tierTreePoints.resize(tier + 1, 0);
    s.tierTreePoints[tier] = n.AsInt();
  }
  const JNode fb = r.Child("tierFallback");
  s.fallbackPlayerLevelBase = fb.Child("playerLevel").Int("base");
  s.fallbackPlayerLevelPerTierAbove1 = fb.Child("playerLevel").Int("perTierAbove1");
  s.fallbackTreePointsPerTierAbove1 = fb.Child("treePoints").Int("perTierAbove1");
  s.fallbackTreePointsMin = fb.Child("treePoints").Int("min");
  s.starterLevelTier1 = r.Child("starterLevels").Int("tier1");
  s.starterLevelOther = r.Child("starterLevels").Int("other");
  s.loadoutSize = r.Int("loadoutSize");
  if (s.loadoutSize < 1) r.Child("loadoutSize").Error("loadoutSize must be >= 1");
  s.groundAoeSkills = r.StrList("groundAoeSkills");
  const JNode sc = r.Child("scaling");
  s.tierWeights.clear();
  for (const JNode& w : sc.Items("tierWeights")) {
    SkillTierWeight tw;
    tw.fromLevel = w.Int("fromLevel");
    if (w.Has("toLevel")) {
      tw.hasToLevel = true;
      tw.toLevel = w.Int("toLevel");
    }
    tw.weight = w.Num("weight");
    s.tierWeights.push_back(tw);
  }
  // tieredScale walks levels 2..L through these brackets in order: they must start at 2, be contiguous, and only the
  // last may be open-ended (classes spec 8).
  if (s.tierWeights.empty() || s.tierWeights.front().fromLevel != 2) {
    sc.Child("tierWeights").Error("tier weights must start at level 2");
  }
  for (size_t i = 0; i < s.tierWeights.size(); ++i) {
    const SkillTierWeight& tw = s.tierWeights[i];
    const bool last = i + 1 == s.tierWeights.size();
    if (!last && (!tw.hasToLevel || s.tierWeights[i + 1].fromLevel != tw.toLevel + 1)) {
      sc.Child("tierWeights").Error("tier weight brackets must be contiguous (only the last may be open-ended)");
    }
    if (tw.hasToLevel && tw.toLevel < tw.fromLevel) sc.Child("tierWeights").Error("bracket toLevel < fromLevel");
  }
  const JNode def = sc.Child("defaults");
  s.scalingDefaults.damagePerLevel = def.Num("damagePerLevel");
  s.scalingDefaults.manaCostPerLevel = def.Num("manaCostPerLevel");
  s.scalingDefaults.cooldownReductionPerLevel = def.Num("cooldownReductionPerLevel");
  s.scalingDefaults.aoeRadiusPerLevel = def.Num("aoeRadiusPerLevel");
  s.scalingDefaults.buffValuePerLevel = def.Num("buffValuePerLevel");
  s.scalingDefaults.buffDurationPerLevel = def.Num("buffDurationPerLevel");
  s.cooldownFloorMs = sc.Num("cooldownFloorMs");
  s.cooldownReductionCapPercent = sc.Num("cooldownReductionCapPercent");
  s.rangeSlackTiles = r.Child("targeting").Num("rangeSlackTiles");
  s.groundAnchorReachSlackTiles = r.Child("targeting").Num("groundAnchorReachSlackTiles");
  s.gamepadSkillButtons = r.IntList("gamepadSkillButtons");
  s.passiveSkills = r.StrList("passiveSkills");
}

void LoadHeroFormulasFile(const JNode& r, ClassTables& out) {
  HeroFormulas& f = out.formulas;
  const JNode d = r.Child("derived");
  f.maxHpBase = d.Child("maxHp").Num("base");
  f.maxHpPerVit = d.Child("maxHp").Num("perVit");
  f.maxHpPerLevelAbove1 = d.Child("maxHp").Num("perLevelAbove1");
  f.maxManaBase = d.Child("maxMana").Num("base");
  f.maxManaPerSpi = d.Child("maxMana").Num("perSpi");
  f.maxManaPerInt = d.Child("maxMana").Num("perInt");
  f.maxManaPerLevelAbove1 = d.Child("maxMana").Num("perLevelAbove1");
  f.baseDamageBase = d.Child("baseDamage").Num("base");
  f.baseDamagePerStr = d.Child("baseDamage").Num("perStr");
  f.baseDamagePerLevel = d.Child("baseDamage").Num("perLevel");
  f.defenseBase = d.Child("defense").Num("base");
  f.defensePerVit = d.Child("defense").Num("perVit");
  f.defensePerLevel = d.Child("defense").Num("perLevel");
  f.moveSpeed = d.Num("moveSpeed");
  f.attackIntervalMs = d.Num("attackIntervalMs");
  f.attackIntervalMinMs = d.Num("attackIntervalMinMs");
  f.attackRange = d.Num("attackRange");
  const JNode g = r.Child("regen");
  f.hpRegenBase = g.Child("hpPerSecond").Num("base");
  f.hpRegenPerVit = g.Child("hpPerSecond").Num("perVit");
  f.manaRegenBase = g.Child("manaPerSecond").Num("base");
  f.manaRegenPerSpi = g.Child("manaPerSecond").Num("perSpi");
  f.campfireRadiusTiles = g.Num("campfireRadiusTiles");
  f.campfireHpMultiplier = g.Num("campfireHpMultiplier");
  f.campfireManaMultiplier = g.Num("campfireManaMultiplier");
  f.poisonedHpRegenMultiplier = g.Num("poisonedHpRegenMultiplier");
  const JNode l = r.Child("leveling");
  f.expToNextA = l.Child("expToNext").Num("a");
  f.expToNextB = l.Child("expToNext").Num("b");
  f.statPointsPerLevel = l.Int("statPointsPerLevel");
  f.skillPointsPerLevel = l.Int("skillPointsPerLevel");
  f.levelCap = l.Int("levelCap", 0);
  // addExp levels up while exp >= expToNext(L): a non-positive requirement would level up on every grant.
  if (!(f.expToNextA + f.expToNextB > 0) || f.expToNextA < 0 || f.expToNextB < 0) {
    l.Child("expToNext").Error("expToNext must be positive for every level >= 1");
  }
  const JNode m = r.Child("damage");
  f.dodgePerDex = m.Num("dodgePerDex");
  f.dodgeCapPercent = m.Num("dodgeCapPercent");
  f.critPerDex = m.Num("critPerDex");
  f.critPerLck = m.Num("critPerLck");
  f.critCapPercent = m.Num("critCapPercent");
  f.critMultiplierBase = m.Num("critMultiplierBase");
  f.critMultiplierPerLck = m.Num("critMultiplierPerLck");
  f.statToDamage = m.Num("statToDamage");
  f.defenseFactor = m.Num("defenseFactor");
  f.resistCapPercent = m.Num("resistCapPercent");
  f.damageReductionCap = m.Num("damageReductionCap");
  f.outgoingMultiplierMin = m.Num("outgoingMultiplierMin");
  f.outgoingMultiplierMax = m.Num("outgoingMultiplierMax");
  f.minDamage = m.Num("minDamage");
}

void LoadBuffCapsFile(const JNode& r, ClassTables& out) {
  out.buffCaps = BuffCaps{};
  for (const auto& [k, n] : r.Members("caps")) {
    BuffStat s{};
    if (!ParseEnum(k, s)) {
      n.Error("unknown buff stat");
      continue;
    }
    out.buffCaps.capped[EnumIndex(s)] = true;
    out.buffCaps.cap[EnumIndex(s)] = n.AsNum();
  }
  for (const JNode& u : r.Items("uncapped")) {
    BuffStat s{};
    if (!ParseEnum(u.AsStr(), s)) u.Error("unknown buff stat");
  }
}

void LoadSpiritProfilesFile(const JNode& r, ClassTables& out) {
  SpiritTable& t = out.spirit;
  for (const auto& [k, n] : r.Members("profiles")) {
    ClassId c{};
    if (!ParseEnum(k, c)) {
      n.Error("unknown class id");
      continue;
    }
    SpiritProfileDef& p = t.profiles[EnumIndex(c)];
    p.id = n.Str("id");
    p.classId = n.Enum<ClassId>("classId");
    p.visualColor = n.Color("visualColor");
    p.maxValue = n.Num("maxValue");
    p.hitGain = n.Num("hitGain");
    p.killGain = n.Num("killGain");
    p.dodgeGain = n.Num("dodgeGain");
    p.critBonusGain = n.Num("critBonusGain");
    p.resonanceDurationMs = n.Num("resonanceDurationMs");
    p.resonanceDamageBonus = n.Num("resonanceDamageBonus");
    p.resonanceManaCostMultiplier = n.Num("resonanceManaCostMultiplier");
    p.resonanceMoveSpeedBonus = n.Num("resonanceMoveSpeedBonus");
    // Spirit::Update drains maxValue / resonanceDurationMs per ms (classes 14.2).
    if (!(p.maxValue > 0)) n.Child("maxValue").Error("maxValue must be > 0");
    if (!(p.resonanceDurationMs > 0)) n.Child("resonanceDurationMs").Error("resonanceDurationMs must be > 0");
  }
  t.fallbackClass = r.Enum<ClassId>("fallbackClass");
  t.spiGainFactor = r.Num("spiGainFactor");
  const std::vector<double> clamp = r.NumList("spiClamp");
  if (clamp.size() == 2) {
    t.spiClampMin = clamp[0];
    t.spiClampMax = clamp[1];
  } else {
    r.Child("spiClamp").Error("expected [min, max]");
  }
}

void LoadStatusEffectsFile(const JNode& r, ClassTables& out) {
  StatusEffectRules& s = out.statusRules;
  s = StatusEffectRules{};
  for (const auto& [k, n] : r.Members("tickIntervalMs")) {
    StatusType t{};
    if (!ParseEnum(k, t)) {
      n.Error("unknown status type");
      continue;
    }
    s.tickIntervalMs[EnumIndex(t)] = n.AsInt();
  }
  for (const auto& [k, n] : r.Members("stacking")) {
    StatusType t{};
    if (!ParseEnum(k, t)) {
      n.Error("unknown status type");
      continue;
    }
    s.stacking[EnumIndex(t)] = n.AsEnum<StatusStacking>();
  }
  const JNode d = r.Child("diminishing");
  s.diminishingAppliesTo = d.EnumList<StatusType>("appliesTo");
  s.diminishingFactor = d.Num("factor");
  s.diminishingImmunityMs = d.Num("immunityMs");
  s.diminishingWindowMs = d.Num("windowMs");
  s.slowMinSpeedFactor = r.Num("slowMinSpeedFactor");
  s.poisonedHpRegenMultiplier = r.Num("poisonedHpRegenMultiplier");
  for (const JNode& k : r.Items("monsterKeywordRules")) {
    MonsterKeywordRule m;
    m.keywords = k.StrList("keywords");
    m.rule = ReadStatusRule(k);
    s.monsterKeywordRules.push_back(std::move(m));
  }
  const JNode e = r.Child("eliteFrozenOnHit");
  s.eliteFrozenStatus = e.Enum<StatusType>("status");
  s.eliteFrozenValue = e.Num("value");
  s.eliteFrozenDurationMs = e.Int("durationMs");
  // skillRules duplicates classes.json derived.statusRule; it is cross-checked in Finalize.
}

}  // namespace abyss::dataload

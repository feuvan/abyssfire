#include "abyss/base/Platform.h"

#include "JsonReader.h"

namespace abyss::dataload {

void LoadPetsFile(const JNode& r, PetTables& out) {
  out.pets.clear();
  for (const JNode& n : r.Items("pets")) {
    PetDef p;
    p.id = n.Str("id");
    p.chapter = n.Int("chapter");
    p.role = n.Enum<PetRole>("role");
    p.rarity = n.Enum<PetRarity>("rarity");
    p.flying = n.Bool("flying");
    p.animCategory = n.Enum<AnimRig>("animCategory");
    const JNode c = n.Child("combat");
    p.style = c.Enum<PetCombatStyle>("style");
    p.range = c.Num("range");
    p.attackMs = c.Num("attackMs");
    p.color = c.Color("color");
    p.element = c.Enum<DamageType>("element");
    const JNode pa = n.Child("passive");
    p.passiveStat = pa.Enum<Stat>("stat");
    p.passiveBase = pa.Num("base");
    p.passivePerLevel = pa.Num("perLevel");
    p.hpFraction = n.Num("hpFraction");
    for (const JNode& a : n.Items("abilities")) {
      PetAbilityDef d;
      d.id = a.Str("id");
      d.kind = a.Enum<PetAbilityKind>("kind");
      d.unlock = a.Int("unlock");
      d.cooldownMs = a.Num("cooldownMs");
      d.range = a.Num("range");
      if (a.Has("damage")) { d.hasDamage = true; d.damage = a.Num("damage"); }
      if (a.Has("value")) { d.hasValue = true; d.value = a.Num("value"); }
      if (a.Has("durationMs")) { d.hasDuration = true; d.durationMs = a.Num("durationMs"); }
      if (a.Has("radius")) { d.hasRadius = true; d.radius = a.Num("radius"); }
      if (a.Has("arc")) { d.hasArc = true; d.arc = a.Num("arc"); }
      if (a.Has("element")) { d.hasElement = true; d.element = a.Enum<DamageType>("element"); }
      d.crit = a.Bool("crit", false);
      d.leap = a.Bool("leap", false);
      if (a.Has("bleed")) { d.hasBleed = true; d.bleed = a.Num("bleed"); }
      if (a.Has("burn")) { d.hasBurn = true; d.burn = a.Num("burn"); }
      if (a.Has("slow")) { d.hasSlow = true; d.slow = a.Num("slow"); }
      if (a.Has("stunMs")) { d.hasStun = true; d.stunMs = a.Num("stunMs"); }
      d.hits = a.Int("hits", 1);
      if (a.Has("mana")) { d.hasMana = true; d.mana = a.Num("mana"); }
      d.self = a.Bool("self", false);
      p.abilities.push_back(std::move(d));
    }
    const JNode der = n.Child("derived");
    p.primaryAbilityId = der.Str("primaryAbilityId", "");
    p.nameKey = der.Child("i18n").Str("name");
    p.descKey = der.Child("i18n").Str("desc");
    p.originKey = der.Child("i18n").Str("origin");
    out.pets.push_back(std::move(p));
  }
  out.maxLevel = r.Int("maxLevel");
  out.evolutionLevels = r.IntList("evolutionLevels");
  out.evolutionMult = r.NumList("evolutionMult");
  out.maxBond = r.Int("maxBond");
  out.leyFruitId = r.Str("leyFruitId");
  const JNode s = r.Child("system");
  PetSystemConstants& k = out.system;
  k.damageBaseFraction = s.Num("damageBaseFraction");
  k.damagePerLevelFraction = s.Num("damagePerLevelFraction");
  k.damageMaxFraction = s.Num("damageMaxFraction");
  k.bondProgressPerLevel = s.Int("bondProgressPerLevel");
  k.bondPerKill = s.Int("bondPerKill");
  k.bondPerActiveMinute = s.Int("bondPerActiveMinute");
  k.bondPerFeed = s.Int("bondPerFeed");
  k.feedExp = s.Num("feedExp");
  k.bondRescueHp = s.Num("bondRescueHp");
  k.bondRescueCooldownMs = s.Num("bondRescueCooldownMs");
  k.baseBondCap = s.Int("baseBondCap");
  k.expToNextBase = s.Child("expToNext").Num("base");
  k.expToNextPerLevel = s.Child("expToNext").Num("perLevel");
  k.killExpBase = s.Child("killExp").Num("base");
  k.killExpPerMonsterLevel = s.Child("killExp").Num("perMonsterLevel");
  k.bondMultiplierPerBond = s.Num("bondMultiplierPerBond");
  k.leyFruitDropElite = s.Child("leyFruitDropChance").Num("elite");
  k.leyFruitDropOther = s.Child("leyFruitDropChance").Num("other");
  const JNode c = r.Child("companion");
  out.companion.exhaustMs = c.Num("exhaustMs");
  out.companion.straySwingChance = c.Num("straySwingChance");
  out.companion.followSpeedTilesPerSec = c.Num("followSpeedTilesPerSec");
  out.companion.dashSpeedTilesPerSec = c.Num("dashSpeedTilesPerSec");
  out.companion.teleportDistanceTiles = c.Num("teleportDistanceTiles");
  out.chapter1Slice = r.Child("port").StrList("chapter1Slice", true);
}

void LoadHomesteadFile(const JNode& r, HomesteadTables& out) {
  out.buildings.clear();
  out.expeditionOptions.clear();
  out.blessings.clear();
  out.towerZoneId = r.Str("towerZoneId");
  out.towerUnlockQuest = r.Str("towerUnlockQuest");
  out.leyFruitId = r.Str("leyFruitId");
  for (const JNode& b : r.Items("buildings")) {
    BuildingDef d;
    d.id = b.Str("id");
    d.name = b.Str("name");
    d.description = b.Str("description", "");
    d.maxLevel = b.Int("maxLevel");
    for (const JNode& c : b.Items("costPerLevel")) d.costPerLevel.push_back(BuildingLevelCost{c.I64("gold"), c.Int("embers", 0)});
    for (const JNode& x : b.Items("bonusPerLevel")) d.bonusPerLevel.push_back(StatValue{x.Enum<Stat>("stat"), x.Num("value")});
    d.unlockQuest = b.Str("unlockQuest", "");
    d.allyNpc = b.Str("allyNpc", "");
    out.buildings.push_back(std::move(d));
  }
  out.gemCombineCount = r.Int("gemCombineCount");
  for (const JNode& e : r.Items("expeditionOptions")) {
    out.expeditionOptions.push_back(ExpeditionOptionDef{e.Str("id"), e.Int("killsRequired"), e.Num("durationMs")});
  }
  for (const JNode& b : r.Items("blessings")) {
    out.blessings.push_back(BlessingDef{b.Str("id"), b.Stats("stats"), b.Str("glyph", "")});
  }
  out.blessingDurationMs = r.Num("blessingDurationMs");
  const JNode e = r.Child("embers");
  out.embersKillElite = e.Int("killElite");
  out.embersKillMiniBoss = e.Int("killMiniBoss");
  out.embersKillAffixedPerAffix = e.Int("killAffixedPerAffix");
  out.embersKillPlain = e.Int("killPlain");
  out.embersQuestMain = e.Int("questMain");
  out.embersQuestSide = e.Int("questSide");
  const JNode g = r.Child("garden");
  out.gardenIntervalByLevel = g.IntList("intervalByLevel");
  out.gardenCapacityByLevel = g.IntList("capacityByLevel");
  out.gardenLeyFruitBase = g.Child("yield").Num("leyFruitBase");
  out.gardenLeyFruitPerLevel = g.Child("yield").Num("leyFruitPerLevel");
  out.gardenHpShare = g.Child("yield").Num("hpShare");
  out.towerHiddenInMilestone1 = r.Child("port").Bool("towerHiddenInMilestone1", true);
}

}  // namespace abyss::dataload

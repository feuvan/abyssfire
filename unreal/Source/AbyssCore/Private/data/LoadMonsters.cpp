#include "abyss/base/Platform.h"

#include <algorithm>
#include <initializer_list>
#include <string>
#include <string_view>

#include "JsonReader.h"

#include "abyss/monsters/Monster.h"

namespace abyss::dataload {

namespace {

// ---- K2 abilities (implementation-plan.md 3 K2): shape rules checked at load (the exporter checks them first) -------

// Fairness floor of an authored windup (monsters.md 1.3: specials >= 600 ms; bosses >= 700 ms, checked by
// DataStore::Finalize, which knows the story boss).
constexpr double kLoadAbilityMinWindupMs = 600;
// Hazards tick at most ten times a second.
constexpr double kLoadHazardMinTickMs = 100;
// Upper bounds (no authored value comes near them; they keep a typo from rooting a caster for hours or a hazard from
// ticking forever): a windup <= 5 s, a hazard <= 60 s (<= 600 ticks at the 100 ms floor, HazardField kHazardMaxTicks),
// a cooldown or initial cooldown <= 10 min.
constexpr double kLoadAbilityMaxWindupMs = 5000;
constexpr double kLoadHazardMaxDurationMs = 60000;
constexpr double kLoadAbilityMaxCooldownMs = 600000;
// K3 (implementation-plan.md 3 K3, monsters.md 1.3): an exploder's fuse lasts >= 800 ms; no on-death effect or rise
// waits more than 10 s / 60 s; a raise searches at most 30 tiles; an on-death spawn stands at most 20 bodies.
constexpr double kLoadBurstMinFuseMs = 800;
constexpr double kLoadOnDeathMaxDelayMs = 10000;
constexpr double kLoadRiseMaxDelayMs = 60000;
constexpr double kLoadSummonMaxRadius = 30;
constexpr int32_t kLoadOnDeathMaxSpawn = 20;
// K4 (implementation-plan.md 3 K4): phase multipliers in (0, 10]; a tether, a feed or a summon ring within 30 tiles, an
// arena within 60; an arena closes over <= 10 min; a phase summon waits <= 60 s, stands back up <= 10 times, each after
// <= 10 min; a feed draws its corpse in <= 10 s.
constexpr double kLoadPhaseMaxMul = 10;
constexpr double kLoadPhaseMaxRadius = 30;
constexpr double kLoadArenaMaxRadius = 60;
constexpr double kLoadPhaseSummonMaxDelayMs = 60000;
constexpr int32_t kLoadPhaseSummonMaxRespawns = 10;
constexpr double kLoadFeedMaxDelayMs = 10000;
// K5 (implementation-plan.md 3 K5): pack, kite, wall, morale, aura and buff radii within 30 tiles; a flee lasts <= 60 s.
constexpr double kLoadPackMaxRadius = 30;
constexpr double kLoadFleeMaxMs = 60000;

// An object's keys must all be known (a misspelt optional field would otherwise be silently ignored).
void LoadMonsterCheckKeys(const JNode& n, std::initializer_list<std::string_view> known, std::string_view what) {
  for (const auto& [key, child] : n.SelfMembers(true)) {
    bool ok = false;
    for (std::string_view k : known) ok = ok || k == key;
    if (!ok) child.Error("unknown " + std::string(what) + " field '" + key + "'");
  }
}

bool LoadAbilityShapeOfKind(AbilityKind kind, AbilityShapeKind& out) {
  switch (kind) {
    case AbilityKind::Circle: out = AbilityShapeKind::Circle; return true;
    case AbilityKind::Cone: out = AbilityShapeKind::Cone; return true;
    case AbilityKind::Line: out = AbilityShapeKind::Line; return true;
    case AbilityKind::Ring: out = AbilityShapeKind::Ring; return true;
    default: return false;
  }
}

// {kind?, radius, arcDeg, length, width, innerRadius}; `kind` defaults to `implied` (required when there is none).
AbilityShapeDef ReadAbilityShape(const JNode& n, bool hasImplied, AbilityShapeKind implied) {
  AbilityShapeDef s;
  LoadMonsterCheckKeys(n, {"kind", "radius", "arcDeg", "length", "width", "innerRadius"}, "shape");
  if (n.Has("kind")) {
    s.kind = n.Enum<AbilityShapeKind>("kind");
    if (hasImplied && s.kind != implied) n.Child("kind").Error("differs from the ability's kind");
  } else if (hasImplied) {
    s.kind = implied;
  } else {
    n.Error("a hazard shape needs its 'kind' (circle | cone | line | ring)");
  }
  s.radius = n.Num("radius", 0.0);
  s.arcDeg = n.Num("arcDeg", 0.0);
  s.length = n.Num("length", 0.0);
  s.width = n.Num("width", 0.0);
  s.innerRadius = n.Num("innerRadius", 0.0);
  switch (s.kind) {
    case AbilityShapeKind::Circle:
      if (!(s.radius > 0)) n.Error("a circle needs radius > 0");
      break;
    case AbilityShapeKind::Ring:
      if (!(s.innerRadius >= 0) || !(s.radius > s.innerRadius)) n.Error("a ring needs radius > innerRadius >= 0");
      break;
    case AbilityShapeKind::Cone:
      if (!(s.radius > 0) || !(s.arcDeg > 0) || !(s.arcDeg <= 360)) n.Error("a cone needs radius > 0, 0 < arcDeg <= 360");
      break;
    case AbilityShapeKind::Line:
      if (!(s.length > 0) || !(s.width > 0)) n.Error("a line needs length > 0 and width > 0");
      break;
  }
  return s;
}

AbilityStatusDef ReadAbilityStatus(const JNode& n) {
  AbilityStatusDef s;
  LoadMonsterCheckKeys(n, {"type", "chance", "ms", "value"}, "status");
  s.present = true;
  s.type = n.Enum<StatusType>("type");
  s.chance = n.Num("chance", 1.0);
  s.ms = n.Num("ms");
  s.value = n.Num("value");
  if (!(s.chance > 0) || !(s.chance <= 1)) n.Child("chance").Error("expected 0 < chance <= 1");
  if (!(s.ms > 0)) n.Child("ms").Error("expected ms > 0");
  if (!(s.value > 0)) n.Child("value").Error("expected value > 0 (a status of value 0 never applies)");
  return s;
}

MonsterAbilityDef ReadMonsterAbility(const JNode& n) {
  MonsterAbilityDef a;
  LoadMonsterCheckKeys(n,
            {"id", "kind", "clip", "windupMs", "cooldownMs", "initialCooldownMs", "rangeMin", "rangeMax", "hpBelowPct",
             "phases", "priority", "aim", "shape", "dmgMul", "damageType", "status", "hitsMonsters", "monsterDmgMul",
             "hazard", "summon", "buff"},
            "ability");
  a.id = n.Str("id");
  if (a.id.empty()) n.Child("id").Error("empty ability id");
  a.kind = n.Enum<AbilityKind>("kind");
  a.clip = n.Str("clip");
  if (a.clip.empty()) n.Child("clip").Error("empty clip");
  if (n.Has("windupMs")) {
    a.hasWindupMs = true;
    a.windupMs = n.Num("windupMs");
    if (!(a.windupMs >= kLoadAbilityMinWindupMs)) {
      n.Child("windupMs").Error("a special winds up >= 600 ms (monsters.md 1.3)");
    } else if (!(a.windupMs <= kLoadAbilityMaxWindupMs)) {
      n.Child("windupMs").Error("expected windupMs <= 5000");
    }
  }
  a.cooldownMs = n.Num("cooldownMs");
  a.initialCooldownMs = n.Num("initialCooldownMs", 0.0);
  if (!(a.cooldownMs >= 0)) {
    n.Child("cooldownMs").Error("expected >= 0");
  } else if (!(a.cooldownMs <= kLoadAbilityMaxCooldownMs)) {
    n.Child("cooldownMs").Error("expected <= 600000");
  }
  if (!(a.initialCooldownMs >= 0)) {
    n.Child("initialCooldownMs").Error("expected >= 0");
  } else if (!(a.initialCooldownMs <= kLoadAbilityMaxCooldownMs)) {
    n.Child("initialCooldownMs").Error("expected <= 600000");
  }
  a.rangeMin = n.Num("rangeMin", 0.0);
  if (!(a.rangeMin >= 0)) n.Child("rangeMin").Error("expected >= 0");
  if (n.Has("rangeMax")) {
    a.hasRangeMax = true;
    a.rangeMax = n.Num("rangeMax");
    if (!(a.rangeMax > a.rangeMin)) n.Child("rangeMax").Error("expected rangeMax > rangeMin");
  }
  if (n.Has("hpBelowPct")) {
    a.hasHpBelowPct = true;
    a.hpBelowPct = n.Num("hpBelowPct");
    if (!(a.hpBelowPct > 0) || !(a.hpBelowPct <= 100)) n.Child("hpBelowPct").Error("expected 0 < hpBelowPct <= 100");
  }
  a.phases = n.IntList("phases", true);
  for (int32_t p : a.phases) {
    if (p < 0) n.Child("phases").Error("phase indices are >= 0");
  }
  a.priority = n.Int("priority", 0);
  a.aim = n.Enum<AbilityAim>("aim");
  a.dmgMul = n.Num("dmgMul", 1.0);
  if (!(a.dmgMul >= 0)) n.Child("dmgMul").Error("expected >= 0");
  if (n.Has("damageType")) {
    a.hasDamageType = true;
    a.damageType = n.Enum<DamageType>("damageType");
  }
  if (n.Has("status")) a.status = ReadAbilityStatus(n.Child("status"));
  a.hitsMonsters = n.Bool("hitsMonsters", false);
  a.monsterDmgMul = n.Num("monsterDmgMul", 1.0);
  if (!(a.monsterDmgMul >= 0)) n.Child("monsterDmgMul").Error("expected >= 0");

  // Shape: required for the ground kinds; its kind follows the ability's (a hazard names its own).
  AbilityShapeKind implied = AbilityShapeKind::Circle;
  const bool hasImplied = LoadAbilityShapeOfKind(a.kind, implied);
  const bool groundKind = hasImplied || a.kind == AbilityKind::Hazard;
  if (n.Has("shape")) {
    if (!groundKind) n.Child("shape").Error("only circle / cone / line / ring / hazard abilities have a shape");
    a.hasShape = true;
    a.shape = ReadAbilityShape(n.Child("shape"), hasImplied, implied);
  } else if (groundKind) {
    n.Error("a " + std::string(EnumName(a.kind)) + " ability needs a shape");
  }

  // Aim per kind (AbilityAim comment).
  bool aimOk = true;
  switch (a.kind) {
    case AbilityKind::Circle:
    case AbilityKind::Ring:
    case AbilityKind::Hazard:
      aimOk = a.aim == AbilityAim::HeroAtWindup || a.aim == AbilityAim::Self || a.aim == AbilityAim::Anchor;
      break;
    case AbilityKind::Cone:
    case AbilityKind::Line:
      aimOk = a.aim != AbilityAim::Corpse;
      break;
    case AbilityKind::Bolt: aimOk = a.aim == AbilityAim::Hero; break;
    case AbilityKind::Raise: aimOk = a.aim == AbilityAim::Corpse; break;
    case AbilityKind::Summon: aimOk = a.aim != AbilityAim::Corpse; break;
    case AbilityKind::Buff:
    case AbilityKind::Call: aimOk = a.aim == AbilityAim::Self; break;
  }
  if (!aimOk) {
    n.Child("aim").Error("aim '" + std::string(EnumName(a.aim)) + "' does not fit a " + std::string(EnumName(a.kind)) +
                         " ability");
  }

  // Hazard: the ability itself (kind hazard) or what a ground strike leaves behind.
  if (n.Has("hazard")) {
    const JNode h = n.Child("hazard");
    LoadMonsterCheckKeys(h, {"durationMs", "tickMs", "tickDmgMul", "status", "shape"}, "hazard");
    if (!groundKind) h.Error("only ground abilities leave a hazard");
    a.hazard.present = true;
    a.hazard.durationMs = h.Num("durationMs");
    a.hazard.tickMs = h.Num("tickMs");
    a.hazard.tickDmgMul = h.Num("tickDmgMul");
    if (!(a.hazard.durationMs > 0)) {
      h.Child("durationMs").Error("expected > 0");
    } else if (!(a.hazard.durationMs <= kLoadHazardMaxDurationMs)) {
      h.Child("durationMs").Error("expected <= 60000");
    }
    if (!(a.hazard.tickMs >= kLoadHazardMinTickMs) || !(a.hazard.tickMs <= a.hazard.durationMs)) {
      h.Child("tickMs").Error("expected 100 <= tickMs <= durationMs");
    }
    if (!(a.hazard.tickDmgMul >= 0)) h.Child("tickDmgMul").Error("expected >= 0");
    if (h.Has("status")) a.hazard.status = ReadAbilityStatus(h.Child("status"));
    if (h.Has("shape")) {
      a.hazard.hasShape = true;
      a.hazard.shape = ReadAbilityShape(h.Child("shape"), false, AbilityShapeKind::Circle);
    }
  } else if (a.kind == AbilityKind::Hazard) {
    n.Error("a hazard ability needs its 'hazard' block");
  }
  if (a.kind == AbilityKind::Hazard && a.hazard.present && a.hazard.hasShape) {
    n.Child("hazard").Child("shape").Error("a hazard ability's shape is the ability's own 'shape'");
  }
  // A hazard ability has no contact hit: its pulses are the hazard's ticks.
  if (a.kind == AbilityKind::Hazard && (n.Has("dmgMul") || n.Has("status"))) {
    n.Error("a hazard ability hits through its hazard: use hazard.tickDmgMul / hazard.status");
  }

  // Summon (raise / summon).
  const bool summons = a.kind == AbilityKind::Raise || a.kind == AbilityKind::Summon;
  if (n.Has("summon")) {
    const JNode s = n.Child("summon");
    LoadMonsterCheckKeys(s, {"monsterId", "count", "at", "anchors", "capAlive", "perWindowCap", "windowMs", "hpPct",
                             "radius"},
                         "summon");
    if (!summons) s.Error("only raise / summon abilities summon");
    a.summon.present = true;
    a.summon.monsterId = s.Str("monsterId");
    a.summon.count = s.Int("count", 1);
    a.summon.at =
        s.Enum<AbilitySummonAt>("at", a.kind == AbilityKind::Raise ? AbilitySummonAt::Corpse : AbilitySummonAt::Self);
    for (const JNode& t : s.Items("anchors", true)) a.summon.anchors.push_back(t.AsTile());
    a.summon.capAlive = s.Int("capAlive", 0);
    a.summon.perWindowCap = s.Int("perWindowCap", 0);
    a.summon.windowMs = s.Num("windowMs", 0.0);
    a.summon.hpPct = s.Num("hpPct", 100.0);
    if (a.summon.monsterId.empty()) s.Child("monsterId").Error("empty monster id");
    if (a.summon.count < 1) s.Child("count").Error("expected >= 1");
    if (a.summon.capAlive < 0 || a.summon.perWindowCap < 0) s.Error("caps are >= 0");
    if (!(a.summon.windowMs >= 0) || !(a.summon.windowMs <= kLoadAbilityMaxCooldownMs)) {
      s.Child("windowMs").Error("expected 0 <= windowMs <= 600000");  // K3: the caster keeps 10 min of its summons
    }
    if (a.summon.perWindowCap > 0 && !(a.summon.windowMs > 0)) {
      s.Child("windowMs").Error("a per-window cap needs windowMs > 0");
    }
    if (!(a.summon.hpPct > 0) || !(a.summon.hpPct <= 100)) s.Child("hpPct").Error("expected 0 < hpPct <= 100");
    if (a.summon.at == AbilitySummonAt::Anchors && a.summon.anchors.empty()) {
      s.Child("anchors").Error("at 'anchors' needs anchors");
    }
    if (a.kind == AbilityKind::Raise && a.summon.at != AbilitySummonAt::Corpse) {
      s.Child("at").Error("a raise stands a corpse up");
    }
    // K3: the corpse search radius of a raise, the ring of a summon at `self`.
    if (s.Has("radius")) {
      a.summon.hasRadius = true;
      a.summon.radius = s.Num("radius");
      if (!(a.summon.radius > 0) || !(a.summon.radius <= kLoadSummonMaxRadius)) {
        s.Child("radius").Error("expected 0 < radius <= 30");
      } else if (a.summon.at == AbilitySummonAt::Anchors) {
        s.Child("radius").Error("a summon at 'anchors' stands its bodies on the anchors: no radius");
      }
    }
  } else if (summons) {
    n.Error("a " + std::string(EnumName(a.kind)) + " ability needs its 'summon' block");
  }

  // Buff (buff / call). K5: a call provokes within `radius` (> 0, required) and buffs only when it names a `stat` (with
  // its `ms`); a buff always names both (radius 0 = the caster alone).
  if (n.Has("buff")) {
    const JNode b = n.Child("buff");
    LoadMonsterCheckKeys(b, {"stat", "mul", "ms", "radius", "family"}, "buff");
    if (a.kind != AbilityKind::Buff && a.kind != AbilityKind::Call) b.Error("only buff / call abilities buff");
    a.buff.present = true;
    const bool call = a.kind == AbilityKind::Call;
    a.buff.hasStat = !call || b.Has("stat");
    if (a.buff.hasStat) a.buff.stat = b.Enum<MonsterBuffStat>("stat");
    a.buff.mul = b.Num("mul", 1.0);
    a.buff.ms = a.buff.hasStat || b.Has("ms") ? b.Num("ms") : 0.0;
    a.buff.radius = b.Num("radius", 0.0);
    a.buff.family = b.Str("family", "");
    if (!(a.buff.mul > 0) || !(a.buff.mul <= kLoadPhaseMaxMul)) b.Child("mul").Error("expected 0 < mul <= 10");
    if (a.buff.hasStat && !(a.buff.ms > 0)) {
      b.Child("ms").Error("expected > 0");
    } else if (!(a.buff.ms <= kLoadAbilityMaxCooldownMs)) {
      b.Child("ms").Error("expected <= 600000");
    }
    if (!a.buff.hasStat && b.Has("ms")) b.Child("ms").Error("a call buffs for `ms` only with a `stat`");
    if (!a.buff.hasStat && b.Has("mul")) b.Child("mul").Error("a call buffs by `mul` only with a `stat`");
    if (!(a.buff.radius >= 0) || !(a.buff.radius <= kLoadPackMaxRadius)) {
      b.Child("radius").Error("expected 0 <= radius <= 30");
    } else if (call && !(a.buff.radius > 0)) {
      b.Child("radius").Error("a call reaches radius > 0 tiles");
    }
    if (b.Has("family") && a.buff.family.empty()) b.Child("family").Error("an empty family id");
  } else if (a.kind == AbilityKind::Buff) {
    n.Error("a buff ability needs its 'buff' block");
  } else if (a.kind == AbilityKind::Call) {
    n.Error("a call ability needs its 'buff' block (its radius, family and optional buff)");
  }
  return a;
}

// K4: an optional {col, row} centre (default: the monster's authored spawn tile, MonsterInstance::homeTile).
void ReadPhaseCentre(const JNode& n, bool& has, TilePos& out) {
  if (!n.Has("centre")) return;
  const JNode c = n.Child("centre");
  LoadMonsterCheckKeys(c, {"col", "row"}, "centre");
  has = true;
  out = c.AsTile();
}

// A multiplier in (0, 10] (default 1).
double ReadPhaseMul(const JNode& n, std::string_view key) {
  const double v = n.Num(key, 1.0);
  if (!(v > 0) || !(v <= kLoadPhaseMaxMul)) n.Child(key).Error("expected 0 < " + std::string(key) + " <= 10");
  return v;
}

// K4 phase row (MonsterData.h MonsterPhaseDef): every key known at every level; ability ids are checked by the caller.
MonsterPhaseDef ReadMonsterPhase(const JNode& n) {
  MonsterPhaseDef p;
  LoadMonsterCheckKeys(n,
                       {"hpBelowPct", "enterAction", "speedMul", "attackSpeedMul", "damageTakenMul", "leash",
                        "abilitiesOn", "abilitiesOff", "summons", "arena", "feed", "stinger"},
                       "phase");
  p.hpBelowPct = n.Num("hpBelowPct");
  if (!(p.hpBelowPct > 0) || !(p.hpBelowPct <= 100)) n.Child("hpBelowPct").Error("expected 0 < hpBelowPct <= 100");
  if (n.Has("enterAction")) {
    p.hasEnterAction = true;
    p.enterAction = n.Enum<AnimAction>("enterAction");
    if (p.enterAction == AnimAction::Idle || p.enterAction == AnimAction::Walk || p.enterAction == AnimAction::Death) {
      n.Child("enterAction").Error("enterAction must be an action, not idle / walk / death");
    }
  }
  p.speedMul = ReadPhaseMul(n, "speedMul");
  p.attackSpeedMul = ReadPhaseMul(n, "attackSpeedMul");
  p.damageTakenMul = ReadPhaseMul(n, "damageTakenMul");
  if (n.Has("leash")) {
    const JNode l = n.Child("leash");
    LoadMonsterCheckKeys(l, {"centre", "radius"}, "leash");
    p.leash.present = true;
    ReadPhaseCentre(l, p.leash.hasCentre, p.leash.centre);
    p.leash.radius = l.Num("radius");
    if (!(p.leash.radius > 0) || !(p.leash.radius <= kLoadPhaseMaxRadius)) l.Child("radius").Error("expected 0 < radius <= 30");
  }
  p.abilitiesOn = n.StrList("abilitiesOn", true);
  p.abilitiesOff = n.StrList("abilitiesOff", true);
  for (const std::string& id : p.abilitiesOn) {
    if (std::find(p.abilitiesOff.begin(), p.abilitiesOff.end(), id) != p.abilitiesOff.end()) {
      n.Child("abilitiesOff").Error("'" + id + "' is switched both on and off");
    }
  }
  for (const JNode& s : n.Items("summons", true)) {
    LoadMonsterCheckKeys(s, {"monsterId", "count", "at", "anchors", "radius", "hpPct", "delayMs", "respawnMs", "respawns"},
                         "phase summon");
    PhaseSummonDef d;
    d.monsterId = s.Str("monsterId");
    d.count = s.Int("count", 1);
    d.at = s.Enum<AbilitySummonAt>("at", AbilitySummonAt::Self);
    for (const JNode& t : s.Items("anchors", true)) {
      LoadMonsterCheckKeys(t, {"col", "row"}, "anchor");
      d.anchors.push_back(t.AsTile());
    }
    d.hpPct = s.Num("hpPct", 100.0);
    d.delayMs = s.Num("delayMs", 0.0);
    d.respawnMs = s.Num("respawnMs", 0.0);
    d.respawns = s.Int("respawns", 0);
    if (d.monsterId.empty()) s.Child("monsterId").Error("empty monster id");
    if (d.count < 1 || d.count > kLoadOnDeathMaxSpawn) s.Child("count").Error("expected 1 <= count <= 20");
    if (d.at == AbilitySummonAt::Corpse) s.Child("at").Error("a phase summons at 'self' or 'anchors'");
    if (d.at == AbilitySummonAt::Anchors && d.anchors.empty()) s.Child("anchors").Error("at 'anchors' needs anchors");
    if (d.at != AbilitySummonAt::Anchors && !d.anchors.empty()) s.Child("anchors").Error("anchors need at 'anchors'");
    if (s.Has("radius")) {
      d.hasRadius = true;
      d.radius = s.Num("radius");
      if (!(d.radius > 0) || !(d.radius <= kLoadPhaseMaxRadius)) {
        s.Child("radius").Error("expected 0 < radius <= 30");
      } else if (d.at == AbilitySummonAt::Anchors) {
        s.Child("radius").Error("a summon at 'anchors' stands its bodies on the anchors: no radius");
      }
    }
    if (!(d.hpPct > 0) || !(d.hpPct <= 100)) s.Child("hpPct").Error("expected 0 < hpPct <= 100");
    if (!(d.delayMs >= 0) || !(d.delayMs <= kLoadPhaseSummonMaxDelayMs)) {
      s.Child("delayMs").Error("expected 0 <= delayMs <= 60000");
    }
    if (!(d.respawnMs >= 0) || !(d.respawnMs <= kLoadAbilityMaxCooldownMs)) {
      s.Child("respawnMs").Error("expected 0 <= respawnMs <= 600000");
    }
    if (d.respawns < 0 || d.respawns > kLoadPhaseSummonMaxRespawns) s.Child("respawns").Error("expected 0 <= respawns <= 10");
    p.summons.push_back(std::move(d));
  }
  if (n.Has("arena")) {
    const JNode a = n.Child("arena");
    LoadMonsterCheckKeys(a, {"kind", "centre", "fromR", "toR", "overMs", "dpsPctMaxHp", "damageType"}, "arena");
    p.arena.present = true;
    p.arena.kind = a.Enum<ArenaKind>("kind");
    ReadPhaseCentre(a, p.arena.hasCentre, p.arena.centre);
    p.arena.fromR = a.Num("fromR");
    p.arena.toR = a.Num("toR");
    p.arena.overMs = a.Num("overMs");
    p.arena.dpsPctMaxHp = a.Num("dpsPctMaxHp");
    p.arena.damageType = a.Enum<DamageType>("damageType", DamageType::Physical);
    if (!(p.arena.fromR > 0) || !(p.arena.fromR <= kLoadArenaMaxRadius)) a.Child("fromR").Error("expected 0 < fromR <= 60");
    if (!(p.arena.toR > 0) || !(p.arena.toR <= p.arena.fromR)) a.Child("toR").Error("expected 0 < toR <= fromR (a ring shrinks)");
    if (!(p.arena.overMs > 0) || !(p.arena.overMs <= kLoadAbilityMaxCooldownMs)) {
      a.Child("overMs").Error("expected 0 < overMs <= 600000");
    }
    if (!(p.arena.dpsPctMaxHp > 0) || !(p.arena.dpsPctMaxHp <= 100)) {
      a.Child("dpsPctMaxHp").Error("expected 0 < dpsPctMaxHp <= 100 (percent of the hero's max life per second)");
    }
  }
  if (n.Has("feed")) {
    const JNode f = n.Child("feed");
    LoadMonsterCheckKeys(f, {"centre", "radius", "healPct", "delayMs"}, "feed");
    p.feed.present = true;
    ReadPhaseCentre(f, p.feed.hasCentre, p.feed.centre);
    p.feed.radius = f.Num("radius");
    p.feed.healPct = f.Num("healPct");
    p.feed.delayMs = f.Num("delayMs", 0.0);
    if (!(p.feed.radius > 0) || !(p.feed.radius <= kLoadPhaseMaxRadius)) f.Child("radius").Error("expected 0 < radius <= 30");
    if (!(p.feed.healPct > 0) || !(p.feed.healPct <= 100)) f.Child("healPct").Error("expected 0 < healPct <= 100");
    if (!(p.feed.delayMs >= 0) || !(p.feed.delayMs <= kLoadFeedMaxDelayMs)) {
      f.Child("delayMs").Error("expected 0 <= delayMs <= 10000");
    }
  }
  if (n.Has("stinger")) {
    p.stinger = n.Str("stinger");
    if (p.stinger.empty()) n.Child("stinger").Error("an empty stinger id");
  }
  return p;
}

// K5 blocks (MonsterData.h "K5"): every key known; radii and times bounded; DataStore::Finalize checks what needs the
// rest of the data (a kite on a ranged monster within its swing reach).
void ReadMonsterK5(const JNode& n, MonsterDef& m) {
  const auto radius = [](const JNode& node, std::string_view key, bool required) {
    if (!required && !node.Has(key)) return 0.0;
    const double v = node.Num(key);
    if (!(v > 0) || !(v <= kLoadPackMaxRadius)) node.Child(key).Error("expected 0 < " + std::string(key) + " <= 30");
    return v;
  };
  if (n.Has("packAggro")) m.packAggro = radius(n, "packAggro", true);
  if (n.Has("kite")) {
    const JNode k = n.Child("kite");
    LoadMonsterCheckKeys(k, {"min", "max"}, "kite");
    m.kite.present = true;
    m.kite.min = radius(k, "min", true);
    m.kite.max = radius(k, "max", true);
    if (!(m.kite.max > m.kite.min)) k.Child("max").Error("expected max > min (the band it keeps from the hero)");
  }
  if (n.Has("block")) {
    const JNode b = n.Child("block");
    LoadMonsterCheckKeys(b, {"arcDeg", "damageMul", "wall"}, "block");
    m.block.present = true;
    m.block.arcDeg = b.Num("arcDeg");
    m.block.damageMul = b.Num("damageMul");
    m.block.wall = radius(b, "wall", false);
    if (!(m.block.arcDeg > 0) || !(m.block.arcDeg <= 360)) b.Child("arcDeg").Error("expected 0 < arcDeg <= 360");
    if (!(m.block.damageMul >= 0) || !(m.block.damageMul < 1)) {
      b.Child("damageMul").Error("expected 0 <= damageMul < 1 (a blocked hit is reduced)");
    }
  }
  if (n.Has("morale")) {
    const JNode o = n.Child("morale");
    LoadMonsterCheckKeys(o, {"fleeOnLeaderDeathMs", "fleeRadius", "sustainFamily", "sustainRadius"}, "morale");
    m.morale.present = true;
    m.morale.fleeOnLeaderDeathMs = o.Num("fleeOnLeaderDeathMs");
    if (!(m.morale.fleeOnLeaderDeathMs > 0) || !(m.morale.fleeOnLeaderDeathMs <= kLoadFleeMaxMs)) {
      o.Child("fleeOnLeaderDeathMs").Error("expected 0 < fleeOnLeaderDeathMs <= 60000");
    }
    m.morale.fleeRadius = radius(o, "fleeRadius", false);
    m.morale.sustainRadius = radius(o, "sustainRadius", false);
    if (o.Has("sustainFamily")) {
      m.morale.sustainFamily = o.Str("sustainFamily");
      if (m.morale.sustainFamily.empty()) o.Child("sustainFamily").Error("an empty family id");
      if (!o.Has("sustainRadius")) o.Child("sustainFamily").Error("a sustain needs its sustainRadius");
    }
  }
  if (n.Has("aura")) {
    const JNode a = n.Child("aura");
    LoadMonsterCheckKeys(a, {"stat", "mul", "radius", "family"}, "aura");
    m.aura.present = true;
    m.aura.stat = a.Enum<MonsterBuffStat>("stat");
    m.aura.mul = a.Num("mul", 1.0);
    m.aura.radius = radius(a, "radius", true);
    if (!(m.aura.mul > 0) || !(m.aura.mul <= kLoadPhaseMaxMul)) a.Child("mul").Error("expected 0 < mul <= 10");
    if (a.Has("family")) {
      m.aura.family = a.Str("family");
      if (m.aura.family.empty()) a.Child("family").Error("an empty family id");
    }
  }
}

}  // namespace

// K3 on-death block (MonsterData.h MonsterOnDeathDef): every key known; per kind its shape, damage and blocks.
MonsterOnDeathDef ReadMonsterOnDeath(const JNode& n) {
  MonsterOnDeathDef d;
  LoadMonsterCheckKeys(n,
                       {"kind", "delayMs", "shape", "dmg", "hpPct", "damageType", "status", "hitsMonsters",
                        "monsterDmgMul", "capHeroMaxHpPct", "puddle", "spawn"},
                       "onDeath");
  d.present = true;
  d.kind = n.Enum<OnDeathKind>("kind");
  d.delayMs = n.Num("delayMs", 0.0);
  if (!(d.delayMs >= 0) || !(d.delayMs <= kLoadOnDeathMaxDelayMs)) {
    n.Child("delayMs").Error("expected 0 <= delayMs <= 10000");
  } else if (d.kind == OnDeathKind::Burst && !(d.delayMs >= kLoadBurstMinFuseMs)) {
    n.Child("delayMs").Error("an exploder's fuse lasts >= 800 ms (monsters.md 1.3)");
  }
  const bool spawns = d.kind == OnDeathKind::Spawn;
  // Shape: a burst is a circle; a ring or a puddle a ring or a circle (`kind` names which; a ring defaults to a ring,
  // a puddle to a circle); a spawn has none.
  if (n.Has("shape")) {
    const JNode sh = n.Child("shape");
    if (spawns) {
      sh.Error("a spawn has no shape");
    } else if (d.kind == OnDeathKind::Burst) {
      d.shape = ReadAbilityShape(sh, true, AbilityShapeKind::Circle);
    } else {
      const AbilityShapeKind implied = d.kind == OnDeathKind::Ring ? AbilityShapeKind::Ring : AbilityShapeKind::Circle;
      d.shape = ReadAbilityShape(sh, !sh.Has("kind"), implied);
      if (d.shape.kind != AbilityShapeKind::Circle && d.shape.kind != AbilityShapeKind::Ring) {
        sh.Child("kind").Error("an on-death shape is a circle or a ring");
      }
    }
  } else if (!spawns) {
    n.Error("a " + std::string(EnumName(d.kind)) + " needs a shape");
  }
  // Damage: dmg or hpPct (not both); a burst needs one, a ring / puddle without one needs a status.
  if (n.Has("dmg")) {
    d.hasDmg = true;
    d.dmg = n.Num("dmg");
    if (!(d.dmg > 0)) n.Child("dmg").Error("expected > 0");
  }
  if (n.Has("hpPct")) {
    d.hasHpPct = true;
    d.hpPct = n.Num("hpPct");
    if (!(d.hpPct > 0) || !(d.hpPct <= 100)) n.Child("hpPct").Error("expected 0 < hpPct <= 100");
  }
  if (d.hasDmg && d.hasHpPct) n.Error("an on-death effect deals dmg or hpPct, not both");
  if (n.Has("damageType")) {
    d.hasDamageType = true;
    d.damageType = n.Enum<DamageType>("damageType");
  }
  if (n.Has("status")) d.status = ReadAbilityStatus(n.Child("status"));
  d.hitsMonsters = n.Bool("hitsMonsters", false);
  d.monsterDmgMul = n.Num("monsterDmgMul", 1.0);
  if (!(d.monsterDmgMul >= 0)) n.Child("monsterDmgMul").Error("expected >= 0");
  if (n.Has("capHeroMaxHpPct")) {
    d.capHeroMaxHpPct = n.Num("capHeroMaxHpPct");
    if (!(d.capHeroMaxHpPct > 0) || !(d.capHeroMaxHpPct <= 100)) {
      n.Child("capHeroMaxHpPct").Error("expected 0 < capHeroMaxHpPct <= 100 (percent of the hero's max life)");
    }
  }
  const bool damages = d.hasDmg || d.hasHpPct;
  switch (d.kind) {
    case OnDeathKind::Burst:
      if (!damages) n.Error("a burst deals damage: dmg or hpPct");
      if (!n.Has("capHeroMaxHpPct")) n.Error("an exploder's damage is capped: capHeroMaxHpPct (monsters.md 1.3)");
      break;
    case OnDeathKind::Ring:
    case OnDeathKind::Puddle:
      if (!damages && !d.status.present) n.Error("a " + std::string(EnumName(d.kind)) + " needs damage or a status");
      break;
    case OnDeathKind::Spawn:
      for (std::string_view k : {"dmg", "hpPct", "damageType", "status", "hitsMonsters", "monsterDmgMul",
                                 "capHeroMaxHpPct"}) {
        if (n.Has(k)) n.Child(k).Error("a spawn deals no damage");
      }
      break;
  }
  // Blocks: puddle {durationMs, tickMs}, spawn {monsterId, count}, each only for its kind.
  if (n.Has("puddle")) {
    const JNode p = n.Child("puddle");
    LoadMonsterCheckKeys(p, {"durationMs", "tickMs"}, "puddle");
    if (d.kind != OnDeathKind::Puddle) p.Error("only a puddle has a 'puddle' block");
    d.puddle.durationMs = p.Num("durationMs");
    d.puddle.tickMs = p.Num("tickMs");
    if (!(d.puddle.durationMs > 0) || !(d.puddle.durationMs <= kLoadHazardMaxDurationMs)) {
      p.Child("durationMs").Error("expected 0 < durationMs <= 60000");
    }
    if (!(d.puddle.tickMs >= kLoadHazardMinTickMs) || !(d.puddle.tickMs <= d.puddle.durationMs)) {
      p.Child("tickMs").Error("expected 100 <= tickMs <= durationMs");
    }
  } else if (d.kind == OnDeathKind::Puddle) {
    n.Error("a puddle needs its 'puddle' block");
  }
  if (n.Has("spawn")) {
    const JNode s = n.Child("spawn");
    LoadMonsterCheckKeys(s, {"monsterId", "count"}, "spawn");
    if (!spawns) s.Error("only a spawn has a 'spawn' block");
    d.spawn.monsterId = s.Str("monsterId");
    d.spawn.count = s.Int("count", 1);
    if (d.spawn.monsterId.empty()) s.Child("monsterId").Error("empty monster id");
    if (d.spawn.count < 1 || d.spawn.count > kLoadOnDeathMaxSpawn) s.Child("count").Error("expected 1 <= count <= 20");
  } else if (spawns) {
    n.Error("a spawn needs its 'spawn' block");
  }
  return d;
}

// K3 rise block (MonsterData.h MonsterRiseAgainDef).
MonsterRiseAgainDef ReadMonsterRiseAgain(const JNode& n) {
  MonsterRiseAgainDef r;
  LoadMonsterCheckKeys(n, {"chance", "delayMs", "hpPct", "notOnCrit"}, "riseAgain");
  r.present = true;
  r.chance = n.Num("chance", 1.0);
  r.delayMs = n.Num("delayMs", 0.0);
  r.hpPct = n.Num("hpPct", 100.0);
  r.notOnCrit = n.Bool("notOnCrit", false);
  if (!(r.chance > 0) || !(r.chance <= 1)) n.Child("chance").Error("expected 0 < chance <= 1");
  if (!(r.delayMs >= 0) || !(r.delayMs <= kLoadRiseMaxDelayMs)) n.Child("delayMs").Error("expected 0 <= delayMs <= 60000");
  if (!(r.hpPct > 0) || !(r.hpPct <= 100)) n.Child("hpPct").Error("expected 0 < hpPct <= 100");
  return r;
}

MonsterDef ReadMonsterDef(const JNode& n, bool withDerived) {
  MonsterDef m;
  m.id = n.Str("id");
  m.name = n.Str("name");
  m.level = n.Int("level");
  m.hp = n.Num("hp");
  m.damage = n.Num("damage");
  m.defense = n.Num("defense");
  m.speed = n.Num("speed");
  m.aggroRange = n.Num("aggroRange");
  m.attackRange = n.Num("attackRange");
  m.attackSpeedMs = n.Num("attackSpeed");
  m.expReward = n.Num("expReward");
  const std::vector<double> gold = n.NumList("goldReward");
  if (gold.size() == 2) {
    m.goldMin = gold[0];
    m.goldMax = gold[1];
  } else {
    n.Child("goldReward").Error("expected [min, max]");
  }
  m.spriteKey = n.Str("spriteKey", "");
  m.elite = n.Bool("elite", false);
  m.isMiniBoss = n.Bool("isMiniBoss", false);
  m.isSubDungeonMiniBoss = n.Bool("isSubDungeonMiniBoss", false);
  m.animCategory = n.Enum<AnimRig>("animCategory", AnimRig::Humanoid);
  m.isRanged = m.attackRange > 2.5;
  m.nameKey = "data.monster." + m.id;
  // Dark remake K2 (monster.damageType, monster.hitCap, monster.abilities) and the web's bossSkills (M9).
  m.damageType = n.Enum<DamageType>("damageType", DamageType::Physical);
  if (n.Has("hitCapHeroMaxHpPct")) {
    m.hitCapHeroMaxHpPct = n.Num("hitCapHeroMaxHpPct");
    if (!(m.hitCapHeroMaxHpPct > 0) || !(m.hitCapHeroMaxHpPct <= 100)) {
      n.Child("hitCapHeroMaxHpPct").Error("expected 0 < hitCapHeroMaxHpPct <= 100 (percent of the hero's max life)");
    }
  }
  for (const JNode& a : n.Items("abilities", true)) {
    MonsterAbilityDef ability = ReadMonsterAbility(a);
    for (const MonsterAbilityDef& other : m.abilities) {
      if (other.id == ability.id) a.Child("id").Error("duplicate ability id '" + ability.id + "'");
    }
    m.abilities.push_back(std::move(ability));
  }
  m.bossSkills = n.StrList("bossSkills", true);
  // Dark remake K3 (monster.family, monster.onDeath, monster.corpse, monster.riseAgain).
  if (n.Has("family")) {
    m.family = n.Str("family");
    if (m.family.empty()) n.Child("family").Error("an empty family id");
  }
  if (n.Has("onDeath")) m.onDeath = ReadMonsterOnDeath(n.Child("onDeath"));
  if (n.Has("corpse")) {
    const JNode c = n.Child("corpse");
    LoadMonsterCheckKeys(c, {"leaves", "consumes"}, "corpse");
    m.corpse.leaves = c.Enum<CorpseLeaves>("leaves");
    m.corpse.consumes = c.Bool("consumes", false);
  }
  if (n.Has("riseAgain")) {
    m.riseAgain = ReadMonsterRiseAgain(n.Child("riseAgain"));
    if (m.corpse.leaves != CorpseLeaves::Corpse) {
      n.Child("riseAgain").Error("a monster rises from its own corpse: corpse.leaves must be 'corpse'");
    }
  }
  // Dark remake K4 (monster.phases): thresholds strictly decreasing; switched abilities are the monster's own.
  for (const JNode& pn : n.Items("phases", true)) {
    MonsterPhaseDef phase = ReadMonsterPhase(pn);
    if (!m.phases.empty() && !(phase.hpBelowPct < m.phases.back().hpBelowPct)) {
      pn.Child("hpBelowPct").Error("phase thresholds strictly decrease (each row is entered below the previous one)");
    }
    const auto known = [&](const std::string& id) {
      for (const MonsterAbilityDef& a : m.abilities) {
        if (a.id == id) return true;
      }
      return false;
    };
    for (const std::string& id : phase.abilitiesOn) {
      if (!known(id)) pn.Child("abilitiesOn").Error("'" + id + "' names no ability of the monster");
    }
    for (const std::string& id : phase.abilitiesOff) {
      if (!known(id)) pn.Child("abilitiesOff").Error("'" + id + "' names no ability of the monster");
    }
    m.phases.push_back(std::move(phase));
  }
  // Dark remake K5 (monster.packAggro, monster.kite, monster.block, monster.morale, monster.aura).
  ReadMonsterK5(n, m);
  if (withDerived) {
    const JNode d = n.Child("derived");
    if (!d.Exists()) {
      n.Error("missing exporter 'derived' block");
      return m;
    }
    m.isRanged = d.Bool("isRanged");
    for (const JNode& s : d.Items("onHitStatus", true)) m.onHitStatus.push_back(ReadStatusRule(s));
    m.projectileColor = d.Color("projectileColor");
    m.homeZone = d.Str("homeZone", "");
    m.nameKey = d.Str("nameKey");
    m.sources = d.EnumList<MonsterSource>("sources");
  }
  return m;
}

DialogueTree ReadDialogueTree(const JNode& n, std::string id, DialogueKind kind) {
  DialogueTree t;
  t.id = std::move(id);
  t.kind = kind;
  t.startNodeId = n.Str("startNodeId");
  for (const auto& [nodeId, nn] : n.Members("nodes")) {
    DialogueNode node;
    node.id = nn.Str("id", nodeId);
    if (node.id != nodeId) nn.Child("id").Error("node id differs from its key");
    node.text = nn.Str("text");
    for (const JNode& c : nn.Items("choices", true)) {
      DialogueChoice ch;
      ch.text = c.Str("text");
      ch.nextNodeId = c.Str("nextNodeId");
      ch.questTrigger = c.Str("questTrigger", "");
      ch.prereqQuests = c.StrList("prereqQuests", true);
      if (c.Has("reward")) {
        const JNode rw = c.Child("reward");
        ch.reward.present = true;
        ch.reward.gold = rw.I64("gold", 0);
        ch.reward.exp = rw.I64("exp", 0);
        ch.reward.items = rw.StrList("items", true);
      }
      node.choices.push_back(std::move(ch));
    }
    node.nextNodeId = nn.Str("nextNodeId", "");
    node.isEnd = nn.Bool("isEnd", false);
    t.nodes.push_back(std::move(node));
  }
  return t;
}

void LoadMonstersFile(const JNode& r, MonsterTables& out) {
  out.defs.clear();
  out.defIndex.Clear();
  out.byZone.clear();
  out.overrides.clear();
  out.lookupOrder = r.StrList("lookupOrder");
  for (const auto& [id, n] : r.Members("defs")) {
    MonsterDef m = ReadMonsterDef(n, true);
    if (m.id != id) n.Child("id").Error("def id differs from its key");
    if (!out.defIndex.Add(m.id)) n.Error("duplicate monster id '" + m.id + "'");
    out.defs.push_back(std::move(m));
  }
  for (const auto& [zone, n] : r.Members("byZone")) {
    out.byZone.push_back(ZoneMonsterList{zone, r.Child("byZone").StrList(zone)});
  }
  const JNode d = r.Child("dungeon");
  out.dungeon.exclusive = d.StrList("exclusive");
  out.dungeon.boss = d.Str("boss");
  out.dungeon.midBoss = d.Str("midBoss");
  out.dungeon.pool = d.StrList("pool");
  for (const JNode& o : r.Items("overrides", true)) {
    MonsterOverride mo;
    mo.id = o.Str("id");
    mo.field = o.Str("field");
    mo.web = o.Num("web");
    mo.port = o.Num("port");
    mo.decision = o.Str("decision", "");
    out.overrides.push_back(std::move(mo));
  }
}

void LoadMiniBossesFile(const JNode& r, MonsterTables& out) {
  out.miniBosses.clear();
  out.miniBossDialogues.clear();
  for (const auto& [zone, n] : r.Members("byZone")) {
    MiniBossEntry e;
    e.zoneId = zone;
    e.monsterId = n.AsStr();
    const JNode sp = r.Child("spawns").Child(zone);
    if (sp.Exists()) {
      e.hasSpawn = true;
      e.spawn = sp.AsTile();
    }
    out.miniBosses.push_back(std::move(e));
  }
  out.subDungeonBosses = r.StrList("subDungeon");
  for (const auto& [id, n] : r.Members("dialogues")) {
    out.miniBossDialogues.push_back(ReadDialogueTree(n, id, DialogueKind::MiniBoss));
  }
}

void LoadQuestHuntsFile(const JNode& r, MonsterTables& out) {
  out.hunts.clear();
  const JNode defs = r.Child("defsNormal");
  for (const JNode& n : r.Items("hunts")) {
    HuntDef h;
    h.questId = n.Str("questId");
    h.zone = n.Str("zone");
    h.huntId = n.Str("huntId");
    h.monsterId = n.Str("monsterId");
    h.name = n.Str("name");
    h.spawn = TilePos{n.Int("col"), n.Int("row")};
    if (n.Has("hpMul")) {
      h.hasHpMul = true;
      h.hpMul = n.Num("hpMul");
    }
    if (n.Has("dmgMul")) {
      h.hasDmgMul = true;
      h.dmgMul = n.Num("dmgMul");
    }
    h.revealAfterPrevious = n.Bool("revealAfterPrevious", false);
    if (n.Has("minions")) {
      h.hasMinions = true;
      h.minionMonsterId = n.Child("minions").Str("monsterId");
      h.minionCount = n.Child("minions").Int("count");
    }
    const JNode def = defs.Child(h.huntId);
    if (def.Exists()) {
      h.defNormal = ReadMonsterDef(def, false);
      h.defNormal.nameKey = "data.monster." + h.huntId;
      h.defNormal.sources.assign(1, MonsterSource::Hunt);
    } else {
      n.Error("no defsNormal entry for hunt '" + h.huntId + "'");
    }
    out.hunts.push_back(std::move(h));
  }
}

void LoadMonsterAiFile(const JNode& r, MonsterTables& out) {
  MonsterAiDef& a = out.ai;
  a.leashRange = r.Num("leashRange");
  a.leashHealFractionPerTick = r.Num("leashHealFractionPerTick");
  a.patrolIntervalMs = r.Num("patrolIntervalMs");
  a.patrolRadius = r.Int("patrolRadius");
  a.arriveEpsilon = r.Num("arriveEpsilon");
  a.chaseDropMul = r.Num("chaseDropMul");
  a.attackExitMul = r.Num("attackExitMul");
  a.moveSpeedScale = r.Num("moveSpeedScale");
  a.moveAccel = r.Num("moveAccel");
  a.rangedThreshold = r.Num("rangedThreshold");
  a.meleeReachMul = r.Num("meleeReachMul");
  a.meleeReachAdd = r.Num("meleeReachAdd");
  a.swingQueryRadius = r.Num("swingQueryRadius");
  a.aiCullRadius = r.Num("aiCullRadius");
  a.activeRefreshMs = r.Num("activeRefreshMs");
  a.spawnJitter = r.Int("spawnJitter");
  a.respawnDelayMs = r.Num("respawnDelayMs");
  a.respawnJitter = r.Int("respawnJitter");
  a.safeZoneRadiusDefault = r.Num("safeZoneRadiusDefault");
  const JNode s = r.Child("monsterStats");
  a.strPerDamage = s.Num("strPerDamage");
  a.dexPerSpeed = s.Num("dexPerSpeed");
  a.vitPerHp = s.Num("vitPerHp");
  a.statInt = s.Int("int");
  a.statSpi = s.Int("spi");
  a.statLck = s.Int("lck");
  const JNode h = r.Child("hunt");
  a.huntHpMul = h.Num("hpMul");
  a.huntDmgMul = h.Num("dmgMul");
  a.huntDefMul = h.Num("defMul");
  a.huntExpMulMin = h.Num("expMulMin");
  a.huntGoldMul = h.Num("goldMul");
  a.huntAggroMin = h.Num("aggroMin");
  a.huntMinionJitter = h.Int("minionJitter");
  a.huntSpotSearchRadius = h.Int("spotSearchRadius");
  a.huntVisualScale = h.Num("visualScale");
  a.huntRevealShakeMs = h.Num("revealShakeMs");
  a.huntRevealShakeIntensity = h.Num("revealShakeIntensity");
  a.bossSightRange = r.Child("boss").Num("sightRange");
  a.bossBarRange = r.Child("boss").Num("barRange");
  a.finalBoss = r.Child("boss").Str("finalBoss");
  a.escortChipRadiusSq = r.Child("escortChip").Num("radiusSq");
  a.escortChipIntervalMs = r.Child("escortChip").Num("intervalMs");
  a.escortChipDamageMul = r.Child("escortChip").Num("damageMul");
  a.defendChipRadiusSq = r.Child("defendChip").Num("radiusSq");
  a.defendChipIntervalMs = r.Child("defendChip").Num("intervalMs");
  a.defendChipDamageMul = r.Child("defendChip").Num("damageMul");
  const JNode p = r.Child("port");
  a.leashMode = p.Child("leash").Enum<LeashMode>("mode");
  a.leashHealFractionPerSecond = p.Child("leash").Num("healFractionPerSecond");
  a.provokeDurationMs = p.Child("provoke").Num("durationMs");
  a.noRespawnRoles = p.StrList("noRespawn");
  a.miniBossOncePerVisit = p.Bool("miniBossOncePerVisit");
  const JNode sb = p.Child("storyBoss");
  a.storyBossId = sb.Str("monsterId");
  a.storyBossOncePerVisit = sb.Bool("oncePerVisit");
  a.storyBossNotAfterQuestTurnIn = sb.Str("notAfterQuestTurnIn");
  a.storyBossFarmableAfterChapter = sb.Bool("farmableAfterChapter");
  a.chapterCompleteQuest = sb.Str("chapterCompleteQuest");
  for (const std::string& role : a.noRespawnRoles) {
    MonsterRole parsed{};
    if (!ParseEnum(role, parsed)) p.Child("noRespawn").Error("unknown monster role '" + role + "'");
  }
  a.patrolTimeoutMs = p.Num("patrolTimeoutMs");
  a.placementTries = p.Int("placementTries");
  a.yawTurnRateDegPerSec = p.Num("yawTurnRateDegPerSec");
}

}  // namespace abyss::dataload

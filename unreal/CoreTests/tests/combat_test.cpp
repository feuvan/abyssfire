// Combat area: damage, statuses, hit feedback, input, targeting, projectiles, CombatSystem, soul echo.
// Vectors: combat-feel.md section 24, classes-stats-skills.md 12.3 / 22 (status DR, Q21), plus ports of the web suites
// HitFeedback.test.ts, CombatInputSystem.test.ts, SoulEcho.test.ts, StatusEffectSystem.test.ts,
// CombatSystemBugs.test.ts and the damage parts of skill-combat-integration.test.ts.
//
// Runtime tests (CombatRig) build the real systems around a SimHarness. They need MonsterSystem::Spawn / ApplyDamage
// (monsters area); while those are stubs the tests that need a monster report it with MESSAGE and return early.
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "SimHarness.h"
#include "abyss/base/Math.h"
#include "abyss/base/Units.h"
#include "abyss/combat/Combat.h"
#include "abyss/combat/CombatInput.h"
#include "abyss/combat/Damage.h"
#include "abyss/combat/HitFeedback.h"
#include "abyss/combat/Projectiles.h"
#include "abyss/combat/SkillTargeting.h"
#include "abyss/combat/SoulEcho.h"
#include "abyss/combat/StatusEffects.h"
#include "abyss/hero/Hero.h"
#include "abyss/hero/Rewards.h"
#include "abyss/monsters/MonsterSystem.h"
#include "abyss/save/SaveData.h"
#include "abyss/sim/GameSim.h"
#include "abyss/story/StoryDirector.h"
#include "abyss/world/Locomotion.h"
#include "abyss/world/Zone.h"
#include "doctest/doctest.h"

using namespace abyss;

namespace {

const DataStore& D() { return test::RealData(); }

DamageRules Rules() {
  const ClassTables& c = D().Classes();
  return DamageRules{&c.formulas, &c.buffCaps, &c.skillRules};
}

// A combatant that owns its buff list and equipment bag (the Combatant points into them; never copy a Fighter).
struct Fighter {
  Combatant c;
  BuffList buffs;
  EquipStats eq;
  Fighter() { c.buffs = &buffs; }
  Fighter(const Fighter&) = delete;
  Fighter& operator=(const Fighter&) = delete;
  void UseEq() { c.eq = &eq; }
  void Buff(BuffStat s, double v) {
    ActiveBuff b;
    b.stat = s;
    b.value = v;
    b.durationMs = 60000;
    buffs.Add(b);
  }
};

// Hero combatant with base stats (no gear, no allocated points), classes 3 / 2.4.
void MakeHero(Fighter& f, ClassId cls, int32_t level = 1) {
  const ClassDef* cd = D().Classes().Find(cls);
  REQUIRE(cd != nullptr);
  const PrimaryStats s = cd->baseStats;
  f.c.stats = s;
  f.c.baseDamage = 8 + s.str * 0.8 + level * 2;
  f.c.defense = 3 + s.vit * 0.5 + level;
  f.c.maxHp = 50 + s.vit * 10 + (level - 1) * 15;
  f.c.mana = 30 + s.spi * 8 + s.int_ * 3 + (level - 1) * 8;
}

// Monster combatant from a def (monsters 1.2).
void MakeMonster(Fighter& f, std::string_view id, double damageOverride = -1) {
  const MonsterDef* d = D().FindMonster(id);
  REQUIRE(d != nullptr);
  const double dmg = damageOverride >= 0 ? damageOverride : d->damage;
  f.c.stats = PrimaryStats{static_cast<int32_t>(std::floor(dmg * 0.8)),
                           static_cast<int32_t>(std::floor(d->speed * 0.1)),
                           static_cast<int32_t>(std::floor(d->hp * 0.1)),
                           3,
                           3,
                           3};
  f.c.baseDamage = dmg;
  f.c.defense = d->defense;
  f.c.maxHp = d->hp;
}

const SkillDef& Skill(std::string_view id) {
  const SkillDef* s = D().FindSkill(id);
  REQUIRE(s != nullptr);
  return *s;
}

// RNG scripted for "no dodge, no crit" (0.99, 0.99) or "no dodge, crit" (0.99, 0.0) - combat 24.
DamageResult Hit(const Fighter& a, const Fighter& d, const SkillDef* s = nullptr, int32_t level = 1, double synergy = 1,
                 bool crit = false) {
  Rng rng(7);
  rng.Script({0.99, crit ? 0.0 : 0.99});
  SkillHitInput in;
  in.skill = s;
  in.level = level;
  in.synergyFactor = synergy;
  return CalculateDamage(Rules(), a.c, d.c, in, false, rng);
}

std::vector<StatusTick> TickStatuses(StatusEffectSystem& s, double now, std::vector<StatusExpiry>* expired = nullptr) {
  std::vector<StatusTick> ticks;
  std::vector<StatusExpiry> exp;
  s.Tick(now, ticks, exp);
  if (expired != nullptr) *expired = exp;
  return ticks;
}

int32_t TickCount(const std::vector<StatusTick>& ticks) {
  int32_t n = 0;
  for (const StatusTick& t : ticks) n += t.ticks;
  return n;
}

bool ContainsExpiry(const std::vector<StatusExpiry>& e, StatusType t) {
  for (const StatusExpiry& x : e) {
    if (x.type == t) return true;
  }
  return false;
}

template <class E>
std::vector<E> EventsOf(const EventSink& sink) {
  std::vector<E> out;
  for (const Event& e : sink.Items()) {
    if (const E* p = std::get_if<E>(&e)) out.push_back(*p);
  }
  return out;
}

// ---------------------------------------------------------------------------------------------------------------------
// Runtime rig: the real combat systems around a SimHarness, stepped in the GameSim order (classes 16).
// ---------------------------------------------------------------------------------------------------------------------
struct CombatRig {
  explicit CombatRig(ClassId cls, uint64_t seed = 3)
      : h(seed),
        hero(std::make_unique<Hero>(h.ctx.data, cls)),
        status(h.ctx.data.Classes().statusRules),
        zone(h.ctx),
        loco(h.ctx),
        monsters(h.ctx),
        projectiles(h.ctx),
        combat(h.ctx),
        soul(h.ctx),
        rewards(h.ctx) {
    SimSystems& s = h.ctx.sys;
    s.hero = hero.get();
    s.status = &status;
    s.zone = &zone;
    s.locomotion = &loco;
    s.monsters = &monsters;
    s.projectiles = &projectiles;
    s.combat = &combat;
    s.soulEcho = &soul;
    s.rewards = &rewards;
    h.onTimer = [this](const Timer& t) {
      switch (t.owner) {
        case TimerOwner::Combat: combat.OnTimer(t); break;
        case TimerOwner::Projectiles: projectiles.OnTimer(t); break;
        case TimerOwner::Monsters: monsters.OnTimer(t); break;
        default: break;
      }
    };
    h.bus.Subscribe<MonsterKilledMsg>([this](const MonsterKilledMsg& m) { combat.OnMonsterKilled(m); });
    h.session.currentMap = D().World().defaultMap;
    ok = zone.EnterZone(D().World().defaultMap, true, Vec2(40, 40));
    hero->Skills().InitStarterLevels();
    hero->RecalcDerived(h.ctx.equip);
    hero->FillHpMana();
    loco.OnZoneEnter();
    combat.OnZoneEnter();
    h.events.Clear();
  }

  // One fixed step in the GameSim order (the parts the combat area drives).
  void Step(int n = 1) {
    for (int i = 0; i < n; ++i) {
      h.Step(1);
      combat.ConsumeBufferedSkill();
      hero->RecalcDerived(h.ctx.equip);
      combat.TickPassives(kSimStepMs);
      loco.Tick(kSimStepMs);
      combat.TickHeroUpdate(kSimStepMs);
      combat.TickCombat();
      combat.TickStatusEffects();
      combat.TickCombatState();
      combat.TickAutoCombat();
      soul.Tick();
    }
  }
  // Steps until the sim clock reaches `ms` (absolute).
  void StepTo(double ms) {
    while (h.clock.NowMs() + 1e-9 < ms) Step();
  }
  // Steps while the NEXT step would still be before `ms` (the clock ends on the last step before it).
  void StepBefore(double ms) {
    while (h.clock.NowMs() + kSimStepMs + 1e-9 < ms) Step();
  }
  // Steps until `pred` holds (at most `maxSteps`); returns whether it did.
  template <class Pred>
  bool StepUntil(Pred pred, int maxSteps = 600) {
    for (int i = 0; i < maxSteps; ++i) {
      if (pred()) return true;
      Step();
    }
    return pred();
  }
  // "no dodge, no crit" for the next calculateDamage.
  void ScriptNoDodge() { h.rng.Get(RngStream::Combat).Script({0.99, 0.99}); }

  // Spawns a normal-difficulty monster at a tile (kNoEntity while MonsterSystem::Spawn is a stub).
  EntityId Spawn(std::string_view defId, Vec2 tile, bool noDodge = true) {
    MonsterSpawnParams p;
    p.baseDef = D().FindMonster(defId);
    p.tile = RoundToTile(tile);
    p.role = MonsterRole::Regular;
    const EntityId id = monsters.Spawn(p);
    if (MonsterInstance* m = monsters.Find(id)) {
      m->pos = tile;
      if (noDodge) m->stats.dex = 0;  // stat dodge 0 %: deterministic hits
    }
    return id;
  }

  int32_t SkillIndex(std::string_view id) const { return hero->Skills().IndexOf(id); }

  test::SimHarness h;
  std::unique_ptr<Hero> hero;
  StatusEffectSystem status;
  ZoneRuntime zone;
  HeroLocomotion loco;
  MonsterSystem monsters;
  ProjectileSystem projectiles;
  CombatSystem combat;
  SoulEchoSystem soul;
  RewardService rewards;
  bool ok = false;
};

#define REQUIRE_MONSTER(id)                                                                     \
  if ((id) == kNoEntity) {                                                                      \
    MESSAGE("MonsterSystem::Spawn is not implemented yet (monsters area): runtime part skipped"); \
    return;                                                                                     \
  }

}  // namespace

TEST_SUITE("combat") {
  // ===================================================================================================================
  // Damage formula (combat 2, classes 12)
  // ===================================================================================================================
  TEST_CASE("damage 2.1: warrior basic attacks and goblin hits (combat-feel worked examples)") {
    Fighter w, slime, goblin;
    MakeHero(w, ClassId::Warrior);
    CHECK(w.c.baseDamage == doctest::Approx(19.6));
    CHECK(w.c.defense == doctest::Approx(9));
    MakeMonster(slime, "slime_green");
    MakeMonster(goblin, "goblin");
    CHECK(Hit(w, slime).damage == 24);
    CHECK(Hit(w, slime, nullptr, 1, 1, true).damage == 38);
    CHECK(Hit(w, slime, nullptr, 1, 1, true).isCrit);
    CHECK(Hit(w, goblin).damage == 23);
    // goblin (dmg 8 -> str 6) hits the warrior: 11 - 4.5 -> 6; with shield_wall DR 0.5 -> 3.
    CHECK(Hit(goblin, w).damage == 6);
    w.Buff(BuffStat::DamageReduction, 0.5);
    CHECK(Hit(goblin, w).damage == 3);
    Fighter w2, chief, strong;
    MakeHero(w2, ClassId::Warrior);
    MakeMonster(chief, "goblin_chief");
    CHECK(Hit(chief, w2).damage == 15);
    MakeMonster(strong, "goblin_chief", 18);  // extra_strong: dmg floor(14 * 1.35) = 18, str 14
    CHECK(Hit(strong, w2).damage == 20);
  }

  TEST_CASE("damage 12.3: skill vectors with scripted rolls") {
    Fighter goblin;
    MakeMonster(goblin, "goblin");
    Fighter w, m, r;
    MakeHero(w, ClassId::Warrior);
    MakeHero(m, ClassId::Mage);
    MakeHero(r, ClassId::Rogue);
    CHECK(Hit(w, goblin, &Skill("slash")).damage == 36);
    CHECK(Hit(m, goblin, &Skill("fireball")).damage == 34);
    CHECK(Hit(m, goblin, &Skill("fireball"), 1, 1, true).damage == 54);
    CHECK(Hit(m, goblin, &Skill("fireball")).type == DamageType::Fire);
    CHECK(Hit(r, goblin, &Skill("backstab")).damage == 36);
    CHECK(Hit(r, goblin, &Skill("backstab"), 1, 1, true).damage == 58);
    CHECK(Hit(r, goblin, &Skill("explosive_trap")).damage == 25);  // fire: the int stat bonus
    // Resonance (mage outgoing 1.2).
    m.c.outgoingMultiplier = 1.2;
    CHECK(Hit(m, goblin, &Skill("fireball")).damage == 41);
    // Warrior L5 slash L5 with whirlwind L3 (synergy 1 + 0.08 * 3).
    Fighter w5;
    MakeHero(w5, ClassId::Warrior, 5);
    CHECK(Hit(w5, goblin, &Skill("slash"), 5, 1.24).damage == 90);
  }

  TEST_CASE("damage 12.3: monster crit, mana shield, poison blade + vanish, death mark, dodge") {
    Fighter goblin;
    MakeMonster(goblin, "goblin");
    Fighter w;
    MakeHero(w, ClassId::Warrior);
    CHECK(Hit(goblin, w, nullptr, 1, 1, true).damage == 12);
    // Mana shield L1 (0.3) on a mage with 152 mana: 7 -> 5 HP, 2 mana.
    Fighter m;
    MakeHero(m, ClassId::Mage);
    CHECK(m.c.mana == 152);
    m.Buff(BuffStat::ManaShield, 0.3);
    const DamageResult ms = Hit(goblin, m);
    CHECK(ms.damage == 5);
    CHECK(ms.manaDamage == 2);
    // A fractional pool (regen) absorbs min(floor(7 x 0.3), D.mana) = 1.5: 5.5 HP (step 17, "D.mana may be fractional").
    m.c.mana = 1.5;
    const DamageResult frac = Hit(goblin, m);
    CHECK(frac.manaDamage == doctest::Approx(1.5));
    CHECK(frac.damage == doctest::Approx(5.5));
    // Rogue basic with poison_blade L1 (0.5) + vanish L1 (1.0): 49.
    Fighter r;
    MakeHero(r, ClassId::Rogue);
    r.Buff(BuffStat::PoisonDamage, 0.5);
    r.Buff(BuffStat::StealthDamage, 1.0);
    CHECK(Hit(r, goblin).damage == 49);
    // Warrior basic vs a death-marked goblin (amplify 0.25): 29.
    goblin.Buff(BuffStat::DamageAmplify, 0.25);
    CHECK(Hit(w, goblin).damage == 29);
    // First roll 0.0: dodged (any non-zero dodge chance).
    Rng rng(1);
    rng.Script({0.0, 0.99});
    const DamageResult d = CalculateDamage(Rules(), w.c, goblin.c, SkillHitInput{}, false, rng);
    CHECK(d.isDodged);
    CHECK(d.damage == 0);
    CHECK(rng.ScriptedRemaining() == 1);  // a dodge returns before the crit draw
  }

  TEST_CASE("damage: draw order - dodge always, crit skipped when forced") {
    Fighter w, goblin;
    MakeHero(w, ClassId::Warrior);
    MakeMonster(goblin, "goblin");
    Rng rng(1);
    rng.Script({0.99, 0.99});
    const DamageResult forced = CalculateDamage(Rules(), w.c, goblin.c, SkillHitInput{}, true, rng);
    CHECK(forced.isCrit);
    CHECK(rng.ScriptedRemaining() == 1);
    // Chance 0 still consumes the dodge draw.
    goblin.c.stats.dex = 0;
    rng.ClearScript();
    rng.Script({0.0, 0.99});
    const DamageResult hit = CalculateDamage(Rules(), w.c, goblin.c, SkillHitInput{}, false, rng);
    CHECK_FALSE(hit.isDodged);
    CHECK(rng.ScriptedRemaining() == 0);
  }

  TEST_CASE("damage: gear terms - flat / percent damage, elemental flat, ignore defense, resist, steal") {
    Fighter a, d;
    MakeHero(a, ClassId::Warrior);
    MakeMonster(d, "goblin");
    a.UseEq();
    a.eq.Ref(Stat::Damage) = 10;          // base 25.6 + 10
    a.eq.Ref(Stat::DamagePercent) = 50;   // x 1.5 -> 53.4
    a.eq.Ref(Stat::FireDamage) = 4;       // + 4 (not crit-multiplied)
    a.eq.Ref(Stat::ElementalDamagePercent) = 50;  // elem 6
    a.eq.Ref(Stat::IgnoreDefense) = 50;   // def 4 -> 2
    a.eq.Ref(Stat::LifeSteal) = 10;
    a.eq.Ref(Stat::ManaSteal) = 5;
    const DamageResult r = Hit(a, d);
    // (53.4 + 6) - 2 * 0.5 = 58.4 -> 58
    CHECK(r.damage == 58);
    CHECK(r.lifeStolen == 5);
    CHECK(r.manaStolen == 2);
    // Resistance: monsters never resist; a hero defender with fire resist 20 + all 10 takes 30 % less fire.
    Fighter mage, hero;
    MakeHero(mage, ClassId::Mage);
    MakeHero(hero, ClassId::Warrior);
    hero.UseEq();
    const int32_t plain = Hit(mage, hero, &Skill("fireball")).damage;
    hero.eq.Ref(Stat::FireResist) = 20;
    hero.eq.Ref(Stat::AllResist) = 10;
    const int32_t resisted = Hit(mage, hero, &Skill("fireball")).damage;
    CHECK(resisted < plain);
    // Physical skills ignore elemental resist.
    Fighter w;
    MakeHero(w, ClassId::Warrior);
    const int32_t phys = Hit(w, hero, &Skill("slash")).damage;
    hero.eq = EquipStats{};
    CHECK(Hit(w, hero, &Skill("slash")).damage == phys);
    // Resist cap 75 %.
    hero.eq.Ref(Stat::AllResist) = 200;
    Fighter big;
    MakeHero(big, ClassId::Mage);
    big.c.baseDamage = 1000;
    const int32_t capped = Hit(big, hero, &Skill("fireball")).damage;
    hero.eq.Ref(Stat::AllResist) = 75;
    CHECK(Hit(big, hero, &Skill("fireball")).damage == capped);
  }

  TEST_CASE("damage: arcane uses allResist only; ice / poison use their own resist") {
    Fighter m, r, def;
    MakeHero(m, ClassId::Mage);
    MakeHero(r, ClassId::Rogue);
    MakeHero(def, ClassId::Warrior);
    def.UseEq();
    const int32_t arcane = Hit(m, def, &Skill("arcane_torrent")).damage;
    const int32_t ice = Hit(m, def, &Skill("ice_arrow")).damage;
    const int32_t poison = Hit(r, def, &Skill("poison_arrow")).damage;
    def.eq.Ref(Stat::IceResist) = 50;
    def.eq.Ref(Stat::PoisonResist) = 50;
    def.eq.Ref(Stat::LightningResist) = 50;
    CHECK(Hit(m, def, &Skill("arcane_torrent")).damage == arcane);
    CHECK(Hit(m, def, &Skill("ice_arrow")).damage < ice);
    CHECK(Hit(r, def, &Skill("poison_arrow")).damage < poison);
    def.eq.Ref(Stat::AllResist) = 50;
    CHECK(Hit(m, def, &Skill("arcane_torrent")).damage < arcane);
  }

  TEST_CASE("damage: buffs (CombatSystemBugs.test.ts) - poison, stealth, defense bonus, damage bonus, DR, caps") {
    Fighter base, atk, def;
    base.c.baseDamage = 100;
    base.c.stats = PrimaryStats{10, 0, 10, 10, 10, 0};
    atk.c.baseDamage = 100;
    atk.c.stats = base.c.stats;
    def.c.stats = PrimaryStats{10, 0, 10, 10, 10, 0};
    const int32_t plain = Hit(base, def).damage;
    atk.Buff(BuffStat::PoisonDamage, 0.5);
    CHECK(Hit(atk, def).damage > plain);
    Fighter stealth;
    stealth.c.baseDamage = 100;
    stealth.c.stats = base.c.stats;
    stealth.Buff(BuffStat::StealthDamage, 1.0);
    CHECK(Hit(stealth, def).damage >= plain * 1.9);
    Fighter bonus;
    bonus.c.baseDamage = 100;
    bonus.c.stats = base.c.stats;
    bonus.Buff(BuffStat::DamageBonus, 0.25);
    CHECK(Hit(bonus, def).damage >= std::floor(plain * 1.24));
    // damageBonus + stealth multiply: >= 2.4x.
    bonus.Buff(BuffStat::StealthDamage, 1.0);
    CHECK(Hit(bonus, def).damage >= std::floor(plain * 2.4));
    // defenseBonus raises effective defense.
    Fighter armored, armoredBuff;
    armored.c.stats = def.c.stats;
    armored.c.defense = 20;
    armoredBuff.c.stats = def.c.stats;
    armoredBuff.c.defense = 20;
    armoredBuff.Buff(BuffStat::DefenseBonus, 0.3);
    CHECK(Hit(base, armoredBuff).damage < Hit(base, armored).damage);
    // DR 0.5 roughly halves; buff DR caps at 0.9.
    Fighter dr;
    dr.c.stats = def.c.stats;
    dr.Buff(BuffStat::DamageReduction, 0.5);
    CHECK(Hit(base, dr).damage <= std::ceil(plain * 0.55));
    dr.Buff(BuffStat::DamageReduction, 0.5);
    CHECK(dr.buffs.Value(BuffStat::DamageReduction, D().Classes().buffCaps) == doctest::Approx(0.9));
    // Gear DR adds to the buff DR inside the same 0.9 cap.
    Fighter gear;
    gear.c.stats = def.c.stats;
    gear.UseEq();
    gear.eq.Ref(Stat::DamageReduction) = 50;
    gear.Buff(BuffStat::DamageReduction, 0.5);
    CHECK(Hit(base, gear).damage == Hit(base, dr).damage);
    // Expired (pruned) buffs stop affecting combat.
    Fighter expiring;
    expiring.c.stats = def.c.stats;
    ActiveBuff b;
    b.stat = BuffStat::DamageReduction;
    b.value = 0.5;
    b.durationMs = 5000;
    expiring.buffs.Add(b);
    const int32_t reduced = Hit(base, expiring).damage;
    expiring.buffs.Prune(6000);
    CHECK(Hit(base, expiring).damage > reduced);
  }

  TEST_CASE("damage: mana shield (CombatSystemBugs.test.ts)") {
    Fighter atk;
    atk.c.baseDamage = 100;
    atk.c.stats = PrimaryStats{10, 0, 10, 10, 10, 0};
    Fighter shielded;
    shielded.c.stats = PrimaryStats{10, 0, 10, 10, 10, 0};
    shielded.c.mana = 100;
    shielded.Buff(BuffStat::ManaShield, 0.3);
    Fighter bare;
    bare.c.stats = shielded.c.stats;
    bare.c.mana = 100;
    const DamageResult r = Hit(atk, shielded);
    CHECK(r.manaDamage > 0);
    CHECK(r.damage < Hit(atk, bare).damage);
    // Overflow: only the mana there is can absorb.
    Fighter low;
    low.c.stats = shielded.c.stats;
    low.c.mana = 10;
    low.Buff(BuffStat::ManaShield, 0.3);
    atk.c.baseDamage = 200;
    const DamageResult o = Hit(atk, low);
    CHECK(o.manaDamage <= 10);
    CHECK(o.damage > 0);
    // No mana: no absorb.
    low.c.mana = 0;
    CHECK(Hit(atk, low).manaDamage == 0);
  }

  TEST_CASE("damage: every one of the 40 skills deals positive damage through the formula (skill-combat-integration)") {
    Fighter def;
    MakeMonster(def, "goblin");
    int32_t count = 0;
    for (const ClassDef& c : D().Classes().classes) {
      Fighter hero;
      MakeHero(hero, c.cls);
      for (const SkillDef& s : c.skills) {
        ++count;
        CHECK_MESSAGE(Hit(hero, def, &s).damage >= 1, s.id);
      }
    }
    CHECK(count == 40);
  }

  TEST_CASE("damage: proc helpers (combat 4.3)") {
    Rng rng(1);
    rng.Script({0.1});
    CHECK(CheckCritDoubleStrike(20, true, rng));     // 10 < 20
    CHECK_FALSE(CheckCritDoubleStrike(20, false, rng));  // no draw without a crit
    CHECK(rng.ScriptedRemaining() == 0);
    rng.Script({0.1});
    CHECK_FALSE(CheckDoubleShot(50, 1.5, rng));  // melee range: never, no draw
    CHECK(rng.ScriptedRemaining() == 1);
    CHECK(CheckDoubleShot(50, 3, rng));
    rng.Script({0.5});
    CHECK_FALSE(CheckFreeCast(50, rng));  // 50 < 50 is false
    CHECK_FALSE(CheckFreeCast(0, rng));
    CHECK(PercentOfMaxHp(285, 5) == 14);
  }

  // ===================================================================================================================
  // Hit feedback (combat 10-11; HitFeedback.test.ts)
  // ===================================================================================================================
  TEST_CASE("classifyHit: kill > crit > damage share; ticks stay ticks; bad max hp is light") {
    const HitFeedbackTable& t = D().Combat().hitFeedback;
    CHECK(ClassifyHit(t, 1, 100, true, true, false) == HitWeight::Kill);
    CHECK(ClassifyHit(t, 1, 100, true, false, false) == HitWeight::Crit);
    CHECK(ClassifyHit(t, 30, 100, false, false, false) == HitWeight::Heavy);
    CHECK(ClassifyHit(t, 10, 100, false, false, false) == HitWeight::Normal);
    CHECK(ClassifyHit(t, 2, 100, false, false, false) == HitWeight::Light);
    CHECK(ClassifyHit(t, 500, 100, false, true, true) == HitWeight::Tick);
    CHECK(ClassifyHit(t, 10, 0, false, false, false) == HitWeight::Light);
  }

  TEST_CASE("HIT_PROFILES escalate with weight; ticks never flinch") {
    const HitFeedbackTable& t = D().Combat().hitFeedback;
    const HitWeight order[] = {HitWeight::Tick, HitWeight::Light, HitWeight::Normal,
                               HitWeight::Heavy, HitWeight::Crit, HitWeight::Kill};
    for (size_t i = 1; i < 6; ++i) {
      const HitProfileDef& prev = t.Profile(order[i - 1]);
      const HitProfileDef& cur = t.Profile(order[i]);
      CHECK(cur.targetStopMs >= prev.targetStopMs);
      CHECK(cur.attackerStopMs >= prev.attackerStopMs);
      CHECK(cur.recoil >= prev.recoil);
      CHECK(cur.shakeIntensity >= prev.shakeIntensity);
    }
    for (HitWeight w : order) CHECK(t.Profile(w).targetStopMs <= 150);
    CHECK(t.Profile(HitWeight::Tick).targetStopMs == 0);
    CHECK(t.Profile(HitWeight::Tick).recoil == 0);
    // 11.0b verbatim spot checks.
    CHECK(t.Profile(HitWeight::Crit).attackerStopMs == 90);
    CHECK(t.Profile(HitWeight::Kill).shakeIntensity == doctest::Approx(0.005));
    // Monster attacker stop: round(attackerStopMs x 0.6).
    CHECK(MonsterAttackerStopMs(t, HitWeight::Normal) == 27);
    CHECK(MonsterAttackerStopMs(t, HitWeight::Crit) == 54);
  }

  TEST_CASE("attackSpeedScale (combat 24) and the web HitFeedback cases") {
    const AnimTimingTable& a = D().Combat().anim;
    CHECK(AttackSpeedScale(a, 610, 1000) == 1);
    CHECK(AttackSpeedScale(a, 610, 300) == doctest::Approx(0.442623).epsilon(1e-5));
    CHECK(AttackSpeedScale(a, 610, 100) == doctest::Approx(0.35));
    CHECK(AttackSpeedScale(a, 0, 500) == 1);
    CHECK(AttackSpeedScale(a, 610, 0) == 1);
    CHECK(AttackSpeedScale(a, 600, 1000) == 1);
    CHECK(AttackSpeedScale(a, 600, 400) == doctest::Approx(0.6));
    CHECK(AttackSpeedScale(a, 600, 50) == doctest::Approx(0.35));
  }

  TEST_CASE("contact and release beats (combat 10.1-10.2, classes 9.4) from anim_timing (no manifest)") {
    const AnimTimingTable& a = D().Combat().anim;
    const AssetManifest none;
    CHECK(ComputeAttackTiming(a, none, "warrior", AnimRig::Warrior, 1000).contactMs == 308);
    CHECK(ComputeAttackTiming(a, none, "mage", AnimRig::Mage, 1000).contactMs == 267);
    CHECK(ComputeAttackTiming(a, none, "rogue", AnimRig::Rogue, 1000).contactMs == 222);
    for (AnimRig r : {AnimRig::Humanoid, AnimRig::Slime, AnimRig::Beast, AnimRig::Large, AnimRig::Flying}) {
      CHECK(ComputeAttackTiming(a, none, "m", r, 1500).contactMs == 250);
    }
    const ActionTiming fast = ComputeAttackTiming(a, none, "warrior", AnimRig::Warrior, 400);
    CHECK(fast.speed == doctest::Approx(0.5902).epsilon(1e-3));
    CHECK(fast.contactMs == 182);
    CHECK(fast.playRate == doctest::Approx(1 / fast.speed));
    // Monster telegraph: 0.62 x contact = 155 at speed 1.
    CHECK(ComputeAttackTiming(a, none, "monster_goblin", AnimRig::Humanoid, 1200).windupMs == doctest::Approx(155));
    // Cast release beats (not speed-scaled).
    CHECK(ComputeCastTiming(a, none, "warrior", AnimRig::Warrior).contactMs == 334);
    CHECK(ComputeCastTiming(a, none, "mage", AnimRig::Mage).contactMs == 230);
    CHECK(ComputeCastTiming(a, none, "rogue", AnimRig::Rogue).contactMs == 246);
    // A rig without castReleaseMs: round(castDuration x 0.46).
    CHECK(ComputeCastTiming(a, none, "m", AnimRig::Humanoid).contactMs ==
          JsRound(a.Preset(AnimRig::Humanoid).castDuration * 0.46));
  }

  TEST_CASE("contact beat: the asset manifest's authored notify wins over anim_timing") {
    const AnimTimingTable& a = D().Combat().anim;
    AssetManifest man;
    man.loaded = true;
    AssetEntryDef asset;
    asset.name = "SK_Warrior";
    asset.gameIds = {"warrior"};
    AnimClipDef attack;
    attack.name = "Attack01";
    attack.hasContactMs = true;
    attack.contactMs = 300;
    AnimClipDef cast;
    cast.name = "Cast01";
    cast.hasReleaseMs = true;
    cast.releaseMs = 320;
    asset.anims = {attack, cast};
    man.assets.push_back(asset);
    man.gameIdToAsset.push_back({"warrior", "SK_Warrior"});
    if (man.FindByGameId("warrior") == nullptr) {
      MESSAGE("AssetManifest::FindByGameId not wired for hand-built manifests; skipped");
      return;
    }
    CHECK(ComputeAttackTiming(a, man, "warrior", AnimRig::Warrior, 1000).contactMs == 300);
    CHECK(ComputeAttackTiming(a, man, "warrior", AnimRig::Warrior, 400).contactMs == JsRound(300 * 400 * 0.9 / 610));
    CHECK(ComputeCastTiming(a, man, "warrior", AnimRig::Warrior).contactMs == 320);
    CHECK(ComputeAttackTiming(a, man, "mage", AnimRig::Mage, 1000).contactMs == 267);  // other assets: fallback
    // A notify on the web's beat in whole ms (308 for 307.69) is that beat: the exact frame value is speed-scaled
    // (10.1 round(frameContactMs x speed)): at speed 0.7 -> round(215.38) = 215, not round(308 x 0.7) = 216.
    const double interval07 = 0.7 * a.Preset(AnimRig::Warrior).attackDuration / 0.9;
    CHECK(ComputeAttackTiming(a, man, "warrior", AnimRig::Warrior, interval07).speed == doctest::Approx(0.7));
    CHECK(ComputeAttackTiming(a, man, "warrior", AnimRig::Warrior, interval07).contactMs == 210);  // moved notify
    man.assets[0].anims[0].contactMs = 308;
    CHECK(ComputeAttackTiming(a, man, "warrior", AnimRig::Warrior, 1000).contactMs == 308);
    CHECK(ComputeAttackTiming(a, man, "warrior", AnimRig::Warrior, interval07).contactMs == 215);
    CHECK(ComputeAttackTiming(a, AssetManifest{}, "warrior", AnimRig::Warrior, interval07).contactMs == 215);
    if (D().Assets().loaded) {  // the exported art manifest (warrior Attack01 Contact notify at 308 ms)
      CHECK(ComputeAttackTiming(a, D().Assets(), "warrior", AnimRig::Warrior, interval07).contactMs == 215);
    }
  }

  TEST_CASE("camera shake rules (combat 11.5, 6.5) and the throttle") {
    const HitFeedbackTable& t = D().Combat().hitFeedback;
    const ShakeRequest crit = HeroHitShake(t, 10, 100, true);
    CHECK(crit.durationMs == 150);
    CHECK(crit.intensity == doctest::Approx(0.008));
    const ShakeRequest small = HeroHitShake(t, 1, 100, false);
    CHECK(small.durationMs == doctest::Approx(51));
    CHECK(small.intensity == doctest::Approx(0.002));
    const ShakeRequest big = HeroHitShake(t, 100, 100, false);
    CHECK(big.durationMs == 120);
    CHECK(big.intensity == doctest::Approx(0.006));
    CHECK(AoeHitShake(t, 0).Empty());
    const ShakeRequest aoe = AoeHitShake(t, 3);
    CHECK(aoe.durationMs == 100);
    CHECK(aoe.intensity == doctest::Approx(0.007));
    ShakeThrottle th;
    CHECK(th.Accept({60, 0.002}, 1000, 100));
    CHECK_FALSE(th.Accept({60, 0.002}, 1050, 100));  // throttled
    CHECK(th.Accept({200, 0.002}, 1100, 100));
    CHECK_FALSE(th.Accept({60, 0.002}, 1250, 100));  // still running (no override)
    CHECK(th.Accept({60, 0.002}, 1300, 100));
    CHECK_FALSE(th.Accept({0, 0.1}, 2000, 100));
    CHECK(ClassImpactColor(t, ClassId::Warrior) == 0xffd98au);
  }

  // ===================================================================================================================
  // Status effects (classes 13, combat 7; StatusEffectSystem.test.ts)
  // ===================================================================================================================
  TEST_CASE("status: burn ticks every second, stacks, expires, does not immobilize") {
    StatusEffectSystem s(D().Classes().statusRules);
    const StatusApplyResult r = s.Apply(10, StatusType::Burn, 10, 5000, 1, 0);
    CHECK(r.outcome == StatusApplyOutcome::Applied);
    CHECK(r.effectiveDurationMs == 5000);
    CHECK(s.EffectsOf(10)[0].tickIntervalMs == 1000);
    CHECK(TickStatuses(s, 500).empty());
    std::vector<StatusTick> t = TickStatuses(s, 1000);
    REQUIRE(t.size() == 1);
    CHECK(t[0].damagePerTick == 10);
    CHECK(t[0].ticks == 1);
    CHECK(TickCount(TickStatuses(s, 2000)) == 1);
    CHECK_FALSE(s.IsImmobilized(10));
    // Two burns stack.
    StatusEffectSystem s2(D().Classes().statusRules);
    s2.Apply(10, StatusType::Burn, 10, 5000, 1, 0);
    s2.Apply(10, StatusType::Burn, 15, 5000, 2, 0);
    CHECK(s2.EffectsOf(10).size() == 2);
    t = TickStatuses(s2, 1000);
    CHECK(t.size() == 2);
    CHECK(t[0].Total() + t[1].Total() == 25);
    // Expiry at the duration (ticks of that call first).
    StatusEffectSystem s3(D().Classes().statusRules);
    s3.Apply(10, StatusType::Burn, 10, 3000, 1, 0);
    std::vector<StatusExpiry> exp;
    CHECK(TickCount(TickStatuses(s3, 3000, &exp)) == 3);
    CHECK(ContainsExpiry(exp, StatusType::Burn));
    CHECK_FALSE(s3.Has(10, StatusType::Burn));
  }

  TEST_CASE("status: freeze / stun immobilize, speed 0, no ticks, replace") {
    StatusEffectSystem s(D().Classes().statusRules);
    CHECK(s.Apply(10, StatusType::Freeze, 1, 2000, 1, 0).effectiveDurationMs == 2000);
    CHECK(s.IsImmobilized(10));
    CHECK(s.SpeedMultiplier(10) == 0);
    CHECK(TickStatuses(s, 1000).empty());
    std::vector<StatusExpiry> exp;
    TickStatuses(s, 2000, &exp);
    CHECK(ContainsExpiry(exp, StatusType::Freeze));
    CHECK_FALSE(s.IsImmobilized(10));
    s.Apply(11, StatusType::Stun, 1, 2000, 1, 0);
    s.Apply(11, StatusType::Stun, 1, 2000, 1, 500);  // DR: 1000, replaces
    int32_t stuns = 0;
    for (const StatusEffect& e : s.EffectsOf(11)) stuns += e.type == StatusType::Stun ? 1 : 0;
    CHECK(stuns == 1);
  }

  TEST_CASE("status DR vectors (combat 24): blocked attempts during immunity do not touch the record") {
    StatusEffectSystem s(D().Classes().statusRules);
    CHECK(s.Apply(5, StatusType::Stun, 1, 2000, 1, 0).effectiveDurationMs == 2000);
    CHECK(s.Apply(5, StatusType::Stun, 1, 2000, 1, 100).effectiveDurationMs == 1000);  // immune until 4100
    const StatusApplyResult blocked = s.Apply(5, StatusType::Stun, 1, 2000, 1, 3000);
    CHECK(blocked.outcome == StatusApplyOutcome::Blocked);
    CHECK(blocked.effectiveDurationMs == 0);
    CHECK(s.Apply(5, StatusType::Stun, 1, 2000, 1, 4200).effectiveDurationMs == 0);  // count 3, immune until 7200
    CHECK(s.Apply(5, StatusType::Stun, 1, 2000, 1, 7100).effectiveDurationMs == 0);  // still immune
    CHECK(s.Apply(5, StatusType::Stun, 1, 2000, 1, 13300).effectiveDurationMs == 2000);  // > 6000 after 4200
  }

  TEST_CASE("status DR vectors (classes 22.9)") {
    StatusEffectSystem s(D().Classes().statusRules);
    CHECK(s.Apply(5, StatusType::Stun, 1, 2000, 1, 10000).effectiveDurationMs == 2000);
    CHECK(s.Apply(5, StatusType::Stun, 1, 2000, 1, 11000).effectiveDurationMs == 1000);  // immune until 15000
    CHECK(s.Apply(5, StatusType::Stun, 1, 2000, 1, 14000).effectiveDurationMs == 0);
    CHECK(s.Apply(5, StatusType::Stun, 1, 2000, 1, 15500).effectiveDurationMs == 0);  // count 3, immune 18500
    CHECK(s.Apply(5, StatusType::Stun, 1, 2000, 1, 21501).effectiveDurationMs == 2000);
    StatusEffectSystem s2(D().Classes().statusRules);
    CHECK(s2.Apply(5, StatusType::Stun, 1, 2000, 1, 10000).effectiveDurationMs == 2000);
    CHECK(s2.Apply(5, StatusType::Stun, 1, 2000, 1, 17000).effectiveDurationMs == 2000);
    // Freeze and stun have independent tracks; burn / poison / bleed / slow never diminish.
    StatusEffectSystem s3(D().Classes().statusRules);
    s3.Apply(5, StatusType::Freeze, 1, 2000, 1, 0);
    s3.Apply(5, StatusType::Freeze, 1, 2000, 1, 2500);
    CHECK(s3.Apply(5, StatusType::Stun, 1, 2000, 1, 4000).effectiveDurationMs == 2000);
    for (StatusType t : {StatusType::Burn, StatusType::Poison, StatusType::Bleed, StatusType::Slow}) {
      StatusEffectSystem x(D().Classes().statusRules);
      CHECK(x.Apply(5, t, 10, 3000, 1, 0).effectiveDurationMs == 3000);
      TickStatuses(x, 3000);
      CHECK(x.Apply(5, t, 10, 3000, 1, 3500).effectiveDurationMs == 3000);
    }
  }

  TEST_CASE("status: the web DR scenarios (CombatSystemBugs.test.ts stun mechanics)") {
    StatusEffectSystem s(D().Classes().statusRules);
    const StatusEffectRules& r = D().Classes().statusRules;
    CHECK(s.Apply(1, StatusType::Stun, 1, 2000, 2, 1000).effectiveDurationMs == 2000);
    const double d2 = s.Apply(1, StatusType::Stun, 1, 2000, 2, 3000).effectiveDurationMs;
    CHECK(d2 == std::floor(2000 * r.diminishingFactor));
    CHECK(s.Apply(1, StatusType::Stun, 1, 2000, 2, 4000).effectiveDurationMs == 0);
    const double immuneEnd = 3000 + d2 + r.diminishingImmunityMs;
    CHECK(s.Apply(1, StatusType::Stun, 1, 2000, 2, immuneEnd - 1).effectiveDurationMs == 0);
    // The blocked attempts (4000, immuneEnd - 1) never touch the record: the window counts from the last counted
    // application (3000), so immuneEnd + window + 1 is a fresh stun.
    CHECK(s.Apply(1, StatusType::Stun, 1, 2000, 2, immuneEnd + r.diminishingWindowMs + 1).effectiveDurationMs == 2000);
    // Death clears the records.
    StatusEffectSystem s2(D().Classes().statusRules);
    s2.Apply(1, StatusType::Stun, 1, 2000, 2, 0);
    s2.ClearEntity(1);
    CHECK(s2.Apply(1, StatusType::Stun, 1, 2000, 2, 1000).effectiveDurationMs == 2000);
    // Rejects.
    CHECK(s2.Apply(1, StatusType::Burn, 10, 0, 2, 0).outcome == StatusApplyOutcome::Rejected);
    CHECK(s2.Apply(1, StatusType::Burn, 0, 3000, 2, 0).outcome == StatusApplyOutcome::Rejected);
    CHECK(s2.Apply(1, StatusType::Burn, 10, -1000, 2, 0).outcome == StatusApplyOutcome::Rejected);
  }

  TEST_CASE("status: poison / slow refresh keeps the stronger value, refreshes start and duration, keeps lastTick") {
    StatusEffectSystem s(D().Classes().statusRules);
    s.Apply(10, StatusType::Poison, 5, 3000, 1, 0);
    TickStatuses(s, 600);
    const StatusApplyResult r = s.Apply(10, StatusType::Poison, 10, 4000, 2, 1000);
    CHECK(r.outcome == StatusApplyOutcome::Refreshed);
    REQUIRE(s.EffectsOf(10).size() == 1);
    CHECK(s.EffectsOf(10)[0].value == 10);
    CHECK(s.EffectsOf(10)[0].startMs == 1000);
    CHECK(s.EffectsOf(10)[0].durationMs == 4000);
    CHECK(s.EffectsOf(10)[0].lastTickMs == 0);
    CHECK(s.EffectsOf(10)[0].source == 2);
    s.Apply(10, StatusType::Poison, 3, 2000, 3, 1500);  // weaker: duration refreshed, value kept
    CHECK(s.EffectsOf(10)[0].value == 10);
    CHECK(s.EffectsOf(10)[0].durationMs == 2000);
    // Slow: 50 -> speed 0.5; 95 / 100 -> the 0.2 floor; refresh keeps the stronger.
    StatusEffectSystem sl(D().Classes().statusRules);
    sl.Apply(1, StatusType::Slow, 50, 3000, 1, 0);
    CHECK(sl.SpeedMultiplier(1) == doctest::Approx(0.5));
    sl.Apply(1, StatusType::Slow, 30, 5000, 2, 1000);
    CHECK(sl.EffectsOf(1).size() == 1);
    CHECK(sl.EffectsOf(1)[0].value == 50);
    CHECK(sl.EffectsOf(1)[0].durationMs == 5000);
    StatusEffectSystem sl2(D().Classes().statusRules);
    sl2.Apply(1, StatusType::Slow, 95, 3000, 1, 0);
    CHECK(sl2.SpeedMultiplier(1) == doctest::Approx(0.2));
    sl2.Apply(2, StatusType::Slow, 90, 3000, 1, 0);
    CHECK(sl2.SpeedMultiplier(2) == doctest::Approx(0.2));
    sl2.Apply(3, StatusType::Slow, 40, 3000, 1, 0);
    CHECK(sl2.SpeedMultiplier(3) == doctest::Approx(0.6));
    CHECK(sl2.SpeedMultiplier(99) == 1);
    // Freeze over slow: 0, then the slow again after the freeze.
    StatusEffectSystem fz(D().Classes().statusRules);
    fz.Apply(1, StatusType::Slow, 50, 5000, 1, 0);
    fz.Apply(1, StatusType::Freeze, 1, 2000, 1, 0);
    CHECK(fz.SpeedMultiplier(1) == 0);
    TickStatuses(fz, 2000);
    CHECK(fz.SpeedMultiplier(1) == doctest::Approx(0.5));
  }

  TEST_CASE("status FIX Q21: ticks keep the remainder and are capped at floor(duration / interval)") {
    StatusEffectSystem s(D().Classes().statusRules);
    // A large delta fires the backlog at once (web), still capped by the duration.
    s.Apply(1, StatusType::Burn, 10, 10000, 1, 0);
    CHECK(TickCount(TickStatuses(s, 3000)) == 3);
    CHECK(TickCount(TickStatuses(s, 99999)) == 7);  // 10 total, then expiry
    CHECK_FALSE(s.Has(1, StatusType::Burn));
    // At 60 Hz a 3000 ms burn yields exactly 3 ticks (the web sometimes lost one).
    for (int32_t startStep : {0, 1, 7, 13, 59}) {
      StatusEffectSystem x(D().Classes().statusRules);
      const double start = startStep * 1000.0 / 60.0;
      x.Apply(1, StatusType::Burn, 4, 3000, 1, start);
      int32_t total = 0;
      for (int32_t step = startStep + 1; step <= startStep + 240; ++step)
        total += TickCount(TickStatuses(x, step * 1000.0 / 60.0));
      CHECK_MESSAGE(total == 3, "start step " << startStep);
      CHECK_FALSE(x.Has(1, StatusType::Burn));
    }
    // classes 22.11(b): poison 5000 at 0 -> 5 ticks at the first steps >= 1000..5000, then expiry.
    StatusEffectSystem p(D().Classes().statusRules);
    p.Apply(1, StatusType::Poison, 3, 5000, 1, 0);
    std::vector<double> tickTimes;
    for (int32_t step = 1; step <= 400; ++step) {
      const double now = step * 1000.0 / 60.0;
      if (TickCount(TickStatuses(p, now)) > 0) tickTimes.push_back(now);
    }
    REQUIRE(tickTimes.size() == 5);
    for (size_t i = 0; i < 5; ++i) CHECK(tickTimes[i] == doctest::Approx(1000.0 * static_cast<double>(i + 1)));
  }

  TEST_CASE("status: mixed effects, masks, clearEntity / clearAll, Remove (antidote) keeps DR records") {
    StatusEffectSystem s(D().Classes().statusRules);
    s.Apply(1, StatusType::Burn, 10, 5000, 9, 0);
    s.Apply(1, StatusType::Poison, 5, 3000, 9, 0);
    s.Apply(1, StatusType::Slow, 30, 4000, 9, 0);
    CHECK(s.EffectsOf(1).size() == 3);
    const std::vector<StatusTick> t = TickStatuses(s, 1000);
    CHECK(t.size() == 2);
    CHECK(s.SpeedMultiplier(1) == doctest::Approx(0.7));
    CHECK(s.StatusMask(1) == ((1u << 0) | (1u << 2) | (1u << 4)));
    CHECK(s.Remove(1, StatusType::Poison));
    CHECK_FALSE(s.Has(1, StatusType::Poison));
    CHECK_FALSE(s.Remove(1, StatusType::Poison));
    s.Apply(2, StatusType::Stun, 1, 1000, 9, 0);
    s.ClearEntity(1);
    CHECK(s.EffectsOf(1).empty());
    CHECK(s.Has(2, StatusType::Stun));
    s.ClearAll();
    CHECK_FALSE(s.Has(2, StatusType::Stun));
    CHECK(TickStatuses(s, 5000).empty());
    // Remove keeps the diminishing record: a second stun right after is halved.
    StatusEffectSystem r(D().Classes().statusRules);
    r.Apply(3, StatusType::Stun, 1, 2000, 9, 0);
    r.Remove(3, StatusType::Stun);
    CHECK(r.Apply(3, StatusType::Stun, 1, 2000, 9, 100).effectiveDurationMs == 1000);
  }

  TEST_CASE("RollStatusRule: one draw per chance rule, value kinds (classes 13.5 / 13.6)") {
    const SkillDef& fireball = Skill("fireball");
    REQUIRE(fireball.statusRules.size() == 1);
    Rng rng(1);
    rng.Script({0.59});
    StatusRuleInput in;
    in.dealtDamage = 34;
    StatusRuleRoll r = RollStatusRule(fireball.statusRules[0], in, rng);
    CHECK(r.applies);
    CHECK(r.status == StatusType::Burn);
    CHECK(r.value == 5);  // max(1, floor(34 x 0.15))
    CHECK(r.durationMs == 3000);
    rng.Script({0.6});
    CHECK_FALSE(RollStatusRule(fireball.statusRules[0], in, rng).applies);
    // No-chance rules draw nothing.
    const SkillDef& poison = Skill("poison_arrow");
    rng.ClearScript();
    rng.Script({0.99});
    in.dealtDamage = 3;
    r = RollStatusRule(poison.statusRules[0], in, rng);
    CHECK(r.applies);
    CHECK(r.value == 1);  // floors at 1
    CHECK(rng.ScriptedRemaining() == 1);
    // slow_trap: round(buffValue x 100) for the scaled buff duration.
    const SkillDef& slowTrap = Skill("slow_trap");
    StatusRuleInput si;
    si.buffValue = 0.4;
    si.buffDurationMs = 5000;
    r = RollStatusRule(slowTrap.statusRules[0], si, rng);
    CHECK(r.status == StatusType::Slow);
    CHECK(r.value == 40);
    CHECK(r.durationMs == 5000);
    // Monster on-hit: fire_elemental burn 30 % of floor(dmg x 0.2).
    const MonsterDef* fe = D().FindMonster("fire_elemental");
    REQUIRE(fe != nullptr);
    REQUIRE(fe->onHitStatus.size() == 1);
    StatusRuleInput mi;
    mi.monsterDamage = fe->damage;
    rng.ClearScript();
    rng.Script({0.1});
    r = RollStatusRule(fe->onHitStatus[0], mi, rng);
    CHECK(r.applies);
    CHECK(r.value == 7);
  }

  TEST_CASE("skill status rules match classes 13.5 for every skill") {
    struct Expect {
      const char* id;
      StatusType type;
      bool chance;
      double p;
    };
    const Expect expected[] = {
        {"fireball", StatusType::Burn, true, 0.6},       {"meteor", StatusType::Burn, true, 0.6},
        {"fire_wall", StatusType::Burn, true, 0.4},      {"combustion", StatusType::Burn, true, 0.4},
        {"explosive_trap", StatusType::Burn, true, 0.4}, {"blizzard", StatusType::Freeze, false, 0},
        {"freeze", StatusType::Freeze, false, 0},        {"ice_arrow", StatusType::Slow, true, 0.35},
        {"poison_arrow", StatusType::Poison, false, 0},  {"poison_cloud", StatusType::Poison, false, 0},
        {"bleed_strike", StatusType::Bleed, false, 0},   {"war_stomp", StatusType::Stun, false, 0},
        {"chain_trap", StatusType::Stun, false, 0}};
    for (const Expect& e : expected) {
      const SkillDef& s = Skill(e.id);
      REQUIRE_MESSAGE(s.statusRules.size() == 1, e.id);
      CHECK(s.statusRules[0].status == e.type);
      CHECK(s.statusRules[0].hasChance == e.chance);
      if (e.chance) CHECK(s.statusRules[0].chance == doctest::Approx(e.p));
    }
    CHECK(Skill("war_stomp").statusRules[0].durationMs == 2000);
    CHECK(Skill("chain_trap").statusRules[0].durationMs == 1000);
    CHECK(Skill("slash").statusRules.empty());
    CHECK(Skill("death_mark").statusRules.empty());
  }

  // ===================================================================================================================
  // Input buffer, dodge, target cycling (CombatInputSystem.test.ts, combat 8-9)
  // ===================================================================================================================
  TEST_CASE("input buffer (combat 6.1)") {
    InputBuffer b(180);
    CHECK(b.Request(3, 1000, true) == InputBuffer::RequestResult::ExecuteNow);
    CHECK_FALSE(b.HasPending());
    CHECK(b.Request(3, 1000, false) == InputBuffer::RequestResult::Buffered);
    CHECK(b.ExpiresAtMs() == 1180);
    CHECK(b.ConsumeReady(1100, [](int32_t) { return false; }) == -1);
    CHECK(b.PendingSkill() == 3);
    CHECK(b.ConsumeReady(1179, [](int32_t) { return true; }) == 3);
    CHECK_FALSE(b.HasPending());
    b.Request(1, 1000, false);
    b.Request(2, 1050, false);  // the newest replaces
    CHECK(b.PendingSkill() == 2);
    CHECK(b.ConsumeReady(1231, [](int32_t) { return true; }) == -1);  // expired at 1230
    CHECK_FALSE(b.HasPending());
    b.Request(4, 100, false);
    b.Clear();
    CHECK_FALSE(b.HasPending());
    // An executable request clears an older buffered one.
    b.Request(4, 100, false);
    CHECK(b.Request(5, 110, true) == InputBuffer::RequestResult::ExecuteNow);
    CHECK_FALSE(b.HasPending());
  }

  TEST_CASE("dodge controller (combat 8.1)") {
    CombatInputDef def = D().Combat().input;
    DodgeController d(def);
    CHECK(d.CooldownProgress(0) == 1);
    CHECK(d.CanStart(1000));
    d.Start(1000);
    CHECK(d.IsInvulnerable(1219));
    CHECK_FALSE(d.IsInvulnerable(1220));
    CHECK_FALSE(d.CanStart(1899));
    CHECK(d.CanStart(1900));
    CHECK(d.CooldownProgress(1000) == 0);
    CHECK(d.CooldownProgress(1450) == doctest::Approx(0.5));
    CHECK(d.CooldownProgress(5000) == 1);
    CHECK(d.CooldownRemainingMs(1450) == 450);
    CHECK(d.CooldownRemainingMs(2500) == 0);
    CHECK(d.ClaimAvoidanceReward(1100));
    CHECK_FALSE(d.ClaimAvoidanceReward(1150));
    CHECK_FALSE(d.ClaimAvoidanceReward(1220));
    d.Start(1900);
    CHECK(d.ClaimAvoidanceReward(1901));
    d.Reset();
    CHECK(d.CanStart(0));
    CHECK_FALSE(d.IsInvulnerable(0));
  }

  TEST_CASE("dodge destination (8.1 + C8): requested direction, facing fallback, class distance, walkability") {
    const CombatInputDef& def = D().Combat().input;
    const auto open = [](int32_t c, int32_t r) { return c >= 0 && r >= 0 && c < 100 && r < 100; };
    Vec2 dest;
    REQUIRE(ComputeDodgeDestination(def, ClassId::Rogue, Vec2(50, 50), Vec2(1, 0), Vec2(0, 1), open, dest));
    CHECK(dest.x == doctest::Approx(52.6));
    CHECK(dest.y == doctest::Approx(50));
    // No requested direction: the hero facing (never the web's screen-right fallback).
    REQUIRE(ComputeDodgeDestination(def, ClassId::Warrior, Vec2(50, 50), Vec2(), Vec2(0, 2), open, dest));
    CHECK(dest.x == doctest::Approx(50));
    CHECK(dest.y == doctest::Approx(51.8));
    REQUIRE(ComputeDodgeDestination(def, ClassId::Mage, Vec2(50, 50), Vec2(3, 4), Vec2(1, 0), open, dest));
    CHECK(dest.x == doctest::Approx(50 + 2.25 * 0.6));
    CHECK(dest.y == doctest::Approx(50 + 2.25 * 0.8));
    // A wall at the far tiles: the first shorter distance whose rounded tile is walkable.
    const auto wall = [](int32_t c, int32_t r) { return c < 52; };  // x >= 51.5 blocked
    REQUIRE(ComputeDodgeDestination(def, ClassId::Rogue, Vec2(49, 50), Vec2(1, 0), Vec2(1, 0), wall, dest));
    CHECK(dest.x == doctest::Approx(49 + 2.35));  // 51.6 -> tile 52 blocked; 51.35 -> 51 ok
    const auto none = [](int32_t, int32_t) { return false; };
    CHECK_FALSE(ComputeDodgeDestination(def, ClassId::Rogue, Vec2(49, 50), Vec2(1, 0), Vec2(1, 0), none, dest));
  }

  TEST_CASE("cycleTargetId (9.3): nearest first, wrap, alive and in range only") {
    const TargetCandidate list[] = {
        {1, Vec2(2, 0), true}, {2, Vec2(5, 0), true}, {3, Vec2(1, 0), false}, {4, Vec2(11, 0), true}};
    CHECK(CycleTarget(list, Vec2(0, 0), 10, kNoEntity) == 1);
    CHECK(CycleTarget(list, Vec2(0, 0), 10, 1) == 2);
    CHECK(CycleTarget(list, Vec2(0, 0), 10, 2) == 1);
    CHECK(CycleTarget(list, Vec2(0, 0), 1, kNoEntity) == kNoEntity);
    CHECK(CycleTarget(list, Vec2(0, 0), 10, 77) == 1);
    // Equal distances: id order.
    const TargetCandidate tie[] = {{9, Vec2(0, 3), true}, {5, Vec2(3, 0), true}};
    CHECK(CycleTarget(tie, Vec2(0, 0), 14, kNoEntity) == 5);
    CHECK(CycleTarget(tie, Vec2(0, 0), 14, 5) == 9);
  }

  // ===================================================================================================================
  // Targeting geometry (classes 9.7, 10; C1, C4, C8)
  // ===================================================================================================================
  TEST_CASE("targeting: ground anchor, radius, line, cone") {
    const TargetCandidate c[] = {{1, Vec2(3, 0), true}, {2, Vec2(5.5, 0), true}, {3, Vec2(7, 0), true},
                                 {4, Vec2(2, 0), false}, {5, Vec2(4, 1), true}};
    // The target within range + 1 wins; else the nearest alive within it.
    CHECK(FindGroundAoeAnchor(c, Vec2(0, 0), 3, Vec2(7, 0), true, 6) == 3);
    CHECK(FindGroundAoeAnchor(c, Vec2(0, 0), 3, Vec2(7, 0), true, 5) == 1);
    CHECK(FindGroundAoeAnchor(c, Vec2(0, 0), 3, Vec2(7, 0), false, 6) == 1);
    CHECK(FindGroundAoeAnchor(c, Vec2(0, 0), kNoEntity, Vec2(), false, 1) == kNoEntity);
    CHECK(CandidatesInRadius(c, Vec2(3, 0), 1.5) == std::vector<EntityId>{1, 5});
    CHECK(NearestAlive(c, Vec2(0, 0), 100) == 1);
    CHECK(NearestAlive(c, Vec2(0, 0), 2) == kNoEntity);
    // piercing line: along 0..range, |perp| <= halfWidth.
    const TargetCandidate line[] = {{1, Vec2(2, 0.5), true}, {2, Vec2(6, -0.9), true}, {3, Vec2(6.1, 0), true},
                                    {4, Vec2(-1, 0), true},  {5, Vec2(3, 1), true}};
    CHECK(CandidatesAlongLine(line, Vec2(0, 0), Vec2(4, 0), 1, 6, 0.9) == std::vector<EntityId>{1, 2});
    CHECK(CandidatesAlongLine(line, Vec2(0, 0), Vec2(0, 0), 1, 6, 0.9) == std::vector<EntityId>{1});
    // C4 multishot cone: 50 degrees around the aim.
    const TargetCandidate cone[] = {{1, Vec2(3, 0), true}, {2, Vec2(3, 1.3), true}, {3, Vec2(3, 1.5), true},
                                    {4, Vec2(-3, 0), true}, {5, Vec2(0, 3), true}, {6, Vec2(5, 0), true}};
    // atan(1.3 / 3) = 23.4 deg (inside 25), atan(1.5 / 3) = 26.6 deg (outside); 6 is beyond the radius.
    CHECK(CandidatesInCone(cone, Vec2(0, 0), Vec2(1, 0), 3.5, 50) == std::vector<EntityId>{1, 2});
  }

  TEST_CASE("teleport destination (FIX Q17 range clamp, C8 touch aim, ring search)") {
    const SkillPortDef& port = Skill("teleport").port;
    REQUIRE(port.hasTeleport);
    CHECK(port.teleportMaxRangeTiles == 8);
    const auto open = [](int32_t, int32_t) { return true; };
    TilePos t;
    TeleportAim aim;
    aim.hasPoint = true;
    aim.point = Vec2(30, 10);
    REQUIRE(ComputeTeleportDestination(port, Vec2(10, 10), aim, 100, 100, open, t));
    CHECK(t == TilePos(18, 10));  // clamped to 8 tiles along the ray
    aim.point = Vec2(13.4, 12.6);
    REQUIRE(ComputeTeleportDestination(port, Vec2(10, 10), aim, 100, 100, open, t));
    CHECK(t == TilePos(13, 13));
    // Map clamp [1, size - 2].
    aim.point = Vec2(-3, 10);
    REQUIRE(ComputeTeleportDestination(port, Vec2(2, 10), aim, 100, 100, open, t));
    CHECK(t == TilePos(1, 10));
    // Touch: joystick beyond the deadzone x 6; else the target; else 6 ahead along the facing.
    TeleportAim touch;
    touch.stickDir = Vec2(0, 0.5);
    REQUIRE(ComputeTeleportDestination(port, Vec2(10, 10), touch, 100, 100, open, t));
    CHECK(t == TilePos(10, 16));
    touch.stickDir = Vec2(0.1, 0);
    touch.hasTarget = true;
    touch.targetPos = Vec2(12, 14);
    REQUIRE(ComputeTeleportDestination(port, Vec2(10, 10), touch, 100, 100, open, t));
    CHECK(t == TilePos(12, 14));
    touch.hasTarget = false;
    touch.facing = Vec2(-1, 0);
    REQUIRE(ComputeTeleportDestination(port, Vec2(10, 10), touch, 100, 100, open, t));
    CHECK(t == TilePos(4, 10));
    // Ring search (row-major dr then dc) when the tile is blocked; none within 3 rings -> unreachable.
    const auto onlyOne = [](int32_t c, int32_t r) { return c == 21 && r == 19; };
    aim.point = Vec2(20, 20);
    REQUIRE(ComputeTeleportDestination(port, Vec2(15, 20), aim, 100, 100, onlyOne, t));
    CHECK(t == TilePos(21, 19));
    const auto firstOfRing = [](int32_t c, int32_t r) { return (c == 21 && r == 21) || (c == 19 && r == 21); };
    REQUIRE(ComputeTeleportDestination(port, Vec2(15, 20), aim, 100, 100, firstOfRing, t));
    CHECK(t == TilePos(19, 21));  // dr = 1 row first, dc = -1 before +1
    const auto blocked = [](int32_t, int32_t) { return false; };
    CHECK_FALSE(ComputeTeleportDestination(port, Vec2(15, 20), aim, 100, 100, blocked, t));
  }

  TEST_CASE("shadow step destination and the charge dash end (C4)") {
    const auto open = [](int32_t, int32_t) { return true; };
    CHECK(ShadowStepDestination(Vec2(10, 10), Vec2(14, 10), 100, 100, open) == TilePos(15, 10));
    CHECK(ShadowStepDestination(Vec2(14, 10), Vec2(14, 10), 100, 100, open) == TilePos(14, 10));  // len 0 -> 1
    const auto noBehind = [](int32_t c, int32_t) { return c != 15; };
    CHECK(ShadowStepDestination(Vec2(10, 10), Vec2(14, 10), 100, 100, noBehind) == TilePos(14, 10));
    CHECK(ShadowStepDestination(Vec2(10, 10), Vec2(98, 10), 100, 100, open) == TilePos(98, 10));  // clamp 98
    const Vec2 end = ChargeDashEnd(Vec2(0, 0), Vec2(5, 0), 1.5);
    CHECK(end.x == doctest::Approx(3.5));
    CHECK(end.y == doctest::Approx(0));
    CHECK(ChargeDashEnd(Vec2(0, 0), Vec2(1, 0), 1.5) == Vec2(0, 0));
    const SkillPortDef& charge = Skill("charge").port;
    CHECK(charge.hasDash);
    CHECK(charge.dashDurationMs == 250);
  }

  // ===================================================================================================================
  // Projectile timing (combat 5.2, 6.4-6.5; S4)
  // ===================================================================================================================
  TEST_CASE("projectile and arrow timing (tileDist x 36 px)") {
    const ProjectileTimingTable& t = D().Combat().projectiles;
    CHECK(MonsterBoltTravelMs(t, Vec2(0, 0), Vec2(1, 0)) == 200);   // 72 -> 200 floor
    CHECK(MonsterBoltTravelMs(t, Vec2(0, 0), Vec2(4, 0)) == 288);   // 144 px x 2
    CHECK(MonsterBoltTravelMs(t, Vec2(0, 0), Vec2(10, 0)) == 500);  // cap
    CHECK(SkillTravelMs(Skill("fireball"), Vec2(0, 0), Vec2(2, 0)) == 300);
    CHECK(SkillTravelMs(Skill("fireball"), Vec2(0, 0), Vec2(8, 0)) == doctest::Approx(432));
    CHECK(SkillTravelMs(Skill("fireball"), Vec2(0, 0), Vec2(20, 0)) == 600);
    CHECK(SkillTravelMs(Skill("ice_arrow"), Vec2(0, 0), Vec2(1, 0)) == 250);
    CHECK(SkillTravelMs(Skill("poison_arrow"), Vec2(0, 0), Vec2(20, 0)) == 500);
    CHECK(SkillTravelMs(Skill("slash"), Vec2(0, 0), Vec2(1, 0)) == 0);
    CHECK(SkillTravelMs(Skill("meteor"), Vec2(0, 0), Vec2(5, 0)) == 0);  // meteor: aoeDelayMs instead
    CHECK(Skill("meteor").aoeDelayMs == 300);
    CHECK(SkillArrowDelayMs(Skill("multishot"), Vec2(0, 0), Vec2(3, 0)) == doctest::Approx(118.8));
    CHECK(SkillArrowDelayMs(Skill("piercing_arrow"), Vec2(0, 0), Vec2(10, 0)) == 260);
    CHECK(SkillArrowDelayMs(Skill("whirlwind"), Vec2(0, 0), Vec2(3, 0)) == 0);
    CHECK(MonsterBoltColor("monster_fire_elemental") == 0xff6600u);
    CHECK(MonsterBoltColor("phoenix") == 0xff6600u);
    CHECK(MonsterBoltColor("ice_golem") == 0x4488ffu);
    CHECK(MonsterBoltColor("monster_goblin_shaman") == 0xcc44ccu);
  }

  // ===================================================================================================================
  // Soul echo (combat 15; SoulEcho.test.ts)
  // ===================================================================================================================
  TEST_CASE("death penalty (combat 15 vectors and the web suite)") {
    const SoulEchoDef& d = D().Combat().soulEcho;
    CHECK(d.minLevel == 5);
    DeathPenalty p = ComputeDeathPenalty(d, 4, 1000, 50, 100, Difficulty::Hell);
    CHECK(p.gold == 0);
    CHECK(p.exp == 0);
    p = ComputeDeathPenalty(d, 5, 1000, 0, 200, Difficulty::Normal);
    CHECK(p.gold == 100);
    CHECK(p.exp == 0);
    p = ComputeDeathPenalty(d, 10, 1000, 50, 1200, Difficulty::Hell);
    CHECK(p.exp == 50);  // min(50, 60)
    p = ComputeDeathPenalty(d, 10, 1000, 200, 550, Difficulty::Normal);
    CHECK(p.gold == 100);
    CHECK(p.exp == 0);
    p = ComputeDeathPenalty(d, 10, 1000, 200, 550, Difficulty::Nightmare);
    CHECK(p.gold == 150);
    CHECK(p.exp == 27);
    p = ComputeDeathPenalty(d, 10, 1000, 200, 550, Difficulty::Hell);
    CHECK(p.gold == 200);
    CHECK(p.exp == 27);
    CHECK(ComputeDeathPenalty(d, 10, 0, 3, 550, Difficulty::Hell).exp == 3);
    CHECK(ComputeDeathPenalty(d, 10, -50, 0, 550, Difficulty::Hell).gold == 0);
  }

  TEST_CASE("soul echo state: claim range / map, fade, free death, save junk") {
    SoulEchoState s;
    bool had = false;
    SoulEchoData faded;
    CHECK(s.Leave(SoulEchoData{"emerald_plains", 40, 40, 100, 0}, had, faded));
    CHECK_FALSE(had);
    SoulEchoData claimed;
    CHECK_FALSE(s.TryClaim("twilight_forest", Vec2(40, 40), 1.5, claimed));
    CHECK_FALSE(s.TryClaim("emerald_plains", Vec2(44, 40), 1.5, claimed));
    CHECK_FALSE(s.TryClaim("emerald_plains", Vec2(41.51, 40), 1.5, claimed));
    CHECK(s.TryClaim("emerald_plains", Vec2(41.5, 40), 1.5, claimed));
    CHECK(claimed.gold == 100);
    CHECK_FALSE(s.Has());
    s.Leave(SoulEchoData{"a", 1, 1, 100, 0}, had, faded);
    CHECK(s.Leave(SoulEchoData{"b", 2, 2, 90, 0}, had, faded));
    CHECK(had);
    CHECK(faded.gold == 100);
    CHECK(s.Echo().mapId == "b");
    CHECK_FALSE(s.Leave(SoulEchoData{"c", 1, 1, 0, 0}, had, faded));  // a free death destroys the old echo
    CHECK(had);
    CHECK_FALSE(s.Has());
    CHECK(s.Load("a", 1, 2, 5, 1));
    CHECK(s.Echo().col == 1);
    CHECK(s.Echo().row == 2);
    CHECK(s.Echo().gold == 5);
    CHECK(s.Echo().exp == 1);
    CHECK_FALSE(s.Load("a", std::nan(""), 2, 5, 1));
    CHECK_FALSE(s.Has());
    CHECK_FALSE(s.Load("a", 1, HUGE_VAL, 5, 1));
  }

  TEST_CASE("soul echo system: save round trip") {
    test::SimHarness h;
    Hero hero(h.ctx.data, ClassId::Warrior);
    h.ctx.sys.hero = &hero;
    SoulEchoSystem sys(h.ctx);
    SaveData in;
    in.soulEcho.present = true;
    in.soulEcho.mapId = "emerald_plains";
    in.soulEcho.col = 12;
    in.soulEcho.row = 34;
    in.soulEcho.gold = 77;
    in.soulEcho.exp = 3;
    sys.ReadSave(in);
    CHECK(sys.State().Has());
    SaveData out;
    sys.WriteSave(out);
    CHECK(out.soulEcho.present);
    CHECK(out.soulEcho.mapId == "emerald_plains");
    CHECK(out.soulEcho.col == 12);
    CHECK(out.soulEcho.row == 34);
    CHECK(out.soulEcho.gold == 77);
    CHECK(out.soulEcho.exp == 3);
    SaveData none;
    sys.ReadSave(none);
    SaveData out2;
    sys.WriteSave(out2);
    CHECK_FALSE(out2.soulEcho.present);
  }

  // ===================================================================================================================
  // Projectile system (C4 ground effects, F2) on a harness
  // ===================================================================================================================
  TEST_CASE("ProjectileSystem: launch / arrive / destroy; F2 destroys monster bolts only") {
    test::SimHarness h;
    ProjectileSystem ps(h.ctx);
    h.ctx.sys.projectiles = &ps;
    h.onTimer = [&](const Timer& t) {
      if (t.owner == TimerOwner::Projectiles) ps.OnTimer(t);
    };
    ProjectileSpec bolt;
    bolt.kind = ProjectileKind::MonsterBolt;
    bolt.source = 77;
    bolt.travelMs = 200;
    const EntityId b = ps.Launch(bolt);
    ProjectileSpec arrow;
    arrow.kind = ProjectileKind::HeroSkill;
    arrow.travelMs = 100;
    const EntityId a = ps.Launch(arrow);
    CHECK(test::CountEvents<EvProjectileLaunched>(h.events) == 2);
    CHECK(ps.Projectiles().size() == 2);
    ps.OnFreezeBegin(false);  // modal: kept (F3)
    CHECK(ps.Projectiles().size() == 2);
    ps.OnFreezeBegin(true);
    CHECK(ps.Projectiles().size() == 1);
    CHECK(ps.Find(b) == nullptr);
    const auto cancelled = EventsOf<EvMonsterAttackCancelled>(h.events);
    REQUIRE(cancelled.size() == 1);
    CHECK(cancelled[0].monster == 77);
    CHECK(cancelled[0].projectile);
    h.Step(6);  // 100 ms: the hero projectile arrives (no CombatSystem registered: no callback)
    CHECK(ps.Find(a) == nullptr);
    const auto ended = EventsOf<EvProjectileEnded>(h.events);
    REQUIRE(ended.size() == 2);
    CHECK_FALSE(ended[0].hit);
    CHECK(ended[1].hit);
    const EntityId c = ps.Launch(arrow);
    ps.Destroy(c);
    CHECK(ps.Projectiles().empty());
    CHECK(h.timers.Empty());
  }

  TEST_CASE("ProjectileSystem: periodic ground effect ticks at start + k x interval and ends at duration (C4)") {
    test::SimHarness h;
    ProjectileSystem ps(h.ctx);
    h.ctx.sys.projectiles = &ps;
    h.onTimer = [&](const Timer& t) {
      if (t.owner == TimerOwner::Projectiles) ps.OnTimer(t);
    };
    GroundEffectSpec g;
    g.skillId = "fire_wall";
    g.center = Vec2(10, 10);
    g.radius = 2;
    g.durationMs = 3000;
    g.ticks = 6;
    const EntityId id = ps.StartGroundEffect(g);
    REQUIRE(ps.FindGroundEffect(id) != nullptr);
    CHECK(ps.FindGroundEffect(id)->ticksDone == 1);  // the first tick at once
    h.Step(30);                                       // 500 ms
    CHECK(ps.FindGroundEffect(id)->ticksDone == 2);
    h.Step(150);  // 3000 ms
    CHECK(ps.FindGroundEffect(id) == nullptr);
    const auto ended = EventsOf<EvGroundEffectEnded>(h.events);
    REQUIRE(ended.size() == 1);
    CHECK(ended[0].triggered);
    CHECK(h.timers.Empty());
    // Armed trap with nobody around expires silently.
    GroundEffectSpec trap;
    trap.skillId = "explosive_trap";
    trap.trigger = GroundTrigger::Armed;
    trap.durationMs = 1000;
    trap.radius = 2;
    const EntityId t = ps.StartGroundEffect(trap);
    ps.TickArmedTraps();  // no MonsterSystem: nothing fires
    h.Step(61);
    CHECK(ps.FindGroundEffect(t) == nullptr);
    const auto ended2 = EventsOf<EvGroundEffectEnded>(h.events);
    REQUIRE(ended2.size() == 2);
    CHECK_FALSE(ended2[1].triggered);
  }

  // ===================================================================================================================
  // CombatSystem runtime (hero side, no monsters needed)
  // ===================================================================================================================
  TEST_CASE("CombatSystem: buff skills release on the cast beat; passives are not castable (C1)") {
    CombatRig rig(ClassId::Warrior);
    REQUIRE(rig.ok);
    Hero& hero = *rig.hero;
    const int32_t shieldWall = rig.SkillIndex("shield_wall");
    const int32_t lifeRegen = rig.SkillIndex("life_regen");
    REQUIRE(hero.Skills().Level(shieldWall) == 1);
    CHECK(rig.combat.RequestSkill(lifeRegen, SkillAim{}) == SkillRequestResult::Passive);
    CHECK_FALSE(rig.combat.CanExecuteSkill(lifeRegen));
    const double mana = hero.Mana();
    const double t0 = rig.h.clock.NowMs();
    CHECK(rig.combat.RequestSkill(shieldWall, SkillAim{}) == SkillRequestResult::Executed);
    CHECK(hero.Mana() == doctest::Approx(mana - 12));  // committed at cast start
    const auto used = EventsOf<EvSkillUsed>(rig.h.events);
    REQUIRE(used.size() == 1);
    CHECK(used[0].skillId == "shield_wall");
    const auto anims = EventsOf<EvPlayAnim>(rig.h.events);
    REQUIRE(anims.size() == 1);
    CHECK(anims[0].action == AnimAction::Cast);
    CHECK(anims[0].contactMs == 334);
    CHECK_FALSE(hero.Buffs().Has(BuffStat::DamageReduction));
    rig.StepBefore(t0 + 334);
    CHECK_FALSE(hero.Buffs().Has(BuffStat::DamageReduction));
    rig.StepTo(t0 + 334);
    CHECK(hero.Buffs().Has(BuffStat::DamageReduction));
    CHECK(hero.Buffs().RawSum(BuffStat::DamageReduction) == doctest::Approx(0.5));
    // On cooldown now: the next request is buffered (EvSkillBuffered) and expires.
    rig.h.events.Clear();
    CHECK(rig.combat.RequestSkill(shieldWall, SkillAim{}) == SkillRequestResult::Buffered);
    CHECK(test::CountEvents<EvSkillBuffered>(rig.h.events) == 1);
    rig.Step(12);
    CHECK_FALSE(rig.combat.Buffer().HasPending());
    // The buff expires after its duration (pruned in TickCombat).
    rig.StepTo(t0 + 334 + 5000 + 20);
    CHECK_FALSE(hero.Buffs().Has(BuffStat::DamageReduction));
  }

  TEST_CASE("CombatSystem: death_mark / shadow_step need a target (FIX Q8); a buffered request fires within 180 ms") {
    CombatRig rig(ClassId::Rogue);
    REQUIRE(rig.ok);
    Hero& hero = *rig.hero;
    const int32_t mark = rig.SkillIndex("death_mark");
    hero.Skills().SetLevel(mark, 1);
    CHECK_FALSE(rig.combat.CanExecuteSkill(mark));  // no monster anywhere
    CHECK(rig.combat.RequestSkill(mark, SkillAim{}) == SkillRequestResult::Buffered);
    rig.Step(12);
    CHECK_FALSE(hero.Buffs().Has(BuffStat::DamageAmplify));  // never the web's self-amplify fallback
    // Mana gate: a buff request without mana is buffered until mana arrives.
    const int32_t blade = rig.SkillIndex("poison_blade");
    hero.Skills().SetLevel(blade, 1);
    hero.SetMana(0);
    CHECK(rig.combat.RequestSkill(blade, SkillAim{}) == SkillRequestResult::Buffered);
    rig.Step(3);
    hero.SetMana(100);
    rig.Step(1);
    CHECK_FALSE(rig.combat.Buffer().HasPending());
    rig.Step(20);
    CHECK(hero.Buffs().Has(BuffStat::PoisonDamage));
  }

  TEST_CASE("CombatSystem: freeCast refunds the mana; the cooldown follows cooldownReduction (classes 8)") {
    CombatRig rig(ClassId::Mage);
    REQUIRE(rig.ok);
    Hero& hero = *rig.hero;
    rig.h.ctx.equip.Ref(Stat::FreeCast) = 100;
    rig.h.ctx.equip.Ref(Stat::CooldownReduction) = 60;  // clamped to 50
    const int32_t armor = rig.SkillIndex("ice_armor");
    const double mana = hero.Mana();
    REQUIRE(rig.combat.RequestSkill(armor, SkillAim{}) == SkillRequestResult::Executed);
    CHECK(hero.Mana() == doctest::Approx(mana));
    CHECK(hero.Skills().ReadyAtMs(armor) == doctest::Approx(rig.h.clock.NowMs() + 7500));
  }

  TEST_CASE("CombatSystem: teleport (instant, range-clamped, unreachable refund) and the immobilized gate (C5)") {
    CombatRig rig(ClassId::Mage);
    REQUIRE(rig.ok);
    Hero& hero = *rig.hero;
    const int32_t tp = rig.SkillIndex("teleport");
    hero.Skills().SetLevel(tp, 1);
    const Vec2 start = hero.Position();
    SkillAim aim;
    aim.hasPoint = true;
    aim.point = start + Vec2(20, 0);
    REQUIRE(rig.combat.RequestSkill(tp, aim) == SkillRequestResult::Executed);
    CHECK(hero.Position() == RoundToTile(start + Vec2(8, 0)).Center());  // instant, 8-tile clamp
    CHECK(test::CountEvents<EvEntityTeleported>(rig.h.events) == 1);
    hero.Skills().ResetCooldowns();
    rig.status.Apply(kHeroEntityId, StatusType::Stun, 1, 2000, 0, rig.h.clock.NowMs());
    CHECK_FALSE(rig.combat.CanExecuteSkill(tp));
    CHECK(rig.combat.RequestSkill(tp, aim) == SkillRequestResult::Buffered);
    CHECK_FALSE(rig.combat.RequestDodge(Vec2(1, 0)));  // immobilized: no dodge
  }

  TEST_CASE("CombatSystem: dodge roll (8.1 + C8) - i-frames, cooldown, facing fallback, events") {
    CombatRig rig(ClassId::Rogue);
    REQUIRE(rig.ok);
    Hero& hero = *rig.hero;
    hero.SetFacing(Vec2(0, 1));
    const Vec2 start = hero.Position();
    REQUIRE(rig.combat.RequestDodge(Vec2()));
    CHECK(hero.Position().x == doctest::Approx(start.x));
    CHECK(hero.Position().y == doctest::Approx(start.y + 2.6));
    const auto started = EventsOf<EvDodgeStarted>(rig.h.events);
    REQUIRE(started.size() == 1);
    CHECK(started[0].cooldownMs == 900);
    CHECK(started[0].invulnerabilityMs == 220);
    CHECK(rig.combat.Dodge().IsInvulnerable(rig.h.clock.NowMs()));
    CHECK_FALSE(rig.combat.RequestDodge(Vec2(1, 0)));  // cooldown
    rig.Step(54);                                       // 900 ms
    CHECK(rig.combat.RequestDodge(Vec2(1, 0)));
  }

  TEST_CASE("CombatSystem: hero death (13.3 + 5.1.1) - Dying once, penalty + echo, respawn after 1100 ms") {
    CombatRig rig(ClassId::Warrior);
    REQUIRE(rig.ok);
    Hero& hero = *rig.hero;
    rig.rewards.GrantExp(hero.ExpToNext(), ExpSource::Debug);  // L2
    for (int i = 0; i < 3; ++i) rig.rewards.GrantExp(hero.ExpToNext(), ExpSource::Debug);
    REQUIRE(hero.Level() == 5);
    rig.rewards.ChangeGold(1000, GoldReason::Debug);
    hero.GetSpirit().Gain(40);
    rig.h.events.Clear();
    const Vec2 deathPos = hero.Position() + Vec2(3, 0);
    hero.SetPosition(deathPos);
    hero.SetHp(0);
    rig.combat.KillHero(HeroDeathCause::Other);
    CHECK(hero.Life() == HeroLife::Dying);
    CHECK(hero.Gold() == 900);
    CHECK(hero.GetSpirit().Value() == 0);
    CHECK(rig.soul.State().Has());
    CHECK(rig.soul.State().Echo().gold == 100);
    CHECK(test::CountEvents<EvHeroDied>(rig.h.events) == 1);
    rig.combat.KillHero(HeroDeathCause::Other);  // no-op
    CHECK(test::CountEvents<EvHeroDied>(rig.h.events) == 1);
    CHECK(hero.Gold() == 900);
    // Dying: skills rejected without buffering, no dodge.
    CHECK(rig.combat.RequestSkill(rig.SkillIndex("shield_wall"), SkillAim{}) == SkillRequestResult::Blocked);
    CHECK_FALSE(rig.combat.Buffer().HasPending());
    CHECK_FALSE(rig.combat.RequestDodge(Vec2(1, 0)));
    const double t0 = rig.h.clock.NowMs();
    rig.StepBefore(t0 + 1100);
    CHECK(hero.Life() == HeroLife::Dying);
    rig.StepTo(t0 + 1100);
    CHECK(hero.Life() == HeroLife::Alive);
    CHECK(hero.Hp() == hero.MaxHp());
    CHECK(hero.Position() == rig.zone.CampPosition(0));
    CHECK(test::CountEvents<EvHeroRespawned>(rig.h.events) == 1);
    // Walk back onto the echo: reclaimed (gold back).
    hero.SetPosition(deathPos + Vec2(1, 0));
    rig.Step();
    CHECK_FALSE(rig.soul.State().Has());
    CHECK(hero.Gold() == 1000);
  }

  TEST_CASE("CombatSystem: ResolvePendingDeath respawns at once without a second penalty; DoT kills bypass deathSave") {
    CombatRig rig(ClassId::Warrior);
    REQUIRE(rig.ok);
    Hero& hero = *rig.hero;
    rig.h.ctx.equip.Ref(Stat::DeathSave) = 1;
    rig.status.Apply(kHeroEntityId, StatusType::Burn, 100000, 3000, 0, rig.h.clock.NowMs());
    rig.StepTo(rig.h.clock.NowMs() + 1000);
    CHECK(hero.Life() == HeroLife::Dying);  // the tick killed the hero (no deathSave)
    rig.combat.ResolvePendingDeath();
    CHECK(hero.Life() == HeroLife::Alive);
    CHECK(hero.Hp() == hero.MaxHp());
    CHECK(rig.h.timers.Empty());
    CHECK_FALSE(rig.status.Has(kHeroEntityId, StatusType::Burn));
  }

  TEST_CASE("CombatSystem: DamageHero with a deathSave proc (T11: re-arms 60 s later on the sim clock)") {
    CombatRig rig(ClassId::Warrior);
    REQUIRE(rig.ok);
    Hero& hero = *rig.hero;
    rig.h.ctx.equip.Ref(Stat::DeathSave) = 1;
    HeroHitRequest r;
    r.amount = 100000;
    r.allowDeathSave = true;
    rig.combat.DamageHero(r);
    CHECK(hero.Life() == HeroLife::Alive);
    CHECK(hero.Hp() == std::floor(hero.MaxHp() * 0.3));
    CHECK(hero.deathSaveReadyAtMs == doctest::Approx(rig.h.clock.NowMs() + 60000));
    rig.combat.DamageHero(r);  // not re-armed yet
    CHECK(hero.Life() == HeroLife::Dying);
  }

  TEST_CASE("CombatSystem: Unyielding procs below 30 % HP once per cooldown; dual wield needs the inventory") {
    CombatRig rig(ClassId::Warrior);
    REQUIRE(rig.ok);
    Hero& hero = *rig.hero;
    const int32_t uny = rig.SkillIndex("unyielding");
    hero.Skills().SetLevel(uny, 1);
    hero.SetHp(hero.MaxHp() * 0.2);
    rig.Step();
    CHECK(hero.Buffs().RawSum(BuffStat::DamageReduction) == doctest::Approx(0.35));
    CHECK(hero.Skills().ReadyAtMs(uny) == doctest::Approx(rig.h.clock.NowMs() + 60000));
    rig.Step(10);
    CHECK(hero.Buffs().RawSum(BuffStat::DamageReduction) == doctest::Approx(0.35));  // one proc
  }

  TEST_CASE("CombatSystem: combat state is a true debounce (A2) and the freeze clears the buffer (F1)") {
    CombatRig rig(ClassId::Warrior);
    REQUIRE(rig.ok);
    CHECK_FALSE(rig.combat.InCombat());
    rig.combat.RequestSkill(rig.SkillIndex("slash"), SkillAim{});  // no target: buffered
    CHECK(rig.combat.Buffer().HasPending());
    rig.combat.OnFreezeBegin(false);
    CHECK_FALSE(rig.combat.Buffer().HasPending());
  }

  // ===================================================================================================================
  // CombatSystem runtime with monsters (needs MonsterSystem::Spawn / ApplyDamage)
  // ===================================================================================================================
  TEST_CASE("runtime: basic attack lands on the contact beat with the class profile and kill credit") {
    CombatRig rig(ClassId::Warrior);
    REQUIRE(rig.ok);
    Hero& hero = *rig.hero;
    const EntityId slime = rig.Spawn("slime_green", hero.Position() + Vec2(1, 0));
    REQUIRE_MONSTER(slime);
    rig.combat.SetAttackTarget(slime, false);
    rig.Step();
    const double swing = rig.h.clock.NowMs();
    const auto anims = EventsOf<EvPlayAnim>(rig.h.events);
    REQUIRE_FALSE(anims.empty());
    CHECK(anims.back().action == AnimAction::Attack);
    CHECK(anims.back().contactMs == 308);
    CHECK(test::CountEvents<EvHit>(rig.h.events) == 0);
    rig.StepTo(swing + 308);
    const auto hits = EventsOf<EvHit>(rig.h.events);
    REQUIRE(hits.size() == 1);
    CHECK(hits[0].target == slime);
    CHECK(hits[0].impactBurst);
    CHECK(hits[0].impactColor == 0xffd98au);
    CHECK(hits[0].attackerStopMs == D().Combat().hitFeedback.Profile(hits[0].weight).attackerStopMs);
    CHECK(hero.GetSpirit().Value() > 0);  // Spirit 'hit'
    // Keep swinging until it dies: exp + gold + target cleanup.
    rig.StepTo(swing + 5000);
    const MonsterInstance* m = rig.monsters.Find(slime);
    CHECK((m == nullptr || !m->IsAlive()));
    CHECK(hero.Exp() >= 12);
    CHECK(hero.Gold() >= 2);
    CHECK(rig.combat.AttackTarget() == kNoEntity);
    CHECK(rig.combat.LastKillReward().monster == slime);
  }

  TEST_CASE("runtime: monster swing - contact 250 ms, whiff out of reach, F2 cancel, dodge i-frames") {
    CombatRig rig(ClassId::Warrior);
    REQUIRE(rig.ok);
    Hero& hero = *rig.hero;
    const EntityId g = rig.Spawn("goblin", hero.Position() + Vec2(1, 0));
    REQUIRE_MONSTER(g);
    hero.lastAttackMs = 1e18;  // the hero does not swing back in this test
    rig.monsters.Find(g)->state = MonsterState::Attack;
    const auto heroHits = [&]() {
      std::vector<EvHit> out;
      for (const EvHit& e : EventsOf<EvHit>(rig.h.events)) {
        if (e.target == kHeroEntityId) out.push_back(e);
      }
      return out;
    };
    // Swing 1 lands at its 250 ms contact (HP regen runs meanwhile, so the hit is read from its event).
    REQUIRE(rig.StepUntil([&] { return rig.combat.IsWindingUp(g); }));
    double swing = rig.h.clock.NowMs();
    const auto anims = EventsOf<EvPlayAnim>(rig.h.events);
    REQUIRE_FALSE(anims.empty());
    CHECK(anims.back().entity == g);
    CHECK(anims.back().contactMs == 250);
    CHECK(anims.back().windupMs == doctest::Approx(155));
    rig.ScriptNoDodge();
    rig.StepBefore(swing + 250);
    CHECK(heroHits().empty());
    rig.StepTo(swing + 250);
    std::vector<EvHit> hits = heroHits();
    REQUIRE(hits.size() == 1);
    CHECK(hits[0].amount == 6);  // goblin -> warrior L1 (combat 2.1)
    CHECK(hits[0].melee);
    CHECK(hits[0].source == g);
    CHECK(hits[0].attackerStopMs == MonsterAttackerStopMs(D().Combat().hitFeedback, hits[0].weight));
    CHECK(test::CountEvents<EvCameraShake>(rig.h.events) >= 1);  // player-hit shake
    // Swing 2: the hero steps out of reach (1.5 x 1.35 + 0.5 = 2.525) during the wind-up -> whiff.
    REQUIRE(rig.StepUntil([&] { return rig.combat.IsWindingUp(g); }));
    swing = rig.h.clock.NowMs();
    const Vec2 near = hero.Position();
    hero.SetPosition(near + Vec2(-3, 0));
    rig.StepTo(swing + 300);
    CHECK(heroHits().size() == 1);
    hero.SetPosition(near);
    // Swing 3: a cinematic freeze cancels the pending contact (F2); lastAttackMs keeps the cancelled swing.
    REQUIRE(rig.StepUntil([&] { return rig.combat.IsWindingUp(g); }));
    swing = rig.h.clock.NowMs();
    rig.combat.OnFreezeBegin(true);
    CHECK(test::CountEvents<EvMonsterAttackCancelled>(rig.h.events) == 1);
    CHECK(rig.monsters.Find(g)->lastAttackMs == doctest::Approx(swing));
    rig.StepTo(swing + 300);
    CHECK(heroHits().size() == 1);
    // Swing 4: a dodge 50 ms into the wind-up covers the contact (i-frames 220 ms): a perfect evade, Spirit 'dodge'.
    REQUIRE(rig.StepUntil([&] { return rig.combat.IsWindingUp(g); }));
    swing = rig.h.clock.NowMs();
    rig.Step(3);
    REQUIRE(rig.combat.RequestDodge(Vec2(0, -1)));
    hero.SetPosition(near);  // keep the hero in reach for the test
    const double sp = hero.GetSpirit().Value();
    rig.StepTo(swing + 300);
    hits = heroHits();
    REQUIRE(hits.size() == 2);
    CHECK(hits[1].dodged);
    CHECK(hits[1].iframeAvoided);
    CHECK(hero.GetSpirit().Value() > sp);
  }

  TEST_CASE("runtime: every active skill executes with its beat (40-skill table, classes 10 / combat 6.7)") {
    for (const ClassDef& cd : D().Classes().classes) {
      for (const SkillDef& s : cd.skills) {
        CAPTURE(s.id);
        CombatRig rig(cd.cls, 11);
        REQUIRE(rig.ok);
        Hero& hero = *rig.hero;
        const int32_t idx = rig.SkillIndex(s.id);
        hero.Skills().SetLevel(idx, 1);
        hero.SetMana(hero.MaxMana());
        if (s.passive) {
          CHECK(rig.combat.RequestSkill(idx, SkillAim{}) == SkillRequestResult::Passive);
          continue;
        }
        const Vec2 heroPos = hero.Position();
        const EntityId target = rig.Spawn("goblin_chief", heroPos + Vec2(1, 0));  // 160 HP: survives one hit
        REQUIRE_MONSTER(target);
        rig.monsters.Find(target)->def.attackSpeedMs = 1e9;  // keep the monster passive
        rig.combat.SetAttackTarget(target, false);
        hero.lastAttackMs = 1e18;  // no basic attacks in this test
        const double t0 = rig.h.clock.NowMs();
        SkillAim aim;
        aim.hasPoint = true;
        aim.point = heroPos + Vec2(-4, 0);
        const SkillRequestResult res = rig.combat.RequestSkill(idx, aim);
        CHECK(res == SkillRequestResult::Executed);
        const auto anims = EventsOf<EvPlayAnim>(rig.h.events);
        REQUIRE(anims.size() >= 1);
        const AnimTimingTable& at = D().Combat().anim;
        const AssetManifest& man = D().Assets();
        const AnimRig heroRig = HeroRig(cd.cls);
        const double beat = s.animKind == SkillAnimKind::Attack
                                ? ComputeAttackTiming(at, man, cd.id, heroRig, hero.Derived().attackSpeedMs).contactMs
                                : ComputeCastTiming(at, man, cd.id, heroRig).contactMs;
        CHECK(anims[0].contactMs == beat);
        CHECK(anims[0].action == (s.animKind == SkillAnimKind::Attack ? AnimAction::Attack : AnimAction::Cast));
        if (!s.instantRelease) {
          rig.StepBefore(t0 + beat);
          CHECK(test::CountEvents<EvHit>(rig.h.events) == 0);  // nothing before the beat
          if (s.hasBuff && s.execKind == SkillExecKind::Buff) CHECK_FALSE(hero.Buffs().Has(s.buff.stat));
        }
        rig.StepTo(t0 + beat + 1200);
        const auto hits = EventsOf<EvHit>(rig.h.events);
        size_t onTarget = 0;
        for (const EvHit& hh : hits) onTarget += (hh.target == target && hh.skillId == s.id) ? 1 : 0;
        switch (s.execKind) {
          case SkillExecKind::Teleport:
            CHECK(hero.Position() != heroPos);
            break;
          case SkillExecKind::ShadowStep:
            CHECK(hero.Buffs().Has(BuffStat::CritBonus));
            CHECK(hero.Position() != heroPos);
            break;
          case SkillExecKind::DeathMark:
            CHECK(rig.monsters.Find(target)->buffs.Has(BuffStat::DamageAmplify));
            CHECK(onTarget == 1);
            break;
          case SkillExecKind::SlowTrap:
            CHECK(rig.status.Has(target, StatusType::Slow));
            CHECK(onTarget == 1);
            break;
          case SkillExecKind::Buff:
            CHECK(hero.Buffs().Has(s.buff.stat));
            CHECK(onTarget == 0);
            break;
          case SkillExecKind::Aoe:
          case SkillExecKind::Single:
            CHECK(onTarget >= 1);
            if (s.port.persistentGround && s.port.groundTrigger == GroundTrigger::Periodic) {
              CHECK(onTarget >= 3);  // ticks over the first 1.2 s
            }
            break;
        }
      }
    }
  }

  // ===================================================================================================================
  // Runtime: C4 upgrades, delays, Q7 / Q9 / Q16 / Q18 / Q19 fixes, kill pipeline, auto-battle, ranged monsters
  // ===================================================================================================================
  TEST_CASE("runtime C4: Charge dashes to melee range and hits at the dash end") {
    CombatRig rig(ClassId::Warrior);
    REQUIRE(rig.ok);
    Hero& hero = *rig.hero;
    const Vec2 start = hero.Position();
    const EntityId t = rig.Spawn("goblin_chief", start + Vec2(4, 0));
    REQUIRE_MONSTER(t);
    const int32_t charge = rig.SkillIndex("charge");
    hero.Skills().SetLevel(charge, 1);
    hero.lastAttackMs = 1e18;
    rig.combat.SetAttackTarget(t, false);
    const double t0 = rig.h.clock.NowMs();
    REQUIRE(rig.combat.RequestSkill(charge, SkillAim{}) == SkillRequestResult::Executed);
    rig.StepTo(t0 + 334);
    const auto dash = EventsOf<EvHeroDash>(rig.h.events);
    REQUIRE_FALSE(dash.empty());
    CHECK(dash[0].phase == EvHeroDash::Phase::Started);
    CHECK(dash[0].to.x == doctest::Approx(start.x + 2.5));
    CHECK(dash[0].durationMs == 250);
    CHECK(test::CountEvents<EvHit>(rig.h.events) == 0);
    rig.StepTo(t0 + 334 + 250 + 20);
    size_t hits = 0;
    for (const EvHit& h : EventsOf<EvHit>(rig.h.events)) hits += h.skillId == "charge" ? 1 : 0;
    CHECK(hits == 1);
    CHECK(hero.Position().x == doctest::Approx(start.x + 2.5));
  }

  TEST_CASE("runtime C4: chain lightning jumps target to target 55 ms apart; meteor lands 300 ms after release") {
    CombatRig rig(ClassId::Mage);
    REQUIRE(rig.ok);
    Hero& hero = *rig.hero;
    const Vec2 p = hero.Position();
    const EntityId a = rig.Spawn("goblin_chief", p + Vec2(3, 0));
    REQUIRE_MONSTER(a);
    const EntityId b = rig.Spawn("goblin_chief", p + Vec2(1, 0));
    const EntityId c = rig.Spawn("goblin_chief", p + Vec2(2, 1));
    const int32_t chain = rig.SkillIndex("chain_lightning");
    hero.Skills().SetLevel(chain, 1);
    const double t0 = rig.h.clock.NowMs();
    REQUIRE(rig.combat.RequestSkill(chain, SkillAim{}) == SkillRequestResult::Executed);
    std::vector<std::pair<EntityId, double>> order;
    for (int i = 0; i < 40; ++i) {
      rig.Step();
      for (const EvHit& h : EventsOf<EvHit>(rig.h.events)) {
        if (h.skillId != "chain_lightning") continue;
        bool seen = false;
        for (const auto& o : order) seen = seen || o.first == h.target;
        if (!seen) order.push_back({h.target, rig.h.clock.NowMs()});
      }
    }
    REQUIRE(order.size() == 3);
    CHECK(order[0].first == b);  // nearest to the hero first, then nearest to the previous link
    CHECK(order[1].first == c);
    CHECK(order[2].first == a);
    CHECK(order[0].second == doctest::Approx(t0 + 230).epsilon(0.1));
    CHECK(order[1].second - order[0].second >= 55 - kSimStepMs);
    CHECK(order[2].second - order[0].second >= 110 - kSimStepMs);
    const auto vfx = EventsOf<EvSkillVfx>(rig.h.events);
    REQUIRE_FALSE(vfx.empty());
    CHECK(vfx.back().points.size() == 3);
    CHECK(vfx.back().staggerMs == 55);
    // Meteor: ground anchor on the lock, one batch 300 ms after the release beat.
    CombatRig m(ClassId::Mage);
    const EntityId target = m.Spawn("goblin_chief", m.hero->Position() + Vec2(3, 0));
    const int32_t meteor = m.SkillIndex("meteor");
    m.hero->Skills().SetLevel(meteor, 1);
    m.combat.SetAttackTarget(target, false);
    m.hero->lastAttackMs = 1e18;
    const double m0 = m.h.clock.NowMs();
    REQUIRE(m.combat.RequestSkill(meteor, SkillAim{}) == SkillRequestResult::Executed);
    m.StepBefore(m0 + 530);
    CHECK(test::CountEvents<EvHit>(m.h.events) == 0);
    m.StepTo(m0 + 530);
    CHECK(test::CountEvents<EvHit>(m.h.events) >= 1);
  }

  TEST_CASE("runtime C4: multishot fans in a 50 degree cone toward the lock with per-target arrow delays") {
    CombatRig rig(ClassId::Rogue);
    REQUIRE(rig.ok);
    Hero& hero = *rig.hero;
    const Vec2 p = hero.Position();
    const EntityId lock = rig.Spawn("goblin_chief", p + Vec2(2, 0));
    REQUIRE_MONSTER(lock);
    const EntityId side = rig.Spawn("goblin_chief", p + Vec2(0, 2));    // 90 degrees off: outside the fan
    const EntityId inFan = rig.Spawn("goblin_chief", p + Vec2(3, 0));  // on the aim, at the radius (3)
    rig.combat.SetAttackTarget(lock, false);
    hero.lastAttackMs = 1e18;
    const int32_t ms = rig.SkillIndex("multishot");
    const double t0 = rig.h.clock.NowMs();
    REQUIRE(rig.combat.RequestSkill(ms, SkillAim{}) == SkillRequestResult::Executed);
    rig.StepTo(t0 + 246);
    CHECK(test::CountEvents<EvHit>(rig.h.events) == 0);  // arrows still flying (2 tiles: 79 ms)
    rig.StepTo(t0 + 246 + 300);
    std::vector<EntityId> hit;
    for (const EvHit& h : EventsOf<EvHit>(rig.h.events)) {
      if (h.skillId == "multishot") hit.push_back(h.target);
    }
    CHECK(std::find(hit.begin(), hit.end(), lock) != hit.end());
    CHECK(std::find(hit.begin(), hit.end(), inFan) != hit.end());
    CHECK(std::find(hit.begin(), hit.end(), side) == hit.end());
  }

  TEST_CASE("runtime C4: fire wall burns 6 ticks over 3 s with 1/6 of the hit; an armed trap waits for a monster") {
    CombatRig rig(ClassId::Mage);
    REQUIRE(rig.ok);
    Hero& hero = *rig.hero;
    const EntityId t = rig.Spawn("goblin_chief", hero.Position() + Vec2(3, 0));
    REQUIRE_MONSTER(t);
    rig.combat.SetAttackTarget(t, false);
    hero.lastAttackMs = 1e18;
    const int32_t fw = rig.SkillIndex("fire_wall");
    hero.Skills().SetLevel(fw, 1);
    const double t0 = rig.h.clock.NowMs();
    REQUIRE(rig.combat.RequestSkill(fw, SkillAim{}) == SkillRequestResult::Executed);
    rig.StepTo(t0 + 230 + 3100);
    std::vector<double> amounts;
    for (const EvHit& h : EventsOf<EvHit>(rig.h.events)) {
      if (h.skillId == "fire_wall" && !h.dodged) amounts.push_back(h.amount);
    }
    CHECK(amounts.size() == 6);
    for (double a : amounts) CHECK(a <= 6);  // full hit 20 (x1.55 crit 31): a sixth each
    const auto started = EventsOf<EvGroundEffectStarted>(rig.h.events);
    REQUIRE(started.size() == 1);
    CHECK(started[0].trigger == GroundTrigger::Periodic);
    CHECK(started[0].center == rig.monsters.Find(t)->pos);  // the ground anchor
    CHECK(test::CountEvents<EvGroundEffectEnded>(rig.h.events) == 1);
    // Rogue explosive trap: armed at the hero's feet, fires when a monster steps inside its radius.
    CombatRig r(ClassId::Rogue);
    const int32_t trap = r.SkillIndex("explosive_trap");
    r.hero->lastAttackMs = 1e18;
    const double r0 = r.h.clock.NowMs();
    REQUIRE(r.combat.RequestSkill(trap, SkillAim{}) == SkillRequestResult::Executed);
    r.StepTo(r0 + 600);
    const auto armed = EventsOf<EvGroundEffectStarted>(r.h.events);
    REQUIRE(armed.size() == 1);
    CHECK(armed[0].trigger == GroundTrigger::Armed);
    CHECK(test::CountEvents<EvGroundEffectTriggered>(r.h.events) == 0);
    const EntityId walker = r.Spawn("goblin_chief", r.hero->Position() + Vec2(1, 1));
    r.Step(2);
    CHECK(test::CountEvents<EvGroundEffectTriggered>(r.h.events) == 1);
    size_t trapHits = 0;
    for (const EvHit& h : EventsOf<EvHit>(r.h.events))
      trapHits += (h.skillId == "explosive_trap" && h.target == walker);
    CHECK(trapHits == 1);
    const auto ended = EventsOf<EvGroundEffectEnded>(r.h.events);
    REQUIRE(ended.size() == 1);
    CHECK(ended[0].triggered);
  }

  TEST_CASE("runtime: fireball flies target-locked; a target that dies in flight takes nothing") {
    CombatRig rig(ClassId::Mage);
    REQUIRE(rig.ok);
    Hero& hero = *rig.hero;
    const EntityId t = rig.Spawn("goblin_chief", hero.Position() + Vec2(5, 0));
    REQUIRE_MONSTER(t);
    rig.combat.SetAttackTarget(t, false);
    hero.lastAttackMs = 1e18;
    const int32_t fb = rig.SkillIndex("fireball");
    const double t0 = rig.h.clock.NowMs();
    REQUIRE(rig.combat.RequestSkill(fb, SkillAim{}) == SkillRequestResult::Executed);
    rig.StepTo(t0 + 230);
    const auto launched = EventsOf<EvProjectileLaunched>(rig.h.events);
    REQUIRE(launched.size() == 1);
    CHECK(launched[0].travelMs == 300);  // clamp(5 x 36 x 1.5 = 270, 300, 600)
    CHECK(launched[0].target == t);
    rig.StepBefore(launched[0].launchMs + 300);
    CHECK(test::CountEvents<EvHit>(rig.h.events) == 0);
    rig.StepTo(launched[0].launchMs + 300);
    CHECK(test::CountEvents<EvHit>(rig.h.events) == 1);
    // Second cast: the target dies before arrival.
    hero.Skills().ResetCooldowns();
    REQUIRE(rig.combat.RequestSkill(fb, SkillAim{}) == SkillRequestResult::Executed);
    rig.StepTo(rig.h.clock.NowMs() + 240);
    MonsterHitRequest kill;
    kill.monster = t;
    kill.amount = 1e6;
    rig.combat.DamageMonster(kill);
    const size_t hitsBefore = test::CountEvents<EvHit>(rig.h.events);
    rig.Step(30);
    CHECK(test::CountEvents<EvHit>(rig.h.events) == hitsBefore);
  }

  TEST_CASE("runtime: FIX Q7 retarget at release - nearest in reach, else fizzle") {
    CombatRig rig(ClassId::Warrior);
    REQUIRE(rig.ok);
    Hero& hero = *rig.hero;
    const Vec2 p = hero.Position();
    const EntityId a = rig.Spawn("goblin_chief", p + Vec2(1, 0));
    REQUIRE_MONSTER(a);
    const EntityId b = rig.Spawn("goblin_chief", p + Vec2(0, 1));
    rig.combat.SetAttackTarget(a, false);
    hero.lastAttackMs = 1e18;
    const int32_t slash = rig.SkillIndex("slash");
    const double t0 = rig.h.clock.NowMs();
    REQUIRE(rig.combat.RequestSkill(slash, SkillAim{}) == SkillRequestResult::Executed);
    rig.Step(5);
    MonsterHitRequest kill;
    kill.monster = a;
    kill.amount = 1e6;
    rig.combat.DamageMonster(kill);
    rig.StepTo(t0 + 320);
    size_t onB = 0;
    for (const EvHit& h : EventsOf<EvHit>(rig.h.events)) onB += (h.skillId == "slash" && h.target == b);
    CHECK(onB == 1);
    // The only monster left is far away: the released slash fizzles (cost already paid).
    CombatRig far(ClassId::Warrior);
    const EntityId near = far.Spawn("goblin_chief", far.hero->Position() + Vec2(1, 0));
    const EntityId distant = far.Spawn("goblin_chief", far.hero->Position() + Vec2(10, 0));
    far.combat.SetAttackTarget(near, false);
    far.hero->lastAttackMs = 1e18;
    REQUIRE(far.combat.RequestSkill(far.SkillIndex("slash"), SkillAim{}) == SkillRequestResult::Executed);
    far.Step(5);
    MonsterHitRequest k2;
    k2.monster = near;
    k2.amount = 1e6;
    far.combat.DamageMonster(k2);
    far.Step(30);
    size_t onDistant = 0;
    for (const EvHit& h : EventsOf<EvHit>(far.h.events)) onDistant += (h.target == distant);
    CHECK(onDistant == 0);
  }

  TEST_CASE("runtime: FIX Q9 a dodged skill hit applies nothing; statuses on landed hits") {
    CombatRig rig(ClassId::Mage);
    REQUIRE(rig.ok);
    Hero& hero = *rig.hero;
    const EntityId t = rig.Spawn("goblin_chief", hero.Position() + Vec2(2, 0));
    REQUIRE_MONSTER(t);
    rig.monsters.Find(t)->stats.dex = 100;  // 30 % dodge
    rig.combat.SetAttackTarget(t, false);
    hero.lastAttackMs = 1e18;
    const int32_t bz = rig.SkillIndex("blizzard");
    hero.Skills().SetLevel(bz, 1);
    const double t0 = rig.h.clock.NowMs();
    REQUIRE(rig.combat.RequestSkill(bz, SkillAim{}) == SkillRequestResult::Executed);
    rig.StepBefore(t0 + 230);
    rig.h.rng.Get(RngStream::Combat).Script({0.0});  // dodge
    rig.StepTo(t0 + 230);
    const auto hits = EventsOf<EvHit>(rig.h.events);
    REQUIRE(hits.size() == 1);
    CHECK(hits[0].dodged);
    CHECK_FALSE(rig.status.Has(t, StatusType::Freeze));
    CHECK(rig.monsters.Find(t)->hp == rig.monsters.Find(t)->maxHp);
    // Landed: the freeze applies (no chance roll).
    hero.Skills().ResetCooldowns();
    rig.monsters.Find(t)->stats.dex = 0;
    const double t1 = rig.h.clock.NowMs();
    REQUIRE(rig.combat.RequestSkill(bz, SkillAim{}) == SkillRequestResult::Executed);
    rig.StepTo(t1 + 230);
    CHECK(rig.status.Has(t, StatusType::Freeze));
    CHECK(test::CountEvents<EvStatusApplied>(rig.h.events) == 1);
  }

  TEST_CASE("runtime: FIX Q16 mana shield drains mana; FIX Q18 shadow step crit buff consumed; FIX Q19 marks expire") {
    CombatRig rig(ClassId::Mage);
    REQUIRE(rig.ok);
    Hero& hero = *rig.hero;
    const EntityId g = rig.Spawn("goblin", hero.Position() + Vec2(1, 0));
    REQUIRE_MONSTER(g);
    hero.lastAttackMs = 1e18;
    ActiveBuff shield;
    shield.stat = BuffStat::ManaShield;
    shield.value = 0.3;
    shield.durationMs = 60000;
    shield.startMs = rig.h.clock.NowMs();
    hero.Buffs().Add(shield);
    rig.monsters.Find(g)->state = MonsterState::Attack;
    REQUIRE(rig.StepUntil([&] { return rig.combat.IsWindingUp(g); }));
    const double swing = rig.h.clock.NowMs();
    rig.StepBefore(swing + 250);
    const double mana = hero.Mana();
    rig.ScriptNoDodge();
    rig.StepTo(swing + 250);
    CHECK(hero.Mana() == doctest::Approx(mana - 2).epsilon(0.01));  // 7 -> 5 HP + 2 mana (regen < 0.05 per step)
    // Rogue shadow step: the crit buff turns into crit points and is consumed by the next landed hit.
    CombatRig r(ClassId::Rogue);
    const EntityId t = r.Spawn("goblin_chief", r.hero->Position() + Vec2(3, 0));
    r.combat.SetAttackTarget(t, false);
    r.hero->lastAttackMs = 1e18;
    const int32_t ss = r.SkillIndex("shadow_step");
    r.hero->Skills().SetLevel(ss, 1);
    REQUIRE(r.combat.RequestSkill(ss, SkillAim{}) == SkillRequestResult::Executed);
    CHECK(r.hero->Buffs().Has(BuffStat::CritBonus));
    CHECK(r.combat.HeroCombatant().extraCritPercent == doctest::Approx(30));
    r.hero->lastAttackMs = 0;  // swing now
    r.Step(20);
    CHECK_FALSE(r.hero->Buffs().Has(BuffStat::CritBonus));
    // Death mark on a monster expires with its duration (8000 ms at L1).
    CombatRig d(ClassId::Rogue);
    const EntityId mk = d.Spawn("goblin_chief", d.hero->Position() + Vec2(2, 0));
    d.combat.SetAttackTarget(mk, false);
    d.hero->lastAttackMs = 1e18;
    const int32_t dm = d.SkillIndex("death_mark");
    d.hero->Skills().SetLevel(dm, 1);
    const double d0 = d.h.clock.NowMs();
    REQUIRE(d.combat.RequestSkill(dm, SkillAim{}) == SkillRequestResult::Executed);
    d.StepTo(d0 + 246);
    REQUIRE(d.monsters.Find(mk)->buffs.Has(BuffStat::DamageAmplify));
    CHECK(d.monsters.Find(mk)->buffs.RawSum(BuffStat::DamageAmplify) == doctest::Approx(0.25));
    d.StepTo(d0 + 246 + 8000 + 20);
    CHECK_FALSE(d.monsters.Find(mk)->buffs.Has(BuffStat::DamageAmplify));
  }

  TEST_CASE("runtime: kill pipeline step 1 - exp bonus, gold, killHeal, DoT kill credit, elite slow motion") {
    CombatRig rig(ClassId::Warrior);
    REQUIRE(rig.ok);
    Hero& hero = *rig.hero;
    rig.h.ctx.equip.Ref(Stat::ExpBonus) = 50;
    rig.h.ctx.equip.Ref(Stat::KillHealPercent) = 10;
    const EntityId s = rig.Spawn("slime_green", hero.Position() + Vec2(5, 0));
    REQUIRE_MONSTER(s);
    hero.SetHp(50);
    rig.monsters.Find(s)->hp = 1;
    rig.status.Apply(s, StatusType::Burn, 5, 3000, kHeroEntityId, rig.h.clock.NowMs());
    rig.StepTo(rig.h.clock.NowMs() + 1000);
    CHECK_FALSE(rig.monsters.Find(s)->IsAlive());
    CHECK(rig.combat.LastKillReward().exp == 18);  // floor(12 x 1.5)
    CHECK(hero.Exp() == 18);
    CHECK(hero.Gold() >= 2);
    CHECK(hero.Gold() <= 4);
    CHECK(hero.Hp() >= 50 + 15);  // + floor(150 x 10 %)
    CHECK_FALSE(rig.status.Has(s, StatusType::Burn));  // the kill pipeline cleared it
    const auto hits = EventsOf<EvHit>(rig.h.events);
    REQUIRE_FALSE(hits.empty());
    CHECK(hits.back().tick);
    CHECK(hits.back().killed);
    // An elite killed by a basic attack: S6 slow motion (0.4 for 200 ms).
    CombatRig e(ClassId::Warrior);
    const EntityId chief = e.Spawn("goblin_chief", e.hero->Position() + Vec2(1, 0));
    e.monsters.Find(chief)->hp = 1;
    e.combat.SetAttackTarget(chief, false);
    e.Step(25);
    const auto slow = EventsOf<EvSlowMotion>(e.h.events);
    REQUIRE(slow.size() == 1);
    CHECK(slow[0].timeScale == doctest::Approx(0.4));
    CHECK(slow[0].realDurationMs == 200);
    CHECK(e.h.clock.Dilation() == doctest::Approx(0.4));
  }

  TEST_CASE("runtime: taunt roar taunts and pulls idle monsters; combat state on/off with a true debounce (A2)") {
    CombatRig rig(ClassId::Warrior);
    REQUIRE(rig.ok);
    Hero& hero = *rig.hero;
    const EntityId g = rig.Spawn("goblin", hero.Position() + Vec2(2, 0));
    REQUIRE_MONSTER(g);
    const EntityId far = rig.Spawn("goblin", hero.Position() + Vec2(8, 0));
    hero.lastAttackMs = 1e18;
    const int32_t tr = rig.SkillIndex("taunt_roar");
    hero.Skills().SetLevel(tr, 1);
    const double t0 = rig.h.clock.NowMs();
    REQUIRE(rig.combat.RequestSkill(tr, SkillAim{}) == SkillRequestResult::Executed);
    rig.StepTo(t0 + 334);
    CHECK(rig.monsters.Find(g)->state == MonsterState::Chase);
    CHECK(rig.monsters.Find(g)->buffs.Has(BuffStat::Taunted));
    CHECK(rig.monsters.Find(far)->state == MonsterState::Idle);
    CHECK(hero.Buffs().Has(BuffStat::DefenseBonus));
    // Combat state: a monster in attack -> on at once; off 1500 ms after the fight stops; a fight resuming inside
    // the debounce cancels it.
    rig.monsters.Find(g)->def.attackSpeedMs = 1e9;
    rig.monsters.Find(g)->state = MonsterState::Attack;
    rig.Step();
    CHECK(rig.combat.InCombat());
    rig.monsters.Find(g)->state = MonsterState::Idle;
    rig.Step();
    const double stop = rig.h.clock.NowMs();
    rig.StepTo(stop + 1000);
    CHECK(rig.combat.InCombat());
    rig.monsters.Find(g)->state = MonsterState::Attack;
    rig.Step();
    rig.monsters.Find(g)->state = MonsterState::Idle;
    rig.StepTo(stop + 1600);
    CHECK(rig.combat.InCombat());  // the first debounce was cancelled
    rig.StepTo(stop + 1000 + 1600);
    CHECK_FALSE(rig.combat.InCombat());
    const auto changes = EventsOf<EvCombatStateChanged>(rig.h.events);
    REQUIRE(changes.size() == 2);
    CHECK(changes[0].inCombat);
    CHECK_FALSE(changes[1].inCombat);
  }

  TEST_CASE("runtime: auto-battle locks the nearest monster in its aggro range and casts the first usable skill (C1)") {
    CombatRig rig(ClassId::Warrior);
    REQUIRE(rig.ok);
    Hero& hero = *rig.hero;
    const EntityId g = rig.Spawn("goblin", hero.Position() + Vec2(4, 0));  // aggro range 5
    REQUIRE_MONSTER(g);
    hero.autoCombat = true;
    hero.lastAttackMs = 1e18;
    rig.Step();
    CHECK(rig.combat.AttackTarget() == g);
    // slash is out of reach (4 > 1.5 + 1): the C1 fix skips it and casts shield_wall, the first usable skill.
    const auto used = EventsOf<EvSkillUsed>(rig.h.events);
    REQUIRE_FALSE(used.empty());
    CHECK(used[0].skillId == "shield_wall");
  }

  TEST_CASE("runtime: a ranged monster fires a bolt at contact; a cinematic destroys it in flight (F2)") {
    CombatRig rig(ClassId::Warrior);
    REQUIRE(rig.ok);
    Hero& hero = *rig.hero;
    const EntityId sh = rig.Spawn("miniboss_goblin_shaman", hero.Position() + Vec2(3, 0));
    REQUIRE_MONSTER(sh);
    REQUIRE(rig.monsters.Find(sh)->def.isRanged);
    hero.lastAttackMs = 1e18;
    rig.monsters.Find(sh)->state = MonsterState::Attack;
    REQUIRE(rig.StepUntil([&] { return rig.combat.IsWindingUp(sh); }));
    const double swing = rig.h.clock.NowMs();
    rig.StepTo(swing + 250);
    const auto bolts = EventsOf<EvProjectileLaunched>(rig.h.events);
    REQUIRE(bolts.size() == 1);
    CHECK(bolts[0].kind == ProjectileKind::MonsterBolt);
    CHECK(bolts[0].travelMs == doctest::Approx(216));  // 3 tiles x 36 x 2
    rig.ScriptNoDodge();
    rig.StepTo(bolts[0].launchMs + 216);
    size_t onHero = 0;
    for (const EvHit& h : EventsOf<EvHit>(rig.h.events)) onHero += (h.target == kHeroEntityId && !h.melee);
    CHECK(onHero == 1);
    // Next bolt: destroyed by a cinematic freeze while flying.
    REQUIRE(rig.StepUntil([&] { return rig.combat.IsWindingUp(sh); }));
    const double swing2 = rig.h.clock.NowMs();
    rig.StepTo(swing2 + 260);
    REQUIRE(rig.projectiles.Projectiles().size() == 1);
    rig.projectiles.OnFreezeBegin(true);
    CHECK(rig.projectiles.Projectiles().empty());
    const auto cancelled = EventsOf<EvMonsterAttackCancelled>(rig.h.events);
    REQUIRE(cancelled.size() == 1);
    CHECK(cancelled[0].projectile);
    rig.Step(20);
    size_t onHero2 = 0;
    for (const EvHit& h : EventsOf<EvHit>(rig.h.events)) onHero2 += (h.target == kHeroEntityId);
    CHECK(onHero2 == 1);
  }

  // ===================================================================================================================
  // GameSim level: D13 clock domains with the combat timers
  // ===================================================================================================================
  TEST_CASE("D13 (classes 22.11 a / e): a hero buff and a pending skill release hold across a freeze") {
    SimConfig cfg;
    cfg.enableDebugCommands = true;
    auto sim = GameSim::Create(D(), cfg);
    REQUIRE(sim != nullptr);
    REQUIRE(sim->NewGame(ClassId::Warrior, Difficulty::Normal, 5));
    sim->Context().sys.story->FinishAllBeats();  // the new-game prologue / chapter card hold the world (S2)
    sim->Step();
    int32_t slot = -1;
    for (int32_t i = 0; i < 6; ++i) {
      if (sim->View().hero.hotbar[static_cast<size_t>(i)].skillId == "shield_wall") slot = i;
    }
    REQUIRE(slot >= 0);
    CmdCastSkill cast;
    cast.slot = slot;
    sim->Submit(cast);
    sim->Step();
    const double castAt = sim->NowMs();
    for (int i = 0; i < 6; ++i) sim->Step();  // 100 ms of the 334 ms wind-up
    sim->Submit(CmdDebug{"freeze", "", 0, {}});
    sim->Step();
    const double frozenAt = sim->NowMs();
    for (int i = 0; i < 120; ++i) sim->Step();  // 2 s frozen: nothing moves
    CHECK(sim->NowMs() == frozenAt);
    REQUIRE(sim->View().heroBuffs != nullptr);
    CHECK_FALSE(sim->View().heroBuffs->Has(BuffStat::DamageReduction));
    sim->Submit(CmdDebug{"unfreeze", "", 0, {}});
    sim->Step();
    double releasedAt = -1;
    for (int i = 0; i < 30 && releasedAt < 0; ++i) {
      sim->Step();
      if (sim->View().heroBuffs->Has(BuffStat::DamageReduction)) releasedAt = sim->NowMs();
    }
    REQUIRE(releasedAt > 0);
    CHECK(releasedAt >= castAt + 334);
    CHECK(releasedAt < castAt + 334 + kSimStepMs + 1e-6);  // exactly its remaining sim time after the freeze
    // (a) the buff's remaining time survives another freeze unchanged.
    ActiveBuff b;
    bool found = false;
    for (const ActiveBuff& x : sim->View().heroBuffs->Items()) {
      if (x.stat == BuffStat::DamageReduction) {
        b = x;
        found = true;
      }
    }
    REQUIRE(found);
    const double left = b.RemainingMs(sim->NowMs());
    sim->Submit(CmdDebug{"freeze", "", 0, {}});
    sim->Step();
    const double left2 = b.RemainingMs(sim->NowMs());
    for (int i = 0; i < 300; ++i) sim->Step();
    CHECK(b.RemainingMs(sim->NowMs()) == doctest::Approx(left2));
    CHECK(sim->View().heroBuffs->Has(BuffStat::DamageReduction));
    CHECK(left - left2 <= kSimStepMs + 1e-6);
  }

  // ===================================================================================================================
  // Audit fixes (C4 ground totals, chain shake, C8 teleport lock, audio miss, dungeon death, spirit source)
  // ===================================================================================================================
  TEST_CASE("C4 GroundTickShare: the tick parts keep the remainder and sum to the one-shot hit") {
    // 29 over 6 ticks: 4 5 5 5 5 5 (not 6 x floor(29 / 6) = 24).
    const int32_t parts[6] = {4, 5, 5, 5, 5, 5};
    for (int32_t k = 0; k < 6; ++k) CHECK(GroundTickShare(29, k, 6) == parts[k]);
    for (int32_t ticks = 1; ticks <= 8; ++ticks) {
      for (int32_t total = 0; total <= 240; ++total) {
        int32_t sum = 0;
        for (int32_t k = 0; k < ticks; ++k) {
          const int32_t part = GroundTickShare(total, k, ticks);
          CHECK(part >= total / ticks);
          CHECK(part <= (total + ticks - 1) / ticks);
          sum += part;
        }
        CHECK(sum == total);
      }
    }
    CHECK(GroundTickShare(7, 0, 1) == 7);
    CHECK(GroundTickShare(7, 6, 6) == 0);   // past the last tick
    CHECK(GroundTickShare(7, -1, 6) == 0);
    CHECK(GroundTickShare(-3, 0, 6) == 0);
    CHECK(GroundTickShare(2147483647, 5, 6) + GroundTickShare(2147483647, 0, 6) > 0);  // no int32 overflow
  }

  TEST_CASE("runtime C4: a fire wall's ticks on one target sum to the web's one-shot hit; burn and Spirit roll once") {
    CombatRig rig(ClassId::Mage);
    REQUIRE(rig.ok);
    Hero& hero = *rig.hero;
    const EntityId t = rig.Spawn("goblin_chief", hero.Position() + Vec2(3, 0));
    REQUIRE_MONSTER(t);
    rig.combat.SetAttackTarget(t, false);
    hero.lastAttackMs = 1e18;
    const int32_t fw = rig.SkillIndex("fire_wall");
    hero.Skills().SetLevel(fw, 1);
    const SkillDef& fire = Skill("fire_wall");
    // The web's one-shot hit with "no dodge, no crit".
    Rng probe(1);
    probe.Script({0.99, 0.99});
    SkillHitInput in;
    in.skill = &fire;
    in.level = 1;
    in.synergyFactor = hero.Skills().SynergyFactor(fire);
    const DamageResult oneShot = CalculateDamage(Rules(), rig.combat.HeroCombatant(),
                                                 CombatSystem::MonsterCombatant(*rig.monsters.Find(t)), in, false, probe);
    REQUIRE(oneShot.damage >= 6);
    const double t0 = rig.h.clock.NowMs();
    REQUIRE(rig.combat.RequestSkill(fw, SkillAim{}) == SkillRequestResult::Executed);
    rig.StepBefore(t0 + 230);
    rig.h.rng.Get(RngStream::Combat).Script({0.99, 0.99, 0.0});  // no dodge, no crit, the 40 % burn applies
    rig.StepTo(t0 + 230 + 3100);
    double total = 0;
    size_t ticks = 0;
    for (const EvHit& h : EventsOf<EvHit>(rig.h.events)) {
      if (h.skillId != "fire_wall" || h.target != t) continue;
      CHECK_FALSE(h.dodged);
      CHECK_FALSE(h.crit);
      total += h.amount;
      ++ticks;
    }
    CHECK(ticks == 6);
    CHECK(total == doctest::Approx(oneShot.damage));  // the same total damage (DECISIONS C4)
    // One burn from the full hit: value max(1, floor(dmg x 0.15)) (classes 13.5), never re-rolled per tick.
    size_t burns = 0;
    for (const EvStatusApplied& e : EventsOf<EvStatusApplied>(rig.h.events)) {
      if (e.target != t || e.type != StatusType::Burn) continue;
      ++burns;
      CHECK(e.value == (std::max)(1.0, std::floor(oneShot.damage * 0.15)));
    }
    CHECK(burns == 1);
    // Spirit 'hit' once for the one web hit (classes 12.1), with its source (14.3).
    size_t hitGains = 0;
    for (const EvSpiritChanged& e : EventsOf<EvSpiritChanged>(rig.h.events)) {
      if (e.hasSource && e.source == SpiritSource::Hit) ++hitGains;
    }
    CHECK(hitGains == 1);

    // A dodged roll misses for the whole effect: one MISS, no damage, no status.
    CombatRig d(ClassId::Mage);
    const EntityId u = d.Spawn("goblin_chief", d.hero->Position() + Vec2(3, 0));
    d.monsters.Find(u)->stats.dex = 100;  // 30 % dodge
    d.combat.SetAttackTarget(u, false);
    d.hero->lastAttackMs = 1e18;
    d.hero->Skills().SetLevel(d.SkillIndex("fire_wall"), 1);
    const double d0 = d.h.clock.NowMs();
    REQUIRE(d.combat.RequestSkill(d.SkillIndex("fire_wall"), SkillAim{}) == SkillRequestResult::Executed);
    d.StepBefore(d0 + 230);
    d.h.rng.Get(RngStream::Combat).Script({0.0});  // dodged
    d.StepTo(d0 + 230 + 3100);
    size_t misses = 0, landed = 0;
    for (const EvHit& h : EventsOf<EvHit>(d.h.events)) {
      if (h.skillId != "fire_wall" || h.target != u) continue;
      (h.dodged ? misses : landed) += 1;
    }
    CHECK(misses == 1);
    CHECK(landed == 0);
    CHECK(d.monsters.Find(u)->hp == d.monsters.Find(u)->maxHp);
    CHECK_FALSE(d.status.Has(u, StatusType::Burn));
  }

  TEST_CASE("runtime C4: the chain lightning batch shake follows the last link and counts every link (6.5)") {
    CombatRig rig(ClassId::Mage);
    REQUIRE(rig.ok);
    Hero& hero = *rig.hero;
    const Vec2 p = hero.Position();
    std::vector<EntityId> ids;
    for (const Vec2 off : {Vec2(3, 0), Vec2(1, 0), Vec2(2, 1)}) {
      const EntityId id = rig.Spawn("goblin_chief", p + off);
      REQUIRE_MONSTER(id);
      MonsterInstance* m = rig.monsters.Find(id);
      m->maxHp = 1e6;  // light hits: no per-hit profile shake competes with the batch shake
      m->hp = 1e6;
      ids.push_back(id);
    }
    const int32_t chain = rig.SkillIndex("chain_lightning");
    hero.Skills().SetLevel(chain, 1);
    const double t0 = rig.h.clock.NowMs();
    REQUIRE(rig.combat.RequestSkill(chain, SkillAim{}) == SkillRequestResult::Executed);
    rig.StepBefore(t0 + 230);
    rig.h.rng.Get(RngStream::Combat).Script({0.99, 0.99, 0.99, 0.99, 0.99, 0.99});  // three links, no crit
    rig.h.events.Clear();
    double lastLinkAt = -1, shakeAt = -1;
    std::vector<EvCameraShake> shakes;
    for (int i = 0; i < 30; ++i) {
      rig.Step();
      for (const EvHit& h : EventsOf<EvHit>(rig.h.events)) {
        if (h.skillId == "chain_lightning") lastLinkAt = rig.h.clock.NowMs();
      }
      for (const EvCameraShake& e : EventsOf<EvCameraShake>(rig.h.events)) {
        shakes.push_back(e);
        shakeAt = rig.h.clock.NowMs();
      }
      rig.h.events.Clear();
    }
    REQUIRE(shakes.size() == 1);
    const HitFeedbackTable& t = D().Combat().hitFeedback;
    CHECK(shakes[0].durationMs == t.aoeHitShakeDurationMs);
    CHECK(shakes[0].intensity == doctest::Approx(t.aoeHitShakeBase + 3 * t.aoeHitShakePerHit));  // 0.007
    CHECK(shakeAt == lastLinkAt);
    CHECK(lastLinkAt >= t0 + 230 + 110 - 1e-6);
  }

  TEST_CASE("C8 touch teleport: without a lock it never blinks toward the nearest monster; the lock wins") {
    CombatRig rig(ClassId::Mage);
    REQUIRE(rig.ok);
    Hero& hero = *rig.hero;
    const int32_t tp = rig.SkillIndex("teleport");
    hero.Skills().SetLevel(tp, 1);
    const SkillPortDef& port = Skill("teleport").port;
    const WalkableFn walk = [&rig](int32_t c, int32_t r) { return rig.zone.Walkable(c, r); };
    const int32_t cols = rig.zone.Grid().Cols();
    const int32_t rows = rig.zone.Grid().Rows();
    const Vec2 start = hero.Position();
    const EntityId near = rig.Spawn("goblin", start + Vec2(5, 0));
    REQUIRE_MONSTER(near);
    REQUIRE(rig.combat.AttackTarget() == kNoEntity);
    REQUIRE(rig.combat.PreferredTarget() == near);  // the release target falls back to the nearest monster
    hero.SetFacing(Vec2(0, 1));
    TeleportAim ahead;  // no pointer, no stick, no lock: 6 tiles along the facing
    ahead.facing = Vec2(0, 1);
    TilePos expect;
    REQUIRE(ComputeTeleportDestination(port, start, ahead, cols, rows, walk, expect));
    REQUIRE(rig.combat.RequestSkill(tp, SkillAim{}) == SkillRequestResult::Executed);
    CHECK(hero.Position() == expect.Center());
    CHECK(hero.Facing() == Vec2(0, 1));  // the cast did not turn toward the unlocked monster
    CHECK(Dist(hero.Position(), rig.monsters.Find(near)->pos) > 5);
    // With a lock: the blink goes to the locked target (clamped to 8 tiles, walkable search).
    hero.Skills().ResetCooldowns();
    hero.FillHpMana();
    const Vec2 here = hero.Position();
    const EntityId locked = rig.Spawn("goblin", here + Vec2(-4, 0));
    rig.combat.SetAttackTarget(locked, false);
    TeleportAim toLock;
    toLock.hasTarget = true;
    toLock.targetPos = rig.monsters.Find(locked)->pos;
    toLock.facing = hero.Facing();
    TilePos lockDest;
    REQUIRE(ComputeTeleportDestination(port, here, toLock, cols, rows, walk, lockDest));
    REQUIRE(rig.combat.RequestSkill(tp, SkillAim{}) == SkillRequestResult::Executed);
    CHECK(hero.Position() == lockDest.Center());
    // A tapped target (SkillAim::target) is a lock too.
    hero.Skills().ResetCooldowns();
    hero.FillHpMana();
    const Vec2 there = hero.Position();
    REQUIRE(rig.combat.AttackTarget() == kNoEntity);  // the blink cleared the lock
    SkillAim tap;
    tap.target = near;
    TeleportAim toTap;
    toTap.hasTarget = true;
    toTap.targetPos = rig.monsters.Find(near)->pos;
    toTap.facing = hero.Facing();
    TilePos tapDest;
    REQUIRE(ComputeTeleportDestination(port, there, toTap, cols, rows, walk, tapDest));
    REQUIRE(rig.combat.RequestSkill(tp, tap) == SkillRequestResult::Executed);
    CHECK(hero.Position() == tapDest.Center());
  }

  TEST_CASE("audio 3.2: a monster's stat dodge of a hero hit is silent; a swing into the i-frames plays `miss`") {
    CombatRig rig(ClassId::Warrior);
    REQUIRE(rig.ok);
    Hero& hero = *rig.hero;
    const EntityId g = rig.Spawn("goblin", hero.Position() + Vec2(1, 0), false);
    REQUIRE_MONSTER(g);
    REQUIRE(rig.monsters.Find(g)->stats.dex > 0);
    rig.combat.SetAttackTarget(g, false);
    rig.Step();
    const double swing = rig.h.clock.NowMs();
    rig.StepBefore(swing + 308);
    rig.h.rng.Get(RngStream::Combat).Script({0.0});  // the goblin dodges the contact
    rig.StepTo(swing + 308);
    const auto hits = EventsOf<EvHit>(rig.h.events);
    REQUIRE(hits.size() == 1);
    CHECK(hits[0].dodged);
    CHECK(hits[0].target == g);
    for (const EvSfx& e : EventsOf<EvSfx>(rig.h.events)) CHECK(e.cue != SfxId::Miss);
    // The goblin swings into the dodge roll's i-frames: `miss`, Spirit 'dodge' with its source.
    hero.lastAttackMs = 1e18;
    rig.combat.ClearAttackTarget();
    rig.monsters.Find(g)->state = MonsterState::Attack;
    rig.h.events.Clear();
    REQUIRE(rig.StepUntil([&] { return rig.combat.IsWindingUp(g); }));
    const double mswing = rig.h.clock.NowMs();
    const Vec2 near = hero.Position();
    rig.Step(3);  // 50 ms into the wind-up: the 220 ms i-frames cover the 250 ms contact
    REQUIRE(rig.combat.RequestDodge(Vec2(0, -1)));
    hero.SetPosition(near);
    rig.StepTo(mswing + 300);
    size_t missCues = 0;
    for (const EvSfx& e : EventsOf<EvSfx>(rig.h.events)) missCues += e.cue == SfxId::Miss;
    CHECK(missCues == 1);
    size_t dodgeGains = 0;
    for (const EvSpiritChanged& e : EventsOf<EvSpiritChanged>(rig.h.events)) {
      if (e.hasSource && e.source == SpiritSource::Dodge) ++dodgeGains;
    }
    CHECK(dodgeGains == 1);
  }

  TEST_CASE("combat 15: a death in a sub-dungeon takes the penalty for good (no echo); spirit events carry a source") {
    CombatRig rig(ClassId::Warrior);
    REQUIRE(rig.ok);
    Hero& hero = *rig.hero;
    for (int i = 0; i < 4; ++i) rig.rewards.GrantExp(hero.ExpToNext(), ExpSource::Debug);
    REQUIRE(hero.Level() == 5);
    rig.rewards.ChangeGold(1000, GoldReason::Debug);
    REQUIRE_FALSE(D().World().subDungeons.empty());
    rig.h.session.currentMap = D().World().subDungeons.front().id;
    rig.h.events.Clear();
    hero.SetHp(0);
    rig.combat.KillHero(HeroDeathCause::Other);
    CHECK(hero.Life() == HeroLife::Dying);
    CHECK(hero.Gold() == 900);              // the 10 % toll is still paid
    CHECK_FALSE(rig.soul.State().Has());   // ... but nothing is left behind
    bool lostLog = false;
    for (const EvLog& e : EventsOf<EvLog>(rig.h.events)) lostLog = lostLog || e.text.key == "zone.soulEcho.lostInDungeon";
    CHECK(lostLog);
    // Death resets the meter: EvSpiritChanged without a source (14.3).
    const auto spirit = EventsOf<EvSpiritChanged>(rig.h.events);
    REQUIRE_FALSE(spirit.empty());
    CHECK(spirit.back().value == 0);
    CHECK_FALSE(spirit.back().hasSource);
    // A labyrinth floor counts too; the overworld leaves an echo.
    CombatRig lab(ClassId::Warrior);
    for (int i = 0; i < 4; ++i) lab.rewards.GrantExp(lab.hero->ExpToNext(), ExpSource::Debug);
    lab.rewards.ChangeGold(1000, GoldReason::Debug);
    lab.h.session.currentMap = "dungeon_floor_1";
    lab.hero->SetHp(0);
    lab.combat.KillHero(HeroDeathCause::Other);
    CHECK(lab.hero->Gold() == 900);
    CHECK_FALSE(lab.soul.State().Has());
    CombatRig field(ClassId::Warrior);
    for (int i = 0; i < 4; ++i) field.rewards.GrantExp(field.hero->ExpToNext(), ExpSource::Debug);
    field.rewards.ChangeGold(1000, GoldReason::Debug);
    field.hero->SetHp(0);
    field.combat.KillHero(HeroDeathCause::Other);
    CHECK(field.soul.State().Has());
    // A kill's Spirit gain names its source.
    CombatRig k(ClassId::Warrior);
    const EntityId slime = k.Spawn("slime_green", k.hero->Position() + Vec2(1, 0));
    REQUIRE_MONSTER(slime);
    k.h.events.Clear();
    MonsterHitRequest kill;
    kill.monster = slime;
    kill.amount = 1e6;
    kill.attacker = kHeroEntityId;
    kill.source = KillSource::HeroSkill;
    k.combat.DamageMonster(kill);
    size_t killGains = 0;
    for (const EvSpiritChanged& e : EventsOf<EvSpiritChanged>(k.h.events)) {
      if (e.hasSource && e.source == SpiritSource::Kill) ++killGains;
    }
    CHECK(killGains == 1);
  }
}

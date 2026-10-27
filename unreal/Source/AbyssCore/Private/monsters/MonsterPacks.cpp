// MonsterSystem, dark remake K5: packs, pack aggro, morale, the shield wall, buffs and auras (implementation-plan.md 3
// K5; rules in monsters/Packs.h; MonsterSystem.h "K5"). Owner area: monsters.
//
// Pointer safety (MonsterSystem.cpp header): nothing here grows `monsters_`, but MonsterAggroMsg handlers run inside
// PackAlarm / ResolveBuffCast, so those loops work on id lists and re-find. Nothing here draws from an RNG stream.
#include "abyss/base/Platform.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <string>
#include <vector>

#include "abyss/base/Math.h"
#include "abyss/combat/Combat.h"
#include "abyss/data/DataStore.h"
#include "abyss/hero/Hero.h"
#include "abyss/monsters/MonsterSystem.h"
#include "abyss/monsters/Packs.h"
#include "abyss/sim/SimContext.h"

namespace abyss {

bool MonsterSystem::Sustained(const MonsterInstance& m) const {
  const MonsterMoraleDef& mo = m.def.morale;
  if (!mo.present || !(mo.sustainRadius > 0)) return false;
  const double r2 = mo.sustainRadius * mo.sustainRadius;
  for (const MonsterInstance& s : monsters_) {
    if (s.id == m.id || !s.IsAlive() || !FamilyMatches(mo.sustainFamily, s.def) || !GrantsFearImmunity(s.def)) continue;
    if (DistSq(s.pos, m.pos) <= r2) return true;
  }
  return false;
}

// Pack aggro (header "K5"): one wave from `from`, never passed on.
void MonsterSystem::PackAlarm(EntityId from) {
  const MonsterInstance* src = Find(from);
  if (src == nullptr || src->packId == 0) return;
  const uint32_t pack = src->packId;
  const Vec2 at = src->pos;
  std::vector<EntityId>& ids = scratchPack_;
  ids.clear();
  for (const MonsterInstance& m : monsters_) {
    if (m.packId == pack && m.id != from && m.IsAlive() && m.def.packAggro > 0) ids.push_back(m.id);
  }
  if (ids.empty()) return;
  const std::vector<EntityId> members = ids;  // a copy: MonsterAggroMsg handlers run below
  const MonsterAiDef& ai = ctx_.data.Monsters().ai;
  const double now = ctx_.Now();
  const Vec2 hero = ctx_.sys.hero != nullptr ? ctx_.sys.hero->Position() : Vec2();
  for (EntityId id : members) {
    MonsterInstance* m = Find(id);
    if (m == nullptr || !m->IsAlive()) continue;
    if (DistSq(m->pos, at) > m->def.packAggro * m->def.packAggro) continue;
    const MonsterState before = m->state;
    if (ProvokeMonster(*m, ai, now, Dist(m->pos, hero)) &&
        (before == MonsterState::Idle || before == MonsterState::Patrol)) {
      PublishAggro(*m, before);
    }
  }
}

// Morale (header "K5"): the kill pipeline's K5 step for a pack leader.
void MonsterSystem::OnPackDeath(const MonsterKilledMsg& msg) {
  const MonsterInstance* dead = Find(msg.monster);
  if (dead == nullptr || !dead->packLeader || dead->packId == 0) return;
  const uint32_t pack = dead->packId;
  const double now = ctx_.Now();
  std::vector<EntityId> ids;
  for (const MonsterInstance& m : monsters_) {
    if (m.packId == pack && m.id != msg.monster && m.IsAlive() && m.def.morale.present) ids.push_back(m.id);
  }
  for (EntityId id : ids) {
    const MonsterInstance* m = Find(id);
    if (m == nullptr || !m->IsAlive()) continue;
    if (m->state == MonsterState::Returning || m->state == MonsterState::Flee) continue;
    const double r = m->def.morale.fleeRadius;
    if (r > 0 && DistSq(m->pos, msg.pos) > r * r) continue;
    if (MonsterFearImmune(*m, now) || Sustained(*m)) continue;  // the drum holds them
    StartFlee(id, msg.monster, msg.pos, now);
  }
}

// A dead caster's buffs (header "K5"): every buff whose source it is ends now; the targets' multipliers refresh at once.
void MonsterSystem::DropBuffsOf(EntityId source, double now) {
  for (MonsterInstance& m : monsters_) {
    if (m.abilityBuffs.empty()) continue;
    const size_t before = m.abilityBuffs.size();
    m.abilityBuffs.erase(std::remove_if(m.abilityBuffs.begin(), m.abilityBuffs.end(),
                                        [source](const MonsterAbilityBuff& b) { return b.source == source; }),
                         m.abilityBuffs.end());
    if (m.abilityBuffs.size() != before) RefreshMonsterBuffs(m, now);
  }
}

void MonsterSystem::StartFlee(EntityId id, EntityId leader, Vec2 from, double now) {
  // It breaks: what it was doing goes (its swing or ability on the way, a beast-drawn swing, a consume channel).
  if (ctx_.sys.combat != nullptr) ctx_.sys.combat->CancelMonsterStrike(id);
  MonsterInstance* m = Find(id);
  if (m == nullptr || !m->IsAlive()) return;
  if (m->consumeCorpse != 0) EndConsume(*m, true);
  m->state = MonsterState::Flee;
  m->fleeUntilMs = now + m->def.morale.fleeOnLeaderDeathMs;
  m->fleeFrom = from;
  m->fleeDir = Vec2();
  m->kiting = false;
  m->hasPatrolTarget = false;
  m->path.clear();
  EvMonsterFlee ev;
  ev.entity = id;
  ev.leader = leader;
  ev.from = from;
  ev.startMs = now;
  ev.untilMs = m->fleeUntilMs;
  ctx_.events.Emit(ev);
}

void MonsterSystem::EndFlee(MonsterInstance& m, FleeEnd reason) {
  m.state = MonsterState::Chase;
  m.path.clear();
  EvMonsterFlee ev;
  ev.entity = m.id;
  ev.from = m.fleeFrom;
  ev.untilMs = m.fleeUntilMs;
  ev.ended = true;
  ev.reason = reason;
  ctx_.events.Emit(ev);
}

// Buffs and auras (header "K5"): the auras reaching each monster this step, then its multipliers.
void MonsterSystem::RefreshBuffsAndAuras(double now) {
  bool anyAura = false;
  for (MonsterInstance& m : monsters_) {
    m.auraMul = {1, 1, 1, 1};
    m.auraMask = 0;
    anyAura = anyAura || (m.def.aura.present && m.IsAlive());
  }
  if (anyAura) {
    for (size_t i = 0; i < monsters_.size(); ++i) {
      if (!monsters_[i].IsAlive() || !monsters_[i].def.aura.present) continue;
      const MonsterAuraDef aura = monsters_[i].def.aura;
      const Vec2 at = monsters_[i].pos;
      const double r2 = aura.radius * aura.radius;
      const size_t stat = static_cast<size_t>(aura.stat);
      const uint8_t bit = static_cast<uint8_t>(1u << stat);
      for (MonsterInstance& t : monsters_) {
        if (!t.IsAlive() || DistSq(t.pos, at) > r2 || !FamilyMatches(aura.family, t.def)) continue;
        t.auraMul[stat] = (t.auraMask & bit) != 0 ? (std::max)(t.auraMul[stat], aura.mul) : aura.mul;
        t.auraMask = static_cast<uint8_t>(t.auraMask | bit);
      }
    }
  }
  for (MonsterInstance& m : monsters_) RefreshMonsterBuffs(m, now);
}

// Shield wall (header "K5"): fighting blockers that close share the mean of their headings. Only fighters (Chase /
// Attack) join: a neighbour walking home, fleeing or standing idle faces elsewhere and would turn a fighter's guard away
// from the hero it is fighting.
void MonsterSystem::AlignShieldWalls() {
  std::vector<size_t>& members = scratchWall_;
  members.clear();
  for (size_t i = 0; i < monsters_.size(); ++i) {
    const MonsterInstance& m = monsters_[i];
    if (m.IsAlive() && m.IsAggro() && m.def.block.present && m.def.block.wall > 0) members.push_back(i);
  }
  if (members.size() < 2) return;
  // Union-find over the members (root = the earliest member in list order).
  std::vector<size_t>& root = scratchWallRoot_;
  root.resize(members.size());
  std::iota(root.begin(), root.end(), size_t{0});
  const auto find = [&](size_t a) {
    while (root[a] != a) a = root[a] = root[root[a]];
    return a;
  };
  for (size_t a = 0; a < members.size(); ++a) {
    for (size_t b = a + 1; b < members.size(); ++b) {
      const MonsterInstance& ma = monsters_[members[a]];
      const MonsterInstance& mb = monsters_[members[b]];
      const double reach = (std::min)(ma.def.block.wall, mb.def.block.wall);
      if (DistSq(ma.pos, mb.pos) > reach * reach) continue;
      const size_t ra = find(a), rb = find(b);
      if (ra != rb) root[(std::max)(ra, rb)] = (std::min)(ra, rb);
    }
  }
  std::vector<Vec2>& headings = scratchWallHeadings_;
  for (size_t g = 0; g < members.size(); ++g) {
    if (find(g) != g) continue;  // g is the root of its wall
    headings.clear();
    for (size_t k = g; k < members.size(); ++k) {
      if (find(k) == g) headings.push_back(monsters_[members[k]].heading);
    }
    if (headings.size() < 2) continue;
    const Vec2 shared = WallHeading(headings);
    for (size_t k = g; k < members.size(); ++k) {
      if (find(k) == g) monsters_[members[k]].heading = shared;
    }
  }
}

// A buff / call ability's contact (header "K5").
void MonsterSystem::ResolveBuffCast(EntityId casterId, const MonsterAbilityDef& a) {
  const MonsterInstance* caster = Find(casterId);
  if (caster == nullptr || !caster->IsAlive()) return;
  const double now = ctx_.Now();
  const AbilityBuffDef b = a.buff;  // a copy: provokes publish below
  const Vec2 centre = caster->pos;
  const double r2 = b.radius * b.radius;
  std::vector<EntityId> reached;  // list order; radius 0 = the caster alone
  for (const MonsterInstance& m : monsters_) {
    if (!m.IsAlive() || !FamilyMatches(b.family, m.def)) continue;
    if (m.id == casterId || (b.radius > 0 && DistSq(m.pos, centre) <= r2)) reached.push_back(m.id);
  }
  EvMonsterBuffCast ev;
  ev.entity = casterId;
  ev.abilityId = a.id;
  ev.kind = a.kind;
  ev.centre = centre;
  ev.radius = b.radius;
  ev.family = b.family;
  ev.hasBuff = b.hasStat;
  ev.stat = b.stat;
  ev.mul = b.mul;
  ev.untilMs = b.hasStat ? now + b.ms : now;
  if (b.hasStat) {
    for (EntityId id : reached) {
      MonsterInstance* m = Find(id);
      if (m == nullptr) continue;
      bool refreshed = false;
      for (MonsterAbilityBuff& e : m->abilityBuffs) {
        if (e.source == casterId && e.stat == b.stat) {  // one buff per caster and stat: a recast refreshes it
          e.mul = b.mul;
          e.untilMs = ev.untilMs;
          refreshed = true;
          break;
        }
      }
      if (!refreshed) m->abilityBuffs.push_back(MonsterAbilityBuff{b.stat, b.mul, ev.untilMs, casterId});
      RefreshMonsterBuffs(*m, now);
      ++ev.buffed;
    }
  }
  std::vector<EntityId> called;
  if (a.kind == AbilityKind::Call) {
    for (EntityId id : reached) {
      if (id != casterId) called.push_back(id);
    }
  }
  ev.called = static_cast<int32_t>(called.size());
  ctx_.events.Emit(ev);
  // The social call: every other monster it reached is provoked (M2), with no pack alarm of its own.
  const MonsterAiDef& ai = ctx_.data.Monsters().ai;
  const Vec2 hero = ctx_.sys.hero != nullptr ? ctx_.sys.hero->Position() : Vec2();
  for (EntityId id : called) {
    MonsterInstance* m = Find(id);
    if (m == nullptr || !m->IsAlive()) continue;
    const MonsterState before = m->state;
    if (ProvokeMonster(*m, ai, now, Dist(m->pos, hero)) &&
        (before == MonsterState::Idle || before == MonsterState::Patrol)) {
      PublishAggro(*m, before);
    }
  }
}

}  // namespace abyss

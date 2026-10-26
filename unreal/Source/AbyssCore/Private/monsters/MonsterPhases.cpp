// MonsterSystem, dark remake K4: boss phases and arenas (implementation-plan.md 3 K4; rules in monsters/Phases.h;
// MonsterSystem.h "K4"). Owner area: monsters.
//
// Pointer safety (MonsterSystem.cpp header): only StandPhaseSummon grows `monsters_`; every loop that reaches it works on
// indices into its own queue and re-finds monsters by id. Nothing here draws from an RNG stream.
#include "abyss/base/Platform.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "abyss/base/Math.h"
#include "abyss/base/StrUtil.h"
#include "abyss/combat/Combat.h"
#include "abyss/combat/HitFeedback.h"
#include "abyss/data/DataStore.h"
#include "abyss/data/MapData.h"
#include "abyss/hero/Hero.h"
#include "abyss/monsters/MonsterSystem.h"
#include "abyss/monsters/Phases.h"
#include "abyss/sim/SimContext.h"
#include "abyss/world/Zone.h"

namespace abyss {

namespace {

// Due times compare on the sim clock with the swing scheduler's tolerance.
constexpr double kPhaseEpsilonMs = 1e-6;
// An arena ticks at most this many times in one step (a step is 16.7 ms; the tick 500 ms).
constexpr int32_t kArenaMaxTicksPerStep = 4;

// An authored centre, else the monster's authored spawn tile (the dais; not the jittered spawn anchor of the M1 leash).
Vec2 PhaseCentre(bool has, TilePos centre, const MonsterInstance& m) {
  return has ? centre.Center() : m.homeTile.Center();
}

}  // namespace

double MonsterSystem::PhaseActionMs(const MonsterDef& def, AnimAction action) const {
  const AssetManifest& manifest = ctx_.data.Assets();
  if (manifest.loaded) {
    const AssetEntryDef* art = manifest.FindByGameId(MonsterArtGameId(manifest, def.spriteKey, def.id));
    const AnimClipDef* clip = art != nullptr ? art->FindClip(std::string(PhaseActionClip(action))) : nullptr;
    if (clip != nullptr && clip->lengthMs > 0) return clip->lengthMs;
  }
  return ctx_.data.Monsters().ai.phaseActionMsDefault;
}

void MonsterSystem::ApplyPhaseRow(MonsterInstance& m) {
  const MonsterPhaseDef* row =
      m.phase > 0 && static_cast<size_t>(m.phase) <= m.def.phases.size() ? &m.def.phases[static_cast<size_t>(m.phase) - 1]
                                                                          : nullptr;
  m.phaseSpeedMul = row != nullptr ? row->speedMul : 1.0;
  m.phaseAttackSpeedMul = row != nullptr ? row->attackSpeedMul : 1.0;
  m.phaseDamageTakenMul = row != nullptr ? row->damageTakenMul : 1.0;
  const bool tether = row != nullptr && row->leash.present;
  const Vec2 centre = tether ? PhaseCentre(row->leash.hasCentre, row->leash.centre, m) : Vec2();
  if (tether != m.hasTether || centre != m.tetherCentre) m.path.clear();  // re-plan toward the new goal
  m.hasTether = tether;
  m.tetherCentre = centre;
  m.tetherRadius = tether ? row->leash.radius : 0.0;
}

void MonsterSystem::EnterPhase(MonsterInstance& m, int32_t row) {
  const double now = ctx_.Now();
  const EntityId id = m.id;
  const MonsterPhaseDef& p = m.def.phases[static_cast<size_t>(row)];
  const int32_t before = m.phase;
  const std::vector<uint8_t> offBefore = m.abilityOff;
  m.phase = row + 1;
  PhaseAbilitiesOff(m.def, m.phase, m.abilityOff);
  // An ability the entry makes usable is ready initialCooldownMs after it (its fight's clock already runs).
  if (m.abilityClockStarted && m.abilityReadyAtMs.size() == m.def.abilities.size()) {
    for (size_t i = 0; i < m.def.abilities.size(); ++i) {
      const MonsterAbilityDef& a = m.def.abilities[i];
      if (!AbilityEnabledInPhase(a, i, offBefore, before) && AbilityEnabledInPhase(a, i, m.abilityOff, m.phase)) {
        m.abilityReadyAtMs[i] = (std::max)(m.abilityReadyAtMs[i], now + a.initialCooldownMs);
      }
    }
  }
  ApplyPhaseRow(m);
  // The enter action interrupts what the monster was doing: its swing / ability on its way (also one drawn to the beast),
  // a consume channel.
  if (p.hasEnterAction) {
    if (ctx_.sys.combat != nullptr) ctx_.sys.combat->CancelMonsterStrike(id);
    if (m.consumeCorpse != 0) EndConsume(m, true);
  }
  EvMonsterPhase ev;
  ev.entity = id;
  ev.phase = m.phase;
  ev.phaseCount = static_cast<int32_t>(m.def.phases.size());
  ev.hpBelowPct = p.hpBelowPct;
  ev.stinger = p.stinger;
  ctx_.events.Emit(std::move(ev));
  if (p.hasEnterAction) {
    const double ms = PhaseActionMs(m.def, p.enterAction);
    m.phaseActionUntilMs = (std::max)(m.phaseActionUntilMs, now + ms);
    m.moveSpeed = 0;
    m.path.clear();
    EvPlayAnim anim;
    anim.entity = id;
    anim.action = p.enterAction;
    anim.clip = std::string(PhaseActionClip(p.enterAction));
    anim.startMs = now;
    anim.durationMs = ms;
    if (ctx_.sys.hero != nullptr) {
      anim.hasFaceTarget = true;
      anim.faceTarget = ctx_.sys.hero->Position();
    }
    ctx_.events.Emit(anim);
  }
  if (p.arena.present) {
    MonsterArena a;
    a.active = true;
    a.kind = p.arena.kind;
    a.phase = m.phase;
    a.centre = PhaseCentre(p.arena.hasCentre, p.arena.centre, m);
    a.fromR = p.arena.fromR;
    a.toR = p.arena.toR;
    a.startMs = now;
    a.overMs = p.arena.overMs;
    a.dpsPctMaxHp = p.arena.dpsPctMaxHp;
    a.damageType = p.arena.damageType;
    a.nextTickMs = now + ctx_.data.Monsters().ai.arenaTickMs;
    m.arena = a;
    EvArena ea;
    ea.entity = id;
    ea.kind = a.kind;
    ea.centre = a.centre;
    ea.radius = a.fromR;
    ea.targetRadius = a.toR;
    ea.startMs = a.startMs;
    ea.endMs = a.startMs + a.overMs;
    ea.tickMs = ctx_.data.Monsters().ai.arenaTickMs;
    ea.dpsPctMaxHp = a.dpsPctMaxHp;
    ea.damageType = a.damageType;
    ctx_.events.Emit(ea);
  }
  for (size_t s = 0; s < p.summons.size(); ++s) {
    for (int32_t k = 0; k < p.summons[s].count; ++k) {
      PhaseSummonEntry e;
      e.seq = nextPhaseSummonSeq_++;
      e.owner = id;
      e.phase = m.phase;
      e.summon = static_cast<uint32_t>(s);
      e.slot = k;
      e.dueMs = now + p.summons[s].delayMs;
      e.respawnsLeft = p.summons[s].respawns;
      phaseSummons_.push_back(e);
    }
  }
}

void MonsterSystem::ClearArena(MonsterInstance& m, bool emit) {
  if (!m.arena.active) return;
  m.arena.active = false;
  if (!emit) return;
  EvArena ea;
  ea.entity = m.id;
  ea.kind = m.arena.kind;
  ea.centre = m.arena.centre;
  ea.radius = ArenaRadiusAt(m.arena.fromR, m.arena.toR, m.arena.overMs, ctx_.Now() - m.arena.startMs);
  ea.targetRadius = m.arena.toR;
  ea.startMs = m.arena.startMs;
  ea.endMs = m.arena.startMs + m.arena.overMs;
  ea.tickMs = ctx_.data.Monsters().ai.arenaTickMs;
  ea.dpsPctMaxHp = m.arena.dpsPctMaxHp;
  ea.damageType = m.arena.damageType;
  ea.cleared = true;
  ctx_.events.Emit(ea);
}

void MonsterSystem::DropPhaseWork(EntityId owner) {
  if (owner == kNoEntity) return;
  phaseSummons_.erase(std::remove_if(phaseSummons_.begin(), phaseSummons_.end(),
                                     [owner](const PhaseSummonEntry& e) { return e.owner == owner; }),
                      phaseSummons_.end());
  for (size_t i = 0; i < feeds_.size();) {
    if (feeds_[i].owner == owner) {
      corpses_.Release(feeds_[i].corpse, owner);
      feeds_.erase(feeds_.begin() + static_cast<std::ptrdiff_t>(i));
    } else {
      ++i;
    }
  }
}

void MonsterSystem::ResetPhases(MonsterInstance& m) {
  const EntityId id = m.id;
  const bool entered = m.phase != 0;
  m.hp = m.maxHp;
  m.phase = 0;
  m.phaseActionUntilMs = 0;
  PhaseAbilitiesOff(m.def, 0, m.abilityOff);
  ApplyPhaseRow(m);
  if (entered) {
    EvMonsterPhase ev;
    ev.entity = id;
    ev.phase = 0;
    ev.phaseCount = static_cast<int32_t>(m.def.phases.size());
    ev.reset = true;
    ctx_.events.Emit(std::move(ev));
  }
  ClearArena(m, true);
  DropPhaseWork(id);
  CollapseRaisedOf(id);  // what its fight stood up falls (no kill, no reward)
}

void MonsterSystem::OnPhaseDeath(const MonsterKilledMsg& msg, uint32_t corpse) {
  const double now = ctx_.Now();
  if (MonsterInstance* dead = Find(msg.monster)) ClearArena(*dead, true);
  DropPhaseWork(msg.monster);
  // A standing phase summon: queued again while its phase holds and respawns remain, else forgotten.
  for (size_t i = 0; i < phaseSummons_.size();) {
    PhaseSummonEntry& e = phaseSummons_[i];
    if (e.queued || e.body != msg.monster) {
      ++i;
      continue;
    }
    const MonsterInstance* owner = Find(e.owner);
    const bool again = e.respawnsLeft > 0 && owner != nullptr && owner->IsAlive() && owner->phase == e.phase &&
                       static_cast<size_t>(e.phase) <= owner->def.phases.size();
    if (!again) {
      phaseSummons_.erase(phaseSummons_.begin() + static_cast<std::ptrdiff_t>(i));
      continue;
    }
    const PhaseSummonDef& sd = owner->def.phases[static_cast<size_t>(e.phase) - 1].summons[e.summon];
    --e.respawnsLeft;
    e.body = kNoEntity;
    e.queued = true;
    e.respawn = true;
    e.dueMs = now + sd.respawnMs;
    ++i;
  }
  // Feed: a Raised body that leaves an unburned corpse near a feeding monster is drawn into it.
  if (msg.role != MonsterRole::Raised || corpse == 0 || corpses_.Find(corpse) == nullptr) return;
  for (const MonsterInstance& m : monsters_) {
    if (!m.IsAlive() || m.id == msg.monster || m.phase <= 0 || static_cast<size_t>(m.phase) > m.def.phases.size()) {
      continue;
    }
    const PhaseFeedDef& f = m.def.phases[static_cast<size_t>(m.phase) - 1].feed;
    if (!f.present) continue;
    const Vec2 centre = PhaseCentre(f.hasCentre, f.centre, m);
    if (DistSq(centre, msg.pos) > f.radius * f.radius) continue;
    if (!corpses_.Hold(corpse, m.id)) return;
    PendingFeed pf;
    pf.owner = m.id;
    pf.corpse = corpse;
    pf.dueMs = now + f.delayMs;
    pf.healPct = f.healPct;
    feeds_.push_back(pf);
    EvMonsterFeed ev;
    ev.entity = m.id;
    ev.fed = msg.monster;
    ev.corpseId = corpse;
    ev.pos = msg.pos;
    ev.startMs = now;
    ev.dueMs = pf.dueMs;
    ctx_.events.Emit(ev);
    return;
  }
}

void MonsterSystem::StandPhaseSummon(PhaseSummonEntry& e) {
  const MonsterInstance* owner = Find(e.owner);
  if (owner == nullptr || static_cast<size_t>(e.phase) > owner->def.phases.size() || e.phase <= 0) return;
  const MonsterPhaseDef& row = owner->def.phases[static_cast<size_t>(e.phase) - 1];
  if (e.summon >= row.summons.size()) return;
  const PhaseSummonDef sd = row.summons[e.summon];  // a copy: standing up grows the monster list
  const MonsterDef* base = ctx_.sys.zone != nullptr && ctx_.sys.zone->HasZone()
                               ? ctx_.data.Monsters().FindForZone(ctx_.sys.zone->MapId(), sd.monsterId)
                               : ctx_.data.Monsters().Find(sd.monsterId);
  if (base == nullptr) return;
  Vec2 spot;
  if (sd.at == AbilitySummonAt::Anchors && !sd.anchors.empty()) {
    spot = SummonSpot(sd.anchors[static_cast<size_t>(e.slot) % sd.anchors.size()].Center(), 0, 0, 1);
  } else {
    const double radius = sd.hasRadius ? sd.radius : ctx_.data.Monsters().ai.summonRingRadius;
    spot = SummonSpot(owner->pos, radius, e.slot, sd.count);
  }
  const MonsterDef summoned = *base;
  e.body = SummonAt(summoned, spot, e.owner, sd.hpPct, StrCat("phase.", e.phase));
}

void MonsterSystem::TickPhases() {
  const double now = ctx_.Now();
  // 1. Entries (list order): every row the monster's life crossed, in order. Nothing here grows the list.
  for (MonsterInstance& m : monsters_) {
    if (m.def.phases.empty() || !m.IsAlive()) continue;
    for (;;) {
      const double hpPct = m.maxHp > 0 ? m.hp / m.maxHp * 100.0 : 100.0;
      const int32_t row = NextPhaseToEnter(m.def.phases, m.phase, hpPct);
      if (row < 0) break;
      EnterPhase(m, row);
    }
  }
  // 2. Arenas (list order): the hero outside the ring at the tick's radius, while the monster fights.
  CombatSystem* combat = ctx_.sys.combat;
  const Hero* hero = ctx_.sys.hero;
  const MonsterAiDef& ai = ctx_.data.Monsters().ai;
  std::vector<EntityId> arenaOwners;
  for (const MonsterInstance& m : monsters_) {
    if (m.arena.active && m.IsAlive()) arenaOwners.push_back(m.id);
  }
  for (EntityId id : arenaOwners) {
    for (int32_t n = 0; n < kArenaMaxTicksPerStep; ++n) {
      MonsterInstance* m = Find(id);
      if (m == nullptr || !m->arena.active || now + kPhaseEpsilonMs < m->arena.nextTickMs) break;
      const MonsterArena a = m->arena;
      m->arena.nextTickMs += ai.arenaTickMs;
      if (combat == nullptr || hero == nullptr || !m->IsAlive() || !m->IsAggro()) continue;
      const Vec2 heroPos = hero->Position();
      const double radius = ArenaRadiusAt(a.fromR, a.toR, a.overMs, a.nextTickMs - a.startMs);
      if (InSafeZone(heroPos) || !ArenaOutside(a.centre, radius, heroPos)) continue;
      combat->ArenaTickHero(id, a.dpsPctMaxHp, a.damageType, ai.arenaTickMs, m->def.hitCapHeroMaxHpPct);
    }
  }
  // 3. Queued phase summons (queue order): an entry's own body stands up; a respawn only while its phase holds.
  std::vector<uint32_t> due;
  for (const PhaseSummonEntry& e : phaseSummons_) {
    if (e.queued && !(now + kPhaseEpsilonMs < e.dueMs)) due.push_back(e.seq);
  }
  for (uint32_t seq : due) {
    const auto find = [&]() -> PhaseSummonEntry* {
      for (PhaseSummonEntry& e : phaseSummons_) {
        if (e.seq == seq) return &e;
      }
      return nullptr;
    };
    PhaseSummonEntry* entry = find();
    if (entry == nullptr) continue;
    const MonsterInstance* owner = Find(entry->owner);
    if (owner == nullptr || !owner->IsAlive() || (entry->respawn && owner->phase != entry->phase)) {
      entry->queued = false;  // dropped below
      entry->body = kNoEntity;
      continue;
    }
    PhaseSummonEntry e = *entry;  // a copy: standing up publishes (EvEntitySpawned, MonsterAggroMsg)
    e.queued = false;
    StandPhaseSummon(e);
    if (PhaseSummonEntry* again = find()) *again = e;
  }
  phaseSummons_.erase(std::remove_if(phaseSummons_.begin(), phaseSummons_.end(),
                                     [](const PhaseSummonEntry& e) { return !e.queued && e.body == kNoEntity; }),
                      phaseSummons_.end());
  // 4. Feeds (schedule order): the corpse, if still there, is consumed and its eater heals.
  for (size_t i = 0; i < feeds_.size();) {
    if (now + kPhaseEpsilonMs < feeds_[i].dueMs) {
      ++i;
      continue;
    }
    const PendingFeed f = feeds_[i];
    feeds_.erase(feeds_.begin() + static_cast<std::ptrdiff_t>(i));
    const MonsterInstance* owner = Find(f.owner);
    if (owner == nullptr || !owner->IsAlive() || corpses_.Find(f.corpse) == nullptr) {
      corpses_.Release(f.corpse, f.owner);
      continue;
    }
    const double heal = std::floor(owner->maxHp * f.healPct / 100.0);
    RemoveCorpse(f.corpse, CorpseRemoveReason::Consumed, f.owner);
    Heal(f.owner, heal);
  }
}

}  // namespace abyss

// MonsterSystem (monsters-ai.md 3.8-3.10, 5-9, 11-12, 14; combat-feel.md 17.4; M1-M10, W9). STUB: owner area
// monsters. Storage, lookups and snapshot filling are real; spawning, AI driving, damage and respawn are stubs.
#include "abyss/base/Platform.h"

#include "abyss/monsters/MonsterSystem.h"

#include "abyss/base/Assert.h"
#include "abyss/base/Math.h"
#include "abyss/combat/StatusEffects.h"
#include "abyss/data/DataStore.h"
#include "abyss/save/SaveData.h"
#include "abyss/sim/SimContext.h"
#include "abyss/sim/Snapshot.h"

namespace abyss {

MonsterSystem::MonsterSystem(SimContext& ctx) : ctx_(ctx) {}

void MonsterSystem::SpawnZonePopulation() {
  ABYSS_UNIMPLEMENTED();
  (void)nextActiveRefreshMs_;
  (void)active_;
}

void MonsterSystem::SpawnMiniBoss() {
  ABYSS_UNIMPLEMENTED();
  (void)miniBossSpawnedThisVisit_;
}

void MonsterSystem::OnZoneExit() {
  monsters_.clear();
  grid_.Clear();
  active_.clear();
  miniBoss_ = kNoEntity;
  miniBossSpawnedThisVisit_ = false;
  miniBossDialogueActive_ = false;
  huntLeaders_.clear();
  ctx_.timers.CancelIf([](const Timer& t) { return t.owner == TimerOwner::Monsters; });
}

EntityId MonsterSystem::Spawn(const MonsterSpawnParams& p) {
  ABYSS_UNIMPLEMENTED();
  return kNoEntity;
}

void MonsterSystem::SpawnDueHunts(bool announce) { ABYSS_UNIMPLEMENTED(); }

std::vector<EntityId> MonsterSystem::SpawnAmbush(std::span<const std::string> monsterIds, int32_t count, Vec2 centre,
                                                 double minDist, double distRange, MonsterRole role) {
  ABYSS_UNIMPLEMENTED();
  return {};
}

void MonsterSystem::CheckMiniBossDialogue() {
  if (miniBoss_ != kNoEntity) ABYSS_UNIMPLEMENTED();
}

void MonsterSystem::TickAI(double dtMs) {
  if (!monsters_.empty()) ABYSS_UNIMPLEMENTED();
}

void MonsterSystem::TickEliteBehaviours() {
  if (!monsters_.empty()) ABYSS_UNIMPLEMENTED();
}

void MonsterSystem::OnTimer(const Timer& t) { ABYSS_UNIMPLEMENTED(); }

void MonsterSystem::DismissMiniBossDialogue() {
  if (!miniBossDialogueActive_) return;
  miniBossDialogueActive_ = false;
  const MonsterInstance* boss = Find(miniBoss_);
  ctx_.events.Emit(EvMiniBossDialogue{false, miniBoss_, boss != nullptr ? boss->def.id : std::string()});
  if (boss != nullptr && boss->IsAlive()) ForceChase(miniBoss_);
}

HitWeight MonsterSystem::ApplyDamage(EntityId id, double amount, const DamageFlags& flags) {
  ABYSS_UNIMPLEMENTED();
  return HitWeight::Tick;
}

void MonsterSystem::Heal(EntityId id, double amount) { ABYSS_UNIMPLEMENTED(); }

void MonsterSystem::Teleport(EntityId id, Vec2 to, TeleportReason reason) { ABYSS_UNIMPLEMENTED(); }

void MonsterSystem::ForceChase(EntityId id) { ABYSS_UNIMPLEMENTED(); }

void MonsterSystem::OnMonsterKilled(const MonsterKilledMsg& m) { ABYSS_UNIMPLEMENTED(); }

void MonsterSystem::Respawn(EntityId deadId) { ABYSS_UNIMPLEMENTED(); }

bool MonsterSystem::InSafeZone(Vec2 p) const {
  ABYSS_UNIMPLEMENTED();
  return false;
}

MonsterInstance* MonsterSystem::Find(EntityId id) {
  for (MonsterInstance& m : monsters_) {
    if (m.id == id) return &m;
  }
  return nullptr;
}

const MonsterInstance* MonsterSystem::Find(EntityId id) const {
  for (const MonsterInstance& m : monsters_) {
    if (m.id == id) return &m;
  }
  return nullptr;
}

void MonsterSystem::QueryAlive(Vec2 centre, double radius, std::vector<EntityId>& out) const {
  grid_.QueryRadius(centre.x, centre.y, radius, out);
  std::vector<EntityId> alive;
  for (EntityId id : out) {
    const MonsterInstance* m = Find(id);
    if (m != nullptr && m->IsAlive()) alive.push_back(id);
  }
  out.swap(alive);
}

EntityId MonsterSystem::NearestAlive(Vec2 from, double maxRange) const {
  return grid_.FindNearest(from.x, from.y, maxRange, [this](EntityId id) {
    const MonsterInstance* m = Find(id);
    return m != nullptr && m->IsAlive();
  });
}

EntityId MonsterSystem::NearestAggro(Vec2 from) const {
  ABYSS_UNIMPLEMENTED();
  return kNoEntity;
}

EntityId MonsterSystem::MonsterAtTile(Vec2 tile) const {
  ABYSS_UNIMPLEMENTED();
  return kNoEntity;
}

EntityId MonsterSystem::NearestAliveOfDef(std::string_view defId, Vec2 from) const {
  EntityId best = kNoEntity;
  double bestSq = 0;
  for (const MonsterInstance& m : monsters_) {  // list order: the first of equally near instances wins
    if (!m.IsAlive() || m.def.id != defId) continue;
    const double dSq = DistSq(m.pos, from);
    if (best == kNoEntity || dSq < bestSq) {
      best = m.id;
      bestSq = dSq;
    }
  }
  return best;
}

bool MonsterSystem::AnyAttacking() const {
  for (const MonsterInstance& m : monsters_) {
    if (m.IsAlive() && m.state == MonsterState::Attack) return true;
  }
  return false;
}

void MonsterSystem::Candidates(std::vector<TargetCandidate>& out) const {
  out.clear();
  for (const MonsterInstance& m : monsters_) out.push_back({m.id, m.pos, m.IsAlive()});
}

bool MonsterSystem::IsHuntPresent(std::string_view huntId) const {
  for (const auto& [id, leader] : huntLeaders_) {
    if (id == huntId) return true;
  }
  return false;
}

void MonsterSystem::FillSnapshot(Snapshot& out) const {
  out.miniBossDialogue = miniBossDialogueActive_;
  for (const MonsterInstance& m : monsters_) {
    MonsterView v;
    v.id = m.id;
    v.defId = m.def.id;
    v.artId = m.def.spriteKey;
    v.nameKey = m.def.nameKey;
    v.storyNamed = m.storyNameShown;
    v.pos = m.pos;
    v.prevPos = m.pos;
    v.heading = m.heading;
    v.speedTilesPerSec = m.moveSpeed;
    v.state = m.state;
    v.role = m.role;
    v.alive = m.IsAlive();
    v.hp = m.hp;
    v.maxHp = m.maxHp;
    v.elite = m.def.elite;
    v.miniBoss = m.def.isMiniBoss;
    for (const MonsterAffix& a : m.affixes) v.affixes.push_back(a.type);
    v.statusMask = ctx_.sys.status != nullptr ? ctx_.sys.status->StatusMask(m.id) : 0u;
    v.visualScale = m.visualScale;
    out.monsters.push_back(std::move(v));
  }
}

void MonsterSystem::WriteSave(SaveData& out) const { out.miniBossDialogueSeen = miniBossDialogueSeen_; }

void MonsterSystem::ReadSave(const SaveData& in) { miniBossDialogueSeen_ = in.miniBossDialogueSeen; }

}  // namespace abyss

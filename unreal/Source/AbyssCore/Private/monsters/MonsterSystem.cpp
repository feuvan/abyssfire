// MonsterSystem (monsters-ai.md 3.8-3.10, 5-9, 11-12, 14; combat-feel.md 17.4; DECISIONS M1-M10, W9). Owner area:
// monsters.
//
// Pointer safety: Spawn / respawn / hunt reveals push into `monsters_`, and ApplyDamage publishes MonsterKilledMsg whose
// handlers can reveal hunts (QuestProgressMsg -> SpawnDueHunts). Every loop that can reach such a path iterates ids
// and re-finds; no MonsterInstance pointer is kept across a publish.
#include "abyss/base/Platform.h"

#include "abyss/monsters/MonsterSystem.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <utility>

#include "abyss/base/I18n.h"
#include "abyss/base/Math.h"
#include "abyss/base/StrUtil.h"
#include "abyss/combat/Combat.h"
#include "abyss/combat/HitFeedback.h"
#include "abyss/combat/StatusEffects.h"
#include "abyss/data/DataStore.h"
#include "abyss/hero/Hero.h"
#include "abyss/monsters/EliteAffixes.h"
#include "abyss/monsters/MonsterDefs.h"
#include "abyss/quests/QuestSystem.h"
#include "abyss/save/SaveData.h"
#include "abyss/sim/SimContext.h"
#include "abyss/sim/Snapshot.h"
#include "abyss/world/Zone.h"

namespace abyss {

MonsterSystem::MonsterSystem(SimContext& ctx) : ctx_(ctx) {}

// =====================================================================================================================
// zone lifecycle
// =====================================================================================================================

std::string MonsterSystem::CurrentZoneId() const {
  if (ctx_.sys.zone != nullptr && ctx_.sys.zone->HasZone()) return ctx_.sys.zone->MapId();
  return ctx_.session.currentMap;
}

void MonsterSystem::BuildWorld() {
  if (worldOverride_) return;
  world_ = MonsterWorld{};
  ZoneRuntime* zone = ctx_.sys.zone;
  if (zone == nullptr || !zone->HasZone()) return;
  world_.cols = zone->Grid().Cols();
  world_.rows = zone->Grid().Rows();
  world_.walkable = [zone](int32_t col, int32_t row) { return zone->Walkable(col, row); };
  world_.findPath = [zone](Vec2 from, Vec2 to, std::vector<TilePos>& out) {
    return zone->Paths().FindPath(from.x, from.y, to.x, to.y, out);
  };
}

void MonsterSystem::SetWorldForTesting(MonsterWorld world) {
  world_ = std::move(world);
  worldOverride_ = true;
  if (world_.cols > 0 && world_.rows > 0 && (world_.cols != preparedCols_ || world_.rows != preparedRows_)) {
    preparedCols_ = world_.cols;
    preparedRows_ = world_.rows;
    grid_.Reset(world_.cols, world_.rows);
    for (const MonsterInstance& m : monsters_) grid_.Insert(m.id, m.pos);
  }
}

void MonsterSystem::EnsureZoneGrid() {
  ZoneRuntime* zone = ctx_.sys.zone;
  if (worldOverride_ || zone == nullptr || !zone->HasZone()) return;
  const int32_t cols = zone->Grid().Cols(), rows = zone->Grid().Rows();
  if (preparedMapId_ == zone->MapId() && preparedCols_ == cols && preparedRows_ == rows) return;
  preparedMapId_ = zone->MapId();
  preparedCols_ = cols;
  preparedRows_ = rows;
  grid_.Reset(cols, rows);
  for (const MonsterInstance& m : monsters_) grid_.Insert(m.id, m.pos);
  BuildWorld();
}

void MonsterSystem::PrepareZone() {
  monsters_.clear();
  active_.clear();
  grid_.Clear();
  activeDue_ = true;
  miniBoss_ = kNoEntity;
  miniBossDialogueMonster_ = kNoEntity;
  miniBossSpawnedThisVisit_ = false;
  miniBossDialogueActive_ = false;
  huntLeaders_.clear();
  if (worldOverride_) return;  // a test world stays until the zone exits
  preparedMapId_.clear();
  preparedCols_ = preparedRows_ = 0;
  EnsureZoneGrid();
}

bool MonsterSystem::Walkable(int32_t col, int32_t row) const {
  if (world_.cols > 0 && world_.rows > 0 && (col < 0 || row < 0 || col >= world_.cols || row >= world_.rows)) {
    return false;
  }
  if (!world_.walkable) return true;
  return world_.walkable(col, row);
}

bool MonsterSystem::InSafeZone(Vec2 p) const {
  ZoneRuntime* zone = ctx_.sys.zone;
  return zone != nullptr && zone->HasZone() && zone->InSafeZone(p);  // any camp: distSq < safeR^2 (strict)
}

bool MonsterSystem::RoleNeverRespawns(MonsterRole role) const {
  // Mini-bosses (zone / sub-dungeon) and seal keepers never respawn in the web; the rest is data (M4 / W9 / M7).
  if (role == MonsterRole::ZoneMiniBoss || role == MonsterRole::SubDungeonMiniBoss || role == MonsterRole::SealKeeper) {
    return true;
  }
  const std::string_view name = EnumName(role);
  for (const std::string& r : ctx_.data.Monsters().ai.noRespawnRoles) {
    if (r == name) return true;
  }
  return false;
}

bool MonsterSystem::StoryBossBlocked() const {
  const MonsterAiDef& ai = ctx_.data.Monsters().ai;
  const QuestSystem* quests = ctx_.sys.quests;
  if (quests == nullptr || ai.storyBossNotAfterQuestTurnIn.empty()) return false;
  if (!quests->IsTurnedIn(ai.storyBossNotAfterQuestTurnIn)) return false;
  // Chapter complete: farmable again (once per visit).
  return !(ai.storyBossFarmableAfterChapter && !ai.chapterCompleteQuest.empty() &&
           quests->IsTurnedIn(ai.chapterCompleteQuest));
}

void MonsterSystem::SpawnZonePopulation() {
  PrepareZone();
  ZoneRuntime* zone = ctx_.sys.zone;
  if (zone == nullptr || !zone->HasZone()) return;
  const MapDef& map = zone->Map();
  const std::string zoneId = zone->MapId();
  const MonsterTables& tables = ctx_.data.Monsters();
  const MonsterAiDef& ai = tables.ai;
  Rng& rng = ctx_.Rand(RngStream::Ai);
  const int32_t cols = world_.cols > 0 ? world_.cols : map.cols;
  const int32_t rows = world_.rows > 0 ? world_.rows : map.rows;
  const bool storyBlocked = StoryBossBlocked();
  for (const MapSpawnDef& spawn : map.spawns) {  // data order
    const MonsterDef* base = tables.FindForZone(zoneId, spawn.monsterId);
    if (base == nullptr) continue;
    MonsterRole role = MonsterRole::Regular;
    if (!ai.storyBossId.empty() && base->id == ai.storyBossId) {
      if (storyBlocked) continue;  // M7: not after its quest is turned in (unless the chapter is complete)
      if (ai.storyBossOncePerVisit) role = MonsterRole::StoryBoss;  // never respawns within the visit
    }
    const MonsterDef def = ScaleMonsterForDifficulty(*base, ctx_.data.Combat().difficulty, ctx_.session.difficulty);
    for (int32_t i = 0; i < spawn.count; ++i) {
      // M10 (FIX Q16): up to placementTries jittered tiles (2 draws each, col first; walkable, outside safe zones),
      // then the anchor itself.
      bool placed = false;
      TilePos tile = spawn.pos;
      for (int32_t attempt = 0; attempt < ai.placementTries; ++attempt) {
        const int32_t c = Clamp(spawn.pos.col + rng.RandomInt(-ai.spawnJitter, ai.spawnJitter), 1, cols - 2);
        const int32_t r = Clamp(spawn.pos.row + rng.RandomInt(-ai.spawnJitter, ai.spawnJitter), 1, rows - 2);
        if (!Walkable(c, r)) continue;
        if (InSafeZone(Vec2(c, r))) continue;
        tile = TilePos{c, r};
        placed = true;
        break;
      }
      if (!placed) {
        if (!Walkable(spawn.pos.col, spawn.pos.row)) continue;
        tile = spawn.pos;
      }
      MonsterSpawnParams p;
      p.baseDef = &def;
      p.alreadyScaled = true;
      p.tile = tile;
      p.role = role;
      p.rollAffixes = def.elite;
      Spawn(p);
    }
  }
}

void MonsterSystem::SpawnMiniBoss() {
  if (miniBossSpawnedThisVisit_ && ctx_.data.Monsters().ai.miniBossOncePerVisit) return;  // M7
  miniBossSpawnedThisVisit_ = true;
  miniBoss_ = kNoEntity;
  ZoneRuntime* zone = ctx_.sys.zone;
  if (zone == nullptr || !zone->HasZone()) return;
  EnsureZoneGrid();
  const MonsterTables& tables = ctx_.data.Monsters();
  const MiniBossEntry* entry = tables.MiniBossFor(zone->MapId());
  if (entry == nullptr || !entry->hasSpawn) return;
  const MonsterDef* def = tables.Find(entry->monsterId);
  if (def == nullptr) return;
  const TilePos tile = entry->spawn;
  const int32_t cols = world_.cols > 0 ? world_.cols : zone->Map().cols;
  const int32_t rows = world_.rows > 0 ? world_.rows : zone->Map().rows;
  if (tile.col < 0 || tile.col >= cols || tile.row < 0 || tile.row >= rows) return;  // no walkability check (web)
  MonsterSpawnParams p;
  p.baseDef = def;
  p.tile = tile;
  p.role = MonsterRole::ZoneMiniBoss;
  p.rollAffixes = true;  // always elite
  miniBoss_ = Spawn(p);
}

void MonsterSystem::OnZoneExit() {
  for (const MonsterInstance& m : monsters_) {
    ctx_.events.Emit(EvEntityDespawned{m.id, EntityKind::Monster, DespawnReason::ZoneUnload});
  }
  monsters_.clear();
  grid_.Clear();
  active_.clear();
  activeDue_ = true;
  miniBoss_ = kNoEntity;
  miniBossDialogueMonster_ = kNoEntity;
  miniBossSpawnedThisVisit_ = false;
  miniBossDialogueActive_ = false;
  huntLeaders_.clear();
  worldOverride_ = false;
  world_ = MonsterWorld{};
  preparedMapId_.clear();
  preparedCols_ = preparedRows_ = 0;
  ctx_.timers.CancelIf([](const Timer& t) { return t.owner == TimerOwner::Monsters; });
}

// =====================================================================================================================
// spawning
// =====================================================================================================================

MonsterInstance MonsterSystem::MakeInstance(const MonsterDef& def, TilePos tile, MonsterRole role) {
  const MonsterAiDef& ai = ctx_.data.Monsters().ai;
  MonsterInstance m;
  m.id = ctx_.ids.Next();
  m.def = def;
  m.originalDef = def;
  m.role = role;
  m.noRespawn = RoleNeverRespawns(role);
  m.hp = m.maxHp = def.hp;
  m.stats = MonsterBaseStats(def, ai);
  m.pos = tile.Center();
  m.prevPos = m.pos;
  m.spawnAnchor = tile;
  m.state = MonsterState::Idle;
  m.active = false;  // joins the activity set at its next refresh (3.8)
  return m;
}

void MonsterSystem::RollAffixesInto(MonsterInstance& m, std::string_view zoneId) {
  const std::vector<EliteAffixType> affixes =
      RollEliteAffixes(ctx_.data.Combat().eliteAffixes, zoneId, ctx_.Rand(RngStream::Ai));
  if (!affixes.empty()) ApplyEliteAffixes(m, affixes, ctx_.data.Combat().eliteAffixes, ctx_.data.Monsters().ai);
}

void MonsterSystem::EmitSpawned(const MonsterInstance& m) {
  ctx_.events.Emit(EvEntitySpawned{m.id, EntityKind::Monster, m.def.id, m.def.spriteKey, m.pos, m.heading,
                                   m.visualScale});
}

void MonsterSystem::PublishAggro(const MonsterInstance& m, MonsterState previous) {
  ctx_.bus.Publish(MonsterAggroMsg{m.id, m.def.id, m.pos, previous});
}

EntityId MonsterSystem::AddInstance(MonsterInstance&& m, bool startChasing) {
  EnsureZoneGrid();
  if (startChasing) m.state = MonsterState::Chase;
  const EntityId id = m.id;
  const Vec2 pos = m.pos;
  monsters_.push_back(std::move(m));
  grid_.Insert(id, pos);
  const MonsterInstance& added = monsters_.back();
  EmitSpawned(added);
  if (startChasing) PublishAggro(added, MonsterState::Idle);  // ambush / rescue / defend: created chasing (A7)
  return id;
}

EntityId MonsterSystem::Spawn(const MonsterSpawnParams& p) {
  if (p.baseDef == nullptr) return kNoEntity;
  const MonsterDef def = p.alreadyScaled
                             ? *p.baseDef
                             : ScaleMonsterForDifficulty(*p.baseDef, ctx_.data.Combat().difficulty, ctx_.session.difficulty);
  MonsterInstance m = MakeInstance(def, p.tile, p.role);
  m.huntId = p.huntId;
  m.visualScale = p.visualScale;
  if (p.rollAffixes) RollAffixesInto(m, p.affixZone.empty() ? std::string_view(CurrentZoneId()) : p.affixZone);
  return AddInstance(std::move(m), p.startChasing);
}

void MonsterSystem::SpawnDueHunts(bool announce) {
  const QuestSystem* quests = ctx_.sys.quests;
  ZoneRuntime* zone = ctx_.sys.zone;
  if (quests == nullptr || zone == nullptr || !zone->HasZone()) return;
  // Drop dead leaders: a re-trigger while the quest is still due spawns a fresh leader and pack.
  std::vector<std::pair<std::string, EntityId>> alive;
  for (auto& [huntId, leader] : huntLeaders_) {
    const MonsterInstance* m = Find(leader);
    if (m != nullptr && m->IsAlive()) alive.emplace_back(huntId, leader);
  }
  huntLeaders_.swap(alive);
  std::vector<std::string> present;
  for (const auto& [huntId, leader] : huntLeaders_) present.push_back(huntId);
  const std::vector<std::pair<const QuestDef*, const QuestProgress*>> open = quests->OpenQuests();
  const std::vector<DueHunt> due = HuntsToSpawn(open, zone->MapId(), present, ctx_.data.Monsters().hunts);
  SpawnHunts(due, announce);
}

void MonsterSystem::SpawnHunts(std::span<const DueHunt> due, bool announce) {
  if (due.empty()) return;
  EnsureZoneGrid();
  const MonsterTables& tables = ctx_.data.Monsters();
  const MonsterAiDef& ai = tables.ai;
  const DifficultyTable& diff = ctx_.data.Combat().difficulty;
  const std::string zoneId = CurrentZoneId();
  Rng& rng = ctx_.Rand(RngStream::Ai);
  const std::function<bool(int32_t, int32_t)> walkable = [this](int32_t c, int32_t r) { return Walkable(c, r); };
  for (const DueHunt& d : due) {
    if (d.hunt == nullptr) continue;
    const HuntDef& hunt = *d.hunt;
    const MonsterDef* base = tables.FindForZone(zoneId, hunt.monsterId);
    if (base == nullptr) continue;
    TilePos spot = hunt.spawn;
    if (!Walkable(spot.col, spot.row) && !FindWalkableNear(hunt.spawn, ai.huntSpotSearchRadius, walkable, spot)) {
      continue;  // retried on the next trigger
    }
    const MonsterDef def =
        ScaleMonsterForDifficulty(MakeHuntDefinition(*base, hunt, ai, hunt.name), diff, ctx_.session.difficulty);
    MonsterSpawnParams lp;
    lp.baseDef = &def;
    lp.alreadyScaled = true;
    lp.tile = spot;
    lp.role = MonsterRole::HuntLeader;
    lp.rollAffixes = true;  // always
    lp.huntId = hunt.huntId;
    lp.visualScale = ai.huntVisualScale;
    const EntityId leader = Spawn(lp);
    huntLeaders_.emplace_back(hunt.huntId, leader);

    const MonsterDef* minionBase = hunt.hasMinions ? tables.FindForZone(zoneId, hunt.minionMonsterId) : nullptr;
    if (minionBase != nullptr) {
      const MonsterDef mdef = ScaleMonsterForDifficulty(*minionBase, diff, ctx_.session.difficulty);
      for (int32_t i = 0; i < hunt.minionCount; ++i) {
        const int32_t c = spot.col + rng.RandomInt(-ai.huntMinionJitter, ai.huntMinionJitter);  // no clamp (web)
        const int32_t r = spot.row + rng.RandomInt(-ai.huntMinionJitter, ai.huntMinionJitter);
        if (!Walkable(c, r)) continue;  // one attempt each
        MonsterSpawnParams mp;
        mp.baseDef = &mdef;
        mp.alreadyScaled = true;
        mp.tile = TilePos{c, r};
        mp.role = MonsterRole::HuntMinion;
        mp.rollAffixes = false;  // never, even for an elite base
        Spawn(mp);
      }
    }

    if (announce) {
      ctx_.events.Log(MakeLoc("zone.quest.huntRevealed", {KeyArg("name", def.nameKey)}), LogType::System);
      ctx_.events.Emit(EvCameraShake{ai.huntRevealShakeMs, ai.huntRevealShakeIntensity});
      ctx_.events.Emit(EvHuntRevealed{hunt.huntId});
    }
  }
}

namespace {
// findWalkableTile (RandomEventSystem, monsters 6.4): the clamped tile itself, else rings 1..maxRadius (dr outer, dc
// inner, ring edges only) inside [1, size - 2].
bool MonsterSysFindWalkableTile(TilePos preferred, int32_t cols, int32_t rows, int32_t maxRadius,
                                const std::function<bool(int32_t, int32_t)>& walkable, TilePos& out) {
  const int32_t pc = Clamp(preferred.col, 1, cols - 2);
  const int32_t pr = Clamp(preferred.row, 1, rows - 2);
  if (walkable(pc, pr)) {
    out = TilePos{pc, pr};
    return true;
  }
  for (int32_t radius = 1; radius <= maxRadius; ++radius) {
    for (int32_t dr = -radius; dr <= radius; ++dr) {
      for (int32_t dc = -radius; dc <= radius; ++dc) {
        if (std::abs(dr) != radius && std::abs(dc) != radius) continue;
        const int32_t nr = pr + dr, nc = pc + dc;
        if (nr >= 1 && nr < rows - 1 && nc >= 1 && nc < cols - 1 && walkable(nc, nr)) {
          out = TilePos{nc, nr};
          return true;
        }
      }
    }
  }
  return false;
}
}  // namespace

std::vector<EntityId> MonsterSystem::SpawnAmbush(std::span<const std::string> monsterIds, int32_t count, Vec2 centre,
                                                 double minDist, double distRange, MonsterRole role) {
  std::vector<EntityId> out;
  EnsureZoneGrid();
  const std::string zoneId = CurrentZoneId();
  const MonsterTables& tables = ctx_.data.Monsters();
  const ZoneMonsterList* zoneList = tables.ZoneList(zoneId);
  Rng& rng = ctx_.Rand(RngStream::Events);
  int32_t cols = world_.cols, rows = world_.rows;
  if ((cols <= 0 || rows <= 0) && ctx_.sys.zone != nullptr && ctx_.sys.zone->HasZone()) {
    cols = ctx_.sys.zone->Map().cols;
    rows = ctx_.sys.zone->Map().rows;
  }
  const std::function<bool(int32_t, int32_t)> walkable = [this](int32_t c, int32_t r) { return Walkable(c, r); };
  for (int32_t i = 0; i < count; ++i) {
    std::string id;
    if (!monsterIds.empty()) {
      id = monsterIds[rng.Index(monsterIds.size())];  // floor(rand * len)
    } else if (zoneList != nullptr && !zoneList->monsterIds.empty()) {
      id = zoneList->monsterIds.front();  // web fallback: the zone's first monster
    } else {
      continue;
    }
    const MonsterDef* base = tables.FindForZone(zoneId, id);
    if (base == nullptr) continue;  // before the placement draws (web order)
    const double angle = rng.Float01() * kTwoPi;
    const double dist = minDist + rng.Float01() * distRange;
    const TilePos preferred{JsRoundInt(centre.x + std::cos(angle) * dist), JsRoundInt(centre.y + std::sin(angle) * dist)};
    TilePos tile;
    if (cols < 3 || rows < 3 || !MonsterSysFindWalkableTile(preferred, cols, rows, 5, walkable, tile)) continue;
    MonsterSpawnParams p;
    p.baseDef = base;  // difficulty-scaled by Spawn; never affixes (monsters 1.3)
    p.tile = tile;
    p.role = role;
    p.rollAffixes = false;
    p.startChasing = true;
    const EntityId e = Spawn(p);
    if (e != kNoEntity) out.push_back(e);
  }
  return out;
}

// =====================================================================================================================
// per step
// =====================================================================================================================

void MonsterSystem::CheckMiniBossDialogue() {
  if (miniBossDialogueActive_ || miniBoss_ == kNoEntity) return;
  MonsterInstance* boss = Find(miniBoss_);
  if (boss == nullptr || !boss->IsAlive()) return;
  const Hero* hero = ctx_.sys.hero;
  if (hero == nullptr || hero->Life() != HeroLife::Alive || hero->Hp() <= 0) return;  // never on a corpse
  const double aggro = boss->def.aggroRange;
  if (DistSq(hero->Position(), boss->pos) > aggro * aggro) return;
  for (const std::string& seen : miniBossDialogueSeen_) {
    if (seen == boss->def.id) return;
  }
  // Seen before showing (web); the boss holds idle while the panel is open.
  boss->state = MonsterState::Idle;
  boss->hasPatrolTarget = false;
  boss->path.clear();
  miniBossDialogueSeen_.push_back(boss->def.id);
  if (ctx_.data.Monsters().MiniBossDialogue(boss->def.id) == nullptr) return;
  miniBossDialogueActive_ = true;
  miniBossDialogueMonster_ = boss->id;
  ctx_.events.Emit(EvMiniBossDialogue{true, boss->id, boss->def.id});
}

void MonsterSystem::DismissMiniBossDialogue() {
  if (!miniBossDialogueActive_) return;
  miniBossDialogueActive_ = false;
  const EntityId id = miniBossDialogueMonster_;
  miniBossDialogueMonster_ = kNoEntity;
  const MonsterInstance* boss = Find(id);
  ctx_.events.Emit(EvMiniBossDialogue{false, id, boss != nullptr ? boss->def.id : std::string()});
  if (boss != nullptr && boss->IsAlive()) ForceChase(id);  // onDismiss: force aggro
}

void MonsterSystem::TickAI(double dtMs) {
  if (monsters_.empty() || ctx_.sys.hero == nullptr) return;
  EnsureZoneGrid();
  const MonsterAiDef& ai = ctx_.data.Monsters().ai;
  const double now = ctx_.Now();
  const Vec2 heroPos = ctx_.sys.hero->Position();
  for (MonsterInstance& m : monsters_) {
    m.prevPos = m.pos;
    m.groundSpeed = 0;
  }

  // Activity set (3.8): every activeRefreshMs (first refresh of a zone immediately), monsters within aiCullRadius of
  // the hero (grid order) plus every aggro monster anywhere (list order).
  if (activeDue_ || now >= nextActiveRefreshMs_) {
    activeDue_ = false;
    nextActiveRefreshMs_ = now + ai.activeRefreshMs;
    grid_.QueryRadius(heroPos.x, heroPos.y, ai.aiCullRadius, active_);
    for (MonsterInstance& m : monsters_) m.active = false;
    for (EntityId id : active_) {
      if (MonsterInstance* m = Find(id)) m->active = true;
    }
    for (MonsterInstance& m : monsters_) {
      if (m.IsAggro() && !m.active) {
        active_.push_back(m.id);
        m.active = true;
      }
    }
  }

  const bool heroInSafe = InSafeZone(heroPos);
  const StatusEffectSystem* status = ctx_.sys.status;
  Rng& rng = ctx_.Rand(RngStream::Ai);
  const std::vector<EntityId> ids = active_;
  std::vector<EntityId> near;
  std::vector<Vec2> neighbours;
  for (EntityId id : ids) {
    MonsterInstance* m = Find(id);
    if (m == nullptr || !m->IsAlive()) continue;
    if (miniBossDialogueActive_ && id == miniBossDialogueMonster_) continue;  // frozen during its dialogue
    if (status != nullptr && status->IsImmobilized(id)) continue;           // no AI, safe-zone check or grid update
    const MonsterState before = m->state;
    // Safe-zone repel: an aggro monster inside a camp radius drops aggro.
    if (m->IsAggro() && InSafeZone(m->pos)) {
      m->state = MonsterState::Idle;
      m->provokedUntilMs = 0;
      m->path.clear();
    }
    MonsterAiInput in;
    in.nowMs = now;
    in.dtMs = dtMs;
    in.heroVisible = !(heroInSafe && !m->IsAggro());  // a hero in a camp is invisible to non-aggro monsters
    in.heroPos = heroPos;
    in.speedMul = status != nullptr ? status->SpeedMultiplier(id) : 1.0;
    neighbours.clear();
    if (ai.separationRadius > 0) {
      grid_.QueryRadius(m->pos.x, m->pos.y, ai.separationRadius, near);
      for (EntityId nid : near) {
        if (nid == id) continue;
        const MonsterInstance* n = Find(nid);
        if (n != nullptr && n->IsAlive()) neighbours.push_back(n->pos);
      }
    }
    in.neighbours = neighbours;
    UpdateMonsterAI(*m, ai, in, world_, rng);
    if (dtMs > 0) m->groundSpeed = Dist(m->pos, m->prevPos) * 1000.0 / dtMs;
    grid_.Update(id, m->pos);
    if (m->state == MonsterState::Chase &&
        (before == MonsterState::Idle || before == MonsterState::Patrol || before == MonsterState::Returning)) {
      PublishAggro(*m, before);
    }
  }
}

void MonsterSystem::TickEliteBehaviours() {
  if (monsters_.empty() || ctx_.sys.hero == nullptr) return;
  EnsureZoneGrid();
  const EliteAffixTable& table = ctx_.data.Combat().eliteAffixes;
  const double now = ctx_.Now();
  Hero& hero = *ctx_.sys.hero;
  Rng& rng = ctx_.Rand(RngStream::Ai);
  int32_t cols = world_.cols, rows = world_.rows;
  if ((cols <= 0 || rows <= 0) && ctx_.sys.zone != nullptr && ctx_.sys.zone->HasZone()) {
    cols = ctx_.sys.zone->Map().cols;
    rows = ctx_.sys.zone->Map().rows;
  }
  std::vector<EntityId> ids;
  for (const MonsterInstance& m : monsters_) {
    if (m.IsAlive() && !m.affixes.empty()) ids.push_back(m.id);
  }
  for (EntityId id : ids) {
    MonsterInstance* m = Find(id);
    if (m == nullptr || !m->IsAlive()) continue;
    const Vec2 heroPos = hero.Position();
    // Teleporting: periodic blink next to the hero while aggro (17.4); no safe-zone check.
    if (m->IsAggro() && cols > 2 && rows > 2) {
      TilePos tile;
      if (EliteTeleportTarget(*m, table, heroPos, now, cols, rows, rng, tile) && Walkable(tile.col, tile.row)) {
        Teleport(id, tile.Center(), TeleportReason::EliteBlink);
        m = Find(id);
        if (m == nullptr) continue;
      }
    }
    // Curse aura: hero within the radius -> one tagged damageAmplify buff, refreshed (17.4).
    bool logNow = false;
    if (EliteCurseAuraInRange(*m, table, heroPos, now, logNow)) {
      ActiveBuff b;
      b.stat = BuffStat::DamageAmplify;
      b.value = table.Def(EliteAffixType::CurseAura).curseAuraReduction;
      b.durationMs = table.curseBuffDurationMs;
      b.startMs = now;
      b.tag = BuffTag::CurseAura;
      b.source = id;
      hero.Buffs().RefreshTagged(b);
      if (logNow) ctx_.events.Log(MakeLoc("zone.combat.curseAura"), LogType::Combat);
    }
  }
}

void MonsterSystem::OnTimer(const Timer& t) {
  if (t.owner != TimerOwner::Monsters) return;
  if (t.kind == static_cast<uint16_t>(MonsterTimerKind::Respawn)) Respawn(t.entity);
}

// =====================================================================================================================
// damage (5)
// =====================================================================================================================

HitWeight MonsterSystem::ApplyDamage(EntityId id, double amount, const DamageFlags& flags) {
  MonsterInstance* m = Find(id);
  if (m == nullptr || m->state == MonsterState::Dead) return HitWeight::Tick;
  const double now = ctx_.Now();
  m->lastDamagedMs = now;
  const bool wasAlive = m->hp > 0;
  m->hp = (std::max)(0.0, m->hp - amount);
  const bool killed = wasAlive && m->hp <= 0;
  const HitWeight weight =
      ClassifyHit(ctx_.data.Combat().hitFeedback, amount, m->maxHp, flags.isCrit, killed, flags.isTick);
  if (flags.hasFrom) {
    m->hasLastHitFrom = true;
    m->lastHitFrom = flags.from;
  }
  if (m->hp <= 0) {
    // die(): the single alive -> dead transition; kill credit exactly once.
    m->state = MonsterState::Dead;
    m->moveSpeed = 0;
    m->groundSpeed = 0;
    m->hasPatrolTarget = false;
    m->path.clear();
    EvPlayAnim anim;
    anim.entity = id;
    anim.action = AnimAction::Death;
    anim.startMs = now;
    anim.durationMs = ctx_.data.Combat().anim.Preset(m->def.animCategory).deathDuration;
    anim.hasFaceTarget = m->hasLastHitFrom;  // thrown away from the last hit source (combat 13.2)
    anim.faceTarget = m->lastHitFrom;
    ctx_.events.Emit(anim);
    MonsterKilledMsg msg;
    msg.monster = id;
    msg.defId = m->def.id;
    msg.level = m->def.level;
    msg.elite = m->def.elite;
    msg.isMiniBoss = m->def.isMiniBoss;
    msg.isSubDungeonMiniBoss = m->def.isSubDungeonMiniBoss;
    msg.eliteAffixCount = m->AffixCount();
    msg.affixLootBonus = m->AffixLootBonus(ctx_.data.Combat().eliteAffixes);
    msg.expReward = m->def.expReward;
    msg.goldMin = m->def.goldMin;
    msg.goldMax = m->def.goldMax;
    msg.pos = m->pos;
    msg.role = m->role;
    msg.killer = flags.attacker;
    msg.source = flags.source;
    msg.killedByEliteBasic = m->def.elite && flags.source == KillSource::HeroBasic;
    ctx_.bus.Publish(msg);  // `m` may dangle from here on (hunt reveals grow the list)
    return weight;
  }
  if (flags.provokes) {
    // M2: a hit provokes an idle / patrolling monster, or a chasing one hit from beyond its aggro range.
    const MonsterState before = m->state;
    const double heroDist = ctx_.sys.hero != nullptr ? Dist(m->pos, ctx_.sys.hero->Position()) : 0.0;
    if (ProvokeMonster(*m, ctx_.data.Monsters().ai, now, heroDist) &&
        (before == MonsterState::Idle || before == MonsterState::Patrol)) {
      PublishAggro(*m, before);
    }
  }
  return weight;
}

void MonsterSystem::Heal(EntityId id, double amount) {
  MonsterInstance* m = Find(id);
  if (m == nullptr || m->state == MonsterState::Dead || !(amount > 0) || m->hp >= m->maxHp) return;
  m->hp = (std::min)(m->maxHp, m->hp + amount);
}

void MonsterSystem::Teleport(EntityId id, Vec2 to, TeleportReason reason) {
  MonsterInstance* m = Find(id);
  if (m == nullptr) return;
  const Vec2 from = m->pos;
  m->pos = to;
  m->prevPos = to;  // no interpolation across a blink
  m->path.clear();
  grid_.Update(id, to);
  ctx_.events.Emit(EvEntityTeleported{id, from, to, reason});
}

void MonsterSystem::ForceChase(EntityId id) {
  MonsterInstance* m = Find(id);
  if (m == nullptr || !m->IsAlive()) return;
  const MonsterState before = m->state;
  if (before != MonsterState::Idle && before != MonsterState::Patrol && before != MonsterState::Returning) return;
  m->state = MonsterState::Chase;
  m->hasPatrolTarget = false;
  m->path.clear();
  PublishAggro(*m, before);
}

// =====================================================================================================================
// kill pipeline (last handler) and respawn (7)
// =====================================================================================================================

void MonsterSystem::OnMonsterKilled(const MonsterKilledMsg& msg) {
  const MonsterInstance* m = Find(msg.monster);
  // Log zone.monsterKill with the exp / gold the combat step credited for this kill.
  int64_t exp = 0, gold = 0;
  if (ctx_.sys.combat != nullptr && ctx_.sys.combat->LastKillReward().monster == msg.monster) {
    exp = ctx_.sys.combat->LastKillReward().exp;
    gold = ctx_.sys.combat->LastKillReward().gold;
  }
  const std::string nameKey = m != nullptr ? m->def.nameKey : "data.monster." + msg.defId;
  ctx_.events.Log(MakeLoc("zone.monsterKill", {KeyArg("monsterName", nameKey), {"exp", ToStr(exp)}, {"gold", ToStr(gold)}}),
                  LogType::Loot);
  // Respawn decision.
  if (msg.monster == miniBoss_) {
    miniBoss_ = kNoEntity;  // never respawns within the visit (M7)
    return;
  }
  if (m == nullptr) return;
  if (m->noRespawn) {
    for (size_t i = 0; i < huntLeaders_.size(); ++i) {
      if (huntLeaders_[i].second == msg.monster) {
        huntLeaders_.erase(huntLeaders_.begin() + static_cast<std::ptrdiff_t>(i));
        break;
      }
    }
    return;
  }
  ctx_.timers.Schedule(ctx_.Now() + ctx_.data.Monsters().ai.respawnDelayMs, TimerOwner::Monsters,
                       static_cast<uint16_t>(MonsterTimerKind::Respawn), msg.monster);
}

void MonsterSystem::Respawn(EntityId deadId) {
  size_t idx = monsters_.size();
  for (size_t i = 0; i < monsters_.size(); ++i) {
    if (monsters_[i].id == deadId) {
      idx = i;
      break;
    }
  }
  if (idx == monsters_.size() || monsters_[idx].IsAlive()) return;
  EnsureZoneGrid();
  const MonsterAiDef& ai = ctx_.data.Monsters().ai;
  Rng& rng = ctx_.Rand(RngStream::Ai);
  // M3: jitter around the ORIGINAL anchor (no drift); up to placementTries walkable tiles outside safe zones, else the
  // anchor itself (no safe-zone check, web).
  const TilePos anchor = monsters_[idx].spawnAnchor;
  TilePos tile = anchor;
  for (int32_t attempt = 0; attempt < ai.placementTries; ++attempt) {
    const int32_t tc = anchor.col + rng.RandomInt(-ai.respawnJitter, ai.respawnJitter);
    const int32_t tr = anchor.row + rng.RandomInt(-ai.respawnJitter, ai.respawnJitter);
    if (!Walkable(tc, tr)) continue;
    if (InSafeZone(Vec2(tc, tr))) continue;
    tile = TilePos{tc, tr};
    break;
  }
  const MonsterInstance& dead = monsters_[idx];
  MonsterInstance fresh = MakeInstance(dead.originalDef, tile, dead.role);  // fresh id, full HP, idle, no swing yet
  fresh.spawnAnchor = anchor;
  fresh.huntId = dead.huntId;
  fresh.visualScale = dead.visualScale;
  fresh.noRespawn = dead.noRespawn;
  if (fresh.originalDef.elite) RollAffixesInto(fresh, CurrentZoneId());  // re-rolled, never compounded
  grid_.Remove(deadId);
  ctx_.events.Emit(EvEntityDespawned{deadId, EntityKind::Monster, DespawnReason::Died});
  monsters_[idx] = std::move(fresh);
  grid_.Insert(monsters_[idx].id, monsters_[idx].pos);
  EmitSpawned(monsters_[idx]);
}

// =====================================================================================================================
// queries
// =====================================================================================================================

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
  int32_t extent = (std::max)(world_.cols, world_.rows);
  if (extent <= 0 && ctx_.sys.zone != nullptr && ctx_.sys.zone->HasZone()) {
    extent = (std::max)(ctx_.sys.zone->Map().cols, ctx_.sys.zone->Map().rows);
  }
  const double range = extent > 0 ? static_cast<double>(extent) : 1e9;
  return grid_.FindNearest(from.x, from.y, range, [this](EntityId id) {
    const MonsterInstance* m = Find(id);
    return m != nullptr && m->IsAlive() && m->IsAggro();
  });
}

EntityId MonsterSystem::MonsterAtTile(Vec2 tile) const {
  std::vector<EntityId> nearby;
  grid_.QueryRadius(tile.x, tile.y, 2, nearby);
  for (EntityId id : nearby) {
    const MonsterInstance* m = Find(id);
    if (m == nullptr || !m->IsAlive()) continue;
    if (std::fabs(m->pos.x - tile.x) < 1.5 && std::fabs(m->pos.y - tile.y) < 1.5) return id;
  }
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
    if (id != huntId) continue;
    const MonsterInstance* m = Find(leader);
    if (m != nullptr && m->IsAlive()) return true;
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
    if (m.storyNameShown) {  // story 8.3: the boss label shows the intro name once renamed
      if (const BossIntroDef* intro = ctx_.data.Story().BossIntroFor(m.def.id)) v.nameKey = intro->name;
    }
    v.pos = m.pos;
    v.prevPos = m.prevPos;
    v.heading = m.heading;
    v.speedTilesPerSec = m.groundSpeed;
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

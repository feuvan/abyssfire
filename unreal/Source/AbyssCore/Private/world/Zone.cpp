// ZoneRuntime (world-map-nav.md 3.4, 7.1, 7.4, 9, 10.3, 12; W3, W5-W8, Q6, Q22): zone construction, the pointer press
// chain, the interact action and prompt, walk-then-act, exits (armed / sealed), zone transitions and the town portal.
// Owner area: world.
#include "abyss/base/Platform.h"

#include "abyss/world/Zone.h"

#include <array>
#include <cmath>

#include "abyss/base/I18n.h"
#include "abyss/base/Math.h"
#include "abyss/combat/Combat.h"
#include "abyss/data/DataStore.h"
#include "abyss/hero/Hero.h"
#include "abyss/items/GroundLoot.h"
#include "abyss/monsters/MonsterSystem.h"
#include "abyss/quests/Lore.h"
#include "abyss/quests/QuestWorld.h"
#include "abyss/save/SaveIO.h"
#include "abyss/sim/SimContext.h"
#include "abyss/sim/Snapshot.h"
#include "abyss/story/StoryDirector.h"
#include "abyss/world/Locomotion.h"
#include "abyss/world/MapGen.h"
#include "abyss/world/RandomEvents.h"

namespace abyss {

namespace zone_impl {
// Camp NPC slot offsets (quests-story-ch1.md 6.2 / world 3.4).
constexpr std::array<TilePos, 6> kZoneCampNpcOffsets{{{-3, -2}, {3, -2}, {-3, 2}, {3, 2}, {0, -3}, {0, 3}}};
constexpr double kZoneClickBox = 1.5;          // 7.1: |dcol| < 1.5 && |drow| < 1.5 hit boxes
constexpr uint32_t kZonePortalFlashColor = 0x4488ff;
constexpr double kZonePortalFlashMs = 200, kZonePortalFlashAlpha = 0.5;

bool ZoneHeroAlive(const SimContext& ctx) {
  const Hero* h = ctx.sys.hero;
  return h != nullptr && h->Life() == HeroLife::Alive && h->Hp() > 0;
}

// The inner tile of a border exit (4.3 g): the walkable pad the hero stands on.
TilePos ZoneExitInner(const MapExitDef& e, int32_t cols, int32_t rows) {
  const int32_t ic = e.pos.col == 0 ? 1 : e.pos.col == cols - 1 ? cols - 2 : e.pos.col;
  const int32_t ir = e.pos.row == 0 ? 1 : e.pos.row == rows - 1 ? rows - 2 : e.pos.row;
  return TilePos{ic, ir};
}
}  // namespace zone_impl

ZoneRuntime::ZoneRuntime(SimContext& ctx) : ctx_(ctx) {}
ZoneRuntime::~ZoneRuntime() = default;

bool ZoneRuntime::EnterZone(std::string_view mapId, bool hasTarget, Vec2 target) {
  using namespace zone_impl;
  const MapDef* map = ctx_.data.FindMap(mapId);
  if (map == nullptr) map = ctx_.data.FindMap(ctx_.data.World().defaultMap);
  if (map == nullptr) return false;
  map_ = map;
  grid_ = BuildZoneGrid(ctx_.data, *map);
  pathfinder_ = std::make_unique<Pathfinder>(grid_);

  npcs_.clear();
  for (const MapCampDef& camp : map->camps) {
    for (size_t i = 0; i < camp.npcs.size(); ++i) {
      const TilePos off = kZoneCampNpcOffsets[i % kZoneCampNpcOffsets.size()];
      NpcPlacement p;
      p.id = ctx_.ids.Next();
      p.npcId = camp.npcs[i];
      p.pos = Vec2(camp.pos.col + off.col, camp.pos.row + off.row);
      npcs_.push_back(std::move(p));
    }
  }
  for (const FieldNpcDef& f : map->fieldNpcs) {
    NpcPlacement p;
    p.id = ctx_.ids.Next();
    p.npcId = f.npcId;
    p.pos = Vec2(f.pos.col, f.pos.row);
    p.field = true;
    npcs_.push_back(std::move(p));
  }

  exits_.clear();
  const WorldConstants& wc = ctx_.data.World().constants;
  for (const MapExitDef& e : map->exits) {
    ExitState s;
    s.def = e;
    s.sealed = ctx_.config.milestone1 && e.targetMap == wc.sealedGateExitTo;
    s.armed = false;  // W8: armed once the hero has been > sqrt(6) tiles away since entering (first Tick)
    exits_.push_back(std::move(s));
  }

  pending_ = PendingInteraction{};
  prompt_ = InteractTarget{};
  portalTimer_ = kNoTimer;
  transitionTimer_ = kNoTimer;
  transitionReady_ = false;
  pendingZone_.clear();

  const Vec2 start = hasTarget ? target : Vec2(map->playerStart.col, map->playerStart.row);
  ctx_.sys.hero->SetPosition(start);
  return true;
}

void ZoneRuntime::RequestZoneChange(std::string_view mapId, Vec2 target) {
  if (map_ == nullptr || ctx_.session.transitioning) return;  // changeZone guard (isTransitioning)
  ctx_.bus.Publish(SaveRequestMsg{SaveReason::ZoneChange});
  ctx_.session.transitioning = true;
  CancelTownPortal();
  pending_ = PendingInteraction{};
  pendingZone_ = std::string(mapId);
  pendingTarget_ = target;
  transitionReady_ = false;
  ctx_.events.Emit(EvZone{EvZone::Phase::TransitionBegan, pendingZone_, RoundToTile(target)});
  transitionTimer_ = ctx_.timers.Schedule(ctx_.Now() + ctx_.data.World().constants.zoneFadeInMs, TimerOwner::World,
                                          static_cast<uint16_t>(WorldTimerKind::ZoneTransition));
}

void ZoneRuntime::ExitZone() {
  if (portalTimer_ != kNoTimer) ctx_.timers.Cancel(portalTimer_);
  if (transitionTimer_ != kNoTimer) ctx_.timers.Cancel(transitionTimer_);
  portalTimer_ = kNoTimer;
  transitionTimer_ = kNoTimer;
  npcs_.clear();
  exits_.clear();
  pending_ = PendingInteraction{};
  prompt_ = InteractTarget{};
  pathfinder_.reset();
  grid_ = ZoneGrid();
  map_ = nullptr;
}

bool ZoneRuntime::TakePendingTransition(std::string& mapId, Vec2& target) {
  if (!transitionReady_) return false;
  transitionReady_ = false;
  mapId = pendingZone_;
  target = pendingTarget_;
  pendingZone_.clear();
  return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// Per step
// ---------------------------------------------------------------------------------------------------------------------
void ZoneRuntime::Tick() {
  if (map_ == nullptr) return;
  TickExits();
  TickPending();
  TickPrompt();
}

void ZoneRuntime::TickExits() {
  // checkExitProximity (9.2) with W7 (sealed gate) and W8 (armed after > sqrt(6) tiles away since entering).
  if (!zone_impl::ZoneHeroAlive(ctx_) || ctx_.session.transitioning) return;
  const WorldConstants& wc = ctx_.data.World().constants;
  const Vec2 hero = ctx_.sys.hero->Position();
  const double armSq = wc.exitArmDistance * wc.exitArmDistance;
  for (ExitState& e : exits_) {
    if (!e.armed && DistSq(hero, e.def.pos.Center()) > armSq) e.armed = true;
  }
  for (ExitState& e : exits_) {
    if (!e.armed || !(DistSq(hero, e.def.pos.Center()) < wc.exitRadiusSq)) continue;
    if (e.sealed) {
      // W7: the sealed gate to chapter 2 shows the coming-soon line; it re-arms once the hero walks away.
      e.armed = false;
      ctx_.events.Emit(EvBanner{BannerKind::ComingSoon, MakeLoc(wc.sealedGateMessageKey), LocText{}});
      ctx_.events.Log(MakeLoc(wc.sealedGateMessageKey), LogType::System);
    } else {
      RequestZoneChange(e.def.targetMap, e.def.target.Center());
    }
    break;  // stop after the first
  }
}

bool ZoneRuntime::ResolveTarget(const InteractTarget& t, Vec2& pos, double& range) const {
  const WorldConstants& wc = ctx_.data.World().constants;
  switch (t.kind) {
    case InteractKind::Loot: {
      const GroundItem* g = ctx_.sys.groundLoot != nullptr ? ctx_.sys.groundLoot->Find(t.id) : nullptr;
      if (g == nullptr) return false;
      pos = g->pos;
      range = std::sqrt(wc.lootRadiusSq);
      return true;
    }
    case InteractKind::Npc: {
      const NpcPlacement* n = FindNpcEntity(t.id);
      if (n == nullptr) return false;
      pos = n->pos;
      range = wc.npcRange;
      return true;
    }
    case InteractKind::HiddenReward: {
      const HiddenRewardProp* p = ctx_.sys.lore != nullptr ? ctx_.sys.lore->FindHiddenReward(t.id) : nullptr;
      if (p == nullptr) return false;
      pos = p->pos;
      range = std::sqrt(wc.hiddenChestRadiusSq);
      return true;
    }
    case InteractKind::EventPuzzle: {
      const ActiveRandomEvent* e = ctx_.sys.randomEvents != nullptr ? ctx_.sys.randomEvents->FindByProp(t.id) : nullptr;
      if (e == nullptr || e->prop == kNoEntity) return false;
      if (e->type == RandomEventType::EnvironmentalPuzzle && e->resolved) return false;
      pos = e->propPos;
      range = std::sqrt(wc.puzzleRadiusSq);
      return true;
    }
    default:
      return false;
  }
}

void ZoneRuntime::TickPending() {
  if (!pending_.active) return;
  HeroLocomotion* loco = ctx_.sys.locomotion;
  Vec2 pos;
  double range = 0;
  if (!zone_impl::ZoneHeroAlive(ctx_) || (loco != nullptr && loco->MoveGeneration() != pending_.moveGeneration) ||
      !ResolveTarget(pending_.target, pos, range)) {
    pending_ = PendingInteraction{};  // replaced by another order / target gone / dead
    return;
  }
  const Vec2 hero = ctx_.sys.hero->Position();
  if (DistSq(hero, pos) <= range * range) {
    const InteractTarget t = pending_.target;
    pending_ = PendingInteraction{};
    ActOn(t);
    return;
  }
  if (loco == nullptr || loco->Path().empty()) pending_ = PendingInteraction{};  // the walk ended out of range
}

void ZoneRuntime::TickPrompt() {
  const InteractTarget t = zone_impl::ZoneHeroAlive(ctx_) ? FindInteractTarget() : InteractTarget{};
  if (t.kind == prompt_.kind && t.id == prompt_.id) {
    prompt_.pos = t.pos;
    return;
  }
  prompt_ = t;
  ctx_.events.Emit(EvInteractPrompt{t.kind, t.id, t.pos});
}

void ZoneRuntime::OnTimer(const Timer& t) {
  switch (static_cast<WorldTimerKind>(t.kind)) {
    case WorldTimerKind::TownPortal: {
      if (t.id != portalTimer_) return;
      portalTimer_ = kNoTimer;
      // 9.4: camera flash, zone_transition SFX, moveTo(dest), arrival log.
      ctx_.events.Emit(EvCameraFlash{zone_impl::kZonePortalFlashColor, zone_impl::kZonePortalFlashMs,
                                     zone_impl::kZonePortalFlashAlpha});
      ctx_.events.Sfx(ctx_.data.Audio().rules.townPortalComplete);
      if (ctx_.sys.locomotion != nullptr) {
        ctx_.sys.locomotion->Teleport(portalDestination_, TeleportReason::TownPortal);
      } else {
        ctx_.sys.hero->SetPosition(portalDestination_);
      }
      ctx_.events.Log(MakeLoc("zone.teleport.toCamp"), LogType::System);
      ctx_.events.Emit(EvTownPortal{EvTownPortal::Phase::Completed, ctx_.data.World().constants.townPortalChannelMs,
                                    portalDestination_});
      break;
    }
    case WorldTimerKind::ZoneTransition:
      if (t.id != transitionTimer_) return;
      transitionTimer_ = kNoTimer;
      transitionReady_ = true;
      break;
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// Interaction
// ---------------------------------------------------------------------------------------------------------------------
InteractTarget ZoneRuntime::FindInteractTarget() const {
  InteractTarget best;
  if (map_ == nullptr || ctx_.sys.hero == nullptr) return best;
  const WorldConstants& wc = ctx_.data.World().constants;
  const Vec2 hero = ctx_.sys.hero->Position();
  double bestSq = 0;
  auto consider = [&](InteractKind kind, EntityId id, Vec2 pos, double rangeSq, const std::string& key) {
    const double d = DistSq(hero, pos);
    if (d > rangeSq) return;
    if (best.kind != InteractKind::None && !(d < bestSq)) return;  // ties: lower order, then list order
    best.kind = kind;
    best.id = id;
    best.pos = pos;
    best.key = key;
    bestSq = d;
  };
  if (ctx_.sys.groundLoot != nullptr) {
    for (const GroundItem& g : ctx_.sys.groundLoot->Items()) {
      consider(InteractKind::Loot, g.id, g.pos, wc.lootRadiusSq, g.item.uid);
    }
  }
  for (const NpcPlacement& n : npcs_) consider(InteractKind::Npc, n.id, n.pos, wc.npcRange * wc.npcRange, n.npcId);
  if (ctx_.sys.lore != nullptr) {
    for (const HiddenRewardProp& p : ctx_.sys.lore->HiddenRewards()) {
      consider(InteractKind::HiddenReward, p.id, p.pos, wc.hiddenChestRadiusSq, p.areaId);
    }
  }
  if (ctx_.sys.randomEvents != nullptr) {
    for (const ActiveRandomEvent& e : ctx_.sys.randomEvents->Active()) {
      if (e.type != RandomEventType::EnvironmentalPuzzle || e.resolved || e.prop == kNoEntity) continue;
      consider(InteractKind::EventPuzzle, e.prop, e.propPos, wc.puzzleRadiusSq, std::string(EnumName(e.type)));
    }
  }
  return best;
}

bool ZoneRuntime::ActOn(const InteractTarget& t) {
  switch (t.kind) {
    case InteractKind::Loot:
      return ctx_.sys.groundLoot != nullptr && ctx_.sys.groundLoot->TryPickUp(t.id);
    case InteractKind::Npc: {
      const NpcPlacement* n = FindNpcEntity(t.id);
      if (n == nullptr || ctx_.sys.questWorld == nullptr) return false;
      const std::string npcId = n->npcId;
      ctx_.sys.questWorld->InteractNpc(npcId);
      return true;
    }
    case InteractKind::HiddenReward:
      return ctx_.sys.lore != nullptr && ctx_.sys.lore->ClaimHiddenReward(t.id);
    case InteractKind::EventPuzzle:
      return ctx_.sys.randomEvents != nullptr && ctx_.sys.randomEvents->InteractProp(t.id);
    default:
      return false;
  }
}

bool ZoneRuntime::ActOrWalk(const InteractTarget& t) {
  Vec2 pos;
  double range = 0;
  if (!ResolveTarget(t, pos, range)) return false;
  const Vec2 hero = ctx_.sys.hero->Position();
  if (DistSq(hero, pos) <= range * range) return ActOn(t);
  HeroLocomotion* loco = ctx_.sys.locomotion;
  if (loco == nullptr || !ctx_.data.World().constants.walkThenAct) return false;
  if (!loco->MoveTo(pos, range)) return false;
  pending_.active = true;
  pending_.target = t;
  pending_.target.pos = pos;
  pending_.range = range;
  pending_.moveGeneration = loco->MoveGeneration();
  return true;
}

bool ZoneRuntime::Interact() {
  if (map_ == nullptr || !zone_impl::ZoneHeroAlive(ctx_)) return false;
  const InteractTarget t = FindInteractTarget();
  if (t.kind == InteractKind::None) return false;  // nothing in range: no-op, no log
  return ActOn(t);
}

bool ZoneRuntime::InteractWith(EntityId target) {
  if (map_ == nullptr || target == kNoEntity || !zone_impl::ZoneHeroAlive(ctx_)) return false;
  InteractTarget t;
  t.id = target;
  if (ctx_.sys.groundLoot != nullptr && ctx_.sys.groundLoot->Find(target) != nullptr) {
    t.kind = InteractKind::Loot;
  } else if (FindNpcEntity(target) != nullptr) {
    t.kind = InteractKind::Npc;
  } else if (ctx_.sys.lore != nullptr && ctx_.sys.lore->FindHiddenReward(target) != nullptr) {
    t.kind = InteractKind::HiddenReward;
  } else if (ctx_.sys.randomEvents != nullptr && ctx_.sys.randomEvents->FindByProp(target) != nullptr) {
    t.kind = InteractKind::EventPuzzle;  // event props (puzzle, merchant) share the 2-tile prop range
  } else {
    return false;
  }
  return ActOrWalk(t);
}

void ZoneRuntime::OnPointerPress(Vec2 tile, PointerButton button, int32_t pointerId) {
  using namespace zone_impl;
  if (map_ == nullptr) return;
  if (ctx_.sys.story != nullptr && ctx_.sys.story->IsCinematic()) return;  // row 0
  if (button == PointerButton::Secondary) {                                // row 2 (before the dead check)
    UseTownPortal();
    return;
  }
  if (!ZoneHeroAlive(ctx_)) return;  // row 3
  pending_ = PendingInteraction{};
  auto inBox = [&tile](Vec2 p) {
    return std::fabs(p.x - tile.x) < kZoneClickBox && std::fabs(p.y - tile.y) < kZoneClickBox;
  };
  // row 4: loot (first in drop order); Q22: an out-of-range drop is walked to and picked up on arrival.
  if (ctx_.sys.groundLoot != nullptr) {
    const EntityId drop = ctx_.sys.groundLoot->LootAt(tile);
    if (drop != kNoEntity) {
      InteractTarget t;
      t.kind = InteractKind::Loot;
      t.id = drop;
      ActOrWalk(t);
      return;
    }
  }
  // row 5: nearest NPC with distSq(npc, tile) < 3.24; W5 / Q6: far NPCs are walked to and talked to on arrival.
  {
    const double pickSq = ctx_.data.World().constants.npcPickRadiusSq;
    const NpcPlacement* nearest = nullptr;
    double nearestSq = 0;
    for (const NpcPlacement& n : npcs_) {
      const double d = DistSq(n.pos, tile);
      if (d < pickSq && (nearest == nullptr || d < nearestSq)) {
        nearest = &n;
        nearestSq = d;
      }
    }
    if (nearest != nullptr) {
      InteractTarget t;
      t.kind = InteractKind::Npc;
      t.id = nearest->id;
      if (ActOrWalk(t)) return;
    }
  }
  // rows 6-8 (Ember Tower, sub-dungeon entrance, labyrinth portal) are later milestones.
  // row 9: hidden-area reward within the 1.5 box (W8: walk then collect).
  if (ctx_.sys.lore != nullptr) {
    for (const HiddenRewardProp& p : ctx_.sys.lore->HiddenRewards()) {
      if (!inBox(p.pos)) continue;
      InteractTarget t;
      t.kind = InteractKind::HiddenReward;
      t.id = p.id;
      if (ActOrWalk(t)) return;
      break;
    }
  }
  // Random-event props (puzzle device, wandering merchant): a web click on the prop sprite (13.3).
  if (ctx_.sys.randomEvents != nullptr) {
    for (const ActiveRandomEvent& e : ctx_.sys.randomEvents->Active()) {
      if (e.prop == kNoEntity || !inBox(e.propPos)) continue;
      if (e.type != RandomEventType::EnvironmentalPuzzle && e.type != RandomEventType::WanderingMerchant) continue;
      if (e.resolved && e.type == RandomEventType::EnvironmentalPuzzle) continue;
      InteractTarget t;
      t.kind = InteractKind::EventPuzzle;
      t.id = e.prop;
      if (ActOrWalk(t)) return;
      break;
    }
  }
  // row 10: living monster in the box -> attack lock + approach to attack range (C7).
  if (ctx_.sys.monsters != nullptr) {
    const EntityId m = ctx_.sys.monsters->MonsterAtTile(tile);
    if (m != kNoEntity) {
      if (ctx_.sys.combat != nullptr) ctx_.sys.combat->SetAttackTarget(m, true);
      return;
    }
  }
  // row 11: exit in the box -> W6 / W8: walk to its inner tile; the proximity trigger fires on arrival.
  for (const ExitState& e : exits_) {
    if (!inBox(e.def.pos.Center())) continue;
    if (ctx_.sys.locomotion != nullptr) {
      const TilePos inner = ZoneExitInner(e.def, grid_.Cols(), grid_.Rows());
      if (ctx_.sys.locomotion->MoveTo(inner.Center()) && ctx_.sys.combat != nullptr) {
        ctx_.sys.combat->ClearAttackTarget();
      }
    }
    return;
  }
  // row 12: ground -> path; always start hold-to-move.
  const TilePos t = RoundToTile(tile);
  if (!grid_.InBounds(t.col, t.row)) return;
  HeroLocomotion* loco = ctx_.sys.locomotion;
  if (loco == nullptr) return;
  if (loco->MoveTo(t.Center()) && ctx_.sys.combat != nullptr) ctx_.sys.combat->ClearAttackTarget();
  loco->BeginHold(pointerId, t);
}

// ---------------------------------------------------------------------------------------------------------------------
// Town portal (9.4 + W3)
// ---------------------------------------------------------------------------------------------------------------------
PortalRefusal ZoneRuntime::CanUseTownPortal() const {
  if (!zone_impl::ZoneHeroAlive(ctx_)) return PortalRefusal::Dead;
  if (IsPortaling() || ctx_.session.transitioning) return PortalRefusal::Busy;
  if (map_ == nullptr || map_->camps.empty()) return PortalRefusal::NoDestination;
  const Vec2 hero = ctx_.sys.hero->Position();
  const Vec2 dest = NearestCamp(hero);
  const double r = SafeZoneRadius();
  if (DistSq(hero, dest) < r * r) return PortalRefusal::AlreadyAtCamp;
  return PortalRefusal::None;
}

bool ZoneRuntime::UseTownPortal() {
  const PortalRefusal refusal = CanUseTownPortal();
  if (refusal == PortalRefusal::AlreadyAtCamp) {
    ctx_.events.Log(MakeLoc("zone.teleport.alreadyAtCamp"), LogType::System);
    return false;
  }
  if (refusal != PortalRefusal::None) return false;
  portalDestination_ = NearestCamp(ctx_.sys.hero->Position());  // W3: the nearest camp of the zone
  if (ctx_.sys.locomotion != nullptr) ctx_.sys.locomotion->Stop();
  if (ctx_.sys.combat != nullptr) ctx_.sys.combat->ClearAttackTarget();
  pending_ = PendingInteraction{};
  ctx_.events.Log(MakeLoc("zone.teleport.opening"), LogType::System);
  const double channel = ctx_.data.World().constants.townPortalChannelMs;
  portalStartMs_ = ctx_.Now();
  portalTimer_ = ctx_.timers.Schedule(ctx_.Now() + channel, TimerOwner::World,
                                      static_cast<uint16_t>(WorldTimerKind::TownPortal));
  ctx_.events.Emit(EvTownPortal{EvTownPortal::Phase::Started, channel, portalDestination_});
  return true;
}

void ZoneRuntime::CancelTownPortal() {
  if (portalTimer_ == kNoTimer) return;
  ctx_.timers.Cancel(portalTimer_);
  portalTimer_ = kNoTimer;
  ctx_.events.Emit(EvTownPortal{EvTownPortal::Phase::Cancelled, ctx_.data.World().constants.townPortalChannelMs,
                                portalDestination_});
}

// ---------------------------------------------------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------------------------------------------------
const std::string& ZoneRuntime::MapId() const {
  static const std::string kEmpty;
  return map_ != nullptr ? map_->id : kEmpty;
}

const NpcPlacement* ZoneRuntime::FindNpc(std::string_view npcId) const {
  for (const NpcPlacement& n : npcs_) {
    if (n.npcId == npcId) return &n;
  }
  return nullptr;
}

const NpcPlacement* ZoneRuntime::FindNpcEntity(EntityId id) const {
  for (const NpcPlacement& n : npcs_) {
    if (n.id == id) return &n;
  }
  return nullptr;
}

double ZoneRuntime::SafeZoneRadius() const {
  return map_ != nullptr ? map_->safeZoneRadiusEffective : ctx_.data.World().constants.safeZoneRadiusDefault;
}

bool ZoneRuntime::InSafeZone(Vec2 p) const {
  if (map_ == nullptr) return false;
  const double r = SafeZoneRadius();
  for (const MapCampDef& c : map_->camps) {
    if (DistSq(p, Vec2(c.pos.col, c.pos.row)) < r * r) return true;
  }
  return false;
}

bool ZoneRuntime::NearCampfire(Vec2 p) const {
  if (map_ == nullptr) return false;
  const double r = ctx_.data.World().constants.campfireRadiusTiles;
  for (const MapCampDef& c : map_->camps) {
    if (DistSq(p, Vec2(c.pos.col, c.pos.row)) <= r * r) return true;
  }
  return false;
}

Vec2 ZoneRuntime::CampPosition(int32_t index) const {
  if (map_ == nullptr) return Vec2(3, 3);
  if (index >= 0 && static_cast<size_t>(index) < map_->camps.size()) {
    const TilePos c = map_->camps[static_cast<size_t>(index)].pos;
    return Vec2(c.col, c.row);
  }
  return Vec2(map_->playerStart.col, map_->playerStart.row);
}

Vec2 ZoneRuntime::NearestCamp(Vec2 p) const {
  if (map_ == nullptr || map_->camps.empty()) return CampPosition(0);
  size_t best = 0;
  double bestSq = 0;
  for (size_t i = 0; i < map_->camps.size(); ++i) {
    const double d = DistSq(p, map_->camps[i].pos.Center());
    if (i == 0 || d < bestSq) {  // first wins ties
      best = i;
      bestSq = d;
    }
  }
  return map_->camps[best].pos.Center();
}

bool ZoneRuntime::NearestWalkableCamp(Vec2 p, Vec2& out) const {
  if (map_ == nullptr) return false;
  std::vector<TilePos> camps;
  for (const MapCampDef& c : map_->camps) camps.push_back(c.pos);
  TilePos tile;
  const ZoneGrid& g = grid_;
  if (!FindNearestWalkablePosition(p.x, p.y, [&g](int32_t c, int32_t r) { return g.Walkable(c, r); }, g.Cols(),
                                   g.Rows(), camps, tile)) {
    return false;
  }
  out = tile.Center();
  return true;
}

void ZoneRuntime::FillSnapshot(Snapshot& out) const {
  if (map_ == nullptr) return;
  out.zone.mapId = map_->id;
  out.zone.nameKey = map_->nameKey;
  out.zone.cols = map_->cols;
  out.zone.rows = map_->rows;
  out.zone.theme = map_->theme;
  out.zone.levelMin = map_->levelMin;
  out.zone.levelMax = map_->levelMax;
  out.zone.safeZoneRadius = SafeZoneRadius();
  out.zone.camps.clear();
  for (const MapCampDef& c : map_->camps) out.zone.camps.emplace_back(c.pos.col, c.pos.row);
  out.zone.grid = &grid_;
  for (const NpcPlacement& n : npcs_) {
    NpcView v;
    v.id = n.id;
    v.npcId = n.npcId;
    const NpcDef* def = ctx_.data.FindNpc(n.npcId);
    v.artId = def != nullptr && !def->spriteId.empty() ? def->spriteId : n.npcId;
    v.pos = n.pos;
    v.facing = n.facing;
    out.npcs.push_back(std::move(v));
  }
  for (const ExitState& e : exits_) {
    WorldMarkerView v;
    v.kind = MarkerKind::Exit;
    v.pos = Vec2(e.def.pos.col, e.def.pos.row);
    v.key = e.def.targetMap;
    v.sealed = e.sealed;
    v.armed = e.armed;
    out.markers.push_back(std::move(v));
  }
  out.prompt.kind = prompt_.kind;
  out.prompt.id = prompt_.id;
  out.prompt.pos = prompt_.pos;
  out.hero.portaling = IsPortaling();
  const double channel = ctx_.data.World().constants.townPortalChannelMs;
  out.hero.portalProgress =
      IsPortaling() && channel > 0 ? Clamp((ctx_.Now() - portalStartMs_) / channel, 0.0, 1.0) : 0.0;
  out.hero.portalRefusal = CanUseTownPortal();
}

}  // namespace abyss

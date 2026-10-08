// ZoneRuntime (world-map-nav.md 3.4, 7.1, 7.4, 9, 10.3, 12; W3, W7, W8, Q6). Zone construction (EnterZone, NPC
// placement, exits) and queries are implemented; input chains, interact, town portal and transitions are STUBS.
// Owner area: world.
#include "abyss/base/Platform.h"

#include "abyss/world/Zone.h"

#include <array>

#include "abyss/base/Assert.h"
#include "abyss/base/Math.h"
#include "abyss/data/DataStore.h"
#include "abyss/hero/Hero.h"
#include "abyss/sim/SimContext.h"
#include "abyss/sim/Snapshot.h"
#include "abyss/world/MapGen.h"

namespace abyss {

namespace {
// Camp NPC slot offsets (quests-story-ch1.md 6.2).
constexpr std::array<TilePos, 6> kCampNpcOffsets{{{-3, -2}, {3, -2}, {-3, 2}, {3, 2}, {0, -3}, {0, 3}}};
}  // namespace

ZoneRuntime::ZoneRuntime(SimContext& ctx) : ctx_(ctx) {}
ZoneRuntime::~ZoneRuntime() = default;

bool ZoneRuntime::EnterZone(std::string_view mapId, bool hasTarget, Vec2 target) {
  const MapDef* map = ctx_.data.FindMap(mapId);
  if (map == nullptr) map = ctx_.data.FindMap(ctx_.data.World().defaultMap);
  if (map == nullptr) return false;
  map_ = map;
  grid_ = BuildZoneGrid(ctx_.data, *map);
  pathfinder_ = std::make_unique<Pathfinder>(grid_);

  npcs_.clear();
  for (const MapCampDef& camp : map->camps) {
    for (size_t i = 0; i < camp.npcs.size(); ++i) {
      const TilePos off = kCampNpcOffsets[i % kCampNpcOffsets.size()];
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
    exits_.push_back(std::move(s));
  }

  pending_ = PendingInteraction{};
  prompt_ = InteractTarget{};
  portalTimer_ = kNoTimer;
  transitionReady_ = false;
  pendingZone_.clear();

  const Vec2 start = hasTarget ? target : Vec2(map->playerStart.col, map->playerStart.row);
  ctx_.sys.hero->SetPosition(start);
  return true;
}

void ZoneRuntime::RequestZoneChange(std::string_view mapId, Vec2 target) {
  ABYSS_UNIMPLEMENTED();
  (void)pendingTarget_;
}

void ZoneRuntime::ExitZone() {
  if (portalTimer_ != kNoTimer) ctx_.timers.Cancel(portalTimer_);
  portalTimer_ = kNoTimer;
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

void ZoneRuntime::Tick() {
  if (map_ != nullptr) ABYSS_UNIMPLEMENTED();
}

void ZoneRuntime::OnTimer(const Timer& t) { ABYSS_UNIMPLEMENTED(); }

void ZoneRuntime::OnPointerPress(Vec2 tile, PointerButton button) { ABYSS_UNIMPLEMENTED(); }

bool ZoneRuntime::Interact() {
  ABYSS_UNIMPLEMENTED();
  return false;
}

bool ZoneRuntime::InteractWith(EntityId target) {
  ABYSS_UNIMPLEMENTED();
  return false;
}

InteractTarget ZoneRuntime::FindInteractTarget() const {
  ABYSS_UNIMPLEMENTED();
  return {};
}

PortalRefusal ZoneRuntime::CanUseTownPortal() const {
  ABYSS_UNIMPLEMENTED();
  return PortalRefusal::NoDestination;
}

bool ZoneRuntime::UseTownPortal() {
  ABYSS_UNIMPLEMENTED();
  (void)portalDestination_;
  return false;
}

void ZoneRuntime::CancelTownPortal() {
  if (portalTimer_ != kNoTimer) ABYSS_UNIMPLEMENTED();
}

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
  ABYSS_UNIMPLEMENTED();
  return CampPosition(0);
}

bool ZoneRuntime::NearestWalkableCamp(Vec2 p, Vec2& out) const {
  ABYSS_UNIMPLEMENTED();
  return false;
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
  out.hero.portalRefusal = CanUseTownPortal();
}

}  // namespace abyss

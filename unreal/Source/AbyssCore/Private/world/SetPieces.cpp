#include "abyss/base/Platform.h"

#include "abyss/world/SetPieces.h"

#include <cmath>

#include "abyss/base/Math.h"
#include "abyss/data/DataStore.h"
#include "abyss/data/MapData.h"
#include "abyss/data/QuestData.h"
#include "abyss/world/MapGen.h"

namespace abyss {

namespace {

constexpr double kDegToRad = kPi / 180.0;
constexpr double kRadToDeg = 180.0 / kPi;
constexpr double kCmPerTile = 100.0;

double NormalizeDeg(double deg) {
  double d = std::fmod(deg, 360.0);
  if (d < 0.0) d += 360.0;
  if (d >= 360.0) d -= 360.0;  // fmod of a value just below a multiple of 360 plus 360
  return d;
}

// Direction (deg, tile space) from `from` toward `to`; false for a zero-length vector.
bool DirectionDeg(Vec2 from, Vec2 to, double& out) {
  const Vec2 d = to - from;
  if (d.x == 0.0 && d.y == 0.0) return false;
  out = DetAtan2(d.y, d.x) * kRadToDeg;
  return true;
}

double ActorYaw(const SetPieceDef& def, Vec2 pos, Vec2 anchorPoint, Vec2 ringCentre, const SetPieceFrame& frame) {
  const double cameraFront = frame.cameraYawDeg + 180.0;
  double front = cameraFront;
  switch (def.facing) {
    case SetPieceFacingKind::Yaw: return NormalizeDeg(def.facingDeg + def.turnDeg);
    case SetPieceFacingKind::Front: front = def.facingDeg; break;
    case SetPieceFacingKind::Camera: break;
    case SetPieceFacingKind::Tile:
      if (!DirectionDeg(pos, def.toward, front)) front = cameraFront;
      break;
    case SetPieceFacingKind::Anchor:
      if (!DirectionDeg(pos, anchorPoint, front)) front = cameraFront;
      break;
    case SetPieceFacingKind::RingCentre:
      if (!DirectionDeg(pos, ringCentre, front)) front = cameraFront;
      break;
  }
  return NormalizeDeg(front + frame.meshYawOffsetDeg + def.turnDeg);
}

}  // namespace

bool ResolveSetPieceAnchor(const SetPieceDef& def, const MapDef& map, const DataStore& data, Vec2& out) {
  switch (def.anchor) {
    case SetPieceAnchorKind::Tile:
      out = def.tile;
      return true;
    case SetPieceAnchorKind::Camp: {
      if (def.camp < 1 || static_cast<size_t>(def.camp) > map.camps.size()) return false;
      const MapCampDef& camp = map.camps[static_cast<size_t>(def.camp - 1)];
      if (def.ref.empty()) {
        out = camp.pos.Center();
        return true;
      }
      for (const CampProp& prop : CampProps(map)) {
        if (prop.campIndex == def.camp - 1 && prop.type == def.ref) {
          out = prop.centre;
          return true;
        }
      }
      return false;
    }
    case SetPieceAnchorKind::Entrance:
      for (const SubDungeonEntranceDef& e : map.subDungeonEntrances) {
        if (e.id == def.ref) {
          out = e.pos.Center();
          return true;
        }
      }
      return false;
    case SetPieceAnchorKind::Region:
      for (const MapRegionDef& r : map.regions) {
        if (r.id == def.ref) {
          out = r.center.Center();
          return true;
        }
      }
      return false;
    case SetPieceAnchorKind::DefendTarget: {
      const QuestDef* q = data.FindQuest(def.ref);
      if (q == nullptr || !q->hasDefendTarget || q->zone != map.id) return false;
      out = q->defendTarget.pos.Center();
      return true;
    }
  }
  return false;
}

void ExpandSetPieces(const SetPieceZoneDef& zone, const MapDef& map, const DataStore& data, const SetPieceFrame& frame,
                     std::vector<SetPiecePlacement>& out) {
  for (const SetPieceDef& def : zone.pieces) {
    Vec2 anchor;
    if (!ResolveSetPieceAnchor(def, map, data, anchor)) continue;
    const Vec2 base = anchor + def.offset;
    if (!def.hasRing) {
      SetPiecePlacement p;
      p.def = &def;
      p.pos = base;
      p.yawDeg = ActorYaw(def, base, anchor, base, frame);
      p.variant = def.variant;
      out.push_back(p);
      continue;
    }
    const SetPieceRingDef& ring = def.ring;
    for (int32_t k = 0; k < ring.count; ++k) {
      bool skipped = false;
      for (const int32_t s : ring.skip) skipped = skipped || s == k;
      if (skipped) continue;
      const double angle = (ring.startDeg + ring.stepDeg * k) * kDegToRad;
      SetPiecePlacement p;
      p.def = &def;
      p.member = k;
      p.pos = base + Vec2(DetCos(angle), DetSin(angle)) * ring.radius;
      p.yawDeg = ActorYaw(def, p.pos, anchor, base, frame);
      p.variant = ring.variants.empty() ? def.variant : ring.variants[static_cast<size_t>(k) % ring.variants.size()];
      out.push_back(p);
    }
  }
}

Vec2 SetPieceFront(const SetPiecePlacement& p, const SetPieceFrame& frame) {
  const double a = (p.yawDeg - frame.meshYawOffsetDeg) * kDegToRad;
  return Vec2(DetCos(a), DetSin(a));
}

Vec2 SetPieceLocalToTile(const SetPiecePlacement& p, double localXCm, double localYCm) {
  const double scale = p.def != nullptr ? p.def->scale : 1.0;
  const double a = p.yawDeg * kDegToRad;
  const double c = DetCos(a);
  const double s = DetSin(a);
  const double x = localXCm * scale / kCmPerTile;
  const double y = localYCm * scale / kCmPerTile;
  return p.pos + Vec2(x * c - y * s, x * s + y * c);
}

void SetPieceCoveredTiles(const SetPiecePlacement& p, double minXCm, double minYCm, double maxXCm, double maxYCm,
                          double padTiles, std::vector<TilePos>& out) {
  const double scale = p.def != nullptr ? p.def->scale : 1.0;
  const double x0 = minXCm * scale / kCmPerTile - padTiles;
  const double x1 = maxXCm * scale / kCmPerTile + padTiles;
  const double y0 = minYCm * scale / kCmPerTile - padTiles;
  const double y1 = maxYCm * scale / kCmPerTile + padTiles;
  if (!(x0 <= x1) || !(y0 <= y1)) return;
  // Every tile centre within the rectangle's circumscribed circle, tested in the mesh frame.
  const double reach = std::sqrt((std::max)(x0 * x0, x1 * x1) + (std::max)(y0 * y0, y1 * y1));
  const int32_t c0 = FloorInt(p.pos.x - reach);
  const int32_t c1 = CeilInt(p.pos.x + reach);
  const int32_t r0 = FloorInt(p.pos.y - reach);
  const int32_t r1 = CeilInt(p.pos.y + reach);
  const double a = p.yawDeg * kDegToRad;
  const double c = DetCos(a);
  const double s = DetSin(a);
  for (int32_t r = r0; r <= r1; ++r) {
    for (int32_t col = c0; col <= c1; ++col) {
      const double dx = col - p.pos.x;
      const double dy = r - p.pos.y;
      const double lx = dx * c + dy * s;   // the inverse rotation
      const double ly = -dx * s + dy * c;
      if (lx >= x0 && lx <= x1 && ly >= y0 && ly <= y1) out.push_back(TilePos{col, r});
    }
  }
}

}  // namespace abyss

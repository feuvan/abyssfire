// Quest guidance (quests-story-ch1.md 3.4 gather spots, 3.5 nearestWalkable, 5.1 computeGuideTarget). Owner area:
// quests+story. Pure functions: every world fact comes through GuideWorld.
#include "abyss/base/Platform.h"

#include "abyss/quests/QuestGuide.h"

#include <cmath>
#include <cstdlib>

#include "abyss/base/Math.h"
#include "abyss/quests/QuestSystem.h"

namespace abyss {

namespace {

// nearest (QuestGuide.ts): squared distance, strictly smaller wins (first on ties).
bool QgNearest(Vec2 from, const std::vector<Vec2>& pts, Vec2& out) {
  bool found = false;
  double best = 0;
  for (const Vec2& p : pts) {
    const double dx = p.x - from.x, dy = p.y - from.y;
    const double d = dx * dx + dy * dy;
    if (!found || d < best) {
      found = true;
      best = d;
      out = p;
    }
  }
  return found;
}

// UTF-16 code units of a UTF-8 string (JS charCodeAt). Malformed bytes count as one unit each.
std::vector<uint16_t> QgUtf16Units(std::string_view s) {
  std::vector<uint16_t> out;
  size_t i = 0;
  while (i < s.size()) {
    const auto b0 = static_cast<uint8_t>(s[i]);
    uint32_t cp = b0;
    size_t len = 1;
    if (b0 >= 0xF0) {
      len = 4;
    } else if (b0 >= 0xE0) {
      len = 3;
    } else if (b0 >= 0xC0) {
      len = 2;
    }
    if (len > 1 && i + len <= s.size()) {
      cp = len == 2 ? (b0 & 0x1Fu) : len == 3 ? (b0 & 0x0Fu) : (b0 & 0x07u);
      for (size_t k = 1; k < len; ++k) cp = (cp << 6) | (static_cast<uint8_t>(s[i + k]) & 0x3Fu);
    } else {
      len = 1;
      cp = b0;
    }
    if (cp >= 0x10000u) {
      cp -= 0x10000u;
      out.push_back(static_cast<uint16_t>(0xD800u + (cp >> 10)));
      out.push_back(static_cast<uint16_t>(0xDC00u + (cp & 0x3FFu)));
    } else {
      out.push_back(static_cast<uint16_t>(cp));
    }
    i += len;
  }
  return out;
}

Vec2 QgCentre(const TileCircle& c) { return Vec2(c.col, c.row); }

// objectiveTarget (5.1), first match wins. False = no target for this objective (the caller tries the next one).
bool QgObjectiveTarget(const QuestDef& quest, const QuestObjectiveDef& obj, int32_t index, const GuideWorld& world,
                       GuideTarget& out) {
  if (obj.type == ObjectiveType::Escort && world.escortTile) {
    Vec2 e;
    if (world.escortTile(e) && JsHypot(e.x - world.player.x, e.y - world.player.y) > 4) {
      out.kind = GuideTargetKind::Escort;
      out.pos = e;
      return true;
    }
  }
  if (obj.type == ObjectiveType::InvestigateClue && world.clueTile) {
    Vec2 c;
    if (world.clueTile(quest.id, index, c)) {
      out.kind = GuideTargetKind::Clue;
      out.pos = c;
      return true;
    }
  }
  if (obj.hasLocation) {
    out.kind = GuideTargetKind::Point;
    out.pos = QgCentre(obj.location);
    return true;
  }
  switch (obj.type) {
    case ObjectiveType::Talk: {
      // Returned even when the NPC is elsewhere: no target, the loop moves on.
      Vec2 p;
      if (world.npcTile && world.npcTile(obj.targetId, p)) {
        out.kind = GuideTargetKind::Npc;
        out.pos = p;
        return true;
      }
      return false;
    }
    case ObjectiveType::DefendWave:
      if (!quest.hasDefendTarget) return false;
      out.kind = GuideTargetKind::Defend;
      out.pos = quest.defendTarget.pos.Center();
      return true;
    case ObjectiveType::Escort:
      if (!quest.hasEscortNpc) return false;
      out.kind = GuideTargetKind::Escort;
      out.pos = quest.escortNpc.start.Center();
      return true;
    case ObjectiveType::CraftCraft:
    case ObjectiveType::CraftDeliver: {
      if (!quest.craftPhases.present) return false;
      const std::string& npc =
          obj.type == ObjectiveType::CraftCraft ? quest.craftPhases.craftNpc : quest.craftPhases.deliverNpc;
      Vec2 p;
      if (world.npcTile && world.npcTile(npc, p)) {
        out.kind = GuideTargetKind::Npc;
        out.pos = p;
        return true;
      }
      return false;
    }
    case ObjectiveType::Collect:
    case ObjectiveType::CraftCollect:
      if (obj.sourceKind == ItemSourceKind::Gather) {
        std::vector<Vec2> spots;
        if (world.gatherSpots) world.gatherSpots(quest.id, index, spots);
        Vec2 n;
        if (QgNearest(world.player, spots, n)) {
          out.kind = GuideTargetKind::GatherNode;
          out.pos = n;
        } else {
          out.kind = GuideTargetKind::Area;
          out.pos = QgCentre(obj.gatherArea);
        }
        return true;
      }
      break;
    default:
      break;
  }
  const std::vector<std::string> ids = ObjectiveMonsters(obj);
  if (ids.empty()) {
    if (!quest.hasQuestArea) return false;
    out.kind = GuideTargetKind::Area;
    out.pos = QgCentre(quest.questArea);
    return true;
  }
  std::vector<Vec2> pts;
  if (world.monsters) world.monsters(ids, pts);
  Vec2 n;
  if (QgNearest(world.player, pts, n)) {
    out.kind = GuideTargetKind::Monster;
    out.pos = n;
    return true;
  }
  pts.clear();
  if (world.spawns) world.spawns(ids, pts);
  if (QgNearest(world.player, pts, n)) {
    out.kind = GuideTargetKind::Spawn;
    out.pos = n;
    return true;
  }
  if (obj.type == ObjectiveType::Kill) {
    bool isHunt = false;
    for (const QuestHuntRef& h : quest.hunts) isHunt = isHunt || h.huntId == obj.targetId;
    Vec2 h;
    if (isHunt && world.huntTile && world.huntTile(obj.targetId, h)) {
      out.kind = GuideTargetKind::Spawn;
      out.pos = h;
      return true;
    }
  }
  if (!quest.hasQuestArea) return false;
  out.kind = GuideTargetKind::Area;
  out.pos = QgCentre(quest.questArea);
  return true;
}

}  // namespace

std::vector<std::string> ObjectiveMonsters(const QuestObjectiveDef& obj) {
  if (obj.type == ObjectiveType::Kill) return {obj.targetId};
  if ((obj.type == ObjectiveType::Collect || obj.type == ObjectiveType::CraftCollect) &&
      obj.sourceKind == ItemSourceKind::Drop) {
    return obj.dropMonsters;
  }
  return {};
}

GuideTarget ComputeGuideTarget(const QuestDef& quest, const QuestProgress& progress, const GuideWorld& world) {
  GuideTarget out;
  out.questId = quest.id;
  if (progress.status == QuestStatus::Completed) {
    std::string giver;
    Vec2 tile;
    if (world.giverOf && world.giverOf(quest.id, giver) && world.npcTile && world.npcTile(giver, tile)) {
      out.kind = GuideTargetKind::Npc;
      out.pos = tile;
      out.reason = GuideReason::TurnIn;
      out.objectiveIndex = -1;
    }
    return out;
  }
  if (progress.status != QuestStatus::Active) return out;
  for (size_t i = 0; i < quest.objectives.size(); ++i) {
    const QuestObjectiveDef& obj = quest.objectives[i];
    const int32_t current = i < progress.objectives.size() ? progress.objectives[i] : 0;
    if (current >= obj.required) continue;
    GuideTarget t;
    t.questId = quest.id;
    if (QgObjectiveTarget(quest, obj, static_cast<int32_t>(i), world, t)) {
      t.reason = GuideReason::Objective;
      t.objectiveIndex = static_cast<int32_t>(i);
      return t;
    }
  }
  return out;
}

GatherSpotRng::GatherSpotRng(std::string_view seedKey) {
  for (uint16_t c : QgUtf16Units(seedKey)) h_ = (h_ ^ c) * 16777619u;
}

double GatherSpotRng::Next() {
  h_ = (h_ ^ (h_ >> 15)) * 2246822519u;
  h_ = (h_ ^ (h_ >> 13)) * 3266489917u;
  h_ ^= h_ >> 16;
  return static_cast<double>(h_) / 4294967296.0;
}

std::vector<TilePos> ResolveGatherSpots(const TileCircle& area, int32_t count,
                                        const std::function<bool(int32_t, int32_t)>& walkable,
                                        std::string_view seedKey) {
  std::vector<TilePos> out;
  if (count <= 0) return out;
  GatherSpotRng rng(seedKey);
  const int64_t attempts = static_cast<int64_t>(count) * 60;
  for (int64_t attempt = 0; attempt < attempts && static_cast<int32_t>(out.size()) < count; ++attempt) {
    const double a = rng.Next() * kPi * 2;
    const double r = std::sqrt(rng.Next()) * area.radius;
    const double cx = static_cast<double>(area.col) + std::cos(a) * r;
    const double cy = static_cast<double>(area.row) + std::sin(a) * r;
    const TilePos p(JsRoundInt(cx), JsRoundInt(cy));
    if (!walkable || !walkable(p.col, p.row)) continue;
    bool tooClose = false;
    for (const TilePos& q : out) tooClose = tooClose || ManhattanDist(p, q) < 3;
    if (tooClose) continue;
    out.push_back(p);
  }
  return out;
}

bool NearestWalkableTile(TilePos tile, const std::function<bool(int32_t, int32_t)>& walkable, int32_t maxRing,
                         TilePos& out) {
  if (!walkable) return false;
  if (walkable(tile.col, tile.row)) {
    out = tile;
    return true;
  }
  for (int32_t ring = 1; ring <= maxRing; ++ring) {
    for (int32_t dr = -ring; dr <= ring; ++dr) {
      for (int32_t dc = -ring; dc <= ring; ++dc) {
        if ((std::max)(std::abs(dc), std::abs(dr)) != ring) continue;
        if (walkable(tile.col + dc, tile.row + dr)) {
          out = TilePos(tile.col + dc, tile.row + dr);
          return true;
        }
      }
    }
  }
  return false;
}

}  // namespace abyss

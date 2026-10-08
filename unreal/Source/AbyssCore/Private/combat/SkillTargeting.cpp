// Skill targeting geometry (classes-stats-skills.md 9.2, 9.7, 10; C1, C4, C8). STUB: owner area hero+combat.
#include "abyss/base/Platform.h"

#include "abyss/combat/SkillTargeting.h"

#include "abyss/base/Assert.h"

namespace abyss {

EntityId NearestAlive(std::span<const TargetCandidate> candidates, Vec2 from, double maxRange) {
  ABYSS_UNIMPLEMENTED();
  return kNoEntity;
}

EntityId FindGroundAoeAnchor(std::span<const TargetCandidate> candidates, Vec2 heroPos, EntityId target,
                             Vec2 targetPos, bool targetAlive, double skillRange) {
  ABYSS_UNIMPLEMENTED();
  return kNoEntity;
}

std::vector<EntityId> CandidatesInRadius(std::span<const TargetCandidate> candidates, Vec2 centre, double radius) {
  ABYSS_UNIMPLEMENTED();
  return {};
}

std::vector<EntityId> CandidatesAlongLine(std::span<const TargetCandidate> candidates, Vec2 heroPos, Vec2 targetPos,
                                          EntityId target, double range, double halfWidth) {
  ABYSS_UNIMPLEMENTED();
  return {};
}

std::vector<EntityId> CandidatesInCone(std::span<const TargetCandidate> candidates, Vec2 heroPos, Vec2 aimDir,
                                       double radius, double coneDeg) {
  ABYSS_UNIMPLEMENTED();
  return {};
}

bool ComputeTeleportDestination(const SkillPortDef& port, Vec2 heroPos, const TeleportAim& aim, int32_t cols,
                                int32_t rows, const WalkableFn& walkable, TilePos& outTile) {
  ABYSS_UNIMPLEMENTED();
  return false;
}

TilePos ShadowStepDestination(Vec2 heroPos, Vec2 targetPos, int32_t cols, int32_t rows, const WalkableFn& walkable) {
  ABYSS_UNIMPLEMENTED();
  return RoundToTile(targetPos);
}

Vec2 ChargeDashEnd(Vec2 heroPos, Vec2 targetPos, double stopRange) {
  ABYSS_UNIMPLEMENTED();
  return heroPos;
}

}  // namespace abyss

// Soul echo / death penalty (combat-feel.md section 15). STUB: owner area hero+combat.
#include "abyss/base/Platform.h"

#include "abyss/combat/SoulEcho.h"

#include "abyss/base/Assert.h"
#include "abyss/save/SaveData.h"
#include "abyss/sim/SimContext.h"
#include "abyss/sim/Snapshot.h"

namespace abyss {

DeathPenalty ComputeDeathPenalty(const SoulEchoDef& def, int32_t level, int64_t gold, int64_t exp, int64_t expToNext,
                                 Difficulty difficulty) {
  ABYSS_UNIMPLEMENTED();
  return {};
}

bool SoulEchoState::Leave(const SoulEchoData& echo, bool& hadPrevious, SoulEchoData& faded) {
  ABYSS_UNIMPLEMENTED();
  hadPrevious = false;
  return false;
}

bool SoulEchoState::TryClaim(std::string_view mapId, Vec2 heroPos, double rangeTiles, SoulEchoData& claimed) {
  ABYSS_UNIMPLEMENTED();
  return false;
}

bool SoulEchoState::Load(const std::string& mapId, double col, double row, int64_t gold, int64_t exp) {
  ABYSS_UNIMPLEMENTED();
  return false;
}

SoulEchoSystem::SoulEchoSystem(SimContext& ctx) : ctx_(ctx) {}

void SoulEchoSystem::OnHeroDied(Vec2 pos, bool inDungeon) { ABYSS_UNIMPLEMENTED(); }

void SoulEchoSystem::Tick() {
  if (state_.Has()) ABYSS_UNIMPLEMENTED();
}

void SoulEchoSystem::OnZoneEnter() {
  if (state_.Has()) ABYSS_UNIMPLEMENTED();
  (void)propId_;
}

void SoulEchoSystem::FillSnapshot(Snapshot& out) const { out.soulEcho = &state_; }

void SoulEchoSystem::WriteSave(SaveData& out) const { ABYSS_UNIMPLEMENTED(); }

void SoulEchoSystem::ReadSave(const SaveData& in) { ABYSS_UNIMPLEMENTED(); }

}  // namespace abyss

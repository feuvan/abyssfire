// Soul echo: the death penalty and the corpse run (combat-feel.md section 15; save-ui-input.md 3.2 `soulEcho`).
//
// Owner area: hero+combat. `ComputeDeathPenalty` and `SoulEchoState` are pure; `SoulEchoSystem` is the runtime glue
// (applies the penalty on death, places/claims the echo, emits events, requests the autosave on claim).
#pragma once

#include <cstdint>
#include <string>

#include "abyss/base/Enums.h"
#include "abyss/base/Platform.h"
#include "abyss/base/Types.h"
#include "abyss/data/CombatData.h"

namespace abyss {

struct SimContext;
struct Snapshot;
struct SaveData;

struct DeathPenalty {
  int64_t gold = 0;
  int64_t exp = 0;
};

// computeDeathPenalty (15): level < minLevel -> {0,0}; gold = floor(max(0, gold) * goldShare[diff]);
// exp = min(max(0, exp), floor(expToNext * expShare[diff])).
ABYSS_API DeathPenalty ComputeDeathPenalty(const SoulEchoDef& def, int32_t level, int64_t gold, int64_t exp, int64_t expToNext,
                                           Difficulty difficulty);

struct SoulEchoData {
  std::string mapId;
  int32_t col = 0, row = 0;
  int64_t gold = 0;
  int64_t exp = 0;
};

class ABYSS_API SoulEchoState {
 public:
  // leave(): the previous echo (if any) is returned through `faded` (log zone.soulEcho.faded); the new echo is stored
  // only when gold > 0 || exp > 0. Returns true when an echo was stored.
  bool Leave(const SoulEchoData& echo, bool& hadPrevious, SoulEchoData& faded);
  // tryClaim: same map and hypot(dcol, drow) <= range -> returns the echo and clears it.
  bool TryClaim(std::string_view mapId, Vec2 heroPos, double rangeTiles, SoulEchoData& claimed);
  bool Has() const { return has_; }
  const SoulEchoData& Echo() const { return echo_; }
  void Clear() { has_ = false; }
  // load(): rejected when col/row are not finite (the caller passes doubles from the save).
  bool Load(const std::string& mapId, double col, double row, int64_t gold, int64_t exp);

 private:
  bool has_ = false;
  SoulEchoData echo_;
};

class ABYSS_API SoulEchoSystem {
 public:
  explicit SoulEchoSystem(SimContext& ctx);

  // Hero death (combat 13.3 step 2): applies the penalty to the hero; overworld -> Leave() + logs + EvEntitySpawned
  // of the echo prop; dungeon -> permanent loss + zone.soulEcho.lostInDungeon.
  void OnHeroDied(Vec2 pos, bool inDungeon);
  // Per step while the hero is alive and the echo is in this zone: claim within claimRangeTiles -> gold + exp,
  // log zone.soulEcho.claimed, SFX resonance, SaveRequestMsg{SoulEchoClaimed}.
  void Tick();
  void OnZoneEnter();  // spawns the echo prop when it lies in this zone
  const SoulEchoState& State() const { return state_; }

  void FillSnapshot(Snapshot& out) const;
  void WriteSave(SaveData& out) const;
  void ReadSave(const SaveData& in);

 private:
  SimContext& ctx_;
  SoulEchoState state_;
  EntityId propId_ = kNoEntity;
};

}  // namespace abyss

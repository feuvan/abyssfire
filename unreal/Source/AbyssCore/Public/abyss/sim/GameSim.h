// GameSim: the facade the UE module talks to (ARCHITECTURE 3.1). One instance per play session.
//
// Clock (S1, S2, S6, D13 classes 19.1 step algorithm):
// * Step() runs exactly one 60 Hz tick when the world is not frozen: drain due timers (ascending due, then scheduling
//   order) -> stop if a timer froze the world -> apply queued commands -> Update(kSimStepMs) in the classes 16 order.
//   While frozen it only applies the commands that may unfreeze (UI / story / panels) and returns.
// * Frame(realDtMs) is the convenience UE calls once per rendered frame: clamps to maxFrameMs, scales by the S6
//   dilation, accumulates, runs 0..maxStepsPerFrame steps, then AdvanceRealTime(realDtMs). On freeze begin it runs
//   OnFreezeBegin (F1 / F2) once and drops the accumulator (no catch-up).
// * AdvanceRealTime(realMs): presentation-clock work owned by the core (story step player, music director timers).
//
// Events: Events() returns everything emitted since the last ClearEvents(); Frame() and Step() clear at their start.
// Snapshot: View() is rebuilt after every Frame / Step / Submit batch.
//
// Owner: lead (contract). Implementation: Private/sim/ (GameSim.cpp step loop, SimWiring.cpp bus subscriptions and
// kill pipeline order, SimCommands.cpp command routing, SimSave.cpp save build / apply order, SimSnapshot.cpp).
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "abyss/base/Enums.h"
#include "abyss/base/Platform.h"
#include "abyss/save/SaveIO.h"
#include "abyss/sim/Commands.h"
#include "abyss/sim/Events.h"
#include "abyss/sim/SimContext.h"
#include "abyss/sim/Snapshot.h"

namespace abyss {

class DataStore;
struct SaveData;
struct SimImpl;

class ABYSS_API GameSim {
 public:
  // The DataStore must be finalized (DataStore::LoadAll succeeded) and outlive the GameSim. Returns nullptr - after
  // reporting through the assert handler, which may return under UE - when it is not: UE must check the result and
  // show the data error instead of starting.
  static std::unique_ptr<GameSim> Create(const DataStore& data, const SimConfig& cfg);
  ~GameSim();
  GameSim(const GameSim&) = delete;
  GameSim& operator=(const GameSim&) = delete;

  // ---- session ----
  // New hero at emerald_plains playerStart (world 9.6), starter skills, RNG streams seeded from `seed` (0 = config
  // default), prologue queued (story 8.2). Returns false for an unknown class or a missing default map.
  bool NewGame(ClassId cls, Difficulty difficulty, uint64_t seed, int32_t slot = 0);
  // ParseSave + ApplySave (save 3.5 order, FIX Q8 / Q35). Returns SaveError::None on success. On failure `err` is set
  // and the current session is unchanged (parse / version / class errors) - except Invalid from a zone that cannot be
  // entered at all (broken data), which leaves no session. UE shows a message for VersionTooNew (save from a newer
  // build) distinct from ParseFailed / NotAnObject / Invalid (corrupt save: offer the .bak).
  // difficultyOverride: the Continue -> difficulty selector choice (save-ui-input 1.2, Q33: hero, map and position are
  // kept). Must not be Locked by DeriveCompletedDifficulties(saved difficulty, saved list) (else Invalid).
  SaveError LoadGame(std::string_view saveJson, std::string* err,
                     std::optional<Difficulty> difficultyOverride = std::nullopt);
  // An already parsed save (tools, tests, the selector flow after SummarizeSave).
  SaveError LoadGame(const SaveData& save, std::string* err);
  // BuildSave (save 3.4) serialised as v4 JSON. A Dying hero is saved as respawned at the camp (C12); UE should
  // prefer to call it when CanSave() or after CmdResolvePendingDeath.
  std::string SaveGame(int64_t unixMs = 0) const;
  bool CanSave() const;  // Alive && hp > 0 && !transitioning
  void BuildSave(SaveData& out, int64_t unixMs) const;

  // ---- loop ----
  void Step();
  int32_t Frame(double realDtMs);  // returns the number of sim steps run
  void AdvanceRealTime(double realMs);
  void Submit(const Command& cmd);

  // ---- output ----
  std::span<const Event> Events() const;
  void ClearEvents();
  const Snapshot& View() const;

  // ---- state queries (tests, tools, UE flow) ----
  bool WorldFrozen() const;
  bool HasSession() const;
  double NowMs() const;
  SimContext& Context();  // tests / debug tools only
  const SimContext& Context() const;

 private:
  explicit GameSim(std::unique_ptr<SimImpl> impl);
  std::unique_ptr<SimImpl> impl_;
};

}  // namespace abyss

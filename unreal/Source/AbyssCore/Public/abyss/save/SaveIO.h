// Save JSON: migration on the generic tree, typed parse with the load normalisations, stable serialisation, the
// position fallback, and the storage interface UE implements.
// Spec: save-ui-input.md 3.1 (storage), 3.3 (v1 -> v2 -> v3 migrations; v3 -> v4 adds defaults; refuse version > current),
// 3.5 (normalisations), 3.7 (findNearestWalkablePosition + test vectors), 3.10 (port design: tmp + rename + .bak),
// 11 (API proposal); DECISIONS U1 (3 slots), U2 (v4, Saved/SaveGames/abyssfire_slot{N}.json), U3 (RapidJSON via
// base/Json; own float parser).
//
// Owner area: world. Per-section readers/writers live in Private/save/ and are split by area so each area can edit
// its own file: SaveItems.cpp (items), SaveQuests.cpp (quests / story / dialogue / achievements / lore), SavePets.cpp
// (pets / homestead), SaveHero.cpp (hero + combat), SaveIO.cpp (top level, world).
#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "abyss/base/Json.h"
#include "abyss/base/Platform.h"
#include "abyss/base/Types.h"
#include "abyss/save/SaveData.h"

namespace abyss {

enum class SaveError : uint8_t { None, ParseFailed, NotAnObject, VersionTooNew, Invalid };
ABYSS_ENUM_STRINGS(SaveError, "none", "parseFailed", "notAnObject", "versionTooNew", "invalid")

// migrateSaveData on the generic tree (3.3): v1 -> v2 -> v3 -> v4, each step only when version < N; idempotent.
// Returns false when the root is not an object.
ABYSS_API bool MigrateRaw(JsonValue& raw);

// ParseSave: JSON -> MigrateRaw -> typed SaveData with every non-version-gated normalisation of 3.3 that does not need
// game data (identified = true, abyss clamps, soul echo finite check, storySeen default). Data-dependent
// normalisations (quest objective counts, migratePetSave, homestead load) run in the systems' ReadSave. Non-finite
// numbers survive parsing as written (JSON has none; a hand-edited 1e999 is a parse error): GameSim treats a non-finite
// hp as a dead save and a non-finite tileCol / tileRow like a dead save's position (camps[0]).
// Errors: ParseFailed (not JSON / too deep, kMaxJsonDepth), NotAnObject, VersionTooNew (UE shows a message: the save is
// from a newer build), Invalid (shape errors).
ABYSS_API SaveError ParseSave(std::string_view json, SaveData& out, std::string* errorMessage = nullptr);

// SerializeSave: stable key order = 3.2 then the v4 fields; numbers via FormatJsonNumber (shortest round trip).
ABYSS_API std::string SerializeSave(const SaveData& s, bool pretty = false);

// findNearestWalkablePosition (3.7): rc = JsRound(col), rr = JsRound(row); walkable -> false (no reset); unwalkable /
// out of bounds and no camps -> false; else the camp minimising (camp - pos)^2 (unrounded; first wins ties) -> true.
ABYSS_API bool FindNearestWalkablePosition(double col, double row,
                                           const std::function<bool(int32_t, int32_t)>& walkable, int32_t cols,
                                           int32_t rows, std::span<const TilePos> camps, TilePos& out);

// ---- per-section JSON helpers (shared by SaveIO and tests) ----
ABYSS_API void WriteItemJson(JsonWriter& w, const ItemInstance& item);
ABYSS_API bool ReadItemJson(const JsonValue& v, ItemInstance& out);
ABYSS_API void WriteQuestProgressJson(JsonWriter& w, const QuestProgress& p);
ABYSS_API bool ReadQuestProgressJson(const JsonValue& v, QuestProgress& out);
ABYSS_API void WritePetJson(JsonWriter& w, const PetInstance& p);
ABYSS_API bool ReadPetJson(const JsonValue& v, PetInstance& out);

// Storage implemented by UE (files under Saved/SaveGames; write *.tmp -> flush -> rename, keep *.bak and fall back
// to it when the main file fails to parse). The core never touches the file system.
struct SaveSlotInfo {
  int32_t slot = 0;
  bool exists = false;
  int64_t timestamp = 0;
  ClassId classId = ClassId::Warrior;
  int32_t level = 1;
  std::string mapId;
  double playTimeMs = 0;
  // Continue card line 2 ("<zone> · <difficulty>") and the difficulty selector (save-ui-input 1.2): the saved
  // difficulty and DeriveCompletedDifficulties(difficulty, save list); showDifficultySelector =
  // ShouldShowDifficultySelector(difficulty, completedDifficulties). The selector's choice goes to
  // GameSim::LoadGame(json, err, difficultyOverride).
  Difficulty difficulty = Difficulty::Normal;
  std::vector<Difficulty> completedDifficulties;
  bool showDifficultySelector = false;
};

class ISaveStorage {
 public:
  virtual ~ISaveStorage() = default;
  virtual bool Read(int32_t slot, std::string& out) = 0;
  virtual bool Write(int32_t slot, std::string_view bytes) = 0;
  virtual bool Remove(int32_t slot) = 0;
  virtual void List(std::vector<SaveSlotInfo>& out) = 0;
};

// Slot summary for the menu (U1) from a parsed save.
ABYSS_API SaveSlotInfo SummarizeSave(int32_t slot, const SaveData& s);

}  // namespace abyss

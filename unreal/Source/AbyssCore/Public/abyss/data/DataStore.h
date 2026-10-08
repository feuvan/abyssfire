// DataStore: every exported table (Data/*.json, see Data/README.md) parsed into typed, immutable C++ structs.
//
// * Bytes in: the host (UE: IPlatformFile / FFileHelper; tests: std::ifstream) reads each file and hands the bytes
//   over through a FileReader callback; the core never opens files (ue58-platform.md 3.3, 10.2).
// * Errors are reported in a DataLoadReport (file, JSON path, message) - no exceptions. A store with errors must not
//   be used to start a GameSim.
// * After LoadAll succeeds the store is immutable; pointers/references into it stay valid for its lifetime, so
//   subsystems keep `const XDef*` freely. One DataStore can back several GameSim instances (tests).
// * Tables that only the offline pipelines use (terrain_styles.json, music themes/scores) are kept as raw JSON
//   (RawTable) after a successful parse.
#pragma once

#include <array>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "abyss/base/I18n.h"
#include "abyss/base/Json.h"
#include "abyss/base/Platform.h"
#include "abyss/data/AssetManifest.h"
#include "abyss/data/AudioData.h"
#include "abyss/data/ClassData.h"
#include "abyss/data/CombatData.h"
#include "abyss/data/DialogueData.h"
#include "abyss/data/ItemData.h"
#include "abyss/data/LoreData.h"
#include "abyss/data/MapData.h"
#include "abyss/data/MonsterData.h"
#include "abyss/data/NpcData.h"
#include "abyss/data/PetData.h"
#include "abyss/data/QuestData.h"
#include "abyss/data/SkillData.h"
#include "abyss/data/StoryData.h"
#include "abyss/data/UiData.h"

namespace abyss {

// Schema version the loader understands (envelope field "schemaVersion" of every file).
inline constexpr int32_t kDataSchemaVersion = 1;

struct DataIssue {
  std::string file;     // "classes.json"
  std::string path;     // JSON path, e.g. "classes[0].skills[3].manaCost"
  std::string message;  // English, for logs and test output
};

struct ABYSS_API DataLoadReport {
  std::vector<DataIssue> errors;
  std::vector<DataIssue> warnings;
  std::vector<std::string> loadedFiles;
  bool Ok() const { return errors.empty(); }
  // Multi-line human-readable summary (first `maxLines` issues).
  std::string Summary(size_t maxLines = 40) const;
};

// Returns the bytes of a data file by name ("classes.json"); false when it does not exist / cannot be read.
using DataFileReader = std::function<bool(std::string_view fileName, std::string& outBytes)>;

// Every table file the store expects (required unless listed in kOptionalDataFiles).
ABYSS_API std::span<const std::string_view> RequiredDataFiles();
ABYSS_API std::span<const std::string_view> OptionalDataFiles();  // assets.json, index.json, i18n_zh-TW.json

class ABYSS_API DataStore {
 public:
  DataStore();
  ~DataStore();
  DataStore(const DataStore&) = delete;
  DataStore& operator=(const DataStore&) = delete;
  DataStore(DataStore&&) noexcept;
  DataStore& operator=(DataStore&&) noexcept;

  // Loads every required + optional file through `read`, then cross-validates the tables (Finalize). Returns
  // report.Ok(). Calling it twice reloads from scratch.
  bool LoadAll(const DataFileReader& read, DataLoadReport& report);

  // Lower-level API (tests, tools): parse one file's bytes into its table, then Finalize once at the end.
  bool LoadFile(std::string_view fileName, std::string_view bytes, DataLoadReport& report);
  bool Finalize(DataLoadReport& report);
  bool IsFinalized() const { return finalized_; }

  // ---- typed tables ----
  const ClassTables& Classes() const { return classes_; }
  const CombatTables& Combat() const { return combat_; }
  const ItemTables& Items() const { return items_; }
  const MonsterTables& Monsters() const { return monsters_; }
  const WorldTables& World() const { return world_; }
  const QuestTables& Quests() const { return quests_; }
  const NpcTables& Npcs() const { return npcs_; }
  const DialogueTables& Dialogues() const { return dialogues_; }
  const StoryScript& Story() const { return story_; }
  const LoreTables& Lore() const { return lore_; }
  const PetTables& Pets() const { return pets_; }
  const HomesteadTables& Homestead() const { return homestead_; }
  const AudioTables& Audio() const { return audio_; }
  const AssetManifest& Assets() const { return assets_; }
  const UiThemeDef& UiTheme() const { return uiTheme_; }
  const RenderQualityTables& RenderQualityProfiles() const { return renderQuality_; }
  const AbyssRunTables& AbyssRun() const { return abyssRun_; }

  // ---- strings ----
  const I18n& Strings() const { return i18n_; }
  I18n& MutableStrings() { return i18n_; }  // locale switching only

  // ---- raw JSON kept for offline/presentation-only tables (terrain_styles.json, music.json) ----
  const JsonValue* RawTable(std::string_view fileName) const;

  // Convenience lookups used everywhere.
  const SkillDef* FindSkill(std::string_view id) const { return classes_.FindSkill(id); }
  const ItemBaseDef* FindItemBase(std::string_view id) const { return items_.FindBase(id); }
  const MonsterDef* FindMonster(std::string_view id) const { return monsters_.Find(id); }
  const MapDef* FindMap(std::string_view id) const { return world_.FindMap(id); }
  const QuestDef* FindQuest(std::string_view id) const { return quests_.Find(id); }
  const NpcDef* FindNpc(std::string_view id) const { return npcs_.Find(id); }
  const PetDef* FindPet(std::string_view id) const { return pets_.Find(id); }

 private:
  void Reset();

  bool finalized_ = false;
  ClassTables classes_;
  CombatTables combat_;
  ItemTables items_;
  MonsterTables monsters_;
  WorldTables world_;
  QuestTables quests_;
  NpcTables npcs_;
  DialogueTables dialogues_;
  StoryScript story_;
  LoreTables lore_;
  PetTables pets_;
  HomesteadTables homestead_;
  AudioTables audio_;
  AssetManifest assets_;
  UiThemeDef uiTheme_;
  RenderQualityTables renderQuality_;
  AbyssRunTables abyssRun_;
  I18n i18n_;
  std::vector<std::pair<std::string, JsonValue>> raw_;
  std::vector<std::string> loadedFiles_;
};

}  // namespace abyss

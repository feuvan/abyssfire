// Private helpers for the table loaders: typed reads from a JsonValue with JSON-path error reporting.
#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/Json.h"
#include "abyss/base/Stats.h"
#include "abyss/base/Types.h"
#include "abyss/data/DataStore.h"

namespace abyss::dataload {

class LoadCtx {
 public:
  LoadCtx(std::string_view file, DataLoadReport& report) : file_(file), report_(&report) {}
  void Error(const std::string& path, std::string message);
  void Warn(const std::string& path, std::string message);
  const std::string& File() const { return file_; }
  size_t ErrorCount() const { return errors_; }

 private:
  std::string file_;
  DataLoadReport* report_;
  size_t errors_ = 0;
};

class JNode {
 public:
  JNode(const JsonValue* v, LoadCtx* ctx, std::string path) : v_(v), ctx_(ctx), path_(std::move(path)) {}

  bool Exists() const { return v_ != nullptr && !v_->IsNull(); }
  const JsonValue& V() const;
  const std::string& Path() const { return path_; }
  LoadCtx& Ctx() const { return *ctx_; }
  void Error(std::string message) const { ctx_->Error(path_, std::move(message)); }

  JNode Child(std::string_view key) const;
  bool Has(std::string_view key) const;  // present and not null

  // ---- self conversions (report an error on a type mismatch) ----
  double AsNum() const;
  int32_t AsInt() const;
  int64_t AsI64() const;
  bool AsBool() const;
  std::string AsStr() const;
  uint32_t AsColor() const;
  template <class E>
  E AsEnum() const {
    E out{};
    if (!v_ || !v_->IsString()) {
      Error("expected an enum string");
      return out;
    }
    if (!ParseEnum(v_->AsString(), out)) Error("unknown value '" + std::string(v_->AsString()) + "'");
    return out;
  }
  Stat AsStat() const { return AsEnum<Stat>(); }
  TilePos AsTile() const;        // {col, row}
  TileCircle AsCircle() const;   // {col, row, radius}

  // ---- members ----
  double Num(std::string_view key) const { return Child(key).AsNum(); }
  double Num(std::string_view key, double def) const;
  int32_t Int(std::string_view key) const { return Child(key).AsInt(); }
  int32_t Int(std::string_view key, int32_t def) const;
  int64_t I64(std::string_view key) const { return Child(key).AsI64(); }
  int64_t I64(std::string_view key, int64_t def) const;
  bool Bool(std::string_view key) const { return Child(key).AsBool(); }
  bool Bool(std::string_view key, bool def) const;
  std::string Str(std::string_view key) const { return Child(key).AsStr(); }
  std::string Str(std::string_view key, std::string_view def) const;
  uint32_t Color(std::string_view key) const { return Child(key).AsColor(); }
  template <class E>
  E Enum(std::string_view key) const {
    return Child(key).AsEnum<E>();
  }
  template <class E>
  E Enum(std::string_view key, E def) const {
    return Has(key) ? Child(key).AsEnum<E>() : def;
  }
  TilePos Tile(std::string_view key) const { return Child(key).AsTile(); }

  // ---- containers ----
  // Elements of this array (error if not an array, unless allowMissing and it is missing/null).
  // NOTE: the self versions have distinct names so a string literal key never converts to `bool allowMissing`.
  std::vector<JNode> SelfItems(bool allowMissing = false) const;
  std::vector<JNode> Items(std::string_view key, bool allowMissing = false) const { return Child(key).SelfItems(allowMissing); }
  // Members of this object in document order.
  std::vector<std::pair<std::string, JNode>> SelfMembers(bool allowMissing = false) const;
  std::vector<std::pair<std::string, JNode>> Members(std::string_view key, bool allowMissing = false) const {
    return Child(key).SelfMembers(allowMissing);
  }
  std::vector<std::string> StrList(std::string_view key, bool allowMissing = false) const;
  std::vector<int32_t> IntList(std::string_view key, bool allowMissing = false) const;
  std::vector<double> NumList(std::string_view key, bool allowMissing = false) const;
  template <class E>
  std::vector<E> EnumList(std::string_view key, bool allowMissing = false) const {
    std::vector<E> out;
    for (const JNode& n : Items(key, allowMissing)) out.push_back(n.AsEnum<E>());
    return out;
  }
  // {stat: value} object -> StatBag (document order).
  StatBag Stats(std::string_view key, bool allowMissing = false) const;

 private:
  const JsonValue* v_;
  LoadCtx* ctx_;
  std::string path_;
};

// Root node for a file; checks the schemaVersion envelope.
JNode Root(const JsonValue& doc, LoadCtx& ctx);

// ---- per-file loaders (one per table file); each fills its part of the store ----
void LoadClassesFile(const JNode& r, ClassTables& out);
void LoadSkillTreesFile(const JNode& r, ClassTables& out);
void LoadSkillRulesFile(const JNode& r, ClassTables& out);
void LoadHeroFormulasFile(const JNode& r, ClassTables& out);
void LoadBuffCapsFile(const JNode& r, ClassTables& out);
void LoadSpiritProfilesFile(const JNode& r, ClassTables& out);
void LoadStatusEffectsFile(const JNode& r, ClassTables& out);

void LoadCombatInputFile(const JNode& r, CombatTables& out);
void LoadProjectileTimingFile(const JNode& r, CombatTables& out);
void LoadAnimTimingFile(const JNode& r, CombatTables& out);
void LoadHitFeedbackFile(const JNode& r, CombatTables& out);
void LoadEliteAffixesFile(const JNode& r, CombatTables& out);
void LoadDifficultyFile(const JNode& r, CombatTables& out);
void LoadSoulEchoFile(const JNode& r, CombatTables& out);

void LoadItemBasesFile(const JNode& r, ItemTables& out);
void LoadAffixesFile(const JNode& r, ItemTables& out);
void LoadSetsFile(const JNode& r, ItemTables& out);
void LoadLegendariesFile(const JNode& r, ItemTables& out);
void LoadEconomyFile(const JNode& r, ItemTables& out);
void LoadLootRulesFile(const JNode& r, ItemTables& out);
void LoadCraftingFile(const JNode& r, ItemTables& out);
void LoadShopsFile(const JNode& r, ItemTables& out);

void LoadMonstersFile(const JNode& r, MonsterTables& out);
void LoadMiniBossesFile(const JNode& r, MonsterTables& out);
void LoadQuestHuntsFile(const JNode& r, MonsterTables& out);
void LoadMonsterAiFile(const JNode& r, MonsterTables& out);
// Shared by monsters.json and quest_hunts.json.
MonsterDef ReadMonsterDef(const JNode& n, bool withDerived);
DialogueTree ReadDialogueTree(const JNode& n, std::string id, DialogueKind kind);
StatusRule ReadStatusRule(const JNode& n);

void LoadMapsFile(const JNode& r, WorldTables& out);
void LoadMapGenFile(const JNode& r, WorldTables& out);
void LoadWorldConstantsFile(const JNode& r, WorldTables& out);
void LoadRandomEventsFile(const JNode& r, WorldTables& out);
void LoadZoneMoodsFile(const JNode& r, WorldTables& out);

void LoadQuestsFile(const JNode& r, QuestTables& out);
void LoadQuestTuningFile(const JNode& r, QuestTables& out);
void LoadAchievementsFile(const JNode& r, QuestTables& out);
void LoadNpcsFile(const JNode& r, NpcTables& out);
void LoadDialogueTreesFile(const JNode& r, DialogueTables& out);
void LoadStoryFile(const JNode& r, StoryScript& out);
void LoadLoreFile(const JNode& r, LoreTables& out);

void LoadPetsFile(const JNode& r, PetTables& out);
void LoadHomesteadFile(const JNode& r, HomesteadTables& out);

void LoadAudioCuesFile(const JNode& r, AudioTables& out);
void LoadMusicFile(const JNode& r, AudioTables& out);
void LoadAssetsFile(const JNode& r, AssetManifest& out);
void LoadUiThemeFile(const JNode& r, UiThemeDef& out);
void LoadRenderQualityFile(const JNode& r, RenderQualityTables& out);
void LoadAbyssRunFile(const JNode& r, AbyssRunTables& out);

}  // namespace abyss::dataload

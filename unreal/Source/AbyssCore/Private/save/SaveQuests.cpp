// SaveData sections owned by quests+story: quests, dialogueState, achievements (save-ui-input.md 3.2; quests-story-ch1
// 1.5, 2.9, 9, 11). STUB.
#include "abyss/base/Platform.h"

#include "abyss/base/Assert.h"
#include "abyss/save/SaveIO.h"
#include "SaveSections.h"

namespace abyss {

void WriteQuestProgressJson(JsonWriter& w, const QuestProgress& p) {
  ABYSS_UNIMPLEMENTED();
  w.StartObject();
  w.EndObject();
}

bool ReadQuestProgressJson(const JsonValue& v, QuestProgress& out) {
  ABYSS_UNIMPLEMENTED();
  return false;
}

namespace savejson {

void WriteQuests(JsonWriter& w, const std::vector<QuestProgress>& quests) {
  w.StartArray();
  for (const QuestProgress& p : quests) WriteQuestProgressJson(w, p);
  w.EndArray();
}

void ReadQuests(const JsonValue& v, std::vector<QuestProgress>& out) {
  out.clear();
  if (v.Type() != JsonType::Array) return;
  for (const JsonValue& e : v.Items()) {
    QuestProgress p;
    if (ReadQuestProgressJson(e, p)) out.push_back(std::move(p));
  }
}

void WriteDialogueState(JsonWriter& w, const std::vector<DialogueNpcState>& states) {
  ABYSS_UNIMPLEMENTED();
  w.StartObject();
  w.EndObject();
}

void ReadDialogueState(const JsonValue& v, std::vector<DialogueNpcState>& out) { ABYSS_UNIMPLEMENTED(); }

void WriteAchievements(JsonWriter& w, const std::vector<std::pair<std::string, int64_t>>& entries) {
  w.StartObject();
  for (const auto& [k, n] : entries) {
    w.Key(k);
    w.Int(n);
  }
  w.EndObject();
}

void ReadAchievements(const JsonValue& v, std::vector<std::pair<std::string, int64_t>>& out) {
  ABYSS_UNIMPLEMENTED();
}

}  // namespace savejson
}  // namespace abyss

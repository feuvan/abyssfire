// SaveData sections owned by quests+story: quests, dialogueState, achievements (save-ui-input.md 3.2-3.3;
// quests-story-ch1.md 1.5, 2.9, 7.1, 9, 11). Readers are lenient: a malformed entry is skipped, a wrong-typed field
// keeps its default, nothing fails the whole load. Data-dependent normalisations (objective counts vs. the current
// definition, achievement ids vs. progress keys) run in QuestSystem::Load / AchievementState::Load.
#include "abyss/base/Platform.h"

#include <algorithm>
#include <cmath>

#include "abyss/base/Math.h"
#include "abyss/save/SaveIO.h"
#include "SaveSections.h"

namespace abyss {

namespace {

// A finite JSON number as an integer count (integral numbers verbatim, others floored; clamped to +-2^53).
bool SqReadCount(const JsonValue& v, int64_t& out) {
  if (v.Type() != JsonType::Number) return false;
  if (v.IsInteger()) {
    out = v.AsInt64();
    return true;
  }
  const double d = v.AsDouble();
  if (!std::isfinite(d)) return false;
  constexpr double kLim = 9007199254740992.0;
  out = static_cast<int64_t>(std::floor(std::clamp(d, -kLim, kLim)));
  return true;
}

}  // namespace

void WriteQuestProgressJson(JsonWriter& w, const QuestProgress& p) {
  w.StartObject();
  w.Key("questId");
  w.String(p.questId);
  w.Key("status");
  w.String(EnumName(p.status));
  w.Key("objectives");
  w.StartArray();
  for (int32_t current : p.objectives) {
    w.StartObject();
    w.Key("current");
    w.Int(current);
    w.EndObject();
  }
  w.EndArray();
  w.EndObject();
}

// {questId, status, objectives: [{current}]}. False (entry dropped) when it is not an object, has no string questId or
// an unknown status ('available' is never stored by the web). A non-array objectives reads as [] (QuestSystem::Load
// then restarts an active / completed quest); an entry without a numeric current reads 0.
bool ReadQuestProgressJson(const JsonValue& v, QuestProgress& out) {
  out = QuestProgress{};
  if (v.Type() != JsonType::Object) return false;
  const JsonValue* id = v.Find("questId");
  if (id == nullptr || id->Type() != JsonType::String || id->AsString().empty()) return false;
  const JsonValue* status = v.Find("status");
  if (status == nullptr || status->Type() != JsonType::String || !ParseEnum(status->AsString(), out.status)) {
    return false;
  }
  out.questId = std::string(id->AsString());
  const JsonValue* objs = v.Find("objectives");
  if (objs != nullptr && objs->Type() == JsonType::Array) {
    for (const JsonValue& o : objs->Items()) {
      int64_t current = 0;
      if (o.Type() == JsonType::Object) {
        if (const JsonValue* c = o.Find("current")) {
          if (!SqReadCount(*c, current)) current = 0;
        }
      }
      out.objectives.push_back(SaturatingInt32(static_cast<double>(current)));
    }
  }
  return true;
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

// dialogueState: {npcId: {visitedNodes: [nodeId], choicesMade: {nodeId: nextNodeId}}} (record only, 7.1).
void WriteDialogueState(JsonWriter& w, const std::vector<DialogueNpcState>& states) {
  w.StartObject();
  for (const DialogueNpcState& s : states) {
    w.Key(s.npcId);
    w.StartObject();
    w.Key("visitedNodes");
    WriteStringList(w, s.visitedNodes);
    w.Key("choicesMade");
    w.StartObject();
    for (const auto& [node, next] : s.choicesMade) {
      w.Key(node);
      w.String(next);
    }
    w.EndObject();
    w.EndObject();
  }
  w.EndObject();
}

void ReadDialogueState(const JsonValue& v, std::vector<DialogueNpcState>& out) {
  out.clear();
  if (v.Type() != JsonType::Object) return;
  for (const JsonMember& m : v.Members()) {
    if (m.value.Type() != JsonType::Object) continue;
    DialogueNpcState s;
    s.npcId = m.key;
    if (const JsonValue* visited = m.value.Find("visitedNodes")) ReadStringList(*visited, s.visitedNodes);
    if (const JsonValue* made = m.value.Find("choicesMade")) {
      if (made->Type() == JsonType::Object) {
        for (const JsonMember& c : made->Members()) {
          if (c.value.Type() == JsonType::String) s.choicesMade.emplace_back(c.key, std::string(c.value.AsString()));
        }
      }
    }
    out.push_back(std::move(s));
  }
}

void WriteAchievements(JsonWriter& w, const std::vector<std::pair<std::string, int64_t>>& entries) {
  w.StartObject();
  for (const auto& [k, n] : entries) {
    w.Key(k);
    w.Int(n);
  }
  w.EndObject();
}

// {key: number} in document order (progress counters and {achId: 1}); non-numeric values are skipped.
void ReadAchievements(const JsonValue& v, std::vector<std::pair<std::string, int64_t>>& out) {
  out.clear();
  if (v.Type() != JsonType::Object) return;
  for (const JsonMember& m : v.Members()) {
    int64_t n = 0;
    if (SqReadCount(m.value, n)) out.emplace_back(m.key, n);
  }
}

}  // namespace savejson
}  // namespace abyss

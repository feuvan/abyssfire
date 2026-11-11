// Test-only helpers. Tests (not the core) may read files with std::ifstream; the core only ever receives bytes.
#pragma once

#include <fstream>
#include <sstream>
#include <string>

#include "abyss/base/Assert.h"
#include "abyss/base/Log.h"

namespace abyss::test {

inline std::string UnrealDir() { return ABYSS_UNREAL_DIR; }
inline std::string DataDir() { return UnrealDir() + "/Data"; }
inline std::string GoldenDir() { return UnrealDir() + "/CoreTests/golden"; }

// Returns the file's bytes, or an empty string when it cannot be read.
inline std::string ReadFile(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return {};
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

// Installs a non-aborting assert handler for the lifetime of the object and counts failures.
class ScopedAssertCounter {
 public:
  ScopedAssertCounter() : previous_(SetAssertHandler(&Handler, this)) {}
  ~ScopedAssertCounter() { SetAssertHandler(previous_.handler, previous_.user); }
  ScopedAssertCounter(const ScopedAssertCounter&) = delete;
  ScopedAssertCounter& operator=(const ScopedAssertCounter&) = delete;
  int Count() const { return count_; }
  const std::string& LastMessage() const { return last_; }

 private:
  static void Handler(const char*, int, const char* expr, const char* msg, void* user) {
    auto* self = static_cast<ScopedAssertCounter*>(user);
    ++self->count_;
    self->last_ = std::string(expr ? expr : "") + " | " + (msg ? msg : "");
  }
  AssertHandlerBinding previous_;
  int count_ = 0;
  std::string last_;
};

}  // namespace abyss::test

#include "abyss/base/Json.h"
#include "abyss/data/DataStore.h"

namespace abyss::test {

// The shipped autopilot_routes.json without its `dungeon` steps (K7; dark remake 3B: the Chapter 1 route goes down the
// Pit). A fixture that rewrites a zone's sub-dungeon entrances, or re-points one at a sub-dungeon of its own
// (content_hooks_test, subdungeon_test), leaves that step going down an entrance that is gone or leads elsewhere. Those
// fixtures do not test the route: they load this text in the table's place.
inline std::string RoutesWithoutDungeonSteps() {
  JsonValue doc;
  if (!ParseJson(ReadFile(DataDir() + "/autopilot_routes.json"), doc)) return {};
  JsonValue* routes = doc.FindMutable("routes");
  if (routes == nullptr) return {};
  for (JsonValue& route : routes->MutableItems()) {
    JsonValue kept = JsonValue::Array();
    if (JsonValue* steps = route.FindMutable("steps")) {
      for (JsonValue& step : steps->MutableItems()) {
        if (step.Get("kind").AsString() != "dungeon") kept.Append(std::move(step));
      }
    }
    route.Set("steps", std::move(kept));
  }
  return WriteJson(doc);
}

// Loads the real exported tables from unreal/Data once per test process. Fails the calling test when loading fails.
inline const DataStore& RealData() {
  static DataStore store;
  static bool loaded = false;
  static std::string summary;
  if (!loaded) {
    DataLoadReport report;
    const bool ok = store.LoadAll(
        [](std::string_view name, std::string& out) {
          out = ReadFile(DataDir() + "/" + std::string(name));
          return !out.empty();
        },
        report);
    summary = report.Summary();
    loaded = true;
    if (!ok) Log(LogLevel::Error, summary);
  }
  return store;
}

}  // namespace abyss::test

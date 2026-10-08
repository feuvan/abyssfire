#include "abyss/base/Platform.h"

#include "abyss/base/Assert.h"

#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include "abyss/base/Log.h"

namespace abyss {
namespace {

struct AssertState {
  AssertHandler handler = nullptr;
  void* user = nullptr;
  int failures = 0;
  std::vector<std::pair<std::string, int>> reportedUnimplemented;
};

AssertState& State() {
  static AssertState s;
  return s;
}

void DefaultHandler(const char* file, int line, const char* expr, const char* msg, void* /*user*/) {
  std::string m = "ABYSS_ASSERT failed: ";
  m += expr ? expr : "";
  m += " (";
  m += msg ? msg : "";
  m += ") at ";
  m += file ? file : "?";
  m += ":";
  m += std::to_string(line);
  Log(LogLevel::Fatal, m);
  std::abort();
}

}  // namespace

AssertHandlerBinding SetAssertHandler(AssertHandler handler, void* user) {
  AssertState& s = State();
  const AssertHandlerBinding previous{s.handler, s.user};
  s.handler = handler;
  s.user = user;
  return previous;
}

AssertHandlerBinding CurrentAssertHandler() { return AssertHandlerBinding{State().handler, State().user}; }

void ReportAssertFailure(const char* file, int line, const char* expr, const char* msg) {
  AssertState& s = State();
  ++s.failures;
  if (s.handler) {
    s.handler(file, line, expr, msg, s.user);
  } else {
    DefaultHandler(file, line, expr, msg, nullptr);
  }
}

void ReportUnimplemented(const char* file, int line, const char* function) {
  AssertState& s = State();
  const std::string f = file ? file : "?";
  for (const auto& r : s.reportedUnimplemented) {
    if (r.second == line && r.first == f) return;
  }
  s.reportedUnimplemented.emplace_back(f, line);
  std::string m = "unimplemented: ";
  m += function ? function : "?";
  m += " (";
  m += f;
  m += ":";
  m += std::to_string(line);
  m += ")";
  Log(LogLevel::Warning, m);
}

int AssertFailureCount() { return State().failures; }

}  // namespace abyss

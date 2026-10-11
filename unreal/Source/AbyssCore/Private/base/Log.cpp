#include "abyss/base/Platform.h"

#include "abyss/base/Log.h"

namespace abyss {
namespace {

struct LogState {
  LogSink sink = nullptr;
  void* user = nullptr;
  LogLevel minLevel = LogLevel::Info;
};

LogState& LogStateRef() {
  static LogState s;
  return s;
}

}  // namespace

std::string_view LogLevelName(LogLevel level) {
  switch (level) {
    case LogLevel::Trace: return "Trace";
    case LogLevel::Debug: return "Debug";
    case LogLevel::Info: return "Info";
    case LogLevel::Warning: return "Warning";
    case LogLevel::Error: return "Error";
    case LogLevel::Fatal: return "Fatal";
  }
  return "?";
}

LogSinkBinding SetLogSink(LogSink sink, void* user) {
  LogState& s = LogStateRef();
  const LogSinkBinding previous{s.sink, s.user};
  s.sink = sink;
  s.user = user;
  return previous;
}

LogSinkBinding CurrentLogSink() { return LogSinkBinding{LogStateRef().sink, LogStateRef().user}; }

void SetMinLogLevel(LogLevel level) { LogStateRef().minLevel = level; }

LogLevel MinLogLevel() { return LogStateRef().minLevel; }

void Log(LogLevel level, std::string_view message) {
  const LogState& s = LogStateRef();
  if (static_cast<int>(level) < static_cast<int>(s.minLevel)) return;
  if (s.sink) s.sink(level, message, s.user);
}

}  // namespace abyss

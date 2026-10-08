// Logging through a callback (the UE module routes it to UE_LOG). No iostreams in the core.
//
// This is the developer log. Player-facing combat-log lines are core Events carrying i18n keys (sim/Events.h).
//
// Lifetime (UE): the sink and its `user` pointer are process-global. The UE owner (GameInstance / subsystem) must
// restore the previous sink in Shutdown / Deinitialize BEFORE destroying the GameSim and the DataStore (they log while
// tearing down), so that a later PIE session or module shutdown never calls into a destroyed object. Prefer
// ScopedLogSink as a member of the owner: its destructor restores whatever was installed before it. SetLogSink returns
// the previous binding so installs nest.
#pragma once

#include <string>
#include <string_view>

#include "abyss/base/Platform.h"

namespace abyss {

enum class LogLevel : uint8_t { Trace, Debug, Info, Warning, Error, Fatal };

ABYSS_API std::string_view LogLevelName(LogLevel level);

using LogSink = void (*)(LogLevel level, std::string_view message, void* user);

struct LogSinkBinding {
  LogSink sink = nullptr;
  void* user = nullptr;
};

// Installs the sink (nullptr = drop messages) and returns the previous binding. Default: no sink (messages dropped).
ABYSS_API LogSinkBinding SetLogSink(LogSink sink, void* user);
ABYSS_API LogSinkBinding CurrentLogSink();
// Messages below this level are dropped before reaching the sink. Default: Info.
ABYSS_API void SetMinLogLevel(LogLevel level);
ABYSS_API LogLevel MinLogLevel();

ABYSS_API void Log(LogLevel level, std::string_view message);

inline void LogInfo(std::string_view m) { Log(LogLevel::Info, m); }
inline void LogWarning(std::string_view m) { Log(LogLevel::Warning, m); }
inline void LogError(std::string_view m) { Log(LogLevel::Error, m); }

// Installs a sink for its own lifetime and restores the previous binding on destruction (RAII; nests).
class ScopedLogSink {
 public:
  ScopedLogSink(LogSink sink, void* user) : previous_(SetLogSink(sink, user)) {}
  ~ScopedLogSink() { SetLogSink(previous_.sink, previous_.user); }
  ScopedLogSink(const ScopedLogSink&) = delete;
  ScopedLogSink& operator=(const ScopedLogSink&) = delete;

 private:
  LogSinkBinding previous_;
};

}  // namespace abyss

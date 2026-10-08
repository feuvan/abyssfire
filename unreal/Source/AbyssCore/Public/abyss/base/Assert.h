// Assertions with a user-installable handler (ue58-platform.md 3.3 "Assertions / logging").
//
// ABYSS_ASSERT(cond, msg): on failure calls the installed handler. The default handler logs at Fatal level through
// Log.h and then calls std::abort(). The UE module installs a handler that routes to ensureMsgf / UE_LOG and returns,
// so code after a failed assert must stay memory-safe: every ABYSS_ASSERT has an explicit safe fallback right after it
// (treat the assert as "this should never happen" and fall back). Third-party code that cannot continue after a failed
// check (RapidJSON) aborts after reporting.
//
// ABYSS_UNIMPLEMENTED(): marks a stub body. It logs one Warning per call site and returns (never aborts), so the
// skeleton GameSim can run end to end while subsystems are being filled in.
//
// Lifetime (UE): like the log sink, the handler + `user` are process-global. The UE owner must restore the previous
// handler in Shutdown / Deinitialize (ScopedAssertHandler as a member does it) before its object is destroyed.
#pragma once

#include "abyss/base/Platform.h"

namespace abyss {

using AssertHandler = void (*)(const char* file, int line, const char* expr, const char* msg, void* user);

struct AssertHandlerBinding {
  AssertHandler handler = nullptr;
  void* user = nullptr;
};

// Installs the handler (nullptr restores the default log-and-abort handler) and returns the previous binding.
// Not thread-safe (the core is single-threaded).
ABYSS_API AssertHandlerBinding SetAssertHandler(AssertHandler handler, void* user);
ABYSS_API AssertHandlerBinding CurrentAssertHandler();

// Called by ABYSS_ASSERT. Returns if the installed handler returns.
ABYSS_API void ReportAssertFailure(const char* file, int line, const char* expr, const char* msg);

// Called by ABYSS_UNIMPLEMENTED. Logs once per (file, line).
ABYSS_API void ReportUnimplemented(const char* file, int line, const char* function);

// Number of assert failures reported since start (tests use it with a non-aborting handler).
ABYSS_API int AssertFailureCount();

// Installs a handler for its own lifetime and restores the previous binding on destruction (RAII; nests).
class ScopedAssertHandler {
 public:
  ScopedAssertHandler(AssertHandler handler, void* user) : previous_(SetAssertHandler(handler, user)) {}
  ~ScopedAssertHandler() { SetAssertHandler(previous_.handler, previous_.user); }
  ScopedAssertHandler(const ScopedAssertHandler&) = delete;
  ScopedAssertHandler& operator=(const ScopedAssertHandler&) = delete;

 private:
  AssertHandlerBinding previous_;
};

}  // namespace abyss

#define ABYSS_ASSERT(cond, msg)                                                  \
  do {                                                                           \
    if (!(cond)) ::abyss::ReportAssertFailure(__FILE__, __LINE__, #cond, (msg)); \
  } while (0)

#define ABYSS_UNIMPLEMENTED() ::abyss::ReportUnimplemented(__FILE__, __LINE__, __func__)

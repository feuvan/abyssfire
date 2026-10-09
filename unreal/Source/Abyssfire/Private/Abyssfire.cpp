// Abyssfire primary game module: log categories and the core log / assert bridge. (Project console variables live in
// Platform/AbyssPlatform.cpp.)
#include "Abyssfire.h"

#include "Misc/AssertionMacros.h"
#include "Modules/ModuleManager.h"

#include <string_view>

#include "abyss/base/Assert.h"
#include "abyss/base/Log.h"
#include "abyss/base/Platform.h"

#include "Framework/AbyssText.h"

DEFINE_LOG_CATEGORY(LogAbyss);
DEFINE_LOG_CATEGORY(LogAbyssCore);

namespace
{
	// Stateless sinks (user pointer unused): safe for any number of game instances / PIE sessions. Installed for the
	// module's lifetime and restored in ShutdownModule, before the module (and these functions) can be unloaded.
	void AbyssCoreLogSink(abyss::LogLevel Level, std::string_view Message, void* /*User*/)
	{
		const FString Text = AbyssText::ToFString(Message);
		switch (Level)
		{
		case abyss::LogLevel::Trace:
		case abyss::LogLevel::Debug:
			UE_LOG(LogAbyssCore, Verbose, TEXT("%s"), *Text);
			break;
		case abyss::LogLevel::Info:
			UE_LOG(LogAbyssCore, Log, TEXT("%s"), *Text);
			break;
		case abyss::LogLevel::Warning:
			UE_LOG(LogAbyssCore, Warning, TEXT("%s"), *Text);
			break;
		case abyss::LogLevel::Error:
		case abyss::LogLevel::Fatal:
			// Never Fatal here: the core keeps running after an assert under UE (Assert.h: safe fallbacks).
			UE_LOG(LogAbyssCore, Error, TEXT("%s"), *Text);
			break;
		}
	}

	// ABYSS_ASSERT under UE: report and RETURN (every core assert has a safe fallback right after it).
	void AbyssCoreAssertHandler(const char* File, int Line, const char* Expr, const char* Msg, void* /*User*/)
	{
		UE_LOG(LogAbyssCore, Error, TEXT("ABYSS_ASSERT(%hs) failed: %hs [%hs:%d]"),
			Expr != nullptr ? Expr : "", Msg != nullptr ? Msg : "", File != nullptr ? File : "?", Line);
		// One ensure report per session (ensure fires once per call site) so the first failure gets a callstack.
		ensureMsgf(false, TEXT("ABYSS_ASSERT(%hs) failed: %hs"), Expr != nullptr ? Expr : "", Msg != nullptr ? Msg : "");
	}
}

class FAbyssfireModule final : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		PreviousLogSink = abyss::SetLogSink(&AbyssCoreLogSink, nullptr);
		PreviousAssertHandler = abyss::SetAssertHandler(&AbyssCoreAssertHandler, nullptr);
#if UE_BUILD_SHIPPING
		abyss::SetMinLogLevel(abyss::LogLevel::Warning);
#else
		abyss::SetMinLogLevel(abyss::LogLevel::Info);
#endif
		UE_LOG(LogAbyss, Log, TEXT("Abyssfire module started (AbyssCore API %d)"), abyss::kCoreApiVersion);
	}

	virtual void ShutdownModule() override
	{
		abyss::SetAssertHandler(PreviousAssertHandler.handler, PreviousAssertHandler.user);
		abyss::SetLogSink(PreviousLogSink.sink, PreviousLogSink.user);
	}

private:
	abyss::LogSinkBinding PreviousLogSink;
	abyss::AssertHandlerBinding PreviousAssertHandler;
};

IMPLEMENT_PRIMARY_GAME_MODULE(FAbyssfireModule, Abyssfire, "Abyssfire");

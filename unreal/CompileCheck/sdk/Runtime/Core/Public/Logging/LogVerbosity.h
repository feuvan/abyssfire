// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Logging/LogVerbosity.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"

namespace ELogVerbosity
{
	enum Type : uint8
	{
		NoLogging = 0,
		Fatal,
		Error,
		Warning,
		Display,
		Log,
		Verbose,
		VeryVerbose,
		All = VeryVerbose,
		NumVerbosity,
		VerbosityMask = 0xf,
		SetColor = 0x40,
		BreakOnLog = 0x80,
	};
}
const TCHAR* ToString(ELogVerbosity::Type Verbosity);
class FString;
ELogVerbosity::Type ParseLogVerbosityFromString(const FString& VerbosityString);

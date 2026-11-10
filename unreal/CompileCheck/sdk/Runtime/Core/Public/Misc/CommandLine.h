// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Misc/CommandLine.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"

struct FCommandLine
{
	static constexpr inline uint32 MaxCommandLineSize = 16384;
	static const TCHAR* Get();
	static const TCHAR* GetForLogging();
	static const TCHAR* GetOriginal();
	static const TCHAR* GetOriginalForLogging();
	static bool IsInitialized();
	static bool Set(const TCHAR* NewCommandLine);
	static void Append(const TCHAR* AppendString);
	static void Parse(const TCHAR* CmdLine, TArray<FString>& Tokens, TArray<FString>& Switches);
	static FString BuildFromArgV(const TCHAR* Prefix, int32 ArgC, TCHAR* ArgV[], const TCHAR* Suffix);
	static FString RemoveExeName(const TCHAR* CmdLine);
};

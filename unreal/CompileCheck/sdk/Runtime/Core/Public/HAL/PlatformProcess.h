// Stub SDK. Real headers: Engine/Source/Runtime/Core/Public/HAL/PlatformProcess.h and
// GenericPlatform/GenericPlatformProcess.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Templates/Function.h"
#include "Containers/UnrealString.h"

class FEvent;
template <typename FuncType> class TFunctionRef;

struct FGenericPlatformProcess
{
	static uint32 GetCurrentProcessId();
	static uint32 GetCurrentCoreNumber();
	static const TCHAR* UserDir();
	static const TCHAR* UserSettingsDir();
	static const TCHAR* UserTempDir();
	static const TCHAR* UserHomeDir();
	static const TCHAR* ApplicationSettingsDir();
	static const TCHAR* ComputerName();
	static const TCHAR* UserName(bool bOnlyAlphaNumeric = true);
	static const TCHAR* ShaderDir();
	static const TCHAR* BaseDir();
	static const TCHAR* ExecutablePath();
	static const TCHAR* ExecutableName(bool bRemoveExtension = true);
	static FString GetApplicationName(uint32 ProcessId);
	static void SetCurrentWorkingDirectoryToBaseDir();
	static FString GetCurrentWorkingDirectory();
	static bool CanLaunchURL(const TCHAR* URL);
	static void LaunchURL(const TCHAR* URL, const TCHAR* Parms, FString* Error);
	static bool LaunchFileInDefaultExternalApplication(const TCHAR* FileName, const TCHAR* Parms = nullptr, int32 Verb = 0, bool bPromptToOpenOnFailure = true);
	static void ExploreFolder(const TCHAR* FilePath);
	static void Sleep(float Seconds);
	static void SleepNoStats(float Seconds);
	[[noreturn]] static void SleepInfinite();
	static void YieldThread();
	static void ConditionalSleep(TFunctionRef<bool()> Condition, float SleepTime = 0.0f);
	static FEvent* GetSynchEventFromPool(bool bIsManualReset = false);
	static void ReturnSynchEventToPool(FEvent* Event);
	static bool SupportsMultithreading();
	static bool IsApplicationForeground();
	static bool IsFirstInstance();
	static void SetThreadName(const TCHAR* ThreadName);
};
struct FPlatformProcess : public FGenericPlatformProcess
{
};

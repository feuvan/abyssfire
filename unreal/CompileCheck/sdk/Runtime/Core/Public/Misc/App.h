// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Misc/App.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "Misc/Guid.h"
#include "Misc/Timespan.h"
#include "Misc/Optional.h"

class FApp
{
public:
	static FString GetBranchName();
	static const TCHAR* GetBuildVersion();
	static FString GetBuildDate();
	static FString GetGraphicsRHI();
	static FString GetEpicProductIdentifier();
	static FGuid GetInstanceId();
	static FString GetInstanceName();
	static FString GetName();
	static const TCHAR* GetProjectName();
	static void SetProjectName(const TCHAR* InProjectName);
	static bool HasProjectName();
	static FGuid GetSessionId();
	static FString GetSessionName();
	static bool IsGame();
	static bool IsEngineInstalled();
	static bool IsInstalled();
	static bool IsStandalone();
	static bool IsUnattended();
	static bool IsBenchmarking();
	static void SetBenchmarking(bool bVal);
	static bool CanEverRender();
	static bool CanEverRenderAudio();
	static bool HasFocus();
	static bool ShouldUseThreadingForPerformance();
	static bool UseFixedTimeStep();
	static void SetUseFixedTimeStep(bool bVal);
	static double GetFixedDeltaTime();
	static void SetFixedDeltaTime(double Seconds);
	static double GetCurrentTime();
	static void SetCurrentTime(double Seconds);
	static double GetLastTime();
	static void UpdateLastTime();
	static double GetDeltaTime();
	static void SetDeltaTime(double Seconds);
	static double GetIdleTime();
	static double GetGameTime();
	static void SetGameTime(double Seconds);
	static float GetVolumeMultiplier();
	static void SetVolumeMultiplier(float InVolumeMultiplier);
	static float GetUnfocusedVolumeMultiplier();
	static void SetUnfocusedVolumeMultiplier(float InVolumeMultiplier);
	static bool UseVRFocus();
	static bool HasVRFocus();
	static void PrintStartupLogMessages();
};

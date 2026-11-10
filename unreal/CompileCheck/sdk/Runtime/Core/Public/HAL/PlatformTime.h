// Stub SDK. Real headers: Engine/Source/Runtime/Core/Public/HAL/PlatformTime.h and
// GenericPlatform/GenericPlatformTime.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"

struct FGenericPlatformTime
{
	static double InitTiming();
	static double GetSecondsPerCycle();
	static double GetSecondsPerCycle64();
	static float ToMilliseconds(const uint32 Cycles);
	static float ToSeconds(const uint32 Cycles);
	static double ToMilliseconds64(const uint64 Cycles);
	static double ToSeconds64(const uint64 Cycles);
	static void SystemTime(int32& Year, int32& Month, int32& DayOfWeek, int32& Day, int32& Hour, int32& Min, int32& Sec, int32& MSec);
	static void UtcTime(int32& Year, int32& Month, int32& DayOfWeek, int32& Day, int32& Hour, int32& Min, int32& Sec, int32& MSec);
	static const TCHAR* StrDate(TCHAR* Dest, SIZE_T DestSize);
	static const TCHAR* StrTime(TCHAR* Dest, SIZE_T DestSize);
	static const TCHAR* StrTimestamp();
	static FString PrettyTime(double Seconds);
};
struct FPlatformTime : public FGenericPlatformTime
{
	static double Seconds();
	static uint32 Cycles();
	static uint64 Cycles64();
};

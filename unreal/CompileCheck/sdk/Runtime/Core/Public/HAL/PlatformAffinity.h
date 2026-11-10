// Stub SDK. Real headers: Engine/Source/Runtime/Core/Public/HAL/PlatformAffinity.h and
// GenericPlatform/GenericPlatformAffinity.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"

enum EThreadPriority
{
	TPri_Normal,
	TPri_AboveNormal,
	TPri_BelowNormal,
	TPri_Highest,
	TPri_Lowest,
	TPri_SlightlyBelowNormal,
	TPri_TimeCritical,
	TPri_Num,
};
enum class EThreadCreateFlags : int8 { None = 0, SMTExclusive = (1 << 0) };
class FGenericPlatformAffinity
{
public:
	static uint64 GetMainGameMask();
	static uint64 GetRenderingThreadMask();
	static uint64 GetNoAffinityMask();
	static uint64 GetPoolThreadMask();
	static uint64 GetTaskGraphThreadMask();
	static uint64 GetAudioRenderThreadMask();
};
using FPlatformAffinity = FGenericPlatformAffinity;

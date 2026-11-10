// Stub SDK. Real headers: Engine/Source/Runtime/Core/Public/HAL/PlatformMemory.h and
// GenericPlatform/GenericPlatformMemory.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"

struct FGenericPlatformMemoryConstants
{
	uint64 TotalPhysical;
	uint64 TotalVirtual;
	SIZE_T PageSize;
	SIZE_T OsAllocationGranularity;
	SIZE_T BinnedPageSize;
	SIZE_T BinnedAllocationGranularity;
	uint64 AddressLimit;
	uint32 TotalPhysicalGB;
};
using FPlatformMemoryConstants = FGenericPlatformMemoryConstants;
struct FGenericPlatformMemoryStats : public FPlatformMemoryConstants
{
	enum class EMemoryPressureStatus : uint8 { Unknown, Nominal, Warning, Critical };
	uint64 AvailablePhysical;
	uint64 AvailableVirtual;
	uint64 UsedPhysical;
	uint64 PeakUsedPhysical;
	uint64 UsedVirtual;
	uint64 PeakUsedVirtual;
	EMemoryPressureStatus GetMemoryPressureStatus() const;
};
using FPlatformMemoryStats = FGenericPlatformMemoryStats;

struct FGenericPlatformMemory
{
	static void Init();
	static FPlatformMemoryStats GetStats();
	static uint64 GetMemoryUsedFast();
	static const FPlatformMemoryConstants& GetConstants();
	static uint32 GetPhysicalGBRam();
	static void* Memmove(void* Dest, const void* Src, SIZE_T Count);
	static int32 Memcmp(const void* Buf1, const void* Buf2, SIZE_T Count);
	static void* Memset(void* Dest, uint8 Char, SIZE_T Count);
	static void* Memzero(void* Dest, SIZE_T Count);
	static void* Memcpy(void* Dest, const void* Src, SIZE_T Count);
	static bool IsExtraDevelopmentMemoryAvailable();
};
struct FPlatformMemory : public FGenericPlatformMemory
{
};

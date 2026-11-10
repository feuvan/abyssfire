// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/HAL/UnrealMemory.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include "HAL/PlatformMemory.h"

enum { DEFAULT_ALIGNMENT = 0 };

struct FMemory
{
	static void* Memmove(void* Dest, const void* Src, SIZE_T Count);
	static int32 Memcmp(const void* Buf1, const void* Buf2, SIZE_T Count);
	static void* Memset(void* Dest, uint8 Char, SIZE_T Count);
	template <class T> static void Memset(T& Src, uint8 ValueToSet) { static_assert(!std::is_pointer_v<T>, "For pointers use the three parameters function"); }
	static void* Memzero(void* Dest, SIZE_T Count);
	template <class T> static void Memzero(T& Src) { static_assert(!std::is_pointer_v<T>, "For pointers use the two parameters function"); }
	static void* Memcpy(void* Dest, const void* Src, SIZE_T Count);
	template <class T> static void Memcpy(T& Dest, const T& Src) { static_assert(!std::is_pointer_v<T>, "For pointers use the three parameters function"); }
	static void Memswap(void* Ptr1, void* Ptr2, SIZE_T Size);
	static void* SystemMalloc(SIZE_T Size);
	static void SystemFree(void* Ptr);
	static void* Malloc(SIZE_T Count, uint32 Alignment = DEFAULT_ALIGNMENT);
	static void* Realloc(void* Original, SIZE_T Count, uint32 Alignment = DEFAULT_ALIGNMENT);
	static void Free(void* Original);
	static SIZE_T GetAllocSize(void* Original);
	static void* MallocZeroed(SIZE_T Count, uint32 Alignment = DEFAULT_ALIGNMENT);
	static SIZE_T QuantizeSize(SIZE_T Count, uint32 Alignment = DEFAULT_ALIGNMENT);
	static void Trim(bool bTrimThreadCaches = true);
};

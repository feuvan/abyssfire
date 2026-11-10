// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Templates/TypeHash.h (from UE 5.x source knowledge).
// GetTypeHash overloads for the built-in types. A key type of TMap / TSet needs a visible GetTypeHash (found by ADL or
// declared before use) and operator==: the SDK containers check both (UE::Stub::CHashableKey).
#pragma once

#include "CoreTypes.h"
#include "Templates/UnrealTypeTraits.h"

uint32 HashCombine(uint32 A, uint32 C);
uint32 HashCombineFast(uint32 A, uint32 B);
uint32 PointerHash(const void* Key);
uint32 PointerHash(const void* Key, uint32 C);
// Two hashes at a time: a variadic HashCombineFast is not declared (its existence and parameter list in 5.x could not
// be confirmed, API_UNCERTAIN.md); nest the calls.

uint32 GetTypeHash(const uint8 A);
uint32 GetTypeHash(const int8 A);
uint32 GetTypeHash(const uint16 A);
uint32 GetTypeHash(const int16 A);
uint32 GetTypeHash(const int32 A);
uint32 GetTypeHash(const uint32 A);
uint32 GetTypeHash(const uint64 A);
uint32 GetTypeHash(const int64 A);
uint32 GetTypeHash(float Value);
uint32 GetTypeHash(double Value);
uint32 GetTypeHash(const TCHAR* S);
uint32 GetTypeHash(const void* A);
uint32 GetTypeHash(void* A);
template <typename EnumType> requires std::is_enum_v<EnumType>
uint32 GetTypeHash(EnumType E) UE_STUB_BODY

namespace UE::Stub
{
	/** What TMap / TSet need from a key (DefaultKeyFuncs): GetTypeHash(Key) and Key == Key. */
	template <typename T>
	concept CHashableKey = requires(const T& A, const T& B)
	{
		{ GetTypeHash(A) } -> std::convertible_to<uint32>;
		{ A == B } -> std::convertible_to<bool>;
	};
}

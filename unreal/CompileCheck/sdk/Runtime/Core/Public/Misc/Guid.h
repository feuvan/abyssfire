// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Misc/Guid.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "Containers/StringFwd.h"

enum class EGuidFormats { Digits, DigitsLower, DigitsWithHyphens, DigitsWithHyphensLower, DigitsWithHyphensInBraces, DigitsWithHyphensInParentheses, HexValuesInBraces, UniqueObjectGuid, Short, Base36Encoded };

struct FGuid
{
public:
	constexpr FGuid() : A(0), B(0), C(0), D(0) {}
	explicit constexpr FGuid(uint32 InA, uint32 InB, uint32 InC, uint32 InD) : A(InA), B(InB), C(InC), D(InD) {}
	explicit FGuid(const FString& InGuidStr);
	friend bool operator==(const FGuid& X, const FGuid& Y) { return ((X.A ^ Y.A) | (X.B ^ Y.B) | (X.C ^ Y.C) | (X.D ^ Y.D)) == 0; }
	friend bool operator!=(const FGuid& X, const FGuid& Y) { return !(X == Y); }
	friend bool operator<(const FGuid& X, const FGuid& Y);
	uint32& operator[](int32 Index);
	const uint32& operator[](int32 Index) const;
	void Invalidate();
	bool IsValid() const;
	FString ToString(EGuidFormats Format = EGuidFormats::Digits) const;
	void AppendString(FString& Out, EGuidFormats Format = EGuidFormats::Digits) const;
	friend uint32 GetTypeHash(const FGuid& Guid);
	static FGuid NewGuid();
	static bool Parse(const FString& GuidString, FGuid& OutGuid);
	static bool ParseExact(const FString& GuidString, EGuidFormats Format, FGuid& OutGuid);
	uint32 A;
	uint32 B;
	uint32 C;
	uint32 D;
};
FString LexToString(const FGuid& Value);

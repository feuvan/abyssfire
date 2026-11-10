// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Misc/FrameRate.h (UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"

struct FFrameRate
{
	FFrameRate() : Numerator(60000), Denominator(1) {}
	FFrameRate(uint32 InNumerator, uint32 InDenominator) : Numerator(static_cast<int32>(InNumerator)), Denominator(static_cast<int32>(InDenominator)) {}
	int32 Numerator;
	int32 Denominator;
	bool IsValid() const { return Denominator > 0; }
	double AsInterval() const;
	double AsDecimal() const;
	FFrameRate Reciprocal() const { return FFrameRate(static_cast<uint32>(Denominator), static_cast<uint32>(Numerator)); }
	friend bool operator==(const FFrameRate& A, const FFrameRate& B);
	friend bool operator!=(const FFrameRate& A, const FFrameRate& B);
};

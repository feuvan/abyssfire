// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Types/SlateVector2.h (UE 5.2+; from UE 5.x source
// knowledge). Slate moved from FVector2D (double) to FVector2f; FDeprecateSlateVector2D is the bridge type Slate uses
// for parameters and results: an FVector2f that also converts implicitly from and to FVector2D. So Slate getters can
// initialise an FVector2D, and FVector2D arguments are accepted, but mixing the two in arithmetic needs an explicit
// FVector2D(...) / FVector2f(...) on one side.
#pragma once

#include "CoreMinimal.h"
#include "Math/Vector2D.h"

#define UE_ENABLE_SLATE_VECTOR_DEPRECATION_MECHANISMS 1

namespace UE::Slate
{
	template <typename ComponentType>
	FVector2f CastToVector2f(const UE::Math::TVector2<ComponentType>& InValue) { return FVector2f((float)InValue.X, (float)InValue.Y); }

	struct FDeprecateSlateVector2D : public FVector2f
	{
		FDeprecateSlateVector2D() = default;
		FDeprecateSlateVector2D(float InX, float InY) : FVector2f(InX, InY) {}
		FDeprecateSlateVector2D(const FVector2f& InV) : FVector2f(InV) {}
		FDeprecateSlateVector2D(const FVector2d& InV) : FVector2f((float)InV.X, (float)InV.Y) {}
		FDeprecateSlateVector2D(const FIntPoint& InV) : FVector2f((float)InV.X, (float)InV.Y) {}
		explicit FDeprecateSlateVector2D(float InF) : FVector2f(InF, InF) {}
		operator FVector2d() const { return FVector2d(X, Y); }

		FDeprecateSlateVector2D operator+(const FVector2f& V) const { return FDeprecateSlateVector2D(X + V.X, Y + V.Y); }
		FDeprecateSlateVector2D operator-(const FVector2f& V) const { return FDeprecateSlateVector2D(X - V.X, Y - V.Y); }
		FDeprecateSlateVector2D operator*(const FVector2f& V) const { return FDeprecateSlateVector2D(X * V.X, Y * V.Y); }
		FDeprecateSlateVector2D operator/(const FVector2f& V) const { return FDeprecateSlateVector2D(X / V.X, Y / V.Y); }
		FDeprecateSlateVector2D operator+(const FVector2d& V) const { return FDeprecateSlateVector2D(X + (float)V.X, Y + (float)V.Y); }
		FDeprecateSlateVector2D operator-(const FVector2d& V) const { return FDeprecateSlateVector2D(X - (float)V.X, Y - (float)V.Y); }
		FDeprecateSlateVector2D operator*(const FVector2d& V) const { return FDeprecateSlateVector2D(X * (float)V.X, Y * (float)V.Y); }
		FDeprecateSlateVector2D operator/(const FVector2d& V) const { return FDeprecateSlateVector2D(X / (float)V.X, Y / (float)V.Y); }
		FDeprecateSlateVector2D operator+(const FDeprecateSlateVector2D& V) const { return FDeprecateSlateVector2D(X + V.X, Y + V.Y); }
		FDeprecateSlateVector2D operator-(const FDeprecateSlateVector2D& V) const { return FDeprecateSlateVector2D(X - V.X, Y - V.Y); }
		FDeprecateSlateVector2D operator*(const FDeprecateSlateVector2D& V) const { return FDeprecateSlateVector2D(X * V.X, Y * V.Y); }
		FDeprecateSlateVector2D operator/(const FDeprecateSlateVector2D& V) const { return FDeprecateSlateVector2D(X / V.X, Y / V.Y); }
		template <typename ArgType> requires std::is_arithmetic_v<ArgType>
		FDeprecateSlateVector2D operator*(ArgType Scale) const { return FDeprecateSlateVector2D(X * (float)Scale, Y * (float)Scale); }
		template <typename ArgType> requires std::is_arithmetic_v<ArgType>
		FDeprecateSlateVector2D operator/(ArgType Scale) const { return FDeprecateSlateVector2D(X / (float)Scale, Y / (float)Scale); }
		FDeprecateSlateVector2D operator-() const { return FDeprecateSlateVector2D(-X, -Y); }
		bool operator==(const FVector2f& V) const { return X == V.X && Y == V.Y; }
		bool operator==(const FVector2d& V) const { return X == (float)V.X && Y == (float)V.Y; }
		bool operator==(const FDeprecateSlateVector2D& V) const { return X == V.X && Y == V.Y; }
	};
	template <typename ArgType> requires std::is_arithmetic_v<ArgType>
	FDeprecateSlateVector2D operator*(ArgType Scale, const FDeprecateSlateVector2D& V) { return V * Scale; }

	using FDeprecateVector2DParameter = FDeprecateSlateVector2D;
	using FDeprecateVector2DResult = FDeprecateSlateVector2D;

	struct FDeprecateSlateOptionalVector2D : public TOptional<FVector2f>
	{
		FDeprecateSlateOptionalVector2D() = default;
		FDeprecateSlateOptionalVector2D(const FVector2f& InV);
		FDeprecateSlateOptionalVector2D(const FVector2d& InV);
		FDeprecateSlateOptionalVector2D(const TOptional<FVector2f>& InV);
		FDeprecateSlateOptionalVector2D(const TOptional<FVector2d>& InV);
		operator TOptional<FVector2d>() const;
	};
	using FDeprecateOptionalVector2DParameter = FDeprecateSlateOptionalVector2D;
}

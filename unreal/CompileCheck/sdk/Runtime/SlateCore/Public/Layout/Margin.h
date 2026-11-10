// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Layout/Margin.h (UE 5.x source knowledge; reflected
// struct: checked against the 5.6 stub). FMargin: Left, Top, Right, Bottom (floats).
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Types/SlateEnums.h"
#include "Types/SlateVector2.h"

struct FMargin
{
	UE_STUB_STRUCT_BODY(FMargin)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	float Left;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	float Top;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	float Right;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	float Bottom;
	constexpr FMargin() : Left(0.0f), Top(0.0f), Right(0.0f), Bottom(0.0f) {}
	constexpr FMargin(float UniformMargin) : Left(UniformMargin), Top(UniformMargin), Right(UniformMargin), Bottom(UniformMargin) {}
	constexpr FMargin(float Horizontal, float Vertical) : Left(Horizontal), Top(Vertical), Right(Horizontal), Bottom(Vertical) {}
	FMargin(const FVector2f& InVector) : Left(InVector.X), Top(InVector.Y), Right(InVector.X), Bottom(InVector.Y) {}
	FMargin(const FVector2D& InVector) : Left((float)InVector.X), Top((float)InVector.Y), Right((float)InVector.X), Bottom((float)InVector.Y) {}
	constexpr FMargin(float InLeft, float InTop, float InRight, float InBottom) : Left(InLeft), Top(InTop), Right(InRight), Bottom(InBottom) {}
	FMargin(const FVector4& InVector);
	FMargin operator*(float Scale) const { return FMargin(Left * Scale, Top * Scale, Right * Scale, Bottom * Scale); }
	FMargin operator*(const FMargin& InScale) const;
	FMargin operator+(const FMargin& InDelta) const;
	FMargin operator-(const FMargin& Other) const;
	bool operator==(const FMargin& Other) const;
	bool operator!=(const FMargin& Other) const;
	UE::Slate::FDeprecateVector2DResult GetTopLeft() const;
	UE::Slate::FDeprecateVector2DResult GetDesiredSize() const;
	template <EOrientation Orientation> float GetTotalSpaceAlong() const UE_STUB_BODY
	float GetTotalSpaceAlong(EOrientation Orientation) const;
};

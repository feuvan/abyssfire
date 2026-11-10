// Stub SDK. Real header: Engine/Source/Runtime/Slate/Public/Widgets/Layout/Anchors.h (UE 5.x source knowledge; reflected
// struct: checked against the 5.6 stub). FAnchors: Minimum / Maximum in 0..1 of the parent (doubles: FVector2D).
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

struct FAnchors
{
	UE_STUB_STRUCT_BODY(FAnchors)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FVector2D Minimum;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FVector2D Maximum;
	FAnchors() : Minimum(0.0f, 0.0f), Maximum(0.0f, 0.0f) {}
	FAnchors(float UnifromAnchors) : Minimum(UnifromAnchors, UnifromAnchors), Maximum(UnifromAnchors, UnifromAnchors) {}
	FAnchors(float Horizontal, float Vertical) : Minimum(Horizontal, Vertical), Maximum(Horizontal, Vertical) {}
	FAnchors(float MinX, float MinY, float MaxX, float MaxY) : Minimum(MinX, MinY), Maximum(MaxX, MaxY) {}
	bool IsStretchedVertical() const;
	bool IsStretchedHorizontal() const;
	bool operator==(const FAnchors& Other) const;
	bool operator!=(const FAnchors& Other) const;
};

// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Layout/FlowDirection.h (UE 5.x source knowledge;
// EFlowDirectionPreference is reflected: checked against the 5.6 stub).
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

/** The layout direction of a widget tree. */
enum class EFlowDirection : uint8
{
	/** Desires content flows from left to right. */
	LeftToRight,
	/** Desires content flows from right to left. */
	RightToLeft,
};

UENUM(BlueprintType)
enum class EFlowDirectionPreference : uint8
{
	Inherit,
	Culture,
	LeftToRight,
	RightToLeft,
};

extern EFlowDirection GSlateFlowDirection;

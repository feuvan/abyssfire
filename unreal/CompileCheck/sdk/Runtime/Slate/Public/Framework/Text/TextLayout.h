// Stub SDK. Real header: Engine/Source/Runtime/Slate/Public/Framework/Text/TextLayout.h (UE 5.x source knowledge;
// reflected enums: checked against the 5.6 stub). Only the justification / flow enums; FTextLayout is not modelled.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

UENUM(BlueprintType)
namespace ETextJustify
{
	enum Type : int
	{
		/** Justify the text logically to the left (right in right-to-left cultures). */
		Left,
		/** Justify the text in the center. */
		Center,
		/** Justify the text logically to the right (left in right-to-left cultures). */
		Right,
		/** Always justify the text to the left, regardless of the flow direction. */
		InvariantLeft,
		/** Always justify the text to the right, regardless of the flow direction. */
		InvariantRight,
	};
}

UENUM(BlueprintType)
enum class ETextFlowDirection : uint8
{
	Auto = 0,
	LeftToRight,
	RightToLeft,
	Culture,
};

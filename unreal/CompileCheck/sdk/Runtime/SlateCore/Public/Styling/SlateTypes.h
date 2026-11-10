// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Styling/SlateTypes.h (UE 5.x source knowledge;
// reflected enums: checked against the 5.6 stub). The widget style structs (FTextBlockStyle, FButtonStyle, ...) are
// added by the stage that first needs them.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Styling/SlateColor.h"
#include "Styling/SlateBrush.h"
#include "Fonts/SlateFontInfo.h"
#include "Layout/Margin.h"

UENUM(BlueprintType)
enum class ECheckBoxState : uint8
{
	Unchecked,
	Checked,
	Undetermined,
};

UENUM(BlueprintType)
enum class ETextOverflowPolicy : uint8
{
	/** Overflowing text will be clipped. */
	Clip = 0,
	/** Overflowing text will be replaced with an ellipsis. */
	Ellipsis,
	/** Overflowing text will be replaced with an ellipsis; a partially clipped line is hidden in multi-line text. */
	MultilineEllipsis,
	/** Overflowing text is replaced with an ellipsis in the middle. */
	MiddleEllipsis,
};

UENUM(BlueprintType)
enum class ETextTransformPolicy : uint8
{
	/** No transform, just use the given text as-is. */
	None = 0,
	/** Convert the text to lowercase for display. */
	ToLower,
	/** Convert the text to uppercase for display. */
	ToUpper,
};

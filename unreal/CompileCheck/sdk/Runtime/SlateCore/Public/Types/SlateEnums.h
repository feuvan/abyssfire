// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Types/SlateEnums.h (UE 5.x source knowledge; reflected
// enums: checked against the 5.6 stub).
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

UENUM(BlueprintType)
enum EHorizontalAlignment : int
{
	HAlign_Fill,
	HAlign_Left,
	HAlign_Center,
	HAlign_Right,
};
UENUM(BlueprintType)
enum EVerticalAlignment : int
{
	VAlign_Fill,
	VAlign_Top,
	VAlign_Center,
	VAlign_Bottom,
};
UENUM(BlueprintType)
enum EOrientation : int
{
	Orient_Horizontal,
	Orient_Vertical,
};
UENUM(BlueprintType)
namespace ETextCommit
{
	enum Type : int
	{
		Default,
		OnEnter,
		OnUserMovedFocus,
		OnCleared,
	};
}
UENUM(BlueprintType)
namespace ESelectInfo
{
	enum Type : int
	{
		OnKeyPress,
		OnNavigation,
		OnMouseClick,
		Direct,
	};
}
/** Enumerates different methods that a button click can be triggered. Normally, DownAndUp is appropriate. */
UENUM(BlueprintType)
namespace EButtonClickMethod
{
	enum Type : int
	{
		/** User must press the button, then release while over the button to trigger the click. */
		DownAndUp,
		/** Click will be triggered immediately on mouse down, and mouse will not be captured. */
		MouseDown,
		/** Click will always be triggered when mouse button is released over the button, even if the button wasn't pressed down over it. */
		MouseUp,
		/** Inside a list, buttons can only be clicked with precise tap: moving the pointer cancels the click. */
		PreciseClick,
	};
}
/** Ways in which touch interactions trigger a "Clicked" event. */
UENUM(BlueprintType)
namespace EButtonTouchMethod
{
	enum Type : int
	{
		/** Most buttons behave this way. */
		DownAndUp,
		/** Click will be triggered immediately on touch down, and touch will not be captured. */
		Down,
		/** Inside a list, buttons can only be clicked with precise tap: moving the pointer cancels the click. */
		PreciseTap,
	};
}
/** Enumerates different methods that a button can be triggered with keyboard / controller. */
UENUM(BlueprintType)
namespace EButtonPressMethod
{
	enum Type : int
	{
		/** User must press the button, then release while the button has focus to trigger the click. */
		DownAndUp,
		/** Click will be triggered immediately on button press. */
		ButtonPress,
		/** Click will always be triggered when a button release occurs on the focused button. */
		ButtonRelease,
	};
}
UENUM(BlueprintType)
enum class EUINavigation : uint8
{
	Left,
	Right,
	Up,
	Down,
	Next,
	Previous,
	Num UMETA(Hidden),
	Invalid UMETA(Hidden),
};
UENUM(BlueprintType)
enum class EUINavigationAction : uint8
{
	Accept,
	Back,
	Num UMETA(Hidden),
	Invalid UMETA(Hidden),
};
UENUM(BlueprintType)
enum class ENavigationGenesis : uint8
{
	Keyboard,
	Controller,
	User,
};

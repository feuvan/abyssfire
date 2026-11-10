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

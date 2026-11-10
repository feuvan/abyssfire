// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Input/NavigationReply.h (UE 5.x source knowledge).
#pragma once

#include "CoreMinimal.h"
#include "Types/SlateEnums.h"

class SWidget;
enum class EUINavigationRule : uint8 { Explicit, Wrap, Stop, Escape, Custom, CustomBoundary, Invalid };

class FNavigationReply
{
public:
	static FNavigationReply Explicit(TSharedPtr<SWidget> InFocusRecipient);
	static FNavigationReply Escape();
	static FNavigationReply Stop();
	static FNavigationReply Wrap();
	EUINavigationRule GetBoundaryRule() const;
	TSharedPtr<SWidget> GetFocusRecipient() const;
};

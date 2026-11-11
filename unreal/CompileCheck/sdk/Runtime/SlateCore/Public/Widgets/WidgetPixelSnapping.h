// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Widgets/WidgetPixelSnapping.h (reflected: checked
// against the 5.6 stub). Its own header so that Widgets/DeclarativeSyntaxSupport.h (the PixelSnappingMethod argument
// of every widget) does not need Widgets/SWidget.h, which it only forward-declares, as in the engine.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

/** The different types of pixel snapping a widget can use. */
UENUM()
enum class EWidgetPixelSnapping : uint8
{
	Inherit,
	Disabled,
	SnapToPixel,
};

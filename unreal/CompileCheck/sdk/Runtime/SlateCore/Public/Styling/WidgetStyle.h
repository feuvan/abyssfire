// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Styling/WidgetStyle.h (UE 5.x source knowledge).
#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"

class FWidgetStyle
{
public:
	FWidgetStyle();
	FWidgetStyle& BlendColorAndOpacityTint(const FLinearColor& InTint);
	FWidgetStyle& BlendOpacity(float InOpacity);
	FWidgetStyle& SetForegroundColor(const FLinearColor& InForeground);
	FWidgetStyle& SetForegroundColor(const TAttribute<struct FSlateColor>& InForeground);
	const FLinearColor& GetColorAndOpacityTint() const;
	const FLinearColor& GetForegroundColor() const;
	const FLinearColor& GetSubduedForegroundColor() const;
};

// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Styling/SlateWidgetStyle.h (UE 5.x source knowledge;
// reflected struct: checked against the 5.6 stub). The base of every widget style struct (FButtonStyle, ...).
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

struct FSlateBrush;

struct FSlateWidgetStyle
{
	UE_STUB_STRUCT_BODY(FSlateWidgetStyle)
	FSlateWidgetStyle();
	virtual ~FSlateWidgetStyle() {}
	virtual void GetResources(TArray<const FSlateBrush*>& OutBrushes) const {}
	virtual const FName GetTypeName() const { return FName(NAME_None); }
};

// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Styling/SlateColor.h (UE 5.x source knowledge; reflected
// struct: checked against the 5.6 stub). FSlateColor: implicit from FLinearColor / a shared linked colour
// (no FColor constructor: `FSlateColor C = FColor::Red;` needs two user conversions and does not compile; the engine's
// EStyleColor constructor is not declared until the module uses Styling/StyleColors.h).
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Styling/WidgetStyle.h"

UENUM()
enum class ESlateColorStylingMode : uint8
{
	UseColor_Specified UMETA(DisplayName = "Specified Color"),
	UseColor_ColorTable UMETA(DisplayName = "Color Table"),
	UseColor_Foreground UMETA(DisplayName = "Foreground Color"),
	UseColor_Foreground_Subdued UMETA(DisplayName = "Subdued Foreground Color"),
	UseColor_UseStyle UMETA(DisplayName = "Use Style Color"),
};

struct FSlateColor
{
	UE_STUB_STRUCT_BODY(FSlateColor)
	FSlateColor();
	FSlateColor(const FLinearColor& InColor);
	FSlateColor(const TSharedRef<FLinearColor>& InColor);
	~FSlateColor() {}
	const FLinearColor& GetColor(const FWidgetStyle& InWidgetStyle) const;
	FLinearColor GetSpecifiedColor() const;
	bool IsColorSpecified() const;
	void Unlink();
	bool operator==(const FSlateColor& Other) const;
	bool operator!=(const FSlateColor& Other) const;
	static FSlateColor UseForeground();
	static FSlateColor UseSubduedForeground();
	static FSlateColor UseStyle();
};

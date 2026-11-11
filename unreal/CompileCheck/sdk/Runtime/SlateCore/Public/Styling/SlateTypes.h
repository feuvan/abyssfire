// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Styling/SlateTypes.h (UE 5.x source knowledge;
// reflected enums / structs / properties: checked against the 5.6 stub). Of the widget style structs only those the
// module (or a widget header it includes) names: FTextBlockStyle, FButtonStyle, FScrollBarStyle, FScrollBoxStyle. The
// Set* builders return *this, as in the engine.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Styling/SlateColor.h"
#include "Styling/SlateBrush.h"
#include "Fonts/SlateFontInfo.h"
#include "Layout/Margin.h"
#include "Sound/SlateSound.h"
#include "Styling/SlateWidgetStyle.h"
#include "Types/SlateVector2.h"

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

/** Used to determine how a scrollable widget reacts to the mouse wheel. */
UENUM()
enum class EConsumeMouseWheel : uint8
{
	/** Only consumes the mouse wheel event when the bar is able to scroll. */
	WhenScrollingPossible,
	/** Always consumes mouse wheel event even if the scrollable widget can not scroll. */
	Always,
	/** Never consumes the mouse wheel. */
	Never,
};

/** Represents the appearance of an STextBlock. */
struct FTextBlockStyle : public FSlateWidgetStyle
{
	UE_STUB_STRUCT_BODY(FTextBlockStyle)
	FTextBlockStyle();
	virtual ~FTextBlockStyle() {}
	virtual void GetResources(TArray<const FSlateBrush*>& OutBrushes) const override;
	static const FName TypeName;
	virtual const FName GetTypeName() const override { return TypeName; }
	static const FTextBlockStyle& GetDefault();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateFontInfo Font;
	FTextBlockStyle& SetFont(const FSlateFontInfo& InFont) { Font = InFont; return *this; }
	FTextBlockStyle& SetFontSize(float InSize) { Font.Size = InSize; return *this; }
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateColor ColorAndOpacity;
	FTextBlockStyle& SetColorAndOpacity(const FSlateColor& InColorAndOpacity) { ColorAndOpacity = InColorAndOpacity; return *this; }
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FDeprecateSlateVector2D ShadowOffset;
	FTextBlockStyle& SetShadowOffset(const UE::Slate::FDeprecateVector2DParameter& InShadowOffset) { ShadowOffset = InShadowOffset; return *this; }
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FLinearColor ShadowColorAndOpacity;
	FTextBlockStyle& SetShadowColorAndOpacity(const FLinearColor& InShadowColorAndOpacity) { ShadowColorAndOpacity = InShadowColorAndOpacity; return *this; }
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateColor SelectedBackgroundColor;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateColor HighlightColor;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush HighlightShape;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush StrikeBrush;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush UnderlineBrush;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	ETextTransformPolicy TransformPolicy;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	ETextOverflowPolicy OverflowPolicy;
};

/** Represents the appearance of an SButton. */
struct FButtonStyle : public FSlateWidgetStyle
{
	UE_STUB_STRUCT_BODY(FButtonStyle)
	FButtonStyle();
	virtual ~FButtonStyle() {}
	virtual void GetResources(TArray<const FSlateBrush*>& OutBrushes) const override;
	static const FName TypeName;
	virtual const FName GetTypeName() const override { return TypeName; }
	static const FButtonStyle& GetDefault();

	/** Button appearance when the button is not hovered or pressed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush Normal;
	FButtonStyle& SetNormal(const FSlateBrush& InNormal) { Normal = InNormal; return *this; }
	/** Button appearance when hovered */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush Hovered;
	FButtonStyle& SetHovered(const FSlateBrush& InHovered) { Hovered = InHovered; return *this; }
	/** Button appearance when pressed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush Pressed;
	FButtonStyle& SetPressed(const FSlateBrush& InPressed) { Pressed = InPressed; return *this; }
	/** Button appearance when disabled, by default this is set to an invalid resource when that is the case default disabled drawing is used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush Disabled;
	FButtonStyle& SetDisabled(const FSlateBrush& InDisabled) { Disabled = InDisabled; return *this; }
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateColor NormalForeground;
	FButtonStyle& SetNormalForeground(const FSlateColor& InNormalForeground) { NormalForeground = InNormalForeground; return *this; }
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateColor HoveredForeground;
	FButtonStyle& SetHoveredForeground(const FSlateColor& InHoveredForeground) { HoveredForeground = InHoveredForeground; return *this; }
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateColor PressedForeground;
	FButtonStyle& SetPressedForeground(const FSlateColor& InPressedForeground) { PressedForeground = InPressedForeground; return *this; }
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateColor DisabledForeground;
	FButtonStyle& SetDisabledForeground(const FSlateColor& InDisabledForeground) { DisabledForeground = InDisabledForeground; return *this; }
	/** Padding that accounts for the border in the button's background image. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FMargin NormalPadding;
	FButtonStyle& SetNormalPadding(const FMargin& InNormalPadding) { NormalPadding = InNormalPadding; return *this; }
	/** Same as NormalPadding but used when the button is pressed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FMargin PressedPadding;
	FButtonStyle& SetPressedPadding(const FMargin& InPressedPadding) { PressedPadding = InPressedPadding; return *this; }
	/** The sound the button should play when pressed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sound, meta = (DisplayName = "Pressed Sound"))
	FSlateSound PressedSlateSound;
	FButtonStyle& SetPressedSound(const FSlateSound& InPressedSound) { PressedSlateSound = InPressedSound; return *this; }
	/** The sound the button should play when clicked */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sound, meta = (DisplayName = "Clicked Sound"))
	FSlateSound ClickedSlateSound;
	/** The sound the button should play when initially hovered over */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sound, meta = (DisplayName = "Hovered Sound"))
	FSlateSound HoveredSlateSound;
	FButtonStyle& SetHoveredSound(const FSlateSound& InHoveredSound) { HoveredSlateSound = InHoveredSound; return *this; }
};

/** Represents the appearance of an SScrollBar. */
struct FScrollBarStyle : public FSlateWidgetStyle
{
	UE_STUB_STRUCT_BODY(FScrollBarStyle)
	FScrollBarStyle();
	virtual ~FScrollBarStyle() {}
	virtual void GetResources(TArray<const FSlateBrush*>& OutBrushes) const override;
	static const FName TypeName;
	virtual const FName GetTypeName() const override { return TypeName; }
	static const FScrollBarStyle& GetDefault();

	/** Background image to use when the scrollbar is oriented horizontally */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush HorizontalBackgroundImage;
	FScrollBarStyle& SetHorizontalBackgroundImage(const FSlateBrush& InHorizontalBackgroundImage) { HorizontalBackgroundImage = InHorizontalBackgroundImage; return *this; }
	/** Background image to use when the scrollbar is oriented vertically */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush VerticalBackgroundImage;
	FScrollBarStyle& SetVerticalBackgroundImage(const FSlateBrush& InVerticalBackgroundImage) { VerticalBackgroundImage = InVerticalBackgroundImage; return *this; }
	/** The image to use to represent the track above the thumb when the scrollbar is oriented vertically */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush VerticalTopSlotImage;
	FScrollBarStyle& SetVerticalTopSlotImage(const FSlateBrush& Value) { VerticalTopSlotImage = Value; return *this; }
	/** The image to use to represent the track above the thumb when the scrollbar is oriented horizontally */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush HorizontalTopSlotImage;
	FScrollBarStyle& SetHorizontalTopSlotImage(const FSlateBrush& Value) { HorizontalTopSlotImage = Value; return *this; }
	/** The image to use to represent the track below the thumb when the scrollbar is oriented vertically */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush VerticalBottomSlotImage;
	FScrollBarStyle& SetVerticalBottomSlotImage(const FSlateBrush& Value) { VerticalBottomSlotImage = Value; return *this; }
	/** The image to use to represent the track below the thumb when the scrollbar is oriented horizontally */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush HorizontalBottomSlotImage;
	FScrollBarStyle& SetHorizontalBottomSlotImage(const FSlateBrush& Value) { HorizontalBottomSlotImage = Value; return *this; }
	/** Image to use when the scrollbar thumb is in its normal state */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush NormalThumbImage;
	FScrollBarStyle& SetNormalThumbImage(const FSlateBrush& InNormalThumbImage) { NormalThumbImage = InNormalThumbImage; return *this; }
	/** Image to use when the scrollbar thumb is in its hovered state */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush HoveredThumbImage;
	FScrollBarStyle& SetHoveredThumbImage(const FSlateBrush& InHoveredThumbImage) { HoveredThumbImage = InHoveredThumbImage; return *this; }
	/** Image to use when the scrollbar thumb is in its dragged state */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush DraggedThumbImage;
	FScrollBarStyle& SetDraggedThumbImage(const FSlateBrush& InDraggedThumbImage) { DraggedThumbImage = InDraggedThumbImage; return *this; }
	/** The thickness in Slate Units of the scrollbar */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	float Thickness;
	FScrollBarStyle& SetThickness(float InThickness) { Thickness = InThickness; return *this; }
};

/** Represents the appearance of an SScrollBox. */
struct FScrollBoxStyle : public FSlateWidgetStyle
{
	UE_STUB_STRUCT_BODY(FScrollBoxStyle)
	FScrollBoxStyle();
	virtual ~FScrollBoxStyle() {}
	virtual void GetResources(TArray<const FSlateBrush*>& OutBrushes) const override;
	static const FName TypeName;
	virtual const FName GetTypeName() const override { return TypeName; }
	static const FScrollBoxStyle& GetDefault();

	/** The thickness of the scroll bar, in Slate units (FVector2f(BarThickness, BarThickness) is SScrollBox's default). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	float BarThickness;
	/** Brush used to draw the top shadow of a scrollbox */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush TopShadowBrush;
	/** Brush used to draw the bottom shadow of a scrollbox */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush BottomShadowBrush;
	/** Brush used to draw the left shadow of a scrollbox */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush LeftShadowBrush;
	/** Brush used to draw the right shadow of a scrollbox */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush RightShadowBrush;
	/** Padding scroll panel that presents the scrolled content */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FMargin HorizontalScrolledContentPadding;
	/** Padding scroll panel that presents the scrolled content */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FMargin VerticalScrolledContentPadding;
};

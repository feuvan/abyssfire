// Stub SDK. Real header: Engine/Source/Runtime/Slate/Public/Widgets/Input/SButton.h (UE 5.x source knowledge; the
// click / touch / press method enums are reflected and checked against the 5.6 stub in Types/SlateEnums.h).
// SButton: an SBorder that clicks. OnClicked returns FReply; OnPressed / OnReleased / OnHovered / OnUnhovered are
// FSimpleDelegate. The style pointers must outlive the widget.
#pragma once

#include "CoreMinimal.h"
#include "Framework/SlateDelegates.h"
#include "Input/Reply.h"
#include "Layout/Margin.h"
#include "Misc/Attribute.h"
#include "Styling/SlateColor.h"
#include "Styling/SlateTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SWidget.h"

class FPaintArgs;
class FSlateWindowElementList;
enum class ETextFlowDirection : uint8;
enum class ETextShapingMethod : uint8;

class SButton : public SBorder
{
public:
	SLATE_BEGIN_ARGS(SButton)
		: _Content()
		, _ButtonStyle(nullptr)   // the engine: &FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button")
		, _TextStyle(nullptr)     // the engine: &FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("ButtonText")
		, _HAlign(HAlign_Fill)
		, _VAlign(VAlign_Fill)
		, _ContentPadding(FMargin(4.0, 2.0))
		, _Text()
		, _ClickMethod(EButtonClickMethod::DownAndUp)
		, _TouchMethod(EButtonTouchMethod::DownAndUp)
		, _PressMethod(EButtonPressMethod::DownAndUp)
		, _DesiredSizeScale(FVector2D(1, 1))
		, _ContentScale(FVector2D(1, 1))
		, _ButtonColorAndOpacity(FLinearColor::White)
		, _ForegroundColor(FSlateColor::UseStyle())
		, _IsFocusable(true)
	{}
		/** Slot for this button's content (optional) */
		SLATE_DEFAULT_SLOT(FArguments, Content)
		/** The visual style of the button */
		SLATE_STYLE_ARGUMENT(FButtonStyle, ButtonStyle)
		/** The text style of the button */
		SLATE_STYLE_ARGUMENT(FTextBlockStyle, TextStyle)
		/** Horizontal alignment */
		SLATE_ARGUMENT(EHorizontalAlignment, HAlign)
		/** Vertical alignment */
		SLATE_ARGUMENT(EVerticalAlignment, VAlign)
		/** Spacing between button's border and the content. */
		SLATE_ATTRIBUTE(FMargin, ContentPadding)
		/** The text to display in this button, if no custom content is specified */
		SLATE_ATTRIBUTE(FText, Text)
		/** Called when the button is clicked */
		SLATE_EVENT(FOnClicked, OnClicked)
		/** Called when the button is pressed */
		SLATE_EVENT(FSimpleDelegate, OnPressed)
		/** Called when the button is released */
		SLATE_EVENT(FSimpleDelegate, OnReleased)
		SLATE_EVENT(FSimpleDelegate, OnHovered)
		SLATE_EVENT(FSimpleDelegate, OnUnhovered)
		/** Sets the rules to use for determining whether the button was clicked. */
		SLATE_ARGUMENT(EButtonClickMethod::Type, ClickMethod)
		/** How should the button be clicked with touch events? */
		SLATE_ARGUMENT(EButtonTouchMethod::Type, TouchMethod)
		/** How should the button be clicked with keyboard/controller button events? */
		SLATE_ARGUMENT(EButtonPressMethod::Type, PressMethod)
		SLATE_ATTRIBUTE(FVector2D, DesiredSizeScale)
		SLATE_ATTRIBUTE(FVector2D, ContentScale)
		SLATE_ATTRIBUTE(FSlateColor, ButtonColorAndOpacity)
		SLATE_ATTRIBUTE(FSlateColor, ForegroundColor)
		/** Sometimes a button should only be mouse-clickable and never keyboard focusable. */
		SLATE_ARGUMENT(bool, IsFocusable)
		/** The sound to play when the button is pressed */
		SLATE_ARGUMENT(TOptional<FSlateSound>, PressedSoundOverride)
		/** The sound to play when the button is hovered */
		SLATE_ARGUMENT(TOptional<FSlateSound>, HoveredSoundOverride)
		/** Which text shaping method should we use? (unset to use the default returned by GetDefaultTextShapingMethod) */
		SLATE_ARGUMENT(TOptional<ETextShapingMethod>, TextShapingMethod)
		/** Which text flow direction should we use? (unset to use the default returned by GetDefaultTextFlowDirection) */
		SLATE_ARGUMENT(TOptional<ETextFlowDirection>, TextFlowDirection)
	SLATE_END_ARGS()

	SButton();
	void Construct(const FArguments& InArgs);

	/** Returns true if this button is currently pressed */
	virtual bool IsPressed() const;
	void SetContentPadding(TAttribute<FMargin> InContentPadding);
	void SetOnClicked(FOnClicked InOnClicked);
	void SetButtonStyle(const FButtonStyle* ButtonStyle);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual bool SupportsKeyboardFocus() const override;
protected:
	virtual FVector2D ComputeDesiredSize(float) const override;
};

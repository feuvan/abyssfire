// Stub SDK. Real header: Engine/Source/Runtime/Slate/Public/Widgets/Layout/SScrollBox.h (UE 5.x source knowledge;
// reflected enums: checked against the 5.6 stub). SScrollBox: a scrolling stack. Slot(): .HAlign / .VAlign / .Padding
// and [ child ]; AddSlot() returns the same builder. The style pointers must outlive the widget. SScrollBar is only
// forward-declared (the engine header includes Widgets/Layout/SScrollBar.h).
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Framework/Layout/Overscroll.h"
#include "Input/Reply.h"
#include "Layout/BasicLayoutWidgetSlot.h"
#include "Layout/Children.h"
#include "Layout/Margin.h"
#include "Layout/Visibility.h"
#include "Misc/Attribute.h"
#include "Styling/SlateTypes.h"
#include "Types/SlateEnums.h"
#include "Types/SlateVector2.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWidget.h"

class FPaintArgs;
class FSlateWindowElementList;
class SScrollBar;

/** Where to scroll the descendant to */
UENUM(BlueprintType)
enum class EDescendantScrollDestination : uint8
{
	/** Scroll the widget into view using the least amount of energy possible. */
	IntoView,
	/** Always scroll the widget so it appears at the top/Left of the scrollable area. */
	TopOrLeft,
	/** Always scroll the widget so it appears at the center of the scrollable area, if possible. */
	Center,
	/** Always scroll the widget so it appears at the bottom/Right of the scrollable area. */
	BottomOrRight,
};

/** Set behavior when user focus changes inside this scroll box */
UENUM(BlueprintType)
enum class EScrollWhenFocusChanges : uint8
{
	/** Don't automatically scroll, navigation or child widget will handle this */
	NoScroll,
	/** Instantly scroll using NavigationDestination rule */
	InstantScroll,
	/** Use animation to scroll using NavigationDestination rule */
	AnimatedScroll,
};

/** SScrollBox can scroll through an arbitrary number of widgets. */
class SScrollBox : public SCompoundWidget
{
public:
	/** A Slot that provides layout options for the contents of a scrollable box. */
	class FSlot : public TBasicLayoutWidgetSlot<FSlot>
	{
	public:
		SLATE_SLOT_BEGIN_ARGS(FSlot, TBasicLayoutWidgetSlot<FSlot>)
		SLATE_SLOT_END_ARGS()
		void Construct(const FChildren& SlotOwner, FSlotArguments&& InArgs);
	};

	SLATE_BEGIN_ARGS(SScrollBox)
		: _Style(nullptr)            // the engine: &FAppStyle::Get().GetWidgetStyle<FScrollBoxStyle>("ScrollBox")
		, _ScrollBarStyle(nullptr)   // the engine: &FAppStyle::Get().GetWidgetStyle<FScrollBarStyle>("ScrollBar")
		, _ExternalScrollbar()
		, _Orientation(Orient_Vertical)
		, _ScrollBarVisibility(EVisibility::Visible)
		, _ScrollBarAlwaysVisible(false)
		, _ScrollBarDragFocusCause(EFocusCause::Mouse)
		, _ScrollBarThickness(FVector2f(9.0f, 9.0f))   // the engine: FVector2f(_Style->BarThickness, _Style->BarThickness)
		, _ScrollBarPadding(2.0f)
		, _AllowOverscroll(EAllowOverscroll::Yes)
		, _BackPadScrolling(false)
		, _FrontPadScrolling(false)
		, _AnimateWheelScrolling(false)
		, _WheelScrollMultiplier(1.f)
		, _NavigationDestination(EDescendantScrollDestination::IntoView)
		, _NavigationScrollPadding(0.0f)
		, _ScrollWhenFocusChanges(EScrollWhenFocusChanges::NoScroll)
		, _ConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible)
	{
		_Clipping = EWidgetClipping::ClipToBounds;
	}
		SLATE_SLOT_ARGUMENT(FSlot, Slots)
		/** Style used to draw this scrollbox */
		SLATE_STYLE_ARGUMENT(FScrollBoxStyle, Style)
		/** Style used to draw this scrollbox's scrollbar */
		SLATE_STYLE_ARGUMENT(FScrollBarStyle, ScrollBarStyle)
		/** Custom scroll bar */
		SLATE_ARGUMENT(TSharedPtr<SScrollBar>, ExternalScrollbar)
		/** The direction that children will be stacked, and also the direction the box will scroll. */
		SLATE_ARGUMENT(EOrientation, Orientation)
		SLATE_ARGUMENT(EVisibility, ScrollBarVisibility)
		SLATE_ARGUMENT(bool, ScrollBarAlwaysVisible)
		SLATE_ARGUMENT(EFocusCause, ScrollBarDragFocusCause)
		SLATE_ARGUMENT(UE::Slate::FDeprecateVector2DParameter, ScrollBarThickness)
		/** This accounts for total internal scroll bar padding; default 2.0f padding from the scroll bar itself is removed */
		SLATE_ARGUMENT(FMargin, ScrollBarPadding)
		SLATE_ARGUMENT(EAllowOverscroll, AllowOverscroll)
		SLATE_ARGUMENT(bool, BackPadScrolling)
		SLATE_ARGUMENT(bool, FrontPadScrolling)
		SLATE_ARGUMENT(bool, AnimateWheelScrolling)
		SLATE_ARGUMENT(float, WheelScrollMultiplier)
		SLATE_ARGUMENT(EDescendantScrollDestination, NavigationDestination)
		/** The amount of padding to ensure exists between the item being navigated to, at the edge of the scrollbox. */
		SLATE_ARGUMENT(float, NavigationScrollPadding)
		SLATE_ARGUMENT(EScrollWhenFocusChanges, ScrollWhenFocusChanges)
		// OnUserScrolled (FOnUserScrolled, declared by Widgets/Layout/SScrollBar.h) is not modelled: the module does not
		// bind it and SScrollBar.h is not part of the SDK.
		SLATE_ARGUMENT(EConsumeMouseWheel, ConsumeMouseWheel)
	SLATE_END_ARGS()

	SScrollBox();
	/** @return a new slot. Slots contain children for SScrollBox */
	static FSlot::FSlotArguments Slot() { return FSlot::FSlotArguments(MakeUnique<FSlot>()); }
	void Construct(const FArguments& InArgs);

	using FScopedWidgetSlotArguments = TPanelChildren<FSlot>::FScopedWidgetSlotArguments;
	/** Adds a slot to SScrollBox */
	FScopedWidgetSlotArguments AddSlot();
	/** Removes a slot at the specified location */
	void RemoveSlot(const TSharedRef<SWidget>& WidgetToRemove);
	/** Removes all children from the box */
	void ClearChildren();
	float GetScrollOffset() const;
	void SetScrollOffset(float NewScrollOffset);
	void ScrollToStart();
	void ScrollToEnd();
	/** Attempt to scroll a widget into view, will safely handle non-descendant widgets */
	void ScrollDescendantIntoView(const TSharedPtr<SWidget>& WidgetToFind, bool InAnimateScroll = true, EDescendantScrollDestination InDestination = EDescendantScrollDestination::IntoView, float InScrollPadding = 0);
};

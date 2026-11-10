// Stub SDK. Real header: Engine/Source/Runtime/Slate/Public/Widgets/SCanvas.h (UE 5.x source knowledge).
// SCanvas: children at absolute positions. Slot(): .Position(FVector2D) .Size(FVector2D) attributes, .HAlign / .VAlign
// (how the child sits relative to Position), [ child ]. No padding on a canvas slot.
#pragma once

#include "CoreMinimal.h"
#include "Layout/BasicLayoutWidgetSlot.h"
#include "Layout/Children.h"
#include "Misc/Attribute.h"
#include "SlotBase.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SPanel.h"
#include "Widgets/SWidget.h"

class FArrangedChildren;
class FPaintArgs;
class FSlateWindowElementList;

class SCanvas : public SPanel
{
public:
	/** A slot of the canvas: position, size, and the alignment of the child relative to the position. */
	class FSlot : public TWidgetSlotWithAttributeSupport<FSlot>, public TAlignmentWidgetSlotMixin<FSlot>
	{
	public:
		FSlot();
		SLATE_SLOT_BEGIN_ARGS_OneMixin(FSlot, TSlotBase<FSlot>, TAlignmentWidgetSlotMixin<FSlot>)
			SLATE_ATTRIBUTE(FVector2D, Position)
			SLATE_ATTRIBUTE(FVector2D, Size)
		SLATE_SLOT_END_ARGS()
		void SetPosition(TAttribute<FVector2D> InPosition);
		FVector2D GetPosition() const;
		void SetSize(TAttribute<FVector2D> InSize);
		FVector2D GetSize() const;
		void Construct(const FChildren& SlotOwner, FSlotArguments&& InArgs);
	};

	SLATE_BEGIN_ARGS(SCanvas)
	{
		_Visibility = EVisibility::SelfHitTestInvisible;
	}
		SLATE_SLOT_ARGUMENT(SCanvas::FSlot, Slots)
	SLATE_END_ARGS()

	SCanvas();
	void Construct(const FArguments& InArgs);
	static FSlot::FSlotArguments Slot() { return FSlot::FSlotArguments(MakeUnique<FSlot>()); }
	using FScopedWidgetSlotArguments = TPanelChildren<FSlot>::FScopedWidgetSlotArguments;
	/** Adds a content slot (it is added when the returned builder dies). */
	FScopedWidgetSlotArguments AddSlot();
	/** Removes a particular content slot; returns the index of the removed slot or -1. */
	int32 RemoveSlot(const TSharedRef<SWidget>& SlotWidget);
	/** Removes all slots from the panel. */
	void ClearChildren();
	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FChildren* GetChildren() override;
protected:
	virtual FVector2D ComputeDesiredSize(float) const override;
};

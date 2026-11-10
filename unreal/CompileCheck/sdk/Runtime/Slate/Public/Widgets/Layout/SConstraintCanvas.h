// Stub SDK. Real header: Engine/Source/Runtime/Slate/Public/Widgets/Layout/SConstraintCanvas.h (UE 5.x source knowledge).
// Anchored canvas (what UMG's Canvas Panel wraps). Slot(): .Offset(FMargin: Left, Top, Width, Height) .Anchors(FAnchors)
// .Alignment(FVector2D) .AutoSize(bool), each a TAttribute; [ child ]. The slot's ZOrder is not declared: the module
// does not set it and its 5.x shape (an attribute, or SLATE_ARGUMENT(TOptional<float>, ZOrder)) is not known here.
#pragma once

#include "CoreMinimal.h"
#include "Layout/Children.h"
#include "Layout/Margin.h"
#include "Misc/Attribute.h"
#include "SlotBase.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Layout/Anchors.h"
#include "Widgets/SPanel.h"

class SConstraintCanvas : public SPanel
{
public:
	class FSlot : public TSlotBase<FSlot>
	{
	public:
		FSlot();
		SLATE_SLOT_BEGIN_ARGS(FSlot, TSlotBase<FSlot>)
			SLATE_ATTRIBUTE(FMargin, Offset)
			SLATE_ATTRIBUTE(FAnchors, Anchors)
			SLATE_ATTRIBUTE(FVector2D, Alignment)
			SLATE_ATTRIBUTE(bool, AutoSize)
		SLATE_SLOT_END_ARGS()
		void Construct(const FChildren& SlotOwner, FSlotArguments&& InArgs);
		void SetOffset(TAttribute<FMargin> InOffset);
		FMargin GetOffset() const;
		void SetAnchors(TAttribute<FAnchors> InAnchors);
		FAnchors GetAnchors() const;
		void SetAlignment(TAttribute<FVector2D> InAlignment);
		FVector2D GetAlignment() const;
		void SetAutoSize(TAttribute<bool> InAutoSize);
		bool GetAutoSize() const;
	};

	SLATE_BEGIN_ARGS(SConstraintCanvas)
	{
		_Visibility = EVisibility::SelfHitTestInvisible;
	}
		SLATE_SLOT_ARGUMENT(FSlot, Slots)
	SLATE_END_ARGS()

	SConstraintCanvas();
	void Construct(const FArguments& InArgs);
	static FSlot::FSlotArguments Slot() { return FSlot::FSlotArguments(MakeUnique<FSlot>()); }
	using FScopedWidgetSlotArguments = TPanelChildren<FSlot>::FScopedWidgetSlotArguments;
	FScopedWidgetSlotArguments AddSlot();
	int32 RemoveSlot(const TSharedRef<SWidget>& SlotWidget);
	void ClearChildren();
	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FChildren* GetChildren() override;
protected:
	virtual FVector2D ComputeDesiredSize(float) const override;
};

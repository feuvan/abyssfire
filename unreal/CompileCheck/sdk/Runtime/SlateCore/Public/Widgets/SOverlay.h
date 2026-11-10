// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Widgets/SOverlay.h (UE 5.x source knowledge).
// SOverlay: children stacked in Z. Slot(): .HAlign / .VAlign / .Padding, [ child ]; AddSlot(ZOrder), RemoveSlot.
#pragma once

#include "CoreMinimal.h"
#include "Layout/BasicLayoutWidgetSlot.h"
#include "Layout/Children.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SPanel.h"

class SOverlay : public SPanel
{
public:
	class FOverlaySlot : public TBasicLayoutWidgetSlot<FOverlaySlot>
	{
	public:
		FOverlaySlot(int32 InZOrder);
		SLATE_SLOT_BEGIN_ARGS(FOverlaySlot, TBasicLayoutWidgetSlot<FOverlaySlot>)
		SLATE_SLOT_END_ARGS()
		int32 GetZOrder() const;
		void SetZOrder(int32 InOrder);
		void Construct(const FChildren& SlotOwner, FSlotArguments&& InArgs);
	};

	SLATE_BEGIN_ARGS(SOverlay)
	{
		_Visibility = EVisibility::SelfHitTestInvisible;
	}
		SLATE_SLOT_ARGUMENT(SOverlay::FOverlaySlot, Slots)
	SLATE_END_ARGS()

	SOverlay();
	void Construct(const FArguments& InArgs);
	int32 GetNumWidgets() const;
	bool RemoveSlot(TSharedRef<SWidget> Widget);
	bool RemoveSlot(int32 ZOrder = INDEX_NONE);
	void ClearChildren();
	static FOverlaySlot::FSlotArguments Slot() { return FOverlaySlot::FSlotArguments(MakeUnique<FOverlaySlot>(0)); }
	using FScopedWidgetSlotArguments = TPanelChildren<FOverlaySlot>::FScopedWidgetSlotArguments;
	FScopedWidgetSlotArguments AddSlot(int32 ZOrder = INDEX_NONE);
	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override;
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual FChildren* GetChildren() override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
};

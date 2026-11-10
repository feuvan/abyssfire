// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Widgets/SCompoundWidget.h (UE 5.x source knowledge).
// A widget with one child slot (ChildSlot [ ... ] with .Padding / .HAlign / .VAlign) and a colour / content scale.
#pragma once

#include "CoreMinimal.h"
#include "Layout/Children.h"
#include "Layout/Margin.h"
#include "Misc/Attribute.h"
#include "Styling/SlateColor.h"
#include "Types/SlateEnums.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"

class FArrangedChildren;
class FSlateWindowElementList;
class FWidgetStyle;

/** The single child slot of SCompoundWidget (FSingleWidgetChildrenWithBasicLayoutSlot in the engine). */
class FCompoundWidgetOneChildSlot : public FChildren, public TPaddingWidgetSlotMixin<FCompoundWidgetOneChildSlot>, public TAlignmentWidgetSlotMixin<FCompoundWidgetOneChildSlot>
{
public:
	FCompoundWidgetOneChildSlot(SWidget* InOwner);
	virtual int32 Num() const override;
	virtual TSharedRef<SWidget> GetChildAt(int32 ChildIndex) override;
	virtual TSharedRef<const SWidget> GetChildAt(int32 ChildIndex) const override;
	FCompoundWidgetOneChildSlot& operator[](const TSharedRef<SWidget>& InChildWidget);
	FCompoundWidgetOneChildSlot& Expose(FCompoundWidgetOneChildSlot*& OutVarToInit);
	void AttachWidget(const TSharedRef<SWidget>& InWidget);
	void DetachWidget();
	const TSharedRef<SWidget>& GetWidget() const;
};

class SCompoundWidget : public SWidget
{
public:
	UE::Slate::FDeprecateVector2DResult GetContentScale() const;
	void SetContentScale(TAttribute<FVector2D> InContentScale);
	FLinearColor GetColorAndOpacity() const;
	void SetColorAndOpacity(TAttribute<FLinearColor> InColorAndOpacity);
	void SetForegroundColor(TAttribute<FSlateColor> InForegroundColor);
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FChildren* GetChildren() override;
	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override;
	virtual FSlateColor GetForegroundColor() const override;
	virtual void SetVisibility(TAttribute<EVisibility> InVisibility) override final;
protected:
	virtual FVector2D ComputeDesiredSize(float) const override;
	SCompoundWidget();
	~SCompoundWidget();
	FCompoundWidgetOneChildSlot ChildSlot;
};

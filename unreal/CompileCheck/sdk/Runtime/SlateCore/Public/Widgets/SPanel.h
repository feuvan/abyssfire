// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Widgets/SPanel.h (UE 5.x source knowledge).
#pragma once

#include "CoreMinimal.h"
#include "Layout/Children.h"
#include "Widgets/SWidget.h"

class FArrangedChildren;
class FSlateWindowElementList;

class SPanel : public SWidget
{
public:
	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override = 0;
	virtual FVector2D ComputeDesiredSize(float) const override = 0;
	virtual FChildren* GetChildren() override = 0;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual void SetVisibility(TAttribute<EVisibility> InVisibility) override final;
protected:
	SPanel();
	int32 PaintArrangedChildren(const FPaintArgs& Args, const FArrangedChildren& ArrangedChildren, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const;
};

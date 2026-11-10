// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Widgets/SLeafWidget.h (UE 5.x source knowledge).
// A widget without children: a subclass MUST override OnPaint and ComputeDesiredSize (both pure here).
#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Widgets/SWidget.h"

class FArrangedChildren;
class FChildren;
class FPaintArgs;
class FSlateWindowElementList;

class SLeafWidget : public SWidget
{
public:
	virtual void SetVisibility(TAttribute<EVisibility> InVisibility) override final;
private:
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override = 0;
	virtual FVector2D ComputeDesiredSize(float) const override = 0;
public:
	virtual FChildren* GetChildren() override final;
	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override final;
};

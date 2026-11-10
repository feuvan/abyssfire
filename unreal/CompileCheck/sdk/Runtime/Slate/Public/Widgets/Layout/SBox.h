// Stub SDK. Real header: Engine/Source/Runtime/Slate/Public/Widgets/Layout/SBox.h (UE 5.x source knowledge).
// SBox: one child with alignment, padding and size overrides (FOptionalSize attributes: implicit from float).
// An SPanel since 5.0 (not an SCompoundWidget: no ChildSlot, no SetColorAndOpacity).
#pragma once

#include "CoreMinimal.h"
#include "Layout/Children.h"
#include "Layout/Margin.h"
#include "Misc/Attribute.h"
#include "Types/SlateEnums.h"
#include "Types/SlateStructs.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SPanel.h"
#include "Widgets/SWidget.h"

class FArrangedChildren;
class FPaintArgs;
class FSlateWindowElementList;

class SBox : public SPanel
{
public:
	SLATE_BEGIN_ARGS(SBox)
		: _HAlign(HAlign_Fill)
		, _VAlign(VAlign_Fill)
		, _Padding(0.0f)
		, _Content()
		, _WidthOverride(FOptionalSize())
		, _HeightOverride(FOptionalSize())
		, _MinDesiredWidth(FOptionalSize())
		, _MinDesiredHeight(FOptionalSize())
		, _MaxDesiredWidth(FOptionalSize())
		, _MaxDesiredHeight(FOptionalSize())
	{
		_Visibility = EVisibility::SelfHitTestInvisible;
	}
		/** Horizontal alignment of content in the area allotted to the SBox by its parent */
		SLATE_ARGUMENT(EHorizontalAlignment, HAlign)
		/** Vertical alignment of content in the area allotted to the SBox by its parent */
		SLATE_ARGUMENT(EVerticalAlignment, VAlign)
		/** Padding between the SBox and the content that it presents. Padding affects desired size. */
		SLATE_ATTRIBUTE(FMargin, Padding)
		/** The widget content presented by the SBox */
		SLATE_DEFAULT_SLOT(FArguments, Content)
		/** When specified, ignore the content's desired size and report the WidthOverride as the Box's desired width. */
		SLATE_ATTRIBUTE(FOptionalSize, WidthOverride)
		/** When specified, ignore the content's desired size and report the HeightOverride as the Box's desired height. */
		SLATE_ATTRIBUTE(FOptionalSize, HeightOverride)
		/** When specified, will report the MinDesiredWidth if larger than the content's desired width. */
		SLATE_ATTRIBUTE(FOptionalSize, MinDesiredWidth)
		/** When specified, will report the MinDesiredHeight if larger than the content's desired height. */
		SLATE_ATTRIBUTE(FOptionalSize, MinDesiredHeight)
		/** When specified, will report the MaxDesiredWidth if smaller than the content's desired width. */
		SLATE_ATTRIBUTE(FOptionalSize, MaxDesiredWidth)
		/** When specified, will report the MaxDesiredHeight if smaller than the content's desired height. */
		SLATE_ATTRIBUTE(FOptionalSize, MaxDesiredHeight)
		SLATE_ATTRIBUTE(FOptionalSize, MinAspectRatio)
		SLATE_ATTRIBUTE(FOptionalSize, MaxAspectRatio)
	SLATE_END_ARGS()

	SBox();
	void Construct(const FArguments& InArgs);

	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual FChildren* GetChildren() override;
	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	/** See the Content slot. */
	void SetContent(const TSharedRef<SWidget>& InContent);
	void SetHAlign(EHorizontalAlignment HAlign);
	void SetVAlign(EVerticalAlignment VAlign);
	void SetPadding(TAttribute<FMargin> InPadding);
	void SetWidthOverride(TAttribute<FOptionalSize> InWidthOverride);
	void SetHeightOverride(TAttribute<FOptionalSize> InHeightOverride);
};

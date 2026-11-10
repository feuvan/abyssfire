// Stub SDK. Real header: Engine/Source/Runtime/Slate/Public/Widgets/Layout/SBorder.h (UE 5.x source knowledge).
// SBorder: a brush behind one child. BorderImage is a `const FSlateBrush*` attribute (the brush must outlive the
// widget), BorderBackgroundColor tints the image, ColorAndOpacity (FLinearColor) the content.
#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Layout/Margin.h"
#include "Misc/Attribute.h"
#include "Styling/SlateColor.h"
#include "Types/SlateEnums.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWidget.h"

class FPaintArgs;
class FSlateWindowElementList;
struct FSlateBrush;

class SBorder : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBorder)
		: _Content()
		, _HAlign(HAlign_Fill)
		, _VAlign(VAlign_Fill)
		, _Padding(FMargin(2.0f))
		, _OnMouseButtonDown()
		, _OnMouseButtonUp()
		, _OnMouseMove()
		, _OnMouseDoubleClick()
		, _BorderImage()   // the engine: FCoreStyle::Get().GetBrush("Border")
		, _ContentScale(FVector2D(1, 1))
		, _DesiredSizeScale(FVector2D(1, 1))
		, _ColorAndOpacity(FLinearColor(1, 1, 1, 1))
		, _BorderBackgroundColor(FLinearColor::White)
		, _ForegroundColor(FSlateColor::UseForeground())
		, _ShowEffectWhenDisabled(true)
		, _FlipForRightToLeftFlowDirection(false)
	{}
		SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_ARGUMENT(EHorizontalAlignment, HAlign)
		SLATE_ARGUMENT(EVerticalAlignment, VAlign)
		SLATE_ATTRIBUTE(FMargin, Padding)
		SLATE_EVENT(FPointerEventHandler, OnMouseButtonDown)
		SLATE_EVENT(FPointerEventHandler, OnMouseButtonUp)
		SLATE_EVENT(FPointerEventHandler, OnMouseMove)
		SLATE_EVENT(FPointerEventHandler, OnMouseDoubleClick)
		SLATE_ATTRIBUTE(const FSlateBrush*, BorderImage)
		SLATE_ATTRIBUTE(FVector2D, ContentScale)
		SLATE_ATTRIBUTE(FVector2D, DesiredSizeScale)
		/** ColorAndOpacity is the color and opacity of content in the border */
		SLATE_ATTRIBUTE(FLinearColor, ColorAndOpacity)
		/** BorderBackgroundColor refers to the actual color and opacity of the supplied border image. */
		SLATE_ATTRIBUTE(FSlateColor, BorderBackgroundColor)
		/** The foreground color of text and some glyphs that appear as the border's content. */
		SLATE_ATTRIBUTE(FSlateColor, ForegroundColor)
		/** Whether or not to show the disabled effect when this border is disabled */
		SLATE_ATTRIBUTE(bool, ShowEffectWhenDisabled)
		/** Flips the background image if the localization's flow direction is RTL */
		SLATE_ARGUMENT(bool, FlipForRightToLeftFlowDirection)
	SLATE_END_ARGS()

	SBorder();
	void Construct(const FArguments& InArgs);

	/** Sets the content for this border. */
	virtual void SetContent(TSharedRef<SWidget> InContent);
	void SetBorderBackgroundColor(TAttribute<FSlateColor> InColorAndOpacity);
	void SetPadding(TAttribute<FMargin> InPadding);
	void SetBorderImage(TAttribute<const FSlateBrush*> InBorderImage);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
protected:
	virtual FVector2D ComputeDesiredSize(float) const override;
};

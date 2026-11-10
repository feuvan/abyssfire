// Stub SDK. Real header: Engine/Source/Runtime/Slate/Public/Widgets/Layout/SScaleBox.h (UE 5.x source knowledge;
// reflected enums: checked against the 5.6 stub). SScaleBox: scales its one child to the allotted area by a rule.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Layout/Visibility.h"
#include "Misc/Attribute.h"
#include "Types/SlateEnums.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWidget.h"

class FArrangedChildren;
class FPaintArgs;
class FSlateWindowElementList;

UENUM(BlueprintType)
namespace EStretchDirection
{
	enum Type : int
	{
		/** Will scale the content up or down. */
		Both,
		/** Will only make the content smaller, will never scale it larger than the content's desired size. */
		DownOnly,
		/** Will only make the content larger, will never scale it smaller than the content's desired size. */
		UpOnly,
	};
}

UENUM(BlueprintType)
namespace EStretch
{
	enum Type : int
	{
		/** Does not scale the content. */
		None,
		/** Scales the content non-uniformly filling the entire space of the area. */
		Fill,
		/** Scales the content uniformly (preserving aspect ratio) until it can no longer scale the content without clipping it. */
		ScaleToFit,
		/** Scales the content uniformly (preserving aspect ratio) until it can no longer scale the content without clipping it along the x-axis. */
		ScaleToFitX,
		/** Scales the content uniformly (preserving aspect ratio) until it can no longer scale the content without clipping it along the y-axis. */
		ScaleToFitY,
		/** Scales the content uniformly (preserving aspect ratio), until all sides meet or exceed the size of the area. */
		ScaleToFill,
		/** Scales the content according to the size of the safe zone currently applied to the viewport. */
		ScaleBySafeZone,
		/** Scales the content by the scale specified by the user. */
		UserSpecified,
		/** Scales the content by the scale specified by the user and also clips. */
		UserSpecifiedWithClipping,
	};
}

class SScaleBox : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SScaleBox)
		: _Content()
		, _HAlign(HAlign_Center)
		, _VAlign(VAlign_Center)
		, _StretchDirection(EStretchDirection::Both)
		, _Stretch(EStretch::None)
		, _UserSpecifiedScale(1.0f)
		, _IgnoreInheritedScale(false)
	{}
		/** Slot for this designers content (optional) */
		SLATE_DEFAULT_SLOT(FArguments, Content)
		/** The horizontal alignment of the content */
		SLATE_ARGUMENT(EHorizontalAlignment, HAlign)
		/** The vertical alignment of the content */
		SLATE_ARGUMENT(EVerticalAlignment, VAlign)
		/** Controls in what direction content can be scaled */
		SLATE_ATTRIBUTE(EStretchDirection::Type, StretchDirection)
		/** The stretching rule to apply when content is stretched */
		SLATE_ATTRIBUTE(EStretch::Type, Stretch)
		/** Optional scale that can be specified by the User */
		SLATE_ATTRIBUTE(float, UserSpecifiedScale)
		/** Undo any inherited scale factor before applying this scale box's scale */
		SLATE_ATTRIBUTE(bool, IgnoreInheritedScale)
#if WITH_EDITOR
		/** Force a particular screen size to be used instead of the reported device size. */
		SLATE_ARGUMENT(TOptional<FVector2D>, OverrideScreenSize)
#endif
	SLATE_END_ARGS()

	SScaleBox();
	virtual ~SScaleBox();
	void Construct(const FArguments& InArgs);
	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	/** See Content slot */
	void SetContent(TSharedRef<SWidget> InContent);
	void SetStretchDirection(EStretchDirection::Type InStretchDirection);
	void SetStretch(EStretch::Type InStretch);
protected:
	virtual FVector2D ComputeDesiredSize(float InScale) const override;
	virtual float GetRelativeLayoutScale(int32 ChildIndex, float LayoutScaleMultiplier) const override;
};

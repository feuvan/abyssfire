// Stub SDK. Real header: Engine/Source/Runtime/Slate/Public/Widgets/Layout/SSafeZone.h (UE 5.x source knowledge).
// SSafeZone: an SBox that pads its child by the device's title-safe (or action-safe) area.
#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"
#include "Misc/Attribute.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Layout/SBox.h"

class SSafeZone : public SBox
{
	SLATE_BEGIN_ARGS(SSafeZone)
		: _HAlign(HAlign_Fill)
		, _VAlign(VAlign_Fill)
		, _Padding(0.0f)
		, _Content()
		, _IsTitleSafe(true)
		, _SafeAreaScale(1, 1, 1, 1)
		, _PadLeft(true)
		, _PadRight(true)
		, _PadTop(true)
		, _PadBottom(true)
#if WITH_EDITOR
		, _OverrideScreenSize()
		, _OverrideDpiScale()
#endif
	{}
		/** Horizontal alignment of content in the area allotted to the SBox by its parent */
		SLATE_ARGUMENT(EHorizontalAlignment, HAlign)
		/** Vertical alignment of content in the area allotted to the SBox by its parent */
		SLATE_ARGUMENT(EVerticalAlignment, VAlign)
		/** Padding between the SBox and the content that it presents. Padding affects desired size. */
		SLATE_ATTRIBUTE(FMargin, Padding)
		/** The widget content presented by the SBox */
		SLATE_DEFAULT_SLOT(FArguments, Content)
		/** True if the zone is TitleSafe, otherwise it's the ActionSafe area. */
		SLATE_ARGUMENT(bool, IsTitleSafe)
		/** The scalar to apply to each side we want to have safe padding for. */
		SLATE_ARGUMENT(FMargin, SafeAreaScale)
		/** If this safe zone should pad for the left side of the screen's safe zone */
		SLATE_ARGUMENT(bool, PadLeft)
		/** If this safe zone should pad for the right side of the screen's safe zone */
		SLATE_ARGUMENT(bool, PadRight)
		/** If this safe zone should pad for the top side of the screen's safe zone */
		SLATE_ARGUMENT(bool, PadTop)
		/** If this safe zone should pad for the bottom side of the screen's safe zone */
		SLATE_ARGUMENT(bool, PadBottom)
#if WITH_EDITOR
		/** Force a particular screen size to be used instead of the reported device size. */
		SLATE_ARGUMENT(TOptional<FVector2D>, OverrideScreenSize)
		/** Use a custom dpi scale. */
		SLATE_ARGUMENT(TOptional<float>, OverrideDpiScale)
#endif
	SLATE_END_ARGS()

public:
	SSafeZone();
	virtual ~SSafeZone();
	void Construct(const FArguments& InArgs);
	void SetTitleSafe(bool bIsTitleSafe);
	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScale) const override;
};

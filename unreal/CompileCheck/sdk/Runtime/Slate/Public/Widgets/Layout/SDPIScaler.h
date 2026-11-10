// Stub SDK. Real header: Engine/Source/Runtime/Slate/Public/Widgets/Layout/SDPIScaler.h (UE 5.x source knowledge).
// SDPIScaler: lays its one child out at DPIScale (a float attribute) times the incoming layout scale.
#pragma once

#include "CoreMinimal.h"
#include "Layout/Children.h"
#include "Layout/Visibility.h"
#include "Misc/Attribute.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SPanel.h"
#include "Widgets/SWidget.h"

class FArrangedChildren;

class SDPIScaler : public SPanel
{
public:
	SLATE_BEGIN_ARGS(SDPIScaler)
		: _DPIScale(1.0f)
		, _Content()
	{
		_Visibility = EVisibility::SelfHitTestInvisible;
	}
		/** The DPI scaling factor */
		SLATE_ATTRIBUTE(float, DPIScale)
		/** The content being presented and scaled. */
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	SDPIScaler();
	void Construct(const FArguments& InArgs);
	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override;
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual FChildren* GetChildren() override;
	/** See the Content attribute */
	void SetContent(TSharedRef<SWidget> InContent);
	/** See the DPIScale attribute */
	void SetDPIScale(TAttribute<float> InDPIScale);
protected:
	virtual float GetRelativeLayoutScale(int32 ChildIndex, float LayoutScaleMultiplier) const override;
};

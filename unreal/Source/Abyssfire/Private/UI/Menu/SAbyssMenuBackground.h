// The painted main-menu backdrop (save-ui-input.md 1.2 buildBackground, render-only): vertical gradient 0x050508 ->
// 0x1a0808, a pulsing fire glow at the bottom, rising embers and sparks, drifting smoke, the slowly rotating rune circle
// behind the title and a vignette. Everything is a function of real time (no state), so it never hitches.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"

#include "UI/Core/AbyssUiContext.h"

class SAbyssMenuBackground : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SAbyssMenuBackground) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext);

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override { return FVector2D(1280.0, 720.0); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

protected:
	/** Animated every frame: never cached by Slate invalidation. */
	virtual bool ComputeVolatility() const override { return true; }

private:
	TSharedPtr<FAbyssUiContext> Context;
	double StartTime = 0.0;
};

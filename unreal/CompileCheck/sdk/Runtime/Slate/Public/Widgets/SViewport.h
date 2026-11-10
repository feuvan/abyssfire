// Stub SDK. Real header: Engine/Source/Runtime/Slate/Public/Widgets/SViewport.h (UE 5.x source knowledge).
// The Slate widget that hosts the game's 3D viewport.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class ISlateViewport;

class SViewport : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SViewport)
		: _Content()
		, _ShowEffectWhenDisabled(true)
		, _RenderDirectlyToWindow(false)
		, _EnableGammaCorrection(true)
		, _EnableBlending(false)
		, _IgnoreTextureAlpha(true)
		, _PreMultipliedAlpha(true)
	{}
		SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_ATTRIBUTE(bool, ShowEffectWhenDisabled)
		SLATE_ARGUMENT(bool, RenderDirectlyToWindow)
		SLATE_ARGUMENT(bool, EnableGammaCorrection)
		SLATE_ARGUMENT(bool, EnableBlending)
		SLATE_ARGUMENT(bool, IgnoreTextureAlpha)
		SLATE_ARGUMENT(bool, PreMultipliedAlpha)
		SLATE_ARGUMENT(TSharedPtr<ISlateViewport>, ViewportInterface)
		SLATE_ATTRIBUTE(FVector2D, ViewportSize)
	SLATE_END_ARGS()

	SViewport();
	void Construct(const FArguments& InArgs);
	void SetViewportInterface(TSharedRef<ISlateViewport> InViewportInterface);
	TWeakPtr<ISlateViewport> GetViewportInterface();
	void SetContent(TSharedPtr<SWidget> InContent);
	const TSharedPtr<SWidget> GetContent() const;
	void SetActive(bool bActive);
	void SetIgnoreTextureAlpha(const bool bInIgnoreTextureAlpha);
	bool GetIgnoreTextureAlpha() const;
	virtual bool SupportsKeyboardFocus() const override;
};

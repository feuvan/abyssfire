// Full-screen dialogs hosted by SAbyssUiRoot: the modal backdrop, confirm dialogs (save-ui-input 7.2, U1), the system
// error dialog (data load failure, unreadable save, save write failure), the controls reference (menu "Controls",
// 1.2 help modal with the Q25 fix: every panel key is listed) and the credits sheet (1.2, port credits).
#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

#include "UI/Core/AbyssUiContext.h"

DECLARE_DELEGATE_OneParam(FOnAbyssConfirmResult, bool /*bConfirmed*/);

/** Dim + vignette backdrop (UiKit backdrop: radial rgba(0,0,0,0.25) -> 0.7, scaled by Alpha). Swallows every press. */
class SAbyssBackdrop : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAbyssBackdrop)
		: _Alpha(0.6f)
	{
	}
		SLATE_ARGUMENT(float, Alpha)
		SLATE_EVENT(FSimpleDelegate, OnPressed)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext);
	void SetAlpha(float InAlpha) { Alpha = InAlpha; }

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnTouchStarted(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent) override;
	virtual FReply OnTouchEnded(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override { return FVector2D(8.0, 8.0); }

private:
	TSharedPtr<FAbyssUiContext> Context;
	FSimpleDelegate OnPressed;
	float Alpha = 0.6f;
};

/** Centred confirm card: title, body (wrapped, optional colour), confirm (+ cancel) buttons. */
class SAbyssConfirmDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAbyssConfirmDialog) {}
		SLATE_EVENT(FOnAbyssConfirmResult, OnResult)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext, const FAbyssConfirmRequest& Request);

private:
	FOnAbyssConfirmResult OnResult;
};

/** System error: title + message (+ Quit for unrecoverable data errors, OK otherwise). */
class SAbyssErrorDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAbyssErrorDialog)
		: _ShowQuit(false)
	{
	}
		SLATE_ARGUMENT(FText, Title)
		SLATE_ARGUMENT(FText, Message)
		SLATE_ARGUMENT(bool, ShowQuit)
		SLATE_EVENT(FSimpleDelegate, OnDismiss)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext);
};

/** Controls reference: keyboard / mouse rows built from the live input bindings, plus the touch and gamepad notes. */
class SAbyssHelpSheet : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAbyssHelpSheet) {}
		SLATE_EVENT(FSimpleDelegate, OnClose)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext);
};

/** Credits of the UE build (engine, fonts, art and audio pipelines). */
class SAbyssCreditsSheet : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAbyssCreditsSheet) {}
		SLATE_EVENT(FSimpleDelegate, OnClose)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext);
};

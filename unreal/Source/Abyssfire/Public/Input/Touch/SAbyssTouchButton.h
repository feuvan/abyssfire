// One touch button (save-ui-input.md 5.7.3 createRoundButton / ghost squares): round medallion or square frame, icon or
// localized label, cooldown sweep + countdown, potion count badge, dim / blocked / active looks, press feedback (scale
// 0.92) and the ready pop (1.12 -> 1 over 180 ms). Fires OnPressed IMMEDIATELY on the press (no release semantics, web
// parity) and claims the pointer (FReply::Handled + capture), so a press on a control never reaches the world (5.7.4).
// Mouse presses behave the same, for desktop testing of the touch layout (-abysstouch).
#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"

#include "Input/Touch/AbyssTouchLayout.h"

class ABYSSFIRE_API SAbyssTouchButton : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SAbyssTouchButton)
		: _Shape(EAbyssTouchShape::Circle)
		, _DrawSize(FVector2D(56.0, 56.0))
		, _HitScale(1.12f)
		, _FillColor(FLinearColor(0.02f, 0.02f, 0.025f, 0.85f))
		, _OutlineColor(FLinearColor(0.66f, 0.38f, 0.07f, 0.9f))
		, _BackgroundBrush(nullptr)
		, _CaptionBelowIcon(false)
	{}
		SLATE_ARGUMENT(EAbyssTouchShape, Shape)
		/** Drawn size (circle: diameter), Slate units. The widget is DrawSize * HitScale (the hit area). */
		SLATE_ARGUMENT(FVector2D, DrawSize)
		SLATE_ARGUMENT(float, HitScale)
		SLATE_ARGUMENT(FLinearColor, FillColor)
		SLATE_ARGUMENT(FLinearColor, OutlineColor)
		/** Optional face texture (UI medallion); drawn instead of the flat fill. Must outlive the widget. */
		SLATE_ARGUMENT(const FSlateBrush*, BackgroundBrush)
		/** Squares: icon in the upper part, label as a caption under it (panel / toggle style). */
		SLATE_ARGUMENT(bool, CaptionBelowIcon)
		/** Base font (CJK capable); resized per button. */
		SLATE_ARGUMENT(FSlateFontInfo, Font)
		SLATE_EVENT(FSimpleDelegate, OnPressed)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// ---- state, set by the owner every tick (cheap; the widget repaints every frame) ----
	void SetLabel(const FString& InLabel) { Label = InLabel; }
	void SetLabelColor(const FLinearColor& InColor) { LabelColor = InColor; }
	void SetIcon(const FSlateBrush* InIcon) { Icon = InIcon; }
	void SetBadge(const FString& InBadge) { Badge = InBadge; }
	/** Remaining fraction 0..1 of a cooldown / channel (0 = ready) and its countdown text. */
	void SetCooldown(float InRemainingFraction, const FString& InText);
	/** Multiplies the opacity (web: 0.7 while cooling, 0.5 when the portal would refuse). */
	void SetDimAlpha(float InAlpha) { DimAlpha = FMath::Clamp(InAlpha, 0.0f, 1.0f); }
	/** Disabled look (dead hero, 5.1.1). Presses still fire: the core decides and logs. */
	void SetBlocked(bool bInBlocked) { bBlocked = bInBlocked; }
	/** Bright outline (lock target held, toggle on). */
	void SetActive(bool bInActive) { bActive = bInActive; }
	void SetOutlineColor(const FLinearColor& InColor) { OutlineColor = InColor; }
	/** Ready pop: scale 1.12 -> 1 over 180 ms. */
	void PlayReadyPop();

	// ---- SWidget ----
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	virtual FReply OnTouchStarted(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent) override;
	virtual FReply OnTouchEnded(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;
	virtual bool SupportsKeyboardFocus() const override { return false; }

private:
	bool IsInsideHitArea(const FGeometry& MyGeometry, const FVector2D& ScreenPosition) const;
	FReply HandlePointerDown(const FGeometry& MyGeometry, const FPointerEvent& Event);
	FReply HandlePointerUp(const FPointerEvent& Event);
	float CurrentScale() const;
	void DrawLines(FSlateWindowElementList& OutDrawElements, int32 LayerId, const FGeometry& AllottedGeometry,
		const FString& Text, const FSlateFontInfo& InFont, const FVector2D& Center, float MaxWidth,
		const FLinearColor& Color) const;

	FSimpleDelegate OnPressed;
	FSlateBrush ShapeBrush;
	const FSlateBrush* BackgroundBrush = nullptr;
	const FSlateBrush* Icon = nullptr;
	FSlateFontInfo BaseFont;
	FString Label;
	FString Badge;
	FString CooldownText;
	FVector2D DrawSize = FVector2D(56.0, 56.0);
	FLinearColor FillColor = FLinearColor::Black;
	FLinearColor OutlineColor = FLinearColor::White;
	FLinearColor LabelColor = FLinearColor::White;
	double PopStartSeconds = -1.0;
	float HitScale = 1.12f;
	float CooldownFraction = 0.0f;
	float DimAlpha = 1.0f;
	int32 PressedPointerIndex = INDEX_NONE;
	EAbyssTouchShape Shape = EAbyssTouchShape::Circle;
	bool bCaptionBelowIcon = false;
	bool bBlocked = false;
	bool bActive = false;
};

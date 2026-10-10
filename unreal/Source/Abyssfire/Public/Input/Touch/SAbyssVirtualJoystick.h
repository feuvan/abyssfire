// The fixed-centre virtual joystick (save-ui-input.md 5.7.3): a press inside the grab circle (base radius * 1.35)
// claims the pointer; the thumb follows the finger clamped to the base radius; value = offset / radius in [-1, 1] per
// axis (screen space, Y down) with a 0.15 dead zone (Q15 fix); release resets. The value is published to
// UAbyssInputSubsystem::SetVirtualStick every Slate tick while held; the player controller injects it into IA_Move
// (ue58-platform.md 8.2). Mouse drags behave the same for desktop testing.
#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "UObject/WeakObjectPtr.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"

class UAbyssInputSubsystem;

class ABYSSFIRE_API SAbyssVirtualJoystick : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SAbyssVirtualJoystick)
		: _BaseRadius(58.0f)
		, _GrabScale(1.35f)
		, _DeadZone(0.15f)
		, _BaseColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.35f))
		, _RingColor(FLinearColor(0.66f, 0.38f, 0.07f, 0.75f))
		, _ThumbColor(FLinearColor(0.85f, 0.72f, 0.45f, 0.85f))
	{}
		/** Base radius, Slate units (px(58) of the layout). The widget is 2 * BaseRadius * GrabScale square. */
		SLATE_ARGUMENT(float, BaseRadius)
		SLATE_ARGUMENT(float, GrabScale)
		SLATE_ARGUMENT(float, DeadZone)
		SLATE_ARGUMENT(FLinearColor, BaseColor)
		SLATE_ARGUMENT(FLinearColor, RingColor)
		SLATE_ARGUMENT(FLinearColor, ThumbColor)
		SLATE_ARGUMENT(TWeakObjectPtr<UAbyssInputSubsystem>, InputSubsystem)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Releases the stick (layer hidden, layout rebuilt, app backgrounded). */
	void Reset();
	bool IsActive() const { return ActivePointerIndex != INDEX_NONE; }
	/** Current value, screen space (X right, Y down), dead zone applied. */
	FVector2D GetValue() const;
	/** A press the layer resolved to the stick (inside the grab circle, under a neighbour's box): grabs like a direct press. */
	FReply BeginForwardedPress(const FPointerEvent& Event);

	// ---- SWidget ----
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	virtual FReply OnTouchStarted(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent) override;
	virtual FReply OnTouchMoved(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent) override;
	virtual FReply OnTouchEnded(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;
	virtual bool SupportsKeyboardFocus() const override { return false; }

private:
	FReply HandleDown(const FGeometry& MyGeometry, const FPointerEvent& Event);
	FReply HandleMove(const FGeometry& MyGeometry, const FPointerEvent& Event);
	FReply HandleUp(const FPointerEvent& Event);
	void UpdateThumb(const FGeometry& MyGeometry, const FVector2D& ScreenPosition);
	void Publish(bool bActive) const;

	TWeakObjectPtr<UAbyssInputSubsystem> InputSubsystem;
	FSlateBrush CircleBrush;
	FLinearColor BaseColor = FLinearColor::Black;
	FLinearColor RingColor = FLinearColor::White;
	FLinearColor ThumbColor = FLinearColor::White;
	/** Thumb offset from the centre, Slate units, clamped to BaseRadius. */
	FVector2D ThumbOffset = FVector2D::ZeroVector;
	float BaseRadius = 58.0f;
	float GrabScale = 1.35f;
	float DeadZone = 0.15f;
	int32 ActivePointerIndex = INDEX_NONE;
};

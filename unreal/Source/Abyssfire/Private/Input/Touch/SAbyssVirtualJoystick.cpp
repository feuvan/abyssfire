#include "Input/Touch/SAbyssVirtualJoystick.h"

#include "Layout/Geometry.h"
#include "InputCoreTypes.h"
#include "Rendering/DrawElements.h"

#include "Input/AbyssInputSubsystem.h"

namespace
{
	constexpr float AbyssJoystick_ThumbRatio = 0.42f;
	constexpr float AbyssJoystick_RingWidth = 2.0f;

	FPaintGeometry AbyssJoystick_Circle(const FGeometry& Geometry, const FVector2D& Center, double Radius)
	{
		const FVector2D Size(Radius * 2.0, Radius * 2.0);
		return Geometry.ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform(FVector2f(Center - Size * 0.5)));
	}
}

void SAbyssVirtualJoystick::Construct(const FArguments& InArgs)
{
	InputSubsystem = InArgs._InputSubsystem;
	BaseRadius = FMath::Max(8.0f, InArgs._BaseRadius);
	GrabScale = FMath::Max(1.0f, InArgs._GrabScale);
	DeadZone = FMath::Clamp(InArgs._DeadZone, 0.0f, 0.9f);
	BaseColor = InArgs._BaseColor;
	RingColor = InArgs._RingColor;
	ThumbColor = InArgs._ThumbColor;

	CircleBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	CircleBrush.TintColor = FSlateColor(FLinearColor::White);
	CircleBrush.OutlineSettings.Width = 0.0f;
	CircleBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;

	ForceVolatile(true);
}

FVector2D SAbyssVirtualJoystick::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	const double Size = 2.0 * BaseRadius * GrabScale;
	return FVector2D(Size, Size);
}

FVector2D SAbyssVirtualJoystick::GetValue() const
{
	if (!IsActive())
	{
		return FVector2D::ZeroVector;
	}
	const FVector2D Value = ThumbOffset / BaseRadius;
	return Value.Size() < DeadZone ? FVector2D::ZeroVector : Value;
}

void SAbyssVirtualJoystick::Publish(bool bActive) const
{
	if (UAbyssInputSubsystem* Subsystem = InputSubsystem.Get())
	{
		Subsystem->SetVirtualStick(GetValue(), bActive);
	}
}

void SAbyssVirtualJoystick::Reset()
{
	const bool bWasActive = IsActive();
	ActivePointerIndex = INDEX_NONE;
	ThumbOffset = FVector2D::ZeroVector;
	if (bWasActive)
	{
		Publish(false);
	}
}

void SAbyssVirtualJoystick::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SLeafWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	if (IsActive())
	{
		Publish(true);   // keeps the subsystem's stick fresh (stale after 250 ms without a tick)
	}
}

void SAbyssVirtualJoystick::UpdateThumb(const FGeometry& MyGeometry, const FVector2D& ScreenPosition)
{
	const FVector2D Local = FVector2D(MyGeometry.AbsoluteToLocal(ScreenPosition));
	const FVector2D Center = FVector2D(MyGeometry.GetLocalSize()) * 0.5;
	FVector2D Offset = Local - Center;
	const double Length = Offset.Size();
	if (Length > BaseRadius && Length > 0.0)
	{
		Offset *= BaseRadius / Length;
	}
	ThumbOffset = Offset;
}

FReply SAbyssVirtualJoystick::HandleDown(const FGeometry& MyGeometry, const FPointerEvent& Event)
{
	if (!Event.IsTouchEvent() && Event.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}
	const FVector2D Local = FVector2D(MyGeometry.AbsoluteToLocal(FVector2D(Event.GetScreenSpacePosition())));
	const FVector2D Center = FVector2D(MyGeometry.GetLocalSize()) * 0.5;
	const double GrabRadius = BaseRadius * GrabScale;
	if (FVector2D::DistSquared(Local, Center) > GrabRadius * GrabRadius)
	{
		return FReply::Unhandled();   // the box corners outside the grab circle belong to the world
	}
	if (IsActive())
	{
		return FReply::Handled();     // a second finger on the stick is claimed but ignored
	}
	ActivePointerIndex = static_cast<int32>(Event.GetPointerIndex());
	UpdateThumb(MyGeometry, FVector2D(Event.GetScreenSpacePosition()));
	Publish(true);
	return FReply::Handled().CaptureMouse(SharedThis(this));
}

FReply SAbyssVirtualJoystick::BeginForwardedPress(const FPointerEvent& Event)
{
	if (IsActive())
	{
		return FReply::Handled();
	}
	ActivePointerIndex = static_cast<int32>(Event.GetPointerIndex());
	UpdateThumb(GetTickSpaceGeometry(), FVector2D(Event.GetScreenSpacePosition()));
	Publish(true);
	return FReply::Handled().CaptureMouse(SharedThis(this));
}

FReply SAbyssVirtualJoystick::HandleMove(const FGeometry& MyGeometry, const FPointerEvent& Event)
{
	if (!IsActive() || static_cast<int32>(Event.GetPointerIndex()) != ActivePointerIndex)
	{
		return FReply::Unhandled();
	}
	UpdateThumb(MyGeometry, FVector2D(Event.GetScreenSpacePosition()));
	Publish(true);
	return FReply::Handled();
}

FReply SAbyssVirtualJoystick::HandleUp(const FPointerEvent& Event)
{
	if (!IsActive() || static_cast<int32>(Event.GetPointerIndex()) != ActivePointerIndex)
	{
		return FReply::Unhandled();
	}
	Reset();
	return FReply::Handled().ReleaseMouseCapture();
}

FReply SAbyssVirtualJoystick::OnTouchStarted(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent)
{
	return HandleDown(MyGeometry, InTouchEvent);
}

FReply SAbyssVirtualJoystick::OnTouchMoved(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent)
{
	return HandleMove(MyGeometry, InTouchEvent);
}

FReply SAbyssVirtualJoystick::OnTouchEnded(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent)
{
	return HandleUp(InTouchEvent);
}

FReply SAbyssVirtualJoystick::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return HandleDown(MyGeometry, MouseEvent);
}

FReply SAbyssVirtualJoystick::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return HandleMove(MyGeometry, MouseEvent);
}

FReply SAbyssVirtualJoystick::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return HandleUp(MouseEvent);
}

void SAbyssVirtualJoystick::OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	Reset();
	SLeafWidget::OnMouseCaptureLost(CaptureLostEvent);
}

int32 SAbyssVirtualJoystick::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FLinearColor StyleTint = InWidgetStyle.GetColorAndOpacityTint();
	const float Presence = IsActive() ? 1.0f : 0.7f;
	auto Tinted = [&StyleTint, Presence](const FLinearColor& Color)
	{
		FLinearColor Result = Color * StyleTint;
		Result.A *= Presence;
		return Result;
	};
	const FVector2D Center = FVector2D(AllottedGeometry.GetLocalSize()) * 0.5;
	int32 Layer = LayerId;

	// base: ring then the translucent disc
	FSlateDrawElement::MakeBox(OutDrawElements, Layer, AbyssJoystick_Circle(AllottedGeometry, Center, BaseRadius + AbyssJoystick_RingWidth),
		&CircleBrush, ESlateDrawEffect::None, Tinted(RingColor));
	++Layer;
	FSlateDrawElement::MakeBox(OutDrawElements, Layer, AbyssJoystick_Circle(AllottedGeometry, Center, BaseRadius), &CircleBrush,
		ESlateDrawEffect::None, Tinted(BaseColor));
	++Layer;
	// inner guide ring at the dead zone
	FSlateDrawElement::MakeBox(OutDrawElements, Layer, AbyssJoystick_Circle(AllottedGeometry, Center, BaseRadius * 0.5f), &CircleBrush,
		ESlateDrawEffect::None, Tinted(FLinearColor(1.0f, 1.0f, 1.0f, 0.05f)));
	++Layer;
	// thumb
	const double ThumbRadius = BaseRadius * AbyssJoystick_ThumbRatio;
	const FVector2D ThumbCenter = Center + ThumbOffset;
	FSlateDrawElement::MakeBox(OutDrawElements, Layer, AbyssJoystick_Circle(AllottedGeometry, ThumbCenter, ThumbRadius + AbyssJoystick_RingWidth),
		&CircleBrush, ESlateDrawEffect::None, Tinted(RingColor));
	++Layer;
	FSlateDrawElement::MakeBox(OutDrawElements, Layer, AbyssJoystick_Circle(AllottedGeometry, ThumbCenter, ThumbRadius), &CircleBrush,
		ESlateDrawEffect::None, Tinted(ThumbColor));
	return Layer + 1;
}

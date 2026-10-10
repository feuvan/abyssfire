#include "Input/Touch/SAbyssTouchButton.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "Layout/Geometry.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"

#include "Input/Touch/AbyssTouchStyle.h"

namespace
{
	constexpr float AbyssTouchButton_PressedScale = 0.92f;
	constexpr float AbyssTouchButton_PopScale = 1.12f;
	constexpr double AbyssTouchButton_PopSeconds = 0.18;
	constexpr float AbyssTouchButton_OutlineWidth = 2.0f;
	constexpr float AbyssTouchButton_RectRadius = 6.0f;
	constexpr int32 AbyssTouchButton_ArcSegments = 48;

	FPaintGeometry AbyssTouchButton_Box(const FGeometry& Geometry, const FVector2D& Center, const FVector2D& Size)
	{
		const FVector2D TopLeft = Center - Size * 0.5;
		return Geometry.ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform(FVector2f(TopLeft)));
	}
}

void SAbyssTouchButton::Construct(const FArguments& InArgs)
{
	Shape = InArgs._Shape;
	DrawSize = InArgs._DrawSize;
	HitScale = FMath::Max(1.0f, InArgs._HitScale);
	FillColor = InArgs._FillColor;
	OutlineColor = InArgs._OutlineColor;
	BackgroundBrush = InArgs._BackgroundBrush;
	bCaptionBelowIcon = InArgs._CaptionBelowIcon;
	BaseFont = InArgs._Font.HasValidFont() ? InArgs._Font : AbyssTouchStyle::FallbackFont(14.0f);
	OnPressed = InArgs._OnPressed;
	LabelColor = AbyssTouchStyle::FromRgb(AbyssTouchStyle::Parchment);

	// Flat shape drawn with Slate's rounded-box shader: a circle (radius = half height) or a rounded square.
	ShapeBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	ShapeBrush.TintColor = FSlateColor(FLinearColor::White);
	ShapeBrush.OutlineSettings.Width = 0.0f;
	if (Shape == EAbyssTouchShape::Circle)
	{
		ShapeBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
	}
	else
	{
		ShapeBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		ShapeBrush.OutlineSettings.CornerRadii = FVector4(AbyssTouchButton_RectRadius, AbyssTouchButton_RectRadius,
			AbyssTouchButton_RectRadius, AbyssTouchButton_RectRadius);
	}

	// Animated (press / pop / cooldown): repaint every frame even under global invalidation.
	ForceVolatile(true);
}

void SAbyssTouchButton::SetCooldown(float InRemainingFraction, const FString& InText)
{
	CooldownFraction = FMath::Clamp(InRemainingFraction, 0.0f, 1.0f);
	CooldownText = InText;
}

void SAbyssTouchButton::PlayReadyPop()
{
	PopStartSeconds = FPlatformTime::Seconds();
}

FVector2D SAbyssTouchButton::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return DrawSize * HitScale;
}

float SAbyssTouchButton::CurrentScale() const
{
	float Scale = PressedPointerIndex != INDEX_NONE ? AbyssTouchButton_PressedScale : 1.0f;
	if (PopStartSeconds >= 0.0)
	{
		const double Elapsed = FPlatformTime::Seconds() - PopStartSeconds;
		if (Elapsed < AbyssTouchButton_PopSeconds)
		{
			const float T = static_cast<float>(Elapsed / AbyssTouchButton_PopSeconds);
			const float EaseOut = 1.0f - (1.0f - T) * (1.0f - T);
			Scale *= FMath::Lerp(AbyssTouchButton_PopScale, 1.0f, EaseOut);
		}
	}
	return Scale;
}

// =====================================================================================================================
// Input
// =====================================================================================================================

bool SAbyssTouchButton::IsInsideHitArea(const FGeometry& MyGeometry, const FVector2D& ScreenPosition) const
{
	const FVector2D Local = FVector2D(MyGeometry.AbsoluteToLocal(ScreenPosition));
	const FVector2D Size = FVector2D(MyGeometry.GetLocalSize());
	if (Shape == EAbyssTouchShape::Rect)
	{
		return Local.X >= 0.0 && Local.Y >= 0.0 && Local.X <= Size.X && Local.Y <= Size.Y;
	}
	const double HitRadius = DrawSize.X * 0.5 * HitScale;
	return FVector2D::DistSquared(Local, Size * 0.5) <= HitRadius * HitRadius;
}

FReply SAbyssTouchButton::HandlePointerDown(const FGeometry& MyGeometry, const FPointerEvent& Event)
{
	if (!Event.IsTouchEvent() && Event.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}
	if (!IsInsideHitArea(MyGeometry, FVector2D(Event.GetScreenSpacePosition())))
	{
		// Outside the circle: SAbyssTouchControls (an ancestor) gives it to the control whose hit area contains it, else
		// the press bubbles on to the world.
		return FReply::Unhandled();
	}
	return BeginForwardedPress(Event);
}

FReply SAbyssTouchButton::BeginForwardedPress(const FPointerEvent& Event)
{
	if (PressedPointerIndex != INDEX_NONE)
	{
		return FReply::Handled();     // a second finger on a held button: claim it, fire nothing
	}
	PressedPointerIndex = static_cast<int32>(Event.GetPointerIndex());
	OnPressed.ExecuteIfBound();       // fire on press (web: no hold / release semantics)
	return FReply::Handled().CaptureMouse(SharedThis(this));
}

FReply SAbyssTouchButton::HandlePointerUp(const FPointerEvent& Event)
{
	if (PressedPointerIndex == INDEX_NONE || static_cast<int32>(Event.GetPointerIndex()) != PressedPointerIndex)
	{
		return FReply::Unhandled();
	}
	PressedPointerIndex = INDEX_NONE;
	return FReply::Handled().ReleaseMouseCapture();
}

FReply SAbyssTouchButton::OnTouchStarted(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent)
{
	return HandlePointerDown(MyGeometry, InTouchEvent);
}

FReply SAbyssTouchButton::OnTouchEnded(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent)
{
	return HandlePointerUp(InTouchEvent);
}

FReply SAbyssTouchButton::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// Also reached by touches Slate falls back to mouse handling for; HandlePointerDown accepts both.
	return HandlePointerDown(MyGeometry, MouseEvent);
}

FReply SAbyssTouchButton::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return HandlePointerUp(MouseEvent);
}

FReply SAbyssTouchButton::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// A fast second tap is a second press (skills are tapped repeatedly).
	return HandlePointerDown(MyGeometry, MouseEvent);
}

void SAbyssTouchButton::OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	PressedPointerIndex = INDEX_NONE;
	SLeafWidget::OnMouseCaptureLost(CaptureLostEvent);
}

// =====================================================================================================================
// Paint
// =====================================================================================================================

void SAbyssTouchButton::DrawLines(FSlateWindowElementList& OutDrawElements, int32 LayerId, const FGeometry& AllottedGeometry,
	const FString& Text, const FSlateFontInfo& InFont, const FVector2D& Center, float MaxWidth, const FLinearColor& Color) const
{
	if (Text.IsEmpty() || !FSlateApplication::IsInitialized())
	{
		return;
	}
	TArray<FString> Lines;
	Text.ParseIntoArray(Lines, TEXT("\n"), /*InCullEmpty*/ true);
	if (Lines.Num() == 0)
	{
		return;
	}
	const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

	// Shrink to fit (web: captions "shrunk to fit").
	FSlateFontInfo Font = InFont;
	double Widest = 0.0;
	for (const FString& Line : Lines)
	{
		Widest = FMath::Max(Widest, static_cast<double>(FVector2D(FontMeasure->Measure(Line, Font)).X));
	}
	if (Widest > MaxWidth && Widest > 0.0)
	{
		Font.Size = FMath::Max(4.0f, static_cast<float>(Font.Size * MaxWidth / Widest));
	}

	TArray<FVector2D, TInlineAllocator<4>> Sizes;
	double TotalHeight = 0.0;
	for (const FString& Line : Lines)
	{
		const FVector2D LineSize(FontMeasure->Measure(Line, Font));
		Sizes.Add(LineSize);
		TotalHeight += LineSize.Y;
	}
	double Y = Center.Y - TotalHeight * 0.5;
	for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
	{
		const FVector2D& LineSize = Sizes[LineIndex];
		const FVector2D TopLeft(Center.X - LineSize.X * 0.5, Y);
		FSlateDrawElement::MakeText(OutDrawElements, LayerId,
			AllottedGeometry.ToPaintGeometry(FVector2f(LineSize), FSlateLayoutTransform(FVector2f(TopLeft))),
			Lines[LineIndex], Font, ESlateDrawEffect::None, Color);
		Y += LineSize.Y;
	}
}

int32 SAbyssTouchButton::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FLinearColor StyleTint = InWidgetStyle.GetColorAndOpacityTint();
	const float Opacity = DimAlpha * (bBlocked ? 0.45f : 1.0f);
	auto Tinted = [&StyleTint, Opacity](const FLinearColor& Color)
	{
		FLinearColor Result = Color * StyleTint;
		Result.A *= Opacity;
		return Result;
	};

	const FVector2D LocalSize = FVector2D(AllottedGeometry.GetLocalSize());
	const FVector2D Center = LocalSize * 0.5;
	const FVector2D Size = DrawSize * CurrentScale();
	const double Radius = Size.Y * 0.5;
	int32 Layer = LayerId;

	// ---- frame: outline (bright when active), face ----
	const FLinearColor Outline = bActive ? AbyssTouchStyle::FromRgb(AbyssTouchStyle::GoldBright) : OutlineColor;
	const float OutlineWidth = bActive ? AbyssTouchButton_OutlineWidth * 1.75f : AbyssTouchButton_OutlineWidth;
	FSlateDrawElement::MakeBox(OutDrawElements, Layer,
		AbyssTouchButton_Box(AllottedGeometry, Center, Size + FVector2D(OutlineWidth * 2.0f, OutlineWidth * 2.0f)), &ShapeBrush,
		ESlateDrawEffect::None, Tinted(Outline));
	++Layer;
	if (BackgroundBrush != nullptr)
	{
		FSlateDrawElement::MakeBox(OutDrawElements, Layer, AbyssTouchButton_Box(AllottedGeometry, Center, Size), BackgroundBrush,
			ESlateDrawEffect::None, Tinted(FLinearColor(FillColor.R, FillColor.G, FillColor.B, 1.0f) * 1.6f));
	}
	else
	{
		FSlateDrawElement::MakeBox(OutDrawElements, Layer, AbyssTouchButton_Box(AllottedGeometry, Center, Size), &ShapeBrush,
			ESlateDrawEffect::None, Tinted(FillColor));
		// soft top highlight (medallion bevel)
		const FVector2D Highlight(Size.X * 0.72, Size.Y * 0.42);
		FSlateDrawElement::MakeBox(OutDrawElements, Layer,
			AbyssTouchButton_Box(AllottedGeometry, Center - FVector2D(0.0, Size.Y * 0.2), Highlight), &ShapeBrush,
			ESlateDrawEffect::None, Tinted(FLinearColor(1.0f, 1.0f, 1.0f, 0.06f)));
	}
	++Layer;

	// ---- icon / label ----
	const bool bCaption = bCaptionBelowIcon && Icon != nullptr && !Label.IsEmpty();
	if (Icon != nullptr)
	{
		const double IconSize = (bCaption ? 0.56 : 0.66) * FMath::Min(Size.X, Size.Y);
		const FVector2D IconCenter = bCaption ? Center - FVector2D(0.0, Size.Y * 0.12) : Center;
		FSlateDrawElement::MakeBox(OutDrawElements, Layer, AbyssTouchButton_Box(AllottedGeometry, IconCenter, FVector2D(IconSize, IconSize)),
			Icon, ESlateDrawEffect::None, Tinted(FLinearColor::White));
		++Layer;
	}
	if (!Label.IsEmpty() && (Icon == nullptr || bCaption))
	{
		const float LabelPx = bCaption ? static_cast<float>(Size.Y * 0.22) : static_cast<float>(FMath::Clamp(Size.Y * 0.26, 9.0, 26.0));
		const FVector2D LabelCenter = bCaption ? FVector2D(Center.X, Center.Y + Size.Y * 0.32) : Center;
		DrawLines(OutDrawElements, Layer, AllottedGeometry, Label, AbyssTouchStyle::Resized(BaseFont, LabelPx), LabelCenter,
			static_cast<float>(Size.X * 0.86), Tinted(LabelColor));
		++Layer;
	}

	// ---- cooldown / channel: dark disc + gold arc of the remaining fraction from 12 o'clock, countdown ----
	if (CooldownFraction > 0.0f)
	{
		FSlateDrawElement::MakeBox(OutDrawElements, Layer, AbyssTouchButton_Box(AllottedGeometry, Center, Size), &ShapeBrush,
			ESlateDrawEffect::None, Tinted(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f)));
		++Layer;
		const int32 Segments = FMath::Max(2, FMath::CeilToInt(AbyssTouchButton_ArcSegments * CooldownFraction));
		const double ArcRadius = FMath::Max(2.0, Radius - 3.0);
		TArray<FVector2f> Points;
		Points.Reserve(Segments + 1);
		for (int32 PointIndex = 0; PointIndex <= Segments; ++PointIndex)
		{
			const double Theta = (static_cast<double>(PointIndex) / Segments) * CooldownFraction * UE_DOUBLE_TWO_PI;
			Points.Add(FVector2f(FVector2D(Center.X + ArcRadius * FMath::Sin(Theta), Center.Y - ArcRadius * FMath::Cos(Theta))));
		}
		FSlateDrawElement::MakeLines(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(), MoveTemp(Points),
			ESlateDrawEffect::None, Tinted(AbyssTouchStyle::FromRgb(AbyssTouchStyle::GoldBright, 0.9f)), /*bAntialias*/ true,
			FMath::Max(2.0f, static_cast<float>(Radius * 0.12)));
		++Layer;
		if (!CooldownText.IsEmpty())
		{
			DrawLines(OutDrawElements, Layer, AllottedGeometry, CooldownText,
				AbyssTouchStyle::Resized(BaseFont, static_cast<float>(FMath::Clamp(Size.Y * 0.36, 10.0, 30.0))), Center,
				static_cast<float>(Size.X * 0.9), Tinted(FLinearColor::White));
			++Layer;
		}
	}

	// ---- badge (potion count), bottom-right ----
	if (!Badge.IsEmpty())
	{
		const float BadgePx = static_cast<float>(FMath::Clamp(Size.Y * 0.3, 9.0, 22.0));
		const FVector2D BadgeCenter = Center + FVector2D(Size.X * 0.34, Size.Y * 0.34);
		DrawLines(OutDrawElements, Layer, AllottedGeometry, Badge, AbyssTouchStyle::Resized(BaseFont, BadgePx), BadgeCenter,
			static_cast<float>(Size.X * 0.6), Tinted(AbyssTouchStyle::FromRgb(AbyssTouchStyle::Parchment)));
		++Layer;
	}
	return Layer;
}

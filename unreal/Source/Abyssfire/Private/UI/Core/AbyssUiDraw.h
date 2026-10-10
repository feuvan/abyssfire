// Paint helpers for the custom-drawn UI widgets (UiKit frames, orbs, bars, cooldown sweeps, minimap markers, floating
// text). Everything is expressed in the widget's local Slate units; colours are linear and get multiplied by the widget
// style tint (fades of parent widgets).
//
// Primitives: boxes (flat / rounded / outlined via cached brushes), lines, text, strip gradients (stacked boxes, no
// gradient element so the orientation semantics never matter), and filled convex polygons / pies / circle segments
// through FSlateDrawElement::MakeCustomVerts with the style's texture-backed white brush (FAbyssUiStyle::SolidTexture;
// the only custom-vertex use; see AbyssUiDraw.cpp).
#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Layout/Geometry.h"
#include "Rendering/DrawElements.h"
#include "Styling/WidgetStyle.h"

class FAbyssUiStyle;

/** One paint pass: geometry, element list, layer and tint shared by every call. */
struct FAbyssPainter
{
	FAbyssPainter(const FGeometry& InGeometry, FSlateWindowElementList& InElements, int32 InLayer, const FWidgetStyle& InWidgetStyle,
		const FAbyssUiStyle& InStyle);

	const FGeometry& Geometry;
	FSlateWindowElementList& Elements;
	int32 Layer;
	FLinearColor Tint;
	const FAbyssUiStyle& Style;

	/** Next layer (for content that must draw above what was painted so far). */
	int32 NextLayer() { return ++Layer; }

	FLinearColor Tinted(const FLinearColor& Color) const;

	// ---- boxes ----
	void Box(const FVector2D& Pos, const FVector2D& Size, const FLinearColor& Color) const;
	void Brush(const FVector2D& Pos, const FVector2D& Size, const FSlateBrush* Brush, const FLinearColor& Color = FLinearColor::White) const;
	void RoundBox(const FVector2D& Pos, const FVector2D& Size, const FLinearColor& Fill, float Radius,
		const FLinearColor& Outline = FLinearColor::Transparent, float OutlineWidth = 0.f) const;
	void Circle(const FVector2D& Center, float Radius, const FLinearColor& Fill, const FLinearColor& Outline = FLinearColor::Transparent,
		float OutlineWidth = 0.f) const;
	void Ring(const FVector2D& Center, float Radius, const FLinearColor& Color, float Width) const;
	/** Rotated square (diamond) centred at Center, edge = Size. */
	void Diamond(const FVector2D& Center, float Size, const FLinearColor& Color) const;
	/** Vertical gradient as Steps stacked strips (Top -> Mid at MidAt -> Bottom). */
	void VerticalGradient(const FVector2D& Pos, const FVector2D& Size, const FLinearColor& Top, const FLinearColor& Bottom,
		int32 Steps = 16) const;
	void VerticalGradient3(const FVector2D& Pos, const FVector2D& Size, const FLinearColor& Top, const FLinearColor& Mid, float MidAt,
		const FLinearColor& Bottom, int32 Steps = 18) const;
	/** Soft radial glow fading from Inner (centre) to transparent at Radius (a gradient mesh; Rings = density hint). */
	void Glow(const FVector2D& Center, float Radius, const FLinearColor& Inner, int32 Rings = 8) const;

	// ---- lines ----
	void Line(const FVector2D& From, const FVector2D& To, const FLinearColor& Color, float Thickness = 1.f) const;
	void Polyline(const TArray<FVector2D>& Points, const FLinearColor& Color, float Thickness = 1.f) const;
	void CircleOutline(const FVector2D& Center, float Radius, const FLinearColor& Color, float Thickness = 1.f, int32 Segments = 40) const;

	// ---- filled polygons (custom vertices) ----
	/** Polygon that is convex or star-shaped around its first point (triangle fan from point 0). */
	void ConvexPolygon(const TArray<FVector2D>& Points, const FLinearColor& Color) const;
	/** Pie from angle A0 to A1 (radians, 0 = right, clockwise on screen), radius R. */
	void Pie(const FVector2D& Center, float Radius, float A0, float A1, const FLinearColor& Color) const;
	/** Pie clipped to the square of half-size HalfSize around Center (skill-slot cooldown sweep). */
	void SquarePie(const FVector2D& Center, float HalfSize, float A0, float A1, const FLinearColor& Color) const;
	/** The part of a circle below the line y = Center.Y + Radius - Level * 2 * Radius, with a sine wave of Amplitude. */
	void CircleLiquid(const FVector2D& Center, float Radius, float Level, float WavePhase, float Amplitude, const FLinearColor& Color) const;
	/** Five-point star. */
	void Star(const FVector2D& Center, float Outer, float Inner, const FLinearColor& Color) const;
	/** Textured quad: four corners (any orientation) with their UVs, e.g. the rotated minimap. */
	void TexturedQuad(const FSlateBrush* Brush, const FVector2D (&Corners)[4], const FVector2D (&UVs)[4],
		const FLinearColor& Color = FLinearColor::White) const;
	/**
	 * Radial gradient between two ellipses (radii per axis): Inner on the inner ellipse, Outer on the outer one. Zero
	 * InnerRadii = a disc from the centre (CSS createRadialGradient stops, story mood backdrops and vignettes).
	 */
	void RadialGradient(const FVector2D& Center, const FVector2D& InnerRadii, const FVector2D& OuterRadii, const FLinearColor& Inner,
		const FLinearColor& Outer, int32 Segments = 48) const;
	/** Textured disc: the brush's UV circle (UVCenter, UVRadius) on a circle of Radius (round portrait medallions). */
	void TexturedCircle(const FSlateBrush* Brush, const FVector2D& Center, float Radius, const FVector2D& UVCenter, float UVRadius,
		const FLinearColor& Color = FLinearColor::White, int32 Segments = 48) const;
	/** Quad with per-corner colours (TL, TR, BR, BL). */
	void ColoredQuad(const FVector2D& Pos, const FVector2D& Size, const FLinearColor& TL, const FLinearColor& TR, const FLinearColor& BR,
		const FLinearColor& BL) const;

	// ---- text ----
	static FVector2D Measure(const FString& Text, const FSlateFontInfo& Font);
	void Text(const FVector2D& Pos, const FString& Text, const FSlateFontInfo& Font, const FLinearColor& Color) const;
	/** Text centred on Center (both axes); Scale scales around the centre. */
	void TextCentered(const FVector2D& Center, const FString& Text, const FSlateFontInfo& Font, const FLinearColor& Color,
		float Scale = 1.f) const;
	void TextShadowed(const FVector2D& Pos, const FString& Text, const FSlateFontInfo& Font, const FLinearColor& Color,
		const FVector2D& ShadowOffset = FVector2D(1.0, 1.0), const FLinearColor& Shadow = FLinearColor(0.f, 0.f, 0.f, 0.8f)) const;

	// ---- UiKit compositions ----
	/** save-ui-input 8.3 frames: Kind 0 = panel, 1 = tooltip, 2 = plate. Accent = hairline colour; HeaderHeight 0 = none. */
	void Frame(const FVector2D& Pos, const FVector2D& Size, int32 Kind, const FLinearColor& Accent, float HeaderHeight = 0.f,
		float BodyAlpha = 1.f) const;
	/** 8.5 card: fill, optional glow, optional left strip, 1 px border. */
	void Card(const FVector2D& Pos, const FVector2D& Size, const FLinearColor& Fill, const FLinearColor& Border,
		const FLinearColor& Strip = FLinearColor::Transparent, const FLinearColor& Glow = FLinearColor::Transparent) const;
	/** 8.5 well: recessed dark box. */
	void Well(const FVector2D& Pos, const FVector2D& Size, float Radius = 4.f) const;
	/** 8.5 bar: trough + fill (Fraction 0..1) with a top highlight. */
	void Bar(const FVector2D& Pos, const FVector2D& Size, float Fraction, const FLinearColor& Fill, bool bTicks = false) const;
	/** 8.5 divider: gold rule with an optional centre diamond. */
	void Divider(const FVector2D& Center, float Width, bool bDiamond = true, const FLinearColor& Color = FLinearColor::Transparent) const;

private:
	void CustomVerts(const TArray<FVector2D>& LocalPoints, const TArray<FLinearColor>& Colors, const TArray<uint32>& Indices,
		const FSlateBrush* TextureBrush = nullptr, const TArray<FVector2D>* UVs = nullptr) const;
};

/** Phaser easing (ue58-platform.md 9.1: FCurveSequence lacks Back). T in [0, 1]. */
namespace AbyssEase
{
	inline float Linear(float T) { return T; }
	inline float QuadOut(float T) { return 1.f - (1.f - T) * (1.f - T); }
	inline float QuadIn(float T) { return T * T; }
	inline float QuadInOut(float T) { return T < 0.5f ? 2.f * T * T : 1.f - FMath::Pow(-2.f * T + 2.f, 2.f) * 0.5f; }
	inline float CubicOut(float T) { const float U = 1.f - T; return 1.f - U * U * U; }
	inline float CubicIn(float T) { return T * T * T; }
	inline float SineOut(float T) { return FMath::Sin(T * UE_HALF_PI); }
	inline float SineInOut(float T) { return -(FMath::Cos(UE_PI * T) - 1.f) * 0.5f; }
	inline float BackOut(float T)
	{
		constexpr float S = 1.70158f;
		const float U = T - 1.f;
		return 1.f + (S + 1.f) * U * U * U + S * U * U;
	}
	inline float Clamp01(float T) { return FMath::Clamp(T, 0.f, 1.f); }
	/** Progress of [Start, Start + Duration] at Now (clamped 0..1). */
	inline float Progress(double Now, double Start, double Duration)
	{
		return Duration <= 0.0 ? 1.f : Clamp01(static_cast<float>((Now - Start) / Duration));
	}
	/** dt-correct exponential approach (web per-frame lerp `x += (t - x) * k` at 60 fps). */
	inline float Approach(float Current, float Target, float PerFrameFactor, float DeltaSeconds)
	{
		const float Keep = FMath::Pow(1.f - PerFrameFactor, DeltaSeconds * 60.f);
		return Target + (Current - Target) * Keep;
	}
}

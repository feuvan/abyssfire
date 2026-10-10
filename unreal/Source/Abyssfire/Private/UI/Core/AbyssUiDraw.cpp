#include "UI/Core/AbyssUiDraw.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/RenderingCommon.h"
#include "Rendering/SlateRenderer.h"

#include "UI/AbyssUiStyle.h"

namespace
{
	FORCEINLINE FPaintGeometry AbyssDraw_Paint(const FGeometry& Geometry, const FVector2D& Pos, const FVector2D& Size, float Scale = 1.f)
	{
		return Geometry.ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform(Scale, FVector2f(Pos)));
	}
}

FAbyssPainter::FAbyssPainter(const FGeometry& InGeometry, FSlateWindowElementList& InElements, int32 InLayer,
	const FWidgetStyle& InWidgetStyle, const FAbyssUiStyle& InStyle)
	: Geometry(InGeometry)
	, Elements(InElements)
	, Layer(InLayer)
	, Tint(InWidgetStyle.GetColorAndOpacityTint())
	, Style(InStyle)
{
}

FLinearColor FAbyssPainter::Tinted(const FLinearColor& Color) const
{
	return Color * Tint;
}

// =====================================================================================================================
// Boxes
// =====================================================================================================================

void FAbyssPainter::Box(const FVector2D& Pos, const FVector2D& Size, const FLinearColor& Color) const
{
	if (Size.X <= 0.0 || Size.Y <= 0.0 || Color.A <= 0.f)
	{
		return;
	}
	FSlateDrawElement::MakeBox(Elements, Layer, AbyssDraw_Paint(Geometry, Pos, Size), Style.White(), ESlateDrawEffect::None, Tinted(Color));
}

void FAbyssPainter::Brush(const FVector2D& Pos, const FVector2D& Size, const FSlateBrush* InBrush, const FLinearColor& Color) const
{
	if (InBrush == nullptr || Size.X <= 0.0 || Size.Y <= 0.0 || Color.A <= 0.f)
	{
		return;
	}
	FSlateDrawElement::MakeBox(Elements, Layer, AbyssDraw_Paint(Geometry, Pos, Size), InBrush, ESlateDrawEffect::None, Tinted(Color));
}

void FAbyssPainter::RoundBox(const FVector2D& Pos, const FVector2D& Size, const FLinearColor& Fill, float Radius,
	const FLinearColor& Outline, float OutlineWidth) const
{
	if (Size.X <= 0.0 || Size.Y <= 0.0)
	{
		return;
	}
	const FLinearColor TintedOutline = Tinted(Outline);
	// The brush carries the colours (an outline is not reliably tinted by the element tint): bake the tint in.
	const FSlateBrush* RoundBrush = Style.Rounded(Tinted(Fill), Radius, TintedOutline, TintedOutline.A > 0.f ? OutlineWidth : 0.f);
	FSlateDrawElement::MakeBox(Elements, Layer, AbyssDraw_Paint(Geometry, Pos, Size), RoundBrush, ESlateDrawEffect::None, FLinearColor::White);
}

void FAbyssPainter::Circle(const FVector2D& Center, float Radius, const FLinearColor& Fill, const FLinearColor& Outline, float OutlineWidth) const
{
	RoundBox(Center - FVector2D(Radius, Radius), FVector2D(Radius * 2.f, Radius * 2.f), Fill, Radius, Outline, OutlineWidth);
}

void FAbyssPainter::Ring(const FVector2D& Center, float Radius, const FLinearColor& Color, float Width) const
{
	RoundBox(Center - FVector2D(Radius, Radius), FVector2D(Radius * 2.f, Radius * 2.f), FLinearColor::Transparent, Radius, Color, Width);
}

void FAbyssPainter::Diamond(const FVector2D& Center, float Size, const FLinearColor& Color) const
{
	const float H = Size * 0.70710678f;
	TArray<FVector2D> Points;
	Points.Add(Center + FVector2D(0.0, -H));
	Points.Add(Center + FVector2D(H, 0.0));
	Points.Add(Center + FVector2D(0.0, H));
	Points.Add(Center + FVector2D(-H, 0.0));
	ConvexPolygon(Points, Color);
}

void FAbyssPainter::VerticalGradient(const FVector2D& Pos, const FVector2D& Size, const FLinearColor& Top, const FLinearColor& Bottom,
	int32 Steps) const
{
	if (Size.Y <= 0.0 || Size.X <= 0.0)
	{
		return;
	}
	Steps = FMath::Clamp(Steps, 1, 64);
	const double StripH = Size.Y / Steps;
	for (int32 Index = 0; Index < Steps; ++Index)
	{
		const float T = Steps == 1 ? 0.f : static_cast<float>(Index) / static_cast<float>(Steps - 1);
		const double Y = Pos.Y + StripH * Index;
		// +0.5 overlap hides seams between strips at fractional DPI scales.
		Box(FVector2D(Pos.X, Y), FVector2D(Size.X, StripH + 0.5), FMath::Lerp(Top, Bottom, T));
	}
}

void FAbyssPainter::VerticalGradient3(const FVector2D& Pos, const FVector2D& Size, const FLinearColor& Top, const FLinearColor& Mid,
	float MidAt, const FLinearColor& Bottom, int32 Steps) const
{
	MidAt = FMath::Clamp(MidAt, 0.05f, 0.95f);
	const int32 TopSteps = FMath::Max(1, FMath::RoundToInt(Steps * MidAt));
	const int32 BottomSteps = FMath::Max(1, Steps - TopSteps);
	const double SplitY = Size.Y * MidAt;
	VerticalGradient(Pos, FVector2D(Size.X, SplitY), Top, Mid, TopSteps);
	VerticalGradient(FVector2D(Pos.X, Pos.Y + SplitY), FVector2D(Size.X, Size.Y - SplitY), Mid, Bottom, BottomSteps);
}

void FAbyssPainter::Glow(const FVector2D& Center, float Radius, const FLinearColor& Inner, int32 Rings) const
{
	if (Radius <= 0.f || Inner.A <= 0.f)
	{
		return;
	}
	// One radial gradient mesh (Inner at the centre -> transparent at Radius), the look of Rings stacked translucent
	// circles without one cached rounded brush per animated radius / alpha. Rings sets the mesh density.
	const int32 Segments = FMath::Clamp(FMath::Max(Rings * 4, FMath::CeilToInt(Radius * 0.5f)), 16, 96);
	RadialGradient(Center, FVector2D::ZeroVector, FVector2D(Radius, Radius), Inner, FLinearColor(Inner.R, Inner.G, Inner.B, 0.f), Segments);
}

// =====================================================================================================================
// Lines
// =====================================================================================================================

void FAbyssPainter::Line(const FVector2D& From, const FVector2D& To, const FLinearColor& Color, float Thickness) const
{
	TArray<FVector2f> Points;
	Points.Add(FVector2f(From));
	Points.Add(FVector2f(To));
	FSlateDrawElement::MakeLines(Elements, Layer, Geometry.ToPaintGeometry(), MoveTemp(Points), ESlateDrawEffect::None, Tinted(Color), true,
		Thickness);
}

void FAbyssPainter::Polyline(const TArray<FVector2D>& InPoints, const FLinearColor& Color, float Thickness) const
{
	if (InPoints.Num() < 2)
	{
		return;
	}
	TArray<FVector2f> Points;
	Points.Reserve(InPoints.Num());
	for (const FVector2D& Point : InPoints)
	{
		Points.Add(FVector2f(Point));
	}
	FSlateDrawElement::MakeLines(Elements, Layer, Geometry.ToPaintGeometry(), MoveTemp(Points), ESlateDrawEffect::None, Tinted(Color), true,
		Thickness);
}

void FAbyssPainter::CircleOutline(const FVector2D& Center, float Radius, const FLinearColor& Color, float Thickness, int32 Segments) const
{
	Segments = FMath::Clamp(Segments, 8, 128);
	TArray<FVector2D> Points;
	Points.Reserve(Segments + 1);
	for (int32 Index = 0; Index <= Segments; ++Index)
	{
		const float A = UE_TWO_PI * static_cast<float>(Index) / static_cast<float>(Segments);
		Points.Add(Center + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Radius);
	}
	Polyline(Points, Color, Thickness);
}

// =====================================================================================================================
// Filled polygons (custom vertices)
// =====================================================================================================================

void FAbyssPainter::RadialGradient(const FVector2D& Center, const FVector2D& InnerRadii, const FVector2D& OuterRadii,
	const FLinearColor& Inner, const FLinearColor& Outer, int32 Segments) const
{
	if ((Inner.A <= 0.f && Outer.A <= 0.f) || OuterRadii.X <= 0.0 || OuterRadii.Y <= 0.0)
	{
		return;
	}
	const int32 Count = FMath::Clamp(Segments, 8, 128);
	TArray<FVector2D> Points;
	TArray<FLinearColor> Colors;
	TArray<uint32> Indices;
	const bool bDisc = InnerRadii.X <= 0.0 || InnerRadii.Y <= 0.0;
	if (bDisc)
	{
		Points.Reserve(Count + 1);
		Colors.Reserve(Count + 1);
		Points.Add(Center);
		Colors.Add(Inner);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const double Angle = UE_DOUBLE_TWO_PI * static_cast<double>(Index) / static_cast<double>(Count);
			Points.Add(Center + FVector2D(FMath::Cos(Angle) * OuterRadii.X, FMath::Sin(Angle) * OuterRadii.Y));
			Colors.Add(Outer);
		}
		Indices.Reserve(Count * 3);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Indices.Add(0u);
			Indices.Add(static_cast<uint32>(1 + Index));
			Indices.Add(static_cast<uint32>(1 + (Index + 1) % Count));
		}
	}
	else
	{
		Points.Reserve(Count * 2);
		Colors.Reserve(Count * 2);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const double Angle = UE_DOUBLE_TWO_PI * static_cast<double>(Index) / static_cast<double>(Count);
			const FVector2D Dir(FMath::Cos(Angle), FMath::Sin(Angle));
			Points.Add(Center + FVector2D(Dir.X * InnerRadii.X, Dir.Y * InnerRadii.Y));
			Colors.Add(Inner);
			Points.Add(Center + FVector2D(Dir.X * OuterRadii.X, Dir.Y * OuterRadii.Y));
			Colors.Add(Outer);
		}
		Indices.Reserve(Count * 6);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const uint32 A = static_cast<uint32>(Index * 2);
			const uint32 B = A + 1u;
			const uint32 C = static_cast<uint32>(((Index + 1) % Count) * 2);
			const uint32 D = C + 1u;
			Indices.Append({ A, B, D, A, D, C });
		}
	}
	CustomVerts(Points, Colors, Indices);
}

void FAbyssPainter::TexturedCircle(const FSlateBrush* InBrush, const FVector2D& Center, float Radius, const FVector2D& UVCenter,
	float UVRadius, const FLinearColor& Color, int32 Segments) const
{
	if (InBrush == nullptr || Radius <= 0.f || Color.A <= 0.f)
	{
		return;
	}
	const int32 Count = FMath::Clamp(Segments, 8, 128);
	TArray<FVector2D> Points;
	TArray<FVector2D> UVs;
	Points.Reserve(Count + 1);
	UVs.Reserve(Count + 1);
	Points.Add(Center);
	UVs.Add(UVCenter);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const double Angle = UE_DOUBLE_TWO_PI * static_cast<double>(Index) / static_cast<double>(Count);
		const FVector2D Dir(FMath::Cos(Angle), FMath::Sin(Angle));
		Points.Add(Center + Dir * static_cast<double>(Radius));
		UVs.Add(UVCenter + Dir * static_cast<double>(UVRadius));
	}
	TArray<FLinearColor> Colors;
	Colors.Add(Color);
	TArray<uint32> Indices;
	Indices.Reserve(Count * 3);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Indices.Add(0u);
		Indices.Add(static_cast<uint32>(1 + Index));
		Indices.Add(static_cast<uint32>(1 + (Index + 1) % Count));
	}
	CustomVerts(Points, Colors, Indices, InBrush, &UVs);
}

void FAbyssPainter::CustomVerts(const TArray<FVector2D>& LocalPoints, const TArray<FLinearColor>& Colors, const TArray<uint32>& Indices,
	const FSlateBrush* TextureBrush, const TArray<FVector2D>* UVs) const
{
	if (LocalPoints.Num() < 3 || Indices.Num() < 3 || !FSlateApplication::IsInitialized())
	{
		return;
	}
	FSlateRenderer* Renderer = FSlateApplication::Get().GetRenderer();
	if (Renderer == nullptr)
	{
		return;
	}
	// The white brush (vertex colours carry the shape colour) or a texture brush (UVs given). The handle stays valid while
	// the brush lives (style cache / icon cache).
	const FSlateBrush& SourceBrush = TextureBrush != nullptr ? *TextureBrush : *Style.White();
	const FSlateResourceHandle Handle = Renderer->GetResourceHandle(SourceBrush, FVector2f::ZeroVector, 1.0f);
	if (!Handle.IsValid())
	{
		return;
	}
	const FSlateRenderTransform& Transform = Geometry.GetAccumulatedRenderTransform();

	TArray<FSlateVertex> Vertices;
	Vertices.Reserve(LocalPoints.Num());
	for (int32 Index = 0; Index < LocalPoints.Num(); ++Index)
	{
		FSlateVertex Vertex;
		FMemory::Memzero(Vertex);
		// Custom vertices are in window space: the widget's accumulated render transform maps local -> window.
		Vertex.Position = Transform.TransformPoint(FVector2f(LocalPoints[Index]));
		const FVector2D UV = UVs != nullptr && UVs->IsValidIndex(Index) ? (*UVs)[Index] : FVector2D(0.5, 0.5);
		Vertex.TexCoords = FVector4f(static_cast<float>(UV.X), static_cast<float>(UV.Y), 1.f, 1.f);
		Vertex.MaterialTexCoords = FVector2f(UV);
		const FLinearColor Color = Tinted(Colors.IsValidIndex(Index) ? Colors[Index] : Colors.Last());
		Vertex.Color = Color.ToFColor(true);
		Vertices.Add(Vertex);
	}
	TArray<SlateIndex> SlateIndices;
	SlateIndices.Reserve(Indices.Num());
	for (const uint32 Index : Indices)
	{
		SlateIndices.Add(static_cast<SlateIndex>(Index));
	}
	FSlateDrawElement::MakeCustomVerts(Elements, Layer, Handle, Vertices, SlateIndices, nullptr, 0, 0);
}

void FAbyssPainter::ConvexPolygon(const TArray<FVector2D>& Points, const FLinearColor& Color) const
{
	if (Points.Num() < 3 || Color.A <= 0.f)
	{
		return;
	}
	TArray<FLinearColor> Colors;
	Colors.Add(Color);
	TArray<uint32> Indices;
	Indices.Reserve((Points.Num() - 2) * 3);
	for (int32 Index = 1; Index + 1 < Points.Num(); ++Index)
	{
		Indices.Add(0);
		Indices.Add(static_cast<uint32>(Index));
		Indices.Add(static_cast<uint32>(Index + 1));
	}
	CustomVerts(Points, Colors, Indices);
}

void FAbyssPainter::Pie(const FVector2D& Center, float Radius, float A0, float A1, const FLinearColor& Color) const
{
	if (A1 <= A0 || Radius <= 0.f || Color.A <= 0.f)
	{
		return;
	}
	const int32 Segments = FMath::Clamp(FMath::CeilToInt((A1 - A0) / FMath::DegreesToRadians(4.f)), 1, 120);
	TArray<FVector2D> Points;
	Points.Reserve(Segments + 2);
	Points.Add(Center);
	for (int32 Index = 0; Index <= Segments; ++Index)
	{
		const float A = FMath::Lerp(A0, A1, static_cast<float>(Index) / static_cast<float>(Segments));
		Points.Add(Center + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Radius);
	}
	// A pie is star-shaped around its centre (point 0): the fan triangulates it for any span.
	ConvexPolygon(Points, Color);
}

void FAbyssPainter::SquarePie(const FVector2D& Center, float HalfSize, float A0, float A1, const FLinearColor& Color) const
{
	if (A1 <= A0 || HalfSize <= 0.f || Color.A <= 0.f)
	{
		return;
	}
	// Sample angles every 4 degrees plus the square's corners so the sweep follows the square exactly.
	TArray<float> Angles;
	const float Step = FMath::DegreesToRadians(4.f);
	for (float A = A0; A < A1; A += Step)
	{
		Angles.Add(A);
	}
	Angles.Add(A1);
	for (int32 Turn = -2; Turn <= 4; ++Turn)
	{
		for (int32 Corner = 0; Corner < 4; ++Corner)
		{
			const float A = UE_HALF_PI * Corner + UE_PI * 0.25f + UE_TWO_PI * Turn;
			if (A > A0 && A < A1)
			{
				Angles.Add(A);
			}
		}
	}
	Angles.Sort();
	TArray<FVector2D> Points;
	Points.Reserve(Angles.Num() + 1);
	Points.Add(Center);
	for (const float A : Angles)
	{
		const float C = FMath::Cos(A);
		const float S = FMath::Sin(A);
		const float Extent = HalfSize / FMath::Max(FMath::Max(FMath::Abs(C), FMath::Abs(S)), 1e-3f);
		Points.Add(Center + FVector2D(C, S) * Extent);
	}
	ConvexPolygon(Points, Color);
}

void FAbyssPainter::CircleLiquid(const FVector2D& Center, float Radius, float Level, float WavePhase, float Amplitude,
	const FLinearColor& Color) const
{
	Level = FMath::Clamp(Level, 0.f, 1.f);
	if (Level <= 0.f || Radius <= 0.f || Color.A <= 0.f)
	{
		return;
	}
	const float SurfaceY = Radius - Level * 2.f * Radius;   // relative to the centre, y down
	const int32 Columns = FMath::Clamp(FMath::CeilToInt(Radius), 8, 64);
	const float ColumnW = 2.f * Radius / Columns;
	TArray<FVector2D> Points;
	TArray<uint32> Indices;
	Points.Reserve(Columns * 4);
	Indices.Reserve(Columns * 6);
	const auto TopAt = [&](float X)
	{
		const float Half = FMath::Sqrt(FMath::Max(0.f, Radius * Radius - X * X));
		const float Wave = SurfaceY + Amplitude * FMath::Sin(X * 0.18f + WavePhase);
		return FMath::Clamp(Wave, -Half, Half);
	};
	const auto BottomAt = [&](float X) { return FMath::Sqrt(FMath::Max(0.f, Radius * Radius - X * X)); };
	for (int32 Column = 0; Column < Columns; ++Column)
	{
		const float X0 = -Radius + Column * ColumnW;
		const float X1 = X0 + ColumnW;
		const float T0 = TopAt(X0);
		const float T1 = TopAt(X1);
		const float B0 = BottomAt(X0);
		const float B1 = BottomAt(X1);
		if (T0 >= B0 && T1 >= B1)
		{
			continue;
		}
		const uint32 Base = static_cast<uint32>(Points.Num());
		Points.Add(Center + FVector2D(X0, T0));
		Points.Add(Center + FVector2D(X1, T1));
		Points.Add(Center + FVector2D(X1, FMath::Max(T1, B1)));
		Points.Add(Center + FVector2D(X0, FMath::Max(T0, B0)));
		Indices.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 3 });
	}
	TArray<FLinearColor> Colors;
	Colors.Add(Color);
	CustomVerts(Points, Colors, Indices);
}

void FAbyssPainter::Star(const FVector2D& Center, float Outer, float Inner, const FLinearColor& Color) const
{
	TArray<FVector2D> Points;
	Points.Add(Center);
	for (int32 Index = 0; Index <= 10; ++Index)
	{
		const float A = -UE_HALF_PI + UE_PI * 0.2f * Index;
		const float R = (Index % 2 == 0) ? Outer : Inner;
		Points.Add(Center + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R);
	}
	// A star is star-shaped around its centre (point 0): a fan from the centre fills it exactly.
	ConvexPolygon(Points, Color);
}

void FAbyssPainter::TexturedQuad(const FSlateBrush* InBrush, const FVector2D (&Corners)[4], const FVector2D (&InUVs)[4],
	const FLinearColor& Color) const
{
	if (InBrush == nullptr || Color.A <= 0.f)
	{
		return;
	}
	TArray<FVector2D> Points;
	TArray<FVector2D> UVs;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Points.Add(Corners[Index]);
		UVs.Add(InUVs[Index]);
	}
	TArray<FLinearColor> Colors;
	Colors.Add(Color);
	TArray<uint32> Indices;
	Indices.Append({ 0u, 1u, 2u, 0u, 2u, 3u });
	CustomVerts(Points, Colors, Indices, InBrush, &UVs);
}

void FAbyssPainter::ColoredQuad(const FVector2D& Pos, const FVector2D& Size, const FLinearColor& TL, const FLinearColor& TR,
	const FLinearColor& BR, const FLinearColor& BL) const
{
	TArray<FVector2D> Points;
	Points.Add(Pos);
	Points.Add(Pos + FVector2D(Size.X, 0.0));
	Points.Add(Pos + Size);
	Points.Add(Pos + FVector2D(0.0, Size.Y));
	TArray<FLinearColor> Colors;
	Colors.Add(TL);
	Colors.Add(TR);
	Colors.Add(BR);
	Colors.Add(BL);
	TArray<uint32> Indices;
	Indices.Append({ 0u, 1u, 2u, 0u, 2u, 3u });
	CustomVerts(Points, Colors, Indices);
}

// =====================================================================================================================
// Text
// =====================================================================================================================

FVector2D FAbyssPainter::Measure(const FString& InText, const FSlateFontInfo& Font)
{
	if (InText.IsEmpty() || !FSlateApplication::IsInitialized())
	{
		return FVector2D::ZeroVector;
	}
	const TSharedRef<FSlateFontMeasure> Measurer = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	return FVector2D(Measurer->Measure(InText, Font));
}

void FAbyssPainter::Text(const FVector2D& Pos, const FString& InText, const FSlateFontInfo& Font, const FLinearColor& Color) const
{
	if (InText.IsEmpty() || Color.A <= 0.f)
	{
		return;
	}
	const FVector2D Size = Measure(InText, Font);
	FSlateDrawElement::MakeText(Elements, Layer, AbyssDraw_Paint(Geometry, Pos, Size), InText, Font, ESlateDrawEffect::None, Tinted(Color));
}

void FAbyssPainter::TextCentered(const FVector2D& Center, const FString& InText, const FSlateFontInfo& Font, const FLinearColor& Color,
	float Scale) const
{
	if (InText.IsEmpty() || Color.A <= 0.f || Scale <= 0.f)
	{
		return;
	}
	const FVector2D Size = Measure(InText, Font);
	const FVector2D TopLeft = Center - Size * (0.5 * Scale);
	FSlateDrawElement::MakeText(Elements, Layer, AbyssDraw_Paint(Geometry, TopLeft, Size, Scale), InText, Font, ESlateDrawEffect::None,
		Tinted(Color));
}

void FAbyssPainter::TextShadowed(const FVector2D& Pos, const FString& InText, const FSlateFontInfo& Font, const FLinearColor& Color,
	const FVector2D& ShadowOffset, const FLinearColor& Shadow) const
{
	if (InText.IsEmpty())
	{
		return;
	}
	const FVector2D Size = Measure(InText, Font);
	FLinearColor ShadowColor = Shadow;
	ShadowColor.A *= Color.A;
	FSlateDrawElement::MakeText(Elements, Layer, AbyssDraw_Paint(Geometry, Pos + ShadowOffset, Size), InText, Font, ESlateDrawEffect::None,
		Tinted(ShadowColor));
	FSlateDrawElement::MakeText(Elements, Layer, AbyssDraw_Paint(Geometry, Pos, Size), InText, Font, ESlateDrawEffect::None, Tinted(Color));
}

// =====================================================================================================================
// UiKit compositions
// =====================================================================================================================

void FAbyssPainter::Frame(const FVector2D& Pos, const FVector2D& Size, int32 Kind, const FLinearColor& Accent, float HeaderHeight,
	float BodyAlpha) const
{
	// 8.3 per variant: corner radius, iron band width, shadow offset.
	const float Radius = Kind == 0 ? 7.f : (Kind == 1 ? 5.f : 6.f);
	const float Band = Kind == 0 ? 5.f : (Kind == 1 ? 3.f : 3.5f);
	const float ShadowY = Kind == 0 ? 5.f : 3.f;
	const FAbyssUiPalette& C = Style.Colors();

	// drop shadow
	RoundBox(Pos + FVector2D(-2.0, ShadowY - 1.0), Size + FVector2D(4.0, 4.0), FLinearColor(0.f, 0.f, 0.f, 0.35f * BodyAlpha), Radius + 3.f);
	RoundBox(Pos + FVector2D(0.0, ShadowY), Size, FLinearColor(0.f, 0.f, 0.f, 0.3f * BodyAlpha), Radius + 1.f);

	// body: #241f26 -> #19161c (0.45) -> #0f0d11 (strips between the rounded corners keep the corner shape)
	const FLinearColor BodyTop = FAbyssUiStyle::WithAlpha(FAbyssUiStyle::Rgb(0x241f26), BodyAlpha);
	const FLinearColor BodyMid = FAbyssUiStyle::WithAlpha(FAbyssUiStyle::Rgb(0x19161c), BodyAlpha);
	const FLinearColor BodyBottom = FAbyssUiStyle::WithAlpha(FAbyssUiStyle::Rgb(0x0f0d11), BodyAlpha);
	RoundBox(Pos, Size, BodyMid, Radius);
	if (Size.Y > Radius * 2.f + 2.f)
	{
		VerticalGradient3(Pos + FVector2D(Band, Radius), FVector2D(Size.X - Band * 2.f, Size.Y - Radius * 2.f), BodyTop, BodyMid, 0.45f,
			BodyBottom, FMath::Clamp(static_cast<int32>(Size.Y / 14.0), 6, 40));
	}
	// warm light from the upper left, vignette towards the edges
	Glow(Pos + FVector2D(Size.X * 0.22, Size.Y * 0.18), static_cast<float>(FMath::Min(Size.X, Size.Y) * 0.75),
		FLinearColor(1.f, 0.55f, 0.25f, 0.06f * BodyAlpha), 6);
	Box(Pos + FVector2D(Band, Size.Y - Band - 6.0), FVector2D(Size.X - Band * 2.f, 6.0), FLinearColor(0.f, 0.f, 0.f, 0.18f * BodyAlpha));

	// header band with its gold rule and centre diamond
	if (HeaderHeight > 0.f)
	{
		const FVector2D HeaderPos = Pos + FVector2D(Band, Band);
		const FVector2D HeaderSize(Size.X - Band * 2.f, HeaderHeight - Band);
		VerticalGradient3(HeaderPos, HeaderSize, FLinearColor(0.19f, 0.08f, 0.012f, 0.34f), FLinearColor(0.06f, 0.023f, 0.005f, 0.2f), 0.5f,
			FLinearColor(0.007f, 0.004f, 0.002f, 0.35f), 8);
		const double RuleY = Pos.Y + HeaderHeight;
		Divider(FVector2D(Pos.X + Size.X * 0.5, RuleY), static_cast<float>(Size.X - Band * 2.f - 8.0), true, C.Gold);
	}

	// iron frame: dark outline, iron band, light bevel, dark inner edge, accent hairline
	RoundBox(Pos, Size, FLinearColor::Transparent, Radius, FAbyssUiStyle::Rgb(0x050407), 1.f);
	RoundBox(Pos + FVector2D(1.0, 1.0), Size - FVector2D(2.0, 2.0), FLinearColor::Transparent, Radius - 1.f, FAbyssUiStyle::Rgb(0x4a444f),
		Band - 1.f);
	Line(Pos + FVector2D(Radius, 1.5), Pos + FVector2D(Size.X - Radius, 1.5), FLinearColor(1.f, 0.87f, 0.73f, 0.22f), 1.f);
	RoundBox(Pos + FVector2D(Band, Band), Size - FVector2D(Band * 2.f, Band * 2.f), FLinearColor::Transparent, FMath::Max(1.f, Radius - Band),
		FAbyssUiStyle::Rgb(0x08070a), 1.f);
	FLinearColor Hairline = Accent;
	Hairline.A = Kind == 1 ? 0.85f : 0.6f;
	RoundBox(Pos + FVector2D(Band * 0.5f, Band * 0.5f), Size - FVector2D(Band, Band), FLinearColor::Transparent, Radius - Band * 0.5f,
		Hairline, 1.f);

	// rivets every 110 px along the band (panels), corner ornaments (gems on panels)
	if (Kind == 0)
	{
		const FLinearColor Rivet = FAbyssUiStyle::Rgb(0x77707c);
		for (double X = Pos.X + 55.0; X < Pos.X + Size.X - 30.0; X += 110.0)
		{
			Circle(FVector2D(X, Pos.Y + Band * 0.5f), 1.6f, Rivet);
			Circle(FVector2D(X, Pos.Y + Size.Y - Band * 0.5f), 1.6f, Rivet);
		}
	}
	const float Ornament = Kind == 0 ? 9.f : 6.f;
	const FVector2D Corners[4] = { Pos, Pos + FVector2D(Size.X, 0.0), Pos + Size, Pos + FVector2D(0.0, Size.Y) };
	for (const FVector2D& Corner : Corners)
	{
		Diamond(Corner, Ornament, C.Gold);
		Diamond(Corner, Ornament * 0.62f, FAbyssUiStyle::Rgb(0x2a1a08));
		if (Kind == 0)
		{
			Diamond(Corner, Ornament * 0.42f, FAbyssUiStyle::Rgb(0xb3202a));
		}
	}
}

void FAbyssPainter::Card(const FVector2D& Pos, const FVector2D& Size, const FLinearColor& Fill, const FLinearColor& Border,
	const FLinearColor& Strip, const FLinearColor& InGlow) const
{
	if (InGlow.A > 0.f)
	{
		FLinearColor Outer = InGlow;
		Outer.A = 0.12f;
		FLinearColor Inner = InGlow;
		Inner.A = 0.22f;
		RoundBox(Pos - FVector2D(4.0, 4.0), Size + FVector2D(8.0, 8.0), FLinearColor::Transparent, 8.f, Outer, 3.f);
		RoundBox(Pos - FVector2D(2.0, 2.0), Size + FVector2D(4.0, 4.0), FLinearColor::Transparent, 6.f, Inner, 2.f);
	}
	RoundBox(Pos + FVector2D(0.0, 1.5), Size, FLinearColor(0.f, 0.f, 0.f, 0.45f), 4.f);
	RoundBox(Pos, Size, Fill, 4.f);
	Box(Pos + FVector2D(2.0, 1.0), FVector2D(Size.X - 4.0, Size.Y * 0.45), FLinearColor(1.f, 0.94f, 0.85f, 0.045f));
	Box(Pos + FVector2D(2.0, Size.Y * 0.7), FVector2D(Size.X - 4.0, Size.Y * 0.3 - 1.0), FLinearColor(0.f, 0.f, 0.f, 0.12f));
	if (Strip.A > 0.f)
	{
		RoundBox(Pos, FVector2D(3.0, Size.Y), Strip, 1.5f);
	}
	RoundBox(Pos, Size, FLinearColor::Transparent, 4.f, Border, 1.f);
}

void FAbyssPainter::Well(const FVector2D& Pos, const FVector2D& Size, float Radius) const
{
	RoundBox(Pos, Size, FAbyssUiStyle::Rgb(0x060508), Radius, FAbyssUiStyle::Rgb(0x3a343f), 1.f);
	Box(Pos + FVector2D(Radius, 1.0), FVector2D(Size.X - Radius * 2.f, 2.0), FLinearColor(0.f, 0.f, 0.f, 0.5f));
}

void FAbyssPainter::Bar(const FVector2D& Pos, const FVector2D& Size, float Fraction, const FLinearColor& Fill, bool bTicks) const
{
	const float Radius = static_cast<float>(Size.Y * 0.5);
	RoundBox(Pos - FVector2D(2.0, 2.0), Size + FVector2D(4.0, 4.0), FAbyssUiStyle::Rgb(0x0c0b0e), Radius + 2.f,
		FAbyssUiStyle::WithAlpha(Style.Colors().Gold, 0.55f), 1.f);
	Fraction = FMath::Clamp(Fraction, 0.f, 1.f);
	if (Fraction > 0.f)
	{
		const double FillW = FMath::Max(Size.X * Fraction, Size.Y < Size.X * Fraction ? 0.0 : Size.Y * 0.5);
		const FVector2D FillSize(FMath::Min(FillW, Size.X), Size.Y);
		RoundBox(Pos, FillSize, FAbyssUiStyle::Darken(Fill, 0.3f), Radius);
		RoundBox(Pos, FVector2D(FillSize.X, Size.Y * 0.62), FAbyssUiStyle::Lighten(Fill, 0.12f), Radius);
		Box(Pos + FVector2D(Radius * 0.6f, 1.0), FVector2D(FMath::Max(0.0, FillSize.X - Radius * 1.2f), FMath::Max(1.0, Size.Y * 0.28)),
			FLinearColor(1.f, 1.f, 1.f, 0.18f));
	}
	if (bTicks)
	{
		for (int32 Tick = 1; Tick < 10; ++Tick)
		{
			const double X = Pos.X + Size.X * Tick / 10.0;
			Line(FVector2D(X, Pos.Y + 1.0), FVector2D(X, Pos.Y + Size.Y - 1.0), FLinearColor(0.f, 0.f, 0.f, 0.45f), 1.f);
		}
	}
}

void FAbyssPainter::Divider(const FVector2D& Center, float Width, bool bDiamond, const FLinearColor& Color) const
{
	const FLinearColor Gold = Color.A > 0.f ? Color : Style.Colors().Gold;
	const float Half = Width * 0.5f;
	// fading ends: three segments per side
	const float Fades[3] = { 0.85f, 0.45f, 0.15f };
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const float From = Half * (Index / 3.f);
		const float To = Half * ((Index + 1) / 3.f);
		FLinearColor Segment = Gold;
		Segment.A *= Fades[Index];
		Line(Center + FVector2D(From, 0.0), Center + FVector2D(To, 0.0), Segment, 1.2f);
		Line(Center - FVector2D(From, 0.0), Center - FVector2D(To, 0.0), Segment, 1.2f);
	}
	if (bDiamond)
	{
		Diamond(Center, 7.f, Gold);
		Diamond(Center, 3.5f, FAbyssUiStyle::Rgb(0x1a0f04));
	}
}

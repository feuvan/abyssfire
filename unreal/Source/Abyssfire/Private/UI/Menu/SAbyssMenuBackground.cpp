#include "UI/Menu/SAbyssMenuBackground.h"

#include "UI/AbyssUiStyle.h"
#include "UI/Core/AbyssUiDraw.h"

namespace
{
	/** Deterministic 0..1 hash for particle i, channel c (no RNG state: the backdrop is a pure function of time). */
	float AbyssMenuBg_Hash(int32 Index, int32 Channel)
	{
		uint32 X = static_cast<uint32>(Index) * 747796405u + static_cast<uint32>(Channel) * 2891336453u + 0x9E3779B9u;
		X = ((X >> ((X >> 28u) + 4u)) ^ X) * 277803737u;
		X = (X >> 22u) ^ X;
		return static_cast<float>(X & 0xFFFFFFu) / static_cast<float>(0xFFFFFFu);
	}

	constexpr int32 AbyssMenuBg_EmberCount = 64;
	constexpr int32 AbyssMenuBg_SparkCount = 22;
	constexpr int32 AbyssMenuBg_SmokeCount = 5;
}

void SAbyssMenuBackground::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext)
{
	Context = InContext;
	StartTime = InContext->Now();
	SetVisibility(EVisibility::HitTestInvisible);
}

int32 SAbyssMenuBackground::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	FAbyssPainter P(AllottedGeometry, OutDrawElements, LayerId, InWidgetStyle, Context->Style());
	const FVector2D Size(AllottedGeometry.GetLocalSize());
	const double T = Context->Now() - StartTime;
	const double W = Size.X;
	const double H = Size.Y;

	// gradient 0x050508 -> 0x1a0808
	P.VerticalGradient(FVector2D::ZeroVector, Size, FAbyssUiStyle::Rgb(0x050508), FAbyssUiStyle::Rgb(0x1a0808), 24);

	// fire glow at the bottom: alpha 0.15 <-> 0.30 over 8 s, scale +-5 % over 10 s
	{
		const float Pulse = 0.5f + 0.5f * FMath::Sin(static_cast<float>(T * UE_TWO_PI / 8.0));
		const float Breath = 1.f + 0.05f * FMath::Sin(static_cast<float>(T * UE_TWO_PI / 10.0));
		P.Glow(FVector2D(W * 0.5, H + 40.0), static_cast<float>(W * 0.45) * Breath, FLinearColor(1.f, 0.32f, 0.06f, 0.15f + 0.15f * Pulse), 14);
		P.Glow(FVector2D(W * 0.5, H + 20.0), static_cast<float>(W * 0.22) * Breath, FLinearColor(1.f, 0.55f, 0.15f, 0.12f + 0.08f * Pulse), 10);
	}

	// drifting smoke
	for (int32 Index = 0; Index < AbyssMenuBg_SmokeCount; ++Index)
	{
		const double Speed = 6.0 + 8.0 * AbyssMenuBg_Hash(Index, 40);
		const double X = FMath::Fmod(AbyssMenuBg_Hash(Index, 41) * (W + 400.0) + T * Speed, W + 400.0) - 200.0;
		const double Y = H * (0.55 + 0.35 * AbyssMenuBg_Hash(Index, 42)) + 20.0 * FMath::Sin(T * 0.2 + Index);
		const float R = 140.f + 120.f * AbyssMenuBg_Hash(Index, 43);
		P.Glow(FVector2D(X, Y), R, FLinearColor(0.35f, 0.3f, 0.32f, 0.05f), 6);
	}

	// rune circle behind the title: (640, 150) displayed 420 px, 360 deg / 120 s, alpha 0.09 <-> 0.17
	{
		const FVector2D Center(W * 0.5, 150.0 + (H - 720.0) * 0.5);
		const float Scale = 420.f / 480.f;
		const float Alpha = 0.13f + 0.04f * FMath::Sin(static_cast<float>(T * UE_TWO_PI / 7.0));
		const FLinearColor Rune = FLinearColor(1.f, 0.55f, 0.2f, Alpha);
		const double Rotation = T * UE_TWO_PI / 120.0;
		for (const float Ring : { 240.f, 226.f, 170.f, 158.f })
		{
			P.CircleOutline(Center, Ring * Scale, Rune, Ring > 200.f ? 1.6f : 1.2f, 96);
		}
		for (int32 TickIndex = 0; TickIndex < 48; ++TickIndex)
		{
			const double A = Rotation + TickIndex * UE_TWO_PI / 48.0;
			const FVector2D Dir(FMath::Cos(A), FMath::Sin(A));
			const double Inner = (TickIndex % 4 == 0 ? 224.0 : 230.0) * Scale;
			P.Line(Center + Dir * Inner, Center + Dir * (240.0 * Scale), Rune, 1.f);
		}
		// hexagram
		for (int32 Triangle = 0; Triangle < 2; ++Triangle)
		{
			TArray<FVector2D> Points;
			for (int32 Corner = 0; Corner <= 3; ++Corner)
			{
				const double A = -Rotation * 0.5 + Triangle * UE_PI / 3.0 + Corner * UE_TWO_PI / 3.0 - UE_HALF_PI;
				Points.Add(Center + FVector2D(FMath::Cos(A), FMath::Sin(A)) * (158.0 * Scale));
			}
			P.Polyline(Points, Rune, 1.2f);
		}
		for (int32 Dot = 0; Dot < 12; ++Dot)
		{
			const double A = Rotation + Dot * UE_TWO_PI / 12.0;
			P.Circle(Center + FVector2D(FMath::Cos(A), FMath::Sin(A)) * (198.0 * Scale), 2.2f, Rune);
		}
		P.Glow(Center, 150.f, FLinearColor(1.f, 0.5f, 0.15f, 0.06f), 8);
	}

	// embers: tints ff4400 / ff6600 / ff8800 / ffaa00, lifespan 3-6 s, rising from the bottom
	static const uint32 EmberTints[4] = { 0xff4400, 0xff6600, 0xff8800, 0xffaa00 };
	for (int32 Index = 0; Index < AbyssMenuBg_EmberCount; ++Index)
	{
		const double Life = 3.0 + 3.0 * AbyssMenuBg_Hash(Index, 1);
		const double Phase = FMath::Fmod(T + AbyssMenuBg_Hash(Index, 2) * Life, Life);
		const double Cycle = FMath::FloorToDouble((T + AbyssMenuBg_Hash(Index, 2) * Life) / Life);
		const float Seed = AbyssMenuBg_Hash(Index, 3 + static_cast<int32>(FMath::Fmod(Cycle, 7.0)));
		const double Speed = 40.0 + 70.0 * AbyssMenuBg_Hash(Index, 4);
		const double X = Seed * W + 18.0 * FMath::Sin(Phase * 1.7 + Index);
		const double Y = H + 6.0 - Phase * Speed;
		const float Fade = FMath::Sin(static_cast<float>(UE_PI * Phase / Life));
		const float R = 1.2f + 2.2f * AbyssMenuBg_Hash(Index, 5);
		const FLinearColor Tint = FAbyssUiStyle::Rgb(EmberTints[Index % 4], 0.85f * Fade);
		// gradient meshes, not cached rounded brushes (every particle animates its alpha)
		P.RadialGradient(FVector2D(X, Y), FVector2D::ZeroVector, FVector2D(R * 2.6, R * 2.6), FAbyssUiStyle::WithAlpha(Tint, 0.24f * Fade),
			FAbyssUiStyle::WithAlpha(Tint, 0.f), 12);
		P.RadialGradient(FVector2D(X, Y), FVector2D::ZeroVector, FVector2D(R, R), Tint, Tint, 10);
	}
	// sparks from a 20 px strip at the bottom: fast and short-lived
	for (int32 Index = 0; Index < AbyssMenuBg_SparkCount; ++Index)
	{
		const double Life = 0.8 + 0.9 * AbyssMenuBg_Hash(Index, 20);
		const double Phase = FMath::Fmod(T + AbyssMenuBg_Hash(Index, 21) * Life, Life);
		const double X = AbyssMenuBg_Hash(Index, 22) * W + (AbyssMenuBg_Hash(Index, 23) - 0.5) * 80.0 * Phase;
		const double Y = H - 20.0 * AbyssMenuBg_Hash(Index, 24) - Phase * (160.0 + 120.0 * AbyssMenuBg_Hash(Index, 25));
		const float Fade = 1.f - static_cast<float>(Phase / Life);
		const FLinearColor Spark(1.f, 0.85f, 0.45f, 0.9f * Fade);
		P.RadialGradient(FVector2D(X, Y), FVector2D::ZeroVector, FVector2D(1.1, 1.1), Spark, Spark, 8);
	}

	// vignette
	const FLinearColor Dark(0.f, 0.f, 0.f, 0.55f);
	const FLinearColor Clear(0.f, 0.f, 0.f, 0.f);
	P.ColoredQuad(FVector2D::ZeroVector, FVector2D(W, H * 0.25), Dark, Dark, Clear, Clear);
	P.ColoredQuad(FVector2D(0.0, H * 0.8), FVector2D(W, H * 0.2), Clear, Clear, Dark, Dark);
	P.ColoredQuad(FVector2D::ZeroVector, FVector2D(W * 0.18, H), Dark, Clear, Clear, Dark);
	P.ColoredQuad(FVector2D(W * 0.82, 0.0), FVector2D(W * 0.18, H), Clear, Dark, Dark, Clear);
	return P.Layer;
}

#include "UI/Hud/SAbyssMinimap.h"

#include "Engine/Texture2D.h"
#include "Rendering/DrawElements.h"
#include "Widgets/Layout/SBox.h"

#include "UI/AbyssUiStyle.h"
#include "UI/Core/AbyssUiDraw.h"
#include "UI/Hud/AbyssMinimapTexture.h"

namespace
{
	/** Tile space -> minimap local (rotation clockwise on screen by Angle radians around Center). */
	struct FAbyssMinimapXform
	{
		FVector2D Center;
		abyss::Vec2 Focus;
		double Scale = 1.0;
		double Cos = 1.0;
		double Sin = 0.0;

		FVector2D ToLocal(const abyss::Vec2& Tile) const
		{
			const double X = (Tile.x - Focus.x) * Scale;
			const double Y = (Tile.y - Focus.y) * Scale;
			return Center + FVector2D(X * Cos - Y * Sin, X * Sin + Y * Cos);
		}
		FVector2D RotateDir(double X, double Y) const
		{
			return FVector2D(X * Cos - Y * Sin, X * Sin + Y * Cos);
		}
	};
}

void SAbyssMinimap::Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext, const TSharedRef<FAbyssHudState>& InState,
	const TSharedRef<FAbyssMinimapTexture>& InTexture)
{
	Ctx = InContext;
	State = InState;
	Texture = InTexture;
	MapSize = InArgs._Size;
	bRotate = InArgs._Rotate;
	bWholeZone = InArgs._WholeZone;
	bInteractive = InArgs._Interactive;
	SetVisibility(bInteractive ? EVisibility::Visible : EVisibility::HitTestInvisible);
	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(MapSize)
		.HeightOverride(MapSize)
	];
}

FReply SAbyssMinimap::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!bInteractive)
	{
		return FReply::Unhandled();
	}
	bWholeZone = !bWholeZone;
	Ctx->PlaySound(abyss::SfxId::Click);
	return FReply::Handled();
}

int32 SAbyssMinimap::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (!State->bValid)
	{
		return LayerId;
	}
	FAbyssPainter P(AllottedGeometry, OutDrawElements, LayerId, InWidgetStyle, Ctx->Style());
	const FVector2D Size(AllottedGeometry.GetLocalSize());
	PaintMap(P, *Ctx, *State, *Texture, FVector2D::ZeroVector, static_cast<float>(FMath::Min(Size.X, Size.Y)), bRotate, bWholeZone, OutDrawElements,
		AllottedGeometry);
	return P.Layer;
}

void SAbyssMinimap::PaintMap(FAbyssPainter& P, const FAbyssUiContext& Ctx, const FAbyssHudState& S, const FAbyssMinimapTexture& Map,
	const FVector2D& Pos, float Size, bool bRotateMap, bool bWhole, FSlateWindowElementList& Elements, const FGeometry& Geometry)
{
	const FAbyssUiPalette& C = Ctx.Style().Colors();
	const int32 Cols = FMath::Max(1, Map.GetCols());
	const int32 Rows = FMath::Max(1, Map.GetRows());

	FAbyssMinimapXform X;
	X.Center = Pos + FVector2D(Size * 0.5f, Size * 0.5f);
	const double Angle = bRotateMap ? FMath::DegreesToRadians(-90.0 - static_cast<double>(S.CameraYawDeg)) : 0.0;
	X.Cos = FMath::Cos(Angle);
	X.Sin = FMath::Sin(Angle);
	if (bWhole)
	{
		X.Focus = abyss::Vec2((Cols - 1) * 0.5, (Rows - 1) * 0.5);
		X.Scale = Size / (FMath::Max(Cols, Rows) * (bRotateMap ? 1.4142 : 1.0));
	}
	else
	{
		X.Focus = S.HeroPos;
		X.Scale = Size / 48.0;   // about 48 tiles across around the hero
	}

	// backing
	P.RoundBox(Pos, FVector2D(Size, Size), FAbyssUiStyle::Rgb(0x07060a), 6.f);

	const FGeometry ClipGeometry = Geometry.MakeChild(FVector2f(Size, Size), FSlateLayoutTransform(FVector2f(Pos)));
	Elements.PushClip(FSlateClippingZone(ClipGeometry));

	// layer 1: tiles with the explored fog
	if (UTexture2D* MapTexture = Map.GetTexture())
	{
		if (const FSlateBrush* Brush = Ctx.RuntimeTextureBrush(MapTexture, FName(TEXT("AbyssMinimapTexture"))))
		{
			const FVector2D Corners[4] = {
				X.ToLocal(abyss::Vec2(-0.5, -0.5)),
				X.ToLocal(abyss::Vec2(Cols - 0.5, -0.5)),
				X.ToLocal(abyss::Vec2(Cols - 0.5, Rows - 0.5)),
				X.ToLocal(abyss::Vec2(-0.5, Rows - 0.5)) };
			const FVector2D UVs[4] = { FVector2D(0.0, 0.0), FVector2D(1.0, 0.0), FVector2D(1.0, 1.0), FVector2D(0.0, 1.0) };
			P.TexturedQuad(Brush, Corners, UVs, FLinearColor::White);
		}
	}
	const FAbyssMinimapMarkers& M = S.Minimap;
	const double K = X.Scale;
	const float Dot = static_cast<float>(FMath::Clamp(K * 0.9, 1.2, 3.0));

	// quest areas, explore / clue / escort / defend (layer 8 of the web, drawn under the dots)
	for (const FAbyssMinimapMarkers::FArea& Area : M.QuestAreas)
	{
		const FLinearColor Color = Area.bMain ? FAbyssUiStyle::Rgb(0xf1c40f) : FAbyssUiStyle::Rgb(0x95a5a6);
		const float R = static_cast<float>(FMath::Max(2.0, Area.Radius * K));
		P.Circle(X.ToLocal(Area.Pos), R, FAbyssUiStyle::WithAlpha(Color, 0.25f), FAbyssUiStyle::WithAlpha(Color, 0.6f), 1.f);
	}
	for (const abyss::Vec2& Point : M.ExplorePoints)
	{
		P.Box(X.ToLocal(Point) - FVector2D(1.5, 1.5), FVector2D(3.0, 3.0), FAbyssUiStyle::WithAlpha(FAbyssUiStyle::Rgb(0xf39c12), 0.5f));
	}
	for (const abyss::Vec2& Point : M.Clues)
	{
		P.Box(X.ToLocal(Point) - FVector2D(1.5, 1.5), FVector2D(3.0, 3.0), FAbyssUiStyle::WithAlpha(FAbyssUiStyle::Rgb(0x9b59b6), 0.6f));
	}
	for (const abyss::Vec2& Point : M.GatherNodes)
	{
		P.Circle(X.ToLocal(Point), 1.6f, FAbyssUiStyle::WithAlpha(FAbyssUiStyle::Rgb(0x7cc04a), 0.75f));
	}
	for (const abyss::Vec2& Point : M.EscortDestinations)
	{
		P.Box(X.ToLocal(Point) - FVector2D(2.0, 2.0), FVector2D(4.0, 4.0), FAbyssUiStyle::WithAlpha(FAbyssUiStyle::Rgb(0xe67e22), 0.6f));
	}
	for (const abyss::Vec2& Point : M.EscortNpcs)
	{
		P.Circle(X.ToLocal(Point), 2.f, FAbyssUiStyle::WithAlpha(FAbyssUiStyle::Rgb(0xe67e22), 0.5f));
	}
	for (const abyss::Vec2& Point : M.DefendTargets)
	{
		const FVector2D Local = X.ToLocal(Point);
		P.Circle(Local, 3.f, FAbyssUiStyle::WithAlpha(FAbyssUiStyle::Rgb(0xe74c3c), 0.5f));
		P.Ring(Local, 4.f, FAbyssUiStyle::WithAlpha(FAbyssUiStyle::Rgb(0xe74c3c), 0.8f), 1.f);
	}
	// exits (layer 3)
	for (const abyss::Vec2& Point : M.Exits)
	{
		P.Box(X.ToLocal(Point) - FVector2D(2.0, 2.0), FVector2D(4.0, 4.0), FAbyssUiStyle::Rgb(0x00e676));
	}
	for (const abyss::Vec2& Point : M.SealedExits)
	{
		P.Box(X.ToLocal(Point) - FVector2D(2.0, 2.0), FVector2D(4.0, 4.0), FAbyssUiStyle::Rgb(0x777777));
	}
	// monsters (layer 7)
	for (const FAbyssMinimapMarkers::FDot& Monster : M.Monsters)
	{
		if (Monster.Kind == 1)
		{
			P.Circle(X.ToLocal(Monster.Pos), FMath::Max(2.f, Dot), FAbyssUiStyle::WithAlpha(FAbyssUiStyle::Rgb(0xff4444), 0.9f));
		}
		else
		{
			P.Circle(X.ToLocal(Monster.Pos), FMath::Max(1.5f, Dot * 0.75f), FAbyssUiStyle::WithAlpha(FAbyssUiStyle::Rgb(0xcc6644), 0.5f));
		}
	}
	// quest givers (layer 5)
	for (const FAbyssMinimapMarkers::FDot& Npc : M.Npcs)
	{
		const bool bTurnIn = Npc.Kind == 1;
		const float R = bTurnIn ? 3.2f : 2.4f;
		const FVector2D Local = X.ToLocal(Npc.Pos);
		P.Circle(Local, R + 1.2f, FLinearColor(0.f, 0.f, 0.f, 0.7f));
		P.Circle(Local, R, bTurnIn ? FAbyssUiStyle::Rgb(0xffd23a) : FAbyssUiStyle::Rgb(0xf5e6a8));
	}
	// guide star (layer 6)
	if (M.bGuide)
	{
		const FVector2D Local = X.ToLocal(M.Guide);
		P.Circle(Local, 5.5f, FAbyssUiStyle::Rgb(0x1a0f04));
		P.Star(Local, 4.5f, 4.5f * 0.45f, M.bGuideTurnIn ? FAbyssUiStyle::Rgb(0xffd23a) : FAbyssUiStyle::Rgb(0xffb347));
	}
	// hero (layer 2, drawn last so it is never covered)
	{
		const FVector2D Local = X.ToLocal(S.HeroPos);
		P.Circle(Local, 4.f, FLinearColor(0.f, 0.f, 0.f, 0.8f));
		P.Circle(Local, 2.8f, FAbyssUiStyle::Rgb(0x7fd4ff));
	}
	Elements.PopClip();

	// frame: iron border, vignette, gold top gem
	P.RoundBox(Pos, FVector2D(Size, Size), FLinearColor::Transparent, 6.f, FAbyssUiStyle::Rgb(0x050407), 1.f);
	P.RoundBox(Pos + FVector2D(1.0, 1.0), FVector2D(Size - 2.f, Size - 2.f), FLinearColor::Transparent, 5.f, FAbyssUiStyle::Rgb(0x4a444f), 3.f);
	P.RoundBox(Pos + FVector2D(3.0, 3.0), FVector2D(Size - 6.f, Size - 6.f), FLinearColor::Transparent, 4.f, FAbyssUiStyle::WithAlpha(C.Gold, 0.55f), 1.f);
	P.Diamond(Pos + FVector2D(Size * 0.5f, 1.0), 9.f, C.Gold);
	P.Diamond(Pos + FVector2D(Size * 0.5f, 1.0), 4.5f, FAbyssUiStyle::Rgb(0xb3202a));

	// north indicator (W4): where tile "up" (row - 1) points on the rotated map
	if (bRotateMap)
	{
		const FVector2D Dir = X.RotateDir(0.0, -1.0);
		const FVector2D Mark = X.Center + Dir.GetSafeNormal() * (Size * 0.5f - 8.f);
		const FString North = Ctx.LocOrStr("ui.compass.north", TEXT("N"));
		P.Circle(Mark, 7.f, FLinearColor(0.02f, 0.015f, 0.03f, 0.85f), FAbyssUiStyle::WithAlpha(C.Gold, 0.8f), 1.f);
		P.TextCentered(Mark, North, Ctx.Style().Body(9.f, true, 1), C.GoldBright);
	}
}

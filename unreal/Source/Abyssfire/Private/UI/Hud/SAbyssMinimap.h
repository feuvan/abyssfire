// SAbyssMinimap: the HUD minimap (save-ui-input 6.11, DECISIONS W2 / W4 / U8): the zone texture with the explored fog,
// centred on the hero and rotated so the camera's forward points up (north indicator on the rim), with the web's marker
// layers (hero, exits, quest givers, guide star, monsters, quest areas / explore / clue / escort / defend markers).
// A click toggles between the local view and the whole zone. Also used (unrotated, whole zone) by the world-map panel.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

#include "UI/Core/AbyssUiContext.h"
#include "UI/Hud/AbyssHudState.h"

class FAbyssMinimapTexture;
struct FAbyssPainter;

class SAbyssMinimap : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAbyssMinimap)
		: _Size(104.f)
		, _Rotate(true)
		, _WholeZone(false)
		, _Interactive(true)
	{
	}
		SLATE_ARGUMENT(float, Size)
		/** W4: rotate with the camera yaw (the HUD); false = north up (world map). */
		SLATE_ARGUMENT(bool, Rotate)
		SLATE_ARGUMENT(bool, WholeZone)
		SLATE_ARGUMENT(bool, Interactive)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext, const TSharedRef<FAbyssHudState>& InState,
		const TSharedRef<FAbyssMinimapTexture>& InTexture);

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	/** Paints the map and markers into a square (shared with the world-map panel). */
	static void PaintMap(FAbyssPainter& P, const FAbyssUiContext& Ctx, const FAbyssHudState& State, const FAbyssMinimapTexture& Texture,
		const FVector2D& Pos, float Size, bool bRotate, bool bWholeZone, FSlateWindowElementList& Elements, const FGeometry& Geometry);

private:
	TSharedPtr<FAbyssUiContext> Ctx;
	TSharedPtr<FAbyssHudState> State;
	TSharedPtr<FAbyssMinimapTexture> Texture;
	float MapSize = 104.f;
	bool bRotate = true;
	bool bWholeZone = false;
	bool bInteractive = true;
};

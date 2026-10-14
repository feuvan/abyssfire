// World map (M; save-ui-input 7.6, DECISIONS U8 / W2 / W4): the five story zones of MapOrder as linked cards (visited
// zones with name and level range, the current one with a bobbing pin; zones not yet visited greyed with their chapter
// name), and the current zone's whole map with the explored fog and the minimap marker layers, rotated with the live
// camera yaw like the HUD minimap (W1: steps point the way they move on screen). Read-only.
#pragma once

#include "CoreMinimal.h"

#include "UI/Hud/AbyssHudState.h"
#include "UI/Panels/SAbyssPanelBase.h"

class FAbyssMinimapTexture;
class SCanvas;

class SAbyssWorldMapPanel : public SAbyssPanelBase
{
public:
	SLATE_BEGIN_ARGS(SAbyssWorldMapPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext, const TSharedRef<FAbyssHudState>& InHudState,
		const TSharedRef<FAbyssMinimapTexture>& InMinimap);

	virtual abyss::PanelId GetPanelId() const override { return abyss::PanelId::WorldMap; }
	virtual void Refresh(const abyss::Snapshot& Snap) override;
	virtual FVector2D GetDesignSize() const override { return FVector2D(680.0, 560.0); }
	/**
	 * 7.6 puts the 236-high zone strip at y 80; this panel adds the zone map (560 high), so it starts at y 24: bottom 584,
	 * clear of the HUD's interact prompt (y 599) and potion medallions (y 601).
	 */
	virtual FVector2D GetDesignPosition() const override { return FVector2D(-1.0, 24.0); }
	/** Explored share / legend refresh (the map itself repaints every frame from the shared minimap texture). */
	virtual double GetAutoRefreshSeconds() const override { return 1.0; }

private:
	TSharedPtr<SCanvas> Body;
	TSharedPtr<FAbyssHudState> HudState;
	TSharedPtr<FAbyssMinimapTexture> Minimap;
};

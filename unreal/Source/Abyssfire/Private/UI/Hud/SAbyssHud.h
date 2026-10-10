// SAbyssHud: the in-game HUD (save-ui-input 6): the bottom group (orbs, bars, skill bar, potions, utility buttons),
// combat log, minimap, info plate, quest tracker, target frame, boss bar, dodge plate, buffs, pet medallion and the
// transient notices. Desktop and touch layouts follow the web's resolved constants (6.2), anchored to the screen edges
// for layouts wider / taller than 1280x720. Hidden while a story beat plays (6.14, by the root).
#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

#include "abyss/sim/Events.h"
#include "abyss/sim/Snapshot.h"

#include "Framework/AbyssTypes.h"
#include "UI/Core/AbyssUiContext.h"
#include "UI/Hud/AbyssHudState.h"

class FAbyssMinimapTexture;
class SAbyssBossBar;
class SAbyssBuffRow;
class SAbyssCombatLog;
class SAbyssDodgePlate;
class SAbyssHudBottom;
class SAbyssInfoPlate;
class SAbyssMinimap;
class SAbyssNotices;
class SAbyssPetMedallion;
class SAbyssQuestTracker;
class SAbyssTargetFrame;
class SAbyssTouchControls;
class SConstraintCanvas;

class SAbyssHud : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAbyssHud) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAbyssUiContext>& InContext, const TSharedRef<FAbyssMinimapTexture>& InMinimap);

	void Sync(const abyss::Snapshot& Snap, const FAbyssFrameInfo& Frame);
	void HandleEvent(const abyss::Event& Event, const abyss::Snapshot* Snap);
	void OnSessionStarted();
	void OnSessionEnded();
	/** Desktop / touch layout switch or locale change. */
	void RebuildLayout();
	void ToggleCombatLog();
	/** The touch control layer (to keep the target frame clear of its top-left toggles). */
	void SetTouchControls(const TSharedPtr<SAbyssTouchControls>& InTouchControls) { TouchControls = InTouchControls; }

	TSharedRef<FAbyssHudState> GetState() const { return State.ToSharedRef(); }
	TSharedPtr<SAbyssCombatLog> GetCombatLog() const { return CombatLog; }
	TSharedPtr<SAbyssNotices> GetNotices() const { return Notices; }

private:
	void SyncHero(const abyss::Snapshot& Snap, double Now, float DeltaSeconds);
	void SyncTarget(const abyss::Snapshot& Snap);
	void SyncMinimapMarkers(const abyss::Snapshot& Snap, double Now);
	void PlaceTargetFrame();

	TSharedPtr<FAbyssUiContext> Ctx;
	TSharedPtr<FAbyssHudState> State;
	TSharedPtr<FAbyssMinimapTexture> MinimapTexture;
	TSharedPtr<SConstraintCanvas> Canvas;
	TSharedPtr<SAbyssHudBottom> Bottom;
	TSharedPtr<SAbyssCombatLog> CombatLog;
	TSharedPtr<SAbyssMinimap> Minimap;
	TSharedPtr<SAbyssQuestTracker> Tracker;
	TSharedPtr<SAbyssTargetFrame> TargetFrame;
	TSharedPtr<SAbyssBossBar> BossBar;
	TSharedPtr<SAbyssInfoPlate> InfoPlate;
	TSharedPtr<SAbyssDodgePlate> DodgePlate;
	TSharedPtr<SAbyssBuffRow> Buffs;
	TSharedPtr<SAbyssPetMedallion> PetMedallion;
	TSharedPtr<SAbyssNotices> Notices;
	TWeakPtr<SAbyssTouchControls> TouchControls;
	double NextTrackerRefresh = 0.0;
	bool bTrackerDirty = true;
	bool bLayoutTouch = false;
	double TargetFrameX = 0.0;
};

// UAbyssAutoTestSubsystem: the in-game autotest (-abyssautotest; switches in Framework/AbyssAutoTest.h, README.md
// "In-game autotest"). Platform-neutral: it runs with a real RHI (also off-screen with -RenderOffScreen) and with
// -nullrhi (no screenshots, everything else).
//
// Flow: main menu (screenshot) -> UAbyssGameInstance::StartNewGame for the class (the main menu's own call) -> the
// shared abyss::AutoPilot plays the scenario (smoke: up to the first main-quest turn-in; chapter: all of chapter 1),
// sending its commands through AAbyssPlayerController::SubmitCommand like the input layer, so the whole presentation
// (world, animation, VFX, UI, audio) runs. Story beats are skipped. At fixed points it photographs the screen WITH the
// UI (FScreenshotRequest): main menu, camp on arrival, each HUD panel opened through the input layer (inventory,
// character, skills, quest log, world map, settings), story beats, the first quest card / dialogue / shop, mid-combat,
// loot on the ground after a kill, the first cast of each hotbar slot, the end, plus one every N seconds.
// It records frame times, counts Error / Warning / Ensure log lines (GLog device), writes <dir>/report.json and exits
// with 0 (scenario completed, no Error / Ensure) or 2 (FPlatformMisc::RequestExitWithStatus). The wall-clock limit
// always ends the run (a watchdog thread force-exits when the game thread is stuck).
//
// Disabled (no instance at all) unless -abyssautotest is on the command line; never in Shipping or the editor.
#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Templates/SharedPointer.h"

#include "abyss/sim/Events.h"
#include "abyss/sim/Snapshot.h"

#include "Framework/AbyssTypes.h"

#include "AbyssAutoTestSubsystem.generated.h"

class FAbyssAutoTestRun;

UCLASS()
class UAbyssAutoTestSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ---- USubsystem ----
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	bool HandleTicker(float DeltaTime);
	void HandleSessionStarted();
	void HandleSessionEnded();
	void HandleCoreEvent(const abyss::Event& Event);
	void HandleFrame(const abyss::Snapshot& Snap, const FAbyssFrameInfo& Frame);

	/** The run (plain C++, defined in the .cpp). */
	TSharedPtr<FAbyssAutoTestRun> TestRun;
	FTSTicker::FDelegateHandle TickerHandle;
	bool bRouterBound = false;
};

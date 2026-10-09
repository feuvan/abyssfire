// IAbyssUiRoot: the Slate UI root (ui agent: UI/). One per game instance, registered with
// UAbyssGameInstance::RegisterUiRoot (typically a UGameInstanceSubsystem or ULocalPlayerSubsystem that registers
// itself in Initialize and unregisters in Deinitialize). It owns every screen: title / slots / class select / difficulty
// / language / settings menus, the HUD, panels, the story overlay, touch controls and the error screen.
//
// Event-driven UI (logs, banners, panels opened by the core via EvPanelRequest, floating text, quest updates) subscribes
// to FAbyssEventRouter; continuous HUD values (orbs, cooldowns, minimap) are pulled from the snapshot in SyncFrame.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "abyss/sim/Snapshot.h"

#include "Framework/AbyssTypes.h"

#include "AbyssUiRoot.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UAbyssUiRoot : public UInterface
{
	GENERATED_BODY()
};

class ABYSSFIRE_API IAbyssUiRoot
{
	GENERATED_BODY()

public:
	/** Boot -> MainMenu / DataError; MainMenu <-> InGame. Called on registration with the current state too. */
	virtual void OnAppStateChanged(EAbyssAppState NewState) = 0;
	/** Per frame while a session runs, after the world view synced (projected nameplates see this frame's actors). */
	virtual void SyncFrame(const abyss::Snapshot& Snap, const FAbyssFrameInfo& Frame) {}
	/**
	 * Esc / Android back / gamepad Start (U4): close the top panel; with nothing open, open the system menu. Returns true
	 * when the UI consumed it. The input agent's IA_Back calls this.
	 */
	virtual bool HandleBack() { return false; }
	/** The locale changed (settings): re-pull every visible text. */
	virtual void OnLocaleChanged() {}
	/** A fatal / blocking error to show (data load failure, corrupt save without backup, save write failure). */
	virtual void ShowSystemError(const FText& Title, const FText& Message) {}
};

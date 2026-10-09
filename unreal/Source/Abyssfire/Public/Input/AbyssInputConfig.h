// UAbyssInputConfig: every Enhanced Input object of the game, created at runtime in C++ (ue58-platform.md 8.1; no
// .uasset). Owned by UAbyssInputSubsystem (app lifetime, UPROPERTY so GC keeps it) and referenced by the player
// controller.
//
// Mapping contexts (priority, when applied):
//   IMC_Global            0    always (Boot .. InGame): IA_Back <- Esc, Android back, gamepad Start (U4)
//   IMC_MenuPad           0    MainMenu: IA_Back <- gamepad B (menu "back" on a pad)
//   IMC_Gameplay_KBM      1    InGame (also in the touch layout: a connected keyboard / -abysstouch testing)
//   IMC_Gameplay_Gamepad  1    InGame (P13: move, attack, dodge, skills, pause; W3 / 7.4 pad additions)
//   IMC_Story           100    InGame while a story beat plays (Snapshot::cinematic): advance / skip; its actions consume
//                              their keys so Space / A / LMB / Esc do not also dodge / cast / click / open the menu.
//
// Default bindings (save-ui-input.md 5.1 / 5.5 / 5.6, world-map-nav.md 7.4, DECISIONS I4 / U6 / P13):
//   KBM:  WASD + arrows move; LMB world press / hold-to-move; RMB + R town portal; 1-6 skills; Space dodge; Tab auto-combat;
//         Q / E HP / MP potion (I4 - the web's Q target cycle moves to T, world 7.4's E interact moves to F);
//         T target cycle ("lock"); F interact; I C K J M V O P panels (U6: no H / U in milestone 1; P only once a beast is
//         owned; O opens the U9 Settings panel); mouse wheel zoom (W1); Esc back / system menu.
//   Pad:  left stick move (dead zone 0.18); A X Y RB skills 1-4, L3 / R3 skills 5-6; B dodge; LB target cycle;
//         RT interact; LT HP potion; D-pad Left MP potion; D-pad Up auto-combat; D-pad Right auto-loot cycle;
//         D-pad Down town portal; View world map; Start back / system menu; right stick Y zoom.
//   Story: Space / Enter / LMB / pad A advance; Esc / pad Start / Android back skip.
#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectPtr.h"

#include "Input/AbyssInputTypes.h"

#include "AbyssInputConfig.generated.h"

class UInputAction;
class UInputMappingContext;

UCLASS(Transient)
class ABYSSFIRE_API UAbyssInputConfig : public UObject
{
	GENERATED_BODY()

public:
	static constexpr int32 PriorityGlobal = 0;
	static constexpr int32 PriorityGameplay = 1;
	static constexpr int32 PriorityStory = 100;

	/** Creates every action and mapping context (idempotent). */
	void Build();
	bool IsBuilt() const { return bBuilt; }

	UInputAction* GetAction(EAbyssInputAction Action) const;
	/** Reverse lookup for handlers bound with FInputActionInstance (GetSourceAction). */
	bool FindActionId(const UInputAction* Action, EAbyssInputAction& OutAction) const;

	UInputMappingContext* GetGlobalContext() const { return GlobalContext; }
	UInputMappingContext* GetMenuPadContext() const { return MenuPadContext; }
	UInputMappingContext* GetKeyboardMouseContext() const { return KeyboardMouseContext; }
	UInputMappingContext* GetGamepadContext() const { return GamepadContext; }
	UInputMappingContext* GetStoryContext() const { return StoryContext; }

	/**
	 * Short ASCII label of the first key bound to Action for Device ("F", "Space", "1", "RT", "A", "LMB"), for HUD button
	 * hints such as the interact prompt. Empty when the action has no key on that device (touch: always empty).
	 */
	FString GetKeyHint(EAbyssInputAction Action, EAbyssInputDevice Device) const;
	/** Short ASCII label of a key (the table GetKeyHint uses). */
	static FString ShortKeyLabel(const FKey& Key);

private:
	struct FKeyBindingRecord
	{
		EAbyssInputAction Action = EAbyssInputAction::Count;
		FKey Key;
		bool bGamepad = false;
	};

	UInputAction* MakeAction(EAbyssInputAction Action);
	/** Maps Key -> Action in Context and records the binding for hints. */
	void MapPlainKey(UInputMappingContext* Context, EAbyssInputAction Action, const FKey& Key);
	/** WASD-style digital key -> 2D move: bSwizzle puts the key on Y, bNegate flips the sign. */
	void MapMoveKey(UInputMappingContext* Context, const FKey& Key, bool bSwizzle, bool bNegate);
	void MapStick(UInputMappingContext* Context, EAbyssInputAction Action, const FKey& Key, float DeadZone, bool bAxial);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> Actions;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> GlobalContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MenuPadContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> KeyboardMouseContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> GamepadContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> StoryContext;

	TArray<FKeyBindingRecord> KeyBindings;
	bool bBuilt = false;
};

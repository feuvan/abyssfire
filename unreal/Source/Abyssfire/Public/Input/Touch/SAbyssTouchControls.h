// SAbyssTouchControls: the whole touch control layer (save-ui-input.md 5.7, world-map-nav.md 7.4, DECISIONS C8 / I4 / Q6 /
// U6 / U9 / W3; layout in AbyssTouchLayout.h).
//
// Hosting (UI agent): add it full screen above the HUD widgets, inside the HUD's SSafeZone, e.g.
//     HudOverlay->AddSlot()[ InputSubsystem->CreateTouchControls(Visuals) ];
// It is self-driving: every tick it reads the GameInstance (app state, touch layout, settings) and the snapshot
// (hotbar cooldowns, potions, portal state, interact prompt, auto toggles, dead hero) and shows itself only in game, in
// the touch layout, outside story beats (6.14 cinematic hide). Its empty area is hit-test invisible, so world taps reach
// the viewport; every control claims its own presses (5.7.4).
//
// Controls -> actions (all through UAbyssInputSubsystem::PressAction, i.e. Enhanced Input injection, ue58 8.2):
//   joystick -> IA_Move (continuous); LOCK -> TargetCycle; skills -> Skill1..6 (only bound hotbar slots); dodge;
//   HP / MP potions; Talk / Use (shown within 2.5 tiles of the core's interact prompt, Q6) -> Interact;
//   auto-combat; auto-loot cycle; town portal (channel sweep, dimmed while the core would refuse, W3);
//   panel squares -> Panel* (U6: no homestead; pets once owned); log -> UI request ToggleCombatLog;
//   menu -> UI request OpenSystemMenu.
// Opacity and size follow FAbyssUserSettings::TouchControlOpacity / TouchControlScale (U9).
#pragma once

#include "CoreMinimal.h"
#include "Layout/SlateRect.h"
#include "UObject/WeakObjectPtr.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

#include <string_view>

#include "abyss/base/Enums.h"
#include "abyss/sim/SimTypes.h"

#include "Input/Touch/AbyssTouchLayout.h"
#include "Input/Touch/AbyssTouchStyle.h"

class SAbyssTouchButton;
class SAbyssVirtualJoystick;
class SConstraintCanvas;
class UAbyssGameInstance;
class UAbyssInputSubsystem;

namespace abyss
{
	struct Snapshot;
}

class ABYSSFIRE_API SAbyssTouchControls : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAbyssTouchControls) {}
		/** Required: the input bridge (state source and injection target). */
		SLATE_ARGUMENT(TWeakObjectPtr<UAbyssInputSubsystem>, InputSubsystem)
		/** Optional icons / font from the HUD's style. */
		SLATE_ARGUMENT(FAbyssTouchVisuals, Visuals)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SAbyssTouchControls() override;

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	/**
	 * Presses bubble here only when the control under the pointer declined them (outside its circle but inside its box,
	 * which can overlap a neighbour's circle on the skill rings): the press goes to the control whose hit area contains
	 * it, else it continues to the world.
	 */
	virtual FReply OnTouchStarted(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	/**
	 * Rectangles (this widget's local Slate units) of the controls shown with the current layout, for the HUD to keep its
	 * own widgets clear (e.g. the touch target frame next to the top-left toggles at k >= 1.66, world 7.4). Empty while the
	 * layer is hidden or before its first tick.
	 */
	void GetOccupiedRects(TArray<FSlateRect>& OutRects) const;
	/** Whether the layer is currently shown (in game, touch layout, no story beat). */
	bool IsLayerShown() const { return bShown; }

private:
	/** Inputs that force a rebuild of the widget tree. */
	struct FLayoutKey
	{
		FIntPoint LayerSize = FIntPoint::ZeroValue;
		int32 UnitScaleCenti = 0;
		uint8 SkillSlotMask = 0;
		bool bShowPets = false;
		abyss::LocaleId Locale = abyss::LocaleId::ZhCN;

		bool operator==(const FLayoutKey& Other) const = default;
	};

	UAbyssGameInstance* GetGameInstance() const;
	bool ComputeShouldShow() const;
	EVisibility GetLayerVisibility() const;
	FLinearColor GetLayerColor() const;

	void RebuildLayout(const FLayoutKey& Key, float UnitScale);
	void UpdateDynamicState(const abyss::Snapshot& Snap, const UAbyssGameInstance& GameInstance);
	void UpdateCooldownButton(EAbyssTouchControl Control, double RemainingMs, double TotalMs, bool bBlocked);
	void HandleControlPressed(EAbyssTouchControl Control);
	FReply RouteMissedPress(const FGeometry& MyGeometry, const FPointerEvent& Event);
	void ResetTransientState();

	/** i18n text of a core key, or the English fallback while the key is missing from the tables. */
	FString LocalizeOr(std::string_view Key, const TCHAR* Fallback) const;
	const FSlateBrush* HudIcon(const TCHAR* IconId) const;
	FSlateFontInfo BaseFont() const;
	TSharedPtr<SAbyssTouchButton> Button(EAbyssTouchControl Control) const;

	TWeakObjectPtr<UAbyssInputSubsystem> InputSubsystem;
	FAbyssTouchVisuals Visuals;
	TSharedPtr<SConstraintCanvas> Canvas;
	TSharedPtr<SAbyssVirtualJoystick> Joystick;
	TArray<TSharedPtr<SAbyssTouchButton>> Buttons;   // indexed by EAbyssTouchControl
	TArray<FAbyssTouchPlacement> Placements;
	FAbyssTouchMetrics Metrics;
	FLayoutKey CurrentKey;
	bool bHasLayout = false;
	/** Last frame's per-control cooldown remaining (ms), for the ready pop. */
	TArray<double> PreviousRemainingMs;
	/** Skill id per hotbar slot (icon / label refresh when the hotbar changes, C3). */
	TArray<FString> SlotSkillIds;
	/** Last labelled states (labels are re-localized only on change or rebuild). */
	int32 LabelledAutoCombat = INDEX_NONE;
	int32 LabelledAutoLoot = INDEX_NONE;
	int32 LabelledPromptKind = INDEX_NONE;
	/** Visible state (mutable: updated by the visibility attribute, which is evaluated even while collapsed). */
	mutable bool bShown = false;
	mutable bool bResetPending = true;
};

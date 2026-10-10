// INPUT AGENT FILE. Input half of AAbyssPlayerController (see Framework/AbyssPlayerController.h "input agent region").
//
// Devices -> abyss::Command (save-ui-input.md 5, world-map-nav.md 7, ue58-platform.md 8; DECISIONS C7 / C8 / I4 / P13 /
// Q6 / S5 / U4 / U6 / W1 / W3 / W8):
//
//   source                                   handling                                      command / request
//   WASD, arrows, left stick, touch joystick IA_Move (polled after input, camera yaw)      CmdSetMoveInput (tile space)
//   LMB / finger on the world                AbyssPicking -> entity, else ground tile      CmdAttackTarget / CmdInteract /
//                                                                                          CmdPickUp / CmdPointerPress
//   LMB / finger held after a ground press   re-pick under the pointer every frame         CmdPointerHold (tile changed)
//   RMB, R, D-pad down, touch portal         IA_AltClick / IA_TownPortal                   CmdTownPortal
//   1-6, A X Y RB L3 R3, skill buttons       IA_Skill1..6 (+ cursor tile on KBM)           CmdCastSkill
//   Space, B, dodge button                   IA_Dodge (move direction, else facing C8)     CmdDodge
//   T, LB, LOCK                              IA_TargetCycle                                CmdCycleTarget
//   Tab, D-pad up / right, toggles           IA_ToggleAuto / IA_AutoLootCycle              CmdToggleAutoCombat / Cycle...
//   F, RT, Talk / Use button                 IA_Interact                                   CmdInteract{none}
//   Q / E, LT / D-pad left, potion buttons   IA_PotionHp / IA_PotionMp                     CmdUsePotion
//   I C K J M V O P, View, panel squares     IA_Panel_* (U6: P only with a beast)          UI request TogglePanel
//   Space / Enter / LMB / A / tap in a beat  IA_StoryAdvance (IMC_Story)                   UI request StoryAdvance
//   Esc / Start / Android back in a beat     IA_StorySkip (IMC_Story)                      UI request StorySkip
//   Esc / Start / Android back               IA_Back                                       IAbyssUiRoot::HandleBack (U4)
//   wheel, pinch, right stick Y              camera rig                                    AAbyssCameraRig::AddZoomInput
//
// The core rejects hero commands while Snapshot::inputBlocked or Dying (D13, U7, 5.1.1); the only states repaired here
// are the "stop" commands such a block may have swallowed (zero move input, hold release), re-sent when it lifts.
#include "Framework/AbyssPlayerController.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/InputComponent.h"
#include "EngineUtils.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Framework/Application/SlateApplication.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Layout/WidgetPath.h"
#include "Misc/App.h"
#include "Widgets/SViewport.h"

#include "abyss/pets/PetSystem.h"
#include "abyss/sim/Commands.h"
#include "abyss/sim/Snapshot.h"

#include "Camera/AbyssCameraRig.h"
#include "Framework/AbyssGameInstance.h"
#include "Framework/AbyssUiRoot.h"
#include "Input/AbyssInputConfig.h"
#include "Input/AbyssInputMath.h"
#include "Input/AbyssInputSubsystem.h"
#include "Input/AbyssPicking.h"

namespace
{
	// Mapping-context bits of AAbyssPlayerController::AbyssAppliedContexts.
	constexpr uint8 AbyssPcInput_CtxGlobal = 1u << 0;
	constexpr uint8 AbyssPcInput_CtxMenuPad = 1u << 1;
	constexpr uint8 AbyssPcInput_CtxKbm = 1u << 2;
	constexpr uint8 AbyssPcInput_CtxPad = 1u << 3;
	constexpr uint8 AbyssPcInput_CtxStory = 1u << 4;

	// Pointer ids of CmdPointerPress / CmdPointerHold: the mouse is 0, fingers 1 + ETouchIndex.
	constexpr int32 AbyssPcInput_MousePointerId = 0;

	int32 AbyssPcInput_TouchPointerId(ETouchIndex::Type Finger)
	{
		return 1 + static_cast<int32>(Finger);
	}

	// Pick slop around projected actor bounds, Slate units (converted with the DPI scale): a finger is wide.
	constexpr float AbyssPcInput_MouseSlopSlate = 4.0f;
	constexpr float AbyssPcInput_TouchSlopSlate = 14.0f;
	// Gamepad right stick Y at full deflection, in wheel notches per second (the rig clamps to W1's 0.75 .. 1.25).
	constexpr double AbyssPcInput_ZoomStickNotchesPerSecond = 6.0;

	const FIntPoint AbyssPcInput_NoTile(MIN_int32, MIN_int32);

	FIntPoint AbyssPcInput_RoundedTile(const abyss::Vec2& Tile)
	{
		const abyss::TilePos Rounded = abyss::RoundToTile(Tile);
		return FIntPoint(Rounded.col, Rounded.row);
	}

	// Discrete actions handled by HandleAbyssActionStarted (Started = the press edge, web JustDown semantics).
	constexpr EAbyssInputAction AbyssPcInput_DiscreteActions[] = {
		EAbyssInputAction::AltClick,
		EAbyssInputAction::Back,
		EAbyssInputAction::StoryAdvance,
		EAbyssInputAction::StorySkip,
		EAbyssInputAction::Skill1,
		EAbyssInputAction::Skill2,
		EAbyssInputAction::Skill3,
		EAbyssInputAction::Skill4,
		EAbyssInputAction::Skill5,
		EAbyssInputAction::Skill6,
		EAbyssInputAction::Dodge,
		EAbyssInputAction::TargetCycle,
		EAbyssInputAction::ToggleAutoCombat,
		EAbyssInputAction::CycleAutoLoot,
		EAbyssInputAction::TownPortal,
		EAbyssInputAction::Interact,
		EAbyssInputAction::PotionHp,
		EAbyssInputAction::PotionMp,
		EAbyssInputAction::PanelInventory,
		EAbyssInputAction::PanelCharacter,
		EAbyssInputAction::PanelSkills,
		EAbyssInputAction::PanelQuestLog,
		EAbyssInputAction::PanelWorldMap,
		EAbyssInputAction::PanelAchievements,
		EAbyssInputAction::PanelSettings,
		EAbyssInputAction::PanelPets,
	};
}

// =====================================================================================================================
// Setup / lifetime
// =====================================================================================================================

void AAbyssPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EnhancedComponent = Cast<UEnhancedInputComponent>(InputComponent);
	if (EnhancedComponent == nullptr)
	{
		UE_LOG(LogAbyssInput, Error,
			TEXT("PlayerController input component is not a UEnhancedInputComponent: check DefaultInputComponentClass in Config/DefaultInput.ini"));
		return;
	}
	if (AbyssInputConfig == nullptr)
	{
		if (UAbyssInputSubsystem* InputSubsystem = GetAbyssInputSubsystem())
		{
			AbyssInputConfig = InputSubsystem->GetConfig();
		}
	}
	const UAbyssInputConfig* ActionConfig = AbyssInputConfig;
	if (ActionConfig == nullptr || !ActionConfig->IsBuilt())
	{
		UE_LOG(LogAbyssInput, Error, TEXT("UAbyssInputSubsystem has no built input config: no input bindings"));
		return;
	}
	if (AbyssBoundInputComponent.Get() == EnhancedComponent)
	{
		return;   // already bound (SetupInputComponent can run again on input-system re-initialisation)
	}
	AbyssBoundInputComponent = EnhancedComponent;

	for (const EAbyssInputAction ActionId : AbyssPcInput_DiscreteActions)
	{
		if (const UInputAction* Action = ActionConfig->GetAction(ActionId))
		{
			EnhancedComponent->BindAction(Action, ETriggerEvent::Started, this, &ThisClass::HandleAbyssActionStarted, ActionId);
		}
	}
	if (const UInputAction* ClickAction = ActionConfig->GetAction(EAbyssInputAction::Click))
	{
		EnhancedComponent->BindAction(ClickAction, ETriggerEvent::Started, this, &ThisClass::HandleAbyssClickStarted);
		EnhancedComponent->BindAction(ClickAction, ETriggerEvent::Completed, this, &ThisClass::HandleAbyssClickCompleted);
	}
	// Analog actions are polled after the input tick (GetBoundActionValue): movement, wheel zoom, stick zoom.
	for (const EAbyssInputAction ActionId : { EAbyssInputAction::Move, EAbyssInputAction::Zoom, EAbyssInputAction::ZoomStick })
	{
		if (const UInputAction* Action = ActionConfig->GetAction(ActionId))
		{
			EnhancedComponent->BindActionValue(Action);
		}
	}
	// World touches (Slate controls claim theirs with FReply::Handled, so only world fingers arrive here, ue58 8.3).
	EnhancedComponent->BindTouch(IE_Pressed, this, &ThisClass::HandleAbyssTouchPressed);
	EnhancedComponent->BindTouch(IE_Repeat, this, &ThisClass::HandleAbyssTouchMoved);
	EnhancedComponent->BindTouch(IE_Released, this, &ThisClass::HandleAbyssTouchReleased);
}

void AAbyssPlayerController::InitAbyssInput()
{
	if (AbyssInputConfig == nullptr)
	{
		if (UAbyssInputSubsystem* InputSubsystem = GetAbyssInputSubsystem())
		{
			AbyssInputConfig = InputSubsystem->GetConfig();
		}
	}
	FMemory::Memzero(AbyssInjectedSource, sizeof(AbyssInjectedSource));
	AbyssAppliedContexts = 0;
	AbyssWorldTouches.Reset();
	bAbyssPinching = false;
	bAbyssMouseHold = false;
	ResetAbyssMovementState();
}

void AAbyssPlayerController::ShutdownAbyssInput()
{
	CancelAbyssWorldPointers();
	ClearAbyssHover();
	ResetAbyssMovementState();
	if (UEnhancedInputLocalPlayerSubsystem* EnhancedSubsystem = GetAbyssEnhancedInputSubsystem())
	{
		if (const UAbyssInputConfig* ActionConfig = AbyssInputConfig)
		{
			const UInputMappingContext* Contexts[] = {
				ActionConfig->GetGlobalContext(), ActionConfig->GetMenuPadContext(), ActionConfig->GetKeyboardMouseContext(),
				ActionConfig->GetGamepadContext(), ActionConfig->GetStoryContext(),
			};
			for (const UInputMappingContext* Context : Contexts)
			{
				if (Context != nullptr && EnhancedSubsystem->HasMappingContext(Context))
				{
					EnhancedSubsystem->RemoveMappingContext(Context);
				}
			}
		}
	}
	AbyssAppliedContexts = 0;
	AbyssCameraRig.Reset();
	AbyssInputConfig = nullptr;
}

void AAbyssPlayerController::OnAbyssInputContextChanged(EAbyssAppState NewState, bool bTouch)
{
	const bool bStateChanged = NewState != AbyssInputAppState;
	const bool bLayoutChanged = bTouch != bAbyssTouchLayout;
	if (bStateChanged || bLayoutChanged)
	{
		// Release while the old state is still current (the release may still reach the running session).
		CancelAbyssWorldPointers();
		ClearAbyssHover();
	}
	AbyssInputAppState = NewState;
	bAbyssTouchLayout = bTouch;
	if (bStateChanged)
	{
		ResetAbyssMovementState();
	}
	const abyss::Snapshot* Snap = GetAbyssGameSnapshot();
	RefreshAbyssMappingContexts(Snap != nullptr && Snap->cinematic);
}

void AAbyssPlayerController::RefreshAbyssMappingContexts(bool bStoryActive)
{
	const UAbyssInputConfig* ActionConfig = AbyssInputConfig;
	UEnhancedInputLocalPlayerSubsystem* EnhancedSubsystem = GetAbyssEnhancedInputSubsystem();
	if (ActionConfig == nullptr || EnhancedSubsystem == nullptr)
	{
		return;   // retried every tick until the local player and the config exist
	}
	uint8 Wanted = AbyssPcInput_CtxGlobal;
	if (AbyssInputAppState == EAbyssAppState::MainMenu)
	{
		Wanted |= AbyssPcInput_CtxMenuPad;
	}
	else if (AbyssInputAppState == EAbyssAppState::InGame)
	{
		// The keyboard context stays on in the touch layout too (tablet keyboards, -abysstouch testing).
		Wanted |= AbyssPcInput_CtxKbm | AbyssPcInput_CtxPad;
		if (bStoryActive)
		{
			Wanted |= AbyssPcInput_CtxStory;
		}
	}
	if (Wanted == AbyssAppliedContexts)
	{
		return;
	}

	struct FAbyssContextEntry
	{
		uint8 Bit;
		const UInputMappingContext* Context;
		int32 Priority;
	};
	const FAbyssContextEntry Entries[] = {
		{ AbyssPcInput_CtxGlobal, ActionConfig->GetGlobalContext(), UAbyssInputConfig::PriorityGlobal },
		{ AbyssPcInput_CtxMenuPad, ActionConfig->GetMenuPadContext(), UAbyssInputConfig::PriorityGlobal },
		{ AbyssPcInput_CtxKbm, ActionConfig->GetKeyboardMouseContext(), UAbyssInputConfig::PriorityGameplay },
		{ AbyssPcInput_CtxPad, ActionConfig->GetGamepadContext(), UAbyssInputConfig::PriorityGameplay },
		{ AbyssPcInput_CtxStory, ActionConfig->GetStoryContext(), UAbyssInputConfig::PriorityStory },
	};
	for (const FAbyssContextEntry& Entry : Entries)
	{
		if (Entry.Context == nullptr)
		{
			continue;
		}
		const bool bWant = (Wanted & Entry.Bit) != 0;
		const bool bHas = EnhancedSubsystem->HasMappingContext(Entry.Context);
		if (bWant && !bHas)
		{
			// Default options: keys held across the rebuild are ignored until released, so the press that closed a story
			// beat (A / Space / LMB) does not also cast / dodge / click once IMC_Story is gone (world 7.4).
			EnhancedSubsystem->AddMappingContext(Entry.Context, Entry.Priority);
		}
		else if (!bWant && bHas)
		{
			EnhancedSubsystem->RemoveMappingContext(Entry.Context);
		}
	}
	AbyssAppliedContexts = Wanted;
}

// =====================================================================================================================
// Per frame
// =====================================================================================================================

void AAbyssPlayerController::PreProcessInput(const float DeltaTime, const bool bGamePaused)
{
	Super::PreProcessInput(DeltaTime, bGamePaused);
	if (AbyssInputAppState != EAbyssAppState::InGame || AbyssInputConfig == nullptr)
	{
		return;
	}
	// Touch joystick -> IA_Move, injected before Enhanced Input evaluates this frame (ue58-platform.md 8.2) so it sums with
	// the keys / stick like any other mapping. The widget publishes screen space with Y down; IA_Move is Y up.
	const UAbyssInputSubsystem* InputSubsystem = GetAbyssInputSubsystem();
	if (InputSubsystem == nullptr || !InputSubsystem->IsVirtualStickActive())
	{
		return;
	}
	const FVector2D Stick = InputSubsystem->GetVirtualStick();
	if (Stick.IsNearlyZero())
	{
		return;
	}
	UEnhancedInputLocalPlayerSubsystem* EnhancedSubsystem = GetAbyssEnhancedInputSubsystem();
	const UInputAction* MoveAction = AbyssInputConfig->GetAction(EAbyssInputAction::Move);
	if (EnhancedSubsystem != nullptr && MoveAction != nullptr)
	{
		EnhancedSubsystem->InjectInputForAction(MoveAction, FInputActionValue(FVector2D(Stick.X, -Stick.Y)), {}, {});
	}
}

void AAbyssPlayerController::PostProcessInput(const float DeltaTime, const bool bGamePaused)
{
	Super::PostProcessInput(DeltaTime, bGamePaused);
	// Injected presses were handled during this evaluation (or swallowed, e.g. the key was already held): forget their
	// sources so a later real key press is not mistaken for a HUD / touch press.
	FMemory::Memzero(AbyssInjectedSource, sizeof(AbyssInjectedSource));
}

void AAbyssPlayerController::TickAbyssInput(float DeltaTime)
{
	const abyss::Snapshot* Snap = GetAbyssGameSnapshot();
	const bool bStoryActive = Snap != nullptr && Snap->cinematic;
	RefreshAbyssMappingContexts(bStoryActive);
	if (Snap == nullptr)
	{
		ClearAbyssHover();
		return;
	}

	// The input block (frozen world, modal panel, U7) lifted: the core may have rejected a "stop" sent meanwhile.
	const bool bBlocked = Snap->inputBlocked;
	if (bAbyssWasInputBlocked && !bBlocked)
	{
		bAbyssMoveResyncPending = true;
		if (AbyssLastReleasedPointer != INDEX_NONE)
		{
			SubmitCommand(abyss::CmdPointerHold{abyss::Vec2(), AbyssLastReleasedPointer, false});
			AbyssLastReleasedPointer = INDEX_NONE;
		}
	}
	bAbyssWasInputBlocked = bBlocked;

	if (bStoryActive)
	{
		// A beat clears hold-to-move in the core (D13 F1); world pointers start fresh after it.
		if (bAbyssMouseHold || AbyssWorldTouches.Num() > 0)
		{
			CancelAbyssWorldPointers();
		}
		ClearAbyssHover();
	}

	TickAbyssMovement(*Snap);
	if (!bStoryActive)
	{
		TickAbyssZoom(DeltaTime);
		TickAbyssHolds(*Snap);
		TickAbyssHover(*Snap);
	}
}

void AAbyssPlayerController::TickAbyssMovement(const abyss::Snapshot& Snap)
{
	// S5 / save-ui-input 5.2: screen-space direction -> tile direction with the live camera yaw, magnitude dropped (Q15).
	// Re-sent every frame while held (the web reads the keys every frame; a freeze or a zone entry clears the core's
	// direct input), zero once on release and again when an input block lifts.
	const abyss::Vec2 Direction = AbyssInputMath::ScreenInputToTileDir(GetAbyssMoveInput(), GetAbyssCameraYaw());
	if (Direction.LengthSq() > 0.0)
	{
		SubmitCommand(abyss::CmdSetMoveInput{Direction, /*screenSpace*/ false});
		bAbyssMoveSentNonZero = true;
		bAbyssMoveResyncPending = false;
	}
	else if (bAbyssMoveSentNonZero || bAbyssMoveResyncPending)
	{
		SubmitCommand(abyss::CmdSetMoveInput{abyss::Vec2(), /*screenSpace*/ false});
		bAbyssMoveSentNonZero = false;
		bAbyssMoveResyncPending = false;
	}
}

void AAbyssPlayerController::TickAbyssZoom(float DeltaTime)
{
	const UEnhancedInputComponent* EnhancedComponent = Cast<UEnhancedInputComponent>(InputComponent);
	const UAbyssInputConfig* ActionConfig = AbyssInputConfig;
	if (EnhancedComponent == nullptr || ActionConfig == nullptr)
	{
		return;
	}
	double Notches = 0.0;
	if (const UInputAction* WheelAction = ActionConfig->GetAction(EAbyssInputAction::Zoom))
	{
		Notches += EnhancedComponent->GetBoundActionValue(WheelAction).Get<float>();   // +1 per notch up = zoom in
	}
	if (const UInputAction* StickAction = ActionConfig->GetAction(EAbyssInputAction::ZoomStick))
	{
		// Real (undilated) frame time: zoom speed does not follow the S6 slow motion.
		const double RealDeltaSeconds = FMath::Min(FApp::GetDeltaTime(), 0.1);
		Notches += EnhancedComponent->GetBoundActionValue(StickAction).Get<float>() * AbyssPcInput_ZoomStickNotchesPerSecond
			* RealDeltaSeconds;
	}
	if (!FMath::IsNearlyZero(Notches))
	{
		ApplyAbyssZoom(static_cast<float>(Notches));
	}
}

void AAbyssPlayerController::TickAbyssHolds(const abyss::Snapshot& Snap)
{
	if (bAbyssMouseHold)
	{
		FVector2D MousePosition;
		if (!IsInputKeyDown(EKeys::LeftMouseButton))
		{
			EndAbyssMouseHold();   // the release event was missed (focus loss, capture lost)
		}
		else if (GetAbyssMouseViewportPosition(MousePosition))
		{
			UpdateAbyssHold(MousePosition, AbyssPcInput_MousePointerId, AbyssMouseHoldTile);
		}
	}

	for (int32 TouchIndex = AbyssWorldTouches.Num() - 1; TouchIndex >= 0; --TouchIndex)
	{
		FAbyssWorldTouch& WorldTouch = AbyssWorldTouches[TouchIndex];
		float TouchX = 0.0f;
		float TouchY = 0.0f;
		bool bPressed = false;
		GetInputTouchState(WorldTouch.Finger, TouchX, TouchY, bPressed);
		if (!bPressed)
		{
			// The release never reached the binding (app deactivated, touch cancelled): end it here.
			if (WorldTouch.bHolding)
			{
				ReleaseAbyssHold(AbyssPcInput_TouchPointerId(WorldTouch.Finger));
			}
			AbyssWorldTouches.RemoveAt(TouchIndex);
			continue;
		}
		if (WorldTouch.bHolding && !WorldTouch.bInert)
		{
			// The camera follows the hero: re-pick under the (possibly still) finger every frame (world 7.2).
			UpdateAbyssHold(WorldTouch.Position, AbyssPcInput_TouchPointerId(WorldTouch.Finger), WorldTouch.LastHoldTile);
		}
	}
	if (AbyssWorldTouches.Num() < 2)
	{
		bAbyssPinching = false;
	}
}

void AAbyssPlayerController::TickAbyssHover(const abyss::Snapshot& Snap)
{
	UAbyssInputSubsystem* InputSubsystem = GetAbyssInputSubsystem();
	const bool bFakingTouch = FSlateApplication::IsInitialized() && FSlateApplication::Get().IsFakingTouchEvents();
	if (InputSubsystem == nullptr || bAbyssTouchLayout || bFakingTouch
		|| InputSubsystem->GetLastInputDevice() != EAbyssInputDevice::KeyboardMouse)
	{
		ClearAbyssHover();
		return;
	}
	FVector2D MousePosition;
	if (!GetAbyssMouseViewportPosition(MousePosition) || !IsAbyssCursorOverWorld())
	{
		ClearAbyssHover();
		return;
	}
	const FAbyssPickResult Pick = AbyssPicking::PickEntity(*this, MousePosition, AbyssPcInput_MouseSlopSlate * GetUiDpiScale());
	InputSubsystem->SetHoveredEntity(Pick.Id);
	switch (Pick.Action)
	{
	case EAbyssPickAction::Attack:
		CurrentMouseCursor = EMouseCursor::Crosshairs;
		break;
	case EAbyssPickAction::Interact:
	case EAbyssPickAction::PickUp:
	case EAbyssPickAction::WalkTo:
		CurrentMouseCursor = EMouseCursor::Hand;
		break;
	case EAbyssPickAction::None:
		CurrentMouseCursor = DefaultMouseCursor;
		break;
	}
}

void AAbyssPlayerController::ClearAbyssHover()
{
	if (UAbyssInputSubsystem* InputSubsystem = GetAbyssInputSubsystem())
	{
		InputSubsystem->SetHoveredEntity(abyss::kNoEntity);
	}
	CurrentMouseCursor = DefaultMouseCursor;
}

void AAbyssPlayerController::ResetAbyssMovementState()
{
	bAbyssMoveSentNonZero = false;
	bAbyssMoveResyncPending = false;
	bAbyssWasInputBlocked = false;
	AbyssLastReleasedPointer = INDEX_NONE;
}

// =====================================================================================================================
// Discrete actions
// =====================================================================================================================

void AAbyssPlayerController::InjectAbyssPress(EAbyssInputAction Action, EAbyssInputDevice Device)
{
	const UAbyssInputConfig* ActionConfig = AbyssInputConfig;
	const UInputAction* InputAction = ActionConfig != nullptr ? ActionConfig->GetAction(Action) : nullptr;
	UEnhancedInputLocalPlayerSubsystem* EnhancedSubsystem = GetAbyssEnhancedInputSubsystem();
	if (InputAction == nullptr || EnhancedSubsystem == nullptr)
	{
		UE_LOG(LogAbyssInput, Verbose, TEXT("InjectAbyssPress(%s): no input action / Enhanced Input subsystem"),
			AbyssInputActionName(Action));
		return;
	}
	const int32 ActionIndex = static_cast<int32>(Action);
	if (ActionIndex >= 0 && ActionIndex < AbyssInputActionCount)
	{
		AbyssInjectedSource[ActionIndex] = static_cast<uint8>(1 + static_cast<uint8>(Device));
	}
	EnhancedSubsystem->InjectInputForAction(InputAction, FInputActionValue(true), {}, {});
}

void AAbyssPlayerController::HandleAbyssActionStarted(EAbyssInputAction Action)
{
	const int32 ActionIndex = static_cast<int32>(Action);
	uint8 InjectedSource = 0;
	if (ActionIndex >= 0 && ActionIndex < AbyssInputActionCount)
	{
		InjectedSource = AbyssInjectedSource[ActionIndex];
		AbyssInjectedSource[ActionIndex] = 0;
	}
	const bool bFromInjection = InjectedSource != 0;
	UAbyssInputSubsystem* InputSubsystem = GetAbyssInputSubsystem();
	// A key press: the Slate pre-processor already reported its device (it runs before the viewport routes the key).
	const EAbyssInputDevice Device = bFromInjection ? static_cast<EAbyssInputDevice>(InjectedSource - 1)
		: (InputSubsystem != nullptr ? InputSubsystem->GetLastInputDevice() : EAbyssInputDevice::KeyboardMouse);
	UAbyssGameInstance* AbyssGame = GetAbyssGameInstance();

	// ---- flow: any app state ----
	if (Action == EAbyssInputAction::Back)
	{
		// U4: close the top panel, else open the system menu (the UI decides; C12 blocks the menu while Dying).
		IAbyssUiRoot* UiRoot = AbyssGame != nullptr ? AbyssGame->GetUiRoot() : nullptr;
		if (UiRoot == nullptr || !UiRoot->HandleBack())
		{
			UE_LOG(LogAbyssInput, Verbose, TEXT("Back not consumed (no UI root or nothing to close)"));
		}
		return;
	}
	if (Action == EAbyssInputAction::StoryAdvance || Action == EAbyssInputAction::StorySkip)
	{
		if (InputSubsystem != nullptr && AbyssInputAppState == EAbyssAppState::InGame)
		{
			FAbyssUiInputRequest Request;
			Request.Kind = Action == EAbyssInputAction::StoryAdvance ? EAbyssUiRequest::StoryAdvance : EAbyssUiRequest::StorySkip;
			Request.Device = Device;
			InputSubsystem->RouteUiRequest(Request);
		}
		return;
	}

	// ---- gameplay and HUD panels: in game, never under a story beat (save-ui-input 1.5 freeze gates) ----
	const abyss::Snapshot* Snap = GetAbyssGameSnapshot();
	if (Snap == nullptr || Snap->cinematic)
	{
		return;
	}
	const int32 SkillSlot = AbyssInputSkillSlot(Action);
	if (SkillSlot != INDEX_NONE)
	{
		CastAbyssSkill(SkillSlot, Device, bFromInjection);
		return;
	}
	switch (Action)
	{
	case EAbyssInputAction::Dodge:
		// C8 / combat 8.1 FIX: the current move direction (keys, stick, joystick), else zero = the hero's facing.
		SubmitCommand(abyss::CmdDodge{GetAbyssCoreScreenDir()});
		return;
	case EAbyssInputAction::TargetCycle:
		SubmitCommand(abyss::CmdCycleTarget{});
		return;
	case EAbyssInputAction::ToggleAutoCombat:
		SubmitCommand(abyss::CmdToggleAutoCombat{});
		return;
	case EAbyssInputAction::CycleAutoLoot:
		SubmitCommand(abyss::CmdCycleAutoLoot{});
		return;
	case EAbyssInputAction::AltClick:      // RMB on the world (save-ui-input 5.3 step 2; touch never has one)
	case EAbyssInputAction::TownPortal:    // W3: the core refuses and logs (dead, busy, already at the camp)
		SubmitCommand(abyss::CmdTownPortal{});
		return;
	case EAbyssInputAction::Interact:      // world 7.4: nearest in-range target (FindInteractTarget)
		SubmitCommand(abyss::CmdInteract{abyss::kNoEntity});
		return;
	case EAbyssInputAction::PotionHp:      // I4
		SubmitCommand(abyss::CmdUsePotion{abyss::PotionSlot::Hp});
		return;
	case EAbyssInputAction::PotionMp:
		SubmitCommand(abyss::CmdUsePotion{abyss::PotionSlot::Mp});
		return;
	case EAbyssInputAction::PanelInventory:
	case EAbyssInputAction::PanelCharacter:
	case EAbyssInputAction::PanelSkills:
	case EAbyssInputAction::PanelQuestLog:
	case EAbyssInputAction::PanelWorldMap:
	case EAbyssInputAction::PanelAchievements:
	case EAbyssInputAction::PanelSettings:
	case EAbyssInputAction::PanelPets:
		ToggleAbyssPanel(Action, Device);
		return;
	default:
		return;
	}
}

void AAbyssPlayerController::CastAbyssSkill(int32 Slot, EAbyssInputDevice Device, bool bFromInjection)
{
	const abyss::Snapshot* Snap = GetAbyssGameSnapshot();
	if (Snap == nullptr)
	{
		return;
	}
	abyss::CmdCastSkill SkillCommand;
	SkillCommand.slot = Slot;
	SkillCommand.target = abyss::kNoEntity;   // the core's preferred target (lock, else nearest; combat 9.2)
	// C8 teleport aim on touch / pad: the joystick / stick direction (screen space for the core's ScreenDirToTile).
	SkillCommand.stickDir = GetAbyssCoreScreenDir();
	// Desktop keyboard: the ground tile under the cursor (teleport, combat 1085). Never for HUD / touch presses: their
	// "cursor" is the button itself.
	if (!bFromInjection && Device == EAbyssInputDevice::KeyboardMouse && !bAbyssTouchLayout)
	{
		FVector2D MousePosition;
		abyss::Vec2 CursorTile;
		if (GetAbyssMouseViewportPosition(MousePosition) && PickAbyssGroundTile(MousePosition, *Snap, CursorTile))
		{
			SkillCommand.hasPoint = true;
			SkillCommand.point = CursorTile;
		}
	}
	SubmitCommand(SkillCommand);
}

void AAbyssPlayerController::ToggleAbyssPanel(EAbyssInputAction Action, EAbyssInputDevice Device)
{
	abyss::PanelId Panel = abyss::PanelId::Inventory;
	if (!AbyssInputActionToPanel(Action, Panel))
	{
		return;
	}
	const abyss::Snapshot* Snap = GetAbyssGameSnapshot();
	if (Panel == abyss::PanelId::Pets && (Snap == nullptr || Snap->pets == nullptr || Snap->pets->Owned().empty()))
	{
		return;   // U6: the ley-beast panel exists once a beast is owned
	}
	if (UAbyssInputSubsystem* InputSubsystem = GetAbyssInputSubsystem())
	{
		FAbyssUiInputRequest Request;
		Request.Kind = EAbyssUiRequest::TogglePanel;
		Request.Panel = Panel;
		Request.Device = Device;
		InputSubsystem->RouteUiRequest(Request);   // the UI owns open state and reports it (CmdOpenPanel / CmdClosePanel)
	}
}

// =====================================================================================================================
// Mouse
// =====================================================================================================================

void AAbyssPlayerController::HandleAbyssClickStarted()
{
	if (GetAbyssGameSnapshot() == nullptr)
	{
		return;
	}
	FVector2D MousePosition;
	if (!GetAbyssMouseViewportPosition(MousePosition))
	{
		return;
	}
	EndAbyssMouseHold();   // a hold whose release was missed
	FIntPoint HoldTile = AbyssPcInput_NoTile;
	bAbyssMouseHold = PressAbyssWorld(MousePosition, AbyssPcInput_MousePointerId, /*bTouchPress*/ false, HoldTile);
	AbyssMouseHoldTile = HoldTile;
}

void AAbyssPlayerController::HandleAbyssClickCompleted()
{
	EndAbyssMouseHold();
}

void AAbyssPlayerController::EndAbyssMouseHold()
{
	if (bAbyssMouseHold)
	{
		bAbyssMouseHold = false;
		ReleaseAbyssHold(AbyssPcInput_MousePointerId);
	}
	AbyssMouseHoldTile = AbyssPcInput_NoTile;
}

// =====================================================================================================================
// Touch (world fingers; ue58-platform.md 8.3, world-map-nav.md 7.3)
// =====================================================================================================================

void AAbyssPlayerController::HandleAbyssTouchPressed(ETouchIndex::Type FingerIndex, FVector Location)
{
	if (UAbyssInputSubsystem* InputSubsystem = GetAbyssInputSubsystem())
	{
		InputSubsystem->NotifyInputDevice(EAbyssInputDevice::Touch);
	}
	// A stale record of this finger (its release was missed) ends first.
	for (int32 TouchIndex = AbyssWorldTouches.Num() - 1; TouchIndex >= 0; --TouchIndex)
	{
		if (AbyssWorldTouches[TouchIndex].Finger == FingerIndex)
		{
			if (AbyssWorldTouches[TouchIndex].bHolding)
			{
				ReleaseAbyssHold(AbyssPcInput_TouchPointerId(FingerIndex));
			}
			AbyssWorldTouches.RemoveAt(TouchIndex);
		}
	}
	if (GetAbyssGameSnapshot() == nullptr)
	{
		return;
	}

	FAbyssWorldTouch NewTouch;
	NewTouch.Finger = FingerIndex;
	NewTouch.Position = FVector2D(Location.X, Location.Y);
	AbyssWorldTouches.Add(NewTouch);
	if (AbyssWorldTouches.Num() >= 2)
	{
		BeginAbyssPinch();   // two world fingers: camera zoom (W1), never a move
		return;
	}
	// One finger: the same press as a left click, hold-to-move from the first frame (world 7.3, no long-press delay).
	FIntPoint HoldTile = AbyssPcInput_NoTile;
	const bool bHolding = PressAbyssWorld(NewTouch.Position, AbyssPcInput_TouchPointerId(FingerIndex), /*bTouchPress*/ true, HoldTile);
	FAbyssWorldTouch& Stored = AbyssWorldTouches.Last();
	Stored.bHolding = bHolding;
	Stored.LastHoldTile = HoldTile;
}

void AAbyssPlayerController::HandleAbyssTouchMoved(ETouchIndex::Type FingerIndex, FVector Location)
{
	FAbyssWorldTouch* Moved = AbyssWorldTouches.FindByPredicate(
		[FingerIndex](const FAbyssWorldTouch& Candidate) { return Candidate.Finger == FingerIndex; });
	if (Moved == nullptr)
	{
		return;
	}
	Moved->Position = FVector2D(Location.X, Location.Y);
	if (!bAbyssPinching || AbyssWorldTouches.Num() < 2)
	{
		return;   // a holding finger's tile is streamed by TickAbyssHolds
	}
	const double NewDistance = FVector2D::Distance(AbyssWorldTouches[0].Position, AbyssWorldTouches[1].Position);
	if (AbyssPinchDistance > 1.0 && NewDistance > 1.0)
	{
		const abyss::Snapshot* Snap = GetAbyssGameSnapshot();
		if (Snap != nullptr && !Snap->cinematic)
		{
			ApplyAbyssZoom(AbyssInputMath::PinchRatioToZoomSteps(static_cast<float>(NewDistance / AbyssPinchDistance)));
		}
	}
	AbyssPinchDistance = NewDistance;
}

void AAbyssPlayerController::HandleAbyssTouchReleased(ETouchIndex::Type FingerIndex, FVector Location)
{
	const int32 TouchIndex = AbyssWorldTouches.IndexOfByPredicate(
		[FingerIndex](const FAbyssWorldTouch& Candidate) { return Candidate.Finger == FingerIndex; });
	if (TouchIndex == INDEX_NONE)
	{
		return;
	}
	if (AbyssWorldTouches[TouchIndex].bHolding)
	{
		ReleaseAbyssHold(AbyssPcInput_TouchPointerId(FingerIndex));
	}
	AbyssWorldTouches.RemoveAt(TouchIndex);
	if (AbyssWorldTouches.Num() < 2)
	{
		bAbyssPinching = false;   // the remaining finger stays inert until it is lifted
		AbyssPinchDistance = 0.0;
	}
}

void AAbyssPlayerController::BeginAbyssPinch()
{
	bAbyssPinching = true;
	for (FAbyssWorldTouch& WorldTouch : AbyssWorldTouches)
	{
		if (WorldTouch.bHolding)
		{
			ReleaseAbyssHold(AbyssPcInput_TouchPointerId(WorldTouch.Finger));
			WorldTouch.bHolding = false;
		}
		WorldTouch.bInert = true;
	}
	AbyssPinchDistance = AbyssWorldTouches.Num() >= 2
		? FVector2D::Distance(AbyssWorldTouches[0].Position, AbyssWorldTouches[1].Position)
		: 0.0;
}

void AAbyssPlayerController::CancelAbyssWorldPointers()
{
	EndAbyssMouseHold();
	for (const FAbyssWorldTouch& WorldTouch : AbyssWorldTouches)
	{
		if (WorldTouch.bHolding)
		{
			ReleaseAbyssHold(AbyssPcInput_TouchPointerId(WorldTouch.Finger));
		}
	}
	AbyssWorldTouches.Reset();
	bAbyssPinching = false;
	AbyssPinchDistance = 0.0;
}

// =====================================================================================================================
// World press / hold
// =====================================================================================================================

bool AAbyssPlayerController::PressAbyssWorld(const FVector2D& ScreenPosition, int32 PointerId, bool bTouchPress,
	FIntPoint& OutHoldTile)
{
	OutHoldTile = AbyssPcInput_NoTile;
	const abyss::Snapshot* Snap = GetAbyssGameSnapshot();
	if (Snap == nullptr)
	{
		return false;
	}
	if (Snap->cinematic)
	{
		// The story overlay normally takes its own taps; one that reaches the world advances the beat (StoryScene parity).
		if (UAbyssInputSubsystem* InputSubsystem = GetAbyssInputSubsystem())
		{
			FAbyssUiInputRequest Request;
			Request.Kind = EAbyssUiRequest::StoryAdvance;
			Request.Device = bTouchPress ? EAbyssInputDevice::Touch : EAbyssInputDevice::KeyboardMouse;
			InputSubsystem->RouteUiRequest(Request);
		}
		return false;
	}
	if (AbyssLastReleasedPointer == PointerId)
	{
		AbyssLastReleasedPointer = INDEX_NONE;   // a new press supersedes the pending release re-send
	}

	// 1. An entity under the pointer (projected bounds, web press priority; world 1.4 "substitute the actor's tile").
	const float SlopPixels = (bTouchPress ? AbyssPcInput_TouchSlopSlate : AbyssPcInput_MouseSlopSlate) * GetUiDpiScale();
	const FAbyssPickResult Pick = AbyssPicking::PickEntity(*this, ScreenPosition, SlopPixels);
	switch (Pick.Action)
	{
	case EAbyssPickAction::Attack:
		SubmitCommand(abyss::CmdAttackTarget{Pick.Id});   // C7: the core approaches to attack range
		return false;
	case EAbyssPickAction::Interact:
		SubmitCommand(abyss::CmdInteract{Pick.Id});       // W8 / Q6: walk there, act on arrival
		return false;
	case EAbyssPickAction::PickUp:
		SubmitCommand(abyss::CmdPickUp{Pick.Id});         // I10 Q22: walk there, pick up on arrival
		return false;
	case EAbyssPickAction::WalkTo:
	{
		// Proximity-only objects: a press at their tile (the core chain walks there and starts the hold).
		const abyss::Vec2 Tile = AbyssInputMath::ClampTileToZone(Pick.Tile, Snap->zone.cols, Snap->zone.rows);
		SubmitCommand(abyss::CmdPointerPress{Tile, abyss::PointerButton::Primary, PointerId});
		OutHoldTile = AbyssPcInput_RoundedTile(Tile);
		return true;
	}
	case EAbyssPickAction::None:
		break;
	}

	// 2. The ground: the core's press chain keeps the web's 1.5-tile tolerance (loot, NPC, monster, exit, then path +
	// hold-to-move, world 7.1).
	abyss::Vec2 GroundTile;
	if (!PickAbyssGroundTile(ScreenPosition, *Snap, GroundTile))
	{
		return false;
	}
	SubmitCommand(abyss::CmdPointerPress{GroundTile, abyss::PointerButton::Primary, PointerId});
	OutHoldTile = AbyssPcInput_RoundedTile(GroundTile);
	return true;
}

void AAbyssPlayerController::UpdateAbyssHold(const FVector2D& ScreenPosition, int32 PointerId, FIntPoint& InOutLastTile)
{
	const abyss::Snapshot* Snap = GetAbyssGameSnapshot();
	abyss::Vec2 Tile;
	if (Snap == nullptr || !PickAbyssGroundTile(ScreenPosition, *Snap, Tile))
	{
		return;
	}
	const FIntPoint Rounded = AbyssPcInput_RoundedTile(Tile);
	if (Rounded == InOutLastTile)
	{
		return;   // the core keeps the last pointer tile; it re-paths on its own 120 ms cadence
	}
	InOutLastTile = Rounded;
	SubmitCommand(abyss::CmdPointerHold{Tile, PointerId, true});
}

void AAbyssPlayerController::ReleaseAbyssHold(int32 PointerId)
{
	SubmitCommand(abyss::CmdPointerHold{abyss::Vec2(), PointerId, false});
	// A release swallowed by an input block (modal panel) would leave the core's hold active: re-send it on unblock.
	const abyss::Snapshot* Snap = GetAbyssGameSnapshot();
	AbyssLastReleasedPointer = (Snap != nullptr && Snap->inputBlocked) ? PointerId : INDEX_NONE;
}

// =====================================================================================================================
// Helpers
// =====================================================================================================================

const abyss::Snapshot* AAbyssPlayerController::GetAbyssGameSnapshot() const
{
	if (AbyssInputAppState != EAbyssAppState::InGame)
	{
		return nullptr;
	}
	const UAbyssGameInstance* AbyssGame = GetAbyssGameInstance();
	return AbyssGame != nullptr ? AbyssGame->GetSnapshot() : nullptr;
}

UEnhancedInputLocalPlayerSubsystem* AAbyssPlayerController::GetAbyssEnhancedInputSubsystem() const
{
	const ULocalPlayer* OwningLocalPlayer = GetLocalPlayer();
	return OwningLocalPlayer != nullptr ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(OwningLocalPlayer)
										: nullptr;
}

UAbyssInputSubsystem* AAbyssPlayerController::GetAbyssInputSubsystem() const
{
	return UAbyssInputSubsystem::Get(this);
}

double AAbyssPlayerController::GetAbyssCameraYaw() const
{
	if (PlayerCameraManager != nullptr)
	{
		return PlayerCameraManager->GetCameraRotation().Yaw;
	}
	return AbyssInputMath::DefaultCameraYawDegrees;
}

FVector2D AAbyssPlayerController::GetAbyssMoveInput() const
{
	const UEnhancedInputComponent* EnhancedComponent = Cast<UEnhancedInputComponent>(InputComponent);
	const UAbyssInputConfig* ActionConfig = AbyssInputConfig;
	const UInputAction* MoveAction = ActionConfig != nullptr ? ActionConfig->GetAction(EAbyssInputAction::Move) : nullptr;
	if (EnhancedComponent == nullptr || MoveAction == nullptr)
	{
		return FVector2D::ZeroVector;
	}
	return EnhancedComponent->GetBoundActionValue(MoveAction).Get<FVector2D>();
}

abyss::Vec2 AAbyssPlayerController::GetAbyssCoreScreenDir() const
{
	return AbyssInputMath::ScreenInputToCoreScreen(GetAbyssMoveInput(), GetAbyssCameraYaw());
}

bool AAbyssPlayerController::GetAbyssMouseViewportPosition(FVector2D& OutPosition) const
{
	float MouseX = 0.0f;
	float MouseY = 0.0f;
	if (!GetMousePosition(MouseX, MouseY))
	{
		return false;
	}
	OutPosition = FVector2D(MouseX, MouseY);
	return true;
}

bool AAbyssPlayerController::PickAbyssGroundTile(const FVector2D& ScreenPosition, const abyss::Snapshot& Snap,
	abyss::Vec2& OutTile) const
{
	abyss::Vec2 Tile;
	if (!DeprojectToTile(ScreenPosition, Tile))
	{
		return false;
	}
	OutTile = AbyssInputMath::ClampTileToZone(Tile, Snap.zone.cols, Snap.zone.rows);
	return true;
}

bool AAbyssPlayerController::IsAbyssCursorOverWorld() const
{
	if (!FSlateApplication::IsInitialized())
	{
		return true;
	}
	FSlateApplication& SlateApp = FSlateApplication::Get();
	const TSharedPtr<SViewport> GameViewportWidget = SlateApp.GetGameViewport();
	if (!GameViewportWidget.IsValid())
	{
		return true;
	}
	const FVector2D CursorPosition = SlateApp.GetCursorPos();
	const FWidgetPath WidgetsUnderCursor = SlateApp.LocateWindowUnderMouse(CursorPosition, SlateApp.GetInteractiveTopLevelWindows());
	if (!WidgetsUnderCursor.IsValid())
	{
		return false;   // outside every game window
	}
	// The deepest hit-testable widget: the viewport itself (HUD roots are SelfHitTestInvisible) or one of the engine's own
	// full-screen game-layer containers; anything else is UI on top of the world.
	const SWidget& Leaf = WidgetsUnderCursor.Widgets.Last().Widget.Get();
	if (&Leaf == static_cast<const SWidget*>(GameViewportWidget.Get()))
	{
		return true;
	}
	static const FName GameLayerManagerType(TEXT("SGameLayerManager"));
	static const FName DebugCanvasType(TEXT("SDebugCanvas"));
	const FName LeafType = Leaf.GetType();
	return LeafType == GameLayerManagerType || LeafType == DebugCanvasType;
}

AAbyssCameraRig* AAbyssPlayerController::FindAbyssCameraRig()
{
	if (AAbyssCameraRig* ViewRig = Cast<AAbyssCameraRig>(GetViewTarget()))
	{
		AbyssCameraRig = ViewRig;
		return ViewRig;
	}
	if (AAbyssCameraRig* Cached = AbyssCameraRig.Get())
	{
		return Cached;
	}
	if (UWorld* GameWorld = GetWorld())
	{
		for (TActorIterator<AAbyssCameraRig> It(GameWorld); It; ++It)
		{
			AbyssCameraRig = *It;
			return *It;
		}
	}
	return nullptr;
}

void AAbyssPlayerController::ApplyAbyssZoom(float Notches)
{
	if (FMath::IsNearlyZero(Notches))
	{
		return;
	}
	if (AAbyssCameraRig* Rig = FindAbyssCameraRig())
	{
		Rig->AddZoomInput(Notches);   // positive = zoom in; the rig clamps to W1's 0.75 .. 1.25 of the default distance
	}
}

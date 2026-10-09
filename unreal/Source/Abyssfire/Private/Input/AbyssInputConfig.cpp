#include "Input/AbyssInputConfig.h"

#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

namespace
{
	bool AbyssInputConfig_IsAnalog(EAbyssInputAction Action)
	{
		return Action == EAbyssInputAction::Move || Action == EAbyssInputAction::Zoom || Action == EAbyssInputAction::ZoomStick;
	}
}

UInputAction* UAbyssInputConfig::MakeAction(EAbyssInputAction Action)
{
	UInputAction* Created = NewObject<UInputAction>(this, FName(AbyssInputActionName(Action)), RF_Transient);
	switch (Action)
	{
	case EAbyssInputAction::Move:
		Created->ValueType = EInputActionValueType::Axis2D;
		// WASD: opposite keys cancel, diagonals add (web keyboard vector, save-ui-input 5.2 step 2); keyboard + stick +
		// injected joystick sum as well (the controller normalises the result - magnitude is ignored, Q15).
		Created->AccumulationBehavior = EInputActionAccumulationBehavior::Cumulative;
		break;
	case EAbyssInputAction::Zoom:
	case EAbyssInputAction::ZoomStick:
		Created->ValueType = EInputActionValueType::Axis1D;
		break;
	default:
		Created->ValueType = EInputActionValueType::Boolean;
		break;
	}
	// UInputAction::bConsumeInput stays at its default (true): the story context's Space / A / LMB / Esc mappings then
	// block the lower-priority gameplay bindings of the same keys while a beat plays.
	return Created;
}

void UAbyssInputConfig::MapPlainKey(UInputMappingContext* Context, EAbyssInputAction Action, const FKey& Key)
{
	Context->MapKey(GetAction(Action), Key);
	FKeyBindingRecord& Record = KeyBindings.AddDefaulted_GetRef();
	Record.Action = Action;
	Record.Key = Key;
	Record.bGamepad = Key.IsGamepadKey();
}

void UAbyssInputConfig::MapMoveKey(UInputMappingContext* Context, const FKey& Key, bool bSwizzle, bool bNegate)
{
	// MapKey returns a reference into the context's mapping array: finish this mapping before the next MapKey
	// (ue58-platform.md 8.1 pitfalls).
	FEnhancedActionKeyMapping& Mapping = Context->MapKey(GetAction(EAbyssInputAction::Move), Key);
	if (bSwizzle)
	{
		Mapping.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(Context));   // default order YXZ: the key drives Y
	}
	if (bNegate)
	{
		Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(Context));
	}
	FKeyBindingRecord& Record = KeyBindings.AddDefaulted_GetRef();
	Record.Action = EAbyssInputAction::Move;
	Record.Key = Key;
	Record.bGamepad = Key.IsGamepadKey();
}

void UAbyssInputConfig::MapStick(UInputMappingContext* Context, EAbyssInputAction Action, const FKey& Key, float DeadZone,
	bool bAxial)
{
	FEnhancedActionKeyMapping& Mapping = Context->MapKey(GetAction(Action), Key);
	UInputModifierDeadZone* DeadZoneModifier = NewObject<UInputModifierDeadZone>(Context);
	DeadZoneModifier->LowerThreshold = DeadZone;
	DeadZoneModifier->UpperThreshold = 1.0f;
	DeadZoneModifier->Type = bAxial ? EDeadZoneType::Axial : EDeadZoneType::Radial;
	Mapping.Modifiers.Add(DeadZoneModifier);
	FKeyBindingRecord& Record = KeyBindings.AddDefaulted_GetRef();
	Record.Action = Action;
	Record.Key = Key;
	Record.bGamepad = true;
}

void UAbyssInputConfig::Build()
{
	if (bBuilt)
	{
		return;
	}
	Actions.Reset();
	Actions.Reserve(AbyssInputActionCount);
	for (int32 ActionIndex = 0; ActionIndex < AbyssInputActionCount; ++ActionIndex)
	{
		Actions.Add(MakeAction(static_cast<EAbyssInputAction>(ActionIndex)));
	}
	KeyBindings.Reset();

	// ---- global: Back (U4). Android back and the pad Start / Menu button behave like Esc (P13 pause menu). ----
	GlobalContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Global"), RF_Transient);
	MapPlainKey(GlobalContext, EAbyssInputAction::Back, EKeys::Escape);
	MapPlainKey(GlobalContext, EAbyssInputAction::Back, EKeys::Android_Back);
	MapPlainKey(GlobalContext, EAbyssInputAction::Back, EKeys::Gamepad_Special_Right);

	// ---- main menu on a pad: B = back (in game B is dodge). ----
	MenuPadContext = NewObject<UInputMappingContext>(this, TEXT("IMC_MenuPad"), RF_Transient);
	MapPlainKey(MenuPadContext, EAbyssInputAction::Back, EKeys::Gamepad_FaceButton_Right);

	// ---- keyboard + mouse (save-ui-input 5.1; I4; world 7.4) ----
	KeyboardMouseContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Gameplay_KBM"), RF_Transient);
	{
		UInputMappingContext* Kbm = KeyboardMouseContext;
		// UE axis convention: +Y = screen up (W), +X = screen right (D).
		MapMoveKey(Kbm, EKeys::W, /*bSwizzle*/ true, /*bNegate*/ false);
		MapMoveKey(Kbm, EKeys::S, true, true);
		MapMoveKey(Kbm, EKeys::A, false, true);
		MapMoveKey(Kbm, EKeys::D, false, false);
		MapMoveKey(Kbm, EKeys::Up, true, false);
		MapMoveKey(Kbm, EKeys::Down, true, true);
		MapMoveKey(Kbm, EKeys::Left, false, true);
		MapMoveKey(Kbm, EKeys::Right, false, false);

		MapPlainKey(Kbm, EAbyssInputAction::Click, EKeys::LeftMouseButton);
		MapPlainKey(Kbm, EAbyssInputAction::AltClick, EKeys::RightMouseButton);
		MapPlainKey(Kbm, EAbyssInputAction::Zoom, EKeys::MouseWheelAxis);

		MapPlainKey(Kbm, EAbyssInputAction::Skill1, EKeys::One);
		MapPlainKey(Kbm, EAbyssInputAction::Skill2, EKeys::Two);
		MapPlainKey(Kbm, EAbyssInputAction::Skill3, EKeys::Three);
		MapPlainKey(Kbm, EAbyssInputAction::Skill4, EKeys::Four);
		MapPlainKey(Kbm, EAbyssInputAction::Skill5, EKeys::Five);
		MapPlainKey(Kbm, EAbyssInputAction::Skill6, EKeys::Six);
		MapPlainKey(Kbm, EAbyssInputAction::Dodge, EKeys::SpaceBar);
		MapPlainKey(Kbm, EAbyssInputAction::TargetCycle, EKeys::T);
		MapPlainKey(Kbm, EAbyssInputAction::ToggleAutoCombat, EKeys::Tab);
		MapPlainKey(Kbm, EAbyssInputAction::TownPortal, EKeys::R);
		MapPlainKey(Kbm, EAbyssInputAction::Interact, EKeys::F);
		MapPlainKey(Kbm, EAbyssInputAction::PotionHp, EKeys::Q);
		MapPlainKey(Kbm, EAbyssInputAction::PotionMp, EKeys::E);

		MapPlainKey(Kbm, EAbyssInputAction::PanelInventory, EKeys::I);
		MapPlainKey(Kbm, EAbyssInputAction::PanelCharacter, EKeys::C);
		MapPlainKey(Kbm, EAbyssInputAction::PanelSkills, EKeys::K);
		MapPlainKey(Kbm, EAbyssInputAction::PanelQuestLog, EKeys::J);
		MapPlainKey(Kbm, EAbyssInputAction::PanelWorldMap, EKeys::M);
		MapPlainKey(Kbm, EAbyssInputAction::PanelAchievements, EKeys::V);
		MapPlainKey(Kbm, EAbyssInputAction::PanelSettings, EKeys::O);
		MapPlainKey(Kbm, EAbyssInputAction::PanelPets, EKeys::P);
	}

	// ---- gamepad (save-ui-input 5.5 web layout + world 7.4 + P13) ----
	GamepadContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Gameplay_Gamepad"), RF_Transient);
	{
		UInputMappingContext* Pad = GamepadContext;
		MapStick(Pad, EAbyssInputAction::Move, EKeys::Gamepad_Left2D, 0.18f, /*bAxial*/ false);
		MapStick(Pad, EAbyssInputAction::ZoomStick, EKeys::Gamepad_RightY, 0.25f, /*bAxial*/ true);

		MapPlainKey(Pad, EAbyssInputAction::Skill1, EKeys::Gamepad_FaceButton_Bottom);
		MapPlainKey(Pad, EAbyssInputAction::Skill2, EKeys::Gamepad_FaceButton_Left);
		MapPlainKey(Pad, EAbyssInputAction::Skill3, EKeys::Gamepad_FaceButton_Top);
		MapPlainKey(Pad, EAbyssInputAction::Skill4, EKeys::Gamepad_RightShoulder);
		MapPlainKey(Pad, EAbyssInputAction::Skill5, EKeys::Gamepad_LeftThumbstick);
		MapPlainKey(Pad, EAbyssInputAction::Skill6, EKeys::Gamepad_RightThumbstick);
		MapPlainKey(Pad, EAbyssInputAction::Dodge, EKeys::Gamepad_FaceButton_Right);
		MapPlainKey(Pad, EAbyssInputAction::TargetCycle, EKeys::Gamepad_LeftShoulder);
		MapPlainKey(Pad, EAbyssInputAction::Interact, EKeys::Gamepad_RightTrigger);
		MapPlainKey(Pad, EAbyssInputAction::PotionHp, EKeys::Gamepad_LeftTrigger);
		MapPlainKey(Pad, EAbyssInputAction::PotionMp, EKeys::Gamepad_DPad_Left);
		MapPlainKey(Pad, EAbyssInputAction::ToggleAutoCombat, EKeys::Gamepad_DPad_Up);
		MapPlainKey(Pad, EAbyssInputAction::CycleAutoLoot, EKeys::Gamepad_DPad_Right);
		MapPlainKey(Pad, EAbyssInputAction::TownPortal, EKeys::Gamepad_DPad_Down);
		MapPlainKey(Pad, EAbyssInputAction::PanelWorldMap, EKeys::Gamepad_Special_Left);
	}

	// ---- story beats (StoryDirector: the UI's overlay decides reveal vs advance) ----
	StoryContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Story"), RF_Transient);
	MapPlainKey(StoryContext, EAbyssInputAction::StoryAdvance, EKeys::SpaceBar);
	MapPlainKey(StoryContext, EAbyssInputAction::StoryAdvance, EKeys::Enter);
	MapPlainKey(StoryContext, EAbyssInputAction::StoryAdvance, EKeys::LeftMouseButton);
	MapPlainKey(StoryContext, EAbyssInputAction::StoryAdvance, EKeys::Gamepad_FaceButton_Bottom);
	MapPlainKey(StoryContext, EAbyssInputAction::StorySkip, EKeys::Escape);
	MapPlainKey(StoryContext, EAbyssInputAction::StorySkip, EKeys::Gamepad_Special_Right);
	MapPlainKey(StoryContext, EAbyssInputAction::StorySkip, EKeys::Android_Back);

	bBuilt = true;
}

UInputAction* UAbyssInputConfig::GetAction(EAbyssInputAction Action) const
{
	const int32 ActionIndex = static_cast<int32>(Action);
	return Actions.IsValidIndex(ActionIndex) ? Actions[ActionIndex].Get() : nullptr;
}

bool UAbyssInputConfig::FindActionId(const UInputAction* Action, EAbyssInputAction& OutAction) const
{
	if (Action == nullptr)
	{
		return false;
	}
	for (int32 ActionIndex = 0; ActionIndex < Actions.Num(); ++ActionIndex)
	{
		if (Actions[ActionIndex].Get() == Action)
		{
			OutAction = static_cast<EAbyssInputAction>(ActionIndex);
			return true;
		}
	}
	return false;
}

FString UAbyssInputConfig::GetKeyHint(EAbyssInputAction Action, EAbyssInputDevice Device) const
{
	if (Device == EAbyssInputDevice::Touch || AbyssInputConfig_IsAnalog(Action))
	{
		return FString();
	}
	const bool bWantGamepad = Device == EAbyssInputDevice::Gamepad;
	for (const FKeyBindingRecord& Record : KeyBindings)
	{
		if (Record.Action == Action && Record.bGamepad == bWantGamepad)
		{
			return ShortKeyLabel(Record.Key);
		}
	}
	return FString();
}

FString UAbyssInputConfig::ShortKeyLabel(const FKey& Key)
{
	struct FKeyLabel
	{
		const FKey* Key;
		const TCHAR* Label;
	};
	static const FKeyLabel Labels[] = {
		{ &EKeys::One, TEXT("1") }, { &EKeys::Two, TEXT("2") }, { &EKeys::Three, TEXT("3") },
		{ &EKeys::Four, TEXT("4") }, { &EKeys::Five, TEXT("5") }, { &EKeys::Six, TEXT("6") },
		{ &EKeys::SpaceBar, TEXT("Space") }, { &EKeys::Escape, TEXT("Esc") }, { &EKeys::Tab, TEXT("Tab") },
		{ &EKeys::Enter, TEXT("Enter") }, { &EKeys::LeftMouseButton, TEXT("LMB") },
		{ &EKeys::RightMouseButton, TEXT("RMB") }, { &EKeys::MouseWheelAxis, TEXT("Wheel") },
		{ &EKeys::Up, TEXT("Up") }, { &EKeys::Down, TEXT("Down") }, { &EKeys::Left, TEXT("Left") },
		{ &EKeys::Right, TEXT("Right") },
		{ &EKeys::Gamepad_FaceButton_Bottom, TEXT("A") }, { &EKeys::Gamepad_FaceButton_Right, TEXT("B") },
		{ &EKeys::Gamepad_FaceButton_Left, TEXT("X") }, { &EKeys::Gamepad_FaceButton_Top, TEXT("Y") },
		{ &EKeys::Gamepad_LeftShoulder, TEXT("LB") }, { &EKeys::Gamepad_RightShoulder, TEXT("RB") },
		{ &EKeys::Gamepad_LeftTrigger, TEXT("LT") }, { &EKeys::Gamepad_RightTrigger, TEXT("RT") },
		{ &EKeys::Gamepad_LeftThumbstick, TEXT("L3") }, { &EKeys::Gamepad_RightThumbstick, TEXT("R3") },
		{ &EKeys::Gamepad_Special_Right, TEXT("Start") }, { &EKeys::Gamepad_Special_Left, TEXT("View") },
		{ &EKeys::Gamepad_DPad_Up, TEXT("D-Up") }, { &EKeys::Gamepad_DPad_Down, TEXT("D-Down") },
		{ &EKeys::Gamepad_DPad_Left, TEXT("D-Left") }, { &EKeys::Gamepad_DPad_Right, TEXT("D-Right") },
		{ &EKeys::Android_Back, TEXT("Back") },
	};
	for (const FKeyLabel& Entry : Labels)
	{
		if (*Entry.Key == Key)
		{
			return Entry.Label;
		}
	}
	return Key.GetFName().ToString();   // letters: "F", "Q", ...
}

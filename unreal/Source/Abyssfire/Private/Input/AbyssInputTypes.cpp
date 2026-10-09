#include "Input/AbyssInputTypes.h"

DEFINE_LOG_CATEGORY(LogAbyssInput);

bool AbyssInputActionToPanel(EAbyssInputAction Action, abyss::PanelId& OutPanel)
{
	switch (Action)
	{
	case EAbyssInputAction::PanelInventory:
		OutPanel = abyss::PanelId::Inventory;
		return true;
	case EAbyssInputAction::PanelCharacter:
		OutPanel = abyss::PanelId::Character;
		return true;
	case EAbyssInputAction::PanelSkills:
		OutPanel = abyss::PanelId::Skills;
		return true;
	case EAbyssInputAction::PanelQuestLog:
		OutPanel = abyss::PanelId::QuestLog;
		return true;
	case EAbyssInputAction::PanelWorldMap:
		OutPanel = abyss::PanelId::WorldMap;
		return true;
	case EAbyssInputAction::PanelAchievements:
		OutPanel = abyss::PanelId::Achievements;
		return true;
	case EAbyssInputAction::PanelSettings:
		OutPanel = abyss::PanelId::Settings;
		return true;
	case EAbyssInputAction::PanelPets:
		OutPanel = abyss::PanelId::Pets;
		return true;
	default:
		return false;
	}
}

const TCHAR* AbyssInputActionName(EAbyssInputAction Action)
{
	switch (Action)
	{
	case EAbyssInputAction::Move: return TEXT("IA_Move");
	case EAbyssInputAction::Zoom: return TEXT("IA_Zoom");
	case EAbyssInputAction::ZoomStick: return TEXT("IA_ZoomStick");
	case EAbyssInputAction::Click: return TEXT("IA_Click");
	case EAbyssInputAction::AltClick: return TEXT("IA_AltClick");
	case EAbyssInputAction::Back: return TEXT("IA_Back");
	case EAbyssInputAction::StoryAdvance: return TEXT("IA_StoryAdvance");
	case EAbyssInputAction::StorySkip: return TEXT("IA_StorySkip");
	case EAbyssInputAction::Skill1: return TEXT("IA_Skill1");
	case EAbyssInputAction::Skill2: return TEXT("IA_Skill2");
	case EAbyssInputAction::Skill3: return TEXT("IA_Skill3");
	case EAbyssInputAction::Skill4: return TEXT("IA_Skill4");
	case EAbyssInputAction::Skill5: return TEXT("IA_Skill5");
	case EAbyssInputAction::Skill6: return TEXT("IA_Skill6");
	case EAbyssInputAction::Dodge: return TEXT("IA_Dodge");
	case EAbyssInputAction::TargetCycle: return TEXT("IA_TargetCycle");
	case EAbyssInputAction::ToggleAutoCombat: return TEXT("IA_ToggleAuto");
	case EAbyssInputAction::CycleAutoLoot: return TEXT("IA_AutoLootCycle");
	case EAbyssInputAction::TownPortal: return TEXT("IA_TownPortal");
	case EAbyssInputAction::Interact: return TEXT("IA_Interact");
	case EAbyssInputAction::PotionHp: return TEXT("IA_PotionHp");
	case EAbyssInputAction::PotionMp: return TEXT("IA_PotionMp");
	case EAbyssInputAction::PanelInventory: return TEXT("IA_Panel_Inventory");
	case EAbyssInputAction::PanelCharacter: return TEXT("IA_Panel_Character");
	case EAbyssInputAction::PanelSkills: return TEXT("IA_Panel_Skills");
	case EAbyssInputAction::PanelQuestLog: return TEXT("IA_Panel_Quest");
	case EAbyssInputAction::PanelWorldMap: return TEXT("IA_Panel_Map");
	case EAbyssInputAction::PanelAchievements: return TEXT("IA_Panel_Achievements");
	case EAbyssInputAction::PanelSettings: return TEXT("IA_Panel_Settings");
	case EAbyssInputAction::PanelPets: return TEXT("IA_Panel_Pets");
	case EAbyssInputAction::Count: break;
	}
	return TEXT("IA_Invalid");
}

const TCHAR* LexToString(EAbyssInputDevice Device)
{
	switch (Device)
	{
	case EAbyssInputDevice::KeyboardMouse: return TEXT("KeyboardMouse");
	case EAbyssInputDevice::Gamepad: return TEXT("Gamepad");
	case EAbyssInputDevice::Touch: return TEXT("Touch");
	}
	return TEXT("?");
}

const TCHAR* LexToString(EAbyssUiRequest Request)
{
	switch (Request)
	{
	case EAbyssUiRequest::TogglePanel: return TEXT("TogglePanel");
	case EAbyssUiRequest::StoryAdvance: return TEXT("StoryAdvance");
	case EAbyssUiRequest::StorySkip: return TEXT("StorySkip");
	case EAbyssUiRequest::ToggleCombatLog: return TEXT("ToggleCombatLog");
	case EAbyssUiRequest::OpenSystemMenu: return TEXT("OpenSystemMenu");
	}
	return TEXT("?");
}

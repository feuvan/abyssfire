// Development console commands for the backbone (not in Shipping). They let the world / input / audio agents run a
// session before the UI exists: `abyss.NewGame mage 0`, `abyss.Continue 0`, `abyss.ReturnToMenu`, ...
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Abyssfire.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

#include <optional>
#include <string>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/sim/Commands.h"

#include "Framework/AbyssGameInstance.h"
#include "Framework/AbyssText.h"
#include "Platform/AbyssSettings.h"

namespace
{
	UAbyssGameInstance* AbyssDebugGameInstance(UWorld* InWorld)
	{
		return InWorld ? Cast<UAbyssGameInstance>(InWorld->GetGameInstance()) : nullptr;
	}

	template <class E>
	bool AbyssDebugParse(const TArray<FString>& Args, int32 Index, E& Out)
	{
		return Args.IsValidIndex(Index) && abyss::ParseEnum(AbyssText::ToStd(Args[Index].ToLower()), Out);
	}

	int32 AbyssDebugSlot(const TArray<FString>& Args, int32 Index)
	{
		return Args.IsValidIndex(Index) ? FCString::Atoi(*Args[Index]) : 0;
	}

	void AbyssDebugNewGame(const TArray<FString>& Args, UWorld* InWorld)
	{
		UAbyssGameInstance* GameInstance = AbyssDebugGameInstance(InWorld);
		if (GameInstance == nullptr)
		{
			return;
		}
		abyss::ClassId ParsedClass = abyss::ClassId::Warrior;
		AbyssDebugParse(Args, 0, ParsedClass);
		abyss::Difficulty ParsedDifficulty = abyss::Difficulty::Normal;
		AbyssDebugParse(Args, 2, ParsedDifficulty);
		GameInstance->StartNewGame(ParsedClass, AbyssDebugSlot(Args, 1), ParsedDifficulty);
	}

	void AbyssDebugContinue(const TArray<FString>& Args, UWorld* InWorld)
	{
		UAbyssGameInstance* GameInstance = AbyssDebugGameInstance(InWorld);
		if (GameInstance == nullptr)
		{
			return;
		}
		std::optional<abyss::Difficulty> Override;
		abyss::Difficulty ParsedDifficulty = abyss::Difficulty::Normal;
		if (AbyssDebugParse(Args, 1, ParsedDifficulty))
		{
			Override = ParsedDifficulty;
		}
		const bool bBackup = Args.Contains(TEXT("backup"));
		const FAbyssLoadResult Result = GameInstance->ContinueSlot(AbyssDebugSlot(Args, 0), Override, bBackup);
		UE_LOG(LogAbyss, Display, TEXT("abyss.Continue: ok %d, missing %d, error %s, backup available %d %s"),
			Result.bOk ? 1 : 0, Result.bSlotMissing ? 1 : 0, *AbyssText::ToFString(abyss::EnumName(Result.Error)),
			Result.bBackupAvailable ? 1 : 0, *Result.Message);
	}

	void AbyssDebugReturnToMenu(const TArray<FString>& Args, UWorld* InWorld)
	{
		if (UAbyssGameInstance* GameInstance = AbyssDebugGameInstance(InWorld))
		{
			GameInstance->ReturnToMainMenu();
		}
	}

	void AbyssDebugSave(const TArray<FString>& Args, UWorld* InWorld)
	{
		if (UAbyssGameInstance* GameInstance = AbyssDebugGameInstance(InWorld))
		{
			UE_LOG(LogAbyss, Display, TEXT("abyss.Save: %d"), GameInstance->SaveNow(/*bSync*/ true) ? 1 : 0);
		}
	}

	void AbyssDebugSlots(const TArray<FString>& Args, UWorld* InWorld)
	{
		UAbyssGameInstance* GameInstance = AbyssDebugGameInstance(InWorld);
		if (GameInstance == nullptr)
		{
			return;
		}
		std::vector<abyss::SaveSlotInfo> Slots;
		GameInstance->ListSlots(Slots);
		for (const abyss::SaveSlotInfo& Info : Slots)
		{
			UE_LOG(LogAbyss, Display, TEXT("slot %d: exists %d, %s Lv%d, %s, %s, selector %d"), Info.slot, Info.exists ? 1 : 0,
				*AbyssText::ToFString(abyss::EnumName(Info.classId)), Info.level, *AbyssText::ToFString(Info.mapId),
				*AbyssText::ToFString(abyss::EnumName(Info.difficulty)), Info.showDifficultySelector ? 1 : 0);
		}
	}

	void AbyssDebugLocale(const TArray<FString>& Args, UWorld* InWorld)
	{
		UAbyssGameInstance* GameInstance = AbyssDebugGameInstance(InWorld);
		abyss::LocaleId Parsed = abyss::LocaleId::ZhCN;
		if (GameInstance == nullptr || !Args.IsValidIndex(0) || !abyss::ParseEnum(AbyssText::ToStd(Args[0]), Parsed))
		{
			return;
		}
		FAbyssUserSettings NewSettings = GameInstance->GetUserSettings();
		NewSettings.Locale = Parsed;
		GameInstance->ApplyUserSettings(NewSettings);
	}

	// abyss.Debug <op> [arg] [value] [col] [row] -> abyss::CmdDebug (SimConfig::enableDebugCommands, non-Shipping).
	void AbyssDebugCommand(const TArray<FString>& Args, UWorld* InWorld)
	{
		UAbyssGameInstance* GameInstance = AbyssDebugGameInstance(InWorld);
		if (GameInstance == nullptr || !Args.IsValidIndex(0))
		{
			return;
		}
		abyss::CmdDebug Command;
		Command.op = AbyssText::ToStd(Args[0]);
		Command.arg = Args.IsValidIndex(1) ? AbyssText::ToStd(Args[1]) : std::string();
		Command.value = Args.IsValidIndex(2) ? FCString::Atod(*Args[2]) : 0.0;
		if (Args.IsValidIndex(4))
		{
			Command.pos = abyss::Vec2(FCString::Atod(*Args[3]), FCString::Atod(*Args[4]));
		}
		GameInstance->Submit(Command);
	}

	FAutoConsoleCommandWithWorldAndArgs GAbyssNewGameCommand(TEXT("abyss.NewGame"),
		TEXT("abyss.NewGame [warrior|mage|rogue] [slot 0..2] [normal|nightmare|hell] - start a new session"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&AbyssDebugNewGame));
	FAutoConsoleCommandWithWorldAndArgs GAbyssContinueCommand(TEXT("abyss.Continue"),
		TEXT("abyss.Continue [slot 0..2] [difficulty override] [backup] - load a slot"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&AbyssDebugContinue));
	FAutoConsoleCommandWithWorldAndArgs GAbyssReturnCommand(TEXT("abyss.ReturnToMenu"),
		TEXT("Save and end the session"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&AbyssDebugReturnToMenu));
	FAutoConsoleCommandWithWorldAndArgs GAbyssSaveCommand(TEXT("abyss.Save"),
		TEXT("Save the active slot now (synchronous)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&AbyssDebugSave));
	FAutoConsoleCommandWithWorldAndArgs GAbyssSlotsCommand(TEXT("abyss.Slots"),
		TEXT("List the save slots"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&AbyssDebugSlots));
	FAutoConsoleCommandWithWorldAndArgs GAbyssLocaleCommand(TEXT("abyss.Locale"),
		TEXT("abyss.Locale zh-CN|zh-TW|en - switch the UI language (persists settings.json)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&AbyssDebugLocale));
	FAutoConsoleCommandWithWorldAndArgs GAbyssDebugCommand(TEXT("abyss.Debug"),
		TEXT("abyss.Debug <op> [arg] [value] [col row] - submit abyss::CmdDebug (giveItem, addExp, addGold, teleport, killAll, freeze, ...)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&AbyssDebugCommand));
}

#endif // !UE_BUILD_SHIPPING

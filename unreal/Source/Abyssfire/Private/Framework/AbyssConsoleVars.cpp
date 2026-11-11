#include "Framework/AbyssConsoleVars.h"

#include "Abyssfire.h"
#include "HAL/IConsoleManager.h"

#include <string>

#include "Framework/AbyssText.h"
#include "World/AbyssQualityRules.h"

namespace
{
	AbyssQualityRules::FConsoleLedger& AbyssConsoleVars_Ledger()
	{
		static AbyssQualityRules::FConsoleLedger Ledger;
		return Ledger;
	}

	/** The variable, or nullptr after noting the failed lookup (one Warning per name). */
	IConsoleVariable* AbyssConsoleVars_Find(const TCHAR* Name, const std::string& Key)
	{
		// Not a per-frame path (the ledger's callers apply on change): no frequent-lookup tracking needed.
		IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Name, /*bTrackFrequentCalls*/ false);
		if (Variable == nullptr && AbyssConsoleVars_Ledger().NoteMissing(Key))
		{
			UE_LOG(LogAbyss, Warning, TEXT("Console variable '%s' does not exist on this engine: the setting is not applied"), Name);
		}
		return Variable;
	}
}

bool AbyssConsoleVars::Apply(const TCHAR* Name, int32 Value)
{
	const std::string Key = AbyssText::ToStd(FString(Name));
	IConsoleVariable* Variable = AbyssConsoleVars_Find(Name, Key);
	if (Variable == nullptr)
	{
		return false;
	}
	if (AbyssConsoleVars_Ledger().WantsChange(Key, static_cast<double>(Value))
		&& AbyssQualityRules::FConsoleLedger::NeedsWrite(static_cast<double>(Variable->GetInt()), static_cast<double>(Value)))
	{
		Variable->Set(Value, ECVF_SetByCode);
	}
	return true;
}

bool AbyssConsoleVars::Apply(const TCHAR* Name, float Value)
{
	const std::string Key = AbyssText::ToStd(FString(Name));
	IConsoleVariable* Variable = AbyssConsoleVars_Find(Name, Key);
	if (Variable == nullptr)
	{
		return false;
	}
	if (AbyssConsoleVars_Ledger().WantsChange(Key, static_cast<double>(Value))
		&& AbyssQualityRules::FConsoleLedger::NeedsWrite(static_cast<double>(Variable->GetFloat()), static_cast<double>(Value)))
	{
		Variable->Set(Value, ECVF_SetByCode);
	}
	return true;
}

IConsoleVariable* AbyssConsoleVars::FindForWrite(const TCHAR* Name)
{
	return AbyssConsoleVars_Find(Name, AbyssText::ToStd(FString(Name)));
}

void AbyssConsoleVars::ForgetWishes()
{
	AbyssConsoleVars_Ledger().ForgetWishes();
}

TArray<FString> AbyssConsoleVars::MissingNames()
{
	TArray<FString> Out;
	for (const std::string& Name : AbyssConsoleVars_Ledger().Missing())
	{
		Out.Add(AbyssText::ToFString(Name));
	}
	return Out;
}

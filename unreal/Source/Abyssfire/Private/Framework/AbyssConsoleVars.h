// The engine console variables the module sets (performance audit F1). One door for every write: a variable is written
// when the value we want changes and the engine's value differs (AbyssQualityRules::FConsoleLedger), so a console or
// device-profile edit is not overwritten every second, and a name the engine does not know logs ONE Warning naming it
// and is listed in the autotest's report.json ("missingConsoleVariables"). Game thread only.
#pragma once

#include "CoreMinimal.h"

class IConsoleVariable;

namespace AbyssConsoleVars
{
	/** Sets `Name` to `Value` (ECVF_SetByCode) under the ledger's rules. False when the engine has no such variable. */
	bool Apply(const TCHAR* Name, int32 Value);
	bool Apply(const TCHAR* Name, float Value);
	/** The variable for a write the caller decides itself (the autotest's t.MaxFPS cap); a failed lookup is noted like
	 *  Apply's. Not for per-frame reads. */
	IConsoleVariable* FindForWrite(const TCHAR* Name);
	/** A new world starts: the next Apply of each name is evaluated afresh (the ledger forgets what it last wanted). */
	void ForgetWishes();
	/** Names whose lookup failed since the process started, in first-seen order. */
	TArray<FString> MissingNames();
}

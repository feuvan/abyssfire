// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Misc/EngineVersionBase.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"

enum class EVersionComponent { Major, Minor, Patch, Changelist, Branch };
enum class EVersionComparison { Neither, First, Second };

class FEngineVersionBase
{
public:
	FEngineVersionBase();
	FEngineVersionBase(uint16 InMajor, uint16 InMinor, uint16 InPatch = 0, uint32 InChangelist = 0);
	uint32 GetChangelist() const;
	uint16 GetMajor() const { return Major; }
	uint16 GetMinor() const { return Minor; }
	uint16 GetPatch() const { return Patch; }
	bool IsLicenseeVersion() const;
	bool IsEmpty() const;
	bool HasChangelist() const;
	static EVersionComparison GetNewest(const FEngineVersionBase& First, const FEngineVersionBase& Second, EVersionComponent* OutComponent);
protected:
	uint16 Major = 0;
	uint16 Minor = 0;
	uint16 Patch = 0;
	uint32 Changelist = 0;
};

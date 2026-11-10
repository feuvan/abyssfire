// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Misc/EngineVersion.h (from UE 5.x source knowledge).
#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "Misc/EngineVersionBase.h"

class FEngineVersion : public FEngineVersionBase
{
public:
	FEngineVersion() = default;
	FEngineVersion(uint16 InMajor, uint16 InMinor, uint16 InPatch, uint32 InChangelist, const FString& InBranch);
	void Set(uint16 InMajor, uint16 InMinor, uint16 InPatch, uint32 InChangelist, const FString& InBranch);
	void Empty();
	bool ExactMatch(const FEngineVersion& Other) const;
	bool IsCompatibleWith(const FEngineVersionBase& Other) const;
	FString ToString(EVersionComponent LastComponent = EVersionComponent::Branch) const;
	const FString GetBranch() const;
	const FString& GetBranchDescriptor() const;
	static bool Parse(const FString& Text, FEngineVersion& OutVersion);
	static const FEngineVersion& Current();
	static const FEngineVersion& CompatibleWith();
};

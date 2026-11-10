// Stub SDK. Real header: Engine/Source/Runtime/CoreUObject/Public/UObject/Package.h (from UE 5.x source knowledge).
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/Class.h"

class UPackage : public UObject
{
	UE_STUB_CLASS_BODY(UPackage, UObject)
public:
	void SetDirtyFlag(bool bIsDirty);
	bool IsDirty() const;
	void FullyLoad();
	bool IsFullyLoaded() const;
	bool HasAnyPackageFlags(uint32 FlagsToCheck) const;
	void SetPackageFlags(uint32 NewFlags);
};

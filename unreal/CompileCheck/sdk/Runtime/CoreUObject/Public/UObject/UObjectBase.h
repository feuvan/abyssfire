// Stub SDK. Real header: Engine/Source/Runtime/CoreUObject/Public/UObject/UObjectBase.h (from UE 5.x source knowledge).
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

class UClass;
class UObject;
class UPackage;

class UObjectBase
{
protected:
	UObjectBase();
public:
	virtual ~UObjectBase();
	bool IsValidLowLevel() const;
	bool IsValidLowLevelFast(bool bRecursive = true) const;
	uint32 GetUniqueID() const;
	UClass* GetClass() const;
	UObject* GetOuter() const;
	FName GetFName() const;
	UPackage* GetExternalPackage() const;
	EObjectFlags GetFlags() const;
	void AtomicallySetFlags(EObjectFlags FlagsToAdd);
	void AtomicallyClearFlags(EObjectFlags FlagsToClear);
};

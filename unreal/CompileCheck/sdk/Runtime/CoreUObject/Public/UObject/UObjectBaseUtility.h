// Stub SDK. Real header: Engine/Source/Runtime/CoreUObject/Public/UObject/UObjectBaseUtility.h
// (from UE 5.x source knowledge).
#pragma once

#include "CoreMinimal.h"
#include "UObject/UObjectBase.h"
#include "UObject/ObjectMacros.h"

class UObjectBaseUtility : public UObjectBase
{
public:
	UObjectBaseUtility();
	void SetFlags(EObjectFlags NewFlags);
	void ClearFlags(EObjectFlags FlagsToClear);
	bool HasAnyFlags(EObjectFlags FlagsToCheck) const;
	bool HasAllFlags(EObjectFlags FlagsToCheck) const;
	EObjectFlags GetMaskedFlags(EObjectFlags Mask = RF_AllFlags) const;
	void MarkAsGarbage();
	void ClearGarbage();
	void AddToRoot();
	void RemoveFromRoot();
	bool IsRooted() const;
	bool IsUnreachable() const;
	bool IsNative() const;
	bool HasAnyInternalFlags(EInternalObjectFlags FlagsToCheck) const;
	FString GetFullName(const UObject* StopOuter = nullptr) const;
	FString GetPathName(const UObject* StopOuter = nullptr) const;
	void GetPathName(const UObject* StopOuter, FString& ResultString) const;
	FString GetFullGroupName(bool bStartWithOuter) const;
	FString GetName() const;
	void GetName(FString& ResultString) const;
	void AppendName(FString& ResultString) const;
	UPackage* GetOutermost() const;
	UPackage* GetPackage() const;
	UObject* GetOutermostObject() const;
	bool MarkPackageDirty() const;
	bool IsTemplate(EObjectFlags TemplateTypes = (EObjectFlags)(RF_ArchetypeObject | RF_ClassDefaultObject)) const;
	UObject* GetTypedOuter(UClass* Target) const;
	template <typename T> T* GetTypedOuter() const { return (T*)GetTypedOuter(T::StaticClass()); }
	/** The engine: `return Cast<InterfaceClassType>(GetImplementingOuterObject(InterfaceClassType::UClassType::StaticClass()));` */
	template <typename InterfaceClassType> requires requires { InterfaceClassType::UClassType::StaticClass(); }
	InterfaceClassType* GetImplementingOuter() const UE_STUB_BODY
	bool IsIn(const UObject* SomeOuter) const;
	bool IsIn(const UPackage* SomePackage) const;
	bool IsInOuter(const UObject* SomeOuter) const;
	bool IsInPackage(const UPackage* SomePackage) const;
	bool IsInA(const UClass* SomeBaseClass) const;
	bool RootPackageHasAnyFlags(uint32 CheckFlagMask) const;
	/** A template only to break the include cycle with UClass: the body does `const UClass* SomeBaseClass = SomeBase;`. */
	template <typename OtherClassType> requires std::is_convertible_v<OtherClassType, const UClass*>
	bool IsA(OtherClassType SomeBase) const UE_STUB_BODY
	template <class T> bool IsA() const { return IsA(T::StaticClass()); }
	const UClass* FindNearestCommonBaseClass(const UClass* TestClass) const;
	void* GetInterfaceAddress(UClass* InterfaceClass);
	void* GetNativeInterfaceAddress(UClass* InterfaceClass);
	const void* GetNativeInterfaceAddress(UClass* InterfaceClass) const;
	bool IsDefaultSubobject() const;
	bool IsTemplateForSubobjects(EObjectFlags TemplateTypes = (EObjectFlags)(RF_ClassDefaultObject | RF_DefaultSubObject | RF_InheritableComponentTemplate)) const;
};

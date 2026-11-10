// Stub SDK. Real header: Engine/Source/Runtime/CoreUObject/Public/UObject/Class.h (from UE 5.x source knowledge).
// UField / UStruct / UClass / UScriptStruct / UEnum / UFunction and StaticEnum<T>() / StaticStruct<T>().
#pragma once

#include "CoreMinimal.h"
// The real Class.h includes these directly (IWYU list of UE 5.x), which is how FRandomStream, FGuid and FCriticalSection
// are complete in any file that includes an engine UObject header (EngineTypes.h includes Class.h).
#include "HAL/CriticalSection.h"
#include "Math/RandomStream.h"
#include "Misc/Guid.h"
#include "Misc/Optional.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

class FProperty;
struct FImplementedInterface;

class UField : public UObject
{
	UE_STUB_CLASS_BODY(UField, UObject)
public:
	UField* Next;
	UClass* GetOwnerClass() const;
	UStruct* GetOwnerStruct() const;
	FString GetAuthoredName() const;
#if WITH_EDITORONLY_DATA
	FText GetDisplayNameText() const;
	FText GetToolTipText(bool bShortTooltip = false) const;
	bool HasMetaData(const TCHAR* Key) const;
	const FString& GetMetaData(const TCHAR* Key) const;
	void SetMetaData(const TCHAR* Key, const TCHAR* InValue);
#endif
};

class UStruct : public UField
{
	UE_STUB_CLASS_BODY(UStruct, UField)
public:
	UStruct* GetSuperStruct() const;
	bool IsChildOf(const UStruct* SomeBase) const;
	template <class T> bool IsChildOf() const { return IsChildOf(T::StaticClass()); }
	FProperty* FindPropertyByName(FName InName) const;
	int32 GetStructureSize() const;
	int32 GetPropertiesSize() const;
	virtual UStruct* GetInheritanceSuper() const;
};

class UScriptStruct : public UStruct
{
	UE_STUB_CLASS_BODY(UScriptStruct, UStruct)
public:
	void InitializeStruct(void* Dest, int32 ArrayDim = 1) const;
	void DestroyStruct(void* Dest, int32 ArrayDim = 1) const;
	void CopyScriptStruct(void* Dest, void const* Src, int32 ArrayDim = 1) const;
};

class UFunction : public UStruct
{
	UE_STUB_CLASS_BODY(UFunction, UStruct)
public:
	uint8 NumParms;
	uint16 ParmsSize;
	FProperty* GetReturnProperty() const;
	bool HasAnyFunctionFlags(uint32 FlagsToCheck) const;
};

class UEnum : public UField
{
	UE_STUB_CLASS_BODY(UEnum, UField)
public:
	enum class EGetByNameFlags { None = 0, ErrorIfNotFound = 0x01, CaseSensitive = 0x02, CheckAuthoredName = 0x04 };
	int32 NumEnums() const;
	FName GetNameByIndex(int32 Index) const;
	int64 GetValueByIndex(int32 Index) const;
	FName GetNameByValue(int64 InValue) const;
	int64 GetValueByName(FName InName, EGetByNameFlags Flags = EGetByNameFlags::None) const;
	int32 GetIndexByName(FName InName, EGetByNameFlags Flags = EGetByNameFlags::None) const;
	int32 GetIndexByValue(int64 InValue) const;
	FString GetNameStringByIndex(int32 InIndex) const;
	FString GetNameStringByValue(int64 InValue) const;
	FText GetDisplayNameTextByIndex(int32 InIndex) const;
	FText GetDisplayNameTextByValue(int64 InValue) const;
	FString GetAuthoredNameStringByValue(int64 InValue) const;
	FString GetValueAsString(int64 InValue) const;
	FName GetValueAsName(int64 InValue) const;
	int64 GetMaxEnumValue() const;
	bool IsValidEnumValue(int64 InValue) const;
	bool IsValidEnumName(FName InName) const;
	template <typename EnumType> static FString GetValueAsString(const EnumType EnumeratorValue) requires std::is_enum_v<EnumType> UE_STUB_BODY
	template <typename EnumType> static FName GetValueAsName(const EnumType EnumeratorValue) requires std::is_enum_v<EnumType> UE_STUB_BODY
	template <typename EnumType> static FText GetDisplayValueAsText(const EnumType EnumeratorValue) requires std::is_enum_v<EnumType> UE_STUB_BODY
};

class UClass : public UStruct
{
	UE_STUB_CLASS_BODY(UClass, UStruct)
public:
	EClassFlags ClassFlags;
	FName ClassConfigName;
	UClass* GetSuperClass() const;
	UObject* GetDefaultObject(bool bCreateIfNeeded = true) const;
	template <class T> T* GetDefaultObject() const UE_STUB_BODY
	FName GetDefaultObjectName() const;
	bool HasAnyClassFlags(EClassFlags FlagsToCheck) const;
	bool HasAllClassFlags(EClassFlags FlagsToCheck) const;
	EClassFlags GetClassFlags() const;
	bool ImplementsInterface(const class UClass* SomeInterface) const;
	UFunction* FindFunctionByName(FName InName) const;
	bool IsNative() const;
	UObject* GetDefaultSubobjectByName(FName ToFind);
	const FString GetConfigName() const;
	UClass* GetAuthoritativeClass();
	FString GetDescription() const;
};

template <typename EnumType> UEnum* StaticEnum() UE_STUB_BODY
template <typename StructType> UScriptStruct* StaticStruct() UE_STUB_BODY
template <typename ClassType> UClass* StaticClass() UE_STUB_BODY
template <typename T> struct TBaseStructure { static UScriptStruct* Get() UE_STUB_BODY };

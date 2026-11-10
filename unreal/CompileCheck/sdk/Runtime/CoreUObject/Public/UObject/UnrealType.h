// Stub SDK. Real header: Engine/Source/Runtime/CoreUObject/Public/UObject/UnrealType.h (UE 5.x source knowledge).
// FProperty and the property-changed events (editor callbacks); only the commonly used surface.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/Class.h"

class FField
{
public:
	virtual ~FField();
	FName GetFName() const;
	FString GetName() const;
	FString GetAuthoredName() const;
};
class FProperty : public FField
{
public:
	int32 ArrayDim;
	int32 ElementSize;
	FString GetNameCPP() const;
	bool HasAnyPropertyFlags(uint64 FlagsToCheck) const;
	template <typename ValueType> ValueType* ContainerPtrToValuePtr(UObject* ContainerPtr, int32 ArrayIndex = 0) const UE_STUB_BODY // explicit-ok: the engine C-casts a void* to ValueType*, any object type
};
namespace EPropertyChangeType { typedef uint32 Type; inline constexpr Type Unspecified = 1 << 0; inline constexpr Type ArrayAdd = 1 << 1; inline constexpr Type ValueSet = 1 << 4; inline constexpr Type Interactive = 1 << 5; }
struct FPropertyChangedEvent
{
	FPropertyChangedEvent(FProperty* InProperty, EPropertyChangeType::Type InChangeType = EPropertyChangeType::Unspecified);
	FProperty* Property;
	FProperty* MemberProperty;
	EPropertyChangeType::Type ChangeType;
	FName GetPropertyName() const;
	FName GetMemberPropertyName() const;
};
#define GET_MEMBER_NAME_CHECKED(ClassName, MemberName) ((void)sizeof(UE::Stub::GetMemberNameCheckedJunk(((ClassName*)0)->MemberName)), FName(TEXT(#MemberName)))
#define GET_MEMBER_NAME_STRING_CHECKED(ClassName, MemberName) ((void)sizeof(UE::Stub::GetMemberNameCheckedJunk(((ClassName*)0)->MemberName)), TEXT(#MemberName))
#define GET_FUNCTION_NAME_CHECKED(ClassName, FunctionName) ((void)sizeof(&ClassName::FunctionName), FName(TEXT(#FunctionName)))
namespace UE::Stub { template <typename T> char GetMemberNameCheckedJunk(const T&) UE_STUB_BODY }

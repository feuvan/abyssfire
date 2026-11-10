// Stub SDK. Real header: Engine/Source/Runtime/CoreUObject/Public/UObject/ConstructorHelpers.h (UE 5.x source knowledge).
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "Templates/SubclassOf.h"

struct ConstructorHelpers
{
	template <class T>
	struct FObjectFinder
	{
		T* Object;
		FObjectFinder(const TCHAR* ObjectToFind, uint32 InLoadFlags = LOAD_None) UE_STUB_BODY
		bool Succeeded() const UE_STUB_BODY
	};
	template <class T>
	struct FObjectFinderOptional
	{
		FObjectFinderOptional(const TCHAR* InObjectToFind, uint32 InLoadFlags = LOAD_None) UE_STUB_BODY
		T* Get() UE_STUB_BODY
		bool Succeeded() UE_STUB_BODY
	};
	template <class T>
	struct FClassFinder
	{
		TSubclassOf<T> Class;
		FClassFinder(const TCHAR* ClassToFind) UE_STUB_BODY
		bool Succeeded() UE_STUB_BODY
	};
};

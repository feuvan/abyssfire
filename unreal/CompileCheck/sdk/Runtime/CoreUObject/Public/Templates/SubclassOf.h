// Stub SDK. Real header: Engine/Source/Runtime/CoreUObject/Public/Templates/SubclassOf.h (from UE 5.x source knowledge).
#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "UObject/ObjectPtr.h"

template <typename T>
class TSubclassOf
{
public:
	using ElementType = T;
	TSubclassOf() = default;
	TSubclassOf(TSubclassOf&&) = default;
	TSubclassOf(const TSubclassOf&) = default;
	TSubclassOf& operator=(TSubclassOf&&) = default;
	TSubclassOf& operator=(const TSubclassOf&) = default;
	TSubclassOf(UClass* From) : Class(From) {}
	template <typename U> requires (!std::is_convertible_v<U&&, UClass*>) && requires(U&& From) { { From.Get() } -> std::convertible_to<UClass*>; }
	TSubclassOf(U&& From) : Class(From.Get()) {}
	template <typename OtherT> requires std::is_convertible_v<OtherT*, T*>
	TSubclassOf(const TSubclassOf<OtherT>& Other) : Class(*Other) {}
	TSubclassOf& operator=(UClass* From) { Class = From; return *this; }
	UClass* operator*() const { return Class; }
	UClass* Get() const { return Class; }
	UClass* operator->() const { return Class; }
	operator UClass*() const { return Class; }
	T* GetDefaultObject() const UE_STUB_BODY
	friend uint32 GetTypeHash(const TSubclassOf& SubclassOf) { return 0; }
private:
	UClass* Class = nullptr;
};

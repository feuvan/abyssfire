// Stub SDK. Real header: Engine/Source/Runtime/CoreUObject/Public/UObject/ScriptInterface.h (from UE 5.x source knowledge).
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Templates/Casts.h"

class FScriptInterface
{
public:
	FScriptInterface(UObject* InObjectPointer = nullptr, void* InInterfacePointer = nullptr);
	UObject* GetObject() const;
	UObject*& GetObjectRef();
	void* GetInterface() const;
	void SetObject(UObject* InObjectPointer);
	void SetInterface(void* InInterfacePointer);
	bool operator==(const FScriptInterface& Other) const;
	bool operator!=(const FScriptInterface& Other) const;
};

template <class InInterfaceType>
class TScriptInterface : public FScriptInterface
{
public:
	using InterfaceType = InInterfaceType;
	TScriptInterface() {}
	TScriptInterface(TYPE_OF_NULLPTR) {}
	template <typename U> requires std::is_convertible_v<U, UObject*>
	TScriptInterface(U&& Source) UE_STUB_BODY
	template <typename ObjectType> TScriptInterface(TObjectPtr<ObjectType> SourceObject) UE_STUB_BODY
	InInterfaceType* operator->() const UE_STUB_BODY
	InInterfaceType& operator*() const UE_STUB_BODY
	InInterfaceType* GetInterface() const UE_STUB_BODY
	explicit operator bool() const UE_STUB_BODY
	template <typename U> requires std::is_convertible_v<U, UObject*>
	TScriptInterface& operator=(U&& Source) UE_STUB_BODY
	TScriptInterface& operator=(TYPE_OF_NULLPTR) UE_STUB_BODY
	bool operator==(const InInterfaceType* Other) const UE_STUB_BODY
	bool operator==(const TScriptInterface& Other) const UE_STUB_BODY
	bool operator==(TYPE_OF_NULLPTR) const UE_STUB_BODY
	friend uint32 GetTypeHash(const TScriptInterface& Instance) { return 0; }
};

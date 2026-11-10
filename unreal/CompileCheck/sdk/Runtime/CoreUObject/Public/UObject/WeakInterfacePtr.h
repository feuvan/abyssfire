// Stub SDK. Real header: Engine/Source/Runtime/CoreUObject/Public/UObject/WeakInterfacePtr.h (from UE 5.x source
// knowledge). TWeakInterfacePtr<IFoo>: weak reference to a UObject through one of its native interfaces.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ScriptInterface.h"
#include "UObject/WeakObjectPtr.h"
#include "Templates/Casts.h"

template <class T>
struct TWeakInterfacePtr
{
	TWeakInterfacePtr() : InterfaceInstance(nullptr) {}
	template <typename U> requires std::is_convertible_v<U, const UObject*>
	TWeakInterfacePtr(U&& Object) : InterfaceInstance(nullptr) {}
	TWeakInterfacePtr(T& Interface) : InterfaceInstance(&Interface) {}
	TWeakInterfacePtr(T* Interface) : InterfaceInstance(Interface) {}
	TWeakInterfacePtr(const TScriptInterface<T>& ScriptInterface) : InterfaceInstance(nullptr) {}
	void Reset() UE_STUB_BODY
	bool IsValid(bool bEvenIfPendingKill, bool bThreadsafeTest = false) const UE_STUB_BODY
	bool IsValid() const UE_STUB_BODY
	bool IsStale(bool bEvenIfPendingKill = false, bool bThreadsafeTest = false) const UE_STUB_BODY
	UObject* GetObject() const UE_STUB_BODY
	T* Get() const UE_STUB_BODY
	T& operator*() const UE_STUB_BODY
	T* operator->() const UE_STUB_BODY
	TWeakInterfacePtr& operator=(const T* Other) UE_STUB_BODY
	TWeakInterfacePtr& operator=(const TWeakInterfacePtr& Other) = default;
	TWeakInterfacePtr& operator=(const TScriptInterface<T>& Other) UE_STUB_BODY
	bool operator==(const TWeakInterfacePtr& Other) const UE_STUB_BODY
	bool operator==(const UObject* Other) const UE_STUB_BODY
	TWeakObjectPtr<UObject> GetWeakObjectPtr() const UE_STUB_BODY
	TScriptInterface<T> ToScriptInterface() const UE_STUB_BODY
	explicit operator bool() const UE_STUB_BODY
private:
	TWeakObjectPtr<UObject> ObjectInstance;
	T* InterfaceInstance;
};
template <typename T> uint32 GetTypeHash(const TWeakInterfacePtr<T>& WeakInterfacePtr) UE_STUB_BODY

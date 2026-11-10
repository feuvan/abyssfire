// Stub SDK. Real header: Engine/Source/Runtime/CoreUObject/Public/Templates/SubclassOf.h (from UE 5.x source knowledge).
// The constraints are the engine's: constructible / assignable from a UClass*, from anything implicitly convertible
// to UClass* that is not itself a TSubclassOf (TObjectPtr<UClass>, nullptr; not TWeakObjectPtr<UClass>), and from a
// TSubclassOf<OtherT> when OtherT* converts to T*. Dereferencing does the runtime IsChildOf(T::StaticClass()) check,
// so T must be complete where operator* / Get / -> / the UClass* conversion / GetDefaultObject are used.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "UObject/ObjectPtr.h"

template <typename T> class TSubclassOf;

template <typename T> struct TIsTSubclassOf { enum { Value = false }; };
template <typename T> struct TIsTSubclassOf<TSubclassOf<T>> { enum { Value = true }; };
template <typename T> struct TIsTSubclassOf<const TSubclassOf<T>> { enum { Value = true }; };
template <typename T> struct TIsTSubclassOf<volatile TSubclassOf<T>> { enum { Value = true }; };
template <typename T> struct TIsTSubclassOf<const volatile TSubclassOf<T>> { enum { Value = true }; };

template <typename T>
class TSubclassOf
{
	template <typename U> friend class TSubclassOf;
public:
	using ElementType = T;
	TSubclassOf() = default;
	TSubclassOf(TSubclassOf&&) = default;
	TSubclassOf(const TSubclassOf&) = default;
	TSubclassOf& operator=(TSubclassOf&&) = default;
	TSubclassOf& operator=(const TSubclassOf&) = default;
	~TSubclassOf() = default;
	/** Constructor that takes a UClass*. */
	TSubclassOf(UClass* From) : Class(From) {}
	/** Construct from a UClass* (or something implicitly convertible to it). */
	template <typename U> requires (!TIsTSubclassOf<std::decay_t<U>>::Value) && std::is_convertible_v<U, UClass*>
	TSubclassOf(U&& From) : Class(From) {}
	/** Construct from another TSubclassOf, only if types are compatible. */
	template <typename OtherT> requires std::is_convertible_v<OtherT*, T*>
	TSubclassOf(const TSubclassOf<OtherT>& Other) : Class(Other.Class) {}
	/** Assign from another TSubclassOf, only if types are compatible. */
	template <typename OtherT> requires std::is_convertible_v<OtherT*, T*>
	TSubclassOf& operator=(const TSubclassOf<OtherT>& Other) { Class = Other.Class; return *this; }
	/** Assign from a UClass*. */
	TSubclassOf& operator=(UClass* From) { Class = From; return *this; }
	/** Assign from a UClass* (or something implicitly convertible to it). */
	template <typename U> requires (!TIsTSubclassOf<std::decay_t<U>>::Value) && std::is_convertible_v<U, UClass*>
	TSubclassOf& operator=(U&& From) { Class = From; return *this; }
	/** Dereference back into a UClass*, does runtime type checking. */
	UClass* operator*() const
	{
		if (!Class || !Class->IsChildOf(T::StaticClass()))
		{
			return nullptr;
		}
		return Class;
	}
	UClass* Get() const { return **this; }
	UClass* operator->() const { return **this; }
	operator UClass*() const { return **this; }
	T* GetDefaultObject() const
	{
		UObject* Result = nullptr;
		if (Class)
		{
			Result = Class->GetDefaultObject();
			(void)Result->IsA(T::StaticClass());
		}
		return (T*)Result;
	}
	friend uint32 GetTypeHash(const TSubclassOf& SubclassOf) { return GetTypeHash(SubclassOf.Class); }
private:
	TObjectPtr<UClass> Class = nullptr;
};

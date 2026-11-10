// Stub SDK. Real header: Engine/Source/Runtime/CoreUObject/Public/Templates/Casts.h (from UE 5.x source knowledge).
// Cast / CastChecked / ExactCast. The static checks are the engine's: both types must be complete, and each must be a
// UObject class or a UINTERFACE I-class (one with a UClassType typedef).
// Constness (API_UNCERTAIN.md #4): the casts return TCopyQualifiersFromTo_T<From, To>*, i.e. Cast<AActor>(ConstObject)
// is a `const AActor*`. Recent engines are believed to declare them this way; older ones returned a plain To*
// (constness dropped). The propagating form is the strict side: code that compiles against it compiles against both.
#pragma once

#include "CoreMinimal.h"
#include "Templates/CopyQualifiersFromTo.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/ObjectPtr.h"

namespace ECastCheckedType { enum Type { NullAllowed, NullChecked }; }

template <typename T>
struct TIsIInterface
{
	enum { Value = requires { typename T::UClassType; } };
};

namespace UE::Stub
{
	template <typename To, typename From>
	constexpr void CheckCast()
	{
		static_assert(sizeof(From) > 0 && sizeof(To) > 0, "Attempting to cast between incomplete types");
		static_assert(bool(TIsIInterface<From>::Value) || std::is_base_of_v<UObject, From>, "Attempting to use Cast<> on a type that is not a UObject or an Interface");
		static_assert(bool(TIsIInterface<To>::Value) || std::is_base_of_v<UObject, To>, "Attempting to use Cast<> to a type that is not a UObject or an Interface");
	}
}

// UNVERIFIED (API_UNCERTAIN.md #4): the return types propagate the constness of From (believed to be the 5.x form).
template <typename To, typename From>
TCopyQualifiersFromTo_T<From, To>* Cast(From* Src) { UE::Stub::CheckCast<To, From>(); return (TCopyQualifiersFromTo_T<From, To>*)Src; }
template <typename To, typename From>
TCopyQualifiersFromTo_T<From, To>* ExactCast(From* Src) { UE::Stub::CheckCast<To, From>(); return (TCopyQualifiersFromTo_T<From, To>*)Src; }
template <typename To, typename From>
TCopyQualifiersFromTo_T<From, To>* CastChecked(From* Src) { UE::Stub::CheckCast<To, From>(); return (TCopyQualifiersFromTo_T<From, To>*)Src; }
template <typename To, typename From>
TCopyQualifiersFromTo_T<From, To>* CastChecked(From* Src, ECastCheckedType::Type CheckType) { UE::Stub::CheckCast<To, From>(); return (TCopyQualifiersFromTo_T<From, To>*)Src; }
template <typename To, typename From>
TCopyQualifiersFromTo_T<From, To>* Cast(const TObjectPtr<From>& Src) { return Cast<To>(Src.Get()); }
template <typename To, typename From>
TCopyQualifiersFromTo_T<From, To>* ExactCast(const TObjectPtr<From>& Src) { return ExactCast<To>(Src.Get()); }
template <typename To, typename From>
TCopyQualifiersFromTo_T<From, To>* CastChecked(const TObjectPtr<From>& Src, ECastCheckedType::Type CheckType = ECastCheckedType::NullChecked) { return CastChecked<To>(Src.Get(), CheckType); }
template <typename To, typename From>
TCopyQualifiersFromTo_T<From, To>* Cast(const TWeakObjectPtr<From>& Src) { return Cast<To>(Src.Get()); }
template <typename To, typename From>
TCopyQualifiersFromTo_T<From, To>* CastChecked(const TWeakObjectPtr<From>& Src, ECastCheckedType::Type CheckType = ECastCheckedType::NullChecked) { return CastChecked<To>(Src.Get(), CheckType); }

template <class T> bool UObject::Implements() const
{
	static_assert(std::is_base_of_v<UObject, T>, "Implements<T>: T is the U-class of the interface (UFoo, not IFoo)");
	return false;
}

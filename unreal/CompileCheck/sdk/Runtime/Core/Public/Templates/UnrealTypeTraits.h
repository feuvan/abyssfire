// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Templates/UnrealTypeTraits.h (+ the small Templates/Is*.h,
// AndOrNot.h, EnableIf.h, Decay.h, RemoveReference.h headers it pulls in). From UE 5.x source knowledge.
#pragma once

#include "CoreTypes.h"
#include <type_traits>

template <bool Predicate, typename Result = void> struct TEnableIf;
template <typename Result> struct TEnableIf<true, Result> { using type = Result; using Type = Result; };
template <typename Result> struct TEnableIf<false, Result> {};

template <typename... Types> struct TAnd;
template <bool LHSValue, typename... RHS> struct TAndValue { static constexpr bool Value = TAnd<RHS...>::value; static constexpr bool value = Value; };
template <typename... RHS> struct TAndValue<false, RHS...> { static constexpr bool Value = false; static constexpr bool value = false; };
template <typename LHS, typename... RHS> struct TAnd<LHS, RHS...> : TAndValue<LHS::Value, RHS...> {};
template <> struct TAnd<> { static constexpr bool Value = true; static constexpr bool value = true; };
template <typename... Types> struct TOr;
template <bool LHSValue, typename... RHS> struct TOrValue { static constexpr bool Value = TOr<RHS...>::value; static constexpr bool value = Value; };
template <typename... RHS> struct TOrValue<true, RHS...> { static constexpr bool Value = true; static constexpr bool value = true; };
template <typename LHS, typename... RHS> struct TOr<LHS, RHS...> : TOrValue<LHS::Value, RHS...> {};
template <> struct TOr<> { static constexpr bool Value = false; static constexpr bool value = false; };
template <typename Type> struct TNot { static constexpr bool Value = !Type::Value; static constexpr bool value = !Type::Value; };

template <typename T> struct TIsArithmetic { enum { Value = std::is_arithmetic_v<T> }; };
template <typename T> struct TIsIntegral { enum { Value = std::is_integral_v<T> }; };
template <typename T> struct TIsFloatingPoint { enum { Value = std::is_floating_point_v<T> }; };
template <typename T> struct TIsSigned { enum { Value = std::is_signed_v<T> }; };
template <typename T> struct TIsPointer { enum { Value = std::is_pointer_v<T> }; };
template <typename T> struct TIsEnum { enum { Value = std::is_enum_v<T> }; };
template <typename T> struct TIsEnumClass { enum { Value = std::is_enum_v<T> && !std::is_convertible_v<T, int> }; };
template <typename T> struct TIsPODType { enum { Value = std::is_standard_layout_v<T> && std::is_trivial_v<T> }; };
template <typename T> struct TIsZeroConstructType { enum { Value = std::is_enum_v<T> || std::is_arithmetic_v<T> || std::is_pointer_v<T> }; };
template <typename T> struct TIsTriviallyDestructible { enum { Value = std::is_trivially_destructible_v<T> }; };
template <typename T> struct TIsTriviallyCopyConstructible { enum { Value = std::is_trivially_copy_constructible_v<T> }; };
template <typename T> struct TIsTriviallyCopyAssignable { enum { Value = std::is_trivially_copy_assignable_v<T> }; };
template <typename T> struct TIsBitwiseConstructible_Dummy {};
template <typename T> struct TIsWeakPointerType { enum { Value = false }; };
template <typename T> struct TIsFunction { enum { Value = std::is_function_v<T> }; };
template <typename T> struct TIsConst { static constexpr bool Value = std::is_const_v<T>; };
template <typename A, typename B> struct TIsSame { enum { Value = std::is_same_v<A, B> }; };
// Traits/IsCharType.h. The engine also specialises it for UCS2CHAR and UTF32CHAR (plain uint16 / uint32 there) and, with
// PLATFORM_TCHAR_IS_CHAR16, for wchar_t. Those are deliberately NOT declared: here an integer or an L'x' literal is never a
// character (`Str += SomeUint32` would append a code unit in the engine, which is not what such code means).
template <typename T> struct TIsCharType { enum { Value = false }; };
template <> struct TIsCharType<ANSICHAR> { enum { Value = true }; };
template <> struct TIsCharType<WIDECHAR> { enum { Value = true }; };
template <> struct TIsCharType<UTF8CHAR> { enum { Value = true }; };
template <typename DerivedType, typename BaseType> struct TIsDerivedFrom
{
	static constexpr bool Value = std::is_base_of_v<BaseType, DerivedType>;
	static constexpr bool IsDerived = Value;
};
template <typename From, typename To> struct TPointerIsConvertibleFromTo { enum { Value = std::is_convertible_v<From*, To*> }; };
template <typename T> struct TRemoveReference { using Type = std::remove_reference_t<T>; };
template <typename T> struct TRemoveCV { using Type = std::remove_cv_t<T>; };
template <typename T> struct TRemovePointer { using Type = std::remove_pointer_t<T>; };
template <typename T> struct TDecay { using Type = std::decay_t<T>; };
template <typename T> using TDecayT = std::decay_t<T>;
template <typename T> struct TIdentity { using Type = T; using type = T; };
template <typename T> using TIdentity_T = T;
template <typename T> struct TCallTraits
{
	using ValueType = T;
	using Reference = T&;
	using ConstReference = const T&;
	using ParamType = std::conditional_t<std::is_arithmetic_v<T> || std::is_pointer_v<T> || std::is_enum_v<T>, const T, const T&>;
	using ConstPointerType = std::conditional_t<std::is_pointer_v<T>, const std::remove_pointer_t<T>*, const T>;
};
template <typename T> struct TTypeTraits : TCallTraits<T>
{
	using ConstInitType = typename TCallTraits<T>::ParamType;
	using ConstPointerType = typename TCallTraits<T>::ConstPointerType;
};
template <typename T> struct TIsContiguousContainer { enum { Value = false }; };
template <typename T> struct TIsTArray { enum { Value = false }; };
template <typename T, typename A> struct TIsTArray<TArray<T, A>> { enum { Value = true }; };
template <typename T, typename A> struct TIsTArray<const TArray<T, A>> { enum { Value = true }; };

// Templates/Requires.h
#define UE_REQUIRES(...) , std::enable_if_t<(__VA_ARGS__), int> = 0
#define UE_REQUIRES_FRIEND(...) , std::enable_if_t<(__VA_ARGS__), int>
#define UE_REQUIRES_DEFINITION(...) , std::enable_if_t<(__VA_ARGS__), int>

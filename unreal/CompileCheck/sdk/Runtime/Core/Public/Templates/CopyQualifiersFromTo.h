// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Templates/CopyQualifiersFromTo.h (from UE 5.x source knowledge).
#pragma once

/** Copies the cv-qualifiers from one type to another, e.g. TCopyQualifiersFromTo_T<const T1, T2> == const T2. */
template <typename From, typename To> struct TCopyQualifiersFromTo { typedef To Type; };
template <typename From, typename To> struct TCopyQualifiersFromTo<const From, To> { typedef const To Type; };
template <typename From, typename To> struct TCopyQualifiersFromTo<volatile From, To> { typedef volatile To Type; };
template <typename From, typename To> struct TCopyQualifiersFromTo<const volatile From, To> { typedef const volatile To Type; };

template <typename From, typename To>
using TCopyQualifiersFromTo_T = typename TCopyQualifiersFromTo<From, To>::Type;

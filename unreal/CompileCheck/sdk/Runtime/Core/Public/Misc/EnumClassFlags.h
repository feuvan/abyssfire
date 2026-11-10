// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Misc/EnumClassFlags.h (from UE 5.x source knowledge).
#pragma once

#include <type_traits>

#define ENUM_CLASS_FLAGS(Enum) \
	inline constexpr Enum& operator|=(Enum& Lhs, Enum Rhs) { return Lhs = (Enum)((__underlying_type(Enum))Lhs | (__underlying_type(Enum))Rhs); } \
	inline constexpr Enum& operator&=(Enum& Lhs, Enum Rhs) { return Lhs = (Enum)((__underlying_type(Enum))Lhs & (__underlying_type(Enum))Rhs); } \
	inline constexpr Enum& operator^=(Enum& Lhs, Enum Rhs) { return Lhs = (Enum)((__underlying_type(Enum))Lhs ^ (__underlying_type(Enum))Rhs); } \
	inline constexpr Enum operator|(Enum Lhs, Enum Rhs) { return (Enum)((__underlying_type(Enum))Lhs | (__underlying_type(Enum))Rhs); } \
	inline constexpr Enum operator&(Enum Lhs, Enum Rhs) { return (Enum)((__underlying_type(Enum))Lhs & (__underlying_type(Enum))Rhs); } \
	inline constexpr Enum operator^(Enum Lhs, Enum Rhs) { return (Enum)((__underlying_type(Enum))Lhs ^ (__underlying_type(Enum))Rhs); } \
	inline constexpr bool operator!(Enum E) { return !(__underlying_type(Enum))E; } \
	inline constexpr Enum operator~(Enum E) { return (Enum)~(__underlying_type(Enum))E; }
#define FRIEND_ENUM_CLASS_FLAGS(Enum) \
	friend Enum& operator|=(Enum& Lhs, Enum Rhs); \
	friend Enum& operator&=(Enum& Lhs, Enum Rhs); \
	friend Enum& operator^=(Enum& Lhs, Enum Rhs); \
	friend Enum operator|(Enum Lhs, Enum Rhs); \
	friend Enum operator&(Enum Lhs, Enum Rhs); \
	friend Enum operator^(Enum Lhs, Enum Rhs); \
	friend bool operator!(Enum E); \
	friend Enum operator~(Enum E);

template <typename Enum> constexpr bool EnumHasAllFlags(Enum Flags, Enum Contains) { using U = __underlying_type(Enum); return ((U)Flags & (U)Contains) == (U)Contains; }
template <typename Enum> constexpr bool EnumHasAnyFlags(Enum Flags, Enum Contains) { using U = __underlying_type(Enum); return ((U)Flags & (U)Contains) != 0; }
template <typename Enum> constexpr void EnumAddFlags(Enum& Flags, Enum FlagsToAdd) { using U = __underlying_type(Enum); Flags = (Enum)((U)Flags | (U)FlagsToAdd); }
template <typename Enum> constexpr void EnumRemoveFlags(Enum& Flags, Enum FlagsToRemove) { using U = __underlying_type(Enum); Flags = (Enum)((U)Flags & ~(U)FlagsToRemove); }

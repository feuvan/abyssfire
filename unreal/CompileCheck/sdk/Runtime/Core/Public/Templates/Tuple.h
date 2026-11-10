// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Templates/Tuple.h (from UE 5.x source knowledge).
// TTuple / TPair. A two-element tuple exposes Key and Value (TPair<K, V> = TTuple<K, V>), as in the engine.
#pragma once

#include "CoreTypes.h"
#include "Templates/UnrealTemplate.h"
#include "Templates/TypeHash.h"
#include <tuple>

template <typename... Types>
struct TTuple
{
	TTuple() = default;
	template <typename... ArgTypes> requires (sizeof...(ArgTypes) == sizeof...(Types)) && (std::is_constructible_v<Types, ArgTypes&&> && ...)
	explicit(!(std::is_convertible_v<ArgTypes&&, Types> && ...)) TTuple(ArgTypes&&... Args) : Storage(static_cast<ArgTypes&&>(Args)...) {}
	template <SIZE_T Index> decltype(auto) Get() { return std::get<Index>(Storage); }
	template <SIZE_T Index> decltype(auto) Get() const { return std::get<Index>(Storage); }
	template <typename T> T& Get() { return std::get<T>(Storage); }
	template <typename T> const T& Get() const { return std::get<T>(Storage); }
	bool operator==(const TTuple& Rhs) const { return Storage == Rhs.Storage; }
	bool operator!=(const TTuple& Rhs) const { return !(Storage == Rhs.Storage); }
	bool operator<(const TTuple& Rhs) const { return Storage < Rhs.Storage; }
	std::tuple<Types...> Storage;
};
template <typename InKeyType, typename InValueType>
struct TTuple<InKeyType, InValueType>
{
	using KeyType = InKeyType;
	using ValueType = InValueType;
	TTuple() = default;
	template <typename KeyInitType, typename ValueInitType>
		requires std::is_constructible_v<InKeyType, KeyInitType&&> && std::is_constructible_v<InValueType, ValueInitType&&>
	explicit(!(std::is_convertible_v<KeyInitType&&, InKeyType> && std::is_convertible_v<ValueInitType&&, InValueType>))
	TTuple(KeyInitType&& InKey, ValueInitType&& InValue)
		: Key(static_cast<KeyInitType&&>(InKey)), Value(static_cast<ValueInitType&&>(InValue)) {}
	template <SIZE_T Index> decltype(auto) Get() { if constexpr (Index == 0) { return (Key); } else { return (Value); } }
	template <SIZE_T Index> decltype(auto) Get() const { if constexpr (Index == 0) { return (Key); } else { return (Value); } }
	bool operator==(const TTuple& Rhs) const { return Key == Rhs.Key && Value == Rhs.Value; }
	bool operator!=(const TTuple& Rhs) const { return !(*this == Rhs); }
	InKeyType Key;
	InValueType Value;
};
template <typename KeyType, typename ValueType> using TPair = TTuple<KeyType, ValueType>;
template <typename... Types> TTuple<std::decay_t<Types>...> MakeTuple(Types&&... Args) { return TTuple<std::decay_t<Types>...>(static_cast<Types&&>(Args)...); }
template <typename... Types> TTuple<Types&...> Tie(Types&... Args) { return TTuple<Types&...>(Args...); }
template <typename... Types> TTuple<Types&&...> ForwardAsTuple(Types&&... Args) UE_STUB_BODY
template <typename... Types> uint32 GetTypeHash(const TTuple<Types...>& Tuple) UE_STUB_BODY

// structured bindings (the engine specialises std::tuple_size / tuple_element for TTuple too)
namespace std
{
	template <typename... Types> struct tuple_size<TTuple<Types...>> : std::integral_constant<std::size_t, sizeof...(Types)> {};
	template <std::size_t N, typename... Types> struct tuple_element<N, TTuple<Types...>> { using type = std::tuple_element_t<N, std::tuple<Types...>>; };
}
template <SIZE_T N, typename... Types> decltype(auto) get(TTuple<Types...>& Tuple) { return Tuple.template Get<N>(); }
template <SIZE_T N, typename... Types> decltype(auto) get(const TTuple<Types...>& Tuple) { return Tuple.template Get<N>(); }
template <SIZE_T N, typename... Types> decltype(auto) get(TTuple<Types...>&& Tuple) { return Tuple.template Get<N>(); }

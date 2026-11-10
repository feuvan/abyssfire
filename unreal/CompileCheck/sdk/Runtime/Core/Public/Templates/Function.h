// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Templates/Function.h (from UE 5.x source knowledge).
// TFunction (copyable, owns), TUniqueFunction (move-only, owns), TFunctionRef (non-owning, never null). The binding
// constructors are constrained like the engine's (TFuncCanBindToFunctor): a functor that cannot be called with the
// signature's arguments, or whose result does not convert to the return type, is a compile error.
#pragma once

#include "CoreTypes.h"
#include "Templates/UnrealTemplate.h"
#include <type_traits>

template <typename FuncType> class TFunctionRef;
template <typename FuncType> class TFunction;
template <typename FuncType> class TUniqueFunction;

namespace UE::Core::Private::Function
{
	template <typename FuncType, typename FunctorType> struct TFuncCanBindToFunctor;
	template <typename FunctorType, typename Ret, typename... ParamTypes>
	struct TFuncCanBindToFunctor<Ret(ParamTypes...), FunctorType>
	{
		static constexpr bool Value = std::is_invocable_r_v<Ret, FunctorType&, ParamTypes...>;
	};
	template <typename FunctorType, typename... ParamTypes>
	struct TFuncCanBindToFunctor<void(ParamTypes...), FunctorType>
	{
		static constexpr bool Value = std::is_invocable_v<FunctorType&, ParamTypes...>;
	};
	template <typename T> struct TIsATFunction { static constexpr bool Value = false; };
	template <typename T> struct TIsATFunction<TFunction<T>> { static constexpr bool Value = true; };
	template <typename T> struct TIsAUniqueFunction { static constexpr bool Value = false; };
	template <typename T> struct TIsAUniqueFunction<TUniqueFunction<T>> { static constexpr bool Value = true; };
	template <typename T> struct TIsATFunctionRef { static constexpr bool Value = false; };
	template <typename T> struct TIsATFunctionRef<TFunctionRef<T>> { static constexpr bool Value = true; };
}

template <typename Ret, typename... ParamTypes>
class TFunctionRef<Ret(ParamTypes...)>
{
public:
	template <typename FunctorType>
		requires (!UE::Core::Private::Function::TIsATFunctionRef<std::decay_t<FunctorType>>::Value)
			&& UE::Core::Private::Function::TFuncCanBindToFunctor<Ret(ParamTypes...), std::decay_t<FunctorType>>::Value
	TFunctionRef(FunctorType&& InFunc UE_LIFETIMEBOUND) UE_STUB_BODY
	TFunctionRef(const TFunctionRef&) = default;
	TFunctionRef& operator=(const TFunctionRef&) const = delete;
	~TFunctionRef() = default;
	Ret operator()(ParamTypes... Params) const UE_STUB_BODY
};

template <typename Ret, typename... ParamTypes>
class TFunction<Ret(ParamTypes...)>
{
public:
	TFunction(TYPE_OF_NULLPTR = nullptr) UE_STUB_BODY
	template <typename FunctorType>
		requires (!UE::Core::Private::Function::TIsATFunction<std::decay_t<FunctorType>>::Value)
			&& UE::Core::Private::Function::TFuncCanBindToFunctor<Ret(ParamTypes...), std::decay_t<FunctorType>>::Value
	TFunction(FunctorType&& InFunc)
	{
		static_assert(std::is_copy_constructible_v<std::decay_t<FunctorType>>,
			"TFunction requires a copyable functor (a lambda capturing a move-only object needs TUniqueFunction)");
	}
	TFunction(TFunction&&) = default;
	TFunction(const TFunction& Other) = default;
	~TFunction() = default;
	TFunction& operator=(TFunction&& Other) UE_STUB_BODY
	TFunction& operator=(const TFunction& Other) UE_STUB_BODY
	explicit operator bool() const UE_STUB_BODY
	bool IsSet() const UE_STUB_BODY
	void Reset() UE_STUB_BODY
	Ret operator()(ParamTypes... Params) const UE_STUB_BODY
	bool operator==(TYPE_OF_NULLPTR) const UE_STUB_BODY
	bool operator!=(TYPE_OF_NULLPTR) const UE_STUB_BODY
};

template <typename Ret, typename... ParamTypes>
class TUniqueFunction<Ret(ParamTypes...)>
{
public:
	TUniqueFunction(TYPE_OF_NULLPTR = nullptr) UE_STUB_BODY
	template <typename FunctorType>
		requires (!UE::Core::Private::Function::TIsAUniqueFunction<std::decay_t<FunctorType>>::Value)
			&& (!UE::Core::Private::Function::TIsATFunction<std::decay_t<FunctorType>>::Value)
			&& UE::Core::Private::Function::TFuncCanBindToFunctor<Ret(ParamTypes...), std::decay_t<FunctorType>>::Value
	TUniqueFunction(FunctorType&& InFunc) UE_STUB_BODY
	TUniqueFunction(TFunction<Ret(ParamTypes...)>&& Other) UE_STUB_BODY
	TUniqueFunction(const TFunction<Ret(ParamTypes...)>& Other) UE_STUB_BODY
	TUniqueFunction& operator=(TUniqueFunction&& Other) UE_STUB_BODY
	TUniqueFunction(TUniqueFunction&&) = default;
	TUniqueFunction(const TUniqueFunction& Other) = delete;
	TUniqueFunction& operator=(const TUniqueFunction& Other) = delete;
	~TUniqueFunction() = default;
	explicit operator bool() const UE_STUB_BODY
	bool IsSet() const UE_STUB_BODY
	void Reset() UE_STUB_BODY
	Ret operator()(ParamTypes... Params) const UE_STUB_BODY
};

template <typename FuncType> bool operator==(TYPE_OF_NULLPTR, const TFunction<FuncType>& Func) UE_STUB_BODY
template <typename FuncType> bool operator!=(TYPE_OF_NULLPTR, const TFunction<FuncType>& Func) UE_STUB_BODY

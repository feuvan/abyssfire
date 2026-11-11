// Stub SDK. Real headers: Engine/Source/Runtime/Core/Public/Delegates/Delegate.h, DelegateSignatureImpl.inl,
// DelegateCombinations.h, MulticastDelegateBase.h (from UE 5.x source knowledge).
//
// TDelegate<Ret(Params...)> (one bound callable) and TMulticastDelegate<void(Params...)> (invocation list). The binders
// are typed like the engine's: a member function bound with BindUObject / BindSP / BindRaw / AddUObject / ... must have
// exactly the signature Ret(Params..., PayloadTypes...) (TMemFunPtrType), a const object needs a const member function,
// BindUObject needs a UObject-derived class, BindSP a TSharedFromThis-derived one, and a lambda must be callable with
// (Params..., Payload...) and return something convertible to Ret. Wrong signatures fail to compile, as in UE.
// A weak lambda (CreateWeakLambda / BindWeakLambda / AddWeakLambda) keeps its owner in a TWeakObjectPtr<UserClass>
// (TWeakBaseFunctorDelegateInstance): UObject owners only. A Slate widget or another TSharedFromThis object uses
// CreateSPLambda / BindSPLambda / AddSPLambda.
#pragma once

#include "CoreTypes.h"
#include "Delegates/DelegateBase.h"
#include "Delegates/IDelegateInstance.h"
#include "Templates/SharedPointer.h"
#include "Templates/UnrealTemplate.h"
#include "Templates/Function.h"
#include "UObject/NameTypes.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UObject;
template <typename T> struct TObjectPtr;

template <bool Const, typename Class, typename FuncType> struct TMemFunPtrType;
template <typename Class, typename RetType, typename... ArgTypes>
struct TMemFunPtrType<false, Class, RetType(ArgTypes...)> { typedef RetType (Class::*Type)(ArgTypes...); };
template <typename Class, typename RetType, typename... ArgTypes>
struct TMemFunPtrType<true, Class, RetType(ArgTypes...)> { typedef RetType (Class::*Type)(ArgTypes...) const; };

namespace UE::Stub
{
	template <typename UserClass> concept CDelegateUObject = std::is_base_of_v<UObject, UserClass>;
	/** BindSP(UserClass*) needs AsShared(): the class must derive from TSharedFromThis. */
	template <typename UserClass> concept CDelegateSharedFromThis = requires(UserClass* Obj) { Obj->AsShared(); };
	template <typename Ret, typename Functor, typename... Args>
	concept CDelegateFunctor = (std::is_void_v<Ret> ? std::is_invocable_v<Functor&, Args...> : std::is_invocable_r_v<Ret, Functor&, Args...>);
}

template <typename DelegateSignature, typename UserPolicy = FDefaultDelegateUserPolicy> class TDelegate;
template <typename DelegateSignature, typename UserPolicy = FDefaultDelegateUserPolicy> class TMulticastDelegate;

template <typename InRetValType, typename... ParamTypes, typename UserPolicy>
class TDelegate<InRetValType(ParamTypes...), UserPolicy> : public TDelegateBase<UserPolicy>
{
public:
	using RetValType = InRetValType;
	using TFuncType = InRetValType(ParamTypes...);
	template <typename... VarTypes> using TFuncPtr = RetValType (*)(ParamTypes..., VarTypes...);
	template <typename UserClass, typename... VarTypes> using TMethodPtr = typename TMemFunPtrType<false, UserClass, RetValType(ParamTypes..., VarTypes...)>::Type;
	template <typename UserClass, typename... VarTypes> using TConstMethodPtr = typename TMemFunPtrType<true, UserClass, RetValType(ParamTypes..., VarTypes...)>::Type;

	TDelegate() = default;
	TDelegate(TYPE_OF_NULLPTR) {}
	TDelegate(const TDelegate& Other) = default;
	TDelegate(TDelegate&& Other) = default;
	TDelegate& operator=(const TDelegate& Other) = default;
	TDelegate& operator=(TDelegate&& Other) = default;
	~TDelegate() = default;

	// ---- factories ----
	template <typename... VarTypes>
	[[nodiscard]] static TDelegate CreateStatic(std::type_identity_t<RetValType (*)(ParamTypes..., std::decay_t<VarTypes>...)> InFunc, VarTypes&&... Vars) UE_STUB_BODY
	template <typename FunctorType, typename... VarTypes>
		requires UE::Stub::CDelegateFunctor<InRetValType, std::decay_t<FunctorType>, ParamTypes..., std::decay_t<VarTypes>...>
	[[nodiscard]] static TDelegate CreateLambda(FunctorType&& InFunctor, VarTypes&&... Vars) UE_STUB_BODY
	template <typename UserClass, typename FunctorType, typename... VarTypes>
		requires UE::Stub::CDelegateFunctor<InRetValType, std::decay_t<FunctorType>, ParamTypes..., std::decay_t<VarTypes>...>
			&& UE::Stub::CDelegateUObject<UserClass>
	[[nodiscard]] static TDelegate CreateWeakLambda(UserClass* InUserObject, FunctorType&& InFunctor, VarTypes&&... Vars) UE_STUB_BODY
	template <typename UserClass, ESPMode Mode, typename FunctorType, typename... VarTypes>
		requires UE::Stub::CDelegateFunctor<InRetValType, std::decay_t<FunctorType>, ParamTypes..., std::decay_t<VarTypes>...>
	[[nodiscard]] static TDelegate CreateSPLambda(const TSharedRef<UserClass, Mode>& InUserObjectRef, FunctorType&& InFunctor, VarTypes&&... Vars) UE_STUB_BODY
	template <typename UserClass, typename FunctorType, typename... VarTypes>
		requires UE::Stub::CDelegateFunctor<InRetValType, std::decay_t<FunctorType>, ParamTypes..., std::decay_t<VarTypes>...> && UE::Stub::CDelegateSharedFromThis<UserClass>
	[[nodiscard]] static TDelegate CreateSPLambda(UserClass* InUserObject, FunctorType&& InFunctor, VarTypes&&... Vars) UE_STUB_BODY
	template <typename UserClass, typename... VarTypes> requires (!std::is_const_v<UserClass>)
	[[nodiscard]] static TDelegate CreateRaw(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, RetValType(ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars) UE_STUB_BODY
	template <typename UserClass, typename... VarTypes>
	[[nodiscard]] static TDelegate CreateRaw(const UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, RetValType(ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars) UE_STUB_BODY
	template <typename UserClass, ESPMode Mode, typename... VarTypes> requires (!std::is_const_v<UserClass>)
	[[nodiscard]] static TDelegate CreateSP(const TSharedRef<UserClass, Mode>& InUserObjectRef, typename TMemFunPtrType<false, UserClass, RetValType(ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars) UE_STUB_BODY
	template <typename UserClass, ESPMode Mode, typename... VarTypes>
	[[nodiscard]] static TDelegate CreateSP(const TSharedRef<UserClass, Mode>& InUserObjectRef, typename TMemFunPtrType<true, UserClass, RetValType(ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars) UE_STUB_BODY
	template <typename UserClass, typename... VarTypes> requires (!std::is_const_v<UserClass>) && UE::Stub::CDelegateSharedFromThis<UserClass>
	[[nodiscard]] static TDelegate CreateSP(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, RetValType(ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars) UE_STUB_BODY
	template <typename UserClass, typename... VarTypes> requires UE::Stub::CDelegateSharedFromThis<const UserClass>
	[[nodiscard]] static TDelegate CreateSP(const UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, RetValType(ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars) UE_STUB_BODY
	template <typename UserClass, typename... VarTypes> requires (!std::is_const_v<UserClass>) && UE::Stub::CDelegateSharedFromThis<UserClass>
	[[nodiscard]] static TDelegate CreateThreadSafeSP(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, RetValType(ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars) UE_STUB_BODY
	template <typename UObjectTemplate, typename... VarTypes> requires UE::Stub::CDelegateUObject<UObjectTemplate>
	[[nodiscard]] static TDelegate CreateUFunction(UObjectTemplate* InUserObject, const FName& InFunctionName, VarTypes&&... Vars) UE_STUB_BODY
	template <typename UserClass, typename... VarTypes> requires (!std::is_const_v<UserClass>) && UE::Stub::CDelegateUObject<UserClass>
	[[nodiscard]] static TDelegate CreateUObject(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, RetValType(ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars) UE_STUB_BODY
	template <typename UserClass, typename... VarTypes> requires UE::Stub::CDelegateUObject<UserClass>
	[[nodiscard]] static TDelegate CreateUObject(const UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, RetValType(ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars) UE_STUB_BODY
	template <typename UserClass, typename... VarTypes> requires UE::Stub::CDelegateUObject<UserClass>
	[[nodiscard]] static TDelegate CreateUObject(TObjectPtr<UserClass> InUserObject, typename TMemFunPtrType<false, UserClass, RetValType(ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars) UE_STUB_BODY
	template <typename UserClass, typename... VarTypes> requires UE::Stub::CDelegateUObject<UserClass>
	[[nodiscard]] static TDelegate CreateUObject(TObjectPtr<UserClass> InUserObject, typename TMemFunPtrType<true, UserClass, RetValType(ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars) UE_STUB_BODY

	// ---- binders (same shapes as the factories) ----
	template <typename... VarTypes>
	void BindStatic(std::type_identity_t<RetValType (*)(ParamTypes..., std::decay_t<VarTypes>...)> InFunc, VarTypes&&... Vars) UE_STUB_BODY
	template <typename FunctorType, typename... VarTypes>
		requires UE::Stub::CDelegateFunctor<InRetValType, std::decay_t<FunctorType>, ParamTypes..., std::decay_t<VarTypes>...>
	void BindLambda(FunctorType&& InFunctor, VarTypes&&... Vars) UE_STUB_BODY
	template <typename UserClass, typename FunctorType, typename... VarTypes>
		requires UE::Stub::CDelegateFunctor<InRetValType, std::decay_t<FunctorType>, ParamTypes..., std::decay_t<VarTypes>...>
			&& UE::Stub::CDelegateUObject<UserClass>
	void BindWeakLambda(UserClass* InUserObject, FunctorType&& InFunctor, VarTypes&&... Vars) UE_STUB_BODY
	template <typename UserClass, typename FunctorType, typename... VarTypes>
		requires UE::Stub::CDelegateFunctor<InRetValType, std::decay_t<FunctorType>, ParamTypes..., std::decay_t<VarTypes>...> && UE::Stub::CDelegateSharedFromThis<UserClass>
	void BindSPLambda(UserClass* InUserObject, FunctorType&& InFunctor, VarTypes&&... Vars) UE_STUB_BODY
	template <typename UserClass, typename... VarTypes> requires (!std::is_const_v<UserClass>)
	void BindRaw(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, RetValType(ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars) UE_STUB_BODY
	template <typename UserClass, typename... VarTypes>
	void BindRaw(const UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, RetValType(ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars) UE_STUB_BODY
	template <typename UserClass, ESPMode Mode, typename... VarTypes> requires (!std::is_const_v<UserClass>)
	void BindSP(const TSharedRef<UserClass, Mode>& InUserObjectRef, typename TMemFunPtrType<false, UserClass, RetValType(ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars) UE_STUB_BODY
	template <typename UserClass, ESPMode Mode, typename... VarTypes>
	void BindSP(const TSharedRef<UserClass, Mode>& InUserObjectRef, typename TMemFunPtrType<true, UserClass, RetValType(ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars) UE_STUB_BODY
	template <typename UserClass, typename... VarTypes> requires (!std::is_const_v<UserClass>) && UE::Stub::CDelegateSharedFromThis<UserClass>
	void BindSP(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, RetValType(ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars) UE_STUB_BODY
	template <typename UserClass, typename... VarTypes> requires UE::Stub::CDelegateSharedFromThis<const UserClass>
	void BindSP(const UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, RetValType(ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars) UE_STUB_BODY
	template <typename UObjectTemplate, typename... VarTypes> requires UE::Stub::CDelegateUObject<UObjectTemplate>
	void BindUFunction(UObjectTemplate* InUserObject, const FName& InFunctionName, VarTypes&&... Vars) UE_STUB_BODY
	template <typename UserClass, typename... VarTypes> requires (!std::is_const_v<UserClass>) && UE::Stub::CDelegateUObject<UserClass>
	void BindUObject(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, RetValType(ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars) UE_STUB_BODY
	template <typename UserClass, typename... VarTypes> requires UE::Stub::CDelegateUObject<UserClass>
	void BindUObject(const UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, RetValType(ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars) UE_STUB_BODY
	template <typename UserClass, typename... VarTypes> requires UE::Stub::CDelegateUObject<UserClass>
	void BindUObject(TObjectPtr<UserClass> InUserObject, typename TMemFunPtrType<false, UserClass, RetValType(ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars) UE_STUB_BODY

	RetValType Execute(ParamTypes... Params) const UE_STUB_BODY
	/** Only delegates without a return value have ExecuteIfBound (the engine constrains it the same way). */
	template <typename DummyRetValType = RetValType> requires std::is_void_v<DummyRetValType>
	bool ExecuteIfBound(ParamTypes... Params) const UE_STUB_BODY
};

template <typename... ParamTypes, typename UserPolicy>
class TMulticastDelegate<void(ParamTypes...), UserPolicy>
{
public:
	using FDelegate = TDelegate<void(ParamTypes...), UserPolicy>;
	TMulticastDelegate() = default;
	TMulticastDelegate(const TMulticastDelegate& Other) = default;
	TMulticastDelegate(TMulticastDelegate&& Other) = default;
	TMulticastDelegate& operator=(const TMulticastDelegate& Other) = default;
	TMulticastDelegate& operator=(TMulticastDelegate&& Other) = default;

	FDelegateHandle Add(FDelegate&& InNewDelegate) UE_STUB_BODY
	FDelegateHandle Add(const FDelegate& InNewDelegate) UE_STUB_BODY
	template <typename... VarTypes>
	FDelegateHandle AddStatic(std::type_identity_t<void (*)(ParamTypes..., std::decay_t<VarTypes>...)> InFunc, VarTypes&&... Vars) UE_STUB_BODY
	template <typename FunctorType, typename... VarTypes>
		requires std::is_invocable_v<std::decay_t<FunctorType>&, ParamTypes..., std::decay_t<VarTypes>...>
	FDelegateHandle AddLambda(FunctorType&& InFunctor, VarTypes&&... Vars) UE_STUB_BODY
	template <typename UserClass, typename FunctorType, typename... VarTypes>
		requires std::is_invocable_v<std::decay_t<FunctorType>&, ParamTypes..., std::decay_t<VarTypes>...>
			&& UE::Stub::CDelegateUObject<UserClass>
	FDelegateHandle AddWeakLambda(UserClass* InUserObject, FunctorType&& InFunctor, VarTypes&&... Vars) UE_STUB_BODY
	template <typename UserClass, typename FunctorType, typename... VarTypes>
		requires std::is_invocable_v<std::decay_t<FunctorType>&, ParamTypes..., std::decay_t<VarTypes>...> && UE::Stub::CDelegateSharedFromThis<UserClass>
	FDelegateHandle AddSPLambda(UserClass* InUserObject, FunctorType&& InFunctor, VarTypes&&... Vars) UE_STUB_BODY
	template <typename UserClass, typename... VarTypes> requires (!std::is_const_v<UserClass>)
	FDelegateHandle AddRaw(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, void(ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars) UE_STUB_BODY
	template <typename UserClass, typename... VarTypes>
	FDelegateHandle AddRaw(const UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, void(ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars) UE_STUB_BODY
	template <typename UserClass, ESPMode Mode, typename... VarTypes> requires (!std::is_const_v<UserClass>)
	FDelegateHandle AddSP(const TSharedRef<UserClass, Mode>& InUserObjectRef, typename TMemFunPtrType<false, UserClass, void(ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars) UE_STUB_BODY
	template <typename UserClass, ESPMode Mode, typename... VarTypes>
	FDelegateHandle AddSP(const TSharedRef<UserClass, Mode>& InUserObjectRef, typename TMemFunPtrType<true, UserClass, void(ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars) UE_STUB_BODY
	template <typename UserClass, typename... VarTypes> requires (!std::is_const_v<UserClass>) && UE::Stub::CDelegateSharedFromThis<UserClass>
	FDelegateHandle AddSP(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, void(ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars) UE_STUB_BODY
	template <typename UserClass, typename... VarTypes> requires UE::Stub::CDelegateSharedFromThis<const UserClass>
	FDelegateHandle AddSP(const UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, void(ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars) UE_STUB_BODY
	template <typename UserClass, typename... VarTypes> requires (!std::is_const_v<UserClass>) && UE::Stub::CDelegateSharedFromThis<UserClass>
	FDelegateHandle AddThreadSafeSP(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, void(ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars) UE_STUB_BODY
	template <typename UObjectTemplate, typename... VarTypes> requires UE::Stub::CDelegateUObject<UObjectTemplate>
	FDelegateHandle AddUFunction(UObjectTemplate* InUserObject, const FName& InFunctionName, VarTypes&&... Vars) UE_STUB_BODY
	template <typename UserClass, typename... VarTypes> requires (!std::is_const_v<UserClass>) && UE::Stub::CDelegateUObject<UserClass>
	FDelegateHandle AddUObject(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, void(ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars) UE_STUB_BODY
	template <typename UserClass, typename... VarTypes> requires UE::Stub::CDelegateUObject<UserClass>
	FDelegateHandle AddUObject(const UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, void(ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars) UE_STUB_BODY
	template <typename UserClass, typename... VarTypes> requires UE::Stub::CDelegateUObject<UserClass>
	FDelegateHandle AddUObject(TObjectPtr<UserClass> InUserObject, typename TMemFunPtrType<false, UserClass, void(ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars) UE_STUB_BODY

	bool Remove(FDelegateHandle Handle) UE_STUB_BODY
	int32 RemoveAll(const void* InUserObject) UE_STUB_BODY
	void Clear() UE_STUB_BODY
	bool IsBound() const UE_STUB_BODY
	bool IsBoundToObject(const void* InUserObject) const UE_STUB_BODY
	void Broadcast(ParamTypes... Params) const UE_STUB_BODY
};
template <typename RetValType, typename... ParamTypes, typename UserPolicy>
class TMulticastDelegate<RetValType(ParamTypes...), UserPolicy>
{
	static_assert(sizeof(RetValType) == 0, "The return type of a multicast delegate must be void");
};
template <typename DelegateSignature> using TTSDelegate = TDelegate<DelegateSignature, FDefaultTSDelegateUserPolicy>;
template <typename DelegateSignature> using TTSMulticastDelegate = TMulticastDelegate<DelegateSignature, FDefaultTSDelegateUserPolicy>;

using FSimpleDelegate = TDelegate<void()>;
using FSimpleMulticastDelegate = TMulticastDelegate<void()>;
using FTSSimpleMulticastDelegate = TTSMulticastDelegate<void()>;

#include "Delegates/DelegateCombinations.h"

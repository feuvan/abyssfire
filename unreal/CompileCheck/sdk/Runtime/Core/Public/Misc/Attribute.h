// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Misc/Attribute.h (from UE 5.x source knowledge).
// TAttribute<T>: a value or a bound getter (TDelegate<T()>). Implicitly constructible from anything T is constructible
// from; Create / CreateLambda / CreateSP / CreateRaw / CreateUObject / CreateStatic bind getters, which must return
// something convertible to T. Not part of CoreMinimal.h (Slate's DeclarativeSyntaxSupport.h brings it in).
#pragma once

#include "CoreTypes.h"
#include "Delegates/Delegate.h"
#include "Templates/Function.h"
#include "Templates/SharedPointer.h"

template <typename ObjectType>
class TAttribute
{
public:
	DECLARE_DELEGATE_RetVal(ObjectType, FGetter);

	TAttribute() : Value(), bIsSet(false) {}
	template <typename OtherType> requires std::is_constructible_v<ObjectType, const OtherType&>
		&& (!std::is_same_v<std::decay_t<OtherType>, TAttribute>)
	TAttribute(const OtherType& InInitialValue) : Value(static_cast<ObjectType>(InInitialValue)), bIsSet(true) {}
	TAttribute(ObjectType&& InInitialValue) : Value(static_cast<ObjectType&&>(InInitialValue)), bIsSet(true) {}
	template <class SourceType>
	TAttribute(TSharedRef<SourceType> InUserObject, typename FGetter::template TConstMethodPtr<SourceType> InMethodPtr) : Value(), bIsSet(true) {}
	template <class SourceType> requires std::is_base_of_v<TSharedFromThis<SourceType>, SourceType> || UE::Stub::CDelegateSharedFromThis<const SourceType>
	TAttribute(const SourceType* InUserObject, typename FGetter::template TConstMethodPtr<SourceType> InMethodPtr) : Value(), bIsSet(true) {}
	TAttribute(const TAttribute&) = default;
	TAttribute(TAttribute&&) = default;
	TAttribute& operator=(const TAttribute&) = default;
	TAttribute& operator=(TAttribute&&) = default;

	[[nodiscard]] static TAttribute Create(const FGetter& InGetter) UE_STUB_BODY
	[[nodiscard]] static TAttribute Create(FGetter&& InGetter) UE_STUB_BODY
	template <typename FuncPtrType> requires std::is_invocable_r_v<ObjectType, FuncPtrType&>
	[[nodiscard]] static TAttribute Create(FuncPtrType&& InFuncPtr) UE_STUB_BODY
	[[nodiscard]] static TAttribute Create(TFunction<ObjectType(void)>&& InFuncPtr) UE_STUB_BODY
	template <typename... VarTypes>
	[[nodiscard]] static TAttribute CreateStatic(std::type_identity_t<ObjectType (*)(std::decay_t<VarTypes>...)> InFuncPtr, VarTypes&&... Vars) UE_STUB_BODY
	template <typename LambdaType, typename... PayloadTypes> requires std::is_invocable_r_v<ObjectType, std::decay_t<LambdaType>&, std::decay_t<PayloadTypes>...>
	[[nodiscard]] static TAttribute CreateLambda(LambdaType&& InCallable, PayloadTypes&&... InputPayload) UE_STUB_BODY
	template <typename SourceType, typename SourceTypeOrBase, typename... PayloadTypes> requires std::is_base_of_v<SourceTypeOrBase, SourceType>
	[[nodiscard]] static TAttribute CreateRaw(const SourceType* InObject, ObjectType (SourceTypeOrBase::*InMethod)(PayloadTypes...) const, std::type_identity_t<PayloadTypes>... InputPayload) UE_STUB_BODY
	template <typename SourceType, typename SourceTypeOrBase, typename... PayloadTypes> requires std::is_base_of_v<SourceTypeOrBase, SourceType> && UE::Stub::CDelegateSharedFromThis<const SourceType>
	[[nodiscard]] static TAttribute CreateSP(const SourceType* InObject, ObjectType (SourceTypeOrBase::*InMethod)(PayloadTypes...) const, std::type_identity_t<PayloadTypes>... InputPayload) UE_STUB_BODY
	template <typename SourceType, typename SourceTypeOrBase, typename... PayloadTypes> requires std::is_base_of_v<SourceTypeOrBase, SourceType> && UE::Stub::CDelegateUObject<SourceType>
	[[nodiscard]] static TAttribute CreateUObject(const SourceType* InObject, ObjectType (SourceTypeOrBase::*InMethod)(PayloadTypes...) const, std::type_identity_t<PayloadTypes>... InputPayload) UE_STUB_BODY

	template <typename OtherType> requires std::is_constructible_v<ObjectType, const OtherType&>
	void Set(const OtherType& InNewValue) UE_STUB_BODY
	void Set(ObjectType&& InNewValue) UE_STUB_BODY
	bool IsSet() const { return bIsSet; }
	const ObjectType& Get() const UE_STUB_BODY
	const ObjectType& Get(const ObjectType& DefaultValue) const UE_STUB_BODY
	void Bind(const FGetter& InGetter) UE_STUB_BODY
	void Bind(FGetter&& InGetter) UE_STUB_BODY
	template <typename LambdaType> requires std::is_invocable_r_v<ObjectType, std::decay_t<LambdaType>&>
	void BindLambda(LambdaType&& InCallable) UE_STUB_BODY
	template <typename... VarTypes>
	void BindStatic(std::type_identity_t<ObjectType (*)(std::decay_t<VarTypes>...)> InFuncPtr, VarTypes&&... Vars) UE_STUB_BODY
	template <class SourceType> void BindRaw(const SourceType* InUserObject, typename FGetter::template TConstMethodPtr<SourceType> InMethodPtr) UE_STUB_BODY
	template <class SourceType> void Bind(TSharedRef<SourceType> InUserObject, typename FGetter::template TConstMethodPtr<SourceType> InMethodPtr) UE_STUB_BODY
	template <class SourceType> void Bind(SourceType* InUserObject, typename FGetter::template TConstMethodPtr<SourceType> InMethodPtr) UE_STUB_BODY
	template <class SourceType> void BindUObject(SourceType* InUserObject, typename FGetter::template TConstMethodPtr<SourceType> InMethodPtr) UE_STUB_BODY
	template <class SourceType> void BindUFunction(SourceType* InUserObject, const FName& InFunctionName) UE_STUB_BODY
	bool IsBound() const UE_STUB_BODY
	const FGetter& GetBinding() const UE_STUB_BODY
	bool IdenticalTo(const TAttribute& InOther) const UE_STUB_BODY
private:
	mutable ObjectType Value;
	bool bIsSet;
};

template <typename LambdaType, typename... PayloadTypes>
[[nodiscard]] auto MakeAttributeLambda(LambdaType&& InCallable, PayloadTypes&&... InputPayload) -> TAttribute<std::decay_t<decltype(InCallable(static_cast<PayloadTypes&&>(InputPayload)...))>> UE_STUB_BODY
template <typename SourceType, typename SourceTypeOrBase, typename ResultType, typename... PayloadTypes>
[[nodiscard]] TAttribute<ResultType> MakeAttributeSP(const SourceType* InObject, ResultType (SourceTypeOrBase::*InMethod)(PayloadTypes...) const, std::type_identity_t<PayloadTypes>... InputPayload) UE_STUB_BODY
template <typename SourceType, typename SourceTypeOrBase, typename ResultType, typename... PayloadTypes>
[[nodiscard]] TAttribute<ResultType> MakeAttributeUObject(const SourceType* InObject, ResultType (SourceTypeOrBase::*InMethod)(PayloadTypes...) const, std::type_identity_t<PayloadTypes>... InputPayload) UE_STUB_BODY

// Stub SDK. Real header: Engine/Plugins/EnhancedInput/Source/EnhancedInput/Public/EnhancedInputComponent.h (UE 5.x
// source knowledge; reflected class: checked against the 5.6 stub).
// BindAction(Action, TriggerEvent, Object, &Class::Handler[, Payload...]): the handler takes nothing, a
// const FInputActionValue& or a const FInputActionInstance&, followed by the payload types. The legacy UInputComponent
// helpers (BindAction by name, BindAxis, BindKey, BindTouch, ...) are deleted on this class, as in the engine.
#pragma once

#include "CoreMinimal.h"
#include "Components/InputComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputTriggers.h"

DECLARE_DELEGATE(FEnhancedInputActionHandlerSignature);
DECLARE_DELEGATE_OneParam(FEnhancedInputActionHandlerValueSignature, const FInputActionValue&);
DECLARE_DELEGATE_OneParam(FEnhancedInputActionHandlerInstanceSignature, const FInputActionInstance&);

struct FEnhancedInputActionEventBinding
{
public:
	FEnhancedInputActionEventBinding() = default;
	FEnhancedInputActionEventBinding(const UInputAction* InAction, ETriggerEvent InTriggerEvent);
	virtual ~FEnhancedInputActionEventBinding() = default;
	const UInputAction* GetAction() const;
	ETriggerEvent GetTriggerEvent() const;
	uint32 GetHandle() const;
};
struct FEnhancedInputActionValueBinding
{
public:
	FEnhancedInputActionValueBinding() {}
	FEnhancedInputActionValueBinding(const UInputAction* InAction);
	const UInputAction* GetAction() const;
	FInputActionValue GetValue() const;
};

class UEnhancedInputComponent : public UInputComponent
{
	UE_STUB_CLASS_BODY(UEnhancedInputComponent, UInputComponent)
public:
	bool HasBindings() const;
	bool RemoveActionEventBinding(const int32 BindingIndex);
	bool RemoveBindingByHandle(const uint32 Handle);
	bool RemoveBinding(const FEnhancedInputActionEventBinding& BindingToRemove);
	void ClearActionEventBindings();
	void ClearActionValueBindings();
	void ClearActionBindings();
	void ClearBindingsForObject(UObject* InOwner);
	FEnhancedInputActionEventBinding& BindAction(const UInputAction* Action, ETriggerEvent TriggerEvent, UObject* Object, FName FunctionName);
	template <class UserClass, typename... VarTypes>
	FEnhancedInputActionEventBinding& BindAction(const UInputAction* Action, ETriggerEvent TriggerEvent, UserClass* Object, typename FEnhancedInputActionHandlerSignature::template TMethodPtr<UserClass, VarTypes...> Func, VarTypes... Vars) UE_STUB_BODY
	template <class UserClass, typename... VarTypes>
	FEnhancedInputActionEventBinding& BindAction(const UInputAction* Action, ETriggerEvent TriggerEvent, UserClass* Object, typename FEnhancedInputActionHandlerValueSignature::template TMethodPtr<UserClass, VarTypes...> Func, VarTypes... Vars) UE_STUB_BODY
	template <class UserClass, typename... VarTypes>
	FEnhancedInputActionEventBinding& BindAction(const UInputAction* Action, ETriggerEvent TriggerEvent, UserClass* Object, typename FEnhancedInputActionHandlerInstanceSignature::template TMethodPtr<UserClass, VarTypes...> Func, VarTypes... Vars) UE_STUB_BODY
	template <typename FuncType, typename... VarTypes>
	FEnhancedInputActionEventBinding& BindActionValueLambda(const UInputAction* Action, ETriggerEvent TriggerEvent, FuncType&& Lambda, VarTypes... Vars) requires std::is_invocable_v<FuncType&, const FInputActionValue&, VarTypes...> UE_STUB_BODY
	template <typename FuncType, typename... VarTypes>
	FEnhancedInputActionEventBinding& BindActionInstanceLambda(const UInputAction* Action, ETriggerEvent TriggerEvent, FuncType&& Lambda, VarTypes... Vars) requires std::is_invocable_v<FuncType&, const FInputActionInstance&, VarTypes...> UE_STUB_BODY
	FEnhancedInputActionValueBinding& BindActionValue(const UInputAction* Action);
	FInputActionValue GetBoundActionValue(const UInputAction* Action) const;

	// The legacy binding helpers are deleted on UEnhancedInputComponent ("Hide away legacy binding helpers").
	template <class UserClass>
	FInputActionBinding& BindAction(const FName ActionName, const EInputEvent KeyEvent, UserClass* Object, typename FInputActionHandlerSignature::TMethodPtr<UserClass> Func) = delete;
	template <class UserClass>
	FInputAxisBinding& BindAxis(const FName AxisName, UserClass* Object, typename FInputAxisHandlerSignature::TMethodPtr<UserClass> Func) = delete;
	FInputAxisBinding& BindAxis(const FName AxisName) = delete;
	template <class UserClass>
	FInputKeyBinding& BindKey(const FKey Key, const EInputEvent KeyEvent, UserClass* Object, typename FInputActionHandlerSignature::TMethodPtr<UserClass> Func) = delete;
	template <class UserClass>
	FInputTouchBinding& BindTouch(const EInputEvent KeyEvent, UserClass* Object, typename FInputTouchHandlerSignature::TMethodPtr<UserClass> Func) = delete;
};

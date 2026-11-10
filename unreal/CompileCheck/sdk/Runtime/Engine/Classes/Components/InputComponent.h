// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Components/InputComponent.h (UE 5.x source knowledge;
// reflected class: checked against the 5.6 stub). The legacy binding helpers (BindAction by name, BindAxis, BindKey,
// BindTouch) and their handler delegate types.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineBaseTypes.h"
#include "InputCoreTypes.h"

DECLARE_DELEGATE(FInputActionHandlerSignature);
DECLARE_DELEGATE_OneParam(FInputActionHandlerWithKeySignature, FKey);
DECLARE_DELEGATE_OneParam(FInputAxisHandlerSignature, float);
DECLARE_DELEGATE_OneParam(FInputVectorAxisHandlerSignature, FVector);
DECLARE_DELEGATE_TwoParams(FInputTouchHandlerSignature, ETouchIndex::Type, FVector);
DECLARE_DELEGATE_OneParam(FInputGestureHandlerSignature, float);

struct FInputBinding
{
	uint8 bConsumeInput : 1;
	uint8 bExecuteWhenPaused : 1;
	FInputBinding();
};
struct FInputActionBinding : public FInputBinding
{
	FInputActionBinding();
	FInputActionBinding(const FName InActionName, const enum EInputEvent InKeyEvent);
	FName GetActionName() const;
};
struct FInputAxisBinding : public FInputBinding
{
	FName AxisName;
	float AxisValue;
};
struct FInputKeyBinding : public FInputBinding
{
	TEnumAsByte<EInputEvent> KeyEvent;
};
struct FInputTouchBinding : public FInputBinding
{
	TEnumAsByte<EInputEvent> KeyEvent;
};

class UInputComponent : public UActorComponent
{
	UE_STUB_CLASS_BODY(UInputComponent, UActorComponent)
public:
	TArray<FInputTouchBinding> TouchBindings;
	TArray<FInputAxisBinding> AxisBindings;
	int32 Priority;
	uint8 bBlockInput : 1;
	float GetAxisValue(const FName AxisName) const;
	bool HasBindings() const;
	void ClearActionBindings();
	void ClearBindingValues();
	template <class UserClass>
	FInputActionBinding& BindAction(const FName ActionName, const EInputEvent KeyEvent, UserClass* Object, typename FInputActionHandlerSignature::TMethodPtr<UserClass> Func) UE_STUB_BODY
	template <class UserClass>
	FInputAxisBinding& BindAxis(const FName AxisName, UserClass* Object, typename FInputAxisHandlerSignature::TMethodPtr<UserClass> Func) UE_STUB_BODY
	FInputAxisBinding& BindAxis(const FName AxisName);
	template <class UserClass>
	FInputKeyBinding& BindKey(const FKey Key, const EInputEvent KeyEvent, UserClass* Object, typename FInputActionHandlerSignature::TMethodPtr<UserClass> Func) UE_STUB_BODY
	template <class UserClass>
	FInputTouchBinding& BindTouch(const EInputEvent KeyEvent, UserClass* Object, typename FInputTouchHandlerSignature::TMethodPtr<UserClass> Func) UE_STUB_BODY
};

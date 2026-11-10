// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Input/Events.h (UE 5.x source knowledge; reflected
// structs: checked against the 5.6 stub). Slate input events.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "InputCoreTypes.h"
#include "Types/SlateEnums.h"
#include "Types/SlateVector2.h"
#include "GenericPlatform/GenericApplication.h"
#include "Layout/Geometry.h"

class FWidgetPath;
class SWidget;
class SWindow;

UENUM(BlueprintType)
enum class EFocusCause : uint8
{
	Mouse,
	Navigation,
	SetDirectly,
	Cleared,
	OtherWidgetLostFocus,
	WindowActivate,
};

struct FFocusEvent
{
	UE_STUB_STRUCT_BODY(FFocusEvent)
	FFocusEvent();
	FFocusEvent(const EFocusCause InCause, uint32 InUserIndex);
	EFocusCause GetCause() const;
	uint32 GetUser() const;
};
struct FCaptureLostEvent
{
	UE_STUB_STRUCT_BODY(FCaptureLostEvent)
	FCaptureLostEvent();
	FCaptureLostEvent(int32 InUserIndex, int32 InPointerIndex);
	int32 UserIndex;
	int32 PointerIndex;
};
struct FInputEvent
{
	UE_STUB_STRUCT_BODY(FInputEvent)
	FInputEvent();
	FInputEvent(const FModifierKeysState& InModifierKeys, const int32 InUserIndex, const bool bInIsRepeat);
	virtual ~FInputEvent();
	bool IsRepeat() const;
	bool IsShiftDown() const;
	bool IsLeftShiftDown() const;
	bool IsRightShiftDown() const;
	bool IsControlDown() const;
	bool IsAltDown() const;
	bool IsCommandDown() const;
	bool AreCapsLocked() const;
	uint32 GetUserIndex() const;
	const FModifierKeysState& GetModifierKeys() const;
	uint64 GetEventTimestamp() const;
	const FWidgetPath* GetEventPath() const;
	virtual FText ToText() const;
	virtual bool IsPointerEvent() const;
	virtual bool IsKeyEvent() const;
};
struct FKeyEvent : public FInputEvent
{
	UE_STUB_STRUCT_BODY(FKeyEvent)
	FKeyEvent();
	FKeyEvent(const FKey InKey, const FModifierKeysState& InModifierKeys, const uint32 InUserIndex, const bool bInIsRepeat, const uint32 InCharacterCode, const uint32 InKeyCode);
	FKey GetKey() const;
	uint32 GetCharacter() const;
	uint32 GetKeyCode() const;
	virtual FText ToText() const override;
	virtual bool IsKeyEvent() const override;
};
struct FAnalogInputEvent : public FKeyEvent
{
	UE_STUB_STRUCT_BODY(FAnalogInputEvent)
	FAnalogInputEvent();
	FAnalogInputEvent(const FKey InKey, const FModifierKeysState& InModifierKeys, const uint32 InUserIndex, const bool bInIsRepeat, const uint32 InCharacterCode, const uint32 InKeyCode, const float InAnalogValue);
	float GetAnalogValue() const;
};
struct FCharacterEvent : public FInputEvent
{
	UE_STUB_STRUCT_BODY(FCharacterEvent)
	FCharacterEvent();
	TCHAR GetCharacter() const;
};
struct FPointerEvent : public FInputEvent
{
	UE_STUB_STRUCT_BODY(FPointerEvent)
	FPointerEvent();
	const UE::Slate::FDeprecateVector2DResult& GetScreenSpacePosition() const;
	const UE::Slate::FDeprecateVector2DResult& GetLastScreenSpacePosition() const;
	const UE::Slate::FDeprecateVector2DResult& GetCursorDelta() const;
	bool IsMouseButtonDown(FKey MouseButton) const;
	FKey GetEffectingButton() const;
	float GetWheelDelta() const;
	uint32 GetPointerIndex() const;
	uint32 GetTouchpadIndex() const;
	float GetTouchForce() const;
	bool IsTouchEvent() const;
	bool IsTouchForceChangedEvent() const;
	bool IsTouchFirstMoveEvent() const;
	const UE::Slate::FDeprecateVector2DResult& GetGestureDelta() const;
	bool IsDirectionInvertedFromDevice() const;
	const TSet<FKey>& GetPressedButtons() const;
	virtual FText ToText() const override;
	virtual bool IsPointerEvent() const override;
	/** Copies the event and rewrites its (protected) screen-space positions: only for FPointerEvent and types derived from it. */
	template <typename PointerEventType> requires std::is_base_of_v<FPointerEvent, PointerEventType>
	static PointerEventType MakeTranslatedEvent(const PointerEventType& InPointerEvent, const class FVirtualPointerPosition& VirtualPosition) UE_STUB_BODY
};
struct FMotionEvent : public FInputEvent
{
	UE_STUB_STRUCT_BODY(FMotionEvent)
	FMotionEvent();
	const FVector& GetTilt() const;
	const FVector& GetRotationRate() const;
	const FVector& GetGravity() const;
	const FVector& GetAcceleration() const;
};
struct FNavigationEvent : public FInputEvent
{
	UE_STUB_STRUCT_BODY(FNavigationEvent)
	FNavigationEvent();
	EUINavigation GetNavigationType() const;
	ENavigationGenesis GetNavigationGenesis() const;
};
struct FWindowActivateEvent
{
	enum EActivationType { EA_Activate, EA_ActivateByMouse, EA_Deactivate };
	EActivationType GetActivationType() const;
	TSharedRef<SWindow> GetAffectedWindow() const;
};

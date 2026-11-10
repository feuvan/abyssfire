// Stub SDK. Real header: Engine/Source/Runtime/InputCore/Classes/InputCoreTypes.h (UE 5.x source knowledge; FKey /
// ETouchIndex / EControllerHand are reflected: checked against the 5.6 stub). FKey, EKeys, touch indices.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Misc/Attribute.h"

struct FKeyDetails;

struct FKey
{
	UE_STUB_STRUCT_BODY(FKey)
	FKey() {}
	FKey(const FName InName);
	FKey(const TCHAR* InName);
	FKey(const ANSICHAR* InName);
	bool IsValid() const;
	bool IsModifierKey() const;
	bool IsGamepadKey() const;
	bool IsTouch() const;
	bool IsMouseButton() const;
	bool IsButtonAxis() const;
	bool IsAxis1D() const;
	bool IsAxis2D() const;
	bool IsAxis3D() const;
	bool IsDigital() const;
	bool IsAnalog() const;
	bool IsBindableInBlueprints() const;
	bool ShouldUpdateAxisWithoutSamples() const;
	bool IsBindableToActions() const;
	bool IsDeprecated() const;
	bool IsGesture() const;
	FText GetDisplayName(bool bLongDisplayName = true) const;
	FString ToString() const;
	FName GetFName() const;
	FName GetMenuCategory() const;
	FKey GetPairedAxisKey() const;
	friend bool operator==(const FKey& KeyA, const FKey& KeyB);
	friend bool operator!=(const FKey& KeyA, const FKey& KeyB);
	friend bool operator<(const FKey& KeyA, const FKey& KeyB);
	friend uint32 GetTypeHash(const FKey& Key);
};

UENUM(BlueprintType, Blueprintable)
namespace ETouchIndex
{
	enum Type : int
	{
		Touch1,
		Touch2,
		Touch3,
		Touch4,
		Touch5,
		Touch6,
		Touch7,
		Touch8,
		Touch9,
		Touch10,
		CursorPointerIndex UMETA(Hidden),
		MAX_TOUCHES UMETA(Hidden),
	};
}
UENUM(BlueprintType)
enum class EControllerHand : uint8
{
	Left,
	Right,
	AnyHand,
	Pad,
	ExternalCamera,
	Gun,
	HMD,
	Chest,
	LeftShoulder,
	RightShoulder,
	LeftElbow,
	RightElbow,
	Waist,
	LeftKnee,
	RightKnee,
	LeftFoot,
	RightFoot,
	Special,
	ControllerHand_Count UMETA(Hidden, DisplayName = "<INVALID>"),
};
UENUM()
namespace ETouchType
{
	enum Type : int
	{
		Began,
		Moved,
		Stationary,
		ForceChanged,
		FirstMove,
		Ended,
		NumTypes,
	};
}

struct EKeys
{
	static const FKey AnyKey;
	static const FKey MouseX;
	static const FKey MouseY;
	static const FKey Mouse2D;
	static const FKey MouseScrollUp;
	static const FKey MouseScrollDown;
	static const FKey MouseWheelAxis;
	static const FKey LeftMouseButton;
	static const FKey RightMouseButton;
	static const FKey MiddleMouseButton;
	static const FKey ThumbMouseButton;
	static const FKey ThumbMouseButton2;
	static const FKey BackSpace;
	static const FKey Tab;
	static const FKey Enter;
	static const FKey Pause;
	static const FKey CapsLock;
	static const FKey Escape;
	static const FKey SpaceBar;
	static const FKey PageUp;
	static const FKey PageDown;
	static const FKey End;
	static const FKey Home;
	static const FKey Left;
	static const FKey Up;
	static const FKey Right;
	static const FKey Down;
	static const FKey Insert;
	static const FKey Delete;
	static const FKey Zero;
	static const FKey One;
	static const FKey Two;
	static const FKey Three;
	static const FKey Four;
	static const FKey Five;
	static const FKey Six;
	static const FKey Seven;
	static const FKey Eight;
	static const FKey Nine;
	static const FKey A;
	static const FKey B;
	static const FKey C;
	static const FKey D;
	static const FKey E;
	static const FKey F;
	static const FKey G;
	static const FKey H;
	static const FKey I;
	static const FKey J;
	static const FKey K;
	static const FKey L;
	static const FKey M;
	static const FKey N;
	static const FKey O;
	static const FKey P;
	static const FKey Q;
	static const FKey R;
	static const FKey S;
	static const FKey T;
	static const FKey U;
	static const FKey V;
	static const FKey W;
	static const FKey X;
	static const FKey Y;
	static const FKey Z;
	static const FKey NumPadZero;
	static const FKey NumPadOne;
	static const FKey NumPadTwo;
	static const FKey NumPadThree;
	static const FKey NumPadFour;
	static const FKey NumPadFive;
	static const FKey NumPadSix;
	static const FKey NumPadSeven;
	static const FKey NumPadEight;
	static const FKey NumPadNine;
	static const FKey Multiply;
	static const FKey Add;
	static const FKey Subtract;
	static const FKey Decimal;
	static const FKey Divide;
	static const FKey F1;
	static const FKey F2;
	static const FKey F3;
	static const FKey F4;
	static const FKey F5;
	static const FKey F6;
	static const FKey F7;
	static const FKey F8;
	static const FKey F9;
	static const FKey F10;
	static const FKey F11;
	static const FKey F12;
	static const FKey NumLock;
	static const FKey ScrollLock;
	static const FKey LeftShift;
	static const FKey RightShift;
	static const FKey LeftControl;
	static const FKey RightControl;
	static const FKey LeftAlt;
	static const FKey RightAlt;
	static const FKey LeftCommand;
	static const FKey RightCommand;
	static const FKey Semicolon;
	static const FKey Equals;
	static const FKey Comma;
	static const FKey Underscore;
	static const FKey Hyphen;
	static const FKey Period;
	static const FKey Slash;
	static const FKey Tilde;
	static const FKey LeftBracket;
	static const FKey Backslash;
	static const FKey RightBracket;
	static const FKey Apostrophe;
	static const FKey Ampersand;
	static const FKey Asterix;
	static const FKey Caret;
	static const FKey Colon;
	static const FKey Dollar;
	static const FKey Exclamation;
	static const FKey LeftParantheses;
	static const FKey RightParantheses;
	static const FKey Quote;
	static const FKey Gamepad_Left2D;
	static const FKey Gamepad_LeftX;
	static const FKey Gamepad_LeftY;
	static const FKey Gamepad_Right2D;
	static const FKey Gamepad_RightX;
	static const FKey Gamepad_RightY;
	static const FKey Gamepad_LeftTriggerAxis;
	static const FKey Gamepad_RightTriggerAxis;
	static const FKey Gamepad_LeftThumbstick;
	static const FKey Gamepad_RightThumbstick;
	static const FKey Gamepad_Special_Left;
	static const FKey Gamepad_Special_Left_X;
	static const FKey Gamepad_Special_Left_Y;
	static const FKey Gamepad_Special_Right;
	static const FKey Gamepad_FaceButton_Bottom;
	static const FKey Gamepad_FaceButton_Right;
	static const FKey Gamepad_FaceButton_Left;
	static const FKey Gamepad_FaceButton_Top;
	static const FKey Gamepad_LeftShoulder;
	static const FKey Gamepad_RightShoulder;
	static const FKey Gamepad_LeftTrigger;
	static const FKey Gamepad_RightTrigger;
	static const FKey Gamepad_DPad_Up;
	static const FKey Gamepad_DPad_Down;
	static const FKey Gamepad_DPad_Right;
	static const FKey Gamepad_DPad_Left;
	static const FKey Gamepad_LeftStick_Up;
	static const FKey Gamepad_LeftStick_Down;
	static const FKey Gamepad_LeftStick_Right;
	static const FKey Gamepad_LeftStick_Left;
	static const FKey Gamepad_RightStick_Up;
	static const FKey Gamepad_RightStick_Down;
	static const FKey Gamepad_RightStick_Right;
	static const FKey Gamepad_RightStick_Left;
	static const FKey Tilt;
	static const FKey RotationRate;
	static const FKey Gravity;
	static const FKey Acceleration;
	static const FKey Gesture_Pinch;
	static const FKey Gesture_Flick;
	static const FKey Gesture_Rotate;
	static const FKey Android_Back;
	static const FKey Android_Volume_Up;
	static const FKey Android_Volume_Down;
	static const FKey Android_Menu;
	static const FKey Invalid;
	static const FKey Virtual_Accept;
	static const FKey Virtual_Back;
	static constexpr int32 NUM_TOUCH_KEYS = 11;
	static const FKey TouchKeys[NUM_TOUCH_KEYS];
	static const FName NAME_GamepadCategory;
	static const FName NAME_MouseCategory;
	static const FName NAME_KeyboardCategory;
	static void Initialize();
	static void GetAllKeys(TArray<FKey>& OutKeys);
	static bool IsModifierKey(FKey Key);
	static bool IsGamepadKey(FKey Key);
	static bool IsAxis(FKey Key);
	static bool IsBindableInBlueprints(const FKey Key);
};

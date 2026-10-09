#include "AbyssInputDeviceTracker.h"

#include "Input/Events.h"
#include "InputCoreTypes.h"

#include "Input/AbyssInputSubsystem.h"

namespace
{
	// A resting stick reports small values; only a deliberate push switches the hints to the gamepad.
	constexpr float AbyssDeviceTracker_AnalogThreshold = 0.3f;
	// Mouse jitter (Slate units) that does not count as "using the mouse".
	constexpr double AbyssDeviceTracker_MouseMoveThreshold = 3.0;
}

FAbyssInputDeviceTracker::FAbyssInputDeviceTracker(UAbyssInputSubsystem& InOwner)
	: Owner(&InOwner)
{
}

void FAbyssInputDeviceTracker::Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor)
{
}

bool FAbyssInputDeviceTracker::HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	Report(InKeyEvent.GetKey().IsGamepadKey() ? EAbyssInputDevice::Gamepad : EAbyssInputDevice::KeyboardMouse);
	return false;
}

bool FAbyssInputDeviceTracker::HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent)
{
	if (InAnalogInputEvent.GetKey().IsGamepadKey() && FMath::Abs(InAnalogInputEvent.GetAnalogValue()) > AbyssDeviceTracker_AnalogThreshold)
	{
		Report(EAbyssInputDevice::Gamepad);
	}
	return false;
}

bool FAbyssInputDeviceTracker::HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.IsTouchEvent())
	{
		return false;   // finger drags: the press already reported Touch
	}
	if (FVector2D(MouseEvent.GetCursorDelta()).Size() > AbyssDeviceTracker_MouseMoveThreshold)
	{
		Report(EAbyssInputDevice::KeyboardMouse);
	}
	return false;
}

bool FAbyssInputDeviceTracker::HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	Report(MouseEvent.IsTouchEvent() ? EAbyssInputDevice::Touch : EAbyssInputDevice::KeyboardMouse);
	return false;
}

void FAbyssInputDeviceTracker::Report(EAbyssInputDevice Device) const
{
	if (UAbyssInputSubsystem* Subsystem = Owner.Get())
	{
		Subsystem->NotifyInputDevice(Device);
	}
}

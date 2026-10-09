// Slate input pre-processor that records which device the player is using (keyboard + mouse, gamepad, touch) for button
// hints and the teleport aim source. It never consumes input.
#pragma once

#include "CoreMinimal.h"
#include "Framework/Application/IInputProcessor.h"
#include "UObject/WeakObjectPtr.h"

#include "Input/AbyssInputTypes.h"

class UAbyssInputSubsystem;

class FAbyssInputDeviceTracker final : public IInputProcessor
{
public:
	explicit FAbyssInputDeviceTracker(UAbyssInputSubsystem& InOwner);

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override;
	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
	virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent) override;
	virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;

private:
	void Report(EAbyssInputDevice Device) const;

	TWeakObjectPtr<UAbyssInputSubsystem> Owner;
};

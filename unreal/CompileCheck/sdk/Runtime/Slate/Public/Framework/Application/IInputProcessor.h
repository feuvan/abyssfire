// Stub SDK. Real header: Engine/Source/Runtime/Slate/Public/Framework/Application/IInputProcessor.h (UE 5.x source
// knowledge). An input pre-processor sees events before Slate widgets; returning true swallows the event. Tick is pure.
#pragma once

#include "CoreMinimal.h"
#include "Input/Events.h"

class FSlateApplication;
class ICursor;

class IInputProcessor
{
public:
	IInputProcessor() {}
	virtual ~IInputProcessor() {}
	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) = 0;
	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) { return false; }
	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) { return false; }
	virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent) { return false; }
	virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) { return false; }
	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) { return false; }
	virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) { return false; }
	virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) { return false; }
	virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& InWheelEvent, const FPointerEvent* InGestureEvent) { return false; }
	virtual bool HandleMotionDetectedEvent(FSlateApplication& SlateApp, const FMotionEvent& MotionEvent) { return false; }
	virtual const TCHAR* GetDebugName() const { return TEXT(""); }
};

// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Types/WidgetActiveTimerDelegate.h (UE 5.x source
// knowledge). An active timer returns Continue or Stop.
#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"

enum class EActiveTimerReturnType : uint8
{
	Continue,
	Stop,
};
DECLARE_DELEGATE_RetVal_TwoParams(EActiveTimerReturnType, FWidgetActiveTimerDelegate, double /*InCurrentTime*/, float /*InDeltaTime*/);
class FActiveTimerHandle
{
public:
	bool IsPendingExecution() const;
	bool IsReady(double CurrentTime) const;
};

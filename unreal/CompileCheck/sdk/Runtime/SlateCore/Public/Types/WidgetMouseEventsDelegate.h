// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Types/WidgetMouseEventsDelegate.h (UE 5.x source
// knowledge).
#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "Input/Reply.h"

struct FGeometry;
struct FPointerEvent;
DECLARE_DELEGATE_RetVal_TwoParams(FReply, FPointerEventHandler, const FGeometry& /*MyGeometry*/, const FPointerEvent& /*MouseEvent*/);
DECLARE_DELEGATE_TwoParams(FNoReplyPointerEventHandler, const FGeometry& /*MyGeometry*/, const FPointerEvent& /*MouseEvent*/);
DECLARE_DELEGATE_OneParam(FSimpleNoReplyPointerEventHandler, const FPointerEvent& /*MouseEvent*/);

// Stub SDK. Real header: Engine/Source/Runtime/Slate/Public/Framework/SlateDelegates.h (UE 5.x source knowledge).
// The delegate types Slate widgets take as events. Only the one a widget header of the SDK names (FOnGetContent,
// FOnFloatValueChanged, FOnTextChanged, ... are added with the widget that first needs them).
#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"

/** Notification for when a button (or a clickable widget) is clicked: return FReply::Handled() to consume the click. */
DECLARE_DELEGATE_RetVal(FReply, FOnClicked)

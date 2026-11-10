// Stub SDK. Real headers: Engine/Source/Runtime/SlateCore/Public/Input/Reply.h and ReplyBase.h (UE 5.x source
// knowledge). FReply: what an event handler returns; the modifiers chain (FReply::Handled().CaptureMouse(Widget)).
#pragma once

#include "CoreMinimal.h"
#include "Input/Events.h"

class SWidget;
class FDragDropOperation;

class FReplyBase
{
public:
	bool IsEventHandled() const { return bIsHandled; }
	const TSharedPtr<SWidget> GetHandler() const;
protected:
	FReplyBase(bool InIsHandled) : bIsHandled(InIsHandled) {}
	bool bIsHandled;
};
template <typename ReplyType>
class TReplyBase : public FReplyBase
{
public:
	TReplyBase(bool bIsHandled) : FReplyBase(bIsHandled) {}
protected:
	ReplyType& Me() { return static_cast<ReplyType&>(*this); }
	ReplyType& SetHandler(const TSharedRef<SWidget>& InHandler) UE_STUB_BODY
};

class FReply : public TReplyBase<FReply>
{
public:
	FReply& CaptureMouse(TSharedRef<SWidget> InMouseCaptor);
	FReply& UseHighPrecisionMouseMovement(TSharedRef<SWidget> InMouseCaptor);
	FReply& SetMousePos(const FIntPoint& NewMousePos);
	FReply& SetUserFocus(TSharedRef<SWidget> GiveMeFocus, EFocusCause ReasonFocusIsChanging = EFocusCause::SetDirectly, bool bInAllUsers = false);
	FReply& ClearUserFocus(bool bInAllUsers = false);
	FReply& ClearUserFocus(EFocusCause ReasonFocusIsChanging, bool bInAllUsers = false);
	FReply& LockMouseToWidget(TSharedRef<SWidget> InWidget);
	FReply& ReleaseMouseLock();
	FReply& ReleaseMouseCapture();
	FReply& DetectDrag(const TSharedRef<SWidget>& DetectDragInMe, FKey MouseButton);
	FReply& BeginDragDrop(TSharedRef<FDragDropOperation> InDragDropContent);
	FReply& EndDragDrop();
	FReply& PreventThrottling();
	bool ShouldReleaseMouse() const;
	bool ShouldSetUserFocus() const;
	bool ShouldReleaseMouseLock() const;
	bool ShouldThrottle() const;
	const TSharedPtr<SWidget>& GetMouseCaptor() const;
	static FReply Handled() { return FReply(true); }
	static FReply Unhandled() { return FReply(false); }
	FString ToString();
private:
	FReply(bool bIsHandled) : TReplyBase<FReply>(bIsHandled) {}
};

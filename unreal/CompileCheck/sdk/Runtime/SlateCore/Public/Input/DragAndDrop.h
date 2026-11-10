// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Input/DragAndDrop.h (UE 5.x source knowledge).
#pragma once

#include "CoreMinimal.h"
#include "Input/Events.h"

class FDragDropOperation : public TSharedFromThis<FDragDropOperation>
{
public:
	FDragDropOperation();
	virtual ~FDragDropOperation();
	virtual bool IsOfTypeImpl(const FString& Type) const { return false; }
	virtual void OnDrop(bool bDropWasHandled, const FPointerEvent& MouseEvent);
	virtual void OnDragged(const class FDragDropEvent& DragDropEvent);
	virtual TSharedPtr<SWidget> GetDefaultDecorator() const { return TSharedPtr<SWidget>(); }
	template <class TType> bool IsOfType() const UE_STUB_BODY
};
class FDragDropEvent : public FPointerEvent
{
public:
	FDragDropEvent(const FPointerEvent& InMouseEvent, const TSharedPtr<FDragDropOperation> InContent);
	TSharedPtr<FDragDropOperation> GetOperation() const;
	template <typename OperationType> TSharedPtr<OperationType> GetOperationAs() const UE_STUB_BODY
};

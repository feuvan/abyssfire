// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Input/DragAndDrop.h (UE 5.x source knowledge).
#pragma once

#include "CoreMinimal.h"
#include "Input/Events.h"

class FDragDropOperation : public TSharedFromThis<FDragDropOperation>
{
public:
	FDragDropOperation();
	virtual ~FDragDropOperation();
	/** Checks whether this drag and drop operation can cast safely to the specified type (DRAG_DROP_OPERATOR_TYPE). */
	virtual bool IsOfTypeImpl(const FString& Type) const { return false; }
	virtual void OnDrop(bool bDropWasHandled, const FPointerEvent& MouseEvent);
	virtual void OnDragged(const class FDragDropEvent& DragDropEvent);
	virtual TSharedPtr<SWidget> GetDefaultDecorator() const { return TSharedPtr<SWidget>(); }
	template <class TType> bool IsOfType() const { return IsOfTypeImpl(TType::GetTypeId()); }
};

/** All drag drop operations must put this in their class body: it gives them the GetTypeId() IsOfType needs. */
#define DRAG_DROP_OPERATOR_TYPE(TYPE, BASE) \
	static const FString& GetTypeId() { static FString Type = TEXT(#TYPE); return Type; } \
	virtual bool IsOfTypeImpl(const FString& Type) const override { return GetTypeId() == Type || BASE::IsOfTypeImpl(Type); }

class FDragDropEvent : public FPointerEvent
{
public:
	FDragDropEvent(const FPointerEvent& InMouseEvent, const TSharedPtr<FDragDropOperation> InContent);
	TSharedPtr<FDragDropOperation> GetOperation() const;
	/** The engine's body. */
	template <typename OperationType>
	TSharedPtr<OperationType> GetOperationAs() const
	{
		if (Content.IsValid() && Content->IsOfType<OperationType>())
		{
			return StaticCastSharedPtr<OperationType>(Content);
		}
		return TSharedPtr<OperationType>();
	}
private:
	TSharedPtr<FDragDropOperation> Content;
};

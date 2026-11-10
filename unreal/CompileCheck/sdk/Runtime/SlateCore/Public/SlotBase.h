// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/SlotBase.h (UE 5.x source knowledge).
// TSlotBase<SlotType>::FSlotArguments is what Panel::Slot() returns in declarative syntax: operator[] attaches the child
// widget, Expose() hands out the slot pointer, and the mixin setters (.HAlign, .Padding, ...) chain on it.
#pragma once

#include "CoreMinimal.h"
#include "Templates/UniquePtr.h"

class SWidget;
class FChildren;

class FSlotBase
{
public:
	FSlotBase();
	FSlotBase(const FChildren& InParent);
	FSlotBase(const TSharedRef<SWidget>& InWidget);
	FSlotBase& operator=(const FSlotBase&) = delete;
	FSlotBase(const FSlotBase&) = delete;
	virtual ~FSlotBase();
	struct FSlotArguments {};
	const FChildren* GetOwner() const;
	SWidget* GetOwnerWidget() const;
	void AttachWidget(TSharedRef<SWidget>&& InWidget);
	void AttachWidget(const TSharedRef<SWidget>& InWidget);
	const TSharedRef<SWidget>& GetWidget() const;
	const TSharedPtr<SWidget> DetachWidget();
};

template <typename SlotType>
class TSlotBase : public FSlotBase
{
public:
	using FSlotBase::FSlotBase;
	SlotType& operator[](TSharedRef<SWidget>&& InChildWidget) { return static_cast<SlotType&>(*this); }
	SlotType& operator[](const TSharedRef<SWidget>& InChildWidget) { return static_cast<SlotType&>(*this); }
	SlotType& Expose(SlotType*& OutVarToInit) { OutVarToInit = static_cast<SlotType*>(this); return static_cast<SlotType&>(*this); }

	struct FSlotArguments : public FSlotBase::FSlotArguments
	{
	public:
		enum EConstructSlotIsFChildren { ConstructSlotIsFChildren };
		FSlotArguments(EConstructSlotIsFChildren) {}
		FSlotArguments(TUniquePtr<SlotType> InSlot) : Slot(static_cast<TUniquePtr<SlotType>&&>(InSlot)) {}
		FSlotArguments(const FSlotArguments&) = delete;
		FSlotArguments& operator=(const FSlotArguments&) = delete;
		FSlotArguments(FSlotArguments&&) = default;
		FSlotArguments& operator=(FSlotArguments&&) = default;
		typename SlotType::FSlotArguments& operator[](TSharedRef<SWidget>&& InChildWidget) { return Me(); }
		typename SlotType::FSlotArguments& operator[](const TSharedRef<SWidget>& InChildWidget) { return Me(); }
		typename SlotType::FSlotArguments& Expose(SlotType*& OutVarToInit) { return Me(); }
		void AttachWidget(const TSharedRef<SWidget>& InChildWidget) {}
		const TSharedPtr<SWidget>& GetAttachedWidget() const UE_STUB_BODY
		SlotType* GetSlot() const { return Slot.Get(); }
		TUniquePtr<SlotType> StealSlot() { return static_cast<TUniquePtr<SlotType>&&>(Slot); }
		typename SlotType::FSlotArguments& Me() { return static_cast<typename SlotType::FSlotArguments&>(*this); }
	private:
		TUniquePtr<SlotType> Slot;
	};
	void Construct(const FChildren& SlotOwner, FSlotArguments&& InArgs) UE_STUB_BODY
};

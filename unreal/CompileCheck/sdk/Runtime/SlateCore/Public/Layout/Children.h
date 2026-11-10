// Stub SDK. Real headers: Engine/Source/Runtime/SlateCore/Public/Layout/Children.h and ChildrenBase.h (UE 5.x source
// knowledge). FChildren and TPanelChildren<SlotType>; FScopedWidgetSlotArguments is what Panel->AddSlot() returns (the
// slot is added when the temporary dies, so the chained setters and operator[] apply first).
#pragma once

#include "CoreMinimal.h"
#include "SlotBase.h"
#include "Layout/BasicLayoutWidgetSlot.h"

class SWidget;

class FChildren
{
public:
	FChildren(SWidget* InOwner);
	FChildren(SWidget* InOwner, FName InName);
	virtual ~FChildren() {}
	virtual int32 Num() const = 0;
	virtual TSharedRef<SWidget> GetChildAt(int32 Index) = 0;
	virtual TSharedRef<const SWidget> GetChildAt(int32 Index) const = 0;
	SWidget& GetOwner() const;
	/** Applies the predicate to every child: Pred(SWidget&), or Pred(const SWidget&) on const children. */
	template <typename Predicate> requires std::is_invocable_v<Predicate&, SWidget&>
	void ForEachWidget(Predicate Pred) UE_STUB_BODY
	template <typename Predicate> requires std::is_invocable_v<Predicate&, const SWidget&>
	void ForEachWidget(Predicate Pred) const UE_STUB_BODY
	FName GetName() const;
};

template <typename SlotType>
class TPanelChildren final : public FChildren
{
public:
	using FChildren::FChildren;
	virtual int32 Num() const override UE_STUB_BODY
	virtual TSharedRef<SWidget> GetChildAt(int32 Index) override UE_STUB_BODY
	virtual TSharedRef<const SWidget> GetChildAt(int32 Index) const override UE_STUB_BODY
	int32 AddSlot(typename SlotType::FSlotArguments&& SlotArgument) UE_STUB_BODY
	void AddSlots(TArray<typename SlotType::FSlotArguments> SlotArguments) UE_STUB_BODY
	void InsertSlot(typename SlotType::FSlotArguments&& SlotArgument, int32 Index) UE_STUB_BODY
	void Move(int32 IndexToMove, int32 IndexToDestination) UE_STUB_BODY
	void Reserve(int32 NumToReserve) UE_STUB_BODY
	bool IsValidIndex(int32 Index) const UE_STUB_BODY
	const SlotType& operator[](int32 Index) const UE_STUB_BODY
	SlotType& operator[](int32 Index) UE_STUB_BODY
	int32 Remove(const TSharedRef<SWidget>& SlotWidget) UE_STUB_BODY
	void RemoveAt(int32 Index) UE_STUB_BODY
	void Empty(int32 Slack = 0) UE_STUB_BODY
	/** Sorts the slots: bool Predicate(const SlotType&, const SlotType&). */
	template <class PREDICATE_CLASS> requires std::is_invocable_r_v<bool, const PREDICATE_CLASS&, const SlotType&, const SlotType&>
	void Sort(const PREDICATE_CLASS& Predicate) UE_STUB_BODY
	void Swap(int32 IndexA, int32 IndexB) UE_STUB_BODY

	struct FScopedWidgetSlotArguments final : public SlotType::FSlotArguments
	{
	public:
		FScopedWidgetSlotArguments(TUniquePtr<SlotType> InSlot, TPanelChildren<SlotType>& InChildren, int32 InIndex)
			: SlotType::FSlotArguments(static_cast<TUniquePtr<SlotType>&&>(InSlot)) {}
		FScopedWidgetSlotArguments(TUniquePtr<SlotType> InSlot, TPanelChildren<SlotType>& InChildren, int32 InIndex, TFunction<void(const SlotType*, int32)> OnAdded)
			: SlotType::FSlotArguments(static_cast<TUniquePtr<SlotType>&&>(InSlot)) {}
		FScopedWidgetSlotArguments(const FScopedWidgetSlotArguments&) = delete;
		FScopedWidgetSlotArguments& operator=(const FScopedWidgetSlotArguments&) = delete;
		FScopedWidgetSlotArguments(FScopedWidgetSlotArguments&&) = default;
		FScopedWidgetSlotArguments& operator=(FScopedWidgetSlotArguments&&) = default;
		virtual ~FScopedWidgetSlotArguments() {}
	};
};

template <typename ChildType>
class TSlotlessChildren final : public FChildren
{
public:
	using FChildren::FChildren;
	virtual int32 Num() const override UE_STUB_BODY
	virtual TSharedRef<SWidget> GetChildAt(int32 Index) override UE_STUB_BODY
	virtual TSharedRef<const SWidget> GetChildAt(int32 Index) const override UE_STUB_BODY
	int32 Add(const TSharedRef<ChildType>& Child) UE_STUB_BODY
	void Empty() UE_STUB_BODY
	int32 Remove(const TSharedRef<ChildType>& Child) UE_STUB_BODY
};

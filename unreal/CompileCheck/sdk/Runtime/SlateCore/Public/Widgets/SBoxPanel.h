// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Widgets/SBoxPanel.h (UE 5.x source knowledge).
// SHorizontalBox / SVerticalBox. Slot(): .AutoWidth() / .FillWidth(c) / .MaxWidth(w) (horizontal), .AutoHeight() /
// .FillHeight(c) / .MaxHeight(h) (vertical), plus .HAlign / .VAlign / .Padding and [ child ]. AddSlot() returns the
// same builder (the slot is added when the temporary dies).
#pragma once

#include "CoreMinimal.h"
#include "Layout/BasicLayoutWidgetSlot.h"
#include "Layout/Children.h"
#include "Types/SlateStructs.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SPanel.h"

class SBoxPanel : public SPanel
{
public:
	template <typename SlotType>
	class TSlot : public TBasicLayoutWidgetSlot<SlotType>
	{
	public:
		SLATE_SLOT_BEGIN_ARGS(TSlot, TBasicLayoutWidgetSlot<SlotType>)
			TOptional<FSizeParam> _SizeParam;
			TAttribute<float> _MinSize;
			TAttribute<float> _MaxSize;
		SLATE_SLOT_END_ARGS()
		float GetSizeValue() const;
		float GetMaxSize() const;
		float GetMinSize() const;
		void SetMaxSize(TAttribute<float> InMaxSize);
		void SetMinSize(TAttribute<float> InMinSize);
		void SetSizeToAuto();
		void SetSizeToStretch(TAttribute<float> StretchCoefficient);
		void Construct(const FChildren& SlotOwner, FSlotArguments&& InArgs);
	};
	class FSlot : public TSlot<FSlot> {};

	int32 RemoveSlot(const TSharedRef<SWidget>& SlotWidget);
	void ClearChildren();
	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override;
	virtual FChildren* GetChildren() override;
	virtual FVector2D ComputeDesiredSize(float) const override;
protected:
	SBoxPanel();
	SBoxPanel(EOrientation InOrientation);
};

class SHorizontalBox : public SBoxPanel
{
public:
	class FSlot : public SBoxPanel::TSlot<FSlot>
	{
	public:
		SLATE_SLOT_BEGIN_ARGS(FSlot, SBoxPanel::TSlot<FSlot>)
			FSlotArguments& AutoWidth() { return Me(); }
			FSlotArguments& FillWidth(TAttribute<float> InStretchCoefficient) { return Me(); }
			FSlotArguments& FillContentWidth(TAttribute<float> InStretchCoefficient, TAttribute<float> InShrinkStretchCoefficient = TAttribute<float>()) { return Me(); }
			FSlotArguments& MinWidth(TAttribute<float> InMinWidth) { return Me(); }
			FSlotArguments& MaxWidth(TAttribute<float> InMaxWidth) { return Me(); }
		SLATE_SLOT_END_ARGS()
		void SetAutoWidth();
		void SetFillWidth(TAttribute<float> InStretchCoefficient);
		void SetMaxWidth(TAttribute<float> InMaxWidth);
		void SetMinWidth(TAttribute<float> InMinWidth);
		void Construct(const FChildren& SlotOwner, FSlotArguments&& InArgs);
	};
	static FSlot::FSlotArguments Slot() { return FSlot::FSlotArguments(MakeUnique<FSlot>()); }

	SLATE_BEGIN_ARGS(SHorizontalBox)
	{
		_Visibility = EVisibility::SelfHitTestInvisible;
	}
		SLATE_SLOT_ARGUMENT(SHorizontalBox::FSlot, Slots)
	SLATE_END_ARGS()

	using FScopedWidgetSlotArguments = TPanelChildren<FSlot>::FScopedWidgetSlotArguments;
	FScopedWidgetSlotArguments AddSlot();
	FScopedWidgetSlotArguments InsertSlot(int32 Index = INDEX_NONE);
	FSlot& GetSlot(int32 SlotIndex);
	const FSlot& GetSlot(int32 SlotIndex) const;
	int32 NumSlots() const;
	bool IsValidSlotIndex(int32 Index) const;
	SHorizontalBox();
	void Construct(const FArguments& InArgs);
};

class SVerticalBox : public SBoxPanel
{
public:
	class FSlot : public SBoxPanel::TSlot<FSlot>
	{
	public:
		SLATE_SLOT_BEGIN_ARGS(FSlot, SBoxPanel::TSlot<FSlot>)
			FSlotArguments& AutoHeight() { return Me(); }
			FSlotArguments& FillHeight(TAttribute<float> InStretchCoefficient) { return Me(); }
			FSlotArguments& FillContentHeight(TAttribute<float> InStretchCoefficient, TAttribute<float> InShrinkStretchCoefficient = TAttribute<float>()) { return Me(); }
			FSlotArguments& MinHeight(TAttribute<float> InMinHeight) { return Me(); }
			FSlotArguments& MaxHeight(TAttribute<float> InMaxHeight) { return Me(); }
		SLATE_SLOT_END_ARGS()
		void SetAutoHeight();
		void SetFillHeight(TAttribute<float> InStretchCoefficient);
		void SetMaxHeight(TAttribute<float> InMaxHeight);
		void SetMinHeight(TAttribute<float> InMinHeight);
		void Construct(const FChildren& SlotOwner, FSlotArguments&& InArgs);
	};
	static FSlot::FSlotArguments Slot() { return FSlot::FSlotArguments(MakeUnique<FSlot>()); }

	SLATE_BEGIN_ARGS(SVerticalBox)
	{
		_Visibility = EVisibility::SelfHitTestInvisible;
	}
		SLATE_SLOT_ARGUMENT(SVerticalBox::FSlot, Slots)
	SLATE_END_ARGS()

	using FScopedWidgetSlotArguments = TPanelChildren<FSlot>::FScopedWidgetSlotArguments;
	FScopedWidgetSlotArguments AddSlot();
	FScopedWidgetSlotArguments InsertSlot(int32 Index = INDEX_NONE);
	FSlot& GetSlot(int32 SlotIndex);
	const FSlot& GetSlot(int32 SlotIndex) const;
	int32 NumSlots() const;
	bool IsValidSlotIndex(int32 Index) const;
	SVerticalBox();
	void Construct(const FArguments& InArgs);
};

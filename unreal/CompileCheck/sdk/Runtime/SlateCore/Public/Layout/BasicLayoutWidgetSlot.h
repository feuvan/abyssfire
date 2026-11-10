// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Layout/BasicLayoutWidgetSlot.h (UE 5.x source
// knowledge). The slot mixins that give panel slots HAlign / VAlign / Padding, on the slot (Set*, Get*) and on its
// declarative FSlotArguments.
#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"
#include "Misc/Attribute.h"
#include "Layout/WidgetSlotWithAttributeSupport.h"
#include "SlotBase.h"
#include "Types/SlateEnums.h"

template <typename MixedIntoType>
class TAlignmentWidgetSlotMixin
{
public:
	TAlignmentWidgetSlotMixin() UE_STUB_BODY
	TAlignmentWidgetSlotMixin(const EHorizontalAlignment InHAlign, const EVerticalAlignment InVAlign) UE_STUB_BODY
	struct FSlotArgumentsMixin
	{
		typename MixedIntoType::FSlotArguments& HAlign(EHorizontalAlignment InHAlignment) { return static_cast<typename MixedIntoType::FSlotArguments&>(*this); }
		typename MixedIntoType::FSlotArguments& VAlign(EVerticalAlignment InVAlignment) { return static_cast<typename MixedIntoType::FSlotArguments&>(*this); }
	};
	MixedIntoType& HAlign(EHorizontalAlignment InHAlignment) { return *static_cast<MixedIntoType*>(this); }
	MixedIntoType& VAlign(EVerticalAlignment InVAlignment) { return *static_cast<MixedIntoType*>(this); }
	void SetHorizontalAlignment(EHorizontalAlignment Alignment) UE_STUB_BODY
	EHorizontalAlignment GetHorizontalAlignment() const UE_STUB_BODY
	void SetVerticalAlignment(EVerticalAlignment Alignment) UE_STUB_BODY
	EVerticalAlignment GetVerticalAlignment() const UE_STUB_BODY
};

template <typename MixedIntoType>
class TPaddingWidgetSlotMixin
{
public:
	TPaddingWidgetSlotMixin() UE_STUB_BODY
	TPaddingWidgetSlotMixin(const FMargin& Margin) UE_STUB_BODY
	struct FSlotArgumentsMixin
	{
		typename MixedIntoType::FSlotArguments& Padding(TAttribute<FMargin> InPadding) { return static_cast<typename MixedIntoType::FSlotArguments&>(*this); }
		typename MixedIntoType::FSlotArguments& Padding(float Uniform) { return static_cast<typename MixedIntoType::FSlotArguments&>(*this); }
		typename MixedIntoType::FSlotArguments& Padding(float Horizontal, float Vertical) { return static_cast<typename MixedIntoType::FSlotArguments&>(*this); }
		typename MixedIntoType::FSlotArguments& Padding(float Left, float Top, float Right, float Bottom) { return static_cast<typename MixedIntoType::FSlotArguments&>(*this); }
	};
	MixedIntoType& Padding(TAttribute<FMargin> InPadding) { return *static_cast<MixedIntoType*>(this); }
	MixedIntoType& Padding(float Uniform) { return *static_cast<MixedIntoType*>(this); }
	MixedIntoType& Padding(float Horizontal, float Vertical) { return *static_cast<MixedIntoType*>(this); }
	MixedIntoType& Padding(float Left, float Top, float Right, float Bottom) { return *static_cast<MixedIntoType*>(this); }
	void SetPadding(TAttribute<FMargin> InPadding) UE_STUB_BODY
	FMargin GetPadding() const UE_STUB_BODY
};

/** A slot with alignment and padding (SBoxPanel, SOverlay, SScrollBox, ... slots derive from it). */
template <typename SlotType>
class TBasicLayoutWidgetSlot : public TWidgetSlotWithAttributeSupport<SlotType>, public TPaddingWidgetSlotMixin<SlotType>, public TAlignmentWidgetSlotMixin<SlotType>
{
public:
	TBasicLayoutWidgetSlot() = default;
	TBasicLayoutWidgetSlot(const EHorizontalAlignment InHAlign, const EVerticalAlignment InVAlign) UE_STUB_BODY
	using TSlotBase<SlotType>::operator[];
	struct FSlotArguments : public TSlotBase<SlotType>::FSlotArguments, public TPaddingWidgetSlotMixin<SlotType>::FSlotArgumentsMixin, public TAlignmentWidgetSlotMixin<SlotType>::FSlotArgumentsMixin
	{
		using TSlotBase<SlotType>::FSlotArguments::FSlotArguments;
	};
	void Construct(const FChildren& SlotOwner, FSlotArguments&& InArgs) UE_STUB_BODY
};

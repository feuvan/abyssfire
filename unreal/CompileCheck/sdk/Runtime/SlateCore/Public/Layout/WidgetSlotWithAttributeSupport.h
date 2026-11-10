// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Layout/WidgetSlotWithAttributeSupport.h (UE 5.x source
// knowledge). The slot base of the panels whose slots hold Slate attributes (TBasicLayoutWidgetSlot, SCanvas::FSlot):
// a TSlotBase. The engine's second base, SlateAttributePrivate::ISlateAttributeContainer, and the TSlateSlotAttribute
// member template are not modelled: the module does not write slot classes of its own.
#pragma once

#include "CoreMinimal.h"
#include "SlotBase.h"

class FChildren;

/** A base slot that supports TSlateContainedAttribute. */
template <typename SlotType>
class TWidgetSlotWithAttributeSupport : public TSlotBase<SlotType>
{
protected:
	using TSlotBase<SlotType>::TSlotBase;
public:
	void Construct(const FChildren& SlotOwner, typename TSlotBase<SlotType>::FSlotArguments&& InArgs) UE_STUB_BODY
	void RequestSortAttribute() UE_STUB_BODY
};

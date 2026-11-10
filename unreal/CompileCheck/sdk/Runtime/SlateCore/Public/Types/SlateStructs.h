// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Types/SlateStructs.h (UE 5.x source knowledge).
// FOptionalSize (unset or a float; implicit from float), FSizeParam / FStretch / FAuto.
#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"

struct FOptionalSize
{
	FOptionalSize() : Size(-1.0f) {}
	FOptionalSize(const float SpecifiedSize) : Size(SpecifiedSize) {}
	bool IsSet() const { return Size != -1.0f; }
	float Get() const { return Size; }
	bool operator==(const FOptionalSize& Other) const { return Size == Other.Size; }
private:
	float Size;
};
struct FSizeParam
{
	enum ESizeRule { SizeRule_Auto, SizeRule_Stretch, SizeRule_StretchContent };
	ESizeRule SizeRule;
	TAttribute<float> Value;
	TAttribute<float> ShrinkValue;
protected:
	FSizeParam(ESizeRule InTypeOfSize, const TAttribute<float>& InValue, const TAttribute<float>& InShrinkValue);
};
struct FStretch : public FSizeParam
{
	FStretch(const TAttribute<float>& StretchPart);
	FStretch();
};
struct FAuto : public FSizeParam
{
	FAuto();
};

// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Fonts/FontCache.h (UE 5.x source knowledge; the enum
// is reflected: checked against the 5.6 stub). Only the text shaping method; the font cache itself is not modelled.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

UENUM(BlueprintType)
enum class ETextShapingMethod : uint8
{
	Auto = 0,
	KerningOnly,
	FullShaping,
};

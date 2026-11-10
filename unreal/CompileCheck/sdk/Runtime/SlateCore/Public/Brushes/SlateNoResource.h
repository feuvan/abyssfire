// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Brushes/SlateNoResource.h (UE 5.x source knowledge).
#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"

struct FSlateNoResource : public FSlateBrush
{
	FSlateNoResource(const UE::Slate::FDeprecateVector2DParameter& InImageSize = FVector2f::ZeroVector);
};

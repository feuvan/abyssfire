// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Brushes/SlateColorBrush.h (UE 5.x source knowledge).
#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"

struct FSlateColorBrush : public FSlateBrush
{
	FSlateColorBrush(const FLinearColor& InColor);
	FSlateColorBrush(const FColor& InColor);
	FSlateColorBrush(const TSharedRef<FLinearColor>& InColor);
	FSlateColorBrush(const FSlateColor& InColor);
};

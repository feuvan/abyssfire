// Stub SDK. Real headers: Engine/Source/Runtime/SlateCore/Public/Brushes/SlateBoxBrush.h and SlateBorderBrush.h (UE 5.x
// source knowledge).
#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"

struct FSlateBoxBrush : public FSlateBrush
{
	FSlateBoxBrush(const FName InImageName, const FMargin& InMargin, const FSlateColor& InColorAndOpacity = FSlateColor(FLinearColor(1, 1, 1, 1)), ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor);
	FSlateBoxBrush(const FString& InImageName, const FMargin& InMargin, const FSlateColor& InColorAndOpacity = FSlateColor(FLinearColor(1, 1, 1, 1)), ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor);
	FSlateBoxBrush(const TCHAR* InImageName, const FMargin& InMargin, const FSlateColor& InColorAndOpacity = FSlateColor(FLinearColor(1, 1, 1, 1)), ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor);
	FSlateBoxBrush(const FName InImageName, const UE::Slate::FDeprecateVector2DParameter& ImageSize, const FMargin& InMargin, const FSlateColor& InColorAndOpacity = FSlateColor(FLinearColor(1, 1, 1, 1)), ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor);
	FSlateBoxBrush(UObject* InResourceObject, const FMargin& InMargin, const FSlateColor& InColorAndOpacity = FSlateColor(FLinearColor(1, 1, 1, 1)), ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor);
	FSlateBoxBrush(UObject* InResourceObject, const UE::Slate::FDeprecateVector2DParameter& ImageSize, const FMargin& InMargin, const FSlateColor& InColorAndOpacity = FSlateColor(FLinearColor(1, 1, 1, 1)), ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor);
};
struct FSlateBorderBrush : public FSlateBrush
{
	FSlateBorderBrush(const FName InImageName, const FMargin& InMargin, const FSlateColor& InColorAndOpacity = FSlateColor(FLinearColor(1, 1, 1, 1)), ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor);
	FSlateBorderBrush(const TCHAR* InImageName, const FMargin& InMargin, const FSlateColor& InColorAndOpacity = FSlateColor(FLinearColor(1, 1, 1, 1)), ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor);
	FSlateBorderBrush(UObject* InResourceObject, const FMargin& InMargin, const FSlateColor& InColorAndOpacity = FSlateColor(FLinearColor(1, 1, 1, 1)), ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor);
};

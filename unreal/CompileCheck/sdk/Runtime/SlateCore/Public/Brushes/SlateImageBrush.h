// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Brushes/SlateImageBrush.h (UE 5.x source knowledge).
#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"

struct FSlateImageBrush : public FSlateBrush
{
	FSlateImageBrush(const FName InImageName, const UE::Slate::FDeprecateVector2DParameter& InImageSize, const FLinearColor& InTint = FLinearColor(1, 1, 1, 1), ESlateBrushTileType::Type InTiling = ESlateBrushTileType::NoTile, ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor);
	FSlateImageBrush(const FString& InImageName, const UE::Slate::FDeprecateVector2DParameter& InImageSize, const FLinearColor& InTint = FLinearColor(1, 1, 1, 1), ESlateBrushTileType::Type InTiling = ESlateBrushTileType::NoTile, ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor);
	FSlateImageBrush(const ANSICHAR* InImageName, const UE::Slate::FDeprecateVector2DParameter& InImageSize, const FLinearColor& InTint = FLinearColor(1, 1, 1, 1), ESlateBrushTileType::Type InTiling = ESlateBrushTileType::NoTile, ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor);
	FSlateImageBrush(const WIDECHAR* InImageName, const UE::Slate::FDeprecateVector2DParameter& InImageSize, const FLinearColor& InTint = FLinearColor(1, 1, 1, 1), ESlateBrushTileType::Type InTiling = ESlateBrushTileType::NoTile, ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor);
	FSlateImageBrush(const FName InImageName, const UE::Slate::FDeprecateVector2DParameter& InImageSize, const TSharedRef<FLinearColor>& InTint, ESlateBrushTileType::Type InTiling = ESlateBrushTileType::NoTile, ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor);
	FSlateImageBrush(const FName InImageName, const UE::Slate::FDeprecateVector2DParameter& InImageSize, const FSlateColor& InTint, ESlateBrushTileType::Type InTiling = ESlateBrushTileType::NoTile, ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor);
	FSlateImageBrush(UObject* InResourceObject, const UE::Slate::FDeprecateVector2DParameter& InImageSize, const FSlateColor& InTint = FSlateColor(FLinearColor(1, 1, 1, 1)), ESlateBrushTileType::Type InTiling = ESlateBrushTileType::NoTile, ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor);
};
struct FSlateVectorImageBrush : public FSlateBrush
{
	FSlateVectorImageBrush(const FString& InImageName, const UE::Slate::FDeprecateVector2DParameter& InImageSize, const FLinearColor& InTint = FLinearColor(1, 1, 1, 1), ESlateBrushTileType::Type InTiling = ESlateBrushTileType::NoTile);
};

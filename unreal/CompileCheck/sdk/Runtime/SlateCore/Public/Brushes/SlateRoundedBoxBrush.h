// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Brushes/SlateRoundedBoxBrush.h (UE 5.x source knowledge).
// Constructor templates over the colour kinds FSlateColor accepts (FLinearColor, FColor, FSlateColor, shared colour).
#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"

struct FSlateRoundedBoxBrush : public FSlateBrush
{
	template <typename FillColorType> requires std::is_constructible_v<FSlateColor, const FillColorType&>
	FSlateRoundedBoxBrush(const FillColorType& InColor, const UE::Slate::FDeprecateVector2DParameter& InImageSize = FVector2f::ZeroVector) UE_STUB_BODY
	template <typename FillColorType> requires std::is_constructible_v<FSlateColor, const FillColorType&>
	FSlateRoundedBoxBrush(const FillColorType& InColor, float InRadius, const UE::Slate::FDeprecateVector2DParameter& InImageSize = FVector2f::ZeroVector) UE_STUB_BODY
	template <typename FillColorType> requires std::is_constructible_v<FSlateColor, const FillColorType&>
	FSlateRoundedBoxBrush(const FillColorType& InColor, FVector4 InRadius, const UE::Slate::FDeprecateVector2DParameter& InImageSize = FVector2f::ZeroVector) UE_STUB_BODY
	template <typename FillColorType, typename OutlineColorType> requires std::is_constructible_v<FSlateColor, const FillColorType&> && std::is_constructible_v<FSlateColor, const OutlineColorType&>
	FSlateRoundedBoxBrush(const FillColorType& InColor, const OutlineColorType& InOutlineColor, float InOutlineWidth, const UE::Slate::FDeprecateVector2DParameter& InImageSize = FVector2f::ZeroVector) UE_STUB_BODY
	template <typename FillColorType, typename OutlineColorType> requires std::is_constructible_v<FSlateColor, const FillColorType&> && std::is_constructible_v<FSlateColor, const OutlineColorType&>
	FSlateRoundedBoxBrush(const FillColorType& InColor, float InRadius, const OutlineColorType& InOutlineColor, float InOutlineWidth, const UE::Slate::FDeprecateVector2DParameter& InImageSize = FVector2f::ZeroVector) UE_STUB_BODY
	template <typename FillColorType, typename OutlineColorType> requires std::is_constructible_v<FSlateColor, const FillColorType&> && std::is_constructible_v<FSlateColor, const OutlineColorType&>
	FSlateRoundedBoxBrush(const FillColorType& InColor, FVector4 InRadius, const OutlineColorType& InOutlineColor, float InOutlineWidth, const UE::Slate::FDeprecateVector2DParameter& InImageSize = FVector2f::ZeroVector) UE_STUB_BODY
};

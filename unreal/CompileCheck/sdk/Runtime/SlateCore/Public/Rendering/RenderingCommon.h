// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Rendering/RenderingCommon.h (UE 5.x source knowledge).
// ESlateDrawEffect, FSlateVertex (custom vertex draw), SlateIndex, FSlateGradientStop.
#pragma once

#include "CoreMinimal.h"
#include "Layout/SlateRect.h"
#include "Rendering/SlateRenderTransform.h"
#include "Types/SlateVector2.h"

enum class ESlateDrawEffect : uint8
{
	None = 0,
	NoBlending = 1 << 0,
	PreMultipliedAlpha = 1 << 1,
	NoGamma = 1 << 2,
	InvertAlpha = 1 << 3,
	NoPixelSnapping = 1 << 4,
	DisabledEffect = 1 << 5,
	IgnoreTextureAlpha = 1 << 6,
	ReverseGamma = 1 << 7,
};
ENUM_CLASS_FLAGS(ESlateDrawEffect);
enum class ESlateVertexRounding : uint8 { Disabled, Enabled };
enum class ESlateBatchDrawFlag : uint16 { None = 0, NoBlending = 0x01, PreMultipliedAlpha = 0x02, NoGamma = 0x04, Wireframe = 0x08, TileU = 0x10, TileV = 0x20, HDR = 0x40, InvertAlpha = 0x80 };
ENUM_CLASS_FLAGS(ESlateBatchDrawFlag);

typedef uint32 SlateIndex;

struct FSlateVertex
{
	float TexCoords[4];
	FVector2f MaterialTexCoords;
	FVector2f Position;
	FColor Color;
	FColor SecondaryColor;
	FSlateVertex();
	template <ESlateVertexRounding Rounding>
	static FSlateVertex Make(const FSlateRenderTransform& RenderTransform, const FVector2f InLocalPosition, const FVector2f InTexCoord, const FVector2f InTexCoord2, const FColor InColor, const FColor SecondaryColor = FColor()) UE_STUB_BODY
	template <ESlateVertexRounding Rounding>
	static FSlateVertex Make(const FSlateRenderTransform& RenderTransform, const FVector2f InLocalPosition, const FVector2f InTexCoord, const FColor& InColor, const FColor SecondaryColor = FColor()) UE_STUB_BODY
	template <ESlateVertexRounding Rounding>
	static FSlateVertex Make(const FSlateRenderTransform& RenderTransform, const FVector2f InLocalPosition, const FVector4f InTexCoords, const FVector2f InMaterialTexCoords, const FColor InColor, const FColor SecondaryColor = FColor()) UE_STUB_BODY
	void SetTexCoords(const FVector4f InTexCoords);
	void SetPosition(const FVector2f InPosition);
};

struct FSlateGradientStop
{
	FVector2f Position;
	FLinearColor Color;
	FSlateGradientStop(const FVector2D& InPosition, const FLinearColor& InColor);
	FSlateGradientStop(const FVector2f& InPosition, const FLinearColor& InColor);
};
class ISlateUpdatableInstanceBuffer;
class ISlateViewport;

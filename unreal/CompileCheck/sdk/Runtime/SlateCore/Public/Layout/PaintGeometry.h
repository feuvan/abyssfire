// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Layout/PaintGeometry.h (UE 5.x source knowledge).
#pragma once

#include "CoreMinimal.h"
#include "Rendering/SlateLayoutTransform.h"
#include "Rendering/SlateRenderTransform.h"
#include "Types/SlateVector2.h"

struct FPaintGeometry
{
	UE::Slate::FDeprecateVector2DResult DrawPosition;
	float DrawScale;
	UE::Slate::FDeprecateVector2DResult GetLocalSize() const;
	const FSlateRenderTransform& GetAccumulatedRenderTransform() const;
	void AppendTransform(const FSlateLayoutTransform& LayoutTransform);
	void SetRenderTransform(const FSlateRenderTransform& InAccumulatedRenderTransform);
	bool HasRenderTransform() const;
	FPaintGeometry();
	FPaintGeometry(const FSlateLayoutTransform& InAccumulatedLayoutTransform, const FSlateRenderTransform& InAccumulatedRenderTransform, const UE::Slate::FDeprecateVector2DParameter& InLocalSize, bool bInHasRenderTransform);
};

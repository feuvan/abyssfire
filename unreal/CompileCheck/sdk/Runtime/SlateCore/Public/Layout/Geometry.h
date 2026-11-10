// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Layout/Geometry.h (UE 5.x source knowledge; reflected
// struct "Geometry": checked against the 5.6 stub). Sizes and positions are FVector2f-based since 5.2: the getters
// return UE::Slate::FDeprecateVector2DResult (converts to FVector2D), parameters take FDeprecateVector2DParameter.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Layout/PaintGeometry.h"
#include "Layout/SlateRect.h"
#include "Layout/Margin.h"
#include "Rendering/SlateLayoutTransform.h"
#include "Rendering/SlateRenderTransform.h"
#include "Types/SlateVector2.h"

class FArrangedWidget;
class SWidget;

struct FGeometry
{
	UE_STUB_STRUCT_BODY(FGeometry)
	FGeometry();
	FGeometry(const FGeometry& RHS) = default;
	FGeometry& operator=(const FGeometry& RHS);
	bool operator==(const FGeometry& Other) const;
	bool operator!=(const FGeometry& Other) const;
	static FGeometry MakeRoot(const UE::Slate::FDeprecateVector2DParameter& InLocalSize, const FSlateLayoutTransform& LayoutTransform);
	FGeometry MakeChild(const UE::Slate::FDeprecateVector2DParameter& InLocalSize, const FSlateLayoutTransform& LayoutTransform, const FSlateRenderTransform& RenderTransform, const UE::Slate::FDeprecateVector2DParameter& RenderTransformPivot) const;
	FGeometry MakeChild(const UE::Slate::FDeprecateVector2DParameter& InLocalSize, const FSlateLayoutTransform& LayoutTransform) const;
	FGeometry MakeChild(const FSlateRenderTransform& RenderTransform, const UE::Slate::FDeprecateVector2DParameter& RenderTransformPivot = FVector2f(0.5f, 0.5f)) const;
	FArrangedWidget MakeChild(const TSharedRef<SWidget>& ChildWidget, const UE::Slate::FDeprecateVector2DParameter& InLocalSize, const FSlateLayoutTransform& LayoutTransform) const;
	FPaintGeometry ToPaintGeometry() const;
	FPaintGeometry ToPaintGeometry(const UE::Slate::FDeprecateVector2DParameter& InLocalSize, const FSlateLayoutTransform& InLayoutTransform, const FSlateRenderTransform& RenderTransform, const UE::Slate::FDeprecateVector2DParameter& RenderTransformPivot = FVector2f(0.5f, 0.5f)) const;
	FPaintGeometry ToPaintGeometry(const UE::Slate::FDeprecateVector2DParameter& InLocalSize, const FSlateLayoutTransform& InLayoutTransform) const;
	FPaintGeometry ToPaintGeometry(const FSlateRenderTransform& RenderTransform, const UE::Slate::FDeprecateVector2DParameter& RenderTransformPivot = FVector2f(0.5f, 0.5f)) const;
	FPaintGeometry ToPaintGeometry(const FSlateLayoutTransform& LayoutTransform) const;
	FPaintGeometry ToOffsetPaintGeometry(const UE::Slate::FDeprecateVector2DParameter& LocalOffset) const;
	FPaintGeometry ToInflatedPaintGeometry(const UE::Slate::FDeprecateVector2DParameter& InflateAmount) const;
	bool IsUnderLocation(const UE::Slate::FDeprecateVector2DParameter& AbsoluteCoordinate) const;
	UE::Slate::FDeprecateVector2DResult AbsoluteToLocal(UE::Slate::FDeprecateVector2DParameter AbsoluteCoordinate) const;
	UE::Slate::FDeprecateVector2DResult LocalToAbsolute(UE::Slate::FDeprecateVector2DParameter LocalCoordinate) const;
	UE::Slate::FDeprecateVector2DResult LocalToRoundedLocal(UE::Slate::FDeprecateVector2DParameter LocalCoordinate) const;
	FSlateRect GetLayoutBoundingRect() const;
	FSlateRect GetLayoutBoundingRect(const FMargin& LocalSpaceExtendBy) const;
	FSlateRect GetLayoutBoundingRect(const FSlateRect& LocalSpaceRect) const;
	FSlateRect GetRenderBoundingRect() const;
	FSlateRect GetRenderBoundingRect(const FMargin& LocalSpaceExtendBy) const;
	FSlateRect GetRenderBoundingRect(const FSlateRect& LocalSpaceRect) const;
	FString ToString() const;
	UE::Slate::FDeprecateVector2DResult GetDrawSize() const;
	UE::Slate::FDeprecateVector2DResult GetLocalSize() const;
	const FSlateRenderTransform& GetAccumulatedRenderTransform() const;
	FSlateLayoutTransform GetAccumulatedLayoutTransform() const;
	void AppendTransform(const FSlateLayoutTransform& LayoutTransform);
	UE::Slate::FDeprecateVector2DResult GetAbsolutePosition() const;
	UE::Slate::FDeprecateVector2DResult GetAbsoluteSize() const;
	UE::Slate::FDeprecateVector2DResult GetAbsolutePositionAtCoordinates(const UE::Slate::FDeprecateVector2DParameter& NormalCoordinates) const;
	UE::Slate::FDeprecateVector2DResult GetLocalPositionAtCoordinates(const UE::Slate::FDeprecateVector2DParameter& NormalCoordinates) const;
	bool HasRenderTransform() const;
	const UE::Slate::FDeprecateVector2DResult Size;
	const float Scale;
	const UE::Slate::FDeprecateVector2DResult AbsolutePosition;
	const UE::Slate::FDeprecateVector2DResult Position;
};

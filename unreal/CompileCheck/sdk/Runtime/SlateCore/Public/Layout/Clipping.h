// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Layout/Clipping.h (UE 5.x source knowledge; reflected
// enum: checked against the 5.6 stub).
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Types/SlateVector2.h"

UENUM(BlueprintType)
enum class EWidgetClipping : uint8
{
	Inherit,
	ClipToBounds,
	ClipToBoundsWithoutIntersecting UMETA(DisplayName = "Clip To Bounds - Without Intersecting (Advanced)"),
	ClipToBoundsAlways UMETA(DisplayName = "Clip To Bounds - Always (Advanced)"),
	OnDemand UMETA(DisplayName = "On Demand (Advanced)"),
};
class FSlateClippingZone
{
public:
	FSlateClippingZone();
	explicit FSlateClippingZone(const struct FSlateRect& AxisAlignedRect);
	explicit FSlateClippingZone(const struct FGeometry& BoundingGeometry);
	explicit FSlateClippingZone(const struct FPaintGeometry& PaintingGeometry);
	FSlateClippingZone(const UE::Slate::FDeprecateVector2DParameter& InTopLeft, const UE::Slate::FDeprecateVector2DParameter& InTopRight,
		const UE::Slate::FDeprecateVector2DParameter& InBottomLeft, const UE::Slate::FDeprecateVector2DParameter& InBottomRight);
	bool GetShouldIntersectParent() const;
	void SetShouldIntersectParent(bool bValue);
	bool IsAxisAligned() const;
	bool HasZeroArea() const;
	bool IsPointInside(const UE::Slate::FDeprecateVector2DParameter& Point) const;
	FSlateRect GetBoundingBox() const;
};

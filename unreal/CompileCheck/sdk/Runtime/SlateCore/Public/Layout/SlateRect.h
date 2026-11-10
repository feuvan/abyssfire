// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Layout/SlateRect.h (UE 5.x source knowledge).
#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"
#include "Types/SlateVector2.h"

struct FSlateRect
{
public:
	float Left;
	float Top;
	float Right;
	float Bottom;
	explicit FSlateRect(float InLeft = -1, float InTop = -1, float InRight = -1, float InBottom = -1) : Left(InLeft), Top(InTop), Right(InRight), Bottom(InBottom) {}
	FSlateRect(const UE::Slate::FDeprecateVector2DParameter& InStartPos, const UE::Slate::FDeprecateVector2DParameter& InEndPos)
		: Left(InStartPos.X), Top(InStartPos.Y), Right(InEndPos.X), Bottom(InEndPos.Y) {}
	static FSlateRect FromPointAndExtent(const UE::Slate::FDeprecateVector2DParameter& TopLeft, const UE::Slate::FDeprecateVector2DParameter& Size);
	bool IsValid() const;
	bool IsEmpty() const;
	UE::Slate::FDeprecateVector2DResult GetSize() const;
	float GetArea() const;
	UE::Slate::FDeprecateVector2DResult GetCenter() const;
	UE::Slate::FDeprecateVector2DResult GetTopLeft() const;
	UE::Slate::FDeprecateVector2DResult GetTopRight() const;
	UE::Slate::FDeprecateVector2DResult GetBottomRight() const;
	UE::Slate::FDeprecateVector2DResult GetBottomLeft() const;
	FSlateRect InsetBy(const struct FMargin& InsetAmount) const;
	FSlateRect ExtendBy(const FMargin& ExtendAmount) const;
	FSlateRect OffsetBy(const UE::Slate::FDeprecateVector2DParameter& OffsetAmount) const;
	FSlateRect ScaleBy(float ScaleBy) const;
	FSlateRect Expand(const FSlateRect& Other) const;
	FSlateRect Round() const;
	FSlateRect IntersectionWith(const FSlateRect& Other) const;
	FSlateRect IntersectionWith(const FSlateRect& Other, bool& OutOverlapping) const;
	bool ContainsPoint(const UE::Slate::FDeprecateVector2DParameter& Point) const;
	bool operator==(const FSlateRect& Other) const;
	bool operator!=(const FSlateRect& Other) const;
	static bool DoRectanglesIntersect(const FSlateRect& A, const FSlateRect& B);
	static bool IsRectangleContained(const FSlateRect& A, const FSlateRect& B);
	FString ToString() const;
};

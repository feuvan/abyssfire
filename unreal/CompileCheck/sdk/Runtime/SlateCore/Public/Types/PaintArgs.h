// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Types/PaintArgs.h (UE 5.x source knowledge).
#pragma once

#include "CoreMinimal.h"
#include "Types/SlateVector2.h"

class SWidget;
class FHittestGrid;

class FPaintArgs
{
public:
	FPaintArgs(const SWidget* PaintParent, FHittestGrid& InRootHittestGrid, UE::Slate::FDeprecateVector2DParameter InWindowOffset, double InCurrentTime, float InDeltaTime);
	FPaintArgs WithNewParent(const SWidget* PaintParent) const;
	FPaintArgs WithNewHitTestGrid(FHittestGrid& NewHitTestGrid) const;
	FHittestGrid& GetHittestGrid() const;
	UE::Slate::FDeprecateVector2DResult GetWindowToDesktopTransform() const;
	double GetCurrentTime() const;
	float GetDeltaTime() const;
	const SWidget* GetPaintParent() const;
};

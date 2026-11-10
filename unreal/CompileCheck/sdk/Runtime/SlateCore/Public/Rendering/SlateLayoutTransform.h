// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Rendering/SlateLayoutTransform.h (UE 5.x source
// knowledge). Uniform scale + translation; all constructors are explicit.
#pragma once

#include "CoreMinimal.h"
#include "Math/TransformCalculus2D.h"
#include "Types/SlateVector2.h"

class FSlateLayoutTransform
{
public:
	explicit FSlateLayoutTransform(float InScale = 1.0f);
	explicit FSlateLayoutTransform(float InScale, const UE::Slate::FDeprecateVector2DParameter& InTranslation);
	explicit FSlateLayoutTransform(const UE::Slate::FDeprecateVector2DParameter& InTranslation);
	UE::Slate::FDeprecateVector2DResult GetTranslation() const;
	float GetScale() const;
	FMatrix ToMatrix() const;
	// TransformPoint / TransformVector are not declared: the module does not call them on a layout transform and their
	// exact 5.x parameter types (FVector2D, FVector2f or the deprecation wrappers) are not known here.
	FSlateLayoutTransform Concatenate(const FSlateLayoutTransform& RHS) const;
	FSlateLayoutTransform Inverse() const;
	bool operator==(const FSlateLayoutTransform& Other) const;
	bool operator!=(const FSlateLayoutTransform& Other) const;
};

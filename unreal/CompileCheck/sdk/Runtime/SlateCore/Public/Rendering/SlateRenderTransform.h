// Stub SDK. Real header: Engine/Source/Runtime/SlateCore/Public/Rendering/SlateRenderTransform.h (UE 5.x source
// knowledge). FSlateRenderTransform is the 2D float transform (scale / shear / rotation + translation).
#pragma once

#include "CoreMinimal.h"
#include "Math/TransformCalculus2D.h"
#include "Rendering/SlateLayoutTransform.h"

typedef FTransform2f FSlateRenderTransform;

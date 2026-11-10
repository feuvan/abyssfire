// Stub SDK. Real header: Engine/Source/Runtime/Engine/Public/SceneView.h (UE 5.x source knowledge).
// FSceneViewProjectionData (what ULocalPlayer::GetProjectionData fills) and the static projection helpers of
// FSceneView; the view itself is not modelled.
#pragma once

#include "CoreMinimal.h"
#include "Math/IntRect.h"
#include "Math/Matrix.h"
#include "Math/TranslationMatrix.h"

/** Projection data for a FSceneView. */
struct FSceneViewProjectionData
{
	/** The view origin. */
	FVector ViewOrigin;
	/** Rotation matrix transforming from world space to view space. */
	FMatrix ViewRotationMatrix;
	/** UE projection matrix projects such that clip space Z=1 is the near plane, and Z=0 is the infinite far plane. */
	FMatrix ProjectionMatrix;
protected:
	/** The unconstrained (no aspect ratio bars applied) view rectangle (also unscaled). */
	FIntRect ViewRect;
	/** The constrained view rectangle (identical to ViewRect if aspect ratio is not constrained). */
	FIntRect ConstrainedViewRect;
public:
	const FIntRect& GetViewRect() const { return ViewRect; }
	const FIntRect& GetConstrainedViewRect() const { return ConstrainedViewRect; }
	FMatrix ComputeViewProjectionMatrix() const;
};

/** A projection from scene space into a 2D screen region. */
class FSceneView
{
public:
	/** Transforms a point from world-space to the view's screen-space (static version). */
	static bool ProjectWorldToScreen(const FVector& WorldPosition, const FIntRect& ViewRect, const FMatrix& ViewProjectionMatrix, FVector2D& out_ScreenPos, bool bShouldCalcOutsideViewPosition = false);
};

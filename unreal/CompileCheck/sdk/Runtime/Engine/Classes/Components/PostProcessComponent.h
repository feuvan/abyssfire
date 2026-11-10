// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Components/PostProcessComponent.h (UE 5.x source knowledge;
// UFUNCTION / UPROPERTY-marked members: checked against the 5.6 stub). The real class also implements
// IInterface_PostProcessVolume (not modelled).
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/SceneComponent.h"
#include "Engine/Scene.h"

/** PostProcessComponent: enables Post process controls for blueprints; combined with a shape component it bounds the volume. */
class UPostProcessComponent : public USceneComponent
{
	UE_STUB_CLASS_BODY(UPostProcessComponent, USceneComponent)
public:
	/** Post process settings to use for this volume. */
	UPROPERTY(interp, Category = PostProcessVolume, BlueprintReadWrite, meta = (ShowOnlyInnerProperties))
	struct FPostProcessSettings Settings;
	/** Priority of this volume. In the case of overlapping volumes the one with the highest priority overrides the lower priority ones. */
	UPROPERTY(interp, Category = PostProcessVolume, BlueprintReadWrite)
	float Priority;
	/** World space radius around the volume that is used for blending (only if not unbound). */
	UPROPERTY(interp, Category = PostProcessVolume, BlueprintReadWrite, meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "6000.0"))
	float BlendRadius;
	/** 0: no effect, 1: full effect. */
	UPROPERTY(interp, Category = PostProcessVolume, BlueprintReadWrite, meta = (UIMin = "0.0", UIMax = "1.0"))
	float BlendWeight;
	/** Whether this volume is enabled or not. */
	UPROPERTY(interp, Category = PostProcessVolume, BlueprintReadWrite)
	uint32 bEnabled : 1;
	/** Set this to false to use the parent shape component as volume bounds. True affects the whole world regardless. */
	UPROPERTY(interp, Category = PostProcessVolume, BlueprintReadWrite)
	uint32 bUnbound : 1;
	/** Adds an Blendable (implements IBlendableInterface) to the array of Blendables (if it doesn't exist) and update the weight. */
	UFUNCTION(BlueprintCallable, Category = "Rendering")
	void AddOrUpdateBlendable(TScriptInterface<IBlendableInterface> InBlendableObject, float InWeight = 1.0f) { Settings.AddBlendable(InBlendableObject, InWeight); }
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
};

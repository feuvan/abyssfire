// Stub SDK. Real header: Engine/Source/Runtime/RHI/Public/RHI.h (+ RHIGlobals.h; UE 5.x source knowledge).
// The RHI globals a game module may read; FUpdateTextureRegion2D (dynamic texture updates).
#pragma once

#include "CoreMinimal.h"
#include "RHIDefinitions.h"

extern ERHIFeatureLevel::Type GMaxRHIFeatureLevel;
extern EShaderPlatform GMaxRHIShaderPlatform;
extern bool GIsRHIInitialized;
extern bool GUsingNullRHI;
extern int32 GMaxTextureDimensions;
extern FString GRHIAdapterName;
extern FString GRHIAdapterDriverDate;
extern uint32 GRHIVendorId;

struct FUpdateTextureRegion2D
{
	uint32 DestX;
	uint32 DestY;
	int32 SrcX;
	int32 SrcY;
	uint32 Width;
	uint32 Height;
	FUpdateTextureRegion2D() : DestX(0), DestY(0), SrcX(0), SrcY(0), Width(0), Height(0) {}
	FUpdateTextureRegion2D(uint32 InDestX, uint32 InDestY, int32 InSrcX, int32 InSrcY, uint32 InWidth, uint32 InHeight)
		: DestX(InDestX), DestY(InDestY), SrcX(InSrcX), SrcY(InSrcY), Width(InWidth), Height(InHeight) {}
};
bool IsRunningRHIInSeparateThread();

// Stub SDK. Real headers: Engine/Source/Runtime/RHI/Public/RHIDefinitions.h, RHIFeatureLevel.h, RHIShaderPlatform.h (UE
// 5.x source knowledge). Enumerations only.
#pragma once

#include "CoreMinimal.h"
#include "PixelFormat.h"

namespace ERHIFeatureLevel
{
	enum Type : int
	{
		ES2_REMOVED,
		ES3_1,
		SM4_REMOVED,
		SM5,
		SM6,
		Num,
	};
}
enum EShaderPlatform : uint16
{
	SP_PCD3D_SM5 = 0,
	SP_METAL = 11,
	SP_METAL_MRT = 12,
	SP_PCD3D_ES3_1 = 14,
	SP_OPENGL_ES3_1_ANDROID = 17,
	SP_METAL_SM5 = 19,
	SP_VULKAN_PCES3_1 = 20,
	SP_VULKAN_SM5 = 22,
	SP_VULKAN_ES3_1_ANDROID = 23,
	SP_METAL_MACES3_1 = 24,
	SP_PCD3D_SM6 = 30,
	SP_METAL_SM6 = 36,
	SP_NumPlatforms = 53,
};
enum ESamplerFilter { SF_Point, SF_Bilinear, SF_Trilinear, SF_AnisotropicPoint, SF_AnisotropicLinear, ESamplerFilter_Num };

// Stub SDK. Real headers: Engine/Source/Runtime/RHI/Public/RHIDefinitions.h, RHIFeatureLevel.h, RHIShaderPlatform.h (UE
// 5.x source knowledge). Enumerations only.
#pragma once

#include "CoreMinimal.h"

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
enum EPixelFormat : uint8
{
	PF_Unknown = 0,
	PF_A32B32G32R32F = 1,
	PF_B8G8R8A8 = 2,
	PF_G8 = 3,
	PF_G16 = 4,
	PF_DXT1 = 5,
	PF_DXT3 = 6,
	PF_DXT5 = 7,
	PF_FloatRGB = 9,
	PF_FloatRGBA = 10,
	PF_R32_FLOAT = 13,
	PF_A16B16G16R16 = 17,
	PF_R16F = 21,
	PF_R8G8B8A8 = 37,
	PF_R8 = 57,
	PF_MAX = 93,
};
enum ESamplerFilter { SF_Point, SF_Bilinear, SF_Trilinear, SF_AnisotropicPoint, SF_AnisotropicLinear, ESamplerFilter_Num };

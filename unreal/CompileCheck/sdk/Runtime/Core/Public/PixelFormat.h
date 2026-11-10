// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/PixelFormat.h (UE 5.x source knowledge). EPixelFormat with the
// formats a game creates textures in; the numeric values are the engine's.
#pragma once

#include "CoreTypes.h"

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

// Stub SDK. Real header: Engine/Source/Runtime/Engine/Public/TextureResource.h (UE 5.x source knowledge).
// FTexture2DMipMap (one mip: its bulk data) and FTexturePlatformData (what UTexture2D::GetPlatformData() returns: the
// mip chain of a cooked / transient texture). FTextureResource is only named.
#pragma once

#include "CoreMinimal.h"
#include "Containers/IndirectArray.h"
#include "PixelFormat.h"
#include "Serialization/BulkData.h"

class UTexture;
class UTexture2D;

/** A 2D texture mip-map. */
struct FTexture2DMipMap
{
	/** Bulk data if stored in the package. */
	FByteBulkData BulkData;
};

/** Platform-specific data used by the texture resource at runtime. */
struct FTexturePlatformData
{
	/** Width of the texture. */
	int32 SizeX;
	/** Height of the texture. */
	int32 SizeY;
	/** Format in which mip data is stored. */
	EPixelFormat PixelFormat;
	/** Mip data or VT data. one or the other. */
	TIndirectArray<struct FTexture2DMipMap> Mips;
	FTexturePlatformData();
	~FTexturePlatformData();
};

/** The rendering resource which represents a texture. */
class FTextureResource
{
public:
	FTextureResource();
	virtual ~FTextureResource();
};

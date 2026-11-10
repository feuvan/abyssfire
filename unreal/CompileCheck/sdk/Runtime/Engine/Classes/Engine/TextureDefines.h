// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Engine/TextureDefines.h (UE 5.x source knowledge;
// enumerators checked against the 5.6 stub). Only the commonly used enumerators of TextureGroup are listed.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

UENUM()
enum TextureGroup : int
{
	TEXTUREGROUP_World,
	TEXTUREGROUP_WorldNormalMap,
	TEXTUREGROUP_WorldSpecular,
	TEXTUREGROUP_Character,
	TEXTUREGROUP_CharacterNormalMap,
	TEXTUREGROUP_CharacterSpecular,
	TEXTUREGROUP_Weapon,
	TEXTUREGROUP_WeaponNormalMap,
	TEXTUREGROUP_WeaponSpecular,
	TEXTUREGROUP_Vehicle,
	TEXTUREGROUP_VehicleNormalMap,
	TEXTUREGROUP_VehicleSpecular,
	TEXTUREGROUP_Cinematic,
	TEXTUREGROUP_Effects,
	TEXTUREGROUP_EffectsNotFiltered,
	TEXTUREGROUP_Skybox,
	TEXTUREGROUP_UI,
	TEXTUREGROUP_Lightmap,
	TEXTUREGROUP_RenderTarget,
	TEXTUREGROUP_MobileFlattened,
	TEXTUREGROUP_ProcBuilding_Face,
	TEXTUREGROUP_ProcBuilding_LightMap,
	TEXTUREGROUP_Shadowmap,
	TEXTUREGROUP_ColorLookupTable,
	TEXTUREGROUP_Terrain_Heightmap,
	TEXTUREGROUP_Terrain_Weightmap,
	TEXTUREGROUP_Bokeh,
	TEXTUREGROUP_IESLightProfile,
	TEXTUREGROUP_Pixels2D,
	TEXTUREGROUP_HierarchicalLOD,
	TEXTUREGROUP_Impostor,
	TEXTUREGROUP_ImpostorNormalDepth,
	TEXTUREGROUP_8BitData,
	TEXTUREGROUP_16BitData,
};

UENUM()
enum TextureFilter : int
{
	TF_Nearest,
	TF_Bilinear,
	TF_Trilinear,
	TF_Default,
	TF_MAX,
};

UENUM()
enum TextureAddress : int
{
	TA_Wrap,
	TA_Clamp,
	TA_Mirror,
	TA_MAX,
};

UENUM()
enum TextureCompressionSettings : int
{
	TC_Default,
	TC_Normalmap,
	TC_Masks,
	TC_Grayscale,
	TC_Displacementmap,
	TC_VectorDisplacementmap,
	TC_HDR,
	TC_EditorIcon,
	TC_Alpha,
	TC_DistanceFieldFont,
	TC_HDR_Compressed,
	TC_BC7,
	TC_HalfFloat,
	TC_LQ,
	TC_EncodedReflectionCapture,
	TC_SingleFloat,
	TC_HDR_F32,
	TC_MAX,
};

UENUM()
enum TextureMipGenSettings : int
{
	TMGS_FromTextureGroup,
	TMGS_SimpleAverage,
	TMGS_Sharpen0,
	TMGS_Sharpen1,
	TMGS_Sharpen2,
	TMGS_Sharpen3,
	TMGS_Sharpen4,
	TMGS_Sharpen5,
	TMGS_Sharpen6,
	TMGS_Sharpen7,
	TMGS_Sharpen8,
	TMGS_Sharpen9,
	TMGS_Sharpen10,
	TMGS_NoMipmaps,
	TMGS_LeaveExistingMips,
	TMGS_Blur1,
	TMGS_Blur2,
	TMGS_Blur3,
	TMGS_Blur4,
	TMGS_Blur5,
	TMGS_Unfiltered,
	TMGS_Angular,
	TMGS_MAX,
};

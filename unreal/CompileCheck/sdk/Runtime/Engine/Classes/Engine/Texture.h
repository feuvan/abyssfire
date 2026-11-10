// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Engine/Texture.h (UE 5.x source knowledge; UPROPERTY-marked
// members: checked against the 5.6 stub). Abstract in the engine (UCLASS(abstract)).
#pragma once

#include "CoreMinimal.h"
#include "Engine/StreamableRenderAsset.h"
#include "Engine/TextureDefines.h"

class FTextureResource;

class UTexture : public UStreamableRenderAsset
{
	UE_STUB_CLASS_BODY(UTexture, UStreamableRenderAsset)
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Texture)
	uint8 SRGB : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Texture)
	TEnumAsByte<enum TextureFilter> Filter;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Compression)
	TEnumAsByte<enum TextureCompressionSettings> CompressionSettings;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LevelOfDetail)
	TEnumAsByte<enum TextureGroup> LODGroup;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LevelOfDetail)
	uint8 NeverStream : 1;
	virtual FTextureResource* CreateResource() PURE_VIRTUAL(UTexture::CreateResource, return nullptr;);
	virtual void UpdateResource();
	FTextureResource* GetResource();
	const FTextureResource* GetResource() const;
	void SetResource(FTextureResource* InResource);
	virtual void ReleaseResource();
	virtual float GetSurfaceWidth() const PURE_VIRTUAL(UTexture::GetSurfaceWidth, return 0;);
	virtual float GetSurfaceHeight() const PURE_VIRTUAL(UTexture::GetSurfaceHeight, return 0;);
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;
};

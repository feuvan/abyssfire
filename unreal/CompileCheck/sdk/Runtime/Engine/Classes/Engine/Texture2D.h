// Stub SDK. Real header: Engine/Source/Runtime/Engine/Classes/Engine/Texture2D.h (UE 5.x source knowledge;
// UFUNCTION / UPROPERTY-marked members: checked against the 5.6 stub). FUpdateTextureRegion2D is only forward-declared
// here, as in the engine: its definition comes with RHI.h.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/Texture.h"
#include "Engine/TextureDefines.h"

struct FUpdateTextureRegion2D;

class UTexture2D : public UTexture
{
	UE_STUB_CLASS_BODY(UTexture2D, UTexture)
public:
	/** The addressing mode to use for the X axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Texture, meta = (DisplayName = "X-axis Tiling Method"), AssetRegistrySearchable, AdvancedDisplay)
	TEnumAsByte<enum TextureAddress> AddressX;
	/** The addressing mode to use for the Y axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Texture, meta = (DisplayName = "Y-axis Tiling Method"), AssetRegistrySearchable, AdvancedDisplay)
	TEnumAsByte<enum TextureAddress> AddressY;
	int32 GetSizeX() const;
	int32 GetSizeY() const;
	/** Gets the X size of the texture, in pixels. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "GetSizeX"), Category = "Rendering|Texture")
	int32 Blueprint_GetSizeX() const;
	/** Gets the Y size of the texture, in pixels. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "GetSizeY"), Category = "Rendering|Texture")
	int32 Blueprint_GetSizeY() const;
	/** Create and initialize a new transient texture (PF_B8G8R8A8 unless told otherwise). */
	static UTexture2D* CreateTransient(int32 InSizeX, int32 InSizeY, EPixelFormat InFormat = PF_B8G8R8A8, const FName InName = NAME_None);
	/**
	 * Asynchronously update a set of regions of a texture with new data.
	 * @param DataCleanupFunc function to be called on the render thread when the update is done (free the data there)
	 */
	void UpdateTextureRegions(int32 MipIndex, uint32 NumRegions, const FUpdateTextureRegion2D* Regions, uint32 SrcPitch, uint32 SrcBpp, uint8* SrcData,
		TFunction<void(uint8* SrcData, const FUpdateTextureRegion2D* Regions)> DataCleanupFunc = [](uint8*, const FUpdateTextureRegion2D*) {});
	virtual FTextureResource* CreateResource() override;
	virtual void UpdateResource() override;
	virtual float GetSurfaceWidth() const override;
	virtual float GetSurfaceHeight() const override;
	virtual void BeginDestroy() override;
};

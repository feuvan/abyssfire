// UAbyssAssetLibrary: runtime asset access by manifest id (ue58-platform.md 10.3). Owns the art manifest, resolves game ids
// to soft object paths under /Game/Abyssfire (see AbyssArtManifest.h for the mapping) and keeps loaded assets referenced
// across zone changes with a small zone LRU (web sheetKeepZones: 2 zones on desktop, 1 on touch, CLAUDE.md).
//
// Everything is loaded on the game thread: synchronously on first use (cheap after the async zone preload issued when a
// zone transition starts), so a missing asset never blocks gameplay and is logged once.
#pragma once

#include "CoreMinimal.h"
#include "Engine/StreamableManager.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/SoftObjectPath.h"

#include "World/AbyssArtManifest.h"

#include "AbyssAssetLibrary.generated.h"

class UAnimSequence;
class UMaterialInterface;
class UMaterialParameterCollection;
class USkeletalMesh;
class UStaticMesh;
class UTexture2D;

UCLASS()
class ABYSSFIRE_API UAbyssAssetLibrary : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UAbyssAssetLibrary* Get(const UObject* WorldContextObject);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	const FAbyssArtManifest& GetManifest() const { return Manifest; }
	bool HasManifest() const { return Manifest.IsLoaded(); }

	// ---- typed loads (nullptr when the asset does not exist; logged once per path) ----
	USkeletalMesh* LoadSkeletalMesh(const FAbyssArtAsset& Asset);
	UStaticMesh* LoadStaticMesh(const FAbyssArtAsset& Asset);
	UAnimSequence* LoadClip(const FAbyssArtClip& Clip);
	/** A content-build asset by name in a folder under /Game/Abyssfire (FX meshes / materials not in the art manifest). */
	UStaticMesh* LoadContentMesh(const TCHAR* Folder, const TCHAR* AssetName);
	/**
	 * Material by name: /Game/Abyssfire/Materials/<Name>, then Materials/Instances/<Name>, then Materials/FX/<Name>
	 * (the content build's folders, Scripts/build_content.py).
	 */
	UMaterialInterface* LoadMaterial(const TCHAR* MaterialName);
	UTexture2D* LoadTexture(const TCHAR* TextureName);
	/** /Game/Abyssfire/Materials/MPC_AF_Lighting (ue58-platform.md 6.4). */
	UMaterialParameterCollection* GetLightingCollection();

	/** Generic cached load; the class must match. */
	UObject* LoadObjectAt(const FSoftObjectPath& Path, UClass* ExpectedClass);
	bool DoesAssetExist(const FSoftObjectPath& Path) const;

	// ---- zone retention ----
	/** A new zone starts using assets: bumps the zone counter (assets touched from now on belong to it). */
	void BeginZone();
	/** Drops references to assets not used by the last KeepZones zones (GC may then unload them). */
	void TrimToRecentZones(int32 KeepZones);
	/** Async preload (zone transition fade): fire and forget; LoadObjectAt finds them resident afterwards. */
	void PreloadAsync(const TArray<FSoftObjectPath>& Paths);

private:
	void TouchLoaded(FName Key);

	FAbyssArtManifest Manifest;
	FStreamableManager Streamable;
	TSharedPtr<FStreamableHandle> PreloadHandle;

	/** Loaded assets (kept alive while referenced here). Key: object path. */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UObject>> Loaded;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialParameterCollection> LightingCollection;

	TMap<FName, uint32> LastZoneUse;
	TSet<FName> MissingPaths;
	uint32 ZoneCounter = 0;
	bool bLightingCollectionResolved = false;
};

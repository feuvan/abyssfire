#include "World/AbyssAssetLibrary.h"

#include "Abyssfire.h"
#include "Animation/AnimSequence.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "Misc/PackageName.h"

UAbyssAssetLibrary* UAbyssAssetLibrary::Get(const UObject* WorldContextObject)
{
	const UWorld* OwningWorld = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const UGameInstance* GameInstance = OwningWorld ? OwningWorld->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UAbyssAssetLibrary>() : nullptr;
}

void UAbyssAssetLibrary::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FString Error;
	FString Source;
	if (Manifest.LoadFromProject(Error, Source))
	{
		UE_LOG(LogAbyss, Log, TEXT("Art manifest: %d assets from %s"), Manifest.Num(), *Source);
	}
	else
	{
		// Not fatal: the world still builds (terrain, VFX from content), characters without art are skipped and logged.
		UE_LOG(LogAbyss, Error, TEXT("Art manifest not loaded: %s"), *Error);
	}
}

void UAbyssAssetLibrary::Deinitialize()
{
	if (PreloadHandle.IsValid())
	{
		PreloadHandle->CancelHandle();
		PreloadHandle.Reset();
	}
	Loaded.Reset();
	LastZoneUse.Reset();
	MissingPaths.Reset();
	LightingCollection = nullptr;
	bLightingCollectionResolved = false;
	Manifest.Reset();
	Super::Deinitialize();
}

bool UAbyssAssetLibrary::DoesAssetExist(const FSoftObjectPath& Path) const
{
	if (Path.IsNull())
	{
		return false;
	}
	return FPackageName::DoesPackageExist(Path.GetLongPackageName());
}

void UAbyssAssetLibrary::TouchLoaded(FName Key)
{
	LastZoneUse.FindOrAdd(Key) = ZoneCounter;
}

UObject* UAbyssAssetLibrary::LoadObjectAt(const FSoftObjectPath& Path, UClass* ExpectedClass)
{
	check(IsInGameThread());
	if (Path.IsNull())
	{
		return nullptr;
	}
	const FName Key(*Path.ToString());
	if (const TObjectPtr<UObject>* Found = Loaded.Find(Key))
	{
		if (UObject* Object = Found->Get())
		{
			TouchLoaded(Key);
			return Object;
		}
	}
	if (MissingPaths.Contains(Key))
	{
		return nullptr;
	}
	UObject* Object = Path.ResolveObject();
	if (Object == nullptr)
	{
		if (!DoesAssetExist(Path))
		{
			MissingPaths.Add(Key);
			UE_LOG(LogAbyss, Warning, TEXT("Asset missing: %s (run Scripts/build_content.py)"), *Path.ToString());
			return nullptr;
		}
		Object = Path.TryLoad();
	}
	if (Object == nullptr || (ExpectedClass != nullptr && !Object->IsA(ExpectedClass)))
	{
		MissingPaths.Add(Key);
		UE_LOG(LogAbyss, Warning, TEXT("Asset %s could not be loaded as %s"), *Path.ToString(),
			ExpectedClass ? *ExpectedClass->GetName() : TEXT("object"));
		return nullptr;
	}
	Loaded.Add(Key, Object);
	TouchLoaded(Key);
	return Object;
}

USkeletalMesh* UAbyssAssetLibrary::LoadSkeletalMesh(const FAbyssArtAsset& Asset)
{
	return Cast<USkeletalMesh>(LoadObjectAt(Asset.GetObjectPath(), USkeletalMesh::StaticClass()));
}

UStaticMesh* UAbyssAssetLibrary::LoadStaticMesh(const FAbyssArtAsset& Asset)
{
	return Cast<UStaticMesh>(LoadObjectAt(Asset.GetObjectPath(), UStaticMesh::StaticClass()));
}

UAnimSequence* UAbyssAssetLibrary::LoadClip(const FAbyssArtClip& Clip)
{
	return Cast<UAnimSequence>(LoadObjectAt(Clip.GetObjectPath(), UAnimSequence::StaticClass()));
}

UStaticMesh* UAbyssAssetLibrary::LoadContentMesh(const TCHAR* Folder, const TCHAR* AssetName)
{
	return Cast<UStaticMesh>(LoadObjectAt(FAbyssArtManifest::MakeObjectPath(Folder, FName(AssetName)), UStaticMesh::StaticClass()));
}

UMaterialInterface* UAbyssAssetLibrary::LoadMaterial(const TCHAR* MaterialName)
{
	static const TCHAR* const Folders[] = { TEXT("Materials"), TEXT("Materials/Instances"), TEXT("Materials/FX") };
	const FName Name(MaterialName);
	for (const TCHAR* Folder : Folders)
	{
		const FSoftObjectPath Path = FAbyssArtManifest::MakeObjectPath(Folder, Name);
		const FName Key(*Path.ToString());
		if (const TObjectPtr<UObject>* Found = Loaded.Find(Key); Found != nullptr && Found->Get() != nullptr)
		{
			TouchLoaded(Key);
			return Cast<UMaterialInterface>(Found->Get());
		}
		if (!DoesAssetExist(Path))
		{
			continue;
		}
		if (UMaterialInterface* Material = Cast<UMaterialInterface>(LoadObjectAt(Path, UMaterialInterface::StaticClass())))
		{
			return Material;
		}
	}
	const FName MissingKey(*FString::Printf(TEXT("material:%s"), MaterialName));
	if (!MissingPaths.Contains(MissingKey))
	{
		MissingPaths.Add(MissingKey);
		UE_LOG(LogAbyss, Warning, TEXT("Material %s not found under %s/Materials (run Scripts/build_content.py)"),
			MaterialName, FAbyssArtManifest::ContentRoot());
	}
	return nullptr;
}

UTexture2D* UAbyssAssetLibrary::LoadTexture(const TCHAR* TextureName)
{
	return Cast<UTexture2D>(LoadObjectAt(FAbyssArtManifest::MakeObjectPath(TEXT("Textures"), FName(TextureName)),
		UTexture2D::StaticClass()));
}

UMaterialParameterCollection* UAbyssAssetLibrary::GetLightingCollection()
{
	if (!bLightingCollectionResolved)
	{
		bLightingCollectionResolved = true;
		LightingCollection = Cast<UMaterialParameterCollection>(LoadObjectAt(
			FAbyssArtManifest::MakeObjectPath(TEXT("Materials"), FName(TEXT("MPC_AF_Lighting"))),
			UMaterialParameterCollection::StaticClass()));
	}
	return LightingCollection;
}

void UAbyssAssetLibrary::BeginZone()
{
	++ZoneCounter;
}

void UAbyssAssetLibrary::TrimToRecentZones(int32 KeepZones)
{
	const uint32 Keep = static_cast<uint32>(FMath::Max(1, KeepZones));
	if (ZoneCounter < Keep)
	{
		return;
	}
	const uint32 Oldest = ZoneCounter - Keep + 1;
	TArray<FName> Drop;
	for (const TPair<FName, uint32>& Pair : LastZoneUse)
	{
		if (Pair.Value < Oldest)
		{
			Drop.Add(Pair.Key);
		}
	}
	for (const FName Key : Drop)
	{
		LastZoneUse.Remove(Key);
		Loaded.Remove(Key);
	}
	if (Drop.Num() > 0)
	{
		UE_LOG(LogAbyss, Verbose, TEXT("Asset library: released %d assets of older zones"), Drop.Num());
	}
}

void UAbyssAssetLibrary::PreloadAsync(const TArray<FSoftObjectPath>& Paths)
{
	TArray<FSoftObjectPath> Existing;
	Existing.Reserve(Paths.Num());
	for (const FSoftObjectPath& Path : Paths)
	{
		const FName Key(*Path.ToString());
		if (Path.IsNull() || MissingPaths.Contains(Key) || Path.ResolveObject() != nullptr)
		{
			continue;
		}
		if (DoesAssetExist(Path))
		{
			Existing.AddUnique(Path);
		}
	}
	if (PreloadHandle.IsValid())
	{
		PreloadHandle->ReleaseHandle();
		PreloadHandle.Reset();
	}
	if (Existing.Num() > 0)
	{
		PreloadHandle = Streamable.RequestAsyncLoad(MoveTemp(Existing));
	}
}

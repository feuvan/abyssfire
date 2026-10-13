#include "World/AbyssAssetLibrary.h"

#include "Abyssfire.h"
#include "Animation/AnimSequence.h"
#include "CoreGlobals.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

#if WITH_EDITOR
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#endif

#include <string_view>

#if WITH_EDITOR
// Uncooked runs compile shaders on first use. Until a material's shader map is ready UE draws its primitives with the
// engine's default material (lit grey, near black away from the sun: the scene's only light is the directional sun),
// which the second Mac playtest showed as dark lumps where the berry bushes and grass tufts stand (the foliage masters
// were still in the "Preparing Shaders" queue). Cooked builds ship compiled shaders and compile this out.
static TAutoConsoleVariable<int32> CVarAbyssWaitForCompilation(
	TEXT("abyss.WaitForCompilation"),
	1,
	TEXT("Uncooked runs (the editor binary): finish pending shader compiles and asynchronous asset builds before the frame ")
	TEXT("is drawn, so nothing shows the engine's default material while its own shaders compile. 0: never. 1: the first ")
	TEXT("frames of every zone, standalone -game runs only (default; PIE is left alone). 2: every frame, PIE too (hitches ")
	TEXT("whenever something new compiles). Scripts/build_content.sh --shaders pre-compiles the masters (shorter waits)."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarAbyssWaitForCompilationMaxSec(
	TEXT("abyss.WaitForCompilationMaxSec"),
	20.f,
	TEXT("abyss.WaitForCompilation: longest wait in seconds (the window does not respond meanwhile). After it the frame is ")
	TEXT("drawn and the rest compiles in the background: zone dressing stays hidden until ready (abyss.DecorShaderGate), ")
	TEXT("other meshes draw with the engine default material meanwhile, and the zone's next frames do not wait again. ")
	TEXT("<= 0: no limit."),
	ECVF_Default);
#endif

namespace AbyssAssetLibraryPrivate
{
#if WITH_EDITOR
	/** One warning per run and entry point when this engine lacks one of the compiling-manager calls used below. */
	void WarnCompileEntryPointMissing(bool& bWarned, const TCHAR* EntryPoint, const TCHAR* Consequence)
	{
		if (!bWarned)
		{
			bWarned = true;
			UE_LOG(LogAbyss, Warning, TEXT("Uncooked run: %s not found on this engine, so %s"), EntryPoint, Consequence);
		}
	}

	FString CompileCountText(int32 Count)
	{
		return Count < 0 ? FString(TEXT("?")) : FString::FromInt(Count);
	}

	// Pending work counts of the engine's compiling managers, called through whichever entry point this engine version
	// declares (resolved at compile time; the other branch is never instantiated). -1: no count available (logged once),
	// wait anyway without a time limit.
	template <typename ShaderManagerType>
	int32 PendingShaderJobs(ShaderManagerType* Manager)
	{
		if (Manager == nullptr)
		{
			return 0;
		}
		if constexpr (requires(ShaderManagerType* M) { M->GetNumRemainingJobs(); })
		{
			return static_cast<int32>(Manager->GetNumRemainingJobs());
		}
		else
		{
			static bool bWarned = false;
			WarnCompileEntryPointMissing(bWarned, TEXT("FShaderCompilingManager::GetNumRemainingJobs"),
				TEXT("abyss.WaitForCompilation waits without shader job counts and without its time limit"));
			return -1;
		}
	}

	template <typename AssetManagerType>
	int32 PendingAssetBuilds(AssetManagerType& Manager)
	{
		if constexpr (requires(AssetManagerType& M) { M.GetNumRemainingAssets(); })
		{
			return static_cast<int32>(Manager.GetNumRemainingAssets());
		}
		else
		{
			static bool bWarned = false;
			WarnCompileEntryPointMissing(bWarned, TEXT("FAssetCompilingManager::GetNumRemainingAssets"),
				TEXT("abyss.WaitForCompilation waits without asset build counts and without its time limit"));
			return -1;
		}
	}

	/**
	 * Applies the results that are ready (built meshes to their components, shader maps to their materials; the asset
	 * manager drives every registered compiling manager). False when this engine has no such entry point.
	 */
	template <typename AssetManagerType>
	bool PumpAssetCompilation(AssetManagerType& Manager)
	{
		if constexpr (requires(AssetManagerType& M) { M.ProcessAsyncTasks(false); })
		{
			Manager.ProcessAsyncTasks(false);
			return true;
		}
		else
		{
			static bool bWarned = false;
			WarnCompileEntryPointMissing(bWarned, TEXT("FAssetCompilingManager::ProcessAsyncTasks"),
				TEXT("abyss.WaitForCompilation blocks until everything is compiled (no time limit)"));
			return false;
		}
	}

	/** Shader maps that are ready, applied to their materials (unlimited time slice); a no-op without the entry point. */
	template <typename ShaderManagerType>
	void PumpShaderCompilation(ShaderManagerType* Manager)
	{
		if constexpr (requires(ShaderManagerType* M) { M->ProcessAsyncResults(false, false); })
		{
			if (Manager != nullptr)
			{
				Manager->ProcessAsyncResults(false, false);
			}
		}
	}
#endif

	/**
	 * Data/assets.json is the exporter's copy of Art/Export/manifest.json and is read first (the core DataStore reads the
	 * same copy for decor footprints, so the fix is the re-export, not reading the art file here). Uncooked runs have both
	 * files: a copy that lags the art export hides every asset added since without an error (the first Mac playtest loaded
	 * 17 of 198 assets: no foliage, props, NPCs or loot bags). Packaged builds have no Art/ folder and skip the check.
	 */
	void WarnIfManifestCopyStale(const FAbyssArtManifest& Loaded, const FString& SourcePath)
	{
		const FString ArtExport = FPaths::ProjectDir() / TEXT("Art/Export/manifest.json");
		if (FPaths::IsSamePath(SourcePath, ArtExport) || !FPaths::FileExists(ArtExport))
		{
			return;
		}
		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *ArtExport, FILEREAD_Silent))
		{
			return;
		}
		FAbyssArtManifest ArtManifest;
		FString ParseError;
		if (!ArtManifest.LoadFromJson(std::string_view(reinterpret_cast<const char*>(Bytes.GetData()), static_cast<size_t>(Bytes.Num())),
			ParseError))
		{
			UE_LOG(LogAbyss, Warning, TEXT("Art manifest check: %s does not parse (%s)"), *ArtExport, *ParseError);
			return;
		}
		int32 MissingFromCopy = 0;
		FString Examples;
		for (const FAbyssArtAsset& Asset : ArtManifest.GetAssets())
		{
			if (Loaded.FindAsset(Asset.Name) != nullptr)
			{
				continue;
			}
			if (MissingFromCopy < 6)
			{
				Examples += Examples.IsEmpty() ? Asset.Name.ToString() : FString(TEXT(", ")) + Asset.Name.ToString();
			}
			++MissingFromCopy;
		}
		if (MissingFromCopy == 0 && ArtManifest.Num() == Loaded.Num())
		{
			return;
		}
		UE_LOG(LogAbyss, Warning, TEXT("Art manifest %s is stale: %d assets vs %d in %s (%d missing from the copy, e.g. %s; "
			"copy %s, art export %s). Zone dressing, NPCs and loot without a manifest entry are not drawn. Re-export from "
			"the repo root: node unreal/Tools/export-data/run.mjs"),
			*SourcePath, Loaded.Num(), ArtManifest.Num(), *ArtExport, MissingFromCopy, Examples.IsEmpty() ? TEXT("-") : *Examples,
			*IFileManager::Get().GetTimeStamp(*SourcePath).ToString(), *IFileManager::Get().GetTimeStamp(*ArtExport).ToString());
	}
}

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
		AbyssAssetLibraryPrivate::WarnIfManifestCopyStale(Manifest, Source);
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

UObject* UAbyssAssetLibrary::LoadFromFolders(TConstArrayView<const TCHAR*> Folders, FName AssetName, UClass* ExpectedClass)
{
	if (AssetName.IsNone())
	{
		return nullptr;
	}
	for (const TCHAR* Folder : Folders)
	{
		const FSoftObjectPath Path = FAbyssArtManifest::MakeObjectPath(Folder, AssetName);
		const FName Key(*Path.ToString());
		if (const TObjectPtr<UObject>* Found = Loaded.Find(Key); Found != nullptr && Found->Get() != nullptr)
		{
			TouchLoaded(Key);
			return Found->Get();
		}
		if (Path.ResolveObject() == nullptr && !DoesAssetExist(Path))
		{
			continue;
		}
		if (UObject* Object = LoadObjectAt(Path, ExpectedClass))
		{
			return Object;
		}
	}
	const FName MissingKey(*FString::Printf(TEXT("folders:%s"), *AssetName.ToString()));
	if (!MissingPaths.Contains(MissingKey))
	{
		MissingPaths.Add(MissingKey);
		UE_LOG(LogAbyss, Warning, TEXT("%s not found under %s (run Scripts/build_content.py)"), *AssetName.ToString(),
			FAbyssArtManifest::ContentRoot());
	}
	return nullptr;
}

UStaticMesh* UAbyssAssetLibrary::LoadStaticMeshByName(FName AssetName)
{
	if (AssetName.IsNone())
	{
		return nullptr;
	}
	if (const FAbyssArtAsset* Asset = Manifest.FindAsset(AssetName); Asset != nullptr && !Asset->IsSkeletal())
	{
		if (UStaticMesh* Mesh = LoadStaticMesh(*Asset))
		{
			return Mesh;
		}
	}
	static const TCHAR* const Folders[] = { TEXT("FX"), TEXT("FX/Meshes"), TEXT("Pickups"), TEXT("Props") };
	return Cast<UStaticMesh>(LoadFromFolders(Folders, AssetName, UStaticMesh::StaticClass()));
}

UTexture2D* UAbyssAssetLibrary::LoadFxTexture(FName Sprite)
{
	if (Sprite.IsNone())
	{
		return nullptr;
	}
	static const TCHAR* const Folders[] = { TEXT("FX/Textures"), TEXT("Textures"), TEXT("FX") };
	const FName TextureName(*FString::Printf(TEXT("T_FX_%s"), *Sprite.ToString()));
	return Cast<UTexture2D>(LoadFromFolders(Folders, TextureName, UTexture2D::StaticClass()));
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

EAbyssCompileWait UAbyssAssetLibrary::WaitForPendingCompilation(bool bZoneEntry)
{
#if WITH_EDITOR
	using namespace AbyssAssetLibraryPrivate;
	const int32 Mode = CVarAbyssWaitForCompilation.GetValueOnGameThread();
	// Mode 1 waits only behind a zone entry of a standalone -game run: in PIE (GIsEditor) the editor's own compile
	// backlog (a material just edited, the content browser) would stall the session.
	if (Mode <= 0 || (Mode == 1 && (GIsEditor || !bZoneEntry)))
	{
		return EAbyssCompileWait::None;
	}
	FAssetCompilingManager& AssetCompiling = FAssetCompilingManager::Get();
	const int32 AssetBuilds = PendingAssetBuilds(AssetCompiling);
	const int32 ShaderJobs = PendingShaderJobs(GShaderCompilingManager);
	if (AssetBuilds == 0 && ShaderJobs == 0)
	{
		return EAbyssCompileWait::None;
	}
	const double MaxSec = static_cast<double>(CVarAbyssWaitForCompilationMaxSec.GetValueOnGameThread());
	const bool bBounded = MaxSec > 0.0 && AssetBuilds >= 0 && ShaderJobs >= 0;
	// Logged before blocking (the window does not respond meanwhile). Unknown counts ('?') are logged at zone entry only,
	// so mode 2 does not log every frame.
	if (bZoneEntry || AssetBuilds > 0 || ShaderJobs > 0)
	{
		const FString ShaderText = CompileCountText(ShaderJobs);
		const FString AssetText = CompileCountText(AssetBuilds);
		const FString LimitText = bBounded ? FString::Printf(TEXT("at most %.1f s"), MaxSec) : FString(TEXT("no time limit"));
		UE_LOG(LogAbyss, Display, TEXT("Uncooked run: finishing %s shader jobs and %s asset builds before drawing (%s, %s). "
			"Pre-compile the masters with Scripts/build_content.sh --shaders; abyss.WaitForCompilation 0 skips this wait."),
			*ShaderText, *AssetText, bZoneEntry ? TEXT("zone entry") : TEXT("mid-play"), *LimitText);
	}
	const double StartSec = FPlatformTime::Seconds();
	bool bTimedOut = false;
	int32 LeftShaderJobs = 0;
	int32 LeftAssetBuilds = 0;
	if (bBounded)
	{
		// The shader workers and build threads keep compiling on their own; poll their counts and apply what is ready,
		// up to the limit. A cold shader cache can take minutes, and nothing can be typed while the game thread waits.
		while (PumpAssetCompilation(AssetCompiling))
		{
			PumpShaderCompilation(GShaderCompilingManager);
			LeftShaderJobs = PendingShaderJobs(GShaderCompilingManager);
			LeftAssetBuilds = PendingAssetBuilds(AssetCompiling);
			if (LeftShaderJobs <= 0 && LeftAssetBuilds <= 0)
			{
				break;
			}
			if (FPlatformTime::Seconds() - StartSec >= MaxSec)
			{
				bTimedOut = true;
				break;
			}
			FPlatformProcess::Sleep(0.02f);
		}
	}
	const double PolledSec = FPlatformTime::Seconds() - StartSec;
	if (bTimedOut)
	{
		UE_LOG(LogAbyss, Warning, TEXT("Uncooked run: stopped waiting after %.1f s (abyss.WaitForCompilationMaxSec) with %d "
			"shader jobs and %d asset builds left. They finish in the background: zone dressing stays hidden until its "
			"shaders are ready (abyss.DecorShaderGate), other meshes draw with the engine default material meanwhile."),
			PolledSec, LeftShaderJobs, LeftAssetBuilds);
		return EAbyssCompileWait::TimedOut;
	}
	// What is left (nothing after a completed poll; everything without a limit or without counts) is finished and
	// applied now.
	AssetCompiling.FinishAllCompilation();
	if (GShaderCompilingManager != nullptr)
	{
		GShaderCompilingManager->FinishAllCompilation();
	}
	const double WaitedSec = FPlatformTime::Seconds() - StartSec;
	UE_CLOG(bZoneEntry || AssetBuilds > 0 || ShaderJobs > 0 || WaitedSec > 0.05, LogAbyss, Display,
		TEXT("Uncooked run: compilation finished in %.2f s"), WaitedSec);
	return EAbyssCompileWait::Finished;
#else
	(void)bZoneEntry;
	return EAbyssCompileWait::None;
#endif
}

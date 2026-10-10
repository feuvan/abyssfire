#include "World/AbyssZoneActor.h"

#include "Abyssfire.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "RHI.h"

#include <string>
#include <string_view>

#include "abyss/base/Enums.h"
#include "abyss/base/Json.h"
#include "abyss/data/DataStore.h"
#include "abyss/data/MapData.h"
#include "abyss/sim/Snapshot.h"
#include "abyss/world/Grid.h"
#include "abyss/world/MapGen.h"

#include "Framework/AbyssText.h"
#include "Framework/AbyssUnits.h"
#include "World/AbyssArtManifest.h"
#include "World/AbyssAssetLibrary.h"

static TAutoConsoleVariable<float> CVarAbyssSunScale(
	TEXT("abyss.SunScale"),
	0.55f,
	TEXT("Sun illuminance on the lit ground as a fraction of pi lux (ue58-platform.md 6.4 B'): lit ground = base x (N.L x this + ambient)."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarAbyssGroundAmbient(
	TEXT("abyss.GroundAmbient"),
	0.55f,
	TEXT("Ambient fill of the lit ground (MPC_AF_Lighting Ambient), so shadowed ground keeps the web's painted-shadow value."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarAbyssExposureBias(
	TEXT("abyss.ExposureBias"),
	0.263f,
	TEXT("Manual exposure bias (EV) so an unlit emissive 1.0 shows as display white: log2(1.2) for UE's EV100 calibration [Verify on 5.8.3]."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarAbyssBloom(
	TEXT("abyss.Bloom"),
	0.6f,
	TEXT("Bloom intensity of the zone post-process on balanced / high tiers (web bloom strength 0.8, threshold 1)."),
	ECVF_Default);

namespace AbyssZonePrivate
{
	constexpr uint8 TileWater = 3;
	constexpr uint8 TileWall = 4;
	constexpr uint8 TileCampWall = 6;
	constexpr int32 BucketTiles = 8;
	constexpr float DecorFadeTarget = 0.75f;   // alpha .25 (world-map-nav.md 15.6)
	constexpr float WallFadeTarget = 0.55f;    // alpha .45
	constexpr float FadeTimeConstantSec = 0.11f;
	constexpr float ShadowCasterMinHeightCm = 45.f;

	FName Id(const TCHAR* Format, const FString& A)
	{
		return FName(*FString::Printf(Format, *A));
	}

	FString ToF(std::string_view Text)
	{
		return AbyssText::ToFString(Text);
	}

	FLinearColor JsonColor(const abyss::JsonValue& Value, const FLinearColor& Fallback)
	{
		if (Value.IsNumber())
		{
			return AbyssWorldUtil::ColorFromRgb(static_cast<uint32>(Value.AsInt64()));
		}
		FLinearColor Parsed;
		if (Value.IsString() && AbyssWorldUtil::ParseHexColor(AbyssText::ToFString(Value.AsString()), Parsed))
		{
			return Parsed;
		}
		return Fallback;
	}

	FIntPoint BucketOf(const FVector& Location)
	{
		return FIntPoint(FMath::FloorToInt32(Location.X / (AbyssUnits::TileUU * BucketTiles)),
			FMath::FloorToInt32(Location.Y / (AbyssUnits::TileUU * BucketTiles)));
	}

	float HashUnit(uint32 Hash)
	{
		return static_cast<float>(Hash & 0xFFFFu) / 65535.f;
	}

	/** Weighted pick from a decor pool by a hash. */
	const std::string* PickWeighted(const std::vector<abyss::WeightedDecor>& Pool, uint32 Hash)
	{
		double Total = 0.0;
		for (const abyss::WeightedDecor& Entry : Pool)
		{
			Total += Entry.weight;
		}
		if (Pool.empty() || Total <= 0.0)
		{
			return nullptr;
		}
		double X = HashUnit(Hash) * Total;
		for (const abyss::WeightedDecor& Entry : Pool)
		{
			X -= Entry.weight;
			if (X <= 0.0)
			{
				return &Entry.type;
			}
		}
		return &Pool.back().type;
	}
}

AAbyssZoneActor::AAbyssZoneActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SetActorEnableCollision(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);

	TerrainMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Terrain"));
	TerrainMesh->SetupAttachment(SceneRoot);
	TerrainMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TerrainMesh->SetGenerateOverlapEvents(false);
	TerrainMesh->SetCanEverAffectNavigation(false);
	TerrainMesh->bUseAsyncCooking = false;
	TerrainMesh->SetCastShadow(false);   // the ground receives sun shadows; hills casting is not worth the cost

	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(SceneRoot);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetCastShadows(true);

	PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcess"));
	PostProcess->SetupAttachment(SceneRoot);
	PostProcess->bUnbound = true;
	PostProcess->Priority = 0.f;
	PostProcess->BlendWeight = 1.f;
}

// =====================================================================================================================
// Build / clear
// =====================================================================================================================

void AAbyssZoneActor::ClearZone()
{
	for (UInstancedStaticMeshComponent* Component : InstanceComponents)
	{
		if (Component != nullptr)
		{
			Component->DestroyComponent();
		}
	}
	InstanceComponents.Reset();
	ComponentByMesh.Reset();
	PendingInstances.Reset();
	PendingStories.Reset();
	Occluders.Reset();
	OccluderBuckets.Reset();
	StoryInstances.Reset();
	FocusedStory = NAME_None;
	EffectSpots.Reset();
	ExitTiles.Reset();
	if (TerrainMesh != nullptr)
	{
		TerrainMesh->ClearAllMeshSections();
	}
	TileTexture = nullptr;
	TerrainMaterial = nullptr;
	WaterMaterial = nullptr;
	GradeMaterial = nullptr;
	Terrain.Reset();
}

void AAbyssZoneActor::BuildZone(const abyss::Snapshot& Snap, const abyss::DataStore& Data, UAbyssAssetLibrary& Assets,
	const FAbyssQualityProfile& Quality)
{
	ClearZone();
	AssetLibrary = &Assets;
	QualityProfile = Quality;
	const FAbyssArtShading& Shading = Assets.GetManifest().GetShading();
	MeshYawOffsetDeg = Shading.MeshYawOffsetDeg;
	CameraYawDeg = Shading.CameraYawDeg;
	DirToLight = Shading.DirToLight;
	RimColor = Shading.RimColor;

	Theme = Snap.zone.theme;
	ThemeName = std::string(abyss::EnumName(Theme));
	WallPaintTile = Data.World().mapGen.Theme(Theme).primaryTile;

	BuildLighting(Snap, Data);
	BuildTerrain(Snap, Data);
	BuildWalls(Snap);
	BuildDecorations(Snap, Data);
	BuildCamps(Snap, Data);
	BuildMarkers(Snap, Data);
	BuildSkirt(Data);
	FlushInstances();
	ApplyQuality(Quality);
	UE_LOG(LogAbyss, Log, TEXT("Zone %s built: %d instanced meshes, %d occluders, %d effect spots"),
		*AbyssText::ToFString(Snap.zone.mapId), InstanceComponents.Num(), Occluders.Num(), EffectSpots.Num());
}

FVector AAbyssZoneActor::GroundPoint(double TileX, double TileY) const
{
	const double X = TileX * AbyssUnits::TileUU;
	const double Y = TileY * AbyssUnits::TileUU;
	return FVector(X, Y, Terrain.GetHeight(X, Y));
}

double AAbyssZoneActor::GetGroundHeight(double WorldX, double WorldY) const
{
	return Terrain.GetHeight(WorldX, WorldY);
}

bool AAbyssZoneActor::RaycastGround(const FVector& Origin, const FVector& Direction, FVector& OutHit) const
{
	return Terrain.Raycast(Origin, Direction, 1.0e6, OutHit);
}

// =====================================================================================================================
// Terrain
// =====================================================================================================================

void AAbyssZoneActor::BuildTerrain(const abyss::Snapshot& Snap, const abyss::DataStore& Data)
{
	const abyss::ZoneGrid* Grid = Snap.zone.grid;
	if (Grid == nullptr || Grid->Cols() <= 0 || Grid->Rows() <= 0)
	{
		UE_LOG(LogAbyss, Warning, TEXT("Zone has no grid: terrain skipped"));
		return;
	}
	FAbyssTerrainSettings Settings;
	if (QualityProfile.bMobile && QualityProfile.Tier == 0)
	{
		Settings.SkirtTiles = 14;
	}
	Terrain.Build(*Grid, Settings);

	// Tile-id texture (M_AF_Terrain: TileIds, point sampled, clamped).
	TArray<uint8> Pixels;
	Terrain.BuildTileTexture(Pixels, static_cast<uint8>(WallPaintTile));
	const int32 Cols = Terrain.GetCols();
	const int32 Rows = Terrain.GetRows();
	TileTexture = UTexture2D::CreateTransient(Cols, Rows, PF_B8G8R8A8, FName(TEXT("T_Zone_TileIds")));
	if (TileTexture != nullptr)
	{
		TileTexture->Filter = TF_Nearest;
		TileTexture->SRGB = false;
		TileTexture->CompressionSettings = TC_VectorDisplacementmap;
		TileTexture->AddressX = TA_Clamp;
		TileTexture->AddressY = TA_Clamp;
		TileTexture->NeverStream = true;
		TileTexture->UpdateResource();
		uint8* Copy = new uint8[Pixels.Num()];
		FMemory::Memcpy(Copy, Pixels.GetData(), Pixels.Num());
		FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, static_cast<uint32>(Cols), static_cast<uint32>(Rows));
		TileTexture->UpdateTextureRegions(0, 1, Region, static_cast<uint32>(Cols * 4), 4u, Copy,
			[](uint8* SrcData, const FUpdateTextureRegion2D* Regions)
			{
				delete[] SrcData;
				delete Regions;
			});
	}

	UAbyssAssetLibrary* Assets = AssetLibrary.Get();
	if (UMaterialInterface* Base = Assets != nullptr ? Assets->LoadMaterial(TEXT("M_AF_Terrain")) : nullptr)
	{
		TerrainMaterial = UMaterialInstanceDynamic::Create(Base, this);
	}
	if (UMaterialInterface* Base = Assets != nullptr ? Assets->LoadMaterial(TEXT("M_AF_Water")) : nullptr)
	{
		WaterMaterial = UMaterialInstanceDynamic::Create(Base, this);
	}
	ApplyTerrainStyle(Data);

	const TArray<FProcMeshTangent> NoTangents;
	int32 Section = 0;
	for (int32 ChunkY = 0; ChunkY < Terrain.GetNumChunksY(); ++ChunkY)
	{
		for (int32 ChunkX = 0; ChunkX < Terrain.GetNumChunksX(); ++ChunkX)
		{
			FAbyssTerrainMesh Ground;
			FAbyssTerrainMesh Water;
			Terrain.BuildChunkMeshes(ChunkX, ChunkY, Ground, Water);
			if (!Ground.IsEmpty())
			{
				TerrainMesh->CreateMeshSection_LinearColor(Section, Ground.Vertices, Ground.Triangles, Ground.Normals, Ground.UV0,
					Ground.Colors, NoTangents, /*bCreateCollision*/ false);
				if (TerrainMaterial != nullptr)
				{
					TerrainMesh->SetMaterial(Section, TerrainMaterial);
				}
				++Section;
			}
			if (!Water.IsEmpty())
			{
				TerrainMesh->CreateMeshSection_LinearColor(Section, Water.Vertices, Water.Triangles, Water.Normals, Water.UV0,
					Water.Colors, NoTangents, /*bCreateCollision*/ false);
				if (WaterMaterial != nullptr)
				{
					TerrainMesh->SetMaterial(Section, WaterMaterial);
				}
				++Section;
			}
		}
	}
}

void AAbyssZoneActor::ApplyTerrainStyle(const abyss::DataStore& Data)
{
	using namespace AbyssZonePrivate;
	if (TerrainMaterial != nullptr)
	{
		if (TileTexture != nullptr)
		{
			TerrainMaterial->SetTextureParameterValue(FName(TEXT("TileIds")), TileTexture);
		}
		TerrainMaterial->SetScalarParameterValue(FName(TEXT("MapCols")), static_cast<float>(Terrain.GetCols()));
		TerrainMaterial->SetScalarParameterValue(FName(TEXT("MapRows")), static_cast<float>(Terrain.GetRows()));
		TerrainMaterial->SetScalarParameterValue(FName(TEXT("WallPaintTile")), static_cast<float>(WallPaintTile));
	}
	if (WaterMaterial != nullptr && TileTexture != nullptr)
	{
		WaterMaterial->SetTextureParameterValue(FName(TEXT("TileIds")), TileTexture);
		WaterMaterial->SetScalarParameterValue(FName(TEXT("MapCols")), static_cast<float>(Terrain.GetCols()));
		WaterMaterial->SetScalarParameterValue(FName(TEXT("MapRows")), static_cast<float>(Terrain.GetRows()));
	}

	// terrain_styles.json (raw table): themes.<theme>.ground.<tile>{base, rank, lip, layers[], accents[], liquid{}}.
	const abyss::JsonValue* Styles = Data.RawTable("terrain_styles.json");
	if (Styles == nullptr)
	{
		return;
	}
	const abyss::JsonValue& ThemeStyle = Styles->Get("themes").Get(ThemeName);
	const abyss::JsonValue& Ground = ThemeStyle.Get("ground");
	for (const abyss::JsonMember& Member : Ground.Members())
	{
		const FString Tile = ToF(Member.key);
		const abyss::JsonValue& Style = Member.value;
		const FLinearColor Base = JsonColor(Style.Get("base"), FLinearColor(0.2f, 0.35f, 0.1f));
		FLinearColor PatchA = Base;
		FLinearColor PatchB = Base;
		const abyss::JsonValue& FirstLayer = Style.Get("layers").At(0);
		if (FirstLayer.Get("colors").IsArray())
		{
			PatchA = JsonColor(FirstLayer.Get("colors").At(0), Base);
			PatchB = JsonColor(FirstLayer.Get("colors").At(1), PatchA);
		}
		const bool bHasLip = Style.Has("lip");
		const FLinearColor Lip = bHasLip ? JsonColor(Style.Get("lip"), Base) : FMath::Lerp(Base, FLinearColor(1.f, 0.88f, 0.67f), 0.25f);
		const FLinearColor Accent = JsonColor(Style.Get("accents").At(0), Base);
		if (TerrainMaterial != nullptr)
		{
			TerrainMaterial->SetVectorParameterValue(Id(TEXT("Tile%sBase"), Tile), Base);
			TerrainMaterial->SetVectorParameterValue(Id(TEXT("Tile%sPatchA"), Tile), PatchA);
			TerrainMaterial->SetVectorParameterValue(Id(TEXT("Tile%sPatchB"), Tile), PatchB);
			TerrainMaterial->SetVectorParameterValue(Id(TEXT("Tile%sLip"), Tile), Lip);
			TerrainMaterial->SetVectorParameterValue(Id(TEXT("Tile%sAccent"), Tile), Accent);
			TerrainMaterial->SetScalarParameterValue(Id(TEXT("Tile%sRank"), Tile), static_cast<float>(Style.Get("rank").AsDouble(0.0)));
		}
		const abyss::JsonValue& Liquid = Style.Get("liquid");
		if (Liquid.IsObject())
		{
			const FLinearColor Shallow = JsonColor(Liquid.Get("shallow"), Base);
			const FLinearColor Foam = JsonColor(Liquid.Get("foam"), FLinearColor::White);
			const FLinearColor Bank = JsonColor(Liquid.Get("bank"), Base * 0.6f);
			FLinearColor Wave = FLinearColor::White;
			for (const abyss::JsonValue& Layer : Style.Get("layers").Items())
			{
				if (Layer.Get("kind").AsString() == std::string_view("wave"))
				{
					Wave = JsonColor(Layer.Get("color"), Wave);
				}
			}
			for (UMaterialInstanceDynamic* Material : { TerrainMaterial.Get(), WaterMaterial.Get() })
			{
				if (Material == nullptr)
				{
					continue;
				}
				Material->SetVectorParameterValue(FName(TEXT("WaterBase")), Base);
				Material->SetVectorParameterValue(FName(TEXT("WaterShallow")), Shallow);
				Material->SetVectorParameterValue(FName(TEXT("WaterFoam")), Foam);
				Material->SetVectorParameterValue(FName(TEXT("WaterBank")), Bank);
				Material->SetVectorParameterValue(FName(TEXT("WaterWave")), Wave);
			}
		}
	}
}

// =====================================================================================================================
// Instances
// =====================================================================================================================

const FAbyssArtAsset* AAbyssZoneActor::ResolveArt(TConstArrayView<FName> GameIds, uint32 Hash) const
{
	const UAbyssAssetLibrary* Assets = AssetLibrary.Get();
	return Assets != nullptr ? Assets->GetManifest().ResolveFirst(GameIds, Hash) : nullptr;
}

bool AAbyssZoneActor::PlaceArt(TConstArrayView<FName> GameIds, uint32 Hash, const FVector& Location, float YawDeg, float Scale,
	bool bOccluder, float FadeTarget, FName StoryId)
{
	const FAbyssArtAsset* Art = ResolveArt(GameIds, Hash);
	UAbyssAssetLibrary* Assets = AssetLibrary.Get();
	if (Art == nullptr || Assets == nullptr || Art->IsSkeletal())
	{
		return false;
	}
	UStaticMesh* Mesh = Assets->LoadStaticMesh(*Art);
	if (Mesh == nullptr)
	{
		return false;
	}
	const float FinalScale = FMath::Max(0.05f, Scale * Art->Scale);
	const FTransform Transform(FRotator(0.f, YawDeg, 0.f), Location, FVector(FinalScale));
	const FBox Bounds = Art->BoundsCm.IsValid ? Art->BoundsCm : Mesh->GetBoundingBox();
	const FVector Extent = Bounds.GetExtent() * FinalScale;
	const float Radius = static_cast<float>(FMath::Max(Extent.X, Extent.Y));
	const float Height = static_cast<float>(Bounds.Max.Z * FinalScale);
	return AddInstance(Mesh, Transform, AbyssZonePrivate::HashUnit(Hash >> 3), bOccluder, FadeTarget, FMath::Max(25.f, Radius),
		FMath::Max(30.f, Height), StoryId);
}

bool AAbyssZoneActor::AddInstance(UStaticMesh* Mesh, const FTransform& Transform, float RandomValue, bool bOccluder,
	float FadeTarget, float RadiusCm, float HeightCm, FName StoryId)
{
	using namespace AbyssZonePrivate;
	if (Mesh == nullptr)
	{
		return false;
	}
	FInstanceBatch& Batch = PendingInstances.FindOrAdd(Mesh);
	const int32 InstanceIndex = Batch.Transforms.Add(Transform);
	Batch.CustomData.Append({ 0.f, RandomValue, 0.f });
	int32 OccluderIndex = INDEX_NONE;
	if (bOccluder)
	{
		FOccluder& Occluder = Occluders.AddDefaulted_GetRef();
		Occluder.Instance = InstanceIndex;   // component resolved in FlushInstances
		Occluder.Location = Transform.GetLocation();
		Occluder.RadiusCm = RadiusCm;
		Occluder.HeightCm = HeightCm;
		Occluder.FadeTarget = FadeTarget;
		OccluderIndex = Occluders.Num() - 1;
		OccluderBuckets.FindOrAdd(BucketOf(Occluder.Location)).Add(OccluderIndex);
	}
	Batch.OccluderSlots.Add(OccluderIndex);
	if (!StoryId.IsNone())
	{
		// The component exists only after FlushInstances: remember the mesh, resolve there.
		PendingStories.Add(FPendingStory{ StoryId, Mesh, InstanceIndex });
	}
	return true;
}

void AAbyssZoneActor::FlushInstances()
{
	using namespace AbyssZonePrivate;
	for (TPair<TObjectPtr<UStaticMesh>, FInstanceBatch>& Pair : PendingInstances)
	{
		UStaticMesh* Mesh = Pair.Key.Get();
		FInstanceBatch& Batch = Pair.Value;
		if (Mesh == nullptr || Batch.Transforms.Num() == 0)
		{
			continue;
		}
		UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
		Component->SetStaticMesh(Mesh);
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetGenerateOverlapEvents(false);
		Component->SetCanEverAffectNavigation(false);
		Component->SetNumCustomDataFloats(AbyssDecorIcd::Count);
		const float MeshHeight = static_cast<float>(Mesh->GetBoundingBox().Max.Z);
		Component->SetCastShadow(MeshHeight >= ShadowCasterMinHeightCm);
		Component->SetupAttachment(SceneRoot);
		Component->RegisterComponent();
		Component->AddInstances(Batch.Transforms, /*bShouldReturnIndices*/ false, /*bWorldSpace*/ true);
		for (int32 Index = 0; Index < Batch.Transforms.Num(); ++Index)
		{
			Component->SetCustomData(Index,
				TArrayView<const float>(Batch.CustomData.GetData() + Index * AbyssDecorIcd::Count, AbyssDecorIcd::Count),
				/*bMarkRenderStateDirty*/ false);
		}
		Component->MarkRenderStateDirty();
		const int32 ComponentIndex = InstanceComponents.Add(Component);
		ComponentByMesh.Add(Mesh, ComponentIndex);
		for (int32 Index = 0; Index < Batch.OccluderSlots.Num(); ++Index)
		{
			if (Occluders.IsValidIndex(Batch.OccluderSlots[Index]))
			{
				Occluders[Batch.OccluderSlots[Index]].Component = ComponentIndex;
			}
		}
	}
	for (const FPendingStory& Story : PendingStories)
	{
		if (const int32* ComponentIndex = ComponentByMesh.Find(Story.Mesh))
		{
			StoryInstances.Add(Story.Id, TPair<int32, int32>(*ComponentIndex, Story.Instance));
		}
	}
	PendingStories.Reset();
	PendingInstances.Reset();
}

// =====================================================================================================================
// Walls, palisades, decorations, camps, markers, skirt
// =====================================================================================================================

void AAbyssZoneActor::BuildWalls(const abyss::Snapshot& Snap)
{
	using namespace AbyssZonePrivate;
	const abyss::ZoneGrid* Grid = Snap.zone.grid;
	if (Grid == nullptr)
	{
		return;
	}
	for (const abyss::WorldMarkerView& Marker : Snap.markers)
	{
		if (Marker.kind == abyss::MarkerKind::Exit)
		{
			ExitTiles.Add(FIntPoint(FMath::RoundToInt32(Marker.pos.x), FMath::RoundToInt32(Marker.pos.y)));
		}
	}
	auto NearExit = [this](int32 Col, int32 Row)
	{
		for (const FIntPoint& Exit : ExitTiles)
		{
			if (FMath::Abs(Exit.X - Col) <= 1 && FMath::Abs(Exit.Y - Row) <= 1)
			{
				return true;
			}
		}
		return false;
	};
	const FString ThemeText = ToF(ThemeName);
	const FName OutcropIds[] = { Id(TEXT("outcrop_%s"), ThemeText), Id(TEXT("wall_%s"), ThemeText), FName(TEXT("outcrop")),
		FName(TEXT("wall")) };
	const FName StakeIds[] = { Id(TEXT("palisade_%s"), ThemeText), FName(TEXT("palisade")), FName(TEXT("camp_wall")) };
	auto IsCampWall = [Grid](int32 Col, int32 Row)
	{
		return Grid->InBounds(Col, Row) && Grid->Tile(Col, Row) == abyss::TileType::CampWall;
	};

	for (int32 Row = 0; Row < Grid->Rows(); ++Row)
	{
		for (int32 Col = 0; Col < Grid->Cols(); ++Col)
		{
			const abyss::TileType Type = Grid->Tile(Col, Row);
			if (Type == abyss::TileType::Wall)
			{
				if (NearExit(Col, Row))
				{
					continue;   // the exit gate / portal stands there (world-map-nav.md 15.2)
				}
				// Variant v = tileHash(c, r, 3) % variants (the manifest sorts the variants by name: V0..V5).
				const uint32 Hash = AbyssWorldUtil::TileHash(Col, Row, 3);
				const float Yaw = static_cast<float>((Hash >> 8) % 360u);
				PlaceArt(OutcropIds, Hash, GroundPoint(Col, Row), Yaw, 1.f, /*bOccluder*/ true, WallFadeTarget);
			}
			else if (Type == abyss::TileType::CampWall)
			{
				// Centre stake + stakes at 0.2 and 0.4 of the line toward every camp-wall neighbour (OutcropPainter mask).
				const FIntPoint Neighbours[] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };
				TArray<FVector2D, TInlineAllocator<9>> Spots;
				Spots.Add(FVector2D(Col, Row));
				for (const FIntPoint& D : Neighbours)
				{
					if (IsCampWall(Col + D.X, Row + D.Y))
					{
						Spots.Add(FVector2D(Col + D.X * 0.2, Row + D.Y * 0.2));
						Spots.Add(FVector2D(Col + D.X * 0.4, Row + D.Y * 0.4));
					}
				}
				for (int32 Index = 0; Index < Spots.Num(); ++Index)
				{
					const uint32 Hash = AbyssWorldUtil::TileHash(Col * 5 + Index, Row, 17);
					const float Yaw = static_cast<float>(Hash % 360u);
					PlaceArt(StakeIds, Hash, GroundPoint(Spots[Index].X, Spots[Index].Y), Yaw, 1.f, /*bOccluder*/ true, WallFadeTarget);
				}
			}
			else if (Type == abyss::TileType::Water)
			{
				// Lily pads on ~1 in 6 water tiles (art-inventory-ch1.md 6.1), floating on the surface.
				const uint32 Hash = AbyssWorldUtil::TileHash(Col, Row, 13);
				if (Hash % 6u == 0u)
				{
					const FName PadIds[] = { Id(TEXT("lily_pad_%s"), ThemeText), FName(TEXT("decor_lily_pad")), FName(TEXT("lily_pad")) };
					FVector Location = AbyssUnits::TileToWorld(abyss::Vec2(Col + (HashUnit(Hash >> 4) - 0.5f) * 0.4f,
						Row + (HashUnit(Hash >> 9) - 0.5f) * 0.4f));
					Location.Z = Terrain.GetSettings().WaterSurfaceCm + 0.5f;
					PlaceArt(PadIds, Hash, Location, static_cast<float>(Hash % 360u), 0.9f + 0.2f * HashUnit(Hash >> 13), false, 0.f);
				}
			}
		}
	}

	// Gate posts at the 2-tile gate of every camp (optional asset, art-inventory-ch1.md 6.3).
	const FName GateIds[] = { Id(TEXT("palisade_gate_%s"), ThemeText), FName(TEXT("palisade_gate")) };
	if (ResolveArt(GateIds, 0) != nullptr)
	{
		for (const abyss::Vec2& Camp : Snap.zone.camps)
		{
			const double GateRow = Camp.y - 5.0;
			PlaceArt(GateIds, 1, GroundPoint(Camp.x - 1.5, GateRow), MeshYawOffsetDeg + 90.f, 1.f, true, WallFadeTarget);
			PlaceArt(GateIds, 2, GroundPoint(Camp.x + 0.5, GateRow), MeshYawOffsetDeg + 90.f, 1.f, true, WallFadeTarget);
		}
	}
}

void AAbyssZoneActor::BuildDecorations(const abyss::Snapshot& Snap, const abyss::DataStore& Data)
{
	using namespace AbyssZonePrivate;
	const abyss::ZoneGrid* Grid = Snap.zone.grid;
	if (Grid == nullptr)
	{
		return;
	}
	const abyss::MapThemeDef& ThemeDef = Data.World().mapGen.Theme(Theme);
	for (const abyss::Decoration& Decor : Grid->Decorations())
	{
		const FString Type = ToF(Decor.type);
		const FName Ids[] = { Id(TEXT("decor_%s"), Type), FName(*Type) };
		const int32 Col = FMath::RoundToInt32(Decor.col);
		const int32 Row = FMath::RoundToInt32(Decor.row);
		const uint32 Hash = AbyssWorldUtil::TileHash(Col, Row, 5);
		bool bTall = Decor.blocking;
		for (const std::string& Tall : ThemeDef.tall)
		{
			bTall = bTall || Tall == Decor.type;
		}
		PlaceArt(Ids, Hash, GroundPoint(Decor.col + Decor.offsetCol, Decor.row + Decor.offsetRow), static_cast<float>(Decor.yawDeg),
			static_cast<float>(Decor.scale), bTall, DecorFadeTarget);
	}
}

void AAbyssZoneActor::BuildCamps(const abyss::Snapshot& Snap, const abyss::DataStore& Data)
{
	using namespace AbyssZonePrivate;
	const abyss::MapDef* Map = Data.FindMap(Snap.zone.mapId);
	if (Map == nullptr)
	{
		return;
	}
	const float FaceCameraYaw = CameraYawDeg + 180.f;   // toward the viewer
	for (const abyss::CampProp& Prop : abyss::CampProps(*Map))
	{
		const FString Type = ToF(Prop.type);
		const FName Ids[] = { Id(TEXT("camp_%s"), Type), Id(TEXT("decor_camp_%s"), Type), FName(*Type) };
		const FVector Location = GroundPoint(Prop.pos.col, Prop.pos.row);
		float FacingYaw = FaceCameraYaw;
		if (Map->camps.size() > static_cast<size_t>(Prop.campIndex))
		{
			const abyss::TilePos Centre = Map->camps[static_cast<size_t>(Prop.campIndex)].pos;
			const FVector2D ToCentre(Centre.col - Prop.pos.col, Centre.row - Prop.pos.row);
			const bool bFacesCentre = Prop.type == "tent" || Prop.type == "well" || Prop.type == "barrel" || Prop.type == "crate";
			if (bFacesCentre && !ToCentre.IsNearlyZero())
			{
				FacingYaw = FMath::RadiansToDegrees(FMath::Atan2(ToCentre.Y, ToCentre.X));
			}
		}
		const bool bTall = Prop.type == "tent" || Prop.type == "well" || Prop.type == "banner";
		const uint32 Hash = AbyssWorldUtil::TileHash(Prop.pos.col, Prop.pos.row, 23);
		PlaceArt(Ids, Hash, Location, FacingYaw + MeshYawOffsetDeg, 1.f, bTall, DecorFadeTarget);
		if (Prop.type == "campfire")
		{
			EffectSpots.Add({ FName(TEXT("camp.campfire")), Location });
		}
		else if (Prop.type == "torch")
		{
			EffectSpots.Add({ FName(TEXT("camp.torch")), Location });
		}
	}
}

void AAbyssZoneActor::BuildMarkers(const abyss::Snapshot& Snap, const abyss::DataStore& Data)
{
	using namespace AbyssZonePrivate;
	const abyss::MapDef* Map = Data.FindMap(Snap.zone.mapId);
	const FVector2D MapCentre(Snap.zone.cols * 0.5, Snap.zone.rows * 0.5);
	for (const abyss::WorldMarkerView& Marker : Snap.markers)
	{
		if (Marker.id != abyss::kNoEntity)
		{
			continue;   // entity markers are prop actors (world builder)
		}
		const FVector Location = GroundPoint(Marker.pos.x, Marker.pos.y);
		if (Marker.kind == abyss::MarkerKind::Exit)
		{
			const FVector2D Inward = (MapCentre - FVector2D(Marker.pos.x, Marker.pos.y)).GetSafeNormal();
			const float FacingYaw = FMath::RadiansToDegrees(FMath::Atan2(Inward.Y, Inward.X));
			const FName SealedIds[] = { FName(TEXT("exit_gate_sealed")), FName(TEXT("exit_sealed")), FName(TEXT("exit_portal")) };
			const FName OpenIds[] = { FName(TEXT("exit_portal")) };
			if (Marker.sealed)
			{
				PlaceArt(SealedIds, 0, Location, FacingYaw + MeshYawOffsetDeg, 1.f, true, DecorFadeTarget);
			}
			else
			{
				PlaceArt(OpenIds, 0, Location, FacingYaw + MeshYawOffsetDeg, 1.f, true, DecorFadeTarget);
			}
			EffectSpots.Add({ Marker.sealed ? FName(TEXT("exit.sealed")) : FName(TEXT("exit.portal")), Location });
		}
		else if (Marker.kind == abyss::MarkerKind::StoryDecoration)
		{
			FString SpriteType;
			if (Map != nullptr)
			{
				for (const abyss::StoryDecorationDef& Story : Map->storyDecorations)
				{
					if (Story.id == Marker.key)
					{
						SpriteType = ToF(Story.spriteType);
					}
				}
			}
			const FString DecorId = ToF(Marker.key);
			const FName Ids[] = { Id(TEXT("decor_%s"), SpriteType), FName(*SpriteType), Id(TEXT("story_%s"), DecorId) };
			const uint32 Hash = AbyssWorldUtil::TileHash(FMath::RoundToInt32(Marker.pos.x), FMath::RoundToInt32(Marker.pos.y), 31);
			PlaceArt(Ids, Hash, Location, CameraYawDeg + 180.f + MeshYawOffsetDeg, 1.f, true, DecorFadeTarget, FName(*DecorId));
		}
	}
}

void AAbyssZoneActor::BuildSkirt(const abyss::DataStore& Data)
{
	using namespace AbyssZonePrivate;
	if (!Terrain.IsValid())
	{
		return;
	}
	// Rolling skirt dressing so the camera never shows bare hills (art-inventory-ch1.md 6.10): the theme's grove pool.
	const abyss::MapThemeDef& ThemeDef = Data.World().mapGen.Theme(Theme);
	const std::vector<abyss::WeightedDecor>& Grove = ThemeDef.grove;
	const int32 Skirt = Terrain.GetSettings().SkirtTiles;
	const int32 Cols = Terrain.GetCols();
	const int32 Rows = Terrain.GetRows();
	const uint32 Density = QualityProfile.bMobile && QualityProfile.Tier == 0 ? 11u : 6u;
	for (int32 Row = -Skirt; Row < Rows + Skirt; ++Row)
	{
		for (int32 Col = -Skirt; Col < Cols + Skirt; ++Col)
		{
			const int32 Outside = FMath::Max(FMath::Max(-Col, Col - (Cols - 1)), FMath::Max(-Row, Row - (Rows - 1)));
			if (Outside < 2)
			{
				continue;
			}
			const uint32 Hash = AbyssWorldUtil::TileHash(Col, Row, 29);
			if (Hash % Density != 0u)
			{
				continue;
			}
			const std::string* Type = PickWeighted(Grove, Hash >> 5);
			if (Type == nullptr)
			{
				continue;
			}
			const FString TypeText = ToF(*Type);
			const FName Ids[] = { Id(TEXT("decor_%s"), TypeText), FName(*TypeText) };
			const double Jx = (HashUnit(Hash >> 7) - 0.5) * 0.8;
			const double Jy = (HashUnit(Hash >> 11) - 0.5) * 0.8;
			PlaceArt(Ids, Hash, GroundPoint(Col + Jx, Row + Jy), static_cast<float>((Hash >> 3) % 360u),
				0.9f + 0.25f * HashUnit(Hash >> 15), /*bOccluder*/ Outside <= 4, DecorFadeTarget);
		}
	}
}

// =====================================================================================================================
// Lighting and look
// =====================================================================================================================

void AAbyssZoneActor::BuildLighting(const abyss::Snapshot& Snap, const abyss::DataStore& Data)
{
	using namespace AbyssZonePrivate;
	const abyss::ZoneMoodTables& Moods = Data.World().moods;
	abyss::MapTheme MoodTheme = Theme;
	Moods.ThemeFor(Snap.zone.mapId, MoodTheme);
	const abyss::ZoneMoodDef& MoodDef = Moods.moods[static_cast<size_t>(MoodTheme)];
	Mood.Ambient = AbyssWorldUtil::ColorFromRgb(MoodDef.ambient);
	Mood.AmbientAlpha = static_cast<float>(MoodDef.ambientAlpha);
	Mood.Vignette = AbyssWorldUtil::ColorFromRgb(MoodDef.vignette);
	Mood.VignetteAlpha = static_cast<float>(MoodDef.vignetteAlpha);
	Mood.Haze = AbyssWorldUtil::ColorFromRgb(MoodDef.haze);
	Mood.HazeAlpha = static_cast<float>(MoodDef.hazeAlpha);
	Mood.Saturation = static_cast<float>(MoodDef.saturation);
	Mood.Contrast = static_cast<float>(MoodDef.contrast);
	Mood.Lift = FVector(MoodDef.lift[0], MoodDef.lift[1], MoodDef.lift[2]);
	Mood.Gain = FVector(MoodDef.gain[0], MoodDef.gain[1], MoodDef.gain[2]);

	// Weather / ambience (world-map-nav.md 14.2): unknown zone ids use an entry of a zone with the same theme.
	AmbienceRecipe = NAME_None;
	const abyss::ZoneWeatherDef* Weather = Moods.WeatherFor(Snap.zone.mapId);
	if (Weather == nullptr)
	{
		for (const std::pair<std::string, abyss::MapTheme>& Entry : Moods.themeByZone)
		{
			if (Entry.second == MoodTheme)
			{
				Weather = Moods.WeatherFor(Entry.first);
				if (Weather != nullptr)
				{
					break;
				}
			}
		}
	}
	if (Weather != nullptr && !Weather->ambience.empty())
	{
		AmbienceRecipe = FName(*(FString(TEXT("ambient.")) + ToF(Weather->ambience)));
	}

	// Sun: the manifest key light (the toon materials and the ground must agree), warm mood tint; pi lux = unlit 1.0.
	SunColor = FMath::Lerp(FLinearColor::White, Mood.Ambient, 0.5f);
	SunColor.A = 1.f;
	const float Fill = CVarAbyssGroundAmbient.GetValueOnGameThread();
	AmbientFill = FLinearColor(Fill * 0.92f, Fill * 0.95f, Fill * 1.08f, 1.f);
	if (Sun != nullptr)
	{
		Sun->SetWorldRotation((-DirToLight).Rotation());
		Sun->SetIntensity(UE_PI * CVarAbyssSunScale.GetValueOnGameThread());
		Sun->SetLightColor(SunColor, /*bSRGB*/ false);
	}

	// Optional exact-grade post-process material (world-map-nav.md 14.1 formula).
	GradeMaterial = nullptr;
	if (UAbyssAssetLibrary* Assets = AssetLibrary.Get())
	{
		static const TCHAR* const Folders[] = { TEXT("Materials"), TEXT("Materials/PostProcess") };
		if (UMaterialInterface* Grade = Cast<UMaterialInterface>(
				Assets->LoadFromFolders(Folders, FName(TEXT("M_AF_PP_Grade")), UMaterialInterface::StaticClass())))
		{
			GradeMaterial = UMaterialInstanceDynamic::Create(Grade, this);
		}
	}
}

void AAbyssZoneActor::ApplyQuality(const FAbyssQualityProfile& Quality)
{
	QualityProfile = Quality;
	if (Sun != nullptr)
	{
		// P10: CSM 2 cascades desktop (3 high, 1 low), 1 cascade high-tier mobile; blob shadows elsewhere.
		const bool bShadows = !Quality.bMobile || Quality.Tier >= 1;
		int32 Cascades = 2;
		if (Quality.bMobile)
		{
			Cascades = Quality.Tier >= 2 ? 2 : 1;
		}
		else
		{
			Cascades = Quality.Tier <= 0 ? 1 : (Quality.Tier >= 2 ? 3 : 2);
		}
		Sun->SetCastShadows(bShadows);
		Sun->SetDynamicShadowCascades(Cascades);
		Sun->SetDynamicShadowDistanceMovableLight(3500.f);
	}
	ApplyPostProcess();
}

void AAbyssZoneActor::ApplyPostProcess()
{
	if (PostProcess == nullptr)
	{
		return;
	}
	FPostProcessSettings& S = PostProcess->Settings;
	S = FPostProcessSettings();

	// Stable toon colours (ue58-platform.md 6.7).
	S.bOverride_AutoExposureMethod = true;
	S.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	S.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	S.AutoExposureApplyPhysicalCameraExposure = false;
	S.bOverride_AutoExposureBias = true;
	S.AutoExposureBias = CVarAbyssExposureBias.GetValueOnGameThread();
	S.bOverride_ToneCurveAmount = true;
	S.ToneCurveAmount = 0.f;
	S.bOverride_BlueCorrection = true;
	S.BlueCorrection = 0.f;
	S.bOverride_ExpandGamut = true;
	S.ExpandGamut = 0.f;
	S.bOverride_MotionBlurAmount = true;
	S.MotionBlurAmount = 0.f;
	S.bOverride_SceneFringeIntensity = true;
	S.SceneFringeIntensity = 0.f;
	S.bOverride_GrainIntensity = true;
	S.GrainIntensity = 0.f;
	S.bOverride_AmbientOcclusionIntensity = true;
	S.AmbientOcclusionIntensity = 0.f;

	// Bloom (balanced / high only).
	S.bOverride_BloomIntensity = true;
	S.BloomIntensity = QualityProfile.bBloom ? CVarAbyssBloom.GetValueOnGameThread() : 0.f;
	S.bOverride_BloomThreshold = true;
	S.BloomThreshold = 1.f;

	// Mood vignette (dark mood colour at the edges).
	S.bOverride_VignetteIntensity = true;
	S.VignetteIntensity = QualityProfile.bColorGrading ? Mood.VignetteAlpha * 0.9f : 0.f;

	S.WeightedBlendables.Array.Reset();
	if (QualityProfile.bColorGrading)
	{
		if (GradeMaterial != nullptr)
		{
			// Exact web grade: the material reads these (WorldContract.md 3.6).
			GradeMaterial->SetVectorParameterValue(FName(TEXT("Ambient")), Mood.Ambient);
			GradeMaterial->SetScalarParameterValue(FName(TEXT("AmbientAlpha")), Mood.AmbientAlpha);
			GradeMaterial->SetVectorParameterValue(FName(TEXT("Haze")), Mood.Haze);
			GradeMaterial->SetScalarParameterValue(FName(TEXT("HazeAlpha")), Mood.HazeAlpha);
			GradeMaterial->SetScalarParameterValue(FName(TEXT("Saturation")), Mood.Saturation);
			GradeMaterial->SetScalarParameterValue(FName(TEXT("Contrast")), Mood.Contrast);
			GradeMaterial->SetVectorParameterValue(FName(TEXT("Lift")), FLinearColor(Mood.Lift.X, Mood.Lift.Y, Mood.Lift.Z));
			GradeMaterial->SetVectorParameterValue(FName(TEXT("Gain")), FLinearColor(Mood.Gain.X, Mood.Gain.Y, Mood.Gain.Z));
			S.WeightedBlendables.Array.Add(FWeightedBlendable(1.f, GradeMaterial));
		}
		else
		{
			// Built-in approximation: ambient multiply -> gain, haze -> global offset, sat / contrast, lift -> shadows offset,
			// gain -> highlights offset.
			const FLinearColor AmbientMul = FMath::Lerp(FLinearColor::White, Mood.Ambient, Mood.AmbientAlpha);
			S.bOverride_ColorGain = true;
			S.ColorGain = FVector4(AmbientMul.R, AmbientMul.G, AmbientMul.B, 1.0);
			S.bOverride_ColorOffset = true;
			S.ColorOffset = FVector4(Mood.Haze.R * Mood.HazeAlpha * 0.5f, Mood.Haze.G * Mood.HazeAlpha * 0.5f,
				Mood.Haze.B * Mood.HazeAlpha * 0.5f, 0.0);
			S.bOverride_ColorSaturation = true;
			S.ColorSaturation = FVector4(1.0, 1.0, 1.0, Mood.Saturation);
			S.bOverride_ColorContrast = true;
			S.ColorContrast = FVector4(1.0, 1.0, 1.0, Mood.Contrast);
			S.bOverride_ColorOffsetShadows = true;
			S.ColorOffsetShadows = FVector4(Mood.Lift.X, Mood.Lift.Y, Mood.Lift.Z, 0.0);
			S.bOverride_ColorOffsetHighlights = true;
			S.ColorOffsetHighlights = FVector4(Mood.Gain.X, Mood.Gain.Y, Mood.Gain.Z, 0.0);
		}
	}
	PostProcess->MarkRenderStateDirty();
}

// =====================================================================================================================
// Occlusion fade and story focus
// =====================================================================================================================

void AAbyssZoneActor::UpdateOcclusion(TConstArrayView<FVector> Targets, const FVector& CameraLocation, float DeltaSec)
{
	using namespace AbyssZonePrivate;
	if (Occluders.Num() == 0)
	{
		return;
	}
	TSet<int32> Covering;
	for (const FVector& Target : Targets)
	{
		const FVector ToCamera = CameraLocation - Target;
		const FVector2D Flat(ToCamera.X, ToCamera.Y);
		const double FlatLength = Flat.Size();
		if (FlatLength < 1.0)
		{
			continue;
		}
		const FVector2D Dir = Flat / FlatLength;
		const double TanElevation = ToCamera.Z / FlatLength;
		// Buckets around the target, extended toward the camera (tall props up to ~10 m in front can cover it).
		const FIntPoint Centre = BucketOf(Target);
		for (int32 By = Centre.Y - 2; By <= Centre.Y + 2; ++By)
		{
			for (int32 Bx = Centre.X - 2; Bx <= Centre.X + 2; ++Bx)
			{
				const TArray<int32>* Bucket = OccluderBuckets.Find(FIntPoint(Bx, By));
				if (Bucket == nullptr)
				{
					continue;
				}
				for (const int32 Index : *Bucket)
				{
					const FOccluder& Occluder = Occluders[Index];
					const FVector2D Rel(Occluder.Location.X - Target.X, Occluder.Location.Y - Target.Y);
					const double Along = FVector2D::DotProduct(Rel, Dir);
					if (Along < -Occluder.RadiusCm || Along > 1200.0)
					{
						continue;
					}
					const double Lateral = FMath::Abs(FVector2D::CrossProduct(Dir, Rel));
					if (Lateral > Occluder.RadiusCm + 25.0)
					{
						continue;
					}
					const double RayZ = Target.Z + FMath::Max(0.0, Along - Occluder.RadiusCm * 0.5) * TanElevation;
					if (RayZ < Occluder.Location.Z + Occluder.HeightCm)
					{
						Covering.Add(Index);
					}
				}
			}
		}
	}

	TSet<int32> Dirty;
	const float Blend = FMath::Min(1.f, DeltaSec / FadeTimeConstantSec);
	for (int32 Index = 0; Index < Occluders.Num(); ++Index)
	{
		FOccluder& Occluder = Occluders[Index];
		const float Target = Covering.Contains(Index) ? Occluder.FadeTarget : 0.f;
		if (Occluder.Fade == Target)
		{
			continue;
		}
		float Fade = Occluder.Fade + (Target - Occluder.Fade) * Blend;
		if (FMath::Abs(Fade - Target) < 0.02f)
		{
			Fade = Target;
		}
		Occluder.Fade = Fade;
		if (InstanceComponents.IsValidIndex(Occluder.Component) && InstanceComponents[Occluder.Component] != nullptr)
		{
			InstanceComponents[Occluder.Component]->SetCustomDataValue(Occluder.Instance, AbyssDecorIcd::Fade, Fade, false);
			Dirty.Add(Occluder.Component);
		}
	}
	for (const int32 ComponentIndex : Dirty)
	{
		InstanceComponents[ComponentIndex]->MarkRenderStateDirty();
	}
}

void AAbyssZoneActor::SetStoryFocus(FName DecorId)
{
	auto Apply = [this](FName Story, float Value)
	{
		const TPair<int32, int32>* Found = StoryInstances.Find(Story);
		if (Found == nullptr || !InstanceComponents.IsValidIndex(Found->Key) || InstanceComponents[Found->Key] == nullptr)
		{
			return;
		}
		InstanceComponents[Found->Key]->SetCustomDataValue(Found->Value, AbyssDecorIcd::Highlight, Value, true);
	};
	if (FocusedStory == DecorId)
	{
		return;
	}
	if (!FocusedStory.IsNone())
	{
		Apply(FocusedStory, 0.f);
	}
	FocusedStory = DecorId;
	if (!DecorId.IsNone())
	{
		Apply(DecorId, 1.f);
	}
}

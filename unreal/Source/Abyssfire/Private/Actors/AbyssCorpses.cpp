#include "Actors/AbyssCorpses.h"

#include "Abyssfire.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/Math.h"
#include "abyss/data/DataStore.h"
#include "abyss/data/MonsterData.h"

#include "Actors/AbyssCharacterActor.h"
#include "Actors/AbyssCorpseRules.h"
#include "Actors/AbyssMonsterLookRules.h"
#include "Framework/AbyssGameInstance.h"
#include "Framework/AbyssSimDriver.h"
#include "Framework/AbyssText.h"
#include "Framework/AbyssUnits.h"
#include "Framework/AbyssWorldView.h"
#include "Vfx/AbyssVfxSystem.h"
#include "World/AbyssArtManifest.h"
#include "World/AbyssAssetLibrary.h"
#include "World/AbyssWorldBuilder.h"

namespace AbyssCorpsesPrivate
{
	FName ToName(std::string_view Id)
	{
		return Id.empty() ? FName(NAME_None) : FName(*AbyssText::ToFString(Id));
	}

	/** An actor's facing on the ground (unit XY), world +X without one. */
	FVector2D FacingOf(const AActor* Actor)
	{
		if (Actor == nullptr)
		{
			return FVector2D(1.0, 0.0);
		}
		const FVector Forward = Actor->GetActorForwardVector();
		const FVector2D Flat(Forward.X, Forward.Y);
		return Flat.IsNearlyZero() ? FVector2D(1.0, 0.0) : Flat.GetSafeNormal();
	}

	FVector2D FlatDirection(const FVector& From, const FVector& To, const FVector2D& Fallback)
	{
		const FVector2D Delta(To.X - From.X, To.Y - From.Y);
		return Delta.IsNearlyZero() ? Fallback : Delta.GetSafeNormal();
	}

	/** A body without a facing of its own (a map seed, a late view) lies at a yaw spread by its id (stable per entry). */
	float SeedYawDeg(uint32 CorpseId)
	{
		return static_cast<float>(((CorpseId * 2654435761u) >> 16) % 360u);
	}

	/** A husk's recipe radius when its corpse block gives none (cm at scale 1). */
	constexpr double DefaultHuskRadiusCm = 60.0;

	/** A body's drawn scale (1 without an actor). */
	double BodyScaleOf(const AAbyssCharacterActor* Actor)
	{
		return Actor != nullptr ? static_cast<double>(Actor->GetBodyScale()) : 1.0;
	}
}

/** Everything the layer tracks (std containers, weak actor pointers; recipes by handle). */
struct FAbyssCorpseState
{
	struct FEntry
	{
		uint32 Id = 0;
		abyss::EntityId Monster = abyss::kNoEntity;
		std::string DefId;
		std::string ArtId;
		abyss::Vec2 Pos;
		double DeathMs = 0.0;
		bool bSeeded = false;
		AbyssCorpseRules::FCorpseBlock Block;
		AbyssCorpseRules::FHuskState Husk;
		TWeakObjectPtr<AAbyssCharacterActor> Actor;
		/** The core removed the entry: only its end's look is left. */
		bool bCoreRemoved = false;
		/** The cap ended it early (presentation only: the entry is still in the registry). */
		bool bRetired = false;
		bool bCrumblePlayed = false;
		/** Its story beat ended since it appeared (a later busy director no longer holds it). */
		bool bBeatEndedSinceAdded = false;
		/** corpse.burn waits for the end of the dispatch (the fire kill's own hit names its colour). */
		bool bBurnPending = false;
		/** EvCorpseRemoved::burner: the fire's source (its side picks ember or violet). */
		abyss::EntityId Burner = abyss::kNoEntity;
	};

	/** A body that stood up this dispatch: the ash its risen monster starts from (EvMonsterRaised follows). */
	struct FRise
	{
		uint32 CorpseId = 0;
		AbyssCorpseRules::FRiseStart Start;
		std::string RaiseRecipe;
		double RiseLengthMs = 0.0;
	};

	struct FConsume
	{
		abyss::EntityId Eater = abyss::kNoEntity;
		uint32 Corpse = 0;
		int32 Handle = 0;
	};

	/** A fire killing blow of this dispatch: the impact colour of a hero-side kill burn. */
	struct FFireKill
	{
		abyss::EntityId Target = abyss::kNoEntity;
		uint32 Rgb = 0;
	};

	/** A living monster's loops (aura, affix, phase look); 0 = none / not playing. */
	struct FLoops
	{
		int32 Aura = 0;
		bool bAuraTried = false;
		int32 Body = 0;
		bool bBodyTried = false;
		std::vector<int32> Affix;
		bool bAffixTried = false;
		int32 Phase = 0;
		bool bSeen = false;
		/** The facing last pushed to the handles (zero: push on the next update). */
		FVector2D LastBlow = FVector2D::ZeroVector;
	};

	AbyssCorpseRules::FAshContract Contract;
	bool bContractLoaded = false;
	/** -1 unknown, 0 the ash twins are missing, 1 present. */
	int32 AshState = -1;
	std::vector<FEntry> Entries;
	std::vector<FRise> Rises;
	std::vector<FConsume> Consumes;
	std::vector<FFireKill> FireKills;
	std::unordered_map<abyss::EntityId, FLoops> Loops;
	/** Corpse blocks by art asset name (parsed once per contract). */
	std::unordered_map<std::string, AbyssCorpseRules::FCorpseBlock> Blocks;
	std::string PlayingBeat;

	// per-frame scratch (cleared, never shrunk)
	std::vector<uint32> Known;
	std::vector<uint32> Present;
	AbyssCorpseRules::FReconcile Reconcile;
	std::vector<AbyssCorpseRules::FHuskEntry> CapEntries;
	std::vector<AbyssCorpseRules::FHuskEntry> CapScratch;
	std::vector<uint32> Overflow;

	int32 IndexOf(uint32 Id) const
	{
		for (size_t I = 0; I < Entries.size(); ++I)
		{
			if (Entries[I].Id == Id)
			{
				return static_cast<int32>(I);
			}
		}
		return INDEX_NONE;
	}

	const AbyssCorpseRules::FCorpseBlock& BlockFor(const FAbyssArtManifest& Manifest, FName ArtName)
	{
		const std::string Key = AbyssText::ToStd(ArtName.ToString());
		auto It = Blocks.find(Key);
		if (It == Blocks.end())
		{
			It = Blocks.emplace(Key, AbyssCorpseRules::ParseCorpseBlock(Manifest.GetAssetJson(ArtName), Contract)).first;
		}
		return It->second;
	}
};

// =====================================================================================================================
// Lifecycle
// =====================================================================================================================

UAbyssCorpseLayer* UAbyssCorpseLayer::Get(const UObject* WorldContextObject)
{
	const UWorld* OwningWorld = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return OwningWorld ? OwningWorld->GetSubsystem<UAbyssCorpseLayer>() : nullptr;
}

bool UAbyssCorpseLayer::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UAbyssCorpseLayer::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UAbyssVfxSystem>();
	State = MakeShared<FAbyssCorpseState>();
	BindEvents();
}

void UAbyssCorpseLayer::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	BindEvents();
}

void UAbyssCorpseLayer::Deinitialize()
{
	UnbindEvents();
	ClearAll();
	State.Reset();
	Super::Deinitialize();
}

void UAbyssCorpseLayer::BindEvents()
{
	if (bEventsBound)
	{
		return;
	}
	const UWorld* OwningWorld = GetWorld();
	UAbyssGameInstance* GameInstance = OwningWorld ? Cast<UAbyssGameInstance>(OwningWorld->GetGameInstance()) : nullptr;
	if (GameInstance == nullptr)
	{
		return;
	}
	FAbyssEventRouter& Router = GameInstance->GetEventRouter();
	Router.On<abyss::EvCorpseAdded>().AddUObject(this, &UAbyssCorpseLayer::HandleCorpseAdded);
	Router.On<abyss::EvCorpseRemoved>().AddUObject(this, &UAbyssCorpseLayer::HandleCorpseRemoved);
	Router.On<abyss::EvMonsterRaised>().AddUObject(this, &UAbyssCorpseLayer::HandleMonsterRaised);
	Router.On<abyss::EvMonsterFeed>().AddUObject(this, &UAbyssCorpseLayer::HandleMonsterFeed);
	Router.On<abyss::EvPlayAnim>().AddUObject(this, &UAbyssCorpseLayer::HandlePlayAnim);
	Router.On<abyss::EvMonsterAttackCancelled>().AddUObject(this, &UAbyssCorpseLayer::HandleAttackCancelled);
	Router.On<abyss::EvMonsterPhase>().AddUObject(this, &UAbyssCorpseLayer::HandleMonsterPhase);
	Router.On<abyss::EvMonsterBuffCast>().AddUObject(this, &UAbyssCorpseLayer::HandleBuffCast);
	Router.On<abyss::EvBlock>().AddUObject(this, &UAbyssCorpseLayer::HandleBlock);
	Router.On<abyss::EvHit>().AddUObject(this, &UAbyssCorpseLayer::HandleHit);
	Router.On<abyss::EvStoryBeat>().AddUObject(this, &UAbyssCorpseLayer::HandleStoryBeat);
	Router.OnSessionEnded.AddUObject(this, &UAbyssCorpseLayer::ClearAll);
	bEventsBound = true;
}

void UAbyssCorpseLayer::UnbindEvents()
{
	if (!bEventsBound)
	{
		return;
	}
	const UWorld* OwningWorld = GetWorld();
	if (UAbyssGameInstance* GameInstance = OwningWorld ? Cast<UAbyssGameInstance>(OwningWorld->GetGameInstance()) : nullptr)
	{
		GameInstance->GetEventRouter().RemoveAll(this);
	}
	bEventsBound = false;
}

void UAbyssCorpseLayer::ApplyQuality(const FAbyssQualityProfile& Profile)
{
	HuskCap = AbyssCorpseRules::HuskCap(Profile.Tier, Profile.bMobile);
	// 2G review X4: husks cast the sun's CSM on desktop only (the world builder's quality refresh reaches the living).
	bHuskSunShadow = AbyssCorpseRules::HuskCastsSunShadow(Profile.bMobile);
	if (State.IsValid())
	{
		for (FAbyssCorpseState::FEntry& Entry : State->Entries)
		{
			if (AAbyssCharacterActor* Actor = Entry.Actor.Get())
			{
				Actor->SetShadowMode(/*bBlob*/ true, /*bCastDynamic*/ bHuskSunShadow);
			}
		}
	}
}

void UAbyssCorpseLayer::WarmAshPipelines(const abyss::Snapshot& Snap)
{
	// 2G review X5: one hidden component per distinct body / held-item mesh of the zone's corpse-holding monsters, its
	// toon / ink slots on the ash twins, registered now (behind the zone fade) so the twins' pipeline states precache
	// before the first kill greys - UAbyssTelegraphSystem registers one hidden quad for M_AF_Telegraph the same way. The
	// map's seeded husks put the twins on their own bodies at the zone entry already.
	UWorld* OwningWorld = GetWorld();
	UAbyssWorldBuilder* Builder = UAbyssWorldBuilder::Get(this);
	UAbyssAssetLibrary* Assets = UAbyssAssetLibrary::Get(this);
	if (OwningWorld == nullptr || Builder == nullptr || Assets == nullptr || !HasAshMaterials())
	{
		return;
	}
	UMaterialInterface* ToonAsh = Assets->LoadMaterial(TEXT("M_AF_Toon_Ash"));
	UMaterialInterface* InkAsh = Assets->LoadMaterial(TEXT("M_AF_Outline_Ash"));
	if (ToonAsh == nullptr || InkAsh == nullptr)
	{
		return;
	}
	if (WarmActor != nullptr)
	{
		WarmActor->Destroy();   // a rebuild without a clear: the copies are made afresh for this zone's bodies
		WarmActor = nullptr;
	}
	WarmComponents.Reset();
	{
		FActorSpawnParameters Params;
		Params.Name = MakeUniqueObjectName(OwningWorld->PersistentLevel.Get(), AActor::StaticClass(), FName(TEXT("AbyssAshWarm")));
		Params.ObjectFlags |= RF_Transient;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		WarmActor = OwningWorld->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
		if (WarmActor == nullptr)
		{
			return;
		}
		USceneComponent* Root = NewObject<USceneComponent>(WarmActor, TEXT("Root"));
		Root->SetMobility(EComponentMobility::Movable);
		WarmActor->SetRootComponent(Root);
		Root->RegisterComponent();
	}
	TSet<const UObject*> Seen;
	TArray<UPrimitiveComponent*> Added;
	for (const abyss::MonsterView& Monster : Snap.monsters)
	{
		const AAbyssCharacterActor* Actor = Builder->FindCharacter(Monster.id);
		if (Actor != nullptr && Actor->WillHoldCorpsePose())
		{
			Actor->AddAshWarmComponents(*WarmActor, ToonAsh, InkAsh, Seen, Added);
		}
	}
	for (UPrimitiveComponent* Component : Added)
	{
		WarmComponents.Add(Component);
	}
	UE_LOG(LogAbyss, Log, TEXT("Corpses: %d ash pipeline warm-up components (%d meshes) registered for the zone"), Added.Num(),
		Seen.Num());
}

void UAbyssCorpseLayer::ClearAll()
{
	// The zone's pipeline warm-up copies go with it (WarmAshPipelines registers the next zone's).
	if (WarmActor != nullptr)
	{
		WarmActor->Destroy();
		WarmActor = nullptr;
	}
	WarmComponents.Reset();
	// The VFX system clears its own emitters with the zone; the handles held here are simply forgotten.
	if (!State.IsValid())
	{
		return;
	}
	for (FAbyssCorpseState::FEntry& Entry : State->Entries)
	{
		if (AAbyssCharacterActor* Actor = Entry.Actor.Get())
		{
			Actor->Destroy();
		}
	}
	State->Entries.clear();
	State->Rises.clear();
	State->Consumes.clear();
	State->FireKills.clear();
	State->Loops.clear();
	State->PlayingBeat.clear();
}

int32 UAbyssCorpseLayer::GetHuskCount() const
{
	int32 Count = 0;
	if (State.IsValid())
	{
		for (const FAbyssCorpseState::FEntry& Entry : State->Entries)
		{
			Count += Entry.Actor.IsValid() ? 1 : 0;
		}
	}
	return Count;
}

// =====================================================================================================================
// Contracts and helpers
// =====================================================================================================================

void UAbyssCorpseLayer::EnsureContract()
{
	if (!State.IsValid() || State->bContractLoaded)
	{
		return;
	}
	const UAbyssAssetLibrary* Assets = UAbyssAssetLibrary::Get(this);
	if (Assets == nullptr || !Assets->HasManifest())
	{
		return;   // the built-in copy until the manifest is there
	}
	State->Contract = AbyssCorpseRules::ParseAshContract(Assets->GetManifest().GetMaterialsJson());
	State->bContractLoaded = true;
	State->Blocks.clear();   // their timelines default to the contract's
	UE_CLOG(!State->Contract.bFromManifest, LogAbyss, Warning,
		TEXT("Corpses: the art manifest has no materials.M_AF_AshDissolve; using the built-in ash contract"));
}

bool UAbyssCorpseLayer::HasAshMaterials()
{
	if (!State.IsValid())
	{
		return false;
	}
	if (State->AshState >= 0)
	{
		return State->AshState == 1;
	}
	UAbyssAssetLibrary* Assets = UAbyssAssetLibrary::Get(this);
	if (Assets == nullptr || !Assets->HasManifest())
	{
		return false;   // not known yet
	}
	const bool bPresent = Assets->LoadMaterial(TEXT("M_AF_Toon_Ash")) != nullptr && Assets->LoadMaterial(TEXT("M_AF_Outline_Ash")) != nullptr;
	State->AshState = bPresent ? 1 : 0;
	UE_CLOG(!bPresent, LogAbyss, Warning,
		TEXT("Corpses: M_AF_Toon_Ash / M_AF_Outline_Ash missing (run Scripts/build_content.py): deaths fade as before, no husks"));
	return bPresent;
}

bool UAbyssCorpseLayer::CorpseHoldFor(const std::string& DefId, FName ArtName, float& OutHoldFromMs, float& OutHuskTopCm)
{
	OutHoldFromMs = 0.f;
	OutHuskTopCm = 0.f;
	if (!State.IsValid() || ArtName.IsNone())
	{
		return false;
	}
	EnsureContract();
	const UAbyssGameInstance* GameInstance = UAbyssGameInstance::Get(this);
	const abyss::DataStore* Data = GameInstance != nullptr ? GameInstance->GetData() : nullptr;
	const abyss::MonsterDef* Def = Data != nullptr ? AbyssMonsterLookRules::FindMonsterDef(*Data, DefId) : nullptr;
	const UAbyssAssetLibrary* Assets = UAbyssAssetLibrary::Get(this);
	if (Def == nullptr || Assets == nullptr || !Assets->HasManifest() || Def->corpse.leaves == abyss::CorpseLeaves::None)
	{
		return false;
	}
	const AbyssCorpseRules::FCorpseBlock& Block = State->BlockFor(Assets->GetManifest(), ArtName);
	if (!AbyssCorpseRules::HoldsCorpsePose(Def->corpse.leaves, Block, HasAshMaterials()))
	{
		return false;
	}
	OutHoldFromMs = static_cast<float>(Block.HoldFromMs);
	OutHuskTopCm = static_cast<float>(Block.HuskTopCm);
	return true;
}

const abyss::Snapshot* UAbyssCorpseLayer::CurrentSnapshot() const
{
	// The driver's index during a frame's dispatch; the sim's own view for the session's first batch (a new game's seeds
	// arrive before the index exists).
	const UAbyssSimDriver* Driver = UAbyssSimDriver::Get(this);
	if (const abyss::Snapshot* Snap = Driver != nullptr ? Driver->GetSnapshotIndex().GetSnapshot() : nullptr)
	{
		return Snap;
	}
	const UAbyssGameInstance* GameInstance = UAbyssGameInstance::Get(this);
	return GameInstance != nullptr ? GameInstance->GetSnapshot() : nullptr;
}

double UAbyssCorpseLayer::SimNowMs() const
{
	const abyss::Snapshot* Snap = CurrentSnapshot();
	return Snap != nullptr ? Snap->simNowMs : 0.0;
}

double UAbyssCorpseLayer::GroundZ(double X, double Y) const
{
	const UAbyssSimDriver* Driver = UAbyssSimDriver::Get(this);
	const IAbyssWorldView* View = Driver != nullptr ? Driver->GetWorldView() : nullptr;
	return View != nullptr ? View->GetGroundHeight(X, Y) : 0.0;
}

FVector UAbyssCorpseLayer::HuskCentre(size_t Index) const
{
	const FAbyssCorpseState::FEntry& Entry = State->Entries[Index];
	FVector Centre;
	if (const AAbyssCharacterActor* Actor = Entry.Actor.Get())
	{
		// The block's centre is authored at scale 1: the body as drawn (a hunt leader x1.25) lies further out.
		const AbyssCorpseRules::FHuskGeometry Geometry = AbyssCorpseRules::HuskGeometry(Entry.Block,
			AbyssCorpsesPrivate::BodyScaleOf(Actor), AbyssCorpsesPrivate::DefaultHuskRadiusCm);
		Centre = Actor->GetActorLocation() + Actor->GetActorForwardVector() * Geometry.CenterForwardCm
			+ Actor->GetActorRightVector() * Geometry.CenterRightCm;
	}
	else
	{
		Centre = AbyssUnits::TileToWorld(Entry.Pos);
	}
	Centre.Z = GroundZ(Centre.X, Centre.Y);
	return Centre;
}

int32 UAbyssCorpseLayer::PlayAtHusk(size_t Index, const std::string& RecipeId, bool bWorldX, bool bHasColor, uint32 Rgb)
{
	UAbyssVfxSystem* Vfx = UAbyssVfxSystem::Get(this);
	if (Vfx == nullptr || RecipeId.empty() || Index >= State->Entries.size())
	{
		return 0;
	}
	const FAbyssCorpseState::FEntry& Entry = State->Entries[Index];
	FAbyssVfxContext Context;
	Context.Origin = Context.Target = Context.Point = HuskCentre(Index);
	// corpse.expire's ash blows along world +X (its recipe note); the rest in the body's frame.
	Context.Blow = bWorldX ? FVector2D(1.0, 0.0) : AbyssCorpsesPrivate::FacingOf(Entry.Actor.Get());
	Context.RadiusCm = static_cast<float>(AbyssCorpseRules::HuskGeometry(Entry.Block, AbyssCorpsesPrivate::BodyScaleOf(Entry.Actor.Get()),
		AbyssCorpsesPrivate::DefaultHuskRadiusCm).RadiusCm);
	Context.bHasColor = bHasColor;
	Context.Color = AbyssWorldUtil::ColorFromRgb(Rgb);
	return Vfx->Play(AbyssCorpsesPrivate::ToName(RecipeId), Context);
}

bool UAbyssCorpseLayer::SpawnHusk(size_t Index)
{
	if (Index >= State->Entries.size() || !HasAshMaterials())
	{
		return false;
	}
	UAbyssWorldBuilder* Builder = UAbyssWorldBuilder::Get(this);
	const UAbyssAssetLibrary* Assets = UAbyssAssetLibrary::Get(this);
	if (Builder == nullptr || Assets == nullptr)
	{
		return false;
	}
	FAbyssCorpseState::FEntry& Entry = State->Entries[Index];
	AAbyssCharacterActor* Actor = Builder->SpawnHuskActor(Entry.DefId, Entry.ArtId, Entry.Pos, AbyssCorpsesPrivate::SeedYawDeg(Entry.Id));
	if (Actor == nullptr)
	{
		return false;   // its art draws no husk (or none resolves): nothing to show
	}
	Entry.Block = State->BlockFor(Assets->GetManifest(), Actor->GetArtName());
	Actor->SetShadowMode(/*bBlob*/ true, /*bCastDynamic*/ bHuskSunShadow);   // 2G review X4: no CSM on mobile
	Actor->PoseAsHusk();
	Entry.Actor = Actor;
	Entry.Husk = AbyssCorpseRules::MakeHusk(Entry.DeathMs, Entry.Block.HoldFromMs, /*bBornHusk*/ true, /*bStoryHeld*/ false);
	Entry.bCrumblePlayed = true;   // born crumbled
	const AbyssCorpseRules::FAshLook Look = AbyssCorpseRules::Evaluate(Entry.Husk, Entry.Block.Timeline, SimNowMs());
	const double TopCm = AbyssCorpseRules::HuskGeometry(Entry.Block, AbyssCorpsesPrivate::BodyScaleOf(Actor),
		AbyssCorpsesPrivate::DefaultHuskRadiusCm).TopCm;
	Actor->SetAshLook(static_cast<float>(Look.Grey), static_cast<float>(Look.Dissolve), static_cast<float>(TopCm));
	Actor->SetHuskShadowAlpha(static_cast<float>(Look.ShadowAlpha));
	return true;
}

void UAbyssCorpseLayer::RemoveEntry(size_t Index)
{
	if (Index >= State->Entries.size())
	{
		return;
	}
	if (AAbyssCharacterActor* Actor = State->Entries[Index].Actor.Get())
	{
		Actor->Destroy();
	}
	State->Entries.erase(State->Entries.begin() + static_cast<std::ptrdiff_t>(Index));
}

// =====================================================================================================================
// Events
// =====================================================================================================================

void UAbyssCorpseLayer::HandleCorpseAdded(const abyss::EvCorpseAdded& Event)
{
	if (!State.IsValid() || State->IndexOf(Event.corpseId) != INDEX_NONE)
	{
		return;
	}
	EnsureContract();
	FAbyssCorpseState::FEntry& Entry = State->Entries.emplace_back();
	Entry.Id = Event.corpseId;
	Entry.Monster = Event.monster;
	Entry.DefId = Event.defId;
	Entry.ArtId = Event.artId;
	Entry.Pos = Event.pos;
	Entry.DeathMs = Event.deathMs;
	Entry.bSeeded = Event.seeded;
	if (Event.monster == abyss::kNoEntity)
	{
		SpawnHusk(State->Entries.size() - 1);   // a map seed: a husk from the start
		return;
	}
	// A kill: the dying actor that holds its Death pose for us (AAbyssCharacterActor::BeginCorpseDeath) changes hands.
	UAbyssWorldBuilder* Builder = UAbyssWorldBuilder::Get(this);
	AAbyssCharacterActor* Actor = Builder != nullptr ? Builder->ReleaseCorpseActor(Event.monster) : nullptr;
	const UAbyssAssetLibrary* Assets = UAbyssAssetLibrary::Get(this);
	if (Actor == nullptr || Assets == nullptr)
	{
		return;   // its death did not hold (no ash twins, no corpse block): today's fade played; the entry stays undrawn
	}
	Actor->AdoptAsCorpse();
	Actor->SetShadowMode(/*bBlob*/ true, /*bCastDynamic*/ bHuskSunShadow);   // 2G review X4: no CSM on mobile
	Entry.Actor = Actor;
	Entry.Block = State->BlockFor(Assets->GetManifest(), Actor->GetArtName());
	const abyss::Snapshot* Snap = CurrentSnapshot();
	const bool bHeld = AbyssCorpseRules::StoryHoldActive(Entry.Block, State->PlayingBeat, Snap != nullptr && Snap->storyBusy, false);
	Entry.Husk = AbyssCorpseRules::MakeHusk(Event.deathMs, Entry.Block.HoldFromMs, /*bBornHusk*/ false, bHeld);
}

void UAbyssCorpseLayer::HandleCorpseRemoved(const abyss::EvCorpseRemoved& Event)
{
	using namespace AbyssCorpseRules;
	if (!State.IsValid())
	{
		return;
	}
	const int32 Index = State->IndexOf(Event.corpseId);
	if (Index == INDEX_NONE)
	{
		return;
	}
	FAbyssCorpseState::FEntry& Entry = State->Entries[Index];
	Entry.bCoreRemoved = true;
	const FRemovalPlan Plan = PlanRemoval(Event.reason, Entry.Block);
	const double NowMs = SimNowMs();
	if (Plan.End == EEnd::Raised)
	{
		// The risen monster stands where it lay and takes the ash it had (EvMonsterRaised right after).
		FAbyssCorpseState::FRise& Rise = State->Rises.emplace_back();
		Rise.CorpseId = Entry.Id;
		Rise.RaiseRecipe = Entry.Block.RaiseRecipe;
		Rise.RiseLengthMs = Entry.Block.RiseLengthMs;
		if (Entry.Actor.IsValid())
		{
			const FAshLook Look = Evaluate(Entry.Husk, Entry.Block.Timeline, NowMs);
			Rise.Start = FRiseStart{ Look.Grey, Look.Dissolve };
		}
		RemoveEntry(static_cast<size_t>(Index));
		return;
	}
	if (!Entry.Actor.IsValid())
	{
		RemoveEntry(static_cast<size_t>(Index));
		return;
	}
	if (Entry.Husk.End != EEnd::None)
	{
		return;   // already ending (the cap retired it): it finishes that way
	}
	BeginEnd(Entry.Husk, Plan.End, Entry.Block.Timeline, NowMs);
	if (Plan.bFireColor)
	{
		Entry.bBurnPending = true;   // played in Tick: the fire kill's own hit of this dispatch may colour it
		Entry.Burner = Event.burner;
	}
	else if (!Plan.Recipe.empty())
	{
		PlayAtHusk(static_cast<size_t>(Index), Plan.Recipe, /*bWorldX*/ Plan.End == EEnd::Expired);
	}
}

void UAbyssCorpseLayer::HandleMonsterRaised(const abyss::EvMonsterRaised& Event)
{
	if (!State.IsValid())
	{
		return;
	}
	UAbyssWorldBuilder* Builder = UAbyssWorldBuilder::Get(this);
	AAbyssCharacterActor* Actor = Builder != nullptr ? Builder->FindCharacter(Event.entity) : nullptr;
	const FAbyssCorpseState::FRise* Rise = nullptr;
	if (Event.corpseId != 0)
	{
		for (const FAbyssCorpseState::FRise& Candidate : State->Rises)
		{
			if (Candidate.CorpseId == Event.corpseId)
			{
				Rise = &Candidate;
				break;
			}
		}
	}
	if (UAbyssVfxSystem* Vfx = UAbyssVfxSystem::Get(this))
	{
		FAbyssVfxContext Context;
		Context.Origin = Context.Target = Context.Point = AbyssUnits::TileToWorld(Event.pos);
		Context.Point.Z = Context.Target.Z = Context.Origin.Z = GroundZ(Context.Origin.X, Context.Origin.Y);
		Context.OriginActor = Actor;
		Context.TargetActor = Actor;
		Context.Blow = AbyssCorpsesPrivate::FacingOf(Actor);
		const std::string Recipe = Rise != nullptr && !Rise->RaiseRecipe.empty() ? Rise->RaiseRecipe : std::string("monster.raise");
		Vfx->Play(AbyssCorpsesPrivate::ToName(Recipe), Context);
	}
	if (Rise != nullptr && Actor != nullptr)
	{
		// The ash reverses over the first RiseFraction of the Rise clip (the core's riseMs is the clip as played).
		const double RiseMs = Event.riseMs > 0.0 ? Event.riseMs : Rise->RiseLengthMs;
		Actor->BeginAshRise(static_cast<float>(Rise->Start.Grey), static_cast<float>(Rise->Start.Dissolve), Event.startMs,
			RiseMs * State->Contract.RiseFraction);
	}
}

void UAbyssCorpseLayer::HandleMonsterFeed(const abyss::EvMonsterFeed& Event)
{
	if (!State.IsValid())
	{
		return;
	}
	const int32 Index = State->IndexOf(Event.corpseId);
	if (Index != INDEX_NONE)
	{
		FAbyssCorpseState::FEntry& Entry = State->Entries[Index];
		AbyssCorpseRules::BeginFeed(Entry.Husk, Entry.Block.Timeline, Event.startMs, Event.dueMs);
	}
	UAbyssVfxSystem* Vfx = UAbyssVfxSystem::Get(this);
	UAbyssWorldBuilder* Builder = UAbyssWorldBuilder::Get(this);
	const UAbyssSimDriver* Driver = UAbyssSimDriver::Get(this);
	const abyss::MonsterView* Feeder = Driver != nullptr ? Driver->GetSnapshotIndex().FindMonster(Event.entity) : nullptr;
	if (Vfx == nullptr || Feeder == nullptr)
	{
		return;
	}
	// monster.<defId>.feed: the body's ash streaming into the feeder (Blow runs corpse -> feeder).
	AAbyssCharacterActor* Actor = Builder != nullptr ? Builder->FindCharacter(Event.entity) : nullptr;
	FAbyssVfxContext Context;
	if (Index != INDEX_NONE)
	{
		Context.Origin = HuskCentre(static_cast<size_t>(Index));
	}
	else
	{
		Context.Origin = AbyssUnits::TileToWorld(Event.pos);
		Context.Origin.Z = GroundZ(Context.Origin.X, Context.Origin.Y);
	}
	Context.Point = Context.Origin;
	Context.Target = Actor != nullptr ? Actor->GetActorLocation() : AbyssUnits::TileToWorld(Feeder->pos, Context.Origin.Z);
	Context.TargetActor = Actor;
	Context.Blow = AbyssCorpsesPrivate::FlatDirection(Context.Origin, Context.Target, FVector2D(1.0, 0.0));
	Vfx->Play(AbyssCorpsesPrivate::ToName("monster." + Feeder->defId + ".feed"), Context);
}

void UAbyssCorpseLayer::HandlePlayAnim(const abyss::EvPlayAnim& Event)
{
	if (State.IsValid() && Event.action == abyss::AnimAction::Death)
	{
		SpawnPuddle(Event.entity);
		return;
	}
	// The gnawer's Consume channel (MonsterSystem::TryStartConsume: EvPlayAnim{Attack, clip Consume}): corpse.consume once.
	if (!State.IsValid() || Event.clip != "Consume")
	{
		return;
	}
	const UAbyssSimDriver* Driver = UAbyssSimDriver::Get(this);
	const abyss::MonsterView* Eater = Driver != nullptr ? Driver->GetSnapshotIndex().FindMonster(Event.entity) : nullptr;
	UAbyssVfxSystem* Vfx = UAbyssVfxSystem::Get(this);
	if (Eater == nullptr || Eater->consumingCorpse == 0 || Vfx == nullptr)
	{
		return;
	}
	for (const FAbyssCorpseState::FConsume& Consume : State->Consumes)
	{
		if (Consume.Eater == Event.entity && Consume.Corpse == Eater->consumingCorpse)
		{
			return;
		}
	}
	const int32 Index = State->IndexOf(Eater->consumingCorpse);
	UAbyssWorldBuilder* Builder = UAbyssWorldBuilder::Get(this);
	AAbyssCharacterActor* Actor = Builder != nullptr ? Builder->FindCharacter(Event.entity) : nullptr;
	FAbyssVfxContext Context;
	Context.Origin = Actor != nullptr ? Actor->GetActorLocation() : AbyssUnits::TileToWorld(Eater->pos);
	Context.OriginActor = Actor;
	Context.Target = Context.Point = Index != INDEX_NONE ? HuskCentre(static_cast<size_t>(Index)) : Context.Origin;
	Context.Blow = AbyssCorpsesPrivate::FlatDirection(Context.Origin, Context.Target, AbyssCorpsesPrivate::FacingOf(Actor));
	const std::string Recipe = Index != INDEX_NONE ? State->Entries[Index].Block.ConsumeRecipe : std::string("corpse.consume");
	FAbyssCorpseState::FConsume& Consume = State->Consumes.emplace_back();
	Consume.Eater = Event.entity;
	Consume.Corpse = Eater->consumingCorpse;
	Consume.Handle = Vfx->Play(AbyssCorpsesPrivate::ToName(Recipe), Context);
}

void UAbyssCorpseLayer::SpawnPuddle(abyss::EntityId Entity)
{
	// Breach-blood (AbyssCorpseRules::ParsePuddle): a body whose corpse block looks `puddle` has no registry entry; its
	// Death clip's tar puddle is drawn here with the shipped Puddle decal sprite (a runtime recipe per art asset).
	UAbyssWorldBuilder* Builder = UAbyssWorldBuilder::Get(this);
	AAbyssCharacterActor* Actor = Builder != nullptr ? Builder->FindCharacter(Entity) : nullptr;
	UAbyssVfxSystem* Vfx = UAbyssVfxSystem::Get(this);
	const UAbyssAssetLibrary* Assets = UAbyssAssetLibrary::Get(this);
	if (Actor == nullptr || Vfx == nullptr || Assets == nullptr || !Assets->HasManifest()
		|| Actor->GetEntityKind() != abyss::EntityKind::Monster || Actor->GetArtName().IsNone())
	{
		return;
	}
	EnsureContract();
	const FAbyssArtManifest& Manifest = Assets->GetManifest();
	const AbyssCorpseRules::FCorpseBlock& Block = State->BlockFor(Manifest, Actor->GetArtName());
	if (Block.Look != AbyssCorpseRules::ELook::Puddle)
	{
		return;
	}
	const double BodyScale = AbyssCorpsesPrivate::BodyScaleOf(Actor);
	const AbyssCorpseRules::FPuddle Puddle = AbyssCorpseRules::ParsePuddle(Manifest.GetAssetJson(Actor->GetArtName()), Block, BodyScale);
	if (!Puddle.bValid)
	{
		return;
	}
	const AbyssCorpseRules::FHuskGeometry Geometry = AbyssCorpseRules::HuskGeometry(Block, BodyScale, AbyssCorpsesPrivate::DefaultHuskRadiusCm);
	FAbyssVfxContext Context;
	FVector Centre = Actor->GetActorLocation() + Actor->GetActorForwardVector() * Geometry.CenterForwardCm
		+ Actor->GetActorRightVector() * Geometry.CenterRightCm;
	Centre.Z = GroundZ(Centre.X, Centre.Y);
	Context.Origin = Context.Target = Context.Point = Centre;
	Context.Blow = AbyssCorpsesPrivate::FacingOf(Actor);
	Context.RadiusCm = static_cast<float>(Puddle.RadiusCm);
	// One runtime recipe per art and drawn size (the recipe carries the size: a x1.25 body's puddle is its own).
	const FName RecipeId(*FString::Printf(TEXT("runtime.puddle.%s.%d"), *Actor->GetArtName().ToString(), FMath::RoundToInt(Puddle.RadiusCm)));
	Vfx->PlayRuntimeRecipe(RecipeId, AbyssCorpseRules::PuddleRecipeJson(Puddle), Context);
}

void UAbyssCorpseLayer::HandleAttackCancelled(const abyss::EvMonsterAttackCancelled& Event)
{
	// A consume channel cut short (a stun or freeze, the body gone): its tearing stops.
	if (!State.IsValid())
	{
		return;
	}
	UAbyssVfxSystem* Vfx = UAbyssVfxSystem::Get(this);
	std::vector<FAbyssCorpseState::FConsume>& Consumes = State->Consumes;
	for (size_t I = 0; I < Consumes.size();)
	{
		if (Consumes[I].Eater == Event.monster)
		{
			if (Vfx != nullptr)
			{
				Vfx->Stop(Consumes[I].Handle);
			}
			Consumes.erase(Consumes.begin() + static_cast<std::ptrdiff_t>(I));
		}
		else
		{
			++I;
		}
	}
}

void UAbyssCorpseLayer::HandleMonsterPhase(const abyss::EvMonsterPhase& Event)
{
	if (!State.IsValid())
	{
		return;
	}
	UAbyssWorldBuilder* Builder = UAbyssWorldBuilder::Get(this);
	AAbyssCharacterActor* Actor = Builder != nullptr ? Builder->FindCharacter(Event.entity) : nullptr;
	UAbyssVfxSystem* Vfx = UAbyssVfxSystem::Get(this);
	const UAbyssAssetLibrary* Assets = UAbyssAssetLibrary::Get(this);
	if (Actor == nullptr || Vfx == nullptr || Assets == nullptr || !Assets->HasManifest())
	{
		return;
	}
	FAbyssCorpseState::FLoops& Loops = State->Loops[Event.entity];
	if (Event.reset)
	{
		Vfx->Stop(Loops.Phase);   // walked home: the look goes with the phase
		Loops.Phase = 0;
		return;
	}
	// The entered row decides whether a clip notify plays the surge (the as-built zealot has no enter action: it plays here).
	const std::string DefId = AbyssText::ToStd(Actor->GetDefId());
	const UAbyssGameInstance* GameInstance = UAbyssGameInstance::Get(this);
	const abyss::DataStore* Data = GameInstance != nullptr ? GameInstance->GetData() : nullptr;
	const abyss::MonsterDef* Def = Data != nullptr ? AbyssMonsterLookRules::FindMonsterDef(*Data, DefId) : nullptr;
	const AbyssMonsterLookRules::FPhaseLook Look = AbyssMonsterLookRules::PhaseLookFor(DefId, Def, Event.phase, Event.reset,
		Assets->GetManifest().GetAssetJson(Actor->GetArtName()));
	FAbyssVfxContext Context;
	Context.Origin = Context.Target = Context.Point = Actor->GetActorLocation();
	Context.OriginActor = Actor;
	Context.TargetActor = Actor;
	Context.Blow = AbyssCorpsesPrivate::FacingOf(Actor);
	if (!Look.Instant.empty())
	{
		Vfx->Play(AbyssCorpsesPrivate::ToName(Look.Instant), Context);
	}
	if (!Look.Loop.empty())
	{
		Vfx->Stop(Loops.Phase);
		Loops.Phase = Vfx->Play(AbyssCorpsesPrivate::ToName(Look.Loop), Context);
		Loops.LastBlow = FVector2D::ZeroVector;   // the next update pushes the body's facing to every loop
	}
}

void UAbyssCorpseLayer::HandleBuffCast(const abyss::EvMonsterBuffCast& Event)
{
	UAbyssVfxSystem* Vfx = UAbyssVfxSystem::Get(this);
	const UAbyssSimDriver* Driver = UAbyssSimDriver::Get(this);
	const abyss::MonsterView* Caster = Driver != nullptr ? Driver->GetSnapshotIndex().FindMonster(Event.entity) : nullptr;
	if (Vfx == nullptr || Caster == nullptr)
	{
		return;
	}
	UAbyssWorldBuilder* Builder = UAbyssWorldBuilder::Get(this);
	AAbyssCharacterActor* Actor = Builder != nullptr ? Builder->FindCharacter(Event.entity) : nullptr;
	const UAbyssGameInstance* GameInstance = UAbyssGameInstance::Get(this);
	const abyss::DataStore* Data = GameInstance != nullptr ? GameInstance->GetData() : nullptr;
	const abyss::MonsterDef* Def = Data != nullptr ? AbyssMonsterLookRules::FindMonsterDef(*Data, Caster->defId) : nullptr;
	// The ability's own clip (its definition, not whatever clip plays now): a clipless call keeps its pulse.
	const std::string_view CastClip = AbyssMonsterLookRules::AbilityCastClip(Def, Event.abilityId);
	const bool bClipShowsPulse = !CastClip.empty() && Actor != nullptr && Actor->ClipHasFxNotifies(AbyssCorpsesPrivate::ToName(CastClip));
	const std::vector<std::string> Chain = AbyssMonsterLookRules::PulseChain(Caster->defId, Event.abilityId,
		Def != nullptr ? std::string_view(Def->family) : std::string_view(), bClipShowsPulse);
	TArray<FName, TInlineAllocator<2>> Ids;
	for (const std::string& Id : Chain)
	{
		Ids.Add(AbyssCorpsesPrivate::ToName(Id));
	}
	FAbyssVfxContext Context;
	Context.Origin = AbyssUnits::TileToWorld(Event.centre);
	Context.Origin.Z = GroundZ(Context.Origin.X, Context.Origin.Y);
	Context.Target = Context.Point = Context.Origin;
	Context.OriginActor = Actor;
	Context.TargetActor = Actor;
	Context.RadiusCm = static_cast<float>(Event.radius * AbyssUnits::TileUU);
	Context.Blow = AbyssCorpsesPrivate::FacingOf(Actor);
	Vfx->PlayFirst(Ids, Context);
}

void UAbyssCorpseLayer::HandleBlock(const abyss::EvBlock& Event)
{
	UAbyssVfxSystem* Vfx = UAbyssVfxSystem::Get(this);
	UAbyssWorldBuilder* Builder = UAbyssWorldBuilder::Get(this);
	AAbyssCharacterActor* Actor = Builder != nullptr ? Builder->FindCharacter(Event.entity) : nullptr;
	const UAbyssAssetLibrary* Assets = UAbyssAssetLibrary::Get(this);
	if (Vfx == nullptr || Actor == nullptr || Assets == nullptr || !Assets->HasManifest())
	{
		return;
	}
	// monster.block across the blow at the blocker's chest (the recipe's frame), its Point on the shield's face socket.
	const AbyssMonsterLookRules::FBlockFx Fx = AbyssMonsterLookRules::ParseBlockFx(Assets->GetManifest().GetAssetJson(Actor->GetArtName()));
	FAbyssVfxContext Context;
	Context.Target = Actor->GetActorLocation();
	Context.TargetActor = Actor;
	Context.Origin = AbyssUnits::TileToWorld(Event.from, Context.Target.Z);
	Context.OriginActor = Builder->FindCharacter(Event.source);
	FVector Face;
	Context.Point = Fx.bValid && Actor->GetHeldItemSocket(AbyssCorpsesPrivate::ToName(Fx.Item), AbyssCorpsesPrivate::ToName(Fx.Socket), Face)
		? Face : Actor->GetAnchorLocation(EAbyssAnchor::Chest);
	Context.Blow = AbyssCorpsesPrivate::FlatDirection(Context.Origin, Context.Target, AbyssCorpsesPrivate::FacingOf(Actor) * -1.0);
	Vfx->Play(AbyssCorpsesPrivate::ToName(Fx.Recipe), Context);
}

void UAbyssCorpseLayer::HandleHit(const abyss::EvHit& Event)
{
	// Fire killing blows of this dispatch: the impact colour of the body they burn at once (resolved in Tick).
	if (!State.IsValid() || Event.dodged || !Event.killed || Event.element != abyss::DamageType::Fire)
	{
		return;
	}
	FAbyssCorpseState::FFireKill& Kill = State->FireKills.emplace_back();
	Kill.Target = Event.target;
	Kill.Rgb = Event.impactColor;
}

void UAbyssCorpseLayer::HandleStoryBeat(const abyss::EvStoryBeat& Event)
{
	if (!State.IsValid())
	{
		return;
	}
	if (Event.phase == abyss::EvStoryBeat::Phase::Began)
	{
		State->PlayingBeat = Event.beatId;
		return;
	}
	if (State->PlayingBeat == Event.beatId)
	{
		State->PlayingBeat.clear();
	}
	for (FAbyssCorpseState::FEntry& Entry : State->Entries)
	{
		if (Entry.Block.bStoryHold && Entry.Block.StoryBeat == Event.beatId)
		{
			Entry.bBeatEndedSinceAdded = true;
		}
	}
}

// =====================================================================================================================
// Per frame
// =====================================================================================================================

void UAbyssCorpseLayer::Tick(const FAbyssFrameInfo& Frame, const abyss::Snapshot& Snap, float VisualDeltaSec, float RealDeltaSec)
{
	using namespace AbyssCorpseRules;
	if (!State.IsValid())
	{
		return;
	}
	EnsureContract();
	FAbyssCorpseState& S = *State;
	const double NowMs = Frame.RenderSimMs;
	UAbyssVfxSystem* Vfx = UAbyssVfxSystem::Get(this);

	// ---- the snapshot's registry: late views appear as husks, entries gone without an event dissolve ----
	S.Known.clear();
	for (const FAbyssCorpseState::FEntry& Entry : S.Entries)
	{
		if (!Entry.bCoreRemoved)
		{
			S.Known.push_back(Entry.Id);
		}
	}
	S.Present.clear();
	for (const abyss::CorpseView& Corpse : Snap.corpses)
	{
		S.Present.push_back(Corpse.id);
	}
	AbyssCorpseRules::Reconcile(S.Known, S.Present, S.Reconcile);
	for (const uint32 Id : S.Reconcile.Remove)
	{
		const int32 Index = S.IndexOf(Id);
		if (Index == INDEX_NONE)
		{
			continue;
		}
		FAbyssCorpseState::FEntry& Entry = S.Entries[Index];
		Entry.bCoreRemoved = true;
		if (!Entry.Actor.IsValid())
		{
			RemoveEntry(static_cast<size_t>(Index));
		}
		else if (Entry.Husk.End == EEnd::None)
		{
			BeginEnd(Entry.Husk, EEnd::Expired, Entry.Block.Timeline, NowMs);
			PlayAtHusk(static_cast<size_t>(Index), Entry.Block.ExpireRecipe, /*bWorldX*/ true);
		}
	}
	for (const uint32 Id : S.Reconcile.Add)
	{
		for (const abyss::CorpseView& Corpse : Snap.corpses)
		{
			if (Corpse.id != Id)
			{
				continue;
			}
			FAbyssCorpseState::FEntry& Entry = S.Entries.emplace_back();
			Entry.Id = Corpse.id;
			Entry.DefId = Corpse.defId;
			Entry.ArtId = Corpse.artId;
			Entry.Pos = Corpse.pos;
			Entry.DeathMs = Corpse.deathMs;
			Entry.bSeeded = Corpse.seeded;
			SpawnHusk(S.Entries.size() - 1);
			break;
		}
	}

	// ---- burns of this dispatch, in the colour of the fire's side (EvCorpseRemoved::burner) ----
	const abyss::EntityId PetId = Snap.pet.present ? Snap.pet.id : abyss::kNoEntity;
	for (size_t I = 0; I < S.Entries.size(); ++I)
	{
		FAbyssCorpseState::FEntry& Entry = S.Entries[I];
		if (!Entry.bBurnPending)
		{
			continue;
		}
		Entry.bBurnPending = false;
		uint32 KillRgb = 0;
		for (const FAbyssCorpseState::FFireKill& Kill : S.FireKills)
		{
			if (Entry.Monster != abyss::kNoEntity && Kill.Target == Entry.Monster)
			{
				KillRgb = Kill.Rgb;   // this body burned with the fire kill that made it
				break;
			}
		}
		const FBurnColor Color = BurnColor(FireSideOf(Entry.Burner, PetId), KillRgb);
		PlayAtHusk(I, Entry.Block.BurnRecipe, /*bWorldX*/ false, Color.bHas, Color.Rgb);
	}

	// ---- story holds: a held body lets go once its beat no longer plays ----
	for (FAbyssCorpseState::FEntry& Entry : S.Entries)
	{
		if (Entry.Husk.bStoryHeld && !StoryHoldActive(Entry.Block, S.PlayingBeat, Snap.storyBusy, Entry.bBeatEndedSinceAdded))
		{
			ReleaseStoryHold(Entry.Husk, NowMs, Entry.Block.HoldFromMs);
		}
	}

	// ---- the visible cap: overflow retires the oldest (seeds and story-held bodies kept first) ----
	S.CapEntries.clear();
	for (const FAbyssCorpseState::FEntry& Entry : S.Entries)
	{
		if (Entry.Actor.IsValid() && Entry.Husk.End == EEnd::None)
		{
			FHuskEntry& Cap = S.CapEntries.emplace_back();
			Cap.Id = Entry.Id;
			Cap.DeathMs = Entry.DeathMs;
			Cap.bSeeded = Entry.bSeeded;
			Cap.bStoryHeld = Entry.Husk.bStoryHeld;
		}
	}
	OverflowHusks(S.CapEntries, HuskCap, S.CapScratch, S.Overflow);
	for (const uint32 Id : S.Overflow)
	{
		const int32 Index = S.IndexOf(Id);
		if (Index != INDEX_NONE)
		{
			FAbyssCorpseState::FEntry& Entry = S.Entries[Index];
			BeginEnd(Entry.Husk, EEnd::Expired, Entry.Block.Timeline, NowMs);
			Entry.bRetired = true;
			PlayAtHusk(static_cast<size_t>(Index), Entry.Block.ExpireRecipe, /*bWorldX*/ true);
		}
	}

	// ---- each husk: the ash look, its crumble, its frame ----
	for (size_t I = 0; I < S.Entries.size();)
	{
		FAbyssCorpseState::FEntry& Entry = S.Entries[I];
		AAbyssCharacterActor* Actor = Entry.Actor.Get();
		if (Actor == nullptr)
		{
			if (Entry.bCoreRemoved)
			{
				S.Entries.erase(S.Entries.begin() + static_cast<std::ptrdiff_t>(I));
				continue;
			}
			++I;   // undrawn (or retired and gone): kept so the snapshot's view of it is not taken for a late one
			continue;
		}
		const FAshLook Look = Evaluate(Entry.Husk, Entry.Block.Timeline, NowMs);
		if (!Entry.bCrumblePlayed && Entry.Husk.End == EEnd::None && !Entry.Husk.bFeeding
			&& NowMs >= CrumbleStartMs(Entry.Husk, Entry.Block.Timeline))
		{
			Entry.bCrumblePlayed = true;
			PlayAtHusk(I, Entry.Block.CrumbleRecipe, /*bWorldX*/ false);
		}
		if (Look.bDone)
		{
			Actor->Destroy();
			Entry.Actor = nullptr;
			if (Entry.bCoreRemoved)
			{
				S.Entries.erase(S.Entries.begin() + static_cast<std::ptrdiff_t>(I));
				continue;
			}
			++I;
			continue;
		}
		const double TopCm = HuskGeometry(Entry.Block, AbyssCorpsesPrivate::BodyScaleOf(Actor), AbyssCorpsesPrivate::DefaultHuskRadiusCm).TopCm;
		Actor->SetAshLook(static_cast<float>(Look.Grey), static_cast<float>(Look.Dissolve), static_cast<float>(TopCm));
		Actor->SetHuskShadowAlpha(static_cast<float>(Look.ShadowAlpha));
		FAbyssCharacterFrame CharacterFrame;
		const FVector Location = Actor->GetActorLocation();
		CharacterFrame.Location = FVector(Location.X, Location.Y, GroundZ(Location.X, Location.Y));
		CharacterFrame.bAlive = false;
		Actor->PresentFrame(CharacterFrame, Frame, VisualDeltaSec, RealDeltaSec);
		++I;
	}

	// ---- consume channels that ended (finished, cut short, the eater gone) ----
	const UAbyssSimDriver* Driver = UAbyssSimDriver::Get(this);
	const FAbyssSnapshotIndex* SnapIndex = Driver != nullptr ? &Driver->GetSnapshotIndex() : nullptr;
	for (size_t I = 0; I < S.Consumes.size();)
	{
		const FAbyssCorpseState::FConsume& Consume = S.Consumes[I];
		const abyss::MonsterView* Eater = SnapIndex != nullptr ? SnapIndex->FindMonster(Consume.Eater) : nullptr;
		if (Eater == nullptr || !Eater->alive || Eater->consumingCorpse != Consume.Corpse)
		{
			if (Vfx != nullptr)
			{
				Vfx->Stop(Consume.Handle);
			}
			S.Consumes.erase(S.Consumes.begin() + static_cast<std::ptrdiff_t>(I));
			continue;
		}
		++I;
	}

	UpdateMonsterLoops(Snap);
	S.FireKills.clear();
	S.Rises.clear();
}

void UAbyssCorpseLayer::UpdateMonsterLoops(const abyss::Snapshot& Snap)
{
	using namespace AbyssMonsterLookRules;
	FAbyssCorpseState& S = *State;
	UAbyssVfxSystem* Vfx = UAbyssVfxSystem::Get(this);
	UAbyssWorldBuilder* Builder = UAbyssWorldBuilder::Get(this);
	auto StopAll = [Vfx](FAbyssCorpseState::FLoops& Loops)
	{
		if (Vfx == nullptr)
		{
			return;
		}
		Vfx->Stop(Loops.Aura);
		Vfx->Stop(Loops.Body);
		for (const int32 Handle : Loops.Affix)
		{
			Vfx->Stop(Handle);
		}
		Vfx->Stop(Loops.Phase);
	};
	for (std::pair<const abyss::EntityId, FAbyssCorpseState::FLoops>& Pair : S.Loops)
	{
		Pair.second.bSeen = false;
	}
	const UAbyssAssetLibrary* Assets = UAbyssAssetLibrary::Get(this);
	const FAbyssArtManifest* Manifest = Assets != nullptr && Assets->HasManifest() ? &Assets->GetManifest() : nullptr;
	for (const abyss::MonsterView& Monster : Snap.monsters)
	{
		auto It = S.Loops.find(Monster.id);
		AAbyssCharacterActor* Actor = Builder != nullptr ? Builder->FindCharacter(Monster.id) : nullptr;
		if (!Monster.alive || Actor == nullptr || Vfx == nullptr)
		{
			if (It != S.Loops.end())
			{
				StopAll(It->second);   // a death (or the actor gone) ends every loop
				S.Loops.erase(It);
			}
			continue;
		}
		const bool bAura = WantsAuraLoop(Monster.alive, Monster.auraRadius);
		bool bAffix = false;
		for (const abyss::EliteAffixType Affix : Monster.affixes)
		{
			bAffix = bAffix || !AffixLoopRecipe(Affix).empty();
		}
		const std::string_view BodyRecipe = Manifest != nullptr ? BodyLoopRecipe(Manifest->GetAssetJson(Actor->GetArtName())) : std::string_view();
		if (!bAura && !bAffix && BodyRecipe.empty() && It == S.Loops.end())
		{
			continue;   // nothing to keep (every chapter 2-5 monster)
		}
		FAbyssCorpseState::FLoops& Loops = S.Loops[Monster.id];
		Loops.bSeen = true;
		const FVector2D Facing = AbyssCorpsesPrivate::FacingOf(Actor);
		FAbyssVfxContext Context;
		Context.Origin = Context.Target = Context.Point = Actor->GetActorLocation();
		Context.OriginActor = Actor;
		Context.TargetActor = Actor;
		Context.Blow = Facing;
		if (bAura && Loops.Aura == 0 && !Loops.bAuraTried)
		{
			// The aura's ground stain at the aura's radius (monster.<defId>.aura, else the art's: the roll-call bearer).
			TArray<FName, TInlineAllocator<2>> Ids;
			for (const std::string& Id : AuraChain(Monster.defId, Monster.artId))
			{
				Ids.Add(AbyssCorpsesPrivate::ToName(Id));
			}
			FAbyssVfxContext Aura = Context;
			Aura.RadiusCm = static_cast<float>(Monster.auraRadius * AbyssUnits::TileUU);
			Loops.Aura = Vfx->PlayFirst(Ids, Aura);
			Loops.bAuraTried = true;   // 0 when no recipe exists: remembered, not retried every frame
		}
		else if (!bAura && Loops.bAuraTried)
		{
			Vfx->Stop(Loops.Aura);
			Loops.Aura = 0;
			Loops.bAuraTried = false;
		}
		if (!BodyRecipe.empty() && !Loops.bBodyTried)
		{
			// The art's own loop on the living body (the Cinderbloat's vent: monster.ash_flakes, A12 monsterLoops).
			Loops.Body = Vfx->Play(AbyssCorpsesPrivate::ToName(BodyRecipe), Context);
			Loops.bBodyTried = true;
		}
		if (bAffix && !Loops.bAffixTried)
		{
			for (const abyss::EliteAffixType Affix : Monster.affixes)
			{
				const std::string_view Recipe = AffixLoopRecipe(Affix);
				if (!Recipe.empty())
				{
					Loops.Affix.push_back(Vfx->Play(AbyssCorpsesPrivate::ToName(Recipe), Context));
				}
			}
			Loops.bAffixTried = true;
		}
		// Loops in the body's frame turn with it (pushed once the facing moved: each push scans the emitters).
		if (BlowTurned(Loops.LastBlow.X, Loops.LastBlow.Y, Facing.X, Facing.Y))
		{
			Loops.LastBlow = Facing;
			Vfx->SetHandleBlow(Loops.Aura, Facing);
			Vfx->SetHandleBlow(Loops.Body, Facing);
			for (const int32 Handle : Loops.Affix)
			{
				Vfx->SetHandleBlow(Handle, Facing);
			}
			Vfx->SetHandleBlow(Loops.Phase, Facing);
		}
	}
	for (auto It = S.Loops.begin(); It != S.Loops.end();)
	{
		if (!It->second.bSeen)
		{
			StopAll(It->second);   // left the snapshot
			It = S.Loops.erase(It);
		}
		else
		{
			++It;
		}
	}
}

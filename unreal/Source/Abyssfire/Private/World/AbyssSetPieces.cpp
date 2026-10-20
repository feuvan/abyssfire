// AAbyssZoneActor's set-piece layer (dark remake U4, implementation-plan.md 4; DECISIONS DR15): Data/set_pieces.json
// placed when a zone is built. The placements are the core's (abyss/world/SetPieces.h: anchors, offsets, rings, facings);
// every decision on top of them is AbyssSetPieceRules.h (id chain, variant hash, socket spots, covered tiles, echo
// anchors, yields), tested in PresentationTests/tests/setpieces_test.cpp. This file converts at the UE boundary (FName,
// FVector, the art manifest, the instanced dressing) and keeps no rule of its own.
#include "World/AbyssZoneActor.h"

#include "Abyssfire.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"

#include <string>
#include <string_view>
#include <vector>

#include "abyss/base/Types.h"
#include "abyss/data/DataStore.h"
#include "abyss/data/MapData.h"
#include "abyss/data/SetPieceData.h"
#include "abyss/sim/Snapshot.h"
#include "abyss/world/SetPieces.h"

#include "Framework/AbyssText.h"
#include "World/AbyssArtManifest.h"
#include "World/AbyssAssetLibrary.h"
#include "World/AbyssSetPieceRules.h"
#include "World/AbyssSetPieces.h"
#include "World/AbyssWorldTypes.h"

namespace AbyssSetPiecesPrivate
{
	FName SetPieceName(std::string_view Id)
	{
		return FName(*AbyssText::ToFString(Id));
	}

	/**
	 * AbyssSetPieceRules::PlacedArt from an asset the zone actor placed: its bounds (manifest boundsCm, else the mesh's
	 * box) and, for the main mesh, its static-mesh sockets, in the mesh frame scaled by the asset's own scale (the
	 * piece's scale is the core's: SetPieceLocalToTile / SetPieceCoveredTiles).
	 */
	void AddSetPieceAsset(const FAbyssArtAsset& Art, UAbyssAssetLibrary& Assets, bool bSockets,
		AbyssSetPieceRules::PlacedArt& Out)
	{
		const double Scale = FMath::Max(0.05, static_cast<double>(Art.Scale));
		FBox Box = Art.BoundsCm;
		if (!Box.IsValid)
		{
			if (UStaticMesh* Mesh = Assets.LoadStaticMesh(Art))
			{
				Box = Mesh->GetBoundingBox();
			}
		}
		if (Box.IsValid)
		{
			Out.Bounds.push_back({ Box.Min.X * Scale, Box.Min.Y * Scale, Box.Max.X * Scale, Box.Max.Y * Scale });
		}
		if (bSockets)
		{
			for (const FAbyssArtSocket& Socket : Art.Sockets)
			{
				Out.Sockets.push_back(AbyssSetPieceRules::Socket{ AbyssText::ToStd(Socket.Name.ToString()), Socket.LocCm.X * Scale,
					Socket.LocCm.Y * Scale });
			}
		}
	}

	FString JoinSetPieceIds(const std::vector<std::string>& Items)
	{
		FString Out;
		for (const std::string& Item : Items)
		{
			Out += Out.IsEmpty() ? AbyssText::ToFString(Item) : TEXT(", ") + AbyssText::ToFString(Item);
		}
		return Out.IsEmpty() ? FString(TEXT("none")) : Out;
	}

	/** A yielded piece's instance: scale 0 far below the map (UAbyssVfxSystem's idle-pool idiom), so no shadow either. */
	FTransform HiddenSetPieceTransform()
	{
		return FTransform(FQuat::Identity, FVector(0.0, 0.0, -100000.0), FVector::ZeroVector);
	}
}

void AAbyssZoneActor::BuildSetPieces(const abyss::Snapshot& Snap, const abyss::DataStore& Data)
{
	SetPieceState = MakeShared<FAbyssSetPieceState>();
	UAbyssAssetLibrary* Assets = AssetLibrary.Get();
	const abyss::SetPieceZoneDef* Zone = Data.SetPieces().ForZone(Snap.zone.mapId);
	const abyss::MapDef* Map = Data.FindMap(Snap.zone.mapId);
	if (Zone == nullptr || Map == nullptr || Assets == nullptr)
	{
		return;   // a zone without set pieces (or Data/set_pieces.json absent: the table is optional)
	}
	abyss::SetPieceFrame Frame;
	Frame.meshYawOffsetDeg = MeshYawOffsetDeg;
	Frame.cameraYawDeg = CameraYawDeg;
	std::vector<abyss::SetPiecePlacement> Placements;
	abyss::ExpandSetPieces(*Zone, *Map, Data, Frame, Placements);

	FAbyssSetPieceState& State = *SetPieceState;
	std::vector<AbyssSetPieceRules::PlacedArt> Arts(Placements.size());
	for (size_t Index = 0; Index < Placements.size(); ++Index)
	{
		const abyss::SetPiecePlacement& Placement = Placements[Index];
		const abyss::SetPieceDef& Def = *Placement.def;
		TArray<FName, TInlineAllocator<4>> Ids;
		for (const std::string& Id : AbyssSetPieceRules::IdChain(Def))
		{
			Ids.Add(AbyssSetPiecesPrivate::SetPieceName(Id));
		}
		const abyss::TilePos Tile = abyss::RoundToTile(Placement.pos);
		const uint32 Hash = AbyssSetPieceRules::VariantHash(Placement.variant,
			AbyssWorldUtil::TileHash(Tile.col, Tile.row, AbyssSetPieceRules::VariantHashSalt));
		const FVector Location = GroundPoint(Placement.pos.x, Placement.pos.y);
		const float YawDeg = static_cast<float>(Placement.yawDeg);
		const float Scale = static_cast<float>(Def.scale);
		// Only a yielding piece needs its instance found again (hidden while a defend target stands on it).
		const FName Key = Def.yieldsToDefendTarget
			? AbyssSetPiecesPrivate::SetPieceName(AbyssSetPieceRules::InstanceKey(Placement)) : NAME_None;
		const FAbyssArtAsset* Art = nullptr;
		FTransform Transform;
		// Tall pieces fade between the camera and the hero (R1, the decor fade target); none blocks anything.
		if (!PlaceArt(Ids, Hash, Location, YawDeg, Scale, Def.occluder, 0.f, Key, &Art, &Transform) || Art == nullptr)
		{
			continue;   // PlaceArt counts it for the zone-build warning; Collect lists the piece as unresolved
		}
		AbyssSetPieceRules::PlacedArt& Placed = Arts[Index];
		Placed.bResolved = true;
		AbyssSetPiecesPrivate::AddSetPieceAsset(*Art, *Assets, /*bSockets*/ true, Placed);
		++State.PlacedPieces;
		++State.PlacedMeshes;
		if (!Key.IsNone())
		{
			State.YieldTransforms.Add(Key, Transform);
		}
		// Ground layers (the bridge's ditch bed, the circles' poured ash): same spot, yaw and variant hash, never occluders.
		for (const std::string& Partner : Def.partners)
		{
			const FName PartnerIds[] = { AbyssSetPiecesPrivate::SetPieceName(Partner) };
			const FAbyssArtAsset* PartnerArt = nullptr;
			if (PlaceArt(PartnerIds, Hash, Location, YawDeg, Scale, false, 0.f, NAME_None, &PartnerArt) && PartnerArt != nullptr)
			{
				AbyssSetPiecesPrivate::AddSetPieceAsset(*PartnerArt, *Assets, /*bSockets*/ false, Placed);
				++State.PlacedMeshes;
			}
		}
	}

	State.Spots = AbyssSetPieceRules::Collect(Placements, Arts);
	const AbyssSetPieceRules::ZoneSpots& Spots = State.Spots;
	// Socket loops (the pit mouth's smoke, the bridge's smoulder) join the zone loops the world builder starts.
	for (const AbyssSetPieceRules::EffectSpot& Effect : Spots.Effects)
	{
		AddEffectSpot(AbyssSetPiecesPrivate::SetPieceName(Effect.Recipe), GroundPoint(Effect.Tile.x, Effect.Tile.y));
	}
	for (const AbyssSetPieceRules::Yielder& Yielder : Spots.Yielders)
	{
		State.YielderKeys.Add(AbyssSetPiecesPrivate::SetPieceName(Yielder.Key));
	}
	int32 EmitterSpotCount = 0;
	for (const AbyssSetPieceRules::EmitterSpots& Emitter : Spots.Emitters)
	{
		EmitterSpotCount += static_cast<int32>(Emitter.Tiles.size());
	}
	UE_LOG(LogAbyss, Log, TEXT("Zone %s set pieces: %d of %d placements drawn (%d meshes), %d effect / %d emitter / ")
		TEXT("%d cue spots, %d echo anchors, %d yielding to a defend target, %d tiles without loose dressing"),
		*AbyssText::ToFString(Snap.zone.mapId), State.PlacedPieces, static_cast<int32>(Placements.size()),
		State.PlacedMeshes, static_cast<int32>(Spots.Effects.size()), EmitterSpotCount,
		static_cast<int32>(Spots.Cues.size()), static_cast<int32>(Spots.Echoes.size()),
		static_cast<int32>(Spots.Yielders.size()), static_cast<int32>(Spots.Covered.size()));
	if (!Spots.Unresolved.empty() || !Spots.MissingSockets.empty())
	{
		// A stale Data/assets.json or art not imported yet (the content build): the piece is left out, the rest stands.
		UE_LOG(LogAbyss, Warning, TEXT("Zone %s set pieces: no art for %s; sockets missing on the resolved art: %s"),
			*AbyssText::ToFString(Snap.zone.mapId), *AbyssSetPiecesPrivate::JoinSetPieceIds(Spots.Unresolved),
			*AbyssSetPiecesPrivate::JoinSetPieceIds(Spots.MissingSockets));
	}
}

void AAbyssZoneActor::GetSetPieceEmitterSpots(TArray<TPair<FName, TArray<FVector2D>>>& Out) const
{
	Out.Reset();
	if (!SetPieceState.IsValid())
	{
		return;
	}
	for (const AbyssSetPieceRules::EmitterSpots& Emitter : SetPieceState->Spots.Emitters)
	{
		TPair<FName, TArray<FVector2D>>& Entry = Out.AddDefaulted_GetRef();
		Entry.Key = AbyssSetPiecesPrivate::SetPieceName(Emitter.Emitter);
		for (const abyss::Vec2& Tile : Emitter.Tiles)
		{
			Entry.Value.Add(FVector2D(Tile.x, Tile.y));
		}
	}
}

void AAbyssZoneActor::GetSetPieceCueSpots(FName CueId, TArray<FAbyssZoneEffectSpot>& Out) const
{
	if (!SetPieceState.IsValid() || CueId.IsNone())
	{
		return;
	}
	const std::string Cue = AbyssText::ToStd(CueId.ToString());
	for (const AbyssSetPieceRules::CueSpot* Spot : AbyssSetPieceRules::CueSpotsFor(SetPieceState->Spots, Cue))
	{
		FAbyssZoneEffectSpot& Effect = Out.AddDefaulted_GetRef();
		Effect.RecipeId = AbyssSetPiecesPrivate::SetPieceName(Spot->Recipe);
		Effect.Location = GroundPoint(Spot->Tile.x, Spot->Tile.y);
		if (!PresentationTheme.IsNone() && Spot->Recipe.find('@') == std::string::npos)
		{
			Effect.ThemedRecipeId = FName(*(Effect.RecipeId.ToString() + TEXT("@") + PresentationTheme.ToString()));
		}
	}
}

bool AAbyssZoneActor::GetSetPieceEchoAnchor(const std::string& LoreId, const abyss::Vec2& LoreTile, abyss::Vec2& OutTile,
	double& OutYawDeg) const
{
	const AbyssSetPieceRules::EchoAnchor* Anchor =
		SetPieceState.IsValid() ? AbyssSetPieceRules::EchoAnchorFor(SetPieceState->Spots, LoreId, LoreTile) : nullptr;
	if (Anchor == nullptr)
	{
		return false;
	}
	OutTile = Anchor->Tile;
	OutYawDeg = Anchor->YawDeg;
	return true;
}

void AAbyssZoneActor::UpdateSetPieceYields(TConstArrayView<abyss::Vec2> DefendTargetTiles)
{
	if (!SetPieceState.IsValid() || SetPieceState->YielderKeys.Num() == 0)
	{
		return;
	}
	FAbyssSetPieceState& State = *SetPieceState;
	TArray<FName, TInlineAllocator<4>> Wanted;
	for (const abyss::Vec2& Tile : DefendTargetTiles)
	{
		if (const AbyssSetPieceRules::Yielder* Yielder = AbyssSetPieceRules::YielderNear(State.Spots, Tile))
		{
			const int32 YielderIndex = static_cast<int32>(Yielder - State.Spots.Yielders.data());
			if (State.YielderKeys.IsValidIndex(YielderIndex))
			{
				Wanted.AddUnique(State.YielderKeys[YielderIndex]);
			}
		}
	}
	for (int32 Index = State.Yielded.Num() - 1; Index >= 0; --Index)
	{
		const FName Key = State.Yielded[Index];
		if (!Wanted.Contains(Key))
		{
			State.Yielded.RemoveAt(Index);
			SetSetPieceHidden(Key, false);
		}
	}
	for (const FName Key : Wanted)
	{
		if (!State.Yielded.Contains(Key))
		{
			State.Yielded.Add(Key);
			SetSetPieceHidden(Key, true);
		}
	}
}

bool AAbyssZoneActor::GetSetPieceYieldFacing(const abyss::Vec2& Tile, float& OutFacingYawDeg) const
{
	const AbyssSetPieceRules::Yielder* Yielder =
		SetPieceState.IsValid() ? AbyssSetPieceRules::YielderNear(SetPieceState->Spots, Tile) : nullptr;
	if (Yielder == nullptr)
	{
		return false;
	}
	// The piece's yaw is an actor yaw (the mesh frame); a prop's facing gets the manifest's mesh yaw offset added.
	OutFacingYawDeg = static_cast<float>(Yielder->YawDeg) - MeshYawOffsetDeg;
	return true;
}

void AAbyssZoneActor::SetSetPieceHidden(FName Key, bool bHidden)
{
	const TPair<int32, int32>* Found = StoryInstances.Find(Key);
	const FTransform* Placed = SetPieceState.IsValid() ? SetPieceState->YieldTransforms.Find(Key) : nullptr;
	if (Found == nullptr || Placed == nullptr || !DecorInstanceComponents.IsValidIndex(Found->Key))
	{
		return;
	}
	UInstancedStaticMeshComponent* Component = DecorInstanceComponents[Found->Key].Get();
	if (Component == nullptr)
	{
		return;
	}
	const TArray<FTransform> Transforms = { bHidden ? AbyssSetPiecesPrivate::HiddenSetPieceTransform() : *Placed };
	// Incremental instance update (5.4+ instance data manager), as the occlusion fade and the VFX pools do.
	Component->BatchUpdateInstancesTransforms(Found->Value, Transforms, /*bWorldSpace*/ true, /*bMarkRenderStateDirty*/ false,
		/*bTeleport*/ true);
	TSet<int32> Dirty;
	TSet<int32> DirtyGhosts;
	Dirty.Add(Found->Key);
	// Its occluder neither covers anything nor keeps a fade (or a fade ghost) while the piece is hidden.
	for (int32 Index = 0; Index < Occluders.Num(); ++Index)
	{
		FOccluder& Occluder = Occluders[Index];
		if (Occluder.Component != Found->Key || Occluder.Instance != Found->Value)
		{
			continue;
		}
		Occluder.bSuppressed = bHidden;
		if (Occluder.Fade != 0.f || Occluder.bGhosted)
		{
			Occluder.Fade = 0.f;
			ApplyOccluderFade(Index, bFadeGhostMode, Dirty, DirtyGhosts);
		}
	}
	for (const int32 ComponentIndex : Dirty)
	{
		if (UInstancedStaticMeshComponent* DirtyComponent = DecorInstanceComponents[ComponentIndex].Get())
		{
			DirtyComponent->MarkRenderInstancesDirty();
		}
	}
	for (const int32 ComponentIndex : DirtyGhosts)
	{
		UpdateFadeGhost(ComponentIndex);
	}
	UE_LOG(LogAbyss, Log, TEXT("Set piece %s %s"), *Key.ToString(),
		bHidden ? TEXT("hidden: a defend target of the same art stands on it") : TEXT("shown again"));
}

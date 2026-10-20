// The set-piece layer's state of one built zone (dark remake U4; data Data/set_pieces.json, placements
// abyss/world/SetPieces.h, rules AbyssSetPieceRules.h). AAbyssZoneActor::BuildSetPieces (AbyssSetPieces.cpp) fills it at
// the zone build; the zone actor reads it for the decorations it leaves out and the pieces that yield to a defend target,
// and hands its spots to the world builder (zone loops, audio emitter spots, cue ripples, echo anchors).
#pragma once

#include "CoreMinimal.h"

#include "World/AbyssSetPieceRules.h"

struct FAbyssSetPieceState
{
	/** Every spot of the zone's pieces (AbyssSetPieceRules::Collect over the placed art). */
	AbyssSetPieceRules::ZoneSpots Spots;
	/** Spots.Yielders[i]'s instance key (the zone actor's keyed instances, StoryInstances). */
	TArray<FName> YielderKeys;
	/** A yielding piece's placed transform, restored when no defend target stands on it any more. */
	TMap<FName, FTransform> YieldTransforms;
	/** Keys of the yielding pieces hidden now. */
	TArray<FName> Yielded;
	/** Placements drawn (main meshes) and the meshes placed in all (with the partners). */
	int32 PlacedPieces = 0;
	int32 PlacedMeshes = 0;
};

// World entity picking without collision (ue58-platform.md 8.4; world-map-nav.md 1.4, 7.1; combat-feel.md 9.1).
//
// Every registered entity actor (UAbyssActorRegistry) is projected to the screen: the world box of its visible mesh
// components (fallback: a kind-sized cylinder at the actor) becomes a screen rectangle, inflated by a pointer slop. Among
// the rectangles under the pointer the winner is chosen by the web's press priority (world-map-nav 7.1: loot, NPC,
// chest / event prop, monster, then walk-to objects), then by how centred the pointer is, then by camera distance.
// The snapshot decides what an entity is (alive monster, marker kind); presentation actors never decide gameplay.
//
// Nothing picked -> the caller deprojects the ground (AAbyssPlayerController::DeprojectToTile) and sends CmdPointerPress,
// whose core chain keeps the web's 1.5-tile tolerance as the fallback.
#pragma once

#include "CoreMinimal.h"

#include "Input/AbyssInputTypes.h"

class APlayerController;

namespace AbyssPicking
{
	/**
	 * The entity whose projected bounds contain ScreenPosition (viewport pixels), or an invalid result. SlopPixels inflates
	 * every rectangle (touch: a finger is wide). Reads the driver's snapshot index; game thread only.
	 */
	ABYSSFIRE_API FAbyssPickResult PickEntity(const APlayerController& Controller, const FVector2D& ScreenPosition,
		float SlopPixels);

	/** What pressing on an entity does, from the snapshot (EAbyssPickAction::None = not pickable now). */
	ABYSSFIRE_API EAbyssPickAction ClassifyEntity(const APlayerController& Controller, abyss::EntityId Id,
		abyss::EntityKind Kind, abyss::Vec2& OutTile, int32& OutPriority);
}

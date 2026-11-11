// The sub-dungeon layer's state of one built zone (dark remake Wave 3 / U6; rules AbyssSubDungeonRules.h): the grade and
// cleanse contract of the zone's theme (vfx_recipes.json dark.<theme>), the cleanse sequence's state, the lamps' twin
// instances and sockets, the niche's spots and the build's counters (the autotest's probe). AAbyssZoneActor fills it at
// the zone build (BuildZone, BuildWalls, BuildSetPieces) and advances it every frame (TickSubDungeon).
#pragma once

#include "CoreMinimal.h"

#include "World/AbyssSubDungeonRules.h"

struct FAbyssSubDungeonLamp
{
	/** Keyed instances (the zone actor's StoryInstances) of the unlit and the lit mesh at one transform. */
	FName UnlitKey;
	FName LitKey;
	abyss::Vec2 Tile;
	/** World positions of the lit mesh's `flame` and `light` sockets (the piece's own location without them): the flame
	 *  loop plays at Flame, its light pool lies on the ground under Light (LightHeightCm over it: the toon slot's spot). */
	FVector Flame = FVector::ZeroVector;
	FVector Light = FVector::ZeroVector;
	float LightHeightCm = 0.f;
	bool bLit = false;
};

struct FAbyssSubDungeonState
{
	/** The hero is inside a sub-dungeon (ZoneView::subDungeon.inside at the build). */
	bool bInside = false;
	/** AbyssSubDungeonRules::CutFromRock: a hand-authored sub-dungeon (void rock, blocks against open tiles only, no
	 *  skirt, a stair for an exit). False in the generated caves. */
	bool bCutFromRock = false;
	/** The mood the cleanse blends to (AbyssThemeRules::FindCleansedMood, resolved once per build). */
	bool bHasCleansedMood = false;
	abyss::MapTheme CleansedMood = abyss::MapTheme::Plains;
	/** The grade / cleanse contract of the zone's theme chain (bValid false: an ungraded zone). */
	AbyssSubDungeonRules::FCleanseSpec Spec;
	AbyssSubDungeonRules::FCleanseState Cleanse;
	/** The cleanse's progress this frame (AbyssSubDungeonRules::StateAlpha; AAbyssZoneActor::UpdateMood writes it). */
	double Alpha = 0.0;
	TArray<FAbyssSubDungeonLamp> Lamps;
	/** Set pieces of the contract's unlit lamp id the map placed (Lamps holds those whose lit twin placed too). */
	int32 LampsExpected = 0;
	/** The Flame Niche's sockets: `niche` and the `mark_<n>` that exist in number order (AbyssSubDungeonRules::MarkOrder:
	 *  no holes), world positions; the niche's tile. */
	bool bHasNiche = false;
	FVector Niche = FVector::ZeroVector;
	abyss::Vec2 NicheTile;
	TArray<FVector> Marks;
	// ---- the build's counters
	int32 WallTiles = 0;
	int32 WallBlocks = 0;
	/** A wall block resolved to art of the theme chain (not the plain outcrop) and the floor paints the chain's style. */
	bool bThemedWall = false;
	bool bThemedFloor = false;
};

// Small shared types of the Abyssfire backbone (no UObjects).
#pragma once

#include "CoreMinimal.h"

#include "abyss/base/Types.h"
#include "abyss/sim/Snapshot.h"

/** Top-level application state, owned by UAbyssGameInstance (save-ui-input.md 1). */
enum class EAbyssAppState : uint8
{
	Boot,       // GameInstance::Init is loading Data/*.json and settings
	DataError,  // the data tables failed to load / validate: the UI shows the error, nothing else runs
	MainMenu,   // no session: title, save slots, class select, difficulty, language, settings
	InGame,     // a GameSim session exists (the world may be frozen: pause menu, dialogue, cinematic)
};

ABYSSFIRE_API const TCHAR* LexToString(EAbyssAppState State);

/**
 * Per-frame timing info handed to every per-frame consumer (world view, UI root, router OnFrame).
 * All "Sim" times are SimClock ms (the core's clock, frozen while the world is frozen).
 */
struct FAbyssFrameInfo
{
	/** Undilated real time of this rendered frame (what was fed to GameSim::Frame), ms. */
	double RealDeltaMs = 0.0;
	/** Snapshot::simNowMs after this frame's steps. */
	double SimNowMs = 0.0;
	/**
	 * The sim time the interpolated pose shows: SimNowMs - (1 - Alpha) * kSimStepMs. Use it to place anything timed
	 * in sim ms (projectile flight from EvProjectileLaunched::launchMs, EvPlayAnim::startMs, ground effects) so it
	 * lines up with Lerp(prevPos, pos, Alpha).
	 */
	double RenderSimMs = 0.0;
	/** Snapshot::interpolationAlpha in [0, 1): render Lerp(prevPos, pos, Alpha) (S1). */
	double Alpha = 0.0;
	/** Number of 60 Hz sim steps run this frame (0..SimConfig::maxStepsPerFrame). */
	int32 StepsThisFrame = 0;
	/** Snapshot::frozen / cinematic (story beat playing: hide the HUD, letterbox). */
	bool bFrozen = false;
	bool bCinematic = false;
	/** Visual global time dilation currently applied by the driver (S6 elite-kill slow motion), 1 = none. */
	float VisualTimeDilation = 1.f;
	/** Monotonic counter of pumped frames (resets per session). */
	uint64 FrameNumber = 0;
};

/**
 * EntityId -> index lookups into the current Snapshot's per-entity vectors. Rebuilt by the driver once per frame,
 * right after GameSim::Frame; pointers it returns are valid until the next call into GameSim (never cache them).
 */
class ABYSSFIRE_API FAbyssSnapshotIndex
{
public:
	void Rebuild(const abyss::Snapshot& Snap);
	void Reset();

	const abyss::Snapshot* GetSnapshot() const { return Snapshot; }

	const abyss::MonsterView* FindMonster(abyss::EntityId Id) const;
	const abyss::NpcView* FindNpc(abyss::EntityId Id) const;
	const abyss::GroundItemView* FindGroundItem(abyss::EntityId Id) const;
	const abyss::PotionDropView* FindPotion(abyss::EntityId Id) const;
	const abyss::ProjectileView* FindProjectile(abyss::EntityId Id) const;
	const abyss::GroundEffectView* FindGroundEffect(abyss::EntityId Id) const;
	const abyss::WorldMarkerView* FindMarker(abyss::EntityId Id) const;

	/**
	 * Tile-space position of any entity that has one in the snapshot (hero, monsters, NPCs, pet, ground items, potions,
	 * markers, ground effects; projectiles report their interpolated flight point as both). OutPrev == OutCur for
	 * entities that do not move. Returns false when the id is not in the snapshot.
	 */
	bool FindEntityPosition(abyss::EntityId Id, abyss::Vec2& OutPrev, abyss::Vec2& OutCur) const;

private:
	const abyss::Snapshot* Snapshot = nullptr;
	TMap<uint32, int32> Monsters;
	TMap<uint32, int32> Npcs;
	TMap<uint32, int32> GroundItems;
	TMap<uint32, int32> Potions;
	TMap<uint32, int32> Projectiles;
	TMap<uint32, int32> GroundEffects;
	TMap<uint32, int32> Markers;
};

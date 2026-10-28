// UAbyssWorldBuilder: the game world's IAbyssWorldView (ARCHITECTURE 6, README "Implementing the interfaces").
//
// * Zone: spawns an AAbyssZoneActor per zone from the snapshot (terrain, water, outcrops, palisades, decor, camps,
//   exits / sealed gate, story props, sun, post-process), starts the zone's looping VFX (campfires, torches, exits,
//   ambience) and the asset-library zone retention (web sheetKeepZones: 2 desktop, 1 touch).
// * Entities: one AAbyssCharacterActor per hero / monster / NPC / pet / escort (blob shadow on every tier, sun CSM cast
//   where the tier has one, P10), one AAbyssPropActor per ground item (its loot_<baseId> pickup mesh or the tinted bag;
//   magic and better loop loot.beam.<quality>, R17), potion, quest marker, lore pickup, hidden reward, event prop, soul
//   echo, defend target; all registered in UAbyssActorRegistry (the driver routes IAbyssPresenter events, the input
//   agent picks through it). Markers that the core never announced with EvEntitySpawned are reconciled from the
//   snapshot every frame.
// * Per frame (SyncFrame, before the UI root): interpolated transforms (Lerp(prevPos, pos, alpha), S1), facing, speed,
//   statuses; camera follow / shake / fades / story focus (AAbyssCameraRig) and the zone's view fit (CSM distance, fog
//   start); occlusion fade (R1); the target ring, selection rims (R16) and quest guide arrow; MPC_AF_Lighting (incl.
//   SunScale, InkScale); the VFX system tick; world widget placements for the UI (IAbyssWorldUi).
// * Camera / feel events: EvCameraShake (honours the camera-shake setting), EvCameraFlash, zone transition fades, hero
//   death / respawn fades, level-up flash + shake + zoom pulse, legendary drop flash, story focus / shake / flash steps,
//   dodge afterimages, equipment weapon meshes (I6).
// * Ground picking for the input agent: RaycastGround / PickGround (ray vs the terrain height field, no collision,
//   ue58-platform.md 8.4).
// * Presentation theme (dark remake U1, Private/World/AbyssThemeRules.h): characters, props and the elite crown of a zone
//   with a presentation theme resolve `<gameId>@<theme>` first (the theme comes from the map id of the snapshot / event
//   at hand, never from the zone build: the next zone's spawns arrive before it); zone loops play `<recipe>@<theme>`
//   when it exists; every frame the zone actor's mood state (dawn, Breach Rim blend) is updated, the ambience loop is
//   swapped when its recipe or tint changes and the mood's light scale reaches the VFX system; monsters get the mood's
//   wind-up tint; echo lore (spriteType echo) shows no mesh and, when read, a ghost figure frozen in its pose for 3 s
//   (manifest materials.MI_AF_Echo); the brightness setting biases the zone exposure.
// * Dark remake 2G (U3, Private/Actors/AbyssMonsterLookRules.h): a monster whose death leaves a husk holds its Death pose
//   (setup from UAbyssCorpseLayer::CorpseHoldFor) and is handed to the corpse layer on EvCorpseAdded (ReleaseCorpseActor);
//   map seeds and late views are spawned for it as posed husks (SpawnHuskActor). A fleeing monster walks / runs and a
//   blocker faces its heading even while it attacks; in a themed zone the elite marker is the Wave 2 sigil (absolute
//   scale on fx_elite, else fx_overhead); the zone's art warm-up covers every definition it can stand up.
// * Set pieces (dark remake U4, AAbyssZoneActor::BuildSetPieces): after a zone build the pieces' audio emitter spots go to
//   UAbyssAudioSystem::SetZoneEmitterSpots (Ashwheat's knocking doors); a bell cue the audio system plays
//   (OnCuePlayed) ripples the bell frame / the Boneyard Gate's bell; an echo a piece poses stands by that piece, turned
//   with it; a defend target standing on a yielding piece (the Boneyard Gate) hides it and takes its facing.
// * Dark remake 2G stage H (Private/World/AbyssWorldStateRules.h): the hero carries its Ashnumb stacks to its actor (the
//   band, mobile low the tint); an NPC the core hides while a story beat that names it is queued or playing (Marl, hidden
//   by M5's turn-in while cs_rf_finale focuses her) keeps its actor drawn, out of the live world, until that beat ends
//   (the director idle, a zone change, or a safety time), and a focus step on it focuses the kept actor.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/WeakInterfacePtr.h"

#include <string>
#include <string_view>

#include "abyss/sim/Events.h"
#include "abyss/sim/Snapshot.h"

#include "Framework/AbyssTypes.h"
#include "Framework/AbyssWorldView.h"
#include "World/AbyssWorldTypes.h"
#include "World/AbyssWorldUi.h"

#include "AbyssWorldBuilder.generated.h"

class AActor;
class AAbyssCameraRig;
/** The NPCs a story beat still needs (Private/World/AbyssWorldBuilder.cpp over AbyssWorldStateRules::FStoryNpcHolds). */
struct FAbyssStoryNpcHolds;
class AAbyssCharacterActor;
class AAbyssPropActor;
class AAbyssZoneActor;
class APlayerController;
class UAbyssAssetLibrary;
class UAbyssAudioSystem;
class UAbyssGameInstance;
class UAbyssVfxSystem;
class UMaterialInterface;
class USkeletalMeshComponent;
class UStaticMeshComponent;
struct FAbyssArtAsset;
struct FAbyssCharacterSetup;
struct FAbyssUserSettings;

namespace abyss
{
	class DataStore;
}

UCLASS()
class ABYSSFIRE_API UAbyssWorldBuilder : public UWorldSubsystem, public IAbyssWorldView
{
	GENERATED_BODY()

public:
	static UAbyssWorldBuilder* Get(const UObject* WorldContextObject);

	// ---- USubsystem ----
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	// ---- IAbyssWorldView ----
	virtual void BuildZone(const abyss::Snapshot& Snap) override;
	virtual void ClearZone() override;
	virtual void SpawnEntity(const abyss::EvEntitySpawned& Event, const abyss::Snapshot& Snap) override;
	virtual void DespawnEntity(const abyss::EvEntityDespawned& Event) override;
	virtual void SyncFrame(const abyss::Snapshot& Snap, const FAbyssFrameInfo& Frame) override;
	virtual double GetGroundHeight(double WorldX, double WorldY) const override;

	// ---- ground picking (input agent, ue58-platform.md 8.4) ----
	/** Ray vs the terrain height field (Z = 0 plane before a zone exists). Direction need not be normalised. */
	bool RaycastGround(const FVector& Origin, const FVector& Direction, FVector& OutHit) const;
	/** Viewport pixel -> ground point and fractional tile (world-map-nav.md 1.4). False above the horizon / off screen. */
	bool PickGround(const APlayerController& Controller, const FVector2D& ScreenPosition, FVector& OutWorld,
		abyss::Vec2& OutTile) const;

	// ---- world UI (UI agent) ----
	/**
	 * The UI agent's world-anchored widget layer (nameplates, HP bars, labels, floating combat text). Replays the
	 * current widgets. When the UI root object also implements IAbyssWorldUi it is found automatically.
	 */
	void SetWorldUi(IAbyssWorldUi* Ui);
	IAbyssWorldUi* GetWorldUi() const;

	// ---- queries ----
	AAbyssCameraRig* GetCameraRig() const { return CameraRig; }
	AAbyssZoneActor* GetZoneActor() const { return ZoneActor; }
	AAbyssCharacterActor* FindCharacter(abyss::EntityId Id) const;
	AAbyssPropActor* FindProp(abyss::EntityId Id) const;
	const FAbyssQualityProfile& GetQualityProfile() const { return Quality; }

	// ---- corpse layer (dark remake U3, UAbyssCorpseLayer) ----
	/**
	 * Hands the dying actor of monster Id over to the corpse layer when it holds its Death pose for it (removed from the
	 * entity maps and the selection rims; its registry entry and nameplate go with the core's despawn). nullptr when it
	 * does not (its death fades as today).
	 */
	AAbyssCharacterActor* ReleaseCorpseActor(abyss::EntityId Id);
	/**
	 * A husk that did not die here (a map seed, a late view): an unregistered monster actor of DefId / ArtId at tile Pos
	 * facing YawDeg, set up to hold its Death pose (the caller poses it). nullptr when its art draws no husk.
	 */
	AAbyssCharacterActor* SpawnHuskActor(const std::string& DefId, const std::string& ArtId, const abyss::Vec2& Pos, float YawDeg);

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	struct FTrack
	{
		FVector2D Prev = FVector2D::ZeroVector;
		FVector2D Last = FVector2D::ZeroVector;
		bool bValid = false;
	};

	struct FLeaving
	{
		TWeakObjectPtr<AActor> Actor;
		FVector Start = FVector::ZeroVector;
		float ElapsedSec = 0.f;
		float FlySec = 0.f;   // > 0: the prop flies to the hero's chest (loot pickup, quest items)
	};

	struct FWidgetEntry
	{
		FAbyssWorldWidgetDesc Desc;
		TWeakObjectPtr<AActor> Actor;
	};

	/** A playing echo (MI_AF_Echo): its figure components and the timeline it fades along. */
	struct FEchoPlayback
	{
		TWeakObjectPtr<AActor> Actor;
		TArray<TWeakObjectPtr<USkeletalMeshComponent>> Figures;
		float ElapsedMs = 0.f;
		double FadeInMs = 450.0;
		double HoldMs = 2100.0;
		double FadeOutMs = 450.0;
		double FlickerAmplitude = 0.08;
		double FlickerHz = 4.0;
	};

	// ---- setup ----
	UAbyssGameInstance* GetAbyssGameInstance() const;
	const abyss::DataStore* GetData() const;
	UAbyssAssetLibrary* GetAssets() const;
	void BindEvents();
	void UnbindEvents();
	/** U4: OnCuePlayed of the audio system (a game-instance subsystem), bound once it exists. */
	void BindAudioCues();
	void ApplySettings(const FAbyssUserSettings& Settings);
	void RefreshQuality();
	void EnsureCameraRig();
	void EnsureHelpers();

	// ---- entities ----
	AAbyssCharacterActor* SpawnCharacter(const abyss::EvEntitySpawned& Event, const abyss::Snapshot& Snap);
	/** Quality: ground items the snapshot does not list yet (EvLootDropped arrives before the next snapshot). */
	AAbyssPropActor* SpawnProp(abyss::EntityId Id, abyss::EntityKind Kind, const std::string& DefId, const std::string& ArtId,
		const abyss::Vec2& Pos, const abyss::Snapshot* Snap, abyss::ItemQuality Quality = abyss::ItemQuality::Normal);
	/** Theme: the presentation theme of the zone the entity is in (PresentationThemeOf), NAME_None = today's chain. */
	const FAbyssArtAsset* ResolveCharacterArt(abyss::EntityKind Kind, const std::string& DefId, const std::string& ArtId,
		FName Theme = NAME_None) const;
	/** The map's presentation theme (`@theme` suffix of its art ids), NAME_None for maps without one or unknown ids. */
	FName PresentationThemeOf(const std::string& MapId) const;
	void ApplyHeroEquipment(const abyss::Snapshot& Snap);
	void UpdateElite(AAbyssCharacterActor& Actor, const abyss::MonsterView& Monster, FName Theme);
	/** The rig-driven part of a character setup (view, anim preset durations, flight, spirit colour, shadow casting). */
	void FillRigSetup(abyss::AnimRig Rig, FAbyssCharacterSetup& Setup) const;
	/** 2G (U3): bCorpseHold / CorpseHoldFromMs / HuskTopCm of a monster setup (UAbyssCorpseLayer::CorpseHoldFor). */
	void ApplyCorpseHold(const std::string& DefId, FAbyssCharacterSetup& Setup);
	void RemoveEntity(abyss::EntityId Id, bool bImmediate, abyss::DespawnReason Reason);
	void DestroyAllEntities(bool bKeepHero);
	void ReconcileMarkers(const abyss::Snapshot& Snap);
	void AddWidget(abyss::EntityId Id, EAbyssWorldWidgetKind Kind, abyss::EntityKind EntityKind, const FString& DefId, AActor* Actor);
	void RemoveWidget(abyss::EntityId Id);
	void HandleCharacterNotify(AAbyssCharacterActor* Actor, FName Notify);

	// ---- per frame ----
	FVector GroundAt(const abyss::Vec2& Tile) const;
	FVector2D TrackedTile(abyss::EntityId Id, const abyss::Vec2& Current, double Alpha, float& OutSpeedTilesPerSec);
	void PresentCharacters(const abyss::Snapshot& Snap, const FAbyssFrameInfo& Frame, float VisualDelta, float RealDelta);
	void PresentProps(const abyss::Snapshot& Snap, const FAbyssFrameInfo& Frame, float VisualDelta);
	void UpdateLeaving(float VisualDelta);
	void UpdateCamera(const abyss::Snapshot& Snap, float RealDelta);
	void UpdateIndicators(const abyss::Snapshot& Snap, float RealDelta);
	/** Selection rims (DECISIONS R16): hero, attack target, hovered monster; the rest None. */
	void UpdateSelectionRims(const abyss::Snapshot& Snap);
	void UpdateLightingCollection(float RealDelta);
	/** The zone's mood at the hero (zone actor), the ambience loop swap and the mood light scale for the VFX system. */
	void UpdateZoneMood(const abyss::Snapshot& Snap);
	/** (Re)starts the ambience loop when the zone actor's recipe or tint differs from the one playing. */
	void RefreshAmbience(const abyss::Snapshot& Snap);
	/**
	 * Echo lore (U1): the figures of MI_AF_Echo.figures[LoreId] at the echo's tile, ghosted, frozen in their pose; when a
	 * set piece poses the echo (U4) they stand by that piece, turned with it.
	 */
	void SpawnEcho(const std::string& LoreId);
	/** Echo figures' fade timeline (real time: the lore text panel may freeze the world); removes finished echoes. */
	void UpdateEchoes(float RealDelta);
	void DestroyEchoes();
	void UpdateWorldWidgets();

	// ---- events ----
	void HandleCameraShake(const abyss::EvCameraShake& Event);
	void HandleCameraFlash(const abyss::EvCameraFlash& Event);
	void HandleZone(const abyss::EvZone& Event);
	void HandleHeroDied(const abyss::EvHeroDied& Event);
	void HandleHeroRespawned(const abyss::EvHeroRespawned& Event);
	void HandleLevelUp(const abyss::EvLevelUp& Event);
	void HandleLootDropped(const abyss::EvLootDropped& Event);
	void HandlePotionDropped(const abyss::EvPotionDropped& Event);
	void HandleFloatingText(const abyss::EvFloatingText& Event);
	void HandleHit(const abyss::EvHit& Event);
	void HandleDodge(const abyss::EvDodgeStarted& Event);
	void HandleStoryStep(const abyss::EvStoryStep& Event);
	void HandleStoryBeat(const abyss::EvStoryBeat& Event);
	void HandleStoryDecorFocus(const abyss::EvStoryDecorFocus& Event);
	void HandleEquipment(const abyss::EvEquipmentChanged& Event);
	void HandlePet(const abyss::EvPet& Event);
	void HandleRenamed(const abyss::EvMonsterRenamed& Event);
	void HandleEmbersGained(const abyss::EvEmbersGained& Event);
	void HandleLoreCollected(const abyss::EvLoreCollected& Event);
	/** 2G stage H: a turn-in / an accept expects the cutscenes its story triggers queue (their NPCs are held). */
	void HandleQuestUpdate(const abyss::EvQuestUpdate& Event);
	void HandleSessionEnded();

	// ---- NPCs a story beat still needs (2G stage H) ----
	/** EvEntityDespawned{Npc, Removed} of an NPC a pending / playing beat names: keep its actor drawn. */
	bool HoldNpcForStory(abyss::EntityId Id);
	/** Releases (destroys) the held NPCs no beat needs any more; bAll: every one (zone change, session end). */
	void ReleaseHeldNpcs(bool bAll);
	/** The held actor of an NPC id, else nullptr. */
	AAbyssCharacterActor* FindHeldNpc(std::string_view NpcId) const;
	void HandleVfxShake(float DurationMs, float Intensity);
	/** U4: a played audio cue ripples the set-piece sockets that listen for it (the bells). */
	void HandleCuePlayed(FName CueId);

	UPROPERTY(Transient)
	TObjectPtr<AAbyssZoneActor> ZoneActor;

	UPROPERTY(Transient)
	TObjectPtr<AAbyssCameraRig> CameraRig;

	UPROPERTY(Transient)
	TMap<uint32, TObjectPtr<AAbyssCharacterActor>> Characters;

	UPROPERTY(Transient)
	TMap<uint32, TObjectPtr<AAbyssPropActor>> Props;

	/** 2G stage H: NPC actors the core hid while a story beat names them (out of Characters and the registry). */
	UPROPERTY(Transient)
	TMap<uint32, TObjectPtr<AAbyssCharacterActor>> HeldNpcs;

	/** Owner of the target ring and the guide arrow. */
	UPROPERTY(Transient)
	TObjectPtr<AActor> HelperActor;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> TargetRing;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> GuideArrow;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> GhostMaterial;

	TArray<FLeaving> Leaving;
	TMap<uint32, FTrack> Tracks;
	TMap<uint32, FWidgetEntry> Widgets;
	/** Ids of props created by ReconcileMarkers (no spawn event): removed when they leave the snapshot. */
	TSet<uint32> ReconciledProps;
	/** Characters given a selection rim last frame (UpdateSelectionRims resets the ones that lose it). */
	TSet<uint32> RimmedCharacters;
	/** Number placement slot of the last hit on an entity this frame (EvHit before EvFloatingText). */
	TMap<uint32, abyss::HitNumberSlot> HitSlots;
	uint64 HitSlotsFrame = 0;
	TArray<int32> ZoneVfxHandles;
	/** The ambience loop playing (U1: swapped when the mood's ambience recipe or tint changes). */
	int32 AmbienceHandle = 0;
	FName PlayingAmbience;
	bool bPlayingAmbienceColor = false;
	FLinearColor PlayingAmbienceColor = FLinearColor::White;
	TArray<FEchoPlayback> Echoes;
	/** 2G stage H: the beats a quest event queued and the NPCs they name (AbyssWorldStateRules::FStoryNpcHolds). */
	TSharedPtr<FAbyssStoryNpcHolds> StoryNpcHolds;
	/** Brightness setting as an exposure bias (EV), pushed to every zone actor. */
	float BrightnessEv = 0.f;
	TWeakInterfacePtr<IAbyssWorldUi> WorldUi;
	FAbyssQualityProfile Quality;
	FName FocusedStoryDecor;
	FDelegateHandle SettingsHandle;
	FDelegateHandle VfxShakeHandle;
	FDelegateHandle CueHandle;
	TWeakObjectPtr<UAbyssAudioSystem> CueAudio;
	FAbyssFrameInfo LastFrame;
	double TimeSec = 0.0;
	float LightingRefreshSec = 0.f;
	float ZoneFadeSec = 0.4f;
	/** Frames left after a zone build whose end waits for pending shader / asset compilation (uncooked runs). */
	int32 CompileWaitFrames = 0;
	bool bZoneBuilt = false;
	bool bEventsBound = false;
	bool bRegistered = false;
	bool bCameraShakeEnabled = true;
	bool bDamageNumbers = true;
	bool bStoryFocusActive = false;
	/** MPC_AF_Lighting diagnostics: the first successful write and the first miss are logged once. */
	bool bLightingWriteLogged = false;
	bool bLightingMissLogged = false;
	abyss::EntityId StoryFocusEntity = abyss::kNoEntity;
};

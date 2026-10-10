// UAbyssWorldBuilder: the game world's IAbyssWorldView (ARCHITECTURE 6, README "Implementing the interfaces").
//
// * Zone: spawns an AAbyssZoneActor per zone from the snapshot (terrain, water, outcrops, palisades, decor, camps,
//   exits / sealed gate, story props, sun, post-process), starts the zone's looping VFX (campfires, torches, exits,
//   ambience) and the asset-library zone retention (web sheetKeepZones: 2 desktop, 1 touch).
// * Entities: one AAbyssCharacterActor per hero / monster / NPC / pet / escort, one AAbyssPropActor per ground item,
//   potion, quest marker, lore pickup, hidden reward, event prop, soul echo, defend target; all registered in
//   UAbyssActorRegistry (the driver routes IAbyssPresenter events, the input agent picks through it). Markers that the
//   core never announced with EvEntitySpawned are reconciled from the snapshot every frame.
// * Per frame (SyncFrame, before the UI root): interpolated transforms (Lerp(prevPos, pos, alpha), S1), facing, speed,
//   statuses; camera follow / shake / fades / story focus (AAbyssCameraRig); occlusion fade (R1); the target ring and
//   quest guide arrow; MPC_AF_Lighting; the VFX system tick; world widget placements for the UI (IAbyssWorldUi).
// * Camera / feel events: EvCameraShake (honours the camera-shake setting), EvCameraFlash, zone transition fades, hero
//   death / respawn fades, level-up flash + shake + zoom pulse, legendary drop flash, story focus / shake / flash steps,
//   dodge afterimages, equipment weapon meshes (I6).
// * Ground picking for the input agent: RaycastGround / PickGround (ray vs the terrain height field, no collision,
//   ue58-platform.md 8.4).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/WeakInterfacePtr.h"

#include <string>

#include "abyss/sim/Events.h"
#include "abyss/sim/Snapshot.h"

#include "Framework/AbyssTypes.h"
#include "Framework/AbyssWorldView.h"
#include "World/AbyssWorldTypes.h"
#include "World/AbyssWorldUi.h"

#include "AbyssWorldBuilder.generated.h"

class AActor;
class AAbyssCameraRig;
class AAbyssCharacterActor;
class AAbyssPropActor;
class AAbyssZoneActor;
class APlayerController;
class UAbyssAssetLibrary;
class UAbyssGameInstance;
class UAbyssVfxSystem;
class UMaterialInterface;
class UStaticMeshComponent;
struct FAbyssArtAsset;
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

	// ---- setup ----
	UAbyssGameInstance* GetAbyssGameInstance() const;
	const abyss::DataStore* GetData() const;
	UAbyssAssetLibrary* GetAssets() const;
	void BindEvents();
	void UnbindEvents();
	void ApplySettings(const FAbyssUserSettings& Settings);
	void RefreshQuality();
	void EnsureCameraRig();
	void EnsureHelpers();

	// ---- entities ----
	AAbyssCharacterActor* SpawnCharacter(const abyss::EvEntitySpawned& Event, const abyss::Snapshot& Snap);
	/** Quality: ground items the snapshot does not list yet (EvLootDropped arrives before the next snapshot). */
	AAbyssPropActor* SpawnProp(abyss::EntityId Id, abyss::EntityKind Kind, const std::string& DefId, const std::string& ArtId,
		const abyss::Vec2& Pos, const abyss::Snapshot* Snap, abyss::ItemQuality Quality = abyss::ItemQuality::Normal);
	const FAbyssArtAsset* ResolveCharacterArt(abyss::EntityKind Kind, const std::string& DefId, const std::string& ArtId) const;
	void ApplyHeroEquipment(const abyss::Snapshot& Snap);
	void UpdateElite(AAbyssCharacterActor& Actor, const abyss::MonsterView& Monster);
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
	void UpdateLightingCollection(float RealDelta);
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
	void HandleSessionEnded();
	void HandleVfxShake(float DurationMs, float Intensity);

	UPROPERTY(Transient)
	TObjectPtr<AAbyssZoneActor> ZoneActor;

	UPROPERTY(Transient)
	TObjectPtr<AAbyssCameraRig> CameraRig;

	UPROPERTY(Transient)
	TMap<uint32, TObjectPtr<AAbyssCharacterActor>> Characters;

	UPROPERTY(Transient)
	TMap<uint32, TObjectPtr<AAbyssPropActor>> Props;

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
	/** Number placement slot of the last hit on an entity this frame (EvHit before EvFloatingText). */
	TMap<uint32, abyss::HitNumberSlot> HitSlots;
	uint64 HitSlotsFrame = 0;
	TArray<int32> ZoneVfxHandles;
	TWeakInterfacePtr<IAbyssWorldUi> WorldUi;
	FAbyssQualityProfile Quality;
	FName FocusedStoryDecor;
	FDelegateHandle SettingsHandle;
	FDelegateHandle VfxShakeHandle;
	FAbyssFrameInfo LastFrame;
	double TimeSec = 0.0;
	float LightingRefreshSec = 0.f;
	float ZoneFadeSec = 0.4f;
	bool bZoneBuilt = false;
	bool bEventsBound = false;
	bool bRegistered = false;
	bool bCameraShakeEnabled = true;
	bool bDamageNumbers = true;
	bool bStoryFocusActive = false;
	abyss::EntityId StoryFocusEntity = abyss::kNoEntity;
};

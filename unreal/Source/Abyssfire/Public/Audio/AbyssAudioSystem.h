// UAbyssAudioSystem: the game's audio player (ARCHITECTURE 6; audio.md 9.6 / 9.7 / 10; DECISIONS A1-A7).
//
// It never decides game rules: the core's AudioDirector / MusicDirector say WHAT plays (EvMusic, EvSfx); this class
// only knows HOW (assets from FAbyssAudioManifest, crossfades, concurrency, panning, volumes, lifecycle).
//
// * Music (EvMusic): two-voice crossfader on persistent 2D UI-sound components. The old track fades out over
//   fadeOutSec, the new one fades in over fadeInSec (linear, audio.md 5.4). A voice that is still fading out when a third
//   request arrives is stopped at once (never orphaned, FIX Q4). restart = false resumes the track where it was last
//   heard (A2: explore after a fight / boss / victory); one-shots (victory) end by themselves. The title menu track is
//   played here on AppState MainMenu (the core has no session there). The manifest's zoneOverrides swap a track key
//   for the current zone (audio.md 11: the Ember Tower plays the procedural plains score).
// * SFX (EvSfx): per-cue concurrency (max voices, stop oldest, minimum retrigger), random variant without immediate
//   repeat (audio.md 9.5), monster family variants (A7, the source's animCategory) plus the family death vocal on
//   monster_death. World cues (EvSfx::spatial) get the A4 mild panning: the source's lateral offset from the hero on
//   screen, scaled by the 25 % spread, becomes the azimuth of a virtual emitter next to the listener (camera), with no
//   distance attenuation. UI cues are 2D.
// * Story (audio.md 10.2): letterbox-in stinger on cutscene / boss intro beats, chapter-card stinger by mood, boss
//   intro title stinger, whisper loop while whisper steps show; music ducks under them.
// * A7: the zone ambience bed (by zone, then by theme) and hero footsteps from the hero actor's FootL / FootR anim
//   notifies with the surface of the tile under the hero (a distance cadence takes over when a clip has no notifies).
// * Volumes: master x bus (music / SFX) from FAbyssUserSettings, applied as component volume multipliers (no sound
//   class assets needed). App background (mobile): everything pauses, one-shots stop; resumed on foreground.
// * Title-menu soundtrack (audio.md 8.3, FIX Q10): packaged tracks with real lengths, real pause and seek, auto-advance;
//   closing it returns to the title theme. Music credits for the credits screen come from the manifest.
//
// Lifetime: game instance subsystem (survives the session cycle; components live in the one game world L_Main).
#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"

#include "abyss/data/AudioData.h"
#include "abyss/sim/Events.h"
#include "abyss/sim/Snapshot.h"

#include "Audio/AbyssAudioManifest.h"
#include "Framework/AbyssTypes.h"

#include "AbyssAudioSystem.generated.h"

class AAbyssCharacterActor;
class UAbyssGameInstance;
class APlayerController;
class UAudioComponent;
class USoundAttenuation;
class USoundBase;

/** A row of the title-menu soundtrack (audio.md 8.3). */
struct FAbyssJukeboxTrack
{
	FName TrackKey;
	/** i18n key of the title (menu.jukebox.track.*): resolve with UAbyssGameInstance::Localize. */
	FString TitleKey;
	/** Real length of the rendered track, seconds. */
	float LengthSec = 0.f;
};

/** A recorded track's licence credit with the track keys that use it (credits screen). */
struct FAbyssMusicCreditRow
{
	FAbyssMusicCredit Credit;
	TArray<FName> Tracks;
};

UCLASS()
class ABYSSFIRE_API UAbyssAudioSystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	static UAbyssAudioSystem* Get(const UObject* WorldContextObject);

	// ---- USubsystem ----
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ---- FTickableGameObject ----
	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override { return false; }
	virtual TStatId GetStatId() const override;
	virtual UWorld* GetTickableGameObjectWorld() const override;

	// ---- API for the UI (UE-owned panels; audio.md 3.3: one click per toggle, FIX Q2) ----
	/** Plays a cue 2D on the SFX volume (click, panel_open, panel_close, error, equip, ...), with its concurrency rules. */
	void PlayUiCue(abyss::SfxId Cue);
	void PlayClick() { PlayUiCue(abyss::SfxId::Click); }
	void PlayPanelOpen() { PlayUiCue(abyss::SfxId::PanelOpen); }
	void PlayPanelClose() { PlayUiCue(abyss::SfxId::PanelClose); }
	void PlayError() { PlayUiCue(abyss::SfxId::Error); }
	/** Settings panel preview: a short cue at the SFX volume (rate limited). */
	void PreviewSfxVolume();

	// ---- title-menu soundtrack (UI agent; audio.md 8.3, FIX Q10) ----
	/** Rows whose track is packaged (its sound loads), in soundtrack order. Indices below refer to this list. */
	TArray<FAbyssJukeboxTrack> GetJukeboxTracks();
	/** Plays row Index from the start (crossfade from whatever plays); the soundtrack mode starts on the first call. */
	void JukeboxPlay(int32 Index);
	/** Real pause / resume of the current row (not a mute). Resuming a row that ended restarts it. */
	void JukeboxSetPaused(bool bPaused);
	/** Jumps inside the current row (seconds, clamped to its length); keeps the paused state. */
	void JukeboxSeek(float Seconds);
	/** Leaves the soundtrack mode and restarts the title theme (fade-in as on returning to the menu). */
	void JukeboxClose();
	bool IsJukeboxActive() const { return JukeboxIndex != INDEX_NONE; }
	int32 GetJukeboxIndex() const { return JukeboxIndex; }
	bool IsJukeboxPaused() const;
	/** Playback position of the current row and its length, seconds (0 when the soundtrack mode is off). */
	float GetJukeboxPositionSec() const;
	float GetJukeboxLengthSec() const;

	/** Credits of the packaged recorded tracks (title, author, licence, url), deduplicated, manifest order. */
	TArray<FAbyssMusicCreditRow> GetMusicCredits() const;
	/** The track key now playing (None = silence). */
	FName GetCurrentTrack() const { return CurrentTrack; }

	// ---- diagnostics (console abyss.Audio.*) ----
	FString DescribeState() const;
	/** Forces a music request as the core would (debug). */
	void DebugPlayMusic(FName TrackKey, bool bRestart);
	void DebugPlayCue(FName CueId, bool bSpatial);
	const FAbyssAudioManifest& GetManifest() const { return Manifest; }

private:
	// ---- setup ----
	UAbyssGameInstance* GetAbyssGameInstance() const;
	UWorld* GetAudioWorld() const;
	/** Loads the manifest and preloads sounds once the data store exists (deferred from Initialize). */
	void EnsureReady();
	/** Loads every manifest sound once (SFX retain their data; music / beds stream from their headers). */
	void PreloadSounds();
	USoundBase* GetSound(FName AssetName, const TCHAR* FallbackFolder);
	USoundAttenuation* GetSpatialAttenuation();

	// ---- app / session / settings ----
	void HandleAppStateChanged(EAbyssAppState NewState);
	void HandleSessionStarted();
	void HandleSessionEnded();
	void HandleAppWillDeactivate();
	void HandleAppHasReactivated();
	void SetSuspended(bool bSuspend);
	void RefreshVolumes(bool bForce);

	// ---- core events ----
	void HandleMusic(const abyss::EvMusic& Event);
	void HandleSfx(const abyss::EvSfx& Event);
	void HandleEntitySpawned(const abyss::EvEntitySpawned& Event);
	void HandleZone(const abyss::EvZone& Event);
	void HandleStoryBeat(const abyss::EvStoryBeat& Event);
	void HandleStoryStep(const abyss::EvStoryStep& Event);
	void HandleFrame(const abyss::Snapshot& Snap, const FAbyssFrameInfo& Frame);

	// ---- music ----
	struct FMusicVoice
	{
		FName TrackKey;
		float Gain = 1.f;
		double PositionSec = 0.0;
		double DurationSec = 0.0;
		bool bLoop = true;
		bool bActive = false;     // the current track (fading in or playing)
		bool bFadingOut = false;
		bool bPaused = false;     // jukebox pause (the position does not advance)
		double FadeOutRemainingSec = 0.0;
	};
	/** Crossfade to TrackKey ("" / None = silence). StartAt < 0: resume (A2) or 0 when unknown. */
	void PlayMusic(FName TrackKey, float FadeOutSec, float FadeInSec, bool bRestart, bool bLoopRequested);
	void StopAllMusic();
	void TickMusic(float RealDeltaSec);
	int32 FindFreeMusicSlot() const;
	int32 FindActiveMusicVoice() const;
	UAudioComponent* EnsureMusicComponent(int32 Slot, USoundBase* Sound);
	float MusicBusGain() const;
	/** Seconds of one pass through a track: manifest, then the asset's own duration (0 = unknown). */
	double ResolveTrackDuration(const FAbyssMusicTrackDef& Track, USoundBase* Sound) const;
	void RefreshJukeboxRows();
	void JukeboxStart(int32 Index, float FadeOutSec);
	void TickJukebox();
	void EndJukebox();

	// ---- SFX ----
	struct FCueState
	{
		TArray<TWeakObjectPtr<UAudioComponent>> Voices;
		double LastStartSec = -1000.0;
		TMap<FName, int32> LastVariant;   // family (or None) -> last asset index
	};
	/** Returns the spawned component (or nullptr: dropped by concurrency / missing asset). */
	UAudioComponent* PlayCue(const FAbyssAudioCueDef& Cue, FName Family, bool bSpatial, const FVector2D& Tile);
	UAudioComponent* PlayOneShot(USoundBase* Sound, float Volume, bool bSpatial, const FVector2D& Tile);
	FName PickVariant(TArray<FName> const& Candidates, FCueState& State, FName Family);
	FName FamilyOfEntity(abyss::EntityId Id) const;
	bool ComputeVirtualEmitter(const FVector2D& Tile, FVector& OutLocation) const;
	APlayerController* GetListenerController() const;
	float SfxBusGain() const;
	void StopAllSfx();

	// ---- story stingers / duck ----
	void StartStinger(FName Asset, const FAbyssDuckDef* Duck, float FadeInSec);
	void StopStinger(float FadeOutSec);
	void StartWhisper();
	void StopWhisper();
	void SetDuck(const FAbyssDuckDef& Duck);
	void ReleaseDuck(const FAbyssDuckDef& Duck);
	void TickDuck(float RealDeltaSec);
	void EndStoryAudio();

	// ---- ambience ----
	void StartAmbience(FName Asset);
	void StopAmbience(float FadeOutSec);
	float AmbienceGain() const;

	// ---- footsteps ----
	void BindHero(const abyss::Snapshot& Snap);
	void HandleHeroNotify(AAbyssCharacterActor* Actor, FName Notify);
	void PlayFootstep();
	void TickFootstepFallback(const abyss::Snapshot& Snap, float RealDeltaSec);
	FName SurfaceUnderHero() const;

	// ---- state ----
	FAbyssAudioManifest Manifest;
	bool bReady = false;
	bool bSuspended = false;
	bool bLifecycleBound = false;
	bool bPreloaded = false;
	FRandomStream Random;
	double NowSec = 0.0;  // real seconds since Initialize (timers)

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<USoundBase>> Sounds;
	TSet<FName> MissingSounds;

	UPROPERTY(Transient)
	TObjectPtr<USoundAttenuation> SpatialAttenuation;

	// Music: two crossfading voices.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> MusicComponents;
	TArray<FMusicVoice> MusicVoices;
	TMap<FName, double> ResumePositions;
	FName CurrentTrack;
	/** Soundtrack mode: the playing row of JukeboxRows (INDEX_NONE = off). */
	int32 JukeboxIndex = INDEX_NONE;
	/** Manifest jukebox rows whose sound is packaged (built once the manifest is loaded). */
	TArray<int32> JukeboxRows;
	bool bJukeboxRowsBuilt = false;

	// Volumes (linear) as last applied.
	float MasterVolume = 1.f;
	float MusicVolume = 0.6f;
	float SfxVolume = 0.8f;
	float AppliedMusicGain = -1.f;

	TMap<FName, FCueState> CueStates;
	/** Monster entity -> family (animCategory), from EvEntitySpawned (deaths can arrive after the snapshot dropped it). */
	TMap<uint32, FName> EntityFamilies;

	// Stingers and duck.
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> StingerComponent;
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> WhisperComponent;
	abyss::StoryBeatKind CurrentBeatKind = abyss::StoryBeatKind::Cutscene;
	bool bBeatActive = false;
	bool bStingerDucking = false;
	bool bWhisperActive = false;
	bool bTitleStinger = false;
	FAbyssDuckDef StingerDuck;
	float DuckTargetDb = 0.f;
	float DuckCurrentDb = 0.f;
	float DuckInSec = 0.5f;
	float DuckOutSec = 0.5f;
	float StingerGain = 1.f;
	float WhisperGain = 1.f;

	// Ambience.
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> AmbienceComponent;
	FName CurrentAmbience;
	float AmbienceAssetGain = 1.f;
	float AmbienceDuckDb = 0.f;
	bool bCinematic = false;

	// Footsteps / hero state from the last frame.
	TWeakObjectPtr<AAbyssCharacterActor> BoundHero;
	FDelegateHandle HeroNotifyHandle;
	bool bHeroMoving = false;
	bool bHeroAlive = true;
	FVector2D HeroTile = FVector2D::ZeroVector;
	FVector2D LastFallbackTile = FVector2D::ZeroVector;
	double FallbackDistanceCm = 0.0;
	double LastFootNotifySec = -1000.0;
	double LastFootstepSec = -1000.0;
	FName HeroSurface;
	int32 LastFootstepVariant = INDEX_NONE;
	double LastPreviewSec = -1000.0;
	bool bInSession = false;
};

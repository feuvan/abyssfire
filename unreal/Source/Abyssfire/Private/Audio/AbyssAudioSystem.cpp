#include "Audio/AbyssAudioSystem.h"

#include "Async/Async.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Stats/Stats.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/Types.h"
#include "abyss/data/DataStore.h"
#include "abyss/data/MonsterData.h"
#include "abyss/data/StoryData.h"
#include "abyss/hero/Hero.h"
#include "abyss/quests/QuestSystem.h"
#include "abyss/world/Grid.h"

#include "Abyssfire.h"
#include "Actors/AbyssCharacterActor.h"
#include "Audio/AbyssAmbienceRules.h"
#include "Audio/AbyssAudioDataRules.h"
#include "Audio/AbyssCueTriggerRules.h"
#include "Audio/AbyssMusicRules.h"
#include "Audio/AbyssVoiceRules.h"
#include "Framework/AbyssActorRegistry.h"
#include "Framework/AbyssEventRouter.h"
#include "Framework/AbyssGameInstance.h"
#include "Framework/AbyssSimDriver.h"
#include "Framework/AbyssText.h"
#include "Framework/AbyssUnits.h"
#include "Platform/AbyssPlatform.h"
#include "Platform/AbyssSettings.h"

namespace AbyssAudioSystemPrivate
{
	constexpr int32 MusicSlots = 2;
	constexpr float MaxTickSec = 0.25f;
	constexpr float VolumeEpsilon = 1e-4f;
	constexpr double PreviewMinIntervalSec = 0.12;
	const TCHAR* const SfxFolder = TEXT("SFX");
	const TCHAR* const MusicFolder = TEXT("Music");
	const TCHAR* const StingerFolder = TEXT("Stingers");
	const TCHAR* const AmbienceFolder = TEXT("Ambience");
	const TCHAR* const FootstepFolder = TEXT("Footsteps");

	FName ToName(std::string_view Utf8)
	{
		return Utf8.empty() ? NAME_None : FName(AbyssText::ToFString(Utf8));
	}

	float DbToLinear(float Db)
	{
		return FMath::Pow(10.f, Db / 20.f);
	}

	/** Applies a volume multiplier only when it changed (each call queues an audio-thread command). */
	void SetVolumeIfChanged(UAudioComponent* Comp, float Volume)
	{
		if (IsValid(Comp) && !FMath::IsNearlyEqual(Comp->VolumeMultiplier, Volume, VolumeEpsilon))
		{
			Comp->SetVolumeMultiplier(Volume);
		}
	}

	bool IsSounding(const UAudioComponent* Comp)
	{
		return IsValid(Comp) && Comp->IsPlaying();
	}

	UAbyssAudioSystem* SystemForWorld(UWorld* InWorld)
	{
		UGameInstance* GI = InWorld ? InWorld->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UAbyssAudioSystem>() : nullptr;
	}

	void ConsoleStatus(const TArray<FString>& Args, UWorld* InWorld)
	{
		if (UAbyssAudioSystem* System = SystemForWorld(InWorld))
		{
			UE_LOG(LogAbyss, Display, TEXT("%s"), *System->DescribeState());
		}
	}

	void ConsoleMusic(const TArray<FString>& Args, UWorld* InWorld)
	{
		UAbyssAudioSystem* System = SystemForWorld(InWorld);
		if (System == nullptr)
		{
			return;
		}
		const FName Key = Args.Num() > 0 ? FName(*Args[0]) : NAME_None;
		const bool bRestart = Args.Num() < 2 || Args[1] != TEXT("resume");
		System->DebugPlayMusic(Key, bRestart);
	}

	void ConsoleCue(const TArray<FString>& Args, UWorld* InWorld)
	{
		UAbyssAudioSystem* System = SystemForWorld(InWorld);
		if (System == nullptr || Args.Num() == 0)
		{
			return;
		}
		System->DebugPlayCue(FName(*Args[0]), Args.Num() > 1 && Args[1] == TEXT("3d"));
	}

	FAutoConsoleCommandWithWorldAndArgs AbyssAudioStatusCommand(TEXT("abyss.Audio.Status"),
		TEXT("Logs the audio system state (music voices, duck, ambience, loaded sounds)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ConsoleStatus));
	FAutoConsoleCommandWithWorldAndArgs AbyssAudioMusicCommand(TEXT("abyss.Audio.Music"),
		TEXT("abyss.Audio.Music <trackKey> [resume]: crossfades to a music track as an EvMusic would (no key = silence)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ConsoleMusic));
	FAutoConsoleCommandWithWorldAndArgs AbyssAudioCueCommand(TEXT("abyss.Audio.Cue"),
		TEXT("abyss.Audio.Cue <cueId> [3d]: plays an SFX cue (e.g. hit, skill_fire, monster_aggro)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ConsoleCue));
}

// =====================================================================================================================
// Dark remake state (U7, implementation-plan.md 4): the rule helpers' tables and their per-session state
// =====================================================================================================================

struct FAbyssAudioRemake
{
	// Tables (manifest voiceFamilies / musicDirector / ambience / cueTriggers), parsed once the manifest is loaded.
	AbyssVoiceRules::VoiceTables Voices;
	AbyssMusicRules::DirectorTables Director;
	AbyssAmbienceRules::AmbienceTables Ambience;
	AbyssCueTriggerRules::TriggerTables Triggers;
	std::vector<AbyssAudioData::TileCircle> AmbienceRegionAreas;
	std::vector<AbyssAudioData::TileCircle> AmbienceLayerAreas;
	/** Track keys the core plays as boss music (music.json bossMusic, the director's bosses). */
	std::vector<std::string> BossScores;
	/** Monster ids a `while` names (music regions, region beds, layers): their liveness is read each frame. */
	std::vector<std::string> WatchedMonsters;

	// Music.
	AbyssMusicRules::Arbiter Arbiter;
	AbyssAudioData::CircleTracker MusicRegions;
	int Region = -1;
	AbyssMusicRules::QuestTrackPick Siege;
	std::string ZoneId;
	std::string PlayingBeat;  // the story beat playing ("" = none)
	abyss::StoryBeatKind PlayingKind = abyss::StoryBeatKind::Cutscene;

	// Ambience: region beds and layers (weights 0..1), emitters (one entry per spot, EmitterComponents parallel).
	AbyssAudioData::CircleTracker AmbienceRegions;
	AbyssAudioData::CircleTracker AmbienceLayers;
	std::vector<double> RegionWeights;
	std::vector<double> LayerWeights;
	/** The asset of each area loop (regions, then layers), converted once with the tables (TickRemakeAmbience). */
	TArray<FName> AreaLoopAssets;
	/** The sounding track as last handed to the arbiter: converted to a std::string only when it changes. */
	FName SoundingName;
	std::string SoundingStd;
	struct FEmitterSpot
	{
		FName Asset;
		double Col = 0.0;
		double Row = 0.0;
		double InnerTiles = 1.0;
		double FalloffTiles = 6.0;
	};
	std::vector<FEmitterSpot> EmitterSpots;
	bool bEmittersDirty = true;
	TMap<FName, TArray<FVector2D>> SetPieceSpots;
	/** Until then a stopped zone's loops finish their fade-out (StopRemakeAmbience) instead of being cut. */
	double FadeOutUntilSec = -1.0;
	/** Between EvZone{TransitionBegan} and {Entered}: no area loop or emitter starts. */
	bool bZoneTransition = false;

	// Cue triggers, voices.
	AbyssCueTriggerRules::Scheduler Scheduler;
	std::string DecorFocus;
	TSet<uint32> EngagedEntities;
	std::vector<std::string> AliveNow;

	bool IsAlive(std::string_view MonsterId) const { return AbyssAudioData::Contains(AliveNow, MonsterId); }

	/** A cutscene or boss intro plays: music layers keep what they chose (the core holds its own changes too). */
	bool IsCutscene() const
	{
		return !PlayingBeat.empty() && (PlayingKind == abyss::StoryBeatKind::Cutscene ||
			PlayingKind == abyss::StoryBeatKind::BossIntro);
	}

	void ResetSession()
	{
		Arbiter.Reset();
		MusicRegions.Reset();
		Region = -1;
		Siege = AbyssMusicRules::QuestTrackPick{};
		ZoneId.clear();
		PlayingBeat.clear();
		AmbienceRegions.Reset();
		AmbienceLayers.Reset();
		RegionWeights.assign(Ambience.regions.size(), 0.0);
		LayerWeights.assign(Ambience.layers.size(), 0.0);
		EmitterSpots.clear();
		bEmittersDirty = true;
		SetPieceSpots.Reset();
		FadeOutUntilSec = -1.0;
		bZoneTransition = false;
		Scheduler.Reset();
		DecorFocus.clear();
		EngagedEntities.Reset();
		AliveNow.clear();
	}
};

namespace AbyssAudioSystemPrivate
{
	std::string ToStd(FName Name)
	{
		return Name.IsNone() ? std::string() : AbyssText::ToStd(Name.ToString());
	}

	/** The voice tables (empty without a remake state: today's animCategory rule). */
	const AbyssVoiceRules::VoiceTables& VoicesOf(const TSharedPtr<FAbyssAudioRemake>& Remake)
	{
		static const AbyssVoiceRules::VoiceTables Empty;
		return Remake.IsValid() ? Remake->Voices : Empty;
	}

	AbyssMusicRules::QuestPhase PhaseOf(const abyss::QuestSystem* Quests, std::string_view QuestId)
	{
		if (Quests == nullptr || QuestId.empty())
		{
			return AbyssMusicRules::QuestPhase::None;
		}
		bool bHasRecord = false;
		const abyss::QuestStatus Status = Quests->StatusOf(QuestId, bHasRecord);
		if (!bHasRecord)
		{
			return AbyssMusicRules::QuestPhase::None;
		}
		switch (Status)
		{
		case abyss::QuestStatus::Active:
			return AbyssMusicRules::QuestPhase::Active;
		case abyss::QuestStatus::Completed:
			return AbyssMusicRules::QuestPhase::Completed;
		default:
			return AbyssMusicRules::QuestPhase::None;
		}
	}
}

// =====================================================================================================================
// Setup
// =====================================================================================================================

UAbyssAudioSystem* UAbyssAudioSystem::Get(const UObject* WorldContextObject)
{
	if (WorldContextObject == nullptr)
	{
		return nullptr;
	}
	if (const UGameInstance* GI = Cast<UGameInstance>(WorldContextObject))
	{
		return GI->GetSubsystem<UAbyssAudioSystem>();
	}
	const UWorld* ContextWorld = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject,
		EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GI = ContextWorld ? ContextWorld->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UAbyssAudioSystem>() : nullptr;
}

bool UAbyssAudioSystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer) || IsRunningDedicatedServer())
	{
		return false;
	}
	return Outer != nullptr && Outer->IsA<UAbyssGameInstance>();
}

void UAbyssAudioSystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Random.Initialize(0x0A0D10);
	Remake = MakeShared<FAbyssAudioRemake>();
	MusicVoices.SetNum(AbyssAudioSystemPrivate::MusicSlots);
	MusicComponents.SetNum(AbyssAudioSystemPrivate::MusicSlots);

	UAbyssGameInstance* GI = GetAbyssGameInstance();
	check(GI != nullptr);
	// The data store and settings load after the subsystems initialise (UAbyssGameInstance::Init calls Super::Init
	// first): the manifest is read on the first tick / app-state change that finds the data ready (EnsureReady).
	FAbyssEventRouter& Router = GI->GetEventRouter();
	Router.On<abyss::EvMusic>().AddUObject(this, &UAbyssAudioSystem::HandleMusic);
	Router.On<abyss::EvSfx>().AddUObject(this, &UAbyssAudioSystem::HandleSfx);
	Router.On<abyss::EvEntitySpawned>().AddUObject(this, &UAbyssAudioSystem::HandleEntitySpawned);
	Router.On<abyss::EvZone>().AddUObject(this, &UAbyssAudioSystem::HandleZone);
	Router.On<abyss::EvStoryBeat>().AddUObject(this, &UAbyssAudioSystem::HandleStoryBeat);
	Router.On<abyss::EvStoryStep>().AddUObject(this, &UAbyssAudioSystem::HandleStoryStep);
	// U7 (dark remake): named voices on anim / dialogue beats, encounter stingers, bells on world events.
	Router.On<abyss::EvPlayAnim>().AddUObject(this, &UAbyssAudioSystem::HandlePlayAnim);
	Router.On<abyss::EvMiniBossDialogue>().AddUObject(this, &UAbyssAudioSystem::HandleMiniBossDialogue);
	Router.On<abyss::EvRandomEvent>().AddUObject(this, &UAbyssAudioSystem::HandleRandomEvent);
	Router.On<abyss::EvStoryDecorFocus>().AddUObject(this, &UAbyssAudioSystem::HandleStoryDecorFocus);
	Router.OnFrame.AddUObject(this, &UAbyssAudioSystem::HandleFrame);
	Router.OnSessionStarted.AddUObject(this, &UAbyssAudioSystem::HandleSessionStarted);
	Router.OnSessionEnded.AddUObject(this, &UAbyssAudioSystem::HandleSessionEnded);
	GI->OnAppStateChanged.AddUObject(this, &UAbyssAudioSystem::HandleAppStateChanged);

	// audio.md 9.7: phones / tablets pause music and SFX in the background (and on deactivation: calls, control centre);
	// desktop keeps playing (the unfocused volume multiplier of [Audio] mutes it, see Audio/README.md).
	if (!bLifecycleBound)
	{
		FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddUObject(this, &UAbyssAudioSystem::HandleAppWillDeactivate);
		FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddUObject(this, &UAbyssAudioSystem::HandleAppHasReactivated);
		if (AbyssPlatform::IsMobilePlatform())
		{
			FCoreDelegates::ApplicationWillDeactivateDelegate.AddUObject(this, &UAbyssAudioSystem::HandleAppWillDeactivate);
			FCoreDelegates::ApplicationHasReactivatedDelegate.AddUObject(this, &UAbyssAudioSystem::HandleAppHasReactivated);
		}
		bLifecycleBound = true;
	}
}

void UAbyssAudioSystem::Deinitialize()
{
	if (UAbyssGameInstance* GI = GetAbyssGameInstance())
	{
		GI->GetEventRouter().RemoveAll(this);
		GI->OnAppStateChanged.RemoveAll(this);
	}
	if (bLifecycleBound)
	{
		FCoreDelegates::ApplicationWillEnterBackgroundDelegate.RemoveAll(this);
		FCoreDelegates::ApplicationHasEnteredForegroundDelegate.RemoveAll(this);
		FCoreDelegates::ApplicationWillDeactivateDelegate.RemoveAll(this);
		FCoreDelegates::ApplicationHasReactivatedDelegate.RemoveAll(this);
		bLifecycleBound = false;
	}
	if (AAbyssCharacterActor* Hero = BoundHero.Get())
	{
		Hero->OnCharacterNotify.Remove(HeroNotifyHandle);
	}
	BoundHero.Reset();
	StopAllSfx();
	StopAllMusic();
	StopRemakeAmbience(0.f);
	for (UAudioComponent* Comp : { StingerComponent.Get(), WhisperComponent.Get(), AmbienceComponent.Get(),
			 EncounterStingerComponent.Get() })
	{
		if (IsValid(Comp))
		{
			Comp->Stop();
		}
	}
	StingerComponent = nullptr;
	WhisperComponent = nullptr;
	AmbienceComponent = nullptr;
	EncounterStingerComponent = nullptr;
	AreaLoopComponents.Reset();
	EmitterComponents.Reset();
	Remake.Reset();
	MusicComponents.Reset();
	Sounds.Reset();
	bReady = false;
	Super::Deinitialize();
}

UAbyssGameInstance* UAbyssAudioSystem::GetAbyssGameInstance() const
{
	return Cast<UAbyssGameInstance>(GetGameInstance());
}

UWorld* UAbyssAudioSystem::GetAudioWorld() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetWorld() : nullptr;
}

void UAbyssAudioSystem::EnsureReady()
{
	if (bReady)
	{
		return;
	}
	const UAbyssGameInstance* GI = GetAbyssGameInstance();
	if (GI == nullptr || !GI->IsDataReady() || GI->GetData() == nullptr)
	{
		return;
	}
	FString Source;
	Manifest.Load(&GI->GetData()->Audio(), Source);
	UE_LOG(LogAbyss, Log, TEXT("Audio manifest: %s (%d assets, %d cues, %d tracks)"), *Source, Manifest.GetAssets().Num(),
		Manifest.GetCues().Num(), Manifest.GetTracks().Num());
	bReady = true;
	BuildRemakeTables();
	RefreshVolumes(/*bForce*/ true);
}

void UAbyssAudioSystem::PreloadSounds()
{
	if (bPreloaded || !bReady)
	{
		return;
	}
	bPreloaded = true;
	const double Start = FPlatformTime::Seconds();
	int32 Loaded = 0;
	for (const TPair<FName, FAbyssAudioAssetDef>& Pair : Manifest.GetAssets())
	{
		if (GetSound(Pair.Key, AbyssAudioSystemPrivate::SfxFolder) != nullptr)
		{
			++Loaded;
		}
	}
	if (!Manifest.IsFromFile())
	{
		// Without a manifest the core's cue table lists the SFX.
		for (const TPair<FName, FAbyssAudioCueDef>& Pair : Manifest.GetCues())
		{
			for (const FName Asset : Pair.Value.Assets)
			{
				Loaded += GetSound(Asset, AbyssAudioSystemPrivate::SfxFolder) != nullptr ? 1 : 0;
			}
		}
	}
	UE_LOG(LogAbyss, Log, TEXT("Audio: %d sounds loaded (%d missing) in %.0f ms"), Loaded, MissingSounds.Num(),
		(FPlatformTime::Seconds() - Start) * 1000.0);
}

USoundBase* UAbyssAudioSystem::GetSound(FName AssetName, const TCHAR* FallbackFolder)
{
	if (AssetName.IsNone())
	{
		return nullptr;
	}
	if (const TObjectPtr<USoundBase>* Found = Sounds.Find(AssetName))
	{
		if (IsValid(*Found))
		{
			return *Found;
		}
	}
	if (MissingSounds.Contains(AssetName))
	{
		return nullptr;
	}
	const FAbyssAudioAssetDef Def = Manifest.ResolveAsset(AssetName, FallbackFolder);
	USoundBase* Sound = Cast<USoundBase>(Def.Path.TryLoad());
	if (Sound == nullptr)
	{
		MissingSounds.Add(AssetName);
		UE_LOG(LogAbyss, Warning, TEXT("Audio: sound %s not found at %s (run unreal/Audio/ue/import_audio.py)"),
			*AssetName.ToString(), *Def.Path.ToString());
		return nullptr;
	}
	Sounds.Add(AssetName, Sound);
	return Sound;
}

USoundAttenuation* UAbyssAudioSystem::GetSpatialAttenuation()
{
	if (!IsValid(SpatialAttenuation))
	{
		// A4: panning only. No distance attenuation, no air absorption, no focus / occlusion / reverb sends: the virtual
		// emitter (ComputeVirtualEmitter) carries only the direction.
		SpatialAttenuation = NewObject<USoundAttenuation>(this, TEXT("AbyssSpatialPanning"));
		FSoundAttenuationSettings& Settings = SpatialAttenuation->Attenuation;
		Settings.bAttenuate = false;
		Settings.bSpatialize = true;
		Settings.bAttenuateWithLPF = false;
		Settings.bEnableListenerFocus = false;
		Settings.bEnableOcclusion = false;
		Settings.bEnableReverbSend = false;
	}
	return SpatialAttenuation;
}

// =====================================================================================================================
// Tick
// =====================================================================================================================

ETickableTickType UAbyssAudioSystem::GetTickableTickType() const
{
	return HasAnyFlags(RF_ClassDefaultObject) ? ETickableTickType::Never : ETickableTickType::Conditional;
}

bool UAbyssAudioSystem::IsTickable() const
{
	return !HasAnyFlags(RF_ClassDefaultObject) && GetGameInstance() != nullptr;
}

TStatId UAbyssAudioSystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAbyssAudioSystem, STATGROUP_Tickables);
}

UWorld* UAbyssAudioSystem::GetTickableGameObjectWorld() const
{
	return GetAudioWorld();
}

void UAbyssAudioSystem::Tick(float DeltaTime)
{
	// Fades, duck and positions run on undilated real time (S6 slow motion does not stretch the music).
	const float RealDelta = FMath::Clamp(static_cast<float>(FApp::GetDeltaTime()), 0.f, AbyssAudioSystemPrivate::MaxTickSec);
	(void)DeltaTime;
	NowSec += RealDelta;
	EnsureReady();
	if (!bReady)
	{
		return;
	}
	RefreshVolumes(/*bForce*/ false);
	TickMusic(RealDelta);
	TickJukebox();
	TickDuck(RealDelta);

	// Ambience: cinematic duck (A7 bed under story beats) + volume.
	const float TargetDb = bCinematic ? Manifest.Ambience.CinematicDuckDb : 0.f;
	const float Step = RealDelta * 12.f;  // dB per second
	AmbienceDuckDb = AmbienceDuckDb < TargetDb ? FMath::Min(TargetDb, AmbienceDuckDb + Step)
		: FMath::Max(TargetDb, AmbienceDuckDb - Step);
	AbyssAudioSystemPrivate::SetVolumeIfChanged(AmbienceComponent, AmbienceGain());
}

// =====================================================================================================================
// App state, session, settings, lifecycle
// =====================================================================================================================

void UAbyssAudioSystem::HandleAppStateChanged(EAbyssAppState NewState)
{
	EnsureReady();
	switch (NewState)
	{
	case EAbyssAppState::MainMenu:
	{
		// audio.md 10.4 rule 1: the title theme restarts on every return to the menu (fade-in 1.0 s).
		PreloadSounds();
		EndStoryAudio();
		EndJukebox();
		StopAmbience(Manifest.Ambience.FadeOutSec);
		const UAbyssGameInstance* GI = GetAbyssGameInstance();
		const abyss::DataStore* Data = GI ? GI->GetData() : nullptr;
		const float FadeOut = Data ? static_cast<float>(Data->Audio().music.zoneFadeSec) : 2.f;
		const float FadeIn = Data ? static_cast<float>(Data->Audio().music.fadeInSec) : 1.f;
		PlayMusic(Manifest.MenuTrack, FadeOut, FadeIn, /*bRestart*/ true, /*bLoopRequested*/ true);
		break;
	}
	case EAbyssAppState::InGame:
		PreloadSounds();
		EndJukebox();  // the session's EvMusic owns the music from here
		break;
	case EAbyssAppState::Boot:
	case EAbyssAppState::DataError:
	default:
		EndJukebox();
		StopAllMusic();
		break;
	}
}

void UAbyssAudioSystem::HandleSessionStarted()
{
	bInSession = true;
	EntityDefs.Reset();
	CueStates.Reset();
	ResetRemakeSession();
	LastFootNotifySec = -1000.0;
	FallbackDistanceCm = 0.0;
}

void UAbyssAudioSystem::HandleSessionEnded()
{
	bInSession = false;
	StopAllSfx();
	EndStoryAudio();
	StopAmbience(Manifest.Ambience.FadeOutSec);
	StopRemakeAmbience(Manifest.Ambience.FadeOutSec);
	ResetRemakeSession();
	if (AAbyssCharacterActor* Hero = BoundHero.Get())
	{
		Hero->OnCharacterNotify.Remove(HeroNotifyHandle);
	}
	BoundHero.Reset();
	HeroNotifyHandle.Reset();
	EntityDefs.Reset();
	bCinematic = false;
}

void UAbyssAudioSystem::HandleAppWillDeactivate()
{
	if (!IsInGameThread())
	{
		AsyncTask(ENamedThreads::GameThread, [WeakThis = TWeakObjectPtr<UAbyssAudioSystem>(this)]()
		{
			if (UAbyssAudioSystem* Self = WeakThis.Get())
			{
				Self->SetSuspended(true);
			}
		});
		return;
	}
	SetSuspended(true);
}

void UAbyssAudioSystem::HandleAppHasReactivated()
{
	if (!IsInGameThread())
	{
		AsyncTask(ENamedThreads::GameThread, [WeakThis = TWeakObjectPtr<UAbyssAudioSystem>(this)]()
		{
			if (UAbyssAudioSystem* Self = WeakThis.Get())
			{
				Self->SetSuspended(false);
			}
		});
		return;
	}
	SetSuspended(false);
}

void UAbyssAudioSystem::SetSuspended(bool bSuspend)
{
	if (bSuspended == bSuspend)
	{
		return;
	}
	bSuspended = bSuspend;
	if (bSuspend)
	{
		StopAllSfx();
	}
	TArray<UAudioComponent*, TInlineAllocator<8>> Persistent;
	for (const TObjectPtr<UAudioComponent>& Comp : MusicComponents)
	{
		Persistent.Add(Comp.Get());
	}
	Persistent.Add(StingerComponent.Get());
	Persistent.Add(WhisperComponent.Get());
	Persistent.Add(AmbienceComponent.Get());
	Persistent.Add(EncounterStingerComponent.Get());
	for (const TObjectPtr<UAudioComponent>& Comp : AreaLoopComponents)
	{
		Persistent.Add(Comp.Get());
	}
	for (const TObjectPtr<UAudioComponent>& Comp : EmitterComponents)
	{
		Persistent.Add(Comp.Get());
	}
	for (UAudioComponent* Comp : Persistent)
	{
		if (IsValid(Comp))
		{
			Comp->SetPaused(bSuspend);
		}
	}
	UE_LOG(LogAbyss, Log, TEXT("Audio %s"), bSuspend ? TEXT("paused (app in background)") : TEXT("resumed"));
}

void UAbyssAudioSystem::RefreshVolumes(bool bForce)
{
	const UAbyssGameInstance* GI = GetAbyssGameInstance();
	if (GI == nullptr)
	{
		return;
	}
	const FAbyssUserSettings& Settings = GI->GetUserSettings();
	const float NewMaster = FMath::Clamp(Settings.MasterVolume, 0.f, 1.f);
	const float NewMusic = FMath::Clamp(Settings.MusicVolume, 0.f, 1.f);
	const float NewSfx = FMath::Clamp(Settings.SfxVolume, 0.f, 1.f);
	if (!bForce && NewMaster == MasterVolume && NewMusic == MusicVolume && NewSfx == SfxVolume)
	{
		return;
	}
	MasterVolume = NewMaster;
	MusicVolume = NewMusic;
	SfxVolume = NewSfx;
	// Music voices follow in TickMusic; the persistent beds update here.
	AbyssAudioSystemPrivate::SetVolumeIfChanged(StingerComponent, StingerGain * MasterVolume * MusicVolume);
	AbyssAudioSystemPrivate::SetVolumeIfChanged(WhisperComponent, WhisperGain * MasterVolume * MusicVolume);
	AbyssAudioSystemPrivate::SetVolumeIfChanged(EncounterStingerComponent, EncounterStingerGain * MasterVolume *
		MusicVolume);
	AbyssAudioSystemPrivate::SetVolumeIfChanged(AmbienceComponent, AmbienceGain());
}

float UAbyssAudioSystem::MusicBusGain() const
{
	return MasterVolume * MusicVolume * AbyssAudioSystemPrivate::DbToLinear(DuckCurrentDb);
}

float UAbyssAudioSystem::SfxBusGain() const
{
	return MasterVolume * SfxVolume;
}

float UAbyssAudioSystem::AmbienceGain() const
{
	// AmbienceBedScale: a region bed that replaces the bed (U7, the Breach Rim) crossfades with it.
	return AmbienceAssetGain * Manifest.Ambience.Gain * MasterVolume * SfxVolume * AmbienceBedScale *
		AbyssAudioSystemPrivate::DbToLinear(AmbienceDuckDb);
}

// =====================================================================================================================
// Music (EvMusic)
// =====================================================================================================================

void UAbyssAudioSystem::HandleMusic(const abyss::EvMusic& Event)
{
	EnsureReady();
	EndJukebox();
	const FName CoreKey = AbyssAudioSystemPrivate::ToName(Event.trackKey);
	FName TrackKey = CoreKey;
	// Presentation override per zone (manifest zoneOverrides): the zone is the snapshot's, which during dispatch is
	// already the zone the command was issued for.
	if (!TrackKey.IsNone() && Manifest.ZoneOverrides.Num() > 0)
	{
		const UAbyssGameInstance* GI = GetAbyssGameInstance();
		if (const abyss::Snapshot* Snap = GI ? GI->GetSnapshot() : nullptr)
		{
			TrackKey = Manifest.ApplyZoneOverride(AbyssAudioSystemPrivate::ToName(Snap->zone.mapId), TrackKey);
		}
	}
	if (!Remake.IsValid())
	{
		PlayMusic(TrackKey, static_cast<float>(Event.fadeOutSec), static_cast<float>(Event.fadeInSec), Event.restart,
			Event.loop);
		return;
	}
	// U7: the request joins the arbitration with the remake's layers (story hold, siege, regions); the decision is
	// applied in this frame's HandleFrame (the router's OnFrame follows the event dispatch in the same rendered frame).
	AbyssMusicRules::CoreRequest Request;
	Request.track = AbyssAudioSystemPrivate::ToStd(TrackKey);
	Request.kind = AbyssMusicRules::Classify(Event.trackKey, Remake->BossScores);
	Request.fadeOutSec = Event.fadeOutSec;
	Request.fadeInSec = Event.fadeInSec;
	Request.loop = Event.loop;
	Request.restart = Event.restart;
	Request.overridden = TrackKey != CoreKey;
	Remake->Arbiter.SetCore(Request);
}

FName UAbyssAudioSystem::SoundingTrack() const
{
	if (CurrentTrack.IsNone() || FindActiveMusicVoice() != INDEX_NONE)
	{
		return CurrentTrack;
	}
	// A one-shot that ended (nothing sounds) is not "the same track"; a track PlayMusic could not voice counts as asked
	// (no new request every frame for a sound that cannot load, or under -nosound where no component can be made; a
	// missing component is retried once a second).
	return UnvoicedTrack == CurrentTrack && NowSec < UnvoicedRetrySec ? CurrentTrack : FName(NAME_None);
}

int32 UAbyssAudioSystem::FindFreeMusicSlot() const
{
	for (int32 I = 0; I < MusicVoices.Num(); ++I)
	{
		if (!MusicVoices[I].bActive && !MusicVoices[I].bFadingOut)
		{
			return I;
		}
	}
	return INDEX_NONE;
}

int32 UAbyssAudioSystem::FindActiveMusicVoice() const
{
	for (int32 I = 0; I < MusicVoices.Num(); ++I)
	{
		if (MusicVoices[I].bActive)
		{
			return I;
		}
	}
	return INDEX_NONE;
}

double UAbyssAudioSystem::ResolveTrackDuration(const FAbyssMusicTrackDef& Track, USoundBase* Sound) const
{
	if (Track.LengthSec > 0.f)
	{
		return Track.LengthSec;
	}
	if (const FAbyssAudioAssetDef* AssetDef = Manifest.FindAsset(Track.Asset); AssetDef != nullptr && AssetDef->LengthSec > 0.f)
	{
		return AssetDef->LengthSec;
	}
	// No manifest (e.g. a build that did not stage it): the wave's own length. USoundBase::Duration holds the real
	// length; GetDuration() would report a looping wave as "indefinitely looping" (10000 s).
	if (Sound != nullptr && Sound->Duration > 0.f)
	{
		return Sound->Duration;
	}
	return 0.0;
}

UAudioComponent* UAbyssAudioSystem::EnsureMusicComponent(int32 Slot, USoundBase* Sound)
{
	UWorld* AudioWorld = GetAudioWorld();
	if (AudioWorld == nullptr || !MusicComponents.IsValidIndex(Slot))
	{
		return nullptr;
	}
	TObjectPtr<UAudioComponent>& Comp = MusicComponents[Slot];
	if (!IsValid(Comp) || Comp->GetWorld() != AudioWorld)
	{
		// 2D, UI sound (keeps playing while the game is paused: menus, cinematics), persistent, never auto-destroyed.
		Comp = UGameplayStatics::CreateSound2D(AudioWorld, Sound, 1.f, 1.f, 0.f, nullptr,
			/*bPersistAcrossLevelTransition*/ true, /*bAutoDestroy*/ false);
		if (Comp != nullptr)
		{
			Comp->bIsUISound = true;
			Comp->bAllowSpatialization = false;
		}
		return Comp;
	}
	if (Comp->IsPlaying())
	{
		Comp->Stop();
	}
	Comp->SetSound(Sound);
	return Comp;
}

void UAbyssAudioSystem::PlayMusic(FName TrackKey, float FadeOutSec, float FadeInSec, bool bRestart, bool bLoopRequested)
{
	FadeOutSec = FMath::Max(0.f, FadeOutSec);
	FadeInSec = FMath::Max(0.f, FadeInSec);

	// The same track already sounding and no restart asked: nothing to do (setState no-op).
	for (int32 I = 0; I < MusicVoices.Num(); ++I)
	{
		const FMusicVoice& Voice = MusicVoices[I];
		if (Voice.bActive && Voice.TrackKey == TrackKey && !TrackKey.IsNone() && !bRestart &&
			AbyssAudioSystemPrivate::IsSounding(MusicComponents[I]))
		{
			return;
		}
	}

	// 1) A voice still fading out from an earlier change stops now (never orphaned, FIX Q4).
	for (int32 I = 0; I < MusicVoices.Num(); ++I)
	{
		FMusicVoice& Voice = MusicVoices[I];
		if (Voice.bFadingOut)
		{
			ResumePositions.Add(Voice.TrackKey, Voice.PositionSec);
			if (IsValid(MusicComponents[I]))
			{
				MusicComponents[I]->Stop();
			}
			Voice = FMusicVoice();
		}
	}
	// 2) The current track fades out (its position is remembered for A2 resumes).
	for (int32 I = 0; I < MusicVoices.Num(); ++I)
	{
		FMusicVoice& Voice = MusicVoices[I];
		if (!Voice.bActive)
		{
			continue;
		}
		ResumePositions.Add(Voice.TrackKey, Voice.PositionSec);
		UAudioComponent* Comp = MusicComponents[I];
		// A paused (soundtrack) voice would never advance its fade: it stops at once.
		if (AbyssAudioSystemPrivate::IsSounding(Comp) && FadeOutSec > 0.f && !Voice.bPaused)
		{
			Comp->FadeOut(FadeOutSec, 0.f, EAudioFaderCurve::Linear);
			Voice.bActive = false;
			Voice.bFadingOut = true;
			Voice.FadeOutRemainingSec = FadeOutSec;
		}
		else
		{
			if (IsValid(Comp))
			{
				Comp->Stop();
			}
			Voice = FMusicVoice();
		}
	}

	CurrentTrack = TrackKey;
	UnvoicedTrack = NAME_None;
	if (TrackKey.IsNone())
	{
		return;  // silence (no theme for the zone)
	}
	const FAbyssMusicTrackDef Track = Manifest.ResolveTrack(TrackKey);
	USoundBase* Sound = GetSound(Track.Asset, AbyssAudioSystemPrivate::MusicFolder);
	if (Sound == nullptr)
	{
		UnvoicedTrack = TrackKey;   // asked for good (SoundingTrack): the sound will not load on a retry either
		UnvoicedRetrySec = TNumericLimits<double>::Max();
		return;
	}
	const int32 Slot = FindFreeMusicSlot();
	UAudioComponent* Comp = Slot != INDEX_NONE ? EnsureMusicComponent(Slot, Sound) : nullptr;
	if (Comp == nullptr)
	{
		UnvoicedTrack = TrackKey;   // -nosound / no audio world: asked, retried once a second (SoundingTrack)
		UnvoicedRetrySec = NowSec + 1.0;
		return;
	}

	const FAbyssAudioAssetDef* AssetDef = Manifest.FindAsset(Track.Asset);
	const double Duration = ResolveTrackDuration(Track, Sound);
	const bool bWaveLoops = Sound->IsLooping();
	if (bLoopRequested != bWaveLoops)
	{
		UE_LOG(LogAbyss, Verbose, TEXT("Audio: track %s asked loop=%d, the asset loops=%d"), *TrackKey.ToString(),
			bLoopRequested ? 1 : 0, bWaveLoops ? 1 : 0);
	}
	double StartSec = 0.0;
	if (!bRestart)
	{
		if (const double* Saved = ResumePositions.Find(TrackKey))
		{
			StartSec = Duration > 0.0 ? FMath::Fmod(*Saved, Duration) : 0.0;
			if (!bLoopRequested && Duration > 0.0 && StartSec > Duration - 0.5)
			{
				StartSec = 0.0;
			}
		}
	}

	const float Gain = Track.Gain * (AssetDef ? AssetDef->Gain : 1.f);
	Comp->SetVolumeMultiplier(Gain * MusicBusGain());
	if (FadeInSec > 0.f)
	{
		Comp->FadeIn(FadeInSec, 1.f, static_cast<float>(StartSec), EAudioFaderCurve::Linear);
	}
	else
	{
		Comp->Play(static_cast<float>(StartSec));
	}
	if (bSuspended)
	{
		Comp->SetPaused(true);
	}

	FMusicVoice& Voice = MusicVoices[Slot];
	Voice = FMusicVoice();
	Voice.TrackKey = TrackKey;
	Voice.Gain = Gain;
	Voice.PositionSec = StartSec;
	Voice.DurationSec = Duration;
	Voice.bLoop = bLoopRequested;
	Voice.bActive = true;
	UE_LOG(LogAbyss, Verbose, TEXT("Audio: music %s (%s at %.2f s, fade out %.2f / in %.2f)"), *TrackKey.ToString(),
		bRestart ? TEXT("restart") : TEXT("resume"), StartSec, FadeOutSec, FadeInSec);
}

void UAbyssAudioSystem::StopAllMusic()
{
	for (int32 I = 0; I < MusicVoices.Num(); ++I)
	{
		if (MusicComponents.IsValidIndex(I) && IsValid(MusicComponents[I]))
		{
			MusicComponents[I]->Stop();
		}
		MusicVoices[I] = FMusicVoice();
	}
	CurrentTrack = NAME_None;
	UnvoicedTrack = NAME_None;
}

void UAbyssAudioSystem::TickMusic(float RealDeltaSec)
{
	const float Bus = MusicBusGain();
	for (int32 I = 0; I < MusicVoices.Num(); ++I)
	{
		FMusicVoice& Voice = MusicVoices[I];
		if (!Voice.bActive && !Voice.bFadingOut)
		{
			continue;
		}
		UAudioComponent* Comp = MusicComponents.IsValidIndex(I) ? MusicComponents[I].Get() : nullptr;
		const bool bHeld = bSuspended || Voice.bPaused;
		if (!bHeld)
		{
			Voice.PositionSec += RealDeltaSec;
			if (Voice.bLoop && Voice.DurationSec > 0.0)
			{
				Voice.PositionSec = FMath::Fmod(Voice.PositionSec, Voice.DurationSec);
			}
		}
		if (Voice.bActive)
		{
			ResumePositions.Add(Voice.TrackKey, Voice.PositionSec);
		}
		if (Voice.bFadingOut)
		{
			Voice.FadeOutRemainingSec -= bHeld ? 0.0 : RealDeltaSec;
			if (Voice.FadeOutRemainingSec <= 0.0 || !AbyssAudioSystemPrivate::IsSounding(Comp))
			{
				if (IsValid(Comp))
				{
					Comp->Stop();
				}
				Voice = FMusicVoice();
				continue;
			}
		}
		else if (!bHeld && !AbyssAudioSystemPrivate::IsSounding(Comp))
		{
			// A one-shot (victory) reached its end; the director sends the next track after its hold.
			Voice = FMusicVoice();
			continue;
		}
		else if (!bHeld && !Voice.bLoop && !IsJukeboxActive() && Voice.DurationSec > 0.0 &&
			Voice.PositionSec >= Voice.DurationSec)
		{
			// One-shot request on a looping asset: end it at its length.
			if (IsValid(Comp))
			{
				Comp->FadeOut(0.25f, 0.f, EAudioFaderCurve::Linear);
			}
			Voice.bActive = false;
			Voice.bFadingOut = true;
			Voice.FadeOutRemainingSec = 0.25;
		}
		AbyssAudioSystemPrivate::SetVolumeIfChanged(Comp, Voice.Gain * Bus);
	}
}

// =====================================================================================================================
// SFX (EvSfx)
// =====================================================================================================================

void UAbyssAudioSystem::HandleSfx(const abyss::EvSfx& Event)
{
	EnsureReady();
	if (!bReady || bSuspended)
	{
		return;
	}
	const FName CueId = AbyssAudioSystemPrivate::ToName(abyss::EnumName(Event.cue));
	// U7: the source monster's voice (family, named cues, encounter stingers) on the three voiced beats, resolved once
	// per def (VoiceOfDef): two map lookups per event, no table scan or string conversion.
	const bool bVoicedBeat = Event.cue == abyss::SfxId::MonsterAggro || Event.cue == abyss::SfxId::MonsterHurt ||
		Event.cue == abyss::SfxId::MonsterDeath;
	const FName DefName = bVoicedBeat ? DefNameOfEntity(Event.source) : FName(NAME_None);
	const FDefVoice& Voice = VoiceOfDef(DefName);
	if (Voice.bHasEncounter)
	{
		if (Event.cue == abyss::SfxId::MonsterAggro && !Voice.bMiniBoss)
		{
			// A hunt's fight starts with its first aggro (a mini-boss's when its pre-fight lines are dismissed).
			PlayEncounterStinger(Event.source, Voice.DefId, /*bSlain*/ false);
		}
		else if (Event.cue == abyss::SfxId::MonsterDeath)
		{
			PlayEncounterStinger(Event.source, Voice.DefId, /*bSlain*/ true);
		}
	}
	const FAbyssAudioCueDef* Cue = Manifest.FindCue(CueId);
	if (Cue == nullptr)
	{
		return;
	}
	const bool bFamilyCue = Cue->Families.Num() > 0 || Cue->VocalLayer.Num() > 0;
	const bool bSilent = bFamilyCue && Voice.bSilent;
	const FName Family = bFamilyCue ? Voice.Family : FName(NAME_None);
	const bool bSpatial = Event.spatial && !Cue->bUiBus;
	FVector2D Tile(Event.pos.x, Event.pos.y);
	if (bSpatial && Event.source != abyss::kNoEntity && Event.pos.x == 0.0 && Event.pos.y == 0.0)
	{
		// A spatial cue without a position: where its source stands now.
		if (const UAbyssSimDriver* Driver = UAbyssSimDriver::Get(GetAudioWorld()))
		{
			abyss::Vec2 Prev, Cur;
			if (Driver->GetSnapshotIndex().FindEntityPosition(Event.source, Prev, Cur))
			{
				Tile = FVector2D(Cur.x, Cur.y);
			}
		}
	}
	// U7: a named cue replaces the family's aggro / hurt (Valdras, the Corpseherd); a silent monster has no family cue.
	if (Event.cue == abyss::SfxId::MonsterAggro || Event.cue == abyss::SfxId::MonsterHurt)
	{
		const FName Named = Event.cue == abyss::SfxId::MonsterAggro ? Voice.AggroCue : Voice.HurtCue;
		if (const FAbyssAudioCueDef* NamedCue = Named.IsNone() ? nullptr : Manifest.FindCue(Named))
		{
			PlayCue(*NamedCue, NAME_None, bSpatial, Tile);
			return;
		}
	}
	if (bSilent && Cue->Families.Num() > 0)
	{
		return;
	}
	PlayCue(*Cue, Family, bSpatial, Tile);

	// A7: the family's death vocal on top of the web's monster_death groan (U7: or the monster's named death cue).
	if (Event.cue == abyss::SfxId::MonsterDeath && !bSilent && !Voice.DeathVocalCue.IsNone())
	{
		if (const FAbyssAudioCueDef* NamedCue = Manifest.FindCue(Voice.DeathVocalCue))
		{
			PlayCue(*NamedCue, NAME_None, bSpatial, Tile);
			return;
		}
	}
	if (!Family.IsNone() && !bSilent)
	{
		if (const TArray<FName>* Layer = Cue->VocalLayer.Find(Family); Layer != nullptr && Layer->Num() > 0)
		{
			FAbyssAudioCueDef Vocal;
			Vocal.Id = FName(*(CueId.ToString() + TEXT("_vocal")));
			Vocal.Assets = *Layer;
			Vocal.Variants = Layer->Num();
			Vocal.ConcurrencyMax = Cue->ConcurrencyMax;
			Vocal.Gain = Cue->Gain;
			PlayCue(Vocal, NAME_None, bSpatial, Tile);
		}
	}
}

void UAbyssAudioSystem::PlayUiCue(abyss::SfxId Cue)
{
	EnsureReady();
	if (!bReady || bSuspended)
	{
		return;
	}
	if (const FAbyssAudioCueDef* Def = Manifest.FindCue(AbyssAudioSystemPrivate::ToName(abyss::EnumName(Cue))))
	{
		PlayCue(*Def, NAME_None, /*bSpatial*/ false, HeroTile);
	}
}

void UAbyssAudioSystem::PreviewSfxVolume()
{
	if (NowSec - LastPreviewSec < AbyssAudioSystemPrivate::PreviewMinIntervalSec)
	{
		return;
	}
	LastPreviewSec = NowSec;
	RefreshVolumes(/*bForce*/ true);
	PlayUiCue(abyss::SfxId::Click);
}

UAudioComponent* UAbyssAudioSystem::PlayCue(const FAbyssAudioCueDef& Cue, FName Family, bool bSpatial,
	const FVector2D& Tile)
{
	FCueState& State = CueStates.FindOrAdd(Cue.Id);
	State.Voices.RemoveAll([](const TWeakObjectPtr<UAudioComponent>& Voice)
	{
		return !AbyssAudioSystemPrivate::IsSounding(Voice.Get());
	});
	// audio.md 9.6 concurrency: minimum retrigger interval, then stop the oldest voice at the cap.
	if (Cue.MinRetriggerSec > 0.f && NowSec - State.LastStartSec < Cue.MinRetriggerSec)
	{
		return nullptr;
	}
	while (State.Voices.Num() >= FMath::Max(1, Cue.ConcurrencyMax))
	{
		if (UAudioComponent* Oldest = State.Voices[0].Get())
		{
			Oldest->Stop();
		}
		State.Voices.RemoveAt(0);
	}

	// Variants of the source's family (A7): `Variants` consecutive assets per family; unknown families use the first.
	TArray<FName> Candidates;
	if (Cue.Families.Num() > 0)
	{
		const int32 FamilyIndex = FMath::Max(0, Cue.Families.IndexOfByKey(Family));
		const int32 Per = FMath::Max(1, Cue.Variants);
		for (int32 I = FamilyIndex * Per; I < (FamilyIndex + 1) * Per && I < Cue.Assets.Num(); ++I)
		{
			Candidates.Add(Cue.Assets[I]);
		}
	}
	if (Candidates.Num() == 0)
	{
		Candidates = Cue.Assets;
	}
	const FName Asset = PickVariant(Candidates, State, Family);
	USoundBase* Sound = GetSound(Asset, AbyssAudioSystemPrivate::SfxFolder);
	if (Sound == nullptr)
	{
		return nullptr;
	}
	const FAbyssAudioAssetDef* AssetDef = Manifest.FindAsset(Asset);
	const float Volume = Cue.Gain * (AssetDef ? AssetDef->Gain : 1.f) * SfxBusGain();
	UAudioComponent* Comp = PlayOneShot(Sound, Volume, bSpatial, Tile);
	if (Comp != nullptr)
	{
		State.Voices.Add(Comp);
		State.LastStartSec = NowSec;
	}
	return Comp;
}

FName UAbyssAudioSystem::PickVariant(TArray<FName> const& Candidates, FCueState& State, FName Family)
{
	if (Candidates.Num() == 0)
	{
		return NAME_None;
	}
	if (Candidates.Num() == 1)
	{
		return Candidates[0];
	}
	// audio.md 9.5: random variant, never the same one twice in a row.
	const int32* Last = State.LastVariant.Find(Family);
	int32 Index = Random.RandRange(0, Candidates.Num() - 1);
	if (Last != nullptr && Index == *Last)
	{
		Index = (Index + 1 + Random.RandRange(0, Candidates.Num() - 2)) % Candidates.Num();
	}
	State.LastVariant.Add(Family, Index);
	return Candidates[Index];
}

UAudioComponent* UAbyssAudioSystem::PlayOneShot(USoundBase* Sound, float Volume, bool bSpatial, const FVector2D& Tile)
{
	UWorld* AudioWorld = GetAudioWorld();
	if (AudioWorld == nullptr || Sound == nullptr)
	{
		return nullptr;
	}
	FVector Location;
	if (bSpatial && ComputeVirtualEmitter(Tile, Location))
	{
		return UGameplayStatics::SpawnSoundAtLocation(AudioWorld, Sound, Location, FRotator::ZeroRotator, Volume, 1.f,
			0.f, GetSpatialAttenuation(), nullptr, /*bAutoDestroy*/ true);
	}
	return UGameplayStatics::SpawnSound2D(AudioWorld, Sound, Volume, 1.f, 0.f, nullptr,
		/*bPersistAcrossLevelTransition*/ false, /*bAutoDestroy*/ true);
}

APlayerController* UAbyssAudioSystem::GetListenerController() const
{
	UWorld* AudioWorld = GetAudioWorld();
	return AudioWorld ? AudioWorld->GetFirstPlayerController() : nullptr;
}

bool UAbyssAudioSystem::ComputeVirtualEmitter(const FVector2D& Tile, FVector& OutLocation) const
{
	// A4 mild panning, listener on the hero: the source's lateral offset from the hero across the screen (the listener's
	// right axis flattened onto the ground), normalised by the play-area half width and scaled by the spread, becomes
	// the azimuth of an emitter VirtualDistanceCm from the listener in its own front / right plane: elevation 0, no
	// distance loss (the attenuation only spatialises). The listener is the player controller's (the camera unless an
	// override is set), so the panning follows whatever the engine uses.
	const APlayerController* PC = GetListenerController();
	if (PC == nullptr)
	{
		return false;
	}
	FVector ListenerLocation, Front, Right;
	PC->GetAudioListenerPosition(ListenerLocation, Front, Right);
	FVector RightFlat(Right.X, Right.Y, 0.0);
	if (!RightFlat.Normalize() || !Front.Normalize() || !Right.Normalize())
	{
		return false;
	}
	const FVector Source = AbyssUnits::TileToWorld(Tile);
	const FVector Hero = AbyssUnits::TileToWorld(HeroTile);
	const double Lateral = FVector::DotProduct(Source - Hero, RightFlat);
	const double Pan = FMath::Clamp(Lateral / Manifest.Spatial.HalfWidthCm, -1.0, 1.0) * Manifest.Spatial.Spread;
	const double Azimuth = Pan * UE_HALF_PI;
	OutLocation = ListenerLocation + (Front * FMath::Cos(Azimuth) + Right * FMath::Sin(Azimuth)) *
		Manifest.Spatial.VirtualDistanceCm;
	return true;
}

FName UAbyssAudioSystem::DefNameOfEntity(abyss::EntityId Id) const
{
	if (Id == abyss::kNoEntity || Id == abyss::kHeroEntityId)
	{
		return NAME_None;
	}
	if (const FName* Found = EntityDefs.Find(Id))
	{
		return *Found;
	}
	const UAbyssSimDriver* Driver = UAbyssSimDriver::Get(GetAudioWorld());
	if (const abyss::MonsterView* Monster = Driver ? Driver->GetSnapshotIndex().FindMonster(Id) : nullptr)
	{
		return AbyssAudioSystemPrivate::ToName(Monster->defId);   // no spawn record: rare
	}
	return NAME_None;
}

FVector2D UAbyssAudioSystem::EntityTile(abyss::EntityId Id) const
{
	if (const UAbyssSimDriver* Driver = UAbyssSimDriver::Get(GetAudioWorld()))
	{
		abyss::Vec2 Prev, Cur;
		if (Driver->GetSnapshotIndex().FindEntityPosition(Id, Prev, Cur))
		{
			return FVector2D(Cur.x, Cur.y);
		}
	}
	return HeroTile;
}

const UAbyssAudioSystem::FDefVoice& UAbyssAudioSystem::VoiceOfDef(FName DefName)
{
	static const FDefVoice NoVoice;
	if (DefName.IsNone())
	{
		return NoVoice;
	}
	if (const FDefVoice* Found = DefVoices.Find(DefName))
	{
		return *Found;
	}
	FDefVoice Out;
	Out.DefId = AbyssAudioSystemPrivate::ToStd(DefName);
	const UAbyssGameInstance* GI = GetAbyssGameInstance();
	const abyss::DataStore* Data = GI ? GI->GetData() : nullptr;
	// The def's rig (today's rule); a def the data does not list (a hunt leader's hunt id) has none, as before.
	const abyss::MonsterDef* Def = Data ? Data->FindMonster(Out.DefId) : nullptr;
	const std::string_view AnimCategory = Def ? abyss::EnumName(Def->animCategory) : std::string_view();
	// U7: voiceFamilies.monsters, then the design family (withheld from the W1 export: none yet), then the rig.
	const AbyssVoiceRules::DefVoice Voice = AbyssVoiceRules::ResolveDefVoice(AbyssAudioSystemPrivate::VoicesOf(Remake),
		Out.DefId, std::string_view(), AnimCategory);
	Out.Family = AbyssAudioSystemPrivate::ToName(Voice.voice.family);   // "" -> None
	Out.bSilent = Voice.voice.IsSilent();
	Out.bMiniBoss = Def != nullptr && Def->isMiniBoss;
	Out.bHasAnimCue = Voice.hasAnimCue;
	Out.AggroCue = AbyssAudioSystemPrivate::ToName(Voice.aggroCue);
	Out.HurtCue = AbyssAudioSystemPrivate::ToName(Voice.hurtCue);
	Out.DeathVocalCue = AbyssAudioSystemPrivate::ToName(Voice.deathVocalCue);
	if (Remake.IsValid())
	{
		using AbyssMusicRules::EncounterBeat;
		const AbyssMusicRules::DirectorTables& Director = Remake->Director;
		Out.bHasEncounter = AbyssMusicRules::EncounterStinger(Director, Out.DefId, EncounterBeat::Engaged) != nullptr ||
			AbyssMusicRules::EncounterStinger(Director, Out.DefId, EncounterBeat::Slain) != nullptr;
	}
	return DefVoices.Add(DefName, MoveTemp(Out));
}

void UAbyssAudioSystem::HandleEntitySpawned(const abyss::EvEntitySpawned& Event)
{
	if (Event.kind != abyss::EntityKind::Monster)
	{
		return;
	}
	const FName DefName = AbyssAudioSystemPrivate::ToName(Event.defId);
	EntityDefs.Add(Event.id, DefName);
	if (bReady)
	{
		VoiceOfDef(DefName);   // resolved at spawn, not on the first hit
	}
}

void UAbyssAudioSystem::StopAllSfx()
{
	for (TPair<FName, FCueState>& Pair : CueStates)
	{
		for (const TWeakObjectPtr<UAudioComponent>& Voice : Pair.Value.Voices)
		{
			if (UAudioComponent* Comp = Voice.Get())
			{
				Comp->Stop();
			}
		}
		Pair.Value.Voices.Reset();
	}
}

void UAbyssAudioSystem::DebugPlayMusic(FName TrackKey, bool bRestart)
{
	EnsureReady();
	if (bInSession && Remake.IsValid())
	{
		// As an EvMusic would: the remake's layers (regions, siege, story hold) still take precedence (U7).
		AbyssMusicRules::CoreRequest Request;
		Request.track = AbyssAudioSystemPrivate::ToStd(TrackKey);
		Request.kind = AbyssMusicRules::Classify(Request.track, Remake->BossScores);
		Request.fadeOutSec = 1.5;
		Request.fadeInSec = 1.0;
		Request.restart = bRestart;
		Remake->Arbiter.SetCore(Request);
		return;
	}
	PlayMusic(TrackKey, 1.5f, 1.f, bRestart, /*bLoopRequested*/ true);
}

void UAbyssAudioSystem::DebugPlayCue(FName CueId, bool bSpatial)
{
	EnsureReady();
	if (const FAbyssAudioCueDef* Cue = Manifest.FindCue(CueId))
	{
		const FName Family = Cue->Families.Num() > 0 ? Cue->Families[0] : FName(NAME_None);
		PlayCue(*Cue, Family, bSpatial, HeroTile + FVector2D(4.0, 0.0));
	}
	else
	{
		UE_LOG(LogAbyss, Warning, TEXT("abyss.Audio.Cue: unknown cue %s"), *CueId.ToString());
	}
}

// =====================================================================================================================
// Zone ambience (A7)
// =====================================================================================================================

void UAbyssAudioSystem::HandleZone(const abyss::EvZone& Event)
{
	EnsureReady();
	switch (Event.phase)
	{
	case abyss::EvZone::Phase::TransitionBegan:
		StopAmbience(Manifest.Ambience.FadeOutSec);
		StopRemakeAmbience(Manifest.Ambience.FadeOutSec);
		if (Remake.IsValid())
		{
			Remake->bZoneTransition = true;
		}
		break;
	case abyss::EvZone::Phase::Exited:
		StopAmbience(Manifest.Ambience.FadeOutSec);
		StopRemakeAmbience(Manifest.Ambience.FadeOutSec);
		EntityDefs.Reset();  // the next zone's spawns follow
		if (Remake.IsValid())
		{
			// The set-piece layer hands the next zone's emitter spots over when it builds that zone (after this).
			Remake->SetPieceSpots.Reset();
			Remake->EngagedEntities.Reset();
		}
		break;
	case abyss::EvZone::Phase::Entered:
	{
		FName Asset;
		const UAbyssGameInstance* GI = GetAbyssGameInstance();
		if (const FName* ByZone = Manifest.Ambience.ByZone.Find(AbyssAudioSystemPrivate::ToName(Event.mapId)))
		{
			Asset = *ByZone;
		}
		else if (const abyss::Snapshot* Snap = GI ? GI->GetSnapshot() : nullptr)
		{
			// U7: the map's presentation theme (the dark remake's `march`) keys the bed, else its theme (as before).
			const abyss::DataStore* Data = GI->GetData();
			const std::string_view Theme = AbyssAmbienceRules::BedThemeName(Data ? Data->FindMap(Event.mapId) : nullptr,
				abyss::EnumName(Snap->zone.theme));
			if (const FName* ByTheme = Manifest.Ambience.ByTheme.Find(AbyssAudioSystemPrivate::ToName(Theme)))
			{
				Asset = *ByTheme;
			}
		}
		if (Remake.IsValid())
		{
			// Hysteresis, trigger timers and the story hold start over in the new zone; the emitters are placed anew.
			Remake->ZoneId = Event.mapId;
			Remake->MusicRegions.Reset();
			Remake->Region = -1;
			Remake->Siege = AbyssMusicRules::QuestTrackPick{};
			Remake->Arbiter.OnZoneChanged();
			Remake->AmbienceRegions.Reset();
			Remake->AmbienceLayers.Reset();
			Remake->Scheduler.OnZoneChanged();
			Remake->DecorFocus.clear();
			Remake->bEmittersDirty = true;
			Remake->bZoneTransition = false;
		}
		if (Asset.IsNone())
		{
			StopAmbience(Manifest.Ambience.FadeOutSec);
		}
		else
		{
			StartAmbience(Asset);
		}
		break;
	}
	default:
		break;
	}
}

void UAbyssAudioSystem::StartAmbience(FName Asset)
{
	if (Asset == CurrentAmbience && AbyssAudioSystemPrivate::IsSounding(AmbienceComponent))
	{
		return;
	}
	USoundBase* Sound = GetSound(Asset, AbyssAudioSystemPrivate::AmbienceFolder);
	UWorld* AudioWorld = GetAudioWorld();
	if (Sound == nullptr || AudioWorld == nullptr)
	{
		return;
	}
	if (!IsValid(AmbienceComponent) || AmbienceComponent->GetWorld() != AudioWorld)
	{
		AmbienceComponent = UGameplayStatics::CreateSound2D(AudioWorld, Sound, 1.f, 1.f, 0.f, nullptr, true, false);
		if (AmbienceComponent == nullptr)
		{
			return;
		}
		AmbienceComponent->bIsUISound = true;
		AmbienceComponent->bAllowSpatialization = false;
	}
	else
	{
		AmbienceComponent->Stop();
		AmbienceComponent->SetSound(Sound);
	}
	const FAbyssAudioAssetDef* Def = Manifest.FindAsset(Asset);
	AmbienceAssetGain = Def ? Def->Gain : 1.f;
	CurrentAmbience = Asset;
	AmbienceComponent->SetVolumeMultiplier(AmbienceGain());
	// A random start point: zone re-entries do not always open on the same gust / bird.
	const float Length = Def && Def->LengthSec > 0.f ? Def->LengthSec : 0.f;
	AmbienceComponent->FadeIn(Manifest.Ambience.FadeInSec, 1.f, Length > 1.f ? Random.FRandRange(0.f, Length - 1.f) : 0.f,
		EAudioFaderCurve::Linear);
	if (bSuspended)
	{
		AmbienceComponent->SetPaused(true);
	}
}

void UAbyssAudioSystem::StopAmbience(float FadeOutSec)
{
	if (AbyssAudioSystemPrivate::IsSounding(AmbienceComponent))
	{
		if (FadeOutSec > 0.f)
		{
			AmbienceComponent->FadeOut(FadeOutSec, 0.f, EAudioFaderCurve::Linear);
		}
		else
		{
			AmbienceComponent->Stop();
		}
	}
	CurrentAmbience = NAME_None;
}

// =====================================================================================================================
// Story stingers and duck (audio.md 10.2)
// =====================================================================================================================

void UAbyssAudioSystem::HandleStoryBeat(const abyss::EvStoryBeat& Event)
{
	EnsureReady();
	if (Event.phase == abyss::EvStoryBeat::Phase::Ended)
	{
		EndStoryAudio();
		if (Remake.IsValid())
		{
			// U7: the story hold's `then` timer starts, the beat's bursts stop, the beat-end bells ring.
			Remake->Arbiter.OnStoryEnded(Remake->Director, Event.beatId, NowSec);
			Remake->Scheduler.StopBound(Event.beatId);
			if (Remake->PlayingBeat == Event.beatId)
			{
				Remake->PlayingBeat.clear();
			}
			StartTriggerRows(AbyssCueTriggerRules::MatchStoryEnd(Remake->Triggers, Event.beatId), std::string_view());
		}
		return;
	}
	if (Remake.IsValid())
	{
		Remake->PlayingBeat = Event.beatId;
		Remake->PlayingKind = Event.kind;
	}
	bBeatActive = true;
	CurrentBeatKind = Event.kind;
	switch (Event.kind)
	{
	case abyss::StoryBeatKind::Cutscene:
	case abyss::StoryBeatKind::BossIntro:
	{
		// Letterbox bars slide in (every cutscene): a short one-shot on the music volume, no duck.
		if (USoundBase* Sound = GetSound(Manifest.Stingers.CutsceneIn, AbyssAudioSystemPrivate::StingerFolder))
		{
			const FAbyssAudioAssetDef* Def = Manifest.FindAsset(Manifest.Stingers.CutsceneIn);
			PlayOneShot(Sound, (Def ? Def->Gain : 1.f) * MasterVolume * MusicVolume, /*bSpatial*/ false, HeroTile);
		}
		break;
	}
	case abyss::StoryBeatKind::Chapter:
	{
		// beat id "chapter_<mapId>" -> the chapter card's mood -> its stinger (ch1: dawn); duck -8 dB.
		FName Asset = Manifest.Stingers.ChapterCardFallback;
		const UAbyssGameInstance* GI = GetAbyssGameInstance();
		const abyss::DataStore* Data = GI ? GI->GetData() : nullptr;
		constexpr std::string_view ChapterPrefix = "chapter_";
		const std::string_view BeatId = Event.beatId;
		if (Data != nullptr && BeatId.substr(0, ChapterPrefix.size()) == ChapterPrefix)
		{
			if (const abyss::ChapterCard* Card = Data->Story().ChapterFor(BeatId.substr(ChapterPrefix.size())))
			{
				if (const FName* ByMood = Manifest.Stingers.ChapterCardByMood.Find(
						AbyssAudioSystemPrivate::ToName(abyss::EnumName(Card->mood))))
				{
					Asset = *ByMood;
				}
			}
		}
		StartStinger(Asset, &Manifest.Stingers.ChapterCardDuck, 0.f);
		break;
	}
	case abyss::StoryBeatKind::Sequence:
	default:
		break;  // sequences change the music through the core (PlayTrack under the story lock)
	}
}

void UAbyssAudioSystem::HandleStoryStep(const abyss::EvStoryStep& Event)
{
	if (Remake.IsValid() && bReady)
	{
		// U7: a story hold (cs_rf_finale's silence) from its step; the bells / voices of this step or slide; a monster's
		// `say` lines in its named voice (story.say).
		Remake->Arbiter.OnStoryStep(Remake->Director, Event.beatId, Event.index, Event.isSlide);
		StartTriggerRows(AbyssCueTriggerRules::MatchStoryStep(Remake->Triggers, Event.beatId, Event.sequenceId,
			Event.index, Event.isSlide), Event.beatId);
		if (!Event.isSlide && Event.step.kind == abyss::StoryStepKind::Say &&
			Event.step.speaker.kind == abyss::StoryActorKind::Monster)
		{
			const std::string_view Named = AbyssVoiceRules::OverrideCue(Remake->Voices, Event.step.speaker.id,
				AbyssVoiceRules::KeyStorySay);
			if (!Named.empty())
			{
				// At the nearest living instance of the speaker (the boss facing the hero), else at the hero.
				FVector2D Tile = HeroTile;
				double BestSq = TNumericLimits<double>::Max();
				const UAbyssGameInstance* GI = GetAbyssGameInstance();
				if (const abyss::Snapshot* Snap = GI ? GI->GetSnapshot() : nullptr)
				{
					for (const abyss::MonsterView& Monster : Snap->monsters)
					{
						const double DistSq = FVector2D::DistSquared(FVector2D(Monster.pos.x, Monster.pos.y), HeroTile);
						if (Monster.alive && Monster.defId == Event.step.speaker.id && DistSq < BestSq)
						{
							BestSq = DistSq;
							Tile = FVector2D(Monster.pos.x, Monster.pos.y);
						}
					}
				}
				PlayNamedCue(AbyssAudioSystemPrivate::ToName(Named), Tile);
			}
		}
	}
	if (Event.isSlide)
	{
		return;
	}
	const abyss::StoryStepKind Kind = Event.step.kind;
	// A whisper loop lasts while whisper steps are shown.
	if (Kind == abyss::StoryStepKind::Whisper)
	{
		StartWhisper();
	}
	else if (bWhisperActive)
	{
		StopWhisper();
	}
	// The boss title duck lasts for the title step only; the stinger itself plays out (3.5 s, matching the card).
	if (bTitleStinger && Kind != abyss::StoryStepKind::Title)
	{
		bTitleStinger = false;
		ReleaseDuck(Manifest.Stingers.BossIntroDuck);
	}
	if (Kind == abyss::StoryStepKind::Title)
	{
		StartStinger(Manifest.Stingers.BossIntro, &Manifest.Stingers.BossIntroDuck, 0.f);
		bTitleStinger = true;
	}
}

void UAbyssAudioSystem::StartStinger(FName Asset, const FAbyssDuckDef* Duck, float FadeInSec)
{
	USoundBase* Sound = GetSound(Asset, AbyssAudioSystemPrivate::StingerFolder);
	UWorld* AudioWorld = GetAudioWorld();
	if (Sound == nullptr || AudioWorld == nullptr)
	{
		return;
	}
	if (!IsValid(StingerComponent) || StingerComponent->GetWorld() != AudioWorld)
	{
		StingerComponent = UGameplayStatics::CreateSound2D(AudioWorld, Sound, 1.f, 1.f, 0.f, nullptr, true, false);
		if (StingerComponent == nullptr)
		{
			return;
		}
		StingerComponent->bIsUISound = true;
		StingerComponent->bAllowSpatialization = false;
	}
	else
	{
		StingerComponent->Stop();
		StingerComponent->SetSound(Sound);
	}
	const FAbyssAudioAssetDef* Def = Manifest.FindAsset(Asset);
	StingerGain = Def ? Def->Gain : 1.f;
	StingerComponent->SetVolumeMultiplier(StingerGain * MasterVolume * MusicVolume);
	if (FadeInSec > 0.f)
	{
		StingerComponent->FadeIn(FadeInSec, 1.f, 0.f, EAudioFaderCurve::Linear);
	}
	else
	{
		StingerComponent->Play();
	}
	if (bSuspended)
	{
		StingerComponent->SetPaused(true);
	}
	if (Duck != nullptr)
	{
		StingerDuck = *Duck;
		bStingerDucking = true;
		SetDuck(*Duck);
	}
}

void UAbyssAudioSystem::StopStinger(float FadeOutSec)
{
	if (AbyssAudioSystemPrivate::IsSounding(StingerComponent))
	{
		if (FadeOutSec > 0.f)
		{
			StingerComponent->FadeOut(FadeOutSec, 0.f, EAudioFaderCurve::Linear);
		}
		else
		{
			StingerComponent->Stop();
		}
	}
	if (bStingerDucking)
	{
		bStingerDucking = false;
		ReleaseDuck(StingerDuck);
	}
	bTitleStinger = false;
}

void UAbyssAudioSystem::StartWhisper()
{
	if (bWhisperActive)
	{
		return;
	}
	USoundBase* Sound = GetSound(Manifest.Stingers.Whisper, AbyssAudioSystemPrivate::StingerFolder);
	UWorld* AudioWorld = GetAudioWorld();
	if (Sound == nullptr || AudioWorld == nullptr)
	{
		return;
	}
	if (!IsValid(WhisperComponent) || WhisperComponent->GetWorld() != AudioWorld)
	{
		WhisperComponent = UGameplayStatics::CreateSound2D(AudioWorld, Sound, 1.f, 1.f, 0.f, nullptr, true, false);
		if (WhisperComponent == nullptr)
		{
			return;
		}
		WhisperComponent->bIsUISound = true;
		WhisperComponent->bAllowSpatialization = false;
	}
	else
	{
		WhisperComponent->Stop();
		WhisperComponent->SetSound(Sound);
	}
	const FAbyssAudioAssetDef* Def = Manifest.FindAsset(Manifest.Stingers.Whisper);
	WhisperGain = Def ? Def->Gain : 1.f;
	WhisperComponent->SetVolumeMultiplier(WhisperGain * MasterVolume * MusicVolume);
	WhisperComponent->FadeIn(Manifest.Stingers.WhisperFadeSec, 1.f, 0.f, EAudioFaderCurve::Linear);
	if (bSuspended)
	{
		WhisperComponent->SetPaused(true);
	}
	bWhisperActive = true;
	SetDuck(Manifest.Stingers.WhisperDuck);
}

void UAbyssAudioSystem::StopWhisper()
{
	if (!bWhisperActive)
	{
		return;
	}
	bWhisperActive = false;
	if (AbyssAudioSystemPrivate::IsSounding(WhisperComponent))
	{
		WhisperComponent->FadeOut(Manifest.Stingers.WhisperFadeSec, 0.f, EAudioFaderCurve::Linear);
	}
	ReleaseDuck(Manifest.Stingers.WhisperDuck);
}

void UAbyssAudioSystem::SetDuck(const FAbyssDuckDef& Duck)
{
	// The deepest active duck wins; the transition uses the requesting duck's fade-in time.
	DuckTargetDb = FMath::Min(DuckTargetDb, Duck.Db);
	DuckInSec = Duck.InSec;
}

void UAbyssAudioSystem::ReleaseDuck(const FAbyssDuckDef& Duck)
{
	float Target = 0.f;
	if (bStingerDucking)
	{
		Target = FMath::Min(Target, StingerDuck.Db);
	}
	if (bWhisperActive)
	{
		Target = FMath::Min(Target, Manifest.Stingers.WhisperDuck.Db);
	}
	if (bEncounterDucking)
	{
		Target = FMath::Min(Target, EncounterDuck.Db);
	}
	DuckTargetDb = Target;
	DuckOutSec = Duck.OutSec;
}

void UAbyssAudioSystem::TickDuck(float RealDeltaSec)
{
	// A ducking stinger that ended by itself (natural end) releases its duck.
	if (bStingerDucking && !bTitleStinger && !AbyssAudioSystemPrivate::IsSounding(StingerComponent) && !bSuspended)
	{
		bStingerDucking = false;
		ReleaseDuck(StingerDuck);
	}
	if (bEncounterDucking && !AbyssAudioSystemPrivate::IsSounding(EncounterStingerComponent) && !bSuspended)
	{
		bEncounterDucking = false;
		ReleaseDuck(EncounterDuck);
	}
	if (DuckCurrentDb == DuckTargetDb)
	{
		return;
	}
	// Linear in dB: a full duck / release takes its in / out time.
	const float Depth = FMath::Max(1.f, FMath::Max(FMath::Abs(DuckCurrentDb), FMath::Abs(DuckTargetDb)));
	if (DuckCurrentDb > DuckTargetDb)
	{
		DuckCurrentDb = FMath::Max(DuckTargetDb, DuckCurrentDb - Depth * RealDeltaSec / FMath::Max(0.01f, DuckInSec));
	}
	else
	{
		DuckCurrentDb = FMath::Min(DuckTargetDb, DuckCurrentDb + Depth * RealDeltaSec / FMath::Max(0.01f, DuckOutSec));
	}
}

void UAbyssAudioSystem::EndStoryAudio()
{
	const float StingerFade = CurrentBeatKind == abyss::StoryBeatKind::Chapter ? Manifest.Stingers.ChapterCardFadeOutSec
		: Manifest.Stingers.BossIntroFadeOutSec;
	StopStinger(bBeatActive ? StingerFade : 0.f);
	StopWhisper();
	bBeatActive = false;
	bTitleStinger = false;
	if (!bStingerDucking && !bWhisperActive && !bEncounterDucking)
	{
		DuckTargetDb = 0.f;
	}
}

// =====================================================================================================================
// Per frame: hero state, footsteps (A7)
// =====================================================================================================================

void UAbyssAudioSystem::HandleFrame(const abyss::Snapshot& Snap, const FAbyssFrameInfo& Frame)
{
	bCinematic = Frame.bCinematic;
	const abyss::Vec2 Tile = AbyssUnits::LerpTile(Snap.hero.prevPos, Snap.hero.pos, Frame.Alpha);
	HeroTile = FVector2D(Tile.x, Tile.y);
	bHeroMoving = Snap.hero.moving && !Snap.frozen;
	bHeroAlive = Snap.hero.life == abyss::HeroLife::Alive;

	HeroSurface = NAME_None;
	if (const abyss::ZoneGrid* Grid = Snap.zone.grid)
	{
		const int32 Col = FMath::FloorToInt32(HeroTile.X + 0.5);
		const int32 Row = FMath::FloorToInt32(HeroTile.Y + 0.5);
		if (Grid->InBounds(Col, Row))
		{
			const FName TileName = AbyssAudioSystemPrivate::ToName(abyss::EnumName(Grid->Tile(Col, Row)));
			if (const FName* Surface = Manifest.Footsteps.TileSurface.Find(TileName))
			{
				HeroSurface = *Surface;
			}
		}
	}
	BindHero(Snap);
	TickFootstepFallback(Snap, static_cast<float>(Frame.RealDeltaMs / 1000.0));
	TickRemake(Snap, static_cast<float>(FMath::Max(0.0, Frame.RealDeltaMs) / 1000.0));
}

void UAbyssAudioSystem::BindHero(const abyss::Snapshot& Snap)
{
	(void)Snap;
	const UAbyssActorRegistry* Registry = UAbyssActorRegistry::Get(GetAudioWorld());
	AAbyssCharacterActor* Hero = Registry ? Registry->FindAs<AAbyssCharacterActor>(abyss::kHeroEntityId) : nullptr;
	if (Hero == BoundHero.Get())
	{
		return;
	}
	if (AAbyssCharacterActor* Old = BoundHero.Get())
	{
		Old->OnCharacterNotify.Remove(HeroNotifyHandle);
	}
	HeroNotifyHandle.Reset();
	BoundHero = Hero;
	if (Hero != nullptr)
	{
		HeroNotifyHandle = Hero->OnCharacterNotify.AddUObject(this, &UAbyssAudioSystem::HandleHeroNotify);
	}
}

void UAbyssAudioSystem::HandleHeroNotify(AAbyssCharacterActor* Actor, FName Notify)
{
	(void)Actor;
	if (!Manifest.Footsteps.Notifies.Contains(Notify))
	{
		return;
	}
	LastFootNotifySec = NowSec;
	if (bHeroMoving && bHeroAlive && !bCinematic)
	{
		PlayFootstep();
	}
}

void UAbyssAudioSystem::TickFootstepFallback(const abyss::Snapshot& Snap, float RealDeltaSec)
{
	(void)Snap;
	(void)RealDeltaSec;
	if (!bHeroMoving || !bHeroAlive || bCinematic)
	{
		FallbackDistanceCm = 0.0;
		LastFallbackTile = HeroTile;
		return;
	}
	// The Run / Walk clips carry FootL / FootR notifies; this cadence only covers clips without them.
	const double Moved = FVector2D::Distance(HeroTile, LastFallbackTile) * AbyssUnits::TileUU;
	LastFallbackTile = HeroTile;
	if (NowSec - LastFootNotifySec < Manifest.Footsteps.NotifyTimeoutSec || Moved > 300.0)
	{
		FallbackDistanceCm = 0.0;  // notifies are driving the steps, or a teleport / zone entry
		return;
	}
	FallbackDistanceCm += Moved;
	if (FallbackDistanceCm >= Manifest.Footsteps.FallbackStrideCm)
	{
		FallbackDistanceCm = FMath::Fmod(FallbackDistanceCm, static_cast<double>(Manifest.Footsteps.FallbackStrideCm));
		PlayFootstep();
	}
}

FName UAbyssAudioSystem::SurfaceUnderHero() const
{
	return HeroSurface.IsNone() ? FName(TEXT("grass")) : HeroSurface;
}

void UAbyssAudioSystem::PlayFootstep()
{
	if (bSuspended || NowSec - LastFootstepSec < Manifest.Footsteps.MinIntervalSec)
	{
		return;
	}
	const TArray<FName>* Variants = Manifest.Footsteps.Surfaces.Find(SurfaceUnderHero());
	if (Variants == nullptr || Variants->Num() == 0)
	{
		return;
	}
	int32 Index = Random.RandRange(0, Variants->Num() - 1);
	if (Variants->Num() > 1 && Index == LastFootstepVariant)
	{
		Index = (Index + 1) % Variants->Num();
	}
	LastFootstepVariant = Index;
	const FName Asset = (*Variants)[Index];
	USoundBase* Sound = GetSound(Asset, AbyssAudioSystemPrivate::FootstepFolder);
	if (Sound == nullptr)
	{
		return;
	}
	const FAbyssAudioAssetDef* Def = Manifest.FindAsset(Asset);
	PlayOneShot(Sound, (Def ? Def->Gain : 1.f) * Manifest.Footsteps.Gain * SfxBusGain(), /*bSpatial*/ false, HeroTile);
	LastFootstepSec = NowSec;
}

// =====================================================================================================================
// Dark remake (U7): music arbitration, ambience layers and emitters, cue triggers, voices on beats
// =====================================================================================================================

void UAbyssAudioSystem::BuildRemakeTables()
{
	if (!Remake.IsValid())
	{
		return;
	}
	FAbyssAudioRemake& R = *Remake;
	DefVoices.Reset();   // resolved against the tables built here
	R.Voices = AbyssVoiceRules::Parse(Manifest.VoiceFamiliesJson);
	R.Director = AbyssMusicRules::Parse(Manifest.MusicDirectorJson);
	R.Ambience = AbyssAmbienceRules::Parse(Manifest.AmbienceJson);
	R.Triggers = AbyssCueTriggerRules::Parse(Manifest.CueTriggersJson);
	R.AmbienceRegionAreas = AbyssAmbienceRules::Areas(R.Ambience.regions);
	R.AmbienceLayerAreas = AbyssAmbienceRules::Areas(R.Ambience.layers);
	R.AreaLoopAssets.Reset();
	for (const std::vector<AbyssAmbienceRules::AreaLoop>* Loops : { &R.Ambience.regions, &R.Ambience.layers })
	{
		for (const AbyssAmbienceRules::AreaLoop& Loop : *Loops)
		{
			R.AreaLoopAssets.Add(AbyssAudioSystemPrivate::ToName(Loop.asset));
		}
	}

	// Boss scores: what the core plays as boss music (classified above the siege and the regions).
	R.BossScores.clear();
	const UAbyssGameInstance* GI = GetAbyssGameInstance();
	if (const abyss::DataStore* Data = GI ? GI->GetData() : nullptr)
	{
		const abyss::MusicDirectorDef& Music = Data->Audio().music;
		R.BossScores.push_back(Music.bossCh1Score);
		for (const std::pair<std::string, std::string>& Row : Music.bossMusic)
		{
			R.BossScores.push_back(Row.second);
		}
	}
	for (const AbyssMusicRules::Boss& Boss : R.Director.bosses)
	{
		if (!Boss.track.empty())
		{
			R.BossScores.push_back(Boss.track);
		}
	}

	// The monsters a `while` reads (their liveness is gathered from the snapshot each frame).
	R.WatchedMonsters.clear();
	const auto Watch = [&R](const AbyssAudioData::WhileCondition& When)
	{
		if (!When.monsterAlive.empty() && !AbyssAudioData::Contains(R.WatchedMonsters, When.monsterAlive))
		{
			R.WatchedMonsters.push_back(When.monsterAlive);
		}
	};
	for (const AbyssMusicRules::Region& Region : R.Director.regions)
	{
		Watch(Region.when);
	}
	for (const AbyssAmbienceRules::AreaLoop& Loop : R.Ambience.regions)
	{
		Watch(Loop.when);
	}
	for (const AbyssAmbienceRules::AreaLoop& Loop : R.Ambience.layers)
	{
		Watch(Loop.when);
	}
	R.ResetSession();
	UE_LOG(LogAbyss, Log, TEXT("Audio U7: %d voiced monsters, %d named cues, %d music regions, %d region beds, %d layers, "
		"%d emitters, %d cue triggers"), static_cast<int32>(R.Voices.monsters.size()),
		static_cast<int32>(R.Voices.overrides.size()), static_cast<int32>(R.Director.regions.size()),
		static_cast<int32>(R.Ambience.regions.size()), static_cast<int32>(R.Ambience.layers.size()),
		static_cast<int32>(R.Ambience.emitters.size()), static_cast<int32>(R.Triggers.rows.size()));
}

void UAbyssAudioSystem::ResetRemakeSession()
{
	if (Remake.IsValid())
	{
		Remake->ResetSession();
	}
	AmbienceBedScale = 1.f;
	if (bEncounterDucking)
	{
		bEncounterDucking = false;
		ReleaseDuck(EncounterDuck);
	}
	if (AbyssAudioSystemPrivate::IsSounding(EncounterStingerComponent))
	{
		EncounterStingerComponent->Stop();
	}
}

void UAbyssAudioSystem::SetZoneEmitterSpots(FName EmitterId, const TArray<FVector2D>& Tiles)
{
	if (!Remake.IsValid() || EmitterId.IsNone())
	{
		return;
	}
	if (Tiles.Num() > 0)
	{
		Remake->SetPieceSpots.Add(EmitterId, Tiles);
	}
	else
	{
		Remake->SetPieceSpots.Remove(EmitterId);
	}
	Remake->bEmittersDirty = true;  // re-placed on the next frame
}

void UAbyssAudioSystem::TickRemake(const abyss::Snapshot& Snap, float RealDeltaSec)
{
	if (!bReady || !Remake.IsValid() || !bInSession)
	{
		return;
	}
	FAbyssAudioRemake& R = *Remake;
	if (R.ZoneId != Snap.zone.mapId)
	{
		R.ZoneId = Snap.zone.mapId;
	}

	// The watched monsters alive now (`while monsterAlive`).
	R.AliveNow.clear();
	for (const std::string& Id : R.WatchedMonsters)
	{
		for (const abyss::MonsterView& Monster : Snap.monsters)
		{
			if (Monster.alive && Monster.defId == Id)
			{
				R.AliveNow.push_back(Id);
				break;
			}
		}
	}
	const AbyssAudioData::TilePoint Tile = AbyssAudioData::TileOf(HeroTile.X, HeroTile.Y);
	const bool bInDirectorZone = R.Director.loaded && !R.Director.zone.empty() && Snap.zone.mapId == R.Director.zone;
	const abyss::QuestSystem* Quests = Snap.quests;
	const auto PhaseOf = [Quests](std::string_view QuestId)
	{
		return AbyssAudioSystemPrivate::PhaseOf(Quests, QuestId);
	};

	// Music layers. During a cutscene / boss intro they keep their choice (the core holds its own changes as well).
	if (!R.IsCutscene())
	{
		if (bInDirectorZone)
		{
			R.MusicRegions.Update(R.Director.regionAreas, Tile);
			R.Region = AbyssMusicRules::PickRegion(R.Director, R.MusicRegions,
				[&R](std::string_view Id) { return R.IsAlive(Id); });
			R.Siege = AbyssMusicRules::PickQuestTrack(R.Director, R.PlayingBeat, PhaseOf);
		}
		else
		{
			R.Region = -1;
			R.Siege = AbyssMusicRules::QuestTrackPick{};
		}
	}
	AbyssMusicRules::FrameState State;
	State.inDirectorZone = bInDirectorZone;
	State.sequence = !R.PlayingBeat.empty() && R.PlayingKind == abyss::StoryBeatKind::Sequence;
	State.siege = R.Siege;
	State.region = R.Region;
	AbyssMusicRules::DefaultFades Fades;
	const UAbyssGameInstance* GI = GetAbyssGameInstance();
	if (const abyss::DataStore* Data = GI ? GI->GetData() : nullptr)
	{
		Fades.stateFadeSec = Data->Audio().music.stateFadeSec;
		Fades.fadeInSec = Data->Audio().music.fadeInSec;
	}
	const FName Sounding = SoundingTrack();
	if (Sounding != R.SoundingName)
	{
		R.SoundingName = Sounding;
		R.SoundingStd = AbyssAudioSystemPrivate::ToStd(Sounding);
	}
	const AbyssMusicRules::Decision Decision = R.Arbiter.Update(R.Director, State, Fades, R.SoundingStd, NowSec);
	if (Decision.change)
	{
		PlayMusic(AbyssAudioSystemPrivate::ToName(Decision.track), static_cast<float>(Decision.fadeOutSec),
			static_cast<float>(Decision.fadeInSec), Decision.restart, Decision.loop);
	}

	TickRemakeAmbience(Snap, RealDeltaSec);

	// Cue trigger timers: burst members, `every` rows (never while a story beat plays).
	const bool bStoryBeat = !R.PlayingBeat.empty();
	const auto Holds = [&R, &Snap, &PhaseOf, Tile, bStoryBeat](size_t Row)
	{
		const AbyssCueTriggerRules::Trigger& T = R.Triggers.rows[Row];
		if (bStoryBeat || (!T.zone.empty() && T.zone != Snap.zone.mapId))
		{
			return false;
		}
		if (!T.whileQuestActive.empty() && PhaseOf(T.whileQuestActive) != AbyssMusicRules::QuestPhase::Active)
		{
			return false;
		}
		return !T.hasWithin || AbyssAudioData::Inside(T.within, Tile, /*bWasInside*/ false);
	};
	const std::vector<size_t> Due = R.Scheduler.Tick(R.Triggers, NowSec, Holds,
		[this]() { return static_cast<double>(Random.FRand()); });
	for (const size_t Row : Due)
	{
		PlayTriggerRow(Row);
	}
}

void UAbyssAudioSystem::TickRemakeAmbience(const abyss::Snapshot& Snap, float RealDeltaSec)
{
	FAbyssAudioRemake& R = *Remake;
	const AbyssAmbienceRules::AmbienceTables& Tables = R.Ambience;
	UWorld* AudioWorld = GetAudioWorld();
	if (AudioWorld == nullptr || R.bZoneTransition)
	{
		return;  // leaving the zone: what plays fades out (StopRemakeAmbience), nothing new starts
	}
	// The bed's bus: ambience gain, master x SFX volume, the cinematic duck.
	const float Shared = Manifest.Ambience.Gain * MasterVolume * SfxVolume *
		AbyssAudioSystemPrivate::DbToLinear(AmbienceDuckDb);
	const auto AssetGain = [this](FName Asset)
	{
		const FAbyssAudioAssetDef* Def = Manifest.FindAsset(Asset);
		return Def ? Def->Gain : 1.f;
	};
	const auto StartOffset = [this](FName Asset)
	{
		const FAbyssAudioAssetDef* Def = Manifest.FindAsset(Asset);
		const float Length = Def && Def->LengthSec > 1.f ? Def->LengthSec : 0.f;
		return Length > 0.f ? static_cast<float>(Random.FRandRange(0.f, Length - 1.f)) : 0.f;
	};

	// Region beds and layers: a weight ramps in / out with the area and its `while`; a region bed that replaces the
	// bed scales the bed by 1 - its weight (the crossfade).
	const AbyssAudioData::TilePoint Tile = AbyssAudioData::TileOf(HeroTile.X, HeroTile.Y);
	R.AmbienceRegions.Update(R.AmbienceRegionAreas, Tile);
	R.AmbienceLayers.Update(R.AmbienceLayerAreas, Tile);
	const size_t NumRegions = Tables.regions.size();
	const size_t NumLoops = NumRegions + Tables.layers.size();
	R.RegionWeights.resize(NumRegions, 0.0);
	R.LayerWeights.resize(Tables.layers.size(), 0.0);
	if (AreaLoopComponents.Num() != static_cast<int32>(NumLoops))
	{
		AreaLoopComponents.SetNum(static_cast<int32>(NumLoops));
	}
	const bool bFadingOut = NowSec < R.FadeOutUntilSec;  // the last zone's loops finish their fade-out
	double Replaced = 0.0;
	for (size_t I = 0; I < NumLoops; ++I)
	{
		const bool bRegion = I < NumRegions;
		const AbyssAmbienceRules::AreaLoop& Loop = bRegion ? Tables.regions[I] : Tables.layers[I - NumRegions];
		double& Weight = bRegion ? R.RegionWeights[I] : R.LayerWeights[I - NumRegions];
		const bool bInside = bRegion ? R.AmbienceRegions.IsInside(I) : R.AmbienceLayers.IsInside(I - NumRegions);
		const bool bOn = (Loop.zone.empty() || Loop.zone == Snap.zone.mapId) && bInside &&
			AbyssAudioData::Holds(Loop.when, [&R](std::string_view Id) { return R.IsAlive(Id); });
		Weight = AbyssAudioData::Ramp(Weight, bOn, Loop.fadeInSec, Loop.fadeOutSec, RealDeltaSec);
		if (bRegion && Loop.replacesBed)
		{
			Replaced = FMath::Max(Replaced, Weight);
		}
		TObjectPtr<UAudioComponent>& Comp = AreaLoopComponents[static_cast<int32>(I)];
		if (Weight <= 0.0)
		{
			if (AbyssAudioSystemPrivate::IsSounding(Comp) && !bFadingOut)
			{
				Comp->Stop();
			}
			continue;
		}
		const FName Asset = R.AreaLoopAssets.IsValidIndex(static_cast<int32>(I)) ? R.AreaLoopAssets[static_cast<int32>(I)]
			: AbyssAudioSystemPrivate::ToName(Loop.asset);
		const float Volume = static_cast<float>(Weight) * AssetGain(Asset) * Shared;
		if (!AbyssAudioSystemPrivate::IsSounding(Comp))
		{
			USoundBase* Sound = GetSound(Asset, AbyssAudioSystemPrivate::AmbienceFolder);
			if (Sound == nullptr)
			{
				continue;
			}
			if (!IsValid(Comp) || Comp->GetWorld() != AudioWorld)
			{
				Comp = UGameplayStatics::CreateSound2D(AudioWorld, Sound, 1.f, 1.f, 0.f, nullptr, true, false);
				if (Comp == nullptr)
				{
					continue;
				}
				Comp->bIsUISound = true;
				Comp->bAllowSpatialization = false;
			}
			else
			{
				Comp->SetSound(Sound);
			}
			Comp->SetVolumeMultiplier(Volume);
			Comp->Play(StartOffset(Asset));
			if (bSuspended)
			{
				Comp->SetPaused(true);
			}
		}
		AbyssAudioSystemPrivate::SetVolumeIfChanged(Comp, Volume);
	}
	AmbienceBedScale = static_cast<float>(1.0 - Replaced);
	AbyssAudioSystemPrivate::SetVolumeIfChanged(AmbienceComponent, AmbienceGain());

	// Emitters: placed once per zone (or when the set pieces hand their spots over), each spot a looping point source
	// with linear distance attenuation and the A4 panning of its direction.
	if (R.bEmittersDirty && !bFadingOut)
	{
		R.bEmittersDirty = false;
		for (const TObjectPtr<UAudioComponent>& Comp : EmitterComponents)
		{
			if (AbyssAudioSystemPrivate::IsSounding(Comp))
			{
				Comp->Stop();
			}
		}
		R.EmitterSpots.clear();
		const abyss::DataStore* Data = GetAbyssGameInstance() ? GetAbyssGameInstance()->GetData() : nullptr;
		const abyss::MapDef* Map = Data ? Data->FindMap(Snap.zone.mapId) : nullptr;
		std::vector<AbyssAmbienceRules::Spot> Spots;
		for (const AbyssAmbienceRules::Emitter& Emitter : Tables.emitters)
		{
			if (!Emitter.zone.empty() && Emitter.zone != Snap.zone.mapId)
			{
				continue;
			}
			std::vector<std::pair<double, double>> Pieces;
			if (const TArray<FVector2D>* Found = R.SetPieceSpots.Find(AbyssAudioSystemPrivate::ToName(Emitter.id)))
			{
				for (const FVector2D& Spot : *Found)
				{
					Pieces.emplace_back(Spot.X, Spot.Y);
				}
			}
			AbyssAmbienceRules::EmitterSpots(Emitter, Pieces, Map, Spots);
			for (const AbyssAmbienceRules::Spot& Spot : Spots)
			{
				R.EmitterSpots.push_back(FAbyssAudioRemake::FEmitterSpot{ AbyssAudioSystemPrivate::ToName(Spot.asset),
					Spot.col, Spot.row, Emitter.innerTiles, Emitter.falloffTiles });
			}
		}
		EmitterComponents.SetNum(static_cast<int32>(R.EmitterSpots.size()));
	}
	for (size_t I = 0; I < R.EmitterSpots.size(); ++I)
	{
		const FAbyssAudioRemake::FEmitterSpot& Spot = R.EmitterSpots[I];
		TObjectPtr<UAudioComponent>& Comp = EmitterComponents[static_cast<int32>(I)];
		const FVector2D SpotTile(Spot.Col, Spot.Row);
		const double Gain = AbyssAmbienceRules::EmitterGain(FVector2D::Distance(HeroTile, SpotTile), Spot.InnerTiles,
			Spot.FalloffTiles);
		if (Gain <= 0.0)
		{
			if (AbyssAudioSystemPrivate::IsSounding(Comp))
			{
				Comp->Stop();
			}
			continue;
		}
		const FName Asset = Spot.Asset;
		const float Volume = static_cast<float>(Gain) * AssetGain(Asset) * Shared;
		FVector Location;
		const bool bPanned = ComputeVirtualEmitter(SpotTile, Location);
		if (bPanned && IsValid(Comp) && !Comp->bAllowSpatialization)
		{
			// A loop that started as a plain 2D sound (no listener on its first frames) becomes a panned point source
			// as soon as a listener exists: the 2D component is dropped and the spot starts again below through
			// SpawnSoundAtLocation (the slot is reused across zones, so it would otherwise stay unpanned for good).
			Comp->Stop();
			Comp->DestroyComponent();
			Comp = nullptr;
		}
		if (!AbyssAudioSystemPrivate::IsSounding(Comp))
		{
			USoundBase* Sound = GetSound(Asset, AbyssAudioSystemPrivate::AmbienceFolder);
			if (Sound == nullptr)
			{
				continue;
			}
			if (!IsValid(Comp) || Comp->GetWorld() != AudioWorld)
			{
				Comp = bPanned
					? UGameplayStatics::SpawnSoundAtLocation(AudioWorld, Sound, Location, FRotator::ZeroRotator, Volume,
						1.f, StartOffset(Asset), GetSpatialAttenuation(), nullptr, /*bAutoDestroy*/ false)
					: UGameplayStatics::CreateSound2D(AudioWorld, Sound, Volume, 1.f, 0.f, nullptr, true, false);
				if (Comp == nullptr)
				{
					continue;
				}
				if (!bPanned)
				{
					// No listener yet (loading): a plain 2D loop, attenuated like the panned one.
					Comp->bIsUISound = true;
					Comp->bAllowSpatialization = false;
					Comp->Play(StartOffset(Asset));
				}
			}
			else
			{
				Comp->SetSound(Sound);
				Comp->SetVolumeMultiplier(Volume);
				Comp->Play(StartOffset(Asset));
			}
			if (bSuspended)
			{
				Comp->SetPaused(true);
			}
		}
		if (bPanned && Comp->bAllowSpatialization)
		{
			Comp->SetWorldLocation(Location);
		}
		AbyssAudioSystemPrivate::SetVolumeIfChanged(Comp, Volume);
	}
}

void UAbyssAudioSystem::StopRemakeAmbience(float FadeOutSec)
{
	for (const TArray<TObjectPtr<UAudioComponent>>* List : { &AreaLoopComponents, &EmitterComponents })
	{
		for (const TObjectPtr<UAudioComponent>& Comp : *List)
		{
			if (!AbyssAudioSystemPrivate::IsSounding(Comp))
			{
				continue;
			}
			if (FadeOutSec > 0.f)
			{
				Comp->FadeOut(FadeOutSec, 0.f, EAudioFaderCurve::Linear);
			}
			else
			{
				Comp->Stop();
			}
		}
	}
	AmbienceBedScale = 1.f;
	if (Remake.IsValid())
	{
		std::fill(Remake->RegionWeights.begin(), Remake->RegionWeights.end(), 0.0);
		std::fill(Remake->LayerWeights.begin(), Remake->LayerWeights.end(), 0.0);
		Remake->EmitterSpots.clear();
		Remake->bEmittersDirty = true;
		Remake->FadeOutUntilSec = FadeOutSec > 0.f ? NowSec + FadeOutSec : -1.0;
	}
}

bool UAbyssAudioSystem::PlayNamedCue(FName CueId, const FVector2D& Tile)
{
	EnsureReady();
	if (!bReady || bSuspended || CueId.IsNone())
	{
		return false;
	}
	const FAbyssAudioCueDef* Cue = Manifest.FindCue(CueId);
	if (Cue == nullptr)
	{
		return false;
	}
	return PlayCue(*Cue, NAME_None, Cue->bSpatial && !Cue->bUiBus, Tile) != nullptr;
}

void UAbyssAudioSystem::PlayTriggerRow(size_t Row)
{
	if (!Remake.IsValid() || Row >= Remake->Triggers.rows.size())
	{
		return;
	}
	const FAbyssAudioRemake& R = *Remake;
	const AbyssCueTriggerRules::Trigger& T = R.Triggers.rows[Row];
	// Near / far: the far cue only when the hero is known to be outside the named music region.
	bool bInsideNear = true;
	if (AbyssCueTriggerRules::NeedsRegion(T))
	{
		for (const AbyssMusicRules::Region& Region : R.Director.regions)
		{
			if (Region.id == T.nearRegion)
			{
				bInsideNear = R.ZoneId == R.Director.zone &&
					AbyssAudioData::Inside(Region.area, AbyssAudioData::TileOf(HeroTile.X, HeroTile.Y), false);
				break;
			}
		}
	}
	const FName CueId = AbyssAudioSystemPrivate::ToName(AbyssCueTriggerRules::CueFor(T, bInsideNear));
	const FVector2D Tile = T.hasTile ? FVector2D(T.tileCol, T.tileRow) : HeroTile;
	if (PlayNamedCue(CueId, Tile))
	{
		OnCuePlayed.Broadcast(CueId);
	}
}

void UAbyssAudioSystem::StartTriggerRows(const std::vector<size_t>& Rows, std::string_view BoundBeat)
{
	if (!Remake.IsValid() || Rows.empty())
	{
		return;
	}
	for (const size_t Row : Rows)
	{
		Remake->Scheduler.Start(Remake->Triggers, Row, NowSec, BoundBeat);
	}
	for (const size_t Row : Remake->Scheduler.TakeDue(NowSec))
	{
		PlayTriggerRow(Row);  // the first member at once; a burst's others follow on the frame timer
	}
}

void UAbyssAudioSystem::PlayEncounterStinger(abyss::EntityId Entity, std::string_view DefId, bool bSlain)
{
	if (!Remake.IsValid() || DefId.empty() || bSuspended)
	{
		return;
	}
	FAbyssAudioRemake& R = *Remake;
	const AbyssMusicRules::Stinger* Stinger = AbyssMusicRules::EncounterStinger(R.Director, DefId,
		bSlain ? AbyssMusicRules::EncounterBeat::Slain : AbyssMusicRules::EncounterBeat::Engaged);
	if (Stinger == nullptr)
	{
		return;
	}
	if (bSlain)
	{
		R.EngagedEntities.Remove(Entity);
	}
	else
	{
		if (R.EngagedEntities.Contains(Entity))
		{
			return;  // once per fight with that instance
		}
		R.EngagedEntities.Add(Entity);
	}
	const FName Asset = AbyssAudioSystemPrivate::ToName(Stinger->asset);
	USoundBase* Sound = GetSound(Asset, AbyssAudioSystemPrivate::StingerFolder);
	UWorld* AudioWorld = GetAudioWorld();
	if (Sound == nullptr || AudioWorld == nullptr)
	{
		return;
	}
	if (!IsValid(EncounterStingerComponent) || EncounterStingerComponent->GetWorld() != AudioWorld)
	{
		EncounterStingerComponent = UGameplayStatics::CreateSound2D(AudioWorld, Sound, 1.f, 1.f, 0.f, nullptr, true, false);
		if (EncounterStingerComponent == nullptr)
		{
			return;
		}
		EncounterStingerComponent->bIsUISound = true;
		EncounterStingerComponent->bAllowSpatialization = false;
	}
	else
	{
		EncounterStingerComponent->Stop();
		EncounterStingerComponent->SetSound(Sound);
	}
	const FAbyssAudioAssetDef* Def = Manifest.FindAsset(Asset);
	EncounterStingerGain = Def ? Def->Gain : 1.f;
	EncounterStingerComponent->SetVolumeMultiplier(EncounterStingerGain * MasterVolume * MusicVolume);
	EncounterStingerComponent->Play();
	// The stinger's own duck (musicDirector.stingers.<id>.duck), released when it ends (TickDuck).
	EncounterDuck.Db = static_cast<float>(Stinger->duckDb);
	EncounterDuck.InSec = static_cast<float>(FMath::Max(0.0, Stinger->duckInSec));
	EncounterDuck.OutSec = static_cast<float>(FMath::Max(0.0, Stinger->duckOutSec));
	bEncounterDucking = true;
	SetDuck(EncounterDuck);
}

void UAbyssAudioSystem::HandlePlayAnim(const abyss::EvPlayAnim& Event)
{
	if (!bReady || !Remake.IsValid() || Remake->Voices.overrides.empty() || Event.entity == abyss::kNoEntity ||
		Event.entity == abyss::kHeroEntityId)
	{
		return;
	}
	const FDefVoice& Voice = VoiceOfDef(DefNameOfEntity(Event.entity));
	if (!Voice.bHasAnimCue)
	{
		return;   // nearly every monster: no anim.<action> cue, no string work
	}
	const std::string_view Named =
		AbyssVoiceRules::OverrideCue(Remake->Voices, Voice.DefId, AbyssVoiceRules::AnimKey(abyss::EnumName(Event.action)));
	if (!Named.empty())
	{
		// e.g. the boss intro's focus-step roar (vox_valdras_roar), every Corpseherd cast (her handbell).
		PlayNamedCue(AbyssAudioSystemPrivate::ToName(Named), EntityTile(Event.entity));
	}
}

void UAbyssAudioSystem::HandleMiniBossDialogue(const abyss::EvMiniBossDialogue& Event)
{
	EnsureReady();
	if (!bReady || !Remake.IsValid())
	{
		return;
	}
	if (Event.opened)
	{
		// Her lullaby, hummed once under the pre-fight lines (miniboss.opened).
		const std::string_view Named = AbyssVoiceRules::OverrideCue(Remake->Voices, Event.monsterId,
			AbyssVoiceRules::KeyMiniBossOpened);
		if (!Named.empty())
		{
			PlayNamedCue(AbyssAudioSystemPrivate::ToName(Named), EntityTile(Event.monster));
		}
		return;
	}
	PlayEncounterStinger(Event.monster, Event.monsterId, /*bSlain*/ false);  // the lines dismissed: the fight starts
}

void UAbyssAudioSystem::HandleRandomEvent(const abyss::EvRandomEvent& Event)
{
	EnsureReady();
	if (!bReady || !Remake.IsValid())
	{
		return;
	}
	StartTriggerRows(AbyssCueTriggerRules::MatchRandomEvent(Remake->Triggers, abyss::EnumName(Event.type),
		Event.resolved, Event.zoneId), std::string_view());
}

void UAbyssAudioSystem::HandleStoryDecorFocus(const abyss::EvStoryDecorFocus& Event)
{
	EnsureReady();
	if (!bReady || !Remake.IsValid() || Event.decorId == Remake->DecorFocus)
	{
		return;  // the same decoration (its tooltip toggled): nothing new
	}
	Remake->DecorFocus = Event.decorId;
	StartTriggerRows(AbyssCueTriggerRules::MatchDecorFocus(Remake->Triggers, Event.decorId), std::string_view());
}

// =====================================================================================================================
// Title-menu soundtrack (audio.md 8.3, FIX Q10)
// =====================================================================================================================

void UAbyssAudioSystem::RefreshJukeboxRows()
{
	EnsureReady();
	if (bJukeboxRowsBuilt || !bReady)
	{
		return;
	}
	bJukeboxRowsBuilt = true;
	JukeboxRows.Reset();
	for (int32 I = 0; I < Manifest.Jukebox.Tracks.Num(); ++I)
	{
		const FAbyssMusicTrackDef Track = Manifest.ResolveTrack(Manifest.Jukebox.Tracks[I].Track);
		if (GetSound(Track.Asset, AbyssAudioSystemPrivate::MusicFolder) != nullptr)
		{
			JukeboxRows.Add(I);  // packaged in this build
		}
	}
}

TArray<FAbyssJukeboxTrack> UAbyssAudioSystem::GetJukeboxTracks()
{
	RefreshJukeboxRows();
	TArray<FAbyssJukeboxTrack> Out;
	for (const int32 RowIndex : JukeboxRows)
	{
		const FAbyssJukeboxEntry& Entry = Manifest.Jukebox.Tracks[RowIndex];
		const FAbyssMusicTrackDef Track = Manifest.ResolveTrack(Entry.Track);
		FAbyssJukeboxTrack Info;
		Info.TrackKey = Entry.Track;
		Info.TitleKey = Entry.TitleKey;
		Info.LengthSec = static_cast<float>(ResolveTrackDuration(Track, GetSound(Track.Asset,
			AbyssAudioSystemPrivate::MusicFolder)));
		Out.Add(MoveTemp(Info));
	}
	return Out;
}

void UAbyssAudioSystem::JukeboxPlay(int32 Index)
{
	JukeboxStart(Index, Manifest.Jukebox.FadeOutSec);
}

void UAbyssAudioSystem::JukeboxStart(int32 Index, float FadeOutSec)
{
	RefreshJukeboxRows();
	if (!JukeboxRows.IsValidIndex(Index))
	{
		return;
	}
	JukeboxIndex = Index;
	// One pass per row (no loop): TickJukebox advances at the real end of the track.
	PlayMusic(Manifest.Jukebox.Tracks[JukeboxRows[Index]].Track, FadeOutSec, Manifest.Jukebox.FadeInSec,
		/*bRestart*/ true, /*bLoopRequested*/ false);
}

void UAbyssAudioSystem::JukeboxSetPaused(bool bPaused)
{
	if (!IsJukeboxActive())
	{
		return;
	}
	const int32 Slot = FindActiveMusicVoice();
	if (Slot == INDEX_NONE)
	{
		if (!bPaused)
		{
			JukeboxPlay(JukeboxIndex);  // the row ended (last row holds at its end): play it again
		}
		return;
	}
	FMusicVoice& Voice = MusicVoices[Slot];
	if (!bPaused && Voice.DurationSec > 0.0 && Voice.PositionSec >= Voice.DurationSec - 0.05)
	{
		JukeboxPlay(JukeboxIndex);
		return;
	}
	Voice.bPaused = bPaused;
	if (UAudioComponent* Comp = MusicComponents[Slot].Get(); IsValid(Comp))
	{
		Comp->SetPaused(bPaused || bSuspended);
	}
}

void UAbyssAudioSystem::JukeboxSeek(float Seconds)
{
	if (!IsJukeboxActive())
	{
		return;
	}
	const int32 Slot = FindActiveMusicVoice();
	if (Slot == INDEX_NONE)
	{
		JukeboxPlay(JukeboxIndex);
		return;
	}
	FMusicVoice& Voice = MusicVoices[Slot];
	UAudioComponent* Comp = MusicComponents[Slot].Get();
	if (!IsValid(Comp))
	{
		return;
	}
	const double MaxSec = Voice.DurationSec > 0.0 ? FMath::Max(0.0, Voice.DurationSec - 0.05) : TNumericLimits<float>::Max();
	const double Target = FMath::Clamp(static_cast<double>(Seconds), 0.0, MaxSec);
	Comp->SetVolumeMultiplier(Voice.Gain * MusicBusGain());
	Comp->Play(static_cast<float>(Target));
	Comp->SetPaused(Voice.bPaused || bSuspended);
	Voice.PositionSec = Target;
	ResumePositions.Add(Voice.TrackKey, Target);
}

void UAbyssAudioSystem::JukeboxClose()
{
	if (!IsJukeboxActive())
	{
		return;
	}
	EndJukebox();
	const UAbyssGameInstance* GI = GetAbyssGameInstance();
	const abyss::DataStore* Data = GI ? GI->GetData() : nullptr;
	const float FadeOut = Data ? static_cast<float>(Data->Audio().music.stateFadeSec) : 1.5f;
	const float FadeIn = Data ? static_cast<float>(Data->Audio().music.fadeInSec) : 1.f;
	PlayMusic(Manifest.MenuTrack, FadeOut, FadeIn, /*bRestart*/ true, /*bLoopRequested*/ true);
}

bool UAbyssAudioSystem::IsJukeboxPaused() const
{
	const int32 Slot = IsJukeboxActive() ? FindActiveMusicVoice() : INDEX_NONE;
	// A row that reached its end (last row) counts as paused: the play button restarts it.
	return IsJukeboxActive() && (Slot == INDEX_NONE || MusicVoices[Slot].bPaused);
}

float UAbyssAudioSystem::GetJukeboxPositionSec() const
{
	if (!IsJukeboxActive())
	{
		return 0.f;
	}
	const int32 Slot = FindActiveMusicVoice();
	if (Slot == INDEX_NONE)
	{
		return GetJukeboxLengthSec();
	}
	const FMusicVoice& Voice = MusicVoices[Slot];
	return static_cast<float>(Voice.DurationSec > 0.0 ? FMath::Min(Voice.PositionSec, Voice.DurationSec) : Voice.PositionSec);
}

float UAbyssAudioSystem::GetJukeboxLengthSec() const
{
	if (!IsJukeboxActive() || !JukeboxRows.IsValidIndex(JukeboxIndex))
	{
		return 0.f;
	}
	const FAbyssMusicTrackDef Track = Manifest.ResolveTrack(Manifest.Jukebox.Tracks[JukeboxRows[JukeboxIndex]].Track);
	const TObjectPtr<USoundBase>* Sound = Sounds.Find(Track.Asset);
	return static_cast<float>(ResolveTrackDuration(Track, Sound ? Sound->Get() : nullptr));
}

void UAbyssAudioSystem::TickJukebox()
{
	if (!IsJukeboxActive() || bSuspended)
	{
		return;
	}
	const int32 Slot = FindActiveMusicVoice();
	if (Slot == INDEX_NONE)
	{
		return;
	}
	FMusicVoice& Voice = MusicVoices[Slot];
	constexpr double EndLeadSec = 0.05;
	if (Voice.bPaused || Voice.DurationSec <= 0.0 || Voice.PositionSec < Voice.DurationSec - EndLeadSec)
	{
		return;
	}
	// End of the row: the next row, or hold at the end of the last one (web MenuScene). The ending row is cut with a
	// short fade: the music waves loop, a long fade-out would let the row's opening bars sound under the next one.
	if (JukeboxRows.IsValidIndex(JukeboxIndex + 1))
	{
		JukeboxStart(JukeboxIndex + 1, static_cast<float>(EndLeadSec));
	}
	else
	{
		Voice.PositionSec = Voice.DurationSec;
		Voice.bPaused = true;
		if (UAudioComponent* Comp = MusicComponents[Slot].Get(); IsValid(Comp))
		{
			Comp->SetPaused(true);
		}
	}
}

void UAbyssAudioSystem::EndJukebox()
{
	if (!IsJukeboxActive())
	{
		return;
	}
	JukeboxIndex = INDEX_NONE;
	for (int32 I = 0; I < MusicVoices.Num(); ++I)
	{
		if (MusicVoices[I].bPaused)
		{
			MusicVoices[I].bPaused = false;
			if (MusicComponents.IsValidIndex(I) && IsValid(MusicComponents[I]))
			{
				MusicComponents[I]->SetPaused(bSuspended);
			}
		}
	}
}

TArray<FAbyssMusicCreditRow> UAbyssAudioSystem::GetMusicCredits() const
{
	TArray<FAbyssMusicCreditRow> Out;
	for (const TPair<FName, FAbyssMusicTrackDef>& Pair : Manifest.GetTracks())
	{
		if (!Pair.Value.Credit.IsSet())
		{
			continue;
		}
		const FAbyssMusicCredit& Credit = Pair.Value.Credit.GetValue();
		FAbyssMusicCreditRow* Row = Out.FindByPredicate([&Credit](const FAbyssMusicCreditRow& R)
		{
			return R.Credit.Title == Credit.Title && R.Credit.Author == Credit.Author;
		});
		if (Row == nullptr)
		{
			Row = &Out.AddDefaulted_GetRef();
			Row->Credit = Credit;
		}
		Row->Tracks.Add(Pair.Key);
	}
	return Out;
}

// =====================================================================================================================
// Diagnostics
// =====================================================================================================================

FString UAbyssAudioSystem::DescribeState() const
{
	FString Out = FString::Printf(TEXT("Audio: ready=%d suspended=%d session=%d manifest=%s volumes master %.2f music %.2f "
		"sfx %.2f duck %.1f dB (target %.1f)\n"), bReady ? 1 : 0, bSuspended ? 1 : 0, bInSession ? 1 : 0,
		Manifest.IsFromFile() ? TEXT("file") : TEXT("defaults"), MasterVolume, MusicVolume, SfxVolume, DuckCurrentDb,
		DuckTargetDb);
	Out += FString::Printf(TEXT("  current track: %s, jukebox row %d%s\n"), *CurrentTrack.ToString(), JukeboxIndex,
		IsJukeboxPaused() ? TEXT(" (paused)") : TEXT(""));
	for (int32 I = 0; I < MusicVoices.Num(); ++I)
	{
		const FMusicVoice& Voice = MusicVoices[I];
		Out += FString::Printf(TEXT("  voice %d: %s %s%s pos %.2f / %.2f s gain %.3f playing=%d\n"), I,
			*Voice.TrackKey.ToString(), Voice.bActive ? TEXT("active") : (Voice.bFadingOut ? TEXT("fading") : TEXT("idle")),
			Voice.bPaused ? TEXT(" paused") : TEXT(""),
			Voice.PositionSec, Voice.DurationSec, Voice.Gain,
			MusicComponents.IsValidIndex(I) && AbyssAudioSystemPrivate::IsSounding(MusicComponents[I]) ? 1 : 0);
	}
	for (const TPair<FName, double>& Pair : ResumePositions)
	{
		Out += FString::Printf(TEXT("  resume %s at %.2f s\n"), *Pair.Key.ToString(), Pair.Value);
	}
	Out += FString::Printf(TEXT("  ambience: %s, stinger playing=%d, whisper=%d, hero surface %s\n"),
		*CurrentAmbience.ToString(), AbyssAudioSystemPrivate::IsSounding(StingerComponent) ? 1 : 0, bWhisperActive ? 1 : 0,
		*SurfaceUnderHero().ToString());
	if (Remake.IsValid())
	{
		const FAbyssAudioRemake& R = *Remake;
		const FString RegionId = R.Region >= 0 && static_cast<size_t>(R.Region) < R.Director.regions.size()
			? AbyssText::ToFString(R.Director.regions[static_cast<size_t>(R.Region)].id) : FString(TEXT("-"));
		Out += FString::Printf(TEXT("  remake: zone %s, music region %s, siege %d%s, hold %d, source %d, beat %s, "
			"bed scale %.2f, emitters %d, triggers %d, voice families %d\n"), *AbyssText::ToFString(R.ZoneId), *RegionId,
			R.Siege.index, R.Siege.then ? TEXT(" (then)") : TEXT(""), R.Arbiter.IsHolding() ? 1 : 0,
			static_cast<int32>(R.Arbiter.CurrentSource()), *AbyssText::ToFString(R.PlayingBeat), AmbienceBedScale,
			static_cast<int32>(R.EmitterSpots.size()), static_cast<int32>(R.Triggers.rows.size()),
			static_cast<int32>(R.Voices.monsters.size()));
		for (size_t I = 0; I < R.RegionWeights.size() && I < R.Ambience.regions.size(); ++I)
		{
			Out += FString::Printf(TEXT("  region bed %s weight %.2f\n"), *AbyssText::ToFString(R.Ambience.regions[I].id),
				R.RegionWeights[I]);
		}
		for (size_t I = 0; I < R.LayerWeights.size() && I < R.Ambience.layers.size(); ++I)
		{
			Out += FString::Printf(TEXT("  layer %s weight %.2f\n"), *AbyssText::ToFString(R.Ambience.layers[I].id),
				R.LayerWeights[I]);
		}
	}
	Out += FString::Printf(TEXT("  sounds loaded %d, missing %d"), Sounds.Num(), MissingSounds.Num());
	for (const FName Missing : MissingSounds)
	{
		Out += TEXT(" ") + Missing.ToString();
	}
	return Out;
}

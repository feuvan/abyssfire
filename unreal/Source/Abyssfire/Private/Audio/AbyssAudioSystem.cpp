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

#include <string>
#include <string_view>

#include "abyss/base/Enums.h"
#include "abyss/base/Types.h"
#include "abyss/data/DataStore.h"
#include "abyss/data/MonsterData.h"
#include "abyss/data/StoryData.h"
#include "abyss/hero/Hero.h"
#include "abyss/world/Grid.h"

#include "Abyssfire.h"
#include "Actors/AbyssCharacterActor.h"
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
	for (UAudioComponent* Comp : { StingerComponent.Get(), WhisperComponent.Get(), AmbienceComponent.Get() })
	{
		if (IsValid(Comp))
		{
			Comp->Stop();
		}
	}
	StingerComponent = nullptr;
	WhisperComponent = nullptr;
	AmbienceComponent = nullptr;
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
	EntityFamilies.Reset();
	CueStates.Reset();
	LastFootNotifySec = -1000.0;
	FallbackDistanceCm = 0.0;
}

void UAbyssAudioSystem::HandleSessionEnded()
{
	bInSession = false;
	StopAllSfx();
	EndStoryAudio();
	StopAmbience(Manifest.Ambience.FadeOutSec);
	if (AAbyssCharacterActor* Hero = BoundHero.Get())
	{
		Hero->OnCharacterNotify.Remove(HeroNotifyHandle);
	}
	BoundHero.Reset();
	HeroNotifyHandle.Reset();
	EntityFamilies.Reset();
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
	return AmbienceAssetGain * Manifest.Ambience.Gain * MasterVolume * SfxVolume *
		AbyssAudioSystemPrivate::DbToLinear(AmbienceDuckDb);
}

// =====================================================================================================================
// Music (EvMusic)
// =====================================================================================================================

void UAbyssAudioSystem::HandleMusic(const abyss::EvMusic& Event)
{
	EnsureReady();
	EndJukebox();
	FName TrackKey = AbyssAudioSystemPrivate::ToName(Event.trackKey);
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
	PlayMusic(TrackKey, static_cast<float>(Event.fadeOutSec), static_cast<float>(Event.fadeInSec), Event.restart,
		Event.loop);
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
	if (TrackKey.IsNone())
	{
		return;  // silence (no theme for the zone)
	}
	const FAbyssMusicTrackDef Track = Manifest.ResolveTrack(TrackKey);
	USoundBase* Sound = GetSound(Track.Asset, AbyssAudioSystemPrivate::MusicFolder);
	if (Sound == nullptr)
	{
		return;
	}
	const int32 Slot = FindFreeMusicSlot();
	UAudioComponent* Comp = Slot != INDEX_NONE ? EnsureMusicComponent(Slot, Sound) : nullptr;
	if (Comp == nullptr)
	{
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
	const FAbyssAudioCueDef* Cue = Manifest.FindCue(CueId);
	if (Cue == nullptr)
	{
		return;
	}
	const FName Family = (Cue->Families.Num() > 0 || Cue->VocalLayer.Num() > 0) ? FamilyOfEntity(Event.source)
		: FName(NAME_None);
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
	PlayCue(*Cue, Family, bSpatial, Tile);

	// A7: the family's death vocal on top of the web's monster_death groan.
	if (!Family.IsNone())
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

FName UAbyssAudioSystem::FamilyOfEntity(abyss::EntityId Id) const
{
	if (Id == abyss::kNoEntity)
	{
		return NAME_None;
	}
	if (const FName* Found = EntityFamilies.Find(Id))
	{
		return *Found;
	}
	const UAbyssGameInstance* GI = GetAbyssGameInstance();
	const abyss::DataStore* Data = GI ? GI->GetData() : nullptr;
	const UAbyssSimDriver* Driver = UAbyssSimDriver::Get(GetAudioWorld());
	if (Data == nullptr || Driver == nullptr)
	{
		return NAME_None;
	}
	if (const abyss::MonsterView* Monster = Driver->GetSnapshotIndex().FindMonster(Id))
	{
		if (const abyss::MonsterDef* Def = Data->FindMonster(Monster->defId))
		{
			return AbyssAudioSystemPrivate::ToName(abyss::EnumName(Def->animCategory));
		}
	}
	return NAME_None;
}

void UAbyssAudioSystem::HandleEntitySpawned(const abyss::EvEntitySpawned& Event)
{
	if (Event.kind != abyss::EntityKind::Monster)
	{
		return;
	}
	const UAbyssGameInstance* GI = GetAbyssGameInstance();
	const abyss::DataStore* Data = GI ? GI->GetData() : nullptr;
	if (const abyss::MonsterDef* Def = Data ? Data->FindMonster(Event.defId) : nullptr)
	{
		EntityFamilies.Add(Event.id, AbyssAudioSystemPrivate::ToName(abyss::EnumName(Def->animCategory)));
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
		break;
	case abyss::EvZone::Phase::Exited:
		StopAmbience(Manifest.Ambience.FadeOutSec);
		EntityFamilies.Reset();  // the next zone's spawns follow
		break;
	case abyss::EvZone::Phase::Entered:
	{
		FName Asset;
		if (const FName* ByZone = Manifest.Ambience.ByZone.Find(AbyssAudioSystemPrivate::ToName(Event.mapId)))
		{
			Asset = *ByZone;
		}
		else if (const UAbyssGameInstance* GI = GetAbyssGameInstance())
		{
			if (const abyss::Snapshot* Snap = GI->GetSnapshot())
			{
				if (const FName* ByTheme = Manifest.Ambience.ByTheme.Find(
						AbyssAudioSystemPrivate::ToName(abyss::EnumName(Snap->zone.theme))))
				{
					Asset = *ByTheme;
				}
			}
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
		return;
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
	if (!bStingerDucking && !bWhisperActive)
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
	Out += FString::Printf(TEXT("  sounds loaded %d, missing %d"), Sounds.Num(), MissingSounds.Num());
	for (const FName Missing : MissingSounds)
	{
		Out += TEXT(" ") + Missing.ToString();
	}
	return Out;
}

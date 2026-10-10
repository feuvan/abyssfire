// FAbyssAudioManifest: where every rendered sound lives and how the UE layer plays it (unreal/Audio/Export/
// audio_manifest.json, written by unreal/Audio/render/render_all.py; audio.md 9, 10, 11; DECISIONS A1-A7).
//
// The manifest is presentation data only (asset paths, loop flags, trims, stinger / ambience / footstep tables, duck
// amounts): which cue plays when is decided by the core (EvSfx / EvMusic). Every field has a default that matches the
// renderer's naming, so a missing manifest still plays the SFX listed by the core's cue table (AudioTables::assets) and
// music by the SW_MUS_<PascalCaseTrackKey> convention.
#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"

#include <string_view>

namespace abyss
{
	struct AudioTables;
}

/** One rendered sound (SoundWave asset). */
struct FAbyssAudioAssetDef
{
	FName Name;
	/** /Game/Abyssfire/Audio/<Folder>/<Name>.<Name> */
	FSoftObjectPath Path;
	/** Linear runtime trim on top of the baked level (1 = as rendered). */
	float Gain = 1.f;
	bool bLoop = false;
	float LengthSec = 0.f;
	int32 Channels = 1;
};

/** Event-routed SFX cue (mirrors audio_cues.json, audio.md 9.6 concurrency). */
struct FAbyssAudioCueDef
{
	FName Id;
	/** Asset names; with families, `Variants` consecutive names per family in `Families` order. */
	TArray<FName> Assets;
	int32 Variants = 1;
	TArray<FName> Families;
	bool bUiBus = false;
	bool bSpatial = false;
	int32 ConcurrencyMax = 2;
	float MinRetriggerSec = 0.f;
	float Gain = 1.f;
	/** monster_death: per-family vocal layer (A7) played with the cue. */
	TMap<FName, TArray<FName>> VocalLayer;
};

struct FAbyssMusicTrackDef
{
	FName Key;
	FName Asset;
	bool bLoop = true;
	float LengthSec = 0.f;
	float Gain = 1.f;
};

struct FAbyssDuckDef
{
	float Db = 0.f;
	float InSec = 0.5f;
	float OutSec = 0.5f;
};

struct FAbyssStingerDefs
{
	TMap<FName, FName> ChapterCardByMood;
	FName ChapterCardFallback;
	FName BossIntro;
	FName Whisper;
	FName CutsceneIn;
	FAbyssDuckDef ChapterCardDuck{-8.f, 0.6f, 0.7f};
	FAbyssDuckDef BossIntroDuck{-10.f, 0.2f, 0.45f};
	FAbyssDuckDef WhisperDuck{-6.f, 0.6f, 0.6f};
	float WhisperFadeSec = 0.6f;
	float ChapterCardFadeOutSec = 0.7f;
	float BossIntroFadeOutSec = 0.45f;
};

struct FAbyssAmbienceDefs
{
	TMap<FName, FName> ByZone;
	TMap<FName, FName> ByTheme;
	float FadeInSec = 2.5f;
	float FadeOutSec = 1.5f;
	float Gain = 1.f;
	float CinematicDuckDb = -6.f;
};

struct FAbyssFootstepDefs
{
	/** surface (grass / dirt / stone) -> variants */
	TMap<FName, TArray<FName>> Surfaces;
	/** tile type name (abyss::TileType EnumName) -> surface */
	TMap<FName, FName> TileSurface;
	TArray<FName> Notifies;
	float FallbackStrideCm = 116.7f;
	float NotifyTimeoutSec = 0.9f;
	float MinIntervalSec = 0.18f;
	float Gain = 1.f;
};

struct FAbyssSpatialDefs
{
	/** A4: fraction of a full pan reached by a source at the edge of the play area. */
	float Spread = 0.25f;
	/** Lateral distance from the hero (cm) treated as the play-area edge (W1: ~16 tiles across). */
	float HalfWidthCm = 800.f;
	/** Distance of the virtual emitter from the listener (no distance attenuation is applied). */
	float VirtualDistanceCm = 100.f;
};

class ABYSSFIRE_API FAbyssAudioManifest
{
public:
	/** Loads the first manifest found (Audio/Export/audio_manifest.json, Data/audio_manifest.json); false = defaults. */
	bool Load(const abyss::AudioTables* CoreCues, FString& OutSource);
	/** Parses manifest JSON text on top of the defaults. */
	bool LoadFromJson(std::string_view Json, const abyss::AudioTables* CoreCues, FString& OutError);
	/** Defaults from the core cue table and the renderer's naming conventions. */
	void MakeDefaults(const abyss::AudioTables* CoreCues);

	const FAbyssAudioAssetDef* FindAsset(FName Name) const;
	/** The asset, or a conventional default (/Game/Abyssfire/Audio/<FallbackFolder>/<Name>). */
	FAbyssAudioAssetDef ResolveAsset(FName Name, const TCHAR* FallbackFolder) const;
	const FAbyssAudioCueDef* FindCue(FName CueId) const;
	const FAbyssMusicTrackDef* FindTrack(FName TrackKey) const;
	/** The track, or the SW_MUS_<PascalCase(key)> convention. */
	FAbyssMusicTrackDef ResolveTrack(FName TrackKey) const;

	FName MenuTrack = TEXT("menu_explore");
	FAbyssStingerDefs Stingers;
	FAbyssAmbienceDefs Ambience;
	FAbyssFootstepDefs Footsteps;
	FAbyssSpatialDefs Spatial;
	float DefaultMusicVolume = 0.6f;
	float DefaultSfxVolume = 0.8f;
	bool IsFromFile() const { return bFromFile; }
	const TMap<FName, FAbyssAudioAssetDef>& GetAssets() const { return Assets; }
	const TMap<FName, FAbyssAudioCueDef>& GetCues() const { return Cues; }
	const TMap<FName, FAbyssMusicTrackDef>& GetTracks() const { return Tracks; }

	/** "emerald_plains_explore" -> "SW_MUS_EmeraldPlainsExplore". */
	static FName ConventionalMusicAsset(FName TrackKey);
	static FSoftObjectPath MakeAssetPath(const FString& Folder, FName Name);

private:
	TMap<FName, FAbyssAudioAssetDef> Assets;
	TMap<FName, FAbyssAudioCueDef> Cues;
	TMap<FName, FAbyssMusicTrackDef> Tracks;
	bool bFromFile = false;
};

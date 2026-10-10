#include "Audio/AbyssAudioManifest.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include <string>

#include "abyss/base/Enums.h"
#include "abyss/base/Json.h"
#include "abyss/data/AudioData.h"

#include "Abyssfire.h"
#include "Framework/AbyssText.h"

namespace AbyssAudioManifestPrivate
{
	const TCHAR* const RootPath = TEXT("/Game/Abyssfire/Audio");

	FName JsonName(const abyss::JsonValue& Value)
	{
		const std::string_view Text = Value.AsString();
		return Text.empty() ? NAME_None : FName(AbyssText::ToFString(Text));
	}

	float JsonFloat(const abyss::JsonValue& Value, float Default)
	{
		return Value.IsNumber() ? static_cast<float>(Value.AsDouble(Default)) : Default;
	}

	TArray<FName> JsonNames(const abyss::JsonValue& Array)
	{
		TArray<FName> Out;
		for (const abyss::JsonValue& Item : Array.Items())
		{
			const FName Name = JsonName(Item);
			if (!Name.IsNone())
			{
				Out.Add(Name);
			}
		}
		return Out;
	}

	void ReadDuck(const abyss::JsonValue& Json, FAbyssDuckDef& Out)
	{
		if (!Json.IsObject())
		{
			return;
		}
		Out.Db = JsonFloat(Json.Get("db"), Out.Db);
		Out.InSec = FMath::Max(0.f, JsonFloat(Json.Get("inSec"), Out.InSec));
		Out.OutSec = FMath::Max(0.f, JsonFloat(Json.Get("outSec"), Out.OutSec));
	}

	FString PascalCase(const FString& Snake)
	{
		TArray<FString> Words;
		Snake.ParseIntoArray(Words, TEXT("_"), /*InCullEmpty*/ true);
		FString Out;
		for (const FString& Word : Words)
		{
			Out += Word.Left(1).ToUpper() + Word.Mid(1);
		}
		return Out;
	}
}

FSoftObjectPath FAbyssAudioManifest::MakeAssetPath(const FString& Folder, FName Name)
{
	const FString AssetName = Name.ToString();
	return FSoftObjectPath(FString::Printf(TEXT("%s/%s/%s.%s"), AbyssAudioManifestPrivate::RootPath, *Folder, *AssetName,
		*AssetName));
}

FName FAbyssAudioManifest::ConventionalMusicAsset(FName TrackKey)
{
	return FName(TEXT("SW_MUS_") + AbyssAudioManifestPrivate::PascalCase(TrackKey.ToString()));
}

void FAbyssAudioManifest::MakeDefaults(const abyss::AudioTables* CoreCues)
{
	*this = FAbyssAudioManifest();

	if (CoreCues != nullptr)
	{
		for (const abyss::AudioCueDef& Def : CoreCues->cues)
		{
			FAbyssAudioCueDef Cue;
			Cue.Id = FName(AbyssText::ToFString(abyss::EnumName(Def.id)));
			for (const std::string& Asset : Def.assets)
			{
				Cue.Assets.Add(FName(AbyssText::ToFString(Asset)));
			}
			Cue.Variants = FMath::Max(1, Def.variants);
			for (const abyss::AnimRig Family : Def.families)
			{
				Cue.Families.Add(FName(AbyssText::ToFString(abyss::EnumName(Family))));
			}
			Cue.bUiBus = Def.bus == abyss::AudioBus::Ui;
			Cue.bSpatial = Def.spatial3d;
			Cue.ConcurrencyMax = FMath::Max(1, Def.concurrencyMax);
			Cue.MinRetriggerSec = static_cast<float>(Def.minRetriggerMs / 1000.0);
			Cues.Add(Cue.Id, MoveTemp(Cue));
		}
	}
	if (FAbyssAudioCueDef* Death = Cues.Find(FName(TEXT("monster_death"))))
	{
		for (const TCHAR* Family : { TEXT("Humanoid"), TEXT("Slime") })
		{
			TArray<FName> Layer;
			for (int32 V = 1; V <= 2; ++V)
			{
				Layer.Add(FName(FString::Printf(TEXT("SW_SFX_MonsterDeathVocal_%s_%02d"), Family, V)));
			}
			Death->VocalLayer.Add(FName(*FString(Family).ToLower()), MoveTemp(Layer));
		}
	}

	Stingers.ChapterCardByMood.Add(FName(TEXT("dawn")), FName(TEXT("SW_STG_ChapterCardDawn")));
	Stingers.ChapterCardFallback = FName(TEXT("SW_STG_ChapterCardDawn"));
	Stingers.BossIntro = FName(TEXT("SW_STG_BossIntro"));
	Stingers.Whisper = FName(TEXT("SW_STG_WhisperLoop"));
	Stingers.CutsceneIn = FName(TEXT("SW_STG_CutsceneIn"));

	Ambience.ByTheme.Add(FName(TEXT("plains")), FName(TEXT("SW_AMB_Plains")));
	Ambience.ByZone.Add(FName(TEXT("emerald_plains")), FName(TEXT("SW_AMB_Plains")));

	for (const TCHAR* Surface : { TEXT("grass"), TEXT("dirt"), TEXT("stone") })
	{
		TArray<FName>& Variants = Footsteps.Surfaces.Add(FName(Surface));
		const FString Pascal = AbyssAudioManifestPrivate::PascalCase(FString(Surface));
		for (int32 V = 1; V <= 4; ++V)
		{
			Variants.Add(FName(FString::Printf(TEXT("SW_SFX_Footstep_%s_%02d"), *Pascal, V)));
		}
	}
	const TCHAR* const TileSurfaces[][2] = {
		{ TEXT("grass"), TEXT("grass") }, { TEXT("dirt"), TEXT("dirt") }, { TEXT("stone"), TEXT("stone") },
		{ TEXT("camp"), TEXT("dirt") }, { TEXT("water"), TEXT("dirt") }, { TEXT("wall"), TEXT("stone") },
		{ TEXT("campWall"), TEXT("dirt") },
	};
	for (const auto& Pair : TileSurfaces)
	{
		Footsteps.TileSurface.Add(FName(Pair[0]), FName(Pair[1]));
	}
	Footsteps.Notifies.Add(FName(TEXT("FootL")));
	Footsteps.Notifies.Add(FName(TEXT("FootR")));

	// audio.md 11 zoneOverrides (QUIRK Q8): the Ember Tower plays the procedural plains score.
	TMap<FName, FName>& Tower = ZoneOverrides.Add(FName(TEXT("ember_tower")));
	Tower.Add(FName(TEXT("emerald_plains_explore")), FName(TEXT("emerald_plains_explore_score")));
	Tower.Add(FName(TEXT("emerald_plains_combat")), FName(TEXT("emerald_plains_combat_score")));

	// audio.md 8.3: the Chapter 1 soundtrack (tracks that are packaged and have a menu.jukebox.track.* title).
	const TCHAR* const JukeboxRows[][2] = {
		{ TEXT("menu.jukebox.track.menu"), TEXT("menu_explore") },
		{ TEXT("menu.jukebox.track.emerald_plains.explore"), TEXT("emerald_plains_explore") },
		{ TEXT("menu.jukebox.track.emerald_plains.combat"), TEXT("emerald_plains_combat") },
		{ TEXT("menu.jukebox.track.abyss_rift.explore"), TEXT("abyss_rift_explore") },
	};
	for (const auto& Row : JukeboxRows)
	{
		Jukebox.Tracks.Add(FAbyssJukeboxEntry{ FString(Row[0]), FName(Row[1]) });
	}
}

bool FAbyssAudioManifest::Load(const abyss::AudioTables* CoreCues, FString& OutSource)
{
	MakeDefaults(CoreCues);
	// Audio/Export is the renderer's output (editor / development); Data/ is the staged copy when the build stages it.
	const FString Candidates[] = {
		FPaths::ProjectDir() / TEXT("Data/audio_manifest.json"),
		FPaths::ProjectDir() / TEXT("Audio/Export/audio_manifest.json"),
	};
	for (const FString& Path : Candidates)
	{
		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *Path, FILEREAD_Silent))
		{
			continue;
		}
		FString Error;
		const std::string_view Text(reinterpret_cast<const char*>(Bytes.GetData()), static_cast<size_t>(Bytes.Num()));
		if (LoadFromJson(Text, CoreCues, Error))
		{
			OutSource = Path;
			return true;
		}
		UE_LOG(LogAbyss, Warning, TEXT("Audio manifest %s rejected: %s"), *Path, *Error);
		MakeDefaults(CoreCues);
	}
	OutSource = TEXT("(defaults: no audio_manifest.json found)");
	return false;
}

bool FAbyssAudioManifest::LoadFromJson(std::string_view Json, const abyss::AudioTables* CoreCues, FString& OutError)
{
	using namespace AbyssAudioManifestPrivate;

	abyss::JsonValue Root;
	abyss::JsonParseError ParseError;
	if (!abyss::ParseJson(Json, Root, &ParseError))
	{
		OutError = FString::Printf(TEXT("JSON error at byte %llu: %s"), static_cast<unsigned long long>(ParseError.offset),
			*AbyssText::ToFString(ParseError.message));
		return false;
	}
	if (!Root.IsObject() || Root.Get("schemaVersion").AsInt(0) != 1)
	{
		OutError = TEXT("not an audio manifest (schemaVersion 1)");
		return false;
	}

	// ---- assets ----
	for (const abyss::JsonMember& Member : Root.Get("assets").Members())
	{
		const abyss::JsonValue& A = Member.value;
		FAbyssAudioAssetDef Def;
		Def.Name = FName(AbyssText::ToFString(Member.key));
		const std::string_view UePath = A.Get("ue").AsString();
		Def.Path = UePath.empty() ? MakeAssetPath(TEXT("SFX"), Def.Name) : FSoftObjectPath(AbyssText::ToFString(UePath));
		Def.Gain = FMath::Max(0.f, JsonFloat(A.Get("gain"), 1.f));
		Def.bLoop = A.Get("loop").AsBool(false);
		Def.LengthSec = JsonFloat(A.Get("lengthSec"), 0.f);
		Def.Channels = A.Get("channels").AsInt(1);
		Assets.Add(Def.Name, MoveTemp(Def));
	}

	// ---- cues (override / extend the core table's asset lists and rules) ----
	for (const abyss::JsonMember& Member : Root.Get("cues").Members())
	{
		const abyss::JsonValue& C = Member.value;
		const FName Id(AbyssText::ToFString(Member.key));
		FAbyssAudioCueDef& Cue = Cues.FindOrAdd(Id);
		Cue.Id = Id;
		if (const TArray<FName> Names = JsonNames(C.Get("assets")); Names.Num() > 0)
		{
			Cue.Assets = Names;
		}
		Cue.Variants = FMath::Max(1, C.Get("variants").AsInt(Cue.Variants));
		if (C.Has("families"))
		{
			Cue.Families = JsonNames(C.Get("families"));
		}
		if (const std::string_view Bus = C.Get("bus").AsString(); !Bus.empty())
		{
			Cue.bUiBus = Bus == "ui";
		}
		if (const std::string_view Spatial = C.Get("spatial").AsString(); !Spatial.empty())
		{
			Cue.bSpatial = Spatial == "3d";
		}
		const abyss::JsonValue& Conc = C.Get("concurrency");
		Cue.ConcurrencyMax = FMath::Max(1, Conc.Get("max").AsInt(Cue.ConcurrencyMax));
		Cue.MinRetriggerSec = FMath::Max(0.f, JsonFloat(Conc.Get("minRetriggerMs"), Cue.MinRetriggerSec * 1000.f) / 1000.f);
		Cue.Gain = FMath::Max(0.f, JsonFloat(C.Get("gain"), Cue.Gain));
		const abyss::JsonValue& Vocal = C.Get("vocalLayer");
		if (Vocal.IsObject())
		{
			Cue.VocalLayer.Reset();
			for (const abyss::JsonMember& Family : Vocal.Get("assets").Members())
			{
				Cue.VocalLayer.Add(FName(AbyssText::ToFString(Family.key)), JsonNames(Family.value));
			}
		}
	}

	// ---- music ----
	for (const abyss::JsonMember& Member : Root.Get("music").Members())
	{
		const abyss::JsonValue& M = Member.value;
		FAbyssMusicTrackDef Track;
		Track.Key = FName(AbyssText::ToFString(Member.key));
		Track.Asset = JsonName(M.Get("asset"));
		if (Track.Asset.IsNone())
		{
			Track.Asset = ConventionalMusicAsset(Track.Key);
		}
		Track.bLoop = M.Get("loop").AsBool(true);
		Track.LengthSec = JsonFloat(M.Get("lengthSec"), 0.f);
		Track.Gain = FMath::Max(0.f, JsonFloat(M.Get("gain"), 1.f));
		const abyss::JsonValue& Credit = M.Get("credit");
		if (Credit.IsObject())
		{
			FAbyssMusicCredit Out;
			Out.Title = AbyssText::ToFString(Credit.Get("title").AsString());
			Out.Author = AbyssText::ToFString(Credit.Get("author").AsString());
			Out.License = AbyssText::ToFString(Credit.Get("license").AsString());
			Out.Url = AbyssText::ToFString(Credit.Get("url").AsString());
			Track.Credit = MoveTemp(Out);
		}
		Tracks.Add(Track.Key, Track);
	}
	if (const FName Menu = JsonName(Root.Get("menuTrack")); !Menu.IsNone())
	{
		MenuTrack = Menu;
	}
	if (Root.Get("zoneOverrides").IsObject())
	{
		ZoneOverrides.Reset();
		for (const abyss::JsonMember& Zone : Root.Get("zoneOverrides").Members())
		{
			TMap<FName, FName>& Table = ZoneOverrides.Add(FName(AbyssText::ToFString(Zone.key)));
			for (const abyss::JsonMember& Row : Zone.value.Members())
			{
				if (const FName To = JsonName(Row.value); !To.IsNone())
				{
					Table.Add(FName(AbyssText::ToFString(Row.key)), To);
				}
			}
		}
	}

	// ---- stingers ----
	const abyss::JsonValue& S = Root.Get("stingers");
	if (S.IsObject())
	{
		Stingers.ChapterCardByMood.Reset();
		for (const abyss::JsonMember& Mood : S.Get("chapterCard").Members())
		{
			Stingers.ChapterCardByMood.Add(FName(AbyssText::ToFString(Mood.key)), JsonName(Mood.value));
		}
		Stingers.ChapterCardFallback = JsonName(S.Get("chapterCardFallback"));
		Stingers.BossIntro = JsonName(S.Get("bossIntro"));
		Stingers.Whisper = JsonName(S.Get("whisper"));
		Stingers.CutsceneIn = JsonName(S.Get("cutsceneIn"));
		const abyss::JsonValue& Duck = S.Get("duck");
		ReadDuck(Duck.Get("chapterCard"), Stingers.ChapterCardDuck);
		ReadDuck(Duck.Get("bossIntro"), Stingers.BossIntroDuck);
		ReadDuck(Duck.Get("whisper"), Stingers.WhisperDuck);
		Stingers.WhisperFadeSec = JsonFloat(S.Get("whisperFadeSec"), Stingers.WhisperFadeSec);
		Stingers.ChapterCardFadeOutSec = JsonFloat(S.Get("chapterCardFadeOutSec"), Stingers.ChapterCardFadeOutSec);
		Stingers.BossIntroFadeOutSec = JsonFloat(S.Get("bossIntroFadeOutSec"), Stingers.BossIntroFadeOutSec);
	}

	// ---- ambience ----
	const abyss::JsonValue& Amb = Root.Get("ambience");
	if (Amb.IsObject())
	{
		Ambience.ByTheme.Reset();
		Ambience.ByZone.Reset();
		for (const abyss::JsonMember& Theme : Amb.Get("themes").Members())
		{
			Ambience.ByTheme.Add(FName(AbyssText::ToFString(Theme.key)), JsonName(Theme.value));
		}
		for (const abyss::JsonMember& Zone : Amb.Get("zones").Members())
		{
			Ambience.ByZone.Add(FName(AbyssText::ToFString(Zone.key)), JsonName(Zone.value));
		}
		Ambience.FadeInSec = JsonFloat(Amb.Get("fadeInSec"), Ambience.FadeInSec);
		Ambience.FadeOutSec = JsonFloat(Amb.Get("fadeOutSec"), Ambience.FadeOutSec);
		Ambience.Gain = FMath::Max(0.f, JsonFloat(Amb.Get("gain"), Ambience.Gain));
		Ambience.CinematicDuckDb = JsonFloat(Amb.Get("duckInCinematicDb"), Ambience.CinematicDuckDb);
	}

	// ---- footsteps ----
	const abyss::JsonValue& Fs = Root.Get("footsteps");
	if (Fs.IsObject())
	{
		Footsteps.Surfaces.Reset();
		for (const abyss::JsonMember& Surface : Fs.Get("surfaces").Members())
		{
			Footsteps.Surfaces.Add(FName(AbyssText::ToFString(Surface.key)), JsonNames(Surface.value));
		}
		if (Fs.Get("tileSurface").IsObject())
		{
			Footsteps.TileSurface.Reset();
			for (const abyss::JsonMember& Tile : Fs.Get("tileSurface").Members())
			{
				Footsteps.TileSurface.Add(FName(AbyssText::ToFString(Tile.key)), JsonName(Tile.value));
			}
		}
		if (Fs.Has("notifies"))
		{
			Footsteps.Notifies = JsonNames(Fs.Get("notifies"));
		}
		Footsteps.FallbackStrideCm = FMath::Max(10.f, JsonFloat(Fs.Get("fallbackStrideCm"), Footsteps.FallbackStrideCm));
		Footsteps.NotifyTimeoutSec = FMath::Max(0.1f, JsonFloat(Fs.Get("notifyTimeoutSec"), Footsteps.NotifyTimeoutSec));
		Footsteps.MinIntervalSec = FMath::Max(0.f, JsonFloat(Fs.Get("minIntervalSec"), Footsteps.MinIntervalSec));
		Footsteps.Gain = FMath::Max(0.f, JsonFloat(Fs.Get("gain"), Footsteps.Gain));
	}

	// ---- jukebox (audio.md 8.3) ----
	const abyss::JsonValue& Jb = Root.Get("jukebox");
	if (Jb.IsObject())
	{
		if (Jb.Has("tracks"))
		{
			Jukebox.Tracks.Reset();
			for (const abyss::JsonValue& Row : Jb.Get("tracks").Items())
			{
				FAbyssJukeboxEntry Entry;
				Entry.TitleKey = AbyssText::ToFString(Row.Get("titleKey").AsString());
				Entry.Track = JsonName(Row.Get("track"));
				if (!Entry.Track.IsNone())
				{
					Jukebox.Tracks.Add(MoveTemp(Entry));
				}
			}
		}
		Jukebox.FadeOutSec = FMath::Max(0.f, JsonFloat(Jb.Get("fadeOutSec"), Jukebox.FadeOutSec));
		Jukebox.FadeInSec = FMath::Max(0.f, JsonFloat(Jb.Get("fadeInSec"), Jukebox.FadeInSec));
	}

	// ---- spatial (A4) ----
	const abyss::JsonValue& Sp = Root.Get("spatial");
	if (Sp.IsObject())
	{
		Spatial.Spread = FMath::Clamp(JsonFloat(Sp.Get("spread"), Spatial.Spread), 0.f, 1.f);
		Spatial.HalfWidthCm = FMath::Max(1.f, JsonFloat(Sp.Get("halfWidthCm"), Spatial.HalfWidthCm));
		Spatial.VirtualDistanceCm = FMath::Max(1.f, JsonFloat(Sp.Get("virtualDistanceCm"), Spatial.VirtualDistanceCm));
	}

	const abyss::JsonValue& Defaults = Root.Get("defaults");
	DefaultMusicVolume = JsonFloat(Defaults.Get("musicVolume"), DefaultMusicVolume);
	DefaultSfxVolume = JsonFloat(Defaults.Get("sfxVolume"), DefaultSfxVolume);

	(void)CoreCues;
	bFromFile = true;
	return true;
}

const FAbyssAudioAssetDef* FAbyssAudioManifest::FindAsset(FName Name) const
{
	return Assets.Find(Name);
}

FAbyssAudioAssetDef FAbyssAudioManifest::ResolveAsset(FName Name, const TCHAR* FallbackFolder) const
{
	if (const FAbyssAudioAssetDef* Found = Assets.Find(Name))
	{
		return *Found;
	}
	FAbyssAudioAssetDef Def;
	Def.Name = Name;
	Def.Path = MakeAssetPath(FallbackFolder, Name);
	return Def;
}

const FAbyssAudioCueDef* FAbyssAudioManifest::FindCue(FName CueId) const
{
	return Cues.Find(CueId);
}

const FAbyssMusicTrackDef* FAbyssAudioManifest::FindTrack(FName TrackKey) const
{
	return Tracks.Find(TrackKey);
}

FName FAbyssAudioManifest::ApplyZoneOverride(FName ZoneId, FName TrackKey) const
{
	if (const TMap<FName, FName>* Table = ZoneOverrides.Find(ZoneId))
	{
		if (const FName* To = Table->Find(TrackKey))
		{
			return *To;
		}
	}
	return TrackKey;
}

FAbyssMusicTrackDef FAbyssAudioManifest::ResolveTrack(FName TrackKey) const
{
	if (const FAbyssMusicTrackDef* Found = Tracks.Find(TrackKey))
	{
		return *Found;
	}
	FAbyssMusicTrackDef Track;
	Track.Key = TrackKey;
	Track.Asset = ConventionalMusicAsset(TrackKey);
	return Track;
}

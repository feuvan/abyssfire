// The art manifest as the presentation layer sees it (ARCHITECTURE 5, Art/blender/README.md 6): every generated asset
// (skeletal / static meshes, clips with their contact / release / notify times, sockets, weapon attachments, blob-shadow
// radius, FX colours) plus the shading contract (key light, rim, camera). Read once at boot from Data/assets.json (the
// exporter's {"manifest": ...} copy of Art/Export/manifest.json, staged UFS) or, in editor builds, straight from
// Art/Export/manifest.json. Parsed with the core's JSON reader (UE reads bytes, the core parses).
//
// Asset path mapping (contract with the content build, Scripts/build_content.py):
//   an asset `<Name>` exported as `Art/Export/<Dir>/<Name>.fbx` is imported to  /Game/Abyssfire/<Dir>/<Name>
//   (object path /Game/Abyssfire/<Dir>/<Name>.<Name>); <Dir> = the directory part of the manifest `fbx` field,
//   else the asset `category`. Clips (`A_*`) use their own `fbx` directory. Textures: /Game/Abyssfire/Textures/<Name>.
//
// Game ids (web texture keys, e.g. player_warrior, monster_goblin, decor_tree, camp_tent, exit_portal, loot_bag) map to
// one or more assets: the top-level `gameIds` object plus each asset's `gameIds` array. Several assets listing the same
// id are VARIANTS, picked by a deterministic hash in asset-name order (e.g. SM_Foliage_Plains_Oak_A/B/C <- decor_tree).
//
// Theme variants (dark remake U1, AbyssThemeRules.h): an id `<gameId>@<theme>` (tree@march, npc_quest_elder@march,
// outcrop_plains@march) is the art a zone with that presentation theme shows in place of `<gameId>`. ResolveFirst with a
// theme tries every id's variant before any plain id; a missing variant falls back to today's asset. The load warns when
// a variant's footprint / blocking differs from its plain id's (the core bakes walkability from the plain ids).
#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"

#include <string_view>

#include "abyss/base/Json.h"

struct FAbyssArtNotify
{
	FName Name;
	float TimeSec = 0.f;
};

struct FAbyssArtClip
{
	FName Name;                 // Idle, Walk, Run, Attack01.., Cast01.., Cast_<Skill>, Hurt, HurtAdd, Dodge, Death, ...
	FName Asset;                // A_<Family>_<Name>_<Action>
	FString Folder;             // content folder under /Game/Abyssfire
	float LengthSec = 0.f;
	bool bLoop = false;
	bool bAdditive = false;
	bool bSignature = false;
	bool bStaticPose = false;
	float ContactSec = -1.f;    // attack contact at play rate 1 (< 0 = none)
	float ReleaseSec = -1.f;    // cast release at play rate 1 (< 0 = none)
	float RefSpeedCmS = 0.f;    // locomotion clips: ground speed the stride was authored for
	float Fps = 0.f;            // authored frame rate (frame -> seconds for frozen poses); 0 = unknown (60)
	TArray<FAbyssArtNotify> Notifies;
	TArray<FName> Skills;       // skill ids this clip presents (signature `skill` + `skills` list)

	FSoftObjectPath GetObjectPath() const;
	/** Contact (attacks) or release (casts) beat, < 0 when the clip has none. */
	float GetBeatSec() const { return ContactSec >= 0.f ? ContactSec : ReleaseSec; }
};

struct FAbyssArtSocket
{
	FName Name;
	FName Bone;        // skeleton sockets; NAME_None for static-mesh sockets
	FVector LocCm = FVector::ZeroVector;
};

struct FAbyssArtAttachment
{
	FName Object;      // static mesh asset name (SM_Hero_Warrior_Sword)
	FName Socket;      // weapon_r / weapon_l
};

/**
 * Runtime FX attachments of a hero (manifest assets.<hero>.fx, Art/blender/README.md hero section; art-inventory-ch1.md
 * 3.1 "FX attachments"). The reviewed game-camera previews include them, so the shipped hero draws them too
 * (UAbyssVfxSystem hero attachments): the visor glow card, weapon trails, the cast blade glow, the hero halo light pool
 * and the emissive bloom strength.
 */
struct FAbyssArtGlowCard
{
	bool bValid = false;
	FName Socket;                                  // skeletal socket on the body (visor)
	FLinearColor Color = FLinearColor::White;
	float RadiusCm = 10.f;
	float Alpha = 0.75f;
	float AlphaPerFx = 0.f;                        // + alphaPerFx x fx (1 while attacking / casting)
	FLinearColor CoreColor = FLinearColor::White;  // hot core card (radius 0 = none)
	float CoreRadiusCm = 0.f;
	float CoreAlpha = 0.f;
	float OffAtDeathFraction = 1.f;                // the card turns off at this fraction of the death clip
};

struct FAbyssArtTrail
{
	bool bValid = false;
	FLinearColor Color = FLinearColor::White;
	float Alpha = 0.55f;
	FName TipSocket = FName(TEXT("tip"));          // static-mesh sockets on the main-hand weapon
	FName MidSocket = FName(TEXT("mid"));
	int32 Samples = 7;                             // trail length = Samples x SampleMs of history
	float SampleMs = 13.f;
};

struct FAbyssArtBladeGlow
{
	bool bValid = false;
	FLinearColor Color = FLinearColor::White;
	float RadiusCm = 33.f;
	float Alpha = 0.55f;
	FLinearColor TipColor = FLinearColor::White;
	float TipRadiusCm = 18.f;
	float TipAlpha = 0.8f;
	int32 EmberCount = 0;
	FLinearColor EmberColor = FLinearColor::White;
};

struct FAbyssArtFx
{
	FAbyssArtGlowCard VisorGlow;
	FAbyssArtTrail AttackTrail;
	FAbyssArtTrail CastTrail;
	FAbyssArtBladeGlow CastBladeGlow;
	bool bHasHeroHalo = false;
	FLinearColor HeroHaloColor = FLinearColor::White;
	float HeroHaloRadiusCm = 200.f;
	bool bHasBloom = false;
	float BloomStrength = 0.f;

	bool HasAttachments() const
	{
		return VisorGlow.bValid || AttackTrail.bValid || CastTrail.bValid || CastBladeGlow.bValid || bHasHeroHalo;
	}
};

struct FAbyssArtAsset
{
	FName Name;
	FName Kind;        // SkeletalMesh | StaticMesh
	FName Category;    // Characters | Monsters | NPCs | Weapons | Props | Terrain | Foliage | VFX | Pickups
	FString Folder;
	FName Skeleton;
	float Scale = 1.f;
	float HeightCm = 0.f;
	float BlobShadowRadiusCm = 0.f;
	FBox BoundsCm = FBox(ForceInit);
	TArray<FName> GameIds;
	TArray<FAbyssArtClip> Clips;
	TArray<FAbyssArtAttachment> Attachments;
	TArray<FAbyssArtSocket> Sockets;
	FName AttachSocket;            // weapons: the socket they go on
	FName WeaponType;              // weapons: sword / axe / mace / dagger / bow / staff / wand / shield
	TArray<FName> HeldBy;          // weapons: the skeletal meshes they were authored for
	FName OutlineClass;            // hero / monster / npc / interactive / decor / ...
	/** UI portrait texture (manifest `portrait`, e.g. T_UI_Portrait_QuestElder_March); NAME_None when the asset has none. */
	FName Portrait;
	bool bHasFootprint = false;
	int32 FootprintW = 1;
	int32 FootprintH = 1;
	bool bBlocking = false;
	bool bHasImpactColor = false;
	FLinearColor ImpactColor = FLinearColor::White;
	bool bHasClassColor = false;
	FLinearColor ClassColor = FLinearColor::White;
	bool bHasSpiritColor = false;
	FLinearColor SpiritColor = FLinearColor::White;
	/** fx attachments (heroes; empty for everything else). */
	FAbyssArtFx Fx;

	bool IsSkeletal() const;
	FSoftObjectPath GetObjectPath() const;
	const FAbyssArtClip* FindClip(FName ClipName) const;
	/** Signature / skill-listed clip for a skill id, else nullptr. */
	const FAbyssArtClip* FindClipForSkill(FName SkillId) const;
	const FAbyssArtSocket* FindSocket(FName SocketName) const;
	/** Height of the bounds (cm), falling back to HeightCm. */
	float GetVisualHeightCm() const;
};

/** manifest.json `shading` (Art/blender/README.md 2): what the UE materials, light and camera must reproduce. */
struct FAbyssArtShading
{
	bool bValid = false;
	/** Unit vector towards the key light, UE axes (shading.keyLight.dirToLightUE). */
	FVector DirToLight = FVector(0.40558, -0.40558, 0.819152);
	FLinearColor RimColor = FLinearColor(1.f, 0.925f, 0.784f);
	float RimAlpha = 0.55f;
	FLinearColor CoolShade = FLinearColor(30.f / 255.f, 20.f / 255.f, 60.f / 255.f);   // sRGB values (tone constants)
	float CoolShadeMix = 0.35f;
	FLinearColor WarmLight = FLinearColor(1.f, 244.f / 255.f, 214.f / 255.f);
	float CameraDistanceCm = 2537.3f;
	float CameraFocusZCm = 50.f;
	float CameraFovHDeg = 35.f;
	float CameraPitchDeg = -50.f;
	float CameraYawDeg = 45.f;
	/** units.characterForward: meshes face +Y; the actor rotates the mesh by this yaw. */
	float MeshYawOffsetDeg = -90.f;
};

class ABYSSFIRE_API FAbyssArtManifest
{
public:
	/** Parses manifest JSON (either the art manifest itself or the exporter envelope {"manifest": {...}}). */
	bool LoadFromJson(std::string_view Json, FString& OutError);
	/** Data/assets.json, else (editor) Art/Export/manifest.json. False when neither exists or parses. */
	bool LoadFromProject(FString& OutError, FString& OutSourcePath);
	void Reset();

	bool IsLoaded() const { return bLoaded; }
	int32 Num() const { return Assets.Num(); }
	const TArray<FAbyssArtAsset>& GetAssets() const { return Assets; }
	const FAbyssArtShading& GetShading() const { return Shading; }

	const FAbyssArtAsset* FindAsset(FName AssetName) const;
	/** Every asset listing the game id (variants), in asset-name order. */
	void FindByGameId(FName GameId, TArray<const FAbyssArtAsset*>& Out) const;
	bool HasGameId(FName GameId) const;
	/** One asset for a game id: variant = Hash % count. */
	const FAbyssArtAsset* ResolveGameId(FName GameId, uint32 VariantHash = 0) const;
	/** First game id of the list that resolves (fallback chains such as {decor_tree, tree}). */
	const FAbyssArtAsset* ResolveFirst(TConstArrayView<FName> GameIds, uint32 VariantHash = 0) const;
	/**
	 * Theme-aware ResolveFirst (U1): with a presentation theme (march), every id's `<id>@<theme>` variant first, in chain
	 * order, then the plain chain; NAME_None = the plain ResolveFirst. A missing variant is not an error: the plain asset
	 * is the intended fallback.
	 */
	const FAbyssArtAsset* ResolveFirst(TConstArrayView<FName> GameIds, uint32 VariantHash, FName Theme) const;
	/** `<GameId>@<Theme>` when the manifest has that variant, else NAME_None. */
	FName FindThemedGameId(FName GameId, FName Theme) const;
	/** Whether any asset is authored for the theme (`<id>@<Theme>` ids). */
	bool HasTheme(FName Theme) const { return ThemedIds.Contains(Theme); }
	/** Weapon mesh of a weapon type, preferring one authored for `HeldBy` (I6 / R4). */
	const FAbyssArtAsset* FindWeapon(FName WeaponType, FName HeldBy) const;

	/**
	 * The manifest's `materials` object (material contracts the runtime reads: MI_AF_Echo's figures / timeline / tint,
	 * M_AF_Telegraph's parameters); a null value without one.
	 */
	const abyss::JsonValue& GetMaterialsJson() const { return MaterialsJson; }
	/** Theme variants whose footprint or blocking differs from the plain id (logged at load). */
	const TArray<FName>& GetThemeParityMismatches() const { return ThemeParityMismatches; }
	/**
	 * Dark remake 2G: the raw JSON of an asset's blocks the typed parse does not cover, for the pure presentation rules
	 * (AbyssAnimActionRules::ParseDeathRuntime; the corpse / monster-look rules): an object with the asset's `fx` (fuse,
	 * frenzy, block, ...) and `corpse` blocks and `clips.<clip name>` holding each clip's `runtime`, `puddleDecal` and
	 * `corpse` blocks (only the keys present). A null value for an asset without any of them or an unknown name.
	 */
	const abyss::JsonValue& GetAssetJson(FName AssetName) const;

	static const TCHAR* ContentRoot() { return TEXT("/Game/Abyssfire"); }
	/** /Game/Abyssfire/<Folder>/<Asset>.<Asset> */
	static FSoftObjectPath MakeObjectPath(const FString& Folder, FName AssetName);

private:
	TArray<FAbyssArtAsset> Assets;
	TMap<FName, int32> AssetsByName;
	TMap<FName, TArray<int32>> AssetsByGameId;
	/** Theme -> plain game id -> its `<id>@<theme>` variant id (built from AssetsByGameId's keys at load). */
	TMap<FName, TMap<FName, FName>> ThemedIds;
	TArray<FName> ThemeParityMismatches;
	abyss::JsonValue MaterialsJson;
	/** GetAssetJson's blocks by asset name (assets that carry any). */
	TMap<FName, abyss::JsonValue> AssetJsonByName;
	FAbyssArtShading Shading;
	bool bLoaded = false;
};

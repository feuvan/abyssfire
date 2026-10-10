#include "World/AbyssArtManifest.h"

#include "Abyssfire.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include <span>
#include <string>

#include "abyss/base/Json.h"

#include "Framework/AbyssText.h"
#include "World/AbyssWorldTypes.h"

namespace AbyssArtManifestPrivate
{
	FName JsonName(const abyss::JsonValue& Value)
	{
		const std::string_view Text = Value.AsString();
		return Text.empty() ? NAME_None : FName(*AbyssText::ToFString(Text));
	}

	FString JsonString(const abyss::JsonValue& Value)
	{
		return AbyssText::ToFString(Value.AsString());
	}

	float JsonFloat(const abyss::JsonValue& Value, float Default)
	{
		return Value.IsNumber() ? static_cast<float>(Value.AsDouble(Default)) : Default;
	}

	bool JsonVector(const abyss::JsonValue& Value, FVector& Out)
	{
		if (!Value.IsArray() || Value.Size() < 3)
		{
			return false;
		}
		Out = FVector(Value.At(0).AsDouble(), Value.At(1).AsDouble(), Value.At(2).AsDouble());
		return true;
	}

	bool JsonColor(const abyss::JsonValue& Value, FLinearColor& Out)
	{
		if (Value.IsString())
		{
			return AbyssWorldUtil::ParseHexColor(JsonString(Value), Out);
		}
		if (Value.IsNumber())
		{
			Out = AbyssWorldUtil::ColorFromRgb(static_cast<uint32>(Value.AsInt64()));
			return true;
		}
		return false;
	}

	/** [r, g, b] in 0..255 sRGB values, kept as sRGB 0..1 (the toon maths runs on sRGB values). */
	bool JsonSrgbTriple(const abyss::JsonValue& Value, FLinearColor& Out)
	{
		if (!Value.IsArray() || Value.Size() < 3)
		{
			return false;
		}
		Out = FLinearColor(static_cast<float>(Value.At(0).AsDouble() / 255.0), static_cast<float>(Value.At(1).AsDouble() / 255.0),
			static_cast<float>(Value.At(2).AsDouble() / 255.0), 1.f);
		return true;
	}

	/** Directory part of an export-relative path ("Characters/SK_X.fbx" -> "Characters"). */
	FString FolderOfFbx(const abyss::JsonValue& Fbx)
	{
		FString Path = JsonString(Fbx);
		Path.ReplaceInline(TEXT("\\"), TEXT("/"));
		int32 Slash = INDEX_NONE;
		if (Path.FindLastChar(TEXT('/'), Slash))
		{
			return Path.Left(Slash);
		}
		return FString();
	}

	void ParseClip(const abyss::JsonValue& Json, const FString& AssetFolder, FAbyssArtClip& Out)
	{
		Out.Name = JsonName(Json.Get("name"));
		Out.Asset = JsonName(Json.Get("asset"));
		Out.Folder = FolderOfFbx(Json.Get("fbx"));
		if (Out.Folder.IsEmpty())
		{
			Out.Folder = AssetFolder;
		}
		Out.LengthSec = JsonFloat(Json.Get("lengthMs"), 0.f) / 1000.f;
		Out.bLoop = Json.Get("loop").AsBool(false);
		Out.bAdditive = Json.Get("additive").AsBool(false);
		Out.bSignature = Json.Get("signature").AsBool(false);
		Out.bStaticPose = Json.Get("staticPose").AsBool(false);
		if (Json.Get("contactMs").IsNumber())
		{
			Out.ContactSec = JsonFloat(Json.Get("contactMs"), 0.f) / 1000.f;
		}
		if (Json.Get("releaseMs").IsNumber())
		{
			Out.ReleaseSec = JsonFloat(Json.Get("releaseMs"), 0.f) / 1000.f;
		}
		Out.RefSpeedCmS = JsonFloat(Json.Get("refSpeedCmS"), 0.f);
		for (const abyss::JsonValue& Notify : Json.Get("notifies").Items())
		{
			FAbyssArtNotify& N = Out.Notifies.AddDefaulted_GetRef();
			N.Name = JsonName(Notify.Get("name"));
			N.TimeSec = JsonFloat(Notify.Get("ms"), 0.f) / 1000.f;
		}
		Out.Notifies.StableSort([](const FAbyssArtNotify& A, const FAbyssArtNotify& B) { return A.TimeSec < B.TimeSec; });
		if (const FName Skill = JsonName(Json.Get("skill")); !Skill.IsNone())
		{
			Out.Skills.AddUnique(Skill);
		}
		for (const abyss::JsonValue& Skill : Json.Get("skills").Items())
		{
			if (const FName SkillName = JsonName(Skill); !SkillName.IsNone())
			{
				Out.Skills.AddUnique(SkillName);
			}
		}
	}

	void ParseFootprint(const abyss::JsonValue& Json, FAbyssArtAsset& Out)
	{
		const abyss::JsonValue* Footprint = Json.Find("footprintTiles");
		if (Footprint == nullptr)
		{
			Footprint = Json.Find("footprint");
		}
		if (Footprint == nullptr)
		{
			return;
		}
		if (Footprint->IsArray() && Footprint->Size() >= 2)
		{
			Out.FootprintW = FMath::Max(1, Footprint->At(0).AsInt(1));
			Out.FootprintH = FMath::Max(1, Footprint->At(1).AsInt(1));
			Out.bHasFootprint = true;
		}
		else if (Footprint->IsObject())
		{
			Out.FootprintW = FMath::Max(1, Footprint->Get("w").AsInt(1));
			Out.FootprintH = FMath::Max(1, Footprint->Get("h").AsInt(1));
			Out.bHasFootprint = true;
		}
		else if (Footprint->IsNumber())
		{
			Out.FootprintW = Out.FootprintH = FMath::Max(1, Footprint->AsInt(1));
			Out.bHasFootprint = true;
		}
	}

	int32 JsonInt(const abyss::JsonValue& Value, int32 Default)
	{
		return Value.IsNumber() ? FMath::RoundToInt32(Value.AsDouble(static_cast<double>(Default))) : Default;
	}

	void ParseTrail(const abyss::JsonValue& Json, FAbyssArtTrail& Out, const FAbyssArtTrail* Defaults)
	{
		if (!Json.IsObject())
		{
			return;
		}
		if (Defaults != nullptr && Defaults->bValid)
		{
			Out = *Defaults;   // castTrail omits samples / sampleMs: same history as the attack trail
		}
		Out.bValid = true;
		JsonColor(Json.Get("color"), Out.Color);
		Out.Alpha = JsonFloat(Json.Get("alpha"), Out.Alpha);
		const std::span<const abyss::JsonValue> Sockets = Json.Get("sockets").Items();
		if (Sockets.size() >= 2)
		{
			Out.TipSocket = JsonName(Sockets[0]);
			Out.MidSocket = JsonName(Sockets[1]);
		}
		Out.Samples = FMath::Clamp(JsonInt(Json.Get("samples"), Out.Samples), 2, 32);
		Out.SampleMs = FMath::Max(1.f, JsonFloat(Json.Get("sampleMs"), Out.SampleMs));
	}

	/** assets.<name>.fx attachments (see FAbyssArtFx); colours are sRGB hex in the manifest, linear here. */
	void ParseFx(const abyss::JsonValue& Fx, FAbyssArtFx& Out)
	{
		if (!Fx.IsObject())
		{
			return;
		}
		const abyss::JsonValue& Visor = Fx.Get("visorGlow");
		if (Visor.IsObject())
		{
			FAbyssArtGlowCard& V = Out.VisorGlow;
			V.bValid = true;
			V.Socket = JsonName(Visor.Get("socket"));
			if (V.Socket.IsNone())
			{
				V.Socket = FName(TEXT("visor"));
			}
			JsonColor(Visor.Get("color"), V.Color);
			V.RadiusCm = JsonFloat(Visor.Get("radiusCm"), V.RadiusCm);
			V.Alpha = JsonFloat(Visor.Get("alpha"), V.Alpha);
			V.AlphaPerFx = JsonFloat(Visor.Get("alphaPerFx"), V.AlphaPerFx);
			JsonColor(Visor.Get("coreColor"), V.CoreColor);
			V.CoreRadiusCm = JsonFloat(Visor.Get("coreRadiusCm"), V.CoreRadiusCm);
			V.CoreAlpha = JsonFloat(Visor.Get("coreAlpha"), V.CoreAlpha);
			V.OffAtDeathFraction = FMath::Clamp(JsonFloat(Visor.Get("offAtDeathFraction"), V.OffAtDeathFraction), 0.f, 1.f);
		}
		ParseTrail(Fx.Get("attackTrail"), Out.AttackTrail, nullptr);
		ParseTrail(Fx.Get("castTrail"), Out.CastTrail, &Out.AttackTrail);
		const abyss::JsonValue& Blade = Fx.Get("castBladeGlow");
		if (Blade.IsObject())
		{
			FAbyssArtBladeGlow& B = Out.CastBladeGlow;
			B.bValid = true;
			JsonColor(Blade.Get("color"), B.Color);
			B.RadiusCm = JsonFloat(Blade.Get("radiusCm"), B.RadiusCm);
			B.Alpha = JsonFloat(Blade.Get("alpha"), B.Alpha);
			JsonColor(Blade.Get("tipColor"), B.TipColor);
			B.TipRadiusCm = JsonFloat(Blade.Get("tipRadiusCm"), B.TipRadiusCm);
			B.TipAlpha = JsonFloat(Blade.Get("tipAlpha"), B.TipAlpha);
			const abyss::JsonValue& Embers = Blade.Get("embers");
			B.EmberCount = FMath::Clamp(JsonInt(Embers.Get("count"), 0), 0, 32);
			JsonColor(Embers.Get("color"), B.EmberColor);
		}
		const abyss::JsonValue& Halo = Fx.Get("heroHalo");
		if (Halo.IsObject())
		{
			Out.bHasHeroHalo = true;
			JsonColor(Halo.Get("color"), Out.HeroHaloColor);
			Out.HeroHaloRadiusCm = JsonFloat(Halo.Get("radiusCm"), Out.HeroHaloRadiusCm);
		}
		const abyss::JsonValue& Bloom = Fx.Get("bloom");
		if (Bloom.IsObject() && Bloom.Get("strength").IsNumber())
		{
			Out.bHasBloom = true;
			Out.BloomStrength = FMath::Max(0.f, JsonFloat(Bloom.Get("strength"), 0.f));
		}
	}

	void ParseAsset(FName AssetName, const abyss::JsonValue& Json, FAbyssArtAsset& Out)
	{
		Out.Name = AssetName;
		Out.Kind = JsonName(Json.Get("kind"));
		Out.Category = JsonName(Json.Get("category"));
		Out.Folder = FolderOfFbx(Json.Get("fbx"));
		if (Out.Folder.IsEmpty())
		{
			Out.Folder = Out.Category.ToString();
		}
		Out.Skeleton = JsonName(Json.Get("skeleton"));
		Out.Scale = JsonFloat(Json.Get("scale"), 1.f);
		Out.HeightCm = JsonFloat(Json.Get("heightCm"), 0.f);
		Out.BlobShadowRadiusCm = JsonFloat(Json.Get("blobShadowRadiusCm"), 0.f);
		FVector Min;
		FVector Max;
		const abyss::JsonValue& Bounds = Json.Get("boundsCm");
		if (JsonVector(Bounds.Get("min"), Min) && JsonVector(Bounds.Get("max"), Max))
		{
			Out.BoundsCm = FBox(Min, Max);
		}
		for (const abyss::JsonValue& GameId : Json.Get("gameIds").Items())
		{
			if (const FName Id = JsonName(GameId); !Id.IsNone())
			{
				Out.GameIds.AddUnique(Id);
			}
		}
		for (const abyss::JsonValue& Clip : Json.Get("anims").Items())
		{
			ParseClip(Clip, Out.Folder, Out.Clips.AddDefaulted_GetRef());
		}
		for (const abyss::JsonValue& Attachment : Json.Get("attachments").Items())
		{
			FAbyssArtAttachment& A = Out.Attachments.AddDefaulted_GetRef();
			A.Object = JsonName(Attachment.Get("object"));
			A.Socket = JsonName(Attachment.Get("socket"));
		}
		for (const abyss::JsonValue& Socket : Json.Get("sockets").Items())
		{
			FAbyssArtSocket& S = Out.Sockets.AddDefaulted_GetRef();
			S.Name = JsonName(Socket.Get("name"));
			S.Bone = JsonName(Socket.Get("bone"));
			if (!JsonVector(Socket.Get("relLocCm"), S.LocCm))
			{
				JsonVector(Socket.Get("locCm"), S.LocCm);
			}
		}
		Out.AttachSocket = JsonName(Json.Get("attachSocket"));
		Out.WeaponType = JsonName(Json.Get("weaponType"));
		for (const abyss::JsonValue& Holder : Json.Get("heldBy").Items())
		{
			Out.HeldBy.AddUnique(JsonName(Holder));
		}
		for (const abyss::JsonValue& Slot : Json.Get("materialSlots").Items())
		{
			if (const FName Class = JsonName(Slot.Get("class")); !Class.IsNone())
			{
				Out.OutlineClass = Class;
			}
		}
		ParseFootprint(Json, Out);
		if (const abyss::JsonValue* Blocking = Json.Find("blocking"); Blocking != nullptr && Blocking->IsBool())
		{
			Out.bBlocking = Blocking->AsBool();
		}
		const abyss::JsonValue& Fx = Json.Get("fx");
		Out.bHasImpactColor = JsonColor(Fx.Get("impactColor"), Out.ImpactColor);
		Out.bHasClassColor = JsonColor(Fx.Get("classColor"), Out.ClassColor);
		Out.bHasSpiritColor = JsonColor(Fx.Get("spiritColor"), Out.SpiritColor);
		ParseFx(Fx, Out.Fx);
	}

	void ParseShading(const abyss::JsonValue& Json, const abyss::JsonValue& Units, FAbyssArtShading& Out)
	{
		if (!Json.IsObject())
		{
			return;
		}
		Out.bValid = true;
		FVector Dir;
		if (JsonVector(Json.Get("keyLight").Get("dirToLightUE"), Dir) && !Dir.IsNearlyZero())
		{
			Out.DirToLight = Dir.GetSafeNormal();
		}
		const abyss::JsonValue& Rim = Json.Get("rim");
		JsonColor(Rim.Get("color"), Out.RimColor);
		Out.RimAlpha = JsonFloat(Rim.Get("alpha"), Out.RimAlpha);
		const abyss::JsonValue& Tone = Json.Get("toneConstants");
		JsonSrgbTriple(Tone.Get("coolShade"), Out.CoolShade);
		Out.CoolShadeMix = JsonFloat(Tone.Get("coolShadeMix"), Out.CoolShadeMix);
		JsonSrgbTriple(Tone.Get("warmLight"), Out.WarmLight);
		const abyss::JsonValue& Camera = Json.Get("camera");
		Out.CameraDistanceCm = JsonFloat(Camera.Get("defaultDistanceCm"), Out.CameraDistanceCm);
		Out.CameraFocusZCm = JsonFloat(Camera.Get("focusZCm"), Out.CameraFocusZCm);
		Out.CameraFovHDeg = JsonFloat(Camera.Get("fovH"), Out.CameraFovHDeg);
		Out.CameraPitchDeg = JsonFloat(Camera.Get("pitchUE"), Out.CameraPitchDeg);
		Out.CameraYawDeg = JsonFloat(Camera.Get("yawUE"), Out.CameraYawDeg);
		// units.characterForward: "+Y (mesh); actor rotates the mesh yaw -90" (a +X-facing export would need no offset).
		const FString Forward = JsonString(Units.Get("characterForward"));
		if (Forward.StartsWith(TEXT("+X")))
		{
			Out.MeshYawOffsetDeg = 0.f;
		}
	}
}

// =====================================================================================================================

FSoftObjectPath FAbyssArtClip::GetObjectPath() const
{
	return FAbyssArtManifest::MakeObjectPath(Folder, Asset);
}

bool FAbyssArtAsset::IsSkeletal() const
{
	static const FName SkeletalMeshKind(TEXT("SkeletalMesh"));
	return Kind == SkeletalMeshKind || Name.ToString().StartsWith(TEXT("SK_"));
}

FSoftObjectPath FAbyssArtAsset::GetObjectPath() const
{
	return FAbyssArtManifest::MakeObjectPath(Folder, Name);
}

const FAbyssArtClip* FAbyssArtAsset::FindClip(FName ClipName) const
{
	return Clips.FindByPredicate([ClipName](const FAbyssArtClip& Clip) { return Clip.Name == ClipName; });
}

const FAbyssArtClip* FAbyssArtAsset::FindClipForSkill(FName SkillId) const
{
	if (SkillId.IsNone())
	{
		return nullptr;
	}
	// A signature montage wins over a shared clip that merely lists the skill.
	if (const FAbyssArtClip* Signature = Clips.FindByPredicate([SkillId](const FAbyssArtClip& Clip)
		{ return Clip.bSignature && Clip.Skills.Contains(SkillId); }))
	{
		return Signature;
	}
	return Clips.FindByPredicate([SkillId](const FAbyssArtClip& Clip) { return Clip.Skills.Contains(SkillId); });
}

const FAbyssArtSocket* FAbyssArtAsset::FindSocket(FName SocketName) const
{
	return Sockets.FindByPredicate([SocketName](const FAbyssArtSocket& Socket) { return Socket.Name == SocketName; });
}

float FAbyssArtAsset::GetVisualHeightCm() const
{
	if (BoundsCm.IsValid)
	{
		return static_cast<float>(BoundsCm.Max.Z - FMath::Min(0.0, BoundsCm.Min.Z));
	}
	return HeightCm;
}

// =====================================================================================================================

void FAbyssArtManifest::Reset()
{
	Assets.Reset();
	AssetsByName.Reset();
	AssetsByGameId.Reset();
	Shading = FAbyssArtShading();
	bLoaded = false;
}

bool FAbyssArtManifest::LoadFromJson(std::string_view Json, FString& OutError)
{
	using namespace AbyssArtManifestPrivate;
	Reset();

	abyss::JsonValue Root;
	abyss::JsonParseError ParseError;
	if (!abyss::ParseJson(Json, Root, &ParseError))
	{
		OutError = FString::Printf(TEXT("JSON parse error at byte %d: %s"), static_cast<int32>(ParseError.offset),
			*AbyssText::ToFString(ParseError.message));
		return false;
	}
	const abyss::JsonValue* Manifest = Root.Find("manifest");
	if (Manifest == nullptr || !Manifest->IsObject())
	{
		Manifest = &Root;
	}
	const abyss::JsonValue* AssetTable = Manifest->Find("assets");
	if (AssetTable == nullptr || !AssetTable->IsObject())
	{
		OutError = TEXT("manifest has no `assets` object");
		return false;
	}

	for (const abyss::JsonMember& Member : AssetTable->Members())
	{
		const FName AssetName(*AbyssText::ToFString(Member.key));
		if (AssetName.IsNone() || AssetsByName.Contains(AssetName))
		{
			continue;
		}
		FAbyssArtAsset& Asset = Assets.AddDefaulted_GetRef();
		ParseAsset(AssetName, Member.value, Asset);
		AssetsByName.Add(AssetName, Assets.Num() - 1);
	}

	// Game id index: every asset's own list, then the top-level map (single asset per id).
	auto AddGameId = [this](FName GameId, int32 AssetIndex)
	{
		TArray<int32>& List = AssetsByGameId.FindOrAdd(GameId);
		List.AddUnique(AssetIndex);
	};
	for (int32 Index = 0; Index < Assets.Num(); ++Index)
	{
		for (const FName GameId : Assets[Index].GameIds)
		{
			AddGameId(GameId, Index);
		}
	}
	if (const abyss::JsonValue* GameIds = Manifest->Find("gameIds"); GameIds != nullptr && GameIds->IsObject())
	{
		for (const abyss::JsonMember& Member : GameIds->Members())
		{
			const FName GameId(*AbyssText::ToFString(Member.key));
			if (const int32* AssetIndex = AssetsByName.Find(JsonName(Member.value)))
			{
				AddGameId(GameId, *AssetIndex);
			}
		}
	}
	for (TPair<FName, TArray<int32>>& Pair : AssetsByGameId)
	{
		Pair.Value.StableSort([this](int32 A, int32 B)
			{ return Assets[A].Name.ToString().Compare(Assets[B].Name.ToString(), ESearchCase::CaseSensitive) < 0; });
	}

	ParseShading(Manifest->Get("shading"), Manifest->Get("units"), Shading);
	bLoaded = true;
	return true;
}

bool FAbyssArtManifest::LoadFromProject(FString& OutError, FString& OutSourcePath)
{
	const FString Candidates[] = {
		FPaths::ProjectDir() / TEXT("Data/assets.json"),
		// Editor / uncooked runs: the art pipeline's file before the exporter copied it into Data/.
		FPaths::ProjectDir() / TEXT("Art/Export/manifest.json"),
	};
	FString LastError = TEXT("no manifest file found (Data/assets.json, Art/Export/manifest.json)");
	for (const FString& Path : Candidates)
	{
		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *Path, FILEREAD_Silent))
		{
			continue;
		}
		const std::string_view Text(reinterpret_cast<const char*>(Bytes.GetData()), static_cast<size_t>(Bytes.Num()));
		FString Error;
		if (LoadFromJson(Text, Error))
		{
			OutSourcePath = Path;
			return true;
		}
		LastError = FString::Printf(TEXT("%s: %s"), *Path, *Error);
	}
	OutError = LastError;
	return false;
}

const FAbyssArtAsset* FAbyssArtManifest::FindAsset(FName AssetName) const
{
	const int32* Index = AssetsByName.Find(AssetName);
	return Index ? &Assets[*Index] : nullptr;
}

void FAbyssArtManifest::FindByGameId(FName GameId, TArray<const FAbyssArtAsset*>& Out) const
{
	Out.Reset();
	if (const TArray<int32>* List = AssetsByGameId.Find(GameId))
	{
		for (const int32 Index : *List)
		{
			Out.Add(&Assets[Index]);
		}
	}
}

bool FAbyssArtManifest::HasGameId(FName GameId) const
{
	const TArray<int32>* List = AssetsByGameId.Find(GameId);
	return List != nullptr && List->Num() > 0;
}

const FAbyssArtAsset* FAbyssArtManifest::ResolveGameId(FName GameId, uint32 VariantHash) const
{
	const TArray<int32>* List = AssetsByGameId.Find(GameId);
	if (List == nullptr || List->Num() == 0)
	{
		return nullptr;
	}
	return &Assets[(*List)[static_cast<int32>(VariantHash % static_cast<uint32>(List->Num()))]];
}

const FAbyssArtAsset* FAbyssArtManifest::ResolveFirst(TConstArrayView<FName> GameIds, uint32 VariantHash) const
{
	for (const FName GameId : GameIds)
	{
		if (const FAbyssArtAsset* Asset = ResolveGameId(GameId, VariantHash))
		{
			return Asset;
		}
		// An asset name used directly as an id (SM_..., SK_...) also resolves.
		if (const FAbyssArtAsset* Direct = FindAsset(GameId))
		{
			return Direct;
		}
	}
	return nullptr;
}

const FAbyssArtAsset* FAbyssArtManifest::FindWeapon(FName WeaponType, FName HeldBy) const
{
	if (WeaponType.IsNone())
	{
		return nullptr;
	}
	const FAbyssArtAsset* AnyHolder = nullptr;
	for (const FAbyssArtAsset& Asset : Assets)
	{
		if (Asset.WeaponType != WeaponType || Asset.IsSkeletal())
		{
			continue;
		}
		if (!HeldBy.IsNone() && Asset.HeldBy.Contains(HeldBy))
		{
			return &Asset;
		}
		if (AnyHolder == nullptr || Asset.HeldBy.Num() == 0)
		{
			AnyHolder = &Asset;
		}
	}
	return AnyHolder;
}

FSoftObjectPath FAbyssArtManifest::MakeObjectPath(const FString& Folder, FName AssetName)
{
	if (AssetName.IsNone())
	{
		return FSoftObjectPath();
	}
	const FString Name = AssetName.ToString();
	FString Path = ContentRoot();
	if (!Folder.IsEmpty())
	{
		Path /= Folder;
	}
	Path /= Name;
	return FSoftObjectPath(FString::Printf(TEXT("%s.%s"), *Path, *Name));
}

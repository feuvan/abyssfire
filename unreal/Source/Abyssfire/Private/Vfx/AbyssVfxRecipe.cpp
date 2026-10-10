#include "Vfx/AbyssVfxRecipe.h"

#include "Abyssfire.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include <string>

#include "abyss/base/Json.h"

#include "Framework/AbyssText.h"
#include "World/AbyssWorldTypes.h"

namespace AbyssVfxRecipePrivate
{
	FName ToName(std::string_view Text)
	{
		return Text.empty() ? NAME_None : FName(*AbyssText::ToFString(Text));
	}

	bool Equals(std::string_view A, const char* B)
	{
		return A == std::string_view(B);
	}

	/** number | [min, max] | [value] */
	bool ReadRange(const abyss::JsonValue& Json, FAbyssVfxRange& Out)
	{
		if (Json.IsNumber())
		{
			Out = FAbyssVfxRange(static_cast<float>(Json.AsDouble()));
			return true;
		}
		if (Json.IsArray() && Json.Size() >= 1 && Json.At(0).IsNumber())
		{
			const float A = static_cast<float>(Json.At(0).AsDouble());
			const float B = Json.Size() >= 2 && Json.At(1).IsNumber() ? static_cast<float>(Json.At(1).AsDouble()) : A;
			Out = FAbyssVfxRange(FMath::Min(A, B), FMath::Max(A, B));
			return true;
		}
		return false;
	}

	void ReadFloat(const abyss::JsonValue& Parent, const char* Key, float& Out)
	{
		const abyss::JsonValue& Value = Parent.Get(Key);
		if (Value.IsNumber())
		{
			Out = static_cast<float>(Value.AsDouble());
		}
	}

	void ReadInt(const abyss::JsonValue& Parent, const char* Key, int32& Out)
	{
		const abyss::JsonValue& Value = Parent.Get(Key);
		if (Value.IsNumber())
		{
			Out = static_cast<int32>(Value.AsDouble());
		}
	}

	void ReadBool(const abyss::JsonValue& Parent, const char* Key, bool& Out)
	{
		const abyss::JsonValue& Value = Parent.Get(Key);
		if (Value.IsBool())
		{
			Out = Value.AsBool();
		}
	}

	void ReadRangeField(const abyss::JsonValue& Parent, const char* Key, FAbyssVfxRange& Out)
	{
		ReadRange(Parent.Get(Key), Out);
	}

	bool ReadVector(const abyss::JsonValue& Json, FVector& Out)
	{
		if (!Json.IsArray() || Json.Size() < 3)
		{
			return false;
		}
		Out = FVector(Json.At(0).AsDouble(), Json.At(1).AsDouble(), Json.At(2).AsDouble());
		return true;
	}

	bool ParseOrient(std::string_view Text, EAbyssVfxOrient& Out)
	{
		if (Equals(Text, "billboard")) { Out = EAbyssVfxOrient::Billboard; return true; }
		if (Equals(Text, "ground")) { Out = EAbyssVfxOrient::Ground; return true; }
		if (Equals(Text, "velocity")) { Out = EAbyssVfxOrient::Velocity; return true; }
		if (Equals(Text, "upright")) { Out = EAbyssVfxOrient::Upright; return true; }
		if (Equals(Text, "beam")) { Out = EAbyssVfxOrient::Beam; return true; }
		return false;
	}

	bool ParseAnchor(std::string_view Text, EAbyssVfxAnchor& Out)
	{
		if (Equals(Text, "origin") || Equals(Text, "source") || Equals(Text, "caster")) { Out = EAbyssVfxAnchor::Origin; return true; }
		if (Equals(Text, "target")) { Out = EAbyssVfxAnchor::Target; return true; }
		if (Equals(Text, "point")) { Out = EAbyssVfxAnchor::Point; return true; }
		if (Equals(Text, "points")) { Out = EAbyssVfxAnchor::Points; return true; }
		if (Equals(Text, "path")) { Out = EAbyssVfxAnchor::Path; return true; }
		if (Equals(Text, "camera")) { Out = EAbyssVfxAnchor::Camera; return true; }
		return false;
	}

	bool ParseHeight(const abyss::JsonValue& Json, EAbyssVfxHeight& Out, float& OutCm)
	{
		if (Json.IsNumber())
		{
			Out = EAbyssVfxHeight::Custom;
			OutCm = static_cast<float>(Json.AsDouble());
			return true;
		}
		const std::string_view Text = Json.AsString();
		if (Equals(Text, "ground") || Equals(Text, "feet")) { Out = EAbyssVfxHeight::Ground; return true; }
		if (Equals(Text, "chest")) { Out = EAbyssVfxHeight::Chest; return true; }
		if (Equals(Text, "head")) { Out = EAbyssVfxHeight::Head; return true; }
		if (Equals(Text, "overhead")) { Out = EAbyssVfxHeight::Overhead; return true; }
		if (Equals(Text, "hand")) { Out = EAbyssVfxHeight::Hand; return true; }
		return false;
	}

	bool ParseDir(std::string_view Text, EAbyssVfxDir& Out)
	{
		if (Equals(Text, "random")) { Out = EAbyssVfxDir::Random; return true; }
		if (Equals(Text, "up")) { Out = EAbyssVfxDir::Up; return true; }
		if (Equals(Text, "down")) { Out = EAbyssVfxDir::Down; return true; }
		if (Equals(Text, "blow")) { Out = EAbyssVfxDir::Blow; return true; }
		if (Equals(Text, "back")) { Out = EAbyssVfxDir::Back; return true; }
		if (Equals(Text, "out")) { Out = EAbyssVfxDir::Out; return true; }
		if (Equals(Text, "in")) { Out = EAbyssVfxDir::In; return true; }
		if (Equals(Text, "flat")) { Out = EAbyssVfxDir::Flat; return true; }
		if (Equals(Text, "tangent")) { Out = EAbyssVfxDir::Tangent; return true; }
		return false;
	}

	bool ParseUnit(std::string_view Text, EAbyssVfxUnit& Out)
	{
		if (Equals(Text, "cm")) { Out = EAbyssVfxUnit::Cm; return true; }
		if (Equals(Text, "ring")) { Out = EAbyssVfxUnit::Ring; return true; }
		if (Equals(Text, "radius")) { Out = EAbyssVfxUnit::Radius; return true; }
		return false;
	}

	/** "#RRGGBB" | "core" | "mid" | "rim" | "dark" | "event" | "white" | 0xRRGGBB number. */
	bool ParseColor(const abyss::JsonValue& Json, EAbyssVfxColor& OutSource, FLinearColor& OutColor)
	{
		if (Json.IsNumber())
		{
			OutSource = EAbyssVfxColor::Fixed;
			OutColor = AbyssWorldUtil::ColorFromRgb(static_cast<uint32>(Json.AsInt64()));
			return true;
		}
		if (!Json.IsString())
		{
			return false;
		}
		const std::string_view Text = Json.AsString();
		if (Equals(Text, "core")) { OutSource = EAbyssVfxColor::Core; return true; }
		if (Equals(Text, "mid")) { OutSource = EAbyssVfxColor::Mid; return true; }
		if (Equals(Text, "rim")) { OutSource = EAbyssVfxColor::Rim; return true; }
		if (Equals(Text, "dark")) { OutSource = EAbyssVfxColor::Dark; return true; }
		if (Equals(Text, "event")) { OutSource = EAbyssVfxColor::Event; return true; }
		if (Equals(Text, "white")) { OutSource = EAbyssVfxColor::White; return true; }
		FLinearColor Parsed;
		if (AbyssWorldUtil::ParseHexColor(AbyssText::ToFString(Text), Parsed))
		{
			OutSource = EAbyssVfxColor::Fixed;
			OutColor = Parsed;
			return true;
		}
		return false;
	}

	FLinearColor Hex(uint32 Rgb)
	{
		return AbyssWorldUtil::ColorFromRgb(Rgb);
	}

	FAbyssVfxPalette MakePalette(uint32 Core, uint32 Mid, uint32 Rim, uint32 Dark)
	{
		FAbyssVfxPalette P;
		P.Core = Hex(Core);
		P.Mid = Hex(Mid);
		P.Rim = Hex(Rim);
		P.Dark = Hex(Dark);
		return P;
	}

	bool LoadFileText(const FString& Path, std::string& Out)
	{
		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *Path, FILEREAD_Silent))
		{
			return false;
		}
		Out.assign(reinterpret_cast<const char*>(Bytes.GetData()), static_cast<size_t>(Bytes.Num()));
		return true;
	}
}

// =====================================================================================================================
// Palette
// =====================================================================================================================

FAbyssVfxPalette FAbyssVfxPalette::FromColor(const FLinearColor& Color)
{
	FAbyssVfxPalette P;
	P.Core = FMath::Lerp(Color, FLinearColor::White, 0.7f);
	P.Mid = FMath::Lerp(Color, FLinearColor::White, 0.25f);
	P.Rim = Color;
	P.Dark = Color * 0.3f;
	P.Core.A = P.Mid.A = P.Rim.A = P.Dark.A = 1.f;
	return P;
}

FLinearColor FAbyssVfxPalette::Get(EAbyssVfxColor Slot, const FLinearColor& EventColor, bool bHasEventColor) const
{
	switch (Slot)
	{
	case EAbyssVfxColor::Core: return Core;
	case EAbyssVfxColor::Mid: return Mid;
	case EAbyssVfxColor::Rim: return Rim;
	case EAbyssVfxColor::Dark: return Dark;
	case EAbyssVfxColor::White: return FLinearColor::White;
	case EAbyssVfxColor::Event: return bHasEventColor ? EventColor : Mid;
	case EAbyssVfxColor::Fixed: break;
	}
	return FLinearColor::White;
}

// =====================================================================================================================
// Presets (web FxKit helpers, art-inventory-ch1.md 8.3; px -> cm x 2.22)
// =====================================================================================================================

bool FAbyssVfxLibrary::ApplyPreset(FName Emitter, FAbyssVfxLayer& L)
{
	const FString Name = Emitter.ToString();
	if (Name == TEXT("glow"))
	{
		L.Sprite = FName(TEXT("Glow"));
		L.Orient = EAbyssVfxOrient::Billboard;
		L.Blend = EAbyssVfxBlend::Additive;
		L.LifeMs = FAbyssVfxRange(300.f);
		L.SizeCm = FAbyssVfxRange(53.f);   // web size 40 px x .6
		L.Grow = 1.3f / 0.6f;
		L.SizePow = 2.f;
		L.Alpha0 = 0.9f;
		L.Alpha1 = 0.f;
		L.FadeIn = 0.08f;
		L.AlphaPow = 1.6f;
		return true;
	}
	if (Name == TEXT("flash"))
	{
		L.Sprite = FName(TEXT("Core"));
		L.Orient = EAbyssVfxOrient::Billboard;
		L.Blend = EAbyssVfxBlend::Additive;
		L.LifeMs = FAbyssVfxRange(160.f);
		L.SizeCm = FAbyssVfxRange(25.f);
		L.Grow = 0.85f / 0.35f;
		L.SizePow = 2.f;
		L.Alpha0 = 0.9f;
		L.ColorSource = EAbyssVfxColor::Core;
		L.Intensity = 1.5f;
		return true;
	}
	if (Name == TEXT("glint"))
	{
		L.Sprite = FName(TEXT("Spark"));
		L.Orient = EAbyssVfxOrient::Billboard;
		L.LifeMs = FAbyssVfxRange(200.f);
		L.SizeCm = FAbyssVfxRange(18.f);
		L.Grow = 1.f / 0.3f;
		L.SizePow = 3.f;
		L.Alpha0 = 1.f;
		L.FadeIn = 0.15f;
		L.AlphaPow = 1.5f;
		L.ColorSource = EAbyssVfxColor::White;
		L.Intensity = 1.5f;
		return true;
	}
	if (Name == TEXT("sparks"))
	{
		L.Sprite = FName(TEXT("Streak"));
		L.Orient = EAbyssVfxOrient::Velocity;
		L.Count = FAbyssVfxRange(6.f);
		L.SpeedCmS = FAbyssVfxRange(200.f, 444.f);
		L.Dir = EAbyssVfxDir::Random;
		L.LifeMs = FAbyssVfxRange(180.f, 340.f);
		L.Drag = 3.f;
		L.SizeCm = FAbyssVfxRange(8.f, 12.f);
		L.Aspect = 0.25f;
		L.StretchPerSpeed = 0.06f;
		L.Grow = 0.5f;
		L.Alpha0 = 1.f;
		L.AlphaPow = 1.2f;
		L.ColorSource = EAbyssVfxColor::Mid;
		L.AltEvery = 3;
		L.AltColorSource = EAbyssVfxColor::White;
		L.Intensity = 1.4f;
		return true;
	}
	if (Name == TEXT("streak"))
	{
		L.Sprite = FName(TEXT("Streak"));
		L.Orient = EAbyssVfxOrient::Velocity;
		L.LifeMs = FAbyssVfxRange(200.f);
		L.SizeCm = FAbyssVfxRange(10.f);
		L.Aspect = 0.2f;
		L.StretchPerSpeed = 0.08f;
		L.Alpha0 = 1.f;
		return true;
	}
	if (Name == TEXT("motes"))
	{
		L.Sprite = FName(TEXT("Ember"));
		L.Orient = EAbyssVfxOrient::Billboard;
		L.Count = FAbyssVfxRange(6.f);
		L.SpawnRadiusCm = 30.f;
		L.SpeedCmS = FAbyssVfxRange(44.f, 155.f);
		L.Dir = EAbyssVfxDir::Random;
		L.LifeMs = FAbyssVfxRange(400.f, 700.f);
		L.SizeCm = FAbyssVfxRange(6.f, 12.f);
		L.GravityCmS2 = -90.f;
		L.Drag = 1.5f;
		L.Alpha0 = 1.f;
		L.AlphaPow = 1.5f;
		L.SpinDegS = FAbyssVfxRange(-120.f, 120.f);
		L.Intensity = 1.3f;
		return true;
	}
	if (Name == TEXT("flames"))
	{
		L.Sprite = FName(TEXT("Flame"));
		L.Orient = EAbyssVfxOrient::Upright;
		L.Blend = EAbyssVfxBlend::Additive;
		L.Count = FAbyssVfxRange(6.f);
		L.SpawnRadiusCm = 20.f;
		L.UpCmS = FAbyssVfxRange(67.f, 155.f);
		L.LifeMs = FAbyssVfxRange(300.f, 520.f);
		L.SizeCm = FAbyssVfxRange(30.f, 50.f);
		L.Aspect = 0.62f;
		L.Grow = 0.7f;
		L.Alpha0 = 0.95f;
		L.FadeIn = 0.1f;
		L.AlphaPow = 1.4f;
		L.ColorSource = EAbyssVfxColor::Mid;
		L.Intensity = 1.3f;
		return true;
	}
	if (Name == TEXT("smoke"))
	{
		L.Sprite = FName(TEXT("Smoke"));
		L.RandomVariants = 2;
		L.Orient = EAbyssVfxOrient::Billboard;
		L.Blend = EAbyssVfxBlend::Translucent;
		L.Count = FAbyssVfxRange(4.f);
		L.SpawnRadiusCm = 20.f;
		L.SpeedCmS = FAbyssVfxRange(33.f, 100.f);
		L.Dir = EAbyssVfxDir::Flat;
		L.GravityCmS2 = -44.f;
		L.Drag = 2.2f;
		L.LifeMs = FAbyssVfxRange(500.f, 900.f);
		L.SizeCm = FAbyssVfxRange(30.f, 50.f);
		L.Grow = 1.25f / 0.6f;
		L.SizePow = 1.5f;
		L.Alpha0 = 0.8f;
		L.FadeIn = 0.12f;
		L.AlphaPow = 1.3f;
		L.SpinDegS = FAbyssVfxRange(-60.f, 60.f);
		L.RotationDeg = FAbyssVfxRange(0.f, 360.f);
		L.ColorSource = EAbyssVfxColor::Fixed;
		L.Color = AbyssVfxRecipePrivate::Hex(0x5A5060);
		return true;
	}
	if (Name == TEXT("debris"))
	{
		L.Mesh = FName(TEXT("SM_FX_Rock_A"));
		L.Sprite = FName(TEXT("Rock"));
		L.RandomVariants = 2;
		L.Orient = EAbyssVfxOrient::Billboard;
		L.Blend = EAbyssVfxBlend::Translucent;
		L.Count = FAbyssVfxRange(4.f);
		L.SpeedCmS = FAbyssVfxRange(133.f, 289.f);
		L.Dir = EAbyssVfxDir::Flat;
		L.UpCmS = FAbyssVfxRange(244.f, 377.f);
		L.GravityCmS2 = 1155.f;
		L.LifeMs = FAbyssVfxRange(420.f, 560.f);
		L.SizeCm = FAbyssVfxRange(8.f, 14.f);
		L.Grow = 1.f;
		L.Alpha0 = 1.f;
		L.AlphaPow = 5.f;
		L.SpinDegS = FAbyssVfxRange(-516.f, 516.f);
		L.RotationDeg = FAbyssVfxRange(0.f, 360.f);
		L.ColorSource = EAbyssVfxColor::Fixed;
		L.Color = AbyssVfxRecipePrivate::Hex(0x8A8E98);
		return true;
	}
	if (Name == TEXT("ring"))
	{
		L.Sprite = FName(TEXT("Ring"));
		L.Orient = EAbyssVfxOrient::Ground;
		L.LifeMs = FAbyssVfxRange(400.f);
		L.SizeCm = FAbyssVfxRange(20.f);
		L.Grow = 5.f;
		L.SizePow = 3.f;
		L.Alpha0 = 0.8f;
		L.FadeIn = 0.05f;
		L.AlphaPow = 1.5f;
		L.HeightCm = 3.f;
		L.Height = EAbyssVfxHeight::Custom;
		return true;
	}
	if (Name == TEXT("shock"))
	{
		L.Sprite = FName(TEXT("Shock"));
		L.Orient = EAbyssVfxOrient::Ground;
		L.LifeMs = FAbyssVfxRange(400.f);
		L.SizeCm = FAbyssVfxRange(20.f);
		L.Grow = 5.f;
		L.SizePow = 3.f;
		L.Alpha0 = 0.9f;
		L.AlphaPow = 1.f;
		L.HeightCm = 2.f;
		L.Height = EAbyssVfxHeight::Custom;
		return true;
	}
	if (Name == TEXT("decal"))
	{
		L.Sprite = FName(TEXT("Scorch"));
		L.Orient = EAbyssVfxOrient::Ground;
		L.Blend = EAbyssVfxBlend::Translucent;
		L.LifeMs = FAbyssVfxRange(1400.f);
		L.SizeCm = FAbyssVfxRange(80.f);
		L.Grow = 1.f / 0.85f;
		L.SizePow = 2.f;
		L.Alpha0 = 0.6f;
		L.FadeIn = 0.1f;
		L.AlphaPow = 2.5f;
		L.RotationDeg = FAbyssVfxRange(0.f, 360.f);
		L.HeightCm = 1.f;
		L.Height = EAbyssVfxHeight::Custom;
		L.ColorSource = EAbyssVfxColor::Fixed;
		L.Color = FLinearColor(0.08f, 0.06f, 0.06f);
		return true;
	}
	if (Name == TEXT("slash"))
	{
		L.Sprite = FName(TEXT("Slash"));
		L.Orient = EAbyssVfxOrient::Billboard;
		L.LifeMs = FAbyssVfxRange(150.f, 230.f);
		L.SizeCm = FAbyssVfxRange(45.f);
		L.Aspect = 2.f;
		L.Grow = 1.2f;
		L.SizePow = 2.f;
		L.Alpha0 = 1.f;
		L.AlphaPow = 1.5f;
		L.bAlignToBlow = true;
		L.Intensity = 1.3f;
		return true;
	}
	if (Name == TEXT("swipe"))
	{
		L.Sprite = FName(TEXT("Slash"));
		L.Orient = EAbyssVfxOrient::Ground;
		L.LifeMs = FAbyssVfxRange(230.f);
		L.SizeCm = FAbyssVfxRange(60.f);
		L.Aspect = 2.f;
		L.Grow = 1.1f;
		L.SizePow = 2.f;
		L.Alpha0 = 1.f;
		L.AlphaPow = 1.5f;
		L.bAlignToBlow = true;
		L.Height = EAbyssVfxHeight::Chest;
		L.Intensity = 1.3f;
		return true;
	}
	if (Name == TEXT("beam"))
	{
		L.Sprite = FName(TEXT("Beam"));
		L.Orient = EAbyssVfxOrient::Upright;
		L.LifeMs = FAbyssVfxRange(700.f);
		L.SizeCm = FAbyssVfxRange(150.f);
		L.Aspect = 0.33f;
		L.Grow = 2.f;
		L.SizePow = 3.f;
		L.Alpha0 = 0.9f;
		L.FadeIn = 0.1f;
		L.AlphaPow = 1.f;
		L.Intensity = 1.3f;
		return true;
	}
	if (Name == TEXT("gather"))
	{
		L.Sprite = FName(TEXT("Glow"));
		L.Orient = EAbyssVfxOrient::Billboard;
		L.Count = FAbyssVfxRange(10.f);
		L.bOrbit = true;
		L.OrbitRadius0Cm = FAbyssVfxRange(90.f, 110.f);
		L.OrbitRadius1Cm = FAbyssVfxRange(0.f);
		L.OrbitDegS = FAbyssVfxRange(143.f, 229.f);
		L.LifeMs = FAbyssVfxRange(260.f, 340.f);
		L.SizeCm = FAbyssVfxRange(10.f, 16.f);
		L.Alpha0 = 0.2f;
		L.Alpha1 = 1.f;
		L.AlphaPow = 1.f;
		L.Height = EAbyssVfxHeight::Chest;
		L.Intensity = 1.3f;
		return true;
	}
	if (Name == TEXT("bolt"))
	{
		L.Sprite = FName(TEXT("Bolt"));
		L.RandomVariants = 4;
		L.Orient = EAbyssVfxOrient::Beam;
		L.LifeMs = FAbyssVfxRange(240.f);
		L.SizeCm = FAbyssVfxRange(18.f);
		L.Grow = 1.f;
		L.Alpha0 = 1.f;
		L.AlphaPow = 1.5f;
		L.Flicker = 0.5f;
		L.Height = EAbyssVfxHeight::Chest;
		L.BeamToHeight = EAbyssVfxHeight::Chest;
		L.Intensity = 1.6f;
		return true;
	}
	if (Name == TEXT("mesh"))
	{
		L.Orient = EAbyssVfxOrient::Billboard;
		L.Blend = EAbyssVfxBlend::Translucent;
		L.LifeMs = FAbyssVfxRange(400.f);
		L.SizeCm = FAbyssVfxRange(100.f);   // mesh layers: the bounding-sphere diameter drawn (cm)
		L.Grow = 1.f;
		L.Alpha0 = 1.f;
		L.Alpha1 = 1.f;
		return true;
	}
	return false;
}

// =====================================================================================================================
// Library
// =====================================================================================================================

void FAbyssVfxLibrary::Reset()
{
	Recipes.Reset();
	Palettes.Reset();
	SpriteCells.Reset();
}

int32 FAbyssVfxLibrary::GetSpriteCells(FName Sprite) const
{
	const int32* Cells = SpriteCells.Find(Sprite);
	return Cells != nullptr ? *Cells : 1;
}

void FAbyssVfxLibrary::AddBuiltInPalettes()
{
	using AbyssVfxRecipePrivate::MakePalette;
	// FxKit PAL (art-inventory-ch1.md 8.2).
	Palettes.Add(FName(TEXT("fire")), MakePalette(0xFFF3C4, 0xFFA532, 0xFF5418, 0x8A1C08));
	Palettes.Add(FName(TEXT("frost")), MakePalette(0xFFFFFF, 0xA6F0FF, 0x46B4F0, 0x1E5AA8));
	Palettes.Add(FName(TEXT("lightning")), MakePalette(0xFFFFFF, 0xCFE0FF, 0x8C8CFF, 0x4A3AA8));
	Palettes.Add(FName(TEXT("poison")), MakePalette(0xF0FFC0, 0x9AEC40, 0x3FAE2E, 0x1C5A1E));
	Palettes.Add(FName(TEXT("shadow")), MakePalette(0xF0DCFF, 0xB478FF, 0x7030C0, 0x24103E));
	Palettes.Add(FName(TEXT("holy")), MakePalette(0xFFFBE4, 0xFFE27A, 0xF4AC28, 0x8A5A10));
	Palettes.Add(FName(TEXT("steel")), MakePalette(0xFFFFFF, 0xEEF3FB, 0xAABBD4, 0x56647C));
	Palettes.Add(FName(TEXT("blood")), MakePalette(0xFFB0A0, 0xE8342C, 0xA01420, 0x4A0810));
	Palettes.Add(FName(TEXT("rage")), MakePalette(0xFFE0B0, 0xFF5A2A, 0xD01E1E, 0x5A0A0A));
	Palettes.Add(FName(TEXT("arcane")), MakePalette(0xFFFFFF, 0xD8B4FF, 0x8A5CFF, 0x34207A));
	Palettes.Add(FName(TEXT("nature")), MakePalette(0xF6FFE0, 0xA8F07A, 0x40C060, 0x1A5A2A));
	Palettes.Add(FName(TEXT("earth")), MakePalette(0xFFF0C8, 0xE8C47A, 0xB08850, 0x5A4430));
	FallbackPalette = MakePalette(0xFFF2C0, 0xF39C12, 0xD87A0A, 0x5A3A08);
}

void FAbyssVfxLibrary::LoadAll()
{
	using namespace AbyssVfxRecipePrivate;
	Reset();
	AddBuiltInPalettes();

	int32 DocumentIndex = 0;
	for (const char* Document : AbyssVfxDefaults::Documents())
	{
		FString Error;
		if (!MergeJson(std::string_view(Document), FString::Printf(TEXT("built-in #%d"), DocumentIndex), Error))
		{
			UE_LOG(LogAbyss, Error, TEXT("VFX defaults #%d: %s"), DocumentIndex, *Error);
		}
		++DocumentIndex;
	}
	const int32 BuiltIn = Recipes.Num();

	const FString Candidates[] = {
		FPaths::ProjectDir() / TEXT("Data/vfx_recipes.json"),
		FPaths::ProjectDir() / TEXT("Art/Export/VFX/vfx_recipes.json"),
	};
	for (const FString& Path : Candidates)
	{
		std::string Text;
		if (!LoadFileText(Path, Text))
		{
			continue;
		}
		FString Error;
		if (MergeJson(Text, Path, Error))
		{
			UE_LOG(LogAbyss, Log, TEXT("VFX recipes: %s merged (%d recipes, %d built in)"), *Path, Recipes.Num(), BuiltIn);
		}
		else
		{
			UE_LOG(LogAbyss, Error, TEXT("VFX recipes: %s: %s"), *Path, *Error);
		}
		break;   // the staged copy wins over the art export
	}
	UE_LOG(LogAbyss, Log, TEXT("VFX library: %d recipes, %d palettes"), Recipes.Num(), Palettes.Num());
}

bool FAbyssVfxLibrary::MergeJson(std::string_view Json, const FString& SourceName, FString& OutError)
{
	using namespace AbyssVfxRecipePrivate;
	abyss::JsonValue Root;
	abyss::JsonParseError ParseError;
	if (!abyss::ParseJson(Json, Root, &ParseError))
	{
		OutError = FString::Printf(TEXT("%s: JSON parse error at byte %d: %s"), *SourceName, static_cast<int32>(ParseError.offset),
			*AbyssText::ToFString(ParseError.message));
		return false;
	}
	if (!Root.IsObject())
	{
		OutError = FString::Printf(TEXT("%s: root is not an object"), *SourceName);
		return false;
	}
	const abyss::JsonValue& Version = Root.Get("schemaVersion");
	if (Version.IsNumber() && Version.AsInt(1) > 1)
	{
		UE_LOG(LogAbyss, Warning, TEXT("%s: schemaVersion %d is newer than this build (1); unknown fields are ignored"),
			*SourceName, Version.AsInt(1));
	}

	for (const abyss::JsonMember& Member : Root.Get("palettes").Members())
	{
		FAbyssVfxPalette Palette;
		const abyss::JsonValue& P = Member.value;
		EAbyssVfxColor Ignored = EAbyssVfxColor::Fixed;
		ParseColor(P.Get("core"), Ignored, Palette.Core);
		ParseColor(P.Get("mid"), Ignored, Palette.Mid);
		ParseColor(P.Get("rim"), Ignored, Palette.Rim);
		ParseColor(P.Get("dark"), Ignored, Palette.Dark);
		Palettes.Add(ToName(Member.key), Palette);
	}

	for (const abyss::JsonMember& Member : Root.Get("sprites").Members())
	{
		const abyss::JsonValue& Cells = Member.value.Get("cells");
		if (Cells.IsNumber())
		{
			SpriteCells.Add(ToName(Member.key), FMath::Clamp(static_cast<int32>(Cells.AsDouble()), 1, 16));
		}
	}

	int32 Failures = 0;
	for (const abyss::JsonMember& Member : Root.Get("recipes").Members())
	{
		const FName Id = ToName(Member.key);
		FAbyssVfxRecipe Recipe;
		FString Error;
		if (Id.IsNone() || !ParseRecipe(Id, Member.value, Recipe, Error))
		{
			++Failures;
			UE_LOG(LogAbyss, Warning, TEXT("%s: recipe '%s' skipped: %s"), *SourceName, *AbyssText::ToFString(Member.key), *Error);
			continue;
		}
		Recipes.Add(Id, MoveTemp(Recipe));
	}
	// "aliases": { "<new id>": "<existing id>" } - resolved after this document's recipes, in document order.
	for (const abyss::JsonMember& Member : Root.Get("aliases").Members())
	{
		const FName AliasId = ToName(Member.key);
		const FName TargetId = ToName(Member.value.AsString());
		const FAbyssVfxRecipe* Target = Recipes.Find(TargetId);
		if (AliasId.IsNone() || Target == nullptr)
		{
			++Failures;
			UE_LOG(LogAbyss, Warning, TEXT("%s: alias '%s' -> '%s' skipped (unknown target)"), *SourceName,
				*AliasId.ToString(), *TargetId.ToString());
			continue;
		}
		FAbyssVfxRecipe Copy = *Target;
		Copy.Id = AliasId;
		Recipes.Add(AliasId, MoveTemp(Copy));
	}
	if (Failures > 0)
	{
		OutError = FString::Printf(TEXT("%s: %d recipe(s) skipped"), *SourceName, Failures);
	}
	return true;
}

bool FAbyssVfxLibrary::ParseRecipe(FName Id, const abyss::JsonValue& Json, FAbyssVfxRecipe& Out, FString& OutError) const
{
	using namespace AbyssVfxRecipePrivate;
	if (!Json.IsObject())
	{
		OutError = TEXT("not an object");
		return false;
	}
	Out.Id = Id;
	if (const FName Palette = ToName(Json.Get("palette").AsString()); !Palette.IsNone())
	{
		Out.Palette = Palette;
	}
	ReadFloat(Json, "arcCm", Out.ArcCm);
	const abyss::JsonValue& Light = Json.Get("light");
	if (Light.IsObject())
	{
		ReadFloat(Light, "radiusCm", Out.LightRadiusCm);
		ReadFloat(Light, "alpha", Out.LightAlpha);
		ReadBool(Light, "flicker", Out.bLightFlicker);
	}
	const abyss::JsonValue& Shake = Json.Get("shake");
	if (Shake.IsArray() && Shake.Size() >= 2)
	{
		Out.ShakeMs = static_cast<float>(Shake.At(0).AsDouble());
		Out.ShakeIntensity = static_cast<float>(Shake.At(1).AsDouble());
	}
	for (const abyss::JsonValue& LayerJson : Json.Get("layers").Items())
	{
		FAbyssVfxLayer Layer;
		FString LayerError;
		if (!ParseLayer(LayerJson, Layer, LayerError))
		{
			OutError = FString::Printf(TEXT("layer %d: %s"), Out.Layers.Num(), *LayerError);
			return false;
		}
		Out.Layers.Add(MoveTemp(Layer));
	}
	if (Out.Layers.Num() == 0 && Out.LightRadiusCm <= 0.f && Out.ShakeMs <= 0.f)
	{
		OutError = TEXT("no layers");
		return false;
	}
	return true;
}

bool FAbyssVfxLibrary::ParseLayer(const abyss::JsonValue& Json, FAbyssVfxLayer& L, FString& OutError) const
{
	using namespace AbyssVfxRecipePrivate;
	if (!Json.IsObject())
	{
		OutError = TEXT("layer is not an object");
		return false;
	}
	if (const abyss::JsonValue* Emitter = Json.Find("emitter"))
	{
		const FName EmitterName = ToName(Emitter->AsString());
		if (!ApplyPreset(EmitterName, L))
		{
			OutError = FString::Printf(TEXT("unknown emitter '%s'"), *EmitterName.ToString());
			return false;
		}
	}
	if (const FName Sprite = ToName(Json.Get("sprite").AsString()); !Sprite.IsNone())
	{
		L.Sprite = Sprite;
	}
	if (const abyss::JsonValue* Mesh = Json.Find("mesh"))
	{
		L.Mesh = ToName(Mesh->AsString());   // "" clears a preset mesh (debris drawn as sprites)
	}
	if (const abyss::JsonValue* Blend = Json.Find("blend"))
	{
		L.Blend = Blend->AsString() == std::string_view("translucent") || Blend->AsString() == std::string_view("normal")
			? EAbyssVfxBlend::Translucent
			: EAbyssVfxBlend::Additive;
	}
	if (const abyss::JsonValue* Orient = Json.Find("orient"); Orient != nullptr && !ParseOrient(Orient->AsString(), L.Orient))
	{
		OutError = TEXT("bad orient");
		return false;
	}
	ReadInt(Json, "variant", L.Variant);
	ReadInt(Json, "randomVariants", L.RandomVariants);

	if (const abyss::JsonValue* At = Json.Find("at"); At != nullptr && !ParseAnchor(At->AsString(), L.At))
	{
		OutError = TEXT("bad anchor `at`");
		return false;
	}
	if (const abyss::JsonValue* Height = Json.Find("height"); Height != nullptr && !ParseHeight(*Height, L.Height, L.HeightCm))
	{
		OutError = TEXT("bad height");
		return false;
	}
	ReadVector(Json.Get("offsetCm"), L.OffsetCm);
	if (const abyss::JsonValue* BeamTo = Json.Find("beamTo"); BeamTo != nullptr && !ParseAnchor(BeamTo->AsString(), L.BeamTo))
	{
		OutError = TEXT("bad beamTo");
		return false;
	}
	if (const abyss::JsonValue* BeamHeight = Json.Find("beamToHeight"))
	{
		float Unused = 0.f;
		ParseHeight(*BeamHeight, L.BeamToHeight, Unused);
	}
	ReadFloat(Json, "spawnRadiusCm", L.SpawnRadiusCm);
	ReadBool(Json, "onRing", L.bSpawnOnRing);
	ReadFloat(Json, "spawnHeightJitterCm", L.SpawnHeightJitterCm);
	if (const abyss::JsonValue* Unit = Json.Find("unit"); Unit != nullptr && !ParseUnit(Unit->AsString(), L.Unit))
	{
		OutError = TEXT("bad unit");
		return false;
	}
	ReadBool(Json, "attach", L.bAttach);

	ReadFloat(Json, "delayMs", L.DelayMs);
	if (const abyss::JsonValue* Count = Json.Find("count"))
	{
		if (Count->IsString())
		{
			L.bCountFromContext = Count->AsString() == std::string_view("sparks");
		}
		else
		{
			ReadRange(*Count, L.Count);
		}
	}
	ReadBool(Json, "countByRadius", L.bCountByRadius);
	ReadFloat(Json, "rate", L.Rate);
	ReadFloat(Json, "durationMs", L.DurationMs);
	ReadFloat(Json, "staggerMs", L.StaggerMs);
	ReadRangeField(Json, "lifeMs", L.LifeMs);

	ReadRangeField(Json, "speedCmS", L.SpeedCmS);
	if (const abyss::JsonValue* Dir = Json.Find("dir"); Dir != nullptr && !ParseDir(Dir->AsString(), L.Dir))
	{
		OutError = TEXT("bad dir");
		return false;
	}
	ReadFloat(Json, "coneDeg", L.ConeDeg);
	ReadRangeField(Json, "upCmS", L.UpCmS);
	ReadFloat(Json, "gravityCmS2", L.GravityCmS2);
	ReadFloat(Json, "drag", L.Drag);
	const abyss::JsonValue& Orbit = Json.Get("orbit");
	if (Orbit.IsObject())
	{
		L.bOrbit = true;
		ReadRangeField(Orbit, "r0Cm", L.OrbitRadius0Cm);
		ReadRangeField(Orbit, "r1Cm", L.OrbitRadius1Cm);
		ReadRangeField(Orbit, "degS", L.OrbitDegS);
	}
	else if (Orbit.IsBool())
	{
		L.bOrbit = Orbit.AsBool();
	}

	ReadRangeField(Json, "sizeCm", L.SizeCm);
	ReadFloat(Json, "grow", L.Grow);
	ReadFloat(Json, "sizePow", L.SizePow);
	ReadFloat(Json, "aspect", L.Aspect);
	ReadFloat(Json, "stretch", L.StretchPerSpeed);

	const abyss::JsonValue& Alpha = Json.Get("alpha");
	if (Alpha.IsArray() && Alpha.Size() >= 2)
	{
		L.Alpha0 = static_cast<float>(Alpha.At(0).AsDouble());
		L.Alpha1 = static_cast<float>(Alpha.At(1).AsDouble());
	}
	else if (Alpha.IsNumber())
	{
		L.Alpha0 = static_cast<float>(Alpha.AsDouble());
	}
	ReadFloat(Json, "fadeIn", L.FadeIn);
	ReadFloat(Json, "alphaPow", L.AlphaPow);
	ReadFloat(Json, "flicker", L.Flicker);
	if (const abyss::JsonValue* Color = Json.Find("color"); Color != nullptr && !ParseColor(*Color, L.ColorSource, L.Color))
	{
		OutError = TEXT("bad color");
		return false;
	}
	ReadInt(Json, "altEvery", L.AltEvery);
	if (const abyss::JsonValue* AltColor = Json.Find("altColor"); AltColor != nullptr
		&& !ParseColor(*AltColor, L.AltColorSource, L.AltColor))
	{
		OutError = TEXT("bad altColor");
		return false;
	}
	ReadFloat(Json, "intensity", L.Intensity);

	ReadRangeField(Json, "rotationDeg", L.RotationDeg);
	ReadRangeField(Json, "spinDegS", L.SpinDegS);
	ReadBool(Json, "alignToBlow", L.bAlignToBlow);
	if (const abyss::JsonValue* Only = Json.Find("only"))
	{
		L.BigFilter = Only->AsString() == std::string_view("big") ? 1 : (Only->AsString() == std::string_view("small") ? -1 : 0);
	}

	// Sanity (malformed data never produces runaway particles).
	L.Count.Min = FMath::Clamp(L.Count.Min, 0.f, 200.f);
	L.Count.Max = FMath::Clamp(L.Count.Max, L.Count.Min, 200.f);
	L.Rate = FMath::Clamp(L.Rate, 0.f, 400.f);
	L.LifeMs.Min = FMath::Max(1.f, L.LifeMs.Min);
	L.LifeMs.Max = FMath::Max(L.LifeMs.Min, L.LifeMs.Max);
	L.Aspect = FMath::Max(0.01f, L.Aspect);
	L.SizePow = FMath::Max(0.01f, L.SizePow);
	L.AlphaPow = FMath::Max(0.01f, L.AlphaPow);
	L.FadeIn = FMath::Clamp(L.FadeIn, 0.f, 0.95f);
	return true;
}

const FAbyssVfxRecipe* FAbyssVfxLibrary::Find(FName Id) const
{
	return Recipes.Find(Id);
}

const FAbyssVfxRecipe* FAbyssVfxLibrary::FindFirst(TConstArrayView<FName> Ids) const
{
	for (const FName Id : Ids)
	{
		if (const FAbyssVfxRecipe* Recipe = Recipes.Find(Id))
		{
			return Recipe;
		}
	}
	return nullptr;
}

const FAbyssVfxPalette* FAbyssVfxLibrary::FindPalette(FName Name) const
{
	return Palettes.Find(Name);
}

const FAbyssVfxPalette& FAbyssVfxLibrary::ElementPalette(abyss::DamageType Element) const
{
	const TCHAR* Name = TEXT("steel");
	switch (Element)
	{
	case abyss::DamageType::Fire: Name = TEXT("fire"); break;
	case abyss::DamageType::Ice: Name = TEXT("frost"); break;
	case abyss::DamageType::Lightning: Name = TEXT("lightning"); break;
	case abyss::DamageType::Poison: Name = TEXT("poison"); break;
	case abyss::DamageType::Arcane: Name = TEXT("arcane"); break;
	case abyss::DamageType::Physical: Name = TEXT("steel"); break;
	}
	const FAbyssVfxPalette* Found = Palettes.Find(FName(Name));
	return Found != nullptr ? *Found : FallbackPalette;
}

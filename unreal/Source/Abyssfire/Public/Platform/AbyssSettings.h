// Device-level user settings (DECISIONS U9; save-ui-input.md 3.8 / 9; ue58-platform.md 10.5): one settings.json next to
// the save slots. Character-level toggles (auto-combat, auto-loot, difficulty) stay in the save (core).
// The file is read / written with the core's JSON (abyss/base/Json.h); unknown or malformed fields fall back to their
// defaults one by one (web parity: each localStorage field was validated by type).
#pragma once

#include "CoreMinimal.h"

#include <string>
#include <string_view>

#include "abyss/base/Enums.h"

/** Graphics quality as chosen by the user (U9). Auto resolves with AbyssPlatform::ResolveQualityTier. */
enum class EAbyssQualitySetting : uint8
{
	Auto,
	Low,
	Balanced,
	High,
};

/** The resolved render tier (ue58-platform.md 6.8: web RenderQuality low / balanced / high). */
enum class EAbyssQualityTier : uint8
{
	Low,
	Balanced,
	High,
};

/** Control layout (save-ui-input.md 5.7.1): Auto = touch on iOS / Android, keyboard + mouse elsewhere. */
enum class EAbyssControlLayout : uint8
{
	Auto,
	Desktop,
	Touch,
};

struct ABYSSFIRE_API FAbyssUserSettings
{
	/** UI language; default zh-CN regardless of the OS language (web parity, src/i18n/index.ts:14-15). */
	abyss::LocaleId Locale = abyss::LocaleId::ZhCN;
	/** Volumes 0..1 (A3 defaults: music 0.6, SFX 0.8). */
	float MasterVolume = 1.0f;
	float MusicVolume = 0.6f;
	float SfxVolume = 0.8f;
	EAbyssQualitySetting Quality = EAbyssQualitySetting::Auto;
	EAbyssControlLayout ControlLayout = EAbyssControlLayout::Auto;
	/** Touch controls: size multiplier (0.75..1.5) and opacity (0.3..1). */
	float TouchControlScale = 1.0f;
	float TouchControlOpacity = 1.0f;
	bool bCameraShake = true;
	bool bDamageNumbers = true;

	/** Clamps every field into its valid range. */
	void Sanitize();

	std::string ToJson() const;
	/** Parses settings.json. Returns false only when the text is not a JSON object (Out is then left at defaults). */
	static bool FromJson(std::string_view Json, FAbyssUserSettings& Out);

	bool operator==(const FAbyssUserSettings& Other) const = default;
};

ABYSSFIRE_API const TCHAR* LexToString(EAbyssQualitySetting Value);
ABYSSFIRE_API const TCHAR* LexToString(EAbyssQualityTier Value);
ABYSSFIRE_API const TCHAR* LexToString(EAbyssControlLayout Value);

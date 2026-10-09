#include "Platform/AbyssSettings.h"

#include "abyss/base/Json.h"

namespace
{
	// settings.json schema version (bump when a field changes meaning).
	constexpr int32 AbyssSettings_SettingsVersion = 1;

	const char* AbyssSettings_QualityName(EAbyssQualitySetting Value)
	{
		switch (Value)
		{
		case EAbyssQualitySetting::Auto: return "auto";
		case EAbyssQualitySetting::Low: return "low";
		case EAbyssQualitySetting::Balanced: return "balanced";
		case EAbyssQualitySetting::High: return "high";
		}
		return "auto";
	}

	bool AbyssSettings_ParseQuality(std::string_view Text, EAbyssQualitySetting& Out)
	{
		if (Text == "auto") { Out = EAbyssQualitySetting::Auto; return true; }
		if (Text == "low") { Out = EAbyssQualitySetting::Low; return true; }
		if (Text == "balanced" || Text == "mid") { Out = EAbyssQualitySetting::Balanced; return true; }
		if (Text == "high") { Out = EAbyssQualitySetting::High; return true; }
		return false;
	}

	const char* AbyssSettings_LayoutName(EAbyssControlLayout Value)
	{
		switch (Value)
		{
		case EAbyssControlLayout::Auto: return "auto";
		case EAbyssControlLayout::Desktop: return "desktop";
		case EAbyssControlLayout::Touch: return "touch";
		}
		return "auto";
	}

	bool AbyssSettings_ParseLayout(std::string_view Text, EAbyssControlLayout& Out)
	{
		if (Text == "auto") { Out = EAbyssControlLayout::Auto; return true; }
		if (Text == "desktop") { Out = EAbyssControlLayout::Desktop; return true; }
		if (Text == "touch") { Out = EAbyssControlLayout::Touch; return true; }
		return false;
	}

	// Floats are written rounded to 3 decimals so settings.json stays readable (0.6, not 0.6000000238418579).
	double AbyssSettings_Rounded(float Value)
	{
		return FMath::RoundToDouble(static_cast<double>(Value) * 1000.0) / 1000.0;
	}

	void AbyssSettings_ReadUnit(const abyss::JsonValue& Root, std::string_view Key, float& InOut)
	{
		const abyss::JsonValue* Value = Root.Find(Key);
		if (Value != nullptr && Value->IsNumber())
		{
			InOut = static_cast<float>(Value->AsDouble(InOut));
		}
	}

	void AbyssSettings_ReadBool(const abyss::JsonValue& Root, std::string_view Key, bool& InOut)
	{
		const abyss::JsonValue* Value = Root.Find(Key);
		if (Value != nullptr && Value->IsBool())
		{
			InOut = Value->AsBool(InOut);
		}
	}
}

void FAbyssUserSettings::Sanitize()
{
	MasterVolume = FMath::Clamp(MasterVolume, 0.0f, 1.0f);
	MusicVolume = FMath::Clamp(MusicVolume, 0.0f, 1.0f);
	SfxVolume = FMath::Clamp(SfxVolume, 0.0f, 1.0f);
	TouchControlScale = FMath::Clamp(TouchControlScale, 0.75f, 1.5f);
	TouchControlOpacity = FMath::Clamp(TouchControlOpacity, 0.3f, 1.0f);
	if (static_cast<size_t>(Locale) >= abyss::EnumCount<abyss::LocaleId>())
	{
		Locale = abyss::LocaleId::ZhCN;
	}
}

std::string FAbyssUserSettings::ToJson() const
{
	abyss::JsonWriter Writer(/*pretty*/ true);
	Writer.StartObject();
	Writer.Key("version");
	Writer.Int(AbyssSettings_SettingsVersion);
	Writer.Key("locale");
	Writer.String(abyss::EnumName(Locale));
	Writer.Key("audio");
	Writer.StartObject();
	Writer.Key("master");
	Writer.Double(AbyssSettings_Rounded(MasterVolume));
	Writer.Key("music");
	Writer.Double(AbyssSettings_Rounded(MusicVolume));
	Writer.Key("sfx");
	Writer.Double(AbyssSettings_Rounded(SfxVolume));
	Writer.EndObject();
	Writer.Key("renderQuality");
	Writer.String(AbyssSettings_QualityName(Quality));
	Writer.Key("controlLayout");
	Writer.String(AbyssSettings_LayoutName(ControlLayout));
	Writer.Key("touchControlScale");
	Writer.Double(AbyssSettings_Rounded(TouchControlScale));
	Writer.Key("touchControlOpacity");
	Writer.Double(AbyssSettings_Rounded(TouchControlOpacity));
	Writer.Key("cameraShake");
	Writer.Bool(bCameraShake);
	Writer.Key("damageNumbers");
	Writer.Bool(bDamageNumbers);
	Writer.EndObject();
	return Writer.Take();
}

bool FAbyssUserSettings::FromJson(std::string_view Json, FAbyssUserSettings& Out)
{
	Out = FAbyssUserSettings();
	abyss::JsonValue Root;
	if (!abyss::ParseJson(Json, Root) || !Root.IsObject())
	{
		return false;
	}

	if (const abyss::JsonValue* LocaleValue = Root.Find("locale"); LocaleValue != nullptr && LocaleValue->IsString())
	{
		abyss::LocaleId Parsed = abyss::LocaleId::ZhCN;
		if (abyss::ParseEnum(LocaleValue->AsString(), Parsed))
		{
			Out.Locale = Parsed;
		}
	}
	if (const abyss::JsonValue* Audio = Root.Find("audio"); Audio != nullptr && Audio->IsObject())
	{
		AbyssSettings_ReadUnit(*Audio, "master", Out.MasterVolume);
		AbyssSettings_ReadUnit(*Audio, "music", Out.MusicVolume);
		AbyssSettings_ReadUnit(*Audio, "sfx", Out.SfxVolume);
	}
	if (const abyss::JsonValue* QualityValue = Root.Find("renderQuality"); QualityValue != nullptr && QualityValue->IsString())
	{
		EAbyssQualitySetting Parsed = EAbyssQualitySetting::Auto;
		if (AbyssSettings_ParseQuality(QualityValue->AsString(), Parsed))
		{
			Out.Quality = Parsed;
		}
	}
	if (const abyss::JsonValue* LayoutValue = Root.Find("controlLayout"); LayoutValue != nullptr && LayoutValue->IsString())
	{
		EAbyssControlLayout Parsed = EAbyssControlLayout::Auto;
		if (AbyssSettings_ParseLayout(LayoutValue->AsString(), Parsed))
		{
			Out.ControlLayout = Parsed;
		}
	}
	AbyssSettings_ReadUnit(Root, "touchControlScale", Out.TouchControlScale);
	AbyssSettings_ReadUnit(Root, "touchControlOpacity", Out.TouchControlOpacity);
	AbyssSettings_ReadBool(Root, "cameraShake", Out.bCameraShake);
	AbyssSettings_ReadBool(Root, "damageNumbers", Out.bDamageNumbers);
	Out.Sanitize();
	return true;
}

const TCHAR* LexToString(EAbyssQualitySetting Value)
{
	switch (Value)
	{
	case EAbyssQualitySetting::Auto: return TEXT("Auto");
	case EAbyssQualitySetting::Low: return TEXT("Low");
	case EAbyssQualitySetting::Balanced: return TEXT("Balanced");
	case EAbyssQualitySetting::High: return TEXT("High");
	}
	return TEXT("Auto");
}

const TCHAR* LexToString(EAbyssQualityTier Value)
{
	switch (Value)
	{
	case EAbyssQualityTier::Low: return TEXT("Low");
	case EAbyssQualityTier::Balanced: return TEXT("Balanced");
	case EAbyssQualityTier::High: return TEXT("High");
	}
	return TEXT("Balanced");
}

const TCHAR* LexToString(EAbyssControlLayout Value)
{
	switch (Value)
	{
	case EAbyssControlLayout::Auto: return TEXT("Auto");
	case EAbyssControlLayout::Desktop: return TEXT("Desktop");
	case EAbyssControlLayout::Touch: return TEXT("Touch");
	}
	return TEXT("Auto");
}

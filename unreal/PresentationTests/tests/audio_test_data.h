// Shared inputs of the U7 audio rule tests (audio_*_test.cpp): the committed audio manifest
// (unreal/Audio/Export/audio_manifest.json) and the real exported data (unreal/Data), each loaded once per process.
#pragma once

#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

#include "abyss/base/Json.h"
#include "abyss/base/Log.h"
#include "abyss/data/DataStore.h"

namespace audio_test
{
	inline std::string ReadBytes(const std::string& Path)
	{
		std::ifstream In(Path, std::ios::binary);
		if (!In)
		{
			return {};
		}
		std::ostringstream Ss;
		Ss << In.rdbuf();
		return Ss.str();
	}

	inline std::string UnrealDir() { return ABYSS_UNREAL_DIR; }

	/** audio_manifest.json as committed (Null when it cannot be read or parsed). */
	inline const abyss::JsonValue& Manifest()
	{
		static abyss::JsonValue Root;
		static bool bLoaded = false;
		if (!bLoaded)
		{
			bLoaded = true;
			const std::string Text = ReadBytes(UnrealDir() + "/Audio/Export/audio_manifest.json");
			abyss::JsonParseError Error;
			if (Text.empty() || !abyss::ParseJson(Text, Root, &Error))
			{
				abyss::Log(abyss::LogLevel::Error, "audio_manifest.json: " + Error.message);
				Root = abyss::JsonValue();
			}
		}
		return Root;
	}

	/** The real exported tables (unreal/Data). */
	inline const abyss::DataStore& Data()
	{
		static abyss::DataStore Store;
		static bool bLoaded = false;
		if (!bLoaded)
		{
			bLoaded = true;
			abyss::DataLoadReport Report;
			const bool bOk = Store.LoadAll(
				[](std::string_view Name, std::string& Out)
				{
					Out = ReadBytes(UnrealDir() + "/Data/" + std::string(Name));
					return !Out.empty();
				},
				Report);
			if (!bOk)
			{
				abyss::Log(abyss::LogLevel::Error, Report.Summary());
			}
		}
		return Store;
	}

	/** Parses a JSON literal of a synthetic table (Null on error). */
	inline abyss::JsonValue Json(std::string_view Text)
	{
		abyss::JsonValue Out;
		if (!abyss::ParseJson(Text, Out))
		{
			return abyss::JsonValue();
		}
		return Out;
	}
}

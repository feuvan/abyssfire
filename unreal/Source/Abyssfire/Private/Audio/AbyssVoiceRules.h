// AbyssVoiceRules: which voice a monster speaks with (U7; manifest `voiceFamilies`, AU1 dark_vox.py).
//
// * Family of a monster: `monsters[defId]` (the Chapter 1 roster), then the design family through `aliases` (the
//   monster's `family` field once the data exports it; withheld in W1, so the glue passes ""), then the monster's
//   animCategory (today's rule: humanoid / slime and the later chapters' rigs). A `silent` monster has no voice: no
//   family cue (monster_aggro / monster_hurt) and no death vocal; the base monster_death groan still plays.
// * Named cues per monster (`overrides`): `monster_aggro` / `monster_hurt` replace the family cue, `monster_death.vocal`
//   replaces the family death vocal, `anim.<action>` plays when the core starts that AnimAction on the monster,
//   `miniboss.opened` when its pre-fight dialogue opens, `story.say` when a cutscene `say` step has it as speaker.
// * ResolveDefVoice bundles a def's answers (family, silent, the three EvSfx named cues, whether an anim cue exists) so
//   UAbyssAudioSystem resolves a def once at spawn and its per-event SFX / anim path does no table scan or string work.
//
// Pure helper (std + AbyssCore public headers only); tested in PresentationTests/tests/audio_voice_test.cpp.
#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "abyss/base/Json.h"

#include "Audio/AbyssAudioDataRules.h"

namespace AbyssVoiceRules
{
	inline constexpr std::string_view KeyAggro = "monster_aggro";
	inline constexpr std::string_view KeyHurt = "monster_hurt";
	inline constexpr std::string_view KeyDeathVocal = "monster_death.vocal";
	inline constexpr std::string_view KeyMiniBossOpened = "miniboss.opened";
	inline constexpr std::string_view KeyStorySay = "story.say";
	inline constexpr std::string_view AnimKeyPrefix = "anim.";

	/** "anim.<action>" for a core AnimAction name (EnumName: "roar", "attack", ...). */
	inline std::string AnimKey(std::string_view ActionName)
	{
		std::string Key(AnimKeyPrefix);
		Key.append(ActionName);
		return Key;
	}

	struct NamedCue
	{
		std::string monster;
		std::string key;
		std::string cue;
	};

	struct VoiceTables
	{
		std::vector<std::string> families;
		std::vector<std::pair<std::string, std::string>> aliases;   // design family -> voice family
		std::vector<std::pair<std::string, std::string>> monsters;  // monster id -> voice family
		std::vector<std::string> silent;
		std::vector<NamedCue> overrides;
		bool loaded = false;
	};

	inline VoiceTables Parse(const abyss::JsonValue& VoiceFamilies)
	{
		VoiceTables Out;
		if (!VoiceFamilies.IsObject())
		{
			return Out;
		}
		Out.families = AbyssAudioData::Strings(VoiceFamilies.Get("families"));
		for (const abyss::JsonMember& Row : VoiceFamilies.Get("aliases").Members())
		{
			if (Row.value.IsString())
			{
				Out.aliases.emplace_back(Row.key, std::string(Row.value.AsString()));
			}
		}
		for (const abyss::JsonMember& Row : VoiceFamilies.Get("monsters").Members())
		{
			if (Row.value.IsString())
			{
				Out.monsters.emplace_back(Row.key, std::string(Row.value.AsString()));
			}
		}
		for (const abyss::JsonMember& Row : VoiceFamilies.Get("silent").Members())
		{
			Out.silent.push_back(Row.key);
		}
		for (const abyss::JsonMember& Monster : VoiceFamilies.Get("overrides").Members())
		{
			for (const abyss::JsonMember& Row : Monster.value.Members())
			{
				if (Row.value.IsString() && !Row.value.AsString().empty())
				{
					Out.overrides.push_back(NamedCue{ Monster.key, Row.key, std::string(Row.value.AsString()) });
				}
			}
		}
		Out.loaded = true;
		return Out;
	}

	enum class FamilySource : unsigned char
	{
		None,
		Silent,
		Monster,
		Alias,
		AnimCategory,
	};

	struct Voice
	{
		std::string family;  // "" for a silent monster (or nothing known)
		FamilySource source = FamilySource::None;
		bool IsSilent() const { return source == FamilySource::Silent; }
	};

	inline const std::string* FindPair(const std::vector<std::pair<std::string, std::string>>& Pairs, std::string_view Key)
	{
		for (const std::pair<std::string, std::string>& Pair : Pairs)
		{
			if (Pair.first == Key)
			{
				return &Pair.second;
			}
		}
		return nullptr;
	}

	/** The voice of a monster def (see the header comment for the order). */
	inline Voice ResolveFamily(const VoiceTables& Tables, std::string_view DefId, std::string_view DesignFamily,
		std::string_view AnimCategory)
	{
		Voice Out;
		if (!DefId.empty() && AbyssAudioData::Contains(Tables.silent, DefId))
		{
			Out.source = FamilySource::Silent;
			return Out;
		}
		if (const std::string* Family = DefId.empty() ? nullptr : FindPair(Tables.monsters, DefId))
		{
			Out.family = *Family;
			Out.source = FamilySource::Monster;
			return Out;
		}
		if (const std::string* Family = DesignFamily.empty() ? nullptr : FindPair(Tables.aliases, DesignFamily))
		{
			Out.family = *Family;
			Out.source = FamilySource::Alias;
			return Out;
		}
		if (!DesignFamily.empty() && AbyssAudioData::Contains(Tables.families, DesignFamily))
		{
			Out.family = std::string(DesignFamily);  // the design family is already a voice family
			Out.source = FamilySource::Alias;
			return Out;
		}
		if (!AnimCategory.empty())
		{
			Out.family = std::string(AnimCategory);
			Out.source = FamilySource::AnimCategory;
		}
		return Out;
	}

	/** The named cue for `Key` on that monster def ("" = none: play the family cue / nothing). */
	inline std::string_view OverrideCue(const VoiceTables& Tables, std::string_view DefId, std::string_view Key)
	{
		if (DefId.empty() || Key.empty())
		{
			return {};
		}
		for (const NamedCue& Row : Tables.overrides)
		{
			if (Row.monster == DefId && Row.key == Key)
			{
				return Row.cue;
			}
		}
		return {};
	}

	/** Whether the def has any `anim.<action>` named cue (the glue skips every other monster's EvPlayAnim at once). */
	inline bool HasAnimCue(const VoiceTables& Tables, std::string_view DefId)
	{
		if (DefId.empty())
		{
			return false;
		}
		for (const NamedCue& Row : Tables.overrides)
		{
			if (Row.monster == DefId && Row.key.compare(0, AnimKeyPrefix.size(), AnimKeyPrefix) == 0)
			{
				return true;
			}
		}
		return false;
	}

	/**
	 * Everything the per-event SFX path needs about one monster def, resolved once (the glue caches it per def at
	 * spawn): its voice (family / silent), the named cues of the three EvSfx beats ("" = the family cue) and whether an
	 * `anim.<action>` cue exists. Same answers as ResolveFamily / OverrideCue / HasAnimCue one by one.
	 */
	struct DefVoice
	{
		Voice voice;
		std::string aggroCue;
		std::string hurtCue;
		std::string deathVocalCue;
		bool hasAnimCue = false;
	};

	inline DefVoice ResolveDefVoice(const VoiceTables& Tables, std::string_view DefId, std::string_view DesignFamily,
		std::string_view AnimCategory)
	{
		DefVoice Out;
		Out.voice = ResolveFamily(Tables, DefId, DesignFamily, AnimCategory);
		Out.aggroCue = std::string(OverrideCue(Tables, DefId, KeyAggro));
		Out.hurtCue = std::string(OverrideCue(Tables, DefId, KeyHurt));
		Out.deathVocalCue = std::string(OverrideCue(Tables, DefId, KeyDeathVocal));
		Out.hasAnimCue = HasAnimCue(Tables, DefId);
		return Out;
	}

	/** The override key an EvSfx cue id stands for: monster_aggro / monster_hurt, monster_death -> its vocal layer. */
	inline std::string_view SfxOverrideKey(std::string_view CueId)
	{
		if (CueId == KeyAggro)
		{
			return KeyAggro;
		}
		if (CueId == KeyHurt)
		{
			return KeyHurt;
		}
		if (CueId == "monster_death")
		{
			return KeyDeathVocal;
		}
		return {};
	}
}

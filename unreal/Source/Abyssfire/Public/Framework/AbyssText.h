// Text bridge between the core (UTF-8 std::string, i18n keys + args) and UE (FString / FText), ue58-platform.md 9.7.
// UE's localisation pipeline is not used for game text: the core's I18n tables (Data/i18n_<locale>.json) are the only
// source of player-facing strings, so every FText built here is culture-invariant.
#pragma once

#include "CoreMinimal.h"

#include <string>
#include <string_view>

#include "abyss/base/I18n.h"

namespace AbyssText
{
	/** UTF-8 (core) -> FString. */
	ABYSSFIRE_API FString ToFString(std::string_view Utf8);
	/** FString -> UTF-8 (core). */
	ABYSSFIRE_API std::string ToStd(const FString& Text);
	/** UTF-8 -> culture-invariant FText (already-resolved text, e.g. a save slot's zone name). */
	ABYSSFIRE_API FText ToText(std::string_view Utf8);

	/** Resolves a core LocText (key + args, args that are keys resolved too) with the given tables. */
	ABYSSFIRE_API FText Localize(const abyss::I18n& Strings, const abyss::LocText& Text);
	/** Resolves a plain key (no args). Returns the key itself when it is missing (web t() behaviour). */
	ABYSSFIRE_API FText Localize(const abyss::I18n& Strings, std::string_view Key);
	ABYSSFIRE_API FString LocalizeString(const abyss::I18n& Strings, const abyss::LocText& Text);
	ABYSSFIRE_API FString LocalizeString(const abyss::I18n& Strings, std::string_view Key);
}

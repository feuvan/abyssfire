#include "Framework/AbyssText.h"

#include "Containers/StringConv.h"
#include "Internationalization/Text.h"

namespace AbyssText
{
	FString ToFString(std::string_view Utf8)
	{
		if (Utf8.empty())
		{
			return FString();
		}
		// UTF8CHAR overload converts the encoding (UE 5.4+; replaces the deprecated FString(int32, const char*)).
		return FString::ConstructFromPtrSize(reinterpret_cast<const UTF8CHAR*>(Utf8.data()), static_cast<int32>(Utf8.size()));
	}

	std::string ToStd(const FString& Text)
	{
		if (Text.IsEmpty())
		{
			return std::string();
		}
		const auto Converted = StringCast<UTF8CHAR>(*Text, Text.Len());
		return std::string(reinterpret_cast<const char*>(Converted.Get()), static_cast<size_t>(Converted.Length()));
	}

	FText ToText(std::string_view Utf8)
	{
		return FText::AsCultureInvariant(ToFString(Utf8));
	}

	FString LocalizeString(const abyss::I18n& Strings, const abyss::LocText& Text)
	{
		return ToFString(Strings.T(Text));
	}

	FString LocalizeString(const abyss::I18n& Strings, std::string_view Key)
	{
		return ToFString(Strings.T(Key));
	}

	FText Localize(const abyss::I18n& Strings, const abyss::LocText& Text)
	{
		return FText::AsCultureInvariant(LocalizeString(Strings, Text));
	}

	FText Localize(const abyss::I18n& Strings, std::string_view Key)
	{
		return FText::AsCultureInvariant(LocalizeString(Strings, Key));
	}
}

// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Internationalization/Internationalization.h (from UE 5.x source knowledge).
#pragma once

#include "Internationalization/Text.h"

class FInternationalization
{
public:
	static FInternationalization& Get();
	static bool IsAvailable();
	bool SetCurrentCulture(const FString& InCultureName);
	FCultureRef GetCurrentCulture() const;
	bool SetCurrentLanguage(const FString& InCultureName);
	FCultureRef GetCurrentLanguage() const;
	bool SetCurrentLanguageAndLocale(const FString& InCultureName);
	FCulturePtr GetCulture(const FString& InCultureName);
	FCultureRef GetDefaultCulture() const;
	FCultureRef GetInvariantCulture() const;
};

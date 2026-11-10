// Stub SDK. Real header: Engine/Source/Runtime/Core/Public/Internationalization/Culture.h (from UE 5.x source knowledge).
#pragma once

#include "Internationalization/CulturePointer.h"

class FCulture
{
public:
	const FString& GetDisplayName() const;
	const FString& GetEnglishName() const;
	FString GetName() const;
	const FString& GetNativeName() const;
	FString GetTwoLetterISOLanguageName() const;
	FString GetThreeLetterISOLanguageName() const;
	FString GetNativeLanguage() const;
	FString GetRegion() const;
	FString GetScript() const;
};

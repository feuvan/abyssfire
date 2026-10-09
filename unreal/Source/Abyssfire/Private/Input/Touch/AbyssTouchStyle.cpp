#include "Input/Touch/AbyssTouchStyle.h"

#include "Fonts/CompositeFont.h"
#include "Misc/Char.h"
#include "Misc/Paths.h"

#include <string_view>

#include "abyss/data/UiData.h"

#include "Framework/AbyssText.h"

namespace
{
	/** The bundled bold body face, built once and intentionally never destroyed (the Slate font cache keys on it). */
	const TSharedPtr<const FCompositeFont>& AbyssTouchStyle_FallbackComposite()
	{
		static const TSharedPtr<const FCompositeFont>* Composite = nullptr;
		if (Composite == nullptr)
		{
			const TSharedRef<FCompositeFont> Font = MakeShared<FCompositeFont>();
			const FString FontDir = FPaths::ProjectDir() / TEXT("Fonts");
			Font->DefaultTypeface.AppendFont(TEXT("Bold"), FontDir / TEXT("NotoSansSC-Bold.otf"), EFontHinting::Default,
				EFontLoadingPolicy::LazyLoad);
			Composite = new TSharedPtr<const FCompositeFont>(Font);
		}
		return *Composite;
	}

	bool AbyssTouchStyle_IsHexDigits(const FString& Digits)
	{
		for (const TCHAR Character : Digits)
		{
			if (!FChar::IsHexDigit(Character))
			{
				return false;
			}
		}
		return true;
	}
}

namespace AbyssTouchStyle
{
	FLinearColor FromRgb(uint32 Rgb, float Alpha)
	{
		const FColor Srgb(static_cast<uint8>((Rgb >> 16) & 0xff), static_cast<uint8>((Rgb >> 8) & 0xff), static_cast<uint8>(Rgb & 0xff), 255);
		FLinearColor Linear(Srgb);
		Linear.A = Alpha;
		return Linear;
	}

	FLinearColor FromHex(const FString& Hex, const FLinearColor& Fallback)
	{
		FString Digits = Hex;
		Digits.RemoveFromStart(TEXT("#"));
		if (Digits.Len() != 6 || !AbyssTouchStyle_IsHexDigits(Digits))
		{
			return Fallback;
		}
		return FLinearColor(FColor::FromHex(Digits));
	}

	FLinearColor ThemeColor(const abyss::UiThemeDef* Theme, const char* Name, const FLinearColor& Fallback)
	{
		if (Theme == nullptr || Name == nullptr)
		{
			return Fallback;
		}
		const std::string_view Wanted(Name);
		for (const std::pair<std::string, std::string>& Entry : Theme->colors)
		{
			if (Entry.first == Wanted)
			{
				return FromHex(AbyssText::ToFString(Entry.second), Fallback);
			}
		}
		return Fallback;
	}

	FSlateFontInfo FallbackFont(float SizePx)
	{
		FSlateFontInfo Font(AbyssTouchStyle_FallbackComposite(), FMath::Max(1.0f, SizePx * 0.75f), TEXT("Bold"));
		Font.OutlineSettings.OutlineSize = 1;
		Font.OutlineSettings.OutlineColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.85f);
		return Font;
	}

	FSlateFontInfo Resized(const FSlateFontInfo& Font, float SizePx)
	{
		FSlateFontInfo Copy = Font;
		Copy.Size = FMath::Max(1.0f, SizePx * 0.75f);
		return Copy;
	}
}

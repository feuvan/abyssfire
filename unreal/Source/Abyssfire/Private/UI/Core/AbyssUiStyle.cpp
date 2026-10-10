#include "UI/AbyssUiStyle.h"

#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateImageBrush.h"
#include "Brushes/SlateNoResource.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Engine/Texture.h"
#include "Fonts/CompositeFont.h"
#include "HAL/FileManager.h"
#include "Misc/Char.h"
#include "Misc/Paths.h"

#include <string>
#include <string_view>
#include <utility>

#include "abyss/data/UiData.h"

#include "Abyssfire.h"
#include "Framework/AbyssText.h"

namespace
{
	/** One family set per script (0 = Simplified, 1 = Traditional). Never destroyed: the Slate font cache keys on them. */
	struct FAbyssUiFontFamilies
	{
		TSharedPtr<const FCompositeFont> Body;
		TSharedPtr<const FCompositeFont> Title;
		TSharedPtr<const FCompositeFont> Serif;
	};

	FAbyssUiFontFamilies* GAbyssUiFontFamilies[2] = { nullptr, nullptr };

	FString AbyssUiStyle_FontPath(const TCHAR* FileName)
	{
		return FPaths::ProjectDir() / TEXT("Fonts") / FileName;
	}

	void AbyssUiStyle_AppendPair(FTypeface& Typeface, const TCHAR* RegularFile, const TCHAR* BoldFile)
	{
		Typeface.AppendFont(TEXT("Regular"), AbyssUiStyle_FontPath(RegularFile), EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
		Typeface.AppendFont(TEXT("Bold"), AbyssUiStyle_FontPath(BoldFile), EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
	}

	/** Code points the Latin title face (Cinzel) does not cover: punctuation / symbols, CJK, full-width forms. */
	void AbyssUiStyle_AddCjkAndSymbolRanges(FCompositeSubFont& SubFont)
	{
		SubFont.CharacterRanges.Add(FInt32Range::Inclusive(0x2000, 0x2BFF));   // punctuation, arrows, shapes, symbols
		SubFont.CharacterRanges.Add(FInt32Range::Inclusive(0x2E80, 0x9FFF));   // CJK radicals .. unified ideographs
		SubFont.CharacterRanges.Add(FInt32Range::Inclusive(0xF900, 0xFAFF));   // CJK compatibility ideographs
		SubFont.CharacterRanges.Add(FInt32Range::Inclusive(0xFE30, 0xFE4F));   // CJK compatibility forms
		SubFont.CharacterRanges.Add(FInt32Range::Inclusive(0xFF00, 0xFFEF));   // half / full-width forms
		SubFont.CharacterRanges.Add(FInt32Range::Inclusive(0x20000, 0x2FFFF)); // CJK extension planes
	}

	/** Logs an error for every bundled font file that is missing (FreeType would silently render nothing / tofu). */
	void AbyssUiStyle_VerifyFontFiles()
	{
		static bool bVerified = false;
		if (bVerified)
		{
			return;
		}
		bVerified = true;
		static const TCHAR* const Files[] = {
			TEXT("NotoSansSC-Regular.otf"), TEXT("NotoSansSC-Bold.otf"), TEXT("NotoSansTC-Regular.otf"), TEXT("NotoSansTC-Bold.otf"),
			TEXT("NotoSerifSC-Regular.otf"), TEXT("NotoSerifSC-Bold.otf"), TEXT("NotoSerifTC-Regular.otf"), TEXT("NotoSerifTC-Bold.otf"),
			TEXT("Cinzel-Regular.ttf"), TEXT("Cinzel-Bold.ttf"),
		};
		for (const TCHAR* File : Files)
		{
			const FString Path = AbyssUiStyle_FontPath(File);
			if (!IFileManager::Get().FileExists(*Path))
			{
				UE_LOG(LogAbyss, Error, TEXT("UI font missing: %s (staged from unreal/Fonts; run Scripts/fonts/build_fonts.py). Text in this face will not render."),
					*Path);
			}
		}
	}

	const FAbyssUiFontFamilies& AbyssUiStyle_Families(bool bTraditional)
	{
		const int32 Index = bTraditional ? 1 : 0;
		if (GAbyssUiFontFamilies[Index] == nullptr)
		{
			AbyssUiStyle_VerifyFontFiles();
			FAbyssUiFontFamilies* Families = new FAbyssUiFontFamilies();

			// Body: the script's Noto Sans; the other script's face fills missing glyphs (per-glyph fallback).
			{
				const TSharedRef<FCompositeFont> Body = MakeShared<FCompositeFont>();
				if (bTraditional)
				{
					AbyssUiStyle_AppendPair(Body->DefaultTypeface, TEXT("NotoSansTC-Regular.otf"), TEXT("NotoSansTC-Bold.otf"));
					AbyssUiStyle_AppendPair(Body->FallbackTypeface.Typeface, TEXT("NotoSansSC-Regular.otf"), TEXT("NotoSansSC-Bold.otf"));
				}
				else
				{
					AbyssUiStyle_AppendPair(Body->DefaultTypeface, TEXT("NotoSansSC-Regular.otf"), TEXT("NotoSansSC-Bold.otf"));
					AbyssUiStyle_AppendPair(Body->FallbackTypeface.Typeface, TEXT("NotoSansTC-Regular.otf"), TEXT("NotoSansTC-Bold.otf"));
				}
				Families->Body = Body;
			}
			// Title: Cinzel for Latin, Noto Sans for CJK and symbols (UI_TITLE_FONT "Cinzel", "Noto Sans SC").
			{
				const TSharedRef<FCompositeFont> Title = MakeShared<FCompositeFont>();
				AbyssUiStyle_AppendPair(Title->DefaultTypeface, TEXT("Cinzel-Regular.ttf"), TEXT("Cinzel-Bold.ttf"));
				FCompositeSubFont Cjk;
				if (bTraditional)
				{
					AbyssUiStyle_AppendPair(Cjk.Typeface, TEXT("NotoSansTC-Regular.otf"), TEXT("NotoSansTC-Bold.otf"));
				}
				else
				{
					AbyssUiStyle_AppendPair(Cjk.Typeface, TEXT("NotoSansSC-Regular.otf"), TEXT("NotoSansSC-Bold.otf"));
				}
				AbyssUiStyle_AddCjkAndSymbolRanges(Cjk);
				Title->SubTypefaces.Add(MoveTemp(Cjk));
				AbyssUiStyle_AppendPair(Title->FallbackTypeface.Typeface, TEXT("NotoSansSC-Regular.otf"), TEXT("NotoSansSC-Bold.otf"));
				Families->Title = Title;
			}
			// Serif: story text and the boss bar.
			{
				const TSharedRef<FCompositeFont> Serif = MakeShared<FCompositeFont>();
				if (bTraditional)
				{
					AbyssUiStyle_AppendPair(Serif->DefaultTypeface, TEXT("NotoSerifTC-Regular.otf"), TEXT("NotoSerifTC-Bold.otf"));
					AbyssUiStyle_AppendPair(Serif->FallbackTypeface.Typeface, TEXT("NotoSerifSC-Regular.otf"), TEXT("NotoSerifSC-Bold.otf"));
				}
				else
				{
					AbyssUiStyle_AppendPair(Serif->DefaultTypeface, TEXT("NotoSerifSC-Regular.otf"), TEXT("NotoSerifSC-Bold.otf"));
					AbyssUiStyle_AppendPair(Serif->FallbackTypeface.Typeface, TEXT("NotoSansSC-Regular.otf"), TEXT("NotoSansSC-Bold.otf"));
				}
				Families->Serif = Serif;
			}
			GAbyssUiFontFamilies[Index] = Families;
		}
		return *GAbyssUiFontFamilies[Index];
	}

	FLinearColor AbyssUiStyle_ThemeColor(const abyss::UiThemeDef* Theme, std::string_view Name, uint32 FallbackRgb)
	{
		const FLinearColor Fallback = FAbyssUiStyle::Rgb(FallbackRgb);
		if (Theme == nullptr)
		{
			return Fallback;
		}
		for (const std::pair<std::string, std::string>& Entry : Theme->colors)
		{
			if (Entry.first == Name)
			{
				return FAbyssUiStyle::Hex(AbyssText::ToFString(Entry.second), Fallback);
			}
		}
		for (const std::pair<std::string, uint32_t>& Entry : Theme->colorNums)
		{
			if (Entry.first == Name)
			{
				return FAbyssUiStyle::Rgb(Entry.second);
			}
		}
		return Fallback;
	}
}

FAbyssUiStyle::FAbyssUiStyle()
{
	WhiteBrush = FSlateColorBrush(FLinearColor::White);
	NoBrush = FSlateNoResource();
	Initialize(nullptr);
}

FAbyssUiStyle::~FAbyssUiStyle() = default;

FLinearColor FAbyssUiStyle::Rgb(uint32 Value, float Alpha)
{
	FLinearColor Linear(FColor(static_cast<uint8>((Value >> 16) & 0xff), static_cast<uint8>((Value >> 8) & 0xff),
		static_cast<uint8>(Value & 0xff), 255));
	Linear.A = Alpha;
	return Linear;
}

FLinearColor FAbyssUiStyle::Hex(const FString& InHex, const FLinearColor& Fallback)
{
	FString Digits = InHex.TrimStartAndEnd();
	Digits.RemoveFromStart(TEXT("#"));
	if (Digits.Len() != 6)
	{
		return Fallback;
	}
	for (const TCHAR Character : Digits)
	{
		if (!FChar::IsHexDigit(Character))
		{
			return Fallback;
		}
	}
	return FLinearColor(FColor::FromHex(Digits));
}

FLinearColor FAbyssUiStyle::Lighten(const FLinearColor& Color, float Amount)
{
	const FColor Srgb = Color.ToFColor(true);
	const auto Up = [Amount](uint8 Channel) { return static_cast<uint8>(FMath::Clamp(Channel + (255 - Channel) * Amount, 0.f, 255.f)); };
	FLinearColor Out(FColor(Up(Srgb.R), Up(Srgb.G), Up(Srgb.B), 255));
	Out.A = Color.A;
	return Out;
}

FLinearColor FAbyssUiStyle::Darken(const FLinearColor& Color, float Amount)
{
	const FColor Srgb = Color.ToFColor(true);
	const auto Down = [Amount](uint8 Channel) { return static_cast<uint8>(FMath::Clamp(Channel * (1.f - Amount), 0.f, 255.f)); };
	FLinearColor Out(FColor(Down(Srgb.R), Down(Srgb.G), Down(Srgb.B), 255));
	Out.A = Color.A;
	return Out;
}

FLinearColor FAbyssUiStyle::WithAlpha(const FLinearColor& Color, float Alpha)
{
	FLinearColor Out = Color;
	Out.A = Alpha;
	return Out;
}

void FAbyssUiStyle::Initialize(const abyss::UiThemeDef* Theme)
{
	Palette.Parchment = AbyssUiStyle_ThemeColor(Theme, "parchment", 0xf0dcae);
	Palette.Heading = AbyssUiStyle_ThemeColor(Theme, "heading", 0xe8c77a);
	Palette.Gold = AbyssUiStyle_ThemeColor(Theme, "gold", 0xd4a54a);
	Palette.GoldBright = AbyssUiStyle_ThemeColor(Theme, "goldBright", 0xffd98a);
	Palette.Text = AbyssUiStyle_ThemeColor(Theme, "text", 0xe0d8cc);
	Palette.TextSoft = AbyssUiStyle_ThemeColor(Theme, "textSoft", 0xbfb4a2);
	Palette.Muted = AbyssUiStyle_ThemeColor(Theme, "muted", 0x9a8f80);
	Palette.Dim = AbyssUiStyle_ThemeColor(Theme, "dim", 0x6e665c);
	Palette.Faint = AbyssUiStyle_ThemeColor(Theme, "faint", 0x4f4940);
	Palette.Good = AbyssUiStyle_ThemeColor(Theme, "good", 0x7ed36a);
	Palette.Bad = AbyssUiStyle_ThemeColor(Theme, "bad", 0xff6b5a);
	Palette.Info = AbyssUiStyle_ThemeColor(Theme, "info", 0x7fb6ff);
	Palette.GoldDark = AbyssUiStyle_ThemeColor(Theme, "goldDarkNum", 0x5a3a10);
	Palette.Iron = AbyssUiStyle_ThemeColor(Theme, "ironNum", 0x3a3540);
	Palette.IronLight = AbyssUiStyle_ThemeColor(Theme, "ironLightNum", 0x6d6573);
	Palette.Card = AbyssUiStyle_ThemeColor(Theme, "cardNum", 0x18151b);
	Palette.CardHover = AbyssUiStyle_ThemeColor(Theme, "cardHoverNum", 0x221d25);
	Palette.Well = AbyssUiStyle_ThemeColor(Theme, "wellNum", 0x0c0b0e);
	Palette.Ink = Rgb(0x120b04);
	Palette.Backdrop = FLinearColor(0.f, 0.f, 0.f, 0.6f);

	static constexpr uint32 QualityFallback[5] = { 0xc8c8c8, 0x4f8cff, 0xffd84a, 0xff8a2a, 0x3ecf6a };
	for (int32 Index = 0; Index < 5; ++Index)
	{
		FLinearColor Color = Rgb(QualityFallback[Index]);
		if (Theme != nullptr && static_cast<size_t>(Index) < Theme->qualityHex.size() && !Theme->qualityHex[static_cast<size_t>(Index)].empty())
		{
			Color = Hex(AbyssText::ToFString(Theme->qualityHex[static_cast<size_t>(Index)]), Color);
		}
		Palette.Quality[Index] = Color;
	}

	// save-ui-input 8.4 button variants: top / bottom gradient, border, label, hover label.
	Buttons[static_cast<int32>(EAbyssButtonKind::Primary)] = { Rgb(0x6b4a1e), Rgb(0x2c1d0b), Rgb(0xd4a54a), Rgb(0xffe7b0), Rgb(0xfff4d6) };
	Buttons[static_cast<int32>(EAbyssButtonKind::Secondary)] = { Rgb(0x3a353f), Rgb(0x17151a), Rgb(0x8a7a64), Rgb(0xe0d8cc), Rgb(0xfff4e0) };
	Buttons[static_cast<int32>(EAbyssButtonKind::Danger)] = { Rgb(0x5a1c16), Rgb(0x220a08), Rgb(0xc0503c), Rgb(0xffb8a6), Rgb(0xffe0d6) };
	Buttons[static_cast<int32>(EAbyssButtonKind::Success)] = { Rgb(0x27451f), Rgb(0x0e1b0c), Rgb(0x6fb35a), Rgb(0xc6f0b4), Rgb(0xeaffe0) };
	Buttons[static_cast<int32>(EAbyssButtonKind::Ghost)] = { Rgb(0x1d1a20), Rgb(0x141216), Rgb(0x4d4552), Rgb(0xbfb4a2), Rgb(0xf0dcae) };

	BuildStyles();
}

void FAbyssUiStyle::BuildStyles()
{
	InvisibleButtonStyle = FButtonStyle()
		.SetNormal(NoBrush)
		.SetHovered(NoBrush)
		.SetPressed(NoBrush)
		.SetDisabled(NoBrush)
		.SetNormalPadding(FMargin(0.f))
		.SetPressedPadding(FMargin(0.f));

	const FSlateBrush Track = FSlateRoundedBoxBrush(Rgb(0x0c0b0e, 0.85f), 3.f);
	const FSlateBrush Thumb = FSlateRoundedBoxBrush(Rgb(0x6d6573), 3.f);
	const FSlateBrush ThumbHover = FSlateRoundedBoxBrush(Rgb(0xd4a54a), 3.f);
	ScrollBarStyle = FScrollBarStyle::GetDefault();
	ScrollBarStyle
		.SetVerticalBackgroundImage(Track)
		.SetHorizontalBackgroundImage(Track)
		.SetVerticalTopSlotImage(NoBrush)
		.SetVerticalBottomSlotImage(NoBrush)
		.SetHorizontalTopSlotImage(NoBrush)
		.SetHorizontalBottomSlotImage(NoBrush)
		.SetNormalThumbImage(Thumb)
		.SetHoveredThumbImage(ThumbHover)
		.SetDraggedThumbImage(ThumbHover);
}

void FAbyssUiStyle::SetSolidTexture(UTexture* Texture)
{
	if (Texture == nullptr)
	{
		SolidTextureBrush = FSlateBrush();
		bHasSolidTexture = false;
		return;
	}
	SolidTextureBrush = FSlateImageBrush(Texture, FVector2D(1.0, 1.0));
	bHasSolidTexture = true;
}

void FAbyssUiStyle::SetLocale(abyss::LocaleId InLocale)
{
	Locale = InLocale;
}

FSlateFontInfo FAbyssUiStyle::Font(EAbyssFontFace Face, float Px, bool bBold, int32 OutlinePx, const FLinearColor& OutlineColor) const
{
	const FAbyssUiFontFamilies& Families = AbyssUiStyle_Families(Locale == abyss::LocaleId::ZhTW);
	const TSharedPtr<const FCompositeFont>* Composite = &Families.Body;
	switch (Face)
	{
	case EAbyssFontFace::Body: Composite = &Families.Body; break;
	case EAbyssFontFace::Title: Composite = &Families.Title; break;
	case EAbyssFontFace::Serif: Composite = &Families.Serif; break;
	}
	FFontOutlineSettings Outline;
	if (OutlinePx > 0)
	{
		Outline.OutlineSize = OutlinePx;
		Outline.OutlineColor = OutlineColor;
	}
	// ue58-platform.md 9.2: a web N px font is Slate size N * 0.75 (points at 96 DPI).
	return FSlateFontInfo(*Composite, FMath::Max(1.f, Px * 0.75f), bBold ? FName(TEXT("Bold")) : FName(TEXT("Regular")), Outline);
}

FLinearColor FAbyssUiStyle::QualityColor(abyss::ItemQuality Quality) const
{
	const int32 Index = static_cast<int32>(Quality);
	return Index >= 0 && Index < 5 ? Palette.Quality[Index] : Palette.Quality[0];
}

const FAbyssButtonColors& FAbyssUiStyle::ButtonColors(EAbyssButtonKind Kind) const
{
	const int32 Index = static_cast<int32>(Kind);
	return Buttons[Index >= 0 && Index < 5 ? Index : 1];
}

const FSlateBrush* FAbyssUiStyle::Rounded(const FLinearColor& Fill, float Radius, const FLinearColor& Outline, float OutlineWidth) const
{
	FBrushKey Key;
	Key.Fill = Fill.ToFColor(false);
	Key.Outline = Outline.ToFColor(false);
	Key.RadiusTenths = FMath::RoundToInt(Radius * 10.f);
	Key.WidthTenths = FMath::RoundToInt(OutlineWidth * 10.f);
	Key.bRounded = true;
	if (const TUniquePtr<FSlateBrush>* Found = BrushCache.Find(Key))
	{
		return Found->Get();
	}
	TUniquePtr<FSlateBrush> Brush = OutlineWidth > 0.f
		? MakeUnique<FSlateBrush>(FSlateRoundedBoxBrush(Fill, Radius, Outline, OutlineWidth))
		: MakeUnique<FSlateBrush>(FSlateRoundedBoxBrush(Fill, Radius));
	const FSlateBrush* Result = Brush.Get();
	BrushCache.Add(Key, MoveTemp(Brush));
	return Result;
}

const FSlateBrush* FAbyssUiStyle::Solid(const FLinearColor& Fill) const
{
	FBrushKey Key;
	Key.Fill = Fill.ToFColor(false);
	Key.bRounded = false;
	if (const TUniquePtr<FSlateBrush>* Found = BrushCache.Find(Key))
	{
		return Found->Get();
	}
	TUniquePtr<FSlateBrush> Brush = MakeUnique<FSlateBrush>(FSlateColorBrush(Fill));
	const FSlateBrush* Result = Brush.Get();
	BrushCache.Add(Key, MoveTemp(Brush));
	return Result;
}

// FAbyssUiStyle: the UiKit look in Slate (save-ui-input.md 8, ue58-platform.md 9.2 / 9.4).
//
// * Fonts: the bundled OFL fonts (unreal/Fonts, staged UFS) loaded at runtime as composite fonts - body = Noto Sans SC,
//   titles = Cinzel with Noto Sans for CJK / symbols, story + boss bar = Noto Serif SC. For zh-TW the CJK faces switch to
//   the TC fonts with the SC fonts as the per-glyph fallback (the exported zh-TW table still contains simplified-only
//   characters, ARCHITECTURE "Fonts"). Composite fonts are created once and never destroyed (the Slate font cache keys on
//   them). Sizes are given in web logical px; Slate font size = px * 0.75 (ue58-platform.md 9.2).
// * Palette: the UI_COLORS tokens from Data/ui_theme.json (fallbacks = the web values), item quality colours.
// * Brushes: a cache of rounded-box / flat brushes with stable addresses (Slate keeps brush pointers until render), the
//   invisible button style used by every SAbyssButton, the scroll bar style.
//
// Game thread only. Owned by UAbyssUiSubsystem (one per game instance).
#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "Templates/UniquePtr.h"

#include "abyss/base/Enums.h"

namespace abyss
{
	struct UiThemeDef;
}

/** Font families of the UiKit. */
enum class EAbyssFontFace : uint8
{
	Body,    // Noto Sans SC / TC
	Title,   // Cinzel (Latin) + Noto Sans (CJK, symbols)
	Serif,   // Noto Serif SC / TC (story text, boss bar)
};

/** UiKit button variants (save-ui-input.md 8.4). */
enum class EAbyssButtonKind : uint8
{
	Primary,
	Secondary,
	Danger,
	Success,
	Ghost,
};

/** The UI_COLORS palette (save-ui-input.md 8.1) as linear colours. */
struct FAbyssUiPalette
{
	FLinearColor Parchment;
	FLinearColor Heading;
	FLinearColor Gold;
	FLinearColor GoldBright;
	FLinearColor Text;
	FLinearColor TextSoft;
	FLinearColor Muted;
	FLinearColor Dim;
	FLinearColor Faint;
	FLinearColor Good;
	FLinearColor Bad;
	FLinearColor Info;
	FLinearColor GoldDark;
	FLinearColor Iron;
	FLinearColor IronLight;
	FLinearColor Card;
	FLinearColor CardHover;
	FLinearColor Well;
	FLinearColor Ink;          // #120b04 title stroke
	FLinearColor Backdrop;     // modal dim
	FLinearColor Quality[5];   // normal, magic, rare, legendary, set
};

/** Colours of one button variant (save-ui-input.md 8.4). */
struct FAbyssButtonColors
{
	FLinearColor Top;
	FLinearColor Bottom;
	FLinearColor Border;
	FLinearColor Label;
	FLinearColor LabelHover;
};

class ABYSSFIRE_API FAbyssUiStyle
{
public:
	FAbyssUiStyle();
	~FAbyssUiStyle();
	FAbyssUiStyle(const FAbyssUiStyle&) = delete;
	FAbyssUiStyle& operator=(const FAbyssUiStyle&) = delete;

	/** Reads the theme colours (nullptr = the web defaults) and builds the fonts / styles. */
	void Initialize(const abyss::UiThemeDef* Theme);
	/** zh-TW switches the CJK faces to the Traditional fonts (SC kept as the glyph fallback). */
	void SetLocale(abyss::LocaleId Locale);
	abyss::LocaleId GetLocale() const { return Locale; }

	// ---- fonts ----
	/** Px = web logical px (Slate size = Px * 0.75). OutlinePx draws the web's black text stroke. */
	FSlateFontInfo Font(EAbyssFontFace Face, float Px, bool bBold = false, int32 OutlinePx = 0,
		const FLinearColor& OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.9f)) const;
	FSlateFontInfo Body(float Px, bool bBold = false, int32 OutlinePx = 0) const { return Font(EAbyssFontFace::Body, Px, bBold, OutlinePx); }
	FSlateFontInfo Title(float Px, bool bBold = false, int32 OutlinePx = 0) const { return Font(EAbyssFontFace::Title, Px, bBold, OutlinePx); }
	FSlateFontInfo Serif(float Px, bool bBold = false, int32 OutlinePx = 0) const { return Font(EAbyssFontFace::Serif, Px, bBold, OutlinePx); }

	// ---- colours ----
	const FAbyssUiPalette& Colors() const { return Palette; }
	FLinearColor QualityColor(abyss::ItemQuality Quality) const;
	const FAbyssButtonColors& ButtonColors(EAbyssButtonKind Kind) const;
	/** 0xRRGGBB (sRGB) -> linear. */
	static FLinearColor Rgb(uint32 Rgb, float Alpha = 1.0f);
	/** "#rrggbb" (sRGB) -> linear; Fallback when malformed. */
	static FLinearColor Hex(const FString& Hex, const FLinearColor& Fallback);
	/** Lighter / darker in sRGB space (UiKit lighten / darken, Amount 0..1). */
	static FLinearColor Lighten(const FLinearColor& Color, float Amount);
	static FLinearColor Darken(const FLinearColor& Color, float Amount);
	static FLinearColor WithAlpha(const FLinearColor& Color, float Alpha);

	// ---- brushes (stable addresses for the lifetime of the style) ----
	const FSlateBrush* White() const { return &WhiteBrush; }
	const FSlateBrush* None() const { return &NoBrush; }
	/** Rounded box (Radius px; Radius >= half the size draws a circle / pill). Cached by value. */
	const FSlateBrush* Rounded(const FLinearColor& Fill, float Radius, const FLinearColor& Outline = FLinearColor::Transparent,
		float OutlineWidth = 0.f) const;
	/** Flat colour box (cached). */
	const FSlateBrush* Solid(const FLinearColor& Fill) const;

	/** SButton style without visuals (SAbyssButton paints its own face). */
	const FButtonStyle& InvisibleButton() const { return InvisibleButtonStyle; }
	const FScrollBarStyle& ScrollBar() const { return ScrollBarStyle; }

private:
	struct FBrushKey
	{
		FColor Fill;
		FColor Outline;
		int32 RadiusTenths = 0;
		int32 WidthTenths = 0;
		bool bRounded = true;

		bool operator==(const FBrushKey& Other) const
		{
			return Fill == Other.Fill && Outline == Other.Outline && RadiusTenths == Other.RadiusTenths
				&& WidthTenths == Other.WidthTenths && bRounded == Other.bRounded;
		}
		friend uint32 GetTypeHash(const FBrushKey& Key)
		{
			uint32 Hash = HashCombine(GetTypeHash(Key.Fill.ToPackedARGB()), GetTypeHash(Key.Outline.ToPackedARGB()));
			Hash = HashCombine(Hash, GetTypeHash(Key.RadiusTenths));
			Hash = HashCombine(Hash, GetTypeHash(Key.WidthTenths));
			return HashCombine(Hash, GetTypeHash(Key.bRounded));
		}
	};

	void BuildStyles();

	FAbyssUiPalette Palette;
	FAbyssButtonColors Buttons[5];
	abyss::LocaleId Locale = abyss::LocaleId::ZhCN;
	FSlateBrush WhiteBrush;
	FSlateBrush NoBrush;
	FButtonStyle InvisibleButtonStyle;
	FScrollBarStyle ScrollBarStyle;
	mutable TMap<FBrushKey, TUniquePtr<FSlateBrush>> BrushCache;
};

// Visual inputs of the touch layer: what the hosting HUD supplies (icons, CJK font) and the web control colours
// (MobileControlsSystem.ts / save-ui-input.md 5.7.3, UiKit palette via Data/ui_theme.json).
#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Templates/Function.h"

struct FSlateBrush;

namespace abyss
{
	struct UiThemeDef;
}

/**
 * Supplied by the HUD that hosts SAbyssTouchControls (all optional). Brushes must outlive the touch layer (keep them in
 * the UI style set). Without icons the buttons draw their localized labels; without a font the layer loads the bundled
 * Noto Sans SC Bold (Fonts/, ue58-platform.md 9.4).
 *
 * HudIcon ids: "medallion" (round button face), "lock", "dodge", "potion_hp", "potion_mp", "portal", "auto", "loot",
 * "log", "menu", "inventory", "character", "skills", "map", "pets", "quest", "talk", "pickup", "open", "use".
 */
struct FAbyssTouchVisuals
{
	/** Skill icon for a skill id (classes data id, e.g. "fireball"); nullptr = first two letters of its name. */
	TFunction<const FSlateBrush*(const FString& /*SkillId*/)> SkillIcon;
	/** HUD glyph by id (see above); nullptr = text only. */
	TFunction<const FSlateBrush*(FName /*IconId*/)> HudIcon;
	/** CJK-capable bold UI font; any size (the buttons resize it). Invalid = bundled fallback. */
	FSlateFontInfo Font;
};

namespace AbyssTouchStyle
{
	/** 0xRRGGBB (sRGB) -> linear colour. */
	ABYSSFIRE_API FLinearColor FromRgb(uint32 Rgb, float Alpha = 1.0f);
	/** "#rrggbb" (sRGB) -> linear colour; Fallback when malformed. */
	ABYSSFIRE_API FLinearColor FromHex(const FString& Hex, const FLinearColor& Fallback);
	/** A named UiKit colour from ui_theme.json ("parchment", "gold", ...), else Fallback. */
	ABYSSFIRE_API FLinearColor ThemeColor(const abyss::UiThemeDef* Theme, const char* Name, const FLinearColor& Fallback);

	/** Bundled Noto Sans SC Bold at SizePx Slate units (font size = px * 0.75, ue58-platform.md 9.2). */
	ABYSSFIRE_API FSlateFontInfo FallbackFont(float SizePx);
	/** Font with a new pixel size (keeps face / outline). */
	ABYSSFIRE_API FSlateFontInfo Resized(const FSlateFontInfo& Font, float SizePx);

	// ---- web control colours (sRGB 0xRRGGBB) ----
	inline constexpr uint32 CornerFill = 0x6a2a24;     // LOCK medallion
	inline constexpr uint32 DodgeFill = 0x1f4a6a;
	inline constexpr uint32 SkillFill = 0x2a2430;
	inline constexpr uint32 PotionHpFill = 0x6a1c1c;   // port (I4): HP / MP potion medallions
	inline constexpr uint32 PotionMpFill = 0x1c2c6a;
	inline constexpr uint32 InteractFill = 0x5a4718;   // port (Q6): gold Talk / Use button
	inline constexpr uint32 PortalRing = 0x4488ff;     // world 7.4: portal swirl colour
	inline constexpr uint32 SecondaryFill = 0x1d1a20;  // toggle frames (secondary, alpha 0.9)
	inline constexpr uint32 GhostFill = 0x141218;      // panel squares (ghost, alpha 0.88)
	inline constexpr uint32 Gold = 0xd4a54a;           // UiKit goldNum
	inline constexpr uint32 GoldBright = 0xffd98a;
	inline constexpr uint32 Parchment = 0xf0dcae;
	inline constexpr uint32 ToggleOn = 0x8ff07a;       // auto-combat on
	inline constexpr uint32 ToggleOff = 0xb0a8b4;      // auto-combat / auto-loot off
	inline constexpr uint32 AutoLootAll = 0xe0d8cc;
}

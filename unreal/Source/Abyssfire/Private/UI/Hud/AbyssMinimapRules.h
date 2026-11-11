// The minimap's tile colours (UI/Hud/AbyssMinimapTexture.cpp: the HUD minimap and the world-map panel share the texture).
//
// The texture used the web minimap's fixed colours for every zone, so the Red March (presentation theme `march`, a grey
// ash field) was drawn in the plains' green: the first Mac run's screenshots show a green disc on a dark screen. A zone
// with a presentation theme now takes its palette from that theme's terrain style (terrain_styles.json
// themes.<theme>.ground.<tile>.base, the colours the 3D ground is painted with), so every themed zone, the Pit included
// (theme `pit`; a theme without a style of its own falls back along its parents, pit -> march), gets its own. A zone
// without a presentation theme (every web zone, the tower, labyrinth floors) keeps the web palette: that is the parity
// the minimap spec asks for (save-ui-input 6.11).
//
// Walls and the camp palisade have no ground style. A wall is the theme's stone darkened by the web's ratio (0x4a /
// 0x6a), the palisade is the style's palisade wood; both are then capped below the darkest walkable ground so that
// "can I walk there" stays readable in a theme whose ground colours sit close together.
//
// Two more rules keep a themed palette readable as a map (S-A review R3: the march's bases are greys 6 luma levels
// apart, so the camp, the tan landmark of the web palette, was lost in the field):
//   * the camp ground is the brightest walkable colour, at least CampOverGround x the brightest of grass / dirt /
//     stone and at least CampOverGroundMin levels above it (its own hue, lifted);
//   * water that does not stand out by hue (channel spread under WaterHueSpread, the march's grey pools) is capped like
//     the other blocked tiles; a coloured water (the plains' blue, the abyss's lava) keeps its base.
//
// UE-free and header-only (std + AbyssCore's JSON), tested in unreal/PresentationTests/tests/minimap_test.cpp.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "abyss/base/Json.h"

namespace AbyssMinimapRules
{
	/** Tile types the minimap colours (abyss::TileType: 0 grass, 1 dirt, 2 stone, 3 water, 4 wall, 5 camp, 6 camp wall). */
	inline constexpr int TileKinds = 7;
	inline constexpr int TileGrass = 0;
	inline constexpr int TileDirt = 1;
	inline constexpr int TileStone = 2;
	inline constexpr int TileWater = 3;
	inline constexpr int TileWall = 4;
	inline constexpr int TileCamp = 5;
	inline constexpr int TileCampWall = 6;

	struct FPalette
	{
		/** 0xRRGGBB per tile type. */
		std::array<std::uint32_t, TileKinds> Tile{};
		/** A tile type outside the table. */
		std::uint32_t Unknown = 0x222222;
		/** The terrain style the colours came from; empty = the web palette. */
		std::string Style;
	};

	/** The web minimap's colours (layer 1 of the web HUD minimap): every zone without a presentation theme. */
	inline FPalette WebPalette()
	{
		FPalette P;
		P.Tile = { 0x4a8c3f, 0x8b7355, 0x6a6a6a, 0x1a5276, 0x4a4a4a, 0x9e7c52, 0x5c4834 };
		return P;
	}

	/** The web's wall-to-stone brightness ratio (0x4a / 0x6a). */
	inline constexpr double WallOfStone = 0.70;
	/** A blocked tile (wall, palisade) is at most this fraction of the darkest walkable ground's brightness. */
	inline constexpr double BlockedBelowGround = 0.72;

	inline constexpr int R(std::uint32_t Rgb) { return static_cast<int>((Rgb >> 16) & 0xff); }
	inline constexpr int G(std::uint32_t Rgb) { return static_cast<int>((Rgb >> 8) & 0xff); }
	inline constexpr int B(std::uint32_t Rgb) { return static_cast<int>(Rgb & 0xff); }

	/** Rec. 601 luma of the 8-bit channels, 0..255. */
	inline constexpr double Luma(std::uint32_t Rgb)
	{
		return 0.299 * R(Rgb) + 0.587 * G(Rgb) + 0.114 * B(Rgb);
	}

	/** Every channel times K (rounded, clamped to 8 bits). */
	inline constexpr std::uint32_t Scale(std::uint32_t Rgb, double K)
	{
		const auto Channel = [K](int V)
		{
			const double Scaled = V * K + 0.5;
			return static_cast<std::uint32_t>(Scaled < 0.0 ? 0.0 : (Scaled > 255.0 ? 255.0 : Scaled));
		};
		return (Channel(R(Rgb)) << 16) | (Channel(G(Rgb)) << 8) | Channel(B(Rgb));
	}

	/** Rgb darkened (never brightened) until its luma is at most MaxLuma. */
	inline constexpr std::uint32_t CapLuma(std::uint32_t Rgb, double MaxLuma)
	{
		const double L = Luma(Rgb);
		return L > MaxLuma && L > 0.0 ? Scale(Rgb, MaxLuma / L) : Rgb;
	}

	/** The camp ground is at least this times the brightest other walkable ground's brightness. */
	inline constexpr double CampOverGround = 1.25;
	/** ... and at least this many brightness levels (0..255) above it: on a dark style (the Pit) the ratio alone is
	 *  too small a step to see. */
	inline constexpr double CampOverGroundMin = 24.0;
	/** Water whose channels are closer together than this (0..255) has no hue to be told by: it is capped as blocked. */
	inline constexpr int WaterHueSpread = 48;

	/** Largest minus smallest channel, 0..255: how far a colour is from grey. */
	inline constexpr int HueSpread(std::uint32_t Rgb)
	{
		const int Max = R(Rgb) > G(Rgb) ? (R(Rgb) > B(Rgb) ? R(Rgb) : B(Rgb)) : (G(Rgb) > B(Rgb) ? G(Rgb) : B(Rgb));
		const int Min = R(Rgb) < G(Rgb) ? (R(Rgb) < B(Rgb) ? R(Rgb) : B(Rgb)) : (G(Rgb) < B(Rgb) ? G(Rgb) : B(Rgb));
		return Max - Min;
	}

	/** Rgb brightened (never darkened) until its luma is at least MinLuma, as far as 8 bits allow (channels clamp at
	 *  255: a colour near white ends a little short); black becomes the grey of that luma. */
	inline constexpr std::uint32_t LiftLuma(std::uint32_t Rgb, double MinLuma)
	{
		const double L = Luma(Rgb);
		if (L >= MinLuma)
		{
			return Rgb;
		}
		return L > 0.0 ? Scale(Rgb, MinLuma / L) : Scale(0x010101, MinLuma);
	}

	/** A 0xRRGGBB number of the style table; false when the value is not one. */
	inline bool ReadRgb(const abyss::JsonValue& Value, std::uint32_t& Out)
	{
		if (!Value.IsNumber())
		{
			return false;
		}
		const std::int64_t Raw = Value.AsInt64(-1);
		if (Raw < 0 || Raw > 0xffffff)
		{
			return false;
		}
		Out = static_cast<std::uint32_t>(Raw);
		return true;
	}

	/** The parent of a presentation theme: the theme whose art and style it shows where it has none of its own
	 *  (implementation-plan 3C: `pit` -> `march`). Empty for a theme without one. */
	inline std::string_view ParentTheme(std::string_view Theme)
	{
		if (Theme == "pit" || Theme == "pit_cleansed")
		{
			return "march";
		}
		return {};
	}

	/** The styles to try for a presentation theme, nearest first: the theme, then its parents. */
	inline std::vector<std::string> StyleChain(std::string_view PresentationTheme)
	{
		std::vector<std::string> Out;
		for (std::string_view Theme = PresentationTheme; !Theme.empty() && Out.size() < 4; Theme = ParentTheme(Theme))
		{
			Out.emplace_back(Theme);
		}
		return Out;
	}

	/**
	 * The palette of a zone. `PresentationTheme` empty (or no style table, or no style along the chain): the web
	 * palette. Otherwise the first style of StyleChain that has a ground table: its base colours, a ground type it does
	 * not list keeps the web colour, the camp lifted, a hueless water capped and walls and the palisade derived as
	 * described at the top.
	 */
	inline FPalette PaletteFor(const abyss::JsonValue* Styles, std::string_view PresentationTheme)
	{
		FPalette P = WebPalette();
		if (Styles == nullptr || PresentationTheme.empty())
		{
			return P;
		}
		for (const std::string& Name : StyleChain(PresentationTheme))
		{
			const abyss::JsonValue& Style = Styles->Get("themes").Get(Name);
			const abyss::JsonValue& Ground = Style.Get("ground");
			if (!Ground.IsObject())
			{
				continue;
			}
			static constexpr std::array<std::pair<int, std::string_view>, 5> GroundKinds = { {
				{ TileGrass, "0" }, { TileDirt, "1" }, { TileStone, "2" }, { TileWater, "3" }, { TileCamp, "5" } } };
			int Read = 0;
			for (const std::pair<int, std::string_view>& Kind : GroundKinds)
			{
				std::uint32_t Rgb = 0;
				if (ReadRgb(Ground.Get(Kind.second).Get("base"), Rgb))
				{
					P.Tile[static_cast<size_t>(Kind.first)] = Rgb;
					++Read;
				}
			}
			if (Read == 0)
			{
				continue;   // a ground table without one usable colour: not a style
			}
			double BrightestGround = 0.0;
			for (const int Walkable : { TileGrass, TileDirt, TileStone })
			{
				const double L = Luma(P.Tile[static_cast<size_t>(Walkable)]);
				BrightestGround = L > BrightestGround ? L : BrightestGround;
			}
			const double CampByRatio = BrightestGround * CampOverGround;
			const double CampByStep = BrightestGround + CampOverGroundMin;
			P.Tile[TileCamp] = LiftLuma(P.Tile[TileCamp], CampByRatio > CampByStep ? CampByRatio : CampByStep);
			double DarkestWalkable = 255.0;
			for (const int Walkable : { TileGrass, TileDirt, TileStone, TileCamp })
			{
				const double L = Luma(P.Tile[static_cast<size_t>(Walkable)]);
				DarkestWalkable = L < DarkestWalkable ? L : DarkestWalkable;
			}
			const double BlockedMax = DarkestWalkable * BlockedBelowGround;
			if (HueSpread(P.Tile[TileWater]) < WaterHueSpread)
			{
				P.Tile[TileWater] = CapLuma(P.Tile[TileWater], BlockedMax);
			}
			P.Tile[TileWall] = CapLuma(Scale(P.Tile[TileStone], WallOfStone), BlockedMax);
			std::uint32_t Wood = 0;
			P.Tile[TileCampWall] = CapLuma(ReadRgb(Style.Get("palisade").Get("wood"), Wood) ? Wood : P.Tile[TileWall], BlockedMax);
			P.Unknown = CapLuma(P.Unknown, BlockedMax);
			P.Style = Name;
			return P;
		}
		return P;
	}

	inline constexpr std::uint32_t ColorOf(const FPalette& P, int TileType)
	{
		return TileType >= 0 && TileType < TileKinds ? P.Tile[static_cast<size_t>(TileType)] : P.Unknown;
	}
}

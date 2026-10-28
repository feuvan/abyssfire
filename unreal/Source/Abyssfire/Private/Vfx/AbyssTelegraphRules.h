// The enemy telegraph layer's presentation rules (dark remake U2, implementation-plan.md 4 U2 / 9 2G; DECISIONS DR5,
// DR16). What UAbyssTelegraphSystem (Private/Vfx/AbyssTelegraphs.cpp) draws for the Wave 2 core's EvTelegraph /
// EvHazard / EvArena (and their snapshot views) and how:
//
// * The M_AF_Telegraph contract (manifest `materials.M_AF_Telegraph`, Art/blender/vfx/fx_recipes_dark.py TELEGRAPH):
//   parsed once (ParseContract; every field defaults to the contract as shipped), per style (normal / elite / boss for
//   strikes and fuses, hazard for lingering zones, arena for a boss ring) the rim width / alpha, the fill alphas, the
//   inner rim, the urgency window, the zone pulses and the arena's outer wash.
// * Geometry (MakeGeometry): one SM_FX_Quad per decal, yawed along the shape's direction and scaled to its bounds (plus
//   a margin: the anti-aliased edge, the arena's outer rim and wash), with the shape's origin in the quad's frame. The
//   material measures the pixel against the shape there (the signed inward distance `w` and the fill coordinate):
//   ShapeSample mirrors that per-pixel code, and telegraph_test.cpp checks it against the core's ShapeContains with a
//   zero body radius for all four shapes (the line is a box: the core's rectangle, not the contract's rounded end).
// * Timing (StrikeState / ZoneState, on the sim clock: Frame.RenderSimMs, so the fill reaches the rim on the clip's
//   contact frame and a cinematic freezes it): fill progress, the urgency pulse over the last 30 % (bosses: 50 %), the
//   contact flash (110 ms, rim #FF3A48, the front colour at .35) then a 170 ms fade; a cancelled windup fades from where
//   it stood; zones pulse from their start to their end, then fade. PackCpd writes the material's custom primitive data.
// * Decisions: which telegraphs draw a crimson decal (TelegraphLook: every ground strike, Keen's status-only ring
//   included; an on-death fuse only in a themed zone and when the effect damages, so Gravecold's slow ring draws none,
//   DR5-slow, and chapters 2-5 / the Labyrinth keep today's look, PresentsOnDeath; raise / summon markers are the
//   breach-violet `monster.raise.mark` recipe), the recipe ids (contact, the fuse's
//   `elite.<affix>.death`, the hazard chain, the arena ring) and the budget (a cap per quality tier; the arena first,
//   then boss > elite > normal, nearest the hero first).
// * The dash (a charge's EvEntityTeleported{Dash}): the actor slides from where it stood over DashSlideMs.
//
// UE-free and header-only (std + AbyssCore public headers), tested in unreal/PresentationTests/tests/telegraph_test.cpp.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/Json.h"
#include "abyss/base/SimClock.h"
#include "abyss/base/Types.h"
#include "abyss/base/Units.h"
#include "abyss/data/CombatData.h"
#include "abyss/data/MonsterData.h"
#include "abyss/sim/SimTypes.h"

namespace AbyssTelegraphRules
{
	inline constexpr double Pi = 3.14159265358979323846;

	// =================================================================================================================
	// The M_AF_Telegraph contract
	// =================================================================================================================

	/** Decal looks: the three strike styles of the core (TelegraphStyle), the lingering hazard zone, the arena ring. */
	enum class EStyle : unsigned char
	{
		Normal,
		Elite,
		Boss,
		Hazard,
		Arena,
	};
	inline constexpr std::size_t StyleCount = 5;

	inline std::string_view StyleName(EStyle Style)
	{
		switch (Style)
		{
		case EStyle::Normal: return "normal";
		case EStyle::Elite: return "elite";
		case EStyle::Boss: return "boss";
		case EStyle::Hazard: return "hazard";
		case EStyle::Arena: return "arena";
		}
		return "normal";
	}

	/** x (Base + Amp sin(2 pi t / PeriodMs)): the urgency and zone pulses (t = sim ms). */
	struct FPulse
	{
		double Base = 1.0;
		double Amp = 0.0;
		double PeriodMs = 1200.0;

		double At(double TimeMs) const
		{
			if (!(PeriodMs > 0.0))
			{
				return Base;
			}
			return Base + Amp * std::sin(2.0 * Pi * TimeMs / PeriodMs);
		}
	};

	/** A second, thinner rim inside the boundary (elites, bosses): at (1 - AtFraction) x the radius (lines: x half width). */
	struct FInnerRim
	{
		bool bOn = false;
		double Alpha = 0.0;
		double AtFraction = 0.82;
		double WidthCm = 0.0;
	};

	struct FStyleLook
	{
		double RimWidthCm = 26.0;
		double RimAlpha = 0.95;
		double FillAlphaStart = 0.07;
		double FillAlphaContact = 0.22;
		/** The bone fill sweeps to contact (strikes); zones draw the rim only. */
		bool bFill = true;
		FInnerRim InnerRim;
		/** The urgency pulse runs from this fraction of the windup (the last 30 %; bosses from half). */
		double UrgencyFrom = 0.7;
		/** Arena: the rim is measured outward from the boundary (the damage is outside). */
		bool bRimOutside = false;
		/** Arena: a faint flat wash of WashRgb outside, ramping in over WashFadeInCm. */
		bool bOuterWash = false;
		uint32_t WashRgb = 0xD11F2E;
		double WashAlpha = 0.0;
		double WashFadeInCm = 60.0;
		/** Zones (hazard, arena): the rim pulses for the zone's whole window. */
		bool bPulse = false;
		FPulse Pulse;
	};

	struct FContract
	{
		/** Material constants (the content build bakes them as M_AF_Telegraph's parameter defaults). */
		uint32_t FillRgb = 0xFFE8DC;
		uint32_t RimRgb = 0xD11F2E;
		uint32_t FrontRgb = 0xFFF4EE;
		uint32_t FlashRimRgb = 0xFF3A48;
		double FillGrain = 0.3;
		double FillTileCm = 160.0;
		double FrontAlpha = 0.85;
		double FrontWidthCm = 5.0;
		double RimTileCm = 120.0;
		/** The urgency pulse over the last 30 % of a windup. */
		FPulse Urgency{ 0.82, 0.18, 180.0 };
		/** Contact: the flash (the fill jumps to FrontColor at FlashFillAlpha, the rim to FlashRimRgb), then the fade. */
		double FlashMs = 110.0;
		double FadeMs = 170.0;
		double FlashFillAlpha = 0.35;
		std::array<FStyleLook, StyleCount> Styles{};
		/** Parsed from the manifest (false: the built-in copy of the contract). */
		bool bFromManifest = false;

		const FStyleLook& Style(EStyle S) const { return Styles[static_cast<std::size_t>(S)]; }
	};

	namespace Detail
	{
		inline bool IsDigit(char C) { return C >= '0' && C <= '9'; }

		/** Every unsigned decimal number of a text, in order ("x (0.85 + 0.15 sin(2 pi t / 1.2 s))" -> .85 .15 2 1.2). */
		inline std::vector<double> Numbers(std::string_view Text)
		{
			std::vector<double> Out;
			std::size_t I = 0;
			while (I < Text.size())
			{
				const bool bLeadingDot = Text[I] == '.' && I + 1 < Text.size() && IsDigit(Text[I + 1]);
				if (!IsDigit(Text[I]) && !bLeadingDot)
				{
					++I;
					continue;
				}
				double Value = 0.0;
				while (I < Text.size() && IsDigit(Text[I]))
				{
					Value = Value * 10.0 + static_cast<double>(Text[I] - '0');
					++I;
				}
				if (I + 1 < Text.size() && Text[I] == '.' && IsDigit(Text[I + 1]))
				{
					++I;
					double Scale = 0.1;
					while (I < Text.size() && IsDigit(Text[I]))
					{
						Value += Scale * static_cast<double>(Text[I] - '0');
						Scale *= 0.1;
						++I;
					}
				}
				Out.push_back(Value);
			}
			return Out;
		}

		inline int HexDigit(char C)
		{
			if (C >= '0' && C <= '9') return C - '0';
			if (C >= 'a' && C <= 'f') return C - 'a' + 10;
			if (C >= 'A' && C <= 'F') return C - 'A' + 10;
			return -1;
		}

		inline void ReadNumber(const abyss::JsonValue& V, double& InOut)
		{
			if (V.IsNumber())
			{
				InOut = V.AsDouble(InOut);
			}
		}
	}

	/** "#RRGGBB" -> 0xRRGGBB; false (Out untouched) when malformed. */
	inline bool ParseHexRgb(std::string_view Text, uint32_t& Out)
	{
		if (!Text.empty() && Text.front() == '#')
		{
			Text.remove_prefix(1);
		}
		if (Text.size() != 6)
		{
			return false;
		}
		uint32_t Value = 0;
		for (const char C : Text)
		{
			const int D = Detail::HexDigit(C);
			if (D < 0)
			{
				return false;
			}
			Value = (Value << 4) | static_cast<uint32_t>(D);
		}
		Out = Value;
		return true;
	}

	/** "x (A + B sin(2 pi t / C s))" (the contract's pulse strings): the last four numbers are A, B, 2, C. */
	inline bool ParsePulse(std::string_view Text, FPulse& Out)
	{
		const std::vector<double> N = Detail::Numbers(Text);
		if (N.size() < 4 || std::fabs(N[N.size() - 2] - 2.0) > 1e-9 || !(N.back() > 0.0))
		{
			return false;
		}
		Out.Base = N[N.size() - 4];
		Out.Amp = N[N.size() - 3];
		Out.PeriodMs = N.back() * 1000.0;
		return true;
	}

	/** The contract as shipped (manifest materials.M_AF_Telegraph, A12): what ParseContract starts from. */
	inline FContract DefaultContract()
	{
		FContract C;
		FStyleLook Base;
		C.Styles[static_cast<std::size_t>(EStyle::Normal)] = Base;

		FStyleLook Elite = Base;
		Elite.RimWidthCm = 30.0;
		Elite.InnerRim = FInnerRim{ true, 0.6, 0.82, 10.0 };
		C.Styles[static_cast<std::size_t>(EStyle::Elite)] = Elite;

		FStyleLook Boss = Base;
		Boss.RimWidthCm = 36.0;
		Boss.FillAlphaContact = 0.27;
		Boss.InnerRim = FInnerRim{ true, 0.7, 0.82, 12.0 };
		Boss.UrgencyFrom = 0.5;
		C.Styles[static_cast<std::size_t>(EStyle::Boss)] = Boss;

		FStyleLook Hazard = Base;
		Hazard.RimAlpha = 0.75;
		Hazard.bFill = false;
		Hazard.bPulse = true;
		Hazard.Pulse = FPulse{ 0.8, 0.2, 1200.0 };
		C.Styles[static_cast<std::size_t>(EStyle::Hazard)] = Hazard;

		FStyleLook Arena = Base;
		Arena.RimAlpha = 0.85;
		Arena.bFill = false;
		Arena.bRimOutside = true;
		Arena.bOuterWash = true;
		Arena.WashRgb = 0xD11F2E;
		Arena.WashAlpha = 0.1;
		Arena.WashFadeInCm = 60.0;
		Arena.bPulse = true;
		Arena.Pulse = FPulse{ 0.85, 0.15, 1200.0 };
		C.Styles[static_cast<std::size_t>(EStyle::Arena)] = Arena;
		return C;
	}

	/**
	 * The contract from the manifest's `materials` object (FAbyssArtManifest::GetMaterialsJson): `params`, `contact`,
	 * `urgency`, `styles.<name>` (each style starts from `params` and takes its own RimWidthCm / RimAlpha /
	 * FillAlphaContact / FillAlphaStart, `fill`, `innerRim`, `urgencyFrom`, `rimSide`, `outerWash`, `pulse`). A missing or
	 * malformed field keeps the shipped value; no `M_AF_Telegraph` object keeps DefaultContract (bFromManifest false).
	 */
	inline FContract ParseContract(const abyss::JsonValue& Materials)
	{
		FContract C = DefaultContract();
		const abyss::JsonValue& T = Materials.Get("M_AF_Telegraph");
		if (!T.IsObject())
		{
			return C;
		}
		C.bFromManifest = true;
		const abyss::JsonValue& P = T.Get("params");
		ParseHexRgb(P.Get("FillColor").AsString(), C.FillRgb);
		ParseHexRgb(P.Get("RimColor").AsString(), C.RimRgb);
		ParseHexRgb(P.Get("FrontColor").AsString(), C.FrontRgb);
		Detail::ReadNumber(P.Get("FillGrain"), C.FillGrain);
		Detail::ReadNumber(P.Get("FillTileCm"), C.FillTileCm);
		Detail::ReadNumber(P.Get("FrontAlpha"), C.FrontAlpha);
		Detail::ReadNumber(P.Get("FrontWidthCm"), C.FrontWidthCm);
		Detail::ReadNumber(P.Get("RimTileCm"), C.RimTileCm);

		FStyleLook Base = C.Style(EStyle::Normal);
		Detail::ReadNumber(P.Get("RimWidthCm"), Base.RimWidthCm);
		Detail::ReadNumber(P.Get("RimAlpha"), Base.RimAlpha);
		Detail::ReadNumber(P.Get("FillAlphaStart"), Base.FillAlphaStart);
		Detail::ReadNumber(P.Get("FillAlphaContact"), Base.FillAlphaContact);

		const abyss::JsonValue& Contact = T.Get("contact");
		Detail::ReadNumber(Contact.Get("flashMs"), C.FlashMs);
		Detail::ReadNumber(Contact.Get("fadeMs"), C.FadeMs);
		Detail::ReadNumber(Contact.Get("fillAlpha"), C.FlashFillAlpha);
		// "... the rim to #FF3A48 ...": the flash rim colour is named in the contact look.
		const std::string_view ContactLook = Contact.Get("look").AsString();
		if (const std::size_t Hash = ContactLook.find("rim to #"); Hash != std::string_view::npos)
		{
			ParseHexRgb(ContactLook.substr(Hash + 7, 7), C.FlashRimRgb);
		}

		// "over the last 30 % of the windup the rim alpha pulses x (0.82 + 0.18 sin(2 pi t / 0.18 s))"
		const std::string_view Urgency = T.Get("urgency").AsString();
		double UrgencyFrom = 0.7;
		if (ParsePulse(Urgency, C.Urgency))
		{
			const std::vector<double> N = Detail::Numbers(Urgency);
			if (Urgency.find("last") != std::string_view::npos && N.size() >= 5 && N.front() > 0.0 && N.front() < 100.0)
			{
				UrgencyFrom = 1.0 - N.front() / 100.0;
			}
		}
		Base.UrgencyFrom = UrgencyFrom;

		const abyss::JsonValue& Styles = T.Get("styles");
		for (std::size_t Index = 0; Index < StyleCount; ++Index)
		{
			const EStyle Style = static_cast<EStyle>(Index);
			FStyleLook Look = Base;
			// The built-in zone / arena flags (fill off, outside rim, pulse) stay unless the manifest says otherwise.
			const FStyleLook& Shipped = C.Styles[Index];
			Look.bFill = Shipped.bFill;
			Look.bRimOutside = Shipped.bRimOutside;
			Look.bOuterWash = Shipped.bOuterWash;
			Look.WashRgb = Shipped.WashRgb;
			Look.WashAlpha = Shipped.WashAlpha;
			Look.WashFadeInCm = Shipped.WashFadeInCm;
			Look.bPulse = Shipped.bPulse;
			Look.Pulse = Shipped.Pulse;
			const abyss::JsonValue& S = Styles.Get(StyleName(Style));
			if (S.IsObject())
			{
				Detail::ReadNumber(S.Get("RimWidthCm"), Look.RimWidthCm);
				Detail::ReadNumber(S.Get("RimAlpha"), Look.RimAlpha);
				Detail::ReadNumber(S.Get("FillAlphaStart"), Look.FillAlphaStart);
				Detail::ReadNumber(S.Get("FillAlphaContact"), Look.FillAlphaContact);
				Detail::ReadNumber(S.Get("urgencyFrom"), Look.UrgencyFrom);
				if (S.Get("fill").IsBool())
				{
					Look.bFill = S.Get("fill").AsBool(Look.bFill);
				}
				const abyss::JsonValue& Inner = S.Get("innerRim");
				if (Inner.IsObject())
				{
					Look.InnerRim.bOn = true;
					Detail::ReadNumber(Inner.Get("alpha"), Look.InnerRim.Alpha);
					Detail::ReadNumber(Inner.Get("atFraction"), Look.InnerRim.AtFraction);
					Detail::ReadNumber(Inner.Get("widthCm"), Look.InnerRim.WidthCm);
				}
				if (S.Get("rimSide").IsString())
				{
					Look.bRimOutside = S.Get("rimSide").AsString() == "outside";
				}
				const abyss::JsonValue& Wash = S.Get("outerWash");
				if (Wash.IsObject())
				{
					Look.bOuterWash = true;
					Detail::ReadNumber(Wash.Get("alpha"), Look.WashAlpha);
					Detail::ReadNumber(Wash.Get("fadeInCm"), Look.WashFadeInCm);
					ParseHexRgb(Wash.Get("color").AsString(), Look.WashRgb);
				}
				if (S.Get("pulse").IsString())
				{
					FPulse Pulse;
					if (ParsePulse(S.Get("pulse").AsString(), Pulse))
					{
						Look.bPulse = true;
						Look.Pulse = Pulse;
					}
				}
			}
			C.Styles[Index] = Look;
		}
		return C;
	}

	// =================================================================================================================
	// Geometry: the quad and the per-pixel shape test
	// =================================================================================================================

	/** M_AF_Telegraph's shape codes (custom primitive data Cpd::Shape). */
	enum class EShape : unsigned char
	{
		Circle = 0,
		Ring = 1,
		Cone = 2,
		Line = 3,
	};

	/** The anti-aliased edge beyond a shape (cm): the quad reaches this far past the boundary. */
	inline constexpr double EdgeMarginCm = 4.0;
	/** The arena's outer wash band (cm outside the boundary) and the part of it that fades out (no square quad edge). */
	inline constexpr double ArenaWashBandCm = 250.0;
	inline constexpr double ArenaWashFadeOutCm = 100.0;
	/** Lift over the terrain (the ground-lift rule of the sprite quads). */
	inline constexpr double GroundLiftCm = 8.0;
	/** Translucent sort priority: over blob shadows (-10), prop pools (-5) and water (-2), under the VFX (1 / 2). */
	inline constexpr int SortPriority = -1;

	struct FGeometry
	{
		EShape Shape = EShape::Circle;
		/** Circle / ring / cone: the radius; line: the length (cm). */
		double ACm = 0.0;
		/** Ring: the inner radius (cm); cone: half the opening (radians); line: half the width (cm). */
		double B = 0.0;
		/** The shape's origin (world cm, UE X / Y) and its direction (unit; +X of the quad). */
		double OriginXCm = 0.0;
		double OriginYCm = 0.0;
		double DirX = 1.0;
		double DirY = 0.0;
		/** The quad: its centre (world cm), yaw (degrees, 0 = +X), size (cm) and the shape's origin in its frame (cm). */
		double CentreXCm = 0.0;
		double CentreYCm = 0.0;
		double YawDeg = 0.0;
		double SizeXCm = 0.0;
		double SizeYCm = 0.0;
		double LocalOriginXCm = 0.0;
		double LocalOriginYCm = 0.0;
		/** How far the fill sweeps (circle / cone: the radius; ring: its width; line: its length; cm). */
		double FillMaxCm = 0.0;
		/** The inner rim's reference (the radius; lines: the half width; cm). */
		double InnerRimRefCm = 0.0;
	};

	inline EShape ShapeOf(abyss::AbilityShapeKind Kind)
	{
		switch (Kind)
		{
		case abyss::AbilityShapeKind::Circle: return EShape::Circle;
		case abyss::AbilityShapeKind::Ring: return EShape::Ring;
		case abyss::AbilityShapeKind::Cone: return EShape::Cone;
		case abyss::AbilityShapeKind::Line: return EShape::Line;
		}
		return EShape::Circle;
	}

	/**
	 * The decal of `Shape` (tiles, degrees) at `Origin` (tiles) facing `Dir` (unit; a zero direction faces +X). OutsideCm:
	 * how far the quad reaches past the boundary (EdgeMarginCm; the arena adds its outward rim and wash band). A ring
	 * whose inner radius is not below its radius draws as a circle; a cone of 360 degrees or more as a circle sector of
	 * half-opening pi.
	 */
	inline FGeometry MakeGeometry(const abyss::AbilityShapeDef& Shape, abyss::Vec2 Origin, abyss::Vec2 Dir, double OutsideCm)
	{
		const double Tile = abyss::kTileSizeUU;
		FGeometry G;
		G.Shape = ShapeOf(Shape.kind);
		const double Len = std::sqrt(Dir.x * Dir.x + Dir.y * Dir.y);
		if (Len > 1e-9)
		{
			G.DirX = Dir.x / Len;
			G.DirY = Dir.y / Len;
		}
		G.OriginXCm = Origin.x * Tile;
		G.OriginYCm = Origin.y * Tile;
		G.YawDeg = std::atan2(G.DirY, G.DirX) * 180.0 / Pi;
		const double Out = (std::max)(0.0, OutsideCm);
		double MinX = 0.0;
		double MaxX = 0.0;
		double MinY = 0.0;
		double MaxY = 0.0;
		switch (G.Shape)
		{
		case EShape::Circle:
		case EShape::Ring:
		{
			G.ACm = (std::max)(0.0, Shape.radius) * Tile;
			const double Inner = (std::max)(0.0, Shape.innerRadius) * Tile;
			if (G.Shape == EShape::Ring && Inner > 0.0 && Inner < G.ACm)
			{
				G.B = Inner;
				G.FillMaxCm = G.ACm - Inner;
			}
			else
			{
				G.Shape = EShape::Circle;
				G.FillMaxCm = G.ACm;
			}
			G.InnerRimRefCm = G.ACm;
			MinX = MinY = -G.ACm;
			MaxX = MaxY = G.ACm;
			break;
		}
		case EShape::Cone:
		{
			G.ACm = (std::max)(0.0, Shape.radius) * Tile;
			G.B = (std::min)(Pi, (std::max)(0.0, Shape.arcDeg) * Pi / 360.0);
			G.FillMaxCm = G.ACm;
			G.InnerRimRefCm = G.ACm;
			MaxX = G.ACm;
			MinX = G.B >= Pi - 1e-12 ? -G.ACm : (std::min)(0.0, G.ACm * std::cos(G.B));
			const double HalfY = G.B >= Pi / 2.0 ? G.ACm : G.ACm * std::sin(G.B);
			MinY = -HalfY;
			MaxY = HalfY;
			break;
		}
		case EShape::Line:
		{
			G.ACm = (std::max)(0.0, Shape.length) * Tile;
			G.B = (std::max)(0.0, Shape.width) * 0.5 * Tile;
			G.FillMaxCm = G.ACm;
			G.InnerRimRefCm = G.B;
			MinX = 0.0;
			MaxX = G.ACm;
			MinY = -G.B;
			MaxY = G.B;
			break;
		}
		}
		MinX -= Out;
		MaxX += Out;
		MinY -= Out;
		MaxY += Out;
		G.SizeXCm = MaxX - MinX;
		G.SizeYCm = MaxY - MinY;
		const double MidX = 0.5 * (MinX + MaxX);
		const double MidY = 0.5 * (MinY + MaxY);
		// The quad's centre is the box's middle, the shape's origin sits at -Mid in the quad's frame.
		G.LocalOriginXCm = -MidX;
		G.LocalOriginYCm = -MidY;
		G.CentreXCm = G.OriginXCm + G.DirX * MidX - G.DirY * MidY;
		G.CentreYCm = G.OriginYCm + G.DirY * MidX + G.DirX * MidY;
		return G;
	}

	/** One pixel of the decal, as the material measures it (hlsl.py AF_Telegraph; the A12 preview's telegraph_layer). */
	struct FSample
	{
		/** Signed distance inward from the boundary (cm): >= 0 inside, minus the distance to the shape outside. */
		double W = -1.0;
		/** The fill coordinate (cm from where the fill starts: the centre, the ring's inner edge, the line's start). */
		double FillR = 0.0;
		/** Arc length along the boundary (cm): the rim texture's U before / RimTileCm. */
		double EdgeU = 0.0;
	};

	/** The sample at a point in the shape's frame (A along the direction, Bp across to its left; cm from the origin). */
	inline FSample ShapeSample(const FGeometry& G, double A, double Bp)
	{
		FSample S;
		const double R = std::sqrt(A * A + Bp * Bp);
		switch (G.Shape)
		{
		case EShape::Circle:
			S.W = G.ACm - R;
			S.EdgeU = std::atan2(Bp, A) * G.ACm;
			S.FillR = R;
			break;
		case EShape::Ring:
			S.W = (std::min)(G.ACm - R, R - G.B);
			S.EdgeU = std::atan2(Bp, A) * G.ACm;
			S.FillR = R - G.B;
			break;
		case EShape::Cone:
		{
			const double Angle = std::atan2(Bp, A);
			S.FillR = R;
			if (G.B >= Pi - 1e-12)
			{
				S.W = G.ACm - R;   // a full turn: the disc
				S.EdgeU = Angle * G.ACm;
				break;
			}
			// The circular sector's signed distance (the aperture about +X; q = (|across|, along), c = the edge's unit
			// vector (sin half, cos half)): max(r - radius, m x sign), m the distance to the edge segment, negative
			// inside. Inside is the core's test (dot(p, dir) >= cos(half) |p| within the radius).
			const double Qx = std::fabs(Bp);
			const double Qy = A;
			const double Cx = std::sin(G.B);
			const double Cy = std::cos(G.B);
			const double T = (std::min)(G.ACm, (std::max)(0.0, Qx * Cx + Qy * Cy));
			const double Mx = Qx - Cx * T;
			const double My = Qy - Cy * T;
			const double M = std::sqrt(Mx * Mx + My * My);
			const double Side = Cy * Qx - Cx * Qy;
			const double Sign = Side > 0.0 ? 1.0 : (Side < 0.0 ? -1.0 : 0.0);
			S.W = -(std::max)(R - G.ACm, M * Sign);
			S.EdgeU = G.ACm - R < M ? Angle * G.ACm : R;
			break;
		}
		case EShape::Line:
		{
			// The box's signed distance (the core's rectangle: square ends), inward positive.
			const double HalfLength = 0.5 * G.ACm;
			const double Qx = std::fabs(A - HalfLength) - HalfLength;
			const double Qy = std::fabs(Bp) - G.B;
			const double Ox = (std::max)(Qx, 0.0);
			const double Oy = (std::max)(Qy, 0.0);
			S.W = -(std::sqrt(Ox * Ox + Oy * Oy) + (std::min)((std::max)(Qx, Qy), 0.0));
			const double Ends = (std::min)(A, G.ACm - A);
			const double Sides = G.B - std::fabs(Bp);
			S.EdgeU = Sides < Ends ? A : Bp;
			S.FillR = A;
			break;
		}
		}
		return S;
	}

	/** The sample at a world point (cm, UE X / Y). */
	inline FSample SampleAtWorld(const FGeometry& G, double XCm, double YCm)
	{
		const double Dx = XCm - G.OriginXCm;
		const double Dy = YCm - G.OriginYCm;
		return ShapeSample(G, Dx * G.DirX + Dy * G.DirY, -Dx * G.DirY + Dy * G.DirX);
	}

	/**
	 * The sample at the quad's UV (SM_FX_Quad: UV (0,0) at local (-50, +50), (1,1) at (+50, -50); the component scales
	 * it to SizeX x SizeY): what the material sees, from UV alone.
	 */
	inline FSample SampleAtQuadUV(const FGeometry& G, double U, double V)
	{
		const double LocalX = (U - 0.5) * G.SizeXCm;
		const double LocalY = (0.5 - V) * G.SizeYCm;
		return ShapeSample(G, LocalX - G.LocalOriginXCm, LocalY - G.LocalOriginYCm);
	}

	/** The quad's UV of a world point (the inverse of the component transform). */
	inline void WorldToQuadUV(const FGeometry& G, double XCm, double YCm, double& OutU, double& OutV)
	{
		const double Dx = XCm - G.CentreXCm;
		const double Dy = YCm - G.CentreYCm;
		const double LocalX = Dx * G.DirX + Dy * G.DirY;
		const double LocalY = -Dx * G.DirY + Dy * G.DirX;
		OutU = G.SizeXCm > 0.0 ? LocalX / G.SizeXCm + 0.5 : 0.5;
		OutV = G.SizeYCm > 0.0 ? 0.5 - LocalY / G.SizeYCm : 0.5;
	}

	/** How far a decal of `Look` reaches past its boundary (the quad margin, cm). */
	inline double OutsideReachCm(const FStyleLook& Look, bool bOuterWash)
	{
		double Out = EdgeMarginCm;
		if (Look.bRimOutside)
		{
			Out += Look.RimWidthCm;
		}
		if (Look.bOuterWash && bOuterWash)
		{
			Out = (std::max)(Out, ArenaWashBandCm + EdgeMarginCm);
		}
		return Out;
	}

	// =================================================================================================================
	// Timing
	// =================================================================================================================

	enum class EPhase : unsigned char
	{
		Before,     // not started (or nothing to draw)
		Windup,     // filling toward contact
		Flash,      // contact: the flash
		Fade,       // after the flash, a cancel or a zone's end
		Done,       // gone
		Zone,       // a zone between its start and its end
	};

	/** What the material draws this frame (PackCpd). */
	struct FState
	{
		EPhase Phase = EPhase::Before;
		bool bVisible = false;
		double Progress = 0.0;
		/** Fill (strikes) or outer wash (arena) alpha, x the fade. */
		double FillAlpha = 0.0;
		/** The front band's alpha (the flash: the flash fill alpha), x the fade. */
		double FrontAlpha = 0.0;
		double RimAlpha = 0.0;
		/** 1 during the contact flash (rim -> FlashRimRgb, fill -> FrontRgb). */
		double Flash = 0.0;
	};

	/** Fill progress (now - start) / (contact - start), clamped to [0, 1]; a contact at or before the start is full. */
	inline double Progress(double NowMs, double StartMs, double ContactMs)
	{
		const double Span = ContactMs - StartMs;
		if (!(Span > 0.0))
		{
			return NowMs >= StartMs ? 1.0 : 0.0;
		}
		return (std::min)(1.0, (std::max)(0.0, (NowMs - StartMs) / Span));
	}

	/** The urgency window: from UrgencyFrom of the windup until contact. */
	inline bool InUrgency(const FStyleLook& Look, double ProgressNow, double NowMs, double ContactMs)
	{
		return ProgressNow >= Look.UrgencyFrom && NowMs < ContactMs;
	}

	/**
	 * A strike's decal (or an on-death fuse's) at NowMs: windup (fill lerps FillAlphaStart -> FillAlphaContact, the rim
	 * pulses in the urgency window), the contact flash for FlashMs, then FadeMs of fade (the fill at full, no pulse).
	 * CancelMs >= 0 (EvMonsterAttackCancelled before contact): the decal fades from where it stood over FadeMs, no flash.
	 */
	inline FState StrikeState(const FContract& C, EStyle Style, double NowMs, double StartMs, double ContactMs,
		double CancelMs = -1.0)
	{
		const FStyleLook& Look = C.Style(Style);
		FState S;
		if (NowMs < StartMs)
		{
			return S;
		}
		const bool bCancelled = CancelMs >= 0.0 && CancelMs < ContactMs;
		if (bCancelled && NowMs >= CancelMs)
		{
			const double K = 1.0 - (NowMs - CancelMs) / (std::max)(1.0, C.FadeMs);
			if (K <= 0.0)
			{
				S.Phase = EPhase::Done;
				return S;
			}
			S.Phase = EPhase::Fade;
			S.bVisible = true;
			S.Progress = Progress(CancelMs, StartMs, ContactMs);
			if (Look.bFill)
			{
				S.FillAlpha = (Look.FillAlphaStart + (Look.FillAlphaContact - Look.FillAlphaStart) * S.Progress) * K;
				S.FrontAlpha = C.FrontAlpha * K;
			}
			S.RimAlpha = Look.RimAlpha * K;
			return S;
		}
		if (NowMs < ContactMs)
		{
			S.Phase = EPhase::Windup;
			S.bVisible = true;
			S.Progress = Progress(NowMs, StartMs, ContactMs);
			if (Look.bFill)
			{
				S.FillAlpha = Look.FillAlphaStart + (Look.FillAlphaContact - Look.FillAlphaStart) * S.Progress;
				S.FrontAlpha = C.FrontAlpha;
			}
			const double Pulse = InUrgency(Look, S.Progress, NowMs, ContactMs) ? C.Urgency.At(NowMs) : 1.0;
			S.RimAlpha = Look.RimAlpha * Pulse;
			return S;
		}
		const double After = NowMs - ContactMs;
		S.Progress = 1.0;
		if (After <= C.FlashMs)
		{
			S.Phase = EPhase::Flash;
			S.bVisible = true;
			S.Flash = 1.0;
			S.FillAlpha = C.FlashFillAlpha;
			S.FrontAlpha = C.FlashFillAlpha;
			S.RimAlpha = Look.RimAlpha;
			return S;
		}
		const double K = 1.0 - (After - C.FlashMs) / (std::max)(1.0, C.FadeMs);
		if (K <= 0.0)
		{
			S.Phase = EPhase::Done;
			return S;
		}
		S.Phase = EPhase::Fade;
		S.bVisible = true;
		if (Look.bFill)
		{
			S.FillAlpha = Look.FillAlphaContact * K;
			S.FrontAlpha = C.FrontAlpha * K;
		}
		S.RimAlpha = Look.RimAlpha * K;
		return S;
	}

	/**
	 * A lingering zone (a hazard until its untilMs, an arena until it clears; EndMs < 0 = still open) at NowMs: the rim
	 * pulses from StartMs to EndMs (the arena also draws its outer wash), then fades out over FadeMs.
	 */
	inline FState ZoneState(const FContract& C, EStyle Style, double NowMs, double StartMs, double EndMs)
	{
		const FStyleLook& Look = C.Style(Style);
		FState S;
		if (NowMs < StartMs)
		{
			return S;
		}
		double K = 1.0;
		if (EndMs >= 0.0 && NowMs > EndMs)
		{
			K = 1.0 - (NowMs - EndMs) / (std::max)(1.0, C.FadeMs);
			if (K <= 0.0)
			{
				S.Phase = EPhase::Done;
				return S;
			}
			S.Phase = EPhase::Fade;
		}
		else
		{
			S.Phase = EPhase::Zone;
		}
		S.bVisible = true;
		S.Progress = 1.0;
		const double Pulse = Look.bPulse ? Look.Pulse.At(NowMs) : 1.0;
		S.RimAlpha = Look.RimAlpha * Pulse * K;
		if (Look.bOuterWash)
		{
			S.FillAlpha = Look.WashAlpha * K;
		}
		return S;
	}

	// =================================================================================================================
	// Custom primitive data (M_AF_Telegraph; Scripts/abyss_content/ue/materials.py build_telegraph reads the same slots)
	// =================================================================================================================

	namespace Cpd
	{
		inline constexpr int Shape = 0;            // EShape
		inline constexpr int A = 1;                // radius / length (cm)
		inline constexpr int B = 2;                // inner radius (cm) / half opening (rad) / half width (cm)
		inline constexpr int RimWidthCm = 3;
		inline constexpr int SizeXCm = 4;          // the quad (cm)
		inline constexpr int SizeYCm = 5;
		inline constexpr int LocalOriginXCm = 6;   // the shape's origin in the quad's frame (cm)
		inline constexpr int LocalOriginYCm = 7;
		inline constexpr int InnerRimAlpha = 8;    // 0 = none
		inline constexpr int InnerRimInsetCm = 9;  // (1 - atFraction) x the radius / half width
		inline constexpr int InnerRimWidthCm = 10;
		inline constexpr int WashBandCm = 11;      // arena: the outer wash band (0 = none)
		inline constexpr int FillOn = 12;          // 1: the bone fill sweeps (strikes)
		inline constexpr int RimOutside = 13;      // 1: arena (rim and wash outside the boundary)
		inline constexpr int Flash = 14;
		inline constexpr int Progress = 15;
		inline constexpr int FillAlpha = 16;       // fill / wash alpha (x fade)
		inline constexpr int FrontAlpha = 17;      // the front band's alpha (x fade)
		inline constexpr int RimAlpha = 18;        // x urgency / pulse / fade
		inline constexpr int Spare = 19;
		inline constexpr int Count = 20;           // five float4 groups (SetCustomPrimitiveDataVector4 at 0, 4, 8, 12, 16)
	}

	/** Every custom primitive data value of a decal this frame (Cpd::Count floats). */
	inline std::array<float, Cpd::Count> PackCpd(const FGeometry& G, const FStyleLook& Look, const FState& S, bool bOuterWash)
	{
		std::array<float, Cpd::Count> Out{};
		Out[Cpd::Shape] = static_cast<float>(static_cast<int>(G.Shape));
		Out[Cpd::A] = static_cast<float>(G.ACm);
		Out[Cpd::B] = static_cast<float>(G.B);
		Out[Cpd::RimWidthCm] = static_cast<float>(Look.RimWidthCm);
		Out[Cpd::SizeXCm] = static_cast<float>(G.SizeXCm);
		Out[Cpd::SizeYCm] = static_cast<float>(G.SizeYCm);
		Out[Cpd::LocalOriginXCm] = static_cast<float>(G.LocalOriginXCm);
		Out[Cpd::LocalOriginYCm] = static_cast<float>(G.LocalOriginYCm);
		if (Look.InnerRim.bOn)
		{
			Out[Cpd::InnerRimAlpha] = static_cast<float>(Look.InnerRim.Alpha);
			Out[Cpd::InnerRimInsetCm] = static_cast<float>((1.0 - Look.InnerRim.AtFraction) * G.InnerRimRefCm);
			Out[Cpd::InnerRimWidthCm] = static_cast<float>(Look.InnerRim.WidthCm);
		}
		const bool bWash = Look.bOuterWash && bOuterWash;
		Out[Cpd::WashBandCm] = bWash ? static_cast<float>(ArenaWashBandCm) : 0.f;
		Out[Cpd::FillOn] = Look.bFill ? 1.f : 0.f;
		Out[Cpd::RimOutside] = Look.bRimOutside ? 1.f : 0.f;
		Out[Cpd::Flash] = static_cast<float>(S.Flash);
		Out[Cpd::Progress] = static_cast<float>(S.Progress);
		Out[Cpd::FillAlpha] = bWash || Look.bFill ? static_cast<float>(S.FillAlpha) : 0.f;
		Out[Cpd::FrontAlpha] = Look.bFill ? static_cast<float>(S.FrontAlpha) : 0.f;
		Out[Cpd::RimAlpha] = static_cast<float>(S.RimAlpha);
		return Out;
	}

	/** The float4 groups SetCustomPrimitiveDataVector4 sends (at Group x 4). */
	inline constexpr int CpdGroups = Cpd::Count / 4;
	/** A group is sent again when one of its values moved by more than this since that group was last sent. */
	inline constexpr float CpdTolerance = 0.0005f;

	/**
	 * Which float4 groups a decal sends this frame (all when the cache is not valid yet) and the cache of what the GPU
	 * holds: only a sent group updates Cache (2G review m4), so a slow drift under the tolerance per frame (a zone pulse
	 * near its peak, a long fade at a high frame rate) accumulates against the sent value until it is sent.
	 */
	inline std::array<bool, CpdGroups> UpdateCpdCache(std::array<float, Cpd::Count>& Cache, bool& bValid,
		const std::array<float, Cpd::Count>& Values)
	{
		std::array<bool, CpdGroups> Send{};
		for (int Group = 0; Group < CpdGroups; ++Group)
		{
			const std::size_t First = static_cast<std::size_t>(Group) * 4;
			bool bChanged = !bValid;
			for (std::size_t Offset = 0; Offset < 4 && !bChanged; ++Offset)
			{
				bChanged = std::fabs(Cache[First + Offset] - Values[First + Offset]) > CpdTolerance;
			}
			if (bChanged)
			{
				for (std::size_t Offset = 0; Offset < 4; ++Offset)
				{
					Cache[First + Offset] = Values[First + Offset];
				}
			}
			Send[static_cast<std::size_t>(Group)] = bChanged;
		}
		bValid = true;
		return Send;
	}

	// =================================================================================================================
	// Decisions and recipe ids
	// =================================================================================================================

	inline EStyle StyleOf(abyss::TelegraphStyle Style)
	{
		switch (Style)
		{
		case abyss::TelegraphStyle::Elite: return EStyle::Elite;
		case abyss::TelegraphStyle::Boss: return EStyle::Boss;
		case abyss::TelegraphStyle::Normal: break;
		}
		return EStyle::Normal;
	}

	/** What an EvTelegraph / TelegraphView draws. */
	enum class ELook : unsigned char
	{
		None,
		Decal,         // the crimson ground decal (M_AF_Telegraph) and its contact recipe
		RaiseMarker,   // the breach-violet raise / summon marker (monster.raise.mark; never the crimson decal, DR5)
	};

	/**
	 * Whether on-death fuses are presented in a zone (2G review M1; DR16 (9)): only in a zone with a presentation theme
	 * (Chapter 1's march). Unthemed zones (chapters 2-5, the Labyrinth's floors, the Ember Tower) keep today's look, as
	 * DR15 (7) keeps their rescue prop: the global elite fuses of the 2B core (Scorched's burst, Gravecold's ring) and the
	 * Labyrinth's `curse.volatile` burst go off there with neither a decal nor an `elite.<type>.death`, as before 2G.
	 * Ability telegraphs are not gated (only Chapter 1 definitions have abilities; a strike always warns).
	 */
	inline bool PresentsOnDeath(bool bThemedZone)
	{
		return bThemedZone;
	}

	/**
	 * Ability telegraphs: a ground strike (circle / cone / line / ring / hazard) draws the decal, whatever its damage (the
	 * Corpseherd's Keen is status-only and keeps its crimson ring: the `notify.FX_Keen` binding draws under U2's decal);
	 * a raise / summon draws the marker. On-death fuses draw the decal only in a themed zone (PresentsOnDeath) and when
	 * the effect damages (bOnDeathDamages): Gravecold's slow ring has its own pre-cue (elite.frozen.death, DR5-slow).
	 */
	inline ELook TelegraphLook(abyss::AbilityKind Kind, abyss::TelegraphSource Source, bool bOnDeathDamages, bool bThemedZone)
	{
		if (Source == abyss::TelegraphSource::OnDeath)
		{
			return PresentsOnDeath(bThemedZone) && bOnDeathDamages ? ELook::Decal : ELook::None;
		}
		switch (Kind)
		{
		case abyss::AbilityKind::Circle:
		case abyss::AbilityKind::Cone:
		case abyss::AbilityKind::Line:
		case abyss::AbilityKind::Ring:
		case abyss::AbilityKind::Hazard:
			return ELook::Decal;
		case abyss::AbilityKind::Raise:
		case abyss::AbilityKind::Summon:
			return ELook::RaiseMarker;
		case abyss::AbilityKind::Bolt:
		case abyss::AbilityKind::Buff:
		case abyss::AbilityKind::Call:
			break;
		}
		return ELook::None;
	}

	inline constexpr std::string_view OnDeathLabel = "onDeath";
	inline constexpr std::string_view AffixLabelPrefix = "affix.";

	/**
	 * The on-death effect a fuse label names (CombatSystem::OnMonsterDeathEffects): "onDeath" = the dead monster's own
	 * (Def), "affix.<type>" = the elite affix's; nullptr for anything else (the zone's "curse.<id>", an unknown
	 * definition or affix).
	 */
	inline const abyss::MonsterOnDeathDef* FindOnDeathDef(std::string_view Label, const abyss::MonsterDef* Def,
		const abyss::EliteAffixTable* Affixes)
	{
		if (Label == OnDeathLabel)
		{
			return Def != nullptr && Def->onDeath.present ? &Def->onDeath : nullptr;
		}
		if (Label.substr(0, AffixLabelPrefix.size()) == AffixLabelPrefix && Affixes != nullptr)
		{
			abyss::EliteAffixType Type = abyss::EliteAffixType::FireEnhanced;
			if (abyss::ParseEnum(Label.substr(AffixLabelPrefix.size()), Type))
			{
				const abyss::MonsterOnDeathDef& D = Affixes->Def(Type).onDeath;
				return D.present ? &D : nullptr;
			}
		}
		return nullptr;
	}

	/** Whether the fuse's effect damages (a fixed `dmg` or an `hpPct`); unknown effects count as damaging (drawn). */
	inline bool OnDeathDamages(std::string_view Label, const abyss::MonsterDef* Def, const abyss::EliteAffixTable* Affixes)
	{
		const abyss::MonsterOnDeathDef* D = FindOnDeathDef(Label, Def, Affixes);
		return D == nullptr || D->hasDmg || D->hasHpPct;
	}

	/** The recipe of a decal's contact beat (Context: circle / ring RadiusCm at the origin; cone origin + Blow; line path). */
	inline std::string_view ContactRecipe(abyss::AbilityShapeKind Kind)
	{
		switch (Kind)
		{
		case abyss::AbilityShapeKind::Cone: return "telegraph.contact.cone";
		case abyss::AbilityShapeKind::Line: return "telegraph.contact.line";
		case abyss::AbilityShapeKind::Circle:
		case abyss::AbilityShapeKind::Ring: break;
		}
		return "telegraph.contact";
	}

	/**
	 * The recipe a fuse plays when it starts (at the corpse): an affix's own on-death look `elite.<type>.death`
	 * (Scorched's smoulder and burst, Gravecold's rime pre-cue and ring: timed in the recipe to the data's delay); none for
	 * a definition's own fuse (the Cinderbloat's is its Death clip's FX_Fuse / FX_Burst notifies), a curse, or any fuse
	 * in an unthemed zone (PresentsOnDeath: chapters 2-5 and the Labyrinth keep today's look).
	 */
	inline std::string FuseStartRecipe(std::string_view Label, bool bThemedZone)
	{
		if (!PresentsOnDeath(bThemedZone) || Label.substr(0, AffixLabelPrefix.size()) != AffixLabelPrefix
			|| Label.size() <= AffixLabelPrefix.size())
		{
			return {};
		}
		std::string Out("elite.");
		Out += Label.substr(AffixLabelPrefix.size());
		Out += ".death";
		return Out;
	}

	inline constexpr std::string_view RaiseMarkerRecipe = "monster.raise.mark";
	inline constexpr std::string_view ArenaRecipe = "hazard.banner_ring";

	/**
	 * A hazard's loop (first existing): `hazard.<abilityId>`, then by shape: a line is the Breach-seep's crack
	 * (`hazard.breach_seep`, along origin -> origin + dir x length), anything else breach fire (`hazard.breach_fire`,
	 * RadiusCm). Stopped at untilMs.
	 */
	inline std::vector<std::string> HazardRecipeChain(std::string_view AbilityId, abyss::AbilityShapeKind Kind)
	{
		std::vector<std::string> Out;
		if (!AbilityId.empty())
		{
			std::string Own("hazard.");
			Own += AbilityId;
			Out.push_back(std::move(Own));
		}
		Out.push_back(Kind == abyss::AbilityShapeKind::Line ? "hazard.breach_seep" : "hazard.breach_fire");
		return Out;
	}

	/** The ground radius a recipe of the shape is scaled by (cm): the radius; a line's half width. */
	inline double RecipeRadiusCm(const abyss::AbilityShapeDef& Shape)
	{
		const double Tile = abyss::kTileSizeUU;
		return Shape.kind == abyss::AbilityShapeKind::Line ? Shape.width * 0.5 * Tile : Shape.radius * Tile;
	}

	// =================================================================================================================
	// Budget
	// =================================================================================================================

	/** Decals drawn at once per quality tier (0 low, 1 balanced, 2 high): overflow only, Chapter 1 never reaches it. */
	inline int DecalCap(int Tier, bool bMobile)
	{
		static constexpr int Desktop[3] = { 24, 32, 32 };
		static constexpr int Mobile[3] = { 16, 24, 32 };
		const int Index = (std::min)(2, (std::max)(0, Tier));
		return bMobile ? Mobile[Index] : Desktop[Index];
	}

	/**
	 * The arena's outer wash is dropped on mobile low. That only trims the quad's margin (at 9 tiles: 23.1 m -> 18.6 m
	 * across, about 35 % less area): the one translucent quad still covers the whole disc (2G review m2). Its blank
	 * inside exits M_AF_Telegraph before any texture sample, so it costs rasterisation and blending, not shading; an
	 * annulus mesh (the inside never rasterised) waits for the device profile (stage H). Count the arena as an 18.6 m
	 * square of translucency in the mobile-low overdraw budget.
	 */
	inline bool ArenaOuterWash(int Tier, bool bMobile)
	{
		return !(bMobile && Tier <= 0);
	}

	/** Priority rank (lower first): the arena, then boss > elite > normal strikes; hazard zones rank with normal ones. */
	inline int RankOf(EStyle Style)
	{
		switch (Style)
		{
		case EStyle::Arena: return 0;
		case EStyle::Boss: return 1;
		case EStyle::Elite: return 2;
		case EStyle::Normal:
		case EStyle::Hazard: break;
		}
		return 3;
	}

	struct FBudgetEntry
	{
		int Rank = 3;
		double DistSq = 0.0;       // to the hero
		double StartMs = 0.0;
		uint32_t Serial = 0;       // creation order (stable tie-break)
	};

	/**
	 * Which entries draw this frame: the Cap best by (rank, distance, start, serial); Out keeps the order of In. Order and
	 * Out are the caller's buffers (resized, never shrunk: the per-frame tick allocates nothing once a fight's peak is
	 * reached, 2G review m3).
	 */
	inline void SelectVisible(const std::vector<FBudgetEntry>& In, int Cap, std::vector<std::size_t>& Order,
		std::vector<bool>& Out)
	{
		Out.assign(In.size(), false);
		if (Cap <= 0)
		{
			return;
		}
		Order.resize(In.size());
		for (std::size_t I = 0; I < In.size(); ++I)
		{
			Order[I] = I;
		}
		std::sort(Order.begin(), Order.end(), [&In](std::size_t L, std::size_t R)
		{
			const FBudgetEntry& A = In[L];
			const FBudgetEntry& B = In[R];
			if (A.Rank != B.Rank) return A.Rank < B.Rank;
			if (A.DistSq != B.DistSq) return A.DistSq < B.DistSq;
			if (A.StartMs != B.StartMs) return A.StartMs < B.StartMs;
			return A.Serial < B.Serial;
		});
		const std::size_t Take = (std::min)(In.size(), static_cast<std::size_t>(Cap));
		for (std::size_t I = 0; I < Take; ++I)
		{
			Out[Order[I]] = true;
		}
	}

	inline std::vector<bool> SelectVisible(const std::vector<FBudgetEntry>& In, int Cap)
	{
		std::vector<std::size_t> Order;
		std::vector<bool> Out;
		SelectVisible(In, Cap, Order, Out);
		return Out;
	}

	// =================================================================================================================
	// Late views (Snapshot::telegraphs / hazards after a load or a zone build) and the dash
	// =================================================================================================================

	/** Two telegraphs are the same windup (an event and its snapshot view): caster, ability, start and origin. */
	inline bool SameTelegraph(abyss::EntityId EntityA, std::string_view AbilityA, double StartA, abyss::Vec2 OriginA,
		abyss::EntityId EntityB, std::string_view AbilityB, double StartB, abyss::Vec2 OriginB)
	{
		return EntityA == EntityB && AbilityA == AbilityB && std::fabs(StartA - StartB) < 0.5
			&& std::fabs(OriginA.x - OriginB.x) < 1e-6 && std::fabs(OriginA.y - OriginB.y) < 1e-6;
	}

	/**
	 * An ability windup the snapshot no longer lists is over without its contact (cancelled) only while the snapshot's
	 * clock is still before the contact (a contact the sim reached this frame drops the view before the render time
	 * gets there; it ends by its own timeline).
	 */
	inline bool MissingMeansCancelled(double SnapshotSimMs, double ContactMs)
	{
		return SnapshotSimMs + 0.5 < ContactMs;
	}

	/** How far before its contact an aborted strike's fade starts when the abort came at (or after) the contact. */
	inline constexpr double CancelBeforeContactMs = 0.5;

	/**
	 * Whether an EvMonsterAttackCancelled for the caster ends this windup, and from which sim ms its fade runs (< 0: not
	 * this windup's). Events are dispatched after the frame's sim steps (FrameEndMs = the snapshot's simNowMs,
	 * StepsThisFrame steps of kSimStepMs), so the abort happened at a step in (FrameEndMs - steps x step, FrameEndMs].
	 * A caster stunned in the step its contact timer fires aborts inside that timer (CombatSystem::ResolveAbility), at a
	 * sim time >= the contact (2G review m1): the windup is this one while the decal has not played its contact
	 * (bContactPlayed: the render clock reached it, the strike landed) and its contact is no earlier than one step before
	 * that window (the core allows one action at a time per monster; a strike that resolved in an earlier frame cannot be
	 * cancelled now). The fade starts at the frame's end or just before the contact, whichever is first, so a strike
	 * aborted at its contact fades from full without the flash.
	 */
	inline double CancelTimeFor(double ContactMs, bool bContactPlayed, double FrameEndMs, int StepsThisFrame)
	{
		if (bContactPlayed)
		{
			return -1.0;
		}
		const double FrameStartMs = FrameEndMs - static_cast<double>((std::max)(1, StepsThisFrame)) * abyss::kSimStepMs;
		if (ContactMs + abyss::kSimStepMs + 0.5 <= FrameStartMs)
		{
			return -1.0;
		}
		return (std::max)(0.0, (std::min)(FrameEndMs, ContactMs - CancelBeforeContactMs));
	}

	/** A late view is worth drawing when its contact (or a zone's end) is still ahead of the render time. */
	inline bool LateViewWorthDrawing(double NowMs, double ContactOrEndMs)
	{
		return NowMs < ContactOrEndMs;
	}

	/**
	 * A charge's slide (EvEntityTeleported{Dash}): the actor shows From and eases to To over DashSlideMs. The actor
	 * plays it on its dodge offset (AAbyssCharacterActor: DodgeOffset x (1 - AbyssWorldUtil::EaseOutQuad(t)), the
	 * hero's dodge slide), so only the offset and the duration are decided here.
	 */
	inline constexpr double DashSlideMs = 200.0;

	struct FDashSlide
	{
		bool bSlide = false;
		double OffsetXCm = 0.0;   // From - To (world cm): the visual offset at the start, eased to 0
		double OffsetYCm = 0.0;
		double DurationMs = 0.0;
	};

	inline FDashSlide DashSlide(abyss::Vec2 FromTile, abyss::Vec2 ToTile)
	{
		FDashSlide S;
		const double Dx = (FromTile.x - ToTile.x) * abyss::kTileSizeUU;
		const double Dy = (FromTile.y - ToTile.y) * abyss::kTileSizeUU;
		if (Dx * Dx + Dy * Dy < 1.0)
		{
			return S;   // under a centimetre: nothing to slide
		}
		S.bSlide = true;
		S.OffsetXCm = Dx;
		S.OffsetYCm = Dy;
		S.DurationMs = DashSlideMs;
		return S;
	}

}

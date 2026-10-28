// The enemy telegraph layer (Source/Abyssfire/Private/Vfx/AbyssTelegraphRules.h, dark remake U2 / 2G): the M_AF_Telegraph
// contract from the real manifest, the decal geometry against the core's ShapeContains for all four shapes (synthetic and
// every Chapter 1 ground telegraph), the strike / fuse / zone timelines, the custom primitive data, which telegraphs draw
// a decal, the recipe ids against the art's vfx_recipes.json, the budget, late views and the dash slide.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "doctest/doctest.h"

#include "abyss/base/Enums.h"
#include "abyss/base/Json.h"
#include "abyss/data/DataStore.h"
#include "abyss/data/MonsterData.h"
#include "abyss/monsters/MonsterAbilities.h"

#include "Vfx/AbyssTelegraphRules.h"
#include "World/AbyssThemeRules.h"
#include "audio_test_data.h"

namespace
{
	using namespace AbyssTelegraphRules;

	const abyss::JsonValue& ArtManifest()
	{
		static abyss::JsonValue Root;
		static bool bLoaded = false;
		if (!bLoaded)
		{
			bLoaded = true;
			const std::string Text = audio_test::ReadBytes(audio_test::UnrealDir() + "/Data/assets.json");
			if (Text.empty() || !abyss::ParseJson(Text, Root))
			{
				Root = abyss::JsonValue();
			}
		}
		const abyss::JsonValue* Manifest = Root.Find("manifest");
		return Manifest != nullptr ? *Manifest : Root;
	}

	const abyss::JsonValue& Recipes()
	{
		static abyss::JsonValue Root;
		static bool bLoaded = false;
		if (!bLoaded)
		{
			bLoaded = true;
			const std::string Text = audio_test::ReadBytes(audio_test::UnrealDir() + "/Art/Export/VFX/vfx_recipes.json");
			if (Text.empty() || !abyss::ParseJson(Text, Root))
			{
				Root = abyss::JsonValue();
			}
		}
		return Root;
	}

	bool HasRecipe(std::string_view Id)
	{
		return Recipes().Get("recipes").Has(Id) || Recipes().Get("aliases").Has(Id);
	}

	const abyss::DataStore& Data() { return audio_test::Data(); }

	abyss::AbilityShapeDef Circle(double R)
	{
		abyss::AbilityShapeDef S;
		S.kind = abyss::AbilityShapeKind::Circle;
		S.radius = R;
		return S;
	}

	abyss::AbilityShapeDef Ring(double R, double Inner)
	{
		abyss::AbilityShapeDef S;
		S.kind = abyss::AbilityShapeKind::Ring;
		S.radius = R;
		S.innerRadius = Inner;
		return S;
	}

	abyss::AbilityShapeDef Cone(double R, double Arc)
	{
		abyss::AbilityShapeDef S;
		S.kind = abyss::AbilityShapeKind::Cone;
		S.radius = R;
		S.arcDeg = Arc;
		return S;
	}

	abyss::AbilityShapeDef Line(double Length, double Width)
	{
		abyss::AbilityShapeDef S;
		S.kind = abyss::AbilityShapeKind::Line;
		S.length = Length;
		S.width = Width;
		return S;
	}

	double Reach(const abyss::AbilityShapeDef& S)
	{
		return abyss::ShapeReach(S) + 1.0;
	}

	/**
	 * Distance (tiles) from a point to the shape's boundary, by an independent construction (the arcs, the cone's two
	 * edge segments, the box's four sides): the grid skips points within a hair of it, where both tests are exact.
	 */
	double BoundaryDistance(const abyss::AbilityShapeDef& Shape, abyss::Vec2 Origin, abyss::Vec2 Dir, abyss::Vec2 P)
	{
		const abyss::Vec2 U = Dir.LengthSq() > 1e-18 ? Dir.Normalized() : abyss::Vec2(1, 0);
		const abyss::Vec2 V = P - Origin;
		const double Along = V.Dot(U);
		const double Across = U.Cross(V);
		const double R = V.Length();
		auto Segment = [](abyss::Vec2 Q, abyss::Vec2 A, abyss::Vec2 B)
		{
			const abyss::Vec2 AB = B - A;
			const double L2 = AB.LengthSq();
			const double T = L2 > 0.0 ? (std::min)(1.0, (std::max)(0.0, (Q - A).Dot(AB) / L2)) : 0.0;
			return (Q - (A + AB * T)).Length();
		};
		switch (Shape.kind)
		{
		case abyss::AbilityShapeKind::Circle:
			return std::fabs(R - Shape.radius);
		case abyss::AbilityShapeKind::Ring:
			return (std::min)(std::fabs(R - Shape.radius), Shape.innerRadius > 0.0 ? std::fabs(R - Shape.innerRadius) : 1e9);
		case abyss::AbilityShapeKind::Cone:
		{
			const double Arc = std::fabs(R - Shape.radius);
			if (Shape.arcDeg >= 360.0)
			{
				return Arc;
			}
			const double Half = Shape.arcDeg * Pi / 360.0;
			const abyss::Vec2 E1(U.x * std::cos(Half) - U.y * std::sin(Half), U.x * std::sin(Half) + U.y * std::cos(Half));
			const abyss::Vec2 E2(U.x * std::cos(Half) + U.y * std::sin(Half), -U.x * std::sin(Half) + U.y * std::cos(Half));
			return (std::min)({ Arc, Segment(P, Origin, Origin + E1 * Shape.radius), Segment(P, Origin, Origin + E2 * Shape.radius) });
		}
		case abyss::AbilityShapeKind::Line:
		{
			const double Half = Shape.width * 0.5;
			const double DA = (std::min)(std::fabs(Along), std::fabs(Along - Shape.length));
			const double DB = std::fabs(std::fabs(Across) - Half);
			const bool bAlongIn = Along >= 0.0 && Along <= Shape.length;
			const bool bAcrossIn = std::fabs(Across) <= Half;
			if (bAlongIn && bAcrossIn) return (std::min)(DA, DB);
			if (bAlongIn) return DB;
			if (bAcrossIn) return DA;
			return std::sqrt(DA * DA + DB * DB);
		}
		}
		return 1e9;
	}

	/**
	 * Samples a grid of world points around the shape and checks the decal's per-pixel inside test (W >= 0) against
	 * ShapeContains with a zero body radius (points within a hair of the boundary are skipped: both are exact there), that
	 * every inside point lies on the quad, and that the quad's UV mapping measures the same sample as the world.
	 * Returns the number of points compared.
	 */
	int CheckAgainstCore(const abyss::AbilityShapeDef& Shape, abyss::Vec2 Origin, abyss::Vec2 Dir, double OutsideCm = EdgeMarginCm)
	{
		const FGeometry G = MakeGeometry(Shape, Origin, Dir, OutsideCm);
		const abyss::Vec2 UnitDir = Dir.LengthSq() > 1e-18 ? Dir.Normalized() : abyss::Vec2(1, 0);
		const double R = Reach(Shape);
		int Compared = 0;
		int Mismatches = 0;
		int OffQuad = 0;
		int UvDisagrees = 0;
		const double Step = 0.061;   // tiles; not a divisor of the shapes' sizes
		for (double Y = -R; Y <= R; Y += Step)
		{
			for (double X = -R; X <= R; X += Step)
			{
				const abyss::Vec2 P(Origin.x + X, Origin.y + Y);
				if (BoundaryDistance(Shape, Origin, UnitDir, P) < 1e-7)
				{
					continue;   // on the boundary
				}
				const FSample S = SampleAtWorld(G, P.x * abyss::kTileSizeUU, P.y * abyss::kTileSizeUU);
				const bool bCore = abyss::ShapeContains(Shape, Origin, UnitDir, P, 0.0);
				const bool bDecal = S.W >= 0.0;
				if (bCore != bDecal)
				{
					++Mismatches;
					if (Mismatches <= 3)
					{
						CAPTURE(P.x);
						CAPTURE(P.y);
						CAPTURE(S.W);
						CHECK(bCore == bDecal);
					}
				}
				if (bDecal)
				{
					double U = 0.0;
					double V = 0.0;
					WorldToQuadUV(G, P.x * abyss::kTileSizeUU, P.y * abyss::kTileSizeUU, U, V);
					if (U < 0.0 || U > 1.0 || V < 0.0 || V > 1.0)
					{
						++OffQuad;
					}
					const FSample Q = SampleAtQuadUV(G, U, V);
					if (std::fabs(Q.W - S.W) > 1e-6 || std::fabs(Q.FillR - S.FillR) > 1e-6)
					{
						++UvDisagrees;
					}
				}
				++Compared;
			}
		}
		CHECK(Mismatches == 0);
		CHECK(OffQuad == 0);
		CHECK(UvDisagrees == 0);
		return Compared;
	}

	struct FGroundTelegraph
	{
		std::string Owner;
		std::string Id;
		abyss::AbilityShapeDef Shape;
	};

	/** Every ground telegraph Chapter 1 can draw: abilities with a ground shape (and their hazards), on-death shapes. */
	std::vector<FGroundTelegraph> ChapterOneGroundTelegraphs()
	{
		std::vector<FGroundTelegraph> Out;
		auto AddAbilities = [&Out](const std::string& Owner, const std::vector<abyss::MonsterAbilityDef>& Abilities)
		{
			for (const abyss::MonsterAbilityDef& A : Abilities)
			{
				if (abyss::AbilityHasGroundShape(A.kind) && A.hasShape)
				{
					Out.push_back({ Owner, A.id, A.shape });
				}
				if (A.hazard.present)
				{
					Out.push_back({ Owner, A.id + ".hazard", A.hazard.hasShape ? A.hazard.shape : A.shape });
				}
			}
		};
		for (const abyss::MonsterDef& Def : Data().Monsters().defs)
		{
			AddAbilities(Def.id, Def.abilities);
			if (Def.onDeath.present && Def.onDeath.kind != abyss::OnDeathKind::Spawn)
			{
				Out.push_back({ Def.id, "onDeath", Def.onDeath.shape });
			}
		}
		for (const abyss::HuntDef& Hunt : Data().Monsters().hunts)
		{
			AddAbilities(Hunt.huntId, Hunt.abilities);
		}
		const abyss::EliteAffixTable& Affixes = Data().Combat().eliteAffixes;
		for (std::size_t I = 0; I < abyss::EnumCount<abyss::EliteAffixType>(); ++I)
		{
			const abyss::EliteAffixDef& Def = Affixes.defs[I];
			if (Def.onDeath.present && Def.onDeath.kind != abyss::OnDeathKind::Spawn)
			{
				Out.push_back({ "affix", std::string("affix.") + std::string(abyss::EnumName(Def.type)), Def.onDeath.shape });
			}
		}
		return Out;
	}
}

TEST_SUITE("telegraph")
{
	TEST_CASE("the M_AF_Telegraph contract parses from the manifest to the shipped values")
	{
		const abyss::JsonValue& Materials = ArtManifest().Get("materials");
		REQUIRE(Materials.Get("M_AF_Telegraph").IsObject());
		const FContract C = ParseContract(Materials);
		CHECK(C.bFromManifest);
		CHECK(C.FillRgb == 0xFFE8DCu);
		CHECK(C.RimRgb == 0xD11F2Eu);
		CHECK(C.FrontRgb == 0xFFF4EEu);
		CHECK(C.FlashRimRgb == 0xFF3A48u);
		CHECK(C.FillGrain == doctest::Approx(0.3));
		CHECK(C.FillTileCm == doctest::Approx(160.0));
		CHECK(C.FrontAlpha == doctest::Approx(0.85));
		CHECK(C.FrontWidthCm == doctest::Approx(5.0));
		CHECK(C.RimTileCm == doctest::Approx(120.0));
		CHECK(C.FlashMs == doctest::Approx(110.0));
		CHECK(C.FadeMs == doctest::Approx(170.0));
		CHECK(C.FlashFillAlpha == doctest::Approx(0.35));
		CHECK(C.Urgency.Base == doctest::Approx(0.82));
		CHECK(C.Urgency.Amp == doctest::Approx(0.18));
		CHECK(C.Urgency.PeriodMs == doctest::Approx(180.0));

		const FStyleLook& Normal = C.Style(EStyle::Normal);
		CHECK(Normal.RimWidthCm == doctest::Approx(26.0));
		CHECK(Normal.RimAlpha == doctest::Approx(0.95));
		CHECK(Normal.FillAlphaStart == doctest::Approx(0.07));
		CHECK(Normal.FillAlphaContact == doctest::Approx(0.22));
		CHECK(Normal.bFill);
		CHECK_FALSE(Normal.InnerRim.bOn);
		CHECK(Normal.UrgencyFrom == doctest::Approx(0.7));
		CHECK_FALSE(Normal.bRimOutside);
		CHECK_FALSE(Normal.bOuterWash);
		CHECK_FALSE(Normal.bPulse);

		const FStyleLook& Elite = C.Style(EStyle::Elite);
		CHECK(Elite.RimWidthCm == doctest::Approx(30.0));
		CHECK(Elite.InnerRim.bOn);
		CHECK(Elite.InnerRim.Alpha == doctest::Approx(0.6));
		CHECK(Elite.InnerRim.AtFraction == doctest::Approx(0.82));
		CHECK(Elite.InnerRim.WidthCm == doctest::Approx(10.0));
		CHECK(Elite.UrgencyFrom == doctest::Approx(0.7));

		const FStyleLook& Boss = C.Style(EStyle::Boss);
		CHECK(Boss.RimWidthCm == doctest::Approx(36.0));
		CHECK(Boss.FillAlphaContact == doctest::Approx(0.27));
		CHECK(Boss.FillAlphaStart == doctest::Approx(0.07));
		CHECK(Boss.InnerRim.Alpha == doctest::Approx(0.7));
		CHECK(Boss.InnerRim.WidthCm == doctest::Approx(12.0));
		CHECK(Boss.UrgencyFrom == doctest::Approx(0.5));

		const FStyleLook& Hazard = C.Style(EStyle::Hazard);
		CHECK(Hazard.RimAlpha == doctest::Approx(0.75));
		CHECK_FALSE(Hazard.bFill);
		CHECK(Hazard.bPulse);
		CHECK(Hazard.Pulse.Base == doctest::Approx(0.8));
		CHECK(Hazard.Pulse.Amp == doctest::Approx(0.2));
		CHECK(Hazard.Pulse.PeriodMs == doctest::Approx(1200.0));
		CHECK_FALSE(Hazard.bRimOutside);

		const FStyleLook& Arena = C.Style(EStyle::Arena);
		CHECK(Arena.RimAlpha == doctest::Approx(0.85));
		CHECK_FALSE(Arena.bFill);
		CHECK(Arena.bRimOutside);
		CHECK(Arena.bOuterWash);
		CHECK(Arena.WashRgb == 0xD11F2Eu);
		CHECK(Arena.WashAlpha == doctest::Approx(0.1));
		CHECK(Arena.WashFadeInCm == doctest::Approx(60.0));
		CHECK(Arena.Pulse.Base == doctest::Approx(0.85));
		CHECK(Arena.Pulse.Amp == doctest::Approx(0.15));
		CHECK(Arena.Pulse.PeriodMs == doctest::Approx(1200.0));

		// The built-in copy is the contract as shipped: parsing the manifest changes no value.
		const FContract D = DefaultContract();
		CHECK_FALSE(D.bFromManifest);
		for (std::size_t I = 0; I < StyleCount; ++I)
		{
			CAPTURE(I);
			const FStyleLook& A = C.Styles[I];
			const FStyleLook& B = D.Styles[I];
			CHECK(A.RimWidthCm == doctest::Approx(B.RimWidthCm));
			CHECK(A.RimAlpha == doctest::Approx(B.RimAlpha));
			CHECK(A.FillAlphaStart == doctest::Approx(B.FillAlphaStart));
			CHECK(A.FillAlphaContact == doctest::Approx(B.FillAlphaContact));
			CHECK(A.bFill == B.bFill);
			CHECK(A.InnerRim.bOn == B.InnerRim.bOn);
			CHECK(A.InnerRim.Alpha == doctest::Approx(B.InnerRim.Alpha));
			CHECK(A.UrgencyFrom == doctest::Approx(B.UrgencyFrom));
			CHECK(A.bRimOutside == B.bRimOutside);
			CHECK(A.bOuterWash == B.bOuterWash);
			CHECK(A.WashAlpha == doctest::Approx(B.WashAlpha));
			CHECK(A.bPulse == B.bPulse);
			CHECK(A.Pulse.Base == doctest::Approx(B.Pulse.Base));
			CHECK(A.Pulse.PeriodMs == doctest::Approx(B.Pulse.PeriodMs));
		}
	}

	TEST_CASE("contract parsing: pulses, colours, numbers; a missing or partial contract keeps the shipped values")
	{
		FPulse P;
		CHECK(ParsePulse("x (0.85 + 0.15 sin(2 pi t / 1.2 s))", P));
		CHECK(P.Base == doctest::Approx(0.85));
		CHECK(P.Amp == doctest::Approx(0.15));
		CHECK(P.PeriodMs == doctest::Approx(1200.0));
		CHECK(P.At(0.0) == doctest::Approx(0.85));
		CHECK(P.At(300.0) == doctest::Approx(1.0));
		CHECK(P.At(900.0) == doctest::Approx(0.7));
		FPulse Kept{ 0.5, 0.25, 10.0 };
		CHECK_FALSE(ParsePulse("a steady glow", Kept));
		CHECK_FALSE(ParsePulse("x (0.8 + 0.2 sin(3 pi t / 1.2 s))", Kept));   // not 2 pi
		CHECK(Kept.Base == 0.5);
		const std::vector<double> N = Detail::Numbers("over the last 30 % of .5 and 12.25");
		REQUIRE(N.size() == 3);
		CHECK(N[0] == 30.0);
		CHECK(N[1] == doctest::Approx(0.5));
		CHECK(N[2] == doctest::Approx(12.25));
		uint32_t Rgb = 7;
		CHECK(ParseHexRgb("#D11F2E", Rgb));
		CHECK(Rgb == 0xD11F2Eu);
		CHECK(ParseHexRgb("ff3a48", Rgb));
		CHECK(Rgb == 0xFF3A48u);
		CHECK_FALSE(ParseHexRgb("#FFF", Rgb));
		CHECK_FALSE(ParseHexRgb("#GG0000", Rgb));
		CHECK(Rgb == 0xFF3A48u);

		// No contract: the built-in copy.
		const FContract None = ParseContract(abyss::JsonValue());
		CHECK_FALSE(None.bFromManifest);
		CHECK(None.Style(EStyle::Boss).RimWidthCm == 36.0);
		// A partial contract: the fields it names, the rest shipped.
		const FContract Partial = ParseContract(audio_test::Json(R"J({"M_AF_Telegraph": {
			"params": {"RimWidthCm": 20, "FillAlphaContact": 0.3, "RimColor": "#112233"},
			"urgency": "over the last 40 % of the windup x (0.5 + 0.5 sin(2 pi t / 0.25 s))",
			"styles": {"elite": {"innerRim": {"alpha": 0.4}}, "hazard": {"fill": true, "RimAlpha": 0.5},
			           "arena": {"rimSide": "inside", "pulse": "steady"}}}})J"));
		CHECK(Partial.bFromManifest);
		CHECK(Partial.RimRgb == 0x112233u);
		CHECK(Partial.FillRgb == 0xFFE8DCu);
		CHECK(Partial.Style(EStyle::Normal).RimWidthCm == 20.0);
		CHECK(Partial.Style(EStyle::Normal).FillAlphaContact == doctest::Approx(0.3));
		CHECK(Partial.Style(EStyle::Normal).UrgencyFrom == doctest::Approx(0.6));
		CHECK(Partial.Urgency.PeriodMs == doctest::Approx(250.0));
		CHECK(Partial.Style(EStyle::Elite).RimWidthCm == 20.0);   // no style override: params
		CHECK(Partial.Style(EStyle::Elite).InnerRim.Alpha == doctest::Approx(0.4));
		CHECK(Partial.Style(EStyle::Elite).InnerRim.AtFraction == doctest::Approx(0.82));
		CHECK(Partial.Style(EStyle::Boss).FillAlphaContact == doctest::Approx(0.3));   // the boss's own .27 is a style key
		CHECK(Partial.Style(EStyle::Hazard).bFill);
		CHECK(Partial.Style(EStyle::Hazard).RimAlpha == 0.5);
		CHECK(Partial.Style(EStyle::Hazard).bPulse);   // no pulse key: the shipped pulse
		CHECK_FALSE(Partial.Style(EStyle::Arena).bRimOutside);
		CHECK(Partial.Style(EStyle::Arena).Pulse.Base == doctest::Approx(0.85));   // an unparsable pulse keeps the shipped one
	}

	TEST_CASE("geometry: the decal's inside test is the core's ShapeContains (body radius 0) for all four shapes")
	{
		const abyss::Vec2 Origins[] = { abyss::Vec2(10.5, 20.25), abyss::Vec2(0.0, 0.0), abyss::Vec2(77.3, 3.9) };
		const abyss::Vec2 Dirs[] = { abyss::Vec2(1, 0), abyss::Vec2(0, 1), abyss::Vec2(-0.6, 0.8), abyss::Vec2(0.31, -0.95),
			abyss::Vec2(0, 0) };
		const abyss::AbilityShapeDef Shapes[] = {
			Circle(1.8), Circle(2.2), Circle(0.75),
			Ring(3.0, 1.5), Ring(9.0, 8.4), Ring(2.0, 0.0),
			Cone(2.6, 120.0), Cone(3.0, 45.0), Cone(2.0, 200.0), Cone(2.5, 360.0), Cone(1.5, 90.0),
			Line(8.0, 1.0), Line(5.0, 1.2), Line(1.0, 3.0),
		};
		int Total = 0;
		for (const abyss::AbilityShapeDef& Shape : Shapes)
		{
			CAPTURE(abyss::EnumName(Shape.kind));
			CAPTURE(Shape.radius);
			CAPTURE(Shape.arcDeg);
			CAPTURE(Shape.length);
			for (const abyss::Vec2& Origin : Origins)
			{
				for (const abyss::Vec2& Dir : Dirs)
				{
					Total += CheckAgainstCore(Shape, Origin, Dir);
				}
			}
		}
		CHECK(Total > 100000);
	}

	TEST_CASE("geometry: the quad, its yaw and margins; boundaries and the fill coordinate")
	{
		// A circle: centred, square, the margin past the radius.
		const FGeometry C = MakeGeometry(Circle(2.2), abyss::Vec2(10, 20), abyss::Vec2(0, 1), EdgeMarginCm);
		CHECK(C.Shape == EShape::Circle);
		CHECK(C.ACm == doctest::Approx(220.0));
		CHECK(C.CentreXCm == doctest::Approx(1000.0));
		CHECK(C.CentreYCm == doctest::Approx(2000.0));
		CHECK(C.SizeXCm == doctest::Approx(2.0 * (220.0 + EdgeMarginCm)));
		CHECK(C.SizeYCm == doctest::Approx(C.SizeXCm));
		CHECK(C.YawDeg == doctest::Approx(90.0));
		CHECK(C.LocalOriginXCm == doctest::Approx(0.0));
		CHECK(C.FillMaxCm == doctest::Approx(220.0));
		// The quad's edge midpoints lie the margin past the boundary.
		CHECK(SampleAtQuadUV(C, 1.0, 0.5).W == doctest::Approx(-EdgeMarginCm));
		CHECK(SampleAtQuadUV(C, 0.5, 0.0).W == doctest::Approx(-EdgeMarginCm));
		CHECK(SampleAtQuadUV(C, 0.5, 0.5).W == doctest::Approx(220.0));
		CHECK(SampleAtWorld(C, 1100.0, 2000.0).FillR == doctest::Approx(100.0));   // the fill grows from the centre
		CHECK(SampleAtWorld(C, 1000.0, 2000.0).FillR == doctest::Approx(0.0));
		// On the boundary W = 0 (inside, like the core's edge), just outside < 0.
		CHECK(SampleAtWorld(C, 1220.0, 2000.0).W == doctest::Approx(0.0));
		CHECK(SampleAtWorld(C, 1220.5, 2000.0).W < 0.0);
		CHECK(abyss::ShapeContains(Circle(2.2), abyss::Vec2(10, 20), abyss::Vec2(1, 0), abyss::Vec2(12.2, 20), 0.0));

		// A line: the box from the origin along the direction; the quad's centre half way, the origin at its back edge.
		const FGeometry L = MakeGeometry(Line(8.0, 1.0), abyss::Vec2(15, 95), abyss::Vec2(1, 0), EdgeMarginCm);
		CHECK(L.Shape == EShape::Line);
		CHECK(L.ACm == doctest::Approx(800.0));
		CHECK(L.B == doctest::Approx(50.0));
		CHECK(L.CentreXCm == doctest::Approx(1500.0 + 400.0));
		CHECK(L.CentreYCm == doctest::Approx(9500.0));
		CHECK(L.SizeXCm == doctest::Approx(800.0 + 2.0 * EdgeMarginCm));
		CHECK(L.SizeYCm == doctest::Approx(100.0 + 2.0 * EdgeMarginCm));
		CHECK(L.LocalOriginXCm == doctest::Approx(-400.0));
		CHECK(L.InnerRimRefCm == doctest::Approx(50.0));
		const FSample Mid = SampleAtWorld(L, 1900.0, 9500.0);
		CHECK(Mid.W == doctest::Approx(50.0));
		CHECK(Mid.FillR == doctest::Approx(400.0));
		// A true signed distance outside too (the edge's anti-aliasing): the quad's edges lie the margin out.
		CHECK(SampleAtQuadUV(L, 1.0, 0.5).W == doctest::Approx(-EdgeMarginCm));
		CHECK(SampleAtQuadUV(L, 0.0, 0.5).W == doctest::Approx(-EdgeMarginCm));
		CHECK(SampleAtQuadUV(L, 0.5, 0.0).W == doctest::Approx(-EdgeMarginCm));
		CHECK(SampleAtWorld(L, 2310.0, 9560.0).W == doctest::Approx(-std::sqrt(200.0)));   // past a corner
		// The far end is square (the core's box, not the contract's rounded end): a corner point is inside.
		CHECK(SampleAtWorld(L, 2299.0, 9549.0).W >= 0.0);
		CHECK(abyss::ShapeContains(Line(8.0, 1.0), abyss::Vec2(15, 95), abyss::Vec2(1, 0), abyss::Vec2(22.99, 95.49), 0.0));

		// A cone of 120 degrees: the box from just behind the apex to the radius.
		const FGeometry K = MakeGeometry(Cone(2.6, 120.0), abyss::Vec2(0, 0), abyss::Vec2(0, -1), EdgeMarginCm);
		CHECK(K.Shape == EShape::Cone);
		CHECK(K.B == doctest::Approx(Pi / 3.0));
		CHECK(K.SizeXCm == doctest::Approx(260.0 + 2.0 * EdgeMarginCm));   // apex .. radius (cos 60 > 0: nothing behind)
		CHECK(K.SizeYCm == doctest::Approx(2.0 * 260.0 * std::sin(Pi / 3.0) + 2.0 * EdgeMarginCm));
		CHECK(K.YawDeg == doctest::Approx(-90.0));
		CHECK(SampleAtWorld(K, 0.0, -270.0).W == doctest::Approx(-10.0));   // on the axis past the radius
		CHECK(SampleAtWorld(K, 0.0, -100.0).W == doctest::Approx(100.0 * std::sin(Pi / 3.0)));   // to the edges
		CHECK(SampleAtWorld(K, 0.0, 50.0).W == doctest::Approx(-50.0));   // behind the apex
		// A cone past 180 degrees reaches behind the apex; a full one is a disc.
		const FGeometry Wide = MakeGeometry(Cone(2.0, 200.0), abyss::Vec2(0, 0), abyss::Vec2(1, 0), 0.0);
		CHECK(Wide.SizeXCm == doctest::Approx(200.0 + 200.0 * std::sin(Pi / 18.0)));   // reaches cos(100 deg) behind
		CHECK(Wide.SizeYCm == doctest::Approx(400.0));
		const FGeometry Full = MakeGeometry(Cone(2.5, 400.0), abyss::Vec2(0, 0), abyss::Vec2(1, 0), 0.0);
		CHECK(Full.B == doctest::Approx(Pi));
		CHECK(Full.SizeXCm == doctest::Approx(500.0));
		CHECK(SampleAtWorld(Full, -100.0, 1.0).W > 0.0);

		// A ring: its width is the fill's run; a ring without a hole is a circle.
		const FGeometry R = MakeGeometry(Ring(3.0, 1.5), abyss::Vec2(5, 5), abyss::Vec2(1, 0), EdgeMarginCm);
		CHECK(R.Shape == EShape::Ring);
		CHECK(R.B == doctest::Approx(150.0));
		CHECK(R.FillMaxCm == doctest::Approx(150.0));
		CHECK(SampleAtWorld(R, 500.0, 500.0).W < 0.0);   // the hole
		CHECK(SampleAtWorld(R, 725.0, 500.0).W == doctest::Approx(75.0));
		CHECK(SampleAtWorld(R, 725.0, 500.0).FillR == doctest::Approx(75.0));
		CHECK(MakeGeometry(Ring(2.0, 0.0), abyss::Vec2(0, 0), abyss::Vec2(1, 0), 0.0).Shape == EShape::Circle);
		CHECK(MakeGeometry(Ring(2.0, 2.5), abyss::Vec2(0, 0), abyss::Vec2(1, 0), 0.0).Shape == EShape::Circle);

		// A zero direction faces +X (the core's AbilityUnitOr fallback).
		const FGeometry Z = MakeGeometry(Line(2.0, 1.0), abyss::Vec2(1, 1), abyss::Vec2(0, 0), 0.0);
		CHECK(Z.DirX == 1.0);
		CHECK(Z.DirY == 0.0);
		CHECK(Z.YawDeg == 0.0);

		// The rim's arc length: a quarter round a circle is a quarter of its circumference.
		CHECK(SampleAtWorld(C, 1000.0, 2219.0).EdgeU == doctest::Approx(0.0));
		CHECK(std::fabs(SampleAtWorld(C, 1219.0, 2000.0).EdgeU - SampleAtWorld(C, 1000.0, 2219.0).EdgeU)
			== doctest::Approx(220.0 * Pi / 2.0));

		// The arena's quad reaches past the boundary by its outward rim, and by its wash band where the wash is drawn.
		const FContract Contract = DefaultContract();
		const FStyleLook& Arena = Contract.Style(EStyle::Arena);
		CHECK(OutsideReachCm(Arena, true) == doctest::Approx(ArenaWashBandCm + EdgeMarginCm));
		CHECK(OutsideReachCm(Arena, false) == doctest::Approx(EdgeMarginCm + Arena.RimWidthCm));
		CHECK(OutsideReachCm(Contract.Style(EStyle::Normal), true) == doctest::Approx(EdgeMarginCm));
		const FGeometry Ring9 = MakeGeometry(Circle(9.0), abyss::Vec2(15, 95), abyss::Vec2(1, 0), OutsideReachCm(Arena, true));
		CHECK(Ring9.SizeXCm == doctest::Approx(2.0 * (900.0 + ArenaWashBandCm + EdgeMarginCm)));
		CHECK(CheckAgainstCore(Circle(9.0), abyss::Vec2(15, 95), abyss::Vec2(1, 0), OutsideReachCm(Arena, true)) > 0);
	}

	TEST_CASE("every Chapter 1 ground telegraph: the decal agrees with ShapeContains")
	{
		const std::vector<FGroundTelegraph> All = ChapterOneGroundTelegraphs();
		std::set<std::string> Ids;
		for (const FGroundTelegraph& T : All)
		{
			Ids.insert(T.Owner + ":" + T.Id);
		}
		// The roster's telegraphs (2F): the slam, the sweep, the brand and its hazard, the seep, the lunge, Keen, the
		// bloat's burst and the two affixes' on-death shapes.
		for (const char* Expected : { "bonegorger:clapper_slam", "valdras:banner_sweep", "valdras:oath_brand",
				 "valdras:oath_brand.hazard", "valdras:breach_seep", "valdras:breach_seep.hazard", "valdras:lunge",
				 "miniboss_corpseherd:keen", "cinderbloat:onDeath", "affix:affix.fire_enhanced", "affix:affix.frozen" })
		{
			CHECK_MESSAGE(Ids.count(Expected) == 1, Expected);
		}
		const abyss::Vec2 Dirs[] = { abyss::Vec2(1, 0), abyss::Vec2(-0.28, 0.96) };
		for (const FGroundTelegraph& T : All)
		{
			CAPTURE(T.Owner);
			CAPTURE(T.Id);
			for (const abyss::Vec2& Dir : Dirs)
			{
				CHECK(CheckAgainstCore(T.Shape, abyss::Vec2(15.0, 95.0), Dir) > 0);
			}
		}
	}

	TEST_CASE("strike timeline: fill to contact, the urgency window, the flash, the fade")
	{
		const FContract C = DefaultContract();
		const double Start = 10000.0;
		const double Contact = 10800.0;   // the Bonegorger's 800 ms slam
		CHECK_FALSE(StrikeState(C, EStyle::Normal, Start - 1.0, Start, Contact).bVisible);
		const FState S0 = StrikeState(C, EStyle::Normal, Start, Start, Contact);
		CHECK(S0.bVisible);
		CHECK(S0.Phase == EPhase::Windup);
		CHECK(S0.Progress == 0.0);
		CHECK(S0.FillAlpha == doctest::Approx(0.07));
		CHECK(S0.FrontAlpha == doctest::Approx(0.85));
		CHECK(S0.RimAlpha == doctest::Approx(0.95));
		CHECK(S0.Flash == 0.0);
		const FState Half = StrikeState(C, EStyle::Normal, Start + 400.0, Start, Contact);
		CHECK(Half.Progress == doctest::Approx(0.5));
		CHECK(Half.FillAlpha == doctest::Approx(0.07 + 0.15 * 0.5));
		CHECK(Half.RimAlpha == doctest::Approx(0.95));   // no pulse before the last 30 %
		// The urgency pulse: from 70 % (560 ms) to contact, x (.82 + .18 sin(2 pi t / 180 ms)) on the sim clock.
		const double T1 = Start + 0.7 * 800.0;
		CHECK(StrikeState(C, EStyle::Normal, T1 - 0.01, Start, Contact).RimAlpha == doctest::Approx(0.95));
		const FState U = StrikeState(C, EStyle::Normal, T1 + 45.0, Start, Contact);
		CHECK(U.RimAlpha == doctest::Approx(0.95 * (0.82 + 0.18 * std::sin(2.0 * Pi * (T1 + 45.0) / 180.0))));
		double Lo = 1.0;
		double Hi = 0.0;
		for (double T = T1; T < Contact; T += 1.0)
		{
			const double A = StrikeState(C, EStyle::Normal, T, Start, Contact).RimAlpha;
			Lo = (std::min)(Lo, A);
			Hi = (std::max)(Hi, A);
		}
		CHECK(Lo == doctest::Approx(0.95 * 0.64).epsilon(0.01));
		CHECK(Hi == doctest::Approx(0.95).epsilon(0.01));
		// Bosses pulse from half the windup.
		CHECK(StrikeState(C, EStyle::Boss, Start + 399.0, Start, Contact).RimAlpha == doctest::Approx(0.95));
		CHECK(InUrgency(C.Style(EStyle::Boss), 0.5, Start + 400.0, Contact));
		CHECK_FALSE(InUrgency(C.Style(EStyle::Normal), 0.5, Start + 400.0, Contact));
		CHECK(StrikeState(C, EStyle::Boss, Start + 799.0, Start, Contact).FillAlpha == doctest::Approx(0.07 + 0.20 * 799.0 / 800.0));
		// The fill reaches the rim exactly at the contact: the flash for 110 ms (the front colour at .35, the rim red).
		const FState F = StrikeState(C, EStyle::Normal, Contact, Start, Contact);
		CHECK(F.Phase == EPhase::Flash);
		CHECK(F.Progress == 1.0);
		CHECK(F.Flash == 1.0);
		CHECK(F.FillAlpha == doctest::Approx(0.35));
		CHECK(F.FrontAlpha == doctest::Approx(0.35));
		CHECK(F.RimAlpha == doctest::Approx(0.95));
		CHECK(StrikeState(C, EStyle::Normal, Contact + 110.0, Start, Contact).Phase == EPhase::Flash);
		// Then the fade over 170 ms (the fill at full, no flash, no pulse), gone at contact + 280 ms.
		const FState Fd = StrikeState(C, EStyle::Normal, Contact + 110.0 + 85.0, Start, Contact);
		CHECK(Fd.Phase == EPhase::Fade);
		CHECK(Fd.Flash == 0.0);
		CHECK(Fd.FillAlpha == doctest::Approx(0.22 * 0.5));
		CHECK(Fd.FrontAlpha == doctest::Approx(0.85 * 0.5));
		CHECK(Fd.RimAlpha == doctest::Approx(0.95 * 0.5));
		CHECK(StrikeState(C, EStyle::Normal, Contact + 280.0, Start, Contact).Phase == EPhase::Done);
		CHECK_FALSE(StrikeState(C, EStyle::Normal, Contact + 280.0, Start, Contact).bVisible);
		// Progress edge cases.
		CHECK(Progress(5.0, 10.0, 20.0) == 0.0);
		CHECK(Progress(25.0, 10.0, 20.0) == 1.0);
		CHECK(Progress(10.0, 10.0, 10.0) == 1.0);
		CHECK(Progress(9.0, 10.0, 10.0) == 0.0);
	}

	TEST_CASE("a cancelled windup fades from where it stood; a cancel after the contact changes nothing")
	{
		const FContract C = DefaultContract();
		const FState Before = StrikeState(C, EStyle::Elite, 1300.0, 1000.0, 1600.0, 1400.0);
		CHECK(Before.Phase == EPhase::Windup);
		const FState At = StrikeState(C, EStyle::Elite, 1400.0, 1000.0, 1600.0, 1400.0);
		CHECK(At.Phase == EPhase::Fade);
		CHECK(At.Progress == doctest::Approx(400.0 / 600.0));
		CHECK(At.Flash == 0.0);
		CHECK(At.RimAlpha == doctest::Approx(0.95));
		const FState Later = StrikeState(C, EStyle::Elite, 1485.0, 1000.0, 1600.0, 1400.0);
		CHECK(Later.Progress == doctest::Approx(400.0 / 600.0));   // frozen at the cancel
		CHECK(Later.RimAlpha == doctest::Approx(0.95 * 0.5));
		CHECK(Later.FillAlpha == doctest::Approx((0.07 + 0.15 * 400.0 / 600.0) * 0.5));
		CHECK(StrikeState(C, EStyle::Elite, 1570.0, 1000.0, 1600.0, 1400.0).Phase == EPhase::Done);
		// Never a flash after a cancel, even past the old contact.
		CHECK(StrikeState(C, EStyle::Elite, 1600.0, 1000.0, 1600.0, 1400.0).Flash == 0.0);
		// A "cancel" at or after the contact is no cancel (the strike landed).
		CHECK(StrikeState(C, EStyle::Elite, 1650.0, 1000.0, 1600.0, 1600.0).Phase == EPhase::Flash);
	}

	TEST_CASE("a cancel event: which windup it ends and from when (a stun in the contact step gives no flash)")
	{
		const double Step = abyss::kSimStepMs;
		const FContract C = DefaultContract();
		// Before the contact: the fade runs from the frame's end, as before.
		CHECK(CancelTimeFor(1800.0, false, 1000.0, 1) == doctest::Approx(1000.0));
		// A stun in the step the contact timer fires: the core aborts inside the timer (ResolveAbility), the event comes
		// with the snapshot at or past the contact. Still this windup's: it fades from just before the contact.
		const double AtContact = CancelTimeFor(1000.0, false, 1000.0 + Step * 0.6, 1);
		CHECK(AtContact == doctest::Approx(1000.0 - CancelBeforeContactMs));
		CHECK(CancelTimeFor(1000.0, false, 1000.0, 1) == doctest::Approx(1000.0 - CancelBeforeContactMs));
		for (double Now = 990.0; Now <= 1300.0; Now += 5.0)
		{
			CAPTURE(Now);
			const FState S = StrikeState(C, EStyle::Boss, Now, 400.0, 1000.0, AtContact);
			CHECK(S.Flash == 0.0);
			CHECK((S.Phase == EPhase::Windup || S.Phase == EPhase::Fade || S.Phase == EPhase::Done));
			if (Now >= 1000.0 && Now < 1100.0)
			{
				CHECK(S.Phase == EPhase::Fade);
				CHECK(S.Progress == doctest::Approx(1.0).epsilon(0.01));   // it fades from full
			}
		}
		// A frame of several steps that ran past the contact (the abort in any of them): still this windup's.
		CHECK(CancelTimeFor(1000.0, false, 1000.0 + 3.5 * Step, 4) == doctest::Approx(1000.0 - CancelBeforeContactMs));
		CHECK(CancelTimeFor(1000.0, false, 1000.0 + 1.5 * Step, 0) == doctest::Approx(1000.0 - CancelBeforeContactMs));
		// Not this windup's: the render clock already played its contact (it landed), or it resolved in an earlier frame
		// (its contact more than a step before this frame's first step).
		CHECK(CancelTimeFor(1000.0, true, 1000.0 + Step * 0.6, 1) < 0.0);
		CHECK(CancelTimeFor(1000.0, false, 1000.0 + 3.0 * Step, 1) < 0.0);
		CHECK(CancelTimeFor(1000.0, false, 1100.0, 1) < 0.0);
		CHECK(CancelTimeFor(1000.0, false, 1000.0 + 6.0 * Step, 4) < 0.0);
	}

	TEST_CASE("zones: a hazard pulses until its untilMs then fades; the arena rim, its wash and its fade")
	{
		const FContract C = DefaultContract();
		// Breach-seep: 1000 ms of ground from its contact.
		CHECK_FALSE(ZoneState(C, EStyle::Hazard, 999.0, 1000.0, 2000.0).bVisible);
		const FState H = ZoneState(C, EStyle::Hazard, 1300.0, 1000.0, 2000.0);
		CHECK(H.Phase == EPhase::Zone);
		CHECK(H.RimAlpha == doctest::Approx(0.75 * (0.8 + 0.2 * std::sin(2.0 * Pi * 1300.0 / 1200.0))));
		CHECK(H.FillAlpha == 0.0);
		CHECK(H.Flash == 0.0);
		CHECK(ZoneState(C, EStyle::Hazard, 2000.0, 1000.0, 2000.0).Phase == EPhase::Zone);
		const FState Hf = ZoneState(C, EStyle::Hazard, 2085.0, 1000.0, 2000.0);
		CHECK(Hf.Phase == EPhase::Fade);
		CHECK(Hf.RimAlpha == doctest::Approx(0.75 * (0.8 + 0.2 * std::sin(2.0 * Pi * 2085.0 / 1200.0)) * 0.5));
		CHECK(ZoneState(C, EStyle::Hazard, 2170.0, 1000.0, 2000.0).Phase == EPhase::Done);
		// The arena: open-ended until cleared, its wash at .1, then the fade.
		const FState A = ZoneState(C, EStyle::Arena, 50000.0, 1000.0, -1.0);
		CHECK(A.Phase == EPhase::Zone);
		CHECK(A.FillAlpha == doctest::Approx(0.1));
		CHECK(A.RimAlpha == doctest::Approx(0.85 * (0.85 + 0.15 * std::sin(2.0 * Pi * 50000.0 / 1200.0))));
		const FState Ac = ZoneState(C, EStyle::Arena, 60085.0, 1000.0, 60000.0);
		CHECK(Ac.Phase == EPhase::Fade);
		CHECK(Ac.FillAlpha == doctest::Approx(0.05));
		CHECK(ZoneState(C, EStyle::Arena, 60200.0, 1000.0, 60000.0).Phase == EPhase::Done);
	}

	TEST_CASE("custom primitive data: the layout the material reads")
	{
		// Five float4 groups (SetCustomPrimitiveDataVector4 at 0, 4, 8, 12, 16), under UE's 36-float cap.
		CHECK(Cpd::Count == 20);
		CHECK(Cpd::Count % 4 == 0);
		const int Slots[] = { Cpd::Shape, Cpd::A, Cpd::B, Cpd::RimWidthCm, Cpd::SizeXCm, Cpd::SizeYCm, Cpd::LocalOriginXCm,
			Cpd::LocalOriginYCm, Cpd::InnerRimAlpha, Cpd::InnerRimInsetCm, Cpd::InnerRimWidthCm, Cpd::WashBandCm, Cpd::FillOn,
			Cpd::RimOutside, Cpd::Flash, Cpd::Progress, Cpd::FillAlpha, Cpd::FrontAlpha, Cpd::RimAlpha, Cpd::Spare };
		std::set<int> Unique(std::begin(Slots), std::end(Slots));
		CHECK(Unique.size() == static_cast<std::size_t>(Cpd::Count));
		CHECK(*Unique.begin() == 0);
		CHECK(*Unique.rbegin() == Cpd::Count - 1);

		const FContract C = DefaultContract();
		const FGeometry G = MakeGeometry(Line(5.0, 1.2), abyss::Vec2(15, 95), abyss::Vec2(0.6, 0.8), EdgeMarginCm);
		const FState S = StrikeState(C, EStyle::Boss, 1300.0, 1000.0, 1800.0);   // before the boss's urgency (half)
		const std::array<float, Cpd::Count> V = PackCpd(G, C.Style(EStyle::Boss), S, true);
		CHECK(V[Cpd::Shape] == 3.f);
		CHECK(V[Cpd::A] == doctest::Approx(500.0));
		CHECK(V[Cpd::B] == doctest::Approx(60.0));
		CHECK(V[Cpd::RimWidthCm] == doctest::Approx(36.0));
		CHECK(V[Cpd::SizeXCm] == doctest::Approx(500.0 + 2.0 * EdgeMarginCm));
		CHECK(V[Cpd::LocalOriginXCm] == doctest::Approx(-250.0));
		CHECK(V[Cpd::InnerRimAlpha] == doctest::Approx(0.7));
		CHECK(V[Cpd::InnerRimInsetCm] == doctest::Approx((1.0 - 0.82) * 60.0));   // lines: the half width
		CHECK(V[Cpd::InnerRimWidthCm] == doctest::Approx(12.0));
		CHECK(V[Cpd::WashBandCm] == 0.f);
		CHECK(V[Cpd::FillOn] == 1.f);
		CHECK(V[Cpd::RimOutside] == 0.f);
		CHECK(V[Cpd::Progress] == doctest::Approx(0.375));
		CHECK(V[Cpd::FillAlpha] == doctest::Approx(0.07 + 0.2 * 0.375));
		CHECK(V[Cpd::FrontAlpha] == doctest::Approx(0.85));
		CHECK(V[Cpd::RimAlpha] == doctest::Approx(0.95));
		CHECK(V[Cpd::Spare] == 0.f);
		// A normal strike has no inner rim.
		CHECK(PackCpd(G, C.Style(EStyle::Normal), S, true)[Cpd::InnerRimAlpha] == 0.f);
		// The arena: rim outside, its wash band (dropped where the tier drops the wash), no fill, no front band.
		const FGeometry Ring9 = MakeGeometry(Circle(9.0), abyss::Vec2(15, 95), abyss::Vec2(1, 0), 0.0);
		const FState Z = ZoneState(C, EStyle::Arena, 5000.0, 1000.0, -1.0);
		const std::array<float, Cpd::Count> AW = PackCpd(Ring9, C.Style(EStyle::Arena), Z, true);
		CHECK(AW[Cpd::RimOutside] == 1.f);
		CHECK(AW[Cpd::FillOn] == 0.f);
		CHECK(AW[Cpd::WashBandCm] == doctest::Approx(ArenaWashBandCm));
		CHECK(AW[Cpd::FillAlpha] == doctest::Approx(0.1));
		CHECK(AW[Cpd::FrontAlpha] == 0.f);
		const std::array<float, Cpd::Count> AN = PackCpd(Ring9, C.Style(EStyle::Arena), Z, false);
		CHECK(AN[Cpd::WashBandCm] == 0.f);
		CHECK(AN[Cpd::FillAlpha] == 0.f);
		// A hazard zone: rim only.
		const std::array<float, Cpd::Count> HZ = PackCpd(G, C.Style(EStyle::Hazard), ZoneState(C, EStyle::Hazard, 1100.0, 1000.0, 2000.0), true);
		CHECK(HZ[Cpd::FillOn] == 0.f);
		CHECK(HZ[Cpd::FillAlpha] == 0.f);
		CHECK(HZ[Cpd::RimAlpha] > 0.f);
	}

	TEST_CASE("custom primitive data sends: a group goes when it moved since it was last sent, a slow drift too")
	{
		REQUIRE(CpdGroups == 5);
		std::array<float, Cpd::Count> Cache{};
		bool bValid = false;
		std::array<float, Cpd::Count> Values{};
		for (std::size_t I = 0; I < Values.size(); ++I)
		{
			Values[I] = static_cast<float>(I) * 0.25f;
		}
		// The first frame sends every group.
		std::array<bool, CpdGroups> Send = UpdateCpdCache(Cache, bValid, Values);
		CHECK(bValid);
		for (const bool B : Send)
		{
			CHECK(B);
		}
		CHECK(Cache == Values);
		// Nothing moved: nothing goes.
		Send = UpdateCpdCache(Cache, bValid, Values);
		for (const bool B : Send)
		{
			CHECK_FALSE(B);
		}
		// One value of group 3 (progress) moves: only that group goes.
		Values[Cpd::Progress] += 0.01f;
		Send = UpdateCpdCache(Cache, bValid, Values);
		CHECK(Send == std::array<bool, CpdGroups>{ false, false, false, true, false });
		CHECK(Cache[Cpd::Progress] == Values[Cpd::Progress]);
		// A slow drift (a pulse near its peak, a long fade at a high frame rate): each frame under the tolerance, the
		// group goes once the drift since the last send exceeds it, and the cache is what was sent (not the last frame).
		const float Sent = Values[Cpd::RimAlpha];
		int Sends = 0;
		for (int Frame = 1; Frame <= 40; ++Frame)
		{
			Values[Cpd::RimAlpha] = Sent - 0.0001f * static_cast<float>(Frame);
			Send = UpdateCpdCache(Cache, bValid, Values);
			const bool bAlpha = Send[static_cast<std::size_t>(Cpd::RimAlpha / 4)];
			Sends += bAlpha ? 1 : 0;
			CHECK(std::fabs(Cache[Cpd::RimAlpha] - Values[Cpd::RimAlpha]) <= CpdTolerance);   // the GPU never lags more
			CHECK_FALSE(Send[0]);
		}
		CHECK(Sends >= 6);   // 4 mm of alpha in .5 mm steps: about every 6th frame
		CHECK(Sends <= 8);
		// An invalidated cache (the quad was released and acquired again) sends everything.
		bValid = false;
		Send = UpdateCpdCache(Cache, bValid, Values);
		for (const bool B : Send)
		{
			CHECK(B);
		}
	}

	TEST_CASE("which telegraphs draw a decal: every strike (Keen too), a fuse only when it damages, raises the marker")
	{
		using abyss::AbilityKind;
		using abyss::TelegraphSource;
		for (const AbilityKind K : { AbilityKind::Circle, AbilityKind::Cone, AbilityKind::Line, AbilityKind::Ring, AbilityKind::Hazard })
		{
			CHECK(TelegraphLook(K, TelegraphSource::Ability, false, true) == ELook::Decal);
			CHECK(TelegraphLook(K, TelegraphSource::Ability, false, false) == ELook::Decal);   // strikes are never theme-gated
		}
		CHECK(TelegraphLook(AbilityKind::Raise, TelegraphSource::Ability, false, true) == ELook::RaiseMarker);
		CHECK(TelegraphLook(AbilityKind::Raise, TelegraphSource::Ability, false, false) == ELook::RaiseMarker);
		CHECK(TelegraphLook(AbilityKind::Summon, TelegraphSource::Ability, false, true) == ELook::RaiseMarker);
		for (const AbilityKind K : { AbilityKind::Bolt, AbilityKind::Buff, AbilityKind::Call })
		{
			CHECK(TelegraphLook(K, TelegraphSource::Ability, true, true) == ELook::None);
		}
		CHECK(TelegraphLook(AbilityKind::Circle, TelegraphSource::OnDeath, true, true) == ELook::Decal);
		CHECK(TelegraphLook(AbilityKind::Ring, TelegraphSource::OnDeath, false, true) == ELook::None);
		// An unthemed zone (chapters 2-5, the Labyrinth) presents no on-death fuse at all, damaging or not (2G review M1).
		CHECK(PresentsOnDeath(true));
		CHECK_FALSE(PresentsOnDeath(false));
		CHECK(TelegraphLook(AbilityKind::Circle, TelegraphSource::OnDeath, true, false) == ELook::None);
		CHECK(TelegraphLook(AbilityKind::Ring, TelegraphSource::OnDeath, false, false) == ELook::None);

		// The real data: Keen deals nothing (status only) and still draws its decal (it is an ability strike).
		const abyss::MonsterDef* Herd = Data().FindMonster("miniboss_corpseherd");
		REQUIRE(Herd != nullptr);
		bool bKeen = false;
		for (const abyss::MonsterAbilityDef& A : Herd->abilities)
		{
			if (A.id == "keen")
			{
				bKeen = true;
				CHECK(A.dmgMul == 0.0);
				CHECK(A.status.present);
				CHECK(TelegraphLook(A.kind, TelegraphSource::Ability, false, true) == ELook::Decal);
			}
			if (A.kind == AbilityKind::Raise)
			{
				CHECK(TelegraphLook(A.kind, TelegraphSource::Ability, false, true) == ELook::RaiseMarker);
			}
		}
		CHECK(bKeen);
		// On-death fuses: the Cinderbloat's burst and Scorched's damage, Gravecold's ring only slows (no decal).
		const abyss::EliteAffixTable& Affixes = Data().Combat().eliteAffixes;
		const abyss::MonsterDef* Bloat = Data().FindMonster("cinderbloat");
		REQUIRE(Bloat != nullptr);
		CHECK(FindOnDeathDef("onDeath", Bloat, &Affixes) == &Bloat->onDeath);
		CHECK(OnDeathDamages("onDeath", Bloat, &Affixes));
		CHECK(OnDeathDamages("affix.fire_enhanced", nullptr, &Affixes));
		REQUIRE(FindOnDeathDef("affix.frozen", nullptr, &Affixes) != nullptr);
		CHECK_FALSE(OnDeathDamages("affix.frozen", nullptr, &Affixes));
		CHECK(Affixes.Def(abyss::EliteAffixType::Frozen).onDeath.status.present);
		// Unknown effects draw (better a decal too many than an invisible burst): a curse, an unknown affix or definition.
		CHECK(FindOnDeathDef("curse.volatile", Bloat, &Affixes) == nullptr);
		CHECK(OnDeathDamages("curse.volatile", Bloat, &Affixes));
		CHECK(OnDeathDamages("affix.nonsense", Bloat, &Affixes));
		CHECK(OnDeathDamages("onDeath", nullptr, &Affixes));
		CHECK(OnDeathDamages("affix.frozen", nullptr, nullptr));
		// A percent of the dying monster's life damages too (the Labyrinth's volatile curse shape on an affix).
		abyss::EliteAffixTable Volatile = Affixes;
		abyss::MonsterOnDeathDef& Frost = Volatile.defs[static_cast<std::size_t>(abyss::EliteAffixType::Frozen)].onDeath;
		Frost.hasHpPct = true;
		Frost.hpPct = 12.0;
		CHECK(OnDeathDamages("affix.frozen", nullptr, &Volatile));
		// Every elite affix with an on-death effect in the shipped table: damage -> decal, a status only -> none.
		for (std::size_t I = 0; I < abyss::EnumCount<abyss::EliteAffixType>(); ++I)
		{
			const abyss::EliteAffixDef& Def = Affixes.defs[I];
			if (!Def.onDeath.present)
			{
				continue;
			}
			const std::string Label = std::string("affix.") + std::string(abyss::EnumName(Def.type));
			CAPTURE(Label);
			CHECK(OnDeathDamages(Label, nullptr, &Affixes) == (Def.onDeath.hasDmg || Def.onDeath.hasHpPct));
		}
		CHECK(StyleOf(abyss::TelegraphStyle::Normal) == EStyle::Normal);
		CHECK(StyleOf(abyss::TelegraphStyle::Elite) == EStyle::Elite);
		CHECK(StyleOf(abyss::TelegraphStyle::Boss) == EStyle::Boss);
	}

	TEST_CASE("recipe ids: contact, fuse, hazard chains, the marker and the ring exist in the art's vfx_recipes.json")
	{
		REQUIRE(Recipes().Get("recipes").IsObject());
		CHECK(ContactRecipe(abyss::AbilityShapeKind::Circle) == "telegraph.contact");
		CHECK(ContactRecipe(abyss::AbilityShapeKind::Ring) == "telegraph.contact");
		CHECK(ContactRecipe(abyss::AbilityShapeKind::Cone) == "telegraph.contact.cone");
		CHECK(ContactRecipe(abyss::AbilityShapeKind::Line) == "telegraph.contact.line");
		for (const abyss::AbilityShapeKind K : { abyss::AbilityShapeKind::Circle, abyss::AbilityShapeKind::Ring,
				 abyss::AbilityShapeKind::Cone, abyss::AbilityShapeKind::Line })
		{
			CHECK_MESSAGE(HasRecipe(ContactRecipe(K)), ContactRecipe(K));
		}
		CHECK(HasRecipe(RaiseMarkerRecipe));
		CHECK(HasRecipe(ArenaRecipe));
		CHECK(HasRecipe("hazard.breach_fire"));
		CHECK(HasRecipe("hazard.breach_seep"));
		// The fuse's start recipe: an affix's own; nothing for a definition's own fuse or a curse, nor in an unthemed zone.
		CHECK(FuseStartRecipe("affix.fire_enhanced", true) == "elite.fire_enhanced.death");
		CHECK(FuseStartRecipe("affix.frozen", true) == "elite.frozen.death");
		CHECK(FuseStartRecipe("onDeath", true).empty());
		CHECK(FuseStartRecipe("curse.volatile", true).empty());
		CHECK(FuseStartRecipe("affix.", true).empty());
		CHECK(FuseStartRecipe("affix.fire_enhanced", false).empty());
		CHECK(FuseStartRecipe("affix.frozen", false).empty());
		const abyss::EliteAffixTable& Affixes = Data().Combat().eliteAffixes;
		for (std::size_t I = 0; I < abyss::EnumCount<abyss::EliteAffixType>(); ++I)
		{
			const abyss::EliteAffixDef& Def = Affixes.defs[I];
			if (Def.onDeath.present)
			{
				const std::string Id = FuseStartRecipe(std::string("affix.") + std::string(abyss::EnumName(Def.type)), true);
				CHECK_MESSAGE(HasRecipe(Id), Id);
			}
		}
		// The hazard chain: the ability's own recipe first, then by shape.
		const std::vector<std::string> Seep = HazardRecipeChain("breach_seep", abyss::AbilityShapeKind::Line);
		REQUIRE(Seep.size() == 2);
		CHECK(Seep[0] == "hazard.breach_seep");
		CHECK(Seep[1] == "hazard.breach_seep");
		const std::vector<std::string> Brand = HazardRecipeChain("oath_brand", abyss::AbilityShapeKind::Circle);
		CHECK(Brand[0] == "hazard.oath_brand");
		CHECK(Brand[1] == "hazard.breach_fire");
		CHECK(HazardRecipeChain("", abyss::AbilityShapeKind::Ring) == std::vector<std::string>{ "hazard.breach_fire" });
		// Every Chapter 1 hazard resolves to an existing recipe.
		for (const abyss::MonsterDef& Def : Data().Monsters().defs)
		{
			for (const abyss::MonsterAbilityDef& A : Def.abilities)
			{
				if (!A.hazard.present)
				{
					continue;
				}
				const abyss::AbilityShapeDef& Shape = A.hazard.hasShape ? A.hazard.shape : A.shape;
				bool bAny = false;
				for (const std::string& Id : HazardRecipeChain(A.id, Shape.kind))
				{
					bAny = bAny || HasRecipe(Id);
				}
				CHECK_MESSAGE(bAny, A.id);
			}
		}
		CHECK(RecipeRadiusCm(Circle(1.8)) == doctest::Approx(180.0));
		CHECK(RecipeRadiusCm(Line(8.0, 1.0)) == doctest::Approx(50.0));
	}

	TEST_CASE("budget: the arena first, boss > elite > normal, nearest the hero first; caps per tier")
	{
		CHECK(RankOf(EStyle::Arena) < RankOf(EStyle::Boss));
		CHECK(RankOf(EStyle::Boss) < RankOf(EStyle::Elite));
		CHECK(RankOf(EStyle::Elite) < RankOf(EStyle::Normal));
		CHECK(RankOf(EStyle::Hazard) == RankOf(EStyle::Normal));
		std::vector<FBudgetEntry> In = {
			{ RankOf(EStyle::Normal), 1.0, 0.0, 1 },
			{ RankOf(EStyle::Normal), 25.0, 0.0, 2 },
			{ RankOf(EStyle::Elite), 400.0, 0.0, 3 },
			{ RankOf(EStyle::Arena), 900.0, 0.0, 4 },
			{ RankOf(EStyle::Normal), 1.0, -5.0, 5 },
			{ RankOf(EStyle::Boss), 100.0, 0.0, 6 },
		};
		const std::vector<bool> Three = SelectVisible(In, 3);
		CHECK(Three == std::vector<bool>{ false, false, true, true, false, true });
		const std::vector<bool> Five = SelectVisible(In, 5);
		CHECK(Five == std::vector<bool>{ true, false, true, true, true, true });   // the older of two equals first
		CHECK(SelectVisible(In, 10) == std::vector<bool>(6, true));
		CHECK(SelectVisible(In, 0) == std::vector<bool>(6, false));
		CHECK(SelectVisible({}, 4).empty());
		// Ties on everything but the serial: creation order.
		const std::vector<FBudgetEntry> Ties = { { 3, 4.0, 1.0, 9 }, { 3, 4.0, 1.0, 2 } };
		CHECK(SelectVisible(Ties, 1) == std::vector<bool>{ false, true });
		// The per-frame form reuses the caller's buffers (stale contents, any size) and gives the same answer; once
		// they have grown to a frame's peak a smaller frame allocates nothing (2G review m3).
		std::vector<std::size_t> Order = { 42, 7 };
		std::vector<bool> Out(9, true);
		SelectVisible(In, 3, Order, Out);
		CHECK(Out == Three);
		const std::size_t* OrderData = Order.data();
		const std::size_t OrderCapacity = Order.capacity();
		SelectVisible(Ties, 1, Order, Out);
		CHECK(Out == std::vector<bool>{ false, true });
		CHECK(Order.data() == OrderData);
		CHECK(Order.capacity() == OrderCapacity);
		SelectVisible(In, 0, Order, Out);
		CHECK(Out == std::vector<bool>(6, false));
		SelectVisible({}, 4, Order, Out);
		CHECK(Out.empty());
		// Caps: never below what a Chapter 1 fight draws at once (overflow only).
		CHECK(DecalCap(0, false) == 24);
		CHECK(DecalCap(1, false) == 32);
		CHECK(DecalCap(2, false) == 32);
		CHECK(DecalCap(0, true) == 16);
		CHECK(DecalCap(1, true) == 24);
		CHECK(DecalCap(2, true) == 32);
		CHECK(DecalCap(-3, true) == 16);
		CHECK(DecalCap(9, false) == 32);
		CHECK_FALSE(ArenaOuterWash(0, true));
		CHECK(ArenaOuterWash(1, true));
		CHECK(ArenaOuterWash(0, false));
	}

	TEST_CASE("late views: the same windup, a missing view is a cancel only before its contact")
	{
		CHECK(SameTelegraph(7, "lunge", 1000.0, abyss::Vec2(1, 2), 7, "lunge", 1000.2, abyss::Vec2(1, 2)));
		CHECK_FALSE(SameTelegraph(7, "lunge", 1000.0, abyss::Vec2(1, 2), 8, "lunge", 1000.0, abyss::Vec2(1, 2)));
		CHECK_FALSE(SameTelegraph(7, "lunge", 1000.0, abyss::Vec2(1, 2), 7, "banner_sweep", 1000.0, abyss::Vec2(1, 2)));
		CHECK_FALSE(SameTelegraph(7, "muster", 1000.0, abyss::Vec2(1, 2), 7, "muster", 1000.0, abyss::Vec2(1, 3)));   // two corpses
		CHECK_FALSE(SameTelegraph(7, "lunge", 1000.0, abyss::Vec2(1, 2), 7, "lunge", 9000.0, abyss::Vec2(1, 2)));
		CHECK(MissingMeansCancelled(1000.0, 1800.0));
		CHECK_FALSE(MissingMeansCancelled(1800.0, 1800.0));   // the sim reached the contact this frame
		CHECK_FALSE(MissingMeansCancelled(1810.0, 1800.0));
		CHECK(LateViewWorthDrawing(1000.0, 1100.0));
		CHECK_FALSE(LateViewWorthDrawing(1100.0, 1100.0));
	}

	TEST_CASE("the dash: a 200 ms slide from where the charge started")
	{
		const FDashSlide S = DashSlide(abyss::Vec2(15.0, 95.0), abyss::Vec2(19.0, 92.0));
		CHECK(S.bSlide);
		CHECK(S.OffsetXCm == doctest::Approx(-400.0));
		CHECK(S.OffsetYCm == doctest::Approx(300.0));
		CHECK(S.DurationMs == DashSlideMs);
		CHECK(DashSlideMs == 200.0);
		CHECK_FALSE(DashSlide(abyss::Vec2(3.0, 3.0), abyss::Vec2(3.0, 3.001)).bSlide);   // a dash stopped at its start
		// The ease is the actor's dodge offset (AbyssWorldUtil::EaseOutQuad in AAbyssCharacterActor), not decided here.
	}

	TEST_CASE("Chapters 2-5 and the Labyrinth look as today: only Chapter 1 definitions telegraph, no fuse is presented")
	{
		const abyss::ZoneMonsterList* Plains = Data().Monsters().ZoneList("emerald_plains");
		REQUIRE(Plains != nullptr);
		const abyss::MiniBossEntry* Herd = Data().Monsters().MiniBossFor("emerald_plains");
		REQUIRE(Herd != nullptr);
		for (const abyss::MonsterDef& Def : Data().Monsters().defs)
		{
			bool bTelegraphs = Def.onDeath.present && Def.onDeath.kind != abyss::OnDeathKind::Spawn;
			for (const abyss::MonsterAbilityDef& A : Def.abilities)
			{
				bTelegraphs = bTelegraphs || abyss::AbilityHasGroundShape(A.kind) || abyss::AbilityKindSummons(A.kind);
			}
			if (!bTelegraphs)
			{
				continue;
			}
			CAPTURE(Def.id);
			const bool bChapterOne = std::find(Plains->monsterIds.begin(), Plains->monsterIds.end(), Def.id) != Plains->monsterIds.end()
				|| Def.id == Herd->monsterId;
			CHECK(bChapterOne);
		}
		// The elite affixes are one table for every zone: Scorched / Gravecold elites burst / ring wherever they roll
		// (the 2B core), and the Labyrinth's volatile curse bursts every corpse on its floors (`curse.volatile`). Only a
		// zone with a presentation theme (emerald_plains' march) presents a fuse; every other map and the Labyrinth's
		// floors (no map data) keep today's look: no decal, no `elite.<type>.death` (2G review M1, DR16 (9)).
		const abyss::EliteAffixTable& Affixes = Data().Combat().eliteAffixes;
		CHECK(Affixes.Def(abyss::EliteAffixType::FireEnhanced).onDeath.present);
		CHECK(Affixes.Def(abyss::EliteAffixType::FireEnhanced).families.empty());
		std::vector<std::string> Labels = { "curse.volatile", "onDeath" };
		for (std::size_t I = 0; I < abyss::EnumCount<abyss::EliteAffixType>(); ++I)
		{
			if (Affixes.defs[I].onDeath.present)
			{
				Labels.push_back(std::string("affix.") + std::string(abyss::EnumName(Affixes.defs[I].type)));
			}
		}
		REQUIRE(Labels.size() >= 4);   // the curse, a definition's own, Scorched, Gravecold
		auto Presented = [&Affixes](const std::string& Label, bool bThemedZone)
		{
			const bool bDamages = OnDeathDamages(Label, nullptr, &Affixes);
			return TelegraphLook(abyss::AbilityKind::Circle, abyss::TelegraphSource::OnDeath, bDamages, bThemedZone) != ELook::None
				|| !FuseStartRecipe(Label, bThemedZone).empty();
		};
		std::size_t Unthemed = 0;
		for (const abyss::MapDef& M : Data().World().maps)
		{
			CAPTURE(M.id);
			const bool bThemedZone = !AbyssThemeRules::ThemeOf(&M).empty();
			CHECK(bThemedZone == (M.id == "emerald_plains"));
			Unthemed += bThemedZone ? 0 : 1;
			for (const std::string& Label : Labels)
			{
				CAPTURE(Label);
				if (!bThemedZone)
				{
					CHECK_FALSE(Presented(Label, false));
				}
			}
		}
		CHECK(Unthemed >= 5);   // chapters 2-5 and the Ember Tower
		const bool bFloorThemed = !AbyssThemeRules::ThemeOf(Data().FindMap("abyss_labyrinth_floor")).empty();
		CHECK_FALSE(bFloorThemed);   // a Labyrinth floor has no map data
		CHECK_FALSE(Presented("curse.volatile", bFloorThemed));
		CHECK_FALSE(Presented("affix.fire_enhanced", bFloorThemed));
		// In Chapter 1 a damaging fuse draws: Scorched (decal + its recipe), the volatile shape (fail visible), Gravecold
		// its recipe alone (DR5-slow).
		CHECK(TelegraphLook(abyss::AbilityKind::Circle, abyss::TelegraphSource::OnDeath,
			OnDeathDamages("affix.fire_enhanced", nullptr, &Affixes), true) == ELook::Decal);
		CHECK(FuseStartRecipe("affix.fire_enhanced", true) == "elite.fire_enhanced.death");
		CHECK(TelegraphLook(abyss::AbilityKind::Circle, abyss::TelegraphSource::OnDeath,
			OnDeathDamages("curse.volatile", nullptr, &Affixes), true) == ELook::Decal);
		CHECK(TelegraphLook(abyss::AbilityKind::Ring, abyss::TelegraphSource::OnDeath,
			OnDeathDamages("affix.frozen", nullptr, &Affixes), true) == ELook::None);
		CHECK(FuseStartRecipe("affix.frozen", true) == "elite.frozen.death");
	}
}

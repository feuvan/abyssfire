// The presentation rules of a hand-authored sub-dungeon (dark remake Wave 3 / U6: the Pit of the Faithful, sub_rf_pit;
// implementation-plan.md 3D notes 1-42, chapter1.md 9.2, DECISIONS DR17, Art/Export/VFX/vfx_recipes.json `dark.pit`).
//
// * Walls: a themed wall block whose art says `placement.yawStepDeg` turns in steps of that angle (the crypt blocks abut
//   face to face); inside a hand-authored sub-dungeon (CutFromRock) only a wall tile that touches an open tile gets a
//   block (the rooms are cut out of solid rock: 240 of the Pit's 1020 wall tiles, 237 placed - three stand behind the
//   stair), the rest is void (black terrain, no mesh). The generated caves keep the surface's rules.
// * The stair: the exit piece faces the floor - the walkable neighbour of the exit tile that has the wall behind it.
// * The entrance piece on the surface: the `_open` art while the entrance is unsealed, its `_sealed` twin while sealed.
// * The light-radius grade and the cleanse (`dark.<theme>`): the ambient the materials multiply by, the hero's pool,
//   and on EvAreaCleansed the sequence - lamps lit nearest first LampStepMs apart from LampFirstMs, the mood and the
//   ambient going to the cleansed ones over GradeMs, the niche's glow and its marks - with its end state (a visit to a
//   cleansed sub-dungeon shows everything lit at once).
// * The stagger the core leaves to the presentation: a dormant rank's Rise along its line (LineRanks). The bodies
//   that fall at the cleanse are NOT staggered (3D note 36 is open: they fall on the core's Death events, in one frame).
// * Small state rules: the spent guardian wears no elite marker, the world map shows the parent zone, an objective's
//   own zone decides what the minimap marks, the scene label of the smoothness measurement.
//
// UE-free and header-only (std + AbyssCore public headers), tested in unreal/PresentationTests/tests/subdungeon_test.cpp.
// The UE glue (AAbyssZoneActor, AbyssSetPieces.cpp, UAbyssWorldBuilder, the HUD, the autotest) converts at the
// boundary and keeps no rule of its own.
#pragma once

#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/Json.h"
#include "abyss/base/Types.h"
#include "abyss/data/MapData.h"

namespace AbyssSubDungeonRules
{
	// =================================================================================================================
	// Walls and the stair
	// =================================================================================================================

	/**
	 * The yaw of a wall tile's block. Without a step (every outcrop of the surface): the hashed 0-359 degrees
	 * AAbyssZoneActor::BuildWalls always used, `(Hash >> 8) % 360`. With the art's `placement.yawStepDeg` (the crypt
	 * blocks: 90): `(Hash >> 8) % (360 / step) * step` - for 90, `(Hash >> 8) % 4 * 90`. A step of a full turn or
	 * more means "never turn" (yaw 0, like every step above 180); a step below MinWallYawStepDeg is no step.
	 */
	inline constexpr double MinWallYawStepDeg = 1.0;
	inline float WallYawDeg(uint32_t Hash, double YawStepDeg)
	{
		const uint32_t Bits = Hash >> 8;
		if (YawStepDeg >= 360.0)
		{
			return 0.f;   // a full turn or more: the block never turns
		}
		if (!(YawStepDeg >= MinWallYawStepDeg))
		{
			return static_cast<float>(Bits % 360u);   // no step (0, negative, NaN, below a degree): the free hashed yaw
		}
		const uint32_t Steps = static_cast<uint32_t>(360.0 / YawStepDeg + 1.0e-6);   // whole steps in a turn (at most 360)
		if (Steps <= 1u)
		{
			return 0.f;
		}
		return static_cast<float>(static_cast<double>(Bits % Steps) * YawStepDeg);
	}

	/**
	 * Whether a wall tile touches an open tile: one of its eight neighbours is inside the grid and is not a wall
	 * (walkable ground or water). IsWall(col, row) is asked for in-grid tiles only.
	 */
	template <class IsWallFnT>
	bool WallTouchesOpen(int Cols, int Rows, int Col, int Row, IsWallFnT&& IsWall)
	{
		for (int Dr = -1; Dr <= 1; ++Dr)
		{
			for (int Dc = -1; Dc <= 1; ++Dc)
			{
				const int C = Col + Dc;
				const int R = Row + Dr;
				if ((Dc == 0 && Dr == 0) || C < 0 || R < 0 || C >= Cols || R >= Rows)
				{
					continue;
				}
				if (!IsWall(C, R))
				{
					return true;
				}
			}
		}
		return false;
	}

	/** A wall tile gets a block always, except in a zone cut from rock (CutFromRock): there only where it touches an
	 *  open tile. */
	inline bool BuildsWallBlock(bool bCutFromRock, bool bTouchesOpen)
	{
		return !bCutFromRock || bTouchesOpen;
	}

	/**
	 * The underground zone rules - void rock, a block only where a wall tile touches an open tile, no skirt, the exit a
	 * stair that faces the floor - belong to a hand-authored sub-dungeon (SubDungeonDef::hasMap: the Pit's rooms are cut
	 * out of solid rock, its crypt blocks abut face to face). The generated caves (sub_dwarf_mine, sub_demon_altar:
	 * organic outcrops with a free yaw) keep the surface's rules: every outcrop, the hills, the grove ring.
	 */
	inline bool CutFromRock(bool bInsideSubDungeon, bool bHandAuthored)
	{
		return bInsideSubDungeon && bHandAuthored;
	}

	struct FFloorSide
	{
		bool bFound = false;
		int Dc = 0;
		int Dr = 0;
	};

	/**
	 * The side of an exit tile its stair faces: the 4-neighbour that is walkable and has a blocked tile opposite (the
	 * wall the stair climbs into). Sides are tried -row, -col, +row, +col (the key-lit and the shade side of the W1
	 * camera first), so a tile open on two sides gives the same answer on every build. Without such a side: the first
	 * walkable neighbour in that order; none: not found (the caller keeps its own facing).
	 */
	template <class IsWalkableFnT>
	FFloorSide ExitFloorSide(int Col, int Row, IsWalkableFnT&& IsWalkable)
	{
		static constexpr int Sides[4][2] = { { 0, -1 }, { -1, 0 }, { 0, 1 }, { 1, 0 } };
		FFloorSide Out;
		for (const int* Side : Sides)
		{
			if (IsWalkable(Col + Side[0], Row + Side[1]) && !IsWalkable(Col - Side[0], Row - Side[1]))
			{
				Out.bFound = true;
				Out.Dc = Side[0];
				Out.Dr = Side[1];
				return Out;
			}
		}
		for (const int* Side : Sides)
		{
			if (IsWalkable(Col + Side[0], Row + Side[1]))
			{
				Out.bFound = true;
				Out.Dc = Side[0];
				Out.Dr = Side[1];
				return Out;
			}
		}
		return Out;
	}

	/** The facing (tile space: 0 = +col, 90 = +row, the camp props' convention) of a front turned toward (Dc, Dr). */
	inline double FacingDeg(int Dc, int Dr)
	{
		constexpr double RadToDeg = 57.29577951308232;
		return std::atan2(static_cast<double>(Dr), static_cast<double>(Dc)) * RadToDeg;
	}

	/**
	 * The actor yaw of a wall overlay whose front (mesh -Y) faces the floor neighbour (Dc, Dr), as the art kit and the
	 * core's set-piece placement give it: atan2(-Dc, Dr) in [0, 360). FacingDeg + the mesh yaw offset (-90) is the same
	 * angle; the tests pin the two against each other and against abyss::ExpandSetPieces.
	 */
	inline double OverlayYawDeg(int Dc, int Dr)
	{
		constexpr double RadToDeg = 57.29577951308232;
		double Deg = std::atan2(static_cast<double>(-Dc), static_cast<double>(Dr)) * RadToDeg;
		if (Deg < 0.0)
		{
			Deg += 360.0;
		}
		return Deg >= 360.0 ? Deg - 360.0 : Deg;
	}

	// =================================================================================================================
	// The entrance piece and twins
	// =================================================================================================================

	inline constexpr std::string_view OpenSuffix = "_open";
	inline constexpr std::string_view SealedSuffix = "_sealed";

	inline bool EndsWith(std::string_view Text, std::string_view Suffix)
	{
		return Text.size() >= Suffix.size() && Text.substr(Text.size() - Suffix.size()) == Suffix;
	}

	/**
	 * The art ids of a piece anchored on a sub-dungeon entrance: unsealed, the chain as authored (the open mouth, the
	 * sealed grate as its fallback); sealed, the `_open` ids give way - each is replaced by its `_sealed` twin - so a
	 * sealed entrance never shows the open art. Ids are never duplicated.
	 */
	inline std::vector<std::string> EntranceIdChain(const std::vector<std::string>& Chain, bool bSealed)
	{
		std::vector<std::string> Out;
		auto Add = [&Out](std::string Id)
		{
			for (const std::string& Have : Out)
			{
				if (Have == Id)
				{
					return;
				}
			}
			Out.push_back(std::move(Id));
		};
		for (const std::string& Id : Chain)
		{
			if (bSealed && EndsWith(Id, OpenSuffix))
			{
				Add(Id.substr(0, Id.size() - OpenSuffix.size()) + std::string(SealedSuffix));
			}
			else
			{
				Add(Id);
			}
		}
		return Out;
	}

	// =================================================================================================================
	// The light-radius grade and the cleanse (vfx_recipes.json dark.<theme>)
	// =================================================================================================================

	/** A light of the grade: colour x gain x clamp(1 - d / radiusTiles)^power on the ground distance d. */
	struct FGradeLight
	{
		double Color[3] = { 1.0, 1.0, 1.0 };
		double RadiusTiles = 0.0;
		double Power = 1.0;
		double Gain = 0.0;

		bool IsOn() const { return RadiusTiles > 0.0 && Gain > 0.0; }
	};

	struct FCleanseSpec
	{
		/** `dark.<theme>` exists and carries a light radius: the zone is graded. */
		bool bValid = false;
		// ---- lightRadius
		double Ambient[3] = { 1.0, 1.0, 1.0 };          // the uncleansed ambient (the materials multiply by it)
		double Ceil = 1.0;                              // min(ambient + pools, Ceil)
		FGradeLight Hero;
		// ---- cleanse
		bool bHasCleanse = false;
		double AmbientCleansed[3] = { 1.0, 1.0, 1.0 };
		double GradeMs = 6000.0;
		double LampFirstMs = 600.0;
		double LampStepMs = 420.0;
		double NicheAtMs = 600.0;
		double MarkStepMs = 180.0;
		/** Where LampFirstMs / NicheAtMs came from: a numeric field (`cleanse.lampFirstMs`, `cleanse.nicheAtMs`), the
		 *  sequence's step that opens with the words (the shipped contract), or neither (the defaults above: the glue
		 *  warns, the shipped-data test fails). */
		bool bLampFirstRead = false;
		bool bNicheAtRead = false;
		FGradeLight Lamp;
		std::string MoodFrom;     // "pit"
		std::string MoodTo;       // "pit_cleansed"
		std::string LampUnlit;    // "setpiece_pit_lamp"
		std::string LampLit;      // "setpiece_pit_lamp_lit"
	};

	// The recipes and sockets of the sequence (vfx_recipes.json dark.pit.cleanse.sequence names them in prose; the ids
	// are the recipes' own).
	inline constexpr std::string_view EmbersRecipe = "prop.pit_cleanse.embers";
	inline constexpr std::string_view LampLightRecipe = "prop.pit_lamp.light";
	inline constexpr std::string_view LampFlameRecipe = "prop.pit_lamp.flame";
	inline constexpr std::string_view NicheRecipe = "prop.pit_cleanse.niche";
	inline constexpr std::string_view MarkRecipe = "prop.pit_cleanse.mark";
	inline constexpr std::string_view FlameSocket = "flame";
	inline constexpr std::string_view LightSocket = "light";
	inline constexpr std::string_view NicheSocket = "niche";
	inline constexpr std::string_view MarkSocketPrefix = "mark_";
	inline constexpr std::string_view ExitFxSocket = "fx_vortex";
	/** The niche's glow plays when the niche is this near the hero ("if the Flame Niche is on screen": the W1 frame is
	 *  about 26 x 15 tiles, so 7 tiles of ground distance is inside it in every direction). A cleanse further away
	 *  holds the glow and the marks until the hero first comes this near (AdvanceCleanse). */
	inline constexpr double NicheViewTiles = 7.0;

	inline bool NicheInView(abyss::Vec2 Niche, abyss::Vec2 Hero)
	{
		const double Dx = Niche.x - Hero.x;
		const double Dy = Niche.y - Hero.y;
		return Dx * Dx + Dy * Dy <= NicheViewTiles * NicheViewTiles;
	}

	inline void ReadTriple(const abyss::JsonValue& Value, double Out[3])
	{
		if (Value.IsArray() && Value.Size() == 3)
		{
			for (size_t I = 0; I < 3; ++I)
			{
				Out[I] = Value.At(I).AsDouble(Out[I]);
			}
		}
	}

	inline constexpr double MinLightPower = 0.01;

	inline FGradeLight ReadLight(const abyss::JsonValue& Value)
	{
		FGradeLight Out;
		if (!Value.IsObject())
		{
			return Out;
		}
		ReadTriple(Value.Get("color"), Out.Color);
		Out.RadiusTiles = Value.Get("radiusTiles").AsDouble(0.0);
		// A power at or below 0 would lift the pool to its full gain at the rim; the shader and the reference see one value.
		const double Power = Value.Get("power").AsDouble(1.0);
		Out.Power = Power >= MinLightPower ? Power : MinLightPower;
		Out.Gain = Value.Get("gain").AsDouble(0.0);
		return Out;
	}

	/** The first word of a `lampSwap` entry ("setpiece_pit_lamp (SM_Pit_NicheLamp)" -> "setpiece_pit_lamp"). */
	inline std::string FirstWord(std::string_view Text)
	{
		const size_t End = Text.find(' ');
		return std::string(End == std::string_view::npos ? Text : Text.substr(0, End));
	}

	/** A sequence step is "about" a subject when its sentence opens with it: the words start within this many characters
	 *  ("the lamps light ...", "if the Flame Niche ..."). A step that only mentions them later is another step. */
	inline constexpr size_t SequenceSubjectChars = 8;

	/**
	 * The `atMs` of the first sequence step whose `do` sentence opens with Words (SequenceSubjectChars). The contract's
	 * sequence is prose; this is the fallback for a contract without the numeric fields. False without such a step.
	 */
	inline bool SequenceAtMs(const abyss::JsonValue& Sequence, std::string_view Words, double& Out)
	{
		for (const abyss::JsonValue& Step : Sequence.Items())
		{
			const size_t At = Step.Get("do").AsString().find(Words);
			if (At != std::string_view::npos && At <= SequenceSubjectChars && Step.Get("atMs").IsNumber())
			{
				Out = Step.Get("atMs").AsDouble(Out);
				return true;
			}
		}
		return false;
	}

	/** A moment of the cleanse: the numeric field `cleanse.<Field>` when the contract has it, else the sequence's step. */
	inline bool ReadCleanseMs(const abyss::JsonValue& Cleanse, std::string_view Field, std::string_view Words, double& Out)
	{
		if (Cleanse.Get(Field).IsNumber())
		{
			Out = Cleanse.Get(Field).AsDouble(Out);
			return true;
		}
		return SequenceAtMs(Cleanse.Get("sequence"), Words, Out);
	}

	/** Parses vfx_recipes.json `dark.<theme>` (the object itself). A value without `lightRadius` is not valid. */
	inline FCleanseSpec ParseCleanseSpec(const abyss::JsonValue& Dark)
	{
		FCleanseSpec Out;
		const abyss::JsonValue& Radius = Dark.Get("lightRadius");
		if (!Radius.IsObject())
		{
			return Out;
		}
		Out.bValid = true;
		ReadTriple(Radius.Get("ambient"), Out.Ambient);
		Out.Ceil = Radius.Get("ceil").AsDouble(1.0);
		Out.Hero = ReadLight(Radius.Get("hero"));
		for (size_t I = 0; I < 3; ++I)
		{
			Out.AmbientCleansed[I] = Out.Ambient[I];
		}
		const abyss::JsonValue& Cleanse = Dark.Get("cleanse");
		if (Cleanse.IsObject())
		{
			Out.bHasCleanse = true;
			const abyss::JsonValue& Grade = Cleanse.Get("grade");
			ReadTriple(Grade.Get("from").Get("ambient"), Out.Ambient);
			ReadTriple(Grade.Get("to").Get("ambient"), Out.AmbientCleansed);
			Out.MoodFrom = std::string(Grade.Get("from").Get("mood").AsString());
			Out.MoodTo = std::string(Grade.Get("to").Get("mood").AsString());
			Out.GradeMs = Grade.Get("overMs").AsDouble(Out.GradeMs);
			Out.Lamp = ReadLight(Grade.Get("lamp"));
			Out.LampStepMs = Cleanse.Get("lampStepMs").AsDouble(Out.LampStepMs);
			Out.bLampFirstRead = ReadCleanseMs(Cleanse, "lampFirstMs", "lamps light", Out.LampFirstMs);
			Out.bNicheAtRead = ReadCleanseMs(Cleanse, "nicheAtMs", "Flame Niche", Out.NicheAtMs);
			Out.MarkStepMs = Cleanse.Get("markStepMs").AsDouble(Out.MarkStepMs);
		}
		const abyss::JsonValue& Swap = Dark.Get("lampSwap");
		Out.LampUnlit = FirstWord(Swap.Get("unlit").AsString());
		Out.LampLit = FirstWord(Swap.Get("lit").AsString());
		return Out;
	}

	/**
	 * The order the lamps light in: nearest the cleanse's `pos` first (squared tile distance), equal distances in
	 * placement order. Out[k] = the index of the lamp that lights k-th.
	 */
	inline std::vector<int> LampOrder(const std::vector<abyss::Vec2>& Lamps, abyss::Vec2 From)
	{
		std::vector<int> Order(Lamps.size());
		std::vector<double> Dist(Lamps.size());
		for (size_t I = 0; I < Lamps.size(); ++I)
		{
			Order[I] = static_cast<int>(I);
			const double Dx = Lamps[I].x - From.x;
			const double Dy = Lamps[I].y - From.y;
			Dist[I] = Dx * Dx + Dy * Dy;
		}
		// insertion sort (a dozen lamps): stable, no <algorithm> macro hazards under the UE shim
		for (size_t I = 1; I < Order.size(); ++I)
		{
			const int Moving = Order[I];
			size_t J = I;
			while (J > 0 && Dist[static_cast<size_t>(Order[J - 1])] > Dist[static_cast<size_t>(Moving)])
			{
				Order[J] = Order[J - 1];
				--J;
			}
			Order[J] = Moving;
		}
		return Order;
	}

	/** When the lamp of rank k (0 = nearest) lights, ms after the cleanse. */
	inline double LampLitAtMs(const FCleanseSpec& Spec, int Rank)
	{
		return Spec.LampFirstMs + static_cast<double>(Rank < 0 ? 0 : Rank) * Spec.LampStepMs;
	}

	/** How many of Count lamps are lit ElapsedMs after the cleanse (rank k is lit from LampLitAtMs(k) on). */
	inline int LampsLitAt(const FCleanseSpec& Spec, int Count, double ElapsedMs)
	{
		if (Count <= 0 || ElapsedMs < Spec.LampFirstMs)
		{
			return 0;
		}
		if (!(Spec.LampStepMs > 0.0))
		{
			return Count;
		}
		// An estimate by division, then settled against LampLitAtMs itself: the two never disagree by rounding (a step
		// that is not a whole number of ms), so the last lamp is lit at the moment SequenceEndMs names.
		const double Estimate = std::floor((ElapsedMs - Spec.LampFirstMs) / Spec.LampStepMs) + 1.0;
		int Lit = Estimate >= static_cast<double>(Count) ? Count : (Estimate > 0.0 ? static_cast<int>(Estimate) : 0);
		while (Lit < Count && ElapsedMs >= LampLitAtMs(Spec, Lit))
		{
			++Lit;
		}
		while (Lit > 0 && ElapsedMs < LampLitAtMs(Spec, Lit - 1))
		{
			--Lit;
		}
		return Lit;
	}

	/** The grade's progress 0..1 (linear over GradeMs; the mood and the ambient blend by it). */
	inline double GradeAlpha(const FCleanseSpec& Spec, double ElapsedMs)
	{
		if (ElapsedMs <= 0.0)
		{
			return 0.0;
		}
		if (!(Spec.GradeMs > 0.0) || ElapsedMs >= Spec.GradeMs)
		{
			return 1.0;
		}
		return ElapsedMs / Spec.GradeMs;
	}

	/** When mark k (1-based, clockwise from the top) glows: the niche's time + k x MarkStepMs. */
	inline double MarkAtMs(const FCleanseSpec& Spec, int MarkNumber)
	{
		return Spec.NicheAtMs + static_cast<double>(MarkNumber < 0 ? 0 : MarkNumber) * Spec.MarkStepMs;
	}

	/** The mark number of a socket name (`mark_3` -> 3, at most MaxMarkNumber); 0 for any other name. */
	inline constexpr int MaxMarkNumber = 99;

	inline int MarkNumber(std::string_view SocketName)
	{
		if (SocketName.size() <= MarkSocketPrefix.size() || SocketName.substr(0, MarkSocketPrefix.size()) != MarkSocketPrefix)
		{
			return 0;
		}
		int Number = 0;
		for (const char C : SocketName.substr(MarkSocketPrefix.size()))
		{
			if (C < '0' || C > '9')
			{
				return 0;
			}
			Number = Number * 10 + (C - '0');
			if (Number > MaxMarkNumber)
			{
				return 0;
			}
		}
		return Number;
	}

	/**
	 * The marks of a niche in the order they glow: the indices of Numbers (MarkNumber of each socket) sorted by number,
	 * without the 0s (no mark) and without a number's second socket. A gap in the numbering (mark_1, mark_3) leaves no
	 * hole: two marks, 180 ms apart.
	 */
	inline std::vector<int> MarkOrder(const std::vector<int>& Numbers)
	{
		std::vector<int> Out;
		for (int Wanted = 1; Wanted <= MaxMarkNumber && Out.size() < Numbers.size(); ++Wanted)
		{
			for (size_t Index = 0; Index < Numbers.size(); ++Index)
			{
				if (Numbers[Index] == Wanted)
				{
					Out.push_back(static_cast<int>(Index));
					break;
				}
			}
		}
		return Out;
	}

	/**
	 * The sequence's last scheduled moment: the grade's end, the last lamp, the niche's glow, the last mark - whichever
	 * is latest. (A niche out of view is held past it: the sequence ends, the glow stays pending - AdvanceCleanse.)
	 */
	inline double SequenceEndMs(const FCleanseSpec& Spec, int LampCount, int MarkCount)
	{
		double End = Spec.GradeMs > Spec.NicheAtMs ? Spec.GradeMs : Spec.NicheAtMs;
		if (LampCount > 0)
		{
			const double LastLamp = LampLitAtMs(Spec, LampCount - 1);
			End = LastLamp > End ? LastLamp : End;
		}
		if (MarkCount > 0)
		{
			const double LastMark = MarkAtMs(Spec, MarkCount);
			End = LastMark > End ? LastMark : End;
		}
		return End;
	}

	/** The ambient of the grade at a progress (per channel, linear). */
	inline void AmbientAt(const FCleanseSpec& Spec, double Alpha, double Out[3])
	{
		const double T = Alpha <= 0.0 ? 0.0 : (Alpha >= 1.0 ? 1.0 : Alpha);
		for (size_t I = 0; I < 3; ++I)
		{
			Out[I] = Spec.Ambient[I] + (Spec.AmbientCleansed[I] - Spec.Ambient[I]) * T;
		}
	}

	/**
	 * The multiplier the graded materials apply at a ground distance from the hero (the reference of the HLSL in
	 * Scripts/abyss_content/hlsl.py `_ZONE_GRADE_CORE`): min(ambient + hero colour x gain x clamp(1 - d / r)^power, ceil), per
	 * channel. The lamps' pools come on top through the recipes' own lights (DECISIONS R18 slots), not through this.
	 */
	inline void GradeMultiplier(const FCleanseSpec& Spec, double Alpha, double HeroDistanceTiles, double Out[3])
	{
		AmbientAt(Spec, Alpha, Out);
		double Pool = 0.0;
		if (Spec.Hero.IsOn())
		{
			const double T = 1.0 - HeroDistanceTiles / Spec.Hero.RadiusTiles;
			Pool = T <= 0.0 ? 0.0 : std::pow(T >= 1.0 ? 1.0 : T, Spec.Hero.Power) * Spec.Hero.Gain;
		}
		for (size_t I = 0; I < 3; ++I)
		{
			const double Value = Out[I] + Spec.Hero.Color[I] * Pool;
			Out[I] = Value > Spec.Ceil ? Spec.Ceil : Value;
		}
	}

	// ---- the sequence's state --------------------------------------------------------------------------------------

	enum class ECleansePhase : uint8_t
	{
		Dark,      // not cleansed
		Playing,   // EvAreaCleansed arrived in this visit: the sequence runs
		Lit,       // the end state (the sequence finished, or the zone was entered cleansed)
	};

	struct FCleanseState
	{
		ECleansePhase Phase = ECleansePhase::Dark;
		double StartMs = 0.0;          // sim clock of the EvAreaCleansed
		std::vector<int> Order;        // LampOrder at the start
		int LampsLit = 0;              // lamps swapped so far (ranks 0 .. LampsLit - 1)
		int MarksPlayed = 0;
		/** The niche's glow and marks are owed (a cleanse seen in this visit; never on an entry already cleansed). */
		bool bNicheArmed = false;
		bool bNichePlayed = false;
		/** The glow was due with the niche out of view: it plays when the hero first sees it, the marks follow from then. */
		bool bNicheHeld = false;
		double NicheShownMs = 0.0;     // ms after the cleanse the glow played at (the marks' base)
		bool bFinished = false;        // the sequence ran to its end in this visit (the autotest's probe)
	};

	/** The niche's glow or a mark is still owed (the zone actor keeps ticking a Lit state for it). */
	inline bool NichePending(const FCleanseState& State, int MarkCount)
	{
		return State.bNicheArmed && (!State.bNichePlayed || State.MarksPlayed < MarkCount);
	}

	/** A zone entered: Lit when the sub-dungeon is already cleansed (everything at once, no sequence), else Dark. */
	inline FCleanseState EnterState(bool bCleansed, int LampCount)
	{
		FCleanseState Out;
		if (bCleansed)
		{
			Out.Phase = ECleansePhase::Lit;
			Out.LampsLit = LampCount;
		}
		return Out;
	}

	/** EvAreaCleansed: the sequence starts from `Pos` at NowMs. A state already Lit or Playing is kept. */
	inline void StartCleanse(FCleanseState& State, const std::vector<abyss::Vec2>& Lamps, abyss::Vec2 Pos, double NowMs)
	{
		if (State.Phase != ECleansePhase::Dark)
		{
			return;
		}
		State.Phase = ECleansePhase::Playing;
		State.StartMs = NowMs;
		State.Order = LampOrder(Lamps, Pos);
		State.LampsLit = 0;
		State.MarksPlayed = 0;
		State.bNicheArmed = true;
		State.bNichePlayed = false;
		State.bNicheHeld = false;
		State.NicheShownMs = 0.0;
	}

	/** The grade's progress of a state at NowMs: 0 dark, 1 lit, GradeAlpha while the sequence plays. */
	inline double StateAlpha(const FCleanseSpec& Spec, const FCleanseState& State, double NowMs)
	{
		switch (State.Phase)
		{
		case ECleansePhase::Lit: return 1.0;
		case ECleansePhase::Playing: return GradeAlpha(Spec, NowMs - State.StartMs);
		case ECleansePhase::Dark: break;
		}
		return 0.0;
	}

	struct FCleanseStep
	{
		/** Lamp indices (placement order) to swap to the lit mesh now, in lighting order. */
		std::vector<int> LightLamps;
		bool bPlayNiche = false;
		/** Mark numbers (1-based) to glow now. */
		std::vector<int> PlayMarks;
		bool bJustFinished = false;
	};

	/**
	 * Advances a sequence to NowMs: the lamps whose time has come (every one that is due, in order, so a long frame or
	 * a pause skips none), the niche's glow and its marks, and the end (Phase Lit, bFinished) once the last scheduled
	 * moment has passed - whatever is left unlit then is lit with it, so Lit always means every lamp.
	 *
	 * The niche's glow needs the niche in view (bNicheInView: "if the Flame Niche is on screen"). Due but out of view
	 * it is held - also past the sequence's end, in Phase Lit - and plays the first time the hero sees the niche; the
	 * marks then follow MarkStepMs apart from that moment. In view on time, the marks keep the contract's schedule
	 * (NicheAtMs + k x MarkStepMs), so one long frame plays the glow and every mark that is due. A state entered
	 * cleansed owes nothing. Nothing happens in Phase Dark.
	 */
	inline FCleanseStep AdvanceCleanse(const FCleanseSpec& Spec, FCleanseState& State, int MarkCount, double NowMs,
		bool bNicheInView = true)
	{
		FCleanseStep Out;
		if (State.Phase == ECleansePhase::Dark)
		{
			return Out;
		}
		const double Elapsed = NowMs - State.StartMs;
		const int LampCount = static_cast<int>(State.Order.size());
		if (State.Phase == ECleansePhase::Playing)
		{
			const bool bEnd = Elapsed >= SequenceEndMs(Spec, LampCount, MarkCount);
			const int Due = bEnd ? LampCount : LampsLitAt(Spec, LampCount, Elapsed);
			while (State.LampsLit < Due)
			{
				Out.LightLamps.push_back(State.Order[static_cast<size_t>(State.LampsLit)]);
				++State.LampsLit;
			}
			if (bEnd)
			{
				State.Phase = ECleansePhase::Lit;
				State.bFinished = true;
				Out.bJustFinished = true;
			}
		}
		if (State.bNicheArmed && !State.bNichePlayed && Elapsed >= Spec.NicheAtMs)
		{
			if (bNicheInView)
			{
				State.bNichePlayed = true;
				State.NicheShownMs = State.bNicheHeld ? Elapsed : Spec.NicheAtMs;
				Out.bPlayNiche = true;
			}
			else
			{
				State.bNicheHeld = true;
			}
		}
		if (State.bNicheArmed && State.bNichePlayed)
		{
			while (State.MarksPlayed < MarkCount
				&& Elapsed >= State.NicheShownMs + static_cast<double>(State.MarksPlayed + 1) * Spec.MarkStepMs)
			{
				++State.MarksPlayed;
				Out.PlayMarks.push_back(State.MarksPlayed);
			}
		}
		return Out;
	}

	// =================================================================================================================
	// Staggers the core leaves to the presentation
	// =================================================================================================================

	/** A dormant rank rises man by man: rank k starts its Rise k x this late (3D note 16: "a few frames per man"). */
	inline constexpr double RiseStaggerMs = 50.0;
	inline constexpr double RiseStaggerMaxMs = 300.0;
	/** A stagger never takes more than this share of the clip (the rest plays faster and ends on the core's time). */
	inline constexpr double StaggerMaxShare = 0.4;

	/** The delay of the Rise of rank k of its line (LineRanks; 0 = the line's first man). */
	inline double RiseDelayMs(int Rank)
	{
		const double Delay = static_cast<double>(Rank < 0 ? 0 : Rank) * RiseStaggerMs;
		return Delay > RiseStaggerMaxMs ? RiseStaggerMaxMs : Delay;
	}

	/**
	 * The ranks of the members of one dormant formation (one pack: the Helm Wall's six), by where they stand: along the
	 * formation's longer axis from its low end, the other axis and then the given order breaking ties. Out[i] = the rank
	 * of Positions[i]; the ranks are 0 .. n-1, each once, and rise monotonically along the line whatever the members'
	 * entity ids are. (The world builder ranks the dormant monsters it shows; the actor delays its Rise by RiseDelayMs.)
	 */
	inline std::vector<int> LineRanks(const std::vector<abyss::Vec2>& Positions)
	{
		const size_t Count = Positions.size();
		std::vector<int> Ranks(Count, 0);
		if (Count < 2)
		{
			return Ranks;
		}
		double MinX = Positions[0].x, MaxX = Positions[0].x, MinY = Positions[0].y, MaxY = Positions[0].y;
		for (const abyss::Vec2& P : Positions)
		{
			MinX = P.x < MinX ? P.x : MinX;
			MaxX = P.x > MaxX ? P.x : MaxX;
			MinY = P.y < MinY ? P.y : MinY;
			MaxY = P.y > MaxY ? P.y : MaxY;
		}
		const bool bAlongX = MaxX - MinX >= MaxY - MinY;
		auto Before = [&Positions, bAlongX](size_t A, size_t B)
		{
			const double MainA = bAlongX ? Positions[A].x : Positions[A].y;
			const double MainB = bAlongX ? Positions[B].x : Positions[B].y;
			if (MainA != MainB)
			{
				return MainA < MainB;
			}
			const double SideA = bAlongX ? Positions[A].y : Positions[A].x;
			const double SideB = bAlongX ? Positions[B].y : Positions[B].x;
			return SideA != SideB ? SideA < SideB : A < B;
		};
		for (size_t I = 0; I < Count; ++I)
		{
			int Rank = 0;
			for (size_t J = 0; J < Count; ++J)
			{
				Rank += J != I && Before(J, I) ? 1 : 0;
			}
			Ranks[I] = Rank;
		}
		return Ranks;
	}

	struct FStaggeredStart
	{
		double StartMs = 0.0;
		double DurationMs = 0.0;
	};

	/**
	 * An action of DurationMs that the core started at StartMs, shown DelayMs late: it starts at StartMs + delay and
	 * lasts DurationMs - delay, so it ends when the core's does (the core roots a rising monster for the clip: a Rise
	 * that ran over would slide). The delay is capped at StaggerMaxShare of the duration; an action without a duration
	 * is not delayed.
	 */
	inline FStaggeredStart StaggerStart(double StartMs, double DurationMs, double DelayMs)
	{
		FStaggeredStart Out{ StartMs, DurationMs };
		if (!(DurationMs > 0.0) || !(DelayMs > 0.0))
		{
			return Out;
		}
		const double Cap = DurationMs * StaggerMaxShare;
		const double Delay = DelayMs > Cap ? Cap : DelayMs;
		Out.StartMs = StartMs + Delay;
		Out.DurationMs = DurationMs - Delay;
		return Out;
	}

	// =================================================================================================================
	// Small state rules
	// =================================================================================================================

	/** A spent guardian (MonsterView::spent: this save has killed him before; his kill gives nothing) wears no elite
	 *  marker: the player is told before the fight (3D note 37). */
	inline bool ShowsEliteMarker(bool bEliteOrMiniBoss, bool bSpent)
	{
		return bEliteOrMiniBoss && !bSpent;
	}

	/** The zone the world map (and the save slot) shows the hero in: the parent zone while inside a sub-dungeon. */
	inline std::string_view WorldZoneId(bool bInside, std::string_view ParentZone, std::string_view MapId)
	{
		return bInside && !ParentZone.empty() ? ParentZone : MapId;
	}

	/** Whether an objective is done in the current map: its own `zone` when it names one, else the quest's. */
	inline bool ObjectiveInMap(std::string_view QuestZone, std::string_view ObjectiveZone, std::string_view MapId)
	{
		return (ObjectiveZone.empty() ? QuestZone : ObjectiveZone) == MapId;
	}

	/**
	 * Whether a quest counts as "of this place" in the tracker (rank 1, after the tracked quest and before the other
	 * zones'): its zone is the map, or - inside a sub-dungeon - the parent zone, or one of its objectives names the map.
	 */
	inline bool QuestInMap(std::string_view QuestZone, std::string_view MapId, bool bInside, std::string_view ParentZone,
		bool bAnyObjectiveInMap)
	{
		return QuestZone == MapId || (bInside && !ParentZone.empty() && QuestZone == ParentZone) || bAnyObjectiveInMap;
	}

	/**
	 * The entrance prompt: the verb (`ui.abyss.enter`) and the place's name (`data.subDungeon.<target>.name`), "Enter The
	 * Pit of the Faithful"; a verb that ends in a non-ASCII character (zh-CN) joins without a space. No name: the verb.
	 */
	inline std::string EnterPrompt(std::string_view Verb, std::string_view Name)
	{
		std::string Out(Verb);
		if (Name.empty())
		{
			return Out;
		}
		if (!Out.empty() && (static_cast<unsigned char>(Out.back()) & 0x80u) == 0u)
		{
			Out += ' ';
		}
		Out += Name;
		return Out;
	}

	/** The zone whose audio overrides (music table) apply: the map's own when it has a table, else its parent's. */
	inline std::string_view AudioZoneId(bool bInside, std::string_view ParentZone, std::string_view MapId, bool bMapHasOwnTable)
	{
		return bInside && !bMapHasOwnTable && !ParentZone.empty() ? ParentZone : MapId;
	}

	// ---- the autotest's probe (report.json `subDungeon`) -----------------------------------------------------------

	struct FProbe
	{
		int Visits = 0;               // zone builds inside a sub-dungeon
		bool bThemeResolved = false;  // a themed wall block and the themed floor style resolved through the chain
		int WallBlocks = 0;           // blocks placed in the last visit
		int LampsExpected = 0;        // set pieces of the contract's unlit lamp id the last visit's map placed
		int Lamps = 0;                // of those, with their lit twin placed (the swap can show)
		int PeakLampsLit = 0;
		int DormantSeen = 0;          // the most distinct dormant monsters one visit showed in their still pose
		int DormantWakes = 0;         // the most of them one visit saw waking
		int Cleanses = 0;             // EvAreaCleansed received
		int SequencesStarted = 0;     // of those, with a built sub-dungeon to play the sequence in
		int SequencesFinished = 0;
		int SequencesCut = 0;         // a sequence the zone was rebuilt or left under (the next build enters lit)
		bool bGradeExpected = false;  // the theme chain has a dark contract (vfx_recipes.json dark.<theme>.lightRadius)
		bool bGraded = false;         // the grade was written to the materials' collection (the content has the parameters)
	};

	/** "" while the presentation kept up with the core; else what the core did that nothing showed. */
	inline std::string ProbeVerdict(const FProbe& P)
	{
		std::string Out;
		auto Add = [&Out](const std::string& Line)
		{
			Out += Out.empty() ? Line : "; " + Line;
		};
		if (P.Visits == 0)
		{
			return Out;
		}
		if (!P.bThemeResolved)
		{
			Add("a sub-dungeon was built but its themed wall / floor did not resolve (theme parent chain, the crypt kit?)");
		}
		if (P.WallBlocks <= 0)
		{
			Add("a sub-dungeon was built without one wall block");
		}
		if (P.bGradeExpected && !P.bGraded)
		{
			Add("the light-radius grade was not written (MPC_AF_Lighting without the Grade* parameters: content older than Wave 3?)");
		}
		if (P.Lamps < P.LampsExpected)
		{
			Add(std::to_string(P.Lamps) + " of " + std::to_string(P.LampsExpected) + " lamps have their lit twin (the lit lamp's art?)");
		}
		// A finished sequence, or an entry after a cut one, shows every lamp lit.
		const int LampsWanted = P.LampsExpected > P.Lamps ? P.LampsExpected : P.Lamps;
		if (LampsWanted > 0 && P.SequencesFinished > 0 && P.PeakLampsLit < LampsWanted)
		{
			Add(std::to_string(P.PeakLampsLit) + " of " + std::to_string(LampsWanted) + " lamps lit after the cleanse");
		}
		if (P.SequencesStarted > P.SequencesFinished + P.SequencesCut)
		{
			Add("the cleanse sequence did not finish");
		}
		if (P.DormantSeen > 0 && P.DormantWakes == 0)
		{
			Add(std::to_string(P.DormantSeen) + " dormant monsters shown but none seen waking");
		}
		return Out;
	}
}

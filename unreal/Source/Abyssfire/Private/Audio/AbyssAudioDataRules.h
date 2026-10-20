// AbyssAudioData: what the dark-remake audio tables share (U7, implementation-plan.md 4): reading the manifest's JSON
// (unreal/Audio/Export/audio_manifest.json) into std types, the region geometry rule and the `while` conditions.
//
// Pure helper (no UE types, std + AbyssCore public headers only): UAbyssAudioSystem uses it, PresentationTests tests it.
//
// Region rule (manifest musicDirector.regionRule / ambience.regionRule): the hero's tile (col, row) is inside a circle
// when (col - c)^2 + (row - r)^2 <= radius^2 (Euclidean tile distance, ExplorationGrid::Reveal's metric) and stays
// inside until that distance exceeds exitRadius (hysteresis). A `while {monsterAlive: id}` must hold too: a live
// instance of that monster id is in the zone.
#pragma once

#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "abyss/base/Json.h"

namespace AbyssAudioData
{
	inline std::string Str(const abyss::JsonValue& Value)
	{
		return std::string(Value.AsString());
	}

	inline double Num(const abyss::JsonValue& Value, double Default)
	{
		return Value.IsNumber() ? Value.AsDouble(Default) : Default;
	}

	inline std::vector<std::string> Strings(const abyss::JsonValue& Array)
	{
		std::vector<std::string> Out;
		for (const abyss::JsonValue& Item : Array.Items())
		{
			if (Item.IsString() && !Item.AsString().empty())
			{
				Out.emplace_back(Item.AsString());
			}
		}
		return Out;
	}

	inline bool Contains(const std::vector<std::string>& List, std::string_view Value)
	{
		for (const std::string& Item : List)
		{
			if (Item == Value)
			{
				return true;
			}
		}
		return false;
	}

	/** A tile circle with an exit radius (hysteresis). exitRadius < radius reads as radius. */
	struct TileCircle
	{
		double col = 0.0;
		double row = 0.0;
		double radius = 0.0;
		double exitRadius = 0.0;
	};

	/** {col, row, radius, exitRadius?} of a manifest row (exitRadius defaults to radius). */
	inline TileCircle ReadCircle(const abyss::JsonValue& Row)
	{
		TileCircle Out;
		Out.col = Num(Row.Get("col"), 0.0);
		Out.row = Num(Row.Get("row"), 0.0);
		Out.radius = Num(Row.Get("radius"), 0.0);
		Out.exitRadius = Num(Row.Get("exitRadius"), Out.radius);
		if (Out.exitRadius < Out.radius)
		{
			Out.exitRadius = Out.radius;
		}
		return Out;
	}

	/** The hero's tile from its (interpolated) position: the tile whose centre is nearest (floor(x + 0.5)). */
	struct TilePoint
	{
		int col = 0;
		int row = 0;
	};

	inline TilePoint TileOf(double X, double Y)
	{
		return TilePoint{ static_cast<int>(std::floor(X + 0.5)), static_cast<int>(std::floor(Y + 0.5)) };
	}

	inline double DistSq(const TileCircle& Circle, double Col, double Row)
	{
		const double Dc = Col - Circle.col;
		const double Dr = Row - Circle.row;
		return Dc * Dc + Dr * Dr;
	}

	/** Region membership with hysteresis: enter within radius, stay within exitRadius. */
	inline bool Inside(const TileCircle& Circle, const TilePoint& Tile, bool bWasInside)
	{
		const double R = bWasInside ? Circle.exitRadius : Circle.radius;
		return DistSq(Circle, static_cast<double>(Tile.col), static_cast<double>(Tile.row)) <= R * R;
	}

	/** A row's `while` condition; an empty monsterAlive always holds. */
	struct WhileCondition
	{
		std::string monsterAlive;
	};

	inline WhileCondition ReadWhile(const abyss::JsonValue& Row)
	{
		WhileCondition Out;
		Out.monsterAlive = Str(Row.Get("while").Get("monsterAlive"));
		return Out;
	}

	/** Whether the condition holds; IsAlive(std::string_view monsterId) -> bool (a live instance is in the zone). */
	template <class IsAliveFn>
	bool Holds(const WhileCondition& Condition, IsAliveFn&& IsAlive)
	{
		return Condition.monsterAlive.empty() || IsAlive(std::string_view(Condition.monsterAlive));
	}

	/** Hysteresis state of a list of circles (one flag per circle, in table order). */
	class CircleTracker
	{
	public:
		void Reset() { InsideFlags.clear(); }

		/** Re-evaluates every circle for the hero's tile (a table that changed size starts outside). */
		void Update(const std::vector<TileCircle>& Circles, const TilePoint& Tile)
		{
			InsideFlags.resize(Circles.size(), 0);
			for (std::size_t I = 0; I < Circles.size(); ++I)
			{
				InsideFlags[I] = Inside(Circles[I], Tile, InsideFlags[I] != 0) ? 1 : 0;
			}
		}

		bool IsInside(std::size_t Index) const { return Index < InsideFlags.size() && InsideFlags[Index] != 0; }

	private:
		std::vector<unsigned char> InsideFlags;
	};

	/** Linear move of a 0..1 weight toward on (1) or off (0): a full fade takes fadeInSec / fadeOutSec (0 = at once). */
	inline double Ramp(double Weight, bool bOn, double FadeInSec, double FadeOutSec, double DeltaSec)
	{
		if (bOn)
		{
			const double Next = FadeInSec > 0.0 ? Weight + DeltaSec / FadeInSec : 1.0;
			return Next > 1.0 ? 1.0 : Next;
		}
		const double Next = FadeOutSec > 0.0 ? Weight - DeltaSec / FadeOutSec : 0.0;
		return Next < 0.0 ? 0.0 : Next;
	}
}

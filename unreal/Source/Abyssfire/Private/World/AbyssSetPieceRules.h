// The UE set-piece layer's presentation rules (dark remake, implementation-plan.md 4 U4; data: Data/set_pieces.json via
// abyss/data/SetPieceData.h, placements: abyss/world/SetPieces.h). What AAbyssZoneActor::BuildSetPieces
// (Private/World/AbyssSetPieces.cpp) and UAbyssWorldBuilder decide on top of the core's placements:
//
// * Art: a piece's id chain (gameId, then its fallbacks; the zone's `@theme` variants are tried first by
//   FAbyssArtManifest::ResolveFirst) and the variant hash (a fixed `variant` index, else the placement's tile hash).
// * Spots (Collect): every socket-bound effect (a looping VFX recipe), audio emitter spot (handed to
//   UAbyssAudioSystem::SetZoneEmitterSpots: Ashwheat's knocking doors) and cue spot (a recipe played when an audio cue
//   trigger plays: the bells), turned by the placement's yaw; the echo anchors (a lore echo whose MI_AF_Echo figures A10
//   posed beside this mesh at yaw 0 takes the piece's position, and its yaw plus the piece's echoTurnDeg); the pieces
//   that yield to a defend target; the tiles the pieces cover (non-blocking zone decoration there is not drawn: no
//   grass through a cottage floor).
// * Queries: the cue spots of a played cue, the echo anchor of a lore id, the yielding piece under a defend target.
// * Event labels (the world layer): `<key>.<zone>` before `<key>`, the rule of the core's RevZoneKey (RandomEvents.cpp),
//   for the treasure cache, the wandering merchant and the puzzle; in a themed zone only, the rescue's name is
//   `sys.event.rescue.<zone>`, else zone.event.rescue.fallbackName (unthemed zones keep the unlabelled rescue prop).
//
// UE-free and header-only (std + AbyssCore public headers), tested in unreal/PresentationTests/tests/setpieces_test.cpp.
// The glue converts FName / FVector at the boundary and keeps no rule of its own.
#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "abyss/base/Math.h"
#include "abyss/base/Types.h"
#include "abyss/data/SetPieceData.h"
#include "abyss/world/SetPieces.h"

namespace AbyssSetPieceRules
{
	// =================================================================================================================
	// Art
	// =================================================================================================================

	/** Salt of the placement's tile hash (AbyssWorldUtil::TileHash(col, row, salt)) when no variant is fixed. */
	inline constexpr int32_t VariantHashSalt = 37;

	/** The art-manifest ids a piece tries, in order: its gameId, then its fallbacks. */
	inline std::vector<std::string> IdChain(const abyss::SetPieceDef& Def)
	{
		std::vector<std::string> Out;
		Out.reserve(1 + Def.fallbackIds.size());
		Out.push_back(Def.gameId);
		for (const std::string& Id : Def.fallbackIds)
		{
			if (!Id.empty() && std::find(Out.begin(), Out.end(), Id) == Out.end())
			{
				Out.push_back(Id);
			}
		}
		return Out;
	}

	/** The hash handed to the manifest's variant pick (Hash % variants): the fixed variant index, else the tile hash. */
	inline uint32_t VariantHash(int32_t FixedVariant, uint32_t TileHash)
	{
		return FixedVariant >= 0 ? static_cast<uint32_t>(FixedVariant) : TileHash;
	}

	inline constexpr std::string_view InstanceKeyPrefix = "setpiece:";

	/** The instance key a placement's main mesh is registered under (the zone actor's keyed instances): "setpiece:<id>#<k>". */
	inline std::string InstanceKey(const abyss::SetPiecePlacement& P)
	{
		std::string Out(InstanceKeyPrefix);
		Out += P.def != nullptr ? P.def->id : std::string("?");
		Out += '#';
		Out += std::to_string(P.member);
		return Out;
	}

	// =================================================================================================================
	// Spots
	// =================================================================================================================

	/** A static-mesh socket of the resolved asset (manifest `sockets[].locCm`: UE cm, the mesh frame). */
	struct Socket
	{
		std::string Name;
		double XCm = 0.0;
		double YCm = 0.0;
	};

	/** The art the presentation resolved for one placement (its main mesh and its partners). */
	struct PlacedArt
	{
		bool bResolved = false;
		std::vector<Socket> Sockets;                 // the main mesh's
		std::vector<std::array<double, 4>> Bounds;   // {minX, minY, maxX, maxY} cm of every mesh placed (main + partners)
	};

	struct EffectSpot
	{
		std::string Recipe;
		abyss::Vec2 Tile;
	};

	struct EmitterSpots
	{
		std::string Emitter;
		std::vector<abyss::Vec2> Tiles;   // placement order: spot i plays the emitter's asset i % n
	};

	struct CueSpot
	{
		std::vector<std::string> Cues;
		std::string Recipe;
		abyss::Vec2 Tile;
	};

	struct EchoAnchor
	{
		std::string LoreId;
		abyss::Vec2 Tile;
		double YawDeg = 0.0;   // the piece's actor yaw + its echoTurnDeg: the echo's figures turn with it
	};

	struct Yielder
	{
		std::string Key;       // InstanceKey of the piece's main mesh
		abyss::Vec2 Tile;
		double YawDeg = 0.0;
	};

	struct ZoneSpots
	{
		std::vector<EffectSpot> Effects;
		std::vector<EmitterSpots> Emitters;     // first-seen emitter order
		std::vector<CueSpot> Cues;
		std::vector<EchoAnchor> Echoes;
		std::vector<Yielder> Yielders;
		std::vector<abyss::TilePos> Covered;    // sorted (row, col), unique
		std::vector<std::string> MissingSockets;  // "<piece>.<socket>" the resolved asset does not have
		std::vector<std::string> Unresolved;      // pieces without art (first-seen order, unique)
	};

	inline const Socket* FindSocket(const PlacedArt& Art, std::string_view Name)
	{
		for (const Socket& S : Art.Sockets)
		{
			if (S.Name == Name)
			{
				return &S;
			}
		}
		return nullptr;
	}

	inline bool TileLess(const abyss::TilePos& A, const abyss::TilePos& B)
	{
		return A.row != B.row ? A.row < B.row : A.col < B.col;
	}

	/**
	 * The zone's spots from its placements and the art resolved for each (Arts[i] belongs to Placements[i]; missing
	 * entries count as unresolved). A placement without art gets no socket spot and covers no tile (nothing is drawn),
	 * but still anchors its echoes (the figures stand where the mesh would).
	 */
	inline ZoneSpots Collect(const std::vector<abyss::SetPiecePlacement>& Placements, const std::vector<PlacedArt>& Arts)
	{
		ZoneSpots Out;
		const PlacedArt NoArt;
		for (std::size_t I = 0; I < Placements.size(); ++I)
		{
			const abyss::SetPiecePlacement& P = Placements[I];
			if (P.def == nullptr)
			{
				continue;
			}
			const abyss::SetPieceDef& Def = *P.def;
			const PlacedArt& Art = I < Arts.size() ? Arts[I] : NoArt;
			for (const std::string& Lore : Def.echoes)
			{
				Out.Echoes.push_back(EchoAnchor{ Lore, P.pos, P.yawDeg + Def.echoTurnDeg });
			}
			if (!Art.bResolved)
			{
				if (std::find(Out.Unresolved.begin(), Out.Unresolved.end(), Def.id) == Out.Unresolved.end())
				{
					Out.Unresolved.push_back(Def.id);
				}
				continue;
			}
			const auto SocketTile = [&](const std::string& Name, abyss::Vec2& Tile)
			{
				const Socket* S = FindSocket(Art, Name);
				if (S == nullptr)
				{
					const std::string Missing = Def.id + "." + Name;
					if (std::find(Out.MissingSockets.begin(), Out.MissingSockets.end(), Missing) == Out.MissingSockets.end())
					{
						Out.MissingSockets.push_back(Missing);
					}
					return false;
				}
				Tile = abyss::SetPieceLocalToTile(P, S->XCm, S->YCm);
				return true;
			};
			for (const abyss::SetPieceEffectDef& E : Def.effects)
			{
				abyss::Vec2 Tile;
				if (SocketTile(E.socket, Tile))
				{
					Out.Effects.push_back(EffectSpot{ E.recipe, Tile });
				}
			}
			for (const abyss::SetPieceEmitterDef& E : Def.emitters)
			{
				abyss::Vec2 Tile;
				if (!SocketTile(E.socket, Tile))
				{
					continue;
				}
				auto Found = std::find_if(Out.Emitters.begin(), Out.Emitters.end(),
					[&E](const EmitterSpots& S) { return S.Emitter == E.emitter; });
				if (Found == Out.Emitters.end())
				{
					Out.Emitters.push_back(EmitterSpots{ E.emitter, {} });
					Found = Out.Emitters.end() - 1;
				}
				Found->Tiles.push_back(Tile);
			}
			for (const abyss::SetPieceCueDef& C : Def.cues)
			{
				abyss::Vec2 Tile;
				if (SocketTile(C.socket, Tile))
				{
					Out.Cues.push_back(CueSpot{ C.cues, C.recipe, Tile });
				}
			}
			if (Def.yieldsToDefendTarget)
			{
				Out.Yielders.push_back(Yielder{ InstanceKey(P), P.pos, P.yawDeg });
			}
			Out.Covered.push_back(abyss::RoundToTile(P.pos));
			for (const std::array<double, 4>& B : Art.Bounds)
			{
				abyss::SetPieceCoveredTiles(P, B[0], B[1], B[2], B[3], 0.0, Out.Covered);
			}
		}
		std::sort(Out.Covered.begin(), Out.Covered.end(), TileLess);
		Out.Covered.erase(std::unique(Out.Covered.begin(), Out.Covered.end()), Out.Covered.end());
		return Out;
	}

	/** The cue spots that play when `CueId` plays (bell_toll, bell_toll_far, bell_self_ring, ...). */
	inline std::vector<const CueSpot*> CueSpotsFor(const ZoneSpots& Spots, std::string_view CueId)
	{
		std::vector<const CueSpot*> Out;
		for (const CueSpot& S : Spots.Cues)
		{
			if (std::find(S.Cues.begin(), S.Cues.end(), CueId) != S.Cues.end())
			{
				Out.push_back(&S);
			}
		}
		return Out;
	}

	/** The anchor of a lore echo (the nearest to its lore tile when several pieces name it), else nullptr. */
	inline const EchoAnchor* EchoAnchorFor(const ZoneSpots& Spots, std::string_view LoreId, abyss::Vec2 LoreTile)
	{
		const EchoAnchor* Best = nullptr;
		double BestDistSq = 0.0;
		for (const EchoAnchor& A : Spots.Echoes)
		{
			if (A.LoreId != LoreId)
			{
				continue;
			}
			const double D = abyss::DistSq(A.Tile, LoreTile);
			if (Best == nullptr || D < BestDistSq)
			{
				Best = &A;
				BestDistSq = D;
			}
		}
		return Best;
	}

	/** One echo figure in the anchor's frame: its offset turned by the anchor's yaw (tiles) and its facing yaw. */
	struct FigurePose
	{
		abyss::Vec2 Offset;      // tiles from the anchor tile
		double FacingYawDeg = 0.0;
	};

	/**
	 * A figure authored at (OffsetCol, OffsetRow) tiles with FacingYawDeg beside a mesh at yaw 0 (AbyssThemeRules::
	 * FEchoFigure, UE axes), posed beside the same mesh turned by YawDeg.
	 */
	inline FigurePose EchoFigurePose(double YawDeg, double OffsetCol, double OffsetRow, double FacingYawDeg)
	{
		abyss::SetPieceDef Unit;   // scale 1
		abyss::SetPiecePlacement P;
		P.def = &Unit;
		P.yawDeg = YawDeg;
		FigurePose Out;
		Out.Offset = abyss::SetPieceLocalToTile(P, OffsetCol * 100.0, OffsetRow * 100.0);
		Out.FacingYawDeg = FacingYawDeg + YawDeg;
		return Out;
	}

	/** Within this many tiles of a defend target a yielding piece is hidden (the target shows the same gate). */
	inline constexpr double YieldRadiusTiles = 1.5;

	/** The yielding piece a defend target at Tile stands on (the nearest within the radius), else nullptr. */
	inline const Yielder* YielderNear(const ZoneSpots& Spots, abyss::Vec2 Tile, double RadiusTiles = YieldRadiusTiles)
	{
		const Yielder* Best = nullptr;
		double BestDistSq = RadiusTiles * RadiusTiles;
		for (const Yielder& Y : Spots.Yielders)
		{
			const double D = abyss::DistSq(Y.Tile, Tile);
			if (D <= BestDistSq)
			{
				Best = &Y;
				BestDistSq = D;
			}
		}
		return Best;
	}

	/**
	 * Whether a generated zone decoration is left out because a set piece stands on its tile: only non-blocking dressing
	 * (grass, flowers, bushes: no walkability depends on it). A blocking one stays (the core blocks its tile; CoreTests
	 * setpieces_test keeps every piece off them).
	 */
	inline bool HidesDecoration(const ZoneSpots& Spots, abyss::TilePos Tile, bool bBlocking)
	{
		return !bBlocking && std::binary_search(Spots.Covered.begin(), Spots.Covered.end(), Tile, TileLess);
	}

	// =================================================================================================================
	// Event labels (the world layer)
	// =================================================================================================================

	/** `<Base>.<ZoneId>` when the strings have it, else Base (RandomEvents.cpp RevZoneKey's rule). */
	template <class HasKeyFn>
	std::string ZoneVariantKey(std::string_view Base, std::string_view ZoneId, HasKeyFn&& Has)
	{
		if (!ZoneId.empty())
		{
			std::string Own(Base);
			Own += '.';
			Own += ZoneId;
			if (Has(std::string_view(Own)))
			{
				return Own;
			}
		}
		return std::string(Base);
	}

	/**
	 * The label key of a random-event prop (its EvEntitySpawned def id) in a zone, or empty for a prop that draws no
	 * label: the treasure cache, the wandering merchant and the puzzle read `zone.event.<kind>.label[.<zone>]` in every
	 * zone (as before U4, a zone without its own string keeps the generic one); the rescue's captive is named only in a
	 * zone with a presentation theme (bThemedZone; chapters 2-5 keep today's unlabelled rescue prop) by
	 * `sys.event.rescue.<zone>`, else zone.event.rescue.fallbackName.
	 */
	template <class HasKeyFn>
	std::string EventLabelKey(std::string_view PropDefId, std::string_view ZoneId, bool bThemedZone, HasKeyFn&& Has)
	{
		if (PropDefId == "treasure_cache")
		{
			return ZoneVariantKey("zone.event.treasureChest.label", ZoneId, Has);
		}
		if (PropDefId == "wandering_merchant")
		{
			return ZoneVariantKey("zone.event.merchant.label", ZoneId, Has);
		}
		if (PropDefId == "environmental_puzzle")
		{
			return ZoneVariantKey("zone.event.puzzle.label", ZoneId, Has);
		}
		if (PropDefId == "rescue")
		{
			if (!bThemedZone)
			{
				return {};
			}
			std::string Own("sys.event.rescue.");
			Own += ZoneId;
			return !ZoneId.empty() && Has(std::string_view(Own)) ? Own : std::string("zone.event.rescue.fallbackName");
		}
		return {};
	}
}

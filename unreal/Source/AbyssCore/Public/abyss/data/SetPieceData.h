// The UE set-piece layer's table (dark remake, implementation-plan.md 4 U4): set_pieces.json, exported from
// unreal/GameData/setPieces.ts. Authored prop groups the presentation places at tiles when it builds a zone (the bell
// frame, Ashwheat's cottages, the Sheepfold's wagon ring, the Breach Rim's dais and banner ring, the sealed Pit mouth,
// ...). Presentation only: never a map-generator input (the golden maps stay put) and never walkability (the sim does not
// read it). The core loads it (optional file) so the data is checked with the maps, quests and lore it names
// (DataStore::Finalize) and so the placement maths (abyss/world/SetPieces.h) is one deterministic function the UE build
// and the tests share.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/Platform.h"
#include "abyss/base/Types.h"

namespace abyss {

// Where a piece stands (before its `offset`).
enum class SetPieceAnchorKind : uint8_t {
  Tile,          // col, row (sub-tile allowed)
  Camp,          // camp number (1-based, MapDef::camps order): its centre, or the centre of its first `prop` (CampProps)
  Entrance,      // a MapDef::subDungeonEntrances id
  Region,        // a MapDef::regions id (its centre)
  DefendTarget,  // the defend target of a quest of the zone
};
ABYSS_ENUM_STRINGS(SetPieceAnchorKind, "tile", "camp", "entrance", "region", "defendTarget")

// Which way a piece turns. Yaw is the actor yaw itself (UE degrees, the mesh's own frame); every other kind turns the
// mesh's front (Blender -Y, the art kit's convention) toward something, and the actor yaw adds the art manifest's mesh
// yaw offset (units.characterForward, -90). turnDeg is added in every case.
enum class SetPieceFacingKind : uint8_t {
  Yaw,         // deg = the actor yaw
  Front,       // deg = the front's direction in tile space (0 = +col, 90 = +row)
  Camera,      // the front toward the W1 camera (WorldConstants::cameraYawDeg + 180), like a camp prop without a front
  Tile,        // the front toward `toward`
  Anchor,      // the front toward the anchor point (before the offset): a camp's hearth, a region's centre
  RingCentre,  // ring members: the front toward the ring's centre
};
ABYSS_ENUM_STRINGS(SetPieceFacingKind, "yaw", "front", "camera", "tile", "anchor", "ringCentre")

// Members k = 0..count-1 at startDeg + k * stepDeg (tile space) on a circle of `radius` tiles round the anchor + offset.
struct SetPieceRingDef {
  int32_t count = 0;
  double radius = 0;
  double startDeg = 0;
  double stepDeg = 0;
  std::vector<int32_t> skip;      // member indices left out (gaps)
  std::vector<int32_t> variants;  // member k shows variants[k % n]; empty = the piece's `variant`
};

// A VFX recipe looping at an asset socket (manifest sockets, UE cm in the mesh frame).
struct SetPieceEffectDef {
  std::string socket;
  std::string recipe;
};

// An audio emitter (Audio/Export/audio_manifest.json ambience.emitters) placed at an asset socket.
struct SetPieceEmitterDef {
  std::string socket;
  std::string emitter;
};

// A recipe played at an asset socket whenever one of `cues` plays (the audio cue triggers: the bells).
struct SetPieceCueDef {
  std::vector<std::string> cues;
  std::string socket;
  std::string recipe;
};

struct SetPieceDef {
  std::string id;    // unique in its zone
  std::string area;  // chapter1.md 1 area it dresses (knellwatch, ashwheat, breach_rim, ...)
  std::string gameId;
  std::vector<std::string> fallbackIds;
  int32_t variant = -1;  // asset index among the id's variants (asset-name order); -1 = the presentation's tile hash
  SetPieceAnchorKind anchor = SetPieceAnchorKind::Tile;
  Vec2 tile;             // Tile
  int32_t camp = 0;      // Camp (1-based)
  std::string ref;       // Camp: prop type ("" = the camp centre); Entrance / Region: the id; DefendTarget: the quest id
  Vec2 offset;           // tiles added to the anchor point
  bool hasRing = false;
  SetPieceRingDef ring;
  SetPieceFacingKind facing = SetPieceFacingKind::Camera;
  double facingDeg = 0;  // Yaw / Front
  Vec2 toward;           // Tile
  double turnDeg = 0;
  double scale = 1;
  std::vector<std::string> partners;  // game ids placed at the same spot and yaw (ground layers)
  std::vector<SetPieceEffectDef> effects;
  std::vector<SetPieceEmitterDef> emitters;
  std::vector<SetPieceCueDef> cues;
  std::vector<std::string> echoes;  // lore echo ids whose MI_AF_Echo figures pose in this piece's frame
  double echoTurnDeg = 0;           // the echo figures' group turns about the piece's origin by this on top of its yaw
  bool occluder = false;            // tall dressing: occlusion fade (R1)
  std::string walkOver;             // non-empty: why the hero may cross its footprint (exempt from the route check)
  bool yieldsToDefendTarget = false;  // hidden while a defend target (the same art) stands on it
  std::string note;
};

struct SetPieceZoneDef {
  std::string zone;
  std::vector<SetPieceDef> pieces;
};

struct ABYSS_API SetPieceTables {
  bool loaded = false;  // set_pieces.json was present
  std::vector<SetPieceZoneDef> zones;

  const SetPieceZoneDef* ForZone(std::string_view zone) const;
};

}  // namespace abyss

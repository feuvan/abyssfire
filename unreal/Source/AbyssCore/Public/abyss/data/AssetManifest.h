// Art asset manifest (ARCHITECTURE 5): Art/Export/manifest.json, copied by the exporter to Data/assets.json as
// {"manifest": <art manifest>}. Optional at load (the art pipeline runs in parallel); when present, animation
// contact/release ms come from here (the web derived them from sheet frames, AnimConfig.attackContact), with
// anim_timing.json's contact table as the fallback. Footprint/blocking of props feed W5 tall-decor blocking.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "abyss/base/Platform.h"

namespace abyss {

struct AnimNotifyDef {
  std::string name;  // Contact, Release, FootL, ...
  double ms = 0;
};

struct AnimClipDef {
  std::string name;   // Idle, Run, Attack01, Cast01, Hurt, Death, Dodge, signature names
  std::string asset;
  double lengthMs = 0;
  int32_t frames = 0;
  double fps = 0;
  bool loop = false;
  bool hasContactMs = false;
  double contactMs = 0;
  bool hasReleaseMs = false;
  double releaseMs = 0;
  bool additive = false;
  double refSpeedCmS = 0;
  std::vector<AnimNotifyDef> notifies;
};

// Largest prop footprint side in tiles (W5 bakes footprints into the walkability grid; DataStore::Finalize checks 1..8).
inline constexpr int32_t kMaxFootprintTiles = 8;

struct ABYSS_API AssetEntryDef {
  std::string name;      // UE asset name (manifest key)
  std::string kind;      // SkeletalMesh | StaticMesh | ...
  std::string category;  // Characters | Monsters | NPCs | Weapons | Props | ...
  std::vector<std::string> gameIds;
  std::string skeleton;
  double scale = 1;
  double heightCm = 0;
  std::vector<AnimClipDef> anims;
  bool hasFootprint = false;
  int32_t footprintW = 1, footprintH = 1;
  bool hasBlocking = false;
  bool blocking = false;

  const AnimClipDef* FindClip(std::string_view clipName) const;
};

struct ABYSS_API AssetManifest {
  bool loaded = false;
  int32_t schemaVersion = 0;
  std::vector<AssetEntryDef> assets;
  std::vector<std::pair<std::string, std::string>> gameIdToAsset;  // sorted by game id

  const AssetEntryDef* FindAsset(std::string_view assetName) const;
  const AssetEntryDef* FindByGameId(std::string_view gameId) const;
};

}  // namespace abyss

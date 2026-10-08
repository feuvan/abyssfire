// Presentation tables the core loads and validates for the UE module: UI theme, render-quality profiles, and the
// later-milestone Abyss Labyrinth run tables. Sources: ui_theme.json, render_quality.json, abyss_run.json.
// The gameplay core does not read these (except the labyrinth milestone, later).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/Platform.h"
#include "abyss/base/Stats.h"

namespace abyss {

struct UiThemeDef {
  int32_t designWidth = 1280, designHeight = 720;
  std::string background;
  LocaleId defaultLocale = LocaleId::ZhCN;
  std::vector<LocaleId> locales;
  std::string fontBody, fontTitle;
  std::vector<std::pair<std::string, std::string>> colors;   // name -> CSS colour (string entries)
  std::vector<std::pair<std::string, uint32_t>> colorNums;   // name -> 0xRRGGBB (numeric entries)
  std::array<std::string, EnumCount<ItemQuality>()> qualityHex{};
  std::vector<std::pair<std::string, double>> touchCssPx;
  double touchScaleMin = 1, touchScaleMax = 2;
  int32_t touchPointers = 4;
  double minTouchTargetPt = 44;
};

enum class RenderQuality : uint8_t { Low, Balanced, High };
ABYSS_ENUM_STRINGS(RenderQuality, "low", "balanced", "high")

struct RenderQualityProfileDef {
  RenderQuality quality = RenderQuality::Balanced;
  int32_t maxDynamicLights = 16;
  double lightingUpdateIntervalMs = 50;
  double particleFrequencyMultiplier = 1;
  bool bloom = true;
  bool colorGrading = true;
  double resolutionScale = 1;
};

struct RenderQualityTables {
  std::array<RenderQualityProfileDef, EnumCount<RenderQuality>()> profiles{};
  double lowPixelBudget = 5000000, highMaxPixelBudget = 3700000, highMinDpr = 2;
  int32_t constrainedCores = 4;
  double constrainedMemoryGb = 4;
};

// abyss_run.json (labyrinth milestone; loaded and validated now so the tables stay consistent).
enum class BoonRarity : uint8_t { Common, Rare, Epic };
ABYSS_ENUM_STRINGS(BoonRarity, "common", "rare", "epic")

struct BoonDef {
  std::string id;
  BoonRarity rarity = BoonRarity::Common;
  StatBag stats;
  int32_t maxStacks = 0;  // 0 = unlimited
  std::string glyph;
};

struct CurseDef {
  std::string id;
  double speedMul = 1, damageMul = 1, hpMul = 1;
  int32_t extraGroups = 0;
  double eliteChance = 0, visionMul = 1, regenPerSec = 0, deathBurst = 0;
  double lootBonus = 0, magicFind = 0;
};

struct FloorThemeDef {
  std::string id;
  MapTheme mapTheme = MapTheme::Plains;
  std::vector<std::string> monsters;
  std::string gatekeeper;
};

struct AbyssRunTables {
  std::vector<BoonDef> boons;
  std::array<double, EnumCount<BoonRarity>()> boonRarityWeight{};
  std::vector<CurseDef> curses;
  std::vector<FloorThemeDef> floorThemes;
  int32_t tierBaseLevel = 42;
  int32_t tierLevelStep = 2;
};

}  // namespace abyss

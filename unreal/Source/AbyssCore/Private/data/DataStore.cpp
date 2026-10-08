#include "abyss/base/Platform.h"

#include "abyss/data/DataStore.h"

#include <algorithm>
#include <cmath>

#include "JsonReader.h"
#include "abyss/base/StrUtil.h"
#include "abyss/base/Units.h"

namespace abyss {

using dataload::JNode;
using dataload::LoadCtx;

namespace {

constexpr std::string_view kRequired[] = {
    "abyss_run.json",      "achievements.json",   "affixes.json",         "anim_timing.json",
    "audio_cues.json",     "buff_caps.json",      "classes.json",         "combat_input.json",
    "crafting.json",       "dialogue_trees.json", "difficulty.json",      "economy.json",
    "elite_affixes.json",  "hero_formulas.json",  "hit_feedback.json",    "homestead.json",
    "i18n_en.json",        "i18n_zh-CN.json",     "item_bases.json",      "legendaries.json",
    "loot_rules.json",     "lore.json",           "map_gen.json",         "maps.json",
    "minibosses.json",     "monster_ai.json",     "monsters.json",        "music.json",
    "npcs.json",           "pets.json",           "projectile_timing.json", "quest_hunts.json",
    "quest_tuning.json",   "quests.json",         "random_events.json",   "render_quality.json",
    "sets.json",           "shops.json",          "skill_rules.json",     "skill_trees.json",
    "soul_echo.json",      "spirit_profiles.json", "status_effects.json", "story.json",
    "terrain_styles.json", "ui_theme.json",       "world_constants.json", "zone_moods.json",
};
constexpr std::string_view kOptional[] = {"i18n_zh-TW.json", "assets.json", "index.json"};

// Tables whose raw JSON stays available after loading (presentation / offline tables).
bool KeepRaw(std::string_view file) {
  return file == "terrain_styles.json" || file == "music.json" || file == "zone_moods.json" ||
         file == "ui_theme.json" || file == "index.json" || file == "map_gen.json";
}

}  // namespace

std::span<const std::string_view> RequiredDataFiles() { return kRequired; }
std::span<const std::string_view> OptionalDataFiles() { return kOptional; }

std::string DataLoadReport::Summary(size_t maxLines) const {
  std::string out = StrCat("data: ", loadedFiles.size(), " files, ", errors.size(), " errors, ", warnings.size(),
                           " warnings\n");
  size_t lines = 0;
  for (const DataIssue& e : errors) {
    if (lines++ >= maxLines) break;
    out += StrCat("  ERROR ", e.file, ": ", e.path, ": ", e.message, "\n");
  }
  for (const DataIssue& w : warnings) {
    if (lines++ >= maxLines) break;
    out += StrCat("  warn  ", w.file, ": ", w.path, ": ", w.message, "\n");
  }
  return out;
}

DataStore::DataStore() = default;
DataStore::~DataStore() = default;
DataStore::DataStore(DataStore&&) noexcept = default;
DataStore& DataStore::operator=(DataStore&&) noexcept = default;

void DataStore::Reset() {
  DataStore fresh;
  *this = std::move(fresh);
}

const JsonValue* DataStore::RawTable(std::string_view fileName) const {
  for (const auto& [name, v] : raw_) {
    if (name == fileName) return &v;
  }
  return nullptr;
}

bool DataStore::LoadAll(const DataFileReader& read, DataLoadReport& report) {
  Reset();
  report = DataLoadReport{};
  for (std::string_view f : kRequired) {
    std::string bytes;
    if (!read(f, bytes)) {
      report.errors.push_back(DataIssue{std::string(f), "", "required data file is missing"});
      continue;
    }
    LoadFile(f, bytes, report);
  }
  for (std::string_view f : kOptional) {
    std::string bytes;
    if (!read(f, bytes)) continue;
    LoadFile(f, bytes, report);
  }
  if (!report.Ok()) return false;
  return Finalize(report);
}

bool DataStore::LoadFile(std::string_view fileName, std::string_view bytes, DataLoadReport& report) {
  finalized_ = false;
  const size_t errorsBefore = report.errors.size();
  const std::string file(fileName);
  if (StartsWith(fileName, "i18n_")) {
    std::string err;
    if (!i18n_.LoadTable(bytes, &err)) report.errors.push_back(DataIssue{file, "", err});
  } else {
    JsonValue doc;
    JsonParseError perr;
    if (!ParseJson(bytes, doc, &perr)) {
      report.errors.push_back(DataIssue{file, "@" + std::to_string(perr.offset), "JSON parse error: " + perr.message});
      return false;
    }
    LoadCtx ctx(fileName, report);
    const JNode r = dataload::Root(doc, ctx);
    using namespace dataload;
    if (fileName == "classes.json") LoadClassesFile(r, classes_);
    else if (fileName == "skill_trees.json") LoadSkillTreesFile(r, classes_);
    else if (fileName == "skill_rules.json") LoadSkillRulesFile(r, classes_);
    else if (fileName == "hero_formulas.json") LoadHeroFormulasFile(r, classes_);
    else if (fileName == "buff_caps.json") LoadBuffCapsFile(r, classes_);
    else if (fileName == "spirit_profiles.json") LoadSpiritProfilesFile(r, classes_);
    else if (fileName == "status_effects.json") LoadStatusEffectsFile(r, classes_);
    else if (fileName == "combat_input.json") LoadCombatInputFile(r, combat_);
    else if (fileName == "projectile_timing.json") LoadProjectileTimingFile(r, combat_);
    else if (fileName == "anim_timing.json") LoadAnimTimingFile(r, combat_);
    else if (fileName == "hit_feedback.json") LoadHitFeedbackFile(r, combat_);
    else if (fileName == "elite_affixes.json") LoadEliteAffixesFile(r, combat_);
    else if (fileName == "difficulty.json") LoadDifficultyFile(r, combat_);
    else if (fileName == "soul_echo.json") LoadSoulEchoFile(r, combat_);
    else if (fileName == "item_bases.json") LoadItemBasesFile(r, items_);
    else if (fileName == "affixes.json") LoadAffixesFile(r, items_);
    else if (fileName == "sets.json") LoadSetsFile(r, items_);
    else if (fileName == "legendaries.json") LoadLegendariesFile(r, items_);
    else if (fileName == "economy.json") LoadEconomyFile(r, items_);
    else if (fileName == "loot_rules.json") LoadLootRulesFile(r, items_);
    else if (fileName == "crafting.json") LoadCraftingFile(r, items_);
    else if (fileName == "shops.json") LoadShopsFile(r, items_);
    else if (fileName == "monsters.json") LoadMonstersFile(r, monsters_);
    else if (fileName == "minibosses.json") LoadMiniBossesFile(r, monsters_);
    else if (fileName == "quest_hunts.json") LoadQuestHuntsFile(r, monsters_);
    else if (fileName == "monster_ai.json") LoadMonsterAiFile(r, monsters_);
    else if (fileName == "maps.json") LoadMapsFile(r, world_);
    else if (fileName == "map_gen.json") LoadMapGenFile(r, world_);
    else if (fileName == "world_constants.json") LoadWorldConstantsFile(r, world_);
    else if (fileName == "random_events.json") LoadRandomEventsFile(r, world_);
    else if (fileName == "zone_moods.json") LoadZoneMoodsFile(r, world_);
    else if (fileName == "quests.json") LoadQuestsFile(r, quests_);
    else if (fileName == "quest_tuning.json") LoadQuestTuningFile(r, quests_);
    else if (fileName == "achievements.json") LoadAchievementsFile(r, quests_);
    else if (fileName == "npcs.json") LoadNpcsFile(r, npcs_);
    else if (fileName == "dialogue_trees.json") LoadDialogueTreesFile(r, dialogues_);
    else if (fileName == "story.json") LoadStoryFile(r, story_);
    else if (fileName == "lore.json") LoadLoreFile(r, lore_);
    else if (fileName == "pets.json") LoadPetsFile(r, pets_);
    else if (fileName == "homestead.json") LoadHomesteadFile(r, homestead_);
    else if (fileName == "audio_cues.json") LoadAudioCuesFile(r, audio_);
    else if (fileName == "music.json") LoadMusicFile(r, audio_);
    else if (fileName == "assets.json") LoadAssetsFile(r, assets_);
    else if (fileName == "ui_theme.json") LoadUiThemeFile(r, uiTheme_);
    else if (fileName == "render_quality.json") LoadRenderQualityFile(r, renderQuality_);
    else if (fileName == "abyss_run.json") LoadAbyssRunFile(r, abyssRun_);
    else if (fileName == "terrain_styles.json" || fileName == "index.json") { /* raw only */ }
    else report.warnings.push_back(DataIssue{file, "", "unknown data file (ignored)"});
    if (KeepRaw(fileName)) {
      raw_.erase(std::remove_if(raw_.begin(), raw_.end(), [&](const auto& e) { return e.first == fileName; }),
                 raw_.end());
      raw_.emplace_back(file, std::move(doc));
    }
  }
  loadedFiles_.push_back(file);
  report.loadedFiles.push_back(file);
  return report.errors.size() == errorsBefore;
}

// =====================================================================================================================
// Finalize: defaults, port data fixes and cross-table validation
// =====================================================================================================================

namespace {

struct Checker {
  DataLoadReport& report;
  void Error(std::string_view file, std::string path, std::string msg) {
    report.errors.push_back(DataIssue{std::string(file), std::move(path), std::move(msg)});
  }
  void Warn(std::string_view file, std::string path, std::string msg) {
    report.warnings.push_back(DataIssue{std::string(file), std::move(path), std::move(msg)});
  }
};

bool Near(double a, double b) { return std::fabs(a - b) < 1e-9; }

}  // namespace

bool DataStore::Finalize(DataLoadReport& report) {
  Checker ck{report};
  const size_t errorsBefore = report.errors.size();

  // ---- classes & skills ----
  if (classes_.classes.size() != EnumCount<ClassId>()) {
    ck.Error("classes.json", "classes", "expected one definition per ClassId");
  }
  for (ClassDef& c : classes_.classes) {
    for (SkillDef& s : c.skills) {
      const std::string path = StrCat("classes.", c.id, ".skills.", s.id);
      // Fill absent scaling fields with skill_rules defaults (classes spec 1.2); presence bits come from ReadSkill.
      const SkillScaling& d = classes_.skillRules.scalingDefaults;
      SkillScaling& sc = s.scaling;
      if (!sc.Has(ScalingField::DamagePerLevel)) sc.damagePerLevel = d.damagePerLevel;
      if (!sc.Has(ScalingField::ManaCostPerLevel)) sc.manaCostPerLevel = d.manaCostPerLevel;
      if (!sc.Has(ScalingField::CooldownReductionPerLevel)) sc.cooldownReductionPerLevel = d.cooldownReductionPerLevel;
      if (!sc.Has(ScalingField::AoeRadiusPerLevel)) sc.aoeRadiusPerLevel = d.aoeRadiusPerLevel;
      if (!sc.Has(ScalingField::BuffValuePerLevel)) sc.buffValuePerLevel = d.buffValuePerLevel;
      if (!sc.Has(ScalingField::BuffDurationPerLevel)) sc.buffDurationPerLevel = d.buffDurationPerLevel;
      // C4 persistent ground effects carry their own lifetime and tick count (never a core constant).
      if (s.port.persistentGround) {
        if (s.port.groundTicks < 1) ck.Error("classes.json", path + ".derived.port.groundTicks", "persistentGround needs groundTicks >= 1");
        if (s.port.groundDurationMs <= 0) {
          ck.Error("classes.json", path + ".derived.port.groundDurationMs", "persistentGround needs groundDurationMs > 0");
        }
      } else if (s.port.groundTicks != 0 || s.port.groundDurationMs != 0) {
        ck.Error("classes.json", path + ".derived.port", "ground fields without persistentGround");
      }
      const SkillTreeDef* tree = classes_.FindTree(s.tree);
      if (!tree) {
        ck.Error("classes.json", path + ".tree", "unknown skill tree '" + s.tree + "'");
      } else if (tree->classId != c.cls) {
        ck.Error("classes.json", path + ".tree", "tree belongs to another class");
      }
      for (const SkillSynergy& syn : s.synergies) {
        if (!c.FindSkill(syn.skillId)) ck.Error("classes.json", path + ".synergies", "unknown skill '" + syn.skillId + "'");
      }
      if (s.tier < 1) ck.Error("classes.json", path + ".tier", "tier must be >= 1");
    }
  }
  for (const std::string& id : classes_.skillRules.groundAoeSkills) {
    const SkillDef* s = classes_.FindSkill(id);
    if (!s) ck.Error("skill_rules.json", "groundAoeSkills", "unknown skill '" + id + "'");
    else if (!s->groundAnchored) ck.Error("classes.json", "skills." + id, "groundAoeSkills lists it but derived.groundAnchored is false");
  }
  for (const std::string& id : classes_.skillRules.passiveSkills) {
    const SkillDef* s = classes_.FindSkill(id);
    if (!s) ck.Error("skill_rules.json", "passiveSkills", "unknown skill '" + id + "'");
    else if (!s->passive) ck.Error("classes.json", "skills." + id, "passiveSkills lists it but derived.passive is false");
  }
  for (size_t i = 0; i < EnumCount<ClassId>(); ++i) {
    if (classes_.spirit.profiles[i].classId != static_cast<ClassId>(i)) {
      ck.Error("spirit_profiles.json", "profiles", "missing profile for a class");
    }
  }

  // ---- units / timing consistency (S4, S5) ----
  if (!Near(combat_.projectiles.pxPerTileProjectile, kProjectilePxPerTile) ||
      !Near(combat_.projectiles.pxPerTileVfx, kVfxPxPerTile)) {
    ck.Error("projectile_timing.json", "portPxPerTile", "differs from Units.h (S4)");
  }
  if (!Near(world_.constants.portUnitsPerTile, kTileSizeUU)) {
    ck.Error("world_constants.json", "tile.portUnitsPerTile", "differs from Units.h kTileSizeUU (S4)");
  }
  if (!Near(world_.constants.heroPxPerTile, kHeroSpeedPxPerTile)) {
    ck.Error("world_constants.json", "heroSpeed.pxPerTile", "differs from Units.h (S5)");
  }
  for (size_t i = 0; i < EnumCount<ClassId>(); ++i) {
    const AnimRig rig = static_cast<AnimRig>(static_cast<size_t>(AnimRig::Warrior) + i);
    const RigContactDef& rc = combat_.anim.Contact(rig);
    if (!Near(rc.contactMsAtSpeed1, combat_.projectiles.heroContactMsAtSpeed1[i])) {
      ck.Error("projectile_timing.json", "heroBeats", "contact differs from anim_timing.contact");
    }
    if (!rc.hasCastReleaseMs || !Near(rc.castReleaseMs, combat_.projectiles.heroCastReleaseMs[i])) {
      ck.Error("projectile_timing.json", "heroBeats", "cast release differs from anim_timing.contact");
    }
  }

  // ---- items ----
  const auto requireBase = [&](std::string_view file, const std::string& path, const std::string& id) {
    if (!items_.FindBase(id)) ck.Error(file, path, "unknown item base '" + id + "'");
  };
  for (const ShopDef& s : items_.shops) {
    for (const std::string& id : s.items) requireBase("shops.json", "shops." + s.npcId, id);
  }
  for (const SetPieceBase& p : items_.setPieceBases) requireBase("sets.json", "pieceBases." + p.pieceId, p.baseId);
  for (const SetDef& s : items_.sets) {
    for (const std::string& piece : s.pieces) {
      if (!items_.FindSetPiece(piece)) ck.Error("sets.json", "sets." + s.id, "piece without a base: " + piece);
    }
  }
  for (const LegendaryDef& l : items_.legendaries) requireBase("legendaries.json", "legendaries." + l.id, l.baseId);
  for (const std::string& m : items_.materialIds) requireBase("item_bases.json", "materialIds", m);
  for (const std::string& m : items_.crafting.materials) requireBase("crafting.json", "materials", m);
  requireBase("loot_rules.json", "drops.leyFruitChance.itemId", items_.loot.leyFruitItemId);
  for (const ItemBaseDef& b : items_.bases) {
    if (b.type == ItemType::Gem && !b.isGem) ck.Error("item_bases.json", "gemStats." + b.id, "gem without a stat row");
  }

  // ---- monsters ----
  for (const ZoneMonsterList& z : monsters_.byZone) {
    for (const std::string& id : z.monsterIds) {
      if (!monsters_.Find(id)) ck.Error("monsters.json", "byZone." + z.zoneId, "unknown monster '" + id + "'");
    }
  }
  for (const MiniBossEntry& m : monsters_.miniBosses) {
    if (!monsters_.Find(m.monsterId)) ck.Error("minibosses.json", "byZone." + m.zoneId, "unknown monster '" + m.monsterId + "'");
    if (!monsters_.MiniBossDialogue(m.monsterId)) ck.Warn("minibosses.json", "dialogues", "no dialogue for " + m.monsterId);
  }
  for (const MonsterOverride& o : monsters_.overrides) {
    const MonsterDef* m = monsters_.Find(o.id);
    if (!m) {
      ck.Error("monsters.json", "overrides", "unknown monster '" + o.id + "'");
    } else if (o.field == "attackRange" && !Near(m->attackRange, o.port)) {
      ck.Error("monsters.json", "defs." + o.id, "override " + o.decision + " not applied to attackRange");
    }
  }
  for (const HuntDef& h : monsters_.hunts) {
    if (!monsters_.Find(h.monsterId)) ck.Error("quest_hunts.json", "hunts." + h.huntId, "unknown base monster");
    if (h.hasMinions && !monsters_.Find(h.minionMonsterId)) ck.Error("quest_hunts.json", "hunts." + h.huntId, "unknown minion monster");
    if (!quests_.Find(h.questId)) ck.Error("quest_hunts.json", "hunts." + h.huntId, "unknown quest '" + h.questId + "'");
  }

  // ---- world ----
  for (const std::string& id : world_.mapOrder) {
    if (!world_.FindMap(id)) ck.Error("maps.json", "order", "unknown map '" + id + "'");
  }
  // Geometry (finding: a generated map with cols/rows <= 0 or 60000 passed and made ZoneGrid allocate GBs). Every
  // authored position must lie on its grid; the ported MapGen / zone runtime index with these values.
  const auto inGrid = [](const MapDef& m, TilePos p) { return p.col >= 0 && p.row >= 0 && p.col < m.cols && p.row < m.rows; };
  const auto checkPos = [&](const MapDef& m, const std::string& where, TilePos p) {
    if (!inGrid(m, p)) {
      ck.Error("maps.json", where, StrCat("tile (", p.col, ",", p.row, ") outside the ", m.cols, "x", m.rows, " grid"));
    }
  };
  for (const MapDef& m : world_.maps) {
    const std::string path = "maps." + m.id;
    if (m.cols < 1 || m.rows < 1 || m.cols > kMaxMapDim || m.rows > kMaxMapDim) {
      ck.Error("maps.json", path, StrCat("map size ", m.cols, "x", m.rows, " outside 1..", kMaxMapDim));
      continue;  // positions cannot be checked against a bad grid
    }
    if (m.levelMin > m.levelMax) ck.Error("maps.json", path + ".levelRange", "levelMin > levelMax");
    checkPos(m, path + ".playerStart", m.playerStart);
    for (size_t i = 0; i < m.spawns.size(); ++i) {
      const MapSpawnDef& sp = m.spawns[i];
      const std::string spPath = StrCat(path, ".spawns[", i, "]");
      checkPos(m, spPath, sp.pos);
      if (sp.count < 0) ck.Error("maps.json", spPath + ".count", "negative spawn count");
    }
    for (size_t i = 0; i < m.camps.size(); ++i) checkPos(m, StrCat(path, ".camps[", i, "]"), m.camps[i].pos);
    for (size_t i = 0; i < m.exits.size(); ++i) {
      const MapExitDef& e = m.exits[i];
      checkPos(m, StrCat(path, ".exits[", i, "]"), e.pos);
      if (const MapDef* target = world_.FindMap(e.targetMap)) {
        if (target->cols >= 1 && target->rows >= 1 && !inGrid(*target, e.target)) {
          ck.Error("maps.json", StrCat(path, ".exits[", i, "].targetCol"),
                   StrCat("target tile (", e.target.col, ",", e.target.row, ") outside ", e.targetMap));
        }
      }
    }
    for (size_t i = 0; i < m.fieldNpcs.size(); ++i) checkPos(m, StrCat(path, ".fieldNpcs[", i, "]"), m.fieldNpcs[i].pos);
    for (size_t i = 0; i < m.petSpawns.size(); ++i) checkPos(m, StrCat(path, ".petSpawns[", i, "]"), m.petSpawns[i].pos);
    for (size_t i = 0; i < m.decorations.size(); ++i) checkPos(m, StrCat(path, ".decorations[", i, "]"), m.decorations[i].pos);
    for (size_t i = 0; i < m.storyDecorations.size(); ++i) {
      checkPos(m, StrCat(path, ".storyDecorations[", i, "]"), m.storyDecorations[i].pos);
    }
    for (size_t i = 0; i < m.subDungeonEntrances.size(); ++i) {
      checkPos(m, StrCat(path, ".subDungeonEntrances[", i, "]"), m.subDungeonEntrances[i].pos);
    }
    for (size_t i = 0; i < m.hiddenAreas.size(); ++i) {
      const HiddenAreaDef& h = m.hiddenAreas[i];
      const std::string hPath = StrCat(path, ".hiddenAreas[", i, "]");
      checkPos(m, hPath, h.center);
      if (!(h.radius >= 0)) ck.Error("maps.json", hPath + ".radius", "negative radius");
      if (h.hasBounds) {
        checkPos(m, hPath + ".start", h.boundsStart);
        checkPos(m, hPath + ".end", h.boundsEnd);
      }
      for (size_t r = 0; r < h.rewards.size(); ++r) checkPos(m, StrCat(hPath, ".rewards[", r, "]"), h.rewards[r].pos);
    }
    for (const MapSpawnDef& s : m.spawns) {
      if (!monsters_.FindForZone(m.id, s.monsterId)) ck.Error("maps.json", path + ".spawns", "unknown monster '" + s.monsterId + "'");
    }
    for (const MapCampDef& c : m.camps) {
      for (const std::string& n : c.npcs) {
        if (!npcs_.Find(n)) ck.Error("maps.json", path + ".camps", "unknown npc '" + n + "'");
      }
    }
    for (const FieldNpcDef& f : m.fieldNpcs) {
      if (!npcs_.Find(f.npcId)) ck.Error("maps.json", path + ".fieldNpcs", "unknown npc '" + f.npcId + "'");
    }
    for (const MapExitDef& e : m.exits) {
      if (!world_.FindMap(e.targetMap)) ck.Error("maps.json", path + ".exits", "unknown target map '" + e.targetMap + "'");
    }
    for (const SubDungeonEntranceDef& s : m.subDungeonEntrances) {
      if (!world_.FindSubDungeon(s.targetSubDungeon)) ck.Error("maps.json", path + ".subDungeonEntrances", "unknown sub-dungeon");
    }
    for (const PetSpawnDef& p : m.petSpawns) {
      if (!pets_.Find(p.petId)) ck.Error("maps.json", path + ".petSpawns", "unknown pet '" + p.petId + "'");
    }
    if (!m.generated) {
      if (m.tiles.size() != static_cast<size_t>(m.rows) || m.collisions.size() != static_cast<size_t>(m.rows)) {
        ck.Error("maps.json", path + ".tiles", "hand-built map rows differ from `rows`");
      }
      for (const std::string& row : m.tiles) {
        if (row.size() != static_cast<size_t>(m.cols)) ck.Error("maps.json", path + ".tiles", "row width differs from `cols`");
      }
    }
    MapTheme t{};
    if (!world_.moods.ThemeFor(m.id, t)) ck.Warn("zone_moods.json", "themeByZone", "no mood theme for " + m.id);
  }
  for (const SubDungeonDef& s : world_.subDungeons) {
    const std::string path = "subDungeons." + s.id;
    if (!world_.FindMap(s.parentZone)) ck.Error("maps.json", path, "unknown parent zone");
    if (!monsters_.Find(s.miniBossId)) ck.Error("maps.json", path, "unknown mini-boss '" + s.miniBossId + "'");
    if (s.cols < 1 || s.rows < 1 || s.cols > kMaxMapDim || s.rows > kMaxMapDim) {
      ck.Error("maps.json", path, StrCat("sub-dungeon size ", s.cols, "x", s.rows, " outside 1..", kMaxMapDim));
      continue;
    }
    if (s.levelMin > s.levelMax) ck.Error("maps.json", path + ".levelRange", "levelMin > levelMax");
    const auto inSub = [&](TilePos p) { return p.col >= 0 && p.row >= 0 && p.col < s.cols && p.row < s.rows; };
    for (const TilePos* p : {&s.playerStart, &s.miniBossPos, &s.exitPos}) {
      if (!inSub(*p)) ck.Error("maps.json", path, StrCat("tile (", p->col, ",", p->row, ") outside the sub-dungeon grid"));
    }
    for (size_t i = 0; i < s.spawns.size(); ++i) {
      if (!inSub(s.spawns[i].pos)) ck.Error("maps.json", StrCat(path, ".spawns[", i, "]"), "spawn outside the sub-dungeon grid");
      if (s.spawns[i].count < 0) ck.Error("maps.json", StrCat(path, ".spawns[", i, "].count"), "negative spawn count");
    }
  }
  // Art manifest footprints (W5 blocking loops over them).
  for (const AssetEntryDef& a : assets_.assets) {
    if (a.hasFootprint && (a.footprintW < 1 || a.footprintH < 1 || a.footprintW > kMaxFootprintTiles ||
                           a.footprintH > kMaxFootprintTiles)) {
      ck.Error("assets.json", "manifest." + a.name + ".footprintTiles",
               StrCat("footprint ", a.footprintW, "x", a.footprintH, " outside 1..", kMaxFootprintTiles));
    }
  }
  // M7 story boss data (monster_ai.json port.storyBoss).
  {
    const MonsterAiDef& ai = monsters_.ai;
    if (!ai.storyBossId.empty() && !monsters_.Find(ai.storyBossId)) {
      ck.Error("monster_ai.json", "port.storyBoss.monsterId", "unknown monster '" + ai.storyBossId + "'");
    }
    for (const std::string* q : {&ai.storyBossNotAfterQuestTurnIn, &ai.chapterCompleteQuest}) {
      if (!q->empty() && !quests_.Find(*q)) ck.Error("monster_ai.json", "port.storyBoss", "unknown quest '" + *q + "'");
    }
  }
  for (const ZoneEventDataDef& z : world_.randomEvents.zones) {
    if (!world_.FindMap(z.zoneId)) ck.Error("random_events.json", "zones." + z.zoneId, "unknown zone");
    for (const std::string& id : z.ambushMonsters) {
      if (!monsters_.FindForZone(z.zoneId, id)) {
        ck.Warn("random_events.json", "zones." + z.zoneId + ".ambushMonsters", "unknown monster '" + id + "' (skipped at spawn)");
      }
    }
  }

  // ---- NPCs, dialogue ----
  for (const NpcDef& n : npcs_.npcs) {
    const std::string path = "npcs." + n.id;
    for (const std::string& id : n.shopItems) requireBase("npcs.json", path + ".shopItems", id);
    for (const std::string& q : n.quests) {
      if (!quests_.Find(q)) ck.Error("npcs.json", path + ".quests", "unknown quest '" + q + "'");
    }
    if (!n.dialogueTreeId.empty() && !dialogues_.Find(n.dialogueTreeId)) {
      ck.Error("npcs.json", path + ".dialogueTreeId", "unknown tree '" + n.dialogueTreeId + "'");
    }
    if (!n.spriteId.empty() && !npcs_.Find(n.spriteId)) ck.Error("npcs.json", path + ".spriteId", "unknown npc");
  }
  for (const DialogueTree& t : dialogues_.trees) {
    const std::string path = "trees." + t.id;
    if (!t.FindNode(t.startNodeId)) ck.Error("dialogue_trees.json", path, "unknown startNodeId");
    for (const DialogueNode& n : t.nodes) {
      if (!n.nextNodeId.empty() && !t.FindNode(n.nextNodeId)) ck.Error("dialogue_trees.json", path + "." + n.id, "unknown nextNodeId");
      for (const DialogueChoice& c : n.choices) {
        if (!t.FindNode(c.nextNodeId)) ck.Error("dialogue_trees.json", path + "." + n.id, "choice to unknown node '" + c.nextNodeId + "'");
        if (!c.questTrigger.empty() && !quests_.Find(c.questTrigger)) ck.Error("dialogue_trees.json", path + "." + n.id, "unknown questTrigger");
        for (const std::string& id : c.reward.items) requireBase("dialogue_trees.json", path + "." + n.id + ".reward", id);
      }
    }
  }

  // ---- quests ----
  for (QuestDef& q : quests_.quests) {
    const std::string path = "quests." + q.id;
    if (!world_.FindMap(q.zone)) ck.Error("quests.json", path + ".zone", "unknown zone '" + q.zone + "'");
    for (const std::string& p : q.prereqQuests) {
      if (!quests_.Find(p)) ck.Error("quests.json", path + ".prereqQuests", "unknown quest '" + p + "'");
    }
    for (const std::string& id : q.rewards.items) requireBase("quests.json", path + ".rewards.items", id);
    if (!q.rewards.petReward.empty() && !pets_.Find(q.rewards.petReward)) ck.Error("quests.json", path, "unknown petReward");
    for (const QuestHuntRef& h : q.hunts) {
      if (!monsters_.FindHunt(h.huntId)) ck.Error("quests.json", path + ".hunts", "hunt missing in quest_hunts.json");
    }
    const NpcDef* giver = npcs_.GiverOf(q.id);
    if (!q.giverNpcId.empty() && (!giver || giver->id != q.giverNpcId)) {
      ck.Error("quests.json", path + ".derived.giverNpcId", "differs from the NPC that lists the quest");
    }
    for (const QuestObjectiveDef& o : q.objectives) {
      if (o.type == ObjectiveType::Kill && !monsters_.Find(o.targetId) && !monsters_.FindHunt(o.targetId)) {
        ck.Error("quests.json", path + ".objectives", "unknown kill target '" + o.targetId + "'");
      }
      if (o.type == ObjectiveType::Talk && !npcs_.Find(o.targetId)) {
        ck.Error("quests.json", path + ".objectives", "unknown talk target '" + o.targetId + "'");
      }
      for (const std::string& m : o.dropMonsters) {
        if (!monsters_.Find(m) && !monsters_.FindHunt(m)) ck.Error("quests.json", path + ".objectives.source", "unknown drop monster '" + m + "'");
      }
    }
  }
  // Q4 data fix: centre the story boss quest's questArea (q_find_goblin_chief) on the boss's spawn (DECISIONS Q4). The
  // quest and the boss come from monster_ai.json port.storyBoss (M7), not from literals.
  for (QuestDef& q : quests_.quests) {
    if (q.id != monsters_.ai.storyBossNotAfterQuestTurnIn || !q.hasQuestArea) continue;
    if (const MapDef* m = world_.FindMap(q.zone)) {
      for (const MapSpawnDef& sp : m->spawns) {
        if (sp.monsterId == monsters_.ai.storyBossId) {
          q.questArea.col = sp.pos.col;
          q.questArea.row = sp.pos.row;
          break;
        }
      }
    }
  }
  if (!i18n_.Lookup(LocaleId::ZhCN, world_.constants.sealedGateMessageKey) ||
      !i18n_.Lookup(LocaleId::En, world_.constants.sealedGateMessageKey)) {
    ck.Error("world_constants.json", "sealedChapter2Gate.messageKey",
             "i18n key missing in zh-CN or en: " + world_.constants.sealedGateMessageKey);
  }
  for (const AchievementDef& a : quests_.achievements) {
    if (a.type == AchievementType::Kill && !a.targetId.empty() && !monsters_.Find(a.targetId)) {
      ck.Error("achievements.json", "achievements." + a.id, "unknown monster target");
    }
  }

  // ---- story ----
  const auto requireKey = [&](std::string_view file, const std::string& path, const std::string& key) {
    if (key.empty()) return;
    if (!i18n_.Lookup(LocaleId::ZhCN, key) || !i18n_.Lookup(LocaleId::En, key)) {
      ck.Error(file, path, "i18n key missing in zh-CN or en: " + key);
    }
  };
  for (const StorySequence* seq : {&story_.prologue, &story_.epilogue, &story_.credits}) {
    for (const StorySlide& s : seq->slides) {
      requireKey("story.json", seq->id, s.heading);
      requireKey("story.json", seq->id, s.title);
      requireKey("story.json", seq->id, s.text);
    }
  }
  for (const ChapterCard& c : story_.chapters) {
    if (!world_.FindMap(c.zoneId)) ck.Error("story.json", "chapters", "unknown zone '" + c.zoneId + "'");
    for (const std::string* k : {&c.number, &c.title, &c.subtitle, &c.text}) requireKey("story.json", "chapters." + c.zoneId, *k);
  }
  for (const Cutscene& c : story_.cutscenes) {
    for (const CutsceneStep& s : c.steps) {
      requireKey("story.json", "cutscenes." + c.id, s.text);
      requireKey("story.json", "cutscenes." + c.id, s.title);
      requireKey("story.json", "cutscenes." + c.id, s.subtitle);
      for (const StoryActor* a : {&s.speaker, &s.target}) {
        if (a->kind == StoryActorKind::Npc && !npcs_.Find(a->id)) ck.Error("story.json", "cutscenes." + c.id, "unknown npc '" + a->id + "'");
        if (a->kind == StoryActorKind::Monster && !monsters_.Find(a->id)) ck.Error("story.json", "cutscenes." + c.id, "unknown monster '" + a->id + "'");
      }
    }
  }
  for (const BossIntroDef& b : story_.bossIntros) {
    if (!story_.FindCutscene(b.cutscene)) ck.Error("story.json", "bossIntros." + b.monsterId, "unknown cutscene");
    if (!monsters_.Find(b.monsterId)) ck.Error("story.json", "bossIntros." + b.monsterId, "unknown monster");
    requireKey("story.json", "bossIntros." + b.monsterId, b.name);
    requireKey("story.json", "bossIntros." + b.monsterId, b.epithet);
  }
  for (const StoryTriggerDef& t : story_.triggers) {
    if (!story_.FindCutscene(t.cutscene)) ck.Error("story.json", "triggers", "unknown cutscene '" + t.cutscene + "'");
    if (!t.grantPet.empty() && !pets_.Find(t.grantPet)) ck.Error("story.json", "triggers", "unknown pet '" + t.grantPet + "'");
    switch (t.on) {
      case StoryTriggerOn::QuestTurnedIn:
      case StoryTriggerOn::QuestAccepted:
        if (!quests_.Find(t.subjectId)) ck.Error("story.json", "triggers", "unknown quest '" + t.subjectId + "'");
        break;
      case StoryTriggerOn::MonsterKilled:
        if (!monsters_.Find(t.subjectId)) ck.Error("story.json", "triggers", "unknown monster '" + t.subjectId + "'");
        break;
      case StoryTriggerOn::ZoneEntered:
        if (!world_.FindMap(t.subjectId)) ck.Error("story.json", "triggers", "unknown zone '" + t.subjectId + "'");
        break;
    }
  }

  // ---- lore, pets, homestead, audio ----
  for (const LoreEntryDef& e : lore_.entries) {
    if (!world_.FindMap(e.zone)) ck.Error("lore.json", "byZone." + e.zone, "unknown zone");
  }
  for (const PetDef& p : pets_.pets) {
    bool found = p.primaryAbilityId.empty();
    for (const PetAbilityDef& a : p.abilities) found = found || a.id == p.primaryAbilityId;
    if (!found) ck.Error("pets.json", "pets." + p.id, "primaryAbilityId not among the abilities");
  }
  for (const std::string& id : pets_.chapter1Slice) {
    if (!pets_.Find(id)) ck.Error("pets.json", "port.chapter1Slice", "unknown pet '" + id + "'");
  }
  for (const BuildingDef& b : homestead_.buildings) {
    if (!b.unlockQuest.empty() && !quests_.Find(b.unlockQuest)) ck.Error("homestead.json", "buildings." + b.id, "unknown unlockQuest");
    if (!b.allyNpc.empty() && !npcs_.Find(b.allyNpc)) ck.Error("homestead.json", "buildings." + b.id, "unknown allyNpc");
    if (b.costPerLevel.size() != static_cast<size_t>(b.maxLevel)) ck.Error("homestead.json", "buildings." + b.id, "costPerLevel length != maxLevel");
  }
  if (!quests_.Find(homestead_.towerUnlockQuest)) ck.Error("homestead.json", "towerUnlockQuest", "unknown quest");
  for (size_t i = 0; i < EnumCount<SfxId>(); ++i) {
    if (!audio_.Find(static_cast<SfxId>(i))) {
      ck.Error("audio_cues.json", "cues", "missing cue '" + std::string(EnumName(static_cast<SfxId>(i))) + "'");
    }
  }

  // ---- i18n ----
  if (!i18n_.HasLocale(LocaleId::ZhCN) || !i18n_.HasLocale(LocaleId::En)) {
    ck.Error("i18n", "", "zh-CN and en tables are required");
  }
  for (const MonsterDef& m : monsters_.defs) {
    if (!i18n_.Lookup(LocaleId::ZhCN, m.nameKey)) ck.Warn("i18n_zh-CN.json", m.nameKey, "monster name key missing");
  }
  for (const QuestDef& q : quests_.quests) {
    if (!i18n_.Lookup(LocaleId::ZhCN, q.nameKey)) ck.Warn("i18n_zh-CN.json", q.nameKey, "quest name key missing");
  }

  finalized_ = report.errors.size() == errorsBefore;
  return finalized_;
}

}  // namespace abyss

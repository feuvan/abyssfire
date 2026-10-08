// Member functions of the typed table structs (lookups and small derived helpers).
#include "abyss/base/Platform.h"

#include <algorithm>
#include <cmath>

#include "abyss/data/DataStore.h"

namespace abyss {

// ---- SkillData ----------------------------------------------------------------------------------------------------
int32_t SkillRules::RequiredPlayerLevel(int32_t tier) const {
  if (tier >= 1 && static_cast<size_t>(tier) < tierPlayerLevel.size()) return tierPlayerLevel[static_cast<size_t>(tier)];
  return fallbackPlayerLevelBase + (tier - 1) * fallbackPlayerLevelPerTierAbove1;
}

int32_t SkillRules::RequiredTreePoints(int32_t tier) const {
  if (tier >= 1 && static_cast<size_t>(tier) < tierTreePoints.size()) return tierTreePoints[static_cast<size_t>(tier)];
  return std::max(fallbackTreePointsMin, (tier - 1) * fallbackTreePointsPerTierAbove1);
}

bool StatusEffectRules::Diminishes(StatusType t) const {
  return std::find(diminishingAppliesTo.begin(), diminishingAppliesTo.end(), t) != diminishingAppliesTo.end();
}

// ---- ClassData ----------------------------------------------------------------------------------------------------
const SkillDef* ClassDef::FindSkill(std::string_view skillId) const {
  for (const SkillDef& s : skills) {
    if (s.id == skillId) return &s;
  }
  return nullptr;
}

int32_t ClassDef::SkillIndex(std::string_view skillId) const {
  for (size_t i = 0; i < skills.size(); ++i) {
    if (skills[i].id == skillId) return static_cast<int32_t>(i);
  }
  return -1;
}

int64_t HeroFormulas::ExpToNext(int32_t level) const {
  const double l = static_cast<double>(level);
  return static_cast<int64_t>(std::floor(expToNextA * l * l + expToNextB * l));
}

double BuffCaps::Apply(BuffStat s, double total) const {
  const size_t i = static_cast<size_t>(s);
  return capped[i] ? std::min(total, cap[i]) : total;
}

const ClassDef* ClassTables::Find(ClassId c) const {
  for (const ClassDef& d : classes) {
    if (d.cls == c) return &d;
  }
  return nullptr;
}

const ClassDef* ClassTables::Find(std::string_view id) const {
  for (const ClassDef& d : classes) {
    if (d.id == id) return &d;
  }
  return nullptr;
}

const SkillDef* ClassTables::FindSkill(std::string_view skillId) const {
  for (const ClassDef& d : classes) {
    if (const SkillDef* s = d.FindSkill(skillId)) return s;
  }
  return nullptr;
}

const SkillTreeDef* ClassTables::FindTree(std::string_view treeId) const {
  for (const SkillTreeDef& t : trees) {
    if (t.id == treeId) return &t;
  }
  return nullptr;
}

// ---- CombatData ---------------------------------------------------------------------------------------------------
double AnimTimingTable::TransitionMs(std::string_view from, std::string_view to) const {
  for (const AnimTransition& t : transitions) {
    if (t.from == from && t.to == to) return t.ms;
  }
  return transitionDefaultMs;
}

void EliteAffixTable::CountFor(std::string_view zoneId, int32_t& outMin, int32_t& outMax) const {
  for (const ZoneAffixCount& z : zoneCounts) {
    if (z.zoneId == zoneId) {
      outMin = z.min;
      outMax = z.max;
      return;
    }
  }
  outMin = defaultCountMin;
  outMax = defaultCountMax;
}

// ---- ItemData -----------------------------------------------------------------------------------------------------
const std::vector<FixedAffixDef>* SetDef::AffixesForPiece(std::string_view pieceId) const {
  for (const SetPieceAffixes& p : pieceAffixes) {
    if (p.pieceId == pieceId) return &p.affixes;
  }
  return nullptr;
}

const ItemBaseDef* ItemTables::FindBase(std::string_view id) const {
  const int32_t i = baseIndex.Find(id);
  return i >= 0 ? &bases[static_cast<size_t>(i)] : nullptr;
}

int32_t ItemTables::BaseIndex(std::string_view id) const { return baseIndex.Find(id); }

const AffixDef* ItemTables::FindAffix(std::string_view id) const {
  for (const AffixDef& a : prefixes) {
    if (a.id == id) return &a;
  }
  for (const AffixDef& a : suffixes) {
    if (a.id == id) return &a;
  }
  return nullptr;
}

const SetDef* ItemTables::FindSet(std::string_view id) const {
  for (const SetDef& s : sets) {
    if (s.id == id) return &s;
  }
  return nullptr;
}

const SetPieceBase* ItemTables::FindSetPiece(std::string_view pieceId) const {
  for (const SetPieceBase& p : setPieceBases) {
    if (p.pieceId == pieceId) return &p;
  }
  return nullptr;
}

const LegendaryDef* ItemTables::FindLegendary(std::string_view id) const {
  for (const LegendaryDef& l : legendaries) {
    if (l.id == id) return &l;
  }
  return nullptr;
}

const LegendaryDef* ItemTables::FindLegendaryForBase(std::string_view baseId) const {
  for (const LegendaryDef& l : legendaries) {
    if (l.baseId == baseId) return &l;
  }
  return nullptr;
}

const ShopDef* ItemTables::FindShop(std::string_view npcId) const {
  for (const ShopDef& s : shops) {
    if (s.npcId == npcId) return &s;
  }
  return nullptr;
}

// ---- MonsterData --------------------------------------------------------------------------------------------------
const MonsterDef* MonsterTables::Find(std::string_view id) const {
  const int32_t i = defIndex.Find(id);
  return i >= 0 ? &defs[static_cast<size_t>(i)] : nullptr;
}

const ZoneMonsterList* MonsterTables::ZoneList(std::string_view zoneId) const {
  for (const ZoneMonsterList& z : byZone) {
    if (z.zoneId == zoneId) return &z;
  }
  return nullptr;
}

const MonsterDef* MonsterTables::FindForZone(std::string_view zoneId, std::string_view id) const {
  if (const ZoneMonsterList* z = ZoneList(zoneId)) {
    for (const std::string& m : z->monsterIds) {
      if (m == id) return Find(id);
    }
  }
  return Find(id);
}

const MiniBossEntry* MonsterTables::MiniBossFor(std::string_view zoneId) const {
  for (const MiniBossEntry& e : miniBosses) {
    if (e.zoneId == zoneId) return &e;
  }
  return nullptr;
}

const DialogueTree* MonsterTables::MiniBossDialogue(std::string_view monsterId) const {
  for (const DialogueTree& t : miniBossDialogues) {
    if (t.id == monsterId) return &t;
  }
  return nullptr;
}

const HuntDef* MonsterTables::FindHunt(std::string_view huntId) const {
  for (const HuntDef& h : hunts) {
    if (h.huntId == huntId) return &h;
  }
  return nullptr;
}

// ---- DialogueData -------------------------------------------------------------------------------------------------
const DialogueNode* DialogueTree::FindNode(std::string_view nodeId) const {
  for (const DialogueNode& n : nodes) {
    if (n.id == nodeId) return &n;
  }
  return nullptr;
}

const DialogueTree* DialogueTables::Find(std::string_view id) const {
  for (const DialogueTree& t : trees) {
    if (t.id == id) return &t;
  }
  return nullptr;
}

// ---- NpcData ------------------------------------------------------------------------------------------------------
const NpcDef* NpcTables::Find(std::string_view id) const {
  for (const NpcDef& n : npcs) {
    if (n.id == id) return &n;
  }
  return nullptr;
}

const NpcDef* NpcTables::GiverOf(std::string_view questId) const {
  for (const NpcDef& n : npcs) {
    for (const std::string& q : n.quests) {
      if (q == questId) return &n;
    }
  }
  return nullptr;
}

// ---- QuestData ----------------------------------------------------------------------------------------------------
const QuestDef* QuestTables::Find(std::string_view id) const {
  const int32_t i = questIndex.Find(id);
  return i >= 0 ? &quests[static_cast<size_t>(i)] : nullptr;
}

const AchievementDef* QuestTables::FindAchievement(std::string_view id) const {
  for (const AchievementDef& a : achievements) {
    if (a.id == id) return &a;
  }
  return nullptr;
}

std::string_view QuestTables::ItemKindFor(std::string_view targetId) const {
  for (const auto& [t, k] : tuning.itemKindByTarget) {
    if (t == targetId) return k;
  }
  return tuning.defaultItemKind;
}

// ---- StoryData ----------------------------------------------------------------------------------------------------
const Cutscene* StoryScript::FindCutscene(std::string_view id) const {
  for (const Cutscene& c : cutscenes) {
    if (c.id == id) return &c;
  }
  return nullptr;
}

const ChapterCard* StoryScript::ChapterFor(std::string_view zoneId) const {
  for (const ChapterCard& c : chapters) {
    if (c.zoneId == zoneId) return &c;
  }
  return nullptr;
}

const BossIntroDef* StoryScript::BossIntroFor(std::string_view monsterId) const {
  for (const BossIntroDef& b : bossIntros) {
    if (b.monsterId == monsterId) return &b;
  }
  return nullptr;
}

// ---- LoreData -----------------------------------------------------------------------------------------------------
const LoreEntryDef* LoreTables::Find(std::string_view id) const {
  for (const LoreEntryDef& e : entries) {
    if (e.id == id) return &e;
  }
  return nullptr;
}

std::vector<const LoreEntryDef*> LoreTables::ForZone(std::string_view zoneId) const {
  std::vector<const LoreEntryDef*> out;
  for (const LoreEntryDef& e : entries) {
    if (e.zone == zoneId) out.push_back(&e);
  }
  return out;
}

// ---- PetData ------------------------------------------------------------------------------------------------------
const PetDef* PetTables::Find(std::string_view id) const {
  for (const PetDef& p : pets) {
    if (p.id == id) return &p;
  }
  return nullptr;
}

const BuildingDef* HomesteadTables::FindBuilding(std::string_view id) const {
  for (const BuildingDef& b : buildings) {
    if (b.id == id) return &b;
  }
  return nullptr;
}

// ---- MapData ------------------------------------------------------------------------------------------------------
const ZoneEventDataDef* RandomEventsDef::ForZone(std::string_view zoneId) const {
  for (const ZoneEventDataDef& z : zones) {
    if (z.zoneId == zoneId) return &z;
  }
  return nullptr;
}

bool ZoneMoodTables::ThemeFor(std::string_view zoneId, MapTheme& out) const {
  for (const auto& [z, t] : themeByZone) {
    if (z == zoneId) {
      out = t;
      return true;
    }
  }
  return false;
}

const ZoneWeatherDef* ZoneMoodTables::WeatherFor(std::string_view zoneId) const {
  for (const ZoneWeatherDef& w : weather) {
    if (w.zoneId == zoneId) return &w;
  }
  return nullptr;
}

const MapDef* WorldTables::FindMap(std::string_view id) const {
  for (const MapDef& m : maps) {
    if (m.id == id) return &m;
  }
  return nullptr;
}

const SubDungeonDef* WorldTables::FindSubDungeon(std::string_view id) const {
  for (const SubDungeonDef& s : subDungeons) {
    if (s.id == id) return &s;
  }
  return nullptr;
}

// ---- AudioData ----------------------------------------------------------------------------------------------------
const AudioCueDef* AudioTables::Find(SfxId id) const {
  for (const AudioCueDef& c : cues) {
    if (c.id == id) return &c;
  }
  return nullptr;
}

// ---- AssetManifest ------------------------------------------------------------------------------------------------
const AnimClipDef* AssetEntryDef::FindClip(std::string_view clipName) const {
  for (const AnimClipDef& c : anims) {
    if (c.name == clipName) return &c;
  }
  return nullptr;
}

const AssetEntryDef* AssetManifest::FindAsset(std::string_view assetName) const {
  for (const AssetEntryDef& a : assets) {
    if (a.name == assetName) return &a;
  }
  return nullptr;
}

const AssetEntryDef* AssetManifest::FindByGameId(std::string_view gameId) const {
  auto it = std::lower_bound(gameIdToAsset.begin(), gameIdToAsset.end(), gameId,
                             [](const std::pair<std::string, std::string>& e, std::string_view k) { return e.first < k; });
  if (it == gameIdToAsset.end() || it->first != gameId) return nullptr;
  return FindAsset(it->second);
}

}  // namespace abyss

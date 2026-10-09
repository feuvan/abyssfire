// Monster definition pipeline (monsters-ai.md 1.2-1.3, 6.4, 9.3; combat-feel.md 14, 17.3). Owner area: monsters.
#include "abyss/base/Platform.h"

#include "abyss/monsters/MonsterDefs.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "abyss/base/Math.h"
#include "abyss/monsters/Monster.h"

namespace abyss {

MonsterDef ScaleMonsterForDifficulty(const MonsterDef& def, const DifficultyTable& table, Difficulty difficulty) {
  if (difficulty == Difficulty::Normal) return def;  // the web returns the same object
  const DifficultyDef& m = table.Def(difficulty);
  MonsterDef out = def;
  out.hp = JsRound(def.hp * m.hpMul);
  out.damage = JsRound(def.damage * m.damageMul);
  out.defense = JsRound(def.defense * m.defenseMul);
  out.expReward = JsRound(def.expReward * m.expMul);
  out.goldMin = JsRound(def.goldMin * m.expMul);  // gold uses the exp multiplier
  out.goldMax = JsRound(def.goldMax * m.expMul);
  return out;
}

MonsterDef MakeHuntDefinition(const MonsterDef& base, const HuntDef& hunt, const MonsterAiDef& ai,
                              std::string_view localizedName) {
  const double hpMul = hunt.hasHpMul ? hunt.hpMul : ai.huntHpMul;
  const double dmgMul = hunt.hasDmgMul ? hunt.dmgMul : ai.huntDmgMul;
  MonsterDef d = base;  // speed, attackRange, attackSpeed, level, spriteKey, animCategory, derived rules inherited
  d.id = hunt.huntId;
  d.name = std::string(localizedName);
  d.nameKey = "data.monster." + hunt.huntId;
  d.hp = JsRound(base.hp * hpMul);
  d.damage = JsRound(base.damage * dmgMul);
  d.defense = JsRound(base.defense * ai.huntDefMul);
  d.expReward = JsRound(base.expReward * (std::max)(ai.huntExpMulMin, hpMul));
  d.goldMin = base.goldMin * ai.huntGoldMul;  // not rounded (web: [g0 * 3, g1 * 3])
  d.goldMax = base.goldMax * ai.huntGoldMul;
  d.aggroRange = (std::max)(base.aggroRange, ai.huntAggroMin);
  d.elite = true;
  d.isMiniBoss = true;
  d.sources = {MonsterSource::Hunt};
  return d;
}

MonsterDef ScaleDefendWave(const MonsterDef& def, int32_t waveIndex, const MonsterAiDef& ai) {
  MonsterDef out = def;
  const double w = static_cast<double>(waveIndex);
  out.hp = std::floor(def.hp * (1 + w * ai.defendWaveHpPerWave));
  out.damage = std::floor(def.damage * (1 + w * ai.defendWaveDmgPerWave));
  return out;
}

MonsterDef RaiseToLevel(const MonsterDef& def, int32_t level, const MonsterAiDef& ai) {
  if (level <= 0 || def.level >= level - ai.raiseLevelWindow) return def;
  const double m = static_cast<double>(level) / static_cast<double>((std::max)(1, def.level));
  MonsterDef out = def;
  out.level = level;
  out.hp = JsRound(def.hp * std::pow(m, ai.raiseHpExp));
  out.damage = JsRound(def.damage * std::pow(m, ai.raiseDamageExp));
  out.defense = JsRound(def.defense * std::pow(m, ai.raiseDefenseExp));
  out.expReward = JsRound(def.expReward * std::pow(m, ai.raiseExpExp));
  out.goldMin = JsRound(def.goldMin * m);
  out.goldMax = JsRound(def.goldMax * m);
  return out;
}

MonsterDef ScaleLabyrinthMonster(const MonsterDef& def, const LabyrinthFloorScale& floor, const DifficultyTable& table,
                                 Difficulty difficulty, const MonsterAiDef& ai) {
  const MonsterDef base = RaiseToLevel(def, floor.levelTarget, ai);
  const DifficultyDef& d = table.Def(difficulty);
  const double depth = static_cast<double>(floor.floorNumber - 1);
  MonsterDef out = base;
  out.speed = JsRound(base.speed * floor.curseSpeedMul);
  out.attackSpeedMs = JsRound(base.attackSpeedMs / floor.curseSpeedMul);
  out.hp = JsRound(base.hp * floor.hpMul * d.hpMul);
  out.damage = JsRound(base.damage * floor.damageMul * d.damageMul);
  out.defense = JsRound(base.defense * floor.defenseMul * d.defenseMul);
  out.expReward = JsRound(base.expReward * (1 + depth * ai.labyrinthExpPerFloor) * d.expMul);
  out.goldMin = JsRound(base.goldMin * (1 + depth * ai.labyrinthGoldPerFloor));
  out.goldMax = JsRound(base.goldMax * (1 + depth * ai.labyrinthGoldPerFloor));
  return out;
}

MonsterDef MakeGatekeeper(const MonsterDef& base, const LabyrinthFloorScale& floor, const DifficultyTable& table,
                          Difficulty difficulty, const MonsterAiDef& ai, std::string_view nameKey) {
  const MonsterDef scaled = ScaleLabyrinthMonster(base, floor, table, difficulty, ai);
  const double hpMul = (base.isMiniBoss || base.elite) ? ai.gatekeeperHpMulElite : ai.gatekeeperHpMul;
  MonsterDef out = scaled;
  out.id = ai.gatekeeperId;
  out.nameKey = std::string(nameKey);
  out.hp = JsRound(scaled.hp * hpMul);
  out.damage = JsRound(scaled.damage * ai.gatekeeperDamageMul);
  out.expReward = JsRound(scaled.expReward * ai.gatekeeperExpMul);
  out.goldMin = scaled.goldMin * ai.gatekeeperGoldMul;  // not rounded (web: [g0 * 3, g1 * 3])
  out.goldMax = scaled.goldMax * ai.gatekeeperGoldMul;
  out.aggroRange = (std::max)(scaled.aggroRange, ai.gatekeeperAggroMin);
  out.elite = true;
  out.isMiniBoss = true;
  return out;
}

PrimaryStats MonsterBaseStats(const MonsterDef& def, const MonsterAiDef& ai) {
  PrimaryStats s;
  s.str = FloorInt(def.damage * ai.strPerDamage);
  s.dex = FloorInt(def.speed * ai.dexPerSpeed);
  s.vit = FloorInt(def.hp * ai.vitPerHp);
  s.int_ = ai.statInt;
  s.spi = ai.statSpi;
  s.lck = ai.statLck;
  return s;
}

CombinedAffixStats CombineAffixes(std::span<const EliteAffixType> affixes, const EliteAffixTable& table) {
  CombinedAffixStats c;
  for (EliteAffixType t : affixes) {
    const EliteAffixDef& d = table.Def(t);
    c.damageMult *= d.damageMult;
    c.speedMult *= d.speedMult;
    c.hpMult *= d.hpMult;
    c.defenseMult *= d.defenseMult;
    c.extraFireDamage += d.extraFireDamage;
    c.lootQualityBonus += d.lootQualityBonus;
    c.lifestealFraction += d.lifestealFraction;
    c.freezeChance = (std::min)(table.freezeChanceCap, c.freezeChance + d.freezeChance);  // capped per step (web)
    if (t == EliteAffixType::Teleporting) c.teleporting = true;
    if (t == EliteAffixType::CurseAura) {
      c.curseAura = true;
      c.curseAuraRadius = (std::max)(c.curseAuraRadius, d.curseAuraRadius);
      c.curseAuraReduction += d.curseAuraReduction;
    }
  }
  return c;
}

void ApplyEliteAffixes(MonsterInstance& m, std::span<const EliteAffixType> affixes, const EliteAffixTable& table,
                       const MonsterAiDef& ai) {
  m.affixes.clear();
  for (EliteAffixType t : affixes) m.affixes.push_back(MonsterAffix{t});
  if (affixes.empty()) return;
  const CombinedAffixStats c = CombineAffixes(affixes, table);
  m.maxHp = std::floor(m.maxHp * c.hpMult);
  m.hp = m.maxHp;
  m.def.damage = std::floor(m.def.damage * c.damageMult);
  m.def.speed = std::floor(m.def.speed * c.speedMult);
  m.def.defense = std::floor(m.def.defense * c.defenseMult);
  m.stats.str = FloorInt(m.def.damage * ai.strPerDamage);  // dex not recomputed (Q18, parity)
}

std::string_view MonsterNameKey(const MonsterDef& def) { return def.nameKey; }

std::string EliteAffixNameKey(EliteAffixType type) { return "sys.eliteAffix.name." + std::string(EnumName(type)); }

std::string MonsterLabelText(const I18n& i18n, std::string_view nameKey, std::span<const EliteAffixType> affixes) {
  const std::string base = i18n.T(nameKey);
  if (affixes.empty()) return base;
  std::string out = "[";
  for (size_t i = 0; i < affixes.size(); ++i) {
    if (i > 0) out += "\xC2\xB7";  // U+00B7 MIDDLE DOT (UTF-8), the web's affix separator
    out += i18n.T(EliteAffixNameKey(affixes[i]));
  }
  out += "] ";
  out += base;
  return out;
}

std::vector<std::string> MiniBossDialogueLineKeys(const DialogueTree& tree, std::string_view monsterId) {
  std::vector<std::string> keys;
  std::vector<std::string_view> visited;
  const DialogueNode* n = tree.FindNode(tree.startNodeId);
  while (n != nullptr) {
    if (std::find(visited.begin(), visited.end(), std::string_view(n->id)) != visited.end()) break;  // a cycle
    visited.push_back(n->id);
    keys.push_back("data.miniBossDialogue." + std::string(monsterId) + "." + n->id);
    if (n->isEnd || n->nextNodeId.empty()) break;
    n = tree.FindNode(n->nextNodeId);
  }
  return keys;
}

}  // namespace abyss

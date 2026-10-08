#include "abyss/base/Platform.h"

#include "JsonReader.h"

#include "abyss/monsters/Monster.h"

namespace abyss::dataload {

MonsterDef ReadMonsterDef(const JNode& n, bool withDerived) {
  MonsterDef m;
  m.id = n.Str("id");
  m.name = n.Str("name");
  m.level = n.Int("level");
  m.hp = n.Num("hp");
  m.damage = n.Num("damage");
  m.defense = n.Num("defense");
  m.speed = n.Num("speed");
  m.aggroRange = n.Num("aggroRange");
  m.attackRange = n.Num("attackRange");
  m.attackSpeedMs = n.Num("attackSpeed");
  m.expReward = n.Num("expReward");
  const std::vector<double> gold = n.NumList("goldReward");
  if (gold.size() == 2) {
    m.goldMin = gold[0];
    m.goldMax = gold[1];
  } else {
    n.Child("goldReward").Error("expected [min, max]");
  }
  m.spriteKey = n.Str("spriteKey", "");
  m.elite = n.Bool("elite", false);
  m.isMiniBoss = n.Bool("isMiniBoss", false);
  m.isSubDungeonMiniBoss = n.Bool("isSubDungeonMiniBoss", false);
  m.animCategory = n.Enum<AnimRig>("animCategory", AnimRig::Humanoid);
  m.isRanged = m.attackRange > 2.5;
  m.nameKey = "data.monster." + m.id;
  if (withDerived) {
    const JNode d = n.Child("derived");
    if (!d.Exists()) {
      n.Error("missing exporter 'derived' block");
      return m;
    }
    m.isRanged = d.Bool("isRanged");
    for (const JNode& s : d.Items("onHitStatus", true)) m.onHitStatus.push_back(ReadStatusRule(s));
    m.projectileColor = d.Color("projectileColor");
    m.homeZone = d.Str("homeZone", "");
    m.nameKey = d.Str("nameKey");
    m.sources = d.EnumList<MonsterSource>("sources");
  }
  return m;
}

DialogueTree ReadDialogueTree(const JNode& n, std::string id, DialogueKind kind) {
  DialogueTree t;
  t.id = std::move(id);
  t.kind = kind;
  t.startNodeId = n.Str("startNodeId");
  for (const auto& [nodeId, nn] : n.Members("nodes")) {
    DialogueNode node;
    node.id = nn.Str("id", nodeId);
    if (node.id != nodeId) nn.Child("id").Error("node id differs from its key");
    node.text = nn.Str("text");
    for (const JNode& c : nn.Items("choices", true)) {
      DialogueChoice ch;
      ch.text = c.Str("text");
      ch.nextNodeId = c.Str("nextNodeId");
      ch.questTrigger = c.Str("questTrigger", "");
      ch.prereqQuests = c.StrList("prereqQuests", true);
      if (c.Has("reward")) {
        const JNode rw = c.Child("reward");
        ch.reward.present = true;
        ch.reward.gold = rw.I64("gold", 0);
        ch.reward.exp = rw.I64("exp", 0);
        ch.reward.items = rw.StrList("items", true);
      }
      node.choices.push_back(std::move(ch));
    }
    node.nextNodeId = nn.Str("nextNodeId", "");
    node.isEnd = nn.Bool("isEnd", false);
    t.nodes.push_back(std::move(node));
  }
  return t;
}

void LoadMonstersFile(const JNode& r, MonsterTables& out) {
  out.defs.clear();
  out.defIndex.Clear();
  out.byZone.clear();
  out.overrides.clear();
  out.lookupOrder = r.StrList("lookupOrder");
  for (const auto& [id, n] : r.Members("defs")) {
    MonsterDef m = ReadMonsterDef(n, true);
    if (m.id != id) n.Child("id").Error("def id differs from its key");
    if (!out.defIndex.Add(m.id)) n.Error("duplicate monster id '" + m.id + "'");
    out.defs.push_back(std::move(m));
  }
  for (const auto& [zone, n] : r.Members("byZone")) {
    out.byZone.push_back(ZoneMonsterList{zone, r.Child("byZone").StrList(zone)});
  }
  const JNode d = r.Child("dungeon");
  out.dungeon.exclusive = d.StrList("exclusive");
  out.dungeon.boss = d.Str("boss");
  out.dungeon.midBoss = d.Str("midBoss");
  out.dungeon.pool = d.StrList("pool");
  for (const JNode& o : r.Items("overrides", true)) {
    MonsterOverride mo;
    mo.id = o.Str("id");
    mo.field = o.Str("field");
    mo.web = o.Num("web");
    mo.port = o.Num("port");
    mo.decision = o.Str("decision", "");
    out.overrides.push_back(std::move(mo));
  }
}

void LoadMiniBossesFile(const JNode& r, MonsterTables& out) {
  out.miniBosses.clear();
  out.miniBossDialogues.clear();
  for (const auto& [zone, n] : r.Members("byZone")) {
    MiniBossEntry e;
    e.zoneId = zone;
    e.monsterId = n.AsStr();
    const JNode sp = r.Child("spawns").Child(zone);
    if (sp.Exists()) {
      e.hasSpawn = true;
      e.spawn = sp.AsTile();
    }
    out.miniBosses.push_back(std::move(e));
  }
  out.subDungeonBosses = r.StrList("subDungeon");
  for (const auto& [id, n] : r.Members("dialogues")) {
    out.miniBossDialogues.push_back(ReadDialogueTree(n, id, DialogueKind::MiniBoss));
  }
}

void LoadQuestHuntsFile(const JNode& r, MonsterTables& out) {
  out.hunts.clear();
  const JNode defs = r.Child("defsNormal");
  for (const JNode& n : r.Items("hunts")) {
    HuntDef h;
    h.questId = n.Str("questId");
    h.zone = n.Str("zone");
    h.huntId = n.Str("huntId");
    h.monsterId = n.Str("monsterId");
    h.name = n.Str("name");
    h.spawn = TilePos{n.Int("col"), n.Int("row")};
    if (n.Has("hpMul")) {
      h.hasHpMul = true;
      h.hpMul = n.Num("hpMul");
    }
    if (n.Has("dmgMul")) {
      h.hasDmgMul = true;
      h.dmgMul = n.Num("dmgMul");
    }
    h.revealAfterPrevious = n.Bool("revealAfterPrevious", false);
    if (n.Has("minions")) {
      h.hasMinions = true;
      h.minionMonsterId = n.Child("minions").Str("monsterId");
      h.minionCount = n.Child("minions").Int("count");
    }
    const JNode def = defs.Child(h.huntId);
    if (def.Exists()) {
      h.defNormal = ReadMonsterDef(def, false);
      h.defNormal.nameKey = "data.monster." + h.huntId;
      h.defNormal.sources = {MonsterSource::Hunt};
    } else {
      n.Error("no defsNormal entry for hunt '" + h.huntId + "'");
    }
    out.hunts.push_back(std::move(h));
  }
}

void LoadMonsterAiFile(const JNode& r, MonsterTables& out) {
  MonsterAiDef& a = out.ai;
  a.leashRange = r.Num("leashRange");
  a.leashHealFractionPerTick = r.Num("leashHealFractionPerTick");
  a.patrolIntervalMs = r.Num("patrolIntervalMs");
  a.patrolRadius = r.Int("patrolRadius");
  a.arriveEpsilon = r.Num("arriveEpsilon");
  a.chaseDropMul = r.Num("chaseDropMul");
  a.attackExitMul = r.Num("attackExitMul");
  a.moveSpeedScale = r.Num("moveSpeedScale");
  a.moveAccel = r.Num("moveAccel");
  a.rangedThreshold = r.Num("rangedThreshold");
  a.meleeReachMul = r.Num("meleeReachMul");
  a.meleeReachAdd = r.Num("meleeReachAdd");
  a.swingQueryRadius = r.Num("swingQueryRadius");
  a.aiCullRadius = r.Num("aiCullRadius");
  a.activeRefreshMs = r.Num("activeRefreshMs");
  a.spawnJitter = r.Int("spawnJitter");
  a.respawnDelayMs = r.Num("respawnDelayMs");
  a.respawnJitter = r.Int("respawnJitter");
  a.safeZoneRadiusDefault = r.Num("safeZoneRadiusDefault");
  const JNode s = r.Child("monsterStats");
  a.strPerDamage = s.Num("strPerDamage");
  a.dexPerSpeed = s.Num("dexPerSpeed");
  a.vitPerHp = s.Num("vitPerHp");
  a.statInt = s.Int("int");
  a.statSpi = s.Int("spi");
  a.statLck = s.Int("lck");
  const JNode h = r.Child("hunt");
  a.huntHpMul = h.Num("hpMul");
  a.huntDmgMul = h.Num("dmgMul");
  a.huntDefMul = h.Num("defMul");
  a.huntExpMulMin = h.Num("expMulMin");
  a.huntGoldMul = h.Num("goldMul");
  a.huntAggroMin = h.Num("aggroMin");
  a.huntMinionJitter = h.Int("minionJitter");
  a.huntSpotSearchRadius = h.Int("spotSearchRadius");
  a.huntVisualScale = h.Num("visualScale");
  a.huntRevealShakeMs = h.Num("revealShakeMs");
  a.huntRevealShakeIntensity = h.Num("revealShakeIntensity");
  a.bossSightRange = r.Child("boss").Num("sightRange");
  a.bossBarRange = r.Child("boss").Num("barRange");
  a.finalBoss = r.Child("boss").Str("finalBoss");
  a.escortChipRadiusSq = r.Child("escortChip").Num("radiusSq");
  a.escortChipIntervalMs = r.Child("escortChip").Num("intervalMs");
  a.escortChipDamageMul = r.Child("escortChip").Num("damageMul");
  a.defendChipRadiusSq = r.Child("defendChip").Num("radiusSq");
  a.defendChipIntervalMs = r.Child("defendChip").Num("intervalMs");
  a.defendChipDamageMul = r.Child("defendChip").Num("damageMul");
  const JNode p = r.Child("port");
  a.leashMode = p.Child("leash").Enum<LeashMode>("mode");
  a.leashHealFractionPerSecond = p.Child("leash").Num("healFractionPerSecond");
  a.provokeDurationMs = p.Child("provoke").Num("durationMs");
  a.noRespawnRoles = p.StrList("noRespawn");
  a.miniBossOncePerVisit = p.Bool("miniBossOncePerVisit");
  const JNode sb = p.Child("storyBoss");
  a.storyBossId = sb.Str("monsterId");
  a.storyBossOncePerVisit = sb.Bool("oncePerVisit");
  a.storyBossNotAfterQuestTurnIn = sb.Str("notAfterQuestTurnIn");
  a.storyBossFarmableAfterChapter = sb.Bool("farmableAfterChapter");
  a.chapterCompleteQuest = sb.Str("chapterCompleteQuest");
  for (const std::string& role : a.noRespawnRoles) {
    MonsterRole parsed{};
    if (!ParseEnum(role, parsed)) p.Child("noRespawn").Error("unknown monster role '" + role + "'");
  }
  a.patrolTimeoutMs = p.Num("patrolTimeoutMs");
  a.placementTries = p.Int("placementTries");
  a.yawTurnRateDegPerSec = p.Num("yawTurnRateDegPerSec");
}

}  // namespace abyss::dataload

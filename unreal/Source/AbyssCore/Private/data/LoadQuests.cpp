#include "abyss/base/Platform.h"

#include "JsonReader.h"

namespace abyss::dataload {
namespace {

// Q8: once both quests.json and quest_tuning.json are loaded (either order), escort objectives without their own label
// show the quest name instead of the exported "to the southern camp" target text. Idempotent.
void ApplyEscortLabelFix(QuestTables& out) {
  if (!out.tuning.escortLabelFix) return;
  for (QuestDef& q : out.quests) {
    for (QuestObjectiveDef& o : q.objectives) {
      if (o.type == ObjectiveType::Escort && o.labelKey.empty() && !q.nameKey.empty()) o.labelKey = q.nameKey;
    }
  }
}

QuestObjectiveDef ReadObjective(const JNode& n) {
  QuestObjectiveDef o;
  o.type = n.Enum<ObjectiveType>("type");
  o.targetId = n.Str("targetId");
  o.targetName = n.Str("targetName", "");
  o.required = n.Int("required");
  if (n.Has("location")) {
    o.hasLocation = true;
    o.location = n.Child("location").AsCircle();
  }
  if (n.Has("source")) {
    const JNode s = n.Child("source");
    const std::string kind = s.Str("kind");
    if (kind == "drop") {
      o.sourceKind = ItemSourceKind::Drop;
      o.dropMonsters = s.StrList("monsters");
      o.dropChance = s.Num("chance");
    } else if (kind == "gather") {
      o.sourceKind = ItemSourceKind::Gather;
      o.gatherArea = s.Child("area").AsCircle();
      o.gatherCount = s.Int("count");
    } else {
      s.Child("kind").Error("unknown item source kind '" + kind + "'");
    }
  }
  o.itemKind = n.Str("itemKind", "");
  o.labelKey = n.Str("labelKey", "");
  return o;
}

StorySlide ReadSlide(const JNode& n) {
  StorySlide s;
  s.heading = n.Str("heading", "");
  s.title = n.Str("title", "");
  s.text = n.Str("text", "");
  if (n.Has("mood")) {
    s.hasMood = true;
    s.mood = n.Enum<StoryMood>("mood");
  }
  return s;
}

StorySequence ReadSequence(const JNode& n) {
  StorySequence q;
  q.id = n.Str("id");
  for (const JNode& s : n.Items("slides")) q.slides.push_back(ReadSlide(s));
  q.credits = n.Bool("credits", false);
  return q;
}

// speaker / target: "player" | "hero" | "villain" | {npc} | {monster} | {col,row}
StoryActor ReadActor(const JNode& n) {
  StoryActor a;
  const JsonValue& v = n.V();
  if (v.IsString()) {
    const std::string_view s = v.AsString();
    if (s == "player") {
      a.kind = StoryActorKind::Player;
    } else if (s == "hero") {
      a.kind = StoryActorKind::Hero;
    } else if (s == "villain") {
      a.kind = StoryActorKind::Villain;
    } else {
      n.Error("unknown story actor '" + std::string(s) + "'");
    }
  } else if (v.IsObject()) {
    if (n.Has("npc")) {
      a.kind = StoryActorKind::Npc;
      a.id = n.Str("npc");
    } else if (n.Has("monster")) {
      a.kind = StoryActorKind::Monster;
      a.id = n.Str("monster");
    } else if (n.Has("col")) {
      a.kind = StoryActorKind::Tile;
      a.tile = n.AsTile();
    } else {
      n.Error("unknown story actor object");
    }
  } else {
    n.Error("expected a story actor");
  }
  return a;
}

}  // namespace

void LoadQuestsFile(const JNode& r, QuestTables& out) {
  out.quests.clear();
  out.questIndex.Clear();
  for (const JNode& n : r.Items("quests")) {
    QuestDef q;
    q.id = n.Str("id");
    q.name = n.Str("name");
    q.description = n.Str("description", "");
    q.zone = n.Str("zone");
    q.type = n.Enum<QuestType>("type");
    q.category = n.Enum<QuestCategory>("category");
    for (const JNode& o : n.Items("objectives")) q.objectives.push_back(ReadObjective(o));
    const JNode rw = n.Child("rewards");
    q.rewards.exp = rw.I64("exp");
    q.rewards.gold = rw.I64("gold");
    q.rewards.items = rw.StrList("items", true);
    q.rewards.petReward = rw.Str("petReward", "");
    if (rw.Has("embers")) {
      q.rewards.hasEmbers = true;
      q.rewards.embers = rw.Int("embers");
    }
    q.rewards.choices = rw.EnumList<RewardSlot>("choices", true);
    if (rw.Has("choiceQuality")) {
      q.rewards.hasChoiceQuality = true;
      q.rewards.choiceQuality = rw.Enum<ItemQuality>("choiceQuality");
    }
    q.prereqQuests = n.StrList("prereqQuests", true);
    q.level = n.Int("level");
    if (n.Has("questArea")) {
      q.hasQuestArea = true;
      q.questArea = n.Child("questArea").AsCircle();
    }
    if (n.Has("escortNpc")) {
      const JNode e = n.Child("escortNpc");
      q.hasEscortNpc = true;
      q.escortNpc.name = e.Str("name");
      q.escortNpc.spriteKey = e.Str("spriteKey", "");
      q.escortNpc.start = TilePos{e.Int("startCol"), e.Int("startRow")};
      q.escortNpc.dest = TilePos{e.Int("destCol"), e.Int("destRow")};
    }
    if (n.Has("defendTarget")) {
      const JNode d = n.Child("defendTarget");
      q.hasDefendTarget = true;
      q.defendTarget.name = d.Str("name");
      q.defendTarget.spriteKey = d.Str("spriteKey", "");
      q.defendTarget.pos = TilePos{d.Int("col"), d.Int("row")};
      q.defendTarget.totalWaves = d.Int("totalWaves");
    }
    for (const JNode& c : n.Items("clues", true)) {
      q.clues.push_back(QuestClueDef{c.Str("id"), c.Str("name", ""), TilePos{c.Int("col"), c.Int("row")}});
    }
    if (n.Has("craftPhases")) {
      const JNode c = n.Child("craftPhases");
      q.craftPhases.present = true;
      for (const JNode& m : c.Items("materials", true)) {
        q.craftPhases.materials.push_back(CraftMaterialReq{m.Str("itemId"), m.Str("name", ""), m.Int("required")});
      }
      q.craftPhases.craftNpc = c.Str("craftNpc");
      q.craftPhases.deliverNpc = c.Str("deliverNpc");
    }
    q.reacceptable = n.Bool("reacceptable", false);
    for (const JNode& h : n.Items("hunts", true)) q.hunts.push_back(QuestHuntRef{h.Str("huntId")});
    const JNode d = n.Child("derived");
    q.giverNpcId = d.Str("giverNpcId", "");
    const std::vector<JNode> kinds = d.Items("objectiveItemKinds", true);
    for (size_t i = 0; i < kinds.size() && i < q.objectives.size(); ++i) {
      if (kinds[i].Exists() && q.objectives[i].itemKind.empty()) q.objectives[i].itemKind = kinds[i].AsStr();
    }
    q.embersOnTurnIn = d.Int("embersOnTurnIn");
    const JNode i18n = d.Child("i18n");
    q.nameKey = i18n.Str("name");
    q.descKey = i18n.Str("desc");
    q.offerKey = i18n.Str("offer");
    q.completeKey = i18n.Str("complete");
    q.orderIndex = static_cast<int32_t>(out.quests.size());
    if (!out.questIndex.Add(q.id)) n.Child("id").Error("duplicate quest id '" + q.id + "'");
    out.quests.push_back(std::move(q));
  }
  ApplyEscortLabelFix(out);
}

void LoadQuestTuningFile(const JNode& r, QuestTables& out) {
  QuestTuningDef& t = out.tuning;
  t.levelGateAbove = r.Int("levelGateAbove");
  t.gatherRange = r.Num("gatherRange");
  t.clueRange = r.Num("clueRange");
  t.guideNear = r.Num("guideNear");
  t.fallbackCollectChance = r.Num("fallbackCollectChance");
  t.itemKindByTarget.clear();
  for (const auto& [k, n] : r.Members("itemKindByTarget")) t.itemKindByTarget.emplace_back(k, n.AsStr());
  t.defaultItemKind = r.Str("defaultItemKind");
  const JNode e = r.Child("escort");
  t.escortHpBase = e.Num("hpBase");
  t.escortHpPerLevel = e.Num("hpPerLevel");
  t.escortSpeedDivisor = e.Num("speedDivisor");
  t.escortSpeedFactor = e.Num("speedFactor");
  t.escortCatchUpTiles = e.Num("catchUpTiles");
  t.escortArriveEscortSq = e.Num("arriveEscortSq");
  t.escortArriveHeroSq = e.Num("arriveHeroSq");
  t.escortThreatRangeSq = e.Num("threatRangeSq");
  t.escortHitIntervalMs = e.Num("hitIntervalMs");
  t.escortDmgMul = e.Num("dmgMul");
  const JNode d = r.Child("defend");
  t.defendHpBase = d.Num("hpBase");
  t.defendHpPerLevel = d.Num("hpPerLevel");
  t.defendStartRangeSq = d.Num("startRangeSq");
  t.defendWaveDelayMs = d.Num("waveDelayMs");
  t.defendHitRangeSq = d.Num("hitRangeSq");
  t.defendHitIntervalMs = d.Num("hitIntervalMs");
  t.defendDmgMul = d.Num("dmgMul");
  t.embersQuestMain = r.Child("embers").Int("questMain");
  t.embersQuestSide = r.Child("embers").Int("questSide");
  t.escortLabelFix = r.Has("port") && r.Child("port").Str("escortLabelFix", "") == "Q8";
  ApplyEscortLabelFix(out);
}

void LoadAchievementsFile(const JNode& r, QuestTables& out) {
  out.achievements.clear();
  for (const JNode& n : r.Items("achievements")) {
    AchievementDef a;
    a.id = n.Str("id");
    a.name = n.Str("name");
    a.description = n.Str("description", "");
    a.title = n.Str("title", "");
    a.type = n.Enum<AchievementType>("type");
    a.targetId = n.Str("targetId", "");
    a.required = n.Int("required");
    if (n.Has("reward")) {
      a.hasReward = true;
      a.rewardStat = n.Child("reward").Enum<Stat>("stat");
      a.rewardValue = n.Child("reward").Num("value");
    }
    out.achievements.push_back(std::move(a));
  }
  const JNode p = r.Child("port");
  out.achievementsCountKillOnce = p.Str("killCounting", "") == "one count per kill";
  out.achievementsExploreDistinct = p.Str("exploreAll", "") == "distinct zones";
}

void LoadNpcsFile(const JNode& r, NpcTables& out) {
  out.npcs.clear();
  for (const auto& [id, n] : r.Members("npcs")) {
    NpcDef d;
    d.id = n.Str("id");
    if (d.id != id) n.Child("id").Error("npc id differs from its key");
    d.name = n.Str("name");
    d.type = n.Enum<NpcType>("type");
    d.dialogue = n.StrList("dialogue");
    d.shopItems = n.StrList("shopItems", true);
    d.quests = n.StrList("quests", true);
    d.spriteId = n.Str("spriteId", "");
    d.dialogueTreeId = n.Str("dialogueTreeId", "");
    d.nameKey = n.Str("nameKey");
    out.npcs.push_back(std::move(d));
  }
}

void LoadDialogueTreesFile(const JNode& r, DialogueTables& out) {
  out.trees.clear();
  for (const auto& [id, n] : r.Members("trees")) {
    const std::string kind = n.Str("kind");
    DialogueKind k = DialogueKind::Npc;
    if (kind == "miniBoss") {
      k = DialogueKind::MiniBoss;
    } else if (kind != "npc") {
      n.Child("kind").Error("unknown dialogue kind '" + kind + "'");
    }
    out.trees.push_back(ReadDialogueTree(n, id, k));
  }
}

void LoadStoryFile(const JNode& r, StoryScript& out) {
  out = StoryScript{};
  out.prologue = ReadSequence(r.Child("prologue"));
  out.epilogue = ReadSequence(r.Child("epilogue"));
  out.credits = ReadSequence(r.Child("credits"));
  for (const JNode& c : r.Items("chapters")) {
    ChapterCard card;
    card.zoneId = c.Str("zoneId");
    card.number = c.Str("number");
    card.title = c.Str("title");
    card.subtitle = c.Str("subtitle");
    card.text = c.Str("text");
    card.mood = c.Enum<StoryMood>("mood");
    out.chapters.push_back(std::move(card));
  }
  for (const auto& [id, c] : r.Members("cutscenes")) {
    Cutscene cs;
    cs.id = c.Str("id");
    if (cs.id != id) c.Child("id").Error("cutscene id differs from its key");
    for (const JNode& s : c.Items("steps")) {
      CutsceneStep st;
      st.kind = s.Enum<StoryStepKind>("kind");
      st.text = s.Str("text", "");
      if (s.Has("speaker")) st.speaker = ReadActor(s.Child("speaker"));
      if (s.Has("target")) st.target = ReadActor(s.Child("target"));
      st.title = s.Str("title", "");
      st.subtitle = s.Str("subtitle", "");
      if (s.Has("ms")) {
        st.hasMs = true;
        st.ms = s.Num("ms");
      }
      if (s.Has("intensity")) {
        st.hasIntensity = true;
        st.intensity = s.Num("intensity");
      }
      if (s.Has("color")) {
        st.hasColor = true;
        st.color = s.Color("color");
      }
      cs.steps.push_back(std::move(st));
    }
    out.cutscenes.push_back(std::move(cs));
  }
  for (const JNode& b : r.Items("bossIntros")) {
    out.bossIntros.push_back(BossIntroDef{b.Str("monsterId"), b.Str("name"), b.Str("epithet"), b.Str("cutscene")});
  }
  for (const JNode& t : r.Items("triggers")) {
    StoryTriggerDef d;
    d.on = t.Enum<StoryTriggerOn>("on");
    switch (d.on) {
      case StoryTriggerOn::QuestTurnedIn:
      case StoryTriggerOn::QuestAccepted: d.subjectId = t.Str("questId"); break;
      case StoryTriggerOn::MonsterKilled: d.subjectId = t.Str("monsterId"); break;
      case StoryTriggerOn::ZoneEntered: d.subjectId = t.Str("zoneId"); break;
    }
    d.cutscene = t.Str("cutscene");
    d.grantPet = t.Str("grantPet", "");
    out.triggers.push_back(std::move(d));
  }
  for (const auto& [k, m] : r.Members("moods")) {
    StoryMoodColors c;
    if (!ParseEnum(k, c.mood)) m.Error("unknown mood");
    c.top = m.Str("top");
    c.bottom = m.Str("bottom");
    c.glow = m.Str("glow");
    c.embers = m.Color("embers");
    out.moods.push_back(std::move(c));
  }
  const JNode t = r.Child("timing");
  const JNode bd = t.Child("beatDelayMs");
  out.timing.beatDelayQuestTurnedInMs = bd.Num("quest_turned_in");
  out.timing.beatDelayQuestAcceptedMs = bd.Num("quest_accepted");
  out.timing.beatDelayZoneEnteredMs = bd.Num("zone_entered");
  out.timing.beatDelayMonsterKilledMs = bd.Num("monster_killed");
  out.timing.bossSightTiles = t.Num("bossSightTiles");
  out.timing.bossBarRangeTiles = t.Num("bossBarRangeTiles");
  out.timing.finalBoss = t.Str("finalBoss");
  out.timing.cameraPanMs = t.Num("cameraPanMs");
  out.timing.letterboxPx = t.Num("letterboxPx");
  out.timing.letterboxMs = t.Num("letterboxMs");
  out.timing.typewriterCps = t.Num("typewriterCps");
  out.timing.chapterHoldMs = t.Num("chapterHoldMs");
  out.timing.focusDefaultMs = t.Num("focusDefaultMs");
  out.timing.creditsPxPerSec = t.Num("creditsPxPerSec");
  const JNode ph = t.Child("phases");
  const JNode seq = ph.Child("sequence");
  out.timing.sequenceBackdropInMs = seq.Num("backdropInMs");
  out.timing.sequenceMoodOutMs = seq.Num("moodOutMs");
  out.timing.sequenceMoodInMs = seq.Num("moodInMs");
  out.timing.slidePartInMs = seq.Num("slidePartInMs");
  out.timing.slideOutMs = seq.Num("slideOutMs");
  out.timing.sequenceBackdropOutMs = seq.Num("backdropOutMs");
  out.timing.chapterIntroMs = ph.Child("chapter").Num("introMs");
  out.timing.chapterOutroMs = ph.Child("chapter").Num("outroMs");
  const auto phase = [&](std::string_view key, StoryPhaseMs& o) {
    const JNode n = ph.Child(key);
    o.inMs = n.Num("inMs");
    o.outMs = n.Num("outMs");
  };
  phase("narrate", out.timing.narrate);
  phase("say", out.timing.say);
  phase("whisper", out.timing.whisper);
  phase("title", out.timing.title);
  out.timing.titleHoldMs = ph.Child("title").Num("holdMs");
}

void LoadLoreFile(const JNode& r, LoreTables& out) {
  out.entries.clear();
  for (const auto& [zone, list] : r.Members("byZone")) {
    for (const JNode& n : list.SelfItems()) {
      LoreEntryDef e;
      e.id = n.Str("id");
      e.zone = n.Str("zone");
      if (e.zone != zone) n.Child("zone").Error("lore zone differs from its byZone key");
      e.name = n.Str("name");
      e.text = n.Str("text");
      e.pos = TilePos{n.Int("col"), n.Int("row")};
      e.spriteType = n.Str("spriteType");
      e.hidden = n.Bool("hidden", false);
      out.entries.push_back(std::move(e));
    }
  }
  out.pickupRangeSq = r.Num("pickupRangeSq");
}

}  // namespace abyss::dataload

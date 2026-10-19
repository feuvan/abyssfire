// Save JSON top level (save-ui-input.md 3.2-3.7, 3.10, 11; U2, U3): migration on the generic tree (v1 -> v2 -> v3 ->
// v4), the typed parse with the load normalisations that need no game data, the stable serialisation (3.2 key order,
// then the v4 fields), the position fallback (3.7) and the slot summary. Per-section readers / writers live in the
// owning areas' Private/save files (SaveSections.h). Owner area: world.
#include "abyss/base/Platform.h"

#include "abyss/save/SaveIO.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "abyss/base/Assert.h"
#include "abyss/base/Math.h"
#include "abyss/data/DataStore.h"
#include "SaveSections.h"

namespace abyss {

namespace saveio_impl {

// JS truthiness of a member (`if (!save.field)`): missing, null, false, 0, NaN and "" are falsy; arrays and objects
// (even empty) are truthy.
bool SaveIoTruthy(const JsonValue* v) {
  if (v == nullptr) return false;
  switch (v->Type()) {
    case JsonType::Null:
      return false;
    case JsonType::Bool:
      return v->AsBool();
    case JsonType::Number: {
      const double d = v->AsDouble();
      return d != 0 && !std::isnan(d);
    }
    case JsonType::String:
      return !v->AsString().empty();
    case JsonType::Array:
    case JsonType::Object:
      return true;
  }
  return false;
}

// `if (!obj[key]) obj[key] = def` (JS assignment keeps an existing key's position, appends a new one).
void SaveIoDefault(JsonValue& obj, std::string_view key, JsonValue def) {
  if (!SaveIoTruthy(obj.Find(key))) obj.Set(key, std::move(def));
}

// The save's version as a number for the `version < N` gates: a number as is; anything else counts as 1 (a missing
// version is treated as the oldest format, so every migration fills its defaults).
double SaveIoVersion(const JsonValue& raw) {
  const JsonValue* v = raw.Find("version");
  if (v != nullptr && v->IsNumber() && std::isfinite(v->AsDouble())) return v->AsDouble();
  return 1;
}

// ensureItemSockets: every object item of an array without truthy `sockets` gets [].
void SaveIoEnsureSockets(JsonValue* items) {
  if (items == nullptr || !items->IsArray()) return;
  for (JsonValue& it : items->MutableItems()) {
    if (it.IsObject() && !SaveIoTruthy(it.Find("sockets"))) it.Set("sockets", JsonValue::Array());
  }
}

void SaveIoMigrateV1toV2(JsonValue& save) {
  // 1. top-level collections
  SaveIoDefault(save, "inventory", JsonValue::Array());
  SaveIoDefault(save, "equipment", JsonValue::Object());
  SaveIoDefault(save, "stash", JsonValue::Array());
  SaveIoDefault(save, "quests", JsonValue::Array());
  SaveIoDefault(save, "exploration", JsonValue::Object());
  SaveIoDefault(save, "achievements", JsonValue::Object());
  // 2. homestead
  if (!SaveIoTruthy(save.Find("homestead"))) {
    JsonValue h = JsonValue::Object();
    h.AppendMember("buildings", JsonValue::Object());
    h.AppendMember("pets", JsonValue::Array());
    save.Set("homestead", std::move(h));
  } else if (JsonValue* h = save.FindMutable("homestead"); h->IsObject()) {
    SaveIoDefault(*h, "buildings", JsonValue::Object());
    SaveIoDefault(*h, "pets", JsonValue::Array());
  }
  // 3. settings
  if (!SaveIoTruthy(save.Find("settings"))) {
    JsonValue st = JsonValue::Object();
    st.AppendMember("autoCombat", JsonValue::Bool(false));
    st.AppendMember("musicVolume", JsonValue::Number(0.5));
    st.AppendMember("sfxVolume", JsonValue::Number(0.7));
    st.AppendMember("autoLootMode", JsonValue::String("off"));
    save.Set("settings", std::move(st));
  }
  // 4. difficulty
  SaveIoDefault(save, "difficulty", JsonValue::String("normal"));
  SaveIoDefault(save, "completedDifficulties", JsonValue::Array());
  // 5. a truthy mercenary that is not an object -> undefined (absent)
  if (const JsonValue* m = save.Find("mercenary"); SaveIoTruthy(m) && !m->IsObject() && !m->IsArray()) {
    save.Remove("mercenary");
  }
  // 6. dialogue / id sets
  SaveIoDefault(save, "dialogueState", JsonValue::Object());
  SaveIoDefault(save, "miniBossDialogueSeen", JsonValue::Array());
  SaveIoDefault(save, "loreCollected", JsonValue::Array());
  SaveIoDefault(save, "discoveredHiddenAreas", JsonValue::Array());
  // 7. sockets on every item
  SaveIoEnsureSockets(save.FindMutable("inventory"));
  SaveIoEnsureSockets(save.FindMutable("stash"));
  if (JsonValue* eq = save.FindMutable("equipment"); eq != nullptr && eq->IsObject()) {
    for (JsonMember& m : eq->MutableMembers()) {
      if (m.value.IsObject() && !SaveIoTruthy(m.value.Find("sockets"))) m.value.Set("sockets", JsonValue::Array());
    }
  }
  // 8.
  save.Set("version", JsonValue::Integer(2));
}

// SpiritSystem(classId, player.spirit).toSaveState() (SpiritSystem.ts restore).
void SaveIoMigrateV2toV3(JsonValue& save, const DataStore* data) {
  if (JsonValue* player = save.FindMutable("player"); player != nullptr && player->IsObject()) {
    double value = 0, remaining = 0;
    const JsonValue* sp = player->Find("spirit");
    if (sp != nullptr && sp->IsObject()) {
      const JsonValue& v = sp->Get("value");
      const JsonValue& r = sp->Get("resonanceRemainingMs");
      value = v.IsNumber() && std::isfinite(v.AsDouble()) ? v.AsDouble() : 0.0;
      remaining = r.IsNumber() && std::isfinite(r.AsDouble()) ? r.AsDouble() : 0.0;
      double maxValue = std::numeric_limits<double>::infinity();
      double maxRemaining = std::numeric_limits<double>::infinity();
      if (data != nullptr) {
        ClassId cls = ClassId::Warrior;  // getSpiritProfile: unknown class -> warrior
        const JsonValue& c = save.Get("classId");
        if (c.IsString()) ParseEnum(c.AsString(), cls);
        const SpiritProfileDef& prof = data->Classes().spirit.For(cls);
        maxValue = prof.maxValue;
        maxRemaining = prof.resonanceDurationMs;
      }
      value = (std::min)((std::max)(value, 0.0), maxValue);
      remaining = (std::min)((std::max)(remaining, 0.0), maxRemaining);
      if (value <= 0 || remaining <= 0) remaining = 0;
    }
    JsonValue state = JsonValue::Object();
    state.AppendMember("value", JsonValue::Number(value));
    state.AppendMember("resonanceRemainingMs", JsonValue::Number(remaining));
    player->Set("spirit", std::move(state));
  }
  save.Set("version", JsonValue::Integer(3));
}

// v3 -> v4 (U2): the port's fields with their defaults (only when missing).
void SaveIoMigrateV3toV4(JsonValue& save) {
  auto addDefault = [&save](std::string_view key, JsonValue def) {
    if (!save.Has(key)) save.Set(key, std::move(def));
  };
  addDefault("slot", JsonValue::Integer(0));
  JsonValue potions = JsonValue::Array();
  potions.Append(JsonValue::String(""));
  potions.Append(JsonValue::String(""));
  addDefault("potionSlots", std::move(potions));
  addDefault("playTimeMs", JsonValue::Integer(0));
  addDefault("dialogueOnce", JsonValue::Array());
  addDefault("hiddenRewardsClaimed", JsonValue::Array());
  addDefault("itemUidCounter", JsonValue::Integer(1));
  addDefault("visitedZones", JsonValue::Array());
  save.Set("version", JsonValue::Integer(4));
}

// ---- typed reading helpers ----
std::string SaveIoString(const JsonValue& obj, std::string_view key, std::string_view def = {}) {
  const JsonValue& v = obj.Get(key);
  return std::string(v.IsString() ? v.AsString() : def);
}

double SaveIoNumber(const JsonValue& obj, std::string_view key, double def) {
  const JsonValue& v = obj.Get(key);
  return v.IsNumber() ? v.AsDouble() : def;
}

void SaveIoForceIdentified(std::vector<ItemInstance>& items) {
  for (ItemInstance& it : items) it.identified = true;
}

// One RNG stream: [s0, s1, s2, s3] integers in [0, 2^32), not all zero (xoshiro128** needs a non-zero state).
bool SaveIoReadRngState(const JsonValue& v, RngState& out) {
  if (!v.IsArray() || v.Size() != 4) return false;
  bool anyNonZero = false;
  for (size_t i = 0; i < 4; ++i) {
    const JsonValue& e = v.At(i);
    if (!e.IsInteger()) return false;
    const int64_t n = e.AsInt64(-1);
    if (n < 0 || n > static_cast<int64_t>(0xFFFFFFFFu)) return false;
    out.s[i] = static_cast<uint32_t>(n);
    anyNonZero = anyNonZero || n != 0;
  }
  return anyNonZero;
}

// rng (v4): {streamName: [s0..s3]} per RngStream. Presence is tracked per stream: a missing / malformed stream is
// marked `missing` (ApplySave seeds it) and the others are kept, so adding a stream to RngStream (or one bad entry)
// does not discard the saved states of the rest. No valid stream at all -> absent (every stream reseeded).
void SaveIoReadRng(const JsonValue& v, SaveRng& out) {
  out = SaveRng{};
  if (!v.IsObject()) return;
  for (size_t i = 0; i < kRngStreamCount; ++i) {
    const JsonValue* s = v.Find(EnumName(static_cast<RngStream>(i)));
    if (s == nullptr || !SaveIoReadRngState(*s, out.streams[i])) {
      out.streams[i] = RngState{};
      out.missing[i] = true;
    } else {
      out.present = true;
    }
  }
  if (!out.present) out = SaveRng{};
}

}  // namespace saveio_impl

bool MigrateRaw(JsonValue& raw, const DataStore* data) {
  using namespace saveio_impl;
  if (!raw.IsObject()) return false;
  if (SaveIoVersion(raw) < 2) SaveIoMigrateV1toV2(raw);
  if (SaveIoVersion(raw) < 3) SaveIoMigrateV2toV3(raw, data);
  if (SaveIoVersion(raw) < 4) SaveIoMigrateV3toV4(raw);
  return true;
}

SaveError ParseSave(std::string_view json, SaveData& out, std::string* errorMessage, const DataStore* data) {
  using namespace saveio_impl;
  auto fail = [errorMessage](SaveError e, std::string msg) {
    if (errorMessage != nullptr) *errorMessage = std::move(msg);
    return e;
  };
  JsonValue raw;
  JsonParseError perr;
  if (!ParseJson(json, raw, &perr)) {
    return fail(SaveError::ParseFailed, "save is not valid JSON at byte " + std::to_string(perr.offset) + ": " +
                                            perr.message);
  }
  if (!raw.IsObject()) return fail(SaveError::NotAnObject, "save root is not an object");
  // 3.3: a save from a newer build is refused, never downgraded.
  if (SaveIoVersion(raw) > kCurrentSaveVersion) {
    return fail(SaveError::VersionTooNew, "save version " + FormatJsonNumber(SaveIoVersion(raw)) +
                                              " is newer than this build (" +
                                              std::to_string(kCurrentSaveVersion) + ")");
  }
  MigrateRaw(raw, data);
  const JsonValue& player = raw.Get("player");
  if (!player.IsObject()) return fail(SaveError::Invalid, "save has no player object");

  SaveData s;
  s.id = SaveIoString(raw, "id", s.id);
  s.version = kCurrentSaveVersion;
  if (const JsonValue& ts = raw.Get("timestamp"); ts.IsNumber() && std::isfinite(ts.AsDouble())) {
    // Saturating (SaveHero HeroInt64 rules): a finite timestamp outside int64 (corrupt / hand-edited slot) must not hit
    // a raw double -> int64 cast (UB; x64 gives INT64_MIN, arm64 saturates).
    s.timestamp = ts.IsInteger() ? ts.AsInt64() : SaturatingInt64(std::floor(ts.AsDouble()));
  }
  ClassId cls = ClassId::Warrior;  // unknown class -> warrior (ZoneScene.ts:427)
  if (const JsonValue& c = raw.Get("classId"); c.IsString()) ParseEnum(c.AsString(), cls);
  s.classId = cls;

  savejson::ReadHero(player, s.player);
  savejson::ReadItemList(raw.Get("inventory"), s.inventory);
  savejson::ReadEquipment(raw.Get("equipment"), s);
  savejson::ReadItemList(raw.Get("stash"), s.stash);
  // 3.3: every item is identified on load (Q9 / I2).
  SaveIoForceIdentified(s.inventory);
  SaveIoForceIdentified(s.stash);
  for (std::optional<ItemInstance>& e : s.equipment) {
    if (e.has_value()) e->identified = true;
  }
  savejson::ReadQuests(raw.Get("quests"), s.quests);
  if (const JsonValue& ex = raw.Get("exploration"); ex.IsObject()) s.exploration = ex;  // dead data, kept verbatim
  savejson::ReadHomestead(raw.Get("homestead"), s.homestead);
  savejson::ReadPets(raw.Get("pets"), s.pets);
  savejson::ReadAchievements(raw.Get("achievements"), s.achievements);
  savejson::ReadSettings(raw.Get("settings"), s.settings);
  Difficulty diff = Difficulty::Normal;
  if (const JsonValue& d = raw.Get("difficulty"); d.IsString() && ParseEnum(d.AsString(), diff)) s.difficulty = diff;
  for (const JsonValue& d : raw.Get("completedDifficulties").Items()) {
    Difficulty cd = Difficulty::Normal;
    if (!d.IsString() || !ParseEnum(d.AsString(), cd)) continue;
    std::vector<Difficulty>& done = s.completedDifficulties;
    if (std::find(done.begin(), done.end(), cd) == done.end()) done.push_back(cd);
  }
  if (const JsonValue* m = raw.Find("mercenary"); m != nullptr && m->IsObject()) {
    s.hasMercenary = true;  // later milestone: kept verbatim
    s.mercenary = *m;
  }
  savejson::ReadDialogueState(raw.Get("dialogueState"), s.dialogueState);
  savejson::ReadStringList(raw.Get("miniBossDialogueSeen"), s.miniBossDialogueSeen);
  savejson::ReadStringList(raw.Get("loreCollected"), s.loreCollected);
  savejson::ReadStringList(raw.Get("discoveredHiddenAreas"), s.discoveredHiddenAreas);
  // storySeen ?? ['prologue'] (veterans skip the prologue).
  s.hasStorySeen = true;
  if (const JsonValue& seen = raw.Get("storySeen"); seen.IsArray()) {
    savejson::ReadStringList(seen, s.storySeen);
  } else {
    s.storySeen = {"prologue"};
  }
  savejson::ReadSoulEcho(raw.Get("soulEcho"), s.soulEcho);
  // abyss: unlockedTier = max(1, v ?? 1), bestTier = max(0, v ?? 0), bestTimeMs kept when it is a finite number.
  if (const JsonValue& ab = raw.Get("abyss"); ab.IsObject()) {
    const double unlocked = SaveIoNumber(ab, "unlockedTier", 1);
    const double best = SaveIoNumber(ab, "bestTier", 0);
    s.abyss.unlockedTier = (std::max)(1, std::isfinite(unlocked) ? FloorInt(unlocked) : 1);
    s.abyss.bestTier = (std::max)(0, std::isfinite(best) ? FloorInt(best) : 0);
    const JsonValue& bt = ab.Get("bestTimeMs");
    if (bt.IsNumber() && std::isfinite(bt.AsDouble())) {
      s.abyss.hasBestTime = true;
      s.abyss.bestTimeMs = bt.AsDouble();
    }
  }

  // ---- v4 ----
  if (const JsonValue& slot = raw.Get("slot"); slot.IsInteger()) {
    s.slot = static_cast<int32_t>(std::clamp<int64_t>(slot.AsInt64(), 0, std::numeric_limits<int32_t>::max()));
  }
  if (const JsonValue& hb = raw.Get("hotbar"); hb.IsArray()) {
    s.hasHotbar = true;
    for (size_t i = 0; i < s.hotbar.size(); ++i) {
      const JsonValue& e = hb.At(i);
      s.hotbar[i] = e.IsString() ? std::string(e.AsString()) : std::string();
    }
  }
  savejson::ReadPotionSlots(raw.Get("potionSlots"), s.potionSlots);
  if (const double pt = SaveIoNumber(raw, "playTimeMs", 0); std::isfinite(pt) && pt > 0) s.playTimeMs = pt;
  SaveIoReadRng(raw.Get("rng"), s.rng);
  savejson::ReadStringList(raw.Get("dialogueOnce"), s.dialogueOnce);
  savejson::ReadStringList(raw.Get("hiddenRewardsClaimed"), s.hiddenRewardsClaimed);
  s.itemUidCounter = savejson::ReadItemUidCounter(raw.Get("itemUidCounter"));
  savejson::ReadStringList(raw.Get("visitedZones"), s.visitedZones);
  // implementation-plan.md 2.7: saves written before the field are the web port's content (1).
  s.contentVersion = 1;
  if (const JsonValue& cv = raw.Get("contentVersion"); cv.IsInteger() && cv.AsInt64() >= 1) {
    s.contentVersion = static_cast<int32_t>(std::min<int64_t>(cv.AsInt64(), std::numeric_limits<int32_t>::max()));
  }

  out = std::move(s);
  if (errorMessage != nullptr) errorMessage->clear();
  return SaveError::None;
}

std::string SerializeSave(const SaveData& s, bool pretty) {
  JsonWriter w(pretty);
  w.StartObject();
  // ---- v3 fields in autoSave's write order (3.2) ----
  w.Key("id");
  w.String(s.id);
  w.Key("version");
  w.Int(kCurrentSaveVersion);
  w.Key("timestamp");
  w.Int(s.timestamp);
  w.Key("classId");
  w.String(EnumName(s.classId));
  w.Key("player");
  savejson::WriteHero(w, s.player);
  w.Key("inventory");
  savejson::WriteItemList(w, s.inventory);
  w.Key("equipment");
  savejson::WriteEquipment(w, s);
  w.Key("stash");
  savejson::WriteItemList(w, s.stash);
  w.Key("quests");
  savejson::WriteQuests(w, s.quests);
  w.Key("exploration");
  if (s.exploration.IsObject()) {
    w.Value(s.exploration);
  } else {
    w.StartObject();
    w.EndObject();
  }
  w.Key("homestead");
  savejson::WriteHomestead(w, s.homestead);
  if (s.pets.present) {
    w.Key("pets");
    savejson::WritePets(w, s.pets);
  }
  w.Key("achievements");
  savejson::WriteAchievements(w, s.achievements);
  w.Key("settings");
  savejson::WriteSettings(w, s.settings);
  w.Key("difficulty");
  w.String(EnumName(s.difficulty));
  w.Key("completedDifficulties");
  w.StartArray();
  for (Difficulty d : s.completedDifficulties) w.String(EnumName(d));
  w.EndArray();
  if (s.hasMercenary && s.mercenary.IsObject()) {
    w.Key("mercenary");
    w.Value(s.mercenary);
  }
  w.Key("dialogueState");
  savejson::WriteDialogueState(w, s.dialogueState);
  w.Key("miniBossDialogueSeen");
  savejson::WriteStringList(w, s.miniBossDialogueSeen);
  w.Key("loreCollected");
  savejson::WriteStringList(w, s.loreCollected);
  w.Key("discoveredHiddenAreas");
  savejson::WriteStringList(w, s.discoveredHiddenAreas);
  if (s.hasStorySeen) {
    w.Key("storySeen");
    savejson::WriteStringList(w, s.storySeen);
  }
  w.Key("soulEcho");
  savejson::WriteSoulEcho(w, s.soulEcho);
  w.Key("abyss");
  w.StartObject();
  w.Key("unlockedTier");
  w.Int(s.abyss.unlockedTier);
  w.Key("bestTier");
  w.Int(s.abyss.bestTier);
  if (s.abyss.hasBestTime) {
    w.Key("bestTimeMs");
    w.Double(s.abyss.bestTimeMs);
  }
  w.EndObject();
  // ---- v4 (U2) ----
  w.Key("slot");
  w.Int(s.slot);
  if (s.hasHotbar) {
    w.Key("hotbar");
    w.StartArray();
    for (const std::string& id : s.hotbar) {
      if (id.empty()) {
        w.Null();
      } else {
        w.String(id);
      }
    }
    w.EndArray();
  }
  w.Key("potionSlots");
  savejson::WritePotionSlots(w, s.potionSlots);
  w.Key("playTimeMs");
  w.Double(std::isfinite(s.playTimeMs) ? s.playTimeMs : 0.0);
  if (s.rng.present) {
    w.Key("rng");
    w.StartObject();
    for (size_t i = 0; i < kRngStreamCount; ++i) {
      if (s.rng.missing[i]) continue;  // parse -> serialise keeps a partial `rng` partial
      w.Key(EnumName(static_cast<RngStream>(i)));
      w.StartArray();
      for (uint32_t v : s.rng.streams[i].s) w.Int(static_cast<int64_t>(v));
      w.EndArray();
    }
    w.EndObject();
  }
  w.Key("dialogueOnce");
  savejson::WriteStringList(w, s.dialogueOnce);
  w.Key("hiddenRewardsClaimed");
  savejson::WriteStringList(w, s.hiddenRewardsClaimed);
  w.Key("itemUidCounter");
  savejson::WriteItemUidCounter(w, s.itemUidCounter);
  w.Key("visitedZones");
  savejson::WriteStringList(w, s.visitedZones);
  w.Key("contentVersion");
  w.Int(s.contentVersion);
  w.EndObject();
  std::string out = w.Take();
  ABYSS_ASSERT(!out.empty(), "SerializeSave produced incomplete JSON");
  return out;
}

namespace {

// Removes the entries `keep` rejects; returns how many went.
template <class T, class Keep>
int32_t SaveIoDropIf(std::vector<T>& v, Keep keep) {
  const size_t before = v.size();
  v.erase(std::remove_if(v.begin(), v.end(), [&keep](const T& e) { return !keep(e); }), v.end());
  return static_cast<int32_t>(before - v.size());
}

// The beat ids StoryProgress records (StoryDirector): the sequences, a zone's chapter card, a cutscene, a boss intro.
bool SaveIoKnownBeat(const DataStore& data, std::string_view id) {
  constexpr std::string_view kChapter = "chapter_", kBoss = "boss_";
  const StoryScript& story = data.Story();
  if (id == "prologue" || id == "epilogue") return true;
  if (story.FindCutscene(id) != nullptr) return true;
  if (id.substr(0, kChapter.size()) == kChapter) return story.ChapterFor(id.substr(kChapter.size())) != nullptr;
  if (id.substr(0, kBoss.size()) == kBoss) return story.BossIntroFor(id.substr(kBoss.size())) != nullptr;
  return false;
}

const DialogueTree* SaveIoTreeOf(const DataStore& data, std::string_view npcId) {
  const NpcDef* npc = data.FindNpc(npcId);
  return npc != nullptr && !npc->dialogueTreeId.empty() ? data.Dialogues().Find(npc->dialogueTreeId) : nullptr;
}

bool SaveIoKnownHiddenArea(const DataStore& data, std::string_view id) {
  for (const MapDef& m : data.World().maps) {
    for (const HiddenAreaDef& h : m.hiddenAreas) {
      if (h.id == id) return true;
    }
  }
  return false;
}

}  // namespace

int32_t ApplyContentVersion(const DataStore& data, SaveData& s) {
  if (s.contentVersion >= kCurrentContentVersion) return 0;
  int32_t dropped = 0;
  dropped += SaveIoDropIf(s.quests, [&](const QuestProgress& p) { return data.FindQuest(p.questId) != nullptr; });
  dropped += SaveIoDropIf(s.storySeen, [&](const std::string& id) { return SaveIoKnownBeat(data, id); });
  // Beats whose id survived but whose content was rewritten (story.json contentUpgrade: the remade prologue and the
  // Red Field's chapter card) are forgotten too, so an older save sees them again.
  if (s.contentVersion < data.Story().contentUpgradeVersion) {
    const std::vector<std::string>& replay = data.Story().contentReplayBeats;
    dropped += SaveIoDropIf(s.storySeen, [&](const std::string& id) {
      return std::find(replay.begin(), replay.end(), id) == replay.end();
    });
  }
  // MonsterSystem records the boss's def id (a sub-dungeon boss too, which has no tree): the monster must still exist.
  dropped += SaveIoDropIf(s.miniBossDialogueSeen, [&](const std::string& id) { return data.FindMonster(id) != nullptr; });
  // Q1 keys "<npcId>|<nodeId>|<choiceIndex>": the NPC's tree still has that node and that choice.
  dropped += SaveIoDropIf(s.dialogueOnce, [&](const std::string& key) {
    const size_t a = key.find('|');
    const size_t b = a == std::string::npos ? a : key.find('|', a + 1);
    if (b == std::string::npos) return false;
    const DialogueTree* tree = SaveIoTreeOf(data, std::string_view(key).substr(0, a));
    const DialogueNode* node = tree != nullptr ? tree->FindNode(std::string_view(key).substr(a + 1, b - a - 1)) : nullptr;
    if (node == nullptr) return false;
    const std::string_view index = std::string_view(key).substr(b + 1);
    if (index.empty() || index.size() > 4 || index.find_first_not_of("0123456789") != std::string_view::npos) return false;
    size_t choice = 0;
    for (char c : index) choice = choice * 10 + static_cast<size_t>(c - '0');
    return choice < node->choices.size();
  });
  // dialogueState: NPCs whose tree is gone go; the others keep only nodes (and node -> next pairs) the tree still has.
  dropped += SaveIoDropIf(s.dialogueState, [&](const DialogueNpcState& st) { return SaveIoTreeOf(data, st.npcId) != nullptr; });
  for (DialogueNpcState& st : s.dialogueState) {
    const DialogueTree* tree = SaveIoTreeOf(data, st.npcId);
    dropped += SaveIoDropIf(st.visitedNodes, [&](const std::string& n) { return tree->FindNode(n) != nullptr; });
    dropped += SaveIoDropIf(st.choicesMade, [&](const std::pair<std::string, std::string>& c) {
      return tree->FindNode(c.first) != nullptr && tree->FindNode(c.second) != nullptr;
    });
  }
  dropped += SaveIoDropIf(s.loreCollected, [&](const std::string& id) { return data.Lore().Find(id) != nullptr; });
  dropped += SaveIoDropIf(s.discoveredHiddenAreas, [&](const std::string& id) { return SaveIoKnownHiddenArea(data, id); });
  // Q5 keys "<areaId>#<rewardIndex>".
  dropped += SaveIoDropIf(s.hiddenRewardsClaimed, [&](const std::string& key) {
    const size_t hash = key.rfind('#');
    return hash != std::string::npos && SaveIoKnownHiddenArea(data, std::string_view(key).substr(0, hash));
  });
  dropped += SaveIoDropIf(s.achievements, [&](const std::pair<std::string, int64_t>& a) {
    return data.Quests().FindAchievement(a.first) != nullptr;
  });
  s.contentVersion = kCurrentContentVersion;
  return dropped;
}

bool FindNearestWalkablePosition(double col, double row, const std::function<bool(int32_t, int32_t)>& walkable,
                                 int32_t cols, int32_t rows, std::span<const TilePos> camps, TilePos& out) {
  const int32_t rc = JsRoundInt(col);
  const int32_t rr = JsRoundInt(row);
  const bool inBounds = rc >= 0 && rr >= 0 && rc < cols && rr < rows;
  if (inBounds && walkable(rc, rr)) return false;
  if (camps.empty()) return false;
  size_t best = 0;
  double bestSq = 0;
  for (size_t i = 0; i < camps.size(); ++i) {
    const double dc = camps[i].col - col, dr = camps[i].row - row;
    const double dSq = dc * dc + dr * dr;
    if (i == 0 || dSq < bestSq) {
      best = i;
      bestSq = dSq;
    }
  }
  out = camps[best];
  return true;
}

SaveSlotInfo SummarizeSave(int32_t slot, const SaveData& s) {
  SaveSlotInfo info;
  info.slot = slot;
  info.exists = true;
  info.timestamp = s.timestamp;
  info.classId = s.classId;
  info.level = s.player.level;
  info.mapId = s.player.currentMap;
  info.playTimeMs = s.playTimeMs;
  info.difficulty = s.difficulty;
  info.completedDifficulties = DeriveCompletedDifficulties(s.difficulty, s.completedDifficulties);
  info.showDifficultySelector = ShouldShowDifficultySelector(info.difficulty, info.completedDifficulties);
  return info;
}

namespace savejson {

void WriteStringList(JsonWriter& w, const std::vector<std::string>& v) {
  w.StartArray();
  for (const std::string& s : v) w.String(s);
  w.EndArray();
}

void ReadStringList(const JsonValue& v, std::vector<std::string>& out) {
  out.clear();
  if (v.Type() != JsonType::Array) return;
  for (const JsonValue& e : v.Items()) {
    if (e.Type() == JsonType::String) out.emplace_back(e.AsString());
  }
}

}  // namespace savejson
}  // namespace abyss

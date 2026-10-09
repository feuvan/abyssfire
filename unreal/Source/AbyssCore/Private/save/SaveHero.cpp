// SaveData sections owned by the hero area: player, settings, soulEcho (save-ui-input.md 3.2 field order, 3.3 lenient
// reads). Key order = the web's autoSave object literals (ZoneScene.ts:4263-4283; SoulEchoData types.ts:601-607).
//
// Readers are lenient: a missing or wrong-typed field keeps its default. Fields whose absence has a load rule elsewhere
// are read as NaN when missing so GameSim can apply it: player.hp (missing / non-finite -> dead save, FIX Q35),
// player.mana (non-finite -> maxMana), player.tileCol / tileRow (non-finite -> camps[0]).
#include "abyss/base/Platform.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "abyss/base/Enums.h"
#include "abyss/base/Math.h"
#include "SaveSections.h"

namespace abyss::savejson {

namespace herosave_detail {

constexpr double kMissingNumber = std::numeric_limits<double>::quiet_NaN();

// A JSON number as double, or `def` when absent / not a number.
double HeroNum(const JsonValue& obj, std::string_view key, double def) {
  const JsonValue* v = obj.Find(key);
  return v != nullptr && v->IsNumber() ? v->AsDouble(def) : def;
}

// floor(number) saturated to int64; `def` for a missing, non-number or non-finite value.
int64_t HeroInt64(const JsonValue& obj, std::string_view key, int64_t def) {
  const double d = HeroNum(obj, key, kMissingNumber);
  if (!std::isfinite(d)) return def;
  const double f = std::floor(d);
  if (f <= -9223372036854775808.0) return (std::numeric_limits<int64_t>::min)();
  if (f >= 9223372036854775807.0) return (std::numeric_limits<int64_t>::max)();
  return static_cast<int64_t>(f);
}

int32_t HeroInt32(const JsonValue& obj, std::string_view key, int32_t def) {
  const double d = HeroNum(obj, key, kMissingNumber);
  return std::isfinite(d) ? SaturatingInt32(std::floor(d)) : def;
}

// A JSON integer-ish value (element of a legacy [id, level] pair).
bool HeroValueToInt32(const JsonValue& v, int32_t& out) {
  if (!v.IsNumber()) return false;
  const double d = v.AsDouble();
  if (!std::isfinite(d)) return false;
  out = SaturatingInt32(std::floor(d));
  return true;
}

void WritePrimaryStats(JsonWriter& w, const PrimaryStats& s) {
  w.StartObject();
  w.Key("str");
  w.Int(s.str);
  w.Key("dex");
  w.Int(s.dex);
  w.Key("vit");
  w.Int(s.vit);
  w.Key("int");
  w.Int(s.int_);
  w.Key("spi");
  w.Int(s.spi);
  w.Key("lck");
  w.Int(s.lck);
  w.EndObject();
}

void ReadPrimaryStats(const JsonValue& v, PrimaryStats& out) {
  if (!v.IsObject()) return;
  out.str = HeroInt32(v, "str", out.str);
  out.dex = HeroInt32(v, "dex", out.dex);
  out.vit = HeroInt32(v, "vit", out.vit);
  out.int_ = HeroInt32(v, "int", out.int_);
  out.spi = HeroInt32(v, "spi", out.spi);
  out.lck = HeroInt32(v, "lck", out.lck);
}

// skillLevels: a record {id: level} (current) or the legacy entry array [[id, level], ...] (ZoneScene.ts:4305).
void ReadSkillLevels(const JsonValue& v, std::vector<std::pair<std::string, int32_t>>& out) {
  out.clear();
  if (v.IsObject()) {
    for (const JsonMember& m : v.Members()) {
      int32_t level = 0;
      if (HeroValueToInt32(m.value, level)) out.emplace_back(m.key, level);
    }
  } else if (v.IsArray()) {
    for (const JsonValue& e : v.Items()) {
      int32_t level = 0;
      if (e.IsArray() && e.Size() >= 2 && e.At(0).IsString() && HeroValueToInt32(e.At(1), level)) {
        out.emplace_back(std::string(e.At(0).AsString()), level);
      }
    }
  }
}

}  // namespace herosave_detail

void WriteHero(JsonWriter& w, const SaveHero& h) {
  w.StartObject();
  w.Key("level");
  w.Int(h.level);
  w.Key("exp");
  w.Int(h.exp);
  w.Key("gold");
  w.Int(h.gold);
  w.Key("hp");
  w.Double(h.hp);
  w.Key("maxHp");
  w.Double(h.maxHp);
  w.Key("mana");
  w.Double(h.mana);
  w.Key("maxMana");
  w.Double(h.maxMana);
  w.Key("stats");
  herosave_detail::WritePrimaryStats(w, h.stats);
  w.Key("freeStatPoints");
  w.Int(h.freeStatPoints);
  w.Key("freeSkillPoints");
  w.Int(h.freeSkillPoints);
  w.Key("skillLevels");
  w.StartObject();
  for (size_t i = 0; i < h.skillLevels.size(); ++i) {
    bool repeated = false;  // a JSON object cannot hold the same key twice; the first entry wins
    for (size_t j = 0; j < i && !repeated; ++j) repeated = h.skillLevels[j].first == h.skillLevels[i].first;
    if (repeated) continue;
    w.Key(h.skillLevels[i].first);
    w.Int(h.skillLevels[i].second);
  }
  w.EndObject();
  if (h.hasSpirit) {
    w.Key("spirit");
    w.StartObject();
    w.Key("value");
    w.Double(h.spirit.value);
    w.Key("resonanceRemainingMs");
    w.Double(h.spirit.resonanceRemainingMs);
    w.EndObject();
  }
  w.Key("tileCol");
  w.Double(h.tileCol);
  w.Key("tileRow");
  w.Double(h.tileRow);
  w.Key("currentMap");
  w.String(h.currentMap);
  w.EndObject();
}

void ReadHero(const JsonValue& v, SaveHero& out) {
  using namespace herosave_detail;
  out = SaveHero{};
  out.hp = kMissingNumber;
  out.mana = kMissingNumber;
  out.tileCol = kMissingNumber;
  out.tileRow = kMissingNumber;
  if (!v.IsObject()) return;
  out.level = HeroInt32(v, "level", out.level);
  out.exp = HeroInt64(v, "exp", out.exp);
  out.gold = HeroInt64(v, "gold", out.gold);
  out.hp = HeroNum(v, "hp", kMissingNumber);
  out.mana = HeroNum(v, "mana", kMissingNumber);
  out.maxHp = HeroNum(v, "maxHp", 0.0);
  out.maxMana = HeroNum(v, "maxMana", 0.0);
  ReadPrimaryStats(v.Get("stats"), out.stats);
  out.freeStatPoints = HeroInt32(v, "freeStatPoints", out.freeStatPoints);
  out.freeSkillPoints = HeroInt32(v, "freeSkillPoints", out.freeSkillPoints);
  ReadSkillLevels(v.Get("skillLevels"), out.skillLevels);
  // spirit (v3): any object is restored through Spirit::Restore (non-finite -> 0, clamps); absent -> {0, 0}.
  const JsonValue& sp = v.Get("spirit");
  if (sp.IsObject()) {
    out.hasSpirit = true;
    out.spirit.value = HeroNum(sp, "value", kMissingNumber);
    out.spirit.resonanceRemainingMs = HeroNum(sp, "resonanceRemainingMs", kMissingNumber);
  }
  out.tileCol = HeroNum(v, "tileCol", kMissingNumber);
  out.tileRow = HeroNum(v, "tileRow", kMissingNumber);
  const JsonValue& map = v.Get("currentMap");
  if (map.IsString()) out.currentMap = std::string(map.AsString());
}

void WriteSettings(JsonWriter& w, const SaveSettings& s) {
  w.StartObject();
  w.Key("autoCombat");
  w.Bool(s.autoCombat);
  w.Key("musicVolume");
  w.Double(s.musicVolume);
  w.Key("sfxVolume");
  w.Double(s.sfxVolume);
  w.Key("autoLootMode");
  w.String(EnumName(s.autoLootMode));
  w.EndObject();
}

void ReadSettings(const JsonValue& v, SaveSettings& out) {
  using namespace herosave_detail;
  out = SaveSettings{};
  if (!v.IsObject()) return;
  const JsonValue& ac = v.Get("autoCombat");
  if (ac.IsBool()) out.autoCombat = ac.AsBool();
  out.musicVolume = HeroNum(v, "musicVolume", out.musicVolume);
  out.sfxVolume = HeroNum(v, "sfxVolume", out.sfxVolume);
  const JsonValue& mode = v.Get("autoLootMode");
  AutoLootMode parsed = AutoLootMode::Off;
  if (mode.IsString() && ParseEnum(mode.AsString(), parsed)) out.autoLootMode = parsed;
}

void WriteSoulEcho(JsonWriter& w, const SaveSoulEcho& e) {
  if (!e.present) {
    w.Null();
    return;
  }
  w.StartObject();
  w.Key("mapId");
  w.String(e.mapId);
  w.Key("col");
  w.Double(e.col);
  w.Key("row");
  w.Double(e.row);
  w.Key("gold");
  w.Int(e.gold);
  w.Key("exp");
  w.Int(e.exp);
  w.EndObject();
}

void ReadSoulEcho(const JsonValue& v, SaveSoulEcho& out) {
  using namespace herosave_detail;
  out = SaveSoulEcho{};
  if (!v.IsObject()) return;
  // SoulEchoState.load: kept only if col and row are finite numbers.
  const double col = HeroNum(v, "col", kMissingNumber);
  const double row = HeroNum(v, "row", kMissingNumber);
  if (!std::isfinite(col) || !std::isfinite(row)) return;
  out.present = true;
  out.col = col;
  out.row = row;
  const JsonValue& map = v.Get("mapId");
  if (map.IsString()) out.mapId = std::string(map.AsString());
  out.gold = (std::max)(int64_t{0}, HeroInt64(v, "gold", 0));
  out.exp = (std::max)(int64_t{0}, HeroInt64(v, "exp", 0));
}

}  // namespace abyss::savejson

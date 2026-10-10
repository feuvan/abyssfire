// SaveData sections owned by pets / homestead (save-ui-input.md 3.2-3.3; quests-story-ch1.md 18.2, 4.4-4.7).
// Readers are lenient: wrong-typed fields keep their defaults and the web's load guards that need no game data are
// applied here (num(v, d) = finite number -> max(0, v), else d; clampInt's "finite number -> floor"). The data-dependent
// rules (known pet ids / expedition options / blessings, the level / exp / bond clamps) run in PetSystem::ReadSave
// (MigratePetSave) and HomesteadTower::Load.
#include "abyss/base/Platform.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "abyss/save/SaveIO.h"
#include "SaveSections.h"

namespace abyss {

namespace {

// num(v, fallback) (HomesteadTower.ts:253-255): a finite number -> max(0, v), else the fallback.
double SpNum(const JsonValue* v, double fallback) {
  if (v == nullptr || v->Type() != JsonType::Number) return fallback;
  const double d = v->AsDouble();
  if (!std::isfinite(d)) return fallback;
  return (std::max)(0.0, d);
}

int32_t SpToInt32(double d) {
  constexpr double kLo = static_cast<double>((std::numeric_limits<int32_t>::min)());
  constexpr double kHi = static_cast<double>((std::numeric_limits<int32_t>::max)());
  return static_cast<int32_t>(std::clamp(std::floor(d), kLo, kHi));
}

// clampInt's number test (PetSystem.ts:427-430): a finite number floored (saturated to the field), else the default.
bool SpReadFloor(const JsonValue* v, double& out) {
  if (v == nullptr || v->Type() != JsonType::Number) return false;
  const double d = v->AsDouble();
  if (!std::isfinite(d)) return false;
  out = std::floor(d);
  return true;
}

int32_t SpIntField(const JsonValue& obj, std::string_view key, int32_t def) {
  double d = 0;
  return SpReadFloor(obj.Find(key), d) ? SpToInt32(d) : def;
}

std::string SpString(const JsonValue* v) {
  return v != nullptr && v->Type() == JsonType::String ? std::string(v->AsString()) : std::string();
}

}  // namespace

// PetSaveInstance {petId, level, exp, evolved, bond, bondProgress} (types.ts:543-559, field order of PetSystem.addPet).
void WritePetJson(JsonWriter& w, const PetInstance& p) {
  w.StartObject();
  w.Key("petId");
  w.String(p.petId);
  w.Key("level");
  w.Int(p.level);
  w.Key("exp");
  w.Int(p.exp);
  w.Key("evolved");
  w.Int(p.evolved);
  w.Key("bond");
  w.Int(p.bond);
  w.Key("bondProgress");
  w.Int(p.bondProgress);
  w.EndObject();
}

// False (entry dropped) when it is not an object or petId is not a string. Every numeric field: a finite number is
// floored, anything else takes clampInt's default (level 1, the others 0); the range clamps and the id checks are
// MigratePetSave's.
bool ReadPetJson(const JsonValue& v, PetInstance& out) {
  out = PetInstance{};
  if (v.Type() != JsonType::Object) return false;
  const JsonValue* id = v.Find("petId");
  if (id == nullptr || id->Type() != JsonType::String) return false;
  out.petId = std::string(id->AsString());
  out.level = SpIntField(v, "level", 1);
  double exp = 0;
  if (SpReadFloor(v.Find("exp"), exp)) {
    constexpr double kLim = 9007199254740992.0;  // 2^53
    out.exp = static_cast<int64_t>(std::clamp(exp, -kLim, kLim));
  }
  out.evolved = SpIntField(v, "evolved", 0);
  out.bond = SpIntField(v, "bond", 0);
  out.bondProgress = SpIntField(v, "bondProgress", 0);
  return true;
}

namespace savejson {

// pets: {owned: PetSaveInstance[], active: string | null} (PetSystem.toSave).
void WritePets(JsonWriter& w, const SavePets& p) {
  w.StartObject();
  w.Key("owned");
  w.StartArray();
  for (const PetInstance& pet : p.owned) WritePetJson(w, pet);
  w.EndArray();
  w.Key("active");
  if (p.active.empty()) {
    w.Null();
  } else {
    w.String(p.active);
  }
  w.EndObject();
}

// `pets` must be an object to count (migratePetSave: `save?.pets` truthy); `owned` must be an array (else the legacy
// list is used); a non-string active reads as null.
void ReadPets(const JsonValue& v, SavePets& out) {
  out = SavePets{};
  if (v.Type() != JsonType::Object) return;
  out.present = true;
  if (const JsonValue* owned = v.Find("owned"); owned != nullptr && owned->Type() == JsonType::Array) {
    out.hasOwned = true;
    for (const JsonValue& e : owned->Items()) {
      PetInstance p;
      if (ReadPetJson(e, p)) out.owned.push_back(std::move(p));
    }
  }
  out.active = SpString(v.Find("active"));
}

// homestead: {buildings: {id: level}, embers, garden: {progress, stock: {itemId: n}}, expedition | null,
// blessing | null, towerReturn | null} (ZoneScene.ts:4277-4280: buildings, then HomesteadTower.toSave()). The legacy
// pets / activePet are never written.
void WriteHomestead(JsonWriter& w, const SaveHomestead& h) {
  w.StartObject();
  w.Key("buildings");
  w.StartObject();
  for (const auto& [id, level] : h.buildings) {
    w.Key(id);
    w.Int(level);
  }
  w.EndObject();
  w.Key("embers");
  w.Int(h.embers);
  w.Key("garden");
  w.StartObject();
  w.Key("progress");
  w.Int(h.garden.progress);
  w.Key("stock");
  w.StartObject();
  for (const auto& [id, n] : h.garden.stock) {
    w.Key(id);
    w.Int(n);
  }
  w.EndObject();
  w.EndObject();
  w.Key("expedition");
  if (h.expedition.has_value()) {
    const ExpeditionState& e = *h.expedition;
    w.StartObject();
    w.Key("petId");
    w.String(e.petId);
    w.Key("optionId");
    w.String(e.optionId);
    w.Key("kills");
    w.Int(e.kills);
    w.Key("killsRequired");
    w.Int(e.killsRequired);
    w.Key("remainingMs");
    w.Double(e.remainingMs);
    w.EndObject();
  } else {
    w.Null();
  }
  w.Key("blessing");
  if (h.blessing.has_value()) {
    const BlessingState& b = *h.blessing;
    w.StartObject();
    w.Key("id");
    w.String(b.id);
    w.Key("level");
    w.Int(b.level);
    w.Key("remainingMs");
    w.Double(b.remainingMs);
    w.EndObject();
  } else {
    w.Null();
  }
  w.Key("towerReturn");
  if (h.towerReturn.has_value()) {
    const TowerReturn& r = *h.towerReturn;
    w.StartObject();
    w.Key("mapId");
    w.String(r.mapId);
    w.Key("col");
    w.Double(r.col);
    w.Key("row");
    w.Double(r.row);
    w.EndObject();
  } else {
    w.Null();
  }
  w.EndObject();
}

void ReadHomestead(const JsonValue& v, SaveHomestead& out) {
  out = SaveHomestead{};
  if (v.Type() != JsonType::Object) return;
  if (const JsonValue* b = v.Find("buildings"); b != nullptr && b->Type() == JsonType::Object) {
    for (const JsonMember& m : b->Members()) {
      double lv = 0;
      if (SpReadFloor(&m.value, lv)) out.buildings.emplace_back(m.key, SpToInt32(lv));
    }
  }
  out.embers = SpToInt32(SpNum(v.Find("embers"), 0));
  if (const JsonValue* g = v.Find("garden"); g != nullptr && g->Type() == JsonType::Object) {
    out.garden.progress = SpToInt32(SpNum(g->Find("progress"), 0));
    if (const JsonValue* s = g->Find("stock"); s != nullptr && s->Type() == JsonType::Object) {
      for (const JsonMember& m : s->Members()) {
        const int32_t n = SpToInt32(SpNum(&m.value, 0));
        if (n > 0) out.garden.stock.emplace_back(m.key, n);
      }
    }
  }
  if (const JsonValue* e = v.Find("expedition"); e != nullptr && e->Type() == JsonType::Object) {
    const JsonValue* petId = e->Find("petId");
    if (petId != nullptr && petId->Type() == JsonType::String) {
      ExpeditionState s;
      s.petId = std::string(petId->AsString());
      s.optionId = SpString(e->Find("optionId"));
      s.kills = SpToInt32(SpNum(e->Find("kills"), 0));
      s.killsRequired = (std::max)(1, SpToInt32(SpNum(e->Find("killsRequired"), 1)));
      s.remainingMs = SpNum(e->Find("remainingMs"), 0);
      out.expedition = std::move(s);
    }
  }
  if (const JsonValue* b = v.Find("blessing"); b != nullptr && b->Type() == JsonType::Object) {
    BlessingState s;
    s.id = SpString(b->Find("id"));
    s.level = (std::max)(1, SpToInt32(SpNum(b->Find("level"), 1)));
    s.remainingMs = SpNum(b->Find("remainingMs"), 0);
    out.blessing = std::move(s);
  }
  if (const JsonValue* r = v.Find("towerReturn"); r != nullptr && r->Type() == JsonType::Object) {
    const JsonValue* mapId = r->Find("mapId");
    if (mapId != nullptr && mapId->Type() == JsonType::String) {
      out.towerReturn = TowerReturn{std::string(mapId->AsString()), SpNum(r->Find("col"), 0), SpNum(r->Find("row"), 0)};
    }
  }
  // Legacy pet block (read only by MigratePetSave).
  if (const JsonValue* pets = v.Find("pets")) {
    out.hasLegacyPets = true;
    out.legacyPets = *pets;
  }
  if (const JsonValue* active = v.Find("activePet"); active != nullptr && active->Type() == JsonType::String) {
    out.hasLegacyActivePet = true;
    out.legacyActivePet = std::string(active->AsString());
  }
}

}  // namespace savejson
}  // namespace abyss

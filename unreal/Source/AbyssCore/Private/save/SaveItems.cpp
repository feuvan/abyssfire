// SaveData sections owned by items: ItemInstance (field names verbatim, I8), inventory, equipment, stash
// (save-ui-input.md 3.2; loot-items-inventory.md 2.4, 16). Readers are lenient: a missing / wrong-typed field keeps its
// default; only a non-object item is skipped.
#include "abyss/base/Platform.h"

#include "abyss/save/SaveIO.h"
#include "SaveSections.h"

namespace abyss {

namespace {

std::string SvItmStr(const JsonValue& o, std::string_view key) {
  const JsonValue& v = o.Get(key);
  return v.IsString() ? std::string(v.AsString()) : std::string();
}

double SvItmNum(const JsonValue& o, std::string_view key, double def) {
  const JsonValue& v = o.Get(key);
  return v.IsNumber() ? v.AsDouble(def) : def;
}

int32_t SvItmInt(const JsonValue& o, std::string_view key, int32_t def) {
  const JsonValue& v = o.Get(key);
  if (!v.IsNumber()) return def;
  if (v.IsInteger()) return v.AsInt(def);
  return def;
}

}  // namespace

// Web ItemInstance key order (createItem / generateSetPiece / crafting): uid, baseId, name, quality, level, affixes,
// sockets, [bonusSockets], [setId], [legendaryEffect], identified, quantity, stats; the port's legendaryId / setPieceId
// follow legendaryEffect when set.
void WriteItemJson(JsonWriter& w, const ItemInstance& item) {
  w.StartObject();
  w.Key("uid");
  w.String(item.uid);
  w.Key("baseId");
  w.String(item.baseId);
  w.Key("name");
  w.String(item.name);
  w.Key("quality");
  w.String(EnumName(item.quality));
  w.Key("level");
  w.Int(item.level);
  w.Key("affixes");
  w.StartArray();
  for (const ItemAffix& a : item.affixes) {
    w.StartObject();
    w.Key("affixId");
    w.String(a.affixId);
    w.Key("name");
    w.String(a.name);
    w.Key("stat");
    w.String(EnumName(a.stat));
    w.Key("value");
    w.Double(a.value);
    w.EndObject();
  }
  w.EndArray();
  w.Key("sockets");
  w.StartArray();
  for (const GemInstance& g : item.sockets) {
    w.StartObject();
    w.Key("gemId");
    w.String(g.gemId);
    w.Key("name");
    w.String(g.name);
    w.Key("stat");
    w.String(EnumName(g.stat));
    w.Key("value");
    w.Double(g.value);
    w.Key("tier");
    w.Int(g.tier);
    w.EndObject();
  }
  w.EndArray();
  if (item.bonusSockets > 0) {
    w.Key("bonusSockets");
    w.Int(item.bonusSockets);
  }
  if (!item.setId.empty()) {
    w.Key("setId");
    w.String(item.setId);
  }
  if (!item.legendaryEffect.empty()) {
    w.Key("legendaryEffect");
    w.String(item.legendaryEffect);
  }
  if (!item.legendaryId.empty()) {
    w.Key("legendaryId");
    w.String(item.legendaryId);
  }
  if (!item.setPieceId.empty()) {
    w.Key("setPieceId");
    w.String(item.setPieceId);
  }
  w.Key("identified");
  w.Bool(item.identified);
  w.Key("quantity");
  w.Int(item.quantity);
  w.Key("stats");
  w.StartObject();
  for (const StatValue& sv : item.stats.Items()) {
    w.Key(EnumName(sv.stat));
    w.Double(sv.value);
  }
  w.EndObject();
  w.EndObject();
}

bool ReadItemJson(const JsonValue& v, ItemInstance& out) {
  if (!v.IsObject()) return false;
  out = ItemInstance{};
  out.uid = SvItmStr(v, "uid");
  out.baseId = SvItmStr(v, "baseId");
  out.name = SvItmStr(v, "name");
  ItemQuality q = ItemQuality::Normal;
  if (ParseEnum(SvItmStr(v, "quality"), q)) out.quality = q;
  out.level = SvItmInt(v, "level", 1);
  for (const JsonValue& a : v.Get("affixes").Items()) {
    if (!a.IsObject()) continue;
    ItemAffix affix;
    if (!ParseEnum(SvItmStr(a, "stat"), affix.stat)) continue;  // unknown stat key: the affix cannot be applied
    affix.affixId = SvItmStr(a, "affixId");
    affix.name = SvItmStr(a, "name");
    affix.value = SvItmNum(a, "value", 0);
    out.affixes.push_back(std::move(affix));
  }
  for (const JsonValue& g : v.Get("sockets").Items()) {  // missing sockets -> [] (SaveSystem fix-up)
    if (!g.IsObject()) continue;
    GemInstance gem;
    if (!ParseEnum(SvItmStr(g, "stat"), gem.stat)) continue;
    gem.gemId = SvItmStr(g, "gemId");
    gem.name = SvItmStr(g, "name");
    gem.value = SvItmNum(g, "value", 0);
    gem.tier = SvItmInt(g, "tier", 1);
    out.sockets.push_back(std::move(gem));
  }
  out.bonusSockets = SvItmInt(v, "bonusSockets", 0);
  out.setId = SvItmStr(v, "setId");
  out.legendaryEffect = SvItmStr(v, "legendaryEffect");
  out.legendaryId = SvItmStr(v, "legendaryId");
  out.setPieceId = SvItmStr(v, "setPieceId");
  const JsonValue& ident = v.Get("identified");
  out.identified = ident.IsBool() ? ident.AsBool(true) : true;
  out.quantity = SvItmInt(v, "quantity", 1);
  const JsonValue& stats = v.Get("stats");
  if (stats.IsObject()) {
    for (const JsonMember& m : stats.Members()) {
      Stat s{};
      if (!ParseEnum(m.key, s) || !m.value.IsNumber()) continue;
      out.stats.Add(s, m.value.AsDouble());
    }
  }
  return true;
}

namespace savejson {

// equipment: Partial<Record<EquipSlot, ItemInstance>> in EquipSlot order.
void WriteEquipment(JsonWriter& w, const SaveData& s) {
  w.StartObject();
  for (size_t i = 0; i < s.equipment.size(); ++i) {
    if (!s.equipment[i].has_value()) continue;
    w.Key(EnumName(static_cast<EquipSlot>(i)));
    WriteItemJson(w, *s.equipment[i]);
  }
  w.EndObject();
}

void ReadEquipment(const JsonValue& v, SaveData& out) {
  for (auto& e : out.equipment) e.reset();
  if (!v.IsObject()) return;
  for (const JsonMember& m : v.Members()) {
    EquipSlot slot{};
    if (!ParseEnum(m.key, slot)) continue;
    ItemInstance item;
    if (ReadItemJson(m.value, item)) out.equipment[EnumIndex(slot)] = std::move(item);
  }
}

void WriteItemList(JsonWriter& w, const std::vector<ItemInstance>& items) {
  w.StartArray();
  for (const ItemInstance& it : items) WriteItemJson(w, it);
  w.EndArray();
}

void ReadItemList(const JsonValue& v, std::vector<ItemInstance>& out) {
  out.clear();
  if (v.Type() != JsonType::Array) return;
  for (const JsonValue& e : v.Items()) {
    ItemInstance item;
    if (ReadItemJson(e, item)) out.push_back(std::move(item));
  }
}

}  // namespace savejson
}  // namespace abyss

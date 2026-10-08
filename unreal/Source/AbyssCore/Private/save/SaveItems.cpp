// SaveData sections owned by items: ItemInstance (field names verbatim, I8), inventory, equipment, stash
// (save-ui-input.md 3.2; loot-items-inventory.md 2.4, 16). STUB.
#include "abyss/base/Platform.h"

#include "abyss/base/Assert.h"
#include "abyss/save/SaveIO.h"
#include "SaveSections.h"

namespace abyss {

void WriteItemJson(JsonWriter& w, const ItemInstance& item) {
  ABYSS_UNIMPLEMENTED();
  w.StartObject();
  w.EndObject();
}

bool ReadItemJson(const JsonValue& v, ItemInstance& out) {
  ABYSS_UNIMPLEMENTED();
  return false;
}

namespace savejson {

void WriteEquipment(JsonWriter& w, const SaveData& s) {
  ABYSS_UNIMPLEMENTED();
  w.StartObject();
  w.EndObject();
}

void ReadEquipment(const JsonValue& v, SaveData& out) { ABYSS_UNIMPLEMENTED(); }

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

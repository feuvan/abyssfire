// Item instance helpers (loot-items-inventory.md 2.4, 4.5, 9.1, 12.4, 15.1; I9). Partly implemented (stat cache, uid
// generator); display name / sell price / socket capacity are stubs. Owner area: items.
#include "abyss/base/Platform.h"

#include "abyss/items/Item.h"

#include "abyss/base/Assert.h"
#include "abyss/base/StrUtil.h"
#include "abyss/data/DataStore.h"

namespace abyss {

void ComputeItemStats(ItemInstance& item) {
  item.stats = StatBag();
  for (const ItemAffix& a : item.affixes) item.stats.Add(a.stat, a.value);
  for (const GemInstance& g : item.sockets) item.stats.Add(g.stat, g.value);
}

int32_t ItemSocketCapacity(const ItemInstance& item, const DataStore& data) {
  ABYSS_UNIMPLEMENTED();
  return 0;
}

std::string ItemDisplayName(const ItemInstance& item, const DataStore& data, const I18n& i18n) {
  ABYSS_UNIMPLEMENTED();
  return item.name;
}

int64_t ItemSellPrice(const ItemInstance& item, const DataStore& data) {
  ABYSS_UNIMPLEMENTED();
  return 1;
}

std::string ItemUidGenerator::Next() {
  std::string uid = "i";
  uid += ToHex(next_++);
  return uid;
}

}  // namespace abyss

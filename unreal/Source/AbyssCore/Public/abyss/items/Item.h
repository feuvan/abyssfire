// Item instances (loot-items-inventory.md 2.4, 4.5) - also the save format (field names verbatim, DECISIONS I8).
//
// Owner area: items. ItemInstance is a plain value type shared by inventory, equipment, stash, ground loot, shops,
// crafting, quest rewards and saves.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/I18n.h"
#include "abyss/base/Platform.h"
#include "abyss/base/Stats.h"

namespace abyss {

class DataStore;
class I18n;
struct ItemBaseDef;

// {affixId, name, stat, value} - random, set and legendary affixes alike (loot 2.4).
struct ItemAffix {
  std::string affixId;
  std::string name;  // zh-CN name baked at creation (save compatibility only; display re-localises, 15)
  Stat stat = Stat::Damage;
  double value = 0;
  bool operator==(const ItemAffix& o) const = default;
};

// A gem in a socket (loot 9.2).
struct GemInstance {
  std::string gemId;  // gem item base id
  std::string name;
  Stat stat = Stat::Str;
  double value = 0;
  int32_t tier = 1;
  bool operator==(const GemInstance& o) const = default;
};

struct ItemInstance {
  std::string uid;     // unique per save ("i" + hex counter, ItemUidGenerator)
  std::string baseId;
  std::string name;    // stored zh name (web compatibility); display uses ItemDisplayName
  ItemQuality quality = ItemQuality::Normal;
  int32_t level = 1;   // item level: affix tiers, legendary scaling, craft cost, salvage yield
  std::vector<ItemAffix> affixes;   // ordered (naming)
  std::vector<GemInstance> sockets; // filled sockets, insertion order
  int32_t bonusSockets = 0;         // punched by the blacksmith (0 or 1)
  std::string setId;                // set membership (set pieces)
  // Effect description baked at creation in zh (web parity, save readability only). Never display it directly: UE shows
  // ItemLegendaryEffectText, which localises it - data.legendary.<legendaryId>.effect for a named legendary and
  // sys.loot.genericLegendaryEffect for a GENERIC legendary (quality legendary + empty legendaryId: a base without a
  // legendary definition, makeLegendary's else branch, loot 4.3 / 15.3 / Q5).
  std::string legendaryEffect;
  std::string legendaryId;          // port addition (loot 2.4): named legendary id, empty otherwise (incl. generic)
  std::string setPieceId;           // port addition: set piece id, empty otherwise
  bool identified = true;           // always true in milestone 1 (I2)
  int32_t quantity = 1;
  StatBag stats;                    // cache = sum of affix values + gem values per stat (ComputeItemStats)

  bool operator==(const ItemInstance& o) const = default;
};

// Recomputes `stats` from affixes + socketed gems (computeStats / recomputeItemStats).
ABYSS_API void ComputeItemStats(ItemInstance& item);

// "Is equipment" everywhere = the base has a slot (loot 2.2). Unknown bases are not equipment.
ABYSS_API bool IsEquipmentItem(const ItemInstance& item, const DataStore& data);

// Socket capacity = base sockets (weapons/armour) + bonusSockets (loot 9.1). Unknown base: 0 + bonusSockets.
ABYSS_API int32_t ItemSocketCapacity(const ItemInstance& item, const DataStore& data);

// Localised display name (getItemDisplayName with the port's FIX Q4, loot 15.1), in `i18n`'s current locale:
//   unknown base -> stored name; named legendary -> data.legendary.<id>.name; set piece -> data.set.<setId>.name + ' ' +
//   base name; normal / no affixes -> base name; else prefixes + base + suffixes (en: words joined by spaces; zh: prefix
//   names concatenated, suffixes after a U+00B7 middle dot). Every key falls back to the data's zh / en names.
ABYSS_API std::string ItemDisplayName(const ItemInstance& item, const DataStore& data, const I18n& i18n);

// The localised legendary effect line of the tooltip (loot 15.2 / 15.3, Q5 "store ids; UE localises"), in `i18n`'s
// current locale: named legendary -> data.legendary.<legendaryId>.effect (fallback: the definition's zh text); generic
// legendary (quality legendary, empty legendaryId, non-empty legendaryEffect) -> sys.loot.genericLegendaryEffect;
// anything else -> the stored legendaryEffect ("" for items without one).
ABYSS_API std::string ItemLegendaryEffectText(const ItemInstance& item, const DataStore& data, const I18n& i18n);

// C11: the legendary specialEffect of a named legendary as an item stat, when its key is one combat reads
// (item_bases appliedSpecialEffects: killHealPercent, elementalDamagePercent, doubleShot, ignoreDefense, dodgeCounter,
// damageReduction, cooldownReduction). False for every other item. Gear stats (Inventory::EquipmentStatBag) and the
// tooltip compare totals (ItemStatTotals, loot 14) both add it.
ABYSS_API bool ItemSpecialEffectStat(const ItemInstance& item, const DataStore& data, Stat& outStat, double& outValue);

// Display-name collation for bag / stash sorting (loot 7.3 port rule: the current culture's collation). The core has no
// collation tables: the UE layer installs one backed by ICU for the active culture (e.g. FText::CompareTo), so zh-CN
// sorts like the web's localeCompare (pinyin). Without one, names compare by UTF-8 byte order (= code point order).
// Returns < 0, 0, > 0. Process-wide like the log sink; SetItemNameCollator returns the previous binding.
using ItemNameCollator = int (*)(std::string_view a, std::string_view b, void* user);
struct ItemNameCollatorBinding {
  ItemNameCollator fn = nullptr;
  void* user = nullptr;
};
ABYSS_API ItemNameCollatorBinding SetItemNameCollator(ItemNameCollator fn, void* user);
ABYSS_API int CompareItemNames(std::string_view a, std::string_view b);

// A log argument naming an item (loot 15.3: core logs carry ids where they can). Normal-quality items are a KeyArg on
// data.item.<baseId>.name; composite names (affixes, sets, legendaries) cannot be one key, so they are rendered with
// the data's I18n in its current locale, prefixed with sys.inventory.qualityPrefix.<quality> when `qualityPrefix`.
ABYSS_API I18nArg ItemNameArg(std::string argName, const ItemInstance& item, const DataStore& data,
                              bool qualityPrefix = false);

// Per-unit sell price shown in the tooltip: floor(base.sellPrice * sellQualityMultiplier[quality]) (I9); unknown base 1.
ABYSS_API int64_t ItemUnitSellPrice(const ItemInstance& item, const DataStore& data);
// Sell price = floor(base.sellPrice * quantity * sellQualityMultiplier[quality]) (12.4 + I9); unknown base -> 1.
ABYSS_API int64_t ItemSellPrice(const ItemInstance& item, const DataStore& data);

// Deterministic per-save uid source (loot 1 "Item uid"): "i" + lower-case hex of a counter saved in v4 saves.
class ABYSS_API ItemUidGenerator {
 public:
  std::string Next();
  uint64_t Counter() const { return next_; }
  void SetCounter(uint64_t next) { next_ = next; }

 private:
  uint64_t next_ = 1;
};

// The counter value a uid of the generator's form ("i" + hex) was made from, or 0 when `uid` has another form.
ABYSS_API uint64_t ItemUidCounterValue(std::string_view uid);

}  // namespace abyss

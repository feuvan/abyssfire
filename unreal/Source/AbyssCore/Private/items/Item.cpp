// Item instance helpers (loot-items-inventory.md 2.4, 4.5, 9.1, 12.4, 15.1; I9, FIX Q4). Owner area: items.
#include "abyss/base/Platform.h"

#include "abyss/items/Item.h"

#include <algorithm>
#include <cmath>

#include "abyss/base/Math.h"
#include "abyss/base/StrUtil.h"
#include "abyss/data/DataStore.h"

namespace abyss {

void ComputeItemStats(ItemInstance& item) {
  item.stats = StatBag();
  for (const ItemAffix& a : item.affixes) item.stats.Add(a.stat, a.value);
  for (const GemInstance& g : item.sockets) item.stats.Add(g.stat, g.value);
}

bool IsEquipmentItem(const ItemInstance& item, const DataStore& data) {
  const ItemBaseDef* base = data.Items().FindBase(item.baseId);
  return base != nullptr && base->IsEquipment();
}

int32_t ItemSocketCapacity(const ItemInstance& item, const DataStore& data) {
  // baseSocketCount: the base's `sockets` for weapon / armour bases; 0 for accessories, consumables, unknown bases.
  const ItemBaseDef* base = data.Items().FindBase(item.baseId);
  int32_t n = 0;
  if (base != nullptr && (base->type == ItemType::Weapon || base->type == ItemType::Armor)) n = base->sockets;
  return n + (std::max)(0, item.bonusSockets);
}

namespace {

bool ItmIsEnglish(const I18n& i18n) { return i18n.Current() == LocaleId::En; }

std::string ItmLocalName(const I18n& i18n, const std::string& key, const std::string& zh, const std::string& en) {
  return i18n.NameOr(key, ItmIsEnglish(i18n) && !en.empty() ? en : zh);
}

}  // namespace

std::string ItemDisplayName(const ItemInstance& item, const DataStore& data, const I18n& i18n) {
  const ItemTables& t = data.Items();
  const ItemBaseDef* base = t.FindBase(item.baseId);
  if (base == nullptr) return item.name;
  const bool en = ItmIsEnglish(i18n);
  const std::string baseName = ItmLocalName(i18n, "data.item." + base->id + ".name", base->name, base->nameEn);

  // FIX Q4: legendary and set items show their real names.
  if (item.quality == ItemQuality::Legendary && !item.legendaryId.empty()) {
    if (const LegendaryDef* l = t.FindLegendary(item.legendaryId)) {
      return ItmLocalName(i18n, "data.legendary." + l->id + ".name", l->name, l->nameEn);
    }
  }
  if (item.quality == ItemQuality::Set && !item.setId.empty()) {
    if (const SetDef* s = t.FindSet(item.setId)) {
      return ItmLocalName(i18n, "data.set." + s->id + ".name", s->name, s->nameEn) + " " + baseName;
    }
  }
  if (item.quality == ItemQuality::Normal || item.affixes.empty()) return baseName;

  // Suffix iff the affix table says so; everything else (incl. unknown / fixed affixes) reads as a prefix.
  std::vector<std::string> prefixes;
  std::vector<std::string> suffixes;
  for (const ItemAffix& a : item.affixes) {
    const AffixDef* def = t.FindAffix(a.affixId);
    std::string name = def != nullptr ? ItmLocalName(i18n, "data.affix." + def->id, def->name, def->nameEn) : a.name;
    if (def != nullptr && def->kind == AffixKind::Suffix) {
      suffixes.push_back(std::move(name));
    } else {
      prefixes.push_back(std::move(name));
    }
  }
  std::string out;
  if (en) {
    for (const std::string& p : prefixes) {
      out += p;
      out += ' ';
    }
    out += baseName;
    for (const std::string& s : suffixes) {
      out += ' ';
      out += s;
    }
  } else {
    for (const std::string& p : prefixes) out += p;
    out += baseName;
    if (!suffixes.empty()) {
      for (size_t i = 0; i < suffixes.size(); ++i) {
        out += "\xC2\xB7";  // U+00B7 middle dot
        out += suffixes[i];
      }
    }
  }
  return out;
}

I18nArg ItemNameArg(std::string argName, const ItemInstance& item, const DataStore& data, bool qualityPrefix) {
  const ItemBaseDef* base = data.Items().FindBase(item.baseId);
  if (base != nullptr && item.quality == ItemQuality::Normal) {
    return KeyArg(std::move(argName), "data.item." + base->id + ".name");
  }
  const I18n& i18n = data.Strings();
  std::string text;
  if (qualityPrefix && item.quality != ItemQuality::Normal) {
    text = i18n.T("sys.inventory.qualityPrefix." + std::string(EnumName(item.quality)));
  }
  text += ItemDisplayName(item, data, i18n);
  return I18nArg{std::move(argName), std::move(text), false};
}

int64_t ItemUnitSellPrice(const ItemInstance& item, const DataStore& data) {
  ItemInstance one = item;
  one.quantity = 1;
  return ItemSellPrice(one, data);
}

int64_t ItemSellPrice(const ItemInstance& item, const DataStore& data) {
  const ItemTables& t = data.Items();
  const ItemBaseDef* base = t.FindBase(item.baseId);
  if (base == nullptr) return 1;
  const double mult = t.economy.sellQualityMultiplier[EnumIndex(item.quality)];
  const double price = static_cast<double>(base->sellPrice) * static_cast<double>(item.quantity) * mult;
  if (!(price > 0)) return 0;
  return static_cast<int64_t>(std::floor((std::min)(price, 9.0e15)));
}

std::string ItemUidGenerator::Next() {
  std::string uid = "i";
  uid += ToHex(next_++);
  return uid;
}

uint64_t ItemUidCounterValue(std::string_view uid) {
  if (uid.size() < 2 || uid.size() > 17 || uid[0] != 'i') return 0;
  uint64_t v = 0;
  for (size_t i = 1; i < uid.size(); ++i) {
    const char c = uid[i];
    uint64_t d = 0;
    if (c >= '0' && c <= '9') {
      d = static_cast<uint64_t>(c - '0');
    } else if (c >= 'a' && c <= 'f') {
      d = static_cast<uint64_t>(c - 'a' + 10);
    } else {
      return 0;
    }
    v = v * 16 + d;
  }
  return v;
}

}  // namespace abyss

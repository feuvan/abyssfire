#include "abyss/base/Platform.h"

#include "abyss/base/Stats.h"

namespace abyss {

int32_t PrimaryStats::Get(PrimaryStat p) const {
  switch (p) {
    case PrimaryStat::Str: return str;
    case PrimaryStat::Dex: return dex;
    case PrimaryStat::Vit: return vit;
    case PrimaryStat::Int: return int_;
    case PrimaryStat::Spi: return spi;
    case PrimaryStat::Lck: return lck;
  }
  return 0;
}

int32_t& PrimaryStats::Ref(PrimaryStat p) {
  switch (p) {
    case PrimaryStat::Str: return str;
    case PrimaryStat::Dex: return dex;
    case PrimaryStat::Vit: return vit;
    case PrimaryStat::Int: return int_;
    case PrimaryStat::Spi: return spi;
    case PrimaryStat::Lck: return lck;
  }
  return str;
}

void StatBag::Add(Stat s, double value) {
  for (StatValue& e : items_) {
    if (e.stat == s) {
      e.value += value;
      return;
    }
  }
  items_.push_back({s, value});
}

void StatBag::Set(Stat s, double value) {
  for (StatValue& e : items_) {
    if (e.stat == s) {
      e.value = value;
      return;
    }
  }
  items_.push_back({s, value});
}

double StatBag::Get(Stat s) const {
  for (const StatValue& e : items_) {
    if (e.stat == s) return e.value;
  }
  return 0.0;
}

bool StatBag::Has(Stat s) const {
  for (const StatValue& e : items_) {
    if (e.stat == s) return true;
  }
  return false;
}

void StatBag::Remove(Stat s) {
  for (size_t i = 0; i < items_.size(); ++i) {
    if (items_[i].stat == s) {
      items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(i));
      return;
    }
  }
}

void StatBag::AddAll(const StatBag& o) {
  for (const StatValue& e : o.items_) Add(e.stat, e.value);
}

void StatBag::AddTo(EquipStats& out) const {
  for (const StatValue& e : items_) out.Add(e.stat, e.value);
}

}  // namespace abyss

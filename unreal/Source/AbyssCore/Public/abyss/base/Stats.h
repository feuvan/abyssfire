// The stat vocabulary (loot spec 2.6, combat-feel.md 1.1, classes-stats-skills.md 1.8).
//
// Stat covers every key that appears in a stat bag anywhere in the data: the 45 EquipStats fields first (in the web's
// EquipStats declaration order, so EquipStats is a dense array over them), then item-only keys (allStats, weapon damage
// produced by aggregation) and homestead bonus keys. EnumName/ParseEnum give the JSON key ("damagePercent", ...).
// Percent-type stats hold whole percents (attackSpeed 10 = 10 %).
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/Platform.h"

namespace abyss {

enum class Stat : uint8_t {
  // ---- EquipStats fields (combat-relevant), web order ----
  Damage,
  DamagePercent,
  Defense,
  DefensePercent,
  MaxHp,
  MaxHpPercent,
  MaxMana,
  MaxManaPercent,
  CritRate,
  CritDamage,
  AttackSpeed,
  LifeSteal,
  ManaSteal,
  HpRegen,
  ManaRegen,
  FireDamage,
  IceDamage,
  LightningDamage,
  PoisonDamage,
  FireResist,
  IceResist,
  LightningResist,
  PoisonResist,
  AllResist,
  MoveSpeed,
  MagicFind,
  ExpBonus,
  CooldownReduction,
  Knockback,
  Str,
  Dex,
  Int,
  Vit,
  Spi,
  Lck,
  KillHealPercent,
  DeathSave,
  CritDoubleStrike,
  DoubleShot,
  FreeCast,
  ElementalDamagePercent,
  IgnoreDefense,
  DamageReduction,
  ThornsHeal,
  DodgeCounter,
  // ---- item-only keys (never reach combat directly) ----
  AllStats,         // diamond gems; expanded to str/dex/int/vit/spi/lck at equipment aggregation (loot 8.2)
  WeaponDamageMin,  // produced by aggregation, dropped before combat (C10: inert in milestone 1)
  WeaponDamageMax,
  // ---- homestead bonus keys (homestead.json buildings) ----
  PotionDiscount,
  StashSlots,
  AltarBonus,
  GemBonus,
  MercExpBonus,
  PetExpBonus,
};

ABYSS_ENUM_STRINGS(Stat, "damage", "damagePercent", "defense", "defensePercent", "maxHp", "maxHpPercent", "maxMana",
                   "maxManaPercent", "critRate", "critDamage", "attackSpeed", "lifeSteal", "manaSteal", "hpRegen",
                   "manaRegen", "fireDamage", "iceDamage", "lightningDamage", "poisonDamage", "fireResist",
                   "iceResist", "lightningResist", "poisonResist", "allResist", "moveSpeed", "magicFind", "expBonus",
                   "cooldownReduction", "knockback", "str", "dex", "int", "vit", "spi", "lck", "killHealPercent",
                   "deathSave", "critDoubleStrike", "doubleShot", "freeCast", "elementalDamagePercent",
                   "ignoreDefense", "damageReduction", "thornsHeal", "dodgeCounter", "allStats", "weaponDamageMin",
                   "weaponDamageMax", "potionDiscount", "stashSlots", "altarBonus", "gemBonus", "mercExpBonus",
                   "petExpBonus")

inline constexpr size_t kEquipStatCount = static_cast<size_t>(Stat::DodgeCounter) + 1;  // 45
inline constexpr size_t kStatCount = EnumCount<Stat>();

constexpr bool IsEquipStat(Stat s) { return static_cast<size_t>(s) < kEquipStatCount; }

// The six primary attribute stats in data order (str, dex, vit, int, spi, lck are the JSON keys of `Stats`).
enum class PrimaryStat : uint8_t { Str, Dex, Vit, Int, Spi, Lck };
ABYSS_ENUM_STRINGS(PrimaryStat, "str", "dex", "vit", "int", "spi", "lck")

constexpr Stat ToStat(PrimaryStat p) {
  switch (p) {
    case PrimaryStat::Str: return Stat::Str;
    case PrimaryStat::Dex: return Stat::Dex;
    case PrimaryStat::Vit: return Stat::Vit;
    case PrimaryStat::Int: return Stat::Int;
    case PrimaryStat::Spi: return Stat::Spi;
    case PrimaryStat::Lck: return Stat::Lck;
  }
  return Stat::Str;
}

// Primary attributes (class base stats + allocated points; integers in practice, classes spec 1.1).
struct ABYSS_API PrimaryStats {
  int32_t str = 0;
  int32_t dex = 0;
  int32_t vit = 0;
  int32_t int_ = 0;
  int32_t spi = 0;
  int32_t lck = 0;

  int32_t Get(PrimaryStat p) const;
  int32_t& Ref(PrimaryStat p);
  bool operator==(const PrimaryStats& o) const = default;
};

// The merged bonus bag consumed by hero derived stats and combat (combat-feel.md 1.1): gear + gems + sets +
// achievements + pet passive + blessing + boons. Zero by default.
struct EquipStats {
  std::array<double, kEquipStatCount> v{};

  double Get(Stat s) const { return IsEquipStat(s) ? v[static_cast<size_t>(s)] : 0.0; }
  double& Ref(Stat s) { return v[static_cast<size_t>(s)]; }  // precondition: IsEquipStat(s)
  // Adds value when s is an EquipStats field; other keys are ignored (the web's getTypedEquipStats filter).
  void Add(Stat s, double value) {
    if (IsEquipStat(s)) v[static_cast<size_t>(s)] += value;
  }
  void AddAll(const EquipStats& o) {
    for (size_t i = 0; i < kEquipStatCount; ++i) v[i] += o.v[i];
  }
  bool operator==(const EquipStats& o) const = default;
};

struct StatValue {
  Stat stat = Stat::Damage;
  double value = 0.0;
  bool operator==(const StatValue& o) const = default;
};

// Insertion-ordered stat -> value bag (item stats, set bonuses, pet/homestead bonuses). Order is kept so saved JSON
// objects keep the web's key order.
class ABYSS_API StatBag {
 public:
  StatBag() = default;
  // Adds to the existing entry or appends a new one.
  void Add(Stat s, double value);
  void Set(Stat s, double value);
  double Get(Stat s) const;  // 0 when absent
  bool Has(Stat s) const;
  void Remove(Stat s);
  void Clear() { items_.clear(); }
  bool Empty() const { return items_.empty(); }
  size_t Size() const { return items_.size(); }
  std::span<const StatValue> Items() const { return items_; }
  void AddAll(const StatBag& o);
  // Copies every EquipStats key into `out` (adds).
  void AddTo(EquipStats& out) const;
  bool operator==(const StatBag& o) const = default;

 private:
  std::vector<StatValue> items_;
};

}  // namespace abyss

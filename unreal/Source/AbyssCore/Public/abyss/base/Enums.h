// Game-wide enums that appear in data tables, saves, commands and events, with their exact JSON/save spellings.
//
// EnumName(e) returns the data/save string; ParseEnum(s, out) parses it (false for unknown strings). EnumCount<E>()
// is the number of values. String tables are the single source of truth for the spellings (they are the web ids).
#pragma once

#include <array>
#include <cstdint>
#include <string_view>

#include "abyss/base/Platform.h"

namespace abyss {

enum class ClassId : uint8_t { Warrior, Mage, Rogue };
enum class Difficulty : uint8_t { Normal, Nightmare, Hell };
enum class DamageType : uint8_t { Physical, Fire, Ice, Lightning, Poison, Arcane };
// Quality order (loot floors): normal < magic < rare < legendary < set (loot spec 2.1).
enum class ItemQuality : uint8_t { Normal, Magic, Rare, Legendary, Set };
enum class ItemType : uint8_t { Weapon, Armor, Accessory, Consumable, Gem, Material, Scroll };
// The 10 equipment slots, in the web's declaration order (loot spec 2.1).
enum class EquipSlot : uint8_t { Helmet, Armor, Gloves, Boots, Weapon, Offhand, Necklace, Ring1, Ring2, Belt };
enum class WeaponType : uint8_t { Sword, Axe, Mace, Dagger, Bow, Staff, Wand, Shield };
enum class StatusType : uint8_t { Burn, Freeze, Poison, Bleed, Slow, Stun };
// Hit feedback weight (combat-feel.md 11.0).
enum class HitWeight : uint8_t { Tick, Light, Normal, Heavy, Crit, Kill };
// Animation rig presets (anim_timing.json "presets"; monster animCategory + the 3 hero rigs).
enum class AnimRig : uint8_t { Humanoid, Slime, Beast, Large, Flying, Serpentine, Demonic, Warrior, Mage, Rogue };
enum class MapTheme : uint8_t { Plains, Forest, Mountain, Desert, Abyss };
enum class AutoLootMode : uint8_t { Off, All, Magic, Rare, Legendary };
enum class LocaleId : uint8_t { ZhCN, ZhTW, En };
// Hero skill trees are data (skill_trees.json); this is the hero/monster/pet "side" of a hit.
enum class Faction : uint8_t { Hero, Monster, Neutral };

template <class E>
struct EnumStrings;

// Declares the string table of an enum (usable by area headers for their own enums, e.g. MonsterState).
#define ABYSS_ENUM_STRINGS(E, ...)                                                    \
  template <>                                                                         \
  struct EnumStrings<E> {                                                             \
    static constexpr auto kNames = std::to_array<std::string_view>({__VA_ARGS__});    \
  };

ABYSS_ENUM_STRINGS(ClassId, "warrior", "mage", "rogue")
ABYSS_ENUM_STRINGS(Difficulty, "normal", "nightmare", "hell")
ABYSS_ENUM_STRINGS(DamageType, "physical", "fire", "ice", "lightning", "poison", "arcane")
ABYSS_ENUM_STRINGS(ItemQuality, "normal", "magic", "rare", "legendary", "set")
ABYSS_ENUM_STRINGS(ItemType, "weapon", "armor", "accessory", "consumable", "gem", "material", "scroll")
ABYSS_ENUM_STRINGS(EquipSlot, "helmet", "armor", "gloves", "boots", "weapon", "offhand", "necklace", "ring1", "ring2",
                   "belt")
ABYSS_ENUM_STRINGS(WeaponType, "sword", "axe", "mace", "dagger", "bow", "staff", "wand", "shield")
ABYSS_ENUM_STRINGS(StatusType, "burn", "freeze", "poison", "bleed", "slow", "stun")
ABYSS_ENUM_STRINGS(HitWeight, "tick", "light", "normal", "heavy", "crit", "kill")
ABYSS_ENUM_STRINGS(AnimRig, "humanoid", "slime", "beast", "large", "flying", "serpentine", "demonic", "warrior", "mage",
                   "rogue")
ABYSS_ENUM_STRINGS(MapTheme, "plains", "forest", "mountain", "desert", "abyss")
ABYSS_ENUM_STRINGS(AutoLootMode, "off", "all", "magic", "rare", "legendary")
ABYSS_ENUM_STRINGS(LocaleId, "zh-CN", "zh-TW", "en")
ABYSS_ENUM_STRINGS(Faction, "hero", "monster", "neutral")

template <class E>
constexpr size_t EnumCount() {
  return EnumStrings<E>::kNames.size();
}

template <class E>
constexpr std::string_view EnumName(E e) {
  const auto i = static_cast<size_t>(e);
  return i < EnumStrings<E>::kNames.size() ? EnumStrings<E>::kNames[i] : std::string_view{};
}

template <class E>
constexpr bool ParseEnum(std::string_view s, E& out) {
  for (size_t i = 0; i < EnumStrings<E>::kNames.size(); ++i) {
    if (EnumStrings<E>::kNames[i] == s) {
      out = static_cast<E>(i);
      return true;
    }
  }
  return false;
}

template <class E>
constexpr size_t EnumIndex(E e) {
  return static_cast<size_t>(e);
}

// Quality helpers (loot spec 2.1).
constexpr bool QualityMeetsFloor(ItemQuality q, ItemQuality floor) { return EnumIndex(q) >= EnumIndex(floor); }

}  // namespace abyss

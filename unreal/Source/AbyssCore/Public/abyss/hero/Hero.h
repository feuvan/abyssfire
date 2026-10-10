// The hero: primary stats, derived stats, resources, leveling, life state, position, and its components
// (skills, buffs, spirit). Spec: classes-stats-skills.md 1.7, 3-6, 18; combat-feel.md 13.3; save-ui-input.md
// 5.1.1 (HeroLife); DECISIONS C1 (gear-inclusive maxima on level-up heal and load), C2, C12.
//
// Owner area: hero+combat. Session-level object (survives zone changes; cooldowns reset on zone change, Q23).
// Movement (path following, direct input) is the world area's HeroLocomotion, which writes Position()/Facing().
#pragma once

#include <cstdint>
#include <string>

#include "abyss/base/Enums.h"
#include "abyss/base/Platform.h"
#include "abyss/base/Stats.h"
#include "abyss/base/Types.h"
#include "abyss/combat/Damage.h"
#include "abyss/data/ClassData.h"
#include "abyss/hero/Buffs.h"
#include "abyss/hero/Skills.h"
#include "abyss/hero/Spirit.h"

namespace abyss {

class DataStore;
struct SaveHero;

enum class HeroLife : uint8_t { Alive, Dying };

// Derived stats (classes 3), recomputed every step with the merged EquipStats.
struct HeroDerived {
  double maxHp = 0;
  double maxMana = 0;
  double baseDamage = 0;   // fractional, never rounded
  double defense = 0;      // fractional
  double moveSpeed = 120;  // web units (iso px/s); ground speed = moveSpeed / 36 tiles/s (S5)
  double attackSpeedMs = 1000;
  double attackRange = 1.5;
};

struct LevelUpResult {
  bool leveledUp = false;
  int32_t level = 1;
};

// Regen modifiers for one step (classes 4.1): campfire x50 within 5 tiles of a camp, poison halves HP regen.
struct RegenModifiers {
  double hpMul = 1.0;
  double mpMul = 1.0;
};

class ABYSS_API Hero {
 public:
  // An unknown class (only possible with an unfinalized DataStore, which GameSim::Create refuses) reports an assert and
  // falls back to an empty class: every accessor stays safe.
  Hero(const DataStore& data, ClassId cls);

  ClassId Class() const { return cls_; }
  const ClassDef& ClassData() const;

  // ---- progression ----
  // These are PURE state changes (no events, no bus messages). Gameplay code never calls AddExp / RemoveExp / AddGold /
  // SpendGold directly: it goes through RewardService (hero/Rewards.h), which emits EvExpGained / EvLevelUp /
  // EvGoldChanged, publishes HeroLevelUpMsg and applies the Dying gates.
  int32_t Level() const { return level_; }
  int64_t Exp() const { return exp_; }  // exp into the current level
  int64_t ExpToNext() const;
  // addExp (5.2): ONE level per call; overflow stays and converts on the next call. On level-up: +5 stat points,
  // +1 skill point, derived recomputed WITH gear (C1 fix of Q2) and HP/MP refilled - unless Dying (no refill).
  LevelUpResult AddExp(int64_t amount, const EquipStats& eq);
  // Death toll (soul echo): removes exp within the current level only.
  void RemoveExp(int64_t amount);
  int64_t Gold() const { return gold_; }
  void AddGold(int64_t amount);
  bool SpendGold(int64_t amount);  // false (unchanged) when not enough
  void SetGold(int64_t gold) { gold_ = gold; }

  // ---- stats ----
  const PrimaryStats& BaseStats() const { return stats_; }  // class base + allocated points (raw)
  int32_t FreeStatPoints() const { return freeStatPoints_; }
  int32_t& MutableFreeSkillPoints() { return freeSkillPoints_; }
  int32_t FreeSkillPoints() const { return freeSkillPoints_; }
  // One point per call; false when no points.
  bool AllocateStat(PrimaryStat s);

  // ---- derived ----
  // recalcDerived (3) with the merged equip stats and the current spirit move multiplier. Does not clamp hp/mana.
  void RecalcDerived(const EquipStats& eq);
  const HeroDerived& Derived() const { return derived_; }
  const EquipStats& EquipStatsUsed() const { return eq_; }  // the bag of the last RecalcDerived
  // Ground speed in tiles/s (S5): moveSpeed / 36 (spirit Resonance and gear included); the caller multiplies the status
  // slow factor (C5).
  double GroundSpeedTilesPerSec() const;

  // ---- resources ----
  double Hp() const { return hp_; }
  double Mana() const { return mana_; }
  double MaxHp() const { return derived_.maxHp; }
  double MaxMana() const { return derived_.maxMana; }
  // HP/MP gains: `value = min(max, value + amount)`, exactly the web's heal sites (potions, life / mana steal, kill and
  // thorns heals, pet / merc heals, free-cast refunds; classes 3 "heals clamp", 4.3, 12.1). So a gain - even of 0 -
  // SNAPS a value that sits above a lowered max (unequipped +maxHp gear; recalcDerived does not clamp) down to the
  // max. Refused (no change) unless Alive (save-ui-input 5.1.1), or for a negative / non-finite amount (no web site
  // passes one). Returns the gain, >= 0 (0 when the value was snapped down).
  double Heal(double amount);
  double RestoreMana(double amount);
  void SpendMana(double amount);       // floor at 0
  double ApplyDamage(double amount);   // hp = max(0, hp - amount); returns the hp removed
  void SetHp(double hp) { hp_ = hp; }
  void SetMana(double mana) { mana_ = mana; }
  void FillHpMana();                   // hp = maxHp, mana = maxMana

  // ---- life ----
  HeroLife Life() const { return life_; }
  bool IsDead() const { return life_ != HeroLife::Alive || hp_ <= 0; }
  void SetLife(HeroLife l) { life_ = l; }

  // ---- position (tile space) ----
  Vec2 Position() const { return pos_; }
  void SetPosition(Vec2 p) { pos_ = p; }
  Vec2 Facing() const { return facing_; }
  void SetFacing(Vec2 f) { facing_ = f; }

  // ---- components ----
  SkillBook& Skills() { return skills_; }
  const SkillBook& Skills() const { return skills_; }
  BuffList& Buffs() { return buffs_; }
  const BuffList& Buffs() const { return buffs_; }
  Spirit& GetSpirit() { return spirit_; }
  const Spirit& GetSpirit() const { return spirit_; }

  // ---- regen (4.1-4.2), in the web's per-step order (16; ZoneScene.ts:1372-1392, Player.update) ----
  // Step 6, FIRST (CombatSystem::TickPassives calls it before the Unyielding hp-ratio check): the Life Regen passive
  // (4.2), linear passiveRule.hpPerSecondPerLevel x level HP/s times mods.hpMul, while 0 < hp < maxHp. Skipped while
  // dead (Dying or hp <= 0).
  void TickLifeRegen(double dtMs, const RegenModifiers& mods);
  // Step 7 (Player.update, after the spirit drain): mana regen, then HP regen (4.1), each clamped at its max. Skipped
  // while dead. Uses the RAW primaries and the gear hpRegen / manaRegen of the last RecalcDerived bag.
  // `mods` (both calls): the step's recovery modifiers (stepRegen below).
  void TickRegen(double dtMs, const RegenModifiers& mods);

  // CombatEntity view for the damage formula (combat-feel 1.2).
  Combatant AsCombatant() const;

  // ---- session flags ----
  bool autoCombat = false;
  AutoLootMode autoLoot = AutoLootMode::Off;
  EntityId attackTarget = kNoEntity;
  double lastAttackMs = 0;          // basic attack swing timer (first swing immediate)
  double deathSaveReadyAtMs = 0;    // T11 / FIX Q24 (survives zone changes; not saved)
  bool dodgeCounterReady = false;   // dodgeCounter gear
  // The step's recovery modifiers (the web's `recovery`: campfire x50 within 5 tiles of a camp, poisoned x0.5 HP),
  // computed ONCE per step by CombatSystem::TickPassives from the position BEFORE this step's movement (16 steps 2, 5)
  // and passed to both TickLifeRegen (step 6) and TickRegen (step 7). Not saved.
  RegenModifiers stepRegen;
  // (HUD potion quick slots, I4, are owned by InventorySystem: PotionSlots / ResolvePotionSlot.)

  // ---- save ----
  void ToSave(SaveHero& out) const;
  // Restores level/exp/gold/stats/points/skills/spirit and the DEFAULT hotbar (GameSim applies SaveData.hotbar after
  // it when present, C3); clears buffs / cooldowns, life = Alive, derived with the current bag; hp / mana are set raw
  // (non-finite hp -> 0, mana -> maxMana) for GameSim to clamp after gear (FIX Q8 / Q35). No position (GameSim places
  // the hero). Normalises level >= 1 and non-negative exp / gold / points.
  void FromSave(const SaveHero& in);

 private:
  const DataStore* data_;
  ClassId cls_;
  const ClassDef* class_;
  int32_t level_ = 1;
  int64_t exp_ = 0;
  int64_t gold_ = 0;
  PrimaryStats stats_;
  int32_t freeStatPoints_ = 0;
  int32_t freeSkillPoints_ = 0;
  HeroDerived derived_;
  EquipStats eq_;
  double hp_ = 0;
  double mana_ = 0;
  HeroLife life_ = HeroLife::Alive;
  Vec2 pos_;
  Vec2 facing_{1, 0};
  SkillBook skills_;
  BuffList buffs_;
  Spirit spirit_;
};

}  // namespace abyss

// Packs, morale, kite, block and buffs: the pure rules (dark remake K5, implementation-plan.md 3 K5; monsters.md 2.2-2.5).
//
// Owner area: monsters. Pure functions over the K5 data (MonsterData.h "K5", MapData.h MapSpawnDef mix / leader);
// MonsterSystem runs packs, morale, the shield wall and auras (MonsterSystem.h "K5"), MonsterAI the flee and kite steps
// (MonsterAI.h "K5"), CombatSystem the block and the buff / call abilities (Combat.h "K5"). No RNG here.
//
// * Packs: a map spawn entry is a pack (monsters-ai.md 6.1); its members spawn in mix order (ExpandSpawnMembers), the
//   leader is the first member of the leader's id. A hunt's leader leads its minions.
// * Block: a hit lands in the front arc when the angle between the blocker's heading and the direction to where the hit
//   came from is at most arcDeg / 2 (the edge counts); a hit with no direction (from on the blocker) is not blocked.
// * Buffs: the multiplier of a stat is the largest among the monster's live buffs of that stat and the auras around it
//   (no stacking); 1 when none. fearImmune is a flag (any live buff or aura of it).
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "abyss/base/Platform.h"
#include "abyss/base/Types.h"
#include "abyss/data/MapData.h"
#include "abyss/data/MonsterData.h"
#include "abyss/monsters/Monster.h"

namespace abyss {

// ---- packs ----
struct SpawnMember {
  std::string monsterId;
  bool leader = false;
};
// The members of a spawn entry in spawn order: its mix expanded (each entry `count` times, list order), else `count` x
// its monsterId; the first member of the leader's id is the leader (none when the leader names no member).
ABYSS_API void ExpandSpawnMembers(const MapSpawnDef& spawn, std::vector<SpawnMember>& out);

// ---- block ----
ABYSS_API bool BlockArcContains(Vec2 pos, Vec2 heading, double arcDeg, Vec2 from);
// A blocked hit: floor(amount x damageMul), never below min(amount, 1) (a landed hit stays a hit).
ABYSS_API double BlockedDamage(double amount, double damageMul);
// The shared heading of a shield wall: the mean of its members' headings (their sum, normalised, in list order); the
// first heading when they cancel out; +col for none.
ABYSS_API Vec2 WallHeading(std::span<const Vec2> headings);

// ---- buffs and auras ----
// The multiplier `stat` has from `buffs` live at `nowMs` (untilMs > now) and an aura multiplier `auraMul` (1 = none):
// the largest of them, else 1 (fearImmune has none: use HasLiveBuff).
ABYSS_API double BuffMultiplier(std::span<const MonsterAbilityBuff> buffs, MonsterBuffStat stat, double nowMs,
                                double auraMul, bool hasAura);
ABYSS_API bool HasLiveBuff(std::span<const MonsterAbilityBuff> buffs, MonsterBuffStat stat, double nowMs);
// The damage multiplier of `m` at `nowMs` (its live damage buffs and the damage aura on it this step): the damage
// formula's outgoing multiplier on its swings, ability hits and bolts.
ABYSS_API double MonsterDamageBuffMul(const MonsterInstance& m, double nowMs);
// `m` cannot be feared at `nowMs`: a live fearImmune buff or a fearImmune aura on it this step.
ABYSS_API bool MonsterFearImmune(const MonsterInstance& m, double nowMs);
// Recomputes the multipliers `m` has this step (MonsterInstance buffDamageMul / buffAttackSpeedMul / buffMoveSpeedMul /
// fearImmune) from its live buffs and its auraMul / auraMask, dropping buffs that ran out (untilMs <= nowMs).
ABYSS_API void RefreshMonsterBuffs(MonsterInstance& m, double nowMs);
// The bit set of the stats its buffs and auras change now (bit per MonsterBuffStat; the snapshot's buffMask).
ABYSS_API uint32_t MonsterBuffMask(const MonsterInstance& m);
// `family` filters a buff, call or aura: empty = every monster, else the monster's family.
ABYSS_API bool FamilyMatches(const std::string& filter, const MonsterDef& def);
// A sustain member (morale): it has a buff / call ability that grants fearImmune.
ABYSS_API bool GrantsFearImmunity(const MonsterDef& def);

}  // namespace abyss

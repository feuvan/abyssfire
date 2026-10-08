// Runtime monster instance (monsters-ai.md 1.2-1.3, 3.1).
//
// Owner area: monsters. A MonsterInstance is plain state; behaviour lives in MonsterAI.h (pure AI tick) and
// MonsterSystem.h (spawning, damage, death, respawn, queries).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/Platform.h"
#include "abyss/base/Stats.h"
#include "abyss/base/Types.h"
#include "abyss/data/CombatData.h"
#include "abyss/data/MonsterData.h"
#include "abyss/hero/Buffs.h"

namespace abyss {

// AI states (monsters 3.1) + the port's leash `Returning` state (M1).
enum class MonsterState : uint8_t { Idle, Patrol, Chase, Attack, Returning, Dead };
ABYSS_ENUM_STRINGS(MonsterState, "idle", "patrol", "chase", "attack", "returning", "dead")

// Why a monster exists; drives respawn eligibility (M4/W9), affix rolls and loot floors (monsters 1.2).
// StoryBoss (M7): the zone spawn of MonsterAiDef::storyBossId (Ch1 goblin_chief); once per visit, no respawn.
enum class MonsterRole : uint8_t {
  Regular,
  ZoneMiniBoss,
  SubDungeonMiniBoss,
  HuntLeader,
  HuntMinion,
  AmbushSpawn,
  RescueSpawn,
  DefendWave,
  LabyrinthFloor,
  SealKeeper,
  StoryBoss,
};
ABYSS_ENUM_STRINGS(MonsterRole, "regular", "miniBoss", "subDungeonMiniBoss", "hunt", "huntMinion", "ambush", "rescue",
                   "defendWave", "labyrinth", "sealKeeper", "storyBoss")

// One rolled elite affix with its behaviour timers (combat-feel.md 17).
struct MonsterAffix {
  EliteAffixType type = EliteAffixType::FireEnhanced;
  double lastTeleportMs = -1e300;  // first teleport check is immediate
  double lastCurseLogMs = -1e300;
};

struct ABYSS_API MonsterInstance {
  EntityId id = kNoEntity;
  MonsterDef def;           // current definition (difficulty / hunt / affix modified copy)
  MonsterDef originalDef;   // post-difficulty, pre-affix (respawn source; affixes never compound)
  MonsterRole role = MonsterRole::Regular;
  bool noRespawn = false;   // M4 / W9 and role rules
  std::string huntId;       // hunt leader: hunt id (== def.id)
  double visualScale = 1.0; // render-only (hunt leaders 1.25)
  // vitals
  double hp = 0, maxHp = 0;
  PrimaryStats stats;       // {str floor(dmg*0.8), dex floor(speed*0.1), vit floor(hp*0.1), 3, 3, 3}
  BuffList buffs;           // death_mark amplify, taunted; pruned by duration (FIX Q19)
  std::vector<MonsterAffix> affixes;
  // position / movement (tile space)
  Vec2 pos;
  Vec2 heading{1, 0};       // unit vector; render yaw follows it (M11 turn rate is render-only)
  double moveSpeed = 0;     // current smoothed speed, tiles/s (velocity form of monsters 3.4)
  TilePos spawnAnchor;      // original zone-entry anchor (M3: respawn and leash use it, never drifts)
  // AI
  MonsterState state = MonsterState::Idle;
  double patrolTimerMs = 0;
  bool hasPatrolTarget = false;
  TilePos patrolTarget;
  double patrolStartedMs = 0;       // M10 patrol timeout
  double provokedUntilMs = 0;       // M2
  std::vector<TilePos> path;        // M6 A* path when line-of-walk is blocked
  double repathAtMs = 0;
  // combat
  double lastAttackMs = 0;          // last swing start (0 = never; first swing immediate)
  double lastDamagedMs = 0;
  bool hasLastHitFrom = false;
  Vec2 lastHitFrom;                 // death throw direction (render)
  double lastEscortChipMs = -1e300; // quests 3.9
  double lastDefendChipMs = 0;      // quests 3.10
  // presentation flags
  bool storyNameShown = false;      // boss label renamed (story 8.3)
  bool active = true;               // in the AI activity set (30 tiles / aggro), monsters 3.8

  bool IsAlive() const { return state != MonsterState::Dead && hp > 0; }
  bool IsAggro() const { return state == MonsterState::Chase || state == MonsterState::Attack; }
  int32_t AffixCount() const { return static_cast<int32_t>(affixes.size()); }
  double AffixLootBonus(const EliteAffixTable& table) const;
};

}  // namespace abyss

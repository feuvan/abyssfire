// MonsterSystem: the zone's monster population (spawning, AI driver, damage, death, respawn, mini-boss, quest hunts,
// event spawns, elite behaviours) and the monster queries every other area uses.
// Spec: monsters-ai.md 3.8-3.10 (driver, tick order, external writes), 4 (swing scheduling lives in CombatSystem),
// 5 (takeDamage / heal / die), 6 (spawning, safe zones, event spawns), 7 (respawn), 8 (mini-bosses), 9 (hunts),
// 11 (kill hook contract), 12 (spatial grid), 14 (core API proposal); combat-feel.md 17.4 (elite behaviours);
// DECISIONS M1-M10, W9, D13 T9 (respawn on the sim clock).
//
// Owner area: monsters. Runtime system (SimContext). Timers: TimerOwner::Monsters (respawn T9). RNG: RngStream::Ai.
// Kill credit: ApplyDamage publishes MonsterKilledMsg exactly once, on the alive -> dead transition, synchronously;
// GameSim's subscriptions run the kill pipeline (monsters-ai 11 order) and MonsterSystem::OnMonsterKilled is the last
// handler (log zone.monsterKill + respawn decision).
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "abyss/base/Platform.h"
#include "abyss/base/Timers.h"
#include "abyss/base/Types.h"
#include "abyss/combat/CombatInput.h"
#include "abyss/monsters/Monster.h"
#include "abyss/monsters/SpatialGrid.h"
#include "abyss/sim/GameplayBus.h"
#include "abyss/sim/SimTypes.h"

namespace abyss {

struct SimContext;
struct Snapshot;
struct SaveData;

enum class MonsterTimerKind : uint16_t { Respawn = 1 };

struct MonsterSpawnParams {
  const MonsterDef* baseDef = nullptr;  // pre-difficulty definition (the system scales it)
  bool alreadyScaled = false;           // def is final (hunt / defend wave paths scale themselves)
  TilePos tile;
  MonsterRole role = MonsterRole::Regular;
  bool rollAffixes = false;             // monsters 1.3 "who rolls affixes"
  bool startChasing = false;            // ambush / rescue / defend spawns
  std::string huntId;
  double visualScale = 1.0;
};

struct DamageFlags {
  bool isCrit = false;
  bool isTick = false;
  bool hasFrom = false;
  Vec2 from;
  EntityId attacker = kNoEntity;
  KillSource source = KillSource::Other;
  bool provokes = true;  // M2: hero hits provoke
};

class ABYSS_API MonsterSystem {
 public:
  explicit MonsterSystem(SimContext& ctx);

  // ---- zone lifecycle ----
  // Zone entry spawn order (monsters 6.1): zone spawns -> (NPCs etc. elsewhere) -> mini-boss -> hunts (announce=false).
  // M7 story boss (MonsterAiDef storyBoss*): the spawn entries of storyBossId get MonsterRole::StoryBoss (never
  // respawn in the visit) and are skipped when storyBossNotAfterQuestTurnIn is turned in, unless chapterCompleteQuest is
  // turned in too and storyBossFarmableAfterChapter.
  void SpawnZonePopulation();
  void SpawnMiniBoss();  // M7: once per visit; goblin_chief rules
  void OnZoneExit();     // drops every monster and respawn timer (monsters are not saved)

  // ---- spawning ----
  EntityId Spawn(const MonsterSpawnParams& p);
  // Quest hunts (9.4): spawns every due hunt of the current zone not present. announce -> zone.quest.huntRevealed
  // log + camera shake + EvHuntRevealed. Triggers: zone entry (false), QuestAcceptedMsg (false), QuestProgressMsg (true).
  void SpawnDueHunts(bool announce);
  // Event spawns (6.4): count monsters picked from ids around `centre` (ring placement + findWalkableTile), chasing,
  // no affixes, noRespawn (M4/W9). Returns the new ids in spawn order.
  std::vector<EntityId> SpawnAmbush(std::span<const std::string> monsterIds, int32_t count, Vec2 centre,
                                    double minDist, double distRange, MonsterRole role);

  // ---- per step (GameSim, monsters 3.9 order) ----
  // 8.3: before the AI loop. Opening the mini-boss panel is a core-owned modal (PanelId::MiniBossDialogue): it blocks
  // hero gameplay input (U7) but does not freeze the sim (D13: it holds only the mini-boss).
  void CheckMiniBossDialogue();
  // 3.8: activity set (250 ms), safe-zone repel, immobilized skip, AI, grid update. Every Idle / Patrol / Returning ->
  // Chase transition publishes MonsterAggroMsg (A7 aggro vocalisation).
  void TickAI(double dtMs);
  void TickEliteBehaviours();    // 17.4: teleporting + curse aura
  void OnTimer(const Timer& t);
  void DismissMiniBossDialogue();  // 8.3 onDismiss: boss -> chase; EvMiniBossDialogue{opened = false}

  // ---- damage (5) ----
  // takeDamage: dead -> Tick weight, nothing else. Else lastDamaged = now; hp = max(0, hp - amount); weight =
  // ClassifyHit; lastHitFrom; hp <= 0 -> state Dead, statuses NOT cleared here (kill pipeline does it),
  // MonsterKilledMsg published. A surviving monster with flags.provokes runs ProvokeMonster (MonsterAI.h, the M2
  // contract: idle / patrol, or chase with the hero beyond aggroRange; Returning ignores it) and publishes
  // MonsterAggroMsg when that took it out of Idle / Patrol.
  HitWeight ApplyDamage(EntityId id, double amount, const DamageFlags& flags);
  // heal(): ignored if dead, amount <= 0 or full (FIX: also used by vampiric).
  void Heal(EntityId id, double amount);
  void Teleport(EntityId id, Vec2 to, TeleportReason reason);
  // Forced state changes from other areas (3.10): taunt (idle/patrol -> chase), ambush spawns, mini-boss dialogue.
  void ForceChase(EntityId id);

  // ---- kill pipeline (last handler) ----
  void OnMonsterKilled(const MonsterKilledMsg& m);

  // ---- queries (alive filtering is explicit) ----
  MonsterInstance* Find(EntityId id);
  const MonsterInstance* Find(EntityId id) const;
  std::span<const MonsterInstance> All() const { return monsters_; }  // includes dead entries until respawn/unload
  // Alive monsters within radius of a point, grid order (12).
  void QueryAlive(Vec2 centre, double radius, std::vector<EntityId>& out) const;
  EntityId NearestAlive(Vec2 from, double maxRange) const;
  EntityId NearestAggro(Vec2 from) const;          // state chase/attack, whole map
  EntityId MonsterAtTile(Vec2 tile) const;         // findMonsterAt: |dcol| < 1.5 && |drow| < 1.5, first alive
  EntityId NearestAliveOfDef(std::string_view defId, Vec2 from) const;  // quest guide / boss scan
  bool AnyAttacking() const;                        // combat state (9.6)
  // TargetCandidate list (combat targeting helpers) of every monster in list order.
  void Candidates(std::vector<TargetCandidate>& out) const;
  EntityId MiniBoss() const { return miniBoss_; }
  bool MiniBossDialogueActive() const { return miniBossDialogueActive_; }
  bool IsHuntPresent(std::string_view huntId) const;

  const SpatialGrid& Grid() const { return grid_; }
  void FillSnapshot(Snapshot& out) const;
  // miniBossDialogueSeen (save 3.2).
  void WriteSave(SaveData& out) const;
  void ReadSave(const SaveData& in);

 private:
  void Respawn(EntityId deadId);
  bool InSafeZone(Vec2 p) const;

  SimContext& ctx_;
  std::vector<MonsterInstance> monsters_;  // spawn order; a respawn replaces the dead entry in place (7)
  SpatialGrid grid_;
  std::vector<EntityId> active_;           // activity set (3.8)
  double nextActiveRefreshMs_ = 0;
  EntityId miniBoss_ = kNoEntity;
  bool miniBossSpawnedThisVisit_ = false;
  bool miniBossDialogueActive_ = false;
  std::vector<std::string> miniBossDialogueSeen_;
  std::vector<std::pair<std::string, EntityId>> huntLeaders_;  // huntId -> leader
};

}  // namespace abyss

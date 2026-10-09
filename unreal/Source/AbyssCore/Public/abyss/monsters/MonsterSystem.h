// MonsterSystem: the zone's monster population (spawning, AI driver, damage, death, respawn, mini-boss, quest hunts,
// event spawns, elite behaviours) and the monster queries every other area uses.
// Spec: monsters-ai.md 3.8-3.10 (driver, tick order, external writes), 4 (swing scheduling lives in CombatSystem),
// 5 (takeDamage / heal / die), 6 (spawning, safe zones, event spawns), 7 (respawn), 8 (mini-bosses), 9 (hunts),
// 11 (kill hook contract), 12 (spatial grid), 14 (core API proposal); combat-feel.md 17.4 (elite behaviours);
// DECISIONS M1-M10, W9, D13 T9 (respawn on the sim clock).
//
// Owner area: monsters. Runtime system (SimContext). Timers: TimerOwner::Monsters (respawn T9). RNG: RngStream::Ai
// (patrol, zone-spawn / respawn / minion placement, elite affix rolls, elite teleports); SpawnAmbush (random-event
// ambush / rescue contents) draws from RngStream::Events (Rng.h stream rules).
// Kill credit: ApplyDamage publishes MonsterKilledMsg exactly once, on the alive -> dead transition, synchronously;
// GameSim's subscriptions run the kill pipeline (monsters-ai 11 order) and MonsterSystem::OnMonsterKilled is the last
// handler (log zone.monsterKill + respawn decision).
#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "abyss/base/Platform.h"
#include "abyss/base/Timers.h"
#include "abyss/base/Types.h"
#include "abyss/combat/CombatInput.h"
#include "abyss/monsters/Hunts.h"
#include "abyss/monsters/Monster.h"
#include "abyss/monsters/MonsterAI.h"
#include "abyss/monsters/SpatialGrid.h"
#include "abyss/sim/GameplayBus.h"
#include "abyss/sim/SimTypes.h"

namespace abyss {

struct SimContext;
struct Snapshot;
struct SaveData;
struct SubDungeonDef;

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
  std::string affixZone;                // zone whose affix count applies ("" = the current zone; sub-dungeons: parent)
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
  // Placement per monster (M10, FIX Q16): up to placementTries tiles spawn +/- spawnJitter (2 RngStream::Ai draws each,
  // col first, clamped to [1, size - 2]; walkable and outside every camp radius), else the anchor when walkable; elites
  // roll their affixes right after their placement (web draw order).
  void SpawnZonePopulation();
  // M7: once per visit at its fixed tile (bounds check only), always rolls affixes. In a sub-dungeon (the zone id is a
  // world SubDungeonDef id) it spawns that sub-dungeon's boss instead (8.5, later milestone).
  void SpawnMiniBoss();
  // Sub-dungeon mini-boss (8.5): the sub-dungeon's fixed tile (bounds check against its size only), difficulty-scaled,
  // affixes rolled with the PARENT zone's count, MonsterRole::SubDungeonMiniBoss (never respawns, no dialogue tree).
  // Becomes MiniBoss(). Returns kNoEntity for an unknown boss or an out-of-bounds tile.
  EntityId SpawnSubDungeonMiniBoss(const SubDungeonDef& sd);
  void OnZoneExit();     // drops every monster (EvEntityDespawned ZoneUnload) and respawn timer (monsters are not saved)

  // ---- spawning ----
  // One monster at a tile: difficulty scaling unless alreadyScaled, stats, spawn anchor = tile, role flags (noRespawn
  // from monster_ai.json port.noRespawn + mini-bosses / seal keepers), optional affixes for the zone, EvEntitySpawned;
  // startChasing spawns publish MonsterAggroMsg. No walkability check (callers place).
  EntityId Spawn(const MonsterSpawnParams& p);
  // Quest hunts (9.4): spawns every due hunt of the current zone not present. announce -> zone.quest.huntRevealed
  // log + camera shake + EvHuntRevealed. Triggers: zone entry (false), QuestAcceptedMsg (false), QuestProgressMsg (true).
  void SpawnDueHunts(bool announce);
  // The spawning half of SpawnDueHunts for an already computed due list (HuntsToSpawn order): leader (hunt def,
  // difficulty, affixes, visual scale), minions (one placement attempt each), announce. Unknown base monsters and
  // hunts without a walkable spot are skipped (retried on the next trigger).
  void SpawnHunts(std::span<const DueHunt> due, bool announce);
  // Event spawns (6.4): count monsters picked from ids around `centre` (ring placement + findWalkableTile), chasing,
  // no affixes, noRespawn (M4/W9). Returns the new ids in spawn order. Ambush: minDist 3, distRange 2 around the event
  // point; rescue: minDist 2, distRange 3 around the rescue NPC (the caller passes count = max(2, randomInt - 1)).
  // Draws (RngStream::Events) per monster: id index floor(rand * len) (an unknown id is skipped before the next two),
  // angle rand * 2 pi, distance minDist + rand * distRange; tile = round(centre + dir * dist) clamped to [1, size - 2],
  // else the first walkable tile of rings 1..5 (dr outer, dc inner).
  std::vector<EntityId> SpawnAmbush(std::span<const std::string> monsterIds, int32_t count, Vec2 centre,
                                    double minDist, double distRange, MonsterRole role);
  // Defend wave `waveIndex` (0-based; quests 3.10, monsters 6.4) around `target`: n = defendWaveBaseCount + w monsters
  // at angles 2 pi k / n on a circle of defendWaveRadius, JsRound, clamped to [edgeMargin, size - 1 - edgeMargin], NO
  // walkability check (web, quests Q8 later); each a uniformly random def of the zone's monster list
  // (RandomInt(0, len - 1), RngStream::Ai, bosses included), difficulty then ScaleDefendWave, chasing, no affixes,
  // MonsterRole::DefendWave (never respawns). Returns the ids in spawn order (empty without a zone list).
  std::vector<EntityId> SpawnDefendWave(Vec2 target, int32_t waveIndex);

  // ---- abstract chip damage for quest actors (monsters 4.4; quests 3.9 / 3.10): no animation, RNG or defense ----
  // `apply(monster, damage)` receives each hit in order and returns false to stop the loop (the actor died: the web
  // returns from the update there). The per-monster timer is stamped before `apply` is called.
  using ChipApply = std::function<bool(EntityId monster, double damage)>;
  // Escort (vector 20): every alive AGGRO monster in list order with distSq(monster, target) < escortChipRadiusSq and
  // now - lastEscortChipMs > escortChipIntervalMs (strictly; the first hit is immediate) deals
  // max(1, floor(def.damage * escortChipDamageMul)). Returns the total dealt.
  double ChipEscort(Vec2 target, const ChipApply& apply);
  // Defend target: each alive monster of `waveMonsters` (given order, aggro or not) with distSq < defendChipRadiusSq and
  // now - lastDefendChipMs > defendChipIntervalMs (lastDefendChipMs starts at 0) deals
  // max(1, floor(def.damage * defendChipDamageMul)). Returns the total dealt.
  double ChipDefendTarget(std::span<const EntityId> waveMonsters, Vec2 target, const ChipApply& apply);

  // ---- per step (GameSim, monsters 3.9 order) ----
  // 8.3: before the AI loop. Opening the mini-boss panel is a core-owned modal (PanelId::MiniBossDialogue): it blocks
  // hero gameplay input (U7) but does not freeze the sim (D13: it holds only the mini-boss).
  void CheckMiniBossDialogue();
  // 3.8: activity set (250 ms), safe-zone repel, immobilized skip, AI, grid update. Every Idle / Patrol / Returning ->
  // Chase transition publishes MonsterAggroMsg (A7 aggro vocalisation). The activity set is the monsters within
  // aiCullRadius of the hero (grid order) plus every aggro AND every Returning (M1) monster anywhere (list order), so a
  // leashed monster always finishes its walk home.
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
  // Boss intro rename (monsters 10 / story 8.3): the StoryDirector's 250 ms scan marks the nearest live instance of a
  // story boss within the bar range once; the snapshot then shows the intro name. True when newly marked (the caller
  // emits EvMonsterRenamed); false for unknown / dead / already renamed monsters. A respawn starts unmarked.
  bool MarkStoryNamed(EntityId id);

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
  // The walkability / A* service the AI and every placement use (built from ZoneRuntime at zone entry).
  const MonsterWorld& World() const { return world_; }
  // Test hook: replaces the zone's walkability / A* service (bounds included) until the next zone entry.
  void SetWorldForTesting(MonsterWorld world);
  void FillSnapshot(Snapshot& out) const;
  // miniBossDialogueSeen (save 3.2).
  void WriteSave(SaveData& out) const;
  void ReadSave(const SaveData& in);

 private:
  void Respawn(EntityId deadId);
  bool InSafeZone(Vec2 p) const;
  bool Walkable(int32_t col, int32_t row) const;
  void PrepareZone();      // zone entry: clears the population, sizes the grid, builds the world service
  void EnsureZoneGrid();   // lazily sizes the grid / world for the live zone (spawns before SpawnZonePopulation)
  void BuildWorld();
  bool RoleNeverRespawns(MonsterRole role) const;
  bool StoryBossBlocked() const;  // M7: the story boss stays away after its quest (until the chapter is done)
  // A fresh instance from a final (scaled) definition at a tile (stats, hp, anchor, role flags); not yet listed.
  MonsterInstance MakeInstance(const MonsterDef& def, TilePos tile, MonsterRole role);
  void RollAffixesInto(MonsterInstance& m, std::string_view zoneId);
  EntityId AddInstance(MonsterInstance&& m, bool startChasing);
  void PublishAggro(const MonsterInstance& m, MonsterState previous);
  void EmitSpawned(const MonsterInstance& m);
  std::string CurrentZoneId() const;

  SimContext& ctx_;
  std::vector<MonsterInstance> monsters_;  // spawn order; a respawn replaces the dead entry in place (7)
  SpatialGrid grid_;
  std::vector<EntityId> active_;           // activity set (3.8)
  double nextActiveRefreshMs_ = 0;
  bool activeDue_ = true;                  // the first activity refresh of a zone is immediately due
  MonsterWorld world_;
  bool worldOverride_ = false;
  std::string preparedMapId_;
  int32_t preparedCols_ = 0, preparedRows_ = 0;
  EntityId miniBoss_ = kNoEntity;
  EntityId miniBossDialogueMonster_ = kNoEntity;
  bool miniBossSpawnedThisVisit_ = false;
  bool miniBossDialogueActive_ = false;
  std::vector<std::string> miniBossDialogueSeen_;
  std::vector<std::pair<std::string, EntityId>> huntLeaders_;  // huntId -> leader
};

}  // namespace abyss

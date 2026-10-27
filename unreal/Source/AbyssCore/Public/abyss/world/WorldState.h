// Dark remake K6 world state (implementation-plan.md 3 K6; chapter1.md 3 quest-gated lines, 8 saved world state): the
// pure rules behind the world flags, NPC lines and NPC visibility, spawn variants and permanent quest rewards. No RNG,
// no I/O. The state lives in SessionState (worldFlags, permanentRewards; saved, SaveData.h); its consumers are:
//   * SimImpl::SetQuestFlags (SimWiring): a quest's setsFlags on its turn-in, the first QuestTurnedInMsg listener
//     (EvWorldFlag per newly set flag, in the quest's data order);
//   * QuestWorld::TurnIn: the permanent reward, once per (quest, difficulty), after the items (EvPermanentReward);
//     SimImpl::RebuildEquipStats adds its max life (a flat MaxHp), the points went to the hero's free points;
//   * ZoneRuntime: NPC visibility (visibleWhen) at the zone's entry and at the start of every ZoneRuntime::Tick;
//     QuestWorld::InteractNpc (the logged line) and DialogueSystem::OpenLinear (DialogueView::lineKey): NPC lines;
//   * MonsterSystem: the spawn variant of a map entry at the zone's spawn and at each respawn; QuestWorld's guide spawn
//     points;
//   * the snapshot (Snapshot::worldFlags, permanentRewards): UE moods, audio emitters, the permanent reward line.
//
// The validator (CheckNpcVisibility; DataStore::Finalize reports its issues, the exporter's tables/world.ts mirrors it)
// finds the NPCs whose visibleWhen could hide them while a quest needs them. It reasons statically over the quest graph:
// while quest Q is offered (no record; for a reacceptable quest also offered again after it failed: a Failed record) /
// active / completed, Q's prerequisites (transitively) are turned in, the quests that need Q (transitively) have no
// record yet, and a flag is set when a prerequisite of Q sets it, unset when no quest sets it or only Q and the quests
// after Q do; anything else is unknown, and an unknown counts as "could hide". A flag set by anything but a quest's
// setsFlags (K7's cleanse) is outside the model until its package extends it.
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/Platform.h"
#include "abyss/data/DialogueData.h"
#include "abyss/data/MapData.h"
#include "abyss/data/NpcData.h"
#include "abyss/data/QuestData.h"
#include "abyss/data/WorldStateData.h"
#include "abyss/monsters/Packs.h"
#include "abyss/quests/QuestSystem.h"
#include "abyss/sim/Session.h"

namespace abyss {

// What a condition reads: the world flags (sorted) and the quests' states (nullptr = no quest has a record).
struct WorldStateView {
  std::span<const std::string> flags;
  const QuestSystem* quests = nullptr;
};

// ---- conditions ----
ABYSS_API bool QuestGateHolds(QuestGate gate, bool hasRecord, QuestStatus status);
ABYSS_API bool WorldLeafHolds(const WorldConditionLeaf& leaf, const WorldStateView& w);
// Every leaf of `all`, and one of `any` when it is not empty.
ABYSS_API bool WorldConditionHolds(const WorldCondition& c, const WorldStateView& w);

// ---- NPCs ----
// The keys of the lines the NPC says now: its `lines` whose `when` holds, in data order. Empty when it has no `lines` (the
// caller keeps the web's `dialogue`) or when none holds.
ABYSS_API std::vector<std::string> NpcLinesNow(const NpcDef& npc, const WorldStateView& w);
// The linear panel's line (the web's dialogue[1] ?? [0] over NpcLinesNow); "" when the NPC has no `lines` or none holds.
ABYSS_API std::string NpcPanelLineNow(const NpcDef& npc, const WorldStateView& w);
// No visibleWhen, or it holds.
ABYSS_API bool NpcVisibleNow(const NpcDef& npc, const WorldStateView& w);

// ---- the validator ----
enum class NpcNeed : uint8_t { Giver, Talk, Craft };
ABYSS_ENUM_STRINGS(NpcNeed, "giver", "talk", "craft")

struct NpcVisibilityIssue {
  std::string npcId;
  std::string questId;
  NpcNeed need = NpcNeed::Giver;
  // true: only while the quest is offered (its giver could hide before it is accepted, or is hidden for sure while
  // another NPC that is visible for sure offers it: the quest waits), a warning. false: an error: while the quest is
  // open (a giver while it is active or completed, a talk / craft NPC while it is active), or a giver hidden for sure
  // whenever the quest is offered (or offered again after failing) with no other NPC listing it or owning a dialogue
  // tree whose questTrigger names it (visible for sure then): the quest could never be accepted.
  bool offerOnly = false;
  std::string message;
};
// Every NPC with a visibleWhen against every quest that needs it (it lists the quest; a talk objective names it; the
// quest's craft phases name it). Ordered by NPC (npcs.json order), then quest (quests.json order). `trees`: the
// dialogue trees an NPC's questTrigger choices accept quests from (nullptr: none).
ABYSS_API std::vector<NpcVisibilityIssue> CheckNpcVisibility(const NpcTables& npcs, const QuestTables& quests,
                                                             const DialogueTables* trees = nullptr);
// Every quest that sets `flag` (setsFlags), in quests.json order.
ABYSS_API std::vector<const QuestDef*> QuestsSettingFlag(const QuestTables& quests, std::string_view flag);

// ---- spawn variants ----
// The entry's first variant whose flag is set (list order), or nullptr.
ABYSS_API const MapSpawnVariantDef* ActiveSpawnVariant(const MapSpawnDef& spawn, std::span<const std::string> flags);
// The entry's members now: its active variant's count x monsterId (no leader), else ExpandSpawnMembers.
ABYSS_API void ExpandSpawnMembersNow(const MapSpawnDef& spawn, std::span<const std::string> flags,
                                     std::vector<SpawnMember>& out);

// ---- permanent rewards ----
// Records (questId, difficulty) and adds the reward to the totals unless that pair was claimed before (or the reward is
// absent). The totals saturate at int32 max (a negative one counts as 0). Returns whether it was granted.
ABYSS_API bool ClaimPermanentReward(PermanentRewardsState& state, std::string_view questId, Difficulty difficulty,
                                    const QuestPermanentRewardDef& reward);

}  // namespace abyss

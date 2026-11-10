// Dark remake K6 world state: conditions, NPC lines and visibility, the visibility validator, spawn variants and
// permanent quest rewards (world/WorldState.h; implementation-plan.md 3 K6). Pure: no RNG, no I/O.
#include "abyss/base/Platform.h"

#include "abyss/world/WorldState.h"

#include <algorithm>
#include <functional>

#include "abyss/base/StrUtil.h"

namespace abyss {

namespace {

bool WsHasFlag(std::span<const std::string> flags, std::string_view flag) {
  return std::binary_search(flags.begin(), flags.end(), flag, std::less<>());
}

bool WsContains(const std::vector<std::string>& v, std::string_view id) {
  return std::find(v.begin(), v.end(), id) != v.end();
}

// ---- the validator's static model (header "The validator") ----

// Three-valued truth: what the data alone says about a condition while a quest is in a given state.
enum class WsTri : uint8_t { False, True, Unknown };

WsTri WsNot(WsTri t) {
  if (t == WsTri::True) return WsTri::False;
  if (t == WsTri::False) return WsTri::True;
  return WsTri::Unknown;
}

// The phase of quest Q the model reasons about: offered (no record, its prerequisites turned in), re-offered (a
// reacceptable quest that failed: a Failed record, offered again), active, completed.
enum class WsPhase : uint8_t { Offered, Reoffered, Active, Completed };

struct WsQuestModel {
  const QuestTables* quests = nullptr;
  std::string_view questId;
  std::vector<std::string_view> before;  // Q's prerequisites, transitively (turned in while Q is offered or open)
  std::vector<std::string_view> after;   // the quests that need Q, transitively (no record while Q is not turned in)
  WsPhase phase = WsPhase::Active;

  bool IsBefore(std::string_view id) const { return std::find(before.begin(), before.end(), id) != before.end(); }
  bool IsAfter(std::string_view id) const { return std::find(after.begin(), after.end(), id) != after.end(); }
};

// Q's prerequisites, transitively (cycle-safe; quests.json order of discovery).
void WsCollectBefore(const QuestTables& qt, std::string_view id, std::vector<std::string_view>& out) {
  const QuestDef* q = qt.Find(id);
  if (q == nullptr) return;
  for (const std::string& p : q->prereqQuests) {
    if (p == id || std::find(out.begin(), out.end(), std::string_view(p)) != out.end()) continue;
    out.push_back(p);
    WsCollectBefore(qt, p, out);
  }
}

// The quests whose prerequisites include Q, transitively.
void WsCollectAfter(const QuestTables& qt, std::string_view id, std::vector<std::string_view>& out) {
  for (const QuestDef& q : qt.quests) {
    if (!WsContains(q.prereqQuests, id) || q.id == id) continue;
    if (std::find(out.begin(), out.end(), std::string_view(q.id)) != out.end()) continue;
    out.push_back(q.id);
    WsCollectAfter(qt, q.id, out);
  }
}

// K7: quest `q` cleanses a sub-dungeon whose cleansedFlag is `flag` (a clear_area objective; QuestObjectiveDef::
// cleansedFlag is derived by DataStore::Finalize).
bool WsCleanses(const QuestDef& q, std::string_view flag) {
  for (const QuestObjectiveDef& o : q.objectives) {
    if (o.type == ObjectiveType::ClearArea && !o.cleansedFlag.empty() && o.cleansedFlag == flag) return true;
  }
  return false;
}

// A world flag while Q is in its phase: set when one of Q's prerequisites sets it (they are turned in); unset when no
// quest sets it, or only Q itself and the quests after Q do (none of them is turned in yet); otherwise unknown.
// K7: a flag a quest sets by cleansing (a clear_area objective) is set while that quest is open, not at its turn-in:
// when the cleanser is a prerequisite of Q it is set; when it is Q itself it is unset while Q was never accepted, set
// once Q is completed (every objective is done), and unknown while Q is active or offered again after failing (the
// flag stays from the first attempt); when the cleanser comes after Q it is unset.
WsTri WsFlagState(const WsQuestModel& m, std::string_view flag) {
  bool otherSetter = false;
  bool unknown = false;
  for (const QuestDef& q : m.quests->quests) {
    const bool turnIn = WsContains(q.setsFlags, flag);
    const bool cleanse = WsCleanses(q, flag);
    if (!turnIn && !cleanse) continue;
    if (m.IsBefore(q.id)) return WsTri::True;
    if (cleanse && q.id == m.questId) {
      if (m.phase == WsPhase::Completed) return WsTri::True;
      if (m.phase == WsPhase::Active || m.phase == WsPhase::Reoffered) unknown = true;
      continue;
    }
    if (q.id != m.questId && !m.IsAfter(q.id)) otherSetter = true;
  }
  return otherSetter || unknown ? WsTri::Unknown : WsTri::False;
}

WsTri WsQuestLeaf(const WsQuestModel& m, std::string_view questId, QuestGate gate) {
  if (questId == m.questId) {
    switch (m.phase) {
      case WsPhase::Offered: return QuestGateHolds(gate, false, QuestStatus::Active) ? WsTri::True : WsTri::False;
      case WsPhase::Reoffered: return QuestGateHolds(gate, true, QuestStatus::Failed) ? WsTri::True : WsTri::False;
      case WsPhase::Active: return QuestGateHolds(gate, true, QuestStatus::Active) ? WsTri::True : WsTri::False;
      case WsPhase::Completed: return QuestGateHolds(gate, true, QuestStatus::Completed) ? WsTri::True : WsTri::False;
    }
  }
  if (m.IsBefore(questId)) return QuestGateHolds(gate, true, QuestStatus::TurnedIn) ? WsTri::True : WsTri::False;
  if (m.IsAfter(questId)) return QuestGateHolds(gate, false, QuestStatus::Active) ? WsTri::True : WsTri::False;
  return WsTri::Unknown;
}

WsTri WsLeaf(const WsQuestModel& m, const WorldConditionLeaf& leaf) {
  switch (leaf.kind) {
    case WorldLeafKind::Flag: return WsFlagState(m, leaf.id);
    case WorldLeafKind::NotFlag: return WsNot(WsFlagState(m, leaf.id));
    case WorldLeafKind::Quest: return WsQuestLeaf(m, leaf.id, leaf.state);
  }
  return WsTri::Unknown;
}

WsTri WsCondition(const WsQuestModel& m, const WorldCondition& c) {
  WsTri all = WsTri::True;
  for (const WorldConditionLeaf& l : c.all) {
    const WsTri t = WsLeaf(m, l);
    if (t == WsTri::False) return WsTri::False;
    if (t == WsTri::Unknown) all = WsTri::Unknown;
  }
  if (c.any.empty()) return all;
  WsTri any = WsTri::False;
  for (const WorldConditionLeaf& l : c.any) {
    const WsTri t = WsLeaf(m, l);
    if (t == WsTri::True) {
      any = WsTri::True;
      break;
    }
    if (t == WsTri::Unknown) any = WsTri::Unknown;
  }
  if (any == WsTri::False) return WsTri::False;
  if (all == WsTri::True && any == WsTri::True) return WsTri::True;
  return WsTri::Unknown;
}

const char* WsPhaseName(WsPhase p) {
  switch (p) {
    case WsPhase::Offered: return "offered";
    case WsPhase::Reoffered: return "offered again after failing";
    case WsPhase::Active: return "active";
    case WsPhase::Completed: return "completed";
  }
  return "?";
}

// Whether quest Q can be accepted from an NPC other than `self` while Q is in `m`'s (offer) phase: another NPC lists Q
// (its quest card) or owns a dialogue tree with a choice whose questTrigger names Q, and has no visibleWhen or one that
// holds there for sure.
bool WsOtherOfferRoute(const WsQuestModel& m, const NpcTables& npcs, const DialogueTables* trees, const NpcDef& self) {
  for (const NpcDef& n : npcs.npcs) {
    if (n.id == self.id) continue;
    bool offers = WsContains(n.quests, m.questId);
    const DialogueTree* tree = trees != nullptr && !n.dialogueTreeId.empty() ? trees->Find(n.dialogueTreeId) : nullptr;
    if (!offers && tree != nullptr) {
      for (const DialogueNode& node : tree->nodes) {
        for (const DialogueChoice& c : node.choices) offers = offers || c.questTrigger == m.questId;
      }
    }
    if (offers && (!n.hasVisibleWhen || WsCondition(m, n.visibleWhen) == WsTri::True)) return true;
  }
  return false;
}

}  // namespace

// ---- conditions ----

bool QuestGateHolds(QuestGate gate, bool hasRecord, QuestStatus status) {
  switch (gate) {
    case QuestGate::NotStarted: return !hasRecord;
    case QuestGate::Active: return hasRecord && status == QuestStatus::Active;
    case QuestGate::Completed: return hasRecord && status == QuestStatus::Completed;
    case QuestGate::Open: return hasRecord && (status == QuestStatus::Active || status == QuestStatus::Completed);
    case QuestGate::TurnedIn: return hasRecord && status == QuestStatus::TurnedIn;
    case QuestGate::NotTurnedIn: return !(hasRecord && status == QuestStatus::TurnedIn);
  }
  return false;
}

bool WorldLeafHolds(const WorldConditionLeaf& leaf, const WorldStateView& w) {
  switch (leaf.kind) {
    case WorldLeafKind::Flag: return WsHasFlag(w.flags, leaf.id);
    case WorldLeafKind::NotFlag: return !WsHasFlag(w.flags, leaf.id);
    case WorldLeafKind::Quest: {
      bool hasRecord = false;
      const QuestStatus status =
          w.quests != nullptr ? w.quests->StatusOf(leaf.id, hasRecord) : QuestStatus::Active;
      return QuestGateHolds(leaf.state, w.quests != nullptr && hasRecord, status);
    }
  }
  return false;
}

bool WorldConditionHolds(const WorldCondition& c, const WorldStateView& w) {
  for (const WorldConditionLeaf& l : c.all) {
    if (!WorldLeafHolds(l, w)) return false;
  }
  if (c.any.empty()) return true;
  for (const WorldConditionLeaf& l : c.any) {
    if (WorldLeafHolds(l, w)) return true;
  }
  return false;
}

// ---- NPCs ----

std::vector<std::string> NpcLinesNow(const NpcDef& npc, const WorldStateView& w) {
  std::vector<std::string> out;
  for (const NpcLineDef& l : npc.lines) {
    if (!l.hasWhen || WorldConditionHolds(l.when, w)) out.push_back(l.key);
  }
  return out;
}

std::string NpcPanelLineNow(const NpcDef& npc, const WorldStateView& w) {
  const std::vector<std::string> lines = NpcLinesNow(npc, w);
  if (lines.empty()) return std::string();
  return lines.size() > 1 ? lines[1] : lines[0];
}

bool NpcVisibleNow(const NpcDef& npc, const WorldStateView& w) {
  return !npc.hasVisibleWhen || WorldConditionHolds(npc.visibleWhen, w);
}

// ---- the validator ----

std::vector<const QuestDef*> QuestsSettingFlag(const QuestTables& quests, std::string_view flag) {
  std::vector<const QuestDef*> out;
  for (const QuestDef& q : quests.quests) {
    if (WsContains(q.setsFlags, flag) || WsCleanses(q, flag)) out.push_back(&q);
  }
  return out;
}

std::vector<NpcVisibilityIssue> CheckNpcVisibility(const NpcTables& npcs, const QuestTables& quests,
                                                   const DialogueTables* trees) {
  std::vector<NpcVisibilityIssue> out;
  for (const NpcDef& npc : npcs.npcs) {
    if (!npc.hasVisibleWhen) continue;
    for (const QuestDef& q : quests.quests) {
      // How the quest needs this NPC: it lists the quest (gives and receives it), a talk objective names it, or the
      // quest's craft phases name it.
      const bool giver = WsContains(npc.quests, q.id);
      bool talk = false;
      for (const QuestObjectiveDef& o : q.objectives) {
        talk = talk || (o.type == ObjectiveType::Talk && o.targetId == npc.id);
      }
      const bool craft =
          q.craftPhases.present && (q.craftPhases.craftNpc == npc.id || q.craftPhases.deliverNpc == npc.id);
      if (!giver && !talk && !craft) continue;
      WsQuestModel m;
      m.quests = &quests;
      m.questId = q.id;
      WsCollectBefore(quests, q.id, m.before);
      WsCollectAfter(quests, q.id, m.after);
      const auto check = [&](WsPhase phase, NpcNeed need, bool offerOnly) {
        m.phase = phase;
        if (WsCondition(m, npc.visibleWhen) == WsTri::True) return false;
        NpcVisibilityIssue issue;
        issue.npcId = npc.id;
        issue.questId = q.id;
        issue.need = need;
        issue.offerOnly = offerOnly;
        issue.message = StrCat("visibleWhen could hide '", npc.id, "' while quest '", q.id, "' is ", WsPhaseName(phase),
                               " and needs it (", EnumName(need), ")");
        out.push_back(std::move(issue));
        return true;
      };
      // The offer phases (never accepted; a reacceptable quest offered again after it failed): a giver that could hide
      // meanwhile only delays the quest (a warning), but one hidden for sure while nothing else offers the quest makes
      // it impossible to accept (an error).
      const auto checkOffer = [&](WsPhase phase) {
        m.phase = phase;
        const WsTri t = WsCondition(m, npc.visibleWhen);
        if (t == WsTri::True) return false;
        if (t == WsTri::Unknown || WsOtherOfferRoute(m, npcs, trees, npc)) return check(phase, NpcNeed::Giver, true);
        NpcVisibilityIssue issue;
        issue.npcId = npc.id;
        issue.questId = q.id;
        issue.need = NpcNeed::Giver;
        issue.offerOnly = false;
        issue.message = StrCat("visibleWhen hides '", npc.id, "' whenever quest '", q.id, "' is ", WsPhaseName(phase),
                               " and nothing else offers it: the quest can never be accepted (giver)");
        out.push_back(std::move(issue));
        return true;
      };
      // Open phases first (errors); the offer phases only when the open ones are fine (one issue per need).
      if (giver && !check(WsPhase::Active, NpcNeed::Giver, false) && !check(WsPhase::Completed, NpcNeed::Giver, false) &&
          !checkOffer(WsPhase::Offered) && q.reacceptable) {
        checkOffer(WsPhase::Reoffered);
      }
      if (talk) check(WsPhase::Active, NpcNeed::Talk, false);
      if (craft) check(WsPhase::Active, NpcNeed::Craft, false);
    }
  }
  return out;
}

// ---- spawn variants ----

const MapSpawnVariantDef* ActiveSpawnVariant(const MapSpawnDef& spawn, std::span<const std::string> flags) {
  for (const MapSpawnVariantDef& v : spawn.variants) {
    if (WsHasFlag(flags, v.whenFlag)) return &v;
  }
  return nullptr;
}

void ExpandSpawnMembersNow(const MapSpawnDef& spawn, std::span<const std::string> flags, std::vector<SpawnMember>& out) {
  const MapSpawnVariantDef* v = ActiveSpawnVariant(spawn, flags);
  if (v == nullptr) {
    ExpandSpawnMembers(spawn, out);
    return;
  }
  out.clear();
  for (int32_t i = 0; i < v->count; ++i) out.push_back(SpawnMember{v->monsterId, false});
}

// ---- permanent rewards ----

bool ClaimPermanentReward(PermanentRewardsState& state, std::string_view questId, Difficulty difficulty,
                          const QuestPermanentRewardDef& reward) {
  if (!reward.present || questId.empty() || state.Claimed(questId, difficulty)) return false;
  state.claimed.push_back(PermanentRewardClaim{std::string(questId), difficulty});
  // Saturating: a loaded total may already sit at int32 max (SaveIO clamps a hand-edited one there).
  const auto add = [](int32_t& total, int32_t amount) {
    const int64_t sum = int64_t{(std::max)(0, total)} + int64_t{(std::max)(0, amount)};
    total = static_cast<int32_t>((std::min)(sum, int64_t{INT32_MAX}));
  };
  add(state.maxLife, reward.maxLife);
  add(state.statPoints, reward.statPoints);
  add(state.skillPoints, reward.skillPoints);
  add(state.respecTokens, reward.respecTokens);
  return true;
}

}  // namespace abyss

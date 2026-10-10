// Dialogue runtime (quests-story-ch1.md section 7: 7.1 tree runtime, 7.2 linear panel; 10.3 elder tree; DECISIONS Q1:
// choice rewards are one-time per (npcId, nodeId, choiceIndex), saved as dialogueOnce; FIX Q11: UE renders the tree
// strings from data.dialogue.<treeId>.<nodeId>.text / .choice.<i>). Owner area: quests+story+pets.
#include "abyss/base/Platform.h"

#include "abyss/quests/Dialogue.h"

#include <algorithm>

#include "abyss/base/I18n.h"
#include "abyss/base/StrUtil.h"
#include "abyss/data/DataStore.h"
#include "abyss/hero/Hero.h"
#include "abyss/hero/Rewards.h"
#include "abyss/items/Inventory.h"
#include "abyss/items/LootGen.h"
#include "abyss/quests/QuestSystem.h"
#include "abyss/save/SaveData.h"
#include "abyss/sim/SimContext.h"
#include "abyss/sim/Snapshot.h"

namespace abyss {

namespace {

bool DlgContains(const std::vector<std::string>& v, std::string_view id) {
  return std::find(v.begin(), v.end(), id) != v.end();
}

// The quest of a choice is "already handled": active or turned_in (7.1 steps 2-3).
bool DlgQuestHandled(const QuestSystem* quests, std::string_view questId) {
  if (quests == nullptr || questId.empty()) return false;
  const QuestProgress* p = quests->Progress(questId);
  return p != nullptr && (p->status == QuestStatus::Active || p->status == QuestStatus::TurnedIn);
}

std::string DlgOnceKey(std::string_view npcId, std::string_view nodeId, int32_t choiceIndex) {
  return StrCat(npcId, "|", nodeId, "|", choiceIndex);
}

}  // namespace

DialogueSystem::DialogueSystem(SimContext& ctx) : ctx_(ctx) {}

void DialogueSystem::OpenTree(std::string_view npcId, const DialogueTree& tree) {
  view_ = DialogueView{};
  view_.open = true;
  view_.npcId = std::string(npcId);
  view_.tree = &tree;
  view_.nodeId = tree.startNodeId;
  if (ctx_.sys.quests != nullptr) view_.completedQuests = ctx_.sys.quests->TurnedInIds();  // 7.1 step 2
  ctx_.events.Emit(EvDialogue{EvDialogue::Kind::Opened, view_.npcId, tree.id, view_.nodeId});
  Render();
}

void DialogueSystem::OpenLinear(std::string_view npcId) {
  view_ = DialogueView{};
  view_.open = true;
  view_.linear = true;
  view_.npcId = std::string(npcId);
  ctx_.events.Emit(EvDialogue{EvDialogue::Kind::Opened, view_.npcId, std::string(), std::string()});
}

// 7.1 step 4. Leave, or a choice / Continue whose next node is missing, ends with Close() (EvDialogue{Closed}); the
// panel state is the open flag, so GameSim's modal diff emits EvPanelRequest{Dialogue, close} and the S2 freeze ends
// with it.
void DialogueSystem::Choose(int32_t buttonIndex) {
  if (!view_.open || view_.linear || view_.tree == nullptr) return;
  if (buttonIndex < 0 || static_cast<size_t>(buttonIndex) >= view_.buttons.size()) return;
  const DialogueTree& tree = *view_.tree;
  const DialogueNode* node = tree.FindNode(view_.nodeId);
  if (node == nullptr) {
    Close();
    return;
  }
  const DialogueButton button = view_.buttons[static_cast<size_t>(buttonIndex)];
  std::string next;
  switch (button.kind) {
    case DialogueButtonKind::Leave:
      Close();
      return;
    case DialogueButtonKind::Back:
      next = tree.startNodeId;
      break;
    case DialogueButtonKind::Continue:
      next = node->nextNodeId;
      break;
    case DialogueButtonKind::Choice: {
      if (button.choiceIndex < 0 || static_cast<size_t>(button.choiceIndex) >= node->choices.size()) return;
      const DialogueChoice& choice = node->choices[static_cast<size_t>(button.choiceIndex)];
      const std::string nodeId = node->id;  // the node stays valid (data), but copy for clarity across callbacks
      next = choice.nextNodeId;
      // Record the choice (choicesMade[nodeId] = next; an existing key keeps its position).
      DialogueNpcState* state = nullptr;
      for (DialogueNpcState& s : states_) {
        if (s.npcId == view_.npcId) state = &s;
      }
      if (state != nullptr) {
        bool found = false;
        for (auto& [k, v] : state->choicesMade) {
          if (k == nodeId) {
            v = next;
            found = true;
          }
        }
        if (!found) state->choicesMade.emplace_back(nodeId, next);
      }
      // Quest trigger: accept when there is no record or it failed (may fail silently; the dialogue still advances).
      // Dialogue-tree acceptances do not pin the guide.
      if (!choice.questTrigger.empty() && ctx_.sys.quests != nullptr) {
        const QuestProgress* p = ctx_.sys.quests->Progress(choice.questTrigger);
        if (p == nullptr || p->status == QuestStatus::Failed) ctx_.sys.quests->Accept(choice.questTrigger);
      }
      // Q1 FIX: the reward is paid once per (npcId, nodeId, choiceIndex) for the whole save.
      if (choice.reward.present) {
        const std::string key = DlgOnceKey(view_.npcId, nodeId, button.choiceIndex);
        if (!DlgContains(rewardOnce_, key)) {
          rewardOnce_.push_back(key);
          RewardService* rewards = ctx_.sys.rewards;
          if (choice.reward.gold != 0 && rewards != nullptr) {
            rewards->ChangeGold(choice.reward.gold, GoldReason::DialogueReward);
            ctx_.events.Log(MakeLoc("ui.dialogue.gotGold", {{"gold", ToStr(choice.reward.gold)}}), LogType::Loot);
          }
          if (choice.reward.exp != 0 && rewards != nullptr) {
            rewards->GrantExp(choice.reward.exp, ExpSource::DialogueReward);
            ctx_.events.Log(MakeLoc("ui.dialogue.gotExp", {{"exp", ToStr(choice.reward.exp)}}), LogType::Loot);
          }
          if (!choice.reward.items.empty() && rewards != nullptr && ctx_.sys.inventory != nullptr &&
              ctx_.sys.hero != nullptr) {
            const LootContext lc{&ctx_.data, &ctx_.Rand(RngStream::Loot), &ctx_.sys.inventory->Uids()};
            for (const std::string& baseId : choice.reward.items) {
              std::optional<ItemInstance> item = CreateItem(lc, baseId, ctx_.sys.hero->Level(), ItemQuality::Normal);
              if (!item.has_value()) continue;
              item->identified = true;
              const I18nArg name = ItemNameArg("name", *item, ctx_.data);
              rewards->GrantItem(*item, OverflowPolicy::Lose, ItemSource::DialogueReward);  // overflow lost (web)
              ctx_.events.Log(MakeLoc("ui.dialogue.gotItem", {name}), LogType::Loot);
            }
          }
        }
      }
      // A listener (quest accept) may have closed the panel.
      if (!view_.open || view_.tree != &tree) return;
      break;
    }
  }
  if (tree.FindNode(next) == nullptr) {
    Close();
    return;
  }
  view_.nodeId = next;
  ctx_.events.Emit(EvDialogue{EvDialogue::Kind::NodeChanged, view_.npcId, tree.id, view_.nodeId});
  Render();
}

void DialogueSystem::Close() {
  if (!view_.open) return;
  ctx_.events.Emit(EvDialogue{EvDialogue::Kind::Closed, view_.npcId, view_.tree ? view_.tree->id : std::string(),
                              view_.nodeId});
  view_ = DialogueView{};
}

// renderDialogueTreeNode (7.1 steps 1-3): visited record, visible choices, buttons.
void DialogueSystem::Render() {
  view_.buttons.clear();
  if (view_.tree == nullptr) return;
  const DialogueTree& tree = *view_.tree;
  const DialogueNode* node = tree.FindNode(view_.nodeId);
  if (node == nullptr) return;

  // 1. Visited (per NPC; record only, never read by any logic).
  DialogueNpcState* state = nullptr;
  for (DialogueNpcState& s : states_) {
    if (s.npcId == view_.npcId) state = &s;
  }
  if (state == nullptr) {
    states_.push_back(DialogueNpcState{view_.npcId, {}, {}});
    state = &states_.back();
  }
  if (!DlgContains(state->visitedNodes, node->id)) state->visitedNodes.push_back(node->id);

  // 2. Visible choices: prereqs all in completedQuests (turned in when the NPC was clicked); a quest choice whose quest
  // is active / turned_in is hidden only when it leads to an end node.
  const QuestSystem* quests = ctx_.sys.quests;
  int32_t visible = 0;
  for (size_t i = 0; i < node->choices.size(); ++i) {
    const DialogueChoice& c = node->choices[i];
    bool prereqs = true;
    for (const std::string& q : c.prereqQuests) prereqs = prereqs && DlgContains(view_.completedQuests, q);
    if (!prereqs) continue;
    const bool handled = DlgQuestHandled(quests, c.questTrigger);
    if (handled) {
      const DialogueNode* target = tree.FindNode(c.nextNodeId);
      if (target != nullptr && target->isEnd) continue;
    }
    view_.buttons.push_back(DialogueButton{DialogueButtonKind::Choice, static_cast<int32_t>(i), handled});
    ++visible;
  }

  // 3. Continue / Back / Leave.
  const bool allFiltered = !node->choices.empty() && visible == 0 && !node->isEnd;
  const bool backToRoot = allFiltered && node->id != tree.startNodeId;
  const bool hasEnd = node->isEnd || (visible == 0 && node->nextNodeId.empty() && !backToRoot);
  if (!node->nextNodeId.empty() && !node->isEnd && visible == 0 && !backToRoot) {
    view_.buttons.push_back(DialogueButton{DialogueButtonKind::Continue, -1, false});
  }
  if (backToRoot) view_.buttons.push_back(DialogueButton{DialogueButtonKind::Back, -1, false});
  if (hasEnd) view_.buttons.push_back(DialogueButton{DialogueButtonKind::Leave, -1, false});
}

void DialogueSystem::FillSnapshot(Snapshot& out) const { out.dialogue = &view_; }

void DialogueSystem::WriteSave(SaveData& out) const {
  out.dialogueState = states_;
  out.dialogueOnce = rewardOnce_;
}

void DialogueSystem::ReadSave(const SaveData& in) {
  states_ = in.dialogueState;
  rewardOnce_ = in.dialogueOnce;
}

}  // namespace abyss

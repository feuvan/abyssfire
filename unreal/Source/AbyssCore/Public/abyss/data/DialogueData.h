// Dialogue trees (NPC branching dialogue and linear mini-boss lines).
// Sources: dialogue_trees.json (trees.<id>, kind npc|miniBoss), minibosses.json (dialogues). Spec: quests-story-ch1.md
// 1.7, 7. Text fields are zh-CN fallbacks; the port renders i18n keys (Q11, M8):
//   NPC tree:  data.dialogue.<treeId>.<nodeId>.text and .choice.<i>
//   mini-boss: data.miniBossDialogue.<monsterId>.<nodeId>
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "abyss/base/Platform.h"

namespace abyss {

enum class DialogueKind : uint8_t { Npc, MiniBoss };

struct DialogueReward {
  bool present = false;
  int64_t gold = 0;
  int64_t exp = 0;
  std::vector<std::string> items;  // item base ids
};

struct DialogueChoice {
  std::string text;
  std::string nextNodeId;
  std::string questTrigger;  // empty = none
  std::vector<std::string> prereqQuests;
  DialogueReward reward;  // one-time per (npcId, nodeId, choiceIndex) (Q1)
};

struct DialogueNode {
  std::string id;
  std::string text;
  std::vector<DialogueChoice> choices;
  std::string nextNodeId;  // linear continuation (mini-boss)
  bool isEnd = false;
};

struct ABYSS_API DialogueTree {
  std::string id;  // tree id (NPC id or mini-boss monster id)
  DialogueKind kind = DialogueKind::Npc;
  std::string startNodeId;
  std::vector<DialogueNode> nodes;  // document order

  const DialogueNode* FindNode(std::string_view nodeId) const;
};

struct ABYSS_API DialogueTables {
  std::vector<DialogueTree> trees;
  const DialogueTree* Find(std::string_view id) const;
};

}  // namespace abyss

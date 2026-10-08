// Branching NPC dialogue runtime, linear dialogue and the mini-boss pre-fight panel contract.
// Spec: quests-story-ch1.md 7.1 (tree runtime: visible choices, buttons, choice effects), 7.2 (linear), 7.3 (mini-boss),
// 10.3 (elder tree); save-ui-input.md 3.2 (dialogueState), 7.10; DECISIONS Q1 (choice rewards are one-time per
// (npcId, nodeId, choiceIndex), saved as dialogueOnce in v4), U7 (dialogue is a modal panel).
//
// Owner area: quests+story+pets. Runtime system (SimContext).
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "abyss/base/Platform.h"
#include "abyss/data/DialogueData.h"

namespace abyss {

struct SimContext;
struct Snapshot;
struct SaveData;

enum class DialogueButtonKind : uint8_t { Choice, Continue, Back, Leave };

struct DialogueButton {
  DialogueButtonKind kind = DialogueButtonKind::Choice;
  int32_t choiceIndex = -1;  // index into the node's choices
  bool inProgress = false;   // ui.dialogue.inProgress suffix + ghost style
};

// Per-NPC record (saved, informational): visited nodes and choices made.
struct DialogueNpcState {
  std::string npcId;
  std::vector<std::string> visitedNodes;
  std::vector<std::pair<std::string, std::string>> choicesMade;  // nodeId -> nextNodeId
};

struct DialogueView {
  bool open = false;
  bool linear = false;        // 7.2 panel (text = dialogue[1] ?? [0])
  std::string npcId;
  const DialogueTree* tree = nullptr;
  std::string nodeId;
  std::vector<DialogueButton> buttons;
  std::vector<std::string> completedQuests;  // turned-in ids captured at open (7.1 step 2)
};

class ABYSS_API DialogueSystem {
 public:
  explicit DialogueSystem(SimContext& ctx);

  void OpenTree(std::string_view npcId, const DialogueTree& tree);
  void OpenLinear(std::string_view npcId);
  // A button press (7.1 step 4): choice effects (questTrigger accept, Q1 one-time reward), navigation or close.
  void Choose(int32_t buttonIndex);
  void Close();  // EvDialogue{Closed}
  const DialogueView& View() const { return view_; }

  void FillSnapshot(Snapshot& out) const;
  void WriteSave(SaveData& out) const;
  void ReadSave(const SaveData& in);

 private:
  void Render();

  SimContext& ctx_;
  DialogueView view_;
  std::vector<DialogueNpcState> states_;
  std::vector<std::string> rewardOnce_;  // Q1 keys "<npcId>|<nodeId>|<choiceIndex>"
};

}  // namespace abyss

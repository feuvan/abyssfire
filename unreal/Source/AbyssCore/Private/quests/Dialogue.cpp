// Dialogue runtime (quests-story-ch1.md section 7; Q1). STUB: owner area quests+story+pets. Opening (state + event),
// Close and the save plumbing are real; Render (buttons) and Choose are stubs.
#include "abyss/base/Platform.h"

#include "abyss/quests/Dialogue.h"

#include "abyss/base/Assert.h"
#include "abyss/quests/QuestSystem.h"
#include "abyss/save/SaveData.h"
#include "abyss/sim/SimContext.h"
#include "abyss/sim/Snapshot.h"

namespace abyss {

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

// Leave, or a choice whose next node is missing, ends with Close() (EvDialogue{Closed}); the panel state is the
// open flag, so GameSim's modal diff emits EvPanelRequest{Dialogue, close} and the S2 freeze ends with it.
void DialogueSystem::Choose(int32_t buttonIndex) { ABYSS_UNIMPLEMENTED(); }

void DialogueSystem::Close() {
  if (!view_.open) return;
  ctx_.events.Emit(EvDialogue{EvDialogue::Kind::Closed, view_.npcId, view_.tree ? view_.tree->id : std::string(),
                              view_.nodeId});
  view_ = DialogueView{};
}

void DialogueSystem::Render() { ABYSS_UNIMPLEMENTED(); }

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

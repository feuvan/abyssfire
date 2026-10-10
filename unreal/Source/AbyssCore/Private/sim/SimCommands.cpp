// Command routing and gates (ARCHITECTURE 3.1; D13 "input while frozen"; U7 modal input blocking; save-ui-input.md
// 5.1.1 Dying gate, C12). Owner: lead; each command calls exactly one area entry point.
#include "abyss/base/Platform.h"

#include "SimImpl.h"

#include <variant>

#include "abyss/base/Log.h"
#include "abyss/base/StrUtil.h"

namespace abyss {

namespace {

template <class... Fs>
struct Overloaded : Fs... {
  using Fs::operator()...;
};
template <class... Fs>
Overloaded(Fs...) -> Overloaded<Fs...>;

// Commands still accepted while the hero is Dying (5.1.1): the read-only information panels, the auto-combat
// preference, story, lifecycle and debug. DECISIONS C12 (binding, overrides 5.1.1's Esc row): Esc and menu actions are
// blocked during the death window - CmdOpenPanel{SystemMenu} is rejected; "return to menu" is only reachable after the
// respawn (or through CmdResolvePendingDeath / app background, which resolve the death first).
bool AllowedWhileDying(const Command& c) {
  if (const CmdOpenPanel* open = std::get_if<CmdOpenPanel>(&c)) {
    return !IsModalPanel(open->panel) && open->panel != PanelId::SystemMenu;
  }
  return std::holds_alternative<CmdClosePanel>(c) || std::holds_alternative<CmdToggleAutoCombat>(c) ||
         std::holds_alternative<CmdStoryAdvance>(c) || std::holds_alternative<CmdStorySkip>(c) ||
         std::holds_alternative<CmdSetTouchMode>(c) || std::holds_alternative<CmdAppBackground>(c) ||
         std::holds_alternative<CmdResolvePendingDeath>(c) || std::holds_alternative<CmdDebug>(c);
}

AutoLootMode NextAutoLoot(AutoLootMode m) {
  switch (m) {
    case AutoLootMode::Off: return AutoLootMode::All;
    case AutoLootMode::All: return AutoLootMode::Magic;
    case AutoLootMode::Magic: return AutoLootMode::Rare;
    case AutoLootMode::Rare: return AutoLootMode::Legendary;
    case AutoLootMode::Legendary: return AutoLootMode::Off;
  }
  return AutoLootMode::Off;
}

}  // namespace

bool IsHeroGameplayCommand(const Command& c) {
  return std::visit(Overloaded{
                        [](const CmdMoveTo&) { return true; },
                        [](const CmdSetMoveInput&) { return true; },
                        [](const CmdStop&) { return true; },
                        [](const CmdPointerPress&) { return true; },
                        [](const CmdPointerHold&) { return true; },
                        [](const CmdAttackTarget&) { return true; },
                        [](const CmdClearTarget&) { return true; },
                        [](const CmdCycleTarget&) { return true; },
                        [](const CmdCastSkill&) { return true; },
                        [](const CmdDodge&) { return true; },
                        [](const CmdUsePotion&) { return true; },
                        [](const CmdInteract&) { return true; },
                        [](const CmdPickUp&) { return true; },
                        [](const CmdTownPortal&) { return true; },
                        // CmdPuzzleAnswer is a modal answer (allowed while its own prompt is open), not hero input.
                        [](const auto&) { return false; },
                    },
                    c);
}

void SimImpl::ApplyCommands(bool frozen) {
  if (pending.empty()) return;
  std::vector<Command> batch;
  batch.swap(pending);
  for (const Command& c : batch) {
    if (!hasSession) break;
    if (IsHeroGameplayCommand(c) && (frozen || InputBlocked())) continue;  // D13 / U7: rejected, not deferred
    if (hero->Life() == HeroLife::Dying && !AllowedWhileDying(c)) continue;  // 5.1.1
    ApplyCommand(c);
  }
}

void SimImpl::ApplyCommand(const Command& cmd) {
  std::visit(
      Overloaded{
          // ---- movement / pointer ----
          [this](const CmdMoveTo& c) {
            combat->ClearAttackTarget();
            locomotion->MoveTo(c.tile);
          },
          [this](const CmdSetMoveInput& c) {
            locomotion->SetMoveInput(c.screenSpace ? ScreenDirToTile(c.dir) : c.dir);
          },
          [this](const CmdStop&) { locomotion->Stop(); },
          [this](const CmdPointerPress& c) { zone->OnPointerPress(c.tile, c.button, c.pointerId); },
          [this](const CmdPointerHold& c) { locomotion->UpdateHold(c.pointerId, RoundToTile(c.tile), c.down); },
          // ---- combat ----
          [this](const CmdAttackTarget& c) { combat->SetAttackTarget(c.target, true); },
          [this](const CmdClearTarget&) { combat->ClearAttackTarget(); },
          [this](const CmdCycleTarget&) { combat->CycleTarget(); },
          [this](const CmdCastSkill& c) {
            SkillAim aim;
            aim.target = c.target;
            aim.hasPoint = c.hasPoint;
            aim.point = c.point;
            aim.stickDir = ScreenDirToTile(c.stickDir);
            combat->RequestSkillSlot(c.slot, aim);
          },
          [this](const CmdDodge& c) { combat->RequestDodge(ScreenDirToTile(c.dir)); },
          [this](const CmdToggleAutoCombat&) { combat->SetAutoCombat(!hero->autoCombat); },
          [this](const CmdCycleAutoLoot&) { hero->autoLoot = NextAutoLoot(hero->autoLoot); },
          [this](const CmdUsePotion& c) { inventory->UsePotionSlot(c.slot); },
          // ---- world ----
          [this](const CmdInteract& c) {
            if (c.target == kNoEntity) {
              zone->Interact();
            } else {
              zone->InteractWith(c.target);
            }
          },
          // W8 / Q22: walk-then-act - in range it picks up at once, else the hero walks and picks up on arrival.
          [this](const CmdPickUp& c) { zone->InteractWith(c.drop); },
          [this](const CmdTownPortal&) { zone->UseTownPortal(); },
          [this](const CmdPuzzleAnswer& c) { randomEvents->AnswerPuzzle(c.prop, c.choice); },
          // ---- items ----
          [this](const CmdEquip& c) { inventory->Equip(c.uid); },
          [this](const CmdUnequip& c) { inventory->Unequip(c.slot); },
          [this](const CmdUseItem& c) { inventory->UseItem(c.uid); },
          [this](const CmdDiscardItem& c) { inventory->Discard(c.uid); },
          [this](const CmdDestroyNormals&) { inventory->DestroyNormals(); },
          [this](const CmdSortBag&) { inventory->SortBag(); },
          [this](const CmdSortStash&) { inventory->SortStash(); },
          [this](const CmdSocketGem& c) { inventory->SocketGem(c.slot, c.gemUid); },
          [this](const CmdUnsocketGem& c) { inventory->UnsocketGem(c.slot, c.index); },
          [this](const CmdBuy& c) { shop->Buy(c.wareIndex); },
          [this](const CmdSell& c) { shop->Sell(c.uid); },
          [this](const CmdBuyback& c) { shop->Buyback(c.index); },
          [this](const CmdStashPut& c) { inventory->StashPut(c.uid); },
          [this](const CmdStashTake& c) { inventory->StashTake(c.uid); },
          [this](const CmdCraft& c) { shop->Craft(c.action, c.uid); },
          [this](const CmdSetPotionSlot& c) { inventory->SetPotionSlot(c.slot, c.baseId); },
          // ---- hero progression ----
          [this](const CmdLearnSkill& c) { combat->LearnSkill(c.skillIndex); },
          [this](const CmdAllocStat& c) { combat->AllocateStat(c.stat, c.points); },
          [this](const CmdSetHotbar& c) { combat->SetHotbar(c.slot, c.skillIndex); },
          // ---- pets ----
          [this](const CmdSetActivePet& c) { pets->SetActivePet(c.petId); },
          [this](const CmdFeedPet& c) { petCompanion->FeedFromBag(c.petId); },
          // ---- quests / dialogue / story ----
          [this](const CmdQuestAccept& c) { questWorld->AcceptFromCard(c.questId); },
          [this](const CmdQuestTurnIn& c) { questWorld->TurnIn(c.questId, c.choice); },
          [this](const CmdQuestTrack& c) { quests->SetTracked(c.questId); },
          [this](const CmdQuestCardClose&) { questWorld->CloseCard(); },
          [this](const CmdQuestCardOpen& c) { questWorld->OpenCard(c.npcId); },
          [this](const CmdDialogueChoose& c) { dialogue->Choose(c.button); },
          [this](const CmdDialogueClose&) { dialogue->Close(); },
          [this](const CmdDialogueOpenTree& c) { questWorld->OpenDialogueTree(c.npcId); },
          [this](const CmdMiniBossDialogueDismiss&) { monsters->DismissMiniBossDialogue(); },
          [this](const CmdStoryAdvance&) { story->Advance(); },
          [this](const CmdStorySkip&) { story->Skip(); },
          // ---- UI / flow ----
          [this](const CmdOpenPanel& c) {
            if (IsCoreOwnedPanel(c.panel)) {  // opened by the core only (SimTypes.h ownership rules)
              LogWarning(StrCat("CmdOpenPanel: ", EnumName(c.panel), " is core-owned (ignored)"));
              return;
            }
            if (!session.panels.IsOpen(c.panel)) session.panels.open.push_back(c.panel);
          },
          [this](const CmdClosePanel& c) {
            if (IsCoreOwnedPanel(c.panel)) {  // UE close request -> the owner's Close() clears the state
              CloseCoreModal(c.panel);
              return;
            }
            auto& open = session.panels.open;
            for (size_t i = 0; i < open.size(); ++i) {
              if (open[i] == c.panel) {
                open.erase(open.begin() + static_cast<std::ptrdiff_t>(i));
                break;
              }
            }
          },
          [this](const CmdSetTouchMode& c) { session.touchMode = c.touch; },
          [this](const CmdAppBackground& c) {
            session.appBackground = c.background;
            if (c.background) {
              combat->ResolvePendingDeath();
              RequestSave(SaveReason::AppBackground);
            }
          },
          [this](const CmdResolvePendingDeath&) { combat->ResolvePendingDeath(); },
          [this](const CmdDebug& c) {
            if (!config.enableDebugCommands) return;
            if (c.op == "freeze") {
              clock.SetFrozen(FreezeReason::Debug, true);
            } else if (c.op == "unfreeze") {
              clock.SetFrozen(FreezeReason::Debug, false);
            } else if (c.op == "teleport") {
              locomotion->Teleport(c.pos, TeleportReason::Debug);
            } else if (c.op == "addGold") {
              rewards->ChangeGold(static_cast<int64_t>(c.value), GoldReason::Debug);
            } else if (c.op == "addExp") {
              rewards->GrantExp(static_cast<int64_t>(c.value), ExpSource::Debug);
            } else {
              LogWarning("CmdDebug: unknown op " + c.op);
            }
          },
      },
      cmd);
}

}  // namespace abyss

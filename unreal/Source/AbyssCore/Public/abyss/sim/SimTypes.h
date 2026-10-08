// Small enums shared by Commands, Events, Snapshot and several areas (the presentation contract vocabulary).
#pragma once

#include <cstdint>

#include "abyss/base/Enums.h"
#include "abyss/base/Platform.h"

namespace abyss {

// UI panels the core needs to know about (U7 freezes / modal input blocking, save-ui-input.md 7).
//
// Ownership of the open state (binding for UE):
// * CORE-OWNED modals (IsCoreOwnedPanel): Dialogue, QuestCard, Shop, Forge (= the blacksmith's shop), Stash,
//   MiniBossDialogue, LoreText, Puzzle. The owning system opens them itself (NPC interaction, mini-boss approach, lore
//   pickup, puzzle prop) and its state is the truth: DialogueSystem::View().open, QuestWorld::Card().open,
//   ShopSystem::State().open, InventorySystem::StashSession().open, MonsterSystem::MiniBossDialogueActive(),
//   LoreSystem::Text().open, RandomEventSystem::Puzzle().open. GameSim derives the freeze (S2: dialogue / quest card)
//   and the input block (U7) from that state and emits EvPanelRequest{open/close} whenever it changes. UE never sends
//   CmdOpenPanel for them (ignored); a UE close (Esc, close button, backdrop) sends CmdClosePanel, which calls the
//   owner's Close(). ExitZone and the hero's death close all of them.
// * UE-OWNED panels: the HUD panels (inventory .. pets; pause on touch, U7), SystemMenu (U4, freezes), Socket and
//   Confirm (modal, block input). UE reports them with CmdOpenPanel / CmdClosePanel (SessionState::panels).
enum class PanelId : uint8_t {
  Inventory,
  Character,
  Skills,
  QuestLog,
  WorldMap,
  Achievements,
  Settings,
  SystemMenu,  // U4 (pause menu: freezes)
  Pets,
  // modal panels (block gameplay input, U7; dialogue + quest card also freeze, S2)
  Dialogue,
  QuestCard,
  Shop,
  Stash,
  Forge,
  Socket,
  MiniBossDialogue,
  LoreText,
  Confirm,
  Puzzle,  // environmental puzzle prompt (world 13.3): 2 buttons, CmdPuzzleAnswer
};
ABYSS_ENUM_STRINGS(PanelId, "inventory", "character", "skills", "questLog", "worldMap", "achievements", "settings",
                   "systemMenu", "pets", "dialogue", "questCard", "shop", "stash", "forge", "socket",
                   "miniBossDialogue", "loreText", "confirm", "puzzle")

constexpr bool IsModalPanel(PanelId p) { return static_cast<uint8_t>(p) >= static_cast<uint8_t>(PanelId::Dialogue); }

// Panels whose open state is owned by a core system (see above).
constexpr bool IsCoreOwnedPanel(PanelId p) {
  switch (p) {
    case PanelId::Dialogue:
    case PanelId::QuestCard:
    case PanelId::Shop:
    case PanelId::Forge:
    case PanelId::Stash:
    case PanelId::MiniBossDialogue:
    case PanelId::LoreText:
    case PanelId::Puzzle:
      return true;
    default:
      return false;
  }
}

// Combat-log line types (save-ui-input.md 6.10).
enum class LogType : uint8_t { System, Combat, Loot, Info, Quest };
ABYSS_ENUM_STRINGS(LogType, "system", "combat", "loot", "info", "quest")

// Animation actions requested from the presentation (P5: timing owned by the core).
enum class AnimAction : uint8_t { Idle, Walk, Attack, Cast, Hurt, Dodge, Death, Signature, Work, Talk };
ABYSS_ENUM_STRINGS(AnimAction, "idle", "walk", "attack", "cast", "hurt", "dodge", "death", "signature", "work", "talk")

enum class DespawnReason : uint8_t { Died, Removed, ZoneUnload, PickedUp, Expired, Collected };
ABYSS_ENUM_STRINGS(DespawnReason, "died", "removed", "zoneUnload", "pickedUp", "expired", "collected")

enum class TeleportReason : uint8_t { Skill, Dodge, TownPortal, Respawn, Load, ZoneEntry, EliteBlink, CatchUp, Debug };
ABYSS_ENUM_STRINGS(TeleportReason, "skill", "dodge", "townPortal", "respawn", "load", "zoneEntry", "eliteBlink",
                   "catchUp", "debug")

enum class ProjectileKind : uint8_t { HeroSkill, MonsterBolt, PetBolt };
ABYSS_ENUM_STRINGS(ProjectileKind, "heroSkill", "monsterBolt", "petBolt")

enum class FloatingTextKind : uint8_t { MonsterDamage, HeroDamage, Miss, Heal, Exp, Gold, Embers, Status, Custom };
ABYSS_ENUM_STRINGS(FloatingTextKind, "monsterDamage", "heroDamage", "miss", "heal", "exp", "gold", "embers", "status",
                   "custom")

// World objects the interact action / prompt can target (world-map-nav.md 7.4).
enum class InteractKind : uint8_t { None, Loot, Npc, TowerObject, SubDungeon, LabyrinthPortal, HiddenReward, EventPuzzle };
ABYSS_ENUM_STRINGS(InteractKind, "none", "loot", "npc", "towerObject", "subDungeon", "labyrinthPortal", "hiddenReward",
                   "eventPuzzle")

// Story beats (quests-story-ch1.md 8.2).
enum class StoryBeatKind : uint8_t { Sequence, Chapter, Cutscene, BossIntro };
ABYSS_ENUM_STRINGS(StoryBeatKind, "sequence", "chapter", "cutscene", "bossIntro")

// HUD potion quick slots (I4).
enum class PotionSlot : uint8_t { Hp, Mp };
ABYSS_ENUM_STRINGS(PotionSlot, "hp", "mp")

// Why the town portal cannot be used now (world 9.4 + W3; ZoneRuntime::CanUseTownPortal, HeroView::portalRefusal: the
// touch portal button is dimmed unless None, world 7.4).
enum class PortalRefusal : uint8_t { None, Dead, Busy, NoDestination, AlreadyAtCamp, AlreadyAtExit };
ABYSS_ENUM_STRINGS(PortalRefusal, "none", "dead", "busy", "noDestination", "alreadyAtCamp", "alreadyAtExit")

// Why gold changed (EvGoldChanged::reason; RewardService::ChangeGold / SpendGold).
enum class GoldReason : uint8_t {
  Kill,            // monster kill gold
  QuestReward,     // quest turn-in
  DialogueReward,  // Q1 one-time dialogue choice reward
  RandomEvent,     // puzzle / rescue reward (world 13.3)
  HiddenReward,    // hidden-area gold pile (Q5)
  SoulEchoClaim,   // reclaimed soul echo
  DeathPenalty,    // soul echo toll (negative)
  ShopBuy,
  ShopSell,
  ShopBuyback,
  Craft,           // blacksmith forge
  Homestead,       // building upgrades (later milestone)
  Debug,
};
ABYSS_ENUM_STRINGS(GoldReason, "kill", "questReward", "dialogueReward", "randomEvent", "hiddenReward", "soulEchoClaim",
                   "deathPenalty", "shopBuy", "shopSell", "shopBuyback", "craft", "homestead", "debug")

// Player transactions (refused while the hero is Dying, save-ui-input 5.1.1); passive credits still apply.
constexpr bool IsPlayerGoldTransaction(GoldReason r) {
  return r == GoldReason::ShopBuy || r == GoldReason::ShopSell || r == GoldReason::ShopBuyback ||
         r == GoldReason::Craft || r == GoldReason::Homestead;
}

// Where exp came from (EvExpGained::source; RewardService::GrantExp).
enum class ExpSource : uint8_t { Kill, Quest, DialogueReward, RandomEvent, SoulEchoClaim, DeathPenalty, Debug };
ABYSS_ENUM_STRINGS(ExpSource, "kill", "quest", "dialogueReward", "randomEvent", "soulEchoClaim", "deathPenalty",
                   "debug")

// What happens to a granted item when the bag is full (RewardService::GrantItem).
//   Lose:   the item is lost (log sys.inventory.bagFull) - web behaviour for loose grants.
//   Stash:  overflow goes to the stash ignoring its capacity (quest turn-in I10, hidden chest Q5).
//   Refuse: nothing happens and the caller keeps the item (purchases refuse before paying, FIX Q6).
enum class OverflowPolicy : uint8_t { Lose, Stash, Refuse };
ABYSS_ENUM_STRINGS(OverflowPolicy, "lose", "stash", "refuse")

// Where a granted item came from (logs, achievements 'collect', ItemPickedMsg for pickups).
enum class ItemSource : uint8_t { Pickup, QuestReward, DialogueReward, HiddenReward, RandomEvent, Shop, Craft, Debug };
ABYSS_ENUM_STRINGS(ItemSource, "pickup", "questReward", "dialogueReward", "hiddenReward", "randomEvent", "shop", "craft",
                   "debug")

// Floating damage number placement of a hit (combat-feel 4.2 items 10-11): the primary hit, the critDoubleStrike extra
// hit (number 20 px higher) and the doubleShot extra arrow (number offset +15, -15 px). Screen px at render scale 1.
enum class HitNumberSlot : uint8_t { Primary, DoubleStrike, DoubleShot };
ABYSS_ENUM_STRINGS(HitNumberSlot, "primary", "doubleStrike", "doubleShot")

// Pointer buttons for world clicks (world-map-nav.md 7.1).
enum class PointerButton : uint8_t { Primary, Secondary };

// Banners / toasts (save-ui-input.md 6.13).
enum class BannerKind : uint8_t { Zone, QuestComplete, LevelUp, Death, Discovery, Toast, Achievement, ComingSoon };
ABYSS_ENUM_STRINGS(BannerKind, "zone", "questComplete", "levelUp", "death", "discovery", "toast", "achievement",
                   "comingSoon")

}  // namespace abyss

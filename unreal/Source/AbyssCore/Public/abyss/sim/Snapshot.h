// Read-only state for rendering and UI (ARCHITECTURE 3.1 "Snapshot").
//
// * Rebuilt by GameSim at the end of every Step / Frame / command batch (each runtime system fills its part through
//   FillSnapshot). Small per-entity views are copies; large containers (bag, quests, achievements, exploration grid)
//   are const pointers into the live systems, valid until the next call into GameSim.
// * Interpolation (S1): views carry prevPos (position before the last step) and Snapshot::interpolationAlpha =
//   accumulator / kSimStepMs, so UE renders Lerp(prevPos, pos, alpha).
// * Coordinates are tile space (UE maps col/row to X/Y with Units.h TileToWorld, S4).
//
// Owner: lead (shared contract). Each area fills its own views; adding a field = coordinate in review.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/Platform.h"
#include "abyss/base/Types.h"
#include "abyss/combat/CombatInput.h"
#include "abyss/data/ItemData.h"
#include "abyss/data/MapData.h"
#include "abyss/hero/Hero.h"
#include "abyss/items/Inventory.h"
#include "abyss/monsters/Monster.h"
#include "abyss/quests/QuestGuide.h"
#include "abyss/quests/QuestSystem.h"
#include "abyss/sim/SimTypes.h"

namespace abyss {

class Inventory;
class QuestSystem;
class PetSystem;
class AchievementState;
class ExplorationGrid;
class SoulEchoState;
class HomesteadTower;
class ZoneGrid;
class LoreSystem;
struct ShopState;
struct QuestCardState;
struct DialogueView;
struct StoryPlayback;
struct EscortState;
struct DefendState;
struct PetCompanionState;
struct ActiveRandomEvent;
struct PuzzlePromptState;
struct LoreTextState;

struct SkillSlotView {
  int32_t skillIndex = -1;  // -1 = empty slot
  std::string skillId;
  int32_t level = 0;
  double cooldownRemainingMs = 0;
  double cooldownTotalMs = 0;
  int32_t manaCost = 0;
  bool affordable = false;
  bool usable = false;  // CanExecuteSkill now
};

struct HeroView {
  ClassId cls = ClassId::Warrior;
  Vec2 pos, prevPos;
  Vec2 facing{1, 0};
  bool moving = false;
  double speedTilesPerSec = 0;
  int32_t level = 1;
  int64_t exp = 0, expToNext = 0, gold = 0;
  double hp = 0, maxHp = 0, mana = 0, maxMana = 0;
  HeroDerived derived;
  PrimaryStats baseStats;
  int32_t freeStatPoints = 0, freeSkillPoints = 0;
  HeroLife life = HeroLife::Alive;
  double spirit = 0, spiritMax = 100;
  bool resonating = false;
  double resonanceRemainingMs = 0;
  uint32_t statusMask = 0;
  std::array<SkillSlotView, 6> hotbar{};
  double dodgeCooldownRemainingMs = 0, dodgeCooldownMs = 0;
  bool invulnerable = false;
  bool autoCombat = false;
  AutoLootMode autoLoot = AutoLootMode::Off;
  EntityId target = kNoEntity;     // attack lock
  EntityId indicator = kNoEntity;  // ring under (lock, else nearest aggro) (combat 9.5)
  bool inCombat = false;
  bool portaling = false;
  double portalProgress = 0;       // 0..1 of the 1.5 s channel
  PortalRefusal portalRefusal = PortalRefusal::None;  // CanUseTownPortal now (touch button dim rule, world 7.4)
  bool dashing = false;            // C4 Charge dash in progress (HeroLocomotion::IsDashing)
  bool lowHp = false;              // danger vignette (combat 11.7)
};

struct MonsterView {
  EntityId id = kNoEntity;
  std::string defId;
  std::string artId;    // spriteKey (asset game id)
  std::string nameKey;  // data.monster.<id> or the story boss name once renamed
  bool storyNamed = false;
  Vec2 pos, prevPos;
  Vec2 heading{1, 0};
  double speedTilesPerSec = 0;
  MonsterState state = MonsterState::Idle;
  MonsterRole role = MonsterRole::Regular;
  bool alive = true;
  double hp = 0, maxHp = 0;
  bool elite = false;
  bool miniBoss = false;
  std::vector<EliteAffixType> affixes;
  uint32_t statusMask = 0;
  double visualScale = 1;
  bool windingUp = false;  // telegraph tint (combat 10.3)
};

struct NpcView {
  EntityId id = kNoEntity;
  std::string npcId;
  std::string artId;
  Vec2 pos;
  Vec2 facing{0, 1};
  NpcMarker marker = NpcMarker::None;
  bool heroNear = false;  // within 3 tiles: alert / face the hero (quests 6.4)
  bool talking = false;
};

struct PetView {
  bool present = false;
  EntityId id = kNoEntity;
  std::string petId;
  int32_t stage = 0;
  Vec2 pos, prevPos;
  Vec2 facing{1, 0};
  double hp = 0, maxHp = 0;
  bool exhausted = false;
  EntityId target = kNoEntity;
};

struct GroundItemView {
  EntityId id = kNoEntity;
  std::string uid;
  std::string baseId;
  ItemQuality quality = ItemQuality::Normal;
  int32_t quantity = 1;
  Vec2 pos;
  Vec2 visualOffset;
};

struct PotionDropView {
  EntityId id = kNoEntity;
  PotionKind kind = PotionKind::Hp;
  int32_t amount = 0;
  Vec2 pos;
};

struct ProjectileView {
  EntityId id = kNoEntity;
  ProjectileKind kind = ProjectileKind::HeroSkill;
  EntityId target = kNoEntity;
  Vec2 from, to;
  double progress = 0;  // 0..1
  std::string vfxId;
  uint32_t color = 0;
};

struct GroundEffectView {
  EntityId id = kNoEntity;
  std::string skillId, vfxId;
  Vec2 center;
  double radius = 0;
  double progress = 0;
};

// Static or quest-driven world props the UE layer spawns actors for.
enum class MarkerKind : uint8_t {
  GatherNode,
  Clue,
  QuestItemPickup,
  LorePickup,
  HiddenReward,
  SoulEcho,
  EscortNpc,
  DefendTarget,
  EventProp,
  StoryDecoration,
  Exit,
  Camp,
};
ABYSS_ENUM_STRINGS(MarkerKind, "gatherNode", "clue", "questItemPickup", "lorePickup", "hiddenReward", "soulEcho",
                   "escortNpc", "defendTarget", "eventProp", "storyDecoration", "exit", "camp")

struct WorldMarkerView {
  EntityId id = kNoEntity;  // kNoEntity for static markers (exits, camps, story decor)
  MarkerKind kind = MarkerKind::GatherNode;
  Vec2 pos;
  std::string key;          // item kind / lore id / area id / exit target map / decor id
  bool sealed = false;      // exits (W7)
  bool armed = true;        // exits (W8)
  double hp = 0, maxHp = 0; // escort / defend
};

struct ZoneView {
  std::string mapId;
  std::string nameKey;
  int32_t cols = 0, rows = 0;
  MapTheme theme = MapTheme::Plains;
  int32_t levelMin = 1, levelMax = 1;
  double safeZoneRadius = 9;
  std::vector<Vec2> camps;
  const ZoneGrid* grid = nullptr;  // tiles + walkability + decorations (render data)
};

struct BossBarView {
  bool show = false;
  EntityId monster = kNoEntity;
  std::string nameKey, epithetKey;
  double hp = 0, maxHp = 0;
};

struct InteractPromptView {
  InteractKind kind = InteractKind::None;
  EntityId id = kNoEntity;
  Vec2 pos;
};

struct ABYSS_API Snapshot {
  // ---- clock / flow ----
  double simNowMs = 0;
  uint64_t steps = 0;
  double interpolationAlpha = 0;
  bool frozen = false;
  uint32_t freezeMask = 0;
  bool cinematic = false;
  bool storyBusy = false;
  bool miniBossDialogue = false;
  bool inputBlocked = false;    // hero gameplay commands are rejected now (frozen or a modal is open, U7)
  std::vector<PanelId> modals;  // core-owned modal panels open now, PanelId order (SimTypes.h ownership rules)
  Difficulty difficulty = Difficulty::Normal;
  double playTimeMs = 0;
  bool canSave = false;
  // ---- entities ----
  HeroView hero;
  std::vector<MonsterView> monsters;
  std::vector<NpcView> npcs;
  PetView pet;
  std::vector<GroundItemView> groundItems;
  std::vector<PotionDropView> potions;
  std::vector<ProjectileView> projectiles;
  std::vector<GroundEffectView> groundEffects;
  std::vector<WorldMarkerView> markers;
  // ---- zone / HUD ----
  ZoneView zone;
  BossBarView bossBar;
  InteractPromptView prompt;
  GuideTarget guide;
  // ---- inventory HUD ----
  int32_t stashCapacity = 0;                    // 80 + homestead stashSlots (InventorySystem::StashCapacity)
  std::array<PotionSlotView, 2> potionSlots{};  // I4 quick slots, PotionSlot order (Hp, Mp)
  // ---- live views (valid until the next GameSim call) ----
  const SkillBook* skills = nullptr;
  const BuffList* heroBuffs = nullptr;
  const Inventory* inventory = nullptr;  // bag, Equipment() (tooltip compare), stash items
  const StashSession* stash = nullptr;   // stash panel session (I7)
  const ShopState* shop = nullptr;
  const QuestSystem* quests = nullptr;
  const QuestCardState* questCard = nullptr;
  const DialogueView* dialogue = nullptr;
  const StoryPlayback* story = nullptr;
  const PetSystem* pets = nullptr;
  const AchievementState* achievements = nullptr;
  const ExplorationGrid* exploration = nullptr;
  const SoulEchoState* soulEcho = nullptr;
  const HomesteadTower* homestead = nullptr;
  const EscortState* escort = nullptr;
  const DefendState* defend = nullptr;
  const LoreSystem* lore = nullptr;  // collected lore ids (quest log lore tab, save 7.5), discovered areas, hidden rewards
  const LoreTextState* loreText = nullptr;
  const std::vector<ActiveRandomEvent>* randomEvents = nullptr;  // active world events of the zone
  const PuzzlePromptState* puzzle = nullptr;

  void ClearDynamic();  // clears the per-step vectors (keeps capacity)
};

}  // namespace abyss

// Core -> presentation events (ARCHITECTURE 3.1). Plain structs in one std::variant.
//
// * Events carry everything the UE layer needs to play animations, VFX, audio and UI feedback; UE never writes core
//   state. Continuous state (positions, HP, cooldowns, inventories) is read from the Snapshot instead.
// * All player-facing text is a LocText (i18n key + args); UE renders it with I18n::T.
// * Times: `*Ms` fields that are absolute are SimClock ms (sim time); durations are ms.
// * Subsystems emit through EventSink (appends in emission order; GameSim exposes the list after a step/frame).
#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/I18n.h"
#include "abyss/base/Platform.h"
#include "abyss/base/Types.h"
#include "abyss/data/AudioData.h"
#include "abyss/data/CombatData.h"
#include "abyss/data/ItemData.h"
#include "abyss/data/MapData.h"
#include "abyss/data/SkillData.h"
#include "abyss/data/StoryData.h"
#include "abyss/sim/SimTypes.h"

namespace abyss {

// ---- entities ----
struct EvEntitySpawned {
  EntityId id = kNoEntity;
  EntityKind kind = EntityKind::None;
  std::string defId;   // monster def id / npc id / pet id / item base id / prop type
  std::string artId;   // asset game id (spriteKey, npc spriteId, ...)
  Vec2 pos;
  Vec2 facing{1, 0};
  double visualScale = 1.0;
};
struct EvEntityDespawned {
  EntityId id = kNoEntity;
  EntityKind kind = EntityKind::None;
  DespawnReason reason = DespawnReason::Removed;
};
struct EvEntityTeleported {
  EntityId id = kNoEntity;
  Vec2 from, to;
  TeleportReason reason = TeleportReason::Skill;
};
// Play an action on an entity. contactMs (attack) / releaseMs (cast) is relative to startMs; the montage should be
// played at a rate that puts its authored Contact/Release notify there (P5: the core owns the timing).
struct EvPlayAnim {
  EntityId entity = kNoEntity;
  AnimAction action = AnimAction::Idle;
  std::string clip;        // optional explicit clip (signature montages: skill id)
  double startMs = 0;      // sim time
  double contactMs = 0;    // attack contact / cast release offset (0 = none)
  double windupMs = 0;     // monster telegraph length (0.62 * contact)
  double durationMs = 0;   // whole action at play rate 1 scaled by speed
  double playRate = 1.0;
  bool hasFaceTarget = false;
  Vec2 faceTarget;
};
// A resolved hit (or miss). The HitFeedback profile is included (combat-feel.md 11) together with the RESOLVED 11.1
// decisions, so UE applies feel without re-implementing any rule:
//   attackerStopMs  hit-stop on the attacker: hero basic attack = profile.attackerStopMs; hero skills = 0; monster
//                   swing on the hero = round(profile.attackerStopMs x 0.6); DoT ticks / pets / environment = 0.
//   impactBurst     play VFXManager.impactBurst (colour impactColor): hero basic attacks and skills except death_mark,
//                   slow_trap and DoT ticks; never on misses.
//   melee           monster -> hero hit from a melee swing (the claw-rake VFX is melee only); false for bolts.
//   numberSlot      floating-number placement (Primary / DoubleStrike 20 px higher / DoubleShot offset +15, -15).
struct EvHit {
  EntityId target = kNoEntity;
  EntityId source = kNoEntity;
  Faction targetFaction = Faction::Monster;
  double amount = 0;
  HitWeight weight = HitWeight::Normal;
  HitProfileDef profile;
  bool crit = false;
  bool dodged = false;       // MISS
  bool tick = false;         // DoT tick
  bool killed = false;
  bool iframeAvoided = false;  // hero dodge-roll i-frames ("perfect evade")
  DamageType element = DamageType::Physical;
  uint32_t impactColor = 0;
  bool hasFrom = false;
  Vec2 from;
  double targetHp = 0, targetMaxHp = 0;
  std::string skillId;       // empty for basic attacks / monster hits
  double attackerStopMs = 0;
  bool impactBurst = false;
  bool melee = false;
  HitNumberSlot numberSlot = HitNumberSlot::Primary;
};
struct EvMonsterAttackCancelled {
  EntityId monster = kNoEntity;
  bool projectile = false;  // false = contact
};
struct EvProjectileLaunched {
  EntityId projectile = kNoEntity;
  ProjectileKind kind = ProjectileKind::HeroSkill;
  EntityId source = kNoEntity, target = kNoEntity;
  Vec2 from, to;
  double launchMs = 0, travelMs = 0;
  std::string vfxId;
  uint32_t color = 0;
};
struct EvProjectileEnded {
  EntityId projectile = kNoEntity;
  bool hit = true;  // false = fizzled / destroyed (F2)
};
struct EvGroundEffectStarted {
  EntityId id = kNoEntity;
  std::string skillId;
  std::string vfxId;
  Vec2 center;
  double radius = 0;
  double startMs = 0, durationMs = 0;
  GroundTrigger trigger = GroundTrigger::Periodic;  // Armed = a trap waiting for a monster (C4)
  int32_t ticks = 1;
};
// An armed trap fired (C4, GroundTrigger::Armed): play the trigger VFX; ticks follow.
struct EvGroundEffectTriggered {
  EntityId id = kNoEntity;
  double triggerMs = 0;
};
struct EvGroundEffectEnded {
  EntityId id = kNoEntity;
  bool triggered = true;  // false = an armed trap expired without firing
};
// Skill VFX that is not a projectile or ground effect (slash arcs, buffs, blinks, chain points...).
struct EvSkillVfx {
  std::string skillId;
  std::string vfxId;
  EntityId caster = kNoEntity;
  EntityId target = kNoEntity;
  Vec2 origin, point;
  double radius = 0;
  std::vector<Vec2> points;  // chain lightning / multishot target points (in order)
  double staggerMs = 0;      // per-point delay (C4 chain 55 ms)
};
struct EvStatusApplied {
  EntityId target = kNoEntity;
  StatusType type = StatusType::Burn;
  double value = 0, durationMs = 0;
  bool refreshed = false;
};
struct EvStatusExpired {
  EntityId target = kNoEntity;
  StatusType type = StatusType::Burn;
};
struct EvFloatingText {
  FloatingTextKind kind = FloatingTextKind::Custom;
  EntityId anchor = kNoEntity;  // or kNoEntity with pos
  Vec2 pos;
  double value = 0;
  bool crit = false;
  DamageType element = DamageType::Physical;
  LocText text;  // Custom / Status
};

// ---- hero ----
// PLAYER_EXP_CHANGED (every RewardService::GrantExp / RemoveExp; amount < 0 for the death toll).
struct EvExpGained {
  int64_t amount = 0;
  int64_t exp = 0;
  int64_t expToNext = 0;
  ExpSource source = ExpSource::Kill;
};
struct EvLevelUp {
  int32_t level = 1;
};
struct EvGoldChanged {
  int64_t gold = 0;   // new total
  int64_t delta = 0;  // applied change
  GoldReason reason = GoldReason::Kill;
};
struct EvSpiritChanged {
  double value = 0, maxValue = 100;
  bool resonating = false;
  double gained = 0;
};
struct EvResonance {
  bool started = true;  // false = ended
  std::string profileId;
  double durationMs = 0;
};
struct EvSkillUsed {
  std::string skillId;
  DamageType damageType = DamageType::Physical;
  int32_t slot = -1;
};
struct EvSkillBuffered {
  std::string skillId;
  double expiresAtMs = 0;
};
struct EvSkillLevelChanged {
  std::string skillId;
  int32_t level = 0;
};
struct EvHotbarChanged {};
struct EvDodgeStarted {
  double cooldownMs = 0, invulnerabilityMs = 0;
  Vec2 from, to;
};
// C4 Charge dash (HeroLocomotion::StartDash): Started -> Arrived, or Interrupted by stun / freeze (C5) where the hero
// stands. Drives the R5 `charge` signature montage and the dash streaks; the position itself comes from the Snapshot.
struct EvHeroDash {
  enum class Phase : uint8_t { Started, Arrived, Interrupted } phase = Phase::Started;
  Vec2 from, to;
  double startMs = 0, durationMs = 0;  // sim clock
  std::string skillId;
};
struct EvTargetChanged {
  EntityId target = kNoEntity;
};
struct EvCombatStateChanged {
  bool inCombat = false;
};
struct EvHeroDied {
  Vec2 pos;
};
struct EvHeroRespawned {
  Vec2 pos;
};
struct EvTownPortal {
  enum class Phase : uint8_t { Started, Completed, Cancelled } phase = Phase::Started;
  double channelMs = 0;
  Vec2 destination;
};

// ---- items ----
struct EvLootDropped {
  EntityId drop = kNoEntity;
  std::string itemUid;
  std::string baseId;
  ItemQuality quality = ItemQuality::Normal;
  int32_t quantity = 1;
  Vec2 pos;
  double expiresAtMs = 0;  // 0 = never
};
struct EvPotionDropped {
  EntityId drop = kNoEntity;
  PotionKind kind = PotionKind::Hp;
  int32_t amount = 0;
  Vec2 pos;
};
struct EvItemPicked {
  std::string itemUid;
  std::string baseId;
  ItemQuality quality = ItemQuality::Normal;
  int32_t quantity = 1;
};
struct EvPotionPicked {
  PotionKind kind = PotionKind::Hp;
  int32_t amount = 0;
};
struct EvInventoryChanged {};
struct EvEquipmentChanged {
  EquipSlot slot = EquipSlot::Weapon;
};
struct EvStashChanged {};
struct EvShopOpened {
  std::string npcId;
  bool blacksmith = false;
};
struct EvShopClosed {
  std::string npcId;
};
struct EvCraftPerformed {
  CraftAction action = CraftAction::Salvage;
  bool ok = false;
  std::string itemUid;
  LocText reason;  // failure reason
};

// ---- quests / NPC / dialogue / story ----
// Progress from a drop, a gather node or a clue carries the source point and the quest-item kind so UE can play the
// pickup visual (quests 3.2: the item pops at the kill / node point, then flies to the hero).
struct EvQuestUpdate {
  enum class Kind : uint8_t { Accepted, Progress, Completed, TurnedIn, Failed, Tracked } kind = Kind::Accepted;
  std::string questId;
  int32_t objectiveIndex = -1;
  int32_t current = 0, required = 0;
  bool completesQuest = false;
  bool hasFrom = false;  // Progress from a drop / gather node / clue
  Vec2 from;             // kill point / node / clue tile
  std::string itemKind;  // quest-item icon kind (QuestItemIcons), "" for kill / talk / explore progress
};
struct EvNpcInteracted {
  std::string npcId;
  EntityId npc = kNoEntity;
};
struct EvQuestCardOpened {
  std::string npcId;
};
struct EvDialogue {
  enum class Kind : uint8_t { Opened, NodeChanged, Closed } kind = Kind::Opened;
  std::string npcId;
  std::string treeId;
  std::string nodeId;
};
struct EvMiniBossDialogue {
  bool opened = true;
  EntityId monster = kNoEntity;
  std::string monsterId;
  // opened only (monsters-ai 8.3 / M8): header name key and every line of the linear tree, in order
  // (start -> nextNodeId until isEnd), as i18n keys data.miniBossDialogue.<monsterId>.<nodeId>.
  std::string nameKey;
  std::vector<std::string> lineKeys;
};
struct EvLoreCollected {
  std::string loreId;
};
struct EvHiddenAreaDiscovered {
  std::string areaId;
};
struct EvAchievementUnlocked {
  std::string achievementId;
};
struct EvStoryState {
  bool active = false;  // StoryDirector busy (STORY_STATE)
  std::string beatId;   // the beat that started / the last beat that finished
};
struct EvStoryBeat {
  enum class Phase : uint8_t { Began, Ended } phase = Phase::Began;
  std::string beatId;
  StoryBeatKind kind = StoryBeatKind::Cutscene;
};
// One presentation step of the current beat (cutscene step, sequence slide or chapter card), on the real-time clock.
// The core resolves what UE would otherwise have to decide (quests-story-ch1.md 8.5):
//   focus      hasFocus + focusEntity / focusPos: the `focus` step's target (player, {npc}, {monster} = the nearest
//              living instance, or a tile). A target that does not exist resolves the step at once (hasFocus false).
//   speaker    `say` steps: speakerNameKey (story.speaker.hero / story.speaker.villain / data.npc.<id>.name / the boss
//              intro name story.boss.<id>.name, else data.monster.<id>) and speakerArtId (portrait art id: the hero's
//              class id, "emblem_villain" for the villain, the NPC's spriteId else its id, the nearest living monster
//              instance's spriteKey else the def's; "emblem_generic" when nothing resolves).
//   timedMs    core-timed length of a self-ending step (focus, shake, flash, wait, title hold, chapter card); 0 = the
//              step waits for StoryAdvance (narrate, say, whisper, slides; the two-tap rule is UE's, StoryTiming).
struct EvStoryStep {
  std::string beatId;
  int32_t index = 0;
  bool isSlide = false;
  std::string sequenceId;  // slides: "prologue" / "epilogue" / "credits" (the epilogue beat plays both), "" otherwise
  CutsceneStep step;   // cutscene steps
  StorySlide slide;    // sequence slides / chapter card text keys
  bool hasFocus = false;
  EntityId focusEntity = kNoEntity;  // kNoEntity for a tile target
  Vec2 focusPos;
  std::string speakerNameKey;
  std::string speakerArtId;
  double timedMs = 0;
};
struct EvBossBar {
  bool show = false;
  EntityId monster = kNoEntity;
  std::string nameKey, epithetKey;
};
struct EvMonsterRenamed {
  EntityId monster = kNoEntity;
  std::string nameKey;
  uint32_t color = 0;
};
struct EvHuntRevealed {
  std::string huntId;
};
struct EvPet {
  enum class Kind : uint8_t { Obtained, Changed, LevelUp, Evolved, Exhausted, Recovered } kind = Kind::Changed;
  std::string petId;
  bool silent = false;
};
struct EvEmbersGained {
  int32_t amount = 0;
  Vec2 pos;
  bool fromKill = true;
};

// ---- world / flow ----
struct EvZone {
  enum class Phase : uint8_t { TransitionBegan, Entered, Exited } phase = Phase::Entered;
  std::string mapId;
  TilePos target;
};
struct EvInteractPrompt {
  InteractKind kind = InteractKind::None;
  EntityId id = kNoEntity;
  Vec2 pos;
};
struct EvRandomEvent {
  bool resolved = false;
  RandomEventType type = RandomEventType::Ambush;
  Vec2 pos;
  std::string zoneId;           // sys.event.* keys are per zone
  EntityId prop = kNoEntity;    // chest / merchant / puzzle prop / rescue NPC
};
// The environmental puzzle prompt (world 13.3; core-owned modal PanelId::Puzzle). Text keys:
// sys.event.puzzle.<zoneId>.<puzzleIndex>.{prompt,solution,reward}; answer with CmdPuzzleAnswer (0 solution, 1 leave).
struct EvPuzzlePrompt {
  bool open = true;  // false = closed (answered, left, zone exit, death)
  EntityId prop = kNoEntity;
  std::string zoneId;
  int32_t puzzleIndex = -1;
};
struct EvStoryDecorFocus {
  std::string decorId;  // empty = none
  bool showTooltip = false;
};
struct EvLog {
  LocText text;
  LogType type = LogType::System;
};
struct EvBanner {
  BannerKind kind = BannerKind::Toast;
  LocText title;
  LocText subtitle;
};
// A core-owned modal panel opened or closed (SimTypes.h IsCoreOwnedPanel): GameSim emits it whenever the owning
// system's state changes, so UE pushes / pops the widget from one signal. npcId: the NPC of a dialogue / quest card /
// shop / stash.
struct EvPanelRequest {
  PanelId panel = PanelId::Inventory;
  bool open = true;
  std::string npcId;
};
struct EvSaveRequested {
  std::string reason;  // SaveReason name; UE calls GameSim::SaveGame() and writes the slot
};

// ---- audio / camera ----
struct EvMusic {
  std::string trackKey;  // "" = silence
  double fadeOutSec = 0, fadeInSec = 0;
  bool loop = true;
  bool restart = false;
};
struct EvSfx {
  SfxId cue = SfxId::Hit;
  bool spatial = false;
  Vec2 pos;
  EntityId source = kNoEntity;
};
struct EvCameraShake {
  double durationMs = 0;
  double intensity = 0;  // web units (C9: same peak at render scale 1)
};
struct EvCameraFlash {
  uint32_t color = 0xffffff;
  double durationMs = 0;
  double alpha = 1;
};
struct EvSlowMotion {
  double timeScale = 1;
  double realDurationMs = 0;  // S6: dilation of sim + visuals
};

using Event = std::variant<EvEntitySpawned, EvEntityDespawned, EvEntityTeleported, EvPlayAnim, EvHit,
                           EvMonsterAttackCancelled, EvProjectileLaunched, EvProjectileEnded, EvGroundEffectStarted,
                           EvGroundEffectTriggered, EvGroundEffectEnded, EvSkillVfx, EvStatusApplied, EvStatusExpired,
                           EvFloatingText, EvExpGained, EvLevelUp, EvGoldChanged, EvSpiritChanged, EvResonance,
                           EvSkillUsed, EvSkillBuffered, EvSkillLevelChanged, EvHotbarChanged, EvDodgeStarted,
                           EvHeroDash, EvTargetChanged, EvCombatStateChanged, EvHeroDied, EvHeroRespawned, EvTownPortal,
                           EvLootDropped, EvPotionDropped, EvItemPicked, EvPotionPicked, EvInventoryChanged,
                           EvEquipmentChanged, EvStashChanged, EvShopOpened, EvShopClosed, EvCraftPerformed,
                           EvQuestUpdate, EvNpcInteracted, EvQuestCardOpened, EvDialogue, EvMiniBossDialogue,
                           EvLoreCollected, EvHiddenAreaDiscovered, EvAchievementUnlocked, EvStoryState, EvStoryBeat,
                           EvStoryStep, EvBossBar, EvMonsterRenamed, EvHuntRevealed, EvPet, EvEmbersGained, EvZone,
                           EvInteractPrompt, EvRandomEvent, EvPuzzlePrompt, EvStoryDecorFocus, EvLog, EvBanner,
                           EvPanelRequest, EvSaveRequested, EvMusic, EvSfx, EvCameraShake, EvCameraFlash, EvSlowMotion>;

// Append-only event list shared by every subsystem of one GameSim.
class EventSink {
 public:
  void Emit(Event e) { events_.push_back(std::move(e)); }
  // Convenience: a combat-log line.
  void Log(LocText text, LogType type = LogType::System) { Emit(EvLog{std::move(text), type}); }
  void Sfx(SfxId cue, bool spatial = false, Vec2 pos = {}, EntityId source = kNoEntity) {
    Emit(EvSfx{cue, spatial, pos, source});
  }
  const std::vector<Event>& Items() const { return events_; }
  size_t Size() const { return events_.size(); }
  void Clear() { events_.clear(); }

 private:
  std::vector<Event> events_;
};

}  // namespace abyss

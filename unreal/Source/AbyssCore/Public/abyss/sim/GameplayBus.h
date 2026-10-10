// Internal gameplay bus (ARCHITECTURE 3.2): synchronous, typed, deterministic notifications between subsystems.
//
// * Not visible to UE: presentation gets sim/Events.h events. The bus carries the cross-area hooks the web did with
//   EventBus listeners and direct ZoneScene calls (kill pipeline, quest triggers, equip-stat invalidation, ...).
// * Publish() calls every handler of that message type synchronously, in subscription order. GameSim subscribes its
//   pipelines at construction in the order the specs require (e.g. the kill hook order of monsters-ai.md 11), so the
//   order never depends on hashing or registration races.
// * No RTTI: handler lists live in a tuple indexed by message type at compile time.
//
// Adding a message type: append it to the Messages list at the bottom (owner: whoever needs it; coordinate in review).
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/Platform.h"
#include "abyss/base/Types.h"
#include "abyss/monsters/Monster.h"

namespace abyss {

// What dealt the killing blow (kill credit is identical for every source, pets spec 18.4).
enum class KillSource : uint8_t { HeroBasic, HeroSkill, StatusTick, Pet, Escort, Environment, Other };

// ---- combat / monsters ----
// Published once, on the single alive -> dead transition inside MonsterSystem::ApplyDamage (monsters 5).
struct MonsterKilledMsg {
  EntityId monster = kNoEntity;
  std::string defId;          // current def id (hunt leaders carry the hunt id)
  int32_t level = 1;
  bool elite = false;
  bool isMiniBoss = false;
  bool isSubDungeonMiniBoss = false;
  int32_t eliteAffixCount = 0;
  double affixLootBonus = 0;  // sum of affix lootQualityBonus
  double expReward = 0;       // spawn-scaled
  double goldMin = 0, goldMax = 0;
  Vec2 pos;
  MonsterRole role = MonsterRole::Regular;
  EntityId killer = kNoEntity;
  KillSource source = KillSource::Other;
  bool killedByEliteBasic = false;  // elite killed by a hero basic attack (slow-mo trigger, combat 11.6)
};

// A monster started chasing the hero: Idle / Patrol / Returning -> Chase (aggro range, provoke M2, taunt, ambush spawn).
// Published by MonsterSystem on that transition; the audio area plays the A7 aggro vocalisation.
struct MonsterAggroMsg {
  EntityId monster = kNoEntity;
  std::string defId;
  Vec2 pos;
  MonsterState previous = MonsterState::Idle;
};

struct HeroDamagedMsg {
  double amount = 0;
  double hpAfter = 0;
  double maxHp = 0;
  EntityId source = kNoEntity;
  bool isTick = false;
};

struct HeroDiedMsg {
  Vec2 pos;
};

struct HeroRespawnedMsg {
  Vec2 pos;
};

struct HeroLevelUpMsg {
  int32_t level = 1;
};

// Movement input from keyboard/stick/click (town-portal cancel W3, pending interactions).
struct HeroMoveInputMsg {};

struct CombatStateChangedMsg {
  bool inCombat = false;
};

// ---- items ----
struct ItemPickedMsg {
  std::string uid;
  std::string baseId;
  ItemQuality quality = ItemQuality::Normal;
};

// Equipment, achievement bonuses, pet passive, blessing or boons changed: the merged EquipStats must be rebuilt.
struct EquipStatsDirtyMsg {};

// ---- quests / story / pets ----
struct QuestAcceptedMsg {
  std::string questId;
};
struct QuestProgressMsg {
  std::string questId;
  int32_t objectiveIndex = 0;
  int32_t current = 0;
  int32_t required = 0;
  std::string targetId;
  int32_t amount = 1;
  bool completesQuest = false;
};
struct QuestCompletedMsg {
  std::string questId;
};
struct QuestTurnedInMsg {
  std::string questId;
};
struct QuestFailedMsg {
  std::string questId;
};
struct QuestTrackedChangedMsg {
  std::string questId;  // empty = none
};
struct NpcInteractedMsg {
  std::string npcId;
  EntityId npc = kNoEntity;
};
struct StoryBeatStartedMsg {
  std::string beatId;
};
struct StoryBeatFinishedMsg {
  std::string beatId;
  std::string grantPet;  // trigger grantPet (applied when the beat finishes)
};
// STORY_STATE: the StoryDirector became busy (a beat started from an idle director) or idle again (queue drained).
// musicTrack: the started beat's sequence music theme ("" = keep the zone music); the music director takes the story
// lock while active (audio 10.4 rule 2). A sequence beat ends its own span with {false} when it finishes (the zone's
// explore track returns) and a following beat retakes the lock with {true, ""} (StoryDirector.h).
// musicState: the sequence's music state, web sequence(id, zone, state) ("explore" or "" = explore; the epilogue and
// credits play "victory", StoryDirector.ts:106-110). The constructor keeps the three-field {active, beatId, musicTrack}
// form valid.
struct StoryStateMsg {
  StoryStateMsg() = default;
  StoryStateMsg(bool isActive, std::string beat, std::string track, std::string state = std::string())
      : active(isActive), beatId(std::move(beat)), musicTrack(std::move(track)), musicState(std::move(state)) {}
  bool active = false;
  std::string beatId;
  std::string musicTrack;
  std::string musicState;
};
// The boss bar (story 8.3 boss scan, 250 ms): shown for a named boss within bossBarRangeTiles, cleared otherwise.
// Audio 10.4 rule 4: bar shown && combat on -> boss music; cleared without a kill -> disengaged.
struct BossBarMsg {
  bool show = false;
  EntityId monster = kNoEntity;
  std::string monsterDefId;
  bool killed = false;  // show == false because the boss died (the kill pipeline clears the bar before audio runs)
};
// The active ley-beast or its look changed: SetActivePet (incl. rest), AddPet that makes it active (quest petReward,
// story grantPet), an evolution of the active beast, a load. GameSim wiring: equip stats rebuilt, then
// PetCompanion::OnPetChanged spawns / despawns / re-spawns the companion entity.
enum class PetChangeReason : uint8_t { Activated, Rested, Obtained, Evolved, Loaded };
struct PetChangedMsg {
  std::string petId;  // the active pet after the change ("" = resting)
  PetChangeReason reason = PetChangeReason::Activated;
};
struct AchievementUnlockedMsg {
  std::string achievementId;
};
struct LoreCollectedMsg {
  std::string loreId;
};

// ---- world / flow ----
struct ZoneEnteredMsg {
  std::string mapId;
  bool firstVisit = false;
};
struct ZoneExitedMsg {
  std::string mapId;
};
struct DifficultyCompletedMsg {
  Difficulty difficulty = Difficulty::Normal;
};

// Autosave triggers (U1, save-ui-input.md 3.6). GameSim defers them while the hero is Dying (3.4).
enum class SaveReason : uint8_t {
  ZoneEntered,
  ZoneChange,
  QuestTurnIn,
  LevelUp,
  StoryQueueFinished,
  SoulEchoClaimed,
  DifficultyCompleted,
  Respawn,
  Timer60s,
  AppBackground,
  ReturnToMenu,
};
ABYSS_ENUM_STRINGS(SaveReason, "zoneEntered", "zoneChange", "questTurnIn", "levelUp", "storyQueueFinished",
                   "soulEchoClaimed", "difficultyCompleted", "respawn", "timer60s", "appBackground", "returnToMenu")
struct SaveRequestMsg {
  SaveReason reason = SaveReason::ZoneEntered;
};

template <class... Ms>
struct MessageList {};

using GameplayMessages =
    MessageList<MonsterKilledMsg, MonsterAggroMsg, HeroDamagedMsg, HeroDiedMsg, HeroRespawnedMsg, HeroLevelUpMsg,
                HeroMoveInputMsg, CombatStateChangedMsg, ItemPickedMsg, EquipStatsDirtyMsg, QuestAcceptedMsg,
                QuestProgressMsg, QuestCompletedMsg, QuestTurnedInMsg, QuestFailedMsg, QuestTrackedChangedMsg,
                NpcInteractedMsg, StoryBeatStartedMsg, StoryBeatFinishedMsg, StoryStateMsg, BossBarMsg, PetChangedMsg,
                AchievementUnlockedMsg, LoreCollectedMsg, ZoneEnteredMsg, ZoneExitedMsg, DifficultyCompletedMsg,
                SaveRequestMsg>;

template <class List>
class BasicGameplayBus;

template <class... Ms>
class BasicGameplayBus<MessageList<Ms...>> {
 public:
  template <class M>
  using Handler = std::function<void(const M&)>;

  template <class M>
  void Subscribe(Handler<M> h) {
    std::get<std::vector<Handler<M>>>(handlers_).push_back(std::move(h));
  }

  template <class M>
  void Publish(const M& msg) const {
    // Copy-free iteration; handlers must not subscribe while being dispatched.
    for (const Handler<M>& h : std::get<std::vector<Handler<M>>>(handlers_)) h(msg);
  }

  template <class M>
  size_t HandlerCount() const {
    return std::get<std::vector<Handler<M>>>(handlers_).size();
  }

  void Clear() { handlers_ = {}; }

 private:
  std::tuple<std::vector<Handler<Ms>>...> handlers_;
};

using GameplayBus = BasicGameplayBus<GameplayMessages>;

}  // namespace abyss

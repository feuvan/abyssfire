// Audio rules in the core: event -> SFX cue mapping and the music director state machine. The rendered assets and the
// mixer are UE's; the core emits EvSfx / EvMusic.
// Spec: audio.md 3 (event -> SFX wiring: listeners, COMBAT_DAMAGE emitters, direct calls), 5 (music director: state,
// inputs, transitions, effective timings, combat flag source, story and menu hooks), 9.8 (core library API),
// 10.4 (Chapter 1 music rules); combat-feel.md 19 (FIX: MonsterDied + CombatDamage for skills and DoTs);
// DECISIONS A2 (true combat-music debounce; explore resumes position), A4 (3D panning for world SFX), A5 (boss_ch1,
// 8000 ms boss-victory hold), A6 (hit_heavy for heavy/crit/kill; player_hurt on damage taken).
//
// Owner area: world. Pure rule functions + `MusicDirector` (pure, real-time ticks) + `AudioDirector` (runtime wrapper
// that listens to the gameplay bus and emits EvSfx / EvMusic).
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "abyss/base/Enums.h"
#include "abyss/base/Platform.h"
#include "abyss/data/AudioData.h"
#include "abyss/sim/GameplayBus.h"

namespace abyss {

struct SimContext;
struct EvHit;

// 3.2: dodged -> miss; crit -> crit; else hit (A6: heavy / kill -> hit_heavy).
ABYSS_API std::optional<SfxId> SfxForCombatHit(const AudioRulesDef& r, bool dodged, bool crit, HitWeight weight);
// skill_used by damage type (fire / ice / lightning; arcane + poison -> skill_buff; else skill_melee).
ABYSS_API SfxId SfxForSkill(const AudioRulesDef& r, DamageType type);
// item_picked by quality (set -> loot_legendary).
ABYSS_API SfxId SfxForPickup(const AudioRulesDef& r, ItemQuality q);
// quest progress cue: objective done / material-or-clue step / none.
ABYSS_API std::optional<SfxId> SfxForQuestProgress(const AudioRulesDef& r, int32_t current, int32_t required,
                                                   std::string_view targetId, bool completesQuest);

enum class MusicState : uint8_t { Explore, Combat, Victory, Boss };
ABYSS_ENUM_STRINGS(MusicState, "explore", "combat", "victory", "boss")

struct MusicCommand {
  std::string trackKey;  // "<themeId>_<state>", "" = silence
  double fadeOutSec = 0;
  double fadeInSec = 0;
  bool loop = true;
  bool restart = false;
};

// resolveMusicTheme: theme table; dungeon_floor_* -> abyss_rift; ember_tower -> emerald_plains; else "".
ABYSS_API std::string ResolveMusicTheme(const MusicDirectorDef& def, std::string_view zoneId);

class ABYSS_API MusicDirector {
 public:
  explicit MusicDirector(const MusicDirectorDef& def);

  void OnZoneEntered(std::string_view zoneId);    // zone fade 2.0 / 1.0
  void OnCombatStateChanged(bool inCombat);       // A2 debounce handled upstream (CombatSystem 9.6)
  void OnBossEngaged(std::string_view bossDefId);
  void OnBossDisengaged();
  void OnBossDefeated(std::string_view bossDefId);  // victory, hold 3000 (boss 8000, A5), back to explore
  void PlayTrack(std::string_view zoneId, MusicState state);  // forced (story, jukebox); restarts
  void SetStoryLock(bool locked);                 // sequences own the music; combat changes queued
  void Tick(double realNowMs);                    // victory auto-return
  std::optional<MusicCommand> TakeCommand();

  MusicState State() const { return state_; }

 private:
  void Transition(double fadeOutSec, double fadeInSec, bool restart);

  const MusicDirectorDef* def_;
  std::string zone_;
  MusicState state_ = MusicState::Explore;
  bool inCombat_ = false;
  bool storyLock_ = false;
  std::string boss_;
  double victoryUntilMs_ = -1;
  double nowMs_ = 0;
  std::optional<MusicCommand> pending_;
};

// Runtime wrapper: bus listeners -> EvSfx (cue + spatial flag A4) and EvMusic. GameSim wiring (SimWiring.cpp):
// ZoneEnteredMsg, CombatStateChangedMsg, MonsterKilledMsg, MonsterAggroMsg, HeroLevelUpMsg, StoryStateMsg, BossBarMsg.
class ABYSS_API AudioDirector {
 public:
  explicit AudioDirector(SimContext& ctx);

  void OnZoneEntered(const ZoneEnteredMsg& m);
  void OnCombatStateChanged(const CombatStateChangedMsg& m);
  void OnMonsterKilled(const MonsterKilledMsg& m);  // monster_death (FIX)
  void OnMonsterAggro(const MonsterAggroMsg& m);    // A7 monster_aggro (spatial, source = the monster)
  void OnLevelUp(const HeroLevelUpMsg& m);          // levelup cue
  // Audio 10.4 rule 2: story lock while a beat plays; a sequence's music theme plays under the lock and the zone's
  // explore track returns when the director goes idle.
  void OnStoryState(const StoryStateMsg& m);
  // Audio 10.4 rule 4: bar shown && combat on -> OnBossEngaged (boss_ch1); bar cleared without a kill ->
  // OnBossDisengaged; cleared by the kill -> OnBossDefeated (victory, 8000 ms hold A5).
  void OnBossBar(const BossBarMsg& m);
  void AdvanceRealTime(double realMs);              // music director timers (presentation clock)
  MusicDirector& Music() { return music_; }

 private:
  void UpdateBossEngagement();

  SimContext& ctx_;
  MusicDirector music_;
  double realNowMs_ = 0;
  bool inCombat_ = false;
  bool bossBar_ = false;
  std::string bossDefId_;
  bool bossEngaged_ = false;
  std::string zoneId_;
  bool storySequenceMusic_ = false;
};

}  // namespace abyss

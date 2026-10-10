// Audio rules and the music director (audio.md 3, 5, 9.8, 10.4; A2, A4-A6). Owner area: world.
//
// MusicDirector priority (10.4): story lock > victory hold > boss > combat > explore. Track keys are
// "<themeId>_<state>" (explore / combat / victory) and the boss score key (A5 `boss_ch1`). Fades (5.4 effective
// timings): zone change and forced tracks fade the old music over zoneFadeSec, state changes over stateFadeSec, the new
// track fades in over fadeInSec. A2: the explore track resumes from its position after combat / boss / victory
// (restart = false); every other track starts from 0.
#include "abyss/base/Platform.h"

#include "abyss/audio/Audio.h"

#include <algorithm>

#include "abyss/data/DataStore.h"
#include "abyss/sim/SimContext.h"

namespace abyss {

std::optional<SfxId> SfxForCombatHit(const AudioRulesDef& r, bool dodged, bool crit, HitWeight weight) {
  if (dodged) return r.combatDodged;
  if (crit) return r.combatCrit;
  if (weight == HitWeight::Heavy || weight == HitWeight::Crit || weight == HitWeight::Kill) return r.heavyHitCue;  // A6
  return r.combatHit;
}

SfxId SfxForSkill(const AudioRulesDef& r, DamageType type) {
  return r.skillUsedByDamageType[static_cast<size_t>(type)];
}

SfxId SfxForPickup(const AudioRulesDef& r, ItemQuality q) { return r.itemPickedByQuality[static_cast<size_t>(q)]; }

std::optional<SfxId> SfxForQuestProgress(const AudioRulesDef& r, int32_t current, int32_t required,
                                         std::string_view targetId, bool completesQuest) {
  if (completesQuest) return std::nullopt;  // the quest_complete fanfare covers it
  if (current >= required) return r.questObjectiveDone;
  for (const std::string& prefix : r.questProgressPrefixes) {
    if (!prefix.empty() && targetId.substr(0, prefix.size()) == prefix) return r.questMaterialOrClueStep;
  }
  return std::nullopt;  // single kills are silent
}

std::string ResolveMusicTheme(const MusicDirectorDef& def, std::string_view zoneId) {
  for (const std::string& z : def.themeZones) {
    if (z == zoneId) return z;
  }
  constexpr std::string_view kDungeonFloorPrefix = "dungeon_floor_";
  if (zoneId.substr(0, kDungeonFloorPrefix.size()) == kDungeonFloorPrefix) return "abyss_rift";
  if (zoneId == "ember_tower") return "emerald_plains";
  return std::string();
}

// ---------------------------------------------------------------------------------------------------------------------
// MusicDirector
// ---------------------------------------------------------------------------------------------------------------------
MusicDirector::MusicDirector(const MusicDirectorDef& def) : def_(&def) {}

void MusicDirector::OnZoneEntered(std::string_view zoneId) {
  const bool zoneChanged = zone_ != zoneId || !started_;
  zone_ = std::string(zoneId);
  boss_.clear();
  victoryUntilMs_ = -1;
  inCombat_ = false;  // a new ZoneScene starts out of combat (desiredState = explore)
  if (storyLock_) {
    // The sequence owns the music; the zone's track starts when the lock is released (or the sequence's closing
    // playTrack names it).
    zonePending_ = zonePending_ || zoneChanged;
    state_ = MusicState::Explore;
    return;
  }
  if (zoneChanged) {
    state_ = MusicState::Explore;  // setZone: a new zone starts in explore with the zone fade
    Transition(def_->zoneFadeSec, def_->fadeInSec, true);
    return;
  }
  SetState(MusicState::Explore);  // same zone (respawn restart, reload): only a state change if any
}

void MusicDirector::OnCombatStateChanged(bool inCombat) {
  inCombat_ = inCombat;
  if (storyLock_ || victoryUntilMs_ >= 0 || state_ == MusicState::Boss) return;  // queued / lower priority
  SetState(inCombat ? MusicState::Combat : MusicState::Explore);
}

void MusicDirector::OnBossEngaged(std::string_view bossDefId) {
  boss_ = std::string(bossDefId);
  if (storyLock_ || victoryUntilMs_ >= 0 || def_->bossCh1Score.empty()) return;
  if (state_ == MusicState::Boss) return;
  state_ = MusicState::Boss;
  Transition(def_->stateFadeSec, def_->fadeInSec, true);
}

void MusicDirector::OnBossDisengaged() {
  boss_.clear();
  if (state_ != MusicState::Boss) return;
  if (storyLock_) {
    state_ = inCombat_ ? MusicState::Combat : MusicState::Explore;
    return;
  }
  SetState(inCombat_ ? MusicState::Combat : MusicState::Explore);
}

void MusicDirector::OnBossDefeated(std::string_view bossDefId) {
  (void)bossDefId;
  boss_.clear();
  if (storyLock_) return;
  state_ = MusicState::Victory;
  victoryUntilMs_ = nowMs_ + def_->bossVictoryHoldMs;  // A5: 8000 ms boss-victory hold
  Transition(def_->stateFadeSec, def_->fadeInSec, true);
}

void MusicDirector::PlayTrack(std::string_view zoneId, MusicState state) {
  // Forced (story sequence, jukebox): restarts even when unchanged; plays under the story lock.
  zone_ = std::string(zoneId);
  state_ = state;
  zonePending_ = false;
  victoryUntilMs_ = state == MusicState::Victory ? nowMs_ + def_->victoryHoldMs : -1;
  Transition(def_->zoneFadeSec, def_->fadeInSec, true);
}

void MusicDirector::SetStoryLock(bool locked) {
  const bool released = storyLock_ && !locked;
  storyLock_ = locked;
  if (!released) return;
  if (zonePending_) {  // a zone entered under the lock: its explore track with the zone fade
    zonePending_ = false;
    state_ = MusicState::Explore;
    victoryUntilMs_ = -1;
    Transition(def_->zoneFadeSec, def_->fadeInSec, true);
  }
  // Changes queued under the lock apply now (boss > combat > explore); a forced PlayTrack may follow and wins.
  if (!boss_.empty() && inCombat_) {
    OnBossEngaged(boss_);
  } else if (state_ != MusicState::Victory) {
    SetState(inCombat_ ? MusicState::Combat : MusicState::Explore);
  }
}

void MusicDirector::Tick(double realNowMs) {
  nowMs_ = realNowMs;
  if (victoryUntilMs_ >= 0 && realNowMs >= victoryUntilMs_) {
    victoryUntilMs_ = -1;
    // Under the story lock combat stays queued: the (forced) victory returns to the sequence zone's explore track.
    SetState(!storyLock_ && inCombat_ ? MusicState::Combat : MusicState::Explore);
  }
}

std::optional<MusicCommand> MusicDirector::TakeCommand() {
  std::optional<MusicCommand> out = std::move(pending_);
  pending_.reset();
  return out;
}

void MusicDirector::SetState(MusicState s) {
  if (s == state_ && started_) return;  // setState: unchanged and playing
  if (s != MusicState::Victory) victoryUntilMs_ = -1;
  const MusicState was = state_;
  state_ = s;
  // A2: back to explore from a fight resumes the explore track where it left off.
  const bool resume =
      s == MusicState::Explore && was != MusicState::Explore && def_->exploreResumesPosition && started_;
  Transition(def_->stateFadeSec, def_->fadeInSec, !resume);
}

std::string MusicDirector::TrackKey() const {
  const std::string theme = ResolveMusicTheme(*def_, zone_);
  if (state_ == MusicState::Boss) return def_->bossCh1Score;
  if (theme.empty()) return std::string();  // no theme: fade out and play nothing
  return theme + "_" + std::string(EnumName(state_));
}

void MusicDirector::Transition(double fadeOutSec, double fadeInSec, bool restart) {
  started_ = true;
  MusicCommand c;
  c.trackKey = TrackKey();
  c.fadeOutSec = fadeOutSec;
  c.fadeInSec = fadeInSec;
  c.loop = state_ != MusicState::Victory;  // victory recordings / stingers are one-shots
  c.restart = restart;
  pending_ = std::move(c);
}

// ---------------------------------------------------------------------------------------------------------------------
// AudioDirector
// ---------------------------------------------------------------------------------------------------------------------
AudioDirector::AudioDirector(SimContext& ctx) : ctx_(ctx), music_(ctx.data.Audio().music) {}

void AudioDirector::OnZoneEntered(const ZoneEnteredMsg& m) {
  zoneId_ = m.mapId;
  bossBar_ = false;
  bossEngaged_ = false;
  inCombat_ = false;
  music_.OnZoneEntered(m.mapId);
}

void AudioDirector::OnCombatStateChanged(const CombatStateChangedMsg& m) {
  inCombat_ = m.inCombat;
  music_.OnCombatStateChanged(m.inCombat);
  UpdateBossEngagement();
}

void AudioDirector::OnMonsterKilled(const MonsterKilledMsg& m) {
  ctx_.events.Sfx(ctx_.data.Audio().rules.monsterDied, true, m.pos, m.monster);
}

void AudioDirector::OnMonsterAggro(const MonsterAggroMsg& m) {
  ctx_.events.Sfx(ctx_.data.Audio().rules.monsterAggro, true, m.pos, m.monster);
}

void AudioDirector::OnLevelUp(const HeroLevelUpMsg&) { ctx_.events.Sfx(ctx_.data.Audio().rules.playerLevelUp); }

void AudioDirector::OnItemPicked(const ItemPickedMsg& m) {
  ctx_.events.Sfx(SfxForPickup(ctx_.data.Audio().rules, m.quality));
}

void AudioDirector::OnQuestAccepted(const QuestAcceptedMsg&) { ctx_.events.Sfx(ctx_.data.Audio().rules.questAccepted); }

void AudioDirector::OnQuestProgress(const QuestProgressMsg& m) {
  if (const std::optional<SfxId> cue =
          SfxForQuestProgress(ctx_.data.Audio().rules, m.current, m.required, m.targetId, m.completesQuest)) {
    ctx_.events.Sfx(*cue);
  }
}

void AudioDirector::OnQuestCompleted(const QuestCompletedMsg&) {
  ctx_.events.Sfx(ctx_.data.Audio().rules.questCompleted);
}

void AudioDirector::OnQuestTurnedIn(const QuestTurnedInMsg&) { ctx_.events.Sfx(ctx_.data.Audio().rules.questTurnedIn); }

void AudioDirector::OnNpcInteracted(const NpcInteractedMsg& m) {
  const NpcDef* def = ctx_.data.FindNpc(m.npcId);
  if (def == nullptr) return;
  const AudioRulesDef& r = ctx_.data.Audio().rules;
  switch (def->type) {
    case NpcType::Quest:
      ctx_.events.Sfx(r.npcInteract);  // NPC_INTERACT (ZoneScene.ts:4159)
      break;
    case NpcType::Merchant:
    case NpcType::Blacksmith:
      ctx_.events.Sfx(r.shopOpen);  // SHOP_OPEN
      break;
    default:
      break;
  }
}

void AudioDirector::OnStoryState(const StoryStateMsg& m) {
  if (m.active && !m.musicTrack.empty()) {
    music_.SetStoryLock(true);
    storySequenceMusic_ = true;
    music_.PlayTrack(m.musicTrack, MusicState::Explore);
  } else if (!m.active && storySequenceMusic_) {
    storySequenceMusic_ = false;
    music_.SetStoryLock(false);
    music_.PlayTrack(zoneId_, MusicState::Explore);  // the zone's explore track returns (StoryDirector.ts:195)
  } else {
    music_.SetStoryLock(m.active);
  }
}

void AudioDirector::OnBossBar(const BossBarMsg& m) {
  if (m.show) {
    bossBar_ = true;
    bossDefId_ = m.monsterDefId;
    UpdateBossEngagement();
    return;
  }
  bossBar_ = false;
  if (bossEngaged_) {
    bossEngaged_ = false;
    if (m.killed) {
      music_.OnBossDefeated(bossDefId_);
    } else {
      music_.OnBossDisengaged();
    }
  }
}

void AudioDirector::UpdateBossEngagement() {
  if (bossBar_ && inCombat_ && !bossEngaged_) {
    bossEngaged_ = true;
    music_.OnBossEngaged(bossDefId_);
  }
}

void AudioDirector::AdvanceRealTime(double realMs) {
  realNowMs_ += realMs;
  music_.Tick(realNowMs_);
  while (std::optional<MusicCommand> c = music_.TakeCommand()) {
    ctx_.events.Emit(EvMusic{c->trackKey, c->fadeOutSec, c->fadeInSec, c->loop, c->restart});
  }
}

}  // namespace abyss

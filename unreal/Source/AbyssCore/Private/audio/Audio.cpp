// Audio rules and the music director (audio.md 3, 5, 9.8, 10.4; A2, A4-A6). STUB: owner area world.
#include "abyss/base/Platform.h"

#include "abyss/audio/Audio.h"

#include "abyss/base/Assert.h"
#include "abyss/data/DataStore.h"
#include "abyss/sim/SimContext.h"

namespace abyss {

std::optional<SfxId> SfxForCombatHit(const AudioRulesDef& r, bool dodged, bool crit, HitWeight weight) {
  ABYSS_UNIMPLEMENTED();
  return std::nullopt;
}

SfxId SfxForSkill(const AudioRulesDef& r, DamageType type) { return r.skillUsedByDamageType[static_cast<size_t>(type)]; }

SfxId SfxForPickup(const AudioRulesDef& r, ItemQuality q) { return r.itemPickedByQuality[static_cast<size_t>(q)]; }

std::optional<SfxId> SfxForQuestProgress(const AudioRulesDef& r, int32_t current, int32_t required,
                                         std::string_view targetId, bool completesQuest) {
  ABYSS_UNIMPLEMENTED();
  return std::nullopt;
}

std::string ResolveMusicTheme(const MusicDirectorDef& def, std::string_view zoneId) {
  ABYSS_UNIMPLEMENTED();
  return std::string(zoneId);
}

MusicDirector::MusicDirector(const MusicDirectorDef& def) : def_(&def) {}

void MusicDirector::OnZoneEntered(std::string_view zoneId) {
  ABYSS_UNIMPLEMENTED();
  zone_ = std::string(zoneId);
  (void)def_;
}

void MusicDirector::OnCombatStateChanged(bool inCombat) {
  ABYSS_UNIMPLEMENTED();
  inCombat_ = inCombat;
}

void MusicDirector::OnBossEngaged(std::string_view bossDefId) { ABYSS_UNIMPLEMENTED(); }

void MusicDirector::OnBossDisengaged() { ABYSS_UNIMPLEMENTED(); }

void MusicDirector::OnBossDefeated(std::string_view bossDefId) { ABYSS_UNIMPLEMENTED(); }

void MusicDirector::PlayTrack(std::string_view zoneId, MusicState state) { ABYSS_UNIMPLEMENTED(); }

void MusicDirector::SetStoryLock(bool locked) { storyLock_ = locked; }

void MusicDirector::Tick(double realNowMs) {
  nowMs_ = realNowMs;
  if (victoryUntilMs_ >= 0 && realNowMs >= victoryUntilMs_) ABYSS_UNIMPLEMENTED();
  (void)boss_;
  (void)state_;
}

std::optional<MusicCommand> MusicDirector::TakeCommand() {
  std::optional<MusicCommand> out = std::move(pending_);
  pending_.reset();
  return out;
}

void MusicDirector::Transition(double fadeOutSec, double fadeInSec, bool restart) { ABYSS_UNIMPLEMENTED(); }

AudioDirector::AudioDirector(SimContext& ctx) : ctx_(ctx), music_(ctx.data.Audio().music) {}

void AudioDirector::OnZoneEntered(const ZoneEnteredMsg& m) {
  zoneId_ = m.mapId;
  bossBar_ = false;
  bossEngaged_ = false;
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

void AudioDirector::OnStoryState(const StoryStateMsg& m) {
  music_.SetStoryLock(m.active);
  if (m.active && !m.musicTrack.empty()) {
    storySequenceMusic_ = true;
    music_.PlayTrack(m.musicTrack, MusicState::Explore);
  } else if (!m.active && storySequenceMusic_) {
    storySequenceMusic_ = false;
    music_.PlayTrack(zoneId_, MusicState::Explore);
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

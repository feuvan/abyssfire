// StoryDirector (quests-story-ch1.md section 8: 8.1 seen ids, 8.2 start / fire / queue / pump, 8.3 boss intros and bar,
// 8.4 cinematic freeze, 8.5 presentation timings; classes-stats-skills.md 19.1 D13 T15 / T16; DECISIONS S2, Q7).
// Owner area: quests+story+pets.
#include "abyss/base/Platform.h"

#include "abyss/story/StoryDirector.h"

#include <algorithm>

#include "abyss/base/Math.h"
#include "abyss/data/DataStore.h"
#include "abyss/hero/Hero.h"
#include "abyss/monsters/MonsterSystem.h"
#include "abyss/quests/QuestSystem.h"
#include "abyss/save/SaveData.h"
#include "abyss/world/Zone.h"
#include "abyss/sim/SimContext.h"
#include "abyss/sim/Snapshot.h"

namespace abyss {

bool StoryProgress::Has(std::string_view id) const { return std::find(seen_.begin(), seen_.end(), id) != seen_.end(); }

void StoryProgress::Add(std::string_view id) {
  if (!Has(id)) seen_.emplace_back(id);
}

void StoryProgress::Load(std::vector<std::string> ids, bool present) {
  seen_.clear();
  if (!present) {
    seen_.emplace_back("prologue");
    return;
  }
  for (std::string& id : ids) Add(id);
}

StoryDirector::StoryDirector(SimContext& ctx) : ctx_(ctx) {}

namespace {

// Web constants that are not data tables (StoryDirector.ts): the sequence music zone (prologue / epilogue play the
// Abyss Rift recording, audio 5.6) and the boss scan period (8.3).
constexpr std::string_view kStorySequenceMusic = "abyss_rift";
constexpr double kStoryBossScanMs = 250;
constexpr uint32_t kStoryBossNameColor = 0xffcf6a;

}  // namespace

// ---------------------------------------------------------------------------------------------------------------------
// Triggers (8.2)
// ---------------------------------------------------------------------------------------------------------------------

// start(): prologue on a brand-new game, this zone's chapter card on its first visit, then zone_entered triggers, then
// (Q7 completion) the lost turn-in cutscenes of this zone.
bool StoryDirector::OnZoneEntered(const ZoneEnteredMsg& m) {
  if (!progress_.Has("prologue")) {
    StoryBeat b;
    b.id = "prologue";
    b.kind = StoryBeatKind::Sequence;
    b.contentId = "prologue";
    b.musicTrack = std::string(kStorySequenceMusic);
    Enqueue(std::move(b));
  }
  const ChapterCard* ch = ctx_.data.Story().ChapterFor(m.mapId);
  const std::string chapterId = "chapter_" + m.mapId;
  if (ch != nullptr && !progress_.Has(chapterId)) {
    StoryBeat b;
    b.id = chapterId;
    b.kind = StoryBeatKind::Chapter;
    b.contentId = m.mapId;
    Enqueue(std::move(b));
    Fire(StoryTriggerOn::ZoneEntered, m.mapId);
    ReplayLostTurnInBeats(m.mapId);
    return true;
  }
  Fire(StoryTriggerOn::ZoneEntered, m.mapId);
  ReplayLostTurnInBeats(m.mapId);
  return false;
}

// Q7 marks a beat seen only when it finishes, but quest_turned_in fires once: the turn-in autosave is written before
// its cutscene plays, so a quit / kill during the cutscene (or its T15 delay), or a zone exit during the delay, would
// lose it - and its grantPet - for good. Entering the quest's zone re-queues, in script order with the normal turn-in
// delay, every quest_turned_in trigger of a turned-in quest of this zone whose cutscene is not seen. The giver stands in
// the quest's zone (QuestContent rule), so the cutscene replays where it was due. Saves without storySeen (pre-story
// data) never replay: the web treated them as having seen nothing but the prologue.
void StoryDirector::ReplayLostTurnInBeats(std::string_view mapId) {
  const QuestSystem* quests = ctx_.sys.quests;
  if (!replayLostBeats_ || quests == nullptr) return;
  const StoryScript& script = ctx_.data.Story();
  for (const StoryTriggerDef& t : script.triggers) {
    if (t.on != StoryTriggerOn::QuestTurnedIn || progress_.Has(t.cutscene)) continue;
    const QuestDef* q = ctx_.data.FindQuest(t.subjectId);
    const QuestProgress* p = quests->Progress(t.subjectId);
    if (q == nullptr || q->zone != mapId || p == nullptr || p->status != QuestStatus::TurnedIn) continue;
    EnqueueCutscene(t.cutscene, script.timing.beatDelayQuestTurnedInMs, t.grantPet);
  }
}

void StoryDirector::OnQuestTurnedIn(const QuestTurnedInMsg& m) { Fire(StoryTriggerOn::QuestTurnedIn, m.questId); }

void StoryDirector::OnQuestAccepted(const QuestAcceptedMsg& m) { Fire(StoryTriggerOn::QuestAccepted, m.questId); }

// onMonsterKilled(defId): monster_killed triggers (0 ms), the boss bar of that boss clears, the final boss queues the
// epilogue (epilogue sequence then credits).
void StoryDirector::OnMonsterKilled(const MonsterKilledMsg& m) {
  Fire(StoryTriggerOn::MonsterKilled, m.defId);
  if (!bossBarFor_.empty() && m.defId == bossBarFor_) SetBossBar(nullptr, kNoEntity, /*killed=*/true);
  if (m.defId == ctx_.data.Story().timing.finalBoss && !progress_.Has("epilogue")) {
    StoryBeat b;
    b.id = "epilogue";
    b.kind = StoryBeatKind::Sequence;
    b.contentId = "epilogue";
    b.musicTrack = std::string(kStorySequenceMusic);
    Enqueue(std::move(b));
  }
}

void StoryDirector::Fire(StoryTriggerOn on, std::string_view key) {
  const StoryTiming& tm = ctx_.data.Story().timing;
  double delay = 0;
  switch (on) {
    case StoryTriggerOn::QuestTurnedIn: delay = tm.beatDelayQuestTurnedInMs; break;
    case StoryTriggerOn::QuestAccepted: delay = tm.beatDelayQuestAcceptedMs; break;
    case StoryTriggerOn::MonsterKilled: delay = tm.beatDelayMonsterKilledMs; break;
    case StoryTriggerOn::ZoneEntered: delay = tm.beatDelayZoneEnteredMs; break;
  }
  for (const StoryTriggerDef& t : ctx_.data.Story().triggers) {
    if (t.on != on || t.subjectId != key) continue;
    EnqueueCutscene(t.cutscene, delay, t.grantPet);
  }
}

void StoryDirector::EnqueueCutscene(std::string_view cutsceneId, double delayMs, std::string_view grantPet) {
  if (ctx_.data.Story().FindCutscene(cutsceneId) == nullptr) return;
  StoryBeat b;
  b.id = std::string(cutsceneId);
  b.kind = StoryBeatKind::Cutscene;
  b.contentId = std::string(cutsceneId);
  b.delayMs = delayMs;
  b.grantPet = std::string(grantPet);
  Enqueue(std::move(b));
}

// Zone unload discards the queue: a beat still waiting on its T15 delay is lost (it was never played, so it is not
// marked seen either - Q7; quest triggers never fire again for it, a chapter card shows on the next first entry).
void StoryDirector::OnZoneExit() {
  queue_.clear();
  if (delayTimer_ != kNoTimer) {
    ctx_.timers.Cancel(delayTimer_);
    delayTimer_ = kNoTimer;
  }
  const bool wasRunning = running_;
  hasCurrent_ = false;
  current_ = StoryBeat{};
  playback_ = StoryPlayback{};
  cinematic_ = false;
  running_ = false;
  if (!bossBarFor_.empty()) SetBossBar(nullptr, kNoEntity, false);
  scanAccMs_ = 0;
  renamed_.clear();
  if (wasRunning) ctx_.events.Emit(EvStoryState{false, std::string()});
  if (audioLock_) PublishStoryState(false, std::string(), std::string());
}

// The music side of STORY_STATE (audio 10.4 rule 2): the director holds the story lock while busy, except that a
// sequence beat (prologue / epilogue) owns its music span: {true, musicTrack} when it starts and {false} when it ends,
// so the zone's explore track returns with the sequence's end (StoryDirector.ts:195) even when more beats follow (the
// new-game chapter card plays under the zone music); the next beat retakes the lock (no music change).
void StoryDirector::PublishStoryState(bool active, const std::string& beatId, const std::string& musicTrack) {
  audioLock_ = active;
  sequenceMusic_ = active && !musicTrack.empty();
  ctx_.bus.Publish(StoryStateMsg{active, beatId, musicTrack});
}

// ---------------------------------------------------------------------------------------------------------------------
// Queue (8.2 pump; D13 T15; Q7)
// ---------------------------------------------------------------------------------------------------------------------

// enqueue(id): ignored when already seen or already queued / in flight.
void StoryDirector::Enqueue(StoryBeat beat) {
  if (progress_.Has(beat.id)) return;
  if (hasCurrent_ && current_.id == beat.id) return;
  for (const StoryBeat& q : queue_) {
    if (q.id == beat.id) return;
  }
  queue_.push_back(std::move(beat));
  Pump();
}

// pump(): beats play strictly one at a time in enqueue order. The director turns busy (STORY_STATE{true}) when the
// first beat is queued; the T15 delay of a beat starts when it reaches the head of the idle queue (the world stays live
// during it); when the queue drains: autosave + STORY_STATE{false}.
void StoryDirector::Pump() {
  if (hasCurrent_) return;
  if (queue_.empty()) {
    if (running_) {
      running_ = false;
      ctx_.bus.Publish(SaveRequestMsg{SaveReason::StoryQueueFinished});
      ctx_.events.Emit(EvStoryState{false, current_.id});
      if (audioLock_) PublishStoryState(false, current_.id, std::string());
    }
    return;
  }
  current_ = std::move(queue_.front());
  queue_.erase(queue_.begin());
  hasCurrent_ = true;
  if (!running_) {
    running_ = true;
    ctx_.events.Emit(EvStoryState{true, current_.id});
    PublishStoryState(true, current_.id, current_.musicTrack);
    current_.musicTrack.clear();  // announced with the busy state
  }
  if (current_.delayMs > 0) {
    delayTimer_ = ctx_.timers.Schedule(ctx_.Now() + current_.delayMs, TimerOwner::Story,
                                       static_cast<uint16_t>(StoryTimerKind::BeatDelay));
    return;
  }
  StartBeat();
}

void StoryDirector::OnTimer(const Timer& t) {
  if (t.kind != static_cast<uint16_t>(StoryTimerKind::BeatDelay) || t.id != delayTimer_) return;
  delayTimer_ = kNoTimer;
  if (hasCurrent_ && !playback_.playing) StartBeat();
}

// The beat's presentation starts: the world freezes (IsCinematic) until the step player finishes, including the
// cutscene's 450 ms camera return (D13).
void StoryDirector::StartBeat() {
  cinematic_ = true;
  playback_ = StoryPlayback{};
  playback_.playing = true;
  playback_.beat = current_;
  ctx_.events.Emit(EvStoryBeat{EvStoryBeat::Phase::Began, current_.id, current_.kind});
  ctx_.bus.Publish(StoryBeatStartedMsg{current_.id});
  // A sequence that is not the beat which turned the director busy announces its music under the story lock; any other
  // beat retakes the lock a finished sequence released.
  if (!current_.musicTrack.empty()) {
    PublishStoryState(true, current_.id, current_.musicTrack);
  } else if (!audioLock_) {
    PublishStoryState(true, current_.id, std::string());
  }
  BuildSegments();
  EnterSegment(0);
  RunPlayer();
}

// Q7: the beat is marked seen when it finishes (a skipped beat counts as finished); grantPet goes out with
// StoryBeatFinishedMsg (GameSim: PetSystem::AddPet); then the next beat or the end of the queue.
void StoryDirector::FinishBeat() {
  if (!hasCurrent_) return;
  const StoryBeat done = current_;
  playback_ = StoryPlayback{};
  cinematic_ = false;
  hasCurrent_ = false;
  progress_.Add(done.id);
  ctx_.events.Emit(EvStoryBeat{EvStoryBeat::Phase::Ended, done.id, done.kind});
  ctx_.bus.Publish(StoryBeatFinishedMsg{done.id, done.grantPet});
  // The sequence's music ends with it: the zone's explore track returns and the lock is released (audio 10.4 rule 2).
  if (sequenceMusic_) PublishStoryState(false, done.id, std::string());
  current_ = StoryBeat{};
  current_.id = done.id;  // the STORY_STATE{false} event names the last finished beat
  Pump();
}

// ---------------------------------------------------------------------------------------------------------------------
// Step player (8.5 timings, StoryTiming phases; real time)
// ---------------------------------------------------------------------------------------------------------------------

void StoryDirector::AppendSequence(const StorySequence& seq, int32_t part) {
  const StoryTiming& tm = ctx_.data.Story().timing;
  std::vector<StorySegment>& segs = playback_.segments;
  const size_t first = segs.size();
  segs.push_back(StorySegment{StorySegmentKind::Backdrop, tm.sequenceBackdropInMs, -1, -1, -1, false, part});
  if (seq.credits) {
    const int32_t creditsAt = static_cast<int32_t>(segs.size());
    segs.push_back(StorySegment{StorySegmentKind::Credits, -1, creditsAt + 1, -1, 0, true, part});
  } else {
    StoryMood current = !seq.slides.empty() && seq.slides[0].hasMood ? seq.slides[0].mood : StoryMood::Embers;
    for (size_t i = 0; i < seq.slides.size(); ++i) {
      const StorySlide& slide = seq.slides[i];
      const int32_t idx = static_cast<int32_t>(i);
      bool emitted = false;
      if (slide.hasMood && slide.mood != current) {
        current = slide.mood;
        segs.push_back(StorySegment{StorySegmentKind::MoodSwap, tm.sequenceMoodOutMs + tm.sequenceMoodInMs, -1, -1,
                                    idx, true, part});
        emitted = true;
      }
      const int32_t slideAt = static_cast<int32_t>(segs.size());
      segs.push_back(StorySegment{StorySegmentKind::Slide, -1, slideAt + 1, -1, idx, !emitted, part});
      segs.push_back(StorySegment{StorySegmentKind::SlideOut, tm.slideOutMs, -1, -1, idx, false, part});
    }
  }
  const int32_t outAt = static_cast<int32_t>(segs.size());
  segs.push_back(StorySegment{StorySegmentKind::Backdrop, tm.sequenceBackdropOutMs, -1, -1, -1, false, part});
  // Skip: every remaining wait of this sequence resolves at once; the backdrop still fades out (StoryScene resets
  // `skipping` before that tween).
  for (size_t i = first; i + 1 < segs.size(); ++i) segs[i].skipTo = outAt;
}

void StoryDirector::BuildSegments() {
  const StoryScript& script = ctx_.data.Story();
  const StoryTiming& tm = script.timing;
  std::vector<StorySegment>& segs = playback_.segments;
  segs.clear();
  const StoryBeat& b = playback_.beat;
  switch (b.kind) {
    case StoryBeatKind::Sequence:
      if (b.contentId == "epilogue") {
        AppendSequence(script.epilogue, 0);
        AppendSequence(script.credits, 1);
        playback_.stepCount = static_cast<int32_t>(script.epilogue.slides.size());
      } else {
        AppendSequence(script.prologue, 0);
        playback_.stepCount = static_cast<int32_t>(script.prologue.slides.size());
      }
      break;
    case StoryBeatKind::Chapter: {
      // Skip (Esc; the chapter card has no skip button) ends the card at once: every tween jumps to its end.
      const int32_t end = 3;
      segs.push_back(StorySegment{StorySegmentKind::ChapterIntro, tm.chapterIntroMs, -1, end, 0, true, 0});
      segs.push_back(StorySegment{StorySegmentKind::ChapterHold, tm.chapterHoldMs, 2, end, 0, false, 0});
      segs.push_back(StorySegment{StorySegmentKind::ChapterOutro, tm.chapterOutroMs, -1, end, 0, false, 0});
      playback_.stepCount = 1;
      break;
    }
    case StoryBeatKind::Cutscene:
    case StoryBeatKind::BossIntro: {
      const Cutscene* cs = script.FindCutscene(b.contentId);
      const size_t n = cs != nullptr ? cs->steps.size() : 0;
      playback_.stepCount = static_cast<int32_t>(n);
      segs.push_back(StorySegment{StorySegmentKind::Letterbox, tm.letterboxMs, -1, -1, -1, false, 0});
      std::vector<size_t> stepStarts;
      for (size_t i = 0; i < n; ++i) {
        const CutsceneStep& st = cs->steps[i];
        const int32_t idx = static_cast<int32_t>(i);
        const int32_t at = static_cast<int32_t>(segs.size());
        stepStarts.push_back(segs.size());
        switch (st.kind) {
          case StoryStepKind::Narrate:
          case StoryStepKind::Say:
          case StoryStepKind::Whisper: {
            const StoryPhaseMs ph = st.kind == StoryStepKind::Narrate ? tm.narrate
                                    : st.kind == StoryStepKind::Say   ? tm.say
                                                                      : tm.whisper;
            segs.push_back(StorySegment{StorySegmentKind::StepIn, ph.inMs, at + 2, -1, idx, true, 0});
            segs.push_back(StorySegment{StorySegmentKind::StepWait, -1, at + 2, -1, idx, false, 0});
            segs.push_back(StorySegment{StorySegmentKind::StepOut, ph.outMs, -1, -1, idx, false, 0});
            break;
          }
          case StoryStepKind::Title:
            segs.push_back(StorySegment{StorySegmentKind::StepIn, tm.title.inMs, at + 2, -1, idx, true, 0});
            segs.push_back(StorySegment{StorySegmentKind::StepHold, tm.titleHoldMs, at + 2, -1, idx, false, 0});
            segs.push_back(StorySegment{StorySegmentKind::StepOut, tm.title.outMs, -1, -1, idx, false, 0});
            break;
          case StoryStepKind::Focus:
            segs.push_back(StorySegment{StorySegmentKind::StepTimed, st.hasMs ? st.ms : tm.focusDefaultMs, -1, -1, idx,
                                        true, 0});
            break;
          case StoryStepKind::Wait:
            segs.push_back(StorySegment{StorySegmentKind::StepTimed, st.hasMs ? st.ms : 0, -1, -1, idx, true, 0});
            break;
          case StoryStepKind::Shake:
          case StoryStepKind::Flash:
            segs.push_back(StorySegment{StorySegmentKind::StepTimed, 0, -1, -1, idx, true, 0});
            break;
        }
      }
      const int32_t outAt = static_cast<int32_t>(segs.size());
      segs.push_back(StorySegment{StorySegmentKind::Letterbox, tm.letterboxMs, -1, -1, -1, false, 0});
      segs.push_back(StorySegment{StorySegmentKind::CameraReturn, tm.cameraPanMs, -1, -1, -1, false, 0});
      // Skip breaks out of the step loop; the letterbox still slides out and the camera returns (not skippable).
      for (int32_t i = 0; i < outAt; ++i) segs[static_cast<size_t>(i)].skipTo = outAt;
      break;
    }
  }
}

void StoryDirector::EnterSegment(int32_t index) {
  playback_.segment = index;
  playback_.segmentElapsedMs = 0;
  if (index < 0 || static_cast<size_t>(index) >= playback_.segments.size()) return;
  StorySegment& seg = playback_.segments[static_cast<size_t>(index)];
  if (seg.index >= 0) playback_.stepIndex = seg.index;
  if (seg.kind == StorySegmentKind::CameraReturn) playback_.returningCamera = true;
  if (seg.kind == StorySegmentKind::Backdrop && seg.part > 0 && index > 0 &&
      playback_.segments[static_cast<size_t>(index - 1)].part != seg.part) {
    playback_.skipping = false;  // StoryScene.begin() resets the skip for the next sequence (credits)
    playback_.stepIndex = -1;
  }
  if (seg.emit) EmitStep(seg);
}

// EvStoryStep: the step / slide with the core-resolved focus and speaker. A focus step whose target does not exist
// resolves at once (its segment shrinks to 0 ms).
void StoryDirector::EmitStep(const StorySegment& seg) {
  const StoryScript& script = ctx_.data.Story();
  const StoryTiming& tm = script.timing;
  const StoryBeat& b = playback_.beat;
  EvStoryStep ev;
  ev.beatId = b.id;
  ev.index = seg.index;
  switch (b.kind) {
    case StoryBeatKind::Sequence: {
      const StorySequence& seq = b.contentId == "epilogue" ? (seg.part == 0 ? script.epilogue : script.credits)
                                                           : script.prologue;
      ev.isSlide = true;
      ev.sequenceId = seq.id;
      if (seg.index >= 0 && static_cast<size_t>(seg.index) < seq.slides.size()) {
        ev.slide = seq.slides[static_cast<size_t>(seg.index)];
      }
      break;
    }
    case StoryBeatKind::Chapter: {
      ev.isSlide = true;
      if (const ChapterCard* ch = script.ChapterFor(b.contentId)) {
        ev.slide.heading = ch->number;
        ev.slide.title = ch->title;
        ev.slide.subtitle = ch->subtitle;
        ev.slide.text = ch->text;
        ev.slide.hasMood = true;
        ev.slide.mood = ch->mood;
      }
      ev.timedMs = tm.chapterIntroMs + tm.chapterHoldMs + tm.chapterOutroMs;
      break;
    }
    case StoryBeatKind::Cutscene:
    case StoryBeatKind::BossIntro: {
      const Cutscene* cs = script.FindCutscene(b.contentId);
      if (cs == nullptr || seg.index < 0 || static_cast<size_t>(seg.index) >= cs->steps.size()) break;
      const CutsceneStep& st = cs->steps[static_cast<size_t>(seg.index)];
      ev.step = st;
      switch (st.kind) {
        case StoryStepKind::Focus: {
          const StoryActorView a = ResolveActor(st.target);
          ev.hasFocus = a.resolved;
          ev.focusEntity = a.entity;
          ev.focusPos = a.pos;
          ev.timedMs = a.resolved ? (st.hasMs ? st.ms : tm.focusDefaultMs) : 0.0;
          if (!a.resolved) playback_.segments[static_cast<size_t>(playback_.segment)].ms = 0;
          break;
        }
        case StoryStepKind::Say: {
          const StoryActorView a = ResolveActor(st.speaker);
          ev.speakerNameKey = a.nameKey;
          ev.speakerArtId = a.artId.empty() ? std::string("emblem_generic") : a.artId;
          break;
        }
        case StoryStepKind::Title: ev.timedMs = tm.titleHoldMs; break;
        case StoryStepKind::Shake: ev.timedMs = st.hasMs ? st.ms : 500; break;
        case StoryStepKind::Flash: ev.timedMs = st.hasMs ? st.ms : 300; break;
        case StoryStepKind::Wait: ev.timedMs = st.hasMs ? st.ms : 0; break;
        case StoryStepKind::Narrate:
        case StoryStepKind::Whisper: break;
      }
      break;
    }
  }
  ctx_.events.Emit(std::move(ev));
}

// Consumes elapsed real time: a timed segment that ran out passes its leftover time to the next one; zero-length
// segments end in the same call; past the last segment the beat finishes.
void StoryDirector::RunPlayer() {
  int32_t guard = 0;
  while (playback_.playing && ++guard < 100000) {
    const int32_t i = playback_.segment;
    if (i < 0 || static_cast<size_t>(i) >= playback_.segments.size()) {
      FinishBeat();
      return;
    }
    const StorySegment& seg = playback_.segments[static_cast<size_t>(i)];
    if (seg.ms < 0) {
      playback_.waitingForInput = true;
      playback_.stepRemainingMs = 0;
      return;
    }
    playback_.waitingForInput = false;
    if (playback_.segmentElapsedMs < seg.ms) {
      playback_.stepRemainingMs = seg.ms - playback_.segmentElapsedMs;
      return;
    }
    const double leftover = playback_.segmentElapsedMs - seg.ms;
    EnterSegment(i + 1);
    playback_.segmentElapsedMs = leftover;
  }
}

void StoryDirector::AdvanceRealTime(double realMs) {
  if (!playback_.playing || !(realMs > 0)) return;
  playback_.segmentElapsedMs += realMs;
  RunPlayer();
}

void StoryDirector::Advance() {
  if (!playback_.playing) return;
  const int32_t i = playback_.segment;
  if (i < 0 || static_cast<size_t>(i) >= playback_.segments.size()) return;
  const int32_t to = playback_.segments[static_cast<size_t>(i)].advanceTo;
  if (to < 0) return;
  EnterSegment(to);
  RunPlayer();
}

void StoryDirector::Skip() {
  if (!playback_.playing) return;
  const int32_t i = playback_.segment;
  if (i < 0 || static_cast<size_t>(i) >= playback_.segments.size()) return;
  const int32_t to = playback_.segments[static_cast<size_t>(i)].skipTo;
  if (to < 0) return;
  playback_.skipping = true;
  EnterSegment(to);
  RunPlayer();
}

void StoryDirector::FinishAllBeats() {
  for (int32_t guard = 0; (hasCurrent_ || !queue_.empty()) && guard < 10000; ++guard) {
    if (!hasCurrent_) {
      Pump();
      continue;
    }
    if (!playback_.playing) {
      if (delayTimer_ != kNoTimer) {
        ctx_.timers.Cancel(delayTimer_);
        delayTimer_ = kNoTimer;
      }
      StartBeat();
    }
    if (playback_.playing) FinishBeat();
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// Boss intros and the boss bar (8.3; monsters-ai 10)
// ---------------------------------------------------------------------------------------------------------------------

void StoryDirector::SetBossBar(const BossIntroDef* intro, EntityId monster, bool killed) {
  const std::string defId = bossBarFor_;
  bossBarFor_ = intro != nullptr ? intro->monsterId : std::string();
  bossBarMonster_ = intro != nullptr ? monster : kNoEntity;
  EvBossBar ev;
  ev.show = intro != nullptr;
  ev.monster = bossBarMonster_;
  if (intro != nullptr) {
    ev.nameKey = intro->name;
    ev.epithetKey = intro->epithet;
  }
  ctx_.events.Emit(std::move(ev));
  ctx_.bus.Publish(BossBarMsg{intro != nullptr, bossBarMonster_, intro != nullptr ? intro->monsterId : defId, killed});
}

// update(delta): every 250 ms (accumulator, the first scan runs at once) - the nearest living instance per boss intro,
// the closest of those: within bossBarRange its nameplate shows the intro name (once per instance), within bossSight
// and not seen -> boss_<id> beat (no delay), within bossBarRange -> the bar (re-emitted when the boss changes), else
// the bar clears.
void StoryDirector::Tick(double dtMs) {
  scanAccMs_ -= dtMs;
  if (scanAccMs_ > 0) return;
  scanAccMs_ = kStoryBossScanMs;
  const MonsterSystem* monsters = ctx_.sys.monsters;
  const Hero* hero = ctx_.sys.hero;
  if (monsters == nullptr || hero == nullptr) return;
  const Vec2 hp = hero->Position();
  const StoryScript& script = ctx_.data.Story();
  const BossIntroDef* nearIntro = nullptr;
  EntityId nearId = kNoEntity;
  double nearD = 0;
  for (const BossIntroDef& intro : script.bossIntros) {
    const EntityId id = monsters->NearestAliveOfDef(intro.monsterId, hp);
    const MonsterInstance* m = id != kNoEntity ? monsters->Find(id) : nullptr;
    if (m == nullptr) continue;
    const double d = JsHypot(m->pos.x - hp.x, m->pos.y - hp.y);
    if (nearIntro == nullptr || d < nearD) {
      nearIntro = &intro;
      nearId = id;
      nearD = d;
    }
  }
  const StoryTiming& tm = script.timing;
  if (nearIntro != nullptr && nearD <= tm.bossBarRangeTiles && ctx_.sys.monsters->MarkStoryNamed(nearId)) {
    ctx_.events.Emit(EvMonsterRenamed{nearId, nearIntro->name, kStoryBossNameColor});
  }
  if (nearIntro != nullptr && nearD <= tm.bossSightTiles && !progress_.Has("boss_" + nearIntro->monsterId)) {
    StoryBeat b;
    b.id = "boss_" + nearIntro->monsterId;
    b.kind = StoryBeatKind::BossIntro;
    b.contentId = nearIntro->cutscene;
    if (script.FindCutscene(b.contentId) != nullptr) Enqueue(std::move(b));
  }
  if (nearIntro != nullptr && nearD <= tm.bossBarRangeTiles) {
    if (bossBarFor_ != nearIntro->monsterId) SetBossBar(nearIntro, nearId, false);
  } else if (!bossBarFor_.empty()) {
    SetBossBar(nullptr, kNoEntity, false);
  }
}

StoryActorView StoryDirector::ResolveActor(const StoryActor& actor) const {
  StoryActorView v;
  const Hero* hero = ctx_.sys.hero;
  const Vec2 heroPos = hero != nullptr ? hero->Position() : Vec2();
  switch (actor.kind) {
    case StoryActorKind::None:
      v.artId = "emblem_generic";
      break;
    case StoryActorKind::Player:
    case StoryActorKind::Hero:
      v.resolved = hero != nullptr;
      v.entity = kHeroEntityId;
      v.pos = heroPos;
      v.nameKey = "story.speaker.hero";
      v.artId = hero != nullptr ? std::string(EnumName(hero->Class())) : std::string("emblem_generic");
      break;
    case StoryActorKind::Villain:
      v.resolved = true;
      v.nameKey = "story.speaker.villain";
      v.artId = "emblem_villain";
      break;
    case StoryActorKind::Npc: {
      const NpcDef* def = ctx_.data.FindNpc(actor.id);
      v.nameKey = def != nullptr && !def->nameKey.empty() ? def->nameKey : "data.npc." + actor.id + ".name";
      v.artId = def != nullptr && !def->spriteId.empty() ? def->spriteId : actor.id;
      const NpcPlacement* p = ctx_.sys.zone != nullptr && ctx_.sys.zone->HasZone() ? ctx_.sys.zone->FindNpc(actor.id)
                                                                                    : nullptr;
      if (p != nullptr) {
        v.resolved = true;
        v.entity = p->id;
        v.pos = p->pos;
      }
      break;
    }
    case StoryActorKind::Monster: {
      const BossIntroDef* intro = ctx_.data.Story().BossIntroFor(actor.id);
      const MonsterDef* def = ctx_.data.Monsters().Find(actor.id);
      v.nameKey = intro != nullptr ? intro->name : (def != nullptr ? def->nameKey : "data.monster." + actor.id);
      v.artId = def != nullptr ? def->spriteKey : std::string();
      const EntityId id =
          ctx_.sys.monsters != nullptr ? ctx_.sys.monsters->NearestAliveOfDef(actor.id, heroPos) : kNoEntity;
      if (const MonsterInstance* m = id != kNoEntity ? ctx_.sys.monsters->Find(id) : nullptr) {
        v.resolved = true;
        v.entity = id;
        v.pos = m->pos;
        if (!m->def.spriteKey.empty()) v.artId = m->def.spriteKey;
      }
      if (v.artId.empty()) v.artId = "emblem_generic";
      break;
    }
    case StoryActorKind::Tile:
      v.resolved = true;
      v.pos = actor.tile.Center();
      break;
  }
  return v;
}

void StoryDirector::FillSnapshot(Snapshot& out) const {
  out.cinematic = cinematic_;
  out.storyBusy = IsBusy();
  out.story = &playback_;
  out.bossBar = BossBarView{};
  if (bossBarFor_.empty()) return;
  out.bossBar.show = true;
  out.bossBar.monster = bossBarMonster_;
  if (const BossIntroDef* intro = ctx_.data.Story().BossIntroFor(bossBarFor_)) {
    out.bossBar.nameKey = intro->name;
    out.bossBar.epithetKey = intro->epithet;
  }
  // hp(): the live instance, null (0 / 0) once it died.
  const MonsterInstance* m = ctx_.sys.monsters != nullptr ? ctx_.sys.monsters->Find(bossBarMonster_) : nullptr;
  if (m != nullptr && m->IsAlive()) {
    out.bossBar.hp = m->hp;
    out.bossBar.maxHp = m->maxHp;
  }
}

void StoryDirector::WriteSave(SaveData& out) const {
  out.hasStorySeen = true;
  out.storySeen = progress_.Seen();
  (void)ctx_;
}

void StoryDirector::ReadSave(const SaveData& in) {
  progress_.Load(in.storySeen, in.hasStorySeen);
  replayLostBeats_ = in.hasStorySeen;
}

}  // namespace abyss

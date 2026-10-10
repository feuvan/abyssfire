// StoryDirector: story beat queue (prologue, chapter cards, triggered cutscenes, boss intros, epilogue + credits), the
// cinematic freeze flag, the boss scan / boss bar, and the core-side step player of the current beat.
// Spec: quests-story-ch1.md 8.1 (StoryProgress / seen ids), 8.2 (start, fire, onMonsterKilled, queue, pump,
// presentation wrappers), 8.3 (boss intros + bar), 8.4 (cinematic freeze), 8.5 (presentation timings: which steps wait
// for input, holds, letterbox, 450 ms return pan), 13 (API proposal); monsters-ai.md 10;
// classes-stats-skills.md 19.1 D13 (T15 trigger delays on the sim clock, IsCinematic from beat start until the
// presenter finishes incl. the return pan, T16 presentation clock); DECISIONS S2, Q7 (seen when the beat FINISHES; a
// skipped beat counts as finished), Q3 (grantPet applied when the beat finishes).
//
// Owner area: quests+story+pets. Runtime system (SimContext). Timers: TimerOwner::Story (T15 delays; sim clock, so they
// pause under any freeze). The step player runs on real time (GameSim::AdvanceRealTime) and emits EvStoryStep with the
// core-resolved focus / speaker (ResolveActor) and the core-timed length of self-ending steps; it times every phase
// with StoryTiming (story.json timing.phases: sequence backdrop / mood / slide parts, chapter card intro 3500 + hold +
// outro 1600, narrate / say / whisper in-out, title reveal + 2200 hold + out, letterbox, 450 ms return pan) so the beat
// ends - and the sim unfreezes - exactly when UE's overlay does. UE renders each step (Slate overlay, camera rig) and
// forwards input: the FIRST input during a reveal (typewriter, staggered slide parts, fades) is handled by UE alone (it
// completes the reveal); UE sends StoryAdvance only for the input that advances (a waiting step, a title / chapter
// hold) and StorySkip for Esc / the skip button.
// Bus: StoryStateMsg is the music director's story lock (audio 10.4 rule 2): {active = true, beatId, musicTrack} when
// the director turns busy (with EvStoryState{true}), {active = false} when the queue drains (with EvStoryState{false}).
// A sequence beat (musicTrack set) owns its own span: {true, musicTrack} at its start, {false} when it finishes (the
// zone's explore track returns), and the next queued beat retakes the lock with {true, ""} when it starts - so the
// new-game chapter card plays under the zone music. EvStoryState stays busy / idle only. The boss scan publishes
// BossBarMsg whenever the bar appears / changes / clears (killed = true when OnMonsterKilled clears it) with EvBossBar.
// Q7 completion: entering a zone re-queues the unseen quest_turned_in cutscenes of its turned-in quests (a quit during
// the cutscene or its delay would otherwise lose them, and their grantPet, for good).
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "abyss/base/Platform.h"
#include "abyss/base/Timers.h"
#include "abyss/base/Types.h"
#include "abyss/data/StoryData.h"
#include "abyss/sim/GameplayBus.h"
#include "abyss/sim/SimTypes.h"

namespace abyss {

struct SimContext;
struct Snapshot;
struct SaveData;

enum class StoryTimerKind : uint16_t { BeatDelay = 1 };

// Seen beat ids (8.1): prologue, chapter_<zoneId>, cs_* (trigger cutscenes), boss_<monsterId>, epilogue.
class ABYSS_API StoryProgress {
 public:
  bool Has(std::string_view id) const;
  void Add(std::string_view id);
  const std::vector<std::string>& Seen() const { return seen_; }
  // load(save.storySeen ?? ['prologue']).
  void Load(std::vector<std::string> ids, bool present);
  void Clear() { seen_.clear(); }

 private:
  std::vector<std::string> seen_;  // insertion order
};

struct StoryBeat {
  std::string id;                 // seen id
  StoryBeatKind kind = StoryBeatKind::Cutscene;
  std::string contentId;          // sequence: prologue/epilogue; chapter: zone id; cutscene / boss intro: cutscene id
  double delayMs = 0;             // T15
  std::string grantPet;
  std::string musicTrack;         // sequence music ("" = keep)
};

// A story actor resolved against the live world (EvStoryStep focus / speaker, quests-story-ch1.md 8.5):
//   player / hero  entity kHeroEntityId at the hero, name story.speaker.hero, art = the hero's class id
//   villain        no entity, name story.speaker.villain, art "emblem_villain"
//   {npc}          the NPC placement of the zone (unresolved when absent), name data.npc.<id>.name, art = spriteId else id
//   {monster}      the nearest LIVING instance to the hero (unresolved when none), name = the boss intro's
//                  story.boss.<id>.name else data.monster.<id>, art = the instance's spriteKey else the def's
//   tile           the tile centre (always resolved, no entity)
// `resolved` false = a focus step resolves at once; a say step still shows its name with art "emblem_generic".
struct StoryActorView {
  bool resolved = false;
  EntityId entity = kNoEntity;
  Vec2 pos;
  std::string nameKey;
  std::string artId;
};

// One piece of the started beat's presentation, timed by the core step player (StoryTiming, quests 8.5):
//   Letterbox      cutscene letterbox in / out (letterboxMs)
//   StepIn         reveal of a narrate / say / whisper / title step (its phase inMs); StoryAdvance ends the step early
//   StepWait       narrate / say / whisper waiting for StoryAdvance
//   StepHold       title hold (titleHoldMs) or StoryAdvance
//   StepOut        fade after a step (its phase outMs)
//   StepTimed      focus (pan ms, 0 when the target does not exist), wait (ms), shake / flash (0: fire-and-forget,
//                  the next step starts in the same frame - web parity, the camera hooks are not awaited)
//   Backdrop       sequence backdrop in / out
//   MoodSwap       sequence mood change before a slide (moodOutMs + moodInMs); the slide's EvStoryStep is emitted here
//   Slide          a sequence slide waiting for StoryAdvance (the staggered parts are UE's reveal, two-tap rule)
//   SlideOut       slide fade out
//   Credits        the credits roll: until StoryAdvance (UE sends it when the roll ends) or StorySkip
//   ChapterIntro / ChapterHold / ChapterOutro   the chapter card (introMs, chapterHoldMs or input, outroMs)
//   CameraReturn   the 450 ms pan back to the hero at the end of a cutscene (never skipped, D13 T16)
enum class StorySegmentKind : uint8_t {
  Letterbox,
  StepIn,
  StepWait,
  StepHold,
  StepOut,
  StepTimed,
  Backdrop,
  MoodSwap,
  Slide,
  SlideOut,
  Credits,
  ChapterIntro,
  ChapterHold,
  ChapterOutro,
  CameraReturn,
};

struct StorySegment {
  StorySegmentKind kind = StorySegmentKind::StepTimed;
  double ms = 0;            // < 0: until StoryAdvance / StorySkip
  int32_t advanceTo = -1;   // segment StoryAdvance jumps to (-1: ignored)
  int32_t skipTo = -1;      // segment StorySkip jumps to (-1: not skippable; == segment count: the beat ends)
  int32_t index = -1;       // cutscene step / slide index presented (EvStoryStep emitted when `emit`)
  bool emit = false;
  int32_t part = 0;         // sequence part of the beat (the epilogue beat: 0 epilogue, 1 credits)
};

// Core-side playback of the started beat (real time).
struct StoryPlayback {
  bool playing = false;
  StoryBeat beat;
  int32_t stepIndex = -1;          // cutscene step / slide index; -1 = intro (letterbox / backdrop)
  int32_t stepCount = 0;
  bool waitingForInput = false;    // narrate / say / whisper / slides / credits (until StoryAdvance)
  double stepRemainingMs = 0;      // the current timed segment's remaining ms (0 while waiting for input)
  bool skipping = false;           // Esc / skip: remaining waits resolve instantly
  bool returningCamera = false;    // cutscene end: 450 ms return pan (not skippable, D13 T16)
  std::vector<StorySegment> segments;
  int32_t segment = -1;            // current segment
  double segmentElapsedMs = 0;
};

class ABYSS_API StoryDirector {
 public:
  explicit StoryDirector(SimContext& ctx);

  // ---- triggers ----
  // start() (8.2): prologue on a new game, chapter card on first visit, zone_entered triggers. Returns true when a
  // chapter card was queued (the plain zone banner is skipped).
  bool OnZoneEntered(const ZoneEnteredMsg& m);
  void OnQuestTurnedIn(const QuestTurnedInMsg& m);
  void OnQuestAccepted(const QuestAcceptedMsg& m);
  void OnMonsterKilled(const MonsterKilledMsg& m);  // monster_killed triggers (0 ms), boss bar clear, epilogue
  void OnZoneExit();                                // discards the queue (a waiting beat is lost, Q14)

  // ---- sim step (not while cinematic) ----
  void Tick(double dtMs);  // 250 ms boss scan: rename (EvMonsterRenamed), boss intro enqueue, EvBossBar
  void OnTimer(const Timer& t);  // T15 delay elapsed -> beat starts, IsCinematic() true

  // ---- presentation (real time) ----
  // Runs the step player: segment timers elapse on real time (leftover time carries into the next segment), zero-length
  // segments run in the same call, a finished beat ends the cinematic (FinishBeat).
  void AdvanceRealTime(double realMs);
  void Advance();  // StoryAdvance command (tap / Space / Enter): ends the current reveal / wait / hold
  void Skip();     // StorySkip command (Esc / skip button): jumps to the beat part's outro (letterbox out, backdrop out)
  // Debug / test hook: plays every queued beat to its end at once (T15 delays dropped, each beat finished: marked seen,
  // grantPet sent, STORY_STATE{false} + the autosave request when the queue drains). The world unfreezes.
  void FinishAllBeats();

  // ---- actors ----
  StoryActorView ResolveActor(const StoryActor& actor) const;

  // ---- state ----
  bool IsCinematic() const { return cinematic_; }
  bool IsBusy() const { return running_ || !queue_.empty(); }
  const StoryPlayback& Playback() const { return playback_; }
  const StoryProgress& Progress() const { return progress_; }
  StoryProgress& MutableProgress() { return progress_; }

  void FillSnapshot(Snapshot& out) const;
  void WriteSave(SaveData& out) const;
  void ReadSave(const SaveData& in);

 private:
  // fire(on, key) (8.2): every trigger in script order with that `on` and key -> EnqueueCutscene with the T15 delay.
  void Fire(StoryTriggerOn on, std::string_view key);
  void ReplayLostTurnInBeats(std::string_view mapId);
  void PublishStoryState(bool active, const std::string& beatId, const std::string& musicTrack);
  void EnqueueCutscene(std::string_view cutsceneId, double delayMs, std::string_view grantPet);
  void Enqueue(StoryBeat beat);
  void Pump();
  void StartBeat();
  void FinishBeat();  // Q7 seen, grantPet (StoryBeatFinishedMsg), next beat or STORY_STATE{false} + SaveRequestMsg
  void BuildSegments();
  void AppendSequence(const StorySequence& seq, int32_t part);
  void EnterSegment(int32_t index);  // index == size: the beat ends
  void RunPlayer();                  // consumes elapsed time / zero-length segments
  void EmitStep(const StorySegment& seg);
  void SetBossBar(const BossIntroDef* intro, EntityId monster, bool killed);

  SimContext& ctx_;
  StoryProgress progress_;
  std::vector<StoryBeat> queue_;
  bool running_ = false;      // the pump is active (STORY_STATE true): a beat is delayed / playing or queued
  bool hasCurrent_ = false;   // `current_` was taken from the queue (waiting on its T15 delay or playing)
  StoryBeat current_;
  bool cinematic_ = false;
  TimerId delayTimer_ = kNoTimer;
  StoryPlayback playback_;
  double scanAccMs_ = 0;
  std::string bossBarFor_;
  EntityId bossBarMonster_ = kNoEntity;
  std::vector<EntityId> renamed_;
  bool audioLock_ = false;        // the last StoryStateMsg was active
  bool sequenceMusic_ = false;    // ... and carried a sequence's music
  bool replayLostBeats_ = true;   // false for a save without storySeen
};

}  // namespace abyss

// Story script. Source: story.json (prologue/epilogue/credits sequences, chapter cards, cutscenes, boss intros,
// triggers - trigger array order matters; moods and timing). Every text field is an i18n key.
// Spec: quests-story-ch1.md 1.8, 8; classes-stats-skills.md 19.1 (T15/T16).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/Platform.h"
#include "abyss/base/Types.h"

namespace abyss {

enum class StoryMood : uint8_t { Embers, Dawn, Night, Forge, Sand, Abyss, Light };
ABYSS_ENUM_STRINGS(StoryMood, "embers", "dawn", "night", "forge", "sand", "abyss", "light")

struct StorySlide {
  std::string heading, title, text;  // i18n keys (empty = absent)
  bool hasMood = false;
  StoryMood mood = StoryMood::Embers;
};

struct StorySequence {
  std::string id;
  std::vector<StorySlide> slides;
  bool credits = false;
};

struct ChapterCard {
  std::string zoneId;
  std::string number, title, subtitle, text;  // i18n keys
  StoryMood mood = StoryMood::Dawn;
};

enum class StoryStepKind : uint8_t { Narrate, Whisper, Say, Focus, Title, Shake, Flash, Wait };
ABYSS_ENUM_STRINGS(StoryStepKind, "narrate", "whisper", "say", "focus", "title", "shake", "flash", "wait")

// Who speaks a `say` step / what a `focus` step looks at.
enum class StoryActorKind : uint8_t { None, Player, Hero, Villain, Npc, Monster, Tile };

struct StoryActor {
  StoryActorKind kind = StoryActorKind::None;
  std::string id;  // npc / monster id
  TilePos tile;    // Tile
};

struct CutsceneStep {
  StoryStepKind kind = StoryStepKind::Narrate;
  std::string text;      // narrate / whisper / say
  StoryActor speaker;    // say
  StoryActor target;     // focus
  std::string title, subtitle;  // title
  bool hasMs = false;
  double ms = 0;         // focus (default focusDefaultMs) / shake (500) / flash (300) / wait
  bool hasIntensity = false;
  double intensity = 0.01;  // shake
  bool hasColor = false;
  uint32_t color = 0xffffff;  // flash
};

struct Cutscene {
  std::string id;
  std::vector<CutsceneStep> steps;
};

struct BossIntroDef {
  std::string monsterId;
  std::string name, epithet;  // i18n keys
  std::string cutscene;
};

enum class StoryTriggerOn : uint8_t { QuestTurnedIn, QuestAccepted, MonsterKilled, ZoneEntered };
ABYSS_ENUM_STRINGS(StoryTriggerOn, "quest_turned_in", "quest_accepted", "monster_killed", "zone_entered")

struct StoryTriggerDef {
  StoryTriggerOn on = StoryTriggerOn::QuestTurnedIn;
  std::string subjectId;  // questId / monsterId / zoneId
  std::string cutscene;
  std::string grantPet;   // empty = none
};

struct StoryMoodColors {
  StoryMood mood = StoryMood::Embers;
  std::string top, bottom, glow;  // CSS colour strings (render-only)
  uint32_t embers = 0;
};

// In / out phase lengths of one presentation step kind (story.json timing.phases; quests-story-ch1.md 8.5).
struct StoryPhaseMs {
  double inMs = 0;   // reveal (fades, staggered parts) before the step waits for input or holds
  double outMs = 0;  // fade after the step
};

// story.json timing. Presentation clock (real ms). The core step player (StoryDirector) times every self-ending phase
// with these numbers so the sim unfreezes exactly when UE's overlay finishes; UE uses the same numbers to render.
// Input contract (two-tap rule): the first input during a reveal completes it on the UE side; UE sends StoryAdvance only
// for the input that advances (a step waiting for input, or a title / chapter hold). StorySkip resolves all remaining
// waits of the beat except the cameraPanMs return.
struct StoryTiming {
  double beatDelayQuestTurnedInMs = 650;
  double beatDelayQuestAcceptedMs = 650;
  double beatDelayZoneEnteredMs = 900;
  double beatDelayMonsterKilledMs = 0;
  double bossSightTiles = 9;
  double bossBarRangeTiles = 14;
  std::string finalBoss = "demon_lord";
  double cameraPanMs = 450;
  double letterboxPx = 78;
  double letterboxMs = 450;
  double typewriterCps = 38;
  double chapterHoldMs = 3800;
  double focusDefaultMs = 900;
  double creditsPxPerSec = 42;
  // ---- phases ----
  // sequences (prologue / epilogue): backdrop fade in, per-slide mood swap (out, in), slide parts fade in one after the
  // other (input shows all), slide fade out, backdrop fade out at the end.
  double sequenceBackdropInMs = 900, sequenceMoodOutMs = 400, sequenceMoodInMs = 500;
  double slidePartInMs = 900, slideOutMs = 450, sequenceBackdropOutMs = 700;
  // chapter card: intro (shade, number, title, subtitle, body: 3500) -> hold chapterHoldMs or input -> outro (1600).
  double chapterIntroMs = 3500, chapterOutroMs = 1600;
  StoryPhaseMs narrate{1100, 650};
  StoryPhaseMs say{220, 160};  // + typewriter: text length / typewriterCps after say.inMs
  StoryPhaseMs whisper{1300, 600};
  StoryPhaseMs title{980, 450};
  double titleHoldMs = 2200;   // hold or input
};

struct ABYSS_API StoryScript {
  StorySequence prologue, epilogue, credits;
  std::vector<ChapterCard> chapters;
  std::vector<Cutscene> cutscenes;  // document order
  std::vector<BossIntroDef> bossIntros;
  std::vector<StoryTriggerDef> triggers;  // order matters
  std::vector<StoryMoodColors> moods;
  StoryTiming timing;

  const Cutscene* FindCutscene(std::string_view id) const;
  const ChapterCard* ChapterFor(std::string_view zoneId) const;
  const BossIntroDef* BossIntroFor(std::string_view monsterId) const;
};

}  // namespace abyss

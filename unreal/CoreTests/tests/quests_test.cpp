// Quests + story area (quests+story+pets owner): quest state machine, guide, quest world, dialogue, achievements,
// lore, story director. Spec vectors: quests-story-ch1.md section 16. Foundation smoke tests only.
#include "SimHarness.h"
#include "abyss/quests/Achievements.h"
#include "abyss/quests/QuestSystem.h"
#include "abyss/story/StoryDirector.h"
#include "doctest/doctest.h"

using namespace abyss;

TEST_SUITE("quests") {
  TEST_CASE("QuestSystem lists: open quests in insertion order, turned-in ids, tracking message") {
    test::SimHarness h;
    QuestSystem qs(h.ctx.data, h.events, h.bus);
    int tracked = 0;
    h.bus.Subscribe<QuestTrackedChangedMsg>([&](const QuestTrackedChangedMsg&) { ++tracked; });
    qs.Load({{"q_kill_slimes", QuestStatus::TurnedIn, {5}}, {"q_collect_slime_gel", QuestStatus::Active, {0, 0}}});
    CHECK(qs.IsTurnedIn("q_kill_slimes"));
    CHECK(qs.TurnedInIds() == std::vector<std::string>{"q_kill_slimes"});
    const auto open = qs.OpenQuests();
    REQUIRE(open.size() == 1);
    CHECK(open[0].first->id == "q_collect_slime_gel");
    qs.SetTracked("q_collect_slime_gel");
    qs.SetTracked("q_collect_slime_gel");
    CHECK(tracked == 1);
    CHECK(qs.Tracked() == "q_collect_slime_gel");
  }

  TEST_CASE("AchievementState starts empty") {
    AchievementState a(test::RealData());
    CHECK_FALSE(a.IsUnlocked("ach_first_kill"));
    CHECK(a.ProgressOf("kill") == 0);
  }
}

TEST_SUITE("story") {
  TEST_CASE("StoryProgress: missing storySeen means veterans skip the prologue; ids are unique") {
    StoryProgress p;
    p.Load({}, /*present=*/false);
    CHECK(p.Has("prologue"));
    p.Load({"chapter_emerald_plains", "chapter_emerald_plains"}, true);
    CHECK_FALSE(p.Has("prologue"));
    CHECK(p.Seen().size() == 1);
    p.Add("cs_ep_whisper");
    CHECK(p.Has("cs_ep_whisper"));
  }
}

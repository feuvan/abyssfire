// StoryDirector (quests-story-ch1.md section 8; D13 T15/T16; Q7). STUB: owner area quests+story+pets.
// StoryProgress, actor resolution (8.5 speaker / focus) and save plumbing are real.
#include "abyss/base/Platform.h"

#include "abyss/story/StoryDirector.h"

#include <algorithm>

#include "abyss/base/Assert.h"
#include "abyss/data/DataStore.h"
#include "abyss/hero/Hero.h"
#include "abyss/monsters/MonsterSystem.h"
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

bool StoryDirector::OnZoneEntered(const ZoneEnteredMsg& m) {
  ABYSS_UNIMPLEMENTED();
  (void)delayTimer_;
  return false;
}

void StoryDirector::OnQuestTurnedIn(const QuestTurnedInMsg& m) { ABYSS_UNIMPLEMENTED(); }

void StoryDirector::OnQuestAccepted(const QuestAcceptedMsg& m) { ABYSS_UNIMPLEMENTED(); }

void StoryDirector::OnMonsterKilled(const MonsterKilledMsg& m) { ABYSS_UNIMPLEMENTED(); }

void StoryDirector::OnZoneExit() {
  ABYSS_UNIMPLEMENTED();
  queue_.clear();
}

void StoryDirector::Tick(double dtMs) {
  ABYSS_UNIMPLEMENTED();
  (void)scanAccMs_;
  (void)renamed_;
}

void StoryDirector::OnTimer(const Timer& t) { ABYSS_UNIMPLEMENTED(); }

void StoryDirector::AdvanceRealTime(double realMs) {
  if (playback_.playing) ABYSS_UNIMPLEMENTED();
}

void StoryDirector::Advance() { ABYSS_UNIMPLEMENTED(); }

void StoryDirector::Skip() { ABYSS_UNIMPLEMENTED(); }

void StoryDirector::Enqueue(StoryBeat beat) { ABYSS_UNIMPLEMENTED(); }

void StoryDirector::Pump() { ABYSS_UNIMPLEMENTED(); }

void StoryDirector::StartBeat() { ABYSS_UNIMPLEMENTED(); }

void StoryDirector::FinishBeat() { ABYSS_UNIMPLEMENTED(); }

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
  out.bossBar.show = !bossBarFor_.empty();
}

void StoryDirector::WriteSave(SaveData& out) const {
  out.hasStorySeen = true;
  out.storySeen = progress_.Seen();
  (void)ctx_;
}

void StoryDirector::ReadSave(const SaveData& in) { progress_.Load(in.storySeen, in.hasStorySeen); }

}  // namespace abyss

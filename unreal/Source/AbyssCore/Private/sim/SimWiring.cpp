// GameplayBus subscriptions, in their binding order (ARCHITECTURE 3.2). Owner: lead; adding a hook = review.
//
// Kill pipeline (monsters-ai.md 11 / combat-feel.md 13.1), one synchronous MonsterKilledMsg from
// MonsterSystem::ApplyDamage:
//   1 combat     statuses cleared, Spirit 'kill', exp (expBonus incl. pet / homestead) and gold (Loot stream) through
//                RewardService, killHeal, floating texts, target cleanup
//   2 pets       PetSystem::OnKill(level)
//   3 achieve.   'kill' once (Q2) + 'kill:defId', level check
//   4 quests     kill progress (may reveal hunts through QuestProgressMsg)
//   5 story      monster_killed triggers, boss bar, epilogue
//   6 homestead  embers, garden, expedition
//   7 flow       difficulty completion (unlock boss in the unlock zone)
//   8 quests     quest drops (3.3)
//   9 items      ley fruit + generateLoot -> ground items / potion pickups
//  10 audio      monster_death SFX (FIX combat 19)
//  11 monsters   zone.monsterKill log + respawn decision (7)
#include "abyss/base/Platform.h"

#include "SimImpl.h"

namespace abyss {

void SimImpl::Wire() {
  // ---- kill pipeline ----
  bus.Subscribe<MonsterKilledMsg>([this](const MonsterKilledMsg& m) { combat->OnMonsterKilled(m); });
  bus.Subscribe<MonsterKilledMsg>([this](const MonsterKilledMsg& m) { pets->OnKill(m.level); });
  bus.Subscribe<MonsterKilledMsg>([this](const MonsterKilledMsg& m) { achievements->OnMonsterKilled(m); });
  bus.Subscribe<MonsterKilledMsg>([this](const MonsterKilledMsg& m) { questWorld->OnMonsterKilled(m); });
  bus.Subscribe<MonsterKilledMsg>([this](const MonsterKilledMsg& m) { story->OnMonsterKilled(m); });
  bus.Subscribe<MonsterKilledMsg>([this](const MonsterKilledMsg& m) { homestead->OnMonsterKilled(m); });
  bus.Subscribe<MonsterKilledMsg>([this](const MonsterKilledMsg& m) { OnMonsterKilledDifficulty(m); });
  bus.Subscribe<MonsterKilledMsg>([this](const MonsterKilledMsg& m) { questWorld->RollQuestDrops(m); });
  bus.Subscribe<MonsterKilledMsg>([this](const MonsterKilledMsg& m) { groundLoot->OnMonsterKilled(m); });
  bus.Subscribe<MonsterKilledMsg>([this](const MonsterKilledMsg& m) { audio->OnMonsterKilled(m); });
  bus.Subscribe<MonsterKilledMsg>([this](const MonsterKilledMsg& m) {
    ++session.totalKills;
    monsters->OnMonsterKilled(m);
  });

  // ---- monsters ----
  bus.Subscribe<MonsterAggroMsg>([this](const MonsterAggroMsg& m) { audio->OnMonsterAggro(m); });  // A7

  // ---- hero (HeroLevelUpMsg comes from RewardService::GrantExp) ----
  bus.Subscribe<HeroLevelUpMsg>([this](const HeroLevelUpMsg& m) {
    achievements->OnLevelUp(m);
    audio->OnLevelUp(m);
    RequestSave(SaveReason::LevelUp);
  });
  bus.Subscribe<HeroMoveInputMsg>([this](const HeroMoveInputMsg&) { zone->OnHeroMoveInput(); });  // W3 cancelOnMove
  bus.Subscribe<HeroDamagedMsg>([this](const HeroDamagedMsg& m) {
    // W3: damage >= 10 % max HP cancels the portal channel.
    const double frac = data.World().constants.townPortalCancelOnDamageFraction;
    if (m.maxHp > 0 && m.amount >= m.maxHp * frac) zone->CancelTownPortal();
  });
  bus.Subscribe<HeroDiedMsg>([this](const HeroDiedMsg&) {
    zone->CancelTownPortal();
    // save-ui-input 5.1.1 (UE's list, now core-owned): dialogue, quest card, shop / forge, stash, mini-boss, lore text
    // and puzzle close at death; EvPanelRequest{close} follows from the modal diff.
    CloseCoreModals();
  });
  bus.Subscribe<HeroRespawnedMsg>([this](const HeroRespawnedMsg&) {
    // save-ui-input 3.4 rule 4: autosave after every completed respawn (flushes a deferred request too).
    RequestSave(SaveReason::Respawn);
  });
  bus.Subscribe<CombatStateChangedMsg>([this](const CombatStateChangedMsg& m) { audio->OnCombatStateChanged(m); });

  // ---- equip-stat invalidation (loot 8.3; FIX Q13 for achievements) ----
  bus.Subscribe<EquipStatsDirtyMsg>([this](const EquipStatsDirtyMsg&) { RebuildEquipStats(); });
  // Q3: the active beast changed (activate / rest, quest petReward, story grantPet, evolution, load): passive stats
  // first, then the companion entity (spawn / despawn / re-spawn with the new stage's art).
  bus.Subscribe<PetChangedMsg>([this](const PetChangedMsg& m) {
    RebuildEquipStats();
    petCompanion->OnPetChanged(m);
  });
  bus.Subscribe<AchievementUnlockedMsg>([this](const AchievementUnlockedMsg&) { RebuildEquipStats(); });

  // ---- items ----
  bus.Subscribe<ItemPickedMsg>([this](const ItemPickedMsg& m) { achievements->OnItemPicked(m); });
  bus.Subscribe<ItemPickedMsg>([this](const ItemPickedMsg& m) { audio->OnItemPicked(m); });  // audio 3.1

  // ---- quests / story / pets ----
  bus.Subscribe<QuestAcceptedMsg>([this](const QuestAcceptedMsg& m) { story->OnQuestAccepted(m); });
  bus.Subscribe<QuestAcceptedMsg>([this](const QuestAcceptedMsg& m) { questWorld->OnQuestAccepted(m); });
  bus.Subscribe<QuestAcceptedMsg>([this](const QuestAcceptedMsg&) { monsters->SpawnDueHunts(false); });
  bus.Subscribe<QuestProgressMsg>([this](const QuestProgressMsg& m) { questWorld->OnQuestProgress(m); });
  bus.Subscribe<QuestProgressMsg>([this](const QuestProgressMsg&) { monsters->SpawnDueHunts(true); });
  // audio 3.1: quest / NPC cues (npc_interact, quest_objective / quest_progress, quest_complete, panel_open).
  bus.Subscribe<QuestAcceptedMsg>([this](const QuestAcceptedMsg& m) { audio->OnQuestAccepted(m); });
  bus.Subscribe<QuestProgressMsg>([this](const QuestProgressMsg& m) { audio->OnQuestProgress(m); });
  bus.Subscribe<QuestCompletedMsg>([this](const QuestCompletedMsg& m) { audio->OnQuestCompleted(m); });
  bus.Subscribe<QuestTurnedInMsg>([this](const QuestTurnedInMsg& m) { audio->OnQuestTurnedIn(m); });
  bus.Subscribe<NpcInteractedMsg>([this](const NpcInteractedMsg& m) { audio->OnNpcInteracted(m); });
  // Turn-in listeners run before exp / gold are paid (quests 4.1): story cutscene enqueue, embers + unlocks.
  bus.Subscribe<QuestTurnedInMsg>([this](const QuestTurnedInMsg& m) { story->OnQuestTurnedIn(m); });
  bus.Subscribe<QuestTurnedInMsg>([this](const QuestTurnedInMsg& m) { homestead->OnQuestTurnedIn(m); });
  bus.Subscribe<StoryBeatFinishedMsg>([this](const StoryBeatFinishedMsg& m) {
    if (!m.grantPet.empty()) pets->AddPet(m.grantPet);
  });
  // Music director (audio 10.4 rules 2 and 4): story lock / sequence music, boss engagement.
  bus.Subscribe<StoryStateMsg>([this](const StoryStateMsg& m) { audio->OnStoryState(m); });
  bus.Subscribe<BossBarMsg>([this](const BossBarMsg& m) { audio->OnBossBar(m); });

  // ---- world / flow ----
  bus.Subscribe<ZoneEnteredMsg>([this](const ZoneEnteredMsg& m) { achievements->OnZoneEntered(m); });
  bus.Subscribe<ZoneEnteredMsg>([this](const ZoneEnteredMsg& m) { audio->OnZoneEntered(m); });
  bus.Subscribe<DifficultyCompletedMsg>([this](const DifficultyCompletedMsg&) {
    RequestSave(SaveReason::DifficultyCompleted);
  });
  bus.Subscribe<SaveRequestMsg>([this](const SaveRequestMsg& m) { RequestSave(m.reason); });
}

// combat-feel.md 14: killing the unlock boss in the unlock zone (not in a labyrinth) completes the difficulty.
void SimImpl::OnMonsterKilledDifficulty(const MonsterKilledMsg& m) {
  const DifficultyTable& t = data.Combat().difficulty;
  if (m.defId != t.unlockBossId || session.currentMap != t.unlockZoneId) return;
  for (Difficulty d : session.completedDifficulties) {
    if (d == session.difficulty) return;
  }
  session.completedDifficulties.push_back(session.difficulty);
  bus.Publish(DifficultyCompletedMsg{session.difficulty});
}

}  // namespace abyss

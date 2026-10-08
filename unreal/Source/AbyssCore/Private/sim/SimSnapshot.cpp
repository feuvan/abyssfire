// Snapshot assembly (ARCHITECTURE 3.1). Owner: lead; each system fills its own views in FillSnapshot.
#include "abyss/base/Platform.h"

#include "SimImpl.h"

namespace abyss {

void Snapshot::ClearDynamic() {
  monsters.clear();
  npcs.clear();
  groundItems.clear();
  potions.clear();
  projectiles.clear();
  groundEffects.clear();
  markers.clear();
  bossBar = BossBarView{};
  prompt = InteractPromptView{};
  pet = PetView{};
}

void SimImpl::BuildSnapshot() {
  Snapshot& s = snapshot;
  s.ClearDynamic();
  s.simNowMs = clock.NowMs();
  s.steps = clock.Steps();
  s.interpolationAlpha = clock.Accumulator() / kSimStepMs;
  s.frozen = clock.IsFrozen();
  s.freezeMask = clock.FreezeMask();
  s.difficulty = session.difficulty;
  s.playTimeMs = session.playTimeMs;
  s.canSave = CanSave();
  if (hero == nullptr) return;
  s.inputBlocked = InputBlocked();
  s.modals.clear();
  const uint32_t modalMask = CoreModalMask();
  for (size_t i = 0; i < EnumCount<PanelId>(); ++i) {
    if ((modalMask & (1u << i)) != 0) s.modals.push_back(static_cast<PanelId>(i));
  }

  // Hero core view (hero area data, read directly).
  HeroView& h = s.hero;
  const Vec2 prev = h.pos;
  h.cls = hero->Class();
  h.pos = hero->Position();
  h.prevPos = hasSession ? prev : h.pos;
  h.facing = hero->Facing();
  h.level = hero->Level();
  h.exp = hero->Exp();
  h.expToNext = hero->ExpToNext();
  h.gold = hero->Gold();
  h.hp = hero->Hp();
  h.maxHp = hero->MaxHp();
  h.mana = hero->Mana();
  h.maxMana = hero->MaxMana();
  h.derived = hero->Derived();
  h.baseStats = hero->BaseStats();
  h.freeStatPoints = hero->FreeStatPoints();
  h.freeSkillPoints = hero->FreeSkillPoints();
  h.life = hero->Life();
  const Spirit& sp = hero->GetSpirit();
  h.spirit = sp.Value();
  h.spiritMax = sp.MaxValue();
  h.resonating = sp.IsResonating();
  h.resonanceRemainingMs = sp.ResonanceRemainingMs();
  h.statusMask = status->StatusMask(kHeroEntityId);
  h.autoCombat = hero->autoCombat;
  h.autoLoot = hero->autoLoot;
  h.lowHp = h.maxHp > 0 && h.hp > 0 && h.hp / h.maxHp < data.Combat().hitFeedback.lowHpBelowRatio;
  const SkillBook& book = hero->Skills();
  for (int32_t i = 0; i < SkillBook::kHotbarSlots; ++i) {
    SkillSlotView& v = h.hotbar[static_cast<size_t>(i)];
    v = SkillSlotView{};
    const int32_t idx = book.HotbarSkill(i);
    if (idx < 0) continue;
    v.skillIndex = idx;
    v.skillId = book.Skill(idx).id;
    v.level = book.Level(idx);
    const double ready = book.ReadyAtMs(idx);
    v.cooldownRemainingMs = ready > s.simNowMs ? ready - s.simNowMs : 0.0;
  }
  s.skills = &book;
  s.heroBuffs = &hero->Buffs();

  // Area views.
  zone->FillSnapshot(s);
  locomotion->FillSnapshot(s);
  exploration->FillSnapshot(s);
  monsters->FillSnapshot(s);
  projectiles->FillSnapshot(s);
  combat->FillSnapshot(s);
  soulEcho->FillSnapshot(s);
  inventory->FillSnapshot(s);
  groundLoot->FillSnapshot(s);
  shop->FillSnapshot(s);
  questWorld->FillSnapshot(s);
  dialogue->FillSnapshot(s);
  achievements->FillSnapshot(s);
  lore->FillSnapshot(s);
  story->FillSnapshot(s);
  homestead->FillSnapshot(s);
  petCompanion->FillSnapshot(s);
  randomEvents->FillSnapshot(s);
  s.quests = quests.get();
  s.pets = pets.get();
}

}  // namespace abyss

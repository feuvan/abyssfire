// BuildSave / ApplySave (save-ui-input.md 3.4 build rules incl. FIX Q34/Q36, 3.5 restore order incl. FIX Q8/Q35;
// DECISIONS U2, C12). Owner: lead (order); each system maps its own section in WriteSave / ReadSave.
#include "abyss/base/Platform.h"

#include "SimImpl.h"

#include <cmath>

#include "abyss/base/Assert.h"
#include "abyss/save/SaveIO.h"

namespace abyss {

void SimImpl::BuildSave(SaveData& out, int64_t unixMs) const {
  out = SaveData{};
  out.id = "slot" + std::to_string(session.slot);
  out.version = kCurrentSaveVersion;
  out.timestamp = unixMs;
  out.slot = session.slot;
  if (!hasSession) return;
  out.classId = hero->Class();
  hero->ToSave(out.player);
  out.player.currentMap = session.currentMap;
  if (hero->Life() != HeroLife::Alive || !(hero->Hp() > 0)) {
    // C12 / rule 1: a Dying hero is written as respawned at the current zone's camps[0] with full HP/MP. The death
    // penalty and the soul echo were already applied at death. Only the dead hero is rewritten: during a zone change
    // (CanSave() false while `transitioning`) UE answers the ZoneChange request after the step, and that save (as well as
    // a 60 s / background save inside the 400 ms fade) keeps the live hero in the old zone (save 3.6, world 9.2).
    const Vec2 camp = zone->CampPosition(0);
    out.player.tileCol = camp.x;
    out.player.tileRow = camp.y;
    out.player.hp = hero->MaxHp();
    out.player.mana = hero->MaxMana();
  }
  inventory->WriteSave(out);
  out.quests.assign(quests->AllProgress().begin(), quests->AllProgress().end());
  homestead->WriteSave(out);
  pets->WriteSave(out);
  achievements->WriteSave(out);
  out.settings.autoCombat = hero->autoCombat;
  out.settings.autoLootMode = hero->autoLoot;
  out.difficulty = session.difficulty;
  out.completedDifficulties = session.completedDifficulties;
  dialogue->WriteSave(out);
  monsters->WriteSave(out);
  lore->WriteSave(out);
  story->WriteSave(out);
  soulEcho->WriteSave(out);
  out.abyss = session.abyss;
  // v4
  out.hasHotbar = true;
  out.hotbar = hero->Skills().HotbarForSave();
  out.potionSlots = inventory->PotionSlots();
  out.playTimeMs = session.playTimeMs;
  out.rng.present = true;
  out.rng.streams = rng.GetStates();
  out.visitedZones = session.visitedZones;  // Q2
}

SaveError SimImpl::ApplySave(const SaveData& s, std::string* err) {
  // Checks that need no state run first, so a refused save leaves the current session untouched.
  if (s.version > kCurrentSaveVersion) {
    if (err != nullptr) *err = "save version too new";
    return SaveError::VersionTooNew;
  }
  if (data.Classes().Find(s.classId) == nullptr) {
    if (err != nullptr) *err = "unknown class";
    return SaveError::Invalid;
  }
  ResetSession();
  BuildSystems(s.classId);
  if (s.rng.present) {
    rng.SetStates(s.rng.streams);
  } else {
    rng.SeedAll(config.defaultSeed ^ static_cast<uint64_t>(s.timestamp));
  }
  session.slot = s.slot;
  session.difficulty = s.difficulty;
  session.completedDifficulties = DeriveCompletedDifficulties(s.difficulty, s.completedDifficulties);
  session.abyss = s.abyss;
  session.playTimeMs = s.playTimeMs;
  session.nextAutosaveAtMs = config.autosaveIntervalMs;
  session.visitedZones = s.visitedZones;  // Q2: zones seen before do not count as first visits again

  // 3.5 order: 1 hero (no position) -> 3 items (equipment before deriving maxima, FIX Q8) -> 4 quests / story / echo ->
  // 5 homestead -> 6 pets -> 7 achievements -> 9 settings / difficulty -> 11-12 dialogue + id sets.
  hero->FromSave(s.player);
  if (s.hasHotbar) hero->Skills().LoadHotbar(s.hotbar);
  inventory->ReadSave(s);
  for (size_t i = 0; i < s.potionSlots.size(); ++i) {
    inventory->SetPotionSlot(static_cast<PotionSlot>(i), s.potionSlots[i]);
  }
  quests->Load(s.quests);
  story->ReadSave(s);
  soulEcho->ReadSave(s);
  homestead->ReadSave(s);
  pets->ReadSave(s);
  achievements->ReadSave(s);
  hero->autoCombat = s.settings.autoCombat;
  hero->autoLoot = s.settings.autoLootMode;
  dialogue->ReadSave(s);
  monsters->ReadSave(s);
  lore->ReadSave(s);
  RebuildEquipStats();
  hero->RecalcDerived(ctx.equip);
  hasSession = true;

  // FIX Q35: a dead (or non-finite) saved hero loads as a completed respawn at camps[0]; else Q8 clamps and the 3.7
  // walkability fallback (handled by the zone entry using the saved position). A non-finite position is treated like a
  // dead save's position (camps[0]) and never reaches the grid / int conversions (Math.h SaturatingInt32).
  const bool deadSave = !(std::isfinite(s.player.hp) && s.player.hp > 0);
  const bool badPos = !(std::isfinite(s.player.tileCol) && std::isfinite(s.player.tileRow));
  const bool ok = EnterZone(s.player.currentMap, !badPos, badPos ? Vec2() : Vec2(s.player.tileCol, s.player.tileRow));
  if (!ok) {
    if (err != nullptr) *err = "unknown map";
    hasSession = false;
    return SaveError::Invalid;
  }
  if (deadSave) {
    hero->SetPosition(zone->CampPosition(0));
    hero->FillHpMana();
    events.Log(MakeLoc("sys.player.respawn"), LogType::System);
  } else {
    hero->SetHp(std::min(s.player.hp, hero->MaxHp()));
    hero->SetMana(std::isfinite(s.player.mana) ? std::clamp(s.player.mana, 0.0, hero->MaxMana()) : hero->MaxMana());
    Vec2 camp;
    if (badPos) {
      hero->SetPosition(zone->CampPosition(0));
      events.Log(MakeLoc("zone.save.positionReset"), LogType::System);
    } else if (zone->NearestWalkableCamp(hero->Position(), camp)) {
      hero->SetPosition(camp);
      events.Log(MakeLoc("zone.save.positionReset"), LogType::System);
    }
  }
  BuildSnapshot();
  return SaveError::None;
}

}  // namespace abyss

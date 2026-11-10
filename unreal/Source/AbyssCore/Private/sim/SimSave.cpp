// BuildSave / ApplySave (save-ui-input.md 3.4 build rules incl. FIX Q34/Q36, 3.5 restore order incl. FIX Q8/Q35;
// DECISIONS U2, C12). Owner: lead (order); each system maps its own section in WriteSave / ReadSave.
#include "abyss/base/Platform.h"

#include "SimImpl.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string>

#include "abyss/base/Assert.h"
#include "abyss/base/Log.h"
#include "abyss/save/SaveIO.h"

namespace abyss {

namespace {

// Salt of the zone-entry streams a load runs its spawns on (S3; see ApplySave).
constexpr uint64_t kSimLoadEntrySalt = 0x6C6F616445E7A1ull;

// A 64-bit seed derived from a saved stream state (pure function of the save).
uint64_t SimLoadDerivedSeed(const RngState& st, size_t streamIndex) {
  uint64_t h = kSimLoadEntrySalt ^ (static_cast<uint64_t>(streamIndex) + 1) * 0x9E3779B97F4A7C15ull;
  for (uint32_t w : st.s) h = SplitMix64(h ^ w).Next();
  return h;
}

// K7: whether the quest log holds the cleanse that sets a sub-dungeon's `flag`: the quest with the clear_area objective
// of that sub-dungeon (one per sub-dungeon, and nothing else sets the flag: DataStore::Finalize) is turned in or
// completed, or - with more objectives after it - still active with that objective done.
bool SimCleanseRecorded(const DataStore& data, const QuestSystem& quests, std::string_view flag) {
  for (const QuestDef& q : data.Quests().quests) {
    for (size_t i = 0; i < q.objectives.size(); ++i) {
      const QuestObjectiveDef& o = q.objectives[i];
      if (o.type != ObjectiveType::ClearArea || o.cleansedFlag != flag) continue;
      const QuestProgress* p = quests.Progress(q.id);
      if (p == nullptr) continue;
      if (p->status == QuestStatus::TurnedIn || p->status == QuestStatus::Completed) return true;
      if (p->status == QuestStatus::Active && i < p->objectives.size() && p->objectives[i] >= o.required) return true;
    }
  }
  return false;
}

}  // namespace

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
  // Dark remake K7 (world 9.6 "in the labyrinth: abyss_rift (15,22)"): a sub-dungeon is rebuilt on every visit and is
  // never a save's map. A save made inside is written in the parent zone at the tile the exit returns to (so is the
  // zone-entry autosave of the sub-dungeon itself); a hero that died inside is written where its respawn restarts the
  // parent (camps[0], fallback playerStart: CombatSystem::DungeonRespawnTarget). While the hero is already on its way
  // out (the 400 ms fade) the save names where that zone change leads: the return tile again for the exit, the parent's
  // camp for a death's restart - also the one ResolvePendingDeath starts when the app goes to the background or quits
  // (save-ui-input 3.4 rule 3: "placed at the parent zone's camps[0] ... then save").
  const SubDungeonDef* sub = data.World().FindSubDungeon(session.currentMap);
  const MapDef* subParent = sub != nullptr ? data.World().FindMapProper(sub->parentZone) : nullptr;
  if (subParent != nullptr) {
    out.player.currentMap = subParent->id;
    out.player.tileCol = sub->returnPos.col;
    out.player.tileRow = sub->returnPos.row;
    std::string leavingTo;
    Vec2 leavingAt;
    if (zone->PendingZoneChange(leavingTo, leavingAt) && leavingTo == subParent->id) {
      out.player.tileCol = leavingAt.x;
      out.player.tileRow = leavingAt.y;
    }
  }
  if (hero->Life() != HeroLife::Alive || !(hero->Hp() > 0)) {
    // C12 / rule 1: a Dying hero is written as respawned at the current zone's camps[0] with full HP/MP. The death
    // penalty and the soul echo were already applied at death. Only the dead hero is rewritten: during a zone change
    // (CanSave() false while `transitioning`) UE answers the ZoneChange request after the step, and that save (as well as
    // a 60 s / background save inside the 400 ms fade) keeps the live hero in the old zone (save 3.6, world 9.2).
    Vec2 camp = zone->CampPosition(0);
    if (subParent != nullptr) {
      const TilePos t = !subParent->camps.empty() ? subParent->camps[0].pos : subParent->playerStart;
      camp = Vec2(t.col, t.row);
    }
    out.player.tileCol = camp.x;
    out.player.tileRow = camp.y;
    out.player.hp = hero->MaxHp();
    out.player.mana = hero->MaxMana();
  } else {
    // A live hero can sit above a lowered maximum until the next heal (unequipping +maxHp / +maxMana gear: recalcDerived
    // never clamps, classes 3), and ApplySave clamps to the gear-inclusive maxima (FIX Q8). Write what a load restores
    // so save -> load -> save is idempotent.
    out.player.hp = std::min(out.player.hp, hero->MaxHp());
    if (std::isfinite(out.player.mana)) out.player.mana = std::clamp(out.player.mana, 0.0, hero->MaxMana());
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
  out.contentVersion = kCurrentContentVersion;
  // Dark remake K6: the world flags and the permanent rewards granted (SerializeSave writes them only when not empty).
  out.worldFlags = session.worldFlags;
  out.permanentRewards = session.permanentRewards;
}

SaveError SimImpl::ApplySave(const SaveData& in, std::string* err) {
  // implementation-plan.md 2.7: a save of an older content version loses the ids the data no longer has (its hero,
  // items and gold stay).
  SaveData s = in;
  if (const int32_t stale = ApplyContentVersion(data, s); stale > 0) {
    LogWarning("save: content version " + std::to_string(in.contentVersion) + " -> " +
               std::to_string(kCurrentContentVersion) + ", " + std::to_string(stale) + " stale id(s) dropped");
  }
  // Checks that need no state run first, so a refused save leaves the current session untouched.
  if (s.version > kCurrentSaveVersion) {
    if (err != nullptr) *err = "save version too new";
    return SaveError::VersionTooNew;
  }
  if (data.Classes().Find(s.classId) == nullptr) {
    if (err != nullptr) *err = "unknown class";
    return SaveError::Invalid;
  }
  // The zone entry falls back to the default map for an unknown saved map; when neither can be entered the load would
  // fail after ResetSession (GameSim.h: Invalid must leave the current session unchanged). Finalize already refuses a
  // store whose defaultMap is unknown; this is the defence in depth.
  if (data.FindMap(s.player.currentMap) == nullptr && data.FindMap(data.World().defaultMap) == nullptr) {
    if (err != nullptr) *err = "unknown map";
    return SaveError::Invalid;
  }
  ResetSession();
  BuildSystems(s.classId);
  // S3: every saved stream is restored exactly; a stream the save lacks (added after it was written, or a malformed
  // entry) gets the state SeedAll(defaultSeed ^ timestamp) gives it - what a save without `rng` uses for all of them.
  const uint64_t fallbackSeed = config.defaultSeed ^ static_cast<uint64_t>(s.timestamp);
  rng.SeedAll(fallbackSeed);
  if (s.rng.present) {
    std::array<RngState, kRngStreamCount> states = rng.GetStates();
    for (size_t i = 0; i < kRngStreamCount; ++i) {
      if (!s.rng.missing[i]) {
        states[i] = s.rng.streams[i];
      } else {
        LogWarning(std::string("save: rng stream '") + std::string(EnumName(static_cast<RngStream>(i))) +
                   "' missing or malformed; reseeded");
      }
    }
    rng.SetStates(states);
  }
  session.slot = s.slot;
  session.difficulty = s.difficulty;
  session.completedDifficulties = DeriveCompletedDifficulties(s.difficulty, s.completedDifficulties);
  session.abyss = s.abyss;
  session.playTimeMs = s.playTimeMs;
  session.nextAutosaveAtMs = config.autosaveIntervalMs;
  session.visitedZones = s.visitedZones;  // Q2: zones seen before do not count as first visits again
  // Dark remake K6: before the zone entry, so its spawn variants and NPC visibility read them. The permanent rewards'
  // stat / skill points are already in the saved hero's free points; maxLife joins the equip-stat merge below.
  // A SaveData built in code (LoadGame(const SaveData&)) is normalised as a parsed one is: the flags a sorted set.
  session.worldFlags = s.worldFlags;
  NormalizeWorldFlags(session.worldFlags);
  session.permanentRewards = s.permanentRewards;
  session.permanentRewards.Normalize();

  // 3.5 order: 1 hero (no position) -> 3 items (equipment before deriving maxima, FIX Q8) -> 4 quests / story / echo ->
  // 5 homestead -> 6 pets -> 7 achievements -> 9 settings / difficulty -> 11-12 dialogue + id sets.
  hero->FromSave(s.player);
  if (s.hasHotbar) hero->Skills().LoadHotbar(s.hotbar);
  inventory->ReadSave(s);
  for (size_t i = 0; i < s.potionSlots.size(); ++i) {
    inventory->SetPotionSlot(static_cast<PotionSlot>(i), s.potionSlots[i]);
  }
  quests->Load(s.quests);
  // K7 (3A review R3): a cleansed sub-dungeon spawns nothing, so its flag must not outlive the cleanse that set it.
  // In play the two move together (QuestWorld::CheckClearArea), but a load can part them: the cleansing quest's record
  // dropped (ApplyContentVersion: its id changed) or reset (QuestSystem::Load: its objective list changed), or a
  // hand-edited save. The quest could then never be finished - nothing left to kill for the objective its clear_area
  // waits for. A cleansedFlag without its cleanse in the quest log is dropped here, before the zone entry reads it:
  // the sub-dungeon is populated again and the quest can cleanse it once more. Other world flags are not touched.
  for (const SubDungeonDef& sd : data.World().subDungeons) {
    if (sd.cleansedFlag.empty() || !session.HasFlag(sd.cleansedFlag)) continue;
    if (SimCleanseRecorded(data, *quests, sd.cleansedFlag)) continue;
    LogWarning("save: world flag '" + sd.cleansedFlag + "' without the cleanse of '" + sd.id +
               "' in the quest log; dropped (the sub-dungeon is not cleansed)");
    std::vector<std::string>& flags = session.worldFlags;
    flags.erase(std::remove(flags.begin(), flags.end(), sd.cleansedFlag), flags.end());
  }
  story->ReadSave(s);
  soulEcho->ReadSave(s);
  homestead->ReadSave(s);
  pets->ReadSave(s);
  RetroGrantStoryPets();  // before the equip-stat merge / derived stats below (the zone entry's call is then a no-op)
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
  bool badPos = !(std::isfinite(s.player.tileCol) && std::isfinite(s.player.tileRow));
  // A save standing in the Ember Tower while the tower is locked (a content-version upgrade dropped the quest that opens
  // it, DECISIONS DR3) loads at the camp of the zone it would return to (its saved towerReturn map, else the default
  // map), as if its position were unusable.
  std::string mapId = s.player.currentMap;
  if (!mapId.empty() && mapId == data.Homestead().towerZoneId && !homestead->Tower().TowerUnlocked()) {
    const std::optional<TowerReturn>& back = homestead->Tower().Return();
    mapId = back.has_value() && data.FindMap(back->mapId) != nullptr ? back->mapId : data.World().defaultMap;
    badPos = true;
    LogWarning("save: the tower is locked; the hero loads in " + mapId);
  }
  // K7: BuildSave never names a sub-dungeon (a save made inside is written at the parent's return tile). A save that
  // does (hand-edited, or its sub-dungeon became one after it was written) loads the same way: in the parent zone at the
  // return tile, so nothing ever loads into a zone that is rebuilt on every visit.
  // Either way the hero stands on a return tile, which is authored against the parent's generated grid: when a prop
  // or a wall stands on it the load uses the exit's rule (ZoneRuntime::EnterZone: the nearest walkable tile within 8)
  // before 3.7's camp fallback, so "a save made inside loads at the entrance" does not depend on that tile. A return
  // tile is recognised by its exact tile coordinates in the parent of a hand-authored sub-dungeon (a walking hero
  // never stands on an unwalkable tile, so only BuildSave's remap puts a save there).
  Vec2 savedPos(s.player.tileCol, s.player.tileRow);
  bool onReturnTile = false;
  if (const SubDungeonDef* sub = data.World().FindSubDungeon(mapId)) {
    LogWarning("save: '" + mapId + "' is a sub-dungeon; the hero loads at its entrance in " + sub->parentZone);
    mapId = sub->parentZone;
    savedPos = Vec2(sub->returnPos.col, sub->returnPos.row);
    badPos = false;
    onReturnTile = true;
  } else if (!badPos) {
    for (const SubDungeonDef& sd : data.World().subDungeons) {
      if (sd.hasMap && sd.parentZone == mapId && savedPos.x == sd.returnPos.col && savedPos.y == sd.returnPos.row) {
        onReturnTile = true;
        break;
      }
    }
  }
  // S3: the zone entry (spawn jitter, elite affix rolls, hunts on RngStream::Ai) runs on streams DERIVED from the loaded
  // states, then the loaded states are put back: the session continues exactly the saved streams (SaveGame() right
  // after LoadGame() reproduces the save) and the first draws in play are not the uniforms that just placed and rolled
  // the zone's monsters. Both are pure functions of the save.
  const auto loadedStreams = rng.GetStates();
  for (size_t i = 0; i < kRngStreamCount; ++i) {
    rng.Get(static_cast<RngStream>(i)).Seed(SimLoadDerivedSeed(loadedStreams[i], i));
  }
  session.arrivingFromSubDungeon = onReturnTile;  // read and cleared by the zone entry
  const bool ok = EnterZone(mapId, !badPos, badPos ? Vec2() : savedPos);
  session.arrivingFromSubDungeon = false;
  if (!ok) {
    if (err != nullptr) *err = "unknown map";
    hasSession = false;
    return SaveError::Invalid;
  }
  rng.SetStates(loadedStreams);
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

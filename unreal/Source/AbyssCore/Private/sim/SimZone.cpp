// Zone entry / exit sequences (world-map-nav.md 9.1-9.3; monsters-ai.md 6.1 spawn order; save-ui-input.md 1.4) and
// the merged EquipStats (loot-items-inventory.md 8.3; quests-story-ch1.md 18.4). Owner: lead (flow) with the world
// area; the per-system work lives in each system's OnZoneEnter / OnZoneExit.
#include "abyss/base/Platform.h"

#include "SimImpl.h"

#include <algorithm>

#include "abyss/base/I18n.h"
#include "abyss/base/StrUtil.h"

namespace abyss {

bool SimImpl::EnterZone(std::string_view mapId, bool hasTarget, Vec2 target) {
  session.transitioning = false;
  if (!zone->EnterZone(mapId, hasTarget, target)) return false;
  const std::string& id = zone->MapId();
  session.currentMap = id;
  const bool firstVisit = !session.HasVisited(id);
  if (firstVisit) session.visitedZones.push_back(id);

  // Per-zone runtime (world 9.1 steps 2-6, monsters 6.1 order). Hero state carries over (9.3).
  status->ClearAll();
  exploration->OnZoneEnter();
  locomotion->OnZoneEnter();
  combat->OnZoneEnter();
  events.Emit(EvEntitySpawned{kHeroEntityId, EntityKind::Hero, std::string(EnumName(hero->Class())),
                              std::string(EnumName(hero->Class())), hero->Position(), hero->Facing(), 1.0});
  for (const NpcPlacement& n : zone->Npcs()) {
    events.Emit(EvEntitySpawned{n.id, EntityKind::Npc, n.npcId, n.npcId, n.pos, n.facing, 1.0});
  }
  monsters->SpawnZonePopulation();
  monsters->SpawnMiniBoss();
  lore->OnZoneEnter();
  questWorld->OnZoneEnter();
  petCompanion->OnZoneEnter();
  monsters->SpawnDueHunts(false);
  soulEcho->OnZoneEnter();
  randomEvents->OnZoneEnter();

  // Step 8-9: ZONE_ENTERED, log, story start (chapter card or the plain banner), autosave.
  const ZoneEnteredMsg msg{id, firstVisit};
  bus.Publish(msg);
  events.Emit(EvZone{EvZone::Phase::Entered, id, RoundToTile(hero->Position())});
  const MapDef& map = zone->Map();
  events.Log(MakeLoc("zone.enterZone", {KeyArg("zoneName", map.nameKey), {"min", ToStr(map.levelMin)},
                                         {"max", ToStr(map.levelMax)}}),
             LogType::System);
  const bool chapterCard = story->OnZoneEntered(msg);
  if (!chapterCard) events.Emit(EvBanner{BannerKind::Zone, MakeLoc(map.nameKey), LocText{}});
  RebuildEquipStats();
  RequestSave(SaveReason::ZoneEntered);
  return true;
}

void SimImpl::ExitZone() {
  if (!zone->HasZone()) return;
  const std::string id = zone->MapId();
  RequestSave(SaveReason::ZoneChange);
  // Every core-owned modal (dialogue, quest card, shop / forge, stash, mini-boss, lore text, puzzle) closes through its
  // owner before the systems drop their zone state; the modal diff emits EvPanelRequest{close}.
  CloseCoreModals();
  SyncCoreModals();
  story->OnZoneExit();
  questWorld->OnZoneExit();
  lore->OnZoneExit();
  randomEvents->OnZoneExit();
  petCompanion->OnZoneExit();
  groundLoot->OnZoneExit();
  projectiles->ClearZone();
  combat->OnZoneExit();
  monsters->OnZoneExit();
  status->ClearAll();
  // combat 11.6: the elite-kill slow motion (S6 dilation) ends with the zone (UE restores its visuals on the event).
  const bool slowMotion = clock.Dilation() != 1.0;
  clock.CancelDilation();
  if (slowMotion) events.Emit(EvSlowMotion{1.0, 0.0});
  // Safety net: nothing zone-bound survives (Sim-owned timers are session level).
  timers.CancelIf([](const Timer& t) { return t.owner != TimerOwner::Sim; });
  bus.Publish(ZoneExitedMsg{id});
  events.Emit(EvZone{EvZone::Phase::Exited, id, TilePos{}});  // W12: emitted (the web never did)
  zone->ExitZone();
}

void SimImpl::RebuildEquipStats() {
  EquipStats e = inventory->Items().GearStats();
  e.AddAll(achievements->State().Bonuses());
  // Active pet passive, except expBonus / magicFind which apply at the kill (quests 18.4).
  const StatBag petBonus = pets->Bonuses();
  for (const StatValue& sv : petBonus.Items()) {
    if (sv.stat == Stat::ExpBonus || sv.stat == Stat::MagicFind) continue;
    e.Add(sv.stat, sv.value);
  }
  homestead->BlessingStats().AddTo(e);
  ctx.equip = e;
}

}  // namespace abyss

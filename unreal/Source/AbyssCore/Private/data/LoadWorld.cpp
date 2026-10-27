#include "abyss/base/Platform.h"

#include <algorithm>
#include <initializer_list>
#include <string>
#include <string_view>

#include "JsonReader.h"

namespace abyss::dataload {
namespace {

void ReadRange(const JNode& n, std::string_view key, int32_t& a, int32_t& b) {
  const std::vector<int32_t> v = n.IntList(key);
  if (v.size() != 2) {
    n.Child(key).Error("expected [min, max]");
    return;
  }
  a = v[0];
  b = v[1];
}

// Every key of a K5 block is known (a misspelt field would otherwise be dropped silently).
void ReadSpawnCheckKeys(const JNode& n, std::initializer_list<std::string_view> known, std::string_view what) {
  for (const auto& [key, child] : n.SelfMembers(true)) {
    bool ok = false;
    for (std::string_view k : known) ok = ok || k == key;
    if (!ok) child.Error("unknown " + std::string(what) + " field '" + key + "'");
  }
}

// `packs`: a map's spawns (K5 mix / leader); a sub-dungeon's spawn has no pack until K7 (nothing spawns it yet, and
// DataStore's pack checks read the maps only), so mix / leader are refused there.
std::vector<MapSpawnDef> ReadSpawns(const JNode& n, bool packs) {
  std::vector<MapSpawnDef> out;
  for (const JNode& s : n.Items("spawns", true)) {
    MapSpawnDef sp;
    sp.pos = TilePos{s.Int("col"), s.Int("row")};
    sp.monsterId = s.Str("monsterId");
    sp.count = s.Int("count");
    if (!packs) {
      for (const char* key : {"mix", "leader"}) {
        if (s.Has(key)) s.Child(key).Error("a sub-dungeon spawn has no pack (mix / leader) until K7");
      }
      if (s.Has("variants")) s.Child("variants").Error("a sub-dungeon spawn has no variants until K7");
      out.push_back(std::move(sp));
      continue;
    }
    // Dark remake K5 (remake schema map.spawnMix): the pack's composition and its leader (DataStore checks the sum, the
    // ids, the entry's own monsterId and that the leader is a member).
    for (const JNode& e : s.Items("mix", true)) {
      ReadSpawnCheckKeys(e, {"monsterId", "count"}, "spawn mix");
      MapSpawnMixDef m;
      m.monsterId = e.Str("monsterId");
      m.count = e.Int("count");
      if (m.monsterId.empty()) e.Child("monsterId").Error("empty monster id");
      if (m.count < 1) e.Child("count").Error("expected >= 1");
      sp.mix.push_back(std::move(m));
    }
    if (s.Has("mix") && sp.mix.empty()) s.Child("mix").Error("an empty mix (omit it for the entry's one monster)");
    if (s.Has("leader")) {
      const JNode l = s.Child("leader");
      ReadSpawnCheckKeys(l, {"monsterId", "elite"}, "spawn leader");
      sp.leader.present = true;
      sp.leader.monsterId = l.Str("monsterId");
      sp.leader.elite = l.Bool("elite", false);
      if (sp.leader.monsterId.empty()) l.Child("monsterId").Error("empty monster id");
    }
    // Dark remake K6 (remake schema map.spawnVariants): what the entry spawns once a world flag is set (DataStore checks
    // the ids; the first variant whose flag is set wins).
    for (const JNode& v : s.Items("variants", true)) {
      ReadSpawnCheckKeys(v, {"whenFlag", "monsterId", "count"}, "spawn variant");
      MapSpawnVariantDef d;
      d.whenFlag = v.Str("whenFlag");
      d.monsterId = v.Str("monsterId");
      d.count = v.Int("count");
      if (d.whenFlag.empty()) v.Child("whenFlag").Error("empty flag id");
      if (d.monsterId.empty()) v.Child("monsterId").Error("empty monster id");
      if (d.count < 0 || d.count > kSpawnVariantMaxCount) {
        v.Child("count").Error("expected 0.." + std::to_string(kSpawnVariantMaxCount));
      }
      for (const MapSpawnVariantDef& prev : sp.variants) {
        if (prev.whenFlag == d.whenFlag) v.Child("whenFlag").Error("a second variant of flag '" + d.whenFlag + "'");
      }
      sp.variants.push_back(std::move(d));
    }
    if (s.Has("variants") && sp.variants.empty()) s.Child("variants").Error("an empty variants list (omit it)");
    out.push_back(std::move(sp));
  }
  return out;
}

std::vector<WeightedDecor> ReadWeighted(const JNode& n) {
  std::vector<WeightedDecor> out;
  for (const JNode& e : n.SelfItems()) {
    const auto items = e.SelfItems();
    if (items.size() != 2) {
      e.Error("expected [type, weight]");
      continue;
    }
    out.push_back(WeightedDecor{items[0].AsStr(), items[1].AsNum()});
  }
  return out;
}

}  // namespace

void LoadMapsFile(const JNode& r, WorldTables& out) {
  out.maps.clear();
  out.subDungeons.clear();
  out.mapOrder = r.StrList("order");
  out.defaultMap = r.Str("defaultMap");
  for (const auto& [id, n] : r.Members("maps")) {
    MapDef m;
    m.id = n.Str("id");
    if (m.id != id) n.Child("id").Error("map id differs from its key");
    m.name = n.Str("name");
    m.nameKey = n.Str("nameKey");
    m.cols = n.Int("cols");
    m.rows = n.Int("rows");
    if (n.Has("theme")) {
      m.hasTheme = true;
      m.theme = n.Enum<MapTheme>("theme");
    }
    m.seed = n.Int("seed", 42);
    m.bgColor = n.Str("bgColor", "");
    m.spawns = ReadSpawns(n, /*packs=*/true);
    for (const JNode& c : n.Items("camps")) {
      m.camps.push_back(MapCampDef{TilePos{c.Int("col"), c.Int("row")}, c.StrList("npcs")});
    }
    m.playerStart = n.Tile("playerStart");
    for (const JNode& e : n.Items("exits")) {
      m.exits.push_back(MapExitDef{TilePos{e.Int("col"), e.Int("row")}, e.Str("targetMap"),
                                   TilePos{e.Int("targetCol"), e.Int("targetRow")}});
    }
    ReadRange(n, "levelRange", m.levelMin, m.levelMax);
    if (n.Has("safeZoneRadius")) {
      m.hasSafeZoneRadius = true;
      m.safeZoneRadius = n.Num("safeZoneRadius");
    }
    m.safeZoneRadiusEffective = n.Num("safeZoneRadiusEffective");
    for (const JNode& p : n.Items("petSpawns", true)) {
      m.petSpawns.push_back(PetSpawnDef{TilePos{p.Int("col"), p.Int("row")}, p.Str("petId"), p.Num("chance")});
    }
    for (const JNode& f : n.Items("fieldNpcs", true)) {
      m.fieldNpcs.push_back(FieldNpcDef{TilePos{f.Int("col"), f.Int("row")}, f.Str("npcId")});
    }
    for (const JNode& h : n.Items("hiddenAreas", true)) {
      HiddenAreaDef a;
      a.id = h.Str("id");
      a.name = h.Str("name");
      a.center = TilePos{h.Int("col"), h.Int("row")};
      a.radius = h.Num("radius");
      if (h.Has("startCol")) {
        a.hasBounds = true;
        a.boundsStart = TilePos{h.Int("startCol"), h.Int("startRow")};
        a.boundsEnd = TilePos{h.Int("endCol"), h.Int("endRow")};
      }
      a.discoveryText = h.Str("discoveryText", "");
      for (const JNode& rw : h.Items("rewards")) {
        HiddenRewardDef d;
        d.type = rw.Enum<HiddenRewardType>("type");
        d.value = rw.Str("value", "");
        d.pos = TilePos{rw.Int("col"), rw.Int("row")};
        a.rewards.push_back(std::move(d));
      }
      m.hiddenAreas.push_back(std::move(a));
    }
    for (const JNode& s : n.Items("subDungeonEntrances", true)) {
      SubDungeonEntranceDef e{s.Str("id"),           s.Str("name"),
                              TilePos{s.Int("col"), s.Int("row")},
                              s.Str("targetSubDungeon"), s.Bool("sealed", false),
                              s.Str("sealedTextKey", "")};
      // K1 (remake schema map.sealedEntrances): a sealed entrance shows its key; only a sealed one has one.
      if (e.sealed && e.sealedTextKey.empty()) {
        s.Child("sealedTextKey").Error("a sealed entrance needs its sealedTextKey");
      }
      if (!e.sealed && !e.sealedTextKey.empty()) {
        s.Child("sealedTextKey").Error("sealedTextKey on an entrance that is not sealed");
      }
      m.subDungeonEntrances.push_back(std::move(e));
    }
    for (const JNode& s : n.Items("storyDecorations", true)) {
      m.storyDecorations.push_back(StoryDecorationDef{s.Str("id"), s.Str("name"), s.Str("description", ""),
                                                      TilePos{s.Int("col"), s.Int("row")}, s.Str("spriteType")});
    }
    // Dark remake presentation (remake schema map.presentationTheme / map.regions): mood themes, render-only.
    if (n.Has("presentationTheme")) {
      m.hasPresentationTheme = true;
      m.presentationTheme = n.Enum<MapTheme>("presentationTheme");
    }
    for (const JNode& g : n.Items("regions", true)) {
      MapRegionDef rg;
      rg.id = g.Str("id");
      rg.center = TilePos{g.Int("col"), g.Int("row")};
      rg.radius = g.Num("radius");
      rg.mood = g.Enum<MapTheme>("mood");
      if (rg.id.empty()) g.Child("id").Error("a mood region needs an id");
      if (!(rg.radius > 0)) g.Child("radius").Error("expected > 0");
      m.regions.push_back(std::move(rg));
    }
    // Dark remake K3 (remake schema map.corpseSeeds): bodies in the corpse registry at zone entry (DataStore checks the
    // monster ids and the tiles).
    for (const JNode& c : n.Items("corpseSeeds", true)) {
      for (const auto& [key, child] : c.SelfMembers(true)) {
        if (key != "col" && key != "row" && key != "monsterId") child.Error("unknown corpse seed field '" + key + "'");
      }
      MapCorpseSeedDef seed;
      seed.pos = TilePos{c.Int("col"), c.Int("row")};
      seed.monsterId = c.Str("monsterId");
      if (seed.monsterId.empty()) c.Child("monsterId").Error("empty monster id");
      m.corpseSeeds.push_back(std::move(seed));
    }
    m.generated = n.Bool("generated");
    if (n.Has("generator")) {
      const JNode g = n.Child("generator");
      m.generatorTheme = g.Enum<MapTheme>("theme");
      m.generatorSeed = g.Int("seed");
      for (const JNode& a : g.Items("avoid")) {
        AvoidPointDef p;
        p.pos = TilePos{a.Int("col"), a.Int("row")};
        if (a.Has("margin")) {
          p.hasMargin = true;
          p.margin = a.Int("margin");
        }
        m.generatorAvoid.push_back(p);
      }
      m.avoidMarginDefault = g.Int("avoidMarginDefault");
    } else if (m.generated) {
      n.Error("generated map without generator inputs");
    }
    m.tiles = n.StrList("tiles", true);
    m.collisions = n.StrList("collisions", true);
    for (const JNode& d : n.Items("decorations", true)) {
      m.decorations.push_back(DecorationDef{TilePos{d.Int("col"), d.Int("row")}, d.Str("type")});
    }
    for (size_t i = 0; i < out.mapOrder.size(); ++i) {
      if (out.mapOrder[i] == m.id) m.orderIndex = static_cast<int32_t>(i);
    }
    out.maps.push_back(std::move(m));
  }
  for (const auto& [id, n] : r.Members("subDungeons")) {
    SubDungeonDef s;
    s.id = n.Str("id");
    if (s.id != id) n.Child("id").Error("sub-dungeon id differs from its key");
    s.name = n.Str("name");
    s.parentZone = n.Str("parentZone");
    s.cols = n.Int("cols");
    s.rows = n.Int("rows");
    s.theme = n.Enum<MapTheme>("theme");
    s.seed = n.Int("seed");
    s.spawns = ReadSpawns(n, /*packs=*/false);
    s.miniBossPos = n.Tile("miniBoss");
    s.miniBossId = n.Child("miniBoss").Str("monsterId");
    s.playerStart = n.Tile("playerStart");
    const JNode e = n.Child("exit");
    s.exitPos = TilePos{e.Int("col"), e.Int("row")};
    s.returnPos = TilePos{e.Int("returnCol"), e.Int("returnRow")};
    ReadRange(n, "levelRange", s.levelMin, s.levelMax);
    s.bgColor = n.Str("bgColor", "");
    out.subDungeons.push_back(std::move(s));
  }
  const JNode t = r.Child("emberTower");
  out.emberTower.size = t.Int("size");
  out.emberTower.center = t.Tile("center");
  out.emberTower.portal = t.Tile("portal");
  out.emberTower.meadow = t.Tile("meadow");
  out.emberTower.meadowRadius = t.Child("meadow").Num("radius");
  out.emberTower.plots.clear();
  for (const auto& [id, p] : t.Members("plots")) {
    out.emberTower.plots.push_back(EmberTowerPlotDef{id, p.AsTile(), p.Tile("npc")});
  }
}

void LoadMapGenFile(const JNode& r, WorldTables& out) {
  MapGenDef& g = out.mapGen;
  const JNode ids = r.Child("tileIds");
  static constexpr std::pair<std::string_view, int32_t> kExpected[] = {
      {"grass", 0}, {"dirt", 1}, {"stone", 2}, {"water", 3}, {"wall", 4}, {"camp", 5}, {"campWall", 6}};
  for (const auto& [name, id] : kExpected) {
    if (ids.Int(name, -1) != id) ids.Child(name).Error("tile id differs from the core's Tile enum");
  }
  g.blockingTiles = r.IntList("blockingTiles");
  for (const auto& [k, n] : r.Members("themes")) {
    MapTheme th{};
    if (!ParseEnum(k, th)) {
      n.Error("unknown theme");
      continue;
    }
    MapThemeDef& d = g.themes[EnumIndex(th)];
    d.primaryTile = n.Int("primaryTile");
    d.secondaryTile = n.Int("secondaryTile");
    d.wallDensity = n.Num("wallDensity");
    ReadRange(n, "waterLakeCount", d.lakeCountMin, d.lakeCountMax);
    ReadRange(n, "waterLakeSize", d.lakeSizeMin, d.lakeSizeMax);
    d.decorTypes = n.StrList("decorTypes");
    d.decorDensity = n.Num("decorDensity");
  }
  for (const auto& [k, n] : r.Members("decorPools")) {
    MapTheme th{};
    if (!ParseEnum(k, th)) {
      n.Error("unknown theme");
      continue;
    }
    MapThemeDef& d = g.themes[EnumIndex(th)];
    d.grove = ReadWeighted(n.Child("grove"));
    d.open = ReadWeighted(n.Child("open"));
    d.tall = n.StrList("tall");
  }
  g.defaultTheme = r.Child("defaults").Enum<MapTheme>("theme");
  g.defaultSeed = r.Child("defaults").Int("seed");
  g.defaultAvoidMargin = r.Child("defaults").Int("avoidMargin");
  const JNode lm = r.Child("landmarkRules");
  g.landmarkLoreMargin = lm.Child("lore").Int("margin");
  g.landmarkMiniBossMargin = lm.Child("miniBoss").Int("margin");
  const JNode q = lm.Child("quest");
  g.landmarkQuestObjectiveLocation = q.Int("objectiveLocation");
  g.landmarkQuestGatherArea = q.Int("gatherArea");
  g.landmarkQuestArea = q.Int("questArea");
  g.landmarkQuestDefendTarget = q.Int("defendTarget");
  g.landmarkQuestEscortStart = q.Int("escortStart");
  g.landmarkQuestEscortDest = q.Int("escortDest");
  g.landmarkQuestClue = q.Int("clue");
  g.rngModulus = r.Child("rng").I64("modulus");
  g.rngMultiplier = r.Child("rng").I64("multiplier");
}

void LoadWorldConstantsFile(const JNode& r, WorldTables& out) {
  WorldConstants& w = out.constants;
  w.tileWidthPx = r.Child("tile").Num("widthPx");
  w.tileHeightPx = r.Child("tile").Num("heightPx");
  w.portUnitsPerTile = r.Child("tile").Num("portUnitsPerTile");
  const JNode c = r.Child("camera");
  w.cameraYawDeg = c.Num("yawDeg");
  w.cameraPitchDeg = c.Num("pitchDeg");
  w.cameraFovDeg = c.Num("fovDeg");
  const std::vector<double> framing = c.NumList("framingTiles");
  if (framing.size() == 2) {
    w.cameraFramingTilesW = framing[0];
    w.cameraFramingTilesH = framing[1];
  } else {
    c.Child("framingTiles").Error("expected [w, h]");
  }
  const std::vector<double> zoom = c.NumList("zoomRange");
  if (zoom.size() == 2) {
    w.cameraZoomMin = zoom[0];
    w.cameraZoomMax = zoom[1];
  } else {
    c.Child("zoomRange").Error("expected [min, max]");
  }
  w.cameraFollowLagSec = c.Num("followLagSec");
  const JNode h = r.Child("heroSpeed");
  w.heroMoveSpeed = h.Num("moveSpeed");
  w.heroPxPerTile = h.Num("pxPerTile");
  w.heroRampMs = h.Num("rampMs");
  w.heroStopMs = h.Num("stopMs");
  w.heroClickStartsInstantly = h.Bool("clickStartsInstantly");
  w.holdMoveRepathMs = r.Num("holdMoveRepathMs");
  w.exitRadiusSq = r.Num("exitRadiusSq");
  w.exitArmDistance = r.Num("exitArmDistance");
  w.exitArmDistanceSq = r.Num("exitArmDistanceSq");
  w.campfireRadiusTiles = r.Child("campfire").Num("radiusTiles");
  w.campfireHpMul = r.Child("campfire").Num("hpMul");
  w.campfireMpMul = r.Child("campfire").Num("mpMul");
  w.safeZoneRadiusDefault = r.Num("safeZoneRadiusDefault");
  w.hiddenAreaExploreRadius = r.Num("hiddenAreaExploreRadius");
  w.escortCatchUpTiles = r.Num("escortCatchUpTiles");
  const JNode tp = r.Child("townPortal");
  w.townPortalChannelMs = tp.Num("channelMs");
  w.townPortalCancelOnMove = tp.Bool("cancelOnMove");
  w.townPortalCancelOnDamageFraction = tp.Num("cancelOnDamageFraction");
  w.deathRespawnMs = r.Num("deathRespawnMs");
  w.zoneFadeInMs = r.Num("zoneFadeInMs");
  const JNode i = r.Child("interact");
  w.lootRadiusSq = i.Num("lootRadiusSq");
  w.hiddenChestRadiusSq = i.Num("hiddenChestRadiusSq");
  w.puzzleRadiusSq = i.Num("puzzleRadiusSq");
  w.npcRange = i.Num("npcRange");
  w.npcPickRadiusSq = i.Num("npcPickRadiusSq");
  w.entranceRadiusSq = i.Num("entranceRadiusSq");
  w.touchTalkRange = i.Num("touchTalkRange");
  w.walkThenAct = i.Bool("walkThenAct");
  w.sealedGateExitTo = r.Child("sealedChapter2Gate").Str("exitTo");
  w.sealedGateMessageKey = r.Child("sealedChapter2Gate").Str("messageKey");
}

void LoadRandomEventsFile(const JNode& r, WorldTables& out) {
  RandomEventsDef& e = out.randomEvents;
  e.types.clear();
  e.zones.clear();
  const JNode c = r.Child("config");
  e.cooldownMs = c.Num("cooldownMs");
  e.safeZoneRadius = c.Num("safeZoneRadius");
  e.minEventsPerWindow = c.Int("minEventsPerWindow");
  e.maxEventsPerWindow = c.Int("maxEventsPerWindow");
  e.frequencyWindowMs = c.Num("frequencyWindowMs");
  e.triggerMoveThresholdTiles = r.Num("triggerMoveThresholdTiles");
  for (const JNode& d : r.Items("defs")) {
    e.types.push_back(RandomEventTypeDef{d.Enum<RandomEventType>("type"), d.Num("weight"), d.Str("name"),
                                         d.Str("message")});
  }
  for (const auto& [zone, z] : r.Members("zones")) {
    ZoneEventDataDef d;
    d.zoneId = zone;
    d.ambushMonsters = z.StrList("ambushMonsters");
    ReadRange(z, "ambushCount", d.ambushCountMin, d.ambushCountMax);
    d.merchantItems = z.StrList("merchantItems");
    for (const JNode& p : z.Items("puzzleDescriptions")) {
      d.puzzles.push_back(PuzzleDef{p.Str("prompt"), p.Str("solution"), p.Str("reward"), p.I64("rewardGold"),
                                    p.I64("rewardExp")});
    }
    d.puzzleSpriteKey = z.Str("puzzleSpriteKey");
    d.rescueNpcName = z.Str("rescueNpcName");
    d.rescueNpcSpriteKey = z.Str("rescueNpcSpriteKey");
    if (z.Has("rescueMonsters")) d.rescueMonsters = z.StrList("rescueMonsters");
    if (z.Has("ambushChampion")) {  // dark remake K6 (remake schema events.ambushChampion)
      const JNode ch = z.Child("ambushChampion");
      ReadSpawnCheckKeys(ch, {"chance", "mix", "leader", "sharedAffix", "leaderLoot", "notFlag"}, "ambush champion");
      ZoneEventDataDef::AmbushChampionDef& a = d.ambushChampion;
      a.present = true;
      a.chance = ch.Num("chance");
      if (!(a.chance > 0 && a.chance <= 1)) ch.Child("chance").Error("expected 0 < chance <= 1");
      int32_t total = 0;
      for (const JNode& me : ch.Items("mix")) {
        ReadSpawnCheckKeys(me, {"monsterId", "count"}, "ambush champion mix");
        MapSpawnMixDef m;
        m.monsterId = me.Str("monsterId");
        m.count = me.Int("count");
        if (m.monsterId.empty()) me.Child("monsterId").Error("empty monster id");
        if (m.count < 1) me.Child("count").Error("expected >= 1");
        total = (std::min)(total + (std::clamp)(m.count, 0, ZoneEventDataDef::kChampionMaxMembers + 1),
                           ZoneEventDataDef::kChampionMaxMembers + 1);  // no overflow
        a.mix.push_back(std::move(m));
      }
      if (a.mix.empty()) ch.Child("mix").Error("a champion pack needs members");
      if (total > ZoneEventDataDef::kChampionMaxMembers) {
        ch.Child("mix").Error("more than " + std::to_string(ZoneEventDataDef::kChampionMaxMembers) + " members");
      }
      a.leader = ch.Str("leader", "");
      if (ch.Has("leader")) {
        bool member = false;
        for (const MapSpawnMixDef& m : a.mix) member = member || m.monsterId == a.leader;
        if (!member) ch.Child("leader").Error("'" + a.leader + "' is not a member of the pack");
      }
      a.sharedAffix = ch.Bool("sharedAffix", false);
      if (ch.Has("leaderLoot")) {
        const JNode ll = ch.Child("leaderLoot");
        if (!ParseEnum(ll.AsStr(), a.leaderLoot) ||
            (a.leaderLoot != ItemQuality::Magic && a.leaderLoot != ItemQuality::Rare)) {
          ll.Error("expected magic | rare");
          a.leaderLoot = ItemQuality::Normal;
        } else if (a.leader.empty()) {
          ll.Error("a guaranteed leader drop needs a leader");
        }
      }
      if (ch.Has("notFlag")) {  // 2F review (remake schema events.ambushVariants)
        a.notFlag = ch.Str("notFlag");
        if (a.notFlag.empty()) ch.Child("notFlag").Error("an empty flag id");
      }
    }
    if (z.Has("ambushVariants")) {  // 2F review (remake schema events.ambushVariants)
      for (const JNode& v : z.Items("ambushVariants")) {
        ReadSpawnCheckKeys(v, {"whenFlag", "monsters"}, "ambush variant");
        ZoneEventDataDef::AmbushVariantDef av;
        av.whenFlag = v.Str("whenFlag");
        av.monsters = v.StrList("monsters");
        if (av.whenFlag.empty()) v.Child("whenFlag").Error("an empty flag id");
        if (av.monsters.empty()) v.Child("monsters").Error("an ambush variant names its monsters");
        for (const std::string& id : av.monsters) {
          if (id.empty()) v.Child("monsters").Error("an empty monster id");
        }
        d.ambushVariants.push_back(std::move(av));
      }
    }
    d.rescueRewardGold = z.Child("rescueReward").I64("gold");
    d.rescueRewardExp = z.Child("rescueReward").I64("exp");
    e.zones.push_back(std::move(d));
  }
  e.resetMoveCounterAfterEveryRoll = r.Child("port").Bool("resetMoveCounterAfterEveryRoll", true);
}

void LoadZoneMoodsFile(const JNode& r, WorldTables& out) {
  ZoneMoodTables& m = out.moods;
  m.themeByZone.clear();
  m.weather.clear();
  for (const auto& [zone, n] : r.Members("themeByZone")) m.themeByZone.emplace_back(zone, n.AsEnum<MapTheme>());
  for (const auto& [k, n] : r.Members("moods")) {
    MapTheme th{};
    if (!ParseEnum(k, th)) {
      n.Error("unknown theme");
      continue;
    }
    ZoneMoodDef& d = m.moods[EnumIndex(th)];
    d.theme = th;
    d.defined = true;
    d.ambient = n.Color("ambient");
    d.ambientAlpha = n.Num("ambientAlpha");
    d.vignette = n.Color("vignette");
    d.vignetteAlpha = n.Num("vignetteAlpha");
    d.haze = n.Color("haze");
    d.hazeAlpha = n.Num("hazeAlpha");
    d.saturation = n.Num("saturation");
    d.contrast = n.Num("contrast");
    const std::vector<double> lift = n.NumList("lift");
    const std::vector<double> gain = n.NumList("gain");
    for (size_t i = 0; i < 3 && i < lift.size(); ++i) d.lift[i] = lift[i];
    for (size_t i = 0; i < 3 && i < gain.size(); ++i) d.gain[i] = gain[i];
    // P14 fog defaults (overridden by port.fog below): halfway between the haze and the ambient colour, per sRGB channel.
    d.fogColor = 0;
    for (const int shift : {16, 8, 0}) {
      const uint32_t mixed = (((d.haze >> shift) & 0xFFu) + ((d.ambient >> shift) & 0xFFu) + 1u) / 2u;
      d.fogColor |= mixed << shift;
    }
    d.fogMaxOpacity = 0.25;
    d.fogDensity = 0.9;
  }
  // DECISIONS P14 (art pass): the UE height fog per mood theme; every field optional (defaults above).
  for (const auto& [k, n] : r.Child("port").Members("fog", /*allowMissing*/ true)) {
    MapTheme th{};
    if (!ParseEnum(k, th)) {
      n.Error("unknown theme");
      continue;
    }
    ZoneMoodDef& d = m.moods[EnumIndex(th)];
    if (n.Has("color")) d.fogColor = n.Color("color");
    d.fogMaxOpacity = n.Num("maxOpacity", d.fogMaxOpacity);
    d.fogDensity = n.Num("density", d.fogDensity);
    if (d.fogMaxOpacity < 0.0 || d.fogMaxOpacity > 1.0) n.Child("maxOpacity").Error("expected 0..1");
    if (d.fogDensity < 0.0) n.Child("density").Error("expected >= 0");
  }
  // Dark remake: per-mood R18 light-scale overrides and the weather a mood brings (both optional, render-only).
  for (const auto& [k, n] : r.Child("port").Members("lightScale", /*allowMissing*/ true)) {
    MapTheme th{};
    if (!ParseEnum(k, th)) {
      n.Error("unknown theme");
      continue;
    }
    ZoneMoodDef& d = m.moods[EnumIndex(th)];
    d.lightScale = n.AsNum();
    if (!(d.lightScale > 0.0 && d.lightScale <= 1.0)) n.Error("expected 0 < lightScale <= 1");
  }
  for (const auto& [k, n] : r.Child("port").Members("moodWeather", /*allowMissing*/ true)) {
    MapTheme th{};
    if (!ParseEnum(k, th)) {
      n.Error("unknown theme");
      continue;
    }
    ZoneMoodDef& d = m.moods[EnumIndex(th)];
    d.hasMoodWeather = true;
    d.moodWeather.type = n.Str("type");
    d.moodWeather.ambience = n.Str("ambience", "");
    if (n.Has("tint")) {
      d.moodWeather.hasTint = true;
      d.moodWeather.tint = n.Color("tint");
    }
  }
  for (const auto& [zone, n] : r.Members("weather")) {
    ZoneWeatherDef w;
    w.zoneId = zone;
    w.type = n.Str("type");
    w.ambience = n.Str("ambience", "");
    if (n.Has("tint")) {
      w.hasTint = true;
      w.tint = n.Color("tint");
    }
    m.weather.push_back(std::move(w));
  }
  // palettes / lights / campThemes / ambientDustTint are render-only (read by UE from RawTable if needed).
}

}  // namespace abyss::dataload

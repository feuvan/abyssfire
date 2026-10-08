#include "abyss/base/Platform.h"

#include <algorithm>

#include "JsonReader.h"

namespace abyss::dataload {
namespace {

SfxId Cue(const JNode& n) { return n.AsEnum<SfxId>(); }

}  // namespace

void LoadAudioCuesFile(const JNode& r, AudioTables& out) {
  out.cues.clear();
  for (const JNode& c : r.Items("cues")) {
    AudioCueDef d;
    d.id = c.Enum<SfxId>("id");
    d.lengthSec = c.Num("lengthSec");
    d.variants = c.Int("variants");
    d.webReachable = c.Bool("webReachable");
    d.assets = c.StrList("assets");
    d.bus = c.Enum<AudioBus>("bus");
    const std::string sp = c.Str("spatial");
    if (sp != "2d" && sp != "3d") c.Child("spatial").Error("expected 2d or 3d");
    d.spatial3d = sp == "3d";
    d.concurrencyMax = c.Child("concurrency").Int("max");
    d.concurrencyRule = c.Child("concurrency").Str("rule");
    d.minRetriggerMs = c.Child("concurrency").Num("minRetriggerMs");
    d.families = c.EnumList<AnimRig>("families", true);
    out.cues.push_back(std::move(d));
  }
  const JNode ru = r.Child("rules");
  AudioRulesDef& a = out.rules;
  a.combatDodged = Cue(ru.Child("combatDamage").Child("dodged"));
  a.combatCrit = Cue(ru.Child("combatDamage").Child("crit"));
  a.combatHit = Cue(ru.Child("combatDamage").Child("hit"));
  for (size_t i = 0; i < EnumCount<ItemQuality>(); ++i) {
    a.itemPickedByQuality[i] = Cue(ru.Child("itemPickedByQuality").Child(EnumName(static_cast<ItemQuality>(i))));
  }
  for (size_t i = 0; i < EnumCount<DamageType>(); ++i) {
    a.skillUsedByDamageType[i] = Cue(ru.Child("skillUsedByDamageType").Child(EnumName(static_cast<DamageType>(i))));
  }
  a.playerLevelUp = Cue(ru.Child("playerLevelUp"));
  a.playerDied = Cue(ru.Child("playerDied"));
  a.dodgeStarted = Cue(ru.Child("dodgeStarted"));
  a.resonanceStarted = Cue(ru.Child("resonanceStarted"));
  a.monsterDied = Cue(ru.Child("monsterDied"));
  a.questCompleted = Cue(ru.Child("questCompleted"));
  a.questAccepted = Cue(ru.Child("questAccepted"));
  a.questTurnedIn = Cue(ru.Child("questTurnedIn"));
  a.npcInteract = Cue(ru.Child("npcInteract"));
  a.shopOpen = Cue(ru.Child("shopOpen"));
  a.inventoryOpen = Cue(ru.Child("inventoryOpen"));
  a.inventoryClose = Cue(ru.Child("inventoryClose"));
  a.uiTogglePanel = Cue(ru.Child("uiTogglePanel"));
  const JNode qp = ru.Child("questProgress");
  a.questObjectiveDone = Cue(qp.Child("objectiveDone"));
  a.questMaterialOrClueStep = Cue(qp.Child("materialOrClueStep"));
  a.questProgressPrefixes = qp.StrList("progressPrefixes");
  const JNode di = ru.Child("direct");
  a.townPortalComplete = Cue(di.Child("townPortalComplete"));
  a.soulEchoReclaimed = Cue(di.Child("soulEchoReclaimed"));
  a.forgeSuccess = Cue(di.Child("forgeSuccess"));
  a.forgeError = Cue(di.Child("forgeError"));
  const JNode port = ru.Child("port");
  a.heavyHitCue = Cue(port.Child("heavyHitCue").Child("cue"));
  a.heroDamageTakenCue = Cue(port.Child("heroDamageTakenCue").Child("cue"));
  a.monsterAggro = Cue(port.Child("monsterAggro").Child("cue"));
  a.monsterHurt = Cue(port.Child("monsterHurt").Child("cue"));
  a.spatialSpread = r.Child("spatial").Num("spread");
}

void LoadMusicFile(const JNode& r, AudioTables& out) {
  MusicDirectorDef& m = out.music;
  const JNode d = r.Child("director");
  m.zoneFadeSec = d.Num("zoneFadeSec");
  m.stateFadeSec = d.Num("stateFadeSec");
  m.fadeInSec = d.Num("fadeInSec");
  m.victoryHoldMs = d.Num("victoryHoldMs");
  m.combatOffDelayMs = d.Num("combatOffDelayMs");
  const JNode p = d.Child("port");
  m.bossVictoryHoldMs = p.Num("bossVictoryHoldMs");
  m.trueDebounce = p.Bool("trueDebounce");
  m.exploreResumesPosition = p.Bool("exploreResumesPosition");
  m.bossCh1Score = p.Str("bossCh1Score");
  m.defaultMusicVolume = r.Child("portSettingsDefaults").Num("musicVolume");
  m.defaultSfxVolume = r.Child("portSettingsDefaults").Num("sfxVolume");
  m.themeZones.clear();
  for (const auto& [k, n] : r.Members("themes")) m.themeZones.push_back(k);
}

void LoadAssetsFile(const JNode& r, AssetManifest& out) {
  out = AssetManifest{};
  const JNode m = r.Child("manifest");
  if (!m.Exists()) {
    r.Error("assets.json has no 'manifest'");
    return;
  }
  out.loaded = true;
  out.schemaVersion = m.Int("schemaVersion", 0);
  for (const auto& [name, a] : m.Members("assets", true)) {
    AssetEntryDef e;
    e.name = name;
    e.kind = a.Str("kind", "");
    e.category = a.Str("category", "");
    e.gameIds = a.StrList("gameIds", true);
    e.skeleton = a.Str("skeleton", "");
    e.scale = a.Num("scale", 1.0);
    e.heightCm = a.Num("heightCm", 0.0);
    for (const JNode& c : a.Items("anims", true)) {
      AnimClipDef clip;
      clip.name = c.Str("name");
      clip.asset = c.Str("asset", "");
      clip.lengthMs = c.Num("lengthMs", 0.0);
      clip.frames = c.Int("frames", 0);
      clip.fps = c.Num("fps", 0.0);
      clip.loop = c.Bool("loop", false);
      if (c.Has("contactMs")) {
        clip.hasContactMs = true;
        clip.contactMs = c.Num("contactMs");
      }
      if (c.Has("releaseMs")) {
        clip.hasReleaseMs = true;
        clip.releaseMs = c.Num("releaseMs");
      }
      clip.additive = c.Bool("additive", false);
      clip.refSpeedCmS = c.Num("refSpeedCmS", 0.0);
      for (const JNode& nt : c.Items("notifies", true)) clip.notifies.push_back(AnimNotifyDef{nt.Str("name"), nt.Num("ms")});
      e.anims.push_back(std::move(clip));
    }
    if (a.Has("footprintTiles")) {
      const std::vector<int32_t> fp = a.IntList("footprintTiles");
      if (fp.size() == 2) {
        e.hasFootprint = true;
        e.footprintW = fp[0];
        e.footprintH = fp[1];
      } else {
        a.Child("footprintTiles").Error("expected [w, h]");
      }
    }
    if (a.Has("blocking")) {
      e.hasBlocking = true;
      e.blocking = a.Bool("blocking");
    }
    for (const std::string& gid : e.gameIds) out.gameIdToAsset.emplace_back(gid, e.name);
    out.assets.push_back(std::move(e));
  }
  std::stable_sort(out.gameIdToAsset.begin(), out.gameIdToAsset.end(),
                   [](const auto& x, const auto& y) { return x.first < y.first; });
}

void LoadUiThemeFile(const JNode& r, UiThemeDef& out) {
  out = UiThemeDef{};
  const JNode d = r.Child("display");
  out.designWidth = d.Int("designWidth");
  out.designHeight = d.Int("designHeight");
  out.background = d.Str("background");
  out.defaultLocale = d.Enum<LocaleId>("defaultLocale");
  out.locales = d.EnumList<LocaleId>("locales");
  out.fontBody = r.Child("fonts").Str("body");
  out.fontTitle = r.Child("fonts").Str("title");
  for (const auto& [k, n] : r.Members("colors")) {
    if (n.V().IsString()) {
      out.colors.emplace_back(k, n.AsStr());
    } else {
      out.colorNums.emplace_back(k, n.AsColor());
    }
  }
  for (size_t i = 0; i < EnumCount<ItemQuality>(); ++i) {
    out.qualityHex[i] = r.Child("qualityHex").Str(EnumName(static_cast<ItemQuality>(i)));
  }
  const JNode t = r.Child("touch");
  for (const auto& [k, n] : t.Members("cssPx")) out.touchCssPx.emplace_back(k, n.AsNum());
  const std::vector<double> sc = t.NumList("scaleClamp");
  if (sc.size() == 2) {
    out.touchScaleMin = sc[0];
    out.touchScaleMax = sc[1];
  } else {
    t.Child("scaleClamp").Error("expected [min, max]");
  }
  out.touchPointers = t.Int("pointers");
  out.minTouchTargetPt = t.Num("minTouchTargetPt");
}

void LoadRenderQualityFile(const JNode& r, RenderQualityTables& out) {
  for (const auto& [k, n] : r.Members("profiles")) {
    RenderQuality q{};
    if (!ParseEnum(k, q)) {
      n.Error("unknown quality");
      continue;
    }
    RenderQualityProfileDef& p = out.profiles[EnumIndex(q)];
    p.quality = n.Enum<RenderQuality>("quality");
    p.maxDynamicLights = n.Int("maxDynamicLights");
    p.lightingUpdateIntervalMs = n.Num("lightingUpdateIntervalMs");
    p.particleFrequencyMultiplier = n.Num("particleFrequencyMultiplier");
    p.bloom = n.Bool("bloom");
    p.colorGrading = n.Bool("colorGrading");
    p.resolutionScale = r.Child("resolutionScale").Num(k);
  }
  const JNode s = r.Child("selection");
  out.lowPixelBudget = s.Num("lowPixelBudget");
  out.highMaxPixelBudget = s.Num("highMaxPixelBudget");
  out.highMinDpr = s.Num("highMinDpr");
  out.constrainedCores = s.Int("constrainedCores");
  out.constrainedMemoryGb = s.Num("constrainedMemoryGb");
}

void LoadAbyssRunFile(const JNode& r, AbyssRunTables& out) {
  out = AbyssRunTables{};
  for (const JNode& b : r.Items("boons")) {
    out.boons.push_back(BoonDef{b.Str("id"), b.Enum<BoonRarity>("rarity"), b.Stats("stats"), b.Int("maxStacks", 0),
                                b.Str("glyph", "")});
  }
  for (const auto& [k, n] : r.Members("boonRarityWeight")) {
    BoonRarity rr{};
    if (!ParseEnum(k, rr)) {
      n.Error("unknown rarity");
      continue;
    }
    out.boonRarityWeight[EnumIndex(rr)] = n.AsNum();
  }
  for (const JNode& c : r.Items("curses")) {
    CurseDef d;
    d.id = c.Str("id");
    d.speedMul = c.Num("speedMul", 1.0);
    d.damageMul = c.Num("damageMul", 1.0);
    d.hpMul = c.Num("hpMul", 1.0);
    d.extraGroups = c.Int("extraGroups", 0);
    d.eliteChance = c.Num("eliteChance", 0.0);
    d.visionMul = c.Num("visionMul", 1.0);
    d.regenPerSec = c.Num("regenPerSec", 0.0);
    d.deathBurst = c.Num("deathBurst", 0.0);
    d.lootBonus = c.Num("lootBonus", 0.0);
    d.magicFind = c.Num("magicFind", 0.0);
    out.curses.push_back(std::move(d));
  }
  for (const JNode& f : r.Items("floorThemes")) {
    out.floorThemes.push_back(
        FloorThemeDef{f.Str("id"), f.Enum<MapTheme>("mapTheme"), f.StrList("monsters"), f.Str("gatekeeper")});
  }
  out.tierBaseLevel = r.Int("tierBaseLevel");
  out.tierLevelStep = r.Int("tierLevelStep");
}

}  // namespace abyss::dataload

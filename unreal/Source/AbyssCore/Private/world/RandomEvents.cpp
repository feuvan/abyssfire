// Random world events (world-map-nav.md section 13; W6, M4/W9, W11): src/systems/RandomEventSystem.ts (trigger rules,
// createEvent draws) and the ZoneScene glue (handle* per type). Owner area: world.
#include "abyss/base/Platform.h"

#include "abyss/world/RandomEvents.h"

#include <algorithm>
#include <cmath>

#include "abyss/base/I18n.h"
#include "abyss/base/Math.h"
#include "abyss/base/SimClock.h"
#include "abyss/base/StrUtil.h"
#include "abyss/combat/Combat.h"
#include "abyss/data/DataStore.h"
#include "abyss/hero/Hero.h"
#include "abyss/hero/Rewards.h"
#include "abyss/items/GroundLoot.h"
#include "abyss/items/Inventory.h"
#include "abyss/items/LootGen.h"
#include "abyss/items/Shop.h"
#include "abyss/monsters/MonsterSystem.h"
#include "abyss/sim/Commands.h"
#include "abyss/sim/SimContext.h"
#include "abyss/sim/Snapshot.h"
#include "abyss/world/Zone.h"

namespace abyss {

namespace randomevents_impl {

constexpr double kRevRescuePollMs = 500;     // 13.3: completion check loop
constexpr double kRevChestFadeMs = 8000 + 1200;  // opened cache chest: fade after 8 s (1.2 s fade)
constexpr int32_t kRevWalkableRadius = 5;    // findWalkableTile default maxRadius
constexpr double kRevAmbushMinDist = 3, kRevAmbushDistRange = 2;
constexpr double kRevRescueMinDist = 2, kRevRescueDistRange = 3;
// createEvent fallbacks when a zone has no event data (RandomEventSystem.ts:383-431).
constexpr int32_t kRevAmbushFallbackMin = 3, kRevAmbushFallbackMax = 5;
constexpr int32_t kRevRescueFallbackMin = 2, kRevRescueFallbackMax = 4;
constexpr int64_t kRevRescueFallbackGold = 50, kRevRescueFallbackExp = 40;
// The fallback puzzle createEvent builds when the zone has no puzzle (sys.event.puzzle.fallback.*, 50 gold, 30 exp).
constexpr int64_t kRevPuzzleFallbackGold = 50, kRevPuzzleFallbackExp = 30;

// i18n key of a puzzle text field: the zone's puzzle (sys.event.puzzle.<zone>.<field>) or the fallback puzzle.
std::string RevPuzzleKey(const std::string& zoneId, int32_t puzzleIndex, const char* field) {
  if (puzzleIndex < 0) return std::string("sys.event.puzzle.fallback.") + field;
  return "sys.event.puzzle." + zoneId + "." + field;
}

const char* RevPropArt(RandomEventType t, const ZoneEventDataDef* zd) {
  switch (t) {
    case RandomEventType::TreasureCache: return "decor_treasure_chest";
    case RandomEventType::WanderingMerchant: return "npc_wandering_merchant";
    case RandomEventType::Rescue: return zd != nullptr ? zd->rescueNpcSpriteKey.c_str() : "npc_rescue";
    case RandomEventType::EnvironmentalPuzzle:
      return zd != nullptr ? zd->puzzleSpriteKey.c_str() : "decor_puzzle_stone";
    case RandomEventType::Ambush: break;
  }
  return "";
}

}  // namespace randomevents_impl

// ---------------------------------------------------------------------------------------------------------------------
// RandomEventRules (pure)
// ---------------------------------------------------------------------------------------------------------------------
void RandomEventRules::Reset() {
  lastEventMs_ = -1e300;
  history_.clear();
  explorationMs_ = 0;
  moved_ = 0;
  hasLast_ = false;
  lastPos_ = Vec2();
}

int32_t RandomEventRules::EventsInWindow(const RandomEventsDef& def, double nowMs) const {
  const double windowStart = nowMs - def.frequencyWindowMs;
  int32_t n = 0;
  for (double t : history_) n += t >= windowStart ? 1 : 0;
  return n;
}

double RandomEventRules::TriggerChance(const RandomEventsDef& def, int32_t eventsInWindow, double explorationMs) {
  double base = 0.07;
  if (eventsInWindow < def.minEventsPerWindow) {
    const double elapsed = (std::min)(explorationMs, def.frequencyWindowMs);
    const double progress = elapsed / def.frequencyWindowMs;
    if (progress > 0.3) base += 0.05 * progress;
  }
  if (eventsInWindow >= def.maxEventsPerWindow - 1) base *= 0.3;
  return (std::min)(0.25, (std::max)(0.02, base));
}

RandomEventType RandomEventRules::PickType(const RandomEventsDef& def, double roll01) {
  if (def.types.empty()) return RandomEventType::Ambush;
  double total = 0;
  for (const RandomEventTypeDef& d : def.types) total += d.weight;
  double roll = roll01 * total;
  for (const RandomEventTypeDef& d : def.types) {
    roll -= d.weight;
    if (roll <= 0) return d.type;
  }
  return def.types.back().type;
}

bool RandomEventRules::Update(const RandomEventsDef& def, const RandomEventTriggerInput& in, Rng& rng,
                              RandomEventType& outType) {
  const bool moved = !hasLast_ || in.heroPos.x != lastPos_.x || in.heroPos.y != lastPos_.y;
  if (moved) {
    explorationMs_ += in.dtMs;
    const Vec2 last = hasLast_ ? lastPos_ : in.heroPos;  // the first call primes the position (no jump)
    moved_ += (std::max)(std::fabs(in.heroPos.x - last.x), std::fabs(in.heroPos.y - last.y));
    lastPos_ = in.heroPos;
    hasLast_ = true;
  }
  if (!moved || in.inCombat || in.eventPending) return false;
  if (in.inSafeZone) return false;
  if (in.nowMs - lastEventMs_ < def.cooldownMs) return false;
  if (moved_ < def.triggerMoveThresholdTiles) return false;
  const double windowStart = in.nowMs - def.frequencyWindowMs;
  history_.erase(std::remove_if(history_.begin(), history_.end(), [windowStart](double t) { return t < windowStart; }),
                 history_.end());
  if (static_cast<int32_t>(history_.size()) >= def.maxEventsPerWindow) return false;
  const double chance = TriggerChance(def, static_cast<int32_t>(history_.size()), explorationMs_);
  const double roll = rng.Float01();
  if (def.resetMoveCounterAfterEveryRoll) moved_ = 0;  // W6
  if (roll > chance) return false;
  moved_ = 0;
  outType = PickType(def, rng.Float01());
  lastEventMs_ = in.nowMs;
  history_.push_back(in.nowMs);
  return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// RandomEventSystem (runtime)
// ---------------------------------------------------------------------------------------------------------------------
RandomEventSystem::RandomEventSystem(SimContext& ctx) : ctx_(ctx) {}

void RandomEventSystem::OnZoneEnter() {
  rules_.Reset();  // GameSession.beginZone: a fresh system per zone (cooldown and history reset)
  events_.clear();
  puzzle_ = PuzzlePromptState{};
}

void RandomEventSystem::OnZoneExit() {
  ClosePuzzle();
  for (ActiveRandomEvent& e : events_) {
    if (e.timer != kNoTimer) ctx_.timers.Cancel(e.timer);
  }
  events_.clear();
}

bool RandomEventSystem::HeroInSafeZone(Vec2 p) const {
  const ZoneRuntime* zone = ctx_.sys.zone;
  if (zone == nullptr || !zone->HasZone()) return true;
  // tileType is read with the float position (W14 parity): only defined on exact integers.
  if (p.x == std::floor(p.x) && p.y == std::floor(p.y)) {
    const TileType t = zone->Grid().Tile(static_cast<int32_t>(p.x), static_cast<int32_t>(p.y));
    if (zone->Grid().InBounds(static_cast<int32_t>(p.x), static_cast<int32_t>(p.y)) &&
        (t == TileType::Camp || t == TileType::CampWall)) {
      return true;
    }
  }
  const double r =
      zone->Map().hasSafeZoneRadius ? zone->Map().safeZoneRadius : ctx_.data.World().randomEvents.safeZoneRadius;
  for (const MapCampDef& c : zone->Map().camps) {
    const double dx = p.x - c.pos.col, dy = p.y - c.pos.row;
    if (std::sqrt(dx * dx + dy * dy) < r) return true;
  }
  return false;
}

bool RandomEventSystem::HasUnresolved() const {
  for (const ActiveRandomEvent& e : events_) {
    if (!e.resolved) return true;
  }
  return false;
}

const ActiveRandomEvent* RandomEventSystem::FindByProp(EntityId prop) const {
  if (prop == kNoEntity) return nullptr;
  for (const ActiveRandomEvent& e : events_) {
    if (e.prop == prop) return &e;
  }
  return nullptr;
}

void RandomEventSystem::Tick() {
  const ZoneRuntime* zone = ctx_.sys.zone;
  Hero& hero = *ctx_.sys.hero;
  if (zone == nullptr || !zone->HasZone()) return;
  // The wandering merchant fades when its own shop closes (13.3).
  for (ActiveRandomEvent& e : events_) {
    if (e.type != RandomEventType::WanderingMerchant || e.prop == kNoEntity) continue;
    const ShopSystem* shop = ctx_.sys.shop;
    const bool open = shop != nullptr && shop->State().open && shop->State().npcId == kWanderingMerchantShopId;
    if (!open) DespawnEventProp(e, DespawnReason::Removed);
  }
  PruneFinished();
  if (hero.Life() != HeroLife::Alive || hero.Hp() <= 0) return;  // checkRandomEvents: dead hero
  RandomEventTriggerInput in;
  in.nowMs = ctx_.Now();
  in.dtMs = kSimStepMs;
  in.heroPos = hero.Position();
  in.inCombat = ctx_.sys.combat != nullptr && ctx_.sys.combat->InCombat();
  in.eventPending = HasUnresolved();
  in.inSafeZone = HeroInSafeZone(in.heroPos);
  RandomEventType type = RandomEventType::Ambush;
  if (rules_.Update(ctx_.data.World().randomEvents, in, ctx_.Rand(RngStream::Events), type)) {
    TriggerEvent(type, in.heroPos);
  }
}

TilePos RandomEventSystem::EventTile(Vec2 pos) const {
  const TilePos rounded = RoundToTile(pos);
  TilePos out;
  if (ctx_.sys.zone != nullptr && ctx_.sys.zone->HasZone() &&
      ctx_.sys.zone->Paths().FindWalkableTile(rounded, randomevents_impl::kRevWalkableRadius, out)) {
    return out;
  }
  return rounded;
}

void RandomEventSystem::SpawnProp(ActiveRandomEvent& e, Vec2 at, const std::string& art) {
  e.prop = ctx_.ids.Next();
  e.propPos = at;
  e.propArt = art;
  ctx_.events.Emit(EvEntitySpawned{e.prop, EntityKind::Prop, std::string(EnumName(e.type)), art, at, Vec2(1, 0), 1.0});
}

void RandomEventSystem::DespawnEventProp(ActiveRandomEvent& e, DespawnReason reason) {
  if (e.prop == kNoEntity) return;
  ctx_.events.Emit(EvEntityDespawned{e.prop, EntityKind::Prop, reason});
  if (puzzle_.open && puzzle_.prop == e.prop) ClosePuzzle();
  e.prop = kNoEntity;
}

void RandomEventSystem::PruneFinished() {
  events_.erase(std::remove_if(events_.begin(), events_.end(),
                               [](const ActiveRandomEvent& e) {
                                 return e.resolved && e.prop == kNoEntity && e.timer == kNoTimer;
                               }),
                events_.end());
}

void RandomEventSystem::Resolve(ActiveRandomEvent& e) {
  if (e.resolved) return;
  e.resolved = true;
  ctx_.events.Emit(EvRandomEvent{true, e.type, e.pos, ctx_.session.currentMap, e.prop});
}

void RandomEventSystem::TriggerEvent(RandomEventType type, Vec2 pos) {
  using namespace randomevents_impl;
  const ZoneRuntime* zone = ctx_.sys.zone;
  if (zone == nullptr || !zone->HasZone()) return;
  const std::string& zoneId = zone->MapId();
  const ZoneEventDataDef* zd = ctx_.data.World().randomEvents.ForZone(zoneId);
  Rng& rng = ctx_.Rand(RngStream::Events);
  ActiveRandomEvent e;
  e.type = type;
  e.pos = pos;
  e.triggeredAtMs = ctx_.Now();
  // createEvent (13.3): draws in order.
  switch (type) {
    case RandomEventType::Ambush: {
      const int32_t mn = zd != nullptr ? zd->ambushCountMin : kRevAmbushFallbackMin;
      const int32_t mx = zd != nullptr ? zd->ambushCountMax : kRevAmbushFallbackMax;
      e.monsterCount = static_cast<int32_t>(std::floor(rng.Float01() * (mx - mn + 1))) + mn;
      break;
    }
    case RandomEventType::Rescue: {
      const int32_t mn = zd != nullptr ? zd->ambushCountMin : kRevRescueFallbackMin;
      const int32_t mx = zd != nullptr ? zd->ambushCountMax : kRevRescueFallbackMax;
      e.monsterCount = (std::max)(2, static_cast<int32_t>(std::floor(rng.Float01() * (mx - mn + 1))) + mn - 1);
      e.rewardGold = zd != nullptr ? zd->rescueRewardGold : kRevRescueFallbackGold;
      e.rewardExp = zd != nullptr ? zd->rescueRewardExp : kRevRescueFallbackExp;
      break;
    }
    case RandomEventType::EnvironmentalPuzzle:
      if (zd != nullptr && !zd->puzzles.empty()) e.puzzleIndex = static_cast<int32_t>(rng.Index(zd->puzzles.size()));
      break;
    case RandomEventType::TreasureCache:
    case RandomEventType::WanderingMerchant:
      break;
  }
  // checkRandomEvents: the event message, RANDOM_EVENT_TRIGGERED.
  ctx_.events.Log(MakeLoc("sys.event.msg." + std::string(EnumName(type))), LogType::Info);
  events_.push_back(std::move(e));
  ActiveRandomEvent& ev = events_.back();
  const std::vector<std::string> noIds;
  const std::vector<std::string>& ids = zd != nullptr ? zd->ambushMonsters : noIds;
  switch (type) {
    case RandomEventType::Ambush: {
      // No ambush list (a zone without event data): SpawnAmbush falls back to the zone's first monster (web parity).
      if (ctx_.sys.monsters != nullptr) {
        ev.monsters = ctx_.sys.monsters->SpawnAmbush(ids, ev.monsterCount, ev.pos, kRevAmbushMinDist,
                                                     kRevAmbushDistRange, MonsterRole::AmbushSpawn);
      }
      ctx_.events.Emit(EvRandomEvent{false, ev.type, ev.pos, zoneId, kNoEntity});
      Resolve(ev);  // ambush auto-resolves once spawned
      break;
    }
    case RandomEventType::TreasureCache: {
      const MapDef& map = zone->Map();
      const double luck = ctx_.sys.groundLoot != nullptr ? ctx_.sys.groundLoot->LootLuck() : 0.0;
      const LootRollInput roll = TreasureCacheRoll(map.levelMin, map.levelMax, luck, Difficulty::Normal);  // loot Q20
      std::vector<ItemInstance> items;
      if (ctx_.sys.inventory != nullptr) {
        const LootContext lc{&ctx_.data, &ctx_.Rand(RngStream::Loot), &ctx_.sys.inventory->Uids()};
        items = GenerateLoot(lc, roll);
      }
      int32_t goldMin = 0, goldMax = 0;
      TreasureCacheGold(map.levelMin, map.levelMax, goldMin, goldMax);
      const int32_t gold = rng.RandomInt(goldMin, goldMax);
      if (ctx_.sys.rewards != nullptr) ctx_.sys.rewards->ChangeGold(gold, GoldReason::RandomEvent);
      ctx_.events.Log(MakeLoc("zone.event.treasureChest.goldReward", {{"gold", ToStr(gold)}}), LogType::Info);
      SpawnProp(ev, ev.pos, RevPropArt(type, zd));
      ev.timer = ctx_.timers.Schedule(ctx_.Now() + kRevChestFadeMs, TimerOwner::World,
                                      static_cast<uint16_t>(RandomEventTimerKind::ChestFade), ev.prop);
      if (ctx_.sys.groundLoot != nullptr) {
        for (ItemInstance& it : items) ctx_.sys.groundLoot->Drop(std::move(it), ev.pos, /*despawns=*/false);
      }
      ctx_.events.Emit(EvRandomEvent{false, ev.type, ev.pos, zoneId, ev.prop});
      Resolve(ev);
      break;
    }
    case RandomEventType::WanderingMerchant: {
      const TilePos t = EventTile(ev.pos);
      SpawnProp(ev, t.Center(), RevPropArt(type, zd));
      if (ctx_.sys.shop != nullptr) {
        // W10 parity: the merchant's priceMultiplier 1.2 is never applied by the shop.
        ctx_.sys.shop->OpenWanderingMerchant(zd != nullptr ? zd->merchantItems : noIds, 1.0);
        ctx_.events.Sfx(ctx_.data.Audio().rules.shopOpen);  // SHOP_OPEN -> panel_open (audio 3.1)
      }
      ctx_.events.Log(MakeLoc("zone.event.merchant.announce"), LogType::Info);
      ctx_.events.Emit(EvRandomEvent{false, ev.type, ev.pos, zoneId, ev.prop});
      Resolve(ev);
      break;
    }
    case RandomEventType::Rescue: {
      const TilePos t = EventTile(ev.pos);
      SpawnProp(ev, t.Center(), RevPropArt(type, zd));
      if (ctx_.sys.monsters != nullptr) {  // empty list -> the zone's first monster (as for the ambush)
        ev.monsters = ctx_.sys.monsters->SpawnAmbush(ids, ev.monsterCount, t.Center(), kRevRescueMinDist,
                                                     kRevRescueDistRange, MonsterRole::RescueSpawn);
      }
      ev.timer = ctx_.timers.Schedule(ctx_.Now() + kRevRescuePollMs, TimerOwner::World,
                                      static_cast<uint16_t>(RandomEventTimerKind::RescuePoll), ev.prop);
      ctx_.events.Emit(EvRandomEvent{false, ev.type, ev.pos, zoneId, ev.prop});
      break;
    }
    case RandomEventType::EnvironmentalPuzzle: {
      // createEvent always builds a puzzle (the zone's, else the fallback puzzle: puzzleIndex -1), so the web's
      // "no puzzle -> resolved at once" branch of handlePuzzleEvent is unreachable.
      ctx_.events.Log(
          MakeLoc("zone.event.puzzle.prompt", {KeyArg("prompt", RevPuzzleKey(zoneId, ev.puzzleIndex, "prompt"))}),
          LogType::Info);
      const TilePos t = EventTile(ev.pos);
      SpawnProp(ev, t.Center(), RevPropArt(type, zd));
      ctx_.events.Emit(EvRandomEvent{false, ev.type, ev.pos, zoneId, ev.prop});
      break;
    }
  }
  PruneFinished();
}

void RandomEventSystem::CompleteRescue(ActiveRandomEvent& e) {
  if (e.resolved) return;
  const std::string zoneId = ctx_.session.currentMap;
  if (ctx_.sys.rewards != nullptr) {
    ctx_.sys.rewards->ChangeGold(e.rewardGold, GoldReason::RandomEvent);
    ctx_.sys.rewards->GrantExp(e.rewardExp, ExpSource::RandomEvent);  // W11: the normal addExp path
  }
  const ZoneEventDataDef* zd = ctx_.data.World().randomEvents.ForZone(zoneId);
  const std::string npcKey = zd != nullptr ? "sys.event.rescue." + zoneId : std::string("sys.event.rescue.fallback");
  ctx_.events.Log(MakeLoc("zone.event.rescue.complete", {KeyArg("npcName", npcKey),
                                                         {"gold", ToStr(e.rewardGold)},
                                                         {"exp", ToStr(e.rewardExp)}}),
                  LogType::Info);
  Resolve(e);
  DespawnEventProp(e, DespawnReason::Removed);
}

void RandomEventSystem::OnTimer(const Timer& t) {
  using namespace randomevents_impl;
  for (ActiveRandomEvent& e : events_) {
    if (e.timer != t.id) continue;
    e.timer = kNoTimer;
    if (t.kind == static_cast<uint16_t>(RandomEventTimerKind::ChestFade)) {
      DespawnEventProp(e, DespawnReason::Expired);
    } else if (t.kind == static_cast<uint16_t>(RandomEventTimerKind::RescuePoll) && !e.resolved) {
      // M4 / W9: event monsters never respawn, so the rescue completes once every tracked monster is dead or gone.
      bool allDefeated = true;
      for (EntityId id : e.monsters) {
        const MonsterInstance* m = ctx_.sys.monsters != nullptr ? ctx_.sys.monsters->Find(id) : nullptr;
        if (m != nullptr && m->IsAlive()) allDefeated = false;
      }
      if (allDefeated) {
        CompleteRescue(e);
      } else {
        e.timer = ctx_.timers.Schedule(ctx_.Now() + kRevRescuePollMs, TimerOwner::World,
                                       static_cast<uint16_t>(RandomEventTimerKind::RescuePoll), e.prop);
      }
    }
    break;
  }
  PruneFinished();
}

bool RandomEventSystem::InteractProp(EntityId prop) {
  const ActiveRandomEvent* e = FindByProp(prop);
  if (e == nullptr) return false;
  switch (e->type) {
    case RandomEventType::WanderingMerchant: {
      if (ctx_.sys.shop == nullptr) return false;
      const ZoneEventDataDef* zd = ctx_.data.World().randomEvents.ForZone(ctx_.session.currentMap);
      const std::vector<std::string> none;
      ctx_.sys.shop->OpenWanderingMerchant(zd != nullptr ? zd->merchantItems : none, 1.0);
      ctx_.events.Sfx(ctx_.data.Audio().rules.shopOpen);  // SHOP_OPEN -> panel_open (audio 3.1)
      return true;
    }
    case RandomEventType::EnvironmentalPuzzle:
      return OpenPuzzle(prop);
    default:
      return false;
  }
}

bool RandomEventSystem::OpenPuzzle(EntityId prop) {
  for (const ActiveRandomEvent& e : events_) {
    if (e.prop != prop || e.type != RandomEventType::EnvironmentalPuzzle || e.resolved) continue;
    puzzle_.open = true;
    puzzle_.prop = prop;
    puzzle_.zoneId = ctx_.session.currentMap;
    puzzle_.puzzleIndex = e.puzzleIndex;
    ctx_.events.Emit(EvPuzzlePrompt{true, prop, puzzle_.zoneId, puzzle_.puzzleIndex});
    return true;
  }
  return false;
}

void RandomEventSystem::ClosePuzzle() {
  if (!puzzle_.open) return;
  const PuzzlePromptState was = puzzle_;
  puzzle_ = PuzzlePromptState{};
  ctx_.events.Emit(EvPuzzlePrompt{false, was.prop, was.zoneId, was.puzzleIndex});
}

bool RandomEventSystem::AnswerPuzzle(EntityId prop, int32_t choice) {
  if (!puzzle_.open || puzzle_.prop != prop) return false;
  if (choice == kPuzzleChoiceLeave) {
    ctx_.events.Log(MakeLoc("zone.event.puzzle.left"), LogType::Info);
    ClosePuzzle();  // the event stays unresolved (world 13.3)
    return true;
  }
  if (choice != kPuzzleChoiceSolve) return false;
  ActiveRandomEvent* ev = nullptr;
  for (ActiveRandomEvent& e : events_) {
    if (e.prop == prop) ev = &e;
  }
  if (ev == nullptr || ev->resolved) {
    ClosePuzzle();
    return false;
  }
  const std::string zoneId = puzzle_.zoneId;
  const ZoneEventDataDef* zd = ctx_.data.World().randomEvents.ForZone(zoneId);
  int64_t gold = randomevents_impl::kRevPuzzleFallbackGold, exp = randomevents_impl::kRevPuzzleFallbackExp;
  int32_t index = -1;
  if (zd != nullptr && ev->puzzleIndex >= 0 && static_cast<size_t>(ev->puzzleIndex) < zd->puzzles.size()) {
    index = ev->puzzleIndex;
    gold = zd->puzzles[static_cast<size_t>(index)].rewardGold;
    exp = zd->puzzles[static_cast<size_t>(index)].rewardExp;
  }
  // ZoneScene.ts:3655: "<solution> - <reward>", then the gold / exp line.
  ctx_.events.Log(MakeLoc("zone.event.puzzle.solved",
                          {KeyArg("solution", randomevents_impl::RevPuzzleKey(zoneId, index, "solution")),
                           KeyArg("reward", randomevents_impl::RevPuzzleKey(zoneId, index, "reward"))}),
                  LogType::Info);
  if (ctx_.sys.rewards != nullptr) {
    ctx_.sys.rewards->ChangeGold(gold, GoldReason::RandomEvent);
    ctx_.sys.rewards->GrantExp(exp, ExpSource::RandomEvent);  // W11
  }
  ctx_.events.Log(MakeLoc("zone.event.puzzle.rewardGoldExp", {{"gold", ToStr(gold)}, {"exp", ToStr(exp)}}),
                  LogType::Info);
  ClosePuzzle();
  Resolve(*ev);
  DespawnEventProp(*ev, DespawnReason::Collected);
  PruneFinished();
  return true;
}

void RandomEventSystem::FillSnapshot(Snapshot& out) const {
  out.randomEvents = &events_;
  out.puzzle = &puzzle_;
  for (const ActiveRandomEvent& e : events_) {
    if (e.prop == kNoEntity) continue;
    WorldMarkerView v;
    v.id = e.prop;
    v.kind = MarkerKind::EventProp;
    v.pos = e.propPos;
    v.key = std::string(EnumName(e.type));
    out.markers.push_back(std::move(v));
  }
}

}  // namespace abyss

// Ground items and potion pickups (loot-items-inventory.md 5.1, 6; I1, I10, C12; D13 T10). Owner area: items.
#include "abyss/base/Platform.h"

#include "abyss/items/GroundLoot.h"

#include <cmath>

#include "abyss/base/Math.h"
#include "abyss/base/StrUtil.h"
#include "abyss/data/DataStore.h"
#include "abyss/hero/Hero.h"
#include "abyss/hero/Rewards.h"
#include "abyss/items/Inventory.h"
#include "abyss/items/LootGen.h"
#include "abyss/pets/Homestead.h"
#include "abyss/pets/PetSystem.h"
#include "abyss/sim/SimContext.h"
#include "abyss/sim/Snapshot.h"

namespace abyss {

namespace {

// Auto-loot ranks (6.4): item normal 0, magic 1, rare 2, legendary / set 3; mode all 0, magic 1, rare 2, legendary 3.
int32_t GlItemRank(ItemQuality q) {
  switch (q) {
    case ItemQuality::Normal: return 0;
    case ItemQuality::Magic: return 1;
    case ItemQuality::Rare: return 2;
    case ItemQuality::Legendary:
    case ItemQuality::Set: return 3;
  }
  return 0;
}

int32_t GlModeRank(AutoLootMode m) {
  switch (m) {
    case AutoLootMode::Off:
    case AutoLootMode::All: return 0;
    case AutoLootMode::Magic: return 1;
    case AutoLootMode::Rare: return 2;
    case AutoLootMode::Legendary: return 3;
  }
  return 0;
}

}  // namespace

GroundLootSystem::GroundLootSystem(SimContext& ctx) : ctx_(ctx) {}

EntityId GroundLootSystem::Drop(ItemInstance item, Vec2 pos, bool despawns) {
  const LootRulesDef& l = ctx_.data.Items().loot;
  Rng& rng = ctx_.Rand(RngStream::Loot);
  GroundItem g;
  g.id = ctx_.ids.Next();
  g.pos = pos;
  const double ox = rng.Float01() * 0.5;  // visual jitter (dropLoot: col + U*0.5, row + U*0.5)
  const double oy = rng.Float01() * 0.5;
  g.visualOffset = Vec2(ox, oy);
  g.droppedAtMs = ctx_.Now();
  g.despawns = despawns;
  const double expires = despawns ? ctx_.Now() + l.groundItemLifetimeMs : 0.0;
  if (despawns) {
    g.timer = ctx_.timers.Schedule(expires, TimerOwner::Items, static_cast<uint16_t>(GroundLootTimerKind::ItemDespawn),
                                   g.id);
  }
  g.item = std::move(item);
  ctx_.events.Emit(EvLootDropped{g.id, g.item.uid, g.item.baseId, g.item.quality, g.item.quantity, pos, expires});
  const EntityId id = g.id;
  items_.push_back(std::move(g));
  return id;
}

EntityId GroundLootSystem::DropPotion(PotionKind kind, int32_t amount, Vec2 pos) {
  const LootRulesDef& l = ctx_.data.Items().loot;
  PotionDrop p;
  p.id = ctx_.ids.Next();
  p.kind = kind;
  p.amount = amount;
  p.pos = pos;
  p.timer = ctx_.timers.Schedule(ctx_.Now() + l.potionPickupLifetimeMs, TimerOwner::Items,
                                 static_cast<uint16_t>(GroundLootTimerKind::PotionDespawn), p.id);
  ctx_.events.Emit(EvPotionDropped{p.id, kind, amount, pos});
  potions_.push_back(p);
  return p.id;
}

double GroundLootSystem::LootLuck() const {
  double luck = 0;
  if (ctx_.sys.hero != nullptr) luck += ctx_.sys.hero->BaseStats().lck;  // raw: class base + allocated points
  if (ctx_.sys.homestead != nullptr) {
    // The web's homeBonus is getTotalBonuses() = building bonuses only; the altar blessing (part of the port's
    // TotalBonuses, Homestead.h) feeds EquipStats instead and never reached the loot roll (loot Q3).
    luck += ctx_.sys.homestead->TotalBonuses().Get(Stat::MagicFind) -
            ctx_.sys.homestead->BlessingStats().Get(Stat::MagicFind);
  }
  if (ctx_.sys.pets != nullptr) luck += ctx_.sys.pets->Bonuses().Get(Stat::MagicFind);
  if (ctx_.sys.inventory != nullptr) {
    // I1: gear lck (incl. diamond allStats) and gear magicFind count, through the same x0.5 / x0.3 coefficients.
    const EquipStats gear = ctx_.sys.inventory->Items().GearStats();
    luck += gear.Get(Stat::Lck) + gear.Get(Stat::MagicFind);
  }
  return luck;
}

void GroundLootSystem::OnMonsterKilled(const MonsterKilledMsg& m) {
  if (ctx_.sys.inventory == nullptr) return;
  const LootRulesDef& l = ctx_.data.Items().loot;
  const LootContext lc{&ctx_.data, &ctx_.Rand(RngStream::Loot), &ctx_.sys.inventory->Uids()};
  // Ley fruit (pets 18): rand < (elite ? 0.12 : 0.015) -> createItem(c_ley_fruit, hero level, normal) on the ground.
  if (lc.rng->Roll(m.elite ? l.leyFruitChanceElite : l.leyFruitChanceOther)) {
    const int32_t heroLevel = ctx_.sys.hero != nullptr ? ctx_.sys.hero->Level() : 1;
    std::optional<ItemInstance> fruit = CreateItem(lc, l.leyFruitItemId, heroLevel, ItemQuality::Normal);
    if (fruit.has_value()) Drop(std::move(*fruit), m.pos);
  }
  LootRollInput in;
  in.monsterLevel = m.level;
  in.elite = m.elite;
  in.isMiniBoss = m.isMiniBoss;
  in.isSubDungeonMiniBoss = m.isSubDungeonMiniBoss;
  in.luck = LootLuck();
  in.affixLootBonus = m.affixLootBonus;
  in.difficulty = ctx_.session.difficulty;
  std::vector<ItemInstance> loot = GenerateLoot(lc, in);
  for (ItemInstance& item : loot) {
    PotionKind kind = PotionKind::Hp;
    int32_t amount = 0;
    if (IsGroundPotion(ctx_.data, item.baseId, kind, amount)) {
      DropPotion(kind, amount, m.pos);  // stack quantity ignored (web)
    } else {
      Drop(std::move(item), m.pos);
    }
  }
}

void GroundLootSystem::Tick() {
  Hero* hero = ctx_.sys.hero;
  if (hero == nullptr || hero->Life() != HeroLife::Alive || hero->Hp() <= 0) return;  // C12: pickups pause
  const LootRulesDef& l = ctx_.data.Items().loot;
  const Vec2 pos = hero->Position();
  // Potions: every step, last to first, collected even at full HP / MP.
  for (size_t i = potions_.size(); i-- > 0;) {
    const PotionDrop p = potions_[i];
    if (DistSq(pos, p.pos) > l.pickupRadiusSq) continue;
    if (p.kind == PotionKind::Hp) {
      hero->Heal(p.amount);
      ctx_.events.Log(MakeLoc("zone.combat.restoreHp", {{"amount", ToStr(p.amount)}}), LogType::Combat);
    } else {
      hero->RestoreMana(p.amount);
      ctx_.events.Log(MakeLoc("zone.combat.restoreMana", {{"amount", ToStr(p.amount)}}), LogType::Info);
    }
    ctx_.events.Emit(EvPotionPicked{p.kind, p.amount});
    RemovePotionAt(i, DespawnReason::Collected);
  }
  // Auto-loot (6.4).
  if (hero->autoLoot != AutoLootMode::Off && ctx_.Now() - lastAutoLootMs_ > l.autoLootIntervalMs) {
    lastAutoLootMs_ = ctx_.Now();
    AutoLoot();
  }
}

void GroundLootSystem::AutoLoot() {
  Hero* hero = ctx_.sys.hero;
  const LootRulesDef& l = ctx_.data.Items().loot;
  const int32_t minRank = GlModeRank(hero->autoLoot);
  const Vec2 pos = hero->Position();
  bool blocked = false;
  for (size_t i = items_.size(); i-- > 0;) {
    if (GlItemRank(items_[i].item.quality) < minRank) continue;
    if (DistSq(pos, items_[i].pos) > l.pickupRadiusSq) continue;
    ItemInstance copy = items_[i].item;
    if (GrantPickup(copy)) {
      RemoveItemAt(i, DespawnReason::PickedUp);
    } else {
      blocked = true;
      break;  // inventory full: stop scanning this tick
    }
  }
  if (blocked && !autoLootBlocked_) ctx_.events.Log(MakeLoc("sys.inventory.bagFull"), LogType::System);
  autoLootBlocked_ = blocked;
}

bool GroundLootSystem::GrantPickup(ItemInstance& item) {
  if (ctx_.sys.rewards != nullptr) {
    return ctx_.sys.rewards->GrantItem(item, OverflowPolicy::Refuse, ItemSource::Pickup) == ItemGrantOutcome::Bag;
  }
  if (ctx_.sys.inventory != nullptr) {
    return ctx_.sys.inventory->Grant(item, OverflowPolicy::Refuse, ItemSource::Pickup) == ItemGrantOutcome::Bag;
  }
  return false;
}

void GroundLootSystem::RemoveItemAt(size_t index, DespawnReason reason) {
  const GroundItem& g = items_[index];
  if (g.timer != kNoTimer) ctx_.timers.Cancel(g.timer);
  ctx_.events.Emit(EvEntityDespawned{g.id, EntityKind::GroundItem, reason});
  items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(index));
}

void GroundLootSystem::RemovePotionAt(size_t index, DespawnReason reason) {
  const PotionDrop& p = potions_[index];
  if (p.timer != kNoTimer) ctx_.timers.Cancel(p.timer);
  ctx_.events.Emit(EvEntityDespawned{p.id, EntityKind::PotionDrop, reason});
  potions_.erase(potions_.begin() + static_cast<std::ptrdiff_t>(index));
}

void GroundLootSystem::OnTimer(const Timer& t) {
  if (t.owner != TimerOwner::Items) return;
  if (t.kind == static_cast<uint16_t>(GroundLootTimerKind::ItemDespawn)) {
    for (size_t i = 0; i < items_.size(); ++i) {
      if (items_[i].id != t.entity) continue;
      items_[i].timer = kNoTimer;  // already popped
      RemoveItemAt(i, DespawnReason::Expired);
      return;
    }
  } else if (t.kind == static_cast<uint16_t>(GroundLootTimerKind::PotionDespawn)) {
    for (size_t i = 0; i < potions_.size(); ++i) {
      if (potions_[i].id != t.entity) continue;
      potions_[i].timer = kNoTimer;
      RemovePotionAt(i, DespawnReason::Expired);
      return;
    }
  }
}

void GroundLootSystem::OnZoneExit() {
  for (const GroundItem& g : items_) ctx_.events.Emit(EvEntityDespawned{g.id, EntityKind::GroundItem, DespawnReason::ZoneUnload});
  for (const PotionDrop& p : potions_) ctx_.events.Emit(EvEntityDespawned{p.id, EntityKind::PotionDrop, DespawnReason::ZoneUnload});
  items_.clear();
  potions_.clear();
  autoLootBlocked_ = false;
  ctx_.timers.CancelIf([](const Timer& t) { return t.owner == TimerOwner::Items; });
}

bool GroundLootSystem::InPickupRange(EntityId drop) const {
  const GroundItem* g = Find(drop);
  if (g == nullptr || ctx_.sys.hero == nullptr) return false;
  return DistSq(ctx_.sys.hero->Position(), g->pos) <= ctx_.data.Items().loot.pickupRadiusSq;
}

bool GroundLootSystem::TryPickUp(EntityId drop) {
  Hero* hero = ctx_.sys.hero;
  if (hero == nullptr || hero->Life() != HeroLife::Alive) return false;
  for (size_t i = 0; i < items_.size(); ++i) {
    if (items_[i].id != drop) continue;
    if (!InPickupRange(drop)) return false;  // the caller walks there first (W8 / FIX Q22)
    ItemInstance copy = items_[i].item;
    if (!GrantPickup(copy)) {
      ctx_.events.Log(MakeLoc("sys.inventory.bagFull"), LogType::System);
      return false;
    }
    RemoveItemAt(i, DespawnReason::PickedUp);
    return true;
  }
  return false;
}

EntityId GroundLootSystem::LootAt(Vec2 tile) const {
  const double box = ctx_.data.Items().loot.clickHitBoxTiles;
  for (const GroundItem& g : items_) {
    if (std::fabs(g.pos.x - tile.x) < box && std::fabs(g.pos.y - tile.y) < box) return g.id;
  }
  return kNoEntity;
}

const GroundItem* GroundLootSystem::Find(EntityId id) const {
  for (const GroundItem& g : items_) {
    if (g.id == id) return &g;
  }
  return nullptr;
}

void GroundLootSystem::FillSnapshot(Snapshot& out) const {
  for (const GroundItem& g : items_) {
    GroundItemView v;
    v.id = g.id;
    v.uid = g.item.uid;
    v.baseId = g.item.baseId;
    v.quality = g.item.quality;
    v.quantity = g.item.quantity;
    v.pos = g.pos;
    v.visualOffset = g.visualOffset;
    out.groundItems.push_back(std::move(v));
  }
  for (const PotionDrop& p : potions_) out.potions.push_back({p.id, p.kind, p.amount, p.pos});
}

}  // namespace abyss

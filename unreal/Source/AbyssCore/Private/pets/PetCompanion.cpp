// The active ley-beast in the world (quests-story-ch1.md 18.5). STUB: owner area quests+story+pets. Spawn / despawn /
// re-spawn on zone entry and on PetChangedMsg are implemented; the per-step behaviour is a stub.
#include "abyss/base/Platform.h"

#include "abyss/pets/PetCompanion.h"

#include "abyss/base/Assert.h"
#include "abyss/base/Math.h"
#include "abyss/base/StrUtil.h"
#include "abyss/combat/Projectiles.h"
#include "abyss/data/DataStore.h"
#include "abyss/hero/Hero.h"
#include "abyss/pets/PetSystem.h"
#include "abyss/world/Zone.h"
#include "abyss/sim/SimContext.h"
#include "abyss/sim/Snapshot.h"

namespace abyss {

PetAction ChoosePetAction(const PetDef& def, const PetActionContext& ctx, const std::vector<double>& abilityReadyAt) {
  ABYSS_UNIMPLEMENTED();
  return {};
}

std::string PetArtId(std::string_view petId, int32_t stage) {
  return stage <= 0 ? StrCat("beast_", petId) : StrCat("beast_", petId, "_e", stage);
}

PetCompanion::PetCompanion(SimContext& ctx) : ctx_(ctx) {}

void PetCompanion::OnZoneEnter() {
  state_ = PetCompanionState{};
  SpawnActive();
}

void PetCompanion::OnZoneExit() { state_ = PetCompanionState{}; }

void PetCompanion::OnPetChanged(const PetChangedMsg& m) {
  if (ctx_.sys.zone == nullptr || !ctx_.sys.zone->HasZone() || ctx_.sys.pets == nullptr) return;
  const PetInstance* active = ctx_.sys.pets->Active();
  if (active == nullptr) {
    Despawn(DespawnReason::Removed);
    return;
  }
  if (state_.present && state_.petId == active->petId && state_.stage == active->evolved) return;
  Despawn(DespawnReason::Removed);
  state_ = PetCompanionState{};  // per-visit state restarts with the new beast / stage (QP2 parity)
  SpawnActive();
}

void PetCompanion::Despawn(DespawnReason reason) {
  if (!state_.present) return;
  ctx_.events.Emit(EvEntityDespawned{state_.entity, EntityKind::Pet, reason});
  state_.present = false;
  state_.entity = kNoEntity;
}

void PetCompanion::SpawnActive() {
  if (ctx_.sys.pets == nullptr || ctx_.sys.hero == nullptr || ctx_.sys.zone == nullptr || !ctx_.sys.zone->HasZone()) {
    return;
  }
  const PetInstance* active = ctx_.sys.pets->Active();
  const PetDef* def = active != nullptr ? ctx_.data.Pets().Find(active->petId) : nullptr;
  if (def == nullptr) return;
  const Hero& hero = *ctx_.sys.hero;
  const Vec2 heroPos = hero.Position();
  Vec2 pos(heroPos.x - 1.2, heroPos.y + 1.2);  // 18.5.3
  const TilePos t = RoundToTile(pos);
  if (!ctx_.sys.zone->Walkable(t.col, t.row)) pos = heroPos;
  state_.present = true;
  state_.entity = ctx_.ids.Next();
  state_.petId = active->petId;
  state_.stage = active->evolved;
  state_.pos = pos;
  state_.maxHp = (std::max)(1.0, JsRound(hero.MaxHp() * def->hpFraction));
  state_.hp = state_.maxHp;
  state_.abilityReadyAtMs.assign(def->abilities.size(), 0.0);
  ctx_.events.Emit(EvEntitySpawned{state_.entity, EntityKind::Pet, state_.petId, PetArtId(state_.petId, state_.stage),
                                   pos, state_.facing, 1.0});
}

void PetCompanion::Tick(double dtMs) { ABYSS_UNIMPLEMENTED(); }

void PetCompanion::OnTimer(const Timer& t) { ABYSS_UNIMPLEMENTED(); }

void PetCompanion::OnBoltArrived(const Projectile& p) { ABYSS_UNIMPLEMENTED(); }

bool PetCompanion::TryReviveHero() {
  ABYSS_UNIMPLEMENTED();
  return false;
}

bool PetCompanion::InterceptMonsterAttack(EntityId monster) {
  ABYSS_UNIMPLEMENTED();
  return false;
}

bool PetCompanion::FeedFromBag(std::string_view petId) {
  ABYSS_UNIMPLEMENTED();
  return false;
}

void PetCompanion::FillSnapshot(Snapshot& out) const {
  out.pet.present = state_.present;
  out.pet.id = state_.entity;
  out.pet.petId = state_.petId;
  out.pet.stage = state_.stage;
  out.pet.pos = state_.pos;
  out.pet.prevPos = state_.pos;
  out.pet.facing = state_.facing;
  out.pet.hp = state_.hp;
  out.pet.maxHp = state_.maxHp;
  out.pet.exhausted = state_.exhaustedUntilMs > ctx_.Now();
  out.pet.target = state_.target;
}

}  // namespace abyss

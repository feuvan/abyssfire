// Soul echo / death penalty (combat-feel.md section 15; save-ui-input.md 3.2 `soulEcho`).
#include "abyss/base/Platform.h"

#include "abyss/combat/SoulEcho.h"

#include <algorithm>
#include <cmath>

#include "abyss/base/I18n.h"
#include "abyss/base/Math.h"
#include "abyss/base/StrUtil.h"
#include "abyss/data/DataStore.h"
#include "abyss/hero/Hero.h"
#include "abyss/hero/Rewards.h"
#include "abyss/save/SaveData.h"
#include "abyss/sim/SimContext.h"
#include "abyss/sim/Snapshot.h"

namespace abyss {

DeathPenalty ComputeDeathPenalty(const SoulEchoDef& def, int32_t level, int64_t gold, int64_t exp, int64_t expToNext,
                                 Difficulty difficulty) {
  DeathPenalty p;
  if (level < def.minLevel) return p;
  const size_t d = static_cast<size_t>(difficulty);
  p.gold = static_cast<int64_t>(std::floor(static_cast<double>((std::max)(int64_t{0}, gold)) * def.goldShare[d]));
  const int64_t expCap = static_cast<int64_t>(std::floor(static_cast<double>(expToNext) * def.expShare[d]));
  p.exp = (std::min)((std::max)(int64_t{0}, exp), (std::max)(int64_t{0}, expCap));
  return p;
}

bool SoulEchoState::Leave(const SoulEchoData& echo, bool& hadPrevious, SoulEchoData& faded) {
  hadPrevious = has_;
  if (has_) faded = echo_;
  has_ = false;
  echo_ = SoulEchoData{};
  if (echo.gold > 0 || echo.exp > 0) {  // a free death still destroys the old echo
    echo_ = echo;
    has_ = true;
  }
  return has_;
}

bool SoulEchoState::TryClaim(std::string_view mapId, Vec2 heroPos, double rangeTiles, SoulEchoData& claimed) {
  if (!has_ || echo_.mapId != mapId) return false;
  if (JsHypot(heroPos.x - echo_.col, heroPos.y - echo_.row) > rangeTiles) return false;
  claimed = echo_;
  has_ = false;
  echo_ = SoulEchoData{};
  return true;
}

bool SoulEchoState::Load(const std::string& mapId, double col, double row, int64_t gold, int64_t exp) {
  has_ = false;
  echo_ = SoulEchoData{};
  if (!std::isfinite(col) || !std::isfinite(row) || mapId.empty()) return false;
  echo_.mapId = mapId;
  echo_.col = JsRoundInt(col);
  echo_.row = JsRoundInt(row);
  echo_.gold = (std::max)(int64_t{0}, gold);
  echo_.exp = (std::max)(int64_t{0}, exp);
  has_ = true;
  return true;
}

SoulEchoSystem::SoulEchoSystem(SimContext& ctx) : ctx_(ctx) {}

void SoulEchoSystem::OnHeroDied(Vec2 pos, bool inDungeon) {
  Hero& hero = *ctx_.sys.hero;
  const DeathPenalty toll = ComputeDeathPenalty(ctx_.data.Combat().soulEcho, hero.Level(), hero.Gold(), hero.Exp(),
                                                hero.ExpToNext(), ctx_.session.difficulty);
  if (ctx_.sys.rewards != nullptr) {
    if (toll.gold > 0) ctx_.sys.rewards->ChangeGold(-toll.gold, GoldReason::DeathPenalty);
    if (toll.exp > 0) ctx_.sys.rewards->RemoveExp(toll.exp, ExpSource::DeathPenalty);
  } else {
    hero.SetGold(hero.Gold() - toll.gold);
    hero.RemoveExp(toll.exp);
  }
  // Dungeons are rebuilt on every run: nowhere to come back to.
  if (inDungeon) {
    if (toll.gold > 0) {
      ctx_.events.Log(MakeLoc("zone.soulEcho.lostInDungeon", {{"gold", ToStr(toll.gold)}}), LogType::System);
    }
    return;
  }
  SoulEchoData echo;
  echo.mapId = ctx_.session.currentMap;
  echo.col = JsRoundInt(pos.x);
  echo.row = JsRoundInt(pos.y);
  echo.gold = toll.gold;
  echo.exp = toll.exp;
  bool hadPrevious = false;
  SoulEchoData faded;
  const bool stored = state_.Leave(echo, hadPrevious, faded);
  if (hadPrevious) {
    ctx_.events.Log(MakeLoc("zone.soulEcho.faded", {{"gold", ToStr(faded.gold)}}), LogType::System);
  }
  DespawnProp(DespawnReason::Removed);
  if (stored) {
    ctx_.events.Log(MakeLoc("zone.soulEcho.left", {{"gold", ToStr(toll.gold)}}), LogType::System);
    SpawnProp();
  }
}

void SoulEchoSystem::Tick() {
  if (!state_.Has()) return;
  const Hero& hero = *ctx_.sys.hero;
  // The fallen hero lies on the echo until respawning; only the living reclaim it.
  if (hero.Life() != HeroLife::Alive || hero.Hp() <= 0) return;
  SoulEchoData claimed;
  if (!state_.TryClaim(ctx_.session.currentMap, hero.Position(), ctx_.data.Combat().soulEcho.claimRangeTiles,
                       claimed)) {
    return;
  }
  if (ctx_.sys.rewards != nullptr) {
    if (claimed.gold > 0) ctx_.sys.rewards->ChangeGold(claimed.gold, GoldReason::SoulEchoClaim);
    if (claimed.exp > 0) ctx_.sys.rewards->GrantExp(claimed.exp, ExpSource::SoulEchoClaim);
  }
  ctx_.events.Log(MakeLoc("zone.soulEcho.claimed", {{"gold", ToStr(claimed.gold)}}), LogType::Loot);
  const AudioRulesDef& rules = ctx_.data.Audio().rules;
  const AudioCueDef* cue = ctx_.data.Audio().Find(rules.soulEchoReclaimed);
  const Vec2 at(claimed.col, claimed.row);
  ctx_.events.Sfx(rules.soulEchoReclaimed, cue != nullptr && cue->spatial3d, at, propId_);
  DespawnProp(DespawnReason::Collected);
  ctx_.bus.Publish(SaveRequestMsg{SaveReason::SoulEchoClaimed});
}

void SoulEchoSystem::OnZoneEnter() {
  propId_ = kNoEntity;  // the previous zone's prop went with the zone
  if (state_.Has() && state_.Echo().mapId == ctx_.session.currentMap) SpawnProp();
}

void SoulEchoSystem::SpawnProp() {
  if (!state_.Has() || state_.Echo().mapId != ctx_.session.currentMap) return;
  if (propId_ != kNoEntity) return;
  propId_ = ctx_.ids.Next();
  const SoulEchoData& e = state_.Echo();
  const std::string art(EnumName(ctx_.sys.hero->Class()));
  ctx_.events.Emit(EvEntitySpawned{propId_, EntityKind::Prop, "soul_echo", art, Vec2(e.col, e.row), Vec2(1, 0), 1.0});
}

void SoulEchoSystem::DespawnProp(DespawnReason reason) {
  if (propId_ == kNoEntity) return;
  ctx_.events.Emit(EvEntityDespawned{propId_, EntityKind::Prop, reason});
  propId_ = kNoEntity;
}

void SoulEchoSystem::FillSnapshot(Snapshot& out) const {
  out.soulEcho = &state_;
  if (propId_ == kNoEntity || !state_.Has()) return;
  WorldMarkerView m;
  m.id = propId_;
  m.kind = MarkerKind::SoulEcho;
  m.pos = Vec2(state_.Echo().col, state_.Echo().row);
  m.key = ToStr(state_.Echo().gold);  // label zone.soulEcho.label {gold}
  out.markers.push_back(std::move(m));
}

void SoulEchoSystem::WriteSave(SaveData& out) const {
  out.soulEcho = SaveSoulEcho{};
  if (!state_.Has()) return;
  const SoulEchoData& e = state_.Echo();
  out.soulEcho.present = true;
  out.soulEcho.mapId = e.mapId;
  out.soulEcho.col = e.col;
  out.soulEcho.row = e.row;
  out.soulEcho.gold = e.gold;
  out.soulEcho.exp = e.exp;
}

void SoulEchoSystem::ReadSave(const SaveData& in) {
  propId_ = kNoEntity;
  state_.Clear();
  if (!in.soulEcho.present) return;
  state_.Load(in.soulEcho.mapId, in.soulEcho.col, in.soulEcho.row, in.soulEcho.gold, in.soulEcho.exp);
}

}  // namespace abyss

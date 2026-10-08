// GameSim facade and step loop (ARCHITECTURE 3.1; DECISIONS S1, S2, S6; classes-stats-skills.md 16 and 19.1 D13
// step algorithm, F1-F3; monsters-ai.md 3.9; world-map-nav.md 17). Owner: lead.
#include "abyss/base/Platform.h"

#include "SimImpl.h"

#include <algorithm>

#include "abyss/base/Assert.h"
#include "abyss/base/Log.h"
#include "abyss/save/SaveIO.h"

namespace abyss {

// ---------------------------------------------------------------------------------------------------------------------
// SimImpl: construction
// ---------------------------------------------------------------------------------------------------------------------
SimImpl::SimImpl(const DataStore& d, const SimConfig& cfg)
    : data(d),
      config(cfg),
      ctx{data, config, clock, rng, timers, events, bus, ids, session, SimSystems{}, EquipStats{}} {
  clock.SetMaxStepsPerFrame(config.maxStepsPerFrame);
  clock.SetMaxFrameMs(config.maxFrameMs);
}

SimImpl::~SimImpl() = default;

void SimImpl::BuildSystems(ClassId cls) {
  bus.Clear();
  ctx.sys = SimSystems{};
  // SimSystems order. Constructors only store references (SimContext.h rules).
  hero = std::make_unique<Hero>(data, cls);
  status = std::make_unique<StatusEffectSystem>(data.Classes().statusRules);
  zone = std::make_unique<ZoneRuntime>(ctx);
  locomotion = std::make_unique<HeroLocomotion>(ctx);
  exploration = std::make_unique<ExplorationSystem>(ctx);
  monsters = std::make_unique<MonsterSystem>(ctx);
  projectiles = std::make_unique<ProjectileSystem>(ctx);
  combat = std::make_unique<CombatSystem>(ctx);
  soulEcho = std::make_unique<SoulEchoSystem>(ctx);
  inventory = std::make_unique<InventorySystem>(ctx);
  groundLoot = std::make_unique<GroundLootSystem>(ctx);
  shop = std::make_unique<ShopSystem>(ctx);
  rewards = std::make_unique<RewardService>(ctx);
  quests = std::make_unique<QuestSystem>(data, events, bus);
  questWorld = std::make_unique<QuestWorld>(ctx);
  dialogue = std::make_unique<DialogueSystem>(ctx);
  achievements = std::make_unique<AchievementSystem>(ctx);
  lore = std::make_unique<LoreSystem>(ctx);
  story = std::make_unique<StoryDirector>(ctx);
  homestead = std::make_unique<HomesteadSystem>(ctx);
  pets = std::make_unique<PetSystem>(data, events, bus);
  petCompanion = std::make_unique<PetCompanion>(ctx);
  randomEvents = std::make_unique<RandomEventSystem>(ctx);
  audio = std::make_unique<AudioDirector>(ctx);

  SimSystems& s = ctx.sys;
  s.hero = hero.get();
  s.status = status.get();
  s.zone = zone.get();
  s.locomotion = locomotion.get();
  s.exploration = exploration.get();
  s.monsters = monsters.get();
  s.projectiles = projectiles.get();
  s.combat = combat.get();
  s.soulEcho = soulEcho.get();
  s.inventory = inventory.get();
  s.groundLoot = groundLoot.get();
  s.shop = shop.get();
  s.rewards = rewards.get();
  s.quests = quests.get();
  s.questWorld = questWorld.get();
  s.dialogue = dialogue.get();
  s.achievements = achievements.get();
  s.lore = lore.get();
  s.story = story.get();
  s.homestead = homestead.get();
  s.pets = pets.get();
  s.petCompanion = petCompanion.get();
  s.randomEvents = randomEvents.get();
  s.audio = audio.get();

  pets->SetBuildingLevelSource([this](std::string_view id) { return homestead->BuildingLevel(id); });
  pets->SetAwaySource([this](std::string_view id) { return homestead->IsPetAway(id); });
  Wire();
}

void SimImpl::ResetSession() {
  timers.Clear();
  events.Clear();
  pending.clear();
  ids.Reset();
  clock.SetSteps(0);
  clock.CancelDilation();
  for (uint32_t bit = 1; bit != 0 && bit <= FreezeBit(FreezeReason::Debug); bit <<= 1) {
    clock.SetFrozen(static_cast<FreezeReason>(bit), false);
  }
  session = SessionState{};
  session.touchMode = config.touchMode;
  ctx.equip = EquipStats{};
  wasFrozen = false;
  coreModalMask = 0;
  coreModalNpc = {};
  hasSession = false;
}

// ---------------------------------------------------------------------------------------------------------------------
// Freeze predicate (S2, U7, D13)
// ---------------------------------------------------------------------------------------------------------------------
namespace {
constexpr uint32_t PanelBit(PanelId p) { return 1u << static_cast<uint32_t>(p); }
}  // namespace

// Core-owned modals (SimTypes.h): the owning systems' state is the truth, so a modal opened by the core itself
// (NPC interaction, mini-boss approach, lore pickup, puzzle prop) blocks input / freezes at once and a core-side close
// (dialogue Leave, turn-in, zone exit, death) never leaves a stale panel behind.
uint32_t SimImpl::CoreModalMask() const {
  uint32_t m = 0;
  if (dialogue != nullptr && dialogue->View().open) m |= PanelBit(PanelId::Dialogue);
  if (questWorld != nullptr && questWorld->Card().open) m |= PanelBit(PanelId::QuestCard);
  if (shop != nullptr && shop->State().open) m |= PanelBit(shop->State().blacksmith ? PanelId::Forge : PanelId::Shop);
  if (inventory != nullptr && inventory->Stash().open) m |= PanelBit(PanelId::Stash);
  if (monsters != nullptr && monsters->MiniBossDialogueActive()) m |= PanelBit(PanelId::MiniBossDialogue);
  if (lore != nullptr && lore->Text().open) m |= PanelBit(PanelId::LoreText);
  if (randomEvents != nullptr && randomEvents->Puzzle().open) m |= PanelBit(PanelId::Puzzle);
  return m;
}

std::string SimImpl::CoreModalNpc(PanelId p) const {
  switch (p) {
    case PanelId::Dialogue: return dialogue->View().npcId;
    case PanelId::QuestCard: return questWorld->Card().npcId;
    case PanelId::Shop:
    case PanelId::Forge: return shop->State().npcId;
    case PanelId::Stash: return inventory->Stash().npcId;
    default: return std::string();
  }
}

void SimImpl::SyncCoreModals() {
  const uint32_t now = CoreModalMask();
  if (now == coreModalMask) return;
  for (size_t i = 0; i < EnumCount<PanelId>(); ++i) {
    const PanelId p = static_cast<PanelId>(i);
    const uint32_t bit = PanelBit(p);
    if ((coreModalMask & bit) != 0 && (now & bit) == 0) {
      events.Emit(EvPanelRequest{p, false, coreModalNpc[i]});
      coreModalNpc[i].clear();
    }
  }
  for (size_t i = 0; i < EnumCount<PanelId>(); ++i) {
    const PanelId p = static_cast<PanelId>(i);
    const uint32_t bit = PanelBit(p);
    if ((coreModalMask & bit) == 0 && (now & bit) != 0) {
      coreModalNpc[i] = CoreModalNpc(p);
      events.Emit(EvPanelRequest{p, true, coreModalNpc[i]});
    }
  }
  coreModalMask = now;
}

void SimImpl::CloseCoreModal(PanelId p) {
  switch (p) {
    case PanelId::Dialogue: dialogue->Close(); break;
    case PanelId::QuestCard: questWorld->CloseCard(); break;
    case PanelId::Shop:
    case PanelId::Forge: shop->Close(); break;
    case PanelId::Stash: inventory->CloseStash(); break;
    case PanelId::MiniBossDialogue: monsters->DismissMiniBossDialogue(); break;
    case PanelId::LoreText: lore->CloseText(); break;
    case PanelId::Puzzle: randomEvents->ClosePuzzle(); break;  // = leave: the event stays unresolved
    default: break;
  }
}

void SimImpl::CloseCoreModals() {
  for (size_t i = 0; i < EnumCount<PanelId>(); ++i) {
    const PanelId p = static_cast<PanelId>(i);
    if (IsCoreOwnedPanel(p) && p != PanelId::Forge) CloseCoreModal(p);  // Forge = the shop's blacksmith tab
  }
}

uint32_t SimImpl::ComputeFreezeMask() const {
  uint32_t mask = clock.FreezeMask() & (FreezeBit(FreezeReason::Modal) | FreezeBit(FreezeReason::Debug));
  if (story != nullptr && story->IsCinematic()) mask |= FreezeBit(FreezeReason::Cinematic);
  const PanelState& p = session.panels;
  if (p.IsOpen(PanelId::SystemMenu)) mask |= FreezeBit(FreezeReason::PauseMenu);
  // S2: dialogue with a quest card freezes - from the owners' state (core-owned modals), never from UE echoes.
  const uint32_t core = CoreModalMask();
  if ((core & (PanelBit(PanelId::Dialogue) | PanelBit(PanelId::QuestCard))) != 0) {
    mask |= FreezeBit(FreezeReason::QuestDialog);
  }
  if (session.touchMode && p.AnyHudPanelOpen()) mask |= FreezeBit(FreezeReason::TouchPanel);
  if (session.appBackground) mask |= FreezeBit(FreezeReason::Background);
  return mask;
}

void SimImpl::SyncFreeze() {
  SyncCoreModals();
  const uint32_t want = ComputeFreezeMask();
  for (uint32_t bit = 1; bit != 0 && bit <= FreezeBit(FreezeReason::Debug); bit <<= 1) {
    clock.SetFrozen(static_cast<FreezeReason>(bit), (want & bit) != 0);
  }
  const bool frozen = clock.IsFrozen();
  if (frozen && !wasFrozen) OnFreezeBegin(clock.IsFrozenBy(FreezeReason::Cinematic));
  wasFrozen = frozen;
}

void SimImpl::OnFreezeBegin(bool cinematic) {
  // F1 (every freeze) + F2 (cinematic) + F3 (everything else kept).
  locomotion->OnFreezeBegin(cinematic);
  combat->OnFreezeBegin(cinematic);
  projectiles->OnFreezeBegin(cinematic);
  if (cinematic) combat->ClearAttackTarget();
}

bool SimImpl::InputBlocked() const {
  return clock.IsFrozen() || session.panels.AnyModalOpen() || CoreModalMask() != 0;
}

// ---------------------------------------------------------------------------------------------------------------------
// Step (D13 step algorithm)
// ---------------------------------------------------------------------------------------------------------------------
void SimImpl::StepOnce() {
  SyncFreeze();
  if (WorldFrozen()) {
    ApplyCommands(/*frozen=*/true);
    SyncFreeze();
    return;
  }
  clock.AdvanceStep();
  const double now = clock.NowMs();
  Timer t;
  while (timers.PopDue(now, t)) {
    DispatchTimer(t);
    SyncFreeze();
    if (WorldFrozen()) return;  // a timer started a beat (T15): web update() returns this frame
  }
  ApplyCommands(/*frozen=*/false);
  SyncFreeze();
  if (WorldFrozen()) return;
  Update(kSimStepMs);
  SyncFreeze();  // a kill inside may start a 0 ms beat -> frozen from the next step
}

void SimImpl::DispatchTimer(const Timer& t) {
  switch (t.owner) {
    case TimerOwner::Sim:
    case TimerOwner::Hero:
      break;
    case TimerOwner::Combat:
      combat->OnTimer(t);
      break;
    case TimerOwner::Projectiles:
      projectiles->OnTimer(t);
      break;
    case TimerOwner::Monsters:
      monsters->OnTimer(t);
      break;
    case TimerOwner::Items:
      groundLoot->OnTimer(t);
      break;
    case TimerOwner::Quests:
      questWorld->OnTimer(t);
      break;
    case TimerOwner::Story:
      story->OnTimer(t);
      break;
    case TimerOwner::Pets:
      petCompanion->OnTimer(t);
      break;
    case TimerOwner::World:
      if (t.kind >= kRandomEventTimerKindBase) {
        randomEvents->OnTimer(t);
      } else {
        zone->OnTimer(t);
      }
      break;
  }
}

// One unfrozen tick in the order of classes-stats-skills 16, monsters-ai 3.9 and world-map-nav 17.
void SimImpl::Update(double dtMs) {
  if (!zone->HasZone()) return;
  // 3. input: buffered skill (keyboard / hold-move are applied inside the locomotion tick)
  combat->ConsumeBufferedSkill();
  // 4. merged equip stats + derived
  RebuildEquipStats();
  hero->RecalcDerived(ctx.equip);
  // 6. passives
  combat->TickPassives(dtMs);
  // 7. hero update: movement, spirit drain, regen
  locomotion->Tick(dtMs);
  combat->TickHeroUpdate(dtMs);
  // monsters: mini-boss dialogue check, AI driver (activity set, safe zones, immobilized skip)
  monsters->CheckMiniBossDialogue();
  monsters->TickAI(dtMs);
  // 9. handleCombat: prune buffs, monster swings, hero basic attack
  combat->TickCombat();
  // 10. pet, escort / defend (quest world), elite behaviours
  petCompanion->Tick(dtMs);
  monsters->TickEliteBehaviours();
  // 11. status effects (ticks then expiry)
  combat->TickStatusEffects();
  // combat state, random events, auto-combat (12)
  combat->TickCombatState();
  randomEvents->Tick();
  combat->TickAutoCombat();
  // world: pickups, exploration, lore / hidden areas, soul echo, exits / interact prompt
  groundLoot->Tick();
  exploration->Tick();
  lore->Tick();
  soulEcho->Tick();
  zone->Tick();
  // quest world (gather / clues / escort / defend / observers), story scan, homestead timers
  questWorld->Tick(dtMs);
  story->Tick(dtMs);
  homestead->Tick(dtMs);
  // session
  session.playTimeMs += dtMs;
  if (config.autosaveIntervalMs > 0 && clock.NowMs() >= session.nextAutosaveAtMs) {
    session.nextAutosaveAtMs = clock.NowMs() + config.autosaveIntervalMs;
    if (!combat->InCombat() && !story->IsCinematic()) RequestSave(SaveReason::Timer60s);
  }
  // zone transition requested by an exit / portal this step
  std::string nextMap;
  Vec2 target;
  if (zone->TakePendingTransition(nextMap, target)) {
    ExitZone();
    EnterZone(nextMap, true, target);
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// Saves (save-ui-input 3.4 rules 1-4)
// ---------------------------------------------------------------------------------------------------------------------
bool SimImpl::CanSave() const {
  return hasSession && hero != nullptr && hero->Life() == HeroLife::Alive && hero->Hp() > 0 && !session.transitioning;
}

void SimImpl::RequestSave(SaveReason reason) {
  if (!hasSession) return;
  if (hero->Life() == HeroLife::Dying) {
    session.savePending = true;
    return;
  }
  session.savePending = false;
  events.Emit(EvSaveRequested{std::string(EnumName(reason))});
}

// ---------------------------------------------------------------------------------------------------------------------
// GameSim facade
// ---------------------------------------------------------------------------------------------------------------------
GameSim::GameSim(std::unique_ptr<SimImpl> impl) : impl_(std::move(impl)) {}
GameSim::~GameSim() = default;

std::unique_ptr<GameSim> GameSim::Create(const DataStore& data, const SimConfig& cfg) {
  if (!data.IsFinalized()) {
    // Hard precondition with a safe fallback: UE's assert handler returns, and building systems on an unfinalized or
    // failed store would dereference missing class data.
    ReportAssertFailure(__FILE__, __LINE__, "data.IsFinalized()", "GameSim::Create: DataStore not finalized");
    return nullptr;
  }
  auto impl = std::make_unique<SimImpl>(data, cfg);
  impl->BuildSystems(ClassId::Warrior);  // placeholder session-less systems so View()/queries are valid
  impl->BuildSnapshot();
  return std::unique_ptr<GameSim>(new GameSim(std::move(impl)));
}

bool GameSim::NewGame(ClassId cls, Difficulty difficulty, uint64_t seed, int32_t slot) {
  SimImpl& s = *impl_;
  if (s.data.Classes().Find(cls) == nullptr) return false;
  if (s.data.FindMap(s.data.World().defaultMap) == nullptr) return false;
  s.ResetSession();
  s.rng.SeedAll(seed != 0 ? seed : s.config.defaultSeed);
  s.BuildSystems(cls);
  s.session.slot = slot;
  s.session.difficulty = difficulty;
  s.session.newGame = true;
  s.session.nextAutosaveAtMs = s.config.autosaveIntervalMs;
  s.hero->Skills().InitStarterLevels();
  s.RebuildEquipStats();
  s.hero->RecalcDerived(s.ctx.equip);
  s.hero->FillHpMana();
  s.hasSession = true;
  const bool ok = s.EnterZone(s.data.World().defaultMap, false, Vec2());
  s.session.newGame = false;
  s.BuildSnapshot();
  return ok;
}

SaveError GameSim::LoadGame(std::string_view saveJson, std::string* err, std::optional<Difficulty> difficultyOverride) {
  SaveData save;
  const SaveError e = ParseSave(saveJson, save, err);
  if (e != SaveError::None) return e;
  if (difficultyOverride.has_value()) {
    // save-ui-input 1.2: the selector shows getDifficultyStates(deriveCompletedDifficulties(saved, list)); the picked
    // difficulty replaces the saved one, hero / map / position are kept (Q33).
    const std::vector<Difficulty> completed = DeriveCompletedDifficulties(save.difficulty, save.completedDifficulties);
    if (GetDifficultyStates(completed).Of(*difficultyOverride) == DifficultyState::Locked) {
      if (err != nullptr) *err = "difficulty locked";
      return SaveError::Invalid;
    }
    save.completedDifficulties = completed;
    save.difficulty = *difficultyOverride;
  }
  return impl_->ApplySave(save, err);
}

SaveError GameSim::LoadGame(const SaveData& save, std::string* err) { return impl_->ApplySave(save, err); }

std::string GameSim::SaveGame(int64_t unixMs) const {
  SaveData out;
  impl_->BuildSave(out, unixMs);
  return SerializeSave(out);
}

bool GameSim::CanSave() const { return impl_->CanSave(); }

void GameSim::BuildSave(SaveData& out, int64_t unixMs) const { impl_->BuildSave(out, unixMs); }

void GameSim::Step() {
  impl_->events.Clear();
  if (impl_->hasSession) impl_->StepOnce();
  impl_->BuildSnapshot();
}

int32_t GameSim::Frame(double realDtMs) {
  SimImpl& s = *impl_;
  s.events.Clear();
  int32_t steps = 0;
  if (s.hasSession) {
    s.SyncFreeze();
    s.clock.AccumulateRealTime(realDtMs);
    if (s.WorldFrozen()) {
      s.StepOnce();  // applies UI commands only
    } else {
      while (s.clock.ConsumeStep()) {
        s.StepOnce();
        ++steps;
        if (s.WorldFrozen()) break;
      }
    }
    s.clock.EndFrame();
  }
  AdvanceRealTime(realDtMs);
  s.BuildSnapshot();
  return steps;
}

void GameSim::AdvanceRealTime(double realMs) {
  SimImpl& s = *impl_;
  if (!(realMs > 0) || !s.hasSession) return;
  s.clock.AdvanceRealClock(realMs);
  s.story->AdvanceRealTime(realMs);
  s.audio->AdvanceRealTime(realMs);
}

void GameSim::Submit(const Command& cmd) { impl_->pending.push_back(cmd); }

std::span<const Event> GameSim::Events() const { return impl_->events.Items(); }

void GameSim::ClearEvents() { impl_->events.Clear(); }

const Snapshot& GameSim::View() const { return impl_->snapshot; }

bool GameSim::WorldFrozen() const { return impl_->WorldFrozen(); }

bool GameSim::HasSession() const { return impl_->hasSession; }

double GameSim::NowMs() const { return impl_->clock.NowMs(); }

SimContext& GameSim::Context() { return impl_->ctx; }

const SimContext& GameSim::Context() const { return impl_->ctx; }

}  // namespace abyss

// AutoPilot (abyss/tools/AutoPilot.h): the Chapter-1 bot as a stack of resumable tasks, driven by a route of
// autopilot_routes.json (implementation-plan.md 6).
//
// Every blocking action of the scripted player (walk somewhere, fight a monster, talk to an NPC, work a quest card,
// ...) is an ApTask whose Run() continues it until it needs the next frame (ApRun::Frame), starts a child task
// (ApRun::Call: the child runs at once; when it is Done the parent's Run() resumes with its result in
// AutoPilotImpl::childResult) or ends (ApRun::Done). Locals that live across frames are members and `st_` says where
// Run() resumes, so a task issues exactly the commands, in the same frames, as the straight-line loop it replaces (the
// playthrough test's former bot: same final saves). The script is the bottom task: its steps are task factories, notes
// and checkpoints (ApRun::Checkpoint), built from the route's steps (BuildScenario); every content id comes from the
// route or the other tables, none is spelled here (the K1 grep gate).
//
// Owner area: tools (shared by CoreTests/tests/playthrough_test.cpp and the UE in-game autotest).
#include "abyss/base/Platform.h"

#include "abyss/tools/AutoPilot.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <deque>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "abyss/base/Math.h"
#include "abyss/data/DataStore.h"
#include "abyss/hero/Skills.h"
#include "abyss/items/Inventory.h"
#include "abyss/items/ItemCompare.h"
#include "abyss/items/Shop.h"
#include "abyss/monsters/MonsterAbilities.h"
#include "abyss/pets/PetSystem.h"
#include "abyss/quests/Dialogue.h"
#include "abyss/quests/Lore.h"
#include "abyss/quests/QuestWorld.h"
#include "abyss/sim/GameSim.h"
#include "abyss/sim/Snapshot.h"
#include "abyss/story/StoryDirector.h"
#include "abyss/world/Grid.h"
#include "abyss/world/RandomEvents.h"

namespace abyss {

namespace autopilot_impl {

constexpr int64_t kApFps = 60;              // bot frames per second (kSimStepMs frames)
constexpr double kApThreatRadius = 6.5;     // fight anything chasing / attacking within this many tiles
constexpr size_t kApRecentNotes = 80;
constexpr int kApMaxRunsPerAdvance = 200000;  // task resumes without a frame before the bot calls itself stuck

enum class ApRun : uint8_t { Frame, Call, Done, Checkpoint };

class ApTask {
 public:
  virtual ~ApTask() = default;
  virtual ApRun Run(AutoPilotImpl& b) = 0;
  bool result = false;

 protected:
  ApRun Finish(bool r) {
    result = r;
    return ApRun::Done;
  }
};

using ApTaskPtr = std::unique_ptr<ApTask>;
using ApTaskFactory = std::function<ApTaskPtr(AutoPilotImpl&)>;
using ApPredicate = std::function<bool(const AutoPilotImpl&)>;
using ApMonsterFilter = std::function<bool(const MonsterView&)>;

}  // namespace autopilot_impl

using namespace autopilot_impl;

// ---------------------------------------------------------------------------------------------------------------------
// The bot's state, queries and per-frame upkeep
// ---------------------------------------------------------------------------------------------------------------------
struct AutoPilotImpl {
  AutoPilotImpl(const DataStore& d, AutoPilotConfig c, AutoPilotSubmit s)
      : data(d), cfg(std::move(c)), submit(std::move(s)) {}
  ~AutoPilotImpl() {
    pendingCall.reset();
    while (!stack.empty()) stack.pop_back();  // innermost first (panel-session guards point back here)
  }
  AutoPilotImpl(const AutoPilotImpl&) = delete;
  AutoPilotImpl& operator=(const AutoPilotImpl&) = delete;

  // ---- wiring ----
  const DataStore& data;
  AutoPilotConfig cfg;
  AutoPilotSubmit submit;
  const Snapshot* view = nullptr;  // the owner's snapshot during a call (also kept for notes between calls)

  // ---- outcome ----
  bool ok = true;
  std::string failure;
  AutoPilotStats stats;
  std::vector<AutoPilotMilestoneRecord> milestones;
  std::string checkpoint, currentStep;
  int64_t frames = 0;
  bool hold = false;
  bool frameOpen = false;     // between PrepareFrame and EndFrame: the owner is running a frame
  bool frameCounted = false;  // `frames` already advanced for that frame (at its first event)
  bool finishedNoted = false;

  // ---- per-frame bookkeeping (the former bot's private state) ----
  std::deque<std::string> recent;
  int64_t storyFrames = 0, miniBossFrames = 0, loreFrames = 0, strayFrames = 0;
  int panelSession = 0;  // > 0 while the bot works a panel it opened itself
  bool feedRefused = false;
  double lastHpPotionMs = -1e9, lastMpPotionMs = -1e9;
  int32_t allocated = 0, allocPending = -1, learnPending = -1;
  int64_t allocPendingAt = 0, learnPendingAt = 0;
  std::set<EntityId> skippedLoot;
  struct SeenMonster {
    EntityId id = kNoEntity;
    std::string defId;
    MonsterRole role = MonsterRole::Regular;
  };
  std::vector<SeenMonster> monsterDefs;  // the monsters before the frame (kill attribution)
  int32_t lastSteps = 0;
  bool lastFrozen = false;
  double heroHpTrack = 0;  // the hero's life before the next hit of the frame (one-hit deaths)
  std::deque<std::pair<double, double>> burst;  // 2F review: {sim ms, damage} the hero took in the last second
  // 2F review: the ability each monster is winding up ({entity, ability id, contact}): a cancel before contact counts.
  struct PendingUse {
    EntityId entity = kNoEntity;
    std::string id;
    double contactMs = 0;
  };
  std::vector<PendingUse> pendingUses;

  // ---- script ----
  struct Step {
    enum class Kind : uint8_t { Task, Note, Checkpoint, Milestone } kind = Kind::Task;
    std::string text;  // task label, note, checkpoint id or milestone detail
    ApTaskFactory make;
  };
  std::vector<Step> steps;
  std::string routeZone;  // the zone of the route the scenario plays (ChapterDone detail)
  size_t stepIdx = 0;
  bool childResult = false;
  ApTaskPtr pendingCall;
  std::vector<ApTaskPtr> stack;  // declared last: destroyed first

  // ---- task plumbing ----
  template <class T, class... A>
  ApRun Call(A&&... args) {
    pendingCall = std::make_unique<T>(std::forward<A>(args)...);
    return ApRun::Call;
  }
  void Submit(const Command& c) {
    if (submit) submit(c);
  }

  // ---- queries ----
  const Snapshot& V() const { return *view; }
  Vec2 HeroPos() const { return V().hero.pos; }
  double Now() const { return V().simNowMs; }
  bool Alive() const { return V().hero.life == HeroLife::Alive && V().hero.hp > 0; }
  // Story, modals and transitions: nothing for the hero to do but wait.
  bool Busy() const {
    const Snapshot& v = V();
    return v.frozen || v.inputBlocked || v.storyBusy || !v.modals.empty();
  }
  const MonsterView* FindMonster(EntityId id) const {
    for (const MonsterView& m : V().monsters) {
      if (m.id == id) return &m;
    }
    return nullptr;
  }
  const NpcView* FindNpc(const std::string& npcId) const {
    for (const NpcView& n : V().npcs) {
      if (n.npcId == npcId) return &n;
    }
    return nullptr;
  }
  EntityId NearestThreat(double radius) const {
    EntityId best = kNoEntity;
    double bestD = radius;
    for (const MonsterView& m : V().monsters) {
      if (!m.alive || (m.state != MonsterState::Chase && m.state != MonsterState::Attack)) continue;
      const double d = Dist(m.pos, HeroPos());
      if (d <= bestD) {
        bestD = d;
        best = m.id;
      }
    }
    return best;
  }
  EntityId NearestMatching(const ApMonsterFilter& match, Vec2 from, double maxDist) const {
    EntityId best = kNoEntity;
    double bestD = maxDist;
    for (const MonsterView& m : V().monsters) {
      if (!m.alive || m.state == MonsterState::Returning) continue;
      if (!match(m)) continue;
      const double d = Dist(m.pos, from);
      if (d < bestD) {
        bestD = d;
        best = m.id;
      }
    }
    return best;
  }
  const QuestProgress* Progress(const std::string& questId) const {
    return V().quests != nullptr ? V().quests->Progress(questId) : nullptr;
  }
  bool HasStatus(const std::string& questId, QuestStatus s) const {
    const QuestProgress* p = Progress(questId);
    return p != nullptr && p->status == s;
  }
  bool QuestDone(const std::string& questId) const {  // completed or turned in
    const QuestProgress* p = Progress(questId);
    return p != nullptr && (p->status == QuestStatus::Completed || p->status == QuestStatus::TurnedIn);
  }
  int32_t ObjCurrent(const std::string& questId, size_t obj) const {
    const QuestProgress* p = Progress(questId);
    return p != nullptr && obj < p->objectives.size() ? p->objectives[obj] : 0;
  }
  // Every condition of a route step's `until` list holds.
  bool Holds(const std::vector<AutoPilotCondition>& conds) const {
    for (const AutoPilotCondition& c : conds) {
      switch (c.kind) {
        case AutoPilotConditionKind::QuestDone:
          if (!QuestDone(c.quest)) return false;
          break;
        case AutoPilotConditionKind::Objective:
          if (ObjCurrent(c.quest, static_cast<size_t>(c.obj)) < c.atLeast) return false;
          break;
      }
    }
    return true;
  }
  // The NPC that gives `questId` (npcs.json quest lists): where a failed escort or defend is taken again.
  std::string GiverOf(const std::string& questId) const {
    const NpcDef* n = data.Npcs().GiverOf(questId);
    return n != nullptr ? n->id : std::string();
  }
  int32_t CountOf(const std::string& baseId) const {
    return V().inventory != nullptr ? V().inventory->CountOf(baseId) : 0;
  }
  int HpPotionTarget() const { return 8; }
  int MpPotionTarget() const { return cfg.cls == ClassId::Mage ? 6 : 2; }

  // ---- gear ----
  double StatWeight(Stat s) const {
    const bool war = cfg.cls == ClassId::Warrior, mage = cfg.cls == ClassId::Mage, rog = cfg.cls == ClassId::Rogue;
    switch (s) {
      case Stat::Damage: return 2.0;
      case Stat::DamagePercent: return 1.5;
      case Stat::Defense: return 0.6;
      case Stat::DefensePercent: return 0.5;
      case Stat::MaxHp: return 0.25;
      case Stat::MaxHpPercent: return 1.0;
      case Stat::MaxMana: return mage ? 0.2 : 0.05;
      case Stat::MaxManaPercent: return mage ? 0.6 : 0.1;
      case Stat::CritRate: return 1.5;
      case Stat::CritDamage: return 0.5;
      case Stat::AttackSpeed: return 1.5;
      case Stat::LifeSteal: return 2.0;
      case Stat::ManaSteal: return 0.5;
      case Stat::HpRegen: return 1.0;
      case Stat::ManaRegen: return mage ? 1.0 : 0.3;
      case Stat::FireDamage:
      case Stat::IceDamage:
      case Stat::LightningDamage:
      case Stat::PoisonDamage: return 1.0;
      case Stat::AllResist: return 0.5;
      case Stat::MoveSpeed: return 0.5;
      case Stat::Str: return war ? 2.0 : 0.3;
      case Stat::Dex: return rog ? 2.0 : 0.5;
      case Stat::Int: return mage ? 2.0 : 0.2;
      case Stat::Vit: return 1.2;
      case Stat::Spi: return mage ? 0.6 : 0.2;
      case Stat::Lck: return 0.4;
      case Stat::AllStats: return 3.0;
      default: return 0.3;
    }
  }
  double Score(const ItemInstance* it) const {
    if (it == nullptr) return 0;
    double s = 1.0;  // anything beats an empty slot
    for (const StatTotal& t : ItemStatTotals(data, *it)) {
      switch (t.key.kind) {
        case CompareKey::Kind::AvgDamage: break;  // inert in milestone 1 (C10)
        case CompareKey::Kind::BaseDefense: s += t.value * 0.4; break;
        case CompareKey::Kind::StatKey: s += t.value * StatWeight(t.key.stat); break;
      }
    }
    return s;
  }
  bool Equippable(const ItemInstance& it) const {
    const ItemBaseDef* base = data.FindItemBase(it.baseId);
    return base != nullptr && base->hasSlot && base->levelReq <= V().hero.level;
  }
  // Positive when `it` would be better than what its slot holds now.
  double UpgradeGain(const ItemInstance& it) const {
    if (V().inventory == nullptr || !Equippable(it)) return 0;
    const std::optional<CompareTarget> target = FindCompareTarget(data, it, V().inventory->Equipment());
    if (!target.has_value()) return 0;
    return Score(&it) - Score(target->equipped);
  }
  bool Wanted(const ItemInstance& it) const {
    const ItemBaseDef* base = data.FindItemBase(it.baseId);
    if (base == nullptr) return false;
    if (!base->hasSlot) return true;  // potions, gems, materials, ley fruit
    if (it.quality != ItemQuality::Normal) return true;
    return UpgradeGain(it) > 0.5;
  }
  // Picks the reward choice that improves the gear most.
  int32_t BestChoice(const std::vector<ItemInstance>& choices) const {
    int32_t best = 0;
    double bestGain = -1e9;
    for (size_t i = 0; i < choices.size(); ++i) {
      const ItemInstance& it = choices[i];
      double g = Score(&it);
      if (V().inventory != nullptr) {
        const std::optional<CompareTarget> t = FindCompareTarget(data, it, V().inventory->Equipment());
        if (t.has_value()) g -= Score(t->equipped);
      }
      if (g > bestGain) {
        bestGain = g;
        best = static_cast<int32_t>(i);
      }
    }
    return best;
  }
  int32_t WareIndex(const std::string& baseId) const {
    if (V().shop == nullptr) return -1;
    for (size_t i = 0; i < V().shop->wares.size(); ++i) {
      if (V().shop->wares[i].baseId == baseId) return static_cast<int32_t>(i);
    }
    return -1;
  }
  int64_t WarePrice(int32_t index) const {
    if (V().shop == nullptr || index < 0 || static_cast<size_t>(index) >= V().shop->wares.size()) return INT64_MAX;
    return V().shop->wares[static_cast<size_t>(index)].price;
  }

  // ---- combat ----
  static bool SkipSkill(const std::string& id) {
    return id == "teleport" || id == "vanish" || id == "shadow_step" || id == "taunt_roar";
  }
  bool TryCast(const MonsterView& m) {
    const HeroView& h = V().hero;
    const double d = Dist(m.pos, h.pos);
    for (int slot = 0; slot < 6; ++slot) {
      const SkillSlotView& s = h.hotbar[static_cast<size_t>(slot)];
      if (s.skillIndex < 0 || !s.usable) continue;
      const SkillDef* def = data.FindSkill(s.skillId);
      if (def == nullptr || def->passive || SkipSkill(def->id)) continue;
      if (def->range > 0 ? d > def->range + 0.25 : d > 2.5) continue;
      Submit(CmdCastSkill{slot, m.id, false, Vec2(), Vec2()});
      ++stats.skillCasts;
      return true;
    }
    return false;
  }
  // ---- W2 rules (dark remake 2F; implementation-plan.md 6): telegraphs, hazards, arenas, priority targets, Feed ----
  // Every rule reads the snapshot (and the definitions in `data`), never a content id, and works for every class.
  //
  // Danger: a damaging ground telegraph whose contact has not come (a K2 strike: circle / cone / line / ring / hazard;
  // a K3 on-death fuse), or a live K2 hazard. Raise / summon markers are no danger.
  struct ApDanger {
    AbilityShapeDef shape;
    Vec2 origin, dir{1, 0};
    double contactMs = 0;  // a telegraph's contact; a hazard's end
    bool hazard = false;
  };
  static bool DamagingTelegraph(AbilityKind k) {
    return k == AbilityKind::Circle || k == AbilityKind::Cone || k == AbilityKind::Line || k == AbilityKind::Ring ||
           k == AbilityKind::Hazard;
  }
  std::vector<ApDanger> Dangers() const {
    std::vector<ApDanger> out;
    const double now = Now();
    for (const TelegraphView& t : V().telegraphs) {
      if (!DamagingTelegraph(t.kind) || t.contactMs <= now) continue;
      if (now < t.startMs + cfg.reactionMs) continue;  // 2F review: not seen yet (AutoPilotConfig::reactionMs)
      out.push_back(ApDanger{t.shape, t.origin, t.dir, t.contactMs, false});
    }
    for (const HazardView& hz : V().hazards) {
      if (hz.untilMs <= now || now < hz.startMs + cfg.reactionMs) continue;
      out.push_back(ApDanger{hz.shape, hz.origin, hz.dir, hz.untilMs, true});
    }
    return out;
  }
  // The hero's body (0.35) plus `margin` touches a danger.
  static bool InDanger(const std::vector<ApDanger>& ds, Vec2 p, double margin) {
    for (const ApDanger& d : ds) {
      if (ShapeContains(d.shape, d.origin, d.dir, p, kAbilityBodyRadiusTiles + margin)) return true;
    }
    return false;
  }
  // Inside every K4 arena with `margin` to spare (its ring hurts outside).
  bool InsideArenas(Vec2 p, double margin) const {
    for (const ArenaView& a : V().arenas) {
      if (Dist(p, a.centre) > a.radius - margin) return false;
    }
    return true;
  }
  bool WalkableAt(Vec2 p) const { return V().zone.grid == nullptr || V().zone.grid->WalkableAt(p); }
  bool StraightWalkable(Vec2 a, Vec2 b) const {
    const double len = Dist(a, b);
    const int n = static_cast<int>(std::ceil(len / 0.4));
    for (int i = 1; i <= n; ++i) {
      if (!WalkableAt(a + (b - a) * (static_cast<double>(i) / n))) return false;
    }
    return true;
  }
  // The nearest spot out of every danger and inside the arenas, reached in a straight walkable line; among spots as near
  // (16 directions on rings of growing radius), the one nearest `prefer` (the fight).
  // `fence` (a named fight's home and radius, else none): no spot farther out than both the radius and the hero now.
  std::optional<Vec2> SafeSpot(const std::vector<ApDanger>& ds, Vec2 prefer, const Vec2* fence = nullptr,
                               double fenceR = 0) const {
    const Vec2 h = HeroPos();
    static constexpr double kRadii[] = {0.5, 0.9, 1.3, 1.8, 2.3, 2.9, 3.6, 4.5, 5.5};
    for (double r : kRadii) {
      std::optional<Vec2> best;
      double bestD = 1e18;
      for (int k = 0; k < 16; ++k) {
        const double a = k * kPi / 8.0;
        const Vec2 p = h + Vec2(DetCos(a), DetSin(a)) * r;
        if (!WalkableAt(p) || InDanger(ds, p, 0.25) || !InsideArenas(p, 0.75) || !StraightWalkable(h, p)) continue;
        if (fence != nullptr && Dist(p, *fence) > fenceR && Dist(p, *fence) > Dist(h, *fence)) continue;
        const double d = DistSq(p, prefer);
        if (d < bestD) {
          bestD = d;
          best = p;
        }
      }
      if (best.has_value()) return best;
    }
    return std::nullopt;
  }
  // Tile vector -> CmdDodge's screen direction (the inverse of Locomotion's ScreenDirToTile).
  static Vec2 TileDirToScreen(Vec2 t) { return Vec2((t.x - t.y) * 0.5, (t.x + t.y) * 0.5); }
  // Out of a danger / back inside an arena: true when it moved the hero (the caller skips its attack this frame).
  int64_t evadeOrderFrame = -1000;
  Vec2 evadeSpot;
  bool evading = false;
  bool Evade(Vec2 prefer, const Vec2* fence = nullptr, double fenceR = 0) {
    const Vec2 h = HeroPos();
    const std::vector<ApDanger> ds = Dangers();
    const bool danger = InDanger(ds, h, 0.1);
    const bool outside = !InsideArenas(h, 0.4);
    if (!danger && !outside) {
      evading = false;
      return false;
    }
    std::optional<Vec2> spot = SafeSpot(ds, prefer, fence, fenceR);
    if (!spot.has_value() && fence != nullptr) spot = SafeSpot(ds, prefer);
    if (!spot.has_value() && outside) {  // no spot in reach: walk toward the arena's centre
      for (const ArenaView& a : V().arenas) {
        if (Dist(h, a.centre) > a.radius - 0.4) {
          spot = a.centre + (h - a.centre).Normalized() * (std::max)(0.0, a.radius - 2.0);
        }
      }
    }
    if (!spot.has_value()) return false;  // cornered: fight on
    if (!evading) {
      evading = true;
      ++stats.telegraphDodges;
    }
    // The dodge roll's i-frames take a contact the walk cannot outrun (a strike due within 200 ms).
    double soonest = 1e18;
    for (const ApDanger& d : ds) {
      if (!d.hazard && ShapeContains(d.shape, d.origin, d.dir, h, kAbilityBodyRadiusTiles)) {
        soonest = (std::min)(soonest, d.contactMs);
      }
    }
    if (cfg.dodgeRoll && soonest - Now() <= 200 && V().hero.dodgeCooldownRemainingMs <= 0 && Dist(*spot, h) > 0.3) {
      Submit(CmdDodge{TileDirToScreen(*spot - h).Normalized()});
      evadeOrderFrame = frames;
      evadeSpot = *spot;
      return true;
    }
    if (frames - evadeOrderFrame > 8 || Dist(*spot, evadeSpot) > 0.75) {
      Submit(CmdMoveTo{*spot});
      evadeOrderFrame = frames;
      evadeSpot = *spot;
    }
    return true;
  }
  // A strike is coming: hold off an attack that would walk into it (the target stands in it, or the approach to the
  // hero's reach crosses it). Long hazards hold only near their end (else the hero would wait out a 4 s brand).
  bool ApproachBlocked(const MonsterView& m) const {
    const std::vector<ApDanger> all = Dangers();
    std::vector<ApDanger> ds;
    const double now = Now();
    for (const ApDanger& d : all) {
      if (!d.hazard || d.contactMs - now <= 1500) ds.push_back(d);
    }
    if (ds.empty()) return false;
    const Vec2 h = HeroPos();
    const double reach = V().hero.derived.attackRange;
    const double d = Dist(h, m.pos);
    if (d <= reach + 0.5) return false;  // it can strike from where it stands
    const Vec2 stop = m.pos + (h - m.pos).Normalized() * reach;
    const int n = static_cast<int>(std::ceil(Dist(h, stop) / 0.4));
    for (int i = 1; i <= n; ++i) {
      if (InDanger(ds, h + (stop - h) * (static_cast<double>(i) / n), 0.1)) return true;
    }
    return false;
  }
  // Priority targets (implementation-plan.md 6 "target priority": raisers, standard-bearers, drummers first): a support
  // caster (a raise / summon / buff / call ability) or an aura owner 2, a pack leader 1, the rest 0.
  const MonsterDef* DefOf(const std::string& defId) const {
    if (const MonsterDef* d = data.Monsters().Find(defId)) return d;
    const HuntDef* h = data.Monsters().FindHunt(defId);
    return h != nullptr ? &h->defNormal : nullptr;
  }
  int ThreatPriority(const MonsterView& m) const {
    if (m.auraRadius > 0) return 2;
    if (const MonsterDef* d = DefOf(m.defId)) {
      for (const MonsterAbilityDef& a : d->abilities) {
        if (AbilityKindCastsBetweenSwings(a.kind)) return 2;
      }
    }
    return m.packLeader ? 1 : 0;
  }
  // The fight to take within `radius`: a priority target first (within the radius + 1.5), else the nearest.
  EntityId BestThreat(double radius) const {
    EntityId best = kNoEntity;
    int bestP = -1;
    double bestD = 0;
    for (const MonsterView& m : V().monsters) {
      if (!m.alive || (m.state != MonsterState::Chase && m.state != MonsterState::Attack)) continue;
      const double d = Dist(m.pos, HeroPos());
      const int p = ThreatPriority(m);
      if (d > (p > 0 ? radius + 1.5 : radius)) continue;
      if (p > bestP || (p == bestP && d < bestD)) {
        bestP = p;
        bestD = d;
        best = m.id;
      }
    }
    return best;
  }
  // Feed (K4, a boss healing from its Raised killed near its centre; 2C handoff "kill adds away from a feeding boss's
  // centre"): the zones of the living monsters in a feeding phase, centre = the row's, else the monster's authored map
  // spawn tile (its dais), radius = the row's.
  struct ApFeedZone {
    Vec2 centre;
    double radius = 0;
  };
  std::vector<ApFeedZone> FeedZones() const {
    std::vector<ApFeedZone> out;
    for (const MonsterView& m : V().monsters) {
      if (!m.alive || m.phase <= 0) continue;
      const MonsterDef* d = DefOf(m.defId);
      if (d == nullptr || static_cast<size_t>(m.phase) > d->phases.size()) continue;
      const PhaseFeedDef& f = d->phases[static_cast<size_t>(m.phase - 1)].feed;
      if (!f.present) continue;
      std::optional<Vec2> centre;
      if (f.hasCentre) centre = f.centre.Center();
      if (!centre.has_value()) {
        if (const MapDef* map = data.World().FindMap(V().zone.mapId)) {
          for (const MapSpawnDef& s : map->spawns) {
            if (s.monsterId == m.defId && !centre.has_value()) centre = s.pos.Center();
          }
        }
      }
      if (centre.has_value()) out.push_back(ApFeedZone{*centre, f.radius});
    }
    return out;
  }
  // A Raised standing inside a feed zone: the spot to lure it out to (the zone's edge + 2.5 on the hero's side).
  std::optional<Vec2> LureSpot(const MonsterView& m) const {
    if (m.role != MonsterRole::Raised) return std::nullopt;
    for (const ApFeedZone& z : FeedZones()) {
      if (Dist(m.pos, z.centre) >= z.radius + 0.5) continue;
      Vec2 away = HeroPos() - z.centre;
      if (away.Length() < 0.5) away = m.pos - z.centre;
      if (away.Length() < 1e-6) away = Vec2(1, 0);
      away = away.Normalized();
      for (int k = 0; k < 8; ++k) {  // the away vector, then turned 30 degrees each way
        const double turn = (k % 2 == 0 ? 1 : -1) * ((k + 1) / 2) * (kPi / 6.0);
        const Vec2 dir(away.x * DetCos(turn) - away.y * DetSin(turn), away.x * DetSin(turn) + away.y * DetCos(turn));
        const Vec2 p = z.centre + dir * (z.radius + 2.5);
        if (WalkableAt(p) && InsideArenas(p, 0.75)) return p;
      }
    }
    return std::nullopt;
  }

  PrimaryStat MainStat() const {
    switch (cfg.cls) {
      case ClassId::Warrior: return PrimaryStat::Str;
      case ClassId::Mage: return PrimaryStat::Int;
      case ClassId::Rogue: return PrimaryStat::Dex;
    }
    return PrimaryStat::Vit;
  }
  std::vector<std::string> SkillPriority() const {
    switch (cfg.cls) {
      case ClassId::Warrior: return {"whirlwind", "slash"};
      case ClassId::Mage: return {"fire_wall", "fireball"};
      case ClassId::Rogue: return {"piercing_arrow", "multishot"};
    }
    return {};
  }

  // ---- diary ----
  void Note(const std::string& s) {
    char head[112];
    const std::string cls(EnumName(cfg.cls));
    if (view != nullptr) {
      std::snprintf(head, sizeof(head), "[%s %7.1fs L%d hp %.0f/%.0f (%.1f,%.1f)] ", cls.c_str(),
                    frames / static_cast<double>(kApFps), view->hero.level, view->hero.hp, view->hero.maxHp,
                    view->hero.pos.x, view->hero.pos.y);
    } else {
      std::snprintf(head, sizeof(head), "[%s %7.1fs] ", cls.c_str(), frames / static_cast<double>(kApFps));
    }
    std::string line = std::string(head) + s;
    if (cfg.onNote) cfg.onNote(line);
    recent.push_back(std::move(line));
    if (recent.size() > kApRecentNotes) recent.pop_front();
  }
  void Fail(const std::string& why) {
    if (!ok) return;
    ok = false;
    failure = why + " | " + StateLine();
    Note("FAIL: " + failure);
  }
  std::string StateLine() const {
    if (view == nullptr) return "no snapshot";
    const Snapshot& v = V();
    std::string s = "frozen=" + std::to_string(v.frozen) + " mask=" + std::to_string(v.freezeMask) +
                    " blocked=" + std::to_string(v.inputBlocked) + " storyBusy=" + std::to_string(v.storyBusy) +
                    " cinematic=" + std::to_string(v.cinematic) + " moving=" + std::to_string(v.hero.moving) +
                    " target=" + std::to_string(v.hero.target) + " modals=[";
    for (PanelId p : v.modals) s += std::string(EnumName(p)) + " ";
    s += "]";
    if (v.escort != nullptr && v.escort->active) {
      char buf[160];
      std::snprintf(buf, sizeof(buf), " escort=(%.2f,%.2f) joined=%d hp=%.0f path=%zu", v.escort->pos.x,
                    v.escort->pos.y, v.escort->joined ? 1 : 0, v.escort->hp, v.escort->path.size());
      s += buf;
    }
    if (v.story != nullptr && v.story->playing) {
      s += " beat=" + v.story->beat.id + " seg=" + std::to_string(v.story->segment) +
           " waiting=" + std::to_string(v.story->waitingForInput);
    }
    return s;
  }
  void Milestone(AutoPilotMilestone kind, std::string detail) {
    AutoPilotMilestoneRecord r;
    r.kind = kind;
    r.detail = std::move(detail);
    r.frame = frames;
    r.simMs = view != nullptr ? view->simNowMs : 0;
    milestones.push_back(r);
    if (cfg.onMilestone) cfg.onMilestone(milestones.back());
  }
  bool Reached(AutoPilotMilestone kind) const {
    for (const AutoPilotMilestoneRecord& r : milestones) {
      if (r.kind == kind) return true;
    }
    return false;
  }

  // ---- per frame, before the sim frame: story beats, dialogue prompts, stray panels ----
  void AutoUi() {
    const Snapshot& v = V();
    if (v.story != nullptr && v.story->playing) {
      ++storyFrames;
      const bool skip = cfg.storyMode == AutoPilotStoryMode::SkipAll || v.story->beat.id == "prologue";
      if (skip) {
        if (storyFrames % 10 == 1) Submit(CmdStorySkip{});
      } else if (storyFrames % 15 == 0) {
        Submit(CmdStoryAdvance{});
      }
    } else {
      storyFrames = 0;
    }
    if (v.miniBossDialogue) {
      if (++miniBossFrames % 40 == 0) Submit(CmdMiniBossDialogueDismiss{});
    } else {
      miniBossFrames = 0;
    }
    if (v.loreText != nullptr && v.loreText->open) {
      if (++loreFrames % 40 == 0) Submit(CmdClosePanel{PanelId::LoreText});
    } else {
      loreFrames = 0;
    }
    if (v.puzzle != nullptr && v.puzzle->open && (frames % 30) == 0) {
      Submit(CmdPuzzleAnswer{v.puzzle->prop, kPuzzleChoiceSolve});
    }
    // A panel the bot did not open (the wandering merchant's shop opens on its own, world 13): look, then close it.
    if (panelSession == 0 && !v.storyBusy && !v.modals.empty()) {
      if (++strayFrames % 45 == 0) {
        for (PanelId p : v.modals) {
          if (p != PanelId::MiniBossDialogue && p != PanelId::LoreText && p != PanelId::Puzzle) {
            Submit(CmdClosePanel{p});
            Note(std::string("closing stray panel ") + std::string(EnumName(p)));
          }
        }
      }
    } else {
      strayFrames = 0;
    }
  }

  // ---- per frame, after the sim frame: potions, stat and skill points ----
  void AutoHero() {
    const Snapshot& v = V();
    const HeroView& h = v.hero;
    if (h.life != HeroLife::Alive) return;
    if (!v.frozen && !v.inputBlocked && h.hp > 0) {
      if (h.hp < 0.35 * h.maxHp && v.potionSlots[0].count > 0 && Now() - lastHpPotionMs >= 900) {
        Submit(CmdUsePotion{PotionSlot::Hp});
        lastHpPotionMs = Now();
        ++stats.potionsDrunk;
      }
      if (h.mana < 0.2 * h.maxMana && v.potionSlots[1].count > 0 && Now() - lastMpPotionMs >= 900 && h.inCombat) {
        Submit(CmdUsePotion{PotionSlot::Mp});
        lastMpPotionMs = Now();
        ++stats.potionsDrunk;
      }
    }
    if (h.freeStatPoints > 0 && allocPending == h.freeStatPoints && frames - allocPendingAt > 30) allocPending = -1;
    if (h.freeSkillPoints > 0 && learnPending == h.freeSkillPoints && frames - learnPendingAt > 30) {
      learnPending = -1;
    }
    if (h.freeStatPoints > 0 && allocPending != h.freeStatPoints) {
      static constexpr int kPattern[5] = {0, 0, 1, 0, 1};  // 0 main, 1 vit
      const PrimaryStat s = kPattern[allocated % 5] == 0 ? MainStat() : PrimaryStat::Vit;
      Submit(CmdAllocStat{s, 1});
      allocPending = h.freeStatPoints;
      allocPendingAt = frames;
      ++allocated;
    } else if (h.freeStatPoints == 0) {
      allocPending = -1;
    }
    if (h.freeSkillPoints > 0 && v.skills != nullptr && learnPending != h.freeSkillPoints) {
      for (const std::string& id : SkillPriority()) {
        const int32_t idx = v.skills->IndexOf(id);
        if (idx < 0) continue;
        if (!v.skills->GetInvestState(idx, h.level, h.freeSkillPoints).canInvest) continue;
        Submit(CmdLearnSkill{idx});
        learnPending = h.freeSkillPoints;
        learnPendingAt = frames;
        Note("learn " + id);
        break;
      }
    } else if (h.freeSkillPoints == 0) {
      learnPending = -1;
    }
  }

  // ---- events ----
  // `frames` advances once per frame, before the frame's events are processed. Events outside a frame (the ones
  // NewGame / LoadGame emit before the first frame, an immediate GameSim::Step) do not advance it.
  void CountFrame() {
    if (frameOpen && !frameCounted) {
      ++frames;
      frameCounted = true;
    }
  }
  void ProcessEvent(const Event& e) {
    if (const EvStoryBeat* b = std::get_if<EvStoryBeat>(&e)) {
      if (b->phase == EvStoryBeat::Phase::Began) {
        stats.beatsBegan.push_back(b->beatId);
        Note("story begins " + b->beatId);
        Milestone(AutoPilotMilestone::StoryBeat, b->beatId);
        if (b->kind == StoryBeatKind::Cutscene) Milestone(AutoPilotMilestone::Cutscene, b->beatId);
      } else {
        stats.beatsEnded.push_back(b->beatId);
        Note("story ends " + b->beatId);
      }
    } else if (const EvLevelUp* l = std::get_if<EvLevelUp>(&e)) {
      ++stats.levelUps;
      Note("level up " + std::to_string(l->level));
      Milestone(AutoPilotMilestone::LevelUp, std::to_string(l->level));
    } else if (std::holds_alternative<EvHeroDied>(e)) {
      ++stats.deaths;
      for (AutoPilotFight& f : stats.fights) {
        if (f.endMs < 0) ++f.heroDeaths;
      }
      Note("hero died");
      Milestone(AutoPilotMilestone::HeroDied, std::string());
    } else if (const EvExpGained* x = std::get_if<EvExpGained>(&e)) {
      const size_t src = static_cast<size_t>(x->source);
      if (src < stats.expBySource.size()) stats.expBySource[src] += x->amount;
    } else if (std::holds_alternative<EvItemPicked>(e)) {
      ++stats.itemsPicked;
    } else if (std::holds_alternative<EvEquipmentChanged>(e)) {
      ++stats.equipChanges;
    } else if (const EvQuestUpdate* q = std::get_if<EvQuestUpdate>(&e)) {
      static const char* kKinds[] = {"accepted", "progress", "completed", "turned in", "failed", "tracked"};
      if (q->kind != EvQuestUpdate::Kind::Tracked) {
        Note(std::string("quest ") + kKinds[static_cast<int>(q->kind)] + " " + q->questId +
             (q->kind == EvQuestUpdate::Kind::Progress
                  ? " #" + std::to_string(q->objectiveIndex) + " " + std::to_string(q->current) + "/" +
                        std::to_string(q->required)
                  : std::string()));
      }
      if (q->kind == EvQuestUpdate::Kind::Accepted) Milestone(AutoPilotMilestone::QuestAccepted, q->questId);
      if (q->kind == EvQuestUpdate::Kind::Completed) Milestone(AutoPilotMilestone::QuestCompleted, q->questId);
      if (q->kind == EvQuestUpdate::Kind::TurnedIn) Milestone(AutoPilotMilestone::QuestTurnedIn, q->questId);
    } else if (const EvBanner* bn = std::get_if<EvBanner>(&e)) {
      if (bn->kind == BannerKind::ComingSoon) ++stats.sealedBanners;
    } else if (const EvPet* p = std::get_if<EvPet>(&e)) {
      if (p->kind == EvPet::Kind::Obtained) {
        stats.petObtained = true;
        Note("pet obtained " + p->petId);
        Milestone(AutoPilotMilestone::PetObtained, p->petId);
      }
    } else if (const EvHit* h = std::get_if<EvHit>(&e)) {
      ObserveFightHit(*h);
      if (h->killed && h->targetFaction == Faction::Monster) {
        ++stats.kills;
        std::string killedDef;
        for (const SeenMonster& m : monsterDefs) {
          if (m.id != h->target) continue;
          killedDef = m.defId;
          if (std::find(stats.killedDefs.begin(), stats.killedDefs.end(), m.defId) == stats.killedDefs.end()) {
            stats.killedDefs.push_back(m.defId);
          }
          if (m.role == MonsterRole::HuntLeader || m.role == MonsterRole::ZoneMiniBoss ||
              m.role == MonsterRole::SubDungeonMiniBoss || m.role == MonsterRole::StoryBoss) {
            Note("killed " + m.defId);
            Milestone(AutoPilotMilestone::BossKill, m.defId);
          }
        }
        if (stats.kills == 1) Milestone(AutoPilotMilestone::FirstKill, killedDef);
      }
    } else if (const EvMonsterRaised* rs = std::get_if<EvMonsterRaised>(&e)) {
      ++stats.raised;
      if (rs->selfRise) ++stats.selfRises;
    } else if (std::holds_alternative<EvBlock>(e)) {
      ++stats.blocks;
    } else if (const EvMonsterFlee* fl = std::get_if<EvMonsterFlee>(&e)) {
      if (!fl->ended) ++stats.flees;
    } else if (const EvMonsterBuffCast* bc = std::get_if<EvMonsterBuffCast>(&e)) {
      ++stats.supportCasts;
      if (AutoPilotFight::AbilityUse* u = FightAbility(bc->entity, bc->abilityId)) ++u->started;  // resolved at contact
    } else if (const EvMonsterAttackCancelled* ac = std::get_if<EvMonsterAttackCancelled>(&e)) {
      const double now = view != nullptr ? view->simNowMs : 0;
      for (const PendingUse& pu : pendingUses) {
        if (pu.entity != ac->monster || now > pu.contactMs) continue;
        if (AutoPilotFight::AbilityUse* u = FightAbility(pu.entity, pu.id)) ++u->cancelled;
      }
      std::erase_if(pendingUses, [ac](const PendingUse& pu) { return pu.entity == ac->monster; });
    } else if (const EvCorpseRemoved* cr = std::get_if<EvCorpseRemoved>(&e)) {
      if (cr->reason == CorpseRemoveReason::Consumed) ++stats.corpsesEaten;
    } else if (const EvTelegraph* tg = std::get_if<EvTelegraph>(&e)) {
      if (tg->source == TelegraphSource::OnDeath) {
        ++stats.onDeathFuses;
      } else {
        ObserveAbilityStart(tg->entity, tg->abilityId, tg->contactMs);
      }
    } else if (const EvStatusApplied* sa = std::get_if<EvStatusApplied>(&e)) {
      if (sa->target == kHeroEntityId && sa->type == StatusType::Ashnumb) {
        ++stats.ashnumbHits;
        stats.ashnumbMaxStacks = (std::max)(stats.ashnumbMaxStacks, static_cast<int>(sa->value));
      }
    } else if (const EvRandomEvent* re = std::get_if<EvRandomEvent>(&e)) {
      if (re->champion && !re->resolved) ++stats.championAmbushes;
    } else if (std::holds_alternative<EvWorldFlag>(e)) {
      ++stats.worldFlags;
    } else if (std::holds_alternative<EvPermanentReward>(e)) {
      ++stats.permanentRewards;
    } else if (const EvMonsterPhase* ph = std::get_if<EvMonsterPhase>(&e)) {
      if (!ph->reset) ++stats.phases;
      if (AutoPilotFight* f = OpenFight(ph->entity)) {
        if (ph->reset) {
          f->phaseMs.clear();
          ++f->resets;
        } else {
          f->phaseMs.push_back(view != nullptr ? view->simNowMs : 0);
        }
      }
    } else if (const EvMonsterFeed* fd = std::get_if<EvMonsterFeed>(&e)) {
      if (AutoPilotFight* f = OpenFight(fd->entity)) ++f->feeds;
    } else if (std::holds_alternative<EvSaveRequested>(e)) {
      ++stats.saveRequests;
    } else if (const EvMiniBossDialogue* mb = std::get_if<EvMiniBossDialogue>(&e)) {
      if (mb->opened) {
        ++stats.miniBossDialogues;
        Note("mini-boss dialogue " + mb->monsterId);
      }
    } else if (const EvZone* z = std::get_if<EvZone>(&e)) {
      if (z->phase == EvZone::Phase::Entered) Milestone(AutoPilotMilestone::ZoneEntered, z->mapId);
    }
  }

  // Dark remake 2F: named fights (hunt leaders, mini-bosses, the story boss) from the first hit they and the hero trade
  // to their kill; the largest hit the hero takes; deaths to one hit. The stats only observe them (no command depends
  // on a fight record); a named monster's leash (MonsterView::leashCentre / leashRange) keeps the bot's fight inside it
  // (ApEngage).
  static bool NamedRole(MonsterRole r) {
    return r == MonsterRole::HuntLeader || r == MonsterRole::ZoneMiniBoss || r == MonsterRole::SubDungeonMiniBoss ||
           r == MonsterRole::StoryBoss;
  }
  // 2F review: an ability use of a named monster (AutoPilotFight::abilities); the uses before its fight opened (a raise
  // at the pull, before the first hit) wait in earlyUses and join the fight when it opens.
  std::vector<std::pair<EntityId, std::vector<AutoPilotFight::AbilityUse>>> earlyUses;
  AutoPilotFight::AbilityUse* FightAbility(EntityId entity, const std::string& id) {
    std::vector<AutoPilotFight::AbilityUse>* uses = nullptr;
    if (AutoPilotFight* f = OpenFight(entity)) {
      uses = &f->abilities;
    } else {
      const SeenMonster* m = SeenById(entity);
      if (m == nullptr || !NamedRole(m->role)) return nullptr;
      for (const AutoPilotFight& done : stats.fights) {
        if (done.entity == entity) return nullptr;  // its fight is over
      }
      for (auto& [e, v] : earlyUses) {
        if (e == entity) uses = &v;
      }
      if (uses == nullptr) {
        earlyUses.emplace_back(entity, std::vector<AutoPilotFight::AbilityUse>());
        uses = &earlyUses.back().second;
      }
    }
    for (AutoPilotFight::AbilityUse& u : *uses) {
      if (u.id == id) return &u;
    }
    uses->push_back(AutoPilotFight::AbilityUse{id, 0, 0});
    return &uses->back();
  }
  void ObserveAbilityStart(EntityId entity, const std::string& id, double contactMs) {
    for (const PendingUse& p : pendingUses) {
      if (p.entity == entity && p.id == id && p.contactMs == contactMs) return;  // a raise's other markers
    }
    std::erase_if(pendingUses, [entity](const PendingUse& p) { return p.entity == entity; });
    pendingUses.push_back(PendingUse{entity, id, contactMs});
    if (AutoPilotFight::AbilityUse* u = FightAbility(entity, id)) ++u->started;
  }
  const SeenMonster* SeenById(EntityId id) const {
    for (const SeenMonster& m : monsterDefs) {
      if (m.id == id) return &m;
    }
    return nullptr;
  }
  AutoPilotFight* OpenFight(EntityId id) {
    for (AutoPilotFight& f : stats.fights) {
      if (f.entity == id && f.endMs < 0) return &f;
    }
    return nullptr;
  }
  void ObserveFightHit(const EvHit& h) {
    const double now = view != nullptr ? view->simNowMs : 0;
    const bool onHero = h.target == kHeroEntityId;
    const bool byHero = h.source == kHeroEntityId;
    if (onHero && !h.dodged && !h.iframeAvoided && h.amount > 0) {
      const double before = heroHpTrack;
      heroHpTrack = h.targetHp;
      // 2F review: the burst - every hit and tick of the last 1000 ms.
      burst.emplace_back(now, h.amount);
      while (!burst.empty() && burst.front().first <= now - 1000.0) burst.pop_front();
      if (h.targetMaxHp > 0) {
        double sum = 0;
        for (const auto& [t, amount] : burst) sum += amount;
        stats.maxBurstTakenPct = (std::max)(stats.maxBurstTakenPct, 100.0 * sum / h.targetMaxHp);
      }
      const SeenMonster* by = SeenById(h.source);
      if (!h.tick && h.targetMaxHp > 0) {
        const double pct = 100.0 * h.amount / h.targetMaxHp;
        if (pct > stats.maxHitTakenPct) {
          stats.maxHitTakenPct = pct;
          stats.maxHitTakenBy = by != nullptr ? by->defId : std::string();
        }
      }
      if (h.killed && h.targetMaxHp > 0 && before >= 0.5 * h.targetMaxHp) {
        ++stats.oneHitDeaths;
        Note("killed by one hit of " + std::to_string(static_cast<int>(h.amount)) + " from " +
             (by != nullptr ? by->defId : std::string("?")));
      }
    }
    const EntityId other = onHero ? h.source : (byHero ? h.target : kNoEntity);
    if (other != kNoEntity && OpenFight(other) == nullptr) {
      const SeenMonster* m = SeenById(other);
      // A fight never reopens on its killed monster (its brand ticking on, an overkill hit in the kill's frame).
      bool fought = false;
      for (const AutoPilotFight& f : stats.fights) fought = fought || f.entity == other;
      if (m != nullptr && NamedRole(m->role) && !fought) {
        AutoPilotFight nf;
        nf.defId = m->defId;
        nf.role = m->role;
        nf.entity = other;
        nf.startMs = now;
        if (const MonsterView* mv = FindMonster(other)) {
          for (EliteAffixType a : mv->affixes) nf.affixes += (nf.affixes.empty() ? "" : "+") + std::string(EnumName(a));
        }
        for (size_t k = 0; k < earlyUses.size(); ++k) {  // 2F review: its uses before the first hit
          if (earlyUses[k].first != other) continue;
          nf.abilities = std::move(earlyUses[k].second);
          earlyUses.erase(earlyUses.begin() + static_cast<std::ptrdiff_t>(k));
          break;
        }
        stats.fights.push_back(nf);
      }
    }
    // A kill ends its fight, whoever dealt it (the hero, its beast, a burst).
    if (h.killed && h.targetFaction == Faction::Monster) {
      if (AutoPilotFight* f = OpenFight(h.target)) {
        f->endMs = now;
        char buf[96];
        std::snprintf(buf, sizeof(buf), " fight %.1f s", (f->endMs - f->startMs) / 1000.0);
        Note(f->defId + buf);
      }
    }
  }

  // ---- script ----
  void AddTask(std::string label, ApTaskFactory make) {
    Step s;
    s.kind = Step::Kind::Task;
    s.text = std::move(label);
    s.make = std::move(make);
    steps.push_back(std::move(s));
  }
  void AddStep(Step::Kind kind, std::string text) {
    Step s;
    s.kind = kind;
    s.text = std::move(text);
    steps.push_back(std::move(s));
  }
  void BuildScenario();
  AutoPilotStatus RunScript();
};

namespace autopilot_impl {

// While alive, the bot works a panel it opened itself (AutoUi leaves its modals alone).
class ApPanelSession {
 public:
  explicit ApPanelSession(AutoPilotImpl& b) : b_(b) { ++b_.panelSession; }
  ~ApPanelSession() { --b_.panelSession; }
  ApPanelSession(const ApPanelSession&) = delete;
  ApPanelSession& operator=(const ApPanelSession&) = delete;

 private:
  AutoPilotImpl& b_;
};

int64_t ApFramesFor(double seconds) { return static_cast<int64_t>(seconds * kApFps); }

// ---------------------------------------------------------------------------------------------------------------------
// Waiting
// ---------------------------------------------------------------------------------------------------------------------

// Frames until the commands submitted so far were applied: a frame may run no sim step at all (S6 slow motion after an
// elite kill dilates the sim clock, S1 accumulator), and commands apply at the start of the next step - or in the
// UI-only step of a frozen frame.
class ApCommit final : public ApTask {
 public:
  ApRun Run(AutoPilotImpl& b) override {
    if (started_) {
      if (b.lastSteps > 0 || b.lastFrozen) return Finish(true);
      ++i_;
    }
    started_ = true;
    if (i_ < 4 * kApFps && b.ok) return ApRun::Frame;
    return Finish(true);
  }

 private:
  int64_t i_ = 0;
  bool started_ = false;
};

class ApWait final : public ApTask {
 public:
  explicit ApWait(double seconds) : seconds_(seconds) {}
  ApRun Run(AutoPilotImpl& b) override {
    if (!started_) {
      end_ = b.frames + ApFramesFor(seconds_);
      started_ = true;
    }
    if (b.frames < end_ && b.ok) return ApRun::Frame;
    return Finish(true);
  }

 private:
  double seconds_;
  int64_t end_ = 0;
  bool started_ = false;
};

// Plays frames until no story beat is queued / playing and no core modal is up (closing stray ones).
class ApWaitIdle final : public ApTask {
 public:
  explicit ApWaitIdle(double timeoutSec = 120) : timeoutSec_(timeoutSec) {}
  ApRun Run(AutoPilotImpl& b) override {
    if (!started_) {
      end_ = b.frames + ApFramesFor(timeoutSec_);
      started_ = true;
    }
    if (b.ok && b.frames < end_) {
      const Snapshot& v = b.V();
      if (!v.storyBusy && !v.frozen && v.modals.empty()) return Finish(true);
      if (!v.storyBusy && !v.modals.empty() && (b.frames % 30) == 0) {
        for (PanelId p : v.modals) b.Submit(CmdClosePanel{p});
      }
      return ApRun::Frame;
    }
    b.Fail("WaitIdle timeout");
    return Finish(false);
  }

 private:
  double timeoutSec_;
  int64_t end_ = 0;
  bool started_ = false;
};

class ApWaitRespawn final : public ApTask {
 public:
  ApRun Run(AutoPilotImpl& b) override {
    if (!started_) {
      b.Note("hero down, waiting for the respawn");
      end_ = b.frames + 30 * kApFps;
      started_ = true;
    }
    if (b.ok && b.frames < end_ && !b.Alive()) return ApRun::Frame;
    if (!b.Alive()) b.Fail("hero never respawned");
    return Finish(true);
  }

 private:
  int64_t end_ = 0;
  bool started_ = false;
};

// ---------------------------------------------------------------------------------------------------------------------
// Fighting and walking
// ---------------------------------------------------------------------------------------------------------------------

// Attacks `target` until it dies (true) or the timeout / hero death (false).
class ApEngage final : public ApTask {
 public:
  ApEngage(EntityId target, double timeoutSec, int depth) : target_(target), timeoutSec_(timeoutSec), depth_(depth) {}
  ApRun Run(AutoPilotImpl& b) override;

 private:
  enum class St : uint8_t { Start, Loop, AfterRespawn, AfterNested };
  St st_ = St::Start;
  EntityId target_;
  double timeoutSec_;
  int depth_;
  int64_t end_ = 0, lastOrder_ = -1000, lastCast_ = -1000;
  // W2 (2F): the last priority check, a lure under way (its end) and a hold (the frame it began).
  int64_t lastPriorityCheck_ = -1000, lureEnd_ = -1, holdFrom_ = -1;
};

// Walks to `goal` (fighting whatever attacks on the way). True once within `arrive` tiles. A `required` walk that times
// out (fights count against `timeoutSec`) or is stuck fails the script; an optional one (a detour the caller can skip,
// e.g. a potion pickup) only notes it and returns false.
class ApGoTo final : public ApTask {
 public:
  ApGoTo(Vec2 goal, double arrive, double timeoutSec = 240, bool fight = true, bool required = true)
      : goal_(goal), arrive_(arrive), timeoutSec_(timeoutSec), fight_(fight), required_(required) {}
  ApRun Run(AutoPilotImpl& b) override;

 private:
  // A walk that did not arrive: fails the script when required, else a note.
  ApRun GiveUp(AutoPilotImpl& b, const char* what);

  enum class St : uint8_t { Start, Loop, AfterRespawn, AfterBusyFrame, AfterFight, AfterMoveFrame };
  St st_ = St::Start;
  Vec2 goal_;
  double arrive_, timeoutSec_;
  bool fight_, required_;
  int64_t end_ = 0, lastOrder_ = -1000, lastProgress_ = 0;
  Vec2 lastPos_;
};

ApRun ApEngage::Run(AutoPilotImpl& b) {
  switch (st_) {
    case St::Start:
      end_ = b.frames + ApFramesFor(timeoutSec_);
      lastOrder_ = -1000;
      lastCast_ = -1000;
      break;
    case St::Loop: break;
    case St::AfterRespawn: return Finish(false);
    case St::AfterNested: lastOrder_ = -1000; break;
  }
  while (b.ok && b.frames < end_) {
    const MonsterView* m = b.FindMonster(target_);
    if (m == nullptr || !m->alive) return Finish(true);
    if (!b.Alive()) {
      st_ = St::AfterRespawn;
      return b.Call<ApWaitRespawn>();
    }
    if (b.Busy()) {
      st_ = St::Loop;
      return ApRun::Frame;
    }
    st_ = St::Loop;
    // W2: out of a strike's shape, a hazard or an arena's burning ring first (a named fight's spot stays near its home:
    // its leash, below; the core's own, MonsterView::leashCentre / leashRange).
    const Vec2 leashCentre = m->leashCentre;
    const Vec2* fightHome = AutoPilotImpl::NamedRole(m->role) && m->leashRange > 0 ? &leashCentre : nullptr;
    const double homeR = m->leashRange - 4.0;
    if (b.Evade(m->pos, fightHome, homeR)) return ApRun::Frame;
    // W2: a priority target (a raiser, a drummer, a banner, a pack's leader) in the fight takes over from a lesser one.
    if (depth_ == 0 && b.frames - lastPriorityCheck_ >= 20) {
      lastPriorityCheck_ = b.frames;
      const EntityId better = b.BestThreat(kApThreatRadius);
      const MonsterView* bm = better != kNoEntity && better != target_ ? b.FindMonster(better) : nullptr;
      if (bm != nullptr && b.ThreatPriority(*bm) > b.ThreatPriority(*m)) {
        b.Note("priority target " + bm->defId);
        ++b.stats.priorityTargets;
        target_ = better;
        lastOrder_ = -1000;
        lureEnd_ = -1;
        return ApRun::Frame;
      }
    }
    // W2: a Raised inside a feeding boss's zone is lured out before it dies (its body would heal the boss).
    if (const std::optional<Vec2> lure = b.LureSpot(*m)) {
      if (lureEnd_ < 0) lureEnd_ = b.frames + 5 * kApFps;
      if (b.frames < lureEnd_) {
        if (Dist(b.HeroPos(), *lure) > 0.75 && (!b.V().hero.moving || b.frames - lastOrder_ > 30)) {
          b.Submit(CmdMoveTo{*lure});
          lastOrder_ = b.frames;
        }
        return ApRun::Frame;
      }
    } else {
      lureEnd_ = -1;
    }
    // A monster walking home (M1) ignores the hero and heals: follow it in instead of shooting it from outside its
    // leash, and take the fight up again where it stands (a boss's long fight, 2F: a ranged hero shooting from past the
    // leash kept a boss walking home and healing).
    if (m->state == MonsterState::Returning) {
      if (Dist(b.HeroPos(), m->pos) > 2.5 && (!b.V().hero.moving || b.frames - lastOrder_ > 30)) {
        b.Submit(CmdMoveTo{m->pos});
        lastOrder_ = b.frames;
      }
      return ApRun::Frame;
    }
    // A named monster's fight stays inside its leash: a hero (a ranged one shooting from afar) farther than the leash
    // range less 3.5 tiles from its home walks back in first, so its chase does not take it past the leash, home, healed
    // to full and back to phase 0 (2F: a long boss fight; a Breachwalker blink past the leash is the core's to skip).
    if (depth_ == 0 && fightHome != nullptr) {
      const Vec2* home = fightHome;
      if (Dist(b.HeroPos(), *home) > homeR + 0.5) {
        const Vec2 in = *home + (b.HeroPos() - *home).Normalized() * (homeR - 1.0);
        if (b.WalkableAt(in) && (!b.V().hero.moving || b.frames - lastOrder_ > 30)) {
          b.Submit(CmdMoveTo{in});
          lastOrder_ = b.frames;
          return ApRun::Frame;
        }
        if (b.V().hero.moving) return ApRun::Frame;
      }
    }
    // W2: a strike is about to land between the hero and its target: wait it out (3 s at most).
    if (b.ApproachBlocked(*m)) {
      if (holdFrom_ < 0) holdFrom_ = b.frames;
      if (b.frames - holdFrom_ < 3 * kApFps) {
        if (b.V().hero.target != kNoEntity || b.V().hero.moving) b.Submit(CmdClearTarget{});
        if (b.V().hero.moving) b.Submit(CmdStop{});
        lastOrder_ = -1000;
        return ApRun::Frame;
      }
    } else {
      holdFrom_ = -1;
    }
    // Something else is hitting us while the target is still far: deal with it first.
    if (depth_ == 0 && Dist(m->pos, b.HeroPos()) > 4.0) {
      const EntityId close = b.NearestThreat(2.5);
      if (close != kNoEntity && close != target_) {
        st_ = St::AfterNested;
        return b.Call<ApEngage>(close, 30.0, 1);
      }
    }
    // A lock whose approach ended short is clicked again, so the hero walks in rather than stand idle with the target out
    // of its reach (2F: a long boss fight). Since the 2F review the core walks a held lock that stepped back by itself
    // (a tether, a kite, a blink: CombatSystem::TickCombat's re-approach) for a player too; this click still covers the
    // bot's own orders (a dodge, a hazard step-out), which end that rule as a player's move would.
    const bool outOfReach = Dist(m->pos, b.HeroPos()) > b.V().hero.derived.attackRange + 0.6 && !b.V().hero.moving;
    if ((b.V().hero.target != target_ || outOfReach) && b.frames - lastOrder_ > 15) {
      b.Submit(CmdAttackTarget{target_});
      lastOrder_ = b.frames;
    }
    if (b.frames - lastCast_ >= 8 && b.TryCast(*m)) lastCast_ = b.frames;
    st_ = St::Loop;
    return ApRun::Frame;
  }
  if (b.ok) b.Note("gave up on target " + std::to_string(target_));
  b.Submit(CmdClearTarget{});
  return Finish(false);
}

ApRun ApGoTo::Run(AutoPilotImpl& b) {
  switch (st_) {
    case St::Start:
      end_ = b.frames + ApFramesFor(timeoutSec_);
      lastOrder_ = -1000;
      lastPos_ = b.HeroPos();
      lastProgress_ = b.frames;
      break;
    case St::Loop: break;
    case St::AfterRespawn: lastOrder_ = -1000; break;
    case St::AfterBusyFrame: lastProgress_ = b.frames; break;
    case St::AfterFight:
      lastOrder_ = -1000;
      lastProgress_ = b.frames;
      break;
    case St::AfterMoveFrame:
      if (Dist(b.HeroPos(), lastPos_) > 1.0) {
        lastPos_ = b.HeroPos();
        lastProgress_ = b.frames;
      } else if (b.frames - lastProgress_ > 30 * kApFps) {
        return GiveUp(b, "stuck");
      }
      break;
  }
  if (b.ok && b.frames < end_) {
    if (Dist(b.HeroPos(), goal_) <= arrive_) return Finish(true);
    if (!b.Alive()) {
      st_ = St::AfterRespawn;
      return b.Call<ApWaitRespawn>();
    }
    if (b.Busy()) {
      st_ = St::AfterBusyFrame;
      return ApRun::Frame;
    }
    // W2: out of a strike's shape or a hazard on the way (the walk resumes after it).
    if (fight_ && b.Evade(goal_)) {
      lastOrder_ = -1000;
      lastProgress_ = b.frames;
      st_ = St::AfterBusyFrame;
      return ApRun::Frame;
    }
    if (fight_) {
      const EntityId t = b.BestThreat(kApThreatRadius);
      if (t != kNoEntity) {
        st_ = St::AfterFight;
        return b.Call<ApEngage>(t, 45.0, 0);
      }
    }
    if (!b.V().hero.moving && b.frames - lastOrder_ > 20) {
      b.Submit(CmdMoveTo{goal_});
      lastOrder_ = b.frames;
    }
    st_ = St::AfterMoveFrame;
    return ApRun::Frame;
  }
  if (b.ok) return GiveUp(b, "timeout");
  return Finish(false);
}

ApRun ApGoTo::GiveUp(AutoPilotImpl& b, const char* what) {
  char buf[128];
  std::snprintf(buf, sizeof(buf), "%s walking to (%.1f,%.1f)", what, goal_.x, goal_.y);
  if (required_) {
    b.Fail(buf);
  } else {
    b.Note(std::string("gave up: ") + buf);
  }
  return Finish(false);
}

// ---------------------------------------------------------------------------------------------------------------------
// Loot and gear
// ---------------------------------------------------------------------------------------------------------------------

class ApEquipUpgrades final : public ApTask {
 public:
  ApRun Run(AutoPilotImpl& b) override {
    if (st_ == St::Start) {
      if (b.V().inventory == nullptr) return Finish(true);
      guard_ = 0;
    } else {  // AfterCommit
      bool stillInBag = false;
      if (b.V().inventory != nullptr) {
        for (const ItemInstance& it : b.V().inventory->Bag()) stillInBag = stillInBag || it.uid == uid_;
      }
      b.Note("equip " + uid_ + (stillInBag ? " (still in the bag)" : ""));
      ++guard_;
    }
    if (guard_ >= 12 || b.V().inventory == nullptr) return Finish(true);
    std::string uid;
    double best = 0.5;
    for (const ItemInstance& it : b.V().inventory->Bag()) {
      const double g = b.UpgradeGain(it);
      if (g > best) {
        best = g;
        uid = it.uid;
      }
    }
    if (uid.empty()) return Finish(true);
    b.Submit(CmdEquip{uid});
    uid_ = uid;
    st_ = St::AfterCommit;
    return b.Call<ApCommit>();
  }

 private:
  enum class St : uint8_t { Start, AfterCommit };
  St st_ = St::Start;
  int guard_ = 0;
  std::string uid_;
};

// P panel: feed ley fruit to the active beast (quests 18.8) while it can still grow.
class ApFeedPet final : public ApTask {
 public:
  ApRun Run(AutoPilotImpl& b) override {
    if (!started_) {
      started_ = true;
      const Snapshot& v = b.V();
      if (b.feedRefused || v.pets == nullptr || v.pets->Active() == nullptr || b.CountOf("c_ley_fruit") <= 0) {
        return Finish(true);
      }
      id_ = std::string(v.pets->ActiveId());
      before_ = v.pets->Active()->exp + 1000 * v.pets->Active()->level;
      b.Submit(CmdFeedPet{id_});
      return b.Call<ApCommit>();
    }
    const PetInstance* p = b.V().pets != nullptr ? b.V().pets->Active() : nullptr;
    if (p != nullptr && p->exp + 1000 * p->level != before_) {
      ++b.stats.petFeeds;
      b.Note("fed " + id_);
    } else {
      b.feedRefused = true;
      b.Note("feeding refused");
    }
    return Finish(true);
  }

 private:
  bool started_ = false;
  std::string id_;
  int64_t before_ = 0;
};

class ApLootNearby final : public ApTask {
 public:
  explicit ApLootNearby(double radius) : radius_(radius) {}
  ApRun Run(AutoPilotImpl& b) override {
    switch (st_) {
      case St::Start: guard_ = 0; break;
      case St::AfterPotion:
        // potions are auto-collected within 2 tiles; mark it so a stuck one cannot loop - also when the walk gave up
        // (an optional detour: its fights ate the 15 s, or the potion is out of reach)
        b.skippedLoot.insert(pick_);
        if (!b.childResult) return Finish(true);
        ++guard_;
        break;
      case St::PickWait: {
        bool present = false;
        for (const GroundItemView& g : b.V().groundItems) present = present || g.id == pick_;
        if (!present) {
          gone_ = true;
        } else if (b.NearestThreat(3) == kNoEntity && b.ok && b.frames < pickEnd_) {
          return ApRun::Frame;
        }
        if (!gone_) b.skippedLoot.insert(pick_);
        ++guard_;
        break;
      }
    }
    while (guard_ < 24 && b.ok) {
      if (b.Busy() || !b.Alive()) return Finish(true);
      if (b.NearestThreat(kApThreatRadius) != kNoEntity) return Finish(true);
      if (b.V().inventory != nullptr && b.V().inventory->IsFull()) return Finish(true);
      EntityId pick = kNoEntity;
      Vec2 pos;
      double bestD = radius_;
      for (const GroundItemView& g : b.V().groundItems) {
        if (g.item == nullptr || b.skippedLoot.count(g.id) != 0 || !b.Wanted(*g.item)) continue;
        const double d = Dist(g.pos, b.HeroPos());
        if (d <= bestD) {
          bestD = d;
          pick = g.id;
          pos = g.pos;
        }
      }
      bool potion = false;
      for (const PotionDropView& p : b.V().potions) {
        const double d = Dist(p.pos, b.HeroPos());
        if (b.skippedLoot.count(p.id) == 0 && d <= bestD) {
          bestD = d;
          pick = p.id;
          pos = p.pos;
          potion = true;
        }
      }
      if (pick == kNoEntity) return Finish(true);
      pick_ = pick;
      if (potion) {
        st_ = St::AfterPotion;
        return b.Call<ApGoTo>(pos, 1.0, 15.0, true, false);  // optional: a timeout skips the potion
      }
      b.Submit(CmdPickUp{pick});
      pickEnd_ = b.frames + 8 * kApFps;
      gone_ = false;
      st_ = St::PickWait;
      return ApRun::Frame;
    }
    return Finish(true);
  }

 private:
  enum class St : uint8_t { Start, AfterPotion, PickWait };
  St st_ = St::Start;
  double radius_;
  int guard_ = 0;
  EntityId pick_ = kNoEntity;
  int64_t pickEnd_ = 0;
  bool gone_ = false;
};

// Kills the monsters `match` accepts (by def id or by role; nearest first, around `centre`) until `done`. `what` names
// them in failures.
class ApHunt final : public ApTask {
 public:
  ApHunt(std::string what, ApMonsterFilter match, Vec2 centre, ApPredicate done, double timeoutSec)
      : what_(std::move(what)), match_(std::move(match)), centre_(centre), done_(std::move(done)),
        timeoutSec_(timeoutSec) {}
  ApRun Run(AutoPilotImpl& b) override {
    switch (st_) {
      case St::Start:
        end_ = b.frames + ApFramesFor(timeoutSec_);
        idleSince_ = -1;
        break;
      case St::Loop: break;
      case St::AfterLoot:
        if (!b.ok) return Finish(false);
        st_ = St::AfterEquip;
        return b.Call<ApEquipUpgrades>();
      case St::AfterEquip:
        st_ = St::AfterFeed;
        return b.Call<ApFeedPet>();
      case St::AfterFeed: {
        const EntityId t = b.NearestMatching(match_, b.HeroPos(), 60);
        if (t != kNoEntity) {
          idleSince_ = -1;
          st_ = St::Loop;
          return b.Call<ApEngage>(t, 60.0, 0);
        }
        if (Dist(b.HeroPos(), centre_) > 4) {
          st_ = St::Loop;
          return b.Call<ApGoTo>(centre_, 3.0, 180.0, true);
        }
        if (idleSince_ < 0) idleSince_ = b.frames;
        if (b.frames - idleSince_ > 120 * kApFps) {
          b.Fail("no " + what_ + " to hunt (nothing respawned)");
          return Finish(false);
        }
        st_ = St::Loop;
        return ApRun::Frame;
      }
    }
    while (b.ok && !done_(b)) {
      if (b.frames >= end_) {
        b.Fail("hunt timeout for " + what_);
        return Finish(false);
      }
      if (!b.Alive()) {
        st_ = St::Loop;
        return b.Call<ApWaitRespawn>();
      }
      if (b.Busy()) {
        st_ = St::Loop;
        return ApRun::Frame;
      }
      const EntityId threat = b.BestThreat(kApThreatRadius);
      if (threat != kNoEntity) {
        st_ = St::Loop;
        return b.Call<ApEngage>(threat, 45.0, 0);
      }
      st_ = St::AfterLoot;
      return b.Call<ApLootNearby>(7.0);
    }
    return Finish(b.ok);
  }

 private:
  enum class St : uint8_t { Start, Loop, AfterLoot, AfterEquip, AfterFeed };
  St st_ = St::Start;
  std::string what_;
  ApMonsterFilter match_;
  Vec2 centre_;
  ApPredicate done_;
  double timeoutSec_;
  int64_t end_ = 0, idleSince_ = -1;
};

// ---------------------------------------------------------------------------------------------------------------------
// NPCs, shops, quest cards
// ---------------------------------------------------------------------------------------------------------------------

class ApClosePanels final : public ApTask {
 public:
  ApRun Run(AutoPilotImpl& b) override {
    if (started_) {
      ++i_;
    }
    started_ = true;
    if (i_ < 120 && b.ok) {
      if (b.V().modals.empty()) return Finish(true);
      if ((i_ % 10) == 0) {
        for (PanelId p : b.V().modals) b.Submit(CmdClosePanel{p});
      }
      return ApRun::Frame;
    }
    if (!b.V().modals.empty()) b.Fail("panel did not close");
    return Finish(b.V().modals.empty());
  }

 private:
  int i_ = 0;
  bool started_ = false;
};

// Walks to the NPC and talks (CmdInteract); true once one of its core modals is up.
class ApTalkTo final : public ApTask {
 public:
  explicit ApTalkTo(std::string npcId) : npcId_(std::move(npcId)) {}
  ApRun Run(AutoPilotImpl& b) override {
    switch (st_) {
      case St::Start:
        st_ = St::AfterFirstIdle;
        return b.Call<ApWaitIdle>();
      case St::AfterFirstIdle: {
        if (!b.childResult) return Finish(false);
        const NpcView* n = b.FindNpc(npcId_);
        if (n == nullptr) {
          b.Fail("npc not in zone: " + npcId_);
          return Finish(false);
        }
        id_ = n->id;
        st_ = St::AfterGoTo;
        return b.Call<ApGoTo>(n->pos, 2.0, 300.0, true);
      }
      case St::AfterGoTo:
        if (!b.childResult) return Finish(false);
        session_.emplace(b);
        attempt_ = 0;
        break;
      case St::AfterIdle:
        if (!b.childResult) return Finish(false);
        b.Submit(CmdInteract{id_});
        i_ = 0;
        if (i_ < 4 * kApFps && b.ok) {
          st_ = St::InteractFrame;
          return ApRun::Frame;
        }
        ++attempt_;
        break;
      case St::InteractFrame:
        if (!b.V().modals.empty()) return Finish(true);
        ++i_;
        if (i_ < 4 * kApFps && b.ok) return ApRun::Frame;
        ++attempt_;
        break;
    }
    if (attempt_ < 3 && b.ok) {
      st_ = St::AfterIdle;
      return b.Call<ApWaitIdle>();
    }
    b.Fail("talking to " + npcId_ + " opened nothing");
    return Finish(false);
  }

 private:
  enum class St : uint8_t { Start, AfterFirstIdle, AfterGoTo, AfterIdle, InteractFrame };
  St st_ = St::Start;
  std::string npcId_;
  EntityId id_ = kNoEntity;
  int attempt_ = 0;
  int64_t i_ = 0;
  std::optional<ApPanelSession> session_;
};

// Sells spare gear and tops up potions at a shop NPC.
class ApShop final : public ApTask {
 public:
  ApShop(std::string npcId, int hpTarget, int mpTarget)
      : npcId_(std::move(npcId)), hpTarget_(hpTarget), mpTarget_(mpTarget) {}
  ApRun Run(AutoPilotImpl& b) override {
    switch (st_) {
      case St::Start:
        st_ = St::AfterTalk;
        return b.Call<ApTalkTo>(npcId_);
      case St::AfterTalk:
        if (!b.childResult) return Finish(false);
        session_.emplace(b);
        if (b.V().shop == nullptr || !b.V().shop->open) {
          b.Fail(npcId_ + ": shop did not open");
          return Finish(false);
        }
        // sell bag equipment that is not an upgrade
        for (const ItemInstance& it : b.V().inventory->Bag()) {
          const ItemBaseDef* base = b.data.FindItemBase(it.baseId);
          if (base != nullptr && base->hasSlot && b.UpgradeGain(it) <= 0.5) sell_.push_back(it.uid);
        }
        sellIdx_ = 0;
        break;
      case St::AfterSell:
        if (b.V().hero.gold > gold_) ++b.stats.itemsSold;
        ++sellIdx_;
        break;
      case St::AfterBuy:
        if (b.CountOf(buyBase_) <= before_) {
          return NextBuy(b);  // break
        }
        ++b.stats.itemsBought;
        ++buyGuard_;
        return BuyLoop(b);
      case St::AfterClose: return Finish(b.childResult);
    }
    if (sellIdx_ < sell_.size()) {
      gold_ = b.V().hero.gold;
      b.Submit(CmdSell{sell_[sellIdx_]});
      st_ = St::AfterSell;
      return b.Call<ApCommit>();
    }
    phase_ = 0;
    StartBuy(b, b.V().hero.level >= 10 ? "c_hp_potion_m" : "c_hp_potion_s", hpTarget_);
    return BuyLoop(b);
  }

 private:
  enum class St : uint8_t { Start, AfterTalk, AfterSell, AfterBuy, AfterClose };
  void StartBuy(AutoPilotImpl& b, const std::string& baseId, int target) {
    buyBase_ = baseId;
    buyTarget_ = target;
    ware_ = b.WareIndex(baseId);
    buyGuard_ = 0;
  }
  ApRun BuyLoop(AutoPilotImpl& b) {
    if (ware_ >= 0 && buyGuard_ < 30 && b.CountOf(buyBase_) < buyTarget_ && b.V().hero.gold >= b.WarePrice(ware_)) {
      before_ = b.CountOf(buyBase_);
      b.Submit(CmdBuy{ware_});
      st_ = St::AfterBuy;
      return b.Call<ApCommit>();
    }
    return NextBuy(b);
  }
  ApRun NextBuy(AutoPilotImpl& b) {
    if (phase_ == 0 && mpTarget_ > 0) {
      phase_ = 1;
      StartBuy(b, "c_mp_potion_s", mpTarget_);
      return BuyLoop(b);
    }
    b.Note("shopped at " + npcId_ + ": hp potions " + std::to_string(b.V().potionSlots[0].count) + ", gold " +
           std::to_string(b.V().hero.gold));
    st_ = St::AfterClose;
    return b.Call<ApClosePanels>();
  }

  St st_ = St::Start;
  std::string npcId_;
  int hpTarget_, mpTarget_;
  std::optional<ApPanelSession> session_;
  std::vector<std::string> sell_;
  size_t sellIdx_ = 0;
  int64_t gold_ = 0;
  int phase_ = 0;
  std::string buyBase_;
  int buyTarget_ = 0;
  int32_t ware_ = -1;
  int buyGuard_ = 0;
  int32_t before_ = 0;
};

// Talks to a quest giver and works its card: turns in everything completed (best reward choice), then accepts the
// offers in `accept` (only those: an empty list turns in and accepts nothing). Re-opens the card after each action like
// the T17 chain offer (900 ms, or after the turn-in cutscene + 300 ms).
class ApVisitGiver final : public ApTask {
 public:
  ApVisitGiver(std::string npcId, std::vector<std::string> accept)
      : npcId_(std::move(npcId)), accept_(std::move(accept)) {}
  ApRun Run(AutoPilotImpl& b) override {
    switch (st_) {
      case St::Start:
        st_ = St::AfterTalk;
        return b.Call<ApTalkTo>(npcId_);
      case St::AfterTalk:
        if (!b.childResult) return Finish(false);
        session_.emplace(b);
        round_ = 0;
        return Round(b);
      case St::AfterTurnIn:
        if (!b.HasStatus(questId_, QuestStatus::TurnedIn)) {
          b.Fail("turn-in refused: " + questId_);
          return Finish(false);
        }
        return Acted(b);
      case St::AfterAccept:
        if (!b.HasStatus(questId_, QuestStatus::Active) && !b.QuestDone(questId_)) {
          b.Fail("accept refused: " + questId_);
          return Finish(false);
        }
        return Acted(b);
      case St::AfterRead:
        if (b.V().storyBusy) {
          st_ = St::AfterStoryIdle;
          return b.Call<ApWaitIdle>();
        }
        st_ = St::AfterIdle;
        return b.Call<ApWaitIdle>();
      case St::AfterStoryIdle:
        st_ = St::AfterStoryPause;
        return b.Call<ApWait>(0.3);
      case St::AfterStoryPause:
        st_ = St::AfterIdle;
        return b.Call<ApWaitIdle>();
      case St::AfterIdle: {
        if (!b.childResult) return Finish(false);
        const NpcView* n = b.FindNpc(npcId_);
        if (n == nullptr || Dist(b.HeroPos(), n->pos) > 3.0) {
          st_ = St::AfterReTalk;
          return b.Call<ApTalkTo>(npcId_);
        }
        b.Submit(CmdQuestCardOpen{npcId_});
        st_ = St::AfterCardOpen;
        return b.Call<ApCommit>();
      }
      case St::AfterReTalk:
        if (!b.childResult) return Finish(false);
        st_ = St::AfterNested;
        return b.Call<ApVisitGiver>(npcId_, accept_);
      case St::AfterNested: return Finish(b.childResult);
      case St::AfterCardOpen:
        if (b.V().questCard == nullptr || !b.V().questCard->open) return Done(b);  // nothing more to offer
        ++round_;
        return Round(b);
      case St::AfterClose:
        if (!b.childResult) return Finish(false);
        return Done(b);
    }
    return Finish(false);
  }

 private:
  enum class St : uint8_t {
    Start, AfterTalk, AfterTurnIn, AfterAccept, AfterRead, AfterStoryIdle, AfterStoryPause, AfterIdle, AfterReTalk,
    AfterNested, AfterCardOpen, AfterClose,
  };
  ApRun Close(AutoPilotImpl& b) {
    st_ = St::AfterClose;
    return b.Call<ApClosePanels>();
  }
  // Every quest the step was told to accept is now open or done: an id the card never offered (a route mistake, a
  // missing prerequisite) fails here instead of as an unrelated failure later.
  ApRun Done(AutoPilotImpl& b) {
    for (const std::string& id : accept_) {
      if (b.HasStatus(id, QuestStatus::Active) || b.QuestDone(id)) continue;
      b.Fail("visit " + npcId_ + ": " + id + " was not on the quest card");
      return Finish(false);
    }
    return Finish(true);
  }
  ApRun Round(AutoPilotImpl& b) {
    if (!(round_ < 16 && b.ok)) return Close(b);
    const QuestCardState* card = b.V().questCard;
    if (card == nullptr || !card->open) return Close(b);  // nothing to offer: a tree / linear dialogue opened instead
    for (const QuestCardEntry& e : card->entries) {
      if (e.quest == nullptr || !e.turnIn) continue;
      questId_ = e.quest->id;
      const int32_t choice = b.BestChoice(e.choices);
      b.Note("turn in " + questId_ + (e.choices.empty() ? "" : " choice " + std::to_string(choice)));
      b.Submit(CmdQuestTurnIn{questId_, choice});
      st_ = St::AfterTurnIn;
      return b.Call<ApCommit>();
    }
    for (const QuestCardEntry& e : card->entries) {
      if (e.quest == nullptr || e.turnIn) continue;
      const std::string& id = e.quest->id;
      if (std::find(accept_.begin(), accept_.end(), id) == accept_.end()) continue;
      questId_ = id;
      b.Note("accept " + questId_);
      b.Submit(CmdQuestAccept{questId_});
      st_ = St::AfterAccept;
      return b.Call<ApCommit>();
    }
    return Close(b);
  }
  // T17: reopen the card after 900 ms, or after the turn-in cutscene (+300 ms).
  ApRun Acted(AutoPilotImpl& b) {
    st_ = St::AfterRead;
    return b.Call<ApWait>(0.9);
  }

  St st_ = St::Start;
  std::string npcId_;
  std::vector<std::string> accept_;
  std::optional<ApPanelSession> session_;
  int round_ = 0;
  std::string questId_;
};

// An NPC's dialogue tree (quests 10.3) from the quest card's "view story" button: from the root, the button whose
// choice leads to each node of `path` in turn (a reward choice pays once, Q1; a questTrigger choice accepts its quest),
// then Leave at the last node.
class ApDialogueTree final : public ApTask {
 public:
  ApDialogueTree(std::string npcId, std::vector<std::string> path) : npcId_(std::move(npcId)), path_(std::move(path)) {}
  ApRun Run(AutoPilotImpl& b) override {
    switch (st_) {
      case St::Start:
        st_ = St::AfterTalk;
        return b.Call<ApTalkTo>(npcId_);
      case St::AfterTalk:
        if (!b.childResult) return Finish(false);
        session_.emplace(b);
        b.Submit(CmdDialogueOpenTree{npcId_});
        st_ = St::AfterOpen;
        return b.Call<ApCommit>();
      case St::AfterOpen: step_ = 0; break;
      case St::AfterRead:
        b.Submit(CmdDialogueChoose{button_});
        st_ = St::AfterChoose;
        return b.Call<ApCommit>();
      case St::AfterChoose: ++step_; break;
      case St::AfterLeave:
        st_ = St::AfterClose;
        return b.Call<ApClosePanels>();
      case St::AfterClose: return Finish(b.childResult);
    }
    if (step_ < path_.size()) {
      const std::string& next = path_[step_];
      const DialogueView* d = b.V().dialogue;
      if (d == nullptr || !d->open || d->tree == nullptr) {
        b.Fail(npcId_ + " dialogue tree not open");
        return Finish(false);
      }
      const DialogueNode* node = d->tree->FindNode(d->nodeId);
      int32_t button = -1;
      for (size_t i = 0; i < d->buttons.size() && node != nullptr; ++i) {
        const DialogueButton& btn = d->buttons[i];
        if (btn.kind == DialogueButtonKind::Choice && btn.choiceIndex >= 0 &&
            static_cast<size_t>(btn.choiceIndex) < node->choices.size() &&
            node->choices[static_cast<size_t>(btn.choiceIndex)].nextNodeId == next) {
          button = static_cast<int32_t>(i);
        }
      }
      if (button < 0) {
        b.Fail(npcId_ + " tree: no button to " + next + " at " + d->nodeId);
        return Finish(false);
      }
      button_ = button;
      st_ = St::AfterRead;
      return b.Call<ApWait>(0.5);  // reading
    }
    const DialogueView* d = b.V().dialogue;
    if (d == nullptr || !d->open || path_.empty() || d->nodeId != path_.back()) {
      b.Fail(npcId_ + " tree did not reach " + (path_.empty() ? std::string("its path") : path_.back()));
      return Finish(false);
    }
    int32_t leave = -1;
    for (size_t i = 0; i < d->buttons.size(); ++i) {
      if (d->buttons[i].kind == DialogueButtonKind::Leave) leave = static_cast<int32_t>(i);
    }
    if (leave < 0) {
      b.Fail(npcId_ + " tree: no Leave button at " + d->nodeId);
      return Finish(false);
    }
    b.Submit(CmdDialogueChoose{leave});
    st_ = St::AfterLeave;
    return b.Call<ApCommit>();
  }

 private:
  enum class St : uint8_t { Start, AfterTalk, AfterOpen, AfterRead, AfterChoose, AfterLeave, AfterClose };
  St st_ = St::Start;
  std::string npcId_;
  std::vector<std::string> path_;
  std::optional<ApPanelSession> session_;
  size_t step_ = 0;
  int32_t button_ = -1;
};

// ---------------------------------------------------------------------------------------------------------------------
// Quest helpers
// ---------------------------------------------------------------------------------------------------------------------

// Walks onto each gather node of `questId` until the objective is full.
class ApGather final : public ApTask {
 public:
  ApGather(std::string questId, size_t obj, int32_t required)
      : questId_(std::move(questId)), obj_(obj), required_(required) {}
  ApRun Run(AutoPilotImpl& b) override {
    switch (st_) {
      case St::Start: guard_ = 0; break;
      case St::AfterGoTo:
        st_ = St::AfterWait;
        return b.Call<ApWait>(0.2);
      case St::AfterWait: ++guard_; break;
    }
    if (guard_ < 40 && b.ok && b.ObjCurrent(questId_, obj_) < required_) {
      Vec2 best;
      double bestD = 1e9;
      for (const WorldMarkerView& m : b.V().markers) {
        if (m.kind != MarkerKind::GatherNode) continue;
        const double d = Dist(m.pos, b.HeroPos());
        if (d < bestD) {
          bestD = d;
          best = m.pos;
        }
      }
      if (bestD > 1e8) {
        b.Fail("no gather node left for " + questId_);
        return Finish(false);
      }
      st_ = St::AfterGoTo;
      return b.Call<ApGoTo>(best, 0.6, 120.0, true);
    }
    return Finish(b.ok && b.ObjCurrent(questId_, obj_) >= required_);
  }

 private:
  enum class St : uint8_t { Start, AfterGoTo, AfterWait };
  St st_ = St::Start;
  std::string questId_;
  size_t obj_;
  int32_t required_;
  int guard_ = 0;
};

// Examines the clue marks of `questId` in objective order.
class ApClues final : public ApTask {
 public:
  ApClues(std::string questId, std::vector<Vec2> tiles) : questId_(std::move(questId)), tiles_(std::move(tiles)) {}
  ApRun Run(AutoPilotImpl& b) override {
    const QuestDef* q = b.data.FindQuest(questId_);
    if (q == nullptr) return Finish(false);
    switch (st_) {
      case St::Start:
        i_ = 0;
        clue_ = 0;
        break;
      case St::AfterGoTo:
        st_ = St::AfterWait;
        return b.Call<ApWait>(0.3);
      case St::AfterWait:
        if (b.ObjCurrent(questId_, i_) < q->objectives[i_].required) {
          b.Fail("clue not taken: " + q->objectives[i_].targetId);
          return Finish(false);
        }
        b.Note("clue " + q->objectives[i_].targetId);
        ++i_;
        break;
    }
    for (; i_ < q->objectives.size() && b.ok; ++i_) {
      if (q->objectives[i_].type != ObjectiveType::InvestigateClue) continue;
      if (clue_ >= tiles_.size()) break;
      const Vec2 tile = tiles_[clue_++];
      if (b.ObjCurrent(questId_, i_) >= q->objectives[i_].required) continue;
      Vec2 mark = tile;
      for (const WorldMarkerView& m : b.V().markers) {
        if (m.kind == MarkerKind::Clue && m.key == questId_ && Dist(m.pos, tile) < 3) mark = m.pos;
      }
      st_ = St::AfterGoTo;
      return b.Call<ApGoTo>(mark, 1.0, 300.0, true);
    }
    return Finish(b.ok);
  }

 private:
  enum class St : uint8_t { Start, AfterGoTo, AfterWait };
  St st_ = St::Start;
  std::string questId_;
  std::vector<Vec2> tiles_;
  size_t i_ = 0, clue_ = 0;
};

// Waits within an explore circle until the objective counts (500 ms observer).
class ApExplore final : public ApTask {
 public:
  ApExplore(std::string questId, size_t obj, Vec2 centre) : questId_(std::move(questId)), obj_(obj), centre_(centre) {}
  ApRun Run(AutoPilotImpl& b) override {
    switch (st_) {
      case St::Start:
        st_ = St::AfterGoTo;
        return b.Call<ApGoTo>(centre_, 2.5, 300.0, true);
      case St::AfterGoTo:
        if (!b.childResult) return Finish(false);
        i_ = 0;
        break;
      case St::Waiting: ++i_; break;
    }
    if (i_ < 3 * kApFps && b.ok && b.ObjCurrent(questId_, obj_) < 1) {
      st_ = St::Waiting;
      return ApRun::Frame;
    }
    if (b.ObjCurrent(questId_, obj_) < 1) {
      b.Fail("explore objective did not count: " + questId_);
      return Finish(false);
    }
    b.Note("explored " + questId_ + "#" + std::to_string(obj_));
    return Finish(true);
  }

 private:
  enum class St : uint8_t { Start, AfterGoTo, Waiting };
  St st_ = St::Start;
  std::string questId_;
  size_t obj_;
  Vec2 centre_;
  int64_t i_ = 0;
};

// Shops at `npcId` (the class's potion targets), then equips upgrades.
class ApTownTrip final : public ApTask {
 public:
  explicit ApTownTrip(std::string npcId) : npcId_(std::move(npcId)) {}
  ApRun Run(AutoPilotImpl& b) override {
    switch (st_) {
      case St::Start:
        st_ = St::AfterShop;
        return b.Call<ApShop>(npcId_, b.HpPotionTarget(), b.MpPotionTarget());
      case St::AfterShop:
        if (!b.childResult) return Finish(false);
        st_ = St::AfterEquip;
        return b.Call<ApEquipUpgrades>();
      case St::AfterEquip: break;
    }
    return Finish(b.ok);
  }

 private:
  enum class St : uint8_t { Start, AfterShop, AfterEquip };
  St st_ = St::Start;
  std::string npcId_;
};

// Town portal home (W3): 1.5 s channel to the nearest camp.
class ApPortalHome final : public ApTask {
 public:
  ApRun Run(AutoPilotImpl& b) override {
    switch (st_) {
      case St::Start:
        st_ = St::AfterIdle;
        return b.Call<ApWaitIdle>();
      case St::AfterIdle:
        if (!b.childResult) return Finish(false);
        attempt_ = 0;
        return Attempt(b);
      case St::AfterFight:
        if (!b.ok) return Attempt(b);
        return Fight(b);
      case St::AfterStop:
        before_ = b.HeroPos();
        b.Submit(CmdTownPortal{});
        i_ = 0;
        return Channel(b);
      case St::Channelling:
        if (Dist(b.HeroPos(), before_) > 10) {
          b.Note("portal home");
          return Finish(true);
        }
        ++i_;
        return Channel(b);
    }
    return Finish(false);
  }

 private:
  enum class St : uint8_t { Start, AfterIdle, AfterFight, AfterStop, Channelling };
  ApRun Attempt(AutoPilotImpl& b) {
    if (attempt_ < 4 && b.ok) return Fight(b);
    b.Note("portal failed, walking");
    return Finish(false);
  }
  ApRun Fight(AutoPilotImpl& b) {
    const EntityId t = b.BestThreat(kApThreatRadius);
    if (t != kNoEntity) {
      st_ = St::AfterFight;
      return b.Call<ApEngage>(t, 45.0, 0);
    }
    b.Submit(CmdStop{});
    st_ = St::AfterStop;
    return b.Call<ApCommit>();
  }
  ApRun Channel(AutoPilotImpl& b) {
    if (i_ < 4 * kApFps && b.ok) {
      st_ = St::Channelling;
      return ApRun::Frame;
    }
    ++attempt_;
    return Attempt(b);
  }

  St st_ = St::Start;
  int attempt_ = 0;
  int64_t i_ = 0;
  Vec2 before_;
};

// Escort (quests 3.9): protect the escort NPC and lead it to its destination. False when the quest failed (the NPC
// died: re-acceptable from its giver), also when it had failed before this step (a lost attempt, not a bot failure), or
// on a bot failure; a quest that was never accepted is a bot failure.
class ApEscort final : public ApTask {
 public:
  explicit ApEscort(std::string questId) : questId_(std::move(questId)) {}
  ApRun Run(AutoPilotImpl& b) override {
    switch (st_) {
      case St::Start: {
        const QuestDef* q = b.data.FindQuest(questId_);
        if (q == nullptr) {
          b.Fail("unknown escort quest " + questId_);
          return Finish(false);
        }
        if (b.QuestDone(questId_)) return Finish(true);
        if (b.HasStatus(questId_, QuestStatus::Failed)) {
          // Lost before this step (the escort joined the hero on an earlier walk and died in a fight there): a lost
          // attempt like one lost on the way, re-acceptable, so ApEscortWithRetries takes it again from its giver.
          b.Note("escort failed before its step (the escort NPC died)");
          ++b.stats.escortFailures;
          return Finish(false);
        }
        if (!b.HasStatus(questId_, QuestStatus::Active)) {  // a route that never accepted it fails here, not by timeout
          b.Fail("escort quest not active: " + questId_);
          return Finish(false);
        }
        dest_ = q->escortNpc.dest.Center();
        st_ = St::AfterGoTo;
        // To the escort NPC: where it stands when it is already out (it joins the hero who passes its start tile and
        // follows from there), else its start tile.
        const EscortState* e = b.V().escort;
        const Vec2 meet = e != nullptr && e->active && e->questId == questId_ ? e->pos : q->escortNpc.start.Center();
        return b.Call<ApGoTo>(meet, 3.0, 300.0, true);
      }
      case St::AfterGoTo:
        if (!b.childResult) return Finish(false);
        end_ = b.frames + 600 * kApFps;
        lastOrder_ = -1000;
        break;
      case St::Loop: break;
      case St::AfterEngage: lastOrder_ = -1000; break;
    }
    if (!(b.ok && b.frames < end_)) {
      if (b.ok) b.Fail("escort timeout");
      return Finish(false);
    }
    st_ = St::Loop;
    if (b.QuestDone(questId_)) {
      b.Note("escort complete");
      return Finish(true);
    }
    if (b.HasStatus(questId_, QuestStatus::Failed)) {
      b.Note("escort failed (the escort NPC died)");  // reacceptable: the caller takes it again from its giver
      ++b.stats.escortFailures;
      return Finish(false);
    }
    if (!b.Alive()) return b.Call<ApWaitRespawn>();
    if (b.Busy()) return ApRun::Frame;
    const EscortState* e = b.V().escort;
    if (e == nullptr || !e->active) {
      b.Fail("no escort NPC out");
      return Finish(false);
    }
    // protect: anything near the merchant or us
    EntityId threat = b.BestThreat(kApThreatRadius);
    if (threat == kNoEntity) {
      for (const MonsterView& m : b.V().monsters) {
        if (m.alive && Dist(m.pos, e->pos) < 4.5 && m.state != MonsterState::Returning &&
            m.state != MonsterState::Idle && m.state != MonsterState::Patrol) {
          threat = m.id;
        }
      }
    }
    if (threat != kNoEntity) {
      st_ = St::AfterEngage;
      return b.Call<ApEngage>(threat, 30.0, 0);
    }
    const double gap = Dist(b.HeroPos(), e->pos);
    if (!e->joined) {
      if (gap > 4) {
        b.Submit(CmdMoveTo{e->pos});
        return b.Call<ApWait>(0.5);
      }
      return ApRun::Frame;
    }
    // The merchant re-paths to us 2.5x a second and stops a tile short (its path drops the first and last step,
    // quests 3.9), so it can rest ~3.5 tiles away while we stand: wait only when it really lags.
    if (gap > 5.0) {
      if (b.V().hero.moving) b.Submit(CmdStop{});
      return ApRun::Frame;
    }
    if (!b.V().hero.moving && b.frames - lastOrder_ > 20) {
      b.Submit(CmdMoveTo{dest_});
      lastOrder_ = b.frames;
    }
    return ApRun::Frame;
  }

 private:
  enum class St : uint8_t { Start, AfterGoTo, Loop, AfterEngage };
  St st_ = St::Start;
  std::string questId_;
  Vec2 dest_;
  int64_t end_ = 0, lastOrder_ = -1000;
};

// The escort, taken again from its giver after a failure (quests 3.9), at most `tries` attempts.
class ApEscortWithRetries final : public ApTask {
 public:
  ApEscortWithRetries(std::string questId, int tries) : questId_(std::move(questId)), tries_(tries) {}
  ApRun Run(AutoPilotImpl& b) override {
    switch (st_) {
      case St::Start: attempt_ = 0; break;
      case St::AfterEscort:
        if (b.childResult) return Finish(true);
        if (b.ok) {
          st_ = St::AfterRetake;
          return b.Call<ApVisitGiver>(b.GiverOf(questId_), std::vector<std::string>{questId_});
        }
        ++attempt_;
        break;
      case St::AfterRetake: ++attempt_; break;
    }
    if (attempt_ < tries_ && b.ok) {
      st_ = St::AfterEscort;
      return b.Call<ApEscort>(questId_);
    }
    if (b.ok && !b.QuestDone(questId_)) b.Fail("escort failed " + std::to_string(tries_) + " times: " + questId_);
    return Finish(b.ok);
  }

 private:
  enum class St : uint8_t { Start, AfterEscort, AfterRetake };
  St st_ = St::Start;
  std::string questId_;
  int tries_;
  int attempt_ = 0;
};

// Defend (quests 3.10): hold the quest's defend target through every wave. The bot stands `standOff` tiles out from the
// target towards the waves' approach (the defendTarget's approachDeg; on the target without one), so the waves meet it
// before they reach the target's chip radius; the waves start while it is within the start range. It fights the wave
// monsters (the one nearest to the target first, whatever its state: a wave ends only when all of it is dead), then
// anything else chasing it, and walks back to its stand between fights. Under 30 % life with no HP potion left it
// retreats to the nearest camp until 80 % (60 s at most). True once the quest completes; false when the target fell
// (the quest failed: reacceptable from its giver), also when it had fallen before this step (a lost attempt), or on a
// bot failure; a quest that was never accepted is a bot failure.
class ApDefend final : public ApTask {
 public:
  ApDefend(std::string questId, double standOff) : questId_(std::move(questId)), standOff_(standOff) {}
  ApRun Run(AutoPilotImpl& b) override {
    switch (st_) {
      case St::Start: {
        const QuestDef* q = b.data.FindQuest(questId_);
        if (q == nullptr || !q->hasDefendTarget) {
          b.Fail("no defend target for " + questId_);
          return Finish(false);
        }
        if (b.QuestDone(questId_)) return Finish(true);
        if (b.HasStatus(questId_, QuestStatus::Failed)) {
          // The target fell before this step (its waves met the hero on an earlier walk past it): a lost attempt,
          // re-acceptable, so ApDefendWithRetries takes it again from its giver.
          b.Note("defend failed before its step (the target fell)");
          ++b.stats.defendFailures;
          return Finish(false);
        }
        if (!b.HasStatus(questId_, QuestStatus::Active)) {  // no waves come for a quest that is not open: fail now
          b.Fail("defend quest not active: " + questId_);
          return Finish(false);
        }
        target_ = q->defendTarget.pos.Center();
        stand_ = target_;
        if (q->defendTarget.hasApproach) {
          const double a = q->defendTarget.approachDeg * kPi / 180.0;
          stand_ = target_ + Vec2(DetCos(a), DetSin(a)) * standOff_;
        }
        end_ = b.frames + 900 * kApFps;
        st_ = St::AfterApproach;
        return b.Call<ApGoTo>(stand_, 1.5, 300.0, true);
      }
      case St::AfterApproach:
        if (!b.childResult) return Finish(false);
        lastOrder_ = -1000;
        break;
      case St::Loop: break;
      case St::AfterEngage: lastOrder_ = -1000; break;
      case St::AfterRetreat:
        healEnd_ = b.frames + 60 * kApFps;
        st_ = St::Healing;
        return ApRun::Frame;
      case St::Healing: {
        const HeroView& h = b.V().hero;
        const bool healed = h.hp >= 0.8 * h.maxHp;
        if (!healed && b.frames < healEnd_ && b.Alive() && !b.QuestDone(questId_) &&
            !b.HasStatus(questId_, QuestStatus::Failed) && b.NearestThreat(kApThreatRadius) == kNoEntity) {
          return ApRun::Frame;
        }
        b.Note("back to the defend target");
        lastOrder_ = -1000;
        break;
      }
    }
    if (!(b.ok && b.frames < end_)) {
      if (b.ok) b.Fail("defend timeout: " + questId_);
      return Finish(false);
    }
    st_ = St::Loop;
    if (b.QuestDone(questId_)) {
      b.Note("defend complete");
      return Finish(true);
    }
    if (b.HasStatus(questId_, QuestStatus::Failed)) {
      b.Note("defend failed (the target fell)");  // reacceptable: the caller takes it again from its giver
      ++b.stats.defendFailures;
      return Finish(false);
    }
    if (!b.Alive()) return b.Call<ApWaitRespawn>();
    if (b.Busy()) return ApRun::Frame;
    const HeroView& h = b.V().hero;
    if (h.hp < 0.3 * h.maxHp && b.V().potionSlots[0].count <= 0) {
      Vec2 camp;
      double best = 1e18;
      for (const Vec2& c : b.V().zone.camps) {
        const double d = DistSq(c, b.HeroPos());
        if (d < best) {
          best = d;
          camp = c;
        }
      }
      if (best < 1e17) {
        b.Note("retreating to heal");
        ++b.stats.defendRetreats;
        st_ = St::AfterRetreat;
        return b.Call<ApGoTo>(camp, 3.0, 120.0, false);
      }
    }
    if (b.Evade(target_)) return ApRun::Frame;  // W2: out of a strike's shape or a hazard
    // The wave first (nearest to the target), then whatever else is on us.
    EntityId threat = kNoEntity;
    double bestD = 20.0;
    for (const MonsterView& m : b.V().monsters) {
      if (!m.alive || m.role != MonsterRole::DefendWave) continue;
      const double d = Dist(m.pos, target_);
      if (d < bestD) {
        bestD = d;
        threat = m.id;
      }
    }
    if (threat == kNoEntity) threat = b.BestThreat(kApThreatRadius);
    if (threat != kNoEntity) {
      st_ = St::AfterEngage;
      return b.Call<ApEngage>(threat, 30.0, 0);
    }
    if (Dist(b.HeroPos(), stand_) > 1.5 && !b.V().hero.moving && b.frames - lastOrder_ > 20) {
      b.Submit(CmdMoveTo{stand_});
      lastOrder_ = b.frames;
    }
    return ApRun::Frame;
  }

 private:
  enum class St : uint8_t { Start, AfterApproach, Loop, AfterEngage, AfterRetreat, Healing };
  St st_ = St::Start;
  std::string questId_;
  double standOff_;
  Vec2 target_, stand_;
  int64_t end_ = 0, lastOrder_ = -1000, healEnd_ = 0;
};

// The defend, taken again from its giver after the target fell (quests 3.10, reacceptable), at most `tries` attempts.
class ApDefendWithRetries final : public ApTask {
 public:
  ApDefendWithRetries(std::string questId, int tries, double standOff)
      : questId_(std::move(questId)), tries_(tries), standOff_(standOff) {}
  ApRun Run(AutoPilotImpl& b) override {
    switch (st_) {
      case St::Start: attempt_ = 0; break;
      case St::AfterDefend:
        if (b.childResult) return Finish(true);
        ++attempt_;
        if (b.ok && attempt_ < tries_) {
          st_ = St::AfterRetake;
          return b.Call<ApVisitGiver>(b.GiverOf(questId_), std::vector<std::string>{questId_});
        }
        break;
      case St::AfterRetake: break;
    }
    if (attempt_ < tries_ && b.ok) {
      st_ = St::AfterDefend;
      return b.Call<ApDefend>(questId_, standOff_);
    }
    if (b.ok && !b.QuestDone(questId_)) b.Fail("defend failed " + std::to_string(tries_) + " times: " + questId_);
    return Finish(b.ok);
  }

 private:
  enum class St : uint8_t { Start, AfterDefend, AfterRetake };
  St st_ = St::Start;
  std::string questId_;
  int tries_;
  double standOff_;
  int attempt_ = 0;
};

// The bottom task: plays AutoPilotImpl::steps from stepIdx.
class ApScript final : public ApTask {
 public:
  ApRun Run(AutoPilotImpl& b) override {
    if (waiting_) {  // the step's task finished (a failure has already set ok = false)
      waiting_ = false;
      ++b.stepIdx;
    }
    while (b.stepIdx < b.steps.size()) {
      AutoPilotImpl::Step& s = b.steps[b.stepIdx];
      switch (s.kind) {
        case AutoPilotImpl::Step::Kind::Note:
          b.Note(s.text);
          ++b.stepIdx;
          break;
        case AutoPilotImpl::Step::Kind::Checkpoint:
          b.checkpoint = s.text;
          b.Note("checkpoint " + s.text);
          b.Milestone(AutoPilotMilestone::Checkpoint, s.text);
          ++b.stepIdx;
          return ApRun::Checkpoint;
        case AutoPilotImpl::Step::Kind::Milestone:
          b.Milestone(AutoPilotMilestone::ScenarioDone, s.text);
          if (b.cfg.scenario == AutoPilotScenario::Chapter1) b.Milestone(AutoPilotMilestone::ChapterDone, b.routeZone);
          ++b.stepIdx;
          break;
        case AutoPilotImpl::Step::Kind::Task:
          b.currentStep = s.text;
          waiting_ = true;
          b.pendingCall = s.make(b);
          return ApRun::Call;
      }
    }
    return Finish(true);
  }

 private:
  bool waiting_ = false;
};

}  // namespace autopilot_impl

// ---------------------------------------------------------------------------------------------------------------------
// The scenarios: the route of the starting zone (autopilot_routes.json), step by step. Checkpoints sit where the
// playthrough test checks the state; Smoke stops at the route's smokeEnd step.
// ---------------------------------------------------------------------------------------------------------------------
void AutoPilotImpl::BuildScenario() {
  using Kind = Step::Kind;
  using SK = AutoPilotStepKind;
  const AutoPilotScenario scenario = cfg.scenario;
  if (scenario == AutoPilotScenario::None) return;
  const AutoPilotRoute* route = data.AutoPilotRoutes().ForZone(data.World().defaultMap);
  if (route == nullptr) {
    Fail("no autopilot route plays the starting zone '" + data.World().defaultMap + "'");
    return;
  }
  routeZone = route->zone;
  const auto at = [](TilePos t) { return t.Center(); };
  for (const AutoPilotStep& s : route->steps) {
    switch (s.kind) {
      case SK::Checkpoint: AddStep(Kind::Checkpoint, s.id); break;
      case SK::Note: AddStep(Kind::Note, s.id); break;
      case SK::WaitIdle: {
        const double sec = s.maxSec;
        AddTask("wait until idle", [sec](AutoPilotImpl&) { return std::make_unique<ApWaitIdle>(sec); });
        break;
      }
      case SK::Talk:
        AddTask("talk to " + s.npc, [npc = s.npc](AutoPilotImpl&) { return std::make_unique<ApTalkTo>(npc); });
        break;
      case SK::ClosePanels:
        AddTask("close the panels", [](AutoPilotImpl&) { return std::make_unique<ApClosePanels>(); });
        break;
      case SK::Tree:
        AddTask("the dialogue tree of " + s.npc, [npc = s.npc, path = s.ids](AutoPilotImpl&) {
          return std::make_unique<ApDialogueTree>(npc, path);
        });
        break;
      case SK::Visit:
        AddTask("quest card of " + s.npc, [npc = s.npc, accept = s.ids](AutoPilotImpl&) {
          return std::make_unique<ApVisitGiver>(npc, accept);
        });
        break;
      case SK::Shop:
        AddTask("shop at " + s.npc, [npc = s.npc, hp = s.hp, mp = s.mp](AutoPilotImpl& b) {
          return std::make_unique<ApShop>(npc, hp >= 0 ? hp : b.HpPotionTarget(), mp >= 0 ? mp : b.MpPotionTarget());
        });
        break;
      case SK::TownTrip:
        AddTask("town trip", [npc = s.npc](AutoPilotImpl&) { return std::make_unique<ApTownTrip>(npc); });
        break;
      case SK::Gather:
        AddTask("gather " + s.quest + "#" + std::to_string(s.obj),
                [quest = s.quest, obj = static_cast<size_t>(s.obj), count = s.count](AutoPilotImpl&) {
                  return std::make_unique<ApGather>(quest, obj, count);
                });
        break;
      case SK::Hunt: {
        const std::vector<std::string> defs = s.ids;
        ApMonsterFilter match = [defs](const MonsterView& m) {
          return std::find(defs.begin(), defs.end(), m.defId) != defs.end();
        };
        AddTask("hunt " + defs.front(), [what = defs.front(), match, centre = at(s.tile), until = s.until,
                                         sec = s.maxSec](AutoPilotImpl&) {
          return std::make_unique<ApHunt>(what, match, centre,
                                          ApPredicate([until](const AutoPilotImpl& b) { return b.Holds(until); }), sec);
        });
        break;
      }
      case SK::Boss: {
        ApMonsterFilter match;
        switch (s.role) {
          case AutoPilotBossRole::StoryBoss:
            match = [](const MonsterView& m) { return m.role == MonsterRole::StoryBoss; };
            break;
          case AutoPilotBossRole::MiniBoss:
            match = [](const MonsterView& m) { return m.role == MonsterRole::ZoneMiniBoss || m.miniBoss; };
            break;
          case AutoPilotBossRole::HuntLeader:
            match = [](const MonsterView& m) { return m.role == MonsterRole::HuntLeader; };
            break;
        }
        const std::string what(EnumName(s.role));
        AddTask("hunt the " + what, [what, match, centre = at(s.tile), until = s.until, sec = s.maxSec](AutoPilotImpl&) {
          return std::make_unique<ApHunt>(what, match, centre,
                                          ApPredicate([until](const AutoPilotImpl& b) { return b.Holds(until); }), sec);
        });
        break;
      }
      case SK::Clues: {
        std::vector<Vec2> tiles;
        for (TilePos t : s.tiles) tiles.push_back(at(t));
        AddTask("clues of " + s.quest, [quest = s.quest, tiles](AutoPilotImpl&) {
          return std::make_unique<ApClues>(quest, tiles);
        });
        break;
      }
      case SK::Explore:
        AddTask("explore " + s.quest + "#" + std::to_string(s.obj),
                [quest = s.quest, obj = static_cast<size_t>(s.obj), centre = at(s.tile)](AutoPilotImpl&) {
                  return std::make_unique<ApExplore>(quest, obj, centre);
                });
        break;
      case SK::Escort:
        AddTask("escort " + s.quest, [quest = s.quest, tries = s.tries](AutoPilotImpl&) {
          return std::make_unique<ApEscortWithRetries>(quest, tries);
        });
        break;
      case SK::Defend:
        AddTask("defend " + s.quest, [quest = s.quest, tries = s.tries, standOff = s.standOff](AutoPilotImpl&) {
          return std::make_unique<ApDefendWithRetries>(quest, tries, standOff);
        });
        break;
      case SK::GoTo: {
        char label[64];
        std::snprintf(label, sizeof(label), "walk to (%d,%d)", s.tile.col, s.tile.row);
        AddTask(label, [goal = at(s.tile), arrive = s.arrive, sec = s.maxSec](AutoPilotImpl&) {
          return std::make_unique<ApGoTo>(goal, arrive, sec, true);
        });
        break;
      }
      case SK::PortalHome:
        AddTask("portal home", [](AutoPilotImpl&) { return std::make_unique<ApPortalHome>(); });
        break;
      case SK::EquipUpgrades:
        AddTask("equip upgrades", [](AutoPilotImpl&) { return std::make_unique<ApEquipUpgrades>(); });
        break;
      case SK::SmokeEnd:
        if (scenario == AutoPilotScenario::Smoke) {
          AddStep(Kind::Milestone, std::string(EnumName(scenario)));
          return;
        }
        break;
    }
  }
  AddStep(Kind::Milestone, std::string(EnumName(scenario)));
}

AutoPilotStatus AutoPilotImpl::RunScript() {
  if (!ok) {
    pendingCall.reset();
    while (!stack.empty()) stack.pop_back();
    return AutoPilotStatus::Finished;
  }
  if (stack.empty()) {
    if (stepIdx >= steps.size()) return AutoPilotStatus::Finished;
    stack.push_back(std::make_unique<ApScript>());
  }
  for (int runs = 0; runs < kApMaxRunsPerAdvance; ++runs) {
    if (!ok) {
      pendingCall.reset();
      while (!stack.empty()) stack.pop_back();
      return AutoPilotStatus::Finished;
    }
    if (stack.empty()) return AutoPilotStatus::Finished;
    ApTask& top = *stack.back();
    switch (top.Run(*this)) {
      case ApRun::Frame: return AutoPilotStatus::NeedFrame;
      case ApRun::Checkpoint: return AutoPilotStatus::Checkpoint;
      case ApRun::Call:
        if (pendingCall == nullptr) {
          Fail("bot task called nothing");
          break;
        }
        stack.push_back(std::move(pendingCall));
        break;
      case ApRun::Done:
        childResult = top.result;
        stack.pop_back();
        break;
    }
  }
  Fail("bot logic made no progress without a frame");
  pendingCall.reset();
  while (!stack.empty()) stack.pop_back();
  return AutoPilotStatus::Finished;
}

// ---------------------------------------------------------------------------------------------------------------------
// AutoPilot
// ---------------------------------------------------------------------------------------------------------------------
AutoPilot::AutoPilot(const DataStore& data, AutoPilotConfig cfg, AutoPilotSubmit submit)
    : impl_(std::make_unique<AutoPilotImpl>(data, std::move(cfg), std::move(submit))) {
  impl_->BuildScenario();
}

AutoPilot::~AutoPilot() = default;

void AutoPilot::AddCheckpoint(const std::string& id) { impl_->AddStep(AutoPilotImpl::Step::Kind::Checkpoint, id); }

void AutoPilot::AddWaitIdle(double timeoutSec) {
  impl_->AddTask("wait until idle",
                 [timeoutSec](AutoPilotImpl&) { return std::make_unique<ApWaitIdle>(timeoutSec); });
}

void AutoPilot::AddWait(double seconds) {
  impl_->AddTask("wait", [seconds](AutoPilotImpl&) { return std::make_unique<ApWait>(seconds); });
}

void AutoPilot::AddGoTo(Vec2 goal, double arrive, double timeoutSec) {
  char label[64];
  std::snprintf(label, sizeof(label), "walk to (%.1f,%.1f)", goal.x, goal.y);
  impl_->AddTask(label, [goal, arrive, timeoutSec](AutoPilotImpl&) {
    return std::make_unique<ApGoTo>(goal, arrive, timeoutSec, true);
  });
}

void AutoPilot::AddLootNearby(double radius) {
  impl_->AddTask("loot nearby", [radius](AutoPilotImpl&) { return std::make_unique<ApLootNearby>(radius); });
}

void AutoPilot::AddEscort(const std::string& questId, int tries) {
  impl_->AddTask("escort " + questId,
                 [questId, tries](AutoPilotImpl&) { return std::make_unique<ApEscortWithRetries>(questId, tries); });
}

void AutoPilot::AddDefend(const std::string& questId, int tries, double standOff) {
  impl_->AddTask("defend " + questId, [questId, tries, standOff](AutoPilotImpl&) {
    return std::make_unique<ApDefendWithRetries>(questId, tries, standOff);
  });
}

AutoPilotStatus AutoPilot::Advance(const Snapshot& view) {
  AutoPilotImpl& b = *impl_;
  b.view = &view;
  if (b.hold) return b.ok ? AutoPilotStatus::NeedFrame : AutoPilotStatus::Finished;
  const AutoPilotStatus st = b.RunScript();
  if (st == AutoPilotStatus::Finished && !b.finishedNoted) {
    b.finishedNoted = true;
    b.Note(b.ok ? "script finished" : "script stopped: " + b.failure);
  }
  return st;
}

void AutoPilot::PrepareFrame(const Snapshot& view) {
  AutoPilotImpl& b = *impl_;
  b.view = &view;
  if (!b.hold) b.AutoUi();
  b.monsterDefs.clear();
  for (const MonsterView& m : view.monsters) {
    b.monsterDefs.push_back(AutoPilotImpl::SeenMonster{m.id, m.defId, m.role});
  }
  b.heroHpTrack = view.hero.hp;
  b.lastFrozen = view.frozen;
  b.frameOpen = true;
}

void AutoPilot::ObserveEvent(const Event& e) {
  impl_->CountFrame();
  impl_->ProcessEvent(e);
}

void AutoPilot::EndFrame(const Snapshot& view, int32_t steps) {
  AutoPilotImpl& b = *impl_;
  b.view = &view;
  if (!b.frameCounted) ++b.frames;
  b.frameOpen = false;
  b.frameCounted = false;
  b.lastSteps = steps;
  if (!b.hold) b.AutoHero();
}

AutoPilotStatus AutoPilot::Tick(GameSim& sim, double dtMs) {
  const AutoPilotStatus st = Advance(sim.View());
  if (st != AutoPilotStatus::NeedFrame) return st;
  RunFrame(sim, dtMs);
  return st;
}

void AutoPilot::RunFrame(GameSim& sim, double dtMs) {
  PrepareFrame(sim.View());
  const int32_t steps = sim.Frame(dtMs);
  for (const Event& e : sim.Events()) ObserveEvent(e);
  EndFrame(sim.View(), steps);
}

void AutoPilot::SetHold(bool hold) {
  if (impl_->hold == hold) return;
  impl_->hold = hold;
  impl_->Note(hold ? "hold" : "resume");
}

bool AutoPilot::Held() const { return impl_->hold; }
bool AutoPilot::Ok() const { return impl_->ok; }
const std::string& AutoPilot::Failure() const { return impl_->failure; }
bool AutoPilot::Finished() const {
  return !impl_->ok || (impl_->stack.empty() && impl_->stepIdx >= impl_->steps.size());
}
const std::string& AutoPilot::CurrentCheckpoint() const { return impl_->checkpoint; }
const std::string& AutoPilot::CurrentStep() const { return impl_->currentStep; }

std::string AutoPilot::RecentNotes() const {
  std::string out;
  for (const std::string& s : impl_->recent) out += s + "\n";
  return out;
}

const AutoPilotStats& AutoPilot::Stats() const { return impl_->stats; }
const std::vector<AutoPilotMilestoneRecord>& AutoPilot::Milestones() const { return impl_->milestones; }
bool AutoPilot::Reached(AutoPilotMilestone kind) const { return impl_->Reached(kind); }
int64_t AutoPilot::Frames() const { return impl_->frames; }
const AutoPilotConfig& AutoPilot::Config() const { return impl_->cfg; }

}  // namespace abyss

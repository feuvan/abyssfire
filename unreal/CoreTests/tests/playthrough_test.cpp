// Headless, seeded, scripted CHAPTER 1 playthrough (ARCHITECTURE 3.3, milestone-1 exit criterion 1).
//
// For each class: new game -> prologue (skipped) -> the camp NPCs (blacksmith, merchant, stash keeper, the elder's
// dialogue tree and quest card) -> every emerald_plains quest (main M1-M5 and the six side quests: kill, drop-collect,
// gather, talk delivery, investigate + hunt, bounty hunt, escort, the pet_sprite clues) plus the zone mini-boss and the
// goblin chief -> the chapter-1 story beats -> save -> load into a fresh GameSim (SaveGame() identical) -> continue to
// the sealed chapter-2 gate.
//
// Everything goes through the public GameSim API: Commands in, Events / Snapshot out. A small bot plays: it paths with
// CmdMoveTo, fights the nearest threat (CmdAttackTarget + hotbar CmdCastSkill), picks up loot (CmdPickUp), equips
// upgrades, buys / sells at the shops, drinks potions under 35 % HP, and spends stat / skill points on level-up. Story
// beats are advanced like a player tapping (the prologue is skipped). No debug commands are used.
//
// Environment switches: ABYSS_PT_VERBOSE=1 prints the bot's notes while it plays; ABYSS_PT_SWEEP=<n> (and
// ABYSS_PT_SWEEP_BASE=<first seed>) plays n extra seeds per class; ABYSS_PT_DUMP=<dir> writes each run's final save
// (<class>_<seed>.json) so GCC and Clang builds can be diffed - they must agree bit for bit.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "TestUtil.h"
#include "abyss/base/Math.h"
#include "abyss/base/SimClock.h"
#include "abyss/data/DataStore.h"
#include "abyss/hero/Skills.h"
#include "abyss/items/Inventory.h"
#include "abyss/items/ItemCompare.h"
#include "abyss/items/Shop.h"
#include "abyss/pets/Homestead.h"
#include "abyss/pets/PetSystem.h"
#include "abyss/quests/Achievements.h"
#include "abyss/quests/Dialogue.h"
#include "abyss/quests/Lore.h"
#include "abyss/quests/QuestWorld.h"
#include "abyss/sim/GameSim.h"
#include "abyss/story/StoryDirector.h"
#include "abyss/world/RandomEvents.h"
#include "doctest/doctest.h"

using namespace abyss;

namespace {

constexpr double kPtFrameMs = kSimStepMs;
constexpr int64_t kPtFps = 60;
constexpr double kPtThreatRadius = 6.5;

bool PtVerbose() {
  const char* v = std::getenv("ABYSS_PT_VERBOSE");
  return v != nullptr && *v != '\0' && *v != '0';
}

const char* PtClassName(ClassId c) {
  switch (c) {
    case ClassId::Warrior: return "warrior";
    case ClassId::Mage: return "mage";
    case ClassId::Rogue: return "rogue";
  }
  return "?";
}

// ---------------------------------------------------------------------------------------------------------------------
// The bot
// ---------------------------------------------------------------------------------------------------------------------
class PtBot {
 public:
  PtBot(GameSim& sim, ClassId cls) : sim_(sim), data_(test::RealData()), cls_(cls), verbose_(PtVerbose()) {}

  // ---- outcome ----
  bool ok = true;
  std::string failure;
  int64_t frames = 0;
  int deaths = 0, levelUps = 0, kills = 0, potionsDrunk = 0, itemsPicked = 0, equipChanges = 0, saveRequests = 0;
  int sealedBanners = 0, miniBossDialogues = 0, itemsSold = 0, itemsBought = 0, skillCasts = 0;
  bool petObtained = false;
  int petFeeds = 0;
  int escortFailures = 0;
  std::array<int64_t, 8> expBySource{};  // ExpSource order
  std::set<std::string> killedDefs;
  std::vector<std::string> beatsBegan, beatsEnded;

  GameSim& Sim() { return sim_; }
  const Snapshot& V() const { return sim_.View(); }
  Vec2 HeroPos() const { return V().hero.pos; }
  double Now() const { return sim_.NowMs(); }
  bool Alive() const { return V().hero.life == HeroLife::Alive && V().hero.hp > 0; }

  void Note(const std::string& s) {
    char head[96];
    std::snprintf(head, sizeof(head), "[%s %7.1fs L%d hp %.0f/%.0f (%.1f,%.1f)] ", PtClassName(cls_),
                  frames / static_cast<double>(kPtFps), V().hero.level, V().hero.hp, V().hero.maxHp, V().hero.pos.x,
                  V().hero.pos.y);
    std::string line = std::string(head) + s;
    if (verbose_) std::printf("%s\n", line.c_str());
    recent_.push_back(std::move(line));
    if (recent_.size() > 80) recent_.pop_front();
  }
  void Fail(const std::string& why) {
    if (!ok) return;
    ok = false;
    failure = why + " | " + StateLine();
    Note("FAIL: " + failure);
  }
  std::string StateLine() const {
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
  std::string Recent() const {
    std::string out;
    for (const std::string& s : recent_) out += s + "\n";
    return out;
  }

  // ---- one rendered frame ----
  void Frame() {
    AutoUi();
    monsterDefs_.clear();
    for (const MonsterView& m : V().monsters) monsterDefs_.emplace_back(m.id, m.defId);
    lastFrozen_ = sim_.WorldFrozen();
    lastSteps_ = sim_.Frame(kPtFrameMs);
    ++frames;
    ProcessEvents();
    AutoHero();
  }
  // Frames until the commands submitted so far were applied: a frame may run no sim step at all (S6 slow motion after
  // an elite kill dilates the sim clock, S1 accumulator), and commands apply at the start of the next step - or in the
  // UI-only step of a frozen frame.
  void Commit() {
    for (int i = 0; i < 4 * kPtFps && ok; ++i) {
      Frame();
      if (lastSteps_ > 0 || lastFrozen_) return;
    }
  }
  void Wait(double seconds) {
    const int64_t end = frames + static_cast<int64_t>(seconds * kPtFps);
    while (frames < end && ok) Frame();
  }
  // Story, modals and transitions: nothing for the hero to do but wait.
  bool Busy() const {
    const Snapshot& v = V();
    return v.frozen || v.inputBlocked || v.storyBusy || !v.modals.empty();
  }
  // Plays frames until no story beat is queued / playing and no core modal is up (closing stray ones).
  bool WaitIdle(double timeoutSec = 120) {
    const int64_t end = frames + static_cast<int64_t>(timeoutSec * kPtFps);
    while (ok && frames < end) {
      const Snapshot& v = V();
      if (!v.storyBusy && !v.frozen && v.modals.empty()) return true;
      if (!v.storyBusy && !v.modals.empty() && (frames % 30) == 0) {
        for (PanelId p : v.modals) sim_.Submit(CmdClosePanel{p});
      }
      Frame();
    }
    Fail("WaitIdle timeout");
    return false;
  }

  // ---- queries ----
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
  EntityId NearestOf(const std::vector<std::string>& defIds, Vec2 from, double maxDist) const {
    EntityId best = kNoEntity;
    double bestD = maxDist;
    for (const MonsterView& m : V().monsters) {
      if (!m.alive || m.state == MonsterState::Returning) continue;
      if (std::find(defIds.begin(), defIds.end(), m.defId) == defIds.end()) continue;
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
  int32_t CountOf(const std::string& baseId) const {
    return V().inventory != nullptr ? V().inventory->CountOf(baseId) : 0;
  }
  size_t BagSize() const { return V().inventory != nullptr ? V().inventory->Bag().size() : 0; }

  // ---- walking ----
  // Walks to `goal` (fighting whatever attacks on the way). True once within `arrive` tiles.
  bool GoTo(Vec2 goal, double arrive, double timeoutSec = 240, bool fight = true) {
    const int64_t end = frames + static_cast<int64_t>(timeoutSec * kPtFps);
    int64_t lastOrder = -1000;
    Vec2 lastPos = HeroPos();
    int64_t lastProgress = frames;
    while (ok && frames < end) {
      if (Dist(HeroPos(), goal) <= arrive) return true;
      if (!Alive()) {
        WaitRespawn();
        lastOrder = -1000;
        continue;
      }
      if (Busy()) {
        Frame();
        lastProgress = frames;
        continue;
      }
      if (fight && FightThreats(kPtThreatRadius)) {
        lastOrder = -1000;
        lastProgress = frames;
        continue;
      }
      if (!V().hero.moving && frames - lastOrder > 20) {
        sim_.Submit(CmdMoveTo{goal});
        lastOrder = frames;
      }
      Frame();
      if (Dist(HeroPos(), lastPos) > 1.0) {
        lastPos = HeroPos();
        lastProgress = frames;
      } else if (frames - lastProgress > 30 * kPtFps) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "stuck walking to (%.1f,%.1f)", goal.x, goal.y);
        Fail(buf);
        return false;
      }
    }
    if (ok) {
      char buf[128];
      std::snprintf(buf, sizeof(buf), "timeout walking to (%.1f,%.1f)", goal.x, goal.y);
      Fail(buf);
    }
    return false;
  }

  void WaitRespawn() {
    Note("hero down, waiting for the respawn");
    const int64_t end = frames + 30 * kPtFps;
    while (ok && frames < end && !Alive()) Frame();
    if (!Alive()) Fail("hero never respawned");
  }

  // ---- fighting ----
  bool FightThreats(double radius) {
    const EntityId t = NearestThreat(radius);
    if (t == kNoEntity) return false;
    Engage(t, 45);
    return true;
  }

  // Attacks `target` until it dies (true) or the timeout / hero death (false).
  bool Engage(EntityId target, double timeoutSec, int depth = 0) {
    const int64_t end = frames + static_cast<int64_t>(timeoutSec * kPtFps);
    int64_t lastOrder = -1000;
    int64_t lastCast = -1000;
    while (ok && frames < end) {
      const MonsterView* m = FindMonster(target);
      if (m == nullptr || !m->alive) return true;
      if (!Alive()) {
        WaitRespawn();
        return false;
      }
      if (Busy()) {
        Frame();
        continue;
      }
      // Something else is hitting us while the target is still far: deal with it first.
      if (depth == 0 && Dist(m->pos, HeroPos()) > 4.0) {
        const EntityId close = NearestThreat(2.5);
        if (close != kNoEntity && close != target) {
          Engage(close, 30, 1);
          lastOrder = -1000;
          continue;
        }
      }
      if (V().hero.target != target && frames - lastOrder > 15) {
        sim_.Submit(CmdAttackTarget{target});
        lastOrder = frames;
      }
      if (frames - lastCast >= 8 && TryCast(*m)) lastCast = frames;
      Frame();
    }
    if (ok) Note("gave up on target " + std::to_string(target));
    sim_.Submit(CmdClearTarget{});
    return false;
  }

  static bool PtSkipSkill(const std::string& id) {
    return id == "teleport" || id == "vanish" || id == "shadow_step" || id == "taunt_roar";
  }

  bool TryCast(const MonsterView& m) {
    const HeroView& h = V().hero;
    const double d = Dist(m.pos, h.pos);
    for (int slot = 0; slot < 6; ++slot) {
      const SkillSlotView& s = h.hotbar[static_cast<size_t>(slot)];
      if (s.skillIndex < 0 || !s.usable) continue;
      const SkillDef* def = data_.FindSkill(s.skillId);
      if (def == nullptr || def->passive || PtSkipSkill(def->id)) continue;
      if (def->range > 0 ? d > def->range + 0.25 : d > 2.5) continue;
      sim_.Submit(CmdCastSkill{slot, m.id, false, Vec2(), Vec2()});
      ++skillCasts;
      return true;
    }
    return false;
  }

  // Kills monsters of `defIds` (nearest first, around `centre`) until `done()`.
  bool Hunt(const std::vector<std::string>& defIds, Vec2 centre, const std::function<bool()>& done,
            double timeoutSec = 600) {
    const int64_t end = frames + static_cast<int64_t>(timeoutSec * kPtFps);
    int64_t idleSince = -1;
    while (ok && !done()) {
      if (frames >= end) {
        Fail("hunt timeout for " + defIds.front());
        return false;
      }
      if (!Alive()) {
        WaitRespawn();
        continue;
      }
      if (Busy()) {
        Frame();
        continue;
      }
      if (FightThreats(kPtThreatRadius)) continue;
      LootNearby(7);
      if (!ok) return false;
      EquipUpgrades();
      FeedPet();
      const EntityId t = NearestOf(defIds, HeroPos(), 60);
      if (t != kNoEntity) {
        idleSince = -1;
        Engage(t, 60);
        continue;
      }
      if (Dist(HeroPos(), centre) > 4) {
        GoTo(centre, 3, 180);
        continue;
      }
      if (idleSince < 0) idleSince = frames;
      if (frames - idleSince > 120 * kPtFps) {
        Fail("no " + defIds.front() + " to hunt (nothing respawned)");
        return false;
      }
      Frame();
    }
    return ok;
  }

  // ---- loot and gear ----
  double StatWeight(Stat s) const {
    const bool war = cls_ == ClassId::Warrior, mage = cls_ == ClassId::Mage, rog = cls_ == ClassId::Rogue;
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
    for (const StatTotal& t : ItemStatTotals(data_, *it)) {
      switch (t.key.kind) {
        case CompareKey::Kind::AvgDamage: break;  // inert in milestone 1 (C10)
        case CompareKey::Kind::BaseDefense: s += t.value * 0.4; break;
        case CompareKey::Kind::StatKey: s += t.value * StatWeight(t.key.stat); break;
      }
    }
    return s;
  }
  bool Equippable(const ItemInstance& it) const {
    const ItemBaseDef* base = data_.FindItemBase(it.baseId);
    return base != nullptr && base->hasSlot && base->levelReq <= V().hero.level;
  }
  // Positive when `it` would be better than what its slot holds now.
  double UpgradeGain(const ItemInstance& it) const {
    if (V().inventory == nullptr || !Equippable(it)) return 0;
    const std::optional<CompareTarget> target = FindCompareTarget(data_, it, V().inventory->Equipment());
    if (!target.has_value()) return 0;
    return Score(&it) - Score(target->equipped);
  }
  bool Wanted(const ItemInstance& it) const {
    const ItemBaseDef* base = data_.FindItemBase(it.baseId);
    if (base == nullptr) return false;
    if (!base->hasSlot) return true;  // potions, gems, materials, ley fruit
    if (it.quality != ItemQuality::Normal) return true;
    return UpgradeGain(it) > 0.5;
  }

  void LootNearby(double radius) {
    for (int guard = 0; guard < 24 && ok; ++guard) {
      if (Busy() || !Alive()) return;
      if (NearestThreat(kPtThreatRadius) != kNoEntity) return;
      if (V().inventory != nullptr && V().inventory->IsFull()) return;
      EntityId pick = kNoEntity;
      Vec2 pos;
      double bestD = radius;
      for (const GroundItemView& g : V().groundItems) {
        if (g.item == nullptr || skippedLoot_.count(g.id) != 0 || !Wanted(*g.item)) continue;
        const double d = Dist(g.pos, HeroPos());
        if (d <= bestD) {
          bestD = d;
          pick = g.id;
          pos = g.pos;
        }
      }
      bool potion = false;
      for (const PotionDropView& p : V().potions) {
        const double d = Dist(p.pos, HeroPos());
        if (skippedLoot_.count(p.id) == 0 && d <= bestD) {
          bestD = d;
          pick = p.id;
          pos = p.pos;
          potion = true;
        }
      }
      if (pick == kNoEntity) return;
      if (potion) {
        if (!GoTo(pos, 1.0, 15)) return;
        // potions are auto-collected within 2 tiles; mark it so a stuck one cannot loop
        skippedLoot_.insert(pick);
        continue;
      }
      sim_.Submit(CmdPickUp{pick});
      const int64_t end = frames + 8 * kPtFps;
      bool gone = false;
      while (ok && frames < end) {
        Frame();
        bool present = false;
        for (const GroundItemView& g : V().groundItems) present = present || g.id == pick;
        if (!present) {
          gone = true;
          break;
        }
        if (NearestThreat(3) != kNoEntity) break;
      }
      if (!gone) skippedLoot_.insert(pick);
    }
  }

  // P panel: feed ley fruit to the active beast (quests 18.8) while it can still grow.
  void FeedPet() {
    const Snapshot& v = V();
    if (feedRefused_ || v.pets == nullptr || v.pets->Active() == nullptr || CountOf("c_ley_fruit") <= 0) return;
    const std::string id(v.pets->ActiveId());
    const int64_t before = v.pets->Active()->exp + 1000 * v.pets->Active()->level;
    sim_.Submit(CmdFeedPet{id});
    Commit();
    const PetInstance* p = V().pets->Active();
    if (p != nullptr && p->exp + 1000 * p->level != before) {
      ++petFeeds;
      Note("fed " + id);
    } else {
      feedRefused_ = true;
      Note("feeding refused");
    }
  }

  void EquipUpgrades() {
    if (V().inventory == nullptr) return;
    for (int guard = 0; guard < 12; ++guard) {
      std::string uid;
      double best = 0.5;
      for (const ItemInstance& it : V().inventory->Bag()) {
        const double g = UpgradeGain(it);
        if (g > best) {
          best = g;
          uid = it.uid;
        }
      }
      if (uid.empty()) return;
      sim_.Submit(CmdEquip{uid});
      Commit();
      bool stillInBag = false;
      for (const ItemInstance& it : V().inventory->Bag()) stillInBag = stillInBag || it.uid == uid;
      Note("equip " + uid + (stillInBag ? " (still in the bag)" : ""));
    }
  }

  // ---- NPCs, shops, quest cards ----
  struct PanelSession {
    explicit PanelSession(PtBot& b) : bot(b) { ++bot.panelSession_; }
    ~PanelSession() { --bot.panelSession_; }
    PanelSession(const PanelSession&) = delete;
    PanelSession& operator=(const PanelSession&) = delete;
    PtBot& bot;
  };
  // Walks to the NPC and talks (CmdInteract); true once one of its core modals is up.
  bool TalkTo(const std::string& npcId) {
    if (!WaitIdle()) return false;
    const NpcView* n = FindNpc(npcId);
    if (n == nullptr) {
      Fail("npc not in zone: " + npcId);
      return false;
    }
    const EntityId id = n->id;
    if (!GoTo(n->pos, 2.0, 300)) return false;
    PanelSession session(*this);
    for (int attempt = 0; attempt < 3 && ok; ++attempt) {
      if (!WaitIdle()) return false;
      sim_.Submit(CmdInteract{id});
      for (int i = 0; i < 4 * kPtFps && ok; ++i) {
        Frame();
        if (!V().modals.empty()) return true;
      }
    }
    Fail("talking to " + npcId + " opened nothing");
    return false;
  }

  bool ClosePanels() {
    for (int i = 0; i < 120 && ok; ++i) {
      if (V().modals.empty()) return true;
      if ((i % 10) == 0) {
        for (PanelId p : V().modals) sim_.Submit(CmdClosePanel{p});
      }
      Frame();
    }
    if (!V().modals.empty()) Fail("panel did not close");
    return V().modals.empty();
  }

  int32_t WareIndex(const std::string& baseId) const {
    if (V().shop == nullptr) return -1;
    for (size_t i = 0; i < V().shop->wares.size(); ++i) {
      if (V().shop->wares[i].baseId == baseId) return static_cast<int32_t>(i);
    }
    return -1;
  }
  int64_t WarePrice(int32_t index) const { return V().shop->wares[static_cast<size_t>(index)].price; }

  // Sells spare gear and tops up potions at a shop NPC.
  bool Shop(const std::string& npcId, int hpTarget, int mpTarget) {
    if (!TalkTo(npcId)) return false;
    PanelSession session(*this);
    if (V().shop == nullptr || !V().shop->open) {
      Fail(npcId + ": shop did not open");
      return false;
    }
    // sell bag equipment that is not an upgrade
    std::vector<std::string> sell;
    for (const ItemInstance& it : V().inventory->Bag()) {
      const ItemBaseDef* base = data_.FindItemBase(it.baseId);
      if (base != nullptr && base->hasSlot && UpgradeGain(it) <= 0.5) sell.push_back(it.uid);
    }
    for (const std::string& uid : sell) {
      const int64_t gold = V().hero.gold;
      sim_.Submit(CmdSell{uid});
      Commit();
      if (V().hero.gold > gold) ++itemsSold;
    }
    const auto buy = [&](const std::string& baseId, int target) {
      const int32_t ware = WareIndex(baseId);
      if (ware < 0) return;
      for (int guard = 0; guard < 30 && CountOf(baseId) < target && V().hero.gold >= WarePrice(ware); ++guard) {
        const int32_t before = CountOf(baseId);
        sim_.Submit(CmdBuy{ware});
        Commit();
        if (CountOf(baseId) <= before) break;
        ++itemsBought;
      }
    };
    buy(V().hero.level >= 10 ? "c_hp_potion_m" : "c_hp_potion_s", hpTarget);
    if (mpTarget > 0) buy("c_mp_potion_s", mpTarget);
    Note("shopped at " + npcId + ": hp potions " + std::to_string(V().potionSlots[0].count) + ", gold " +
         std::to_string(V().hero.gold));
    return ClosePanels();
  }

  int HpPotionTarget() const { return 8; }
  int MpPotionTarget() const { return cls_ == ClassId::Mage ? 6 : 2; }

  // Picks the reward choice that improves the gear most.
  int32_t BestChoice(const std::vector<ItemInstance>& choices) const {
    int32_t best = 0;
    double bestGain = -1e9;
    for (size_t i = 0; i < choices.size(); ++i) {
      const ItemInstance& it = choices[i];
      double g = Score(&it);
      if (V().inventory != nullptr) {
        const std::optional<CompareTarget> t = FindCompareTarget(data_, it, V().inventory->Equipment());
        if (t.has_value()) g -= Score(t->equipped);
      }
      if (g > bestGain) {
        bestGain = g;
        best = static_cast<int32_t>(i);
      }
    }
    return best;
  }

  // Talks to a quest giver and works its card: turns in everything completed (best reward choice), then accepts the
  // offers in `accept` (all offers when empty). Re-opens the card after each action like the T17 chain offer (900 ms,
  // or after the turn-in cutscene + 300 ms).
  bool VisitGiver(const std::string& npcId, const std::vector<std::string>& accept) {
    if (!TalkTo(npcId)) return false;
    PanelSession session(*this);
    for (int round = 0; round < 16 && ok; ++round) {
      const QuestCardState* card = V().questCard;
      if (card == nullptr || !card->open) {
        // nothing to offer: a tree / linear dialogue opened instead
        return ClosePanels();
      }
      bool acted = false;
      for (const QuestCardEntry& e : card->entries) {
        if (e.quest == nullptr || !e.turnIn) continue;
        const std::string id = e.quest->id;
        const int32_t choice = BestChoice(e.choices);
        Note("turn in " + id + (e.choices.empty() ? "" : " choice " + std::to_string(choice)));
        sim_.Submit(CmdQuestTurnIn{id, choice});
        Commit();
        if (!HasStatus(id, QuestStatus::TurnedIn)) {
          Fail("turn-in refused: " + id);
          return false;
        }
        acted = true;
        break;
      }
      if (!acted) {
        for (const QuestCardEntry& e : card->entries) {
          if (e.quest == nullptr || e.turnIn) continue;
          const std::string id = e.quest->id;
          if (!accept.empty() && std::find(accept.begin(), accept.end(), id) == accept.end()) continue;
          Note("accept " + id);
          sim_.Submit(CmdQuestAccept{id});
          Commit();
          if (!HasStatus(id, QuestStatus::Active) && !QuestDone(id)) {
            Fail("accept refused: " + id);
            return false;
          }
          acted = true;
          break;
        }
      }
      if (!acted) return ClosePanels();
      // T17: reopen the card after 900 ms, or after the turn-in cutscene (+300 ms).
      Wait(0.9);
      if (V().storyBusy) {
        WaitIdle();
        Wait(0.3);
      }
      if (!WaitIdle()) return false;
      if (Dist(HeroPos(), FindNpc(npcId)->pos) > 3.0) return TalkTo(npcId) && VisitGiver(npcId, accept);
      sim_.Submit(CmdQuestCardOpen{npcId});
      Commit();
      if (V().questCard == nullptr || !V().questCard->open) return true;  // nothing more to offer
    }
    return ClosePanels();
  }

  // The elder's dialogue tree (quests 10.3) from the quest card's "view story" button: root -> reward_ask -> help
  // (+30 gold / +50 exp, Q1 one-time) -> other_quests -> accept_gel [Q:q_collect_slime_gel] -> Leave.
  bool ElderTree() {
    if (!TalkTo("quest_elder")) return false;
    PanelSession session(*this);
    sim_.Submit(CmdDialogueOpenTree{"quest_elder"});
    Commit();
    const char* path[] = {"reward_ask", "help", "other_quests", "accept_gel"};
    for (const char* next : path) {
      const DialogueView* d = V().dialogue;
      if (d == nullptr || !d->open || d->tree == nullptr) {
        Fail("elder dialogue tree not open");
        return false;
      }
      const DialogueNode* node = d->tree->FindNode(d->nodeId);
      int32_t button = -1;
      for (size_t b = 0; b < d->buttons.size() && node != nullptr; ++b) {
        const DialogueButton& btn = d->buttons[b];
        if (btn.kind == DialogueButtonKind::Choice && btn.choiceIndex >= 0 &&
            static_cast<size_t>(btn.choiceIndex) < node->choices.size() &&
            node->choices[static_cast<size_t>(btn.choiceIndex)].nextNodeId == next) {
          button = static_cast<int32_t>(b);
        }
      }
      if (button < 0) {
        Fail(std::string("elder tree: no button to ") + next + " at " + d->nodeId);
        return false;
      }
      Wait(0.5);  // reading
      sim_.Submit(CmdDialogueChoose{button});
      Commit();
    }
    const DialogueView* d = V().dialogue;
    if (d == nullptr || !d->open || d->nodeId != "accept_gel") {
      Fail("elder tree did not reach accept_gel");
      return false;
    }
    int32_t leave = -1;
    for (size_t b = 0; b < d->buttons.size(); ++b) {
      if (d->buttons[b].kind == DialogueButtonKind::Leave) leave = static_cast<int32_t>(b);
    }
    if (leave < 0) {
      Fail("elder tree: no Leave button at accept_gel");
      return false;
    }
    sim_.Submit(CmdDialogueChoose{leave});
    Commit();
    return ClosePanels();
  }

  // ---- quest helpers ----
  // Walks onto each gather node of `questId` until the objective is full.
  bool Gather(const std::string& questId, size_t obj, int32_t required) {
    for (int guard = 0; guard < 40 && ok && ObjCurrent(questId, obj) < required; ++guard) {
      Vec2 best;
      double bestD = 1e9;
      for (const WorldMarkerView& m : V().markers) {
        if (m.kind != MarkerKind::GatherNode) continue;
        const double d = Dist(m.pos, HeroPos());
        if (d < bestD) {
          bestD = d;
          best = m.pos;
        }
      }
      if (bestD > 1e8) {
        Fail("no gather node left for " + questId);
        return false;
      }
      GoTo(best, 0.6, 120);
      Wait(0.2);
    }
    return ok && ObjCurrent(questId, obj) >= required;
  }

  // Examines the clue marks of `questId` in objective order.
  bool Clues(const std::string& questId, const std::vector<Vec2>& tiles) {
    const QuestDef* q = data_.FindQuest(questId);
    if (q == nullptr) return false;
    size_t clue = 0;
    for (size_t i = 0; i < q->objectives.size() && ok; ++i) {
      if (q->objectives[i].type != ObjectiveType::InvestigateClue) continue;
      if (clue >= tiles.size()) break;
      const Vec2 tile = tiles[clue++];
      if (ObjCurrent(questId, i) >= q->objectives[i].required) continue;
      Vec2 mark = tile;
      for (const WorldMarkerView& m : V().markers) {
        if (m.kind == MarkerKind::Clue && m.key == questId && Dist(m.pos, tile) < 3) mark = m.pos;
      }
      GoTo(mark, 1.0, 300);
      Wait(0.3);
      if (ObjCurrent(questId, i) < q->objectives[i].required) {
        Fail("clue not taken: " + q->objectives[i].targetId);
        return false;
      }
      Note("clue " + q->objectives[i].targetId);
    }
    return ok;
  }

  // Waits within an explore circle until the objective counts (500 ms observer).
  bool Explore(const std::string& questId, size_t obj, Vec2 centre) {
    if (!GoTo(centre, 2.5, 300)) return false;
    for (int i = 0; i < 3 * kPtFps && ok && ObjCurrent(questId, obj) < 1; ++i) Frame();
    if (ObjCurrent(questId, obj) < 1) {
      Fail("explore objective did not count: " + questId);
      return false;
    }
    Note("explored " + questId + "#" + std::to_string(obj));
    return true;
  }

  bool TownTrip() {
    if (!Shop("merchant", HpPotionTarget(), MpPotionTarget())) return false;
    EquipUpgrades();
    return ok;
  }

  // Town portal home (W3): 1.5 s channel to the nearest camp.
  bool PortalHome() {
    if (!WaitIdle()) return false;
    for (int attempt = 0; attempt < 4 && ok; ++attempt) {
      while (FightThreats(kPtThreatRadius) && ok) {
      }
      sim_.Submit(CmdStop{});
      Commit();
      const Vec2 before = HeroPos();
      sim_.Submit(CmdTownPortal{});
      for (int i = 0; i < 4 * kPtFps && ok; ++i) {
        Frame();
        if (Dist(HeroPos(), before) > 10) {
          Note("portal home");
          return true;
        }
      }
    }
    Note("portal failed, walking");
    return false;
  }

  // ---- escort (quests 3.9) ----
  bool Escort(const std::string& questId) {
    const QuestDef* q = data_.FindQuest(questId);
    const Vec2 start = q->escortNpc.start.Center();
    const Vec2 dest = q->escortNpc.dest.Center();
    if (!GoTo(start, 3.0, 300)) return false;
    const int64_t end = frames + 600 * kPtFps;
    int64_t lastOrder = -1000;
    while (ok && frames < end) {
      if (QuestDone(questId)) {
        Note("escort complete");
        return true;
      }
      if (HasStatus(questId, QuestStatus::Failed)) {
        Note("escort failed (the merchant died)");  // reacceptable: the caller takes it again from the elder
        ++escortFailures;
        return false;
      }
      if (!Alive()) {
        WaitRespawn();
        continue;
      }
      if (Busy()) {
        Frame();
        continue;
      }
      const EscortState* e = V().escort;
      if (e == nullptr || !e->active) {
        Fail("no escort NPC out");
        return false;
      }
      // protect: anything near the merchant or us
      EntityId threat = NearestThreat(kPtThreatRadius);
      if (threat == kNoEntity) {
        for (const MonsterView& m : V().monsters) {
          if (m.alive && Dist(m.pos, e->pos) < 4.5 && m.state != MonsterState::Returning &&
              m.state != MonsterState::Idle && m.state != MonsterState::Patrol) {
            threat = m.id;
          }
        }
      }
      if (threat != kNoEntity) {
        Engage(threat, 30);
        lastOrder = -1000;
        continue;
      }
      const double gap = Dist(HeroPos(), e->pos);
      if (!e->joined) {
        if (gap > 4) {
          sim_.Submit(CmdMoveTo{e->pos});
          Wait(0.5);
        } else {
          Frame();
        }
        continue;
      }
      // The merchant re-paths to us 2.5x a second and stops a tile short (its path drops the first and last step,
      // quests 3.9), so it can rest ~3.5 tiles away while we stand: wait only when it really lags.
      if (gap > 5.0) {
        if (V().hero.moving) sim_.Submit(CmdStop{});
        Frame();
        continue;
      }
      if (!V().hero.moving && frames - lastOrder > 20) {
        sim_.Submit(CmdMoveTo{dest});
        lastOrder = frames;
      }
      Frame();
    }
    if (ok) Fail("escort timeout");
    return false;
  }

 private:
  void AutoUi() {
    const Snapshot& v = V();
    if (v.story != nullptr && v.story->playing) {
      ++storyFrames_;
      if (v.story->beat.id == "prologue") {
        if (storyFrames_ % 10 == 1) sim_.Submit(CmdStorySkip{});
      } else if (storyFrames_ % 15 == 0) {
        sim_.Submit(CmdStoryAdvance{});
      }
    } else {
      storyFrames_ = 0;
    }
    if (v.miniBossDialogue) {
      if (++miniBossFrames_ % 40 == 0) sim_.Submit(CmdMiniBossDialogueDismiss{});
    } else {
      miniBossFrames_ = 0;
    }
    if (v.loreText != nullptr && v.loreText->open) {
      if (++loreFrames_ % 40 == 0) sim_.Submit(CmdClosePanel{PanelId::LoreText});
    } else {
      loreFrames_ = 0;
    }
    if (v.puzzle != nullptr && v.puzzle->open && (frames % 30) == 0) {
      sim_.Submit(CmdPuzzleAnswer{v.puzzle->prop, kPuzzleChoiceSolve});
    }
    // A panel the bot did not open (the wandering merchant's shop opens on its own, world 13): look, then close it.
    if (panelSession_ == 0 && !v.storyBusy && !v.modals.empty()) {
      if (++strayFrames_ % 45 == 0) {
        for (PanelId p : v.modals) {
          if (p != PanelId::MiniBossDialogue && p != PanelId::LoreText && p != PanelId::Puzzle) {
            sim_.Submit(CmdClosePanel{p});
            Note(std::string("closing stray panel ") + std::string(EnumName(p)));
          }
        }
      }
    } else {
      strayFrames_ = 0;
    }
  }

  PrimaryStat MainStat() const {
    switch (cls_) {
      case ClassId::Warrior: return PrimaryStat::Str;
      case ClassId::Mage: return PrimaryStat::Int;
      case ClassId::Rogue: return PrimaryStat::Dex;
    }
    return PrimaryStat::Vit;
  }

  std::vector<std::string> SkillPriority() const {
    switch (cls_) {
      case ClassId::Warrior: return {"whirlwind", "slash"};
      case ClassId::Mage: return {"fire_wall", "fireball"};
      case ClassId::Rogue: return {"piercing_arrow", "multishot"};
    }
    return {};
  }

  void AutoHero() {
    const Snapshot& v = V();
    const HeroView& h = v.hero;
    if (h.life != HeroLife::Alive) return;
    if (!v.frozen && !v.inputBlocked && h.hp > 0) {
      if (h.hp < 0.35 * h.maxHp && v.potionSlots[0].count > 0 && Now() - lastHpPotionMs_ >= 900) {
        sim_.Submit(CmdUsePotion{PotionSlot::Hp});
        lastHpPotionMs_ = Now();
        ++potionsDrunk;
      }
      if (h.mana < 0.2 * h.maxMana && v.potionSlots[1].count > 0 && Now() - lastMpPotionMs_ >= 900 && h.inCombat) {
        sim_.Submit(CmdUsePotion{PotionSlot::Mp});
        lastMpPotionMs_ = Now();
        ++potionsDrunk;
      }
    }
    if (h.freeStatPoints > 0 && allocPending_ == h.freeStatPoints && frames - allocPendingAt_ > 30) allocPending_ = -1;
    if (h.freeSkillPoints > 0 && learnPending_ == h.freeSkillPoints && frames - learnPendingAt_ > 30) {
      learnPending_ = -1;
    }
    if (h.freeStatPoints > 0 && allocPending_ != h.freeStatPoints) {
      static constexpr int kPattern[5] = {0, 0, 1, 0, 1};  // 0 main, 1 vit
      const PrimaryStat s = kPattern[allocated_ % 5] == 0 ? MainStat() : PrimaryStat::Vit;
      sim_.Submit(CmdAllocStat{s, 1});
      allocPending_ = h.freeStatPoints;
      allocPendingAt_ = frames;
      ++allocated_;
    } else if (h.freeStatPoints == 0) {
      allocPending_ = -1;
    }
    if (h.freeSkillPoints > 0 && v.skills != nullptr && learnPending_ != h.freeSkillPoints) {
      for (const std::string& id : SkillPriority()) {
        const int32_t idx = v.skills->IndexOf(id);
        if (idx < 0) continue;
        if (!v.skills->GetInvestState(idx, h.level, h.freeSkillPoints).canInvest) continue;
        sim_.Submit(CmdLearnSkill{idx});
        learnPending_ = h.freeSkillPoints;
        learnPendingAt_ = frames;
        Note("learn " + id);
        break;
      }
    } else if (h.freeSkillPoints == 0) {
      learnPending_ = -1;
    }
  }

  void ProcessEvents() {
    for (const Event& e : sim_.Events()) {
      if (const EvStoryBeat* b = std::get_if<EvStoryBeat>(&e)) {
        if (b->phase == EvStoryBeat::Phase::Began) {
          beatsBegan.push_back(b->beatId);
          Note("story begins " + b->beatId);
        } else {
          beatsEnded.push_back(b->beatId);
          Note("story ends " + b->beatId);
        }
      } else if (const EvLevelUp* l = std::get_if<EvLevelUp>(&e)) {
        ++levelUps;
        Note("level up " + std::to_string(l->level));
      } else if (std::holds_alternative<EvHeroDied>(e)) {
        ++deaths;
        Note("hero died");
      } else if (const EvExpGained* x = std::get_if<EvExpGained>(&e)) {
        expBySource[static_cast<size_t>(x->source)] += x->amount;
      } else if (std::holds_alternative<EvItemPicked>(e)) {
        ++itemsPicked;
      } else if (std::holds_alternative<EvEquipmentChanged>(e)) {
        ++equipChanges;
      } else if (const EvQuestUpdate* q = std::get_if<EvQuestUpdate>(&e)) {
        static const char* kKinds[] = {"accepted", "progress", "completed", "turned in", "failed", "tracked"};
        if (q->kind != EvQuestUpdate::Kind::Tracked) {
          Note(std::string("quest ") + kKinds[static_cast<int>(q->kind)] + " " + q->questId +
               (q->kind == EvQuestUpdate::Kind::Progress
                    ? " #" + std::to_string(q->objectiveIndex) + " " + std::to_string(q->current) + "/" +
                          std::to_string(q->required)
                    : std::string()));
        }
      } else if (const EvBanner* bn = std::get_if<EvBanner>(&e)) {
        if (bn->kind == BannerKind::ComingSoon) ++sealedBanners;
      } else if (const EvPet* p = std::get_if<EvPet>(&e)) {
        if (p->kind == EvPet::Kind::Obtained) {
          petObtained = true;
          Note("pet obtained " + p->petId);
        }
      } else if (const EvHit* h = std::get_if<EvHit>(&e)) {
        if (h->killed && h->targetFaction == Faction::Monster) {
          ++kills;
          for (const auto& [id, def] : monsterDefs_) {
            if (id == h->target) {
              killedDefs.insert(def);
              if (def.rfind("hunt_", 0) == 0 || def.rfind("miniboss_", 0) == 0 || def == "goblin_chief") {
                Note("killed " + def);
              }
            }
          }
        }
      } else if (std::holds_alternative<EvSaveRequested>(e)) {
        ++saveRequests;
      } else if (const EvMiniBossDialogue* mb = std::get_if<EvMiniBossDialogue>(&e)) {
        if (mb->opened) {
          ++miniBossDialogues;
          Note("mini-boss dialogue " + mb->monsterId);
        }
      }
    }
  }

  GameSim& sim_;
  const DataStore& data_;
  ClassId cls_;
  bool verbose_;
  std::deque<std::string> recent_;
  int64_t storyFrames_ = 0, miniBossFrames_ = 0, loreFrames_ = 0, strayFrames_ = 0;
  int panelSession_ = 0;
  bool feedRefused_ = false;  // > 0 while the bot works a panel it opened itself
  double lastHpPotionMs_ = -1e9, lastMpPotionMs_ = -1e9;
  int32_t allocated_ = 0, allocPending_ = -1, learnPending_ = -1;
  std::set<EntityId> skippedLoot_;
  std::vector<std::pair<EntityId, std::string>> monsterDefs_;
  int32_t lastSteps_ = 0;
  bool lastFrozen_ = false;
  int64_t allocPendingAt_ = 0, learnPendingAt_ = 0;  // the monsters before the frame (kill attribution)
};

// ---------------------------------------------------------------------------------------------------------------------
// The script
// ---------------------------------------------------------------------------------------------------------------------
struct PtResult {
  bool ok = false;
  std::string failure;
  std::string recent;
  std::string finalSave;  // SaveGame(kPtSaveMs) after the finale
};

constexpr int64_t kPtSaveMs = 1760000000000;

const std::vector<std::string> kPtAllCh1Quests = {
    "q_kill_slimes",       "q_kill_goblins",   "q_explore_goblin_camp",    "q_find_goblin_chief",
    "q_secure_plains",     "q_collect_slime_gel", "q_herb_gathering",      "q_lost_pendant",
    "q_bandit_trouble",    "q_pet_sprite_friend", "q_escort_merchant_plains"};

bool PtContains(const std::vector<std::string>& v, const std::string& s) {
  return std::find(v.begin(), v.end(), s) != v.end();
}

// Plays chapter 1 to the finale (no save/load). Checkpoints are CHECKed along the way.
void PtPlayChapter1(PtBot& bot) {
  // ---- new game: prologue (skipped) + chapter card ----
  CHECK(bot.V().storyBusy);
  bot.WaitIdle(180);
  CHECK(PtContains(bot.beatsEnded, "prologue"));
  CHECK(PtContains(bot.beatsEnded, "chapter_emerald_plains"));
  CHECK(bot.V().zone.mapId == "emerald_plains");
  if (!bot.ok) return;

  // ---- the camp ----
  bot.Note("== camp");
  if (bot.TalkTo("blacksmith")) {
    CHECK(bot.V().shop != nullptr);
    CHECK(bot.V().shop->blacksmith);
    bot.ClosePanels();
  }
  if (bot.TalkTo("stash")) {
    CHECK(bot.V().stash != nullptr);
    CHECK(bot.V().stash->open);
    bot.ClosePanels();
  }
  bot.ElderTree();
  CHECK(bot.HasStatus("q_collect_slime_gel", QuestStatus::Active));
  CHECK(bot.V().hero.gold == 30);  // reward_ask supplies (Q1, one-time)
  bot.VisitGiver("quest_elder", {"q_kill_slimes", "q_herb_gathering", "q_pet_sprite_friend"});
  CHECK(bot.HasStatus("q_kill_slimes", QuestStatus::Active));
  CHECK(bot.HasStatus("q_herb_gathering", QuestStatus::Active));
  CHECK(bot.HasStatus("q_pet_sprite_friend", QuestStatus::Active));
  bot.Shop("merchant", 2, 0);
  if (!bot.ok) return;

  // ---- herbs, slimes, gel ----
  bot.Note("== herbs");
  bot.Gather("q_herb_gathering", 0, 5);
  CHECK(bot.HasStatus("q_herb_gathering", QuestStatus::Completed));
  bot.Note("== slimes");
  bot.Hunt({"slime_green"}, Vec2(25, 20), [&] {
    return bot.QuestDone("q_kill_slimes") && bot.ObjCurrent("q_collect_slime_gel", 0) >= 6;
  });
  CHECK(bot.HasStatus("q_kill_slimes", QuestStatus::Completed));
  CHECK(bot.ObjCurrent("q_collect_slime_gel", 0) == 6);
  if (!bot.ok) return;

  // ---- turn in M1 + herbs (cs_ep_mark), take M2 + the pendant ----
  bot.Note("== elder: M1");
  bot.VisitGiver("quest_elder", {"q_kill_goblins", "q_lost_pendant"});
  CHECK(bot.HasStatus("q_kill_slimes", QuestStatus::TurnedIn));
  CHECK(bot.HasStatus("q_herb_gathering", QuestStatus::TurnedIn));
  bot.WaitIdle();
  CHECK(PtContains(bot.beatsEnded, "cs_ep_mark"));
  CHECK(bot.HasStatus("q_kill_goblins", QuestStatus::Active));
  CHECK(bot.HasStatus("q_lost_pendant", QuestStatus::Active));
  bot.TownTrip();
  if (!bot.ok) return;

  // ---- pendant clues 1-2, the herbalist (gel delivery), clue 3, the thief ----
  bot.Note("== pendant + herbalist");
  bot.Clues("q_lost_pendant", {Vec2(30, 24), Vec2(42, 34)});
  bot.Shop("plains_herbalist", bot.HpPotionTarget(), bot.MpPotionTarget());
  CHECK(bot.HasStatus("q_collect_slime_gel", QuestStatus::Completed));
  bot.Clues("q_lost_pendant", {Vec2(30, 24), Vec2(42, 34), Vec2(54, 44)});
  bot.Hunt({"hunt_pendant_thief"}, Vec2(60, 48), [&] { return bot.QuestDone("q_lost_pendant"); }, 300);
  CHECK(bot.HasStatus("q_lost_pendant", QuestStatus::Completed));
  if (!bot.ok) return;

  // ---- goblins ----
  bot.Note("== goblins");
  bot.Hunt({"goblin"}, Vec2(35, 40), [&] { return bot.QuestDone("q_kill_goblins"); });
  CHECK(bot.HasStatus("q_kill_goblins", QuestStatus::Completed));
  if (!bot.ok) return;

  // ---- the sprite's hide-and-seek ----
  bot.Note("== sprite");
  bot.Clues("q_pet_sprite_friend", {Vec2(78, 12), Vec2(92, 10), Vec2(100, 22), Vec2(86, 28)});
  CHECK(bot.HasStatus("q_pet_sprite_friend", QuestStatus::Completed));
  if (!bot.ok) return;

  // ---- home: gel, M2, pendant, sprite (pet) ----
  bot.Note("== elder: M2");
  bot.PortalHome();
  bot.VisitGiver("quest_elder", {"q_explore_goblin_camp", "q_escort_merchant_plains"});
  CHECK(bot.HasStatus("q_collect_slime_gel", QuestStatus::TurnedIn));
  CHECK(bot.HasStatus("q_kill_goblins", QuestStatus::TurnedIn));
  CHECK(bot.HasStatus("q_lost_pendant", QuestStatus::TurnedIn));
  CHECK(bot.HasStatus("q_pet_sprite_friend", QuestStatus::TurnedIn));
  CHECK(bot.HasStatus("q_explore_goblin_camp", QuestStatus::Active));
  CHECK(bot.HasStatus("q_escort_merchant_plains", QuestStatus::Active));
  bot.WaitIdle();
  CHECK(bot.petObtained);
  CHECK(bot.V().pet.present);
  CHECK(bot.V().pet.petId == "pet_sprite");
  bot.TownTrip();
  if (!bot.ok) return;

  // ---- M3: the goblin camp; escort the merchant south ----
  bot.Note("== goblin camp + escort");
  bot.Explore("q_explore_goblin_camp", 0, Vec2(40, 52));
  CHECK(bot.HasStatus("q_explore_goblin_camp", QuestStatus::Completed));
  for (int attempt = 0; attempt < 3 && bot.ok && !bot.Escort("q_escort_merchant_plains"); ++attempt) {
    if (bot.ok) bot.VisitGiver("quest_elder", {"q_escort_merchant_plains"});  // failed -> offered again (3.9)
  }
  CHECK(bot.HasStatus("q_escort_merchant_plains", QuestStatus::Completed));
  if (!bot.ok) return;

  // ---- the wanderer's bounty ----
  bot.Note("== bounty");
  bot.VisitGiver("plains_wanderer", {"q_bandit_trouble"});
  CHECK(bot.HasStatus("q_bandit_trouble", QuestStatus::Active));
  bot.Hunt({"hunt_redcap_gruk"}, Vec2(82, 72), [&] { return bot.QuestDone("q_bandit_trouble"); }, 400);
  CHECK(bot.HasStatus("q_bandit_trouble", QuestStatus::Completed));
  bot.VisitGiver("plains_wanderer", {});
  CHECK(bot.HasStatus("q_bandit_trouble", QuestStatus::TurnedIn));
  if (!bot.ok) return;

  // ---- the zone mini-boss: goblin shaman ----
  bot.Note("== mini-boss");
  bot.GoTo(Vec2(60, 55), 8, 300);
  bool shamanDead = false;
  bot.Hunt({"miniboss_goblin_shaman"}, Vec2(60, 55), [&] {
    bool alive = false;
    for (const MonsterView& m : bot.V().monsters) alive = alive || (m.miniBoss && m.alive);
    shamanDead = !alive;
    return shamanDead;
  }, 300);
  CHECK(shamanDead);
  CHECK(bot.miniBossDialogues >= 1);
  if (!bot.ok) return;

  // ---- home: M3 (cs_ep_whisper), escort; take M4 ----
  bot.Note("== elder: M3");
  bot.PortalHome();
  bot.GoTo(Vec2(15, 18), 4, 400);
  bot.VisitGiver("quest_elder", {"q_find_goblin_chief"});
  CHECK(bot.HasStatus("q_explore_goblin_camp", QuestStatus::TurnedIn));
  CHECK(bot.HasStatus("q_escort_merchant_plains", QuestStatus::TurnedIn));
  CHECK(bot.HasStatus("q_find_goblin_chief", QuestStatus::Active));
  bot.WaitIdle();
  CHECK(PtContains(bot.beatsEnded, "cs_ep_whisper"));
  bot.TownTrip();
  if (!bot.ok) return;

  // ---- M4: the goblin chief (boss intro) ----
  bot.Note("== chief");
  bot.GoTo(Vec2(18, 88), 3, 400);
  bot.Hunt({"goblin_chief"}, Vec2(15, 95), [&] { return bot.QuestDone("q_find_goblin_chief"); }, 400);
  CHECK(bot.HasStatus("q_find_goblin_chief", QuestStatus::Completed));
  CHECK(PtContains(bot.beatsEnded, "boss_goblin_chief"));
  if (!bot.ok) return;

  // ---- home: M4 (weapon / armour choice); take M5 ----
  bot.Note("== elder: M4");
  bot.PortalHome();
  bot.VisitGiver("quest_elder", {"q_secure_plains"});
  CHECK(bot.HasStatus("q_find_goblin_chief", QuestStatus::TurnedIn));
  CHECK(bot.HasStatus("q_secure_plains", QuestStatus::Active));
  bot.TownTrip();
  if (!bot.ok) return;

  // ---- M5: the seal of veins ----
  bot.Note("== M5");
  bot.Explore("q_secure_plains", 0, Vec2(65, 30));
  bot.Explore("q_secure_plains", 1, Vec2(50, 55));
  CHECK(bot.HasStatus("q_secure_plains", QuestStatus::Completed));
  bot.PortalHome();
  bot.VisitGiver("quest_elder", {});
  CHECK(bot.HasStatus("q_secure_plains", QuestStatus::TurnedIn));
  bot.WaitIdle();
  CHECK(PtContains(bot.beatsEnded, "cs_ep_finale"));
  bot.EquipUpgrades();
}

void PtCheckChapterDone(PtBot& bot) {
  const Snapshot& v = bot.V();
  for (const std::string& q : kPtAllCh1Quests) {
    INFO("quest " << q);
    CHECK(bot.HasStatus(q, QuestStatus::TurnedIn));
  }
  for (const char* beat : {"prologue", "chapter_emerald_plains", "cs_ep_mark", "cs_ep_whisper", "boss_goblin_chief",
                           "cs_ep_finale"}) {
    INFO("beat " << beat);
    CHECK(PtContains(bot.beatsEnded, beat));
    CHECK(v.story != nullptr);
  }
  // Exp ledger: the level + exp into it account for exactly the exp granted (classes 5.1 curve, 5.2 one level per
  // grant with the overflow kept), and quest rewards pay the chapter's 2660 (quests 10.1 totals).
  int64_t granted = 0;
  for (int64_t x : bot.expBySource) granted += x;
  int64_t cumulative = 0;
  for (int32_t l = 1; l < v.hero.level; ++l) cumulative += static_cast<int64_t>(std::floor(3.0 * l * l + 25.0 * l));
  CHECK(cumulative + v.hero.exp == granted);
  CHECK(bot.expBySource[static_cast<size_t>(ExpSource::Quest)] == 2660);
  CHECK(bot.expBySource[static_cast<size_t>(ExpSource::DialogueReward)] == 50);  // reward_ask, once (Q1)
  CHECK(bot.expBySource[static_cast<size_t>(ExpSource::DeathPenalty)] == 0);     // Normal: no exp toll
  // Level band. The chapter's quest rewards alone are 2660 exp (Lv 11 starts at 2530) and the kills every quest needs
  // (10 slimes, 15 goblins, the pendant thief, Gruk, the chief) add >= 589 more, so a complete chapter 1 ends at Lv 12
  // or above with the web's numbers (1-10 is the zone's monster band, not where the chapter ends); a bot that also
  // fights what aggroes on the way lands on 12-14.
  INFO("level " << v.hero.level);
  CHECK(v.hero.level >= 12);
  CHECK(v.hero.level <= 14);
  CHECK(v.hero.gold > 0);
  CHECK(v.hero.gold < 100000);
  CHECK(v.hero.freeStatPoints == 0);
  CHECK(bot.itemsPicked > 0);
  CHECK(bot.equipChanges > 0);
  CHECK(v.inventory != nullptr);
  int equipped = 0;
  for (const auto& slot : v.inventory->Equipment()) equipped += slot.has_value() ? 1 : 0;
  CHECK(equipped >= 3);  // drops vary; the pick-one rewards alone fill a weapon / armour and jewellery slot
  CHECK(v.inventory->Bag().size() < 100);
  CHECK(v.pet.present);
  REQUIRE(v.pets != nullptr);
  const PetInstance* sprite = v.pets->Find("pet_sprite");
  REQUIRE(sprite != nullptr);
  CHECK(v.pets->ActiveId() == "pet_sprite");
  CHECK(sprite->level > 1);  // beast exp from every kill since the turn-in (18.3 onKill)
  // the elite kills the chapter requires (the chief and both hunt leaders) and the zone mini-boss
  for (const char* boss : {"goblin_chief", "miniboss_goblin_shaman", "hunt_pendant_thief", "hunt_redcap_gruk"}) {
    INFO("killed " << boss);
    CHECK(bot.killedDefs.count(boss) == 1);
  }
  // embers (quests 4.4): 20 from the eleven turn-ins + 5 per elite / mini-boss / affixed kill (>= the four above)
  REQUIRE(v.homestead != nullptr);
  CHECK(v.homestead->Embers() >= 40);
  CHECK((v.homestead->Embers() - 20) % 5 == 0);
  CHECK(v.homestead->IsBuildingUnlocked("herb_garden"));  // q_secure_plains (4.6)
  // achievements a straight chapter-1 run earns (quests 10.5)
  REQUIRE(v.achievements != nullptr);
  for (const char* ach : {"ach_first_kill", "ach_kill_goblin_chief", "ach_quest_10", "ach_level_10"}) {
    INFO("achievement " << ach);
    CHECK(v.achievements->IsUnlocked(ach));
  }
  CHECK(bot.deaths <= 2);
  CHECK(bot.levelUps >= 11);
  CHECK(bot.kills >= 40);
  CHECK(bot.saveRequests > 0);
  CHECK_FALSE(v.frozen);
  CHECK(v.modals.empty());
  CHECK(v.hero.life == HeroLife::Alive);
}

// Save -> load into a fresh GameSim (identical SaveGame) -> walk to the sealed chapter-2 gate.
void PtSaveLoadAndGate(PtBot& bot, ClassId cls) {
  bot.WaitIdle();
  REQUIRE(bot.Sim().CanSave());
  const std::string save = bot.Sim().SaveGame(kPtSaveMs);
  auto fresh = GameSim::Create(test::RealData(), SimConfig{});
  REQUIRE(fresh != nullptr);
  std::string err;
  REQUIRE(fresh->LoadGame(save, &err) == SaveError::None);
  const std::string again = fresh->SaveGame(kPtSaveMs);
  CHECK(again == save);
  if (again != save) {
    size_t i = 0;
    while (i < again.size() && i < save.size() && again[i] == save[i]) ++i;
    const size_t from = i > 120 ? i - 120 : 0;
    MESSAGE("first difference at " << i << "\n  saved:  " << save.substr(from, 260) << "\n  loaded: "
                                   << again.substr(from, 260));
  }
  CHECK(fresh->View().zone.mapId == "emerald_plains");
  CHECK(fresh->View().hero.level == bot.V().hero.level);

  PtBot cont(*fresh, cls);
  cont.Frame();
  cont.WaitIdle(60);
  CHECK(cont.V().quests->IsTurnedIn("q_secure_plains"));
  // the sealed gate to chapter 2 (W7): a coming-soon banner, no transition
  bool sealedMarker = false;
  for (const WorldMarkerView& m : cont.V().markers) {
    if (m.kind == MarkerKind::Exit && m.key == "twilight_forest") sealedMarker = m.sealed;
  }
  CHECK(sealedMarker);
  cont.GoTo(Vec2(118, 60), 0.4, 600);  // the exit's inner tile (the exit itself sits on the border wall)
  for (int i = 0; i < 3 * kPtFps && cont.sealedBanners == 0; ++i) cont.Frame();
  CHECK(cont.ok);
  if (!cont.ok) MESSAGE(cont.failure << "\n" << cont.Recent());
  CHECK(cont.sealedBanners >= 1);
  cont.Wait(2);
  CHECK(cont.V().zone.mapId == "emerald_plains");
}

PtResult PtRun(ClassId cls, uint64_t seed, bool saveLoad) {
  PtResult r;
  auto sim = GameSim::Create(test::RealData(), SimConfig{});
  REQUIRE(sim != nullptr);
  REQUIRE(sim->NewGame(cls, Difficulty::Normal, seed));
  PtBot bot(*sim, cls);
  PtPlayChapter1(bot);
  r.ok = bot.ok;
  r.failure = bot.failure;
  r.recent = bot.Recent();
  if (!bot.ok) {
    MESSAGE(std::string(PtClassName(cls)) << " seed " << seed << " playthrough failed: " << bot.failure << "\n"
                                          << bot.Recent());
    CHECK(bot.ok);
    return r;
  }
  PtCheckChapterDone(bot);
  MESSAGE(std::string(PtClassName(cls))
          << ": level " << bot.V().hero.level << ", gold " << bot.V().hero.gold << ", kills " << bot.kills
          << ", deaths " << bot.deaths << ", potions " << bot.potionsDrunk << ", picked " << bot.itemsPicked
          << ", sold " << bot.itemsSold << ", casts " << bot.skillCasts << ", play time "
          << sim->View().playTimeMs / 60000.0 << " min, exp kill " << bot.expBySource[0] << " quest "
          << bot.expBySource[1] << " dialogue " << bot.expBySource[2] << " event " << bot.expBySource[3]
          << ", pet feeds " << bot.petFeeds);
  r.finalSave = sim->SaveGame(kPtSaveMs);
  // ABYSS_PT_DUMP=<dir>: keep the final saves (e.g. to diff GCC and Clang builds, which must agree bit for bit).
  if (const char* dir = std::getenv("ABYSS_PT_DUMP")) {
    std::ofstream out(std::string(dir) + "/" + PtClassName(cls) + "_" + std::to_string(seed) + ".json",
                      std::ios::binary);
    out << r.finalSave;
  }
  if (saveLoad) PtSaveLoadAndGate(bot, cls);
  return r;
}

}  // namespace

TEST_SUITE("playthrough") {
  TEST_CASE("chapter 1 playthrough: warrior") { PtRun(ClassId::Warrior, 101, true); }
  TEST_CASE("chapter 1 playthrough: mage") { PtRun(ClassId::Mage, 202, true); }
  TEST_CASE("chapter 1 playthrough: rogue") { PtRun(ClassId::Rogue, 303, true); }
  // Stress sweep (off by default): ABYSS_PT_SWEEP=<n> plays n more seeds per class, ABYSS_PT_SWEEP_BASE the first seed.
  TEST_CASE("chapter 1 playthrough: seed sweep (ABYSS_PT_SWEEP)") {
    const char* n = std::getenv("ABYSS_PT_SWEEP");
    const char* base = std::getenv("ABYSS_PT_SWEEP_BASE");
    const int count = n != nullptr ? std::atoi(n) : 0;
    const uint64_t first = base != nullptr ? static_cast<uint64_t>(std::atoll(base)) : 1000;
    for (int i = 0; i < count; ++i) {
      for (ClassId cls : {ClassId::Warrior, ClassId::Mage, ClassId::Rogue}) {
        INFO(std::string(PtClassName(cls)) << " seed " << first + static_cast<uint64_t>(i));
        PtRun(cls, first + static_cast<uint64_t>(i), true);
      }
    }
  }
  TEST_CASE("chapter 1 playthrough is deterministic: same seed twice -> identical save JSON") {
    const PtResult a = PtRun(ClassId::Rogue, 404, false);
    const PtResult b = PtRun(ClassId::Rogue, 404, false);
    REQUIRE(a.ok);
    REQUIRE(b.ok);
    CHECK(a.finalSave == b.finalSave);
  }
}

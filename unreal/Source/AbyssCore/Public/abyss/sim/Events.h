// Core -> presentation events (ARCHITECTURE 3.1). Plain structs in one std::variant.
//
// * Events carry everything the UE layer needs to play animations, VFX, audio and UI feedback; UE never writes core
//   state. Continuous state (positions, HP, cooldowns, inventories) is read from the Snapshot instead.
// * All player-facing text is a LocText (i18n key + args); UE renders it with I18n::T.
// * Times: `*Ms` fields that are absolute are SimClock ms (sim time); durations are ms.
// * Subsystems emit through EventSink (appends in emission order; GameSim exposes the list after a step/frame).
#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/I18n.h"
#include "abyss/base/Platform.h"
#include "abyss/base/Types.h"
#include "abyss/data/AudioData.h"
#include "abyss/data/CombatData.h"
#include "abyss/data/ItemData.h"
#include "abyss/data/MapData.h"
#include "abyss/data/MonsterData.h"
#include "abyss/data/SkillData.h"
#include "abyss/data/StoryData.h"
#include "abyss/hero/Spirit.h"
#include "abyss/items/Item.h"
#include "abyss/sim/SimTypes.h"

namespace abyss {

// ---- entities ----
struct EvEntitySpawned {
  EntityId id = kNoEntity;
  EntityKind kind = EntityKind::None;
  std::string defId;   // monster def id / npc id / pet id / item base id / prop type
  std::string artId;   // asset game id (spriteKey, npc spriteId, ...)
  Vec2 pos;
  Vec2 facing{1, 0};
  double visualScale = 1.0;
};
struct EvEntityDespawned {
  EntityId id = kNoEntity;
  EntityKind kind = EntityKind::None;
  DespawnReason reason = DespawnReason::Removed;
};
struct EvEntityTeleported {
  EntityId id = kNoEntity;
  Vec2 from, to;
  TeleportReason reason = TeleportReason::Skill;
};
// Play an action on an entity. contactMs (attack) / releaseMs (cast) is relative to startMs; the montage should be
// played at a rate that puts its authored Contact/Release notify there (P5: the core owns the timing).
struct EvPlayAnim {
  EntityId entity = kNoEntity;
  AnimAction action = AnimAction::Idle;
  std::string clip;        // optional explicit clip (signature montages: skill id)
  double startMs = 0;      // sim time
  double contactMs = 0;    // attack contact / cast release offset (0 = none)
  double windupMs = 0;     // monster telegraph length (0.62 * contact)
  double durationMs = 0;   // whole action at play rate 1 scaled by speed
  double playRate = 1.0;
  bool hasFaceTarget = false;
  Vec2 faceTarget;
};
// A resolved hit (or miss). The HitFeedback profile is included (combat-feel.md 11) together with the RESOLVED 11.1
// decisions, so UE applies feel without re-implementing any rule:
//   attackerStopMs  hit-stop on the attacker: hero basic attack = profile.attackerStopMs; hero skills = 0; monster
//                   swing on the hero = round(profile.attackerStopMs x 0.6); DoT ticks / pets / environment = 0.
//   impactBurst     play VFXManager.impactBurst (colour impactColor): hero basic attacks and skills except death_mark,
//                   slow_trap and DoT ticks; never on misses.
//   melee           monster -> hero hit from a melee swing (the claw-rake VFX is melee only); false for bolts.
//   numberSlot      floating-number placement (Primary / DoubleStrike 20 px higher / DoubleShot offset +15, -15).
struct EvHit {
  EntityId target = kNoEntity;
  EntityId source = kNoEntity;
  Faction targetFaction = Faction::Monster;
  double amount = 0;
  HitWeight weight = HitWeight::Normal;
  HitProfileDef profile;
  bool crit = false;
  bool dodged = false;       // MISS
  bool tick = false;         // DoT tick
  bool killed = false;
  bool iframeAvoided = false;  // hero dodge-roll i-frames ("perfect evade")
  DamageType element = DamageType::Physical;
  uint32_t impactColor = 0;
  bool hasFrom = false;
  Vec2 from;
  double targetHp = 0, targetMaxHp = 0;
  std::string skillId;       // empty for basic attacks / monster hits
  double attackerStopMs = 0;
  bool impactBurst = false;
  bool melee = false;
  HitNumberSlot numberSlot = HitNumberSlot::Primary;
  // Ground scorch decal under the target (combat 6.4 #7, C6 SkillDef::scorch): landed single-target skill hits only.
  bool scorch = false;
  DamageType scorchType = DamageType::Fire;
};
// A pending monster contact that will not land: F2 (a cinematic cancels every contact and bolt), and for a K2 ability
// also its abort (S7: the caster died or was stunned during the windup): UE drops the caster's telegraph and stops the
// wind-up tint.
struct EvMonsterAttackCancelled {
  EntityId monster = kNoEntity;
  bool projectile = false;  // false = contact
};
// Dark remake K2 (implementation-plan.md 3 K2, 4 U2; DR5): a monster ability started its windup with a ground shape
// (circle / cone / line / ring / hazard). UE draws the decal at `origin` facing `dir` (crimson rim, bone fill that
// sweeps to full exactly at contactMs) in the style's variant; it ends at contactMs (EvHazard follows when the ability
// leaves ground fire) or on EvMonsterAttackCancelled for the entity. Times are absolute sim ms:
// fill = (now - startMs) / (contactMs - startMs). The monster's animation is the EvPlayAnim emitted just before it
// (action Attack / Attack02 / Cast / Cast02 / Roar, `clip` = the ability's clip, contactMs = the same beat): that
// EvPlayAnim is the plan's "EvMonsterAnim {entity, action}".
struct EvTelegraph {
  EntityId entity = kNoEntity;  // the caster
  std::string abilityId;
  AbilityKind kind = AbilityKind::Circle;
  AbilityShapeDef shape;        // kind + radius / arcDeg / length / width / innerRadius (tiles, degrees)
  Vec2 origin;
  Vec2 dir{1, 0};               // unit (cones and lines)
  double startMs = 0;
  double contactMs = 0;
  TelegraphStyle style = TelegraphStyle::Normal;
  // K3: an on-death fuse (source OnDeath: `entity` is the dead monster, `abilityId` "onDeath" / "affix.<type>" /
  // "curse.<id>", `kind` the shape's: Circle / Ring) ends at contactMs (no cancel: a dead monster's fuse always goes off).
  // A raise / summon (kind Raise / Summon, source Ability) draws a breach-violet marker (DR5: enemy magic that is not a
  // damage hazard) per corpse / spot, ended like a strike's telegraph.
  TelegraphSource source = TelegraphSource::Ability;
  // 2F review: a dash (a charge, MonsterAbilityDef::dash): the caster moves to `dashTo` at contact (EvEntityTeleported
  // reason Dash), after the hit.
  bool hasDash = false;
  Vec2 dashTo;
};
// Dark remake K2: a ground hazard placed by a monster ability (combat/HazardField.h) at `origin` facing `dir`; it ticks on
// the sim clock until untilMs (absolute sim ms; UE fades it out then). A zone change drops every hazard silently.
struct EvHazard {
  uint32_t id = 0;
  EntityId source = kNoEntity;  // the caster
  std::string abilityId;
  AbilityShapeDef shape;
  Vec2 origin;
  Vec2 dir{1, 0};
  DamageType damageType = DamageType::Physical;
  double startMs = 0;
  double untilMs = 0;
  double tickMs = 0;
};
// Dark remake K3 (implementation-plan.md 3 K3, 4 U3; monsters/Corpses.h): a body joined the zone's corpse registry: a
// kill (its Death pose holds, then it crumbles to the ash husk / the carcass, DR6), or a map seed at zone entry (already
// a husk). expiresMs (absolute sim ms) is when it fades (0 = never: a seed). A fire killing blow registers the body
// burned: EvCorpseRemoved{burned} follows at once. `monster` is the dead instance (kNoEntity for a seed); `spent` = it
// rose once already (never raised again).
struct EvCorpseAdded {
  uint32_t corpseId = 0;
  EntityId monster = kNoEntity;
  std::string defId;
  std::string artId;
  std::string family;
  CorpseKind kind = CorpseKind::Corpse;
  Vec2 pos;
  double deathMs = 0;
  double expiresMs = 0;
  bool seeded = false;
  bool spent = false;
};
// A body left the registry: it faded (expired: its time ran out or the per-zone cap pushed it out), stood up (raised:
// `by` = the raiser, kNoEntity for the Unburied rise; EvMonsterRaised follows), was eaten (consumed: `by` = the eater,
// at the end of its channel) or burned (fire over it). A zone change drops every body silently.
struct EvCorpseRemoved {
  uint32_t corpseId = 0;
  CorpseRemoveReason reason = CorpseRemoveReason::Expired;
  EntityId by = kNoEntity;
};
// A monster stood up (MonsterRole::Raised): from corpse `corpseId` (a raise; `selfRise` = the Unburied rise of the dead
// monster's own body) or summoned at `pos` (corpseId 0: a summon ability, an on-death spawn). EvEntitySpawned comes
// first, EvPlayAnim{Rise} right after; the body is rooted for riseMs. `raiser` = the caster it collapses with
// (kNoEntity: nobody). abilityId = the raise / summon ability, "riseAgain", or the on-death label.
struct EvMonsterRaised {
  EntityId entity = kNoEntity;
  EntityId raiser = kNoEntity;
  uint32_t corpseId = 0;
  std::string defId;
  std::string abilityId;
  Vec2 pos;
  double startMs = 0;
  double riseMs = 0;
  bool selfRise = false;
};
// A Raised body fell because its raiser died (monsters.md 1.3: every Raised collapses with its raiser): no kill (no
// credit, no on-death effect, no rise); its body stays as a spent corpse (EvCorpseAdded follows when it leaves one). The
// EvPlayAnim{Death} before it plays the fall.
struct EvMonsterCollapsed {
  EntityId entity = kNoEntity;
  EntityId raiser = kNoEntity;
};
// Dark remake K4 (implementation-plan.md 3 K4; monsters/Phases.h): a monster entered phase `phase` (MonsterInstance::
// phase: the rows entered) because its life dropped below the row's hpBelowPct (the boss bar ticks; UE plays `stinger`
// when set, and the phase's look, e.g. a standard that ignites). A hit that crosses two thresholds emits two, in order.
// The row's enter action is the EvPlayAnim right after it (rooted for its durationMs; it still takes damage). `reset`:
// the monster walked home (M1) and is back to phase 0 at full life (UE drops the phase look; no stinger).
struct EvMonsterPhase {
  EntityId entity = kNoEntity;
  int32_t phase = 0;
  int32_t phaseCount = 0;   // rows in the monster's phase table
  double hpBelowPct = 0;    // the entered row's threshold (0 on a reset)
  std::string stinger;      // music stinger id ("" = none)
  bool reset = false;
};
// Dark remake K4: a phase lit a soft-enrage arena around `centre` (kind ringShrink: its radius goes from `radius` at
// startMs to `targetRadius` at endMs, linearly, then stays); the hero outside it takes damage every tickMs. `cleared`:
// the arena went out (its monster died, collapsed or walked home, or a later phase lit another one: a new EvArena for
// the same entity replaces it); a zone change drops it silently.
struct EvArena {
  EntityId entity = kNoEntity;  // the monster whose phase lit it
  ArenaKind kind = ArenaKind::RingShrink;
  Vec2 centre;
  double radius = 0;
  double targetRadius = 0;
  double startMs = 0;
  double endMs = 0;
  double tickMs = 0;
  double dpsPctMaxHp = 0;
  DamageType damageType = DamageType::Physical;
  bool cleared = false;
};
// Dark remake K4 Feed: a Raised monster (`fed`) killed within a feeding phase's radius is drawn into `entity`: its corpse
// is held from startMs; at dueMs, if fire has not burned it, EvCorpseRemoved{consumed, by entity} follows and the monster
// heals (its life in the snapshot).
struct EvMonsterFeed {
  EntityId entity = kNoEntity;
  EntityId fed = kNoEntity;
  uint32_t corpseId = 0;
  Vec2 pos;
  double startMs = 0;
  double dueMs = 0;
};
// Dark remake K5 (implementation-plan.md 3 K5; monsters/Packs.h): a monster's block took a hit from within its front arc
// (MonsterDef::block): `amount` is the damage it took (the EvHit that follows carries it), `blocked` what the shield
// stopped, in the same units (both after a K4 phase's damageTakenMul: amount + blocked is what the hit would have dealt
// unblocked); `from` is where the hit came from. UE sparks the shield (the art's EvBlock VFX at socket `face`) and plays
// the clang.
struct EvBlock {
  EntityId entity = kNoEntity;  // the blocker
  EntityId source = kNoEntity;  // the attacker (kHeroEntityId, a pet, a monster)
  Vec2 from;
  double amount = 0;
  double blocked = 0;
};
// Dark remake K5 morale: a monster broke and flees (MonsterState::Flee) because `leader`, its pack's leader, died at
// `from` (it runs away from there until untilMs), or (`ended`) its flee ended: `reason` expired, sustained (a sustain
// member or a fearImmune buff steadied it), blocked (no step led away: it fights) or interrupted (the leash or a camp).
// A death or a zone change ends a flee silently.
struct EvMonsterFlee {
  EntityId entity = kNoEntity;
  EntityId leader = kNoEntity;
  Vec2 from;
  double startMs = 0;
  double untilMs = 0;
  bool ended = false;
  FleeEnd reason = FleeEnd::Expired;
};
// Dark remake K5: a buff / call ability resolved at its contact around `centre` (the caster) within `radius`: a call
// reached `called` other monsters (of `family`; each is provoked unless already fighting: the social call), and `buffed`
// monsters (the caster included) got `stat` x `mul` until untilMs (when `hasBuff`). UE plays the pulse (DR5:
// breach-violet, enemy magic that is not a damage hazard).
struct EvMonsterBuffCast {
  EntityId entity = kNoEntity;
  std::string abilityId;
  AbilityKind kind = AbilityKind::Buff;
  Vec2 centre;
  double radius = 0;
  std::string family;
  bool hasBuff = false;
  MonsterBuffStat stat = MonsterBuffStat::Damage;
  double mul = 1;
  double untilMs = 0;
  int32_t buffed = 0;
  int32_t called = 0;
};
// Dark remake K6 (world/WorldState.h): a world flag was set (a quest's setsFlags on its turn-in; flags are never
// cleared). Emitted once per flag, in the quest's data order, before the turn-in's other events. UE re-reads moods /
// audio emitters from Snapshot::worldFlags.
struct EvWorldFlag {
  std::string flag;
  std::string questId;  // the quest whose turn-in set it
};
// Dark remake K6: a quest's permanent reward was granted (once per quest and difficulty; QuestWorld::TurnIn, after the
// items): the amounts this grant added (the running totals are Snapshot::permanentRewards).
struct EvPermanentReward {
  std::string questId;
  Difficulty difficulty = Difficulty::Normal;
  int32_t maxLife = 0;
  int32_t statPoints = 0;
  int32_t skillPoints = 0;
  int32_t respecTokens = 0;
};
struct EvProjectileLaunched {
  EntityId projectile = kNoEntity;
  ProjectileKind kind = ProjectileKind::HeroSkill;
  EntityId source = kNoEntity, target = kNoEntity;
  Vec2 from, to;
  double launchMs = 0, travelMs = 0;
  std::string vfxId;
  uint32_t color = 0;
};
struct EvProjectileEnded {
  EntityId projectile = kNoEntity;
  bool hit = true;  // false = fizzled / destroyed (F2)
};
struct EvGroundEffectStarted {
  EntityId id = kNoEntity;
  std::string skillId;
  std::string vfxId;
  Vec2 center;
  double radius = 0;
  double startMs = 0, durationMs = 0;
  GroundTrigger trigger = GroundTrigger::Periodic;  // Armed = a trap waiting for a monster (C4)
  int32_t ticks = 1;
};
// An armed trap fired (C4, GroundTrigger::Armed): play the trigger VFX; ticks follow.
struct EvGroundEffectTriggered {
  EntityId id = kNoEntity;
  double triggerMs = 0;
};
struct EvGroundEffectEnded {
  EntityId id = kNoEntity;
  bool triggered = true;  // false = an armed trap expired without firing
};
// Skill VFX that is not a projectile or ground effect (slash arcs, buffs, blinks, chain points...).
struct EvSkillVfx {
  std::string skillId;
  std::string vfxId;
  EntityId caster = kNoEntity;
  EntityId target = kNoEntity;
  Vec2 origin, point;
  double radius = 0;
  std::vector<Vec2> points;  // chain lightning / multishot target points (in order)
  double staggerMs = 0;      // per-point delay (C4 chain 55 ms)
  // C14 (hero skills): the monster at each of `points` (same order; kNoEntity for a ground point such as a no-target
  // chain's end), and the unit cast direction the core aimed the cone / line / projectile / chain along (zero for
  // casts without one: buffs, novas, blinks, pets).
  std::vector<EntityId> pointTargets;
  Vec2 dir;
  // C14: the sim time of the release (hero skills; < 0 = not stamped, e.g. pet abilities). A host frame dispatches the
  // events of up to maxStepsPerFrame steps at once, so presentation that must land on a core delay (the multishot
  // arrows' SkillArrowDelayMs) times from this, not from the frame's snapshot time.
  double atMs = -1;
};
// `value`: the status value applied (DoT per tick, slow percent, ...); for a stackCount status (dark remake K6 Ashnumb)
// the stacks the target carries after this application (UE's AshnumbTint band).
struct EvStatusApplied {
  EntityId target = kNoEntity;
  StatusType type = StatusType::Burn;
  double value = 0, durationMs = 0;
  bool refreshed = false;
};
struct EvStatusExpired {
  EntityId target = kNoEntity;
  StatusType type = StatusType::Burn;
};
struct EvFloatingText {
  FloatingTextKind kind = FloatingTextKind::Custom;
  EntityId anchor = kNoEntity;  // or kNoEntity with pos
  Vec2 pos;
  double value = 0;
  bool crit = false;
  DamageType element = DamageType::Physical;
  LocText text;  // Custom / Status
};

// ---- hero ----
// PLAYER_EXP_CHANGED (every RewardService::GrantExp / RemoveExp; amount < 0 for the death toll).
struct EvExpGained {
  int64_t amount = 0;
  int64_t exp = 0;
  int64_t expToNext = 0;
  ExpSource source = ExpSource::Kill;
};
struct EvLevelUp {
  int32_t level = 1;
};
struct EvGoldChanged {
  int64_t gold = 0;   // new total
  int64_t delta = 0;  // applied change
  GoldReason reason = GoldReason::Kill;
};
struct EvSpiritChanged {
  double value = 0, maxValue = 100;
  bool resonating = false;
  double gained = 0;
  // classes 14.3 / 17 `source?`: set on a combat gain (hit / kill / dodge); absent on resonance end, drain and death.
  bool hasSource = false;
  SpiritSource source = SpiritSource::Hit;
};
struct EvResonance {
  bool started = true;  // false = ended
  std::string profileId;
  double durationMs = 0;
};
struct EvSkillUsed {
  std::string skillId;
  DamageType damageType = DamageType::Physical;
  int32_t slot = -1;
};
struct EvSkillBuffered {
  std::string skillId;
  double expiresAtMs = 0;
};
struct EvSkillLevelChanged {
  std::string skillId;
  int32_t level = 0;
};
struct EvHotbarChanged {};
struct EvDodgeStarted {
  double cooldownMs = 0, invulnerabilityMs = 0;
  Vec2 from, to;
};
// C4 Charge dash (HeroLocomotion::StartDash): Started -> Arrived, or Interrupted by stun / freeze (C5) where the hero
// stands. Drives the R5 `charge` signature montage and the dash streaks; the position itself comes from the Snapshot.
struct EvHeroDash {
  enum class Phase : uint8_t { Started, Arrived, Interrupted } phase = Phase::Started;
  Vec2 from, to;
  double startMs = 0, durationMs = 0;  // sim clock
  std::string skillId;
};
struct EvTargetChanged {
  EntityId target = kNoEntity;
};
struct EvCombatStateChanged {
  bool inCombat = false;
};
struct EvHeroDied {
  Vec2 pos;
};
struct EvHeroRespawned {
  Vec2 pos;
};
struct EvTownPortal {
  enum class Phase : uint8_t { Started, Completed, Cancelled } phase = Phase::Started;
  double channelMs = 0;
  Vec2 destination;
};

// ---- items ----
struct EvLootDropped {
  EntityId drop = kNoEntity;
  std::string itemUid;
  std::string baseId;
  ItemQuality quality = ItemQuality::Normal;
  int32_t quantity = 1;
  Vec2 pos;
  double expiresAtMs = 0;  // 0 = never
  // loot 17 LootDropSpawned {dropId, item}: the whole item, so the world nameplate shows ItemDisplayName (affixes,
  // legendaryId, setId; loot 18 / FIX Q16) and the tooltip can be built before the next snapshot.
  ItemInstance item;
  Vec2 visualOffset;       // render offset from `pos` in tiles (GroundItem::visualOffset)
  // A treasure-cache drop (dropLootAtPosition, loot 6.1): it falls in over `fallInMs` (from the chest, loot 18) and has
  // no ITEM_DROPPED feedback (loot 6.5: no legendary / set camera flash or shake). Monster drops: false / 0.
  bool cacheDrop = false;
  double fallInMs = 0;
};
struct EvPotionDropped {
  EntityId drop = kNoEntity;
  PotionKind kind = PotionKind::Hp;
  int32_t amount = 0;
  Vec2 pos;
};
struct EvItemPicked {
  std::string itemUid;
  std::string baseId;
  ItemQuality quality = ItemQuality::Normal;
  int32_t quantity = 1;
  ItemInstance item;  // loot 17 ItemPicked {item}: the picked item as granted (HUD notice: ItemDisplayName x quantity)
};
// loot 7.3 / 17 ItemDiscarded {item}: a bag item destroyed by the discard action (not by sell / salvage / bulk destroy).
struct EvItemDiscarded {
  ItemInstance item;
};
struct EvPotionPicked {
  PotionKind kind = PotionKind::Hp;
  int32_t amount = 0;
};
struct EvInventoryChanged {};
struct EvEquipmentChanged {
  EquipSlot slot = EquipSlot::Weapon;
};
struct EvStashChanged {};
struct EvShopOpened {
  std::string npcId;
  bool blacksmith = false;
};
struct EvShopClosed {
  std::string npcId;
};
struct EvCraftPerformed {
  CraftAction action = CraftAction::Salvage;
  bool ok = false;
  std::string itemUid;
  LocText reason;  // failure reason
};

// ---- quests / NPC / dialogue / story ----
// Progress from a drop, a gather node or a clue carries the source point and the quest-item kind so UE can play the
// pickup visual (quests 3.2: the item pops at the kill / node point, then flies to the hero).
struct EvQuestUpdate {
  enum class Kind : uint8_t { Accepted, Progress, Completed, TurnedIn, Failed, Tracked } kind = Kind::Accepted;
  std::string questId;
  int32_t objectiveIndex = -1;
  int32_t current = 0, required = 0;
  bool completesQuest = false;
  bool hasFrom = false;  // Progress from a drop / gather node / clue
  Vec2 from;             // kill point / node / clue tile
  std::string itemKind;  // quest-item icon kind (QuestItemIcons), "" for kill / talk / explore progress
  // Progress only (quests 13 QuestProgress payload): the objective's targetId and the requested amount (the web's
  // QUEST_PROGRESS `amount`, before the clamp to `required`).
  std::string targetId;
  int32_t amount = 0;
};
struct EvNpcInteracted {
  std::string npcId;
  EntityId npc = kNoEntity;
};
struct EvQuestCardOpened {
  std::string npcId;
};
struct EvDialogue {
  enum class Kind : uint8_t { Opened, NodeChanged, Closed } kind = Kind::Opened;
  std::string npcId;
  std::string treeId;
  std::string nodeId;
};
struct EvMiniBossDialogue {
  bool opened = true;
  EntityId monster = kNoEntity;
  std::string monsterId;
  // opened only (monsters-ai 8.3 / M8): header name key and every line of the linear tree, in order
  // (start -> nextNodeId until isEnd), as i18n keys data.miniBossDialogue.<monsterId>.<nodeId>.
  std::string nameKey;
  std::vector<std::string> lineKeys;
};
struct EvLoreCollected {
  std::string loreId;
};
struct EvHiddenAreaDiscovered {
  std::string areaId;
};
struct EvAchievementUnlocked {
  std::string achievementId;
};
struct EvStoryState {
  bool active = false;  // StoryDirector busy (STORY_STATE)
  std::string beatId;   // the beat that started / the last beat that finished
};
struct EvStoryBeat {
  enum class Phase : uint8_t { Began, Ended } phase = Phase::Began;
  std::string beatId;
  StoryBeatKind kind = StoryBeatKind::Cutscene;
};
// One presentation step of the current beat (cutscene step, sequence slide or chapter card), on the real-time clock.
// The core resolves what UE would otherwise have to decide (quests-story-ch1.md 8.5):
//   focus      hasFocus + focusEntity / focusPos: the `focus` step's target (player, {npc}, {monster} = the nearest
//              living instance, or a tile). A target that does not exist resolves the step at once (hasFocus false).
//   speaker    `say` steps: speakerNameKey (story.speaker.hero / story.speaker.villain / data.npc.<id>.name / the boss
//              intro name story.boss.<id>.name, else data.monster.<id>) and speakerArtId (portrait art id: the hero's
//              class id, "emblem_villain" for the villain, the NPC's spriteId else its id, the nearest living monster
//              instance's spriteKey else the def's; "emblem_generic" when nothing resolves).
//   timedMs    core-timed length of a self-ending step (focus, shake, flash, wait, title hold, chapter card); 0 = the
//              step waits for StoryAdvance (narrate, say, whisper, slides; the two-tap rule is UE's, StoryTiming).
struct EvStoryStep {
  std::string beatId;
  int32_t index = 0;
  bool isSlide = false;
  std::string sequenceId;  // slides: "prologue" / "epilogue" / "credits" (the epilogue beat plays both), "" otherwise
  CutsceneStep step;   // cutscene steps
  StorySlide slide;    // sequence slides / chapter card text keys
  bool hasFocus = false;
  EntityId focusEntity = kNoEntity;  // kNoEntity for a tile target
  Vec2 focusPos;
  std::string speakerNameKey;
  std::string speakerArtId;
  double timedMs = 0;
};
struct EvBossBar {
  bool show = false;
  EntityId monster = kNoEntity;
  std::string nameKey, epithetKey;
};
struct EvMonsterRenamed {
  EntityId monster = kNoEntity;
  std::string nameKey;
  uint32_t color = 0;
};
struct EvHuntRevealed {
  std::string huntId;
};
struct EvPet {
  enum class Kind : uint8_t { Obtained, Changed, LevelUp, Evolved, Exhausted, Recovered } kind = Kind::Changed;
  std::string petId;
  bool silent = false;
};
struct EvEmbersGained {
  int32_t amount = 0;
  Vec2 pos;
  bool fromKill = true;
};

// ---- world / flow ----
struct EvZone {
  enum class Phase : uint8_t { TransitionBegan, Entered, Exited } phase = Phase::Entered;
  std::string mapId;
  TilePos target;
};
struct EvInteractPrompt {
  InteractKind kind = InteractKind::None;
  EntityId id = kNoEntity;
  Vec2 pos;
};
struct EvRandomEvent {
  bool resolved = false;
  RandomEventType type = RandomEventType::Ambush;
  Vec2 pos;
  std::string zoneId;           // sys.event.* keys are per zone
  EntityId prop = kNoEntity;    // chest / merchant / puzzle prop / rescue NPC
  bool champion = false;        // dark remake K6: an ambush that rolled the zone's champion pack (ambushChampion)
};
// The environmental puzzle prompt (world 13.3; core-owned modal PanelId::Puzzle). Text keys:
// sys.event.puzzle.<zoneId>.<puzzleIndex>.{prompt,solution,reward}; answer with CmdPuzzleAnswer (0 solution, 1 leave).
struct EvPuzzlePrompt {
  bool open = true;  // false = closed (answered, left, zone exit, death)
  EntityId prop = kNoEntity;
  std::string zoneId;
  int32_t puzzleIndex = -1;
};
struct EvStoryDecorFocus {
  std::string decorId;  // empty = none
  bool showTooltip = false;
};
struct EvLog {
  LocText text;
  LogType type = LogType::System;
};
struct EvBanner {
  BannerKind kind = BannerKind::Toast;
  LocText title;
  LocText subtitle;
};
// A core-owned modal panel opened or closed (SimTypes.h IsCoreOwnedPanel): GameSim emits it whenever the owning
// system's state changes, so UE pushes / pops the widget from one signal. npcId: the NPC of a dialogue / quest card /
// shop / stash.
struct EvPanelRequest {
  PanelId panel = PanelId::Inventory;
  bool open = true;
  std::string npcId;
};
struct EvSaveRequested {
  std::string reason;  // SaveReason name; UE calls GameSim::SaveGame() and writes the slot
};

// ---- audio / camera ----
struct EvMusic {
  std::string trackKey;  // "" = silence
  double fadeOutSec = 0, fadeInSec = 0;
  bool loop = true;
  bool restart = false;
};
struct EvSfx {
  SfxId cue = SfxId::Hit;
  bool spatial = false;
  Vec2 pos;
  EntityId source = kNoEntity;
};
struct EvCameraShake {
  double durationMs = 0;
  double intensity = 0;  // web units (C9: same peak at render scale 1)
};
struct EvCameraFlash {
  uint32_t color = 0xffffff;
  double durationMs = 0;
  double alpha = 1;
};
struct EvSlowMotion {
  double timeScale = 1;
  double realDurationMs = 0;  // S6: dilation of sim + visuals
};

using Event = std::variant<EvEntitySpawned, EvEntityDespawned, EvEntityTeleported, EvPlayAnim, EvHit,
                           EvMonsterAttackCancelled, EvProjectileLaunched, EvProjectileEnded, EvGroundEffectStarted,
                           EvGroundEffectTriggered, EvGroundEffectEnded, EvSkillVfx, EvStatusApplied, EvStatusExpired,
                           EvFloatingText, EvExpGained, EvLevelUp, EvGoldChanged, EvSpiritChanged, EvResonance,
                           EvSkillUsed, EvSkillBuffered, EvSkillLevelChanged, EvHotbarChanged, EvDodgeStarted,
                           EvHeroDash, EvTargetChanged, EvCombatStateChanged, EvHeroDied, EvHeroRespawned, EvTownPortal,
                           EvLootDropped, EvPotionDropped, EvItemPicked, EvItemDiscarded, EvPotionPicked, EvInventoryChanged,
                           EvEquipmentChanged, EvStashChanged, EvShopOpened, EvShopClosed, EvCraftPerformed,
                           EvQuestUpdate, EvNpcInteracted, EvQuestCardOpened, EvDialogue, EvMiniBossDialogue,
                           EvLoreCollected, EvHiddenAreaDiscovered, EvAchievementUnlocked, EvStoryState, EvStoryBeat,
                           EvStoryStep, EvBossBar, EvMonsterRenamed, EvHuntRevealed, EvPet, EvEmbersGained, EvZone,
                           EvInteractPrompt, EvRandomEvent, EvPuzzlePrompt, EvStoryDecorFocus, EvLog, EvBanner,
                           EvPanelRequest, EvSaveRequested, EvMusic, EvSfx, EvCameraShake, EvCameraFlash, EvSlowMotion,
                           EvTelegraph, EvHazard, EvCorpseAdded, EvCorpseRemoved, EvMonsterRaised,
                           EvMonsterCollapsed, EvMonsterPhase, EvArena, EvMonsterFeed, EvBlock, EvMonsterFlee,
                           EvMonsterBuffCast, EvWorldFlag, EvPermanentReward>;

// Append-only event list shared by every subsystem of one GameSim.
class EventSink {
 public:
  void Emit(Event e) { events_.push_back(std::move(e)); }
  // Convenience: a combat-log line.
  void Log(LocText text, LogType type = LogType::System) { Emit(EvLog{std::move(text), type}); }
  void Sfx(SfxId cue, bool spatial = false, Vec2 pos = {}, EntityId source = kNoEntity) {
    Emit(EvSfx{cue, spatial, pos, source});
  }
  const std::vector<Event>& Items() const { return events_; }
  size_t Size() const { return events_.size(); }
  void Clear() { events_.clear(); }

 private:
  std::vector<Event> events_;
};

}  // namespace abyss

// Audio cue table and music-director constants. Sources: audio_cues.json, music.json (director part; the procedural
// themes/scores are offline-render input kept as raw JSON). Spec: audio.md 3, 5, 9.8, 11.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "abyss/base/Enums.h"
#include "abyss/base/Platform.h"

namespace abyss {

// SFX cue ids (= web SFXType, audio.md 9.8, then the port cues). Order = audio_cues.json cue order.
// A7 port cues (monster vocalisations): MonsterAggro (idle / patrol / returning -> chase, MonsterAggroMsg) and MonsterHurt
// (non-lethal, non-tick hit) are emitted by the core with the monster as EvSfx::source; UE picks the variant of the
// source's family (AudioCueDef::families = monster animCategory). Hero footsteps and the ambience bed are UE-side
// (Run-animation notifies + tile type; zone mood): they follow animation / zone state, not gameplay events.
enum class SfxId : uint8_t {
  Hit,
  HitHeavy,
  Crit,
  Miss,
  Block,
  PlayerHurt,
  MonsterDeath,
  PlayerDeath,
  Dodge,
  Resonance,
  SkillMelee,
  SkillFire,
  SkillIce,
  SkillLightning,
  SkillHeal,
  SkillBuff,
  LootCommon,
  LootMagic,
  LootRare,
  LootLegendary,
  Equip,
  Potion,
  Click,
  PanelOpen,
  PanelClose,
  Error,
  ZoneTransition,
  QuestComplete,
  QuestProgress,
  QuestObjective,
  LevelUp,
  NpcInteract,
  Anvil,
  // ---- port cues (A7) ----
  MonsterAggro,
  MonsterHurt,
};
ABYSS_ENUM_STRINGS(SfxId, "hit", "hit_heavy", "crit", "miss", "block", "player_hurt", "monster_death", "player_death",
                   "dodge", "resonance", "skill_melee", "skill_fire", "skill_ice", "skill_lightning", "skill_heal",
                   "skill_buff", "loot_common", "loot_magic", "loot_rare", "loot_legendary", "equip", "potion",
                   "click", "panel_open", "panel_close", "error", "zone_transition", "quest_complete",
                   "quest_progress", "quest_objective", "levelup", "npc_interact", "anvil", "monster_aggro",
                   "monster_hurt")

enum class AudioBus : uint8_t { Combat, Ui };
ABYSS_ENUM_STRINGS(AudioBus, "combat", "ui")

struct AudioCueDef {
  SfxId id = SfxId::Hit;
  double lengthSec = 0;
  int32_t variants = 1;
  bool webReachable = true;
  std::vector<std::string> assets;  // SW_SFX_<Id>[_0n]
  AudioBus bus = AudioBus::Combat;
  bool spatial3d = false;
  int32_t concurrencyMax = 1;
  std::string concurrencyRule;  // "stopOldest"
  double minRetriggerMs = 0;
  std::vector<AnimRig> families;  // per-family cues (A7): one asset set per monster animCategory; empty = one set
};

// Event -> cue mapping rules (audio_cues.json rules; audio.md 3).
struct AudioRulesDef {
  SfxId combatDodged = SfxId::Miss, combatCrit = SfxId::Crit, combatHit = SfxId::Hit;
  std::array<SfxId, EnumCount<ItemQuality>()> itemPickedByQuality{};
  std::array<SfxId, EnumCount<DamageType>()> skillUsedByDamageType{};
  SfxId playerLevelUp = SfxId::LevelUp, playerDied = SfxId::PlayerDeath, dodgeStarted = SfxId::Dodge,
        resonanceStarted = SfxId::Resonance, monsterDied = SfxId::MonsterDeath, questCompleted = SfxId::QuestComplete,
        questAccepted = SfxId::NpcInteract, questTurnedIn = SfxId::QuestComplete, npcInteract = SfxId::NpcInteract,
        shopOpen = SfxId::PanelOpen, inventoryOpen = SfxId::PanelOpen, inventoryClose = SfxId::PanelClose,
        uiTogglePanel = SfxId::Click;
  SfxId questObjectiveDone = SfxId::QuestObjective, questMaterialOrClueStep = SfxId::QuestProgress;
  std::vector<std::string> questProgressPrefixes;  // mat_, clue_
  SfxId townPortalComplete = SfxId::ZoneTransition, soulEchoReclaimed = SfxId::Resonance, forgeSuccess = SfxId::Anvil,
        forgeError = SfxId::Error;
  SfxId heavyHitCue = SfxId::HitHeavy;    // A6: heavy/crit/kill weights
  SfxId heroDamageTakenCue = SfxId::PlayerHurt;  // A6
  SfxId monsterAggro = SfxId::MonsterAggro;      // A7: MonsterAggroMsg
  SfxId monsterHurt = SfxId::MonsterHurt;        // A7: non-lethal, non-tick hit on a monster
  double spatialSpread = 0.25;            // A4
};

struct MusicDirectorDef {
  double zoneFadeSec = 2, stateFadeSec = 1.5, fadeInSec = 1;
  double victoryHoldMs = 3000;
  double combatOffDelayMs = 1500;
  double bossVictoryHoldMs = 8000;  // A5
  bool trueDebounce = true;          // A2
  bool exploreResumesPosition = true;
  std::string bossCh1Score = "boss_ch1";
  double defaultMusicVolume = 0.6, defaultSfxVolume = 0.8;  // A3
  std::vector<std::string> themeZones;  // zones with a music theme (music.json themes keys)
};

struct ABYSS_API AudioTables {
  std::vector<AudioCueDef> cues;
  AudioRulesDef rules;
  MusicDirectorDef music;
  const AudioCueDef* Find(SfxId id) const;
};

}  // namespace abyss

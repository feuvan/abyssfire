/**
 * audio_cues.json and music.json.
 *
 * The SFX id list is read from the SFXType union (src/systems/audio/types.ts) and cross-checked against
 * the SFXEngine dispatch. The event → cue rules are probed on the live AudioManager singleton (its
 * EventBus listeners are registered at module load): playSFX is replaced by a recorder and each event is
 * emitted with representative payloads. Per-cue rendering/playback metadata (length, noise variants,
 * bus, spatialisation, concurrency) comes from the audio port spec (audio.md §4.1, §9.6, §9.7) — it does
 * not exist in the web code.
 */
import { audioManager } from '../../../../../src/systems/audio/AudioManager';
import * as AudioManagerMod from '../../../../../src/systems/audio/AudioManager';
import { ZONE_THEMES, ZONE_SCORES } from '../../../../../src/systems/audio/MusicEngine';
import * as MusicEngineMod from '../../../../../src/systems/audio/MusicEngine';
import { MODES } from '../../../../../src/systems/audio/Composer';
import * as ComposerMod from '../../../../../src/systems/audio/Composer';
import * as ScorePlayerMod from '../../../../../src/systems/audio/ScorePlayer';
import { EventBus, GameEvents } from '../../../../../src/utils/EventBus';
import { assert, assertSource, plain, readSource, type TableResult } from '../util';

declare const __BGM_MANIFEST__: Record<string, string>;

const exp = <T>(mod: unknown, name: string): T => {
  const v = (mod as Record<string, unknown>)[`__expose_${name}`];
  assert(v !== undefined, `exposed binding ${name} missing`);
  return v as T;
};

/** audio.md §4.1: render length (s), noise layers → 4 baked variants, reachable in the web build. */
const CUE_SPEC: Record<string, { lengthSec: number; variants: number; webReachable: boolean }> = {
  hit: { lengthSec: 0.2, variants: 4, webReachable: true },
  hit_heavy: { lengthSec: 0.35, variants: 4, webReachable: false },
  crit: { lengthSec: 0.35, variants: 4, webReachable: true },
  miss: { lengthSec: 0.2, variants: 1, webReachable: true },
  block: { lengthSec: 0.2, variants: 4, webReachable: false },
  player_hurt: { lengthSec: 0.25, variants: 4, webReachable: false },
  monster_death: { lengthSec: 0.6, variants: 4, webReachable: false },
  player_death: { lengthSec: 1.2, variants: 1, webReachable: true },
  dodge: { lengthSec: 0.22, variants: 4, webReachable: true },
  resonance: { lengthSec: 0.61, variants: 4, webReachable: true },
  skill_melee: { lengthSec: 0.35, variants: 4, webReachable: true },
  skill_fire: { lengthSec: 0.5, variants: 4, webReachable: true },
  skill_ice: { lengthSec: 0.45, variants: 4, webReachable: true },
  skill_lightning: { lengthSec: 0.4, variants: 4, webReachable: true },
  skill_heal: { lengthSec: 0.55, variants: 4, webReachable: false },
  skill_buff: { lengthSec: 0.5, variants: 1, webReachable: true },
  loot_common: { lengthSec: 0.2, variants: 1, webReachable: true },
  loot_magic: { lengthSec: 0.3, variants: 1, webReachable: true },
  loot_rare: { lengthSec: 0.52, variants: 4, webReachable: true },
  loot_legendary: { lengthSec: 0.79, variants: 1, webReachable: true },
  equip: { lengthSec: 0.1, variants: 4, webReachable: true },
  potion: { lengthSec: 0.25, variants: 1, webReachable: false },
  click: { lengthSec: 0.05, variants: 1, webReachable: true },
  panel_open: { lengthSec: 0.1, variants: 1, webReachable: true },
  panel_close: { lengthSec: 0.1, variants: 1, webReachable: false },
  error: { lengthSec: 0.2, variants: 1, webReachable: true },
  zone_transition: { lengthSec: 1.02, variants: 1, webReachable: true },
  quest_complete: { lengthSec: 0.75, variants: 1, webReachable: true },
  quest_progress: { lengthSec: 0.41, variants: 1, webReachable: true },
  quest_objective: { lengthSec: 0.52, variants: 1, webReachable: true },
  levelup: { lengthSec: 1.02, variants: 1, webReachable: true },
  npc_interact: { lengthSec: 0.19, variants: 1, webReachable: true },
  anvil: { lengthSec: 1.0, variants: 4, webReachable: true },
};

/**
 * DECISIONS A7 (not in the web): basic monster vocalisations. The core emits these cues (EvSfx with the monster as
 * `source`): `monster_aggro` when a monster starts chasing the hero (idle / patrol / returning → chase, MonsterAggroMsg),
 * `monster_hurt` on a non-lethal hit (not DoT ticks); death keeps the existing `monster_death`. Per family: UE plays
 * the asset of the source monster's animCategory (`SW_SFX_<Cue>_<Family>`). Hero footsteps (grass / dirt / stone, from
 * Run-animation notifies and the tile under the foot) and the plains ambience bed (zone mood) are UE-side: they follow
 * animation and zone state, not gameplay events, so they have no core cue.
 */
const PORT_CUES: Record<string, { lengthSec: number; variants: number; families: string[]; decision: string }> = {
  monster_aggro: { lengthSec: 0.5, variants: 2, families: ['humanoid', 'slime'], decision: 'A7' },
  monster_hurt: { lengthSec: 0.3, variants: 2, families: ['humanoid', 'slime'], decision: 'A7' },
};

/** audio.md §9.7: UI / progression cues play 2D; the rest are world (combat) cues with mild 3D panning (A4). */
const UI_CUES = ['click', 'panel_open', 'panel_close', 'error', 'loot_common', 'loot_magic', 'loot_rare', 'loot_legendary', 'equip',
  'potion', 'quest_complete', 'quest_progress', 'quest_objective', 'levelup', 'npc_interact', 'zone_transition', 'anvil', 'resonance'];

/** audio.md §9.6 sound concurrency (new in the port). */
function concurrency(id: string): { max: number; rule: string; minRetriggerMs: number } {
  if (id === 'hit' || id === 'crit' || id === 'miss') return { max: 8, rule: 'stopOldest', minRetriggerMs: 20 };
  if (id.startsWith('skill_')) return { max: 4, rule: 'stopOldest', minRetriggerMs: 0 };
  if (id.startsWith('loot_')) return { max: 3, rule: 'stopOldest', minRetriggerMs: 0 };
  return { max: 2, rule: 'stopOldest', minRetriggerMs: 0 };
}

const pascal = (id: string) => id.split('_').map(w => w[0].toUpperCase() + w.slice(1)).join('');

function sfxIdsFromSource(): string[] {
  const src = readSource('src/systems/audio/types.ts');
  const m = /export type SFXType =([^;]+);/.exec(src);
  assert(m, 'SFXType union not found');
  return [...m[1].matchAll(/'([a-z_]+)'/g)].map(x => x[1]);
}

/** Emit `event` with `payload` and return the cues the real AudioManager plays. */
function probe(event: string, payload?: unknown): string[] {
  const played: string[] = [];
  const am = audioManager as unknown as { playSFX: (id: string) => void };
  const saved = am.playSFX;
  am.playSFX = (id: string) => { played.push(id); };
  try { EventBus.emit(event, payload); } finally { am.playSFX = saved; }
  return played;
}

export function exportAudio(): TableResult[] {
  const ids = sfxIdsFromSource();
  const engine = readSource('src/systems/audio/SFXEngine.ts');
  for (const id of ids) assert(engine.includes(`case '${id}':`), `SFXEngine has no case for ${id}`);
  assert(ids.length === Object.keys(CUE_SPEC).length && ids.every(id => CUE_SPEC[id]), 'CUE_SPEC must list exactly the SFXType ids');

  const cues: Record<string, unknown>[] = ids.map(id => {
    const spec = CUE_SPEC[id];
    const ui = UI_CUES.includes(id);
    const base = `SW_SFX_${pascal(id)}`;
    return {
      id, ...spec,
      assets: spec.variants > 1 ? Array.from({ length: spec.variants }, (_, i) => `${base}_0${i + 1}`) : [base],
      bus: ui ? 'ui' : 'combat',
      spatial: ui ? '2d' : '3d',
      concurrency: concurrency(id),
    };
  });
  // Port cues (A7) after the web cues: SfxId order = this order.
  for (const [id, spec] of Object.entries(PORT_CUES)) {
    assert(!ids.includes(id), `PORT_CUES ${id} is now a web SFX id`);
    const base = `SW_SFX_${pascal(id)}`;
    cues.push({
      id, lengthSec: spec.lengthSec, variants: spec.variants, webReachable: false,
      assets: spec.families.flatMap(f => Array.from({ length: spec.variants }, (_, i) => `${base}_${pascal(f)}_0${i + 1}`)),
      families: spec.families,
      bus: 'combat', spatial: '3d', concurrency: { max: 3, rule: 'stopOldest', minRetriggerMs: 120 },
      decision: spec.decision,
    });
  }

  // Event → cue rules, probed on the real listeners.
  const E = GameEvents as unknown as Record<string, string>;
  const one = (event: string, payload?: unknown): string | null => {
    const p = probe(event, payload);
    assert(p.length <= 1, `${event} played ${p.length} cues`);
    return p[0] ?? null;
  };
  const rules = {
    combatDamage: { dodged: one(E.COMBAT_DAMAGE, { isDodged: true, isCrit: true }), crit: one(E.COMBAT_DAMAGE, { isCrit: true }), hit: one(E.COMBAT_DAMAGE, {}) },
    itemPickedByQuality: Object.fromEntries(['normal', 'magic', 'rare', 'legendary', 'set'].map(q => [q, one(E.ITEM_PICKED, { item: { quality: q } })])),
    skillUsedByDamageType: Object.fromEntries(['physical', 'fire', 'ice', 'lightning', 'poison', 'arcane'].map(d => [d, one(E.SKILL_USED, { damageType: d })])),
    playerLevelUp: one(E.PLAYER_LEVEL_UP, { level: 2 }),
    playerDied: one(E.PLAYER_DIED, {}),
    dodgeStarted: one(E.DODGE_STARTED, {}),
    resonanceStarted: one(E.SPIRIT_RESONANCE_STARTED, {}),
    monsterDied: one(E.MONSTER_DIED, {}),
    questCompleted: one(E.QUEST_COMPLETED, {}),
    questAccepted: one(E.QUEST_ACCEPTED, {}),
    questTurnedIn: one(E.QUEST_TURNED_IN, {}),
    questProgress: {
      completesQuest: one(E.QUEST_PROGRESS, { current: 1, required: 1, targetId: 'mat_herb', completesQuest: true }),
      objectiveDone: one(E.QUEST_PROGRESS, { current: 3, required: 3, targetId: 'goblin', completesQuest: false }),
      materialOrClueStep: one(E.QUEST_PROGRESS, { current: 1, required: 3, targetId: 'mat_herb', completesQuest: false }),
      clueStep: one(E.QUEST_PROGRESS, { current: 1, required: 3, targetId: 'clue_x', completesQuest: false }),
      killStep: one(E.QUEST_PROGRESS, { current: 1, required: 3, targetId: 'goblin', completesQuest: false }),
      progressPrefixes: ['mat_', 'clue_'],
    },
    npcInteract: one(E.NPC_INTERACT, {}),
    shopOpen: one(E.SHOP_OPEN, {}),
    inventoryOpen: one(E.INVENTORY_OPEN, {}),
    inventoryClose: one(E.INVENTORY_CLOSE, {}),
    uiTogglePanel: one(E.UI_TOGGLE_PANEL, {}),
    direct: { townPortalComplete: 'zone_transition', soulEchoReclaimed: 'resonance', forgeSuccess: 'anvil', forgeError: 'error' },
    port: {
      monsterDeathOnEveryKill: 'FIX (audio Q1)',
      oneClickPerPanelToggle: 'FIX (audio Q2)',
      heavyHitCue: { weights: ['heavy', 'crit', 'kill'], cue: 'hit_heavy', decision: 'A6' },
      heroDamageTakenCue: { cue: 'player_hurt', decision: 'A6' },
      monsterAggro: { cue: 'monster_aggro', on: 'idle/patrol/returning -> chase (MonsterAggroMsg)', decision: 'A7' },
      monsterHurt: { cue: 'monster_hurt', on: 'non-lethal, non-tick hit on a monster', decision: 'A7' },
      ueSide: { footsteps: 'Run anim notifies + tile type (grass/dirt/stone)', ambienceBed: 'zone mood', decision: 'A7' },
    },
  };
  assert(rules.skillUsedByDamageType.fire === 'skill_fire' && rules.combatDamage.dodged === 'miss', 'audio wiring probe');

  // Music.
  assertSource('src/systems/audio/MusicEngine.ts', 'this._transition(ctx, destination, 2.0);', 'this._transition(ctx, destination, 1.5);',
    '}, 3000);', 'this._transition(ctx, destination, 1.0);');
  const music = {
    themes: plain(ZONE_THEMES),
    scores: plain(ZONE_SCORES),
    scoreTrim: exp<number>(MusicEngineMod, 'SCORE_TRIM'),
    composer: { modes: plain(MODES), rhythms4: plain(exp<number[][]>(ComposerMod, 'RHYTHMS_4')), rhythms3: plain(exp<number[][]>(ComposerMod, 'RHYTHMS_3')),
      bassFloor: exp<number>(ScorePlayerMod, 'BASS_FLOOR') },
    director: { zoneFadeSec: 2.0, stateFadeSec: 1.5, fadeInSec: 1.0, victoryHoldMs: 3000, combatOffDelayMs: 1500,
      port: { bossVictoryHoldMs: 8000, trueDebounce: true, exploreResumesPosition: true, bossCh1Score: 'boss_ch1', decision: 'A2, A5' } },
    settingsDefaults: plain(exp<Record<string, unknown>>(AudioManagerMod, 'DEFAULT_SETTINGS')),
    portSettingsDefaults: { musicVolume: 0.6, sfxVolume: 0.8, loudnessLufs: -16, decision: 'A3' },
    bgmFiles: plain(__BGM_MANIFEST__),
  };
  return [
    {
      file: 'audio_cues.json',
      source: ['src/systems/audio/types.ts', 'src/systems/audio/SFXEngine.ts', 'src/systems/audio/AudioManager.ts', 'unreal/Docs/spec/audio.md'],
      data: { cues, rules, spatial: { listener: 'hero', spread: 0.25, decision: 'A4' } },
      counts: { cues: cues.length },
    },
    {
      file: 'music.json',
      source: ['src/systems/audio/MusicEngine.ts', 'src/systems/audio/Composer.ts', 'src/systems/audio/ScorePlayer.ts', 'src/systems/audio/AudioManager.ts', 'public/assets/audio/bgm'],
      data: music,
      counts: { themes: Object.keys(ZONE_THEMES).length, scores: Object.keys(ZONE_SCORES).length, bgmFiles: Object.keys(music.bgmFiles).length },
    },
  ];
}

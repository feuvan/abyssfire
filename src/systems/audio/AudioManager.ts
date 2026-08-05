/**
 * AudioManager — coordinator and public API for the audio subsystem.
 *
 * Owns the AudioContext, master gain nodes, and all sub-engines.
 * Replaces the old `audioSystem` singleton.
 *
 * Responsibilities:
 *   - Lazy AudioContext init on first user interaction (getCtx())
 *   - Master musicGain + sfxGain nodes routed to ctx.destination
 *   - Instantiates SFXEngine, MusicEngine, AudioLoader
 *   - Wires all EventBus listeners
 *   - Persists/loads settings via localStorage key 'abyssfire_audio'
 */

import { EventBus, GameEvents } from '../../utils/EventBus';
import { AudioLoader } from './AudioLoader';
import { MusicEngine } from './MusicEngine';
import { SFXEngine } from './SFXEngine';
import type { AudioSettings, MusicState, SFXType } from './types';

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

const STORAGE_KEY = 'abyssfire_audio';

const DEFAULT_SETTINGS: AudioSettings = {
  bgmVolume: 0.15,
  sfxVolume: 0.3,
  bgmMuted: false,
  sfxMuted: false,
};

// ---------------------------------------------------------------------------
// AudioManager
// ---------------------------------------------------------------------------

export class AudioManager {
  private static readonly MUSIC_STATES = ['explore', 'combat', 'victory'] as const;

  // Lazy-initialised — null until first getCtx() call.
  private ctx: AudioContext | null = null;
  private musicGain: GainNode | null = null;
  private sfxGain: GainNode | null = null;

  private loader: AudioLoader;
  private musicEngine: MusicEngine;
  private sfxEngine: SFXEngine;

  private settings: AudioSettings;
  private zoneMusicRequestId = 0;
  private lifecycle: 'locked' | 'unlocking' | 'ready' | 'failed' = 'locked';
  private unlockPromise: Promise<void> | null = null;
  private desiredZone = 'menu';
  private desiredState: MusicState = 'explore';
  private unlockGestureArmed = false;
  private readonly unlockGestureHandler = (): void => {
    this.removeUnlockGesture();
    void this.unlock();
  };

  constructor() {
    this.loader = new AudioLoader();
    this.musicEngine = new MusicEngine(this.loader);
    this.sfxEngine = new SFXEngine(this.loader);

    this.settings = this.loadSettings();
    this.setupEventListeners();
    this.setupUnlockGesture();
  }

  // ---------------------------------------------------------------------------
  // Public API
  // ---------------------------------------------------------------------------

  /** Resume AudioContext — call on user gesture to unblock browser autoplay policy. */
  ensureContext(): void {
    void this.unlock();
  }

  /** Play a sound effect through the sfxGain node (no-op if muted). */
  playSFX(type: SFXType): void {
    if (this.settings.sfxMuted) return;
    const ctx = this.getReadyContext();
    if (!ctx) return;
    if (!this.sfxGain) return;
    this.sfxEngine.play(ctx, this.sfxGain, type);
  }

  /** Set BGM volume (0–1). Updates gain node and persists. */
  setMusicVolume(v: number): void {
    this.settings.bgmVolume = Math.max(0, Math.min(1, v));
    if (this.musicGain && !this.settings.bgmMuted) {
      this.musicGain.gain.value = this.settings.bgmVolume;
    }
    this.saveSettings();
  }

  /** Set SFX volume (0–1). Updates gain node and persists. */
  setSFXVolume(v: number): void {
    this.settings.sfxVolume = Math.max(0, Math.min(1, v));
    if (this.sfxGain && !this.settings.sfxMuted) {
      this.sfxGain.gain.value = this.settings.sfxVolume;
    }
    this.saveSettings();
  }

  /** Toggle BGM mute. Persists. */
  toggleMusicMute(): void {
    this.settings.bgmMuted = !this.settings.bgmMuted;
    if (this.musicGain) {
      this.musicGain.gain.value = this.settings.bgmMuted ? 0 : this.settings.bgmVolume;
    }
    this.saveSettings();
  }

  /** Toggle SFX mute. Persists. */
  toggleSFXMute(): void {
    this.settings.sfxMuted = !this.settings.sfxMuted;
    if (this.sfxGain) {
      this.sfxGain.gain.value = this.settings.sfxMuted ? 0 : this.settings.sfxVolume;
    }
    this.saveSettings();
  }

  /** Return a shallow copy of current settings. */
  getSettings(): AudioSettings {
    return { ...this.settings };
  }

  /** Return the AudioLoader instance (for BootScene to decode audio into). */
  getLoader(): AudioLoader {
    return this.loader;
  }

  /** Play a specific zone+state track combination (for jukebox). */
  playTrack(zoneId: string, state: MusicState): void {
    this.desiredZone = zoneId;
    this.desiredState = state;
    void this.unlock().then(() => this.applyDesiredMusic(true));
  }

  /** Temporarily mute/unmute music without persisting (for jukebox pause). */
  setMusicTempMute(muted: boolean): void {
    if (this.musicGain) {
      this.musicGain.gain.value = muted ? 0 : (this.settings.bgmMuted ? 0 : this.settings.bgmVolume);
    }
  }

  // ---------------------------------------------------------------------------
  // AudioContext — lazy init
  // ---------------------------------------------------------------------------

  /**
   * Returns the AudioContext, creating it on first call.
   * Also creates the master gain nodes and resumes a suspended context.
   */
  private setupUnlockGesture(): void {
    if (typeof window === 'undefined' || this.unlockGestureArmed || this.lifecycle === 'ready') return;
    this.unlockGestureArmed = true;
    window.addEventListener('pointerdown', this.unlockGestureHandler, { capture: true });
    window.addEventListener('keydown', this.unlockGestureHandler, { capture: true });
  }

  private removeUnlockGesture(): void {
    if (typeof window === 'undefined' || !this.unlockGestureArmed) return;
    this.unlockGestureArmed = false;
    window.removeEventListener('pointerdown', this.unlockGestureHandler, { capture: true });
    window.removeEventListener('keydown', this.unlockGestureHandler, { capture: true });
  }

  private unlock(): Promise<void> {
    if (this.lifecycle === 'ready' && this.ctx?.state === 'running') return Promise.resolve();
    if (this.unlockPromise) return this.unlockPromise;
    if (typeof AudioContext === 'undefined') {
      this.lifecycle = 'failed';
      return Promise.resolve();
    }

    this.lifecycle = 'unlocking';
    const attempt = (async () => {
      if (!this.ctx || this.ctx.state === 'closed') {
        this.ctx = new AudioContext();
        this.ctx.onstatechange = () => {
          if (this.ctx?.state === 'suspended') {
            this.lifecycle = 'locked';
            this.setupUnlockGesture();
          }
        };

        this.musicGain = this.ctx.createGain();
        this.musicGain.gain.value = this.settings.bgmMuted ? 0 : this.settings.bgmVolume;
        this.musicGain.connect(this.ctx.destination);

        this.sfxGain = this.ctx.createGain();
        this.sfxGain.gain.value = this.settings.sfxMuted ? 0 : this.settings.sfxVolume;
        this.sfxGain.connect(this.ctx.destination);
      }

      await this.ctx.resume();
      this.lifecycle = this.ctx.state === 'running' ? 'ready' : 'failed';
      if (this.lifecycle === 'ready') this.applyDesiredMusic(true);
    })().catch(async () => {
      this.lifecycle = 'failed';
      if (this.ctx) await this.ctx.close().catch(() => undefined);
      this.ctx = null;
      this.musicGain = null;
      this.sfxGain = null;
    }).finally(() => {
      this.unlockPromise = null;
      if (this.lifecycle === 'failed') this.setupUnlockGesture();
    });
    this.unlockPromise = attempt;
    return attempt;
  }

  private getReadyContext(): AudioContext | null {
    return this.lifecycle === 'ready' ? this.ctx : null;
  }

  private applyDesiredMusic(force = false): void {
    const ctx = this.getReadyContext();
    if (!ctx || !this.musicGain) return;
    this.musicEngine.setZone(ctx, this.musicGain, this.desiredZone, force);
    this.musicEngine.setState(ctx, this.musicGain, this.desiredState);
    void this.loadZoneMusicBuffers(this.desiredZone, ctx);
  }

  // ---------------------------------------------------------------------------
  // EventBus wiring
  // ---------------------------------------------------------------------------

  private setupEventListeners(): void {
    // --- Combat ---
    EventBus.on(GameEvents.COMBAT_DAMAGE, (payload: { isDodged?: boolean; isCrit?: boolean }) => {
      if (payload?.isDodged) {
        this.playSFX('miss');
      } else if (payload?.isCrit) {
        this.playSFX('crit');
      } else {
        this.playSFX('hit');
      }
    });

    // --- Player progression ---
    EventBus.on(GameEvents.PLAYER_LEVEL_UP, () => {
      this.playSFX('levelup');
    });

    EventBus.on(GameEvents.PLAYER_DIED, () => {
      this.playSFX('player_death');
    });

    EventBus.on(GameEvents.DODGE_STARTED, () => {
      this.playSFX('dodge');
    });

    EventBus.on(GameEvents.SPIRIT_RESONANCE_STARTED, () => {
      this.playSFX('resonance');
    });

    // --- Monsters ---
    EventBus.on(GameEvents.MONSTER_DIED, () => {
      this.playSFX('monster_death');
    });

    // --- Loot ---
    EventBus.on(GameEvents.ITEM_PICKED, (payload: { item?: { quality?: string } }) => {
      switch (payload?.item?.quality) {
        case 'magic':
          this.playSFX('loot_magic');
          break;
        case 'rare':
          this.playSFX('loot_rare');
          break;
        case 'legendary':
        case 'set':
          this.playSFX('loot_legendary');
          break;
        default:
          this.playSFX('loot_common');
          break;
      }
    });

    // --- Skills ---
    EventBus.on(GameEvents.SKILL_USED, (payload: { damageType?: string }) => {
      switch (payload?.damageType) {
        case 'fire':
          this.playSFX('skill_fire');
          break;
        case 'ice':
          this.playSFX('skill_ice');
          break;
        case 'lightning':
          this.playSFX('skill_lightning');
          break;
        case 'arcane':
        case 'poison':
          this.playSFX('skill_buff');
          break;
        default:
          this.playSFX('skill_melee');
          break;
      }
    });

    // --- Zone / Music ---
    EventBus.on(GameEvents.ZONE_ENTERED, (payload: { mapId?: string }) => {
      if (!payload?.mapId) return;
      this.desiredZone = payload.mapId;
      this.desiredState = 'explore';
      this.applyDesiredMusic();
    });

    EventBus.on(GameEvents.COMBAT_STATE_CHANGED, (payload: { inCombat?: boolean }) => {
      this.desiredState = payload?.inCombat ? 'combat' : 'explore';
      this.applyDesiredMusic();
    });

    // --- Quests ---
    EventBus.on(GameEvents.QUEST_COMPLETED, () => {
      this.playSFX('quest_complete');
    });

    EventBus.on(GameEvents.QUEST_ACCEPTED, () => {
      this.playSFX('npc_interact');
    });

    EventBus.on(GameEvents.QUEST_TURNED_IN, () => {
      this.playSFX('quest_complete');
    });

    // --- NPC / UI panels ---
    EventBus.on(GameEvents.NPC_INTERACT, () => {
      this.playSFX('npc_interact');
    });

    EventBus.on(GameEvents.SHOP_OPEN, () => {
      this.playSFX('panel_open');
    });

    EventBus.on(GameEvents.INVENTORY_OPEN, () => {
      this.playSFX('panel_open');
    });

    EventBus.on(GameEvents.INVENTORY_CLOSE, () => {
      this.playSFX('panel_close');
    });

    EventBus.on(GameEvents.UI_TOGGLE_PANEL, () => {
      this.playSFX('click');
    });
  }

  // ---------------------------------------------------------------------------
  // Settings persistence
  // ---------------------------------------------------------------------------

  private loadSettings(): AudioSettings {
    try {
      const raw = localStorage.getItem(STORAGE_KEY);
      if (raw) {
        const parsed = JSON.parse(raw) as Partial<AudioSettings>;
        return {
          bgmVolume: typeof parsed.bgmVolume === 'number' ? parsed.bgmVolume : DEFAULT_SETTINGS.bgmVolume,
          sfxVolume: typeof parsed.sfxVolume === 'number' ? parsed.sfxVolume : DEFAULT_SETTINGS.sfxVolume,
          bgmMuted:  typeof parsed.bgmMuted  === 'boolean' ? parsed.bgmMuted  : DEFAULT_SETTINGS.bgmMuted,
          sfxMuted:  typeof parsed.sfxMuted  === 'boolean' ? parsed.sfxMuted  : DEFAULT_SETTINGS.sfxMuted,
        };
      }
    } catch (_) {
      // Ignore parse errors — fall through to defaults.
    }
    return { ...DEFAULT_SETTINGS };
  }

  private saveSettings(): void {
    try {
      localStorage.setItem(STORAGE_KEY, JSON.stringify(this.settings));
    } catch (_) {
      // Quota exceeded or private browsing — silently ignore.
    }
  }

  private async loadZoneMusicBuffers(zoneId: string, ctx: AudioContext): Promise<void> {
    const requestId = ++this.zoneMusicRequestId;
    const keepKeys = new Set(
      AudioManager.MUSIC_STATES.map((state) => `bgm_${zoneId}_${state}`)
    );

    // Keep only the current zone's music in memory.
    this.loader.releaseMatching((key) => key.startsWith('bgm_') && !keepKeys.has(key));

    await Promise.all(
      AudioManager.MUSIC_STATES.map(async (state) => {
        const manifestKey = `${zoneId}_${state}`;
        const relativeUrl = __BGM_MANIFEST__[manifestKey];
        if (!relativeUrl) return;
        const key = `bgm_${manifestKey}`;
        const url = `${import.meta.env.BASE_URL}${relativeUrl}`;
        await this.loader.loadAudioFromUrl(ctx, key, url);
      }),
    );

    if (requestId !== this.zoneMusicRequestId) return;
    if (!this.ctx || !this.musicGain) return;

    this.musicEngine.refresh(this.ctx, this.musicGain);
  }
}

// ---------------------------------------------------------------------------
// Singleton export
// ---------------------------------------------------------------------------

export const audioManager = new AudioManager();

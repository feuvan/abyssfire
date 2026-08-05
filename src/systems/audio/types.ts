export type SFXType = 'hit' | 'hit_heavy' | 'crit' | 'miss' | 'block' | 'player_hurt'
  | 'monster_death' | 'player_death'
  | 'dodge' | 'resonance'
  | 'skill_melee' | 'skill_fire' | 'skill_ice' | 'skill_lightning' | 'skill_heal' | 'skill_buff'
  | 'loot_common' | 'loot_magic' | 'loot_rare' | 'loot_legendary' | 'equip' | 'potion'
  | 'click' | 'panel_open' | 'panel_close' | 'error'
  | 'zone_transition' | 'quest_complete' | 'levelup' | 'npc_interact';

export type MusicState = 'explore' | 'combat' | 'victory';

export interface ZoneTheme {
  id: string;
  scale: number[];        // frequencies in Hz
  baseKey: number;        // root frequency
  tempo: number;          // BPM for rhythm
  padWaveform: OscillatorType;
  mood: 'pastoral' | 'mysterious' | 'epic' | 'exotic' | 'dark';
  padFilterCutoff?: number;   // override default padCutoff for mood
  padLFORate?: number;        // override default padLfoRate for mood
  // Layer gain overrides for zone-specific volume control
  padGain?: number;           // pad layer master gain, default 0.15
  melodyPeakGainMin?: number; // min peak gain for melody notes
  melodyPeakGainMax?: number; // max peak gain for melody notes
  chimePeakGainMin?: number;  // min peak gain for chime notes
  chimePeakGainMax?: number;  // max peak gain for chime notes
  // Effects chain parameters
  reverbMix?: number;         // wet level 0-1, default 0.25
  reverbDecay?: number;       // decay time in seconds, default 1.5
  reverbPreDelay?: number;    // pre-delay in seconds, default 0.01
  compressorThreshold?: number; // in dB, default -18
  compressorKnee?: number;    // in dB, default 6
  compressorRatio?: number;   // ratio, default 4
  compressorAttack?: number;  // in seconds, default 0.003
  compressorRelease?: number; // in seconds, default 0.25
}

/** Configuration for the effects chain applied to BGM output. */
export interface EffectsChainConfig {
  reverb: {
    mix: number;          // wet level 0-1
    decay: number;        // decay time in seconds
    preDelay: number;     // pre-delay in seconds
  };
  compressor: {
    threshold: number;    // in dB
    knee: number;         // in dB
    ratio: number;        // ratio
    attack: number;       // in seconds
    release: number;      // in seconds
  };
  limiter: {
    threshold: number;    // in dB, typically -1 to 0
    release: number;      // in seconds
  };
}

export interface AudioSettings {
  bgmVolume: number;      // 0-1
  sfxVolume: number;      // 0-1
  bgmMuted: boolean;
  sfxMuted: boolean;
}

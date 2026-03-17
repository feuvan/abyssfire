import Phaser from 'phaser';
import { SpriteGenerator } from '../graphics/SpriteGenerator';
import { SkillEffectSystem } from '../systems/SkillEffectSystem';
import { audioManager } from '../systems/audio/AudioManager';
// import { buildFrameSizeRegistry } from '../graphics/sprites/types';
// import { TEXTURE_SCALE } from '../config';

export class BootScene extends Phaser.Scene {
  constructor() {
    super({ key: 'BootScene' });
  }

  preload(): void {
    const { width, height } = this.cameras.main;

    // Dark vignette background
    const bg = this.add.graphics();
    bg.fillGradientStyle(0x0a0a14, 0x0a0a14, 0x1a1020, 0x1a1020, 1);
    bg.fillRect(0, 0, width, height);

    // Game title with glow
    const titleText = this.add.text(width / 2, height / 2 - 70, '渊  火', {
      fontSize: '36px', color: '#c0934a', fontFamily: '"Cinzel", serif', fontStyle: 'bold',
      stroke: '#000000', strokeThickness: 4,
    }).setOrigin(0.5).setAlpha(0);
    const subtitleText = this.add.text(width / 2, height / 2 - 36, 'A B Y S S F I R E', {
      fontSize: '12px', color: '#8a7060', fontFamily: '"Cinzel", serif',
    }).setOrigin(0.5).setAlpha(0);

    // Fade in title
    this.tweens.add({ targets: titleText, alpha: 1, duration: 600, ease: 'Power2' });
    this.tweens.add({ targets: subtitleText, alpha: 0.7, duration: 800, delay: 200, ease: 'Power2' });
    // Title breathing pulse
    this.tweens.add({ targets: titleText, alpha: 0.75, yoyo: true, repeat: -1, duration: 1500, ease: 'Sine.easeInOut' });

    // Floating ember particles during load
    for (let i = 0; i < 15; i++) {
      const ember = this.add.circle(
        width * 0.2 + Math.random() * width * 0.6,
        height + 10,
        1 + Math.random() * 2,
        Phaser.Display.Color.RandomRGB(180, 220).color,
      ).setAlpha(0);
      this.tweens.add({
        targets: ember,
        y: -10, alpha: { from: 0, to: 0.4 + Math.random() * 0.4 },
        x: `+=${-30 + Math.random() * 60}`,
        duration: 3000 + Math.random() * 4000,
        delay: Math.random() * 2000,
        repeat: -1,
        ease: 'Sine.easeIn',
      });
    }

    // Progress bar
    const barW = 300, barH = 6;
    const barY = height / 2 + 10;
    this.add.rectangle(width / 2, barY, barW, barH, 0x1a1a2e).setStrokeStyle(1, 0x333344);
    const fill = this.add.rectangle((width - barW) / 2 + 2, barY, 0, barH - 2, 0xc0934a).setOrigin(0, 0.5);
    const loadingText = this.add.text(width / 2, barY + 18, '锻造渊火...', {
      fontSize: '11px', color: '#8a7060', fontFamily: '"Cinzel", serif',
    }).setOrigin(0.5);
    this.load.on('progress', (v: number) => { fill.width = (barW - 4) * v; });
    this.load.on('complete', () => { loadingText.setText('准备就绪!'); });

    // Silently handle missing asset files — procedural fallbacks will fill gaps
    this.load.on('loaderror', (file: Phaser.Loader.File) => {
      console.debug(`[BootScene] Asset not found, will use fallback: ${file.key}`);
    });

    // ── External art assets (uncomment when PNGs are added to public/assets/) ──
    // Currently all sprites/tiles are procedurally generated.
    // To override with external art, add the PNG and uncomment the matching load:
    //
    // Entity spritesheets:
    // const registry = buildFrameSizeRegistry();
    // const s = TEXTURE_SCALE;
    // for (const [key, { frameWidth, frameHeight }] of Object.entries(registry)) {
    //   this.load.spritesheet(key, this.getAssetPath(key), { frameWidth: frameWidth * s, frameHeight: frameHeight * s });
    // }
    //
    // Decorations:  this.load.image('decor_tree', 'assets/sprites/decorations/decor_tree.png');
    // Effects:      this.load.image('loot_bag', 'assets/sprites/effects/loot_bag.png');
    // Tiles:        this.load.image('tile_grass', 'assets/tiles/tile_grass.png');
    // Tile variants: this.load.image('tile_grass_0', 'assets/tiles/tile_grass_0.png');

    // ── Audio assets ─────────────────────
    const zones = ['emerald_plains', 'twilight_forest', 'anvil_mountains', 'scorching_desert', 'abyss_rift'];
    const musicStates = ['explore', 'combat', 'victory'];
    for (const z of zones) {
      for (const s of musicStates) {
        this.load.audio(`bgm_${z}_${s}`, `assets/audio/bgm/${z}_${s}.mp3`);
      }
    }
    // SFX external overrides — only load if files are actually shipped.
    // Currently no SFX mp3 files exist; SFXEngine uses procedural synthesis.
    // Uncomment when real SFX assets are added to public/assets/audio/sfx/.
    // const sfxTypes = [
    //   'hit', 'hit_heavy', 'crit', 'miss', 'block', 'player_hurt', 'monster_death', 'player_death',
    //   'skill_melee', 'skill_fire', 'skill_ice', 'skill_lightning', 'skill_heal', 'skill_buff',
    //   'loot_common', 'loot_magic', 'loot_rare', 'loot_legendary', 'equip', 'potion',
    //   'click', 'panel_open', 'panel_close', 'error',
    //   'zone_transition', 'quest_complete', 'levelup', 'npc_interact',
    // ];
    // for (const t of sfxTypes) {
    //   this.load.audio(`sfx_${t}`, `assets/audio/sfx/${t}.mp3`);
    // }
  }

  create(): void {
    // Generate all procedural textures + sprite sheets + animations
    const generator = new SpriteGenerator(this);
    generator.generateAll();

    // Skill effect particle textures
    SkillEffectSystem.generateTextures(this);

    // Pass any successfully loaded audio files to the AudioLoader.
    // Phaser's WebAudioSoundManager decodes audio during loading, so the cache
    // contains AudioBuffer objects (not raw ArrayBuffers).
    const loader = audioManager.getLoader();
    const audioKeys = this.cache.audio.getKeys();
    for (const key of audioKeys) {
      const audioData = this.cache.audio.get(key);
      if (audioData instanceof AudioBuffer) {
        loader.storeBuffer(key, audioData);
      } else if (audioData instanceof ArrayBuffer) {
        const ctx = new AudioContext();
        loader.decodeAudio(ctx, key, audioData).catch(() => {
          console.debug(`[BootScene] Failed to decode audio: ${key}`);
        });
      }
    }

    this.scene.start('MenuScene');
  }

  private getAssetPath(key: string): string {
    if (key.startsWith('player_')) return `assets/sprites/players/${key}.png`;
    if (key.startsWith('monster_')) return `assets/sprites/monsters/${key}.png`;
    if (key.startsWith('npc_')) return `assets/sprites/npcs/${key}.png`;
    return `assets/sprites/${key}.png`;
  }
}

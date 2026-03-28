import Phaser from 'phaser';
import { GAME_WIDTH, GAME_HEIGHT, TEXTURE_SCALE, DPR } from '../config';
import { SaveSystem } from '../systems/SaveSystem';
import { AllClasses } from '../data/classes/index';
import { EventBus, GameEvents } from '../utils/EventBus';
import { audioManager } from '../systems/audio/AudioManager';
import type { SaveData } from '../data/types';
import { SpriteGenerator } from '../graphics/SpriteGenerator';
import { DifficultySystem, DIFFICULTY_ORDER } from '../systems/DifficultySystem';
import type { Difficulty } from '../systems/DifficultySystem';
import { t, setLocale, getLocale } from '../i18n';

function fs(basePx: number): string {
  return `${Math.round(basePx * DPR)}px`;
}
const px = (n: number) => Math.round(n * DPR);

const W = GAME_WIDTH * DPR;
const H = GAME_HEIGHT * DPR;

const JUKEBOX_TRACKS = [
  { titleKey: 'menu.jukebox.track.menu', zoneId: 'menu', state: 'explore' as const, duration: 120 },
  { titleKey: 'menu.jukebox.track.emerald_plains.explore', zoneId: 'emerald_plains', state: 'explore' as const, duration: 180 },
  { titleKey: 'menu.jukebox.track.emerald_plains.combat', zoneId: 'emerald_plains', state: 'combat' as const, duration: 150 },
  { titleKey: 'menu.jukebox.track.twilight_forest.explore', zoneId: 'twilight_forest', state: 'explore' as const, duration: 210 },
  { titleKey: 'menu.jukebox.track.twilight_forest.combat', zoneId: 'twilight_forest', state: 'combat' as const, duration: 150 },
  { titleKey: 'menu.jukebox.track.anvil_mountains.explore', zoneId: 'anvil_mountains', state: 'explore' as const, duration: 180 },
  { titleKey: 'menu.jukebox.track.anvil_mountains.combat', zoneId: 'anvil_mountains', state: 'combat' as const, duration: 150 },
  { titleKey: 'menu.jukebox.track.scorching_desert.explore', zoneId: 'scorching_desert', state: 'explore' as const, duration: 180 },
  { titleKey: 'menu.jukebox.track.scorching_desert.combat', zoneId: 'scorching_desert', state: 'combat' as const, duration: 150 },
  { titleKey: 'menu.jukebox.track.abyss_rift.explore', zoneId: 'abyss_rift', state: 'explore' as const, duration: 210 },
  { titleKey: 'menu.jukebox.track.abyss_rift.combat', zoneId: 'abyss_rift', state: 'combat' as const, duration: 180 },
];

function fmtTime(sec: number): string {
  const m = Math.floor(sec / 60);
  const s = Math.floor(sec % 60);
  return `${String(m).padStart(2, '0')}:${String(s).padStart(2, '0')}`;
}

export class MenuScene extends Phaser.Scene {
  private menuContainer: Phaser.GameObjects.Container | null = null;
  private classContainer: Phaser.GameObjects.Container | null = null;
  private helpContainer: Phaser.GameObjects.Container | null = null;
  private jukeboxContainer: Phaser.GameObjects.Container | null = null;
  private difficultyContainer: Phaser.GameObjects.Container | null = null;
  private creditsContainer: Phaser.GameObjects.Container | null = null;
  private langContainer: Phaser.GameObjects.Container | null = null;
  private titleContainer: Phaser.GameObjects.Container | null = null;

  /** Tracks current save for re-rendering main menu after locale change */
  private currentSave: SaveData | null = null;
  /** Tracks which panel is visible for LOCALE_CHANGED reactivity */
  private activePanel: 'menu' | 'class' | 'help' | 'jukebox' | 'credits' | 'difficulty' | 'lang' = 'menu';

  constructor() {
    super({ key: 'MenuScene' });
  }

  create(): void {
    const cx = W / 2;

    this.buildBackground(cx);
    this.buildTitle(cx);
    this.startBGM();
    this.checkForSaves();

    // Listen for locale changes to re-render the active panel
    EventBus.on(GameEvents.LOCALE_CHANGED, this.onLocaleChanged, this);
    this.events.once('shutdown', () => {
      EventBus.off(GameEvents.LOCALE_CHANGED, this.onLocaleChanged, this);
    });
  }

  private onLocaleChanged = (): void => {
    // Re-render title (subtitle changes per locale)
    this.rebuildTitle();

    // Re-render whichever panel is currently active
    switch (this.activePanel) {
      case 'menu':
        this.showMainMenu(this.currentSave);
        break;
      case 'class':
        this.classContainer?.destroy(); this.classContainer = null;
        this.showClassSelection();
        break;
      case 'help':
        this.helpContainer?.destroy(); this.helpContainer = null;
        this.showHelp();
        break;
      case 'jukebox':
        // Jukebox has internal timer state; full re-render would lose playback position.
        // Just close and re-open to keep it simple.
        this.jukeboxContainer?.destroy(); this.jukeboxContainer = null;
        this.showJukebox();
        break;
      case 'credits':
        this.creditsContainer?.destroy(); this.creditsContainer = null;
        this.showCredits();
        break;
      case 'difficulty':
        if (this.currentSave) {
          this.difficultyContainer?.destroy(); this.difficultyContainer = null;
          this.showDifficultySelector(this.currentSave);
        }
        break;
      case 'lang':
        this.langContainer?.destroy(); this.langContainer = null;
        this.showLanguageSelector();
        break;
    }
  };

  // ---------------------------------------------------------------------------
  // Background layers
  // ---------------------------------------------------------------------------

  private buildBackground(cx: number): void {
    // Layer 1 — Void gradient
    const bgGrad = this.add.graphics();
    bgGrad.fillGradientStyle(0x050508, 0x050508, 0x1a0808, 0x1a0808, 1);
    bgGrad.fillRect(0, 0, W, H);
    bgGrad.setDepth(0);

    // Layer 2 — Fire glow (pulsing radial ellipse at bottom-center)
    this.buildFireGlow(cx);

    // Layer 3 — Ember particles
    const bottomRect = new Phaser.Geom.Rectangle(0, H - px(20), W, px(20));
    const bottomZone = new Phaser.GameObjects.Particles.Zones.RandomZone(
      bottomRect as unknown as Phaser.Types.GameObjects.Particles.RandomZoneSource,
    );
    const embers = this.add.particles(0, 0, 'particle_flame', {
      emitZone: bottomZone,
      speed: { min: 15, max: 45 },
      angle: { min: 260, max: 280 },
      scale: { start: 0.4, end: 0.05 },
      alpha: { start: 0.7, end: 0 },
      lifespan: { min: 3000, max: 6000 },
      frequency: 100,
      tint: [0xff4400, 0xff6600, 0xff8800, 0xffaa00],
      blendMode: Phaser.BlendModes.ADD,
      gravityY: -10,
    });
    embers.setDepth(2);

    // Layer 4 — Spark particles
    const sparks = this.add.particles(0, 0, 'particle_spark', {
      emitZone: bottomZone,
      speed: { min: 30, max: 70 },
      angle: { min: 255, max: 285 },
      scale: { start: 0.3, end: 0 },
      alpha: { start: 0.9, end: 0 },
      lifespan: { min: 1500, max: 3500 },
      frequency: 300,
      tint: [0xffcc44, 0xffffaa],
      blendMode: Phaser.BlendModes.ADD,
    });
    sparks.setDepth(3);

    // Layer 5 — Smoke haze
    this.buildSmokeHaze();

    // Layer 6 — Title fire glow
    this.buildTitleGlow(cx);
  }

  private buildFireGlow(cx: number): void {
    const glowKey = 'menu_fire_glow';
    if (!this.textures.exists(glowKey)) {
      const canvas = this.textures.createCanvas(glowKey, 512, 512)!;
      const ctx2d = canvas.getContext();
      const gradient = ctx2d.createRadialGradient(256, 256, 0, 256, 256, 256);
      gradient.addColorStop(0, 'rgba(255, 100, 20, 0.6)');
      gradient.addColorStop(0.4, 'rgba(200, 50, 0, 0.3)');
      gradient.addColorStop(0.7, 'rgba(120, 20, 0, 0.1)');
      gradient.addColorStop(1, 'rgba(60, 10, 0, 0)');
      ctx2d.fillStyle = gradient;
      ctx2d.fillRect(0, 0, 512, 512);
      canvas.refresh();
    }

    const glow = this.add.image(cx, H + px(40), glowKey);
    glow.setDisplaySize(W * 1.2, px(500));
    glow.setBlendMode(Phaser.BlendModes.ADD);
    glow.setAlpha(0.2);
    glow.setDepth(1);

    this.tweens.add({
      targets: glow,
      alpha: { from: 0.15, to: 0.30 },
      duration: 8000,
      ease: 'Sine.easeInOut',
      yoyo: true,
      repeat: -1,
    });
    this.tweens.add({
      targets: glow,
      scaleX: { from: glow.scaleX * 0.95, to: glow.scaleX * 1.05 },
      scaleY: { from: glow.scaleY * 0.95, to: glow.scaleY * 1.05 },
      duration: 10000,
      ease: 'Sine.easeInOut',
      yoyo: true,
      repeat: -1,
    });
  }

  private buildSmokeHaze(): void {
    const smokeCount = 5;
    for (let i = 0; i < smokeCount; i++) {
      const radius = 200 + Math.random() * 100;
      const startX = Math.random() * W;
      const startY = H * 0.3 + Math.random() * H * 0.4;
      const alpha = 0.03 + Math.random() * 0.03;

      const smoke = this.add.circle(startX, startY, radius, 0x222222, alpha);
      smoke.setDepth(4);

      const driftDuration = 20000 + Math.random() * 10000;
      const direction = Math.random() < 0.5 ? 1 : -1;
      this.tweens.add({
        targets: smoke,
        x: startX + direction * (W * 0.4),
        duration: driftDuration,
        ease: 'Linear',
        yoyo: true,
        repeat: -1,
      });
    }
  }

  private buildTitleGlow(cx: number): void {
    const titleGlowKey = 'menu_title_glow';
    if (!this.textures.exists(titleGlowKey)) {
      const canvas = this.textures.createCanvas(titleGlowKey, 256, 256)!;
      const ctx2d = canvas.getContext();
      const gradient = ctx2d.createRadialGradient(128, 128, 0, 128, 128, 128);
      gradient.addColorStop(0, 'rgba(255, 160, 40, 0.4)');
      gradient.addColorStop(0.5, 'rgba(200, 100, 20, 0.15)');
      gradient.addColorStop(1, 'rgba(100, 50, 0, 0)');
      ctx2d.fillStyle = gradient;
      ctx2d.fillRect(0, 0, 256, 256);
      canvas.refresh();
    }

    const titleGlow = this.add.image(cx, px(150), titleGlowKey);
    titleGlow.setDisplaySize(px(400), px(200));
    titleGlow.setBlendMode(Phaser.BlendModes.ADD);
    titleGlow.setAlpha(0.1);
    titleGlow.setDepth(5);

    this.tweens.add({
      targets: titleGlow,
      alpha: { from: 0.08, to: 0.15 },
      duration: 8000,
      ease: 'Sine.easeInOut',
      yoyo: true,
      repeat: -1,
    });
  }

  // ---------------------------------------------------------------------------
  // Title & decorative elements
  // ---------------------------------------------------------------------------

  private buildTitle(cx: number): void {
    this.titleContainer = this.add.container(0, 0).setDepth(10);

    // Decorative line above title
    const lineY = px(80);
    const lineGfx = this.add.graphics();
    lineGfx.lineStyle(1, 0xc0934a, 0.3);
    lineGfx.beginPath();
    lineGfx.moveTo(cx - px(200), lineY);
    lineGfx.lineTo(cx + px(200), lineY);
    lineGfx.strokePath();
    lineGfx.fillStyle(0xc0934a, 0.5);
    lineGfx.fillCircle(cx - px(200), lineY, px(2));
    lineGfx.fillCircle(cx + px(200), lineY, px(2));
    this.titleContainer.add(lineGfx);

    // Title — ABYSSFIRE never changes
    this.titleContainer.add(this.add.text(cx, px(130), t('menu.title'), {
      fontSize: fs(52),
      color: '#c0934a',
      fontFamily: '"Cinzel", serif',
      fontStyle: 'bold',
      stroke: '#3a2a10',
      strokeThickness: Math.round(4 * DPR),
    }).setOrigin(0.5));

    // Subtitle — locale-dependent
    this.titleContainer.add(this.add.text(cx, px(188), t('menu.subtitle'), {
      fontSize: fs(32),
      color: '#d4a84b',
      fontFamily: '"Noto Sans SC", "Noto Sans TC", sans-serif',
      fontStyle: 'bold',
      stroke: '#2a1a08',
      strokeThickness: Math.round(3 * DPR),
    }).setOrigin(0.5));

    // Decorative line below title
    const lineGfx2 = this.add.graphics();
    lineGfx2.lineStyle(1, 0xc0934a, 0.3);
    lineGfx2.beginPath();
    lineGfx2.moveTo(cx - px(160), px(215));
    lineGfx2.lineTo(cx + px(160), px(215));
    lineGfx2.strokePath();
    this.titleContainer.add(lineGfx2);

    // Version
    this.titleContainer.add(this.add.text(cx, H - px(20), 'v0.22.0', {
      fontSize: fs(13),
      color: '#333340',
      fontFamily: '"Cinzel", serif',
    }).setOrigin(0.5));
  }

  private rebuildTitle(): void {
    const cx = W / 2;
    if (this.titleContainer) {
      this.titleContainer.destroy();
      this.titleContainer = null;
    }
    this.buildTitle(cx);
  }

  // ---------------------------------------------------------------------------
  // BGM
  // ---------------------------------------------------------------------------

  private startBGM(): void {
    // Try starting immediately — works if AudioContext already exists (e.g. returning from game)
    EventBus.emit(GameEvents.ZONE_ENTERED, { mapId: 'menu' });

    // DOM-level handler ensures ctx.resume() runs inside a real user gesture call stack.
    // Phaser's input system defers callbacks to the game loop, which browsers don't
    // consider a user gesture — so AudioContext.resume() gets silently rejected.
    const resumeAudio = () => {
      audioManager.ensureContext();
      document.removeEventListener('pointerdown', resumeAudio);
      document.removeEventListener('keydown', resumeAudio);
    };
    document.addEventListener('pointerdown', resumeAudio);
    document.addEventListener('keydown', resumeAudio);

    this.events.once('shutdown', () => {
      document.removeEventListener('pointerdown', resumeAudio);
      document.removeEventListener('keydown', resumeAudio);
    });
  }

  // ---------------------------------------------------------------------------
  // Menu logic
  // ---------------------------------------------------------------------------

  private async checkForSaves(): Promise<void> {
    const saveSystem = new SaveSystem();
    const save = await saveSystem.loadAutoSave();
    this.showMainMenu(save ?? null);
  }

  private showMainMenu(save: SaveData | null): void {
    if (this.menuContainer) { this.menuContainer.destroy(); }
    this.menuContainer = this.add.container(0, 0).setDepth(10);
    this.currentSave = save;
    this.activePanel = 'menu';

    const cx = W / 2;
    let y = save ? px(300) : px(340);

    if (save) {
      // "Continue" button — shows class name + level
      const classData = AllClasses[save.classId];
      const className = classData?.name ?? save.classId;
      const label = t('menu.continue', { class: className, level: String(save.player.level) });

      const bg = this.add.rectangle(cx, y, px(320), px(65), 0x12121e, 0.9)
        .setStrokeStyle(1.5, 0xc0934a, 0.8).setInteractive({ useHandCursor: true });
      bg.on('pointerover', () => { bg.setStrokeStyle(2, 0xc0934a, 1); bg.setFillStyle(0x1a1a2e, 0.95); });
      bg.on('pointerout', () => { bg.setStrokeStyle(1.5, 0xc0934a, 0.8); bg.setFillStyle(0x12121e, 0.9); });
      bg.on('pointerdown', () => {
        // Show difficulty selector when save has non-normal difficulty OR completed difficulties.
        // Also derive completedDifficulties from persisted difficulty for migrated saves.
        save.completedDifficulties = DifficultySystem.deriveCompletedDifficulties(
          (save.difficulty as any) ?? 'normal',
          save.completedDifficulties,
        );
        if (DifficultySystem.shouldShowDifficultySelector(save.difficulty, save.completedDifficulties)) {
          this.menuContainer?.destroy(); this.menuContainer = null;
          this.showDifficultySelector(save);
        } else {
          this.loadGame(save);
        }
      });
      this.menuContainer.add(bg);

      this.menuContainer.add(this.add.text(cx, y - px(6), label, {
        fontSize: fs(20), color: '#e8e0d4', fontFamily: '"Cinzel", "Noto Sans SC", "Noto Sans TC", serif',
      }).setOrigin(0.5));
      this.menuContainer.add(this.add.text(cx, y + px(16), t('menu.continueSubtitle'), {
        fontSize: fs(13), color: '#c0934a', fontFamily: '"Noto Sans SC", "Noto Sans TC", sans-serif',
      }).setOrigin(0.5));

      y += px(90);
    }

    // "New Game" button
    const newBg = this.add.rectangle(cx, y, px(320), px(55), 0x12121e, 0.9)
      .setStrokeStyle(1.5, 0x555566, 0.6).setInteractive({ useHandCursor: true });
    newBg.on('pointerover', () => { newBg.setStrokeStyle(2, 0x888899, 1); newBg.setFillStyle(0x1a1a2e, 0.95); });
    newBg.on('pointerout', () => { newBg.setStrokeStyle(1.5, 0x555566, 0.6); newBg.setFillStyle(0x12121e, 0.9); });
    newBg.on('pointerdown', () => {
      this.menuContainer?.destroy(); this.menuContainer = null;
      this.showClassSelection();
    });
    this.menuContainer.add(newBg);
    this.menuContainer.add(this.add.text(cx, y, t('menu.newGame'), {
      fontSize: fs(20), color: '#a0907a', fontFamily: '"Cinzel", "Noto Sans SC", "Noto Sans TC", serif',
    }).setOrigin(0.5));

    y += px(70);

    // "Help" button
    const helpBg = this.add.rectangle(cx, y, px(320), px(45), 0x12121e, 0.9)
      .setStrokeStyle(1.5, 0x555566, 0.4).setInteractive({ useHandCursor: true });
    helpBg.on('pointerover', () => { helpBg.setStrokeStyle(2, 0x888899, 0.8); helpBg.setFillStyle(0x1a1a2e, 0.95); });
    helpBg.on('pointerout', () => { helpBg.setStrokeStyle(1.5, 0x555566, 0.4); helpBg.setFillStyle(0x12121e, 0.9); });
    helpBg.on('pointerdown', () => this.showHelp());
    this.menuContainer.add(helpBg);
    this.menuContainer.add(this.add.text(cx, y, t('menu.help'), {
      fontSize: fs(16), color: '#888880', fontFamily: '"Noto Sans SC", "Noto Sans TC", sans-serif',
    }).setOrigin(0.5));

    y += px(60);

    // "Soundtrack" button
    const ostBg = this.add.rectangle(cx, y, px(320), px(45), 0x12121e, 0.9)
      .setStrokeStyle(1.5, 0x555566, 0.4).setInteractive({ useHandCursor: true });
    ostBg.on('pointerover', () => { ostBg.setStrokeStyle(2, 0x888899, 0.8); ostBg.setFillStyle(0x1a1a2e, 0.95); });
    ostBg.on('pointerout', () => { ostBg.setStrokeStyle(1.5, 0x555566, 0.4); ostBg.setFillStyle(0x12121e, 0.9); });
    ostBg.on('pointerdown', () => this.showJukebox());
    this.menuContainer.add(ostBg);
    this.menuContainer.add(this.add.text(cx, y, t('menu.ost'), {
      fontSize: fs(16), color: '#888880', fontFamily: '"Noto Sans SC", "Noto Sans TC", sans-serif',
    }).setOrigin(0.5));

    y += px(60);

    // "Credits" button
    const creditsBg = this.add.rectangle(cx, y, px(320), px(45), 0x12121e, 0.9)
      .setStrokeStyle(1.5, 0x555566, 0.4).setInteractive({ useHandCursor: true });
    creditsBg.on('pointerover', () => { creditsBg.setStrokeStyle(2, 0x888899, 0.8); creditsBg.setFillStyle(0x1a1a2e, 0.95); });
    creditsBg.on('pointerout', () => { creditsBg.setStrokeStyle(1.5, 0x555566, 0.4); creditsBg.setFillStyle(0x12121e, 0.9); });
    creditsBg.on('pointerdown', () => this.showCredits());
    this.menuContainer.add(creditsBg);
    this.menuContainer.add(this.add.text(cx, y, t('menu.credits'), {
      fontSize: fs(16), color: '#888880', fontFamily: '"Noto Sans SC", "Noto Sans TC", sans-serif',
    }).setOrigin(0.5));

    y += px(60);

    // "Language" button — after Credits
    const langBg = this.add.rectangle(cx, y, px(320), px(45), 0x12121e, 0.9)
      .setStrokeStyle(1.5, 0x555566, 0.4).setInteractive({ useHandCursor: true });
    langBg.on('pointerover', () => { langBg.setStrokeStyle(2, 0x888899, 0.8); langBg.setFillStyle(0x1a1a2e, 0.95); });
    langBg.on('pointerout', () => { langBg.setStrokeStyle(1.5, 0x555566, 0.4); langBg.setFillStyle(0x12121e, 0.9); });
    langBg.on('pointerdown', () => {
      this.menuContainer?.destroy(); this.menuContainer = null;
      this.showLanguageSelector();
    });
    this.menuContainer.add(langBg);
    this.menuContainer.add(this.add.text(cx, y, t('menu.language'), {
      fontSize: fs(16), color: '#888880', fontFamily: '"Noto Sans SC", "Noto Sans TC", sans-serif',
    }).setOrigin(0.5));
  }

  private showClassSelection(): void {
    this.classContainer = this.add.container(0, 0).setDepth(10);
    this.activePanel = 'class';
    const cx = W / 2;

    this.classContainer.add(this.add.text(cx, px(260), t('menu.classSelect.title'), {
      fontSize: fs(20),
      color: '#a0907a',
      fontFamily: '"Noto Sans SC", "Noto Sans TC", sans-serif',
    }).setOrigin(0.5));

    const classes = [
      { id: 'warrior', nameKey: 'menu.classSelect.warrior.name', descKey: 'menu.classSelect.warrior.desc', color: 0xc0392b, accent: '#e74c3c' },
      { id: 'mage', nameKey: 'menu.classSelect.mage.name', descKey: 'menu.classSelect.mage.desc', color: 0x6c3483, accent: '#9b59b6' },
      { id: 'rogue', nameKey: 'menu.classSelect.rogue.name', descKey: 'menu.classSelect.rogue.desc', color: 0x1e8449, accent: '#27ae60' },
    ];

    classes.forEach((cls, i) => {
      const y = px(320) + i * px(80);
      const bg = this.add.rectangle(cx, y, px(320), px(65), 0x12121e, 0.9)
        .setStrokeStyle(1.5, cls.color, 0.6)
        .setInteractive({ useHandCursor: true });

      // Animated class icon preview
      const spriteKey = `player_${cls.id}`;
      SpriteGenerator.ensurePlayerSheet(this, cls.id);
      if (this.textures.exists(spriteKey)) {
        const preview = this.add.sprite(cx - px(130), y, spriteKey, 0).setScale(0.7 / TEXTURE_SCALE);
        const idleKey = `${spriteKey}_idle`;
        if (this.anims.exists(idleKey)) preview.play(idleKey);
        this.classContainer!.add(preview);
        // Subtle breathing animation
        this.tweens.add({
          targets: preview, scaleY: (0.7 / TEXTURE_SCALE) * 1.04,
          duration: 1200, yoyo: true, repeat: -1, ease: 'Sine.easeInOut',
        });
      }

      this.classContainer!.add(bg);
      this.classContainer!.add(this.add.text(cx, y - px(12), t(cls.nameKey), {
        fontSize: fs(20),
        color: '#e8e0d4',
        fontFamily: '"Cinzel", "Noto Sans SC", "Noto Sans TC", serif',
      }).setOrigin(0.5));

      this.classContainer!.add(this.add.text(cx, y + px(12), t(cls.descKey), {
        fontSize: fs(14),
        color: cls.accent,
        fontFamily: '"Noto Sans SC", "Noto Sans TC", sans-serif',
      }).setOrigin(0.5));

      bg.on('pointerover', () => {
        bg.setStrokeStyle(2, cls.color, 1);
        bg.setFillStyle(0x1a1a2e, 0.95);
        // Play attack anim on hover
        const atkKey = `${spriteKey}_attack`;
        const spr = this.classContainer?.list.find(
          c => c instanceof Phaser.GameObjects.Sprite && (c as Phaser.GameObjects.Sprite).texture.key === spriteKey
        ) as Phaser.GameObjects.Sprite | undefined;
        if (spr && this.anims.exists(atkKey)) {
          spr.play(atkKey);
          spr.once('animationcomplete', () => {
            const idleAnim = `${spriteKey}_idle`;
            if (this.anims.exists(idleAnim)) spr.play(idleAnim);
          });
        }
      });
      bg.on('pointerout', () => {
        bg.setStrokeStyle(1.5, cls.color, 0.6);
        bg.setFillStyle(0x12121e, 0.9);
      });
      bg.on('pointerdown', () => this.startGame(cls.id));
    });

    // Back button
    const backBtn = this.add.text(cx, px(570), t('menu.back'), {
      fontSize: fs(14), color: '#888', fontFamily: '"Noto Sans SC", "Noto Sans TC", sans-serif',
    }).setOrigin(0.5).setInteractive({ useHandCursor: true });
    backBtn.on('pointerdown', () => {
      this.classContainer?.destroy(); this.classContainer = null;
      this.checkForSaves();
    });
    this.classContainer.add(backBtn);
  }

  private showHelp(): void {
    if (this.helpContainer) { this.helpContainer.destroy(); }
    this.helpContainer = this.add.container(0, 0).setDepth(20);
    this.activePanel = 'help';

    const cx = W / 2;
    const panelW = px(460);
    const panelH = px(520);
    const panelX = cx;
    const panelY = H / 2;

    // Dimmed backdrop
    const backdrop = this.add.rectangle(cx, H / 2, W, H, 0x000000, 0.6).setInteractive();
    this.helpContainer.add(backdrop);

    // Panel background
    const panel = this.add.rectangle(panelX, panelY, panelW, panelH, 0x0e0e1a, 0.95)
      .setStrokeStyle(1.5, 0xc0934a, 0.6);
    this.helpContainer.add(panel);

    // Title
    this.helpContainer.add(this.add.text(panelX, panelY - panelH / 2 + px(24), t('menu.helpPanel.title'), {
      fontSize: fs(22), color: '#c0934a', fontFamily: '"Cinzel", "Noto Sans SC", "Noto Sans TC", serif', fontStyle: 'bold',
    }).setOrigin(0.5));

    // Decorative line
    const lineGfx = this.add.graphics();
    lineGfx.lineStyle(1, 0xc0934a, 0.4);
    lineGfx.beginPath();
    lineGfx.moveTo(panelX - px(160), panelY - panelH / 2 + px(44));
    lineGfx.lineTo(panelX + px(160), panelY - panelH / 2 + px(44));
    lineGfx.strokePath();
    this.helpContainer.add(lineGfx);

    const categories: { titleKey: string; keys: [string, string][] }[] = [
      {
        titleKey: 'menu.helpPanel.cat.movement',
        keys: [
          ['W / A / S / D', t('menu.helpPanel.movement.wasd')],
          ['Mouse LMB', t('menu.helpPanel.movement.mouse')],
        ],
      },
      {
        titleKey: 'menu.helpPanel.cat.combat',
        keys: [
          ['1 - 6', t('menu.helpPanel.combat.skills')],
          ['TAB', t('menu.helpPanel.combat.autoCombat')],
          ['R / RMB', t('menu.helpPanel.combat.teleport')],
        ],
      },
      {
        titleKey: 'menu.helpPanel.cat.ui',
        keys: [
          ['I', t('menu.helpPanel.ui.inventory')],
          ['C', t('menu.helpPanel.ui.character')],
          ['K', t('menu.helpPanel.ui.skillTree')],
          ['J', t('menu.helpPanel.ui.questLog')],
          ['M', t('menu.helpPanel.ui.map')],
          ['H', t('menu.helpPanel.ui.homestead')],
          ['O', t('menu.helpPanel.ui.audio')],
          ['ESC', t('menu.helpPanel.ui.escape')],
        ],
      },
    ];

    let y = panelY - panelH / 2 + px(60);
    const leftX = panelX - panelW / 2 + px(30);
    const rightX = panelX + panelW / 2 - px(30);

    for (const cat of categories) {
      // Category title
      this.helpContainer.add(this.add.text(leftX, y, t(cat.titleKey), {
        fontSize: fs(14), color: '#d4a84b', fontFamily: '"Noto Sans SC", "Noto Sans TC", sans-serif', fontStyle: 'bold',
      }).setOrigin(0, 0.5));
      y += px(22);

      for (const [key, desc] of cat.keys) {
        // Key label
        this.helpContainer.add(this.add.text(leftX + px(8), y, key, {
          fontSize: fs(12), color: '#e0d8cc', fontFamily: '"Noto Sans SC", "Noto Sans TC", sans-serif',
        }).setOrigin(0, 0.5));
        // Description
        this.helpContainer.add(this.add.text(rightX, y, desc, {
          fontSize: fs(12), color: '#888880', fontFamily: '"Noto Sans SC", "Noto Sans TC", sans-serif',
        }).setOrigin(1, 0.5));
        y += px(18);
      }
      y += px(10);
    }

    // Close button
    const closeBtn = this.add.text(panelX, panelY + panelH / 2 - px(24), t('menu.back'), {
      fontSize: fs(14), color: '#888', fontFamily: '"Noto Sans SC", "Noto Sans TC", sans-serif',
    }).setOrigin(0.5).setInteractive({ useHandCursor: true });
    closeBtn.on('pointerdown', () => {
      this.helpContainer?.destroy();
      this.helpContainer = null;
      this.activePanel = 'menu';
    });
    backdrop.on('pointerdown', () => {
      this.helpContainer?.destroy();
      this.helpContainer = null;
      this.activePanel = 'menu';
    });
    this.helpContainer.add(closeBtn);
  }

  private showJukebox(): void {
    if (this.jukeboxContainer) { this.jukeboxContainer.destroy(); }
    this.jukeboxContainer = this.add.container(0, 0).setDepth(20);
    this.activePanel = 'jukebox';

    const cx = W / 2;
    const panelW = px(460);
    const panelH = px(510);
    const panelX = cx;
    const panelY = H / 2;
    const panelTop = panelY - panelH / 2;
    const panelLeft = panelX - panelW / 2;
    const innerLeft = panelLeft + px(20);
    const innerRight = panelLeft + panelW - px(20);
    const innerW = panelW - px(40);

    // ---- State ----
    let trackIndex = 0;
    let elapsed = 0;
    let paused = false;

    // ---- Backdrop ----
    const backdrop = this.add.rectangle(cx, H / 2, W, H, 0x000000, 0.6).setInteractive();
    this.jukeboxContainer.add(backdrop);

    // ---- Panel ----
    this.jukeboxContainer.add(
      this.add.rectangle(panelX, panelY, panelW, panelH, 0x0a0a14, 0.96)
        .setStrokeStyle(1.5, 0xc0934a, 0.5)
    );

    // ---- Header ----
    this.jukeboxContainer.add(this.add.text(panelX, panelTop + px(20), t('menu.jukebox.header'), {
      fontSize: fs(18), color: '#c0934a', fontFamily: '"Cinzel", serif', fontStyle: 'bold',
    }).setOrigin(0.5));

    const totalDur = JUKEBOX_TRACKS.reduce((sum, tr) => sum + tr.duration, 0);
    this.jukeboxContainer.add(this.add.text(panelX, panelTop + px(38),
      t('menu.jukebox.subtitle', { count: String(JUKEBOX_TRACKS.length), duration: fmtTime(totalDur) }), {
      fontSize: fs(11), color: '#666660', fontFamily: '"Noto Sans SC", "Noto Sans TC", sans-serif',
    }).setOrigin(0.5));

    const hdrLine = this.add.graphics();
    hdrLine.lineStyle(1, 0xc0934a, 0.3);
    hdrLine.beginPath();
    hdrLine.moveTo(innerLeft, panelTop + px(52));
    hdrLine.lineTo(innerRight, panelTop + px(52));
    hdrLine.strokePath();
    this.jukeboxContainer.add(hdrLine);

    // ---- Track list ----
    const listTop = panelTop + px(60);
    const rowH = px(30);

    // Alternating row backgrounds
    for (let i = 0; i < JUKEBOX_TRACKS.length; i++) {
      if (i % 2 === 1) {
        this.jukeboxContainer.add(
          this.add.rectangle(panelX, listTop + i * rowH + rowH / 2, panelW - px(12), rowH, 0x0f0f1c, 0.5)
        );
      }
    }

    // Active track highlight
    const highlight = this.add.rectangle(panelX, listTop + rowH / 2, panelW - px(12), rowH, 0xc0934a, 0.10);
    this.jukeboxContainer.add(highlight);

    const numTexts: Phaser.GameObjects.Text[] = [];
    const titleTexts: Phaser.GameObjects.Text[] = [];
    const durTexts: Phaser.GameObjects.Text[] = [];

    for (let i = 0; i < JUKEBOX_TRACKS.length; i++) {
      const track = JUKEBOX_TRACKS[i];
      const rowY = listTop + i * rowH + rowH / 2;

      const hit = this.add.rectangle(panelX, rowY, panelW - px(12), rowH, 0x000000, 0)
        .setInteractive({ useHandCursor: true });
      hit.on('pointerover', () => { if (i !== trackIndex) hit.setFillStyle(0x222230, 0.5); });
      hit.on('pointerout', () => hit.setFillStyle(0x000000, 0));
      hit.on('pointerdown', () => doPlay(i));
      this.jukeboxContainer!.add(hit);

      const num = this.add.text(innerLeft, rowY, String(i + 1).padStart(2, '0'), {
        fontSize: fs(11), color: '#555550', fontFamily: '"Cinzel", monospace',
      }).setOrigin(0, 0.5);
      numTexts.push(num);
      this.jukeboxContainer!.add(num);

      const title = this.add.text(innerLeft + px(28), rowY, t(track.titleKey), {
        fontSize: fs(13), color: '#999990', fontFamily: '"Noto Sans SC", "Noto Sans TC", sans-serif',
      }).setOrigin(0, 0.5);
      titleTexts.push(title);
      this.jukeboxContainer!.add(title);

      const dur = this.add.text(innerRight, rowY, fmtTime(track.duration), {
        fontSize: fs(11), color: '#555550', fontFamily: '"Cinzel", monospace',
      }).setOrigin(1, 0.5);
      durTexts.push(dur);
      this.jukeboxContainer!.add(dur);
    }

    // ---- Separator ----
    const listEnd = listTop + JUKEBOX_TRACKS.length * rowH;
    const sepGfx = this.add.graphics();
    sepGfx.lineStyle(1, 0x333340, 0.5);
    sepGfx.beginPath();
    sepGfx.moveTo(innerLeft, listEnd + px(6));
    sepGfx.lineTo(innerRight, listEnd + px(6));
    sepGfx.strokePath();
    this.jukeboxContainer.add(sepGfx);

    // ---- Progress bar ----
    const progY = listEnd + px(22);
    const progH = px(4);

    this.jukeboxContainer.add(
      this.add.rectangle(innerLeft + innerW / 2, progY, innerW, progH, 0x1a1a28, 1)
    );

    const progFill = this.add.graphics();
    this.jukeboxContainer.add(progFill);

    // Larger click area for seeking
    const progHit = this.add.rectangle(innerLeft + innerW / 2, progY, innerW, px(16), 0x000000, 0)
      .setInteractive({ useHandCursor: true });
    progHit.on('pointerdown', (pointer: Phaser.Input.Pointer) => {
      const ratio = Math.max(0, Math.min(1, (pointer.x - innerLeft) / innerW));
      elapsed = ratio * JUKEBOX_TRACKS[trackIndex].duration;
      updateUI();
    });
    this.jukeboxContainer.add(progHit);

    // ---- Transport ----
    const transY = progY + px(26);

    const counterText = this.add.text(innerLeft, transY,
      `01 / ${String(JUKEBOX_TRACKS.length).padStart(2, '0')}`, {
      fontSize: fs(11), color: '#555550', fontFamily: '"Cinzel", monospace',
    }).setOrigin(0, 0.5);
    this.jukeboxContainer.add(counterText);

    const prevBtn = this.add.text(panelX - px(40), transY, '\u23EE', {
      fontSize: fs(16), color: '#888880', fontFamily: 'sans-serif',
    }).setOrigin(0.5).setInteractive({ useHandCursor: true });
    prevBtn.on('pointerover', () => prevBtn.setColor('#c0934a'));
    prevBtn.on('pointerout', () => prevBtn.setColor('#888880'));
    prevBtn.on('pointerdown', () => {
      if (elapsed > 3) doPlay(trackIndex); else doPlay(Math.max(0, trackIndex - 1));
    });
    this.jukeboxContainer.add(prevBtn);

    const ppBtn = this.add.text(panelX, transY, '\u23F8', {
      fontSize: fs(18), color: '#c0934a', fontFamily: 'sans-serif',
    }).setOrigin(0.5).setInteractive({ useHandCursor: true });
    ppBtn.on('pointerover', () => ppBtn.setColor('#e8e0d4'));
    ppBtn.on('pointerout', () => ppBtn.setColor('#c0934a'));
    ppBtn.on('pointerdown', () => {
      if (paused) {
        if (elapsed >= JUKEBOX_TRACKS[trackIndex].duration) {
          doPlay(0);
        } else {
          paused = false;
          audioManager.setMusicTempMute(false);
          ppBtn.setText('\u23F8');
        }
      } else {
        paused = true;
        audioManager.setMusicTempMute(true);
        ppBtn.setText('\u25B6');
      }
      updateUI();
    });
    this.jukeboxContainer.add(ppBtn);

    const nextBtn = this.add.text(panelX + px(40), transY, '\u23ED', {
      fontSize: fs(16), color: '#888880', fontFamily: 'sans-serif',
    }).setOrigin(0.5).setInteractive({ useHandCursor: true });
    nextBtn.on('pointerover', () => nextBtn.setColor('#c0934a'));
    nextBtn.on('pointerout', () => nextBtn.setColor('#888880'));
    nextBtn.on('pointerdown', () => doNext());
    this.jukeboxContainer.add(nextBtn);

    const timeText = this.add.text(innerRight, transY, '00:00 / 02:00', {
      fontSize: fs(11), color: '#666660', fontFamily: '"Cinzel", monospace',
    }).setOrigin(1, 0.5);
    this.jukeboxContainer.add(timeText);

    // ---- Close ----
    const closeBtn = this.add.text(panelX, panelTop + panelH - px(22), t('menu.back'), {
      fontSize: fs(14), color: '#888', fontFamily: '"Noto Sans SC", "Noto Sans TC", sans-serif',
    }).setOrigin(0.5).setInteractive({ useHandCursor: true });
    closeBtn.on('pointerdown', () => doClose());
    backdrop.on('pointerdown', () => doClose());
    this.jukeboxContainer.add(closeBtn);

    // ---- Logic ----
    const updateUI = () => {
      const track = JUKEBOX_TRACKS[trackIndex];
      const progress = track.duration > 0 ? Math.min(elapsed / track.duration, 1) : 0;

      highlight.setY(listTop + trackIndex * rowH + rowH / 2);

      for (let i = 0; i < JUKEBOX_TRACKS.length; i++) {
        const active = i === trackIndex;
        numTexts[i].setText(active ? '\u25B6' : String(i + 1).padStart(2, '0'));
        numTexts[i].setColor(active ? '#c0934a' : '#555550');
        titleTexts[i].setColor(active ? '#e8e0d4' : '#999990');
        durTexts[i].setColor(active ? '#c0934a' : '#555550');
        durTexts[i].setText(active ? fmtTime(elapsed) : fmtTime(JUKEBOX_TRACKS[i].duration));
      }

      progFill.clear();
      const fillW = innerW * progress;
      if (fillW > 0) {
        progFill.fillStyle(0xc0934a, 1);
        progFill.fillRect(innerLeft, progY - progH / 2, fillW, progH);
      }

      timeText.setText(`${fmtTime(elapsed)} / ${fmtTime(track.duration)}`);
      counterText.setText(
        `${String(trackIndex + 1).padStart(2, '0')} / ${String(JUKEBOX_TRACKS.length).padStart(2, '0')}`
      );
    };

    const doPlay = (index: number) => {
      trackIndex = index;
      elapsed = 0;
      paused = false;
      const track = JUKEBOX_TRACKS[index];
      audioManager.playTrack(track.zoneId, track.state);
      audioManager.setMusicTempMute(false);
      ppBtn.setText('\u23F8');
      updateUI();
    };

    const doNext = () => {
      if (trackIndex < JUKEBOX_TRACKS.length - 1) {
        doPlay(trackIndex + 1);
      }
    };

    const doClose = () => {
      timer.destroy();
      audioManager.setMusicTempMute(false);
      EventBus.emit(GameEvents.ZONE_ENTERED, { mapId: 'menu' });
      this.jukeboxContainer?.destroy();
      this.jukeboxContainer = null;
      this.activePanel = 'menu';
    };

    this.events.once('shutdown', () => {
      if (this.jukeboxContainer) {
        timer.destroy();
        audioManager.setMusicTempMute(false);
      }
    });

    // ---- Timer ----
    const timer = this.time.addEvent({
      delay: 250,
      loop: true,
      callback: () => {
        if (paused) return;
        elapsed += 0.25;
        if (elapsed >= JUKEBOX_TRACKS[trackIndex].duration) {
          if (trackIndex < JUKEBOX_TRACKS.length - 1) {
            doPlay(trackIndex + 1);
          } else {
            elapsed = JUKEBOX_TRACKS[trackIndex].duration;
            paused = true;
            ppBtn.setText('\u25B6');
            updateUI();
          }
          return;
        }
        updateUI();
      },
    });

    doPlay(0);
  }

  private showCredits(): void {
    if (this.creditsContainer) { this.creditsContainer.destroy(); }
    this.creditsContainer = this.add.container(0, 0).setDepth(20);
    this.activePanel = 'credits';

    const cx = W / 2;
    const panelW = px(500);
    const panelH = px(560);
    const panelX = cx;
    const panelY = H / 2;
    const panelTop = panelY - panelH / 2;
    const panelLeft = panelX - panelW / 2;
    const innerLeft = panelLeft + px(24);
    const innerRight = panelLeft + panelW - px(24);

    // Backdrop
    const backdrop = this.add.rectangle(cx, H / 2, W, H, 0x000000, 0.6).setInteractive();
    this.creditsContainer.add(backdrop);

    // Panel
    this.creditsContainer.add(
      this.add.rectangle(panelX, panelY, panelW, panelH, 0x0e0e1a, 0.95)
        .setStrokeStyle(1.5, 0xc0934a, 0.6)
    );

    // Title
    this.creditsContainer.add(this.add.text(panelX, panelTop + px(24), t('menu.creditsPanel.title'), {
      fontSize: fs(22), color: '#c0934a', fontFamily: '"Cinzel", "Noto Sans SC", "Noto Sans TC", serif', fontStyle: 'bold',
    }).setOrigin(0.5));

    // Decorative line
    const lineGfx = this.add.graphics();
    lineGfx.lineStyle(1, 0xc0934a, 0.4);
    lineGfx.beginPath();
    lineGfx.moveTo(innerLeft, panelTop + px(44));
    lineGfx.lineTo(innerRight, panelTop + px(44));
    lineGfx.strokePath();
    this.creditsContainer.add(lineGfx);

    let y = panelTop + px(60);

    // --- Tile Art ---
    this.creditsContainer.add(this.add.text(innerLeft, y, t('menu.creditsPanel.tileArt'), {
      fontSize: fs(14), color: '#d4a84b', fontFamily: '"Noto Sans SC", "Noto Sans TC", sans-serif', fontStyle: 'bold',
    }).setOrigin(0, 0.5));
    y += px(22);
    this.creditsContainer.add(this.add.text(innerLeft + px(8), y, 'Isometric Landscape — Kenney (kenney.nl)', {
      fontSize: fs(12), color: '#e0d8cc', fontFamily: '"Noto Sans SC", "Noto Sans TC", sans-serif',
    }).setOrigin(0, 0.5));
    y += px(16);
    this.creditsContainer.add(this.add.text(innerLeft + px(8), y, t('menu.creditsPanel.tileArt.license'), {
      fontSize: fs(11), color: '#888880', fontFamily: '"Noto Sans SC", "Noto Sans TC", sans-serif',
    }).setOrigin(0, 0.5));

    y += px(28);

    // --- BGM ---
    this.creditsContainer.add(this.add.text(innerLeft, y, t('menu.creditsPanel.bgm'), {
      fontSize: fs(14), color: '#d4a84b', fontFamily: '"Noto Sans SC", "Noto Sans TC", sans-serif', fontStyle: 'bold',
    }).setOrigin(0, 0.5));
    y += px(18);
    this.creditsContainer.add(this.add.text(innerLeft + px(8), y, t('menu.creditsPanel.bgm.source'), {
      fontSize: fs(12), color: '#e0d8cc', fontFamily: '"Noto Sans SC", "Noto Sans TC", sans-serif',
    }).setOrigin(0, 0.5));
    y += px(22);

    const bgmCredits: { artist: string; tracks: string; license: string }[] = [
      { artist: 'DST', tracks: 'GrassLands Theme', license: 'CC0' },
      { artist: 'cynicmusic', tracks: 'Battle Theme A / B, Dark Forest Theme, Victory Fanfare Short', license: 'CC0' },
      { artist: 'RandomMind', tracks: 'Medieval: Victory Theme', license: 'CC0' },
      { artist: 'Zane Little Music', tracks: 'Glizzy Elf Forest RPG Music Pack', license: 'CC0' },
      { artist: 'Cesar da Rocha', tracks: 'Fantasy Choir 2', license: 'CC0' },
      { artist: 'Juhani Junkala', tracks: 'Epic Boss Battle', license: 'CC0' },
      { artist: 'Tarush Singhal', tracks: 'Desert Theme', license: 'CC0' },
      { artist: 'antonioraymond71', tracks: 'Desert Battle Theme', license: 'GPL 2.0' },
      { artist: 'JaggedStone', tracks: 'Loopable Dungeon Ambience', license: 'CC0' },
    ];

    for (const credit of bgmCredits) {
      this.creditsContainer.add(this.add.text(innerLeft + px(8), y, credit.artist, {
        fontSize: fs(11), color: '#c0934a', fontFamily: '"Noto Sans SC", sans-serif',
      }).setOrigin(0, 0.5));
      this.creditsContainer.add(this.add.text(innerRight, y, credit.license, {
        fontSize: fs(10), color: '#666660', fontFamily: '"Cinzel", monospace',
      }).setOrigin(1, 0.5));
      y += px(16);
      this.creditsContainer.add(this.add.text(innerLeft + px(16), y, credit.tracks, {
        fontSize: fs(10), color: '#888880', fontFamily: '"Noto Sans SC", sans-serif',
        wordWrap: { width: panelW - px(80) },
      }).setOrigin(0, 0.5));
      y += px(18);
    }

    y += px(8);

    // Separator
    const sepGfx = this.add.graphics();
    sepGfx.lineStyle(1, 0x333340, 0.5);
    sepGfx.beginPath();
    sepGfx.moveTo(innerLeft, y);
    sepGfx.lineTo(innerRight, y);
    sepGfx.strokePath();
    this.creditsContainer.add(sepGfx);
    y += px(16);

    // Engine credit
    this.creditsContainer.add(this.add.text(innerLeft, y, t('menu.creditsPanel.engine'), {
      fontSize: fs(14), color: '#d4a84b', fontFamily: '"Noto Sans SC", "Noto Sans TC", sans-serif', fontStyle: 'bold',
    }).setOrigin(0, 0.5));
    y += px(20);
    this.creditsContainer.add(this.add.text(innerLeft + px(8), y, 'Phaser 3 — phaser.io (MIT License)', {
      fontSize: fs(12), color: '#e0d8cc', fontFamily: '"Noto Sans SC", "Noto Sans TC", sans-serif',
    }).setOrigin(0, 0.5));

    // Close button
    const closeBtn = this.add.text(panelX, panelTop + panelH - px(22), t('menu.back'), {
      fontSize: fs(14), color: '#888', fontFamily: '"Noto Sans SC", "Noto Sans TC", sans-serif',
    }).setOrigin(0.5).setInteractive({ useHandCursor: true });
    closeBtn.on('pointerdown', () => {
      this.creditsContainer?.destroy();
      this.creditsContainer = null;
      this.activePanel = 'menu';
    });
    backdrop.on('pointerdown', () => {
      this.creditsContainer?.destroy();
      this.creditsContainer = null;
      this.activePanel = 'menu';
    });
    this.creditsContainer.add(closeBtn);
  }

  private loadGame(save: SaveData): void {
    this.scene.start('ZoneScene', {
      classId: save.classId,
      mapId: save.player.currentMap,
      saveData: save,
    });
  }

  // ---------------------------------------------------------------------------
  // Difficulty Selector
  // ---------------------------------------------------------------------------

  private showDifficultySelector(save: SaveData): void {
    if (this.difficultyContainer) { this.difficultyContainer.destroy(); }
    this.difficultyContainer = this.add.container(0, 0).setDepth(10);
    this.activePanel = 'difficulty';

    const cx = W / 2;
    const completedDiffs = save.completedDifficulties ?? [];
    const states = DifficultySystem.getDifficultyStates(completedDiffs);
    const currentDiff = save.difficulty ?? 'normal';

    // Difficulty locale key map
    const DIFF_LABEL_KEYS: Record<Difficulty, string> = {
      normal: 'menu.difficulty.normal',
      nightmare: 'menu.difficulty.nightmare',
      hell: 'menu.difficulty.hell',
    };
    const DIFF_DESC_KEYS: Record<Difficulty, string> = {
      normal: 'menu.difficulty.desc.normal',
      nightmare: 'menu.difficulty.desc.nightmare',
      hell: 'menu.difficulty.desc.hell',
    };

    // Title
    this.difficultyContainer.add(this.add.text(cx, px(275), t('menu.difficulty.title'), {
      fontSize: fs(22),
      color: '#c0934a',
      fontFamily: '"Noto Sans SC", "Noto Sans TC", sans-serif',
      fontStyle: 'bold',
    }).setOrigin(0.5));

    // Difficulty colors
    const DIFF_COLORS: Record<Difficulty, number> = {
      normal: 0x4a8c4a,
      nightmare: 0xc0392b,
      hell: 0x8b0000,
    };
    const DIFF_TEXT_COLORS: Record<Difficulty, string> = {
      normal: '#4ade80',
      nightmare: '#ef4444',
      hell: '#ff4444',
    };

    DIFFICULTY_ORDER.forEach((diff, i) => {
      const y = px(340) + i * px(80);
      const state = states[diff];
      const isLocked = state === 'locked';
      const isCompleted = state === 'completed';
      const isCurrent = diff === currentDiff;

      const borderColor = isLocked ? 0x333344 : DIFF_COLORS[diff];
      const bgAlpha = isLocked ? 0.5 : 0.9;
      const borderAlpha = isLocked ? 0.3 : (isCurrent ? 1.0 : 0.6);

      const bg = this.add.rectangle(cx, y, px(340), px(65), 0x12121e, bgAlpha)
        .setStrokeStyle(isCurrent ? 2.5 : 1.5, borderColor, borderAlpha);

      if (!isLocked) {
        bg.setInteractive({ useHandCursor: true });
        bg.on('pointerover', () => {
          bg.setStrokeStyle(2.5, borderColor, 1);
          bg.setFillStyle(0x1a1a2e, 0.95);
        });
        bg.on('pointerout', () => {
          bg.setStrokeStyle(isCurrent ? 2.5 : 1.5, borderColor, borderAlpha);
          bg.setFillStyle(0x12121e, bgAlpha);
        });
        bg.on('pointerdown', () => {
          save.difficulty = diff;
          this.difficultyContainer?.destroy(); this.difficultyContainer = null;
          this.loadGame(save);
        });
      }

      this.difficultyContainer!.add(bg);

      // Difficulty label with state indicator
      let label = t(DIFF_LABEL_KEYS[diff]);
      if (isCompleted) {
        label = `✓ ${label}`;
      } else if (isLocked) {
        label = `🔒 ${label}`;
      }

      const textColor = isLocked ? '#555555' : (isCurrent ? '#ffffff' : DIFF_TEXT_COLORS[diff]);

      this.difficultyContainer!.add(this.add.text(cx, y - px(10), label, {
        fontSize: fs(20),
        color: textColor,
        fontFamily: '"Noto Sans SC", "Noto Sans TC", sans-serif',
        fontStyle: isCurrent ? 'bold' : 'normal',
      }).setOrigin(0.5));

      // Description text
      const descText = isLocked ? t('menu.difficulty.locked') : t(DIFF_DESC_KEYS[diff]);
      this.difficultyContainer!.add(this.add.text(cx, y + px(14), descText, {
        fontSize: fs(13),
        color: isLocked ? '#444444' : '#888880',
        fontFamily: '"Noto Sans SC", "Noto Sans TC", sans-serif',
      }).setOrigin(0.5));

      // Current difficulty indicator
      if (isCurrent && !isLocked) {
        this.difficultyContainer!.add(this.add.text(cx + px(140), y - px(10), t('menu.difficulty.current'), {
          fontSize: fs(11),
          color: '#c0934a',
          fontFamily: '"Noto Sans SC", "Noto Sans TC", sans-serif',
        }).setOrigin(0.5));
      }
    });

    // Back button
    const backY = px(340) + 3 * px(80);
    const backBg = this.add.rectangle(cx, backY, px(200), px(45), 0x12121e, 0.9)
      .setStrokeStyle(1.5, 0x555566, 0.4).setInteractive({ useHandCursor: true });
    backBg.on('pointerover', () => { backBg.setStrokeStyle(2, 0x888899, 0.8); backBg.setFillStyle(0x1a1a2e, 0.95); });
    backBg.on('pointerout', () => { backBg.setStrokeStyle(1.5, 0x555566, 0.4); backBg.setFillStyle(0x12121e, 0.9); });
    backBg.on('pointerdown', () => {
      this.difficultyContainer?.destroy(); this.difficultyContainer = null;
      this.showMainMenu(save);
    });
    this.difficultyContainer.add(backBg);
    this.difficultyContainer.add(this.add.text(cx, backY, t('menu.backShort'), {
      fontSize: fs(16), color: '#888880', fontFamily: '"Noto Sans SC", "Noto Sans TC", sans-serif',
    }).setOrigin(0.5));
  }

  // ---------------------------------------------------------------------------
  // Language Selector
  // ---------------------------------------------------------------------------

  private showLanguageSelector(): void {
    if (this.langContainer) { this.langContainer.destroy(); }
    this.langContainer = this.add.container(0, 0).setDepth(10);
    this.activePanel = 'lang';

    const cx = W / 2;
    const currentLang = getLocale();

    // Title
    this.langContainer.add(this.add.text(cx, px(300), t('menu.language'), {
      fontSize: fs(22),
      color: '#c0934a',
      fontFamily: '"Noto Sans SC", "Noto Sans TC", sans-serif',
      fontStyle: 'bold',
    }).setOrigin(0.5));

    const options: { key: string; label: string; localeId: string }[] = [
      { key: 'menu.langSelect.zhCN', label: t('menu.langSelect.zhCN'), localeId: 'zh-CN' },
      { key: 'menu.langSelect.zhTW', label: t('menu.langSelect.zhTW'), localeId: 'zh-TW' },
      { key: 'menu.langSelect.en', label: t('menu.langSelect.en'), localeId: 'en' },
    ];

    options.forEach((opt, i) => {
      const y = px(370) + i * px(70);
      const isCurrent = opt.localeId === currentLang;
      const borderColor = isCurrent ? 0xc0934a : 0x555566;
      const borderAlpha = isCurrent ? 0.8 : 0.4;

      const bg = this.add.rectangle(cx, y, px(320), px(50), 0x12121e, 0.9)
        .setStrokeStyle(isCurrent ? 2 : 1.5, borderColor, borderAlpha)
        .setInteractive({ useHandCursor: true });

      bg.on('pointerover', () => { bg.setStrokeStyle(2, 0xc0934a, 1); bg.setFillStyle(0x1a1a2e, 0.95); });
      bg.on('pointerout', () => { bg.setStrokeStyle(isCurrent ? 2 : 1.5, borderColor, borderAlpha); bg.setFillStyle(0x12121e, 0.9); });
      bg.on('pointerdown', () => {
        // setLocale triggers LOCALE_CHANGED which re-renders via onLocaleChanged
        setLocale(opt.localeId);
      });

      this.langContainer!.add(bg);
      this.langContainer!.add(this.add.text(cx, y, opt.label, {
        fontSize: fs(18),
        color: isCurrent ? '#e8e0d4' : '#a0907a',
        fontFamily: '"Noto Sans SC", "Noto Sans TC", sans-serif',
        fontStyle: isCurrent ? 'bold' : 'normal',
      }).setOrigin(0.5));

      if (isCurrent) {
        this.langContainer!.add(this.add.text(cx + px(140), y, t('menu.difficulty.current'), {
          fontSize: fs(11),
          color: '#c0934a',
          fontFamily: '"Noto Sans SC", "Noto Sans TC", sans-serif',
        }).setOrigin(0.5));
      }
    });

    // Back button
    const backBtn = this.add.text(cx, px(590), t('menu.back'), {
      fontSize: fs(14), color: '#888', fontFamily: '"Noto Sans SC", "Noto Sans TC", sans-serif',
    }).setOrigin(0.5).setInteractive({ useHandCursor: true });
    backBtn.on('pointerdown', () => {
      this.langContainer?.destroy(); this.langContainer = null;
      this.checkForSaves();
    });
    this.langContainer.add(backBtn);
  }

  private startGame(classId: string): void {
    this.scene.start('ZoneScene', { classId, mapId: 'emerald_plains' });
  }
}

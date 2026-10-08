import Phaser from 'phaser';
import { GAME_WIDTH, GAME_HEIGHT, TEXTURE_SCALE, DPR, RENDER_SCALE } from '../config';
import { SaveSystem } from '../systems/SaveSystem';

import { EventBus, GameEvents } from '../utils/EventBus';
import { DisposableScope } from '../utils/DisposableScope';
import { ensureGameplayScenes } from './GameplayLoader';
import { audioManager } from '../systems/audio/AudioManager';
import type { SaveData } from '../data/types';
import { SpriteGenerator } from '../graphics/SpriteGenerator';
import { DifficultySystem, DIFFICULTY_ORDER } from '../systems/DifficultySystem';
import type { Difficulty } from '../systems/DifficultySystem';
import { t, setLocale, getLocale } from '../i18n';
import { getZoneName } from '../i18n/gameAccessors';
import { AllClasses } from '../data/classes';
import { addFrame, addButton, addDivider, addTitleFlourishes, UI_COLORS, type UiButton, type ButtonVariant } from '../ui/UiKit';
import { applyScreenCamera } from '../rendering/RenderScalePhaser';

const MENU_FONT = '"Noto Sans SC", "Noto Sans TC", sans-serif';
const MENU_TITLE_FONT = '"Cinzel", "Noto Sans SC", "Noto Sans TC", serif';

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
  private subscriptions = new DisposableScope();
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
    applyScreenCamera(this);
    // Coming back from a zone (Esc → menu) must hand the music back to the title theme.
    audioManager.playTrack('menu', 'explore');
    this.subscriptions = new DisposableScope();
    const cx = W / 2;

    this.buildBackground(cx);
    this.buildTitle(cx);
    this.startBGM();
    this.checkForSaves();

    // Listen for locale changes to re-render the active panel
    this.subscriptions.on(EventBus, GameEvents.LOCALE_CHANGED, this.onLocaleChanged, this);
    this.events.once('shutdown', () => this.subscriptions.dispose());
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

    // Layer 7 — Slowly turning rune circle behind the title
    this.buildRuneCircle(cx);

    // Layer 8 — Vignette to frame the scene
    const vigKey = 'menu_vignette';
    if (!this.textures.exists(vigKey)) {
      const canvas = this.textures.createCanvas(vigKey, 320, 180)!;
      const ctx2d = canvas.getContext();
      const g = ctx2d.createRadialGradient(160, 80, 40, 160, 90, 190);
      g.addColorStop(0, 'rgba(0,0,0,0)');
      g.addColorStop(0.65, 'rgba(0,0,0,0.18)');
      g.addColorStop(1, 'rgba(0,0,0,0.6)');
      ctx2d.fillStyle = g;
      ctx2d.fillRect(0, 0, 320, 180);
      canvas.refresh();
    }
    this.add.image(cx, H / 2, vigKey).setDisplaySize(W, H).setDepth(6);
  }

  private buildRuneCircle(cx: number): void {
    const key = 'menu_rune_circle';
    const S = 512;
    if (!this.textures.exists(key)) {
      const canvas = this.textures.createCanvas(key, S, S)!;
      const ctx = canvas.getContext();
      const c = S / 2;
      ctx.strokeStyle = 'rgba(255,170,80,0.55)';
      ctx.lineWidth = 2;
      for (const r of [240, 226, 170, 158]) {
        ctx.beginPath(); ctx.arc(c, c, r, 0, Math.PI * 2); ctx.stroke();
      }
      // rune ticks between the outer rings
      ctx.lineWidth = 3;
      for (let i = 0; i < 48; i++) {
        const a = (i / 48) * Math.PI * 2;
        const r0 = 229, r1 = i % 4 === 0 ? 238 : 234;
        ctx.beginPath();
        ctx.moveTo(c + Math.cos(a) * r0, c + Math.sin(a) * r0);
        ctx.lineTo(c + Math.cos(a) * r1, c + Math.sin(a) * r1);
        ctx.stroke();
      }
      // hexagram
      ctx.lineWidth = 2;
      for (let k = 0; k < 2; k++) {
        ctx.beginPath();
        for (let i = 0; i <= 3; i++) {
          const a = -Math.PI / 2 + k * Math.PI / 3 + (i * Math.PI * 2) / 3;
          const x = c + Math.cos(a) * 158, y = c + Math.sin(a) * 158;
          if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
        }
        ctx.stroke();
      }
      // small glyph dots
      ctx.fillStyle = 'rgba(255,200,120,0.7)';
      for (let i = 0; i < 12; i++) {
        const a = (i / 12) * Math.PI * 2;
        ctx.beginPath(); ctx.arc(c + Math.cos(a) * 198, c + Math.sin(a) * 198, 4, 0, Math.PI * 2); ctx.fill();
      }
      canvas.refresh();
    }
    const ring = this.add.image(cx, px(150), key).setDisplaySize(px(420), px(420))
      .setBlendMode(Phaser.BlendModes.ADD).setAlpha(0.13).setDepth(5);
    this.tweens.add({ targets: ring, angle: 360, duration: 120000, repeat: -1 });
    this.tweens.add({ targets: ring, alpha: { from: 0.09, to: 0.17 }, duration: 5000, yoyo: true, repeat: -1, ease: 'Sine.easeInOut' });
  }

  /** Framed menu button (UiKit). */
  private menuButton(x: number, y: number, w: number, h: number, label: string, onClick: () => void, variant: ButtonVariant = 'secondary', fontSize = 16): UiButton {
    return addButton(this, x, y, w, h, label, { variant, fontSize, fontFamily: MENU_FONT, onClick: () => onClick() });
  }

  /** Opaque modal panel frame with a centred title (returns content top y). */
  private addModalFrame(container: Phaser.GameObjects.Container, cx: number, cy: number, w: number, h: number, title: string): number {
    const left = cx - w / 2, top = cy - h / 2;
    container.add(addFrame(this, left, top, w, h, { variant: 'panel', header: px(44) }));
    const titleT = this.add.text(cx, top + px(23), title, {
      fontSize: fs(20), color: UI_COLORS.parchment, fontFamily: MENU_TITLE_FONT, fontStyle: 'bold',
      stroke: '#120b04', strokeThickness: Math.round(3 * DPR),
    }).setOrigin(0.5);
    container.add(addTitleFlourishes(this, cx, top + px(23), titleT.width));
    container.add(titleT);
    return top + px(56);
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

    // Ornamental rule above the title
    this.titleContainer.add(addDivider(this, cx, px(78), px(440)));

    // Title — ABYSSFIRE never changes; molten-gold gradient fill
    const title = this.add.text(cx, px(130), t('menu.title'), {
      fontSize: fs(58),
      color: '#e8b04a',
      fontFamily: '"Cinzel", serif',
      fontStyle: 'bold',
      stroke: '#2a1606',
      strokeThickness: Math.round(6 * DPR),
      shadow: { offsetX: 0, offsetY: 4, color: '#000000', blur: 12, fill: true, stroke: true },
    }).setOrigin(0.5);
    const grad = title.context.createLinearGradient(0, 0, 0, title.height);
    grad.addColorStop(0, '#fff3c4');
    grad.addColorStop(0.45, '#f0b84e');
    grad.addColorStop(0.7, '#c46a1c');
    grad.addColorStop(1, '#7a2a0c');
    title.setFill(grad);
    this.titleContainer.add(title);
    // soft breathing glow on the title
    this.tweens.add({ targets: title, scale: { from: 1, to: 1.015 }, duration: 3000, yoyo: true, repeat: -1, ease: 'Sine.easeInOut' });

    // Subtitle — locale-dependent
    const sub = this.add.text(cx, px(190), t('menu.subtitle'), {
      fontSize: fs(30),
      color: '#e8c77a',
      fontFamily: MENU_FONT,
      fontStyle: 'bold',
      stroke: '#1a0e04',
      strokeThickness: Math.round(4 * DPR),
      shadow: { offsetX: 0, offsetY: 2, color: '#000000', blur: 6, fill: true },
    }).setOrigin(0.5);
    this.titleContainer.add(addTitleFlourishes(this, cx, px(190), sub.width + px(12)));
    this.titleContainer.add(sub);

    // Ornamental rule below the title
    this.titleContainer.add(addDivider(this, cx, px(222), px(360), false));

    // Version
    this.titleContainer.add(this.add.text(cx, H - px(18), 'v0.22.0', {
      fontSize: fs(12),
      color: '#6a5a48',
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
    const menu = this.menuContainer;

    const cx = W / 2;
    let y = save ? px(296) : px(320);

    if (save) {
      // Save slot card — portrait, class + level, zone, difficulty
      const cardW = px(380), cardH = px(86);
      const left = cx - cardW / 2, top = y - cardH / 2;
      const frame = addFrame(this, left, top, cardW, cardH, { variant: 'tooltip', accent: 0xffd98a });
      menu.add(frame);
      const hit = this.add.rectangle(cx, y, cardW, cardH, 0xffc860, 0).setInteractive({ useHandCursor: true });
      menu.add(hit);

      // Portrait: animated class sprite in a round well
      const pcx = left + px(46), pcy = y;
      const well = this.add.graphics();
      well.fillStyle(0x000000, 0.6); well.fillCircle(pcx, pcy + 1.5, px(34));
      well.fillStyle(0x16121a, 1); well.fillCircle(pcx, pcy, px(33));
      well.lineStyle(2, 0xd4a54a, 1); well.strokeCircle(pcx, pcy, px(33));
      menu.add(well);
      const spriteKey = `player_${save.classId}`;
      SpriteGenerator.ensurePlayerSheet(this, save.classId);
      if (this.textures.exists(spriteKey)) {
        const maskG = this.make.graphics({});
        maskG.fillStyle(0xffffff); maskG.fillCircle(pcx, pcy, px(31));
        const portrait = this.add.sprite(pcx, pcy + px(52), spriteKey, 0).setOrigin(0.5, 1);
        portrait.setScale(px(150) / Math.max(1, portrait.height));
        portrait.setMask(maskG.createGeometryMask());
        const idleKey = `${spriteKey}_idle`;
        if (this.anims.exists(idleKey)) portrait.play(idleKey);
        menu.add(portrait);
        menu.once('destroy', () => maskG.destroy());
      }

      const classNameKey = `data.class.${save.classId}.name`;
      const className = t(classNameKey);
      const label = t('menu.continue', { class: className, level: String(save.player.level) });
      const tx = left + px(92);
      menu.add(this.add.text(tx, top + px(20), label, {
        fontSize: fs(17), color: UI_COLORS.parchment, fontFamily: MENU_TITLE_FONT, fontStyle: 'bold',
        stroke: '#000000', strokeThickness: Math.round(2 * DPR),
      }).setOrigin(0, 0.5));
      const diff = (save.difficulty ?? 'normal') as Difficulty;
      const diffLabel = t(`menu.difficulty.${diff}`);
      menu.add(this.add.text(tx, top + px(44), `${getZoneName(save.player.currentMap)}  ·  ${diffLabel}`, {
        fontSize: fs(13), color: UI_COLORS.textSoft, fontFamily: MENU_FONT,
      }).setOrigin(0, 0.5));
      menu.add(this.add.text(tx, top + px(66), t('menu.continueSubtitle'), {
        fontSize: fs(12), color: '#e8c77a', fontFamily: MENU_FONT, fontStyle: 'italic',
      }).setOrigin(0, 0.5));
      const arrow = this.add.text(left + cardW - px(22), y, '▶', {
        fontSize: fs(18), color: '#e8c77a', fontFamily: MENU_FONT,
      }).setOrigin(0.5);
      menu.add(arrow);

      hit.on('pointerover', () => { hit.setFillStyle(0xffc860, 0.08); arrow.setColor('#fff3c4'); frame.setTint(0xfff0d0); });
      hit.on('pointerout', () => { hit.setFillStyle(0xffc860, 0); arrow.setColor('#e8c77a'); frame.clearTint(); });
      hit.on('pointerdown', () => {
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

      y += px(84);
    }

    // "New Game" button
    menu.add(this.menuButton(cx, y, px(320), px(50), t('menu.newGame'), () => {
      this.menuContainer?.destroy(); this.menuContainer = null;
      this.showClassSelection();
    }, save ? 'secondary' : 'primary', 20));

    y += px(56);

    const secondary: [string, () => void][] = [
      [t('menu.help'), () => this.showHelp()],
      [t('menu.ost'), () => this.showJukebox()],
      [t('menu.credits'), () => this.showCredits()],
      [t('menu.language'), () => {
        this.menuContainer?.destroy(); this.menuContainer = null;
        this.showLanguageSelector();
      }],
    ];
    for (const [label, cb] of secondary) {
      menu.add(this.menuButton(cx, y, px(280), px(38), label, cb, 'ghost', 15));
      y += px(46);
    }
  }

  private showClassSelection(): void {
    this.classContainer = this.add.container(0, 0).setDepth(10);
    this.activePanel = 'class';
    const cx = W / 2;
    const cont = this.classContainer;

    const heading = this.add.text(cx, px(250), t('menu.classSelect.title'), {
      fontSize: fs(20),
      color: UI_COLORS.parchment,
      fontFamily: MENU_TITLE_FONT,
      fontStyle: 'bold',
      stroke: '#120b04', strokeThickness: Math.round(3 * DPR),
    }).setOrigin(0.5);
    cont.add(addTitleFlourishes(this, cx, px(250), heading.width));
    cont.add(heading);

    const classes = [
      { id: 'warrior', nameKey: 'menu.classSelect.warrior.name', descKey: 'menu.classSelect.warrior.desc', color: 0xd0473a, accent: '#ff8a72' },
      { id: 'mage', nameKey: 'menu.classSelect.mage.name', descKey: 'menu.classSelect.mage.desc', color: 0x9b59d6, accent: '#d0a0ff' },
      { id: 'rogue', nameKey: 'menu.classSelect.rogue.name', descKey: 'menu.classSelect.rogue.desc', color: 0x3fb86a, accent: '#8ff0a8' },
    ];

    const cardW = px(250), cardH = px(356), gap = px(26);
    const cardsTop = px(274);

    // Soft coloured radial light used behind each hero
    const lightKey = 'menu_class_light';
    if (!this.textures.exists(lightKey)) {
      const canvas = this.textures.createCanvas(lightKey, 128, 128)!;
      const ctx2d = canvas.getContext();
      const g = ctx2d.createRadialGradient(64, 64, 0, 64, 64, 64);
      g.addColorStop(0, 'rgba(255,255,255,0.9)');
      g.addColorStop(0.4, 'rgba(255,255,255,0.35)');
      g.addColorStop(1, 'rgba(255,255,255,0)');
      ctx2d.fillStyle = g;
      ctx2d.fillRect(0, 0, 128, 128);
      canvas.refresh();
    }

    classes.forEach((cls, i) => {
      const ccx = cx + (i - 1) * (cardW + gap);
      const card = this.add.container(ccx, cardsTop + cardH / 2);
      cont.add(card);
      const left = -cardW / 2, top = -cardH / 2;

      const hoverGlow = this.add.image(0, 0, lightKey).setDisplaySize(cardW * 1.5, cardH * 1.25)
        .setTint(cls.color).setAlpha(0).setBlendMode(Phaser.BlendModes.ADD);
      card.add(hoverGlow);
      card.add(addFrame(this, left, top, cardW, cardH, { variant: 'panel', gem: cls.color, accent: cls.color }));

      // Hero stage: coloured light + pedestal
      const stageY = top + px(128);
      card.add(this.add.image(0, stageY - px(20), lightKey).setDisplaySize(px(220), px(200)).setTint(cls.color).setAlpha(0.35).setBlendMode(Phaser.BlendModes.ADD));
      const ped = this.add.graphics();
      ped.fillStyle(0x000000, 0.55); ped.fillEllipse(0, stageY + px(62), px(130), px(26));
      ped.lineStyle(2, cls.color, 0.8); ped.strokeEllipse(0, stageY + px(60), px(120), px(22));
      ped.lineStyle(1, 0xffd98a, 0.5); ped.strokeEllipse(0, stageY + px(60), px(96), px(16));
      card.add(ped);

      // Animated class sprite (new cel-shaded character art)
      const spriteKey = `player_${cls.id}`;
      SpriteGenerator.ensurePlayerSheet(this, cls.id);
      let preview: Phaser.GameObjects.Sprite | null = null;
      if (this.textures.exists(spriteKey)) {
        preview = this.add.sprite(0, stageY + px(64), spriteKey, 0).setOrigin(0.5, 1);
        const baseScale = px(170) / Math.max(1, preview.height);
        preview.setScale(baseScale);
        const idleKey = `${spriteKey}_idle`;
        if (this.anims.exists(idleKey)) preview.play(idleKey);
        card.add(preview);
      }

      // Name + description
      card.add(this.add.text(0, top + px(222), t(cls.nameKey), {
        fontSize: fs(20), color: UI_COLORS.parchment, fontFamily: MENU_TITLE_FONT, fontStyle: 'bold',
        stroke: '#000000', strokeThickness: Math.round(3 * DPR),
      }).setOrigin(0.5));
      card.add(this.add.text(0, top + px(248), t(cls.descKey), {
        fontSize: fs(13), color: cls.accent, fontFamily: MENU_FONT, align: 'center',
        wordWrap: { width: cardW - px(36), useAdvancedWrap: true },
      }).setOrigin(0.5, 0));

      // Signature skill icons
      const skills = (AllClasses[cls.id]?.skills ?? []).filter(sk => this.textures.exists(`skill_icon_${sk.id}`)).slice(0, 4);
      const iconSz = px(30), iconGap = px(8);
      const rowW = skills.length * iconSz + Math.max(0, skills.length - 1) * iconGap;
      skills.forEach((sk, k) => {
        const ix = -rowW / 2 + iconSz / 2 + k * (iconSz + iconGap);
        const iy = top + px(292);
        const fr = this.add.graphics();
        fr.fillStyle(0x07060a, 1); fr.fillRoundedRect(ix - iconSz / 2 - 2, iy - iconSz / 2 - 2, iconSz + 4, iconSz + 4, 4);
        fr.lineStyle(1.5, 0xd4a54a, 0.9); fr.strokeRoundedRect(ix - iconSz / 2 - 2, iy - iconSz / 2 - 2, iconSz + 4, iconSz + 4, 4);
        card.add(fr);
        card.add(this.add.image(ix, iy, `skill_icon_${sk.id}`).setDisplaySize(iconSz, iconSz));
      });

      // Choose button
      const btn = addButton(this, 0, top + cardH - px(30), cardW - px(60), px(34), t('menu.classSelect.confirm'), {
        variant: 'primary', fontSize: 15, bold: true, fontFamily: MENU_FONT,
        onClick: () => this.startGame(cls.id),
      });
      card.add(btn);

      // Whole card is clickable too
      const hit = this.add.rectangle(0, -px(24), cardW, cardH - px(56), 0x000000, 0).setInteractive({ useHandCursor: true });
      card.addAt(hit, 2);
      const baseY = card.y;
      const onOver = () => {
        this.tweens.killTweensOf(card);
        this.tweens.add({ targets: card, y: baseY - px(8), duration: 160, ease: 'Quad.easeOut' });
        this.tweens.add({ targets: hoverGlow, alpha: 0.35, duration: 160 });
        const atkKey = `${spriteKey}_attack`;
        if (preview && this.anims.exists(atkKey)) {
          preview.play(atkKey);
          preview.once('animationcomplete', () => {
            const idleAnim = `${spriteKey}_idle`;
            if (preview && preview.active && this.anims.exists(idleAnim)) preview.play(idleAnim);
          });
        }
      };
      const onOut = (pointer?: Phaser.Input.Pointer) => {
        if (pointer && Math.abs((pointer.x / RENDER_SCALE) - card.x) < cardW / 2 && Math.abs((pointer.y / RENDER_SCALE) - baseY) < cardH / 2) return;
        this.tweens.killTweensOf(card);
        this.tweens.add({ targets: card, y: baseY, duration: 160, ease: 'Quad.easeOut' });
        this.tweens.add({ targets: hoverGlow, alpha: 0, duration: 200 });
      };
      hit.on('pointerover', () => { if (card.y >= baseY - 1) onOver(); });
      hit.on('pointerout', onOut);
      btn.bg.on('pointerover', () => { if (card.y >= baseY - 1) onOver(); });
      btn.bg.on('pointerout', onOut);
      hit.on('pointerdown', () => this.startGame(cls.id));

      // staggered entrance
      card.setAlpha(0);
      card.y = baseY + px(24);
      this.tweens.add({ targets: card, alpha: 1, y: baseY, duration: 360, delay: 80 * i, ease: 'Cubic.easeOut' });
    });

    // Back button
    cont.add(this.menuButton(cx, px(668), px(160), px(34), t('menu.backShort'), () => {
      this.classContainer?.destroy(); this.classContainer = null;
      this.checkForSaves();
    }, 'ghost', 14));
  }

  private showHelp(): void {
    if (this.helpContainer) { this.helpContainer.destroy(); }
    this.helpContainer = this.add.container(0, 0).setDepth(20);
    this.activePanel = 'help';

    const cx = W / 2;
    const panelW = px(460);
    const panelH = px(500);
    const panelX = cx;
    const panelY = H / 2;

    // Dimmed backdrop
    const backdrop = this.add.rectangle(cx, H / 2, W, H, 0x000000, 0.72).setInteractive();
    this.helpContainer.add(backdrop);

    // Panel frame + title
    this.addModalFrame(this.helpContainer, panelX, panelY, panelW, panelH, t('menu.helpPanel.title'));

    const categories: { titleKey: string; keys: [string, string][] }[] = [
      {
        titleKey: 'menu.helpPanel.cat.movement',
        keys: [
          ['W / A / S / D', t('menu.helpPanel.movement.wasd')],
          [t('menu.helpPanel.movement.mouseKey'), t('menu.helpPanel.movement.mouse')],
        ],
      },
      {
        titleKey: 'menu.helpPanel.cat.combat',
        keys: [
          ['1 - 6', t('menu.helpPanel.combat.skills')],
          ['SPACE', t('menu.helpPanel.combat.dodge')],
          ['Q', t('menu.helpPanel.combat.target')],
          ['TAB', t('menu.helpPanel.combat.autoCombat')],
          [t('menu.helpPanel.combat.teleportKey'), t('menu.helpPanel.combat.teleport')],
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
          ['P', t('menu.helpPanel.ui.pets')],
          ['U', t('menu.helpPanel.ui.companion')],
          ['O', t('menu.helpPanel.ui.audio')],
          ['ESC', t('menu.helpPanel.ui.escape')],
        ],
      },
    ];

    let y = panelY - panelH / 2 + px(70);
    const leftX = panelX - panelW / 2 + px(30);
    const rightX = panelX + panelW / 2 - px(30);

    for (const cat of categories) {
      // Category title
      this.helpContainer.add(this.add.text(leftX, y, t(cat.titleKey), {
        fontSize: fs(14), color: '#e8c77a', fontFamily: MENU_FONT, fontStyle: 'bold',
      }).setOrigin(0, 0.5));
      y += px(22);

      for (const [key, desc] of cat.keys) {
        // Key label as a key-cap
        const keyT = this.add.text(leftX + px(14), y, key, {
          fontSize: fs(11), color: '#f0dcae', fontFamily: MENU_FONT, fontStyle: 'bold',
        }).setOrigin(0, 0.5);
        const cap = this.add.graphics();
        cap.fillStyle(0x1d1a21, 1);
        cap.fillRoundedRect(keyT.x - px(6), y - px(8), keyT.width + px(12), px(16), px(3));
        cap.lineStyle(1, 0x8a7a64, 1);
        cap.strokeRoundedRect(keyT.x - px(6), y - px(8), keyT.width + px(12), px(16), px(3));
        this.helpContainer.add(cap);
        this.helpContainer.add(keyT);
        // Description
        this.helpContainer.add(this.add.text(rightX, y, desc, {
          fontSize: fs(12), color: '#888880', fontFamily: '"Noto Sans SC", "Noto Sans TC", sans-serif',
        }).setOrigin(1, 0.5));
        y += px(18);
      }
      y += px(10);
    }

    // Close button
    const closeBtn = this.menuButton(panelX, panelY + panelH / 2 - px(28), px(140), px(32), t('menu.backShort'), () => {
      this.helpContainer?.destroy();
      this.helpContainer = null;
      this.activePanel = 'menu';
    }, 'secondary', 14);
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
    const panelH = px(530);
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
    const backdrop = this.add.rectangle(cx, H / 2, W, H, 0x000000, 0.72).setInteractive();
    this.jukeboxContainer.add(backdrop);

    // ---- Panel + Header ----
    this.addModalFrame(this.jukeboxContainer, panelX, panelY, panelW, panelH, t('menu.jukebox.header'));

    const totalDur = JUKEBOX_TRACKS.reduce((sum, tr) => sum + tr.duration, 0);
    this.jukeboxContainer.add(this.add.text(panelX, panelTop + px(56),
      t('menu.jukebox.subtitle', { count: String(JUKEBOX_TRACKS.length), duration: fmtTime(totalDur) }), {
      fontSize: fs(11), color: '#666660', fontFamily: '"Noto Sans SC", "Noto Sans TC", sans-serif',
    }).setOrigin(0.5));

    // ---- Track list ----
    const listTop = panelTop + px(70);
    const rowH = px(30);

    // Alternating row backgrounds
    for (let i = 0; i < JUKEBOX_TRACKS.length; i++) {
      if (i % 2 === 1) {
        this.jukeboxContainer.add(
          this.add.rectangle(panelX, listTop + i * rowH + rowH / 2, panelW - px(28), rowH, 0x000000, 0.25)
        );
      }
    }

    // Active track highlight
    const highlight = this.add.rectangle(panelX, listTop + rowH / 2, panelW - px(28), rowH, 0xd4a54a, 0.16).setStrokeStyle(1, 0xd4a54a, 0.5);
    this.jukeboxContainer.add(highlight);

    const numTexts: Phaser.GameObjects.Text[] = [];
    const titleTexts: Phaser.GameObjects.Text[] = [];
    const durTexts: Phaser.GameObjects.Text[] = [];

    for (let i = 0; i < JUKEBOX_TRACKS.length; i++) {
      const track = JUKEBOX_TRACKS[i];
      const rowY = listTop + i * rowH + rowH / 2;

      const hit = this.add.rectangle(panelX, rowY, panelW - px(28), rowH, 0x000000, 0)
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
      this.add.rectangle(innerLeft + innerW / 2, progY, innerW, progH + px(2), 0x07060a, 1).setStrokeStyle(1, 0x4a4250, 1)
    );

    const progFill = this.add.graphics();
    this.jukeboxContainer.add(progFill);

    // Larger click area for seeking
    const progHit = this.add.rectangle(innerLeft + innerW / 2, progY, innerW, px(16), 0x000000, 0)
      .setInteractive({ useHandCursor: true });
    progHit.on('pointerdown', (pointer: Phaser.Input.Pointer) => {
      const ratio = Math.max(0, Math.min(1, ((pointer.x / RENDER_SCALE) - innerLeft) / innerW));
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
    const closeBtn = this.menuButton(panelX, panelTop + panelH - px(30), px(140), px(32), t('menu.backShort'), () => doClose(), 'secondary', 14);
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
    const panelH = px(600);
    const panelX = cx;
    const panelY = H / 2;
    const panelTop = panelY - panelH / 2;
    const panelLeft = panelX - panelW / 2;
    const innerLeft = panelLeft + px(24);
    const innerRight = panelLeft + panelW - px(24);

    // Backdrop
    const backdrop = this.add.rectangle(cx, H / 2, W, H, 0x000000, 0.72).setInteractive();
    this.creditsContainer.add(backdrop);

    // Panel + title
    this.addModalFrame(this.creditsContainer, panelX, panelY, panelW, panelH, t('menu.creditsPanel.title'));

    let y = panelTop + px(70);

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
    const closeBtn = this.menuButton(panelX, panelTop + panelH - px(30), px(140), px(32), t('menu.backShort'), () => {
      this.creditsContainer?.destroy();
      this.creditsContainer = null;
      this.activePanel = 'menu';
    }, 'secondary', 14);
    backdrop.on('pointerdown', () => {
      this.creditsContainer?.destroy();
      this.creditsContainer = null;
      this.activePanel = 'menu';
    });
    this.creditsContainer.add(closeBtn);
  }

  private async loadGame(save: SaveData): Promise<void> {
    await ensureGameplayScenes(this.game);
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
    const diffTitle = this.add.text(cx, px(272), t('menu.difficulty.title'), {
      fontSize: fs(22),
      color: UI_COLORS.parchment,
      fontFamily: MENU_TITLE_FONT,
      fontStyle: 'bold',
      stroke: '#120b04', strokeThickness: Math.round(3 * DPR),
    }).setOrigin(0.5);
    this.difficultyContainer.add(addTitleFlourishes(this, cx, px(272), diffTitle.width));
    this.difficultyContainer.add(diffTitle);

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
      const y = px(340) + i * px(76);
      const state = states[diff];
      const isLocked = state === 'locked';
      const isCompleted = state === 'completed';
      const isCurrent = diff === currentDiff;
      const cardW = px(360), cardH = px(64);

      const frame = addFrame(this, cx - cardW / 2, y - cardH / 2, cardW, cardH, {
        variant: 'tooltip', accent: isLocked ? 0x3f3845 : DIFF_COLORS[diff],
      });
      if (isLocked) frame.setAlpha(0.6);
      this.difficultyContainer!.add(frame);
      const bg = this.add.rectangle(cx, y, cardW, cardH, 0xffc860, 0);

      if (!isLocked) {
        bg.setInteractive({ useHandCursor: true });
        bg.on('pointerover', () => { bg.setFillStyle(0xffc860, 0.07); frame.setTint(0xfff0d0); });
        bg.on('pointerout', () => { bg.setFillStyle(0xffc860, 0); frame.clearTint(); });
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

      const textColor = isLocked ? '#6a635c' : (isCurrent ? '#ffffff' : DIFF_TEXT_COLORS[diff]);

      this.difficultyContainer!.add(this.add.text(cx, y - px(10), label, {
        fontSize: fs(20),
        color: textColor,
        fontFamily: MENU_FONT,
        fontStyle: 'bold',
        stroke: '#000000', strokeThickness: Math.round(3 * DPR),
      }).setOrigin(0.5));

      // Description text
      const descText = isLocked ? t('menu.difficulty.locked') : t(DIFF_DESC_KEYS[diff]);
      this.difficultyContainer!.add(this.add.text(cx, y + px(14), descText, {
        fontSize: fs(13),
        color: isLocked ? '#5a5550' : UI_COLORS.textSoft,
        fontFamily: MENU_FONT,
      }).setOrigin(0.5));

      // Current difficulty indicator
      if (isCurrent && !isLocked) {
        const tag = this.add.text(cx + cardW / 2 - px(16), y - px(18), t('menu.difficulty.current'), {
          fontSize: fs(11),
          color: '#ffe7a0',
          fontFamily: MENU_FONT, fontStyle: 'bold',
        }).setOrigin(1, 0.5);
        this.difficultyContainer!.add(tag);
      }
    });

    // Back button
    const backY = px(340) + 3 * px(76) + px(8);
    this.difficultyContainer.add(this.menuButton(cx, backY, px(180), px(38), t('menu.backShort'), () => {
      this.difficultyContainer?.destroy(); this.difficultyContainer = null;
      this.showMainMenu(save);
    }, 'ghost', 15));
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
    const langTitle = this.add.text(cx, px(290), t('menu.language'), {
      fontSize: fs(22),
      color: UI_COLORS.parchment,
      fontFamily: MENU_TITLE_FONT,
      fontStyle: 'bold',
      stroke: '#120b04', strokeThickness: Math.round(3 * DPR),
    }).setOrigin(0.5);
    this.langContainer.add(addTitleFlourishes(this, cx, px(290), langTitle.width));
    this.langContainer.add(langTitle);

    const options: { key: string; label: string; localeId: string }[] = [
      { key: 'menu.langSelect.zhCN', label: t('menu.langSelect.zhCN'), localeId: 'zh-CN' },
      { key: 'menu.langSelect.zhTW', label: t('menu.langSelect.zhTW'), localeId: 'zh-TW' },
      { key: 'menu.langSelect.en', label: t('menu.langSelect.en'), localeId: 'en' },
    ];

    options.forEach((opt, i) => {
      const y = px(360) + i * px(62);
      const isCurrent = opt.localeId === currentLang;
      const btn = this.menuButton(cx, y, px(320), px(46), opt.label, () => {
        // setLocale triggers LOCALE_CHANGED which re-renders via onLocaleChanged
        setLocale(opt.localeId);
      }, isCurrent ? 'primary' : 'secondary', 18);
      this.langContainer!.add(btn);

      if (isCurrent) {
        this.langContainer!.add(this.add.text(cx + px(140), y, t('menu.difficulty.current'), {
          fontSize: fs(11),
          color: '#ffe7a0',
          fontFamily: MENU_FONT, fontStyle: 'bold',
        }).setOrigin(1, 0.5));
      }
    });

    // Back button
    this.langContainer.add(this.menuButton(cx, px(560), px(180), px(38), t('menu.backShort'), () => {
      this.langContainer?.destroy(); this.langContainer = null;
      this.checkForSaves();
    }, 'ghost', 15));
  }

  private async startGame(classId: string): Promise<void> {
    await ensureGameplayScenes(this.game);
    this.scene.start('ZoneScene', { classId, mapId: 'emerald_plains' });
  }
}

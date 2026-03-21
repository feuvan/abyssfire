import Phaser from 'phaser';
import { GAME_WIDTH, GAME_HEIGHT, TEXTURE_SCALE, DPR } from '../config';
import { SaveSystem } from '../systems/SaveSystem';
import { AllClasses } from '../data/classes/index';
import { EventBus, GameEvents } from '../utils/EventBus';
import { audioManager } from '../systems/audio/AudioManager';
import type { SaveData } from '../data/types';

function fs(basePx: number): string {
  return `${Math.round(basePx * DPR)}px`;
}
const px = (n: number) => Math.round(n * DPR);

const W = GAME_WIDTH * DPR;
const H = GAME_HEIGHT * DPR;

export class MenuScene extends Phaser.Scene {
  private menuContainer: Phaser.GameObjects.Container | null = null;
  private classContainer: Phaser.GameObjects.Container | null = null;
  private helpContainer: Phaser.GameObjects.Container | null = null;

  constructor() {
    super({ key: 'MenuScene' });
  }

  create(): void {
    const cx = W / 2;

    this.buildBackground(cx);
    this.buildTitle(cx);
    this.startBGM();
    this.checkForSaves();
  }

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
    lineGfx.setDepth(10);

    // Title
    this.add.text(cx, px(130), 'ABYSSFIRE', {
      fontSize: fs(52),
      color: '#c0934a',
      fontFamily: '"Cinzel", serif',
      fontStyle: 'bold',
      stroke: '#3a2a10',
      strokeThickness: Math.round(4 * DPR),
    }).setOrigin(0.5).setDepth(10);

    this.add.text(cx, px(188), '渊   火', {
      fontSize: fs(32),
      color: '#d4a84b',
      fontFamily: '"Noto Sans SC", sans-serif',
      fontStyle: 'bold',
      stroke: '#2a1a08',
      strokeThickness: Math.round(3 * DPR),
    }).setOrigin(0.5).setDepth(10);

    // Decorative line below title
    const lineGfx2 = this.add.graphics();
    lineGfx2.lineStyle(1, 0xc0934a, 0.3);
    lineGfx2.beginPath();
    lineGfx2.moveTo(cx - px(160), px(215));
    lineGfx2.lineTo(cx + px(160), px(215));
    lineGfx2.strokePath();
    lineGfx2.setDepth(10);

    // Version
    this.add.text(cx, H - px(20), 'v0.6.0 - HD', {
      fontSize: fs(13),
      color: '#333340',
      fontFamily: '"Cinzel", serif',
    }).setOrigin(0.5).setDepth(10);
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

    const cx = W / 2;
    let y = save ? px(300) : px(340);

    if (save) {
      // "Continue" button — shows class name + level
      const classData = AllClasses[save.classId];
      const className = classData?.name ?? save.classId;
      const label = `继续游戏 - ${className} Lv.${save.player.level}`;

      const bg = this.add.rectangle(cx, y, px(320), px(65), 0x12121e, 0.9)
        .setStrokeStyle(1.5, 0xc0934a, 0.8).setInteractive({ useHandCursor: true });
      bg.on('pointerover', () => { bg.setStrokeStyle(2, 0xc0934a, 1); bg.setFillStyle(0x1a1a2e, 0.95); });
      bg.on('pointerout', () => { bg.setStrokeStyle(1.5, 0xc0934a, 0.8); bg.setFillStyle(0x12121e, 0.9); });
      bg.on('pointerdown', () => this.loadGame(save));
      this.menuContainer.add(bg);

      this.menuContainer.add(this.add.text(cx, y - px(6), label, {
        fontSize: fs(20), color: '#e8e0d4', fontFamily: '"Cinzel", "Noto Sans SC", serif',
      }).setOrigin(0.5));
      this.menuContainer.add(this.add.text(cx, y + px(16), '继续你的冒险', {
        fontSize: fs(13), color: '#c0934a', fontFamily: '"Noto Sans SC", sans-serif',
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
    this.menuContainer.add(this.add.text(cx, y, '新的旅程', {
      fontSize: fs(20), color: '#a0907a', fontFamily: '"Cinzel", "Noto Sans SC", serif',
    }).setOrigin(0.5));

    y += px(70);

    // "Help" button
    const helpBg = this.add.rectangle(cx, y, px(320), px(45), 0x12121e, 0.9)
      .setStrokeStyle(1.5, 0x555566, 0.4).setInteractive({ useHandCursor: true });
    helpBg.on('pointerover', () => { helpBg.setStrokeStyle(2, 0x888899, 0.8); helpBg.setFillStyle(0x1a1a2e, 0.95); });
    helpBg.on('pointerout', () => { helpBg.setStrokeStyle(1.5, 0x555566, 0.4); helpBg.setFillStyle(0x12121e, 0.9); });
    helpBg.on('pointerdown', () => this.showHelp());
    this.menuContainer.add(helpBg);
    this.menuContainer.add(this.add.text(cx, y, '帮助', {
      fontSize: fs(16), color: '#888880', fontFamily: '"Noto Sans SC", sans-serif',
    }).setOrigin(0.5));
  }

  private showClassSelection(): void {
    this.classContainer = this.add.container(0, 0).setDepth(10);
    const cx = W / 2;

    this.classContainer.add(this.add.text(cx, px(260), '选 择 职 业', {
      fontSize: fs(20),
      color: '#a0907a',
      fontFamily: '"Noto Sans SC", sans-serif',
    }).setOrigin(0.5));

    const classes = [
      { id: 'warrior', name: '战士 Warrior', desc: '钢铁意志,剑盾无双', color: 0xc0392b, accent: '#e74c3c' },
      { id: 'mage', name: '法师 Mage', desc: '奥术之力,毁天灭地', color: 0x6c3483, accent: '#9b59b6' },
      { id: 'rogue', name: '盗贼 Rogue', desc: '暗影潜行,一击致命', color: 0x1e8449, accent: '#27ae60' },
    ];

    classes.forEach((cls, i) => {
      const y = px(320) + i * px(80);
      const bg = this.add.rectangle(cx, y, px(320), px(65), 0x12121e, 0.9)
        .setStrokeStyle(1.5, cls.color, 0.6)
        .setInteractive({ useHandCursor: true });

      // Animated class icon preview
      const spriteKey = `player_${cls.id}`;
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
      this.classContainer!.add(this.add.text(cx, y - px(12), cls.name, {
        fontSize: fs(20),
        color: '#e8e0d4',
        fontFamily: '"Cinzel", "Noto Sans SC", serif',
      }).setOrigin(0.5));

      this.classContainer!.add(this.add.text(cx, y + px(12), cls.desc, {
        fontSize: fs(14),
        color: cls.accent,
        fontFamily: '"Noto Sans SC", sans-serif',
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
    const backBtn = this.add.text(cx, px(570), '← 返回', {
      fontSize: fs(14), color: '#888', fontFamily: '"Noto Sans SC", sans-serif',
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
    this.helpContainer.add(this.add.text(panelX, panelY - panelH / 2 + px(24), '快捷键', {
      fontSize: fs(22), color: '#c0934a', fontFamily: '"Cinzel", "Noto Sans SC", serif', fontStyle: 'bold',
    }).setOrigin(0.5));

    // Decorative line
    const lineGfx = this.add.graphics();
    lineGfx.lineStyle(1, 0xc0934a, 0.4);
    lineGfx.beginPath();
    lineGfx.moveTo(panelX - px(160), panelY - panelH / 2 + px(44));
    lineGfx.lineTo(panelX + px(160), panelY - panelH / 2 + px(44));
    lineGfx.strokePath();
    this.helpContainer.add(lineGfx);

    const categories: { title: string; keys: [string, string][] }[] = [
      {
        title: '移动',
        keys: [
          ['W / A / S / D', '上 / 左 / 下 / 右移动'],
          ['鼠标左键', '点击移动 / 攻击 / 交互'],
        ],
      },
      {
        title: '战斗',
        keys: [
          ['1 - 6', '使用技能'],
          ['TAB', '切换自动战斗'],
          ['R / 右键', '传送回营地'],
        ],
      },
      {
        title: '界面',
        keys: [
          ['I', '背包'],
          ['C', '角色属性'],
          ['K', '技能树'],
          ['J', '任务日志'],
          ['M', '地图'],
          ['H', '家园'],
          ['O', '音频设置'],
          ['ESC', '返回主菜单'],
        ],
      },
    ];

    let y = panelY - panelH / 2 + px(60);
    const leftX = panelX - panelW / 2 + px(30);
    const rightX = panelX + panelW / 2 - px(30);

    for (const cat of categories) {
      // Category title
      this.helpContainer.add(this.add.text(leftX, y, cat.title, {
        fontSize: fs(14), color: '#d4a84b', fontFamily: '"Noto Sans SC", sans-serif', fontStyle: 'bold',
      }).setOrigin(0, 0.5));
      y += px(22);

      for (const [key, desc] of cat.keys) {
        // Key label
        this.helpContainer.add(this.add.text(leftX + px(8), y, key, {
          fontSize: fs(12), color: '#e0d8cc', fontFamily: '"Noto Sans SC", sans-serif',
        }).setOrigin(0, 0.5));
        // Description
        this.helpContainer.add(this.add.text(rightX, y, desc, {
          fontSize: fs(12), color: '#888880', fontFamily: '"Noto Sans SC", sans-serif',
        }).setOrigin(1, 0.5));
        y += px(18);
      }
      y += px(10);
    }

    // Close button
    const closeBtn = this.add.text(panelX, panelY + panelH / 2 - px(24), '← 返回', {
      fontSize: fs(14), color: '#888', fontFamily: '"Noto Sans SC", sans-serif',
    }).setOrigin(0.5).setInteractive({ useHandCursor: true });
    closeBtn.on('pointerdown', () => {
      this.helpContainer?.destroy();
      this.helpContainer = null;
    });
    backdrop.on('pointerdown', () => {
      this.helpContainer?.destroy();
      this.helpContainer = null;
    });
    this.helpContainer.add(closeBtn);
  }

  private loadGame(save: SaveData): void {
    this.scene.start('ZoneScene', {
      classId: save.classId,
      mapId: save.player.currentMap,
      saveData: save,
    });
  }

  private startGame(classId: string): void {
    this.scene.start('ZoneScene', { classId, mapId: 'emerald_plains' });
  }
}

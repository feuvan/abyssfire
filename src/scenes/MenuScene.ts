import Phaser from 'phaser';
import { GAME_WIDTH, GAME_HEIGHT, TEXTURE_SCALE } from '../config';
import { SaveSystem } from '../systems/SaveSystem';
import { AllClasses } from '../data/classes/index';
import type { SaveData } from '../data/types';

export class MenuScene extends Phaser.Scene {
  private menuContainer: Phaser.GameObjects.Container | null = null;
  private classContainer: Phaser.GameObjects.Container | null = null;

  constructor() {
    super({ key: 'MenuScene' });
  }

  create(): void {
    const cx = GAME_WIDTH / 2;

    // Dark gradient background
    const bgGrad = this.add.graphics();
    bgGrad.fillGradientStyle(0x0a0a1a, 0x0a0a1a, 0x1a1020, 0x1a1020, 1);
    bgGrad.fillRect(0, 0, GAME_WIDTH, GAME_HEIGHT);

    // Ambient particle effect
    for (let i = 0; i < 30; i++) {
      const px = Math.random() * GAME_WIDTH;
      const py = Math.random() * GAME_HEIGHT;
      const p = this.add.circle(px, py, 1 + Math.random() * 1.5, 0xc0934a, 0.1 + Math.random() * 0.2);
      this.tweens.add({
        targets: p, y: py - 40 - Math.random() * 60, alpha: 0, duration: 3000 + Math.random() * 4000,
        repeat: -1, yoyo: false, delay: Math.random() * 3000,
        onRepeat: () => { p.setPosition(Math.random() * GAME_WIDTH, GAME_HEIGHT + 10); p.setAlpha(0.1 + Math.random() * 0.2); },
      });
    }

    // Decorative line
    const lineY = 80;
    const lineGfx = this.add.graphics();
    lineGfx.lineStyle(1, 0xc0934a, 0.3);
    lineGfx.beginPath();
    lineGfx.moveTo(cx - 200, lineY);
    lineGfx.lineTo(cx + 200, lineY);
    lineGfx.strokePath();
    lineGfx.fillStyle(0xc0934a, 0.5);
    lineGfx.fillCircle(cx - 200, lineY, 2);
    lineGfx.fillCircle(cx + 200, lineY, 2);

    // Title
    this.add.text(cx, 130, 'ABYSSFIRE', {
      fontSize: '52px',
      color: '#c0934a',
      fontFamily: '"Cinzel", serif',
      fontStyle: 'bold',
      stroke: '#3a2a10',
      strokeThickness: 4,
    }).setOrigin(0.5);

    this.add.text(cx, 188, '渊   火', {
      fontSize: '26px',
      color: '#d4a84b',
      fontFamily: '"Noto Sans SC", sans-serif',
      fontStyle: 'bold',
      stroke: '#2a1a08',
      strokeThickness: 3,
    }).setOrigin(0.5);

    // Decorative line below title
    const lineGfx2 = this.add.graphics();
    lineGfx2.lineStyle(1, 0xc0934a, 0.3);
    lineGfx2.beginPath();
    lineGfx2.moveTo(cx - 160, 215);
    lineGfx2.lineTo(cx + 160, 215);
    lineGfx2.strokePath();

    // Version
    this.add.text(cx, GAME_HEIGHT - 20, 'v0.6.0 - HD', {
      fontSize: '11px',
      color: '#333340',
      fontFamily: '"Cinzel", serif',
    }).setOrigin(0.5);

    this.checkForSaves();
  }

  private async checkForSaves(): Promise<void> {
    const saveSystem = new SaveSystem();
    const save = await saveSystem.loadAutoSave();
    this.showMainMenu(save ?? null);
  }

  private showMainMenu(save: SaveData | null): void {
    if (this.menuContainer) { this.menuContainer.destroy(); }
    this.menuContainer = this.add.container(0, 0);

    const cx = GAME_WIDTH / 2;
    let y = save ? 300 : 340;

    if (save) {
      // "Continue" button — shows class name + level
      const classData = AllClasses[save.classId];
      const className = classData?.name ?? save.classId;
      const label = `继续游戏 - ${className} Lv.${save.player.level}`;

      const bg = this.add.rectangle(cx, y, 320, 65, 0x12121e, 0.9)
        .setStrokeStyle(1.5, 0xc0934a, 0.8).setInteractive({ useHandCursor: true });
      bg.on('pointerover', () => { bg.setStrokeStyle(2, 0xc0934a, 1); bg.setFillStyle(0x1a1a2e, 0.95); });
      bg.on('pointerout', () => { bg.setStrokeStyle(1.5, 0xc0934a, 0.8); bg.setFillStyle(0x12121e, 0.9); });
      bg.on('pointerdown', () => this.loadGame(save));
      this.menuContainer.add(bg);

      this.menuContainer.add(this.add.text(cx, y - 6, label, {
        fontSize: '16px', color: '#e8e0d4', fontFamily: '"Cinzel", "Noto Sans SC", serif',
      }).setOrigin(0.5));
      this.menuContainer.add(this.add.text(cx, y + 16, '继续你的冒险', {
        fontSize: '11px', color: '#c0934a', fontFamily: '"Noto Sans SC", sans-serif',
      }).setOrigin(0.5));

      y += 90;
    }

    // "New Game" button
    const newBg = this.add.rectangle(cx, y, 320, 55, 0x12121e, 0.9)
      .setStrokeStyle(1.5, 0x555566, 0.6).setInteractive({ useHandCursor: true });
    newBg.on('pointerover', () => { newBg.setStrokeStyle(2, 0x888899, 1); newBg.setFillStyle(0x1a1a2e, 0.95); });
    newBg.on('pointerout', () => { newBg.setStrokeStyle(1.5, 0x555566, 0.6); newBg.setFillStyle(0x12121e, 0.9); });
    newBg.on('pointerdown', () => {
      this.menuContainer?.destroy(); this.menuContainer = null;
      this.showClassSelection();
    });
    this.menuContainer.add(newBg);
    this.menuContainer.add(this.add.text(cx, y, '新的旅程', {
      fontSize: '16px', color: '#a0907a', fontFamily: '"Cinzel", "Noto Sans SC", serif',
    }).setOrigin(0.5));
  }

  private showClassSelection(): void {
    this.classContainer = this.add.container(0, 0);
    const cx = GAME_WIDTH / 2;

    this.classContainer.add(this.add.text(cx, 260, '选 择 职 业', {
      fontSize: '18px',
      color: '#a0907a',
      fontFamily: '"Noto Sans SC", sans-serif',
    }).setOrigin(0.5));

    const classes = [
      { id: 'warrior', name: '战士 Warrior', desc: '钢铁意志,剑盾无双', color: 0xc0392b, accent: '#e74c3c' },
      { id: 'mage', name: '法师 Mage', desc: '奥术之力,毁天灭地', color: 0x6c3483, accent: '#9b59b6' },
      { id: 'rogue', name: '盗贼 Rogue', desc: '暗影潜行,一击致命', color: 0x1e8449, accent: '#27ae60' },
    ];

    classes.forEach((cls, i) => {
      const y = 320 + i * 80;
      const bg = this.add.rectangle(cx, y, 320, 65, 0x12121e, 0.9)
        .setStrokeStyle(1.5, cls.color, 0.6)
        .setInteractive({ useHandCursor: true });

      // Class icon preview
      const spriteKey = `player_${cls.id}`;
      if (this.textures.exists(spriteKey)) {
        const preview = this.add.image(cx - 130, y, spriteKey).setScale(0.7 / TEXTURE_SCALE);
        this.classContainer!.add(preview);
      }

      this.classContainer!.add(bg);
      this.classContainer!.add(this.add.text(cx, y - 12, cls.name, {
        fontSize: '18px',
        color: '#e8e0d4',
        fontFamily: '"Cinzel", "Noto Sans SC", serif',
      }).setOrigin(0.5));

      this.classContainer!.add(this.add.text(cx, y + 12, cls.desc, {
        fontSize: '12px',
        color: cls.accent,
        fontFamily: '"Noto Sans SC", sans-serif',
      }).setOrigin(0.5));

      bg.on('pointerover', () => {
        bg.setStrokeStyle(2, cls.color, 1);
        bg.setFillStyle(0x1a1a2e, 0.95);
      });
      bg.on('pointerout', () => {
        bg.setStrokeStyle(1.5, cls.color, 0.6);
        bg.setFillStyle(0x12121e, 0.9);
      });
      bg.on('pointerdown', () => this.startGame(cls.id));
    });

    // Back button
    const backBtn = this.add.text(cx, 570, '← 返回', {
      fontSize: '12px', color: '#888', fontFamily: '"Noto Sans SC", sans-serif',
    }).setOrigin(0.5).setInteractive({ useHandCursor: true });
    backBtn.on('pointerdown', () => {
      this.classContainer?.destroy(); this.classContainer = null;
      this.checkForSaves();
    });
    this.classContainer.add(backBtn);
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

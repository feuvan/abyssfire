import Phaser from 'phaser';
import { GAME_WIDTH, GAME_HEIGHT } from '../config';
import { EventBus, GameEvents } from '../utils/EventBus';
import { getItemBase } from '../data/items/bases';
import { AllMaps, MapOrder } from '../data/maps/index';
import { audioSystem } from '../systems/AudioSystem';
import type { Player } from '../entities/Player';
import type { ZoneScene } from './ZoneScene';

const FONT = '"Noto Sans SC", sans-serif';
const TITLE_FONT = '"Cinzel", "Noto Sans SC", serif';
const LOG_MAX_LINES = 8;

export class UIScene extends Phaser.Scene {
  private player!: Player;
  private zone!: ZoneScene;

  private hpBar!: Phaser.GameObjects.Rectangle;
  private hpText!: Phaser.GameObjects.Text;
  private manaBar!: Phaser.GameObjects.Rectangle;
  private manaText!: Phaser.GameObjects.Text;
  private expBar!: Phaser.GameObjects.Rectangle;
  private levelText!: Phaser.GameObjects.Text;
  private goldText!: Phaser.GameObjects.Text;
  private autoCombatText!: Phaser.GameObjects.Text;
  private skillSlots: Phaser.GameObjects.Container[] = [];
  private skillCooldownOverlays: Phaser.GameObjects.Rectangle[] = [];
  private logTexts: Phaser.GameObjects.Text[] = [];
  private logMessages: { text: string; type: string }[] = [];
  private questTracker!: Phaser.GameObjects.Text;
  private zoneLabel!: Phaser.GameObjects.Text;

  private inventoryPanel: Phaser.GameObjects.Container | null = null;
  private shopPanel: Phaser.GameObjects.Container | null = null;
  private mapPanel: Phaser.GameObjects.Container | null = null;
  private skillPanel: Phaser.GameObjects.Container | null = null;
  private charPanel: Phaser.GameObjects.Container | null = null;
  private homesteadPanel: Phaser.GameObjects.Container | null = null;
  private minimap!: Phaser.GameObjects.Graphics;

  constructor() {
    super({ key: 'UIScene' });
  }

  init(data: { player: Player; zone: ZoneScene }): void {
    this.player = data.player;
    this.zone = data.zone;
  }

  create(): void {
    this.createHPManaBar();
    this.createExpBar();
    this.createSkillBar();
    this.createLogPanel();
    this.createInfoDisplay();
    this.createQuestTracker();
    this.createMinimap();
    this.setupEventListeners();
  }

  private createHPManaBar(): void {
    const x = 16, barW = 200, barH = 16;
    // Portrait frame
    const portrait = this.add.rectangle(x + 18, 24, 36, 36, 0x1a1a2e)
      .setStrokeStyle(2, 0xc0934a).setDepth(3000);
    const classLetter = this.player.classData.id === 'warrior' ? 'W' : this.player.classData.id === 'mage' ? 'M' : 'R';
    this.add.text(x + 18, 24, classLetter, {
      fontSize: '16px', color: '#c0934a', fontFamily: TITLE_FONT, fontStyle: 'bold',
    }).setOrigin(0.5).setDepth(3001);

    const hpX = x + 42, hpY = 14;
    // HP bar background
    this.add.rectangle(hpX, hpY, barW, barH, 0x1a1212).setOrigin(0, 0.5).setStrokeStyle(1, 0x333333).setDepth(3000);
    this.hpBar = this.add.rectangle(hpX + 1, hpY, barW - 2, barH - 2, 0xc0392b).setOrigin(0, 0.5).setDepth(3001);
    this.hpText = this.add.text(hpX + barW / 2, hpY, '', {
      fontSize: '11px', color: '#fff', fontFamily: FONT,
    }).setOrigin(0.5).setDepth(3002);

    const manaY = hpY + barH + 4;
    this.add.rectangle(hpX, manaY, barW, barH, 0x121226).setOrigin(0, 0.5).setStrokeStyle(1, 0x333333).setDepth(3000);
    this.manaBar = this.add.rectangle(hpX + 1, manaY, barW - 2, barH - 2, 0x2471a3).setOrigin(0, 0.5).setDepth(3001);
    this.manaText = this.add.text(hpX + barW / 2, manaY, '', {
      fontSize: '11px', color: '#fff', fontFamily: FONT,
    }).setOrigin(0.5).setDepth(3002);
  }

  private createExpBar(): void {
    const barW = GAME_WIDTH - 30, barH = 8, y = GAME_HEIGHT - 8;
    this.add.rectangle(15, y, barW, barH, 0x1a1a1a).setOrigin(0, 0.5).setStrokeStyle(1, 0x333333).setDepth(3000);
    this.expBar = this.add.rectangle(15, y, 0, barH - 2, 0x8e44ad).setOrigin(0, 0.5).setDepth(3001);
    this.levelText = this.add.text(15, y - 12, '', {
      fontSize: '11px', color: '#b08cce', fontFamily: FONT,
    }).setOrigin(0, 0.5).setDepth(3002);
  }

  private createSkillBar(): void {
    const slotSize = 42, gap = 5;
    const skills = this.player.classData.skills;
    const totalW = skills.length * (slotSize + gap) - gap;
    const startX = (GAME_WIDTH - totalW) / 2 - 50;
    const y = GAME_HEIGHT - 50;

    // Skill bar background
    this.add.rectangle(GAME_WIDTH / 2 - 20, y, totalW + 140, slotSize + 10, 0x0a0a14, 0.7)
      .setStrokeStyle(1, 0x333344).setDepth(2999);

    this.skillSlots = [];
    this.skillCooldownOverlays = [];

    for (let i = 0; i < skills.length; i++) {
      const x = startX + i * (slotSize + gap);
      const skill = skills[i];
      const container = this.add.container(x + slotSize / 2, y).setDepth(3000);
      const bg = this.add.rectangle(0, 0, slotSize, slotSize, 0x1a1a2e).setStrokeStyle(1.5, 0x555566);
      container.add(bg);
      container.add(this.add.text(0, -6, skill.name.substring(0, 2), {
        fontSize: '14px', color: '#e0d8cc', fontFamily: FONT, fontStyle: 'bold',
      }).setOrigin(0.5));
      container.add(this.add.text(0, 14, `${i + 1}`, {
        fontSize: '10px', color: '#666680', fontFamily: FONT,
      }).setOrigin(0.5));
      const cdOverlay = this.add.rectangle(0, 0, slotSize, slotSize, 0x000000, 0.6).setVisible(false);
      container.add(cdOverlay);
      this.skillCooldownOverlays.push(cdOverlay);
      bg.setInteractive({ useHandCursor: true });
      bg.on('pointerdown', () => EventBus.emit(GameEvents.UI_SKILL_CLICK, { index: i, skillId: skill.id }));
      this.skillSlots.push(container);
    }

    // Auto combat button
    const acX = startX + totalW + gap + 24;
    const acBg = this.add.rectangle(acX, y, 50, slotSize, 0x1a1a2e)
      .setStrokeStyle(1.5, 0x555566).setInteractive({ useHandCursor: true }).setDepth(3000);
    this.autoCombatText = this.add.text(acX, y, 'AUTO\nOFF', {
      fontSize: '10px', color: '#666680', fontFamily: FONT, align: 'center',
    }).setOrigin(0.5).setDepth(3001);
    acBg.on('pointerdown', () => {
      this.player.autoCombat = !this.player.autoCombat;
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: `自动战斗: ${this.player.autoCombat ? '开启' : '关闭'}`, type: 'system' });
    });

    // Inventory button
    const invX = acX + 56;
    const invBg = this.add.rectangle(invX, y, 50, slotSize, 0x1a1a2e)
      .setStrokeStyle(1.5, 0x8e44ad).setInteractive({ useHandCursor: true }).setDepth(3000);
    this.add.text(invX, y, '背包\n(I)', {
      fontSize: '10px', color: '#b08cce', fontFamily: FONT, align: 'center',
    }).setOrigin(0.5).setDepth(3001);
    invBg.on('pointerdown', () => this.toggleInventory());
  }

  private createLogPanel(): void {
    const panelW = 300, panelH = 140, x = 10, y = GAME_HEIGHT - 210;
    this.add.rectangle(x, y, panelW, panelH, 0x000000, 0.55)
      .setOrigin(0, 0).setStrokeStyle(1, 0x222233).setDepth(2999);
    this.add.text(x + 8, y + 4, '战斗日志', {
      fontSize: '10px', color: '#c0934a', fontFamily: FONT,
    }).setDepth(3000);
    for (let i = 0; i < LOG_MAX_LINES; i++) {
      this.logTexts.push(
        this.add.text(x + 8, y + 18 + i * 14, '', {
          fontSize: '10px', color: '#aaa', fontFamily: FONT, wordWrap: { width: panelW - 16 },
        }).setDepth(3000)
      );
    }
  }

  private createInfoDisplay(): void {
    this.goldText = this.add.text(GAME_WIDTH - 16, 16, '', {
      fontSize: '13px', color: '#f1c40f', fontFamily: FONT,
    }).setOrigin(1, 0).setDepth(3000);
    this.zoneLabel = this.add.text(GAME_WIDTH - 16, 34, '', {
      fontSize: '11px', color: '#8a8090', fontFamily: FONT,
    }).setOrigin(1, 0).setDepth(3000);
  }

  private createQuestTracker(): void {
    this.questTracker = this.add.text(GAME_WIDTH - 16, 52, '', {
      fontSize: '10px', color: '#c0934a', fontFamily: FONT, align: 'right', wordWrap: { width: 220 },
    }).setOrigin(1, 0).setDepth(3000);
  }

  private setupEventListeners(): void {
    EventBus.on(GameEvents.LOG_MESSAGE, (data: { text: string; type: string }) => {
      this.logMessages.push(data);
      if (this.logMessages.length > LOG_MAX_LINES) this.logMessages.shift();
      this.updateLogDisplay();
    });
    EventBus.on(GameEvents.SHOP_OPEN, (data: { npcId: string; shopItems: string[]; type: string }) => {
      this.openShop(data);
    });
    EventBus.on(GameEvents.UI_TOGGLE_PANEL, (data: { panel: string }) => {
      if (data.panel === 'inventory') this.toggleInventory();
      if (data.panel === 'map') this.toggleMap();
      if (data.panel === 'skills') this.toggleSkillTree();
      if (data.panel === 'character') this.toggleCharacter();
      if (data.panel === 'homestead') this.toggleHomestead();
    });
    EventBus.on('ui:refresh', (data: { player: Player; zone: ZoneScene }) => {
      this.player = data.player;
      this.zone = data.zone;
    });
  }

  private updateLogDisplay(): void {
    const colors: Record<string, string> = { system: '#c0934a', combat: '#c0392b', loot: '#27ae60', info: '#2e86c1' };
    for (let i = 0; i < LOG_MAX_LINES; i++) {
      if (i < this.logMessages.length) {
        this.logTexts[i].setText(this.logMessages[i].text).setColor(colors[this.logMessages[i].type] ?? '#aaa');
      } else {
        this.logTexts[i].setText('');
      }
    }
  }

  // --- Inventory Panel ---
  private toggleInventory(): void {
    if (this.inventoryPanel) { this.inventoryPanel.destroy(); this.inventoryPanel = null; return; }
    this.closeAllPanels();
    audioSystem.playSFX('click');
    const pw = 400, ph = 440, px = (GAME_WIDTH - pw) / 2, py = 20;
    this.inventoryPanel = this.add.container(px, py).setDepth(4000);
    const bg = this.add.rectangle(0, 0, pw, ph, 0x0f0f1e, 0.95).setOrigin(0, 0).setStrokeStyle(2, 0xc0934a);
    this.inventoryPanel.add(bg);
    this.inventoryPanel.add(this.add.text(pw / 2, 12, '背包', {
      fontSize: '16px', color: '#c0934a', fontFamily: TITLE_FONT, fontStyle: 'bold',
    }).setOrigin(0.5, 0));
    const closeBtn = this.add.text(pw - 16, 8, 'X', {
      fontSize: '16px', color: '#e74c3c', fontFamily: FONT,
    }).setOrigin(0.5, 0).setInteractive({ useHandCursor: true });
    closeBtn.on('pointerdown', () => this.toggleInventory());
    this.inventoryPanel.add(closeBtn);

    // Equipment slots
    const equipSlots = ['helmet', 'armor', 'gloves', 'boots', 'weapon', 'offhand', 'necklace', 'ring1', 'ring2', 'belt'];
    const slotNames = ['头盔', '铠甲', '手套', '鞋子', '武器', '副手', '项链', '戒指1', '戒指2', '腰带'];
    const slotSize = 32;
    equipSlots.forEach((slot, i) => {
      const sx = 12 + (i % 5) * (slotSize + 8);
      const sy = 36 + Math.floor(i / 5) * (slotSize + 16);
      const eq = this.zone.inventorySystem.equipment[slot as keyof typeof this.zone.inventorySystem.equipment];
      const slotBg = this.add.rectangle(sx + slotSize / 2, sy + slotSize / 2, slotSize, slotSize, eq ? this.getQualityColorNum(eq.quality) : 0x222233)
        .setStrokeStyle(1, 0x444455);
      this.inventoryPanel!.add(slotBg);
      this.inventoryPanel!.add(this.add.text(sx + slotSize / 2, sy + slotSize + 3, slotNames[i], {
        fontSize: '8px', color: '#777788', fontFamily: FONT,
      }).setOrigin(0.5, 0));
      if (eq) {
        this.inventoryPanel!.add(this.add.text(sx + slotSize / 2, sy + slotSize / 2, eq.name.charAt(0), {
          fontSize: '12px', color: '#fff', fontFamily: FONT, fontStyle: 'bold',
        }).setOrigin(0.5));
      }
    });

    // Inventory grid
    const inv = this.zone.inventorySystem.inventory;
    const gridStartY = 110;
    const cols = 8;
    inv.forEach((item, i) => {
      const ix = 12 + (i % cols) * (slotSize + 6);
      const iy = gridStartY + Math.floor(i / cols) * (slotSize + 6);
      const itemBg = this.add.rectangle(ix + slotSize / 2, iy + slotSize / 2, slotSize, slotSize, this.getQualityColorNum(item.quality))
        .setStrokeStyle(1, 0x555566).setInteractive({ useHandCursor: true });
      this.inventoryPanel!.add(itemBg);
      this.inventoryPanel!.add(this.add.text(ix + slotSize / 2, iy + slotSize / 2, item.name.charAt(0), {
        fontSize: '12px', color: '#fff', fontFamily: FONT, fontStyle: 'bold',
      }).setOrigin(0.5));
      if (item.quantity > 1) {
        this.inventoryPanel!.add(this.add.text(ix + slotSize - 2, iy + slotSize - 2, `${item.quantity}`, {
          fontSize: '9px', color: '#ffd700', fontFamily: FONT,
        }).setOrigin(1, 1));
      }
      itemBg.on('pointerdown', () => {
        const base = getItemBase(item.baseId);
        if (base && base.slot) {
          this.zone.inventorySystem.equip(item.uid);
        } else if (base && (base.type === 'consumable' || base.type === 'scroll')) {
          const result = this.zone.inventorySystem.useConsumable(item.uid);
          if (result) {
            if (result.effect === 'heal') this.player.hp = Math.min(this.player.maxHp, this.player.hp + result.value);
            if (result.effect === 'mana') this.player.mana = Math.min(this.player.maxMana, this.player.mana + result.value);
          }
        }
        this.toggleInventory(); this.toggleInventory();
      });
    });

    const statsY = gridStartY + Math.ceil(inv.length / cols) * (slotSize + 6) + 10;
    const eqStats = this.zone.inventorySystem.getEquipmentStats();
    const statText = Object.entries(eqStats).map(([k, v]) => `${k}: +${v}`).join('  ');
    this.inventoryPanel.add(this.add.text(12, Math.min(statsY, ph - 30), `装备加成: ${statText || '无'}`, {
      fontSize: '9px', color: '#777788', fontFamily: FONT, wordWrap: { width: pw - 24 },
    }));
  }

  // --- Shop Panel ---
  private openShop(data: { npcId: string; shopItems: string[]; type: string }): void {
    this.closeAllPanels();
    audioSystem.playSFX('click');
    const pw = 340, ph = 380, px = (GAME_WIDTH - pw) / 2, py = 40;
    this.shopPanel = this.add.container(px, py).setDepth(4000);
    this.shopPanel.add(this.add.rectangle(0, 0, pw, ph, 0x0f0f1e, 0.95).setOrigin(0, 0).setStrokeStyle(2, 0xc0934a));
    const title = data.type === 'blacksmith' ? '铁匠铺' : '商店';
    this.shopPanel.add(this.add.text(pw / 2, 12, title, {
      fontSize: '16px', color: '#c0934a', fontFamily: TITLE_FONT, fontStyle: 'bold',
    }).setOrigin(0.5, 0));
    const closeBtn = this.add.text(pw - 16, 8, 'X', {
      fontSize: '16px', color: '#e74c3c', fontFamily: FONT,
    }).setOrigin(0.5, 0).setInteractive({ useHandCursor: true });
    closeBtn.on('pointerdown', () => { this.shopPanel?.destroy(); this.shopPanel = null; });
    this.shopPanel.add(closeBtn);

    data.shopItems.forEach((itemId, i) => {
      const base = getItemBase(itemId);
      if (!base) return;
      const iy = 40 + i * 28;
      if (iy > ph - 30) return;
      const buyPrice = base.sellPrice * 3;
      const canAfford = this.player.gold >= buyPrice;
      this.shopPanel!.add(this.add.text(14, iy, base.name, {
        fontSize: '11px', color: canAfford ? '#e0d8cc' : '#555', fontFamily: FONT,
      }));
      this.shopPanel!.add(this.add.text(pw - 14, iy, `${buyPrice}G`, {
        fontSize: '11px', color: canAfford ? '#f1c40f' : '#555', fontFamily: FONT,
      }).setOrigin(1, 0));
      if (canAfford) {
        const buyBtn = this.add.text(pw - 60, iy, '[买]', {
          fontSize: '11px', color: '#27ae60', fontFamily: FONT,
        }).setInteractive({ useHandCursor: true });
        buyBtn.on('pointerdown', () => {
          if (this.player.gold >= buyPrice) {
            this.player.gold -= buyPrice;
            audioSystem.playSFX('buy');
            const item = this.zone.lootSystem.createItem(itemId, this.player.level, 'normal');
            if (item) { item.identified = true; this.zone.inventorySystem.addItem(item); }
            this.shopPanel?.destroy(); this.shopPanel = null;
            this.openShop(data);
          }
        });
        this.shopPanel!.add(buyBtn);
      }
    });
    this.shopPanel.add(this.add.text(14, ph - 22, `金币: ${this.player.gold}G`, {
      fontSize: '11px', color: '#f1c40f', fontFamily: FONT,
    }));
  }

  // --- World Map ---
  private toggleMap(): void {
    if (this.mapPanel) { this.mapPanel.destroy(); this.mapPanel = null; return; }
    this.closeAllPanels();
    const pw = 480, ph = 220, px = (GAME_WIDTH - pw) / 2, py = 80;
    this.mapPanel = this.add.container(px, py).setDepth(4000);
    this.mapPanel.add(this.add.rectangle(0, 0, pw, ph, 0x0f0f1e, 0.95).setOrigin(0, 0).setStrokeStyle(2, 0x27ae60));
    this.mapPanel.add(this.add.text(pw / 2, 10, '暗烬大陆', {
      fontSize: '16px', color: '#27ae60', fontFamily: TITLE_FONT, fontStyle: 'bold',
    }).setOrigin(0.5, 0));
    const closeBtn = this.add.text(pw - 16, 8, 'X', {
      fontSize: '16px', color: '#e74c3c', fontFamily: FONT,
    }).setOrigin(0.5, 0).setInteractive({ useHandCursor: true });
    closeBtn.on('pointerdown', () => this.toggleMap());
    this.mapPanel.add(closeBtn);

    MapOrder.forEach((mapId, i) => {
      const map = AllMaps[mapId];
      const x = 36 + i * 88, y = 70;
      const isCurrent = (this.zone as any).currentMapId === mapId;
      const color = isCurrent ? 0x27ae60 : 0x1a1a2e;
      this.mapPanel!.add(this.add.rectangle(x, y, 72, 44, color)
        .setStrokeStyle(isCurrent ? 2 : 1, isCurrent ? 0x27ae60 : 0x444455));
      this.mapPanel!.add(this.add.text(x, y, map.name.substring(0, 4), {
        fontSize: '11px', color: '#e0d8cc', fontFamily: FONT,
      }).setOrigin(0.5));
      this.mapPanel!.add(this.add.text(x, y + 28, `Lv.${map.levelRange[0]}-${map.levelRange[1]}`, {
        fontSize: '9px', color: '#888', fontFamily: FONT,
      }).setOrigin(0.5));
      if (i < MapOrder.length - 1) {
        this.mapPanel!.add(this.add.text(x + 42, y, '→', {
          fontSize: '14px', color: '#444455', fontFamily: FONT,
        }).setOrigin(0.5));
      }
    });
    this.mapPanel.add(this.add.text(pw / 2, ph - 20, '按 M 关闭', {
      fontSize: '10px', color: '#555', fontFamily: FONT,
    }).setOrigin(0.5));
  }

  // --- Skill Tree Panel (K) ---
  private toggleSkillTree(): void {
    if (this.skillPanel) { this.skillPanel.destroy(); this.skillPanel = null; return; }
    this.closeAllPanels();
    const pw = 480, ph = 420, px = (GAME_WIDTH - pw) / 2, py = 10;
    this.skillPanel = this.add.container(px, py).setDepth(4000);
    this.skillPanel.add(this.add.rectangle(0, 0, pw, ph, 0x0f0f1e, 0.95).setOrigin(0, 0).setStrokeStyle(2, 0x8e44ad));
    this.skillPanel.add(this.add.text(pw / 2, 10, `技能树 - ${this.player.classData.name}`, {
      fontSize: '15px', color: '#b08cce', fontFamily: TITLE_FONT, fontStyle: 'bold',
    }).setOrigin(0.5, 0));
    this.skillPanel.add(this.add.text(pw / 2, 28, `剩余技能点: ${this.player.freeSkillPoints}`, {
      fontSize: '11px', color: '#f1c40f', fontFamily: FONT,
    }).setOrigin(0.5, 0));
    const closeBtn = this.add.text(pw - 14, 8, 'X', {
      fontSize: '16px', color: '#e74c3c', fontFamily: FONT,
    }).setInteractive({ useHandCursor: true });
    closeBtn.on('pointerdown', () => this.toggleSkillTree());
    this.skillPanel.add(closeBtn);

    const trees = new Map<string, typeof this.player.classData.skills>();
    for (const skill of this.player.classData.skills) {
      if (!trees.has(skill.tree)) trees.set(skill.tree, []);
      trees.get(skill.tree)!.push(skill);
    }
    let treeIdx = 0;
    const treeW = (pw - 24) / Math.max(trees.size, 1);
    for (const [treeName, skills] of trees) {
      const tx = 12 + treeIdx * treeW;
      this.skillPanel.add(this.add.text(tx + treeW / 2, 46, treeName, {
        fontSize: '11px', color: '#c0934a', fontFamily: FONT,
      }).setOrigin(0.5, 0));
      skills.forEach((skill, si) => {
        const sy = 65 + si * 60;
        const level = this.player.getSkillLevel(skill.id);
        const canLevel = this.player.freeSkillPoints > 0 && level < skill.maxLevel;
        const slotBg = this.add.rectangle(tx + treeW / 2, sy + 12, treeW - 14, 52, 0x1a1a2e)
          .setStrokeStyle(1, canLevel ? 0xf1c40f : 0x333344).setOrigin(0.5, 0);
        this.skillPanel!.add(slotBg);
        this.skillPanel!.add(this.add.text(tx + treeW / 2, sy + 16, skill.name, {
          fontSize: '11px', color: '#e0d8cc', fontFamily: FONT,
        }).setOrigin(0.5, 0));
        this.skillPanel!.add(this.add.text(tx + treeW / 2, sy + 32, `Lv.${level}/${skill.maxLevel}  MP:${skill.manaCost}  CD:${(skill.cooldown / 1000).toFixed(1)}s`, {
          fontSize: '9px', color: '#888', fontFamily: FONT,
        }).setOrigin(0.5, 0));
        this.skillPanel!.add(this.add.text(tx + treeW / 2, sy + 46, skill.description, {
          fontSize: '8px', color: '#666', fontFamily: FONT, wordWrap: { width: treeW - 20 },
        }).setOrigin(0.5, 0));
        if (canLevel) {
          const plusBtn = this.add.text(tx + treeW - 14, sy + 16, '+', {
            fontSize: '16px', color: '#27ae60', fontFamily: FONT, fontStyle: 'bold',
          }).setInteractive({ useHandCursor: true });
          plusBtn.on('pointerdown', () => {
            if (this.player.freeSkillPoints > 0) {
              this.player.freeSkillPoints--;
              this.player.skillLevels.set(skill.id, level + 1);
              this.toggleSkillTree(); this.toggleSkillTree();
            }
          });
          this.skillPanel!.add(plusBtn);
        }
      });
      treeIdx++;
    }
  }

  // --- Character Stats Panel (C) ---
  private toggleCharacter(): void {
    if (this.charPanel) { this.charPanel.destroy(); this.charPanel = null; return; }
    this.closeAllPanels();
    const pw = 320, ph = 380, px = (GAME_WIDTH - pw) / 2, py = 30;
    this.charPanel = this.add.container(px, py).setDepth(4000);
    this.charPanel.add(this.add.rectangle(0, 0, pw, ph, 0x0f0f1e, 0.95).setOrigin(0, 0).setStrokeStyle(2, 0x2471a3));
    this.charPanel.add(this.add.text(pw / 2, 10, `角色属性 - ${this.player.classData.name}`, {
      fontSize: '15px', color: '#5dade2', fontFamily: TITLE_FONT, fontStyle: 'bold',
    }).setOrigin(0.5, 0));
    this.charPanel.add(this.add.text(pw / 2, 28, `Lv.${this.player.level}  剩余属性点: ${this.player.freeStatPoints}`, {
      fontSize: '11px', color: '#f1c40f', fontFamily: FONT,
    }).setOrigin(0.5, 0));
    const closeBtn = this.add.text(pw - 14, 8, 'X', {
      fontSize: '16px', color: '#e74c3c', fontFamily: FONT,
    }).setInteractive({ useHandCursor: true });
    closeBtn.on('pointerdown', () => this.toggleCharacter());
    this.charPanel.add(closeBtn);

    const stats: [string, keyof typeof this.player.stats, string][] = [
      ['力量 STR', 'str', '物理伤害/负重'],
      ['敏捷 DEX', 'dex', '闪避/暴击率/攻速'],
      ['体质 VIT', 'vit', '生命值/物理抗性'],
      ['智力 INT', 'int', '魔法伤害/法术抗性'],
      ['精神 SPI', 'spi', '法力值/法力回复'],
      ['幸运 LCK', 'lck', '掉宝率/暴击倍率'],
    ];
    stats.forEach(([label, key, desc], i) => {
      const sy = 50 + i * 40;
      const val = this.player.stats[key];
      this.charPanel!.add(this.add.text(14, sy, label, {
        fontSize: '12px', color: '#e0d8cc', fontFamily: FONT,
      }));
      this.charPanel!.add(this.add.text(140, sy, `${val}`, {
        fontSize: '12px', color: '#fff', fontFamily: FONT, fontStyle: 'bold',
      }));
      this.charPanel!.add(this.add.text(14, sy + 16, desc, {
        fontSize: '8px', color: '#666', fontFamily: FONT,
      }));
      if (this.player.freeStatPoints > 0) {
        const plusBtn = this.add.text(170, sy, '[+]', {
          fontSize: '12px', color: '#27ae60', fontFamily: FONT,
        }).setInteractive({ useHandCursor: true });
        plusBtn.on('pointerdown', () => {
          if (this.player.freeStatPoints > 0) {
            this.player.freeStatPoints--;
            this.player.stats[key]++;
            this.player.recalcDerived();
            this.toggleCharacter(); this.toggleCharacter();
          }
        });
        this.charPanel!.add(plusBtn);
      }
    });

    const dy = 50 + stats.length * 40 + 8;
    const eqStats = this.zone.inventorySystem.getEquipmentStats();
    const derived = [
      `HP: ${Math.ceil(this.player.hp)}/${this.player.maxHp}`,
      `MP: ${Math.ceil(this.player.mana)}/${this.player.maxMana}`,
      `攻击: ${Math.floor(this.player.baseDamage)}${eqStats['damage'] ? ` (+${eqStats['damage']})` : ''}`,
      `防御: ${Math.floor(this.player.defense)}${eqStats['defense'] ? ` (+${eqStats['defense']})` : ''}`,
      `金币: ${this.player.gold}G`,
    ];
    derived.forEach((line, i) => {
      this.charPanel!.add(this.add.text(14, dy + i * 16, line, {
        fontSize: '10px', color: '#888', fontFamily: FONT,
      }));
    });
  }

  // --- Homestead Panel (H) ---
  private toggleHomestead(): void {
    if (this.homesteadPanel) { this.homesteadPanel.destroy(); this.homesteadPanel = null; return; }
    this.closeAllPanels();
    const pw = 420, ph = 400, px = (GAME_WIDTH - pw) / 2, py = 20;
    this.homesteadPanel = this.add.container(px, py).setDepth(4000);
    this.homesteadPanel.add(this.add.rectangle(0, 0, pw, ph, 0x0f0f1e, 0.95).setOrigin(0, 0).setStrokeStyle(2, 0xc0934a));
    this.homesteadPanel.add(this.add.text(pw / 2, 10, '家园', {
      fontSize: '16px', color: '#c0934a', fontFamily: TITLE_FONT, fontStyle: 'bold',
    }).setOrigin(0.5, 0));
    const closeBtn = this.add.text(pw - 14, 8, 'X', {
      fontSize: '16px', color: '#e74c3c', fontFamily: FONT,
    }).setInteractive({ useHandCursor: true });
    closeBtn.on('pointerdown', () => this.toggleHomestead());
    this.homesteadPanel.add(closeBtn);

    const hs = this.zone.homesteadSystem;
    const buildings = hs.getAllBuildings();

    buildings.forEach((b, i) => {
      const sy = 36 + i * 46;
      const lv = hs.getBuildingLevel(b.id);
      const maxed = lv >= b.maxLevel;
      const cost = maxed ? 0 : b.costPerLevel[lv]?.gold ?? 0;
      const canUpgrade = !maxed && this.player.gold >= cost;

      this.homesteadPanel!.add(this.add.text(14, sy, `${b.name} Lv.${lv}/${b.maxLevel}`, {
        fontSize: '11px', color: '#e0d8cc', fontFamily: FONT,
      }));
      this.homesteadPanel!.add(this.add.text(14, sy + 16, b.description, {
        fontSize: '9px', color: '#666', fontFamily: FONT,
      }));
      if (!maxed) {
        const upBtn = this.add.text(pw - 14, sy + 6, `升级 ${cost}G`, {
          fontSize: '10px', color: canUpgrade ? '#27ae60' : '#555', fontFamily: FONT,
        }).setOrigin(1, 0);
        if (canUpgrade) {
          upBtn.setInteractive({ useHandCursor: true });
          upBtn.on('pointerdown', () => {
            const actualCost = hs.upgrade(b.id);
            this.player.gold -= actualCost;
            this.toggleHomestead(); this.toggleHomestead();
          });
        }
        this.homesteadPanel!.add(upBtn);
      } else {
        this.homesteadPanel!.add(this.add.text(pw - 14, sy + 6, '已满级', {
          fontSize: '10px', color: '#c0934a', fontFamily: FONT,
        }).setOrigin(1, 0));
      }
    });

    const petY = 36 + buildings.length * 46 + 10;
    this.homesteadPanel.add(this.add.text(14, petY, '── 宠物 ──', {
      fontSize: '11px', color: '#c0934a', fontFamily: FONT,
    }));
    const pets = hs.pets;
    if (pets.length === 0) {
      this.homesteadPanel.add(this.add.text(14, petY + 18, '暂无宠物 (击杀Boss有机会获得)', {
        fontSize: '9px', color: '#555', fontFamily: FONT,
      }));
    }
    pets.forEach((p, i) => {
      const pd = hs.getAllPets().find(d => d.id === p.petId);
      const isActive = hs.activePet === p.petId;
      this.homesteadPanel!.add(this.add.text(14, petY + 18 + i * 18, `${pd?.name ?? p.petId} Lv.${p.level} ${isActive ? '[激活]' : ''}`, {
        fontSize: '10px', color: isActive ? '#27ae60' : '#888', fontFamily: FONT,
      }));
    });
  }

  // --- Minimap ---
  private createMinimap(): void {
    const size = 100, padding = 10;
    const x = GAME_WIDTH - size - padding, y = padding + 60;
    this.add.rectangle(x + size / 2, y + size / 2, size + 4, size + 4, 0x000000, 0.5)
      .setStrokeStyle(1, 0x333344).setDepth(2999);
    this.minimap = this.add.graphics().setDepth(3000);
    this.minimap.setPosition(x, y);
  }

  private updateMinimap(): void {
    if (!this.minimap || !this.zone) return;
    this.minimap.clear();
    const mapData = AllMaps[(this.zone as any).currentMapId];
    if (!mapData) return;
    const size = 100;
    const sx = size / mapData.cols, sy = size / mapData.rows;
    const tileColors: Record<number, number> = {
      0: 0x4a8c3f, 1: 0x8b7355, 2: 0x6a6a6a, 3: 0x1a5276, 4: 0x4a4a4a, 5: 0x9e7c52,
    };
    for (let r = 0; r < mapData.rows; r++) {
      for (let c = 0; c < mapData.cols; c++) {
        const color = tileColors[mapData.tiles[r][c]] ?? 0x222222;
        this.minimap.fillStyle(color, 0.75);
        this.minimap.fillRect(c * sx, r * sy, Math.ceil(sx), Math.ceil(sy));
      }
    }
    // Player dot
    this.minimap.fillStyle(0x5dade2);
    this.minimap.fillCircle(this.player.tileCol * sx, this.player.tileRow * sy, 3);
    // Exits
    for (const exit of mapData.exits) {
      this.minimap.fillStyle(0x00e676);
      this.minimap.fillRect(exit.col * sx - 1.5, exit.row * sy - 1.5, 4, 4);
    }
  }

  private closeAllPanels(): void {
    if (this.inventoryPanel) { this.inventoryPanel.destroy(); this.inventoryPanel = null; }
    if (this.shopPanel) { this.shopPanel.destroy(); this.shopPanel = null; }
    if (this.mapPanel) { this.mapPanel.destroy(); this.mapPanel = null; }
    if (this.skillPanel) { this.skillPanel.destroy(); this.skillPanel = null; }
    if (this.charPanel) { this.charPanel.destroy(); this.charPanel = null; }
    if (this.homesteadPanel) { this.homesteadPanel.destroy(); this.homesteadPanel = null; }
  }

  private getQualityColorNum(quality: string): number {
    switch (quality) {
      case 'magic': return 0x2471a3;
      case 'rare': return 0xc0934a;
      case 'legendary': return 0xd35400;
      case 'set': return 0x1e8449;
      default: return 0x222233;
    }
  }

  update(time: number): void {
    if (!this.player) return;
    const barW = 198;
    const hpR = this.player.hp / this.player.maxHp;
    this.hpBar.width = barW * Math.max(0, hpR);
    this.hpText.setText(`${Math.ceil(this.player.hp)}/${this.player.maxHp}`);
    const manaR = this.player.mana / this.player.maxMana;
    this.manaBar.width = barW * Math.max(0, manaR);
    this.manaText.setText(`${Math.ceil(this.player.mana)}/${this.player.maxMana}`);
    const expN = this.player.expToNextLevel();
    this.expBar.width = (GAME_WIDTH - 32) * (this.player.exp / expN);
    this.levelText.setText(`Lv.${this.player.level} (${this.player.exp}/${expN})`);
    this.goldText.setText(`${this.player.gold} G`);
    this.autoCombatText.setText(`AUTO\n${this.player.autoCombat ? 'ON' : 'OFF'}`)
      .setColor(this.player.autoCombat ? '#27ae60' : '#666680');

    if (this.zone && (this.zone as any).currentMapId) {
      const map = AllMaps[(this.zone as any).currentMapId];
      if (map) this.zoneLabel.setText(map.name);
    }

    const skills = this.player.classData.skills;
    for (let i = 0; i < Math.min(skills.length, this.skillCooldownOverlays.length); i++) {
      const cd = this.player.skillCooldowns.get(skills[i].id) ?? 0;
      this.skillCooldownOverlays[i].setVisible(time < cd);
    }

    if (Math.floor(time / 200) % 2 === 0) this.updateMinimap();

    if (this.zone?.questSystem) {
      const active = this.zone.questSystem.getActiveQuests();
      const lines = active.slice(0, 3).map(({ quest, progress }) => {
        const obj = quest.objectives[0];
        const cur = progress.objectives[0]?.current ?? 0;
        const status = progress.status === 'completed' ? '[完成]' : `(${cur}/${obj.required})`;
        return `${quest.name} ${status}`;
      });
      this.questTracker.setText(lines.join('\n'));
    }
  }
}

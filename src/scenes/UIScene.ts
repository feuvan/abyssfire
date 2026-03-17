import Phaser from 'phaser';
import { GAME_WIDTH, GAME_HEIGHT } from '../config';
import { EventBus, GameEvents } from '../utils/EventBus';
import { getItemBase } from '../data/items/bases';
import { AllMaps, MapOrder } from '../data/maps/index';
import { NPCDefinitions } from '../data/npcs';
import { audioManager } from '../systems/audio/AudioManager';
import type { Player } from '../entities/Player';
import type { ZoneScene } from './ZoneScene';
import type { ItemInstance, WeaponBase, ArmorBase } from '../data/types';

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
  private skillCooldownTexts: Phaser.GameObjects.Text[] = [];
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
  private dialoguePanel: Phaser.GameObjects.Container | null = null;
  private dialogueBackdrop: Phaser.GameObjects.Rectangle | null = null;
  private questLogPanel: Phaser.GameObjects.Container | null = null;
  private questLogTab: 'active' | 'completed' = 'active';
  private questLogPage = 0;
  private questLogSelectedIndex = 0;
  private minimap!: Phaser.GameObjects.Graphics;
  private tooltipContainer: Phaser.GameObjects.Container | null = null;
  private inventoryPage = 0;
  private shopInventoryPage = 0;
  private autoLootText!: Phaser.GameObjects.Text;
  private contextPopup: Phaser.GameObjects.Container | null = null;
  private audioPanel: Phaser.GameObjects.Container | null = null;

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
    this.events.on('shutdown', this.shutdown, this);
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
    const totalSkillW = skills.length * (slotSize + gap) - gap;

    const utilBtnW = 50, utilGap = 6, utilCount = 3;
    const totalUtilW = utilCount * utilBtnW + (utilCount - 1) * utilGap;
    const skillUtilGap = gap + 4;
    const fullBarW = totalSkillW + skillUtilGap + totalUtilW;
    const startX = (GAME_WIDTH - fullBarW) / 2;
    const y = GAME_HEIGHT - 50;
    const bgPad = 8;

    // Skill bar background — centered around all elements
    this.add.rectangle(GAME_WIDTH / 2, y, fullBarW + bgPad * 2, slotSize + 10, 0x0a0a14, 0.7)
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
      const cdText = this.add.text(0, 0, '', {
        fontSize: '12px', color: '#ffffff', fontFamily: FONT, fontStyle: 'bold',
        stroke: '#000000', strokeThickness: 2,
      }).setOrigin(0.5).setVisible(false);
      container.add(cdText);
      this.skillCooldownTexts.push(cdText);
      bg.setInteractive({ useHandCursor: true });
      bg.on('pointerdown', () => EventBus.emit(GameEvents.UI_SKILL_CLICK, { index: i, skillId: skill.id }));
      this.skillSlots.push(container);
    }

    // Utility buttons — evenly spaced after skill slots
    const utilStartX = startX + totalSkillW + skillUtilGap;

    // Auto combat button
    const acX = utilStartX + utilBtnW / 2;
    const acBg = this.add.rectangle(acX, y, utilBtnW, slotSize, 0x1a1a2e)
      .setStrokeStyle(1.5, 0x555566).setInteractive({ useHandCursor: true }).setDepth(3000);
    this.autoCombatText = this.add.text(acX, y, 'AUTO\nOFF', {
      fontSize: '10px', color: '#666680', fontFamily: FONT, align: 'center',
    }).setOrigin(0.5).setDepth(3001);
    acBg.on('pointerdown', () => {
      this.player.autoCombat = !this.player.autoCombat;
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: `自动战斗: ${this.player.autoCombat ? '开启' : '关闭'}`, type: 'system' });
    });

    // Auto-loot button
    const alX = acX + utilBtnW + utilGap;
    const alBg = this.add.rectangle(alX, y, utilBtnW, slotSize, 0x1a1a2e)
      .setStrokeStyle(1.5, 0x555566).setInteractive({ useHandCursor: true }).setDepth(3000);
    this.autoLootText = this.add.text(alX, y, '拾取\nOFF', {
      fontSize: '10px', color: '#666680', fontFamily: FONT, align: 'center',
    }).setOrigin(0.5).setDepth(3001);
    alBg.on('pointerdown', () => {
      const modes: Array<'off' | 'all' | 'magic' | 'rare'> = ['off', 'all', 'magic', 'rare'];
      const idx = modes.indexOf(this.player.autoLootMode);
      this.player.autoLootMode = modes[(idx + 1) % modes.length];
    });

    // Inventory button
    const invX = alX + utilBtnW + utilGap;
    const invBg = this.add.rectangle(invX, y, utilBtnW, slotSize, 0x1a1a2e)
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
    EventBus.on(GameEvents.NPC_INTERACT, (data: { npcName: string; dialogue: string; actions: { label: string; callback: () => void }[] }) => {
      this.openDialogue(data);
    });
    EventBus.on(GameEvents.UI_TOGGLE_PANEL, (data: { panel: string }) => {
      if (data.panel === 'inventory') this.toggleInventory();
      if (data.panel === 'map') this.toggleMap();
      if (data.panel === 'skills') this.toggleSkillTree();
      if (data.panel === 'character') this.toggleCharacter();
      if (data.panel === 'homestead') this.toggleHomestead();
      if (data.panel === 'quest') this.toggleQuestLog();
      if (data.panel === 'audio') this.toggleAudioSettings();
    });
    EventBus.on('ui:refresh', (data: { player: Player; zone: ZoneScene }) => {
      this.player = data.player;
      this.zone = data.zone;
    });
  }

  private updateLogDisplay(): void {
    const colors: Record<string, string> = { system: '#c0934a', combat: '#c0392b', loot: '#27ae60', info: '#2e86c1' };
    for (let i = 0; i < LOG_MAX_LINES; i++) {
      const txt = this.logTexts[i];
      if (!txt || !txt.active) continue;
      if (i < this.logMessages.length) {
        txt.setText(this.logMessages[i].text).setColor(colors[this.logMessages[i].type] ?? '#aaa');
      } else {
        txt.setText('');
      }
    }
  }

  // --- Inventory Panel ---
  private toggleInventory(): void {
    if (this.inventoryPanel) { this.inventoryPanel.destroy(); this.inventoryPanel = null; this.hideItemTooltip(); this.hideContextPopup(); return; }
    this.closeAllPanels();
    audioManager.playSFX('click');
    const pw = 520, ph = 500, px = (GAME_WIDTH - pw) / 2, py = 10;
    const inv = this.zone.inventorySystem.inventory;
    const itemsPerPage = 50;
    const totalPages = Math.max(1, Math.ceil(inv.length / itemsPerPage));
    if (this.inventoryPage >= totalPages) this.inventoryPage = totalPages - 1;

    this.inventoryPanel = this.add.container(px, py).setDepth(4000);
    this.animatePanelOpen(this.inventoryPanel);
    const bg = this.add.rectangle(0, 0, pw, ph, 0x0f0f1e, 0.95).setOrigin(0, 0).setStrokeStyle(2, 0xc0934a);
    this.inventoryPanel.add(bg);

    // Title with item count and page
    this.inventoryPanel.add(this.add.text(14, 12, `背包 (${inv.length}/${100})`, {
      fontSize: '14px', color: '#c0934a', fontFamily: TITLE_FONT, fontStyle: 'bold',
    }));

    // Sort button
    const sortBtn = this.add.text(pw - 120, 10, '[整理]', {
      fontSize: '12px', color: '#5dade2', fontFamily: FONT,
    }).setInteractive({ useHandCursor: true });
    sortBtn.on('pointerdown', () => {
      this.zone.inventorySystem.sortInventory();
      this.refreshInventory();
    });
    this.inventoryPanel.add(sortBtn);

    // Destroy normal items button
    const destroyBtn = this.add.text(pw - 68, 10, '[销毁]', {
      fontSize: '12px', color: '#e74c3c', fontFamily: FONT,
    }).setInteractive({ useHandCursor: true });
    destroyBtn.on('pointerdown', () => {
      this.zone.inventorySystem.destroyNormalItems();
      this.inventoryPage = 0;
      this.refreshInventory();
    });
    this.inventoryPanel.add(destroyBtn);

    // Close button
    const closeBtn = this.add.text(pw - 16, 10, 'X', {
      fontSize: '14px', color: '#e74c3c', fontFamily: FONT,
    }).setOrigin(0.5, 0).setInteractive({ useHandCursor: true });
    closeBtn.on('pointerdown', () => this.toggleInventory());
    this.inventoryPanel.add(closeBtn);

    // Equipment slots — 5x2 grid
    const equipSlots = ['helmet', 'armor', 'gloves', 'boots', 'weapon', 'offhand', 'necklace', 'ring1', 'ring2', 'belt'];
    const slotNames = ['头盔', '铠甲', '手套', '鞋子', '武器', '副手', '项链', '戒指1', '戒指2', '腰带'];
    const eqSlotSize = 36;
    const eqGap = 6;
    const eqStartX = 14;
    const eqStartY = 36;
    equipSlots.forEach((slot, i) => {
      const sx = eqStartX + (i % 5) * (eqSlotSize + eqGap);
      const sy = eqStartY + Math.floor(i / 5) * (eqSlotSize + 16);
      const eq = this.zone.inventorySystem.equipment[slot as keyof typeof this.zone.inventorySystem.equipment];
      const slotBg = this.add.rectangle(sx + eqSlotSize / 2, sy + eqSlotSize / 2, eqSlotSize, eqSlotSize, eq ? this.getQualityColorNum(eq.quality) : 0x222233)
        .setStrokeStyle(1, 0x444455).setInteractive({ useHandCursor: true });
      this.inventoryPanel!.add(slotBg);
      this.inventoryPanel!.add(this.add.text(sx + eqSlotSize / 2, sy + eqSlotSize + 2, slotNames[i], {
        fontSize: '8px', color: '#777788', fontFamily: FONT,
      }).setOrigin(0.5, 0));
      if (eq) {
        this.inventoryPanel!.add(this.add.text(sx + eqSlotSize / 2, sy + eqSlotSize / 2, eq.name.charAt(0), {
          fontSize: '13px', color: '#fff', fontFamily: FONT, fontStyle: 'bold',
        }).setOrigin(0.5));
        slotBg.on('pointerover', (pointer: Phaser.Input.Pointer) => {
          this.showItemTooltip(eq, pointer.x, pointer.y);
        });
        slotBg.on('pointerout', () => this.hideItemTooltip());
        slotBg.on('pointerdown', () => {
          this.zone.inventorySystem.unequip(slot as any);
          this.refreshInventory();
        });
      }
    });

    // Divider
    const divY = eqStartY + 2 * (eqSlotSize + 16) + 2;
    this.inventoryPanel.add(this.add.rectangle(pw / 2, divY, pw - 20, 1, 0x333344));

    // Inventory grid — 10 cols x 5 rows per page
    const gridStartY = divY + 6;
    const cols = 10;
    const slotSize = 36;
    const gap = 4;
    const pageItems = inv.slice(this.inventoryPage * itemsPerPage, (this.inventoryPage + 1) * itemsPerPage);
    pageItems.forEach((item, i) => {
      const ix = 14 + (i % cols) * (slotSize + gap);
      const iy = gridStartY + Math.floor(i / cols) * (slotSize + gap);
      const itemBg = this.add.rectangle(ix + slotSize / 2, iy + slotSize / 2, slotSize, slotSize, this.getQualityColorNum(item.quality))
        .setStrokeStyle(1, 0x555566).setInteractive({ useHandCursor: true });
      this.inventoryPanel!.add(itemBg);
      this.inventoryPanel!.add(this.add.text(ix + slotSize / 2, iy + slotSize / 2, item.name.charAt(0), {
        fontSize: '13px', color: '#fff', fontFamily: FONT, fontStyle: 'bold',
      }).setOrigin(0.5));
      if (item.quantity > 1) {
        this.inventoryPanel!.add(this.add.text(ix + slotSize - 2, iy + slotSize - 2, `${item.quantity}`, {
          fontSize: '9px', color: '#ffd700', fontFamily: FONT,
        }).setOrigin(1, 1));
      }
      itemBg.on('pointerover', (pointer: Phaser.Input.Pointer) => {
        this.showItemTooltip(item, pointer.x, pointer.y);
      });
      itemBg.on('pointerout', () => this.hideItemTooltip());
      itemBg.on('pointerdown', (pointer: Phaser.Input.Pointer) => {
        this.hideItemTooltip();
        this.showContextPopup(item, pointer.x, pointer.y);
      });
    });

    // Pagination
    const pageY = gridStartY + 5 * (slotSize + gap) + 4;
    if (totalPages > 1) {
      if (this.inventoryPage > 0) {
        const prevBtn = this.add.text(pw / 2 - 80, pageY, '< 上一页', {
          fontSize: '11px', color: '#5dade2', fontFamily: FONT,
        }).setInteractive({ useHandCursor: true });
        prevBtn.on('pointerdown', () => { this.inventoryPage--; this.refreshInventory(); });
        this.inventoryPanel.add(prevBtn);
      }
      this.inventoryPanel.add(this.add.text(pw / 2, pageY, `第${this.inventoryPage + 1}/${totalPages}页`, {
        fontSize: '11px', color: '#888', fontFamily: FONT,
      }).setOrigin(0.5, 0));
      if (this.inventoryPage < totalPages - 1) {
        const nextBtn = this.add.text(pw / 2 + 40, pageY, '下一页 >', {
          fontSize: '11px', color: '#5dade2', fontFamily: FONT,
        }).setInteractive({ useHandCursor: true });
        nextBtn.on('pointerdown', () => { this.inventoryPage++; this.refreshInventory(); });
        this.inventoryPanel.add(nextBtn);
      }
    }

    // Equipment stats at bottom
    const eqStats = this.zone.inventorySystem.getEquipmentStats();
    const statText = Object.entries(eqStats).map(([k, v]) => `${k}: +${v}`).join('  ');
    this.inventoryPanel.add(this.add.text(14, ph - 22, `装备加成: ${statText || '无'}`, {
      fontSize: '9px', color: '#777788', fontFamily: FONT, wordWrap: { width: pw - 28 },
    }));
  }

  // --- Shop Panel (Diablo-style split) ---
  private openShop(data: { npcId: string; shopItems: string[]; type: string }, keepPage = false): void {
    this.closeAllPanels();
    audioManager.playSFX('click');
    if (!keepPage) this.shopInventoryPage = 0;

    // Backdrop for outside-click dismiss
    this.dialogueBackdrop = this.add.rectangle(GAME_WIDTH / 2, GAME_HEIGHT / 2, GAME_WIDTH, GAME_HEIGHT, 0x000000, 0.3)
      .setInteractive().setDepth(3999);
    this.dialogueBackdrop.on('pointerdown', () => {
      this.hideItemTooltip();
      if (this.shopPanel) { this.shopPanel.destroy(); this.shopPanel = null; }
      if (this.dialogueBackdrop) { this.dialogueBackdrop.destroy(); this.dialogueBackdrop = null; }
    });

    const pw = 700, ph = 460, px = (GAME_WIDTH - pw) / 2, py = 40;
    const dividerX = 320;
    this.shopPanel = this.add.container(px, py).setDepth(4000);
    this.animatePanelOpen(this.shopPanel);
    this.shopPanel.add(this.add.rectangle(0, 0, pw, ph, 0x0f0f1e, 0.95).setOrigin(0, 0).setStrokeStyle(2, 0xc0934a));
    const title = data.type === 'blacksmith' ? '铁匠铺' : '商店';
    this.shopPanel.add(this.add.text(pw / 2, 12, title, {
      fontSize: '16px', color: '#c0934a', fontFamily: TITLE_FONT, fontStyle: 'bold',
    }).setOrigin(0.5, 0));
    const closeBtn = this.add.text(pw - 16, 10, 'X', {
      fontSize: '14px', color: '#e74c3c', fontFamily: FONT,
    }).setOrigin(0.5, 0).setInteractive({ useHandCursor: true });
    closeBtn.on('pointerdown', () => {
      this.hideItemTooltip();
      if (this.shopPanel) { this.shopPanel.destroy(); this.shopPanel = null; }
      if (this.dialogueBackdrop) { this.dialogueBackdrop.destroy(); this.dialogueBackdrop = null; }
    });
    this.shopPanel.add(closeBtn);

    // Divider
    this.shopPanel.add(this.add.rectangle(dividerX, 36, 1, ph - 56, 0x333344).setOrigin(0, 0));

    // --- LEFT: Merchant items ---
    this.shopPanel.add(this.add.text(dividerX / 2, 38, '商品列表', {
      fontSize: '12px', color: '#c0934a', fontFamily: FONT,
    }).setOrigin(0.5, 0));

    data.shopItems.forEach((itemId, i) => {
      const base = getItemBase(itemId);
      if (!base) return;
      const iy = 58 + i * 28;
      if (iy > ph - 50) return;
      const buyPrice = base.sellPrice * 3;
      const canAfford = this.player.gold >= buyPrice;
      this.shopPanel!.add(this.add.text(14, iy, base.name, {
        fontSize: '11px', color: canAfford ? '#e0d8cc' : '#555', fontFamily: FONT,
      }));
      this.shopPanel!.add(this.add.text(dividerX - 60, iy, `${buyPrice}G`, {
        fontSize: '11px', color: canAfford ? '#f1c40f' : '#555', fontFamily: FONT,
      }).setOrigin(1, 0));
      if (canAfford) {
        const buyBtn = this.add.text(dividerX - 14, iy, '[买]', {
          fontSize: '11px', color: '#27ae60', fontFamily: FONT,
        }).setOrigin(1, 0).setInteractive({ useHandCursor: true });
        buyBtn.on('pointerdown', () => {
          if (this.player.gold >= buyPrice) {
            this.player.gold -= buyPrice;
            audioManager.playSFX('click');
            const item = this.zone.lootSystem.createItem(itemId, this.player.level, 'normal');
            if (item) { item.identified = true; this.zone.inventorySystem.addItem(item); }
            this.shopPanel?.destroy(); this.shopPanel = null;
            if (this.dialogueBackdrop) { this.dialogueBackdrop.destroy(); this.dialogueBackdrop = null; }
            this.openShop(data, true);
          }
        });
        this.shopPanel!.add(buyBtn);
      }
    });
    this.shopPanel.add(this.add.text(14, ph - 26, `金币: ${this.player.gold}G`, {
      fontSize: '11px', color: '#f1c40f', fontFamily: FONT,
    }));

    // --- RIGHT: Player inventory for selling ---
    const rightX = dividerX + 10;
    const rightW = pw - dividerX - 20;
    this.shopPanel.add(this.add.text(dividerX + rightW / 2 + 10, 38, '你的背包', {
      fontSize: '12px', color: '#c0934a', fontFamily: FONT,
    }).setOrigin(0.5, 0));

    const inv = this.zone.inventorySystem.inventory;
    const shopSlotSize = 32;
    const shopCols = 8;
    const shopGap = 4;
    const shopItemsPerPage = 40;
    const shopTotalPages = Math.max(1, Math.ceil(inv.length / shopItemsPerPage));
    if (this.shopInventoryPage >= shopTotalPages) this.shopInventoryPage = shopTotalPages - 1;
    const shopPageItems = inv.slice(this.shopInventoryPage * shopItemsPerPage, (this.shopInventoryPage + 1) * shopItemsPerPage);
    const shopGridY = 58;

    shopPageItems.forEach((item, i) => {
      const ix = rightX + (i % shopCols) * (shopSlotSize + shopGap);
      const iy = shopGridY + Math.floor(i / shopCols) * (shopSlotSize + shopGap);
      const itemBg = this.add.rectangle(ix + shopSlotSize / 2, iy + shopSlotSize / 2, shopSlotSize, shopSlotSize, this.getQualityColorNum(item.quality))
        .setStrokeStyle(1, 0x555566).setInteractive({ useHandCursor: true });
      this.shopPanel!.add(itemBg);
      this.shopPanel!.add(this.add.text(ix + shopSlotSize / 2, iy + shopSlotSize / 2, item.name.charAt(0), {
        fontSize: '11px', color: '#fff', fontFamily: FONT, fontStyle: 'bold',
      }).setOrigin(0.5));
      if (item.quantity > 1) {
        this.shopPanel!.add(this.add.text(ix + shopSlotSize - 1, iy + shopSlotSize - 1, `${item.quantity}`, {
          fontSize: '8px', color: '#ffd700', fontFamily: FONT,
        }).setOrigin(1, 1));
      }
      itemBg.on('pointerover', (pointer: Phaser.Input.Pointer) => {
        this.showItemTooltip(item, pointer.x, pointer.y);
      });
      itemBg.on('pointerout', () => this.hideItemTooltip());
      itemBg.on('pointerdown', () => {
        this.hideItemTooltip();
        const needsConfirm = item.quality === 'rare' || item.quality === 'legendary' || item.quality === 'set';
        if (needsConfirm) {
          this.showSellConfirm(item, data);
        } else {
          const gold = this.zone.inventorySystem.sellItem(item.uid);
          this.player.gold += gold;
          audioManager.playSFX('click');
          this.shopPanel?.destroy(); this.shopPanel = null;
          if (this.dialogueBackdrop) { this.dialogueBackdrop.destroy(); this.dialogueBackdrop = null; }
          this.openShop(data, true);
        }
      });
    });

    // Shop inventory pagination
    if (shopTotalPages > 1) {
      const pageY = ph - 50;
      if (this.shopInventoryPage > 0) {
        const prevBtn = this.add.text(rightX + rightW / 2 - 60, pageY, '< 上页', {
          fontSize: '10px', color: '#5dade2', fontFamily: FONT,
        }).setInteractive({ useHandCursor: true });
        prevBtn.on('pointerdown', () => {
          this.shopInventoryPage--;
          this.shopPanel?.destroy(); this.shopPanel = null;
          if (this.dialogueBackdrop) { this.dialogueBackdrop.destroy(); this.dialogueBackdrop = null; }
          this.openShop(data, true);
        });
        this.shopPanel.add(prevBtn);
      }
      this.shopPanel.add(this.add.text(rightX + rightW / 2, pageY, `${this.shopInventoryPage + 1}/${shopTotalPages}`, {
        fontSize: '10px', color: '#888', fontFamily: FONT,
      }).setOrigin(0.5, 0));
      if (this.shopInventoryPage < shopTotalPages - 1) {
        const nextBtn = this.add.text(rightX + rightW / 2 + 30, pageY, '下页 >', {
          fontSize: '10px', color: '#5dade2', fontFamily: FONT,
        }).setInteractive({ useHandCursor: true });
        nextBtn.on('pointerdown', () => {
          this.shopInventoryPage++;
          this.shopPanel?.destroy(); this.shopPanel = null;
          if (this.dialogueBackdrop) { this.dialogueBackdrop.destroy(); this.dialogueBackdrop = null; }
          this.openShop(data, true);
        });
        this.shopPanel.add(nextBtn);
      }
    }

    // Gold on right side too
    this.shopPanel.add(this.add.text(rightX, ph - 26, `金币: ${this.player.gold}G`, {
      fontSize: '11px', color: '#f1c40f', fontFamily: FONT,
    }));
  }

  private showSellConfirm(item: ItemInstance, shopData: { npcId: string; shopItems: string[]; type: string }): void {
    this.hideContextPopup();
    const base = getItemBase(item.baseId);
    const sellPrice = base ? base.sellPrice * item.quantity : 1;
    const popW = 180, popH = 60;
    const px = (GAME_WIDTH - popW) / 2, py = (GAME_HEIGHT - popH) / 2;
    this.contextPopup = this.add.container(px, py).setDepth(5002);
    this.contextPopup.add(this.add.rectangle(0, 0, popW, popH, 0x0a0a18, 0.95).setOrigin(0, 0).setStrokeStyle(1, 0xc0934a));
    this.contextPopup.add(this.add.text(popW / 2, 8, `卖出 ${item.name} (${sellPrice}G)?`, {
      fontSize: '10px', color: '#e0d8cc', fontFamily: FONT, wordWrap: { width: popW - 16 },
    }).setOrigin(0.5, 0));
    const yesBtn = this.add.text(popW / 2 - 30, 38, '[确定]', {
      fontSize: '11px', color: '#27ae60', fontFamily: FONT,
    }).setInteractive({ useHandCursor: true });
    yesBtn.on('pointerdown', () => {
      const gold = this.zone.inventorySystem.sellItem(item.uid);
      this.player.gold += gold;
      audioManager.playSFX('click');
      this.hideContextPopup();
      this.shopPanel?.destroy(); this.shopPanel = null;
      if (this.dialogueBackdrop) { this.dialogueBackdrop.destroy(); this.dialogueBackdrop = null; }
      this.openShop(shopData, true);
    });
    this.contextPopup.add(yesBtn);
    const noBtn = this.add.text(popW / 2 + 30, 38, '[取消]', {
      fontSize: '11px', color: '#888', fontFamily: FONT,
    }).setInteractive({ useHandCursor: true });
    noBtn.on('pointerdown', () => this.hideContextPopup());
    this.contextPopup.add(noBtn);
  }

  // --- World Map ---
  private toggleMap(): void {
    if (this.mapPanel) { this.mapPanel.destroy(); this.mapPanel = null; return; }
    this.closeAllPanels();
    const pw = 480, ph = 220, px = (GAME_WIDTH - pw) / 2, py = 80;
    this.mapPanel = this.add.container(px, py).setDepth(4000);
    this.animatePanelOpen(this.mapPanel);
    this.mapPanel.add(this.add.rectangle(0, 0, pw, ph, 0x0f0f1e, 0.95).setOrigin(0, 0).setStrokeStyle(2, 0x27ae60));
    this.mapPanel.add(this.add.text(pw / 2, 10, '渊火', {
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
    this.animatePanelOpen(this.skillPanel);
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
    this.animatePanelOpen(this.charPanel);
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
    this.animatePanelOpen(this.homesteadPanel);
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

    // Quest NPC markers on minimap
    if (this.zone?.questSystem) {
      for (const camp of mapData.camps) {
        for (const npcId of camp.npcs) {
          const npcDef = NPCDefinitions[npcId];
          if (!npcDef || npcDef.type !== 'quest' || !npcDef.quests) continue;
          let hasAvailable = false;
          let hasCompleted = false;
          for (const qid of npcDef.quests) {
            const prog = this.zone.questSystem.progress.get(qid);
            if (prog && prog.status === 'completed') hasCompleted = true;
          }
          if (!hasCompleted) {
            const avail = this.zone.questSystem.getAvailableQuests(npcDef.quests, this.player.level);
            for (const q of avail) {
              if (!this.zone.questSystem.progress.has(q.id)) { hasAvailable = true; break; }
            }
          }
          if (hasCompleted || hasAvailable) {
            const color = hasCompleted ? 0xf1c40f : 0xf1c40f;
            this.minimap.fillStyle(color);
            this.minimap.fillCircle(camp.col * sx, camp.row * sy, 2.5);
          }
        }
      }

      // Monster dots on minimap (red = aggro, orange = nearby)
      const monsters = (this.zone as any).monsters as { tileCol: number; tileRow: number; isAlive: () => boolean; isAggro: () => boolean }[];
      if (monsters) {
        for (const m of monsters) {
          if (!m.isAlive()) continue;
          const color = m.isAggro() ? 0xff4444 : 0xcc6644;
          const alpha = m.isAggro() ? 0.9 : 0.5;
          this.minimap.fillStyle(color, alpha);
          this.minimap.fillCircle(m.tileCol * sx, m.tileRow * sy, m.isAggro() ? 2 : 1.5);
        }
      }

      // Active quest target area markers
      const activeQuests = this.zone.questSystem.getActiveQuests();
      for (const { quest, progress } of activeQuests) {
        if (progress.status !== 'active') continue;
        if (quest.zone !== (this.zone as any).currentMapId) continue;
        // Quest area marker
        if (quest.questArea) {
          const qa = quest.questArea;
          const qColor = quest.category === 'main' ? 0xf1c40f : 0x95a5a6;
          this.minimap.fillStyle(qColor, 0.25);
          this.minimap.fillCircle(qa.col * sx, qa.row * sy, qa.radius * sx);
          this.minimap.lineStyle(1, qColor, 0.6);
          this.minimap.strokeCircle(qa.col * sx, qa.row * sy, qa.radius * sx);
        }
        // Explore objective markers
        for (let i = 0; i < quest.objectives.length; i++) {
          const obj = quest.objectives[i];
          if (obj.type === 'explore' && obj.location && progress.objectives[i].current < obj.required) {
            this.minimap.fillStyle(0xf39c12, 0.5);
            this.minimap.fillRect(obj.location.col * sx - 1.5, obj.location.row * sy - 1.5, 3, 3);
          }
        }
      }
    }
  }

  // --- NPC Dialogue Panel ---
  private openDialogue(data: { npcName: string; dialogue: string; actions: { label: string; callback: () => void }[] }): void {
    this.closeDialogue();
    this.closeAllPanels();
    audioManager.playSFX('click');

    // Full-screen transparent backdrop to catch outside clicks
    this.dialogueBackdrop = this.add.rectangle(GAME_WIDTH / 2, GAME_HEIGHT / 2, GAME_WIDTH, GAME_HEIGHT, 0x000000, 0.3)
      .setInteractive().setDepth(3999);
    this.dialogueBackdrop.on('pointerdown', () => this.closeDialogue());

    const pw = 360, ph = 60 + data.actions.length * 32 + 30;
    const px = (GAME_WIDTH - pw) / 2, py = GAME_HEIGHT / 2 - ph / 2;
    this.dialoguePanel = this.add.container(px, py).setDepth(4000);
    this.animatePanelOpen(this.dialoguePanel);
    const bg = this.add.rectangle(0, 0, pw, ph, 0x0f0f1e, 0.95).setOrigin(0, 0).setStrokeStyle(2, 0xc0934a);
    this.dialoguePanel.add(bg);

    // NPC name
    this.dialoguePanel.add(this.add.text(pw / 2, 10, data.npcName, {
      fontSize: '14px', color: '#c0934a', fontFamily: TITLE_FONT, fontStyle: 'bold',
    }).setOrigin(0.5, 0));

    // Dialogue text
    this.dialoguePanel.add(this.add.text(pw / 2, 32, data.dialogue, {
      fontSize: '12px', color: '#e0d8cc', fontFamily: FONT, wordWrap: { width: pw - 30 },
    }).setOrigin(0.5, 0));

    // Action buttons
    const btnStartY = 60;
    data.actions.forEach((action, i) => {
      const by = btnStartY + i * 32;
      const btnBg = this.add.rectangle(pw / 2, by + 12, pw - 40, 26, 0x1a2a1a)
        .setStrokeStyle(1, 0x27ae60).setInteractive({ useHandCursor: true });
      btnBg.on('pointerdown', () => {
        action.callback();
        this.closeDialogue();
      });
      this.dialoguePanel!.add(btnBg);
      this.dialoguePanel!.add(this.add.text(pw / 2, by + 12, action.label, {
        fontSize: '12px', color: '#27ae60', fontFamily: FONT,
      }).setOrigin(0.5));
    });

    // Close hint
    this.dialoguePanel.add(this.add.text(pw / 2, ph - 16, '点击外部关闭', {
      fontSize: '9px', color: '#555', fontFamily: FONT,
    }).setOrigin(0.5));
  }

  private closeDialogue(): void {
    EventBus.emit(GameEvents.DIALOGUE_CLOSE);
    if (this.dialogueBackdrop) { this.dialogueBackdrop.destroy(); this.dialogueBackdrop = null; }
    if (this.dialoguePanel) { this.dialoguePanel.destroy(); this.dialoguePanel = null; }
  }

  // --- Quest Log Panel ---
  private toggleQuestLog(): void {
    if (this.questLogPanel) {
      this.questLogPanel.destroy();
      this.questLogPanel = null;
      return;
    }
    this.closeAllPanels();
    this.createQuestLogPanel();
  }

  private createQuestLogPanel(): void {
    const pw = 700, ph = 480;
    const px = (GAME_WIDTH - pw) / 2, py = (GAME_HEIGHT - ph) / 2;
    this.questLogPanel = this.add.container(px, py).setDepth(4000);
    this.animatePanelOpen(this.questLogPanel);

    // Background
    const bg = this.add.rectangle(0, 0, pw, ph, 0x0f0f1e, 0.95).setOrigin(0, 0).setStrokeStyle(2, 0xc0934a);
    this.questLogPanel.add(bg);

    // Title
    this.questLogPanel.add(this.add.text(pw / 2, 14, '任务日志', {
      fontSize: '16px', color: '#c0934a', fontFamily: TITLE_FONT, fontStyle: 'bold',
    }).setOrigin(0.5, 0));

    // Close button
    const closeBtn = this.add.text(pw - 16, 10, '✕', {
      fontSize: '16px', color: '#c0392b', fontFamily: FONT,
    }).setOrigin(0.5, 0).setInteractive({ useHandCursor: true });
    closeBtn.on('pointerdown', () => this.toggleQuestLog());
    this.questLogPanel.add(closeBtn);

    // Tab buttons
    const tabY = 38;
    const activeTab = this.add.rectangle(100, tabY, 160, 24, this.questLogTab === 'active' ? 0x1a2a3a : 0x111122)
      .setStrokeStyle(1, 0x2471a3).setInteractive({ useHandCursor: true });
    activeTab.on('pointerdown', () => { this.questLogTab = 'active'; this.questLogPage = 0; this.questLogSelectedIndex = 0; this.refreshQuestLog(); });
    this.questLogPanel.add(activeTab);
    this.questLogPanel.add(this.add.text(100, tabY, '进行中', {
      fontSize: '12px', color: this.questLogTab === 'active' ? '#5dade2' : '#666', fontFamily: FONT,
    }).setOrigin(0.5));

    const completedTab = this.add.rectangle(270, tabY, 160, 24, this.questLogTab === 'completed' ? 0x1a2a3a : 0x111122)
      .setStrokeStyle(1, 0x2471a3).setInteractive({ useHandCursor: true });
    completedTab.on('pointerdown', () => { this.questLogTab = 'completed'; this.questLogPage = 0; this.questLogSelectedIndex = 0; this.refreshQuestLog(); });
    this.questLogPanel.add(completedTab);
    this.questLogPanel.add(this.add.text(270, tabY, '已完成', {
      fontSize: '12px', color: this.questLogTab === 'completed' ? '#5dade2' : '#666', fontFamily: FONT,
    }).setOrigin(0.5));

    // Render quest content
    this.renderQuestLogContent();
  }

  private refreshQuestLog(): void {
    if (!this.questLogPanel) return;
    this.questLogPanel.destroy();
    this.questLogPanel = null;
    this.createQuestLogPanel();
  }

  private renderQuestLogContent(): void {
    if (!this.questLogPanel || !this.zone?.questSystem) return;

    const pw = 700, listW = 240, detailX = 255, detailW = 430;
    const listStartY = 58, itemH = 28, maxItems = 14;

    // Get quest list
    let quests: { quest: import('../data/types').QuestDefinition; progress: import('../data/types').QuestProgress }[];
    if (this.questLogTab === 'active') {
      quests = this.zone.questSystem.getActiveQuests();
    } else {
      quests = [];
      for (const [id, prog] of this.zone.questSystem.progress.entries()) {
        if (prog.status === 'turned_in') {
          const q = this.zone.questSystem.quests.get(id);
          if (q) quests.push({ quest: q, progress: prog });
        }
      }
    }

    // Sort: main first
    quests.sort((a, b) => {
      if (a.quest.category === 'main' && b.quest.category !== 'main') return -1;
      if (a.quest.category !== 'main' && b.quest.category === 'main') return 1;
      return a.quest.level - b.quest.level;
    });

    const totalPages = Math.max(1, Math.ceil(quests.length / maxItems));
    const pageQuests = quests.slice(this.questLogPage * maxItems, (this.questLogPage + 1) * maxItems);

    // Divider line
    this.questLogPanel.add(this.add.rectangle(listW + 7, 58, 1, 400, 0x333344).setOrigin(0, 0));

    // Quest list
    pageQuests.forEach((entry, i) => {
      const y = listStartY + i * itemH;
      const isSelected = i === this.questLogSelectedIndex;
      const listBg = this.add.rectangle(5, y, listW, itemH - 2, isSelected ? 0x1a2a3a : 0x0f0f1e)
        .setOrigin(0, 0).setStrokeStyle(isSelected ? 1 : 0, 0x2471a3)
        .setInteractive({ useHandCursor: true });
      listBg.on('pointerdown', () => { this.questLogSelectedIndex = i; this.refreshQuestLog(); });
      this.questLogPanel!.add(listBg);

      const tagColor = entry.quest.category === 'main' ? '#c0934a' : '#95a5a6';
      const tag = entry.quest.category === 'main' ? '[主]' : '[支]';
      this.questLogPanel!.add(this.add.text(12, y + 5, tag, {
        fontSize: '10px', color: tagColor, fontFamily: FONT, fontStyle: 'bold',
      }));
      const nameColor = this.questLogTab === 'completed' ? '#555' : (isSelected ? '#e0d8cc' : '#aaa');
      this.questLogPanel!.add(this.add.text(36, y + 5, `${entry.quest.name} Lv.${entry.quest.level}`, {
        fontSize: '10px', color: nameColor, fontFamily: FONT,
      }));
    });

    // Pagination
    if (totalPages > 1) {
      const pageY = listStartY + maxItems * itemH + 4;
      if (this.questLogPage > 0) {
        const prevBtn = this.add.text(40, pageY, '◀ 上一页', {
          fontSize: '10px', color: '#5dade2', fontFamily: FONT,
        }).setInteractive({ useHandCursor: true });
        prevBtn.on('pointerdown', () => { this.questLogPage--; this.questLogSelectedIndex = 0; this.refreshQuestLog(); });
        this.questLogPanel.add(prevBtn);
      }
      this.questLogPanel.add(this.add.text(120, pageY, `${this.questLogPage + 1}/${totalPages}`, {
        fontSize: '10px', color: '#666', fontFamily: FONT,
      }));
      if (this.questLogPage < totalPages - 1) {
        const nextBtn = this.add.text(160, pageY, '下一页 ▶', {
          fontSize: '10px', color: '#5dade2', fontFamily: FONT,
        }).setInteractive({ useHandCursor: true });
        nextBtn.on('pointerdown', () => { this.questLogPage++; this.questLogSelectedIndex = 0; this.refreshQuestLog(); });
        this.questLogPanel.add(nextBtn);
      }
    }

    // No quests message
    if (pageQuests.length === 0) {
      this.questLogPanel.add(this.add.text(120, 120, this.questLogTab === 'active' ? '暂无进行中的任务' : '暂无已完成的任务', {
        fontSize: '12px', color: '#555', fontFamily: FONT,
      }).setOrigin(0.5, 0));
      return;
    }

    // Quest detail (right side)
    const selected = pageQuests[this.questLogSelectedIndex] ?? pageQuests[0];
    if (!selected) return;

    let dy = listStartY;

    // Quest name
    this.questLogPanel.add(this.add.text(detailX + detailW / 2, dy, selected.quest.name, {
      fontSize: '15px', color: '#c0934a', fontFamily: TITLE_FONT, fontStyle: 'bold',
    }).setOrigin(0.5, 0));
    dy += 24;

    // Category + Level + Zone
    const zoneNames: Record<string, string> = {
      emerald_plains: '翡翠平原', twilight_forest: '暮色森林',
      anvil_mountains: '铁砧山脉', scorching_desert: '灼热沙漠', abyss_rift: '深渊裂隙',
    };
    const catText = selected.quest.category === 'main' ? '主线任务' : '支线任务';
    const catColor = selected.quest.category === 'main' ? '#c0934a' : '#95a5a6';
    this.questLogPanel.add(this.add.text(detailX + 5, dy, `${catText}  |  Lv.${selected.quest.level}  |  ${zoneNames[selected.quest.zone] ?? selected.quest.zone}`, {
      fontSize: '10px', color: catColor, fontFamily: FONT,
    }));
    dy += 20;

    // Description
    this.questLogPanel.add(this.add.text(detailX + 5, dy, selected.quest.description, {
      fontSize: '11px', color: '#bbb', fontFamily: FONT, wordWrap: { width: detailW - 20 },
    }));
    dy += 40;

    // Objectives header
    this.questLogPanel.add(this.add.text(detailX + 5, dy, '目标:', {
      fontSize: '11px', color: '#e0d8cc', fontFamily: FONT, fontStyle: 'bold',
    }));
    dy += 18;

    // Objectives with progress
    for (let i = 0; i < selected.quest.objectives.length; i++) {
      const obj = selected.quest.objectives[i];
      const cur = selected.progress.objectives[i]?.current ?? 0;
      const done = cur >= obj.required;
      const statusText = done ? '✓' : `${cur}/${obj.required}`;
      const objColor = done ? '#27ae60' : '#e0d8cc';

      this.questLogPanel.add(this.add.text(detailX + 15, dy, `• ${obj.targetName}  ${statusText}`, {
        fontSize: '10px', color: objColor, fontFamily: FONT,
      }));

      // Progress bar
      const barX = detailX + 15, barY = dy + 14, barW = detailW - 40, barH = 4;
      this.questLogPanel.add(this.add.rectangle(barX, barY, barW, barH, 0x1a1a2e).setOrigin(0, 0).setStrokeStyle(1, 0x333344));
      const fillW = Math.min(barW * (cur / obj.required), barW);
      if (fillW > 0) {
        this.questLogPanel.add(this.add.rectangle(barX, barY, fillW, barH, done ? 0x27ae60 : 0x2471a3).setOrigin(0, 0));
      }
      dy += 24;
    }

    dy += 8;

    // Rewards
    this.questLogPanel.add(this.add.text(detailX + 5, dy, '奖励:', {
      fontSize: '11px', color: '#e0d8cc', fontFamily: FONT, fontStyle: 'bold',
    }));
    dy += 18;

    const rewardParts: string[] = [];
    rewardParts.push(`经验 +${selected.quest.rewards.exp}`);
    rewardParts.push(`金币 +${selected.quest.rewards.gold}`);
    if (selected.quest.rewards.items) {
      rewardParts.push(`物品 x${selected.quest.rewards.items.length}`);
    }
    this.questLogPanel.add(this.add.text(detailX + 15, dy, rewardParts.join('  |  '), {
      fontSize: '10px', color: '#f1c40f', fontFamily: FONT,
    }));
    dy += 20;

    // Prereqs
    if (selected.quest.prereqQuests && selected.quest.prereqQuests.length > 0) {
      dy += 4;
      const prereqNames = selected.quest.prereqQuests.map(pid => {
        const pq = this.zone!.questSystem.quests.get(pid);
        return pq ? pq.name : pid;
      });
      this.questLogPanel.add(this.add.text(detailX + 5, dy, `前置任务: ${prereqNames.join(', ')}`, {
        fontSize: '9px', color: '#666', fontFamily: FONT,
      }));
    }
  }

  // --- Item Tooltip ---
  private showItemTooltip(item: ItemInstance, screenX: number, screenY: number): void {
    this.hideItemTooltip();
    const base = getItemBase(item.baseId);
    const tipW = 200;
    const lines: { text: string; color: string; size: string }[] = [];
    const qualityColors: Record<string, string> = {
      normal: '#cccccc', magic: '#5dade2', rare: '#f1c40f', legendary: '#e67e22', set: '#2ecc71',
    };
    const qualityLabels: Record<string, string> = {
      normal: '普通', magic: '魔法', rare: '稀有', legendary: '传奇', set: '套装',
    };
    lines.push({ text: item.name, color: qualityColors[item.quality] ?? '#ccc', size: '12px' });
    lines.push({ text: `${qualityLabels[item.quality] ?? ''} Lv.${item.level}`, color: '#888', size: '9px' });
    if (base) {
      const typeLabels: Record<string, string> = {
        weapon: '武器', armor: '护甲', accessory: '饰品', consumable: '消耗品', gem: '宝石', material: '材料', scroll: '卷轴',
      };
      const slotLabels: Record<string, string> = {
        helmet: '头盔', armor: '铠甲', gloves: '手套', boots: '鞋子', weapon: '武器',
        offhand: '副手', necklace: '项链', ring1: '戒指', ring2: '戒指', belt: '腰带',
      };
      let typeLine = typeLabels[base.type] ?? base.type;
      if (base.slot) typeLine += ` (${slotLabels[base.slot] ?? base.slot})`;
      lines.push({ text: typeLine, color: '#777', size: '9px' });
      if ('baseDamage' in base) {
        const wb = base as WeaponBase;
        lines.push({ text: `伤害: ${wb.baseDamage[0]}-${wb.baseDamage[1]}`, color: '#e0d8cc', size: '10px' });
      }
      if ('baseDefense' in base) {
        const ab = base as ArmorBase;
        lines.push({ text: `防御: ${ab.baseDefense}`, color: '#e0d8cc', size: '10px' });
      }
    }
    if (!item.identified && item.quality !== 'normal') {
      lines.push({ text: '[未鉴定]', color: '#e74c3c', size: '10px' });
    } else {
      for (const affix of item.affixes) {
        lines.push({ text: `${affix.name}: +${affix.value}`, color: '#5dade2', size: '9px' });
      }
    }
    if (item.legendaryEffect) {
      lines.push({ text: item.legendaryEffect, color: '#e67e22', size: '9px' });
    }
    if (base) {
      lines.push({ text: `售价: ${base.sellPrice}G`, color: '#f1c40f', size: '9px' });
    }

    let tipH = 12;
    for (const line of lines) tipH += parseInt(line.size) + 4;
    tipH += 8;

    // Clamp to screen
    let tx = screenX + 12;
    let ty = screenY - 10;
    if (tx + tipW > GAME_WIDTH) tx = screenX - tipW - 12;
    if (ty + tipH > GAME_HEIGHT) ty = GAME_HEIGHT - tipH - 4;
    if (ty < 4) ty = 4;

    const qualityBorderColors: Record<string, number> = {
      normal: 0x555555, magic: 0x5dade2, rare: 0xf1c40f, legendary: 0xe67e22, set: 0x2ecc71,
    };
    const borderColor = qualityBorderColors[item.quality] ?? 0x666677;
    this.tooltipContainer = this.add.container(tx, ty).setDepth(5000);
    this.tooltipContainer.add(this.add.rectangle(0, 0, tipW, tipH, 0x0a0a18, 0.95).setOrigin(0, 0).setStrokeStyle(1.5, borderColor));
    let ly = 6;
    for (const line of lines) {
      this.tooltipContainer.add(this.add.text(8, ly, line.text, {
        fontSize: line.size, color: line.color, fontFamily: FONT, wordWrap: { width: tipW - 16 },
      }));
      ly += parseInt(line.size) + 4;
    }
  }

  private hideItemTooltip(): void {
    if (this.tooltipContainer) { this.tooltipContainer.destroy(); this.tooltipContainer = null; }
  }

  // --- Context Popup ---
  private showContextPopup(item: ItemInstance, screenX: number, screenY: number): void {
    this.hideContextPopup();
    const base = getItemBase(item.baseId);
    const actions: { label: string; callback: () => void }[] = [];
    if (base && base.slot) {
      actions.push({ label: '装备', callback: () => {
        this.zone.inventorySystem.equip(item.uid);
        this.hideContextPopup();
        this.refreshInventory();
      }});
    } else if (base && (base.type === 'consumable' || base.type === 'scroll')) {
      actions.push({ label: '使用', callback: () => {
        const result = this.zone.inventorySystem.useConsumable(item.uid);
        if (result) {
          if (result.effect === 'heal') this.player.hp = Math.min(this.player.maxHp, this.player.hp + result.value);
          if (result.effect === 'mana') this.player.mana = Math.min(this.player.maxMana, this.player.mana + result.value);
        }
        this.hideContextPopup();
        this.refreshInventory();
      }});
    }
    const needsConfirm = item.quality === 'rare' || item.quality === 'legendary' || item.quality === 'set';
    actions.push({ label: '丢弃', callback: () => {
      if (needsConfirm) {
        this.showDiscardConfirm(item);
      } else {
        this.zone.inventorySystem.discardItem(item.uid);
        this.hideContextPopup();
        this.refreshInventory();
      }
    }});

    const popW = 80, btnH = 24;
    const popH = actions.length * btnH + 8;
    let px = screenX;
    let py = screenY;
    if (px + popW > GAME_WIDTH) px = GAME_WIDTH - popW - 4;
    if (py + popH > GAME_HEIGHT) py = GAME_HEIGHT - popH - 4;

    this.contextPopup = this.add.container(px, py).setDepth(5001);
    this.contextPopup.add(this.add.rectangle(0, 0, popW, popH, 0x0a0a18, 0.95).setOrigin(0, 0).setStrokeStyle(1, 0x555566));
    actions.forEach((action, i) => {
      const by = 4 + i * btnH;
      const btnBg = this.add.rectangle(4, by, popW - 8, btnH - 2, 0x1a1a2e).setOrigin(0, 0)
        .setInteractive({ useHandCursor: true });
      btnBg.on('pointerdown', () => action.callback());
      btnBg.on('pointerover', () => btnBg.setFillStyle(0x2a2a3e));
      btnBg.on('pointerout', () => btnBg.setFillStyle(0x1a1a2e));
      this.contextPopup!.add(btnBg);
      this.contextPopup!.add(this.add.text(popW / 2, by + btnH / 2 - 1, action.label, {
        fontSize: '11px', color: action.label === '丢弃' ? '#e74c3c' : '#e0d8cc', fontFamily: FONT,
      }).setOrigin(0.5));
    });
  }

  private hideContextPopup(): void {
    if (this.contextPopup) { this.contextPopup.destroy(); this.contextPopup = null; }
  }

  private showDiscardConfirm(item: ItemInstance): void {
    this.hideContextPopup();
    const popW = 160, popH = 60;
    const px = (GAME_WIDTH - popW) / 2, py = (GAME_HEIGHT - popH) / 2;
    this.contextPopup = this.add.container(px, py).setDepth(5002);
    this.contextPopup.add(this.add.rectangle(0, 0, popW, popH, 0x0a0a18, 0.95).setOrigin(0, 0).setStrokeStyle(1, 0xe74c3c));
    this.contextPopup.add(this.add.text(popW / 2, 8, '确定丢弃?', {
      fontSize: '12px', color: '#e74c3c', fontFamily: FONT,
    }).setOrigin(0.5, 0));
    const yesBtn = this.add.text(popW / 2 - 30, 34, '[确定]', {
      fontSize: '11px', color: '#e74c3c', fontFamily: FONT,
    }).setInteractive({ useHandCursor: true });
    yesBtn.on('pointerdown', () => {
      this.zone.inventorySystem.discardItem(item.uid);
      this.hideContextPopup();
      this.refreshInventory();
    });
    this.contextPopup.add(yesBtn);
    const noBtn = this.add.text(popW / 2 + 30, 34, '[取消]', {
      fontSize: '11px', color: '#27ae60', fontFamily: FONT,
    }).setInteractive({ useHandCursor: true });
    noBtn.on('pointerdown', () => this.hideContextPopup());
    this.contextPopup.add(noBtn);
  }

  private refreshInventory(): void {
    if (this.inventoryPanel) { this.inventoryPanel.destroy(); this.inventoryPanel = null; this.toggleInventory(); }
  }

  // --- Audio Settings Panel ---
  private toggleAudioSettings(): void {
    if (this.audioPanel) { this.audioPanel.destroy(); this.audioPanel = null; return; }
    this.closeAllPanels();
    const pw = 360, ph = 180, px = (GAME_WIDTH - pw) / 2, py = (GAME_HEIGHT - ph) / 2;
    this.audioPanel = this.add.container(px, py).setDepth(4000);
    this.animatePanelOpen(this.audioPanel);
    this.audioPanel.add(this.add.rectangle(0, 0, pw, ph, 0x0f0f1e, 0.95).setOrigin(0, 0).setStrokeStyle(2, 0xc0934a));
    this.audioPanel.add(this.add.text(pw / 2, 12, '音频设置', {
      fontSize: '14px', color: '#c0934a', fontFamily: '"Cinzel", "Noto Sans SC", serif', fontStyle: 'bold',
    }).setOrigin(0.5, 0));

    const settings = audioManager.getSettings();
    const sliderW = 160, sliderH = 10, sliderX = 90, labelX = 14;
    const hitH = 28; // tall hit area for easy clicking

    const makeSlider = (y: number, label: string, initial: number, muted: boolean,
      onVolume: (v: number) => void, onMute: () => boolean) => {
      this.audioPanel!.add(this.add.text(labelX, y, label, { fontSize: '12px', color: '#e0d8cc', fontFamily: '"Noto Sans SC", sans-serif' }));
      const track = this.add.rectangle(sliderX, y + 6, sliderW, sliderH, 0x333344).setOrigin(0, 0.5);
      const fill = this.add.rectangle(sliderX, y + 6, sliderW * initial, sliderH, 0xc0934a).setOrigin(0, 0.5);
      const handle = this.add.circle(sliderX + sliderW * initial, y + 6, 7, 0xe0d8cc);
      const pctText = this.add.text(sliderX + sliderW + 8, y + 1, `${Math.round(initial * 100)}%`, {
        fontSize: '10px', color: '#aaa', fontFamily: '"Noto Sans SC", sans-serif',
      });
      // Invisible hit area covering the full track height
      const hitArea = this.add.rectangle(sliderX + sliderW / 2, y + 6, sliderW + 14, hitH, 0x000000, 0)
        .setInteractive({ useHandCursor: true });
      this.audioPanel!.add([track, fill, handle, pctText, hitArea]);

      let dragging = false;
      const updateSlider = (pointerX: number) => {
        const localX = Math.max(0, Math.min(pointerX - px - sliderX, sliderW));
        const v = localX / sliderW;
        handle.x = sliderX + localX;
        fill.width = localX;
        pctText.setText(`${Math.round(v * 100)}%`);
        onVolume(v);
      };
      hitArea.on('pointerdown', (p: Phaser.Input.Pointer) => { dragging = true; updateSlider(p.x); });
      this.input.on('pointermove', (p: Phaser.Input.Pointer) => { if (dragging) updateSlider(p.x); });
      this.input.on('pointerup', () => { dragging = false; });

      // Mute button
      const muteBtn = this.add.text(sliderX + sliderW + 42, y, muted ? '[静音]' : '[开启]', {
        fontSize: '10px', color: muted ? '#c0392b' : '#27ae60', fontFamily: '"Noto Sans SC", sans-serif',
      }).setInteractive({ useHandCursor: true });
      muteBtn.on('pointerdown', () => {
        const nowMuted = onMute();
        muteBtn.setText(nowMuted ? '[静音]' : '[开启]').setColor(nowMuted ? '#c0392b' : '#27ae60');
      });
      this.audioPanel!.add(muteBtn);
    };

    // BGM slider
    makeSlider(46, '背景音乐', settings.bgmVolume, settings.bgmMuted,
      (v) => audioManager.setMusicVolume(v),
      () => { audioManager.toggleMusicMute(); return audioManager.getSettings().bgmMuted; });

    // SFX slider
    makeSlider(90, '音效', settings.sfxVolume, settings.sfxMuted,
      (v) => audioManager.setSFXVolume(v),
      () => { audioManager.toggleSFXMute(); return audioManager.getSettings().sfxMuted; });

    // Close button
    const closeBtn = this.add.text(pw - 12, 8, 'X', {
      fontSize: '12px', color: '#888', fontFamily: '"Noto Sans SC", sans-serif',
    }).setOrigin(1, 0).setInteractive({ useHandCursor: true });
    closeBtn.on('pointerdown', () => {
      if (this.audioPanel) { this.audioPanel.destroy(); this.audioPanel = null; }
    });
    this.audioPanel.add(closeBtn);
  }

  /** Animate a panel container opening with scale + alpha pop-in */
  private animatePanelOpen(panel: Phaser.GameObjects.Container): void {
    panel.setScale(0.92, 0.92).setAlpha(0);
    this.tweens.add({
      targets: panel,
      scaleX: 1, scaleY: 1, alpha: 1,
      duration: 150,
      ease: 'Back.easeOut',
    });
  }

  private closeAllPanels(): void {
    if (this.inventoryPanel) { this.inventoryPanel.destroy(); this.inventoryPanel = null; }
    if (this.shopPanel) { this.shopPanel.destroy(); this.shopPanel = null; }
    if (this.mapPanel) { this.mapPanel.destroy(); this.mapPanel = null; }
    if (this.skillPanel) { this.skillPanel.destroy(); this.skillPanel = null; }
    if (this.charPanel) { this.charPanel.destroy(); this.charPanel = null; }
    if (this.homesteadPanel) { this.homesteadPanel.destroy(); this.homesteadPanel = null; }
    if (this.questLogPanel) { this.questLogPanel.destroy(); this.questLogPanel = null; }
    if (this.audioPanel) { this.audioPanel.destroy(); this.audioPanel = null; }
    this.hideItemTooltip();
    this.hideContextPopup();
    this.closeDialogue();
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

  shutdown(): void {
    EventBus.removeAllListeners(GameEvents.LOG_MESSAGE);
    EventBus.removeAllListeners(GameEvents.SHOP_OPEN);
    EventBus.removeAllListeners(GameEvents.NPC_INTERACT);
    EventBus.removeAllListeners(GameEvents.UI_TOGGLE_PANEL);
    EventBus.removeAllListeners('ui:refresh');
  }

  /** Interpolate HP bar color: green -> yellow -> red based on ratio */
  private hpColor(ratio: number): number {
    if (ratio > 0.5) {
      // green to yellow (0x2ecc71 -> 0xf1c40f)
      const t = (ratio - 0.5) * 2;
      const r = Math.round(0xf1 + (0x2e - 0xf1) * t);
      const g = Math.round(0xc4 + (0xcc - 0xc4) * t);
      const b = Math.round(0x0f + (0x71 - 0x0f) * t);
      return (r << 16) | (g << 8) | b;
    }
    // yellow to red (0xf1c40f -> 0xc0392b)
    const t = ratio * 2;
    const r = Math.round(0xc0 + (0xf1 - 0xc0) * t);
    const g = Math.round(0x39 + (0xc4 - 0x39) * t);
    const b = Math.round(0x2b + (0x0f - 0x2b) * t);
    return (r << 16) | (g << 8) | b;
  }

  update(time: number): void {
    if (!this.player) return;
    const barW = 198;
    const hpR = Math.max(0, this.player.hp / this.player.maxHp);
    const targetHpW = barW * hpR;
    // Smooth bar interpolation
    this.hpBar.width += (targetHpW - this.hpBar.width) * 0.15;
    this.hpBar.fillColor = this.hpColor(hpR);
    this.hpText.setText(`${Math.ceil(this.player.hp)}/${this.player.maxHp}`);
    // Low HP pulse
    if (hpR < 0.3 && hpR > 0) {
      const pulse = 0.6 + Math.sin(time * 0.008) * 0.4;
      this.hpBar.alpha = pulse;
    } else {
      this.hpBar.alpha = 1;
    }

    const manaR = Math.max(0, this.player.mana / this.player.maxMana);
    const targetManaW = barW * manaR;
    this.manaBar.width += (targetManaW - this.manaBar.width) * 0.15;
    this.manaText.setText(`${Math.ceil(this.player.mana)}/${this.player.maxMana}`);
    const expN = this.player.expToNextLevel();
    this.expBar.width = (GAME_WIDTH - 32) * (this.player.exp / expN);
    this.levelText.setText(`Lv.${this.player.level} (${this.player.exp}/${expN})`);
    this.goldText.setText(`${this.player.gold} G`);
    this.autoCombatText.setText(`AUTO\n${this.player.autoCombat ? 'ON' : 'OFF'}`)
      .setColor(this.player.autoCombat ? '#27ae60' : '#666680');

    // Auto-loot button update
    const alLabels: Record<string, string> = { off: '拾取\nOFF', all: '拾取\n全部', magic: '拾取\n魔法+', rare: '拾取\n稀有+' };
    const alColors: Record<string, string> = { off: '#666680', all: '#e0d8cc', magic: '#2471a3', rare: '#c0934a' };
    this.autoLootText.setText(alLabels[this.player.autoLootMode] ?? '拾取\nOFF')
      .setColor(alColors[this.player.autoLootMode] ?? '#666680');

    if (this.zone && (this.zone as any).currentMapId) {
      const map = AllMaps[(this.zone as any).currentMapId];
      if (map) this.zoneLabel.setText(map.name);
    }

    const skills = this.player.classData.skills;
    for (let i = 0; i < Math.min(skills.length, this.skillCooldownOverlays.length); i++) {
      const cd = this.player.skillCooldowns.get(skills[i].id) ?? 0;
      const remaining = cd - time;
      const onCd = remaining > 0;
      this.skillCooldownOverlays[i].setVisible(onCd);
      // Show remaining seconds on cooldown
      const cdText = this.skillCooldownTexts[i];
      if (onCd) {
        const secs = Math.ceil(remaining / 1000);
        if (cdText?.active) cdText.setVisible(true).setText(`${secs}`);
        // Fade overlay alpha based on cooldown progress
        const totalCd = (skills[i].cooldown ?? 1) * 1000;
        this.skillCooldownOverlays[i].alpha = 0.3 + 0.4 * (remaining / totalCd);
      } else {
        if (cdText?.active) cdText.setVisible(false);
      }
    }

    if (Math.floor(time / 200) % 2 === 0) this.updateMinimap();

    if (this.zone?.questSystem) {
      const active = this.zone.questSystem.getActiveQuests();
      const lines: string[] = [];
      // Sort: main quests first
      const sorted = active.sort((a, b) => {
        if (a.quest.category === 'main' && b.quest.category !== 'main') return -1;
        if (a.quest.category !== 'main' && b.quest.category === 'main') return 1;
        return 0;
      });
      for (const { quest, progress } of sorted) {
        const tag = quest.category === 'main' ? '[主线]' : '[支线]';
        const statusTag = progress.status === 'completed' ? ' [完成]' : '';
        lines.push(`${tag} ${quest.name}${statusTag}`);
        for (let i = 0; i < quest.objectives.length; i++) {
          const obj = quest.objectives[i];
          const cur = progress.objectives[i]?.current ?? 0;
          const done = cur >= obj.required;
          const mark = done ? '✓' : `${cur}/${obj.required}`;
          lines.push(`  ${obj.targetName} ${mark}`);
        }
      }
      this.questTracker.setText(lines.join('\n'));
    }
  }
}

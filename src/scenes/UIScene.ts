import Phaser from 'phaser';
import { GAME_WIDTH, GAME_HEIGHT, DPR } from '../config';
import { EventBus, GameEvents } from '../utils/EventBus';
import { getItemBase } from '../data/items/bases';
import { STAT_DISPLAY } from '../data/items/affixes';
import { AllMaps, MapOrder } from '../data/maps/index';
import { getSkillManaCost, getSkillCooldown, getSkillDamageMultiplier, getSkillBuffValue, getSkillBuffDuration, getSkillAoeRadius } from '../systems/CombatSystem';
import { NPCDefinitions } from '../data/npcs';
import { audioManager } from '../systems/audio/AudioManager';
import type { Player } from '../entities/Player';
import type { ZoneScene } from './ZoneScene';
import type { ItemInstance, WeaponBase, ArmorBase } from '../data/types';
import { MercenarySystem, MERCENARY_DEFS, MERCENARY_TYPES } from '../systems/MercenarySystem';
import type { MercenaryState } from '../systems/MercenarySystem';

const FONT = '"Noto Sans SC", sans-serif';
const TITLE_FONT = '"Cinzel", "Noto Sans SC", serif';
const LOG_MAX_LINES = 8;
const GLOBE_R = Math.round(40 * DPR);

const W = GAME_WIDTH * DPR;
const H = GAME_HEIGHT * DPR;

function fs(basePx: number): string {
  return `${Math.round(basePx * DPR)}px`;
}

const px = (n: number) => Math.round(n * DPR);

function getDirection(dc: number, dr: number): string {
  const angle = Math.atan2(dr, dc) * 180 / Math.PI;
  if (angle >= -22.5 && angle < 22.5) return '东';
  if (angle >= 22.5 && angle < 67.5) return '东南';
  if (angle >= 67.5 && angle < 112.5) return '南';
  if (angle >= 112.5 && angle < 157.5) return '西南';
  if (angle >= 157.5 || angle < -157.5) return '西';
  if (angle >= -157.5 && angle < -112.5) return '西北';
  if (angle >= -112.5 && angle < -67.5) return '北';
  return '东北';
}

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
  private questTracker!: Phaser.GameObjects.Container;
  private questTrackerTexts: Phaser.GameObjects.Text[] = [];
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
  private audioPanelInputCleanup: Array<() => void> = [];
  private companionPanel: Phaser.GameObjects.Container | null = null;
  private nextMinimapRefreshAt = 0;
  private nextQuestTrackerRefreshAt = 0;
  private lastQuestTrackerSignature = '';

  constructor() {
    super({ key: 'UIScene' });
  }

  init(data: { player: Player; zone: ZoneScene }): void {
    this.player = data.player;
    this.zone = data.zone;
  }

  create(): void {
    this.skillSlots = [];
    this.skillCooldownOverlays = [];
    this.skillCooldownTexts = [];
    this.logTexts = [];
    this.logMessages = [];
    this.questTrackerTexts = [];
    this.nextMinimapRefreshAt = 0;
    this.nextQuestTrackerRefreshAt = 0;
    this.lastQuestTrackerSignature = '';
    this.createHPManaBar();
    this.createExpBar();
    this.createSkillBar();
    this.createLogPanel();
    this.createInfoDisplay();
    this.createQuestTracker();
    this.createMinimap();
    this.setupEventListeners();
    this.events.once('shutdown', this.shutdown, this);
  }

  private createHPManaBar(): void {
    // Compute skill bar width to position globes adjacent to it
    const slotSize = px(42), gap = px(5);
    const skills = this.player.classData.skills;
    const totalSkillW = skills.length * (slotSize + gap) - gap;
    const utilBtnW = px(50), utilGap = px(6), utilCount = 3;
    const totalUtilW = utilCount * utilBtnW + (utilCount - 1) * utilGap;
    const skillUtilGap = gap + px(4);
    const fullBarW = totalSkillW + skillUtilGap + totalUtilW;
    const bgPad = px(8);
    const y = H - px(50);

    const barBgLeft = (W - fullBarW) / 2 - bgPad;
    const barBgRight = (W + fullBarW) / 2 + bgPad;
    const globeGap = px(14);
    const hpGlobeX = barBgLeft - globeGap - GLOBE_R;
    const mpGlobeX = barBgRight + globeGap + GLOBE_R;

    // Extended bottom panel background connecting globes and skill bar
    const panelLeft = hpGlobeX - GLOBE_R - px(6);
    const panelRight = mpGlobeX + GLOBE_R + px(6);
    this.add.rectangle(
      (panelLeft + panelRight) / 2, y,
      panelRight - panelLeft, slotSize + px(12),
      0x0a0a14, 0.7,
    ).setStrokeStyle(Math.round(1 * DPR), 0x333344).setDepth(2998);

    // HP Globe (left)
    const hp = this.createGlobe(hpGlobeX, y, GLOBE_R, 0x1a0808, 0xaa2222);
    this.hpBar = hp.fill;
    this.hpText = hp.text;

    // Mana Globe (right)
    const mp = this.createGlobe(mpGlobeX, y, GLOBE_R, 0x08081a, 0x2244aa);
    this.manaBar = mp.fill;
    this.manaText = mp.text;
  }

  private createGlobe(
    cx: number, cy: number, radius: number,
    bgColor: number, fillColor: number,
  ): { fill: Phaser.GameObjects.Rectangle; text: Phaser.GameObjects.Text } {
    const d = 3000;

    // Outer glow
    const glow = this.add.graphics().setDepth(d - 2);
    glow.fillStyle(fillColor, 0.06);
    glow.fillCircle(cx, cy, radius + px(8));

    // Dark background
    const bg = this.add.graphics().setDepth(d - 1);
    bg.fillStyle(bgColor, 1);
    bg.fillCircle(cx, cy, radius);

    // Geometry mask for fill
    const maskGfx = this.make.graphics({});
    maskGfx.fillStyle(0xffffff);
    maskGfx.fillCircle(cx, cy, radius - px(2));
    const mask = maskGfx.createGeometryMask();

    // Fill rectangle (height animates, anchored at bottom via update)
    const fill = this.add.rectangle(cx, cy - radius, radius * 2, radius * 2, fillColor)
      .setOrigin(0.5, 0).setDepth(d);
    fill.setMask(mask);

    // Glass highlight
    const hl = this.add.graphics().setDepth(d + 1);
    hl.fillStyle(0xffffff, 0.08);
    hl.fillEllipse(cx - radius * 0.1, cy - radius * 0.3, radius * 0.65, radius * 0.35);

    // Gold frame
    const frame = this.add.graphics().setDepth(d + 2);
    frame.lineStyle(2.5 * DPR, 0x6a5630, 1);
    frame.strokeCircle(cx, cy, radius);
    frame.lineStyle(1 * DPR, 0xc0934a, 0.6);
    frame.strokeCircle(cx, cy, radius + px(2));

    // Value text
    const text = this.add.text(cx, cy + px(2), '', {
      fontSize: fs(13), color: '#ffffff', fontFamily: FONT, fontStyle: 'bold',
      stroke: '#000000', strokeThickness: Math.round(3 * DPR),
    }).setOrigin(0.5).setDepth(d + 3);

    return { fill, text };
  }

  private createExpBar(): void {
    const barW = W - px(30), barH = px(8), y = H - px(8);
    this.add.rectangle(px(15), y, barW, barH, 0x1a1a1a).setOrigin(0, 0.5).setStrokeStyle(Math.round(1 * DPR), 0x333333).setDepth(3000);
    this.expBar = this.add.rectangle(px(15), y, 0, barH - px(2), 0x8e44ad).setOrigin(0, 0.5).setDepth(3001);
    this.levelText = this.add.text(px(15), y - px(12), '', {
      fontSize: fs(13), color: '#b08cce', fontFamily: FONT,
    }).setOrigin(0, 0.5).setDepth(3002);
  }

  private createSkillBar(): void {
    const slotSize = px(42), gap = px(5);
    const skills = this.player.classData.skills;
    const totalSkillW = skills.length * (slotSize + gap) - gap;

    const utilBtnW = px(50), utilGap = px(6), utilCount = 3;
    const totalUtilW = utilCount * utilBtnW + (utilCount - 1) * utilGap;
    const skillUtilGap = gap + px(4);
    const fullBarW = totalSkillW + skillUtilGap + totalUtilW;
    const startX = (W - fullBarW) / 2;
    const y = H - px(50);
    const bgPad = px(8);

    // Skill bar background — centered around all elements
    this.add.rectangle(W / 2, y, fullBarW + bgPad * 2, slotSize + px(10), 0x0a0a14, 0.7)
      .setStrokeStyle(Math.round(1 * DPR), 0x333344).setDepth(2999);

    this.skillSlots = [];
    this.skillCooldownOverlays = [];
    this.skillCooldownTexts = [];

    for (let i = 0; i < skills.length; i++) {
      const x = startX + i * (slotSize + gap);
      const skill = skills[i];
      const container = this.add.container(x + slotSize / 2, y).setDepth(3000);
      const bg = this.add.rectangle(0, 0, slotSize, slotSize, 0x1a1a2e).setStrokeStyle(1.5 * DPR, 0x555566);
      container.add(bg);
      const iconKey = `skill_icon_${skill.id}`;
      if (this.textures.exists(iconKey)) {
        container.add(this.add.image(0, px(-2), iconKey)
          .setDisplaySize(slotSize - px(6), slotSize - px(6)));
      } else {
        container.add(this.add.text(0, px(-6), skill.name.substring(0, 2), {
          fontSize: fs(16), color: '#e0d8cc', fontFamily: FONT, fontStyle: 'bold',
        }).setOrigin(0.5));
      }
      container.add(this.add.text(0, px(14), `${i + 1}`, {
        fontSize: fs(12), color: '#666680', fontFamily: FONT,
      }).setOrigin(0.5));
      const cdOverlay = this.add.rectangle(0, 0, slotSize, slotSize, 0x000000, 0.6).setVisible(false);
      container.add(cdOverlay);
      this.skillCooldownOverlays.push(cdOverlay);
      const cdText = this.add.text(0, 0, '', {
        fontSize: fs(14), color: '#ffffff', fontFamily: FONT, fontStyle: 'bold',
        stroke: '#000000', strokeThickness: Math.round(2 * DPR),
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
      .setStrokeStyle(1.5 * DPR, 0x555566).setInteractive({ useHandCursor: true }).setDepth(3000);
    this.autoCombatText = this.add.text(acX, y, 'AUTO\nOFF', {
      fontSize: fs(12), color: '#666680', fontFamily: FONT, align: 'center',
    }).setOrigin(0.5).setDepth(3001);
    acBg.on('pointerdown', () => {
      this.player.autoCombat = !this.player.autoCombat;
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: `自动战斗: ${this.player.autoCombat ? '开启' : '关闭'}`, type: 'system' });
    });

    // Auto-loot button
    const alX = acX + utilBtnW + utilGap;
    const alBg = this.add.rectangle(alX, y, utilBtnW, slotSize, 0x1a1a2e)
      .setStrokeStyle(1.5 * DPR, 0x555566).setInteractive({ useHandCursor: true }).setDepth(3000);
    this.autoLootText = this.add.text(alX, y, '拾取\nOFF', {
      fontSize: fs(12), color: '#666680', fontFamily: FONT, align: 'center',
    }).setOrigin(0.5).setDepth(3001);
    alBg.on('pointerdown', () => {
      const modes: Array<'off' | 'all' | 'magic' | 'rare'> = ['off', 'all', 'magic', 'rare'];
      const idx = modes.indexOf(this.player.autoLootMode);
      this.player.autoLootMode = modes[(idx + 1) % modes.length];
    });

    // Inventory button
    const invX = alX + utilBtnW + utilGap;
    const invBg = this.add.rectangle(invX, y, utilBtnW, slotSize, 0x1a1a2e)
      .setStrokeStyle(1.5 * DPR, 0x8e44ad).setInteractive({ useHandCursor: true }).setDepth(3000);
    this.add.text(invX, y, '背包\n(I)', {
      fontSize: fs(12), color: '#b08cce', fontFamily: FONT, align: 'center',
    }).setOrigin(0.5).setDepth(3001);
    invBg.on('pointerdown', () => this.toggleInventory());
  }

  private createLogPanel(): void {
    this.logTexts = [];
    const panelW = px(300), panelH = px(140), x = px(10), y = H - px(210);
    this.add.rectangle(x, y, panelW, panelH, 0x000000, 0.55)
      .setOrigin(0, 0).setStrokeStyle(Math.round(1 * DPR), 0x222233).setDepth(2999);
    this.add.text(x + px(8), y + px(4), '战斗日志', {
      fontSize: fs(12), color: '#c0934a', fontFamily: FONT,
    }).setDepth(3000);
    for (let i = 0; i < LOG_MAX_LINES; i++) {
      this.logTexts.push(
        this.add.text(x + px(8), y + px(18) + i * px(14), '', {
          fontSize: fs(12), color: '#aaa', fontFamily: FONT, wordWrap: { width: panelW - px(16) },
        }).setDepth(3000)
      );
    }
  }

  private createInfoDisplay(): void {
    this.goldText = this.add.text(W - px(16), px(16), '', {
      fontSize: fs(14), color: '#f1c40f', fontFamily: FONT,
    }).setOrigin(1, 0).setDepth(3000);
    this.zoneLabel = this.add.text(W - px(16), px(34), '', {
      fontSize: fs(13), color: '#8a8090', fontFamily: FONT,
    }).setOrigin(1, 0).setDepth(3000);
  }

  private createQuestTracker(): void {
    this.questTrackerTexts = [];
    this.lastQuestTrackerSignature = '';
    this.questTracker = this.add.container(px(16), px(52)).setDepth(3000);
  }

  private setupEventListeners(): void {
    EventBus.on(GameEvents.LOG_MESSAGE, this.handleLogMessage, this);
    EventBus.on(GameEvents.SHOP_OPEN, this.handleShopOpen, this);
    EventBus.on(GameEvents.NPC_INTERACT, this.handleNpcInteract, this);
    EventBus.on(GameEvents.UI_TOGGLE_PANEL, this.handlePanelToggle, this);
    EventBus.on('ui:refresh', this.handleUiRefresh, this);
  }

  private handleLogMessage(data: { text: string; type: string }): void {
    this.logMessages.push(data);
    if (this.logMessages.length > LOG_MAX_LINES) this.logMessages.shift();
    this.updateLogDisplay();
  }

  private handleShopOpen(data: { npcId: string; shopItems: string[]; type: string }): void {
    this.openShop(data);
  }

  private handleNpcInteract(data: { npcName: string; dialogue: string; actions: { label: string; callback: () => void }[] }): void {
    this.openDialogue(data);
  }

  private handlePanelToggle(data: { panel: string }): void {
    if (data.panel === 'inventory') this.toggleInventory();
    if (data.panel === 'map') this.toggleMap();
    if (data.panel === 'skills') this.toggleSkillTree();
    if (data.panel === 'character') this.toggleCharacter();
    if (data.panel === 'homestead') this.toggleHomestead();
    if (data.panel === 'quest') this.toggleQuestLog();
    if (data.panel === 'audio') this.toggleAudioSettings();
    if (data.panel === 'companion') this.toggleCompanion();
  }

  private handleUiRefresh(data: { player: Player; zone: ZoneScene }): void {
    this.player = data.player;
    this.zone = data.zone;
    this.nextMinimapRefreshAt = 0;
    this.nextQuestTrackerRefreshAt = 0;
    this.lastQuestTrackerSignature = '';
  }

  private cleanupAudioPanelInputHandlers(): void {
    for (const cleanup of this.audioPanelInputCleanup) cleanup();
    this.audioPanelInputCleanup = [];
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
    const pw = px(520), ph = px(500), panelX = (W - pw) / 2, panelY = px(10);
    const inv = this.zone.inventorySystem.inventory;
    const itemsPerPage = 50;
    const totalPages = Math.max(1, Math.ceil(inv.length / itemsPerPage));
    if (this.inventoryPage >= totalPages) this.inventoryPage = totalPages - 1;

    this.inventoryPanel = this.add.container(panelX, panelY).setDepth(4000);
    this.animatePanelOpen(this.inventoryPanel);
    const bg = this.add.rectangle(0, 0, pw, ph, 0x0f0f1e, 0.95).setOrigin(0, 0).setStrokeStyle(Math.round(2 * DPR), 0xc0934a);
    this.inventoryPanel.add(bg);

    // Title with item count and page
    this.inventoryPanel.add(this.add.text(px(14), px(12), `背包 (${inv.length}/${100})`, {
      fontSize: fs(16), color: '#c0934a', fontFamily: TITLE_FONT, fontStyle: 'bold',
    }));

    // Sort button
    const sortBtn = this.add.text(pw - px(120), px(10), '[整理]', {
      fontSize: fs(14), color: '#5dade2', fontFamily: FONT,
    }).setInteractive({ useHandCursor: true });
    sortBtn.on('pointerdown', () => {
      this.zone.inventorySystem.sortInventory();
      this.refreshInventory();
    });
    this.inventoryPanel.add(sortBtn);

    // Destroy normal items button
    const destroyBtn = this.add.text(pw - px(68), px(10), '[销毁]', {
      fontSize: fs(14), color: '#e74c3c', fontFamily: FONT,
    }).setInteractive({ useHandCursor: true });
    destroyBtn.on('pointerdown', () => {
      this.zone.inventorySystem.destroyNormalItems();
      this.inventoryPage = 0;
      this.refreshInventory();
    });
    this.inventoryPanel.add(destroyBtn);

    // Close button
    const closeBtn = this.add.text(pw - px(16), px(10), 'X', {
      fontSize: fs(16), color: '#e74c3c', fontFamily: FONT,
    }).setOrigin(0.5, 0).setInteractive({ useHandCursor: true });
    closeBtn.on('pointerdown', () => this.toggleInventory());
    this.inventoryPanel.add(closeBtn);

    // Equipment slots — 5x2 grid
    const equipSlots = ['helmet', 'armor', 'gloves', 'boots', 'weapon', 'offhand', 'necklace', 'ring1', 'ring2', 'belt'];
    const slotNames = ['头盔', '铠甲', '手套', '鞋子', '武器', '副手', '项链', '戒指1', '戒指2', '腰带'];
    const eqSlotSize = px(36);
    const eqGap = px(6);
    const eqStartX = px(14);
    const eqStartY = px(36);
    equipSlots.forEach((slot, i) => {
      const sx = eqStartX + (i % 5) * (eqSlotSize + eqGap);
      const sy = eqStartY + Math.floor(i / 5) * (eqSlotSize + px(16));
      const eq = this.zone.inventorySystem.equipment[slot as keyof typeof this.zone.inventorySystem.equipment];
      const slotBg = this.add.rectangle(sx + eqSlotSize / 2, sy + eqSlotSize / 2, eqSlotSize, eqSlotSize, eq ? this.getQualityColorNum(eq.quality) : 0x222233)
        .setStrokeStyle(Math.round(1 * DPR), 0x444455).setInteractive({ useHandCursor: true });
      this.inventoryPanel!.add(slotBg);
      this.inventoryPanel!.add(this.add.text(sx + eqSlotSize / 2, sy + eqSlotSize + px(2), slotNames[i], {
        fontSize: fs(11), color: '#777788', fontFamily: FONT,
      }).setOrigin(0.5, 0));
      if (eq) {
        this.inventoryPanel!.add(this.add.text(sx + eqSlotSize / 2, sy + eqSlotSize / 2, eq.name.charAt(0), {
          fontSize: fs(14), color: '#fff', fontFamily: FONT, fontStyle: 'bold',
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
    const divY = eqStartY + 2 * (eqSlotSize + px(16)) + px(2);
    this.inventoryPanel.add(this.add.rectangle(pw / 2, divY, pw - px(20), Math.round(1 * DPR), 0x333344));

    // Inventory grid — 10 cols x 5 rows per page
    const gridStartY = divY + px(6);
    const cols = 10;
    const slotSize = px(36);
    const gap = px(4);
    const pageItems = inv.slice(this.inventoryPage * itemsPerPage, (this.inventoryPage + 1) * itemsPerPage);
    pageItems.forEach((item, i) => {
      const ix = px(14) + (i % cols) * (slotSize + gap);
      const iy = gridStartY + Math.floor(i / cols) * (slotSize + gap);
      const itemBg = this.add.rectangle(ix + slotSize / 2, iy + slotSize / 2, slotSize, slotSize, this.getQualityColorNum(item.quality))
        .setStrokeStyle(Math.round(1 * DPR), 0x555566).setInteractive({ useHandCursor: true });
      this.inventoryPanel!.add(itemBg);
      this.inventoryPanel!.add(this.add.text(ix + slotSize / 2, iy + slotSize / 2, item.name.charAt(0), {
        fontSize: fs(14), color: '#fff', fontFamily: FONT, fontStyle: 'bold',
      }).setOrigin(0.5));
      if (item.quantity > 1) {
        this.inventoryPanel!.add(this.add.text(ix + slotSize - px(2), iy + slotSize - px(2), `${item.quantity}`, {
          fontSize: fs(12), color: '#ffd700', fontFamily: FONT,
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
    const pageY = gridStartY + 5 * (slotSize + gap) + px(4);
    if (totalPages > 1) {
      if (this.inventoryPage > 0) {
        const prevBtn = this.add.text(pw / 2 - px(80), pageY, '< 上一页', {
          fontSize: fs(13), color: '#5dade2', fontFamily: FONT,
        }).setInteractive({ useHandCursor: true });
        prevBtn.on('pointerdown', () => { this.inventoryPage--; this.refreshInventory(); });
        this.inventoryPanel.add(prevBtn);
      }
      this.inventoryPanel.add(this.add.text(pw / 2, pageY, `第${this.inventoryPage + 1}/${totalPages}页`, {
        fontSize: fs(13), color: '#888', fontFamily: FONT,
      }).setOrigin(0.5, 0));
      if (this.inventoryPage < totalPages - 1) {
        const nextBtn = this.add.text(pw / 2 + px(40), pageY, '下一页 >', {
          fontSize: fs(13), color: '#5dade2', fontFamily: FONT,
        }).setInteractive({ useHandCursor: true });
        nextBtn.on('pointerdown', () => { this.inventoryPage++; this.refreshInventory(); });
        this.inventoryPanel.add(nextBtn);
      }
    }

    // Equipment stats at bottom
    const eqStats = this.zone.inventorySystem.getEquipmentStats();
    const statText = Object.entries(eqStats)
      .filter(([, v]) => v !== 0)
      .map(([k, v]) => {
        const disp = STAT_DISPLAY[k];
        const label = disp ? disp.label : k;
        const suffix = disp?.isPercent ? '%' : '';
        return `${label}+${v}${suffix}`;
      }).join('  ');
    this.inventoryPanel.add(this.add.text(px(14), ph - px(22), `装备加成: ${statText || '无'}`, {
      fontSize: fs(12), color: '#777788', fontFamily: FONT, wordWrap: { width: pw - px(28) },
    }));
  }

  // --- Shop Panel (Diablo-style split) ---
  private openShop(data: { npcId: string; shopItems: string[]; type: string }, keepPage = false): void {
    this.closeAllPanels();
    audioManager.playSFX('click');
    if (!keepPage) this.shopInventoryPage = 0;

    // Backdrop for outside-click dismiss
    this.dialogueBackdrop = this.add.rectangle(W / 2, H / 2, W, H, 0x000000, 0.3)
      .setInteractive().setDepth(3999);
    this.dialogueBackdrop.on('pointerdown', () => {
      this.hideItemTooltip();
      if (this.shopPanel) { this.shopPanel.destroy(); this.shopPanel = null; }
      if (this.dialogueBackdrop) { this.dialogueBackdrop.destroy(); this.dialogueBackdrop = null; }
    });

    const pw = px(700), ph = px(460), panelX = (W - pw) / 2, panelY = px(40);
    const dividerX = px(320);
    this.shopPanel = this.add.container(panelX, panelY).setDepth(4000);
    this.animatePanelOpen(this.shopPanel);
    this.shopPanel.add(this.add.rectangle(0, 0, pw, ph, 0x0f0f1e, 0.95).setOrigin(0, 0).setStrokeStyle(Math.round(2 * DPR), 0xc0934a));
    const title = data.type === 'blacksmith' ? '铁匠铺' : '商店';
    this.shopPanel.add(this.add.text(pw / 2, px(12), title, {
      fontSize: fs(18), color: '#c0934a', fontFamily: TITLE_FONT, fontStyle: 'bold',
    }).setOrigin(0.5, 0));
    const closeBtn = this.add.text(pw - px(16), px(10), 'X', {
      fontSize: fs(16), color: '#e74c3c', fontFamily: FONT,
    }).setOrigin(0.5, 0).setInteractive({ useHandCursor: true });
    closeBtn.on('pointerdown', () => {
      this.hideItemTooltip();
      if (this.shopPanel) { this.shopPanel.destroy(); this.shopPanel = null; }
      if (this.dialogueBackdrop) { this.dialogueBackdrop.destroy(); this.dialogueBackdrop = null; }
    });
    this.shopPanel.add(closeBtn);

    // Divider
    this.shopPanel.add(this.add.rectangle(dividerX, px(36), Math.round(1 * DPR), ph - px(56), 0x333344).setOrigin(0, 0));

    // --- LEFT: Merchant items ---
    this.shopPanel.add(this.add.text(dividerX / 2, px(38), '商品列表', {
      fontSize: fs(14), color: '#c0934a', fontFamily: FONT,
    }).setOrigin(0.5, 0));

    data.shopItems.forEach((itemId, i) => {
      const base = getItemBase(itemId);
      if (!base) return;
      const iy = px(58) + i * px(28);
      if (iy > ph - px(50)) return;
      const buyPrice = base.sellPrice * 3;
      const canAfford = this.player.gold >= buyPrice;
      this.shopPanel!.add(this.add.text(px(14), iy, base.name, {
        fontSize: fs(13), color: canAfford ? '#e0d8cc' : '#555', fontFamily: FONT,
      }));
      this.shopPanel!.add(this.add.text(dividerX - px(60), iy, `${buyPrice}G`, {
        fontSize: fs(13), color: canAfford ? '#f1c40f' : '#555', fontFamily: FONT,
      }).setOrigin(1, 0));
      if (canAfford) {
        const buyBtn = this.add.text(dividerX - px(14), iy, '[买]', {
          fontSize: fs(13), color: '#27ae60', fontFamily: FONT,
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
    this.shopPanel.add(this.add.text(px(14), ph - px(26), `金币: ${this.player.gold}G`, {
      fontSize: fs(13), color: '#f1c40f', fontFamily: FONT,
    }));

    // --- RIGHT: Player inventory for selling ---
    const rightX = dividerX + px(10);
    const rightW = pw - dividerX - px(20);
    this.shopPanel.add(this.add.text(dividerX + rightW / 2 + px(10), px(38), '你的背包', {
      fontSize: fs(14), color: '#c0934a', fontFamily: FONT,
    }).setOrigin(0.5, 0));

    const inv = this.zone.inventorySystem.inventory;
    const shopSlotSize = px(32);
    const shopCols = 8;
    const shopGap = px(4);
    const shopItemsPerPage = 40;
    const shopTotalPages = Math.max(1, Math.ceil(inv.length / shopItemsPerPage));
    if (this.shopInventoryPage >= shopTotalPages) this.shopInventoryPage = shopTotalPages - 1;
    const shopPageItems = inv.slice(this.shopInventoryPage * shopItemsPerPage, (this.shopInventoryPage + 1) * shopItemsPerPage);
    const shopGridY = px(58);

    shopPageItems.forEach((item, i) => {
      const ix = rightX + (i % shopCols) * (shopSlotSize + shopGap);
      const iy = shopGridY + Math.floor(i / shopCols) * (shopSlotSize + shopGap);
      const itemBg = this.add.rectangle(ix + shopSlotSize / 2, iy + shopSlotSize / 2, shopSlotSize, shopSlotSize, this.getQualityColorNum(item.quality))
        .setStrokeStyle(Math.round(1 * DPR), 0x555566).setInteractive({ useHandCursor: true });
      this.shopPanel!.add(itemBg);
      this.shopPanel!.add(this.add.text(ix + shopSlotSize / 2, iy + shopSlotSize / 2, item.name.charAt(0), {
        fontSize: fs(13), color: '#fff', fontFamily: FONT, fontStyle: 'bold',
      }).setOrigin(0.5));
      if (item.quantity > 1) {
        this.shopPanel!.add(this.add.text(ix + shopSlotSize - px(1), iy + shopSlotSize - px(1), `${item.quantity}`, {
          fontSize: fs(11), color: '#ffd700', fontFamily: FONT,
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
      const pageY = ph - px(50);
      if (this.shopInventoryPage > 0) {
        const prevBtn = this.add.text(rightX + rightW / 2 - px(60), pageY, '< 上页', {
          fontSize: fs(12), color: '#5dade2', fontFamily: FONT,
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
        fontSize: fs(12), color: '#888', fontFamily: FONT,
      }).setOrigin(0.5, 0));
      if (this.shopInventoryPage < shopTotalPages - 1) {
        const nextBtn = this.add.text(rightX + rightW / 2 + px(30), pageY, '下页 >', {
          fontSize: fs(12), color: '#5dade2', fontFamily: FONT,
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
    this.shopPanel.add(this.add.text(rightX, ph - px(26), `金币: ${this.player.gold}G`, {
      fontSize: fs(13), color: '#f1c40f', fontFamily: FONT,
    }));
  }

  private showSellConfirm(item: ItemInstance, shopData: { npcId: string; shopItems: string[]; type: string }): void {
    this.hideContextPopup();
    const base = getItemBase(item.baseId);
    const sellPrice = base ? base.sellPrice * item.quantity : 1;
    const popW = px(180), popH = px(60);
    const popX = (W - popW) / 2, popY = (H - popH) / 2;
    this.contextPopup = this.add.container(popX, popY).setDepth(5002);
    this.contextPopup.add(this.add.rectangle(0, 0, popW, popH, 0x0a0a18, 0.95).setOrigin(0, 0).setStrokeStyle(Math.round(1 * DPR), 0xc0934a));
    this.contextPopup.add(this.add.text(popW / 2, px(8), `卖出 ${item.name} (${sellPrice}G)?`, {
      fontSize: fs(12), color: '#e0d8cc', fontFamily: FONT, wordWrap: { width: popW - px(16) },
    }).setOrigin(0.5, 0));
    const yesBtn = this.add.text(popW / 2 - px(30), px(38), '[确定]', {
      fontSize: fs(13), color: '#27ae60', fontFamily: FONT,
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
    const noBtn = this.add.text(popW / 2 + px(30), px(38), '[取消]', {
      fontSize: fs(13), color: '#888', fontFamily: FONT,
    }).setInteractive({ useHandCursor: true });
    noBtn.on('pointerdown', () => this.hideContextPopup());
    this.contextPopup.add(noBtn);
  }

  // --- World Map ---
  private toggleMap(): void {
    if (this.mapPanel) { this.mapPanel.destroy(); this.mapPanel = null; return; }
    this.closeAllPanels();
    const pw = px(480), ph = px(220), panelX = (W - pw) / 2, panelY = px(80);
    this.mapPanel = this.add.container(panelX, panelY).setDepth(4000);
    this.animatePanelOpen(this.mapPanel);
    this.mapPanel.add(this.add.rectangle(0, 0, pw, ph, 0x0f0f1e, 0.95).setOrigin(0, 0).setStrokeStyle(Math.round(2 * DPR), 0x27ae60));
    this.mapPanel.add(this.add.text(pw / 2, px(10), '渊火', {
      fontSize: fs(18), color: '#27ae60', fontFamily: TITLE_FONT, fontStyle: 'bold',
    }).setOrigin(0.5, 0));
    const closeBtn = this.add.text(pw - px(16), px(8), 'X', {
      fontSize: fs(18), color: '#e74c3c', fontFamily: FONT,
    }).setOrigin(0.5, 0).setInteractive({ useHandCursor: true });
    closeBtn.on('pointerdown', () => this.toggleMap());
    this.mapPanel.add(closeBtn);

    MapOrder.forEach((mapId, i) => {
      const map = AllMaps[mapId];
      const x = px(36) + i * px(88), y = px(70);
      const isCurrent = (this.zone as any).currentMapId === mapId;
      const color = isCurrent ? 0x27ae60 : 0x1a1a2e;
      this.mapPanel!.add(this.add.rectangle(x, y, px(72), px(44), color)
        .setStrokeStyle(isCurrent ? Math.round(2 * DPR) : Math.round(1 * DPR), isCurrent ? 0x27ae60 : 0x444455));
      this.mapPanel!.add(this.add.text(x, y, map.name.substring(0, 4), {
        fontSize: fs(13), color: '#e0d8cc', fontFamily: FONT,
      }).setOrigin(0.5));
      this.mapPanel!.add(this.add.text(x, y + px(28), `Lv.${map.levelRange[0]}-${map.levelRange[1]}`, {
        fontSize: fs(12), color: '#888', fontFamily: FONT,
      }).setOrigin(0.5));
      if (i < MapOrder.length - 1) {
        this.mapPanel!.add(this.add.text(x + px(42), y, '\u2192', {
          fontSize: fs(16), color: '#444455', fontFamily: FONT,
        }).setOrigin(0.5));
      }
    });
    this.mapPanel.add(this.add.text(pw / 2, ph - px(20), '按 M 关闭', {
      fontSize: fs(12), color: '#555', fontFamily: FONT,
    }).setOrigin(0.5));
  }

  // --- Skill Tree Panel (K) ---
  private skillTooltip: Phaser.GameObjects.Container | null = null;

  private toggleSkillTree(): void {
    if (this.skillPanel) { this.skillPanel.destroy(); this.skillPanel = null; this.skillTooltip?.destroy(); this.skillTooltip = null; return; }
    this.closeAllPanels();

    const TREE_NAMES: Record<string, string> = {
      combat_master: '进攻大师', guardian: '守护者', berserker: '狂战士',
      fire: '烈焰', frost: '寒冰', arcane: '奥术',
      assassination: '暗杀', archery: '射术', traps: '陷阱',
    };
    const TREE_COLORS: Record<string, number> = {
      combat_master: 0xd4a017, guardian: 0xf1c40f, berserker: 0xcc3333,
      fire: 0xe74c3c, frost: 0x5dade2, arcane: 0x8e44ad,
      assassination: 0x27ae60, archery: 0xcc8844, traps: 0xff6600,
    };
    const DMG_COLORS: Record<string, number> = {
      physical: 0xcccccc, fire: 0xff6633, ice: 0x66ccff,
      lightning: 0x5dade2, poison: 0x33cc33, arcane: 0xbb77ff,
    };
    const DMG_NAMES: Record<string, string> = {
      physical: '物理', fire: '火焰', ice: '冰霜',
      lightning: '闪电', poison: '毒素', arcane: '奥术',
    };

    const pw = px(660), ph = px(500);
    const panelX = (W - pw) / 2, panelY = px(5);
    this.skillPanel = this.add.container(panelX, panelY).setDepth(4000);
    this.animatePanelOpen(this.skillPanel);

    // Background with double border
    const bg = this.add.graphics().setDepth(0);
    bg.fillStyle(0x0d0d1a, 0.97);
    bg.fillRoundedRect(0, 0, pw, ph, px(8));
    bg.lineStyle(Math.round(2 * DPR), 0x8e44ad, 0.6);
    bg.strokeRoundedRect(0, 0, pw, ph, px(8));
    bg.lineStyle(Math.round(1 * DPR), 0x8e44ad, 0.15);
    bg.strokeRoundedRect(px(4), px(4), pw - px(8), ph - px(8), px(6));
    this.skillPanel.add(bg);

    // Header bar with gradient
    const headerH = px(50);
    const headerBg = this.add.graphics();
    headerBg.fillStyle(0x14142e, 1);
    headerBg.fillRect(px(4), px(4), pw - px(8), headerH);
    headerBg.fillGradientStyle(0x8e44ad, 0x8e44ad, 0x14142e, 0x14142e, 0.15, 0.15, 0, 0);
    headerBg.fillRect(px(4), px(4), pw - px(8), headerH);
    this.skillPanel.add(headerBg);

    this.skillPanel.add(this.add.text(pw / 2, px(10), '技 能 树', {
      fontSize: fs(20), color: '#d4b8e8', fontFamily: TITLE_FONT, fontStyle: 'bold',
    }).setOrigin(0.5, 0));
    const spColor = this.player.freeSkillPoints > 0 ? '#f1c40f' : '#555566';
    this.skillPanel.add(this.add.text(pw / 2, px(33), `${this.player.classData.name}  ·  剩余技能点: ${this.player.freeSkillPoints}`, {
      fontSize: fs(13), color: spColor, fontFamily: FONT,
    }).setOrigin(0.5, 0));

    // Close button
    const closeBg = this.add.circle(pw - px(18), px(18), px(12), 0x331111, 0.6).setInteractive({ useHandCursor: true });
    const closeX = this.add.text(pw - px(18), px(18), '\u2715', {
      fontSize: fs(14), color: '#e74c3c', fontFamily: FONT, fontStyle: 'bold',
    }).setOrigin(0.5);
    closeBg.on('pointerdown', () => this.toggleSkillTree());
    closeBg.on('pointerover', () => closeBg.setFillStyle(0x551111, 0.9));
    closeBg.on('pointerout', () => closeBg.setFillStyle(0x331111, 0.6));
    this.skillPanel.add(closeBg);
    this.skillPanel.add(closeX);

    // Gather trees
    const trees = new Map<string, typeof this.player.classData.skills>();
    for (const skill of this.player.classData.skills) {
      if (!trees.has(skill.tree)) trees.set(skill.tree, []);
      trees.get(skill.tree)!.push(skill);
    }

    const treeCount = trees.size;
    const margin = px(10);
    const gapBetween = px(8);
    const treeW = Math.floor((pw - margin * 2 - gapBetween * (treeCount - 1)) / treeCount);
    const contentTop = headerH + px(12);
    const contentH = ph - contentTop - px(20);

    // Card dimensions
    const cardW = treeW - px(12);
    const cardH = px(68);
    const iconSize = px(38);

    // Tooltip builder
    const showTooltip = (skill: typeof this.player.classData.skills[0], cardWorldX: number, cardWorldY: number) => {
      this.skillTooltip?.destroy();
      const level = this.player.getSkillLevel(skill.id);
      const scaledDmg = getSkillDamageMultiplier(skill, level);
      const scaledMana = getSkillManaCost(skill, level);
      const scaledCD = getSkillCooldown(skill, level);
      const scaledAoe = getSkillAoeRadius(skill, level);
      const dmgName = DMG_NAMES[skill.damageType] ?? skill.damageType;

      const lines: string[] = [];
      lines.push(skill.description);
      lines.push('');
      if (skill.damageMultiplier > 0) lines.push(`伤害: ${Math.round(scaledDmg * 100)}% ${dmgName}`);
      lines.push(`消耗: ${scaledMana} MP`);
      lines.push(`冷却: ${(scaledCD / 1000).toFixed(1)}s`);
      lines.push(`范围: ${skill.range}格`);
      if (skill.aoe && scaledAoe > 0) lines.push(`AOE半径: ${scaledAoe.toFixed(1)}格`);
      if (skill.critBonus) lines.push(`暴击加成: +${skill.critBonus}%`);
      if (skill.stunDuration) lines.push(`眩晕: ${(skill.stunDuration / 1000).toFixed(1)}s`);
      if (skill.buff) {
        const buffVal = getSkillBuffValue(skill, level);
        const buffDur = getSkillBuffDuration(skill, level);
        lines.push(`增益: ${skill.buff.stat} +${Math.round(buffVal * 100)}% (${(buffDur / 1000).toFixed(0)}s)`);
      }

      // Synergy info
      if (skill.synergies && skill.synergies.length > 0) {
        lines.push('');
        lines.push('─ 协同增益 ─');
        for (const syn of skill.synergies) {
          const synSkill = this.player.classData.skills.find(s => s.id === syn.skillId);
          const synLv = this.player.getSkillLevel(syn.skillId);
          if (synSkill) {
            const bonus = Math.round(syn.damagePerLevel * synLv * 100);
            lines.push(`${synSkill.name}: +${syn.damagePerLevel * 100}%/级 (当前+${bonus}%)`);
          }
        }
      }

      // Next level preview
      if (level < skill.maxLevel) {
        lines.push('');
        lines.push(`─ 下一级 (Lv${level + 1}) ─`);
        const nextDmg = getSkillDamageMultiplier(skill, level + 1);
        const nextMana = getSkillManaCost(skill, level + 1);
        const nextCD = getSkillCooldown(skill, level + 1);
        if (skill.damageMultiplier > 0) {
          const delta = Math.round((nextDmg - scaledDmg) * 100);
          lines.push(`伤害: ${Math.round(nextDmg * 100)}% (+${delta}%)`);
        }
        if (nextMana !== scaledMana) lines.push(`消耗: ${nextMana} MP`);
        if (nextCD !== scaledCD) lines.push(`冷却: ${(nextCD / 1000).toFixed(1)}s`);
      }

      const tipW = px(280);
      const tipPad = px(12);
      const wrapW = tipW - tipPad * 2;
      const tipText = lines.join('\n');

      // Measure header first
      const tipHeader = this.add.text(0, 0, `${skill.name} (${skill.nameEn})`, {
        fontSize: fs(12), color: '#f0e8d0', fontFamily: FONT, fontStyle: 'bold',
        wordWrap: { width: wrapW, useAdvancedWrap: true },
      });
      const headerH = tipHeader.height;
      const headerBottom = tipPad + headerH + px(6);

      // Body text positioned below header
      const textObj = this.add.text(tipPad, headerBottom, tipText, {
        fontSize: fs(11), color: '#ddd8cc', fontFamily: FONT, lineSpacing: px(2),
        wordWrap: { width: wrapW, useAdvancedWrap: true },
      });

      const finalH = textObj.y + textObj.height + tipPad;

      // Position tooltip to the right of card, or left if not enough space
      let tipX = cardWorldX + cardW + px(8);
      if (tipX + tipW > W) tipX = cardWorldX - tipW - px(8);
      let tipY = cardWorldY;
      if (tipY + finalH > H) tipY = H - finalH - px(4);
      if (tipY < px(4)) tipY = px(4);

      this.skillTooltip = this.add.container(tipX, tipY).setDepth(5000);
      const tipBg = this.add.graphics();
      tipBg.fillStyle(0x0a0a18, 0.97);
      tipBg.fillRoundedRect(0, 0, tipW, finalH, px(4));
      tipBg.lineStyle(Math.round(2 * DPR), 0x8e44ad, 0.7);
      tipBg.strokeRoundedRect(0, 0, tipW, finalH, px(4));
      this.skillTooltip.add(tipBg);
      tipHeader.setPosition(tipW / 2, tipPad).setOrigin(0.5, 0);
      this.skillTooltip.add(tipHeader);
      this.skillTooltip.add(textObj);
    };

    const hideTooltip = () => {
      this.skillTooltip?.destroy();
      this.skillTooltip = null;
    };

    let treeIdx = 0;
    for (const [treeName, treeSkills] of trees) {
      const treeColor = TREE_COLORS[treeName] ?? 0x888888;
      const treeColorHex = '#' + treeColor.toString(16).padStart(6, '0');
      const tx = margin + treeIdx * (treeW + gapBetween);
      const tcx = tx + treeW / 2;

      // Tree column background
      const colBg = this.add.graphics();
      colBg.fillStyle(0x0e0e20, 0.6);
      colBg.fillRoundedRect(tx, contentTop, treeW, contentH, px(5));
      colBg.lineStyle(Math.round(1 * DPR), treeColor, 0.2);
      colBg.strokeRoundedRect(tx, contentTop, treeW, contentH, px(5));
      this.skillPanel.add(colBg);

      // Tree header with accent
      const treeHeaderY = contentTop + px(6);
      const displayName = TREE_NAMES[treeName] ?? treeName;
      this.skillPanel.add(this.add.text(tcx, treeHeaderY, `─ ${displayName} ─`, {
        fontSize: fs(13), color: treeColorHex, fontFamily: FONT, fontStyle: 'bold',
      }).setOrigin(0.5, 0));

      // Sort skills by tier
      const sortedSkills = [...treeSkills].sort((a, b) => a.tier - b.tier);
      const skillStartY = treeHeaderY + px(26);
      const cardGap = px(10);

      sortedSkills.forEach((skill, si) => {
        const level = this.player.getSkillLevel(skill.id);
        const canLevel = this.player.freeSkillPoints > 0 && level < skill.maxLevel;
        const isLearned = level > 0;
        const isMaxed = level >= skill.maxLevel;
        const cardX = tcx - cardW / 2;
        const cardY = skillStartY + si * (cardH + cardGap);
        const dmgColor = DMG_COLORS[skill.damageType] ?? 0xcccccc;
        const dmgColorHex = '#' + dmgColor.toString(16).padStart(6, '0');

        // Connection line to next card
        if (si < sortedSkills.length - 1) {
          const lineGfx = this.add.graphics();
          const lineAlpha = isLearned ? 0.5 : 0.12;
          lineGfx.lineStyle(Math.round(2 * DPR), treeColor, lineAlpha);
          lineGfx.beginPath();
          lineGfx.moveTo(tcx, cardY + cardH);
          lineGfx.lineTo(tcx, cardY + cardH + cardGap);
          lineGfx.strokePath();
          // Arrow
          const arrowY = cardY + cardH + cardGap - px(1);
          lineGfx.fillStyle(treeColor, lineAlpha);
          lineGfx.fillTriangle(tcx, arrowY + px(2), tcx - px(4), arrowY - px(4), tcx + px(4), arrowY - px(4));
          this.skillPanel!.add(lineGfx);
        }

        // Card background - bordered rectangle
        const cardGfx = this.add.graphics();
        const cardBgColor = isLearned ? 0x161630 : 0x0c0c18;
        const borderColor = isMaxed ? 0xf1c40f : (canLevel ? 0x44dd44 : (isLearned ? treeColor : 0x2a2a3e));
        const borderAlpha = isLearned ? 0.9 : 0.4;
        cardGfx.fillStyle(cardBgColor, 0.95);
        cardGfx.fillRoundedRect(cardX, cardY, cardW, cardH, px(4));
        cardGfx.lineStyle(Math.round(isMaxed ? 2 * DPR : 1.5 * DPR), borderColor, borderAlpha);
        cardGfx.strokeRoundedRect(cardX, cardY, cardW, cardH, px(4));
        // Maxed golden glow
        if (isMaxed) {
          cardGfx.lineStyle(Math.round(1 * DPR), 0xf1c40f, 0.15);
          cardGfx.strokeRoundedRect(cardX - px(2), cardY - px(2), cardW + px(4), cardH + px(4), px(5));
        }
        this.skillPanel!.add(cardGfx);

        // Icon area (left side of card) — square with dark background
        const iconX = cardX + px(6);
        const iconY = cardY + (cardH - iconSize) / 2;
        const iconGfx = this.add.graphics();
        iconGfx.fillStyle(0x080810, 0.9);
        iconGfx.fillRoundedRect(iconX, iconY, iconSize, iconSize, px(3));
        iconGfx.lineStyle(Math.round(1 * DPR), dmgColor, isLearned ? 0.6 : 0.2);
        iconGfx.strokeRoundedRect(iconX, iconY, iconSize, iconSize, px(3));
        this.skillPanel!.add(iconGfx);

        // Skill icon texture
        const iconKey = `skill_icon_${skill.id}`;
        const iconCx = iconX + iconSize / 2;
        const iconCy = iconY + iconSize / 2;
        if (this.textures.exists(iconKey)) {
          const iconImg = this.add.image(iconCx, iconCy, iconKey)
            .setDisplaySize(iconSize - px(4), iconSize - px(4))
            .setAlpha(isLearned ? 1 : 0.35);
          this.skillPanel!.add(iconImg);
        } else {
          // Fallback: colored dot + tier
          const dotR = px(6);
          iconGfx.fillStyle(dmgColor, isLearned ? 0.6 : 0.15);
          iconGfx.fillCircle(iconCx, iconCy, dotR);
          this.skillPanel!.add(this.add.text(iconCx, iconCy, `T${skill.tier}`, {
            fontSize: fs(11), color: isLearned ? '#ccc' : '#444', fontFamily: FONT, fontStyle: 'bold',
          }).setOrigin(0.5));
        }

        // Text area (right side of card)
        const textX = iconX + iconSize + px(8);
        const textAreaW = cardW - iconSize - px(22);

        // Skill name
        const nameColor = isMaxed ? '#f1c40f' : (isLearned ? '#e8e0d0' : '#777788');
        this.skillPanel!.add(this.add.text(textX, cardY + px(8), skill.name, {
          fontSize: fs(13), color: nameColor, fontFamily: FONT, fontStyle: 'bold',
        }));

        // Level bar — visual Lv / maxLv
        const lvBarX = textX;
        const lvBarY = cardY + px(26);
        const lvBarW = Math.min(textAreaW - px(40), px(80));
        const lvBarH = px(6);
        const lvBarBg = this.add.graphics();
        lvBarBg.fillStyle(0x1a1a2e, 1);
        lvBarBg.fillRoundedRect(lvBarX, lvBarY, lvBarW, lvBarH, px(2));
        const fillW = skill.maxLevel > 0 ? Math.round((level / skill.maxLevel) * lvBarW) : 0;
        if (fillW > 0) {
          lvBarBg.fillStyle(isMaxed ? 0xf1c40f : treeColor, 0.8);
          lvBarBg.fillRoundedRect(lvBarX, lvBarY, fillW, lvBarH, px(2));
        }
        lvBarBg.lineStyle(Math.round(1 * DPR), treeColor, 0.3);
        lvBarBg.strokeRoundedRect(lvBarX, lvBarY, lvBarW, lvBarH, px(2));
        this.skillPanel!.add(lvBarBg);

        // Level text next to bar
        const lvColor = isMaxed ? '#f1c40f' : (isLearned ? '#aaaacc' : '#555566');
        this.skillPanel!.add(this.add.text(lvBarX + lvBarW + px(4), lvBarY - px(1), `${level}/${skill.maxLevel}`, {
          fontSize: fs(10), color: lvColor, fontFamily: FONT,
        }));

        // Stats row: damage%, mana, cooldown
        const scaledDmg = getSkillDamageMultiplier(skill, level);
        const scaledMana = getSkillManaCost(skill, level);
        const scaledCD = getSkillCooldown(skill, level);
        const statsY = cardY + px(38);
        let statsStr = '';
        if (skill.damageMultiplier > 0) statsStr += `${Math.round(scaledDmg * 100)}%`;
        statsStr += `  MP${scaledMana}  CD${(scaledCD / 1000).toFixed(1)}s`;
        this.skillPanel!.add(this.add.text(textX, statsY, statsStr, {
          fontSize: fs(10), color: '#666680', fontFamily: FONT,
        }));

        // Damage type label
        const dmgName = DMG_NAMES[skill.damageType] ?? '';
        if (dmgName) {
          this.skillPanel!.add(this.add.text(textX, statsY + px(13), dmgName, {
            fontSize: fs(9), color: dmgColorHex, fontFamily: FONT,
          }));
        }

        // Synergy indicator dot
        if (skill.synergies && skill.synergies.length > 0 && isLearned) {
          let hasActiveSyn = false;
          for (const syn of skill.synergies) {
            if (this.player.getSkillLevel(syn.skillId) > 0) { hasActiveSyn = true; break; }
          }
          if (hasActiveSyn) {
            const synDot = this.add.graphics();
            synDot.fillStyle(0x7766cc, 0.8);
            synDot.fillCircle(cardX + cardW - px(10), cardY + px(10), px(4));
            this.skillPanel!.add(synDot);
          }
        }

        // Hover area over entire card for tooltip
        const cardHit = this.add.rectangle(cardX + cardW / 2, cardY + cardH / 2, cardW, cardH, 0x000000, 0)
          .setInteractive({ useHandCursor: false });
        cardHit.on('pointerover', () => {
          cardGfx.clear();
          cardGfx.fillStyle(isLearned ? 0x1c1c3e : 0x101020, 0.98);
          cardGfx.fillRoundedRect(cardX, cardY, cardW, cardH, px(4));
          cardGfx.lineStyle(Math.round(2 * DPR), borderColor, 1);
          cardGfx.strokeRoundedRect(cardX, cardY, cardW, cardH, px(4));
          showTooltip(skill, panelX + cardX, panelY + cardY);
        });
        cardHit.on('pointerout', () => {
          cardGfx.clear();
          cardGfx.fillStyle(cardBgColor, 0.95);
          cardGfx.fillRoundedRect(cardX, cardY, cardW, cardH, px(4));
          cardGfx.lineStyle(Math.round(isMaxed ? 2 * DPR : 1.5 * DPR), borderColor, borderAlpha);
          cardGfx.strokeRoundedRect(cardX, cardY, cardW, cardH, px(4));
          if (isMaxed) {
            cardGfx.lineStyle(Math.round(1 * DPR), 0xf1c40f, 0.15);
            cardGfx.strokeRoundedRect(cardX - px(2), cardY - px(2), cardW + px(4), cardH + px(4), px(5));
          }
          hideTooltip();
        });
        this.skillPanel!.add(cardHit);

        // + Button — bottom right of card (added AFTER cardHit so it sits on top and receives clicks)
        if (canLevel) {
          const btnSize = px(20);
          const btnX = cardX + cardW - btnSize - px(5);
          const btnY = cardY + cardH - btnSize - px(5);
          const btnBg = this.add.graphics();
          btnBg.fillStyle(0x1a3a1a, 0.9);
          btnBg.fillRoundedRect(btnX, btnY, btnSize, btnSize, px(3));
          btnBg.lineStyle(Math.round(1 * DPR), 0x27ae60, 0.8);
          btnBg.strokeRoundedRect(btnX, btnY, btnSize, btnSize, px(3));
          this.skillPanel!.add(btnBg);
          const btnText = this.add.text(btnX + btnSize / 2, btnY + btnSize / 2, '+', {
            fontSize: fs(14), color: '#27ae60', fontFamily: FONT, fontStyle: 'bold',
          }).setOrigin(0.5);
          this.skillPanel!.add(btnText);

          const hitArea = this.add.rectangle(btnX + btnSize / 2, btnY + btnSize / 2, btnSize, btnSize, 0x000000, 0)
            .setInteractive({ useHandCursor: true });
          hitArea.on('pointerover', () => {
            btnBg.clear();
            btnBg.fillStyle(0x225522, 1);
            btnBg.fillRoundedRect(btnX, btnY, btnSize, btnSize, px(3));
            btnBg.lineStyle(Math.round(2 * DPR), 0x44dd44, 1);
            btnBg.strokeRoundedRect(btnX, btnY, btnSize, btnSize, px(3));
            btnText.setColor('#44dd44');
          });
          hitArea.on('pointerout', () => {
            btnBg.clear();
            btnBg.fillStyle(0x1a3a1a, 0.9);
            btnBg.fillRoundedRect(btnX, btnY, btnSize, btnSize, px(3));
            btnBg.lineStyle(Math.round(1 * DPR), 0x27ae60, 0.8);
            btnBg.strokeRoundedRect(btnX, btnY, btnSize, btnSize, px(3));
            btnText.setColor('#27ae60');
          });
          hitArea.on('pointerdown', () => {
            if (this.player.freeSkillPoints > 0) {
              this.player.freeSkillPoints--;
              this.player.skillLevels.set(skill.id, level + 1);
              this.toggleSkillTree(); this.toggleSkillTree();
            }
          });
          this.skillPanel!.add(hitArea);
        }
      });

      treeIdx++;
    }

    // Footer
    this.skillPanel.add(this.add.text(pw / 2, ph - px(12), '按 K 关闭  ·  悬停查看详情', {
      fontSize: fs(10), color: '#3a3a4a', fontFamily: FONT,
    }).setOrigin(0.5));
  }

  // --- Character Stats Panel (C) ---
  private toggleCharacter(): void {
    if (this.charPanel) { this.charPanel.destroy(); this.charPanel = null; return; }
    this.closeAllPanels();
    const pw = px(320), ph = px(380), panelX = (W - pw) / 2, panelY = px(30);
    this.charPanel = this.add.container(panelX, panelY).setDepth(4000);
    this.animatePanelOpen(this.charPanel);
    this.charPanel.add(this.add.rectangle(0, 0, pw, ph, 0x0f0f1e, 0.95).setOrigin(0, 0).setStrokeStyle(Math.round(2 * DPR), 0x2471a3));
    this.charPanel.add(this.add.text(pw / 2, px(10), `角色属性 - ${this.player.classData.name}`, {
      fontSize: fs(17), color: '#5dade2', fontFamily: TITLE_FONT, fontStyle: 'bold',
    }).setOrigin(0.5, 0));
    this.charPanel.add(this.add.text(pw / 2, px(28), `Lv.${this.player.level}  剩余属性点: ${this.player.freeStatPoints}`, {
      fontSize: fs(13), color: '#f1c40f', fontFamily: FONT,
    }).setOrigin(0.5, 0));
    const closeBtn = this.add.text(pw - px(14), px(8), 'X', {
      fontSize: fs(18), color: '#e74c3c', fontFamily: FONT,
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
      const sy = px(50) + i * px(40);
      const val = this.player.stats[key];
      this.charPanel!.add(this.add.text(px(14), sy, label, {
        fontSize: fs(14), color: '#e0d8cc', fontFamily: FONT,
      }));
      this.charPanel!.add(this.add.text(px(140), sy, `${val}`, {
        fontSize: fs(14), color: '#fff', fontFamily: FONT, fontStyle: 'bold',
      }));
      this.charPanel!.add(this.add.text(px(14), sy + px(16), desc, {
        fontSize: fs(11), color: '#666', fontFamily: FONT,
      }));
      if (this.player.freeStatPoints > 0) {
        const plusBtn = this.add.text(px(170), sy, '[+]', {
          fontSize: fs(14), color: '#27ae60', fontFamily: FONT,
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

    const dy = px(50) + stats.length * px(40) + px(8);
    const eqStats = this.zone.inventorySystem.getEquipmentStats();
    const critPct = (this.player.stats.dex * 0.2 + this.player.stats.lck * 0.5 + (eqStats['critRate'] ?? 0)).toFixed(1);
    const derived = [
      `HP: ${Math.ceil(this.player.hp)}/${this.player.maxHp}`,
      `MP: ${Math.ceil(this.player.mana)}/${this.player.maxMana}`,
      `攻击: ${Math.floor(this.player.baseDamage)}${eqStats['damage'] ? ` (+${eqStats['damage']})` : ''}${eqStats['damagePercent'] ? ` +${eqStats['damagePercent']}%` : ''}`,
      `防御: ${Math.floor(this.player.defense)}${eqStats['defense'] ? ` (+${eqStats['defense']})` : ''}`,
      `暴击率: ${critPct}%  暴击伤害: ${150 + (eqStats['critDamage'] ?? 0)}%`,
      `金币: ${this.player.gold}G`,
    ];
    derived.forEach((line, i) => {
      this.charPanel!.add(this.add.text(px(14), dy + i * px(16), line, {
        fontSize: fs(12), color: '#888', fontFamily: FONT,
      }));
    });
  }

  // --- Homestead Panel (H) ---
  private toggleHomestead(): void {
    if (this.homesteadPanel) { this.homesteadPanel.destroy(); this.homesteadPanel = null; return; }
    this.closeAllPanels();
    const pw = px(420), ph = px(400), panelX = (W - pw) / 2, panelY = px(20);
    this.homesteadPanel = this.add.container(panelX, panelY).setDepth(4000);
    this.animatePanelOpen(this.homesteadPanel);
    this.homesteadPanel.add(this.add.rectangle(0, 0, pw, ph, 0x0f0f1e, 0.95).setOrigin(0, 0).setStrokeStyle(Math.round(2 * DPR), 0xc0934a));
    this.homesteadPanel.add(this.add.text(pw / 2, px(10), '家园', {
      fontSize: fs(18), color: '#c0934a', fontFamily: TITLE_FONT, fontStyle: 'bold',
    }).setOrigin(0.5, 0));
    const closeBtn = this.add.text(pw - px(14), px(8), 'X', {
      fontSize: fs(18), color: '#e74c3c', fontFamily: FONT,
    }).setInteractive({ useHandCursor: true });
    closeBtn.on('pointerdown', () => this.toggleHomestead());
    this.homesteadPanel.add(closeBtn);

    const hs = this.zone.homesteadSystem;
    const buildings = hs.getAllBuildings();

    buildings.forEach((b, i) => {
      const sy = px(36) + i * px(46);
      const lv = hs.getBuildingLevel(b.id);
      const maxed = lv >= b.maxLevel;
      const cost = maxed ? 0 : b.costPerLevel[lv]?.gold ?? 0;
      const canUpgrade = !maxed && this.player.gold >= cost;

      this.homesteadPanel!.add(this.add.text(px(14), sy, `${b.name} Lv.${lv}/${b.maxLevel}`, {
        fontSize: fs(13), color: '#e0d8cc', fontFamily: FONT,
      }));
      this.homesteadPanel!.add(this.add.text(px(14), sy + px(16), b.description, {
        fontSize: fs(12), color: '#666', fontFamily: FONT,
      }));
      if (!maxed) {
        const upBtn = this.add.text(pw - px(14), sy + px(6), `升级 ${cost}G`, {
          fontSize: fs(12), color: canUpgrade ? '#27ae60' : '#555', fontFamily: FONT,
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
        this.homesteadPanel!.add(this.add.text(pw - px(14), sy + px(6), '已满级', {
          fontSize: fs(12), color: '#c0934a', fontFamily: FONT,
        }).setOrigin(1, 0));
      }
    });

    const petY = px(36) + buildings.length * px(46) + px(10);
    this.homesteadPanel.add(this.add.text(px(14), petY, '── 宠物 ──', {
      fontSize: fs(13), color: '#c0934a', fontFamily: FONT,
    }));
    const pets = hs.pets;
    if (pets.length === 0) {
      this.homesteadPanel.add(this.add.text(px(14), petY + px(18), '暂无宠物 (击杀Boss有机会获得)', {
        fontSize: fs(12), color: '#555', fontFamily: FONT,
      }));
    }
    pets.forEach((p, i) => {
      const pd = hs.getAllPets().find(d => d.id === p.petId);
      const isActive = hs.activePet === p.petId;
      this.homesteadPanel!.add(this.add.text(px(14), petY + px(18) + i * px(18), `${pd?.name ?? p.petId} Lv.${p.level} ${isActive ? '[激活]' : ''}`, {
        fontSize: fs(12), color: isActive ? '#27ae60' : '#888', fontFamily: FONT,
      }));
    });
  }

  // --- Minimap ---
  private createMinimap(): void {
    const size = px(100), padding = px(10);
    const x = W - size - padding, y = padding + px(60);
    this.add.rectangle(x + size / 2, y + size / 2, size + px(4), size + px(4), 0x000000, 0.5)
      .setStrokeStyle(Math.round(1 * DPR), 0x333344).setDepth(2999);
    this.minimap = this.add.graphics().setDepth(3000);
    this.minimap.setPosition(x, y);
  }

  private updateMinimap(): void {
    if (!this.minimap || !this.zone) return;
    this.minimap.clear();
    const mapData = AllMaps[(this.zone as any).currentMapId];
    if (!mapData) return;
    const size = px(100);
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
    this.minimap.fillCircle(this.player.tileCol * sx, this.player.tileRow * sy, 3 * DPR);
    // Exits
    for (const exit of mapData.exits) {
      this.minimap.fillStyle(0x00e676);
      this.minimap.fillRect(exit.col * sx - 1.5 * DPR, exit.row * sy - 1.5 * DPR, 4 * DPR, 4 * DPR);
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
            this.minimap.fillCircle(camp.col * sx, camp.row * sy, 2.5 * DPR);
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
          this.minimap.fillCircle(m.tileCol * sx, m.tileRow * sy, m.isAggro() ? 2 * DPR : 1.5 * DPR);
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
          this.minimap.lineStyle(1 * DPR, qColor, 0.6);
          this.minimap.strokeCircle(qa.col * sx, qa.row * sy, qa.radius * sx);
        }
        // Explore objective markers
        for (let i = 0; i < quest.objectives.length; i++) {
          const obj = quest.objectives[i];
          if (obj.type === 'explore' && obj.location && progress.objectives[i].current < obj.required) {
            this.minimap.fillStyle(0xf39c12, 0.5);
            this.minimap.fillRect(obj.location.col * sx - 1.5 * DPR, obj.location.row * sy - 1.5 * DPR, 3 * DPR, 3 * DPR);
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
    this.dialogueBackdrop = this.add.rectangle(W / 2, H / 2, W, H, 0x000000, 0.3)
      .setInteractive().setDepth(3999);
    this.dialogueBackdrop.on('pointerdown', () => this.closeDialogue());

    const pw = px(360), ph = px(60) + data.actions.length * px(32) + px(30);
    const panelX = (W - pw) / 2, panelY = H / 2 - ph / 2;
    this.dialoguePanel = this.add.container(panelX, panelY).setDepth(4000);
    this.animatePanelOpen(this.dialoguePanel);
    const bg = this.add.rectangle(0, 0, pw, ph, 0x0f0f1e, 0.95).setOrigin(0, 0).setStrokeStyle(Math.round(2 * DPR), 0xc0934a);
    this.dialoguePanel.add(bg);

    // NPC name
    this.dialoguePanel.add(this.add.text(pw / 2, px(10), data.npcName, {
      fontSize: fs(16), color: '#c0934a', fontFamily: TITLE_FONT, fontStyle: 'bold',
    }).setOrigin(0.5, 0));

    // Dialogue text
    this.dialoguePanel.add(this.add.text(pw / 2, px(32), data.dialogue, {
      fontSize: fs(14), color: '#e0d8cc', fontFamily: FONT, wordWrap: { width: pw - px(30) },
    }).setOrigin(0.5, 0));

    // Action buttons
    const btnStartY = px(60);
    data.actions.forEach((action, i) => {
      const by = btnStartY + i * px(32);
      const btnBg = this.add.rectangle(pw / 2, by + px(12), pw - px(40), px(26), 0x1a2a1a)
        .setStrokeStyle(Math.round(1 * DPR), 0x27ae60).setInteractive({ useHandCursor: true });
      btnBg.on('pointerdown', () => {
        action.callback();
        this.closeDialogue();
      });
      this.dialoguePanel!.add(btnBg);
      this.dialoguePanel!.add(this.add.text(pw / 2, by + px(12), action.label, {
        fontSize: fs(14), color: '#27ae60', fontFamily: FONT,
      }).setOrigin(0.5));
    });

    // Close hint
    this.dialoguePanel.add(this.add.text(pw / 2, ph - px(16), '点击外部关闭', {
      fontSize: fs(12), color: '#555', fontFamily: FONT,
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
    const pw = px(700), ph = px(480);
    const panelX = (W - pw) / 2, panelY = (H - ph) / 2;
    this.questLogPanel = this.add.container(panelX, panelY).setDepth(4000);
    this.animatePanelOpen(this.questLogPanel);

    // Background
    const bg = this.add.rectangle(0, 0, pw, ph, 0x0f0f1e, 0.95).setOrigin(0, 0).setStrokeStyle(Math.round(2 * DPR), 0xc0934a);
    this.questLogPanel.add(bg);

    // Title
    this.questLogPanel.add(this.add.text(pw / 2, px(14), '任务日志', {
      fontSize: fs(18), color: '#c0934a', fontFamily: TITLE_FONT, fontStyle: 'bold',
    }).setOrigin(0.5, 0));

    // Close button
    const closeBtn = this.add.text(pw - px(16), px(10), '\u2715', {
      fontSize: fs(18), color: '#c0392b', fontFamily: FONT,
    }).setOrigin(0.5, 0).setInteractive({ useHandCursor: true });
    closeBtn.on('pointerdown', () => this.toggleQuestLog());
    this.questLogPanel.add(closeBtn);

    // Tab buttons
    const tabY = px(38);
    const activeTab = this.add.rectangle(px(100), tabY, px(160), px(24), this.questLogTab === 'active' ? 0x1a2a3a : 0x111122)
      .setStrokeStyle(Math.round(1 * DPR), 0x2471a3).setInteractive({ useHandCursor: true });
    activeTab.on('pointerdown', () => { this.questLogTab = 'active'; this.questLogPage = 0; this.questLogSelectedIndex = 0; this.refreshQuestLog(); });
    this.questLogPanel.add(activeTab);
    this.questLogPanel.add(this.add.text(px(100), tabY, '进行中', {
      fontSize: fs(14), color: this.questLogTab === 'active' ? '#5dade2' : '#666', fontFamily: FONT,
    }).setOrigin(0.5));

    const completedTab = this.add.rectangle(px(270), tabY, px(160), px(24), this.questLogTab === 'completed' ? 0x1a2a3a : 0x111122)
      .setStrokeStyle(Math.round(1 * DPR), 0x2471a3).setInteractive({ useHandCursor: true });
    completedTab.on('pointerdown', () => { this.questLogTab = 'completed'; this.questLogPage = 0; this.questLogSelectedIndex = 0; this.refreshQuestLog(); });
    this.questLogPanel.add(completedTab);
    this.questLogPanel.add(this.add.text(px(270), tabY, '已完成', {
      fontSize: fs(14), color: this.questLogTab === 'completed' ? '#5dade2' : '#666', fontFamily: FONT,
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

    const pw = px(700), listW = px(240), detailX = px(255), detailW = px(430);
    const listStartY = px(58), itemH = px(28), maxItems = 14;

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
    this.questLogPanel.add(this.add.rectangle(listW + px(7), px(58), Math.round(1 * DPR), px(400), 0x333344).setOrigin(0, 0));

    // Quest list
    pageQuests.forEach((entry, i) => {
      const y = listStartY + i * itemH;
      const isSelected = i === this.questLogSelectedIndex;
      const listBg = this.add.rectangle(px(5), y, listW, itemH - px(2), isSelected ? 0x1a2a3a : 0x0f0f1e)
        .setOrigin(0, 0).setStrokeStyle(isSelected ? Math.round(1 * DPR) : 0, 0x2471a3)
        .setInteractive({ useHandCursor: true });
      listBg.on('pointerdown', () => { this.questLogSelectedIndex = i; this.refreshQuestLog(); });
      this.questLogPanel!.add(listBg);

      const tagColor = entry.quest.category === 'main' ? '#c0934a' : '#95a5a6';
      const tag = entry.quest.category === 'main' ? '[主]' : '[支]';
      this.questLogPanel!.add(this.add.text(px(12), y + px(5), tag, {
        fontSize: fs(12), color: tagColor, fontFamily: FONT, fontStyle: 'bold',
      }));
      const nameColor = this.questLogTab === 'completed' ? '#555' : (isSelected ? '#e0d8cc' : '#aaa');
      this.questLogPanel!.add(this.add.text(px(36), y + px(5), `${entry.quest.name} Lv.${entry.quest.level}`, {
        fontSize: fs(12), color: nameColor, fontFamily: FONT,
      }));
    });

    // Pagination
    if (totalPages > 1) {
      const pageY = listStartY + maxItems * itemH + px(4);
      if (this.questLogPage > 0) {
        const prevBtn = this.add.text(px(40), pageY, '\u25C0 上一页', {
          fontSize: fs(12), color: '#5dade2', fontFamily: FONT,
        }).setInteractive({ useHandCursor: true });
        prevBtn.on('pointerdown', () => { this.questLogPage--; this.questLogSelectedIndex = 0; this.refreshQuestLog(); });
        this.questLogPanel.add(prevBtn);
      }
      this.questLogPanel.add(this.add.text(px(120), pageY, `${this.questLogPage + 1}/${totalPages}`, {
        fontSize: fs(12), color: '#666', fontFamily: FONT,
      }));
      if (this.questLogPage < totalPages - 1) {
        const nextBtn = this.add.text(px(160), pageY, '下一页 \u25B6', {
          fontSize: fs(12), color: '#5dade2', fontFamily: FONT,
        }).setInteractive({ useHandCursor: true });
        nextBtn.on('pointerdown', () => { this.questLogPage++; this.questLogSelectedIndex = 0; this.refreshQuestLog(); });
        this.questLogPanel.add(nextBtn);
      }
    }

    // No quests message
    if (pageQuests.length === 0) {
      this.questLogPanel.add(this.add.text(px(120), px(120), this.questLogTab === 'active' ? '暂无进行中的任务' : '暂无已完成的任务', {
        fontSize: fs(14), color: '#555', fontFamily: FONT,
      }).setOrigin(0.5, 0));
      return;
    }

    // Quest detail (right side)
    const selected = pageQuests[this.questLogSelectedIndex] ?? pageQuests[0];
    if (!selected) return;

    let dy = listStartY;

    // Quest name
    this.questLogPanel.add(this.add.text(detailX + detailW / 2, dy, selected.quest.name, {
      fontSize: fs(17), color: '#c0934a', fontFamily: TITLE_FONT, fontStyle: 'bold',
    }).setOrigin(0.5, 0));
    dy += px(24);

    // Category + Level + Zone
    const zoneNames: Record<string, string> = {
      emerald_plains: '翡翠平原', twilight_forest: '暮色森林',
      anvil_mountains: '铁砧山脉', scorching_desert: '灼热沙漠', abyss_rift: '深渊裂隙',
    };
    const catText = selected.quest.category === 'main' ? '主线任务' : '支线任务';
    const catColor = selected.quest.category === 'main' ? '#c0934a' : '#95a5a6';
    this.questLogPanel.add(this.add.text(detailX + px(5), dy, `${catText}  |  Lv.${selected.quest.level}  |  ${zoneNames[selected.quest.zone] ?? selected.quest.zone}`, {
      fontSize: fs(12), color: catColor, fontFamily: FONT,
    }));
    dy += px(20);

    // Description
    this.questLogPanel.add(this.add.text(detailX + px(5), dy, selected.quest.description, {
      fontSize: fs(13), color: '#bbb', fontFamily: FONT, wordWrap: { width: detailW - px(20) },
    }));
    dy += px(40);

    // Objectives header
    this.questLogPanel.add(this.add.text(detailX + px(5), dy, '目标:', {
      fontSize: fs(13), color: '#e0d8cc', fontFamily: FONT, fontStyle: 'bold',
    }));
    dy += px(18);

    // Objectives with progress
    for (let i = 0; i < selected.quest.objectives.length; i++) {
      const obj = selected.quest.objectives[i];
      const cur = selected.progress.objectives[i]?.current ?? 0;
      const done = cur >= obj.required;
      const statusText = done ? '\u2713' : `${cur}/${obj.required}`;
      const objColor = done ? '#27ae60' : '#e0d8cc';

      this.questLogPanel.add(this.add.text(detailX + px(15), dy, `\u2022 ${obj.targetName}  ${statusText}`, {
        fontSize: fs(12), color: objColor, fontFamily: FONT,
      }));

      // Progress bar
      const barX = detailX + px(15), barY = dy + px(14), barW = detailW - px(40), barH = px(4);
      this.questLogPanel.add(this.add.rectangle(barX, barY, barW, barH, 0x1a1a2e).setOrigin(0, 0).setStrokeStyle(Math.round(1 * DPR), 0x333344));
      const fillW = Math.min(barW * (cur / obj.required), barW);
      if (fillW > 0) {
        this.questLogPanel.add(this.add.rectangle(barX, barY, fillW, barH, done ? 0x27ae60 : 0x2471a3).setOrigin(0, 0));
      }
      dy += px(24);
    }

    dy += px(8);

    // Rewards
    this.questLogPanel.add(this.add.text(detailX + px(5), dy, '奖励:', {
      fontSize: fs(13), color: '#e0d8cc', fontFamily: FONT, fontStyle: 'bold',
    }));
    dy += px(18);

    const rewardParts: string[] = [];
    rewardParts.push(`经验 +${selected.quest.rewards.exp}`);
    rewardParts.push(`金币 +${selected.quest.rewards.gold}`);
    if (selected.quest.rewards.items) {
      rewardParts.push(`物品 x${selected.quest.rewards.items.length}`);
    }
    this.questLogPanel.add(this.add.text(detailX + px(15), dy, rewardParts.join('  |  '), {
      fontSize: fs(12), color: '#f1c40f', fontFamily: FONT,
    }));
    dy += px(20);

    // Prereqs
    if (selected.quest.prereqQuests && selected.quest.prereqQuests.length > 0) {
      dy += px(4);
      const prereqNames = selected.quest.prereqQuests.map(pid => {
        const pq = this.zone!.questSystem.quests.get(pid);
        return pq ? pq.name : pid;
      });
      this.questLogPanel.add(this.add.text(detailX + px(5), dy, `前置任务: ${prereqNames.join(', ')}`, {
        fontSize: fs(12), color: '#666', fontFamily: FONT,
      }));
    }
  }

  // --- Item Tooltip ---
  private showItemTooltip(item: ItemInstance, screenX: number, screenY: number): void {
    this.hideItemTooltip();
    const base = getItemBase(item.baseId);
    const tipW = px(200);
    const lines: { text: string; color: string; size: number }[] = [];
    const qualityColors: Record<string, string> = {
      normal: '#cccccc', magic: '#5dade2', rare: '#f1c40f', legendary: '#e67e22', set: '#2ecc71',
    };
    const qualityLabels: Record<string, string> = {
      normal: '普通', magic: '魔法', rare: '稀有', legendary: '传奇', set: '套装',
    };
    lines.push({ text: item.name, color: qualityColors[item.quality] ?? '#ccc', size: 14 });
    lines.push({ text: `${qualityLabels[item.quality] ?? ''} Lv.${item.level}`, color: '#888', size: 12 });
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
      lines.push({ text: typeLine, color: '#777', size: 12 });
      if ('baseDamage' in base) {
        const wb = base as WeaponBase;
        lines.push({ text: `伤害: ${wb.baseDamage[0]}-${wb.baseDamage[1]}`, color: '#e0d8cc', size: 12 });
      }
      if ('baseDefense' in base) {
        const ab = base as ArmorBase;
        lines.push({ text: `防御: ${ab.baseDefense}`, color: '#e0d8cc', size: 12 });
      }
    }
    if (!item.identified && item.quality !== 'normal') {
      lines.push({ text: '[未鉴定]', color: '#e74c3c', size: 12 });
    } else {
      for (const affix of item.affixes) {
        const disp = STAT_DISPLAY[affix.stat];
        const label = disp ? disp.label : affix.name;
        const suffix = disp?.isPercent ? '%' : '';
        lines.push({ text: `+${affix.value}${suffix} ${label}`, color: '#5dade2', size: 12 });
      }
    }
    if (item.legendaryEffect) {
      lines.push({ text: item.legendaryEffect, color: '#e67e22', size: 12 });
    }
    if (base) {
      lines.push({ text: `售价: ${base.sellPrice}G`, color: '#f1c40f', size: 12 });
    }

    let tipH = px(12);
    for (const line of lines) tipH += px(line.size) + px(4);
    tipH += px(8);

    // Clamp to screen
    let tx = screenX + px(12);
    let ty = screenY - px(10);
    if (tx + tipW > W) tx = screenX - tipW - px(12);
    if (ty + tipH > H) ty = H - tipH - px(4);
    if (ty < px(4)) ty = px(4);

    const qualityBorderColors: Record<string, number> = {
      normal: 0x555555, magic: 0x5dade2, rare: 0xf1c40f, legendary: 0xe67e22, set: 0x2ecc71,
    };
    const borderColor = qualityBorderColors[item.quality] ?? 0x666677;
    this.tooltipContainer = this.add.container(tx, ty).setDepth(5000);
    this.tooltipContainer.add(this.add.rectangle(0, 0, tipW, tipH, 0x0a0a18, 0.95).setOrigin(0, 0).setStrokeStyle(1.5 * DPR, borderColor));
    let ly = px(6);
    for (const line of lines) {
      this.tooltipContainer.add(this.add.text(px(8), ly, line.text, {
        fontSize: fs(line.size), color: line.color, fontFamily: FONT, wordWrap: { width: tipW - px(16) },
      }));
      ly += px(line.size) + px(4);
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

    const popW = px(80), btnH = px(24);
    const popH = actions.length * btnH + px(8);
    let popX = screenX;
    let popY = screenY;
    if (popX + popW > W) popX = W - popW - px(4);
    if (popY + popH > H) popY = H - popH - px(4);

    this.contextPopup = this.add.container(popX, popY).setDepth(5001);
    this.contextPopup.add(this.add.rectangle(0, 0, popW, popH, 0x0a0a18, 0.95).setOrigin(0, 0).setStrokeStyle(Math.round(1 * DPR), 0x555566));
    actions.forEach((action, i) => {
      const by = px(4) + i * btnH;
      const btnBg = this.add.rectangle(px(4), by, popW - px(8), btnH - px(2), 0x1a1a2e).setOrigin(0, 0)
        .setInteractive({ useHandCursor: true });
      btnBg.on('pointerdown', () => action.callback());
      btnBg.on('pointerover', () => btnBg.setFillStyle(0x2a2a3e));
      btnBg.on('pointerout', () => btnBg.setFillStyle(0x1a1a2e));
      this.contextPopup!.add(btnBg);
      this.contextPopup!.add(this.add.text(popW / 2, by + btnH / 2 - px(1), action.label, {
        fontSize: fs(13), color: action.label === '丢弃' ? '#e74c3c' : '#e0d8cc', fontFamily: FONT,
      }).setOrigin(0.5));
    });
  }

  private hideContextPopup(): void {
    if (this.contextPopup) { this.contextPopup.destroy(); this.contextPopup = null; }
  }

  private showDiscardConfirm(item: ItemInstance): void {
    this.hideContextPopup();
    const popW = px(160), popH = px(60);
    const popX = (W - popW) / 2, popY = (H - popH) / 2;
    this.contextPopup = this.add.container(popX, popY).setDepth(5002);
    this.contextPopup.add(this.add.rectangle(0, 0, popW, popH, 0x0a0a18, 0.95).setOrigin(0, 0).setStrokeStyle(Math.round(1 * DPR), 0xe74c3c));
    this.contextPopup.add(this.add.text(popW / 2, px(8), '确定丢弃?', {
      fontSize: fs(14), color: '#e74c3c', fontFamily: FONT,
    }).setOrigin(0.5, 0));
    const yesBtn = this.add.text(popW / 2 - px(30), px(34), '[确定]', {
      fontSize: fs(13), color: '#e74c3c', fontFamily: FONT,
    }).setInteractive({ useHandCursor: true });
    yesBtn.on('pointerdown', () => {
      this.zone.inventorySystem.discardItem(item.uid);
      this.hideContextPopup();
      this.refreshInventory();
    });
    this.contextPopup.add(yesBtn);
    const noBtn = this.add.text(popW / 2 + px(30), px(34), '[取消]', {
      fontSize: fs(13), color: '#27ae60', fontFamily: FONT,
    }).setInteractive({ useHandCursor: true });
    noBtn.on('pointerdown', () => this.hideContextPopup());
    this.contextPopup.add(noBtn);
  }

  private refreshInventory(): void {
    if (this.inventoryPanel) { this.inventoryPanel.destroy(); this.inventoryPanel = null; this.toggleInventory(); }
  }

  // --- Companion Panel (P) ---
  private toggleCompanion(): void {
    if (this.companionPanel) { this.companionPanel.destroy(); this.companionPanel = null; return; }
    this.closeAllPanels();
    audioManager.playSFX('click');
    this.buildCompanionPanel();
  }

  private buildCompanionPanel(): void {
    const pw = px(480), ph = px(440), panelX = (W - pw) / 2, panelY = px(20);
    this.companionPanel = this.add.container(panelX, panelY).setDepth(4000);
    this.animatePanelOpen(this.companionPanel);

    // Background
    this.companionPanel.add(
      this.add.rectangle(0, 0, pw, ph, 0x0f0f1e, 0.95).setOrigin(0, 0).setStrokeStyle(Math.round(2 * DPR), 0x27ae60)
    );

    // Title
    this.companionPanel.add(this.add.text(pw / 2, px(10), '佣兵伙伴', {
      fontSize: fs(18), color: '#27ae60', fontFamily: TITLE_FONT, fontStyle: 'bold',
    }).setOrigin(0.5, 0));

    // Close button
    const closeBtn = this.add.text(pw - px(16), px(8), 'X', {
      fontSize: fs(18), color: '#e74c3c', fontFamily: FONT,
    }).setOrigin(0.5, 0).setInteractive({ useHandCursor: true });
    closeBtn.on('pointerdown', () => this.toggleCompanion());
    this.companionPanel.add(closeBtn);

    const mercSys = this.zone?.mercenarySystem;
    if (!mercSys) return;

    const merc = mercSys.getMercenary();

    if (!merc) {
      // No mercenary — show hire panel
      this.renderHirePanel(pw, ph, mercSys);
    } else {
      // Has mercenary — show info panel
      this.renderMercenaryInfo(pw, ph, merc, mercSys);
    }

    // Footer
    this.companionPanel.add(this.add.text(pw / 2, ph - px(14), '按 P 关闭', {
      fontSize: fs(10), color: '#3a3a4a', fontFamily: FONT,
    }).setOrigin(0.5));
  }

  private renderHirePanel(pw: number, ph: number, mercSys: MercenarySystem): void {
    if (!this.companionPanel) return;

    this.companionPanel.add(this.add.text(pw / 2, px(36), '─ 可雇佣佣兵 ─', {
      fontSize: fs(14), color: '#c0934a', fontFamily: FONT,
    }).setOrigin(0.5, 0));

    // Check if player is near camp
    const safeRadius = (this.zone as any)?.mapData?.safeZoneRadius ?? 9;
    const campPositions: { col: number; row: number }[] = (this.zone as any)?.campPositions ?? [];
    let isNearCamp = false;
    for (const camp of campPositions) {
      const dx = this.player.tileCol - camp.col;
      const dy = this.player.tileRow - camp.row;
      if (Math.sqrt(dx * dx + dy * dy) < safeRadius) {
        isNearCamp = true;
        break;
      }
    }

    if (!isNearCamp) {
      this.companionPanel.add(this.add.text(pw / 2, px(60), '需要在营地NPC附近才能雇佣佣兵', {
        fontSize: fs(13), color: '#888', fontFamily: FONT,
      }).setOrigin(0.5, 0));
      return;
    }

    const typeNames: Record<string, string> = {
      tank: '坦克', melee: '近战输出', ranged: '远程输出', healer: '治疗', mage: '法师',
    };
    const roleColors: Record<string, number> = {
      tank: 0x2471a3, melee: 0xc0392b, ranged: 0x27ae60, healer: 0xf1c40f, mage: 0x8e44ad,
    };

    const cardH = px(68);
    const startY = px(58);

    MERCENARY_TYPES.forEach((type, i) => {
      const def = MERCENARY_DEFS[type];
      const cy = startY + i * (cardH + px(6));
      const roleColor = roleColors[type] ?? 0x888888;
      const canAfford = this.player.gold >= def.hireCost;

      // Card bg
      const cardBg = this.add.rectangle(pw / 2, cy + cardH / 2, pw - px(24), cardH, 0x111122, 0.95)
        .setStrokeStyle(Math.round(1 * DPR), roleColor, 0.6);
      this.companionPanel!.add(cardBg);

      // Color indicator
      this.companionPanel!.add(
        this.add.rectangle(px(16), cy + cardH / 2, px(6), cardH - px(8), roleColor).setOrigin(0, 0.5)
      );

      // Type name + Chinese label
      this.companionPanel!.add(this.add.text(px(30), cy + px(6), `${def.name} (${typeNames[type]})`, {
        fontSize: fs(14), color: '#e0d8cc', fontFamily: FONT, fontStyle: 'bold',
      }));

      // Description
      this.companionPanel!.add(this.add.text(px(30), cy + px(24), def.description, {
        fontSize: fs(11), color: '#888', fontFamily: FONT, wordWrap: { width: pw - px(180) },
      }));

      // Stats preview
      const statsStr = `HP:${def.baseHp}  伤害:${def.baseDamage}  防御:${def.baseDefense}  范围:${def.attackRange}`;
      this.companionPanel!.add(this.add.text(px(30), cy + px(42), statsStr, {
        fontSize: fs(10), color: '#666', fontFamily: FONT,
      }));

      // Cost + Hire button
      this.companionPanel!.add(this.add.text(pw - px(80), cy + px(10), `${def.hireCost}G`, {
        fontSize: fs(14), color: canAfford ? '#f1c40f' : '#555', fontFamily: FONT, fontStyle: 'bold',
      }).setOrigin(0.5, 0));

      const hireBtnBg = this.add.rectangle(pw - px(80), cy + px(38), px(80), px(22), canAfford ? 0x1a3a1a : 0x1a1a1a)
        .setStrokeStyle(Math.round(1 * DPR), canAfford ? 0x27ae60 : 0x333333);
      this.companionPanel!.add(hireBtnBg);

      const hireBtnText = this.add.text(pw - px(80), cy + px(38), '雇佣', {
        fontSize: fs(13), color: canAfford ? '#27ae60' : '#555', fontFamily: FONT,
      }).setOrigin(0.5);
      this.companionPanel!.add(hireBtnText);

      if (canAfford) {
        hireBtnBg.setInteractive({ useHandCursor: true });
        hireBtnBg.on('pointerdown', () => {
          const result = mercSys.hire(type, this.player.gold);
          if (result.success) {
            this.player.gold -= result.cost;
            // Spawn mercenary sprite in zone
            const zoneScene = this.zone as any;
            if (zoneScene?.spawnMercenarySprite) {
              zoneScene.spawnMercenarySprite();
            }
            // Rebuild panel
            this.companionPanel?.destroy();
            this.companionPanel = null;
            this.buildCompanionPanel();
          }
        });
        hireBtnBg.on('pointerover', () => hireBtnBg.setFillStyle(0x225522));
        hireBtnBg.on('pointerout', () => hireBtnBg.setFillStyle(0x1a3a1a));
      }
    });
  }

  private renderMercenaryInfo(pw: number, ph: number, merc: MercenaryState, mercSys: MercenarySystem): void {
    if (!this.companionPanel) return;
    const def = MERCENARY_DEFS[merc.type];
    const roleColors: Record<string, number> = {
      tank: 0x2471a3, melee: 0xc0392b, ranged: 0x27ae60, healer: 0xf1c40f, mage: 0x8e44ad,
    };
    const typeNames: Record<string, string> = {
      tank: '坦克', melee: '近战输出', ranged: '远程输出', healer: '治疗', mage: '法师',
    };
    const roleColor = roleColors[merc.type] ?? 0x888888;

    // Merc name + level
    this.companionPanel.add(this.add.text(pw / 2, px(36), `${def.name} (${typeNames[merc.type]})`, {
      fontSize: fs(16), color: '#e0d8cc', fontFamily: FONT, fontStyle: 'bold',
    }).setOrigin(0.5, 0));

    // Alive/dead status
    const statusText = merc.alive ? `Lv.${merc.level}  状态: 存活` : `Lv.${merc.level}  状态: 阵亡`;
    const statusColor = merc.alive ? '#27ae60' : '#e74c3c';
    this.companionPanel.add(this.add.text(pw / 2, px(56), statusText, {
      fontSize: fs(13), color: statusColor, fontFamily: FONT,
    }).setOrigin(0.5, 0));

    // Description
    this.companionPanel.add(this.add.text(px(14), px(80), def.description, {
      fontSize: fs(12), color: '#888', fontFamily: FONT,
    }));

    // Stats section
    let sy = px(102);
    this.companionPanel.add(this.add.text(px(14), sy, '─ 属性 ─', {
      fontSize: fs(13), color: '#c0934a', fontFamily: FONT,
    }));
    sy += px(20);

    // HP/Mana bars
    const barW = px(140), barH = px(8);
    // HP bar
    this.companionPanel.add(this.add.text(px(14), sy, 'HP', {
      fontSize: fs(11), color: '#e74c3c', fontFamily: FONT,
    }));
    this.companionPanel.add(
      this.add.rectangle(px(50), sy + px(4), barW, barH, 0x1a1a1a).setOrigin(0, 0.5)
        .setStrokeStyle(Math.round(1 * DPR), 0x333333)
    );
    const hpFillW = merc.alive ? Math.max(0, barW * (merc.hp / merc.maxHp)) : 0;
    if (hpFillW > 0) {
      this.companionPanel.add(
        this.add.rectangle(px(50), sy + px(4), hpFillW, barH, 0xe74c3c).setOrigin(0, 0.5)
      );
    }
    this.companionPanel.add(this.add.text(px(200), sy, `${Math.ceil(merc.hp)}/${merc.maxHp}`, {
      fontSize: fs(11), color: '#aaa', fontFamily: FONT,
    }));
    sy += px(18);

    // Mana bar
    this.companionPanel.add(this.add.text(px(14), sy, 'MP', {
      fontSize: fs(11), color: '#2471a3', fontFamily: FONT,
    }));
    this.companionPanel.add(
      this.add.rectangle(px(50), sy + px(4), barW, barH, 0x1a1a1a).setOrigin(0, 0.5)
        .setStrokeStyle(Math.round(1 * DPR), 0x333333)
    );
    const manaFillW = merc.alive ? Math.max(0, barW * (merc.mana / merc.maxMana)) : 0;
    if (manaFillW > 0) {
      this.companionPanel.add(
        this.add.rectangle(px(50), sy + px(4), manaFillW, barH, 0x2471a3).setOrigin(0, 0.5)
      );
    }
    this.companionPanel.add(this.add.text(px(200), sy, `${Math.ceil(merc.mana)}/${merc.maxMana}`, {
      fontSize: fs(11), color: '#aaa', fontFamily: FONT,
    }));
    sy += px(18);

    // EXP
    const expNeeded = mercSys.expToNextLevel(merc.level);
    this.companionPanel.add(this.add.text(px(14), sy, `经验: ${merc.exp}/${expNeeded}`, {
      fontSize: fs(11), color: '#b08cce', fontFamily: FONT,
    }));
    sy += px(18);

    // Base stats
    const statRow = [
      `攻击: ${merc.baseDamage}`,
      `防御: ${merc.defense}`,
      `范围: ${merc.attackRange}`,
    ].join('  |  ');
    this.companionPanel.add(this.add.text(px(14), sy, statRow, {
      fontSize: fs(11), color: '#aaa', fontFamily: FONT,
    }));
    sy += px(18);

    // Primary stats
    const primaryStats = `力:${Math.floor(merc.stats.str)}  敏:${Math.floor(merc.stats.dex)}  体:${Math.floor(merc.stats.vit)}  智:${Math.floor(merc.stats.int)}  精:${Math.floor(merc.stats.spi)}  幸:${Math.floor(merc.stats.lck)}`;
    this.companionPanel.add(this.add.text(px(14), sy, primaryStats, {
      fontSize: fs(11), color: '#888', fontFamily: FONT,
    }));
    sy += px(22);

    // Equipment section
    this.companionPanel.add(this.add.text(px(14), sy, '─ 装备 ─', {
      fontSize: fs(13), color: '#c0934a', fontFamily: FONT,
    }));
    sy += px(20);

    const slotSize = px(36);

    // Weapon slot
    const weaponItem = merc.equipment.weapon;
    const weaponBg = this.add.rectangle(px(14) + slotSize / 2, sy + slotSize / 2, slotSize, slotSize,
      weaponItem ? this.getQualityColorNum(weaponItem.quality) : 0x222233)
      .setStrokeStyle(Math.round(1 * DPR), roleColor, 0.5);
    this.companionPanel.add(weaponBg);
    this.companionPanel.add(this.add.text(px(14) + slotSize / 2, sy + slotSize + px(2), '武器', {
      fontSize: fs(10), color: '#777788', fontFamily: FONT,
    }).setOrigin(0.5, 0));
    if (weaponItem) {
      this.companionPanel.add(this.add.text(px(14) + slotSize / 2, sy + slotSize / 2, weaponItem.name.charAt(0), {
        fontSize: fs(14), color: '#fff', fontFamily: FONT, fontStyle: 'bold',
      }).setOrigin(0.5));
    }

    // Armor slot
    const armorItem = merc.equipment.armor;
    const armorBg = this.add.rectangle(px(14) + slotSize * 2, sy + slotSize / 2, slotSize, slotSize,
      armorItem ? this.getQualityColorNum(armorItem.quality) : 0x222233)
      .setStrokeStyle(Math.round(1 * DPR), roleColor, 0.5);
    this.companionPanel.add(armorBg);
    this.companionPanel.add(this.add.text(px(14) + slotSize * 2, sy + slotSize + px(2), '护甲', {
      fontSize: fs(10), color: '#777788', fontFamily: FONT,
    }).setOrigin(0.5, 0));
    if (armorItem) {
      this.companionPanel.add(this.add.text(px(14) + slotSize * 2, sy + slotSize / 2, armorItem.name.charAt(0), {
        fontSize: fs(14), color: '#fff', fontFamily: FONT, fontStyle: 'bold',
      }).setOrigin(0.5));
    }

    sy += slotSize + px(22);

    // Action buttons
    const btnY = sy + px(6);

    if (merc.alive) {
      // Dismiss button
      const dismissBg = this.add.rectangle(px(60), btnY, px(100), px(26), 0x2a1a1a)
        .setStrokeStyle(Math.round(1 * DPR), 0xc0392b).setInteractive({ useHandCursor: true });
      dismissBg.on('pointerdown', () => {
        mercSys.dismiss();
        const zoneScene = this.zone as any;
        if (zoneScene?.destroyMercenarySprite) {
          zoneScene.destroyMercenarySprite();
        }
        this.companionPanel?.destroy();
        this.companionPanel = null;
        this.buildCompanionPanel();
      });
      this.companionPanel.add(dismissBg);
      this.companionPanel.add(this.add.text(px(60), btnY, '解雇', {
        fontSize: fs(13), color: '#e74c3c', fontFamily: FONT,
      }).setOrigin(0.5));
    } else {
      // Revive button
      const reviveCost = def.reviveCost;
      const canRevive = this.player.gold >= reviveCost;

      // Check if near camp
      const safeRadius = (this.zone as any)?.mapData?.safeZoneRadius ?? 9;
      const campPositions: { col: number; row: number }[] = (this.zone as any)?.campPositions ?? [];
      let isNearCamp = false;
      for (const camp of campPositions) {
        const dx = this.player.tileCol - camp.col;
        const dy = this.player.tileRow - camp.row;
        if (Math.sqrt(dx * dx + dy * dy) < safeRadius) {
          isNearCamp = true;
          break;
        }
      }

      if (!isNearCamp) {
        this.companionPanel.add(this.add.text(pw / 2, btnY, '在营地NPC处复活佣兵', {
          fontSize: fs(12), color: '#888', fontFamily: FONT,
        }).setOrigin(0.5));
      } else {
        const reviveBg = this.add.rectangle(px(100), btnY, px(160), px(26), canRevive ? 0x1a3a1a : 0x1a1a1a)
          .setStrokeStyle(Math.round(1 * DPR), canRevive ? 0x27ae60 : 0x333333);
        this.companionPanel.add(reviveBg);
        this.companionPanel.add(this.add.text(px(100), btnY, `复活 (${reviveCost}G)`, {
          fontSize: fs(13), color: canRevive ? '#27ae60' : '#555', fontFamily: FONT,
        }).setOrigin(0.5));

        if (canRevive) {
          reviveBg.setInteractive({ useHandCursor: true });
          reviveBg.on('pointerdown', () => {
            const result = mercSys.revive(this.player.gold);
            if (result.success) {
              this.player.gold -= result.cost;
              // Set position near player
              mercSys.setPosition(this.player.tileCol + 1, this.player.tileRow + 1);
              const zoneScene = this.zone as any;
              if (zoneScene?.spawnMercenarySprite) {
                zoneScene.spawnMercenarySprite();
              }
              this.companionPanel?.destroy();
              this.companionPanel = null;
              this.buildCompanionPanel();
            }
          });
        }
      }

      // Dismiss button (even when dead, can dismiss to hire a new one)
      const dismissBg2 = this.add.rectangle(pw - px(80), btnY, px(100), px(26), 0x2a1a1a)
        .setStrokeStyle(Math.round(1 * DPR), 0xc0392b).setInteractive({ useHandCursor: true });
      dismissBg2.on('pointerdown', () => {
        mercSys.dismiss();
        const zoneScene = this.zone as any;
        if (zoneScene?.destroyMercenarySprite) {
          zoneScene.destroyMercenarySprite();
        }
        this.companionPanel?.destroy();
        this.companionPanel = null;
        this.buildCompanionPanel();
      });
      this.companionPanel.add(dismissBg2);
      this.companionPanel.add(this.add.text(pw - px(80), btnY, '解雇', {
        fontSize: fs(13), color: '#e74c3c', fontFamily: FONT,
      }).setOrigin(0.5));
    }
  }

  // --- Audio Settings Panel ---
  private toggleAudioSettings(): void {
    if (this.audioPanel) {
      this.cleanupAudioPanelInputHandlers();
      this.audioPanel.destroy();
      this.audioPanel = null;
      return;
    }
    this.closeAllPanels();
    const pw = px(360), ph = px(180), panelX = (W - pw) / 2, panelY = (H - ph) / 2;
    this.audioPanel = this.add.container(panelX, panelY).setDepth(4000);
    this.animatePanelOpen(this.audioPanel);
    this.audioPanel.add(this.add.rectangle(0, 0, pw, ph, 0x0f0f1e, 0.95).setOrigin(0, 0).setStrokeStyle(Math.round(2 * DPR), 0xc0934a));
    this.audioPanel.add(this.add.text(pw / 2, px(12), '音频设置', {
      fontSize: fs(16), color: '#c0934a', fontFamily: TITLE_FONT, fontStyle: 'bold',
    }).setOrigin(0.5, 0));

    const settings = audioManager.getSettings();
    const sliderW = px(160), sliderH = px(10), sliderX = px(90), labelX = px(14);
    const hitH = px(28); // tall hit area for easy clicking

    const makeSlider = (y: number, label: string, initial: number, muted: boolean,
      onVolume: (v: number) => void, onMute: () => boolean) => {
      this.audioPanel!.add(this.add.text(labelX, y, label, { fontSize: fs(14), color: '#e0d8cc', fontFamily: FONT }));
      const track = this.add.rectangle(sliderX, y + px(6), sliderW, sliderH, 0x333344).setOrigin(0, 0.5);
      const fill = this.add.rectangle(sliderX, y + px(6), sliderW * initial, sliderH, 0xc0934a).setOrigin(0, 0.5);
      const handle = this.add.circle(sliderX + sliderW * initial, y + px(6), px(7), 0xe0d8cc);
      const pctText = this.add.text(sliderX + sliderW + px(8), y + px(1), `${Math.round(initial * 100)}%`, {
        fontSize: fs(12), color: '#aaa', fontFamily: FONT,
      });
      // Invisible hit area covering the full track height
      const hitArea = this.add.rectangle(sliderX + sliderW / 2, y + px(6), sliderW + px(14), hitH, 0x000000, 0)
        .setInteractive({ useHandCursor: true });
      this.audioPanel!.add([track, fill, handle, pctText, hitArea]);

      let dragging = false;
      const updateSlider = (pointerX: number) => {
        const localX = Math.max(0, Math.min(pointerX - panelX - sliderX, sliderW));
        const v = localX / sliderW;
        handle.x = sliderX + localX;
        fill.width = localX;
        pctText.setText(`${Math.round(v * 100)}%`);
        onVolume(v);
      };
      hitArea.on('pointerdown', (p: Phaser.Input.Pointer) => { dragging = true; updateSlider(p.x); });
      const pointerMoveHandler = (p: Phaser.Input.Pointer) => { if (dragging) updateSlider(p.x); };
      const pointerUpHandler = () => { dragging = false; };
      this.input.on('pointermove', pointerMoveHandler);
      this.input.on('pointerup', pointerUpHandler);
      this.audioPanelInputCleanup.push(() => this.input.off('pointermove', pointerMoveHandler));
      this.audioPanelInputCleanup.push(() => this.input.off('pointerup', pointerUpHandler));

      // Mute button
      const muteBtn = this.add.text(sliderX + sliderW + px(42), y, muted ? '[静音]' : '[开启]', {
        fontSize: fs(12), color: muted ? '#c0392b' : '#27ae60', fontFamily: FONT,
      }).setInteractive({ useHandCursor: true });
      muteBtn.on('pointerdown', () => {
        const nowMuted = onMute();
        muteBtn.setText(nowMuted ? '[静音]' : '[开启]').setColor(nowMuted ? '#c0392b' : '#27ae60');
      });
      this.audioPanel!.add(muteBtn);
    };

    // BGM slider
    makeSlider(px(46), '背景音乐', settings.bgmVolume, settings.bgmMuted,
      (v) => audioManager.setMusicVolume(v),
      () => { audioManager.toggleMusicMute(); return audioManager.getSettings().bgmMuted; });

    // SFX slider
    makeSlider(px(90), '音效', settings.sfxVolume, settings.sfxMuted,
      (v) => audioManager.setSFXVolume(v),
      () => { audioManager.toggleSFXMute(); return audioManager.getSettings().sfxMuted; });

    // Close button
    const closeBtn = this.add.text(pw - px(12), px(8), 'X', {
      fontSize: fs(14), color: '#888', fontFamily: FONT,
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
    if (this.skillTooltip) { this.skillTooltip.destroy(); this.skillTooltip = null; }
    if (this.charPanel) { this.charPanel.destroy(); this.charPanel = null; }
    if (this.homesteadPanel) { this.homesteadPanel.destroy(); this.homesteadPanel = null; }
    if (this.questLogPanel) { this.questLogPanel.destroy(); this.questLogPanel = null; }
    if (this.companionPanel) { this.companionPanel.destroy(); this.companionPanel = null; }
    if (this.audioPanel) {
      this.cleanupAudioPanelInputHandlers();
      this.audioPanel.destroy();
      this.audioPanel = null;
    }
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
    this.closeAllPanels();
    this.cleanupAudioPanelInputHandlers();
    EventBus.off(GameEvents.LOG_MESSAGE, this.handleLogMessage, this);
    EventBus.off(GameEvents.SHOP_OPEN, this.handleShopOpen, this);
    EventBus.off(GameEvents.NPC_INTERACT, this.handleNpcInteract, this);
    EventBus.off(GameEvents.UI_TOGGLE_PANEL, this.handlePanelToggle, this);
    EventBus.off('ui:refresh', this.handleUiRefresh, this);
    this.skillSlots = [];
    this.skillCooldownOverlays = [];
    this.skillCooldownTexts = [];
    this.logTexts = [];
    this.questTrackerTexts = [];
    this.nextMinimapRefreshAt = 0;
    this.nextQuestTrackerRefreshAt = 0;
    this.lastQuestTrackerSignature = '';
  }

  /** Interpolate HP bar color: green -> yellow -> red based on ratio */
  update(time: number): void {
    if (!this.player) return;
    const globeH = GLOBE_R * 2;
    const globeBottom = (H - px(50)) + GLOBE_R;

    const hpR = Math.max(0, this.player.hp / this.player.maxHp);
    const targetHpH = globeH * hpR;
    this.hpBar.height += (targetHpH - this.hpBar.height) * 0.15;
    this.hpBar.y = globeBottom - this.hpBar.height;
    const hpText = `${Math.ceil(this.player.hp)}/${this.player.maxHp}`;
    if (this.hpText.text !== hpText) this.hpText.setText(hpText);
    if (hpR < 0.3 && hpR > 0) {
      const pulse = 0.6 + Math.sin(time * 0.008) * 0.4;
      this.hpBar.alpha = pulse;
    } else {
      this.hpBar.alpha = 1;
    }

    const manaR = Math.max(0, this.player.mana / this.player.maxMana);
    const targetManaH = globeH * manaR;
    this.manaBar.height += (targetManaH - this.manaBar.height) * 0.15;
    this.manaBar.y = globeBottom - this.manaBar.height;
    const manaText = `${Math.ceil(this.player.mana)}/${this.player.maxMana}`;
    if (this.manaText.text !== manaText) this.manaText.setText(manaText);
    const expN = this.player.expToNextLevel();
    this.expBar.width = (W - px(32)) * (this.player.exp / expN);
    const levelText = `Lv.${this.player.level} (${this.player.exp}/${expN})`;
    if (this.levelText.text !== levelText) this.levelText.setText(levelText);
    const goldText = `${this.player.gold} G`;
    if (this.goldText.text !== goldText) this.goldText.setText(goldText);
    const autoCombatText = `AUTO\n${this.player.autoCombat ? 'ON' : 'OFF'}`;
    const autoCombatColor = this.player.autoCombat ? '#27ae60' : '#666680';
    if (this.autoCombatText.text !== autoCombatText) this.autoCombatText.setText(autoCombatText);
    if (this.autoCombatText.style.color !== autoCombatColor) this.autoCombatText.setColor(autoCombatColor);

    // Auto-loot button update
    const alLabels: Record<string, string> = { off: '拾取\nOFF', all: '拾取\n全部', magic: '拾取\n魔法+', rare: '拾取\n稀有+' };
    const alColors: Record<string, string> = { off: '#666680', all: '#e0d8cc', magic: '#2471a3', rare: '#c0934a' };
    const autoLootText = alLabels[this.player.autoLootMode] ?? '拾取\nOFF';
    const autoLootColor = alColors[this.player.autoLootMode] ?? '#666680';
    if (this.autoLootText.text !== autoLootText) this.autoLootText.setText(autoLootText);
    if (this.autoLootText.style.color !== autoLootColor) this.autoLootText.setColor(autoLootColor);

    if (this.zone && (this.zone as any).currentMapId) {
      const map = AllMaps[(this.zone as any).currentMapId];
      if (map && this.zoneLabel.text !== map.name) this.zoneLabel.setText(map.name);
    }

    const skills = this.player.classData.skills;
    for (let i = 0; i < Math.min(skills.length, this.skillCooldownOverlays.length); i++) {
      const cd = this.player.skillCooldowns.get(skills[i].id) ?? 0;
      const remaining = cd - time;
      const onCd = remaining > 0;
      if (this.skillCooldownOverlays[i].visible !== onCd) {
        this.skillCooldownOverlays[i].setVisible(onCd);
      }
      // Show remaining seconds on cooldown
      const cdText = this.skillCooldownTexts[i];
      if (onCd) {
        const secs = Math.ceil(remaining / 1000);
        if (cdText?.active) {
          if (!cdText.visible) cdText.setVisible(true);
          const nextText = `${secs}`;
          if (cdText.text !== nextText) cdText.setText(nextText);
        }
        // Fade overlay alpha based on cooldown progress
        const totalCd = getSkillCooldown(skills[i], this.player.getSkillLevel(skills[i].id));
        this.skillCooldownOverlays[i].alpha = 0.3 + 0.4 * (remaining / totalCd);
      } else {
        if (cdText?.active && cdText.visible) cdText.setVisible(false);
      }
    }

    if (time >= this.nextMinimapRefreshAt) {
      this.nextMinimapRefreshAt = time + 250;
      this.updateMinimap();
    }

    if (this.zone?.questSystem && time >= this.nextQuestTrackerRefreshAt) {
      this.nextQuestTrackerRefreshAt = time + 250;
      this.refreshQuestTracker();
    }
  }

  private refreshQuestTracker(): void {
    if (!this.zone?.questSystem) return;

    const active = this.zone.questSystem.getActiveQuests();
    const sorted = [...active].sort((a, b) => {
      if (a.quest.category === 'main' && b.quest.category !== 'main') return -1;
      if (a.quest.category !== 'main' && b.quest.category === 'main') return 1;
      return 0;
    });

    const entries: { text: string; isTitle: boolean; isMain: boolean; isDone: boolean }[] = [];
    const playerCol = this.zone?.player ? Math.round(this.zone.player.tileCol) : 0;
    const playerRow = this.zone?.player ? Math.round(this.zone.player.tileRow) : 0;
    for (const { quest, progress } of sorted) {
      const tag = quest.category === 'main' ? '[主线]' : '[支线]';
      const statusTag = progress.status === 'completed' ? ' \u2713' : '';
      entries.push({ text: `${tag} ${quest.name}${statusTag}`, isTitle: true, isMain: quest.category === 'main', isDone: progress.status === 'completed' });
      for (let i = 0; i < quest.objectives.length; i++) {
        const obj = quest.objectives[i];
        const cur = progress.objectives[i]?.current ?? 0;
        const done = cur >= obj.required;
        const mark = done ? '\u2713' : `${cur}/${obj.required}`;
        let locHint = '';
        if (obj.type === 'explore' && !done && obj.location) {
          const dc = obj.location.col - playerCol;
          const dr = obj.location.row - playerRow;
          const dist = Math.sqrt(dc * dc + dr * dr);
          if (dist > 3) {
            const dir = getDirection(dc, dr);
            const distLabel = dist > 30 ? '很远' : dist > 15 ? '较远' : '附近';
            locHint = ` (${dir} ${distLabel})`;
          }
        }
        entries.push({ text: `  ${obj.targetName} ${mark}${locHint}`, isTitle: false, isMain: quest.category === 'main', isDone: done });
      }
    }

    const signature = entries.map(entry => `${entry.text}|${entry.isTitle ? 1 : 0}|${entry.isMain ? 1 : 0}|${entry.isDone ? 1 : 0}`).join('\n');
    if (signature === this.lastQuestTrackerSignature) return;
    this.lastQuestTrackerSignature = signature;

    let y = 0;
    for (let i = 0; i < entries.length; i++) {
      const e = entries[i];
      let t = this.questTrackerTexts[i];
      if (!t) {
        t = this.add.text(0, 0, '', { fontFamily: FONT }).setOrigin(0, 0);
        this.questTracker.add(t);
        this.questTrackerTexts.push(t);
      }
      t.setVisible(true);
      t.setText(e.text);
      t.setY(y);
      if (e.isTitle) {
        t.setFontSize(fs(13));
        t.setColor(e.isMain ? '#e8c252' : '#a89060');
        t.setFontStyle('bold');
      } else {
        t.setFontSize(fs(10));
        t.setColor(e.isDone ? '#66aa66' : '#aaaaaa');
        t.setFontStyle('');
      }
      y += e.isTitle ? px(18) : px(14);
    }
    for (let i = entries.length; i < this.questTrackerTexts.length; i++) {
      if (this.questTrackerTexts[i].visible) this.questTrackerTexts[i].setVisible(false);
    }
  }
}

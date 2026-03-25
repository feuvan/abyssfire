import Phaser from 'phaser';
import { GAME_WIDTH, GAME_HEIGHT, DPR } from '../config';
import { EventBus, GameEvents } from '../utils/EventBus';
import { getItemBase, GEM_STAT_MAP } from '../data/items/bases';
import { STAT_DISPLAY } from '../data/items/affixes';
import { SetDefinitions } from '../data/items/sets';
import { DUNGEON_EXCLUSIVE_SETS } from '../data/dungeonData';
import { AllMaps, MapOrder } from '../data/maps/index';
import { getSkillManaCost, getSkillCooldown, getSkillDamageMultiplier, getSkillBuffValue, getSkillBuffDuration, getSkillAoeRadius } from '../systems/CombatSystem';
import { NPCDefinitions } from '../data/npcs';
import { audioManager } from '../systems/audio/AudioManager';
import type { Player } from '../entities/Player';
import type { ZoneScene } from './ZoneScene';
import type { ItemInstance, WeaponBase, ArmorBase, DialogueTree, DialogueNode, DialogueChoice, EquipSlot } from '../data/types';
import { MercenarySystem, MERCENARY_DEFS, MERCENARY_TYPES } from '../systems/MercenarySystem';
import { QUEST_TYPE_LABELS } from '../systems/QuestSystem';
import type { MercenaryState } from '../systems/MercenarySystem';
import { LoreByZone, AllLoreEntries, getLoreCountByZone } from '../data/loreCollectibles';
import type { LoreEntry } from '../data/loreCollectibles';

const FONT = '"Noto Sans SC", sans-serif';
const TITLE_FONT = '"Cinzel", "Noto Sans SC", serif';
const LOG_MAX_LINES = 8;
const GLOBE_R = Math.round(40 * DPR);

/** Unified panel styling config used by ALL panels for visual consistency. */
const PANEL_STYLE = {
  /** Panel background */
  bg: { color: 0x0f0f1e, alpha: 0.95 },
  /** Panel border — same for all panels */
  border: { color: 0xc0934a, width: 2, radius: 0 },
  /** Header area */
  header: {
    height: 36,
    font: TITLE_FONT,
    fontSize: 18,
    color: '#c0934a',
  },
  /** Close button */
  close: {
    fontSize: 16,
    color: '#e74c3c',
    hoverColor: '#ff6666',
  },
  /** Depth layering */
  depth: {
    backdrop: 3999,
    panel: 4000,
    subPanel: 4001,
    tooltip: 5000,
    contextMenu: 5001,
    confirmDialog: 5002,
    toast: 6000,
  },
  /** Tooltip styling */
  tooltip: {
    bg: { color: 0x0a0a18, alpha: 0.97 },
    border: { color: 0xc0934a, width: 1.5 },
    font: FONT,
    titleSize: 13,
    bodySize: 11,
    lineSpacing: 2,
    padding: 10,
  },
} as const;

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
  private socketPanel: Phaser.GameObjects.Container | null = null;
  private socketPanelSlot: string | null = null;
  private achievementPanel: Phaser.GameObjects.Container | null = null;
  private nextMinimapRefreshAt = 0;
  private nextQuestTrackerRefreshAt = 0;
  private lastQuestTrackerSignature = '';
  /** Dialogue tree state: visited nodes and choices per NPC. */
  private dialogueTreeState: Record<string, { visitedNodes: string[]; choicesMade: Record<string, string> }> = {};
  /** Current dialogue tree scroll offset for long text. */
  private dialogueScrollY = 0;
  /** Mini-boss cinematic dialogue panel. */
  private miniBossDialoguePanel: Phaser.GameObjects.Container | null = null;
  private miniBossDialogueBackdrop: Phaser.GameObjects.Rectangle | null = null;
  /** Lore text popup panel. */
  private loreTextPanel: Phaser.GameObjects.Container | null = null;
  private loreTextBackdrop: Phaser.GameObjects.Rectangle | null = null;
  /** Lore log sub-tab in quest log. */
  private questLogLoreTab = false;

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
      const modes: Array<'off' | 'all' | 'magic' | 'rare' | 'legendary'> = ['off', 'all', 'magic', 'rare', 'legendary'];
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
          fontSize: fs(12), color: '#aaa', fontFamily: FONT, wordWrap: { width: panelW - px(16), useAdvancedWrap: true },
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
    EventBus.on(GameEvents.MINIBOSS_DIALOGUE, this.handleMiniBossDialogue, this);
    EventBus.on(GameEvents.LORE_COLLECTED, this.handleLoreCollected, this);
    EventBus.on(GameEvents.ACHIEVEMENT_UNLOCKED, this.handleAchievementUnlocked, this);
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

  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  private handleNpcInteract(data: any): void {
    if (data.dialogueTree) {
      this.openDialogueTree(data);
    } else {
      this.openDialogue(data);
    }
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
    if (data.panel === 'achievement') this.toggleAchievement();
  }

  private handleUiRefresh(data: { player: Player; zone: ZoneScene }): void {
    this.player = data.player;
    this.zone = data.zone;
    this.nextMinimapRefreshAt = 0;
    this.nextQuestTrackerRefreshAt = 0;
    this.lastQuestTrackerSignature = '';
  }

  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  private handleMiniBossDialogue(data: any): void {
    this.showMiniBossDialogue(data.bossName, data.dialogueTree, data.onDismiss);
  }

  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  private handleLoreCollected(data: any): void {
    this.showLoreText(data.entry);
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

    this.inventoryPanel = this.add.container(panelX, panelY).setDepth(PANEL_STYLE.depth.panel);
    this.animatePanelOpen(this.inventoryPanel);
    this.inventoryPanel.add(this.createPanelBg(pw, ph));

    // Title with item count and page
    this.inventoryPanel.add(this.add.text(px(14), px(12), `背包 (${inv.length}/${100})`, {
      fontSize: fs(PANEL_STYLE.header.fontSize), color: PANEL_STYLE.header.color, fontFamily: PANEL_STYLE.header.font, fontStyle: 'bold',
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
    this.inventoryPanel.add(this.createPanelCloseBtn(pw, () => this.toggleInventory()));

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
        // Socket indicator: small diamond on items with sockets
        const maxSock = this.zone.inventorySystem.getMaxSockets(slot as any);
        if (maxSock > 0) {
          const sockLabel = `◆${eq.sockets.length}/${maxSock}`;
          this.inventoryPanel!.add(this.add.text(sx + eqSlotSize - px(2), sy + px(2), sockLabel, {
            fontSize: fs(9), color: '#8be9fd', fontFamily: FONT,
          }).setOrigin(1, 0));
        }
        slotBg.on('pointerover', (pointer: Phaser.Input.Pointer) => {
          this.showItemTooltip(eq, pointer.x, pointer.y);
        });
        slotBg.on('pointerout', () => this.hideItemTooltip());
        slotBg.on('pointerdown', () => {
          const ms = this.zone.inventorySystem.getMaxSockets(slot as any);
          if (ms > 0) {
            this.hideItemTooltip();
            this.openSocketPanel(slot as any);
          } else {
            this.zone.inventorySystem.unequip(slot as any);
            this.refreshInventory();
          }
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
      fontSize: fs(12), color: '#777788', fontFamily: FONT, wordWrap: { width: pw - px(28), useAdvancedWrap: true },
    }));
  }

  // --- Shop Panel (Diablo-style split) ---
  private reopenShop(data: { npcId: string; shopItems: string[]; type: string }): void {
    this.shopPanel?.destroy(); this.shopPanel = null;
    if (this.dialogueBackdrop) { this.dialogueBackdrop.destroy(); this.dialogueBackdrop = null; }
    this.openShop(data, true);
  }

  private openShop(data: { npcId: string; shopItems: string[]; type: string }, keepPage = false): void {
    this.closeAllPanels();
    audioManager.playSFX('click');
    if (!keepPage) this.shopInventoryPage = 0;

    // Backdrop for outside-click dismiss
    this.dialogueBackdrop = this.add.rectangle(W / 2, H / 2, W, H, 0x000000, 0.3)
      .setInteractive().setDepth(PANEL_STYLE.depth.backdrop);
    this.dialogueBackdrop.on('pointerdown', () => {
      this.hideItemTooltip();
      if (this.shopPanel) { this.shopPanel.destroy(); this.shopPanel = null; }
      if (this.dialogueBackdrop) { this.dialogueBackdrop.destroy(); this.dialogueBackdrop = null; }
    });

    const pw = px(700), ph = px(460), panelX = (W - pw) / 2, panelY = px(40);
    const dividerX = px(320);
    this.shopPanel = this.add.container(panelX, panelY).setDepth(PANEL_STYLE.depth.panel);
    this.animatePanelOpen(this.shopPanel);
    this.shopPanel.add(this.createPanelBg(pw, ph));
    const title = data.type === 'blacksmith' ? '铁匠铺' : '商店';
    this.shopPanel.add(this.createPanelTitle(pw, title));
    this.shopPanel.add(this.createPanelCloseBtn(pw, () => {
      this.hideItemTooltip();
      if (this.shopPanel) { this.shopPanel.destroy(); this.shopPanel = null; }
      if (this.dialogueBackdrop) { this.dialogueBackdrop.destroy(); this.dialogueBackdrop = null; }
    }));

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
            this.reopenShop(data);
          }
        });
        this.shopPanel!.add(buyBtn);
      }
    });

    // --- Buyback section ---
    const buybackItems = this.zone.inventorySystem.buybackItems;
    if (buybackItems.length > 0) {
      const buybackStartY = px(58) + data.shopItems.length * px(28) + px(12);
      this.shopPanel.add(this.add.rectangle(dividerX / 2, buybackStartY - px(4), dividerX - px(28), Math.round(1 * DPR), 0x333344));
      this.shopPanel.add(this.add.text(dividerX / 2, buybackStartY, '回购', {
        fontSize: fs(13), color: '#c0934a', fontFamily: FONT,
      }).setOrigin(0.5, 0));
      buybackItems.forEach((entry, i) => {
        const by = buybackStartY + px(20) + i * px(24);
        if (by > ph - px(50)) return;
        const canAfford = this.player.gold >= entry.buybackPrice;
        const qualColor = this.getQualityTextColor(entry.item.quality);
        this.shopPanel!.add(this.add.text(px(14), by, entry.item.name, {
          fontSize: fs(12), color: canAfford ? qualColor : '#555', fontFamily: FONT,
        }));
        this.shopPanel!.add(this.add.text(dividerX - px(60), by, `${entry.buybackPrice}G`, {
          fontSize: fs(12), color: canAfford ? '#e8a040' : '#555', fontFamily: FONT,
        }).setOrigin(1, 0));
        if (canAfford) {
          const bbBtn = this.add.text(dividerX - px(14), by, '[回购]', {
            fontSize: fs(12), color: '#e8a040', fontFamily: FONT,
          }).setOrigin(1, 0).setInteractive({ useHandCursor: true });
          bbBtn.on('pointerdown', () => {
            if (this.player.gold >= entry.buybackPrice) {
              const result = this.zone.inventorySystem.buybackItem(i);
              if (result) {
                this.player.gold -= result.cost;
                audioManager.playSFX('click');
                EventBus.emit(GameEvents.LOG_MESSAGE, { text: `回购了 ${result.item.name}`, type: 'loot' });
              }
              this.reopenShop(data);
            }
          });
          this.shopPanel!.add(bbBtn);
        }
      });
    }

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
      itemBg.on('pointerdown', (pointer: Phaser.Input.Pointer) => {
        this.hideItemTooltip();
        const isRightClick = pointer.rightButtonDown();
        const isHighValue = item.quality === 'legendary' || item.quality === 'set';

        if (isRightClick && !isHighValue) {
          // Right-click quick-sell for rare and below
          const gold = this.zone.inventorySystem.sellItem(item.uid);
          this.player.gold += gold;
          audioManager.playSFX('click');
          this.reopenShop(data);
        } else if (isHighValue) {
          this.showSellConfirm(item, data);
        } else {
          // Left-click on normal/magic — also sell directly
          const gold = this.zone.inventorySystem.sellItem(item.uid);
          this.player.gold += gold;
          audioManager.playSFX('click');
          this.reopenShop(data);
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
          this.reopenShop(data);
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
          this.reopenShop(data);
        });
        this.shopPanel.add(nextBtn);
      }
    }

    // Gold on right side too
    this.shopPanel.add(this.add.text(rightX, ph - px(26), `金币: ${this.player.gold}G`, {
      fontSize: fs(13), color: '#f1c40f', fontFamily: FONT,
    }));

    // Hint for right-click selling
    this.shopPanel.add(this.add.text(rightX, ph - px(12), '右键快速卖出', {
      fontSize: fs(10), color: '#555566', fontFamily: FONT,
    }));
  }

  private showSellConfirm(item: ItemInstance, shopData: { npcId: string; shopItems: string[]; type: string }): void {
    this.hideContextPopup();
    const base = getItemBase(item.baseId);
    const sellPrice = base ? base.sellPrice * item.quantity : 1;
    const popW = px(180), popH = px(60);
    const popX = (W - popW) / 2, popY = (H - popH) / 2;
    this.contextPopup = this.add.container(popX, popY).setDepth(PANEL_STYLE.depth.confirmDialog);
    this.contextPopup.add(this.add.rectangle(0, 0, popW, popH, 0x0a0a18, 0.95).setOrigin(0, 0).setStrokeStyle(Math.round(1 * DPR), PANEL_STYLE.border.color));
    this.contextPopup.add(this.add.text(popW / 2, px(8), `卖出 ${item.name} (${sellPrice}G)?`, {
      fontSize: fs(12), color: '#e0d8cc', fontFamily: FONT, wordWrap: { width: popW - px(16), useAdvancedWrap: true },
    }).setOrigin(0.5, 0));
    const yesBtn = this.add.text(popW / 2 - px(30), px(38), '[确定]', {
      fontSize: fs(13), color: '#27ae60', fontFamily: FONT,
    }).setInteractive({ useHandCursor: true });
    yesBtn.on('pointerdown', () => {
      const gold = this.zone.inventorySystem.sellItem(item.uid);
      this.player.gold += gold;
      audioManager.playSFX('click');
      this.hideContextPopup();
      this.reopenShop(shopData);
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
    this.mapPanel = this.add.container(panelX, panelY).setDepth(PANEL_STYLE.depth.panel);
    this.animatePanelOpen(this.mapPanel);
    this.mapPanel.add(this.createPanelBg(pw, ph));
    this.mapPanel.add(this.createPanelTitle(pw, '渊火'));
    this.mapPanel.add(this.createPanelCloseBtn(pw, () => this.toggleMap()));

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
  /** Active skill tree tab index (persists across panel reopens within session). */
  private skillTreeActiveTab = 0;
  /** Scroll offset per tab for skill tree overflow content. */
  private skillTreeScrollY: number[] = [];
  /** Wheel handler cleanup for skill tree panel. */
  private skillTreeWheelHandler: ((e: Phaser.Input.Pointer, _gos: Phaser.GameObjects.GameObject[], dx: number, dy: number) => void) | null = null;

  private skillTooltip: Phaser.GameObjects.Container | null = null;

  private toggleSkillTree(): void {
    if (this.skillPanel) {
      if (this.skillTreeWheelHandler) {
        this.input.off('wheel', this.skillTreeWheelHandler);
        this.skillTreeWheelHandler = null;
      }
      this.skillPanel.destroy(); this.skillPanel = null; this.skillTooltip?.destroy(); this.skillTooltip = null; return;
    }
    this.closeAllPanels();
    const TREE_NAMES: Record<string, string> = {
      combat_master: '战斗大师', guardian: '守护者', berserker: '狂战士',
      fire: '烈焰', frost: '冰霜', arcane: '奥术',
      assassination: '刺杀', archery: '箭术', traps: '陷阱',
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

    const pw = px(660), ph = px(520);
    const panelX = (W - pw) / 2, panelY = px(5);
    this.skillPanel = this.add.container(panelX, panelY).setDepth(PANEL_STYLE.depth.panel);
    this.animatePanelOpen(this.skillPanel);

    // Background with unified style
    this.skillPanel.add(this.createPanelBg(pw, ph));

    // Header bar with gradient
    const headerH = px(50);
    const headerBg = this.add.graphics();
    headerBg.fillStyle(0x14142e, 1);
    headerBg.fillRect(px(4), px(4), pw - px(8), headerH);
    headerBg.fillGradientStyle(0x8e44ad, 0x8e44ad, 0x14142e, 0x14142e, 0.15, 0.15, 0, 0);
    headerBg.fillRect(px(4), px(4), pw - px(8), headerH);
    this.skillPanel.add(headerBg);

    this.skillPanel.add(this.createPanelTitle(pw, '技 能 树'));
    const spColor = this.player.freeSkillPoints > 0 ? '#f1c40f' : '#555566';
    this.skillPanel.add(this.add.text(pw / 2, px(33), `${this.player.classData.name}  ·  剩余技能点: ${this.player.freeSkillPoints}`, {
      fontSize: fs(13), color: spColor, fontFamily: FONT,
    }).setOrigin(0.5, 0));

    // Close button
    this.skillPanel.add(this.createPanelCloseBtn(pw, () => this.toggleSkillTree()));

    // Gather trees
    const treeNames: string[] = [];
    const treeSkillsMap = new Map<string, typeof this.player.classData.skills>();
    for (const skill of this.player.classData.skills) {
      if (!treeSkillsMap.has(skill.tree)) { treeSkillsMap.set(skill.tree, []); treeNames.push(skill.tree); }
      treeSkillsMap.get(skill.tree)!.push(skill);
    }

    const treeCount = treeNames.length;
    if (this.skillTreeScrollY.length !== treeCount) {
      this.skillTreeScrollY = new Array(treeCount).fill(0);
    }
    if (this.skillTreeActiveTab >= treeCount) this.skillTreeActiveTab = 0;

    // === Tabs ===
    const tabH = px(28);
    const tabY = headerH + px(6);
    const tabMargin = px(10);
    const tabGap = px(4);
    const tabTotalW = pw - tabMargin * 2;
    const tabW = Math.floor((tabTotalW - tabGap * (treeCount - 1)) / treeCount);

    // Content area dimensions
    const contentTop = tabY + tabH + px(8);
    const contentH = ph - contentTop - px(24);
    const contentInnerW = pw - px(24);

    // Card dimensions — uniform for all trees
    const cardW = Math.min(contentInnerW - px(20), px(600));
    const cardH = px(72);
    const iconSize = px(42);
    const cardGap = px(12);

    // Scrollable content container
    const scrollContainer = this.add.container(0, 0);
    this.skillPanel.add(scrollContainer);

    // Clip mask for scroll area
    const clipMask = this.make.graphics({});
    clipMask.fillStyle(0xffffff);
    clipMask.fillRect(panelX + px(10), panelY + contentTop, pw - px(20), contentH);
    const mask = clipMask.createGeometryMask();
    scrollContainer.setMask(mask);

    // Render active tab content
    const renderTab = (tabIndex: number) => {
      scrollContainer.removeAll(true);
      this.skillTooltip?.destroy(); this.skillTooltip = null;

      const treeName = treeNames[tabIndex];
      const treeSkills = treeSkillsMap.get(treeName) ?? [];
      const treeColor = TREE_COLORS[treeName] ?? 0x888888;
      const sortedSkills = [...treeSkills].sort((a, b) => a.tier - b.tier);

      const scrollY = this.skillTreeScrollY[tabIndex] ?? 0;
      const totalContentH = sortedSkills.length * (cardH + cardGap) + px(30);
      const maxScrollY = Math.max(0, totalContentH - contentH);

      // Scrollbar
      if (totalContentH > contentH) {
        const sbX = pw - px(16);
        const sbTrackH = contentH - px(8);
        const sbTrackY = contentTop + px(4);
        const sbTrack = this.add.graphics();
        sbTrack.fillStyle(0x1a1a2e, 0.6);
        sbTrack.fillRoundedRect(sbX, sbTrackY, px(6), sbTrackH, px(3));
        scrollContainer.add(sbTrack);

        const thumbRatio = Math.min(1, contentH / totalContentH);
        const thumbH = Math.max(px(20), sbTrackH * thumbRatio);
        const thumbY = sbTrackY + (maxScrollY > 0 ? (scrollY / maxScrollY) * (sbTrackH - thumbH) : 0);
        const sbThumb = this.add.graphics();
        sbThumb.fillStyle(treeColor, 0.5);
        sbThumb.fillRoundedRect(sbX, thumbY, px(6), thumbH, px(3));
        scrollContainer.add(sbThumb);
      }

      // Prerequisite lines (behind cards)
      const skillStartY = contentTop + px(10);
      const cardStartX = (pw - cardW) / 2;

      for (let si = 0; si < sortedSkills.length - 1; si++) {
        const curLevel = this.player.getSkillLevel(sortedSkills[si].id);
        const isLearned = curLevel > 0;
        const lineGfx = this.add.graphics();
        const lineAlpha = isLearned ? 0.6 : 0.15;
        const lineColor = isLearned ? treeColor : 0x3a3a4e;

        const cx = cardStartX + cardW / 2;
        const curCardY = skillStartY + si * (cardH + cardGap) - scrollY;
        const nextCardY = skillStartY + (si + 1) * (cardH + cardGap) - scrollY;
        const startLineY = curCardY + cardH;
        const endLineY = nextCardY;

        if (endLineY > startLineY) {
          lineGfx.lineStyle(Math.round(2 * DPR), lineColor, lineAlpha);
          lineGfx.beginPath();
          lineGfx.moveTo(cx, startLineY);
          lineGfx.lineTo(cx, endLineY);
          lineGfx.strokePath();
          // Arrow head
          lineGfx.fillStyle(lineColor, lineAlpha);
          lineGfx.fillTriangle(cx, endLineY, cx - px(5), endLineY - px(7), cx + px(5), endLineY - px(7));
          // Glow for learned
          if (isLearned) {
            lineGfx.lineStyle(Math.round(4 * DPR), treeColor, 0.08);
            lineGfx.beginPath();
            lineGfx.moveTo(cx, startLineY);
            lineGfx.lineTo(cx, endLineY);
            lineGfx.strokePath();
          }
        }
        scrollContainer.add(lineGfx);
      }

      // Render skill cards
      sortedSkills.forEach((skill, si) => {
        const level = this.player.getSkillLevel(skill.id);
        const canLevel = this.player.freeSkillPoints > 0 && level < skill.maxLevel;
        const isLearned = level > 0;
        const isMaxed = level >= skill.maxLevel;
        const cardX = cardStartX;
        const cardY = skillStartY + si * (cardH + cardGap) - scrollY;
        const dmgColor = DMG_COLORS[skill.damageType] ?? 0xcccccc;
        const dmgColorHex = '#' + dmgColor.toString(16).padStart(6, '0');

        // === Card background with 4 distinct states ===
        const cardGfx = this.add.graphics();
        let cardBgColor: number;
        let borderColor: number;
        let borderAlpha: number;
        let borderWidth: number;

        if (isMaxed) {
          cardBgColor = 0x1e1a10;
          borderColor = 0xf1c40f;
          borderAlpha = 1;
          borderWidth = 2.5;
        } else if (isLearned) {
          cardBgColor = 0x161630;
          borderColor = treeColor;
          borderAlpha = 0.9;
          borderWidth = 1.5;
        } else if (canLevel) {
          cardBgColor = 0x101828;
          borderColor = 0x44dd44;
          borderAlpha = 0.8;
          borderWidth = 2;
        } else {
          cardBgColor = 0x0c0c18;
          borderColor = 0x2a2a3e;
          borderAlpha = 0.4;
          borderWidth = 1;
        }

        cardGfx.fillStyle(cardBgColor, 0.95);
        cardGfx.fillRoundedRect(cardX, cardY, cardW, cardH, px(5));
        cardGfx.lineStyle(Math.round(borderWidth * DPR), borderColor, borderAlpha);
        cardGfx.strokeRoundedRect(cardX, cardY, cardW, cardH, px(5));
        if (isMaxed) {
          cardGfx.lineStyle(Math.round(1 * DPR), 0xf1c40f, 0.12);
          cardGfx.strokeRoundedRect(cardX - px(2), cardY - px(2), cardW + px(4), cardH + px(4), px(6));
          cardGfx.lineStyle(Math.round(1 * DPR), 0xf1c40f, 0.06);
          cardGfx.strokeRoundedRect(cardX - px(4), cardY - px(4), cardW + px(8), cardH + px(8), px(7));
        }
        if (canLevel && !isLearned) {
          cardGfx.lineStyle(Math.round(1 * DPR), 0x44dd44, 0.06);
          cardGfx.strokeRoundedRect(cardX + px(1), cardY + px(1), cardW - px(2), cardH - px(2), px(4));
        }
        scrollContainer.add(cardGfx);

        // State badge (top-right)
        const badgeGfx = this.add.graphics();
        const badgeX = cardX + cardW - px(8);
        const badgeY = cardY + px(8);
        if (isMaxed) {
          badgeGfx.fillStyle(0xf1c40f, 0.9);
          badgeGfx.fillCircle(badgeX, badgeY, px(3));
          for (let i = 0; i < 5; i++) {
            const a = Phaser.Math.DegToRad(-90 + i * 72);
            badgeGfx.fillCircle(badgeX + Math.cos(a) * px(6), badgeY + Math.sin(a) * px(6), px(1.5));
          }
        } else if (isLearned) {
          badgeGfx.fillStyle(treeColor, 0.7);
          badgeGfx.fillCircle(badgeX, badgeY, px(4));
        } else if (canLevel) {
          badgeGfx.fillStyle(0x44dd44, 0.5);
          badgeGfx.fillCircle(badgeX, badgeY, px(5));
          badgeGfx.fillStyle(0x44dd44, 0.8);
          badgeGfx.fillCircle(badgeX, badgeY, px(3));
        }
        scrollContainer.add(badgeGfx);

        // === Icon area with gradient shading ===
        const iconX = cardX + px(8);
        const iconY = cardY + (cardH - iconSize) / 2;
        const iconGfx = this.add.graphics();
        iconGfx.fillStyle(0x080810, 0.95);
        iconGfx.fillRoundedRect(iconX, iconY, iconSize, iconSize, px(4));
        iconGfx.lineStyle(Math.round(1.5 * DPR), dmgColor, isLearned ? 0.7 : 0.25);
        iconGfx.strokeRoundedRect(iconX, iconY, iconSize, iconSize, px(4));
        if (isLearned) {
          iconGfx.fillStyle(dmgColor, 0.08);
          iconGfx.fillRoundedRect(iconX + px(1), iconY + px(1), iconSize / 2, iconSize / 2, px(3));
        }
        scrollContainer.add(iconGfx);

        // Skill icon texture
        const iconKey = `skill_icon_${skill.id}`;
        const iconCx = iconX + iconSize / 2;
        const iconCy = iconY + iconSize / 2;
        if (this.textures.exists(iconKey)) {
          const iconImg = this.add.image(iconCx, iconCy, iconKey)
            .setDisplaySize(iconSize - px(6), iconSize - px(6))
            .setAlpha(isLearned ? 1 : 0.3);
          scrollContainer.add(iconImg);
        } else {
          iconGfx.fillStyle(dmgColor, isLearned ? 0.5 : 0.12);
          const dR = px(8);
          iconGfx.fillTriangle(iconCx, iconCy - dR, iconCx + dR, iconCy, iconCx, iconCy + dR);
          iconGfx.fillTriangle(iconCx, iconCy - dR, iconCx - dR, iconCy, iconCx, iconCy + dR);
          scrollContainer.add(this.add.text(iconCx, iconCy, `T${skill.tier}`, {
            fontSize: fs(10), color: isLearned ? '#ccc' : '#444', fontFamily: FONT, fontStyle: 'bold',
          }).setOrigin(0.5));
        }

        // === Text area ===
        const textX = iconX + iconSize + px(10);

        // Skill name
        const nameColor = isMaxed ? '#f1c40f' : (isLearned ? '#e8e0d0' : (canLevel ? '#aabbcc' : '#555566'));
        scrollContainer.add(this.add.text(textX, cardY + px(8), skill.name, {
          fontSize: fs(14), color: nameColor, fontFamily: FONT, fontStyle: 'bold',
        }));

        // English name
        scrollContainer.add(this.add.text(textX, cardY + px(24), skill.nameEn, {
          fontSize: fs(9), color: '#444458', fontFamily: FONT,
        }));

        // Level pips
        const pipY = cardY + px(40);
        const pipR = px(3);
        const pipGap = px(8);
        const maxPips = Math.min(skill.maxLevel, 20);
        const pipStartX = textX;
        for (let p = 0; p < maxPips; p++) {
          const pipX = pipStartX + p * pipGap;
          if (pipX + pipR > cardX + cardW - px(30)) break;
          const pipGfx = this.add.graphics();
          if (p < level) {
            pipGfx.fillStyle(isMaxed ? 0xf1c40f : treeColor, 0.9);
            pipGfx.fillCircle(pipX, pipY, pipR);
          } else {
            pipGfx.fillStyle(0x2a2a3e, 0.6);
            pipGfx.fillCircle(pipX, pipY, pipR);
            pipGfx.lineStyle(Math.round(0.5 * DPR), 0x3a3a4e, 0.5);
            pipGfx.strokeCircle(pipX, pipY, pipR);
          }
          scrollContainer.add(pipGfx);
        }
        // Level text
        const maxVisiblePips = Math.min(maxPips, Math.floor((cardW - iconSize - px(60)) / pipGap));
        const lvColor = isMaxed ? '#f1c40f' : (isLearned ? '#aaaacc' : '#444458');
        scrollContainer.add(this.add.text(pipStartX + maxVisiblePips * pipGap + px(4), pipY - px(4), `${level}/${skill.maxLevel}`, {
          fontSize: fs(10), color: lvColor, fontFamily: FONT,
        }));

        // Stats row
        const scaledDmg = getSkillDamageMultiplier(skill, level);
        const scaledMana = getSkillManaCost(skill, level);
        const scaledCD = getSkillCooldown(skill, level);
        const statsY = cardY + px(54);
        let statsStr = '';
        if (skill.damageMultiplier > 0) statsStr += `${Math.round(scaledDmg * 100)}%`;
        statsStr += `  MP${scaledMana}  CD${(scaledCD / 1000).toFixed(1)}s`;
        const dmgName = DMG_NAMES[skill.damageType] ?? '';
        if (dmgName) statsStr += `  ${dmgName}`;
        scrollContainer.add(this.add.text(textX, statsY, statsStr, {
          fontSize: fs(10), color: '#666680', fontFamily: FONT,
        }));

        // Synergy badge
        if (skill.synergies && skill.synergies.length > 0 && isLearned) {
          let hasActiveSyn = false;
          for (const syn of skill.synergies) {
            if (this.player.getSkillLevel(syn.skillId) > 0) { hasActiveSyn = true; break; }
          }
          if (hasActiveSyn) {
            const synBadge = this.add.graphics();
            synBadge.fillStyle(0x7766cc, 0.6);
            synBadge.fillRoundedRect(cardX + cardW - px(40), cardY + cardH - px(18), px(35), px(14), px(3));
            scrollContainer.add(synBadge);
            scrollContainer.add(this.add.text(cardX + cardW - px(22), cardY + cardH - px(11), '协同', {
              fontSize: fs(8), color: '#aa99ee', fontFamily: FONT,
            }).setOrigin(0.5));
          }
        }

        // Hover area for tooltip
        const cardHit = this.add.rectangle(cardX + cardW / 2, cardY + cardH / 2, cardW, cardH, 0x000000, 0)
          .setInteractive({ useHandCursor: false });
        cardHit.on('pointerover', () => {
          cardGfx.clear();
          cardGfx.fillStyle(isLearned ? 0x1c1c3e : (canLevel ? 0x141e30 : 0x101020), 0.98);
          cardGfx.fillRoundedRect(cardX, cardY, cardW, cardH, px(5));
          cardGfx.lineStyle(Math.round(2 * DPR), borderColor, 1);
          cardGfx.strokeRoundedRect(cardX, cardY, cardW, cardH, px(5));
          this.showSkillTooltip(skill, panelX + cardX, panelY + cardY, cardW, DMG_NAMES, TREE_NAMES);
        });
        cardHit.on('pointerout', () => {
          cardGfx.clear();
          cardGfx.fillStyle(cardBgColor, 0.95);
          cardGfx.fillRoundedRect(cardX, cardY, cardW, cardH, px(5));
          cardGfx.lineStyle(Math.round(borderWidth * DPR), borderColor, borderAlpha);
          cardGfx.strokeRoundedRect(cardX, cardY, cardW, cardH, px(5));
          if (isMaxed) {
            cardGfx.lineStyle(Math.round(1 * DPR), 0xf1c40f, 0.12);
            cardGfx.strokeRoundedRect(cardX - px(2), cardY - px(2), cardW + px(4), cardH + px(4), px(6));
          }
          if (canLevel && !isLearned) {
            cardGfx.lineStyle(Math.round(1 * DPR), 0x44dd44, 0.06);
            cardGfx.strokeRoundedRect(cardX + px(1), cardY + px(1), cardW - px(2), cardH - px(2), px(4));
          }
          this.skillTooltip?.destroy(); this.skillTooltip = null;
        });
        scrollContainer.add(cardHit);

        // + Button
        if (canLevel) {
          const btnSize = px(22);
          const btnX = cardX + cardW - btnSize - px(6);
          const btnY = cardY + px(6);
          const btnBg = this.add.graphics();
          btnBg.fillStyle(0x1a3a1a, 0.9);
          btnBg.fillRoundedRect(btnX, btnY, btnSize, btnSize, px(4));
          btnBg.lineStyle(Math.round(1 * DPR), 0x27ae60, 0.8);
          btnBg.strokeRoundedRect(btnX, btnY, btnSize, btnSize, px(4));
          scrollContainer.add(btnBg);
          const btnText = this.add.text(btnX + btnSize / 2, btnY + btnSize / 2, '+', {
            fontSize: fs(15), color: '#27ae60', fontFamily: FONT, fontStyle: 'bold',
          }).setOrigin(0.5);
          scrollContainer.add(btnText);

          const hitArea = this.add.rectangle(btnX + btnSize / 2, btnY + btnSize / 2, btnSize, btnSize, 0x000000, 0)
            .setInteractive({ useHandCursor: true });
          hitArea.on('pointerover', () => {
            btnBg.clear();
            btnBg.fillStyle(0x225522, 1);
            btnBg.fillRoundedRect(btnX, btnY, btnSize, btnSize, px(4));
            btnBg.lineStyle(Math.round(2 * DPR), 0x44dd44, 1);
            btnBg.strokeRoundedRect(btnX, btnY, btnSize, btnSize, px(4));
            btnText.setColor('#44dd44');
          });
          hitArea.on('pointerout', () => {
            btnBg.clear();
            btnBg.fillStyle(0x1a3a1a, 0.9);
            btnBg.fillRoundedRect(btnX, btnY, btnSize, btnSize, px(4));
            btnBg.lineStyle(Math.round(1 * DPR), 0x27ae60, 0.8);
            btnBg.strokeRoundedRect(btnX, btnY, btnSize, btnSize, px(4));
            btnText.setColor('#27ae60');
          });
          hitArea.on('pointerdown', () => {
            if (this.player.freeSkillPoints > 0) {
              this.player.freeSkillPoints--;
              this.player.skillLevels.set(skill.id, level + 1);
              this.toggleSkillTree(); this.toggleSkillTree();
            }
          });
          scrollContainer.add(hitArea);
        }
      });
    };

    // Draw tab buttons
    const tabContainer = this.add.container(0, 0);
    this.skillPanel.add(tabContainer);

    const drawTabs = () => {
      tabContainer.removeAll(true);
      treeNames.forEach((treeName, ti) => {
        const treeColor = TREE_COLORS[treeName] ?? 0x888888;
        const treeColorHex = '#' + treeColor.toString(16).padStart(6, '0');
        const displayName = TREE_NAMES[treeName] ?? treeName;
        const isActive = ti === this.skillTreeActiveTab;
        const tx = tabMargin + ti * (tabW + tabGap);

        const tabGfx = this.add.graphics();
        if (isActive) {
          tabGfx.fillStyle(0x1a1a34, 1);
          tabGfx.fillRoundedRect(tx, tabY, tabW, tabH, { tl: px(5), tr: px(5), bl: 0, br: 0 });
          tabGfx.lineStyle(Math.round(2 * DPR), treeColor, 0.8);
          tabGfx.strokeRoundedRect(tx, tabY, tabW, tabH, { tl: px(5), tr: px(5), bl: 0, br: 0 });
          tabGfx.fillStyle(treeColor, 0.8);
          tabGfx.fillRect(tx + px(4), tabY + tabH - px(3), tabW - px(8), px(3));
        } else {
          tabGfx.fillStyle(0x0e0e1e, 0.7);
          tabGfx.fillRoundedRect(tx, tabY, tabW, tabH, { tl: px(5), tr: px(5), bl: 0, br: 0 });
          tabGfx.lineStyle(Math.round(1 * DPR), 0x2a2a3e, 0.5);
          tabGfx.strokeRoundedRect(tx, tabY, tabW, tabH, { tl: px(5), tr: px(5), bl: 0, br: 0 });
        }
        tabContainer.add(tabGfx);

        const treeSkills = treeSkillsMap.get(treeName) ?? [];
        const learnedCount = treeSkills.filter(s => this.player.getSkillLevel(s.id) > 0).length;

        const tabLabel = this.add.text(tx + tabW / 2, tabY + tabH / 2 - px(1), displayName, {
          fontSize: fs(12), color: isActive ? treeColorHex : '#555566', fontFamily: FONT, fontStyle: isActive ? 'bold' : 'normal',
        }).setOrigin(0.5);
        tabContainer.add(tabLabel);

        if (learnedCount > 0) {
          const badge = this.add.text(tx + tabW - px(8), tabY + px(4), `${learnedCount}`, {
            fontSize: fs(8), color: isActive ? treeColorHex : '#444', fontFamily: FONT,
          }).setOrigin(0.5, 0);
          tabContainer.add(badge);
        }

        const tabHit = this.add.rectangle(tx + tabW / 2, tabY + tabH / 2, tabW, tabH, 0x000000, 0)
          .setInteractive({ useHandCursor: true });
        tabHit.on('pointerdown', () => {
          if (this.skillTreeActiveTab !== ti) {
            this.skillTreeActiveTab = ti;
            drawTabs();
            renderTab(ti);
          }
        });
        tabContainer.add(tabHit);
      });
    };

    // Wheel scroll handler
    this.skillTreeWheelHandler = (_pointer: Phaser.Input.Pointer, _gos: Phaser.GameObjects.GameObject[], _dx: number, dy: number) => {
      if (!this.skillPanel) return;
      const tabIndex = this.skillTreeActiveTab;
      const treeSkills = treeSkillsMap.get(treeNames[tabIndex]) ?? [];
      const totalContentH = treeSkills.length * (cardH + cardGap) + px(30);
      const maxScrollY = Math.max(0, totalContentH - contentH);
      const scroll = this.skillTreeScrollY[tabIndex] ?? 0;
      const newScroll = Phaser.Math.Clamp(scroll + dy * 0.5, 0, maxScrollY);
      if (newScroll !== scroll) {
        this.skillTreeScrollY[tabIndex] = newScroll;
        renderTab(tabIndex);
      }
    };
    this.input.on('wheel', this.skillTreeWheelHandler);

    drawTabs();
    renderTab(this.skillTreeActiveTab);

    // Footer
    this.skillPanel.add(this.add.text(pw / 2, ph - px(14), '按 K 关闭  ·  悬停查看详情  ·  滚轮翻页', {
      fontSize: fs(10), color: '#3a3a4a', fontFamily: FONT,
    }).setOrigin(0.5));
  }

  /** Show skill tooltip — extracted helper method */
  private showSkillTooltip(
    skill: typeof this.player.classData.skills[0],
    cardWorldX: number, cardWorldY: number, cardW: number,
    DMG_NAMES: Record<string, string>, _TREE_NAMES: Record<string, string>,
  ): void {
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
    const tipPad = px(PANEL_STYLE.tooltip.padding);
    const wrapW = tipW - tipPad * 2;
    const tipText = lines.join('\n');

    const tipHeader = this.add.text(0, 0, `${skill.name} (${skill.nameEn})`, {
      fontSize: fs(PANEL_STYLE.tooltip.titleSize), color: '#f0e8d0', fontFamily: PANEL_STYLE.tooltip.font, fontStyle: 'bold',
      wordWrap: { width: wrapW, useAdvancedWrap: true },
    });
    const headerHeight = tipHeader.height;
    const headerBottom = tipPad + headerHeight + px(6);

    const textObj = this.add.text(tipPad, headerBottom, tipText, {
      fontSize: fs(PANEL_STYLE.tooltip.bodySize), color: '#ddd8cc', fontFamily: PANEL_STYLE.tooltip.font, lineSpacing: px(PANEL_STYLE.tooltip.lineSpacing),
      wordWrap: { width: wrapW, useAdvancedWrap: true },
    });

    const finalH = textObj.y + textObj.height + tipPad;

    let tipX = cardWorldX + cardW + px(8);
    if (tipX + tipW > W) tipX = cardWorldX - tipW - px(8);
    let tipY = cardWorldY;
    if (tipY + finalH > H) tipY = H - finalH - px(4);
    if (tipY < px(4)) tipY = px(4);

    this.skillTooltip = this.add.container(tipX, tipY).setDepth(PANEL_STYLE.depth.tooltip);
    const tipBg = this.add.rectangle(0, 0, tipW, finalH, PANEL_STYLE.tooltip.bg.color, PANEL_STYLE.tooltip.bg.alpha)
      .setOrigin(0, 0)
      .setStrokeStyle(PANEL_STYLE.tooltip.border.width * DPR, PANEL_STYLE.tooltip.border.color);
    this.skillTooltip.add(tipBg);
    tipHeader.setPosition(tipW / 2, tipPad).setOrigin(0.5, 0);
    this.skillTooltip.add(tipHeader);
    this.skillTooltip.add(textObj);
  }

  // --- Character Stats Panel (C) ---
  private toggleCharacter(): void {
    if (this.charPanel) { this.charPanel.destroy(); this.charPanel = null; return; }
    this.closeAllPanels();
    const pw = px(320), ph = px(440), panelX = (W - pw) / 2, panelY = px(20);
    this.charPanel = this.add.container(panelX, panelY).setDepth(PANEL_STYLE.depth.panel);
    this.animatePanelOpen(this.charPanel);
    this.charPanel.add(this.createPanelBg(pw, ph));
    this.charPanel.add(this.createPanelTitle(pw, `角色属性 - ${this.player.classData.name}`));
    this.charPanel.add(this.add.text(pw / 2, px(28), `Lv.${this.player.level}  剩余属性点: ${this.player.freeStatPoints}`, {
      fontSize: fs(13), color: '#f1c40f', fontFamily: FONT,
    }).setOrigin(0.5, 0));
    this.charPanel.add(this.createPanelCloseBtn(pw, () => this.toggleCharacter()));

    const statKeys: [string, keyof typeof this.player.stats, string][] = [
      ['力量 STR', 'str', '物理伤害/负重'],
      ['敏捷 DEX', 'dex', '闪避/暴击率/攻速'],
      ['体质 VIT', 'vit', '生命值/物理抗性'],
      ['智力 INT', 'int', '魔法伤害/法术抗性'],
      ['精神 SPI', 'spi', '法力值/法力回复'],
      ['幸运 LCK', 'lck', '掉宝率/暴击倍率'],
    ];
    const eqStatsRaw = this.zone.inventorySystem.getEquipmentStats();
    const statRowH = px(36);
    statKeys.forEach(([label, key, desc], i) => {
      const sy = px(50) + i * statRowH;
      const base = this.player.stats[key];
      const bonus = eqStatsRaw[key] ?? 0;
      this.charPanel!.add(this.add.text(px(14), sy, label, {
        fontSize: fs(13), color: '#e0d8cc', fontFamily: FONT,
      }));
      const valStr = bonus > 0 ? `${base} (+${bonus})` : `${base}`;
      this.charPanel!.add(this.add.text(px(140), sy, valStr, {
        fontSize: fs(13), color: bonus > 0 ? '#8be9fd' : '#fff', fontFamily: FONT, fontStyle: 'bold',
      }));
      this.charPanel!.add(this.add.text(px(14), sy + px(15), desc, {
        fontSize: fs(10), color: '#666', fontFamily: FONT,
      }));
      if (this.player.freeStatPoints > 0) {
        const plusBtn = this.add.text(pw - px(40), sy, '[+]', {
          fontSize: fs(13), color: '#27ae60', fontFamily: FONT,
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

    const dividerY = px(50) + statKeys.length * statRowH + px(2);
    const divider = this.add.rectangle(pw / 2, dividerY, pw - px(28), Math.round(1 * DPR), 0x333344);
    this.charPanel.add(divider);

    const dy = dividerY + px(8);
    const eqStats = this.zone.inventorySystem.getEquipmentStats();
    const effectiveDex = this.player.stats.dex + (eqStats['dex'] ?? 0);
    const effectiveLck = this.player.stats.lck + (eqStats['lck'] ?? 0);
    const critPct = (effectiveDex * 0.2 + effectiveLck * 0.5 + (eqStats['critRate'] ?? 0)).toFixed(1);
    const derived = [
      `HP: ${Math.ceil(this.player.hp)}/${this.player.maxHp}`,
      `MP: ${Math.ceil(this.player.mana)}/${this.player.maxMana}`,
      `攻击: ${Math.floor(this.player.baseDamage)}${eqStats['damage'] ? ` (+${eqStats['damage']})` : ''}${eqStats['damagePercent'] ? ` +${eqStats['damagePercent']}%` : ''}`,
      `防御: ${Math.floor(this.player.defense)}${eqStats['defense'] ? ` (+${eqStats['defense']})` : ''}`,
      `暴击率: ${critPct}%  暴击伤害: ${150 + (eqStats['critDamage'] ?? 0)}%`,
      `金币: ${this.player.gold}G`,
    ];
    derived.forEach((line, i) => {
      this.charPanel!.add(this.add.text(px(14), dy + i * px(18), line, {
        fontSize: fs(12), color: '#aaa', fontFamily: FONT,
      }));
    });
  }

  // --- Homestead Panel (H) ---
  /** Building icon drawing helpers keyed by building id */
  private static readonly BUILDING_ICONS: Record<string, (g: Phaser.GameObjects.Graphics, cx: number, cy: number, s: number, level: number, maxLevel: number) => void> = {
    herb_garden: (g, cx, cy, s, level, maxLevel) => {
      // Pot/garden — evolves from bare pot to lush garden
      const potW = s * 0.6, potH = s * 0.35;
      g.fillStyle(0x8B4513, 0.9);
      g.fillRoundedRect(cx - potW / 2, cy + s * 0.1, potW, potH, s * 0.06);
      g.fillStyle(0x553311, 0.8);
      g.fillRect(cx - potW * 0.55 / 2, cy + s * 0.06, potW * 0.55, s * 0.06);
      // Plants grow with level
      const plantCount = Math.max(1, Math.ceil(level / maxLevel * 5));
      for (let i = 0; i < plantCount; i++) {
        const px = cx - potW / 2 + (i + 0.5) * (potW / plantCount);
        const ph = s * 0.15 + (level / maxLevel) * s * 0.25;
        g.fillStyle(0x27ae60, 0.9);
        g.fillTriangle(px, cy + s * 0.1 - ph, px - s * 0.06, cy + s * 0.1, px + s * 0.06, cy + s * 0.1);
        if (level >= 3) {
          g.fillStyle(0xff6b6b, 0.8);
          g.fillCircle(px, cy + s * 0.1 - ph + s * 0.02, s * 0.03);
        }
      }
    },
    training_ground: (g, cx, cy, s, level, maxLevel) => {
      // Training dummy — evolves from simple post to armored dummy
      g.fillStyle(0x8B7355, 0.9);
      g.fillRect(cx - s * 0.04, cy - s * 0.1, s * 0.08, s * 0.4);
      // Cross arm
      g.fillRect(cx - s * 0.2, cy - s * 0.05, s * 0.4, s * 0.06);
      // Head
      const headR = s * (0.08 + level / maxLevel * 0.04);
      g.fillStyle(0xDEB887, 0.9);
      g.fillCircle(cx, cy - s * 0.2, headR);
      // Armor at higher levels
      if (level >= 2) {
        g.fillStyle(0x888888, 0.6);
        g.fillRect(cx - s * 0.1, cy - s * 0.1, s * 0.2, s * 0.2);
      }
      if (level >= 4) {
        g.lineStyle(2, 0xc0934a, 0.8);
        g.strokeCircle(cx, cy - s * 0.2, headR + s * 0.03);
      }
    },
    gem_workshop: (g, cx, cy, s, level, maxLevel) => {
      // Gem/anvil — evolves from simple anvil to glowing gem station
      g.fillStyle(0x555555, 0.9);
      g.fillRect(cx - s * 0.2, cy + s * 0.05, s * 0.4, s * 0.15);
      g.fillRect(cx - s * 0.15, cy - s * 0.1, s * 0.3, s * 0.15);
      // Gems on top, count increases with level
      const gemColors = [0xff4444, 0x4488ff, 0x44ff44, 0xffcc44];
      const gemCount = Math.min(gemColors.length, Math.max(1, Math.ceil(level / maxLevel * 4)));
      for (let i = 0; i < gemCount; i++) {
        const gx = cx - s * 0.12 + i * s * 0.08;
        g.fillStyle(gemColors[i], 0.9);
        const gr = s * 0.04;
        g.fillTriangle(gx, cy - s * 0.2, gx - gr, cy - s * 0.12, gx + gr, cy - s * 0.12);
        g.fillRect(gx - gr, cy - s * 0.12, gr * 2, gr);
      }
      // Sparkle at high levels
      if (level >= 3) {
        g.fillStyle(0xffffff, 0.6);
        g.fillCircle(cx + s * 0.15, cy - s * 0.22, s * 0.02);
        g.fillCircle(cx - s * 0.1, cy - s * 0.18, s * 0.015);
      }
    },
    pet_house: (g, cx, cy, s, level, maxLevel) => {
      // Pet house — evolves from small hut to grand shelter
      const houseW = s * (0.4 + level / maxLevel * 0.2);
      const houseH = s * (0.25 + level / maxLevel * 0.1);
      g.fillStyle(0x8B6914, 0.9);
      g.fillRect(cx - houseW / 2, cy, houseW, houseH);
      // Roof
      g.fillStyle(0xA0522D, 0.9);
      g.fillTriangle(cx, cy - s * 0.2, cx - houseW / 2 - s * 0.05, cy + s * 0.02, cx + houseW / 2 + s * 0.05, cy + s * 0.02);
      // Door
      g.fillStyle(0x553311, 0.9);
      g.fillRect(cx - s * 0.04, cy + houseH * 0.3, s * 0.08, houseH * 0.7);
      // Paw prints at higher levels
      if (level >= 2) {
        g.fillStyle(0xDEB887, 0.5);
        g.fillCircle(cx + houseW / 2 + s * 0.08, cy + houseH - s * 0.02, s * 0.025);
        g.fillCircle(cx + houseW / 2 + s * 0.12, cy + houseH - s * 0.06, s * 0.015);
      }
    },
    warehouse: (g, cx, cy, s, level, maxLevel) => {
      // Warehouse/crate — evolves from small crate to stacked warehouse
      const crateCount = Math.max(1, Math.ceil(level / maxLevel * 3));
      for (let i = 0; i < crateCount; i++) {
        const crateW = s * 0.25;
        const crateH = s * 0.2;
        const ox = (i - (crateCount - 1) / 2) * s * 0.15;
        const oy = -i * s * 0.12;
        g.fillStyle(0x8B7355, 0.9);
        g.fillRect(cx - crateW / 2 + ox, cy + oy, crateW, crateH);
        g.lineStyle(1, 0x664422, 0.7);
        g.strokeRect(cx - crateW / 2 + ox, cy + oy, crateW, crateH);
        // Cross planks
        g.lineStyle(1, 0x664422, 0.5);
        g.beginPath();
        g.moveTo(cx - crateW / 2 + ox, cy + oy);
        g.lineTo(cx + crateW / 2 + ox, cy + oy + crateH);
        g.moveTo(cx + crateW / 2 + ox, cy + oy);
        g.lineTo(cx - crateW / 2 + ox, cy + oy + crateH);
        g.strokePath();
      }
    },
    altar: (g, cx, cy, s, level, maxLevel) => {
      // Mystical altar — evolves with glow intensity
      g.fillStyle(0x555566, 0.9);
      g.fillRect(cx - s * 0.15, cy + s * 0.05, s * 0.3, s * 0.2);
      g.fillRect(cx - s * 0.2, cy + s * 0.2, s * 0.4, s * 0.06);
      // Crystal on top
      const crystalH = s * (0.15 + level / maxLevel * 0.1);
      g.fillStyle(0x8e44ad, 0.8);
      g.fillTriangle(cx, cy + s * 0.05 - crystalH, cx - s * 0.06, cy + s * 0.05, cx + s * 0.06, cy + s * 0.05);
      // Glow intensifies with level
      const glowAlpha = 0.1 + (level / maxLevel) * 0.3;
      g.fillStyle(0xbb77ff, glowAlpha);
      g.fillCircle(cx, cy - s * 0.05, s * 0.15 + level * s * 0.02);
      // Runes at higher levels
      if (level >= 2) {
        g.fillStyle(0xbb77ff, 0.5);
        g.fillCircle(cx - s * 0.12, cy + s * 0.15, s * 0.02);
        g.fillCircle(cx + s * 0.12, cy + s * 0.15, s * 0.02);
      }
    },
  };

  /** Pet rarity colors */
  private static readonly PET_RARITY_COLORS: Record<string, number> = {
    common: 0x888888,
    rare: 0x2471a3,
    epic: 0xd35400,
  };

  /** Pet bonus stat icon emojis (display text) */
  private static readonly PET_STAT_LABELS: Record<string, string> = {
    expBonus: '经验',
    damage: '攻击',
    magicFind: '掉宝',
    critRate: '暴击',
    hpRegen: '回血',
    attackSpeed: '攻速',
    defense: '防御',
    manaRegen: '回蓝',
  };

  private toggleHomestead(): void {
    if (this.homesteadPanel) { this.homesteadPanel.destroy(); this.homesteadPanel = null; return; }
    this.closeAllPanels();
    const pw = px(480), ph = px(520), panelX = (W - pw) / 2, panelY = px(10);
    this.homesteadPanel = this.add.container(panelX, panelY).setDepth(PANEL_STYLE.depth.panel);
    this.animatePanelOpen(this.homesteadPanel);
    this.homesteadPanel.add(this.createPanelBg(pw, ph));
    this.homesteadPanel.add(this.createPanelTitle(pw, '家 园'));
    this.homesteadPanel.add(this.createPanelCloseBtn(pw, () => this.toggleHomestead()));

    const hs = this.zone.homesteadSystem;
    const buildings = hs.getAllBuildings();

    // === Buildings Section ===
    const sectionHeaderY = px(38);
    const divider1 = this.add.graphics();
    divider1.fillStyle(0xc0934a, 0.3);
    divider1.fillRect(px(14), sectionHeaderY, pw - px(28), px(1));
    this.homesteadPanel.add(divider1);
    this.homesteadPanel.add(this.add.text(px(14), sectionHeaderY + px(4), '── 建筑 ──', {
      fontSize: fs(12), color: '#c0934a', fontFamily: FONT, fontStyle: 'bold',
    }));

    const buildingStartY = sectionHeaderY + px(22);
    const buildingH = px(56);
    const buildingGap = px(6);
    const iconAreaSize = px(42);

    buildings.forEach((b, i) => {
      const sy = buildingStartY + i * (buildingH + buildingGap);
      const lv = hs.getBuildingLevel(b.id);
      const maxed = lv >= b.maxLevel;
      const cost = maxed ? 0 : b.costPerLevel[lv]?.gold ?? 0;
      const canUpgrade = !maxed && this.player.gold >= cost;

      // Building card background
      const cardGfx = this.add.graphics();
      cardGfx.fillStyle(maxed ? 0x1a1810 : 0x0e0e20, 0.7);
      cardGfx.fillRoundedRect(px(10), sy, pw - px(20), buildingH, px(4));
      cardGfx.lineStyle(Math.round(1 * DPR), maxed ? 0xc0934a : 0x2a2a3e, maxed ? 0.5 : 0.3);
      cardGfx.strokeRoundedRect(px(10), sy, pw - px(20), buildingH, px(4));
      this.homesteadPanel!.add(cardGfx);

      // Building icon area
      const iconX = px(16);
      const iconY = sy + (buildingH - iconAreaSize) / 2;
      const iconGfx = this.add.graphics();
      iconGfx.fillStyle(0x0a0a18, 0.9);
      iconGfx.fillRoundedRect(iconX, iconY, iconAreaSize, iconAreaSize, px(4));
      iconGfx.lineStyle(Math.round(1 * DPR), maxed ? 0xc0934a : 0x333344, 0.5);
      iconGfx.strokeRoundedRect(iconX, iconY, iconAreaSize, iconAreaSize, px(4));
      this.homesteadPanel!.add(iconGfx);

      // Draw building icon (procedural illustration)
      const iconDrawer = UIScene.BUILDING_ICONS[b.id];
      if (iconDrawer) {
        const buildingIconGfx = this.add.graphics();
        iconDrawer(buildingIconGfx, iconX + iconAreaSize / 2, iconY + iconAreaSize / 2, iconAreaSize, lv, b.maxLevel);
        this.homesteadPanel!.add(buildingIconGfx);
      }

      // Text area
      const textX = iconX + iconAreaSize + px(10);
      this.homesteadPanel!.add(this.add.text(textX, sy + px(6), b.name, {
        fontSize: fs(13), color: maxed ? '#c0934a' : '#e0d8cc', fontFamily: FONT, fontStyle: 'bold',
      }));
      this.homesteadPanel!.add(this.add.text(textX, sy + px(22), b.description, {
        fontSize: fs(10), color: '#666680', fontFamily: FONT,
      }));

      // Level progress pips
      const pipY = sy + px(38);
      const pipR = px(3);
      const pipGapH = px(10);
      for (let p = 0; p < b.maxLevel; p++) {
        const pipX = textX + p * pipGapH;
        const pipGfxB = this.add.graphics();
        if (p < lv) {
          pipGfxB.fillStyle(maxed ? 0xc0934a : 0x27ae60, 0.9);
          pipGfxB.fillCircle(pipX, pipY, pipR);
        } else {
          pipGfxB.fillStyle(0x2a2a3e, 0.5);
          pipGfxB.fillCircle(pipX, pipY, pipR);
          pipGfxB.lineStyle(Math.round(0.5 * DPR), 0x3a3a4e, 0.4);
          pipGfxB.strokeCircle(pipX, pipY, pipR);
        }
        this.homesteadPanel!.add(pipGfxB);
      }
      // Level text
      this.homesteadPanel!.add(this.add.text(textX + b.maxLevel * pipGapH + px(4), pipY - px(4), `Lv.${lv}/${b.maxLevel}`, {
        fontSize: fs(9), color: maxed ? '#c0934a' : '#666680', fontFamily: FONT,
      }));

      // Upgrade button (global style: bright green if affordable, grey if not, gold badge if maxed)
      if (maxed) {
        const badgeGfx = this.add.graphics();
        const badgeX = pw - px(48);
        const badgeY = sy + buildingH / 2;
        badgeGfx.fillStyle(0xc0934a, 0.15);
        badgeGfx.fillRoundedRect(badgeX - px(20), badgeY - px(10), px(40), px(20), px(4));
        badgeGfx.lineStyle(Math.round(1 * DPR), 0xc0934a, 0.4);
        badgeGfx.strokeRoundedRect(badgeX - px(20), badgeY - px(10), px(40), px(20), px(4));
        this.homesteadPanel!.add(badgeGfx);
        this.homesteadPanel!.add(this.add.text(badgeX, badgeY, '已满级', {
          fontSize: fs(10), color: '#c0934a', fontFamily: FONT, fontStyle: 'bold',
        }).setOrigin(0.5));
      } else {
        const btnW = px(70);
        const btnH = px(24);
        const btnX = pw - px(24) - btnW;
        const btnY = sy + (buildingH - btnH) / 2;
        const btnGfx = this.add.graphics();
        const btnColor = canUpgrade ? 0x1a3a1a : 0x1a1a2e;
        const btnBorder = canUpgrade ? 0x27ae60 : 0x333344;
        btnGfx.fillStyle(btnColor, 0.9);
        btnGfx.fillRoundedRect(btnX, btnY, btnW, btnH, px(4));
        btnGfx.lineStyle(Math.round(1 * DPR), btnBorder, canUpgrade ? 0.8 : 0.4);
        btnGfx.strokeRoundedRect(btnX, btnY, btnW, btnH, px(4));
        this.homesteadPanel!.add(btnGfx);

        const btnText = this.add.text(btnX + btnW / 2, btnY + btnH / 2, `升级 ${cost}G`, {
          fontSize: fs(11), color: canUpgrade ? '#27ae60' : '#555566', fontFamily: FONT,
        }).setOrigin(0.5);
        this.homesteadPanel!.add(btnText);

        if (canUpgrade) {
          const hitArea = this.add.rectangle(btnX + btnW / 2, btnY + btnH / 2, btnW, btnH, 0x000000, 0)
            .setInteractive({ useHandCursor: true });
          hitArea.on('pointerover', () => {
            btnGfx.clear();
            btnGfx.fillStyle(0x225522, 1);
            btnGfx.fillRoundedRect(btnX, btnY, btnW, btnH, px(4));
            btnGfx.lineStyle(Math.round(2 * DPR), 0x44dd44, 1);
            btnGfx.strokeRoundedRect(btnX, btnY, btnW, btnH, px(4));
            btnText.setColor('#44dd44');
          });
          hitArea.on('pointerout', () => {
            btnGfx.clear();
            btnGfx.fillStyle(btnColor, 0.9);
            btnGfx.fillRoundedRect(btnX, btnY, btnW, btnH, px(4));
            btnGfx.lineStyle(Math.round(1 * DPR), btnBorder, 0.8);
            btnGfx.strokeRoundedRect(btnX, btnY, btnW, btnH, px(4));
            btnText.setColor('#27ae60');
          });
          hitArea.on('pointerdown', () => {
            const actualCost = hs.upgrade(b.id);
            this.player.gold -= actualCost;
            this.toggleHomestead(); this.toggleHomestead();
          });
          this.homesteadPanel!.add(hitArea);
        }
      }
    });

    // === Pets Section ===
    const petSectionY = buildingStartY + buildings.length * (buildingH + buildingGap) + px(8);
    const divider2 = this.add.graphics();
    divider2.fillStyle(0xc0934a, 0.3);
    divider2.fillRect(px(14), petSectionY, pw - px(28), px(1));
    this.homesteadPanel.add(divider2);

    const petCapacity = 1 + hs.getBuildingLevel('pet_house');
    this.homesteadPanel.add(this.add.text(px(14), petSectionY + px(4), `── 宠物 (${hs.pets.length} 只) ──`, {
      fontSize: fs(12), color: '#c0934a', fontFamily: FONT, fontStyle: 'bold',
    }));

    const pets = hs.pets;
    const petStartY = petSectionY + px(22);
    const petCardH = px(48);
    const petGap = px(6);
    const petIconSize = px(34);

    if (pets.length === 0) {
      this.homesteadPanel.add(this.add.text(pw / 2, petStartY + px(10), '暂无宠物\n击杀Boss·完成任务·稀有刷新可获得', {
        fontSize: fs(11), color: '#444458', fontFamily: FONT, align: 'center',
      }).setOrigin(0.5, 0));
    }

    pets.forEach((p, i) => {
      const pd = hs.getAllPets().find(d => d.id === p.petId);
      if (!pd) return;
      const isActive = hs.activePet === p.petId;
      const py = petStartY + i * (petCardH + petGap);
      const rarityColor = UIScene.PET_RARITY_COLORS[pd.rarity] ?? 0x888888;

      // Pet card bg
      const petCard = this.add.graphics();
      petCard.fillStyle(isActive ? 0x0e1e0e : 0x0e0e20, 0.7);
      petCard.fillRoundedRect(px(10), py, pw - px(20), petCardH, px(4));
      petCard.lineStyle(Math.round(1.5 * DPR), isActive ? 0x27ae60 : rarityColor, isActive ? 0.7 : 0.3);
      petCard.strokeRoundedRect(px(10), py, pw - px(20), petCardH, px(4));
      // Active highlight glow
      if (isActive) {
        petCard.lineStyle(Math.round(1 * DPR), 0x27ae60, 0.1);
        petCard.strokeRoundedRect(px(8), py - px(2), pw - px(16), petCardH + px(4), px(5));
      }
      this.homesteadPanel!.add(petCard);

      // Pet icon area with rarity-colored border
      const petIconX = px(16);
      const petIconY = py + (petCardH - petIconSize) / 2;
      const petIconGfx = this.add.graphics();
      petIconGfx.fillStyle(0x0a0a18, 0.9);
      petIconGfx.fillRoundedRect(petIconX, petIconY, petIconSize, petIconSize, px(4));
      petIconGfx.lineStyle(Math.round(1.5 * DPR), rarityColor, 0.7);
      petIconGfx.strokeRoundedRect(petIconX, petIconY, petIconSize, petIconSize, px(4));
      // Simple procedural pet icon based on petId
      const pcx = petIconX + petIconSize / 2;
      const pcy = petIconY + petIconSize / 2;
      petIconGfx.fillStyle(rarityColor, 0.5);
      // Body
      petIconGfx.fillCircle(pcx, pcy + petIconSize * 0.05, petIconSize * 0.25);
      // Head
      petIconGfx.fillCircle(pcx, pcy - petIconSize * 0.15, petIconSize * 0.18);
      // Eyes
      petIconGfx.fillStyle(0xffffff, 0.8);
      petIconGfx.fillCircle(pcx - petIconSize * 0.06, pcy - petIconSize * 0.18, petIconSize * 0.04);
      petIconGfx.fillCircle(pcx + petIconSize * 0.06, pcy - petIconSize * 0.18, petIconSize * 0.04);
      this.homesteadPanel!.add(petIconGfx);

      // Pet name + evolution suffix
      const evolvedStages = hs.getEvolutionStages();
      let displayName = pd.name;
      if (p.evolved > 0 && evolvedStages[p.evolved - 1]) {
        displayName += evolvedStages[p.evolved - 1].nameSuffix;
      }
      const rarityHex = '#' + rarityColor.toString(16).padStart(6, '0');

      const petTextX = petIconX + petIconSize + px(8);
      this.homesteadPanel!.add(this.add.text(petTextX, py + px(5), displayName, {
        fontSize: fs(12), color: isActive ? '#44dd88' : rarityHex, fontFamily: FONT, fontStyle: 'bold',
      }));

      // Bonus stat label
      const statLabel = UIScene.PET_STAT_LABELS[pd.bonusStat] ?? pd.bonusStat;
      const currentBonus = pd.bonusValue + pd.bonusPerLevel * p.level;
      this.homesteadPanel!.add(this.add.text(petTextX, py + px(19), `${statLabel} +${currentBonus.toFixed(1)}`, {
        fontSize: fs(9), color: '#888899', fontFamily: FONT,
      }));

      // Exp bar
      const expBarX = petTextX;
      const expBarY = py + px(33);
      const expBarW = px(100);
      const expBarH = px(5);
      const expThreshold = p.level * 20;
      const expRatio = expThreshold > 0 ? Math.min(1, p.exp / expThreshold) : 1;
      const expBarGfx = this.add.graphics();
      expBarGfx.fillStyle(0x1a1a2e, 1);
      expBarGfx.fillRoundedRect(expBarX, expBarY, expBarW, expBarH, px(2));
      if (expRatio > 0) {
        expBarGfx.fillStyle(isActive ? 0x27ae60 : rarityColor, 0.7);
        expBarGfx.fillRoundedRect(expBarX, expBarY, Math.round(expBarW * expRatio), expBarH, px(2));
      }
      expBarGfx.lineStyle(Math.round(0.5 * DPR), 0x333344, 0.5);
      expBarGfx.strokeRoundedRect(expBarX, expBarY, expBarW, expBarH, px(2));
      this.homesteadPanel!.add(expBarGfx);

      // Level text
      const isMaxLevel = p.level >= pd.maxLevel;
      this.homesteadPanel!.add(this.add.text(expBarX + expBarW + px(4), expBarY - px(2), isMaxLevel ? `Lv.${p.level} MAX` : `Lv.${p.level} (${p.exp}/${expThreshold})`, {
        fontSize: fs(8), color: isMaxLevel ? '#c0934a' : '#555566', fontFamily: FONT,
      }));

      // Active indicator badge
      if (isActive) {
        this.homesteadPanel!.add(this.add.text(pw - px(30), py + petCardH / 2, '✦', {
          fontSize: fs(14), color: '#27ae60', fontFamily: FONT,
        }).setOrigin(0.5));
      }
    });

    // Footer
    this.homesteadPanel.add(this.add.text(pw / 2, ph - px(14), '按 H 关闭  ·  建筑提供家园加成', {
      fontSize: fs(10), color: '#3a3a4a', fontFamily: FONT,
    }).setOrigin(0.5));
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
    // Use AllMaps for regular zones; fall back to scene's mapData for dungeons/sub-dungeons
    const mapData = AllMaps[(this.zone as any).currentMapId] ?? (this.zone as any).mapData;
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
    // Exits
    for (const exit of mapData.exits) {
      this.minimap.fillStyle(0x00e676);
      this.minimap.fillRect(exit.col * sx - 1.5 * DPR, exit.row * sy - 1.5 * DPR, 4 * DPR, 4 * DPR);
    }

    // Dungeon portal marker (red-orange diamond) — only in abyss_rift
    if ((this.zone as any).currentMapId === 'abyss_rift' && !(this.zone as any).isInDungeon) {
      const dpCol = 60, dpRow = 60; // DungeonSystem portal position
      this.minimap.fillStyle(0xFF6600, 0.9);
      this.minimap.fillCircle(dpCol * sx, dpRow * sy, 3 * DPR);
      this.minimap.lineStyle(1 * DPR, 0xFF3300, 0.8);
      this.minimap.strokeCircle(dpCol * sx, dpRow * sy, 4 * DPR);
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
              const qProg = this.zone.questSystem.progress.get(q.id);
              if (!qProg || (qProg.status === 'failed' && q.reacceptable)) { hasAvailable = true; break; }
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
          // Investigate clue markers (purple)
          if (obj.type === 'investigate_clue' && obj.location && progress.objectives[i].current < obj.required) {
            this.minimap.fillStyle(0x9b59b6, 0.6);
            this.minimap.fillRect(obj.location.col * sx - 1.5 * DPR, obj.location.row * sy - 1.5 * DPR, 3 * DPR, 3 * DPR);
          }
          // Escort destination marker (orange)
          if (obj.type === 'escort' && obj.location && progress.objectives[i].current < obj.required) {
            this.minimap.fillStyle(0xe67e22, 0.6);
            this.minimap.fillRect(obj.location.col * sx - 2 * DPR, obj.location.row * sy - 2 * DPR, 4 * DPR, 4 * DPR);
          }
        }

        // Defend target marker (red)
        if (quest.type === 'defend' && quest.defendTarget && progress.status === 'active') {
          const dt = quest.defendTarget;
          this.minimap.fillStyle(0xe74c3c, 0.5);
          this.minimap.fillCircle(dt.col * sx, dt.row * sy, 3 * DPR);
          this.minimap.lineStyle(1 * DPR, 0xe74c3c, 0.8);
          this.minimap.strokeCircle(dt.col * sx, dt.row * sy, 4 * DPR);
        }

        // Escort NPC start marker (orange)
        if (quest.type === 'escort' && quest.escortNpc && progress.status === 'active') {
          const en = quest.escortNpc;
          this.minimap.fillStyle(0xe67e22, 0.5);
          this.minimap.fillCircle(en.startCol * sx, en.startRow * sy, 2 * DPR);
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
      .setInteractive().setDepth(PANEL_STYLE.depth.backdrop);
    this.dialogueBackdrop.on('pointerdown', () => this.closeDialogue());

    const pw = px(360), ph = px(60) + data.actions.length * px(32) + px(30);
    const panelX = (W - pw) / 2, panelY = H / 2 - ph / 2;
    this.dialoguePanel = this.add.container(panelX, panelY).setDepth(PANEL_STYLE.depth.panel);
    this.animatePanelOpen(this.dialoguePanel);
    this.dialoguePanel.add(this.createPanelBg(pw, ph));

    // NPC name
    this.dialoguePanel.add(this.add.text(pw / 2, px(10), data.npcName, {
      fontSize: fs(PANEL_STYLE.header.fontSize), color: PANEL_STYLE.header.color, fontFamily: PANEL_STYLE.header.font, fontStyle: 'bold',
    }).setOrigin(0.5, 0));

    // Dialogue text
    this.dialoguePanel.add(this.add.text(pw / 2, px(32), data.dialogue, {
      fontSize: fs(14), color: '#e0d8cc', fontFamily: FONT, wordWrap: { width: pw - px(30), useAdvancedWrap: true },
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

  // --- Branching Dialogue Tree Panel ---
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  private openDialogueTree(data: any): void {
    const tree: DialogueTree = data.dialogueTree;
    const npcId: string = data.npcId;
    const npcName: string = data.npcName;
    const completedQuests: string[] = data.completedQuests ?? [];
    const questSystem = data.questSystem;
    const player = data.player;
    const homesteadSystem = data.homesteadSystem;
    const achievementSystem = data.achievementSystem;
    const turnedIn: string[] = data.turnedIn ?? [];

    // Initialize dialogue state for this NPC if not exists
    if (!this.dialogueTreeState[npcId]) {
      this.dialogueTreeState[npcId] = { visitedNodes: [], choicesMade: {} };
    }
    const state = this.dialogueTreeState[npcId];

    // Show turn-in text first if any quests were turned in
    if (turnedIn.length > 0) {
      this.renderDialogueTreeNode(tree, tree.nodes[tree.startNodeId], npcId, npcName, completedQuests, questSystem, player, homesteadSystem, achievementSystem, state, '感谢你完成了任务！接下来还有事情要做...');
    } else {
      this.renderDialogueTreeNode(tree, tree.nodes[tree.startNodeId], npcId, npcName, completedQuests, questSystem, player, homesteadSystem, achievementSystem, state);
    }
  }

  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  private renderDialogueTreeNode(
    tree: DialogueTree,
    node: DialogueNode,
    npcId: string,
    npcName: string,
    completedQuests: string[],
    questSystem: any,
    player: any,
    homesteadSystem: any,
    achievementSystem: any,
    state: { visitedNodes: string[]; choicesMade: Record<string, string> },
    prefixText?: string,
  ): void {
    this.closeDialogue();
    this.closeAllPanels();
    audioManager.playSFX('click');
    this.dialogueScrollY = 0;

    // Mark node as visited
    if (!state.visitedNodes.includes(node.id)) {
      state.visitedNodes.push(node.id);
    }

    // Full-screen transparent backdrop
    this.dialogueBackdrop = this.add.rectangle(W / 2, H / 2, W, H, 0x000000, 0.35)
      .setInteractive().setDepth(PANEL_STYLE.depth.backdrop);

    const pw = px(460), maxPh = px(500);
    const headerH = px(50);
    const footerH = px(24);

    // Determine display text
    const displayText = prefixText ? `${prefixText}\n\n${node.text}` : node.text;

    // Filter choices by prerequisites
    const visibleChoices: DialogueChoice[] = [];
    if (node.choices && node.choices.length > 0) {
      for (const choice of node.choices) {
        if (choice.prereqQuests && choice.prereqQuests.length > 0) {
          const meetsPrereqs = choice.prereqQuests.every(qid => completedQuests.includes(qid));
          if (!meetsPrereqs) continue;
        }
        // Hide choices only when quest is already handled AND the target is a dead-end
        if (choice.questTrigger && questSystem) {
          const prog = questSystem.progress.get(choice.questTrigger);
          if (prog && (prog.status === 'active' || prog.status === 'turned_in')) {
            const targetNode = tree.nodes[choice.nextNodeId];
            if (targetNode && targetNode.isEnd) continue;
          }
        }
        visibleChoices.push(choice);
      }
    }

    // Measure text height
    const textMeasure = this.add.text(0, 0, displayText, {
      fontSize: fs(13), color: '#e0d8cc', fontFamily: FONT,
      wordWrap: { width: pw - px(36), useAdvancedWrap: true },
      lineSpacing: px(3),
    });
    const textHeight = textMeasure.height;
    textMeasure.destroy();

    // Calculate button area height
    const btnH = px(30);
    const btnGap = px(6);
    const choicesToShow = visibleChoices.length > 0 ? visibleChoices : [];
    // When all choices were filtered out on a non-root branching node, offer a "go back" button
    const allChoicesFiltered = node.choices && node.choices.length > 0 && choicesToShow.length === 0 && !node.isEnd;
    const showBackToRoot = allChoicesFiltered && node.id !== tree.startNodeId;
    const hasEndBtn = node.isEnd || (choicesToShow.length === 0 && !node.nextNodeId && !showBackToRoot);
    const numBtns = choicesToShow.length + (hasEndBtn ? 1 : 0) + (showBackToRoot ? 1 : 0) + (node.nextNodeId && !node.isEnd && choicesToShow.length === 0 && !showBackToRoot ? 1 : 0);
    const btnAreaH = numBtns * (btnH + btnGap) + px(10);

    // Calculate panel height
    const contentH = textHeight + px(20);
    const maxScrollArea = maxPh - headerH - btnAreaH - footerH;
    const needsScroll = contentH > maxScrollArea;
    const scrollAreaH = needsScroll ? maxScrollArea : contentH;
    const ph = headerH + scrollAreaH + btnAreaH + footerH;
    const panelX = (W - pw) / 2, panelY = Math.max(px(20), (H - ph) / 2);

    this.dialoguePanel = this.add.container(panelX, panelY).setDepth(PANEL_STYLE.depth.panel);
    this.animatePanelOpen(this.dialoguePanel);

    // Background
    this.dialoguePanel.add(this.createPanelBg(pw, ph));

    // Header accent line
    const headerLine = this.add.rectangle(pw / 2, headerH, pw - px(20), Math.round(1 * DPR), 0x333344);
    this.dialoguePanel.add(headerLine);

    // NPC name
    this.dialoguePanel.add(this.add.text(pw / 2, px(12), npcName, {
      fontSize: fs(PANEL_STYLE.header.fontSize), color: PANEL_STYLE.header.color, fontFamily: PANEL_STYLE.header.font, fontStyle: 'bold',
    }).setOrigin(0.5, 0));

    // NPC type subtitle
    this.dialoguePanel.add(this.add.text(pw / 2, px(32), '─ 对话 ─', {
      fontSize: fs(11), color: '#555566', fontFamily: FONT,
    }).setOrigin(0.5, 0));

    // Scrollable text area with mask
    const textAreaY = headerH + px(4);
    const textAreaH = scrollAreaH;

    // Create a mask for scrolling text
    const maskGraphics = this.make.graphics({});
    maskGraphics.fillStyle(0xffffff);
    maskGraphics.fillRect(panelX + px(14), panelY + textAreaY, pw - px(28), textAreaH);
    const textMask = maskGraphics.createGeometryMask();

    const textContainer = this.add.container(px(18), textAreaY + px(4));
    textContainer.setMask(textMask);
    this.dialoguePanel.add(textContainer);

    const npcText = this.add.text(0, -this.dialogueScrollY, displayText, {
      fontSize: fs(13), color: '#e0d8cc', fontFamily: FONT,
      wordWrap: { width: pw - px(36), useAdvancedWrap: true },
      lineSpacing: px(3),
    });
    textContainer.add(npcText);

    // Scroll indicators
    if (needsScroll) {
      const scrollHint = this.add.text(pw - px(20), textAreaY + textAreaH - px(14), '▼', {
        fontSize: fs(12), color: '#c0934a', fontFamily: FONT,
      }).setOrigin(0.5);
      this.dialoguePanel.add(scrollHint);
      this.tweens.add({
        targets: scrollHint, alpha: 0.3, duration: 600,
        yoyo: true, repeat: -1, ease: 'Sine.easeInOut',
      });

      // Mouse wheel scrolling
      const maxScroll = Math.max(0, contentH - textAreaH + px(8));
      const scrollHandler = (_pointer: Phaser.Input.Pointer, _gx: unknown[], _gy: unknown, _gz: unknown, event: WheelEvent) => {
        this.dialogueScrollY = Math.max(0, Math.min(maxScroll, this.dialogueScrollY + (event.deltaY > 0 ? px(30) : -px(30))));
        npcText.y = -this.dialogueScrollY;
      };
      // eslint-disable-next-line @typescript-eslint/no-explicit-any
      this.input.on('wheel', scrollHandler as any);
      // Cleanup on panel destroy
      const originalDestroy = this.dialoguePanel.destroy.bind(this.dialoguePanel);
      this.dialoguePanel.destroy = (...args: Parameters<typeof originalDestroy>) => {
        // eslint-disable-next-line @typescript-eslint/no-explicit-any
        this.input.off('wheel', scrollHandler as any);
        maskGraphics.destroy();
        return originalDestroy(...args);
      };
    } else {
      maskGraphics.destroy();
    }

    // Choice buttons area
    const btnStartY = textAreaY + textAreaH + px(6);
    let btnIdx = 0;

    // Render visible choices
    for (const choice of choicesToShow) {
      const by = btnStartY + btnIdx * (btnH + btnGap);

      // Dim choices whose quest is already active (navigation-only)
      let questAlreadyActive = false;
      if (choice.questTrigger && questSystem) {
        const prog = questSystem.progress.get(choice.questTrigger);
        if (prog && (prog.status === 'active' || prog.status === 'turned_in')) questAlreadyActive = true;
      }
      const btnColor = questAlreadyActive ? 0x1a1a2a : 0x1a2a1a;
      const btnStroke = questAlreadyActive ? 0x556688 : 0x27ae60;
      const textColor = questAlreadyActive ? '#556688' : '#27ae60';
      const hoverBg = questAlreadyActive ? 0x222233 : 0x224422;
      const hoverText = questAlreadyActive ? '#7799bb' : '#44dd44';
      const labelText = questAlreadyActive ? `${choice.text}（进行中）` : choice.text;

      const btnBg = this.add.rectangle(pw / 2, by + btnH / 2, pw - px(40), btnH, btnColor)
        .setStrokeStyle(Math.round(1 * DPR), btnStroke).setInteractive({ useHandCursor: true });

      const btnText = this.add.text(pw / 2, by + btnH / 2, labelText, {
        fontSize: fs(13), color: textColor, fontFamily: FONT,
      }).setOrigin(0.5);

      // Truncate long choice text
      if (btnText.width > pw - px(56)) {
        btnText.setStyle({ wordWrap: { width: pw - px(56), useAdvancedWrap: true } });
      }

      btnBg.on('pointerover', () => {
        btnBg.setFillStyle(hoverBg);
        btnText.setColor(hoverText);
      });
      btnBg.on('pointerout', () => {
        btnBg.setFillStyle(btnColor);
        btnText.setColor(textColor);
      });
      btnBg.on('pointerdown', () => {
        // Record choice
        state.choicesMade[node.id] = choice.nextNodeId;

        // Apply quest trigger
        if (choice.questTrigger && questSystem) {
          const prog = questSystem.progress.get(choice.questTrigger);
          if (!prog || (prog.status === 'failed')) {
            questSystem.acceptQuest(choice.questTrigger);
          }
        }

        // Apply reward
        if (choice.reward) {
          if (choice.reward.gold && player) {
            player.gold += choice.reward.gold;
            EventBus.emit(GameEvents.LOG_MESSAGE, { text: `获得 ${choice.reward.gold} 金币`, type: 'loot' });
          }
          if (choice.reward.exp && player) {
            player.addExp(choice.reward.exp);
            EventBus.emit(GameEvents.LOG_MESSAGE, { text: `获得 ${choice.reward.exp} 经验`, type: 'loot' });
          }
          if (choice.reward.items && this.zone) {
            for (const itemId of choice.reward.items) {
              const item = this.zone.lootSystem.createItem(itemId, player?.level ?? 1, 'normal');
              if (item) {
                item.identified = true;
                this.zone.inventorySystem.addItem(item);
                EventBus.emit(GameEvents.LOG_MESSAGE, { text: `获得物品: ${item.name}`, type: 'loot' });
              }
            }
          }
        }

        // Navigate to next node
        const nextNode = tree.nodes[choice.nextNodeId];
        if (nextNode) {
          this.renderDialogueTreeNode(tree, nextNode, npcId, npcName, completedQuests, questSystem, player, homesteadSystem, achievementSystem, state);
        } else {
          this.closeDialogue();
        }
      });

      this.dialoguePanel!.add(btnBg);
      this.dialoguePanel!.add(btnText);
      btnIdx++;
    }

    // Auto-continue button (for nodes with nextNodeId but no choices)
    if (node.nextNodeId && !node.isEnd && choicesToShow.length === 0 && !showBackToRoot) {
      const by = btnStartY + btnIdx * (btnH + btnGap);
      const continueBg = this.add.rectangle(pw / 2, by + btnH / 2, pw - px(40), btnH, 0x1a1a2e)
        .setStrokeStyle(Math.round(1 * DPR), 0x5dade2).setInteractive({ useHandCursor: true });
      const continueText = this.add.text(pw / 2, by + btnH / 2, '继续', {
        fontSize: fs(13), color: '#5dade2', fontFamily: FONT,
      }).setOrigin(0.5);
      continueBg.on('pointerover', () => { continueBg.setFillStyle(0x1a2a3a); continueText.setColor('#88ccff'); });
      continueBg.on('pointerout', () => { continueBg.setFillStyle(0x1a1a2e); continueText.setColor('#5dade2'); });
      continueBg.on('pointerdown', () => {
        const nextNode = tree.nodes[node.nextNodeId!];
        if (nextNode) {
          this.renderDialogueTreeNode(tree, nextNode, npcId, npcName, completedQuests, questSystem, player, homesteadSystem, achievementSystem, state);
        } else {
          this.closeDialogue();
        }
      });
      this.dialoguePanel!.add(continueBg);
      this.dialoguePanel!.add(continueText);
      btnIdx++;
    }

    // Back-to-root button when all choices filtered on a non-root node
    if (showBackToRoot) {
      const by = btnStartY + btnIdx * (btnH + btnGap);
      const backBg = this.add.rectangle(pw / 2, by + btnH / 2, pw - px(40), btnH, 0x1a1a2e)
        .setStrokeStyle(Math.round(1 * DPR), 0xc0934a).setInteractive({ useHandCursor: true });
      const backText = this.add.text(pw / 2, by + btnH / 2, '← 返回', {
        fontSize: fs(13), color: '#c0934a', fontFamily: FONT,
      }).setOrigin(0.5);
      backBg.on('pointerover', () => { backBg.setFillStyle(0x2a2a1a); backText.setColor('#ddbb66'); });
      backBg.on('pointerout', () => { backBg.setFillStyle(0x1a1a2e); backText.setColor('#c0934a'); });
      backBg.on('pointerdown', () => {
        const rootNode = tree.nodes[tree.startNodeId];
        this.renderDialogueTreeNode(tree, rootNode, npcId, npcName, completedQuests, questSystem, player, homesteadSystem, achievementSystem, state);
      });
      this.dialoguePanel!.add(backBg);
      this.dialoguePanel!.add(backText);
      btnIdx++;
    }

    // End/leave button
    if (hasEndBtn) {
      const by = btnStartY + btnIdx * (btnH + btnGap);
      const leaveBg = this.add.rectangle(pw / 2, by + btnH / 2, pw - px(40), btnH, 0x1a1a1a)
        .setStrokeStyle(Math.round(1 * DPR), 0x666680).setInteractive({ useHandCursor: true });
      const leaveText = this.add.text(pw / 2, by + btnH / 2, '离开', {
        fontSize: fs(13), color: '#888', fontFamily: FONT,
      }).setOrigin(0.5);
      leaveBg.on('pointerover', () => { leaveBg.setFillStyle(0x222222); leaveText.setColor('#aaa'); });
      leaveBg.on('pointerout', () => { leaveBg.setFillStyle(0x1a1a1a); leaveText.setColor('#888'); });
      leaveBg.on('pointerdown', () => this.closeDialogue());
      this.dialoguePanel!.add(leaveBg);
      this.dialoguePanel!.add(leaveText);
    }

    // Footer hint
    this.dialoguePanel.add(this.add.text(pw / 2, ph - px(14), needsScroll ? '滚轮滚动查看更多' : '', {
      fontSize: fs(10), color: '#444455', fontFamily: FONT,
    }).setOrigin(0.5));

    // Backdrop click closes dialogue
    this.dialogueBackdrop!.on('pointerdown', () => this.closeDialogue());
  }

  /** Get dialogue tree state for saving. */
  getDialogueState(): Record<string, { visitedNodes: string[]; choicesMade: Record<string, string> }> {
    return this.dialogueTreeState;
  }

  /** Restore dialogue tree state from save data. */
  setDialogueState(state: Record<string, { visitedNodes: string[]; choicesMade: Record<string, string> }>): void {
    this.dialogueTreeState = state ?? {};
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
    this.questLogPanel = this.add.container(panelX, panelY).setDepth(PANEL_STYLE.depth.panel);
    this.animatePanelOpen(this.questLogPanel);

    // Background
    this.questLogPanel.add(this.createPanelBg(pw, ph));

    // Title
    this.questLogPanel.add(this.createPanelTitle(pw, '任务日志'));

    // Close button
    this.questLogPanel.add(this.createPanelCloseBtn(pw, () => this.toggleQuestLog()));

    // Tab buttons
    const tabY = px(38);
    const activeTab = this.add.rectangle(px(80), tabY, px(130), px(24), this.questLogTab === 'active' && !this.questLogLoreTab ? 0x1a2a3a : 0x111122)
      .setStrokeStyle(Math.round(1 * DPR), 0x2471a3).setInteractive({ useHandCursor: true });
    activeTab.on('pointerdown', () => { this.questLogTab = 'active'; this.questLogLoreTab = false; this.questLogPage = 0; this.questLogSelectedIndex = 0; this.refreshQuestLog(); });
    this.questLogPanel.add(activeTab);
    this.questLogPanel.add(this.add.text(px(80), tabY, '进行中', {
      fontSize: fs(14), color: this.questLogTab === 'active' && !this.questLogLoreTab ? '#5dade2' : '#666', fontFamily: FONT,
    }).setOrigin(0.5));

    const completedTab = this.add.rectangle(px(220), tabY, px(130), px(24), this.questLogTab === 'completed' && !this.questLogLoreTab ? 0x1a2a3a : 0x111122)
      .setStrokeStyle(Math.round(1 * DPR), 0x2471a3).setInteractive({ useHandCursor: true });
    completedTab.on('pointerdown', () => { this.questLogTab = 'completed'; this.questLogLoreTab = false; this.questLogPage = 0; this.questLogSelectedIndex = 0; this.refreshQuestLog(); });
    this.questLogPanel.add(completedTab);
    this.questLogPanel.add(this.add.text(px(220), tabY, '已完成', {
      fontSize: fs(14), color: this.questLogTab === 'completed' && !this.questLogLoreTab ? '#5dade2' : '#666', fontFamily: FONT,
    }).setOrigin(0.5));

    const loreTab = this.add.rectangle(px(360), tabY, px(130), px(24), this.questLogLoreTab ? 0x1a2a3a : 0x111122)
      .setStrokeStyle(Math.round(1 * DPR), 0xDAA520).setInteractive({ useHandCursor: true });
    loreTab.on('pointerdown', () => { this.questLogLoreTab = true; this.refreshQuestLog(); });
    this.questLogPanel.add(loreTab);
    this.questLogPanel.add(this.add.text(px(360), tabY, '传说', {
      fontSize: fs(14), color: this.questLogLoreTab ? '#DAA520' : '#666', fontFamily: FONT,
    }).setOrigin(0.5));

    // Render content based on active tab
    if (this.questLogLoreTab) {
      this.renderLoreLogContent();
    } else {
      this.renderQuestLogContent();
    }
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

      // Quest type label for new types
      const questTypeColors: Record<string, string> = {
        escort: '#e67e22', defend: '#e74c3c', investigate: '#9b59b6', craft: '#1abc9c',
      };
      const typeLabel = QUEST_TYPE_LABELS[entry.quest.type] ?? '';
      const hasTypeTag = ['escort', 'defend', 'investigate', 'craft'].includes(entry.quest.type);
      const typeSuffix = hasTypeTag ? ` [${typeLabel}]` : '';

      const nameColor = this.questLogTab === 'completed' ? '#555' : (isSelected ? '#e0d8cc' : '#aaa');
      this.questLogPanel!.add(this.add.text(px(36), y + px(5), `${entry.quest.name} Lv.${entry.quest.level}`, {
        fontSize: fs(12), color: nameColor, fontFamily: FONT,
      }));

      if (hasTypeTag) {
        const typeColor = questTypeColors[entry.quest.type] ?? '#aaa';
        const nameWidth = px(36) + this.add.text(0, 0, `${entry.quest.name} Lv.${entry.quest.level}`, {
          fontSize: fs(12), fontFamily: FONT,
        }).setVisible(false).width;
        this.questLogPanel!.add(this.add.text(nameWidth + px(4), y + px(5), `[${typeLabel}]`, {
          fontSize: fs(10), color: typeColor, fontFamily: FONT, fontStyle: 'bold',
        }));
      }
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
    const questTypeLabel = QUEST_TYPE_LABELS[selected.quest.type] ?? '';
    const typeDisplay = ['escort', 'defend', 'investigate', 'craft'].includes(selected.quest.type)
      ? `  |  类型: ${questTypeLabel}` : '';
    this.questLogPanel.add(this.add.text(detailX + px(5), dy, `${catText}  |  Lv.${selected.quest.level}  |  ${zoneNames[selected.quest.zone] ?? selected.quest.zone}${typeDisplay}`, {
      fontSize: fs(12), color: catColor, fontFamily: FONT,
    }));
    dy += px(20);

    // Description
    this.questLogPanel.add(this.add.text(detailX + px(5), dy, selected.quest.description, {
      fontSize: fs(13), color: '#bbb', fontFamily: FONT, wordWrap: { width: detailW - px(20), useAdvancedWrap: true },
    }));
    dy += px(40);

    // Type-specific summary for investigate, defend, craft
    if (selected.quest.type === 'investigate') {
      const totalClues = selected.quest.objectives.filter(o => o.type === 'investigate_clue').length;
      const foundClues = selected.quest.objectives
        .map((o, i) => o.type === 'investigate_clue' && (selected.progress.objectives[i]?.current ?? 0) >= o.required ? 1 : 0)
        .reduce((a: number, b: number) => a + b, 0);
      this.questLogPanel.add(this.add.text(detailX + px(5), dy, `线索 ${foundClues}/${totalClues}`, {
        fontSize: fs(13), color: '#9b59b6', fontFamily: FONT, fontStyle: 'bold',
      }));
      dy += px(18);
    } else if (selected.quest.type === 'defend' && selected.quest.defendTarget) {
      const waveObj = selected.quest.objectives.find(o => o.type === 'defend_wave');
      const waveIdx = waveObj ? selected.quest.objectives.indexOf(waveObj) : -1;
      const curWave = waveIdx >= 0 ? (selected.progress.objectives[waveIdx]?.current ?? 0) : 0;
      this.questLogPanel.add(this.add.text(detailX + px(5), dy, `浪潮 ${curWave}/${selected.quest.defendTarget.totalWaves}`, {
        fontSize: fs(13), color: '#e74c3c', fontFamily: FONT, fontStyle: 'bold',
      }));
      dy += px(18);
    } else if (selected.quest.type === 'craft') {
      // Show current craft phase
      const qs = this.zone?.questSystem;
      if (qs) {
        const phaseLabel = qs.getCraftPhaseLabel(selected.quest, selected.progress);
        this.questLogPanel.add(this.add.text(detailX + px(5), dy, `当前阶段: ${phaseLabel}`, {
          fontSize: fs(13), color: '#1abc9c', fontFamily: FONT, fontStyle: 'bold',
        }));
        dy += px(18);
      }
    }

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
    const tipW = px(220);
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
      // Base item description
      if (base.description) {
        lines.push({ text: base.description, color: '#8a8a7a', size: 11 });
      }
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
    // Gem socketing effect (when hovering a gem item)
    if (base?.type === 'gem') {
      const gemInfo = GEM_STAT_MAP[item.baseId];
      if (gemInfo) {
        const disp = STAT_DISPLAY[gemInfo.stat];
        const label = disp ? disp.label : gemInfo.stat;
        const suffix = disp?.isPercent ? '%' : '';
        lines.push({ text: `镶嵌效果: +${gemInfo.value}${suffix} ${label}`, color: '#8be9fd', size: 12 });
      }
    }
    // Socketed gems
    if (item.sockets && item.sockets.length > 0) {
      lines.push({ text: '── 宝石 ──', color: '#8be9fd', size: 11 });
      for (const gem of item.sockets) {
        const disp = STAT_DISPLAY[gem.stat];
        const label = disp ? disp.label : gem.stat;
        const suffix = disp?.isPercent ? '%' : '';
        lines.push({ text: `◆ ${gem.name}: +${gem.value}${suffix} ${label}`, color: '#8be9fd', size: 11 });
      }
    }
    // Socket count (if base has sockets)
    if (base && 'sockets' in base) {
      const maxSock = (base as WeaponBase | ArmorBase).sockets;
      if (maxSock > 0) {
        const filled = item.sockets?.length ?? 0;
        lines.push({ text: `插槽: ${filled}/${maxSock}`, color: '#666', size: 11 });
      }
    }

    // ── Set bonus section ──
    if (item.setId) {
      const allSets = [...SetDefinitions, ...DUNGEON_EXCLUSIVE_SETS];
      const setDef = allSets.find(s => s.id === item.setId);
      if (setDef) {
        const equippedCount = this.zone.inventorySystem.getEquippedSetPieceCount(setDef.id);
        const totalPieces = setDef.pieces.length;
        lines.push({ text: '', color: '#333', size: 4 }); // spacer
        lines.push({ text: `${setDef.name} (${equippedCount}/${totalPieces})`, color: '#2ecc71', size: 13 });
        for (const bonus of setDef.bonuses) {
          const isActive = equippedCount >= bonus.count;
          const prefix = isActive ? '✓' : '○';
          const color = isActive ? '#2ecc71' : '#555550';
          lines.push({ text: `${prefix} (${bonus.count}) ${bonus.description}`, color, size: 11 });
        }
      }
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
    if (tx < px(4)) tx = px(4);

    const qualityBorderColors: Record<string, number> = {
      normal: 0x555555, magic: 0x5dade2, rare: 0xf1c40f, legendary: 0xe67e22, set: 0x2ecc71,
    };
    const borderColor = qualityBorderColors[item.quality] ?? PANEL_STYLE.tooltip.border.color;
    this.tooltipContainer = this.add.container(tx, ty).setDepth(PANEL_STYLE.depth.tooltip);
    this.tooltipContainer.add(
      this.add.rectangle(0, 0, tipW, tipH, PANEL_STYLE.tooltip.bg.color, PANEL_STYLE.tooltip.bg.alpha)
        .setOrigin(0, 0).setStrokeStyle(PANEL_STYLE.tooltip.border.width * DPR, borderColor)
    );
    let ly = px(6);
    for (const line of lines) {
      this.tooltipContainer.add(this.add.text(px(8), ly, line.text, {
        fontSize: fs(line.size), color: line.color, fontFamily: PANEL_STYLE.tooltip.font, wordWrap: { width: tipW - px(16), useAdvancedWrap: true },
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

    this.contextPopup = this.add.container(popX, popY).setDepth(PANEL_STYLE.depth.contextMenu);
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
    this.contextPopup = this.add.container(popX, popY).setDepth(PANEL_STYLE.depth.confirmDialog);
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

  // --- Socket Panel ---
  private openSocketPanel(equipSlot: EquipSlot): void {
    // Close existing socket panel
    if (this.socketPanel) { this.socketPanel.destroy(); this.socketPanel = null; this.socketPanelSlot = null; }

    const equipItem = this.zone.inventorySystem.equipment[equipSlot];
    if (!equipItem) return;

    const base = getItemBase(equipItem.baseId);
    if (!base) return;

    const maxSockets = this.zone.inventorySystem.getMaxSockets(equipSlot);
    if (maxSockets === 0) return;

    this.socketPanelSlot = equipSlot;
    audioManager.playSFX('click');

    const pw = px(360), ph = px(380), panelX = (W - pw) / 2, panelY = px(60);
    this.socketPanel = this.add.container(panelX, panelY).setDepth(PANEL_STYLE.depth.subPanel);
    this.animatePanelOpen(this.socketPanel);

    // Background
    this.socketPanel.add(this.createPanelBg(pw, ph));

    // Title
    this.socketPanel.add(this.createPanelTitle(pw, '宝石镶嵌'));

    // Close button
    this.socketPanel.add(this.createPanelCloseBtn(pw, () => {
      if (this.socketPanel) { this.socketPanel.destroy(); this.socketPanel = null; this.socketPanelSlot = null; }
    }));

    // Item name
    const qualityColors: Record<string, string> = {
      normal: '#cccccc', magic: '#5dade2', rare: '#f1c40f', legendary: '#e67e22', set: '#2ecc71',
    };
    this.socketPanel.add(this.add.text(pw / 2, px(36), equipItem.name, {
      fontSize: fs(15), color: qualityColors[equipItem.quality] ?? '#ccc', fontFamily: FONT, fontStyle: 'bold',
    }).setOrigin(0.5, 0));

    // Socket slots display
    const sockStartY = px(62);
    const sockSize = px(48);
    const sockGap = px(12);
    const totalW = maxSockets * sockSize + (maxSockets - 1) * sockGap;
    const sockStartX = (pw - totalW) / 2;

    this.socketPanel.add(this.add.text(pw / 2, sockStartY, `插槽 (${equipItem.sockets.length}/${maxSockets})`, {
      fontSize: fs(13), color: '#aaa', fontFamily: FONT,
    }).setOrigin(0.5, 0));

    const GEM_COLORS: Record<string, number> = {
      g_ruby: 0xcc3333, g_sapphire: 0x3366cc, g_emerald: 0x33aa33, g_topaz: 0xccaa33, g_diamond: 0xaaddff,
    };
    const GEM_LABELS: Record<string, string> = {
      g_ruby: '红', g_sapphire: '蓝', g_emerald: '绿', g_topaz: '黄', g_diamond: '钻',
    };

    for (let i = 0; i < maxSockets; i++) {
      const sx = sockStartX + i * (sockSize + sockGap);
      const sy = sockStartY + px(22);
      const filled = i < equipItem.sockets.length;
      const gem = filled ? equipItem.sockets[i] : null;

      // Slot background
      const gemIconKey = gem ? gem.gemId.replace(/_\d+$/, '') : '';
      const slotColor = gem ? (GEM_COLORS[gemIconKey] ?? 0x333366) : 0x1a1a2e;
      const slotBg = this.add.rectangle(sx + sockSize / 2, sy + sockSize / 2, sockSize, sockSize, slotColor, gem ? 0.7 : 0.4)
        .setStrokeStyle(Math.round(2 * DPR), gem ? 0x8be9fd : 0x444466);
      this.socketPanel!.add(slotBg);

      if (gem) {
        // Gem icon text
        const gemLabel = GEM_LABELS[gemIconKey] ?? '◆';
        this.socketPanel!.add(this.add.text(sx + sockSize / 2, sy + sockSize / 2 - px(4), gemLabel, {
          fontSize: fs(18), color: '#fff', fontFamily: FONT, fontStyle: 'bold',
        }).setOrigin(0.5));

        // Tier indicator
        this.socketPanel!.add(this.add.text(sx + sockSize / 2, sy + sockSize / 2 + px(12), `T${gem.tier}`, {
          fontSize: fs(10), color: '#aaa', fontFamily: FONT,
        }).setOrigin(0.5));

        // Gem name below slot
        const gemDisp = STAT_DISPLAY[gem.stat];
        const gemStatLabel = gemDisp ? gemDisp.label : gem.stat;
        const gemSuffix = gemDisp?.isPercent ? '%' : '';
        this.socketPanel!.add(this.add.text(sx + sockSize / 2, sy + sockSize + px(4), `${gem.name} (+${gem.value}${gemSuffix}${gemStatLabel})`, {
          fontSize: fs(10), color: '#8be9fd', fontFamily: FONT,
        }).setOrigin(0.5, 0));

        // Remove button
        const removeBtn = this.add.text(sx + sockSize / 2, sy + sockSize + px(18), '[取出]', {
          fontSize: fs(11), color: '#e74c3c', fontFamily: FONT,
        }).setOrigin(0.5, 0).setInteractive({ useHandCursor: true });
        const socketIndex = i;
        removeBtn.on('pointerdown', () => {
          this.zone.inventorySystem.unsocketGem(equipSlot, socketIndex);
          this.zone.invalidateEquipStats();
          this.refreshSocketPanel(equipSlot);
          this.refreshInventory();
        });
        this.socketPanel!.add(removeBtn);
      } else {
        // Empty slot indicator
        this.socketPanel!.add(this.add.text(sx + sockSize / 2, sy + sockSize / 2, '◇', {
          fontSize: fs(20), color: '#333366', fontFamily: FONT,
        }).setOrigin(0.5));

        // Empty label below
        this.socketPanel!.add(this.add.text(sx + sockSize / 2, sy + sockSize + px(4), '空插槽', {
          fontSize: fs(10), color: '#555', fontFamily: FONT,
        }).setOrigin(0.5, 0));
      }
    }

    // Divider
    const divY = sockStartY + px(22) + sockSize + px(40);
    this.socketPanel.add(this.add.rectangle(pw / 2, divY, pw - px(20), Math.round(1 * DPR), 0x333344));

    // Available gems from inventory
    this.socketPanel.add(this.add.text(pw / 2, divY + px(8), '背包中的宝石', {
      fontSize: fs(14), color: '#c0934a', fontFamily: FONT,
    }).setOrigin(0.5, 0));

    const gemsInInventory = this.zone.inventorySystem.inventory.filter(item => {
      const b = getItemBase(item.baseId);
      return b && b.type === 'gem';
    });

    const gemGridY = divY + px(30);
    const gemSlotSize = px(40);
    const gemGap = px(6);
    const gemCols = 7;
    const hasEmptySlots = equipItem.sockets.length < maxSockets;

    if (gemsInInventory.length === 0) {
      this.socketPanel.add(this.add.text(pw / 2, gemGridY + px(20), '没有宝石', {
        fontSize: fs(13), color: '#555', fontFamily: FONT,
      }).setOrigin(0.5));
    } else {
      gemsInInventory.forEach((gemItem, i) => {
        const gx = px(14) + (i % gemCols) * (gemSlotSize + gemGap);
        const gy = gemGridY + Math.floor(i / gemCols) * (gemSlotSize + gemGap + px(2));

        const gemIconKey = gemItem.baseId.replace(/_\d+$/, '');
        const gemColor = GEM_COLORS[gemIconKey] ?? 0x333366;
        const gemBg = this.add.rectangle(gx + gemSlotSize / 2, gy + gemSlotSize / 2, gemSlotSize, gemSlotSize, gemColor, 0.5)
          .setStrokeStyle(Math.round(1 * DPR), hasEmptySlots ? 0x8be9fd : 0x444466)
          .setInteractive({ useHandCursor: hasEmptySlots });
        this.socketPanel!.add(gemBg);

        // Gem label
        const label = GEM_LABELS[gemIconKey] ?? '◆';
        this.socketPanel!.add(this.add.text(gx + gemSlotSize / 2, gy + gemSlotSize / 2 - px(2), label, {
          fontSize: fs(14), color: '#fff', fontFamily: FONT, fontStyle: 'bold',
        }).setOrigin(0.5));

        // Quantity
        if (gemItem.quantity > 1) {
          this.socketPanel!.add(this.add.text(gx + gemSlotSize - px(2), gy + gemSlotSize - px(2), `${gemItem.quantity}`, {
            fontSize: fs(10), color: '#ffd700', fontFamily: FONT,
          }).setOrigin(1, 1));
        }

        // Stat label below gem slot
        const gInfo = GEM_STAT_MAP[gemItem.baseId];
        if (gInfo) {
          const gDisp = STAT_DISPLAY[gInfo.stat];
          const gLabel = gDisp ? gDisp.label : gInfo.stat;
          const gSuffix = gDisp?.isPercent ? '%' : '';
          this.socketPanel!.add(this.add.text(gx + gemSlotSize / 2, gy + gemSlotSize + px(1), `+${gInfo.value}${gSuffix}${gLabel}`, {
            fontSize: fs(9), color: '#8be9fd', fontFamily: FONT,
          }).setOrigin(0.5, 0));
        }

        // Tooltip on hover
        gemBg.on('pointerover', (pointer: Phaser.Input.Pointer) => {
          this.showItemTooltip(gemItem, pointer.x, pointer.y);
        });
        gemBg.on('pointerout', () => this.hideItemTooltip());

        // Click to socket
        if (hasEmptySlots) {
          gemBg.on('pointerdown', () => {
            this.hideItemTooltip();
            const success = this.zone.inventorySystem.socketGem(equipSlot, gemItem.uid);
            if (success) {
              this.zone.invalidateEquipStats();
              this.refreshSocketPanel(equipSlot);
              this.refreshInventory();
            }
          });
        }
      });
    }

    // Unequip button at bottom
    const unequipBtn = this.add.text(pw / 2, ph - px(22), '[卸下装备]', {
      fontSize: fs(13), color: '#e74c3c', fontFamily: FONT,
    }).setOrigin(0.5, 0).setInteractive({ useHandCursor: true });
    unequipBtn.on('pointerdown', () => {
      if (this.socketPanel) { this.socketPanel.destroy(); this.socketPanel = null; this.socketPanelSlot = null; }
      this.zone.inventorySystem.unequip(equipSlot);
      this.refreshInventory();
    });
    this.socketPanel.add(unequipBtn);
  }

  /** Refresh the socket panel for the current slot. */
  private refreshSocketPanel(equipSlot: EquipSlot): void {
    if (this.socketPanel) {
      this.socketPanel.destroy();
      this.socketPanel = null;
      this.socketPanelSlot = null;
      this.openSocketPanel(equipSlot);
    }
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
    const pw = px(500), ph = px(520), panelX = (W - pw) / 2, panelY = px(10);
    this.companionPanel = this.add.container(panelX, panelY).setDepth(PANEL_STYLE.depth.panel);
    this.animatePanelOpen(this.companionPanel);

    // Background
    this.companionPanel.add(this.createPanelBg(pw, ph));

    // Title
    this.companionPanel.add(this.createPanelTitle(pw, '伙伴系统'));

    // Close button
    this.companionPanel.add(this.createPanelCloseBtn(pw, () => this.toggleCompanion()));

    const mercSys = this.zone?.mercenarySystem;
    if (!mercSys) return;

    const merc = mercSys.getMercenary();

    // Mercenary section header
    this.companionPanel.add(this.add.text(px(14), px(36), '─ 佣兵 ─', {
      fontSize: fs(13), color: '#c0934a', fontFamily: FONT,
    }));

    if (!merc) {
      // No mercenary — compact hire hint
      this.companionPanel.add(this.add.text(px(14), px(56), '在营地NPC处雇佣佣兵 (详见佣兵面板)', {
        fontSize: fs(12), color: '#888', fontFamily: FONT,
      }));
    } else {
      // Has mercenary — show compact info
      const def = MERCENARY_DEFS[merc.type];
      const typeNames: Record<string, string> = {
        tank: '坦克', melee: '近战输出', ranged: '远程输出', healer: '治疗', mage: '法师',
      };
      const statusText = merc.alive
        ? `${def.name} (${typeNames[merc.type]}) Lv.${merc.level}  HP:${Math.ceil(merc.hp)}/${merc.maxHp}`
        : `${def.name} (${typeNames[merc.type]}) Lv.${merc.level}  [阵亡]`;
      this.companionPanel.add(this.add.text(px(14), px(56), statusText, {
        fontSize: fs(12), color: merc.alive ? '#e0d8cc' : '#e74c3c', fontFamily: FONT,
      }));
    }

    // Pet section
    this.renderPetSection(pw, ph);

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
        fontSize: fs(11), color: '#888', fontFamily: FONT, wordWrap: { width: pw - px(180), useAdvancedWrap: true },
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

  /** Render the pet section within the companion panel. */
  private renderPetSection(pw: number, ph: number): void {
    if (!this.companionPanel) return;
    const hs = this.zone?.homesteadSystem;
    if (!hs) return;

    const petStartY = px(82);
    const maxSlots = hs.getMaxPetSlots();

    this.companionPanel.add(this.add.text(px(14), petStartY, `─ 宠物 (${hs.pets.length} 只) ─`, {
      fontSize: fs(13), color: '#c0934a', fontFamily: FONT,
    }));

    if (hs.pets.length === 0) {
      this.companionPanel.add(this.add.text(px(14), petStartY + px(22), '暂无宠物。可通过击杀BOSS、完成任务或探索获得。', {
        fontSize: fs(11), color: '#888', fontFamily: FONT,
        wordWrap: { width: pw - px(28), useAdvancedWrap: true },
      }));
      return;
    }

    const cardH = px(52);
    const startY = petStartY + px(22);
    const allPets = hs.getAllPets();

    const rarityColors: Record<string, string> = {
      common: '#88cc88', rare: '#5599ff', epic: '#cc66ff',
    };

    hs.pets.forEach((pet, i) => {
      const def = allPets.find(p => p.id === pet.petId);
      if (!def) return;
      const cy = startY + i * (cardH + px(4));
      if (cy + cardH > ph - px(30)) return; // Prevent overflow

      const isActive = hs.activePet === pet.petId;
      const rarityColor = rarityColors[def.rarity] ?? '#888';
      const cardBgColor = isActive ? 0x1a2a1a : 0x111122;
      const borderColor = isActive ? 0x27ae60 : 0x333344;

      // Card background
      const cardBg = this.add.rectangle(pw / 2, cy + cardH / 2, pw - px(24), cardH, cardBgColor, 0.95)
        .setStrokeStyle(Math.round(1 * DPR), borderColor, 0.8)
        .setInteractive({ useHandCursor: true });
      this.companionPanel!.add(cardBg);

      // Active indicator
      if (isActive) {
        this.companionPanel!.add(this.add.text(px(16), cy + px(4), '★', {
          fontSize: fs(14), color: '#f1c40f', fontFamily: FONT,
        }));
      }

      // Pet name with evolution
      const displayName = hs.getPetDisplayName(pet);
      this.companionPanel!.add(this.add.text(px(32), cy + px(4), `${displayName} Lv.${pet.level}`, {
        fontSize: fs(13), color: rarityColor, fontFamily: FONT, fontStyle: 'bold',
      }));

      // Description
      this.companionPanel!.add(this.add.text(px(32), cy + px(20), def.description, {
        fontSize: fs(10), color: '#888', fontFamily: FONT,
        wordWrap: { width: pw - px(200), useAdvancedWrap: true },
      }));

      // EXP bar
      const expNeeded = pet.level * 20;
      const expRatio = pet.level >= def.maxLevel ? 1 : pet.exp / expNeeded;
      const barW = px(80), barH = px(6);
      const barX = px(32), barY = cy + px(36);
      this.companionPanel!.add(
        this.add.rectangle(barX + barW / 2, barY + barH / 2, barW, barH, 0x1a1a1a)
          .setStrokeStyle(Math.round(1 * DPR), 0x333333)
      );
      if (expRatio > 0) {
        this.companionPanel!.add(
          this.add.rectangle(barX, barY, Math.max(1, barW * expRatio), barH, 0x8e44ad).setOrigin(0, 0)
        );
      }
      const expText = pet.level >= def.maxLevel ? 'MAX' : `${pet.exp}/${expNeeded}`;
      this.companionPanel!.add(this.add.text(barX + barW + px(4), barY - px(1), expText, {
        fontSize: fs(9), color: '#b08cce', fontFamily: FONT,
      }));

      // Evolution badge
      if (pet.evolved > 0) {
        const evoBadge = pet.evolved >= 2 ? '至尊' : '觉醒';
        this.companionPanel!.add(this.add.text(barX + barW + px(44), barY - px(1), `[${evoBadge}]`, {
          fontSize: fs(9), color: '#f1c40f', fontFamily: FONT,
        }));
      }

      // Bonus stat display
      const evoMult = hs.getEvolutionMultiplier(pet);
      const baseBonus = def.bonusValue + def.bonusPerLevel * pet.level;
      const bonusVal = Math.floor(baseBonus * evoMult);
      this.companionPanel!.add(this.add.text(pw - px(110), cy + px(4), `+${bonusVal} ${def.bonusStat}`, {
        fontSize: fs(11), color: '#aaa', fontFamily: FONT,
      }));

      // Activate button
      if (!isActive) {
        const actBtn = this.add.text(pw - px(60), cy + px(22), '[激活]', {
          fontSize: fs(12), color: '#27ae60', fontFamily: FONT,
        }).setInteractive({ useHandCursor: true });
        actBtn.on('pointerdown', () => {
          hs.setActivePet(pet.petId);
          // Respawn pet sprite
          const zoneScene = this.zone as any;
          if (zoneScene?.spawnPetSprite) zoneScene.spawnPetSprite();
          this.companionPanel?.destroy();
          this.companionPanel = null;
          this.buildCompanionPanel();
        });
        this.companionPanel!.add(actBtn);
      } else {
        const deactBtn = this.add.text(pw - px(60), cy + px(22), '[取消]', {
          fontSize: fs(12), color: '#888', fontFamily: FONT,
        }).setInteractive({ useHandCursor: true });
        deactBtn.on('pointerdown', () => {
          hs.setActivePet(null);
          // Remove pet sprite
          const zoneScene = this.zone as any;
          if (zoneScene?.destroyPetSprite) zoneScene.destroyPetSprite();
          this.companionPanel?.destroy();
          this.companionPanel = null;
          this.buildCompanionPanel();
        });
        this.companionPanel!.add(deactBtn);
      }

      // Feed button
      const feedBtn = this.add.text(pw - px(110), cy + px(36), '[喂养]', {
        fontSize: fs(11), color: pet.level >= def.maxLevel ? '#555' : '#5dade2', fontFamily: FONT,
      });
      if (pet.level < def.maxLevel) {
        feedBtn.setInteractive({ useHandCursor: true });
        feedBtn.on('pointerdown', () => {
          // Check if player has the feed item
          const inv = this.zone?.inventorySystem;
          if (!inv) return;
          const feedItemIdx = inv.inventory.findIndex(it => it.baseId === def.feedItem);
          if (feedItemIdx === -1) {
            EventBus.emit(GameEvents.LOG_MESSAGE, { text: `需要 ${def.feedItem} 来喂养宠物!`, type: 'system' });
            return;
          }
          // Consume feed item
          const feedItem = inv.inventory[feedItemIdx];
          if (feedItem.quantity > 1) {
            feedItem.quantity--;
          } else {
            inv.inventory.splice(feedItemIdx, 1);
          }
          hs.feedPet(pet.petId);
          // Respawn pet sprite to update name if evolved
          const zoneScene = this.zone as any;
          if (zoneScene?.spawnPetSprite && hs.activePet === pet.petId) {
            zoneScene.spawnPetSprite();
          }
          this.companionPanel?.destroy();
          this.companionPanel = null;
          this.buildCompanionPanel();
        });
      }
      this.companionPanel!.add(feedBtn);

      // Click card to toggle active
      cardBg.on('pointerdown', () => {
        if (isActive) {
          hs.setActivePet(null);
          const zoneScene = this.zone as any;
          if (zoneScene?.destroyPetSprite) zoneScene.destroyPetSprite();
        } else {
          hs.setActivePet(pet.petId);
          const zoneScene = this.zone as any;
          if (zoneScene?.spawnPetSprite) zoneScene.spawnPetSprite();
        }
        this.companionPanel?.destroy();
        this.companionPanel = null;
        this.buildCompanionPanel();
      });
    });
  }

  // --- Achievement Unlock Toast ---
  private handleAchievementUnlocked(data: { achievement: import('../data/types').AchievementDefinition }): void {
    const ach = data.achievement;
    const toastW = px(320), toastH = px(60);
    const toastX = (W - toastW) / 2, toastY = px(60);
    const toast = this.add.container(toastX, toastY).setDepth(PANEL_STYLE.depth.toast).setAlpha(0);

    // Background with gold border
    const bg = this.add.graphics();
    bg.fillStyle(0x1a1a0a, 0.95);
    bg.fillRoundedRect(0, 0, toastW, toastH, px(6));
    bg.lineStyle(Math.round(2 * DPR), 0xf1c40f, 0.9);
    bg.strokeRoundedRect(0, 0, toastW, toastH, px(6));
    toast.add(bg);

    // Gold star icon
    toast.add(this.add.text(px(14), toastH / 2, '★', {
      fontSize: fs(22), color: '#f1c40f', fontFamily: FONT,
    }).setOrigin(0, 0.5));

    // Achievement name and description
    toast.add(this.add.text(px(42), px(10), `成就解锁: ${ach.name}`, {
      fontSize: fs(14), color: '#f1c40f', fontFamily: FONT, fontStyle: 'bold',
    }));
    const rewardParts: string[] = [];
    if (ach.reward) {
      const statDisp = STAT_DISPLAY[ach.reward.stat];
      const label = statDisp ? statDisp.label : ach.reward.stat;
      rewardParts.push(`${label}+${ach.reward.value}`);
    }
    if (ach.title) rewardParts.push(`称号: ${ach.title}`);
    const subText = rewardParts.length > 0 ? `${ach.description}  |  ${rewardParts.join('  ')}` : ach.description;
    toast.add(this.add.text(px(42), px(32), subText, {
      fontSize: fs(11), color: '#e0d8cc', fontFamily: FONT,
      wordWrap: { width: toastW - px(56), useAdvancedWrap: true },
    }));

    // Animate in
    this.tweens.add({
      targets: toast,
      alpha: 1,
      y: toastY + px(10),
      duration: 400,
      ease: 'Back.easeOut',
    });

    // Auto-dismiss after 3.5s
    this.time.delayedCall(3500, () => {
      this.tweens.add({
        targets: toast,
        alpha: 0,
        y: toastY - px(20),
        duration: 300,
        ease: 'Power2',
        onComplete: () => toast.destroy(),
      });
    });
  }

  // --- Achievement Panel (V) ---
  private toggleAchievement(): void {
    if (this.achievementPanel) { this.achievementPanel.destroy(); this.achievementPanel = null; return; }
    this.closeAllPanels();
    audioManager.playSFX('click');

    const pw = px(520), ph = px(500), panelX = (W - pw) / 2, panelY = px(10);
    this.achievementPanel = this.add.container(panelX, panelY).setDepth(PANEL_STYLE.depth.panel);
    this.animatePanelOpen(this.achievementPanel);

    // Background
    this.achievementPanel.add(this.createPanelBg(pw, ph));

    // Title
    this.achievementPanel.add(this.createPanelTitle(pw, '成 就'));

    // Close button
    this.achievementPanel.add(this.createPanelCloseBtn(pw, () => this.toggleAchievement()));

    // Unlocked title display
    const achSystem = this.zone?.achievementSystem;
    if (!achSystem) return;
    const achievements = achSystem.getAll();
    const unlockedTitles = achievements.filter(a => a.isUnlocked && a.title).map(a => a.title!);
    if (unlockedTitles.length > 0) {
      this.achievementPanel.add(this.add.text(pw / 2, px(34), `当前称号: ${unlockedTitles[unlockedTitles.length - 1]}`, {
        fontSize: fs(12), color: '#f1c40f', fontFamily: FONT,
      }).setOrigin(0.5, 0));
    }

    // Summary line
    const unlocked = achievements.filter(a => a.isUnlocked).length;
    this.achievementPanel.add(this.add.text(pw / 2, px(48), `已解锁: ${unlocked}/${achievements.length}`, {
      fontSize: fs(12), color: '#888', fontFamily: FONT,
    }).setOrigin(0.5, 0));

    // Achievement list
    const listTop = px(68);
    const rowH = px(56);
    const listH = ph - listTop - px(22);
    const maxVisible = Math.floor(listH / rowH);

    // Create scrollable content
    for (let i = 0; i < Math.min(achievements.length, maxVisible); i++) {
      const ach = achievements[i];
      const ry = listTop + i * rowH;
      this.renderAchievementRow(ach, px(10), ry, pw - px(20), rowH - px(4));
    }

    // Scroll support if more than visible
    if (achievements.length > maxVisible) {
      let scrollOffset = 0;
      const rebuildList = () => {
        // Remove old list items (keep bg, title, close, summary)
        const keepCount = 6 + (unlockedTitles.length > 0 ? 1 : 0);
        while (this.achievementPanel && this.achievementPanel.list.length > keepCount) {
          const child = this.achievementPanel.list[this.achievementPanel.list.length - 1];
          if (child && 'destroy' in child) (child as Phaser.GameObjects.GameObject).destroy();
          this.achievementPanel.remove(child);
        }
        const start = scrollOffset;
        const end = Math.min(start + maxVisible, achievements.length);
        for (let i = start; i < end; i++) {
          const ach = achievements[i];
          const ry = listTop + (i - start) * rowH;
          this.renderAchievementRow(ach, px(10), ry, pw - px(20), rowH - px(4));
        }
        // Scroll indicator
        if (this.achievementPanel) {
          this.achievementPanel.add(this.add.text(pw - px(14), ph - px(16), `${scrollOffset + 1}-${end}/${achievements.length}`, {
            fontSize: fs(10), color: '#555', fontFamily: FONT,
          }).setOrigin(1, 1));
        }
      };
      // Mouse wheel scroll — store handler ref for cleanup
      // eslint-disable-next-line @typescript-eslint/no-explicit-any
      const achWheelHandler = (_pointer: Phaser.Input.Pointer, _gx: number[], _gy: number[], _gz: number[], _gw: number, _gh: number, dy: number) => {
        if (!this.achievementPanel) return;
        if (dy > 0 && scrollOffset < achievements.length - maxVisible) { scrollOffset++; rebuildList(); }
        if (dy < 0 && scrollOffset > 0) { scrollOffset--; rebuildList(); }
      };
      // eslint-disable-next-line @typescript-eslint/no-explicit-any
      this.input.on('wheel', achWheelHandler as any);
      // Cleanup on panel destroy
      const originalDestroy = this.achievementPanel.destroy.bind(this.achievementPanel);
      this.achievementPanel.destroy = (...args: Parameters<typeof originalDestroy>) => {
        // eslint-disable-next-line @typescript-eslint/no-explicit-any
        this.input.off('wheel', achWheelHandler as any);
        return originalDestroy(...args);
      };
    }

    // Footer
    this.achievementPanel.add(this.add.text(pw / 2, ph - px(14), '按 V 关闭', {
      fontSize: fs(10), color: '#3a3a4a', fontFamily: FONT,
    }).setOrigin(0.5));
  }

  private renderAchievementRow(
    ach: import('../data/types').AchievementDefinition & { current: number; isUnlocked: boolean },
    x: number, y: number, w: number, h: number
  ): void {
    if (!this.achievementPanel) return;
    const isUnlocked = ach.isUnlocked;

    // Row background
    const rowBg = this.add.graphics();
    const bgColor = isUnlocked ? 0x1a1a0a : 0x0c0c18;
    const borderColor = isUnlocked ? 0xc0934a : 0x2a2a3e;
    const borderAlpha = isUnlocked ? 0.8 : 0.4;
    rowBg.fillStyle(bgColor, 0.9);
    rowBg.fillRoundedRect(x, y, w, h, px(4));
    rowBg.lineStyle(Math.round(1.5 * DPR), borderColor, borderAlpha);
    rowBg.strokeRoundedRect(x, y, w, h, px(4));
    // Unlocked glow
    if (isUnlocked) {
      rowBg.lineStyle(Math.round(1 * DPR), 0xf1c40f, 0.15);
      rowBg.strokeRoundedRect(x - px(1), y - px(1), w + px(2), h + px(2), px(5));
    }
    this.achievementPanel.add(rowBg);

    // Icon area
    const iconSize = px(36);
    const iconX = x + px(6);
    const iconY = y + (h - iconSize) / 2;
    const iconGfx = this.add.graphics();
    iconGfx.fillStyle(isUnlocked ? 0x2a2a0a : 0x080810, 0.9);
    iconGfx.fillRoundedRect(iconX, iconY, iconSize, iconSize, px(3));
    iconGfx.lineStyle(Math.round(1 * DPR), isUnlocked ? 0xf1c40f : 0x333344, 0.6);
    iconGfx.strokeRoundedRect(iconX, iconY, iconSize, iconSize, px(3));
    this.achievementPanel.add(iconGfx);

    // Star icon (gold for unlocked, grey for locked)
    const starColor = isUnlocked ? '#f1c40f' : '#444455';
    this.achievementPanel.add(this.add.text(iconX + iconSize / 2, iconY + iconSize / 2, isUnlocked ? '★' : '☆', {
      fontSize: fs(18), color: starColor, fontFamily: FONT,
    }).setOrigin(0.5));

    // Text area
    const textX = iconX + iconSize + px(8);
    const textAreaW = w - iconSize - px(20);

    // Name
    const nameColor = isUnlocked ? '#f1c40f' : '#777788';
    this.achievementPanel.add(this.add.text(textX, y + px(4), ach.name, {
      fontSize: fs(13), color: nameColor, fontFamily: FONT, fontStyle: 'bold',
    }));

    // Description
    this.achievementPanel.add(this.add.text(textX, y + px(20), ach.description, {
      fontSize: fs(10), color: isUnlocked ? '#e0d8cc' : '#555566', fontFamily: FONT,
      wordWrap: { width: textAreaW - px(100), useAdvancedWrap: true },
    }));

    // Progress bar
    const barW = px(80), barH = px(8);
    const barX = x + w - barW - px(8);
    const barY = y + px(8);
    const progress = Math.min(ach.current / ach.required, 1);

    const barGfx = this.add.graphics();
    barGfx.fillStyle(0x1a1a2e, 1);
    barGfx.fillRoundedRect(barX, barY, barW, barH, px(2));
    const fillW = Math.round(progress * barW);
    if (fillW > 0) {
      barGfx.fillStyle(isUnlocked ? 0xf1c40f : 0x555577, 0.8);
      barGfx.fillRoundedRect(barX, barY, fillW, barH, px(2));
    }
    barGfx.lineStyle(Math.round(1 * DPR), isUnlocked ? 0xf1c40f : 0x333344, 0.3);
    barGfx.strokeRoundedRect(barX, barY, barW, barH, px(2));
    this.achievementPanel.add(barGfx);

    // Progress text
    const progText = isUnlocked ? `${ach.required}/${ach.required}` : `${Math.min(ach.current, ach.required)}/${ach.required}`;
    this.achievementPanel.add(this.add.text(barX + barW / 2, barY + barH + px(2), progText, {
      fontSize: fs(9), color: isUnlocked ? '#f1c40f' : '#555566', fontFamily: FONT,
    }).setOrigin(0.5, 0));

    // Reward info
    const rewardParts: string[] = [];
    if (ach.reward) {
      const statDisp = STAT_DISPLAY[ach.reward.stat];
      const label = statDisp ? statDisp.label : ach.reward.stat;
      rewardParts.push(`${label}+${ach.reward.value}`);
    }
    if (ach.title) rewardParts.push(`称号: ${ach.title}`);
    if (rewardParts.length > 0) {
      const rewardColor = isUnlocked ? '#8be9fd' : '#444455';
      this.achievementPanel.add(this.add.text(barX + barW / 2, barY + barH + px(14), rewardParts.join('  '), {
        fontSize: fs(9), color: rewardColor, fontFamily: FONT,
      }).setOrigin(0.5, 0));
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
    this.audioPanel = this.add.container(panelX, panelY).setDepth(PANEL_STYLE.depth.panel);
    this.animatePanelOpen(this.audioPanel);
    this.audioPanel.add(this.createPanelBg(pw, ph));
    this.audioPanel.add(this.createPanelTitle(pw, '音频设置'));

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
    this.audioPanel.add(this.createPanelCloseBtn(pw, () => {
      this.cleanupAudioPanelInputHandlers();
      if (this.audioPanel) { this.audioPanel.destroy(); this.audioPanel = null; }
    }));
  }

  // ─── Mini-Boss Cinematic Dialogue ─────────────────────────────────────

  /** Show a cinematic pre-fight dialogue panel for a mini-boss. */
  private showMiniBossDialogue(bossName: string, dialogueTree: DialogueTree, onDismiss: () => void): void {
    this.closeAllPanels();
    audioManager.playSFX('click');

    // Full-screen dark backdrop
    this.miniBossDialogueBackdrop = this.add.rectangle(W / 2, H / 2, W, H, 0x000000, 0.55)
      .setInteractive().setDepth(PANEL_STYLE.depth.backdrop);

    const pw = px(500), ph = px(260);
    const panelX = (W - pw) / 2, panelY = (H - ph) / 2;
    this.miniBossDialoguePanel = this.add.container(panelX, panelY).setDepth(PANEL_STYLE.depth.panel);
    this.animatePanelOpen(this.miniBossDialoguePanel);

    // Background with unified style + red accent border for boss encounter
    const bg = this.add.rectangle(0, 0, pw, ph, PANEL_STYLE.bg.color, PANEL_STYLE.bg.alpha).setOrigin(0, 0)
      .setStrokeStyle(Math.round(PANEL_STYLE.border.width * DPR), 0xe74c3c);
    this.miniBossDialoguePanel.add(bg);

    // Boss name header
    this.miniBossDialoguePanel.add(this.add.text(pw / 2, px(10), `⚔ ${bossName} ⚔`, {
      fontSize: fs(PANEL_STYLE.header.fontSize), color: '#e74c3c', fontFamily: PANEL_STYLE.header.font, fontStyle: 'bold',
    }).setOrigin(0.5, 0));

    // Separator line
    this.miniBossDialoguePanel.add(
      this.add.rectangle(pw / 2, px(42), pw - px(40), Math.round(1 * DPR), 0x660000).setOrigin(0.5, 0)
    );

    // Collect all dialogue lines from the tree
    const lines: string[] = [];
    let nodeId: string | undefined = dialogueTree.startNodeId;
    while (nodeId) {
      const node: DialogueNode | undefined = dialogueTree.nodes[nodeId];
      if (!node) break;
      lines.push(node.text);
      if (node.isEnd) break;
      nodeId = node.nextNodeId;
    }

    // Display lines with typewriter-style presentation
    let dy = px(52);
    for (const line of lines) {
      this.miniBossDialoguePanel.add(this.add.text(px(20), dy, line, {
        fontSize: fs(13), color: '#ddd', fontFamily: FONT,
        wordWrap: { width: pw - px(40), useAdvancedWrap: true }, lineSpacing: px(3),
      }));
      dy += px(50);
    }

    // Dismiss button
    const btnY = ph - px(30);
    const btn = this.add.text(pw / 2, btnY, '[ 开战 ]', {
      fontSize: fs(16), color: '#e74c3c', fontFamily: FONT, fontStyle: 'bold',
    }).setOrigin(0.5).setInteractive({ useHandCursor: true });
    btn.on('pointerover', () => btn.setColor('#ff6666'));
    btn.on('pointerout', () => btn.setColor('#e74c3c'));
    btn.on('pointerdown', () => {
      this.closeMiniBossDialogue();
      onDismiss();
    });
    this.miniBossDialoguePanel.add(btn);

    // Also allow backdrop click to dismiss
    this.miniBossDialogueBackdrop.on('pointerdown', () => {
      this.closeMiniBossDialogue();
      onDismiss();
    });
  }

  private closeMiniBossDialogue(): void {
    if (this.miniBossDialoguePanel) {
      this.miniBossDialoguePanel.destroy();
      this.miniBossDialoguePanel = null;
    }
    if (this.miniBossDialogueBackdrop) {
      this.miniBossDialogueBackdrop.destroy();
      this.miniBossDialogueBackdrop = null;
    }
  }

  // ─── Lore Text Popup ─────────────────────────────────────────────────

  /** Show a lore text popup when a collectible is picked up. */
  private showLoreText(entry: LoreEntry): void {
    // Close existing lore panel if open
    if (this.loreTextPanel) { this.loreTextPanel.destroy(); this.loreTextPanel = null; }
    if (this.loreTextBackdrop) { this.loreTextBackdrop.destroy(); this.loreTextBackdrop = null; }

    audioManager.playSFX('click');

    // Semi-transparent backdrop
    this.loreTextBackdrop = this.add.rectangle(W / 2, H / 2, W, H, 0x000000, 0.4)
      .setInteractive().setDepth(PANEL_STYLE.depth.backdrop);

    const pw = px(440), ph = px(240);
    const panelX = (W - pw) / 2, panelY = (H - ph) / 2;
    this.loreTextPanel = this.add.container(panelX, panelY).setDepth(PANEL_STYLE.depth.panel);
    this.animatePanelOpen(this.loreTextPanel);

    // Background
    this.loreTextPanel.add(this.createPanelBg(pw, ph));

    // Header icon + name
    this.loreTextPanel.add(this.add.text(pw / 2, px(10), `📜 ${entry.name}`, {
      fontSize: fs(PANEL_STYLE.header.fontSize), color: PANEL_STYLE.header.color, fontFamily: PANEL_STYLE.header.font, fontStyle: 'bold',
    }).setOrigin(0.5, 0));

    // Zone name
    const zoneNames: Record<string, string> = {
      emerald_plains: '翡翠平原', twilight_forest: '暮色森林',
      anvil_mountains: '铁砧山脉', scorching_desert: '灼热沙漠', abyss_rift: '深渊裂隙',
    };
    this.loreTextPanel.add(this.add.text(pw / 2, px(36), zoneNames[entry.zone] ?? entry.zone, {
      fontSize: fs(11), color: '#888', fontFamily: FONT,
    }).setOrigin(0.5, 0));

    // Separator
    this.loreTextPanel.add(
      this.add.rectangle(pw / 2, px(52), pw - px(40), Math.round(1 * DPR), 0x333344).setOrigin(0.5, 0)
    );

    // Lore text
    this.loreTextPanel.add(this.add.text(px(20), px(60), entry.text, {
      fontSize: fs(12), color: '#ccc', fontFamily: FONT,
      wordWrap: { width: pw - px(40), useAdvancedWrap: true }, lineSpacing: px(3),
    }));

    // Close button
    this.loreTextPanel.add(this.createPanelCloseBtn(pw, () => this.closeLoreText()));

    // Dismiss on backdrop click
    this.loreTextBackdrop.on('pointerdown', () => this.closeLoreText());

    // Auto-close after 8 seconds
    this.time.delayedCall(8000, () => this.closeLoreText());
  }

  private closeLoreText(): void {
    if (this.loreTextPanel) { this.loreTextPanel.destroy(); this.loreTextPanel = null; }
    if (this.loreTextBackdrop) { this.loreTextBackdrop.destroy(); this.loreTextBackdrop = null; }
  }

  // ─── Lore Log Panel (Sub-tab in Quest Log) ───────────────────────────

  /** Render the lore log content inside the quest log panel. */
  private renderLoreLogContent(): void {
    if (!this.questLogPanel || !this.zone) return;

    const pw = px(700);
    const zoneNames: Record<string, string> = {
      emerald_plains: '翡翠平原', twilight_forest: '暮色森林',
      anvil_mountains: '铁砧山脉', scorching_desert: '灼热沙漠', abyss_rift: '深渊裂隙',
    };
    const zoneOrder = ['emerald_plains', 'twilight_forest', 'anvil_mountains', 'scorching_desert', 'abyss_rift'];

    const collected = this.zone.getLoreCollected();

    let dy = px(58);

    for (const zoneId of zoneOrder) {
      const zoneLore = LoreByZone[zoneId] ?? [];
      if (zoneLore.length === 0) continue;

      const discoveredCount = zoneLore.filter(l => collected.has(l.id)).length;
      const totalCount = zoneLore.length;
      const zoneName = zoneNames[zoneId] ?? zoneId;
      const progressColor = discoveredCount >= totalCount ? '#27ae60' : '#c0934a';

      // Zone header with progress
      this.questLogPanel.add(this.add.text(px(20), dy, `${zoneName}  —  ${discoveredCount}/${totalCount} 收集`, {
        fontSize: fs(15), color: progressColor, fontFamily: TITLE_FONT, fontStyle: 'bold',
      }));
      dy += px(24);

      // Progress bar
      const barX = px(20), barW = pw - px(60), barH = px(5);
      this.questLogPanel.add(this.add.rectangle(barX, dy, barW, barH, 0x1a1a2e).setOrigin(0, 0)
        .setStrokeStyle(Math.round(1 * DPR), 0x333344));
      if (discoveredCount > 0) {
        const fillW = Math.round(barW * (discoveredCount / totalCount));
        this.questLogPanel.add(this.add.rectangle(barX, dy, fillW, barH, discoveredCount >= totalCount ? 0x27ae60 : 0xDAA520).setOrigin(0, 0));
      }
      dy += px(12);

      // Lore entries
      for (const entry of zoneLore) {
        const found = collected.has(entry.id);
        const icon = found ? '✦' : '?';
        const nameText = found ? entry.name : '未发现';
        const color = found ? '#e0d8cc' : '#444';

        this.questLogPanel.add(this.add.text(px(30), dy, `${icon}  ${nameText}`, {
          fontSize: fs(12), color, fontFamily: FONT,
        }));

        if (found) {
          // Show truncated lore text
          const truncText = entry.text.length > 40 ? entry.text.substring(0, 40) + '...' : entry.text;
          this.questLogPanel.add(this.add.text(px(50), dy + px(16), truncText, {
            fontSize: fs(10), color: '#777', fontFamily: FONT,
            wordWrap: { width: pw - px(80), useAdvancedWrap: true },
          }));
          dy += px(36);
        } else {
          dy += px(20);
        }
      }

      dy += px(8);
    }

    // No lore message
    if (collected.size === 0) {
      this.questLogPanel.add(this.add.text(pw / 2, px(120), '尚未发现任何传说', {
        fontSize: fs(14), color: '#555', fontFamily: FONT,
      }).setOrigin(0.5, 0));
    }
  }

  /** Create a unified panel background rectangle using PANEL_STYLE. */
  private createPanelBg(pw: number, ph: number): Phaser.GameObjects.Rectangle {
    return this.add.rectangle(0, 0, pw, ph, PANEL_STYLE.bg.color, PANEL_STYLE.bg.alpha)
      .setOrigin(0, 0)
      .setStrokeStyle(Math.round(PANEL_STYLE.border.width * DPR), PANEL_STYLE.border.color);
  }

  /** Create a unified panel header title using PANEL_STYLE. */
  private createPanelTitle(pw: number, title: string): Phaser.GameObjects.Text {
    return this.add.text(pw / 2, px(10), title, {
      fontSize: fs(PANEL_STYLE.header.fontSize), color: PANEL_STYLE.header.color,
      fontFamily: PANEL_STYLE.header.font, fontStyle: 'bold',
    }).setOrigin(0.5, 0);
  }

  /** Create a unified close button at top-right using PANEL_STYLE. */
  private createPanelCloseBtn(pw: number, onClose: () => void): Phaser.GameObjects.Text {
    const btn = this.add.text(pw - px(16), px(10), '✕', {
      fontSize: fs(PANEL_STYLE.close.fontSize), color: PANEL_STYLE.close.color, fontFamily: FONT,
    }).setOrigin(0.5, 0).setInteractive({ useHandCursor: true });
    btn.on('pointerdown', onClose);
    btn.on('pointerover', () => btn.setColor(PANEL_STYLE.close.hoverColor));
    btn.on('pointerout', () => btn.setColor(PANEL_STYLE.close.color));
    return btn;
  }

  /** Create a unified tooltip container with PANEL_STYLE tooltip styling. */
  private createTooltipContainer(
    screenX: number, screenY: number, tipW: number, tipH: number, borderColor?: number,
  ): { container: Phaser.GameObjects.Container; bg: Phaser.GameObjects.Rectangle } {
    let tx = screenX + px(12);
    let ty = screenY - px(10);
    if (tx + tipW > W) tx = screenX - tipW - px(12);
    if (ty + tipH > H) ty = H - tipH - px(4);
    if (ty < px(4)) ty = px(4);
    if (tx < px(4)) tx = px(4);

    const container = this.add.container(tx, ty).setDepth(PANEL_STYLE.depth.tooltip);
    const bg = this.add.rectangle(0, 0, tipW, tipH, PANEL_STYLE.tooltip.bg.color, PANEL_STYLE.tooltip.bg.alpha)
      .setOrigin(0, 0)
      .setStrokeStyle(PANEL_STYLE.tooltip.border.width * DPR, borderColor ?? PANEL_STYLE.tooltip.border.color);
    container.add(bg);
    return { container, bg };
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
    if (this.skillPanel) {
      if (this.skillTreeWheelHandler) { this.input.off('wheel', this.skillTreeWheelHandler); this.skillTreeWheelHandler = null; }
      this.skillPanel.destroy(); this.skillPanel = null;
    }
    if (this.skillTooltip) { this.skillTooltip.destroy(); this.skillTooltip = null; }
    if (this.charPanel) { this.charPanel.destroy(); this.charPanel = null; }
    if (this.homesteadPanel) { this.homesteadPanel.destroy(); this.homesteadPanel = null; }
    if (this.questLogPanel) { this.questLogPanel.destroy(); this.questLogPanel = null; }
    if (this.companionPanel) { this.companionPanel.destroy(); this.companionPanel = null; }
    if (this.socketPanel) { this.socketPanel.destroy(); this.socketPanel = null; this.socketPanelSlot = null; }
    if (this.achievementPanel) { this.achievementPanel.destroy(); this.achievementPanel = null; }
    if (this.loreTextPanel) { this.loreTextPanel.destroy(); this.loreTextPanel = null; }
    if (this.loreTextBackdrop) { this.loreTextBackdrop.destroy(); this.loreTextBackdrop = null; }
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

  private getQualityTextColor(quality: string): string {
    switch (quality) {
      case 'magic': return '#5dade2';
      case 'rare': return '#f1c40f';
      case 'legendary': return '#e67e22';
      case 'set': return '#2ecc71';
      default: return '#e0d8cc';
    }
  }

  shutdown(): void {
    this.closeAllPanels();
    this.cleanupAudioPanelInputHandlers();
    EventBus.off(GameEvents.LOG_MESSAGE, this.handleLogMessage, this);
    EventBus.off(GameEvents.SHOP_OPEN, this.handleShopOpen, this);
    EventBus.off(GameEvents.NPC_INTERACT, this.handleNpcInteract, this);
    EventBus.off(GameEvents.UI_TOGGLE_PANEL, this.handlePanelToggle, this);
    EventBus.off(GameEvents.MINIBOSS_DIALOGUE, this.handleMiniBossDialogue, this);
    EventBus.off(GameEvents.LORE_COLLECTED, this.handleLoreCollected, this);
    EventBus.off(GameEvents.ACHIEVEMENT_UNLOCKED, this.handleAchievementUnlocked, this);
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
    const alLabels: Record<string, string> = { off: '拾取\nOFF', all: '拾取\n全部', magic: '拾取\n魔法+', rare: '拾取\n稀有+', legendary: '拾取\n传奇+' };
    const alColors: Record<string, string> = { off: '#666680', all: '#e0d8cc', magic: '#2471a3', rare: '#c0934a', legendary: '#e67e22' };
    const autoLootText = alLabels[this.player.autoLootMode] ?? '拾取\nOFF';
    const autoLootColor = alColors[this.player.autoLootMode] ?? '#666680';
    if (this.autoLootText.text !== autoLootText) this.autoLootText.setText(autoLootText);
    if (this.autoLootText.style.color !== autoLootColor) this.autoLootText.setColor(autoLootColor);

    if (this.zone && (this.zone as any).currentMapId) {
      const map = AllMaps[(this.zone as any).currentMapId] ?? (this.zone as any).mapData;
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
      // Add type tag for new quest types
      const typeTag = ['escort', 'defend', 'investigate', 'craft'].includes(quest.type)
        ? ` [${QUEST_TYPE_LABELS[quest.type] ?? ''}]` : '';
      entries.push({ text: `${tag} ${quest.name}${typeTag}${statusTag}`, isTitle: true, isMain: quest.category === 'main', isDone: progress.status === 'completed' });

      // For investigate quests, show aggregate clue count
      if (quest.type === 'investigate') {
        const totalClues = quest.objectives.filter(o => o.type === 'investigate_clue').length;
        const foundClues = quest.objectives
          .map((o, idx) => o.type === 'investigate_clue' && (progress.objectives[idx]?.current ?? 0) >= o.required ? 1 : 0)
          .reduce((a: number, b: number) => a + b, 0);
        entries.push({ text: `  线索 ${foundClues}/${totalClues}`, isTitle: false, isMain: quest.category === 'main', isDone: foundClues >= totalClues });
      }

      // For defend quests, show wave counter
      if (quest.type === 'defend' && quest.defendTarget) {
        const waveObj = quest.objectives.find(o => o.type === 'defend_wave');
        const waveIdx = waveObj ? quest.objectives.indexOf(waveObj) : -1;
        const curWave = waveIdx >= 0 ? (progress.objectives[waveIdx]?.current ?? 0) : 0;
        entries.push({ text: `  浪潮 ${curWave}/${quest.defendTarget.totalWaves}`, isTitle: false, isMain: quest.category === 'main', isDone: curWave >= quest.defendTarget.totalWaves });
      }

      // For other quest types and non-aggregate objectives, show individual entries
      if (quest.type !== 'investigate' && quest.type !== 'defend') {
        for (let i = 0; i < quest.objectives.length; i++) {
          const obj = quest.objectives[i];
          const cur = progress.objectives[i]?.current ?? 0;
          const done = cur >= obj.required;
          const mark = done ? '\u2713' : `${cur}/${obj.required}`;
          let locHint = '';
          if ((obj.type === 'explore' || obj.type === 'investigate_clue' || obj.type === 'escort') && !done && obj.location) {
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

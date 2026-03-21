import Phaser from 'phaser';
import { GAME_WIDTH, GAME_HEIGHT, DPR } from '../config';
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

    for (let i = 0; i < skills.length; i++) {
      const x = startX + i * (slotSize + gap);
      const skill = skills[i];
      const container = this.add.container(x + slotSize / 2, y).setDepth(3000);
      const bg = this.add.rectangle(0, 0, slotSize, slotSize, 0x1a1a2e).setStrokeStyle(1.5 * DPR, 0x555566);
      container.add(bg);
      container.add(this.add.text(0, px(-6), skill.name.substring(0, 2), {
        fontSize: fs(16), color: '#e0d8cc', fontFamily: FONT, fontStyle: 'bold',
      }).setOrigin(0.5));
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
    this.questTracker = this.add.container(px(16), px(52)).setDepth(3000);
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
    const statText = Object.entries(eqStats).map(([k, v]) => `${k}: +${v}`).join('  ');
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
  private toggleSkillTree(): void {
    if (this.skillPanel) { this.skillPanel.destroy(); this.skillPanel = null; return; }
    this.closeAllPanels();
    const pw = px(480), ph = px(420), panelX = (W - pw) / 2, panelY = px(10);
    this.skillPanel = this.add.container(panelX, panelY).setDepth(4000);
    this.animatePanelOpen(this.skillPanel);
    this.skillPanel.add(this.add.rectangle(0, 0, pw, ph, 0x0f0f1e, 0.95).setOrigin(0, 0).setStrokeStyle(Math.round(2 * DPR), 0x8e44ad));
    this.skillPanel.add(this.add.text(pw / 2, px(10), `技能树 - ${this.player.classData.name}`, {
      fontSize: fs(17), color: '#b08cce', fontFamily: TITLE_FONT, fontStyle: 'bold',
    }).setOrigin(0.5, 0));
    this.skillPanel.add(this.add.text(pw / 2, px(28), `剩余技能点: ${this.player.freeSkillPoints}`, {
      fontSize: fs(13), color: '#f1c40f', fontFamily: FONT,
    }).setOrigin(0.5, 0));
    const closeBtn = this.add.text(pw - px(14), px(8), 'X', {
      fontSize: fs(18), color: '#e74c3c', fontFamily: FONT,
    }).setInteractive({ useHandCursor: true });
    closeBtn.on('pointerdown', () => this.toggleSkillTree());
    this.skillPanel.add(closeBtn);

    const trees = new Map<string, typeof this.player.classData.skills>();
    for (const skill of this.player.classData.skills) {
      if (!trees.has(skill.tree)) trees.set(skill.tree, []);
      trees.get(skill.tree)!.push(skill);
    }
    let treeIdx = 0;
    const treeW = (pw - px(24)) / Math.max(trees.size, 1);
    for (const [treeName, skills] of trees) {
      const tx = px(12) + treeIdx * treeW;
      this.skillPanel.add(this.add.text(tx + treeW / 2, px(46), treeName, {
        fontSize: fs(13), color: '#c0934a', fontFamily: FONT,
      }).setOrigin(0.5, 0));
      skills.forEach((skill, si) => {
        const sy = px(65) + si * px(60);
        const level = this.player.getSkillLevel(skill.id);
        const canLevel = this.player.freeSkillPoints > 0 && level < skill.maxLevel;
        const slotBg = this.add.rectangle(tx + treeW / 2, sy + px(12), treeW - px(14), px(52), 0x1a1a2e)
          .setStrokeStyle(Math.round(1 * DPR), canLevel ? 0xf1c40f : 0x333344).setOrigin(0.5, 0);
        this.skillPanel!.add(slotBg);
        this.skillPanel!.add(this.add.text(tx + treeW / 2, sy + px(16), skill.name, {
          fontSize: fs(13), color: '#e0d8cc', fontFamily: FONT,
        }).setOrigin(0.5, 0));
        this.skillPanel!.add(this.add.text(tx + treeW / 2, sy + px(32), `Lv.${level}/${skill.maxLevel}  MP:${skill.manaCost}  CD:${(skill.cooldown / 1000).toFixed(1)}s`, {
          fontSize: fs(12), color: '#888', fontFamily: FONT,
        }).setOrigin(0.5, 0));
        this.skillPanel!.add(this.add.text(tx + treeW / 2, sy + px(46), skill.description, {
          fontSize: fs(11), color: '#666', fontFamily: FONT, wordWrap: { width: treeW - px(20) },
        }).setOrigin(0.5, 0));
        if (canLevel) {
          const plusBtn = this.add.text(tx + treeW - px(14), sy + px(16), '+', {
            fontSize: fs(18), color: '#27ae60', fontFamily: FONT, fontStyle: 'bold',
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
    const derived = [
      `HP: ${Math.ceil(this.player.hp)}/${this.player.maxHp}`,
      `MP: ${Math.ceil(this.player.mana)}/${this.player.maxMana}`,
      `攻击: ${Math.floor(this.player.baseDamage)}${eqStats['damage'] ? ` (+${eqStats['damage']})` : ''}`,
      `防御: ${Math.floor(this.player.defense)}${eqStats['defense'] ? ` (+${eqStats['defense']})` : ''}`,
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
        lines.push({ text: `${affix.name}: +${affix.value}`, color: '#5dade2', size: 12 });
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

  // --- Audio Settings Panel ---
  private toggleAudioSettings(): void {
    if (this.audioPanel) { this.audioPanel.destroy(); this.audioPanel = null; return; }
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
      this.input.on('pointermove', (p: Phaser.Input.Pointer) => { if (dragging) updateSlider(p.x); });
      this.input.on('pointerup', () => { dragging = false; });

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
  update(time: number): void {
    if (!this.player) return;
    const globeH = GLOBE_R * 2;
    const globeBottom = (H - px(50)) + GLOBE_R;

    const hpR = Math.max(0, this.player.hp / this.player.maxHp);
    const targetHpH = globeH * hpR;
    this.hpBar.height += (targetHpH - this.hpBar.height) * 0.15;
    this.hpBar.y = globeBottom - this.hpBar.height;
    this.hpText.setText(`${Math.ceil(this.player.hp)}/${this.player.maxHp}`);
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
    this.manaText.setText(`${Math.ceil(this.player.mana)}/${this.player.maxMana}`);
    const expN = this.player.expToNextLevel();
    this.expBar.width = (W - px(32)) * (this.player.exp / expN);
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
      // Sort: main quests first
      const sorted = active.sort((a, b) => {
        if (a.quest.category === 'main' && b.quest.category !== 'main') return -1;
        if (a.quest.category !== 'main' && b.quest.category === 'main') return 1;
        return 0;
      });

      // Build line entries
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

      // Reuse or create text objects
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
      // Hide unused text objects
      for (let i = entries.length; i < this.questTrackerTexts.length; i++) {
        this.questTrackerTexts[i].setVisible(false);
      }
    }
  }
}

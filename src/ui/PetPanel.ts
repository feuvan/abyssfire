/**
 * 灵兽 panel (key P, HUD medallion, touch panel button, links from the homestead and
 * companion panels): the beasts the hero has met, which one fights, feeding with
 * 灵脉果, level / exp, evolution stage, bond pips and the abilities (locked until 觉醒).
 *
 * Built by UIScene through a small host (panel frame, title, close button, buttons,
 * close-all) so it shares the panel look and the touch fit-to-screen behaviour.
 * Coordinates are logical (1280×720); UIScene's camera applies RENDER_SCALE.
 */
import Phaser from 'phaser';
import { DPR, GAME_WIDTH } from '../config';
import { EventBus, GameEvents } from '../utils/EventBus';
import { DisposableScope } from '../utils/DisposableScope';
import { t } from '../i18n';
import { SpriteGenerator } from '../graphics/SpriteGenerator';
import { ensureItemIcon } from '../graphics/icons/ItemIcons';
import {
  PETS, PET_MAX_LEVEL, PET_EVOLUTION_LEVELS, PET_MAX_BOND, LEY_FRUIT_ID,
  petNameKey, petDescKey, petOriginKey, petRoleKey, petStatKey, petAbilityNameKey, petAbilityDescKey,
  primaryAbility, type PetDef,
} from '../data/pets';
import { petExpToNext, petPassiveValue, type PetSystem, type PetInstance } from '../systems/PetSystem';
import type { InventorySystem } from '../systems/InventorySystem';
import {
  UI_FONT, UI_COLORS, drawCard, drawWell, drawBarFill, pipTexture, medallionTexture, addSectionHeader,
  type ButtonOptions, type UiButton,
} from './UiKit';

const px = (n: number): number => Math.round(n * DPR);
const fs = (n: number): string => `${Math.round(n * DPR)}px`;

const RARITY_COLOR: Record<PetDef['rarity'], number> = { common: 0x8fd08a, rare: 0x6fa8ff, epic: 0xc58cff };
const ELEMENT_COLOR: Record<string, number> = { fire: 0xff8a3a, arcane: 0xc070ff, physical: 0xd8c8a8 };

export interface PetPanelHost {
  scene: Phaser.Scene;
  pets(): PetSystem | null;
  inventory(): InventorySystem | null;
  isMobile: boolean;
  depth: number;
  closeAllPanels(): void;
  createPanelBg(pw: number, ph: number): Phaser.GameObjects.Image;
  createPanelTitle(pw: number, title: string): Phaser.GameObjects.Container;
  createPanelCloseBtn(pw: number, onClose: () => void): Phaser.GameObjects.Image;
  makeButton(x: number, y: number, w: number, h: number, label: string, onClick: () => void, opts?: Omit<ButtonOptions, 'onClick'>): UiButton;
  animatePanelOpen(panel: Phaser.GameObjects.Container): void;
}

function tr(key: string, fallback: string, params?: Record<string, string | number>): string {
  const s = t(key, params);
  return s === key ? fallback : s;
}

/** Texture key for a beast's portrait (the cel-shaded pet decoration). */
export function petPortraitKey(scene: Phaser.Scene, petId: string): string | null {
  const key = `decor_pet_${petId}`;
  SpriteGenerator.ensureDecoration(scene, key);
  return scene.textures.exists(key) ? key : null;
}

export class PetPanel {
  private readonly host: PetPanelHost;
  private panel: Phaser.GameObjects.Container | null = null;
  private selected: string | null = null;
  private readonly subs = new DisposableScope();
  private hudBtn: Phaser.GameObjects.Container | null = null;
  private hudPortrait: Phaser.GameObjects.Image | null = null;
  private hudKey = '';
  private hudSize = 36;

  constructor(host: PetPanelHost) {
    this.host = host;
    this.subs.on(EventBus, GameEvents.PET_CHANGED, () => {
      if (this.panel) this.rebuild();
      this.refreshHud();
    });
  }

  isOpen(): boolean {
    return !!this.panel;
  }

  toggle(): void {
    if (this.panel) { this.close(); return; }
    this.host.closeAllPanels();
    this.open();
  }

  close(): void {
    this.panel?.destroy();
    this.panel = null;
  }

  /** Re-render in place (locale change, feed, activation). */
  rebuild(): void {
    if (!this.panel) return;
    const x = this.panel.x, y = this.panel.y, scale = this.panel.scale;
    this.panel.destroy();
    this.panel = null;
    this.open(false);
    this.panel!.setPosition(x, y).setScale(scale).setAlpha(1);
  }

  destroy(): void {
    this.close();
    this.hudBtn?.destroy();
    this.hudBtn = null;
    this.subs.dispose();
  }

  // ─── HUD medallion (desktop) ─────────────────────────────────────────────

  /** Round button with the active beast's portrait; hidden until a beast is owned. */
  createHudButton(x: number, y: number, size: number): void {
    const scene = this.host.scene;
    this.hudBtn?.destroy();
    const c = scene.add.container(x, y).setDepth(3000);
    const ring = scene.add.image(0, 0, medallionTexture(scene, size, 0x2a3a30)).setInteractive({ useHandCursor: true });
    c.add(ring);
    ring.on('pointerover', () => ring.setTint(0xfff0c8));
    ring.on('pointerout', () => ring.clearTint());
    ring.on('pointerdown', (_p: Phaser.Input.Pointer, _x: number, _y: number, ev: Phaser.Types.Input.EventData) => {
      ev?.stopPropagation?.();
      this.toggle();
    });
    const key = scene.add.text(size / 2 - px(2), size / 2 - px(2), 'P', {
      fontSize: fs(9), color: UI_COLORS.goldBright, fontFamily: UI_FONT, fontStyle: 'bold',
      stroke: '#000000', strokeThickness: Math.round(2 * DPR),
    }).setOrigin(1, 1);
    c.add(key);
    this.hudBtn = c;
    this.hudSize = size;
    this.hudPortrait = null;
    this.hudKey = '';
    this.refreshHud(size);
  }

  /** Update the HUD medallion (zone change / save loaded). */
  syncHud(): void {
    this.hudKey = '\u0000';
    this.refreshHud();
  }

  private refreshHud(size = this.hudSize): void {
    const c = this.hudBtn;
    if (!c) return;
    const pets = this.host.pets();
    const active = pets?.getActivePetInstance();
    const any = (pets?.pets.length ?? 0) > 0;
    c.setVisible(any);
    const want = active?.petId ?? '';
    if (want === this.hudKey) return;
    this.hudKey = want;
    this.hudPortrait?.destroy();
    this.hudPortrait = null;
    if (!active) return;
    const tex = petPortraitKey(this.host.scene, active.petId);
    if (!tex) return;
    const img = this.host.scene.add.image(0, px(2), tex);
    const s = (size * 0.78) / Math.max(img.width, img.height);
    img.setScale(s);
    c.addAt(img, 1);
    this.hudPortrait = img;
  }

  // ─── Panel ───────────────────────────────────────────────────────────────

  private open(animate = true): void {
    const scene = this.host.scene;
    const pets = this.host.pets();
    const W = GAME_WIDTH * DPR;
    const pw = px(660), ph = px(540);
    const panel = scene.add.container(Math.round((W - pw) / 2), px(10)).setDepth(this.host.depth);
    this.panel = panel;
    if (animate) this.host.animatePanelOpen(panel);
    panel.add(this.host.createPanelBg(pw, ph));
    panel.add(this.host.createPanelTitle(pw, t('ui.pet.title')));
    panel.add(this.host.createPanelCloseBtn(pw, () => this.close()));
    if (!pets) return;

    const owned = pets.pets;
    panel.add(scene.add.text(px(20), px(52), t('ui.pet.owned', { count: owned.length, total: PETS.length }), {
      fontSize: fs(12), color: UI_COLORS.textSoft, fontFamily: UI_FONT, fontStyle: 'bold',
    }).setOrigin(0, 0.5));
    const fruit = this.fruitCount();
    const fruitIcon = scene.add.image(pw - px(64), px(52), ensureItemIcon(scene, 'c_ley_fruit')).setDisplaySize(px(22), px(22));
    panel.add(fruitIcon);
    panel.add(scene.add.text(pw - px(50), px(52), `× ${fruit}`, {
      fontSize: fs(12), color: '#9ff6e0', fontFamily: UI_FONT, fontStyle: 'bold',
    }).setOrigin(0, 0.5));

    // Beasts met first (active on top), then the ones still out there.
    const order = [...PETS].sort((a, b) => {
      const ia = owned.findIndex(p => p.petId === a.id), ib = owned.findIndex(p => p.petId === b.id);
      const ka = ia < 0 ? 100 + PETS.indexOf(a) : (a.id === pets.activePet ? -1 : ia);
      const kb = ib < 0 ? 100 + PETS.indexOf(b) : (b.id === pets.activePet ? -1 : ib);
      return ka - kb;
    });
    if (!this.selected || !PETS.some(p => p.id === this.selected)) {
      this.selected = pets.activePet ?? owned[0]?.petId ?? null;
    }

    const listX = px(16), listY = px(70), listW = px(208), rowH = px(50), rowGap = px(4);
    order.forEach((def, i) => {
      const inst = pets.getPetInstance(def.id);
      this.renderRow(panel, def, inst, listX, listY + i * (rowH + rowGap), listW, rowH, pets);
    });

    const detailX = listX + listW + px(14);
    const detailW = pw - detailX - px(16);
    if (owned.length === 0) {
      panel.add(scene.add.text(detailX + detailW / 2, px(250), t('ui.pet.none'), {
        fontSize: fs(13), color: UI_COLORS.muted, fontFamily: UI_FONT, align: 'center', lineSpacing: 6,
        wordWrap: { width: detailW - px(30), useAdvancedWrap: true },
      }).setOrigin(0.5));
    } else if (this.selected) {
      const def = PETS.find(p => p.id === this.selected)!;
      this.renderDetail(panel, def, pets.getPetInstance(def.id), detailX, px(70), detailW, ph - px(70) - px(34), pets);
    }

    panel.add(scene.add.text(pw / 2, ph - px(16), t(this.host.isMobile ? 'ui.pet.footerTouch' : 'ui.pet.footer'), {
      fontSize: fs(11), color: UI_COLORS.muted, fontFamily: UI_FONT,
    }).setOrigin(0.5));
  }

  private renderRow(
    panel: Phaser.GameObjects.Container, def: PetDef, inst: PetInstance | undefined,
    x: number, y: number, w: number, h: number, pets: PetSystem,
  ): void {
    const scene = this.host.scene;
    const isSel = this.selected === def.id;
    const isActive = pets.activePet === def.id;
    const rarity = RARITY_COLOR[def.rarity];
    const g = scene.add.graphics();
    drawCard(g, x, y, w, h, isActive
      ? { fill: 0x172414, border: 0x6fd35a, glow: isSel ? 0x6fd35a : undefined, strip: 0x6fd35a }
      : isSel ? { fill: 0x221d25, border: UI_COLORS.goldNum, glow: UI_COLORS.goldNum }
        : { border: inst ? rarity : 0x2c2830, borderAlpha: inst ? 0.6 : 1 });
    panel.add(g);

    const iconS = h - px(12);
    const icx = x + px(8) + iconS / 2, icy = y + h / 2;
    const well = scene.add.graphics();
    drawWell(well, icx - iconS / 2, icy - iconS / 2, iconS, iconS, px(4), inst ? rarity : 0x2c2830);
    panel.add(well);
    const tex = petPortraitKey(scene, def.id);
    if (tex) {
      const img = scene.add.image(icx, icy + px(2), tex);
      img.setScale((iconS * 0.9) / Math.max(img.width, img.height));
      if (!inst) img.setTintFill(0x1a1620).setAlpha(0.9);
      panel.add(img);
    }

    const tx = icx + iconS / 2 + px(8);
    const name = inst ? pets.getPetDisplayName(inst) : '？？？';
    panel.add(scene.add.text(tx, y + px(8), name, {
      fontSize: fs(12), color: inst ? (isActive ? '#8ff07a' : '#' + rarity.toString(16).padStart(6, '0')) : UI_COLORS.dim,
      fontFamily: UI_FONT, fontStyle: 'bold',
    }).setOrigin(0, 0));
    const sub = inst
      ? `${t('ui.pet.level', { level: inst.level })} · ${tr(petRoleKey(def.role), def.role)}`
      : t(`ui.pet.chapter.${def.chapter}`);
    panel.add(scene.add.text(tx, y + px(28), sub, {
      fontSize: fs(10), color: inst ? UI_COLORS.textSoft : UI_COLORS.faint, fontFamily: UI_FONT,
      wordWrap: { width: w - (tx - x) - px(6), useAdvancedWrap: true }, maxLines: 1,
    }).setOrigin(0, 0));
    if (isActive) {
      panel.add(scene.add.text(x + w - px(10), y + px(10), '✦', {
        fontSize: fs(14), color: '#8ff07a', fontFamily: UI_FONT,
      }).setOrigin(1, 0));
    } else if (inst && pets.isAway(def.id)) {
      panel.add(scene.add.text(x + w - px(8), y + px(10), t('ui.pet.away'), {
        fontSize: fs(9), color: UI_COLORS.info, fontFamily: UI_FONT,
      }).setOrigin(1, 0));
    }

    const hit = scene.add.rectangle(x, y, w, h, 0x000000, 0).setOrigin(0, 0).setInteractive({ useHandCursor: true });
    hit.on('pointerdown', () => {
      if (this.selected === def.id) return;
      this.selected = def.id;
      this.rebuild();
    });
    panel.add(hit);
  }

  private renderDetail(
    panel: Phaser.GameObjects.Container, def: PetDef, inst: PetInstance | undefined,
    x: number, y: number, w: number, h: number, pets: PetSystem,
  ): void {
    const scene = this.host.scene;
    const rarity = RARITY_COLOR[def.rarity];
    const card = scene.add.graphics();
    drawCard(card, x, y, w, h, { border: 0x3f3845 });
    panel.add(card);

    // Portrait
    const portS = px(96);
    const pcx = x + px(14) + portS / 2, pcy = y + px(14) + portS / 2;
    const pg = scene.add.graphics();
    drawWell(pg, pcx - portS / 2, pcy - portS / 2, portS, portS, px(6), rarity);
    pg.fillStyle(rarity, 0.08);
    pg.fillCircle(pcx, pcy + px(10), portS * 0.36);
    panel.add(pg);
    const tex = petPortraitKey(scene, def.id);
    if (tex) {
      const img = scene.add.image(pcx, pcy + px(6), tex);
      img.setScale((portS * 0.9) / Math.max(img.width, img.height));
      if (!inst) img.setTintFill(0x1a1620);
      panel.add(img);
    }

    const tx = pcx + portS / 2 + px(14);
    const textW = x + w - tx - px(12);
    const name = inst ? pets.getPetDisplayName(inst) : '？？？';
    panel.add(scene.add.text(tx, y + px(14), name, {
      fontSize: fs(17), color: '#' + rarity.toString(16).padStart(6, '0'), fontFamily: UI_FONT, fontStyle: 'bold',
      stroke: '#000000', strokeThickness: Math.round(2 * DPR),
    }));
    const evo = inst ? inst.evolved : 0;
    panel.add(scene.add.text(tx, y + px(40), `${tr(petRoleKey(def.role), def.role)} · ${t(`ui.pet.evo.${evo}`)}`, {
      fontSize: fs(11), color: UI_COLORS.goldBright, fontFamily: UI_FONT,
    }));
    panel.add(scene.add.text(tx, y + px(58), inst ? tr(petDescKey(def.id), '') : tr(petOriginKey(def.id), ''), {
      fontSize: fs(11), color: UI_COLORS.textSoft, fontFamily: UI_FONT, lineSpacing: 3,
      wordWrap: { width: textW, useAdvancedWrap: true }, maxLines: 3,
    }));

    if (!inst) return;

    // Level / exp
    let cy = y + portS + px(30);
    const barX = x + px(14), barW = w - px(28);
    const maxed = inst.level >= PET_MAX_LEVEL;
    const need = petExpToNext(inst.level);
    panel.add(scene.add.text(barX, cy, t(maxed ? 'ui.pet.levelMax' : 'ui.pet.level', { level: inst.level }), {
      fontSize: fs(12), color: UI_COLORS.text, fontFamily: UI_FONT, fontStyle: 'bold',
    }).setOrigin(0, 0.5));
    const nextEvo = PET_EVOLUTION_LEVELS.find(l => l > inst.level);
    panel.add(scene.add.text(barX + barW, cy, maxed ? '' : `${t('ui.pet.exp', { exp: inst.exp, need })}${nextEvo ? '  ·  ' + t('ui.pet.nextEvo', { level: nextEvo }) : ''}`, {
      fontSize: fs(10), color: UI_COLORS.muted, fontFamily: UI_FONT,
    }).setOrigin(1, 0.5));
    cy += px(12);
    const eg = scene.add.graphics();
    drawWell(eg, barX, cy, barW, px(8), px(3));
    drawBarFill(eg, barX + 1, cy + 1, Math.round((barW - 2) * (maxed ? 1 : Math.min(1, inst.exp / need))), px(6), 0x9b6fe0);
    panel.add(eg);

    // Bond pips
    cy += px(24);
    const cap = pets.getBondCap();
    panel.add(scene.add.text(barX, cy, t('ui.pet.bond'), {
      fontSize: fs(12), color: UI_COLORS.text, fontFamily: UI_FONT, fontStyle: 'bold',
    }).setOrigin(0, 0.5));
    const pipS = px(14);
    for (let i = 0; i < PET_MAX_BOND; i++) {
      const col = i < inst.bond ? 0xff7fa8 : null;
      const pip = scene.add.image(barX + px(46) + i * (pipS + px(6)), cy, pipTexture(scene, pipS, col));
      if (i >= cap) pip.setAlpha(0.3);
      panel.add(pip);
    }
    const bondNote = inst.bond >= PET_MAX_BOND
      ? t('ui.pet.bondMax', { ability: tr(petAbilityNameKey(primaryAbility(def)?.id ?? ''), '') })
      : cap < PET_MAX_BOND ? t('ui.pet.bondCap', { cap }) : `${inst.bondProgress}%`;
    panel.add(scene.add.text(barX + px(46) + PET_MAX_BOND * (pipS + px(6)) + px(4), cy, bondNote, {
      fontSize: fs(10), color: inst.bond >= PET_MAX_BOND ? '#ff9fc0' : UI_COLORS.muted, fontFamily: UI_FONT,
      wordWrap: { width: barW - px(46) - PET_MAX_BOND * (pipS + px(6)) - px(4), useAdvancedWrap: true }, maxLines: 2,
    }).setOrigin(0, 0.5));

    // Passive
    cy += px(24);
    panel.add(scene.add.text(barX, cy, t('ui.pet.passive'), {
      fontSize: fs(12), color: UI_COLORS.text, fontFamily: UI_FONT, fontStyle: 'bold',
    }).setOrigin(0, 0.5));
    panel.add(scene.add.text(barX + px(46), cy, tr(petStatKey(def.passive.stat), def.passive.stat, { value: petPassiveValue(def, inst) }), {
      fontSize: fs(12), color: UI_COLORS.good, fontFamily: UI_FONT, fontStyle: 'bold',
    }).setOrigin(0, 0.5));

    // Abilities
    cy += px(22);
    panel.add(addSectionHeader(scene, barX, cy, barW, t('ui.pet.abilities')));
    cy += px(14);
    const abH = px(46);
    for (const a of def.abilities) {
      const unlocked = a.unlock <= inst.evolved;
      const ag = scene.add.graphics();
      const accent = ELEMENT_COLOR[a.element ?? def.combat.element] ?? 0xd8c8a8;
      drawCard(ag, barX, cy, barW, abH - px(4), unlocked ? { border: accent, borderAlpha: 0.55, strip: accent } : { border: 0x2c2830 });
      panel.add(ag);
      const nameText = tr(petAbilityNameKey(a.id), a.id);
      const meta = a.kind === 'revive' ? t('ui.pet.passiveTag') : t('ui.pet.cooldown', { sec: Math.round(a.cooldownMs / 1000) });
      panel.add(scene.add.text(barX + px(12), cy + px(5), nameText, {
        fontSize: fs(12), color: unlocked ? UI_COLORS.goldBright : UI_COLORS.dim, fontFamily: UI_FONT, fontStyle: 'bold',
      }));
      panel.add(scene.add.text(barX + barW - px(10), cy + px(6), unlocked ? meta : `🔒 ${t('ui.pet.locked')}`, {
        fontSize: fs(10), color: unlocked ? UI_COLORS.muted : UI_COLORS.dim, fontFamily: UI_FONT,
      }).setOrigin(1, 0));
      panel.add(scene.add.text(barX + px(12), cy + px(23), tr(petAbilityDescKey(a.id), ''), {
        fontSize: fs(10), color: unlocked ? UI_COLORS.textSoft : UI_COLORS.faint, fontFamily: UI_FONT,
        wordWrap: { width: barW - px(24), useAdvancedWrap: true }, maxLines: 1,
      }));
      cy += abH;
    }

    // Buttons
    const by = y + h - px(24);
    const isActive = pets.activePet === def.id;
    const away = pets.isAway(def.id);
    const actBtn = this.host.makeButton(x + w / 2 - px(80), by, px(140), px(32), t(isActive ? 'ui.pet.setRest' : away ? 'ui.pet.away' : 'ui.pet.setActive'), () => {
      pets.setActivePet(isActive ? null : def.id);
    }, { variant: isActive ? 'secondary' : 'success', disabled: !isActive && away, fontSize: 13 });
    const fruit = this.fruitCount();
    const feedBtn = this.host.makeButton(x + w / 2 + px(80), by, px(140), px(32), t('ui.pet.feed', { count: fruit }), () => this.feed(def.id), {
      variant: 'secondary', color: '#9ff6e0', disabled: fruit <= 0 || !pets.canFeed(def.id), fontSize: 13,
    });
    panel.add([actBtn, feedBtn]);
  }

  private fruitCount(): number {
    const inv = this.host.inventory();
    if (!inv) return 0;
    return inv.inventory.filter(i => i.baseId === LEY_FRUIT_ID).reduce((n, i) => n + i.quantity, 0);
  }

  private feed(petId: string): void {
    const pets = this.host.pets();
    const inv = this.host.inventory();
    if (!pets || !inv) return;
    const item = inv.inventory.find(i => i.baseId === LEY_FRUIT_ID);
    if (!item) {
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('sys.pet.noFruit'), type: 'system' });
      return;
    }
    const inst = pets.getPetInstance(petId);
    if (!pets.canFeed(petId)) {
      if (inst) EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('sys.pet.feedFull', { name: pets.getPetDisplayName(inst) }), type: 'system' });
      return;
    }
    inv.removeItem(item.uid, 1);
    pets.feedPet(petId);
    EventBus.emit(GameEvents.INVENTORY_CHANGED, {});
    this.rebuild();
  }
}

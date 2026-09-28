/**
 * EmberTower — the Ember Tower (余烬之塔) in the world. Created by ZoneScene
 * for every regular zone:
 *
 *   - field zones: an 归炉 hearthstone beside each camp; clicking it (once the
 *     tower is unlocked) takes the hero to the tower and remembers where to
 *     bring them back.
 *   - the tower zone: the tower, each wing's plot drawn at its stage (ruin /
 *     restored / thriving), the resting ley-beasts in the meadow and the
 *     return portal.
 *
 * It also runs the homestead's game-side actions (embers from kills and
 * quests, the garden, gem combining, expeditions, blessings) so the scene
 * and the homestead panel only call into one place.
 */
import Phaser from 'phaser';
import { TEXTURE_SCALE, DPR } from '../config';
import { cartToIso } from '../utils/IsometricUtils';
import { EventBus, GameEvents } from '../utils/EventBus';
import { t } from '../i18n';
import { getBuildingName, getItemBaseName } from '../i18n/gameAccessors';
import { SpriteGenerator } from '../graphics/SpriteGenerator';
import { AllQuests } from '../data/quests/all_quests';
import { GEM_STAT_MAP } from '../data/items/bases';
import {
  BUILDINGS, GEM_COMBINE_COUNT, TOWER_UNLOCK_QUEST, TOWER_ZONE_ID,
  buildingStage, embersForKill, embersForQuest, gemCombineBlock, gemCombineGold, nextGemId,
  type GemCombineBlock,
} from '../data/homestead';
import { TOWER_CENTER, TOWER_MEADOW, TOWER_PLOTS, TOWER_PORTAL } from '../data/maps/ember_tower';
import type { ItemInstance, MapData, MonsterDefinition } from '../data/types';
import type { HomesteadSystem } from './HomesteadSystem';
import type { InventorySystem } from './InventorySystem';
import type { QuestSystem } from './QuestSystem';

function fs(basePx: number): string {
  return `${Math.round(basePx * DPR)}px`;
}

/** The part of the pet system the tower needs (PetSystem satisfies it). */
export interface PetRoster {
  pets: readonly { petId: string; level: number }[];
  activePet: string | null;
  addPet(petId: string, opts?: { silent?: boolean }): boolean;
}

export interface EmberTowerHost {
  scene: Phaser.Scene;
  mapId: string;
  mapData: MapData;
  /** A regular zone (not a dungeon floor or sub-dungeon). */
  regularZone: boolean;
  homestead: HomesteadSystem;
  pets: PetRoster;
  quests: QuestSystem;
  inventory: InventorySystem;
  player: () => { tileCol: number; tileRow: number; level: number; gold: number; sprite: { x: number; y: number } };
  spendGold: (n: number) => void;
  addGold: (n: number) => void;
  createItem: (baseId: string) => ItemInstance | null;
  /** Out of combat, alive, no cutscene. */
  isSafe: () => boolean;
  changeZone: (mapId: string, col: number, row: number) => void;
  save: () => void;
  statsChanged: () => void;
  floatText: (x: number, y: number, text: string, color: string) => void;
}

/** Homestead panel pages. */
export type HomesteadPage = 'buildings' | 'garden' | 'workshop' | 'caravan' | 'altar';

/** Which panel page each ally opens. */
const NPC_PAGE: Record<string, HomesteadPage | 'pets'> = {
  tower_elder: 'buildings',
  tower_herbalist: 'garden',
  tower_hermit: 'pets',
  tower_dwarf: 'workshop',
  tower_nomad: 'caravan',
  tower_warden: 'altar',
};

/** Stand-in props until the tower art exists (key → fallback decor). */
const FALLBACK_STAGE = ['decor_collapsed_pillar', 'decor_ruins', 'decor_ancient_statue'];

/** Hearthstone candidates around a camp centre (south side, clear of tents and NPCs). */
const HEARTH_OFFSETS = [[1, 4], [-1, 4], [0, 5], [2, 5], [-2, 5], [4, 3], [-4, 3]];

/** Story beats that hand out a ley-beast, for saves that passed them before the beasts existed. */
const STORY_PETS: { quest: string; petId: string }[] = [
  { quest: 'q_kill_werewolf_alpha', petId: 'pet_storm_wolf' },
  { quest: 'q_seal_fire_rift', petId: 'pet_phoenix' },
];

interface MeadowPet { petId: string; sprite: Phaser.GameObjects.Container; col: number; row: number; nextMove: number }

export class EmberTower {
  private readonly h: EmberTowerHost;
  private readonly objects: Phaser.GameObjects.GameObject[] = [];
  private plotObjects: Phaser.GameObjects.GameObject[] = [];
  private hearths: { col: number; row: number; glow: Phaser.GameObjects.GameObject | null; label: Phaser.GameObjects.Text }[] = [];
  private towerImage: Phaser.GameObjects.Image | null = null;
  private meadow: MeadowPet[] = [];
  private portalArmed = false;
  private gardenLabel: Phaser.GameObjects.Text | null = null;
  private readonly onTurnedIn = (d: { questId: string }): void => this.handleQuestTurnedIn(d.questId);
  private readonly onUpgraded = (): void => this.drawPlots();

  constructor(host: EmberTowerHost) {
    this.h = host;
    this.syncUnlocks();
    if (this.inTower) {
      if (this.h.homestead.tower.onEnterTower()) {
        EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('homestead.log.blessingHome'), type: 'system' });
        this.h.statsChanged();
      }
      this.buildTower();
    } else if (host.regularZone) {
      this.buildHearths();
    }
    EventBus.on(GameEvents.QUEST_TURNED_IN, this.onTurnedIn);
    EventBus.on(GameEvents.HOMESTEAD_UPGRADED, this.onUpgraded);
  }

  static isTowerZone(mapId: string): boolean {
    return mapId === TOWER_ZONE_ID;
  }

  /** Tower allies stand in the tower only once their wing is unlocked (the elder is always home). */
  static npcPresent(mapId: string, npcId: string, homestead: HomesteadSystem): boolean {
    if (mapId !== TOWER_ZONE_ID || npcId === 'tower_elder') return true;
    const b = BUILDINGS.find(x => x.allyNpc === npcId);
    return !b || homestead.tower.isBuildingUnlocked(b.id);
  }

  get inTower(): boolean {
    return this.h.mapId === TOWER_ZONE_ID;
  }

  private turnedInQuests(): string[] {
    const out: string[] = [];
    for (const [id, p] of this.h.quests.progress.entries()) if (p.status === 'turned_in') out.push(id);
    return out;
  }

  private syncUnlocks(): string[] {
    return this.h.homestead.tower.syncUnlocks(this.turnedInQuests());
  }

  /** Grant the story's ley-beasts to saves that passed their beats before they existed. */
  retroGrantPets(): void {
    const done = new Set(this.turnedInQuests());
    for (const s of STORY_PETS) {
      if (done.has(s.quest) && !this.h.pets.pets.some(p => p.petId === s.petId)) this.h.pets.addPet(s.petId);
    }
  }

  // ── Entering / leaving ──────────────────────────────────────

  /** Why the hero can't go to the tower right now (null: they can). */
  enterBlock(): 'locked' | 'here' | 'unsafe' | null {
    if (!this.h.homestead.tower.towerUnlocked) return 'locked';
    if (this.inTower) return 'here';
    if (!this.h.regularZone || !this.h.isSafe()) return 'unsafe';
    return null;
  }

  enterTower(): boolean {
    const block = this.enterBlock();
    if (block) {
      if (block !== 'here') EventBus.emit(GameEvents.LOG_MESSAGE, { text: t(`homestead.enter.${block}`), type: 'system' });
      return false;
    }
    const p = this.h.player();
    this.h.homestead.tower.towerReturn = { mapId: this.h.mapId, col: Math.round(p.tileCol), row: Math.round(p.tileRow) };
    const start = this.h.mapData.id === TOWER_ZONE_ID ? this.h.mapData.playerStart : { col: 24, row: 41 };
    this.h.changeZone(TOWER_ZONE_ID, start.col, start.row);
    return true;
  }

  leaveTower(): void {
    const r = this.h.homestead.tower.towerReturn ?? { mapId: 'emerald_plains', col: 15, row: 22 };
    this.h.changeZone(r.mapId, r.col, r.row);
  }

  // ── Kills, quests, time ─────────────────────────────────────

  onKill(def: MonsterDefinition, eliteAffixCount: number, at: { x: number; y: number }): void {
    const tower = this.h.homestead.tower;
    const embers = tower.addEmbers(embersForKill(def, eliteAffixCount));
    if (embers > 0) {
      this.h.floatText(at.x - 18, at.y - 52, t('homestead.float.embers', { n: embers }), '#ff9a4a');
    }
    tower.onKillGarden();
    if (tower.onKillExpedition()) this.announceExpeditionBack();
  }

  private handleQuestTurnedIn(questId: string): void {
    const tower = this.h.homestead.tower;
    const quest = AllQuests.find(q => q.id === questId);
    if (quest) {
      const n = tower.addEmbers(embersForQuest(quest));
      if (n > 0) EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('homestead.log.questEmbers', { n }), type: 'loot' });
    }
    const wasOpen = tower.towerUnlocked;
    for (const id of this.syncUnlocks()) {
      const def = BUILDINGS.find(b => b.id === id);
      EventBus.emit(GameEvents.LOG_MESSAGE, {
        text: t('homestead.log.wingUnlocked', { name: getBuildingName(id, def?.name ?? id) }),
        type: 'system',
      });
    }
    if (!wasOpen && questId === TOWER_UNLOCK_QUEST && tower.towerUnlocked) {
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('homestead.log.towerUnlocked'), type: 'system' });
      this.drawHearthLabels();
    }
  }

  /** Advance play time (the scene skips this during cutscenes). */
  tick(deltaMs: number): void {
    const tower = this.h.homestead.tower;
    const r = tower.tick(this.inTower ? 0 : deltaMs);
    if (this.inTower && tower.expedition && !tower.expeditionDone) {
      // Expeditions keep travelling while the hero rests at home.
      tower.expedition.remainingMs = Math.max(0, tower.expedition.remainingMs - deltaMs);
      if (tower.expeditionDone) this.announceExpeditionBack();
    }
    if (r.blessingEnded) {
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('homestead.log.blessingFaded'), type: 'system' });
      this.h.statsChanged();
    }
    if (r.expeditionReturned) this.announceExpeditionBack();
  }

  private announceExpeditionBack(): void {
    EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('homestead.log.expeditionBack'), type: 'system' });
  }

  // ── Panel actions ───────────────────────────────────────────

  /** Harvest the garden into the bag (only in the tower). Returns items taken. */
  harvestGarden(): number {
    if (!this.inTower) return 0;
    const tower = this.h.homestead.tower;
    let taken = 0;
    for (const [id, count] of Object.entries(tower.harvest())) {
      let left = count;
      while (left > 0) {
        const item = this.h.createItem(id);
        if (!item) break;
        item.quantity = 1;
        if (!this.h.inventory.addItem(item)) break;
        left--;
        taken++;
      }
      tower.returnToGarden(id, left);
    }
    if (taken > 0) {
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('homestead.log.harvested', { n: taken }), type: 'loot' });
      EventBus.emit(GameEvents.INVENTORY_CHANGED, {});
      this.h.save();
    }
    this.drawGardenLabel();
    return taken;
  }

  /** Gem stacks in the bag, merged by base id. */
  gemCounts(): Record<string, number> {
    const out: Record<string, number> = {};
    for (const it of this.h.inventory.inventory) {
      if (GEM_STAT_MAP[it.baseId]) out[it.baseId] = (out[it.baseId] ?? 0) + it.quantity;
    }
    return out;
  }

  gemBlock(gemId: string): GemCombineBlock {
    const p = this.h.player();
    return gemCombineBlock(gemId, this.gemCounts()[gemId] ?? 0, this.h.homestead.getBuildingLevel('gem_workshop'), p.level, p.gold);
  }

  /** Combine three gems into one of the next tier (in the tower). */
  combineGem(gemId: string): boolean {
    if (!this.inTower || !this.h.homestead.tower.isBuildingUnlocked('gem_workshop') || this.gemBlock(gemId)) return false;
    const next = nextGemId(gemId)!;
    const made = this.h.createItem(next);
    if (!made) return false;
    let need = GEM_COMBINE_COUNT;
    for (const it of [...this.h.inventory.inventory]) {
      if (need <= 0) break;
      if (it.baseId !== gemId) continue;
      const take = Math.min(need, it.quantity);
      this.h.inventory.removeItem(it.uid, take);
      need -= take;
    }
    this.h.spendGold(gemCombineGold(GEM_STAT_MAP[next].tier));
    made.quantity = 1;
    if (!this.h.inventory.addItem(made)) this.h.inventory.stash.push(made);
    EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('homestead.log.gemCombined', { name: getItemBaseName(next) }), type: 'loot' });
    EventBus.emit(GameEvents.INVENTORY_CHANGED, {});
    this.h.save();
    return true;
  }

  /** Pets that could go on an expedition: owned, not fighting, not already away. */
  idlePets(): string[] {
    const tower = this.h.homestead.tower;
    return this.h.pets.pets.map(p => p.petId).filter(id => id !== this.h.pets.activePet && !tower.isPetAway(id));
  }

  sendExpedition(petId: string, optionId: string): boolean {
    if (!this.inTower) return false;
    const ok = this.h.homestead.tower.sendExpedition(petId, optionId, this.h.pets.pets.map(p => p.petId), this.h.pets.activePet);
    if (ok) {
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('homestead.log.expeditionSent'), type: 'system' });
      this.drawMeadow();
      this.h.save();
    }
    return ok;
  }

  claimExpedition(): boolean {
    if (!this.inTower) return false;
    const p = this.h.player();
    const r = this.h.homestead.tower.claimExpedition(p.level);
    if (!r) return false;
    this.h.addGold(r.gold);
    for (const it of r.items) {
      for (let i = 0; i < it.count; i++) {
        const item = this.h.createItem(it.itemId);
        if (!item) continue;
        item.quantity = 1;
        if (!this.h.inventory.addItem(item)) this.h.inventory.stash.push(item);
      }
    }
    EventBus.emit(GameEvents.LOG_MESSAGE, {
      text: t('homestead.log.expeditionClaimed', { embers: r.embers, gold: r.gold }), type: 'loot',
    });
    EventBus.emit(GameEvents.INVENTORY_CHANGED, {});
    this.drawMeadow();
    this.h.save();
    return true;
  }

  buyBlessing(id: string): boolean {
    if (!this.inTower || !this.h.homestead.tower.buyBlessing(id)) return false;
    EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('homestead.log.blessed', { name: t(`homestead.blessing.${id}.name`) }), type: 'system' });
    this.h.statsChanged();
    this.h.save();
    return true;
  }

  upgradeBuilding(id: string): boolean {
    const p = this.h.player();
    if (!this.h.homestead.canUpgrade(id, p.gold)) return false;
    const gold = this.h.homestead.upgrade(id);
    this.h.spendGold(gold);
    this.h.save();
    return true;
  }

  // ── Interaction ─────────────────────────────────────────────

  /** A tower ally was clicked: open their page. Returns true if handled. */
  interactNpc(npcId: string): boolean {
    const page = NPC_PAGE[npcId];
    if (!page || !this.inTower) return false;
    if (page === 'garden' && this.h.homestead.tower.gardenStockCount() > 0) this.harvestGarden();
    if (page === 'pets') {
      EventBus.emit(GameEvents.UI_TOGGLE_PANEL, { panel: 'pets' });
      return true;
    }
    EventBus.emit(GameEvents.UI_TOGGLE_PANEL, { panel: 'homestead', page });
    return true;
  }

  /** A click on the ground: hearthstone, portal or a wing's plot. Returns true if handled. */
  handleClick(col: number, row: number): boolean {
    const p = this.h.player();
    const near = (c: number, r: number, reach = 3) => (p.tileCol - c) ** 2 + (p.tileRow - r) ** 2 <= reach * reach;
    const on = (c: number, r: number, size = 1.6) => Math.abs(col - c) <= size && Math.abs(row - r) <= size;
    for (const hs of this.hearths) {
      if (on(hs.col, hs.row) && near(hs.col, hs.row)) { this.enterTower(); return true; }
    }
    if (!this.inTower) return false;
    if (on(TOWER_PORTAL.col, TOWER_PORTAL.row) && near(TOWER_PORTAL.col, TOWER_PORTAL.row)) { this.leaveTower(); return true; }
    for (const [id, plot] of Object.entries(TOWER_PLOTS)) {
      if (!on(plot.col, plot.row, 1.5) || !near(plot.col, plot.row, 4)) continue;
      const page: HomesteadPage = id === 'herb_garden' ? 'garden' : id === 'gem_workshop' ? 'workshop'
        : id === 'training_ground' ? 'caravan' : id === 'altar' ? 'altar' : 'buildings';
      if (page === 'garden' && this.h.homestead.tower.gardenStockCount() > 0) this.harvestGarden();
      EventBus.emit(GameEvents.UI_TOGGLE_PANEL, { panel: 'homestead', page });
      return true;
    }
    return false;
  }

  // ── Per frame ───────────────────────────────────────────────

  update(time: number): void {
    const p = this.h.player();
    if (this.inTower) {
      // Step into the portal ring to go back (armed once the hero has stepped away from it).
      const d2 = (p.tileCol - TOWER_PORTAL.col) ** 2 + (p.tileRow - TOWER_PORTAL.row) ** 2;
      if (d2 > 6) this.portalArmed = true;
      else if (this.portalArmed && d2 < 1.1 && this.h.isSafe()) { this.portalArmed = false; this.leaveTower(); return; }
      // Fade the tower when the hero walks behind it.
      if (this.towerImage) {
        const img = this.towerImage;
        const behind = p.sprite.y < img.y - 20 && p.sprite.y > img.y - img.displayHeight * img.originY + 20
          && Math.abs(p.sprite.x - img.x) < img.displayWidth * 0.4;
        const target = behind ? 0.35 : 1;
        if (Math.abs(img.alpha - target) > 0.01) img.setAlpha(img.alpha + (target - img.alpha) * 0.15);
      }
      this.updateMeadow(time);
    } else {
      for (const hs of this.hearths) {
        const near = (p.tileCol - hs.col) ** 2 + (p.tileRow - hs.row) ** 2 <= 9;
        hs.label.setAlpha(near ? 1 : 0.75);
      }
    }
  }

  // ── Visuals ─────────────────────────────────────────────────

  /** Place a prop by its ground line; falls back to `fallback` until the art exists. */
  private placeProp(col: number, row: number, key: string, fallback: string, scale = 1): Phaser.GameObjects.Image | null {
    const scene = this.h.scene;
    SpriteGenerator.ensureDecoration(scene, key);
    let tex = key;
    let s = scale;
    if (!scene.textures.exists(tex)) {
      SpriteGenerator.ensureDecoration(scene, fallback);
      tex = fallback;
      if (!scene.textures.exists(tex)) return null;
    } else {
      s = 1;
    }
    const meta = SpriteGenerator.getDecorMeta(tex);
    const { x, y } = cartToIso(col, row);
    const img = scene.add.image(x, y, tex).setOrigin(0.5, meta ? meta.anchorY : 0.85).setScale(s / TEXTURE_SCALE);
    img.setDepth(meta?.flat ? y + 5 : y + 70);
    const anim = SpriteGenerator.getLoopAnimKey(scene, tex);
    if (anim) {
      const sprite = scene.add.sprite(img.x, img.y, tex).setOrigin(img.originX, img.originY).setScale(img.scaleX).setDepth(img.depth);
      sprite.play(anim);
      img.destroy();
      return sprite as unknown as Phaser.GameObjects.Image;
    }
    return img;
  }

  private label(col: number, row: number, text: string, color: string, dy: number, size = 11): Phaser.GameObjects.Text {
    const { x, y } = cartToIso(col, row);
    return this.h.scene.add.text(x, y + dy, text, {
      fontSize: fs(size), color, fontFamily: '"Noto Sans SC", sans-serif', fontStyle: 'bold',
      stroke: '#000000', strokeThickness: Math.round(3 * DPR),
    }).setOrigin(0.5).setDepth(y + 200);
  }

  private buildHearths(): void {
    const map = this.h.mapData;
    for (const camp of map.camps) {
      const spot = HEARTH_OFFSETS.map(([dc, dr]) => ({ col: camp.col + dc, row: camp.row + dr }))
        .find(s => map.collisions[s.row]?.[s.col]);
      if (!spot) continue;
      const img = this.placeProp(spot.col, spot.row, 'decor_tower_hearthstone', 'decor_ritual_circle', 0.7);
      if (img) this.objects.push(img);
      const { x, y } = cartToIso(spot.col, spot.row);
      const glow = this.h.scene.add.ellipse(x, y - 4, 34, 14, 0xff8a3a, 0.25).setDepth(y + 4);
      this.h.scene.tweens.add({ targets: glow, alpha: 0.55, duration: 1100, yoyo: true, repeat: -1, ease: 'Sine.easeInOut' });
      this.objects.push(glow);
      const label = this.label(spot.col, spot.row, '', '#ffb070', -46, 10);
      this.objects.push(label);
      this.hearths.push({ col: spot.col, row: spot.row, glow, label });
    }
    this.drawHearthLabels();
  }

  private drawHearthLabels(): void {
    const open = this.h.homestead.tower.towerUnlocked;
    for (const hs of this.hearths) {
      hs.label.setText(t(open ? 'homestead.hearth.label' : 'homestead.hearth.dormant')).setColor(open ? '#ffb070' : '#9a8f86');
    }
  }

  private buildTower(): void {
    const tower = this.placeProp(TOWER_CENTER.col, TOWER_CENTER.row, 'decor_tower_main', 'decor_ruins', 2.6);
    if (tower) { this.objects.push(tower); this.towerImage = tower; }
    const portal = this.placeProp(TOWER_PORTAL.col, TOWER_PORTAL.row, 'decor_tower_portal', 'decor_ritual_circle', 1.1);
    if (portal) this.objects.push(portal);
    this.objects.push(this.label(TOWER_PORTAL.col, TOWER_PORTAL.row, t('homestead.portal.label'), '#9fd8ff', -30, 10));
    this.drawPlots();
    this.drawMeadow();
  }

  /** (Re)draw every wing at its stage, with its name and level. */
  private drawPlots(): void {
    if (!this.inTower) return;
    for (const o of this.plotObjects) o.destroy();
    this.plotObjects = [];
    const hs = this.h.homestead;
    for (const def of BUILDINGS) {
      const plot = TOWER_PLOTS[def.id];
      if (!plot) continue;
      const lv = hs.getBuildingLevel(def.id);
      const unlocked = hs.tower.isBuildingUnlocked(def.id);
      const stage = buildingStage(def, lv, unlocked);
      const img = this.placeProp(plot.col, plot.row, `decor_tower_${def.id}_${stage}`, FALLBACK_STAGE[stage], stage === 0 ? 1 : 1.2);
      if (img) this.plotObjects.push(img);
      const name = getBuildingName(def.id, def.name);
      const text = unlocked ? `${name}  Lv.${lv}` : `${name} · ${t('homestead.plot.ruin')}`;
      this.plotObjects.push(this.label(plot.col, plot.row, text, unlocked ? '#ffe2a8' : '#8f8a86', stage === 0 ? -58 : -96, 10));
    }
    this.drawGardenLabel();
  }

  private drawGardenLabel(): void {
    if (!this.inTower) return;
    this.gardenLabel?.destroy();
    this.gardenLabel = null;
    const n = this.h.homestead.tower.gardenStockCount();
    if (n <= 0) return;
    const plot = TOWER_PLOTS.herb_garden;
    this.gardenLabel = this.label(plot.col, plot.row, t('homestead.plot.harvest', { n }), '#9dff8a', -112, 10);
    this.h.scene.tweens.add({ targets: this.gardenLabel, y: this.gardenLabel.y - 4, duration: 700, yoyo: true, repeat: -1, ease: 'Sine.easeInOut' });
  }

  /** Owned ley-beasts that are neither fighting nor away rest in the meadow. */
  private drawMeadow(): void {
    if (!this.inTower) return;
    for (const m of this.meadow) m.sprite.destroy();
    this.meadow = [];
    const map = this.h.mapData;
    const scene = this.h.scene;
    this.idlePets().forEach((petId, i) => {
      const a = (i / Math.max(1, this.h.pets.pets.length)) * Math.PI * 2;
      let col = Math.round(TOWER_MEADOW.col + Math.cos(a) * 2);
      let row = Math.round(TOWER_MEADOW.row + Math.sin(a) * 2);
      if (!map.collisions[row]?.[col]) { col = TOWER_MEADOW.col; row = TOWER_MEADOW.row; }
      const { x, y } = cartToIso(col, row);
      const c = scene.add.container(x, y).setDepth(y + 50);
      const key = `decor_pet_${petId}`;
      SpriteGenerator.ensureDecoration(scene, key);
      c.add(scene.add.ellipse(0, 3, 18, 7, 0x000000, 0.22));
      if (scene.textures.exists(key)) {
        const img = scene.add.image(0, -18, key).setScale(0.9 / TEXTURE_SCALE);
        c.add(img);
        scene.tweens.add({ targets: img, y: -21, duration: 900 + i * 130, yoyo: true, repeat: -1, ease: 'Sine.easeInOut' });
      } else {
        c.add(scene.add.circle(0, -10, 8, 0x9fd8a0));
      }
      this.meadow.push({ petId, sprite: c, col, row, nextMove: scene.time.now + 1500 + i * 700 });
    });
  }

  private updateMeadow(time: number): void {
    const map = this.h.mapData;
    for (const m of this.meadow) {
      if (time < m.nextMove) continue;
      m.nextMove = time + 2500 + Math.random() * 3500;
      const col = m.col + Math.round((Math.random() - 0.5) * 4);
      const row = m.row + Math.round((Math.random() - 0.5) * 4);
      if (Math.hypot(col - TOWER_MEADOW.col, row - TOWER_MEADOW.row) > TOWER_MEADOW.radius || !map.collisions[row]?.[col]) continue;
      m.col = col; m.row = row;
      const { x, y } = cartToIso(col, row);
      const body = m.sprite.list[1] as Phaser.GameObjects.Image | undefined;
      if (body && 'setFlipX' in body) body.setFlipX(x < m.sprite.x);
      this.h.scene.tweens.add({
        targets: m.sprite, x, y, duration: 1400, ease: 'Sine.easeInOut',
        onUpdate: () => m.sprite.setDepth(m.sprite.y + 50),
      });
    }
  }

  destroy(): void {
    EventBus.off(GameEvents.QUEST_TURNED_IN, this.onTurnedIn);
    EventBus.off(GameEvents.HOMESTEAD_UPGRADED, this.onUpgraded);
    for (const o of [...this.objects, ...this.plotObjects]) o.destroy();
    for (const m of this.meadow) m.sprite.destroy();
    this.gardenLabel?.destroy();
    this.objects.length = 0;
    this.plotObjects = [];
    this.meadow = [];
    this.hearths = [];
    this.towerImage = null;
  }
}

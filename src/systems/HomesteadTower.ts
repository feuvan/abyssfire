/**
 * HomesteadTower — the Ember Tower's state beside the building levels:
 * the embers purse, which wings the story has unlocked, the herb garden's
 * stock, the caravan expedition and the altar blessing. Pure (no Phaser);
 * owned by HomesteadSystem (`homestead.tower`) and saved in `SaveData.homestead`.
 */
import type { EquipStats } from './CombatSystem';
import type { HomesteadBlessing, HomesteadExpedition, HomesteadSaveExtras } from '../data/types';
import {
  BLESSING_DURATION_MS, BLESSINGS, BUILDINGS, EXPEDITION_OPTIONS, TOWER_UNLOCK_QUEST,
  blessingCost, blessingStats, gardenCapacity, gardenInterval, rollExpeditionReward, rollGardenYield,
  type ExpeditionReward,
} from '../data/homestead';

export interface TowerHost {
  buildings: Record<string, number>;
}

export type ExpeditionBlock = 'locked' | 'busy' | 'noPet' | 'active' | 'option' | null;
export type BlessingBlock = 'locked' | 'embers' | 'unknown' | null;

export class HomesteadTower {
  embers = 0;
  /** Main quests turned in (drives the story unlocks). */
  private turnedIn = new Set<string>();
  garden: { progress: number; stock: Record<string, number> } = { progress: 0, stock: {} };
  expedition: HomesteadExpedition | null = null;
  blessing: HomesteadBlessing | null = null;
  /** Where the tower's return portal sends the hero. */
  towerReturn: { mapId: string; col: number; row: number } | null = null;

  private readonly host: TowerHost;

  constructor(host: TowerHost) {
    this.host = host;
  }

  reset(): void {
    this.embers = 0;
    this.turnedIn = new Set();
    this.garden = { progress: 0, stock: {} };
    this.expedition = null;
    this.blessing = null;
    this.towerReturn = null;
  }

  private level(id: string): number {
    return this.host.buildings[id] ?? 0;
  }

  // ── Unlocks ─────────────────────────────────────────────────

  /**
   * Feed the set of turned-in quests (on load and after each turn-in). A wing
   * whose ally has just arrived is restored to level 1 for free. Returns the
   * wings newly unlocked by this call.
   */
  syncUnlocks(turnedIn: Iterable<string>): string[] {
    const before = new Set(BUILDINGS.filter(b => this.isBuildingUnlocked(b.id)).map(b => b.id));
    this.turnedIn = new Set(turnedIn);
    const fresh: string[] = [];
    for (const b of BUILDINGS) {
      if (!b.unlockQuest || !this.isBuildingUnlocked(b.id)) continue;
      if (this.level(b.id) <= 0) this.host.buildings[b.id] = 1;
      if (!before.has(b.id)) fresh.push(b.id);
    }
    return fresh;
  }

  get towerUnlocked(): boolean {
    return this.turnedIn.has(TOWER_UNLOCK_QUEST);
  }

  isBuildingUnlocked(id: string): boolean {
    const def = BUILDINGS.find(b => b.id === id);
    if (!def) return false;
    return !def.unlockQuest || this.turnedIn.has(def.unlockQuest);
  }

  // ── Embers ──────────────────────────────────────────────────

  addEmbers(n: number): number {
    const add = Math.max(0, Math.floor(n));
    this.embers += add;
    return add;
  }

  // ── Herb garden ─────────────────────────────────────────────

  gardenStockCount(): number {
    return Object.values(this.garden.stock).reduce((a, b) => a + b, 0);
  }

  /** A kill anywhere feeds the garden; returns the item grown this kill, if any. */
  onKillGarden(rng: () => number = Math.random): string | null {
    const lv = this.level('herb_garden');
    if (lv <= 0 || !this.isBuildingUnlocked('herb_garden')) return null;
    if (this.gardenStockCount() >= gardenCapacity(lv)) return null;
    this.garden.progress++;
    if (this.garden.progress < gardenInterval(lv)) return null;
    this.garden.progress = 0;
    const id = rollGardenYield(lv, rng);
    this.garden.stock[id] = (this.garden.stock[id] ?? 0) + 1;
    return id;
  }

  /** Empty the garden (the caller hands back what did not fit via returnToGarden). */
  harvest(): Record<string, number> {
    const out = this.garden.stock;
    this.garden.stock = {};
    return out;
  }

  returnToGarden(itemId: string, count: number): void {
    if (count > 0) this.garden.stock[itemId] = (this.garden.stock[itemId] ?? 0) + count;
  }

  // ── Caravan expedition ──────────────────────────────────────

  expeditionBlock(petId: string | null, ownedPets: readonly string[], activePet: string | null, optionId: string): ExpeditionBlock {
    if (this.level('training_ground') <= 0 || !this.isBuildingUnlocked('training_ground')) return 'locked';
    if (this.expedition) return 'busy';
    if (!EXPEDITION_OPTIONS.some(o => o.id === optionId)) return 'option';
    if (!petId || !ownedPets.includes(petId)) return 'noPet';
    if (petId === activePet) return 'active';
    return null;
  }

  sendExpedition(petId: string, optionId: string, ownedPets: readonly string[], activePet: string | null): boolean {
    if (this.expeditionBlock(petId, ownedPets, activePet, optionId)) return false;
    const opt = EXPEDITION_OPTIONS.find(o => o.id === optionId)!;
    this.expedition = { petId, optionId, kills: 0, killsRequired: opt.killsRequired, remainingMs: opt.durationMs };
    return true;
  }

  isPetAway(petId: string): boolean {
    return this.expedition?.petId === petId;
  }

  get expeditionDone(): boolean {
    const e = this.expedition;
    return !!e && (e.kills >= e.killsRequired || e.remainingMs <= 0);
  }

  /** Bring the pet home and roll what it found (null while it is still out). */
  claimExpedition(playerLevel: number, rng: () => number = Math.random): (ExpeditionReward & { petId: string }) | null {
    const e = this.expedition;
    if (!e || !this.expeditionDone) return null;
    const reward = rollExpeditionReward(e.optionId, this.level('training_ground'), playerLevel, rng);
    this.expedition = null;
    this.embers += reward.embers;
    return { ...reward, petId: e.petId };
  }

  // ── Altar blessing ──────────────────────────────────────────

  blessingBlock(id: string): BlessingBlock {
    const lv = this.level('altar');
    if (lv <= 0 || !this.isBuildingUnlocked('altar')) return 'locked';
    if (!BLESSINGS.some(b => b.id === id)) return 'unknown';
    if (this.embers < blessingCost(lv)) return 'embers';
    return null;
  }

  /** Spend embers on a blessing (replaces the current one). */
  buyBlessing(id: string): boolean {
    if (this.blessingBlock(id)) return false;
    const lv = this.level('altar');
    this.embers -= blessingCost(lv);
    this.blessing = { id, level: lv, remainingMs: BLESSING_DURATION_MS };
    return true;
  }

  blessingStats(): Partial<EquipStats> {
    return this.blessing ? blessingStats(this.blessing.id, this.blessing.level) : {};
  }

  // ── Time and kills ──────────────────────────────────────────

  /**
   * Advance play time outside the tower. Returns what changed so the scene can
   * announce it (the blessing faded, the expedition came back).
   */
  tick(deltaMs: number): { blessingEnded: boolean; expeditionReturned: boolean } {
    let blessingEnded = false;
    let expeditionReturned = false;
    if (this.blessing) {
      this.blessing.remainingMs -= deltaMs;
      if (this.blessing.remainingMs <= 0) { this.blessing = null; blessingEnded = true; }
    }
    if (this.expedition && !this.expeditionDone) {
      this.expedition.remainingMs = Math.max(0, this.expedition.remainingMs - deltaMs);
      expeditionReturned = this.expeditionDone;
    }
    return { blessingEnded, expeditionReturned };
  }

  /** A kill counts toward the expedition; returns true when this kill brought it home. */
  onKillExpedition(): boolean {
    if (!this.expedition || this.expeditionDone) return false;
    this.expedition.kills++;
    return this.expeditionDone;
  }

  /** Entering the tower ends the blessing ("lasts until your next return"). */
  onEnterTower(): boolean {
    const had = !!this.blessing;
    this.blessing = null;
    return had;
  }

  // ── Save ────────────────────────────────────────────────────

  toSave(): Required<HomesteadSaveExtras> {
    return {
      embers: this.embers,
      garden: { progress: this.garden.progress, stock: { ...this.garden.stock } },
      expedition: this.expedition ? { ...this.expedition } : null,
      blessing: this.blessing ? { ...this.blessing } : null,
      towerReturn: this.towerReturn ? { ...this.towerReturn } : null,
    };
  }

  /** Load from a save; every field is optional (saves from before the tower get safe defaults). */
  load(data: HomesteadSaveExtras | undefined): void {
    this.reset();
    if (!data) return;
    this.embers = num(data.embers, 0);
    const g = data.garden;
    if (g && typeof g === 'object') {
      this.garden.progress = num(g.progress, 0);
      for (const [k, v] of Object.entries(g.stock ?? {})) {
        const n = num(v, 0);
        if (n > 0) this.garden.stock[k] = n;
      }
    }
    const e = data.expedition;
    if (e && typeof e.petId === 'string' && EXPEDITION_OPTIONS.some(o => o.id === e.optionId)) {
      this.expedition = {
        petId: e.petId, optionId: e.optionId,
        kills: num(e.kills, 0), killsRequired: Math.max(1, num(e.killsRequired, 1)), remainingMs: num(e.remainingMs, 0),
      };
    }
    const b = data.blessing;
    if (b && BLESSINGS.some(d => d.id === b.id) && num(b.remainingMs, 0) > 0) {
      this.blessing = { id: b.id, level: Math.max(1, num(b.level, 1)), remainingMs: num(b.remainingMs, 0) };
    }
    const r = data.towerReturn;
    if (r && typeof r.mapId === 'string') this.towerReturn = { mapId: r.mapId, col: num(r.col, 0), row: num(r.row, 0) };
  }
}

function num(v: unknown, fallback: number): number {
  return typeof v === 'number' && Number.isFinite(v) ? Math.max(0, v) : fallback;
}

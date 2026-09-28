/**
 * 灵兽 (ley-beasts): ownership, growth, bond, passive bonuses and the pure combat
 * decision logic. The Phaser side (sprite, following, playing abilities) lives in
 * PetCompanion; this module has no scene dependency so it can be unit tested.
 *
 * Growth (docs/homestead-pets.md §三):
 *  - Level 1–20 from kills while active (and resting exp for the others once the
 *    tower's 月井 `pet_house` is built). 灵脉果 (`c_ley_fruit`) speeds it up.
 *  - Evolution at 10 (觉醒) and 20 (至尊): passive ×1.5 / ×2, second ability at 觉醒.
 *  - Bond 0–5 from active time, kills and feeding. Bond scales the passive; at max
 *    bond the beast fires its signature ability once when the hero drops below 30% HP.
 */
import { EventBus, GameEvents } from '../utils/EventBus';
import { t } from '../i18n';
import type { SaveData } from '../data/types';
import {
  PETS, PET_MAX_LEVEL, PET_EVOLUTION_LEVELS, PET_EVOLUTION_MULT, PET_MAX_BOND,
  getPetDef, unlockedAbilities, petNameKey, type PetDef, type PetAbilityDef,
} from '../data/pets';

export interface PetInstance {
  petId: string;
  level: number;
  exp: number;
  /** Evolution stage: 0, 1 (觉醒), 2 (至尊). */
  evolved: number;
  /** 0–5. */
  bond: number;
  /** 0–99 towards the next bond level. */
  bondProgress: number;
}

export interface PetSaveData {
  owned: PetInstance[];
  active: string | null;
}

/** Base pet attack as a fraction of hero damage (kept from the old pet formula). */
export const PET_DAMAGE_BASE_FRACTION = 0.05;
export const PET_DAMAGE_PER_LEVEL_FRACTION = 0.005;
/** Cap on the level-based fraction (5% + 20 × 0.5% = 15%). */
export const PET_DAMAGE_MAX_FRACTION = 0.15;

/** Bond progress needed per bond level. */
export const BOND_PROGRESS_PER_LEVEL = 100;
/** Bond progress per kill with the pet active / per minute active / per fruit. */
export const BOND_PER_KILL = 1;
export const BOND_PER_ACTIVE_MINUTE = 2;
export const BOND_PER_FEED = 20;
/** Exp granted by one 灵脉果. */
export const FEED_EXP = 120;
/** Max-bond emergency trigger: hero HP threshold and cooldown. */
export const BOND_RESCUE_HP = 0.3;
export const BOND_RESCUE_COOLDOWN_MS = 60000;
/** Bond cap without the 月井; each 月井 level raises it by one up to PET_MAX_BOND. */
export const BASE_BOND_CAP = 3;

/** Exp to go from `level` to `level + 1`. */
export function petExpToNext(level: number): number {
  return 60 + 40 * level;
}

/** Exp a kill gives the active beast. */
export function petKillExp(monsterLevel: number): number {
  return 10 + Math.max(0, Math.floor(monsterLevel));
}

/** Evolution stage for a level. */
export function evolutionForLevel(level: number): number {
  let stage = 0;
  for (const lv of PET_EVOLUTION_LEVELS) if (level >= lv) stage++;
  return stage;
}

export function bondMultiplier(bond: number): number {
  return 1 + 0.1 * Math.max(0, Math.min(PET_MAX_BOND, bond));
}

/** Passive value of a beast at its level / evolution / bond. */
export function petPassiveValue(def: PetDef, pet: Pick<PetInstance, 'level' | 'evolved' | 'bond'>): number {
  const base = def.passive.base + def.passive.perLevel * (Math.max(1, pet.level) - 1);
  const evo = PET_EVOLUTION_MULT[Math.max(0, Math.min(PET_EVOLUTION_MULT.length - 1, pet.evolved))] ?? 1;
  return Math.round(base * evo * bondMultiplier(pet.bond) * 10) / 10;
}

/** Pet basic-attack damage from the hero's damage (level fraction capped, × evolution). */
export function petAttackDamage(playerDamage: number, pet: Pick<PetInstance, 'level' | 'evolved'>): number {
  const fraction = Math.min(
    PET_DAMAGE_MAX_FRACTION,
    PET_DAMAGE_BASE_FRACTION + pet.level * PET_DAMAGE_PER_LEVEL_FRACTION,
  );
  const evo = PET_EVOLUTION_MULT[Math.max(0, Math.min(PET_EVOLUTION_MULT.length - 1, pet.evolved))] ?? 1;
  const raw = Math.floor(playerDamage * fraction * evo);
  return raw > 0 ? Math.max(1, raw) : 0;
}

/** Chance a monster kill drops a 灵脉果. */
export function leyFruitDropChance(elite: boolean): number {
  return elite ? 0.12 : 0.015;
}

/** Sum two bonus maps (e.g. homestead buildings + active beast). */
export function mergeBonuses(a: Record<string, number>, b: Record<string, number>): Record<string, number> {
  const out = { ...a };
  for (const [k, v] of Object.entries(b)) out[k] = (out[k] ?? 0) + v;
  return out;
}

// ─── Combat decision (pure) ────────────────────────────────────────────────

export type PetAction =
  | { type: 'ability'; ability: PetAbilityDef }
  | { type: 'attack' }
  | { type: 'approach' }
  | { type: 'follow' }
  | { type: 'rest' };

export interface PetDecisionContext {
  now: number;
  /** Unlocked abilities in priority order. */
  abilities: PetAbilityDef[];
  /** Ability id → time it is ready again. */
  readyAt: Record<string, number>;
  /** Pet is exhausted (0 HP) or the world is paused. */
  exhausted: boolean;
  /** The pet should stay out of fights (safe zone / camp). */
  peaceful: boolean;
  /** Distance pet → hero (tiles). */
  heroDist: number;
  heroHpRatio: number;
  /** Monsters currently swinging at the hero. */
  heroAttackers: number;
  /** Distance pet → target (tiles), null without a target. */
  targetDist: number | null;
  /** The target already carries this pet's mark. */
  targetMarked: boolean;
  /** Enemies within the pet's AoE reach (for cone / nova). */
  enemiesNearTarget: number;
  basicRange: number;
  basicReadyAt: number;
  /** Too far from the hero: drop the fight and catch up. */
  leash?: number;
}

/** Whether one ability is worth casting right now (ignores its cooldown). */
export function abilityUseful(a: PetAbilityDef, ctx: PetDecisionContext): boolean {
  const inRange = ctx.targetDist !== null && ctx.targetDist <= Math.max(a.range, 0.5);
  switch (a.kind) {
    case 'heal': return ctx.heroHpRatio < 0.7;
    case 'shield': return ctx.heroAttackers > 0 && ctx.heroHpRatio < 0.85;
    case 'taunt': return ctx.heroAttackers > 0;
    case 'buff': return ctx.targetDist !== null && ctx.targetDist <= 8;
    case 'mark': return inRange && !ctx.targetMarked;
    case 'strike':
    case 'bolt': return inRange;
    case 'cone':
    case 'nova': return inRange && ctx.enemiesNearTarget >= 1;
    case 'revive': return false;
  }
  return false;
}

/**
 * Pick what the beast does this tick: a ready, useful ability (in list order,
 * healing first when the hero is low), else a basic attack in range, else close in
 * on the target, else follow the hero.
 */
export function choosePetAction(ctx: PetDecisionContext): PetAction {
  if (ctx.exhausted) return { type: 'rest' };
  const leash = ctx.leash ?? 11;
  if (ctx.peaceful || ctx.heroDist > leash) return { type: 'follow' };

  const ready = ctx.abilities.filter(a => a.kind !== 'revive' && (ctx.readyAt[a.id] ?? 0) <= ctx.now);
  // Sustain first when the hero is in trouble.
  if (ctx.heroHpRatio < 0.5) {
    const sustain = ready.find(a => (a.kind === 'heal' || a.kind === 'shield' || a.kind === 'taunt') && abilityUseful(a, ctx));
    if (sustain) return { type: 'ability', ability: sustain };
  }
  for (const a of ready) {
    if (abilityUseful(a, ctx)) return { type: 'ability', ability: a };
  }
  if (ctx.targetDist === null) return { type: 'follow' };
  if (ctx.targetDist <= ctx.basicRange) {
    return ctx.basicReadyAt <= ctx.now ? { type: 'attack' } : { type: 'rest' };
  }
  return { type: 'approach' };
}

/** Max bond: fire the signature ability once when the hero is low (60 s cooldown). */
export function shouldBondRescue(bond: number, heroHpRatio: number, now: number, lastRescueAt: number): boolean {
  return bond >= PET_MAX_BOND && heroHpRatio > 0 && heroHpRatio < BOND_RESCUE_HP
    && now - lastRescueAt >= BOND_RESCUE_COOLDOWN_MS;
}

// ─── System ────────────────────────────────────────────────────────────────

function petName(petId: string): string {
  const key = petNameKey(petId);
  const s = t(key);
  return s === key ? petId : s;
}

export class PetSystem {
  pets: PetInstance[] = [];
  activePet: string | null = null;
  /** Building level lookup (the tower's 月井 is `pet_house`); set by the session. */
  private buildingLevel: (id: string) => number = () => 0;
  /** Accumulated active time towards the next bond tick. */
  private activeMs = 0;

  /** Pets away on a tower expedition can't be made active; set by the session. */
  private awayCheck: (petId: string) => boolean = () => false;

  setBuildingLevelSource(fn: (id: string) => number): void {
    this.buildingLevel = fn;
  }

  setAwaySource(fn: (petId: string) => boolean): void {
    this.awayCheck = fn;
  }

  /** Away on an expedition (the tower's caravan post). */
  isAway(petId: string): boolean {
    return this.awayCheck(petId);
  }

  getAllPets(): PetDef[] { return PETS; }
  getPetDef(petId: string): PetDef | undefined { return getPetDef(petId); }
  getPetInstance(petId: string): PetInstance | undefined { return this.pets.find(p => p.petId === petId); }
  hasPet(petId: string): boolean { return this.pets.some(p => p.petId === petId); }

  getActivePetInstance(): PetInstance | undefined {
    return this.activePet ? this.getPetInstance(this.activePet) : undefined;
  }

  getActivePetDef(): PetDef | undefined {
    return this.activePet ? getPetDef(this.activePet) : undefined;
  }

  /** Localised name with evolution suffix (·觉醒 / ·至尊). */
  getPetDisplayName(pet: Pick<PetInstance, 'petId' | 'evolved'>): string {
    const name = petName(pet.petId);
    if (pet.evolved >= 2) return t('sys.pet.evoName.2', { name });
    if (pet.evolved >= 1) return t('sys.pet.evoName.1', { name });
    return name;
  }

  /** Bond cap: 3, +1 per 月井 level up to 5. */
  getBondCap(): number {
    return Math.min(PET_MAX_BOND, BASE_BOND_CAP + Math.max(0, this.buildingLevel('pet_house')));
  }

  /** Exp multiplier from the 月井 (10% per level). */
  getExpMultiplier(): number {
    return 1 + Math.max(0, this.buildingLevel('pet_house')) * 0.1;
  }

  getUnlockedAbilities(petId: string): PetAbilityDef[] {
    const def = getPetDef(petId);
    const pet = this.getPetInstance(petId);
    return def && pet ? unlockedAbilities(def, pet.evolved) : [];
  }

  /**
   * Grant a beast. Returns false if unknown or already owned. The first beast
   * becomes active. `silent` skips the log line (cutscenes announce it themselves).
   */
  addPet(petId: string, opts: { silent?: boolean } = {}): boolean {
    if (!getPetDef(petId)) return false;
    if (this.hasPet(petId)) {
      if (!opts.silent) EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('sys.pet.duplicate'), type: 'system' });
      return false;
    }
    this.pets.push({ petId, level: 1, exp: 0, evolved: 0, bond: 0, bondProgress: 0 });
    if (!this.activePet) this.activePet = petId;
    if (!opts.silent) {
      EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('sys.pet.obtained', { name: petName(petId) }), type: 'system' });
    }
    EventBus.emit(GameEvents.PET_OBTAINED, { petId, silent: !!opts.silent });
    EventBus.emit(GameEvents.PET_CHANGED, { petId });
    return true;
  }

  setActivePet(petId: string | null): void {
    if (petId !== null && (!this.hasPet(petId) || this.isAway(petId))) return;
    if (this.activePet === petId) return;
    this.activePet = petId;
    this.activeMs = 0;
    EventBus.emit(GameEvents.PET_CHANGED, { petId });
  }

  /** Add exp to one beast, levelling / evolving as needed. Returns levels gained. */
  addExp(petId: string, amount: number, opts: { silent?: boolean } = {}): number {
    const pet = this.getPetInstance(petId);
    if (!pet || amount <= 0 || pet.level >= PET_MAX_LEVEL) return 0;
    pet.exp += Math.floor(amount);
    let gained = 0;
    while (pet.level < PET_MAX_LEVEL && pet.exp >= petExpToNext(pet.level)) {
      pet.exp -= petExpToNext(pet.level);
      pet.level++;
      gained++;
      if (!opts.silent) {
        EventBus.emit(GameEvents.LOG_MESSAGE, {
          text: t('sys.pet.levelUp', { name: this.getPetDisplayName(pet), level: pet.level }), type: 'system',
        });
      }
      const stage = evolutionForLevel(pet.level);
      if (stage > pet.evolved) {
        const before = this.getPetDisplayName(pet);
        pet.evolved = stage;
        EventBus.emit(GameEvents.LOG_MESSAGE, {
          text: t('sys.pet.evolved', { name: before, evolvedName: this.getPetDisplayName(pet) }), type: 'system',
        });
      }
    }
    if (pet.level >= PET_MAX_LEVEL) pet.exp = 0;
    if (gained > 0) EventBus.emit(GameEvents.PET_CHANGED, { petId });
    return gained;
  }

  /** Add bond progress (capped by the 月井 bond cap). Returns bond levels gained. */
  addBond(petId: string, progress: number): number {
    const pet = this.getPetInstance(petId);
    if (!pet || progress <= 0) return 0;
    const cap = this.getBondCap();
    if (pet.bond >= cap) { pet.bondProgress = 0; return 0; }
    pet.bondProgress += progress;
    let gained = 0;
    while (pet.bond < cap && pet.bondProgress >= BOND_PROGRESS_PER_LEVEL) {
      pet.bondProgress -= BOND_PROGRESS_PER_LEVEL;
      pet.bond++;
      gained++;
      EventBus.emit(GameEvents.LOG_MESSAGE, {
        text: t('sys.pet.bondUp', { name: this.getPetDisplayName(pet), bond: pet.bond }), type: 'system',
      });
    }
    if (pet.bond >= cap) pet.bondProgress = 0;
    pet.bondProgress = Math.floor(pet.bondProgress);
    if (gained > 0) EventBus.emit(GameEvents.PET_CHANGED, { petId });
    return gained;
  }

  /** A monster died: the active beast learns from it, resting beasts too once the 月井 stands. */
  onKill(monsterLevel: number): void {
    const exp = petKillExp(monsterLevel) * this.getExpMultiplier();
    if (this.activePet) {
      this.addExp(this.activePet, exp);
      this.addBond(this.activePet, BOND_PER_KILL);
    }
    const well = this.buildingLevel('pet_house');
    if (well > 0) this.grantRestingExp(Math.floor(petKillExp(monsterLevel) * (0.2 + 0.1 * (well - 1))));
  }

  /** Exp for every owned beast that is not active (the tower's 月井). */
  grantRestingExp(amount: number): void {
    if (amount <= 0) return;
    for (const pet of this.pets) {
      if (pet.petId === this.activePet) continue;
      this.addExp(pet.petId, amount, { silent: true });
    }
  }

  /** Active time (ms) outside safe zones builds bond. */
  tickActive(deltaMs: number): void {
    if (!this.activePet || deltaMs <= 0) return;
    this.activeMs += deltaMs;
    while (this.activeMs >= 60000) {
      this.activeMs -= 60000;
      this.addBond(this.activePet, BOND_PER_ACTIVE_MINUTE);
    }
  }

  canFeed(petId: string): boolean {
    const pet = this.getPetInstance(petId);
    if (!pet) return false;
    return pet.level < PET_MAX_LEVEL || pet.bond < this.getBondCap();
  }

  /** Feed one 灵脉果 (the caller consumes the item). */
  feedPet(petId: string): boolean {
    if (!this.canFeed(petId)) return false;
    const pet = this.getPetInstance(petId)!;
    EventBus.emit(GameEvents.LOG_MESSAGE, { text: t('sys.pet.fed', { name: this.getPetDisplayName(pet) }), type: 'system' });
    this.addExp(petId, FEED_EXP * this.getExpMultiplier());
    this.addBond(petId, BOND_PER_FEED);
    EventBus.emit(GameEvents.PET_CHANGED, { petId });
    return true;
  }

  /** Passive bonuses of the active beast, keyed like EquipStats. */
  getBonuses(): Record<string, number> {
    const pet = this.getActivePetInstance();
    const def = this.getActivePetDef();
    if (!pet || !def) return {};
    return { [def.passive.stat]: petPassiveValue(def, pet) };
  }

  /** Active beast's basic attack damage from the hero's damage. */
  calculatePetDamage(playerDamage: number): number {
    const pet = this.getActivePetInstance();
    return pet ? petAttackDamage(playerDamage, pet) : 0;
  }

  // ─── Save ────────────────────────────────────────────────────────────────

  toSave(): PetSaveData {
    return { owned: this.pets.map(p => ({ ...p })), active: this.activePet };
  }

  /**
   * Restore from a save. Prefers `save.pets`; saves from before the ley-beasts read
   * the old `homestead.pets / activePet` (level/exp kept, evolution re-derived,
   * bond starts at 0; unknown ids dropped).
   */
  loadSave(save: Partial<Pick<SaveData, 'pets' | 'homestead'>> | null | undefined): void {
    const data = migratePetSave(save);
    this.pets = data.owned;
    this.activePet = data.active;
    this.activeMs = 0;
  }

  resetState(): void {
    this.pets = [];
    this.activePet = null;
    this.activeMs = 0;
  }
}

function clampInt(v: unknown, lo: number, hi: number, dflt: number): number {
  const n = typeof v === 'number' && Number.isFinite(v) ? Math.floor(v) : dflt;
  return Math.max(lo, Math.min(hi, n));
}

/** Normalise the pet part of any save (new `pets` field or legacy homestead block). */
export function migratePetSave(save: Partial<Pick<SaveData, 'pets' | 'homestead'>> | null | undefined): PetSaveData {
  const src = save?.pets;
  const legacy = save?.homestead;
  const rawOwned: Partial<PetInstance>[] = src?.owned ?? legacy?.pets ?? [];
  const rawActive = src ? src.active : legacy?.activePet ?? null;
  const owned: PetInstance[] = [];
  for (const p of rawOwned) {
    if (!p || typeof p.petId !== 'string' || !getPetDef(p.petId)) continue;
    if (owned.some(o => o.petId === p.petId)) continue;
    const level = clampInt(p.level, 1, PET_MAX_LEVEL, 1);
    owned.push({
      petId: p.petId,
      level,
      exp: level >= PET_MAX_LEVEL ? 0 : clampInt(p.exp, 0, petExpToNext(level) - 1, 0),
      evolved: Math.max(evolutionForLevel(level), clampInt(p.evolved, 0, 2, 0)),
      bond: clampInt(p.bond, 0, PET_MAX_BOND, 0),
      bondProgress: clampInt(p.bondProgress, 0, BOND_PROGRESS_PER_LEVEL - 1, 0),
    });
  }
  const active = typeof rawActive === 'string' && owned.some(o => o.petId === rawActive) ? rawActive : null;
  return { owned, active };
}

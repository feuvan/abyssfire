import { describe, it, expect, beforeEach, vi } from 'vitest';

// Mock Phaser before importing modules that depend on it (EventBus)
vi.mock('phaser', () => ({
  default: {
    Events: {
      EventEmitter: class MockEventEmitter {
        private handlers = new Map<string, Function[]>();
        on(event: string, fn: Function) {
          if (!this.handlers.has(event)) this.handlers.set(event, []);
          this.handlers.get(event)!.push(fn);
          return this;
        }
        off(event: string, fn: Function) {
          const fns = this.handlers.get(event);
          if (fns) this.handlers.set(event, fns.filter(f => f !== fn));
          return this;
        }
        emit(event: string, ...args: any[]) {
          const fns = this.handlers.get(event);
          if (fns) fns.forEach(fn => fn(...args));
          return true;
        }
      },
    },
  },
}));

import {
  PetSystem, petExpToNext, petKillExp, evolutionForLevel, petPassiveValue, petAttackDamage,
  choosePetAction, abilityUseful, shouldBondRescue, migratePetSave, mergeBonuses, bondMultiplier,
  BOND_PROGRESS_PER_LEVEL, BOND_RESCUE_COOLDOWN_MS, FEED_EXP, PET_DAMAGE_MAX_FRACTION,
  type PetDecisionContext,
} from '../systems/PetSystem';
import {
  PETS, PET_MAX_LEVEL, PET_MAX_BOND, getPetDef, unlockedAbilities, primaryAbility, LEY_FRUIT_ID,
  petNameKey, petDescKey, petOriginKey, petAbilityNameKey, petAbilityDescKey, petRoleKey, petStatKey,
} from '../data/pets';
import { EventBus, GameEvents } from '../utils/EventBus';
import { getItemBase } from '../data/items/bases';
import zhCN from '../i18n/locales/zh-CN';
import en from '../i18n/locales/en';
import type { SaveData } from '../data/types';

const ORIGINAL_IDS = [
  'pet_sprite', 'pet_dragon', 'pet_owl', 'pet_cat', 'pet_phoenix',
  'pet_storm_wolf', 'pet_jade_tortoise', 'pet_void_butterfly',
];

function levelTo(ps: PetSystem, petId: string, level: number): void {
  let guard = 0;
  while (ps.getPetInstance(petId)!.level < level && guard++ < 100) {
    ps.addExp(petId, petExpToNext(ps.getPetInstance(petId)!.level), { silent: true });
  }
}

describe('ley-beast data (src/data/pets.ts)', () => {
  it('keeps the eight original pet ids (save compatibility)', () => {
    expect(PETS.map(p => p.id).sort()).toEqual([...ORIGINAL_IDS].sort());
  });

  it('every beast has a base ability and one unlocked by 觉醒', () => {
    for (const def of PETS) {
      expect(def.abilities.some(a => a.unlock === 0 && a.kind !== 'revive'), def.id).toBe(true);
      expect(def.abilities.some(a => a.unlock === 1), def.id).toBe(true);
      expect(primaryAbility(def)?.unlock).toBe(0);
      for (const a of def.abilities) {
        expect(a.cooldownMs >= 0).toBe(true);
        if (a.kind === 'strike' || a.kind === 'bolt' || a.kind === 'cone') expect(a.damage ?? 0).toBeGreaterThan(0);
      }
    }
  });

  it('ability ids are unique', () => {
    const ids = PETS.flatMap(p => p.abilities.map(a => a.id));
    expect(new Set(ids).size).toBe(ids.length);
  });

  it('design table: roles, passives and signature abilities', () => {
    const expectRole = (id: string, role: string, stat: string, kind: string) => {
      const d = getPetDef(id)!;
      expect(d.role).toBe(role);
      expect(d.passive.stat).toBe(stat);
      expect(primaryAbility(d)!.kind).toBe(kind);
    };
    expectRole('pet_sprite', 'support', 'expBonus', 'heal');
    expectRole('pet_owl', 'scout', 'magicFind', 'mark');
    expectRole('pet_storm_wolf', 'melee', 'attackSpeed', 'strike');
    expectRole('pet_cat', 'assassin', 'critRate', 'strike');
    expectRole('pet_jade_tortoise', 'tank', 'defense', 'taunt');
    expectRole('pet_dragon', 'ranged', 'damagePercent', 'cone');
    expectRole('pet_phoenix', 'support', 'hpRegen', 'heal');
    expectRole('pet_void_butterfly', 'caster', 'manaRegen', 'bolt');
    expect(getPetDef('pet_storm_wolf')!.abilities[0].bleed).toBeGreaterThan(0);
    expect(getPetDef('pet_cat')!.abilities[0].crit).toBe(true);
    expect(getPetDef('pet_phoenix')!.abilities.some(a => a.kind === 'revive')).toBe(true);
    expect(getPetDef('pet_void_butterfly')!.abilities[0].mana).toBeGreaterThan(0);
  });

  it('every name / description / ability / role / stat key exists in zh-CN and en', () => {
    const keys: string[] = [];
    for (const def of PETS) {
      keys.push(petNameKey(def.id), petDescKey(def.id), petOriginKey(def.id), petRoleKey(def.role), petStatKey(def.passive.stat));
      for (const a of def.abilities) keys.push(petAbilityNameKey(a.id), petAbilityDescKey(a.id));
    }
    keys.push('data.item.c_ley_fruit.name', 'ui.pet.title', 'sys.pet.evoName.1', 'sys.pet.evoName.2', 'zone.pet.rareLabel');
    for (const k of keys) {
      expect(zhCN[k], `zh-CN ${k}`).toBeTruthy();
      expect(en[k], `en ${k}`).toBeTruthy();
    }
  });

  it('灵脉果 is a stackable consumable', () => {
    const base = getItemBase(LEY_FRUIT_ID);
    expect(base?.type).toBe('consumable');
    expect(base?.stackable).toBe(true);
  });
});

describe('PetSystem ownership', () => {
  let ps: PetSystem;
  beforeEach(() => { ps = new PetSystem(); });

  it('starts empty', () => {
    expect(ps.pets).toEqual([]);
    expect(ps.activePet).toBeNull();
    expect(ps.getBonuses()).toEqual({});
    expect(ps.calculatePetDamage(100)).toBe(0);
  });

  it('addPet grants a beast, auto-activates the first and emits PET_OBTAINED', () => {
    const got: unknown[] = [];
    const fn = (d: unknown) => got.push(d);
    EventBus.on(GameEvents.PET_OBTAINED, fn);
    expect(ps.addPet('pet_owl')).toBe(true);
    expect(ps.addPet('pet_cat', { silent: true })).toBe(true);
    EventBus.off(GameEvents.PET_OBTAINED, fn);
    expect(ps.activePet).toBe('pet_owl');
    expect(ps.getPetInstance('pet_cat')).toMatchObject({ level: 1, exp: 0, evolved: 0, bond: 0, bondProgress: 0 });
    expect(got).toEqual([{ petId: 'pet_owl', silent: false }, { petId: 'pet_cat', silent: true }]);
  });

  it('rejects duplicates and unknown ids', () => {
    ps.addPet('pet_owl');
    expect(ps.addPet('pet_owl')).toBe(false);
    expect(ps.addPet('pet_unicorn')).toBe(false);
    expect(ps.pets).toHaveLength(1);
  });

  it('setActivePet only accepts owned beasts that are home', () => {
    ps.addPet('pet_owl');
    ps.addPet('pet_cat');
    ps.setActivePet('pet_dragon');
    expect(ps.activePet).toBe('pet_owl');
    ps.setAwaySource(id => id === 'pet_cat');
    ps.setActivePet('pet_cat');
    expect(ps.activePet).toBe('pet_owl');
    ps.setActivePet(null);
    expect(ps.activePet).toBeNull();
  });

  it('display name carries the evolution suffix', () => {
    ps.addPet('pet_storm_wolf');
    const inst = ps.getPetInstance('pet_storm_wolf')!;
    const base = ps.getPetDisplayName(inst);
    inst.evolved = 1;
    expect(ps.getPetDisplayName(inst)).not.toBe(base);
    expect(ps.getPetDisplayName(inst)).toContain(base);
  });
});

describe('PetSystem growth: exp, level, evolution', () => {
  let ps: PetSystem;
  beforeEach(() => { ps = new PetSystem(); ps.addPet('pet_sprite', { silent: true }); });

  it('exp curve and kill exp', () => {
    expect(petExpToNext(1)).toBe(100);
    expect(petExpToNext(10)).toBeGreaterThan(petExpToNext(9));
    expect(petKillExp(20)).toBeGreaterThan(petKillExp(1));
  });

  it('levels up and carries overflow exp', () => {
    ps.addExp('pet_sprite', petExpToNext(1) + 5);
    const inst = ps.getPetInstance('pet_sprite')!;
    expect(inst.level).toBe(2);
    expect(inst.exp).toBe(5);
  });

  it('evolves at 10 (觉醒) and 20 (至尊), unlocking the second ability', () => {
    expect(evolutionForLevel(9)).toBe(0);
    expect(evolutionForLevel(10)).toBe(1);
    expect(evolutionForLevel(20)).toBe(2);
    levelTo(ps, 'pet_sprite', 10);
    expect(ps.getPetInstance('pet_sprite')!.evolved).toBe(1);
    expect(ps.getUnlockedAbilities('pet_sprite').map(a => a.id)).toContain('sprite_ley_ward');
    levelTo(ps, 'pet_sprite', 20);
    const inst = ps.getPetInstance('pet_sprite')!;
    expect(inst.level).toBe(PET_MAX_LEVEL);
    expect(inst.evolved).toBe(2);
    expect(ps.addExp('pet_sprite', 99999)).toBe(0);
    expect(inst.level).toBe(PET_MAX_LEVEL);
  });

  it('base abilities only before evolution', () => {
    expect(ps.getUnlockedAbilities('pet_sprite').map(a => a.id)).toEqual(['sprite_heal_pulse']);
    const phoenix = getPetDef('pet_phoenix')!;
    expect(unlockedAbilities(phoenix, 0).map(a => a.kind)).toEqual(['heal', 'revive']);
  });

  it('a big exp grant can level several times and evolve at once', () => {
    let total = 0;
    for (let l = 1; l < 12; l++) total += petExpToNext(l);
    expect(ps.addExp('pet_sprite', total, { silent: true })).toBe(11);
    expect(ps.getPetInstance('pet_sprite')!.evolved).toBe(1);
  });

  it('kills feed the active beast; resting beasts learn only with the 月井', () => {
    ps.addPet('pet_owl', { silent: true });
    let well = 0;
    ps.setBuildingLevelSource(id => (id === 'pet_house' ? well : 0));
    ps.onKill(10);
    expect(ps.getPetInstance('pet_sprite')!.exp).toBe(petKillExp(10));
    expect(ps.getPetInstance('pet_owl')!.exp).toBe(0);
    well = 2;
    ps.onKill(10);
    expect(ps.getPetInstance('pet_owl')!.exp).toBeGreaterThan(0);
    // 月井 also speeds up the active beast (+10% per level)
    expect(ps.getExpMultiplier()).toBeCloseTo(1.2);
  });

  it('grantRestingExp skips the active beast', () => {
    ps.addPet('pet_owl', { silent: true });
    ps.grantRestingExp(40);
    expect(ps.getPetInstance('pet_sprite')!.exp).toBe(0);
    expect(ps.getPetInstance('pet_owl')!.exp).toBe(40);
  });

  it('feeding gives exp and bond', () => {
    expect(ps.feedPet('pet_sprite')).toBe(true);
    const inst = ps.getPetInstance('pet_sprite')!;
    expect(inst.level > 1 || inst.exp >= FEED_EXP - petExpToNext(1)).toBe(true);
    expect(inst.bondProgress).toBeGreaterThan(0);
    expect(ps.feedPet('pet_owl')).toBe(false);
  });
});

describe('PetSystem bond', () => {
  let ps: PetSystem;
  beforeEach(() => { ps = new PetSystem(); ps.addPet('pet_cat', { silent: true }); });

  it('bond is capped at 3 without the 月井, 5 with it', () => {
    ps.addBond('pet_cat', BOND_PROGRESS_PER_LEVEL * 10);
    expect(ps.getPetInstance('pet_cat')!.bond).toBe(3);
    ps.setBuildingLevelSource(id => (id === 'pet_house' ? 2 : 0));
    expect(ps.getBondCap()).toBe(PET_MAX_BOND);
    ps.addBond('pet_cat', BOND_PROGRESS_PER_LEVEL * 10);
    expect(ps.getPetInstance('pet_cat')!.bond).toBe(PET_MAX_BOND);
    expect(ps.getPetInstance('pet_cat')!.bondProgress).toBe(0);
  });

  it('active time builds bond', () => {
    ps.tickActive(60000 * 50);
    expect(ps.getPetInstance('pet_cat')!.bond).toBe(1);
  });

  it('a fully fed max-level, max-bond beast refuses more fruit', () => {
    ps.setBuildingLevelSource(() => 5);
    levelTo(ps, 'pet_cat', 20);
    ps.addBond('pet_cat', 1000);
    expect(ps.canFeed('pet_cat')).toBe(false);
    expect(ps.feedPet('pet_cat')).toBe(false);
  });

  it('bond multiplier: +10% per bond level', () => {
    expect(bondMultiplier(0)).toBe(1);
    expect(bondMultiplier(5)).toBeCloseTo(1.5);
  });
});

describe('PetSystem passive bonuses and damage', () => {
  it('getBonuses reports only the active beast, scaled by level / evolution / bond', () => {
    const ps = new PetSystem();
    ps.addPet('pet_cat', { silent: true });
    ps.addPet('pet_owl', { silent: true });
    const cat = getPetDef('pet_cat')!;
    expect(ps.getBonuses()).toEqual({ critRate: cat.passive.base });
    const inst = ps.getPetInstance('pet_cat')!;
    const lv1 = petPassiveValue(cat, inst);
    inst.level = 10; inst.evolved = 1;
    const evo = petPassiveValue(cat, inst);
    expect(evo).toBeGreaterThan(lv1 * 1.5);
    inst.bond = 5;
    expect(petPassiveValue(cat, inst)).toBeCloseTo(Math.round(evo * 1.5 * 10) / 10, 1);
    ps.setActivePet('pet_owl');
    expect(Object.keys(ps.getBonuses())).toEqual(['magicFind']);
    ps.setActivePet(null);
    expect(ps.getBonuses()).toEqual({});
  });

  it('pet attack: 5% of hero damage at Lv.1, capped at 15% (× evolution)', () => {
    expect(petAttackDamage(100, { level: 1, evolved: 0 })).toBe(5);
    expect(petAttackDamage(100, { level: 10, evolved: 0 })).toBe(10);
    expect(petAttackDamage(100, { level: 20, evolved: 0 })).toBe(100 * PET_DAMAGE_MAX_FRACTION);
    expect(petAttackDamage(100, { level: 20, evolved: 2 })).toBe(30);
    expect(petAttackDamage(0, { level: 5, evolved: 0 })).toBe(0);
    expect(petAttackDamage(3, { level: 1, evolved: 0 })).toBe(0);
    expect(petAttackDamage(20, { level: 1, evolved: 0 })).toBe(1);
  });

  it('mergeBonuses adds overlapping stats', () => {
    expect(mergeBonuses({ a: 1, b: 2 }, { b: 3, c: 4 })).toEqual({ a: 1, b: 5, c: 4 });
  });
});

describe('PetSystem save + migration', () => {
  it('round-trips through toSave / loadSave', () => {
    const ps = new PetSystem();
    ps.addPet('pet_dragon', { silent: true });
    ps.addPet('pet_owl', { silent: true });
    levelTo(ps, 'pet_dragon', 12);
    ps.addBond('pet_dragon', 150);
    ps.setActivePet('pet_owl');
    const saved = JSON.parse(JSON.stringify({ pets: ps.toSave() }));
    const ps2 = new PetSystem();
    ps2.loadSave(saved);
    expect(ps2.pets).toEqual(ps.pets);
    expect(ps2.activePet).toBe('pet_owl');
  });

  it('migrates the old homestead pet list (pre ley-beast saves)', () => {
    const old = {
      homestead: {
        buildings: { pet_house: 2 },
        pets: [
          { petId: 'pet_sprite', level: 5, exp: 30 },
          { petId: 'pet_dragon', level: 12, exp: 50, evolved: 0 },
          { petId: 'wolf', level: 3, exp: 15 },
          { petId: 'pet_sprite', level: 9, exp: 1 },
        ],
        activePet: 'pet_dragon',
      },
    } as Partial<SaveData>;
    const ps = new PetSystem();
    ps.loadSave(old);
    expect(ps.pets.map(p => p.petId)).toEqual(['pet_sprite', 'pet_dragon']);
    expect(ps.getPetInstance('pet_sprite')).toEqual({ petId: 'pet_sprite', level: 5, exp: 30, evolved: 0, bond: 0, bondProgress: 0 });
    // evolution re-derived from the level
    expect(ps.getPetInstance('pet_dragon')!.evolved).toBe(1);
    expect(ps.activePet).toBe('pet_dragon');
  });

  it('drops an active pet that is not owned, and clamps junk values', () => {
    const data = migratePetSave({
      homestead: { buildings: {}, pets: [{ petId: 'pet_cat', level: 99, exp: -5 }], activePet: 'pet_owl' },
    });
    expect(data.active).toBeNull();
    expect(data.owned[0]).toMatchObject({ level: PET_MAX_LEVEL, exp: 0, evolved: 2 });
  });

  it('prefers the new `pets` field over the legacy block', () => {
    const data = migratePetSave({
      homestead: { buildings: {}, pets: [{ petId: 'pet_cat', level: 3, exp: 0 }], activePet: 'pet_cat' },
      pets: { owned: [{ petId: 'pet_owl', level: 4, exp: 10, evolved: 0, bond: 2, bondProgress: 40 }], active: 'pet_owl' },
    });
    expect(data.owned.map(p => p.petId)).toEqual(['pet_owl']);
    expect(data.owned[0].bond).toBe(2);
    expect(data.active).toBe('pet_owl');
  });

  it('handles missing data', () => {
    expect(migratePetSave(undefined)).toEqual({ owned: [], active: null });
    expect(migratePetSave({ homestead: { buildings: {} } })).toEqual({ owned: [], active: null });
  });
});

describe('pet combat decisions (choosePetAction)', () => {
  const sprite = getPetDef('pet_sprite')!;
  const owl = getPetDef('pet_owl')!;
  const tortoise = getPetDef('pet_jade_tortoise')!;
  const dragon = getPetDef('pet_dragon')!;

  const ctx = (over: Partial<PetDecisionContext> = {}): PetDecisionContext => ({
    now: 10000,
    abilities: unlockedAbilities(owl, 0),
    readyAt: {},
    exhausted: false,
    peaceful: false,
    heroDist: 2,
    heroHpRatio: 1,
    heroAttackers: 0,
    targetDist: 3,
    targetMarked: false,
    enemiesNearTarget: 1,
    basicRange: 5,
    basicReadyAt: 0,
    ...over,
  });

  it('rests while exhausted and follows in safe zones / when leashed', () => {
    expect(choosePetAction(ctx({ exhausted: true })).type).toBe('rest');
    expect(choosePetAction(ctx({ peaceful: true })).type).toBe('follow');
    expect(choosePetAction(ctx({ heroDist: 20 })).type).toBe('follow');
    expect(choosePetAction(ctx({ targetDist: null })).type).toBe('follow');
  });

  it('uses a ready ability when useful, then falls back to the basic attack', () => {
    const a = choosePetAction(ctx());
    expect(a.type).toBe('ability');
    if (a.type === 'ability') expect(a.ability.id).toBe('owl_moon_mark');
    // already marked → basic attack
    expect(choosePetAction(ctx({ targetMarked: true })).type).toBe('attack');
    // on cooldown → basic attack
    expect(choosePetAction(ctx({ readyAt: { owl_moon_mark: 20000 } })).type).toBe('attack');
    // basic on cooldown → wait
    expect(choosePetAction(ctx({ targetMarked: true, basicReadyAt: 20000 })).type).toBe('rest');
  });

  it('closes in when the target is out of reach', () => {
    expect(choosePetAction(ctx({ targetDist: 12, targetMarked: true })).type).toBe('approach');
    // mark range is 8: a target at 7 is marked from range
    const a = choosePetAction(ctx({ targetDist: 7 }));
    expect(a.type).toBe('ability');
  });

  it('heals only when the hero needs it', () => {
    const base = ctx({ abilities: unlockedAbilities(sprite, 1), basicRange: 4.5 });
    expect(choosePetAction(base).type).toBe('attack');
    const low = choosePetAction({ ...base, heroHpRatio: 0.4 });
    expect(low.type === 'ability' && low.ability.kind).toBe('heal');
    // shield when surrounded
    const mob = choosePetAction({ ...base, heroHpRatio: 0.8, heroAttackers: 2 });
    expect(mob.type === 'ability' && mob.ability.kind).toBe('shield');
    // heal on cooldown while low → shield takes over
    const cd = choosePetAction({ ...base, heroHpRatio: 0.4, heroAttackers: 1, readyAt: { sprite_heal_pulse: 99999 } });
    expect(cd.type === 'ability' && cd.ability.kind).toBe('shield');
  });

  it('the tortoise taunts only when something is hitting the hero', () => {
    const base = ctx({ abilities: unlockedAbilities(tortoise, 0), basicRange: 1.4, targetDist: 1 });
    expect(choosePetAction(base).type).toBe('attack');
    const a = choosePetAction({ ...base, heroAttackers: 1 });
    expect(a.type === 'ability' && a.ability.kind).toBe('taunt');
  });

  it('breath needs the target in range', () => {
    const [breath] = unlockedAbilities(dragon, 0);
    expect(abilityUseful(breath, ctx({ targetDist: 3 }))).toBe(true);
    expect(abilityUseful(breath, ctx({ targetDist: 6 }))).toBe(false);
    expect(abilityUseful(breath, ctx({ targetDist: 3, enemiesNearTarget: 0 }))).toBe(false);
  });

  it('never picks the passive revive', () => {
    const phoenix = getPetDef('pet_phoenix')!;
    const a = choosePetAction(ctx({ abilities: unlockedAbilities(phoenix, 0), heroHpRatio: 0.1, readyAt: { phoenix_ember_mend: 99999 } }));
    expect(a.type === 'ability' ? a.ability.kind : a.type).not.toBe('revive');
  });

  it('max bond rescue: below 30% HP, once per minute', () => {
    expect(shouldBondRescue(5, 0.2, 100000, -Infinity)).toBe(true);
    expect(shouldBondRescue(4, 0.2, 100000, -Infinity)).toBe(false);
    expect(shouldBondRescue(5, 0.5, 100000, -Infinity)).toBe(false);
    expect(shouldBondRescue(5, 0, 100000, -Infinity)).toBe(false);
    expect(shouldBondRescue(5, 0.2, 100000, 100000 - BOND_RESCUE_COOLDOWN_MS + 1)).toBe(false);
    expect(shouldBondRescue(5, 0.2, 100000, 100000 - BOND_RESCUE_COOLDOWN_MS)).toBe(true);
  });
});

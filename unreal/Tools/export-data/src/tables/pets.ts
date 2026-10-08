/**
 * pets.json — ley-beast definitions (src/data/pets.ts) verbatim plus the PetSystem / PetCompanion
 * constants. Formula coefficients are checked against the real helpers.
 */
import { PETS, PET_MAX_LEVEL, PET_EVOLUTION_LEVELS, PET_EVOLUTION_MULT, PET_MAX_BOND, LEY_FRUIT_ID, primaryAbility } from '../../../../../src/data/pets';
import * as PetSystemMod from '../../../../../src/systems/PetSystem';
import * as PetCompanionMod from '../../../../../src/systems/PetCompanion';
import { assert, plain, type TableResult } from '../util';

const exp = <T>(mod: unknown, name: string): T => {
  const v = (mod as Record<string, unknown>)[`__expose_${name}`];
  assert(v !== undefined, `exposed binding ${name} missing`);
  return v as T;
};

export function exportPets(): TableResult[] {
  const P = PetSystemMod;
  for (let L = 1; L <= 25; L++) {
    assert(P.petExpToNext(L) === 60 + 40 * L, 'petExpToNext');
    assert(P.petKillExp(L) === 10 + L, 'petKillExp');
  }
  for (let b = -1; b <= 7; b++) assert(P.bondMultiplier(b) === 1 + 0.1 * Math.max(0, Math.min(PET_MAX_BOND, b)), 'bondMultiplier');
  assert(P.evolutionForLevel(9) === 0 && P.evolutionForLevel(10) === 1 && P.evolutionForLevel(20) === 2, 'evolutionForLevel');
  const ids = new Set<string>();
  for (const p of PETS) { assert(!ids.has(p.id), `duplicate pet ${p.id}`); ids.add(p.id); }
  return [{
    file: 'pets.json',
    source: ['src/data/pets.ts', 'src/systems/PetSystem.ts', 'src/systems/PetCompanion.ts'],
    data: {
      pets: PETS.map(p => ({ ...plain(p), derived: { primaryAbilityId: primaryAbility(p)?.id ?? null,
        i18n: { name: `data.pet.${p.id}.name`, desc: `data.pet.${p.id}.desc`, origin: `data.pet.${p.id}.origin` } } })),
      maxLevel: PET_MAX_LEVEL,
      evolutionLevels: [...PET_EVOLUTION_LEVELS],
      evolutionMult: [...PET_EVOLUTION_MULT],
      maxBond: PET_MAX_BOND,
      leyFruitId: LEY_FRUIT_ID,
      system: {
        damageBaseFraction: P.PET_DAMAGE_BASE_FRACTION,
        damagePerLevelFraction: P.PET_DAMAGE_PER_LEVEL_FRACTION,
        damageMaxFraction: P.PET_DAMAGE_MAX_FRACTION,
        bondProgressPerLevel: P.BOND_PROGRESS_PER_LEVEL,
        bondPerKill: P.BOND_PER_KILL,
        bondPerActiveMinute: P.BOND_PER_ACTIVE_MINUTE,
        bondPerFeed: P.BOND_PER_FEED,
        feedExp: P.FEED_EXP,
        bondRescueHp: P.BOND_RESCUE_HP,
        bondRescueCooldownMs: P.BOND_RESCUE_COOLDOWN_MS,
        baseBondCap: P.BASE_BOND_CAP,
        expToNext: { base: 60, perLevel: 40 },
        killExp: { base: 10, perMonsterLevel: 1 },
        bondMultiplierPerBond: 0.1,
        passiveRounding: 'round(x*10)/10',
        leyFruitDropChance: { elite: P.leyFruitDropChance(true), other: P.leyFruitDropChance(false) },
      },
      companion: {
        exhaustMs: PetCompanionMod.PET_EXHAUST_MS,
        straySwingChance: exp<number>(PetCompanionMod, 'STRAY_SWING_CHANCE'),
        followSpeedTilesPerSec: exp<number>(PetCompanionMod, 'FOLLOW_SPEED'),
        dashSpeedTilesPerSec: exp<number>(PetCompanionMod, 'DASH_SPEED'),
        teleportDistanceTiles: exp<number>(PetCompanionMod, 'TELEPORT_DIST'),
      },
      port: { chapter1Slice: ['pet_sprite'], decision: 'Q3' },
    },
    counts: { pets: PETS.length, abilities: PETS.reduce((n, p) => n + p.abilities.length, 0) },
  }];
}

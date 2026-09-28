/**
 * 灵兽 (ley-beasts) — see docs/homestead-pets.md §三.
 *
 * Eight beasts keep the original pet ids for save compatibility. Every
 * player-facing string is an i18n key (src/i18n/locales/pets.ts):
 *   data.pet.<id>.name / .desc / .origin
 *   data.pet.ability.<abilityId>.name / .desc
 *   data.pet.role.<role>
 */
import type { MonsterAnimCategory } from './types';

export type PetRole = 'support' | 'scout' | 'melee' | 'assassin' | 'tank' | 'ranged' | 'caster';

/**
 * What an ability does. The AI picks abilities by kind (PetSystem.choosePetAction)
 * and PetCompanion plays them.
 *   heal   – restore `value` × hero max HP
 *   shield – hero damage reduction `value` for `durationMs`
 *   mark   – target takes +`value` damage for `durationMs` (damageAmplify debuff)
 *   strike – melee hit for `damage` × pet attack (optional leap / bleed / crit / hits)
 *   bolt   – projectile for `damage` × pet attack (optional `mana` restore to the hero)
 *   cone   – breath in front of the pet, `radius` tiles long, `arc` radians wide
 *   nova   – burst of `radius` tiles around the target (or the pet with `self`)
 *   taunt  – nearby monsters attack the pet for `durationMs`; shields the hero (`value`)
 *   buff   – hero outgoing damage +`value` for `durationMs`
 *   revive – passive: once per zone, the hero rises at `value` × max HP instead of dying
 */
export type PetAbilityKind = 'heal' | 'shield' | 'mark' | 'strike' | 'bolt' | 'cone' | 'nova' | 'taunt' | 'buff' | 'revive';

export type PetElement = 'physical' | 'fire' | 'ice' | 'lightning' | 'poison' | 'arcane';

export interface PetAbilityDef {
  id: string;
  kind: PetAbilityKind;
  /** Evolution stage that unlocks it: 0 = from the start, 1 = 觉醒 (Lv.10). */
  unlock: 0 | 1;
  cooldownMs: number;
  /** Tiles from the pet to the target (0 = no target needed). */
  range: number;
  /** Multiplier on the pet's attack damage. */
  damage?: number;
  /** Effect magnitude (heal fraction, damage reduction, amplify, mana fraction, revive HP). */
  value?: number;
  durationMs?: number;
  radius?: number;
  /** Cone width in radians. */
  arc?: number;
  element?: PetElement;
  /** Always crit (背刺). */
  crit?: boolean;
  /** Leap onto the target before striking (扑击). */
  leap?: boolean;
  /** Bleed / burn / slow / stun applied on hit: fraction of the hit per tick, or slow %. */
  bleed?: number;
  burn?: number;
  slow?: number;
  stunMs?: number;
  /** Number of hits (strike). */
  hits?: number;
  /** Fraction of the hero's max mana restored (bolt). */
  mana?: number;
  /** Nova centred on the pet instead of its target. */
  self?: boolean;
}

export interface PetCombatStyle {
  style: 'melee' | 'ranged';
  /** Basic attack reach in tiles. */
  range: number;
  /** Basic attack interval. */
  attackMs: number;
  /** Projectile / impact colour. */
  color: number;
  element: PetElement;
}

export interface PetDef {
  id: string;
  /** Chapter the beast belongs to (story beat that grants it). */
  chapter: 1 | 2 | 3 | 4 | 5;
  role: PetRole;
  rarity: 'common' | 'rare' | 'epic';
  /** Flies over walls / water when following. */
  flying: boolean;
  animCategory: MonsterAnimCategory;
  combat: PetCombatStyle;
  /** Passive bonus: `base + perLevel × (level − 1)`, × evolution × bond multipliers. */
  passive: { stat: string; base: number; perLevel: number };
  /** Max HP as a fraction of the hero's max HP. */
  hpFraction: number;
  abilities: PetAbilityDef[];
}

export const PET_MAX_LEVEL = 20;
/** Evolution thresholds: 觉醒 at 10, 至尊 at 20. */
export const PET_EVOLUTION_LEVELS: readonly number[] = [10, 20];
/** Passive multiplier per evolution stage (0, 1, 2). */
export const PET_EVOLUTION_MULT: readonly number[] = [1, 1.5, 2];
export const PET_MAX_BOND = 5;
/** Consumable fed to beasts. */
export const LEY_FRUIT_ID = 'c_ley_fruit';

export const PETS: PetDef[] = [
  {
    id: 'pet_sprite', chapter: 1, role: 'support', rarity: 'common', flying: true, animCategory: 'flying',
    combat: { style: 'ranged', range: 4.5, attackMs: 1800, color: 0x8ff0c0, element: 'arcane' },
    passive: { stat: 'expBonus', base: 3, perLevel: 0.4 },
    hpFraction: 0.45,
    abilities: [
      { id: 'sprite_heal_pulse', kind: 'heal', unlock: 0, cooldownMs: 12000, range: 0, value: 0.08 },
      { id: 'sprite_ley_ward', kind: 'shield', unlock: 1, cooldownMs: 20000, range: 0, value: 0.2, durationMs: 5000 },
    ],
  },
  {
    id: 'pet_owl', chapter: 2, role: 'scout', rarity: 'common', flying: true, animCategory: 'flying',
    combat: { style: 'ranged', range: 5, attackMs: 1700, color: 0xbfd8ff, element: 'physical' },
    passive: { stat: 'magicFind', base: 5, perLevel: 0.8 },
    hpFraction: 0.45,
    abilities: [
      { id: 'owl_moon_mark', kind: 'mark', unlock: 0, cooldownMs: 10000, range: 8, value: 0.15, durationMs: 6000 },
      { id: 'owl_talon_dive', kind: 'strike', unlock: 1, cooldownMs: 9000, range: 7, damage: 2.2, leap: true },
    ],
  },
  {
    id: 'pet_storm_wolf', chapter: 2, role: 'melee', rarity: 'epic', flying: false, animCategory: 'beast',
    combat: { style: 'melee', range: 1.3, attackMs: 1200, color: 0xdfe8ff, element: 'physical' },
    passive: { stat: 'attackSpeed', base: 3, perLevel: 0.35 },
    hpFraction: 0.7,
    abilities: [
      { id: 'wolf_pounce', kind: 'strike', unlock: 0, cooldownMs: 8000, range: 5, damage: 1.5, leap: true, bleed: 0.25, durationMs: 4000 },
      { id: 'wolf_moon_howl', kind: 'buff', unlock: 1, cooldownMs: 22000, range: 0, value: 0.15, durationMs: 6000 },
    ],
  },
  {
    id: 'pet_cat', chapter: 2, role: 'assassin', rarity: 'rare', flying: false, animCategory: 'beast',
    combat: { style: 'melee', range: 1.2, attackMs: 1100, color: 0xb070ff, element: 'physical' },
    passive: { stat: 'critRate', base: 2, perLevel: 0.25 },
    hpFraction: 0.55,
    abilities: [
      { id: 'cat_backstab', kind: 'strike', unlock: 0, cooldownMs: 7000, range: 1.6, damage: 2.2, crit: true },
      { id: 'cat_shadow_flurry', kind: 'strike', unlock: 1, cooldownMs: 12000, range: 1.6, damage: 0.9, hits: 3 },
    ],
  },
  {
    id: 'pet_jade_tortoise', chapter: 3, role: 'tank', rarity: 'epic', flying: false, animCategory: 'large',
    combat: { style: 'melee', range: 1.4, attackMs: 1700, color: 0x6fe0b0, element: 'physical' },
    passive: { stat: 'defense', base: 4, perLevel: 1.2 },
    hpFraction: 1.2,
    abilities: [
      { id: 'tortoise_taunt', kind: 'taunt', unlock: 0, cooldownMs: 14000, range: 0, radius: 4, value: 0.25, durationMs: 5000 },
      { id: 'tortoise_quake', kind: 'nova', unlock: 1, cooldownMs: 12000, range: 1.8, damage: 1.0, radius: 2.5, stunMs: 1200, self: true },
    ],
  },
  {
    id: 'pet_dragon', chapter: 3, role: 'ranged', rarity: 'rare', flying: true, animCategory: 'flying',
    combat: { style: 'ranged', range: 4.5, attackMs: 1600, color: 0xff7a2a, element: 'fire' },
    passive: { stat: 'damagePercent', base: 3, perLevel: 0.45 },
    hpFraction: 0.6,
    abilities: [
      { id: 'dragon_fire_breath', kind: 'cone', unlock: 0, cooldownMs: 9000, range: 3.5, damage: 1.3, radius: 4, arc: Math.PI / 3, element: 'fire', burn: 0.15, durationMs: 3000 },
      { id: 'dragon_magma_orb', kind: 'bolt', unlock: 1, cooldownMs: 8000, range: 6, damage: 2.0, element: 'fire', radius: 1.5 },
    ],
  },
  {
    id: 'pet_phoenix', chapter: 4, role: 'support', rarity: 'epic', flying: true, animCategory: 'flying',
    combat: { style: 'ranged', range: 4.5, attackMs: 1800, color: 0xffb040, element: 'fire' },
    passive: { stat: 'hpRegen', base: 2, perLevel: 0.5 },
    hpFraction: 0.55,
    abilities: [
      { id: 'phoenix_ember_mend', kind: 'heal', unlock: 0, cooldownMs: 14000, range: 0, value: 0.12 },
      { id: 'phoenix_rekindle', kind: 'revive', unlock: 0, cooldownMs: 0, range: 0, value: 0.4 },
      { id: 'phoenix_flame_ring', kind: 'nova', unlock: 1, cooldownMs: 13000, range: 2.5, damage: 1.2, radius: 2.5, element: 'fire', burn: 0.12, durationMs: 3000 },
    ],
  },
  {
    id: 'pet_void_butterfly', chapter: 5, role: 'caster', rarity: 'epic', flying: true, animCategory: 'flying',
    combat: { style: 'ranged', range: 5, attackMs: 1700, color: 0xcc44cc, element: 'arcane' },
    passive: { stat: 'manaRegen', base: 1.5, perLevel: 0.35 },
    hpFraction: 0.45,
    abilities: [
      { id: 'butterfly_void_bolt', kind: 'bolt', unlock: 0, cooldownMs: 6000, range: 6, damage: 1.5, element: 'arcane', mana: 0.05 },
      { id: 'butterfly_void_rift', kind: 'nova', unlock: 1, cooldownMs: 12000, range: 6, damage: 1.1, radius: 2.2, element: 'arcane', slow: 35, durationMs: 3000 },
    ],
  },
];

const PET_BY_ID = new Map(PETS.map(p => [p.id, p]));

export function getPetDef(id: string): PetDef | undefined {
  return PET_BY_ID.get(id);
}

/** Abilities unlocked at an evolution stage (passives like `revive` included). */
export function unlockedAbilities(def: PetDef, evolved: number): PetAbilityDef[] {
  return def.abilities.filter(a => a.unlock <= evolved);
}

/** The beast's signature (first) active ability — what max bond auto-triggers. */
export function primaryAbility(def: PetDef): PetAbilityDef | undefined {
  return def.abilities.find(a => a.kind !== 'revive');
}

export function petNameKey(id: string): string { return `data.pet.${id}.name`; }
export function petDescKey(id: string): string { return `data.pet.${id}.desc`; }
export function petOriginKey(id: string): string { return `data.pet.${id}.origin`; }
export function petAbilityNameKey(abilityId: string): string { return `data.pet.ability.${abilityId}.name`; }
export function petAbilityDescKey(abilityId: string): string { return `data.pet.ability.${abilityId}.desc`; }
export function petRoleKey(role: PetRole): string { return `data.pet.role.${role}`; }
export function petStatKey(stat: string): string { return `data.pet.stat.${stat}`; }

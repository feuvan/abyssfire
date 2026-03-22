import { EventBus, GameEvents } from '../utils/EventBus';
import { clamp, chance } from '../utils/MathUtils';
import type { Stats, SkillDefinition } from '../data/types';

/** Equipment-derived stats injected into combat calculations. */
export interface EquipStats {
  damage: number;
  damagePercent: number;
  defense: number;
  defensePercent: number;
  maxHp: number;
  maxHpPercent: number;
  maxMana: number;
  maxManaPercent: number;
  critRate: number;
  critDamage: number;
  attackSpeed: number;
  lifeSteal: number;
  manaSteal: number;
  hpRegen: number;
  manaRegen: number;
  fireDamage: number;
  iceDamage: number;
  lightningDamage: number;
  poisonDamage: number;
  fireResist: number;
  iceResist: number;
  lightningResist: number;
  poisonResist: number;
  allResist: number;
  moveSpeed: number;
  magicFind: number;
  expBonus: number;
  cooldownReduction: number;
  knockback: number;
  // Special effects (from set bonuses / legendaries)
  killHealPercent: number;
  deathSave: number;
  critDoubleStrike: number;
  doubleShot: number;
  freeCast: number;
  elementalDamagePercent: number;
  ignoreDefense: number;
  damageReduction: number;
  thornsHeal: number;
  dodgeCounter: number;
}

export function emptyEquipStats(): EquipStats {
  return {
    damage: 0, damagePercent: 0,
    defense: 0, defensePercent: 0,
    maxHp: 0, maxHpPercent: 0,
    maxMana: 0, maxManaPercent: 0,
    critRate: 0, critDamage: 0, attackSpeed: 0,
    lifeSteal: 0, manaSteal: 0,
    hpRegen: 0, manaRegen: 0,
    fireDamage: 0, iceDamage: 0, lightningDamage: 0, poisonDamage: 0,
    fireResist: 0, iceResist: 0, lightningResist: 0, poisonResist: 0, allResist: 0,
    moveSpeed: 0, magicFind: 0, expBonus: 0, cooldownReduction: 0, knockback: 0,
    killHealPercent: 0, deathSave: 0, critDoubleStrike: 0, doubleShot: 0,
    freeCast: 0, elementalDamagePercent: 0, ignoreDefense: 0, damageReduction: 0,
    thornsHeal: 0, dodgeCounter: 0,
  };
}

export interface CombatEntity {
  id: string;
  name: string;
  hp: number;
  maxHp: number;
  mana: number;
  maxMana: number;
  stats: Stats;
  level: number;
  baseDamage: number;
  defense: number;
  attackSpeed: number;
  attackRange: number;
  buffs: ActiveBuff[];
  equipStats?: EquipStats;
}

export interface ActiveBuff {
  stat: string;
  value: number;
  duration: number;
  startTime: number;
}

export interface DamageResult {
  damage: number;
  isCrit: boolean;
  isDodged: boolean;
  damageType: 'physical' | 'fire' | 'ice' | 'lightning' | 'poison' | 'arcane';
  lifeStolen: number;
  manaStolen: number;
}

/**
 * D2-style tiered scaling: early levels give more value per point.
 * Bracket multipliers: levels 1-8 = 100%, 9-16 = 75%, 17-20 = 50%.
 */
function tieredScale(perLevel: number, level: number): number {
  if (level <= 1) return 0;
  let total = 0;
  for (let i = 2; i <= level; i++) {
    if (i <= 8) total += perLevel;
    else if (i <= 16) total += perLevel * 0.75;
    else total += perLevel * 0.5;
  }
  return total;
}

/** Resolve the effective damage multiplier for a skill at a given level. */
export function getSkillDamageMultiplier(skill: SkillDefinition, level: number): number {
  const base = skill.damageMultiplier;
  const perLevel = skill.scaling?.damagePerLevel ?? 0.05;
  return base + tieredScale(perLevel, level);
}

/** Resolve the effective mana cost for a skill at a given level. */
export function getSkillManaCost(skill: SkillDefinition, level: number): number {
  const base = skill.manaCost;
  const perLevel = skill.scaling?.manaCostPerLevel ?? 0.5;
  return Math.floor(base + tieredScale(perLevel, level));
}

/** Resolve the effective cooldown (ms) for a skill at a given level. */
export function getSkillCooldown(skill: SkillDefinition, level: number, cdr = 0): number {
  const base = skill.cooldown;
  const perLevel = skill.scaling?.cooldownReductionPerLevel ?? 0;
  const raw = Math.max(500, Math.floor(base - tieredScale(perLevel, level)));
  return Math.floor(raw * (1 - clamp(cdr, 0, 50) / 100));
}

/** Resolve the effective AoE radius for a skill at a given level. */
export function getSkillAoeRadius(skill: SkillDefinition, level: number): number {
  const base = skill.aoeRadius ?? 0;
  const perLevel = skill.scaling?.aoeRadiusPerLevel ?? 0;
  return base + tieredScale(perLevel, level);
}

/** Resolve the effective buff value for a skill at a given level. */
export function getSkillBuffValue(skill: SkillDefinition, level: number): number {
  const base = skill.buff?.value ?? 0;
  const perLevel = skill.scaling?.buffValuePerLevel ?? 0.02;
  return base + tieredScale(perLevel, level);
}

/** Resolve the effective buff duration (ms) for a skill at a given level. */
export function getSkillBuffDuration(skill: SkillDefinition, level: number): number {
  const base = skill.buff?.duration ?? 0;
  const perLevel = skill.scaling?.buffDurationPerLevel ?? 0;
  return Math.floor(base + tieredScale(perLevel, level));
}

/** Calculate synergy bonus multiplier (1.0 = no bonus). */
export function getSynergyBonus(
  skill: SkillDefinition,
  skillLevels: Map<string, number> | Record<string, number>,
): number {
  if (!skill.synergies || skill.synergies.length === 0) return 1;
  let bonus = 0;
  for (const syn of skill.synergies) {
    const synLevel = skillLevels instanceof Map
      ? (skillLevels.get(syn.skillId) ?? 0)
      : (skillLevels[syn.skillId] ?? 0);
    bonus += syn.damagePerLevel * synLevel;
  }
  return 1 + bonus;
}

/** Get effective resistance for a damage type (sum of specific + allResist, capped at 75). */
function getResistance(eq: EquipStats | undefined, dmgType: string): number {
  if (!eq) return 0;
  let res = eq.allResist;
  if (dmgType === 'fire') res += eq.fireResist;
  else if (dmgType === 'ice') res += eq.iceResist;
  else if (dmgType === 'lightning') res += eq.lightningResist;
  else if (dmgType === 'poison') res += eq.poisonResist;
  return clamp(res, 0, 75);
}

export class CombatSystem {
  calculateDamage(
    attacker: CombatEntity,
    defender: CombatEntity,
    skill?: SkillDefinition,
    skillLevel = 1,
    skillLevels?: Map<string, number>,
  ): DamageResult {
    const atkEq = attacker.equipStats;
    const defEq = defender.equipStats;

    // Dodge check
    const dodgeRate = clamp(defender.stats.dex * 0.3, 0, 30);
    if (chance(dodgeRate)) {
      return { damage: 0, isCrit: false, isDodged: true, damageType: 'physical', lifeStolen: 0, manaStolen: 0 };
    }

    // Crit check
    const skillCritBonus = skill?.critBonus ?? 0;
    const eqCritRate = atkEq?.critRate ?? 0;
    const critRate = clamp(
      attacker.stats.dex * 0.2 + attacker.stats.lck * 0.5 + skillCritBonus + eqCritRate,
      0,
      75,
    );
    const isCrit = chance(critRate);
    const eqCritDmg = atkEq?.critDamage ?? 0;
    const critMultiplier = isCrit ? 1.5 + attacker.stats.lck * 0.01 + eqCritDmg / 100 : 1;

    const damageType = skill?.damageType ?? 'physical';
    let baseDmg: number;
    let multiplier = 1;

    if (skill) {
      const isPhysical = damageType === 'physical';
      const statBonus = isPhysical ? attacker.stats.str : attacker.stats.int;
      baseDmg = attacker.baseDamage + statBonus * 0.5;
      multiplier = getSkillDamageMultiplier(skill, skillLevel);
      if (skillLevels) {
        multiplier *= getSynergyBonus(skill, skillLevels);
      }
    } else {
      baseDmg = attacker.baseDamage + attacker.stats.str * 0.5;
    }

    // Flat equipment damage bonus
    if (atkEq) {
      baseDmg += atkEq.damage;
    }

    // Percent equipment damage bonus
    if (atkEq && atkEq.damagePercent > 0) {
      baseDmg *= 1 + atkEq.damagePercent / 100;
    }

    // Elemental flat damage from equipment
    let elementalFlat = 0;
    if (atkEq) {
      elementalFlat += atkEq.fireDamage + atkEq.iceDamage + atkEq.lightningDamage + atkEq.poisonDamage;
      if (atkEq.elementalDamagePercent > 0) {
        elementalFlat *= 1 + atkEq.elementalDamagePercent / 100;
      }
    }

    // Damage reduction from buffs
    let damageReduction = 0;
    for (const buff of defender.buffs) {
      if (buff.stat === 'damageReduction') {
        damageReduction += buff.value;
      }
    }
    // Permanent damage reduction from gear
    if (defEq && defEq.damageReduction > 0) {
      damageReduction += defEq.damageReduction / 100;
    }

    // Defense with percent bonus
    let effectiveDefense = defender.defense;
    if (defEq && defEq.defense > 0) effectiveDefense += defEq.defense;
    if (defEq && defEq.defensePercent > 0) effectiveDefense *= 1 + defEq.defensePercent / 100;

    // Ignore defense from attacker gear
    const ignoreDefPct = atkEq?.ignoreDefense ?? 0;
    if (ignoreDefPct > 0) {
      effectiveDefense *= 1 - ignoreDefPct / 100;
    }

    const rawDamage = baseDmg * multiplier * critMultiplier + elementalFlat;
    const afterDef = Math.max(1, rawDamage - effectiveDefense * 0.5);
    let finalDamage = afterDef * (1 - damageReduction);

    // Elemental resistance reduces non-physical damage
    if (damageType !== 'physical') {
      const resist = getResistance(defEq, damageType);
      finalDamage *= 1 - resist / 100;
    }

    finalDamage = Math.max(1, Math.floor(finalDamage));

    // Life steal / mana steal
    const lifeStealPct = atkEq?.lifeSteal ?? 0;
    const manaStealPct = atkEq?.manaSteal ?? 0;
    const lifeStolen = lifeStealPct > 0 ? Math.floor(finalDamage * lifeStealPct / 100) : 0;
    const manaStolen = manaStealPct > 0 ? Math.floor(finalDamage * manaStealPct / 100) : 0;

    return { damage: finalDamage, isCrit, isDodged: false, damageType, lifeStolen, manaStolen };
  }

  applyDamage(target: CombatEntity, result: DamageResult): void {
    if (result.isDodged) {
      EventBus.emit(GameEvents.COMBAT_DAMAGE, {
        targetId: target.id,
        damage: 0,
        isDodged: true,
        isCrit: false,
      });
      return;
    }

    target.hp = Math.max(0, target.hp - result.damage);
    EventBus.emit(GameEvents.COMBAT_DAMAGE, {
      targetId: target.id,
      damage: result.damage,
      isDodged: false,
      isCrit: result.isCrit,
    });

    if (target.hp <= 0) {
      EventBus.emit(GameEvents.MONSTER_DIED, { id: target.id, name: target.name });
    }
  }

  canUseSkill(entity: CombatEntity, skill: SkillDefinition, skillLevel: number = 1): boolean {
    return entity.mana >= getSkillManaCost(skill, skillLevel);
  }

  useSkillMana(entity: CombatEntity, skill: SkillDefinition, skillLevel: number = 1): void {
    entity.mana = Math.max(0, entity.mana - getSkillManaCost(skill, skillLevel));
  }

  updateBuffs(entity: CombatEntity, now: number): void {
    entity.buffs = entity.buffs.filter(b => now - b.startTime < b.duration);
  }

  addBuff(entity: CombatEntity, buff: ActiveBuff): void {
    entity.buffs.push(buff);
  }
}

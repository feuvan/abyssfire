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
  /** Damage redirected to mana by Mana Shield (caller should deduct from entity mana). */
  manaDamage?: number;
}

// ---------------------------------------------------------------------------
// Stun tracking for diminishing returns
// ---------------------------------------------------------------------------

export interface StunRecord {
  /** Number of stuns applied within the immunity-reset window. */
  stunCount: number;
  /** Timestamp of last stun application. */
  lastStunTime: number;
  /** If > 0, entity is immune to stun until this timestamp. */
  immuneUntil: number;
}

/** Stun diminishing-returns config (exported for tests). */
export const STUN_DIMINISH_FACTOR = 0.5;  // 50% duration on 2nd stun
export const STUN_IMMUNITY_DURATION = 3000;  // 3s immunity after 2nd stun
export const STUN_WINDOW = 6000;  // reset stun count after 6s of no stuns

/** Buff stacking caps. Values are the max total for additive stacking. */
export const BUFF_CAPS: Record<string, number> = {
  damageReduction: 0.9,   // 90% max damage reduction
  defenseBonus: 2.0,       // 200% max defense bonus
  damageBonus: 3.0,        // 300% max damage bonus
  attackSpeed: 1.0,        // 100% max attack speed buff
  poisonDamage: 5.0,       // 500% max poison bonus
  stealthDamage: 5.0,      // 500% max stealth bonus
  manaShield: 0.9,         // 90% max mana shield conversion
};

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

/**
 * Get the aggregated value of a specific buff stat from an entity's active buffs,
 * capped by the stacking limit defined in BUFF_CAPS.
 */
export function getBuffValue(entity: CombatEntity, stat: string): number {
  let total = 0;
  for (const buff of entity.buffs) {
    if (buff.stat === stat) {
      total += buff.value;
    }
  }
  const cap = BUFF_CAPS[stat];
  if (cap !== undefined) {
    total = Math.min(total, cap);
  }
  return total;
}

export class CombatSystem {
  /** Per-entity stun tracking for diminishing returns. Keyed by entity id. */
  private stunRecords = new Map<string, StunRecord>();

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

    // --- Buff: poisonDamage adds bonus poison flat damage ---
    const poisonBuff = getBuffValue(attacker, 'poisonDamage');
    if (poisonBuff > 0) {
      elementalFlat += attacker.baseDamage * poisonBuff;
    }

    // --- Buff: stealthDamage multiplies total damage (consumed on use) ---
    const stealthBuff = getBuffValue(attacker, 'stealthDamage');

    // --- Buff: damageBonus multiplies total damage ---
    const damageBonusBuff = getBuffValue(attacker, 'damageBonus');

    // Damage reduction from buffs (damageReduction stat)
    let damageReduction = getBuffValue(defender, 'damageReduction');
    // Permanent damage reduction from gear
    if (defEq && defEq.damageReduction > 0) {
      damageReduction += defEq.damageReduction / 100;
    }
    // Cap total damage reduction
    damageReduction = Math.min(damageReduction, BUFF_CAPS.damageReduction);

    // --- Buff: defenseBonus increases effective defense ---
    const defenseBonusBuff = getBuffValue(defender, 'defenseBonus');

    // Defense with percent bonus
    let effectiveDefense = defender.defense;
    if (defEq && defEq.defense > 0) effectiveDefense += defEq.defense;
    if (defEq && defEq.defensePercent > 0) effectiveDefense *= 1 + defEq.defensePercent / 100;
    // Apply defenseBonus buff (multiplicative on effective defense)
    if (defenseBonusBuff > 0) {
      effectiveDefense *= 1 + defenseBonusBuff;
    }

    // Ignore defense from attacker gear
    const ignoreDefPct = atkEq?.ignoreDefense ?? 0;
    if (ignoreDefPct > 0) {
      effectiveDefense *= 1 - ignoreDefPct / 100;
    }

    const rawDamage = baseDmg * multiplier * critMultiplier + elementalFlat;
    const afterDef = Math.max(1, rawDamage - effectiveDefense * 0.5);
    let finalDamage = afterDef * (1 - damageReduction);

    // Apply damageBonus buff
    if (damageBonusBuff > 0) {
      finalDamage *= 1 + damageBonusBuff;
    }

    // Apply stealthDamage buff (consumed on use — caller should remove the buff after attack)
    if (stealthBuff > 0) {
      finalDamage *= 1 + stealthBuff;
    }

    // Elemental resistance reduces non-physical damage
    if (damageType !== 'physical') {
      const resist = getResistance(defEq, damageType);
      finalDamage *= 1 - resist / 100;
    }

    finalDamage = Math.max(1, Math.floor(finalDamage));

    // --- Mana Shield: redirect portion of damage to mana ---
    let manaDamage: number | undefined;
    const manaShieldValue = getBuffValue(defender, 'manaShield');
    if (manaShieldValue > 0 && defender.mana > 0) {
      const redirected = Math.floor(finalDamage * manaShieldValue);
      const actualManaAbsorb = Math.min(redirected, defender.mana);
      finalDamage = Math.max(1, finalDamage - actualManaAbsorb);
      manaDamage = actualManaAbsorb;
    }

    // Life steal / mana steal
    const lifeStealPct = atkEq?.lifeSteal ?? 0;
    const manaStealPct = atkEq?.manaSteal ?? 0;
    const lifeStolen = lifeStealPct > 0 ? Math.floor(finalDamage * lifeStealPct / 100) : 0;
    const manaStolen = manaStealPct > 0 ? Math.floor(finalDamage * manaStealPct / 100) : 0;

    return { damage: finalDamage, isCrit, isDodged: false, damageType, lifeStolen, manaStolen, manaDamage };
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

    // Deduct mana if Mana Shield redirected damage
    if (result.manaDamage && result.manaDamage > 0) {
      target.mana = Math.max(0, target.mana - result.manaDamage);
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

  // ---------------------------------------------------------------------------
  // Stun mechanics with diminishing returns
  // ---------------------------------------------------------------------------

  /**
   * Apply stun to a target entity.
   * Returns the effective stun duration (0 if immune).
   * The caller is responsible for setting the monster's state to 'stunned'
   * and preventing movement/attack for the returned duration.
   */
  applyStun(targetId: string, baseDuration: number, now: number): number {
    if (baseDuration <= 0) return 0;

    let record = this.stunRecords.get(targetId);
    if (!record) {
      record = { stunCount: 0, lastStunTime: 0, immuneUntil: 0 };
      this.stunRecords.set(targetId, record);
    }

    // Check immunity
    if (now < record.immuneUntil) {
      return 0;
    }

    // Reset stun count if outside the stun window
    if (now - record.lastStunTime > STUN_WINDOW) {
      record.stunCount = 0;
    }

    record.stunCount++;
    record.lastStunTime = now;

    let effectiveDuration = baseDuration;

    if (record.stunCount === 2) {
      // Second stun: 50% duration
      effectiveDuration = Math.floor(baseDuration * STUN_DIMINISH_FACTOR);
      // After 2nd stun, grant immunity
      record.immuneUntil = now + effectiveDuration + STUN_IMMUNITY_DURATION;
    } else if (record.stunCount > 2) {
      // Should not happen if immunity is working, but safety: immune
      record.immuneUntil = now + STUN_IMMUNITY_DURATION;
      return 0;
    }

    return effectiveDuration;
  }

  /**
   * Check whether an entity is currently stunned based on its active buffs.
   * Stun is tracked as a buff with stat='stunned'.
   */
  isStunned(entity: CombatEntity, now: number): boolean {
    for (const buff of entity.buffs) {
      if (buff.stat === 'stunned' && now - buff.startTime < buff.duration) {
        return true;
      }
    }
    return false;
  }

  /** Get the stun record for an entity (useful for testing). */
  getStunRecord(entityId: string): StunRecord | undefined {
    return this.stunRecords.get(entityId);
  }

  /** Clear stun records (e.g. on entity death or zone change). */
  clearStunRecord(entityId: string): void {
    this.stunRecords.delete(entityId);
  }

  // ---------------------------------------------------------------------------
  // Taunt: force monster aggro
  // ---------------------------------------------------------------------------

  /**
   * Apply taunt to monsters in range.
   * Returns the list of monster IDs that were taunted.
   * The caller is responsible for:
   *   1. Setting each monster's target to the player
   *   2. Setting each monster's state to 'chase' or 'attack'
   *
   * @param monsterIds - IDs of monsters within AoE range
   * @param playerId - ID of the player applying taunt
   * @param duration - Duration of taunt in ms
   * @param now - Current timestamp
   */
  applyTaunt(
    monsters: CombatEntity[],
    playerId: string,
    duration: number,
    now: number,
  ): string[] {
    const tauntedIds: string[] = [];
    for (const monster of monsters) {
      // Add a 'taunted' buff to the monster
      this.addBuff(monster, {
        stat: 'taunted',
        value: 1,
        duration,
        startTime: now,
      });
      tauntedIds.push(monster.id);
    }
    EventBus.emit(GameEvents.LOG_MESSAGE, {
      message: `嘲讽怒吼影响了${tauntedIds.length}个敌人`,
    });
    return tauntedIds;
  }

  /**
   * Check if a monster is taunted.
   */
  isTaunted(entity: CombatEntity, now: number): boolean {
    for (const buff of entity.buffs) {
      if (buff.stat === 'taunted' && now - buff.startTime < buff.duration) {
        return true;
      }
    }
    return false;
  }

  /**
   * Consume stealth damage buff after an attack.
   * Removes all stealthDamage buffs from the entity.
   */
  consumeStealthBuff(entity: CombatEntity): void {
    entity.buffs = entity.buffs.filter(b => b.stat !== 'stealthDamage');
  }
}

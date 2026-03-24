import { randomInt } from '../utils/MathUtils';

// ---------------------------------------------------------------------------
// Elite Monster Affix System
// ---------------------------------------------------------------------------

/**
 * Affix type identifiers. Each corresponds to a distinct mechanic and visual.
 */
export type EliteAffixType =
  | 'fire_enhanced'   // Fire aura + extra fire damage
  | 'swift'           // Increased movement speed
  | 'teleporting'     // Periodic blink near player
  | 'extra_strong'    // +30%+ damage
  | 'curse_aura'      // Debuff aura reducing player stats in proximity
  | 'vampiric'        // Lifesteal on hit
  | 'frozen';         // Chance to apply slow/freeze on hit

/**
 * Full definition of an elite affix, including name, stat modifiers,
 * and behavioral parameters.
 */
export interface EliteAffixDefinition {
  type: EliteAffixType;
  /** Chinese display name for the affix */
  name: string;
  /** English name (for data/debug) */
  nameEn: string;
  /** Flat multiplier applied to monster damage (1.0 = no change) */
  damageMult: number;
  /** Flat multiplier applied to monster speed (1.0 = no change) */
  speedMult: number;
  /** Flat multiplier applied to monster HP (1.0 = no change) */
  hpMult: number;
  /** Flat multiplier applied to monster defense (1.0 = no change) */
  defenseMult: number;
  /** Extra fire damage as fraction of base damage (fire_enhanced only) */
  extraFireDamage: number;
  /** Teleport cooldown in ms (teleporting only) */
  teleportCooldownMs: number;
  /** Curse aura radius in tiles (curse_aura only) */
  curseAuraRadius: number;
  /** Curse aura stat reduction fraction (e.g. 0.15 = 15% reduction) */
  curseAuraReduction: number;
  /** Lifesteal fraction (vampiric only, e.g. 0.2 = 20%) */
  lifestealFraction: number;
  /** Chance to freeze on hit (frozen only, 0-1) */
  freezeChance: number;
  /** VFX tint color (used for aura/glow) */
  vfxColor: number;
  /** Loot quality bonus (additive to quality roll, e.g. +10 = +10% to better quality) */
  lootQualityBonus: number;
}

// ---------------------------------------------------------------------------
// Affix Definitions (const data)
// ---------------------------------------------------------------------------

export const ELITE_AFFIX_DEFINITIONS: Record<EliteAffixType, EliteAffixDefinition> = {
  fire_enhanced: {
    type: 'fire_enhanced',
    name: '炎魔',
    nameEn: 'Fire Enhanced',
    damageMult: 1.0,
    speedMult: 1.0,
    hpMult: 1.2,
    defenseMult: 1.0,
    extraFireDamage: 0.3,
    teleportCooldownMs: 0,
    curseAuraRadius: 0,
    curseAuraReduction: 0,
    lifestealFraction: 0,
    freezeChance: 0,
    vfxColor: 0xff4400,
    lootQualityBonus: 5,
  },
  swift: {
    type: 'swift',
    name: '迅捷',
    nameEn: 'Swift',
    damageMult: 1.0,
    speedMult: 1.6,
    hpMult: 1.0,
    defenseMult: 1.0,
    extraFireDamage: 0,
    teleportCooldownMs: 0,
    curseAuraRadius: 0,
    curseAuraReduction: 0,
    lifestealFraction: 0,
    freezeChance: 0,
    vfxColor: 0x44ff88,
    lootQualityBonus: 3,
  },
  teleporting: {
    type: 'teleporting',
    name: '瞬移',
    nameEn: 'Teleporting',
    damageMult: 1.1,
    speedMult: 1.0,
    hpMult: 1.15,
    defenseMult: 1.0,
    extraFireDamage: 0,
    teleportCooldownMs: 5000,
    curseAuraRadius: 0,
    curseAuraReduction: 0,
    lifestealFraction: 0,
    freezeChance: 0,
    vfxColor: 0xaa44ff,
    lootQualityBonus: 5,
  },
  extra_strong: {
    type: 'extra_strong',
    name: '狂暴',
    nameEn: 'Extra Strong',
    damageMult: 1.35,
    speedMult: 1.0,
    hpMult: 1.3,
    defenseMult: 1.15,
    extraFireDamage: 0,
    teleportCooldownMs: 0,
    curseAuraRadius: 0,
    curseAuraReduction: 0,
    lifestealFraction: 0,
    freezeChance: 0,
    vfxColor: 0xff2222,
    lootQualityBonus: 8,
  },
  curse_aura: {
    type: 'curse_aura',
    name: '诅咒',
    nameEn: 'Curse Aura',
    damageMult: 1.1,
    speedMult: 1.0,
    hpMult: 1.2,
    defenseMult: 1.0,
    extraFireDamage: 0,
    teleportCooldownMs: 0,
    curseAuraRadius: 4,
    curseAuraReduction: 0.15,
    lifestealFraction: 0,
    freezeChance: 0,
    vfxColor: 0x8844aa,
    lootQualityBonus: 6,
  },
  vampiric: {
    type: 'vampiric',
    name: '吸血',
    nameEn: 'Vampiric',
    damageMult: 1.1,
    speedMult: 1.0,
    hpMult: 1.25,
    defenseMult: 1.0,
    extraFireDamage: 0,
    teleportCooldownMs: 0,
    curseAuraRadius: 0,
    curseAuraReduction: 0,
    lifestealFraction: 0.2,
    freezeChance: 0,
    vfxColor: 0xcc0000,
    lootQualityBonus: 5,
  },
  frozen: {
    type: 'frozen',
    name: '冰封',
    nameEn: 'Frozen',
    damageMult: 1.05,
    speedMult: 0.9,
    hpMult: 1.2,
    defenseMult: 1.1,
    extraFireDamage: 0,
    teleportCooldownMs: 0,
    curseAuraRadius: 0,
    curseAuraReduction: 0,
    lifestealFraction: 0,
    freezeChance: 0.25,
    vfxColor: 0x4488ff,
    lootQualityBonus: 5,
  },
};

/** All affix types for random selection. */
const ALL_AFFIX_TYPES: EliteAffixType[] = Object.keys(ELITE_AFFIX_DEFINITIONS) as EliteAffixType[];

// ---------------------------------------------------------------------------
// Runtime state for an elite monster's active affixes
// ---------------------------------------------------------------------------

export interface EliteAffixInstance {
  definition: EliteAffixDefinition;
  /** Last teleport timestamp (for teleporting affix) */
  lastTeleportTime: number;
  /** Last curse aura tick timestamp */
  lastCurseTickTime: number;
}

// ---------------------------------------------------------------------------
// Zone → affix count mapping
// ---------------------------------------------------------------------------

/**
 * Maps zone IDs to [minAffixes, maxAffixes] for elite monsters.
 * Zones 1-3: 1 affix, Zone 4: 1-2, Zone 5: 2-3.
 */
const ZONE_AFFIX_COUNTS: Record<string, [number, number]> = {
  emerald_plains: [1, 1],
  twilight_forest: [1, 1],
  anvil_mountains: [1, 1],
  scorching_desert: [1, 2],
  abyss_rift: [2, 3],
};

// ---------------------------------------------------------------------------
// EliteAffixSystem — Pure logic, no Phaser dependency
// ---------------------------------------------------------------------------

export class EliteAffixSystem {
  /**
   * Roll random affixes for an elite monster based on zone.
   * Returns empty array for non-elite monsters or unknown zones.
   */
  rollAffixes(zoneId: string, isElite: boolean): EliteAffixInstance[] {
    if (!isElite) return [];
    const range = ZONE_AFFIX_COUNTS[zoneId] ?? [1, 1];
    const count = randomInt(range[0], range[1]);
    return this.selectAffixes(count);
  }

  /**
   * Select `count` unique random affixes and create instances.
   */
  selectAffixes(count: number): EliteAffixInstance[] {
    const available = [...ALL_AFFIX_TYPES];
    const selected: EliteAffixInstance[] = [];
    const actualCount = Math.min(count, available.length);

    for (let i = 0; i < actualCount; i++) {
      const idx = randomInt(0, available.length - 1);
      const type = available[idx];
      available.splice(idx, 1);
      selected.push({
        definition: ELITE_AFFIX_DEFINITIONS[type],
        lastTeleportTime: 0,
        lastCurseTickTime: 0,
      });
    }

    return selected;
  }

  /**
   * Build the display name for an affix elite monster.
   * Format: "[affix1·affix2] 原名" — all in Chinese.
   */
  buildAffixName(baseName: string, affixes: EliteAffixInstance[]): string {
    if (affixes.length === 0) return baseName;
    const affixNames = affixes.map(a => a.definition.name).join('·');
    return `[${affixNames}] ${baseName}`;
  }

  /**
   * Compute combined stat multipliers from stacked affixes.
   */
  getCombinedStats(affixes: EliteAffixInstance[]): {
    damageMult: number;
    speedMult: number;
    hpMult: number;
    defenseMult: number;
    extraFireDamage: number;
    lootQualityBonus: number;
    lifestealFraction: number;
    freezeChance: number;
  } {
    let damageMult = 1;
    let speedMult = 1;
    let hpMult = 1;
    let defenseMult = 1;
    let extraFireDamage = 0;
    let lootQualityBonus = 0;
    let lifestealFraction = 0;
    let freezeChance = 0;

    for (const affix of affixes) {
      const def = affix.definition;
      // Multiplicative stacking for multipliers
      damageMult *= def.damageMult;
      speedMult *= def.speedMult;
      hpMult *= def.hpMult;
      defenseMult *= def.defenseMult;
      // Additive stacking for extras
      extraFireDamage += def.extraFireDamage;
      lootQualityBonus += def.lootQualityBonus;
      lifestealFraction += def.lifestealFraction;
      // Freeze chance stacks additively (capped at 0.5)
      freezeChance = Math.min(0.5, freezeChance + def.freezeChance);
    }

    return { damageMult, speedMult, hpMult, defenseMult, extraFireDamage, lootQualityBonus, lifestealFraction, freezeChance };
  }

  /**
   * Check if this set of affixes includes a specific type.
   */
  hasAffix(affixes: EliteAffixInstance[], type: EliteAffixType): boolean {
    return affixes.some(a => a.definition.type === type);
  }

  /**
   * Get the teleporting affix instance if present.
   */
  getTeleportingAffix(affixes: EliteAffixInstance[]): EliteAffixInstance | undefined {
    return affixes.find(a => a.definition.type === 'teleporting');
  }

  /**
   * Get the curse aura affix instance if present.
   */
  getCurseAuraAffix(affixes: EliteAffixInstance[]): EliteAffixInstance | undefined {
    return affixes.find(a => a.definition.type === 'curse_aura');
  }

  /**
   * Check if teleport should trigger (cooldown check).
   */
  shouldTeleport(affix: EliteAffixInstance, now: number): boolean {
    return now - affix.lastTeleportTime >= affix.definition.teleportCooldownMs;
  }

  /**
   * Compute extra fire damage for a hit (fire_enhanced affix).
   */
  getExtraFireDamage(affixes: EliteAffixInstance[], baseDamage: number): number {
    const stats = this.getCombinedStats(affixes);
    return Math.floor(baseDamage * stats.extraFireDamage);
  }

  /**
   * Get the primary VFX color for a set of affixes (uses the first affix's color).
   */
  getPrimaryVfxColor(affixes: EliteAffixInstance[]): number {
    if (affixes.length === 0) return 0xffffff;
    return affixes[0].definition.vfxColor;
  }

  /**
   * Get all VFX colors for multi-affix display.
   */
  getAllVfxColors(affixes: EliteAffixInstance[]): number[] {
    return affixes.map(a => a.definition.vfxColor);
  }
}

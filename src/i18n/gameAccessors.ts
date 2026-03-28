/**
 * Locale-aware accessor functions for game data (items, affixes, stats).
 * These resolve the correct translated name based on the active locale.
 */
import { t, getLocale } from './index';
import type { ItemInstance } from '../data/types';
import { getItemBase } from '../data/items/bases';
import { STAT_DISPLAY, AllAffixes } from '../data/items/affixes';

/** Get translated item base name. Falls back to base.name for zh-CN. */
export function getItemBaseName(baseId: string): string {
  const locale = getLocale();
  const base = getItemBase(baseId);
  if (!base) return baseId;
  if (locale === 'en') {
    const enName = t(`data.item.${baseId}.name`);
    // If the key returns itself, fallback to nameEn or name
    if (enName !== `data.item.${baseId}.name`) return enName;
    return (base as any).nameEn ?? base.name;
  }
  // zh-CN or zh-TW: use locale key (zh-TW auto-converts from zh-CN)
  const locName = t(`data.item.${baseId}.name`);
  if (locName !== `data.item.${baseId}.name`) return locName;
  return base.name;
}

/** Get translated item base description. */
export function getItemBaseDesc(baseId: string): string {
  const base = getItemBase(baseId);
  if (!base || !base.description) return '';
  const locDesc = t(`data.item.${baseId}.desc`);
  if (locDesc !== `data.item.${baseId}.desc`) return locDesc;
  return base.description;
}

/** Look up AffixDefinition by affix ID. */
function findAffixDef(affixId: string) {
  return AllAffixes.find(a => a.id === affixId);
}

/** Get translated item instance display name (with affixes if applicable). */
export function getItemDisplayName(item: ItemInstance): string {
  const locale = getLocale();
  const base = getItemBase(item.baseId);
  if (!base) return item.name;

  // Base name
  let baseName: string;
  if (locale === 'en') {
    const enName = t(`data.item.${item.baseId}.name`);
    baseName = (enName !== `data.item.${item.baseId}.name`) ? enName : ((base as any).nameEn ?? base.name);
  } else {
    const locName = t(`data.item.${item.baseId}.name`);
    baseName = (locName !== `data.item.${item.baseId}.name`) ? locName : base.name;
  }

  // For normal/consumable/gem items, just return the base name
  if (item.quality === 'normal' || !item.affixes || item.affixes.length === 0) {
    return baseName;
  }

  // For magic/rare, assemble prefix + base + suffix
  // Look up affix definitions to determine prefix/suffix
  const prefixes: { affixId: string; name: string }[] = [];
  const suffixes: { affixId: string; name: string }[] = [];
  for (const affix of item.affixes) {
    const def = findAffixDef(affix.affixId);
    if (def && def.type === 'suffix') {
      suffixes.push(affix);
    } else {
      prefixes.push(affix);
    }
  }

  let name = baseName;
  if (locale === 'en') {
    // English: "Sharp Short Sword of Life"
    const prefixNames = prefixes.map(a => {
      const def = findAffixDef(a.affixId);
      if (def) return def.nameEn;
      return a.name;
    });
    const suffixNames = suffixes.map(a => {
      const def = findAffixDef(a.affixId);
      if (def) return def.nameEn;
      return a.name;
    });
    if (prefixNames.length > 0) name = prefixNames.join(' ') + ' ' + name;
    if (suffixNames.length > 0) name = name + ' ' + suffixNames.join(' ');
  } else {
    // Chinese: use locale-resolved affix name
    const prefixNames = prefixes.map(a => {
      const def = findAffixDef(a.affixId);
      return def ? getAffixName(def) : a.name;
    });
    const suffixNames = suffixes.map(a => {
      const def = findAffixDef(a.affixId);
      return def ? getAffixName(def) : a.name;
    });
    if (prefixNames.length > 0) name = prefixNames.join('') + name;
    if (suffixNames.length > 0) name = name + '·' + suffixNames.join('·');
  }

  return name;
}

/** Get the translated affix name. */
export function getAffixName(affix: { id?: string; name: string; nameEn?: string }): string {
  const locale = getLocale();
  if (locale === 'en') {
    return affix.nameEn ?? affix.name;
  }
  // zh-CN/zh-TW — for zh-TW, the name from locale data will be auto-converted
  if (affix.id) {
    const locName = t(`data.affix.${affix.id}.name`);
    if (locName !== `data.affix.${affix.id}.name`) return locName;
  }
  return affix.name;
}

/** Get the translated stat display label (replaces STAT_DISPLAY[key].label). */
export function getStatLabel(statKey: string): string {
  const localeLabel = t(`ui.stat.${statKey}`);
  if (localeLabel !== `ui.stat.${statKey}`) return localeLabel;
  // Fallback to STAT_DISPLAY
  const disp = STAT_DISPLAY[statKey];
  return disp ? disp.label : statKey;
}

/** Check if a stat is a percent stat. */
export function isStatPercent(statKey: string): boolean {
  const disp = STAT_DISPLAY[statKey];
  return disp?.isPercent ?? false;
}

/** Get the translated quality label for an item quality. */
export function getQualityLabel(quality: string): string {
  return t(`ui.tooltip.quality.${quality}`);
}

/** Get the translated set name. */
export function getSetName(setId: string, fallback: string): string {
  const locName = t(`data.set.${setId}.name`);
  return locName !== `data.set.${setId}.name` ? locName : fallback;
}

/** Get translated set bonus description. */
export function getSetBonusDesc(setId: string, bonusIndex: number, fallback: string): string {
  const locDesc = t(`data.set.${setId}.bonus.${bonusIndex}`);
  return locDesc !== `data.set.${setId}.bonus.${bonusIndex}` ? locDesc : fallback;
}

/** Get translated class name for use in UI. */
export function getClassName(classId: string): string {
  const key = `data.class.${classId}.name`;
  const locName = t(key);
  return locName !== key ? locName : classId;
}

/** Get the translated compass direction. */
export function getDirection(dc: number, dr: number): string {
  const angle = Math.atan2(dr, dc) * 180 / Math.PI;
  if (angle >= -22.5 && angle < 22.5) return t('ui.compass.east');
  if (angle >= 22.5 && angle < 67.5) return t('ui.compass.southeast');
  if (angle >= 67.5 && angle < 112.5) return t('ui.compass.south');
  if (angle >= 112.5 && angle < 157.5) return t('ui.compass.southwest');
  if (angle >= 157.5 || angle < -157.5) return t('ui.compass.west');
  if (angle >= -157.5 && angle < -112.5) return t('ui.compass.northwest');
  if (angle >= -112.5 && angle < -67.5) return t('ui.compass.north');
  return t('ui.compass.northeast');
}

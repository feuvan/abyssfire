/**
 * Overrides for tables that live inside web system files (implementation-plan.md §2.4). Those files are code, so they are
 * not redirected; instead each exporter table applies `override ?? web` per entry and records what it changed in a
 * `remake` block of the written file (`{"remake": {"source": [...], "replaced": ["achievements.ach_kill_slime"], ...}}`).
 *
 * Every table is optional. Per table:
 *   replace  web entries by key (the key must exist in the web table; for id-keyed lists the value may carry a new id,
 *            which renames the entry in place and keeps its slot)
 *   add      new entries, appended in this order (the key must not exist in the web table)
 *   remove   web entries to drop (the keys must exist)
 * An entry is replaced whole: to change one field, spread the web entry (import it from `@web/...`; system modules are
 * not redirected, so they may be imported here). Unknown table names, unknown keys and a table the exporter did not
 * apply fail the export (unreal/Tools/export-data/src/remake.ts).
 */
import type { AchievementDefinition, CampTheme } from './types';
import type { ZoneEventData } from '@web/systems/RandomEventSystem';
import type { ZoneMood, ZonePaletteEntry } from '@web/graphics/ZonePalette';
import type { EliteAffixDefinition } from '@web/systems/EliteAffixSystem';
import type { TerrainTheme } from '@web/graphics/terrain/TerrainStyles';

export interface EntryOverride<T> {
  readonly replace?: Readonly<Record<string, T>>;
  readonly add?: Readonly<Record<string, T>>;
  readonly remove?: readonly string[];
}

/** WeatherSystem's (module-private) WeatherConfig, widened for remake ambiences. */
export interface RemakeWeather {
  type: string;
  ambience?: string;
  tint?: number;
}

/** zone_moods.json `port.fog` row (DECISIONS P14). */
export interface RemakeFog {
  color: number;
  maxOpacity: number;
  density: number;
}

export interface RemakeOverrides {
  /** achievements.json `achievements` (AchievementSystem ACHIEVEMENTS), by id. */
  readonly achievements?: EntryOverride<AchievementDefinition>;
  /** random_events.json `zones` and shops.json `wanderingMerchant` (RandomEventSystem ZONE_EVENT_DATA), by zone id. */
  readonly randomEvents?: EntryOverride<ZoneEventData>;
  /** zone_moods.json. */
  readonly zoneMoods?: {
    /** `themeByZone` (LightingSystem ZONE_THEME_BY_ID), zone id -> mood theme. */
    readonly themeByZone?: EntryOverride<string>;
    /** `moods` (ZonePalette ZONE_MOODS), by theme. */
    readonly moods?: EntryOverride<ZoneMood>;
    /** `palettes` (ZonePalette ZONE_PALETTES), by theme. */
    readonly palettes?: EntryOverride<ZonePaletteEntry>;
    /** `weather` (WeatherSystem ZONE_WEATHER), by zone id. */
    readonly weather?: EntryOverride<RemakeWeather>;
    /** `weatherByTheme` (WeatherSystem THEME_WEATHER), theme -> zone id whose weather it borrows. */
    readonly weatherByTheme?: EntryOverride<string>;
    /** `campThemes` (camp-themes CAMP_THEMES), by theme. */
    readonly campThemes?: EntryOverride<CampTheme>;
    /** `port.fog` (exporter table, DECISIONS P14), by theme. */
    readonly fog?: EntryOverride<RemakeFog>;
  };
  /** elite_affixes.json. */
  readonly eliteAffixes?: {
    /** `definitions` (EliteAffixSystem ELITE_AFFIX_DEFINITIONS; `order` follows), by affix type. */
    readonly definitions?: EntryOverride<EliteAffixDefinition>;
    /** `zoneAffixCounts` (EliteAffixSystem ZONE_AFFIX_COUNTS), zone id -> [min, max]. */
    readonly zoneAffixCounts?: EntryOverride<[number, number]>;
  };
  /** terrain_styles.json `themes` (TerrainStyles TERRAIN_THEMES), by theme. */
  readonly terrainStyles?: EntryOverride<TerrainTheme>;
}

/** Phase 0: no overrides (the export equals the web's). The content switch (C1) fills in Chapter 1's. */
export const REMAKE_OVERRIDES: RemakeOverrides = {};

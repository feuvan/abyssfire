import { EmeraldPlainsMap } from './emerald_plains';
import { TwilightForestMap } from './twilight_forest';
import { AnvilMountainsMap } from './anvil_mountains';
import { ScorchingDesertMap } from './scorching_desert';
import { AbyssRiftMap } from './abyss_rift';
import { EmberTowerMap } from './ember_tower';
import { MapGenerator } from '../../systems/MapGenerator';
import type { MapData } from '../types';
import { LoreByZone } from '../loreCollectibles';
import { MiniBossSpawns } from '../miniBosses';
import { AllQuests } from '../quests/all_quests';

/** Positions defined outside the map files (quests, lore, mini-bosses) that must stay walkable. */
function externalLandmarks(mapId: string): { col: number; row: number; margin?: number }[] {
  const pts: { col: number; row: number; margin?: number }[] = [];
  for (const lore of LoreByZone[mapId] ?? []) pts.push({ col: lore.col, row: lore.row, margin: 2 });
  const boss = MiniBossSpawns[mapId];
  if (boss) pts.push({ ...boss, margin: 5 });
  for (const q of AllQuests) {
    if (q.zone !== mapId) continue;
    for (const o of q.objectives) {
      if (o.location) pts.push({ col: o.location.col, row: o.location.row, margin: 3 });
      if (o.source?.kind === 'gather') pts.push({ col: o.source.area.col, row: o.source.area.row, margin: 3 });
    }
    if (q.questArea) pts.push({ col: q.questArea.col, row: q.questArea.row, margin: 4 });
    if (q.defendTarget) pts.push({ col: q.defendTarget.col, row: q.defendTarget.row, margin: 6 });
    if (q.escortNpc) {
      pts.push({ col: q.escortNpc.startCol, row: q.escortNpc.startRow, margin: 3 });
      pts.push({ col: q.escortNpc.destCol, row: q.escortNpc.destRow, margin: 3 });
    }
    for (const clue of q.clues ?? []) pts.push({ col: clue.col, row: clue.row, margin: 2 });
  }
  return pts;
}

// Build the map registry and run procedural generation on maps with empty tiles
const rawMaps: Record<string, MapData> = {
  emerald_plains: EmeraldPlainsMap,
  twilight_forest: TwilightForestMap,
  anvil_mountains: AnvilMountainsMap,
  scorching_desert: ScorchingDesertMap,
  abyss_rift: AbyssRiftMap,
  // The homestead (余烬之塔): hand-authored, outside the zone progression (not in MapOrder).
  ember_tower: EmberTowerMap,
};

// Generate tiles/collisions for any map that has empty tile arrays
for (const key of Object.keys(rawMaps)) {
  const map = rawMaps[key];
  if (map.tiles.length === 0 && map.theme) {
    rawMaps[key] = MapGenerator.generate(map, externalLandmarks(key));
  }
}

export const AllMaps: Record<string, MapData> = rawMaps;

export const MapOrder = [
  'emerald_plains',
  'twilight_forest',
  'anvil_mountains',
  'scorching_desert',
  'abyss_rift',
];

export { EmeraldPlainsMap, TwilightForestMap, AnvilMountainsMap, ScorchingDesertMap, AbyssRiftMap };

/**
 * maps.json, map_gen.json + golden fixtures unreal/CoreTests/golden/maps/<id>.json and
 * unreal/CoreTests/golden/maps/_vectors.json.
 *
 * maps.json holds every zone's *authored* MapData (the anchors the C++ MapGen consumes — DECISIONS W10:
 * the generator is ported bit-exact and runs in the core), the generator inputs (theme, seed and the
 * external landmark list `externalLandmarks(mapId)` of src/data/maps/index.ts), and the hand-built
 * ember_tower grid. The golden fixtures are the TS MapGenerator output (AllMaps after module load) for the
 * C++ golden test: tiles as row strings, decorations in generation order, hashes and counts.
 */
import { AllMaps, MapOrder, EmeraldPlainsMap, TwilightForestMap, AnvilMountainsMap, ScorchingDesertMap, AbyssRiftMap } from '../../../../../src/data/maps';
import * as MapsIndex from '../../../../../src/data/maps';
import { EmberTowerMap, TOWER_SIZE, TOWER_CENTER, TOWER_PORTAL, TOWER_MEADOW, TOWER_PLOTS } from '../../../../../src/data/maps/ember_tower';
import * as MapGen from '../../../../../src/systems/MapGenerator';
import { AllSubDungeons, SubDungeonsByZone } from '../../../../../src/data/subDungeons';
import { NPCDefinitions } from '../../../../../src/data/npcs';
import type { MapData } from '../../../../../src/data/types';
import { assert, plain, sha256, formatJson, SCHEMA_VERSION, type TableResult } from '../util';

const exp = <T>(mod: unknown, name: string): T => {
  const v = (mod as Record<string, unknown>)[`__expose_${name}`];
  assert(v !== undefined, `exposed binding ${name} missing`);
  return v as T;
};

/** Hashes quoted by world-map-nav.md §3.3 / §4.9 (computed on this branch when the spec was written). */
const SPEC_GOLDEN: Record<string, { tilesSha: string; decorSha: string; walkable?: number }> = {
  emerald_plains: { tilesSha: '141fbb8218bb104c', decorSha: '2450ba59a1db0194', walkable: 13167 },
  twilight_forest: { tilesSha: '69726f1529497960', decorSha: '2badccd8450f9f42', walkable: 11668 },
  anvil_mountains: { tilesSha: 'd95cade88db96fdd', decorSha: 'af1cfaccfa3243ad', walkable: 12336 },
  scorching_desert: { tilesSha: 'da688f33090a24c0', decorSha: '11c339464ce5b157', walkable: 13225 },
  abyss_rift: { tilesSha: '02ea788855bd5589', decorSha: '1f548ca0721573b8', walkable: 12489 },
  ember_tower: { tilesSha: '71d784ebd7cd5c57', decorSha: 'f81acaad0c96b137', walkable: 1673 },
};

/**
 * Camp NPCs the port adds (DECISIONS I7: a stash keeper in the Chapter 1 camp). Appended to the camp's `npcs` (slot
 * order = the camp NPC offsets, quests-story-ch1.md 6.2) and recorded in the map's `port` block. Camps / NPCs are not
 * generator inputs, so the golden maps are unchanged.
 */
const PORT_CAMP_NPCS: Record<string, { campIndex: number; append: string[]; decision: string }[]> = {
  emerald_plains: [{ campIndex: 0, append: ['stash'], decision: 'I7' }],
};

const rowsOf = (grid: number[][]): string[] => grid.map(r => r.join(''));
const boolRows = (grid: boolean[][]): string[] => grid.map(r => r.map(b => (b ? '1' : '0')).join(''));

function hashes(m: MapData): { tilesSha: string; decorSha: string; walkable: number } {
  return {
    tilesSha: sha256(m.tiles.map(r => r.join(',')).join(';')).slice(0, 16),
    decorSha: sha256((m.decorations ?? []).map(d => `${d.col},${d.row},${d.type}`).join(';')).slice(0, 16),
    walkable: m.collisions.flat().filter(Boolean).length,
  };
}

export function exportMaps(): { tables: TableResult[]; golden: { path: string; text: string }[] } {
  const externalLandmarks = exp<(id: string) => { col: number; row: number; margin?: number }[]>(MapsIndex, 'externalLandmarks');
  const themeConfigs = exp<Record<string, unknown>>(MapGen, 'THEME_CONFIGS');
  const decorPools = exp<Record<string, unknown>>(MapGen, 'DECOR_POOLS');
  const tileIds = {
    grass: exp<number>(MapGen, 'TILE_GRASS'), dirt: exp<number>(MapGen, 'TILE_DIRT'), stone: exp<number>(MapGen, 'TILE_STONE'),
    water: exp<number>(MapGen, 'TILE_WATER'), wall: exp<number>(MapGen, 'TILE_WALL'), camp: exp<number>(MapGen, 'TILE_CAMP'),
    campWall: exp<number>(MapGen, 'TILE_CAMP_WALL'),
  };
  const blocking = [tileIds.water, tileIds.wall, tileIds.campWall];

  // Authored maps (before generation). The per-zone objects are not mutated by MapGenerator.generate.
  const authored: Record<string, MapData> = {
    emerald_plains: EmeraldPlainsMap, twilight_forest: TwilightForestMap, anvil_mountains: AnvilMountainsMap,
    scorching_desert: ScorchingDesertMap, abyss_rift: AbyssRiftMap, ember_tower: EmberTowerMap,
  };
  assert(Object.keys(authored).join() === Object.keys(AllMaps).join(), 'AllMaps registry changed — update maps.ts');

  const maps: Record<string, unknown> = {};
  const golden: { path: string; text: string }[] = [];
  const warnings: string[] = [];
  for (const [id, raw] of Object.entries(authored)) {
    const generated = raw.tiles.length === 0 && !!raw.theme;
    const { tiles, collisions, decorations, ...anchors } = plain(raw);
    const landmarks = generated ? externalLandmarks(id) : [];
    const portCampNpcs = PORT_CAMP_NPCS[id] ?? [];
    for (const add of portCampNpcs) {
      const camp = (anchors.camps as { npcs: string[] }[] | undefined)?.[add.campIndex];
      assert(camp, `${id}: PORT_CAMP_NPCS camp ${add.campIndex} missing`);
      for (const npcId of add.append) {
        assert(NPCDefinitions[npcId], `${id}: PORT_CAMP_NPCS unknown npc ${npcId}`);
        assert(!camp.npcs.includes(npcId), `${id}: camp ${add.campIndex} already has ${npcId} — drop the port addition`);
        camp.npcs.push(npcId);
      }
    }
    maps[id] = {
      ...anchors,
      ...(portCampNpcs.length ? { port: { campNpcsAdded: portCampNpcs } } : {}),
      generated,
      generator: generated ? { theme: raw.theme ?? 'plains', seed: raw.seed ?? 42, avoid: landmarks, avoidMarginDefault: 4 } : null,
      // Generated maps: tiles/collisions/decorations come from the C++ MapGen (golden in CoreTests). Hand-built
      // maps carry their grid: tiles as row strings of tile ids, collisions as '1'(walkable)/'0' rows.
      tiles: generated ? [] : rowsOf(tiles),
      collisions: generated ? [] : boolRows(collisions),
      decorations: generated ? [] : (decorations ?? []),
      safeZoneRadiusEffective: raw.safeZoneRadius ?? 9,
      nameKey: `data.zone.${id}`,
    };
    assert(generated || tiles.length === raw.rows, `${id}: hand-built map without tiles`);

    // Golden: the TS generator output.
    const gen = AllMaps[id];
    assert(gen.tiles.length === gen.rows && gen.tiles[0].length === gen.cols, `${id}: generated size`);
    if (generated) {
      for (let r = 0; r < gen.rows; r++) for (let c = 0; c < gen.cols; c++) {
        assert(gen.collisions[r][c] === !blocking.includes(gen.tiles[r][c]), `${id}: collisions not derived from tiles at ${c},${r}`);
      }
    }
    const h = hashes(gen);
    const spec = SPEC_GOLDEN[id];
    if (spec && (spec.tilesSha !== h.tilesSha || spec.decorSha !== h.decorSha || (spec.walkable !== undefined && spec.walkable !== h.walkable))) {
      warnings.push(`${id}: generated map differs from the spec's golden hashes (spec ${spec.tilesSha}/${spec.decorSha}/${spec.walkable}, now ${h.tilesSha}/${h.decorSha}/${h.walkable})`);
    }
    const tileCounts: Record<string, number> = {};
    for (const [name, v] of Object.entries(tileIds)) tileCounts[name] = gen.tiles.flat().filter(t => t === v).length;
    const decorCounts: Record<string, number> = {};
    for (const d of gen.decorations ?? []) decorCounts[d.type] = (decorCounts[d.type] ?? 0) + 1;
    const fixture = {
      schemaVersion: SCHEMA_VERSION,
      source: ['src/data/maps/index.ts', 'src/systems/MapGenerator.ts', `src/data/maps/${id}.ts`],
      id, cols: gen.cols, rows: gen.rows, generated,
      theme: gen.theme ?? 'plains', seed: gen.seed ?? 42,
      avoid: landmarks,
      golden: { ...h, tileCounts, decorCounts, decorationCount: (gen.decorations ?? []).length },
      tiles: rowsOf(gen.tiles),
      // Generated maps: collisions = tile not in {3,4,6}; hand-built maps carry their own.
      collisions: generated ? null : boolRows(gen.collisions),
      decorations: (gen.decorations ?? []).map(d => [d.col, d.row, d.type]),
    };
    golden.push({ path: `CoreTests/golden/maps/${id}.json`, text: formatJson(fixture) + '\n' });
  }
  for (const w of warnings) console.warn(`  warning: ${w}`);

  // Generator vectors for the C++ port (world-map-nav §4.9 item 1).
  const SeededRandom = exp<new (seed: number) => { next(): number; nextInt(a: number, b: number): number; state: number }>(MapGen, 'SeededRandom');
  const groveNoise = exp<(c: number, r: number, seed: number, scale: number) => number>(MapGen, 'groveNoise');
  const pickWeighted = exp<(pool: [string, number][], r: number) => string>(MapGen, 'pickWeighted');
  const rngVectors = [12345, 42, 1, 2147483646, 2147483647, 4817].map(seed => {
    const rng = new SeededRandom(seed);
    const states: number[] = [];
    const values: number[] = [];
    for (let i = 0; i < 8; i++) { values.push(rng.next()); states.push((rng as unknown as { state: number }).state); }
    const ints: number[] = [];
    for (let i = 0; i < 8; i++) ints.push(rng.nextInt(-3, 7));
    return { seed, states, values, nextIntMinus3To7: ints };
  });
  const r12345 = rngVectors[0];
  assert(r12345.states[0] === 207482415 && r12345.states[4] === 24794531, 'SeededRandom vector (spec §4.1)');
  const noise: unknown[] = [];
  for (const [seed, scale] of [[12345, 5], [24690, 7], [4817, 3.5]] as const) {
    for (const [c, r] of [[0, 0], [1, 0], [3, 7], [10, 10], [57, 93], [119, 119], [64, 2]]) noise.push([c, r, seed, scale, groveNoise(c, r, seed, scale)]);
  }
  const hypot: unknown[] = [];
  for (const [a, b] of [[3, 4], [1, 1], [0.5, 2.25], [5.84, 0.1], [-7, 3], [1e-8, 1], [117, 119], [0.1, 0.2], [2.5, 6.68], [13, 84]]) hypot.push([a, b, Math.hypot(a, b)]);
  // FP tripwire (Platform.h): FNV-1a 64 over the little-endian IEEE-754 bits of Math.hypot(a, b) for every integer pair
  // a, b in [-150, 150] (a outer, b inner). A core built with fast-math / FMA contraction drifts on thousands of pairs
  // (Kahan compensation reassociated), so CoreTests compares its JsHypot hash with this V8 value.
  const hypotGrid = (() => {
    const lo = -150, hi = 150;
    const mask = (1n << 64n) - 1n, prime = 0x100000001b3n;
    let h = 0xcbf29ce484222325n;
    const view = new DataView(new ArrayBuffer(8));
    let count = 0;
    for (let a = lo; a <= hi; a++) for (let b = lo; b <= hi; b++) {
      view.setFloat64(0, Math.hypot(a, b), true);
      for (let i = 0; i < 8; i++) { h ^= BigInt(view.getUint8(i)); h = (h * prime) & mask; }
      count++;
    }
    return { min: lo, max: hi, order: 'a outer, b inner', count, fnv1a64: h.toString(16).padStart(16, '0') };
  })();
  const pool: [string, number][] = [['tree', 5], ['tree_round', 3], ['bush', 3], ['grass', 2]];
  const picks = [0, 0.1, 0.3846153846153846, 0.3846153846153847, 0.5, 0.99, 0.999999].map(r => [r, pickWeighted(pool, r)]);
  golden.push({
    path: 'CoreTests/golden/maps/_vectors.json',
    text: formatJson({ schemaVersion: SCHEMA_VERSION, source: ['src/systems/MapGenerator.ts'],
      seededRandom: rngVectors, groveNoise: { columns: ['col', 'row', 'seed', 'scale', 'value'], rows: noise },
      mathHypot: { columns: ['a', 'b', 'Math.hypot(a,b)'], rows: hypot, grid: hypotGrid }, pickWeighted: { pool, picks } }) + '\n',
  });

  const subDungeons = plain(AllSubDungeons);
  const tables: TableResult[] = [
    {
      file: 'maps.json',
      source: ['src/data/maps/*.ts', 'src/data/maps/index.ts', 'src/data/subDungeons.ts', 'src/systems/MapGenerator.ts'],
      data: {
        order: [...MapOrder],
        defaultMap: 'emerald_plains',
        maps,
        emberTower: { size: TOWER_SIZE, center: TOWER_CENTER, portal: TOWER_PORTAL, meadow: TOWER_MEADOW, plots: plain(TOWER_PLOTS) },
        subDungeons,
        subDungeonsByZone: plain(SubDungeonsByZone),
      },
      counts: { maps: Object.keys(maps).length, subDungeons: Object.keys(subDungeons).length },
    },
    {
      file: 'map_gen.json',
      source: ['src/systems/MapGenerator.ts', 'src/data/maps/index.ts'],
      data: {
        tileIds,
        blockingTiles: blocking,
        themes: plain(themeConfigs),
        decorPools: plain(decorPools),
        defaults: { theme: 'plains', seed: 42, avoidMargin: 4 },
        landmarkRules: {
          lore: { margin: 2 }, miniBoss: { margin: 5 },
          quest: { objectiveLocation: 3, gatherArea: 3, questArea: 4, defendTarget: 6, escortStart: 3, escortDest: 3, clue: 2 },
          order: 'lore (LoreByZone order), mini-boss, then quests in AllQuests order: objectives, questArea, defendTarget, escort start/dest, clues',
        },
        rng: { kind: 'Park-Miller minimal standard', modulus: 2147483647, multiplier: 16807 },
      },
      counts: { themes: Object.keys(themeConfigs).length },
    },
  ];
  return { tables, golden };
}

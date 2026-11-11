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
import { MonstersByZone, getMonsterDef } from '../../../../../src/data/monsters';
import { MiniBossByZone } from '../../../../../src/data/miniBosses';
import type { MapData as RemakeMapData, MapSpawn as RemakeMapSpawn, MonsterDefinition as RemakeMonster,
  QuestDefinition as RemakeQuest, SubDungeonMapData as RemakeSubDungeon } from '../../../../GameData/types';
import { AllQuests } from '../../../../../src/data/quests/all_quests';
import { CHAPTER_CONFIG } from '../../../../GameData/config';
import { assert, plain, sha256, formatJson, SCHEMA_VERSION, type TableResult, type GoldenFile } from '../util';
import { withholdRemakeFields, noteRequiredKey } from '../remake';
import { flagsSetByQuests, isHandAuthored } from '../subdungeons';

const exp = <T>(mod: unknown, name: string): T => {
  const v = (mod as Record<string, unknown>)[`__expose_${name}`];
  assert(v !== undefined, `exposed binding ${name} missing`);
  return v as T;
};

/**
 * Hashes quoted by world-map-nav.md §3.3 / §4.9 (computed on this branch when the spec was written). emerald_plains is
 * the dark remake's one planned re-baseline (DECISIONS DR4, chapter1.md §17; package 1B part 1): the finale's two
 * explore landmarks (65,30) (50,55) became the Boneyard Gate defend target (10,32), the sealed Pit entrance (17,26) and
 * the echo lore spots (29,15) (38,47) (20,92) were added; the web map was 141fbb8218bb104c / 2450ba59a1db0194 / 13167.
 */
const SPEC_GOLDEN: Record<string, { tilesSha: string; decorSha: string; walkable?: number }> = {
  emerald_plains: { tilesSha: '54ce0471e96c5d0e', decorSha: '7a501f14066cbb08', walkable: 13169 },
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

/**
 * The modules `externalLandmarks(mapId)` (src/data/maps/index.ts) reads: generator `avoid` inputs, so they feed
 * maps.json and the golden maps through web code (provenance `deps`, main.ts).
 */
const LANDMARK_SOURCES = ['src/data/loreCollectibles.ts', 'src/data/miniBosses.ts', 'src/data/quests/all_quests.ts'];

const rowsOf = (grid: number[][]): string[] => grid.map(r => r.join(''));
const boolRows = (grid: boolean[][]): string[] => grid.map(r => r.map(b => (b ? '1' : '0')).join(''));

function hashes(m: MapData): { tilesSha: string; decorSha: string; walkable: number } {
  return {
    tilesSha: sha256(m.tiles.map(r => r.join(',')).join(';')).slice(0, 16),
    decorSha: sha256((m.decorations ?? []).map(d => `${d.col},${d.row},${d.type}`).join(';')).slice(0, 16),
    walkable: m.collisions.flat().filter(Boolean).length,
  };
}

/**
 * The spawn entries of a zone (a map, or K7 a hand-authored sub-dungeon), checked as LoadWorld.cpp / DataStore::Finalize
 * check them.
 */
function checkSpawns(where: string, zoneId: string, spawns: readonly RemakeMapSpawn[], cols: number, rows: number,
  walkable?: (col: number, row: number) => boolean): void {
  const id = zoneId;
  const knownMonster = (monsterId: string) => !!((MonstersByZone[id] ?? []).find(m => m.id === monsterId) ??
    getMonsterDef(monsterId) ?? Object.values(MiniBossByZone).find(m => m.id === monsterId));
  const storyBoss = CHAPTER_CONFIG.storyBoss.monsterId;
  const flagSetters = flagsSetByQuests(AllQuests as RemakeQuest[]);
  spawns.forEach((sp, i) => {
    const at = `${where}.spawns[${i}]`;
    if (sp.mix !== undefined) {
      assert(Array.isArray(sp.mix) && sp.mix.length > 0, `${at}.mix: an empty mix (omit it for the entry's one monster)`);
      let sum = 0;
      sp.mix.forEach((e, k) => {
        for (const key of Object.keys(e)) assert(['monsterId', 'count'].includes(key), `${at}.mix[${k}]: unknown field '${key}'`);
        assert(Number.isInteger(e.count) && e.count >= 1, `${at}.mix[${k}].count: an integer >= 1`);
        assert(typeof e.monsterId === 'string' && knownMonster(e.monsterId), `${at}.mix[${k}].monsterId: unknown monster '${e.monsterId}'`);
        assert(e.monsterId !== storyBoss, `${at}.mix[${k}].monsterId: the story boss keeps a spawn entry of its own`);
        sum += e.count;
      });
      assert(sum === sp.count, `${at}.mix: the mix sums to ${sum}, the entry's count is ${sp.count}`);
      assert(sp.mix.some(e => e.monsterId === sp.monsterId),
        `${at}.monsterId: '${sp.monsterId}' is not in the entry's mix (name one of its members)`);
    }
    if (sp.leader !== undefined) {
      for (const key of Object.keys(sp.leader)) assert(['monsterId', 'elite'].includes(key), `${at}.leader: unknown field '${key}'`);
      assert(sp.leader.elite === undefined || typeof sp.leader.elite === 'boolean', `${at}.leader.elite: a boolean`);
      const members = sp.mix ? sp.mix.map(e => e.monsterId) : sp.count > 0 ? [sp.monsterId] : [];
      assert(members.includes(sp.leader.monsterId), `${at}.leader: '${sp.leader.monsterId}' is not a member of the pack`);
      assert(!members.includes(storyBoss), `${at}.leader: the story boss keeps a spawn entry of its own`);
    }
    // K6 (LoadWorld.cpp, DataStore::Finalize): variants {whenFlag, monsterId, count 0..100}, one per flag, a known
    // monster of the zone, never the story boss nor on its entry; a flag no quest sets warns (it never holds).
    if (sp.variants !== undefined) {
      assert(Array.isArray(sp.variants) && sp.variants.length > 0, `${at}.variants: an empty list (omit it)`);
      const bossEntry = sp.monsterId === storyBoss || (sp.mix ?? []).some(e => e.monsterId === storyBoss);
      assert(!bossEntry, `${at}.variants: the story boss's entry has no variants (its spawn reads the entry)`);
      const flags = new Set<string>();
      sp.variants.forEach((v, k) => {
        const vAt = `${at}.variants[${k}]`;
        for (const key of Object.keys(v)) assert(['whenFlag', 'monsterId', 'count'].includes(key), `${vAt}: unknown field '${key}'`);
        assert(typeof v.whenFlag === 'string' && v.whenFlag !== '', `${vAt}.whenFlag: an empty flag`);
        assert(!flags.has(v.whenFlag), `${vAt}.whenFlag: a second variant of flag '${v.whenFlag}'`);
        flags.add(v.whenFlag);
        assert(Number.isInteger(v.count) && v.count >= 0 && v.count <= 100, `${vAt}.count: an integer 0..100`);
        assert(typeof v.monsterId === 'string' && knownMonster(v.monsterId), `${vAt}.monsterId: unknown monster '${v.monsterId}'`);
        assert(v.monsterId !== storyBoss, `${vAt}.monsterId: the story boss keeps a spawn entry of its own`);
        if (!flagSetters.has(v.whenFlag)) console.warn(`  warning: ${vAt}.whenFlag: no quest sets flag '${v.whenFlag}' (setsFlags)`);
      });
    }
    // K7 (LoadWorld.cpp, MapData.h MapSpawnDormantDef): dormant {wakeRange 0 < r <= 30, rise?, line? {dCol, dRow} each
    // -8..8 and not both 0 with every member on the grid - and, in a hand-authored grid, on a walkable tile -,
    // faceDeg? -360..360}.
    if (sp.dormant !== undefined) {
      const d = sp.dormant as unknown as Record<string, unknown>;
      const dAt = `${at}.dormant`;
      assert(d && typeof d === 'object' && !Array.isArray(d), `${dAt}: expected {wakeRange, rise?, line?, faceDeg?}`);
      for (const key of Object.keys(d)) assert(['wakeRange', 'rise', 'line', 'faceDeg'].includes(key), `${dAt}: unknown field '${key}'`);
      assert(typeof d.wakeRange === 'number' && d.wakeRange > 0 && d.wakeRange <= 30, `${dAt}.wakeRange: expected > 0 and <= 30`);
      assert(d.rise === undefined || typeof d.rise === 'boolean', `${dAt}.rise: a boolean`);
      if (d.line !== undefined) {
        const l = d.line as Record<string, unknown>;
        assert(l && typeof l === 'object', `${dAt}.line: expected {dCol, dRow}`);
        for (const key of Object.keys(l)) assert(['dCol', 'dRow'].includes(key), `${dAt}.line: unknown field '${key}'`);
        for (const key of ['dCol', 'dRow']) {
          assert(Number.isInteger(l[key]) && Math.abs(l[key] as number) <= 8, `${dAt}.line.${key}: an integer -8..8`);
        }
        assert(l.dCol !== 0 || l.dRow !== 0, `${dAt}.line: a line needs a step (dCol / dRow not both 0)`);
        // The most members the entry ever spawns: its count or a variant's (MapData.h SpawnMaxMembers).
        const members = Math.max(sp.count, ...(sp.variants ?? []).map(v => v.count));
        // The line starts where the pack stands (MapData.h DormantLineTile reads MapSpawnDef::pos): the entry's `at`
        // when it has one (map.spawnAt; its shape is checked with the map below), else its col / row.
        const lineAt = sp.at && Number.isInteger(sp.at.col) && Number.isInteger(sp.at.row) ? sp.at : sp;
        const tileOf = (k: number) => ({ col: lineAt.col + (l.dCol as number) * k, row: lineAt.row + (l.dRow as number) * k });
        const last = tileOf(Math.max(0, members - 1));
        assert(last.col >= 0 && last.row >= 0 && last.col < cols && last.row < rows,
          `${dAt}.line: the line's last member (${last.col},${last.row}) stands outside the grid`);
        // A hand-authored grid (`walkable` given): every member stands on a walkable tile of its own - at runtime a
        // member whose tile is not walkable would stand on the anchor, on top of member 0.
        for (let k = 1; walkable && k < members; k++) {
          const t = tileOf(k);
          assert(walkable(t.col, t.row), `${dAt}.line: member ${k} stands on (${t.col},${t.row}), which is not walkable`);
        }
      }
      assert(d.faceDeg === undefined || (typeof d.faceDeg === 'number' && d.faceDeg >= -360 && d.faceDeg <= 360), `${dAt}.faceDeg: expected -360..360`);
    }
  });
}

const BLOCKING_TILE_IDS = [3, 4, 6];  // water, wall, camp wall (MapGenerator TILE_*; asserted in exportMaps)

/**
 * K7 (LoadWorld.cpp, DataStore::Finalize; MapData.h SubDungeonDef): a generated sub-dungeon (the web's) carries no
 * remake field; a hand-authored one (with `tiles`) has rows x cols tile ids 0..6, collisions of the same size (default:
 * not water / wall / camp wall), its player start, exit, boss and spawn anchors on walkable tiles of the grid, an exit
 * that is not the start, a parent that is a map, a mini-boss and spawns of known monsters, and a `cleansedFlag` that is
 * its own. Returns the record to write (tiles / collisions as row strings).
 */
function checkSubDungeon(id: string, raw: RemakeSubDungeon, all: Readonly<Record<string, RemakeSubDungeon>>): Record<string, unknown> {
  const where = `subDungeons.${id}`;
  assert(raw.id === id, `${where}: id '${raw.id}' differs from its key`);
  assert(AllMaps[raw.parentZone] && !all[raw.parentZone], `${where}.parentZone: '${raw.parentZone}' is not a map`);
  assert(!AllMaps[id], `${where}: a map has the same id`);
  const hand = isHandAuthored(raw);
  const out = withholdRemakeFields('subDungeon', plain(raw), where) as unknown as Record<string, unknown>;
  if (!hand) {
    for (const key of ['collisions', 'decorations', 'presentationTheme', 'regions', 'corpseSeeds', 'storyDecorations', 'cleansedFlag']) {
      assert((raw as unknown as Record<string, unknown>)[key] === undefined, `${where}.${key}: only a hand-authored sub-dungeon (one with tiles) has it`);
    }
    for (const [i, sp] of raw.spawns.entries()) {
      for (const key of ['mix', 'leader', 'variants', 'dormant', 'guard', 'at']) {
        assert((sp as unknown as Record<string, unknown>)[key] === undefined, `${where}.spawns[${i}].${key}: ` +
          (key === 'at' ? "only a map's spawn has it" : "only a hand-authored sub-dungeon's spawn has it"));
      }
    }
    return out;
  }
  const tiles = raw.tiles!;
  assert(Number.isInteger(raw.cols) && Number.isInteger(raw.rows) && raw.cols >= 1 && raw.rows >= 1 && raw.cols <= 512 && raw.rows <= 512,
    `${where}: size ${raw.cols}x${raw.rows} outside 1..512`);
  assert(tiles.length === raw.rows && tiles.every(r => r.length === raw.cols), `${where}.tiles: expected ${raw.rows} rows of ${raw.cols} tiles`);
  assert(tiles.every(r => r.every(t => Number.isInteger(t) && t >= 0 && t <= 6)), `${where}.tiles: tile ids are 0..6`);
  const collisions = raw.collisions ?? tiles.map(r => r.map(t => !BLOCKING_TILE_IDS.includes(t)));
  assert(collisions.length === raw.rows && collisions.every(r => r.length === raw.cols), `${where}.collisions: expected ${raw.rows} rows of ${raw.cols}`);
  const walkable = (c: number, r: number) => Number.isInteger(c) && Number.isInteger(r) && r >= 0 && r < raw.rows && c >= 0 && c < raw.cols && collisions[r][c] === true;
  const anchors: [string, { col: number; row: number }][] = [['playerStart', raw.playerStart], ['exit', raw.exit], ['miniBoss', raw.miniBoss],
    ...raw.spawns.map((sp, i): [string, { col: number; row: number }] => [`spawns[${i}]`, sp])];
  for (const [name, p] of anchors) assert(walkable(p.col, p.row), `${where}.${name}: tile (${p.col},${p.row}) is not a walkable tile of the grid`);
  assert(raw.playerStart.col !== raw.exit.col || raw.playerStart.row !== raw.exit.row, `${where}.exit: the exit stands on the player start`);
  const parent = AllMaps[raw.parentZone];
  assert(raw.exit.returnCol >= 0 && raw.exit.returnRow >= 0 && raw.exit.returnCol < parent.cols && raw.exit.returnRow < parent.rows,
    `${where}.exit: the return tile (${raw.exit.returnCol},${raw.exit.returnRow}) lies outside ${raw.parentZone}`);
  for (const d of raw.decorations ?? []) {
    assert(d.col >= 0 && d.row >= 0 && d.col < raw.cols && d.row < raw.rows, `${where}.decorations: (${d.col},${d.row}) outside the grid`);
  }
  for (const sp of raw.spawns) assert(Number.isInteger(sp.count) && sp.count >= 0, `${where}.spawns: negative spawn count`);
  checkSpawns(where, raw.parentZone, raw.spawns, raw.cols, raw.rows, walkable);
  // 3B review (MapData.h MapSpawnDef::guard, DataStore::Finalize): at most one entry guards the guardian.
  let guards = 0;
  raw.spawns.forEach((sp, i) => {
    assert(sp.guard === undefined || typeof sp.guard === 'boolean', `${where}.spawns[${i}].guard: a boolean`);
    assert(sp.at === undefined, `${where}.spawns[${i}].at: only a generated map's spawn has it (a hand-authored sub-dungeon has no generator: move the entry)`);
    if (sp.guard) assert(++guards <= 1, `${where}.spawns[${i}].guard: a second guard entry (the guardian joins one pack)`);
  });
  const entrances = ((parent as unknown as RemakeMapData).subDungeonEntrances ?? []).filter(e => e.targetSubDungeon === id);
  if (!entrances.length) console.warn(`  warning: ${where}: no entrance of '${raw.parentZone}' leads to it`);
  if (raw.cleansedFlag !== undefined) {
    assert(typeof raw.cleansedFlag === 'string' && raw.cleansedFlag !== '', `${where}.cleansedFlag: an empty flag id (omit it)`);
    for (const [otherId, other] of Object.entries(all)) {
      assert(otherId === id || other.cleansedFlag !== raw.cleansedFlag, `${where}.cleansedFlag: '${raw.cleansedFlag}' is also the flag of '${otherId}'`);
    }
    for (const q of AllQuests as RemakeQuest[]) {
      assert(!(q.setsFlags ?? []).includes(raw.cleansedFlag), `${where}.cleansedFlag: '${raw.cleansedFlag}' is also set by quest '${q.id}' (setsFlags)`);
    }
  }
  // The zone banner and the enter line show its name (the core derives the key; DataStore::Finalize requires it).
  noteRequiredKey(`data.subDungeon.${id}.name`, where);
  // Grid in the hand-built map format: tile rows as digit strings, collisions as '1' (walkable) / '0' rows. A field the
  // schema gate withheld stays withheld.
  if ('tiles' in out) {
    out.tiles = rowsOf(tiles);
    out.collisions = boolRows(collisions);
  }
  return out;
}

export function exportMaps(): { tables: TableResult[]; golden: GoldenFile[] } {
  const externalLandmarks = exp<(id: string) => { col: number; row: number; margin?: number }[]>(MapsIndex, 'externalLandmarks');
  const themeConfigs = exp<Record<string, unknown>>(MapGen, 'THEME_CONFIGS');
  const decorPools = exp<Record<string, unknown>>(MapGen, 'DECOR_POOLS');
  const tileIds = {
    grass: exp<number>(MapGen, 'TILE_GRASS'), dirt: exp<number>(MapGen, 'TILE_DIRT'), stone: exp<number>(MapGen, 'TILE_STONE'),
    water: exp<number>(MapGen, 'TILE_WATER'), wall: exp<number>(MapGen, 'TILE_WALL'), camp: exp<number>(MapGen, 'TILE_CAMP'),
    campWall: exp<number>(MapGen, 'TILE_CAMP_WALL'),
  };
  const blocking = [tileIds.water, tileIds.wall, tileIds.campWall];
  assert(blocking.join() === BLOCKING_TILE_IDS.join(), 'MapGenerator blocking tile ids changed: update BLOCKING_TILE_IDS');

  // Authored maps (before generation). The per-zone objects are not mutated by MapGenerator.generate.
  const authored: Record<string, MapData> = {
    emerald_plains: EmeraldPlainsMap, twilight_forest: TwilightForestMap, anvil_mountains: AnvilMountainsMap,
    scorching_desert: ScorchingDesertMap, abyss_rift: AbyssRiftMap, ember_tower: EmberTowerMap,
  };
  assert(Object.keys(authored).join() === Object.keys(AllMaps).join(), 'AllMaps registry changed — update maps.ts');

  const maps: Record<string, unknown> = {};
  const golden: GoldenFile[] = [];
  const warnings: string[] = [];
  for (const [id, raw] of Object.entries(authored)) {
    const generated = raw.tiles.length === 0 && !!raw.theme;
    for (const e of (raw as { subDungeonEntrances?: { id: string; sealed?: boolean; sealedTextKey?: string }[] }).subDungeonEntrances ?? []) {
      // K1 (ZoneRuntime / LoadWorld.cpp): a sealed entrance shows its sealedTextKey; only a sealed one has one.
      assert(!!e.sealed === !!e.sealedTextKey, `maps.${id}.subDungeonEntrances.${e.id}: sealed and sealedTextKey go together`);
    }
    // K3 (LoadWorld.cpp, DataStore::Finalize): a corpse seed is {col, row, monsterId} on the grid, of a monster whose body
    // leaves a corpse entry (corpse.leaves corpse / remains).
    ((raw as unknown as RemakeMapData).corpseSeeds ?? []).forEach((seed, i) => {
      const at = `maps.${id}.corpseSeeds[${i}]`;
      for (const k of Object.keys(seed)) assert(['col', 'row', 'monsterId'].includes(k), `${at}: unknown field '${k}'`);
      assert(Number.isInteger(seed.col) && Number.isInteger(seed.row) && seed.col >= 0 && seed.row >= 0 &&
        seed.col < raw.cols && seed.row < raw.rows, `${at}: tile (${seed.col},${seed.row}) outside the ${raw.cols}x${raw.rows} grid`);
      const def = ((MonstersByZone[id] ?? []).find(m => m.id === seed.monsterId) ?? getMonsterDef(seed.monsterId) ??
        Object.values(MiniBossByZone).find(m => m.id === seed.monsterId)) as RemakeMonster | undefined;
      assert(def, `${at}.monsterId: unknown monster '${seed.monsterId}'`);
      const leaves = def!.corpse?.leaves ?? 'none';
      assert(leaves !== 'none', `${at}.monsterId: '${seed.monsterId}' leaves no corpse (corpse.leaves)`);
    });
    // K5 (LoadWorld.cpp, DataStore::Finalize): a spawn's mix {monsterId, count >= 1} names monsters and sums to the entry's
    // count, and the entry's own monsterId is one of them (the story-boss spawn and the quest guide's spawn points read
    // it); its leader {monsterId, elite?} is a member (the entry's monsterId without a mix); the story boss keeps a spawn
    // entry of its own (its once-per-visit spawn reads the entry's monsterId). Not generator inputs: the golden is
    // unchanged. K7: the spawns of a hand-authored sub-dungeon carry the same fields (checkSubDungeon); a generated
    // sub-dungeon's (the web's two) carry none.
    checkSpawns(`maps.${id}`, id, raw.spawns as RemakeMapSpawn[], raw.cols, raw.rows);
    (raw.spawns as RemakeMapSpawn[]).forEach((sp, i) =>
      assert(sp.guard === undefined, `maps.${id}.spawns[${i}].guard: only a spawn of a hand-authored sub-dungeon guards its guardian`));
    // Stage H (LoadWorld.cpp, MapData.h MapSpawnDef::generatorAnchor): `at` {col, row} is where the pack stands; the
    // entry's col / row stay the generator's inputs, so the golden map does not move. A generated map only, an integer
    // tile of the grid that is not the entry's own, and a walkable tile of the generated map.
    (raw.spawns as RemakeMapSpawn[]).forEach((sp, i) => {
      if (sp.at === undefined) return;
      const at = `maps.${id}.spawns[${i}].at`;
      assert(generated, `${at}: only a generated map's spawn has it (move the entry of a hand-built map)`);
      assert(sp.at && typeof sp.at === 'object' && !Array.isArray(sp.at), `${at}: expected {col, row}`);
      for (const k of Object.keys(sp.at)) assert(['col', 'row'].includes(k), `${at}: unknown field '${k}'`);
      assert(Number.isInteger(sp.at.col) && Number.isInteger(sp.at.row) && sp.at.col >= 0 && sp.at.row >= 0 &&
        sp.at.col < raw.cols && sp.at.row < raw.rows, `${at}: tile (${sp.at.col},${sp.at.row}) outside the ${raw.cols}x${raw.rows} grid`);
      assert(sp.at.col !== sp.col || sp.at.row !== sp.row, `${at}: the entry's own tile (omit it)`);
      // Not inside a camp's safe radius (DataStore::Finalize; the zone's test is strict): every placement try would be
      // refused there and the whole pack would stand on `at` itself, inside the camp.
      for (const c of raw.camps) {
        assert(Math.hypot(sp.at.col - c.col, sp.at.row - c.row) >= (raw.safeZoneRadius ?? 9),
          `${at}: tile (${sp.at.col},${sp.at.row}) inside the safe radius of the camp at (${c.col},${c.row})`);
      }
    });
    // DataStore::Finalize: an exit leads to a zone proper (K7: a sub-dungeon is entered through an entrance).
    for (const e of raw.exits) {
      assert(!(AllSubDungeons as Record<string, unknown>)[e.targetMap],
        `maps.${id}.exits: target '${e.targetMap}' is a sub-dungeon (it is entered through a subDungeonEntrance)`);
      assert(AllMaps[e.targetMap], `maps.${id}.exits: unknown target map '${e.targetMap}'`);
    }
    const { tiles, collisions, decorations, ...anchors } = withholdRemakeFields('map', plain(raw), `maps.${id}`);
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
    // Stage H: a pack that stands off its generator anchor (`spawns[].at`) stands on a walkable tile of the generated map.
    for (const [i, sp] of (raw.spawns as RemakeMapSpawn[]).entries()) {
      if (sp.at === undefined) continue;
      assert(!blocking.includes(gen.tiles[sp.at.row][sp.at.col]),
        `maps.${id}.spawns[${i}].at: tile (${sp.at.col},${sp.at.row}) of the generated map is not walkable (tile id ${gen.tiles[sp.at.row][sp.at.col]})`);
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
    golden.push({ path: `CoreTests/golden/maps/${id}.json`, text: formatJson(fixture) + '\n',
      deps: [...fixture.source, ...(generated ? LANDMARK_SOURCES : [])] });
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
    deps: ['src/systems/MapGenerator.ts'],
  });

  // K7: sub-dungeons go through the remake field gate; a hand-authored one ships its grid like a hand-built map.
  const allSubDungeons = AllSubDungeons as unknown as Record<string, RemakeSubDungeon>;
  const subDungeons: Record<string, unknown> = {};
  for (const [id, raw] of Object.entries(allSubDungeons)) subDungeons[id] = checkSubDungeon(id, raw, allSubDungeons);
  // An entrance into a hand-authored sub-dungeon stands in that sub-dungeon's parent zone (its exit returns there).
  for (const [mapId, raw] of Object.entries(authored)) {
    for (const e of (raw as unknown as RemakeMapData).subDungeonEntrances ?? []) {
      const target = allSubDungeons[e.targetSubDungeon];
      assert(e.sealed || target, `maps.${mapId}.subDungeonEntrances.${e.id}: unknown sub-dungeon '${e.targetSubDungeon}'`);
      assert(!target || !isHandAuthored(target) || target.parentZone === mapId,
        `maps.${mapId}.subDungeonEntrances.${e.id}: leads to '${e.targetSubDungeon}', a sub-dungeon of '${target?.parentZone}'`);
    }
  }
  const tables: TableResult[] = [
    {
      file: 'maps.json',
      source: ['src/data/maps/*.ts', 'src/data/maps/index.ts', 'src/data/subDungeons.ts', 'src/systems/MapGenerator.ts'],
      deps: LANDMARK_SOURCES,
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

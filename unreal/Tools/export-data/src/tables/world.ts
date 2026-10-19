/**
 * World tables: npcs, dialogue_trees, shops, lore, achievements, random_events, zone_moods,
 * world_constants, homestead, abyss_run, render_quality.
 */
import { NPCDefinitions } from '../../../../../src/data/npcs';
import { DialogueTrees } from '../../../../../src/data/dialogueTrees';
import { MiniBossDialogues } from '../../../../../src/data/miniBosses';
import { LoreByZone, AllLoreEntries } from '../../../../../src/data/loreCollectibles';
import * as AchievementMod from '../../../../../src/systems/AchievementSystem';
import { DEFAULT_RANDOM_EVENT_CONFIG, RANDOM_EVENT_DEFS, ZONE_EVENT_DATA } from '../../../../../src/systems/RandomEventSystem';
import { ZONE_MOODS, ZONE_PALETTES } from '../../../../../src/graphics/ZonePalette';
import * as WeatherMod from '../../../../../src/systems/WeatherSystem';
import * as LightingMod from '../../../../../src/systems/LightingSystem';
import { CAMP_THEMES } from '../../../../../src/data/camp-themes';
import * as ZoneSceneMod from '../../../../../src/scenes/ZoneScene';
import { ZONE_CAMERA_ZOOM } from '../../../../../src/scenes/ZoneScene';
import * as Homestead from '../../../../../src/data/homestead';
import * as EmberTowerMod from '../../../../../src/systems/EmberTower';
import { BOONS, BOON_RARITY_WEIGHT, CURSES, FLOOR_THEMES, TIER_BASE_LEVEL, TIER_LEVEL_STEP } from '../../../../../src/data/abyssRun';
import * as RenderQualityMod from '../../../../../src/rendering/RenderQuality';
import { GAME_WIDTH, GAME_HEIGHT, TILE_WIDTH, TILE_HEIGHT } from '../../../../../src/config';
import { getItemBase } from '../../../../../src/data/items/bases';
import { AllMaps } from '../../../../../src/data/maps';
import type { DialogueTree } from '../../../../../src/data/types';
import { CHAPTER_CONFIG } from '../../../../GameData/config';
import { assert, assertSource, plain, type TableResult } from '../util';
import { noteConfig, overrideById, overrideEntries, withholdRemakeFields } from '../remake';

const ZS = 'src/scenes/ZoneScene.ts';
const exp = <T>(mod: unknown, name: string): T => {
  const v = (mod as Record<string, unknown>)[`__expose_${name}`];
  assert(v !== undefined, `exposed binding ${name} missing`);
  return v as T;
};

/**
 * Wares the port does not sell (dropped from shops.json and npcs.json shopItems; listed in shops.json port.removedWares):
 * I4 removes the town-portal scroll (the portal is free); I2: every item drops identified, so no ID scrolls in
 * milestone 1. The item bases stay in item_bases.json (old data / later milestones).
 */
const REMOVED_WARES: Record<string, string> = { c_tp_scroll: 'I4', c_id_scroll: 'I2' };

/**
 * FIX loot Q12 (loot-items-inventory.md 12.7 / 19; world-map-nav.md 13 wandering_merchant): the event's merchantItems
 * are not item bases (`iron_sword`, `hp_potion`, ...), so the web opened an empty shop. The port maps every listed id to
 * a real base of that zone's tier (maps.json levelRange); priceMultiplier stays unused (W10 parity).
 */
const WANDERING_MERCHANT_IDS: Record<string, Record<string, string>> = {
  emerald_plains: { iron_sword: 'w_short_sword', leather_armor: 'a_leather_armor', hp_potion: 'c_hp_potion_s', mp_potion: 'c_mp_potion_s' },
  twilight_forest: { steel_sword: 'w_broad_sword', chain_armor: 'a_chain_mail', hp_potion: 'c_hp_potion_m', mp_potion: 'c_mp_potion_m' },
  anvil_mountains: { mithril_sword: 'w_claymore', plate_armor: 'a_plate_armor', hp_potion: 'c_hp_potion_m', mp_potion: 'c_mp_potion_m' },
  scorching_desert: { desert_blade: 'w_flamberge', desert_armor: 'a_dragon_armor', hp_potion: 'c_hp_potion_l', mp_potion: 'c_mp_potion_m' },
  abyss_rift: { abyssal_blade: 'w_demon_blade', demon_armor: 'a_demon_armor', hp_potion: 'c_hp_potion_l', mp_potion: 'c_mp_potion_m' },
};

function treeIdOf(tree: DialogueTree | undefined): string | null {
  if (!tree) return null;
  const id = Object.entries(DialogueTrees).find(([, t]) => t === tree)?.[0];
  assert(id, 'NPC dialogue tree not registered in DialogueTrees');
  return id;
}

export function exportWorld(): TableResult[] {
  // ── npcs.json ──
  const npcs: Record<string, unknown> = {};
  for (const [id, npc] of Object.entries(NPCDefinitions)) {
    const { dialogueTree, ...rest } = npc;
    const out: Record<string, unknown> = { ...withholdRemakeFields('npc', plain(rest), `npcs.${id}`), dialogueTreeId: treeIdOf(dialogueTree),
      nameKey: `data.npc.${id}.name` };
    if (npc.shopItems) out.shopItems = npc.shopItems.filter(item => !REMOVED_WARES[item]);
    npcs[id] = out;
  }

  // ── dialogue_trees.json ──
  const trees: Record<string, unknown> = {};
  for (const [id, t] of Object.entries(DialogueTrees)) trees[id] = { kind: 'npc', ...plain(t) };
  for (const [id, t] of Object.entries(MiniBossDialogues)) {
    assert(!(id in trees), `dialogue tree id clash ${id}`);
    trees[id] = { kind: 'miniBoss', ...plain(t) };
  }
  let nodeCount = 0;
  for (const t of Object.values(trees) as { nodes: Record<string, unknown>; startNodeId: string }[]) {
    assert(t.nodes[t.startNodeId], 'dialogue start node missing');
    nodeCount += Object.keys(t.nodes).length;
  }

  // ── shops.json ──
  const shops: Record<string, string[]> = {};
  const removedWares: { npcId: string; itemId: string; decision: string }[] = [];
  for (const [id, npc] of Object.entries(NPCDefinitions)) {
    if (!npc.shopItems) continue;
    for (const item of npc.shopItems) assert(getItemBase(item), `shop ${id} sells unknown item ${item}`);
    shops[id] = npc.shopItems.filter(item => {
      const decision = REMOVED_WARES[item];
      if (decision) removedWares.push({ npcId: id, itemId: item, decision });
      return !decision;
    });
  }
  for (const item of Object.keys(REMOVED_WARES)) assert(removedWares.some(r => r.itemId === item), `REMOVED_WARES: nobody sells ${item}`);
  // RandomEventSystem ZONE_EVENT_DATA, with the remake's per-zone pools (GameData/world.ts) — feeds both files.
  const zoneEvents = overrideEntries('randomEvents', ['random_events.json', 'shops.json'], ZONE_EVENT_DATA);
  const eventMerchant: Record<string, unknown> = {};
  for (const [zone, z] of Object.entries(zoneEvents)) {
    const idMap = WANDERING_MERCHANT_IDS[zone] ?? {};
    const unknownItemIds = z.merchantItems.filter(i => !getItemBase(i));
    for (const id of unknownItemIds) assert(idMap[id], `WANDERING_MERCHANT_IDS: ${zone} does not map ${id}`);
    for (const [from, to] of Object.entries(idMap)) {
      assert(z.merchantItems.includes(from), `WANDERING_MERCHANT_IDS: ${zone} maps ${from}, which it does not sell`);
      assert(getItemBase(to) && !REMOVED_WARES[to], `WANDERING_MERCHANT_IDS: ${zone}.${from} -> unknown or removed base ${to}`);
    }
    eventMerchant[zone] = { items: [...z.merchantItems], unknownItemIds, idMap: { ...idMap } };
  }
  for (const zone of Object.keys(WANDERING_MERCHANT_IDS)) assert(zoneEvents[zone], `WANDERING_MERCHANT_IDS: unknown zone ${zone}`);

  // ── lore.json / achievements.json ──
  const achievements = overrideById('achievements', ['achievements.json'],
    exp<{ id: string }[]>(AchievementMod, 'ACHIEVEMENTS'));

  assertSource('src/data/homestead.ts', 'return Math.max(6, 16 - 2 * level);', 'return level <= 0 ? 0 : 4 + 4 * level;',
    'if (rng() < 0.12 + 0.03 * level) return LEY_FRUIT_ID;', 'const hp = rng() < 0.6;');

  // ── random_events.json ──
  assertSource('src/systems/RandomEventSystem.ts', 'private readonly TRIGGER_MOVE_THRESHOLD = 3;');

  // ── zone_moods.json ── (each web table with the remake's entries from GameData/world.ts)
  const moodsOut = ['zone_moods.json'];
  const zoneMoods = overrideEntries('zoneMoods.moods', moodsOut, ZONE_MOODS);
  const zoneMoodTables = {
    themeByZone: overrideEntries('zoneMoods.themeByZone', moodsOut, exp<Record<string, string>>(LightingMod, 'ZONE_THEME_BY_ID')),
    palettes: overrideEntries('zoneMoods.palettes', moodsOut, ZONE_PALETTES),
    weather: overrideEntries('zoneMoods.weather', moodsOut, exp<Record<string, unknown>>(WeatherMod, 'ZONE_WEATHER')),
    weatherByTheme: overrideEntries('zoneMoods.weatherByTheme', moodsOut, exp<Record<string, string>>(WeatherMod, 'THEME_WEATHER')),
    campThemes: overrideEntries('zoneMoods.campThemes', moodsOut, CAMP_THEMES),
    // DECISIONS P14 (art pass): the UE ExponentialHeightFog per mood (the web had no fog; its haze becomes the fog).
    fog: overrideEntries('zoneMoods.fog', moodsOut, {
      plains: { color: 0xf2e8cc, maxOpacity: 0.22, density: 0.85 },
      forest: { color: 0x8c86c8, maxOpacity: 0.32, density: 1.1 },
      mountain: { color: 0xcedcee, maxOpacity: 0.3, density: 1.0 },
      desert: { color: 0xffdea6, maxOpacity: 0.26, density: 0.9 },
      abyss: { color: 0x6e2a40, maxOpacity: 0.35, density: 1.2 },
    }),
  };
  assertSource(ZS, 'emerald_plains: 0x88cc88,', 'twilight_forest: 0x66aa66,', 'anvil_mountains: 0x998888,',
    'scorching_desert: 0xffaa44,', 'abyss_rift: 0xff6622,', 'const tint = tints[this.currentMapId] || 0xccccaa;',
    'radius: 80, color: 0xffeedd, intensity: 0.4,', 'y: pos.y - 8, radius: 120, color: 0xff8800, intensity: 0.85, flicker: true,',
    'y: pos.y - 40, radius: 70, color: 0xff6600, intensity: 0.65, flicker: true,');

  // ── world_constants.json ──
  const gate = CHAPTER_CONFIG.sealedGate;
  noteConfig(['world_constants.json'], 'sealedGate');
  assert(AllMaps[gate.exitTo] && Object.values(AllMaps).some(m => m.exits.some(e => e.targetMap === gate.exitTo)),
    `GameData/config.ts sealedGate.exitTo: no exit leads to '${gate.exitTo}'`);
  assertSource(ZS, 'if (dSq < 2.25) {', 'this.mapData.safeZoneRadius ?? 9,', 'this.cameras.main.fadeIn(400);',
    'this.time.delayedCall(1100, () => {', 'this.time.delayedCall(1500, () => {',
    'if (subEntrance && distanceSq(this.player.tileCol, this.player.tileRow, subEntrance.col, subEntrance.row) <= 9) {',
    'const nearbyMonsters = this.monsterGrid.queryRadius(this.player.tileCol, this.player.tileRow, 10);',
    'if (npc && npc.isNearPlayer(this.player.tileCol, this.player.tileRow, 3)) {', 'if (dSq < 3.24 && dSq < bestDist) {',
    'if (hiddenChest && distanceSq(this.player.tileCol, this.player.tileRow, hiddenChest.col, hiddenChest.row) <= 4) {');

  const rq = {
    profiles: plain(exp<Record<string, unknown>>(RenderQualityMod, 'PROFILES')),
    resolutionScale: plain(exp<Record<string, number>>(RenderQualityMod, 'RESOLUTION_BY_QUALITY')),
    selection: { lowPixelBudget: 5_000_000, highMaxPixelBudget: 3_700_000, highMinDpr: 2, constrainedCores: 4, constrainedMemoryGb: 4 },
    clampResolution: { step: 0.5, min: 1, max: 2 },
  };
  assertSource('src/rendering/RenderQuality.ts', 'pixelBudget > 5_000_000', 'env.devicePixelRatio >= 2 && pixelBudget <= 3_700_000',
    'env.hardwareConcurrency <= 4', 'env.deviceMemory <= 4', 'return Math.max(1, Math.min(2, Math.round(value * 2) / 2));');

  return [
    {
      file: 'npcs.json',
      source: ['src/data/npcs.ts', 'src/data/dialogueTrees.ts'],
      data: { npcs, keyOrderMatters: 'questGiverOf tie-break' },
      counts: { npcs: Object.keys(npcs).length },
    },
    {
      file: 'dialogue_trees.json',
      source: ['src/data/dialogueTrees.ts', 'src/data/miniBosses.ts'],
      data: { trees },
      counts: { trees: Object.keys(trees).length, nodes: nodeCount },
    },
    {
      file: 'shops.json',
      source: ['src/data/npcs.ts', 'src/systems/RandomEventSystem.ts'],
      data: { shops, wanderingMerchant: eventMerchant, port: { removedWares, wanderingMerchantIdMap: 'loot Q12 FIX' } },
      counts: { shops: Object.keys(shops).length },
    },
    {
      file: 'lore.json',
      source: ['src/data/loreCollectibles.ts'],
      data: { byZone: plain(LoreByZone), pickupRangeSq: 4, i18n: { name: 'data.lore.<id>.name', text: 'data.lore.<id>.text' } },
      counts: { entries: AllLoreEntries.length },
    },
    {
      file: 'achievements.json',
      source: ['src/systems/AchievementSystem.ts'],
      data: { achievements: plain(achievements), port: { killCounting: 'one count per kill', exploreAll: 'distinct zones', decision: 'Q2' } },
      counts: { achievements: achievements.length },
    },
    {
      file: 'random_events.json',
      source: ['src/systems/RandomEventSystem.ts'],
      data: {
        config: plain(DEFAULT_RANDOM_EVENT_CONFIG),
        triggerMoveThresholdTiles: 3,
        defs: plain(RANDOM_EVENT_DEFS),
        zones: plain(zoneEvents),
        port: { resetMoveCounterAfterEveryRoll: true, decision: 'W6' },
        i18n: { message: 'sys.event.msg.<type>', rescue: 'sys.event.rescue.<zone>', puzzle: 'sys.event.puzzle.<zone>.{prompt,solution,reward}' },
      },
      counts: { types: RANDOM_EVENT_DEFS.length, zones: Object.keys(zoneEvents).length },
    },
    {
      file: 'zone_moods.json',
      source: ['src/graphics/ZonePalette.ts', 'src/systems/WeatherSystem.ts', 'src/systems/LightingSystem.ts', 'src/data/camp-themes.ts', ZS],
      data: {
        themeByZone: plain(zoneMoodTables.themeByZone),
        moods: plain(zoneMoods),
        palettes: plain(zoneMoodTables.palettes),
        weather: plain(zoneMoodTables.weather),
        weatherByTheme: plain(zoneMoodTables.weatherByTheme),
        ambientDustTint: { emerald_plains: 0x88cc88, twilight_forest: 0x66aa66, anvil_mountains: 0x998888, scorching_desert: 0xffaa44, abyss_rift: 0xff6622, fallback: 0xccccaa },
        lights: {
          hero: { radiusPx: 80, color: 0xffeedd, intensity: 0.4 },
          campfire: { radiusPx: 120, color: 0xff8800, intensity: 0.85, flicker: true, heightPx: 8 },
          torch: { radiusPx: 70, color: 0xff6600, intensity: 0.65, flicker: true, heightPx: 40 },
        },
        campThemes: plain(zoneMoodTables.campThemes),
        // DECISIONS P14 (art pass): the UE ExponentialHeightFog per mood (the web had no fog; its haze becomes the fog).
        // Colours are 0xRRGGBB sRGB like every colour here; maxOpacity is FogMaxOpacity; density is the look-dev
        // extinction, opacity 1 - 2^(-density x metres / 10) past the start (UE FogDensity = density / ln 2 x abyss.FogScale,
        // the ln 2 of UE's line integral). Start = camera distance + 1 m and height falloff 0.2 are code constants.
        port: {
          fog: plain(zoneMoodTables.fog),
          decision: 'P14 (art pass)',
        },
      },
      counts: { themes: Object.keys(zoneMoods).length },
    },
    {
      file: 'world_constants.json',
      source: [ZS, 'src/config.ts', 'src/systems/MapGenerator.ts'],
      data: {
        tile: { widthPx: TILE_WIDTH, heightPx: TILE_HEIGHT, portUnitsPerTile: 100, decision: 'S4' },
        designResolution: [GAME_WIDTH, GAME_HEIGHT],
        webCameraZoom: ZONE_CAMERA_ZOOM,
        // W1 (art pass): framingTiles[0] across the horizontal FOV at the focus sets the default distance
        // (13.4 x 50 / tan(17.5 deg) = 2125 cm, hero ~150 px feet-to-plume at 720p); heroPx720 / defaultDistanceCm document it.
        camera: { yawDeg: 45, pitchDeg: -50, fovDeg: 35, framingTiles: [13.4, 10.4], heroPx720: 150, defaultDistanceCm: 2125,
          zoomRange: [0.75, 1.25], followLagSec: 0.12, decision: 'W1 (art pass)' },
        heroSpeed: { moveSpeed: 120, pxPerTile: 36, rampMs: 90, stopMs: 60, clickStartsInstantly: true, decision: 'S5' },
        holdMoveRepathMs: exp<number>(ZoneSceneMod, 'HOLD_MOVE_REPATH_MS'),
        exitRadiusSq: 2.25,
        exitArmDistance: Math.sqrt(6),
        exitArmDistanceSq: 6, // W8 strict distSq > 6 (Math.sqrt(6) ** 2 is 5.999999999999999)
        exitArmNote: 'W8: an exit fires only after the hero was > sqrt(6) tiles from it since entering the zone',
        campfire: {
          radiusTiles: exp<number>(ZoneSceneMod, 'CAMPFIRE_RECOVERY_RADIUS'),
          hpMul: exp<number>(ZoneSceneMod, 'CAMPFIRE_HP_REGEN_MULTIPLIER'),
          mpMul: exp<number>(ZoneSceneMod, 'CAMPFIRE_MANA_REGEN_MULTIPLIER'),
        },
        safeZoneRadiusDefault: 9,
        hiddenAreaExploreRadius: 10,
        escortCatchUpTiles: exp<number>(ZoneSceneMod, 'ESCORT_CATCH_UP_TILES'),
        townPortal: { channelMs: 1500, cancelOnMove: true, cancelOnDamageFraction: 0.1, destination: 'nearest camp', decision: 'W3, U10' },
        deathRespawnMs: 1100,
        zoneFadeInMs: 400,
        interact: { lootRadiusSq: 4, hiddenChestRadiusSq: 4, puzzleRadiusSq: 4, npcRange: 3, npcPickRadiusSq: 3.24, entranceRadiusSq: 9, touchTalkRange: 2.5,
          walkThenAct: true, decision: 'W8, Q6' },
        tallDecorBlocks: { decision: 'W5', note: 'trees, boulders, tents, walls, wells, statues block walking; small decor does not' },
        sealedChapter2Gate: { exitTo: CHAPTER_CONFIG.sealedGate.exitTo, messageKey: CHAPTER_CONFIG.sealedGate.messageKey,
          decision: 'W7 (port string, i18n PORT_STRINGS)' },
      },
      counts: { constants: 20 },
    },
    {
      file: 'homestead.json',
      source: ['src/data/homestead.ts', 'src/systems/EmberTower.ts'],
      data: {
        towerZoneId: Homestead.TOWER_ZONE_ID,
        towerUnlockQuest: Homestead.TOWER_UNLOCK_QUEST,
        leyFruitId: Homestead.LEY_FRUIT_ID,
        buildings: plain(Homestead.BUILDINGS),
        gemCombineCount: Homestead.GEM_COMBINE_COUNT,
        expeditionOptions: plain(Homestead.EXPEDITION_OPTIONS),
        blessings: plain(Homestead.BLESSINGS),
        blessingDurationMs: Homestead.BLESSING_DURATION_MS,
        embers: {
          killElite: Homestead.embersForKill({ elite: true }, 0), killMiniBoss: Homestead.embersForKill({ isMiniBoss: true }, 0),
          killAffixedPerAffix: Homestead.embersForKill({}, 1), killPlain: Homestead.embersForKill({}, 0),
          questMain: Homestead.embersForQuest({ category: 'main', rewards: {} }), questSide: Homestead.embersForQuest({ category: 'side', rewards: {} }),
        },
        garden: {
          intervalByLevel: [0, 1, 2, 3, 4, 5, 6].map(l => Homestead.gardenInterval(l)),
          capacityByLevel: [0, 1, 2, 3, 4, 5, 6].map(l => Homestead.gardenCapacity(l)),
          interval: { base: 16, perLevel: -2, min: 6 }, capacity: { base: 4, perLevel: 4, zeroAtLevel0: true },
          yield: { leyFruitBase: 0.12, leyFruitPerLevel: 0.03, hpShare: 0.6 },
        },
        // EmberTower.retroGrantPets (called on every zone create): a turned-in quest grants its story beat's ley-beast
        // when the pet is missing (a save that landed between the beat's trigger and its end lost the grantPet).
        storyPets: plain(exp<{ quest: string; petId: string }[]>(EmberTowerMod, 'STORY_PETS')),
        port: { towerHiddenInMilestone1: true, decision: 'Q3' },
      },
      counts: { buildings: Homestead.BUILDINGS.length, storyPets: exp<unknown[]>(EmberTowerMod, 'STORY_PETS').length },
    },
    {
      file: 'abyss_run.json',
      source: ['src/data/abyssRun.ts'],
      data: { boons: plain(BOONS), boonRarityWeight: plain(BOON_RARITY_WEIGHT), curses: plain(CURSES), floorThemes: plain(FLOOR_THEMES),
        tierBaseLevel: TIER_BASE_LEVEL, tierLevelStep: TIER_LEVEL_STEP },
      counts: { boons: BOONS.length, curses: CURSES.length, floorThemes: FLOOR_THEMES.length },
    },
    {
      file: 'render_quality.json',
      source: ['src/rendering/RenderQuality.ts'],
      data: rq,
      counts: { profiles: Object.keys(rq.profiles).length },
    },
  ];
}

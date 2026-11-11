/**
 * The UE set-piece layer (implementation-plan.md §2.1 / §4 U4; dark-remake chapter1.md §1, §2, §5, §9.1, §10): authored
 * prop groups the Unreal build places at tiles when it builds a zone — Knellwatch's bell frame, hearth dressing and Name
 * Wall, Ashwheat's cottages and burned bridge, the Sheepfold's wagon ring, the Breach Rim's dais, ash circles and banner
 * ring, the helmet stakes of the Dead Ranks approach, the Boneyard Gate and the open Pit mouth; and, since Wave 3, the
 * dressing of the Pit of the Faithful itself (zone `sub_rf_pit`: helm niches, lamps, tag curtains, racks, piers, the
 * Flame Niche). Exported to `Data/set_pieces.json` by unreal/Tools/export-data/src/tables/setpieces.ts.
 *
 * Presentation only: set pieces are **not** map-generator inputs (the golden maps do not move) and **not** walkability
 * (non-blocking by default, DECISIONS DR15). The core reads the table for checks and the placement maths
 * (abyss/data/SetPieceData.h, abyss/world/SetPieces.h); `AAbyssZoneActor::BuildSetPieces` (Private/World/AbyssSetPieces.cpp)
 * draws them through the zone dressing (instanced, occlusion fade, the zone's `@theme` art first).
 *
 * Conventions (tile space, the core's: col = +x / east, row = +y / south; angles 0 = +col, 90 = +row):
 * - `gameId` is an art-manifest game id (Data/assets.json); the zone's presentation theme is tried first
 *   (`<id>@march`), then `fallbackIds`. Several assets on one id are variants: `variant` picks one by index (asset-name
 *   order: _A = 0, _B = 1), else the tile hash does.
 * - `facing`: `yaw` is the actor yaw itself (UE degrees, the mesh's own frame, e.g. the dais's crack faces −X at yaw 0);
 *   every other kind turns the mesh's front (its −Y in Blender, the kit convention) toward something: `camera` (the W1
 *   camera, like a camp prop without a front), `tile`, `anchor` (the anchor point before `offset`: a camp's hearth, a
 *   region's centre) or `ringCentre`; `front` gives the front's direction in degrees. `turnDeg` is added.
 * - `walkOver` names why the hero may cross the footprint (a ground layer, a walkable dais, the burned bridge's gap):
 *   CoreTests setpieces_test checks that no other piece covers a tile of a path the bot's route walks, an interaction
 *   point (NPC, lore, clue, explore centre, story decoration, entrance, defend target) or a blocking decoration.
 * - Sockets (`effects`, `emitters`, `cues`) are the asset's manifest sockets (UE cm in the mesh frame), turned by the
 *   placement's yaw. `effects` loop a VFX recipe there, `emitters` hand the spot to an audio emitter of
 *   Audio/Export/audio_manifest.json (`ambience.emitters`), `cues` play a recipe there when one of the audio cue triggers
 *   plays (UAbyssAudioSystem::OnCuePlayed). `echoes` pose those lore echoes' MI_AF_Echo figures in the piece's frame
 *   (the art contract's `nextTo`: A10 authored the figures beside this mesh at yaw 0); `echoTurnDeg` turns that figure
 *   group about the piece's origin on top of the piece's yaw, so the figures can kneel on the lore spot's side of a
 *   mesh whose own yaw is fixed by other art (PresentationTests setpieces_test keeps every figure on screen from its
 *   lore spot).
 * - `yieldsToDefendTarget`: hidden while a quest's defend target (the same gate art) stands on it; the target takes the
 *   piece's yaw.
 * - `onWall` (Wave 3, the crypt kit): the piece is an overlay on a **wall** tile (its anchor), its front turned to the
 *   floor tile it faces (`facing: {kind: 'tile'}` on a neighbour: the actor yaw is then the kit's atan2(-dc, dr)); the
 *   wall block under it stays. Every other piece stands on open ground (CoreTests setpieces_test checks both).
 */

export type SetPieceTile = readonly [col: number, row: number];

export type SetPieceAnchor =
  /** A tile (sub-tile allowed). */
  | { readonly kind: 'tile'; readonly col: number; readonly row: number }
  /** Camp `camp` (1-based, the map's `camps` order): its centre (the campfire), or the centre of its first `prop`. */
  | { readonly kind: 'camp'; readonly camp: number; readonly prop?: string }
  /** A `subDungeonEntrances` id of the map. */
  | { readonly kind: 'entrance'; readonly id: string }
  /** A mood `regions` id of the map (its centre). */
  | { readonly kind: 'region'; readonly id: string }
  /** The defend target of a quest of the zone. */
  | { readonly kind: 'defendTarget'; readonly quest: string };

export type SetPieceFacing =
  | { readonly kind: 'yaw'; readonly deg: number }
  | { readonly kind: 'front'; readonly deg: number }
  | { readonly kind: 'camera' }
  | { readonly kind: 'tile'; readonly col: number; readonly row: number }
  | { readonly kind: 'anchor' }
  | { readonly kind: 'ringCentre' };

/** Members k = 0..count-1 at `startDeg + k × stepDeg` (default 360 / count) on a circle round the anchor (+ offset). */
export interface SetPieceRing {
  readonly count: number;
  readonly radius: number;
  readonly startDeg: number;
  readonly stepDeg?: number;
  /** Member indices left out (gaps). */
  readonly skip?: readonly number[];
  /** Member k shows variant `variants[k % n]`. */
  readonly variants?: readonly number[];
}

export interface SetPieceSocketEffect { readonly socket: string; readonly recipe: string }
export interface SetPieceSocketEmitter { readonly socket: string; readonly emitter: string }
export interface SetPieceSocketCue { readonly cues: readonly string[]; readonly socket: string; readonly recipe: string }

export interface SetPieceDef {
  /** Unique in its zone (logs, tests). */
  readonly id: string;
  /** chapter1.md §1 area it dresses. */
  readonly area: string;
  readonly gameId: string;
  readonly fallbackIds?: readonly string[];
  readonly variant?: number;
  readonly anchor: SetPieceAnchor;
  readonly offset?: SetPieceTile;
  readonly ring?: SetPieceRing;
  readonly facing: SetPieceFacing;
  readonly turnDeg?: number;
  readonly scale?: number;
  /** Placed at the same spot and yaw (the ground layers: the ditch bed, the poured ash). */
  readonly partners?: readonly string[];
  readonly effects?: readonly SetPieceSocketEffect[];
  readonly emitters?: readonly SetPieceSocketEmitter[];
  readonly cues?: readonly SetPieceSocketCue[];
  readonly echoes?: readonly string[];
  /** Degrees the echo figures' group turns about the piece's origin on top of its yaw (needs `echoes`). */
  readonly echoTurnDeg?: number;
  /** Tall dressing: fades between the camera and the hero / nearby monsters (R1). */
  readonly occluder?: boolean;
  readonly walkOver?: string;
  readonly yieldsToDefendTarget?: boolean;
  /** A wall overlay: anchored on a wall tile, facing a floor neighbour (the crypt kit's niches, lamps, curtains). */
  readonly onWall?: boolean;
  /** Design note (exported). */
  readonly note?: string;
}

export interface SetPieceZone {
  readonly zone: string;
  readonly pieces: readonly SetPieceDef[];
}

/**
 * Knellwatch's bell (the keepers' ley-iron bell the fort was built around, story-bible §2) ripples on every toll of the
 * fort's bell: the tolls near / far, the dawn toll and the siege's self-ring ("钟无人自鸣 / The bell rings with no hand
 * on it", cs_rf_siege step 2 and every 20-30 s while q_rf_last_toll is active; 1C review). The Boneyard Gate's own bell
 * answers the self-ring as an echo (the art lane's note), so at the siege the gate's bell moves too.
 */
const BELL_CUE: SetPieceSocketCue = {
  cues: ['bell_toll', 'bell_toll_far', 'bell_dawn_toll', 'bell_self_ring'], socket: 'bell', recipe: 'camp.bell_toll',
};

/** The Red Field (emerald_plains; chapter1.md §1). */
const RED_FIELD: SetPieceZone = {
  zone: 'emerald_plains',
  pieces: [
    // ── Knellwatch, camp 1 (chapter1.md §2). The camp kit itself is the presentation theme's (camp_*@march). ──
    {
      id: 'knellwatch_hearth_dressing', area: 'knellwatch', gameId: 'setpiece_hearth_dressing',
      anchor: { kind: 'camp', camp: 1, prop: 'campfire' }, facing: { kind: 'camera' },
      walkOver: 'knee-high stools and a blanket line round the hearth; the hearth cell stays the respawn point. Its ' +
        'blockingCells are not walkability in Wave 1 (a core + golden change, DECISIONS DR15)',
      note: 'the hearth\'s stools and blanket line, at the campfire\'s tile and its camera-facing yaw (A10 note)',
    },
    {
      id: 'knellwatch_bell_frame', area: 'knellwatch', gameId: 'setpiece_bell_frame',
      anchor: { kind: 'camp', camp: 1 }, offset: [-2.6, 4.4], facing: { kind: 'anchor' },
      cues: [BELL_CUE], occluder: true,
      note: 'the squat belfry with the cracked bell, its door toward the hearth; in the camp\'s south-west corner, ' +
        '>= 2 tiles from every NPC slot and off the paths (the art lane\'s (0, 3.9) stood on the south lane, the ' +
        'south-east corner is where the roads leave the camp)',
    },
    {
      id: 'knellwatch_name_wall', area: 'knellwatch', gameId: 'setpiece_name_wall',
      anchor: { kind: 'camp', camp: 1 }, offset: [-4.45, 0.32], facing: { kind: 'front', deg: 0 }, occluder: true,
      note: 'hundreds of bronze name-tags on the inner west palisade between its torches, facing into the camp',
    },
    // ── The burn pit: the Pit mouth at entrance_rf_pit, open since Wave 3 (the way down into sub_rf_pit; Wave 1 drew
    // the sealed grate here: same anchor, yaw and footprint, plan *3D notes* item 7). ──
    {
      id: 'pit_mouth', area: 'burn_pit', gameId: 'setpiece_pit_mouth_open', fallbackIds: ['setpiece_pit_mouth_sealed'],
      anchor: { kind: 'entrance', id: 'entrance_rf_pit' }, facing: { kind: 'camera' },
      effects: [{ socket: 'smoke', recipe: 'camp.torch@march' }],
      walkOver: 'the pit mouth on the entrance tile: the hero walks to its rim and climbs down (socket entrance is the ' +
        'entrance point, ladder_top where the ladder leaves the hole)',
      note: 'the fire is out; the march brazier\'s smoulder (no light) still rises at socket smoke',
    },
    // ── The Boneyard Gate (M5's defend target, chapter1.md §5). ──
    {
      id: 'boneyard_gate', area: 'boneyard_gate', gameId: 'setpiece_boneyard_gate',
      anchor: { kind: 'defendTarget', quest: 'q_rf_last_toll' }, facing: { kind: 'yaw', deg: 180 },
      cues: [{ cues: ['bell_self_ring'], socket: 'bell', recipe: 'camp.bell_toll' }],   // echoes Knellwatch's bell
      occluder: true, yieldsToDefendTarget: true,
      walkOver: 'the burial road runs through the lychgate: the gate is M5\'s defend target, whose tile the core keeps ' +
        'open (the waves reach it, the hero stands beside it), as it does for the target the quest draws here',
      note: 'the lychgate where the burial road leaves the fort, tile-aligned with its front toward the fort and the ' +
        'camera (A10 scene knellwatch_gate); during M5 the defend target (decor_defend_boneyard_gate, the same mesh) ' +
        'stands here instead',
    },
    // ── Ashwheat (40,52) r8 and the burned bridge (38,48) (chapter1.md M3). ──
    {
      id: 'ashwheat_bridge', area: 'ashwheat', gameId: 'setpiece_burned_bridge',
      anchor: { kind: 'tile', col: 38, row: 48 }, facing: { kind: 'yaw', deg: 90 },
      partners: ['setpiece_burned_bridge_ditch'], effects: [{ socket: 'smoulder', recipe: 'camp.torch@march' }],
      echoes: ['lore_rf_echo_02'],
      walkOver: 'the road crosses the dry ditch through the burned span',
      note: 'the span runs north-south over the ditch; the whole abutment (mesh -X) is the near, Knellwatch bank, so ' +
        'its socket echo lies 0.9 tiles from lore_rf_echo_02 (38,47) and the echo\'s two figures wait there, turned ' +
        'to the far bank',
    },
    {
      id: 'ashwheat_cottage_west', area: 'ashwheat', gameId: 'setpiece_ashwheat_cottage', variant: 0,
      anchor: { kind: 'tile', col: 37.4, row: 53 }, facing: { kind: 'camera' }, turnDeg: 20,
      emitters: [{ socket: 'knock', emitter: 'ashwheat_knock' }], occluder: true,
      note: 'doors barred from inside, a cold candle in the window, knocking behind the door',
    },
    {
      id: 'ashwheat_cottage_south', area: 'ashwheat', gameId: 'setpiece_ashwheat_cottage', variant: 1,
      anchor: { kind: 'tile', col: 40.6, row: 56.2 }, facing: { kind: 'camera' }, turnDeg: -10,
      emitters: [{ socket: 'knock', emitter: 'ashwheat_knock' }], occluder: true,
    },
    {
      id: 'ashwheat_cottage_east', area: 'ashwheat', gameId: 'setpiece_ashwheat_cottage', variant: 0,
      anchor: { kind: 'tile', col: 43.4, row: 50.8 }, facing: { kind: 'camera' }, turnDeg: -40,
      emitters: [{ socket: 'knock', emitter: 'ashwheat_knock' }], occluder: true,
    },
    // ── The Sheepfold (60,55): the Corpseherd's ring of overturned wagons, gaps north and south (chapter1.md §10). ──
    {
      id: 'sheepfold_wagons', area: 'sheepfold', gameId: 'setpiece_sheepfold_wagon',
      anchor: { kind: 'tile', col: 60, row: 55 }, offset: [0, -0.5],
      ring: { count: 10, radius: 5.8, startDeg: -18, stepDeg: -36, skip: [2, 4, 5, 7], variants: [0, 1] },
      facing: { kind: 'ringCentre' }, occluder: true,
      note: 'six wagons on the ring of A10 scene sheepfold (r 6 there; 5.8 about a point half a tile north of her ' +
        'spawn keeps the south-east wagon off the rock), beds open toward the centre, gaps north and south ' +
        '(chapter1.md §10) and west, where the wagon ruts come in from Ashwheat (the road the bot and the player take)',
    },
    // ── The Breach Rim (15,95), r 16 (chapter1.md §9.1). ──
    {
      id: 'breach_dais', area: 'breach_rim', gameId: 'setpiece_breach_dais',
      anchor: { kind: 'region', id: 'breach_rim' }, facing: { kind: 'yaw', deg: 0 },
      echoes: ['lore_rf_echo_03'], echoTurnDeg: 135,
      walkOver: 'the walkable dais (top 5-7 cm): Valdras\'s floor',
      note: 'the Seal of Veins at the region\'s centre, the breach crack west toward the chasm (yaw 0, chapter1.md ' +
        '§9.1); lore_rf_echo_03\'s kneeling figure, authored at the west breach facing the seal, turns 135° about the ' +
        'centre with its group: it kneels at the north-east kerb, still facing the seal, ~3 tiles from the echo\'s ' +
        'lore spot (20,92) instead of 8.5 (the edge of the screen when the hero reads it)',
    },
    {
      id: 'breach_circle_west', area: 'breach_rim', gameId: 'setpiece_ritual_circle',
      anchor: { kind: 'region', id: 'breach_rim' }, offset: [-4, -4], facing: { kind: 'yaw', deg: 0 },
      partners: ['setpiece_ritual_circle_ash'], walkOver: 'a walkable ash circle (heaps and poured ash)',
    },
    {
      id: 'breach_circle_east', area: 'breach_rim', gameId: 'setpiece_ritual_circle',
      anchor: { kind: 'region', id: 'breach_rim' }, offset: [4, 4], facing: { kind: 'yaw', deg: 0 },
      partners: ['setpiece_ritual_circle_ash'], walkOver: 'a walkable ash circle (heaps and poured ash)',
    },
    {
      id: 'breach_banner_ring', area: 'breach_rim', gameId: 'setpiece_banner_ring',
      anchor: { kind: 'region', id: 'breach_rim' },
      ring: { count: 12, radius: 9, startDeg: 3.5408, stepDeg: -30, variants: [1, 0, 0] },
      facing: { kind: 'ringCentre' }, occluder: true,
      note: 'twelve scorched banners at r 9 round the dais, their fronts toward it (A10 scene breach_rim, turned by ' +
        'half a step so no banner stands on the road in from the Dead Ranks, nor on rock)',
    },
    // Helmets on spears along the Dead Ranks approach (30,80) -> the Rim: three where the field begins (the Red Field
    // vignette, by the Red Banner) and three at the Rim's north-east edge (A10 scene breach_rim).
    {
      id: 'dead_ranks_stake_1', area: 'dead_ranks', gameId: 'setpiece_helmet_stake', variant: 0,
      anchor: { kind: 'tile', col: 37.6, row: 73.4 }, facing: { kind: 'camera' }, turnDeg: -25,
    },
    {
      id: 'dead_ranks_stake_2', area: 'dead_ranks', gameId: 'setpiece_helmet_stake', variant: 1,
      anchor: { kind: 'tile', col: 35.9, row: 75.6 }, facing: { kind: 'camera' },
    },
    {
      id: 'dead_ranks_stake_3', area: 'dead_ranks', gameId: 'setpiece_helmet_stake', variant: 0,
      anchor: { kind: 'tile', col: 38.6, row: 74.6 }, facing: { kind: 'camera' }, turnDeg: 25,
    },
    {
      id: 'rim_stake_1', area: 'breach_rim', gameId: 'setpiece_helmet_stake', variant: 1,
      anchor: { kind: 'region', id: 'breach_rim' }, offset: [3.54, -4.38], facing: { kind: 'camera' }, turnDeg: 25,
    },
    {
      id: 'rim_stake_2', area: 'breach_rim', gameId: 'setpiece_helmet_stake', variant: 0,
      anchor: { kind: 'region', id: 'breach_rim' }, offset: [6.5, -3.0], facing: { kind: 'camera' },
    },
    {
      id: 'rim_stake_3', area: 'breach_rim', gameId: 'setpiece_helmet_stake', variant: 1,
      anchor: { kind: 'region', id: 'breach_rim' }, offset: [0.6, -6.6], facing: { kind: 'camera' }, turnDeg: -25,
    },
  ],
};

// ── The Pit of the Faithful (sub_rf_pit; chapter1.md §9.2, maps/sub_rf_pit.ts). The floor and the wall blocks are the
// map's tiles (terrain style `pit`, `outcrop_plains@pit`) and the stair is the exit's own art (`exit_portal@pit`); here is
// the dressing of the crypt kit (Art/blender/environment/crypt_pit.py, vignette_pit.py): overlays on wall tiles facing the
// floor - every one on a wall whose face looks toward -row (key-lit at the W1 camera) or -col (the shade band), the
// two sides the camera sees -, the Tag Cellar's free-standing racks (their tiles are blocked in the map) and ash
// drifts. The lamps are `setpiece_pit_lamp`: UE swaps each for `setpiece_pit_lamp_lit` when the Pit is cleansed
// (vfx_recipes.json `dark.pit.cleanse`: nearest first, 420 ms apart). ──
const WALL_NOTE = 'an overlay on the wall tile\'s face: it hangs proud of the block, above the floor';
/** A crypt overlay on wall tile (col, row), its front to the floor tile (col + dc, row + dr). */
const onWall = (id: string, area: string, gameId: string, col: number, row: number, dc: number, dr: number,
  more: Partial<SetPieceDef> = {}): SetPieceDef => ({
  id, area, gameId, anchor: { kind: 'tile', col, row }, facing: { kind: 'tile', col: col + dc, row: row + dr },
  onWall: true, walkOver: WALL_NOTE, ...more,
});
/** A niche lamp (unlit until `rf_pit_cleansed`): sockets flame (the wick) and light (the pool, 0.75 m out on the floor). */
const lamp = (id: string, area: string, col: number, row: number, dc: number, dr: number): SetPieceDef =>
  onWall(`${area}_lamp_${id}`, area, 'setpiece_pit_lamp', col, row, dc, dr);
/** The kit's corner piece: wall ends, door jambs, inner corners (four-sided: any 90 degree yaw). */
const pier = (id: string, area: string, col: number, row: number, dc: number, dr: number): SetPieceDef =>
  onWall(`${area}_pier_${id}`, area, 'setpiece_pit_wall_pier', col, row, dc, dr, { occluder: true });
/** Ash drifted over the flags: ground cover (variant 0 has no ink hull), walk-over. */
const ashDrift = (id: string, area: string, col: number, row: number, variant: number, deg: number): SetPieceDef => ({
  id: `${area}_ash_${id}`, area, gameId: 'setpiece_pit_ash_drift', variant, anchor: { kind: 'tile', col, row },
  facing: { kind: 'yaw', deg }, walkOver: 'ash drifted over the floor (ground cover, 7 cm)',
});
const range = (from: number, to: number): number[] => Array.from({ length: to - from + 1 }, (_, k) => from + k);

const PIT: SetPieceZone = {
  zone: 'sub_rf_pit',
  pieces: [
    // ── The Ash Steps (5,5): the stair alcove's jambs, ash at the stair's foot and in the far corner. ──
    pier('stair_w', 'pit_ash_steps', 3, 9, 0, -1),
    pier('stair_e', 'pit_ash_steps', 7, 9, 0, -1),
    lamp('s', 'pit_ash_steps', 9, 9, 0, -1),
    lamp('e', 'pit_ash_steps', 12, 7, -1, 0),
    pier('door', 'pit_ash_steps', 12, 6, -1, 0),
    // The first helms (3B review): the entry beat's third line speaks of rows of helmets set into the walls, and the
    // Helm Wall is two rooms on. Three niches on the Steps' south wall, east of the stair - (8,9) is 5 tiles from the
    // arrival tile, inside the hero's light - and three along the passage's south face, on the way to the wall itself.
    ...[8, 10, 11].map((col, k) => onWall(`pit_ash_steps_niche_${k + 1}`, 'pit_ash_steps', 'setpiece_pit_helm_niche',
      col, 9, 0, -1, { variant: k % 2 })),
    ...[13, 15, 17].map((col, k) => onWall(`pit_ash_steps_niche_p${k + 1}`, 'pit_ash_steps', 'setpiece_pit_helm_niche',
      col, 6, 0, -1, { variant: (k + 1) % 2 })),
    ashDrift('foot_w', 'pit_ash_steps', 3.6, 7.4, 0, 40),
    ashDrift('foot_e', 'pit_ash_steps', 6.5, 7.3, 1, 200),
    ashDrift('alcove', 'pit_ash_steps', 5.0, 9.2, 0, 150),
    ashDrift('corner', 'pit_ash_steps', 2.6, 2.6, 1, 80),
    // ── The Helm Wall (24,8): kettle helms in the niches of the south wall (A / B alternating), behind the rank of six
    // (row 11, cols 20-25); two more niches and the lamps on the east wall. ──
    pier('door', 'pit_helm_wall', 20, 13, 0, -1),
    ...range(21, 28).map((col, k) => onWall(`pit_helm_wall_niche_${k + 1}`, 'pit_helm_wall', 'setpiece_pit_helm_niche',
      col, 13, 0, -1, { variant: k % 2 })),
    lamp('s', 'pit_helm_wall', 29, 13, 0, -1),
    onWall('pit_helm_wall_niche_e1', 'pit_helm_wall', 'setpiece_pit_helm_niche', 31, 8, -1, 0, { variant: 0 }),
    onWall('pit_helm_wall_niche_e2', 'pit_helm_wall', 'setpiece_pit_helm_niche', 31, 12, -1, 0, { variant: 1 }),
    lamp('e1', 'pit_helm_wall', 31, 6, -1, 0),
    lamp('e2', 'pit_helm_wall', 31, 10, -1, 0),
    // ── The passage down to the Tag Cellar. ──
    lamp('bend', 'pit_passage', 20, 16, -1, 0),
    // ── The Tag Cellar (10,24): tag curtains on the south wall, single strands (four to a tile, the three variants in
    // turn) on the east wall either side of the door, four racks in two rows with an aisle between. ──
    ...range(4, 16).filter(col => col !== 7 && col !== 13).map(col => onWall(`pit_tag_cellar_curtain_${col}`,
      'pit_tag_cellar', 'setpiece_pit_tag_curtain', col, 30, 0, -1)),
    lamp('s1', 'pit_tag_cellar', 7, 30, 0, -1),
    lamp('s2', 'pit_tag_cellar', 13, 30, 0, -1),
    ...[19, 20, 21, 22, 27, 28, 29].flatMap(row => [-0.33, -0.11, 0.11, 0.33].map((along, k) =>
      onWall(`pit_tag_cellar_strand_${row}_${k + 1}`, 'pit_tag_cellar', 'setpiece_pit_tag_strand', 17, row, -1, 0,
        { offset: [0, along], variant: (row + k) % 3 }))),
    pier('door_n', 'pit_tag_cellar', 17, 23, -1, 0),
    pier('door_s', 'pit_tag_cellar', 17, 26, -1, 0),
    ...([[6.5, 22], [12.5, 22], [6.5, 26], [12.5, 26]] as const).map(([col, row], k): SetPieceDef => ({
      id: `pit_tag_cellar_rack_${k + 1}`, area: 'pit_tag_cellar', gameId: 'setpiece_pit_tag_rack',
      anchor: { kind: 'tile', col, row }, facing: { kind: 'yaw', deg: 0 }, occluder: true,
      note: 'free-standing, 2 x 1 along the row; its two tiles are blocked in the map (maps/sub_rf_pit.ts `o`)',
    })),
    // ── The Pitwarden's Cut (28,26). ──
    pier('door', 'pitwardens_cut', 24, 32, 0, -1),
    lamp('s1', 'pitwardens_cut', 27, 32, 0, -1),
    lamp('s2', 'pitwardens_cut', 32, 32, 0, -1),
    lamp('e', 'pitwardens_cut', 35, 26, -1, 0),
    pier('e', 'pitwardens_cut', 35, 22, -1, 0),
    ashDrift('corner', 'pitwardens_cut', 33.4, 30.6, 1, 120),
    // ── The Flame Niche (20,35): the empty niche on the south wall, facing -row and the door (pivot on the middle of
    // its three wall tiles; its two steps project 0.9 m over the floor row in front), a lamp either side. Sockets
    // niche, turn_in and mark_1..5: where the cleanse's `prop.pit_cleanse.niche` / `.mark` play (UE, EvAreaCleansed). ──
    onWall('pit_flame_niche', 'pit_flame_niche', 'setpiece_pit_flame_niche', 20, 38, 0, -1, {
      occluder: true,
      walkOver: 'the niche stands on the wall; its two steps (0.31 m) reach over the floor row in front and are walked on',
      note: 'an empty niche shaped like a flame, five burn marks round it (chapter1.md §9.2): what S7\'s turn-in line ' +
        'speaks of',
    }),
    lamp('w', 'pit_flame_niche', 17, 38, 0, -1),
    lamp('e', 'pit_flame_niche', 23, 38, 0, -1),
    lamp('side', 'pit_flame_niche', 26, 35, -1, 0),
  ],
};

export const SET_PIECE_ZONES: readonly SetPieceZone[] = [RED_FIELD, PIT];

import type { SubDungeonMapData } from '../types';

/**
 * 信者之坑 / The Pit of the Faithful (`sub_rf_pit`, Docs/dark-remake/chapter1.md §9.2; DECISIONS DR7; Wave 3, package
 * 3B): the floor under the burn pit where the hero woke. A hand-authored 40 x 40 sub-dungeon (K7: a `subDungeons`
 * record with `tiles`, registered in ../subDungeons.ts), entered through `entrance_rf_pit` at (17,26) of the Red Field
 * and left by the Ash Steps' stair.
 *
 * Five rooms at the places chapter1.md lists, on one path (every walkable tile is reached from the stair; the content
 * test pins it):
 *
 *   the Ash Steps (5,5) ── the Helm Wall (24,8)
 *                              │
 *   the Tag Cellar (10,24) ────┘
 *        │
 *        └── the Pitwarden's Cut (28,26) ── the Flame Niche (20,35)
 *
 * - **The Ash Steps** 灰阶: the hero arrives at (5,5); the stair is the exit, in a three-tile alcove of the south wall
 *   (the exit tile (5,10) in front of a straight wall row, the flight rising behind it: Art `exit_portal@pit`). The
 *   alcove keeps the exit's 1.5-tile trigger off the room's floor: a fight on the Steps does not walk the hero out.
 *   Nothing stands in the room: the hero lands in quiet and the entry beat (`cs_rf_pit`) plays to an empty stair.
 *   The four breachclots wait in the passage east of it (12-17, 4-5), past the seep: more than twice their aggro
 *   range from the arrival tile wherever the placement jitter puts them (3B review; the content test pins it).
 * - **The Helm Wall** 盔墙: six Risen footmen in a rank before the niche wall (the south wall: its faces look toward
 *   -row, the key-lit side at the W1 camera), dormant until the hero comes within 3 tiles of one or one is hit; then
 *   the whole rank wakes (K7 dormant entry). The rank stands on (20..25, 11) and the way on is the door (18-19, 13)
 *   at its west end: the room's west column is at most 2.5 tiles from the rank's first man, so nobody walks from the
 *   passage to the door without waking the wall (3B review: at (21..26) the column was exactly 3.000 tiles away and
 *   the wall could be walked past; the content test walks it).
 * - **The Tag Cellar** 名牌窖: four racks of name-tags (2 x 1, blocking: `o` below) and the dead the tags were cut
 *   from; four gravegnawers that came down to feed, two arbalesters behind the far racks.
 * - **The Pitwarden's Cut** 掘坑官之室: dug earth; the Pitwarden (`miniBoss`, ../subDungeons.ts) behind two
 *   shieldbearers, the pit he keeps digging already full. The two are his guard (`guard`: one pack with him) and stand
 *   as a formation either side of the line from the door to him, (26,25) and (26,27), facing the door: provoke any of
 *   the three and the three come (3B review: as a plain entry they scattered, patrolled and let him fight alone).
 * - **The Flame Niche** 火龛: no monsters; the empty niche on the south wall (facing -row, the door on the far side).
 *   S7's last objective (an explore spot on the paving before it) brings the hero here after the cleanse.
 *
 * Levels: the roster stands at its own levels (monsters.md §2.0: clot 1, footman 2, gnawer 3, arbalester 5,
 * shieldbearer 7, the Pitwarden 8); `levelRange` is the banner's label (K7 as built: a sub-dungeon raises nobody).
 * Nothing respawns inside and no random event rolls (every sub-dungeon's rule); once `rf_pit_cleansed` is set (S7's
 * `clear_area`: the Pitwarden dead and at most 5 hostiles left) the Pit spawns nothing.
 *
 * Tile ids are the terrain style `pit`'s (GameData/world.ts): 0 ash, 1 dug earth, 2 flagstone, 3 the seep (blocking),
 * 4 wall (the crypt block `outcrop_plains@pit`), 5 the niche's paving. The wall dressing (helm niches, lamps, tag
 * curtains, piers, the niche) is the set-piece layer's (GameData/setPieces.ts, zone `sub_rf_pit`).
 */

// One character per tile, row by row (row 0 is the north edge, col 0 the west edge).
//   #  wall            .  flagstone       ,  ash            :  dug earth
//   =  paving          ~  the seep        o  flagstone under a tag rack (blocked)
const ROWS: readonly string[] = [
  /*         0000000000111111111122222222223333333333 */
  /*         0123456789012345678901234567890123456789 */
  /*  0 */ '########################################',
  /*  1 */ '########################################',
  /*  2 */ '##,,.....~~~############################',
  /*  3 */ '##,,.......~############################',
  /*  4 */ '##.............................#########',
  /*  5 */ '##.............................#########',
  /*  6 */ '##.,,,,,....######.............#########',
  /*  7 */ '##.,,,,,....######.............#########',
  /*  8 */ '##.,,,,,....######.............#########',
  /*  9 */ '####,,,###########.............#########',
  /* 10 */ '####,,,###########.............#########',
  /* 11 */ '##################.............#########',
  /* 12 */ '##################.............#########',
  /* 13 */ '##################..####################',
  /* 14 */ '##################..####################',
  /* 15 */ '##################..####################',
  /* 16 */ '#############.......####################',
  /* 17 */ '#############.......####################',
  /* 18 */ '#############..#########################',
  /* 19 */ '####.............#######################',
  /* 20 */ '####.............#######################',
  /* 21 */ '####.............#####.............#####',
  /* 22 */ '####..oo....oo...#####..:::::::::..#####',
  /* 23 */ '####.............#####..:::::::::..#####',
  /* 24 */ '####....................:::::::::::#####',
  /* 25 */ '####....................:::::::::::#####',
  /* 26 */ '####..oo....oo...#####..:::::::::::#####',
  /* 27 */ '####.............#####..:::::::::::#####',
  /* 28 */ '####.............#####..:::::::::::#####',
  /* 29 */ '####.............#####..:::::::::::#####',
  /* 30 */ '######################....:::::::::#####',
  /* 31 */ '######################....:::::::::#####',
  /* 32 */ '######################..################',
  /* 33 */ '###############...........##############',
  /* 34 */ '###############...........##############',
  /* 35 */ '###############..=======..##############',
  /* 36 */ '###############..=======..##############',
  /* 37 */ '###############..=======..##############',
  /* 38 */ '########################################',
  /* 39 */ '########################################',
];

const TILE_OF: Readonly<Record<string, number>> = { '#': 4, '.': 2, ',': 0, ':': 1, '=': 5, '~': 3, 'o': 2 };
/** Not walkable: the wall, the seep and the two tiles under each tag rack (set pieces do not block by themselves). */
const BLOCKED = '#~o';

export const SubRfPitMap: SubDungeonMapData = {
  id: 'sub_rf_pit',
  name: '信者之坑',
  parentZone: 'emerald_plains',
  cols: 40,
  rows: 40,
  // The generator theme (the wall tile's art slot, `outcrop_plains`); the look is the presentation theme's.
  theme: 'plains',
  seed: 99903,
  // Art, terrain style and mood resolve `pit` first, then its parent `march` (Art/blender kit/theme.py; DR17).
  presentationTheme: 'pit',
  tiles: ROWS.map(row => [...row].map(ch => TILE_OF[ch])),
  collisions: ROWS.map(row => [...row].map(ch => !BLOCKED.includes(ch))),
  spawns: [
    // Past the Ash Steps: four breachclots in the passage to the Helm Wall (the jitter keeps them on its tiles, cols
    // 13-17: 8 tiles or more from the arrival tile; their aggro range is 4).
    { col: 15, row: 4, monsterId: 'breachclot', count: 4 },
    // The Helm Wall: six footmen at attention, a rank along row 11 with their backs to the niches, facing -row
    // (270 degrees). Still until the hero is within 3 tiles of one or one is hit; then the whole rank rises (K7).
    {
      col: 20, row: 11, monsterId: 'risen_footman', count: 6,
      dormant: { wakeRange: 3, line: { dCol: 1, dRow: 0 }, faceDeg: 270 },
    },
    // The Tag Cellar: the gravegnawers that came down to feed (a pack: one provoked wakes the four), and two
    // arbalesters behind the far racks.
    { col: 10, row: 24, monsterId: 'gravegnawer', count: 4 },
    { col: 10, row: 28, monsterId: 'risen_arbalest', count: 2 },
    // The Pitwarden's Cut: his two shieldbearers, his guard - one pack with him (`guard`), standing either side of the
    // way from the door to him, (26,25) and (26,27), facing the door (180 degrees). They hold their tiles (a dormant
    // formation: no scatter, no patrol) until the hero is inside their own aggro range (5), one of the three is hit or
    // he gives the alarm; already standing, they need no Rise.
    {
      col: 26, row: 25, monsterId: 'risen_shieldbearer', count: 2, guard: true,
      dormant: { wakeRange: 5, rise: false, line: { dCol: 0, dRow: 2 }, faceDeg: 180 },
    },
  ],
  miniBoss: { col: 28, row: 26, monsterId: 'sub_pitwarden' },
  playerStart: { col: 5, row: 5 },
  // The stair. It returns to the burn pit's rim, three tiles south of the pit mouth (entrance_rf_pit (17,26)): clear
  // of the mouth's prop, inside its interact reach.
  exit: { col: 5, row: 10, returnCol: 17, returnRow: 29 },
  // The banner's label: S7's level to the Pitwarden's (K7 as built; chapter1.md §9.2 *Levels*).
  levelRange: [5, 8],
  bgColor: '#0c0a0c',
  // S7's clear_area sets it (chapter1.md §8 *Saved world state*): the lamps light, the grade warms, nothing spawns.
  cleansedFlag: 'rf_pit_cleansed',
  // The dead of the pit (they never fade; K3 seeds). The Tag Cellar's lie by the racks their tags hang on: a
  // gravegnawer within a tile of one feeds (2 s, +30 % life, the body gone), out of a fight or under half life. The
  // Cut's lie round the pit the Pitwarden keeps digging; nothing down here raises them.
  corpseSeeds: [
    { col: 5, row: 22, monsterId: 'risen_footman' },
    { col: 14, row: 22, monsterId: 'risen_footman' },
    { col: 5, row: 26, monsterId: 'risen_footman' },
    { col: 14, row: 26, monsterId: 'risen_footman' },
    { col: 31, row: 24, monsterId: 'risen_footman' },
    { col: 32, row: 28, monsterId: 'risen_footman' },
    { col: 29, row: 30, monsterId: 'risen_footman' },
  ],
};

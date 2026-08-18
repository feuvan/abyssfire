// src/graphics/sprites/types.ts
import type { DrawUtils } from '../DrawUtils';

export type MonsterAction = 'idle' | 'walk' | 'attack' | 'hurt' | 'death';
export type PlayerAction = MonsterAction | 'dodge' | 'cast';
export type NPCAction = 'working' | 'alert' | 'idle' | 'talking';
export type EntityAction = MonsterAction | PlayerAction | NPCAction;

/**
 * Canonical player-sheet layout. Keep generation and Phaser registration
 * driven by this table so adding poses cannot silently shift later actions.
 */
export const PLAYER_ACTION_ORDER = [
  'idle',
  'walk',
  'attack',
  'hurt',
  'dodge',
  'death',
  'cast',
] as const satisfies readonly PlayerAction[];

export const PLAYER_ACTION_FRAME_COUNTS: Readonly<Record<PlayerAction, number>> = {
  idle: 6,
  walk: 8,
  attack: 8,
  hurt: 4,
  dodge: 6,
  death: 6,
  cast: 8,
};

export const PLAYER_TOTAL_FRAMES = PLAYER_ACTION_ORDER.reduce(
  (total, action) => total + PLAYER_ACTION_FRAME_COUNTS[action],
  0,
);

export function getPlayerActionFrameRange(action: PlayerAction): {
  start: number;
  end: number;
} {
  let start = 0;
  for (const candidate of PLAYER_ACTION_ORDER) {
    const end = start + PLAYER_ACTION_FRAME_COUNTS[candidate] - 1;
    if (candidate === action) return { start, end };
    start = end + 1;
  }
  return { start: 0, end: 0 };
}

export interface EntityDrawer {
  readonly key: string;
  readonly frameW: number;       // before TEXTURE_SCALE
  readonly frameH: number;       // before TEXTURE_SCALE
  readonly totalFrames: number;

  drawFrame(
    ctx: CanvasRenderingContext2D,
    frame: number,               // 0-based index within the current action
    action: EntityAction,
    w: number,                   // scaled frame width
    h: number,                   // scaled frame height
    utils: DrawUtils,
  ): void;
}

/** Map of texture key → {frameWidth, frameHeight} for BootScene spritesheet loading */
export type FrameSizeRegistry = Record<string, { frameWidth: number; frameHeight: number }>;

/** Build frame-size registry from the existing configs. Used by BootScene for spritesheet loading. */
export function buildFrameSizeRegistry(): FrameSizeRegistry {
  return {
    // Players (64x96, PLAYER_TOTAL_FRAMES frames)
    player_warrior: { frameWidth: 64, frameHeight: 96 },
    player_mage: { frameWidth: 64, frameHeight: 96 },
    player_rogue: { frameWidth: 64, frameHeight: 96 },
    // Monsters (various sizes, 20 frames)
    monster_slime: { frameWidth: 48, frameHeight: 40 },
    monster_goblin: { frameWidth: 48, frameHeight: 56 },
    monster_goblin_chief: { frameWidth: 60, frameHeight: 68 },
    monster_skeleton: { frameWidth: 44, frameHeight: 64 },
    monster_zombie: { frameWidth: 44, frameHeight: 60 },
    monster_werewolf: { frameWidth: 52, frameHeight: 64 },
    monster_werewolf_alpha: { frameWidth: 56, frameHeight: 68 },
    monster_gargoyle: { frameWidth: 52, frameHeight: 60 },
    monster_stone_golem: { frameWidth: 60, frameHeight: 68 },
    monster_mountain_troll: { frameWidth: 64, frameHeight: 72 },
    monster_fire_elemental: { frameWidth: 48, frameHeight: 60 },
    monster_desert_scorpion: { frameWidth: 52, frameHeight: 44 },
    monster_sandworm: { frameWidth: 56, frameHeight: 48 },
    monster_phoenix: { frameWidth: 56, frameHeight: 56 },
    monster_imp: { frameWidth: 40, frameHeight: 48 },
    monster_lesser_demon: { frameWidth: 52, frameHeight: 64 },
    monster_succubus: { frameWidth: 48, frameHeight: 64 },
    monster_demon_lord: { frameWidth: 72, frameHeight: 84 },
    monster_dungeon_shade: { frameWidth: 48, frameHeight: 60 },
    monster_dungeon_fiend: { frameWidth: 56, frameHeight: 68 },
    monster_dungeon_boss: { frameWidth: 80, frameHeight: 96 },
    monster_dungeon_mid_boss: { frameWidth: 64, frameHeight: 76 },
    monster_goblin_shaman: { frameWidth: 52, frameHeight: 60 },
    monster_shadow_weaver: { frameWidth: 52, frameHeight: 64 },
    monster_iron_guardian: { frameWidth: 60, frameHeight: 72 },
    monster_sand_wraith: { frameWidth: 52, frameHeight: 64 },
    monster_void_herald: { frameWidth: 56, frameHeight: 68 },
    monster_sub_mine_guardian: { frameWidth: 56, frameHeight: 68 },
    monster_sub_altar_keeper: { frameWidth: 56, frameHeight: 68 },
    // NPCs (80x120, 24 frames)
    npc_blacksmith: { frameWidth: 80, frameHeight: 120 },
    npc_blacksmith_advanced: { frameWidth: 80, frameHeight: 120 },
    npc_merchant: { frameWidth: 80, frameHeight: 120 },
    npc_merchant_desert: { frameWidth: 80, frameHeight: 120 },
    npc_stash: { frameWidth: 80, frameHeight: 120 },
    npc_quest_elder: { frameWidth: 80, frameHeight: 120 },
    npc_quest_scout: { frameWidth: 80, frameHeight: 120 },
    npc_forest_hermit: { frameWidth: 80, frameHeight: 120 },
    npc_quest_dwarf: { frameWidth: 80, frameHeight: 120 },
    npc_quest_nomad: { frameWidth: 80, frameHeight: 120 },
    npc_quest_warden: { frameWidth: 80, frameHeight: 120 },
    // Event NPCs are generated lazily when the corresponding event/quest appears.
    npc_rescue_lost_traveler: { frameWidth: 80, frameHeight: 120 },
    npc_rescue_wounded_hunter: { frameWidth: 80, frameHeight: 120 },
    npc_rescue_trapped_miner: { frameWidth: 80, frameHeight: 120 },
    npc_rescue_caravan_guard: { frameWidth: 80, frameHeight: 120 },
    npc_rescue_abyss_explorer: { frameWidth: 80, frameHeight: 120 },
    npc_escort_traveling_merchant: { frameWidth: 80, frameHeight: 120 },
    npc_escort_wounded_explorer: { frameWidth: 80, frameHeight: 120 },
    npc_mercenary_tank: { frameWidth: 80, frameHeight: 120 },
    npc_mercenary_melee: { frameWidth: 80, frameHeight: 120 },
    npc_mercenary_ranged: { frameWidth: 80, frameHeight: 120 },
    npc_mercenary_healer: { frameWidth: 80, frameHeight: 120 },
    npc_mercenary_mage: { frameWidth: 80, frameHeight: 120 },
  };
}

import Phaser from 'phaser';

export const EventBus = new Phaser.Events.EventEmitter();

export const GameEvents = {
  PLAYER_HEALTH_CHANGED: 'player:health',
  PLAYER_MANA_CHANGED: 'player:mana',
  PLAYER_SPIRIT_CHANGED: 'player:spirit',
  PLAYER_EXP_CHANGED: 'player:exp',
  PLAYER_LEVEL_UP: 'player:levelup',
  PLAYER_DIED: 'player:died',
  SPIRIT_RESONANCE_STARTED: 'spirit:resonance_started',
  SPIRIT_RESONANCE_ENDED: 'spirit:resonance_ended',
  DODGE_STARTED: 'combat:dodge_started',
  TARGET_CHANGED: 'combat:target_changed',
  SKILL_BUFFERED: 'combat:skill_buffered',
  SKILL_LEVEL_CHANGED: 'skill:level_changed',
  MONSTER_DIED: 'monster:died',
  COMBAT_DAMAGE: 'combat:damage',
  SKILL_USED: 'skill:used',
  ITEM_PICKED: 'item:picked',
  ITEM_DROPPED: 'item:dropped',
  ZONE_ENTERED: 'zone:entered',
  ZONE_EXIT: 'zone:exit',
  LOG_MESSAGE: 'log:message',
  NPC_INTERACT: 'npc:interact',
  SHOP_OPEN: 'shop:open',
  SHOP_CLOSE: 'shop:close',
  DIALOGUE_CLOSE: 'dialogue:close',
  INVENTORY_OPEN: 'inventory:open',
  INVENTORY_CLOSE: 'inventory:close',
  INVENTORY_CHANGED: 'inventory:changed',
  QUEST_ACCEPTED: 'quest:accepted',
  QUEST_COMPLETED: 'quest:completed',
  QUEST_TURNED_IN: 'quest:turned_in',
  QUEST_FAILED: 'quest:failed',
  /** An objective advanced: { questId, objectiveIndex, current, required, targetId, amount }. */
  QUEST_PROGRESS: 'quest:progress',
  /** The quest shown by the guide arrow changed: { questId | null }. */
  QUEST_TRACKED_CHANGED: 'quest:tracked_changed',
  /** A story beat started/finished playing: { active: boolean }. */
  STORY_STATE: 'story:state',
  /** Show (BossBarState) or hide (null) the boss health bar. */
  BOSS_BAR: 'story:boss_bar',
  ACHIEVEMENT_UNLOCKED: 'achievement:unlocked',
  HOMESTEAD_UPGRADED: 'homestead:upgraded',
  /** Ley-beast state changed (owned / active / level / bond): { petId | null }. */
  PET_CHANGED: 'pet:changed',
  /** A ley-beast joined the hero: { petId, silent }. */
  PET_OBTAINED: 'pet:obtained',
  ITEM_DISCARDED: 'item:discarded',
  SAVE_GAME: 'save:game',
  LOAD_GAME: 'load:game',
  UI_SKILL_CLICK: 'ui:skill_click',
  UI_DODGE_REQUEST: 'ui:dodge_request',
  UI_TARGET_CYCLE: 'ui:target_cycle',
  UI_TOGGLE_PANEL: 'ui:toggle_panel',
  COMBAT_STATE_CHANGED: 'combat:state_changed',
  MINIBOSS_DIALOGUE: 'miniboss:dialogue',
  LORE_COLLECTED: 'lore:collected',
  RANDOM_EVENT_TRIGGERED: 'random_event:triggered',
  RANDOM_EVENT_RESOLVED: 'random_event:resolved',
  HIDDEN_AREA_DISCOVERED: 'hidden_area:discovered',
  STORY_DECORATION_INTERACT: 'story_decoration:interact',
  SUBDUNGEON_ENTER: 'subdungeon:enter',
  SUBDUNGEON_EXIT: 'subdungeon:exit',
  DUNGEON_ENTER: 'dungeon:enter',
  DUNGEON_FLOOR_CHANGE: 'dungeon:floor_change',
  DUNGEON_EXIT: 'dungeon:exit',
  DUNGEON_BOSS_KILLED: 'dungeon:boss_killed',
  /** Zone → UI: open the tier picker at the labyrinth portal (DungeonTierPickPayload). */
  DUNGEON_TIER_PICK: 'dungeon:tier_pick',
  /** UI → Zone: `{ tier }` chosen, or `{ tier: 0 }` to cancel. */
  DUNGEON_TIER_CHOSEN: 'dungeon:tier_chosen',
  /** Zone → UI: offer boons after a floor (DungeonBoonOfferPayload). */
  DUNGEON_BOON_OFFER: 'dungeon:boon_offer',
  /** UI → Zone: `{ boonId }` picked. */
  DUNGEON_BOON_CHOSEN: 'dungeon:boon_chosen',
  /** Zone → UI: run status for the HUD (DungeonHudPayload); `null` hides it. */
  DUNGEON_HUD: 'dungeon:hud',
  /** Zone → UI: the run is over (DungeonRunEndPayload). */
  DUNGEON_RUN_END: 'dungeon:run_end',
  GEM_SOCKET_OPEN: 'gem_socket:open',
  LOCALE_CHANGED: 'locale:changed',
} as const;

// ── Abyss Labyrinth payloads ──

export interface DungeonTierPickPayload {
  /** Highest tier the hero may start. */
  unlockedTier: number;
  /** Best tier cleared so far (0 = none). */
  bestTier: number;
  /** Recommended hero level per tier: TIER_BASE_LEVEL + (tier - 1) * TIER_LEVEL_STEP. */
  heroLevel: number;
}

export interface DungeonBoonOfferPayload {
  /** Floor just cleared. */
  floor: number;
  /** Boon ids from BOONS (src/data/abyssRun.ts), usually three. */
  options: string[];
  /** Stacks already held this run, by boon id. */
  held: Record<string, number>;
}

export interface DungeonHudPayload {
  tier: number;
  floor: number;
  totalFloors: number;
  /** FLOOR_THEMES id. */
  theme: string;
  /** CURSES id on this floor, or null. */
  curse: string | null;
  boons: Record<string, number>;
  /** The exit opens once the gatekeeper falls (always open on the boss floor after the boss). */
  sealOpen: boolean;
  /** Localised gatekeeper / boss name while the seal holds. */
  sealKeeper: string | null;
  kills: number;
  /** Run start (Date.now()) for the timer. */
  startedAt: number;
}

export interface DungeonRunEndPayload {
  result: 'cleared' | 'fallen' | 'abandoned';
  tier: number;
  floorsCleared: number;
  totalFloors: number;
  kills: number;
  timeMs: number;
  boons: Record<string, number>;
  /** Cleared a tier above the previous best. */
  newBest: boolean;
  /** Highest tier now available. */
  unlockedTier: number;
}


/**
 * RandomEventSystem — triggers random events during exploration.
 *
 * Event types:
 *  1. Ambush — spawn zone-level monsters with warning '伏兵出现!'
 *  2. Treasure Cache — interactable chest with zone-scaled loot
 *  3. Wandering Merchant — temp NPC with unique inventory
 *  4. Rescue — stranded NPC + surrounding hostiles, reward on clear
 *  5. Environmental Puzzle — interaction-based challenge with reward
 *
 * Trigger conditions:
 *  - Player moving on exploration tiles
 *  - Configurable cooldown (≥30s between events)
 *  - No triggers in camps/safe zones
 *  - No triggers during active combat
 *
 * Zone-appropriate scaling for monster levels and loot tiers.
 * Event frequency: 3-8 events per 5 minutes of exploration.
 */

// ─── Types ────────────────────────────────────────────────────────────────

export type RandomEventType = 'ambush' | 'treasure_cache' | 'wandering_merchant' | 'rescue' | 'environmental_puzzle';

export interface RandomEventDefinition {
  type: RandomEventType;
  /** Weight for random selection (higher = more likely). */
  weight: number;
  /** Chinese name for UI display. */
  name: string;
  /** Chinese description for combat log / notification. */
  message: string;
}

export interface ActiveEvent {
  type: RandomEventType;
  /** World-time (ms) when the event was triggered. */
  triggeredAt: number;
  /** Tile position where the event was spawned. */
  col: number;
  row: number;
  /** Whether the event has been completed / dismissed. */
  resolved: boolean;
  /** Additional context (e.g., monster IDs, reward items, puzzle state). */
  context: Record<string, unknown>;
}

export interface RandomEventConfig {
  /** Minimum cooldown between events in ms (default: 30000 = 30s). */
  cooldownMs: number;
  /** Safe zone radius (tiles) — events don't trigger near camps. */
  safeZoneRadius: number;
  /** Target events per 5 minutes (range). */
  minEventsPerWindow: number;
  maxEventsPerWindow: number;
  /** Exploration window for frequency tracking (ms). Default: 300000 = 5 min. */
  frequencyWindowMs: number;
}

/** Zone scaling data passed at construction. */
export interface ZoneScaleInfo {
  zoneId: string;
  levelRange: [number, number];
}

// ─── Default config ───────────────────────────────────────────────────────

export const DEFAULT_RANDOM_EVENT_CONFIG: RandomEventConfig = {
  cooldownMs: 30_000,       // 30s minimum between events
  safeZoneRadius: 9,        // tiles
  minEventsPerWindow: 3,
  maxEventsPerWindow: 8,
  frequencyWindowMs: 300_000, // 5 minutes
};

// ─── Event definitions (all Chinese text) ─────────────────────────────────

export const RANDOM_EVENT_DEFS: RandomEventDefinition[] = [
  { type: 'ambush', weight: 30, name: '伏击', message: '伏兵出现!' },
  { type: 'treasure_cache', weight: 25, name: '宝箱', message: '发现了一个隐藏的宝箱!' },
  { type: 'wandering_merchant', weight: 15, name: '流浪商人', message: '一位流浪商人出现在你面前。' },
  { type: 'rescue', weight: 20, name: '救援', message: '有人被怪物包围了! 快去营救!' },
  { type: 'environmental_puzzle', weight: 10, name: '谜题', message: '你发现了一个古老的谜题装置。' },
];

// ─── Per-zone event data ──────────────────────────────────────────────────

export interface ZoneEventData {
  /** Monster IDs that can spawn during ambush/rescue events. */
  ambushMonsters: string[];
  /** Number of monsters spawned during ambush. */
  ambushCount: [number, number]; // [min, max]
  /** Merchant item pool (item base IDs). */
  merchantItems: string[];
  /** Puzzle descriptions per zone. */
  puzzleDescriptions: { prompt: string; solution: string; reward: string; rewardGold: number; rewardExp: number }[];
  /** Rescue NPC name (Chinese). */
  rescueNpcName: string;
  /** Rescue reward in gold and exp. */
  rescueReward: { gold: number; exp: number };
}

export const ZONE_EVENT_DATA: Record<string, ZoneEventData> = {
  emerald_plains: {
    ambushMonsters: ['slime_green', 'goblin'],
    ambushCount: [3, 5],
    merchantItems: ['iron_sword', 'leather_armor', 'hp_potion', 'mp_potion'],
    puzzleDescriptions: [
      { prompt: '石柱上的符文需要按正确顺序触摸。', solution: '按下发光的符文', reward: '获得了经验和金币!', rewardGold: 50, rewardExp: 30 },
    ],
    rescueNpcName: '迷路的旅人',
    rescueReward: { gold: 30, exp: 25 },
  },
  twilight_forest: {
    ambushMonsters: ['wolf', 'bandit'],
    ambushCount: [4, 6],
    merchantItems: ['steel_sword', 'chain_armor', 'hp_potion', 'mp_potion'],
    puzzleDescriptions: [
      { prompt: '古树的树根形成了一个迷宫，需要找到正确的路径。', solution: '沿着发光的苔藓前进', reward: '获得了经验和金币!', rewardGold: 80, rewardExp: 60 },
    ],
    rescueNpcName: '受伤的猎人',
    rescueReward: { gold: 60, exp: 50 },
  },
  anvil_mountains: {
    ambushMonsters: ['golem', 'harpy'],
    ambushCount: [4, 7],
    merchantItems: ['mithril_sword', 'plate_armor', 'hp_potion', 'mp_potion'],
    puzzleDescriptions: [
      { prompt: '矿洞墙壁上有三个宝石插槽等待填入。', solution: '按颜色顺序放入宝石', reward: '获得了经验和金币!', rewardGold: 120, rewardExp: 100 },
    ],
    rescueNpcName: '被困的矿工',
    rescueReward: { gold: 100, exp: 80 },
  },
  scorching_desert: {
    ambushMonsters: ['scorpion', 'sand_elemental'],
    ambushCount: [5, 8],
    merchantItems: ['desert_blade', 'desert_armor', 'hp_potion', 'mp_potion'],
    puzzleDescriptions: [
      { prompt: '沙漠中的日晷指向一个隐藏的方向。', solution: '转动日晷指针', reward: '获得了经验和金币!', rewardGold: 160, rewardExp: 140 },
    ],
    rescueNpcName: '沙漠中的商队护卫',
    rescueReward: { gold: 140, exp: 120 },
  },
  abyss_rift: {
    ambushMonsters: ['demon_imp', 'shadow_fiend'],
    ambushCount: [5, 9],
    merchantItems: ['abyssal_blade', 'demon_armor', 'hp_potion', 'mp_potion'],
    puzzleDescriptions: [
      { prompt: '深渊裂缝中闪烁着不稳定的魔法阵。', solution: '按正确的符文序列激活', reward: '获得了经验和金币!', rewardGold: 200, rewardExp: 180 },
    ],
    rescueNpcName: '深渊探险家',
    rescueReward: { gold: 180, exp: 160 },
  },
};

// ─── System ───────────────────────────────────────────────────────────────

export class RandomEventSystem {
  private config: RandomEventConfig;
  private zoneInfo: ZoneScaleInfo;
  private lastEventTime = -Infinity;
  private activeEvent: ActiveEvent | null = null;
  private eventHistory: number[] = []; // timestamps of triggered events
  private explorationTime = 0; // total ms spent moving/exploring
  private lastPlayerCol = -1;
  private lastPlayerRow = -1;

  /** Accumulated movement distance (tiles) since last event check. */
  private movementAccum = 0;
  /** Tiles moved since last trigger check. */
  private readonly TRIGGER_MOVE_THRESHOLD = 3;

  constructor(zoneInfo: ZoneScaleInfo, config?: Partial<RandomEventConfig>) {
    this.zoneInfo = zoneInfo;
    this.config = { ...DEFAULT_RANDOM_EVENT_CONFIG, ...config };
  }

  // ── Getters ───────────────────────────────────────────────────────────

  getActiveEvent(): ActiveEvent | null {
    return this.activeEvent;
  }

  getConfig(): Readonly<RandomEventConfig> {
    return this.config;
  }

  getLastEventTime(): number {
    return this.lastEventTime;
  }

  getExplorationTime(): number {
    return this.explorationTime;
  }

  getEventHistory(): readonly number[] {
    return this.eventHistory;
  }

  // ── Core logic ────────────────────────────────────────────────────────

  /**
   * Called every frame from ZoneScene.update().
   * Returns a newly triggered event, or null if no event triggers.
   */
  update(
    time: number,
    delta: number,
    playerCol: number,
    playerRow: number,
    inCombat: boolean,
    campPositions: { col: number; row: number }[],
    tileType?: number, // tile type at player position (5 = camp)
  ): ActiveEvent | null {
    // Track exploration time (only when player is moving)
    const moved = playerCol !== this.lastPlayerCol || playerRow !== this.lastPlayerRow;
    if (moved) {
      this.explorationTime += delta;
      const dx = Math.abs(playerCol - (this.lastPlayerCol >= 0 ? this.lastPlayerCol : playerCol));
      const dy = Math.abs(playerRow - (this.lastPlayerRow >= 0 ? this.lastPlayerRow : playerRow));
      this.movementAccum += Math.max(dx, dy);
      this.lastPlayerCol = playerCol;
      this.lastPlayerRow = playerRow;
    }

    // ── Check conditions ─────────────────────────────────────────────
    if (!moved) return null;
    if (inCombat) return null;
    if (this.activeEvent && !this.activeEvent.resolved) return null;

    // Not in camp/safe zone
    if (this.isInSafeZone(playerCol, playerRow, campPositions, tileType)) return null;

    // Cooldown enforcement
    if (time - this.lastEventTime < this.config.cooldownMs) return null;

    // Movement threshold
    if (this.movementAccum < this.TRIGGER_MOVE_THRESHOLD) return null;

    // Frequency cap: prune old events outside the window
    const windowStart = time - this.config.frequencyWindowMs;
    this.eventHistory = this.eventHistory.filter(t => t >= windowStart);
    if (this.eventHistory.length >= this.config.maxEventsPerWindow) return null;

    // Probability roll: scale so we hit the target range over the window
    const eventChance = this.calculateTriggerChance(time);
    if (Math.random() > eventChance) return null;

    // ── Trigger event ────────────────────────────────────────────────
    this.movementAccum = 0;
    const eventType = this.selectEventType();
    const event = this.createEvent(eventType, time, playerCol, playerRow);
    this.activeEvent = event;
    this.lastEventTime = time;
    this.eventHistory.push(time);

    return event;
  }

  /**
   * Check whether coordinates fall inside a camp/safe zone.
   */
  isInSafeZone(
    col: number,
    row: number,
    campPositions: { col: number; row: number }[],
    tileType?: number,
  ): boolean {
    // Camp tile type (5) always counts as safe
    if (tileType === 5 || tileType === 6) return true; // 5=camp, 6=camp_wall

    for (const camp of campPositions) {
      const dx = col - camp.col;
      const dy = row - camp.row;
      const dist = Math.sqrt(dx * dx + dy * dy);
      if (dist < this.config.safeZoneRadius) return true;
    }
    return false;
  }

  /**
   * Mark the active event as resolved.
   */
  resolveActiveEvent(): void {
    if (this.activeEvent) {
      this.activeEvent.resolved = true;
    }
  }

  /**
   * Force-set the last event time (useful for testing/loading saves).
   */
  setLastEventTime(time: number): void {
    this.lastEventTime = time;
  }

  /**
   * Reset the system state (e.g., on zone change).
   */
  reset(zoneInfo?: ZoneScaleInfo): void {
    if (zoneInfo) this.zoneInfo = zoneInfo;
    this.activeEvent = null;
    this.eventHistory = [];
    this.explorationTime = 0;
    this.lastPlayerCol = -1;
    this.lastPlayerRow = -1;
    this.movementAccum = 0;
    // Preserve lastEventTime for cross-zone cooldown
  }

  // ── Internal helpers ──────────────────────────────────────────────────

  /**
   * Calculate trigger chance based on frequency targets and exploration time.
   * Designed to produce 3-8 events per 5 minutes of movement.
   */
  private calculateTriggerChance(time: number): number {
    const windowStart = time - this.config.frequencyWindowMs;
    const recentEvents = this.eventHistory.filter(t => t >= windowStart).length;

    // Base chance per movement threshold (3 tiles ≈ every ~3-5 seconds of movement)
    // 5 min = 300s, at ~4s per threshold check, that's ~75 checks per window
    // For ~5.5 events avg: 5.5 / 75 ≈ 0.073 base chance
    let baseChance = 0.07;

    // Ramp up if below minimum target
    if (recentEvents < this.config.minEventsPerWindow) {
      // Time into the window
      const elapsedInWindow = Math.min(
        this.explorationTime,
        this.config.frequencyWindowMs,
      );
      const progressRatio = elapsedInWindow / this.config.frequencyWindowMs;
      // If we're past half the window and below min, increase chance
      if (progressRatio > 0.3) {
        baseChance += 0.05 * progressRatio;
      }
    }

    // Reduce chance if approaching max
    if (recentEvents >= this.config.maxEventsPerWindow - 1) {
      baseChance *= 0.3;
    }

    return Math.min(0.25, Math.max(0.02, baseChance));
  }

  /**
   * Select an event type based on weighted probabilities.
   */
  private selectEventType(): RandomEventType {
    const totalWeight = RANDOM_EVENT_DEFS.reduce((sum, d) => sum + d.weight, 0);
    let roll = Math.random() * totalWeight;
    for (const def of RANDOM_EVENT_DEFS) {
      roll -= def.weight;
      if (roll <= 0) return def.type;
    }
    return RANDOM_EVENT_DEFS[RANDOM_EVENT_DEFS.length - 1].type;
  }

  /**
   * Create an ActiveEvent instance.
   */
  private createEvent(type: RandomEventType, time: number, col: number, row: number): ActiveEvent {
    const zoneData = ZONE_EVENT_DATA[this.zoneInfo.zoneId];
    const context: Record<string, unknown> = {};

    switch (type) {
      case 'ambush': {
        const monsters = zoneData?.ambushMonsters ?? [];
        const [minCount, maxCount] = zoneData?.ambushCount ?? [3, 5];
        const count = Math.floor(Math.random() * (maxCount - minCount + 1)) + minCount;
        context.monsterIds = monsters;
        context.monsterCount = count;
        context.levelRange = this.zoneInfo.levelRange;
        break;
      }
      case 'treasure_cache': {
        // Zone-scaled loot quality
        const avgLevel = Math.floor((this.zoneInfo.levelRange[0] + this.zoneInfo.levelRange[1]) / 2);
        context.lootLevel = avgLevel;
        context.qualityBoost = Math.floor(avgLevel / 10);
        break;
      }
      case 'wandering_merchant': {
        context.merchantItems = zoneData?.merchantItems ?? [];
        context.priceMultiplier = 1.2; // slightly more expensive
        break;
      }
      case 'rescue': {
        const monsters = zoneData?.ambushMonsters ?? [];
        const [minCount, maxCount] = zoneData?.ambushCount ?? [2, 4];
        const count = Math.max(2, Math.floor(Math.random() * (maxCount - minCount + 1)) + minCount - 1);
        context.monsterIds = monsters;
        context.monsterCount = count;
        context.rescueNpcName = zoneData?.rescueNpcName ?? '被困的旅人';
        context.reward = zoneData?.rescueReward ?? { gold: 50, exp: 40 };
        break;
      }
      case 'environmental_puzzle': {
        const puzzles = zoneData?.puzzleDescriptions ?? [];
        const puzzle = puzzles.length > 0
          ? puzzles[Math.floor(Math.random() * puzzles.length)]
          : { prompt: '一个古老的谜题', solution: '解开谜题', reward: '获得了奖励!', rewardGold: 50, rewardExp: 30 };
        context.puzzle = puzzle;
        break;
      }
    }

    return {
      type,
      triggeredAt: time,
      col,
      row,
      resolved: false,
      context,
    };
  }

  /**
   * Get the event definition (Chinese name/message) for a given event type.
   */
  static getEventDef(type: RandomEventType): RandomEventDefinition | undefined {
    return RANDOM_EVENT_DEFS.find(d => d.type === type);
  }

  /**
   * Get zone-specific event data.
   */
  static getZoneEventData(zoneId: string): ZoneEventData | undefined {
    return ZONE_EVENT_DATA[zoneId];
  }

  /**
   * Compute the average zone level from a level range.
   */
  static getZoneAvgLevel(levelRange: [number, number]): number {
    return Math.floor((levelRange[0] + levelRange[1]) / 2);
  }

  /**
   * Find a walkable tile near a preferred position.
   * collisions[r][c] === true means the tile IS walkable.
   * If the preferred position is blocked, search outward in expanding rings.
   * Returns the position if found, or null if no walkable tile within maxRadius.
   */
  static findWalkableTile(
    preferredCol: number,
    preferredRow: number,
    collisions: boolean[][],
    cols: number,
    rows: number,
    maxRadius = 5,
  ): { col: number; row: number } | null {
    // Clamp to valid range
    const pc = Math.max(1, Math.min(cols - 2, preferredCol));
    const pr = Math.max(1, Math.min(rows - 2, preferredRow));

    // Check the preferred position first
    if (collisions[pr]?.[pc]) {
      return { col: pc, row: pr };
    }

    // Search outward in expanding rings
    for (let radius = 1; radius <= maxRadius; radius++) {
      for (let dr = -radius; dr <= radius; dr++) {
        for (let dc = -radius; dc <= radius; dc++) {
          if (Math.abs(dr) !== radius && Math.abs(dc) !== radius) continue; // only ring edges
          const nr = pr + dr;
          const nc = pc + dc;
          if (nr >= 1 && nr < rows - 1 && nc >= 1 && nc < cols - 1 && collisions[nr]?.[nc]) {
            return { col: nc, row: nr };
          }
        }
      }
    }

    return null;
  }
}
